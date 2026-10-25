"""Install sandbox native packages through signed HTTPS repositories on GitHub."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from openwrt_clients import container, download


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
    with tempfile.TemporaryDirectory(dir=os.environ["RUNNER_TEMP"], prefix="core-ci-native-client-") as temporary:
        root = Path(temporary)
        public = root / "public"
        public.mkdir(mode=0o755)
        download(base + "/" + args.receipt, root / "receipt.json")
        receipt = json.loads((root / "receipt.json").read_text())
        assert receipt["feed"]["base_url"].rstrip("/") == base
        target = receipt["compatibility"]
        assert target["family"] == "native" and target["format"] in ("deb", "rpm")
        assert len(receipt["packages"]) == 1
        package = receipt["packages"][0]
        image = receipt["environment"]["resolved_image"]
        assert re.fullmatch(r"[a-zA-Z0-9./:_-]+@sha256:[a-f0-9]{64}", image)
        subprocess.run(["docker", "pull", "--platform", target["platform"], image], check=True)
        shutil.copyfile(Path(args.keys) / "apt.asc", public / "apt.asc")
        if target["format"] == "deb":
            assert target["distribution"] == "debian", "this acceptance fixture uses Debian"
            feed = base + "/apt/" + target["distribution"] + "/" + target["suite"]
            for name, key in (("bad", "/usr/share/keyrings/debian-archive-keyring.gpg"), ("good", "/proof/apt.asc")):
                (public / (name + ".list")).write_text(
                    "deb [signed-by=" + key + "] " + feed + " " + target["suite"] + " main\n"
                )
            bootstrap = "apt-get update\napt-get install -y ca-certificates\n"
            options = " -o Dir::Etc::sourceparts=- -o APT::Update::Error-Mode=any"
            negative = bootstrap + "apt-get -o Dir::Etc::sourcelist=/proof/bad.list" + options + " update\n"
            apt = "apt-get -o Dir::Etc::sourcelist=/proof/good.list" + options
            positive = (
                bootstrap + apt + " update\n" + apt + ' install -y "$PACKAGE=$VERSION"\n'
                'test "$(dpkg-query -W -f=\'${Version}\' "$PACKAGE")" = "$VERSION"\n"$COMMAND"\n'
            )
        else:
            assert target["distribution"] == "fedora", "this acceptance fixture uses Fedora"
            folder = Path(package["path"]).parent
            assert not folder.is_absolute() and ".." not in folder.parts
            for name, key in (("bad", "/etc/pki/rpm-gpg/RPM-GPG-KEY-fedora-43-primary"), ("good", "/proof/apt.asc")):
                directory = public / name
                directory.mkdir(mode=0o755)
                (directory / "core-ci.repo").write_text(
                    "[core-ci]\nname=core-ci acceptance\nbaseurl="
                    + base
                    + "/"
                    + str(folder)
                    + "\nenabled=1\ngpgcheck=1\nrepo_gpgcheck=1\ngpgkey=file://"
                    + key
                    + "\n"
                )
            negative = (
                "dnf -y --setopt=reposdir=/proof/bad makecache\n"
                'dnf -y --setopt=reposdir=/proof/bad install "$PACKAGE-$VERSION"\n'
            )
            positive = (
                "dnf -y --setopt=reposdir=/proof/good makecache\n"
                'dnf -y --setopt=reposdir=/proof/good install "$PACKAGE-$VERSION"\n'
                'test "$(rpm -q --qf \'%{VERSION}-%{RELEASE}\' "$PACKAGE")" = "$VERSION"\n"$COMMAND"\n'
            )
        for path in public.rglob("*"):
            path.chmod(0o755 if path.is_dir() else 0o644)
        rejected = container(image, public, negative, package, args.command)
        assert rejected.returncode != 0 and re.search(
            r"signature|public key|signing key|NO_PUBKEY", rejected.stdout, re.IGNORECASE
        ), "no repository signature rejection observed with unrelated keys"
        accepted = container(image, public, positive, package, args.command)
        assert accepted.returncode == 0, "authenticated repository refresh/installation failed"
        assert accepted.stdout.strip().splitlines()[-1] == args.expected_output, "installed runtime output mismatch"
        print(
            json.dumps({"receipt": args.receipt, "image": image, "format": target["format"], "result": "PASS"}),
            flush=True,
        )


if __name__ == "__main__":
    main()
