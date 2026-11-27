"""Qualify the HTTP claim, ownership, run polling, restart, and dashboard boundaries."""

import hashlib
import hmac
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).parent / "dashboard"))
from fixture import fixture
from database import eventually


def check(build):
    with fixture(build, small=True) as fx:
        fx.ready()
        assert fx.request("/api/v1/waves/w1/plan.json", browser=False).status == 404
        assert fx.request("/api/v1/requests/unknown/claim", b"", browser=False).status == 401
        assert fx.request("/api/v1/requests/unknown/attempts/1", browser=False).status == 404
        unauthenticated = fx.request("/api/dispatch-paused", b'{"paused":true}',
                                     {"Content-Type": "application/json", "Origin": fx.app}, browser=False)
        assert unauthenticated.status == 401, (unauthenticated.status, unauthenticated.read())
        assert fx.request("/").status == 200
        assert fx.mints == 1
        assert fx.webhook("owner/a") == 200
        run, values = fx.dispatches.get(timeout=12)
        second_run, second = fx.dispatches.get(timeout=12)
        assert run != second_run
        assert fx.status()["reservations"]["queued"] == 2
        frozen = fx.plan_bytes(values)
        facts = dict(attempt=int(values["attempt"]), run_id=run, run_attempt=1)
        request = values["request_id"]
        grant = fx.callback(values, "claim", **facts)
        assert grant.status == 200
        stamp = grant.headers["X-CoreCI-Timestamp"]
        raw = grant.read()
        expected = hmac.new(b"fixture-callback", stamp.encode() + b"." + raw, hashlib.sha256).hexdigest()
        assert grant.headers["X-CoreCI-Signature-256"] == "sha256=" + expected
        assert json.loads(raw) == dict(facts, id=request, granted=True, document_sha256=hashlib.sha256(frozen).hexdigest())
        assert fx.callback(values, "claim", **dict(facts, run_id=run + 1000)).status == 409
        assert fx.callback(values, "claim", **dict(facts, run_attempt=2)).status == 409
        assert fx.callback(values, "claim", **facts).status == 200
        with fx.request("/events", headers={"Accept": "text/event-stream"}) as stream:
            assert stream.readline().strip() == b"event: snapshot"
            payload = stream.readline()
            model = json.loads(payload.removeprefix(b"data: "))
            assert any(item["request"] == request and item["state"] == "processing" for item in model["items"])
        assert fx.callback(values, "report", **facts, status="finished", outcome="built", artifact_id=5,
                           artifact_digest="a" * 64, inventory='[{"name":"a"}]').status == 200
        assert any(item["request"] == request and item["state"] == "processing" for item in fx.status()["items"])
        fx.restart()
        assert eventually(lambda: fx.request("/api/v1/status.json", browser=False).status == 200)
        assert fx.plan_bytes(values) == frozen
        assert fx.callback(values, "claim", **facts).status == 200
        assert fx.request("/").status == 200
        fx.runs[run].update(status="completed", conclusion="success")
        publication_run, publication = fx.dispatches.get(timeout=12)
        assert "publication_id" in publication
        plan = fx.plan(publication)
        assert plan["expected_head"] == "f" * 40 and plan["items"][0]["request"] == request
        claim = dict(attempt=int(publication["attempt"]), run_id=publication_run, run_attempt=1)
        assert fx.callback(publication, "claim", **claim).status == 200
        receipt = dict(publication=publication["publication_id"], attempt=1, request=request, outcome="published")
        fx.head = "e" * 40
        fx.snapshots["refs/snapshots/feed/" + publication["publication_id"]] = fx.head
        fx.ledger = dict(entries=[], receipts=[receipt])
        fx.runs[publication_run].update(status="completed", conclusion="failure")
        assert eventually(lambda: any(item["request"] == request and item["state"] == "published" for item in fx.status()["items"]))
        second_fact = dict(attempt=int(second["attempt"]), run_id=second_run, run_attempt=1)
        assert fx.callback(second, "claim", **second_fact).status == 200
        fx.runs[second_run].update(status="completed", conclusion="cancelled")
        assert eventually(lambda: any(item["request"] == second["request_id"] and
                                      item["reason"] == "conclusion cancelled, no report" for item in fx.status()["items"]))
        retry = next(item for item in fx.status()["items"] if item["request"] == second["request_id"])
        assert fx.post("/api/retry", {"item": retry["id"]})[0] == 200
        again, retried = fx.dispatches.get(timeout=12)
        assert again != second_run and retried["attempt"] == "2" and retried["request_id"] == second["request_id"]
        original = fx.plan(second)["inputs"]
        assert fx.plan(retried)["inputs"] == original
        assert fx.post("/api/dispatch-paused", {"paused": True})[0] == 200
        assert fx.status()["paused"]
        assert fx.request("/api/v1/badge/a/t1.svg", browser=False).status == 200
    return None


if __name__ == "__main__":
    check(Path(sys.argv[1]).resolve())
