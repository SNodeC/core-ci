"""Real daemon: frozen CI corrections, persisted pause, range blocks and no dispatch."""
import json
from pathlib import Path
import subprocess
import sys
import time
from database import eventually

sys.path.insert(0, str(Path(__file__).parent / 'dashboard'))
from fixture import fixture


def run(build):
    with fixture(build, small=True) as fx:
        def status():
            response = fx.request('/api/v1/status.json', browser=False)
            value = json.loads(response.read()) if response.status == 200 else None
            return value
        def plan():
            wave = status()['wave']['id']
            value = json.loads(fx.request('/api/v1/waves/' + wave + '/plan.json', browser=False).read())
            return value
        fx.ready()
        assert fx.request('/').status == 200
        assert fx.post('/api/dispatch-paused', {'paused': True})[0] == 200
        fx.tags = {'owner/' + name: {'v3.0.0' if name == 'a' else 'v1.0.0': 'b' * 40} for name in 'abcde'}
        assert fx.post('/api/reconcile', {})[0] == 200
        assert eventually(lambda: status()['wave'] is not None)
        frozen = plan()
        assert status()['dispatch_paused'] and fx.dispatches.empty()
        assert all(unit['ci_commit'] == 'a' * 40 for target in frozen['targets'] for unit in target['units'])
        fx.process.terminate()
        assert fx.process.wait(timeout=10) == 0
        fx.process = subprocess.Popen(fx.command, env=fx.env, stdout=fx.log, stderr=subprocess.STDOUT)
        assert eventually(lambda: status() is not None)
        assert status()['dispatch_paused'] and plan() == frozen and fx.dispatches.empty()
        assert fx.request('/').status == 200
        assert fx.post('/api/dispatch-paused', {'paused': False})[0] == 200
        owner, values = fx.dispatches.get(timeout=10)
        assert fx.plan(values) == frozen
        fx.started(owner, values)
        entry = frozen['targets'][0]['units'][0]
        fx.unit(values, entry, status='failed')
        fx.finished(owner, values, 'failure')
        fx.ci_commit, fx.ci_tree = 'e' * 40, 'f' * 40
        assert fx.post('/api/retry', {'package': 'a', 'target': 't1'})[0] == 200
        owner, corrected = fx.dispatches.get(timeout=10)
        updated = fx.plan(corrected)
        retry = updated['targets'][0]['units'][0]
        assert retry['commit'] == entry['commit'] and retry['ci_commit'] == 'e' * 40
        assert retry['revision'] == entry['revision'] + 1 and retry['build_id'] != entry['build_id']
        assert fx.plan(values)['targets'] == frozen['targets']
        fx.complete(owner, corrected)
        fx.ci_commit = 'c' * 40
        assert fx.post('/api/reconcile', {})[0] == 200
        time.sleep(1.2)
        assert fx.dispatches.empty(), 'ordinary master commit rebuilt the unchanged CI tree'
        assert fx.post('/api/dispatch-paused', {'paused': True})[0] == 200
        fx.sources['owner/b']['depends_on'] = [dict(package='a', version='>=2.0,<3')]
        fx.ci_tree = '1' * 40
        assert fx.post('/api/reconcile', {})[0] == 200
        assert eventually(lambda: any(unit['status'] == 'Blocked' for unit in status()['units']))
        blocked = next(unit for unit in status()['units'] if unit['package'] == 'b' and unit['target'] == 't1')
        assert blocked['failure']['reason'] == 'dependency a 3.0.0 outside >=2.0,<3'
        assert all(unit['package'] != 'b' for target in plan()['targets'] for unit in target['units'])
        assert fx.dispatches.empty()
    print('CI correction, same tag revision, unchanged tree, range blocking and pause across restart passed')


if __name__ == '__main__':
    run(sys.argv[1])
