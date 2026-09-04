"""Merge rules ported from SuzumeUtils.pm apply_suzume_merge()."""

import regex

from .constants import (
    COLLOQUIAL_PRONOUNS,
    COMPOUND_VERB_V2_GODAN,
    COMPOUND_VERB_V2_ICHIDAN,
    COMPOUND_VERB_V2_NOT_AFTER_SURU,
    COMPOUND_VERB_V2_SURU_ONLY,
    COUNTER_UNITS,
    DERIVED_ADJECTIVE_SUFFIX_LEMMAS,
    DERIVED_VERB_SUFFIX_FORMS,
    FAMILY_TERMS,
    FIXED_FUNCTION_LEMMAS,
    FIXED_FUNCTION_SEARCH_UNITS,
    FIXED_INFLECTED_FUNCTION_UNITS,
    HIRAGANA_COMPOUNDS,
    KANA_COUNTER_SUFFIXES,
    KANA_NUMBER_STEMS,
    NAI_ADJECTIVES,
    TARI_ADVERB_STEMS,
    TEMPORAL_COMPOUND_UNITS,
    TEMPORAL_PREFIX_KANJI,
)
from .core_lexicon import core_headwords_by_length
from .mecab import is_single_token_of_pos, mecab_analyze
from .merge_postprocessors import (
    KARI_MIZENKEI_CELL,
    _postprocess_adj_bungo,
    _postprocess_adj_kari,
    _postprocess_ascii_joiner_merge,
    _postprocess_atode,
    _postprocess_bound_suffix_noun_cell,
    _postprocess_bound_voiced_suffix,
    _postprocess_classical_mu,
    _postprocess_classical_shimu,
    _postprocess_demo_copula,
    _postprocess_dialectal,
    _postprocess_distributive_quantity,
    _postprocess_epenthetic_sa,
    _postprocess_filler_split,
    _postprocess_gamashii,
    _postprocess_ha_row_godan,
    _postprocess_historical_kana_word,
    _postprocess_honorific_split,
    _postprocess_izenkei_concessive,
    _postprocess_kakari_pronoun_split,
    _postprocess_kamo,
    _postprocess_kanji_merge,
    _postprocess_ku_nominalization,
    _postprocess_kuruwa,
    _postprocess_nde_split,
    _postprocess_nickname_merge,
    _postprocess_nidan_cell,
    _postprocess_nominal_classical_copula,
    _postprocess_nominal_zukeru,
    _postprocess_noni,
    _postprocess_onomatopoeia_tto_merge,
    _postprocess_prefix_split,
    _postprocess_productive_mimetics,
    _postprocess_search_unit_split,
    _postprocess_small_kana_head_merge,
    _postprocess_tomo_particle,
    _postprocess_totomoni,
    classical_adjective_lemma,
    nidan_cell,
)
from .split_rules import base_from_renyokei, bases_from_renyokei

# Cells of the classical i-adjective paradigm that a kanji stem forms, paired
# with the closed set of function words each one hosts.  The reference
# dictionary carries none of them, so their surfaces fall back to unrelated
# verbs (かりき as かりきる) or to a lexicalized adverb (悪しからず).
_CLASSICAL_ADJECTIVE_CELLS: tuple[tuple[str, str, str], ...] = (
    (r"(?:し)?から", r"ず|む", "助動詞"),
    (r"(?:し)?かり", r"けり|き|し", "助動詞"),
    (r"(?:し)?けれ|(?:し)?かれ", r"ど", "助詞"),
)
_CLASSICAL_AUXILIARY_LEMMAS: dict[str, str] = {"ず": "ぬ", "し": "き"}


# Cells that select an irrealis, paired with the reading each one keeps once the
# ハ行四段 未然形 in front of it is restored. は is also the topic particle, so
# the row's irrealis is visible only where one of these follows it, and the
# reference dictionary carries the row for the few verbs it happens to list
# (思ふ) while reading the kana as the particle everywhere else.
_HA_ROW_IRREALIS_CELLS: dict[str, tuple[str, str]] = {
    "しむ": ("助動詞", "しむ"),
    "まし": ("助動詞", "まし"),
    "ず": ("助動詞", "ぬ"),
    "む": ("助動詞", "む"),
    "じ": ("助動詞", "じ"),
    "れ": ("助動詞", "れる"),
    "ば": ("助詞", "ば"),
    "く": ("動詞", "くる"),
}
KU_NOMINALIZER = "く"
_HA_ROW_IRREALIS_TAILS = "|".join(sorted(_HA_ROW_IRREALIS_CELLS, key=len, reverse=True))


def _ha_row_irrealis_cells(remaining: str) -> list[dict] | None:
    """Split the ハ行四段 未然形 from the cell that selects it (言|は|しむ).

    The row's terminal ふ is its headword, and the modern ワ行五段 spelling of the
    same verb is what the dictionary does carry, so asking it for stem + う tells
    a real irrealis from a nominal that happens to end in the topic particle.
    """
    match = regex.match(
        rf"^(\p{{Han}}+)(は)({_HA_ROW_IRREALIS_TAILS})(?=$|[^\p{{Hiragana}}])",
        remaining,
    )
    if match is None:
        return None
    run, cell, tail = match.groups()
    for offset in range(len(run)):
        stem = run[offset:]
        if not is_single_token_of_pos(stem + "う", "動詞"):
            continue
        host = run[:offset]
        host_token = [{"surface": host, "pos": "名詞", "lemma": host}] if host else []
        # ク語法 names the predicate rather than continuing it, so the cell and
        # the nominalizer are one nominal (言はく, 思はく) — which is how the
        # dictionary already reads the modern spelling of the same word.
        if tail == KU_NOMINALIZER:
            nominal = stem + cell + tail
            return [*host_token, {"surface": nominal, "pos": "名詞", "lemma": nominal}]
        tail_pos, tail_lemma = _HA_ROW_IRREALIS_CELLS[tail]
        return [
            *host_token,
            {"surface": stem + cell, "pos": "動詞", "lemma": stem + "ふ"},
            {"surface": tail, "pos": tail_pos, "lemma": tail_lemma},
        ]
    return None


def _classical_adjective_cells(remaining: str) -> list[dict] | None:
    """Split a classical i-adjective cell from the function word it hosts.

    The stem is the longest suffix of the leading kanji run that the reference
    dictionary conjugates, not the whole run: a subject noun stands in the same
    run as the adjective it heads (山|高かりけり, 波|高からず), and a stem with
    okurigana carries hiragana of its own (冷た|かり).  Taking the run whole
    builds a non-word out of the noun and leaves the cell to fall back on an
    unrelated verb.
    """
    for inflection, tails, tail_pos in _CLASSICAL_ADJECTIVE_CELLS:
        match = regex.match(
            rf"^(\p{{Han}}+\p{{Hiragana}}*?)({inflection})({tails})(?=$|[^\p{{Hiragana}}])",
            remaining,
        )
        if match is None:
            continue
        run, cell, tail = match.groups()
        for offset in range(len(run)):
            stem = run[offset:]
            if not regex.match(r"^\p{Han}", stem):
                break
            # Every cell of the paradigm shares the adjective's stem, so swapping the
            # matched ending for the 未然形 one gives the probe the dictionary knows.
            lemma = classical_adjective_lemma(stem + cell[: -len(KARI_MIZENKEI_CELL)] + KARI_MIZENKEI_CELL)
            if lemma is None:
                continue
            host = run[:offset]
            return [
                *([{"surface": host, "pos": "名詞", "lemma": host}] if host else []),
                {"surface": stem + cell, "pos": "形容詞", "lemma": lemma},
                {"surface": tail, "pos": tail_pos, "lemma": _CLASSICAL_AUXILIARY_LEMMAS.get(tail, tail)},
            ]
        return None
    return None


def _reads_as_one_verb(lemma: str) -> bool:
    """Whether the reference dictionary reads `lemma` as a single verb."""
    if not lemma:
        return False
    tokens = mecab_analyze(lemma)
    return len(tokens) == 1 and tokens[0].get("pos") == "動詞"


def _fixed_te_search_unit(surface: str) -> dict | None:
    """Return a closed lexical て-unit that must not be read as a te-form."""
    if not surface.endswith("て"):
        return None
    tokens = mecab_analyze(surface)
    if len(tokens) != 1 or tokens[0].get("pos") not in ("助詞", "副詞"):
        return None
    token = tokens[0]
    return {
        "surface": surface,
        "pos": token["pos"],
        "pos_sub1": token.get("pos_sub1"),
        "pos_sub2": token.get("pos_sub2"),
        "conj_type": token.get("conj_type"),
        "conj_form": token.get("conj_form"),
        "lemma": token.get("lemma") or surface,
    }


# Numeric-approximation/aggregation prefixes that modify a whole quantity and split
# off the following number+counter (約|二時間, 計|五名), unlike ordinal 第 which binds
# to its number (第三十四|回). Mirrors normalize::isNumericApproxPrefixKanji in the core.
_APPROX_NUMERIC_PREFIXES = {"約", "計", "総"}
_PRODUCTIVE_COMPOUND_V2 = frozenset(COMPOUND_VERB_V2_GODAN + COMPOUND_VERB_V2_ICHIDAN)
_PRODUCTIVE_ICHIDAN_COMPOUND_V2 = frozenset(COMPOUND_VERB_V2_ICHIDAN)
_NOMINALIZING_PARTICLES = frozenset({"を", "は", "が", "の", "に", "で", "へ", "と", "も"})
_KANA_NUMBER_COUNTERS = tuple(
    sorted((stem + suffix for stem in KANA_NUMBER_STEMS for suffix in KANA_COUNTER_SUFFIXES), key=len, reverse=True)
)
_FIXED_FUNCTION_SEARCH_UNITS = tuple(sorted(FIXED_FUNCTION_SEARCH_UNITS, key=len, reverse=True))
_FIXED_INFLECTED_FUNCTION_UNITS = tuple(sorted(FIXED_INFLECTED_FUNCTION_UNITS, key=len, reverse=True))
_KEYCAP_EMOJI = regex.compile(r"[0-9#*]\uFE0F?\u20E3")
# Characters a hashtag body may contain. A tag ends at whitespace, punctuation or any
# other symbol, so the body class is exactly "word text" in any script.
_HASHTAG_BODY_CLASS = r"[\p{Han}\p{Hiragana}\p{Katakana}\p{Latin}\p{Nd}_\u30FC\u3005]"
HASHTAG_BODY_CHAR = regex.compile(_HASHTAG_BODY_CLASS)
HASHTAG_BODY_RUN = regex.compile(_HASHTAG_BODY_CLASS + "+")
# A kana run carrying at least one ー that is not word-final: emphatic lengthening
# rather than a token boundary. A trailing ー is left to prolonged-sound-merge.
_KANA_PROLONGED_RUN = regex.compile(r"[\p{Hiragana}ー]+")
_MEDIAL_PROLONGED_RUN = regex.compile(r"\p{Hiragana}+ー+\p{Hiragana}+")


def _single_token_at(text: str, offset: int, surface: str) -> dict | None:
    """Read `text` and return the token starting at `offset` when it is exactly `surface`."""
    cursor = 0
    for token in mecab_analyze(text):
        if cursor == offset:
            return token if token.get("surface") == surface else None
        cursor += len(token.get("surface", ""))
        if cursor > offset:
            return None
    return None


def _opens_hashtag(text: str, pos: int) -> bool:
    """A marker opens a tag at a text boundary.

    Start of input, whitespace, punctuation, or the marker of a preceding tag
    (#東京#テスト) all qualify. Scanning back over body characters and running out of
    text means the marker sits inside an ordinary word (C#).
    """
    cursor = pos
    while cursor > 0:
        if not HASHTAG_BODY_CHAR.match(text[cursor - 1]):
            return True
        cursor -= 1
    return pos == 0


_PRETOKENIZED_QUANTITY = regex.compile(r"(?:\d{1,3}(?:,\d{3})+|\d+)円")
_PRETOKENIZED_COMMA_NUMBER = regex.compile(r"\d{1,3}(?:,\d{3})+")
_PRETOKENIZED_EMAIL = regex.compile(r"[A-Za-z0-9][A-Za-z0-9._+\-]*@[A-Za-z0-9\-]+(?:\.[A-Za-z0-9\-]+)+")
_DURATION_BEFORE_SPAN_KAN = regex.compile(
    r"[0-9０-９〇零一二三四五六七八九十百千万億兆数半]+(?:年|月|日|週|(?:ヶ|ケ|カ|ヵ|箇|か)月)$"
)
# A succession of these finite units is one time/date/ratio search unit
# (十時三十分, 二〇二五年三月, 三割五分), independent of the next token.
_COUNTER_CHAIN_TAILS = frozenset({"年", "月", "日", "週", "時", "分", "秒", "間", "泊", "割"})
_COUNTER_CHAIN_UNIT = regex.compile(r"[0-9０-９〇零一二三四五六七八九十百千万億兆]+[年月日時分秒間泊割]$")


def _heads_nidan_cell(tokens: list[dict], index: int) -> bool:
    """Whether the token at ``index`` is the stem of a classical 二段 finite cell."""
    following = tokens[index + 1] if index + 1 < len(tokens) else None
    return nidan_cell(tokens[index], following) is not None


def _kanji_noun_run(tokens: list[dict], start: int) -> tuple[int, str]:
    """Return the complete mergeable kanji-noun run beginning at ``start``."""
    if start >= len(tokens):
        return start, ""
    token = tokens[start]
    if not (
        regex.match(r"^[\p{Han}]+$", token.get("surface", ""))
        and token.get("pos") == "名詞"
        and token.get("pos_sub1", "") not in ("接尾", "固有名詞", "副詞可能")
    ):
        return start, ""
    # A kanji the dictionary reads as a bare noun is the stem of a classical 二段
    # verb when the kana after it completes a finite cell (老|ゆる, 絶|ゆれ).
    # Absorbing it into a compound would bury a verb inside a noun that is not a
    # word, so the run stops before such a stem.
    if _heads_nidan_cell(tokens, start):
        return start, ""

    index = start + 1
    combined = token.get("surface", "")
    while index < len(tokens):
        following = tokens[index]
        surface = following.get("surface", "")
        is_mergeable = (
            regex.match(r"^[\p{Han}]+$", surface)
            and following.get("pos") == "名詞"
            and following.get("pos_sub1", "") not in ("接尾", "固有名詞", "形容動詞語幹", "副詞可能", "数")
        )
        if not is_mergeable or _heads_nidan_cell(tokens, index):
            break
        combined += surface
        index += 1

    if index < len(tokens) and tokens[index].get("surface", "") in ("付け", "者", "人"):
        combined += tokens[index].get("surface", "")
        index += 1
    return index, combined


def _denominal_ru_form(tokens: list[dict], index: int) -> str | None:
    """Return the productive denominal-る surface starting at ``index``.

    A reference dictionary sometimes treats the final inflection of a noun-
    derived Godan-る verb as an unrelated classical auxiliary or non-word verb.
    The token immediately before it is the nominal host, so the malformed tail
    itself—not a vocabulary list of derived verbs—identifies the boundary.
    """
    tail = tokens[index]
    surface = tail.get("surface", "")
    conj_type = tail.get("conj_type", "")
    if tail.get("pos") == "助動詞" and (
        (surface in ("る", "れ") and conj_type == "文語・ル") or (surface == "り" and conj_type == "文語・リ")
    ):
        return surface
    # Some reference-dictionary paths first retag the stranded terminal る
    # as a nominal suffix. It still has no nominal host here: immediately
    # after an ordinary noun it is the productive denominal verb ending.
    if surface == "る" and tail.get("pos") != "助詞":
        return surface
    if (
        surface == "っ"
        and tail.get("pos") == "動詞"
        and tail.get("pos_sub1") == "非自立"
        and tail.get("lemma") == "く"
        and index + 1 < len(tokens)
        and tokens[index + 1].get("surface") == "た"
    ):
        return surface
    if (
        surface == "ら"
        and tail.get("pos") == "名詞"
        and tail.get("pos_sub1") == "接尾"
        and index + 1 < len(tokens)
        and tokens[index + 1].get("surface") == "ない"
    ):
        return surface
    if (
        regex.fullmatch(r"[\p{Katakana}ー]+っ", surface)
        and tail.get("pos") == "動詞"
        and tail.get("conj_type") == "五段・ラ行"
        and tail.get("lemma", "").endswith("る")
    ):
        return surface
    return None


def _separate_counter_case_particles(tokens: list[dict]) -> list[dict]:
    """Expose a case particle lexicalized onto a counter token."""
    separated: list[dict] = []
    for index, token in enumerate(tokens):
        surface = token.get("surface", "")
        is_counter = (
            token.get("pos") == "名詞" and token.get("pos_sub1") == "接尾" and token.get("pos_sub2") == "助数詞"
        )
        following_is_numeral = (
            index + 1 < len(tokens)
            and tokens[index + 1].get("pos") == "名詞"
            and tokens[index + 1].get("pos_sub1") == "数"
        )
        if not is_counter or not surface.endswith("の") or len(surface) == 1 or following_is_numeral:
            separated.append(token)
            continue
        counter = token.copy()
        counter["surface"] = surface[:-1]
        if counter.get("lemma") == surface:
            counter["lemma"] = surface[:-1]
        separated.extend((counter, {"surface": "の", "pos": "助詞", "pos_sub1": "格助詞", "lemma": "の"}))
    return separated


def apply_suzume_merge(tokens: list[dict], text: str) -> tuple[list[dict], str | None]:
    """Apply Suzume merge rules to MeCab tokens.

    Returns:
        Tuple of (merged tokens, applied rule name or None).
    """
    tokens = _separate_counter_case_particles(tokens)
    result: list[dict] = []
    i = 0
    applied_rule: str | None = None
    standalone_noun_indexes: set[int] = set()

    while i < len(tokens):
        t = tokens[i].copy()
        if i in standalone_noun_indexes:
            t["pos"] = "名詞"
            t["pos_sub1"] = "一般"
            t["pos_sub2"] = None
        merged = False

        # Calculate position in text
        pos_in_text = sum(len(tokens[k].get("surface", "")) for k in range(i))
        remaining = text[pos_in_text:] if pos_in_text < len(text) else ""

        # A reference headword can absorb topic は into a following unknown
        # kana noun (そこ + はにわ, ここ + はいり + ぐち).  L2 evidence for the
        # suffix noun restores the productive topic boundary, while requiring
        # a preceding nominal keeps a standalone lexical noun such as はにわ
        # intact.
        if (
            not merged
            and result
            and result[-1].get("pos") in ("名詞", "代名詞", "Noun", "Pronoun")
            and t.get("surface", "").startswith("は")
            and t.get("surface") != "は"
        ):
            for noun in core_headwords_by_length("nouns.tsv"):
                topic_noun = "は" + noun
                if not remaining.startswith(topic_noun):
                    continue
                consumed = ""
                j = i
                while j < len(tokens) and len(consumed) < len(topic_noun):
                    consumed += tokens[j].get("surface", "")
                    j += 1
                if consumed != topic_noun:
                    continue
                result.extend(
                    (
                        {"surface": "は", "pos": "助詞", "pos_sub1": "係助詞", "lemma": "は"},
                        {"surface": noun, "pos": "Noun", "lemma": noun},
                    )
                )
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "topic+l2-noun-boundary"
                break

        # After the attributive copula of a na-adjective, ものの is a closed
        # concessive particle when a predicate follows.  IPADIC happens to
        # split this host class into formal noun + genitive, unlike the same
        # connective after a verb.  A following predicate keeps a genuine
        # nominal genitive (静かなものの色) out of this rule.
        if (
            not merged
            and t.get("surface") == "もの"
            and t.get("pos") == "名詞"
            and i > 0
            and tokens[i - 1].get("surface") == "な"
            and tokens[i - 1].get("pos") == "助動詞"
            and i + 2 < len(tokens)
            and tokens[i + 1].get("surface") == "の"
            and tokens[i + 1].get("pos") == "助詞"
            and tokens[i + 2].get("pos") in ("動詞", "形容詞")
        ):
            result.append({"surface": "ものの", "pos": "助詞", "lemma": "ものの"})
            i += 2
            merged = True
            if applied_rule is None:
                applied_rule = "na-adjective-monono"

        # Preserve a classical kari adjective before generic noun recovery can
        # absorb its kanji stem. The classical terminal cell validates the open
        # adjective class and supplies its headword; the suffix/auxiliary cells
        # are grammatical.
        if not merged:
            cells = _classical_adjective_cells(remaining)
            if cells is not None:
                source_span = "".join(cell["surface"] for cell in cells)
                consumed = ""
                j = i
                while j < len(tokens) and len(consumed) < len(source_span):
                    consumed += tokens[j].get("surface", "")
                    j += 1
                if consumed == source_span:
                    result.extend(cells)
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = "classical-adjective-kari"

        # The ハ行四段 irrealis needs the same protection, and for the same
        # reason: its cell kana is the topic particle, so generic noun recovery
        # takes the stem in front of it as a word of its own (言|は|しむ).
        if not merged:
            cells = _ha_row_irrealis_cells(remaining)
            if cells is not None:
                source_span = "".join(cell["surface"] for cell in cells)
                consumed = ""
                j = i
                while j < len(tokens) and len(consumed) < len(source_span):
                    consumed += tokens[j].get("surface", "")
                    j += 1
                if consumed == source_span:
                    result.extend(cells)
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = "classical-ha-row-irrealis"

        # The parallel particle とか is a closed unit after a predicate or
        # copula.  The reference lattice can split its final occurrence into
        # quotative と plus focus か, despite retaining the same particle in
        # the preceding parallel member.
        if (
            not merged
            and t.get("surface") == "と"
            and t.get("pos") == "助詞"
            and i > 0
            and tokens[i - 1].get("pos") in ("動詞", "助動詞", "形容詞")
            and i + 1 < len(tokens)
            and tokens[i + 1].get("surface") == "か"
            and tokens[i + 1].get("pos") == "助詞"
        ):
            result.append({"surface": "とか", "pos": "助詞", "lemma": "とか"})
            i += 2
            merged = True
            if applied_rule is None:
                applied_rule = "parallel-toka"

        # Volitional う licenses concessive とも as one connective particle.
        # Without that inflectional environment, と + も remains a quotative
        # plus focus-particle sequence (行くとも思わない).
        if (
            not merged
            and t.get("surface") == "と"
            and t.get("pos") == "助詞"
            and i > 0
            and tokens[i - 1].get("surface") == "う"
            and tokens[i - 1].get("pos") == "助動詞"
            and i + 1 < len(tokens)
            and tokens[i + 1].get("surface") == "も"
            and tokens[i + 1].get("pos") == "助詞"
        ):
            result.append({"surface": "とも", "pos": "助詞", "lemma": "とも"})
            i += 2
            merged = True
            if applied_rule is None:
                applied_rule = "volitional-tomo"

        # A nominalizer (ん/の) followed by だって carries the same adverbial
        # particle as a nominal host (学生だって).  Punctuation makes IPADIC
        # choose the compositional copula + quotative lattice only here.
        if (
            not merged
            and t.get("surface") == "だ"
            and t.get("pos") == "助動詞"
            and i > 0
            and tokens[i - 1].get("surface") in ("ん", "の")
            and tokens[i - 1].get("pos") in ("名詞", "助詞", "Particle")
            and (tokens[i - 1].get("pos") == "名詞" or (i > 1 and tokens[i - 2].get("pos") == "動詞"))
            and i + 1 < len(tokens)
            and tokens[i + 1].get("surface") == "って"
            and tokens[i + 1].get("pos") == "助詞"
        ):
            result.append({"surface": "だって", "pos": "助詞", "lemma": "だって"})
            i += 2
            merged = True
            if applied_rule is None:
                applied_rule = "nominalizer-datte"

        # Native pre-tokenization sees the normalized ASCII punctuation before
        # analysis. Recover its open-pattern quantity and email units when
        # MeCab emitted their punctuation as separate records.
        if not merged:
            pretokenized_match = _KEYCAP_EMOJI.match(remaining)
            pretokenized_rule = "keycap-emoji"
            if pretokenized_match is None:
                pretokenized_match = _PRETOKENIZED_QUANTITY.match(remaining)
                pretokenized_rule = "number+unit"
            if pretokenized_match is None:
                pretokenized_match = _PRETOKENIZED_COMMA_NUMBER.match(remaining)
                pretokenized_rule = "comma-number"
            if pretokenized_match is None:
                pretokenized_match = _PRETOKENIZED_EMAIL.match(remaining)
                pretokenized_rule = "email"
            if pretokenized_match is not None:
                unit = pretokenized_match.group(0)
                consumed = ""
                j = i
                while j < len(tokens) and len(consumed) < len(unit):
                    consumed += tokens[j].get("surface", "")
                    j += 1
                if consumed == unit:
                    result.append({"surface": unit, "pos": "名詞", "lemma": unit})
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = pretokenized_rule

        # A comma-grouped numeral can be split inside the token holding its
        # counter (1 , 000人). Recover the numeral and expose a productive
        # counter boundary; currency remains the established combined unit.
        if not merged:
            comma_match = _PRETOKENIZED_COMMA_NUMBER.match(remaining)
            if comma_match is not None:
                number = comma_match.group(0)
                counter = next(
                    (unit for unit in COUNTER_UNITS if unit != "円" and remaining.startswith(number + unit)), ""
                )
                if counter:
                    source_span = number + counter
                    consumed = ""
                    j = i
                    while j < len(tokens) and len(consumed) < len(source_span):
                        consumed += tokens[j].get("surface", "")
                        j += 1
                    if consumed == source_span:
                        result.extend(
                            (
                                {"surface": number, "pos": "名詞", "pos_sub1": "数", "lemma": number},
                                {
                                    "surface": counter,
                                    "pos": "名詞",
                                    "pos_sub1": "接尾",
                                    "pos_sub2": "助数詞",
                                    "lemma": counter,
                                },
                            )
                        )
                        i = j
                        merged = True
                        if applied_rule is None:
                            applied_rule = "comma-number+counter"

        # A closed subsidiary inflection may be split into arbitrary pieces
        # by the reference dictionary (い+た+だけ+ませ).  Consume the exact
        # source span only when its next token is a licensed auxiliary follower,
        # before the generic V1+V2 compound rule can absorb the initial piece.
        if not merged:
            fixed_form = next((form for form in _FIXED_INFLECTED_FUNCTION_UNITS if remaining.startswith(form)), "")
            if fixed_form:
                consumed = ""
                j = i
                while j < len(tokens) and len(consumed) < len(fixed_form):
                    consumed += tokens[j].get("surface", "")
                    j += 1
                pos, lemma, followers = FIXED_INFLECTED_FUNCTION_UNITS[fixed_form]
                following = tokens[j].get("surface", "") if j < len(tokens) else ""
                if consumed == fixed_form and any(following.startswith(follower) for follower in followers):
                    result.append({"surface": fixed_form, "pos": pos, "lemma": lemma})
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = "fixed-inflected-function-unit"

        # Closed function words and formal nouns remain one search unit even
        # when the reference dictionary splits them into homographic pieces
        # (そん+なら, お+それ, が+てら). Consume an exact source-text span so the
        # rule never absorbs a partial token or crosses the fixed word's end.
        if not merged:
            fixed_word = next((word for word in _FIXED_FUNCTION_SEARCH_UNITS if remaining.startswith(word)), "")
            if fixed_word:
                consumed = ""
                j = i
                while j < len(tokens) and len(consumed) < len(fixed_word):
                    consumed += tokens[j].get("surface", "")
                    j += 1
                if consumed == fixed_word:
                    result.append(
                        {
                            "surface": fixed_word,
                            "pos": FIXED_FUNCTION_SEARCH_UNITS[fixed_word],
                            "lemma": FIXED_FUNCTION_LEMMAS.get(fixed_word, fixed_word),
                        }
                    )
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = "fixed-function-search-unit"

        # An L2 noun is lexical evidence that an otherwise ambiguous sequence
        # is one search unit. Recover only whole adjacent MeCab tokens: a
        # headword ending inside a token must not consume that token's suffix.
        if not merged:
            for noun in core_headwords_by_length("nouns.tsv"):
                if not remaining.startswith(noun):
                    continue
                consumed = ""
                j = i
                while j < len(tokens) and len(consumed) < len(noun):
                    consumed += tokens[j].get("surface", "")
                    j += 1
                if consumed != noun or j == i + 1:
                    continue
                starts_as_closed_class = t.get("pos") in ("助詞", "助動詞", "連体詞")
                corrected_two_mora_noun = (
                    j == i + 2 and tokens[i + 1].get("pos") == "Particle" and tokens[i + 1].get("pos_sub1") == "一般"
                )
                if starts_as_closed_class and j < i + 3 and not corrected_two_mora_noun:
                    continue
                follows_verb_as_classical_ha_row = (
                    bool(result)
                    and result[-1].get("pos") in ("動詞", "Verb")
                    and j < len(tokens)
                    and tokens[j].get("surface") in ("ひ", "ふ", "へ")
                )
                if follows_verb_as_classical_ha_row:
                    continue
                # A headword spelled like the volitional-hosting irrealis plus
                # the auxiliary it selects is not evidence that the span is one
                # search unit. That cell has its own label because it exists for
                # nothing else, so the analyzer assigns it only where the
                # inflection is real (向こ + う in 顔を向こうとした, against the
                # noun 向こう in 塀の向こう側), and merging there would bury an
                # inflectional boundary it had already found. The plain irrealis
                # is not gated: it is also what an ordinary noun's first mora
                # gets misread as, which is the case this recovery exists for
                # (みず read as 見る + ず).
                covers_volitional_irrealis_chain = (
                    t.get("pos") == "動詞"
                    and (t.get("conj_form") or "") == "未然ウ接続"
                    and all(tokens[k].get("pos") == "助動詞" for k in range(i + 1, j))
                )
                if covers_volitional_irrealis_chain:
                    continue
                # Use the canonical POS label as a boundary marker. The raw
                # kanji and classical-stem recovery passes operate on MeCab's
                # Japanese POS labels and must not absorb a dictionary-backed
                # search unit into its neighbor.
                result.append({"surface": noun, "pos": "Noun", "lemma": noun})
                if j < len(tokens) and tokens[j].get("pos_sub1") == "接尾" and tokens[j].get("pos_sub2") != "助数詞":
                    follower_surface = tokens[j].get("surface", "")
                    follower_analysis = mecab_analyze(follower_surface)
                    if (
                        len(follower_analysis) == 1
                        and follower_analysis[0].get("pos") == "名詞"
                        and follower_analysis[0].get("pos_sub1") != "接尾"
                    ):
                        standalone_noun_indexes.add(j)
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "l2-noun"
                break

        # 0. Kana number + counter.  Raw MeCab can split these closed quantity
        # readings at arbitrary syllables (い|ちまい, よ|ん|に|ん), so consume
        # exactly one finite L1 composition by source-text length.
        if not merged:
            kana_quantity = next((quantity for quantity in _KANA_NUMBER_COUNTERS if remaining.startswith(quantity)), "")
            if kana_quantity:
                consumed = ""
                j = i
                while j < len(tokens) and len(consumed) < len(kana_quantity):
                    consumed += tokens[j].get("surface", "")
                    j += 1
                if consumed == kana_quantity:
                    result.append({"surface": kana_quantity, "pos": "名詞", "pos_sub1": "数", "lemma": kana_quantity})
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = "kana-number+unit"

        # A classical ha-row irrealis immediately selected by a negative
        # auxiliary is a verb cell, not the topic particle (言は+ざる,
        # 言は+ず).  Reconstruct the historical terminal ふ from the productive
        # paradigm; ordinary noun+は clauses have no such auxiliary follower.
        if not merged and regex.fullmatch(r"\p{Han}+", t.get("surface", "")) and i + 1 < len(tokens):
            nxt = tokens[i + 1]
            following = tokens[i + 2] if i + 2 < len(tokens) else None
            negative = following is not None and following.get("surface") in ("ず", "ざる", "ぬ", "ね")
            if nxt.get("surface") == "は" and negative:
                stem = t["surface"]
                auxiliary_surface = following["surface"]
                result.extend(
                    (
                        {"surface": stem + "は", "pos": "動詞", "lemma": stem + "ふ"},
                        {"surface": auxiliary_surface, "pos": "助動詞", "lemma": "ぬ"},
                    )
                )
                i += 3
                merged = True
                if applied_rule is None:
                    applied_rule = "classical-ha-row-negative"

            elif nxt.get("surface") == "はず":
                stem = t["surface"]
                result.extend(
                    (
                        {"surface": stem + "は", "pos": "動詞", "lemma": stem + "ふ"},
                        {"surface": "ず", "pos": "助動詞", "lemma": "ぬ"},
                    )
                )
                i += 2
                merged = True
                if applied_rule is None:
                    applied_rule = "classical-ha-row-negative"

        # The directional particle cannot host a past, perfective, or negative
        # auxiliary. In that environment a Han stem plus へ is the historical
        # lower-bigrade continuative (終へ+た, 終へ+ぬ), not a case phrase.
        if not merged and regex.fullmatch(r"\p{Han}+", t.get("surface", "")) and i + 2 < len(tokens):
            nxt = tokens[i + 1]
            following = tokens[i + 2]
            if nxt.get("surface") == "へ" and following.get("surface") in ("た", "て", "ぬ", "ず", "ざる"):
                stem = t["surface"]
                result.extend(
                    (
                        {"surface": stem + "へ", "pos": "動詞", "lemma": stem + "ふ"},
                        {
                            "surface": following["surface"],
                            "pos": "助動詞",
                            "lemma": "ぬ" if following["surface"] in ("ぬ", "ず", "ざる") else following["surface"],
                        },
                    )
                )
                i += 3
                merged = True
                if applied_rule is None:
                    applied_rule = "classical-he-auxiliary"

        # Dialectal どえ- intensifiers remain one modifier.  Require the
        # emphatic prefix and a following content word, rather than changing a
        # bare えりゃー verb by surface alone.
        if not merged and regex.match(r"^どえ(?:らい|りゃー)", remaining):
            intensifier = "どえりゃー" if remaining.startswith("どえりゃー") else "どえらい"
            consumed = ""
            j = i
            while j < len(tokens) and len(consumed) < len(intensifier):
                consumed += tokens[j].get("surface", "")
                j += 1
            if consumed == intensifier and j < len(tokens) and tokens[j].get("pos") not in ("助詞", "記号"):
                result.append({"surface": intensifier, "pos": "副詞", "lemma": intensifier})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "dialectal-doe-intensifier"

        # 1. Full date pattern
        if not merged:
            m = regex.match(r"^(\d+年\d+月\d+日)", remaining)
            if m:
                date = m.group(1)
                length = 0
                j = i
                while j < len(tokens) and length < len(date):
                    length += len(tokens[j].get("surface", ""))
                    j += 1
                if length == len(date):
                    result.append({"surface": date, "pos": "名詞", "lemma": date})
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = "date"

        # 1.3. お + family/honorific terms
        if not merged and t.get("surface") == "お" and "接頭詞" in t.get("pos", "") and i + 1 < len(tokens):
            next_surface = tokens[i + 1].get("surface", "")
            if next_surface in FAMILY_TERMS:
                combined = "お" + next_surface
                result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                i += 2
                merged = True
                if applied_rule is None:
                    applied_rule = "family-merge"

        # 1.4. Fixed temporal adverb split by the reference analyzer.
        if not merged and t.get("surface") == "かね" and i + 1 < len(tokens):
            if tokens[i + 1].get("surface") == "て":
                result.append({"surface": "かねて", "pos": "副詞", "lemma": "かねて"})
                i += 2
                merged = True
                if applied_rule is None:
                    applied_rule = "kanete-merge"

        if not merged and t.get("surface") == "より" and result:
            if result[-1].get("surface") == "かねて":
                result.append({"surface": "より", "pos": "助詞", "lemma": "より"})
                i += 1
                merged = True

        # 1.5. URL pattern
        if not merged:
            m = regex.match(r"^(https?://[a-zA-Z0-9\-._~:/?#\[\]@!$&'()*+,;=%]+)", remaining)
            if m:
                url = m.group(1)
                url = regex.sub(r"[.,)\]']+$", "", url)
                length = 0
                j = i
                while j < len(tokens) and length < len(url):
                    length += len(tokens[j].get("surface", ""))
                    j += 1
                if length == len(url):
                    result.append({"surface": url, "pos": "名詞", "lemma": url})
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = "url"

        # 1c. Mixed-script reduplication (一つひとつ, 一人ひとり). Writing the same
        # word twice in two scripts is how the distributive adverbial is spelled,
        # and the two halves are one search unit. The reference analyzer merges
        # only the pairs its lexicon happens to list, which is why 一つひとつ確認する
        # comes back whole while 一つひとつ調べる comes back split. Matching on the
        # reading is what makes the pattern general; requiring the surfaces to
        # differ is what keeps an identical repetition (二つ二つに分ける) out, where
        # two separate quantities are a live reading. A reduplication written
        # entirely in kanji (一人一人, 一件一件) is already covered by the
        # number+counter rule below.
        if not merged and t.get("pos") == "名詞" and i + 1 < len(tokens):
            nxt = tokens[i + 1]
            reading = t.get("reading", "")
            surface = t.get("surface", "")
            if reading and nxt.get("pos") == "名詞" and nxt.get("reading") == reading and nxt.get("surface") != surface:
                combined = surface + nxt.get("surface", "")
                result.append({"surface": combined, "pos": "名詞", "pos_sub1": "数", "lemma": combined})
                i += 2
                merged = True
                if applied_rule is None:
                    applied_rule = "mixed-script-reduplication"

        # A duration expression followed by the suffix 間 is one search unit.
        # The reference dictionary splits lexical heads such as 半年 and 半月
        # before 間, while numeric heads happen to arrive through the number+
        # counter path below.  Key on the productive quantity+duration shape,
        # rather than either dictionary headword, so both forms agree.
        if not merged and i + 1 < len(tokens):
            nxt = tokens[i + 1]
            if (
                t.get("pos") == "名詞"
                and _DURATION_BEFORE_SPAN_KAN.fullmatch(t.get("surface", ""))
                and nxt.get("surface") == "間"
                and nxt.get("pos") == "名詞"
                and nxt.get("pos_sub1") == "接尾"
            ):
                combined = t["surface"] + nxt["surface"]
                result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                i += 2
                merged = True
                if applied_rule is None:
                    applied_rule = "duration+span-kan"

        # A noun-derived Godan-る verb is one lexical predicate.  Rejoin a
        # malformed reference tail (事故+る, 事故+っ+た, ミ+スっ+た) by its
        # inflectional evidence; the rule also covers productive hosts that
        # are absent from the reference lexicon.
        if (
            not merged
            and t.get("pos") == "名詞"
            and t.get("pos_sub1") not in ("接尾", "代名詞")
            and i + 1 < len(tokens)
        ):
            tail = _denominal_ru_form(tokens, i + 1)
            if tail is not None:
                combined = t.get("surface", "") + tail
                lemma = combined[:-1] + "る" if tail.endswith(("っ", "ら", "り", "れ")) else combined
                result.append({"surface": combined, "pos": "動詞", "lemma": lemma})
                i += 2
                merged = True
                if applied_rule is None:
                    applied_rule = "denominal-ru-verb"

        # 2. Number + counter/katakana
        if not merged and t.get("pos") == "名詞" and t.get("pos_sub1") == "数":
            j = i + 1
            combined = t.get("surface", "")
            while j < len(tokens):
                nxt = tokens[j]
                ns = nxt.get("surface", "")
                np = nxt.get("pos", "")
                ns1 = nxt.get("pos_sub1", "")
                ns2 = nxt.get("pos_sub2", "")

                counter_continues_fraction = (
                    ns.endswith("の")
                    and j + 1 < len(tokens)
                    and tokens[j + 1].get("pos") == "名詞"
                    and tokens[j + 1].get("pos_sub1") == "数"
                )
                is_counter = (
                    np == "名詞"
                    and ns1 == "接尾"
                    and ns2 == "助数詞"
                    and (not ns.endswith("の") or counter_continues_fraction)
                )
                # The span marker 間 (名詞/接尾/一般) closes any duration quantity
                # (三ヶ月+間 → 三ヶ月間).  Non-numeric dictionary heads are handled by
                # the duration+span-kan rule immediately above.
                is_span_kan = (
                    ns == "間" and np == "名詞" and ns1 == "接尾" and _DURATION_BEFORE_SPAN_KAN.search(combined)
                )
                is_calendar_month = ns == "月" and regex.match(r"^(?:1[0-2]|[1-9])$", combined)
                is_katakana_noun = np == "名詞" and regex.match(r"^[\u30A0-\u30FF]+$", ns)
                is_chuu_suffix = ns == "中" and np == "名詞" and ns1 == "接尾"
                is_me_suffix = ns == "目" and np == "名詞" and ns1 == "接尾"
                is_large_unit = np == "名詞" and ns1 == "数" and ns in ("万", "億", "兆")
                is_number_after_large = combined.endswith(("万", "億", "兆")) and np == "名詞" and ns1 == "数"
                is_number_after_counter_chain = combined[-1:] in _COUNTER_CHAIN_TAILS and np == "名詞" and ns1 == "数"
                # IPADIC also emits calendar pieces such as 三月 as one
                # non-numeric token. It is still the next numeral+counter
                # member of a chain whose left member has already been read.
                is_compact_counter_chain_unit = (
                    combined[-1:] in _COUNTER_CHAIN_TAILS and _COUNTER_CHAIN_UNIT.fullmatch(ns) is not None
                )
                is_number_after_decimal = combined.endswith(".") and np == "名詞" and ns1 == "数"
                is_counter_aux = ns == "つ" and np in ("助動詞", "動詞")
                is_percent = ns == "%"
                is_decimal = ns == "."
                is_consecutive_number = np == "名詞" and ns1 == "数" and regex.match(r"^[0-9０-９]+$", ns)
                is_kanji_number_run = (
                    np == "名詞" and ns1 == "数" and regex.match(r"^[一二三四五六七八九十百千万億兆〇零]+$", ns)
                )
                is_alpha_unit = regex.match(r"^[A-Za-z]+$", ns) and np == "名詞"

                if any(
                    [
                        is_counter,
                        is_span_kan,
                        is_calendar_month,
                        is_katakana_noun,
                        is_chuu_suffix,
                        is_me_suffix,
                        is_large_unit,
                        is_number_after_large,
                        is_number_after_counter_chain,
                        is_compact_counter_chain_unit,
                        is_number_after_decimal,
                        is_counter_aux,
                        is_percent,
                        is_decimal,
                        is_alpha_unit,
                        is_consecutive_number,
                        is_kanji_number_run,
                    ]
                ):
                    combined += ns
                    j += 1
                    if any([is_katakana_noun, is_chuu_suffix, is_me_suffix, is_counter_aux, is_percent, is_alpha_unit]):
                        break
                else:
                    break
            if j > i + 1:
                result.append({"surface": combined, "pos": "名詞", "pos_sub1": "数", "lemma": combined})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "number+unit"

        # 2a2. Address number pattern
        if not merged and t.get("pos") == "名詞" and t.get("pos_sub1") == "数":
            j = i + 1
            combined = t.get("surface", "")
            has_hyphen = False
            while j + 1 < len(tokens):
                hyphen = tokens[j]
                next_num = tokens[j + 1]
                if hyphen.get("surface") == "-" and next_num.get("pos") == "名詞" and next_num.get("pos_sub1") == "数":
                    combined += "-" + next_num.get("surface", "")
                    j += 2
                    has_hyphen = True
                else:
                    break
            if has_hyphen:
                result.append({"surface": combined, "pos": "名詞", "pos_sub1": "数", "lemma": combined})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "address-number"

        # 2b. Prefix + number (第一, 第二, etc.)
        # Only merge numbers, not counters — 第一+毛 should stay split
        # An approximation prefix (約/計/総) modifies the whole quantity and splits off
        # the number+counter (約|二時間, 計|五名), unlike an ordinal prefix (第) that binds
        # to its number (第三十四|回). Skip approximation prefixes here so the number binds
        # right to its counter via the number+counter rule.
        if (
            not merged
            and t.get("pos") == "接頭詞"
            and t.get("pos_sub1") == "数接続"
            and t.get("surface", "") not in _APPROX_NUMERIC_PREFIXES
        ):
            j = i + 1
            combined = t.get("surface", "")
            while j < len(tokens):
                nxt = tokens[j]
                is_number = nxt.get("pos") == "名詞" and nxt.get("pos_sub1") == "数"
                if is_number:
                    combined += nxt.get("surface", "")
                    j += 1
                else:
                    break
            if j > i + 1:
                result.append({"surface": combined, "pos": "名詞", "pos_sub1": "数", "lemma": combined})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "number+unit"

        # 2c. Noun + 書/誌 suffix
        if not merged and t.get("pos") == "名詞" and i + 1 < len(tokens):
            nxt = tokens[i + 1]
            if nxt.get("surface", "") in ("書", "誌") and nxt.get("pos") == "名詞" and nxt.get("pos_sub1") == "接尾":
                combined = t.get("surface", "") + nxt["surface"]
                result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                i += 2
                merged = True
                if applied_rule is None:
                    applied_rule = "noun+suffix-char"

        # 2c2. Noun + productive search-unit suffix
        if not merged and t.get("pos") == "名詞" and i + 1 < len(tokens):
            nxt = tokens[i + 1]
            if (
                nxt.get("surface", "") in ("時", "率", "性", "長")
                and nxt.get("pos") == "名詞"
                and nxt.get("pos_sub1") == "接尾"
            ):
                combined = t.get("surface", "") + nxt["surface"]
                result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                i += 2
                merged = True
                if applied_rule is None:
                    applied_rule = "noun+suffix"

        # 2c3. Version number
        if not merged and t.get("surface", "") in ("v", "V") and i + 1 < len(tokens):
            j = i + 1
            combined = t["surface"]
            while j < len(tokens):
                ns = tokens[j].get("surface", "")
                if regex.match(r"^\d+$", ns) or ns == ".":
                    combined += ns
                    j += 1
                else:
                    break
            if j > i + 1:
                result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "version"

        # 2c4. Brand + number
        if not merged and regex.match(r"^[A-Za-z]+$", t.get("surface", "")) and t.get("pos") == "名詞":
            if i + 1 < len(tokens):
                nxt = tokens[i + 1]
                ns = nxt.get("surface", "")
                if regex.match(r"^\d+$", ns) and nxt.get("pos") == "名詞":
                    combined = t["surface"] + ns
                    result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                    i += 2
                    merged = True
                    if applied_rule is None:
                        applied_rule = "brand+number"

        # 2d. Prefix + Noun (kanji only)
        # Suzume design: 御 is a productive prefix that always splits off
        # (御 + 尽力, 御 + 挨拶, 御 + 協力). Skip merge for 御 prefix.
        if (
            not merged
            and t.get("pos") == "接頭詞"
            and t.get("pos_sub1") == "名詞接続"
            and t.get("surface", "") != "御"
            and i + 1 < len(tokens)
        ):
            nxt = tokens[i + 1]
            # A na-adjective stem heads a predicate rather than joining a
            # compound, so the prefix stays a separate modifier there (超|簡単,
            # 超|重要) while a plain noun host still yields one search unit
            # (超高速, 超大型).
            noun_end, noun_surface = _kanji_noun_run(tokens, i + 1)
            if noun_surface and nxt.get("pos_sub1") != "形容動詞語幹":
                combined = t.get("surface", "") + noun_surface
                # A temporal prefix heads a temporal noun, so only a temporal unit
                # continues it (今週, 今度, 毎時). Before an ordinary noun the prefix
                # is the adverbial 今 and the noun is its own word (今|紙, 今|水).
                temporal_break = (
                    t.get("surface", "") in TEMPORAL_PREFIX_KANJI
                    and nxt.get("surface", "")[:1] not in TEMPORAL_COMPOUND_UNITS
                )
                if not temporal_break and regex.match(r"^[\p{Han}]+$", combined):
                    result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                    i = noun_end
                    merged = True
                    if applied_rule is None:
                        applied_rule = "prefix+noun"

        # 3. Nai-adjective merge
        if not merged:
            for adj in NAI_ADJECTIVES:
                if remaining.startswith(adj):
                    length = 0
                    j = i
                    while j < len(tokens) and length < len(adj):
                        length += len(tokens[j].get("surface", ""))
                        j += 1
                    if length == len(adj):
                        result.append({"surface": adj, "pos": "形容詞", "lemma": adj})
                        i = j
                        merged = True
                        if applied_rule is None:
                            applied_rule = "nai-adjective"
                        break

        # 4. Elongated adjective
        if not merged and t.get("pos") == "形容詞" and t.get("conj_form") == "ガル接続":
            j = i + 1
            if j < len(tokens) and tokens[j].get("surface") == "ー":
                combined = t.get("surface", "") + "ー"
                lemma = t.get("lemma") or (t.get("surface", "") + "い")
                j += 1
                if j < len(tokens):
                    ns = tokens[j].get("surface", "")
                    if ns == "い":
                        combined += "い"
                        j += 1
                    elif regex.match(r"^い(ね|よ|な|わ|ぞ|さ|か|の|けど)$", ns):
                        particle = ns[1:]
                        combined += "い"
                        result.append({"surface": combined, "pos": "形容詞", "lemma": lemma})
                        result.append({"surface": particle, "pos": "助詞", "lemma": particle})
                        i = j + 1
                        merged = True
                        if applied_rule is None:
                            applied_rule = "elongated-adjective"
                if not merged:
                    result.append({"surface": combined, "pos": "形容詞", "lemma": lemma})
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = "elongated-adjective"

        # The volitional う closes a predicate, so a case particle cannot attach
        # to it -- a case particle needs a nominal host.  A reference dictionary
        # that lacks the kana spelling of a nominal reads its tail as the
        # volitional of a homographic verb (むこうへ as 向く + う), which leaves
        # the particle with nothing to govern.  The quotative と takes a clause
        # rather than a nominal, and 意志形 + に + も is the concessive frame, so
        # both keep the auxiliary boundary.
        if not merged and t.get("pos") == "動詞" and i + 2 < len(tokens):
            volitional = tokens[i + 1]
            governing = tokens[i + 2]
            follower = tokens[i + 3] if i + 3 < len(tokens) else None
            if (
                volitional.get("pos") == "助動詞"
                and volitional.get("lemma") == "う"
                and governing.get("pos") == "助詞"
                and governing.get("pos_sub1") == "格助詞"
                and governing.get("pos_sub2") == "一般"
                and governing.get("surface") != "と"
                and not (follower is not None and follower.get("pos_sub1") == "係助詞")
            ):
                nominal = t.get("surface", "") + "う"
                result.append({"surface": nominal, "pos": "名詞", "pos_sub1": "一般", "lemma": nominal})
                i += 2
                merged = True
                if applied_rule is None:
                    applied_rule = "volitional-before-case-particle"

        # 4b. Vowel repetition: verb + repeated う (2+)
        if not merged and t.get("pos") == "動詞":
            j = i + 1
            combined = t.get("surface", "")
            lemma = t.get("lemma") or t.get("surface", "")
            u_count = 0
            while j < len(tokens):
                nxt = tokens[j]
                if nxt.get("surface") == "う" and nxt.get("pos") == "助動詞":
                    combined += "う"
                    u_count += 1
                    j += 1
                else:
                    break
            if u_count >= 2:
                result.append({"surface": combined, "pos": "動詞", "lemma": lemma})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "vowel-repeat"

        # 4c. Emphatic sokuon in past tense
        if not merged and t.get("pos") == "動詞" and "連用" in (t.get("conj_form") or ""):
            j = i + 1
            if j < len(tokens) and tokens[j].get("surface") == "たっ":
                combined = t.get("surface", "") + "たっ"
                lemma = t.get("lemma") or t.get("surface", "")
                result.append({"surface": combined, "pos": "動詞", "lemma": lemma})
                i = j + 1
                merged = True
                if applied_rule is None:
                    applied_rule = "emphatic-sokuon"

        # 4c-2. The colloquial negative elides the ら of a ra-row godan verb
        # (帰らない -> 帰んない, やらない -> やんない). The reference analyzer has
        # its own cell for it and assigns it correctly to 帰ん, やん, 座ん and
        # 分かん, but loses the reading for a bare-hiragana stem and reads the ん
        # as the standalone negative auxiliary instead, leaving a lemma the
        # sentence never contained (わか/わく + ん for わかんない). The following
        # ない is what rules that reading out: the negative ん is itself a
        # sentence-final form (わからん), so nothing negates it a second time.
        # The base is the ra-row verb the elided mora belongs to, which is the
        # stem the analyzer kept plus る.
        if (
            not merged
            and t.get("pos") == "動詞"
            and (t.get("conj_type") or "").startswith("五段")
            and "未然" in (t.get("conj_form") or "")
            and i + 2 < len(tokens)
            and tokens[i + 1].get("surface") == "ん"
            and tokens[i + 1].get("pos") == "助動詞"
            and (tokens[i + 2].get("lemma") or "") == "ない"
        ):
            stem = t.get("surface", "")
            result.append({"surface": stem + "ん", "pos": "動詞", "lemma": stem + "る"})
            # The negative is the auxiliary here, which is how the analyzer tags
            # it wherever it recognizes the contraction itself. Reached from the
            # standalone ん it comes back as the adjective instead, and leaving
            # that in place would tag one construction two ways.
            result.append({"surface": tokens[i + 2].get("surface", ""), "pos": "助動詞", "lemma": "ない"})
            i += 3
            merged = True
            if applied_rule is None:
                applied_rule = "ra-row-negative-contraction"

        # 4d. Adjective vowel repetition
        if not merged and t.get("pos") == "形容詞" and t.get("surface", "").endswith("い"):
            j = i + 1
            if j < len(tokens):
                nxt = tokens[j]
                if nxt.get("surface") == "いい" and nxt.get("pos") == "形容詞":
                    combined = t.get("surface", "") + "いい"
                    lemma = t.get("lemma") or t.get("surface", "")
                    result.append({"surface": combined, "pos": "形容詞", "lemma": lemma})
                    i = j + 1
                    merged = True
                    if applied_rule is None:
                        applied_rule = "vowel-repeat"

        # 4d-1. The completive auxiliary しまう contracts after a te-form:
        # 読んで+もうた and 読んで+しもうた. MeCab can split the closed
        # auxiliary across arbitrary token boundaries (も/うた, し/もう), so
        # recover it from the source span rather than a particular raw analysis.
        if not merged and result and result[-1].get("surface") in ("て", "で"):
            contracted = ""
            tail = ""
            if remaining.startswith("しもうた"):
                contracted, tail = "しもう", "た"
            elif remaining.startswith("もうた"):
                contracted, tail = "もう", "た"
            elif remaining.startswith("しもう"):
                contracted = "しもう"
            elif remaining.startswith("もう"):
                contracted = "もう"
            if contracted:
                source_span = contracted + tail
                consumed = 0
                j = i
                while j < len(tokens) and consumed < len(source_span):
                    consumed += len(tokens[j].get("surface", ""))
                    j += 1
                if consumed == len(source_span):
                    result.append({"surface": contracted, "pos": "助動詞", "lemma": "しまう"})
                    if tail:
                        result.append({"surface": tail, "pos": "助動詞", "lemma": tail})
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = "contracted-shimau"

        # 4d-2. Nominal host + productive adjective suffix
        # くさい derives an adjective from its host instead of predicating over
        # a separate preceding word, so the two form one search unit. Only a
        # free nominal can be a host: after a particle, an adverb or a
        # determiner the adjective is the predicate and keeps its own token
        # (この魚は|くさい, ちょっと|くさい, その|くさい匂い).
        if (
            not merged
            and t.get("pos") == "名詞"
            and t.get("pos_sub1") not in ("非自立", "代名詞")
            and i + 1 < len(tokens)
        ):
            nxt = tokens[i + 1]
            if nxt.get("pos") == "形容詞" and nxt.get("lemma") in DERIVED_ADJECTIVE_SUFFIX_LEMMAS:
                host = t.get("surface", "")
                result.append(
                    {
                        "surface": host + nxt.get("surface", ""),
                        "pos": "形容詞",
                        "lemma": host + (nxt.get("lemma") or ""),
                    }
                )
                i += 2
                merged = True
                if applied_rule is None:
                    applied_rule = "nominal+derived-adjective"

        # 4e. Emphatic lengthening inside a word
        # A ー between two kana is emphasis, not a boundary, but the reference analyzer
        # breaks at it and invents readings for the pieces (ひどーい → ひ/どー/い).
        # Strip the marks and re-read: when the plain form is one word, the lengthened
        # surface is that word. This must precede the trailing-ー merge below, which
        # would otherwise close the token at the mark and hide the medial case.
        if not merged and _MEDIAL_PROLONGED_RUN.match(remaining):
            run = _KANA_PROLONGED_RUN.match(remaining).group(0)
            end = len(run)
            while end > 0:
                candidate = run[:end]
                if _MEDIAL_PROLONGED_RUN.fullmatch(candidate):
                    plain = candidate.replace("ー", "")
                    # Re-read in place, not in isolation: a bare たい is a noun to the
                    # analyzer, an auxiliary after a continuative verb.
                    base = _single_token_at(
                        text[:pos_in_text] + plain + text[pos_in_text + len(candidate) :],
                        pos_in_text,
                        plain,
                    )
                    if base is not None:
                        consumed = 0
                        j = i
                        while j < len(tokens) and consumed < len(candidate):
                            consumed += len(tokens[j].get("surface", ""))
                            j += 1
                        if consumed == len(candidate):
                            result.append(
                                {
                                    "surface": candidate,
                                    "pos": base.get("pos", ""),
                                    "lemma": base.get("lemma") or plain,
                                }
                            )
                            i = j
                            merged = True
                            if applied_rule is None:
                                applied_rule = "emphatic-lengthening"
                        break
                end -= 1

        # 4f. Prolonged sound mark (ー) merge
        # Merge a trailing ー with the preceding token. Every mark is kept, because
        # dropping the repeats would leave the token sequence no longer covering the
        # input (うれしーーー). The one exception mirrors the tokenizer's own
        # normalization: repeated marks directly before a kanji are separator-like
        # elongation and collapse to a single mark (長いーー音 → 長いー音).
        if not merged and i + 1 < len(tokens):
            next_surface = tokens[i + 1].get("surface", "")
            if regex.match(r"^ー+$", next_surface):
                marks = next_surface
                j = i + 2
                while j < len(tokens) and regex.match(r"^ー+$", tokens[j].get("surface", "")):
                    marks += tokens[j].get("surface", "")
                    j += 1
                following = remaining[len(t.get("surface", "")) + len(marks) :][:1]
                if len(marks) > 1 and regex.match(r"\p{Han}", following):
                    marks = "ー"
                combined = t.get("surface", "") + marks
                lemma = t.get("lemma") or t.get("surface", "")
                result.append({"surface": combined, "pos": t.get("pos", ""), "lemma": lemma})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "prolonged-sound-merge"

        # 5. タリ活用副詞
        if not merged:
            derived_tari = regex.match(r"^\p{Han}+然と", remaining)
            if derived_tari is not None:
                adverb = derived_tari.group(0)
                previous_surface = result[-1].get("surface", "") if result else ""
                following_surface = remaining[len(adverb) : len(adverb) + 1]
                if previous_surface == "の" and following_surface == "は":
                    derived_tari = None
            if derived_tari is not None:
                adverb = derived_tari.group(0)
                length = 0
                j = i
                while j < len(tokens) and length < len(adverb):
                    length += len(tokens[j].get("surface", ""))
                    j += 1
                if length == len(adverb):
                    result.append({"surface": adverb, "pos": "副詞", "lemma": adverb[:-1]})
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = "tari-adverb"

        if not merged:
            for stem in TARI_ADVERB_STEMS:
                adverb = stem + "と"
                if remaining.startswith(adverb):
                    length = 0
                    j = i
                    while j < len(tokens) and length < len(adverb):
                        length += len(tokens[j].get("surface", ""))
                        j += 1
                    if length == len(adverb):
                        result.append({"surface": adverb, "pos": "副詞", "lemma": stem})
                        i = j
                        merged = True
                        if applied_rule is None:
                            applied_rule = "tari-adverb"
                        break

        # 5a. Verb renyokei + 会
        if not merged and t.get("pos") == "動詞" and t.get("conj_form") == "連用形":
            j = i + 1
            if j < len(tokens):
                nxt = tokens[j]
                if nxt.get("surface") == "会" and nxt.get("pos") == "名詞" and nxt.get("pos_sub1") == "接尾":
                    combined = t.get("surface", "") + "会"
                    result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                    i = j + 1
                    merged = True
                    if applied_rule is None:
                        applied_rule = "verb-renyokei+kai"

        # 5a'. Short simple verb renyokei + 方 (歩き方, やり方, 読み方, 言い方)
        # remains a lexical search unit. Longer compound continuatives retain
        # the productive suffix boundary (打ち合わせ + 方, 組み合わせ + 方).
        if not merged and t.get("pos") == "動詞" and t.get("conj_form") == "連用形":
            j = i + 1
            if j < len(tokens):
                nxt = tokens[j]
                if (
                    len(t.get("surface", "")) <= 2
                    and not (t.get("surface") == "し" and result and result[-1].get("surface") == "に")
                    and nxt.get("surface") == "方"
                    and nxt.get("pos") == "名詞"
                    and nxt.get("pos_sub1") == "接尾"
                ):
                    combined = t.get("surface", "") + "方"
                    result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                    i = j + 1
                    merged = True
                    if applied_rule is None:
                        applied_rule = "verb-renyokei+kata"

        # 5a''. Noun + verb-forming derivational suffix (謎めく, 冗談めかす).
        # The suffix builds one godan paradigm with its host, so the whole
        # derived verb inflects as a unit and carries no internal boundary.
        # The reference dictionary merges only the entries it happens to hold
        # (春めい) and splits the rest.
        if not merged and t.get("pos") == "名詞" and t.get("pos_sub1") != "接尾" and i + 1 < len(tokens):
            nxt = tokens[i + 1]
            suffix_lemma = DERIVED_VERB_SUFFIX_FORMS.get(nxt.get("surface", ""))
            if suffix_lemma is not None and nxt.get("pos") == "動詞":
                combined = t.get("surface", "") + nxt.get("surface", "")
                result.append(
                    {
                        "surface": combined,
                        "pos": "動詞",
                        "lemma": t.get("surface", "") + suffix_lemma,
                    }
                )
                i += 2
                merged = True
                if applied_rule is None:
                    applied_rule = "noun+derived-verb-suffix"

        # 5b. Proper noun + region suffix
        # A destination suffix is one productive search unit with its nominal
        # host (東京行き, 学校行き).  The 接尾 feature supplies the boundary
        # evidence; no place-name or ordinary-noun list is needed.
        if not merged and t.get("pos") == "名詞" and i + 1 < len(tokens):
            nxt = tokens[i + 1]
            if nxt.get("surface") == "行き" and nxt.get("pos") == "名詞" and nxt.get("pos_sub1") == "接尾":
                combined = t.get("surface", "") + "行き"
                result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                i += 2
                merged = True
                if applied_rule is None:
                    applied_rule = "destination-suffix"

        # 5c. Proper noun + region suffix
        if not merged and t.get("pos") == "名詞" and t.get("pos_sub1") == "固有名詞" and t.get("pos_sub2") == "地域":
            j = i + 1
            combined = t.get("surface", "")
            while j < len(tokens):
                nxt = tokens[j]
                ns = nxt.get("surface", "")
                if ns == "行き":
                    break
                is_proper_region = (
                    nxt.get("pos") == "名詞" and nxt.get("pos_sub1") == "固有名詞" and nxt.get("pos_sub2") == "地域"
                )
                is_region_suffix = (
                    nxt.get("pos") == "名詞" and nxt.get("pos_sub1") == "接尾" and nxt.get("pos_sub2") == "地域"
                )
                is_kanji_noun = (
                    nxt.get("pos") == "名詞" and regex.match(r"^[\p{Han}]+$", ns) and nxt.get("pos_sub1") != "接尾"
                )
                if is_proper_region or is_region_suffix or is_kanji_noun:
                    combined += ns
                    j += 1
                else:
                    break
            if j > i + 1:
                result.append({"surface": combined, "pos": "名詞", "pos_sub1": "固有名詞", "lemma": combined})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "proper-noun"

        # 6. Kanji compound
        if not merged:
            kanji_end, combined = _kanji_noun_run(tokens, i)
            if kanji_end > i + 1:
                result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                i = kanji_end
                merged = True
                if applied_rule is None:
                    applied_rule = "kanji-compound"

        # 7. Katakana compound
        if not merged and regex.match(r"^[\u30A0-\u30FF]+$", t.get("surface", "")) and t.get("pos") == "名詞":
            j = i + 1
            combined = t.get("surface", "")
            while (
                j < len(tokens)
                and regex.match(r"^[\u30A0-\u30FF]+$", tokens[j].get("surface", ""))
                and tokens[j].get("pos") == "名詞"
            ):
                combined += tokens[j].get("surface", "")
                j += 1
            if j > i + 1:
                result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "katakana-compound"

        # 7b. Alphabet + Katakana/Kanji compound
        if not merged and regex.match(r"^[A-Za-z]+$", t.get("surface", "")) and t.get("pos") == "名詞":
            j = i + 1
            combined = t.get("surface", "")
            while j < len(tokens):
                nxt = tokens[j]
                ns = nxt.get("surface", "")
                np = nxt.get("pos", "")
                is_katakana = regex.match(r"^[\u30A0-\u30FF]+$", ns) and np == "名詞"
                is_kanji = regex.match(r"^[\p{Han}]+$", ns) and np == "名詞"
                if is_katakana or is_kanji:
                    combined += ns
                    j += 1
                    break  # Only merge one following token
                else:
                    break
            if j > i + 1:
                result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "alphabet-compound"

        # 7c. Snake_case identifier
        if not merged and regex.match(r"^[A-Za-z0-9]+$", t.get("surface", "")) and t.get("pos") == "名詞":
            j = i + 1
            combined = t.get("surface", "")
            found_underscore = False
            while j < len(tokens):
                nxt = tokens[j]
                if nxt.get("surface") == "_":
                    if j + 1 < len(tokens):
                        after = tokens[j + 1]
                        if regex.match(r"^[A-Za-z0-9]+$", after.get("surface", "")):
                            combined += "_" + after["surface"]
                            j += 2
                            found_underscore = True
                            continue
                    break
                else:
                    break
            if found_underscore:
                result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "snake-case"

        # 7b. Mention pattern
        if not merged and t.get("surface") == "@":
            j = i + 1
            combined = "@"
            found_mention = False
            while j < len(tokens):
                ns = tokens[j].get("surface", "")
                if regex.match(r"^[A-Za-z0-9]+$", ns):
                    combined += ns
                    j += 1
                    found_mention = True
                elif ns == "_" and found_mention:
                    if j + 1 < len(tokens) and regex.match(r"^[A-Za-z0-9]+$", tokens[j + 1].get("surface", "")):
                        combined += "_"
                        j += 1
                    else:
                        break
                else:
                    break
            if found_mention:
                result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "mention"

        # 7c. Hashtag pattern
        # A hashtag is a single search unit: the marker plus every following token
        # that is still hashtag-body text. Stopping after one token would cut a tag
        # whose body spans several morphemes (#経済成長, #美しい景色).
        if not merged and t.get("surface") in ("#", "＃"):
            marker = t["surface"]
            body_match = HASHTAG_BODY_RUN.match(remaining, len(marker)) if _opens_hashtag(text, pos_in_text) else None
            body = body_match.group(0) if body_match else ""
            if body:
                consumed = 0
                j = i + 1
                while j < len(tokens) and consumed < len(body):
                    consumed += len(tokens[j].get("surface", ""))
                    j += 1
                if consumed == len(body):
                    combined = marker + body
                    result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = "hashtag"

        # 8. Colloquial pronouns
        if not merged:
            for pronoun in COLLOQUIAL_PRONOUNS:
                if remaining.startswith(pronoun):
                    length = 0
                    j = i
                    while j < len(tokens) and length < len(pronoun):
                        length += len(tokens[j].get("surface", ""))
                        j += 1
                    if length == len(pronoun):
                        result.append({"surface": pronoun, "pos": "代名詞", "lemma": pronoun})
                        i = j
                        merged = True
                        if applied_rule is None:
                            applied_rule = "colloquial-pronoun"
                        break

        # 8b. Character speech: にゃ+ん -> にゃん
        if not merged and t.get("surface") == "にゃ":
            if i + 1 < len(tokens) and tokens[i + 1].get("surface") == "ん":
                result.append({"surface": "にゃん", "pos": "助詞", "lemma": "にゃん"})
                i += 2
                merged = True
                if applied_rule is None:
                    applied_rule = "character-speech"

        # 9. Compound verbs
        following_source = remaining[len(t.get("surface", "")) :]
        begins_fixed_subsidiary = any(following_source.startswith(form) for form in _FIXED_INFLECTED_FUNCTION_UNITS)
        v1_surface = t.get("surface", "")
        # A compound verb's first member is an independent verb. The voice
        # auxiliaries share the 動詞 tag but carry 接尾, and letting them through
        # builds a headword out of an auxiliary and the subsidiary that follows
        # it (れ続ける), which drops the passive from the analysis entirely.
        v1_verb_renyokei = (
            t.get("pos") == "動詞" and t.get("pos_sub1") != "接尾" and "連用" in (t.get("conj_form") or "")
        )
        # MeCab frequently lexicalizes a bare renyokei as a noun (座り, 入り).
        # Reconstructing its base is a productive morphology check; the closed
        # V2 class below prevents this from becoming an unrestricted noun+verb
        # merge rule.
        # A dependent noun keeps its own boundary: 〜たきり is the formal noun, not
        # a nominalized 切り, however well it reconstructs as one.
        v1_nominal_renyokei = (
            t.get("pos") == "名詞" and t.get("pos_sub1") != "非自立" and base_from_renyokei(v1_surface) is not None
        )
        if not merged and not begins_fixed_subsidiary and (v1_verb_renyokei or v1_nominal_renyokei):
            j = i + 1
            if j < len(tokens):
                nxt = tokens[j]
                if nxt.get("pos") == "動詞" and (nxt.get("lemma") or nxt.get("surface", "")) != "でる":
                    next_lemma = nxt.get("lemma") or nxt.get("surface", "")
                    v2_base = ""
                    for v2 in COMPOUND_VERB_V2_GODAN + COMPOUND_VERB_V2_ICHIDAN:
                        if next_lemma == v2:
                            v2_base = v2
                            break
                    v1_is_suru = (t.get("lemma") or v1_surface) == "する"
                    restricted = COMPOUND_VERB_V2_NOT_AFTER_SURU if v1_is_suru else COMPOUND_VERB_V2_SURU_ONLY
                    if v2_base in restricted:
                        v2_base = ""
                    if v2_base:
                        combined = t.get("surface", "") + nxt.get("surface", "")
                        compound_lemma = t.get("surface", "") + v2_base
                        result.append({"surface": combined, "pos": "動詞", "lemma": compound_lemma})
                        i = j + 1
                        merged = True
                        if applied_rule is None:
                            applied_rule = "compound-verb"

        # 9a. A productive V1+V2 continuative directly nominalized by a
        # particle is one deverbal compound search unit. MeCab often tags V2
        # as a noun in this context (押し/下げ/を), so the finite-verb rule above
        # cannot see it. Reconstruct V2 through the conjugation table and
        # require both the closed V2 class and the nominalizing follower.
        v1_renyokei = t.get("pos") == "動詞" and "連用" in (t.get("conj_form") or "")
        v1_nominal_renyokei = t.get("pos") == "名詞" and base_from_renyokei(v1_surface) is not None
        if not merged and (v1_renyokei or v1_nominal_renyokei):
            if i + 2 < len(tokens):
                nxt = tokens[i + 1]
                follower = tokens[i + 2]
                v2_readings = bases_from_renyokei(nxt.get("surface", ""))
                v2_base = next((base for base in v2_readings if base in _PRODUCTIVE_COMPOUND_V2), None)
                nominalizing_particle = (
                    follower.get("pos") == "助詞" and follower.get("surface") in _NOMINALIZING_PARTICLES
                )
                # A bound or verbal-noun V2 reading attaches to whatever stem
                # precedes it (仕立て+直し, 引き+寄せ); a free nominal V2 attaches
                # only behind an unambiguous verb continuative (送り+届け). Two free
                # nominals side by side are coordinated, not compounded, and keep
                # their boundary (上がり + 下がり).
                v2_is_bound_reading = nxt.get("pos") == "接尾辞" or nxt.get("pos_sub1") in {"接尾", "サ変接続"}
                # A し-final stem is the continuative of a Godan-sa verb (出し, 押し)
                # however MeCab tags it, so it counts as a verbal head as well.
                v1_is_verbal = v1_renyokei or v1_surface.endswith("し")
                if (
                    nxt.get("pos") in {"名詞", "接尾辞"}
                    and v2_base is not None
                    and (v2_is_bound_reading or v1_is_verbal)
                    and nominalizing_particle
                ):
                    combined = v1_surface + nxt.get("surface", "")
                    result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                    i += 2
                    merged = True
                    if applied_rule is None:
                        applied_rule = "compound-renyokei-nominal"

        # 9b. Lexicalized こもる compounds MeCab fails to merge because the
        # renyokei prefix (引き) is highly productive and tagged as a noun.
        # As a tokenizer, 引きこもり/引きこもる is a single search unit; treat こもる
        # as V2 even when MeCab tags the preceding renyokei form as a noun.
        if not merged and t.get("pos") == "名詞":
            j = i + 1
            if j < len(tokens):
                nxt = tokens[j]
                next_lemma = nxt.get("lemma") or nxt.get("surface", "")
                if nxt.get("pos") == "動詞" and next_lemma in ("こもる", "籠る", "籠もる"):
                    combined = t.get("surface", "") + nxt.get("surface", "")
                    result.append({"surface": combined, "pos": "動詞", "lemma": t.get("surface", "") + "こもる"})
                    i = j + 1
                    merged = True
                    if applied_rule is None:
                        applied_rule = "komoru-compound"

        # 10. Lexicalized hiragana words
        if not merged:
            for word in sorted(HIRAGANA_COMPOUNDS.keys(), key=len, reverse=True):
                if remaining.startswith(word):
                    length = 0
                    j = i
                    consumed = ""
                    while j < len(tokens) and length < len(word):
                        token_surface = tokens[j].get("surface", "")
                        consumed += token_surface
                        length += len(token_surface)
                        j += 1
                    residual = consumed[len(word) :]
                    if consumed.startswith(word) and (not residual or residual in _NOMINALIZING_PARTICLES):
                        result.append({"surface": word, "pos": HIRAGANA_COMPOUNDS[word], "lemma": word})
                        if residual:
                            result.append({"surface": residual, "pos": "助詞", "lemma": residual})
                        i = j
                        merged = True
                        if applied_rule is None:
                            applied_rule = "hiragana-compound"
                        break

        # 11. Colloquial intensifier めちゃ
        if not merged and t.get("surface") == "め" and t.get("pos") == "名詞":
            if i + 1 < len(tokens) and tokens[i + 1].get("surface") == "ちゃ":
                result.append({"surface": "めちゃ", "pos": "副詞", "lemma": "めちゃ"})
                i += 2
                merged = True
                if applied_rule is None:
                    applied_rule = "mecha-merge"

        # AだのBだの coordinates with one particle repeated. The reference
        # analyzer lexicalizes it inside the sentence but falls back to the
        # copula plus の at the end, so the same morpheme comes out two
        # different ways in a single coordination. An earlier だの in the same
        # sentence is what identifies the frame.
        if (
            not merged
            and t.get("surface") == "だ"
            and i + 1 < len(tokens)
            and tokens[i + 1].get("surface") == "の"
            and any(prior.get("surface") == "だの" for prior in result)
        ):
            result.append({"surface": "だの", "pos": "助詞", "lemma": "だの"})
            i += 2
            merged = True
            if applied_rule is None:
                applied_rule = "dano-coordination"

        # 11b. ず+に -> ずに
        if not merged and t.get("surface") == "ず" and t.get("pos") == "助動詞":
            if i + 1 < len(tokens) and tokens[i + 1].get("surface") == "に":
                result.append({"surface": "ずに", "pos": "助動詞", "lemma": "ず"})
                i += 2
                merged = True
                if applied_rule is None:
                    applied_rule = "zu-ni-merge"

        # 11bb. Productive renyokei + たて suffix.  MeCab sometimes reads
        # the closed freshness suffix as the unrelated past auxiliary + te
        # particle (e.g. でき+た+て).  The raw token stream still contains
        # punctuation, so doing this before symbol filtering cannot join across
        # sentence boundaries.
        if (
            not merged
            and t.get("pos") == "動詞"
            and i + 2 < len(tokens)
            and tokens[i + 1].get("surface") == "た"
            and tokens[i + 1].get("pos") == "助動詞"
            and tokens[i + 2].get("surface") == "て"
            and tokens[i + 2].get("pos") == "助詞"
        ):
            result.append(
                {
                    "surface": t.get("surface", ""),
                    "pos": "動詞",
                    "lemma": t.get("lemma") or base_from_renyokei(t.get("surface", "")) or t.get("surface", ""),
                }
            )
            result.append({"surface": "たて", "pos": "接尾辞", "pos_sub1": "接尾", "lemma": "たて"})
            i += 3
            merged = True
            if applied_rule is None:
                applied_rule = "productive-tate-suffix"

        # Closed lexical units ending in て remain intact before every
        # inflectional cell of ある.  The reference analyzer can expose the
        # same compound particle as one token before ある but as several tokens
        # before あった, so inspect the complete source span rather than the
        # current tokenization.
        if not merged:
            span = ""
            follower_idx = i
            while follower_idx < len(tokens):
                follower_surface = tokens[follower_idx].get("surface", "")
                if follower_surface in ("ある", "あっ", "あり", "あれ"):
                    break
                span += follower_surface
                follower_idx += 1
            if follower_idx < len(tokens):
                fixed_te_unit = _fixed_te_search_unit(span)
                if fixed_te_unit is not None:
                    result.append(fixed_te_unit)
                    i = follower_idx
                    merged = True
                    if applied_rule is None:
                        applied_rule = "fixed-te-search-unit-before-aru"

        # 11c. Resultative 〜てある retains the te-particle boundary.  MeCab
        # may emit an ichidan te-form as one token (並べて), while Suzume keeps
        # the productive verb stem + て + ある chain for its grammar model.
        #
        # Ending in て is not on its own evidence of a te-form: compound case
        # particles and lexical adverbs end the same way (について, 全て).  The
        # split is therefore only taken when the stem it would leave behind
        # actually names a verb, which is what a te-form always decomposes into.
        # Without that check the lemma is fabricated by appending る to whatever
        # precedes the て (についる, 全る).
        if (
            not merged
            and t.get("surface", "").endswith("て")
            and len(t.get("surface", "")) > 1
            and i + 1 < len(tokens)
            and tokens[i + 1].get("surface") in ("ある", "あっ", "あり", "あれ")
        ):
            stem = t["surface"][:-1]
            lemma = t.get("lemma") or stem
            if lemma == t["surface"]:
                lemma = stem + "る"
            if _reads_as_one_verb(lemma):
                result.append({"surface": stem, "pos": "動詞", "lemma": lemma})
                result.append({"surface": "て", "pos": "助詞", "lemma": "て"})
                i += 1
                merged = True
                if applied_rule is None:
                    applied_rule = "te-aru-split"

        # No merge: pass through
        if not merged:
            lemma = t.get("lemma") or t.get("surface", "")
            lemma = FIXED_FUNCTION_LEMMAS.get(t.get("surface", ""), lemma)
            result.append(
                {
                    "surface": t.get("surface", ""),
                    "pos": t.get("pos", ""),
                    "pos_sub1": t.get("pos_sub1"),
                    "pos_sub2": t.get("pos_sub2"),
                    "conj_type": t.get("conj_type"),
                    "conj_form": t.get("conj_form"),
                    "lemma": lemma,
                }
            )
            i += 1

    # Post-process passes
    result = _postprocess_kamo(result, applied_rule)
    result, applied_rule = _postprocess_totomoni(result, applied_rule)
    result, applied_rule = _postprocess_noni(result, applied_rule)
    result, applied_rule = _postprocess_atode(result, applied_rule)
    _postprocess_epenthetic_sa(result)
    result, applied_rule = _postprocess_honorific_split(result, applied_rule)
    result, applied_rule = _postprocess_prefix_split(result, applied_rule)
    result, applied_rule = _postprocess_nde_split(result, applied_rule)
    result, applied_rule = _postprocess_filler_split(result, applied_rule)
    result, applied_rule = _postprocess_kuruwa(result, applied_rule)
    result, applied_rule = _postprocess_demo_copula(result, applied_rule)
    result, applied_rule = _postprocess_gamashii(result, applied_rule)
    result, applied_rule = _postprocess_adj_bungo(result, applied_rule)
    result, applied_rule = _postprocess_adj_kari(result, applied_rule)
    result, applied_rule = _postprocess_ha_row_godan(result, applied_rule)
    result, applied_rule = _postprocess_nidan_cell(result, applied_rule)
    result, applied_rule = _postprocess_nominal_classical_copula(result, applied_rule)
    result, applied_rule = _postprocess_historical_kana_word(result, applied_rule)
    result, applied_rule = _postprocess_kakari_pronoun_split(result, applied_rule)
    result, applied_rule = _postprocess_classical_mu(result, applied_rule)
    result, applied_rule = _postprocess_ku_nominalization(result, applied_rule)
    result, applied_rule = _postprocess_classical_shimu(result, applied_rule)
    result, applied_rule = _postprocess_izenkei_concessive(result, applied_rule)
    result, applied_rule = _postprocess_tomo_particle(result, applied_rule)
    result, applied_rule = _postprocess_bound_voiced_suffix(result, applied_rule)
    result, applied_rule = _postprocess_bound_suffix_noun_cell(result, applied_rule)
    result, applied_rule = _postprocess_kanji_merge(result, applied_rule)
    result, applied_rule = _postprocess_nickname_merge(result, applied_rule)
    result, applied_rule = _postprocess_search_unit_split(result, applied_rule)
    result, applied_rule = _postprocess_onomatopoeia_tto_merge(result, applied_rule)
    result, applied_rule = _postprocess_productive_mimetics(result, applied_rule)
    result, applied_rule = _postprocess_distributive_quantity(result, applied_rule)
    result, applied_rule = _postprocess_nominal_zukeru(result, applied_rule)
    result, applied_rule = _postprocess_ascii_joiner_merge(result, applied_rule)
    result, applied_rule = _postprocess_small_kana_head_merge(result, applied_rule)
    _postprocess_dialectal(result)

    return result, applied_rule
