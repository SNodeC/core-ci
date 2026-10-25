"""Render reviewable source workflow bytes; never writes to GitHub."""

import argparse
import hashlib
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--worker", required=True)
    parser.add_argument("--tooling-sha", required=True)
    parser.add_argument("--output", required=True)
    arguments = parser.parse_args()
    if re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", arguments.worker) is None:
        raise ValueError("worker must be owner/repository")
    if re.fullmatch(r"[a-f0-9]{40}", arguments.tooling_sha) is None:
        raise ValueError("tooling must be a full reviewed commit SHA")
    content = (Path(__file__).parent / "source/core-ci-build.yml.in").read_text()
    content = content.replace("@WORKER@", arguments.worker).replace("@TOOLING_SHA@", arguments.tooling_sha)
    Path(arguments.output).write_text(content)
    blob = b"blob " + str(len(content.encode())).encode() + b"\0" + content.encode()
    print("Approved source-workflow Git blob SHA:", hashlib.sha1(blob).hexdigest())


if __name__ == "__main__":
    main()
