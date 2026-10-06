"""Merge steps for dates, URLs, reduplications, counters, and numbered nouns."""

import functools

import regex

from .constants import FAMILY_TERMS, TEMPORAL_COMPOUND_UNITS, TEMPORAL_PREFIX_KANJI
from .mecab import mecab_analyze, reads_as_counter
from .merge_rules_helpers import _covered_length, _kanji_noun_run
from .merge_rules_state import MergeState

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


# Numeric-approximation/aggregation prefixes that modify a whole quantity and split
# off the following number+counter (約|二時間, 計|五名), unlike ordinal 第 which binds
# to its number (第三十四|回). Mirrors normalize::isNumericApproxPrefixKanji in the core.
_APPROX_NUMERIC_PREFIXES = {"約", "計", "総"}


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


def _merge_dates_and_literals(state: MergeState) -> bool:
    """Dates, family terms, temporal adverbs, and URLs."""
    tokens = state.tokens
    result = state.result
    i = state.i
    t = state.t
    merged = state.merged
    applied_rule = state.applied_rule
    remaining = state.remaining
    try:
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
    finally:
        state.i = i
        state.merged = merged
        state.applied_rule = applied_rule
    return False


def _merge_reduplications_and_derived_verbs(state: MergeState) -> bool:
    """Reduplications, durations, and noun-derived verbs."""
    tokens = state.tokens
    result = state.result
    i = state.i
    t = state.t
    merged = state.merged
    applied_rule = state.applied_rule
    try:
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

        # The counterfactual まし has the 已然/未然 ましか before ば (見ましかば);
        # the reference reads the か as a particle after the terminal まし.
        if (
            not merged
            and t.get("surface") == "まし"
            and t.get("pos") == "助動詞"
            and i + 2 < len(tokens)
            and tokens[i + 1].get("surface") == "か"
            and tokens[i + 2].get("surface") == "ば"
            and tokens[i + 2].get("pos") == "助詞"
        ):
            result.append({"surface": "ましか", "pos": "助動詞", "lemma": "まし"})
            i += 2
            merged = True
            if applied_rule is None:
                applied_rule = "classical-mashika"

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
    finally:
        state.i = i
        state.merged = merged
        state.applied_rule = applied_rule
    return False


def _merge_counter_expressions(state: MergeState) -> bool:
    """Numbers followed by counters, units, or katakana."""
    tokens = state.tokens
    result = state.result
    i = state.i
    t = state.t
    merged = state.merged
    applied_rule = state.applied_rule
    try:
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
    finally:
        state.i = i
        state.merged = merged
        state.applied_rule = applied_rule
    return False


def _merge_numbered_and_prefixed_nouns(state: MergeState) -> bool:
    """Addresses, prefixed numbers, suffixed nouns, and versions."""
    tokens = state.tokens
    result = state.result
    i = state.i
    t = state.t
    merged = state.merged
    applied_rule = state.applied_rule
    try:
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
    finally:
        state.i = i
        state.merged = merged
        state.applied_rule = applied_rule
    return False
