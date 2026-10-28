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


def main():
    directory = Path(sys.argv[1])
    directory.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=directory) as temporary:
        root = Path(temporary)
        preparation(root)
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
