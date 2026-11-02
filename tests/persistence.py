"""Isolated native MariaDB boundary tests. Never connects to a configured server."""
import os
from pathlib import Path
import selectors
import socket
import subprocess
import sys
import tempfile
import threading
import time


def run(command, **kwargs):
    result = subprocess.run(command, text=True, capture_output=True, timeout=45, **kwargs)
    if result.returncode:
        raise RuntimeError(f"{command[0]} exited {result.returncode}: {result.stdout}\n{result.stderr}")
    return result.stdout


def drop_commit_reply(front, backend):
    """Forward the real MariaDB protocol, discard only UPDATE's server response."""
    listener = socket.socket(socket.AF_UNIX)
    listener.bind(str(front))
    listener.listen(1)
    dropped = False
    while not dropped:
        client, _ = listener.accept()
        server = socket.socket(socket.AF_UNIX)
        server.connect(str(backend))
        pending = {client: b"", server: b""}
        selector = selectors.DefaultSelector()
        selector.register(client, selectors.EVENT_READ)
        selector.register(server, selectors.EVENT_READ)
        updating = False
        running = True
        while running:
            for key, _ in selector.select(20):
                source = key.fileobj
                data = source.recv(1024 * 1024)
                if not data:
                    running = False
                else:
                    pending[source] += data
                    complete = True
                    while complete and len(pending[source]) >= 4:
                        length = int.from_bytes(pending[source][:3], "little") + 4
                        complete = len(pending[source]) >= length
                        if complete:
                            packet, pending[source] = pending[source][:length], pending[source][length:]
                            if source is client and packet[4:5] == b"\x03" and packet[5:].startswith(b"UPDATE ci_graph"):
                                updating = True
                            if source is server and updating:
                                assert packet[4:5] == b"\x00", "Only an actual committed OK acknowledgement may be discarded"
                                running = False
                                dropped = True
                            else:
                                (server if source is client else client).sendall(packet)
        selector.close()
        client.close()
        server.close()
    listener.close()


def main():
    build = Path(sys.argv[1]).resolve()
    parent = Path("/tmp/codex/build/core-ci/persistence")
    parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="case-", dir=parent) as temporary:
        root = Path(temporary)
        backend = root / "db.sock"
        datadir = root / "data"
        run(["mariadb-install-db", "--no-defaults", f"--datadir={datadir}",
             "--auth-root-authentication-method=normal", "--skip-test-db"])
        with (root / "server.log").open("w") as log:
            server = subprocess.Popen(["/usr/sbin/mariadbd", "--no-defaults", f"--datadir={datadir}",
                f"--socket={backend}", f"--pid-file={root / 'server.pid'}", "--skip-networking",
                f"--user={os.geteuid()}"], stdout=log, stderr=log)
            try:
                ready = False
                deadline = time.monotonic() + 20
                while not ready and time.monotonic() < deadline and server.poll() is None:
                    ready = subprocess.run(["mariadb", "--no-defaults", f"--socket={backend}", "-uroot", "-e", "SELECT 1"],
                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0
                    if not ready:
                        time.sleep(0.05)
                assert ready, (root / "server.log").read_text()
                run(["mariadb", "--no-defaults", f"--socket={backend}", "-uroot", "-e",
                     "CREATE DATABASE race; CREATE DATABASE rejection; CREATE DATABASE unknown_commit; CREATE DATABASE projection; CREATE DATABASE reject_projection; CREATE DATABASE unknown_projection; CREATE DATABASE internal;"])
                config = root / "empty.conf"
                config.touch()
                binary = root / "isolated-store-test"
                binary.symlink_to(build / "store-contract-test")
                for mode, database in [("race", "race"), ("restart", "race"), ("reject", "rejection"),
                                       ("unknown", "unknown_commit"), ("restart", "unknown_commit"), ("projection", "projection"), ("reject_projection", "reject_projection"), ("unknown_projection", "unknown_projection"), ("internal", "internal")]:
                    front = backend
                    proxy = None
                    if mode.startswith("unknown"):
                        front = root / f"{mode}-proxy.sock"
                        proxy = threading.Thread(target=drop_commit_reply, args=(front, backend), daemon=True)
                        proxy.start()
                        while not front.exists():
                            time.sleep(0.01)
                    environment = dict(os.environ, CI_TEST_MODE=mode, CI_TEST_SOCKET=str(front),
                                       CI_TEST_BACKEND=str(backend), CI_TEST_DATABASE=database)
                    print(run([str(binary), "--config-file", str(config), "--log-file", str(root / f"{database}-{mode}.log"),
                               "--quiet=true"], env=environment).strip())
                    if proxy:
                        proxy.join(5)
                        assert not proxy.is_alive(), "Commit-loss proxy must terminate"
                print("Real MariaDB: committed-before-dispatch, duplicate CAS, rejection, lost commit acknowledgement, restart/no replay passed")
            finally:
                server.terminate()
                server.wait(timeout=20)


if __name__ == "__main__":
    main()
