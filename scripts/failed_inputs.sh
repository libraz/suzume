#!/bin/bash
# Extract failed test inputs from ctest output
# Usage: ./scripts/failed_inputs.sh [options] [test_output_file]
#
# Options:
#   -v, --verbose    Show test case ID with input
#   -c, --cli        Run suzume-cli on each failed input
#   -m, --mecab      Run MeCab on each failed input
#   -a, --all        Show verbose + cli + mecab output
#   -h, --help       Show this help
#
# Examples:
#   ./scripts/failed_inputs.sh                    # List failed inputs
#   ./scripts/failed_inputs.sh -v                 # List with test IDs
#   ./scripts/failed_inputs.sh -c                 # Run suzume-cli on failures
#   ./scripts/failed_inputs.sh -m                 # Run MeCab on failures
#   ./scripts/failed_inputs.sh /tmp/other.txt    # Use different file

set -e

VERBOSE=0
RUN_CLI=0
RUN_MECAB=0
INPUT_FILE="/tmp/test.txt"

while [[ $# -gt 0 ]]; do
    case $1 in
        -v|--verbose) VERBOSE=1; shift ;;
        -c|--cli) RUN_CLI=1; shift ;;
        -m|--mecab) RUN_MECAB=1; shift ;;
        -a|--all) VERBOSE=1; RUN_CLI=1; RUN_MECAB=1; shift ;;
        -h|--help)
            head -17 "$0" | tail -16
            exit 0
            ;;
        -) INPUT_FILE="-"; shift ;;
        *) INPUT_FILE="$1"; shift ;;
    esac
done

extract_failures() {
    grep -E "Input:|FAILED.*Tokenize" | \
    awk -v verbose="$VERBOSE" '
    /Input:/ {
        input = $0
        sub(/.*Input: /, "", input)
    }
    /FAILED.*Tokenize/ {
        if (input != "") {
            if (verbose) {
                # Extract test ID from FAILED line
                id = $0
                gsub(/.*Tokenize\//, "", id)
                gsub(/,.*/, "", id)
                print id "\t" input
            } else {
                print input
            }
            input = ""
        }
    }'
}

process_input() {
    local input="$1"
    local id="$2"

    if [ "$VERBOSE" = "1" ] && [ -n "$id" ]; then
        echo "=== $id ==="
    fi
    echo "Input: $input"

    if [ "$RUN_CLI" = "1" ] && [ -x "./build/bin/suzume-cli" ]; then
        echo "--- suzume ---"
        ./build/bin/suzume-cli "$input" 2>/dev/null || true
    fi

    if [ "$RUN_MECAB" = "1" ] && command -v mecab &>/dev/null; then
        echo "--- MeCab ---"
        echo "$input" | mecab
    fi

    if [ "$RUN_CLI" = "1" ] || [ "$RUN_MECAB" = "1" ]; then
        echo ""
    fi
}

if [ "$INPUT_FILE" = "-" ]; then
    CONTENT=$(cat)
else
    if [ ! -f "$INPUT_FILE" ]; then
        echo "Error: File not found: $INPUT_FILE" >&2
        echo "Run: ctest --test-dir build --output-on-failure > /tmp/test.txt 2>&1" >&2
        exit 1
    fi
    CONTENT=$(cat "$INPUT_FILE")
fi

FAILURES=$(echo "$CONTENT" | extract_failures)
COUNT=$(echo "$FAILURES" | grep -c . || true)

if [ "$COUNT" = "0" ]; then
    echo "No tokenization test failures found."
    exit 0
fi

echo "# $COUNT failed tokenization tests" >&2

if [ "$RUN_CLI" = "1" ] || [ "$RUN_MECAB" = "1" ]; then
    # Process each failure with cli/mecab output
    echo "$FAILURES" | while IFS=$'\t' read -r first second; do
        if [ "$VERBOSE" = "1" ]; then
            process_input "$second" "$first"
        else
            process_input "$first" ""
        fi
    done
else
    # Simple list mode
    echo "$FAILURES"
fi
