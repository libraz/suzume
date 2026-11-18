#!/usr/bin/env bash
#
# WASM size regression gate. Suzume ships to the browser, so binary size is a product
# constraint: L1 entries, hardcoded lists, and static data all land in the .wasm and
# nobody notices until release. This fails when the built artifact exceeds the committed
# baseline by more than TOLERANCE_PCT, and always prints raw and reproducible
# gzip (-9 -n) sizes. Legacy one-number baselines remain raw-only compatible.
#
# The baseline is only comparable when it was recorded under the shipped build
# conditions, so record it with the pinned Emscripten version used by CI and with
# the git-tracked dictionary variant (`make wasm-dict`, which feeds the compiler
# only `git ls-files 'data/user/*.tsv'`). A baseline taken from a working tree that
# also holds untracked user dictionaries measures a binary nobody ships, and the
# gate silently gains headroom it does not have.
#
# Usage:
#   scripts/check_wasm_size.sh <path-to.wasm>          # check against baseline
#   scripts/check_wasm_size.sh <path-to.wasm> update   # set raw + gzip baseline

set -eu
cd "$(git rev-parse --show-toplevel)"

WASM="${1:?usage: check_wasm_size.sh <path-to.wasm> [update]}"
MODE="${2:-check}"
BASELINE="scripts/wasm-size-baseline.txt"
TOLERANCE_PCT=5

[ -f "$WASM" ] || { echo "❌ wasm artifact not found: $WASM"; exit 1; }
raw_size=$(wc -c < "$WASM" | tr -d ' ')
gzip_size=$(gzip -9 -n -c "$WASM" | wc -c | tr -d ' ')

if [ "$MODE" = "update" ]; then
  # Other gzip implementations compress the same module to a different size
  # (Apple's ~0.2% larger), so the recorded value is GNU gzip's, as in CI.
  case "$(gzip --version 2>&1 | head -1)" in
    "gzip "[0-9]*) ;;
    *) echo "❌ the baseline is recorded with GNU gzip; re-record on the build host with: make wasm-linux, then scripts/check_wasm_size.sh $WASM update there"; exit 1 ;;
  esac
  printf '%s\t%s\n' "$raw_size" "$gzip_size" > "$BASELINE"
  echo "✅ wasm-size baseline set to raw=$raw_size bytes ($((raw_size/1024)) KB), gzip=$gzip_size bytes"
  exit 0
fi

[ -f "$BASELINE" ] || { echo "❌ missing $BASELINE (run: scripts/check_wasm_size.sh $WASM update)"; exit 1; }
baseline_line=$(tr -d '\r\n' < "$BASELINE")
IFS="$(printf '\t')" read -r raw_base gzip_base <<EOF
$baseline_line
EOF

[ -n "$raw_base" ] || { echo "❌ invalid WASM baseline: $BASELINE"; exit 1; }
size_failed=0
# Print one measurement against its baseline, mirror it to the CI step summary,
# and flag it when it exceeds the tolerance.
compare_size() {
  local label="$1" size="$2" base="$3" limit delta pct
  limit=$(( base + base * TOLERANCE_PCT / 100 ))
  delta=$(( size - base ))
  pct=$(( delta * 100 / base ))
  printf 'wasm %s: %d KB (baseline %d KB, delta %+d KB / %+d%%)\n' \
    "$label" "$((size/1024))" "$((base/1024))" "$((delta/1024))" "$pct"
  if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
    printf '%s: %d KB (baseline %d KB, %+d%%)\n' \
      "$label" "$((size/1024))" "$((base/1024))" "$pct" >> "$GITHUB_STEP_SUMMARY"
  fi
  if [ "$size" -gt "$limit" ]; then
    size_failed=1
  fi
}

if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
  printf '### WASM size\n' >> "$GITHUB_STEP_SUMMARY"
fi
compare_size raw "$raw_size" "$raw_base"
if [ -n "${gzip_base:-}" ]; then
  compare_size gzip "$gzip_size" "$gzip_base"
else
  printf 'wasm gzip: %d KB (legacy baseline has no gzip value)\n' "$((gzip_size/1024))"
  if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
    printf 'gzip: %d KB (legacy baseline unavailable)\n' "$((gzip_size/1024))" >> "$GITHUB_STEP_SUMMARY"
  fi
fi

if [ "$size_failed" -ne 0 ]; then
  echo "❌ wasm grew more than ${TOLERANCE_PCT}% over baseline."
  echo "   Investigate raw CODE/DATA and gzip regressions."
  echo "   If intentional, run: scripts/check_wasm_size.sh $WASM update"
  exit 1
fi
echo "✅ wasm size within +${TOLERANCE_PCT}% of baseline"
