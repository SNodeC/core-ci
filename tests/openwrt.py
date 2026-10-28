"""Local archive/control boundaries only; SDK execution and signatures require real GitHub acceptance."""

import io
from contextlib import redirect_stdout
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "deployment/worker/core_ci"))
import contract
import sdk
import environment
import sign_openwrt
from sign_native import publish_key


def rejects(operation):
    rejected = False
    try:
        operation()
    except (ValueError, tarfile.TarError):
        rejected = True
    assert rejected, "unsafe SDK/input was accepted"


def preparation(root):
    # Exercise configuration, make-variable queries and subprocess ownership without compiling packages.
    fixture = root / "sdk-fixture"
    (fixture / "toolchain/bin").mkdir(parents=True)
    (fixture / "include").mkdir()
    (fixture / "staging").mkdir()
    compiler = fixture / "toolchain/bin/configured-gcc"
    compiler.write_text(
        '#!/bin/sh\ncase "$1" in -dumpmachine) echo configured-linux;; -dumpfullversion) echo 1.2.3;; esac\n'
    )
    compiler.chmod(0o755)
    (fixture / "Makefile").write_text('defconfig:\n\ttest "$$(umask)" = 0022\n\ttouch .config\n')
    (fixture / "include/version.mk").write_text("VERSION_NUMBER:=3.4.5\n")
    (fixture / "rules.mk").write_text(
        "include $(TOPDIR)/.config\nARCH_PACKAGES:=configured\nBOARD:=board\nSUBTARGET:=subtarget\n"
        "STAGING_DIR:=$(CURDIR)/staging\nTARGET_CROSS:=configured-\nTOOLCHAIN_DIR:=$(CURDIR)/toolchain\n"
        "val.%:\n\t@echo $($*)\n"
    )
    archive = root / "sdk-fixture.tar.gz"
    with tarfile.open(archive, "w:gz") as opened:
        opened.add(fixture, arcname="sdk")
    expected = {
        "release": "3.4.5",
        "sdk": {"url": "https://sdk.example/sdk.tar.gz", "sha256": contract.file_hash(archive)},
    }
    target = {"arch": "configured", "build": {"target": "board/subtarget", "format": "ipk"}}
    original_run = subprocess.run

    def commands(arguments, **options):
        result = None
        if arguments[0] == "curl":
            shutil.copyfile(archive, arguments[arguments.index("--output") + 1])
        else:
            result = original_run(arguments, **options)
        return result

    mask = os.umask(0o077)
    try:
        with patch.object(environment, "sdk", return_value=expected), patch.object(
            sdk.subprocess, "run", side_effect=commands
        ):
            resolved = {**expected}
            directory, staging = sdk.prepare(target, resolved, root / "prepared")
            assert (directory / ".config").stat().st_mode & 0o777 == 0o644
            assert staging == Path("staging")
            assert resolved["toolchain"]["compiler_sha256"] == contract.file_hash(compiler)
            assert resolved["toolchain"]["triple"] == "configured-linux"
            original_hash = contract.file_hash(archive)
            messages = io.StringIO()
            with patch.object(environment, "sdk", return_value=expected) as metadata, patch.object(
                sdk.subprocess, "run", wraps=original_run
            ) as executed, redirect_stdout(messages):
                supplied = {**expected}
                second, other_staging = sdk.prepare(target, supplied, root / "supplied", archive)
                assert supplied == resolved and other_staging == staging
                assert (second / ".config").is_file() and contract.file_hash(archive) == original_hash
                assert metadata.call_count == 1 and not any(call.args[0][0] == "curl" for call in executed.call_args_list)
            assert messages.getvalue().splitlines() == ["SDK metadata start", "SDK metadata verified",
                                                       "SDK archive verified", "SDK extraction completed"]
            link = root / "linked-sdk"
            link.symlink_to(archive)
            oversized = root / "oversized-sdk"
            with oversized.open("wb") as sparse:
                sparse.truncate(1024**3 + 1)
            with patch.object(sdk, "extract", side_effect=AssertionError("invalid SDK extracted")), patch.object(
                sdk.shutil, "copyfile", side_effect=AssertionError("invalid SDK copied")
            ):
                for index, invalid in enumerate((link, fixture, oversized, root / "missing-sdk")):
                    rejects(lambda: sdk.prepare(target, expected, root / ("invalid-supplied-" + str(index)), invalid))
            corrupted = root / "corrupted-sdk"
            corrupted.write_bytes(b"not the official archive")
            with patch.object(sdk, "extract", side_effect=AssertionError("wrong hash extracted")):
                rejects(lambda: sdk.prepare(target, expected, root / "wrong-supplied", corrupted))
        assert os.umask(0o077) == 0o077, "SDK configuration changed its parent's restrictive umask"
    finally:
        os.umask(mask)


def publication_paths(root):
    # Synthetic SDK tools prove layout, indexing and replay; they do not prove package signatures.
    fixture = root / "publisher-sdk"
    (fixture / "staging_dir/host/bin").mkdir(parents=True)
    (fixture / "scripts").mkdir()
    usign = fixture / "staging_dir/host/bin/usign"
    usign.write_text("#!/usr/bin/env python3\nimport pathlib,sys\na=sys.argv[1:]\n"
                     "if '-F' in a: print('synthetic-key')\n"
                     "elif '-S' in a: pathlib.Path(a[a.index('-x')+1]).write_bytes(b'synthetic-signature')\n"
                     "elif '-V' in a: assert pathlib.Path(a[a.index('-x')+1]).read_bytes()==b'synthetic-signature'\n")
    usign.chmod(0o755)
    indexer = fixture / "scripts/ipkg-make-index.sh"
    indexer.write_text('test "$1" = . || exit 1\nfor file in *.ipk; do printf "Filename: %s\\n" "$file"; done\n')
    apk = root / "synthetic-apk"
    apk.write_text("#!/usr/bin/env python3\nimport pathlib,sys\na=sys.argv[1:]\n"
                   "if 'adbsign' in a:\n p=pathlib.Path(a[-1]); p.write_bytes(p.read_bytes()+b'synthetic-signature')\n"
                   "elif 'mkndx' in a:\n p=pathlib.Path(a[a.index('--output')+1]); "
                   "p.write_text('\\n'.join(a[a.index('--output')+2:]))\n"
                   "elif 'verify' in a: assert pathlib.Path(a[-1]).stat().st_size > 0\n")
    apk.chmod(0o755)
    private = root / "publisher-private.pem"
    public = root / "publisher-public.pem"
    subprocess.run(["openssl", "genpkey", "-algorithm", "EC", "-pkeyopt", "ec_paramgen_curve:P-256",
                    "-out", str(private)], check=True, stderr=subprocess.PIPE)
    private.chmod(0o600)
    subprocess.run(["openssl", "pkey", "-in", str(private), "-pubout", "-out", str(public)],
                   check=True, stderr=subprocess.PIPE)
    feed = root / "publication-feed"
    feed.mkdir()
    archive = root / "supplied-sdk.archive"
    archive.write_bytes(b"synthetic SDK archive handoff")
    for package_format, suite in (("ipk", "24.10"), ("apk", "25.12")):
        recorded = []
        for revision in (1, 2):
            artifacts = root / (package_format + "-artifacts-" + str(revision))
            artifacts.mkdir()
            (artifacts / "package").write_bytes(b"synthetic unsigned package")
            environment = {"release": suite + ".1", "sdk": {"sha256": str(revision) * 64}, "toolchain": {}}
            manifest = {"environment": environment, "development_export": None,
                        "packages": [{"file": "package", "name": "example", "version": "1.0-r" + str(revision),
                                      "arch": "x86_64", "sha256": contract.file_hash(artifacts / "package")}]}
            contract.write_json(artifacts / "manifest.json", manifest)
            selected = {"execution": ("a" if package_format == "ipk" else "b") * 64,
                        "attempt": "synthetic:" + str(revision), "revision": revision, "dependencies": [],
                        "source": {"commit": "c" * 40, "tag": "v1.0.0", "deployment": {
                            "signing": {"usign_key_sha256": contract.file_hash(public),
                                        "apk_key_sha256": contract.file_hash(public)},
                            "feed": {"repository": "Example/feed", "branch": "main", "base_url": "https://feed.example"}}},
                        "target": {"family": "openwrt", "distribution": "openwrt", "suite": suite, "arch": "x86_64",
                                   "build": {"format": package_format, "target": "x86/64"}}}
            temporary = root / (package_format + "-temporary-" + str(revision))
            temporary.mkdir()
            with patch.object(sign_openwrt, "prepare", return_value=(fixture, fixture)) as prepared, patch.object(
                sign_openwrt, "verify_manifest", return_value=manifest
            ), patch.dict(os.environ, {"CORE_CI_APK": str(apk)}):
                receipt = sign_openwrt.update(selected, {}, {}, artifacts, feed, private, public, temporary, archive)
                prepared.assert_called_once_with(selected["target"], environment, temporary / "sdk", archive)
                folder = "openwrt/" + suite + "/x86_64/"
                filename = "example_1.0-r" + str(revision) + "_x86_64.ipk" if package_format == "ipk" else (
                    "example-1.0-r" + str(revision) + ".apk")
                assert receipt["packages"][0]["path"] == folder + filename
                assert (feed / folder / filename).is_file()
                indexes = ("Packages", "Packages.gz", "Packages.sig") if package_format == "ipk" else ("packages.adb",)
                assert {item["path"] for item in receipt["indexes"]} == {folder + name for name in indexes}
                if package_format == "ipk":
                    text = (feed / folder / "Packages").read_text()
                    assert all("Filename: example_1.0-r" + str(value) + "_x86_64.ipk" in text
                               for value in range(1, revision + 1))
                else:
                    assert all(str(feed / folder / ("example-1.0-r" + str(value) + ".apk")) in (
                        feed / folder / "packages.adb").read_text() for value in range(1, revision + 1))
                before = {path: contract.file_hash(path) for path in feed.rglob("*") if path.is_file()}
                assert sign_openwrt.update(selected, {}, {}, artifacts, feed, private, public, temporary, archive) == receipt
                assert prepared.call_count == 1, "replay repeated SDK preparation"
                assert before == {path: contract.file_hash(path) for path in feed.rglob("*") if path.is_file()}
                recorded.append(receipt)
        assert recorded[0]["compatibility"]["sdk_sha256"] != recorded[1]["compatibility"]["sdk_sha256"]
        assert Path(recorded[0]["packages"][0]["path"]).parent == Path(recorded[1]["packages"][0]["path"]).parent


def main():
    directory = Path(sys.argv[1])
    directory.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=directory) as temporary:
        root = Path(temporary)
        preparation(root)
        publication_paths(root)
        # A new SDK feed key must not replace the existing APK feed's trust file.
        feed = root / "feed"
        (feed / "keys").mkdir(parents=True)
        legacy = feed / "keys/apk.pem"
        legacy.write_bytes(b"retained legacy APK public key")
        before = legacy.read_bytes()
        selected = Path(__file__).resolve().parents[1] / "deployment/worker/keys/core-ci-apk.pem"
        publish_key(feed, selected)
        assert legacy.read_bytes() == before
        assert (feed / "keys/core-ci-apk.pem").read_bytes() == selected.read_bytes()
        publish_key(feed, selected)
        replacement = root / "apk.pem"
        replacement.write_bytes(selected.read_bytes())
        rejects(lambda: publish_key(feed, replacement))
        assert legacy.read_bytes() == before, "legacy APK key was replaced"
        with patch.dict(os.environ, {"RUNNER_TEMP": str(root)}):
            control = root / "control"
            control.write_text(
                "Package: selected-project\nVersion: 1.2.3-r7\nArchitecture: configured\nDepends: libc\nDescription: Local control fixture\n"
            )
            subprocess.run(["tar", "-czf", str(root / "control.tar.gz"), "-C", str(root), "./control"], check=True)
            subprocess.run(["tar", "-czf", str(root / "selected.ipk"), "-C", str(root), "./control.tar.gz"], check=True)
            assert contract.package_metadata(root / "selected.ipk", "ipk") == {
                "name": "selected-project",
                "version": "1.2.3-r7",
                "arch": "configured",
                "dependencies": "libc",
            }
            archive = root / "sdk.tar"
            with tarfile.open(archive, "w") as opened:
                item = tarfile.TarInfo("sdk/inside")
                item.size = 2
                opened.addfile(item, io.BytesIO(b"ok"))
                link = tarfile.TarInfo("sdk/link")
                link.type = tarfile.SYMTYPE
                link.linkname = "inside"
                opened.addfile(link)
                host_link = tarfile.TarInfo("sdk/staging_dir/host/bin/retained")
                host_link.type = tarfile.SYMTYPE
                host_link.linkname = "../../../inside"
                opened.addfile(host_link)
                # Official SDKs retain bindings to the machine that assembled them.
                for name, target in (
                    ("file", "/usr/bin/file"),
                    ("gcc", "/builder/openwrt/ccache_cc.sh"),
                    ("python3", "/opt/venv/bin/python3.9"),
                ):
                    binding = tarfile.TarInfo("sdk/staging_dir/host/bin/" + name)
                    binding.type = tarfile.SYMTYPE
                    binding.linkname = target
                    opened.addfile(binding)
            sdk.extract(archive, root / "valid")
            assert (root / "valid/sdk/link").read_text() == "ok"
            assert (root / "valid/sdk/staging_dir/host/bin/retained").read_text() == "ok"
            for name in ("file", "gcc", "python3"):
                assert not os.path.lexists(root / "valid/sdk/staging_dir/host/bin" / name)
            # Omitting host bindings must not let an oversized archive bypass the entry bound.
            with patch.object(sdk.tarfile, "open") as archive_open:
                bounded = archive_open.return_value.__enter__.return_value
                bounded.__iter__.return_value = (binding for _ in range(200001))
                rejects(lambda: sdk.extract(root / "oversized-sdk.tar.gz", root / "oversized"))
                bounded.extractall.assert_not_called()
            for name, target in (
                ("sdk/../../escape", ""),
                ("sdk/escape", "/etc/passwd"),
                ("sdk/staging_dir/host/bin/escape", "../../../../../escape"),
                ("sdk/staging_dir/host/lib/escape", "/etc/passwd"),
                ("sdk/staging_dir/host/bin/../../../../escape", "/etc/passwd"),
                ("sdk/staging_dir/toolchain/bin/escape", "/usr/bin/gcc"),
            ):
                with tarfile.open(archive, "w") as opened:
                    item = tarfile.TarInfo(name)
                    if target:
                        item.type = tarfile.SYMTYPE
                        item.linkname = target
                    opened.addfile(item)
                rejects(lambda: sdk.extract(archive, root / "invalid"))
            # A mismatched official checksum is rejected before any SDK extraction/tool execution.
            expected = {"release": "99.1.1", "sdk": {"url": "https://sdk.example/sdk.tar.xz", "sha256": "a" * 64}}

            def download(arguments, **options):
                Path(arguments[arguments.index("--output") + 1]).write_bytes(b"modified SDK bytes")

            with patch.object(environment, "sdk", return_value=expected), patch.object(
                sdk.subprocess, "run", side_effect=download
            ), patch.object(sdk, "extract", side_effect=AssertionError("unverified SDK extracted")):
                rejects(lambda: sdk.prepare({}, expected, root / "checksum"))
    print("PASS actual IPK controls and safe SDK archive/checksum boundaries; no SDK or APK execution claimed")


if __name__ == "__main__":
    main()
