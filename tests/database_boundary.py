"""Exercise the fresh database boundary using isolated real MariaDB, not GitHub."""

import argparse
import os
from pathlib import Path
import pwd
import subprocess
import threading
import tempfile

import lifecycle
from enrollment import DatabaseProxy


def exercise(arguments):
    root = Path(arguments.directory)
    root.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="r-", dir=root))
    data = root / "data"
    temporary = root / "tmp"
    data.mkdir(mode=0o700)
    temporary.mkdir(mode=0o700)
    lifecycle.run([
        "mariadb-install-db", "--no-defaults", "--datadir=" + str(data),
        "--tmpdir=" + str(temporary), "--auth-root-authentication-method=normal", "--skip-test-db",
    ])
    data.chmod(0o700)
    server = None
    proxy = None
    thread = None
    socket = root / "db.sock"
    config = root / "controller.conf"
    config.write_text("")
    try:
        with (root / "server.log").open("w") as output:
            server = subprocess.Popen([
                "/usr/sbin/mariadbd", "--no-defaults", "--datadir=" + str(data),
                "--tmpdir=" + str(temporary), "--socket=" + str(socket),
                "--pid-file=" + str(root / "server.pid"), "--skip-networking",
                "--user=" + pwd.getpwuid(os.geteuid()).pw_name, "--innodb-buffer-pool-size=32M",
            ], stdout=output, stderr=subprocess.STDOUT)
            lifecycle.wait_sql(socket, server)
            lifecycle.query(socket, "CREATE DATABASE boundary; CREATE TABLE boundary.probe(revision INT NOT NULL); "
                            "INSERT INTO boundary.probe VALUES(0)")
            application_socket = socket
            if arguments.case == "lost-ack":
                application_socket = root / "proxy.sock"
                proxy = DatabaseProxy(application_socket, socket)
                proxy.drop_commit = True
                proxy.drop_query = b"UPDATE PROBE"
                thread = threading.Thread(target=proxy.serve_forever, daemon=True)
                thread.start()
            modes = ("apply", "restore") if arguments.case == "graph" else ("missing",) if arguments.case == "graph-missing" else (arguments.case,)
            if arguments.case == "graph":
                schema = (Path(__file__).resolve().parents[1] / "sql/schema.sql").read_text()
                lifecycle.query(socket, "USE boundary; " + schema)
            for mode in modes:
                result = subprocess.run([
                    arguments.executable, "--config-file", str(config), "probe", "--socket", str(application_socket),
                    "--case", mode,
                ], capture_output=True, text=True, timeout=20)
                (root / (mode + ".log")).write_text(result.stdout + result.stderr)
                assert result.returncode == 0, result.stdout + result.stderr
                assert "PASS " + mode in result.stdout, result.stdout + result.stderr
            if arguments.case == "graph":
                assert lifecycle.query(socket, "SELECT JSON_VALUE(document,'$.generation') FROM boundary.graph_state").strip() == "13"
            elif not arguments.case.startswith("graph"):
                expected = "13" if arguments.case == "writes" else "1"
                assert lifecycle.query(socket, "SELECT revision FROM boundary.probe").strip() == expected
                if arguments.case == "lost-ack":
                    assert proxy.dropped == 1, "actual successful atomic write acknowledgement was not intercepted"
    finally:
        if proxy:
            proxy.shutdown()
            proxy.server_close()
        if thread:
            thread.join(timeout=3)
        lifecycle.stop(server)
    print("PASS isolated MariaDB:", arguments.case)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", required=True)
    parser.add_argument("--executable", required=True)
    parser.add_argument("--case", required=True, choices=("writes", "failures", "lost-ack", "graph", "graph-missing"))
    exercise(parser.parse_args())
