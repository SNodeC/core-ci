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
        elif '/contents/' in path:
            if path.endswith('registry.json'):
                state.requested.set()
                assert state.release.wait(20), 'declaration fixture not released'
                data = state.registry
            elif path.endswith('targets.json'):
                data = state.catalog
            else:
                repo = path.split('/repos/', 1)[1].split('/contents/', 1)[0]
                ref = urllib.parse.parse_qs(query.query)['ref'][0]
                assert ref == 'a' * 40 or ref in state.tags.get(repo, {}).values(), 'declaration not read at a tag commit'
                state.contents.append((repo, ref))
                data = state.sources[repo]
            self.respond(200, data)
        elif '/commits/' in path:
            self.respond(200, dict(sha='a' * 40))
        elif path.endswith('/tags'):
            repo = path.removeprefix('/repos/').removesuffix('/tags')
            self.respond(200, [dict(name=tag, commit=dict(sha=sha)) for tag, sha in state.tags.get(repo, {}).items()])
        elif '/actions/runs' in path:
            raise AssertionError('controller attempted removed job lookup')
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
            assert claims['iss'] == '123' and abs(claims['iat'] - (int(time.time()) - 60)) <= 3
            state.mints += 1
            self.respond(201, dict(token='fixture-installation', expires_at=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime(time.time() + 3600))))
        elif self.path == '/oauth2/token':
            data = state.codes.pop(fields['code'])
            assert b64(hashlib.sha256(fields['code_verifier'].encode()).digest()) == data['code_challenge']
            assert fields['redirect_uri'] == data['redirect_uri']
            now = int(time.time())
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
            self.respond(200, dict(access_token=access, id_token=sign('id')))
        elif self.path == '/oauth2/introspect':
            claims = state.tokens.get(fields.get('token'))
            active = bool(claims) and claims['sub'] not in state.revoked
            self.respond(200, dict(active=active, sub=claims['sub'], sid=claims['sid'], mfa_verified=True) if active else dict(active=False))
        elif self.path.endswith('/dispatches'):
            values = json.loads(raw)['inputs']
            if 'build_id' in values:
                rid = state.next_run
                state.next_run += 1
                main = 'publish' if 'publish.yml' in self.path else 'build'
                state.dispatches.put((rid, values, main))
            held = state.holds.get((values.get('build_id'), main))
            if held:
                held.wait(20)
            self.respond(204)
        else:
            self.respond(404)


class Fixture:
    def __init__(self, build, work, small=False, wrong_host=False, parser=()):
        self.build, self.work = Path(build), Path(work)
        self.codes, self.tokens = {}, {}
        self.next_run = 100
        self.tags, self.contents, self.api_requests, self.mints = {}, [], [], 0
        self.holds = {}
        self.subject, self.revoked = 'owner', set()
        self.token_lifetime = 3600
        self.dispatches = queue.Queue()
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
            self.registry = dict(feeds={f: dict(publisher='coreci-publish.yml') for f in ('f1', 'f2')},
                                 default_feed='f1', build='coreci-build.yml', repositories=['owner/' + p for p in 'abcde'])
            self.catalog = dict(targets=[dict(id=t, distribution='test', suite='stable', arch='test', build=dict(format='deb')) for t in ('t1', 't2')])
            self.sources = {'owner/' + p: dict(package=p, depends_on=q, matrix=[dict(id='t1', feed='f1')] +
                                           ([] if p == 'e' else [dict(id='t2', feed='f2')]))
                            for p, q in [('a', []), ('b', ['a']), ('c', ['a']), ('d', ['b', 'c']), ('e', ['d'])]}
        self.env = dict(os.environ, SSL_CERT_FILE=str(self.cert), CORECI_WEBHOOK_SECRET='fixture-webhook',
                        CORECI_CALLBACK_SECRET='fixture-callback',
                        CORECI_DB_PASSWORD=os.environ.get('CORECI_TEST_DB_PASSWORD', ''))
        subprocess.run([self.build / 'coreci-reset-store'], env=self.env, check=True, stdout=subprocess.DEVNULL)
        self.context = ssl.create_default_context(cafile=str(self.cert))
        self.server = ThreadingHTTPServer(('127.0.0.1', self.api_port), Provider)
        self.server.state = self
        tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        tls.load_cert_chain(self.cert, self.key_path)
        self.server.socket = tls.wrap_socket(self.server.socket, server_side=True)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.log = (self.work / 'daemon.log').open('w')
        command = [self.build / 'core-ci', '--registry', 'Fixture/Registry:ci/registry.json',
                   '--workflow-repository', 'Fixture/Workflows', '--target-catalog', 'ci/targets.json',
                   '--github-app-id', '123', '--github-installation-id', '42', '--github-app-key-file', self.key_path,
                   '--db-host', self.env['CORECI_TEST_DB_HOST'], '--db-port', self.env.get('CORECI_TEST_DB_PORT', '0'),
                   '--db-user', self.env['CORECI_TEST_DB_USER'], '--db-name', self.env['CORECI_TEST_DB_NAME'],
                   '--callback-base-url', self.app, '--github-api-url', self.api.replace('localhost', '127.0.0.1') if wrong_host else self.api, '--github-ca-cert', self.cert,
                   '--tls-cert', self.cert, '--tls-key', self.key_path, '--listen-address', '127.0.0.1', '--listen-port', str(self.app_port),
                   '--idp-base-url', self.api, '--idp-client-id', 'coreci-test', '--idp-public-key-path', self.public,
                   '--dashboard-users', 'owner', '--dashboard-redirect-port', str(self.redirect_port), '--max-concurrent-builds', '0', 'coreci']
        if parser:
            command += ['http', 'parser', *parser]
        self.command = [str(x) for x in command]
        self.process = subprocess.Popen(self.command, env=self.env, stdout=self.log, stderr=subprocess.STDOUT)
        if not wrong_host and not self.requested.wait(10):
            self.log.flush()
            (self.build / 'fixture-startup.log').write_bytes((self.work / 'daemon.log').read_bytes())
            self.close()
            raise AssertionError('controller did not start declaration load; startup log saved')
        self.jar = http.cookiejar.CookieJar()
        self.browser = urllib.request.build_opener(urllib.request.HTTPSHandler(context=self.context), urllib.request.HTTPCookieProcessor(self.jar))

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
        count = sum(len(p['matrix']) for p in self.sources.values())
        while not ready and time.monotonic() < deadline:
            value = json.loads(self.request('/api/v1/status.json').read())
            ready = len(value['units']) == count
            if not ready:
                time.sleep(.05)
        assert ready

    def webhook(self, repo, tag='v1.0.0'):
        self.tags.setdefault(repo, {})[tag] = 'b' * 40
        raw = json.dumps(dict(ref='refs/tags/' + tag, created=True, deleted=False, after='a' * 40, repository=dict(full_name=repo))).encode()
        response = self.request('/api/v1/github/webhook', raw, {'Content-Type': 'application/json', 'X-GitHub-Event': 'push',
            'X-GitHub-Delivery': repo + tag, 'X-Hub-Signature-256': 'sha256=' + hmac.new(b'fixture-webhook', raw, hashlib.sha256).hexdigest()})
        return response.status

    def report(self, rid, values, phase, outcome):
        fields = dict(phase=phase, status=outcome, run_id=str(rid), run_attempt='1', artifact='artifact', feed_commit='b' * 40, reason='fixture failure')
        raw = urllib.parse.urlencode(fields).encode()
        stamp = str(int(time.time()))
        signature = hmac.new(b'fixture-callback', stamp.encode() + b'.' + raw, hashlib.sha256).hexdigest()
        response = self.request('/api/v1/builds/' + urllib.parse.quote(values['build_id'], safe='') + '/report', raw,
            {'Content-Type': 'application/x-www-form-urlencoded', 'X-CoreCI-Timestamp': stamp, 'X-CoreCI-Signature-256': 'sha256=' + signature})
        assert response.status == 200

    def close(self):
        self.release.set()
        self.process.terminate()
        self.process.wait(timeout=10)
        self.log.close()
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=5)
        log = (self.work / 'daemon.log').read_text(errors='replace')
        assert not any(token in log for token in self.tokens), 'credential appeared in fixture log'
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
