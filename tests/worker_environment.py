"""Worker resolution boundaries with tool responses supplied by fixtures, not live Docker."""

import copy
import json
from pathlib import Path
import sys
import tempfile
import zipfile
import shutil
from unittest.mock import patch
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "deployment/worker/core_ci"))
import environment
from contract import compatibility, digest, file_hash
import github


def rejects(operation):
    rejected = False
    try:
        operation()
    except ValueError:
        rejected = True
    assert rejected, "invalid worker environment accepted"


def retry_evidence(directory):
    selected = {
        "source": {
            "repository": "Example/source",
            "repository_id": 11,
            "deployment": {
                "controller_app_bot_id": 42,
                "source_workflow": {"path": ".github/workflows/core-ci-build.yml", "blob_sha": "b" * 40},
            },
        }
    }
    earlier = {
        "run": {"id": 100, "workflow_commit": "c" * 40, "workflow_blob_sha": "b" * 40},
        "attempt": "a" * 64 + ":1",
        "assignment_sha256": "d" * 64,
        "artifact_name": "assigned-environment",
    }
    selected["target"] = {"family": "native"}
    selected["execution"] = "a" * 64
    selected["attempt"] = "a" * 64 + ":2"
    selected["environment_from"] = [earlier]
    prefix = "/repos/Example/source"
    environment_identity = {
        "image": "configured:tag",
        "platform": "linux/amd64",
        "digest": "sha256:" + "e" * 64,
        "resolved_image": "registry.example/image@sha256:" + "e" * 64,
        "config_digest": "sha256:" + "f" * 64,
    }
    with tempfile.TemporaryDirectory(dir=directory) as temporary:
        root = Path(temporary)
        archive = root / "recorded.zip"
        with zipfile.ZipFile(archive, "w") as zipped:
            zipped.writestr(
                "environment.json",
                json.dumps({"assignment_sha256": earlier["assignment_sha256"], "environment": environment_identity}),
            )
        item = {
            "id": 101,
            "name": earlier["artifact_name"],
            "expired": False,
            "workflow_run": {"id": 100},
            "digest": "sha256:" + file_hash(archive),
            "size_in_bytes": archive.stat().st_size,
        }
        fixtures = {
            prefix: {"id": 11},
            prefix
            + "/actions/runs/100/attempts/1": {
                "id": 100,
                "repository": {"id": 11},
                "head_repository": {"id": 11},
                "actor": {"id": 42},
                "event": "workflow_dispatch",
                "status": "completed",
                "conclusion": "failure",
                "run_attempt": 1,
                "head_sha": "c" * 40,
                "path": ".github/workflows/core-ci-build.yml",
                "display_title": "core-ci build " + earlier["attempt"] + " " + earlier["assignment_sha256"],
            },
            prefix + "/contents/.github/workflows/core-ci-build.yml?ref=" + "c" * 40: {"sha": "b" * 40},
            prefix + "/actions/runs/100/artifacts?per_page=100": {"total_count": 1, "artifacts": [item]},
            prefix
            + "/actions/runs/100/attempts/1/jobs?per_page=100": {
                "total_count": 1,
                "jobs": [
                    {
                        "name": "build / core-ci package " + earlier["attempt"],
                        "run_id": 100,
                        "run_attempt": 1,
                        "steps": [
                            {"name": "Build selected packages in an isolated container", "conclusion": "skipped"},
                            {"name": "Record resolved build environment", "conclusion": "failure"},
                        ],
                    }
                ],
            },
        }

        def read(path, token):
            assert token == "synthetic-read-only-token"
            return fixtures[path]

        def download(path, token, destination, expected):
            assert path == prefix + "/actions/artifacts/101/zip"
            assert expected["digest"] == "sha256:" + file_hash(archive)
            shutil.copyfile(archive, destination)

        with patch.dict(
            environment.os.environ,
            {"RUNNER_TEMP": str(root), "GITHUB_TOKEN": "synthetic-read-only-token", "GITHUB_RUN_ATTEMPT": "1"},
        ), patch.object(environment, "api", side_effect=read), patch.object(
            github, "api", side_effect=read
        ), patch.object(
            environment, "download_artifact", side_effect=download
        ), patch.object(
            environment, "native", side_effect=AssertionError("retry resolved a mutable image")
        ):
            assert environment.previous(selected) == environment_identity
            # Retry references preserve both historical and human-readable assigned names.
            earlier["label"] = "Example/source v1.2.3 | debian trixie | arm64 | deb | r1"
            fixtures[prefix + "/actions/runs/100/attempts/1"]["display_title"] = "Build " + earlier["label"]
            fixtures[prefix + "/actions/runs/100/attempts/1/jobs?per_page=100"]["jobs"][0]["name"] = (
                "build / Build " + earlier["label"]
            )
            assert environment.previous(selected) == environment_identity
            wrapped = root / "next-environment.json"
            with patch.object(environment, "assignment", return_value=selected), patch.object(
                sys,
                "argv",
                [
                    "environment.py",
                    "--assignment",
                    "assigned.json",
                    "--assignment-sha256",
                    digest(selected),
                    "--output",
                    str(wrapped),
                ],
            ), patch.dict(environment.os.environ, {"EXECUTION": selected["execution"], "ATTEMPT": selected["attempt"]}):
                with patch.dict(environment.os.environ, {"GITHUB_RUN_ATTEMPT": "2"}):
                    rejects(environment.main)
                assert not wrapped.exists()
                environment.main()
            assert json.loads(wrapped.read_text()) == {
                "assignment_sha256": digest(selected),
                "environment": environment_identity,
            }
            # A retry that failed before resolving anything still refers back to older usable evidence.
            selected["environment_from"].append({**earlier, "run": {**earlier["run"], "id": 102}})
            fixtures[prefix + "/actions/runs/102/attempts/1"] = {
                **fixtures[prefix + "/actions/runs/100/attempts/1"],
                "id": 102,
            }
            fixtures[prefix + "/actions/runs/102/artifacts?per_page=100"] = {"total_count": 0, "artifacts": []}
            fixtures[prefix + "/actions/runs/102/attempts/1/jobs?per_page=100"] = copy.deepcopy(
                fixtures[prefix + "/actions/runs/100/attempts/1/jobs?per_page=100"]
            )
            fixtures[prefix + "/actions/runs/102/attempts/1/jobs?per_page=100"]["jobs"][0]["run_id"] = 102
            assert environment.previous(selected) == environment_identity
            selected["environment_from"].pop()
            original = archive.read_bytes()
            with zipfile.ZipFile(archive, "w") as zipped:
                zipped.writestr(
                    "environment.json",
                    json.dumps({"assignment_sha256": "wrong-assignment", "environment": environment_identity}),
                )
            item.update(digest="sha256:" + file_hash(archive), size_in_bytes=archive.stat().st_size)
            rejects(lambda: environment.previous(selected))
            archive.write_bytes(original)
            item.update(digest="sha256:" + file_hash(archive), size_in_bytes=archive.stat().st_size)
            item["expired"] = True
            rejects(lambda: environment.previous(selected))
            item["expired"] = False
            fixtures[prefix + "/actions/runs/100/attempts/1"]["actor"]["id"] = 999
            rejects(lambda: environment.previous(selected))
            fixtures[prefix + "/actions/runs/100/attempts/1"]["actor"]["id"] = 42
            listing = fixtures[prefix + "/actions/runs/100/artifacts?per_page=100"]
            listing.update(total_count=0, artifacts=[])
            assert environment.previous(selected) is None, "pre-build failure must permit first resolution"
            steps = fixtures[prefix + "/actions/runs/100/attempts/1/jobs?per_page=100"]["jobs"][0]["steps"]
            steps[0]["conclusion"] = "failure"
            rejects(lambda: environment.previous(selected))
            steps[0]["conclusion"] = "skipped"
            steps[1]["conclusion"] = "success"
            rejects(lambda: environment.previous(selected))


def host_manifest(directory, family="native"):
    import native
    import worker
    from contract import evidence, write_json

    selected = worker.selected("deb" if family == "native" else "apk")
    selected["target"]["family"] = family
    source = directory / "source-fixture"
    (source / "ci").mkdir(parents=True)
    (source / "ci/native.sh").write_text("#!/bin/sh\nexit 0\n")
    selected["source"].update(title="Fixture", depends_on=[], build={family: "ci/native.sh"})
    write_json(
        source / "ci/packages.json",
        {
            **{key: selected["source"][key] for key in ("title", "package", "depends_on", "build")},
            "schema": 1,
            "matrix": [selected["target"]],
        },
    )
    identity = {"resolved_image": "configured@sha256:" + "e" * 64, "platform": "linux/amd64", "config_digest": "f" * 64}
    identity = identity if family == "native" else {"host": identity, "sdk": {"sha256": "a" * 64}}
    document = {"assignment_sha256": digest(selected), "environment": identity}
    write_json(directory / "environment.json", document)
    manifest = {**evidence(selected), "environment": identity}
    for field in (None, "environment", "source"):
        if family == "openwrt":
            pristine = directory / "core-ci-sdk/extracted/sdk"
            pristine.mkdir(parents=True)
            (pristine / "pristine").write_text("SDK fixture")
        calls = []
        supplied = copy.deepcopy(manifest)
        if field is not None:
            supplied[field] = (
                {"config_digest": "forged"} if field == "environment" else {**supplied[field], "commit": "0" * 40}
            )
        arguments = SimpleNamespace(
            assignment="fixture",
            assignment_sha256=digest(selected),
            source=str(source),
            environment=str(directory / "environment.json"),
            output=str(directory / (field or "valid")),
        )

        def container(command, **options):
            calls.append(command)
            if command[:2] == ["docker", "build"]:
                context = Path(command[-1])
                dockerfile = (context / "Dockerfile").read_text()
                assert "COPY sdk" not in dockerfile and "CORE_CI_APK" not in dockerfile
                if family == "openwrt":
                    assert (context / ".dockerignore").read_text() == "sdk\n"
            if command[:2] == ["docker", "run"] and family == "native":
                assert not any("target=/sdk" in part or "CORE_CI_APK" in part for part in command)
            if command[:2] == ["docker", "run"] and family == "openwrt":
                mounted = [part for part in command if "target=/sdk," in part]
                assert len(mounted) == 1 and mounted[0].endswith("target=/sdk,readonly")
                assert (Path(mounted[0].split("source=", 1)[1].split(",")[0]) / "pristine").read_text() == "SDK fixture"
                assert "CORE_CI_APK=/sdk/staging_dir/host/bin/apk" in command
            if command[:2] == ["docker", "run"] and "--network=none" not in command:
                write_json(Path(arguments.output) / "outputs.json", {"packages": []})
            return SimpleNamespace(stdout=json.dumps(supplied))

        with patch.dict(
            native.os.environ,
            {
                "GITHUB_REPOSITORY": selected["source"]["repository"],
                "GITHUB_REPOSITORY_ID": str(selected["source"]["repository_id"]),
                "GITHUB_RUN_ATTEMPT": "1",
                "RUNNER_TEMP": str(directory),
            },
        ), patch.object(native, "assignment", return_value=selected), patch.object(
            native, "prepare_dependencies"
        ), patch.object(
            native.subprocess, "run", side_effect=container
        ), patch.object(
            native,
            "output",
            side_effect=[
                selected["source"]["commit"],
                selected["source"]["deployment"]["worker"]["tooling_sha"],
                selected["source"]["definition_blob_sha"],
                json.dumps([{"Id": (identity if family == "native" else identity["host"])["config_digest"]}]),
            ],
        ):
            if field is None:
                native.build(arguments)
                assert json.loads((Path(arguments.output) / "manifest.json").read_text()) == manifest
            else:
                rejects(lambda: native.build(arguments))
                assert not (Path(arguments.output) / "manifest.json").exists()
            assert len([call for call in calls if call[:2] == ["docker", "run"]]) == 2


def main():
    target = {"family": "native", "build": {"image": "configured:tag", "platform": "linux/amd64"}}
    metadata = [
        {
            "Os": "linux",
            "Architecture": "amd64",
            "Id": "sha256:" + "a" * 64,
            "RepoDigests": ["registry.example/image@sha256:" + "b" * 64],
        }
    ]
    with patch.object(environment.subprocess, "run") as pull, patch.object(
        environment, "output", return_value=json.dumps(metadata)
    ):
        resolved = environment.native(target)
        pull.assert_called_once_with(
            ["docker", "pull", "--platform", "linux/amd64", "configured:tag"], check=True, timeout=600
        )
        assert resolved["config_digest"] == metadata[0]["Id"] and resolved["digest"] == "sha256:" + "b" * 64
    for invalid in ({**metadata[0], "Architecture": "arm64"}, {**metadata[0], "RepoDigests": []}):
        with patch.object(environment.subprocess, "run"), patch.object(
            environment, "output", return_value=json.dumps([invalid])
        ):
            rejects(lambda: environment.native(target))
    target = {
        "family": "openwrt",
        "distribution": "openwrt",
        "suite": "99.1",
        "arch": "configured",
        "build": {"target": "custom/target", "format": "apk"},
    }
    versions = {"versions_list": ["99.1.9", "99.1.11-rc1", "99.1.10", "100.1.1", "99.1.11"]}
    profile = {
        "version_number": "99.1.11",
        "target": "custom/target",
        "arch_packages": "configured",
        "default_packages": ["apk-openssl"],
    }
    sums = "a" * 64 + "  openwrt-sdk-configured.tar.zst\n"
    # Standard tools own TLS; fixtures exercise the selection and compatibility contract.
    with patch.dict(environment.os.environ, {"OPENWRT_DOWNLOAD_BASE_URL": "https://sdk.example"}):
        with patch.object(environment, "output", side_effect=[json.dumps(versions), json.dumps(profile), sums]):
            resolved = environment.sdk(target)
        assert resolved["release"] == "99.1.11"
        assert resolved["sdk"] == {
            "sha256": "a" * 64,
            "url": "https://sdk.example/releases/99.1.11/targets/custom/target/openwrt-sdk-configured.tar.zst",
        }
        for key, value in (("arch_packages", "wrong"), ("target", "wrong/target"), ("default_packages", ["opkg"])):
            invalid = {**profile, key: value}
            with patch.object(environment, "output", side_effect=[json.dumps(versions), json.dumps(invalid)]):
                rejects(lambda: environment.sdk(target))
        with patch.object(
            environment, "output", return_value=json.dumps({"versions_list": ["99.1.11-rc1", "100.1.1"]})
        ):
            rejects(lambda: environment.sdk(target))
        with patch.object(environment, "output", side_effect=[json.dumps(versions), json.dumps(profile), sums + sums]):
            rejects(lambda: environment.sdk(target))
    resolved["toolchain"] = {"triple": "configured", "version": "1", "compiler_sha256": "c" * 64}
    resolved["staging_dir"] = "staging_dir/target-configured"
    selected = {"target": target}
    other = copy.deepcopy(resolved)
    other["sdk"]["sha256"] = "b" * 64
    assert compatibility(selected, resolved) != compatibility(
        selected, other
    ), "SDK exports lost physical compatibility checks"
    directory = Path(sys.argv[1])
    directory.mkdir(parents=True, exist_ok=True)
    # Exercise the worker entry point: roots discover latest; dependent joins inherit receipts.
    selected.update({"execution": "a" * 64, "attempt": "a" * 64 + ":1", "dependencies": []})
    host = {"resolved_image": "host@sha256:" + "d" * 64}
    with patch.object(environment, "assignment", return_value=selected), patch.object(
        environment, "previous", return_value=None
    ), patch.object(environment, "native", return_value=host), patch("sdk.prepare") as prepare, patch.object(
        sys,
        "argv",
        [
            "environment.py",
            "--assignment",
            "input",
            "--assignment-sha256",
            "hash",
            "--output",
            str(directory / "resolved.json"),
        ],
    ), patch.dict(
        environment.os.environ,
        {
            "RUNNER_TEMP": str(directory),
            "EXECUTION": selected["execution"],
            "ATTEMPT": selected["attempt"],
            "GITHUB_RUN_ATTEMPT": "1",
        },
    ):
        with patch.object(environment, "sdk", return_value=copy.deepcopy(resolved)) as latest:
            environment.main()
            latest.assert_called_once_with(target)
        receipt = {"environment": copy.deepcopy(resolved), "compatibility": compatibility(selected, resolved)}
        selected["dependencies"] = [{"receipt": copy.deepcopy(receipt)}, {"receipt": copy.deepcopy(receipt)}]
        with patch.object(environment, "sdk", side_effect=AssertionError("dependent resolved latest SDK")):
            environment.main()
            recorded = json.loads((directory / "resolved.json").read_text())["environment"]
            assert recorded == {**resolved, "host": host}
            selected["dependencies"][1]["receipt"]["environment"]["sdk"]["sha256"] = "f" * 64
            rejects(environment.main)
            selected["dependencies"][1]["receipt"] = copy.deepcopy(receipt)
            selected["dependencies"][1]["receipt"]["compatibility"]["arch"] = "other"
            rejects(environment.main)
        assert prepare.call_count == 2, "incompatible join reached SDK preparation"
    retry_evidence(directory)
    with tempfile.TemporaryDirectory(dir=directory) as fixture:
        host_manifest(Path(fixture))
    with tempfile.TemporaryDirectory(dir=directory) as fixture:
        host_manifest(Path(fixture), "openwrt")
    print("PASS worker-only image/SDK resolution fixtures; no Docker or SDK archive was executed")


if __name__ == "__main__":
    main()
