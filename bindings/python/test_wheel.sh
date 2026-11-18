#!/bin/bash
# Run the binding test suite against one built wheel installed into a clean
# environment, so the tests exercise the packaged library and dictionaries
# rather than the checkout.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/wheel_env.sh"
resolve_wheel_argument "$@"
create_wheel_env suzume-wheel-test
TEST_ROOT="$WHEEL_ENV_ROOT"
# The test tools come from uv.lock at the versions the dev environment uses;
# the wheel itself goes in without dependency resolution so that nothing is
# re-resolved against the index.
uv export --project "$SCRIPT_DIR" --locked --only-group dev --no-emit-project \
    --format requirements.txt --output-file "$TEST_ROOT/dev-requirements.txt" --quiet
uv pip install --python "$TEST_ROOT/venv/bin/python" --require-hashes \
    --requirement "$TEST_ROOT/dev-requirements.txt"
install_wheel_into_env

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
