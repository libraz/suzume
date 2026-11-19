"""Standalone CLI for suzume normalization.

Called via subprocess by MCP tools so that code changes are reflected
without MCP server restart. Each invocation starts a fresh Python process,
importing the latest normalization code.

Usage:
    # Single text
    python -m suzume_mcp normalize "食べている"

    # Batch (JSON array on stdin)
    echo '["食べている","走った"]' | python -m suzume_mcp normalize --batch

    # Normalization rule name per text, and Suzume POS per raw MeCab token
    echo '["経済成長"]' | python -m suzume_mcp rule
    echo '[{"pos":"名詞","pos_sub1":"一般"}]' | python -m suzume_mcp map-pos

    # Native suzume-cli call (JSON request object on stdin; see core/native_cli.py)
    echo '{"op":"analyze","cli":"...","texts":["東京"],...}' | python -m suzume_mcp native
"""

import json
import sys


def cmd_normalize(args: list[str]) -> None:
    """Run get_expected_tokens and output JSON."""
    # Lazy import: ensures fresh module load each process invocation
    from .core.suzume_utils import format_expected, get_expected_tokens, get_mecab_tokens

    batch = "--batch" in args
    raw_mecab = "--mecab" in args

    if batch:
        texts = _read_json_array()
    else:
        non_flag = [a for a in args if not a.startswith("-")]
        if not non_flag:
            print(json.dumps({"error": "No text argument provided"}), file=sys.stderr)
            sys.exit(1)
        texts = non_flag

    results = []
    for text in texts:
        try:
            if not isinstance(text, str):
                raise TypeError(f"Expected text string, got {type(text).__name__}")
            if raw_mecab:
                tokens = get_mecab_tokens(text)
                _oracle_tokens, _oracle_source, rule = get_expected_tokens(text)
                source = "MeCab"
            else:
                tokens, source, rule = get_expected_tokens(text)
            formatted = format_expected(tokens)
            results.append({"tokens": formatted, "source": source, "rule": rule})
        except Exception as exc:
            results.append({"error": str(exc)})

    if batch or len(texts) > 1:
        print(json.dumps(results, ensure_ascii=False))
    else:
        print(json.dumps(results[0], ensure_ascii=False))


def _read_json_array() -> list:
    data = json.loads(sys.stdin.read())
    if not isinstance(data, list):
        print(json.dumps({"error": "Expected JSON array on stdin"}), file=sys.stderr)
        sys.exit(1)
    return data


def cmd_rule(_args: list[str]) -> None:
    """Print the normalization rule name for each text of a JSON array on stdin."""
    from .core.suzume_utils import get_suzume_rule

    print(json.dumps([get_suzume_rule(text) for text in _read_json_array()], ensure_ascii=False))


def cmd_map_pos(_args: list[str]) -> None:
    """Print the Suzume POS for each raw MeCab token of a JSON array on stdin."""
    from .core.pos_mapping import map_mecab_pos

    print(json.dumps([map_mecab_pos(token) for token in _read_json_array()], ensure_ascii=False))


def cmd_native(_args: list[str]) -> None:
    """Run one native suzume-cli request (a JSON object on stdin) and print its JSON result."""
    from .core.native_cli import run

    print(json.dumps(run(json.loads(sys.stdin.read())), ensure_ascii=False))


_COMMANDS = {"normalize": cmd_normalize, "rule": cmd_rule, "map-pos": cmd_map_pos, "native": cmd_native}


def main() -> None:
    args = sys.argv[1:]
    if not args:
        print("Usage: python -m suzume_mcp normalize <text>", file=sys.stderr)
        print("       python -m suzume_mcp normalize --batch  (JSON array on stdin)", file=sys.stderr)
        print("       python -m suzume_mcp rule | map-pos     (JSON array on stdin)", file=sys.stderr)
        print("       python -m suzume_mcp native             (JSON request object on stdin)", file=sys.stderr)
        sys.exit(1)

    command = _COMMANDS.get(args[0])
    if command is not None:
        command(args[1:])
    else:
        print(f"Unknown command: {args[0]}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
