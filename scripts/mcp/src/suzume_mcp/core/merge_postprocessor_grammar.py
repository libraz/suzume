"""Grammar-boundary and function-word post-processing passes."""

import regex

from .constants import (
    HONORIFIC_EXCEPTIONS,
    HONORIFIC_FRAME_TAILS,
    HONORIFIC_SUFFIXES,
    PREFIX_EXCEPTIONS,
)
from .mecab import mecab_analyze
from .merge_postprocessor_common import _continuative_verb_tokens


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

    The concessive closes its clause, so a binding particle after the two morae
    rules it out whatever the host: 好きなのには and なのにもかかわらず are the
    nominalizer plus に. MeCab's own のに is split back in that position.
    """
    merged = []
    skip_next = False
    for j, curr in enumerate(result):
        if skip_next:
            skip_next = False
            continue
        following = result[j + 1] if j + 1 < len(result) else {}
        if (
            curr.get("surface") == "のに"
            and curr.get("pos_sub1") == "接続助詞"
            and following.get("pos_sub1") == "係助詞"
        ):
            merged.append({"surface": "の", "pos": "名詞", "pos_sub1": "非自立", "lemma": "の"})
            merged.append({"surface": "に", "pos": "助詞", "pos_sub1": "格助詞", "lemma": "に"})
            if applied_rule is None:
                applied_rule = "noni-binding-split"
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
            and (j + 2 >= len(result) or result[j + 2].get("pos_sub1") != "係助詞")
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
    verb stem after the prefix (ご迷惑をおかけする). 御 is the kanji spelling of
    the same prefix and follows the same criterion.

    An honorific prefix never attaches to a pronoun, so a prefix the reference
    reads before one (お/やつ for おやつ) belongs to a different noun and the
    pair is merged back into it.
    """
    new_result: list[dict] = []
    for index, t in enumerate(result):
        surface = t.get("surface", "")
        pos = t.get("pos", "")
        pos_sub1 = t.get("pos_sub1", "")
        previous = new_result[-1] if new_result else None
        if (
            pos == "名詞"
            and pos_sub1 == "代名詞"
            and previous is not None
            and previous.get("pos") == "接頭詞"
            and previous.get("surface") in ("お", "ご", "御")
        ):
            merged = previous["surface"] + surface
            new_result[-1] = {"surface": merged, "pos": "名詞", "pos_sub1": "一般", "lemma": merged}
            if applied_rule is None:
                applied_rule = "prefix-pronoun-merge"
            continue
        if pos == "名詞" and pos_sub1 != "接尾" and surface not in PREFIX_EXCEPTIONS:
            m = regex.match(r"^(お|ご|御)([\p{Han}\p{Hiragana}]+)$", surface)
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


# A derivation the reference does hold as one adjective. Its cells are read off
# this frame, so every cell the suffix がましい inflects into is recognized
# without listing them.
_GAMASHII_FRAME_HOST = "未練"


def _gamashii_cell(pieces: str) -> list[dict] | None:
    """Read がまし + a cell through the frame; return the adjective and a trailing さ."""
    probe = mecab_analyze(_GAMASHII_FRAME_HOST + pieces)
    if not probe or probe[0].get("pos") != "形容詞" or probe[0].get("lemma") != f"{_GAMASHII_FRAME_HOST}がましい":
        return None
    rest = probe[1:]
    if rest and not (len(rest) == 1 and rest[0].get("surface") == "さ" and rest[0].get("pos_sub1") == "接尾"):
        return None
    return probe


def _postprocess_gamashii(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Rebuild the adjective-forming suffix がましい from its scattered pieces.

    The reference dictionary holds the lexicalized derivations (押しつけがましい,
    未練がましい) as single adjectives but has no entry for the productive rest,
    where it reads the が as a case particle or a suffix and the rest as the
    verbs 増す, しく or しかる (恩/着せ/が/まし/さ, 言い訳/がま/しく). The host plus
    がましい is one adjective either way; which cell the pieces spell is read off
    a lexicalized derivation the reference does know.
    """
    new_result: list[dict] = []
    index = 0
    while index < len(result):
        head = result[index]
        opens = (head.get("surface") == "が" and head.get("pos") == "助詞") or (
            head.get("surface") == "がま" and head.get("pos_sub1") == "接尾"
        )
        cell: list[dict] | None = None
        end = index
        if opens and new_result:
            for end in range(min(len(result), index + 3), index + 1, -1):
                pieces = "".join(t.get("surface", "") for t in result[index:end])
                if pieces.startswith("がまし") and len(pieces) > 3:
                    cell = _gamashii_cell(pieces)
                    if cell is not None:
                        break
        if cell is not None:
            # The host is whatever precedes the suffix: a bare noun
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
            adjective = cell[0]["surface"][len(_GAMASHII_FRAME_HOST) :]
            new_result.append({"surface": host + adjective, "pos": "形容詞", "lemma": f"{host}がましい"})
            new_result.extend({"surface": "さ", "pos": "名詞", "pos_sub1": "接尾", "lemma": "さ"} for _ in cell[1:])
            index = end
            if applied_rule is None:
                applied_rule = "gamashii-adjective"
            continue
        new_result.append(head)
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
        predicate = result[index + 2] if index + 2 < len(result) else None
        # Before the auxiliary verb ある the で is the copula with も inserted
        # (学生+で+も+ある), a different construction from the particle でも.
        if (
            token.get("surface") == "で"
            and following is not None
            and following.get("surface") == "も"
            and predicate is not None
            and predicate.get("lemma") == "ある"
        ):
            new_result.append({"surface": "で", "pos": "助動詞", "lemma": "だ"})
            index += 1
            continue
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


_HONORIFIC_PREFIXES = frozenset({"お", "ご"})
# A prefix after one of these is standing where a word genuinely ended, so the
# join below must not reach across them even when the two spell a word together
# (と + お is also the numeral とお).
_CLOSED_CLASS_BEFORE_PREFIX = frozenset({"助詞", "助動詞", "接続詞", "接頭詞", "記号", "動詞", "形容詞"})


def _postprocess_word_internal_honorific_prefix(
    result: list[dict], applied_rule: str | None
) -> tuple[list[dict], str | None]:
    """Rejoin an honorific prefix that landed inside a word.

    The prefix opens the word it binds to, so it needs a word boundary on its
    left as well. In front of a kanji nominal the dictionary sometimes finds one
    where there is none, inventing a reading for the morae before it and cutting
    an ordinary word in two (りん+ご+栽培). Whether the boundary is real is
    decidable: joining the prefix back on and re-reading the result returns one
    word exactly when there was no boundary, and the same word standing alone or
    before a particle is already read whole (りんご, りんごを食べる).
    """
    merged: list[dict] = []
    for token in result:
        surface = token.get("surface", "")
        previous = merged[-1] if merged else None
        if (
            previous is not None
            and token.get("pos") == "接頭詞"
            and surface in _HONORIFIC_PREFIXES
            and previous.get("pos") not in _CLOSED_CLASS_BEFORE_PREFIX
        ):
            joined = previous.get("surface", "") + surface
            rejoined = mecab_analyze(joined)
            if len(rejoined) == 1 and rejoined[0].get("surface") == joined:
                merged[-1] = rejoined[0]
                if applied_rule is None:
                    applied_rule = "word-internal-honorific-prefix"
                continue
        merged.append(token)
    return merged, applied_rule


# The imperative an ichidan verb builds on its bare stem, and the cell the
# reference dictionary names when it does reach that reading.
_IMPERATIVE_YO = "よ"
_ICHIDAN_CONJ_TYPE = "一段"
_IMPERATIVE_YO_CELL = "命令ｙｏ"
_CONTINUATIVE_CELL = "連用形"


def _postprocess_ichidan_imperative_yo(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Join the imperative よ to the ichidan stem it inflects.

    よ closes an ichidan imperative on the bare stem, and the reference
    dictionary holds that cell — but only reaches it for the pairs its own
    entries cover, reading the rest as the continuative plus the final
    particle. A bare continuative closes no clause, so there is nothing in
    front of a final particle for it to comment on, and the imperative is the
    only reading left. Only the ichidan paradigm builds this cell: a godan
    verb takes the e-row instead (集まれ + よ really is a final particle).

    A subsidiary verb is excluded because it is not standing on a bare stem at
    all — the te-form in front of it is what it continues, so the complex is a
    closed predicate already and the よ behind it has something to comment on
    (待っ + て + て + よ).
    """
    merged: list[dict] = []
    for token in result:
        host = merged[-1] if merged else None
        if (
            host is None
            or token.get("surface") != _IMPERATIVE_YO
            or token.get("pos") != "助詞"
            or host.get("pos") != "動詞"
            or host.get("pos_sub1") == "非自立"
            or not (host.get("conj_type") or "").startswith(_ICHIDAN_CONJ_TYPE)
            or host.get("conj_form") != _CONTINUATIVE_CELL
        ):
            merged.append(token)
            continue
        merged[-1] = {
            **host,
            "surface": host.get("surface", "") + _IMPERATIVE_YO,
            "conj_form": _IMPERATIVE_YO_CELL,
        }
        if applied_rule is None:
            applied_rule = "ichidan-imperative-yo"
    return merged, applied_rule


def _heads_property_nominal(token: dict) -> bool:
    """Whether a token is the adjective stem み nominalizes."""
    return token.get("pos") == "形容詞"


# An adverb headword whose surface still ends in material of its own, with the
# probe that reads its head back and the tokens the tail spells. The ablative
# takes a nominal, so appending another case particle selects the same reading;
# an adverb head needs no probe at all, since an adverb stands alone.
_DECOMPOSABLE_ADVERB_TAILS = {
    "から": (
        "が",
        "名詞",
        ({"surface": "から", "pos": "助詞", "pos_sub1": "格助詞", "conj_form": "*", "lemma": "から"},),
    ),
    "して": (
        "",
        "副詞",
        (
            {"surface": "し", "pos": "動詞", "pos_sub1": "自立", "conj_form": "連用形", "lemma": "する"},
            {"surface": "て", "pos": "助詞", "pos_sub1": "接続助詞", "conj_form": "*", "lemma": "て"},
        ),
    ),
}


# The interrogative series, which is closed. Its members lexicalize with the
# predicates behind them (どうして, どうやら, どうにか, どうも), so a head drawn
# from it heads a fixed adverb rather than the productive phrase.
_INTERROGATIVE_HEADS = frozenset({"どう", "いか", "なぜ", "なに", "なん"})


def _decomposed_adverb_head(surface: str, probe_suffix: str, head_pos: str) -> dict | None:
    """Read an adverb's head back as the word the construction needs there."""
    if surface in _INTERROGATIVE_HEADS:
        return None
    tokens = mecab_analyze(surface + probe_suffix)
    if len(tokens) != (2 if probe_suffix else 1) or tokens[0].get("surface") != surface:
        return None
    head = tokens[0]
    return head if head.get("pos") == head_pos else None


def _te_form_after_object(token: dict, previous: dict | None) -> list[dict] | None:
    """Read an adverb in て after a direct object as the verb's te-form.

    The reference lists 果たして as an adverb and gives it that tag even where
    the object marker in front of it makes it the verb 果たす (約束を果たして).
    An adverb takes no object, so behind を the headword is the continuative
    plus the conjunctive て whenever the same stem reads as a verb before た.
    """
    surface = token.get("surface", "")
    if (
        token.get("pos") != "副詞"
        or not surface.endswith("て")
        or len(surface) < 3
        or previous is None
        or previous.get("surface") != "を"
        or previous.get("pos") != "助詞"
    ):
        return None
    probe = mecab_analyze(surface[:-1] + "た")
    if len(probe) != 2 or probe[0].get("pos") != "動詞" or probe[0].get("surface") != surface[:-1]:
        return None
    return [probe[0], {"surface": "て", "pos": "助詞", "pos_sub1": "接続助詞", "lemma": "て"}]


def _postprocess_decomposable_adverb(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Give back the boundary an adverb headword swallowed.

    A case particle is what makes the nominal in front of it an argument, and an
    adverb tag over the pair takes that away — 根から comes back as one adverb
    whose lemma is a phrase, while 家から and 枝から come back as a noun and its
    particle. Reading the head back through another case particle settles which
    it is without a word list.

    The continuative of する plus the conjunctive particle is the same case: the
    entry もしかして covers it while もしかしたら and もしかすると come back in
    pieces, so the tag turns on which of the phrases the lexicon happens to
    hold. Requiring the head to be an adverb on its own is what keeps the
    genuinely single adverbs out — 決して, 大して and 概して all leave a bare
    nominal behind, and 果たして leaves two tokens.
    """
    expanded: list[dict] = []
    for token in result:
        surface = token.get("surface", "")
        te_form = _te_form_after_object(token, expanded[-1] if expanded else None)
        if te_form is not None:
            expanded.extend(te_form)
            if applied_rule is None:
                applied_rule = "decomposable-adverb"
            continue
        tail = next(
            (tail for tail in _DECOMPOSABLE_ADVERB_TAILS if surface.endswith(tail) and len(surface) > len(tail)),
            "",
        )
        if not tail or token.get("pos") != "副詞":
            expanded.append(token)
            continue
        probe_suffix, head_pos, tail_tokens = _DECOMPOSABLE_ADVERB_TAILS[tail]
        head = _decomposed_adverb_head(surface[: -len(tail)], probe_suffix, head_pos)
        if head is None:
            expanded.append(token)
            continue
        expanded.append(head)
        expanded.extend(dict(tail_token) for tail_token in tail_tokens)
        if applied_rule is None:
            applied_rule = "decomposable-adverb"
    return expanded, applied_rule


# A derivational suffix that builds a nominal from a predicate, with the base it
# selects. It is fully productive, but the reference dictionary only joins it to
# the host when the pair happens to be one of its headwords — 痛み and 強み come
# back whole while 熱み and 酸み come back in pieces, which is the lexicon
# deciding a question the morphology already answers.
_DERIVATIONAL_NOMINAL_SUFFIXES = {
    "み": _heads_property_nominal,
}


def _postprocess_derivational_nominal_suffix(
    result: list[dict], applied_rule: str | None
) -> tuple[list[dict], str | None]:
    """Join a productive nominalizing suffix to the base it selects."""
    merged: list[dict] = []
    for token in result:
        host = merged[-1] if merged else None
        selects_host = _DERIVATIONAL_NOMINAL_SUFFIXES.get(token.get("surface", ""))
        if (
            host is None
            or selects_host is None
            or token.get("pos") != "名詞"
            or token.get("pos_sub1") != "接尾"
            or not selects_host(host)
        ):
            merged.append(token)
            continue
        combined = host.get("surface", "") + token.get("surface", "")
        merged[-1] = {"surface": combined, "pos": "名詞", "pos_sub1": "一般", "lemma": combined}
        if applied_rule is None:
            applied_rule = "derivational-nominal-suffix"
    return merged, applied_rule


_CONJUNCTIVE_TE = "て"


def _postprocess_nominal_before_conjunctive_te(
    result: list[dict], applied_rule: str | None
) -> tuple[list[dict], str | None]:
    """Reopen a nominal that swallowed the continuative て attaches to.

    て is only ever the conjunctive particle, and it takes a continuative — a
    nominal in front of it heads no dependency the sentence could have. The
    reference dictionary reaches that position by matching its longest nominal
    headword and then labelling て a case particle, which is the tell: the same
    surface comes back as a verb as soon as a case particle intervenes (夜が明けて
    beside 夜明けて), so the reading turns on the greedy match rather than on the
    grammar. The polite-auxiliary probe recovers the continuative and the
    boundary in front of it, and て goes back to being conjunctive.
    """
    normalized: list[dict] = []
    index = 0
    while index < len(result):
        token = result[index]
        follower = result[index + 1] if index + 1 < len(result) else None
        recovered = (
            _continuative_verb_tokens(token.get("surface", ""))
            if token.get("pos") == "名詞"
            and follower is not None
            and follower.get("surface") == _CONJUNCTIVE_TE
            and follower.get("pos") == "助詞"
            and follower.get("pos_sub1") == "格助詞"
            else None
        )
        if recovered is None:
            normalized.append(token)
            index += 1
            continue
        normalized.extend(recovered)
        normalized.append(
            {"surface": _CONJUNCTIVE_TE, "pos": "助詞", "pos_sub1": "接続助詞", "lemma": _CONJUNCTIVE_TE},
        )
        index += 2
        if applied_rule is None:
            applied_rule = "nominal-before-conjunctive-te"
    return normalized, applied_rule
