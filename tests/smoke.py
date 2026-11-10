#!/usr/bin/env python3
"""CTest smoke harness. All database access is through the SNode.C reset fixture and daemon."""
import hashlib
import hmac
import json
import os
from pathlib import Path
import re
import socket
import ssl
import subprocess
import sys
import tempfile
import time
import urllib.parse
import xml.etree.ElementTree as ET


def ports(count):
    reserved = []
    try:
        for _ in range(count):
            sock = socket.socket()
            reserved.append(sock)
            sock.bind(("127.0.0.1", 0))
        result = [sock.getsockname()[1] for sock in reserved]
    finally:
        for sock in reserved:
            sock.close()
    return result


def run():
    build = Path(sys.argv[1]).resolve()
    env = dict(os.environ, CORECI_GITHUB_TOKEN="smoke-token", CORECI_WEBHOOK_SECRET="smoke-webhook",
               CORECI_CALLBACK_SECRET="smoke-callback", CORECI_ADMIN_TOKEN="smoke-admin",
               CORECI_DB_PASSWORD=os.environ.get("CORECI_TEST_DB_PASSWORD", ""))
    processes = []
    logs = []
    subprocess.run([str(build / "coreci-reset-store")], env=env, check=True, timeout=10)
    (build / "tmp").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="smoke-", dir=build / "tmp") as directory:
        work = Path(directory)
        declarations = work / "declarations"
        declarations.mkdir()
        (declarations / "registry.json").write_text(
            '{"feeds":{"f1":{"publisher":"owner/feeds/.github/workflows/publish.yml","presentation":"owner/feeds/.github/workflows/status.yml"},'
            '"f2":{"publisher":"owner/feeds/.github/workflows/publish.yml"}},"default_feed":"f1","status_url":"https://localhost/api/v1/status.json",'
            '"build":"owner/recipes/.github/workflows/build.yml","repositories":["owner/a","owner/b","owner/c","owner/d","owner/e"]}')
        for p, prerequisites in (("a", '[]'), ("b", '["a"]'), ("c", '["a"]'), ("d", '["b","c"]'), ("e", '["d"]')):
            matrix = '{"id":"t1","feed":"f1"}' + ('' if p == "e" else ',{"id":"t2","feed":"f2"}')
            (declarations / (p + ".json")).write_text(
                '{"package":"' + p + '","depends_on":' + prerequisites + ',"matrix":[' + matrix + ']}')
        settings = json.loads((declarations / "registry.json").read_text())
        settings["targets"] = "ci/targets.json"
        (declarations / "registry.json").write_text(json.dumps(settings))
        (declarations / "targets.json").write_text(json.dumps({"targets": [dict(id=t, distribution="test", suite="stable", arch="test", build={"format": "deb"}) for t in ("t1", "t2")]}))
        loads = 0
        def certificate(name, host):
            cert, key = work / (name + ".crt"), work / (name + ".key")
            subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                            "-subj", "/CN=" + host, "-addext", "subjectAltName=DNS:" + host,
                            "-keyout", str(key), "-out", str(cert)], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            return cert, key
        cert, key = certificate("good", "localhost")
        wrong_cert, wrong_key = certificate("wrong", "wrong.invalid")
        public_key = work / "idp-public.pem"
        subprocess.run(["openssl", "pkey", "-in", str(key), "-pubout", "-out", str(public_key)], check=True)
        api_port, bad_port, app_port, second_port = ports(4)
        assert len({api_port, bad_port, app_port, second_port}) == 4
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
        def app_command(listen_port, parser=()):
            command = [build / "core-ci", "--registry", "owner/registry:ci/registry.json", "--status-interval", "30", "--config-file", config, "--db-host", os.environ["CORECI_TEST_DB_HOST"],
                       "--db-port", os.environ.get("CORECI_TEST_DB_PORT", "0"), "--db-user", os.environ["CORECI_TEST_DB_USER"],
                       "--db-name", os.environ["CORECI_TEST_DB_NAME"], "--github-api-url", api, "--github-ca-cert", cert,
                       "--callback-base-url", app_url, "--tls-cert", cert, "--tls-key", key,
                       "--listen-address", "127.0.0.1", "--listen-port", str(listen_port),
                       "--idp-client-id", "smoke", "--idp-public-key-path", public_key, "--dashboard-users", "owner", "coreci"]
            if parser:
                command += ["http", "parser", *parser]
            return command
        def launch_app(listen_port, name, pipe=False, parser=()):
            return start(app_command(listen_port, parser), name, pipe)
        def raw_status(headers):
            context = ssl.create_default_context(cafile=str(cert))
            with socket.create_connection(("127.0.0.1", app_port), timeout=5) as tcp:
                with context.wrap_socket(tcp, server_hostname="localhost") as connection:
                    connection.sendall(headers.encode("ascii"))
                    line = bytearray()
                    while b"\r\n" not in line and len(line) < 8192:
                        data = connection.recv(1024)
                        assert data, "connection closed before HTTP rejection"
                        line.extend(data)
            return int(line.split(b"\r\n", 1)[0].split()[1])
        def wait_dispatch(count):
            assert curl(api + "/wait?count=" + str(count))[0] == 200
        def wait_load():
            nonlocal loads
            loads += 1
            assert curl(api + "/wait-load?count=" + str(loads))[0] == 200
        def webhook(package, tag="v1.0.0", delivery=None):
            body = ('{"ref":"refs/tags/' + tag + '","created":true,"deleted":false,"after":"' + "a" * 40 +
                    '","repository":{"full_name":"OwNeR/' + package + '"}}')
            signature = "sha256=" + hmac.new(b"smoke-webhook", body.encode(), hashlib.sha256).hexdigest()
            return curl(app_url + "/api/v1/github/webhook", body,
                        ["Content-Type: application/json", "X-GitHub-Event: push", "X-GitHub-Delivery: " + (delivery or package + tag),
                         "X-Hub-Signature-256: " + signature])
        def report(package, target, phase, status, version="1.0.0", literal_plus=False, revision=1):
            build_id = package + "~" + target + "~" + version + "~r" + str(revision)
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
        def signed_path(path):
            body = "phase=build&status=started&run_id=101&run_attempt=1"
            stamp = str(int(time.time()))
            signature = "sha256=" + hmac.new(b"smoke-callback", (stamp + "." + body).encode(), hashlib.sha256).hexdigest()
            return curl(app_url + path, body, ["Content-Type: application/x-www-form-urlencoded",
                        "X-CoreCI-Timestamp: " + stamp, "X-CoreCI-Signature-256: " + signature])
        def badge_title(package, message):
            code, svg = curl(app_url + "/api/v1/badge/" + urllib.parse.quote(package, safe="") + "/t1.svg")
            assert code == 200, (code, svg)
            root = ET.fromstring(svg)
            assert root.attrib["height"] == "20" and root.attrib["role"] == "img"
            assert root.find("{http://www.w3.org/2000/svg}title").text == package + ": " + message, svg
        def request_count(build_id, kind):
            return sum(1 for line in trace.read_text().splitlines() if line.startswith("POST ") and kind + ".yml/dispatches" in line
                       and '"build_id":"' + build_id + '"' in line)
        try:
            start([build / "coreci-fake-github", "--requests", trace, "--declarations", declarations, "--certificate", cert, "--private-key", key, "--port", str(api_port), "fake"], "fake")
            assert curl(api + "/wait?count=0", retry=True)[0] == 200
            subprocess.run([str(build / "coreci-transport-probe"), api, str(cert), "204"], check=True, timeout=10, env=env)
            assert "BAD HEADERS" not in trace.read_text()
            print("PASS: verified TLS dispatch and GitHub headers/body", flush=True)
            start([build / "coreci-fake-github", "--requests", wrong_trace, "--declarations", declarations, "--certificate", wrong_cert,
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
            for name, maximum in (("body-bytes", 25 * 1024 * 1024), ("header-bytes", 64 * 1024),
                                  ("header-fields", 100), ("start-line-bytes", 8 * 1024)):
                for value in (0, maximum + 1):
                    invalid = subprocess.run(app_command(app_port, ["--maximum-" + name, str(value)]),
                                             env=env, capture_output=True, text=True, timeout=10)
                    assert invalid.returncode != 0 and "invalid option: maximum-" + name in invalid.stderr, (invalid.returncode, invalid.stdout, invalid.stderr)
            lower = launch_app(app_port, "lower-limits", parser=["--maximum-body-bytes", "16384",
                               "--maximum-header-bytes", "1024", "--maximum-header-fields", "4",
                               "--maximum-start-line-bytes", "1024"])
            assert curl(app_url + "/api/v1/status", retry=True)[0] == 200
            wait_load()
            assert raw_status("POST /api/v1/github/webhook HTTP/1.1\r\nHost: localhost\r\nContent-Length: 16385\r\n\r\n") == 413
            assert raw_status("GET / HTTP/1.1\r\nHost: localhost\r\nX-Large: " + "x" * 1500 + "\r\n\r\n") == 431
            assert raw_status("GET / HTTP/1.1\r\nHost: localhost\r\n" + "X-Test: x\r\n" * 4 + "\r\n") == 431
            assert raw_status("GET /" + "x" * 1500 + " HTTP/1.1\r\nHost: localhost\r\n\r\n") == 431
            stop(lower)
            print("PASS: all four parser limits may be lowered; zero and above-cap values refused", flush=True)
            daemon = launch_app(app_port, "daemon")
            assert curl(app_url + "/api/v1/status", retry=True)[0] == 200
            wait_load()
            declared = 25 * 1024 * 1024 + 1
            assert raw_status("POST /api/v1/github/webhook HTTP/1.1\r\nHost: localhost\r\nContent-Length: " + str(declared) + "\r\n\r\n") == 413
            print("PASS: declared body " + str(declared) + " bytes rejected with 413 after headers; body bytes sent = 0", flush=True)
            assert raw_status("GET / HTTP/1.1\r\nHost: localhost\r\n" + "X-Large: " + "x" * 8000 + "\r\n" +
                              ("X-More: " + "x" * 8000 + "\r\n") * 8 + "\r\n") == 431
            assert raw_status("GET / HTTP/1.1\r\nHost: localhost\r\n" + "X-Test: x\r\n" * 100 + "\r\n") == 431
            assert raw_status("GET /" + "x" * 8192 + " HTTP/1.1\r\nHost: localhost\r\n\r\n") == 431
            oversized = "x" * (65 * 1024)
            assert curl(app_url + "/api/v1/builds/unknown/report", oversized,
                        ["Content-Type: application/x-www-form-urlencoded", "X-CoreCI-Timestamp: " + str(int(time.time())),
                         "X-CoreCI-Signature-256: invalid"]) == (413, "body too large")
            print("PASS: 65 KiB callback rejected with 413 before authentication despite invalid signature", flush=True)
            assert curl(app_url + "/unknown")[0] == 404
            assert curl(app_url + "/api/v1/status", "")[0] == 405
            # Explicit HEAD handlers preserve 405 instead of express's automatic HEAD-to-GET fallback.
            assert raw_status("HEAD /api/v1/status HTTP/1.1\r\nHost: localhost\r\n\r\n") == 405
            assert raw_status("HEAD /api/v1/badge/d/t1.svg HTTP/1.1\r\nHost: localhost\r\n\r\n") == 405
            assert curl(app_url + "/api/v1/builds/a%ZZ/report", "")[0] == 400
            assert curl(app_url + "/api/v1/builds/a%252Bb/report", "")[0] == 401
            assert signed_path("/api/v1/builds/a%252Bb/report") == (200, "ignored: unknown or stale buildId")
            assert signed_path("/api/v1/builds/a%25ZZb/report") == (200, "ignored: unknown or stale buildId")
            # One decode leaves a literal %2B (three characters); a second would make this valid-length id.
            assert signed_path("/api/v1/builds/" + "a" * 253 + "%252B/report")[0] == 400
            assert signed_path("/api/v1/builds/" + "a" * 252 + "%252B/report") == (200, "ignored: unknown or stale buildId")
            print("PASS: express routes, 404/405, framework decode errors before auth; %252B stays %2B after one decode", flush=True)
            badge_title("d", "no release")
            badge_title("unknown", "unknown")
            badge_title("&<>\"'", "unknown")
            headers = subprocess.run(["curl", "-sS", "--cacert", str(cert), "--max-time", "8", "-D", "-", "-o", os.devnull,
                                      app_url + "/api/v1/badge/d/t1.svg"], check=True, text=True, capture_output=True).stdout.lower()
            assert "content-type: image/svg+xml; charset=utf-8" in headers
            assert "cache-control: no-cache, no-store, must-revalidate" in headers
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
            assert signed_form("phase=build", skew=-301)[0] == 401
            assert signed_form("phase=build", content_type="application/x-www-form-urlencodedBAD")[0] == 400
            assert signed_form("x=" + "x" * 65536)[0] == 413
            assert signed_form("phase=publish&status=started") == (200, "ignored: no event")
            print("PASS: config-file/CLI precedence; HTTP auth, validation, limits, methods and DOT", flush=True)
            for package in ("b", "c", "d", "e"):
                assert webhook(package)[0] == 200
            assert curl(api + "/hold?id=a~t2~1.0.0~r1&kind=build")[0] == 200
            assert webhook("a")[0] == 200
            count += 2; wait_dispatch(count)
            report("a", "t1", "build", "started")
            report("a", "t1", "build", "succeeded"); count += 1; wait_dispatch(count)
            report("a", "t1", "publish", "succeeded"); count += 2; wait_dispatch(count)
            assert request_count("d~t1~1.0.0~r1", "build") == 0
            report("b", "t1", "build", "succeeded"); count += 1; wait_dispatch(count)
            report("b", "t1", "publish", "succeeded")
            assert request_count("d~t1~1.0.0~r1", "build") == 0
            assert curl(api + "/hold?id=c~t1~1.0.0~r1&kind=publish")[0] == 200
            report("c", "t1", "build", "succeeded"); count += 1; wait_dispatch(count)
            stop(daemon)
            daemon = launch_app(app_port, "restarted")
            assert curl(app_url + "/api/v1/status", retry=True)[0] == 200
            wait_load()
            count += 2; wait_dispatch(count)
            assert request_count("a~t2~1.0.0~r1", "build") == 2
            assert request_count("c~t1~1.0.0~r1", "publish") == 2
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
            assert request_count("d~t1~1.0.0~r1", "build") == 1
            badge_title("d", "building 1.0.0")
            d = next(line for line in trace.read_text().splitlines() if '"build_id":"d~t1~1.0.0~r1"' in line)
            assert '"prerequisites":"b=1.0.0-1,c=1.0.0-1"' in d
            report("a", "t2", "build", "succeeded"); count += 1; wait_dispatch(count)
            report("a", "t2", "publish", "succeeded"); count += 2; wait_dispatch(count)
            print("PASS: S1 diamond joins once with both published inputs; t2 progresses independently", flush=True)
            report("d", "t1", "build", "succeeded"); count += 1; wait_dispatch(count)
            report("d", "t1", "publish", "succeeded"); count += 1; wait_dispatch(count)
            badge_title("d", "1.0.0-1")
            print("PASS: live badge no-release, unknown, XML escaping, headers, building and published revision", flush=True)
            report("e", "t1", "build", "succeeded"); count += 1; wait_dispatch(count)
            report("e", "t1", "publish", "succeeded")
            code, body = curl(app_url + "/api/v1/status.json")
            status = json.loads(body)
            assert code == 200 and status['schema'] == 1 and status['generation'] > 0
            assert body == json.dumps(status,sort_keys=True,separators=(',',':'))
            unit = next(u for u in status['units'] if (u['package'],u['target']) == ('e','t1'))
            assert unit['status'] == 'Settled' and unit['published']['runId'] == 101
            previous = sum('/status.yml/dispatches ' in line for line in trace.read_text().splitlines())
            waited = subprocess.run(['curl','--silent','--show-error','--cacert',str(cert),'--max-time','40',
                api+'/wait-status?count='+str(previous+1)],capture_output=True,text=True,timeout=45,check=True)
            assert waited.stdout == 'ready'
            sent = [line for line in trace.read_text().splitlines() if '/status.yml/dispatches ' in line][-1]
            request = json.loads(sent.split(' ',2)[2])
            assert request['ref'] == 'trunk' and request['inputs']['feed'] == 'f1'
            assert int(request['inputs']['generation']) >= status['generation']
            print('PASS: canonical status.json and presentation dispatch after publication',flush=True)
            rebuild_url = app_url + '/api/v1/units/e/t1/rebuild'
            assert curl(rebuild_url, '')[0] == 401
            assert curl(rebuild_url, '', ['Authorization: Bearer wrong'])[0] == 401
            assert curl(rebuild_url)[0] == 405
            assert curl(rebuild_url, '', ['Authorization: Bearer smoke-admin']) == (200, 'accepted')
            count += 1; wait_dispatch(count)
            assert request_count('e~t1~1.0.0~r2', 'build') == 1
            assert curl(rebuild_url, '', ['Authorization: Bearer smoke-admin'])[1].startswith('ignored:')
            report('e', 't1', 'build', 'succeeded', revision=2); count += 1; wait_dispatch(count)
            report('e', 't1', 'publish', 'succeeded', revision=2)
            print('PASS: authenticated rebuild route allocates the next revision and publishes the same version', flush=True)
            changed = '{"package":"e","depends_on":["b","d"],"matrix":[{"id":"t1","feed":"f1"}]}'
            (declarations / "e.json").write_text(changed)
            def branch_push(delivery):
                body = '{"ref":"refs/heads/trunk","deleted":false,"repository":{"full_name":"OwNeR/e","default_branch":"trunk"}}'
                signature = "sha256=" + hmac.new(b"smoke-webhook", body.encode(), hashlib.sha256).hexdigest()
                return curl(app_url + "/api/v1/github/webhook", body, ["Content-Type: application/json", "X-GitHub-Event: push",
                    "X-GitHub-Delivery: " + delivery, "X-Hub-Signature-256: " + signature])
            assert branch_push("new-prerequisite") == (202, "reload scheduled")
            assert branch_push("new-prerequisite") == (200, "ignored: delivery")
            wait_load(); count += 1; wait_dispatch(count)
            assert request_count("e~t1~1.0.0~r3", "build") == 1
            posts = [line for line in trace.read_text().splitlines() if line.startswith("POST ")]
            assert '\"build_id\":\"e~t1~1.0.0~r3\"' in posts[-1]
            assert '\"prerequisites\":\"b=1.0.0-1,d=1.0.0-1\"' in posts[-1]
            print("PASS: signed default-branch push records GraphChanged; only e is rebuilt with new inputs", flush=True)
            (declarations / "e.json").write_text(changed.replace('["b","d"]', '["e"]'))
            assert branch_push("cycle") == (202, "reload scheduled")
            wait_load()
            cyclic = curl(app_url + "/api/v1/status")[1]
            assert "cycle: e -> e" in cyclic.splitlines()[0], cyclic
            assert "e\tt1\t1.0.0\t1.0.0-2\t" in cyclic, cyclic
            assert request_count("e~t1~1.0.0~r3", "build") == 1
            (declarations / "e.json").write_text(changed)
            assert curl(app_url + "/api/v1/reload", "", ["Authorization: Bearer smoke-admin"]) == (202, "started")
            wait_load()
            assert curl(app_url + "/api/v1/status")[1].startswith("PACKAGE\tTARGET")
            print("PASS: cyclic declaration preserves graph and appears on status; successful reload clears error", flush=True)
            assert curl(api + "/hold-file")[0] == 200
            before_loads = sum("/contents/ci%2Fregistry.json" in line for line in trace.read_text().splitlines())
            assert curl(app_url + "/api/v1/reload", "", ["Authorization: Bearer smoke-admin"]) == (202, "started")
            assert curl(api + "/wait-file")[0] == 200
            for _ in range(3):
                assert curl(app_url + "/api/v1/reload", "", ["Authorization: Bearer smoke-admin"]) == (202, "already running")
            assert curl(api + "/release-file")[0] == 200
            wait_load(); wait_load()
            after_loads = sum("/contents/ci%2Fregistry.json" in line for line in trace.read_text().splitlines())
            assert after_loads - before_loads == 2, (before_loads, after_loads)
            print("PASS: reload triggers during a load coalesce into exactly one further load", flush=True)
            assert curl(api + "/hold-tag")[0] == 200
            assert curl(app_url + "/api/v1/reload", "", ["Authorization: Bearer smoke-admin"]) == (202, "started")
            assert curl(api + "/wait-tag")[0] == 200
            wait_load()
            assert curl(app_url + "/api/v1/reconcile", "", ["Authorization: Bearer smoke-admin"]) == (202, "already running")
            assert curl(app_url + "/api/v1/reload", "", ["Authorization: Bearer smoke-admin"]) == (202, "started")
            wait_load()
            assert curl(api + "/release-tag")[0] == 200
            print("PASS: a completed declaration load reconciles even while an older tag round is pending", flush=True)
            stop(daemon)
            daemon = launch_app(app_port, "graph-restarted")
            assert curl(app_url + "/api/v1/status", retry=True)[0] == 200
            wait_load()
            assert "e\tt1\t1.0.0\t1.0.0-2\tBuilding" in curl(app_url + "/api/v1/status")[1]
            assert request_count("e~t1~1.0.0~r3", "build") == 1
            print("PASS: changed graph and execution state survive a fresh process replay", flush=True)
            assert webhook("a", "v1.0.1+meta")[0] == 200
            count += 2; wait_dispatch(count)
            # a@t1 was settled, so its new version dispatch is already allocated.
            report("a", "t1", "build", "started", "1.0.1+meta", literal_plus=True)
            report("a", "t2", "build", "started", "1.0.1+meta")
            print("PASS: callback route accepts encoded %2B and literal dot, plus and tilde in current buildIds", flush=True)
            assert "BAD HEADERS" not in trace.read_text()
            stop(daemon)
            subprocess.run([str(build / "coreci-reset-store")], env=env, check=True, timeout=10)
            assert curl(api + "/tag-pages")[0] == 200
            reconciled = launch_app(app_port, "reconciliation")
            assert curl(app_url + "/api/v1/status", retry=True)[0] == 200
            wait_load()
            count += 2; wait_dispatch(count)
            final_status = curl(app_url + "/api/v1/status")[1]
            assert "a\tt1\t1.0.0\t" in final_status, final_status
            assert "b\tt1\t-\t-\tNoRelease" in final_status, final_status
            assert "c\tt1\t-\t-\tNoRelease" in final_status, final_status
            assert "d\tt1\t2.0.0\t" in final_status, final_status
            assert "e\tt1\t-\t-\tNoRelease" in final_status, final_status
            a_dispatch = [line for line in trace.read_text().splitlines() if '"build_id":"a~t1~1.0.0~r1"' in line][-1]
            assert '"tag":"1.0.0"' in a_dispatch
            assert "/repos/owner/b/tags?page=2" in trace.read_text()
            print("PASS: reconciliation pagination, highest version, lexical tie, prerelease filtering and per-repository abort", flush=True)
            interrupt = start([build / 'coreci-interrupt-store'], 'interrupt', pipe=True)
            locked = False
            line = None
            while not locked and line != '':
                line = interrupt.stdout.readline()
                locked = line.strip() == 'LOCKED'
            assert locked, 'database fixture did not acquire the released writer lock'
            assert webhook('a', 'v1.0.9')[0] == 503
            assert curl(app_url + '/api/v1/status.json') == (503, 'unavailable')
            badge_title('d', 'unavailable')
            assert interrupt.wait(timeout=12) == 0
            ready = False
            attempts = 0
            while not ready and attempts < 10:
                ready = curl(app_url + '/api/v1/status.json')[0] == 200
                attempts += 1
                if not ready:
                    time.sleep(1)
            assert ready, 'status.json did not recover after database replay'
            print('PASS: status.json is 503 during real connection-loss recovery and returns after replay', flush=True)
            stop(reconciled)
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
