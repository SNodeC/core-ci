"""Synthetic GitHub evidence for worker provenance; package/signing proof is in worker.py."""

import argparse
import copy
from pathlib import Path
import shutil
import sys
import zipfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", required=True)
    arguments = parser.parse_args()
    directory = Path(arguments.directory).resolve()
    shutil.rmtree(directory, ignore_errors=True)
    directory.mkdir(parents=True)
    repository = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(repository / "deployment/worker/core_ci"))
    import contract
    import github

    selected = {
        "execution": "a" * 64,
        "attempt": "a" * 64 + ":1",
        "artifact_name": "core-ci-build-" + "a" * 64 + "-r1",
        "source": {
            "repository": "Example/source",
            "repository_id": 11,
            "deployment": {
                "controller_app_bot_id": 42,
                "source_workflow": {"path": ".github/workflows/core-ci-build.yml", "blob_sha": "b" * 40},
            },
        },
    }
    build_result = {
        "assignment_sha256": contract.digest(selected),
        "run": {"id": 100, "workflow_commit": "c" * 40, "workflow_blob_sha": "b" * 40},
        "artifacts": [{"id": 101, "name": selected["artifact_name"], "job_id": 102, "digest": "", "size_bytes": 0}],
    }
    archive = directory / "assigned.zip"
    with zipfile.ZipFile(archive, "w") as zipped:
        zipped.writestr("manifest.json", "{}")
    build_result["artifacts"][0].update(
        digest="sha256:" + contract.file_hash(archive), size_bytes=archive.stat().st_size
    )
    prefix = "/repos/Example/source"
    fixtures = {
        prefix: {"id": 11},
        prefix
        + "/actions/runs/100": {
            "id": 100,
            "repository": {"id": 11},
            "head_repository": {"id": 11},
            "actor": {"id": 42},
            "event": "workflow_dispatch",
            "run_attempt": 1,
            "status": "completed",
            "conclusion": "success",
            "head_sha": "c" * 40,
            "display_title": "core-ci build " + selected["attempt"] + " " + contract.digest(selected),
            "path": ".github/workflows/core-ci-build.yml",
        },
        prefix + "/contents/.github/workflows/core-ci-build.yml?ref=" + "c" * 40: {"sha": "b" * 40},
        prefix
        + "/actions/jobs/102": {
            "id": 102,
            "run_id": 100,
            "run_attempt": 1,
            "status": "completed",
            "conclusion": "success",
            "name": "build / core-ci package " + selected["attempt"],
        },
        prefix
        + "/actions/artifacts/101": {
            "id": 101,
            "name": selected["artifact_name"],
            "workflow_run": {"id": 100},
            "expired": False,
            "digest": build_result["artifacts"][0]["digest"],
            "size_in_bytes": archive.stat().st_size,
        },
    }
    original_api = github.api
    original_download = github.download_artifact
    downloads = []
    current = copy.deepcopy(fixtures)

    def api(path, token):
        assert token == "synthetic-scoped-reader"
        result = current[path]
        return result

    def download(path, token, destination, expected):
        assert path == prefix + "/actions/artifacts/101/zip" and token == "synthetic-scoped-reader"
        assert expected == fixtures[prefix + "/actions/artifacts/101"]
        downloads.append(path)
        shutil.copyfile(archive, destination)

    try:
        github.api = api
        github.download_artifact = download
        github.artifact(selected, build_result, "synthetic-scoped-reader", directory / "valid")
        assert (directory / "valid/manifest.json").read_text() == "{}" and len(downloads) == 1
        for path, field, value in (
            (prefix, "id", 12),
            (prefix + "/actions/runs/100", "actor", {"id": 999}),
            (prefix + "/actions/runs/100", "run_attempt", 2),
            (prefix + "/actions/runs/100", "head_sha", "d" * 40),
            (prefix + "/actions/runs/100", "head_repository", {"id": 999}),
            (prefix + "/actions/runs/100", "display_title", "unassigned work"),
            (prefix + "/actions/jobs/102", "run_id", 999),
            (prefix + "/actions/jobs/102", "conclusion", "failure"),
            (prefix + "/actions/artifacts/101", "workflow_run", {"id": 999}),
            (prefix + "/actions/artifacts/101", "digest", "sha256:" + "0" * 64),
            (prefix + "/actions/artifacts/101", "expired", True),
            (prefix + "/contents/.github/workflows/core-ci-build.yml?ref=" + "c" * 40, "sha", "d" * 40),
        ):
            current = copy.deepcopy(fixtures)
            current[path][field] = value
            rejected = False
            try:
                github.artifact(selected, build_result, "synthetic-scoped-reader", directory / "invalid")
            except ValueError:
                rejected = True
            assert rejected and len(downloads) == 1, (path, field, "invalid provenance reached artifact download")
    finally:
        github.api = original_api
        github.download_artifact = original_download
    print(
        "Synthetic worker provenance: assigned artifact accepted; 12 run/job/workflow/artifact mismatches rejected before download"
    )


if __name__ == "__main__":
    main()
