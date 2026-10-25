"""Local OCI TLS boundary fixtures; no live build or package acceptance."""

import argparse
import hashlib
import http.server
import json
import os
from pathlib import Path
import ssl
import subprocess
import tempfile
import threading
import urllib.parse

from lifecycle import run


def exercise(args, root):
    root = Path(root)
    key = root / "tls-key.pem"
    certificate = root / "tls.pem"
    run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1", "-keyout", str(key), "-out", str(certificate),
         "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost,IP:127.0.0.1"])
    key.chmod(0o600)
    config = json.dumps({"os": "linux", "architecture": "arm64" if args.case == "wrong-platform" else "amd64"}).encode()
    digest = lambda body: "sha256:" + hashlib.sha256(body).hexdigest()
    descriptor = lambda body, media: {"digest": digest(body), "size": len(body), "mediaType": media}
    manifest = json.dumps({"schemaVersion": 2, "mediaType": "application/vnd.oci.image.manifest.v1+json",
                           "config": descriptor(config, "application/vnd.oci.image.config.v1+json"), "layers": []}).encode()
    entry = descriptor(manifest, "application/vnd.oci.image.manifest.v1+json")
    entry["urls"] = ["https://unapproved.invalid/ignored"]
    entry["data"] = "ignored embedded alternative"
    entry["platform"] = {"os": "linux", "architecture": "amd64"}
    if args.case == "wrong-size":
        entry["size"] += 1
    index = json.dumps({"schemaVersion": 2, "mediaType": "application/vnd.oci.image.index.v1+json",
                        "manifests": [entry, entry] if args.case == "ambiguous" else [entry]}).encode()
    errors = []
    seen = []
    downloads = []
    origin = ""
    redirect_origin = ""

    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def reply(self, status, data=b"", headers=None):
            self.send_response(status)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Connection", "close")
            for name, value in (headers or {}).items():
                self.send_header(name, value)
            self.end_headers()
            self.wfile.write(data)
            self.close_connection = True

        def do_GET(self):
            seen.append(self.path)
            if self.headers.get("Host") != origin.removeprefix("https://"):
                errors.append("wrong registry Host")
            parsed = urllib.parse.urlsplit(self.path)
            authorization = self.headers.get("Authorization")
            if parsed.path == "/token":
                if authorization is not None or urllib.parse.parse_qs(parsed.query) != {"scope": ["repository:library/sample:pull"], "service": ["fixture"]}:
                    errors.append("incorrect token scope or leaked authorization")
                self.reply(200, json.dumps({"token": "local-pull-token"}).encode())
            elif authorization is None:
                realm = "https://unapproved.invalid/token" if args.case == "unapproved-auth" else origin + "/token"
                scope = "repository:other:push" if args.case == "wrong-scope" else "repository:library/sample:pull"
                self.reply(401, headers={"WWW-Authenticate": 'Bearer realm="' + realm + '",service="fixture",scope="' + scope + '"'})
            elif authorization != "Bearer local-pull-token":
                errors.append("incorrect registry authorization")
                self.reply(403)
            elif parsed.path == "/v2/library/sample/manifests/series":
                data = manifest if args.case == "single" else index
                headers = {"Docker-Content-Digest": "sha256:" + "0" * 64} if args.case == "wrong-header" else {}
                self.reply(200, data, headers)
            elif parsed.path == "/v2/library/sample/manifests/" + digest(manifest):
                self.reply(200, manifest + (b" " if args.case == "modified-manifest" else b""))
            elif parsed.path == "/v2/library/sample/blobs/" + digest(config):
                if args.case in ("redirect", "unapproved-redirect"):
                    location = redirect_origin + "/config" if args.case == "redirect" else "https://unapproved.invalid/config"
                    self.reply(307, headers={"Location": location})
                else:
                    self.reply(200, config + (b" " if args.case == "modified-config" else b""))
            else:
                errors.append("unexpected registry request " + self.path)
                self.reply(404)

    class Download(Handler):
        def do_GET(self):
            downloads.append(self.path)
            if self.headers.get("Host") != redirect_origin.removeprefix("https://"):
                errors.append("wrong download Host")
            if self.headers.get("Authorization") is not None:
                errors.append("registry token leaked to a redirect origin")
            self.reply(200, config)

    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certificate, key)
    registry = http.server.HTTPServer(("127.0.0.1", 0), Handler)
    download = http.server.HTTPServer(("127.0.0.1", 0), Download)
    servers = [registry, download]
    threads = []
    for server in servers:
        server.socket = context.wrap_socket(server.socket, server_side=True)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        threads.append(thread)
    origin = "https://127.0.0.1:" + str(registry.server_port)
    # A different host exercises verification parameter reset between IP and DNS peers.
    redirect_origin = "https://localhost:" + str(download.server_port)
    settings = root / "settings.conf"
    settings.write_text('registry.default-registry="127.0.0.1:' + str(registry.server_port) + '"\n'
                        'registry.default-namespace="library"\n'
                        'registry.allowed-origin=["' + origin + '", "' + redirect_origin + '"]\n'
                        'registry.sdk-origin=["' + origin + '"]\n')
    environment = os.environ.copy()
    environment.update(CORE_CI_ENV_CASE=args.case, CORE_CI_ENV_SDK=origin + "/sdk.tar.zst", CORE_CI_ENV_MANIFEST=digest(manifest))
    try:
        result = subprocess.run([args.executable, "--config-file", str(settings), "--log-file", str(root / "framework.log"),
                                 "environment-http", "tls", "--ca-cert", str(certificate)],
                                env=environment, capture_output=True, text=True, timeout=15)
        assert result.returncode == 0, result.stdout + result.stderr
        assert not errors, errors
        assert "local-pull-token" not in result.stdout + result.stderr
        assert not any("sdk.tar" in path for path in seen), "controller downloaded an SDK instead of pinning its identity"
        assert seen.count("/token?scope=repository%3Alibrary%2Fsample%3Apull&service=fixture") <= 1, "unbounded authentication retry"
        if args.case in ("unapproved-auth", "wrong-scope"):
            assert len(seen) == 1, "unapproved authentication caused additional requests"
        if args.case == "redirect":
            assert downloads == ["/config"], downloads
        else:
            assert downloads == [], downloads
        print(result.stdout.strip())
    finally:
        for server in servers:
            server.shutdown()
            server.server_close()
        for thread in threads:
            thread.join(timeout=3)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True)
    parser.add_argument("--directory", required=True)
    parser.add_argument("--case", required=True)
    args = parser.parse_args()
    Path(args.directory).mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="registry-", dir=args.directory) as temporary:
        exercise(args, temporary)
