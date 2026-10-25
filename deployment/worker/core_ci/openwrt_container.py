"""Unprivileged OpenWrt source hook; publication never imports or executes this module."""

import os
from pathlib import Path
import shutil
import subprocess

from contract import assignment, compatibility, package_metadata, read_json, require, versions


def main():
    require(os.getuid() == 1000 and os.getgid() == 1000, "OpenWrt hooks require the isolated build user")
    selected = assignment("/assignment.json")
    target = selected["target"]
    environment = read_json("/environment.json")
    root = Path("/work/sdk")
    shutil.copytree("/sdk", root, symlinks=True)
    staging = root / environment["staging_dir"]
    require(staging.resolve().is_relative_to(root) and staging.is_dir(), "invalid SDK staging directory")
    for directory in Path("/prerequisites").iterdir():
        receipt = read_json(directory / "receipt.json")
        require(
            receipt["compatibility"] == compatibility(selected, environment), "SDK prerequisite compatibility mismatch"
        )
        packages = root / "bin/packages" / target["arch"] / "core-ci-prerequisites"
        packages.mkdir(parents=True, exist_ok=True)
        for package in receipt["packages"]:
            actual = package_metadata(directory / package["file"], target["build"]["format"])
            require(
                all(actual[key] == package[key] for key in ("name", "version", "arch", "dependencies")),
                "prerequisite package metadata mismatch",
            )
            shutil.copyfile(directory / package["file"], packages / package["file"])
        if receipt.get("development_export"):
            require(
                receipt["development_export"]["compatibility"] == compatibility(selected, environment),
                "SDK development export mismatch",
            )
            # Own include/lib/share files only, already archive-checked by the trusted wrapper.
            for file in (directory / "development").rglob("*"):
                if file.is_file():
                    destination = staging / "usr" / file.relative_to(directory / "development")
                    require(
                        destination.resolve().is_relative_to(root) and not destination.exists(),
                        "development export would replace SDK or another prerequisite's files",
                    )
            shutil.copytree(directory / "development", staging / "usr", dirs_exist_ok=True)
    source = Path("/work/source")
    shutil.copytree("/source", source, symlinks=True)
    hook = (source / selected["source"]["build"]["openwrt"]).resolve()
    require(hook.is_relative_to(source) and hook.is_file(), "OpenWrt hook escapes selected checkout")
    encoded = versions(selected["source"]["tag"], selected["revision"], target["build"]["format"])
    values = {
        "PATH": os.environ["PATH"],
        "HOME": "/work",
        "SOURCE_DIR": str(source),
        "SDK_DIR": str(root),
        "SDK_STAGING_DIR": str(staging),
        "TARGET_JSON": "/target.json",
        "SOURCE_VERSION": selected["source"]["tag"],
        "PACKAGE_REVISION": str(selected["revision"]),
        "PACKAGE_VERSION": encoded["full"],
        "PACKAGE_UPSTREAM_VERSION": encoded["version"],
        "PACKAGE_RELEASE": encoded["release"],
        "PACKAGE_NAMESPACE": selected["source"]["package"],
        "PREREQUISITES_DIR": "/prerequisites",
        "OUTPUT_DIR": "/output",
    }
    subprocess.run(["/bin/sh", str(hook)], cwd=source, env=values, check=True, timeout=3600)


if __name__ == "__main__":
    main()
