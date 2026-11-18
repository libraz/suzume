#!/bin/bash
# Build the Linux wheel inside the pinned manylinux image, then smoke-test it
# and run the binding test suite against it in that same image. CI runs this on
# every change and the publish workflow runs it for the release, so the image's
# compiler and the packaged wheel are both exercised before a tag exists.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

docker run --rm \
    --user "$(id -u):$(id -g)" \
    --volume "$PROJECT_ROOT:/project" \
    --workdir /project \
    "$(cat "$PROJECT_ROOT/scripts/manylinux-image.txt")" \
    /bin/bash -euxo pipefail -c '
        /opt/python/cp312-cp312/bin/python -m venv /tmp/suzume-wheel-build
        source /tmp/suzume-wheel-build/bin/activate
        python -m pip install --no-cache-dir \
            "auditwheel==6.4.2" \
            "cmake==3.31.6" \
            "wheel==0.45.1"
        bindings/python/build_wheel.sh
        for wheel in bindings/python/dist/*.whl; do
            bindings/python/smoke_wheel.sh "$wheel"
            bindings/python/test_wheel.sh "$wheel"
        done
    '
