"""Actual daemon regressions: uncertain state transactions, direct loading, and teardown."""
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
        def sql(statement):
            return subprocess.check_output(cli + ['probe', '-e', statement], text=True).strip()
        def restart():
            fx.process = subprocess.Popen(fx.command, env=fx.env, stdout=fx.log, stderr=subprocess.STDOUT)
        def status():
            response = fx.request('/api/v1/status.json', browser=False)
            return json.loads(response.read()) if response.status == 200 else False
        try:
            fx.ready()
            if mode == 'restart-dispatch':
                assert fx.webhook('owner/a') == 200
                original, values = fx.dispatches.get(timeout=10)
                frozen = fx.plan(values)
                fx.process.kill()
                fx.process.wait(timeout=10)
                sql("UPDATE coreci_waves SET accepted=0,attempt=2,dispatched_at=deadline-1099,start_deadline=deadline-199; "
                    "UPDATE coreci_state SET clock=(SELECT dispatched_at FROM coreci_waves LIMIT 1)")
                fx.advance(901)
                restart()
                owner, retried = fx.dispatches.get(timeout=10)
                assert owner != original and retried == values, 'startup did not resend the stored pending wave'
                assert fx.plan(values) == frozen, 'startup changed the frozen plan'
                assert eventually(lambda: sql("SELECT attempt FROM coreci_waves LIMIT 1") == '2')
                fx.complete(owner, values)
            elif mode == 'publication-dispatch':
                assert fx.webhook('owner/a') == 200
                owner, values = fx.dispatches.get(timeout=10)
                fx.started(owner, values)
                target = fx.plan(values)['targets'][0]
                for entry in target['units']:
                    fx.unit(values, entry)
                fx.dispatch_response.clear()
                fx.target(values, target['id'])
                first, publication = fx.dispatches.get(timeout=10)
                frozen = fx.plan(publication)
                fx.process.kill()
                fx.process.wait(timeout=10)
                fx.dispatch_response.set()
                restart()
                second, retried = fx.dispatches.get(timeout=10)
                assert first != second and retried == publication
                assert fx.plan(publication) == frozen, 'restart changed the pending publication'
                fx.started(first, publication)
                fx.started(second, publication)
                assert fx.plan(publication)['owner_run_id'] == first
                for item in frozen['items']:
                    for entry in item['units']:
                        fx.unit(publication, entry, 'publish')
                fx.finished(first, publication)
                fx.finished(owner, values)
                assert all(unit['published']['revision'] == 1 for unit in status()['units']
                           if unit['package'] == 'a' and unit['target'] == target['id'])
            elif mode in ('load', 'slow', 'shutdown', 'shutdown-idle', 'shutdown-int'):
                fx.process.kill()
                fx.process.wait(timeout=10)
                sql("INSERT INTO coreci_log(seq,logged_at,description) SELECT 1000000+seq,0,'display only' FROM seq_1_to_10000")
                proxy.arm('FROM coreci_targets ORDER BY', 'read' if mode == 'load' else 'slow')
                started = time.monotonic()
                restart()
                assert proxy.fired.wait(10), 'startup state load was not reached'
                if mode in ('shutdown', 'shutdown-idle', 'shutdown-int'):
                    if mode == 'shutdown-idle':
                        assert eventually(status), 'populated state did not become live'
                    connections = proxy.connections
                    before = sql('SELECT COUNT(*) FROM coreci_log')
                    fx.process.send_signal(signal.SIGINT if mode == 'shutdown-int' else signal.SIGTERM)
                    assert fx.process.wait(timeout=10) == 0
                    assert proxy.connections == connections, 'shutdown reopened the database'
                    assert before == sql('SELECT COUNT(*) FROM coreci_log'), 'shutdown wrote new state'
                else:
                    assert eventually(status), 'startup failed to recover'
                    if mode == 'slow':
                        assert time.monotonic() - started >= 3, 'slow state query was not exercised'
                    assert fx.webhook('owner/a') == 200
                    owner, values = fx.dispatches.get(timeout=10)
                    fx.complete(owner, values)
            else:
                if mode == 'append':
                    proxy.arm('=COMMIT', 'after')
                    assert fx.webhook('owner/a') == 503, 'uncertain transaction was acknowledged'
                    assert proxy.fired.wait(10), 'commit fault boundary was not reached'
                    owner, values = fx.dispatches.get(timeout=10)
                else:
                    fx.dispatch_response.clear()
                    assert fx.webhook('owner/a') == 200
                    owner, values = fx.dispatches.get(timeout=10)
                    proxy.arm('=COMMIT', 'before' if mode == 'result-before' else 'after')
                    fx.dispatch_response.set()
                    assert proxy.fired.wait(10), 'dispatch-result commit fault boundary was not reached'
                    if mode == 'result-before':
                        duplicate, retried = fx.dispatches.get(timeout=10)
                        assert duplicate != owner and retried == values, 'recovery changed the wave identity'
                assert eventually(status), 'transaction recovery never became live'
                assert sql('SELECT COUNT(*) FROM coreci_waves') == '1'
                assert sql('SELECT MAX(revision) FROM coreci_revisions') == '1'
                fx.complete(owner, values)
                value = status()
                assert value['wave']['phase'] == 'Finished' and value['wave']['runId'] == owner
                assert all(unit['status'] == 'Published' for unit in value['units'] if unit['package'] == 'a')
                time.sleep(.1)
                assert fx.dispatches.empty(), 'resolved attempt was dispatched again'
            fx.log.flush()
            log = (work / 'daemon.log').read_text(errors='replace')
            assert 'MariaDB cancellation failed' not in log
            if mode in ('slow', 'shutdown', 'shutdown-idle', 'shutdown-int'):
                assert 'Lost connection' not in log
            print(mode + ': direct-state recovery/ownership verified')
        except Exception:
            fx.log.flush()
            log = (work / 'daemon.log').read_text(errors='replace')
            print('\n'.join(line for line in log.splitlines() if any(word in line for word in
                ('db.mariadb', 'store unavailable', 'cancel', 'dispatch'))), file=sys.stderr)
            raise
        finally:
            fx.close()


if __name__ == '__main__':
    run(Path(sys.argv[1]).resolve(), sys.argv[2])
