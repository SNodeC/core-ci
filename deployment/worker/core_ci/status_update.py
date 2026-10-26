"""Trusted status-only feed update from the controller's public graph facts."""

import argparse
import json
import os
from pathlib import Path
import tempfile
from urllib.parse import urlsplit
import urllib.request

from contract import canonical, read_json, regular_file, require, run
from publish import checked_push, prepare_commit, render, writer_token


def executing_attempt():
    require(os.environ["GITHUB_RUN_ATTEMPT"] == "1" and os.environ["GITHUB_EVENT_NAME"] == "workflow_dispatch",
            "status updates require an original dispatched attempt")
    require(os.environ["GITHUB_ACTOR_ID"] == os.environ["CONTROLLER_APP_BOT_ID"], "status actor is not the controller")


def prepare(feed, minimum_generation):
    executing_attempt()
    url = os.environ["CORE_CI_STATUS_URL"]
    address = urlsplit(url)
    require(address.scheme == "https" and address.hostname and not address.username and not address.password
            and not address.query and not address.fragment and address.path == "/public/status", "invalid controller status URL")
    request = urllib.request.Request(url, headers={"Cache-Control": "no-cache"})
    with urllib.request.urlopen(request, timeout=30) as response:
        raw = response.read(8 * 1024 * 1024 + 1)
        require(response.url == url and len(raw) <= 8 * 1024 * 1024, "redirected or oversized controller facts")
    facts = json.loads(raw)
    feed_identity = {"repository": os.environ["FEED_REPOSITORY"], "repository_id": int(os.environ["FEED_REPOSITORY_ID"]),
                     "branch": os.environ["FEED_BRANCH"]}
    require(facts["schema"] == 1 and facts["feed"] == feed_identity and type(facts["generation"]) is int
            and facts["generation"] >= minimum_generation, "controller facts do not match this feed/generation")
    path = feed / "status/controller.json"
    if path.exists():
        previous = read_json(regular_file(feed, "status/controller.json"), 8 * 1024 * 1024)
        require(facts["generation"] >= previous["generation"], "refusing stale controller facts")
        require(facts["generation"] != previous["generation"] or facts == previous, "conflicting facts for the same generation")
    require(not path.is_symlink() and not path.parent.is_symlink(), "linked controller projection destination")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(canonical(facts) + "\n")
    paths = ["status/controller.json"] + render(feed)
    prepare_commit(feed, paths, "Update public core-ci status")
    return paths


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--feed", required=True)
    parser.add_argument("--phase", choices=("prepare", "push"), required=True)
    parser.add_argument("--generation", type=int, required=True)
    arguments = parser.parse_args()
    executing_attempt()
    os.umask(0o077)
    feed = Path(arguments.feed).resolve()
    if arguments.phase == "prepare":
        prepare(feed, arguments.generation)
    else:
        # Verify the prepared bytes before minting a token. No rendering in the push phase.
        run(["git", "-C", str(feed), "diff", "--exit-code", "HEAD"], stdout=None)
        projection = read_json(regular_file(feed, "status/controller.json"), 8 * 1024 * 1024)
        require(projection["generation"] >= arguments.generation, "prepared projection is older than the notification")
        with tempfile.TemporaryDirectory(prefix="core-ci-status-key-", dir=os.environ["RUNNER_TEMP"]) as temporary:
            key = Path(temporary) / "writer.pem"
            key.write_text(os.environ.pop("PACKAGES_APP_PRIVATE_KEY"))
            key.chmod(0o600)
            token = writer_token(key, projection["feed"])
            checked_push(feed, projection["feed"]["branch"], token)
        print("Pushed public status generation", projection["generation"])


if __name__ == "__main__":
    main()
