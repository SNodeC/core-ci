"""Qualify the HTTP claim, ownership, run polling, restart, and dashboard boundaries."""

import hashlib
import hmac
import json
import os
from pathlib import Path
import subprocess
import sys
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).parent / "dashboard"))
from fixture import fixture
from database import database, eventually


def restart(fx, *documents):
    def identities():
        status = fx.status()
        items = [(item["id"], item.get("request"), item.get("attempt"), item.get("run"), item["state"])
                 for item in status["items"]]
        return status["paused"], status["reservations"], items, status["publications"]

    before = identities()
    frozen = [fx.plan_bytes(values) for values in documents]
    fx.restart()
    assert eventually(lambda: fx.status()["ready"])
    assert identities() == before
    assert [fx.plan_bytes(values) for values in documents] == frozen
    assert fx.dispatches.empty()


def item_state(fx, values):
    return next(item["state"] for item in fx.status()["items"] if item.get("request") == values["request_id"])


def check(build):
    with fixture(build, small=True, coreci_options={"batch-delay": "10"}) as fx:
        fx.ready()
        invalid = fx.work / "invalid.conf"
        invalid.write_text(fx.config_path.read_text().replace(
            "[coreci]\n", '[coreci]\nqualification_fault="after_push:test"\n', 1))
        rejected = subprocess.run(fx.command[:2] + [str(invalid)] + fx.command[3:], env=fx.env,
                                  capture_output=True, text=True, timeout=10)
        assert rejected.returncode and "qualification_fault" in rejected.stdout + rejected.stderr
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
        assert fx.callback(values, "report", **dict(facts, run_id=run + 1000), status="finished", outcome="failed").status == 409
        assert fx.callback(values, "report", **facts, status="finished", outcome="failed").status == 200
        assert fx.callback(values, "report", **facts, status="started").status == 200
        assert any(item["request"] == request and item["state"] == "processing" for item in fx.status()["items"])
        fx.restart()
        assert eventually(lambda: fx.request("/api/v1/status.json", browser=False).status == 200)
        assert fx.plan_bytes(values) == frozen
        assert fx.callback(values, "claim", **facts).status == 200
        assert fx.request("/").status == 200
        fx.ambiguous_publication = True
        assert fx.post("/api/dispatch-paused", {"paused": True})[0] == 200
        fx.runs[run].update(status="completed", conclusion="success")
        assert eventually(lambda: item_state(fx, values) == "built")
        assert fx.status()["publications"] == []
        restart(fx, values, second)
        assert fx.request("/").status == 200
        assert fx.post("/api/dispatch-paused", {"paused": False})[0] == 200
        assert eventually(lambda: fx.status()["reservations"]["publication"] == 1)
        batching = fx.status()["publications"][0]
        assert batching["state"] == "dispatched" and batching["run"] == 0
        assert fx.request(f'/api/v1/publications/{batching["id"]}/attempts/1', browser=False).status == 404
        restart(fx, values, second)
        publication_run, publication = fx.dispatches.get(timeout=16)
        assert "publication_id" in publication
        assert publication["publication_id"] == batching["id"] and publication["attempt"] == "1"
        assert eventually(lambda: any(path.endswith("/actions/workflows/publish.yml/runs")
                                      for path in fx.api_requests))
        plan = fx.plan(publication)
        assert plan["expected_head"] == "f" * 40 and plan["items"][0]["request"] == request
        claim = dict(attempt=int(publication["attempt"]), run_id=publication_run, run_attempt=1)
        assert fx.callback(publication, "claim", **claim).status == 200
        fx.feed_response.clear()
        receipt = dict(publication=publication["publication_id"], attempt=1, request=request, outcome="published")
        fx.head = "e" * 40
        fx.snapshots["refs/snapshots/feed/" + publication["publication_id"]] = fx.head
        fx.ledger = dict(entries=[], receipts=[receipt])
        fx.runs[publication_run].update(status="completed", conclusion="failure")
        assert eventually(lambda: any(p["state"] == "completed" and not p["reconciled"]
                                      for p in fx.status()["publications"]))
        restart(fx, values, second, publication)
        fx.feed_response.set()
        assert eventually(lambda: any(item["request"] == request and item["state"] == "published" for item in fx.status()["items"]))
        second_fact = dict(attempt=int(second["attempt"]), run_id=second_run, run_attempt=1)
        assert fx.callback(second, "claim", **second_fact).status == 200
        fx.runs[second_run].update(status="completed", conclusion="cancelled")
        assert eventually(lambda: any(item["request"] == second["request_id"] and
                                      item["reason"] == "conclusion cancelled, no report" for item in fx.status()["items"]))
        retry = next(item for item in fx.status()["items"] if item["request"] == second["request_id"])
        assert fx.request("/").status == 200
        assert fx.post("/api/retry", {"item": retry["id"]})[0] == 200
        again, retried = fx.dispatches.get(timeout=12)
        assert again != second_run and retried["attempt"] == "2" and retried["request_id"] == second["request_id"]
        original = fx.plan(second)["inputs"]
        assert fx.plan(retried)["inputs"] == original
        assert fx.post("/api/dispatch-paused", {"paused": True})[0] == 200
        assert fx.status()["paused"]
        assert fx.request("/api/v1/badge/a/t1.svg", browser=False).status == 200
        check_arrivals(fx, again, retried, values)
    return None


def check_arrivals(fx, active_run, active, published):
    frozen = fx.plan_bytes(active)
    facts = dict(attempt=int(active["attempt"]), run_id=active_run, run_attempt=1)
    assert fx.callback(active, "claim", **facts).status == 200
    for name in ("f", "g"):
        repo = f"owner/{name}"
        fx.sources[repo] = dict(fx.sources["owner/e"], package=name, title=name,
                                depends_on=[dict(package="a", version=">=2")])
        fx.tags[repo] = {"v1.9.0": "c" * 40, "v1.10.0": "d" * 40, "v2.0.0-rc.1": "e" * 40}
        fx.registry["repositories"].append(repo)
    fx.tags["owner/a"].update({"v2.0.0": "2" * 40, "v3.0.0": "3" * 40})
    assert fx.post("/api/reconcile", {})[0] == 200
    fx.sources["owner/g"]["releases"] = ">=1,,<9"
    assert fx.post("/api/reconcile", {})[0] == 503
    fx.sources["owner/g"]["releases"] = ">=1,<9"
    assert fx.post("/api/admit-tag", {"repo": "owner/f", "tag": "v1.9.0"})[0] == 200
    assert fx.post("/api/reconcile", {})[0] == 200

    def arrivals():
        items = fx.status()["items"]
        return {item["id"]: item for item in items if item["version"] != "1.0.0"}

    expected = {"a/2.0.0/t1", "a/2.0.0/t2", "a/3.0.0/t1", "a/3.0.0/t2",
                "f/1.9.0/t1", "f/1.10.0/t1", "g/1.10.0/t1"}
    assert set(arrivals()) == expected
    assert eventually(lambda: all(item.get("condition") == "waiting"
                                 for name, item in arrivals().items() if not name.startswith("a/")))
    assert fx.status()["reservations"] == dict(slots=2, queued=0, running=1, publication=0)
    assert fx.dispatches.empty()

    restart(fx, active)
    assert fx.status()["paused"]
    assert set(arrivals()) == expected
    assert fx.plan_bytes(active) == frozen
    assert fx.callback(active, "claim", **facts).status == 200
    assert fx.request("/").status == 200
    assert fx.post("/api/dispatch-paused", {"paused": False})[0] == 200
    run, values = fx.dispatches.get(timeout=12)
    inputs = fx.plan(values)["inputs"]
    assert (inputs["package"], inputs["version"], inputs["target"]["id"]) == ("a", "2.0.0", "t1")
    assert fx.status()["reservations"] == dict(slots=2, queued=1, running=1, publication=0)
    assert fx.callback(values, "claim", attempt=1, run_id=run, run_attempt=1).status == 200
    assert fx.plan_bytes(active) == frozen
    assert fx.dispatches.empty()

    old = fx.plan(published)["inputs"]
    fx.revisions["owner/a"] = ("5" * 40, "6" * 40)
    branch_push = dict(ref="refs/heads/main", deleted=False, repository=dict(full_name="owner/a", default_branch="main"))
    assert fx.push(branch_push, "owner/a main changed ci tree") == 200
    assert eventually(lambda: item_state(fx, published) == "enrolled")
    assert ("owner/a", "5" * 40) in fx.contents
    assert fx.callback(values, "report", attempt=1, run_id=run, run_attempt=1, status="finished", outcome="failed").status == 200
    fx.runs[run].update(status="completed", conclusion="failure")
    _, rebuilt = fx.dispatches.get(timeout=12)
    new = fx.plan(rebuilt)["inputs"]
    assert (new["commit"], new["version"], new["target"]) == (old["commit"], old["version"], old["target"])
    assert new["revision"] == old["revision"] + 1 and new["ci_commit"] == "5" * 40
    assert fx.plan_bytes(active) == frozen


def check_recovery(build):
    # Give this scenario a separate database so its attempts and pause are independent.
    with database() as (_, _, environment, _), patch.dict(os.environ, environment):
        with fixture(build, small=True, coreci_options={"queue-deadline": "7"}) as fx:
            fx.drop_build = True
            fx.hold_cancellation = True
            fx.ready()
            assert fx.webhook("owner/a") == 200
            lost = fx.dropped.get(timeout=10)
            known_run, known = fx.dispatches.get(timeout=10)
            assert fx.callback(known, "claim", attempt=1, run_id=known_run, run_attempt=1).status == 200
            second_run, replacement = fx.dispatches.get(timeout=15)
            assert replacement["request_id"] == lost["request_id"] and replacement["attempt"] == "2"
            assert fx.plan(replacement)["inputs"] == fx.plan(lost)["inputs"]
            assert fx.callback(lost, "claim", attempt=1, run_id=200, run_attempt=1).status == 409
            assert fx.status()["reservations"] == dict(slots=2, queued=1, running=1, publication=0)
            restart(fx, lost, known, replacement)
            assert fx.callback(lost, "claim", attempt=1, run_id=200, run_attempt=1).status == 409
            assert eventually(lambda: fx.cancellations == [second_run])
            restart(fx, lost, known, replacement)
            assert fx.cancellations == [second_run]
            assert fx.status()["reservations"] == dict(slots=2, queued=1, running=1, publication=0)
            fx.runs[second_run].update(status="completed", conclusion="cancelled")
            assert eventually(lambda: item_state(fx, replacement) == "failed")
            assert fx.status()["reservations"] == dict(slots=2, queued=0, running=1, publication=0)
            assert fx.dispatches.empty()


if __name__ == "__main__":
    build = Path(sys.argv[1]).resolve()
    if sys.argv[2:] == ["Http_Recovery"]:
        check_recovery(build)
    else:
        check(build)
