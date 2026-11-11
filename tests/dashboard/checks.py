import concurrent.futures
import http.cookiejar
import json
import queue
import sys
import threading
import time
import urllib.error
import urllib.request
from fixture import fixture


class Stream:
    def __init__(self, fx):
        self.response = fx.request('/events', headers={'Accept': 'text/event-stream'})
        assert self.response.status == 200
        assert self.response.headers['Content-Type'] == 'text/event-stream'
        assert self.response.headers['Cache-Control'] == 'no-store'
        self.response.fp.raw._sock.settimeout(90)
        self.error = None
        self.closing = False
        self.messages, self.comments = queue.Queue(), queue.Queue()
        self.closed = threading.Event()
        self.thread = threading.Thread(target=self.read, daemon=True)
        self.thread.start()

    def read(self):
        kind = ''
        try:
            line = self.response.readline()
            while line:
                text = line.decode().rstrip('\r\n')
                if text.startswith('event: '):
                    kind = text[7:]
                elif text.startswith('data: '):
                    self.messages.put((time.monotonic(), kind, json.loads(text[6:])))
                elif text.startswith(':'):
                    self.comments.put(time.monotonic())
                line = self.response.readline()
        except Exception as error:
            if not self.closing:
                self.error = type(error).__name__
        finally:
            self.closed.set()

    def next(self):
        return self.messages.get(timeout=10)

    def close(self):
        import socket
        self.closing = True
        if not self.closed.is_set():
            self.response.fp.raw._sock.shutdown(socket.SHUT_RDWR)
            self.thread.join(timeout=5)
        self.response.close()


def stream_checks(fx):
    assert fx.request('/').status == 200
    stream = Stream(fx)
    at, kind, value = stream.next()
    assert kind == 'snapshot' and value['ready'] is False and value['units'] == []
    fx.ready()
    _, kind, value = stream.next()
    assert kind == 'update' and value['ready'] is True and len(value['units']) == 178
    assert all({'distribution', 'suite', 'arch', 'format', 'prerequisites'} <= set(unit) for unit in value['units'])
    held = [fx.request('/events') for _ in range(63)]
    assert all(response.status == 200 for response in held)
    assert fx.request('/events').status == 503
    for response in held:
        response.close()
    assert fx.webhook('SNodeC/snode.c') == 200
    fx.dispatches.get(timeout=10)
    for version in range(1, 5):
        assert fx.webhook('SNodeC/snode.c', 'v1.0.' + str(version)) == 200
    time.sleep(3)
    updates = []
    while not stream.messages.empty():
        updates.append(stream.messages.get())
    assert updates and all(item[1] == 'update' for item in updates)
    assert all(b[0] - a[0] >= .85 for a, b in zip(updates, updates[1:])), 'updates were not coalesced'
    first_comment = stream.comments.get(timeout=32)
    assert 29 <= first_comment - at <= 31.5
    second_comment = stream.comments.get(timeout=32)
    assert 59 <= second_comment - at <= 62.5
    assert not stream.closed.is_set(), 'authenticated SSE must survive the 60-second timeout and session recheck'
    # Force real connection-loss recovery using the existing SNode.C-only fixture.
    import subprocess
    interrupt = subprocess.Popen([fx.build / 'coreci-interrupt-store'], env=fx.env, stdout=subprocess.PIPE, text=True)
    locked, line = False, None
    while not locked and line != '':
        line = interrupt.stdout.readline()
        locked = line.strip() == 'LOCKED'
    assert locked
    while not stream.messages.empty():
        stream.messages.get()
    assert fx.webhook('SNodeC/snode.c', 'v1.0.9') == 503
    assert fx.request('/api/v1/status.json').status == 503
    unavailable = fx.post('/api/retry', dict(package='snodec', target='debian-trixie-amd64'))
    assert unavailable[0] == 503 and unavailable[1]['error'] == 'unavailable'
    time.sleep(2)
    assert stream.messages.empty(), 'model sent during recovery'
    assert interrupt.wait(timeout=12) == 0
    _, _, recovered = stream.next()
    assert recovered['ready'] is True
    stream.close()
    print('S22: snapshot/ready, 178 catalog units, 64-stream limit, coalescing, 30s keep-alive, recovery silence/replay passed')


def button_checks(fx):
    fx.ready()
    import http.client
    for method in ('GET', 'POST'):
        connection = http.client.HTTPConnection('127.0.0.1', fx.redirect_port)
        connection.request(method, '/path?query=value', body=b'')
        redirected = connection.getresponse()
        assert redirected.status == 308 and redirected.headers['Location'] == fx.app + '/path?query=value'
        redirected.read()
        connection.close()
    assert fx.request('/').status == 200
    for content_type, expected in [('Application/Json; charset=UTF-8', 200), ('application/json-extra', 400)]:
        response = fx.request('/api/retry', b'{"package":"unknown","target":"unknown"}',
                              {'Content-Type': content_type, 'Origin': fx.app})
        assert response.status == expected
    for name in ('enroll', 'request', 'reconcile', 'retire'):
        code, value = fx.post('/api/' + name, {})
        assert code == 400 and value['error'].startswith('Not supported by core-ci: ')
    assert fx.webhook('SNodeC/snode.c') == 200
    rid, values, main = fx.dispatches.get(timeout=10)
    assert main == 'build'
    fx.report(rid, values, 'build', 'failed')
    code, value = fx.post('/api/retry', dict(package=values['package'], target=values['target']))
    assert code == 200 and value['result'] == 'accepted'
    retry_id, retry_values, _ = fx.dispatches.get(timeout=10)
    deadline = time.monotonic() + 10
    while (retry_values['target'] != values['target'] or retry_values['revision'] == values['revision']) and time.monotonic() < deadline:
        retry_id, retry_values, _ = fx.dispatches.get(timeout=10)
    assert retry_id != rid and retry_values['target'] == values['target']
    assert int(retry_values['revision']) == int(values['revision']) + 1
    assert fx.post('/api/retry', {'package': 'unknown', 'target': 'unknown'})[0] == 200
    fx.release.clear()
    fx.requested.clear()
    with concurrent.futures.ThreadPoolExecutor() as workers:
        pending = workers.submit(fx.post, '/api/refresh', {})
        assert fx.requested.wait(10)
        assert not pending.done()
        fx.release.set()
        assert pending.result(timeout=10)[0] == 200
    print('S23: Retry commits and allocates the next revision; Refresh waits for loading; four unsupported operations passed')


def auth_checks(fx):
    fx.ready()
    class NoRedirect(urllib.request.HTTPRedirectHandler):
        def redirect_request(self, req, fp, code, msg, headers, newurl):
            return None
    opener = urllib.request.build_opener(urllib.request.HTTPSHandler(context=fx.context), NoRedirect())
    response = None
    try:
        opener.open(fx.app + '/')
    except urllib.error.HTTPError as error:
        response = error
    assert response.code == 302 and '/oauth2/authorize?' in response.headers['Location']
    assert fx.request('/').status == 200
    cookie = next(item for item in fx.jar if item.name == 'coreci_access_token')
    assert cookie.secure and cookie.path == '/' and 43190 <= cookie.expires - time.time() <= 43200
    assert 'HttpOnly' in cookie._rest and cookie._rest['SameSite'] == 'Lax'
    assert fx.request('/app.js?ignored=1').status == 200
    fx.subject = 'other'
    other = urllib.request.build_opener(urllib.request.HTTPSHandler(context=fx.context), urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()))
    refused = None
    try:
        other.open(fx.app + '/')
    except urllib.error.HTTPError as error:
        refused = error
    assert refused.code == 403, 'another subject with the same email was admitted'
    fx.subject = 'owner'
    assert fx.request('/api/v1/status', browser=False).status == 200
    assert fx.request('/api/v1/reload', b'', browser=False).status == 401
    stream = Stream(fx)
    stream.next()
    fx.revoked.add('owner')
    invalidated = time.monotonic()
    assert stream.closed.wait(61)
    assert stream.error is None, stream.error
    elapsed = time.monotonic() - invalidated
    assert elapsed <= 60.5
    print('S24: PKCE/state/nonce, subject allowlist, same-email denial, secure 12h cookie, admin isolation passed; revoked stream closed after %.3f s' % elapsed)


result = 77
if all(__import__('os').environ.get(key) for key in ('CORECI_TEST_DB_HOST', 'CORECI_TEST_DB_USER', 'CORECI_TEST_DB_NAME')):
    with fixture(sys.argv[1]) as fx:
        {'stream': stream_checks, 'buttons': button_checks, 'auth': auth_checks}[sys.argv[2]](fx)
    result = 0
raise SystemExit(result)
