"""Live OpenWrt client acceptance on GitHub; no controller or signing credentials."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import shlex
import sys
import subprocess
import tempfile


def download(url, destination, checksum=None):
    subprocess.run(
        [
            "curl",
            "--fail",
            "--silent",
            "--show-error",
            "--max-time",
            "180",
            "--max-filesize",
            str(1024**3),
            "--output",
            str(destination),
            url,
        ],
        check=True,
    )
    if checksum is not None:
        assert hashlib.sha256(destination.read_bytes()).hexdigest() == checksum, "download checksum mismatch"


def container(image, public, script, package, command):
    result = subprocess.run(
        [
            "docker",
            "run",
            "--rm",
            "--cap-drop=ALL",
            "--cap-add=CHOWN",
            "--cap-add=DAC_OVERRIDE",
            "--cap-add=FOWNER",
            "--cap-add=SETUID",
            "--cap-add=SETGID",
            "--security-opt=no-new-privileges",
            "--pids-limit=256",
            "--memory=1g",
            "--cpus=2",
            "--mount",
            "type=bind,source=" + str(public) + ",target=/proof,readonly",
            "--env",
            "PACKAGE=" + package["name"],
            "--env",
            "VERSION=" + package["version"],
            "--env",
            "COMMAND=" + command,
            image,
            "/bin/sh",
            "-ec",
            script,
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=180,
    )
    print(result.stdout, flush=True)
    return result


def version_comparisons(package_format):
    # Use deployment-owned codec and actual selected OpenWrt client comparators.
    sys.path.insert(0, str(Path("core_ci").resolve()))
    from contract import versions

    tags = [
        "0.9.9",
        "1.0.0-0",
        "1.0.0-1",
        "1.0.0-2",
        "1.0.0-10",
        "1.0.0-A",
        "1.0.0-Z",
        "1.0.0-a",
        "1.0.0-a-",
        "1.0.0-a0",
        "1.0.0-alpha",
        "1.0.0-alpha.0",
        "1.0.0-alpha.1",
        "1.0.0-alpha.2",
        "1.0.0-alpha.10",
        "1.0.0-alpha.A",
        "1.0.0-alpha.a",
        "1.0.0-alpha.beta",
        "1.0.0-beta",
        "1.0.0-rc.1",
        "1.0.0",
        "1.0.1-0",
        "1.0.1",
        "2.0.0",
    ]
    pairs = [
        (versions(left, 7, package_format)["full"], versions(right, 1, package_format)["full"])
        for left, right in zip(tags, tags[1:])
    ]
    pairs.append((versions("1.0.0", 1, package_format)["full"], versions("1.0.0", 2, package_format)["full"]))
    commands = []
    for left, right in pairs:
        a, b = shlex.quote(left), shlex.quote(right)
        if package_format == "ipk":
            commands.append("opkg compare-versions " + a + " '<' " + b)
        else:
            commands.append('test "$(apk version -t ' + a + " " + b + ')" = "<"')
    result = "\n".join(commands) + "\necho 'PASS OpenWrt SemVer/rebuild ordering'\n"
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--receipt", required=True)
    parser.add_argument("--keys", required=True)
    parser.add_argument("--command", required=True)
    parser.add_argument("--expected-output", required=True)
    args = parser.parse_args()
    assert re.fullmatch(r"receipts/[a-f0-9]{64}/[0-9]+\.json", args.receipt)
    base = os.environ["FEED_BASE_URL"].rstrip("/")
    assert base.startswith("https://")
    with tempfile.TemporaryDirectory(dir=os.environ["RUNNER_TEMP"], prefix="core-ci-client-") as temporary:
        root = Path(temporary)
        public = root / "public"
        public.mkdir(mode=0o755)
        download(base + "/" + args.receipt, root / "receipt.json")
        receipt = json.loads((root / "receipt.json").read_text())
        assert receipt["feed"]["base_url"].rstrip("/") == base
        target = receipt["compatibility"]
        assert (
            target["family"] == "openwrt" and target["arch"] == "x86_64"
        ), "this live runner test requires OpenWrt x86_64"
        assert target["format"] in ("ipk", "apk") and len(receipt["packages"]) == 1
        package = receipt["packages"][0]
        folder = str(Path(package["path"]).parent)
        assert not Path(folder).is_absolute() and ".." not in Path(folder).parts
        feed = base + "/" + folder
        release = target["release"]
        assert re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", release)
        assert re.fullmatch(r"[a-z0-9_-]+/[a-z0-9_-]+", target["target"])
        origin = os.environ.get("OPENWRT_DOWNLOAD_BASE_URL", "https://downloads.openwrt.org").rstrip("/")
        directory = origin + "/releases/" + release + "/targets/" + target["target"] + "/"
        download(directory + "sha256sums", root / "sha256sums")
        filename = "openwrt-" + release + "-" + target["target"].replace("/", "-") + "-rootfs.tar.gz"
        matches = re.findall(
            r"^([a-f0-9]{64})\s+\*?" + re.escape(filename) + r"$", (root / "sha256sums").read_text(), re.MULTILINE
        )
        assert len(matches) == 1, "exact matching official root filesystem is unavailable"
        archive = root / "rootfs.tar.gz"
        download(directory + filename, archive, matches[0])
        image = "core-ci-client:" + target["format"]
        subprocess.run(["docker", "import", "--platform", "linux/amd64", str(archive), image], check=True)
        try:
            if target["format"] == "ipk":
                shutil.copyfile(Path(args.keys) / "opkg.pub", public / "opkg.pub")
                (public / "opkg.conf").write_text(
                    "dest root /\nlists_dir ext /var/opkg-lists\narch all 1\narch noarch 1\narch x86_64 10\n"
                    "option check_signature\nsrc/gz core-ci " + feed + "\n"
                )
                common = "mkdir -p /tmp /var/lock /var/opkg-lists\n"
                negative = common + 'opkg -f /proof/opkg.conf update\nopkg -f /proof/opkg.conf install "$PACKAGE"'
                positive = (
                    common
                    + "cp /proof/opkg.pub /etc/opkg/keys/$(usign -F -p /proof/opkg.pub)\n"
                    + (
                        'opkg -f /proof/opkg.conf update\nopkg -f /proof/opkg.conf install "$PACKAGE"\n'
                        'opkg status "$PACKAGE" | grep -Fx "Version: $VERSION"\n"$COMMAND"\n'
                    )
                )
            else:
                (public / "apk-keys").mkdir(mode=0o755)
                shutil.copyfile(Path(args.keys) / "core-ci-apk.pem", public / "apk-keys/core-ci-apk.pem")
                (public / "repositories").write_text(feed + "/packages.adb\n")
                negative = "mkdir -p /tmp\napk --keys-dir /etc/apk/keys --repositories-file /proof/repositories update"
                positive = (
                    "mkdir -p /tmp\napk --version\napk --keys-dir /proof/apk-keys --repositories-file /proof/repositories update\n"
                    'apk --keys-dir /proof/apk-keys --repositories-file /proof/repositories add "$PACKAGE=$VERSION"\n"$COMMAND"\n'
                )
            for path in public.rglob("*"):
                path.chmod(0o755 if path.is_dir() else 0o644)
            rejected = container(image, public, negative, package, args.command)
            assert rejected.returncode != 0 and re.search(
                r"untrusted|signature.*fail|fail.*signature", rejected.stdout, re.IGNORECASE
            ), "no signature rejection observed with unrelated keys"
            accepted = container(
                image,
                public,
                "mkdir -p /tmp /var/lock\n" + version_comparisons(target["format"]) + positive,
                package,
                args.command,
            )
            assert accepted.returncode == 0, "authenticated repository refresh/installation failed"
            assert accepted.stdout.strip().splitlines()[-1] == args.expected_output, "installed runtime output mismatch"
            print(
                json.dumps(
                    {
                        "receipt": args.receipt,
                        "rootfs": filename,
                        "sha256": matches[0],
                        "format": target["format"],
                        "result": "PASS",
                    }
                ),
                flush=True,
            )
        finally:
            subprocess.run(["docker", "image", "rm", image], check=False)


if __name__ == "__main__":
    main()
