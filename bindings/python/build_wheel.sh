#!/bin/bash
# Build and verify a binary wheel from a git checkout.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PYTHON_PKG="$SCRIPT_DIR/src/suzume"
SYSTEM="$(uname -s)"
ARCH="$(uname -m)"

case "$SYSTEM/$ARCH" in
    Darwin/arm64)
        LIB_NAME="libsuzume.dylib"
        PLATFORM_TAG="macosx_11_0_arm64"
        DEPLOYMENT_TARGET="11.0"
        BUILD_PLATFORM="macos-arm64"
        ;;
    Darwin/x86_64)
        LIB_NAME="libsuzume.dylib"
        PLATFORM_TAG="macosx_10_15_x86_64"
        DEPLOYMENT_TARGET="10.15"
        BUILD_PLATFORM="macos-x86_64"
        ;;
    Linux/x86_64)
        LIB_NAME="libsuzume.so"
        PLATFORM_TAG="manylinux_2_17_x86_64"
        BUILD_PLATFORM="linux-x86_64"
        ;;
    *)
        echo "Error: wheel builds are unsupported on $SYSTEM/$ARCH" >&2
        exit 1
        ;;
esac

# A platform-specific directory prevents an older local configuration from
# silently reusing objects compiled for another architecture or deployment target.
BUILD_DIR="${SUZUME_PYTHON_BUILD_DIR:-$PROJECT_ROOT/build-python-$BUILD_PLATFORM}"

# uv supplies the build frontend and every packaging tool at the versions
# pinned here, so nothing depends on the python found on PATH.
WHEEL_VERSION="0.48.0"
AUDITWHEEL_VERSION="6.8.2"
PATCHELF_VERSION="0.19.3.0"
source "$SCRIPT_DIR/wheel_env.sh"

wheel_tool() {
    uv tool run --from "wheel==$WHEEL_VERSION" wheel "$@"
}

auditwheel_tool() {
    uv tool run --from "auditwheel==$AUDITWHEEL_VERSION" \
        --with "patchelf==$PATCHELF_VERSION" auditwheel "$@"
}

# Set SINGLE_WHEEL to the only wheel in dist/, or fail describing $1.
require_single_wheel() {
    local wheels=("$SCRIPT_DIR"/dist/*.whl)
    if [[ ${#wheels[@]} -ne 1 ]]; then
        echo "Error: expected exactly one $1" >&2
        exit 1
    fi
    SINGLE_WHEEL="${wheels[0]}"
}

if ! git -C "$PROJECT_ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "Error: public wheels must be built from a git checkout" >&2
    exit 1
fi

CORE_TSV_FILES=()
while IFS= read -r -d '' TRACKED_PATH; do
    CORE_TSV_FILES+=("$PROJECT_ROOT/$TRACKED_PATH")
done < <(git -C "$PROJECT_ROOT" ls-files -z -- 'data/core/*.tsv')

USER_TSV_FILES=()
while IFS= read -r -d '' TRACKED_PATH; do
    USER_TSV_FILES+=("$PROJECT_ROOT/$TRACKED_PATH")
done < <(git -C "$PROJECT_ROOT" ls-files -z -- 'data/user/*.tsv')

if [[ ${#CORE_TSV_FILES[@]} -eq 0 || ${#USER_TSV_FILES[@]} -eq 0 ]]; then
    echo "Error: no git-tracked dictionary TSVs found" >&2
    exit 1
fi

CMAKE_ARGS=(
    -B "$BUILD_DIR"
    -DCMAKE_BUILD_TYPE=Release
    -DBUILD_SHARED=ON
    -DBUILD_CLI=ON
    -DBUILD_TESTING=OFF
    -DENABLE_DEBUG_INFO=OFF
    -DENABLE_DEBUG_LOG=OFF
    -DSUZUME_LIB_SOVERSION=OFF
)

if [[ "$SYSTEM" == "Darwin" ]]; then
    # Both variables are set before configuration so compiler feature checks and
    # every native object use the same minimum OS as the wheel tag.
    export MACOSX_DEPLOYMENT_TARGET="$DEPLOYMENT_TARGET"
    CMAKE_ARGS+=(
        -DCMAKE_OSX_ARCHITECTURES="$ARCH"
        -DCMAKE_OSX_DEPLOYMENT_TARGET="$DEPLOYMENT_TARGET"
    )
fi

echo "=== Building suzume shared library + dictionary compiler (Release) ==="
cmake "${CMAKE_ARGS[@]}" "$PROJECT_ROOT"
# A bare `--parallel` is an unbounded job count; a caller's
# CMAKE_BUILD_PARALLEL_LEVEL wins over the processor count.
cmake --build "$BUILD_DIR" --target suzume_shared suzume-cli \
    --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-$(getconf _NPROCESSORS_ONLN)}"

BUILT_LIBRARY="$BUILD_DIR/lib/$LIB_NAME"
if [[ ! -f "$BUILT_LIBRARY" ]]; then
    echo "Error: expected shared library was not built: $BUILT_LIBRARY" >&2
    exit 1
fi

if [[ "$SYSTEM" == "Darwin" ]]; then
    BUILT_ARCHS="$(lipo -archs "$BUILT_LIBRARY")"
    if [[ "$BUILT_ARCHS" != "$ARCH" ]]; then
        echo "Error: dylib architecture '$BUILT_ARCHS' does not match wheel architecture '$ARCH'" >&2
        exit 1
    fi

    MIN_OS="$(
        otool -l "$BUILT_LIBRARY" | awk '
            $1 == "cmd" {
                build_version = ($2 == "LC_BUILD_VERSION")
                legacy_version = ($2 == "LC_VERSION_MIN_MACOSX")
                next
            }
            build_version && $1 == "minos" { print $2; exit }
            legacy_version && $1 == "version" { print $2; exit }
        '
    )"
    if [[ "$MIN_OS" != "$DEPLOYMENT_TARGET" ]]; then
        echo "Error: dylib minimum macOS '$MIN_OS' does not match wheel tag target '$DEPLOYMENT_TARGET'" >&2
        exit 1
    fi
fi

echo "=== Compiling dictionaries from git-tracked TSVs ==="
"$BUILD_DIR/bin/suzume-cli" dict compile "${CORE_TSV_FILES[@]}" "$PYTHON_PKG/core.dic"
"$BUILD_DIR/bin/suzume-cli" dict compile "${USER_TSV_FILES[@]}" "$PYTHON_PKG/user.dic"

echo "=== Bundling shared library + dictionaries ==="
rm -f "$PYTHON_PKG/libsuzume.dylib" "$PYTHON_PKG/libsuzume.so"
cp "$BUILT_LIBRARY" "$PYTHON_PKG/$LIB_NAME"
if [[ "$SYSTEM" == "Darwin" ]]; then
    install_name_tool -id "@loader_path/$LIB_NAME" "$PYTHON_PKG/$LIB_NAME"
fi

echo "=== Building wheel ==="
rm -rf "$SCRIPT_DIR/dist"
mkdir -p "$SCRIPT_DIR/dist"
uv build --wheel --out-dir "$SCRIPT_DIR/dist" "$SCRIPT_DIR"

shopt -s nullglob
require_single_wheel "wheel before platform processing"
INITIAL_WHEEL="$SINGLE_WHEEL"

if [[ "$SYSTEM" == "Darwin" ]]; then
    # The dylib target was verified above; only now is it safe to apply the
    # corresponding platform tag.
    wheel_tool tags --platform-tag "$PLATFORM_TAG" --remove "$INITIAL_WHEEL"
else
    # auditwheel needs a Linux wheel as input. This is an intermediate tag only:
    # the public manylinux tag is produced and validated by auditwheel repair.
    wheel_tool tags --platform-tag "linux_x86_64" --remove "$INITIAL_WHEEL"
    WHEELS=("$SCRIPT_DIR"/dist/*.whl)
    INITIAL_WHEEL="${WHEELS[0]}"
    REPAIR_DIR="$SCRIPT_DIR/dist-repaired"
    rm -rf "$REPAIR_DIR"
    mkdir -p "$REPAIR_DIR"
    auditwheel_tool repair --plat "$PLATFORM_TAG" --wheel-dir "$REPAIR_DIR" "$INITIAL_WHEEL"
    rm -f "$INITIAL_WHEEL"
    mv "$REPAIR_DIR"/*.whl "$SCRIPT_DIR/dist/"
    rmdir "$REPAIR_DIR"

    require_single_wheel "repaired Linux wheel"
    AUDIT_OUTPUT="$(auditwheel_tool show "$SINGLE_WHEEL")"
    printf '%s\n' "$AUDIT_OUTPUT"
    if [[ "$AUDIT_OUTPUT" != *"$PLATFORM_TAG"* ]]; then
        echo "Error: auditwheel did not validate $PLATFORM_TAG compatibility" >&2
        exit 1
    fi
fi

echo "=== Done ==="
ls -lh "$SCRIPT_DIR"/dist/*.whl
