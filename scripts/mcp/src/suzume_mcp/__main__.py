"""Standalone CLI for suzume normalization.

Called via subprocess by MCP tools so that code changes are reflected
without MCP server restart. Each invocation starts a fresh Python process,
importing the latest normalization code.

Usage:
    # Single text
    python -m suzume_mcp normalize "食べている"

    # Batch (JSON array on stdin)
    echo '["食べている","走った"]' | python -m suzume_mcp normalize --batch
"""

import json
import sys


def cmd_normalize(args: list[str]) -> None:
    """Run get_expected_tokens and output JSON."""
    # Lazy import: ensures fresh module load each process invocation
    from .core.suzume_utils import format_expected, get_expected_tokens

    batch = "--batch" in args
    texts: list[str] = []

    if batch:
        data = json.loads(sys.stdin.read())
        if isinstance(data, list):
            texts = data
        else:
            print(json.dumps({"error": "Expected JSON array on stdin"}), file=sys.stderr)
            sys.exit(1)
    else:
        non_flag = [a for a in args if not a.startswith("-")]
        if not non_flag:
            print(json.dumps({"error": "No text argument provided"}), file=sys.stderr)
            sys.exit(1)
        texts = non_flag

    results = []
    for text in texts:
        tokens, source, rule = get_expected_tokens(text)
        formatted = format_expected(tokens)
        results.append({"tokens": formatted, "source": source, "rule": rule})

    if batch or len(texts) > 1:
        print(json.dumps(results, ensure_ascii=False))
    else:
        print(json.dumps(results[0], ensure_ascii=False))


def main() -> None:
    args = sys.argv[1:]
    if not args:
        print("Usage: python -m suzume_mcp normalize <text>", file=sys.stderr)
        print("       python -m suzume_mcp normalize --batch  (JSON array on stdin)", file=sys.stderr)
        sys.exit(1)

    subcmd = args[0]
    if subcmd == "normalize":
        cmd_normalize(args[1:])
    else:
        print(f"Unknown command: {subcmd}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
