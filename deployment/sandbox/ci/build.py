"""Source-owned recipes for the four small sandbox C projects; not controller tooling."""

import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tarfile
import tempfile


def run(arguments, **options):
    subprocess.run([str(value) for value in arguments], check=True, **options)


def main():
    source = Path(os.environ["SOURCE_DIR"])
    output = Path(os.environ["OUTPUT_DIR"])
    target = json.loads(Path(os.environ["TARGET_JSON"]).read_text())
    definition = json.loads((source / "ci/packages.json").read_text())
    name = definition["package"]
    family = sys.argv[1]
    package_format = target["build"]["format"]
    dependencies = definition["depends_on"]
    assert name in {"demo-core", "demo-app", "demo-side", "demo-join"}
    assert family == target["family"]
    compiler = ["cc"]
    archiver = ["ar"]
    with tempfile.TemporaryDirectory(prefix="demo-build-") as temporary:
        work = Path(temporary)
        root = work / "root"
        binary = root / "usr/bin" / name
        binary.parent.mkdir(parents=True)
        includes = ["-I" + str(source / "src")]
        libraries = []
        for prerequisite in sorted(Path(os.environ["PREREQUISITES_DIR"]).iterdir()):
            development = prerequisite / "development"
            if development.exists():
                includes.append("-I" + str(development / "include"))
                libraries.extend(sorted((development / "lib").glob("*.a")))
        if family == "openwrt":
            sdk = Path(os.environ["SDK_DIR"])
            query = work / "query.mk"
            query.write_text("TOPDIR:=$(CURDIR)\nSDK:=1\ninclude $(TOPDIR)/rules.mk\n")
            values = {}
            for variable in ("TARGET_CC", "TARGET_AR", "TOOLCHAIN_DIR"):
                value = subprocess.check_output(
                    ["make", "--no-print-directory", "--silent", "-C", str(sdk), "-f", str(query), "val." + variable],
                    text=True,
                ).strip()
                values[variable] = shlex.split(value)
                assert values[variable], "SDK cannot identify " + variable
            binaries = Path(values["TOOLCHAIN_DIR"][0]) / "bin"
            compiler = [str(binaries / values["TARGET_CC"][0])] + values["TARGET_CC"][1:]
            archiver = [str(binaries / values["TARGET_AR"][0])] + values["TARGET_AR"][1:]
        options = ["-Wall", "-Wextra", "-Werror", "-O2"] + includes
        sources = sorted((source / "src").glob("*.c"))
        run(compiler + options + sources + ["-Wl,--start-group"] + libraries + ["-Wl,--end-group", "-o", binary])
        if family == "native":
            result = subprocess.check_output([str(binary)], text=True).strip()
            pattern = re.escape(name) + r"(?: uses core)? [0-9]+"
            if name == "demo-join":
                pattern = r"demo-join core ([0-9]+), side ([0-9]+)"
            assert re.fullmatch(pattern, result), "sandbox runtime output differs from the source interface"
            if name == "demo-join":
                values = list(map(int, re.findall(r"[0-9]+", result)))
                assert values[1] == values[0] + 2, "join did not consume compatible core/side exports"
        declared = {
            "packages": [{"file": name + "." + package_format, "component": name}],
            "summary": {
                "compilers": [
                    {
                        "name": Path(compiler[0]).name,
                        "version": subprocess.check_output(compiler + ["--version"], text=True).splitlines()[0],
                    }
                ],
                "tests": {"passed": int(family == "native"), "failed": 0, "skipped": int(family != "native")},
            },
        }
        if name in {"demo-core", "demo-side"}:
            stem = name.removeprefix("demo-")
            development = work / "development"
            (development / "include").mkdir(parents=True)
            (development / "lib").mkdir()
            run(compiler + options + ["-c", source / "src" / (stem + ".c"), "-o", work / "library.o"])
            library = development / "lib" / ("lib" + name + ".a")
            run(archiver + ["rcs", library, work / "library.o"])
            shutil.copyfile(source / "src" / (stem + ".h"), development / "include" / (name + ".h"))
            shutil.copytree(development, root / "usr", dirs_exist_ok=True)
            with tarfile.open(output / "development.tar.gz", "w:gz") as archive:
                for item in sorted(development.rglob("*")):
                    if item.is_file():
                        archive.add(item, arcname=item.relative_to(development), recursive=False)
            declared["development_export"] = "development.tar.gz"
        version = os.environ["PACKAGE_VERSION"]
        arch = target["arch"]
        if package_format in {"deb", "ipk"}:
            control = root / ("DEBIAN" if package_format == "deb" else "CONTROL")
            control.mkdir()
            required = ["libc6" if package_format == "deb" else "libc"] + dependencies
            (control / "control").write_text(
                f"Package: {name}\nVersion: {version}\nArchitecture: {arch}\n"
                "Maintainer: VOC-Experiments sandbox <noreply@example.invalid>\n"
                "Description: Graph-driven package pipeline acceptance project\n"
                "Section: devel\nPriority: optional\nDepends: " + ", ".join(required) + "\n"
            )
            if package_format == "deb":
                run(["dpkg-deb", "--build", "--root-owner-group", root, output / (name + ".deb")])
            else:
                packages = work / "packages"
                packages.mkdir()
                run(["sh", Path(os.environ["SDK_DIR"]) / "scripts/ipkg-build", root, packages])
                built = list(packages.glob("*.ipk"))
                assert len(built) == 1
                shutil.copyfile(built[0], output / (name + ".ipk"))
        elif package_format == "rpm":
            top = work / "rpm"
            for directory in ("SPECS", "SOURCES", "tmp", "db"):
                (top / directory).mkdir(parents=True)
            shutil.copytree(root, top / "SOURCES/root")
            requirements = "".join("Requires: " + dependency + "\n" for dependency in dependencies)
            files = "\n".join("/" + str(item.relative_to(root)) for item in sorted(root.rglob("*")) if item.is_file())
            spec = top / "SPECS/demo.spec"
            spec.write_text(
                f"Name: {name}\nVersion: {os.environ['PACKAGE_UPSTREAM_VERSION']}\nRelease: {os.environ['PACKAGE_RELEASE']}\n"
                "Summary: Graph-driven package pipeline acceptance project\nLicense: LicenseRef-Unspecified\n"
                + requirements
                + "%description\nVOC-Experiments sandbox project.\n%install\nmkdir -p %{buildroot}\n"
                "cp -a %{_sourcedir}/root/. %{buildroot}/\n%files\n" + files + "\n"
            )
            run(
                [
                    "rpmbuild",
                    "--target",
                    arch,
                    "--define",
                    "_topdir " + str(top),
                    "--define",
                    "_tmppath " + str(top / "tmp"),
                    "--define",
                    "_dbpath " + str(top / "db"),
                    "--define",
                    "_build_id_links none",
                    "--define",
                    "debug_package %{nil}",
                    "-bb",
                    spec,
                ]
            )
            built = list((top / "RPMS").rglob("*.rpm"))
            assert len(built) == 1
            shutil.copyfile(built[0], output / (name + ".rpm"))
        elif package_format == "apk":
            arguments = [
                Path(os.environ["SDK_DIR"]) / "staging_dir/host/bin/apk",
                "mkpkg",
                "--files",
                root,
                "--info",
                "name:" + name,
                "--info",
                "version:" + version,
                "--info",
                "arch:" + arch,
                "--info",
                "description:VOC-Experiments sandbox project",
                "--info",
                "license:LicenseRef-Unspecified",
            ]
            arguments += ["--info", "depends:" + " ".join(["libc"] + dependencies)]
            run(arguments + ["--output", output / (name + ".apk")])
        else:
            raise ValueError("Unsupported sandbox package format")
        (output / "outputs.json").write_text(json.dumps(declared) + "\n")


if __name__ == "__main__":
    main()
