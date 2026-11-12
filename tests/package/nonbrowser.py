"""Exercise the target build with a disposable database and no browser runtime."""
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time

build = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(dir=build, prefix='runtime-') as directory:
    runtime = Path(directory)
    for name in ('config', 'state', 'run'):
        (runtime / name).mkdir(mode=0o700)
    with socket.socket() as probe:
        probe.bind(('127.0.0.1', 0))
        port = probe.getsockname()[1]
    environment = dict(os.environ, XDG_CONFIG_HOME=str(runtime / 'config'), XDG_STATE_HOME=str(runtime / 'state'),
                       XDG_RUNTIME_DIR=str(runtime / 'run'), CORECI_TEST_DB_HOST='127.0.0.1',
                       CORECI_TEST_DB_PORT=str(port), CORECI_TEST_DB_USER='test', CORECI_TEST_DB_NAME='test',
                       CORECI_TEST_DB_PASSWORD='', PYTHONDONTWRITEBYTECODE='1')
    endpoint = str(runtime / 's')
    subprocess.run(['mariadb-install-db', '--no-defaults', '--datadir=' + str(runtime / 'db'),
                    '--auth-root-authentication-method=normal', '--skip-test-db'], check=True)
    server = subprocess.Popen(['/usr/sbin/mariadbd', '--no-defaults', '--datadir=' + str(runtime / 'db'),
                               '--socket=' + endpoint, '--pid-file=' + str(runtime / 'pid'), '--bind-address=127.0.0.1',
                               '--port=' + str(port), '--log-error=' + str(runtime / 'server.log')])
    admin = ['mariadb-admin', '--no-defaults', '--protocol=socket', '--socket=' + endpoint, '-u', 'root']
    try:
        deadline = time.monotonic() + 30
        ready = False
        while not ready and server.poll() is None and time.monotonic() < deadline:
            ready = subprocess.run(admin + ['ping', '--silent'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0
            if not ready:
                time.sleep(0.1)
        assert ready, 'disposable database did not become ready'
        subprocess.run(['mariadb', '--no-defaults', '--protocol=socket', '--socket=' + endpoint, '-u', 'root', '-e',
                        "CREATE DATABASE test; CREATE USER 'test'@'127.0.0.1'; GRANT ALL ON test.* TO 'test'@'127.0.0.1';"], check=True)
        # M_DashboardEndToEnd launches Chromium despite its integration label;
        # its browser interactions are validated separately on the host.
        subprocess.run(['ctest', '--test-dir', str(build), '--output-on-failure', '-LE', 'browser',
                        '-E', '^M_DashboardEndToEnd$', '--output-junit', str(build / 'nonbrowser.xml')],
                       env=environment, check=True)
        import xml.etree.ElementTree as ET
        cases = list(ET.parse(build / 'nonbrowser.xml').getroot().iter('testcase'))
        assert cases and all(case.find('skipped') is None for case in cases), 'target test skipped'
    finally:
        subprocess.run(admin + ['shutdown'], check=True)
        server.wait(timeout=30)
        assert server.returncode == 0
