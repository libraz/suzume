#!/usr/bin/env bash
#
# Version-mirror gate. The version lives in the root CMakeLists.txt project()
# declaration (single source of truth) and is hand-mirrored into every binding
# manifest. This fails if any mirror drifts from the canonical version.
set -eu
cd "$(git rev-parse --show-toplevel)"

canonical=$(grep -Eo 'project\(suzume VERSION [0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt \
  | grep -Eo '[0-9]+\.[0-9]+\.[0-9]+')

if [ -z "${canonical:-}" ]; then
  echo "❌ could not read canonical version from CMakeLists.txt"
  exit 1
fi

status=0
check() {
  local label="$1" file="$2" found="$3" expected="${4:-$canonical}"
  if [ "$found" != "$expected" ]; then
    echo "❌ $label ($file): $found != $expected"
    status=1
  else
    echo "✅ $label: $found"
  fi
}

wasm_ver=$(grep -Eo '"version": *"[0-9]+\.[0-9]+\.[0-9]+"' bindings/wasm/package.json \
  | grep -Eo '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
check "wasm package.json" bindings/wasm/package.json "$wasm_ver"

py_ver=$(grep -Eo '^version = "[0-9]+\.[0-9]+\.[0-9]+"' bindings/python/pyproject.toml \
  | grep -Eo '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
check "python pyproject.toml" bindings/python/pyproject.toml "$py_ver"

# uv.lock records the project's own version, so a bump that skips `uv lock`
# leaves a stale lock that `uv run --locked` rejects.
lock_ver=$(awk '
  /^\[\[package\]\]/ { in_project = 0 }
  /^name = "suzume"$/ { in_project = 1; next }
  in_project && /^version = / { gsub(/"/, "", $3); print $3; exit }
' bindings/python/uv.lock 2>/dev/null || true)
check "python uv.lock" bindings/python/uv.lock "${lock_ver:-missing}"
if [ "${lock_ver:-missing}" != "$canonical" ]; then
  echo "   refresh it with: (cd bindings/python && uv lock)"
fi

# Every workflow provisions its tools with one uv, one mise and one Emscripten
# release: ci.yml states each (uv and mise in its workflow-level env, Emscripten
# in its setup-emsdk step) and publish.yml mirrors it.
tool_version() {
  case "$2" in
    setup-emsdk) scripts/emsdk_version.sh "$1" ;;
    *) grep -Eo "^  $2: *\"[^\"]+\"" "$1" 2>/dev/null | grep -Eo '"[^"]+"' | tr -d '"' | head -1 ;;
  esac
}
for tool_var in UV_VERSION MISE_VERSION setup-emsdk; do
  ci_ver=$(tool_version .github/workflows/ci.yml "$tool_var")
  publish_ver=$(tool_version .github/workflows/publish.yml "$tool_var")
  if [ -z "$ci_ver" ]; then
    echo "❌ ci.yml $tool_var (.github/workflows/ci.yml): missing"
    status=1
  else
    check "publish.yml $tool_var" .github/workflows/publish.yml "${publish_ver:-missing}" "$ci_ver"
  fi
done

if [ "$status" -eq 0 ]; then
  echo "All version mirrors consistent: $canonical"
fi
exit "$status"
