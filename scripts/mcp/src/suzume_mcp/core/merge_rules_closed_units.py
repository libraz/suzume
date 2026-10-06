"""Merge steps for closed function words, fixed search units, and negation or intensifier units."""

import unicodedata

import regex

from .constants import (
    ARCHAIC_PERSONAL_PRONOUNS,
    COUNTER_UNITS,
    FIXED_FUNCTION_LEMMAS,
    FIXED_FUNCTION_SEARCH_UNITS,
    FIXED_INFLECTED_FUNCTION_UNITS,
    GREETING_INTERJECTIONS,
    KANA_COUNTER_SUFFIXES,
    KANA_NUMBER_STEMS,
    KANA_PERSONAL_PRONOUNS,
)
from .core_lexicon import clipped_greetings, core_headwords_by_length
from .mecab import is_single_token_of_pos, mecab_analyze
from .merge_postprocessors import KARI_MIZENKEI_CELL, classical_adjective_lemma
from .merge_rules_helpers import _FIXED_INFLECTED_FUNCTION_UNITS, _UTTERANCE_FINAL_PARTICLE_HEADS, _consume_span
from .merge_rules_state import MergeState
from .split_rules import bases_from_renyokei

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


def _is_punctuation_surface(surface: str) -> bool:
    """Return whether every character of the surface is punctuation."""
    return bool(surface) and all(unicodedata.category(char).startswith("P") for char in surface)


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


_HA_ROW_REALIS_TAILS = ("ども", "ど", "ば")


def _ha_row_realis_cells(remaining: str) -> list[dict] | None:
    """Split the ハ行四段 已然形 from the concessive/conditional it hosts (言|へ|ども).

    The cell kana is the directional case particle, so the dictionary reads the
    stem in front as a noun of its own.  As for the irrealis, the modern ワ行五段
    spelling of the stem (stem + う) tells a real verb from a nominal that takes
    the case particle (家へ).
    """
    match = regex.match(
        rf"^(\p{{Han}}+)(へ)({'|'.join(_HA_ROW_REALIS_TAILS)})(?=$|[^\p{{Hiragana}}])",
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
        return [
            *([{"surface": host, "pos": "名詞", "lemma": host}] if host else []),
            {"surface": stem + cell, "pos": "動詞", "lemma": stem + "ふ"},
            {"surface": tail, "pos": "助詞", "pos_sub1": "接続助詞", "lemma": tail},
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


_KANA_NUMBER_COUNTERS = tuple(
    sorted((stem + suffix for stem in KANA_NUMBER_STEMS for suffix in KANA_COUNTER_SUFFIXES), key=len, reverse=True)
)


_FIXED_FUNCTION_SEARCH_UNITS = tuple(sorted(FIXED_FUNCTION_SEARCH_UNITS, key=len, reverse=True))


_KEYCAP_EMOJI = regex.compile(r"[0-9#*]\uFE0F?\u20E3")


_PRETOKENIZED_QUANTITY = regex.compile(r"(?:\d{1,3}(?:,\d{3})+|\d+)円")


_PRETOKENIZED_COMMA_NUMBER = regex.compile(r"\d{1,3}(?:,\d{3})+")


_PRETOKENIZED_EMAIL = regex.compile(r"[A-Za-z0-9][A-Za-z0-9._+\-]*@[A-Za-z0-9\-]+(?:\.[A-Za-z0-9\-]+)+")


def _is_verb_base(base: str) -> bool:
    """Whether a reconstructed kana dictionary form is a verb in a verbal frame.

    A bare kana form can be an adverb or a noun on its own (かく, のむ), so the
    probe places it before ことができる, which only a verb can precede.
    """
    probe = mecab_analyze(base + "ことができる")
    return bool(probe) and probe[0].get("surface") == base and probe[0].get("pos") == "動詞"


def _merge_closed_function_units(state: MergeState) -> bool:
    """Closed pronouns, formal nouns, and connective particles that stay one search unit."""
    tokens = state.tokens
    result = state.result
    i = state.i
    t = state.t
    merged = state.merged
    applied_rule = state.applied_rule
    remaining = state.remaining
    try:
        # A kana personal pronoun is a closed-class word the reference does not
        # list, so its spelling falls apart into readable pieces (かの+じょ).
        # The archaic personal pronouns (それがし, わらわ, 拙僧) share their kana
        # with ordinary words (それ+が+し, 笑わ), so they are taken only where a
        # nominal phrase can stand: opening a phrase and closed by a particle, the
        # copula, punctuation or the end of the input.
        if not merged:
            for pronoun in (*KANA_PERSONAL_PRONOUNS, *ARCHAIC_PERSONAL_PRONOUNS):
                if not remaining.startswith(pronoun):
                    continue
                consumed, j = _consume_span(tokens, i, len(pronoun))
                if pronoun in ARCHAIC_PERSONAL_PRONOUNS and not (
                    (i == 0 or tokens[i - 1].get("pos") in ("助詞", "記号"))
                    and (
                        j == len(tokens)
                        or tokens[j].get("pos") in ("助詞", "記号")
                        or (tokens[j].get("pos") == "助動詞" and tokens[j].get("lemma") in ("だ", "です"))
                    )
                ):
                    continue
                # A pronoun the reference read as one other word (わらわ as 笑わ)
                # is retagged; one it already split is rejoined.
                if consumed == pronoun and (j - i > 1 or pronoun in ARCHAIC_PERSONAL_PRONOUNS):
                    result.append({"surface": pronoun, "pos": "名詞", "pos_sub1": "代名詞", "lemma": pronoun})
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = "kana-personal-pronoun"
                    break
            if merged:
                return True

        # The kana spelling of 左様 is one na-adjective stem; the reference cuts it
        # into さ+よう (a degree adverb and the formal noun) before the copula.
        if (
            not merged
            and t.get("surface") == "さ"
            and i + 2 < len(tokens)
            and tokens[i + 1].get("surface") == "よう"
            and tokens[i + 2].get("surface") in ("な", "で", "だ", "です")
        ):
            result.append({"surface": "さよう", "pos": "名詞", "pos_sub1": "形容動詞語幹", "lemma": "さよう"})
            i += 2
            merged = True
            if applied_rule is None:
                applied_rule = "kana-sayou-stem"
            return True
        # Where the reference does list さよう, it tags the adverb; the word is the
        # same na-adjective stem before its copula or adverbial に.
        if (
            not merged
            and t.get("surface") == "さよう"
            and t.get("pos") == "副詞"
            and i + 1 < len(tokens)
            and tokens[i + 1].get("surface") in ("な", "に", "で", "だ", "です")
        ):
            result.append({"surface": "さよう", "pos": "名詞", "pos_sub1": "形容動詞語幹", "lemma": "さよう"})
            i += 1
            merged = True
            if applied_rule is None:
                applied_rule = "kana-sayou-stem"
            return True

        # The formal noun もの takes an attributive form, so a continuative in
        # front of it is the first member of the compound noun (たべ+もの as
        # 食べもの, のみもの, かいもの). The reference keeps the kanji compounds
        # whole but splits the kana ones, sometimes misreading the continuative
        # as a noun or an adjective stem; a host whose base is a real verb is
        # the same continuative.
        if not merged and i + 1 < len(tokens) and tokens[i + 1].get("surface") == "もの":
            host_surface = t.get("surface", "")
            continuative_verb = t.get("pos") == "動詞" and t.get("conj_form") == "連用形"
            kana_host = regex.fullmatch(r"\p{Hiragana}{2,}", host_surface) is not None
            misread_host = (
                kana_host
                and (
                    t.get("pos") in ("動詞", "名詞") or (t.get("pos") == "形容詞" and t.get("conj_form") == "ガル接続")
                )
                and t.get("pos_sub1") not in ("代名詞", "非自立")
                and any(_is_verb_base(base) for base in bases_from_renyokei(host_surface))
            )
            if continuative_verb or misread_host:
                compound = host_surface + "もの"
                result.append({"surface": compound, "pos": "名詞", "pos_sub1": "一般", "lemma": compound})
                i += 2
                merged = True
                if applied_rule is None:
                    applied_rule = "continuative-mono-compound"
                return True

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
                consumed, j = _consume_span(tokens, i, len(topic_noun))
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

        # After a clause-final predicate (a verb, an i-adjective, or the
        # auxiliaries た/だ/ない/な), ものの is a closed concessive particle when a
        # predicate follows.  IPADIC splits it into formal noun + genitive
        # depending on the host and the follower.  A following predicate keeps
        # a genuine nominal genitive (静かなものの色, 読んだものの数) out of
        # this rule.
        if (
            not merged
            and t.get("surface") == "もの"
            and t.get("pos") == "名詞"
            and i > 0
            and (
                tokens[i - 1].get("pos") in ("動詞", "形容詞")
                or (tokens[i - 1].get("pos") == "助動詞" and tokens[i - 1].get("surface") in ("た", "だ", "ない", "な"))
            )
            and i + 2 < len(tokens)
            and tokens[i + 1].get("surface") == "の"
            and tokens[i + 1].get("pos") == "助詞"
            and tokens[i + 2].get("pos") in ("動詞", "形容詞")
        ):
            result.append({"surface": "ものの", "pos": "助詞", "lemma": "ものの"})
            i += 2
            merged = True
            if applied_rule is None:
                applied_rule = "predicate-monono"

        # Preserve a classical kari adjective before generic noun recovery can
        # absorb its kanji stem. The classical terminal cell validates the open
        # adjective class and supplies its headword; the suffix/auxiliary cells
        # are grammatical. The ハ行四段 irrealis needs the same protection, and
        # for the same reason: its cell kana is the topic particle, so generic
        # noun recovery takes the stem in front of it as a word of its own (言|は|しむ).
        for cells_of, label in (
            (_classical_adjective_cells, "classical-adjective-kari"),
            (_ha_row_irrealis_cells, "classical-ha-row-irrealis"),
            (_ha_row_realis_cells, "classical-ha-row-realis"),
        ):
            if merged:
                break
            cells = cells_of(remaining)
            if cells is not None:
                source_span = "".join(cell["surface"] for cell in cells)
                consumed, j = _consume_span(tokens, i, len(source_span))
                if consumed == source_span:
                    result.extend(cells)
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = label

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

        # After a nominal, the reference reads ともに as the suffix とも plus the
        # case particle に, but after a counter it keeps the adverb ともに
        # (二人ともに).  Both are the same adverb of joint inclusion.
        if (
            not merged
            and t.get("surface") == "とも"
            and t.get("pos") == "名詞"
            and t.get("pos_sub1") == "接尾"
            and i > 0
            and tokens[i - 1].get("pos") == "名詞"
            and i + 1 < len(tokens)
            and tokens[i + 1].get("surface") == "に"
            and tokens[i + 1].get("pos") == "助詞"
        ):
            result.append({"surface": "ともに", "pos": "副詞", "pos_sub1": "一般", "lemma": "ともに"})
            i += 2
            merged = True
            if applied_rule is None:
                applied_rule = "nominal-tomoni"

        # The interrogative pronoun いつ is read as the continuative of いる plus
        # the classical perfect つ when a particle or the copula follows.  A
        # phrase-initial pair closed by a particle, the copula or punctuation is
        # the nominal phrase the pronoun heads, not a finite classical predicate.
        # Punctuation is recognized by its characters too: the reference tags
        # the ASCII ? the oracle text normalizes to as a noun.
        if (
            not merged
            and t.get("surface") == "い"
            and t.get("pos") == "動詞"
            and (i == 0 or tokens[i - 1].get("pos") in ("助詞", "記号"))
            and i + 1 < len(tokens)
            and tokens[i + 1].get("surface") == "つ"
            and tokens[i + 1].get("pos") == "助動詞"
            and (
                i + 2 == len(tokens)
                or tokens[i + 2].get("pos") in ("助詞", "記号")
                or _is_punctuation_surface(tokens[i + 2].get("surface", ""))
                or (tokens[i + 2].get("pos") == "助動詞" and tokens[i + 2].get("lemma") in ("だ", "です"))
            )
        ):
            result.append({"surface": "いつ", "pos": "名詞", "pos_sub1": "代名詞", "lemma": "いつ"})
            i += 2
            merged = True
            if applied_rule is None:
                applied_rule = "interrogative-itsu"

        # The interrogative adverb いかが is read as いか+が whenever a predicate
        # rather than a copula follows.  An honorific prefix or a polite verb
        # chain after it is the register of the adverb, never of a noun subject.
        if (
            not merged
            and t.get("surface") == "いか"
            and t.get("pos") == "名詞"
            and i + 2 < len(tokens)
            and tokens[i + 1].get("surface") == "が"
            and tokens[i + 1].get("pos") == "助詞"
            and (
                tokens[i + 2].get("pos") == "接頭詞"
                # The reference lexicalizes some honorific nouns whole (お過ごし).
                or (
                    tokens[i + 2].get("pos") == "名詞"
                    and str(tokens[i + 2].get("surface", "")).startswith(("お", "ご", "御"))
                )
                or (
                    tokens[i + 2].get("pos") == "動詞"
                    and i + 3 < len(tokens)
                    and tokens[i + 3].get("lemma") == "ます"
                    and tokens[i + 3].get("pos") == "助動詞"
                )
            )
        ):
            result.append({"surface": "いかが", "pos": "副詞", "pos_sub1": "一般", "lemma": "いかが"})
            i += 2
            merged = True
            if applied_rule is None:
                applied_rule = "interrogative-ikaga"

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

        # The contracted progressive imperative てろ/でろ is one subsidiary-verb
        # cell; without a following particle IPADIC reads it as te-form plus a
        # stray noun ろ.  Re-emit the shape it gives the same cell before よ.
        if (
            not merged
            and t.get("surface") in ("て", "で")
            and t.get("pos") == "助詞"
            and t.get("pos_sub1") == "接続助詞"
            and i + 1 < len(tokens)
            and tokens[i + 1].get("surface") == "ろ"
            and tokens[i + 1].get("pos") == "名詞"
        ):
            result.append(
                {
                    "surface": t["surface"] + "ろ",
                    "pos": "動詞",
                    "pos_sub1": "非自立",
                    "conj_type": "一段",
                    "conj_form": "命令ｒｏ",
                    "lemma": t["surface"] + "る",
                }
            )
            i += 2
            merged = True
            if applied_rule is None:
                applied_rule = "contracted-progressive-imperative"
    finally:
        state.i = i
        state.merged = merged
        state.applied_rule = applied_rule
    return False


def _merge_fixed_search_units(state: MergeState) -> bool:
    """Pretokenized quantities, fixed function words, L2 nouns, and kana quantities."""
    tokens = state.tokens
    result = state.result
    standalone_noun_indexes = state.standalone_noun_indexes
    i = state.i
    t = state.t
    merged = state.merged
    applied_rule = state.applied_rule
    remaining = state.remaining
    try:
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
                consumed, j = _consume_span(tokens, i, len(unit))
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
                    consumed, j = _consume_span(tokens, i, len(source_span))
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
                consumed, j = _consume_span(tokens, i, len(fixed_form))
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
                consumed, j = _consume_span(tokens, i, len(fixed_word))
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

        if not merged:
            greeting = next((word for word in GREETING_INTERJECTIONS if remaining.startswith(word)), "")
            if greeting:
                consumed, j = _consume_span(tokens, i, len(greeting))
                if consumed == greeting:
                    result.append({"surface": greeting, "pos": "感動詞", "lemma": greeting})
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = "greeting-interjection"

        # A listed greeting clipped of its long vowel (おはよ, ありがと) is still
        # that greeting when it closes the run or hands off to a final particle.
        if not merged:
            clipped = clipped_greetings()
            clip = next((word for word in sorted(clipped, key=len, reverse=True) if remaining.startswith(word)), "")
            span = clip + regex.match(r"[ー〜～]*", remaining[len(clip) :]).group() if clip else ""
            after = remaining[len(span) : len(span) + 1]
            if clip and (
                after == "" or after in _UTTERANCE_FINAL_PARTICLE_HEADS or regex.match(r"\p{Hiragana}", after) is None
            ):
                consumed, j = _consume_span(tokens, i, len(span))
                if consumed == span:
                    result.append({"surface": span, "pos": "感動詞", "lemma": clipped[clip]})
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = "clipped-greeting"

        # An L2 noun is lexical evidence that an otherwise ambiguous sequence
        # is one search unit. Recover only whole adjacent MeCab tokens: a
        # headword ending inside a token must not consume that token's suffix.
        if not merged:
            for noun in core_headwords_by_length("nouns.tsv"):
                if not remaining.startswith(noun):
                    continue
                consumed, j = _consume_span(tokens, i, len(noun))
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
                consumed, j = _consume_span(tokens, i, len(kana_quantity))
                if consumed == kana_quantity:
                    result.append({"surface": kana_quantity, "pos": "名詞", "pos_sub1": "数", "lemma": kana_quantity})
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = "kana-number+unit"
    finally:
        state.i = i
        state.merged = merged
        state.applied_rule = applied_rule
    return False


def _merge_negation_and_intensifier_units(state: MergeState) -> bool:
    """Classical irrealis, directional particle, and intensifier units."""
    tokens = state.tokens
    result = state.result
    i = state.i
    t = state.t
    merged = state.merged
    applied_rule = state.applied_rule
    remaining = state.remaining
    try:
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
            consumed, j = _consume_span(tokens, i, len(intensifier))
            if consumed == intensifier and j < len(tokens) and tokens[j].get("pos") not in ("助詞", "記号"):
                result.append({"surface": intensifier, "pos": "副詞", "lemma": intensifier})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "dialectal-doe-intensifier"
    finally:
        state.i = i
        state.merged = merged
        state.applied_rule = applied_rule
    return False
