#!/usr/bin/env python3
"""CTest smoke harness. All database access is through the SNode.C reset fixture and daemon."""
import hashlib
import hmac
import os
from pathlib import Path
import re
import socket
import subprocess
import sys
import tempfile
import time
import urllib.parse


def port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def run():
    build = Path(sys.argv[1]).resolve()
    env = dict(os.environ, CORECI_GITHUB_TOKEN="smoke-token", CORECI_WEBHOOK_SECRET="smoke-webhook",
               CORECI_CALLBACK_SECRET="smoke-callback", CORECI_ADMIN_TOKEN="smoke-admin",
               CORECI_DB_PASSWORD=os.environ.get("CORECI_TEST_DB_PASSWORD", ""))
    processes = []
    logs = []
    subprocess.run([str(build / "coreci-reset-store")], env=env, check=True, timeout=10)
    with tempfile.TemporaryDirectory(prefix="smoke-", dir=build / "tmp") as directory:
        work = Path(directory)
        def certificate(name, host):
            cert, key = work / (name + ".crt"), work / (name + ".key")
            subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                            "-subj", "/CN=" + host, "-addext", "subjectAltName=DNS:" + host,
                            "-keyout", str(key), "-out", str(cert)], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            return cert, key
        cert, key = certificate("good", "localhost")
        wrong_cert, wrong_key = certificate("wrong", "wrong.invalid")
        api_port, bad_port, app_port, second_port = port(), port(), port(), port()
        trace, wrong_trace = work / "requests.txt", work / "wrong.txt"
        def start(command, name, pipe=False):
            log = work / (name + ".log")
            stream = subprocess.PIPE if pipe else log.open("w")
            p = subprocess.Popen([str(x) for x in command], env=env, stdout=stream, stderr=subprocess.STDOUT, text=True)
            processes.append(p)
            if not pipe:
                logs.append(log)
                stream.close()
            return p
        def stop(p):
            if p.poll() is None:
                p.terminate()
                p.wait(timeout=10)
        def curl(url, body=None, headers=(), retry=False):
            command = ["curl", "--silent", "--show-error", "--cacert", str(cert), "--max-time", "8", "--write-out", "\n%{http_code}"]
            if retry:
                command += ["--retry", "5", "--retry-all-errors", "--retry-connrefused", "--retry-delay", "1"]
            for header in headers:
                command += ["-H", header]
            if body is not None:
                command += ["--data-binary", "@-"]
            result = subprocess.run(command + [url], text=True, input=body, capture_output=True, timeout=55, check=True)
            text, code = result.stdout.rsplit("\n", 1)
            return int(code), text
        api = "https://localhost:" + str(api_port)
        app_url = "https://localhost:" + str(app_port)
        config = work / "nonsecret.conf"
        config.write_text('db-user="wrong_user_from_config"\nbuild-timeout="7200"\n')
        def launch_app(listen_port, name, pipe=False):
            command = [build / "tests/core-ci-smoke", "--config-file", config, "--db-host", os.environ["CORECI_TEST_DB_HOST"],
                       "--db-port", os.environ.get("CORECI_TEST_DB_PORT", "0"), "--db-user", os.environ["CORECI_TEST_DB_USER"],
                       "--db-name", os.environ["CORECI_TEST_DB_NAME"], "--github-api-url", api, "--github-ca-cert", cert,
                       "--callback-base-url", app_url, "--tls-cert", cert, "--tls-key", key,
                       "--listen-address", "127.0.0.1", "--listen-port", str(listen_port), "coreci"]
            return start(command, name, pipe)
        def wait_dispatch(count):
            assert curl(api + "/wait?count=" + str(count))[0] == 200
        def webhook(package, tag="v1.0.0", delivery=None):
            body = ('{"ref":"refs/tags/' + tag + '","created":true,"deleted":false,"after":"' + "a" * 40 +
                    '","repository":{"full_name":"owner/' + package + '"}}')
            signature = "sha256=" + hmac.new(b"smoke-webhook", body.encode(), hashlib.sha256).hexdigest()
            return curl(app_url + "/api/v1/github/webhook", body,
                        ["Content-Type: application/json", "X-GitHub-Event: push", "X-GitHub-Delivery: " + (delivery or package + tag),
                         "X-Hub-Signature-256: " + signature])
        def report(package, target, phase, status, version="1.0.0", literal_plus=False):
            build_id = package + "." + target + "." + version + ".r1"
            values = {"phase": phase, "status": status, "run_id": "101" if phase == "build" else "201"}
            if status == "started": values["run_attempt"] = "1"
            if status == "succeeded": values["artifact" if phase == "build" else "feed_commit"] = "artifact" if phase == "build" else "b" * 40
            body = urllib.parse.urlencode(values)
            stamp = str(int(time.time()))
            signature = "sha256=" + hmac.new(b"smoke-callback", (stamp + "." + body).encode(), hashlib.sha256).hexdigest()
            result = curl(app_url + "/api/v1/builds/" + urllib.parse.quote(build_id, safe=".+" if literal_plus else ".") + "/report", body,
                          ["Content-Type: application/x-www-form-urlencoded", "X-CoreCI-Timestamp: " + stamp,
                           "X-CoreCI-Signature-256: " + signature])
            assert result == (200, "accepted"), result
        def request_count(build_id, kind):
            return sum(1 for line in trace.read_text().splitlines() if line.startswith("POST ") and kind + ".yml/dispatches" in line
                       and '"build_id":"' + build_id + '"' in line)
        try:
            start([build / "coreci-fake-github", "--requests", trace, "--certificate", cert, "--private-key", key, "--port", str(api_port), "fake"], "fake")
            assert curl(api + "/wait?count=0", retry=True)[0] == 200
            subprocess.run([str(build / "coreci-transport-probe"), api, str(cert), "204"], check=True, timeout=10, env=env)
            assert "BAD HEADERS" not in trace.read_text()
            print("PASS: verified TLS dispatch and GitHub headers/body", flush=True)
            start([build / "coreci-fake-github", "--requests", wrong_trace, "--certificate", wrong_cert,
                   "--private-key", wrong_key, "--port", str(bad_port), "fake"], "wrong-fake")
            # curl verifies this fixture using its own CA and hostname; establish readiness through its configuration-free listener.
            ready = subprocess.run(["curl", "-sS", "--retry", "5", "--retry-all-errors", "--retry-connrefused", "--retry-delay", "1",
                                    "--cacert", str(wrong_cert), "--resolve", "wrong.invalid:" + str(bad_port) + ":127.0.0.1",
                                    "--noproxy", "*", "https://wrong.invalid:" + str(bad_port) + "/wait?count=0"], capture_output=True, timeout=30)
            assert ready.returncode == 0
            subprocess.run([str(build / "coreci-transport-probe"), "https://localhost:" + str(bad_port), str(wrong_cert), "0"],
                           check=True, timeout=10, env=env)
            assert not wrong_trace.exists() or not wrong_trace.read_text().strip()
            print("PASS: wrong-host certificate rejected; no HTTP request reached fake", flush=True)
            # Probe made one valid dispatch before the S1 sequence.
            count = 1
            daemon = launch_app(app_port, "daemon")
            assert curl(app_url + "/api/v1/status", retry=True)[0] == 200
            assert curl(app_url + "/unknown")[0] == 404
            assert curl(app_url + "/api/v1/status", "")[0] == 405
            assert curl(app_url + "/api/v1/graph.dot")[1].startswith("digraph coreci")
            assert curl(app_url + "/api/v1/reconcile", "")[0] == 401
            assert curl(app_url + "/api/v1/github/webhook", "not json")[0] == 401
            def signed_form(body, skew=0, content_type="application/x-www-form-urlencoded"):
                stamp = str(int(time.time()) + skew)
                signature = "sha256=" + hmac.new(b"smoke-callback", (stamp + "." + body).encode(), hashlib.sha256).hexdigest()
                return curl(app_url + "/api/v1/builds/unknown/report", body,
                            ["Content-Type: " + content_type, "X-CoreCI-Timestamp: " + stamp, "X-CoreCI-Signature-256: " + signature])
            assert signed_form("phase=build&phase=publish")[0] == 400
            assert signed_form("phase=%xx")[0] == 400
            assert signed_form("phase=build", skew=301)[0] == 401
            assert signed_form("phase=build", content_type="application/x-www-form-urlencodedBAD")[0] == 400
            assert signed_form("x=" + "x" * 65536)[0] == 413
            assert signed_form("phase=publish&status=started") == (200, "ignored: no event")
            print("PASS: config-file/CLI precedence; HTTP auth, validation, limits, methods and DOT", flush=True)
            for package in ("b", "c", "d", "e"):
                assert webhook(package)[0] == 200
            assert curl(api + "/hold?id=a.t2.1.0.0.r1&kind=build")[0] == 200
            assert webhook("a")[0] == 200
            count += 2; wait_dispatch(count)
            report("a", "t1", "build", "started")
            report("a", "t1", "build", "succeeded"); count += 1; wait_dispatch(count)
            report("a", "t1", "publish", "succeeded"); count += 2; wait_dispatch(count)
            assert request_count("d.t1.1.0.0.r1", "build") == 0
            report("b", "t1", "build", "succeeded"); count += 1; wait_dispatch(count)
            report("b", "t1", "publish", "succeeded")
            assert request_count("d.t1.1.0.0.r1", "build") == 0
            assert curl(api + "/hold?id=c.t1.1.0.0.r1&kind=publish")[0] == 200
            report("c", "t1", "build", "succeeded"); count += 1; wait_dispatch(count)
            stop(daemon)
            daemon = launch_app(app_port, "restarted")
            assert curl(app_url + "/api/v1/status", retry=True)[0] == 200
            count += 2; wait_dispatch(count)
            assert request_count("a.t2.1.0.0.r1", "build") == 2
            assert request_count("c.t1.1.0.0.r1", "publish") == 2
            print("PASS: restart re-emitted the same build and publish dispatches", flush=True)
            second = launch_app(second_port, "second", pipe=True)
            observed = False
            for line in second.stdout:
                if "store unavailable; recovering" in line:
                    observed = True
                    break
            assert observed
            refused = subprocess.run(["curl", "-sS", "--cacert", str(cert), "--max-time", "1",
                                      "https://localhost:" + str(second_port) + "/api/v1/status"], capture_output=True)
            assert refused.returncode != 0
            stop(second)
            assert curl(app_url + "/api/v1/status")[0] == 200
            print("PASS: second instance refused by writer lock; first remains usable", flush=True)
            report("c", "t1", "publish", "succeeded"); count += 1; wait_dispatch(count)
            assert request_count("d.t1.1.0.0.r1", "build") == 1
            d = next(line for line in trace.read_text().splitlines() if '"build_id":"d.t1.1.0.0.r1"' in line)
            assert '"prerequisites":"b=1.0.0-1,c=1.0.0-1"' in d
            report("a", "t2", "build", "succeeded"); count += 1; wait_dispatch(count)
            report("a", "t2", "publish", "succeeded"); count += 2; wait_dispatch(count)
            print("PASS: S1 diamond joins once with both published inputs; t2 progresses independently", flush=True)
            assert webhook("a", "v1.0.1+meta")[0] == 200
            count += 2; wait_dispatch(count)
            # The earlier diamond build is still consistent with the published prerequisites.
            report("d", "t1", "build", "succeeded"); count += 1; wait_dispatch(count)
            # a@t1 was settled, so its new version dispatch is already allocated.
            report("a", "t1", "build", "started", "1.0.1+meta", literal_plus=True)
            print("PASS: callback route accepts literal dot and plus in a current buildId", flush=True)
            assert "BAD HEADERS" not in trace.read_text()
            stop(daemon)
            subprocess.run([str(build / "coreci-reset-store")], env=env, check=True, timeout=10)
            assert curl(api + "/tag-pages")[0] == 200
            reconciled = launch_app(app_port, "reconciliation")
            assert curl(app_url + "/api/v1/status", retry=True)[0] == 200
            count += 2; wait_dispatch(count)
            final_status = curl(app_url + "/api/v1/status")[1]
            assert "a\tt1\t1.0.0\t" in final_status, final_status
            assert "b\tt1\t-\t-\tNoRelease" in final_status, final_status
            assert "c\tt1\t-\t-\tNoRelease" in final_status, final_status
            assert "d\tt1\t2.0.0\t" in final_status, final_status
            assert "e\tt1\t-\t-\tNoRelease" in final_status, final_status
            a_dispatch = [line for line in trace.read_text().splitlines() if '"build_id":"a.t1.1.0.0.r1"' in line][-1]
            assert '"tag":"1.0.0"' in a_dispatch
            assert "/repos/owner/b/tags?page=2" in trace.read_text()
            stop(reconciled)
            print("PASS: reconciliation pagination, highest version, lexical tie, prerelease filtering and per-repository abort", flush=True)
            for log in logs:
                if log.name in ("daemon.log", "restarted.log", "reconciliation.log"):
                    assert all(secret not in log.read_text() for secret in ("smoke-token", "smoke-webhook", "smoke-callback", "smoke-admin"))
            print("PASS: daemon logs contain no authentication secrets", flush=True)
        except BaseException:
            for log in logs:
                print("--- " + log.name + " ---", file=sys.stderr)
                print(log.read_text(), file=sys.stderr)
            raise
        finally:
            for process in reversed(processes):
                stop(process)
            subprocess.run([str(build / "coreci-reset-store")], env=env, check=True, timeout=10)
    return 0


def main():
    result = 77
    if any(not os.environ.get(name) for name in ("CORECI_TEST_DB_HOST", "CORECI_TEST_DB_USER", "CORECI_TEST_DB_NAME")):
        print("Skipped: CORECI_TEST_DB_HOST, CORECI_TEST_DB_USER and CORECI_TEST_DB_NAME must be set")
    else:
        result = run()
    return result


if __name__ == "__main__":
    sys.exit(main())
