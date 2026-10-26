"""Trusted IPK/APKv3 signing and indexing using independently verified SDK host tools."""

import gzip
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from contract import file_hash, output, publication_receipt, read_json, require, run, verify_manifest, write_json
from sdk import prepare
from sign_native import (
    add_new,
    feed_component,
    file_identity,
    publish_export,
    publish_key,
    receipt_path,
    retained_receipt,
)


def update(selected, build_result, publication_run, artifacts, feed, private_key, public_key, temporary_root):
    feed = Path(feed).resolve()
    receipt = retained_receipt(selected, build_result, publication_run, feed)
    if receipt is None:
        # The source artifact supplies no executable or key authority. Re-read official metadata and verify the SDK again.
        environment = read_json(Path(artifacts) / "manifest.json")["environment"]
        sdk, staging = prepare(selected["target"], environment, Path(temporary_root) / "sdk")
        manifest = verify_manifest(selected, artifacts, build_result)
        target = selected["target"]
        package_format = target["build"]["format"]
        require(package_format in ("ipk", "apk"), "unsupported OpenWrt package format")
        key_field = "usign_key_sha256" if package_format == "ipk" else "apk_key_sha256"
        require(
            file_hash(public_key) == selected["source"]["deployment"]["signing"][key_field],
            "trusted OpenWrt public key mismatch",
        )
        require(Path(private_key).stat().st_mode & 0o077 == 0, "private key requires owner-only permissions")
        receipt = publication_receipt(selected, build_result, publication_run, manifest)
        folder = (
            feed
            / "openwrt"
            / feed_component(target["suite"])
            / feed_component(target["arch"])
            / package_format
            / environment["sdk"]["sha256"]
        )
        folder.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="verification-", dir=temporary_root) as temporary:
            scratch = Path(temporary)
            keys = scratch / "keys"
            keys.mkdir()
            if package_format == "ipk":
                tool = str(sdk / "staging_dir/host/bin/usign")
                require(
                    output([tool, "-F", "-s", str(private_key)]) == output([tool, "-F", "-p", str(public_key)]),
                    "usign private key differs from trusted public key",
                )
            else:
                tool = os.environ["CORE_CI_APK"]
                trusted = keys / Path(public_key).name
                shutil.copyfile(public_key, trusted)
                for source, destination, public in (
                    (private_key, scratch / "private-public.der", False),
                    (public_key, scratch / "trusted.der", True),
                ):
                    run(
                        ["openssl", "pkey"]
                        + (["-pubin"] if public else [])
                        + ["-in", str(source), "-pubout", "-outform", "DER", "-out", str(destination)],
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.PIPE,
                    )
                require(
                    file_hash(scratch / "private-public.der") == file_hash(scratch / "trusted.der"),
                    "APK private key differs from trusted public key",
                )
                # Require EC P-256, independently of the artifact and the secret's name.
                require(
                    "prime256v1" in output(["openssl", "pkey", "-pubin", "-in", str(public_key), "-text", "-noout"]),
                    "APK key must use EC P-256",
                )
            for package in manifest["packages"]:
                filename = package["name"] + "-" + package["version"] + "." + package_format
                destination = folder / filename
                add_new(Path(artifacts) / package["file"], destination)
                if package_format == "apk":
                    # Only unsigned build input is accepted here. All subsequent verification and client use require signatures.
                    run(
                        [
                            tool,
                            "--allow-untrusted",
                            "adbsign",
                            "--reset-signatures",
                            "--sign",
                            str(private_key),
                            str(destination),
                        ],
                        stderr=subprocess.PIPE,
                    )
                    run(
                        [tool, "--keys-dir", str(keys), "verify", str(destination)],
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.PIPE,
                    )
                receipt["packages"].append(
                    {
                        **package,
                        **file_identity(feed, destination),
                        "file": filename,
                        "unsigned_sha256": package["sha256"],
                    }
                )
            if package_format == "ipk":
                index = folder / "Packages"
                with index.open("wb") as outgoing:
                    subprocess.run(
                        ["/bin/bash", str(sdk / "scripts/ipkg-make-index.sh"), "."],
                        cwd=folder,
                        env={**os.environ, "MKHASH": str(sdk / "staging_dir/host/bin/mkhash")},
                        check=True,
                        stdout=outgoing,
                        stderr=subprocess.PIPE,
                        timeout=120,
                    )
                with index.open("rb") as incoming, (folder / "Packages.gz").open("wb") as outgoing:
                    with gzip.GzipFile(fileobj=outgoing, mode="wb", mtime=0) as compressed:
                        shutil.copyfileobj(incoming, compressed)
                signature = folder / "Packages.sig"
                run(
                    [tool, "-S", "-m", str(index), "-s", str(private_key), "-x", str(signature)], stderr=subprocess.PIPE
                )
                run([tool, "-V", "-m", str(index), "-p", str(public_key), "-x", str(signature)], stderr=subprocess.PIPE)
                indexes = [index, folder / "Packages.gz", signature]
            else:
                index = folder / "packages.adb"
                packages = sorted(str(path) for path in folder.glob("*.apk"))
                run(
                    [
                        tool,
                        "--keys-dir",
                        str(keys),
                        "mkndx",
                        "--sign",
                        str(private_key),
                        "--output",
                        str(index),
                        *packages,
                    ],
                    stderr=subprocess.PIPE,
                )
                run(
                    [tool, "--keys-dir", str(keys), "verify", str(index)],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.PIPE,
                )
                indexes = [index]
            receipt["indexes"] = [file_identity(feed, item) for item in indexes]
        publish_key(feed, public_key)
        receipt["development_export"] = publish_export(feed, selected, manifest, artifacts)
        path = feed / receipt_path(selected)
        path.parent.mkdir(parents=True, exist_ok=True)
        write_json(path, receipt)
    return receipt
