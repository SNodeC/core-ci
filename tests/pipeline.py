"""Local public-API pipeline checks: real isolated MariaDB, synthetic TLS GitHub/feed."""

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import hmac
import http.server
import json
import os
from pathlib import Path
import pwd
import ssl
import subprocess
import threading
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request

import lifecycle


def exercise(arguments):
    root = Path(arguments.directory)
    root.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="r-", dir=root))
    data = root / "data"
    data.mkdir(mode=0o700)
    password, secret, key, tls_key, cert = [root / name for name in ("password", "hmac", "app.pem", "tls.key", "tls.pem")]
    password.write_text("")
    secret.write_bytes(b"synthetic-controller-test-secret-32-bytes")
    lifecycle.run(["openssl", "genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:2048", "-out", str(key)])
    lifecycle.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1", "-keyout", str(tls_key),
                   "-out", str(cert), "-subj", "/CN=api.github.com", "-addext", "subjectAltName=DNS:api.github.com,IP:127.0.0.1"])
    for private in (password, secret, key, tls_key):
        private.chmod(0o600)
    lifecycle.run(["mariadb-install-db", "--no-defaults", "--datadir=" + str(data), "--auth-root-authentication-method=normal", "--skip-test-db"])
    data.chmod(0o700)
    socket = root / "db.sock"
    db_log = (root / "database.log").open("w")
    database = subprocess.Popen(["/usr/sbin/mariadbd", "--no-defaults", "--datadir=" + str(data), "--socket=" + str(socket), "--skip-networking",
                                 "--pid-file=" + str(root / "database.pid"), "--user=" + pwd.getpwuid(os.geteuid()).pw_name,
                                 "--innodb-buffer-pool-size=32M"], stdout=db_log, stderr=subprocess.STDOUT)
    application, server, thread = None, None, None
    canonical = lambda value: json.dumps(value, sort_keys=True, separators=(",", ":"))
    sha = lambda value: hashlib.sha1(value).hexdigest()
    blob_id = lambda value: sha(b"blob " + str(len(value)).encode() + b"\0" + value)
    trace, errors, runs, jobs, dispatches, blobs, trees, commits, public_files = [], [], {}, {}, [], {}, {}, {}, {}
    workflow_writes = []
    lock = threading.Lock()
    missing_artifacts, unreadable = set(), set()
    available, references = {}, {}
    holds = {"tags": None, "commit": None, "token": None, "publication": False}
    token_started, token_release = threading.Event(), threading.Event()
    publication_started, publication_release = threading.Event(), threading.Event()
    tags_started, tags_release = threading.Event(), threading.Event()
    commit_started, commit_release = threading.Event(), threading.Event()
    artifact_started, artifact_release = threading.Event(), threading.Event()
    ids = {"core": 1, "app": 2, "side": 3, "join": 4, "worker": 9, "feed": 10}
    tooling = "f" * 40
    workflow = (Path(__file__).resolve().parents[1] / "deployment/source/core-ci-build.yml.in").read_text()
    workflow = workflow.replace("@WORKER@", "Owner/worker").replace("@TOOLING_SHA@", tooling).encode()
    publisher = b"synthetic approved publisher workflow"
    matrices = []
    for arch in ("amd64", "arm64"):
        matrices.append({"id": "debian-" + arch, "family": "native", "distribution": "debian", "suite": "trixie", "arch": arch,
                         "runner": "ubuntu-24.04", "build": {"format": "deb", "image": "debian:trixie", "platform": "linux/" + arch}})
    matrices.append({"id": "fedora", "family": "native", "distribution": "fedora", "suite": "43", "arch": "x86_64",
                     "runner": "ubuntu-24.04", "build": {"format": "rpm", "image": "fedora:43", "platform": "linux/amd64"}})
    for release, package_format in (("24.10", "ipk"), ("25.12", "apk")):
        matrices.append({"id": "openwrt-" + package_format, "family": "openwrt", "distribution": "openwrt", "suite": release,
                         "arch": "x86_64", "runner": "ubuntu-24.04", "build": {"format": package_format, "target": "x86/64"}})
    definitions, enrolled = {}, {}

    def tree_files(repository, commit, files):
        hierarchy = {}
        for path, content in files.items():
            current = hierarchy
            parts = path.split("/")
            for part in parts[:-1]:
                current = current.setdefault(part, {})
            identity = blob_id(content)
            blobs[identity] = content
            current[parts[-1]] = identity

        def encode(directory):
            entries = [{"path": name, "mode": "040000" if isinstance(value, dict) else "100644",
                        "type": "tree" if isinstance(value, dict) else "blob", "sha": encode(value) if isinstance(value, dict) else value}
                       for name, value in sorted(directory.items())]
            identity = sha(canonical(entries).encode())
            trees[identity] = entries
            return identity

        commits[(repository, commit)] = encode(hierarchy)

    for name in ("core", "app", "side", "join"):
        dependencies = [] if name == "core" else ["core"] + (["app", "side"] if name == "join" else [])
        definition = {"schema": 1, "title": name, "package": name, "depends_on": dependencies,
                      "build": {"native": "ci/build.sh", "openwrt": "ci/openwrt.sh"},
                      "matrix": matrices[:1] if arguments.case.startswith("preflight-") else matrices}
        content = canonical(definition).encode()
        revision = sha((name + "-source").encode())
        definitions[name] = revision
        available[name] = ["v1.0.0"]
        references[(name, "v1.0.0")] = revision
        enrolled[name] = {"id": ids[name], "repository": "Owner/" + name, "name": name, "branch": "main",
                          "revision": revision, "blob_sha": blob_id(content), "definition": definition}
        for commit in (revision, "main"):
            actual = revision if commit != "main" else sha((name + "-workflow").encode())
            tree_files(name, actual, {"ci/packages.json": content, ".github/workflows/core-ci-build.yml": workflow})
            if commit == "main":
                definitions[name + "-main"] = actual
    definitions["worker-main"] = "e" * 40
    tree_files("worker", "e" * 40, {".github/workflows/core-ci-publish.yml": publisher, ".github/workflows/core-ci-status.yml": publisher})

    def release(name, tag, visible=True):
        revision = sha((name + "-" + tag).encode())
        references[(name, tag)] = revision
        if visible:
            available[name] = list(dict.fromkeys(available[name] + [tag]))
        tree_files(name, revision, {"ci/packages.json": canonical(enrolled[name]["definition"]).encode(),
                                   ".github/workflows/core-ci-build.yml": workflow})
        return revision

    class GitHub(http.server.BaseHTTPRequestHandler):
        def log_message(self, *values):
            pass

        def reply(self, status, body, raw=False):
            content = body if raw else canonical(body).encode()
            self.send_response(status)
            self.send_header("Content-Length", str(len(content)))
            self.send_header("Connection", "close")
            self.end_headers()
            try:
                self.wfile.write(content)
            except (BrokenPipeError, ConnectionResetError):
                pass
            self.close_connection = True

        def do_PUT(self):
            import base64
            trace.append((time.monotonic(), "PUT", self.path))
            try:
                body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                assert self.path == "/repos/Owner/core/contents/.github/workflows/core-ci-build.yml"
                assert body["branch"] == "main"
                content = base64.b64decode(body["content"], validate=True)
                assert content == workflow
                workflow_writes.append(body)
                revision = sha(("installed-" + str(len(workflow_writes))).encode())
                tree_files("core", revision, {"ci/packages.json": canonical(enrolled["core"]["definition"]).encode(),
                                              ".github/workflows/core-ci-build.yml": content})
                definitions["core-main"] = revision
                self.reply(201, {"commit": {"sha": revision}, "content": {"sha": blob_id(content)}})
            except Exception as failure:
                errors.append(str(failure))
                self.reply(500, {"error": str(failure)})

        def do_POST(self):
            with lock:
                trace.append((time.monotonic(), "POST", self.path))
            try:
                body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                if self.path.startswith("/app/installations/"):
                    assert body["repository_ids"][0] in ids.values() and len(body["repository_ids"]) == 1
                    assert body["permissions"] in ({"contents": "read"}, {"actions": "read"}, {"actions": "write"}, {"contents": "write", "workflows": "write"})
                    if body["repository_ids"] == [holds["token"]] and body["permissions"] == {"actions": "write"}:
                        token_started.set()
                        assert token_release.wait(15), "test token hold was not released"
                    self.reply(201, {"token": "synthetic-token", "expires_at": "2099-01-01T00:00:00Z"})
                elif "core-ci-status.yml/dispatches" in self.path:
                    self.reply(200, {"workflow_run_id": 999999})
                else:
                    selected = json.loads(body["inputs"]["assignment"])
                    identity = selected["execution"]
                    stage = "publication" if "publish.yml" in self.path else "build"
                    state = json.loads(lifecycle.query(socket, "SELECT document FROM boundary.graph_state"))["executions"][identity]["attempts"][-1]["state"]
                    assert state == ("publishing" if stage == "publication" else "building"), "dispatch preceded committed authority"
                    assert body["inputs"]["assignment_sha256"] == hashlib.sha256(canonical(selected).encode()).hexdigest()
                    assert len(selected["dependencies"]) == len(selected["source"]["depends_on"])
                    for prerequisite in selected["dependencies"]:
                        assert prerequisite["receipt"]["feed_revision"]
                    with lock:
                        run_id = 1000 + len(dispatches)
                        repository = "worker" if stage == "publication" else selected["source"]["name"]
                        runs[run_id] = {"id": run_id, "run_attempt": 1, "repository": {"id": ids[repository]},
                            "head_repository": {"id": ids[repository]}, "actor": {"id": 42}, "event": "workflow_dispatch",
                            "display_title": ("Publish " if stage == "publication" else "Build ") + selected["label"],
                            "head_sha": definitions[repository + "-main"], "head_branch": "main", "path": ".github/workflows/core-ci-" + ("publish" if stage == "publication" else "build") + ".yml",
                            "status": "queued", "conclusion": None}
                        if arguments.case == "changed" and run_id == 1000:
                            changed = sha(b"branch advanced after preflight")
                            tree_files(repository, changed, {"ci/packages.json": canonical(enrolled[repository]["definition"]).encode(),
                                                            ".github/workflows/core-ci-build.yml": workflow})
                            runs[run_id]["head_sha"] = changed
                        dispatches.append({"id": run_id, "stage": stage, "selected": selected, "inputs": body["inputs"], "repository": repository})
                    if stage == "publication" and holds["publication"]:
                        publication_started.set()
                        assert publication_release.wait(15), "test publication hold was not released"
                    if arguments.case == "cold" and run_id == 1000:
                        self.reply(204, b"", True)
                    else:
                        self.reply(200, {"workflow_run_id": run_id})
            except Exception as failure:
                errors.append(str(failure))
                self.reply(500, {"error": str(failure)})

        def do_GET(self):
            with lock:
                trace.append((time.monotonic(), "GET", self.path))
            try:
                parsed = urllib.parse.urlsplit(self.path)
                if parsed.path.startswith("/receipts/"):
                    self.reply(200 if parsed.path[1:] in public_files else 404, public_files.get(parsed.path[1:], b""), True)
                else:
                    parts = parsed.path.split("/")
                    repository = parts[3]
                    suffix = "/" + "/".join(parts[4:]) if len(parts) > 4 else ""
                    assert self.headers.get("Authorization") == "Bearer synthetic-token"
                    if not suffix:
                        self.reply(404 if repository in unreadable else 200,
                                   {"id": ids[repository], "name": repository, "full_name": "Owner/" + repository})
                    elif suffix == "/tags":
                        listed = list(available[repository])
                        if repository == holds["tags"]:
                            tags_started.set()
                            assert tags_release.wait(15), "test tag hold was not released"
                        self.reply(200, [{"name": tag} for tag in listed])
                    elif suffix.startswith("/git/ref/tags/"):
                        tag = urllib.parse.unquote(suffix[len("/git/ref/tags/"):])
                        self.reply(200, {"object": {"sha": references[(repository, tag)], "type": "commit"}})
                    elif suffix == "/commits" and repository == "feed":
                        path = urllib.parse.parse_qs(parsed.query)["path"][0]
                        commit = definitions.get(path)
                        self.reply(200, [{"sha": commit, "commit": {"tree": {"sha": commits[("feed", commit)]}}}] if commit else [])
                    elif suffix.startswith("/commits/"):
                        ref = urllib.parse.unquote(suffix[9:])
                        commit = definitions[repository + "-main"] if ref in ("main", "alternate") else ref
                        if commit == holds["commit"]:
                            commit_started.set()
                            assert commit_release.wait(15), "test commit hold was not released"
                        self.reply(200, {"sha": commit, "commit": {"tree": {"sha": commits[(repository, commit)]}}})
                    elif suffix.startswith("/git/trees/"):
                        def flatten(identity, prefix=""):
                            result = []
                            for entry in trees[identity]:
                                item = {**entry, "path": prefix + entry["path"]}
                                result.append(item)
                                if entry["type"] == "tree":
                                    result.extend(flatten(entry["sha"], item["path"] + "/"))
                            return result
                        self.reply(200, {"truncated": False, "tree": flatten(suffix[11:]) if parsed.query else trees[suffix[11:]]})
                    elif suffix.startswith("/git/blobs/"):
                        import base64
                        content = blobs[suffix[11:]]
                        self.reply(200, {"encoding": "base64", "size": len(content), "content": base64.b64encode(content).decode()})
                    elif "/actions/workflows/" in suffix and suffix.endswith("/runs"):
                        selected_runs = [run for run in runs.values() if run["repository"]["id"] == ids[repository]]
                        listing = [{**run, "run_attempt": 2} for run in selected_runs]
                        listing += [{**run, "id": run["id"] + 9000, "actor": {"id": 999}} for run in selected_runs]
                        self.reply(200, {"total_count": len(listing), "workflow_runs": listing})
                    elif "/artifacts" in suffix:
                        run_id = int(parts[6])
                        if arguments.case == "overlap" and run_id == 1000:
                            artifact_started.set()
                            assert artifact_release.wait(10), "test artifact hold was not released"
                        selected = next(row["selected"] for row in dispatches if row["id"] == run_id)
                        artifacts = [{"id": run_id + 10000, "name": selected["artifact_name"], "expired": run_id in missing_artifacts,
                            "digest": "sha256:" + "a" * 64, "size_in_bytes": 100, "created_at": "2026-10-27T10:01:00Z", "workflow_run": {"id": run_id}}]
                        if run_id in missing_artifacts and arguments.case == "missing":
                            artifacts = []
                        self.reply(200, {"total_count": len(artifacts), "artifacts": artifacts})
                    elif "/attempts/1" in suffix:
                        run_id = int(parts[6])
                        if suffix.endswith("/jobs"):
                            values = [jobs[run_id]] if run_id in jobs else []
                            self.reply(200, {"total_count": len(values), "jobs": values})
                        else:
                            self.reply(200, runs[run_id])
                    else:
                        raise AssertionError("unexpected request " + self.path)
            except Exception as failure:
                errors.append(str(failure))
                self.reply(500, {"error": str(failure)})

    try:
        lifecycle.wait_sql(socket, database)
        schema = (Path(__file__).resolve().parents[1] / "sql/schema.sql").read_text()
        lifecycle.query(socket, "CREATE DATABASE boundary; USE boundary; " + schema)
        snapshot = json.loads(canonical(enrolled))
        if arguments.case == "sources":
            snapshot["join"]["definition"]["depends_on"] = ["core"]
        state = {"generation": 1, "next_revision": 1, "repositories": snapshot, "executions": {}}
        lifecycle.query(socket, "UPDATE boundary.graph_state SET document=CONVERT(X'" + canonical(state).encode().hex() + "' USING utf8mb4)")
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), GitHub)
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(cert, tls_key)
        server.socket = context.wrap_socket(server.socket, server_side=True)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        port, webhook_port = lifecycle.free_port(), lifecycle.free_port()
        origin = "http://127.0.0.1:" + str(port)
        webhook_origin = "https://127.0.0.1:" + str(webhook_port)
        webhook_context = ssl.create_default_context(cafile=str(cert))
        config = root / "controller.conf"
        options = {"github.app-id": 42, "github.installation-id": 7, "github.bot-id": 42, "github.allowed-owner": "Owner",
                   "github.repository": ["Owner/" + name + "=" + str(identity) for name, identity in ids.items() if name != "feed"],
                   "github.private-key-file": str(key), "github.webhook-secret-file": str(secret), "pipeline.worker-repository": "Owner/worker",
                   "pipeline.tooling-sha": tooling, "pipeline.source-workflow-blob": blob_id(workflow), "pipeline.publication-workflow-blob": blob_id(publisher),
                   "pipeline.status-workflow": ".github/workflows/core-ci-status.yml", "pipeline.status-workflow-blob": blob_id(publisher),
                   "pipeline.feed-repository": "Owner/feed", "pipeline.feed-id": 10, "pipeline.feed-branch": "main",
                   "pipeline.feed-base-url": "https://127.0.0.1:" + str(server.server_port) + "/", "pipeline.gpg-fingerprint": "A" * 40,
                   "pipeline.usign-key-sha256": "b" * 64, "pipeline.apk-key-sha256": "c" * 64}
        config.write_text("\n".join(name + "=" + json.dumps(value) for name, value in options.items()) + "\n")
        command = [arguments.executable, "--config-file", str(config), "--log-file", str(root / "controller.log"), "database", "--mode", "unmanaged",
                   "--socket", str(socket), "--name", "boundary", "--user", "root", "--password-file", str(password), "web", "local", "--port", str(port),
                   "webhook", "--disabled=false", "local", "--port", str(webhook_port), "tls", "--cert", str(cert), "--cert-key", str(tls_key),
                   "github-http", "remote", "--host", "127.0.0.1", "--port", str(server.server_port), "tls", "--ca-cert", str(cert),
                   "feed-http", "remote", "--host", "127.0.0.1", "--port", str(server.server_port), "tls", "--ca-cert", str(cert)]

        def api(path, body=None, headers=None, webhook=False):
            outgoing = None if body is None else canonical(body).encode()
            request = urllib.request.Request((webhook_origin if webhook else origin) + path, data=outgoing,
                headers=headers or {"Content-Type": "application/json"})
            try:
                with urllib.request.urlopen(request, timeout=10, context=webhook_context if webhook else None) as response:
                    result = response.status, json.load(response)
            except urllib.error.HTTPError as response:
                result = response.code, json.load(response)
            return result

        def wait(predicate, description):
            deadline = time.monotonic() + 10
            while not predicate() and time.monotonic() < deadline:
                assert application.poll() is None, "controller exited: " + (root / "stdout.log").read_text()[-4000:]
                time.sleep(0.01)
            assert predicate(), description + ": " + (root / "stdout.log").read_text()[-4000:]

        def report(work, job=False, status="completed", conclusion="success", wrong=None):
            run = {**runs[work["id"]], "status": status, "conclusion": conclusion if status == "completed" else None}
            if not job:
                runs[work["id"]] = dict(run)
            if job:
                run = {"id": work["id"] + 20000, "run_id": work["id"], "run_attempt": 1, "head_sha": run["head_sha"],
                       "name": ("Publish " if work["stage"] == "publication" else "build / Build ") + work["selected"]["label"],
                       "status": status, "conclusion": conclusion if status == "completed" else None, "started_at": "2026-10-27T10:00:00Z",
                       "completed_at": "2026-10-27T10:02:00Z" if status == "completed" else None,
                       "steps": [{"name": "Publish checked feed update", "conclusion": "success"}] if work["stage"] == "publication" else []}
            if wrong:
                run.update(wrong)
            if job:
                jobs[work["id"]] = run
            event = "workflow_job" if job else "workflow_run"
            body = {"installation": {"id": 7}, "repository": {"id": ids[work["repository"]], "full_name": "Owner/" + work["repository"]},
                    "action": status, event: run}
            signature = "sha256=" + hmac.new(secret.read_bytes(), canonical(body).encode(), hashlib.sha256).hexdigest()
            return api("/github/webhook", body, {"Content-Type": "application/json", "X-GitHub-Event": event,
                                                "X-Hub-Signature-256": signature}, True)

        def push(name, tag):
            payload = {"installation": {"id": 7}, "repository": {"id": ids[name], "full_name": "Owner/" + name},
                       "ref": "refs/tags/" + tag, "after": references.get((name, tag), "a" * 40), "deleted": False}
            signature = "sha256=" + hmac.new(secret.read_bytes(), canonical(payload).encode(), hashlib.sha256).hexdigest()
            return api("/github/webhook", payload, {"Content-Type": "application/json", "X-GitHub-Event": "push", "X-Hub-Signature-256": signature}, True)

        def finish_boundary(description):
            assert not errors, errors
            assert len({work["selected"]["attempt"] + work["stage"] for work in dispatches}) == len(dispatches), "replacement or duplicate mutation"
            (root / "request-trace.json").write_text(json.dumps(trace, indent=2))
            (root / "graph.json").write_text(json.dumps(api("/api/graph")[1], indent=2))
            print("PASS local synthetic pipeline boundary", arguments.case, description)

        application = subprocess.Popen(command, stdout=(root / "stdout.log").open("w"), stderr=subprocess.STDOUT)
        lifecycle.wait_http(port, application)
        if arguments.case.startswith("preflight-") and arguments.case != "preflight-live":
            publication = "publish" in arguments.case
            known = arguments.case.endswith("known")
            if publication:
                assert api("/api/work/request", {"repository": "Owner/core", "tag": "v1.0.0"})[0] == 200
                wait(lambda: len(dispatches) == 1, "single-target build did not dispatch")
                # Bind the build before holding worker approval or its HTTP mutation.
                assert report(dispatches[0], status="in_progress")[0] == 202
                wait(lambda: any(row["build_run"] for row in api("/api/graph")[1]["executions"]), "build run did not bind")
                if known:
                    holds["publication"] = True
                else:
                    holds["commit"] = definitions["worker-main"]
                assert report(dispatches[0])[0] == 202 and report(dispatches[0], job=True)[0] == 202
                assert (publication_started if known else commit_started).wait(5)
                wait(lambda: any(row["state"] == "publishing" for row in api("/api/graph")[1]["executions"]), "publication authority was not committed")
            else:
                if known:
                    holds["token"] = ids["core"]
                else:
                    holds["commit"] = definitions["core-main"]
                assert api("/api/work/request", {"repository": "Owner/core", "tag": "v1.0.0"})[0] == 200
                assert (token_started if known else commit_started).wait(5)
            checkpoint = json.loads(lifecycle.query(socket, "SELECT document FROM boundary.graph_state"))
            active = next(row for row in checkpoint["executions"].values() if row["source"]["name"] == "core")
            attempt = active["attempts"][-1]
            stage = "publication" if publication else "build"
            assert (stage in attempt.get("preflight", {})) == known
            assert attempt[stage + "_run"] is None
            original_posts = len(dispatches)
            # Simulate process loss before blocked callbacks can classify shutdown errors.
            application.kill()
            application.wait(timeout=5)
            holds.update({"commit": None, "token": None, "publication": False})
            commit_release.set()
            token_release.set()
            publication_release.set()
            application = subprocess.Popen(command, stdout=(root / "restart.log").open("w"), stderr=subprocess.STDOUT)
            lifecycle.wait_http(port, application)
            execution = active["id"]
            expected = "publishing" if publication else "building"
            wait(lambda: next(row for row in api("/api/graph")[1]["executions"] if row["id"] == execution)["state"] == (expected if known else "failed"),
                 "restored preflight boundary was classified incorrectly")
            time.sleep(0.2)
            assert len(dispatches) == original_posts, "restart replayed an assigned mutation"
            restored = next(row for row in api("/api/graph")[1]["executions"] if row["id"] == execution)
            if known:
                assert not restored["failure"] and api("/api/work/retry", {"execution": execution})[0] == 409
                assert any("/actions/" in item[2] and ("/runs?" in item[2] or "/attempts/1" in item[2]) for item in trace), "restored work was not inspected"
            else:
                assert restored["failure"] == "workflow not submitted; dispatch preflight absent"
                assert api("/api/work/retry", {"execution": execution})[0] == 200
                wait(lambda: len(dispatches) == original_posts + 1, "definitely unsubmitted work could not be retried explicitly")
                assert dispatches[-1]["stage"] == "build" and dispatches[-1]["selected"]["revision"] > attempt["revision"]
            finish_boundary("restored preflight absence is definite; committed preflight remains assigned")
        elif arguments.case == "preflight-live":
            holds["commit"] = definitions["core-main"]
            assert api("/api/work/request", {"repository": "Owner/core", "tag": "v1.0.0"})[0] == 200 and commit_started.wait(5)
            assert api("/api/work/reconcile", {})[0] == 200
            assert not any(row["failure"] for row in api("/api/graph")[1]["executions"]) and not dispatches
            commit_release.set()
            wait(lambda: len(dispatches) == 1, "operator reconcile cancelled a live preflight or duplicated its dispatch")
            finish_boundary("ordinary reconcile does not fail an in-flight preflight")
        elif arguments.case == "non-semver":
            assert push("core", "nightly")[0] == 202
            wait(lambda: "invalid semantic version tag" in (root / "stdout.log").read_text(), "non-SemVer push was not handled")
            assert not api("/api/graph")[1]["executions"] and not dispatches
            finish_boundary("non-SemVer notification rejected without selection")
        elif arguments.case in ("backport", "explicit-tag"):
            release("core", "v2.0.0")
            if arguments.case == "backport":
                assert push("core", "v1.0.0")[0] == 202
            else:
                assert api("/api/work/request", {"repository": "Owner/core", "tag": "v1.0.0"})[0] == 200
            expected_tag = "v2.0.0" if arguments.case == "backport" else "v1.0.0"
            wait(lambda: len(dispatches) == 5, "target builds did not dispatch")
            assert all(work["selected"]["source"]["tag"] == expected_tag for work in dispatches)
            if arguments.case == "explicit-tag":
                assert not any("/repos/Owner/core/tags?" in item[2] for item in trace), "explicit tag became an automatic selection"
            finish_boundary("correct automatic highest versus explicit tag")
        elif arguments.case == "shared-tags":
            holds["tags"] = "core"
            assert push("core", "v1.0.0")[0] == 202 and tags_started.wait(5)
            release("core", "v2.0.0")
            assert push("core", "v2.0.0")[0] == 202
            wait(lambda: sum(item[2] == "/repos/Owner/core" for item in trace) >= 2, "second source request did not start")
            time.sleep(0.2)
            assert sum("/repos/Owner/core/tags?" in item[2] for item in trace) == 1, "fixture did not force a shared stale response"
            tags_release.set()
            wait(lambda: len([work for work in dispatches if work["selected"]["source"]["tag"] == "v2.0.0"]) == 5,
                 "pushed-tag floor was lost behind a shared old tag-list response")
            assert sum("/repos/Owner/core/tags?" in item[2] for item in trace) == 1
            newest = [row for row in api("/api/graph")[1]["executions"] if row["repository"] == "Owner/core" and row["tag"] == "v2.0.0"]
            assert len(newest) == 5 and all(not row["superseded_by"] for row in newest)
            finish_boundary("two signed pushes share one stale tag-list response; higher tag selected")
        elif arguments.case == "enrollment-race":
            holds["tags"] = "core"
            with ThreadPoolExecutor(max_workers=1) as pool:
                pending = pool.submit(api, "/api/work/request", {"repository": "Owner/core"})
                assert tags_started.wait(5)
                assert api("/api/repositories/refresh", {"repository": "Owner/core", "branch": "alternate"})[0] == 200
                tags_release.set()
                status, blocked = pending.result(timeout=10)
            assert status == 409 and blocked["error"] == "enrollment changed during source selection" and not dispatches
            assert not api("/api/graph")[1]["executions"]
            finish_boundary("enrollment branch guard preserved")
        else:
            if arguments.case == "workflow":
                endpoint = "/api/repositories/workflow"
                query = {"repository": "Owner/core"}
                status, preview = api(endpoint, query)
                assert status == 200 and preview["blob_sha"] == blob_id(workflow)
                assert api(endpoint, {**query, "write": True, "head": "stale", "blob_sha": preview["blob_sha"]})[0] == 409
                assert not workflow_writes
                assert api(endpoint, {**query, "write": True, "head": preview["head"], "blob_sha": preview["blob_sha"]})[1]["github_committed"] is False
                missing = sha(b"branch without workflow")
                tree_files("core", missing, {"ci/packages.json": canonical(enrolled["core"]["definition"]).encode()})
                definitions["core-main"] = missing
                status, preview = api(endpoint, query)
                assert status == 200 and preview["blob_sha"] == "" and preview["head"] == missing
                status, installed = api(endpoint, {**query, "write": True, "head": missing, "blob_sha": ""})
                assert status == 200 and installed["github_committed"] is True and installed["blob_sha"] == blob_id(workflow)
                assert len(workflow_writes) == 1 and "sha" not in workflow_writes[0]
                assert api(endpoint, query)[1]["blob_sha"] == blob_id(workflow)
            if arguments.case == "sources":
                unreadable.add("side")
                status, blocked = api("/api/work/request", {"repository": "Owner/core", "tag": "v1.0.0"})
                assert status == 503 and "side" in blocked["repositories"] and not dispatches
                assert not api("/api/graph")[1]["executions"]
                unreadable.clear()
                assert api("/api/work/request", {"repository": "Owner/app", "tag": "v1.0.0"})[0] == 409
                assert not dispatches and not api("/api/graph")[1]["executions"], "missing prerequisites triggered an autobuild"
            if arguments.case == "chain":
                notification = {"installation": {"id": 7}, "repository": {"id": 1, "full_name": "Owner/core"},
                                "ref": "refs/tags/v1.0.0", "after": definitions["core"], "deleted": False}
                signature = "sha256=" + hmac.new(secret.read_bytes(), canonical(notification).encode(), hashlib.sha256).hexdigest()
                assert api("/github/webhook", notification, {"Content-Type": "application/json", "X-GitHub-Event": "push", "X-Hub-Signature-256": signature}, True)[0] == 202
            else:
                assert api("/api/work/request", {"repository": "Owner/core", "tag": "v1.0.0"})[0] == 200
            wait(lambda: len(dispatches) == 5, "five root targets were not dispatched")
            assert len(api("/api/graph")[1]["executions"]) == 20
            first = dispatches[0]
            if arguments.case == "cold":
                offset = len(trace)
                assert api("/api/work/reconcile", {})[0] == 200
                wait(lambda: next(row for row in api("/api/graph")[1]["executions"] if row["id"] == first["selected"]["execution"])["build_run"] is not None,
                     "unbound dispatch was not recovered from attempt-one evidence")
                bound = next(row for row in api("/api/graph")[1]["executions"] if row["id"] == first["selected"]["execution"])["build_run"]
                assert bound["run_attempt"] == 1 and bound["id"] == 1000 and len(dispatches) == 5
                assert not any("/repos/Owner/core/git/" in item[2] for item in trace[offset:]), "cold reconciliation repeated immutable workflow proof"
            proof_offset = len(trace)
            assert report(first, status="in_progress")[0] == 202
            wait(lambda: any(row["build_run"] for row in api("/api/graph")[1]["executions"]), "run progress was not persisted")
            time.sleep(0.5)
            if arguments.case == "changed":
                assert any("/repos/Owner/core/commits/" + runs[1000]["head_sha"] in item[2] for item in trace[proof_offset:]), "changed run commit reused stale preflight proof"
                assert any("/repos/Owner/core/git/trees/" in item[2] for item in trace[proof_offset:])
                assert not any("/git/blobs/" in item[2] for item in trace[proof_offset:]), "approval downloaded unused workflow bytes"
            offset = len(trace)
            with ThreadPoolExecutor(max_workers=4) as pool:
                replies = list(pool.map(lambda unused: report(first, status="in_progress"), range(8)))
            time.sleep(0.3)
            assert all(reply[0] == 202 for reply in replies)
            assert len(trace) == offset, "unchanged webhook burst made outgoing requests: " + repr(trace[offset:])
            generation = api("/api/graph")[1]["generation"]
            for wrong in ({"actor": {"id": 999}}, {"path": ".github/workflows/unapproved.yml"}, {"run_attempt": 2}):
                assert report(first, status="in_progress", wrong=wrong)[0] == 202
            assert api("/github/webhook", {}, {"Content-Type": "application/json", "X-GitHub-Event": "ping", "X-Hub-Signature-256": "invalid"}, True)[0] == 400
            time.sleep(0.1)
            assert api("/api/graph")[1]["generation"] == generation, "invalid execution evidence changed the graph"

            if arguments.case == "queued-job":
                assert report(first, job=True, status="queued", wrong={"started_at": None})[0] == 202
                wait(lambda: next(row for row in api("/api/graph")[1]["executions"] if row["id"] == first["selected"]["execution"])["build_job"].get("job_status") == "queued", "queued job report did not apply")
                assert report(first, job=True, status="in_progress")[0] == 202
                wait(lambda: next(row for row in api("/api/graph")[1]["executions"] if row["id"] == first["selected"]["execution"])["build_job"].get("job_status") == "in_progress", "running job report did not apply")
                assert report(first, job=True)[0] == 202
                wait(lambda: next(row for row in api("/api/graph")[1]["executions"] if row["id"] == first["selected"]["execution"])["build_job"].get("job_status") == "completed", "completed job report did not apply")
            if arguments.case == "selection-progress":
                release("core", "v2.0.0")
                holds["tags"] = "core"
                assert push("core", "v2.0.0")[0] == 202 and tags_started.wait(5)
                before = api("/api/graph")[1]["generation"]
                assert report(first, job=True, status="queued", wrong={"started_at": None})[0] == 202
                assert report(first, job=True, status="in_progress")[0] == 202
                wait(lambda: api("/api/graph")[1]["generation"] > before, "concurrent evidence was not applied")
                tags_release.set()
                wait(lambda: len([work for work in dispatches if work["selected"]["source"]["tag"] == "v2.0.0"]) == 5,
                     "ordinary job progress dropped a signed tag selection")
                finish_boundary("signed tag survives queued/running evidence commits during reads")
            elif arguments.case == "crossing-selections":
                holds["tags"] = "app"
                holds["commit"] = release("core", "v2.0.0", visible=False)
                assert push("core", "v2.0.0")[0] == 202 and tags_started.wait(5) and commit_started.wait(5)
                release("app", "v2.0.0")
                assert push("app", "v2.0.0")[0] == 202
                time.sleep(0.2)
                tags_release.set()
                wait(lambda: len([row for row in api("/api/graph")[1]["executions"] if row["repository"] == "Owner/app" and row["tag"] == "v2.0.0"]) == 5,
                     "newer concurrent source selection did not commit")
                prior = api("/api/graph")[1]
                commit_release.set()
                wait(lambda: "newer source selection committed during source selection: app" in (root / "stdout.log").read_text(),
                     "stale source conflict was not reported")
                assert api("/api/graph")[1] == prior, "stale selection overwrote newer source work"
                count = sum("/repos/Owner/core/tags?" in item[2] for item in trace)
                time.sleep(0.2)
                assert sum("/repos/Owner/core/tags?" in item[2] for item in trace) == count, "source conflict started automatic reselection"
                finish_boundary("true source conflict reported without retry or newer-node changes")
            else:
                if arguments.case in ("missing", "expired"):
                    missing_artifacts.add(first["id"])
                    assert report(first)[0] == 202 and report(first, job=True)[0] == 202
                    wait(lambda: next(row for row in api("/api/graph")[1]["executions"] if row["id"] == first["selected"]["execution"])["state"] == "failed",
                         "missing/expired assigned artifact did not fail explicitly")
                    failed = next(row for row in api("/api/graph")[1]["executions"] if row["id"] == first["selected"]["execution"])
                    assert failed["failure"] == "required artifacts missing/expired" and len(dispatches) == 5
                    assert api("/api/work/retry", {"execution": failed["id"]})[0] == 200
                    wait(lambda: len(dispatches) == 6, "explicit retry did not dispatch a new rebuild")

                def current_builds(names):
                    latest = {work["selected"]["execution"]: work for work in dispatches
                              if work["stage"] == "build" and work["selected"]["source"]["name"] in names}
                    return list(latest.values())

                completed = set()
                for project_names in (("core",), ("app", "side"), ("join",)):
                    expected = 5 * len(project_names)
                    wait(lambda: len(current_builds(project_names)) == expected,
                         "ready dependent builds were not dispatched")
                    cohort = current_builds(project_names)
                    for index, work in enumerate(cohort):
                        if index % 2:
                            assert report(work, job=True)[0] == 202
                            assert report(work)[0] == 202
                        else:
                            assert report(work)[0] == 202
                            assert report(work, job=True)[0] == 202
                    if arguments.case == "overlap" and project_names == ("core",):
                        assert artifact_started.wait(5), "artifact overlap fixture was not reached"
                        for _ in range(8):
                            assert report(first)[0] == 202 and report(first, job=True)[0] == 202
                        job_url = "https://github.com/Owner/core/actions/runs/1000/job/21000"
                        assert report(first, job=True, wrong={"html_url": job_url})[0] == 202
                        wait(lambda: next(row for row in api("/api/graph")[1]["executions"] if row["id"] == first["selected"]["execution"])["build_job"].get("html_url") == job_url,
                             "progress was blocked behind the artifact read")
                        assert sum("/runs/1000/artifacts?" in item[2] for item in trace) == 1
                        artifact_release.set()
                    wait(lambda: len([work for work in dispatches if work["stage"] == "publication" and work["selected"]["source"]["name"] in project_names]) == expected,
                         "independent publishers were not all admitted")
                    publishers = [work for work in dispatches if work["stage"] == "publication" and work["selected"]["source"]["name"] in project_names]
                    for work in publishers:
                        assert report(work, status="in_progress")[0] == 202
                        if arguments.case in ("skip", "skip-job") and project_names == ("core",) and work == publishers[0]:
                            offset = sum("Owner/feed" in item[2] for item in trace)
                            if arguments.case == "skip-job":
                                literal = "Publish ${{ fromJSON(inputs.assignment).label }}"
                                assert report(work, job=True, wrong={"name": literal})[0] == 202
                                time.sleep(0.1)
                                assert next(row for row in api("/api/graph")[1]["executions"] if row["id"] == work["selected"]["execution"])["state"] == "publishing", "unevaluated name authorized a successful publisher"
                                assert report(work, job=True, conclusion="skipped", wrong={"name": literal, "steps": []})[0] == 202
                            else:
                                assert report(work, job=True, conclusion="failure", wrong={"steps": [{"name": "Publish checked feed update", "conclusion": "skipped"}]})[0] == 202
                            execution = work["selected"]["execution"]
                            wait(lambda: next(row for row in api("/api/graph")[1]["executions"] if row["id"] == execution)["state"] == "failed",
                                 "trusted skipped push was not a definite failure")
                            assert sum("Owner/feed" in item[2] for item in trace) == offset, "skipped push searched the feed"
                            assert api("/api/work/retry", {"execution": execution})[0] == 200
                            wait(lambda: len([row for row in dispatches if row["stage"] == "build" and row["selected"]["execution"] == execution]) == 2,
                                 "retry did not produce a new build")
                            rebuilt = next(row for row in current_builds(("core",)) if row["selected"]["execution"] == execution)
                            assert rebuilt["selected"]["revision"] > work["selected"]["revision"]
                            assert rebuilt["selected"]["environment_from"][0]["run"]["id"] == json.loads(work["inputs"]["build_result"])["run"]["id"]
                            assert report(rebuilt)[0] == 202 and report(rebuilt, job=True)[0] == 202
                            wait(lambda: len([row for row in dispatches if row["stage"] == "publication" and row["selected"]["execution"] == execution]) == 2,
                                 "rebuilt target was not admitted for publication")
                            work = [row for row in dispatches if row["stage"] == "publication" and row["selected"]["execution"] == execution][-1]
                            assert report(work, status="in_progress")[0] == 202
                        recovery = arguments.case == "recovery" and project_names == ("core",) and work == publishers[0]
                        if recovery:
                            offset = sum("/repos/Owner/feed/commits?" in item[2] for item in trace)
                            assert report(work, job=True)[0] == 202
                            assert report(work, conclusion="failure")[0] == 202
                            wait(lambda: sum("/repos/Owner/feed/commits?" in item[2] for item in trace) > offset, "possible push was not investigated")
                            time.sleep(0.15)
                            assert next(row for row in api("/api/graph")[1]["executions"] if row["id"] == work["selected"]["execution"])["state"] == "publishing"
                            assert api("/api/work/retry", {"execution": work["selected"]["execution"]})[0] == 409, "uncertain mutation was replaced"
                            offset = sum("Owner/feed" in item[2] for item in trace)
                            for _ in range(8):
                                assert report(work, job=True)[0] == 202
                            time.sleep(0.15)
                            assert sum("Owner/feed" in item[2] for item in trace) == offset, "unchanged completion repeated failed receipt proof"
                            before_restart = len(dispatches)
                            lifecycle.stop(application)
                            application = subprocess.Popen(command, stdout=(root / "pending-restart.log").open("w"), stderr=subprocess.STDOUT)
                            lifecycle.wait_http(port, application)
                            time.sleep(0.4)
                            assert len(dispatches) == before_restart, "restart replayed an uncertain publication"
                        identity = {"id": work["id"], "run_attempt": 1, "repository_id": 9, "actor_id": 42,
                                    "workflow_commit": definitions["worker-main"], "workflow_blob_sha": blob_id(publisher),
                                    "html_url": "https://github.com/Owner/worker/actions/runs/" + str(work["id"])}
                        selected = work["selected"]
                        receipt = {"schema": 1, "execution": selected["execution"], "attempt": selected["attempt"],
                                   "assignment_sha256": work["inputs"]["assignment_sha256"], "source_commit": selected["source"]["commit"],
                                   "run": identity, "build_result": json.loads(work["inputs"]["build_result"]), "feed": selected["source"]["deployment"]["feed"],
                                   "packages": [{"name": selected["source"]["package"], "sha256": "a" * 64}], "environment": {"fixture": "synthetic"}}
                        path = "receipts/" + selected["execution"] + "/" + str(selected["revision"]) + ".json"
                        public_files[path] = canonical(receipt).encode()
                        receipt_bytes = public_files[path]
                        commit = sha(canonical(sorted(public_files)).encode())
                        tree_files("feed", commit, public_files)
                        definitions[path] = commit
                        if recovery:
                            public_files[path] = b"tampered HTTPS receipt"
                            offset = sum(item[2] == "/" + path for item in trace)
                            assert api("/api/work/reconcile", {})[0] == 200
                            wait(lambda: sum(item[2] == "/" + path for item in trace) > offset, "HTTPS proof was not checked")
                            time.sleep(0.15)
                            assert next(row for row in api("/api/graph")[1]["executions"] if row["id"] == selected["execution"])["state"] == "publishing"
                            public_files[path] = receipt_bytes
                            assert api("/api/work/reconcile", {})[0] == 200
                        else:
                            assert report(work, job=True)[0] == 202
                            assert report(work)[0] == 202
                        completed.add(selected["execution"])
                    wait(lambda: sum(row["state"] == "published" for row in api("/api/graph")[1]["executions"]) == len(completed), "publication proof was not accepted")
                expected_dispatches = 42 if arguments.case in ("skip", "skip-job") else 41 if arguments.case in ("missing", "expired") else 40
                expected_artifacts = 21 if arguments.case in ("skip", "skip-job", "missing", "expired") else 20
                assert len(dispatches) == expected_dispatches and len(completed) == 20
                assert sum("/artifacts?" in item[2] for item in trace) == expected_artifacts, "artifact reads multiplied"
                graph = api("/api/graph")[1]
                assert all(row["receipt"]["publication_verified_utc"] and "assignment" not in row for row in graph["executions"])
                assert all(row["build_job"]["status"] == "completed" for row in graph["executions"])
                assert len(api("/public/status")[1]["executions"]) == 20
                lifecycle.stop(application)
                application = subprocess.Popen(command, stdout=(root / "restart.log").open("w"), stderr=subprocess.STDOUT)
                lifecycle.wait_http(port, application)
                assert api("/api/graph")[1] == graph, "restart changed durable graph/evidence"
                time.sleep(0.2)
                assert len(dispatches) == expected_dispatches, "restart created replacement work"
                assert not errors, errors
                metrics = {"kind": "local synthetic GitHub and real isolated MariaDB", "outgoing_total": len(trace),
                           "token_requests": sum("/access_tokens" in item[2] for item in trace),
                           "artifact_reads": sum("/artifacts?" in item[2] for item in trace), "unchanged_burst_requests": 0}
                (root / "request-trace.json").write_text(json.dumps(trace, indent=2))
                from collections import Counter
                metrics["request_paths"] = dict(Counter(method + " " + path.split("?")[0] for _, method, path in trace))
                (root / "measurements.json").write_text(json.dumps(metrics, indent=2))
                print("PASS synthetic full twenty-node chain", arguments.case, canonical({key: value for key, value in metrics.items() if key != "request_paths"}))
    finally:
        tags_release.set()
        commit_release.set()
        token_release.set()
        publication_release.set()
        artifact_release.set()
        lifecycle.stop(application)
        if server:
            server.shutdown()
            server.server_close()
        if thread:
            thread.join(timeout=3)
        lifecycle.stop(database)
        db_log.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True)
    parser.add_argument("--directory", required=True)
    parser.add_argument("--case", choices=("chain", "recovery", "cold", "skip", "missing", "expired", "sources", "overlap", "workflow", "changed", "skip-job", "selection-progress", "shared-tags", "backport", "explicit-tag", "non-semver", "enrollment-race", "crossing-selections", "preflight-build-absent", "preflight-build-known", "preflight-publish-absent", "preflight-publish-known", "preflight-live", "queued-job"), default="chain")
    exercise(parser.parse_args())
