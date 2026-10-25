"""Trusted container bootstrap; the source hook runs as an unprivileged child."""

import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

from contract import assignment, output, read_json, require, versions


def main():
    selected = assignment("/assignment.json")
    target = selected["target"]
    package_format = target["build"]["format"]
    values = {}
    for line in Path("/etc/os-release").read_text().splitlines():
        if "=" in line:
            name, value = line.split("=", 1)
            values[name] = shlex.split(value)[0] if value else ""
    require(values.get("ID") == target["distribution"], "image distribution differs from target")
    require(
        target["suite"] in (values.get("VERSION_ID"), values.get("VERSION_CODENAME")),
        "image series differs from target",
    )
    arch = (
        output(["dpkg", "--print-architecture"]) if package_format == "deb" else output(["rpm", "--eval", "%{_arch}"])
    )
    require(arch == target["arch"], "image package architecture differs from target")
    package_files = []
    expected = []
    for directory in Path("/prerequisites").iterdir():
        receipt = read_json(directory / "receipt.json")
        for package in receipt["packages"]:
            package_files.append(str(directory / package["file"]))
            expected.append(package)
    # Dependency installation happens while preparing the disposable build image, before hooks.
    if "--install" in sys.argv:
        if package_files:
            if package_format == "deb":
                subprocess.run(["apt-get", "update"], check=True, timeout=300)
                subprocess.run(
                    ["apt-get", "install", "--yes", "--no-install-recommends", *package_files], check=True, timeout=300
                )
            else:
                subprocess.run(
                    ["dnf", "install", "--assumeyes", "--setopt=install_weak_deps=False", *package_files],
                    check=True,
                    timeout=300,
                )
    else:
        execute_hook(selected, target, package_format, expected)


def execute_hook(selected, target, package_format, expected):
    require(
        os.getuid() == 1000 and os.getgid() == 1000, "source hooks require the isolated unprivileged container user"
    )
    for package in expected:
        actual = (
            output(["dpkg-query", "--show", "--showformat=${Version}", package["name"]])
            if package_format == "deb"
            else output(["rpm", "-q", "--queryformat", "%{VERSION}-%{RELEASE}", package["name"]])
        )
        require(actual == package["version"], "package manager substituted a prerequisite version")
    source = Path("/work/source")
    shutil.copytree("/source", source, symlinks=True)
    hook = (source / selected["source"]["build"]["native"]).resolve()
    require(hook.is_relative_to(source) and hook.is_file(), "hook escapes selected source checkout")
    encoded = versions(selected["source"]["tag"], selected["revision"], package_format)
    environment = {
        "PATH": "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
        "HOME": "/tmp",
        "LANG": "C.UTF-8",
        "SOURCE_DIR": str(source),
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
    subprocess.run(
        ["/bin/sh", str(hook)],
        cwd=source,
        env=environment,
        check=True,
        timeout=3000,
    )


if __name__ == "__main__":
    main()
