#!/bin/bash
# Compare two test outputs and show improved/regressed test cases
# Usage: ./scripts/compare_tests.sh <before.txt> <after.txt>
#
# Options:
#   -i, --improved   Show only improved (was failing, now passing)
#   -r, --regressed  Show only regressed (was passing, now failing)
#   -s, --summary    Show only summary counts
#   -c, --cli        Run suzume-cli on changed inputs
#   -m, --mecab      Run MeCab on changed inputs
#   -h, --help       Show this help
#
# Examples:
#   ./scripts/compare_tests.sh /tmp/before.txt /tmp/after.txt
#   ./scripts/compare_tests.sh -i /tmp/before.txt /tmp/after.txt  # improved only
#   ./scripts/compare_tests.sh -r /tmp/before.txt /tmp/after.txt  # regressed only

set -e

SHOW_IMPROVED=1
SHOW_REGRESSED=1
SUMMARY_ONLY=0
RUN_CLI=0
RUN_MECAB=0

while [[ $# -gt 0 ]]; do
    case $1 in
        -i|--improved) SHOW_IMPROVED=1; SHOW_REGRESSED=0; shift ;;
        -r|--regressed) SHOW_IMPROVED=0; SHOW_REGRESSED=1; shift ;;
        -s|--summary) SUMMARY_ONLY=1; shift ;;
        -c|--cli) RUN_CLI=1; shift ;;
        -m|--mecab) RUN_MECAB=1; shift ;;
        -h|--help)
            head -16 "$0" | tail -15
            exit 0
            ;;
        *) break ;;
    esac
done

if [ $# -lt 2 ]; then
    echo "Usage: $0 [options] <before.txt> <after.txt>" >&2
    echo "Run '$0 --help' for options" >&2
    exit 1
fi

BEFORE="$1"
AFTER="$2"

if [ ! -f "$BEFORE" ]; then
    echo "Error: File not found: $BEFORE" >&2
    exit 1
fi

if [ ! -f "$AFTER" ]; then
    echo "Error: File not found: $AFTER" >&2
    exit 1
fi

# Extract failed inputs with test IDs
extract_failures() {
    grep -E "Input:|FAILED.*Tokenize" "$1" 2>/dev/null | \
    awk '
    /Input:/ {
        input = $0
        sub(/.*Input: /, "", input)
    }
    /FAILED.*Tokenize/ {
        if (input != "") {
            id = $0
            gsub(/.*Tokenize\//, "", id)
            gsub(/,.*/, "", id)
            print id "\t" input
            input = ""
        }
    }'
}

# Create temp files for comparison
BEFORE_FAILS=$(mktemp)
AFTER_FAILS=$(mktemp)
trap "rm -f $BEFORE_FAILS $AFTER_FAILS" EXIT

extract_failures "$BEFORE" | sort > "$BEFORE_FAILS"
extract_failures "$AFTER" | sort > "$AFTER_FAILS"

BEFORE_COUNT=$(wc -l < "$BEFORE_FAILS" | tr -d ' ')
AFTER_COUNT=$(wc -l < "$AFTER_FAILS" | tr -d ' ')

# Find improved (in before but not in after)
IMPROVED=$(comm -23 "$BEFORE_FAILS" "$AFTER_FAILS")
IMPROVED_COUNT=$(echo "$IMPROVED" | grep -c . || true)

# Find regressed (in after but not in before)
REGRESSED=$(comm -13 "$BEFORE_FAILS" "$AFTER_FAILS")
REGRESSED_COUNT=$(echo "$REGRESSED" | grep -c . || true)

# Find unchanged failures (in both)
UNCHANGED=$(comm -12 "$BEFORE_FAILS" "$AFTER_FAILS")
UNCHANGED_COUNT=$(echo "$UNCHANGED" | grep -c . || true)

# Summary
echo "=== Test Comparison Summary ===" >&2
echo "Before: $BEFORE_COUNT failures" >&2
echo "After:  $AFTER_COUNT failures" >&2
echo "---" >&2
echo "Improved:  $IMPROVED_COUNT (was failing, now passing)" >&2
echo "Regressed: $REGRESSED_COUNT (was passing, now failing)" >&2
echo "Unchanged: $UNCHANGED_COUNT (still failing)" >&2

DELTA=$((BEFORE_COUNT - AFTER_COUNT))
if [ $DELTA -gt 0 ]; then
    echo "Net change: -$DELTA failures (improvement)" >&2
elif [ $DELTA -lt 0 ]; then
    echo "Net change: +$((-DELTA)) failures (regression)" >&2
else
    echo "Net change: 0 (no change in failure count)" >&2
fi
echo "" >&2

if [ "$SUMMARY_ONLY" = "1" ]; then
    exit 0
fi

process_input() {
    local input="$1"
    local id="$2"

    echo "  [$id] $input"

    if [ "$RUN_CLI" = "1" ] && [ -x "./build/bin/suzume-cli" ]; then
        echo "    suzume: $(./build/bin/suzume-cli "$input" 2>/dev/null | tr '\n' ' ')"
    fi

    if [ "$RUN_MECAB" = "1" ] && command -v mecab &>/dev/null; then
        echo "    MeCab:  $(echo "$input" | mecab | grep -v EOS | cut -f1 | tr '\n' ' ')"
    fi
}

if [ "$SHOW_IMPROVED" = "1" ] && [ "$IMPROVED_COUNT" -gt 0 ]; then
    echo "### IMPROVED ($IMPROVED_COUNT) ###"
    echo "$IMPROVED" | while IFS=$'\t' read -r id input; do
        process_input "$input" "$id"
    done
    echo ""
fi

if [ "$SHOW_REGRESSED" = "1" ] && [ "$REGRESSED_COUNT" -gt 0 ]; then
    echo "### REGRESSED ($REGRESSED_COUNT) ###"
    echo "$REGRESSED" | while IFS=$'\t' read -r id input; do
        process_input "$input" "$id"
    done
    echo ""
fi
