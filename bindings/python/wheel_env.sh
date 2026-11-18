# shellcheck shell=bash
# Sourced by the wheel scripts in this directory after they set SCRIPT_DIR.

# uv supplies the interpreter pinned for the binding, never the python on PATH.
export UV_MANAGED_PYTHON=1
UV_PYTHON="$(<"$SCRIPT_DIR/.python-version")"
export UV_PYTHON

# Set WHEEL to the absolute path of the single wheel argument, or exit with usage.
# shellcheck disable=SC2034  # WHEEL is read by the sourcing script
resolve_wheel_argument() {
    if [[ $# -ne 1 ]]; then
        echo "Usage: $0 path/to/suzume-*.whl" >&2
        exit 2
    fi
    if [[ ! -f "$1" ]]; then
        echo "Error: wheel not found: $1" >&2
        exit 1
    fi
    WHEEL="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
}

# Create a throwaway environment root WHEEL_ENV_ROOT (removed at script exit)
# holding an empty venv; $1 is the mktemp name prefix.
# shellcheck disable=SC2034  # WHEEL_ENV_ROOT is read by the sourcing script and the trap
create_wheel_env() {
    WHEEL_ENV_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/$1.XXXXXX")"
    trap 'rm -rf "$WHEEL_ENV_ROOT"' EXIT
    uv venv "$WHEEL_ENV_ROOT/venv"
}

# Install $WHEEL into the venv without dependency resolution and isolate the
# environment from user site-packages and PYTHONPATH.
install_wheel_into_env() {
    uv pip install --python "$WHEEL_ENV_ROOT/venv/bin/python" --no-deps "$WHEEL"
    export PYTHONNOUSERSITE=1
    unset PYTHONPATH
}
