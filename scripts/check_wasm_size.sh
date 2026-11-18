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
raw_limit=$(( raw_base + raw_base * TOLERANCE_PCT / 100 ))
raw_delta=$(( raw_size - raw_base ))
raw_pct=$(( raw_delta * 100 / raw_base ))

printf 'wasm raw: %d KB (baseline %d KB, delta %+d KB / %+d%%)\n' \
  "$((raw_size/1024))" "$((raw_base/1024))" "$((raw_delta/1024))" "$raw_pct"

gzip_failed=0
if [ -n "${gzip_base:-}" ]; then
  gzip_limit=$(( gzip_base + gzip_base * TOLERANCE_PCT / 100 ))
  gzip_delta=$(( gzip_size - gzip_base ))
  gzip_pct=$(( gzip_delta * 100 / gzip_base ))
  printf 'wasm gzip: %d KB (baseline %d KB, delta %+d KB / %+d%%)\n' \
    "$((gzip_size/1024))" "$((gzip_base/1024))" "$((gzip_delta/1024))" "$gzip_pct"
  [ "$gzip_size" -gt "$gzip_limit" ] && gzip_failed=1
else
  printf 'wasm gzip: %d KB (legacy baseline has no gzip value)\n' "$((gzip_size/1024))"
fi

[ -n "${GITHUB_STEP_SUMMARY:-}" ] && {
  printf '### WASM size\nraw: %d KB (baseline %d KB, %+d%%)\n' \
    "$((raw_size/1024))" "$((raw_base/1024))" "$raw_pct" >> "$GITHUB_STEP_SUMMARY"
  if [ -n "${gzip_base:-}" ]; then
    printf 'gzip: %d KB (baseline %d KB, %+d%%)\n' \
      "$((gzip_size/1024))" "$((gzip_base/1024))" "$gzip_pct" >> "$GITHUB_STEP_SUMMARY"
  else
    printf 'gzip: %d KB (legacy baseline unavailable)\n' "$((gzip_size/1024))" >> "$GITHUB_STEP_SUMMARY"
  fi
}

if [ "$raw_size" -gt "$raw_limit" ] || [ "$gzip_failed" -ne 0 ]; then
  echo "❌ wasm grew more than ${TOLERANCE_PCT}% over baseline."
  echo "   Investigate raw CODE/DATA and gzip regressions."
  echo "   If intentional, run: scripts/check_wasm_size.sh $WASM update"
  exit 1
fi
echo "✅ wasm size within +${TOLERANCE_PCT}% of baseline"
