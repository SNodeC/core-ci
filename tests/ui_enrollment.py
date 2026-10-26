"""Interactive browser boundary fixture; no GitHub calls or live acceptance.

Run with --directory under /tmp/codex/build/core-ci. Open the printed URL, enroll
Owner/join with all four package formats and core/side dependencies, and submit
again after the deliberate first write failure. saved.json records exact inputs.
/test/update adds a repository through SSE to check that drafts stay intact.
"""

import argparse
import copy
import http.server
import json
from pathlib import Path
import threading


def serve(directory):
    root = Path(__file__).resolve().parents[1] / "ui"
    condition = threading.Condition()
    generation = 0
    writes = 0
    executions = []
    base = {"schema": 1, "title": "Core", "package": "core", "depends_on": [],
            "build": {"native": "ci/native.sh"}, "matrix": [
                {"id": "core-row", "family": "native", "distribution": "custom", "suite": "series", "arch": "customarch",
                 "runner": ["self-hosted", "custom-label"], "build": {"image": "custom:series", "platform": "linux/custom", "format": "deb"}}]}
    repositories = [{"repository": "Owner/" + name, "branch": "main", "revision": "a" * 40,
                     "blob_sha": "b" * 40, "definition": {**copy.deepcopy(base), "title": name.title(), "package": name}}
                    for name in ("core", "app", "side")]

    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def reply(self, status, value):
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(value).encode())

        def do_GET(self):
            nonlocal generation
            if self.path == "/api/events":
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.end_headers()
                current = -1
                try:
                    while True:
                        with condition:
                            condition.wait_for(lambda: current != generation, timeout=60)
                            current = generation
                            for name, value in [("graph", {"executions": executions, "generation": current, "mutations_allowed": True}),
                                                ("repositories", {"repositories": repositories})]:
                                self.wfile.write(("event:" + name + "\ndata:" + json.dumps(value) + "\n\n").encode())
                            self.wfile.flush()
                except (BrokenPipeError, ConnectionResetError):
                    pass
            elif self.path == "/test/metadata":
                with condition:
                    sample = {"id": "recorded", "label": "Recorded build", "repository": "Owner/core", "tag": "v1.0.0",
                              "target_row": base["matrix"][0], "revision": 2, "state": "published", "activity": "published",
                              "automatic": True, "prerequisites": [], "blockers": [], "build_job": {
                                  "started_at": "2026-10-26T10:00:00Z", "completed_at": "2026-10-26T10:01:42Z"},
                              "receipt": {"publication_verified_utc": "2026-10-26T10:02:00Z", "summary": {
                                  "compilers": [{"name": "cc", "version": "compiler <img src=x onerror=alert(1)>"}],
                                  "tests": {"passed": 3, "failed": 0, "skipped": 1, "coverage_percent": 0,
                                            "report_url": "https://reports.example.test/results"}}}}
                    executions[:] = [sample, {**sample, "id": "historical", "label": "Older build", "revision": 1, "receipt": {}}]
                    generation += 1
                    condition.notify_all()
                self.reply(200, {"synthetic_metadata": generation})
            elif self.path == "/test/update":
                with condition:
                    repositories.append({**repositories[0], "repository": "Owner/extra"})
                    generation += 1
                    condition.notify_all()
                self.reply(200, {"synthetic_update": generation})
            elif self.path == "/api/repositories":
                self.reply(200, {"repositories": repositories})
            elif self.path in ("/", "/confirmed-reset", "/app.js", "/style.css"):
                file = root / ({"/": "index.html", "/confirmed-reset": "index.html"}.get(self.path, self.path[1:]))
                self.send_response(200)
                self.send_header("Content-Type", {".html": "text/html", ".js": "application/javascript", ".css": "text/css"}[file.suffix])
                self.end_headers()
                content = file.read_bytes()
                if self.path == "/confirmed-reset":
                    # Deterministic native-confirm substitute only in this disposable fixture.
                    script = b'<script>window.confirm = function(message) { document.documentElement.dataset.confirmation = message; return true; };</script>'
                    content = content.replace(b'<script src="/app.js"', script + b'<script src="/app.js"')
                self.wfile.write(content)
            else:
                self.reply(404, {})

        def do_POST(self):
            value = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
            if self.path == "/api/repositories/save":
                definition = value["definition"]
                if value["repository"] == "Owner/core":
                    assert value["revision"] == "a" * 40 and value["blob_sha"] == "f" * 40
                    assert len(definition["matrix"]) == 1 and definition["depends_on"] == []
                    assert definition["title"] == "Repaired Core" and definition["package"] == "core"
                    print("PASS structured replacement retained the imported blob/revision write guard", flush=True)
                    self.reply(502, {"github_committed": False, "error": "Repair payload verified; no write performed"})
                else:
                    self.save_join(value, definition)
            elif self.path == "/api/repositories/refresh":
                self.reply(422, {"github_committed": False, "content": "{invalid fixture JSON", "blob_sha": "f" * 40,
                                 "github_revision": "e" * 40, "error": "Unsupported committed fixture definition"})
            else:
                self.reply(409, {"github_committed": False, "error": "Fixture accepts definition saves only"})

        def save_join(self, value, definition):
            nonlocal writes
            assert value["repository"] == "Owner/join" and value["branch"] == "main"
            assert value["revision"] == "" and value["blob_sha"] == "", "new enrollment borrowed another repository's revision"
            assert definition["depends_on"] == ["core", "side"]
            assert {row["build"]["format"] for row in definition["matrix"]} == {"deb", "rpm", "ipk", "apk"}
            assert len({row["id"] for row in definition["matrix"]}) == len(definition["matrix"])
            assert definition["build"] == {"native": "ci/native.sh", "openwrt": "ci/openwrt.sh"}
            assert next(row for row in definition["matrix"] if row["build"]["format"] == "deb")["runner"] == ["self-hosted", "custom-label"]
            assert next(row for row in definition["matrix"] if row["build"]["format"] == "rpm")["runner"] == "custom-runner"
            (directory / "saved.json").write_text(json.dumps(value, indent=2))
            writes += 1
            if writes == 1:
                self.reply(502, {"github_committed": False, "error": "Deliberate fixture write rejection"})
            else:
                repositories.append({**value, "revision": "c" * 40, "blob_sha": "d" * 40})
                self.reply(200, {"repository": value["repository"], "github_committed": True, "revision": "c" * 40})
                print("PASS browser enrollment payload, revision guard, all formats and exact runner selectors", flush=True)

    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    print("Local synthetic browser fixture: http://127.0.0.1:" + str(server.server_port), flush=True)
    server.serve_forever()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", type=Path, required=True)
    arguments = parser.parse_args()
    arguments.directory.mkdir(parents=True, exist_ok=True)
    serve(arguments.directory)
