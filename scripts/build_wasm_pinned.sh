#!/usr/bin/env bash
#
# Build the WASM module with the Emscripten release CI pins, from an SDK that
# is installed beside other releases (default /opt/emsdk-<version>, override
# with EMSDK_PINNED_ROOT), then run the size gate on the result.
#
# The release is read from the setup-emsdk step in ci.yml, the one place the
# repository states it. The SDK is selected here rather than by the caller's
# environment, and the build stops unless emcc reports that release and its
# LLVM lies under the selected SDK: a module from any other compiler is not
# comparable with the size baseline.
#
# Every directory is a build-linux-* one so the native CLI that compiles the
# dictionaries and the Emscripten tree never touch build/ or build-wasm.
#
# Usage: scripts/build_wasm_pinned.sh            (JOBS caps the build workers)
set -eu
cd "$(git rev-parse --show-toplevel)"

version=$(scripts/emsdk_version.sh)
[ -n "$version" ] || { echo "❌ no setup-emsdk version in .github/workflows/ci.yml"; exit 1; }

sdk="${EMSDK_PINNED_ROOT:-/opt/emsdk-$version}"
[ -d "$sdk" ] || { echo "❌ Emscripten $version not installed at $sdk"; exit 1; }
sdk=$(cd "$sdk" && pwd -P)

# All three are needed: a login profile may export EM_CONFIG for another SDK,
# and emcc reads that before anything on PATH.
export EMSDK="$sdk"
export EM_CONFIG="$sdk/.emscripten"
export PATH="$sdk:$sdk/upstream/emscripten:$PATH"

reported=$(emcc --version | head -1)
case "$reported" in
  *" $version "*) ;;
  *) echo "❌ emcc is not $version: $reported"; exit 1 ;;
esac
llvm_root=$(cd "$(em-config LLVM_ROOT)" && pwd -P)
case "$llvm_root/" in
  "$sdk"/*) ;;
  *) echo "❌ em-config LLVM_ROOT ($llvm_root) is outside $sdk"; exit 1 ;;
esac
echo "✅ Emscripten $version at $sdk"

jobs="${JOBS:-4}"
cli_dir=build-linux-wasm-cli
cmake -B "$cli_dir" -DCMAKE_BUILD_TYPE=Release -DBUILD_WASM=OFF
cmake --build "$cli_dir" --target suzume-cli --parallel "$jobs"
make wasm BUILD_DIR="$cli_dir" WASM_BUILD_DIR=build-linux-wasm JOBS="$jobs"
scripts/check_wasm_size.sh bindings/wasm/dist/suzume.wasm
