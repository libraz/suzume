#!/usr/bin/env python3
"""Verify closed C++ facts mirrored by the Python test oracle."""

from __future__ import annotations

import ast
import re
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
CONSTANTS = ROOT / "scripts/mcp/src/suzume_mcp/core/constants.py"
MERGE_RULES = ROOT / "scripts/mcp/src/suzume_mcp/core/merge_rules.py"
SUBSIDIARIES = ROOT / "scripts/mcp/src/suzume_mcp/core/postprocessor_subsidiaries.py"
KANA_CONSTANTS = ROOT / "src/core/kana_constants.h"
CHAR_TYPE = ROOT / "src/normalize/char_type.cpp"
AUXILIARIES = ROOT / "src/dictionary/entries/auxiliaries.cpp"


def assignment_value(path: Path, name: str, function: str | None = None) -> Any:
    tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    nodes: list[ast.AST] = list(tree.body)
    if function is not None:
        functions = [node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name == function]
        if len(functions) != 1:
            raise RuntimeError(f"{path}: expected one function named {function}")
        nodes = list(ast.walk(functions[0]))

    for node in nodes:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            if any(isinstance(target, ast.Name) and target.id == name for target in targets):
                return ast.literal_eval(node.value)
    raise RuntimeError(f"{path}: assignment {name} not found")


def core_kanji_ranges() -> tuple[tuple[int, int], ...]:
    source = KANA_CONSTANTS.read_text(encoding="utf-8")
    body = source.split("inline bool isKanjiCodepoint", 1)[1].split("inline bool isOnbinCodepoint", 1)[0]
    ranges = re.findall(
        r"cp\s*>=\s*(0x[0-9A-Fa-f]+)\s*&&\s*cp\s*<=\s*(0x[0-9A-Fa-f]+)",
        body,
    )
    return tuple((int(low, 16), int(high, 16)) for low, high in ranges)


def core_numeric_prefixes() -> set[str]:
    source = CHAR_TYPE.read_text(encoding="utf-8")
    return set(re.findall(r"\{U'([^']+)',\s*kNumericApproxPrefix\}", source))


def core_subsidiary_auxiliaries() -> dict[str, str]:
    source = AUXILIARIES.read_text(encoding="utf-8")
    entries = {
        surface: lemma
        for surface, lemma in re.findall(
            r'aux\("([^"]+)",\s*"([^"]+)",\s*EPOS::AuxInability\)',
            source,
        )
    }
    honorific = re.search(r'aux\("たまえ",\s*"([^"]+)",\s*EPOS::AuxHonorific\)', source)
    if honorific is None:
        raise RuntimeError(f"{AUXILIARIES}: renyokei subsidiary たまえ not found")
    entries["たまえ"] = honorific.group(1)
    return entries


def compare(name: str, core_value: Any, oracle_value: Any) -> bool:
    if core_value == oracle_value:
        return True
    print(f"{name} mirror mismatch")
    print(f"  core:   {core_value!r}")
    print(f"  oracle: {oracle_value!r}")
    return False


def main() -> int:
    checks = (
        (
            "KANJI_RANGES",
            core_kanji_ranges(),
            tuple(assignment_value(CONSTANTS, "KANJI_RANGES")),
        ),
        (
            "_APPROX_NUMERIC_PREFIXES",
            core_numeric_prefixes(),
            set(assignment_value(MERGE_RULES, "_APPROX_NUMERIC_PREFIXES")),
        ),
        (
            "postprocess_closed_subsidiary_aux",
            core_subsidiary_auxiliaries(),
            dict(
                assignment_value(
                    SUBSIDIARIES,
                    "lemmas",
                    function="postprocess_closed_subsidiary_aux",
                )
            ),
        ),
    )
    if not all(compare(*check) for check in checks):
        return 1
    print("oracle mirror check OK: kanji, numeric prefixes, and subsidiary auxiliaries")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
