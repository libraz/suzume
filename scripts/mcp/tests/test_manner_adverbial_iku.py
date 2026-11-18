"""Kanji 行く after a demonstrative manner adverbial is the predicate."""

import pytest

from suzume_mcp.core.suzume_utils import get_expected_tokens


def _pos_of(text: str, surface: str) -> str:
    tokens, _, _ = get_expected_tokens(text)
    return next(token["pos"] for token in tokens if token["surface"] == surface)


@pytest.mark.parametrize(
    "text,surface",
    [
        ("駅までどうやって行きますか", "行き"),
        ("こうやって行きます", "行き"),
        ("そうやって行った", "行っ"),
        ("こうして行きます", "行き"),
        ("こうやって行ってみる", "行っ"),
    ],
)
def test_kanji_motion_verb_after_manner_adverbial_is_a_verb(text: str, surface: str) -> None:
    assert _pos_of(text, surface) == "Verb"


@pytest.mark.parametrize(
    "text,surface",
    [
        ("これからどうやっていくか", "いく"),
        ("歩いて行きます", "行き"),
        ("急いで行きます", "行き"),
    ],
)
def test_kana_iku_and_other_te_hosts_stay_the_aspect_auxiliary(text: str, surface: str) -> None:
    assert _pos_of(text, surface) == "Auxiliary"
