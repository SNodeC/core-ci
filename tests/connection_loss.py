"""Actual daemon regressions: uncertain appends, startup replay, and teardown."""
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time
from unittest.mock import patch

from database import database, eventually

sys.path.insert(0, str(Path(__file__).parent / 'dashboard'))
from fixture import Fixture


def run(build, mode):
    with database(build) as (proxy, cli, environment, server_work), patch.dict(os.environ, environment):
        work = server_work / 'controller'
        work.mkdir()
        fx = Fixture(build, work, small=True, virtual_clock=mode == 'restart-dispatch', wave_deadline=2000)
        try:
            fx.ready()
            if mode == 'restart-dispatch':
                assert fx.webhook('owner/a') == 200
                original, values = fx.dispatches.get(timeout=10)
                frozen = fx.plan(values)
                fx.process.kill()
                fx.process.wait(timeout=10)
                # Persist the exact unresolved-attempt state, including an old
                # Resumed that already advanced attempt 2 without sending it.
                subprocess.run(cli + ['probe', '-e', "DELETE FROM coreci_events WHERE event_type='WaveDispatchAccepted'; INSERT INTO coreci_events(recorded_at,event_type,event_at) SELECT UTC_TIMESTAMP(3),'Resumed',MAX(event_at)+901 FROM coreci_events"], check=True)
                fx.advance(901)
                fx.process = subprocess.Popen(fx.command, env=fx.env, stdout=fx.log, stderr=subprocess.STDOUT)
                owner, retried = fx.dispatches.get(timeout=10)
                assert owner != original and retried == values, 'startup did not resend the persisted pending wave'
                assert fx.plan(values) == frozen, 'startup changed the frozen plan'
                assert eventually(lambda: subprocess.check_output(cli + ['probe', '-e', "SELECT attempt FROM coreci_events WHERE event_type='WaveDispatchAccepted' ORDER BY seq DESC LIMIT 1"], text=True).strip() == '2'), 'startup changed the pending attempt'
                fx.complete(owner, values)
            elif mode in ('replay', 'slow', 'shutdown', 'shutdown-idle', 'shutdown-int'):
                fx.process.kill()
                fx.process.wait(timeout=10)
                subprocess.run(cli + ['probe', '-e', "INSERT INTO coreci_events(recorded_at,event_type,event_at) SELECT UTC_TIMESTAMP(3),'Resumed',UNIX_TIMESTAMP()-10 FROM seq_1_to_10000"], check=True)
                proxy.arm('SELECT seq,HEX(event_type)', 'read' if mode == 'replay' else 'slow')
                started = time.monotonic()
                fx.process = subprocess.Popen(fx.command, env=fx.env, stdout=fx.log, stderr=subprocess.STDOUT)
                assert proxy.fired.wait(10), 'startup replay was not reached'
                def ready():
                    response = fx.request('/api/v1/status.json', browser=False)
                    return response.status == 200
                if mode in ('shutdown', 'shutdown-idle', 'shutdown-int'):
                    if mode == 'shutdown-idle':
                        assert eventually(ready), 'populated replay did not become live'
                    connections = proxy.connections
                    before = subprocess.check_output(cli + ['probe', '-e', 'SELECT COUNT(*) FROM coreci_events'], text=True)
                    fx.process.send_signal(signal.SIGINT if mode == 'shutdown-int' else signal.SIGTERM)
                    status = fx.process.wait(timeout=10)
                    assert status == 0, 'normal stop exited with status ' + str(status)
                    print('shutdown database connections: before=' + str(connections) + ', after=' + str(proxy.connections))
                    assert proxy.connections == connections, 'shutdown reopened the database through a recovery callback'
                    after = subprocess.check_output(cli + ['probe', '-e', 'SELECT COUNT(*) FROM coreci_events'], text=True)
                    assert before == after, 'shutdown persisted new recovery events'
                else:
                    assert eventually(ready), 'startup failed to recover'
                    if mode == 'slow':
                        assert time.monotonic() - started >= 3, 'long-running replay query was not exercised'
                    assert fx.webhook('owner/a') == 200
                    owner, values = fx.dispatches.get(timeout=10)
                    fx.complete(owner, values)
            else:
                event = 'Tick' if mode == 'append' else 'WaveDispatchAccepted'
                proxy.arm("X'" + event.encode().hex() + "'", 'before' if mode == 'result-before' else 'after')
                assert fx.webhook('owner/a') == 200
                assert proxy.fired.wait(10), 'append fault boundary was not reached'
                owner, values = fx.dispatches.get(timeout=10)
                if mode == 'result-before':
                    duplicate, retried = fx.dispatches.get(timeout=10)
                    assert duplicate != owner and retried == values, 'recovery changed the wave identity or frozen inputs'
                def recovered():
                    response = fx.request('/api/v1/status.json', browser=False)
                    return json.loads(response.read()) if response.status == 200 else False
                assert eventually(recovered), 'append recovery never became live'
                rows = subprocess.check_output(cli + ['probe', '-e', 'SELECT seq,event_type,attempt FROM coreci_events ORDER BY seq'], text=True)
                assert '\tTick\t' in rows and rows.count('\tResumed\t') >= 2, 'committed history was not reread'
                fx.complete(owner, values)
                status = json.loads(fx.request('/api/v1/status.json', browser=False).read())
                assert status['wave']['phase'] == 'Finished' and status['wave']['runId'] == owner
                assert all(unit['status'] == 'Published' for unit in status['units'] if unit['package'] == 'a')
                time.sleep(.1)
                assert fx.dispatches.empty(), 'resolved attempt was dispatched again'
            fx.log.flush()
            before_close = (work / 'daemon.log').read_text(errors='replace')
            assert 'MariaDB cancellation failed' not in before_close, 'shutdown misclassified as cancellation failure'
            if mode in ('slow', 'shutdown', 'shutdown-idle', 'shutdown-int'):
                assert 'Lost connection' not in before_close, 'slow replay or framework shutdown misclassified as connection loss'
            print(mode + ': daemon recovery/ownership verified')
        except Exception:
            fx.log.flush()
            log = (work / 'daemon.log').read_text(errors='replace')
            print('\n'.join(line for line in log.splitlines() if any(word in line for word in
                ('db.mariadb', 'store unavailable', 'cancel', 'dispatch', 'replay ignored'))), file=sys.stderr)
            server_log = (server_work / 'server.log').read_text(errors='replace')
            print('\n'.join(line for line in server_log.splitlines() if 'Aborted connection' in line), file=sys.stderr)
            raise
        finally:
            fx.close()


if __name__ == '__main__':
    run(Path(sys.argv[1]).resolve(), sys.argv[2])
