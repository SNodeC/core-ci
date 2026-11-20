"""Late owner facts survive stored deadlines and feed the next plan."""
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


def run(build):
    with fixture(build, small=True, virtual_clock=True, wave_deadline=60) as fx:
        fx.ready()
        assert fx.webhook('owner/a') == 200
        owner, values = fx.dispatches.get(timeout=10)
        fx.started(owner, values)
        plan = fx.plan(values)
        first, second = plan['targets']
        fx.unit(values, first['units'][0])
        fx.unit(values, second['units'][0], status='started')

        def status():
            return json.loads(fx.request('/api/v1/status.json', browser=False).read())

        deadline = sql("SELECT deadline FROM coreci_waves")
        fx.advance(70)
        assert eventually(lambda: status()['wave']['phase'] == 'Finished')
        assert status()['wave']['conclusion'] == 'timeout'
        assert sql("SELECT expired FROM coreci_waves") == '1'
        fx.process.terminate()
        assert fx.process.wait(timeout=10) == 0
        fx.settings['coreci']['wave-deadline'] = '86400'
        fx.write_config()
        fx.process = subprocess.Popen(fx.command, env=fx.env, stdout=fx.log, stderr=subprocess.STDOUT)
        assert eventually(lambda: fx.request('/api/v1/status.json', browser=False).status == 200)
        assert status()['wave']['phase'] == 'Finished'
        assert sql("SELECT deadline FROM coreci_waves") == deadline
        assert fx.plan(values) == plan

        before = fx.request('/api/v1/status.json', browser=False).read()
        fx.started(owner + 1, values)
        for outcome in ('started', 'succeeded', 'failed'):
            fx.report(values, scope='unit', run_id=owner + 1, build_id=first['units'][0]['build_id'],
                      phase='build', status=outcome, reason='wrong-owner')
        fx.report(values, scope='target', status='finished', run_id=owner + 1,
                  target=first['id'], artifact='wrong-owner')
        fx.finished(owner + 1, values)
        assert fx.request('/api/v1/status.json', browser=False).read() == before

        fx.target(values, first['id'])
        publication_owner, publication = fx.dispatches.get(timeout=10)
        fx.started(publication_owner, publication)
        fx.unit(values, second['units'][0])
        fx.target(values, second['id'])
        assert fx.dispatches.empty(), 'second publication overlapped the first'
        for item in fx.plan(publication)['items']:
            for entry in item['units']:
                fx.unit(publication, entry, 'publish')
        fx.finished(publication_owner, publication)
        next_owner, next_publication = fx.publish_one()
        published = [unit for unit in status()['units'] if unit['package'] == 'a']
        assert all(unit['status'] == 'Published' and unit['failure'] is None for unit in published), published
        assert {unit['published']['runId'] for unit in published} == {publication_owner, next_owner}
        before = fx.request('/api/v1/status.json', browser=False).read()
        for item in fx.plan(publication)['items']:
            for entry in item['units']:
                fx.unit(publication, entry, 'publish')
        assert fx.request('/api/v1/status.json', browser=False).read() == before
        fx.finished(owner, values)
        assert status()['wave']['conclusion'] == 'success'
        assert fx.webhook('owner/b') == 200
        next_owner, next_values = fx.dispatches.get(timeout=10)
        assert sql("SELECT deadline-dispatched_at FROM coreci_waves ORDER BY dispatched_at DESC LIMIT 1") == '86400'
        for target in fx.plan(next_values)['targets']:
            reused = next(entry for entry in target['units'] if entry['package'] == 'a')
            assert reused['action'] == 'reuse' and reused['revision'] == 1 and reused['build_id'] == ''
        assert fx.plan(values) == plan
        fx.complete(next_owner, next_values)
    print('S27: stored deadlines, late owner facts, distinct publication ownership and next-plan reuse passed')


result = 77
if all(os.environ.get(key) for key in ('CORECI_TEST_DB_HOST', 'CORECI_TEST_DB_USER', 'CORECI_TEST_DB_NAME')):
    run(sys.argv[1])
    result = 0
raise SystemExit(result)
