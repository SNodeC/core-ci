"""Exercise public lifecycle boundaries against an isolated real MariaDB server."""

import argparse
import json
import os
from pathlib import Path
import pwd
import signal
import socket
import subprocess
import time
import tempfile
import urllib.error
import urllib.request


def run(command):
    result = subprocess.run(command, capture_output=True, text=True, timeout=20)
    assert result.returncode == 0, result.stdout + result.stderr
    return result.stdout


def free_port():
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        port = listener.getsockname()[1]
    return port


def wait_sql(path, process):
    deadline = time.monotonic() + 15
    connected = False
    while not connected and time.monotonic() < deadline and process.poll() is None:
        result = subprocess.run(
            ["mariadb", "--no-defaults", "--socket=" + str(path), "--user=root", "-e", "SELECT 1"],
            capture_output=True,
            timeout=2,
        )
        connected = result.returncode == 0
        if not connected:
            time.sleep(0.05)
    assert connected, "test fixture MariaDB did not start"


def query(path, sql):
    return run(["mariadb", "--no-defaults", "--socket=" + str(path), "--user=root", "-N", "-e", sql])


def wait_http(port, process):
    deadline = time.monotonic() + 10
    body = None
    while body is None and time.monotonic() < deadline and process.poll() is None:
        try:
            with urllib.request.urlopen("http://127.0.0.1:" + str(port) + "/api/status", timeout=1) as response:
                body = json.load(response)
        except (OSError, urllib.error.URLError):
            time.sleep(0.05)
    assert body == {"database": "available"}, "no successful live database status"


def stop(process):
    if process is not None and process.poll() is None:
        process.send_signal(signal.SIGTERM)
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)
            raise AssertionError("process did not terminate normally")


def exercise(args):
    directory = Path(args.directory)
    directory.mkdir(parents=True, exist_ok=True)
    directory.chmod(0o700)
    config = directory / "controller.conf"
    config.write_text("")
    password = directory / "password"
    password.write_text("")
    password.chmod(0o600)

    if args.case == "help":
        root = subprocess.run([args.executable, "--config-file", str(config), "--help=expanded"], capture_output=True, text=True, timeout=5)
        assert "--managed-executable" in root.stdout
        assert "--password-file" in root.stdout
        web = subprocess.run([args.executable, "--config-file", str(config), "web", "--help=expanded"], capture_output=True, text=True, timeout=5)
        assert "--port" in web.stdout and "127.0.0.1" in web.stdout
        webhook = subprocess.run([args.executable, "--config-file", str(config), "webhook", "--help=expanded"], capture_output=True, text=True, timeout=5)
        assert "--disabled" in webhook.stdout and "[true]" in webhook.stdout
        enabled = subprocess.run([args.executable, "--config-file", str(config), "webhook", "--disabled=false", "--help=expanded"], capture_output=True, text=True, timeout=5)
        assert "8443" in enabled.stdout and "--cert-key" in enabled.stdout
        parser_help = subprocess.run([args.executable, "--config-file", str(config), "webhook", "--disabled=false", "http", "parser", "--help"], capture_output=True, text=True, timeout=5)
        assert "--maximum-body-bytes" in parser_help.stdout and "2097152" in parser_help.stdout
        assert "--publication-limit" not in root.stdout, "removed admission cap remains configurable"
        assert "Initial MariaDB connection established" not in root.stdout + root.stderr
    else:
        datadir = directory / "data"
        assert not datadir.exists(), "use a clean task test directory"
        datadir.mkdir(mode=0o700)
        temporary = directory / "tmp"
        temporary.mkdir(mode=0o700)
        certificate = directory / "webhook.pem"
        tls_key = directory / "webhook-key.pem"
        run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1", "-keyout", str(tls_key),
             "-out", str(certificate), "-subj", "/CN=localhost", "-addext", "subjectAltName=IP:127.0.0.1"])
        tls_key.chmod(0o600)
        webhook_port = free_port()
        account = pwd.getpwuid(os.geteuid()).pw_name
        run(["mariadb-install-db", "--no-defaults", "--datadir=" + str(datadir), "--auth-root-authentication-method=normal", "--skip-test-db", "--tmpdir=" + str(temporary)])
        datadir.chmod(0o700)
        db_socket = datadir / "db"
        pid_file = datadir / "server.pid"
        server_config = directory / "server.cnf"
        server_config.write_text("[mariadbd]\nuser=" + account + "\ninnodb-buffer-pool-size=32M\ntmpdir=" + str(temporary) + "\nlog-error=" + str(directory / "server.log") + "\n")
        server_command = ["/usr/sbin/mariadbd", "--defaults-file=" + str(server_config), "--datadir=" + str(datadir), "--socket=" + str(db_socket), "--pid-file=" + str(pid_file), "--skip-networking"]
        fixture = None
        application = None
        app_log = directory / "application.log"
        port = free_port()
        occupied = None
        if args.case == "webhook-failure":
            occupied = socket.socket()
            occupied.bind(("127.0.0.1", webhook_port))
            occupied.listen()
        try:
            with (directory / "fixture.log").open("w") as fixture_log:
                fixture = subprocess.Popen(server_command, stdout=fixture_log, stderr=subprocess.STDOUT)
                wait_sql(db_socket, fixture)
                query(db_socket, "CREATE DATABASE core_ci; CREATE USER 'core_ci_test'@'localhost' IDENTIFIED BY ''; GRANT SELECT,INSERT,UPDATE,DELETE ON core_ci.* TO 'core_ci_test'@'localhost';")

                schema = (Path(__file__).resolve().parent.parent / "sql/schema.sql").read_text()
                if args.case != "schema-failure":
                    query(db_socket, "USE core_ci; " + schema)

                managed = args.case in ("managed", "initial-failure", "startup-failure")
                if managed:
                    stop(fixture)
                name = "missing_schema" if args.case in ("initial-failure", "unmanaged-initial-failure") else "core_ci"
                command = [args.executable, "--config-file", str(config), "--log-file", str(directory / "framework.log"), "database", "--mode", "managed" if managed else "unmanaged", "--socket", str(db_socket), "--user", "core_ci_test", "--name", name, "--password-file", str(password)]
                if managed:
                    command += ["--managed-executable", "/bin/false" if args.case == "startup-failure" else "/usr/sbin/mariadbd", "--managed-config-file", str(server_config), "--managed-datadir", str(datadir), "--managed-pid-file", str(pid_file)]
                command += ["web", "local", "--port", str(port), "webhook", "--disabled=false", "local", "--port", str(webhook_port),
                            "tls", "--cert", str(certificate), "--cert-key", str(tls_key)]
                with app_log.open("w") as output:
                    application = subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT)
                    if args.case in ("initial-failure", "unmanaged-initial-failure", "startup-failure", "schema-failure"):
                        application.wait(timeout=10)
                        log = app_log.read_text()
                        assert "Listening on" not in log and "Webhooks listening on" not in log, log
                        for closed_port in (port, webhook_port):
                            with socket.socket() as probe:
                                assert probe.connect_ex(("127.0.0.1", closed_port)) != 0, "initial failure left a listener"
                        if args.case != "schema-failure":
                            assert "Initial MariaDB connection established" not in log, log
                        if managed:
                            assert not db_socket.exists(), "managed failure left its server running"
                        else:
                            assert fixture.poll() is None, "unmanaged initial failure stopped the external server"
                            assert "1" in query(db_socket, "SELECT 1")
                        assert "failed" in log or "exited" in log, log
                    else:
                        wait_http(port, application)
                        child_pid = int(pid_file.read_text())
                        if args.case == "sse-heartbeat":
                            origin = "http://127.0.0.1:" + str(port)
                            with event_stream(origin, timeout=35) as stream:
                                assert stream.headers.get_content_type() == "text/event-stream"
                                stream_event(stream)
                                stream_event(stream)
                                generation = json.load(urllib.request.urlopen(origin + "/api/graph"))["generation"]
                                started = time.monotonic()
                                for _ in range(3):
                                    assert stream.readline().strip() == b":keep-alive", "idle SSE stream closed or sent a data event"
                                    assert not stream.readline().strip(), "heartbeat was not a complete SSE comment"
                                    assert application.poll() is None, "controller stopped during idle streaming"
                                assert time.monotonic() - started > 60, "idle connection was not tested beyond its timeout"
                                assert json.load(urllib.request.urlopen(origin + "/api/graph"))["generation"] == generation
                            wait_http(port, application)
                        if args.case == "unmanaged":
                            connection_id = query(db_socket, "SELECT ID FROM information_schema.PROCESSLIST WHERE USER='core_ci_test' LIMIT 1").strip()
                            assert connection_id, "no application database connection"
                            query(db_socket, "KILL CONNECTION " + connection_id)
                            time.sleep(0.1)
                            wait_http(port, application)
                            assert app_log.read_text().count("Listening on") == 1, "reconnection registered a second listener"
                            assert app_log.read_text().count("Webhooks listening on") == 1, "reconnection registered a second TLS listener"
                        stop(application)
                        if args.case == "webhook-failure":
                            assert "Webhook listener failed" in app_log.read_text(), "no bind failure reported"
                        if managed:
                            gone = False
                            try:
                                os.kill(child_pid, 0)
                            except ProcessLookupError:
                                gone = True
                            assert gone, "managed server survived controller shutdown"
                        else:
                            assert fixture.poll() is None, "unmanaged controller stopped the external server"
                            assert "1" in query(db_socket, "SELECT 1")
        finally:
            if occupied is not None:
                occupied.close()
            stop(application)
            stop(fixture)
    print("PASS", args.case)



def event_stream(origin, timeout=10):
    request = urllib.request.Request(origin + "/api/events", headers={"Accept": "text/event-stream"})
    response = urllib.request.urlopen(request, timeout=timeout)
    assert response.headers.get_content_type() == "text/event-stream"
    return response


def stream_event(response):
    fields = {}
    line = response.readline()
    while line and line.strip():
        key, _, value = line.decode().partition(":")
        fields[key] = value.strip()
        line = response.readline()
    assert "event" in fields and "data" in fields, fields
    result = fields["event"], json.loads(fields["data"])
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True)
    parser.add_argument("--directory", required=True)
    parser.add_argument("--case", required=True)
    arguments = parser.parse_args()
    Path(arguments.directory).mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="run-", dir=arguments.directory) as temporary:
        arguments.directory = temporary
        try:
            exercise(arguments)
        except Exception:
            for filename in ("application.log", "server.log", "fixture.log"):
                report = Path(temporary) / filename
                if report.exists():
                    print(filename, report.read_text(errors="replace"))
            raise
