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
                data = state.sources[repo]
            self.respond(200, data)
        elif '/commits/' in path:
            self.respond(200, dict(sha='a' * 40))
        elif path.endswith('/tags'):
            self.respond(200, [])
        elif '/actions/runs' in path:
            parts = path.split('/actions/runs', 1)
            if path.endswith('/jobs'):
                rid = int(parts[1].split('/')[1])
                run = state.runs[rid]
                self.respond(200, dict(jobs=[dict(name=run['main'], html_url=run['html_url'] + '/job/1',
                    status=run['status'], conclusion=run['conclusion'], started_at='2026-01-01T00:00:00Z',
                    completed_at='2026-01-01T00:01:00Z' if run['status'] == 'completed' else None)]))
            elif parts[1]:
                rid = int(parts[1].strip('/'))
                self.respond(200 if rid in state.runs else 404, state.runs.get(rid, {}))
            else:
                self.respond(200, dict(workflow_runs=list(state.runs.values())))
        elif path.startswith('/repos/'):
            repo = path.removeprefix('/repos/')
            self.respond(200, dict(full_name=repo, default_branch='master' if repo in ('SNodeC/snode.c', 'SNodeC/mqttsuite') else 'main'))
        else:
            self.respond(404)

    def do_POST(self):
        state = self.server.state
        raw = self.rfile.read(int(self.headers.get('Content-Length', '0')))
        fields = {k: v[0] for k, v in urllib.parse.parse_qs(raw.decode()).items()}
        if self.path == '/oauth2/token':
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
                rid = len(state.runs) + 100
                main = 'publish' if 'publish.yml' in self.path else 'build'
                repo = self.path.split('/repos/', 1)[1].split('/actions/', 1)[0]
                state.runs[rid] = dict(id=rid, run_attempt=1, path='.github/workflows/coreci-' + main + '.yml',
                    display_title=values['build_id'], html_url='https://github.com/' + repo + '/actions/runs/' + str(rid),
                    status='in_progress', conclusion=None, main=main)
                state.dispatches.put((rid, values, main))
            self.respond(204)
        else:
            self.respond(404)


class Fixture:
    def __init__(self, build, work):
        self.build, self.work = Path(build), Path(work)
        self.codes, self.tokens, self.runs = {}, {}, {}
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
        self.env = dict(os.environ, SSL_CERT_FILE=str(self.cert), CORECI_GITHUB_TOKEN='fixture-github', CORECI_WEBHOOK_SECRET='fixture-webhook',
                        CORECI_CALLBACK_SECRET='fixture-callback', CORECI_ADMIN_TOKEN='fixture-admin',
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
        command = [self.build / 'core-ci', '--registry', 'SNodeC/DistributionPackages:ci/registry.json',
                   '--db-host', self.env['CORECI_TEST_DB_HOST'], '--db-port', self.env.get('CORECI_TEST_DB_PORT', '0'),
                   '--db-user', self.env['CORECI_TEST_DB_USER'], '--db-name', self.env['CORECI_TEST_DB_NAME'],
                   '--callback-base-url', self.app, '--github-api-url', self.api, '--github-ca-cert', self.cert,
                   '--tls-cert', self.cert, '--tls-key', self.key_path, '--listen-address', '127.0.0.1', '--listen-port', str(self.app_port),
                   '--idp-base-url', self.api, '--idp-client-id', 'coreci-test', '--idp-public-key-path', self.public,
                   '--dashboard-users', 'owner', '--dashboard-redirect-port', str(self.redirect_port), '--max-concurrent-builds', '0', 'coreci']
        self.process = subprocess.Popen([str(x) for x in command], env=self.env, stdout=self.log, stderr=subprocess.STDOUT)
        if not self.requested.wait(10):
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
        while not ready and time.monotonic() < deadline:
            value = json.loads(self.request('/api/v1/status.json').read())
            ready = len(value['units']) == 178
            if not ready:
                time.sleep(.05)
        assert ready

    def webhook(self, repo, tag='v1.0.0'):
        raw = json.dumps(dict(ref='refs/tags/' + tag, created=True, deleted=False, after='a' * 40, repository=dict(full_name=repo))).encode()
        response = self.request('/api/v1/github/webhook', raw, {'Content-Type': 'application/json', 'X-GitHub-Event': 'push',
            'X-GitHub-Delivery': repo + tag, 'X-Hub-Signature-256': 'sha256=' + hmac.new(b'fixture-webhook', raw, hashlib.sha256).hexdigest()})
        return response.status

    def report(self, rid, values, phase, outcome):
        self.runs[rid].update(status='in_progress' if outcome == 'started' else 'completed',
                              conclusion=None if outcome == 'started' else 'success' if outcome == 'succeeded' else 'failure')
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
        assert not any(secret in log for secret in ('fixture-admin', 'fixture-github', 'fixture-webhook', 'fixture-callback')), 'credential appeared in fixture log'


@contextmanager
def fixture(build):
    root = Path(build) / 'tmp'
    root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='dashboard-', dir=root) as directory:
        value = Fixture(build, directory)
        try:
            yield value
        finally:
            value.close()
