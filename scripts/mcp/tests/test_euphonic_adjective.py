"""Real dictionary evidence for the kana adjective continuative repair."""

from copy import deepcopy

import pytest

from suzume_mcp.core.mecab import mecab_analyze
from suzume_mcp.core.postprocessor_mecab_contractions import (
    repair_euphonic_adjective_adverb,
)


@pytest.mark.parametrize(
    "text",
    ["ようできました", "ようできた", "ようできます", "ようなりました", "よう出来ました", "よう見える"],
)
def test_general_adverb_before_verb_is_registered_adjective_continuative(text):
    tokens = mecab_analyze(text)
    before = deepcopy(tokens)
    assert tokens[0]["pos"] == "副詞"
    assert tokens[0]["pos_sub1"] == "一般"
    tokens[0]["pronunciation"] = "ヨー"
    repair_euphonic_adjective_adverb(tokens)
    assert tokens[0]["surface"] == "よう"
    assert tokens[0]["reading"] == before[0]["reading"]
    assert tokens[0]["pronunciation"] == "ヨー"
    assert tokens[0]["pos"] == "形容詞"
    assert tokens[0]["lemma"] == "よい"
    assert tokens[0]["conj_type"] == mecab_analyze("よい")[0]["conj_type"]
    assert tokens[0]["conj_form"] == "連用ゴザイ接続"
    assert tokens[1:] == before[1:]


@pytest.mark.parametrize(
    "text",
    [
        "ようだ",
        "ようです",
        "そのようにできました",
        "できるようになった",
        "こうできました",
        "そう見える",
        "どう見える",
        "とうとうなりました",
        "ようやった",
        "ことなりました",
    ],
)
def test_preserves_non_adjectival_readings(text):
    tokens = mecab_analyze(text)
    before = deepcopy(tokens)
    repair_euphonic_adjective_adverb(tokens)
    assert tokens == before


@pytest.mark.parametrize("text", ["すごう見える", "良うできました", "よろしゅうございます", "早う起きた"])
def test_preserves_existing_euphonic_adjective_repair(text):
    tokens = mecab_analyze(text)
    surface = tokens[0]["surface"]
    repair_euphonic_adjective_adverb(tokens)
    assert tokens[0]["surface"] == surface
    assert tokens[0]["pos"] == "形容詞"
    assert tokens[0]["conj_form"] == "連用ゴザイ接続"
