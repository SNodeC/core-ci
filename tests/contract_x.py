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


def restart(fx, *documents, hard=False):
    def identities():
        status = fx.status()
        items = [(item["id"], item.get("request"), item.get("attempt"), item.get("run"), item["state"])
                 for item in status["items"]]
        return status["paused"], status["reservations"], items, status["publications"]

    before = identities()
    frozen = [fx.plan_bytes(values) for values in documents]
    fx.restart(hard=hard)
    assert eventually(lambda: fx.status()["ready"])
    assert identities() == before
    assert [fx.plan_bytes(values) for values in documents] == frozen


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
        fx.ledger = dict(publication=publication["publication_id"], entries=[], receipts=[receipt])
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


def check_dispatch(build, failure):
    with database() as (_, _, environment, _), patch.dict(os.environ, environment):
        with fixture(build, small=True, coreci_options={"queue-deadline": "45"}) as fx:
            fx.sources["owner/a"]["targets"] = ["t1"]
            if failure == 422:
                fx.dispatch_status = failure
            else:
                fx.dispatch_responses.append(failure)
            fx.ready()
            assert fx.webhook("owner/a") == 200
            assert eventually(lambda: len(fx.dispatch_posts) >= 1)
            original, started_at = fx.dispatch_posts[0]
            original_bytes = fx.plan_bytes(original)
            if failure == 422:
                def rejected():
                    items = fx.status()["items"]
                    return len(items) == 1 and items[0]["state"] == "failed"
                assert eventually(rejected, seconds=16), (fx.dispatch_posts, fx.status())
                assert fx.status()["items"][0]["reason"] == "dispatch rejected 422"
                assert len(fx.dispatch_posts) == 4
                assert len(fx.runs) == 0
                assert fx.status()["reservations"] == dict(slots=2, queued=0, running=0, publication=0)
            else:
                assert eventually(lambda: len(fx.dispatch_posts) >= 2, seconds=12), fx.dispatch_posts
                run, recovered = fx.dispatches.get(timeout=5)
                assert recovered == original and recovered["attempt"] == "1"
                assert fx.callback(recovered, "claim", attempt=1, run_id=run, run_attempt=1).status == 200
                assert len(fx.dispatch_posts) == 2
                assert len(fx.runs) == 1
                assert fx.status()["reservations"] == dict(slots=2, queued=0, running=1, publication=0)
            assert all(values == original for values, _ in fx.dispatch_posts)
            timestamps = [t for _, t in fx.dispatch_posts]
            assert timestamps[-1] > started_at
            assert all(later - earlier >= 0.4 for earlier, later in zip(timestamps, timestamps[1:]))
            assert fx.plan_bytes(original) == original_bytes


def check_claimed_cancellation(build):
    with database() as (_, _, environment, _), patch.dict(os.environ, environment):
        with fixture(build, small=True, coreci_options={"execution-deadline": "4"}) as fx:
            fx.sources["owner/a"]["targets"] = ["t1"]
            fx.hold_cancellation = True
            fx.ready()
            assert fx.webhook("owner/a") == 200
            run, values = fx.dispatches.get(timeout=10)
            facts = dict(attempt=1, run_id=run, run_attempt=1)
            assert fx.callback(values, "claim", **facts).status == 200
            fx.runs[run]["status"] = "in_progress"
            assert eventually(lambda: run in fx.cancellations)
            assert fx.callback(values, "report", **facts, status="finished", outcome="failed", reason="cancelled").status == 200
            assert item_state(fx, values) == "processing"
            assert fx.status()["reservations"]["running"] == 1
            fx.runs[run].update(status="completed", conclusion="cancelled")
            assert eventually(lambda: item_state(fx, values) == "failed")
            before = fx.status()
            item = before["items"][0]
            assert item["reason"] == "cancelled"
            assert before["reservations"]["running"] == 0
            fx.runs[run].update(status="in_progress", conclusion=None, run_attempt=2)
            assert fx.callback(values, "claim", **dict(facts, run_attempt=2)).status == 409
            fx.runs[run].update(status="completed", conclusion="success")
            assert fx.callback(values, "report", **dict(facts, run_attempt=2), status="finished", outcome="failed").status == 409
            restart(fx, values, hard=True)
            assert fx.status()["items"] == before["items"]
            assert fx.status()["reservations"] == before["reservations"]
            assert len(fx.dispatch_posts) == 1


def check_hard_restart(build, checkpoint):
    with database() as (proxy, cli, environment, _), patch.dict(os.environ, environment):
        with fixture(build, small=True, coreci_options={"queue-deadline": "7"}) as fx:
            fx.sources["owner/a"]["targets"] = ["t1"]
            fx.ready()
            if checkpoint in ("pending", "sent"):
                field = json.dumps({"dispatch": checkpoint}, separators=(",", ":"))[1:-1]
                proxy.arm(field.encode().hex().upper(), "commit")
            assert fx.webhook("owner/a") == 200
            if checkpoint in ("pending", "sent"):
                assert proxy.fired.wait(10), f"commit boundary for {checkpoint} was not reached"
                raw = subprocess.check_output(
                    [*cli, "probe", "-e", "SELECT HEX(payload) FROM coreci_records WHERE section = 'attempts'"], text=True)
                record = json.loads(bytes.fromhex(raw.strip()))
                assert record["dispatch"] == checkpoint and record["run"] == 0
                assert not fx.dispatch_posts
                original = dict(request_id=record["id"], attempt="1", feed_branch="feed")
                fx.restart(hard=True)
                proxy.release_commit.set()
                run, resumed = fx.dispatches.get(timeout=18)
                assert resumed["request_id"] == original["request_id"]
                assert resumed["attempt"] == ("1" if checkpoint == "pending" else "2")
                assert fx.plan_bytes(original) == record["document"].encode()
                assert fx.plan(resumed)["inputs"] == fx.plan(original)["inputs"]
                assert fx.callback(resumed, "claim", attempt=int(resumed["attempt"]), run_id=run, run_attempt=1).status == 200
                assert fx.status()["reservations"] == dict(slots=2, queued=0, running=1, publication=0)
                assert len(fx.dispatch_posts) == 1
                if checkpoint == "sent":
                    assert fx.callback(original, "claim", attempt=1, run_id=run + 100, run_attempt=1).status == 409
            else:
                run, values = fx.dispatches.get(timeout=10)
                facts = dict(attempt=1, run_id=run, run_attempt=1)
                assert fx.callback(values, "claim", **facts).status == 200
                restart(fx, values, hard=True)
                assert fx.callback(values, "claim", **facts).status == 200
                assert len(fx.dispatch_posts) == 1


def check_pagination(build):
    with database() as (_, _, environment, _), patch.dict(os.environ, environment):
        with fixture(build, small=True) as fx:
            fx.sources["owner/a"]["targets"] = ["t1"]
            tag_path = "/repos/owner/a/tags"
            run_path = "/repos/Fixture/Workflows/actions/workflows/build.yml/runs"
            fx.tags["owner/a"] = {f"v1.{minor}.0": "2" * 40 for minor in range(101)}
            fx.tags["owner/a"]["v2.0.0"] = "3" * 40
            fx.ambiguous_build = True
            fx.run_list_response.clear()
            fx.ready()
            run, dispatch = fx.dispatches.get(timeout=12)
            assert (tag_path, 1) in fx.page_requests and (tag_path, 2) in fx.page_requests
            assert fx.plan(dispatch)["inputs"]["version"] == "2.0.0"
            for i in range(201):
                fx.create_run(f"build unrelated-{i}", status="completed", conclusion="success")
            fx.foreign_links.add(run_path)
            fx.run_list_response.set()
            assert eventually(lambda: fx.page_requests.count((run_path, 1)) >= 2)
            assert all(page == 1 for path, page in fx.page_requests if path == run_path)
            assert fx.status()["items"][0]["run"] == 0
            fx.foreign_links.remove(run_path)
            assert eventually(lambda: fx.status()["items"][0]["run"] == run)
            assert (run_path, 2) in fx.page_requests and (run_path, 3) in fx.page_requests
            assert fx.callback(dispatch, "claim", attempt=1, run_id=run, run_attempt=1).status == 200
            assert len(fx.dispatch_posts) == 1


def check_rerun(build):
    with database() as (_, _, environment, _), patch.dict(os.environ, environment):
        with fixture(build, small=True) as fx:
            fx.sources["owner/a"]["targets"] = ["t1"]
            fx.ready()
            assert fx.webhook("owner/a") == 200
            run, values = fx.dispatches.get(timeout=10)
            facts = dict(attempt=1, run_id=run, run_attempt=1)
            assert fx.callback(values, "claim", **facts).status == 200
            with fx.run_lock:
                fx.run_attempts[run, 1] = dict(fx.runs[run], status="completed", conclusion="cancelled")
                fx.runs[run].update(run_attempt=2, status="in_progress")
            assert eventually(lambda: item_state(fx, values) == "failed", seconds=8)
            before = fx.status()["items"]
            assert before[0]["reason"] == "conclusion cancelled, no report"
            assert fx.status()["reservations"]["running"] == 0
            assert fx.callback(values, "claim", **dict(facts, run_attempt=2)).status == 409
            fx.runs[run].update(status="completed", conclusion="success")
            restart(fx, values, hard=True)
            assert fx.status()["items"] == before
            assert len(fx.dispatch_posts) == 1


def finish_build(fx, run, values):
    facts = dict(attempt=int(values["attempt"]), run_id=run, run_attempt=1)
    assert fx.callback(values, "claim", **facts).status == 200
    assert fx.callback(values, "report", **facts, status="finished", outcome="built", artifact_id=5,
                       artifact_digest="a" * 64, inventory='[{"name":"a"}]').status == 200
    fx.runs[run].update(status="completed", conclusion="success")


def check_pending_publication(build):
    with database() as (_, _, environment, _), patch.dict(os.environ, environment):
        with fixture(build, small=True, virtual_clock=True, wave_deadline=1200) as fx:
            fx.sources["owner/a"]["targets"] = ["t1"]
            fx.ready()
            assert fx.webhook("owner/a") == 200
            run, build_request = fx.dispatches.get(timeout=10)
            finish_build(fx, run, build_request)
            first, publication = fx.dispatches.get(timeout=10)
            pid = publication["publication_id"]
            frozen = fx.plan(publication)
            assert eventually(lambda: fx.status()["publications"][0]["run"] == first)
            title = f"publish {pid}/1"
            fx.allowed_duplicate_titles.add(title)
            conflicting = fx.create_run(title, status="pending", group="publish-feed")
            assert fx.runs[first]["conclusion"] == "cancelled"
            assert fx.callback(publication, "claim", attempt=1, run_id=conflicting, run_attempt=1).status == 409
            fx.runs[conflicting].update(status="completed", conclusion="success")
            assert eventually(lambda: fx.status()["publications"][0]["state"] == "completed")
            fx.advance(4)
            assert eventually(lambda: len(fx.status()["publications"]) == 2)
            assert fx.status()["reservations"]["publication"] == 1
            assert len(fx.dispatch_posts) == 2
            fx.advance(65)
            recovered_run, recovered = fx.dispatches.get(timeout=10)
            assert recovered["publication_id"] == pid and recovered["attempt"] == "2"
            assert fx.plan(recovered)["items"] == frozen["items"]
            assert fx.callback(recovered, "claim", attempt=2, run_id=recovered_run, run_attempt=1).status == 200
            receipt = dict(publication=pid, attempt=2, request=build_request["request_id"], outcome="published")
            latest = "e" * 40
            fx.snapshots["refs/snapshots/feed/" + pid] = latest
            fx.ledgers[latest] = dict(publication=pid, entries=[], receipts=[receipt])
            fx.head = latest
            fx.runs[recovered_run].update(status="completed", conclusion="success")
            assert eventually(lambda: item_state(fx, build_request) == "published")
            assert eventually(lambda: fx.status()["reservations"]["publication"] == 0)
            assert all(p["reconciled"] for p in fx.status()["publications"])
            assert len(fx.dispatch_posts) == 3


def check_restore(build):
    with database() as (_, _, environment, _), patch.dict(os.environ, environment):
        with fixture(build, small=True, virtual_clock=True, coreci_options={"feed-branch": "feed-staging"}) as fx:
            fx.sources["owner/a"]["targets"] = ["t1"]
            fx.ready()
            assert fx.webhook("owner/a") == 200
            build_run, build_request = fx.dispatches.get(timeout=10)
            finish_build(fx, build_run, build_request)
            branch, pre_x = fx.feed_branch, fx.head
            pre_x_ref = "refs/snapshots/feed/pre-x"
            fx.snapshots[pre_x_ref] = pre_x

            def push_publication(commit, lag=False):
                run, values = fx.dispatches.get(timeout=10)
                publication = values["publication_id"]
                snapshot = f"refs/snapshots/{branch}/{publication}"
                plan = fx.plan(values)
                assert fx.callback(values, "claim", attempt=int(values["attempt"]), run_id=run, run_attempt=1).status == 200
                receipt = dict(publication=publication, attempt=int(values["attempt"]), request=build_request["request_id"],
                               outcome="published", reason="already published" if not lag else "")
                entry = dict(plan["items"][0], published_at=fx.now())
                fx.ledgers[commit] = dict(publication=publication, entries=[entry], receipts=[receipt])
                if not lag:
                    fx.snapshots[snapshot] = commit
                fx.head = commit
                fx.runs[run].update(status="completed", conclusion="success")
                return snapshot, plan

            published_head = "e" * 40
            published_ref, original = push_publication(published_head, lag=True)
            assert eventually(lambda: published_head in fx.ledger_reads)
            assert fx.status()["feed_commit"] == pre_x
            fx.advance(5)
            assert eventually(lambda: fx.ledger_reads.count(published_head) >= 2)
            assert len(fx.status()["publications"]) == 1
            fx.head = pre_x
            assert eventually(lambda: fx.status().get("feed_status") == f"feed head behind: {pre_x}")
            assert fx.status()["feed_commit"] == pre_x
            assert len(fx.status()["publications"]) == 1
            fx.snapshots[published_ref] = published_head
            fx.head = published_head
            assert eventually(lambda: item_state(fx, build_request) == "published")
            assert eventually(lambda: fx.status()["reservations"]["publication"] == 0)

            title = f"restore {branch} {pre_x_ref}"
            fx.allowed_duplicate_titles.add(title)
            old_time = fx.now() - 20
            fx.head = pre_x
            for evidence in (None, (pre_x_ref, "failure", None), (published_ref, "success", None),
                             (pre_x_ref, "success", old_time)):
                count = fx.ledger_reads.count(pre_x)
                if evidence:
                    fx.advance(3)
                    fx.restore_run(branch, *evidence)
                assert eventually(lambda: fx.ledger_reads.count(pre_x) > count)
                assert eventually(lambda: fx.status().get("feed_status") == f"feed head behind: {pre_x}")
                assert fx.status()["feed_commit"] == published_head
                assert len(fx.status()["publications"]) == 1

            fx.advance(3)
            fx.restore_run(branch, pre_x_ref, "success")
            assert eventually(lambda: fx.status().get("feed_status") == f"restored to {pre_x_ref}")
            assert fx.status()["paused"] and fx.status()["feed_commit"] == pre_x
            assert item_state(fx, build_request) == "built"
            assert len(fx.dispatch_posts) == 2
            assert fx.request("/").status == 200
            assert fx.post("/api/dispatch-paused", {"paused": False})[0] == 200
            latest = "d" * 40
            latest_ref, republished = push_publication(latest)
            assert republished["items"] == original["items"]
            assert eventually(lambda: item_state(fx, build_request) == "published")
            assert eventually(lambda: fx.status()["reservations"]["publication"] == 0)
            assert fx.status()["items"][0]["reason"] == "already published"

            fx.advance(3)
            fx.restore_run(branch, pre_x_ref, "success")
            fx.head = pre_x
            assert eventually(lambda: fx.status().get("feed_status") == f"restored to {pre_x_ref}")
            assert item_state(fx, build_request) == "built"
            fx.advance(3)
            fx.restore_run(branch, latest_ref, "success")
            fx.head = latest
            assert eventually(lambda: fx.status().get("feed_status") == f"restored to {latest_ref}")
            assert fx.status()["paused"] and item_state(fx, build_request) == "published"
            assert fx.post("/api/dispatch-paused", {"paused": False})[0] == 200
            count = len(fx.ledger_reads)
            fx.advance(5)
            assert eventually(lambda: len(fx.ledger_reads) > count + 1)
            assert len(fx.dispatch_posts) == 3


if __name__ == "__main__":
    build = Path(sys.argv[1]).resolve()
    mode = sys.argv[2] if len(sys.argv) > 2 else "Http"
    failures = {"Http_Dispatch_4xx": 422, "Http_Dispatch_5xx": 503, "Http_Dispatch_Connection": 0}
    if mode == "Http_Recovery":
        check_recovery(build)
    elif mode in failures:
        check_dispatch(build, failures[mode])
    elif mode == "Http_Claimed_Cancel":
        check_claimed_cancellation(build)
    elif mode.startswith("Http_Kill_"):
        check_hard_restart(build, mode.removeprefix("Http_Kill_").lower())
    elif mode == "Http_Pagination":
        check_pagination(build)
    elif mode == "Http_Rerun":
        check_rerun(build)
    elif mode == "Http_Publish_Pending":
        check_pending_publication(build)
    elif mode == "Http_Restore":
        check_restore(build)
    else:
        check(build)
