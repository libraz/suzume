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
