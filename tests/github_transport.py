"""Local TLS/App contract tests; these are not live GitHub acceptance."""

import argparse
import base64
import datetime
import http.server
import json
import os
from pathlib import Path
import ssl
import signal
import socket
import sys
import subprocess
import tempfile
import threading
import time


def run(command, data=None):
    result = subprocess.run(command, input=data, capture_output=True, timeout=15)
    assert result.returncode == 0, "fixture command failed: " + result.stderr.decode(errors="replace")
    return result.stdout


def exercise(args, directory):
    bind_address = "127.0.0.1"
    if args.case == "multi-address":
        hosts = Path(directory) / "hosts"
        hosts.write_text("127.0.0.2 core-ci-multi.test\n127.0.0.1 core-ci-multi.test\n")
        run(["mount", "--bind", str(hosts), "/etc/hosts"])
        addresses = list(dict.fromkeys(row[4][0] for row in socket.getaddrinfo(
            "core-ci-multi.test", 443, socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_TCP, socket.AI_CANONNAME)))
        assert len(addresses) == 2, addresses
        bind_address = addresses[1]
    public_mode = False
    parallel = args.case in ("parallel", "parallel-single", "shutdown")
    shutdown_started = threading.Event()
    shutdown_release = threading.Event()
    invalid_capacity = args.case in ("capacity-zero", "capacity-seventeen")
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
    token_grants = []
    active = 0
    peak = 0
    lock = threading.Lock()

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
                if args.case == "mutation-lost-response" and self.path == "/repos/Owner/example":
                    seen.append("repository")
                    assert self.headers.get("Authorization") == "Bearer unit-test-only-token"
                    self.close_connection = True
                else:
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
                    assert body["repository_ids"] == [99]
                    token_grants.append(body["permissions"])
                    assert body["permissions"] in ({"contents": "read"}, {"actions": "read"}) if args.case == "token-reuse" else body["permissions"] == {"contents": "read"}
                    if args.case == "token-rejected":
                        self.send_json(403, {"message": "fixture rejected token"})
                    elif args.case == "token-lost-response":
                        self.close_connection = True
                    else:
                        expiry = "2000-01-01T00:00:00Z" if args.case == "expired-token" else "2099-01-01T00:00:00Z"
                        if args.case == "token-reuse" and seen.count("token") == 1:
                            expiry = (datetime.datetime.now(datetime.timezone.utc) + datetime.timedelta(seconds=2)).strftime("%Y-%m-%dT%H:%M:%SZ")
                        self.send_json(201, {"token": "unit-test-only-token", "expires_at": expiry})
            except Exception as error:
                errors.append(type(error).__name__)
                self.send_json(400, {"message": "fixture rejected request"})

        def do_GET(self):
            nonlocal active, peak
            seen.append("repository")
            selected = "Other/feed" if public_mode else "Owner/example"
            probe = int(self.path.rsplit("/", 1)[1]) if parallel else None
            expected_path = "/repos/" + selected + ("/probe/" + str(probe) if probe is not None else "")
            if self.path != expected_path or self.headers.get("Authorization") != "Bearer unit-test-only-token":
                errors.append("unexpected repository request")
            if args.case == "token-reuse" and seen.count("repository") == 1:
                # The initial identical reads share one GET; expire its token before either callback runs.
                time.sleep(2.1)
            if args.case == "shutdown":
                shutdown_started.set()
                assert shutdown_release.wait(5), "shutdown fixture was not released"
                self.close_connection = True
            elif probe is not None:
                with lock:
                    active += 1
                    peak = max(peak, active)
                time.sleep(0.2 if probe == 0 else 0.04)
            if args.case in ("lost-response", "shutdown"):
                self.close_connection = True
            else:
                self.send_json(200, {"full_name": selected, "id": 1001 if public_mode else 99, "probe": probe})
            if probe is not None:
                with lock:
                    active -= 1

    server_type = http.server.ThreadingHTTPServer if parallel else http.server.HTTPServer
    server = server_type((bind_address, 0), Handler)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certificate, tls_key)
    server.socket = context.wrap_socket(server.socket, server_side=True)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    expected = "200" if args.case in ("success", "token-reuse", "parallel", "parallel-single", "multi-address") else "403" if args.case == "token-rejected" else "502" if args.case == "expired-token" else "0"
    command = [args.executable, "--config-file", str(config), "--log-file", str(root / "framework.log"), "github", "--app-id", "42", "--installation-id", "7", "--private-key-file", str(app_key), "--allowed-owner", "Owner", "--repository", "Owner/example=99", "github-http", "remote", "--host", "127.0.0.1", "--port", str(server.server_port)]
    if args.case == "multi-address":
        command[command.index("--host") + 1] = "core-ci-multi.test"
        command[1:1] = ["--log-level", "5"]
    if args.case == "disabled":
        command += ["probe-http", "--disabled=true"]
    if args.case in ("parallel-single", "shutdown") or invalid_capacity:
        capacity = "1" if parallel else "0" if args.case == "capacity-zero" else "17"
        command.insert(command.index("remote"), "--max-in-flight=" + capacity)
    if args.case != "wrong-ca":
        command += ["tls", "--ca-cert", str(certificate)]
    environment = os.environ.copy()
    environment["CORE_CI_TEST_EXPECTED_STATUS"] = expected
    if args.case == "mutation-lost-response":
        environment["CORE_CI_TEST_METHOD"] = "POST"
    if args.case == "disabled":
        environment["CORE_CI_TEST_DIRECT_DISABLED"] = "1"
        environment["CORE_CI_TEST_EXPECTED_ERROR"] = "disabled; request was not submitted"
    if args.case == "shutdown":
        environment["CORE_CI_TEST_SHUTDOWN"] = "1"
    if parallel:
        environment["CORE_CI_TEST_PARALLEL"] = "1"
    if args.case == "token-reuse":
        environment["CORE_CI_TEST_REUSE"] = "1"
    environment["CORE_CI_TEST_EXPECTED_SUBMISSION"] = "1" if args.case in ("success", "lost-response", "mutation-lost-response", "token-reuse", "parallel", "parallel-single", "multi-address") else "0"
    try:
        if args.case == "shutdown":
            process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=environment)
            try:
                assert shutdown_started.wait(5), "submitted request was not held"
                process.send_signal(signal.SIGTERM)
                stdout, stderr = process.communicate(timeout=5)
                result = subprocess.CompletedProcess(command, process.returncode, stdout, stderr)
            finally:
                shutdown_release.set()
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=3)
        else:
            result = subprocess.run(command, capture_output=True, text=True, env=environment, timeout=15)
        assert (result.returncode != 0 and "range" in result.stdout + result.stderr) if invalid_capacity else result.returncode == 0, result.stdout + result.stderr
        assert not errors, errors
        assert "unit-test-only-token" not in result.stdout + result.stderr
        if args.case == "disabled" or invalid_capacity:
            assert seen == [], "disabled/invalid instance submitted HTTP"
        elif args.case in ("wrong-hostname", "wrong-ca"):
            assert seen == [], "authorization was sent before TLS identity was verified"
        elif args.case in ("token-rejected", "token-lost-response", "expired-token"):
            assert seen == ["token"], "a failed token request reached the repository operation"
        elif args.case == "shutdown":
            assert seen == ["token", "repository"], seen
        elif parallel:
            assert seen.count("token") == 1 and seen.count("repository") == 12, seen
            assert (peak == 1 if args.case == "parallel-single" else 2 <= peak <= 8), peak
            print("Synthetic concurrent HTTP peak:", peak)
        elif args.case == "token-reuse":
            assert seen.count("token") == 3 and seen.count("repository") == 3, seen
            assert seen[:2] == ["token", "repository"], "cold identical reads did not share their GET"
            assert token_grants.count({"contents": "read"}) == 2 and token_grants.count({"actions": "read"}) == 1, token_grants
        else:
            assert seen == ["token", "repository"], "request was dropped or replayed"
        if args.case == "multi-address":
            assert "using next socketaddress" in (result.stdout + result.stderr).lower(), result.stdout + result.stderr
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
if arguments.case == "multi-address" and os.environ.get("CORE_CI_MULTI_NS") != "1":
    result = subprocess.run(["unshare", "--user", "--map-current-user", "--keep-caps", "--mount", "--propagation", "private",
                             sys.executable, __file__, *sys.argv[1:]], env={**os.environ, "CORE_CI_MULTI_NS": "1"},
                            capture_output=True, text=True, timeout=20)
    print(result.stdout, end="")
    print(result.stderr, end="", file=sys.stderr)
    unsupported = "unshare failed: Operation not permitted" in result.stderr
    raise SystemExit(77 if unsupported else result.returncode)
else:
    with tempfile.TemporaryDirectory(prefix="run-", dir=arguments.directory) as temporary:
        exercise(arguments, temporary)
