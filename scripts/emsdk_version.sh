#!/usr/bin/env bash
#
# Print the Emscripten release a workflow's setup-emsdk step pins (default
# .github/workflows/ci.yml, the canonical place), or nothing if it pins none.
set -eu
awk '
  /uses: mymindstorm\/setup-emsdk@/ { in_step = 1; next }
  in_step && /version:/ { sub(/.*version:[ \t]*/, ""); print; exit }
' "${1:-.github/workflows/ci.yml}" 2>/dev/null || true
