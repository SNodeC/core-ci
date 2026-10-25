"""Publication-only views derived from retained receipts, never execution state."""

import argparse
from html import escape
from pathlib import Path
import re
from urllib.parse import quote

from contract import read_json, relative_path, require, write_json


def render(feed):
    feed = Path(feed)
    rows = []
    for path in sorted((feed / "receipts").glob("*/*.json")):
        require(not path.is_symlink(), "status receipt must be a regular file")
        receipt = read_json(path)
        require(receipt["schema"] == 1, "unsupported status receipt schema")
        build = receipt["build_result"]["run"]["html_url"]
        publication = receipt["run"]["html_url"]
        require(
            re.fullmatch(r"https://github.com/([A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+)/actions/runs/[0-9]+", build),
            "invalid status build URL",
        )
        repository = build.split("/actions/runs/")[0].removeprefix("https://github.com/")
        require(
            re.fullmatch(r"https://github.com/[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+/actions/runs/[0-9]+", publication),
            "invalid status publication URL",
        )
        base = receipt["feed"]["base_url"].rstrip("/") + "/"
        require(base.startswith("https://"), "status feed requires HTTPS")
        target = receipt["compatibility"]
        for package in receipt["packages"]:
            relative_path(package["path"])
            rows.append(
                {
                    "repository": repository,
                    "target": "/".join(target[key] for key in ("distribution", "suite", "arch", "format")),
                    "package": package["name"],
                    "version": package["version"],
                    "source_commit": receipt["source_commit"],
                    "sha256": package["sha256"],
                    "package_url": base + quote(package["path"]),
                    "receipt_url": base + quote(path.relative_to(feed).as_posix()),
                    "build_url": build,
                    "publication_url": publication,
                }
            )
    rows.sort(key=lambda row: (row["repository"], row["target"], row["package"], row["version"], row["receipt_url"]))
    status = feed / "status"
    status.mkdir(exist_ok=True)
    note = "Published receipts only. Waiting, building and failed work are shown in the core-ci controller."
    paths = []
    for repository in [None] + sorted({row["repository"] for row in rows}):
        title = (repository or "Package feed") + " publications"
        table = (
            "<table><thead><tr>"
            + "".join(
                "<th>" + heading + "</th>" for heading in ("Repository", "Target", "Package", "Version", "Evidence")
            )
            + "</tr></thead><tbody>\n"
        )
        for row in rows:
            if repository is None or row["repository"] == repository:
                links = " · ".join(
                    '<a href="' + escape(row[key + "_url"], quote=True) + '">' + label + "</a>"
                    for key, label in (
                        ("package", "Package"),
                        ("build", "Build"),
                        ("publication", "Publication"),
                        ("receipt", "Receipt"),
                    )
                )
                table += (
                    "<tr>"
                    + "".join(
                        "<td>" + escape(row[key]) + "</td>" for key in ("repository", "target", "package", "version")
                    )
                    + "<td>"
                    + links
                    + "</td></tr>\n"
                )
        table += "</tbody></table>\n"
        name = repository.split("/")[1] if repository else "README"
        path = status / (name + ".md")
        path.write_text('<a id="banner"></a>\n\n# ' + title + "\n\n" + note + "\n\n" + table)
        paths.append(path.relative_to(feed).as_posix())
        if repository is None:
            page = '<!doctype html><html lang="en"><meta charset="utf-8"><title>' + escape(title) + "</title>"
            page += '<meta name="viewport" content="width=device-width"><body><h1>' + escape(title) + "</h1>"
            page += "<p>" + note + "</p>" + table + "</body></html>\n"
            for path in (feed / "index.html", status / "index.html"):
                path.write_text(page)
                paths.append(path.relative_to(feed).as_posix())
    write_json(feed / "status.json", {"schema": 1, "publications": rows})
    paths.append("status.json")
    return paths


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--feed", required=True)
    render(parser.parse_args().feed)
