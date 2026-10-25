"""Native worker entry point. No signing credentials are accepted here."""

import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from contract import assignment, compatibility, download_feed, extract_archive, output, read_json, require, write_json


TOOLS = {
    "deb": "apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends build-essential python3 pkg-config ca-certificates",
    "rpm": "dnf install -y gcc gcc-c++ make rpm-build python3 pkgconf-pkg-config findutils tar gzip && dnf clean all",
}


def prepare_dependencies(selected, destination):
    destination.mkdir()
    for dependency in selected["dependencies"]:
        receipt = dependency["receipt"]
        base = receipt["feed"]["base_url"]
        require(
            receipt["execution"] == dependency["execution"] and receipt["attempt"] == dependency["attempt"],
            "prerequisite receipt mismatch",
        )
        require(receipt["compatibility"] == compatibility(selected), "prerequisite target incompatibility")
        root = destination / dependency["execution"]
        root.mkdir()
        write_json(root / "receipt.json", receipt)
        for package in receipt["packages"]:
            require("/" not in package["file"], "invalid prerequisite filename")
            download_feed(base, package["path"], root / package["file"], package)
        if receipt.get("development_export"):
            export = receipt["development_export"]
            require(export["compatibility"] == compatibility(selected), "incompatible development export")
            archive = root / "development.tar.gz"
            download_feed(base, export["path"], archive, export)
            extract_archive(archive, root / "development", "tar")


def build(arguments):
    selected = assignment(arguments.assignment, arguments.assignment_sha256)
    require(selected["source"]["repository"] == os.environ["GITHUB_REPOSITORY"], "source repository mismatch")
    require(
        str(selected["source"]["repository_id"]) == os.environ["GITHUB_REPOSITORY_ID"], "source repository ID mismatch"
    )
    require(os.environ["GITHUB_RUN_ATTEMPT"] == "1", "reruns require an explicit controller retry")
    require(selected["target"]["family"] == "native", "OpenWrt requires its SDK worker")
    source = Path(arguments.source).resolve()
    tooling = Path(__file__).resolve().parents[1]
    require(
        output(["git", "-C", str(source), "rev-parse", "HEAD"]) == selected["source"]["commit"],
        "checkout is not the selected source",
    )
    require(
        output(["git", "-C", str(tooling), "rev-parse", "HEAD"])
        == selected["source"]["deployment"]["worker"]["tooling_sha"],
        "tooling checkout mismatch",
    )
    definition = source / "ci/packages.json"
    blob = output(["git", "hash-object", str(definition)])
    require(blob == selected["source"]["definition_blob_sha"], "checkout definition does not match selection")
    document = read_json(definition)
    require(selected["target"] in document["matrix"], "selected target absent from committed definition")
    for key in ("package", "title", "depends_on", "build"):
        require(document[key] == selected["source"][key], "committed definition differs from assignment")
    hook = (source / selected["source"]["build"]["native"]).resolve()
    require(hook.is_relative_to(source) and hook.is_file(), "hook escapes selected source checkout")
    environment = selected["environment"]
    image = environment["resolved_image"]
    require(
        re.fullmatch(r"[a-z0-9][a-z0-9.:/_-]*@sha256:[a-f0-9]{64}", image) is not None,
        "worker requires digest-qualified image",
    )
    require(image.endswith("@" + environment["digest"]), "image identity mismatch")
    platform = selected["target"]["build"]["platform"]
    require(re.fullmatch(r"linux/[a-z0-9_]+(?:/[a-z0-9_]+)?", platform) is not None, "invalid image platform")
    package_format = selected["target"]["build"]["format"]
    require(package_format in TOOLS, "unsupported native format")
    destination = Path(arguments.output).resolve()
    require(not destination.exists(), "output destination must be new")
    destination.mkdir()
    destination.chmod(0o777)
    tag = "core-ci-build-" + selected["execution"] + ":" + str(selected["revision"])
    with tempfile.TemporaryDirectory(prefix="core-ci-", dir=os.environ["RUNNER_TEMP"]) as temporary:
        work = Path(temporary)
        write_json(work / "assignment.json", selected)
        write_json(work / "target.json", selected["target"])
        prepare_dependencies(selected, work / "prerequisites")
        # Only this copied tree enters the hook container. No .git credentials or host workspace.
        shutil.copytree(source, work / "source", ignore=shutil.ignore_patterns(".git"), symlinks=True)
        shutil.copytree(tooling / "core_ci", work / "core_ci")
        (work / "Dockerfile").write_text(
            "FROM "
            + image
            + "\nRUN "
            + TOOLS[package_format]
            + "\nCOPY core_ci /tool\nCOPY prerequisites /prerequisites\n"
            + "COPY assignment.json /assignment.json\nRUN python3 /tool/native_container.py --install\n"
        )
        try:
            subprocess.run(["docker", "pull", "--platform", platform, image], check=True, timeout=600)
            metadata = json.loads(output(["docker", "image", "inspect", image]))
            require(
                len(metadata) == 1 and metadata[0]["Id"] == environment["config_digest"],
                "Docker image configuration differs from pinned registry evidence",
            )
            # Hosted runner UID need not be the container UID. Every mounted input is read-only.
            for path in work.rglob("*"):
                if not path.is_symlink():
                    path.chmod(0o755 if path.is_dir() else path.stat().st_mode | 0o444)
            subprocess.run(
                [
                    "docker",
                    "build",
                    "--platform",
                    platform,
                    "--tag",
                    tag,
                    "--file",
                    str(work / "Dockerfile"),
                    str(work),
                ],
                check=True,
                timeout=1200,
            )
            mounts = [
                "--mount",
                "type=bind,source=" + str(tooling / "core_ci") + ",target=/tool,readonly",
                "--mount",
                "type=bind,source=" + str(work / "assignment.json") + ",target=/assignment.json,readonly",
                "--mount",
                "type=bind,source=" + str(work / "target.json") + ",target=/target.json,readonly",
                "--mount",
                "type=bind,source=" + str(work / "prerequisites") + ",target=/prerequisites,readonly",
            ]
            common = [
                "docker",
                "run",
                "--rm",
                "--user=1000:1000",
                "--platform",
                platform,
                "--cap-drop=ALL",
                "--security-opt=no-new-privileges",
                "--pids-limit=256",
                "--memory=4g",
                "--cpus=2",
            ]
            subprocess.run(
                common
                + mounts
                + [
                    "--read-only",
                    "--tmpfs",
                    "/tmp:rw,exec,size=2g,mode=1777",
                    "--tmpfs",
                    "/work:rw,exec,size=4g,mode=1777",
                    "--mount",
                    "type=bind,source=" + str(work / "source") + ",target=/source,readonly",
                    "--mount",
                    "type=bind,source=" + str(destination) + ",target=/output",
                    tag,
                    "python3",
                    "/tool/native_container.py",
                ],
                check=True,
                timeout=3600,
            )
            # Hook container has exited. No source process can race the independent validation.
            result = subprocess.run(
                common
                + [
                    "--network=none",
                    "--read-only",
                    "--tmpfs",
                    "/tmp:rw,size=64m,mode=1777",
                    "--tmpfs",
                    "/work:rw,size=1g,mode=1777",
                ]
                + mounts
                + [
                    "--mount",
                    "type=bind,source=" + str(destination) + ",target=/work/output,readonly",
                    tag,
                    "python3",
                    "-c",
                    "from contract import *; s=assignment('/assignment.json'); "
                    "print(canonical(validate_outputs(s, '/work/output', read_json('/work/output/outputs.json'))))",
                ],
                check=True,
                text=True,
                stdout=subprocess.PIPE,
                timeout=300,
            )
            manifest = json.loads(result.stdout)
            (destination / "outputs.json").unlink()
            write_json(destination / "manifest.json", manifest)
        finally:
            subprocess.run(
                ["docker", "image", "rm", tag], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
            )
