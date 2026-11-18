#!/bin/bash
# Build the Linux wheel inside the pinned manylinux image, then smoke-test it
# and run the binding test suite against it in that same image. CI runs this on
# every change and the publish workflow runs it for the release, so the image's
# compiler and the packaged wheel are both exercised before a tag exists.
#
# UV_VERSION comes from the calling workflow's env, the one place the uv
# release is pinned; the image's own CPython only bootstraps that uv.
set -euo pipefail

: "${UV_VERSION:?UV_VERSION must name the uv release to install in the image}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# The container runs as the invoking user, which has no home directory in the
# image; uv's cache, managed interpreters and tools live under this one.
docker run --rm \
    --user "$(id -u):$(id -g)" \
    --env HOME=/tmp/suzume-home \
    --env UV_VERSION \
    --volume "$PROJECT_ROOT:/project" \
    --workdir /project \
    "$(cat "$PROJECT_ROOT/scripts/manylinux-image.txt")" \
    /bin/bash -euxo pipefail -c '
        /opt/python/cp312-cp312/bin/python -m venv /tmp/suzume-uv
        /tmp/suzume-uv/bin/python -m pip install --no-cache-dir "uv==$UV_VERSION"
        export PATH="/tmp/suzume-uv/bin:$HOME/.local/bin:$PATH"
        export UV_MANAGED_PYTHON=1
        export UV_PYTHON="$(<bindings/python/.python-version)"
        uv tool install "cmake==3.31.6"
        bindings/python/build_wheel.sh
        for wheel in bindings/python/dist/*.whl; do
            bindings/python/smoke_wheel.sh "$wheel"
            bindings/python/test_wheel.sh "$wheel"
        done
    '
