"""Verified OpenWrt SDK preparation, used only by trusted GitHub worker code."""

import os
from pathlib import Path
import resource
import shutil
import subprocess
import tarfile

from contract import file_hash, output, require, run


def prepare(target, environment, destination, archive=None):
    from environment import sdk

    print("SDK metadata start", flush=True)
    expected = sdk(target, environment["release"])
    require(
        all(environment[key] == value for key, value in expected.items()),
        "SDK identity differs from official release metadata",
    )
    print("SDK metadata verified", flush=True)
    destination = Path(destination)
    require(not destination.exists(), "SDK destination must be new")
    destination.mkdir()
    supplied = Path(archive) if archive is not None else None
    archive = destination / "sdk.archive"
    if supplied is not None:
        require(
            supplied.is_file() and not supplied.is_symlink() and supplied.stat().st_size <= 1024**3,
            "supplied SDK requires a regular file within 1 GiB",
        )
        shutil.copyfile(supplied, archive)
    else:
        subprocess.run(
            [
                "curl",
                "--fail",
                "--silent",
                "--show-error",
                "--max-filesize",
                str(1024**3),
                "--output",
                str(archive),
                expected["sdk"]["url"],
            ],
            check=True,
            timeout=600,
        )
    require(
        archive.stat().st_size <= 1024**3 and file_hash(archive) == expected["sdk"]["sha256"], "SDK checksum mismatch"
    )
    print("SDK archive verified", flush=True)
    extracted = destination / "extracted"
    extract(archive, extracted, expected["sdk"]["url"].endswith(".zst"))
    print("SDK extraction completed", flush=True)
    roots = list(extracted.iterdir())
    require(len(roots) == 1 and roots[0].is_dir() and not roots[0].is_symlink(), "SDK archive requires one directory")
    root = roots[0]
    run(["make", "--no-print-directory", "-C", str(root), "defconfig"], preexec_fn=lambda: os.umask(0o022))
    query = destination / "query.mk"
    query.write_text("TOPDIR:=$(CURDIR)\nSDK:=1\ninclude $(TOPDIR)/rules.mk\ninclude $(TOPDIR)/include/version.mk\n")
    values = {}
    for variable in "ARCH_PACKAGES VERSION_NUMBER BOARD SUBTARGET STAGING_DIR TARGET_CROSS TOOLCHAIN_DIR".split():
        values[variable] = output(
            ["make", "--no-print-directory", "--silent", "-C", str(root), "-f", str(query.resolve()), "val." + variable]
        )
        require(bool(values[variable]) and "\n" not in values[variable], "SDK cannot report " + variable)
    require(
        values["ARCH_PACKAGES"] == target["arch"]
        and values["VERSION_NUMBER"] == environment["release"]
        and values["BOARD"] + "/" + values["SUBTARGET"] == target["build"]["target"],
        "extracted SDK target identity mismatch",
    )
    compiler = (Path(values["TOOLCHAIN_DIR"]) / "bin" / (values["TARGET_CROSS"] + "gcc")).resolve()
    require(compiler.is_relative_to(root.resolve()) and compiler.is_file(), "SDK compiler escapes verified SDK")
    identity = {
        "triple": output([str(compiler), "-dumpmachine"]),
        "version": output([str(compiler), "-dumpfullversion"]),
        "compiler_sha256": file_hash(compiler),
    }
    require("toolchain" not in environment or environment["toolchain"] == identity, "SDK toolchain changed")
    environment["toolchain"] = identity
    staging = Path(values["STAGING_DIR"]).resolve()
    require(staging.is_relative_to(root.resolve()), "SDK staging path escapes SDK")
    require(
        environment.get("staging_dir", str(staging.relative_to(root.resolve())))
        == str(staging.relative_to(root.resolve())),
        "SDK staging identity changed",
    )
    environment["staging_dir"] = str(staging.relative_to(root.resolve()))
    if target["build"]["format"] == "apk":
        tool = root / "staging_dir/host/bin/apk"
        require(
            tool.is_file() and output([str(tool), "--version"]).startswith("apk-tools 3."), "SDK requires apk-tools v3"
        )
        os.environ["CORE_CI_APK"] = str(tool)
    return root, staging.relative_to(root.resolve())


def extract(archive, destination, zstd=False):
    archive = Path(archive)
    temporary = archive.with_suffix(".tar")
    if zstd:
        with temporary.open("wb") as outgoing:
            subprocess.run(
                ["zstd", "--decompress", "--stdout", str(archive)],
                stdout=outgoing,
                check=True,
                timeout=300,
                preexec_fn=lambda: resource.setrlimit(resource.RLIMIT_FSIZE, (4 * 1024**3, 4 * 1024**3)),
            )
        archive = temporary
    try:
        with tarfile.open(archive, "r:*") as opened:
            members = []
            size = 0
            for index, item in enumerate(opened):
                size += item.size
                require(
                    index < 200000 and size <= 4 * 1024**3 and len(item.name) <= 1024,
                    "SDK archive exceeds resource bounds",
                )
                # Host bindings belong to the SDK builder; OpenWrt regenerates them for its current host.
                host_command = Path(item.name).parts[1:-1] == ("staging_dir", "host", "bin")
                if not (item.issym() and item.linkname.startswith("/") and host_command):
                    members.append(item)
            # Python's standard data filter rejects traversal, external links, devices and special files.
            opened.extractall(destination, members=members, filter="data")
    finally:
        temporary.unlink(missing_ok=True)
