#!/bin/sh
# Reference hook: compile, execute a test and package with distribution-owned tools.
set -eu
: "${SOURCE_DIR:?}" "${TARGET_JSON:?}" "${OUTPUT_DIR:?}" "${PACKAGE_NAMESPACE:?}"
: "${PACKAGE_VERSION:?}" "${PACKAGE_UPSTREAM_VERSION:?}" "${PACKAGE_RELEASE:?}"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cc -Wall -Wextra -Werror "$SOURCE_DIR/src/hello.c" -o "$work/hello"
test "$("$work/hello")" = 'core-ci reference package'
format=$(python3 -c 'import json,os; print(json.load(open(os.environ["TARGET_JSON"]))["build"]["format"])')
arch=$(python3 -c 'import json,os; print(json.load(open(os.environ["TARGET_JSON"]))["arch"])')
if [ "$format" = deb ]; then
    mkdir -p "$work/root/DEBIAN" "$work/root/usr/bin"
    cp "$work/hello" "$work/root/usr/bin/$PACKAGE_NAMESPACE"
    cat > "$work/root/DEBIAN/control" <<EOF
Package: $PACKAGE_NAMESPACE
Version: $PACKAGE_VERSION
Architecture: $arch
Maintainer: core-ci reference <noreply@example.invalid>
Description: Small standalone reference project
Depends: libc6
Section: devel
Priority: optional
EOF
    dpkg-deb --build --root-owner-group "$work/root" "$OUTPUT_DIR/$PACKAGE_NAMESPACE.deb"
elif [ "$format" = rpm ]; then
    mkdir -p "$work/rpm/SOURCES" "$work/rpm/SPECS" "$work/rpm/tmp" "$work/rpm/db"
    cp "$work/hello" "$work/rpm/SOURCES/hello"
    cat > "$work/rpm/SPECS/reference.spec" <<EOF
Name: $PACKAGE_NAMESPACE
Version: $PACKAGE_UPSTREAM_VERSION
Release: $PACKAGE_RELEASE
Summary: Small standalone reference project
License: MIT
Source0: hello
%description
Small reference package built and tested by a source-owned hook.
%install
mkdir -p %{buildroot}/usr/bin
cp %{SOURCE0} %{buildroot}/usr/bin/$PACKAGE_NAMESPACE
%files
/usr/bin/$PACKAGE_NAMESPACE
EOF
    rpmbuild --define "_topdir $work/rpm" --define "_tmppath $work/rpm/tmp" \
        --define "_dbpath $work/rpm/db" --define '_build_ncpus 2' \
        --define '_build_id_links none' --define 'debug_package %{nil}' \
        -bb "$work/rpm/SPECS/reference.spec"
    find "$work/rpm/RPMS" -name '*.rpm' -exec cp '{}' "$OUTPUT_DIR/$PACKAGE_NAMESPACE.rpm" \;
else
    printf '%s\n' 'Reference hook supports DEB and RPM only' >&2
    exit 1
fi
python3 - <<'PY'
import json
import os
import subprocess
from pathlib import Path
format = json.load(open(os.environ['TARGET_JSON']))['build']['format']
name = os.environ['PACKAGE_NAMESPACE']
declared = {'packages': [{'file': name + '.' + format, 'component': name}],
            'summary': {'compilers': [{'name': 'cc', 'version': subprocess.check_output(['cc', '--version'], text=True).splitlines()[0]}],
                        'tests': {'passed': 1, 'failed': 0, 'skipped': 0}}}
Path(os.environ['OUTPUT_DIR'], 'outputs.json').write_text(json.dumps(declared))
PY
