from http.server import BaseHTTPRequestHandler, HTTPServer
import os
from pathlib import Path
import ssl
import sys
import tempfile
import threading

from persistence import run


def main():
    build = Path(sys.argv[1]).resolve()
    parent = Path("/tmp/codex/build/core-ci/https")
    parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="case-", dir=parent) as temporary:
        root = Path(temporary)
        authority = root / "ca"
        authority.mkdir()
        key, cert = root / "key.pem", authority / "cert.pem"
        run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1", "-subj", "/CN=localhost",
             "-addext", "subjectAltName=DNS:localhost", "-keyout", str(key), "-out", str(cert)])
        run(["openssl", "rehash", str(authority)])
        requests = []

        class Handler(BaseHTTPRequestHandler):
            def do_POST(self):
                assert self.headers.get("Authorization") == "Bearer fixture-token"
                assert self.rfile.read(int(self.headers["Content-Length"])) == b"fixture-body"
                requests.append(self.path)
                status = int(self.path[1:])
                self.send_response(status)
                self.send_header("Content-Length", "16")
                if status == 302:
                    self.send_header("Location", "https://unrelated.invalid/never-follow")
                self.end_headers()
                self.wfile.write(b"fixture-response")

            def log_message(self, *args):
                pass

        server = HTTPServer(("127.0.0.1", 0), Handler)
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(cert, key)
        server.socket = context.wrap_socket(server.socket, server_side=True)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        binary = root / "isolated-https-test"
        binary.symlink_to(build / "https-contract-test")
        config = root / "empty.conf"
        config.touch()
        try:
            for host, status, expected in [("localhost", 200, 200), ("localhost", 302, 302), ("localhost", 500, 500), ("127.0.0.1", 200, 0)]:
                environment = dict(os.environ, SSL_CERT_DIR=str(authority), CI_TEST_URL=f"https://{host}:{server.server_port}/{status}",
                                   CI_TEST_STATUS=str(expected))
                run([str(binary), "--config-file", str(config), "--log-file", str(root / "native.log"), "--quiet=true"], env=environment)
            assert requests == ["/200", "/302", "/500"], requests
            print("Native HTTPS: verified certificate/host, authenticated POST bytes, exact response, no redirect, remote error, wrong-host rejection passed")
        finally:
            server.shutdown()
            thread.join(5)
            server.server_close()


if __name__ == "__main__":
    main()
