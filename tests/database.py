"""Disposable socket-only MariaDB and packet-boundary fault injection."""
from contextlib import contextmanager
import os
from pathlib import Path
import pwd
import select
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time


def eventually(predicate, seconds=15):
    deadline = time.monotonic() + seconds
    value = False
    while not value and time.monotonic() < deadline:
        try:
            value = predicate()
        except (OSError, ValueError):
            pass
        if not value:
            time.sleep(.05)
    return value


class Proxy:
    def __init__(self, endpoint):
        self.endpoint = endpoint
        self.listener = socket.socket()
        self.listener.bind(('127.0.0.1', 0))
        self.listener.listen()
        self.port = self.listener.getsockname()[1]
        self.rule = None
        self.fired = threading.Event()
        self.stop = threading.Event()
        self.connections = 0
        self.thread = threading.Thread(target=self.accept, daemon=True)
        self.thread.start()

    def arm(self, needle, boundary):
        self.fired.clear()
        self.rule = (needle, boundary)

    def accept(self):
        self.listener.settimeout(.2)
        while not self.stop.is_set():
            try:
                client, _ = self.listener.accept()
                self.connections += 1
                threading.Thread(target=self.forward, args=(client,), daemon=True).start()
            except socket.timeout:
                pass

    def forward(self, client):
        upstream = socket.socket(socket.AF_UNIX)
        buffers = {client: b'', upstream: b''}
        query = ''
        try:
            upstream.connect(str(self.endpoint))
            alive = True
            while alive and not self.stop.is_set():
                ready, _, _ = select.select([client, upstream], [], [], .2)
                for source in ready:
                    incoming = source.recv(65536)
                    if incoming:
                        buffers[source] += incoming
                        while alive and len(buffers[source]) >= 4 and len(buffers[source]) >= 4 + int.from_bytes(buffers[source][:3], 'little'):
                            size = 4 + int.from_bytes(buffers[source][:3], 'little')
                            packet, buffers[source] = buffers[source][:size], buffers[source][size:]
                            if source is client and packet[4:5] == b'\x03':
                                query = packet[5:].decode(errors='replace')
                            needle, boundary = self.rule or ('', '')
                            matched = bool(needle) and (query == needle[1:] if needle.startswith('=') else needle in query)
                            cut = matched and ((source is client and boundary == 'before') or
                                (source is upstream and (boundary == 'read' or
                                 (boundary == 'after' and packet[4:5] == b'\x00'))))
                            if matched and source is client and boundary == 'slow':
                                query = query.replace('FROM coreci_targets ORDER BY',
                                    'FROM coreci_targets JOIN (SELECT SLEEP(3)) AS load_delay ORDER BY')
                                payload = b'\x03' + query.encode()
                                packet = len(payload).to_bytes(3, 'little') + packet[3:4] + payload
                                self.rule = None
                                self.fired.set()
                            if cut:
                                self.rule = None
                                self.fired.set()
                                alive = False
                            else:
                                (upstream if source is client else client).sendall(packet)
                    else:
                        alive = False
        except OSError:
            pass
        finally:
            upstream.close()
            client.close()

    def close(self):
        self.stop.set()
        self.thread.join(timeout=2)
        self.listener.close()


@contextmanager
def database(build):
    root = Path(build) / 'tmp'
    root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='mdb-', dir=root) as directory:
        work = Path(directory)
        endpoint = work / 's'
        account = pwd.getpwuid(os.getuid()).pw_name
        server = proxy = None
        try:
            with (work / 'initialize.log').open('w') as output:
                subprocess.run(['mariadb-install-db', '--no-defaults', '--datadir=' + str(work / 'data'),
                    '--tmpdir=' + str(work), '--auth-root-authentication-method=socket',
                    '--auth-root-socket-user=' + account, '--skip-test-db'], check=True,
                    stdout=output, stderr=subprocess.STDOUT, timeout=40)
            executable = shutil.which('mariadbd') or '/usr/sbin/mariadbd'
            with (work / 'process.log').open('w') as output:
                server = subprocess.Popen([executable, '--no-defaults', '--user=' + account,
                    '--datadir=' + str(work / 'data'), '--tmpdir=' + str(work), '--socket=' + str(endpoint),
                    '--pid-file=' + str(work / 'server.pid'), '--log-error=' + str(work / 'server.log'),
                    '--skip-networking', '--log-warnings=2', '--innodb-buffer-pool-size=64M',
                    '--innodb-log-file-size=16M'], stdout=output, stderr=subprocess.STDOUT)
            cli = ['mariadb', '--no-defaults', '--socket=' + str(endpoint), '-u', account, '-N', '-B']
            assert eventually(lambda: subprocess.run(cli + ['-e', 'SELECT 1'], stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL).returncode == 0), 'disposable MariaDB did not start'
            subprocess.run(cli + ['-e', "CREATE DATABASE probe; CREATE USER 'coreci_test'@'localhost'; GRANT ALL ON probe.* TO 'coreci_test'@'localhost'"], check=True)
            proxy = Proxy(endpoint)
            environment = dict(os.environ, CORECI_TEST_DB_HOST='127.0.0.1', CORECI_TEST_DB_PORT=str(proxy.port),
                CORECI_TEST_DB_USER='coreci_test', CORECI_TEST_DB_NAME='probe', CORECI_TEST_DB_PASSWORD='',
                PYTHONDONTWRITEBYTECODE='1')
            yield proxy, cli, environment, work
        finally:
            if proxy is not None:
                proxy.close()
            if server is not None:
                server.terminate()
                server.wait(timeout=15)


def main():
    build = Path(sys.argv[1]).resolve()
    with database(build) as (_, _, environment, _):
        result = subprocess.run(['ctest', '--test-dir', str(build), '--output-on-failure', *sys.argv[2:]],
                                env=environment).returncode
    return result


if __name__ == '__main__':
    sys.exit(main())
