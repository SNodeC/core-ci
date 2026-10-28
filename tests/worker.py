"""Real package tools at the build/artifact boundary; no GitHub/live acceptance claims."""

import argparse
import copy
import gzip
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import zipfile
from xml.etree import ElementTree


def rejected(operation, message):
    failed = False
    try:
        operation()
    except (ValueError, OSError, subprocess.CalledProcessError):
        failed = True
    assert failed, message


def selected(package_format):
    result = {
        "schema": 1,
        "execution": "1" * 64,
        "attempt": "1" * 64 + ":7",
        "revision": 7,
        "artifact_name": "core-ci-build-" + "1" * 64 + "-r7",
        "source": {
            "repository": "Example/project",
            "repository_id": 10,
            "name": "project",
            "package": "reference-project",
            "tag": "v1.2.3-rc.1+build",
            "commit": "a" * 40,
            "definition_revision": "a" * 40,
            "definition_blob_sha": "b" * 40,
            "deployment": {"worker": {"tooling_sha": "c" * 40}},
        },
        "target": {
            "id": "configured",
            "family": "native",
            "distribution": "configured",
            "suite": "configured",
            "arch": "amd64" if package_format == "deb" else "x86_64",
            "runner": "configured",
            "build": {"format": package_format, "platform": "linux/amd64", "image": "configured:tag"},
        },
        "dependencies": [],
    }
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", required=True)
    arguments = parser.parse_args()
    repository = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(repository / "deployment/worker/core_ci"))
    import contract
    import sign_native

    directory = Path(arguments.directory).resolve()
    shutil.rmtree(directory, ignore_errors=True)
    directory.mkdir(parents=True)
    os.environ["TMPDIR"] = str(directory)
    keys = directory / "test-keys"
    keys.mkdir(mode=0o700)
    try:
        home = keys / "gnupg"
        home.mkdir(mode=0o700)
        passphrase = keys / "passphrase"
        passphrase.write_text("")
        passphrase.chmod(0o600)
        public = keys / "public.asc"
        private = keys / "private.asc"
        try:
            subprocess.run(
                [
                    "gpg",
                    "--homedir",
                    str(home),
                    "--batch",
                    "--pinentry-mode",
                    "loopback",
                    "--passphrase-file",
                    str(passphrase),
                    "--quick-generate-key",
                    "core-ci isolated test <test@example.invalid>",
                    "rsa2048",
                    "sign",
                    "0",
                ],
                check=True,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                timeout=60,
            )
            fingerprint = sign_native.fingerprints(
                contract.output(["gpg", "--homedir", str(home), "--with-colons", "--list-keys"])
            )[0]
            for mode, file in (("--export", public), ("--export-secret-keys", private)):
                with file.open("wb") as outgoing:
                    subprocess.run(
                        [
                            "gpg",
                            "--homedir",
                            str(home),
                            "--batch",
                            "--pinentry-mode",
                            "loopback",
                            "--passphrase-file",
                            str(passphrase),
                            "--armor",
                            mode,
                            fingerprint,
                        ],
                        check=True,
                        stdout=outgoing,
                        stderr=subprocess.PIPE,
                    )
                file.chmod(0o600)
        finally:
            subprocess.run(
                ["gpgconf", "--homedir", str(home), "--kill", "all"],
                check=False,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
        feed = directory / "feed"
        feed.mkdir()
        # Reference hooks compile, execute their test binary and build actual native packages.
        for package_format in ("deb", "rpm"):
            request = selected(package_format)
            request["source"]["deployment"]["signing"] = {"gpg_fingerprint": fingerprint}
            request["source"]["deployment"]["feed"] = {
                "repository": "Example/feed",
                "repository_id": 30,
                "branch": "main",
                "base_url": "https://example.invalid/packages/",
            }
            bundle = directory / package_format
            bundle.mkdir()
            root = bundle / "packages"
            root.mkdir()
            target = directory / (package_format + ".json")
            contract.write_json(target, request["target"])
            encoded = contract.versions(request["source"]["tag"], request["revision"], package_format)
            environment = {
                **os.environ,
                "SOURCE_DIR": str(repository / "deployment/reference/native"),
                "TARGET_JSON": str(target),
                "OUTPUT_DIR": str(root),
                "PACKAGE_NAMESPACE": request["source"]["package"],
                "PACKAGE_VERSION": encoded["full"],
                "PACKAGE_UPSTREAM_VERSION": encoded["version"],
                "PACKAGE_RELEASE": encoded["release"],
            }
            with (directory / (package_format + "-build.log")).open("w") as log:
                subprocess.run(
                    ["sh", str(repository / "deployment/reference/native/ci/native.sh")],
                    env=environment,
                    check=True,
                    timeout=90,
                    stdout=log,
                    stderr=subprocess.STDOUT,
                )
            declared = contract.read_json(root / "outputs.json")
            resolved = {
                "image": "configured:tag",
                "platform": "linux/amd64",
                "digest": "sha256:" + "a" * 64,
                "config_digest": "sha256:" + "b" * 64,
                "resolved_image": "registry.example/image@sha256:" + "a" * 64,
            }
            manifest = contract.validate_outputs(request, root, declared, resolved)
            (root / "outputs.json").unlink()
            contract.write_json(root / "manifest.json", manifest)
            assert contract.verify_manifest(request, root) == manifest
            assert manifest["summary"]["tests"] == {"passed": 1, "failed": 0, "skipped": 0}
            assert manifest["summary"]["compilers"][0]["name"] == "cc"
            extra_tests = {
                "passed": 3,
                "failed": 0,
                "skipped": 1,
                "coverage_percent": 0,
                "report_url": "https://reports.example.test/results",
            }
            assert contract.build_summary({"tests": extra_tests})["tests"] == extra_tests
            for invalid in (
                {"tests": {"passed": True, "failed": 0, "skipped": 0}},
                {"tests": {"passed": -1, "failed": 0, "skipped": 0}},
                {"compilers": [{"name": "cc", "version": "bad\ntext"}]},
                {"state": "published"},
                {"tests": {**extra_tests, "coverage_percent": float("nan")}},
                {"tests": {**extra_tests, "report_url": "javascript:alert(1)"}},
            ):
                rejected(lambda: contract.build_summary(invalid), "invalid/authoritative summary accepted")
            spoofed = {**manifest, "build_job": {"id": 44, "run_id": 11, "run_attempt": 1}}
            contract.write_json(root / "manifest.json", spoofed)
            rejected(lambda: contract.verify_manifest(request, root), "source supplied verified job facts")
            facts = {"run": {"id": 11}, "artifacts": [{"job_id": 44}]}
            assert contract.verify_manifest(request, root, facts) == spoofed
            rejected(
                lambda: contract.verify_manifest(request, root, {"run": {"id": 99}, "artifacts": [{"job_id": 44}]}),
                "job timing from another execution accepted",
            )
            (bundle / "sdk.archive").write_bytes(b"sibling is not package authority")
            contract.write_json(root / "manifest.json", manifest)
            assert contract.verify_manifest(request, root) == manifest
            changed = copy.deepcopy(manifest)
            changed["environment"]["image"] = "another:image"
            contract.write_json(root / "manifest.json", changed)
            rejected(lambda: contract.verify_manifest(request, root), "environment disagreed with assigned image")
            contract.write_json(root / "manifest.json", manifest)
            wrong = copy.deepcopy(request)
            wrong["source"]["package"] = "another-repository"
            rejected(lambda: contract.validate_outputs(wrong, root, declared, resolved), "namespace escape accepted")
            wrong = copy.deepcopy(request)
            wrong["target"]["arch"] = "wrong-architecture"
            rejected(
                lambda: contract.validate_outputs(wrong, root, declared, resolved), "architecture mismatch accepted"
            )
            wrong = copy.deepcopy(manifest)
            wrong["packages"][0]["sha256"] = "0" * 64
            contract.write_json(root / "manifest.json", wrong)
            rejected(lambda: contract.verify_manifest(request, root), "modified package checksum accepted")
            contract.write_json(root / "manifest.json", manifest)
            (root / "extra-package").write_text("undeclared")
            rejected(lambda: contract.verify_manifest(request, root), "undeclared output accepted")
            (root / "extra-package").unlink()
            # Separate directories represent separate package-format executions.
            request["execution"] = ("2" if package_format == "rpm" else "1") * 64
            request["attempt"] = request["execution"] + ":7"
            for key, value in contract.evidence(request).items():
                manifest[key] = value
            contract.write_json(root / "manifest.json", manifest)
            build_result = {"run": {"id": 11}, "assignment_sha256": contract.digest(request), "artifacts": [{"id": 22}]}
            publication_run = {"id": 33}
            receipt = sign_native.update(
                request, build_result, publication_run, root, feed, private, passphrase, public, directory
            )
            package = receipt["packages"][0]
            target = request["target"]
            if package_format == "deb":
                folder = target["distribution"] + "/pool/" + target["suite"]
                filename = package["name"] + "_" + package["version"] + "_" + package["arch"] + ".deb"
                index = target["distribution"] + "/dists/" + target["suite"] + "/"
                assert {item["path"] for item in receipt["indexes"]} == {
                    index + path for path in ("Release", "Release.gpg", "InRelease",
                                             "main/binary-amd64/Packages", "main/binary-amd64/Packages.gz")}
                assert "Filename: " + folder.removeprefix(target["distribution"] + "/") + "/" + filename in (
                    feed / index / "main/binary-amd64/Packages").read_text()
            else:
                repository_root = target["distribution"] + "/" + target["suite"] + "/" + target["arch"]
                folder = repository_root + "/Packages"
                filename = package["name"] + "-" + package["version"] + "." + package["arch"] + ".rpm"
                indexes = {item["path"] for item in receipt["indexes"]}
                assert repository_root + "/repodata/repomd.xml.asc" in indexes
                repomd = ElementTree.parse(feed / repository_root / "repodata/repomd.xml")
                primary = feed / repository_root / repomd.find("{*}data[@type='primary']/{*}location").attrib["href"]
                xml = gzip.decompress(primary.read_bytes()) if primary.suffix == ".gz" else subprocess.check_output(
                    ["zstd", "-dc", str(primary)])
                locations = ElementTree.fromstring(xml).findall(".//{*}location")
                assert [location.attrib["href"] for location in locations] == ["Packages/" + filename]
            assert package["path"] == folder + "/" + filename
            assert receipt["summary"] == manifest["summary"] and receipt["source"]["tag"] == request["source"]["tag"]
            linked = {
                **request,
                "dependencies": [{"receipt": {**receipt, "dependencies": [{"private": "not copied"}]}}],
            }
            public_receipt = contract.publication_receipt(linked, {}, {}, manifest)
            assert public_receipt["dependencies"][0]["packages"] == receipt["packages"]
            assert "dependencies" not in public_receipt["dependencies"][0]
            compact_linked = copy.deepcopy(linked)
            compact_linked["dependencies"][0]["receipt"].pop("indexes")
            assert contract.publication_receipt(compact_linked, {}, {}, manifest) == {
                **public_receipt, "assignment_sha256": contract.digest(compact_linked)
            }
            before = {
                str(file.relative_to(feed)): contract.file_hash(file) for file in feed.rglob("*") if file.is_file()
            }
            assert (
                sign_native.update(
                    request, build_result, publication_run, root, feed, private, passphrase, public, directory
                )
                == receipt
            )
            assert before == {
                str(file.relative_to(feed)): contract.file_hash(file) for file in feed.rglob("*") if file.is_file()
            }, "identical replay changed published bytes"
            wrong = copy.deepcopy(request)
            wrong["source"]["deployment"]["signing"]["gpg_fingerprint"] = "F" * 40

            def wrong_key():
                with sign_native.signing_key(private, passphrase, public, "F" * 40, directory):
                    raise AssertionError("wrong signing authority accepted")

            rejected(wrong_key, "wrong signing key accepted")
            signed = feed / receipt["indexes"][-1]["path"]
            if package_format == "deb":
                assert signed.name == "InRelease"
                wrong_keyring = keys / "wrong-key.gpg"
                with wrong_keyring.open("wb") as outgoing:
                    subprocess.run(
                        [
                            "gpg",
                            "--batch",
                            "--output",
                            "-",
                            "--dearmor",
                            str(repository / "deployment/worker/keys/apt.asc"),
                        ],
                        stdout=outgoing,
                        stderr=subprocess.PIPE,
                        check=True,
                    )
                rejected(
                    lambda: contract.run(
                        ["gpgv", "--keyring", str(wrong_keyring), str(signed)],
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.PIPE,
                    ),
                    "wrong APT verification key accepted",
                )
                signed.write_bytes(signed.read_bytes().replace(b"Suite:", b"WRONG:", 1))
                with sign_native.signing_key(private, passphrase, public, fingerprint, directory) as (_, _, keyring):
                    rejected(
                        lambda: contract.run(
                            ["gpgv", "--keyring", str(keyring), str(signed)],
                            stdout=subprocess.DEVNULL,
                            stderr=subprocess.PIPE,
                        ),
                        "modified APT index signature accepted",
                    )
            else:
                package = feed / receipt["packages"][0]["path"]
                with package.open("r+b") as outgoing:
                    outgoing.seek(-1, 2)
                    outgoing.write(b"X")
                rpmdb = directory / "rpm-client-keyring"
                contract.run(["rpmkeys", "--dbpath", str(rpmdb), "--import", str(public)], stderr=subprocess.PIPE)
                rejected(
                    lambda: contract.run(
                        ["rpmkeys", "--dbpath", str(rpmdb), "--checksig", str(package)],
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.PIPE,
                    ),
                    "modified RPM signature/digest accepted",
                )
                rejected(
                    lambda: sign_native.retained_receipt(request, build_result, publication_run, feed),
                    "modified retained package accepted",
                )
        # Verify the canonical general prerelease codec with the actual dpkg and RPM comparators.
        tags = [
            "0.9.9",
            "1.0.0-0",
            "1.0.0-1",
            "1.0.0-2",
            "1.0.0-10",
            "1.0.0-A",
            "1.0.0-Z",
            "1.0.0-a",
            "1.0.0-a-",
            "1.0.0-a0",
            "1.0.0-alpha",
            "1.0.0-alpha.0",
            "1.0.0-alpha.1",
            "1.0.0-alpha.2",
            "1.0.0-alpha.10",
            "1.0.0-alpha.A",
            "1.0.0-alpha.a",
            "1.0.0-alpha.beta",
            "1.0.0-beta",
            "1.0.0-rc.1",
            "1.0.0",
            "1.0.1-0",
            "1.0.1",
            "2.0.0",
        ]
        for left, right in zip(tags, tags[1:]):
            a = contract.versions(left, 7, "deb")["full"]
            b = contract.versions(right, 1, "deb")["full"]
            subprocess.run(["dpkg", "--compare-versions", a, "lt", b], check=True)
            a = contract.versions(left, 7, "rpm")["full"]
            b = contract.versions(right, 1, "rpm")["full"]
            comparison = contract.output(["rpm", "--eval", '%{lua:print(rpm.vercmp("' + a + '", "' + b + '"))}'])
            assert comparison == "-1", (left, right, a, b, comparison)
        assert contract.versions("v1.0.0+build", 1, "deb") == contract.versions("1.0.0", 1, "deb")
        rejected(lambda: contract.versions("1.0.0-01", 1, "deb"), "invalid SemVer accepted")
        rejected(lambda: contract.versions("1.0.0", 0, "rpm"), "unreserved revision accepted")
        for name in ("../escape", "/absolute", "a/../../escape", "a/./b", "a//b", "a\\b"):
            archive = directory / "unsafe.zip"
            with zipfile.ZipFile(archive, "w") as zipped:
                zipped.writestr(name, "must not escape")
            rejected(
                lambda: contract.extract_archive(archive, directory / "extracted", "zip"), "unsafe ZIP path accepted"
            )
            assert not (directory / "extracted").exists()
        archive = directory / "link.tar.gz"
        with tarfile.open(archive, "w:gz") as exported:
            link = tarfile.TarInfo("include/escape")
            link.type = tarfile.SYMTYPE
            link.linkname = "/etc/passwd"
            exported.addfile(link)
        rejected(
            lambda: contract.extract_archive(archive, directory / "extracted", "tar"), "unsafe export link accepted"
        )
        print("Real DEB/RPM build, signing/indexing, immutable replay and adversarial artifact/signature checks passed")
    finally:
        shutil.rmtree(keys, ignore_errors=True)


if __name__ == "__main__":
    main()
