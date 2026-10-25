#!/bin/sh
# Standalone reference: compile with the selected SDK and use its standard package tools.
set -eu
: "${SOURCE_DIR:?}" "${SDK_DIR:?}" "${SDK_STAGING_DIR:?}" "${OUTPUT_DIR:?}" "${TARGET_JSON:?}"
: "${PACKAGE_NAMESPACE:?}" "${PACKAGE_VERSION:?}"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cat > "$work/query.mk" <<EOF
TOPDIR:=$SDK_DIR
SDK:=1
include \$(TOPDIR)/rules.mk
EOF
compiler=$(make --no-print-directory --silent -C "$SDK_DIR" -f "$work/query.mk" val.TARGET_CC)
format=$(python3 -c 'import json,os; print(json.load(open(os.environ["TARGET_JSON"]))["build"]["format"])')
arch=$(python3 -c 'import json,os; print(json.load(open(os.environ["TARGET_JSON"]))["arch"])')
mkdir -p "$work/root/usr/bin"
"$compiler" -Wall -Wextra -Werror -I"$SDK_STAGING_DIR/usr/include" -L"$SDK_STAGING_DIR/usr/lib" \
    "$SOURCE_DIR/src/hello.c" -o "$work/root/usr/bin/$PACKAGE_NAMESPACE"
if [ "$format" = ipk ]; then
    mkdir "$work/root/CONTROL"
    cat > "$work/root/CONTROL/control" <<EOF
Package: $PACKAGE_NAMESPACE
Version: $PACKAGE_VERSION
Architecture: $arch
Description: Small standalone SDK reference project
Depends: libc
Section: utils
Priority: optional
EOF
    "$SDK_DIR/scripts/ipkg-build" "$work/root" "$OUTPUT_DIR"
elif [ "$format" = apk ]; then
    "$SDK_DIR/staging_dir/host/bin/apk" mkpkg --files "$work/root" \
        --info "name:$PACKAGE_NAMESPACE" --info "version:$PACKAGE_VERSION" --info "arch:$arch" \
        --info 'description:Small standalone SDK reference project' --info 'license:MIT' --info 'depends:libc' \
        --output "$OUTPUT_DIR/$PACKAGE_NAMESPACE-$PACKAGE_VERSION.apk"
else
    echo 'Unsupported OpenWrt package format' >&2
    exit 1
fi
python3 - <<'PY'
import json, os
from pathlib import Path
root = Path(os.environ['OUTPUT_DIR'])
files = list(root.iterdir())
assert len(files) == 1
(root / 'outputs.json').write_text(json.dumps({'packages': [{'file': files[0].name, 'component': os.environ['PACKAGE_NAMESPACE']}]}))
PY
# Cross-target runtime execution requires a compatible target root/emulator; acceptance must record it separately.
