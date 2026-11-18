#!/bin/sh
# Smoke-test supported CMake build/install configurations in isolated directories.

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_root=$(dirname "$script_dir")
smoke_root=$(mktemp -d "${TMPDIR:-/tmp}/suzume-cmake-smoke.XXXXXX")

cleanup() {
    if [ -n "${smoke_root:-}" ] && [ "$smoke_root" != "/" ]; then
        rm -rf "$smoke_root"
    fi
}
trap cleanup EXIT HUP INT TERM

# A bare `--parallel` hands the native tool an unbounded job count. Build
# through CMAKE_BUILD_PARALLEL_LEVEL instead, defaulting to the processor count
# capped at 16; a caller's own value wins.
if [ -z "${CMAKE_BUILD_PARALLEL_LEVEL:-}" ]; then
    processors=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)
    if [ "$processors" -gt 16 ]; then
        processors=16
    fi
    CMAKE_BUILD_PARALLEL_LEVEL=$processors
fi
export CMAKE_BUILD_PARALLEL_LEVEL

require_file() {
    if [ ! -f "$1" ]; then
        echo "Expected file was not produced: $1" >&2
        exit 1
    fi
}

require_absent() {
    if [ -e "$1" ]; then
        echo "Unexpected installed artifact: $1" >&2
        exit 1
    fi
}

echo "[cmake-smoke] default build and install"
default_build="$smoke_root/default-build"
default_prefix="$smoke_root/default-prefix"
cmake -S "$project_root" -B "$default_build" \
    -DBUILD_TESTING=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$default_prefix"
cmake --build "$default_build"
cmake --build "$default_build" --target build-dict
cmake --install "$default_build"
require_file "$default_prefix/bin/suzume-cli"
require_file "$default_prefix/share/suzume/core.dic"
require_file "$default_prefix/share/suzume/user.dic"

echo "[cmake-smoke] BUILD_CLI=OFF library-only install"
library_build="$smoke_root/library-build"
library_prefix="$smoke_root/library-prefix"
cmake -S "$project_root" -B "$library_build" \
    -DBUILD_CLI=OFF \
    -DBUILD_TESTING=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$library_prefix"
cmake --build "$library_build"
cmake --install "$library_build"
require_file "$library_prefix/include/suzume/suzume_c.h"
require_absent "$library_prefix/bin/suzume-cli"
require_absent "$library_prefix/share/suzume/core.dic"
require_absent "$library_prefix/share/suzume/user.dic"

echo "[cmake-smoke] SUZUME_INSTALL=OFF build without install artifacts"
no_install_build="$smoke_root/no-install-build"
no_install_prefix="$smoke_root/no-install-prefix"
cmake -S "$project_root" -B "$no_install_build" \
    -DBUILD_TESTING=OFF \
    -DSUZUME_INSTALL=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$no_install_prefix"
cmake --build "$no_install_build"
require_file "$no_install_build/bin/suzume-cli"
cmake --install "$no_install_build"
require_absent "$no_install_prefix/bin/suzume-cli"

if command -v ninja >/dev/null 2>&1; then
    echo "[cmake-smoke] Ninja Multi-Config build-dict"
    multi_build="$smoke_root/multi-config-build"
    cmake -S "$project_root" -B "$multi_build" \
        -G "Ninja Multi-Config" \
        -DBUILD_TESTING=OFF \
        -DSUZUME_INSTALL=OFF
    cmake --build "$multi_build" --config Debug --target build-dict
    require_file "$multi_build/bin/Debug/suzume-cli"
else
    echo "[cmake-smoke] SKIP Ninja Multi-Config (ninja not available)"
fi

echo "[cmake-smoke] all configurations passed"
