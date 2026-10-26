"""Synthetic GitHub TLS/API source-selection boundary; no live job acceptance."""

import argparse
import base64
import hashlib
import http.server
import json
import os
from pathlib import Path
import ssl
import subprocess
import tempfile
import threading

from lifecycle import run


def exercise(args, root):
    root = Path(root)
    key = root / "app.pem"
    tls_key = root / "tls-key.pem"
    certificate = root / "tls.pem"
    run(["openssl", "genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:2048", "-out", str(key)])
    key.chmod(0o600)
    run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1", "-keyout", str(tls_key), "-out", str(certificate), "-subj", "/CN=api.github.com", "-addext", "subjectAltName=DNS:api.github.com"])
    tls_key.chmod(0o600)
    config = root / "controller.conf"
    config.write_text('github.app-id=42\ngithub.installation-id=7\ngithub.allowed-owner="Owner"\n'
                      'github.repository=["Owner/example=99"]\ngithub.private-key-file="' + str(key) + '"\n')
    if args.case == "duplicate-grant":
        config.write_text(config.read_text().replace('github.repository=["Owner/example=99"]',
                                                   'github.repository=["Owner/example=99", "Owner/alias=99"]'))
    config.chmod(0o600)
    roots = {}
    directories = {}
    blobs = {}
    for letter in ("c", "d", "e"):
        commit = letter * 40
        document = {"schema": 1, "title": "from-" + letter, "package": "example", "depends_on": [], "build": {"native": "ci/native.sh"},
                    "matrix": [{"id": "row", "family": "native", "distribution": "custom", "suite": "series", "arch": "configured", "runner": "configured",
                                "build": {"image": "custom:series", "platform": "linux/configured", "format": "deb"}}]}
        content = json.dumps(document).encode() if args.case != "invalid-definition" else b'{"schema":17}'
        sha = hashlib.sha1(b"blob " + str(len(content)).encode() + b"\0" + content).hexdigest()
        tree = hashlib.sha1(("root-" + commit).encode()).hexdigest()
        directory = hashlib.sha1(("ci-" + commit).encode()).hexdigest()
        roots[tree] = directory
        directories[directory] = sha
        blobs[sha] = content
    errors = []
    seen = []
    selections = 0

    class GitHub(http.server.BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def send_json(self, status, body):
            data = json.dumps(body).encode()
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(data)
            self.close_connection = True

        def do_POST(self):
            body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
            if self.path != "/app/installations/7/access_tokens" or body != {"repository_ids": [99], "permissions": {"contents": "read"}}:
                errors.append("unexpected App scope")
            self.send_json(201, {"token": "synthetic-source-token", "expires_at": "2099-01-01T00:00:00Z"})

        def do_GET(self):
            nonlocal selections
            seen.append(self.path)
            if self.headers.get("Authorization") != "Bearer synthetic-source-token":
                errors.append("missing scoped read token")
            suffix = self.path.removeprefix("/repos/Owner/example")
            if suffix == "":
                self.send_json(200, {"id": 99, "full_name": "Owner/example"})
            elif suffix == "/tags?per_page=100&page=1":
                if args.case == "no-tags":
                    names = ["not-a-version"]
                elif args.case == "ambiguous":
                    names = ["v1.0.0+first", "1.0.0+second"]
                else:
                    names = ["v1.9.0", "v1.10.0", "v2.0.0-rc.1"] + ["not-a-version-" + str(n) for n in range(97)]
                self.send_json(200, [{"name": name} for name in names])
            elif suffix == "/tags?per_page=100&page=2":
                self.send_json(200, [{"name": "v2.0.0-rc.2"}])
            elif suffix == "/git/ref/tags/v2.0.0-rc.2":
                selections += 1
                self.send_json(200, {"ref": "refs/tags/v2.0.0-rc.2", "object": {"type": "commit", "sha": ("d" if selections == 1 else "e") * 40}})
            elif suffix == "/git/ref/tags/v0.9.0%2Bbuild":
                self.send_json(200, {"ref": "refs/tags/v0.9.0+build", "object": {"type": "tag", "sha": "a" * 40}})
            elif suffix in ("/git/tags/" + "a" * 40, "/git/tags/" + "b" * 40):
                first = suffix.endswith("a" * 40)
                self.send_json(200, {"sha": ("a" if first else "b") * 40, "object": {"type": "tag" if first else "commit", "sha": ("b" if first else "c") * 40}})
            elif suffix.startswith("/git/commits/") and suffix[13:] in ("c" * 40, "d" * 40, "e" * 40):
                commit = suffix[13:]
                self.send_json(200, {"sha": commit, "tree": {"sha": hashlib.sha1(("root-" + commit).encode()).hexdigest()}})
            elif suffix.startswith("/git/trees/") and suffix[11:] in roots:
                sha = suffix[11:]
                self.send_json(200, {"sha": sha, "truncated": False, "tree": [{"path": "ci", "type": "tree", "mode": "040000", "sha": roots[sha]}]})
            elif suffix.startswith("/git/trees/") and suffix[11:] in directories:
                sha = suffix[11:]
                self.send_json(200, {"sha": sha, "truncated": False, "tree": [{"path": "packages.json", "type": "blob", "mode": "120000" if args.case == "symlink" else "100644", "sha": directories[sha]}]})
            elif suffix.startswith("/git/blobs/") and suffix[11:] in blobs:
                sha = suffix[11:]
                content = blobs[sha] + (b"modified" if args.case == "modified-blob" else b"")
                self.send_json(200, {"sha": sha, "encoding": "base64", "content": base64.b64encode(content).decode()})
            else:
                errors.append("unexpected source request " + suffix)
                self.send_json(404, {})

    server = http.server.HTTPServer(("127.0.0.1", 0), GitHub)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certificate, tls_key)
    server.socket = context.wrap_socket(server.socket, server_side=True)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    environment = os.environ.copy()
    environment["CORE_CI_SOURCE_CASE"] = args.case
    try:
        result = subprocess.run([args.executable, "--config-file", str(config), "--log-level", "2", "github-http", "remote", "--host", "127.0.0.1", "--port", str(server.server_port), "tls", "--ca-cert", str(certificate)], env=environment, capture_output=True, text=True, timeout=20)
        if args.case == "duplicate-grant":
            assert result.returncode != 0 and "grant" in result.stderr.lower(), result.stdout + result.stderr
            assert seen == [], "invalid deployment grants caused GitHub reads"
        else:
            assert result.returncode == 0, result.stdout + result.stderr
        assert not errors, errors
        assert "synthetic-source-token" not in result.stdout + result.stderr
        assert not any("heads/" in path for path in seen), "source selection substituted a branch"
        if args.case == "success":
            assert selections == 2, "tag move was not resolved independently"
        elif args.case == "symlink":
            assert not any("/git/blobs/" in path for path in seen), "symlink definition was dereferenced"
        elif args.case == "invalid-tag":
            assert seen == [], "invalid direct tag caused GitHub reads"
        print(result.stdout.strip())
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=3)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True)
    parser.add_argument("--directory", required=True)
    parser.add_argument("--case", required=True)
    args = parser.parse_args()
    Path(args.directory).mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="source-", dir=args.directory) as temporary:
        exercise(args, temporary)
