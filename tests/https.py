"""Real native HTTPS clients; disposable CA, TCP/TLS endpoints, no live service."""
import collections
import os
from pathlib import Path
import socket
import ssl
import sys
import tempfile
import threading

from persistence import run


def main():
    build = Path(sys.argv[1]).resolve()
    parent = Path('/tmp/codex/build/core-ci/https')
    parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='case-', dir=parent) as temporary:
        root = Path(temporary)
        authority = root / 'ca'
        authority.mkdir()
        key, cert = root / 'key.pem', authority / 'cert.pem'
        run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1', '-subj', '/CN=localhost',
             '-addext', 'subjectAltName=DNS:localhost', '-keyout', str(key), '-out', str(cert)])
        run(['openssl', 'rehash', str(authority)])
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(cert, key)
        binary = root / 'isolated-https-test'
        binary.symlink_to(build / 'https-contract-test')
        config = root / 'empty.conf'
        config.touch()
        failures = []

        def case(name, mode, expected, connects, method='GET', host='localhost', second=False, trust=True):
            listener = socket.socket()
            listener.bind(('127.0.0.1', 0))
            port = listener.getsockname()[1]
            listener.listen()
            listener.settimeout(.2)
            stop = threading.Event()
            accepted = []
            requests = collections.Counter()
            errors = []

            def serve():
                while not stop.is_set():
                    try:
                        raw, _ = listener.accept()
                    except socket.timeout:
                        continue
                    except OSError:
                        break
                    accepted.append(1)
                    try:
                        raw.settimeout(3)
                        if mode == 'tls':
                            raw.close()
                        else:
                            with context.wrap_socket(raw, server_side=True) as peer:
                                data = b''
                                while b'\r\n\r\n' not in data:
                                    data += peer.recv(8192)
                                header, body = data.split(b'\r\n\r\n', 1)
                                assert b'Authorization:Bearer fixture-token' in header, header
                                while len(body) < len(b'fixture-body'):
                                    body += peer.recv(8192)
                                assert body == b'fixture-body', body
                                path = header.split(b' ')[1].split(b'?')[0]
                                requests[path] += 1
                                if mode == 'close' or mode == 'recover' and requests[path] <= 2:
                                    pass
                                elif mode == 'malformed':
                                    peer.sendall(b'not HTTP sentinel-private-body\r\n\r\n')
                                elif mode == 'partial':
                                    peer.sendall(b'HTTP/1.1 200 OK\r\nContent-Length: 16\r\n\r\ncut')
                                else:
                                    status = int(mode) if mode.isdigit() else 200
                                    response = (f'HTTP/1.1 {status} Fixture\r\nContent-Length: 16\r\n'
                                                'Retry-After: 7\r\nConnection: close\r\n')
                                    if status == 302:
                                        response += 'Location: https://unrelated.invalid/never-follow\r\n'
                                    peer.sendall(response.encode() + b'\r\nfixture-response')
                    except ssl.SSLError:
                        raw.close()  # Expected certificate/hostname rejection.
                    except Exception as error:
                        errors.append(str(error))
                        raw.close()

            thread = threading.Thread(target=serve, daemon=True)
            if mode == 'refused':
                listener.close()
            else:
                thread.start()
            log = root / f'{name}.log'
            env = dict(os.environ, SSL_CERT_DIR=str(authority if trust else root / 'empty-ca'), SSL_CERT_FILE=str(root / 'absent.pem'),
                       CI_TEST_URL=f'https://{host}:{port}/signed-private-path?sentinel-secret-query',
                       CI_TEST_STATUS=str(expected), CI_TEST_METHOD=method)
            if second:
                env['CI_TEST_SECOND_URL'] = f'https://{host}:{port}/another-private-path?sentinel-secret-query'
            try:
                command = [str(binary), '--config-file', str(config), '--log-file', str(log), '--log-level', '2', '--quiet=false', '--enforce-log-file=true']
                if os.getenv('CI_HTTPS_MEMCHECK'):
                    command = ['valgrind', '--leak-check=full', '--show-leak-kinds=definite,indirect',
                               '--errors-for-leak-kinds=definite,indirect', '--error-exitcode=91',
                               f'--log-file={root / (name + ".memcheck")}', *command]
                output = run(command, env=env)
                if os.getenv('CI_HTTPS_MEMCHECK'):
                    report = (root / (name + '.memcheck')).read_text()
                    assert 'ERROR SUMMARY: 0 errors' in report, report
                    Path(os.environ['CI_HTTPS_MEMCHECK'], name + '.memcheck').write_text(report)
                logged = log.read_text() if log.exists() else ''
                results = [line for line in output.splitlines() if line.startswith('RESULT ')]
                assert len(results) == (2 if second else 1), output
                for forbidden in ['signed-private-path', 'another-private-path', 'sentinel-secret-query',
                                  'fixture-token', 'sentinel-private-body', 'Authorization', 'HTTP 0']:
                    assert forbidden not in output + logged, (forbidden, output, logged)
                assert not errors, errors
                if mode != 'refused':
                    assert len(accepted) == connects, (name, accepted, output, logged)
                # ERR is visible at level 2, including attempts preceding success.
                transport_logs = [line for line in logged.splitlines() if 'GET https://localhost ' in line
                                  or 'POST https://localhost ' in line or 'GET https://127.0.0.1 ' in line]
                if mode in ['refused', 'tls', 'close', 'malformed', 'partial', 'recover'] or not trust or host == '127.0.0.1':
                    expected_logs = connects - (2 if second else 1) if mode == 'recover' else connects
                    assert len(transport_logs) == expected_logs, (name, transport_logs, output)
                if mode == 'tls' or not trust or host == '127.0.0.1':
                    assert all('tls: connection ended before HTTP request' in line for line in results), results
                if mode in ['close', 'partial']:
                    assert all('response: connection closed before HTTP response' in line for line in results), results
                if mode in ['malformed', '600']:
                    assert all('HTTP delivery/parse failure' in line for line in results), results
                print(f'{name}: {connects} transport attempts, {len(results)} final callback(s), safe context; ERR visibility checked')
            finally:
                stop.set()
                if thread.is_alive():
                    thread.join(4)
                listener.close()

        cases = [('connect-refused', 'refused', 0, 3), ('pre-handshake-close', 'tls', 0, 3),
                 ('response-close', 'close', 0, 3), ('recover', 'recover', 200, 3),
                 ('concurrent-independent', 'recover', 200, 6, 'GET', 'localhost', True),
                 ('post-no-reissue', 'close', 0, 1, 'POST'), ('malformed-latched', 'malformed', 0, 1),
                 ('mid-body-cut', 'partial', 0, 3), ('wrong-host', '200', 0, 3, 'GET', '127.0.0.1'),
                 ('untrusted-ca', '200', 0, 3, 'GET', 'localhost', False, False)]
        cases += [(f'final-{status}', str(status), status, 1) for status in [200, 226, 302, 403, 427, 429, 503]]
        # Native parser rejects 600 before reporting a response: terminal parse failure, no reissue.
        cases.append(('out-of-range-parser-rejection', '600', 0, 1))
        for args in cases:
            try:
                case(*args)
            except Exception as error:
                failures.append(f'{args[0]}: {error}')
        assert not failures, '\n'.join(failures)
        print('Native HTTPS bounded reissue, first-final-event latching, certificate checks and safe diagnostics passed')


if __name__ == '__main__':
    main()
