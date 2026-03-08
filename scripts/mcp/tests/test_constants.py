"""Tests for constants integrity."""

from suzume_mcp.core.constants import (
    COMPOUND_VERB_V2_GODAN,
    COMPOUND_VERB_V2_ICHIDAN,
    FAMILY_TERMS,
    HONORIFIC_EXCEPTIONS,
    NAI_ADJECTIVES,
    PARTICLE_CORRECTIONS,
    POS_MAP,
    POS_NORM_MAP,
    PREFIX_EXCEPTIONS,
    SLANG_ADJ_STEMS,
    SLANG_VERB_STEMS,
    TARI_ADVERB_STEMS,
    VALID_POS,
    WORD_EXCEPTIONS,
)


def test_pos_map_has_all_keys():
    """POS_MAP should have all standard MeCab POS categories."""
    expected_keys = {
        "名詞",
        "動詞",
        "形容詞",
        "副詞",
        "助詞",
        "助動詞",
        "接続詞",
        "感動詞",
        "連体詞",
        "接頭詞",
        "接尾辞",
        "代名詞",
        "記号",
        "フィラー",
        "その他",
    }
    assert set(POS_MAP.keys()) == expected_keys


def test_pos_map_values_are_english():
    for val in POS_MAP.values():
        assert val[0].isupper(), f"POS value should be capitalized: {val}"
        assert val.isascii(), f"POS value should be ASCII: {val}"


def test_valid_pos_covers_pos_map():
    for val in POS_MAP.values():
        assert val in VALID_POS


def test_nai_adjectives_end_with_nai():
    for adj in NAI_ADJECTIVES:
        assert adj.endswith("ない"), f"{adj} should end with ない"


def test_slang_adj_stems_nonempty():
    assert len(SLANG_ADJ_STEMS) >= 6


def test_slang_verb_stems_nonempty():
    assert len(SLANG_VERB_STEMS) >= 3


def test_tari_adverb_stems_nonempty():
    assert len(TARI_ADVERB_STEMS) >= 20


def test_compound_verb_lists_nonempty():
    assert len(COMPOUND_VERB_V2_GODAN) >= 20
    assert len(COMPOUND_VERB_V2_ICHIDAN) >= 10


def test_family_terms_is_set():
    assert isinstance(FAMILY_TERMS, set)
    assert "兄ちゃん" in FAMILY_TERMS


def test_honorific_exceptions_includes_family():
    assert "お兄ちゃん" in HONORIFIC_EXCEPTIONS
    assert "お客様" in HONORIFIC_EXCEPTIONS


def test_prefix_exceptions_includes_fixed_words():
    assert "おいで" in PREFIX_EXCEPTIONS
    assert "お金" in PREFIX_EXCEPTIONS


def test_particle_corrections_all_particle():
    for v in PARTICLE_CORRECTIONS.values():
        assert v == "Particle"


def test_word_exceptions_nonempty():
    assert len(WORD_EXCEPTIONS) >= 10


def test_pos_norm_map_completeness():
    """All uppercase POS values should be in norm map."""
    for val in POS_MAP.values():
        assert val.upper() in POS_NORM_MAP
