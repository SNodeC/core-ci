FROM debian:bookworm
RUN apt-get update && apt-get install -y --no-install-recommends build-essential cmake ninja-build pkg-config ca-certificates git libbluetooth-dev libmagic-dev libmariadb-dev mariadb-server libssl-dev openssl libncurses-dev nlohmann-json3-dev libsqlite3-dev python3 python3-numpy python3-pil
COPY snodec /dependencies/snodec
RUN cmake -S /dependencies/snodec -B /tmp/codex/build/snode.c/bookworm -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DSNODEC_BUILD_TESTS=OFF -DSNODEC_BUILD_APPS=OFF && cmake --build /tmp/codex/build/snode.c/bookworm --parallel 4 && cmake --install /tmp/codex/build/snode.c/bookworm && ldconfig
COPY idp /dependencies/idp
RUN cmake -S /dependencies/idp -B /tmp/codex/build/snode.c-idp/bookworm -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DAUTH_CLIENT_ONLY=ON && cmake --build /tmp/codex/build/snode.c-idp/bookworm --parallel 4 && cmake --install /tmp/codex/build/snode.c-idp/bookworm --component auth-client
COPY source /source
RUN cmake -S /source -B /tmp/codex/build/core-ci/bookworm -G Ninja -DCMAKE_BUILD_TYPE=Release -DCORECI_WITH_SNODEC=ON && cmake --build /tmp/codex/build/core-ci/bookworm --parallel 4
RUN useradd -u 1000 -U -M -d /tmp diagnostic && chown -R 1000:1000 /tmp/codex/build/core-ci/bookworm
USER 1000:1000
CMD ["python3", "/source/tests/package/nonbrowser.py", "/tmp/codex/build/core-ci/bookworm"]
