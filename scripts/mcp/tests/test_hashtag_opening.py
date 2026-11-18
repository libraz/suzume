"""Where a # opens a hashtag unit in the expected tokens."""

import pytest

from suzume_mcp.core.suzume_utils import get_expected_tokens


def _surfaces(text: str) -> list[str]:
    tokens, _, _ = get_expected_tokens(text)
    return [token["surface"] for token in tokens]


@pytest.mark.parametrize(
    "text,expected",
    [
        ("#東京", ["#東京"]),
        ("#経済成長 の話", ["#経済成長", "の", "話"]),
        ("東京 #テスト", ["東京", "#テスト"]),
        ("#東京#テスト", ["#東京", "#テスト"]),
        ("今日は、#東京#テスト に行く", ["今日", "は", "#東京", "#テスト", "に", "行く"]),
    ],
)
def test_marker_opens_a_tag_at_a_boundary_or_after_a_tag(text: str, expected: list[str]) -> None:
    assert _surfaces(text) == expected


@pytest.mark.parametrize(
    "text,expected",
    [
        ("C#が好き", ["C", "が", "好き"]),
        ("今日、C#が好き", ["今日", "C", "が", "好き"]),
        ("今日、F#とC#を書く", ["今日", "F", "と", "C", "を", "書く"]),
        ("色は#fffです", ["色", "は", "fff", "です"]),
    ],
)
def test_marker_after_a_word_character_opens_no_tag(text: str, expected: list[str]) -> None:
    assert _surfaces(text) == expected
