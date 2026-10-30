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
from unittest.mock import patch


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
            import retention
            pruning = directory / (package_format + "-retention")
            shutil.copytree(feed, pruning)
            kept = pruning / receipt["packages"][0]["path"]
            preserved = kept.with_name("retained_" + kept.name)
            shutil.copyfile(kept, preserved)
            prior = contract.file_hash(preserved)
            objects = receipt["packages"] + ([receipt["development_export"]] if receipt["development_export"] else [])
            objects += [sign_native.file_identity(pruning, pruning / sign_native.receipt_path(request))]
            command = {**request, "kind": "retention", "objects": objects}
            corrupted = copy.deepcopy(command)
            corrupted["objects"][-1]["sha256"] = "0" * 64
            rejected(lambda: retention.update(corrupted, pruning, private, passphrase, public, directory, ""), "changed removal bytes accepted")
            assert all((pruning / item["path"]).exists() for item in objects), "partial deletion preceded complete validation"
            if package_format == "deb":
                other = directory / "other-architecture"
                (other / "DEBIAN").mkdir(parents=True)
                (other / "DEBIAN/control").write_text("Package: unrelated\nVersion: 1\nArchitecture: arm64\nMaintainer: Test <test@example.invalid>\nDescription: fixture\n")
                contract.run(["dpkg-deb", "--build", str(other), str(kept.parent / "unrelated_1_arm64.deb")], stdout=subprocess.DEVNULL)
                with sign_native.signing_key(private, passphrase, public, fingerprint, directory) as (_, gpg, keyring):
                    sign_native.apt_indexes(pruning, request["target"]["distribution"], request["target"]["suite"], "arm64", gpg, keyring)
                other_index = pruning / request["target"]["distribution"] / "dists" / request["target"]["suite"] / "main/binary-arm64/Packages"
                other_bytes = other_index.read_bytes()
                wrong_arch = {**command, "target": {**command["target"], "arch": "arm64"}}
                rejected(lambda: retention.update(wrong_arch, pruning, private, passphrase, public, directory, ""), "foreign architecture deleted")
            linked = copy.deepcopy(command)
            linked["objects"] = [{**objects[0], "path": "exports/" + "a" * 64 + "/1/development.tar.gz"}]
            link = pruning / "exports" / ("a" * 64)
            link.parent.mkdir(exist_ok=True)
            link.symlink_to(kept.parent, target_is_directory=True)
            rejected(lambda: retention.update(linked, pruning, private, passphrase, public, directory, ""), "linked parent accepted")
            for bad in ({**objects[0], "size": 0}, {**objects[0], "size": objects[0]["size"] + 1}):
                rejected(lambda: retention.update({**command, "objects": [bad]}, pruning, private, passphrase, public, directory, ""),
                         "invalid removal size accepted")
            rejected(lambda: retention.update({**command, "objects": objects + objects[:1]}, pruning, private, passphrase, public, directory, ""),
                     "duplicate removal accepted")
            assert all((pruning / item["path"]).exists() for item in objects)
            metadata_feed = directory / (package_format + "-metadata-only")
            shutil.copytree(feed, metadata_feed)
            metadata = [item for item in objects if item not in receipt["packages"]]
            prior_bytes = {str(path.relative_to(metadata_feed)): path.read_bytes() for path in metadata_feed.rglob("*") if path.is_file()}
            with patch.object(retention, "signing_key") as signing, patch.object(retention, "apt_indexes") as apt, patch.object(
                retention, "rpm_indexes"
            ) as rpm:
                paths, removed = retention.update({**command, "objects": metadata}, metadata_feed, private, passphrase, public, directory, "")
                assert removed == paths == [item["path"] for item in metadata]
                assert not signing.called and not apt.called and not rpm.called
            assert {str(path.relative_to(metadata_feed)): path.read_bytes() for path in metadata_feed.rglob("*") if path.is_file()} == {
                name: data for name, data in prior_bytes.items() if name not in removed
            }
            # Distribution names that resemble metadata roots still require real package signing/indexing.
            for distribution in ("exports", "receipts"):
                named_feed = directory / (package_format + "-" + distribution)
                shutil.copytree(feed, named_feed)
                shutil.copytree(named_feed / request["target"]["distribution"], named_feed / distribution, dirs_exist_ok=True)
                shutil.rmtree(named_feed / request["target"]["distribution"])
                named_package = {**receipt["packages"][0], "path": distribution + "/" + receipt["packages"][0]["path"].split("/", 1)[1]}
                named_command = {**command, "target": {**command["target"], "distribution": distribution}, "objects": [named_package]}
                indexer = "apt_indexes" if package_format == "deb" else "rpm_indexes"
                with patch.object(retention, indexer, wraps=getattr(retention, indexer)) as indexed:
                    retention.update(named_command, named_feed, private, passphrase, public, directory, "")
                    indexed.assert_called_once()
            paths, removed = retention.update(command, pruning, private, passphrase, public, directory, "")
            assert removed == [item["path"] for item in objects] and all(not (pruning / name).exists() for name in removed)
            assert contract.file_hash(preserved) == prior and all((pruning / name).is_file() for name in set(paths) - set(removed))
            if package_format == "deb":
                assert other_index.read_bytes() == other_bytes and "arm64" in (other_index.parents[2] / "Release").read_text()
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
