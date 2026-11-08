"""Closed kana number and ordinal readings used by the MeCab normalizer."""

import pytest

from suzume_mcp.core.merge_rules_closed_units import (
    _kana_ordinal_right_boundary,
    _kana_ordinal_surface_at,
    _merge_closed_function_units,
)
from suzume_mcp.core.merge_rules_state import MergeState
from suzume_mcp.core.suzume_utils import get_expected_tokens


def _fused_state(text):
    token = {"surface": text, "pos": "名詞", "lemma": text}
    return MergeState(tokens=[token], text=text, i=0, t=token.copy(), remaining=text)


@pytest.mark.parametrize(
    ("text", "ordinal"),
    [
        ("ねこがいちばんだ", "いちばん"),
        ("いちばんだ", "いちばん"),
        ("にばんだ", "にばん"),
        ("さんばんだ", "さんばん"),
        ("よんばんだ", "よんばん"),
        ("じゅうばんだ", "じゅうばん"),
        ("じゅういちばんだった", "じゅういちばん"),
        ("にじゅういちばんです", "にじゅういちばん"),
        ("きゅうじゅうきゅうばんでした", "きゅうじゅうきゅうばん"),
        ("いちばんだ。", "いちばん"),
        ("いちばんだね。", "いちばん"),
    ],
)
def test_get_expected_tokens_restores_clause_final_kana_ordinal(text, ordinal):
    tokens, _source, _rule = get_expected_tokens(text)
    matches = [token for token in tokens if token["surface"] == ordinal]

    assert len(matches) == 1
    assert matches[0]["pos"] == "Noun"
    assert matches[0]["pos_sub1"] == "数"
    assert matches[0]["lemma"] == ordinal


@pytest.mark.parametrize(
    ("text", "ordinal", "closes_clause"),
    [
        ("じゅうばん", "じゅうばん", True),
        ("いちばんだ", "いちばん", True),
        ("にじゅう", "", False),
        ("いちじゅうばんだ", "", False),
        ("ひとつばんだ", "", False),
        ("ひとりばんだ", "", False),
        ("かんばんだ", "", False),
        ("こんばんは", "", False),
        ("いちばむ", "", False),
        ("にばんだいがある", "にばん", False),
        ("にばんそうこうがある", "にばん", False),
        ("いちばん美しい", "いちばん", False),
        ("よんさんばんだ", "", False),
        ("二番だ", "", False),
        ("二番目だ", "", False),
        ("第二番だ", "", False),
        ("今晩だ", "", False),
        ("読んだ", "", False),
        ("泳いだ", "", False),
    ],
)
def test_kana_ordinal_parser_is_closed_and_boundary_gated(text, ordinal, closes_clause):
    parsed = _kana_ordinal_surface_at(text)

    assert parsed == ordinal
    if ordinal:
        assert _kana_ordinal_right_boundary(text[len(ordinal) :]) is closes_clause


@pytest.mark.parametrize(
    "tail",
    ["", "。", "、", "！", "？", "」", "）", "だ", "だった", "です", "でした", "だよ。", "でしたね。"],
)
def test_kana_ordinal_accepts_only_clause_final_copula_tails(tail):
    assert _kana_ordinal_right_boundary(tail)


@pytest.mark.parametrize("tail", ["美しい", "だいがある", "だと", "だよね", "でしたもの", "ですなもの"])
def test_kana_ordinal_rejects_open_copula_or_attributive_tails(tail):
    assert not _kana_ordinal_right_boundary(tail)


def test_existing_kana_quantity_and_kanji_controls_are_not_rewritten_as_ordinals():
    for text, quantity in (("いちまいだ", "いちまい"), ("よんにんだ", "よんにん"), ("ふた月", "ふた月")):
        tokens, _source, _rule = get_expected_tokens(text)
        matches = [token for token in tokens if token["surface"] == quantity]

        assert len(matches) == 1
        assert matches[0]["pos"] == "Noun"


def test_kana_ordinal_consumes_only_its_exact_source_span():
    ordinal = "にじゅういちばん"
    token_surfaces = ["に", "じゅう", "いち", "ばん", "でした", "ね", "。"]
    tokens = [{"surface": surface, "pos": "助詞", "lemma": surface} for surface in token_surfaces]
    state = MergeState(
        tokens=tokens,
        text="".join(token_surfaces),
        i=0,
        t=tokens[0].copy(),
        remaining="".join(token_surfaces),
    )

    assert _merge_closed_function_units(state)
    assert state.result == [{"surface": ordinal, "pos": "名詞", "pos_sub1": "数", "lemma": ordinal}]
    assert state.i == 4
    assert tokens[state.i]["surface"] == "でした"


@pytest.mark.parametrize(
    ("token_surfaces", "tail_surface", "final_particle"),
    [
        (["きゅうじゅうきゅうばんでした"], "でした", ""),
        (["きゅうじゅうきゅうばんで", "した"], "でした", ""),
        (["きゅうじゅうきゅうばんでしたね"], "でしたね", "ね"),
    ],
)
def test_fused_raw_ordinal_tail_is_reparsed_without_consuming_the_copula(token_surfaces, tail_surface, final_particle):
    ordinal = "きゅうじゅうきゅうばん"
    text = "".join(token_surfaces)
    tokens = [{"surface": surface, "pos": "名詞", "lemma": surface} for surface in token_surfaces]
    state = MergeState(tokens=tokens, text=text, i=0, t=tokens[0].copy(), remaining=text)

    assert _merge_closed_function_units(state)
    assert state.result[0] == {"surface": ordinal, "pos": "名詞", "pos_sub1": "数", "lemma": ordinal}
    reparsed_tail = state.result[1:]
    assert "".join(token["surface"] for token in reparsed_tail) == tail_surface
    if final_particle:
        assert reparsed_tail[-1]["surface"] == final_particle
        assert reparsed_tail[-1]["pos"] == "助詞"
        assert reparsed_tail[-1]["pos_sub1"] == "終助詞"
        reparsed_auxiliaries = reparsed_tail[:-1]
    else:
        reparsed_auxiliaries = reparsed_tail
    assert reparsed_auxiliaries
    assert all(token["pos"] == "助動詞" for token in reparsed_auxiliaries)
    assert state.i == len(tokens)


@pytest.mark.parametrize(
    "text",
    [
        "いちじゅうばんだ",
        "いちまいだ",
        "よんにんだ",
        "ふた月",
        "二番だ",
        "二番目だ",
        "第二番だ",
        "今晩だ",
        "読んだ",
        "泳いだ",
        "いちばん美しい",
        "にばんだいがある",
        "にばんそうこうがある",
    ],
)
def test_fused_raw_controls_do_not_create_kana_ordinal_nouns(text):
    state = _fused_state(text)

    _merge_closed_function_units(state)

    assert not any(
        token.get("pos_sub1") == "数" and token.get("surface", "").endswith("ばん") for token in state.result
    )
