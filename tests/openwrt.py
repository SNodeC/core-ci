"""Local archive/control boundaries only; SDK execution and signatures require real GitHub acceptance."""

import io
import copy
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
    archive = root / "supplied-tools"
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
            with patch.object(sign_openwrt, "verified_tools", return_value=fixture) as prepared, patch.object(
                sign_openwrt, "verify_manifest", return_value=manifest
            ), patch.dict(os.environ, {"CORE_CI_APK": str(apk), "CORE_CI_TOOLS_SHA256": "f" * 64}):
                receipt = sign_openwrt.update(selected, {}, {}, artifacts, feed, private, public, temporary, archive)
                prepared.assert_called_once_with(archive, "f" * 64, selected, environment)
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
                assert prepared.call_count == 1, "replay repeated tools verification"
                assert before == {path: contract.file_hash(path) for path in feed.rglob("*") if path.is_file()}
                recorded.append(receipt)
        assert recorded[0]["compatibility"]["sdk_sha256"] != recorded[1]["compatibility"]["sdk_sha256"]
        assert Path(recorded[0]["packages"][0]["path"]).parent == Path(recorded[1]["packages"][0]["path"]).parent

        import retention
        objects = recorded[0]["packages"]
        command = {**selected, "kind": "retention", "objects": objects, "environment": environment}
        preserved = feed / recorded[1]["packages"][0]["path"]
        retained_hash = contract.file_hash(preserved)
        with patch.object(sign_openwrt, "verified_tools", return_value=fixture), patch.dict(
            os.environ, {"CORE_CI_APK": str(apk), "CORE_CI_TOOLS_SHA256": "f" * 64}
        ):
            _, removed = retention.update(command, feed, private, None, public, temporary, archive)
        assert removed == [objects[0]["path"]] and not (feed / removed[0]).exists()
        assert contract.file_hash(preserved) == retained_hash
        index = feed / folder / ("Packages" if package_format == "ipk" else "packages.adb")
        assert Path(removed[0]).name not in index.read_text() and preserved.name in index.read_text()


def tools_boundary(root):
    # Synthetic ELF dependency responses exercise the production handoff, without executing SDK tools.
    source = root / "tool-sdk"
    selected = {"execution": "a" * 64, "attempt": "fixture", "target": {"build": {"format": "ipk"}}}
    identity = {"sdk": {"sha256": "b" * 64}, "toolchain": {"triple": "fixture"}}
    library = "staging_dir/host/lib/libfixture.so"
    for name in sdk.PUBLICATION_FILES["ipk"] | {library}:
        path = source / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"synthetic tool")
    with patch.object(sdk, "output", return_value="(NEEDED) [libfixture.so]"):
        bundle = root / "verified-tools"
        expected = sdk.publication_tools(source, selected, identity, bundle)
    original = contract.read_json(bundle / "tools.json")
    assert library in {item["path"] for item in original["files"]}
    assert len(original["files"]) == len(sdk.PUBLICATION_FILES["ipk"]) + 1
    for field, replacement in (("execution", "other"), ("attempt", "other"), ("target", {}),
                               ("environment", {}), ("assignment_sha256", "0" * 64), ("schema", 2)):
        document = copy.deepcopy(original)
        document[field] = replacement
        contract.write_json(bundle / "tools.json", document)
        rejects(lambda: sdk.verified_tools(bundle, contract.file_hash(bundle / "tools.json"), selected, identity))
    contract.write_json(bundle / "tools.json", original)
    rejects(lambda: sdk.verified_tools(bundle, "0" * 64, selected, identity))
    for kind in ("duplicate", "missing", "size", "hash", "traversal"):
        document = copy.deepcopy(original)
        if kind == "duplicate":
            document["files"].append(document["files"][0])
        elif kind == "missing":
            document["files"].pop()
        elif kind == "traversal":
            document["files"][0]["path"] = "../outside"
        else:
            document["files"][0][kind if kind == "size" else "sha256"] = 1 if kind == "size" else "0" * 64
        contract.write_json(bundle / "tools.json", document)
        rejects(lambda: sdk.verified_tools(bundle, contract.file_hash(bundle / "tools.json"), selected, identity))
    contract.write_json(bundle / "tools.json", original)
    extra = bundle / "unexpected"
    extra.write_bytes(b"extra")
    rejects(lambda: sdk.verified_tools(bundle, expected, selected, identity))
    extra.unlink()
    tool = bundle / original["files"][0]["path"]
    before = tool.read_bytes()
    tool.write_bytes(b"altered executable")
    rejects(lambda: sdk.verified_tools(bundle, expected, selected, identity))
    tool.write_bytes(before)
    assert all(not (bundle / item["path"]).stat().st_mode & 0o111 for item in original["files"])
    assert sdk.verified_tools(bundle, expected, selected, identity) == bundle
    assert all((bundle / item["path"]).stat().st_mode & 0o111 for item in original["files"])
    for index, needed in enumerate(("../outside", "missing.so")):
        with patch.object(sdk, "output", return_value="(NEEDED) [" + needed + "]"):
            rejects(lambda: sdk.publication_tools(source, selected, identity, root / ("bad-library-" + str(index))))
    with patch.object(sdk, "output", return_value="".join("(NEEDED) [lib" + str(i) + ".so]\n" for i in range(33))):
        for i in range(33):
            (source / ("staging_dir/host/lib/lib" + str(i) + ".so")).write_bytes(b"library")
        rejects(lambda: sdk.publication_tools(source, selected, identity, root / "too-many-tools"))
    large = source / "staging_dir/host/lib/runas.so"
    with large.open("wb") as opened:
        opened.truncate(64 * 1024**2 + 1)
    with patch.object(sdk, "output", return_value=""):
        rejects(lambda: sdk.publication_tools(source, selected, identity, root / "oversized-tools"))
    large.write_bytes(b"library")
    internal = source / "staging_dir/host/bin/mkhash"
    internal.unlink()
    internal.symlink_to(source / "staging_dir/host/bin/.mkhash.bin")
    with patch.object(sdk, "output", return_value=""):
        sdk.publication_tools(source, selected, identity, root / "internal-tools")
    assert not (root / "internal-tools/staging_dir/host/bin/mkhash").is_symlink()
    tool.unlink()
    missing = copy.deepcopy(original)
    missing["files"] = [item for item in missing["files"] if item["path"] != original["files"][0]["path"]]
    contract.write_json(bundle / "tools.json", missing)
    rejects(lambda: sdk.verified_tools(bundle, contract.file_hash(bundle / "tools.json"), selected, identity))
    contract.write_json(bundle / "tools.json", original)
    tool.symlink_to(source / original["files"][0]["path"])
    rejects(lambda: sdk.verified_tools(bundle, expected, selected, identity))
    selected["target"]["build"]["format"] = "apk"
    for name in sdk.PUBLICATION_FILES["apk"]:
        path = source / name
        if not path.exists():
            path.write_bytes(b"APK tool")
    with patch.object(sdk, "output", return_value=""):
        apk_tools = root / "apk-tools"
        apk_hash = sdk.publication_tools(source, selected, identity, apk_tools)
    with patch.dict(os.environ):
        sdk.verified_tools(apk_tools, apk_hash, selected, identity)
        assert os.environ["CORE_CI_APK"] == str(apk_tools / "staging_dir/host/bin/apk")
    selected["target"]["build"]["format"] = "ipk"
    escaped = source / "staging_dir/host/bin/usign"
    escaped.unlink()
    escaped.symlink_to(root / "synthetic-apk")
    with patch.object(sdk, "output", return_value=""):
        rejects(lambda: sdk.publication_tools(source, selected, identity, root / "escaped-tools"))

def tool_selectors(root):
    import validate
    selected = {"execution": "fixture", "attempt": "fixture", "target": {"family": "openwrt"}}
    argv = ["sdk.py", "--assignment", "fixture", "--assignment-sha256", "fixture", "--output", str(root / "selector-tools")]
    with patch.object(sys, "argv", argv), patch.object(sdk, "assignment", return_value=selected), patch.object(
        validate, "trusted_deployment"
    ), patch.dict(os.environ, {"EXECUTION": "fixture", "ATTEMPT": "fixture", "RUNNER_TEMP": str(root),
                              "GITHUB_OUTPUT": str(root / "tool-output")}), patch.object(sdk, "prepare") as prepare:
        for raw in (" " * 16385, "{}\n", "[]"):
            with patch.dict(os.environ, {"CORE_CI_SDK_ENVIRONMENT": raw}):
                rejects(sdk.main)
        assert not prepare.called
        with patch.dict(os.environ, {"CORE_CI_SDK_ENVIRONMENT": "{}"}), patch.object(
            sdk, "publication_tools", return_value="c" * 64
        ) as pack:
            prepare.return_value = (root, root)
            sdk.main()
            assert prepare.call_count == 1 and pack.call_count == 1
            assert (root / "tool-output").read_text() == "tools-sha256=" + "c" * 64 + "\n"
            prepare.side_effect = lambda target, identity, destination: (identity.update(changed=True), root)
            rejects(sdk.main)
            assert pack.call_count == 1

def main():
    directory = Path(sys.argv[1])
    directory.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=directory) as temporary:
        root = Path(temporary)
        preparation(root)
        publication_paths(root)
        tools_boundary(root)
        tool_selectors(root)
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
