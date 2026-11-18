#!/usr/bin/env python3
"""Count the code-ratchet metrics for core source files.

Prints one ``file<TAB>surface_cmp<TAB>score_literals<TAB>score_additions`` row
per argument. ``scripts/check_code_guardrails.sh`` owns the baseline and the
comparison; this module owns what each number means:

  surface_cmp      Japanese string literals a string is tested against: an
                   operand of ``==``/``!=``, or an argument of a prefix, suffix,
                   substring or ``compare`` test. ASCII literals (option keys,
                   diagnostic labels) are not words and are not counted.
  score_literals   floating-point literals that reach a shipped build. Comments,
                   string literals, code compiled only under SUZUME_DEBUG_INFO
                   and the arguments of SUZUME_DEBUG_* macros are not counted.
  score_additions  references to a constant of a score namespace in a scorer
                   translation unit, through any alias of that namespace.

Usage:
  scripts/code_guardrail_metrics.py <file>...
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

# Namespaces whose constants are score values. A scorer file reaches them through
# local aliases (`namespace cost = ...::bigram_cost;`), which are resolved per file.
SCORE_NAMESPACES = ("bigram_cost", "scorer")

# Markers left in place of string literals: one holding non-ASCII text, any other.
WORD = '"W"'
OTHER = '"A"'

_LITERAL_EQUALITY = re.compile(rf"[!=]=\s*{WORD}|{WORD}\s*[!=]=")
_LITERAL_TEST_CALL = re.compile(r"\b(?:(?:starts|ends)_?[wW]ith\w*|compare|r?find)\s*\(")
_FLOAT_LITERAL = re.compile(r"\b[0-9]+\.[0-9]+(?:[eE][-+]?[0-9]+)?[fFlL]?\b")
_DEBUG_MACRO_CALL = re.compile(r"\bSUZUME_DEBUG_\w+\s*\(")
_NAMESPACE_ALIAS = re.compile(r"\bnamespace\s+(\w+)\s*=\s*([\w:]+)\s*;")
_CONDITIONAL = re.compile(r"^\s*#\s*(ifdef|ifndef|if|elif|else|endif)\b(.*)$")


def _literal_marker(literal: str) -> str:
    return OTHER if literal.isascii() else WORD


def strip_comments_and_literals(text: str) -> str:
    """Drop comments and reduce each string literal to a marker, keeping line breaks."""
    out: list[str] = []
    index = 0
    size = len(text)
    while index < size:
        if text.startswith("//", index):
            end = text.find("\n", index)
            index = size if end < 0 else end
        elif text.startswith("/*", index):
            end = text.find("*/", index + 2)
            end = size if end < 0 else end + 2
            out.append("\n" * text.count("\n", index, end))
            index = end
        elif text.startswith('R"', index):
            delimiter_end = text.index("(", index)
            closing = ")" + text[index + 2 : delimiter_end] + '"'
            end = text.index(closing, delimiter_end) + len(closing)
            out.append(_literal_marker(text[index:end]) + "\n" * text.count("\n", index, end))
            index = end
        elif text[index] in "\"'":
            quote = text[index]
            end = index + 1
            while end < size and text[end] != quote:
                end += 2 if text[end] == "\\" else 1
            if quote == '"':
                out.append(_literal_marker(text[index:end]))
            else:
                out.append("'C'")
            index = end + 1
        else:
            out.append(text[index])
            index += 1
    return "".join(out)


def call_arguments(text: str, opening: int) -> str:
    """Return the text between the parenthesis at ``opening`` and its match."""
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == "(":
            depth += 1
        elif text[index] == ")":
            depth -= 1
            if depth == 0:
                return text[opening + 1 : index]
    return text[opening + 1 :]


def blank_debug_only_code(text: str) -> str:
    """Remove code that only a SUZUME_DEBUG_INFO build compiles, and debug macro arguments."""
    lines: list[str] = []
    # Per open conditional: (current branch is debug-only, its #else branch is debug-only).
    stack: list[tuple[bool, bool]] = []
    for line in text.split("\n"):
        directive = _CONDITIONAL.match(line)
        if directive is None:
            lines.append("" if any(debug_only for debug_only, _ in stack) else line)
            continue
        keyword, condition = directive.groups()
        names_debug = "SUZUME_DEBUG_INFO" in condition
        negated = keyword == "ifndef" or "!defined" in condition.replace(" ", "")
        if keyword in ("ifdef", "ifndef", "if"):
            stack.append((names_debug and not negated, names_debug and negated))
        elif keyword in ("elif", "else") and stack:
            stack[-1] = (stack[-1][1], False)
        elif keyword == "endif" and stack:
            stack.pop()
        lines.append("")
    blanked = "\n".join(lines)

    out: list[str] = []
    position = 0
    for match in _DEBUG_MACRO_CALL.finditer(blanked):
        if match.start() < position:
            continue
        arguments = call_arguments(blanked, match.end() - 1)
        out.append(blanked[position : match.end()])
        position = match.end() + len(arguments)
    out.append(blanked[position:])
    return "".join(out)


def count_literal_word_tests(code: str) -> int:
    count = len(_LITERAL_EQUALITY.findall(code))
    for match in _LITERAL_TEST_CALL.finditer(code):
        count += call_arguments(code, match.end() - 1).count(WORD)
    return count


def count_score_references(code: str) -> int:
    qualifiers = set(SCORE_NAMESPACES)
    aliases = _NAMESPACE_ALIAS.findall(code)
    # An alias may name another alias, so resolve until the set stops growing.
    grown = True
    while grown:
        grown = False
        for alias, target in aliases:
            if alias not in qualifiers and target.split("::")[-1] in qualifiers:
                qualifiers.add(alias)
                grown = True
    names = "|".join(sorted(map(re.escape, qualifiers)))
    return len(re.findall(rf"(?<![\w:])(?:{names})::(?:\w+::)*k[A-Z]\w*", code))


def metrics(path: str) -> tuple[int, int, int]:
    code = strip_comments_and_literals(Path(path).read_text(encoding="utf-8"))
    word_tests = count_literal_word_tests(code)
    floats = len(_FLOAT_LITERAL.findall(blank_debug_only_code(code)))
    name = Path(path).name
    is_scorer_unit = Path(path).parent.name == "analysis" and name.startswith("scorer") and name.endswith(".cpp")
    additions = count_score_references(code) if is_scorer_unit else 0
    return word_tests, floats, additions


def main(arguments: list[str]) -> int:
    if not arguments:
        print(__doc__, file=sys.stderr)
        return 2
    for path in arguments:
        print(path, *metrics(path), sep="\t")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
