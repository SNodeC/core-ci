"""Loopback HTTP regression for ambiguous start recovery and owner admission."""
import json
import os
from pathlib import Path
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).parent / 'dashboard'))
from fixture import fixture


def run(build):
    with fixture(build, small=True, virtual_clock=True, wave_deadline=2000) as fx:
        fx.ready()
        assert fx.webhook('owner/a') == 200
        original, values = fx.dispatches.get(timeout=10)
        plan = fx.plan(values)
        assert plan['owner_run_id'] is None
        fx.process.kill()
        fx.process.wait(timeout=10)
        fx.process = subprocess.Popen(fx.command, env=fx.env, stdout=fx.log, stderr=subprocess.STDOUT)
        ready = False
        deadline = time.monotonic() + 10
        while not ready and time.monotonic() < deadline:
            try:
                ready = fx.request('/api/v1/status.json', browser=False).status == 200
            except OSError:
                time.sleep(.05)
        assert ready
        fx.advance(850)
        time.sleep(1)
        assert fx.dispatches.empty(), 'restart redispatched before the configured start timeout'
        fx.advance(60)
        owner, retried = fx.dispatches.get(timeout=5)
        assert owner != original and retried == values, 'retry changed the frozen wave identity or inputs'
        assert fx.plan(values) == plan
        fx.started(owner, values)
        plan['owner_run_id'] = owner
        assert fx.plan(values) == plan
        before = fx.request('/api/v1/status.json', browser=False).read()
        fx.started(original, values)
        fx.report(values, scope='unit', run_id=original, build_id=plan['targets'][0]['units'][0]['build_id'],
                  phase='build', status='started')
        fx.finished(original, values)
        assert fx.plan(values) == plan, 'duplicate run changed the owner plan'
        assert fx.request('/api/v1/status.json', browser=False).read() == before, 'non-owner callbacks changed state'
        fx.complete(owner, values)
        status = json.loads(fx.request('/api/v1/status.json', browser=False).read())
        assert all(unit['status'] == 'Published' for unit in status['units'] if unit['package'] == 'a')
        assert status['wave']['phase'] == 'Finished' and status['wave']['runId'] == owner
        assert fx.plan(values) == plan and fx.dispatches.empty()
    print('S26: HTTP204 without start, restart, default900s retry, frozen plan and first-owner callback fencing passed')
    return None


result = 77
if all(os.environ.get(key) for key in ('CORECI_TEST_DB_HOST', 'CORECI_TEST_DB_USER', 'CORECI_TEST_DB_NAME')):
    run(sys.argv[1])
    result = 0
raise SystemExit(result)
