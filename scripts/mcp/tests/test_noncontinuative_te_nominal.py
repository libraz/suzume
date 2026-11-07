"""Nominal boundaries proved by an impossible continuative and a copula."""

import pytest

from suzume_mcp.core.suzume_utils import get_expected_tokens


@pytest.mark.parametrize(
    "text,surface",
    [
        ("ねこがにがてだ", "にがて"),
        ("ねこがにがてだった", "にがて"),
        ("これはにがてです", "にがて"),
        ("これはにがてでした", "にがて"),
        ("泳ぐのがにがてだ", "にがて"),
        ("かたてだ", "かたて"),
        ("あらてだ", "あらて"),
    ],
)
def test_finite_copula_selects_nominal(text, surface):
    tokens, _, _ = get_expected_tokens(text)
    matching = [token for token in tokens if token["surface"] == surface]
    assert len(matching) == 1
    assert matching[0]["pos"] == "Noun"
    assert matching[0]["lemma"] == surface


@pytest.mark.parametrize("text", ["苦手だ", "にがてな人"])
def test_lexical_or_attributive_adjectival_stem_is_preserved(text):
    tokens, _, _ = get_expected_tokens(text)
    assert tokens[0]["pos"] == "Adjective"
    assert tokens[0]["surface"] == ("苦手" if text == "苦手だ" else "にがて")


@pytest.mark.parametrize("text", ["読んだ", "泳いだ", "食べてだ", "つなぎてだ", "よくてだ", "こうてだ"])
def test_productive_inflections_do_not_become_nominals(text):
    tokens, _, _ = get_expected_tokens(text)
    assert not (tokens[0]["pos"] == "Noun" and tokens[0]["surface"] == text[:-1])


@pytest.mark.parametrize(
    "copula,following", [("なの", None), ("なん", None), ("なら", None), ("な", "の"), ("な", "ん")]
)
def test_explanatory_and_conditional_copulas_do_not_supply_nominal_proof(copula, following):
    from copy import deepcopy

    from suzume_mcp.core.postprocessor_nominals import postprocess_noncontinuative_te_nominal

    tokens = [
        {"surface": "にが", "pos": "Verb", "lemma": "にぐ"},
        {"surface": "て", "pos": "Particle", "lemma": "て"},
        {"surface": copula, "pos": "Auxiliary", "lemma": "だ"},
    ]
    if following:
        tokens.append({"surface": following, "pos": "Noun", "lemma": following})
    before = deepcopy(tokens)
    assert not postprocess_noncontinuative_te_nominal(tokens)
    assert tokens == before
