"""Tests proving each lexicon-reading normalization rule against a fake lexicon.

Every test injects its own headwords, so the expectations hold whatever the
real data/core TSVs contain. An autouse fixture empties every lexicon accessor
and stubs the reference analyzer, so a rule can only react to what a test
registers.
"""

import copy
import sys
from typing import ClassVar

import pytest

import suzume_mcp.core.postprocessors  # noqa: F401  (imports every rule module)
from suzume_mcp.core.merge_postprocessor_classical_verbs import _postprocess_plain_verb_e_row_lemma
from suzume_mcp.core.merge_rules import apply_suzume_merge
from suzume_mcp.core.postprocessor_function_words import postprocess_listed_interjection
from suzume_mcp.core.postprocessor_predicates import postprocess_adjective_garu, postprocess_bare_sa_row_imperative
from suzume_mcp.core.postprocessor_subsidiaries import postprocess_mu_verb_desiderative
from tests.helpers import tok as _tok

CORE = "suzume_mcp.core"
CLOSED_UNITS = f"{CORE}.merge_rules_closed_units"
COMPOUNDS = f"{CORE}.merge_rules_compounds"
CLASSICAL_VERBS = f"{CORE}.merge_postprocessor_classical_verbs"
PREDICATES = f"{CORE}.postprocessor_predicates"
SUBSIDIARIES = f"{CORE}.postprocessor_subsidiaries"


def _opaque_analyzer(text, *_args, **_kwargs):
    """Stand in for the reference analyzer: every text is one unknown noun, so no probe finds a reading."""
    return [{"surface": text, "pos": "名詞", "pos_sub1": "一般", "lemma": text}]


@pytest.fixture(autouse=True)
def isolated_lexicon(monkeypatch):
    """Empty every lexicon accessor and replace the reference analyzer with an opaque stub."""
    monkeypatch.setattr(f"{CORE}.core_lexicon.core_entries", lambda filename: ())
    for name, module in list(sys.modules.items()):
        if not name.startswith(f"{CORE}.") or module is None:
            continue
        if hasattr(module, "core_headwords"):
            monkeypatch.setattr(module, "core_headwords", lambda filename: frozenset())
        if hasattr(module, "core_headwords_by_length"):
            monkeypatch.setattr(module, "core_headwords_by_length", lambda filename: ())
        if hasattr(module, "mecab_analyze"):
            monkeypatch.setattr(module, "mecab_analyze", _opaque_analyzer)


def _headwords(monkeypatch, module, files):
    """Register headwords per TSV for `core_headwords` in one rule module."""
    monkeypatch.setattr(f"{module}.core_headwords", lambda filename: frozenset(files.get(filename, ())))


def _headwords_by_length(monkeypatch, module, files):
    """Register headwords per TSV for `core_headwords_by_length`, longest first."""
    monkeypatch.setattr(
        f"{module}.core_headwords_by_length",
        lambda filename: tuple(sorted(files.get(filename, ()), key=len, reverse=True)),
    )


def _entries(monkeypatch, files):
    """Register raw TSV rows per file for the helpers built on `core_entries`."""
    monkeypatch.setattr(f"{CORE}.core_lexicon.core_entries", lambda filename: tuple(files.get(filename, ())))


def _assert_merge_untouched(tokens, text):
    """Assert no rule fired; the pipeline only fills in empty detail fields, so compare the core triple."""
    merged, rule = apply_suzume_merge(copy.deepcopy(tokens), text)
    assert [(t["surface"], t["pos"], t["lemma"]) for t in merged] == [
        (t["surface"], t["pos"], t["lemma"]) for t in tokens
    ]
    assert rule is None


class TestTopicBeforeListedNoun:
    @staticmethod
    def _tokens():
        return [_tok("そこ", pos="名詞", pos_sub1="代名詞"), _tok("はほげら"), _tok("だ", pos="助動詞")]

    def test_splits_topic_from_a_listed_noun(self, monkeypatch):
        _headwords_by_length(monkeypatch, CLOSED_UNITS, {"nouns.tsv": ["ほげら"]})

        merged, rule = apply_suzume_merge(self._tokens(), "そこはほげらだ")

        assert [(t["surface"], t["pos"], t["lemma"]) for t in merged] == [
            ("そこ", "名詞", "そこ"),
            ("は", "助詞", "は"),
            ("ほげら", "Noun", "ほげら"),
            ("だ", "助動詞", "だ"),
        ]
        assert rule == "topic+l2-noun-boundary"

    def test_splits_topic_from_a_noun_spread_over_several_tokens(self, monkeypatch):
        _headwords_by_length(monkeypatch, CLOSED_UNITS, {"nouns.tsv": ["ほげらぐち"]})
        tokens = [
            _tok("ここ", pos="名詞", pos_sub1="代名詞"),
            _tok("はほげ", pos="動詞"),
            _tok("らぐち"),
            _tok("だ", pos="助動詞"),
        ]

        merged, rule = apply_suzume_merge(tokens, "ここはほげらぐちだ")

        assert [t["surface"] for t in merged] == ["ここ", "は", "ほげらぐち", "だ"]
        assert merged[2] == {"surface": "ほげらぐち", "pos": "Noun", "lemma": "ほげらぐち"}
        assert rule == "topic+l2-noun-boundary"

    def test_leaves_tokens_alone_when_no_noun_is_listed(self):
        _assert_merge_untouched(self._tokens(), "そこはほげらだ")

    @pytest.mark.parametrize(
        ("files", "reason"),
        [
            ({"nouns.tsv": ["ほげり"]}, "one character differs"),
            ({"nouns.tsv": ["ほげ"]}, "prefix of the span"),
            ({"nouns.tsv": ["ほげらる"]}, "superset of the span"),
            ({"adjectives.tsv": ["ほげら"]}, "listed in another file"),
        ],
    )
    def test_ignores_a_near_miss_noun(self, monkeypatch, files, reason):
        _headwords_by_length(monkeypatch, CLOSED_UNITS, files)

        _assert_merge_untouched(self._tokens(), "そこはほげらだ")

    def test_keeps_a_standalone_listed_noun_without_a_preceding_nominal(self, monkeypatch):
        _headwords_by_length(monkeypatch, CLOSED_UNITS, {"nouns.tsv": ["ほげら"]})

        _assert_merge_untouched([_tok("はほげら"), _tok("だ", pos="助動詞")], "はほげらだ")


class TestListedNounSpanAcrossCells:
    """A listed noun whose span crosses a reference cell boundary at either end."""

    _CELLS: ClassVar[dict[str, list[dict[str, str]]]] = {
        "し": [{"surface": "し", "pos": "動詞", "pos_sub1": "自立", "conj_form": "連用形", "lemma": "する"}],
        "せ": [{"surface": "せ", "pos": "動詞", "pos_sub1": "自立", "conj_form": "連用形", "lemma": "せる"}],
    }

    @pytest.fixture(autouse=True)
    def _reference_cells(self, monkeypatch):
        monkeypatch.setattr(
            f"{CLOSED_UNITS}.mecab_analyze", lambda text, *_a, **_k: self._CELLS.get(text) or _opaque_analyzer(text)
        )
        _headwords_by_length(monkeypatch, CLOSED_UNITS, {"nouns.tsv": ["ほげら"]})

    def test_splits_case_particle_from_a_noun_starting_inside_its_cell(self):
        tokens = [
            _tok("ここ", pos="名詞", pos_sub1="代名詞"),
            _tok("でほ", pos="動詞"),
            _tok("げら"),
            _tok("する", pos="動詞"),
        ]

        merged, rule = apply_suzume_merge(tokens, "ここでほげらする")

        assert [(t["surface"], t["pos"], t["lemma"]) for t in merged] == [
            ("ここ", "名詞", "ここ"),
            ("で", "助詞", "で"),
            ("ほげら", "Noun", "ほげら"),
            ("する", "動詞", "する"),
        ]
        assert rule == "l2-noun-span"

    def test_keeps_a_case_particle_cell_without_a_preceding_nominal(self):
        _assert_merge_untouched([_tok("でほ", pos="動詞"), _tok("げら"), _tok("する", pos="動詞")], "でほげらする")

    @pytest.mark.parametrize(
        ("left", "text"),
        [([], "おほげらを"), ([_tok("の", pos="助詞", pos_sub1="連体化")], "のおほげらを")],
    )
    def test_splits_prefix_from_a_noun_starting_inside_its_cell(self, left, text):
        tokens = [*left, _tok("おほ", pos="動詞"), _tok("げ"), _tok("ら"), _tok("を", pos="助詞")]

        merged, rule = apply_suzume_merge(tokens, text)

        assert [(t["surface"], t["pos"], t["lemma"]) for t in merged][len(left) :] == [
            ("お", "接頭詞", "お"),
            ("ほげら", "Noun", "ほげら"),
            ("を", "助詞", "を"),
        ]
        assert rule == "l2-noun-span"

    def test_re_reads_a_light_verb_fused_into_the_last_cell(self):
        tokens = [
            _tok("部屋", pos="名詞"),
            _tok("を", pos="助詞", pos_sub1="格助詞"),
            _tok("ほげ", pos="副詞"),
            _tok("らし", pos="動詞", conj_form="連用形"),
            _tok("た", pos="助動詞"),
        ]

        merged, rule = apply_suzume_merge(tokens, "部屋をほげらした")

        assert [(t["surface"], t["pos"], t["lemma"]) for t in merged] == [
            ("部屋", "名詞", "部屋"),
            ("を", "助詞", "を"),
            ("ほげら", "Noun", "ほげら"),
            ("し", "動詞", "する"),
            ("た", "助動詞", "た"),
        ]
        assert rule == "l2-noun-span"

    def test_keeps_a_last_cell_whose_overflow_is_not_the_light_verb(self):
        tokens = [_tok("ほげ", pos="副詞"), _tok("らせ", pos="動詞"), _tok("た", pos="助動詞")]

        _assert_merge_untouched(tokens, "ほげらせた")

    def test_keeps_a_prefixed_noun_the_reference_spells_as_one_cell(self):
        _assert_merge_untouched([_tok("おほげら"), _tok("を", pos="助詞")], "おほげらを")

    def test_never_breaks_a_nominal_cell_at_the_left_edge(self, monkeypatch):
        # 声 + がけ (suffix) + が: a listed けが must not cut the suffix cell the reference chose.
        _headwords_by_length(monkeypatch, CLOSED_UNITS, {"nouns.tsv": ["けが"]})
        tokens = [
            _tok("声", pos="名詞"),
            _tok("がけ", pos="名詞", pos_sub1="接尾"),
            _tok("が", pos="助詞", pos_sub1="格助詞"),
            _tok("大切", pos="名詞", pos_sub1="形容動詞語幹"),
            _tok("だ", pos="助動詞"),
        ]

        _assert_merge_untouched(tokens, "声がけが大切だ")

    @pytest.mark.parametrize(
        ("tokens", "text"),
        [
            # ご + listed とき inside one auxiliary cell (ごとき).
            pytest.param(
                [_tok("の", pos="助詞", pos_sub1="連体化"), _tok("ごほげら", pos="助動詞"), _tok("だ", pos="助動詞")],
                "のごほげらだ",
                id="head-inside-one-cell",
            ),
            # listed noun + light verb inside one verb cell (はなし, 思い出し).
            pytest.param(
                [_tok("を", pos="助詞", pos_sub1="格助詞"), _tok("ほげらし", pos="動詞"), _tok("た", pos="助動詞")],
                "をほげらした",
                id="overflow-inside-one-cell",
            ),
        ],
    )
    def test_keeps_a_listed_noun_that_crosses_no_cell_boundary(self, tokens, text):
        _assert_merge_untouched(tokens, text)

    def test_never_breaks_a_nominal_cell_at_the_right_edge(self):
        tokens = [_tok("ほげ", pos="副詞"), _tok("らし", pos="名詞"), _tok("た", pos="助動詞")]

        _assert_merge_untouched(tokens, "ほげらした")


class TestClippedGreeting:
    GREETING = ("ほげそう", "INTERJECTION")

    def test_rejoins_a_clipped_greeting_with_the_full_lemma(self, monkeypatch):
        _entries(monkeypatch, {"expressions.tsv": [self.GREETING]})

        merged, rule = apply_suzume_merge([_tok("ほげそ", pos="感動詞")], "ほげそ")

        assert merged == [{"surface": "ほげそ", "pos": "感動詞", "lemma": "ほげそう"}]
        assert rule == "clipped-greeting"

    def test_absorbs_a_held_vowel_mark_and_a_closing_sokuon(self, monkeypatch):
        _entries(monkeypatch, {"expressions.tsv": [self.GREETING]})

        for text, tokens in (
            ("ほげそー", [_tok("ほげそ", pos="感動詞"), _tok("ー", pos="記号")]),
            ("ほげそっ", [_tok("ほげそ", pos="感動詞"), _tok("っ", pos="助詞")]),
        ):
            merged, rule = apply_suzume_merge(tokens, text)

            assert merged == [{"surface": text, "pos": "感動詞", "lemma": "ほげそう"}]
            assert rule == "clipped-greeting"

    def test_hands_off_to_a_final_particle(self, monkeypatch):
        _entries(monkeypatch, {"expressions.tsv": [self.GREETING]})
        tokens = [_tok("ほげそ", pos="感動詞"), _tok("ね", pos="助詞")]

        merged, rule = apply_suzume_merge(tokens, "ほげそね")

        assert merged[0] == {"surface": "ほげそ", "pos": "感動詞", "lemma": "ほげそう"}
        assert merged[1]["surface"] == "ね"
        assert rule == "clipped-greeting"

    def test_leaves_the_span_alone_when_no_greeting_is_listed(self):
        _assert_merge_untouched([_tok("ほげそ", pos="感動詞")], "ほげそ")

    @pytest.mark.parametrize(
        "files",
        [
            pytest.param({"expressions.tsv": [("ほげぞう", "INTERJECTION")]}, id="one-character-differs"),
            pytest.param({"expressions.tsv": [("ほげそそう", "INTERJECTION")]}, id="superset-headword"),
            pytest.param({"expressions.tsv": [("ほげそう", "NOUN")]}, id="wrong-class"),
            pytest.param({"nouns.tsv": [("ほげそう", "INTERJECTION")]}, id="wrong-file"),
            pytest.param({"expressions.tsv": [("ほげいう", "INTERJECTION")]}, id="closing-mora-not-o-row"),
            pytest.param({"expressions.tsv": [("ほげそ", "INTERJECTION")]}, id="headword-lacks-long-vowel"),
        ],
    )
    def test_ignores_a_near_miss_entry(self, monkeypatch, files):
        _entries(monkeypatch, files)

        _assert_merge_untouched([_tok("ほげそ", pos="感動詞")], "ほげそ")

    def test_does_not_clip_before_a_non_particle_kana(self, monkeypatch):
        _entries(monkeypatch, {"expressions.tsv": [self.GREETING]})

        merged, rule = apply_suzume_merge([_tok("ほげそ", pos="感動詞"), _tok("た", pos="助動詞")], "ほげそた")

        assert rule != "clipped-greeting"
        assert [t["surface"] for t in merged] == ["ほげそ", "た"]


class TestListedNounMerge:
    def test_merges_adjacent_tokens_spelling_a_listed_noun(self, monkeypatch):
        _headwords_by_length(monkeypatch, CLOSED_UNITS, {"nouns.tsv": ["ぷるげ"]})
        tokens = [_tok("ぷる"), _tok("げ"), _tok("を", pos="助詞")]

        merged, rule = apply_suzume_merge(tokens, "ぷるげを")

        assert [(t["surface"], t["pos"], t["lemma"]) for t in merged] == [
            ("ぷるげ", "Noun", "ぷるげ"),
            ("を", "助詞", "を"),
        ]
        assert rule == "l2-noun"

    def test_prefers_the_longest_listed_noun(self, monkeypatch):
        _headwords_by_length(monkeypatch, CLOSED_UNITS, {"nouns.tsv": ["ぷる", "ぷるげら"]})
        tokens = [_tok("ぷる"), _tok("げ"), _tok("ら")]

        merged, rule = apply_suzume_merge(tokens, "ぷるげら")

        assert merged == [{"surface": "ぷるげら", "pos": "Noun", "lemma": "ぷるげら"}]
        assert rule == "l2-noun"

    def test_leaves_tokens_alone_when_no_noun_is_listed(self):
        _assert_merge_untouched([_tok("ぷる"), _tok("げ"), _tok("を", pos="助詞")], "ぷるげを")

    @pytest.mark.parametrize(
        ("files", "reason"),
        [
            ({"nouns.tsv": ["ぷるそ"]}, "one character differs"),
            ({"nouns.tsv": ["ぷるげらん"]}, "superset of the span"),
            ({"nouns.tsv": ["ぷる"]}, "prefix ending inside no token boundary of a longer span"),
            ({"adjectives.tsv": ["ぷるげ"]}, "listed in another file"),
        ],
    )
    def test_ignores_a_near_miss_noun(self, monkeypatch, files, reason):
        _headwords_by_length(monkeypatch, CLOSED_UNITS, files)

        _assert_merge_untouched([_tok("ぷる"), _tok("げ"), _tok("を", pos="助詞")], "ぷるげを")

    def test_does_not_rewrite_a_single_token_that_already_spells_the_noun(self, monkeypatch):
        _headwords_by_length(monkeypatch, CLOSED_UNITS, {"nouns.tsv": ["ぷるげ"]})

        _assert_merge_untouched([_tok("ぷるげ"), _tok("を", pos="助詞")], "ぷるげを")


class TestNounPlusKanaAdjective:
    @staticmethod
    def _tokens(adjective_surface="ぷるしかっ", adjective_lemma="ぷるしい"):
        return [
            _tok("ほげ"),
            _tok(adjective_surface, pos="形容詞", lemma=adjective_lemma, conj_form="連用タ接続"),
        ]

    def test_joins_a_noun_and_a_kana_adjective_when_the_whole_is_listed(self, monkeypatch):
        _headwords(monkeypatch, COMPOUNDS, {"adjectives.tsv": ["ほげぷるしい"]})

        merged, rule = apply_suzume_merge(self._tokens(), "ほげぷるしかっ")

        assert merged == [
            _tok("ほげぷるしかっ", pos="形容詞", lemma="ほげぷるしい", conj_form="連用タ接続"),
        ]
        assert rule == "noun+kana-adjective-listed"

    def test_leaves_tokens_alone_when_no_adjective_is_listed(self):
        _assert_merge_untouched(self._tokens(), "ほげぷるしかっ")

    @pytest.mark.parametrize(
        "files",
        [
            pytest.param({"adjectives.tsv": ["ほげぷるそい"]}, id="one-character-differs"),
            pytest.param({"adjectives.tsv": ["ほげぷるし"]}, id="prefix-of-the-joined-lemma"),
            pytest.param({"adjectives.tsv": ["ほげぷるしいい"]}, id="superset-of-the-joined-lemma"),
            pytest.param({"adjectives.tsv": ["ぷるしい"]}, id="adjective-alone-without-the-noun"),
            pytest.param({"nouns.tsv": ["ほげぷるしい"]}, id="listed-in-another-file"),
        ],
    )
    def test_ignores_a_near_miss_headword(self, monkeypatch, files):
        _headwords(monkeypatch, COMPOUNDS, files)

        _assert_merge_untouched(self._tokens(), "ほげぷるしかっ")

    def test_does_not_join_a_kanji_adjective(self, monkeypatch):
        _headwords(monkeypatch, COMPOUNDS, {"adjectives.tsv": ["ほげ古い"]})
        tokens = [_tok("ほげ"), _tok("古い", pos="形容詞")]

        _assert_merge_untouched(tokens, "ほげ古い")


class TestListedKanaAdjective:
    ENTRY = ("ぷるしい", "ADJECTIVE", "I_ADJ")

    def test_merges_the_split_cell_into_the_listed_adjective(self, monkeypatch):
        _entries(monkeypatch, {"adjectives.tsv": [self.ENTRY]})

        merged, rule = apply_suzume_merge([_tok("ぷる", pos="動詞"), _tok("しかっ", pos="形容詞")], "ぷるしかっ")

        assert merged == [{"surface": "ぷるしかっ", "pos": "形容詞", "lemma": "ぷるしい"}]
        assert rule == "l2-adjective"

    def test_merges_the_attributive_cell(self, monkeypatch):
        _entries(monkeypatch, {"adjectives.tsv": [self.ENTRY]})
        tokens = [_tok("ぷる", pos="動詞"), _tok("し", pos="名詞"), _tok("い", pos="助動詞")]

        merged, rule = apply_suzume_merge(tokens, "ぷるしい")

        assert merged == [{"surface": "ぷるしい", "pos": "形容詞", "lemma": "ぷるしい"}]
        assert rule == "l2-adjective"

    def test_leaves_tokens_alone_when_no_adjective_is_listed(self):
        _assert_merge_untouched([_tok("ぷる", pos="動詞"), _tok("しかっ", pos="形容詞")], "ぷるしかっ")

    @pytest.mark.parametrize(
        "files",
        [
            pytest.param({"adjectives.tsv": [("ぷるそい", "ADJECTIVE", "I_ADJ")]}, id="one-character-differs"),
            pytest.param({"adjectives.tsv": [("ぷるしいい", "ADJECTIVE", "I_ADJ")]}, id="superset-headword"),
            pytest.param({"adjectives.tsv": [("ぷい", "ADJECTIVE", "I_ADJ")]}, id="shorter-headword"),
            pytest.param({"adjectives.tsv": [("ぷるしい", "NOUN", "I_ADJ")]}, id="wrong-class"),
            pytest.param({"adjectives.tsv": [("ぷるしい", "ADJECTIVE", "NA_ADJ")]}, id="wrong-conjugation"),
            pytest.param({"adjectives.tsv": [("ぷる死い", "ADJECTIVE", "I_ADJ")]}, id="not-wholly-kana"),
            pytest.param({"verbs.tsv": [("ぷるしい", "ADJECTIVE", "I_ADJ")]}, id="wrong-file"),
        ],
    )
    def test_ignores_a_near_miss_entry(self, monkeypatch, files):
        _entries(monkeypatch, files)

        _assert_merge_untouched([_tok("ぷる", pos="動詞"), _tok("しかっ", pos="形容詞")], "ぷるしかっ")

    def test_does_not_rewrite_a_single_token_cell(self, monkeypatch):
        _entries(monkeypatch, {"adjectives.tsv": [self.ENTRY]})

        merged, rule = apply_suzume_merge([_tok("ぷるしかっ", pos="形容詞", lemma="ぷるしい")], "ぷるしかっ")

        assert [(t["surface"], t["pos"], t["lemma"]) for t in merged] == [("ぷるしかっ", "形容詞", "ぷるしい")]
        assert rule is None


class TestListedKanaIchidanVerb:
    ENTRY = ("ぷらべる", "VERB", "ICHIDAN")

    @staticmethod
    def _verb(surface, form):
        return {
            "surface": surface,
            "pos": "動詞",
            "pos_sub1": "自立",
            "conj_type": "一段",
            "conj_form": form,
            "lemma": "ぷらべる",
        }

    def test_merges_the_stem_and_terminal_into_the_listed_verb(self, monkeypatch):
        _entries(monkeypatch, {"verbs.tsv": [self.ENTRY]})

        merged, rule = apply_suzume_merge([_tok("ぷら", pos="動詞"), _tok("べる", pos="動詞")], "ぷらべる")

        assert merged == [self._verb("ぷらべる", "基本形")]
        assert rule == "kana-ichidan-lexicon"

    def test_merges_a_bare_stem_as_the_continuative(self, monkeypatch):
        _entries(monkeypatch, {"verbs.tsv": [self.ENTRY]})
        tokens = [_tok("ぷ", pos="助詞"), _tok("らべ", pos="動詞"), _tok("た", pos="助動詞")]

        merged, rule = apply_suzume_merge(tokens, "ぷらべた")

        assert merged[0] == self._verb("ぷらべ", "連用形")
        assert [(t["surface"], t["pos"]) for t in merged[1:]] == [("た", "助動詞")]
        assert rule == "kana-ichidan-lexicon"

    def test_does_not_start_on_a_particle_right_after_a_predicate(self, monkeypatch):
        _entries(monkeypatch, {"verbs.tsv": [self.ENTRY]})
        tokens = [_tok("よん", pos="動詞"), _tok("ぷら", pos="助詞"), _tok("べる", pos="動詞")]

        _assert_merge_untouched(tokens, "よんぷらべる")

    def test_leaves_tokens_alone_when_no_verb_is_listed(self):
        _assert_merge_untouched([_tok("ぷら", pos="動詞"), _tok("べる", pos="動詞")], "ぷらべる")

    @pytest.mark.parametrize(
        "files",
        [
            pytest.param({"verbs.tsv": [("ぷらぺる", "VERB", "ICHIDAN")]}, id="one-character-differs"),
            pytest.param({"verbs.tsv": [("ぷらべるべる", "VERB", "ICHIDAN")]}, id="superset-headword"),
            pytest.param({"verbs.tsv": [("ぷる", "VERB", "ICHIDAN")]}, id="shorter-headword"),
            pytest.param({"verbs.tsv": [("ぷらべる", "VERB", "GODAN_RA")]}, id="wrong-conjugation"),
            pytest.param({"verbs.tsv": [("ぷらべる", "NOUN", "ICHIDAN")]}, id="wrong-class"),
            pytest.param({"verbs.tsv": [("ぷら辺る", "VERB", "ICHIDAN")]}, id="not-wholly-kana"),
            pytest.param({"nouns.tsv": [("ぷらべる", "VERB", "ICHIDAN")]}, id="wrong-file"),
        ],
    )
    def test_ignores_a_near_miss_entry(self, monkeypatch, files):
        _entries(monkeypatch, files)

        _assert_merge_untouched([_tok("ぷら", pos="動詞"), _tok("べる", pos="動詞")], "ぷらべる")

    def test_does_not_rewrite_a_single_token_verb(self, monkeypatch):
        _entries(monkeypatch, {"verbs.tsv": [self.ENTRY]})

        _assert_merge_untouched([_tok("ぷらべる", pos="動詞")], "ぷらべる")


class TestMuVerbDesiderative:
    @staticmethod
    def _tokens():
        return [
            {"surface": "ぷるし", "pos": "Adjective", "lemma": "ぷるしい"},
            {"surface": "みたい", "pos": "Auxiliary", "lemma": "みたい"},
        ]

    def test_resegments_into_the_listed_verb_continuative_and_tai(self, monkeypatch):
        _headwords(monkeypatch, SUBSIDIARIES, {"verbs.tsv": ["ぷるしむ"]})
        tokens = self._tokens()

        assert postprocess_mu_verb_desiderative(tokens)

        assert tokens == [
            {"surface": "ぷるしみ", "pos": "Verb", "lemma": "ぷるしむ"},
            {"surface": "たい", "pos": "Auxiliary", "lemma": "たい"},
        ]

    def test_keeps_the_adjective_when_no_verb_is_listed(self):
        tokens = self._tokens()

        assert not postprocess_mu_verb_desiderative(tokens)
        assert tokens == self._tokens()

    @pytest.mark.parametrize(
        "files",
        [
            pytest.param({"verbs.tsv": ["ぷるしぬ"]}, id="one-character-differs"),
            pytest.param({"verbs.tsv": ["ぷるし"]}, id="prefix-without-mu"),
            pytest.param({"verbs.tsv": ["ぷるしむる"]}, id="superset-headword"),
            pytest.param({"verbs.tsv": ["ぷしむ"]}, id="different-stem"),
            pytest.param({"nouns.tsv": ["ぷるしむ"]}, id="listed-in-another-file"),
        ],
    )
    def test_ignores_a_near_miss_verb(self, monkeypatch, files):
        _headwords(monkeypatch, SUBSIDIARIES, files)
        tokens = self._tokens()

        assert not postprocess_mu_verb_desiderative(tokens)
        assert tokens == self._tokens()


class TestAdjectiveGaru:
    ADJECTIVES = (("ぷるしい", "ADJECTIVE", "I_ADJ"), ("ぽるげ", "ADJECTIVE", "NA_ADJ"))

    def test_splits_a_garu_verb_after_a_listed_i_adjective_stem(self, monkeypatch):
        _entries(monkeypatch, {"adjectives.tsv": self.ADJECTIVES})
        tokens = [_tok("ぷるしがる", "Verb")]

        assert postprocess_adjective_garu(tokens)

        assert tokens == [
            {"surface": "ぷるし", "pos": "Adjective", "lemma": "ぷるしい"},
            {"surface": "がる", "pos": "Verb", "lemma": "がる"},
        ]

    def test_splits_a_garu_form_after_a_listed_na_adjective(self, monkeypatch):
        _entries(monkeypatch, {"adjectives.tsv": self.ADJECTIVES})
        tokens = [_tok("ぽるげがっ", "Verb")]

        assert postprocess_adjective_garu(tokens)

        assert tokens == [
            {"surface": "ぽるげ", "pos": "Adjective", "lemma": "ぽるげ"},
            {"surface": "がっ", "pos": "Verb", "lemma": "がる"},
        ]

    def test_keeps_a_listed_verb_surface_or_lemma(self, monkeypatch):
        _entries(monkeypatch, {"adjectives.tsv": self.ADJECTIVES})
        _headwords(monkeypatch, PREDICATES, {"verbs.tsv": ["ぷるしがる"]})

        for token in (_tok("ぷるしがる", "Verb"), _tok("ぷるしがり", "Verb", lemma="ぷるしがる")):
            tokens = [token]
            before = copy.deepcopy(tokens)

            assert not postprocess_adjective_garu(tokens)
            assert tokens == before

    def test_retags_the_stem_before_a_separate_garu_token(self, monkeypatch):
        _entries(monkeypatch, {"adjectives.tsv": self.ADJECTIVES})
        tokens = [_tok("ぷるし", "Noun"), _tok("がる", "Auxiliary")]

        assert postprocess_adjective_garu(tokens)

        assert tokens == [
            {"surface": "ぷるし", "pos": "Adjective", "lemma": "ぷるしい"},
            {"surface": "がる", "pos": "Verb", "lemma": "がる"},
        ]

    def test_a_listed_noun_host_licenses_garu_and_stays_a_noun(self, monkeypatch):
        _headwords(monkeypatch, PREDICATES, {"nouns.tsv": ["ほげら"]})
        tokens = [_tok("ほげら", "Noun"), _tok("がる", "Auxiliary")]

        assert postprocess_adjective_garu(tokens)

        assert tokens == [_tok("ほげら", "Noun"), {"surface": "がる", "pos": "Verb", "lemma": "がる"}]

    @pytest.mark.parametrize(
        "token",
        [
            pytest.param(_tok("ぷるしがる", "Verb"), id="garu-verb"),
            pytest.param(_tok("ぽるげがる", "Verb"), id="garu-verb-na"),
        ],
    )
    def test_leaves_garu_forms_alone_when_nothing_is_listed(self, token):
        tokens = [copy.deepcopy(token)]

        assert not postprocess_adjective_garu(tokens)
        assert tokens == [token]

    def test_leaves_a_separate_garu_token_alone_when_nothing_is_listed(self):
        tokens = [_tok("ぷるし", "Noun"), _tok("がる", "Auxiliary")]

        assert not postprocess_adjective_garu(tokens)
        assert tokens == [_tok("ぷるし", "Noun"), _tok("がる", "Auxiliary")]

    @pytest.mark.parametrize(
        "files",
        [
            pytest.param({"adjectives.tsv": [("ぷるそい", "ADJECTIVE", "I_ADJ")]}, id="one-character-differs"),
            pytest.param({"adjectives.tsv": [("ぷるしいい", "ADJECTIVE", "I_ADJ")]}, id="superset-headword"),
            pytest.param({"adjectives.tsv": [("ぷるい", "ADJECTIVE", "I_ADJ")]}, id="shorter-headword"),
            pytest.param({"adjectives.tsv": [("ぷるし", "ADJECTIVE", "I_ADJ")]}, id="i-adjective-without-final-i"),
            pytest.param({"adjectives.tsv": [("ぷるしい", "ADJECTIVE", "NA_ADJ")]}, id="wrong-conjugation"),
            pytest.param({"adjectives.tsv": [("ぷるしい", "NOUN", "I_ADJ")]}, id="wrong-class"),
            pytest.param({"nouns.tsv": [("ぷるしい", "ADJECTIVE", "I_ADJ")]}, id="wrong-file"),
        ],
    )
    def test_ignores_a_near_miss_adjective_entry(self, monkeypatch, files):
        _entries(monkeypatch, files)
        split_case = [_tok("ぷるしがる", "Verb")]
        retag_case = [_tok("ぷるし", "Noun"), _tok("がる", "Auxiliary")]

        assert not postprocess_adjective_garu(split_case)
        assert not postprocess_adjective_garu(retag_case)
        assert split_case == [_tok("ぷるしがる", "Verb")]
        assert retag_case == [_tok("ぷるし", "Noun"), _tok("がる", "Auxiliary")]

    @pytest.mark.parametrize(
        "files",
        [
            pytest.param({"nouns.tsv": ["ほげり"]}, id="one-character-differs"),
            pytest.param({"nouns.tsv": ["ほげ"]}, id="prefix-headword"),
            pytest.param({"nouns.tsv": ["ほげらる"]}, id="superset-headword"),
            pytest.param({"verbs.tsv": ["ほげら"]}, id="listed-in-another-file"),
        ],
    )
    def test_ignores_a_near_miss_noun_host(self, monkeypatch, files):
        _headwords(monkeypatch, PREDICATES, files)
        tokens = [_tok("ほげら", "Noun"), _tok("がる", "Auxiliary")]

        assert not postprocess_adjective_garu(tokens)
        assert tokens == [_tok("ほげら", "Noun"), _tok("がる", "Auxiliary")]

    def test_a_listed_noun_spelling_does_not_license_garu_on_a_non_noun(self, monkeypatch):
        _headwords(monkeypatch, PREDICATES, {"nouns.tsv": ["ほげら"]})
        tokens = [_tok("ほげら", "Verb"), _tok("がる", "Auxiliary")]

        assert not postprocess_adjective_garu(tokens)
        assert tokens == [_tok("ほげら", "Verb"), _tok("がる", "Auxiliary")]


class TestBareSaRowImperative:
    @staticmethod
    def _causative_stem():
        return [_tok("ほげか", "Verb", lemma="ほげく"), _tok("せ", "Auxiliary", lemma="せる")]

    @staticmethod
    def _lone_form():
        return [_tok("ほげかせ", "Verb", lemma="ほげかせる")]

    def test_merges_a_stem_and_se_into_the_listed_sa_row_verb(self, monkeypatch):
        _headwords(monkeypatch, PREDICATES, {"verbs.tsv": ["ほげかす"]})
        tokens = self._causative_stem()

        assert postprocess_bare_sa_row_imperative(tokens)

        assert tokens == [{"surface": "ほげかせ", "pos": "Verb", "lemma": "ほげかす"}]

    def test_relemmatizes_a_lone_se_form(self, monkeypatch):
        _headwords(monkeypatch, PREDICATES, {"verbs.tsv": ["ほげかす"]})
        tokens = self._lone_form()

        assert postprocess_bare_sa_row_imperative(tokens)

        assert tokens == [_tok("ほげかせ", "Verb", lemma="ほげかす")]

    def test_a_longer_kana_tail_of_the_stem_is_enough(self, monkeypatch):
        _headwords(monkeypatch, PREDICATES, {"verbs.tsv": ["げかす"]})
        tokens = self._lone_form()

        assert postprocess_bare_sa_row_imperative(tokens)

        assert tokens[0]["lemma"] == "ほげかす"

    def test_a_single_kanji_tail_is_enough(self, monkeypatch):
        _headwords(monkeypatch, PREDICATES, {"verbs.tsv": ["出す"]})
        tokens = [_tok("ほげ出せ", "Verb", lemma="ほげ出せる")]

        assert postprocess_bare_sa_row_imperative(tokens)

        assert tokens[0]["lemma"] == "ほげ出す"

    def test_accepts_the_imperative_before_a_final_particle(self, monkeypatch):
        _headwords(monkeypatch, PREDICATES, {"verbs.tsv": ["ほげかす"]})
        tokens = [*self._lone_form(), _tok("よ", "Particle")]

        assert postprocess_bare_sa_row_imperative(tokens)

        assert tokens[0]["lemma"] == "ほげかす"

    @pytest.mark.parametrize("build", ["_causative_stem", "_lone_form"])
    def test_leaves_tokens_alone_when_no_verb_is_listed(self, build):
        tokens = getattr(self, build)()

        assert not postprocess_bare_sa_row_imperative(tokens)
        assert tokens == getattr(self, build)()

    @pytest.mark.parametrize(
        "files",
        [
            pytest.param({"verbs.tsv": ["ほげさす"]}, id="one-character-differs"),
            pytest.param({"verbs.tsv": ["ほげか"]}, id="prefix-without-su"),
            pytest.param({"verbs.tsv": ["ほげかすす"]}, id="superset-headword"),
            pytest.param({"verbs.tsv": ["ほげかせる"]}, id="ichidan-verb-not-sa-row"),
            pytest.param({"verbs.tsv": ["かす"]}, id="lone-kana-tail"),
            pytest.param({"nouns.tsv": ["ほげかす"]}, id="listed-in-another-file"),
        ],
    )
    @pytest.mark.parametrize("build", ["_causative_stem", "_lone_form"])
    def test_ignores_a_near_miss_verb(self, monkeypatch, files, build):
        _headwords(monkeypatch, PREDICATES, files)
        tokens = getattr(self, build)()

        assert not postprocess_bare_sa_row_imperative(tokens)
        assert tokens == getattr(self, build)()


class TestPlainVerbERowLemma:
    @staticmethod
    def _tokens():
        return [
            {"surface": "ほげれ", "pos": "動詞", "lemma": "ほげれる"},
            {"surface": "目標", "pos": "名詞", "pos_sub1": "一般", "lemma": "目標"},
        ]

    @pytest.fixture(autouse=True)
    def analyzer_accepts_every_surface(self, monkeypatch):
        """Take the reference-dictionary check out of the picture; only the lexicon gate is under test."""
        monkeypatch.setattr(f"{CLASSICAL_VERBS}.is_single_token_of_pos", lambda surface, pos: True)

    def test_relemmatizes_to_the_listed_plain_verb_before_a_noun(self, monkeypatch):
        _headwords(monkeypatch, CLASSICAL_VERBS, {"verbs.tsv": ["ほげる"]})

        tagged, rule = _postprocess_plain_verb_e_row_lemma(self._tokens(), None)

        assert tagged[0] == {"surface": "ほげれ", "pos": "動詞", "lemma": "ほげる"}
        assert tagged[1] == self._tokens()[1]
        assert rule == "plain-verb-e-row-lemma"

    def test_leaves_the_lemma_when_no_verb_is_listed(self):
        tagged, rule = _postprocess_plain_verb_e_row_lemma(self._tokens(), None)

        assert tagged == self._tokens()
        assert rule is None

    @pytest.mark.parametrize(
        "files",
        [
            pytest.param({"verbs.tsv": ["ほげろ"]}, id="one-character-differs"),
            pytest.param({"verbs.tsv": ["ほげ"]}, id="prefix-headword"),
            pytest.param({"verbs.tsv": ["ほげるる"]}, id="superset-headword"),
            pytest.param({"verbs.tsv": ["ほげる", "ほげれる"]}, id="e-row-ichidan-verb-also-listed"),
            pytest.param({"nouns.tsv": ["ほげる"]}, id="listed-in-another-file"),
        ],
    )
    def test_ignores_a_near_miss_verb(self, monkeypatch, files):
        _headwords(monkeypatch, CLASSICAL_VERBS, files)

        tagged, rule = _postprocess_plain_verb_e_row_lemma(self._tokens(), None)

        assert tagged == self._tokens()
        assert rule is None


class TestListedInterjection:
    @staticmethod
    def _tokens(pos="Adverb"):
        return [{"surface": "ほげとう", "pos": pos, "lemma": "ほげたい"}]

    @pytest.mark.parametrize("pos", ["Adverb", "Adjective"])
    def test_retags_a_listed_interjection_with_its_surface_as_lemma(self, monkeypatch, pos):
        _entries(monkeypatch, {"expressions.tsv": [("ほげとう", "INTERJECTION")]})
        tokens = self._tokens(pos)

        assert postprocess_listed_interjection(tokens)

        assert tokens == [{"surface": "ほげとう", "pos": "Interjection", "lemma": "ほげとう"}]

    def test_leaves_the_token_when_no_interjection_is_listed(self):
        tokens = self._tokens()

        assert not postprocess_listed_interjection(tokens)
        assert tokens == self._tokens()

    @pytest.mark.parametrize(
        "files",
        [
            pytest.param({"expressions.tsv": [("ほげとお", "INTERJECTION")]}, id="one-character-differs"),
            pytest.param({"expressions.tsv": [("ほげ", "INTERJECTION")]}, id="prefix-headword"),
            pytest.param({"expressions.tsv": [("ほげとうう", "INTERJECTION")]}, id="superset-headword"),
            pytest.param({"expressions.tsv": [("ほげとう", "ADVERB")]}, id="wrong-class"),
            pytest.param({"nouns.tsv": [("ほげとう", "INTERJECTION")]}, id="wrong-file"),
        ],
    )
    def test_ignores_a_near_miss_entry(self, monkeypatch, files):
        _entries(monkeypatch, files)
        tokens = self._tokens()

        assert not postprocess_listed_interjection(tokens)
        assert tokens == self._tokens()

    def test_does_not_retag_a_token_of_another_class(self, monkeypatch):
        _entries(monkeypatch, {"expressions.tsv": [("ほげとう", "INTERJECTION")]})
        tokens = self._tokens("Noun")

        assert not postprocess_listed_interjection(tokens)
        assert tokens == self._tokens("Noun")
