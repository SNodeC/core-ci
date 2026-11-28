"""Rehearse the Contract X cutover floor with a verified schema-7 dump and ledger."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

from database import database


def revision_floors(dump, ledger):
    with database() as (_, cli, _, _):
        with dump.open("rb") as source:
            subprocess.run([*cli, "probe"], stdin=source, check=True)
        version = subprocess.check_output([*cli, "probe", "-e", "SELECT version FROM coreci_schema"], text=True).strip()
        if version != "7":
            raise ValueError(f"expected schema 7, found {version}")
        statements = [
            f"SELECT package, version, target, revision FROM {table}"
            for table in ("coreci_revisions", "coreci_plan", "coreci_published", "coreci_failures")
        ]
        statements.append("SELECT prerequisite, version, target, revision FROM coreci_inputs")
        raw = subprocess.check_output([*cli, "probe", "-e", " UNION ALL ".join(statements)], text=True)

    database_rows = [line.split("\t") for line in raw.splitlines()]
    ledger_rows = [
        [entry["unit"]["package"], entry["unit"]["version"], entry["target"]["id"], entry["unit"]["revision"]]
        for entry in ledger
    ]
    maxima = []
    for rows in (database_rows, ledger_rows):
        largest = {}
        for package, version, target, raw_revision in rows:
            if raw_revision != "NULL":
                key = package, version, target
                revision = int(raw_revision)
                if any(not part or part == "NULL" or "/" in part for part in key) or revision < 1:
                    raise ValueError(f"invalid historical revision record {key!r}")
                largest[key] = max(largest.get(key, 0), revision)
        maxima.append(largest)

    old, feed = maxima
    floors = {key: max(old.get(key, 0), feed.get(key, 0)) + 1 for key in sorted(old.keys() | feed.keys())}
    for rows in (database_rows, ledger_rows):
        for package, version, target, raw_revision in rows:
            if raw_revision != "NULL" and floors[package, version, target] <= int(raw_revision):
                raise ValueError(f"floor does not exceed recorded revision for {(package, version, target)!r}")
    return [
        dict(package=key[0], version=key[1], target=key[2], database_max=old.get(key, 0), ledger_max=feed.get(key, 0), floor=floor)
        for key, floor in floors.items()
    ]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dump", type=Path, required=True)
    parser.add_argument("--dump-sha256", required=True)
    parser.add_argument("--ledger", type=Path, required=True)
    parser.add_argument("--ledger-sha256", required=True)
    parser.add_argument("--snapshot", required=True)
    options = parser.parse_args()
    with options.dump.open("rb") as backup:
        digest = hashlib.file_digest(backup, "sha256").hexdigest()
    if digest != options.dump_sha256:
        raise SystemExit("backup hash mismatch")
    ledger = options.ledger.read_bytes()
    if hashlib.sha256(ledger).hexdigest() != options.ledger_sha256:
        raise SystemExit("ledger hash mismatch")
    floors = revision_floors(options.dump, json.loads(ledger))
    print(json.dumps(dict(snapshot=options.snapshot, dump_sha256=options.dump_sha256,
                          ledger_sha256=options.ledger_sha256, revision_floors=floors), indent=2))


if __name__ == "__main__":
    main()
