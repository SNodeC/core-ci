"""Focused public-boundary checks for SNodeC deployment interoperability."""

import copy
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import unittest
from types import SimpleNamespace
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "deployment/worker/core_ci"))
import native
import native_container
import openwrt_container
from native_container import package
from contract import canonical, digest, package_metadata, versions
from status_update import projection, snapshot

TEMP = Path("/tmp/codex/build/core-ci/worker-interoperability")
TEMP.mkdir(parents=True, exist_ok=True)


def work(identity, index, tag, state, prerequisites):
    return {
        "id": identity, "index": index, "source": {"repository": "SNodeC/snode.c", "tag": tag},
        "target": {"id": "debian-trixie-amd64", "distribution": "debian", "suite": "trixie", "arch": "amd64",
                   "build": {"format": "deb"}},
        "prerequisites": prerequisites, "active": {"id": identity + "-attempt", "revision": index + 1, "state": state},
    }


class Worker(unittest.TestCase):
    def test_independent_ci_checkout(self):
        with tempfile.TemporaryDirectory(dir=TEMP) as temporary:
            root = Path(temporary)
            source, ci = root / "source", root / "definition"
            document = {"schema": 1, "package": "fixture", "title": "Fixture", "depends_on": [],
                        "build": {"native": "ci/build.sh"}, "matrix": [
                            {"id": "debian", "family": "native", "distribution": "debian", "suite": "trixie",
                             "arch": "amd64", "build": {"format": "deb"}}]}
            revisions = []
            for directory, definition in [(source, {"schema": 99}), (ci, document)]:
                (directory / "ci").mkdir(parents=True)
                (directory / "ci/packages.json").write_text(canonical(definition))
                (directory / "application.txt").write_text("tag source" if directory == source else "CI branch source")
                (directory / "ci/build.sh").write_text("#!/bin/sh\nprintf '%s' CI-HOOK\n")
                subprocess.run(["git", "init", "--quiet", str(directory)], check=True)
                subprocess.run(["git", "-C", str(directory), "add", "."], check=True)
                subprocess.run(["git", "-C", str(directory), "-c", "user.name=Fixture", "-c",
                                "user.email=fixture@example.invalid", "commit", "--quiet", "-m", "Fixture"], check=True)
                revisions.append(subprocess.check_output(["git", "-C", str(directory), "rev-parse", "HEAD"], text=True).strip())
            blob = subprocess.check_output(["git", "hash-object", str(ci / "ci/packages.json")], text=True).strip()
            tooling = subprocess.check_output(["git", "-C", str(ROOT), "rev-parse", "HEAD"], text=True).strip()
            selected = {"schema": 1, "execution": "a" * 64, "attempt": "a" * 64 + ":1", "revision": 1,
                        "artifact_name": "fixture", "source": {
                            "repository": "Fixture/source", "repository_id": 42, "tag": "v1.0.0", "commit": revisions[0],
                            "definition_revision": revisions[1], "definition_blob_sha": blob,
                            "deployment": {"worker": {"tooling_sha": tooling}},
                            **{key: document[key] for key in ("package", "title", "depends_on", "build")}},
                        "target": document["matrix"][0], "dependencies": []}
            assignment = root / "assignment.json"
            environment = root / "environment.json"
            environment.write_text(json.dumps({"assignment_sha256": digest(selected), "environment": {
                "resolved_image": "fixture@sha256:" + "b" * 64, "platform": "linux/amd64"}}))
            arguments = SimpleNamespace(assignment=str(assignment), assignment_sha256=digest(selected),
                                        environment=str(environment), source=str(source), definition=str(ci), output=str(root / "output"))
            real_run = subprocess.run
            observed = []

            class Prepared(Exception):
                pass

            def command(argv, **options):
                if argv[:3] == ["docker", "image", "rm"]:
                    result = subprocess.CompletedProcess(argv, 0)
                elif argv[0] == "docker":
                    # All real identity checks and input preparation precede Docker.
                    copies = sorted(root.glob("core-ci-*"))
                    self.assertEqual(len(copies), 1)
                    work = copies[0]
                    self.assertEqual((work / "source/application.txt").read_text(), "tag source")
                    self.assertEqual((work / "definition/application.txt").read_text(), "CI branch source")
                    self.assertEqual(json.loads((work / "definition/ci/packages.json").read_text()), document)
                    self.assertFalse((work / "source/.git").exists())
                    self.assertFalse((work / "definition/.git").exists())
                    observed.append(argv)
                    raise Prepared()
                else:
                    result = real_run(argv, **options)
                return result

            with patch.dict(os.environ, {"GITHUB_REPOSITORY": "Fixture/source", "GITHUB_REPOSITORY_ID": "42",
                                         "GITHUB_RUN_ATTEMPT": "1", "RUNNER_TEMP": str(root)}):
                assignment.write_text(canonical(selected))
                with patch("subprocess.run", side_effect=command), self.assertRaises(Prepared):
                    native.build(arguments)
                self.assertEqual(len(observed), 1)
                for field, message in [("commit", "selected source"), ("definition_revision", "selected CI revision"),
                                       ("definition_blob_sha", "definition does not match")]:
                    invalid = copy.deepcopy(selected)
                    invalid["source"][field] = "f" * 40
                    assignment.write_text(canonical(invalid))
                    arguments.assignment_sha256 = digest(invalid)
                    with self.assertRaisesRegex(ValueError, message):
                        native.build(arguments)
                assignment.write_text(canonical(selected))
                arguments.assignment_sha256 = digest(selected)
                (ci / "ci/build.sh").unlink()
                (ci / "ci/build.sh").symlink_to(source / "ci/build.sh")
                with self.assertRaisesRegex(ValueError, "hook escapes selected CI checkout"):
                    native.build(arguments)

    def test_native_executes_ci_hook_against_tag_source(self):
        with tempfile.TemporaryDirectory(dir=TEMP) as temporary:
            root = Path(temporary)
            source, ci, work = root / "source", root / "definition", root / "work/source"
            source.mkdir()
            ci.mkdir()
            work.parent.mkdir()
            (source / "application.txt").write_text("tag source")
            (source / "hook.sh").write_text("exit 99\n")
            (ci / "hook.sh").write_text('set -eu\ncat "$SOURCE_DIR/application.txt" > "$SOURCE_DIR/result"\n'
                                        'cat "$CI_DIR/helper" >> "$SOURCE_DIR/result"\n')
            (ci / "helper").write_text("CI helper")
            selected = {"source": {"tag": "v1.0.0", "commit": "1" * 40, "package": "fixture",
                                   "build": {"native": "hook.sh"}}, "revision": 1}
            real_copy = native_container.shutil.copytree

            def copy_input(src, destination, *arguments, **options):
                result = real_copy(source if src == "/source" else src, destination, *arguments, **options)
                return result

            def path(value):
                result = {"/work/source": work, "/definition": ci}.get(value, Path(value))
                return result

            with patch.object(native_container, "Path", side_effect=path), patch("os.getuid", return_value=1000), \
                    patch("os.getgid", return_value=1000), patch("shutil.copytree", side_effect=copy_input):
                native_container.execute_hook(selected, {}, "deb", [])
            self.assertEqual((work / "result").read_text(), "tag sourceCI helper")
            self.assertEqual((source / "application.txt").read_text(), "tag source")
            self.assertFalse((source / "result").exists())

    def test_openwrt_executes_ci_hook_against_tag_source(self):
        with tempfile.TemporaryDirectory(dir=TEMP) as temporary:
            root = Path(temporary)
            for name in ["source", "definition", "sdk/staging", "prerequisites", "work"]:
                (root / name).mkdir(parents=True, exist_ok=True)
            (root / "source/application.txt").write_text("tag source")
            (root / "source/hook.sh").write_text("exit 99\n")
            (root / "definition/hook.sh").write_text(
                'set -eu\ncat "$SOURCE_DIR/application.txt" > "$SOURCE_DIR/result"\n'
                'cat "$CI_DIR/helper" >> "$SOURCE_DIR/result"\n')
            (root / "definition/helper").write_text("CI helper")
            selected = {"source": {"tag": "v1.0.0", "commit": "1" * 40, "package": "fixture",
                                   "build": {"openwrt": "hook.sh"}}, "revision": 1,
                        "target": {"arch": "x86_64", "build": {"format": "ipk"}}}
            real_copy = openwrt_container.shutil.copytree

            def path(value):
                result = root / str(value).lstrip("/")
                return result

            def copy_input(src, destination, *arguments, **options):
                result = real_copy(path(src) if str(src) in ["/sdk", "/source"] else src, destination, *arguments, **options)
                return result

            def assignment(value):
                self.assertEqual(value, "/assignment.json")
                return selected

            def environment(value):
                self.assertEqual(value, "/environment.json")
                return {"staging_dir": "staging"}

            with patch.object(openwrt_container, "Path", side_effect=path), \
                    patch.object(openwrt_container, "assignment", side_effect=assignment), \
                    patch.object(openwrt_container, "read_json", side_effect=environment), \
                    patch("os.getuid", return_value=1000), patch("os.getgid", return_value=1000), \
                    patch("shutil.copytree", side_effect=copy_input):
                openwrt_container.main()
            self.assertEqual((root / "work/source/result").read_text(), "tag sourceCI helper")
            self.assertFalse((root / "source/result").exists())

    def test_initial_stream_and_presentation(self):
        graph = {"repositories": {"SNodeC/snode.c": {"definition": {"title": "SNode.C", "matrix": [
            {"distribution": "debian", "suite": "trixie", "arch": "amd64", "build": {"format": "deb"}},
        ]}}}, "executions": {}, "ready": False}
        records = [work("core/~", 0, "v2.10.0", "building", []),
                   work("later", 1, "v2.9.0", "waiting", ["core/~"])]
        messages = [graph]
        for record in records:
            token = record["id"].replace("~", "~0").replace("/", "~1")
            messages.append([{"op": "add", "path": "/executions/" + token, "value": record}])
        messages.append([{"op": "replace", "path": "/ready", "value": True}])
        wire = b": heartbeat\n\n" + b"".join(b"data: " + json.dumps(value).encode() + b"\n\n" for value in messages)

        class Handler(BaseHTTPRequestHandler):
            def do_GET(self):
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.end_headers()
                self.wfile.write(wire)

            def log_message(self, *arguments):
                pass

        server = HTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever)
        thread.start()
        try:
            actual = snapshot("http://127.0.0.1:" + str(server.server_port) + "/events")
        finally:
            server.shutdown()
            thread.join()
            server.server_close()
        self.assertTrue(actual["ready"])
        self.assertEqual(actual["executions"], {record["id"]: record for record in records})
        os.environ.update(FEED_REPOSITORY="SNodeC/Packages", FEED_REPOSITORY_ID="1404446502", FEED_BRANCH="main")
        facts = projection(actual)
        self.assertEqual(facts["projects"][0]["versions"], ["v2.10.0", "v2.9.0"])
        self.assertEqual(facts["executions"][0]["blockers"], ["core/~"])
        self.assertIsNone(facts["executions"][0]["publication_verified_utc"])
        retired = copy.deepcopy(actual)
        retired["executions"]["later"]["active"]["retirement"] = {"remote_outcome": "unknown"}
        self.assertEqual(projection(retired)["executions"][0]["activity"], "Retired")

    def test_real_cpack_deb_and_rpm(self):
        with tempfile.TemporaryDirectory(dir=TEMP) as temporary:
            root = Path(temporary)
            (root / "payload").write_text("isolated CPack fixture\n")
            (root / "PackageConfig.cmake").write_text(
                'set(CPACK_DEBIAN_FULL_PACKAGE_DEPENDS "snodec-common (= ${CPACK_PACKAGE_VERSION})")\n'
                'set(CPACK_RPM_FULL_PACKAGE_REQUIRES "snodec-common = ${CPACK_PACKAGE_VERSION}-${CPACK_RPM_PACKAGE_RELEASE}")\n'
            )
            (root / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.20)\nproject(snodec_fixture VERSION 2.1.0 LANGUAGES NONE)\n"
                "install(FILES payload DESTINATION share/snodec COMPONENT common)\n"
                "install(FILES payload DESTINATION share/snodec/full COMPONENT full)\n"
                "set(CPACK_PACKAGE_NAME snodec)\nset(CPACK_PACKAGE_VERSION 2.1.0)\n"
                "set(CPACK_PACKAGE_DESCRIPTION_SUMMARY \"isolated fixture\")\n"
                "set(CPACK_PACKAGE_CONTACT fixture@example.invalid)\n"
                "set(CPACK_DEBIAN_PACKAGE_MAINTAINER fixture)\nset(CPACK_RPM_PACKAGE_LICENSE MIT)\n"
                "set(CPACK_DEB_COMPONENT_INSTALL ON)\nset(CPACK_RPM_COMPONENT_INSTALL ON)\n"
                "set(CPACK_COMPONENTS_GROUPING IGNORE)\nset(CPACK_DEBIAN_FULL_PACKAGE_NAME snodec)\n"
                "set(CPACK_RPM_FULL_PACKAGE_NAME snodec)\n"
                'set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/PackageConfig.cmake")\ninclude(CPack)\n'
            )
            build = root / "build"
            subprocess.run(["cmake", "-S", str(root), "-B", str(build)], check=True)
            for kind in ("deb", "rpm"):
                with self.subTest(format=kind):
                    destination = root / kind
                    destination.mkdir()
                    target = root / "target.json"
                    target.write_text(json.dumps({"build": {"format": kind}}))
                    os.environ.update(TARGET_JSON=str(target), OUTPUT_DIR=str(destination),
                                      SOURCE_VERSION="v2.1.0", PACKAGE_REVISION="7")
                    package(str(build))
                    declared = json.loads((destination / "outputs.json").read_text())
                    self.assertEqual(len(declared["packages"]), 2)
                    for item in declared["packages"]:
                        actual = package_metadata(destination / item["file"], kind)
                        self.assertEqual(actual["version"], versions("v2.1.0", 7, kind)["full"])
                        self.assertEqual(actual["name"], item["component"])
                        if actual["name"] == "snodec":
                            self.assertIn(versions("v2.1.0", 7, kind)["full"], str(actual["dependencies"]))


if __name__ == "__main__":
    unittest.main()
