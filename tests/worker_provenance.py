"""Synthetic GitHub evidence for worker provenance; package/signing proof is in worker.py."""

import argparse
import copy
from pathlib import Path
import shutil
import sys
import subprocess
import os
import zipfile
from unittest.mock import patch


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
        + "/actions/runs/100/attempts/1": {
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
            "html_url": "https://github.com/Example/source/actions/runs/100/job/102",
            "started_at": "2026-10-26T10:00:00Z",
            "completed_at": "2026-10-26T10:00:42Z",
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
    fixtures[prefix + "/actions/runs/100"] = {
        **fixtures[prefix + "/actions/runs/100/attempts/1"],
        "run_attempt": 2,
        "conclusion": "failure",
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
        observed = github.artifact(selected, build_result, "synthetic-scoped-reader", directory / "valid")
        assert observed["id"] == 102 and observed["started_at"] == "2026-10-26T10:00:00Z"
        assert (directory / "valid/manifest.json").read_text() == "{}" and len(downloads) == 1
        # Human labels do not replace immutable assignment/run/artifact verification.
        readable = copy.deepcopy(selected)
        readable["label"] = "Example/source v1.2.3 | debian trixie | arm64 | deb | r1"
        readable_result = copy.deepcopy(build_result)
        readable_result["assignment_sha256"] = contract.digest(readable)
        current = copy.deepcopy(fixtures)
        current[prefix + "/actions/runs/100/attempts/1"]["display_title"] = "Build " + readable["label"]
        current[prefix + "/actions/jobs/102"]["name"] = "build / Build " + readable["label"]
        github.artifact(readable, readable_result, "synthetic-scoped-reader", directory / "readable")
        assert (directory / "readable/manifest.json").read_text() == "{}"
        wrong_label = copy.deepcopy(readable)
        wrong_label["label"] = readable["label"].replace("arm64", "amd64")
        for expected_hash in (readable_result["assignment_sha256"], contract.digest(wrong_label)):
            rejected_label = False
            try:
                github.artifact(
                    wrong_label,
                    {**readable_result, "assignment_sha256": expected_hash},
                    "synthetic-scoped-reader",
                    directory / "wrong-label",
                )
            except ValueError:
                rejected_label = True
            assert rejected_label, "changing the label bypassed the assignment/run identity checks"
        current = copy.deepcopy(fixtures)
        for path, field, value in (
            (prefix, "id", 12),
            (prefix + "/actions/runs/100/attempts/1", "actor", {"id": 999}),
            (prefix + "/actions/runs/100/attempts/1", "run_attempt", 2),
            (prefix + "/actions/runs/100/attempts/1", "head_sha", "d" * 40),
            (prefix + "/actions/runs/100/attempts/1", "head_repository", {"id": 999}),
            (prefix + "/actions/runs/100/attempts/1", "display_title", "unassigned work"),
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
            assert rejected and len(downloads) == 2, (path, field, "invalid provenance reached artifact download")
    finally:
        github.api = original_api
        github.download_artifact = original_download
    import validate

    with patch.dict(
        os.environ,
        {
            "CONTROLLER_APP_BOT_ID": "42",
            "GITHUB_ACTOR_ID": "42",
            "GITHUB_EVENT_NAME": "workflow_dispatch",
            "GITHUB_RUN_ATTEMPT": "2",
        },
    ):
        rejected = False
        try:
            validate.trusted_deployment(selected)
        except ValueError:
            rejected = True
        assert rejected, "publication rerun reached protected deployment work"
    # Publication views are projections of retained receipts, including older versions.
    import feed_status
    from html.parser import HTMLParser

    feed = directory / "status-feed"
    receipt_path = feed / "receipts" / ("a" * 64) / "1.json"
    receipt_path.parent.mkdir(parents=True)
    package = feed / "pool" / "project.deb"
    package.parent.mkdir()
    package.write_bytes(b"immutable package bytes")
    receipt = {
        "schema": 1,
        "source_commit": "c" * 40,
        "feed": {"base_url": "https://packages.example.test/"},
        "compatibility": {"distribution": "configured", "suite": "series", "arch": "arch", "format": "deb"},
        "build_result": {"run": {"html_url": "https://github.com/Example/source/actions/runs/100"}},
        "run": {"html_url": "https://github.com/Example/worker/actions/runs/200"},
        "packages": [{"name": "project<&>", "version": "1.0-1", "path": "pool/project.deb", "sha256": "d" * 64}],
    }
    contract.write_json(receipt_path, receipt)
    receipt["dependencies"] = [{"execution": "e" * 64, "attempt": "e" * 64 + ":1", "packages": []}]
    receipt["build_job"] = observed
    receipt["summary"] = {
        "compilers": [{"name": "cc", "version": "compiler <reported>"}],
        "tests": {
            "passed": 3,
            "failed": 0,
            "skipped": 1,
            "coverage_percent": 0,
            "report_url": "https://reports.example.test/results",
        },
    }
    receipt["packages"][0]["version"] = "2.0-2"
    contract.write_json(receipt_path.with_name("2.json"), receipt)
    paths = feed_status.render(feed)
    assert set(paths) == {"index.html", "status/index.html", "status/README.md", "status/source.md", "status.json"}
    data = contract.read_json(feed / "status.json")["publications"]
    assert [row["version"] for row in data] == ["1.0-1", "2.0-2"]
    assert all(row["repository"] == "Example/source" for row in data)
    assert data[0]["build_duration_seconds"] is None and data[1]["build_duration_seconds"] == 42
    assert all(row["published_utc"] is None and row["work"] is None for row in data)
    assert data[0]["dependencies"] is None and data[1]["dependencies"] == receipt["dependencies"]
    project_page = (feed / "status/source.md").read_text()
    assert "Hook-reported" in project_page and "compiler &lt;reported&gt;" in project_page
    assert "Published (UTC)" in project_page and "Available" in project_page
    assert "coverage 0%" in project_page and 'href="https://reports.example.test/results"' in project_page

    class Links(HTMLParser):
        def __init__(self):
            super().__init__()
            self.links = []

        def handle_starttag(self, tag, attrs):
            if tag == "a":
                self.links.extend(value for key, value in attrs if key == "href")

    page = (feed / "index.html").read_text()
    links = Links()
    links.feed(page)
    assert len(links.links) == 8 and "https://packages.example.test/pool/project.deb" in links.links
    assert "project&lt;&amp;&gt;" in page and "project<&>" not in page
    assert "Waiting, building and failed work" in page
    before = {str(path.relative_to(feed)): contract.file_hash(path) for path in feed.rglob("*") if path.is_file()}
    assert feed_status.render(feed) == paths
    assert before == {
        str(path.relative_to(feed)): contract.file_hash(path) for path in feed.rglob("*") if path.is_file()
    }
    assert package.read_bytes() == b"immutable package bytes"
    receipt["run"]["html_url"] = "javascript:alert(1)"
    contract.write_json(receipt_path, receipt)
    rejected = False
    try:
        feed_status.render(feed)
    except ValueError:
        rejected = True
    assert rejected and (feed / "index.html").read_text() == page
    receipt["run"]["html_url"] = "https://github.com/Example/worker/actions/runs/200"
    receipt["packages"][0]["path"] = "../escape"
    contract.write_json(receipt_path, receipt)
    rejected = False
    try:
        feed_status.render(feed)
    except ValueError:
        rejected = True
    assert rejected and (feed / "index.html").read_text() == page
    receipt["packages"][0]["path"] = "pool/project.deb"
    receipt["build_result"]["run"]["html_url"] = "https://github.com/Example/README/actions/runs/100"
    contract.write_json(receipt_path, receipt)
    feed_status.render(feed)
    assert "Example/README" in (feed / "status/README.md").read_text()
    assert "Example/source" in (feed / "status/README.md").read_text()
    assert "Example/README" in (feed / "status/README/README.md").read_text()
    assert "Example/source" not in (feed / "status/README/README.md").read_text()
    # Deployment templates get public parameters and structured coordinates, not job credentials.
    from jinja2 import UndefinedError

    templates = directory / "templates"
    shutil.copytree(repository / "deployment/worker/status-templates", templates)
    contract.write_json(
        templates / "presentation.json",
        {
            "schema": 1,
            "title": "Public <title>",
            "parameters": {"support": "Owner-defined <support>"},
            "projects": {"Example/source": {"page": "source-packages"}},
            "assets": ["assets/logo.txt"],
        },
    )
    (templates / "assets").mkdir()
    (templates / "assets/logo.txt").write_text("public artwork")
    (templates / "index.html.j2").write_text(
        "{{ site.parameters.support }} {{ projects[0].targets[0].coordinates.arch }}"
    )
    custom_paths = feed_status.render(feed, templates)
    assert (feed / "index.html").read_text() == "Owner-defined &lt;support&gt; arch"
    assert {"status/assets/logo.txt", "status/source-packages.md"} <= set(custom_paths)
    assert package.read_bytes() == b"immutable package bytes"
    retained = {str(path.relative_to(feed)): path.read_bytes() for path in feed.rglob("*") if path.is_file()}
    (templates / "index.html.j2").write_text("{{ missing_required_value }}")
    try:
        feed_status.render(feed, templates)
        raise AssertionError("undefined required template parameter accepted")
    except UndefinedError:
        pass
    assert retained == {str(path.relative_to(feed)): path.read_bytes() for path in feed.rglob("*") if path.is_file()}
    (templates / "index.html.j2").write_text("public template")
    settings = contract.read_json(templates / "presentation.json")
    settings["projects"]["Example/README"] = {"page": "source-packages"}
    contract.write_json(templates / "presentation.json", settings)
    try:
        feed_status.render(feed, templates)
        raise AssertionError("colliding project pages accepted")
    except ValueError:
        pass
    assert retained == {str(path.relative_to(feed)): path.read_bytes() for path in feed.rglob("*") if path.is_file()}
    del settings["projects"]["Example/README"]
    contract.write_json(templates / "presentation.json", settings)
    (templates / "index.html.j2").write_text("public template")
    (templates / "escape.j2").symlink_to(directory / "not-public")
    try:
        feed_status.render(feed, templates)
        raise AssertionError("linked template input accepted")
    except ValueError:
        pass
    (templates / "escape.j2").unlink()
    settings = contract.read_json(templates / "presentation.json")
    settings["assets"] = ["../keys/apt.asc"]
    contract.write_json(templates / "presentation.json", settings)
    try:
        feed_status.render(feed, templates)
        raise AssertionError("template asset escaped its output authority")
    except ValueError:
        pass
    settings["assets"] = ["assets/logo.txt"]
    contract.write_json(templates / "presentation.json", settings)
    shutil.rmtree(feed / "status/assets")
    protected = feed / "keys"
    protected.mkdir()
    (protected / "logo.txt").write_text("protected existing key")
    (feed / "status/assets").symlink_to(protected, target_is_directory=True)
    try:
        feed_status.render(feed, templates)
        raise AssertionError("status parent symlink redirected output to protected paths")
    except ValueError:
        pass
    assert (protected / "logo.txt").read_text() == "protected existing key"
    (feed / "status/assets").unlink()
    # The publisher invokes the same renderer with a clean environment.
    import publish

    with patch.dict(os.environ, {"GITHUB_TOKEN": "synthetic-must-not-reach-renderer"}):
        with patch.object(publish.subprocess, "run", wraps=publish.subprocess.run) as child:
            publish.render(feed)
            assert "GITHUB_TOKEN" not in child.call_args.kwargs["env"]
    print(
        "Receipt templates: parameters, structured fields, missing facts, escaping, strict errors, paths and replay verified"
    )
    # These empty Git fixtures contain no application/source checkout or real credential.
    import publish

    remote = directory / "remote.git"
    writer = directory / "writer"
    os.environ["TMPDIR"] = str(directory)
    subprocess.run(["git", "init", "--quiet", "--bare", "--initial-branch=main", str(remote)], check=True)
    subprocess.run(["git", "init", "--quiet", "--initial-branch=main", str(writer)], check=True)
    contract.run(["git", "-C", str(writer), "remote", "add", "origin", str(remote)])
    (writer / "receipt.json").write_text("initial receipt\n")
    publish.prepare_commit(writer, ["receipt.json"])
    assert (
        subprocess.run(
            ["git", "--git-dir", str(remote), "show-ref", "--verify", "--quiet", "refs/heads/main"]
        ).returncode
        != 0
    )
    first = publish.checked_push(writer, "main", "synthetic-no-access-token")
    publish.prepare_commit(writer, ["receipt.json"])
    assert publish.checked_push(writer, "main", "synthetic-no-access-token") == first
    assert contract.output(["git", "--git-dir", str(remote), "rev-parse", "main"]) == first
    (writer / ".gitignore").write_text("ignored-package\n")
    (writer / "ignored-package").write_text("must be committed with indexes")
    rejected = False
    try:
        publish.prepare_commit(writer, ["ignored-package"])
    except (ValueError, subprocess.CalledProcessError):
        rejected = True
    assert rejected and contract.output(["git", "--git-dir", str(remote), "rev-parse", "main"]) == first
    tree = contract.output(["git", "--git-dir", str(remote), "rev-parse", "main^{tree}"])
    concurrent = contract.output(
        [
            "git",
            "--git-dir",
            str(remote),
            "-c",
            "user.name=Fixture",
            "-c",
            "user.email=test@example.invalid",
            "commit-tree",
            tree,
            "-p",
            first,
            "-m",
            "Concurrent feed update",
        ]
    )
    contract.run(["git", "--git-dir", str(remote), "update-ref", "refs/heads/main", concurrent, first])
    (writer / "receipt.json").write_text("new receipt\n")
    rejected = False
    try:
        publish.prepare_commit(writer, ["receipt.json"])
        publish.checked_push(writer, "main", "synthetic-no-access-token")
    except subprocess.CalledProcessError:
        rejected = True
    assert rejected and contract.output(["git", "--git-dir", str(remote), "rev-parse", "main"]) == concurrent
    prepared = copy.deepcopy(selected)
    prepared["revision"] = 1
    prepared["target"] = {"build": {"format": "deb"}}
    prepared["source"]["deployment"]["feed"] = {"branch": "main"}
    empty_receipt = {"packages": [], "indexes": [], "development_export": None}
    argv = [
        "publish.py",
        "--phase",
        "prepare",
        "--assignment",
        "assigned.json",
        "--assignment-sha256",
        "fixture",
        "--build-result",
        "result.json",
        "--artifacts",
        "outputs",
        "--feed",
        str(writer),
    ]

    def failed_render(feed):
        assert not list(directory.glob("core-ci-secrets-*")), "rendering saw temporary signing files"
        raise ValueError("invalid status receipt")

    with patch.object(sys, "argv", argv), patch.dict(
        os.environ,
        {
            "EXECUTION": prepared["execution"],
            "ATTEMPT": prepared["attempt"],
            "RUNNER_TEMP": str(directory),
            "APT_SIGNING_KEY": "synthetic-test-material",
        },
    ), patch.object(publish, "assignment", return_value=prepared), patch.object(
        publish, "trusted_deployment"
    ), patch.object(
        publish, "run_identity", return_value={}
    ), patch.object(
        publish, "read_json", return_value={}
    ), patch.object(
        publish, "update", return_value=empty_receipt
    ), patch.object(
        publish, "retained_receipt", return_value=empty_receipt
    ), patch.object(
        publish, "render", side_effect=failed_render
    ), patch.object(
        publish, "prepare_commit"
    ) as commit, patch.object(
        publish, "checked_push"
    ) as push, patch.object(
        publish, "writer_token"
    ) as token:
        rejected = False
        try:
            publish.main()
        except ValueError:
            rejected = True
        assert rejected and not commit.called and not push.called and not token.called
    assert not list(directory.glob("core-ci-secrets-*")), "temporary key material survived preparation failure"
    shutil.rmtree(writer)
    shutil.rmtree(remote)
    print("Real local Git: checked update accepted, ignored package and stale publication rejected")
    print(
        "Synthetic worker provenance: assigned artifact accepted; 12 run/job/workflow/artifact mismatches rejected before download"
    )


if __name__ == "__main__":
    main()
