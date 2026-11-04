"""Real MariaDB plus synthetic GitHub TLS boundary; not live GitHub acceptance."""

import argparse
import base64
import http.server
import hashlib
import json
import os
from pathlib import Path
import pwd
import signal
import select
import socket
import socketserver
import ssl
import subprocess
import tempfile
import time
import threading
import urllib.error
import urllib.request

import lifecycle



class DatabaseProxy(socketserver.ThreadingUnixStreamServer):
    daemon_threads = True

    def __init__(self, path, backend):
        self.backend = str(backend)
        self.drop_commit = False
        self.drop_query = None
        self.dropped = 0
        self.rewrite = None
        self.rewrites = 0
        self.intercepted = []
        super().__init__(str(path), DatabaseRelay)


class DatabaseRelay(socketserver.BaseRequestHandler):
    def handle(self):
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as backend:
            backend.connect(self.server.backend)
            buffers = {self.request: b"", backend: b""}
            connected = True
            commit = False
            while connected:
                readable, _, _ = select.select([self.request, backend], [], [], 2)
                for source in readable:
                    chunk = source.recv(65536)
                    if not chunk:
                        connected = False
                    else:
                        buffers[source] += chunk
                        ready = True
                        while connected and ready and len(buffers[source]) >= 4:
                            size = int.from_bytes(buffers[source][:3], "little") + 4
                            ready = len(buffers[source]) >= size
                            if ready:
                                packet = buffers[source][:size]
                                buffers[source] = buffers[source][size:]
                                if source is self.request:
                                    commit = packet[4:] == b"\x03COMMIT" or (self.server.drop_query is not None and
                                             packet[4:5] == b"\x03" and packet[5:].upper().startswith(self.server.drop_query))
                                    if self.server.rewrite is not None and packet[4:].upper() == b"\x03" + self.server.rewrite:
                                        self.server.intercepted.append(packet[5:].decode())
                                        payload = b"\x03CORE_CI_INVALID_SQL"
                                        packet = len(payload).to_bytes(3, "little") + packet[3:4] + payload
                                        self.server.rewrite = None
                                        self.server.rewrites += 1
                                    backend.sendall(packet)
                                elif commit and self.server.drop_commit:
                                    assert packet[4] == 0, "fixture expected an actual MariaDB mutation success"
                                    self.server.drop_commit = False
                                    self.server.dropped += 1
                                    connected = False
                                else:
                                    self.request.sendall(packet)
                                    commit = False


def exercise(args, root):
    directory = Path(root)
    data = directory / "data"
    data.mkdir(mode=0o700)
    temporary = directory / "tmp"
    temporary.mkdir(mode=0o700)
    account = pwd.getpwuid(os.geteuid()).pw_name
    lifecycle.run(["mariadb-install-db", "--no-defaults", "--datadir=" + str(data), "--auth-root-authentication-method=normal", "--skip-test-db", "--tmpdir=" + str(temporary)])
    data.chmod(0o700)
    db_socket = data / "server.sock"
    server_config = directory / "server.cnf"
    server_config.write_text("[mariadbd]\nuser=" + account + "\ninnodb-buffer-pool-size=32M\ntmpdir=" + str(temporary) + "\n")
    key = directory / "app.pem"
    tls_key = directory / "tls-key.pem"
    certificate = directory / "tls.pem"
    lifecycle.run(["openssl", "genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:2048", "-out", str(key)])
    key.chmod(0o600)
    lifecycle.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1", "-keyout", str(tls_key), "-out", str(certificate), "-subj", "/CN=api.github.com", "-addext", "subjectAltName=DNS:api.github.com"])
    tls_key.chmod(0o600)
    head = "a" * 40
    blob = None
    document = None
    raw_document = b""
    writes = 0
    reject = False
    lose = False
    errors = []

    class GitHub(http.server.BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def send_json(self, status, body):
            encoded = json.dumps(body).encode()
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(encoded)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(encoded)
            self.close_connection = True

        def do_POST(self):
            body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
            if self.path != "/app/installations/7/access_tokens" or body["repository_ids"] != [99] or body["permissions"] not in ({"contents": "read"}, {"contents": "write"}, {"actions": "read"}):
                errors.append("unexpected App scope")
            self.send_json(201, {"token": "synthetic-token", "expires_at": "2099-01-01T00:00:00Z"})

        def do_GET(self):
            if self.headers.get("Authorization") != "Bearer synthetic-token":
                errors.append("missing scoped token")
            if self.path == "/repos/Owner/example":
                self.send_json(200, {"id": 99, "full_name": "Owner/example"})
            elif self.path == "/repos/Owner/example/actions/workflows/core-ci-build.yml/runs?event=workflow_dispatch&per_page=100&page=1":
                self.send_json(200, {"total_count": 0, "workflow_runs": []})
            elif self.path == "/repos/Owner/example/actions/runs/123/attempts/1":
                self.send_json(404, {})
            elif self.path in ("/repos/Owner/example/commits/main", "/repos/Owner/example/commits/" + head):
                self.send_json(200, {"sha": head, "commit": {"tree": {"sha": "c" * 40}}})
            elif self.path.startswith("/repos/Owner/example/tags?"):
                self.send_json(200, [])
            elif self.path == "/repos/Owner/example/git/ref/heads/main":
                self.send_json(200, {"object": {"sha": head}})
            elif self.path == "/repos/Owner/example/git/commits/" + head:
                self.send_json(200, {"sha": head, "tree": {"sha": "c" * 40}})
            elif self.path == "/repos/Owner/example/git/trees/" + "c" * 40 + "?recursive=1":
                entries = [] if document is None else [
                    {"path": "ci", "type": "tree", "mode": "040000", "sha": "d" * 40},
                    {"path": "ci/packages.json", "type": "blob", "mode": "100644", "sha": blob}]
                self.send_json(200, {"truncated": False, "tree": entries})
            elif self.path == "/repos/Owner/example/git/trees/" + "c" * 40:
                entries = [] if document is None else [{"path": "ci", "type": "tree", "mode": "040000", "sha": "d" * 40}]
                self.send_json(200, {"sha": "c" * 40, "truncated": False, "tree": entries})
            elif self.path == "/repos/Owner/example/git/trees/" + "d" * 40:
                self.send_json(200, {"sha": "d" * 40, "truncated": False, "tree": [{"path": "packages.json", "type": "blob", "mode": "100644", "sha": blob}]})
            elif self.path == "/repos/Owner/example/git/blobs/" + str(blob):
                self.send_json(200, {"sha": blob, "encoding": "base64", "size": len(raw_document), "content": base64.b64encode(raw_document).decode()})
            else:
                errors.append("unexpected GitHub read: " + self.path)
                self.send_json(404, {})

        def do_PUT(self):
            errors.append("controller attempted a GitHub definition write")
            self.send_json(403, {})

    fixture = None
    application = None
    github = http.server.HTTPServer(("127.0.0.1", 0), GitHub)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certificate, tls_key)
    github.socket = context.wrap_socket(github.socket, server_side=True)
    thread = threading.Thread(target=github.serve_forever, daemon=True)
    thread.start()
    port = lifecycle.free_port()
    origin = "http://127.0.0.1:" + str(port)
    password = directory / "password"
    password.write_text("")
    password.chmod(0o600)
    config = directory / "controller.conf"
    config.write_text('github.app-id=42\ngithub.installation-id=7\ngithub.allowed-owner="Owner"\ngithub.repository=["Owner/example=99"]\ngithub.private-key-file="' + str(key) + '"\n')
    config.chmod(0o600)
    proxy = DatabaseProxy(directory / "p.sock", db_socket)
    proxy_thread = threading.Thread(target=proxy.serve_forever, daemon=True)
    proxy_thread.start()
    command = [args.executable, "--config-file", str(config), "--log-file", str(directory / "framework.log"), "database", "--mode", "unmanaged", "--socket", str(directory / "p.sock"), "--user", "core_ci_test", "--name", "core_ci", "--password-file", str(password), "web", "local", "--port", str(port), "github-http", "remote", "--host", "127.0.0.1", "--port", str(github.server_port), "tls", "--ca-cert", str(certificate)]

    def request(path, body=None, request_origin=origin, endpoint=origin, content_type="application/json"):
        payload = None if body is None else json.dumps(body).encode()
        headers = {"Content-Type": content_type}
        if request_origin is not None:
            headers["Origin"] = request_origin
        req = urllib.request.Request(endpoint + path, data=payload, headers=headers)
        try:
            response = urllib.request.urlopen(req, timeout=10)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            result = json.load(response)
            status = response.status
        return status, result

    definition = {"schema": 1, "title": "Example", "package": "example", "depends_on": [], "build": {"native": "ci/build.sh"}, "matrix": [{"id": "row", "family": "native", "distribution": "custom", "suite": "series", "arch": "customarch", "runner": ["self-hosted", "custom-label"], "build": {"image": "example:latest", "platform": "linux/custom", "format": "deb"}}]}
    document = definition
    raw_document = json.dumps(document).encode()
    blob = hashlib.sha1(b"blob " + str(len(raw_document)).encode() + b"\0" + raw_document).hexdigest()
    try:
        with (directory / "server.log").open("w") as db_log, (directory / "application.log").open("w") as app_log:
            fixture = subprocess.Popen(["/usr/sbin/mariadbd", "--defaults-file=" + str(server_config), "--datadir=" + str(data), "--socket=" + str(db_socket), "--pid-file=" + str(data / "server.pid"), "--skip-networking"], stdout=db_log, stderr=subprocess.STDOUT)
            lifecycle.wait_sql(db_socket, fixture)
            lifecycle.query(db_socket, "CREATE DATABASE core_ci; CREATE USER 'core_ci_test'@'localhost' IDENTIFIED BY ''; GRANT SELECT,INSERT,UPDATE,DELETE ON core_ci.* TO 'core_ci_test'@'localhost'; USE core_ci; " + (Path(__file__).parent.parent / "sql/schema.sql").read_text())
            application = subprocess.Popen(command, stdout=app_log, stderr=subprocess.STDOUT)
            lifecycle.wait_http(port, application)
            assert request("/api/repositories") == (200, {"repositories": []})
            stream = lifecycle.event_stream(origin)
            peer = lifecycle.event_stream(origin)
            for receiver in (stream, peer):
                assert lifecycle.stream_event(receiver) == ("graph", request("/api/graph")[1])
                assert lifecycle.stream_event(receiver) == ("repositories", {"repositories": []})
            status, result = request("/api/repositories/refresh", {"repository": "Owner/example", "branch": "main"})
            assert status == 200 and result["revision"] == head, result
            first = head
            for receiver in (stream, peer):
                event, current_graph = lifecycle.stream_event(receiver)
                assert event == "graph" and current_graph == request("/api/graph")[1]
                event, current_repositories = lifecycle.stream_event(receiver)
                assert event == "repositories" and current_repositories["repositories"][0]["revision"] == first
            peer.close()
            listed = request("/api/repositories")[1]["repositories"]
            assert len(listed) == 1 and listed[0]["definition"] == definition and listed[0]["revision"] == head
            status, result = request("/api/repositories/refresh", {"repository": "Owner/example", "branch": "main"})
            assert status == 200 and writes == 0 and result["revision"] == first, result
            stream.close()
            lifecycle.stop(application)
            application = subprocess.Popen(command, stdout=app_log, stderr=subprocess.STDOUT)
            lifecycle.wait_http(port, application)
            assert request("/api/repositories")[1]["repositories"] == listed
            for removed in ("save", "workflow"):
                req = urllib.request.Request(origin + "/api/repositories/" + removed, data=b"{}", headers={"Content-Type": "application/json"})
                try:
                    urllib.request.urlopen(req)
                    raise AssertionError("removed GitHub write route remains")
                except urllib.error.HTTPError as error:
                    assert error.code == 404
            assert writes == 0 and not errors, errors
    finally:
        lifecycle.stop(application)
        proxy.shutdown()
        proxy.server_close()
        proxy_thread.join(timeout=3)
        lifecycle.stop(fixture)
        github.shutdown()
        github.server_close()
        thread.join(timeout=3)
    print("PASS read-only GitHub enrollment: committed definition, SSE, restart and no GitHub write routes")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True)
    parser.add_argument("--directory", required=True)
    args = parser.parse_args()
    Path(args.directory).mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="enrollment-", dir=args.directory) as temporary:
        try:
            exercise(args, temporary)
        except Exception:
            for name in ("application.log", "server.log"):
                report = Path(temporary) / name
                if report.exists():
                    print(name, "\n".join(report.read_text(errors="replace").splitlines()[-20:]))
            raise
