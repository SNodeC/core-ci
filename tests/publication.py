"""Real execution/state/DB path, isolated from live GitHub and MariaDB."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time

from persistence import drop_commit_reply, run


def main():
    build = Path(sys.argv[1]).resolve()
    parent = Path('/tmp/codex/build/core-ci/publication-confirmation')
    parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='case-', dir=parent) as temporary:
        root = Path(temporary)
        backend = root / 'db.sock'
        run(['mariadb-install-db', '--no-defaults', f'--datadir={root / "data"}',
             '--auth-root-authentication-method=normal', '--skip-test-db'])
        with (root / 'server.log').open('w') as log:
            server = subprocess.Popen(['/usr/sbin/mariadbd', '--no-defaults', f'--datadir={root / "data"}',
                f'--socket={backend}', f'--pid-file={root / "pid"}', '--skip-networking', f'--user={os.geteuid()}'],
                stdout=log, stderr=log)
            try:
                ready = False
                deadline = time.monotonic() + 20
                while not ready and time.monotonic() < deadline and server.poll() is None:
                    ready = subprocess.run(['mariadb', '--no-defaults', f'--socket={backend}', '-uroot', '-e', 'SELECT 1'],
                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0
                    if not ready:
                        time.sleep(0.05)
                assert ready, (root / 'server.log').read_text()
                config = root / 'empty.conf'
                config.touch()
                binary = root / 'publication-test'
                binary.symlink_to(build / 'publication-contract-test')
                cases = ['queued', 'running', 'missing', 'skipped', 'failure', 'duplicate', 'publish_failed',
                         'wrong_job_run', 'wrong_job_attempt', 'wrong_actor', 'wrong_workflow', 'wrong_run_attempt',
                         'wrong_blob', 'missing_receipt', 'wrong_receipt', 'complete', 'duplicate_event', 'out_of_order', 'reject', 'unknown',
                         'resume_complete', 'resume_running', 'resume_unbound', 'resume_mixed', 'resume_race',
                         'resume_build', 'resume_build_running', 'resume_reject', 'resume_unknown']
                failures = []
                for number, case in enumerate(cases):
                    try:
                        database = f'publication_{number}'
                        run(['mariadb', '--no-defaults', f'--socket={backend}', '-uroot', '-e', f'CREATE DATABASE {database}'])
                        env = dict(os.environ, CI_PUBLICATION_CASE=case, CI_TEST_DATABASE=database,
                                   CI_TEST_SOCKET=str(backend), CI_TEST_BACKEND=str(backend))
                        command = [str(binary), '--config-file', str(config), '--log-file', str(root / f'{case}.log'), '--quiet=true']
                        if case in ['reject', 'unknown', 'resume_reject', 'resume_unknown']:
                            env['CI_PUBLICATION_CASE'] = 'running'
                            print(run(command, env=env).strip())
                            env.update(CI_PUBLICATION_CASE=case, CI_PUBLICATION_PREPARED='1')
                            if case.endswith('reject'):
                                run(['mariadb', '--no-defaults', f'--socket={backend}', '-uroot', database, '-e',
                                     "CREATE TRIGGER reject_update BEFORE UPDATE ON ci_graph FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='isolated rejection'"])
                            else:
                                front = root / f'{case}-proxy.sock'
                                proxy = threading.Thread(target=drop_commit_reply, args=(front, backend), daemon=True)
                                proxy.start()
                                while not front.exists():
                                    time.sleep(0.01)
                                env['CI_TEST_SOCKET'] = str(front)
                        print(run(command, env=env).strip())
                        if case.endswith('unknown'):
                            proxy.join(5)
                            assert not proxy.is_alive()
                            env.update(CI_TEST_SOCKET=str(backend), CI_PUBLICATION_CASE='resume_again' if case == 'resume_unknown' else 'restart')
                            print(run(command, env=env).strip())
                        if case == 'resume_complete':
                            env.update(CI_PUBLICATION_CASE='resume_again', CI_PUBLICATION_PREPARED='1')
                            print(run(command, env=env).strip())
                    except Exception as error:
                        failures.append(f'{case}: {error}')
                        print(f'FAILED {case}: {error}', flush=True)
                assert not failures, '\n'.join(failures)
                print('Publication confirmation: actual observation, authenticated identities, exact release, duplicates, rejected/unknown commits, restart passed')
            finally:
                server.terminate()
                server.wait(timeout=20)


if __name__ == '__main__':
    main()
