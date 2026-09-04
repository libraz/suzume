"""Focused post-processing passes used by the MeCab merge pipeline."""

from functools import cache

import regex

from .constants import (
    BOUND_SUFFIX_VERB_NOUN_CELLS,
    CLASSICAL_ADJECTIVE_LEMMA_OVERRIDES,
    HISTORICAL_KANA_RESPELLING,
    HONORIFIC_EXCEPTIONS,
    HONORIFIC_FRAME_TAILS,
    HONORIFIC_SUFFIXES,
    KANJI_PREFIX_COMPOUNDS,
    PREFIX_EXCEPTIONS,
    SEARCH_UNIT_COMPOUNDS,
)
from .mecab import is_single_token_of_pos, mecab_analyze

_IDEOGRAPHIC_SEQUENCE = regex.compile(r"^[\p{Han}\uFE00-\uFE0F\U000E0100-\U000E01EF]+$")


def _postprocess_kamo(result: list[dict], applied_rule: str | None) -> list[dict]:
    """Merge か+も -> かも (compound particle)."""
    merged = []
    skip_next = False
    for j, curr in enumerate(result):
        if skip_next:
            skip_next = False
            continue
        if (
            j < len(result) - 1
            and curr.get("surface") == "か"
            and curr.get("pos") == "助詞"
            and result[j + 1].get("surface") == "も"
            and result[j + 1].get("pos") == "助詞"
        ):
            merged.append({"surface": "かも", "pos": "助詞", "lemma": "かも"})
            skip_next = True
        else:
            merged.append(curr)
    return merged


def _postprocess_totomoni(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Merge the kanji spelling of the parallel compound particle と共に."""
    merged = []
    idx = 0
    while idx < len(result):
        if (
            idx + 1 < len(result)
            and result[idx].get("surface") == "と"
            and result[idx].get("pos") == "助詞"
            and result[idx + 1].get("surface") == "共に"
            and result[idx + 1].get("pos") == "副詞"
        ):
            merged.append({"surface": "と共に", "pos": "助詞", "lemma": "と共に"})
            idx += 2
            if applied_rule is None:
                applied_rule = "totomoni-compound-particle"
            continue
        if (
            idx + 2 < len(result)
            and result[idx].get("surface") == "と"
            and result[idx].get("pos") == "助詞"
            and result[idx + 1].get("surface") == "共"
            and result[idx + 2].get("surface") == "に"
            and result[idx + 2].get("pos") == "助詞"
        ):
            merged.append({"surface": "と共に", "pos": "助詞", "lemma": "と共に"})
            idx += 3
            if applied_rule is None:
                applied_rule = "totomoni-compound-particle"
            continue
        merged.append(result[idx])
        idx += 1
    return merged, applied_rule


#: The progressive subsidiary in both its spellings (食べている, 食べてる).
_PROGRESSIVE_LEMMAS = frozenset({"いる", "てる"})


def _postprocess_noni(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Merge の+に -> のに after a host that admits only the concessive reading.

    Concessive のに attaches to an attributive form, so the host decides whether
    the two morae are one particle or the nominalizer plus a goal marker. After
    an auxiliary — 学生な, 静かな, 読んだ — only the concessive reading survives,
    and an i-adjective closes the same cell (東京が寒いのに). A verb host is left
    split because both readings stay open there: 読むのに覚えられない is concessive
    but 読むのに時間がかかる is the goal, and nothing in the form separates them.

    The progressive is the one verbal host that decides it. The dictionary reads
    てる and いる as subsidiary verbs rather than auxiliaries, so they miss the
    test above, but the goal reading nominalizes an action and a progressive
    names an ongoing state instead — 食べてるのに is concessive wherever it
    stands. The other subsidiaries stay out: 食べておくのに時間がかかる still
    nominalizes an action.
    """
    merged = []
    skip_next = False
    for j, curr in enumerate(result):
        if skip_next:
            skip_next = False
            continue
        host = result[j - 1] if j >= 1 else {}
        host_is_attributive = (
            host.get("surface", "") in ("た", "っ", "だ")
            or host.get("pos") == "助動詞"
            or (
                host.get("pos") == "動詞"
                and host.get("pos_sub1") == "非自立"
                and host.get("lemma") in _PROGRESSIVE_LEMMAS
            )
            or (host.get("pos") == "形容詞" and host.get("surface", "").endswith("い"))
        )
        if (
            j >= 1
            and j < len(result) - 1
            and curr.get("surface") == "の"
            and result[j + 1].get("surface") == "に"
            and host_is_attributive
        ):
            merged.append({"surface": "のに", "pos": "助詞", "lemma": "のに"})
            skip_next = True
            if applied_rule is None:
                applied_rule = "noni-merge"
        else:
            merged.append(curr)
    return merged, applied_rule


def _postprocess_atode(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Split 後で(副詞) -> 後+で."""
    new_result = []
    for t in result:
        if t.get("surface") == "後で" and t.get("pos") == "副詞":
            new_result.append({"surface": "後", "pos": "名詞", "lemma": "後"})
            new_result.append({"surface": "で", "pos": "助詞", "lemma": "で"})
            if applied_rule is None:
                applied_rule = "atode-split"
        else:
            new_result.append(t)
    return new_result, applied_rule


def _postprocess_epenthetic_sa(result: list[dict]) -> None:
    """Fix epenthetic さ in adjective+さ+そう pattern."""
    for j in range(1, len(result) - 1):
        prev = result[j - 1]
        curr = result[j]
        nxt = result[j + 1]
        if (
            prev.get("pos") == "形容詞"
            and curr.get("surface") == "さ"
            and curr.get("pos_sub1") == "接尾"
            and nxt.get("surface") == "そう"
        ):
            curr["pos"] = "Suffix"
            curr.pop("pos_sub1", None)
            curr.pop("pos_sub2", None)


def _postprocess_honorific_split(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Restore productive honorific boundaries hidden by lexicalized tokens."""
    honorific_re = "|".join(regex.escape(s) for s in HONORIFIC_SUFFIXES)

    new_result = []
    for t in result:
        surface = t.get("surface", "")
        if t.get("pos") == "動詞":
            predicate = regex.match(r"^(お|ご)([\p{Han}]+)(に)([\p{Hiragana}]+)$", surface)
            if predicate:
                prefix, noun, particle, verb_surface = predicate.groups()
                lemma_match = regex.match(
                    rf"^{regex.escape(prefix + noun + particle)}([\p{{Hiragana}}]+)$",
                    t.get("lemma", ""),
                )
                verb_lemma = lemma_match.group(1) if lemma_match else verb_surface
                new_result.extend(
                    [
                        {"surface": prefix, "pos": "接頭詞", "lemma": prefix},
                        {"surface": noun, "pos": "名詞", "lemma": noun},
                        {"surface": particle, "pos": "助詞", "lemma": particle},
                        {"surface": verb_surface, "pos": "動詞", "lemma": verb_lemma},
                    ]
                )
                if applied_rule is None:
                    applied_rule = "honorific-predicate-split"
                continue
            separated_predicate = regex.match(r"^([\p{Han}]+)(に)([\p{Han}\p{Hiragana}]+)$", surface)
            has_honorific_prefix = (
                new_result and new_result[-1].get("pos") == "接頭詞" and new_result[-1].get("surface") in {"お", "ご"}
            )
            if separated_predicate and has_honorific_prefix:
                noun, particle, verb_surface = separated_predicate.groups()
                lemma_match = regex.match(
                    rf"^{regex.escape(noun + particle)}([\p{{Han}}\p{{Hiragana}}]+)$",
                    t.get("lemma", ""),
                )
                verb_lemma = lemma_match.group(1) if lemma_match else verb_surface
                new_result.extend(
                    [
                        {"surface": noun, "pos": "名詞", "lemma": noun},
                        {"surface": particle, "pos": "助詞", "lemma": particle},
                        {"surface": verb_surface, "pos": "動詞", "lemma": verb_lemma},
                    ]
                )
                if applied_rule is None:
                    applied_rule = "honorific-predicate-split"
                continue
        # An honorific attaches to a term of address, which IPADIC tags 名詞-一般
        # or 名詞-固有名詞. A na-adjective stem never is one, so a 形容動詞語幹
        # ending in さま carries the 様 of manner (逆さま) rather than the
        # honorific, and its boundary is word-internal.
        is_na_adjective_stem = t.get("pos_sub1") == "形容動詞語幹"
        if surface not in HONORIFIC_EXCEPTIONS and not is_na_adjective_stem:
            m = regex.match(rf"^(お)?([\p{{Han}}]+)({honorific_re})$", surface)
            if m:
                prefix, kanji, suffix = m.group(1), m.group(2), m.group(3)
                if prefix:
                    new_result.append({"surface": prefix, "pos": "接頭詞", "lemma": prefix})
                new_result.append({"surface": kanji, "pos": "名詞", "lemma": kanji})
                new_result.append({"surface": suffix, "pos": "名詞", "pos_sub1": "接尾", "lemma": suffix})
                if applied_rule is None:
                    applied_rule = "honorific-split"
                continue
        new_result.append(t)
    return new_result, applied_rule


def _postprocess_prefix_split(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Split お/ご+noun patterns.

    The prefix is separable when the remainder carries a kanji, which is what
    an honorific written over a free noun looks like (お仕事, ご意見). An
    all-hiragana remainder is part of the lexeme far more often than not
    (おなか, おだやか, おごそか, おかず) and its leading mora is not a prefix at
    all, so it only separates inside the humble/honorific frame that requires a
    verb stem after the prefix (ご迷惑をおかけする).
    """
    new_result = []
    for index, t in enumerate(result):
        surface = t.get("surface", "")
        pos = t.get("pos", "")
        pos_sub1 = t.get("pos_sub1", "")
        if pos == "名詞" and pos_sub1 != "接尾" and surface not in PREFIX_EXCEPTIONS:
            m = regex.match(r"^(お|ご)([\p{Han}\p{Hiragana}]+)$", surface)
            following = result[index + 1].get("surface", "") if index + 1 < len(result) else ""
            separable = regex.search(r"\p{Han}", m.group(2)) is not None if m else False
            if m and (separable or following in HONORIFIC_FRAME_TAILS):
                prefix, noun = m.group(1), m.group(2)
                new_result.append({"surface": prefix, "pos": "接頭詞", "lemma": prefix})
                new_result.append({"surface": noun, "pos": "名詞", "lemma": noun})
                if applied_rule is None:
                    applied_rule = "prefix-split"
                continue
        new_result.append(t)
    return new_result, applied_rule


# Inflections that can only follow the adjective-forming suffix がましい, never
# the verb 増す. 増す is also written in kanji here, so the kana surface below
# already separates the two; these tails are the second, independent check.
_GAMASHII_TAILS: tuple[tuple[str, str], ...] = (
    ("い", "動詞"),
    ("さ", "名詞"),
)


def _postprocess_gamashii(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Rebuild the adjective-forming suffix がましい from its scattered pieces.

    The reference dictionary holds the lexicalized derivations (押しつけがましい,
    未練がましい) as single adjectives but has no entry for the productive rest,
    where it reads the が as a case particle and まし as the verb 増す
    (恩/着せ/が/まし/さ). The host plus がましい is one adjective either way.
    """
    new_result: list[dict] = []
    index = 0
    while index < len(result):
        tail = result[index + 2] if index + 2 < len(result) else None
        matched_tail = (
            next(
                (
                    surface
                    for surface, pos in _GAMASHII_TAILS
                    if tail is not None and tail.get("surface") == surface and tail.get("pos") == pos
                ),
                "",
            )
            if tail is not None
            else ""
        )
        if (
            matched_tail
            and result[index].get("surface") == "が"
            and result[index].get("pos") == "助詞"
            and result[index + 1].get("surface") == "まし"
            and result[index + 1].get("pos") == "動詞"
            and new_result
        ):
            # The host is whatever precedes the particle: a bare noun
            # (言い訳がましい) or a noun plus a continuative verb (恩着せがましい).
            host_size = (
                2
                if len(new_result) >= 2
                and new_result[-1].get("pos") == "動詞"
                and new_result[-1].get("conj_form") == "連用形"
                and new_result[-2].get("pos") == "名詞"
                else 1
            )
            host = "".join(t.get("surface", "") for t in new_result[-host_size:])
            del new_result[-host_size:]
            stem = f"{host}がまし"
            lemma = f"{stem}い"
            if matched_tail == "い":
                new_result.append({"surface": lemma, "pos": "形容詞", "lemma": lemma})
            else:
                new_result.append({"surface": stem, "pos": "形容詞", "lemma": lemma})
                new_result.append({"surface": "さ", "pos": "名詞", "pos_sub1": "接尾", "lemma": "さ"})
            index += 3
            if applied_rule is None:
                applied_rule = "gamashii-adjective"
            continue
        new_result.append(result[index])
        index += 1
    return new_result, applied_rule


def _postprocess_demo_copula(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Keep nominal でも as one adverbial particle independent of its predicate.

    The reference lattice sometimes exposes the same construction as 格助詞
    で + 係助詞 も and sometimes as the compound 副助詞 でも. The following
    open-class predicate cannot change that grammatical boundary, so normalize
    both representations to the latter.
    """
    new_result: list[dict] = []
    index = 0
    while index < len(result):
        token = result[index]
        following = result[index + 1] if index + 1 < len(result) else None
        preceding = new_result[-1] if new_result else None
        if (
            token.get("surface") == "で"
            and token.get("pos_sub1") == "格助詞"
            and following is not None
            and following.get("surface") == "も"
            and following.get("pos_sub1") == "係助詞"
            and preceding is not None
            and preceding.get("pos_sub1") != "形容動詞語幹"
        ):
            new_result.append({"surface": "でも", "pos": "助詞", "pos_sub1": "副助詞", "lemma": "でも"})
            index += 2
            if applied_rule is None:
                applied_rule = "demo-adverbial-particle"
            continue
        new_result.append(token)
        index += 1
    return new_result, applied_rule


def _postprocess_nde_split(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Split contracted んでる verb forms."""
    new_result = []
    for t in result:
        surface = t.get("surface", "")
        pos = t.get("pos", "")
        m = regex.match(r"^(.+ん)(で)(る)$", surface)
        if pos == "動詞" and m:
            stem, de, ru = m.group(1), m.group(2), m.group(3)
            new_result.append({"surface": stem, "pos": "動詞", "lemma": t.get("lemma") or stem})
            new_result.append({"surface": de, "pos": "助詞", "lemma": de})
            new_result.append({"surface": ru, "pos": "動詞", "lemma": "いる"})
            if applied_rule is None:
                applied_rule = "nde-contract-split"
        else:
            new_result.append(t)
    return new_result, applied_rule


def _postprocess_filler_split(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Split filler tokens like そうですね -> そう+です+ね."""
    new_result = []
    for t in result:
        surface = t.get("surface", "")
        pos = t.get("pos", "")
        m = regex.match(r"^(そう)(です)(ね|か|よ|よね)?$", surface)
        if pos == "フィラー" and m:
            sou, desu, particle = m.group(1), m.group(2), m.group(3)
            new_result.append({"surface": sou, "pos": "名詞", "pos_sub1": "形容動詞語幹", "lemma": sou})
            new_result.append({"surface": desu, "pos": "助動詞", "lemma": "です"})
            if particle:
                new_result.append({"surface": particle, "pos": "助詞", "lemma": particle})
            if applied_rule is None:
                applied_rule = "filler-split"
        else:
            new_result.append(t)
    return new_result, applied_rule


def _postprocess_kuruwa(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Fix kuruwa kotoba segmentation: あ+りん -> あり+ん."""
    new_result = []
    skip_next = False
    for j, curr in enumerate(result):
        if skip_next:
            skip_next = False
            continue
        if curr.get("surface") == "あ" and j + 1 < len(result) and result[j + 1].get("surface") == "りん":
            new_result.append({"surface": "あり", "pos": "動詞", "lemma": "ある"})
            new_result.append({"surface": "ん", "pos": "助動詞", "lemma": "ん"})
            skip_next = True
            if applied_rule is None:
                applied_rule = "kuruwa-fix"
        else:
            new_result.append(curr)
    return new_result, applied_rule


def _postprocess_adj_bungo(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Fix archaic adjective form: 恐し + いとも → 恐しい + と + も.

    When 形容詞 in 文語基本形 is followed by a token starting with い + particles,
    merge い back into the adjective and split out the particles.
    """
    new_result: list[dict] = []
    skip_next = False
    for j, curr in enumerate(result):
        if skip_next:
            skip_next = False
            continue
        if curr.get("pos") == "形容詞" and curr.get("conj_form") == "文語基本形" and j + 1 < len(result):
            nxt = result[j + 1]
            ns = nxt.get("surface", "")
            if ns.startswith("い"):
                adj_surface = curr["surface"] + "い"
                # The reference analyzer may normalize the lemma to another
                # modern spelling.  Suzume preserves the observed productive
                # -しい base, so the oracle must do the same after restoring
                # the split terminal い.
                new_result.append({"surface": adj_surface, "pos": "形容詞", "lemma": adj_surface})
                rest = ns[1:]
                for ch in rest:
                    new_result.append({"surface": ch, "pos": "助詞", "lemma": ch})
                skip_next = True
                if applied_rule is None:
                    applied_rule = "adj-bungo-fix"
                continue
        new_result.append(curr)
    return new_result, applied_rule


_KARI_TAILS = ("かり", "かる", "かれ")
KARI_MIZENKEI_CELL = "から"
_KARI_MAX_TOKEN_RUN = 4


def classical_adjective_lemma(mizenkei: str) -> str | None:
    """Read the modern headword of a classical adjective off its 未然形 cell.

    The reference dictionary conjugates the から cell itself (難しから -> 難しい,
    遅から -> 遅い), so asking it resolves the ク/シク split without having to
    guess whether a し belongs to the stem or to the ending.  Only its own stale
    headwords need correcting on the way out.
    """
    from .mecab import mecab_analyze

    tokens = mecab_analyze(mizenkei)
    if len(tokens) != 1:
        return None
    token = tokens[0]
    if token.get("pos") != "形容詞" or token.get("surface") != mizenkei:
        return None
    lemma = token.get("lemma")
    return CLASSICAL_ADJECTIVE_LEMMA_OVERRIDES.get(lemma, lemma) or None


_KARI_CELL_POS = ("形容詞", "助動詞")


def _kari_cell_analysis(surface: str) -> tuple[str, str] | None:
    """Read the word class and lemma of a カリ cell off its 未然形.

    The supplementary conjugation belongs to the i-adjective and to every
    auxiliary that inflects like one (べし, たい, らしい), and the reference
    dictionary carries the 未然形 cell of both kinds (高から, べから) while losing
    the rest.  That cell is therefore the probe: a surface whose カリ ending can
    be swapped for から and still analyze as one word is a cell of the same
    paradigm, and the probe settles the word class along with the lemma.
    """
    from .mecab import mecab_analyze

    mizenkei = surface[: -len(KARI_MIZENKEI_CELL)] + KARI_MIZENKEI_CELL
    tokens = mecab_analyze(mizenkei)
    if len(tokens) != 1:
        return None
    token = tokens[0]
    pos = token.get("pos")
    if pos not in _KARI_CELL_POS or token.get("surface") != mizenkei:
        return None
    lemma = CLASSICAL_ADJECTIVE_LEMMA_OVERRIDES.get(token.get("lemma"), token.get("lemma"))
    return (pos, lemma) if lemma else None


def _postprocess_adj_kari(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Rebuild the classical supplementary (カリ) conjugation.

    から/かり/かる/かれ are cells of the word's own inflection table, not a stem
    plus an auxiliary: there is no 助動詞 かり in the classical inventory. The
    reference dictionary only carries the 未然形 cell, so the others fall back to
    unrelated verbs (高+かり as かりる, 冷+たかる as たかる, 小+さかり as さかる) and,
    for the auxiliaries that inflect like an adjective, to a sentence-final
    particle the position cannot host (べ+かり, where the 終助詞 べ closes a clause
    and so can never stand in front of anything). Restore each run as the one
    token the 未然形 cell already yields, word class and lemma together.
    """
    merged: list[dict] = []
    idx = 0
    while idx < len(result):
        run = ""
        matched_end = 0
        matched_cell = None
        for end in range(idx, min(idx + _KARI_MAX_TOKEN_RUN, len(result))):
            run += result[end].get("surface", "")
            # The rule repairs a split the dictionary got wrong, so a surface it
            # already analyzes as one word needs no repair (明かり stays a noun).
            if end == idx:
                continue
            if len(run) <= len(KARI_MIZENKEI_CELL) or not run.endswith(_KARI_TAILS):
                continue
            cell = _kari_cell_analysis(run)
            if cell is not None:
                matched_end = end + 1
                matched_cell = cell
                break
        if matched_cell is not None:
            cell_pos, cell_lemma = matched_cell
            surface = "".join(result[pos].get("surface", "") for pos in range(idx, matched_end))
            merged.append({"surface": surface, "pos": cell_pos, "lemma": cell_lemma})
            idx = matched_end
            if applied_rule is None:
                applied_rule = "adj-kari-conjugation"
            # The whole point of the かり cell is to carry a classical auxiliary,
            # so a lone し behind it is the 連体形 of the past き. The dictionary
            # never saw the かり token, so it read that し as the サ変
            # continuative it shares its spelling with.
            if (
                surface.endswith("かり")
                and idx < len(result)
                and result[idx].get("surface") == "し"
                and result[idx].get("pos") == "動詞"
            ):
                merged.append({"surface": "し", "pos": "助動詞", "lemma": "き"})
                idx += 1
            continue
        merged.append(result[idx])
        idx += 1
    return merged, applied_rule


_A_ROW_TO_U_ROW = {
    "か": "く",
    "が": "ぐ",
    "さ": "す",
    "た": "つ",
    "な": "ぬ",
    "ば": "ぶ",
    "ま": "む",
    "ら": "る",
    "わ": "う",
}
_CLASSICAL_IRREALIS_AUX = "む"
_HIRAGANA_TAIL = regex.compile(r"\p{Hiragana}$")


def _is_single_verb(surface: str) -> bool:
    """Whether the reference dictionary reads a surface as exactly one verb."""
    from .mecab import mecab_analyze

    tokens = mecab_analyze(surface)
    return len(tokens) == 1 and tokens[0].get("pos") == "動詞" and tokens[0].get("surface") == surface


def _retag_suffix_without_host(tokens: list[dict]) -> list[dict]:
    """Drop the 接尾 subtype from a noun that has no host to attach to.

    A suffix attaches to a nominal, and an auxiliary is not one, so a noun in
    that position is the head of the relative clause the auxiliary closes. The
    dictionary reaches the suffix reading only when the classical chain in front
    defeats it (せ+し+水 against 見+し+水, which it reads correctly), which is why
    the repair rides with the rule that rebuilt the chain.
    """
    for index, token in enumerate(tokens):
        if index == 0 or token.get("pos") != "名詞" or token.get("pos_sub1") != "接尾":
            continue
        if tokens[index - 1].get("pos") == "助動詞":
            tokens[index] = {**token, "pos_sub1": "一般"}
    return tokens


def _postprocess_classical_mu(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Restore the boundary of the classical conjectural む.

    む attaches to a verb's 未然形 (読ま+む, 書か+む), but the reference dictionary
    carries no such auxiliary. It therefore either hands the irrealis kana to a
    lexical verb spanning the boundary (書+かむ, read as 噛む) or swallows む into
    a longer idiom (読ま+むとする). Rebuild 未然形 + む and re-analyze the rest.
    """
    from .mecab import mecab_analyze

    merged: list[dict] = []
    idx = 0
    while idx < len(result):
        token = result[idx]
        surface = token.get("surface", "")
        previous = merged[-1] if merged else None
        # The irrealis kana was handed to the following verb (書 + かむ).
        if (
            previous is not None
            and token.get("pos") == "動詞"
            and len(surface) == 2
            and surface[0] in _A_ROW_TO_U_ROW
            and surface[1] == _CLASSICAL_IRREALIS_AUX
        ):
            stem = previous.get("surface", "")
            lemma = stem + _A_ROW_TO_U_ROW[surface[0]]
            if stem and _is_single_verb(lemma):
                merged[-1] = {"surface": stem + surface[0], "pos": "動詞", "lemma": lemma}
                merged.append({"surface": surface[1], "pos": "助動詞", "lemma": surface[1]})
                idx += 1
                if applied_rule is None:
                    applied_rule = "classical-mu-boundary"
                continue
        # む opened a longer idiom the dictionary lists as one word (むとする).
        # The stem it attaches to is a 未然形, which ends in an a-row kana for a
        # consonant stem (行か) and in an i-/e-row kana for a vowel stem
        # (見え, 流れ).  The dictionary holds no irrealis for the vowel-stem
        # class and reads that stem as a deverbal noun, so the POS cannot carry
        # the test — a content word ending in hiragana is the whole condition,
        # and the conjugation class drops out of it.  A kanji-final host is left
        # out on purpose: there the volitional split rule reads the tail in
        # context and keeps the case particle that re-analysis would lose.
        if (
            previous is not None
            and previous.get("pos") in ("動詞", "名詞")
            and _HIRAGANA_TAIL.search(previous.get("surface", ""))
            and len(surface) > 1
            and surface[0] == _CLASSICAL_IRREALIS_AUX
        ):
            # A noun cannot host む, so the deverbal reading the dictionary
            # produced for the vowel stem has to be undone as well: the stem plus
            # る is the vowel-stem verb it was cut from (流れ → 流れる).  Confirm
            # that lemma against the dictionary rather than assuming it, so a
            # nominal host the rule reached by another route keeps its POS.
            if previous.get("pos") == "名詞":
                stem = previous.get("surface", "")
                lemma = stem + "る"
                if _is_single_verb(lemma):
                    merged[-1] = {"surface": stem, "pos": "動詞", "lemma": lemma}
            merged.append({"surface": surface[0], "pos": "助動詞", "lemma": surface[0]})
            merged.extend(mecab_analyze(surface[1:]))
            idx += 1
            if applied_rule is None:
                applied_rule = "classical-mu-boundary"
            continue
        # The boundary already stands, but with no auxiliary entry to land on the
        # dictionary files the bare mora as an interjection (成ら+む, 待た+む).  An
        # interjection cannot follow an irrealis stem, so the position settles the
        # POS and the auxiliary keeps one reading across every context.
        if (
            previous is not None
            and previous.get("pos") == "動詞"
            and previous.get("surface", "")[-1:] in _A_ROW_TO_U_ROW
            and surface == _CLASSICAL_IRREALIS_AUX
            and token.get("pos") != "助動詞"
        ):
            merged.append({"surface": surface, "pos": "助動詞", "lemma": surface})
            idx += 1
            if applied_rule is None:
                applied_rule = "classical-mu-boundary"
            continue
        merged.append(token)
        idx += 1
    if applied_rule == "classical-mu-boundary":
        _retag_suffix_without_host(merged)
    return merged, applied_rule


_KU_NOMINALIZER = "く"


def _postprocess_ku_nominalization(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Rebuild the classical ク語法 nominalization as one noun.

    ク語法 turns a predicate into a noun by attaching く to the 未然形 (言わ+く,
    思わ+く).  The reference dictionary lists the fossilized members it happens to
    carry as whole nouns — 曰く, 老いらく — and fragments the rest, so the same
    formation is one search unit under one spelling and two or three tokens under
    another.  Both fragmentations are repaired here: the irrealis kana handed to a
    following verb (言 + わく, the shape 書 + かむ already takes), and the bare
    nominalizer left standing after the stem (思わ + く).
    """
    merged: list[dict] = []
    idx = 0
    while idx < len(result):
        token = result[idx]
        surface = token.get("surface", "")
        previous = merged[-1] if merged else None
        # 言 + わく: the irrealis kana went to whatever followed the stem, which
        # the dictionary reads as a verb or a noun depending on the sentence.
        if (
            previous is not None
            and token.get("pos") in ("動詞", "名詞")
            and len(surface) == 2
            and surface[0] in _A_ROW_TO_U_ROW
            and surface[1] == _KU_NOMINALIZER
        ):
            stem = previous.get("surface", "")
            if stem and _is_single_verb(stem + _A_ROW_TO_U_ROW[surface[0]]):
                combined = stem + surface
                merged[-1] = {"surface": combined, "pos": "名詞", "lemma": combined}
                idx += 1
                if applied_rule is None:
                    applied_rule = "ku-nominalization"
                continue
        # 思わ + く: the stem is already whole and く stands on its own.
        if (
            previous is not None
            and previous.get("pos") == "動詞"
            and previous.get("surface", "")[-1:] in _A_ROW_TO_U_ROW
            and surface == _KU_NOMINALIZER
        ):
            combined = previous.get("surface", "") + surface
            merged[-1] = {"surface": combined, "pos": "名詞", "lemma": combined}
            idx += 1
            if applied_rule is None:
                applied_rule = "ku-nominalization"
            continue
        merged.append(token)
        idx += 1
    return merged, applied_rule


_E_ROW_TO_U_ROW = {
    "え": "う",
    "け": "く",
    "げ": "ぐ",
    "せ": "す",
    "て": "つ",
    "ね": "ぬ",
    "べ": "ぶ",
    "め": "む",
    "れ": "る",
}
_CONCESSIVE_PARTICLES = ("ど", "ども")


_TOMO_CONCESSIVE_AUXILIARIES = frozenset({"ず"})


def _postprocess_tomo_particle(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Keep とも whole where it is one particle rather than と plus も.

    とも is a single particle in two frames: after a counted quantity it is the
    universal quantifier (二人とも), and after the negative auxiliary ず or an
    adjective adverbial form it is the concessive conjunctive particle
    (読まずとも, 少なくとも). The reference dictionary lexicalizes only
    the handful of quantities it happens to list and splits the rest, so the
    boundary is restored from the host instead. Every other host keeps the
    case particle と plus the binding particle も (願いとも違う).
    """
    merged: list[dict] = []
    skip_next = False
    for idx, token in enumerate(result):
        if skip_next:
            skip_next = False
            continue
        following = result[idx + 1] if idx + 1 < len(result) else None
        host = merged[-1] if merged else None
        if (
            following is not None
            and host is not None
            and token.get("surface") == "と"
            and token.get("pos") == "助詞"
            and following.get("surface") == "も"
            and following.get("pos") == "助詞"
        ):
            host_surface = host.get("surface", "")
            host_pos = host.get("pos", "")
            quantifier_host = host_pos == "名詞" and host.get("pos_sub1") == "数"
            concessive_host = (host_pos == "助動詞" and host_surface in _TOMO_CONCESSIVE_AUXILIARIES) or (
                host_pos == "形容詞" and host_surface.endswith("く")
            )
            if quantifier_host or concessive_host:
                merged.append({"surface": "とも", "pos": "助詞", "lemma": "とも"})
                skip_next = True
                if applied_rule is None:
                    applied_rule = "tomo-particle-boundary"
                continue
        merged.append(token)
    return merged, applied_rule


def _postprocess_izenkei_concessive(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Give the 已然形 before a concessive conjunction its plain verb lemma.

    ど/ども select the 已然形, which the modern paradigm spells like the
    hypothetical (書け+ど, 飲め+ど). The potential verb of the same stem reaches
    that conjunction only through its own 已然形 (書けれ+ど), so a bare e-row form
    here belongs to the plain verb. The reference dictionary splits the two
    readings by row, giving 飲む for one and 書ける for the other.
    """
    tagged: list[dict] = []
    for idx, token in enumerate(result):
        following = result[idx + 1] if idx + 1 < len(result) else None
        surface = token.get("surface", "")
        if (
            following is not None
            and following.get("surface") in _CONCESSIVE_PARTICLES
            and following.get("pos") == "助詞"
            and token.get("pos") == "動詞"
            and token.get("lemma") == surface + "る"
            and surface[-1:] in _E_ROW_TO_U_ROW
        ):
            plain = surface[:-1] + _E_ROW_TO_U_ROW[surface[-1]]
            if _is_single_verb(plain):
                tagged.append({**token, "lemma": plain})
                if applied_rule is None:
                    applied_rule = "izenkei-concessive-lemma"
                continue
        tagged.append(token)
    return tagged, applied_rule


_CLASSICAL_CAUSATIVE_FORMS = ("しむ", "しめ", "しむる", "しむれ")


def _postprocess_classical_shimu(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Tag the classical causative しむ as an auxiliary.

    しむ conjugates 下二段 and attaches to the same 未然形 as the modern せる, but
    the reference dictionary has no such auxiliary and falls back to a lexical
    verb of the same spelling (書か+しむ). After an irrealis there is no verb
    reading available, so the cell is the auxiliary.
    """
    tagged: list[dict] = []
    for idx, token in enumerate(result):
        previous = result[idx - 1] if idx > 0 else None
        if (
            previous is not None
            and previous.get("pos") == "動詞"
            and previous.get("surface", "")[-1:] in _A_ROW_TO_U_ROW
            and token.get("surface") in _CLASSICAL_CAUSATIVE_FORMS
        ):
            tagged.append({**token, "pos": "助動詞", "lemma": "しむ"})
            if applied_rule is None:
                applied_rule = "classical-shimu-auxiliary"
            continue
        tagged.append(token)
    return tagged, applied_rule


_HA_ROW_TAILS = ("は", "ひ", "ふ", "へ")
_HA_ROW_DETACHED_TAILS = ("ひ", "ふ", "へ")
_HA_ROW_STEM_POS = ("名詞", "動詞", "形容詞", "副詞", "接尾辞")


_HA_ROW_FRAME_STEM = "思"


def _ha_row_fabricated_ichidan(token: dict) -> bool:
    """Whether a verb token's lemma is the 一段 reading invented for a ハ行 cell."""
    surface = token.get("surface", "")
    return token.get("pos") == "動詞" and token.get("lemma") == surface + "る"


def _ha_row_cell_auxiliary(surface: str) -> dict | None:
    """Split a ハ行 cell from the auxiliary a fabricated verb swallowed it into.

    The cell kana does not always fall out on its own: where it opens a longer
    run, the reference dictionary reads the whole run as an unrelated 五段 verb
    (適+ひたる as 浸る). Writing the same run behind the one stem whose ハ行 row
    the dictionary does carry settles what the cell actually hosts, and that
    frame cannot be misread because the row is listed for it.
    """
    from .mecab import mecab_analyze

    if len(surface) < 2 or surface[0] not in _HA_ROW_DETACHED_TAILS:
        return None
    probe = mecab_analyze(_HA_ROW_FRAME_STEM + surface)
    if len(probe) != 2 or probe[0].get("surface") != _HA_ROW_FRAME_STEM + surface[0]:
        return None
    if probe[0].get("pos") != "動詞" or probe[0].get("conj_type") != "四段・ハ行":
        return None
    return probe[1] if probe[1].get("pos") == "助動詞" else None


def _postprocess_ha_row_godan(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Rebuild the classical ハ行四段 conjugation (候ふ, 移ろひ, 思へ).

    ハ行四段 is the historical-kana spelling of the modern ワ行五段 row, so
    は/ひ/ふ/へ are cells of one verb whose terminal form is the lemma. The
    reference dictionary carries the row only for the handful of verbs it
    happens to list (思ふ); everywhere else the cell kana falls out as a separate
    one-mora verb with an invented 一段 lemma (候+ふ as ふる, 移ろ+ひ as ひる), or
    the whole cell keeps such a lemma (思へ as 思へる). Reattach the detached
    kana to its stem and give both the row's own terminal ふ.
    """
    merged: list[dict] = []
    idx = 0
    while idx < len(result):
        token = result[idx]
        following = result[idx + 1] if idx + 1 < len(result) else None
        if (
            following is not None
            and following.get("surface") in _HA_ROW_DETACHED_TAILS
            and _ha_row_fabricated_ichidan(following)
            and token.get("pos") in _HA_ROW_STEM_POS
        ):
            stem = token.get("surface", "")
            merged.append({"surface": stem + following["surface"], "pos": "動詞", "lemma": stem + "ふ"})
            idx += 2
            # The terminal cell in front of a nominal is the adnominal, and an
            # adnominal takes a head noun, so the following word is not the bound
            # counter the dictionary reads it as elsewhere (舞ふ+間 against 三+間).
            if (
                following["surface"] == "ふ"
                and idx < len(result)
                and result[idx].get("pos") == "名詞"
                and result[idx].get("pos_sub1") == "接尾"
            ):
                merged.append({**result[idx], "pos_sub1": "一般"})
                idx += 1
            if applied_rule is None:
                applied_rule = "ha-row-godan-conjugation"
            continue
        # A longer fabricated verb hides the same cell behind its own opening
        # kana, and what follows the cell there is an auxiliary (適+ひ+たる).
        if following is not None and following.get("pos") == "動詞" and token.get("pos") in _HA_ROW_STEM_POS:
            auxiliary = _ha_row_cell_auxiliary(following.get("surface", ""))
            if auxiliary is not None:
                stem = token.get("surface", "")
                merged.append({"surface": stem + following["surface"][0], "pos": "動詞", "lemma": stem + "ふ"})
                merged.append(auxiliary)
                idx += 2
                if applied_rule is None:
                    applied_rule = "ha-row-godan-conjugation"
                continue
        # The classical honorific stem is tagged as a suffix, and its imperative
        # cell then falls out as the direction particle (給+へ). A suffix never
        # takes that particle, so the pair is one 命令形 of the ハ行四段 verb.
        if (
            following is not None
            and token.get("surface") == "給"
            and token.get("pos") == "名詞"
            and token.get("pos_sub1") == "接尾"
            and following.get("surface") in _HA_ROW_DETACHED_TAILS
            and following.get("pos") == "助詞"
        ):
            stem = token.get("surface", "")
            merged.append({"surface": stem + following["surface"], "pos": "動詞", "lemma": stem + "ふ"})
            idx += 2
            if applied_rule is None:
                applied_rule = "ha-row-godan-conjugation"
            continue
        surface = token.get("surface", "")
        if len(surface) > 1 and surface.endswith(_HA_ROW_TAILS) and _ha_row_fabricated_ichidan(token):
            merged.append({**token, "lemma": surface[:-1] + "ふ"})
            idx += 1
            if applied_rule is None:
                applied_rule = "ha-row-godan-conjugation"
            continue
        merged.append(token)
        idx += 1
    return merged, applied_rule


# The 終止形 of a classical 二段 verb is its kanji stem plus one U-row kana.  The
# modern descendant is 一段, so the same stem takes the row's E-row or I-row kana
# plus る, and that headword is what the reference dictionary does carry.  The ダ行
# row is the one whose kana was absorbed into the kanji's own reading (出づ -> 出る),
# so there the modern headword is the bare stem plus る.
_NIDAN_TERMINAL_ROWS: dict[str, tuple[str, ...]] = {
    "う": ("え", "い"),
    "く": ("け", "き"),
    "ぐ": ("げ", "ぎ"),
    "す": ("せ", "し"),
    "つ": ("て", "ち"),
    "づ": ("で", "じ", ""),
    "ぬ": ("ね", "に"),
    "ふ": ("え", "い"),
    "ぶ": ("べ", "び"),
    "む": ("め", "み"),
    "ゆ": ("え", "い"),
    "る": ("れ", "り"),
}
# Those same kana also spell classical auxiliaries that attach to a 未然形 or a
# 連用形 (見|つ, 見|ぬ, 見|む).  After an inflected verb the kana is the auxiliary,
# so only a stem the dictionary did not inflect can head a 二段 terminal cell.
_NIDAN_AUXILIARY_HOMOGRAPHS = frozenset({"す", "つ", "ぬ", "ふ", "む", "る"})
_NIDAN_INFLECTED_STEM_FORMS = frozenset({"未然形", "連用形"})


_NIDAN_CELL = regex.compile(rf"^(\p{{Han}}[\p{{Han}}\p{{Hiragana}}]*?)([{''.join(_NIDAN_TERMINAL_ROWS)}])([るれ]?)$")
# The kana a 二段 終止形 ends in, for callers that only see a rebuilt cell.
NIDAN_TERMINAL_KANA = frozenset(_NIDAN_TERMINAL_ROWS)


@cache
def _nidan_terminal_lemma(stem: str, terminal: str) -> str | None:
    """Return the 終止形 when a stem plus a U-row kana is a classical 二段 verb."""
    for vowel in _NIDAN_TERMINAL_ROWS[terminal]:
        if is_single_token_of_pos(stem + vowel + "る", "動詞"):
            return stem + terminal
    return None


def _nidan_cell_match(token: dict, following: dict) -> tuple[regex.Match, str] | None:
    """Match a 二段 cell across a token pair, allowing an unanalyzed tail.

    Where the cell ends the phrase the dictionary reads its kana as one unknown
    noun and glues whatever follows onto that token (出|づるか, 出|づまじ).  An
    unknown word carries no analysis to preserve, so the cell may be matched
    against a prefix of it and the rest handed back to the dictionary.
    """
    head, tail = token.get("surface", ""), following.get("surface", "")
    cell = _NIDAN_CELL.match(head + tail)
    if cell is not None:
        return cell, ""
    if following.get("lemma") not in ("*", "", None):
        return None
    for width in (2, 1):  # the cell's kana tail is the terminal plus an optional る
        if width < len(tail) and (cell := _NIDAN_CELL.match(head + tail[:width])) is not None:
            return cell, tail[width:]
    return None


def nidan_cell(token: dict, following: dict | None) -> tuple[str, str, str] | None:
    """Return (surface, 終止形, remainder) when two tokens spell one 二段 finite cell.

    終止形 is the stem plus one U-row kana, 連体形 adds る and 已然形 adds れ.  The
    stem keeps whatever 送り仮名 the modern headword carries (聞こ|ゆ), so it is
    matched as a kanji head plus the kana that follow, shortest first: the longest
    stem would swallow the 連体形 る of 消|ゆ|る.  Asking the dictionary for the
    modern 一段 headword the same stem builds decides whether the pair is a verb,
    without listing the classical paradigm.  The remainder is whatever the window
    matched past the cell, and is left for the caller to re-analyze.
    """
    if following is None:
        return None
    matched = _nidan_cell_match(token, following)
    if matched is None:
        return None
    cell, remainder = matched
    stem, terminal, inflection = cell.groups()
    # The same kana spell classical auxiliaries that attach to a 未然形 or a
    # 連用形 (見|つ, 見|ぬ, 見|つる).  After a stem the dictionary inflected, the
    # kana is that auxiliary and not part of the verb — unless the stem's lemma is
    # the cell itself (流|るる is read as the 未然形 of 流る), where the kana is
    # that verb's own 送り仮名.
    if (
        terminal in _NIDAN_AUXILIARY_HOMOGRAPHS
        and token.get("pos") == "動詞"
        and token.get("conj_form") in _NIDAN_INFLECTED_STEM_FORMS
        and token.get("lemma") != stem + terminal
    ):
        return None
    lemma = _nidan_terminal_lemma(stem, terminal)
    return None if lemma is None else (lemma + inflection, lemma, remainder)


def _postprocess_nidan_cell(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Rebuild the finite cells of a classical 二段 verb (受く, 越ゆ, 求むる, 聞こゆれ).

    The reference dictionary carries only the modern 一段 headword, so the kana
    falls out as whatever else it can spell — an adjective stem (受+く as くい), a
    bare noun (越+ゆ), the カ変 くる, or the 完了 つ — and the kanji is left as a
    noun that is not a word on its own.  Where the two tokens fall is not fixed
    (求む|る but 見|ゆる), so the window is matched on its combined surface.
    """
    merged: list[dict] = []
    idx = 0
    while idx < len(result):
        token = result[idx]
        cell = nidan_cell(token, result[idx + 1] if idx + 1 < len(result) else None)
        if cell is not None:
            surface, lemma, remainder = cell
            merged.append({"surface": surface, "pos": "動詞", "lemma": lemma})
            if remainder:
                merged.extend(mecab_analyze(remainder))
            idx += 2
            if applied_rule is None:
                applied_rule = "classical-nidan-cell"
            continue
        merged.append(token)
        idx += 1
    return merged, applied_rule


# The closed set of 係助詞 a 係り結び opens with.  The reference dictionary
# lexicalizes one demonstrative + 係助詞 pair as an adverb, which buries the
# particle that governs the clause's final form.
_KAKARI_PARTICLES = ("ぞ", "こそ", "なむ", "や")


@cache
def _reads_as_pronoun(surface: str) -> bool:
    """Whether the reference dictionary reads a surface as exactly one pronoun."""
    tokens = mecab_analyze(surface)
    return len(tokens) == 1 and tokens[0].get("pos_sub1") == "代名詞" and tokens[0].get("surface") == surface


def _postprocess_kakari_pronoun_split(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Split a demonstrative that the dictionary fused with its 係助詞 (これぞ).

    A 係助詞 governs the form its clause ends in, so burying it inside a
    lexicalized adverb loses the only token that explains the 結び.  The
    demonstrative in front of it is a pronoun the dictionary carries on its own.
    """
    split: list[dict] = []
    for index, token in enumerate(result):
        surface = token.get("surface", "")
        # The same particles are read as interjections where the dictionary has
        # no entry for their 係助詞 use. A nominal in front of one is the phrase
        # it marks, which is the position an interjection never fills.
        previous = result[index - 1] if index > 0 else None
        if (
            token.get("pos") == "感動詞"
            and surface in _KAKARI_PARTICLES
            and previous is not None
            and previous.get("pos") == "名詞"
        ):
            split.append({"surface": surface, "pos": "助詞", "pos_sub1": "係助詞", "lemma": surface})
            if applied_rule is None:
                applied_rule = "kakari-pronoun-split"
            continue
        particle = next((p for p in _KAKARI_PARTICLES if surface.endswith(p)), None)
        head = surface[: -len(particle)] if particle else ""
        if token.get("pos") == "副詞" and head and _reads_as_pronoun(head):
            split.append({"surface": head, "pos": "名詞", "pos_sub1": "代名詞", "lemma": head})
            split.append({"surface": particle, "pos": "助詞", "pos_sub1": "係助詞", "lemma": particle})
            if applied_rule is None:
                applied_rule = "kakari-pronoun-split"
            continue
        split.append(token)
    return split, applied_rule


@cache
def _modern_kana_word(surface: str) -> dict | None:
    """Return the modern-spelling reading of a historical-kana surface, if it is one word."""
    modern = surface.translate(HISTORICAL_KANA_RESPELLING)
    if modern == surface:
        return None
    tokens = mecab_analyze(modern)
    if len(tokens) != 1 or tokens[0].get("surface") != modern or tokens[0].get("pos") == "動詞":
        return None
    return tokens[0]


def _postprocess_historical_kana_word(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Read a historical-kana word through its modern spelling (いづれ, まづ).

    The reference dictionary carries the modern orthography, so a word written
    with づ/ぢ/ゐ/ゑ falls back on whatever inflected cell those kana happen to
    complete — いづれ becomes the 已然形 of 出づ rather than the pronoun it
    spells. Respelling is a one-for-one substitution, so asking the dictionary
    for the modern form recovers the word's own class. A modern form that is
    itself a verb is left alone: there the fallback and the word coincide.
    """
    retagged: list[dict] = []
    for token in result:
        modern = _modern_kana_word(token.get("surface", "")) if token.get("pos") == "動詞" else None
        if modern is None:
            retagged.append(token)
            continue
        retagged.append({**modern, "surface": token["surface"], "lemma": token["surface"]})
        if applied_rule is None:
            applied_rule = "historical-kana-word"
    return retagged, applied_rule


def _postprocess_nominal_classical_copula(
    result: list[dict], applied_rule: str | None
) -> tuple[list[dict], str | None]:
    """Tag なる/なり directly after a nominal as the classical copula (道なる).

    Modern なる needs the case particle に in front of it, so a bare nominal
    host leaves only the literary copula. The reference dictionary carries the
    lexical verb for that spelling and reaches for it whenever its own headword
    list happens to miss the auxiliary reading.
    """
    tagged: list[dict] = []
    for index, token in enumerate(result):
        previous = result[index - 1] if index > 0 else None
        if (
            token.get("pos") == "動詞"
            and token.get("lemma") == "なる"
            and previous is not None
            and previous.get("pos") == "名詞"
        ):
            tagged.append({**token, "pos": "助動詞", "conj_type": "文語・ナリ", "lemma": "なり"})
            if applied_rule is None:
                applied_rule = "nominal-classical-copula"
            continue
        tagged.append(token)
    return tagged, applied_rule


def _postprocess_nickname_merge(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Merge hiragana nickname + honorific into a single token.

    Tokenizer use case: short hiragana nicknames like たっちゃん / ゆうちゃん /
    けんちゃん / わんちゃん should be one search unit, not split as stem+suffix.
    Kanji or katakana names (太郎+ちゃん, ピー+ちゃん) keep splitting.

    Also handles MeCab's misparses where the nickname spans 3+ tokens
    (e.g., たっちゃん → たっ + ちゃ + ん). The scan greedily concatenates
    consecutive hiragana tokens (preceding `prev_is_prefix == False`) and
    merges when the concatenated surface is short hiragana stem + honorific.
    """
    honorifics = ("ちゃん", "くん", "さん")
    hira_re = regex.compile(r"^[\p{Hiragana}っー]+$")

    merged: list[dict] = []
    i = 0
    while i < len(result):
        # Hiragana run starting at i that ends with a honorific. Skip if prev
        # is a prefix (お/ご) — let family-merge handle those.
        prev_is_prefix = merged and merged[-1].get("pos", "") == "接頭詞"
        if not prev_is_prefix and i < len(result) and hira_re.match(result[i].get("surface", "")):
            j = i
            run = ""
            while j < len(result) and hira_re.match(result[j].get("surface", "")):
                run += result[j].get("surface", "")
                j += 1
            matched = False
            for h in honorifics:
                if run.endswith(h):
                    stem = run[: len(run) - len(h)]
                    # Stem 2-3 hiragana chars. 1-char stems (e.g., おさん) are
                    # too short and risk false merges (がおさん → が+おさん bad).
                    if 2 <= len(stem) <= 3:
                        merged.append({"surface": run, "pos": "名詞", "lemma": run})
                        i = j
                        if applied_rule is None:
                            applied_rule = "nickname-merge"
                        matched = True
                        break
            if matched:
                continue
            # No nickname match — append the current token and continue
            merged.append(result[i])
            i += 1
            continue
        merged.append(result[i])
        i += 1
    return merged, applied_rule


def _postprocess_kanji_merge(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Merge consecutive all-kanji tokens.

    Also merges single-kanji + kanji-starting tokens when MeCab incorrectly
    splits compound words (e.g., 微+笑み → 微笑み).
    """
    merged = []
    for curr in result:
        surface = curr.get("surface", "")
        # Suzume design: tokenizer use case prefers X+suffix as a single search
        # unit. These suffixes are not treated as token boundaries; X+SUFFIX
        # merges via kanji-merge.
        #   家/力/化/法/論/員/式/感/的/安 — productive but one search unit
        # 様/氏 keep splitting (honorific separates from name).
        is_merge_allowed_suffix = surface in ("家", "力", "化", "法", "論", "員", "式", "感", "的", "風", "安")
        # Suzume design: 御 is a productive prefix that always splits off
        # (御 + 尽力, 御 + 挨拶, 御 + 協力). Skip kanji-merge after 御 prefix tokens.
        prev_is_go_prefix = merged and merged[-1].get("surface", "") == "御" and merged[-1].get("pos", "") == "接頭詞"
        # A na-adjective stem heads a predicate, so a bound modifier in front of
        # it stays outside: 時|不思議|な, 激|簡単|だ, 鬼|簡単|だ.  Such a modifier
        # does bond with a plain noun into one search unit (激安, 超高速), but it
        # stops at a predicate head — the same boundary it already keeps before an
        # i-adjective (激|冷たい).  Only the modifier side is held back: a full
        # noun preceding the stem still compounds (再利用可能), which is why this
        # is restricted to a non-independent noun or a single kanji.  MeCab tags
        # the stem as a noun, so the generic kanji merge would otherwise erase
        # that boundary before POS normalization.
        na_adjective_stem_boundary = (
            merged
            and curr.get("pos", "") == "名詞"
            and curr.get("pos_sub1", "") == "形容動詞語幹"
            and merged[-1].get("pos", "") == "名詞"
            and (merged[-1].get("pos_sub1", "") == "非自立" or len(merged[-1].get("surface", "")) == 1)
        )
        if merged and (
            (
                _IDEOGRAPHIC_SEQUENCE.fullmatch(surface)
                and _IDEOGRAPHIC_SEQUENCE.fullmatch(merged[-1].get("surface", ""))
                # Kanji adjacency alone is not a compound boundary.  In
                # particular, a temporal noun followed by a one-kanji verb
                # stem (the pattern 日+見+た) must retain the predicate
                # boundary.  Restrict this recovery pass to nominal pieces.
                and curr.get("pos", "") == "名詞"
                and merged[-1].get("pos", "") == "名詞"
                and "々" not in merged[-1].get("surface", "")
                and (merged[-1].get("pos_sub1", "") not in ("副詞可能", "固有名詞", "数") or is_merge_allowed_suffix)
                and merged[-1].get("pos", "") != "副詞"
                and (curr.get("pos_sub1", "") != "接尾" or is_merge_allowed_suffix)
                # A number+counter unit (五分, 二時間, 五名) is its own search unit and
                # must not fold into a preceding noun/prefix (徒歩|五分, 約|二時間).
                and curr.get("pos_sub1", "") != "数"
                and not prev_is_go_prefix
                and not na_adjective_stem_boundary
            )
            or (surface == "々" and _IDEOGRAPHIC_SEQUENCE.fullmatch(merged[-1].get("surface", "")))
            or (
                merged[-1].get("surface", "") in KANJI_PREFIX_COMPOUNDS
                and surface in KANJI_PREFIX_COMPOUNDS[merged[-1]["surface"]]
                and merged[-1].get("pos") != "Noun"
            )
        ):
            merged[-1]["surface"] += surface
            merged[-1]["lemma"] = merged[-1]["surface"]
            merged[-1]["pos"] = "名詞"
            if applied_rule is None:
                applied_rule = "kanji-merge"
        else:
            merged.append(curr)
    return merged, applied_rule


def _postprocess_search_unit_split(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Re-split kanji-merged tokens that absorbed part of a search-unit compound.

    Example: kanji-merge produces AB+C, but BC should be one token.
    This splits AB → A+B, then merges B+C → BC.
    """
    new_result: list[dict] = []
    skip_next = False
    for j, curr in enumerate(result):
        if skip_next:
            skip_next = False
            continue
        if j < len(result) - 1:
            nxt = result[j + 1]
            curr_surface = curr.get("surface", "")
            nxt_surface = nxt.get("surface", "")
            for word, word_pos in SEARCH_UNIT_COMPOUNDS.items():
                # Check if word spans across curr (ending) + nxt (beginning)
                for split_pos in range(1, len(word)):
                    prefix = word[:split_pos]
                    suffix = word[split_pos:]
                    if curr_surface.endswith(prefix) and nxt_surface == suffix:
                        head = curr_surface[: -len(prefix)]
                        if head:
                            new_result.append({"surface": head, "pos": curr.get("pos", ""), "lemma": head})
                        new_result.append({"surface": word, "pos": word_pos, "lemma": word})
                        skip_next = True
                        if applied_rule is None:
                            applied_rule = "search-unit-split"
                        break
                if skip_next:
                    break
        if not skip_next or j < len(result) - 1:
            if not skip_next:
                new_result.append(curr)
    # Handle last token if not skipped
    if not skip_next and len(result) > 0:
        pass  # Already appended in the loop
    return new_result, applied_rule


# Characters an ASCII word keeps inside itself. The reference tokenizer emits
# them as standalone symbol tokens (Coca + - + Cola), which breaks the search
# unit into fragments no query matches, so the pieces are rejoined here.
_ASCII_WORD_JOINERS = ".-'&/"


def _postprocess_ascii_joiner_merge(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Merge ASCII + word-internal joiner + ASCII/number tokens."""
    merged = []
    for j, curr in enumerate(result):
        surface = curr.get("surface", "")
        if (
            len(surface) == 1
            and surface in _ASCII_WORD_JOINERS
            and merged
            and regex.match(r"^[a-zA-Z]+$", merged[-1].get("surface", ""))
            and j + 1 < len(result)
            and regex.match(r"^[a-zA-Z0-9]+$", result[j + 1].get("surface", ""))
        ):
            merged[-1]["surface"] += surface
            merged[-1]["lemma"] = merged[-1]["surface"]
            if applied_rule is None:
                applied_rule = "ascii-joiner-merge"
        elif (
            merged
            and merged[-1].get("surface", "").endswith(tuple(_ASCII_WORD_JOINERS))
            and regex.match(r"^[a-zA-Z0-9]+$", surface)
        ):
            merged[-1]["surface"] += surface
            merged[-1]["lemma"] = merged[-1]["surface"]
        else:
            merged.append(curr)
    return merged, applied_rule


# Small kana that cannot open a mora, and therefore cannot open a morpheme.
# The sokuon っ and the moraic ん are excluded: both do stand alone as tokens.
_NON_INITIAL_SMALL_KANA = "ゃゅょぁぃぅぇぉゎャュョァィゥェォヮ"


def _postprocess_small_kana_head_merge(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Reattach a token that opens with a small kana to the one before it.

    A palatalised or small-vowel kana is the second half of a mora, so a token
    starting with one is a split inside a mora rather than a morpheme boundary.
    The reference dictionary produces such fragments for surfaces it does not
    know (読ん+じ+ょる).
    """
    merged: list[dict] = []
    for curr in result:
        surface = curr.get("surface", "")
        if merged and surface and surface[0] in _NON_INITIAL_SMALL_KANA:
            merged[-1]["surface"] += surface
            merged[-1]["lemma"] = merged[-1]["surface"]
            if applied_rule is None:
                applied_rule = "small-kana-head-merge"
            continue
        merged.append(curr)
    return merged, applied_rule


# The colloquial volitional reduces its う to a geminate before the quotative
# と (行こう+と → 行こっと, し+よう+と → しよっと), so a run closing on っと is not
# automatically a mimetic. The reference dictionary marks the difference: a
# predicate leaves an o-row verb stem or the volitional auxiliary directly in
# front of the っと token, while a mimetic has no predicate there at all.
_O_ROW_KANA: frozenset[str] = frozenset("おこそとのほもよろごぞどぼぽょ")


def _opens_volitional_tto(previous: dict | None) -> bool:
    """Whether a token can carry the volitional that っと contracts."""
    if previous is None:
        return False
    if previous.get("pos") == "助動詞" and previous.get("lemma") in ("う", "よう"):
        return True
    return previous.get("pos") == "動詞" and previous.get("surface", "")[-1:] in _O_ROW_KANA


def _postprocess_onomatopoeia_tto_merge(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Merge onomatopoeia stem + っと → Xっと (adverb).

    MeCab splits: どき+っと, ぱっ+と, etc.
    Suzume treats Xっと as a single adverb unit.
    Pattern: short hiragana/katakana token + っと where stem is 1-4 chars.
    """
    merged: list[dict] = []
    skip_next = False
    for j, curr in enumerate(result):
        if skip_next:
            skip_next = False
            continue
        if (
            j < len(result) - 1
            and result[j + 1].get("surface") == "っと"
            and not _opens_volitional_tto(curr)
            and regex.match(r"^[\p{Hiragana}\p{Katakana}ー]{1,4}$", curr.get("surface", ""))
        ):
            combined = curr.get("surface", "") + "っと"
            merged.append({"surface": combined, "pos": "副詞", "lemma": combined})
            skip_next = True
            if applied_rule is None:
                applied_rule = "onomatopoeia-tto-merge"
        else:
            merged.append(curr)
    return merged, applied_rule


def _is_productive_mimetic_stem(surface: str) -> bool:
    """Recognize productive hiragana mimetic shapes without a word list."""
    if not regex.fullmatch(r"[\p{Hiragana}ー]{3,12}", surface):
        return False
    length = len(surface)
    if length % 2 == 0 and surface[: length // 2] == surface[length // 2 :]:
        return True
    if regex.fullmatch(r".{2,4}ん.{2,4}ん", surface):
        return True
    if regex.fullmatch(r".っ.[らり]", surface):
        return True
    # Alternating two-mora mimetics such as ちくたく share their closing
    # mora even when the two halves are not identical.
    return length == 4 and surface[1] == surface[3]


def _postprocess_productive_mimetics(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Rebuild productive mimetic search units from arbitrary MeCab splits.

    Repetition and fixed phonological shapes are lexical content; a following
    adverbial と remains its own particle.  The productive Xっと shape instead
    includes と in the mimetic itself.
    """
    normalized: list[dict] = []
    idx = 0
    while idx < len(result):
        surface = result[idx].get("surface", "")
        mimetic_suru_splits = {
            "している": (
                {"surface": "し", "pos": "動詞", "lemma": "する"},
                {"surface": "て", "pos": "助詞", "lemma": "て"},
                {"surface": "いる", "pos": "助動詞", "lemma": "いる"},
            ),
            "してる": (
                {"surface": "し", "pos": "動詞", "lemma": "する"},
                {"surface": "てる", "pos": "助動詞", "lemma": "てる"},
            ),
            "した": (
                {"surface": "し", "pos": "動詞", "lemma": "する"},
                {"surface": "た", "pos": "助動詞", "lemma": "た"},
            ),
            "して": (
                {"surface": "し", "pos": "動詞", "lemma": "する"},
                {"surface": "て", "pos": "助詞", "lemma": "て"},
            ),
            "する": ({"surface": "する", "pos": "動詞", "lemma": "する"},),
        }
        fused_suffix = next(
            (
                suffix
                for suffix in mimetic_suru_splits
                if surface.endswith(suffix) and _is_productive_mimetic_stem(surface[: -len(suffix)])
            ),
            "",
        )
        if fused_suffix:
            stem = surface[: -len(fused_suffix)]
            normalized.append({"surface": stem, "pos": "副詞", "lemma": stem})
            normalized.extend(mimetic_suru_splits[fused_suffix])
            idx += 1
            if applied_rule is None:
                applied_rule = "productive-mimetic-suru"
            continue

        matched = False
        max_end = min(len(result), idx + 4)
        for end in range(max_end, idx, -1):
            combined = "".join(token.get("surface", "") for token in result[idx:end])
            starts_at_real_boundary = (
                result[idx].get("pos") != "助詞" or idx == 0 or result[idx - 1].get("pos") == "記号"
            )
            closes_volitional_tto = result[end - 1].get("surface") == "っと" and _opens_volitional_tto(
                result[end - 2] if end - 2 >= idx else None
            )
            if (
                starts_at_real_boundary
                and not closes_volitional_tto
                and combined.endswith("っと")
                and regex.fullmatch(r"[\p{Hiragana}ー]{3,12}", combined)
            ):
                normalized.append({"surface": combined, "pos": "副詞", "lemma": combined})
                idx = end
                matched = True
            elif (
                result[idx].get("pos") != "助詞"
                and combined.endswith("と")
                and _is_productive_mimetic_stem(combined[:-1])
            ):
                stem = combined[:-1]
                normalized.append({"surface": stem, "pos": "副詞", "lemma": stem})
                normalized.append({"surface": "と", "pos": "助詞", "lemma": "と"})
                idx = end
                matched = True
            elif (
                end == idx + 1
                and result[idx].get("pos") in {"その他", "副詞", "感動詞"}
                and _is_productive_mimetic_stem(combined)
            ):
                normalized.append({"surface": combined, "pos": "副詞", "lemma": combined})
                idx = end
                matched = True
            if matched:
                if applied_rule is None:
                    applied_rule = "productive-mimetic"
                break
        if not matched:
            normalized.append(result[idx])
            idx += 1
    return normalized, applied_rule


def _postprocess_nominal_zukeru(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Merge a kanji nominal host with the productive Ichidan suffix づける."""
    normalized: list[dict] = []
    idx = 0
    zukeru_forms = ("づける", "づけ", "づけれ", "づけよ", "づけろ")
    while idx < len(result):
        current = result[idx]
        if (
            idx + 1 < len(result)
            and current.get("pos") == "名詞"
            and regex.fullmatch(r"[\p{Han}]{2,}", current.get("surface", ""))
            and result[idx + 1].get("surface") in zukeru_forms
            and result[idx + 1].get("pos") == "動詞"
        ):
            surface = current.get("surface", "") + result[idx + 1].get("surface", "")
            normalized.append(
                {
                    "surface": surface,
                    "pos": "動詞",
                    "lemma": current.get("surface", "") + "づける",
                }
            )
            idx += 2
            if applied_rule is None:
                applied_rule = "nominal-zukeru"
            continue
        normalized.append(current)
        idx += 1
    return normalized, applied_rule


def _is_quantity_unit(surface: str) -> bool:
    """Return whether surface is a productive numeral+kanji unit."""
    numeric = regex.match(r"^[0-9０-９一二三四五六七八九十百千万億兆]+", surface)
    if numeric is None:
        return False
    unit = surface[numeric.end() :]
    return bool(unit and regex.fullmatch(r"[\p{Han}]+", unit))


def _postprocess_distributive_quantity(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Merge repeated numeral+unit phrases such as 一語一語 structurally."""
    normalized: list[dict] = []
    idx = 0
    while idx < len(result):
        surface = result[idx].get("surface", "")
        split_prefix = ""
        for width in range(2, len(surface) // 2 + 1):
            unit = surface[:width]
            if surface.startswith(unit + unit) and _is_quantity_unit(unit):
                split_prefix = unit + unit
                break
        if result[idx].get("pos") == "名詞" and split_prefix:
            normalized.append({"surface": split_prefix, "pos": "名詞", "lemma": split_prefix})
            remainder = surface[len(split_prefix) :]
            if remainder:
                normalized.append({"surface": remainder, "pos": "名詞", "lemma": remainder})
            idx += 1
            if applied_rule is None:
                applied_rule = "distributive-quantity"
            continue
        if (
            idx + 1 < len(result)
            and result[idx].get("pos") == "名詞"
            and result[idx + 1].get("pos") == "名詞"
            and result[idx + 1].get("surface") == surface
            and _is_quantity_unit(surface)
        ):
            combined = surface + surface
            normalized.append({"surface": combined, "pos": "名詞", "lemma": combined})
            idx += 2
            if applied_rule is None:
                applied_rule = "distributive-quantity"
            continue
        normalized.append(result[idx])
        idx += 1
    return normalized, applied_rule


def _postprocess_dialectal(result: list[dict]) -> None:
    """Fix POS/lemmas for dialectal/special patterns."""
    for j, curr in enumerate(result):
        surface = curr.get("surface", "")
        if surface in ("おいで", "お出で"):
            curr["pos"] = "副詞"
            curr["lemma"] = "おいで"
        if j < len(result) - 1:
            nxt = result[j + 1]
            if surface == "なん" and nxt.get("surface") == "し":
                curr["pos"] = "名詞"
                curr["lemma"] = "なん"
                nxt["pos"] = "動詞"
                nxt["lemma"] = "する"


def _postprocess_bound_voiced_suffix(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Rejoin the bound voiced suffix がかる when it was split at its first mora.

    がかる is a bound suffix on a nominal host (芝居がかった, 紫がかって). The
    reference analyzer knows a few hosts lexically and splits the rest into the
    case particle が plus a remainder that is not a word on its own, so the same
    suffix is analyzed two different ways depending on the host. Rejoin the split
    form so the oracle treats every host alike.

    がましい is handled at the end of the pipeline instead: the compound merges
    that build its host run later than this pass, and rejoining the suffix here
    would consume the が they use to recognize that host.
    """
    tails = ("かっ", "かる", "かり", "かれ", "から", "かろ")
    new_result: list[dict] = []
    idx = 0
    while idx < len(result):
        token = result[idx]
        following = result[idx + 1] if idx + 1 < len(result) else None
        host_is_nominal = bool(new_result) and new_result[-1].get("pos") in ("名詞", "Noun", "動詞", "Verb")
        if (
            host_is_nominal
            and token.get("surface") == "が"
            and following is not None
            and following.get("surface", "") in tails
        ):
            merged = token.get("surface", "") + following.get("surface", "")
            pos = "動詞"
            lemma = "がかる"
            new_result.append({"surface": merged, "pos": pos, "lemma": lemma})
            if applied_rule is None:
                applied_rule = "bound-voiced-suffix"
            idx += 2
            continue
        new_result.append(token)
        idx += 1
    return new_result, applied_rule


def _postprocess_bound_suffix_noun_cell(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Restore a bound suffix verb cell the reference dictionary read as a noun.

    形式ばった / 四角ばった are the host plus the derivational suffix ばる in its
    past form, and the analyzer already reads that suffix as a verb wherever its
    spelling is not a word (形式ばって -> ばっ/ばる). Only the cells that spell a
    known noun break, and they break silently: ばった becomes the insect. A
    nominal host is required, which is the environment the suffix takes.
    """
    new_result: list[dict] = []
    for token in result:
        cell = BOUND_SUFFIX_VERB_NOUN_CELLS.get(token.get("surface", ""))
        host_is_nominal = bool(new_result) and new_result[-1].get("pos") in ("名詞", "Noun")
        if cell is None or not host_is_nominal or token.get("pos") not in ("名詞", "Noun"):
            new_result.append(token)
            continue
        suffix_surface, lemma, auxiliary = cell
        new_result.append({"surface": suffix_surface, "pos": "動詞", "lemma": lemma})
        new_result.append({"surface": auxiliary, "pos": "助動詞", "lemma": auxiliary})
        if applied_rule is None:
            applied_rule = "bound-suffix-noun-cell"
    return new_result, applied_rule
