#!/usr/bin/env bash
#
# Mechanical guardrails for the design principles in CLAUDE.md that were previously
# only enforced by convention. Ratchet-based: per core source file it counts
#   - surface_cmp:    Japanese string literals a string is tested against (hardcoded word tests)
#   - score_literals: float literals in shipped code (magic numbers not via named constants)
#   - score_additions: score-namespace constants referenced in scorer translation units
# and fails if any metric EXCEEDS the committed baseline. Metrics may only go down;
# an intentional reduction is recorded by re-running with `update`, which shows up as
# a baseline diff in the commit.
#
# Also verifies WASM-core purity: no
# <fstream>/<thread>/<filesystem>/<mutex>/<iostream> in core layers outside an
# actual __EMSCRIPTEN__-excluded preprocessor branch or the explicit allowlist.
#
# Portable to bash 3.2 (macOS) and bash 4+ (CI): no associative arrays, greps guarded.
#
# Usage:
#   scripts/check_code_guardrails.sh          # check (CI)
#   scripts/check_code_guardrails.sh update   # regenerate baseline after an intentional drop

set -eu
export LC_ALL=C
cd "$(git rev-parse --show-toplevel)"

BASELINE="scripts/guardrail-baseline.tsv"
MODE="${1:-check}"

# scripts/code_guardrail_metrics.py owns what each metric counts.
METRICS="scripts/code_guardrail_metrics.py"

# Analysis, grammar, and postprocessing carry candidate/scoring decisions across
# both translation units and headers. Core headers are included because a scoring
# literal parked in lattice/Viterbi would otherwise sit outside the ratchet.
FILES=$(
  {
    find src/analysis src/grammar src/postprocess -type f \( -name '*.cpp' -o -name '*.h' \)
    find src/core -type f -name '*.h'
  } | sort
)

gen_baseline() {
  printf '# file\tsurface_cmp\tscore_literals\tscore_additions\n'
  # shellcheck disable=SC2086
  python3 "$METRICS" $FILES
}

# WASM-core purity: native file-I/O, thread, and stream headers must never reach
# the WASM build. Track preprocessor branches so an unrelated __EMSCRIPTEN__
# mention elsewhere in the file cannot hide an unguarded include.
PURITY_DIRS="src/core src/normalize src/analysis src/dictionary src/postprocess src/grammar src/pretokenizer"
# Native-only file APIs must be wrapped in an __EMSCRIPTEN__ guard. The user
# dictionary has a separate native file loader until it receives the same split.
PURITY_ALLOW="src/dictionary/user_dict.cpp"

unguarded_purity_includes() {
  awk '
    function branch_state(line, expression) {
      if (line ~ /^[[:space:]]*#[[:space:]]*ifndef[[:space:]]+__EMSCRIPTEN__([[:space:]]|$)/) {
        return -1
      }
      if (line ~ /^[[:space:]]*#[[:space:]]*ifdef[[:space:]]+__EMSCRIPTEN__([[:space:]]|$)/) {
        return 1
      }

      expression = line
      sub(/^[[:space:]]*#[[:space:]]*(if|elif)[[:space:]]+/, "", expression)
      sub(/[[:space:]]*(\/\/|\/\*).*/, "", expression)
      gsub(/[[:space:]]/, "", expression)

      if (expression ~ /^!defined\(?__EMSCRIPTEN__\)?$/ ||
          expression ~ /^!__EMSCRIPTEN__$/) {
        return -1
      }
      if (expression ~ /^defined\(?__EMSCRIPTEN__\)?$/ ||
          expression ~ /^__EMSCRIPTEN__$/) {
        return 1
      }
      # A false term in an AND expression makes the whole branch native-only.
      if (expression ~ /!defined\(?__EMSCRIPTEN__\)?/ &&
          expression !~ /\|\|/) {
        return -1
      }
      # A true term in an OR expression makes the branch active on WASM.
      if (expression ~ /(^|\|\|)defined\(?__EMSCRIPTEN__\)?(\|\||$)/ &&
          expression !~ /&&/) {
        return 1
      }
      return 0
    }

    /^[[:space:]]*#[[:space:]]*(if|ifdef|ifndef)([[:space:]]|$)/ {
      depth++
      state[depth] = branch_state($0)
      seen_true[depth] = state[depth] == 1
      seen_unknown[depth] = state[depth] == 0
      next
    }
    /^[[:space:]]*#[[:space:]]*elif([[:space:]]|$)/ {
      if (depth > 0) {
        if (seen_true[depth]) {
          state[depth] = -1
        } else if (seen_unknown[depth]) {
          state[depth] = 0
        } else {
          state[depth] = branch_state($0)
          seen_true[depth] = state[depth] == 1
          seen_unknown[depth] = state[depth] == 0
        }
      }
      next
    }
    /^[[:space:]]*#[[:space:]]*else([[:space:]]|$)/ {
      if (depth > 0) {
        if (seen_true[depth]) {
          state[depth] = -1
        } else if (seen_unknown[depth]) {
          state[depth] = 0
        } else {
          state[depth] = 1
        }
      }
      next
    }
    /^[[:space:]]*#[[:space:]]*endif([[:space:]]|$)/ {
      if (depth > 0) {
        delete state[depth]
        delete seen_true[depth]
        delete seen_unknown[depth]
        depth--
      }
      next
    }
    /^[[:space:]]*#[[:space:]]*include[[:space:]]*<(fstream|thread|filesystem|mutex|iostream)>/ {
      guarded = 0
      for (level = 1; level <= depth; level++) {
        if (state[level] == -1) {
          guarded = 1
        }
      }
      if (!guarded) {
        print FNR ":" $0
      }
    }
  ' "$1"
}

check_purity() {
  local bad=0 hit unguarded
  for hit in $(grep -rlE '#[[:space:]]*include[[:space:]]*<(fstream|thread|filesystem|mutex|iostream)>' \
    $PURITY_DIRS 2>/dev/null || true); do
    case " $PURITY_ALLOW " in *" $hit "*) continue ;; esac
    unguarded=$(unguarded_purity_includes "$hit")
    if [ -n "$unguarded" ]; then
      while IFS= read -r violation; do
        echo "❌ WASM-core purity: $hit:$violation is not excluded from the Emscripten preprocessor branch"
      done <<EOF
$unguarded
EOF
      bad=1
    fi
  done
  return $bad
}

if [ "$MODE" = "update" ]; then
  gen_baseline > "$BASELINE"
  echo "✅ baseline written to $BASELINE"
  exit 0
fi

[ -f "$BASELINE" ] || { echo "❌ missing $BASELINE (run: scripts/check_code_guardrails.sh update)"; exit 1; }

fail=0
current=$(gen_baseline)
for f in $FILES; do
  cur=$(printf '%s\n' "$current" | grep -F "$(printf '%s\t' "$f")" || true)
  s=$(echo "$cur" | cut -f2); fl=$(echo "$cur" | cut -f3); add=$(echo "$cur" | cut -f4)
  base=$(grep -F "$(printf '%s\t' "$f")" "$BASELINE" || true)
  if [ -z "$base" ]; then
    echo "❌ new scanned file not in baseline: $f (run: update)"; fail=1; continue
  fi
  bs=$(echo "$base" | cut -f2); bf=$(echo "$base" | cut -f3); ba=$(echo "$base" | cut -f4)
  [ "$s"  -gt "$bs" ] && { echo "❌ $f literal word tests $s > baseline $bs (generalize with grammar rules; don't add word tests)"; fail=1; } || true
  [ "$fl" -gt "$bf" ] && { echo "❌ $f float literals $fl > baseline $bf (use named constants in *_constants.h)"; fail=1; } || true
  [ "$add" -gt "$ba" ] && { echo "❌ $f scorer rules $add > baseline $ba (reuse a semantic group or predicate)"; fail=1; } || true
done

check_purity || fail=1

if [ "$fail" = 0 ]; then
  echo "✅ code guardrails OK (ratchet + WASM-core purity)"
else
  echo ""
  echo "A rise means a design-principle regression. Fix it, or after an intentional REDUCTION run:"
  echo "  scripts/check_code_guardrails.sh update"
fi
exit $fail
