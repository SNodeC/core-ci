"""Local TLS/App contract tests; these are not live GitHub acceptance."""

import argparse
import base64
import http.server
import json
import os
from pathlib import Path
import ssl
import subprocess
import tempfile
import threading
import time


def run(command, data=None):
    result = subprocess.run(command, input=data, capture_output=True, timeout=15)
    assert result.returncode == 0, "fixture command failed: " + result.stderr.decode(errors="replace")
    return result.stdout


def exercise(args, directory):
    public_mode = False
    root = Path(directory)
    app_key = root / "app-key.pem"
    public = root / "app-public.pem"
    tls_key = root / "tls-key.pem"
    certificate = root / "tls.pem"
    config = root / "controller.conf"
    config.write_text("")
    run(["openssl", "genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:2048", "-out", str(app_key)])
    app_key.chmod(0o600)
    run(["openssl", "pkey", "-in", str(app_key), "-pubout", "-out", str(public)])
    hostname = "wrong.example" if args.case == "wrong-hostname" else "api.github.com"
    run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1", "-keyout", str(tls_key), "-out", str(certificate), "-subj", "/CN=" + hostname, "-addext", "subjectAltName=DNS:" + hostname])
    tls_key.chmod(0o600)
    errors = []
    seen = []

    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, format, *values):
            pass

        def send_json(self, status, body):
            data = json.dumps(body).encode()
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(data)
            self.close_connection = True

        def do_POST(self):
            try:
                seen.append("token")
                assert self.path == "/app/installations/7/access_tokens"
                value = self.headers["Authorization"].removeprefix("Bearer ")
                header, payload, signature = value.split(".")
                claims = json.loads(base64.urlsafe_b64decode(payload + "=" * (-len(payload) % 4)))
                assert claims["iss"] == "42"
                assert abs(claims["iat"] - (time.time() - 60)) < 30
                assert time.time() < claims["exp"] <= time.time() + 600
                sig = root / "signature"
                sig.write_bytes(base64.urlsafe_b64decode(signature + "=" * (-len(signature) % 4)))
                run(["openssl", "dgst", "-sha256", "-verify", str(public), "-signature", str(sig)], (header + "." + payload).encode())
                body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                assert body == {"repository_ids": [99], "permissions": {"contents": "read"}}
                if args.case == "token-rejected":
                    self.send_json(403, {"message": "fixture rejected token"})
                elif args.case == "token-lost-response":
                    self.close_connection = True
                else:
                    self.send_json(201, {"token": "unit-test-only-token"})
            except Exception as error:
                errors.append(type(error).__name__)
                self.send_json(400, {"message": "fixture rejected request"})

        def do_GET(self):
            seen.append("repository")
            selected = "Other/feed" if public_mode else "Owner/example"
            if self.path != "/repos/" + selected or self.headers.get("Authorization") != "Bearer unit-test-only-token":
                errors.append("unexpected repository request")
            if args.case == "lost-response":
                self.close_connection = True
            else:
                self.send_json(200, {"full_name": selected, "id": 1001 if public_mode else 99})

    server = http.server.HTTPServer(("127.0.0.1", 0), Handler)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certificate, tls_key)
    server.socket = context.wrap_socket(server.socket, server_side=True)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    expected = "200" if args.case == "success" else "403" if args.case == "token-rejected" else "0"
    command = [args.executable, "--config-file", str(config), "--log-file", str(root / "framework.log"), "github", "--app-id", "42", "--installation-id", "7", "--private-key-file", str(app_key), "--allowed-owner", "Owner", "--repository", "Owner/example=99", "github-http", "remote", "--host", "127.0.0.1", "--port", str(server.server_port)]
    if args.case != "wrong-ca":
        command += ["tls", "--ca-cert", str(certificate)]
    environment = os.environ.copy()
    environment["CORE_CI_TEST_EXPECTED_STATUS"] = expected
    environment["CORE_CI_TEST_EXPECTED_SUBMISSION"] = "1" if args.case in ("success", "lost-response") else "0"
    try:
        result = subprocess.run(command, capture_output=True, text=True, env=environment, timeout=15)
        assert result.returncode == 0, result.stdout + result.stderr
        assert not errors, errors
        assert "unit-test-only-token" not in result.stdout + result.stderr
        if args.case in ("wrong-hostname", "wrong-ca"):
            assert seen == [], "authorization was sent before TLS identity was verified"
        elif args.case in ("token-rejected", "token-lost-response"):
            assert seen == ["token"], "a failed token request reached the repository operation"
        else:
            assert seen == ["token", "repository"], "request was dropped or replayed"
        if args.case == "success":
            public_mode = True
            seen.clear()
            environment["CORE_CI_TEST_REPOSITORY"] = "Other/feed"
            environment["CORE_CI_TEST_PUBLIC_READ"] = "1"
            result = subprocess.run(command, capture_output=True, text=True, env=environment, timeout=15)
            assert result.returncode == 0 and not errors, result.stdout + result.stderr + str(errors)
            assert seen == ["token", "repository"], "public read bypassed scoped installation authentication"
            # Token fixture above still requires only repository 99 and contents:read, never feed repository 1001.
            del environment["CORE_CI_TEST_PUBLIC_READ"]
            environment["CORE_CI_TEST_EXPECTED_STATUS"] = "0"
            environment["CORE_CI_TEST_EXPECTED_SUBMISSION"] = "0"
            for method in ("GET", "POST"):
                environment["CORE_CI_TEST_METHOD"] = method
                for repository in ("Other/feed", "Owner/ungranted"):
                    seen.clear()
                    environment["CORE_CI_TEST_REPOSITORY"] = repository
                    result = subprocess.run(command, capture_output=True, text=True, env=environment, timeout=15)
                    assert result.returncode == 0 and seen == [], "ungranted source request reached GitHub"
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=3)
    print("PASS local TLS/App contract", args.case)


parser = argparse.ArgumentParser()
parser.add_argument("--executable", required=True)
parser.add_argument("--directory", required=True)
parser.add_argument("--case", required=True)
arguments = parser.parse_args()
Path(arguments.directory).mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix="run-", dir=arguments.directory) as temporary:
    exercise(arguments, temporary)
