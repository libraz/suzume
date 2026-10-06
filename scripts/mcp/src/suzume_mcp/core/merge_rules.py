"""Merge rules ported from SuzumeUtils.pm apply_suzume_merge()."""

import functools
import unicodedata

import regex

from .constants import (
    ADVERBIAL_NA_ADJECTIVES,
    ARCHAIC_PERSONAL_PRONOUNS,
    CHARACTER_SPEECH_COPULAS,
    COLLOQUIAL_PRONOUNS,
    COMPOUND_VERB_V2_GODAN,
    COMPOUND_VERB_V2_ICHIDAN,
    COMPOUND_VERB_V2_NOT_AFTER_SURU,
    COMPOUND_VERB_V2_SURU_ONLY,
    COUNTER_UNITS,
    DERIVED_ADJECTIVE_SUFFIX_LEMMAS,
    DERIVED_VERB_FRAGMENT_SPAN,
    DERIVED_VERB_SUFFIX_FORMS,
    FAMILY_TERMS,
    FIXED_FUNCTION_LEMMAS,
    FIXED_FUNCTION_SEARCH_UNITS,
    FIXED_INFLECTED_FUNCTION_UNITS,
    GREETING_INTERJECTIONS,
    HIRAGANA_COMPOUNDS,
    KANA_COUNTER_SUFFIXES,
    KANA_NUMBER_STEMS,
    KANA_PERSONAL_PRONOUNS,
    LETTER_FORMULAS,
    NAI_ADJECTIVES,
    TARI_ADVERB_STEMS,
    TEMPORAL_COMPOUND_UNITS,
    TEMPORAL_PREFIX_KANJI,
)
from .core_lexicon import clipped_greetings, core_headwords, core_headwords_by_length, kana_i_adjective_lemmas
from .mecab import is_single_token_of_pos, mecab_analyze, reads_as_counter
from .merge_postprocessor_common import _continuative_verb_tokens
from .merge_postprocessors import (
    KARI_MIZENKEI_CELL,
    apply_merge_postprocessors,
    classical_adjective_lemma,
    nidan_cell,
    reads_as_continuative,
)
from .split_rules import _is_single_i_adjective, base_from_renyokei, bases_from_renyokei

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


# Verbs whose て-form after に lexicalized into a closed compound case particle
# (について, にあたって, に対して), keyed by the dictionary lemma.
_COMPOUND_CASE_PARTICLE_VERBS = frozenset(("つく", "あたる", "当たる", "対する"))


@functools.lru_cache(maxsize=4096)
def _reads_as_compound_case_particle(host: str, span: str) -> bool:
    """Whether the reference reads ``span`` as one particle after ``host`` in a noun frame.

    The adnominal frame (X+について+の意見) removes the follower that made the
    reference pick the literal verb, so its answer reflects the host alone.
    """
    probe = mecab_analyze(host + span + "の意見")
    return any(token.get("surface") == span and token.get("pos") == "助詞" for token in probe)


def _selects_te_form(token: dict) -> bool:
    """Whether a follower selects a verb's て-form rather than a particle phrase.

    Auxiliary verbs (いる, しまう, ください), the existential ある and the
    sequential から all attach to a literal verb (席に|つい|て|ください).
    """
    return (
        (token.get("pos") == "動詞" and (token.get("pos_sub1") == "非自立" or token.get("lemma") in ("いる", "ある")))
        or (token.get("pos") == "形容詞" and token.get("pos_sub1") == "非自立")
        or token.get("surface") == "から"
    )


def _consume_span(tokens: list[dict], start: int, length: int) -> tuple[str, int]:
    """Join surfaces from `start` until they cover `length` characters; return them and the end index."""
    consumed, end = "", start
    while end < len(tokens) and len(consumed) < length:
        consumed += tokens[end].get("surface", "")
        end += 1
    return consumed, end


def _covered_length(tokens: list[dict], start: int, length: int) -> tuple[int, int]:
    """Like `_consume_span`, but return how many characters the joined surfaces cover."""
    consumed, end = _consume_span(tokens, start, length)
    return len(consumed), end


_CHARACTER_SPEECH_COPULAS = tuple(sorted(CHARACTER_SPEECH_COPULAS, key=len, reverse=True))
_COPULA_HOST_POS = frozenset({"名詞", "副詞", "動詞", "形容詞", "助動詞"})
_UTTERANCE_FINAL_PARTICLE_HEADS = frozenset("よねなかぞぜわさ")


# The cells of がる after the や of やがる (やがら, やがり, やがる, やがれ, やがろ, やがっ).
_YAGARU_TAIL = regex.compile(r"が[らりるれろっ]")


def _closes_utterance(following: str) -> bool:
    """Whether nothing but punctuation, the text end or a final particle follows."""
    return (
        not following
        or regex.match(r"[\p{P}\p{S}\p{Z}]", following) is not None
        or following[0] in _UTTERANCE_FINAL_PARTICLE_HEADS
    )


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
# Cells of an i-adjective after its stem, longest first so かっ wins over か.
_I_ADJECTIVE_CELL_ENDINGS = ("かっ", "けれ", "かろ", "く", "い", "き")
_PRODUCTIVE_COMPOUND_V2 = frozenset(COMPOUND_VERB_V2_GODAN + COMPOUND_VERB_V2_ICHIDAN)
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


# Counter tails that chain into a following quantity of the same dimension.
_TIME_COUNTER_TAILS = frozenset("年月週日時間分秒")
_COUNTER_DIMENSION_SUCCESSORS = {"泊": frozenset("日"), "割": frozenset("分厘")}


def _same_counter_dimension(tail: str, next_tail: str) -> bool:
    """Whether a quantity ending in `next_tail` continues one ending in `tail`."""
    if tail in _TIME_COUNTER_TAILS:
        return next_tail in _TIME_COUNTER_TAILS
    return next_tail in _COUNTER_DIMENSION_SUCCESSORS.get(tail, frozenset())


def _continues_counter_dimension(tail: str, tokens: list[dict], number_index: int) -> bool:
    """Whether the numeral at `number_index` opens a quantity of the same dimension as `tail`."""
    index = number_index
    while index < len(tokens) and tokens[index].get("pos") == "名詞" and tokens[index].get("pos_sub1") == "数":
        index += 1
    if index >= len(tokens):
        return False
    counter = tokens[index].get("surface", "")
    return counter != "" and _same_counter_dimension(tail, counter[-1])


def _heads_nidan_cell(tokens: list[dict], index: int) -> bool:
    """Whether the token at ``index`` is the stem of a classical 二段 finite cell."""
    following = tokens[index + 1] if index + 1 < len(tokens) else None
    return nidan_cell(tokens[index], following) is not None


# Relative-time nouns after a duration keep their own boundary (3年+後, 5分+前).
_TEMPORAL_RELATION_SUFFIXES = frozenset({"前", "後"})


# Kanji closing a quantity phrase after the noun it measures (2段階+目, 3時限+目).
_QUANTITY_PHRASE_SUFFIXES = frozenset({"半", "目", "間"})


def _is_plain_kanji_noun(token: dict) -> bool:
    """Whether a token is an all-kanji common noun that can extend a quantity phrase."""
    return (
        token.get("pos") == "名詞"
        and token.get("pos_sub1") not in ("数", "代名詞", "固有名詞", "非自立")
        and regex.fullmatch(r"\p{Han}+", token.get("surface", "")) is not None
    )


def _absorb_unevenly_cut_kanji_run(
    tokens: list[dict], i: int, j: int, combined: str, last_was_counter: bool
) -> tuple[int, str]:
    """Extend a quantity phrase over a kanji run that its counter cuts unevenly.

    The kanji after a numeral are read in two-kanji words once the counter is
    taken off. When an odd number is left, the counter cut lands inside a word
    (2世+帯住宅, 3年+計画書, 3部+作), so the whole run is the search unit; an
    even remainder keeps the boundary (5人+家族, 24時間+営業). The counter is
    either already absorbed into the phrase or, when the dictionary emitted it
    inside a noun (2+世帯, 3+段階), the leading kanji of that noun that read as
    counters. A closing quantity-phrase suffix (目, 半) binds to the whole phrase
    and is not counted (2段階目), and relational 前/後 always stand alone (3年+後).
    """
    absorbed_counter = j > i + 1 and last_was_counter
    k = j
    while k < len(tokens) and _is_plain_kanji_noun(tokens[k]):
        k += 1
    run = "".join(tok["surface"] for tok in tokens[j:k])
    if not run or run[0] in _TEMPORAL_RELATION_SUFFIXES:
        return j, combined
    noun = tokens[j].get("surface", "")
    if absorbed_counter:
        counter_len = 0
    elif j == i + 1 and reads_as_counter(run[0], suffix_only=True):
        counter_len = 1
        while counter_len < len(noun) and reads_as_counter(noun[counter_len]):
            counter_len += 1
    else:
        return j, combined
    has_closing_suffix = len(run) > 1 and run[-1] in _QUANTITY_PHRASE_SUFFIXES
    counted = run[:-1] if has_closing_suffix else run
    remainder = len(counted) - counter_len
    if remainder % 2 == 1 or (remainder == 0 and has_closing_suffix):
        return k, combined + run
    # An even remainder after a noun made of counters alone still takes that
    # noun as the counter (3段階+評価).
    if not absorbed_counter and counter_len == len(noun):
        return j + 1, combined + noun
    return j, combined


def _kanji_noun_run(tokens: list[dict], start: int) -> tuple[int, str]:
    """Return the complete mergeable kanji-noun run beginning at ``start``."""
    if start >= len(tokens):
        return start, ""
    token = tokens[start]
    if not (
        regex.match(r"^[\p{Han}]+$", token.get("surface", ""))
        and token.get("pos") == "名詞"
        and token.get("pos_sub1", "") not in ("接尾", "固有名詞", "副詞可能")
        and token.get("surface", "") not in LETTER_FORMULAS
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
            and following.get("pos_sub1", "") not in ("接尾", "固有名詞", "形容動詞語幹", "副詞可能", "数", "代名詞")
        )
        # A pronoun is a phrase of its own (俺|失敗, 彼|仕事); it opens a
        # compound only with a bound one-kanji element (何+気).
        if surface in LETTER_FORMULAS:
            is_mergeable = False
        if is_mergeable and index == start + 1 and token.get("pos_sub1", "") == "代名詞":
            is_mergeable = len(surface) == 1
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
        and tokens[index + 1].get("surface", "").startswith(("た", "て", "ちゃ", "ちま"))
    ):
        return surface
    if (
        surface == "ら"
        and tail.get("pos") == "名詞"
        and tail.get("pos_sub1") == "接尾"
        and index + 1 < len(tokens)
        and (
            tokens[index + 1].get("surface") == "ない"
            or (tokens[index + 1].get("pos_sub1") == "接尾" and tokens[index + 1].get("lemma") in ("せる", "れる"))
        )
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


_SOUROU = "候"


def _retag_classical_sourou(tokens: list[dict]) -> list[dict]:
    """Read a standalone 候 as the epistolary verb 候ふ unless something modifies it.

    The dictionary knows 候 only as the noun of 時候 (新緑の候), which is a head
    that takes an adnominal modifier.  Everywhere else the character is the
    classical verb: after a continuative (申し+候), after a nominal predicate
    (御座+候), or opening a clause (候+て, 候+間).  A continuative the dictionary
    filed as a noun (存じ, 見え) is read back as the verb it is.
    """
    retagged: list[dict] = []
    for token in tokens:
        previous = retagged[-1] if retagged else None
        modified = previous is not None and (
            previous.get("pos") == "連体詞" or (previous.get("pos") == "助詞" and previous.get("pos_sub1") == "連体化")
        )
        # A suffix needs a nominal host, so behind the verb it is the formal noun
        # the predicate modifies (候+間).
        if previous is not None and previous.get("lemma") == "候ふ" and token.get("pos_sub1") == "接尾":
            retagged.append({**token, "pos_sub1": "非自立"})
            continue
        if token.get("surface") != _SOUROU or token.get("pos") != "名詞" or modified:
            retagged.append(token)
            continue
        if previous is not None and previous.get("pos") == "名詞":
            reading = _continuative_verb_tokens(previous.get("surface", ""))
            if reading is not None and len(reading) == 1:
                retagged[-1] = {**previous, **reading[0], "conj_form": "連用形"}
        retagged.append({"surface": _SOUROU, "pos": "動詞", "pos_sub1": "自立", "conj_form": "基本形", "lemma": "候ふ"})
    return retagged


def _is_verb_base(base: str) -> bool:
    """Whether a reconstructed kana dictionary form is a verb in a verbal frame.

    A bare kana form can be an adverb or a noun on its own (かく, のむ), so the
    probe places it before ことができる, which only a verb can precede.
    """
    probe = mecab_analyze(base + "ことができる")
    return bool(probe) and probe[0].get("surface") == base and probe[0].get("pos") == "動詞"


def apply_suzume_merge(tokens: list[dict], text: str) -> tuple[list[dict], str | None]:
    """Apply Suzume merge rules to MeCab tokens.

    Returns:
        Tuple of (merged tokens, applied rule name or None).
    """
    tokens = _retag_classical_sourou(_separate_counter_case_particles(tokens))
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
                continue

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
                continue

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

        # 1. Full date pattern
        if not merged:
            m = regex.match(r"^(\d+年\d+月\d+日)", remaining)
            if m:
                date = m.group(1)
                length, j = _covered_length(tokens, i, len(date))
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
                length, j = _covered_length(tokens, i, len(url))
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

        # The classical inferential めり takes a terminal predicate; the
        # dictionary has no entry and reads it as a noun め plus the perfect り,
        # which a noun cannot carry.  A predicate is never a nominal host, so the
        # denominal verb reading below does not apply after one either.
        if (
            not merged
            and t.get("surface") == "め"
            and t.get("pos") == "名詞"
            and i > 0
            and tokens[i - 1].get("pos") in ("動詞", "形容詞", "助動詞")
            and tokens[i - 1].get("conj_form") == "基本形"
            and i + 1 < len(tokens)
            and tokens[i + 1].get("surface") == "り"
            and tokens[i + 1].get("pos") == "助動詞"
        ):
            result.append({"surface": "めり", "pos": "助動詞", "lemma": "めり"})
            i += 2
            merged = True
            if applied_rule is None:
                applied_rule = "classical-meri"

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

        # The same verb before contracted てる/てた comes back as the quotative
        # って plus a classical る or a past た, which a quotation never takes
        # (沼+って+る): it is the sokuonbin 沼っ plus the progressive.
        if (
            not merged
            and t.get("pos") == "名詞"
            and t.get("pos_sub1") not in ("接尾", "代名詞")
            and i + 2 < len(tokens)
            and tokens[i + 1].get("surface") == "って"
            and tokens[i + 1].get("pos") == "助詞"
            and tokens[i + 2].get("pos") == "助動詞"
            and (
                (tokens[i + 2].get("surface") == "る" and tokens[i + 2].get("conj_type") == "文語・ル")
                or tokens[i + 2].get("surface") == "た"
            )
        ):
            stem = t.get("surface", "") + "っ"
            result.append({"surface": stem, "pos": "動詞", "lemma": stem[:-1] + "る"})
            progressive = "てる" if tokens[i + 2].get("surface") == "る" else "て"
            result.append({"surface": progressive, "pos": "動詞", "pos_sub1": "非自立", "lemma": "てる"})
            i += 3 if progressive == "てる" else 2
            merged = True
            if applied_rule is None:
                applied_rule = "denominal-ru-verb"

        # 2. Number + counter/katakana
        # 何 in front of a counter suffix fills the numeral slot — it is the
        # interrogative quantity, and there is no reading where a pronoun takes
        # a counter. The reference dictionary decides its subtype by position
        # rather than by that, calling it a numeral standing alone or after a
        # topic-marked subject and a pronoun elsewhere, which left the same
        # phrase merged in one sentence and split in the next.
        is_interrogative_quantity = (
            t.get("surface") == "何"
            and t.get("pos") == "名詞"
            and i + 1 < len(tokens)
            and tokens[i + 1].get("pos") == "名詞"
            and tokens[i + 1].get("pos_sub1") == "接尾"
        )
        # A calendar piece the dictionary emits whole (四月) opens the chain too
        # when a numeral follows it, as the digit spelling 4月1日 already does.
        opens_calendar_chain = (
            _COUNTER_CHAIN_UNIT.fullmatch(t.get("surface", "")) is not None
            and i + 1 < len(tokens)
            and tokens[i + 1].get("pos") == "名詞"
            and tokens[i + 1].get("pos_sub1") == "数"
        )
        if (
            not merged
            and t.get("pos") == "名詞"
            and (t.get("pos_sub1") == "数" or is_interrogative_quantity or opens_calendar_chain)
        ):
            j = i + 1
            combined = t.get("surface", "")
            last_was_counter = False
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
                # In context the reference dictionary demotes both halves at
                # once: 何 becomes a pronoun and its counter loses the 助数詞
                # subtype. Nothing but a counter follows 何 in the numeral slot,
                # so the suffix is one whatever subtype it was given.
                is_interrogative_counter = is_interrogative_quantity and j == i + 1 and np == "名詞" and ns1 == "接尾"
                is_span_kan = (
                    ns == "間" and np == "名詞" and ns1 == "接尾" and _DURATION_BEFORE_SPAN_KAN.search(combined)
                )
                is_calendar_month = ns == "月" and regex.match(r"^(?:1[0-2]|[1-9])$", combined)
                is_katakana_noun = np == "名詞" and regex.match(r"^[\u30A0-\u30FF]+$", ns)
                is_chuu_suffix = ns == "中" and np == "名詞" and ns1 == "接尾"
                is_me_suffix = ns == "目" and np == "名詞" and ns1 == "接尾"
                is_large_unit = np == "名詞" and ns1 == "数" and ns in ("万", "億", "兆")
                is_number_after_large = combined.endswith(("万", "億", "兆")) and np == "名詞" and ns1 == "数"
                # A quantity chains on only within one dimension (1時間30分,
                # 1泊2日); where the dimension changes the second quantity is a
                # rate over the first (1泊+5000円, 1日+3回).
                is_number_after_counter_chain = (
                    combined[-1:] in _COUNTER_CHAIN_TAILS
                    and np == "名詞"
                    and ns1 == "数"
                    and _continues_counter_dimension(combined[-1], tokens, j)
                )
                # IPADIC also emits calendar pieces such as 三月 as one
                # non-numeric token. It is still the next numeral+counter
                # member of a chain whose left member has already been read.
                is_compact_counter_chain_unit = (
                    combined[-1:] in _COUNTER_CHAIN_TAILS
                    and _COUNTER_CHAIN_UNIT.fullmatch(ns) is not None
                    and _same_counter_dimension(combined[-1], ns[-1])
                )
                is_number_after_decimal = combined.endswith(".") and np == "名詞" and ns1 == "数"
                is_counter_aux = ns == "つ" and np in ("助動詞", "動詞")
                is_percent = ns == "%"
                is_decimal = ns == "."
                is_consecutive_number = (
                    np == "名詞"
                    and ns1 == "数"
                    and regex.match(r"^[0-9０-９]+$", ns)
                    # A digit run cut into pieces, or the denominator of a
                    # fraction (3分の+1); a counter in between ends the quantity.
                    and regex.match(r"[0-9０-９,，.．の]", combined[-1:])
                )
                is_kanji_number_run = (
                    np == "名詞" and ns1 == "数" and regex.match(r"^[一二三四五六七八九十百千万億兆〇零]+$", ns)
                )
                is_alpha_unit = regex.match(r"^[A-Za-z]+$", ns) and np == "名詞"

                if any(
                    [
                        is_counter,
                        is_interrogative_counter,
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
                    last_was_counter = is_counter or is_interrogative_counter
                    if any([is_katakana_noun, is_chuu_suffix, is_me_suffix, is_counter_aux, is_percent, is_alpha_unit]):
                        # The ordinal 目 still closes a unit that ends the chain
                        # (2つ+目, 3ページ+目), as it does after a counter (3回目).
                        if (
                            (is_katakana_noun or is_counter_aux)
                            and j < len(tokens)
                            and tokens[j].get("surface") == "目"
                            and tokens[j].get("pos") == "名詞"
                        ):
                            combined += "目"
                            j += 1
                        break
                else:
                    break
            j, combined = _absorb_unevenly_cut_kanji_run(tokens, i, j, combined, last_was_counter)
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

        # The closed compound case particles (について, にあたって, に対して) stay
        # one word whatever follows, unless the follower selects the literal
        # verb's て-form (駅に|つい|て|から).
        if (
            not merged
            and result
            and i + 2 < len(tokens)
            and t.get("surface") == "に"
            and t.get("pos") == "助詞"
            and tokens[i + 1].get("pos") == "動詞"
            and tokens[i + 1].get("lemma") in _COMPOUND_CASE_PARTICLE_VERBS
            and tokens[i + 2].get("surface") == "て"
            and tokens[i + 2].get("pos") == "助詞"
            and (i + 3 >= len(tokens) or not _selects_te_form(tokens[i + 3]))
        ):
            span = "に" + tokens[i + 1].get("surface", "") + "て"
            if _reads_as_compound_case_particle(result[-1].get("surface", ""), span):
                result.append({"surface": span, "pos": "助詞", "pos_sub1": "格助詞", "pos_sub2": "連語", "lemma": span})
                i += 3
                merged = True
                if applied_rule is None:
                    applied_rule = "compound-case-particle"

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
            # A compound is right-headed, so the run's last piece decides
            # (最|重要 but 最重要課題).
            if noun_surface and tokens[noun_end - 1].get("pos_sub1") != "形容動詞語幹":
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

        # The obligation and prohibition frames (〜なあかん, 〜たらあかん) end in
        # the fixed あかん the reference already holds as one word; it splits it
        # into a literal あく+ん only when it fails to see the frame.
        if (
            not merged
            and t.get("surface") == "あか"
            and t.get("pos") == "動詞"
            and t.get("lemma") == "あく"
            and i + 1 < len(tokens)
            and tokens[i + 1].get("surface") == "ん"
            and tokens[i + 1].get("pos") == "助動詞"
            and result
            and (
                (result[-1].get("surface") == "な" and result[-1].get("lemma") == "ない")
                or result[-1].get("surface") in ("たら", "だら")
            )
        ):
            result.append({"surface": "あかん", "pos": "感動詞", "lemma": "あかん"})
            i += 2
            merged = True
            if applied_rule is None:
                applied_rule = "obligation-akan"

        # 3. Nai-adjective merge
        # After a nominal, でしょ+う+が+ない spelling で + しょうがない is the
        # continuative copula and the adjective (暇でしょうがない); the
        # reference reads the copula's conjectural cell across the boundary.
        if (
            not merged
            and t.get("surface") == "でしょ"
            and t.get("pos") == "助動詞"
            and result
            and result[-1].get("pos") in ("名詞", "Noun", "Adjective")
        ):
            for adj in NAI_ADJECTIVES:
                if not remaining.startswith("で" + adj):
                    continue
                length, j = _covered_length(tokens, i, len(adj) + 1)
                if length == len(adj) + 1 and tokens[j - 1].get("surface", "").startswith("な"):
                    # で keeps the tag the reference gives it after the same
                    # host elsewhere: copula after a na-stem, particle otherwise.
                    if result[-1].get("pos_sub1") == "形容動詞語幹":
                        result.append({"surface": "で", "pos": "助動詞", "lemma": "だ"})
                    else:
                        result.append({"surface": "で", "pos": "助詞", "pos_sub1": "格助詞", "lemma": "で"})
                    result.append({"surface": adj, "pos": "形容詞", "lemma": adj})
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = "copula+nai-adjective"
                    break

        if not merged:
            for adj in NAI_ADJECTIVES:
                if remaining.startswith(adj):
                    length, j = _covered_length(tokens, i, len(adj))
                    if length == len(adj):
                        result.append({"surface": adj, "pos": "形容詞", "lemma": adj})
                        i = j
                        merged = True
                        if applied_rule is None:
                            applied_rule = "nai-adjective"
                        break

        # An L2 kana i-adjective is lexical evidence for its whole inflected
        # cell, which the reference analyzer can split into a verb or a
        # different adjective plus ない (すく + なかっ for すくなかっ).
        if not merged:
            for adjective in kana_i_adjective_lemmas():
                stem = adjective[:-1]
                if not remaining.startswith(stem):
                    continue
                for ending in _I_ADJECTIVE_CELL_ENDINGS:
                    cell = stem + ending
                    if not remaining.startswith(cell):
                        continue
                    consumed, j = _consume_span(tokens, i, len(cell))
                    if consumed != cell or j == i + 1:
                        continue
                    result.append({"surface": cell, "pos": "形容詞", "lemma": adjective})
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = "l2-adjective"
                    break
                if merged:
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

        # 4c. A sokuon after the past auxiliary: the reference reads た+っ as
        # the verb たつ (or たる). Before the phrase end it is emphasis on the
        # past, which keeps its own token (来+たっ, やっ+たっ); before more kana
        # it geminates onto what follows (し+た+っちゃ, 泣い+た+っぴ).
        if (
            not merged
            and t.get("pos") in ("動詞", "助動詞", "形容詞")
            and "連用" in (t.get("conj_form") or "")
            and i + 1 < len(tokens)
            and tokens[i + 1].get("surface") == "たっ"
            and tokens[i + 1].get("pos") == "動詞"
        ):
            result.append(t)
            following = tokens[i + 2] if i + 2 < len(tokens) else None
            if following is None or following.get("pos") == "記号":
                result.append({"surface": "たっ", "pos": "助動詞", "lemma": "た"})
            else:
                result.append({"surface": "た", "pos": "助動詞", "lemma": "た"})
                geminated = dict(following)
                geminated["surface"] = "っ" + following.get("surface", "")
                if (following.get("lemma") or following.get("surface")) == following.get("surface"):
                    geminated["lemma"] = geminated["surface"]
                # Keep the running text offsets: the mark moves, it is not added.
                tokens[i + 1] = {**tokens[i + 1], "surface": "た"}
                tokens[i + 2] = geminated
            i += 2
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

        # 4c-3. The prohibitive な takes the terminal form, whose ichidan る
        # contracts to ん before it (見んなよ, 食べんなよ). The reference gives
        # most stems that cell but reads some (寝, 忘れ) as the irrealis plus the
        # negative ん; a phrase-final な (alone or before よ) is the prohibitive.
        if (
            not merged
            and t.get("pos") == "動詞"
            and t.get("conj_type") == "一段"
            and "未然" in (t.get("conj_form") or "")
            and i + 2 < len(tokens)
            and tokens[i + 1].get("surface") == "ん"
            and tokens[i + 1].get("pos") == "助動詞"
            and tokens[i + 2].get("surface") == "な"
            and tokens[i + 2].get("pos_sub1") == "終助詞"
        ):
            after = tokens[i + 3] if i + 3 < len(tokens) else None
            if after is None or after.get("pos") == "記号" or after.get("surface") == "よ":
                stem = t.get("surface", "")
                result.append({**t, "surface": stem + "ん", "conj_form": "体言接続特殊"})
                i += 2
                merged = True
                if applied_rule is None:
                    applied_rule = "ichidan-prohibitive-contraction"

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
                consumed, j = _covered_length(tokens, i, len(source_span))
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

        # っぽい after a verb continuative derives a dispositional adjective
        # (飽きっぽい, 怒りっぽい) with no boundary inside; the continuative is
        # often tagged a noun, so its reading is recovered by probe.
        if (
            not merged
            and i + 1 < len(tokens)
            and tokens[i + 1].get("pos") == "形容詞"
            and tokens[i + 1].get("lemma") == "っぽい"
            and (
                (t.get("pos") == "動詞" and t.get("conj_form") == "連用形")
                or (t.get("pos") == "名詞" and reads_as_continuative(t.get("surface", "")))
            )
        ):
            host = t.get("surface", "")
            nxt = tokens[i + 1]
            result.append({"surface": host + nxt.get("surface", ""), "pos": "形容詞", "lemma": host + "っぽい"})
            i += 2
            merged = True
            if applied_rule is None:
                applied_rule = "continuative+ppoi-adjective"

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
                        consumed, j = _covered_length(tokens, i, len(candidate))
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

        # 8b. Character speech: にゃ + ー* + ん? is one sentence-final particle
        # (にゃ, にゃー, にゃーん, にゃん). It runs before the prolonged-mark merge
        # so the marks and the closing ん stay inside the particle.
        if not merged and t.get("surface") == "にゃ":
            j = i + 1
            particle = "にゃ"
            while j < len(tokens) and regex.fullmatch(r"ー+", tokens[j].get("surface", "")):
                particle += tokens[j].get("surface", "")
                j += 1
            if j < len(tokens) and tokens[j].get("surface") == "ん":
                particle += "ん"
                j += 1
            if j > i + 1:
                # A mark held inside the particle (にゃーん) is emphasis on にゃん.
                lemma = regex.sub(r"ー+", "", particle) if particle.endswith("ん") else particle
                result.append({"surface": particle, "pos": "助詞", "lemma": lemma})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "character-speech"

        # 8c. Role-language copulas (ざます, やんす, ござんす, っス) are split by the
        # reference into homographic pieces (ざま+す, やん+す, っ+ス). Each closes
        # a nominal or a predicate as one auxiliary, so it is taken only there
        # and only where the utterance, a phrase or a final particle follows.
        if not merged and result:
            copula = next((word for word in _CHARACTER_SPEECH_COPULAS if remaining.startswith(word)), "")
            host = result[-1]
            if (
                copula
                and _closes_utterance(remaining[len(copula) :])
                and (host.get("pos") in _COPULA_HOST_POS or host.get("surface") == "で")
            ):
                consumed, j = _covered_length(tokens, i, len(copula))
                if consumed == len(copula):
                    if host.get("surface") == "で":
                        # The で in front is the copula's continuative, which
                        # the reference reads as the case particle here.
                        result[-1] = {**host, "pos": "助動詞", "pos_sub1": None, "lemma": "だ"}
                    result.append({"surface": copula, "pos": "助動詞", "lemma": CHARACTER_SPEECH_COPULAS[copula]})
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = "character-speech"

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
                if (previous_surface == "の" and following_surface == "は") or (
                    adverb[:-1] in core_headwords("nouns.tsv") | core_headwords("adjectives.tsv")
                    or adverb[:-1] in ADVERBIAL_NA_ADJECTIVES
                ):
                    derived_tari = None
            if derived_tari is not None:
                adverb = derived_tari.group(0)
                length, j = _covered_length(tokens, i, len(adverb))
                if length == len(adverb):
                    result.append({"surface": adverb, "pos": "副詞", "lemma": adverb[:-1]})
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = "tari-adverb"

        # Reduplication is the other productive source of this class (淡々と,
        # 着々と, 深々と). MeCab already reads the reduplicated stem as an adverb
        # where it knows the word, so that tag is what separates the taru form
        # from a plural whose と is a case or conjunctive particle: 人々と話す and
        # 山々と川 keep 名詞-一般 and stay split. Stems MeCab does not know at all
        # fall through to the list below.
        if not merged and t.get("pos") == "副詞":
            adverb_surface = t.get("surface", "")
            if (
                adverb_surface.endswith("々")
                and remaining.startswith(adverb_surface + "と")
                and i + 1 < len(tokens)
                and tokens[i + 1].get("pos") == "助詞"
            ):
                result.append({"surface": adverb_surface + "と", "pos": "副詞", "lemma": adverb_surface})
                i += 2
                merged = True
                if applied_rule is None:
                    applied_rule = "tari-adverb"
            elif adverb_surface.endswith("々と"):
                # MeCab reads a handful of these as one adverb already, and cites
                # them with the connective attached. The stem is the citation form
                # everywhere else in this class, so a taru adverb gets one lemma
                # however it was reached.
                result.append({"surface": adverb_surface, "pos": "副詞", "lemma": adverb_surface[:-1]})
                i += 1
                merged = True
                if applied_rule is None:
                    applied_rule = "tari-adverb"

        if not merged:
            for stem in TARI_ADVERB_STEMS:
                adverb = stem + "と"
                if remaining.startswith(adverb):
                    length, j = _covered_length(tokens, i, len(adverb))
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
        # The host keeps the verb tag only while what follows selects the
        # continuative cell: with the copula behind it the same 動き comes back
        # as a nominal and the derivation is lost, so the reading is recovered
        # from the polite auxiliary rather than read off the tag.
        if not merged and (
            (t.get("pos") == "動詞" and t.get("conj_form") == "連用形")
            or (t.get("pos") == "名詞" and reads_as_continuative(t.get("surface", "")))
        ):
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

        # 5a''. Noun + adjective-forming めかしい (艶めかしい, 古めかしい). The
        # reference holds a few as single adjectives and reads the rest as the
        # verb めかす plus a stray いる; like がましい, the host plus the suffix
        # is one adjective.
        if (
            not merged
            and t.get("pos") == "名詞"
            and t.get("pos_sub1") != "接尾"
            and i + 2 < len(tokens)
            and tokens[i + 1].get("surface") == "めかし"
            and tokens[i + 1].get("pos") == "動詞"
            and tokens[i + 2].get("surface") == "い"
        ):
            adjective = t.get("surface", "") + "めかしい"
            result.append({"surface": adjective, "pos": "形容詞", "lemma": adjective})
            i += 3
            merged = True
            if applied_rule is None:
                applied_rule = "noun+mekashii-adjective"

        # 5a'''. A noun + verb-forming derivational suffix (謎めく, 冗談めかす)
        # keeps the boundary between host and suffix, but when the cell the
        # suffix stands in is one the reference dictionary has no entry for, it
        # cuts a bound suffix out of the suffix's own material and reads
        # whatever is left as some other word (謎 + め as a suffix + きたる as an
        # adnominal). The paradigm is found across the fragments, and the
        # bound-suffix tag is the evidence that the cut went through the
        # derivation: an ordinary noun after a noun is left alone.
        if (
            not merged
            and t.get("pos") == "名詞"
            and t.get("pos_sub1") != "接尾"
            and i + 1 < len(tokens)
            and tokens[i + 1].get("pos_sub1") == "接尾"
        ):
            joined = ""
            for span in range(i + 1, min(i + 1 + DERIVED_VERB_FRAGMENT_SPAN, len(tokens))):
                joined += tokens[span].get("surface", "")
                form = max(
                    (form for form in DERIVED_VERB_SUFFIX_FORMS if joined.startswith(form)),
                    key=len,
                    default="",
                )
                if not form:
                    continue
                # The host of the derivation is nominal even where the
                # reference lists the word as a na-adjective stem (皮肉).
                result.append({**t, "pos_sub1": "一般"})
                result.append({"surface": form, "pos": "動詞", "lemma": DERIVED_VERB_SUFFIX_FORMS[form]})
                result.extend(mecab_analyze(joined[len(form) :]))
                i = span + 1
                merged = True
                if applied_rule is None:
                    applied_rule = "noun+derived-verb-suffix"
                break

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
                # A na-adjective stem is a predicate base, not a nominal a place
                # name compounds with. Letting it through built a proper noun out
                # of an intensifying prefix the dictionary happens to hold as a
                # place name and the predicate behind it, which is also why the
                # same prefix stayed apart from an adjective and from a stem the
                # dictionary tags as a prefix instead.
                is_kanji_noun = (
                    nxt.get("pos") == "名詞"
                    and regex.match(r"^[\p{Han}]+$", ns)
                    and nxt.get("pos_sub1") not in ("接尾", "形容動詞語幹")
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
                consumed, j = _covered_length(tokens, i + 1, len(body))
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
                    length, j = _covered_length(tokens, i, len(pronoun))
                    if length == len(pronoun):
                        result.append({"surface": pronoun, "pos": "代名詞", "lemma": pronoun})
                        i = j
                        merged = True
                        if applied_rule is None:
                            applied_rule = "colloquial-pronoun"
                        break

        # 8e. The reference reads Xらしげ as the plural suffix ら plus a name
        # しげ (大人+ら+しげ) unless context steers it to らし+げ (子供+らし+げ).
        # ら directly before しげ is the stem of らしい with the suffix げ.
        if (
            not merged
            and t.get("surface") == "ら"
            and t.get("pos_sub1") == "接尾"
            and i + 1 < len(tokens)
            and tokens[i + 1].get("surface") == "しげ"
        ):
            result.append({"surface": "らし", "pos": "助動詞", "conj_form": "ガル接続", "lemma": "らしい"})
            result.append({"surface": "げ", "pos": "名詞", "pos_sub1": "接尾", "lemma": "げ"})
            i += 2
            merged = True
            if applied_rule is None:
                applied_rule = "degree-suffix-ge"

        # 8d. The pejorative auxiliary やがる takes a verb continuative (来+やがっ+た,
        # 待たせ+やがっ+て). The reference cuts it into や (read as a particle, the
        # copula or a cell of やる) and a がる suffix, or a noun for がれ; each
        # cut is put back into one auxiliary after the continuative.
        if (
            not merged
            and (
                (t.get("pos") == "動詞" and "連用" in (t.get("conj_form") or ""))
                or (t.get("pos") == "名詞" and reads_as_continuative(t.get("surface", "")))
            )
            and i + 2 < len(tokens)
            and tokens[i + 1].get("surface") == "や"
            and _YAGARU_TAIL.fullmatch(tokens[i + 2].get("surface", ""))
        ):
            if t.get("pos") == "名詞":
                t = {
                    "surface": t["surface"],
                    "pos": "動詞",
                    "lemma": base_from_renyokei(t["surface"]) or t.get("lemma"),
                }
            result.append(t)
            result.append({"surface": "や" + tokens[i + 2]["surface"], "pos": "助動詞", "lemma": "やがる"})
            i += 3
            merged = True
            if applied_rule is None:
                applied_rule = "pejorative-yagaru"

        # 9. Compound verbs
        following_source = remaining[len(t.get("surface", "")) :]
        begins_fixed_subsidiary = any(following_source.startswith(form) for form in _FIXED_INFLECTED_FUNCTION_UNITS)
        v1_surface = t.get("surface", "")
        # A compound verb's first member is an independent verb. The voice
        # auxiliaries share the 動詞 tag but carry 接尾, and letting them through
        # builds a headword out of an auxiliary and the subsidiary that follows
        # it (れ続ける), which drops the passive from the analysis entirely.
        # A subsidiary verb (て+いただき, て+み) is bound to the te-form before it
        # and heads no compound either (いただき+重ね, not いただき重ねる).
        v1_verb_renyokei = (
            t.get("pos") == "動詞"
            and t.get("pos_sub1") not in ("接尾", "非自立")
            and "連用" in (t.get("conj_form") or "")
        )
        # MeCab frequently lexicalizes a bare renyokei as a noun (座り, 入り).
        # Reconstructing its base is a productive morphology check; the closed
        # V2 class below prevents this from becoming an unrestricted noun+verb
        # merge rule.
        # A dependent noun keeps its own boundary: 〜たきり is the formal noun, not
        # a nominalized 切り, however well it reconstructs as one.
        # A pronoun keeps its own boundary for the same reason and a stronger
        # one: it is a closed-class deictic naming no event, so it heads no
        # compound verb — however well its surface reconstructs as a
        # continuative (それ off 逸れる, あれ off 荒れる). The reference dictionary
        # reaches that reading only when the particle after the pronoun is
        # dropped, which is where the merge produced a headword that is not a
        # word.
        v1_nominal_renyokei = (
            t.get("pos") == "名詞"
            and t.get("pos_sub1") not in ("非自立", "代名詞")
            and base_from_renyokei(v1_surface) is not None
        )
        # A kana V1 spelling a whole i-adjective (いい) is that adjective, and the
        # やっ+て after it is the exclamatory や plus the quotative って.
        v1_is_kana_adjective = regex.fullmatch(r"\p{Hiragana}+", v1_surface) is not None and _is_single_i_adjective(
            v1_surface
        )
        if (
            not merged
            and v1_is_kana_adjective
            and t.get("pos") == "動詞"
            and i + 2 < len(tokens)
            and tokens[i + 1].get("surface") == "やっ"
            and tokens[i + 2].get("surface") == "て"
        ):
            result.extend(
                (
                    {"surface": v1_surface, "pos": "形容詞", "lemma": v1_surface},
                    {"surface": "や", "pos": "助詞", "pos_sub1": "終助詞", "lemma": "や"},
                    {"surface": "って", "pos": "助詞", "pos_sub1": "格助詞", "lemma": "って"},
                )
            )
            i += 3
            merged = True
            if applied_rule is None:
                applied_rule = "adjective-ya-quotative"
        if (
            not merged
            and not begins_fixed_subsidiary
            and not v1_is_kana_adjective
            and (v1_verb_renyokei or v1_nominal_renyokei)
        ):
            j = i + 1
            if j < len(tokens):
                nxt = tokens[j]
                # A few second members are homographs of an adnominal, and the
                # dictionary picks that tag by position — the same compound
                # merges after a case-marked subject and splits inside a
                # relative clause. Membership of the closed second-member class
                # below is the evidence; the tag it arrived with is not.
                v2_is_adnominal_homograph = nxt.get("pos") == "連体詞" and (
                    nxt.get("surface", "") in _PRODUCTIVE_COMPOUND_V2
                )
                if (nxt.get("pos") == "動詞" or v2_is_adnominal_homograph) and (
                    nxt.get("lemma") or nxt.get("surface", "")
                ) != "でる":
                    next_lemma = nxt.get("lemma") or nxt.get("surface", "")
                    v2_base = next_lemma if next_lemma in _PRODUCTIVE_COMPOUND_V2 else ""
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
        v1_nominal_renyokei = (
            t.get("pos") == "名詞" and t.get("pos_sub1") != "代名詞" and base_from_renyokei(v1_surface) is not None
        )
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

    return apply_merge_postprocessors(result, applied_rule)
