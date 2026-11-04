"""Read-only enrollment browser fixture. Synthetic inputs; no GitHub or live mutation."""

import argparse
import http.server
import json
from pathlib import Path


def serve(directory):
    assets = Path(__file__).resolve().parents[1] / "ui"
    definition = {"schema": 1, "title": "Core", "package": "core", "depends_on": [], "build": {"native": "ci/native.sh"},
                  "matrix": [{"id": "debian-amd64", "family": "native", "distribution": "debian", "suite": "trixie", "arch": "amd64",
                              "runner": "ubuntu-24.04", "build": {"image": "debian:trixie", "platform": "linux/amd64", "format": "deb"}}]}
    repositories = [{"repository": "Owner/core", "branch": "main", "revision": "a" * 40, "definition": definition}]
    requests = []

    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def reply(self, status, value):
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(value).encode())

        def do_GET(self):
            if self.path in ("/", "/app.js", "/style.css"):
                name = "index.html" if self.path == "/" else self.path[1:]
                self.send_response(200)
                self.send_header("Content-Type", {"index.html": "text/html", "app.js": "application/javascript", "style.css": "text/css"}[name])
                self.end_headers()
                self.wfile.write((assets / name).read_bytes())
            elif self.path == "/api/repositories":
                self.reply(200, {"repositories": repositories})
            elif self.path == "/api/events":
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.end_headers()
                for name, value in (("graph", {"executions": [], "mutations_allowed": True}), ("repositories", {"repositories": repositories})):
                    self.wfile.write(("event:" + name + "\ndata:" + json.dumps(value) + "\n\n").encode())
            else:
                self.reply(404, {})

        def do_POST(self):
            value = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
            requests.append({"path": self.path, "body": value})
            (directory / "requests.json").write_text(json.dumps(requests, indent=2))
            if self.path == "/api/repositories/refresh" and value == {"repository": "Owner/core", "branch": "main"}:
                self.reply(200, repositories[0])
            else:
                self.reply(404, {"error": "unsupported fixture operation"})

    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    print("Local synthetic read-only browser fixture: http://127.0.0.1:" + str(server.server_port), flush=True)
    server.serve_forever()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", type=Path, required=True)
    arguments = parser.parse_args()
    arguments.directory.mkdir(parents=True, exist_ok=True)
    serve(arguments.directory)
