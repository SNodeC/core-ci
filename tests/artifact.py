import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile


def main():
    build = Path(sys.argv[1])
    parent = Path("/tmp/codex/build/core-ci/artifacts")
    parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="case-", dir=parent) as temporary:
        root = Path(temporary)
        graph = root / "graph.json"
        subprocess.run([str(build / "graph-contract-test"), str(graph)], check=True)
        active = json.loads(graph.read_text())["executions"][0]["attempts"][-1]
        original = active["build_manifest"]

        def archive(name, manifest=None, filename="example.deb", mode=zipfile.ZIP_DEFLATED, extra=None, zip64=False):
            with zipfile.ZipFile(root / f"{name}.zip", "w", compression=mode) as output:
                output.writestr("manifest.json", json.dumps(original if manifest is None else manifest))
                if zip64:
                    with output.open(filename, "w", force_zip64=True) as package:
                        package.write(b"package")
                else:
                    output.writestr(filename, b"package")
                if extra:
                    output.writestr(*extra)

        archive("good-deflated")
        archive("good-stored", mode=zipfile.ZIP_STORED)
        archive("good-local-zip64", zip64=True)
        summary = copy.deepcopy(original)
        summary["summary"] = {"compilers": [{"name": "Fixture compiler", "version": "1"}],
                              "tests": {"passed": 1, "failed": 0, "skipped": 0, "coverage_percent": 90}}
        archive("good-summary", summary)
        summary["summary"]["tests"]["passed"] = -1
        archive("bad-summary", summary)
        nested = copy.deepcopy(original)
        nested["packages"][0]["file"] = "outputs/example.deb"
        archive("good-relative-subdirectory", nested, filename="outputs/example.deb", extra=("outputs/", b""))
        for field, value in [("attempt", "unrelated"), ("assignment_sha256", "0" * 64), ("source", {}),
                             ("dependencies", [{"execution": "unrelated"}]), ("package_version", {}), ("build_job", {})]:
            changed = copy.deepcopy(original)
            changed[field] = value
            archive("bad-" + field, changed)
        changed = copy.deepcopy(original)
        changed["packages"][0]["sha256"] = "0" * 64
        archive("bad-package-hash", changed)
        changed = copy.deepcopy(original)
        changed["environment"]["platform"] = "unrelated"
        archive("bad-environment", changed)
        archive("bad-traversal", filename="../example.deb")
        archive("bad-extra", extra=("undeclared", b"x"))
        archive("bad-duplicate", extra=("example.deb", b"package"))
        archive("bad-truncated")
        path = root / "bad-truncated.zip"
        path.write_bytes(path.read_bytes()[:-8])
        archive("bad-link", extra=(zipfile.ZipInfo("link"), b"target"))
        with zipfile.ZipFile(root / "bad-link.zip", "a") as output:
            output.filelist[-1].external_attr = (0o120777 << 16)
        subprocess.run([str(build / "artifact-contract-test"), str(graph), str(root)], check=True)


if __name__ == "__main__":
    main()
