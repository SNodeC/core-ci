#!/usr/bin/env python3
"""Loopback-only functional qualification of the installed dependencies and this build."""
import hashlib
import hmac
import json
import os
from pathlib import Path
import queue
import socket
import ssl
import subprocess
import sys
import threading
import time
import urllib.parse

sys.path.insert(0, str(Path(__file__).parent / 'dashboard'))
from fixture import fixture

def raw_status(fx, headers):
    with socket.create_connection(('127.0.0.1', fx.app_port), timeout=5) as tcp:
        with fx.context.wrap_socket(tcp, server_hostname='localhost') as connection:
            connection.sendall(headers.encode('ascii'))
            line = b''
            while b'\r\n' not in line and len(line) < 8192:
                data = connection.recv(1024)
                assert data
                line += data
    return int(line.split(b'\r\n', 1)[0].split()[1])


def run(build):
    with fixture(build, small=True, wrong_host=True) as fx:
        fx.release.set()
        time.sleep(2)
        assert fx.process.poll() is None
        assert fx.request('/api/v1/status.json', browser=False).status == 200
        assert not fx.api_requests, 'wrong-host TLS sent HTTP bytes'
        print('PASS: hostname mismatch rejected before any GitHub request')

    with fixture(build, small=True, parser=['--maximum-body-bytes', '16384', '--maximum-header-bytes', '1024',
                                          '--maximum-header-fields', '4', '--maximum-start-line-bytes', '1024']) as fx:
        fx.ready()
        assert raw_status(fx, 'POST /api/v1/github/webhook HTTP/1.1\r\nHost: localhost\r\nContent-Length: 16385\r\n\r\n') == 413
        assert raw_status(fx, 'GET / HTTP/1.1\r\nHost: localhost\r\nX-Large: ' + 'x' * 1500 + '\r\n\r\n') == 431
        assert raw_status(fx, 'GET / HTTP/1.1\r\nHost: localhost\r\n' + 'X-Test: x\r\n' * 4 + '\r\n') == 431
        assert raw_status(fx, 'GET /' + 'x' * 1500 + ' HTTP/1.1\r\nHost: localhost\r\n\r\n') == 431
    with fixture(build, small=True) as fx:
        fx.ready()
        assert fx.request('/').status == 200
        assert fx.mints == 1
        def state(package, target):
            value = json.loads(fx.request('/api/v1/status.json', browser=False).read())
            return next(u for u in value['units'] if (u['package'], u['target']) == (package, target))
        def push(repo, default='master', delivery='branch-push'):
            raw = json.dumps(dict(ref='refs/heads/' + default, deleted=False,
                repository=dict(full_name=repo, default_branch=default))).encode()
            response = fx.request('/api/v1/github/webhook', raw, {'Content-Type': 'application/json',
                'X-GitHub-Event': 'push', 'X-GitHub-Delivery': delivery,
                'X-Hub-Signature-256': 'sha256=' + hmac.new(b'fixture-webhook', raw, hashlib.sha256).hexdigest()})
            return response.status
        for name, maximum in (('body-bytes', 25 * 1024 * 1024), ('header-bytes', 64 * 1024),
                              ('header-fields', 100), ('start-line-bytes', 8 * 1024)):
            for value in (0, maximum + 1):
                checked = subprocess.run(fx.command + ['http', 'parser', '--maximum-' + name, str(value)],
                    env=fx.env, capture_output=True, timeout=10)
                assert checked.returncode != 0 and ('invalid option: maximum-' + name).encode() in checked.stderr
        assert raw_status(fx, 'POST /api/v1/github/webhook HTTP/1.1\r\nHost: localhost\r\nContent-Length: 26214401\r\n\r\n') == 413
        assert raw_status(fx, 'GET / HTTP/1.1\r\nHost: localhost\r\n' + 'X-Test: x\r\n' * 100 + '\r\n') == 431
        assert raw_status(fx, 'GET /' + 'x' * 8192 + ' HTTP/1.1\r\nHost: localhost\r\n\r\n') == 431
        assert fx.request('/api/v1/waves/unknown/report', b'x' * 65537, browser=False).status == 413
        for path in ('status', 'graph.dot', 'reload', 'reconcile', 'units/p/t/retry', 'units/p/t/rebuild', 'builds/id/report'):
            assert fx.request('/api/v1/' + path, browser=False).status == 404
            assert fx.request('/api/v1/' + path, b'', browser=False).status == 404
        assert fx.request('/api/v1/status.json', b'', browser=False).status == 405
        assert raw_status(fx, 'HEAD /api/v1/status.json HTTP/1.1\r\nHost: localhost\r\n\r\n') == 405
        assert fx.request('/api/v1/waves/a%ZZ/report', b'', browser=False).status == 400
        assert fx.request('/api/v1/waves/a%252Bb/report', b'', browser=False).status == 401
        def signed_form(body, skew=0, content_type='application/x-www-form-urlencoded'):
            stamp = str(int(time.time()) + skew)
            signature = hmac.new(b'fixture-callback', (stamp + '.' + body).encode(), hashlib.sha256).hexdigest()
            return fx.request('/api/v1/waves/unknown/report', body.encode(),
                {'Content-Type': content_type, 'X-CoreCI-Timestamp': stamp, 'X-CoreCI-Signature-256': 'sha256=' + signature}, browser=False)
        assert signed_form('phase=build&phase=publish').status == 400
        assert signed_form('phase=%xx').status == 400
        assert signed_form('phase=build', skew=-301).status == 401
        assert signed_form('phase=build', content_type='application/x-www-form-urlencodedBAD').status == 400
        assert signed_form('phase=publish&status=started').status == 400
        assert signed_form('scope=unit&build_id=id&phase=publish&status=started').status == 200
        for repo in 'bcde':
            assert fx.webhook('owner/' + repo) == 200
        assert fx.webhook('owner/a') == 200
        rid, values = fx.dispatches.get(timeout=12)
        plan = fx.plan(values)
        assert len(plan['targets']) == 2
        assert all(unit['action'] == 'build' for target in plan['targets'] for unit in target['units'])
        assert fx.dispatches.empty(), 'tag pushes must coalesce into one wave'
        fx.started(rid, values)
        first = plan['targets'][0]['units'][0]
        fx.unit(values, first, status='started')
        fx.process.kill()
        fx.process.wait(timeout=10)
        fx.process = subprocess.Popen(fx.command, env=fx.env, stdout=fx.log, stderr=subprocess.STDOUT)
        deadline = time.monotonic() + 10
        ready = False
        while not ready and time.monotonic() < deadline:
            try:
                ready = fx.request('/api/v1/status.json', browser=False).status == 200
            except OSError:
                time.sleep(.05)
        assert ready
        time.sleep(2)
        assert fx.dispatches.empty(), 'restart must wait for the existing wave'
        assert fx.plan(values) == plan
        for target in plan['targets']:
            for entry in target['units']:
                if entry['build_id'] != first['build_id']:
                    fx.unit(values, entry, status='started')
                fx.unit(values, entry)
                fx.unit(values, entry, 'publish', 'succeeded')
        fx.finished(rid, values)
        assert state('e', 't1')['status'] == 'Published'
        assert '1.0.0-1' in fx.request('/api/v1/badge/d/t1.svg', browser=False).read().decode()
        # Second instance must not acquire this database's writer lock or listen.
        command = list(fx.command)
        command[command.index('--listen-port') + 1] = str(fx.redirect_port)
        command[command.index('--dashboard-redirect-port') + 1] = '0'
        refused = subprocess.Popen(command, env=fx.env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        try:
            try:
                output, _ = refused.communicate(timeout=3)
            except subprocess.TimeoutExpired:
                refused.terminate()
                output, _ = refused.communicate(timeout=10)
            assert b'store unavailable; recovering' in output
        finally:
            if refused.poll() is None:
                refused.kill()
                refused.wait(timeout=10)
        assert fx.post('/api/rebuild', dict(package='e', target='t1'))[0] == 200
        rebuilt_id, rebuilt = fx.dispatches.get(timeout=12)
        rebuilt_plan = fx.plan(rebuilt)
        assert len(rebuilt_plan['targets']) == 1
        builds = [unit for unit in rebuilt_plan['targets'][0]['units'] if unit['action'] == 'build']
        assert len(builds) == 1 and builds[0]['package'] == 'e' and builds[0]['revision'] == 2
        assert all(unit['action'] == 'reuse' for unit in rebuilt_plan['targets'][0]['units'] if unit['package'] != 'e')
        fx.complete(rebuilt_id, rebuilt)
        # A source default-branch change has no effect on the loaded tag-bound graph.
        before = len(fx.contents)
        assert push('owner/e') == 202
        time.sleep(.2)
        assert len(fx.contents) == before
        # Add one source and remove another by changing the registry.
        fx.sources['owner/f'] = dict(fx.sources['owner/a'], package='f', title='f', targets=['t1'])
        fx.tags['owner/f'] = {'v1.0.0': 'b' * 40}
        fx.registry['repositories'].remove('owner/e')
        fx.registry['repositories'].append('owner/f')
        assert push('Fixture/Registry', 'main', 'registry-unit-change') == 202
        assert fx.post('/api/reconcile', {})[0] == 200
        assert state('d', 't1')['status'] == 'Published' and state('d', 't1')['published']['revision'] == 1
        added_id, added = fx.dispatches.get(timeout=12)
        assert state('f', 't1')['selection']['revision'] == 1
        assert all(u['package'] != 'e' for u in json.loads(fx.request('/api/v1/status.json', browser=False).read())['units'])
        assert any(ref == 'b' * 40 for _, ref in fx.contents)
        assert all('/actions/runs' not in path for path in fx.api_requests)
        fx.complete(added_id, added)
        before = fx.request('/api/v1/status.json', browser=False).read()
        # Missing/older tags must never pair bootstrap/older declarations with retained releases.
        saved = fx.tags['owner/f']
        for tags in ({}, {'v0.9.0': 'b' * 40}, {'1.0.0': 'b' * 40}, {'v1.0.0': 'c' * 40}):
            fx.tags['owner/f'] = tags
            fx.sources['owner/f']['targets'].append('t2')
            assert fx.post('/api/reload', {})[0] == 503
            assert fx.request('/api/v1/status.json', browser=False).read() == before
            fx.sources['owner/f']['targets'].pop()
        fx.registry['repositories'].remove('owner/f')
        assert fx.post('/api/reload', {})[0] == 200
        dormant = fx.request('/api/v1/status.json', browser=False).read()
        fx.registry['repositories'].append('owner/f')
        fx.tags['owner/f'] = {}
        assert fx.post('/api/reload', {})[0] == 503
        assert fx.request('/api/v1/status.json', browser=False).read() == dormant
        fx.tags['owner/f'] = saved
        assert fx.post('/api/reload', {})[0] == 200
        assert state('f', 't1')['status'] == 'Published' and state('f', 't1')['published']['revision'] == 1
        before = fx.request('/api/v1/status.json', browser=False).read()
        fx.sources['owner/g'] = dict(fx.sources['owner/a'], package='g', title='g', depends_on=['g'], targets=['t1'])
        fx.tags['owner/g'] = {'v1.0.0': 'b' * 40}
        fx.registry['repositories'].append('owner/g')
        assert fx.post('/api/reload', {})[0] == 503
        assert fx.request('/api/v1/status.json', browser=False).read() == before
        assert fx.mints == 2, 'restart should mint once; other requests reuse the token'
        print('PASS: real TLS/App JWT, limits and routes, wave diamond plan, reuse, independent targets,')
        print('      callback-before-dispatch/restart, writer lock, badges, IDP rebuild, tag declarations and registry unit changes')

result = 77
if all(os.environ.get(key) for key in ('CORECI_TEST_DB_HOST', 'CORECI_TEST_DB_USER', 'CORECI_TEST_DB_NAME')):
    run(sys.argv[1])
    result = 0
raise SystemExit(result)
