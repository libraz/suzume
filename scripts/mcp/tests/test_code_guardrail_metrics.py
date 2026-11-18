"""Tests for the code-ratchet counters in scripts/code_guardrail_metrics.py."""

from __future__ import annotations

import importlib.util
from pathlib import Path
from types import ModuleType

import pytest


@pytest.fixture(scope="module")
def metrics_module() -> ModuleType:
    path = Path(__file__).resolve().parents[2] / "code_guardrail_metrics.py"
    spec = importlib.util.spec_from_file_location("code_guardrail_metrics", path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _measure(module: ModuleType, tmp_path: Path, source: str, name: str = "rules.cpp") -> tuple[int, int, int]:
    directory = tmp_path / "analysis"
    directory.mkdir(exist_ok=True)
    path = directory / name
    path.write_text(source, encoding="utf-8")
    return module.metrics(str(path))


_LITERALS = {"japanese": '"ない"', "ascii": '"binary_dict"', "empty": '""'}

_FORMS = {
    "eq_right": "bool hit = surface == {lit};",
    "eq_left": "bool hit = {lit} == prev.surface;",
    "not_equal": "bool hit = lemma != {lit};",
    "ends_with": "bool hit = utf8::endsWith(edge.surface, {lit});",
    "starts_with_any": "bool hit = startsWithAny(text, {{{lit}}});",
    "compare": "bool hit = surface.compare(0, 3, {lit}) == 0;",
    "find": "bool hit = surface.find({lit}) != std::string::npos;",
}

_CONTEXTS = {
    "code": "{stmt}\n",
    "line_comment": "// {stmt}\n",
    "block_comment": "/* note\n{stmt}\n*/\n",
    "debug_guard": "#ifdef SUZUME_DEBUG_INFO\n{stmt}\n#endif\n",
}

# Pairwise covering array over (form, literal, context), plus every form once in
# the configuration that must count.
_WORD_TEST_CASES = [
    ("find", "empty", "line_comment"),
    ("not_equal", "empty", "block_comment"),
    ("ends_with", "ascii", "code"),
    ("starts_with_any", "japanese", "line_comment"),
    ("eq_right", "ascii", "debug_guard"),
    ("eq_left", "empty", "code"),
    ("eq_left", "japanese", "debug_guard"),
    ("compare", "ascii", "block_comment"),
    ("ends_with", "japanese", "block_comment"),
    ("starts_with_any", "ascii", "code"),
    ("starts_with_any", "empty", "block_comment"),
    ("ends_with", "empty", "debug_guard"),
    ("find", "japanese", "code"),
    ("eq_right", "empty", "block_comment"),
    ("not_equal", "ascii", "line_comment"),
    ("eq_left", "ascii", "line_comment"),
    ("not_equal", "japanese", "debug_guard"),
    ("ends_with", "ascii", "line_comment"),
    ("compare", "empty", "line_comment"),
    ("find", "ascii", "block_comment"),
    ("not_equal", "ascii", "code"),
    ("compare", "japanese", "debug_guard"),
    ("starts_with_any", "empty", "debug_guard"),
    ("find", "ascii", "debug_guard"),
    ("compare", "empty", "code"),
    ("eq_left", "empty", "block_comment"),
    ("eq_right", "ascii", "code"),
    ("eq_right", "japanese", "line_comment"),
    *((form, "japanese", "code") for form in _FORMS if form != "find"),
]


@pytest.mark.parametrize(("form", "literal", "context"), _WORD_TEST_CASES)
def test_word_tests_count_japanese_literals_in_compiled_code(
    metrics_module: ModuleType, tmp_path: Path, form: str, literal: str, context: str
) -> None:
    statement = _FORMS[form].format(lit=_LITERALS[literal])
    source = _CONTEXTS[context].format(stmt=statement)
    expected = 1 if literal == "japanese" and context in ("code", "debug_guard") else 0
    assert _measure(metrics_module, tmp_path, source)[0] == expected


def test_word_tests_count_each_literal_of_a_list(metrics_module: ModuleType, tmp_path: Path) -> None:
    source = 'bool hit = utf8::endsWithAny(surface, {"ない", "たい", "x"}) || surface == other.surface;\n'
    assert _measure(metrics_module, tmp_path, source)[0] == 2


def test_word_tests_ignore_literals_that_are_not_compared(metrics_module: ModuleType, tmp_path: Path) -> None:
    source = 'candidate.lemma = "する";\nSUZUME_DEBUG_LOG("surface == " << surface);\n'
    assert _measure(metrics_module, tmp_path, source)[0] == 0


def test_float_literals_count_only_shipped_code(metrics_module: ModuleType, tmp_path: Path) -> None:
    source = (
        "// scale 0.25F per step\n"
        "constexpr float kStep = 0.5F;\n"
        'const char* version = "1.25";\n'
        "#ifdef SUZUME_DEBUG_INFO\n"
        "cand.confidence = 0.7F;\n"
        "#else\n"
        "float fallback = 1.5F;\n"
        "#endif\n"
        "#ifndef SUZUME_DEBUG_INFO\n"
        "float shipped = 2.5F;\n"
        "#else\n"
        "float traced = 3.5F;\n"
        "#endif\n"
        'SUZUME_DEBUG_CANDIDATE(cand, 0.6F, "pattern");\n'
        "float after = cost + 4.5F;\n"
    )
    assert _measure(metrics_module, tmp_path, source)[1] == 4


def test_score_references_follow_namespace_aliases(metrics_module: ModuleType, tmp_path: Path) -> None:
    source = (
        "namespace cost = suzume::analysis::bigram_cost;\n"
        "namespace sc = suzume::analysis::scorer;\n"
        "namespace scale = cost;\n"
        "float total = cost::kStrong + sc::kBosBonus + scale::kWeak + sc::scale::kAlmostNever\n"
        "    + scorer::kDirect + bigram_cost::kDirect;  // cost::kInComment\n"
        "size_t bytes = core::kJapaneseCharBytes + hiragana::kTo.size();\n"
        "auto flag = core::LatticeEdge::kFromDictionary;\n"
    )
    assert _measure(metrics_module, tmp_path, source, "scorer_rules.cpp")[2] == 6


def test_score_references_are_counted_in_scorer_units_only(metrics_module: ModuleType, tmp_path: Path) -> None:
    source = "namespace cost = suzume::analysis::bigram_cost;\nfloat total = cost::kStrong;\n"
    assert _measure(metrics_module, tmp_path, source, "verb_candidates.cpp")[2] == 0
    assert _measure(metrics_module, tmp_path, source, "scorer.h")[2] == 0
