"""The formal noun わり + に after a modifier versus the degree adverb わりに."""

import pytest

from suzume_mcp.core.suzume_utils import get_expected_tokens


def _surfaces(text: str) -> list[str]:
    tokens, _, _ = get_expected_tokens(text)
    return [token["surface"] for token in tokens]


@pytest.mark.parametrize(
    "text",
    [
        "読むわりに速い",
        "高いわりにまずい",
        "食べたわりに痩せている",
        "静かなわりに人が多い",
        "年のわりに若い",
        "年齢のわりに若い",
        "本のわりに進む",
    ],
)
def test_wari_after_a_modifier_is_a_formal_noun_with_ni(text: str) -> None:
    tokens, _, _ = get_expected_tokens(text)
    index = next(idx for idx, token in enumerate(tokens) if token["surface"] == "わり")
    assert (tokens[index]["pos"], tokens[index]["lemma"]) == ("Noun", "わり")
    assert (tokens[index + 1]["surface"], tokens[index + 1]["pos"]) == ("に", "Particle")
    assert "わりに" not in _surfaces(text)


@pytest.mark.parametrize("text", ["わりに速い", "今日はわりに暖かい"])
def test_wari_ni_without_a_modifier_stays_the_degree_adverb(text: str) -> None:
    tokens, _, _ = get_expected_tokens(text)
    adverb = next(token for token in tokens if token["surface"] == "わりに")
    assert adverb["pos"] == "Adverb"


@pytest.mark.parametrize(
    "text,expected",
    [
        ("割に合わない", ["割", "に", "合わ", "ない"]),
        ("値段の割に美味しい", ["値段", "の", "割", "に", "美味しい"]),
    ],
)
def test_kanji_wari_is_untouched(text: str, expected: list[str]) -> None:
    assert _surfaces(text) == expected


def test_wari_to_is_untouched_by_the_ni_rule() -> None:
    before = _surfaces("わりと速い")
    assert "わりに" not in before
    assert before[-1] == "速い"
