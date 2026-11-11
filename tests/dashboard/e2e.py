import json
import os
from pathlib import Path
import subprocess
import sys
import time
from fixture import fixture

result = 77
if all(os.environ.get(key) for key in ('CORECI_TEST_DB_HOST', 'CORECI_TEST_DB_USER', 'CORECI_TEST_DB_NAME')):
    output = Path(sys.argv[1]) / 'dashboard-evidence/e2e'
    output.mkdir(parents=True, exist_ok=True)
    with fixture(sys.argv[1]) as fx:
        fx.ready()
        browser = subprocess.Popen(['node', str(Path(__file__).with_suffix('.cjs')), fx.app, str(output)],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
        def command(action, **values):
            browser.stdin.write(json.dumps(dict(action=action, **values)) + '\n')
            browser.stdin.flush()
            line = browser.stdout.readline()
            assert line, 'browser process ended unexpectedly'
            response = json.loads(line)
            assert response['ok'], response.get('error')
        def assigned(target, phase, revision=None):
            deadline = time.monotonic() + 15
            row = fx.dispatches.get(timeout=15)
            while (row[1]['target'] != target or row[2] != phase or (revision and int(row[1]['revision']) != revision)) and time.monotonic() < deadline:
                row = fx.dispatches.get(timeout=15)
            assert row[1]['target'] == target and row[2] == phase
            return row
        try:
            command('login')
            command('save', name='01-empty')
            assert fx.webhook('SNodeC/snode.c') == 200
            rid, values, phase = fx.dispatches.get(timeout=15)
            target = values['target']
            fx.report(rid, values, 'build', 'started')
            command('wait', target=target, state='building')
            command('focus', target=target)
            command('save', name='02-building')
            fx.report(rid, values, 'build', 'failed')
            command('wait', target=target, state='failed')
            command('save', name='03-failed')
            command('retry')
            rid, values, phase = assigned(target, 'build', int(values['revision']) + 1)
            fx.report(rid, values, 'build', 'started')
            command('wait', target=target, state='building')
            command('save', name='04-retry')
            fx.report(rid, values, 'build', 'succeeded')
            publish_id, publish_values, _ = assigned(target, 'publish')
            command('wait', target=target, state='publishing')
            command('save', name='05-publishing')
            fx.report(publish_id, publish_values, 'publish', 'succeeded')
            command('wait', target=target, state='published', job='completed')
            command('save', name='06-published')
            command('refresh')
            command('save', name='07-refresh')
            fx.token_lifetime = 10
            command('expiring-login')
            fx.token_lifetime = 3600
            command('wait-disconnected')
            assert fx.webhook('SNodeC/snode.c', 'v1.0.99') == 200
            issued = len(fx.tokens)
            command('reconnect')
            assert len(fx.tokens) > issued, 'Reconnect must obtain a newly authorized token after expiry'
            command('wait', target=target, state='building')
            command('save', name='08-reauthenticated-stream')
            command('finish')
            browser.stdin.close()
            assert browser.wait(timeout=10) == 0
            console = json.loads((output / 'console.json').read_text())
            assert not console, console
            print('S6: 178 S5 units + S3 registry/catalog; OIDC login, build/Retry/publish/Refresh, token expiry, Reconnect reauthorization and fresh committed state; eight screenshots; no unexpected console errors')
        finally:
            if browser.poll() is None:
                browser.terminate()
                try:
                    browser.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    browser.kill()
                    browser.wait(timeout=10)
    result = 0
raise SystemExit(result)
