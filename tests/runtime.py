"""Public runtime/UI checks with generated keys and a new, disposable database."""
import hashlib
import hmac
import http.client
import json
import os
from pathlib import Path
import signal
import select
import socket
import ssl
import subprocess
import sys
import tempfile
import time

from persistence import run
from sse import check_delivery


def main():
    build = Path(sys.argv[1]).resolve()
    web_port, hook_port = 18080, 18443
    parent = Path("/tmp/codex/build/core-ci/runtime")
    parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="case-", dir=parent) as temporary:
        root = Path(temporary)
        backend = root / "db.sock"
        datadir = root / "data"
        graph_file = root / "fixture.json"
        run([str(build / "graph-contract-test"), str(graph_file)])
        graph = json.loads(graph_file.read_text())
        for work in graph["executions"]:
            repository = work["source"]["repository"]
            graph["repositories"][repository] = {
                "ref": "reviewed", "commit": work["source"]["commit"], "blob": work["source"]["definition_blob_sha"],
                "definition": {"package": work["source"]["package"], "title": work["source"]["title"]}}
        building = graph["executions"][-2]
        observed = json.loads(json.dumps(graph["executions"][0]["attempts"][-1]["facts"]["build_run"]))
        observed["status"] = "in_progress"
        observed["conclusion"] = None
        observed["jobs"][0].update(status="in_progress", conclusion=None, completed_at=None,
            steps=[{"number": 1, "name": "Compile fixture", "status": "in_progress", "conclusion": None}])
        building["attempts"][-1]["facts"]["build_run"] = observed
        retiring = graph["executions"][-1]["attempts"][-1]
        retiring["state"] = "publishing"
        for key in ["failure_kind", "retirement"]:
            retiring.pop(key, None)
        for key in ["publish_run", "publish_run_id"]:
            retiring["facts"].pop(key, None)
        graph_file.write_text(json.dumps(graph))
        deployment = root / "deployment.json"
        deployment.write_text(json.dumps(graph["executions"][0]["source"]["deployment"]))
        config = root / "empty.conf"
        config.touch()
        key = root / "app.pem"
        certificate = root / "tls.pem"
        run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1", "-subj", "/CN=localhost",
             "-addext", "subjectAltName=DNS:localhost", "-keyout", str(key), "-out", str(certificate)])
        key.chmod(0o600)
        secret = root / "webhook.key"
        secret_bytes = b"isolated-webhook-fixture-32-bytes-minimum"
        secret.write_bytes(secret_bytes)
        secret.chmod(0o600)
        binary = root / "isolated-controller"
        binary.symlink_to(build / "core-ci")
        base = [str(binary), "--config-file", str(config), "--log-file", str(root / "controller.log"), "--quiet=false",
            "--deployment-file", str(deployment), "--app-id", "101", "--installation-id", "102", "--app-key-file", str(key),
            "--webhook-secret-file", str(secret), "--database", "runtime", "--database-user", "root", "--database-socket", str(backend),
            "web", "local", "--port", str(web_port), "webhook", "local", "--port", str(hook_port)]
        for port in [web_port, hook_port]:
            with socket.socket() as probe:
                probe.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                probe.bind(("127.0.0.1", port))
        failure = subprocess.run(base, text=True, capture_output=True, timeout=10)
        failure_text = failure.stderr + failure.stdout
        if (root / "controller.log").exists():
            failure_text += (root / "controller.log").read_text()
        assert "No such file" in failure_text or "Can't connect" in failure_text, failure_text
        for port in [web_port, hook_port]:
            with socket.socket() as probe:
                assert probe.connect_ex(("127.0.0.1", port)) != 0, "Initial DB failure cannot open a listener"
        print("Initial DB failure exits without listener or pending startup")
        run(["mariadb-install-db", "--no-defaults", f"--datadir={datadir}",
             "--auth-root-authentication-method=normal", "--skip-test-db"])
        log = (root / "server.log").open("w")
        server = subprocess.Popen(["/usr/sbin/mariadbd", "--no-defaults", f"--datadir={datadir}", f"--socket={backend}",
            f"--pid-file={root / 'server.pid'}", "--skip-networking", f"--user={os.geteuid()}"], stdout=log, stderr=log)
        controller = None
        limitations = []

        def sql(statement):
            return run(["mariadb", "--no-defaults", f"--socket={backend}", "-uroot", "-N", "-B"], input=statement)

        def wait_http(expected=5):
            ready = False
            deadline = time.monotonic() + 10
            while not ready and time.monotonic() < deadline and controller.poll() is None:
                try:
                    connection = http.client.HTTPConnection("127.0.0.1", web_port, timeout=1)
                    connection.request("GET", "/api/graph")
                    response = connection.getresponse()
                    ready = response.status == 200 and len(json.loads(response.read())["executions"]) == expected
                    connection.close()
                except (OSError, http.client.HTTPException):
                    time.sleep(0.05)
            assert ready, (root / "controller-output.log").read_text()

        try:
            ready = False
            deadline = time.monotonic() + 20
            while not ready and time.monotonic() < deadline and server.poll() is None:
                try:
                    sql("SELECT 1")
                    ready = True
                except RuntimeError:
                    time.sleep(0.05)
            assert ready, (root / "server.log").read_text()
            sql("CREATE DATABASE broken; CREATE TABLE broken.ci_graph(wrong INT)")
            broken = list(base)
            broken[broken.index("--database") + 1] = "broken"
            failed_schema = subprocess.run(broken, text=True, capture_output=True, timeout=10)
            failure_text = failed_schema.stderr + failed_schema.stdout
            if (root / "controller.log").exists():
                failure_text += (root / "controller.log").read_text()
            assert "Column count" in failure_text or "Unknown column" in failure_text, failure_text
            with socket.socket() as probe:
                assert probe.connect_ex(("127.0.0.1", web_port)) != 0, "Schema failure cannot open a listener"
            print("Initial schema failure releases the client without listening or changing the external server")
            sql("CREATE DATABASE runtime; CREATE TABLE runtime.ci_graph(id INT PRIMARY KEY, revision BIGINT UNSIGNED NOT NULL, "
                "body LONGTEXT NOT NULL CHECK(JSON_VALID(body))) ENGINE=InnoDB;")

            def seed(value=graph):
                body = json.dumps(value).encode().hex()
                sql(f"REPLACE INTO runtime.ci_graph VALUES (1,0,CONVERT(0x{body} USING utf8mb4))")

            seed({"repositories": {}, "executions": []})
            output = (root / "controller-output.log").open("w")
            controller = subprocess.Popen(base + ["webhook", "--disabled=false", "tls", "--cert", str(certificate), "--cert-key", str(key)],
                stdout=output, stderr=output)
            wait_http(0)
            seed()
            wait_http()
            threads = [task.joinpath("comm").read_text().strip() for task in Path(f"/proc/{controller.pid}/task").iterdir()]
            print(f"Native process has {len(threads)} OS threads; controller runs in the single SNode.C event loop")
            context = ssl.create_default_context(cafile=str(certificate))
            for signature, status in [("sha256=" + "0" * 64, 401),
                ("sha256=" + hmac.new(secret_bytes, b"{}", hashlib.sha256).hexdigest(), 200)]:
                request = http.client.HTTPSConnection("localhost", hook_port, context=context, timeout=5)
                request.request("POST", "/github/webhook", b"{}", {"Content-Type": "application/json", "X-GitHub-Event": "ping",
                    "X-Hub-Signature-256": signature})
                response = request.getresponse()
                assert response.status == status, response.read()
                response.read()
                request.close()
            request = http.client.HTTPSConnection("localhost", hook_port, context=context, timeout=5)
            request.request("GET", "/api/graph")
            response = request.getresponse()
            assert response.status == 404, "Management cannot be exposed on webhook listener"
            response.read()
            request.close()
            print("Native startup, real HTTP graph and TLS/HMAC listener separation passed")
            locker = subprocess.Popen(["mariadb", "--no-defaults", f"--socket={backend}", "-uroot", "-N", "-B", "--unbuffered"],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            try:
                locker.stdin.write("LOCK TABLES runtime.ci_graph WRITE; SELECT 'locked';\n")
                locker.stdin.flush()
                assert select.select([locker.stdout], [], [], 5)[0] and locker.stdout.readline().strip() == "locked"
                abandoned = socket.create_connection(("127.0.0.1", web_port), timeout=5)
                abandoned.sendall(b"GET /events HTTP/1.1\r\nHost: localhost\r\n\r\n")
                deadline = time.monotonic() + 5
                pending = False
                while not pending and time.monotonic() < deadline:
                    pending = sql("SELECT COUNT(*) FROM information_schema.PROCESSLIST "
                        "WHERE STATE LIKE 'Waiting for table%' AND INFO LIKE 'SELECT body,revision%'").strip() != "0"
                    if not pending:
                        time.sleep(0.02)
                assert pending, "SSE read must actually be delayed behind the MariaDB lock"
                abandoned.shutdown(socket.SHUT_RDWR)
                abandoned.close()
                probe = http.client.HTTPConnection("127.0.0.1", web_port, timeout=5)
                probe.request("GET", "/")
                assert probe.getresponse().read(), "Event loop must process disconnected client while DB is delayed"
                probe.close()
            finally:
                locker.communicate("UNLOCK TABLES;\n", timeout=5)
            wait_http()
            print("Abandoned SSE connection during a real delayed DB read cannot crash the controller")
            if len(sys.argv) > 2 and sys.argv[2] == "sse":
                check_delivery(web_port, graph, seed)
            else:
                stream = http.client.HTTPConnection("127.0.0.1", web_port, timeout=40)
                stream.request("GET", "/events")
                events = stream.getresponse()
                assert events.status == 200 and events.getheader("Content-Type").startswith("text/event-stream")
                snapshot = events.readline()
                assert snapshot.startswith(b"data: ") and json.loads(snapshot[6:]) == graph
                assert events.readline() == b"\n"
                started = time.monotonic()
                for count in range(3):
                    heartbeat = events.readline()
                    while heartbeat in (b"\n", b"\r\n"):
                        heartbeat = events.readline()
                    assert heartbeat == b": keep-alive\n", f"Expected SSE comment without reconnecting; received {heartbeat!r}"
                    assert events.readline() == b"\n", "Heartbeat must be a complete SSE frame"
                    elapsed = time.monotonic() - started
                    assert 25 * (count + 1) <= elapsed <= 35 * (count + 1), elapsed
                assert elapsed > 60, "The same idle SSE connection must survive beyond the transport idle timeout"
                stream.close()
                assert sql("SELECT revision FROM runtime.ci_graph WHERE id=1").strip() == "0", "Heartbeat cannot mutate graph facts"
                print(f"Single idle SSE connection survived {elapsed:.1f}s with three 30-second comments and no graph mutation")
                if os.getenv("CI_BROWSER_MODULES"):
                    command = ["node", str(Path(__file__).with_name("ui.cjs")), str(graph_file), str(backend), str(web_port)]
                    print(run(command, env=dict(os.environ, CI_UI_SCREENSHOT=str(parent / "populated-ui.png"))).strip())
            controller.send_signal(signal.SIGTERM)
            controller.wait(timeout=10)
            controller = None
            assert server.poll() is None and sql("SELECT 1").strip() == "1", "Unmanaged server remains alive"
            seed({"repositories": {}, "executions": []})
            server.terminate()
            server.wait(timeout=20)
            managed_failure = list(base) + ["--managed-datadir", str(datadir)]
            managed_failure[managed_failure.index("--database-socket") + 1] = str(root / "absent" / "db.sock")
            early_exit = subprocess.run(managed_failure, text=True, capture_output=True, timeout=15)
            assert "exited before readiness" in early_exit.stdout, early_exit.stdout
            assert not (datadir / "core-ci.pid").exists(), "Early managed exit is reaped without pending startup"
            with socket.socket() as probe:
                assert probe.connect_ex(("127.0.0.1", web_port)) != 0, "Early server exit cannot open a listener"
            managed_failure = list(base) + ["--managed-datadir", str(datadir)]
            managed_failure[managed_failure.index("--database") + 1] = "broken"
            failed_schema = subprocess.run(managed_failure, text=True, capture_output=True, timeout=15)
            assert "Column count" in failed_schema.stdout or "Unknown column" in failed_schema.stdout, failed_schema.stdout
            assert not (datadir / "core-ci.pid").exists(), "Failed managed startup releases its database process"
            for port in [web_port, hook_port]:
                with socket.socket() as probe:
                    assert probe.connect_ex(("127.0.0.1", port)) != 0, "Managed schema failure cannot open a listener"
            print("Managed schema failure releases owned server and startup observers without listening")
            seed_command = base + ["--managed-datadir", str(datadir)]
            with socket.socket() as occupied:
                occupied.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                occupied.bind(("127.0.0.1", web_port))
                occupied.listen()
                bind_failure = subprocess.run(seed_command, text=True, capture_output=True, timeout=15)
                assert not (datadir / "core-ci.pid").exists(), "Listener failure must release the managed database"
                with socket.socket() as probe:
                    assert probe.connect_ex(("127.0.0.1", hook_port)) != 0, "Failed required listener cannot start the webhook"
            print("Required listener bind failure releases startup resources without starting the webhook")
            controller = subprocess.Popen(seed_command, stdout=output, stderr=output)
            try:
                wait_http(0)
                managed_pid = int((datadir / "core-ci.pid").read_text())
                assert managed_pid != server.pid, "Managed mode must own its own process"
                controller.send_signal(signal.SIGTERM)
                controller.wait(timeout=15)
                assert not Path(f"/proc/{managed_pid}").exists(), "Managed child must be released on exit"
            except AssertionError as failure:
                limitations.append("Managed startup failed: " + str(failure))
            if controller.poll() is not None:
                controller = None
            print("Unmanaged and managed server ownership checks completed")
            output.close()
            assert not limitations, "; ".join(limitations)
        finally:
            if controller and controller.poll() is None:
                controller.terminate()
                try:
                    controller.wait(timeout=35)
                except subprocess.TimeoutExpired:
                    controller.kill()
                    controller.wait(timeout=5)
                owned_pid = datadir / "core-ci.pid"
                if owned_pid.exists():
                    pid = int(owned_pid.read_text())
                    command = Path(f"/proc/{pid}/cmdline")
                    if command.exists() and str(datadir).encode() in command.read_bytes():
                        os.kill(pid, signal.SIGTERM)
            if server.poll() is None:
                server.terminate()
                server.wait(timeout=20)
            log.close()


if __name__ == "__main__":
    main()
