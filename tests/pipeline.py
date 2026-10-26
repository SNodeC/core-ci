"""Real MariaDB and synthetic GitHub/feed HTTPS pipeline boundaries, not live jobs."""

import argparse
import concurrent.futures
import base64
import datetime
import hashlib
import hmac
import http.server
import json
import os
from pathlib import Path
import pwd
import ssl
import subprocess
import tempfile
import time
import threading
import urllib.error
import urllib.parse
import urllib.request

import lifecycle


def exercise(args, directory):
    root = Path(directory)
    data = root / "data"
    temporary = root / "tmp"
    data.mkdir(mode=0o700)
    temporary.mkdir(mode=0o700)
    lifecycle.run(["mariadb-install-db", "--no-defaults", "--datadir=" + str(data), "--tmpdir=" + str(temporary),
                   "--auth-root-authentication-method=normal", "--skip-test-db"])
    data.chmod(0o700)
    db_socket = data / "server.sock"
    app_key = root / "app.pem"
    tls_key = root / "tls-key.pem"
    certificate = root / "tls.pem"
    lifecycle.run(["openssl", "genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:2048", "-out", str(app_key)])
    app_key.chmod(0o600)
    lifecycle.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1", "-keyout", str(tls_key),
                   "-out", str(certificate), "-subj", "/CN=api.github.com", "-addext", "subjectAltName=DNS:api.github.com,IP:127.0.0.1"])
    tls_key.chmod(0o600)
    secret = os.urandom(48)
    secret_file = root / "webhook.secret"
    secret_file.write_bytes(secret)
    secret_file.chmod(0o600)
    password = root / "password"
    password.write_text("")
    password.chmod(0o600)
    source_sha = "d" * 40
    older_sha = "c" * 40
    head = "a" * 40
    definitions = {}
    workflow_files = {}
    build_status_reads = []
    trees = {}
    blobs = {}
    writes = 0
    dispatches = []
    publications = []
    status_refreshes = []
    feed_files = {}
    deployed_files = {}
    feed_commits = {}
    runs = {}
    errors = []
    lose_next = False
    reject_workflow = False
    delayed_run = False
    unavailable_artifacts = False
    missing_artifacts = False
    malformed_publication_jobs = False
    job_attempt = 1
    workflow = (Path(__file__).resolve().parents[1] / "deployment/source/core-ci-build.yml.in").read_text().replace("@WORKER@", "Owner/worker").replace("@TOOLING_SHA@", "f" * 40).encode()
    workflow_blob = hashlib.sha1(b"blob " + str(len(workflow)).encode() + b"\0" + workflow).hexdigest()
    blobs[workflow_blob] = workflow

    definition = {"schema": 1, "title": "Example ü", "package": "example", "depends_on": [], "build": {"native": "ci/native.sh"},
                  "matrix": [{"id": "native", "family": "native", "distribution": "source-defined", "suite": "series", "arch": "amd64",
                              "runner": ["self-hosted", "selected"], "build": {"image": "sample:series", "platform": "linux/amd64", "format": "deb"}}]}

    def register(commit, document):
        raw = json.dumps(document).encode()
        blob = hashlib.sha1(b"blob " + str(len(raw)).encode() + b"\0" + raw).hexdigest()
        definitions[commit] = (document, blob)
        blobs[blob] = raw
        ids = [hashlib.sha1((kind + commit).encode()).hexdigest() for kind in ("root", "ci", "github", "workflows")]
        trees[ids[0]] = [{"path": "ci", "type": "tree", "mode": "040000", "sha": ids[1]},
                         {"path": ".github", "type": "tree", "mode": "040000", "sha": ids[2]}]
        trees[ids[1]] = [{"path": "packages.json", "type": "blob", "mode": "100644", "sha": blob}]
        trees[ids[2]] = [{"path": "workflows", "type": "tree", "mode": "040000", "sha": ids[3]}]
        trees[ids[3]] = [{"path": path, "type": "blob", "mode": "100644", "sha": workflow_blob} for path in ("core-ci-build.yml", "core-ci-publish.yml", "core-ci-status.yml")]
        return blob

    register(head, definition)
    register(source_sha, definition)
    older_definition = json.loads(json.dumps(definition))
    older_definition["build"]["native"] = "ci/native-older.sh"
    register(older_sha, older_definition)

    class GitHub(http.server.BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def reply(self, status, body):
            content = json.dumps(body).encode()
            self.send_response(status)
            self.send_header("Content-Length", str(len(content)))
            self.send_header("Content-Type", "application/json")
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(content)
            self.close_connection = True

        def do_POST(self):
            nonlocal lose_next
            value = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
            if self.path == "/app/installations/7/access_tokens":
                if value["repository_ids"] not in ([99], [1000]) or value["permissions"] not in ({"contents": "read"}, {"contents": "write"}, {"actions": "read"}, {"actions": "write"}, {"contents": "write", "workflows": "write"}):
                    errors.append("unexpected installation token scope")
                self.reply(201, {"token": "synthetic-pipeline-token"})
            elif self.path == "/repos/Owner/example/actions/workflows/core-ci-build.yml/dispatches":
                try:
                    assignment = json.loads(value["inputs"]["assignment"])
                    execution = value["inputs"]["execution"]
                    # Read real durable state at the instant the fake GitHub receives dispatch.
                    durable = lifecycle.query(db_socket, "SELECT state FROM core_ci.attempts WHERE id='" + assignment["attempt"] + "'").strip()
                    assert durable == "building", "dispatch occurred before durable build authorization"
                    assert assignment["execution"] == execution and value["inputs"]["assignment_sha256"] == hashlib.sha256(json.dumps(assignment, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
                    assignment_file = root / "assigned-work.json"
                    assignment_file.write_text(value["inputs"]["assignment"])
                    worker_path = str(Path(__file__).resolve().parents[1] / "deployment/worker/core_ci")
                    lifecycle.run(["python3", "-c", "import sys; sys.path.insert(0,sys.argv[1]); from contract import assignment; assignment(sys.argv[2],sys.argv[3])",
                                   worker_path, str(assignment_file), value["inputs"]["assignment_sha256"]])
                    assert assignment["label"] == "Owner/example " + assignment["source"]["tag"] + " | source-defined series | amd64 | deb | r" + str(assignment["revision"])
                    assert assignment["target"]["runner"] == ["self-hosted", "selected"]
                    assert "environment" not in assignment and "environment" not in assignment["source"]
                    run_id = 100 + len(dispatches)
                    run = {"id": run_id, "run_attempt": 1, "event": "workflow_dispatch", "repository": {"id": 99}, "head_repository": {"id": 99},
                           "actor": {"id": 42}, "display_title": "Build " + assignment["label"],
                           "path": ".github/workflows/core-ci-build.yml", "head_sha": head,
                           "html_url": "https://github.com/Owner/example/actions/runs/" + str(run_id) + ("x" * 65536 if run_id == 103 else ""), "status": "queued" if run_id == 100 else "in_progress", "conclusion": None}
                    runs[run_id] = run
                    dispatches.append(assignment)
                    if lose_next:
                        lose_next = False
                        self.close_connection = True
                    else:
                        self.reply(200, {"workflow_run_id": run_id})
                except Exception as error:
                    errors.append(str(error))
                    self.reply(422, {})
            elif self.path == "/repos/Owner/worker/actions/workflows/core-ci-status.yml/dispatches":
                generation = int(value["inputs"]["generation"])
                durable = int(lifecycle.query(db_socket, "SELECT generation FROM core_ci.graph_revision").strip())
                assert 0 < generation <= durable, "status notification preceded the committed graph"
                assert set(value["inputs"]) == {"generation"}, "status dispatch leaked graph/configuration data"
                status_refreshes.append(generation)
                self.reply(204, {})
            elif self.path == "/repos/Owner/worker/actions/workflows/core-ci-publish.yml/dispatches":
                try:
                    selected = json.loads(value["inputs"]["assignment"])
                    durable = lifecycle.query(db_socket, "SELECT state FROM core_ci.attempts WHERE id='" + selected["attempt"] + "'").strip()
                    assert durable == "publishing", "publication dispatched before committed authority"
                    assert lifecycle.query(db_socket, "SELECT COUNT(*) FROM core_ci.attempts WHERE state='publishing'").strip() == "1"
                    run_id = 500 + len(publications)
                    runs[run_id] = {"id": run_id, "run_attempt": 1, "event": "workflow_dispatch", "repository": {"id": 1000}, "head_repository": {"id": 1000},
                                    "actor": {"id": 42}, "display_title": "Publish " + selected["label"],
                                    "path": ".github/workflows/core-ci-publish.yml", "head_sha": head, "html_url": "https://github.com/Owner/worker/actions/runs/" + str(run_id),
                                    "status": "in_progress", "conclusion": None}
                    publications.append({"assignment": selected, "build_result": json.loads(value["inputs"]["build_result"])})
                    self.reply(200, {"workflow_run_id": run_id})
                except Exception as error:
                    errors.append(str(error))
                    self.reply(422, {})
            else:
                errors.append("unexpected mutation " + self.path)
                self.reply(404, {})

        def do_PUT(self):
            nonlocal head, writes
            value = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
            try:
                if self.path == "/repos/Owner/example/contents/.github/workflows/core-ci-build.yml":
                    if reject_workflow:
                        self.reply(403, {"message": "fixture workflow permission denied"})
                    else:
                        raw = base64.b64decode(value["content"])
                        assert raw == workflow and value.get("sha", "") == workflow_files.get(head, "")
                        workflow_files[head] = workflow_blob
                        self.reply(201, {"commit": {"sha": head}, "content": {"sha": workflow_blob}})
                else:
                    self.write_definition(value)
            except Exception as error:
                errors.append(str(error))
                self.reply(422, {})

        def write_definition(self, value):
            nonlocal head, writes
            try:
                assert self.path == "/repos/Owner/example/contents/ci/packages.json"
                raw = base64.b64decode(value["content"])
                assert value["sha"] == definitions[head][1]
                writes += 1
                head = hashlib.sha1(("head-" + str(writes)).encode()).hexdigest()
                document = json.loads(raw)
                register(head, document)
                # Preserve the exact committed bytes, including the UI writer's formatting.
                blob = hashlib.sha1(b"blob " + str(len(raw)).encode() + b"\0" + raw).hexdigest()
                definitions[head] = (document, blob)
                blobs[blob] = raw
                trees[hashlib.sha1(("ci" + head).encode()).hexdigest()][0]["sha"] = blob
                self.reply(201, {"commit": {"sha": head}, "content": {"sha": blob}})
            except Exception as error:
                errors.append(str(error))
                self.reply(422, {})

        def do_GET(self):
            parsed = urllib.parse.urlsplit(self.path)
            path = parsed.path
            if path.startswith("/receipts/"):
                content = deployed_files.get(path)
                self.send_response(200 if content else 404)
                self.send_header("Content-Length", str(len(content or b"")))
                self.send_header("Connection", "close")
                self.end_headers()
                self.wfile.write(content or b"")
                self.close_connection = True
            else:
                public = path.startswith("/repos/Owner/feed")
                if self.headers.get("Authorization") != "Bearer synthetic-pipeline-token":
                    errors.append("missing scoped GitHub authorization")
                prefix = "/repos/Owner/" + ("feed" if public else "worker" if path.startswith("/repos/Owner/worker") else "example")
                suffix = path.removeprefix(prefix)
                if path == prefix:
                    self.reply(200, {"id": 1001 if public else 1000 if prefix.endswith("worker") else 99, "full_name": prefix.removeprefix("/repos/")})
                elif suffix == "/git/ref/heads/main":
                    self.reply(200, {"object": {"sha": head}})
                elif suffix == "/commits/main":
                    self.reply(200, {"sha": head})
                elif suffix == "/tags":
                    self.reply(200, [{"name": "v0.9.0+build"}, {"name": "v2.0.0-rc.1"}])
                elif suffix in ("/git/ref/tags/v0.9.0%2Bbuild", "/git/ref/tags/v2.0.0-rc.1"):
                    older = "%2B" in suffix
                    self.reply(200, {"ref": "refs/tags/" + ("v0.9.0+build" if older else "v2.0.0-rc.1"),
                                     "object": {"type": "commit", "sha": older_sha if older else source_sha}})
                elif public and suffix == "/commits":
                    selected_path = urllib.parse.parse_qs(parsed.query)["path"][0]
                    self.reply(200, [{"sha": "e" * 40}] if selected_path in feed_files else [])
                elif public and suffix.startswith("/git/commits/") and suffix[13:] in feed_commits:
                    self.reply(200, {"sha": suffix[13:], "tree": {"sha": feed_commits[suffix[13:]]}})
                elif suffix.startswith("/git/commits/") and suffix[13:] in definitions:
                    commit = suffix[13:]
                    self.reply(200, {"sha": commit, "tree": {"sha": hashlib.sha1(("root" + commit).encode()).hexdigest()}})
                elif suffix.startswith("/git/trees/") and suffix[11:] in trees:
                    sha = suffix[11:]
                    self.reply(200, {"sha": sha, "truncated": False, "tree": trees[sha]})
                elif suffix.startswith("/git/blobs/") and suffix[11:] in blobs:
                    sha = suffix[11:]
                    self.reply(200, {"sha": sha, "encoding": "base64", "content": base64.b64encode(blobs[sha]).decode()})
                elif suffix == "/actions/workflows/core-ci-build.yml/runs":
                    self.reply(200, {"workflow_runs": list(runs.values()) + [{**run, "id": run["id"] + 9000, "actor": {"id": 999}} for run in runs.values()]})
                elif suffix.startswith("/actions/runs/"):
                    parts = suffix.split("/")
                    run_id = int(parts[3])
                    run = runs[run_id]
                    assignment = publications[run_id - 500]["assignment"] if run_id >= 500 else dispatches[run_id - 100]
                    if len(parts) == 4:
                        self.reply(200, {**run, "run_attempt": 2, "status": "completed", "conclusion": "failure"})
                    elif parts[4:6] == ["attempts", "1"] and len(parts) == 6:
                        if delayed_run:
                            time.sleep(1)
                        if run_id < 500:
                            build_status_reads.append(run_id)
                        self.reply(200, run)
                    elif parts[4:] == ["attempts", "1", "jobs"]:
                        if run_id >= 500 and malformed_publication_jobs:
                            self.reply(200, {"total_count": 1, "jobs": "not a job array"})
                        else:
                            self.reply(200, {"total_count": 1, "jobs": [{"id": run_id + 1000, "run_id": run_id, "run_attempt": job_attempt,
                                "name": ("Publish " if run_id >= 500 else "build / Build ") + assignment["label"],
                                "status": run["status"], "conclusion": run["conclusion"],
                                "started_at": "2026-10-26T10:00:00Z", "completed_at": "2026-10-26T10:03:25Z",
                                "steps": [{"name": "Publish checked feed update", "conclusion": "skipped" if run_id == 501 else "failure"}]}]})
                    elif parts[4] == "artifacts" and unavailable_artifacts:
                        self.reply(unavailable_artifacts, {"total_count": 1, "artifacts": []})
                    elif parts[4] == "artifacts" and missing_artifacts:
                        self.reply(200, {"total_count": 0, "artifacts": []})
                    elif parts[4] == "artifacts":
                        self.reply(200, {"total_count": 2, "artifacts": [{"id": run_id + 3000, "name": assignment["artifact_name"] + "-environment"}, {"id": run_id + 2000, "name": assignment["artifact_name"],
                                         "digest": "sha256:" + "e" * 64, "size_in_bytes": 1024, "expired": False, "workflow_run": {"id": run_id}}]})
                    else:
                        errors.append("unexpected run query " + suffix)
                        self.reply(404, {})
                else:
                    errors.append("unexpected GitHub query " + suffix)
                    self.reply(404, {})

    github = http.server.HTTPServer(("127.0.0.1", 0), GitHub)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certificate, tls_key)
    github.socket = context.wrap_socket(github.socket, server_side=True)
    thread = threading.Thread(target=github.serve_forever, daemon=True)
    thread.start()
    port = lifecycle.free_port()
    origin = "http://127.0.0.1:" + str(port)
    webhook_port = lifecycle.free_port()
    webhook_origin = "https://127.0.0.1:" + str(webhook_port)
    webhook_context = ssl.create_default_context(cafile=str(certificate))
    feed_origin = "https://127.0.0.1:" + str(github.server_port)
    settings = root / "controller.conf"
    settings.write_text('github.app-id=42\ngithub.installation-id=7\ngithub.bot-id=42\ngithub.allowed-owner="Owner"\n'
        'github.repository=["Owner/example=99", "Owner/worker=1000"]\ngithub.private-key-file="' + str(app_key) + '"\n'
        'github.webhook-secret-file="' + str(secret_file) + '"\nui.origin="' + origin + '"\n'
        'pipeline.worker-repository="Owner/worker"\npipeline.tooling-sha="' + "f" * 40 + '"\n'
        'pipeline.source-workflow-blob="' + workflow_blob + '"\npipeline.publication-ref="main"\npipeline.publication-workflow-blob="' + workflow_blob + '"\n'
        'pipeline.feed-repository="Owner/feed"\npipeline.feed-id=1001\npipeline.feed-branch="feed"\npipeline.feed-base-url="' + feed_origin + '/"\n'
        'pipeline.gpg-fingerprint="' + "A" * 40 + '"\npipeline.usign-key-sha256="' + "a" * 64 + '"\npipeline.apk-key-sha256="' + "b" * 64 + '"\n')
    settings.write_text(settings.read_text() + 'pipeline.status-workflow=".github/workflows/core-ci-status.yml"\n'
                        + 'pipeline.status-workflow-blob="' + workflow_blob + '"\n')
    settings.chmod(0o600)
    command = [args.executable, "--config-file", str(settings), "--log-file", str(root / "framework.log"), "database", "--mode", "unmanaged",
               "--socket", str(db_socket), "--user", "core_ci_test", "--name", "core_ci", "--password-file", str(password),
               "web", "local", "--port", str(port), "github-http", "remote", "--host", "127.0.0.1", "--port", str(github.server_port),
               "tls", "--ca-cert", str(certificate), "feed-http", "tls", "--ca-cert", str(certificate),
               "webhook", "--disabled=false", "local", "--port", str(webhook_port),
               "tls", "--cert", str(certificate), "--cert-key", str(tls_key)]

    def request(path, body=None, headers=None, webhook=False):
        payload = None if body is None else json.dumps(body).encode()
        req = urllib.request.Request((webhook_origin if webhook else origin) + path, data=payload, headers=headers or {"Content-Type": "application/json", "Origin": origin})
        try:
            response = urllib.request.urlopen(req, timeout=15, context=webhook_context if webhook else None)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            result = (response.status, json.load(response))
        return result

    def report(run_id, signature=None, repository_id=99):
        body = {"installation": {"id": 7}, "repository": {"id": 1000 if run_id >= 500 else repository_id, "full_name": "Owner/worker" if run_id >= 500 else "Owner/example"}, "workflow_run": runs[run_id]}
        raw = json.dumps(body).encode()
        signature = signature or "sha256=" + hmac.new(secret, raw, hashlib.sha256).hexdigest()
        log_path = root / "app.log"
        offset = log_path.stat().st_size
        started = time.monotonic()
        result = request("/github/webhook", body, {"Content-Type": "application/json", "X-GitHub-Event": "workflow_run", "X-Hub-Signature-256": signature}, webhook=True)
        if delayed_run:
            assert time.monotonic() - started < 0.8, "webhook waited for delayed GitHub evidence"
        if result[0] == 202 and "accepted" in result[1].get("event", ""):
            completed = None
            deadline = time.monotonic() + 10
            while completed is None and time.monotonic() < deadline:
                for line in log_path.read_bytes()[offset:].decode().splitlines():
                    if "GitHub webhook completed: " in line:
                        completed = json.loads(line.split("GitHub webhook completed: ", 1)[1])
                if completed is None:
                    time.sleep(0.01)
            assert completed is not None, "accepted webhook did not finish processing"
            result = (completed["status"], completed["result"])
        return result

    database = None
    application = None
    try:
        with (root / "db.log").open("w") as db_log, (root / "app.log").open("w") as app_log:
            database = subprocess.Popen(["/usr/sbin/mariadbd", "--no-defaults", "--datadir=" + str(data), "--socket=" + str(db_socket),
                "--pid-file=" + str(data / "pid"), "--skip-networking", "--innodb-buffer-pool-size=32M", "--tmpdir=" + str(temporary),
                "--user=" + pwd.getpwuid(os.geteuid()).pw_name], stdout=db_log, stderr=subprocess.STDOUT)
            lifecycle.wait_sql(db_socket, database)
            schema = (Path(__file__).resolve().parents[1] / "sql/schema.sql").read_text()
            lifecycle.query(db_socket, "CREATE DATABASE core_ci; USE core_ci; " + schema +
                " CREATE USER 'core_ci_test'@'localhost'; GRANT SELECT,INSERT,UPDATE,DELETE ON core_ci.* TO 'core_ci_test'@'localhost';")
            application = subprocess.Popen(command, stdout=app_log, stderr=subprocess.STDOUT)
            lifecycle.wait_http(port, application)
            # Public TLS endpoint has no administration/UI routes; HTTP endpoint has no webhook.
            for endpoint, path, payload in ((webhook_origin, "/", None), (webhook_origin, "/api/graph", None), (webhook_origin, "/api/events", None),
                                            (webhook_origin, "/api/repositories/save", b"{}"), (origin, "/github/webhook", b"{}")):
                probe = urllib.request.Request(endpoint + path, data=payload,
                    headers={"Origin": origin, "Content-Type": "application/json"})
                try:
                    response = urllib.request.urlopen(probe, timeout=5, context=webhook_context)
                except urllib.error.HTTPError as error:
                    response = error
                with response:
                    assert response.status == 404, "listener exposed a route owned by the other endpoint"
            rejected_tls = False
            try:
                urllib.request.urlopen(webhook_origin + "/", timeout=5)
            except urllib.error.URLError as error:
                rejected_tls = isinstance(error.reason, ssl.SSLCertVerificationError)
            assert rejected_tls, "TLS client accepted an untrusted test certificate"
            status, saved = request("/api/repositories/save", {"repository": "Owner/example", "branch": "main", "revision": "",
                "blob_sha": definitions[head][1], "definition": definition})
            assert status == 200 and saved["enrolled"] and saved["work"]["status"] == 202, saved
            assert writes == 1 and len(dispatches) == 1 and dispatches[0]["source"]["commit"] == source_sha
            assert runs[100]["head_sha"] != source_sha, "workflow and selected source were conflated"
            first_id = dispatches[0]["execution"]
            first = next(row for row in request("/api/graph")[1]["executions"] if row["id"] == first_id)
            assert first["activity"] == "build queued on GitHub"
            assert "started_at" not in first["build_job"], "queued metadata was mistaken for an actual job start"
            evidence_stream = lifecycle.event_stream(origin)
            lifecycle.stream_event(evidence_stream)
            lifecycle.stream_event(evidence_stream)
            job_attempt = 2
            assert report(100)[0] == 409, "attempt-2 job evidence was accepted"
            job_attempt = 1
            runs[100]["status"] = "in_progress"
            assert report(100)[0] == 200
            event, early = lifecycle.stream_event(evidence_stream)
            assert event == "graph"
            early = next(row for row in early["executions"] if row["id"] == first_id)
            assert early["activity"] == "build running" and early["build_job"]["started_at"] == "2026-10-26T10:00:00Z"
            assert "completed_at" not in early["build_job"] and early["state"] == "building"
            public_early = next(row for row in request("/public/status", webhook=True)[1]["executions"] if row["id"] == first_id)
            assert public_early["activity"] == early["activity"] and public_early["build_job"] == early["build_job"]
            assert public_early["build_job_url"] == early["build_run"]["html_url"] + "/job/" + str(early["build_job"]["job_id"])
            evidence_stream.close()
            immutable_run = early["build_run"]
            generation = request("/api/graph")[1]["generation"]
            assert report(100)[0] == 200
            assert request("/api/graph")[1]["generation"] == generation, "duplicate progress report mutated the graph"
            status, preview = request("/api/repositories/workflow", {"repository": "Owner/example"})
            assert status == 200 and preview["proposed"].encode() == workflow and preview["blob_sha"] == workflow_blob, preview
            status, unchanged = request("/api/repositories/workflow", {"repository": "Owner/example", "head": head, "blob_sha": workflow_blob, "write": True})
            assert status == 200 and unchanged["unchanged"] and unchanged["github_revision"] == head, unchanged
            assert request("/api/repositories/workflow", {"repository": "Owner/example", "head": "0" * 40, "blob_sha": workflow_blob, "write": True})[0] == 409
            workflow_tree = trees[hashlib.sha1(("workflows" + head).encode()).hexdigest()]
            workflow_tree[0]["sha"] = hashlib.sha1(b"blob 7\0changed").hexdigest()
            blobs[workflow_tree[0]["sha"]] = b"changed"
            workflow_files[head] = workflow_tree[0]["sha"]
            reject_workflow = True
            status, failed_write = request("/api/repositories/workflow", {"repository": "Owner/example", "head": head, "blob_sha": workflow_files[head], "write": True})
            assert status == 403 and failed_write["github_committed"] is False, failed_write
            reject_workflow = False
            # Update existing content, then create an absent workflow with the same approved bytes.
            status, written = request("/api/repositories/workflow", {"repository": "Owner/example", "head": head, "blob_sha": workflow_files[head], "write": True})
            assert status == 200 and written["github_committed"] and written["blob_sha"] == workflow_blob, written
            workflow_tree[0]["sha"] = workflow_blob
            entry = workflow_tree.pop(0)
            workflow_files[head] = ""
            status, absent = request("/api/repositories/workflow", {"repository": "Owner/example"})
            assert status == 200 and absent["blob_sha"] == "", absent
            status, created = request("/api/repositories/workflow", {"repository": "Owner/example", "head": head, "blob_sha": "", "write": True})
            assert status == 200 and created["github_committed"], created
            workflow_tree.insert(0, entry)

            assert report(100, "sha256=" + "0" * 64)[0] == 401
            assert report(100, repository_id=999)[0] == 422
            delayed_run = True
            assert report(100)[0] == 200 and len(dispatches) == 1
            delayed_run = False
            lifecycle.stop(application)
            application = subprocess.Popen(command, stdout=app_log, stderr=subprocess.STDOUT)
            lifecycle.wait_http(port, application)
            assert request("/api/work/reconcile", {})[0] == 200 and len(dispatches) == 1, "restart dispatched a replacement"
            restored_early = next(row for row in request("/api/graph")[1]["executions"] if row["id"] == first_id)
            assert restored_early["build_job"] == early["build_job"] and restored_early["build_run"] == immutable_run
            stream = lifecycle.event_stream(origin)
            assert lifecycle.stream_event(stream) == ("graph", request("/api/graph")[1])
            assert lifecycle.stream_event(stream)[0] == "repositories"
            lose_next = True
            status, selected = request("/api/work/request", {"repository": "Owner/example", "tag": "v0.9.0+build"})
            assert status == 202 and len(dispatches) == 2, selected
            older = dispatches[1]
            assert older["source"]["commit"] == older_sha and older["source"]["build"]["native"] == "ci/native-older.sh"
            event, pushed = lifecycle.stream_event(stream)
            remaining = 20
            while remaining > 0 and (event != "graph" or not any(row["tag"] == "v0.9.0+build" and row["state"] == "building"
                    for row in pushed["executions"])):
                event, pushed = lifecycle.stream_event(stream)
                remaining -= 1
            assert remaining > 0, "committed build selection did not reach SSE"
            public = request("/public/status", webhook=True)[1]
            assert public["projects"][0]["versions"] == ["v2.0.0-rc.1", "v0.9.0+build"]
            assert public["feed"]["repository_id"] == 1001
            assert public["executions"] and all("assignment" not in item and "source" not in item for item in public["executions"])
            assert all(set(item["coordinates"]) == {"distribution", "suite", "arch", "format"} for item in public["executions"])
            assert "private-key" not in json.dumps(public) and "deployment" not in json.dumps(public)
            graph = request("/api/graph")[1]["executions"]
            assert all(row["target_row"]["id"] == row["target"] for row in graph)
            assert next(row for row in graph if row["id"] == older["execution"])["build_run"] is None
            assert request("/api/work/reconcile", {})[0] == 200 and len(dispatches) == 2, "lost dispatch response caused replay"
            runs[101].update(status="completed", conclusion="failure")
            assert report(101)[0] == 200
            assert next(row for row in request("/api/graph")[1]["executions"] if row["id"] == older["execution"])["state"] == "failed"
            assert request("/api/work/retry", {"execution": older["execution"]})[0] == 202 and len(dispatches) == 3
            assert dispatches[2]["source"]["commit"] == older_sha and dispatches[2]["revision"] > dispatches[0]["revision"]
            reference = dispatches[2]["environment_from"]
            assert len(reference) == 1 and reference[0]["run"]["id"] == 101
            assert reference[0]["attempt"] == older["attempt"] and reference[0]["artifact_name"] == older["artifact_name"] + "-environment"
            assert reference[0]["assignment_sha256"] == hashlib.sha256(json.dumps(older, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
            assert report(101)[0] == 200, "late report was not idempotent"
            runs[102].update(status="completed", conclusion="success")
            assert report(102)[0] == 202 and len(publications) == 1
            current = next(row for row in request("/api/graph")[1]["executions"] if row["id"] == older["execution"])
            assert current["state"] == "publishing"
            assert current["activity"] == "publication running"
            assert current["publication_job"]["started_at"] == "2026-10-26T10:00:00Z"
            public_publishing = next(row for row in request("/public/status", webhook=True)[1]["executions"] if row["id"] == current["id"])
            assert public_publishing["publication_job"] == current["publication_job"]

            assert "completed_at" not in current["publication_job"]
            timing = {"started_at": "2026-10-26T10:00:00Z", "completed_at": "2026-10-26T10:03:25Z"}
            assert {key: current["build_job"][key] for key in timing} == timing, "UI timing is not the assigned attempt-1 package job evidence"
            assert publications[0]["build_result"]["build_job"] == timing
            assert current["revision"] == dispatches[2]["revision"] and len(dispatches) == 3
            runs[100].update(status="completed", conclusion="success")
            assert report(100)[0] == 200 and len(publications) == 1, "second writer started before the assigned publication resolved"
            event, pushed = lifecycle.stream_event(stream)
            remaining = 20
            while remaining > 0 and (event != "graph" or not any(row["id"] == older["execution"] and row["revision"] == current["revision"]
                    and row["state"] == "building" for row in pushed["executions"])):
                event, pushed = lifecycle.stream_event(stream)
                remaining -= 1
            assert remaining > 0, "explicit retry did not update SSE"
            stream.close()
            generation = request("/api/graph")[1]["generation"]
            assert report(102)[0] == 200 and len(dispatches) == 3
            assert request("/api/graph")[1]["generation"] == generation, "duplicate report caused a redundant mutation"
            assert status_refreshes, "committed changes did not notify the trusted status workflow"
            runs[500].update(status="completed", conclusion="failure")
            assert report(500)[0] == 202, "a failed publication job without a receipt must preserve uncertainty"
            assert next(row for row in request("/api/graph")[1]["executions"] if row["id"] == older["execution"])["state"] == "publishing"
            verified_reads = build_status_reads.count(100)
            lifecycle.stop(application)
            application = subprocess.Popen(command, stdout=app_log, stderr=subprocess.STDOUT)
            lifecycle.wait_http(port, application)
            request("/api/work/reconcile", {})
            assert build_status_reads.count(100) == verified_reads, "restart re-read an already persisted verified build"
            assert len(publications) == 1, "restart replaced uncertain publication"
            restored = next(row for row in request("/api/graph")[1]["executions"] if row["id"] == older["execution"])
            assert {key: restored["build_job"][key] for key in timing} == timing, "recorded build timing was lost on restart"
            selected = publications[0]["assignment"]
            public_run = next(row for row in request("/api/graph")[1]["executions"] if row["id"] == older["execution"])["publication_run"]
            receipt = {"schema": 1, "execution": selected["execution"], "attempt": selected["attempt"],
                       "environment": {"image": "worker-resolved", "digest": "sha256:" + "a" * 64},
                       "assignment_sha256": hashlib.sha256(json.dumps(selected, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode()).hexdigest(),
                       "source_commit": selected["source"]["commit"], "run": public_run, "build_result": publications[0]["build_result"],
                       "publication_verified_utc": "1900-01-01T00:00:00Z",
                       "summary": {"compilers": [{"name": "cc", "version": "reported compiler"}],
                                   "tests": {"passed": 3, "failed": 0, "skipped": 1}},
                       "feed": selected["source"]["deployment"]["feed"], "packages": [{"name": "example", "sha256": "e" * 64}]}
            path = "receipts/" + selected["execution"] + "/" + str(selected["revision"]) + ".json"
            raw = json.dumps(receipt).encode()
            sha = hashlib.sha1(b"blob " + str(len(raw)).encode() + b"\0" + raw).hexdigest()
            blobs[sha] = raw
            feed_files[path] = raw
            ids = [hashlib.sha1(("feed-" + str(index)).encode()).hexdigest() for index in range(3)]
            feed_commits["e" * 40] = ids[0]
            trees[ids[0]] = [{"path": "receipts", "type": "tree", "mode": "040000", "sha": ids[1]}]
            trees[ids[1]] = [{"path": selected["execution"], "type": "tree", "mode": "040000", "sha": ids[2]}]
            trees[ids[2]] = [{"path": str(selected["revision"]) + ".json", "type": "blob", "mode": "100644", "sha": sha}]
            deployed_files["/" + path] = b"modified HTTPS receipt"
            assert report(500)[0] == 202, "modified HTTPS receipt authorized publication"
            unverified = next(row for row in request("/api/graph")[1]["executions"] if row["id"] == older["execution"])
            assert unverified["state"] == "publishing" and unverified["receipt"] is None
            deployed_files["/" + path] = raw
            verified_after = time.time()
            assert report(500)[0] == 202, "verified publication did not release the next completed build"
            current = next(row for row in request("/api/graph")[1]["executions"] if row["id"] == older["execution"])
            assert current["state"] == "published" and len(publications) == 2
            recorded_receipt = current["receipt"]
            verified_at = recorded_receipt["publication_verified_utc"]
            observed = datetime.datetime.fromisoformat(verified_at.replace("Z", "+00:00")).timestamp()
            assert verified_after - 1 <= observed <= time.time(), "verification time did not come from the controller"
            assert recorded_receipt["summary"] == receipt["summary"]
            assert report(500)[0] == 200 and report(102)[0] == 200, "late report regressed published state"
            assert next(row for row in request("/api/graph")[1]["executions"] if row["id"] == older["execution"])["receipt"] == recorded_receipt
            assert request("/api/repositories/save", {"repository": "Owner/worker", "branch": "main", "definition": definition})[0] == 422
            runs[100].update(status="completed", conclusion="success")
            assert report(100)[0] == 200 and len(publications) == 2, "completed build was dispatched for publication more than once"
            runs[501].update(status="completed", conclusion="failure")
            malformed_publication_jobs = True
            assert report(501)[0] == 202, "malformed job evidence authorized replacement"
            malformed_publication_jobs = False
            assert report(501)[0] == 200, "proven pre-push failure was left uncertain"
            initial = next(row for row in request("/api/graph")[1]["executions"] if row["id"] == dispatches[0]["execution"])
            assert initial["state"] == "failed" and "before the feed-update step" in initial["failure"]
            assert request("/api/work/retry", {"execution": initial["id"]})[0] == 202 and len(dispatches) == 4
            assert lifecycle.query(db_socket, "SELECT COUNT(*) FROM core_ci.attempts").strip() == "4"
            runs[103].update(status="completed", conclusion="success")
            status, blocked = report(103)
            assert status == 200 and initial["id"] in blocked["blocked"] and len(publications) == 2, (status, blocked, len(publications))
            oversized = next(row for row in request("/api/graph")[1]["executions"] if row["id"] == initial["id"])
            assert oversized["state"] == "building" and any("payload" in reason for reason in oversized["blockers"])
            source_sha = "9" * 40
            register(source_sha, definition)
            assert request("/api/work/request", {"repository": "Owner/example", "tag": "v2.0.0-rc.1"})[0] == 202
            assert len(dispatches) == 5 and dispatches[4]["source"]["commit"] == source_sha
            runs[104].update(status="completed", conclusion="success")
            assert report(104)[0] == 202 and len(publications) == 3, "oversized publication prevented independent dispatch"
            assert publications[-1]["assignment"]["execution"] != initial["id"]
            for digit in ("8", "7"):
                source_sha = digit * 40
                register(source_sha, definition)
                assert request("/api/work/request", {"repository": "Owner/example", "tag": "v2.0.0-rc.1"})[0] == 202
            lock = subprocess.Popen(["mariadb", "--no-defaults", "--socket=" + str(db_socket), "--user=root",
                "--batch", "--skip-column-names", "--unbuffered", "--execute",
                "LOCK TABLES core_ci.graph_revision WRITE; SELECT 'locked'; DO SLEEP(1); UNLOCK TABLES;"], stdout=subprocess.PIPE, text=True)
            assert lock.stdout.readline().strip() == "locked"
            for run_id in (105, 106):
                runs[run_id].update(status="completed", conclusion="failure")
            with concurrent.futures.ThreadPoolExecutor(max_workers=2) as reports:
                pending = [reports.submit(report, run_id) for run_id in (105, 106)]
                outcomes = [response.result() for response in pending]
            assert lock.wait(timeout=5) == 0
            assert all(result[0] == 200 for result in outcomes), outcomes
            current = {row["id"]: row for row in request("/api/graph")[1]["executions"]}
            deadline = time.monotonic() + 5
            # Webhook acknowledgements precede persistence; another report's log is not completion evidence.
            while not all(current[dispatches[index]["execution"]]["state"] == "failed" for index in (5, 6)) and time.monotonic() < deadline:
                time.sleep(0.01)
                current = {row["id"]: row for row in request("/api/graph")[1]["executions"]}
            assert all(current[dispatches[index]["execution"]]["state"] == "failed" for index in (5, 6))
            runs[106].update(conclusion="success")
            assert request("/api/work/retry", {"execution": dispatches[6]["execution"]})[0] == 202
            runs[107].update(status="completed", conclusion="success")
            for unavailable_artifacts in (403, 200):
                assert report(107)[0] == 503
                assert next(row for row in request("/api/graph")[1]["executions"] if row["id"] == dispatches[7]["execution"])["state"] == "building"
            unavailable_artifacts = False
            missing_artifacts = True
            assert report(107)[0] == 200
            current = next(row for row in request("/api/graph")[1]["executions"] if row["id"] == dispatches[7]["execution"])
            assert current["state"] == "failed" and current["failure"] == "required artifacts missing/expired"
            assert current["build_job"]["completed_at"] == "2026-10-26T10:03:25Z", "failure lost verified completion timing"
            original_settings = settings.read_text()
            lifecycle.stop(application)
            settings.write_text(original_settings.replace('pipeline.tooling-sha="' + "f" * 40 + '"', 'pipeline.tooling-sha="' + "e" * 40 + '"'))
            application = subprocess.Popen(command, stdout=app_log, stderr=subprocess.STDOUT)
            lifecycle.wait_http(port, application)
            status, mismatch = request("/api/work/request", {"repository": "Owner/example", "tag": "v2.0.0-rc.1"})
            assert status == 422 and "approved blob" in mismatch["error"] and len(dispatches) == 8
            lifecycle.stop(application)
            settings.write_text(original_settings)
            application = subprocess.Popen(command, stdout=app_log, stderr=subprocess.STDOUT)
            lifecycle.wait_http(port, application)
            historical = next(row for row in request("/api/graph")[1]["executions"] if row["id"] == older["execution"])
            assert historical["receipt"] == recorded_receipt, "restart changed publication timing or summary"
            assert not errors, errors
            print("PASS local pipeline: enrollment/highest tag, committed dispatch, raw HMAC, exact older source, lost response, restart, failure/retry, late reports and assigned artifact identities")
    finally:
        lifecycle.stop(application)
        lifecycle.stop(database)
        github.shutdown()
        github.server_close()
        thread.join(timeout=3)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True)
    parser.add_argument("--directory", required=True)
    args = parser.parse_args()
    Path(args.directory).mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="pipeline-", dir=args.directory) as temporary:
        exercise(args, temporary)
