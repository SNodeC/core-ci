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
        assert "Initial MariaDB connection established" not in root.stdout + root.stderr
    else:
        datadir = directory / "data"
        assert not datadir.exists(), "use a clean task test directory"
        datadir.mkdir(mode=0o700)
        temporary = directory / "tmp"
        temporary.mkdir(mode=0o700)
        account = pwd.getpwuid(os.geteuid()).pw_name
        run(["mariadb-install-db", "--no-defaults", "--datadir=" + str(datadir), "--auth-root-authentication-method=normal", "--skip-test-db", "--tmpdir=" + str(temporary)])
        datadir.chmod(0o700)
        db_socket = datadir / "server.sock"
        pid_file = datadir / "server.pid"
        server_config = directory / "server.cnf"
        server_config.write_text("[mariadbd]\nuser=" + account + "\ninnodb-buffer-pool-size=32M\ntmpdir=" + str(temporary) + "\nlog-error=" + str(directory / "server.log") + "\n")
        server_command = ["/usr/sbin/mariadbd", "--defaults-file=" + str(server_config), "--datadir=" + str(datadir), "--socket=" + str(db_socket), "--pid-file=" + str(pid_file), "--skip-networking"]
        fixture = None
        application = None
        app_log = directory / "application.log"
        port = free_port()
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
                command += ["web", "local", "--port", str(port)]
                with app_log.open("w") as output:
                    application = subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT)
                    if args.case in ("initial-failure", "unmanaged-initial-failure", "startup-failure", "schema-failure"):
                        application.wait(timeout=10)
                        log = app_log.read_text()
                        assert "Listening on" not in log, log
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
                        if args.case == "unmanaged":
                            connection_id = query(db_socket, "SELECT ID FROM information_schema.PROCESSLIST WHERE USER='core_ci_test' LIMIT 1").strip()
                            assert connection_id, "no application database connection"
                            query(db_socket, "KILL CONNECTION " + connection_id)
                            time.sleep(0.1)
                            wait_http(port, application)
                            assert app_log.read_text().count("Listening on") == 1, "reconnection registered a second listener"
                        stop(application)
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
            stop(application)
            stop(fixture)
    print("PASS", args.case)


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
