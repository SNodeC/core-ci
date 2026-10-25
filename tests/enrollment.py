"""Real MariaDB plus synthetic GitHub TLS boundary; not live GitHub acceptance."""

import argparse
import base64
import http.server
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
import threading
import urllib.error
import urllib.request

import lifecycle



class DatabaseProxy(socketserver.ThreadingUnixStreamServer):
    daemon_threads = True

    def __init__(self, path, backend):
        self.backend = str(backend)
        self.drop_commit = False
        self.dropped = 0
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
                                    commit = packet[4:] == b"\x03COMMIT"
                                    backend.sendall(packet)
                                elif commit and self.server.drop_commit:
                                    assert packet[4] == 0, "fixture expected an actual MariaDB COMMIT success"
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
            if self.path != "/app/installations/7/access_tokens" or body["repository_ids"] != [99] or body["permissions"] not in ({"contents": "read"}, {"contents": "write"}):
                errors.append("unexpected App scope")
            self.send_json(201, {"token": "synthetic-token"})

        def do_GET(self):
            if self.headers.get("Authorization") != "Bearer synthetic-token":
                errors.append("missing scoped token")
            if self.path == "/repos/Owner/example":
                self.send_json(200, {"id": 99, "full_name": "Owner/example"})
            elif self.path == "/repos/Owner/example/git/ref/heads/main":
                self.send_json(200, {"object": {"sha": head}})
            elif self.path == "/repos/Owner/example/contents/ci/packages.json?ref=" + head:
                if document is None:
                    self.send_json(404, {"message": "not found"})
                else:
                    self.send_json(200, {"type": "file", "sha": blob, "encoding": "base64", "content": base64.b64encode(json.dumps(document).encode()).decode()})
            else:
                errors.append("unexpected GitHub read: " + self.path)
                self.send_json(404, {})

        def do_PUT(self):
            nonlocal head, blob, document, writes
            body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
            assert self.path == "/repos/Owner/example/contents/ci/packages.json"
            if reject:
                self.send_json(403, {"message": "fixture denied"})
            elif body.get("sha") != blob:
                self.send_json(409, {"message": "conflict"})
            else:
                writes += 1
                head = format(writes + 10, "040x")
                blob = format(writes + 20, "040x")
                document = json.loads(base64.b64decode(body["content"]))
                if lose:
                    self.close_connection = True
                else:
                    self.send_json(201, {"commit": {"sha": head}, "content": {"sha": blob}})

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
    config.write_text('github.app-id=42\ngithub.installation-id=7\ngithub.allowed-owner="Owner"\ngithub.repository=["Owner/example=99"]\ngithub.private-key-file="' + str(key) + '"\nui.origin="' + origin + '"\n')
    config.chmod(0o600)
    proxy = DatabaseProxy(directory / "p.sock", db_socket)
    proxy_thread = threading.Thread(target=proxy.serve_forever, daemon=True)
    proxy_thread.start()
    command = [args.executable, "--config-file", str(config), "--log-file", str(directory / "framework.log"), "database", "--mode", "unmanaged", "--socket", str(directory / "p.sock"), "--user", "core_ci_test", "--name", "core_ci", "--password-file", str(password), "web", "local", "--port", str(port), "github-http", "remote", "--host", "127.0.0.1", "--port", str(github.server_port), "tls", "--ca-cert", str(certificate)]

    def request(path, body=None, request_origin=origin):
        payload = None if body is None else json.dumps(body).encode()
        headers = {"Content-Type": "application/json", "Origin": request_origin}
        req = urllib.request.Request(origin + path, data=payload, headers=headers)
        try:
            response = urllib.request.urlopen(req, timeout=10)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            result = json.load(response)
            status = response.status
        return status, result

    definition = {"schema": 1, "title": "Example", "package": "example", "depends_on": [], "build": {"native": "ci/build.sh"}, "matrix": [{"id": "row", "family": "native", "distribution": "custom", "suite": "series", "arch": "customarch", "runner": ["self-hosted", "custom-label"], "build": {"image": "example:latest", "platform": "linux/custom", "format": "deb"}}]}
    save = {"repository": "Owner/example", "branch": "main", "definition": definition, "revision": ""}
    try:
        with (directory / "server.log").open("w") as db_log, (directory / "application.log").open("w") as app_log:
            fixture = subprocess.Popen(["/usr/sbin/mariadbd", "--defaults-file=" + str(server_config), "--datadir=" + str(data), "--socket=" + str(db_socket), "--pid-file=" + str(data / "server.pid"), "--skip-networking"], stdout=db_log, stderr=subprocess.STDOUT)
            lifecycle.wait_sql(db_socket, fixture)
            lifecycle.query(db_socket, "CREATE DATABASE core_ci; CREATE USER 'core_ci_test'@'localhost' IDENTIFIED BY ''; GRANT SELECT,INSERT,UPDATE,DELETE ON core_ci.* TO 'core_ci_test'@'localhost'; USE core_ci; " + (Path(__file__).parent.parent / "sql/schema.sql").read_text())
            application = subprocess.Popen(command, stdout=app_log, stderr=subprocess.STDOUT)
            lifecycle.wait_http(port, application)
            assert request("/api/repositories") == (200, {"repositories": []})
            status, result = request("/api/repositories/save", save, "http://untrusted.example")
            assert status == 403 and writes == 0, result
            status, result = request("/api/repositories/save", save)
            assert status == 200 and result["github_committed"] is True and writes == 1, result
            first = result["revision"]
            status, listed = request("/api/repositories")
            assert status == 200 and listed["repositories"][0]["definition"] == definition, listed
            assert listed["repositories"][0]["revision"] == first
            status, result = request("/api/repositories/save", save)
            assert status == 409 and writes == 1, result
            save["revision"] = first
            reject = True
            status, result = request("/api/repositories/save", save)
            assert status == 502 and result["github_committed"] is False and writes == 1, result
            reject = False
            lose = True
            status, result = request("/api/repositories/save", save)
            assert status == 502 and result["github_committed"] is None and writes == 2, result
            assert request("/api/repositories")[1]["repositories"][0]["revision"] == first
            lose = False
            status, result = request("/api/repositories/refresh", {"repository": "Owner/example", "branch": "main"})
            assert status == 200 and result["revision"] == head and writes == 2, result
            lifecycle.stop(application)
            application = subprocess.Popen(command, stdout=app_log, stderr=subprocess.STDOUT)
            lifecycle.wait_http(port, application)
            assert request("/api/repositories")[1]["repositories"][0]["revision"] == head
            # A database failure after a successful GitHub commit must be reported honestly.
            save["revision"] = head
            lifecycle.query(db_socket, "REVOKE INSERT ON core_ci.* FROM 'core_ci_test'@'localhost'")
            connection_id = lifecycle.query(db_socket, "SELECT ID FROM information_schema.PROCESSLIST WHERE USER='core_ci_test' LIMIT 1").strip()
            lifecycle.query(db_socket, "KILL CONNECTION " + connection_id)
            lifecycle.wait_http(port, application)
            status, result = request("/api/repositories/save", save)
            assert status == 503 and result["github_committed"] is True and writes == 3, result
            assert request("/api/repositories")[1]["repositories"][0]["revision"] == save["revision"], "failed transaction leaked an enrollment revision"
            lifecycle.query(db_socket, "GRANT INSERT ON core_ci.* TO 'core_ci_test'@'localhost'")
            connection_id = lifecycle.query(db_socket, "SELECT ID FROM information_schema.PROCESSLIST WHERE USER='core_ci_test' LIMIT 1").strip()
            lifecycle.query(db_socket, "KILL CONNECTION " + connection_id)
            lifecycle.wait_http(port, application)
            status, result = request("/api/repositories/refresh", {"repository": "Owner/example", "branch": "main"})
            assert status == 200, result
            save["revision"] = head
            proxy.drop_commit = True
            status, result = request("/api/repositories/save", save)
            assert status == 503 and result["github_committed"] is True and proxy.dropped == 1, result
            # The server really committed, but the application did not receive proof.
            assert lifecycle.query(db_socket, "SELECT configuration_revision FROM core_ci.repositories").strip() == head
            previous_writes = writes
            status, result = request("/api/repositories/save", save)
            assert status == 503 and writes == previous_writes, "uncertain database outcome allowed another remote mutation"
            lifecycle.stop(application)
            application = subprocess.Popen(command, stdout=app_log, stderr=subprocess.STDOUT)
            lifecycle.wait_http(port, application)
            assert request("/api/repositories")[1]["repositories"][0]["revision"] == head
            assert not errors, errors
            assert "synthetic-token" not in (directory / "application.log").read_text()
            for log in directory.glob("*.log"):
                assert "synthetic-token" not in log.read_text(errors="replace")
    finally:
        lifecycle.stop(application)
        proxy.shutdown()
        proxy.server_close()
        proxy_thread.join(timeout=3)
        lifecycle.stop(fixture)
        github.shutdown()
        github.server_close()
        thread.join(timeout=3)
    print("PASS local enrollment: committed revisions, stale edits, rejected/uncertain writes, reconciliation, restart, rollback, lost COMMIT and origin protection")


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
