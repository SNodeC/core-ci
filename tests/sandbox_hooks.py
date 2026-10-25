"""Real local DEB/RPM tools over synthetic C fixtures; not GitHub or installation acceptance."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile

repository = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(repository / "deployment/worker/core_ci"))
from contract import package_metadata


def main():
    directory = Path(sys.argv[1])
    directory.mkdir(parents=True, exist_ok=True)
    # Match the reviewed public interfaces, while changing the value to detect substitution.
    fixtures = {
        "demo-core": {
            "core.h": "int demo_value(void);\n",
            "core.c": '#include "core.h"\nint demo_value(void) { return 37; }\n',
            "core-main.c": '#include "core.h"\n#include <stdio.h>\nint main(void) { printf("demo-core %d\\n", demo_value()); return 0; }\n',
        },
        "demo-app": {
            "app.c": '#include <demo-core.h>\n#include <stdio.h>\nint main(void) { printf("demo-app uses core %d\\n", demo_value()); return 0; }\n',
        },
        "demo-side": {
            "side.h": "int demo_side_value(void);\n",
            "side.c": '#include "side.h"\n#include <demo-core.h>\nint demo_side_value(void) { return demo_value() + 2; }\n',
            "side-main.c": '#include "side.h"\n#include <stdio.h>\nint main(void) { printf("demo-side %d\\n", demo_side_value()); return 0; }\n',
        },
        "demo-join": {
            "app.c": '#include <demo-core.h>\n#include <demo-side.h>\n#include <stdio.h>\nint main(void) { printf("demo-join core %d, side %d\\n", demo_value(), demo_side_value()); return 0; }\n',
        },
    }
    with tempfile.TemporaryDirectory(dir=directory) as temporary:
        root = Path(temporary)
        for package_format, arch in (("deb", "amd64"), ("rpm", "x86_64")):
            published = {}
            for name, files in fixtures.items():
                work = root / package_format / name
                source = work / "source"
                (source / "src").mkdir(parents=True)
                (source / "ci").mkdir()
                for filename, contents in files.items():
                    (source / "src" / filename).write_text(contents)
                definition = json.loads((repository / "deployment/sandbox/definitions" / (name + ".json")).read_text())
                (source / "ci/packages.json").write_text(json.dumps(definition))
                row = next(
                    item
                    for item in definition["matrix"]
                    if item["build"]["format"] == package_format and item["arch"] == arch
                )
                target = work / "target.json"
                target.write_text(json.dumps(row))
                prerequisites = work / "prerequisites"
                prerequisites.mkdir()
                for dependency in definition["depends_on"]:
                    export = published[dependency] / "development.tar.gz"
                    if export.exists():
                        with tarfile.open(export) as archive:
                            archive.extractall(prerequisites / dependency / "development", filter="data")
                output = work / "output"
                output.mkdir()
                (work / "temporary").mkdir()
                environment = {
                    **os.environ,
                    "TMPDIR": str(work / "temporary"),
                    "SOURCE_DIR": str(source),
                    "OUTPUT_DIR": str(output),
                    "TARGET_JSON": str(target),
                    "PREREQUISITES_DIR": str(prerequisites),
                    "PACKAGE_VERSION": "1.0.0-7",
                    "PACKAGE_UPSTREAM_VERSION": "1.0.0",
                    "PACKAGE_RELEASE": "7",
                }
                subprocess.run(
                    [sys.executable, str(repository / "deployment/sandbox/ci/build.py"), "native"],
                    env=environment,
                    check=True,
                )
                metadata = package_metadata(output / (name + "." + package_format), package_format)
                assert metadata["name"] == name and metadata["version"] == "1.0.0-7" and metadata["arch"] == arch
                required = " ".join(metadata["dependencies"])
                assert all(dependency in required for dependency in definition["depends_on"])
                declared = json.loads((output / "outputs.json").read_text())
                assert declared["packages"] == [{"file": name + "." + package_format, "component": name}]
                if name in {"demo-core", "demo-side"}:
                    with tarfile.open(output / "development.tar.gz") as archive:
                        assert set(archive.getnames()) == {"include/" + name + ".h", "lib/lib" + name + ".a"}
                else:
                    assert "development_export" not in declared
                published[name] = output
    print("PASS eight real DEB/RPM builds over synthetic dependency fixtures; not container/client/live acceptance")


if __name__ == "__main__":
    main()
