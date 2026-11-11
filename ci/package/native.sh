#!/bin/sh
# Dependency installation belongs to trusted preparation.
set -eu
: "${SOURCE_DIR:?}" "${TARGET_JSON:?}" "${OUTPUT_DIR:?}"
format=$(python3 -c 'import json,os; print(json.load(open(os.environ["TARGET_JSON"]))["build"]["format"])')
if [ "$format" = deb ]; then
    libdir="lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)"
else
    libdir=lib64
    if [ -f /opt/rh/gcc-toolset-14/enable ]; then
        . /opt/rh/gcc-toolset-14/enable
    fi
fi
build=/work/build-product
cmake -S "$SOURCE_DIR" -B "$build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_INSTALL_LIBDIR="$libdir" -DCORECI_WITH_SNODEC=ON
cmake --build "$build" --parallel "$(nproc)"
ctest --test-dir "$build" -L '^(core|persistence|adapters)$' \
    --output-on-failure --output-junit core-ci-tests.xml
python3 /tool/native_container.py --package "$build"
