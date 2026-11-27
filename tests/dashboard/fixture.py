"""Disposable loopback GitHub/OIDC fixtures; never contacts configured feed URLs."""
import base64
from contextlib import contextmanager
import hashlib
import hmac
import http.cookiejar
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import queue
import socket
import ssl
import subprocess
import tempfile
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from cryptography.hazmat.primitives.asymmetric import padding
from cryptography.hazmat.primitives import hashes, serialization


def port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        result = sock.getsockname()[1]
    return result


def b64(value):
    return base64.urlsafe_b64encode(value).decode().rstrip('=')


class Provider(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def respond(self, status, body=None, headers=None):
        raw = json.dumps(body).encode() if body is not None else b''
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(raw)))
        for name, value in (headers or {}).items():
            self.send_header(name, value)
        self.end_headers()
        self.wfile.write(raw)

    def do_GET(self):
        query = urllib.parse.urlsplit(self.path)
        path = urllib.parse.unquote(query.path)
        state = self.server.state
        state.api_requests.append(path)
        if path == '/oauth2/authorize':
            values = {k: v[0] for k, v in urllib.parse.parse_qs(query.query).items()}
            assert values['code_challenge_method'] == 'S256'
            assert values['scope'] == 'openid profile email'
            assert values['acr_values'] == 'urn:snodec:acr:mfa'
            assert values['state'] and values['nonce'] and values['code_challenge']
            code = str(len(state.codes) + 1)
            state.codes[code] = dict(values, subject=state.subject)
            self.respond(302, headers={'Location': values['redirect_uri'] + '?' + urllib.parse.urlencode(dict(code=code, state=values['state']))})
        elif '/git/ref/heads/' in path:
            self.respond(200, dict(object=dict(sha=state.head)))
        elif '/git/matching-refs/snapshots/' in path:
            self.respond(200, [dict(ref=ref, object=dict(sha=sha)) for ref, sha in state.snapshots.items()])
        elif path.endswith('/contents/publications.json'):
            self.respond(200, state.ledger)
        elif '/contents/' in path:
            if path.endswith('registry.json'):
                state.requested.set()
                assert state.release.wait(20), 'declaration fixture not released'
                data = state.registry
            elif path.endswith('targets.json'):
                data = state.catalog
            elif path.endswith('/contents/'):
                data = [dict(name='ci', type='dir', sha=state.ci_tree)]
            else:
                repo = path.split('/repos/', 1)[1].split('/contents/', 1)[0]
                ref = urllib.parse.parse_qs(query.query)['ref'][0]
                assert ref == state.ci_commit, 'declaration not read at default-branch tip'
                state.contents.append((repo, ref))
                data = dict(state.sources[repo], schema=2)
                data['depends_on'] = [dict(package=item, version='>=0.0.0') if isinstance(item, str) else item
                                      for item in data['depends_on']]
            self.respond(200, data)
        elif '/commits/' in path:
            self.respond(200, dict(sha=state.ci_commit))
        elif path.endswith('/tags'):
            repo = path.removeprefix('/repos/').removesuffix('/tags')
            self.respond(200, [dict(name=tag, commit=dict(sha=sha)) for tag, sha in state.tags.get(repo, {}).items()])
        elif '/actions/runs/' in path:
            run = state.runs.get(int(path.rsplit('/', 1)[1]))
            self.respond(200 if run else 404, run)
        elif '/actions/workflows/' in path and path.endswith('/runs'):
            kind = 'publish ' if 'publish.yml' in path else 'build '
            self.respond(200, dict(workflow_runs=[run for run in state.runs.values()
                                                 if run['display_title'].startswith(kind)]))
        elif path.startswith('/repos/'):
            repo = path.removeprefix('/repos/')
            self.respond(200, dict(full_name=repo, default_branch='master' if repo in ('SNodeC/snode.c', 'SNodeC/mqttsuite') else 'main'))
        else:
            self.respond(404)

    def do_POST(self):
        state = self.server.state
        raw = self.rfile.read(int(self.headers.get('Content-Length', '0')))
        fields = {k: v[0] for k, v in urllib.parse.parse_qs(raw.decode()).items()}
        if self.path == '/app/installations/42/access_tokens':
            credential = self.headers['Authorization'].removeprefix('Bearer ')
            first, second, signature = credential.split('.')
            state.key.public_key().verify(base64.urlsafe_b64decode(signature + '=' * (-len(signature) % 4)),
                                          (first + '.' + second).encode(), padding.PKCS1v15(), hashes.SHA256())
            claims = json.loads(base64.urlsafe_b64decode(second + '=' * (-len(second) % 4)))
            assert claims['iss'] == '123' and abs(claims['iat'] - (state.now() - 60)) <= 3
            state.mints += 1
            self.respond(201, dict(token='fixture-installation', expires_at=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime(state.now() + 3600))))
        elif self.path == '/oauth2/token':
            if fields['grant_type'] == 'authorization_code':
                data = state.codes.pop(fields['code'])
                assert b64(hashlib.sha256(fields['code_verifier'].encode()).digest()) == data['code_challenge']
                assert fields['redirect_uri'] == data['redirect_uri']
            else:
                assert fields['grant_type'] == 'refresh_token'
                data = state.refresh_tokens.pop(fields['refresh_token'])
                if data['subject'] not in state.revoked:
                    state.renewals.append((state.now(), data['expiry']))
            if data['subject'] in state.revoked:
                self.respond(400, dict(error='invalid_grant'))
            else:
                now = state.now()
                claims = dict(iss=state.api, aud='coreci-test', sub=data['subject'], sid='session-' + data['subject'],
                              email='same@example.invalid', username='Fixture', scope=['openid', 'profile', 'email'],
                              exp=now + state.token_lifetime, iat=now, nbf=now, mfa_verified=True)
                def sign(kind):
                    payload = dict(claims, token_use=kind)
                    if kind == 'id':
                        payload.update(nonce=data['nonce'], acr='urn:snodec:acr:mfa')
                    message = b64(json.dumps(dict(alg='RS256', typ='JWT' if kind == 'id' else 'at+jwt')).encode()) + '.' + b64(json.dumps(payload).encode())
                    result = message + '.' + b64(state.key.sign(message.encode(), padding.PKCS1v15(), hashes.SHA256()))
                    return result
                access = sign('access')
                state.tokens[access] = claims
                state.refresh_sequence += 1
                refresh = 'fixture-refresh-' + str(state.refresh_sequence)
                state.refresh_tokens[refresh] = dict(data, expiry=claims['exp'])
                self.respond(200, dict(access_token=access, id_token=sign('id'), refresh_token=refresh, refresh_retry=True))
        elif self.path == '/oauth2/introspect':
            claims = state.tokens.get(fields.get('token'))
            active = bool(claims) and claims['sub'] not in state.revoked and claims['exp'] > state.now()
            if state.unavailable:
                self.respond(503, dict(error='temporarily_unavailable'))
            else:
                self.respond(200, dict(active=active, sub=claims['sub'], sid=claims['sid'], mfa_verified=True) if active else dict(active=False))
        elif self.path.endswith('/dispatches'):
            values = json.loads(raw)['inputs']
            publication = self.path.endswith('/actions/workflows/publish.yml/dispatches')
            assert publication or self.path.endswith('/actions/workflows/build.yml/dispatches')
            assert json.loads(raw)['ref'] == 'main' and json.loads(raw)['return_run_details'] is True
            identity = 'publication_id' if publication else 'request_id'
            assert set(values) == {identity, 'attempt', 'feed_branch'} | ({'fault'} if publication else set())
            assert all(isinstance(value, str) for value in values.values())
            assert values['feed_branch'] in ('feed', 'feed-staging')
            rid = state.next_run
            state.next_run += 1
            title = ('publish ' if publication else 'build ') + values[identity] + '/' + values['attempt']
            state.runs[rid] = dict(id=rid, status='queued', conclusion=None, display_title=title, run_attempt=1)
            state.dispatches.put((rid, values))
            assert state.dispatch_response.wait(20), 'dispatch response fixture not released'
            if publication and state.ambiguous_publication:
                self.respond(204)
            else:
                self.respond(200, dict(workflow_run_id=rid))
        elif '/actions/runs/' in self.path and self.path.endswith('/cancel'):
            rid = int(self.path.split('/')[-2])
            state.runs[rid].update(status='completed', conclusion='cancelled')
            self.respond(202)
        else:
            self.respond(404)


class Fixture:
    ci_commit = 'a' * 40
    ci_tree = 'd' * 40
    def __init__(self, build, work, small=False, wrong_host=False, parser=(), virtual_clock=False, wave_deadline=120):
        self.build, self.work = Path(build), Path(work)
        self.codes, self.tokens = {}, {}
        self.refresh_tokens, self.renewals = {}, []
        self.refresh_sequence = 0
        self.offset = 0
        self.next_run = 100
        self.head = 'f' * 40
        self.snapshots = {'refs/snapshots/feed/pre-x': self.head}
        self.ledger = []
        self.runs = {}
        self.ambiguous_publication = False
        self.tags, self.contents, self.api_requests, self.mints = {}, [], [], 0
        self.subject, self.revoked = 'owner', set()
        self.unavailable = False
        self.token_lifetime = 3600
        self.dispatches = queue.Queue()
        self.dispatch_response = threading.Event()
        self.dispatch_response.set()
        self.requested, self.release = threading.Event(), threading.Event()
        self.api_port, self.app_port, self.redirect_port = port(), port(), port()
        self.api = 'https://localhost:' + str(self.api_port)
        self.app = 'https://localhost:' + str(self.app_port)
        self.cert, self.key_path = self.work / 'fixture.crt', self.work / 'fixture.key'
        subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1', '-subj', '/CN=localhost',
                        '-addext', 'subjectAltName=DNS:localhost', '-keyout', self.key_path, '-out', self.cert],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.key = serialization.load_pem_private_key(self.key_path.read_bytes(), password=None)
        self.public = self.work / 'idp-public.pem'
        self.public.write_bytes(self.key.public_key().public_bytes(serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo))
        data = Path(__file__).resolve().parents[1] / 'data/dashboard'
        self.registry = json.loads((data / 'registry.json').read_bytes())
        self.catalog = json.loads((data / 'targets.json').read_bytes())
        self.sources = {repo: json.loads((data / (name + '.json')).read_bytes()) for repo, name in
                        [('SNodeC/snode.c', 'snodec'), ('SNodeC/mqttsuite', 'mqttsuite'), ('SNodeC/snode.c-idp', 'snodec-idp')]}
        if small:
            self.registry = dict(schema=1, repositories=['owner/' + p for p in 'abcde'])
            self.catalog = dict(targets=[dict(id=t, family='deb', distribution='test', release='stable',
                                            codename='stable', arch='test', image='fixture:test', runner='ubuntu-latest',
                                            platform='linux/amd64', emulated=False) for t in ('t1', 't2')])
            self.sources = {'owner/' + p: dict(schema=2, package=p, title=p, releases='>=1,<9', depends_on=q,
                build=dict(deb='ci/deb.sh', rpm='ci/rpm.sh'),
                build_dependencies=dict(deb=[], rpm=[], openwrt=[]),
                targets=['t1'] + ([] if p == 'e' else ['t2']))
                for p, q in [('a', []), ('b', ['a']), ('c', ['a']), ('d', ['b', 'c']), ('e', ['d'])]}
        self.env = {key: value for key, value in os.environ.items()
                    if key not in ('CORECI_WEBHOOK_SECRET', 'CORECI_CALLBACK_SECRET', 'CORECI_DB_PASSWORD')}
        self.env['SSL_CERT_FILE'] = str(self.cert)
        self.secret_files = {}
        for name, value in [('webhook-secret-file', 'fixture-webhook'), ('callback-secret-file', 'fixture-callback'),
                            ('db-password-file', os.environ.get('CORECI_TEST_DB_PASSWORD', ''))]:
            path = self.work / name
            path.write_text(value)
            self.secret_files[name] = str(path)
        if virtual_clock:
            self.clock_path = self.work / 'clock-offset'
            self.clock_path.write_text('0')
            self.env.update(LD_PRELOAD=str(self.build / 'libcoreci-test-clock.so'), CORECI_TEST_CLOCK_FILE=str(self.clock_path))
        self.context = ssl.create_default_context(cafile=str(self.cert))
        self.server = ThreadingHTTPServer(('127.0.0.1', self.api_port), Provider)
        self.server.state = self
        tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        tls.load_cert_chain(self.cert, self.key_path)
        self.server.socket = tls.wrap_socket(self.server.socket, server_side=True)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.log = (self.work / 'daemon.log').open('w')
        self.config_path = self.work / 'coreci.conf'
        self.settings = {
            'coreci': dict(registry='Fixture/Registry:ci/registry.json', **{
                'workflow-repository': 'Fixture/Workflows', 'target-catalog': 'ci/targets.json',
                'github-app-id': '123', 'github-installation-id': '42', 'github-app-key-file': str(self.key_path),
                'db-host': self.env['CORECI_TEST_DB_HOST'], 'db-port': self.env.get('CORECI_TEST_DB_PORT', '0'),
                'db-user': self.env['CORECI_TEST_DB_USER'], 'db-name': self.env['CORECI_TEST_DB_NAME'],
                'callback-base-url': self.app, 'github-api-url': self.api.replace('localhost', '127.0.0.1') if wrong_host else self.api,
                'github-ca-cert': str(self.cert), 'idp-base-url': self.api, 'idp-client-id': 'coreci-test',
                'idp-public-key-path': str(self.public), 'dashboard-users': 'owner', 'batch-delay': '1',
                'queue-deadline': str(wave_deadline), 'execution-deadline': str(wave_deadline), 'callback-grace': '2',
                'github_slots': '2', 'run-poll': '1', **self.secret_files}),
            'coreci.local': dict(host='127.0.0.1', port=self.app_port),
            'coreci.socket': {'reuse-address': True},
            'coreci.http.parser': {'maximum-body-bytes': 26214400, 'maximum-header-bytes': 65536,
                                   'maximum-header-fields': 100, 'maximum-start-line-bytes': 8192},
            'coreci.tls': dict(cert=str(self.cert), **{'cert-key': str(self.key_path)}),
            'coreci-redirect.local': dict(host='127.0.0.1', port=self.redirect_port)}
        self.write_config()
        self.command = [str(self.build / 'core-ci'), '--config-file', str(self.config_path), 'coreci', 'local', 'tls', 'socket', 'http', 'parser', 'coreci-redirect', 'local', 'coreci']
        if parser:
            index = self.command.index('parser') + 1
            self.command[index:index] = parser
        self.process = subprocess.Popen(self.command, env=self.env, stdout=self.log, stderr=subprocess.STDOUT)
        if not wrong_host and not self.requested.wait(10):
            self.log.flush()
            (self.build / 'fixture-startup.log').write_bytes((self.work / 'daemon.log').read_bytes())
            self.close()
            raise AssertionError('controller did not start declaration load; startup log saved')
        self.jar = http.cookiejar.CookieJar()
        self.browser = urllib.request.build_opener(urllib.request.HTTPSHandler(context=self.context), urllib.request.HTTPCookieProcessor(self.jar))

    def write_config(self):
        self.config_path.write_text('\n'.join('[' + section + ']\n' + '\n'.join(
            key + '=' + json.dumps(value) for key, value in values.items()) for section, values in self.settings.items()) + '\n')

    def now(self):
        return int(time.time()) + self.offset

    def advance(self, seconds):
        self.offset += seconds
        temporary = self.clock_path.with_suffix('.next')
        temporary.write_text(str(self.offset))
        temporary.replace(self.clock_path)

    def request(self, path, body=None, headers=None, browser=True):
        request = urllib.request.Request(self.app + path, data=body, headers=headers or {})
        try:
            if browser:
                response = self.browser.open(request, timeout=12)
            else:
                response = urllib.request.urlopen(request, context=self.context, timeout=12)
        except urllib.error.HTTPError as error:
            response = error
        return response

    def post(self, path, data):
        response = self.request(path, json.dumps(data).encode(), {'Content-Type': 'application/json', 'Origin': self.app})
        return response.status, json.loads(response.read())

    def ready(self):
        self.release.set()
        deadline = time.monotonic() + 10
        ready = False
        while not ready and time.monotonic() < deadline:
            ready = self.status()['ready']
            if not ready:
                time.sleep(.05)
        assert ready

    def status(self):
        return json.loads(self.request('/api/v1/status.json', browser=False).read())

    def webhook(self, repo, tag='v1.0.0'):
        self.tags.setdefault(repo, {})[tag] = 'b' * 40
        raw = json.dumps(dict(ref='refs/tags/' + tag, created=True, deleted=False, after='a' * 40, repository=dict(full_name=repo))).encode()
        response = self.request('/api/v1/github/webhook', raw, {'Content-Type': 'application/json', 'X-GitHub-Event': 'push',
            'X-GitHub-Delivery': repo + tag, 'X-Hub-Signature-256': 'sha256=' + hmac.new(b'fixture-webhook', raw, hashlib.sha256).hexdigest()})
        return response.status

    def workflow_path(self, values):
        return ('/api/v1/publications/' + values['publication_id'] if 'publication_id' in values
                else '/api/v1/requests/' + values['request_id'])

    def plan_bytes(self, values):
        response = self.request(self.workflow_path(values) + '/attempts/' + values['attempt'], browser=False)
        assert response.status == 200 and response.headers['Cache-Control'] == 'no-store'
        return response.read()

    def plan(self, values):
        return json.loads(self.plan_bytes(values))

    def callback(self, values, route, **fields):
        raw = urllib.parse.urlencode(fields).encode()
        stamp = str(self.now())
        signature = hmac.new(b'fixture-callback', stamp.encode() + b'.' + raw, hashlib.sha256).hexdigest()
        response = self.request(self.workflow_path(values) + '/' + route, raw,
            {'Content-Type': 'application/x-www-form-urlencoded', 'X-CoreCI-Timestamp': stamp,
             'X-CoreCI-Signature-256': 'sha256=' + signature}, browser=False)
        return response

    def restart(self):
        self.process.terminate()
        self.process.wait(timeout=10)
        self.process = subprocess.Popen(self.command, env=self.env, stdout=self.log, stderr=subprocess.STDOUT)
        return None

    def close(self):
        self.release.set()
        self.dispatch_response.set()
        self.process.terminate()
        self.process.wait(timeout=10)
        self.log.close()
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=5)
        log = (self.work / 'daemon.log').read_text(errors='replace')
        assert not any(token in log for token in self.tokens), 'credential appeared in fixture log'
        assert not any(cookie.value in log for cookie in getattr(self, 'jar', ()) if cookie.value), 'browser credential appeared in fixture log'
        assert not any(secret in log for secret in ('fixture-installation', 'fixture-webhook', 'fixture-callback')), 'credential appeared in fixture log'


@contextmanager
def fixture(build, **options):
    root = Path(build) / 'tmp'
    root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='dashboard-', dir=root) as directory:
        value = Fixture(build, directory, **options)
        try:
            yield value
        except Exception:
            value.log.flush()
            (value.build / 'fixture-failure.log').write_bytes((value.work / 'daemon.log').read_bytes())
            raise
        finally:
            value.close()
