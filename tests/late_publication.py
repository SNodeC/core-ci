"""Late owner facts survive deadline expiry/replay and feed the next plan."""
import json
import os
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).parent / 'dashboard'))
from fixture import fixture
from database import eventually


def sql(statement):
    command = ['mariadb', '--no-defaults', '--skip-ssl', '--host=' + os.environ['CORECI_TEST_DB_HOST'],
               '--port=' + os.environ.get('CORECI_TEST_DB_PORT', '3306'),
               '--user=' + os.environ['CORECI_TEST_DB_USER'], '--database=' + os.environ['CORECI_TEST_DB_NAME'],
               '-NB', '-e', statement]
    return subprocess.check_output(command, env=dict(os.environ, MYSQL_PWD=os.environ.get('CORECI_TEST_DB_PASSWORD', '')),
                                   text=True).strip()


def restart(fx):
    fx.process = subprocess.Popen(fx.command, env=fx.env, stdout=fx.log, stderr=subprocess.STDOUT)
    assert eventually(lambda: fx.request('/api/v1/status.json', browser=False).status == 200)


def run(build):
    with fixture(build, small=True, virtual_clock=True, wave_deadline=60) as fx:
        fx.ready()
        # Simulate real schema 5 and an interrupted additive upgrade. Existing
        # startup records have no allocations; their common metadata was unused.
        for column_present in (False, True):
            fx.process.terminate()
            assert fx.process.wait(timeout=10) == 0
            if not column_present:
                sql('ALTER TABLE coreci_events DROP COLUMN dispatch_deadline')
            sql('UPDATE coreci_schema SET version=5')
            maximum = sql('SELECT MAX(seq) FROM coreci_events')
            columns = ('seq,recorded_at,event_type,package,target,tag,source_sha,origin,build_id,wave_id,http_status,'
                       'message,run_id,run_attempt,attempt,artifact,feed_commit,reason,conclusion,event_at')
            rows = sql(f'SELECT {columns} FROM coreci_events WHERE seq<={maximum} ORDER BY seq')
            graphs = sql('CHECKSUM TABLE coreci_graph_packages,coreci_graph_targets,coreci_graph_prerequisites,coreci_graph_releases')
            restart(fx)
            assert sql('SELECT version FROM coreci_schema') == '6'
            assert sql(f'SELECT {columns} FROM coreci_events WHERE seq<={maximum} ORDER BY seq') == rows
            assert sql('CHECKSUM TABLE coreci_graph_packages,coreci_graph_targets,coreci_graph_prerequisites,coreci_graph_releases') == graphs
        assert fx.webhook('owner/a') == 200
        owner, values = fx.dispatches.get(timeout=10)
        fx.started(owner, values)
        plan = fx.plan(values)
        entries = [entry for target in plan['targets'] for entry in target['units']]
        fx.unit(values, entries[0])
        fx.unit(values, entries[1], status='started')

        def status():
            return json.loads(fx.request('/api/v1/status.json', browser=False).read())

        before = status()['units']
        fx.advance(70)
        assert eventually(lambda: status()['wave']['phase'] == 'Finished')
        assert status()['wave']['conclusion'] == 'timeout'
        assert sql("SELECT COUNT(*) FROM coreci_events WHERE event_type='WaveExpired'") == '1'
        assert status()['units'] == before, 'deadline synthesized unit outcomes'
        fx.process.terminate()
        assert fx.process.wait(timeout=10) == 0
        fx.command[fx.command.index('--wave-deadline') + 1] = '86400'
        restart(fx)
        assert status()['wave']['phase'] == 'Finished', 'configuration change revived the expired wave'
        assert status()['units'] == before, 'replay changed expired unit outcomes'
        assert fx.plan(values) == plan

        before = fx.request('/api/v1/status.json', browser=False).read()
        fx.started(owner + 1, values)
        for phase, outcome in [('build', 'started'), ('build', 'succeeded'), ('build', 'failed'),
                               ('publish', 'started'), ('publish', 'succeeded'), ('publish', 'failed')]:
            fx.report(values, scope='unit', run_id=owner + 1, build_id=entries[0]['build_id'],
                      phase=phase, status=outcome, artifact='wrong-owner', feed_commit='c' * 40, reason='wrong-owner')
        fx.finished(owner + 1, values)
        assert fx.request('/api/v1/status.json', browser=False).read() == before, 'late non-owner changed state'

        fx.unit(values, entries[0], 'publish')
        fx.unit(values, entries[1])
        fx.unit(values, entries[1], 'publish')
        published = [unit for unit in status()['units'] if unit['package'] == 'a']
        assert all(unit['status'] == 'Published' and unit['failure'] is None for unit in published), published
        assert all(unit['published']['runId'] == owner and unit['published']['feedCommit'] == 'b' * 40
                   for unit in published)
        assert status()['wave']['phase'] == 'Finished' and fx.dispatches.empty()
        before = fx.request('/api/v1/status.json', browser=False).read()
        fx.unit(values, entries[0], 'publish')
        assert fx.request('/api/v1/status.json', browser=False).read() == before, 'duplicate late publication changed state'
        fx.finished(owner, values)
        assert status()['wave']['conclusion'] == 'success'
        assert fx.webhook('owner/b') == 200
        next_owner, next_values = fx.dispatches.get(timeout=10)
        seq = int(next_values['wave_id'][1:])
        assert sql(f'SELECT dispatch_deadline-event_at FROM coreci_events WHERE seq={seq}') == '86400'
        for target in fx.plan(next_values)['targets']:
            reused = next(entry for entry in target['units'] if entry['package'] == 'a')
            assert reused['action'] == 'reuse' and reused['revision'] == 1 and reused['build_id'] == ''
        assert fx.plan(values) == plan, 'late callbacks changed the frozen owner plan'
        fx.complete(next_owner, next_values)
    print('S27: deadline/replay, late owner facts, non-owner fence and next-plan reuse passed')
    return None


result = 77
if all(os.environ.get(key) for key in ('CORECI_TEST_DB_HOST', 'CORECI_TEST_DB_USER', 'CORECI_TEST_DB_NAME')):
    run(sys.argv[1])
    result = 0
raise SystemExit(result)
