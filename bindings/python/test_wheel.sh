#!/bin/bash
# Run the binding test suite against one built wheel installed into a clean
# environment, so the tests exercise the packaged library and dictionaries
# rather than the checkout.
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 path/to/suzume-*.whl" >&2
    exit 2
fi

WHEEL_INPUT="$1"
if [[ ! -f "$WHEEL_INPUT" ]]; then
    echo "Error: wheel not found: $WHEEL_INPUT" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WHEEL_DIR="$(cd "$(dirname "$WHEEL_INPUT")" && pwd)"
WHEEL="$WHEEL_DIR/$(basename "$WHEEL_INPUT")"
PYTHON_BIN="${PYTHON_BIN:-python3}"
TEST_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/suzume-wheel-test.XXXXXX")"
trap 'rm -rf "$TEST_ROOT"' EXIT

"$PYTHON_BIN" -m venv "$TEST_ROOT/venv"
# The dev extra is the single place the test tool versions are declared.
"$TEST_ROOT/venv/bin/python" -m pip install "${WHEEL}[dev]"

export PYTHONNOUSERSITE=1
unset PYTHONPATH

# The tests live in the checkout; the package under test must not.
cd "$SCRIPT_DIR"
"$TEST_ROOT/venv/bin/python" - "$TEST_ROOT/venv" <<'PY'
import sys
from pathlib import Path

import suzume

package = Path(suzume.__file__).resolve()
environment = Path(sys.argv[1]).resolve()
assert environment in package.parents, f"tests would import the checkout: {package}"
PY

"$TEST_ROOT/venv/bin/python" -m pytest -q -p no:cacheprovider tests

echo "Wheel test suite passed: $(basename "$WHEEL")"
