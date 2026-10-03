"""Context-dependent adjective and predicate forms."""

import regex

from .constants import (
    COPULA_SURFACES,
)
from .core_lexicon import adjective_garu_stems, core_headwords
from .mecab import mecab_analyze
from .pos_mapping import _is_katakana_onomatopoeia
from .postprocessor_common import reports_mutation

_GODAN_ERO_TO_BASE = {
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


@reports_mutation
def postprocess_sou(tokens: list[dict]) -> bool:
    """Read a katakana host of そう as an adjective stem (エモ+そう)."""
    for i, t in enumerate(tokens):
        if t.get("surface") != "そう":
            continue

        # Katakana adjective stem + そう: Noun -> Adjective
        if i > 0:
            prev = tokens[i - 1]
            prev_surface = prev.get("surface", "")
            if (
                prev.get("pos") == "Noun"
                and regex.match(r"^[\u30A0-\u30FF]+$", prev_surface)
                and not _is_katakana_onomatopoeia(prev_surface)
            ):
                prev["pos"] = "Adjective"
                prev["lemma"] = prev_surface + "い"


def postprocess_honorific_i_adjective(tokens: list[dict]) -> bool:
    """Restore an i-adjective ending in -しい after honorific prefix お."""
    changed = False
    for idx in range(1, len(tokens)):
        token = tokens[idx]
        if (
            tokens[idx - 1].get("surface") == "お"
            and tokens[idx - 1].get("pos") == "Prefix"
            and token.get("pos") == "Noun"
            and token.get("surface", "").endswith("しい")
        ):
            token["pos"] = "Adjective"
            token["lemma"] = token["surface"]
            changed = True
    return changed


def postprocess_i_adjective_upper_bound(tokens: list[dict]) -> bool:
    """Restore an i-adjective continuative before the upper-bound particle とも."""
    changed = False
    for idx, token in enumerate(tokens[:-1]):
        surface = token.get("surface", "")
        if token.get("pos") != "Noun" or not surface.endswith("く"):
            continue
        if tokens[idx + 1].get("surface") != "とも":
            continue
        token["pos"] = "Adjective"
        token["lemma"] = surface[:-1] + "い"
        changed = True
    return changed


@reports_mutation
def postprocess_ii(tokens: list[dict]) -> bool:
    """Fix いい: Verb(いう) -> Adjective when not followed by verb."""
    for i, t in enumerate(tokens):
        if t.get("surface") != "いい":
            continue
        if t.get("pos") != "Verb" or t.get("lemma") != "いう":
            continue
        next_is_verb = False
        if i < len(tokens) - 1:
            next_is_verb = tokens[i + 1].get("pos") == "Verb"
        if not next_is_verb:
            t["pos"] = "Adjective"
            t["lemma"] = "いい"


def postprocess_teki_na_adjective(tokens: list[dict]) -> bool:
    """Classify X的 in every na-adjective predicate/inflection position."""
    changed = False
    for idx, token in enumerate(tokens):
        if token.get("pos") != "Noun" or not token.get("surface", "").endswith("的"):
            continue
        following = tokens[idx + 1] if idx + 1 < len(tokens) else None
        if following is not None and following.get("surface") not in COPULA_SURFACES | {"に"}:
            continue
        token["pos"] = "Adjective"
        changed = True
    return changed


def postprocess_chigai_negative_adjective(tokens: list[dict]) -> bool:
    """Keep deverbal 〜違い before ない in its nominal-adjective reading."""
    changed = False
    for idx, token in enumerate(tokens[1:], start=1):
        previous = tokens[idx - 1]
        if token.get("surface") != "ない" or not previous.get("surface", "").endswith("違い"):
            continue
        previous["pos"] = "Noun"
        if previous.get("surface") == "違い":
            previous["lemma"] = "ちがい"
        token["pos"] = "Adjective"
        changed = True
    return changed


def postprocess_adjective_garu(tokens: list[dict]) -> bool:
    """Split and type productive L2 adjective/noun-stem + がる forms."""
    garu_forms = frozenset({"がら", "がり", "がる", "がれ", "がろ", "がっ"})
    adjective_stems = adjective_garu_stems()
    lexical_verbs = core_headwords("verbs.tsv")
    lexical_nouns = core_headwords("nouns.tsv")
    changed = False
    idx = 0
    while idx < len(tokens):
        token = tokens[idx]
        surface = token.get("surface", "")
        if token.get("pos") == "Verb" and surface not in lexical_verbs and token.get("lemma") not in lexical_verbs:
            for form in garu_forms:
                if not surface.endswith(form):
                    continue
                stem = surface[: -len(form)]
                lemma = adjective_stems.get(stem)
                if lemma is not None:
                    tokens[idx : idx + 1] = [
                        {"surface": stem, "pos": "Adjective", "lemma": lemma},
                        {"surface": form, "pos": "Verb", "lemma": "がる"},
                    ]
                    changed = True
                    idx += 1
                break
        idx += 1

    for idx in range(1, len(tokens)):
        token = tokens[idx]
        previous = tokens[idx - 1]
        if token.get("surface") not in garu_forms:
            continue
        adjective_lemma = adjective_stems.get(previous.get("surface", ""))
        noun_host = previous.get("pos") == "Noun" and previous.get("surface") in lexical_nouns
        if previous.get("pos") != "Adjective" and adjective_lemma is None and not noun_host:
            continue
        if adjective_lemma is not None and (
            previous.get("pos") != "Adjective" or previous.get("lemma") != adjective_lemma
        ):
            previous["pos"] = "Adjective"
            previous["lemma"] = adjective_lemma
            changed = True
        if token.get("pos") != "Verb" or token.get("lemma") != "がる":
            token["pos"] = "Verb"
            token["lemma"] = "がる"
            changed = True
    return changed


def postprocess_short_hiragana_onbin(tokens: list[dict]) -> bool:
    """Normalize a short pure-hiragana 撥音便 immediately before だ/で.

    After a te-form, たん is the contracted progressive's past た plus the
    nominalizer ん (食べてたんだ), never an onbin verb.
    """
    changed = False
    for idx in range(len(tokens) - 1):
        token = tokens[idx]
        surface = token.get("surface", "")
        if (
            surface == "たん"
            and idx > 0
            and tokens[idx - 1].get("surface") in ("て", "で")
            and tokens[idx - 1].get("pos") == "Particle"
            and token.get("pos") in ("Noun", "Verb")
        ):
            tokens[idx : idx + 1] = [
                {"surface": "た", "pos": "Auxiliary", "lemma": "た"},
                {"surface": "ん", "pos": "Particle", "lemma": "の"},
            ]
            # The same て before a bare た is the contracted てる (書いてた).
            if tokens[idx - 1]["surface"] == "て":
                tokens[idx - 1].update(pos="Auxiliary", lemma="てる")
            changed = True
            continue
        if (
            len(surface) == 2
            and surface.endswith("ん")
            and regex.fullmatch(r"\p{Hiragana}+", surface)
            and tokens[idx].get("pos") in ("Noun", "Verb")
            and tokens[idx + 1].get("surface") in ("だ", "で")
        ):
            lemma = token.get("lemma", "")
            has_valid_onbin_lemma = (
                token.get("pos") == "Verb" and lemma[:-1] == surface[:-1] and lemma.endswith(("む", "ぶ", "ぬ"))
            )
            if has_valid_onbin_lemma:
                continue
            token["pos"] = "Verb"
            token["lemma"] = surface[:-1] + "む"
            changed = True
    return changed


def postprocess_hiragana_godan_wa_terminal(tokens: list[dict]) -> bool:
    """Merge a pure-hiragana Godan-wa base split from final auxiliary う."""
    if len(tokens) != 2 or tokens[0].get("pos") != "Verb" or tokens[1].get("surface") != "う":
        return False
    stem = tokens[0].get("surface", "")
    # An o-row stem followed by う is normally a volitional form (e.g. 書こう),
    # not a dictionary-form Godan-wa verb split at its final vowel.
    if stem and stem[-1] in "おこそとのほもよろをごぞどぼぽょ":
        return False
    surface = stem + "う"
    if len(surface) < 3 or not regex.fullmatch(r"\p{Hiragana}+", surface):
        return False
    tokens[:] = [{"surface": surface, "pos": "Verb", "lemma": surface}]
    return True


def postprocess_onaji_predicate(tokens: list[dict]) -> bool:
    """Normalize predicative 同じ across the complete copula paradigm.

    The reference also fuses kana おなじ with its attributive copula into one
    adverb (おなじなんです); that is the same predicate and is split back.
    """
    changed = False
    idx = 0
    while idx < len(tokens):
        if tokens[idx].get("surface") == "おなじな" and tokens[idx].get("pos") == "Adverb":
            tokens[idx : idx + 1] = [
                {"surface": "おなじ", "pos": "Adjective", "lemma": "おなじ"},
                {"surface": "な", "pos": "Auxiliary", "lemma": "だ"},
            ]
            changed = True
        idx += 1
    for idx, token in enumerate(tokens):
        following = tokens[idx + 1] if idx + 1 < len(tokens) else None
        if (
            token.get("surface") in ("同じ", "おなじ")
            and token.get("pos") in ("Determiner", "Noun")
            and (following is None or following.get("surface") in COPULA_SURFACES)
        ):
            token["pos"] = "Adjective"
            token["lemma"] = token["surface"]
            changed = True
    return changed


def postprocess_na_adj_noun(tokens: list[dict]) -> bool:
    """Treat a bare na-adjective stem in a syntactic noun position as a noun.

    An i-adjective cannot directly take を, while a na-adjective stem can be
    used nominally (for example, 平静を保つ). A predicate immediately before
    the stem also closes a relative clause, making the following stem its
    nominal head (落ち着いた雰囲気). These are syntactic corrections, not
    lexical exceptions; adjective readings before な/に/すぎる remain untouched.
    """
    changed = False
    for idx, token in enumerate(tokens):
        if token.get("pos") != "Adjective":
            continue
        lemma = token.get("lemma", token.get("surface", ""))
        if lemma.endswith("い"):
            continue
        following = tokens[idx + 1] if idx + 1 < len(tokens) else None
        follows_past_relative_clause = (
            idx > 0
            and tokens[idx - 1].get("pos") == "Auxiliary"
            and tokens[idx - 1].get("lemma") == "た"
            and following is None
        )
        precedes_accusative = idx + 1 < len(tokens) and tokens[idx + 1].get("surface") == "を"
        if not follows_past_relative_clause and not precedes_accusative:
            continue
        token["pos"] = "Noun"
        token["lemma"] = token.get("surface", "")
        changed = True
    return changed


def postprocess_hiragana_yaka_adverbial(tokens: list[dict]) -> bool:
    """Repair a split hiragana na-adjective in the productive 〜やかに form."""
    for idx in range(len(tokens) - 1):
        combined = tokens[idx].get("surface", "") + tokens[idx + 1].get("surface", "")
        if len(combined) < 4 or not combined.endswith("やかに") or not regex.fullmatch(r"\p{Hiragana}+", combined):
            continue
        adjective = combined[:-1]
        tokens[idx : idx + 2] = [
            {"surface": adjective, "pos": "Adjective", "lemma": adjective},
            {"surface": "に", "pos": "Particle", "lemma": "に"},
        ]
        return True
    return False


def postprocess_modifier_godan_imperative(tokens: list[dict]) -> bool:
    """Restore a Godan imperative misread as an Ichidan stem after a modifier."""
    changed = False
    for idx in range(1, len(tokens)):
        previous, token = tokens[idx - 1], tokens[idx]
        following = tokens[idx + 1] if idx + 1 < len(tokens) else None
        surface = token.get("surface", "")
        base_suffix = _GODAN_ERO_TO_BASE.get(surface[-1:])
        # A connective particle (て, ば, etc.) continues the predicate and
        # cannot license an imperative reading.  Only sentence-final particles
        # retain that interpretation (e.g. 待てよ).
        final_particle = following is not None and following.get("surface") in {
            "よ",
            "ね",
            "ぞ",
            "ぜ",
            "か",
            "な",
            "わ",
            "さ",
        }
        clause_final = following is None or following.get("pos") == "Symbol" or final_particle
        if (
            previous.get("pos") in ("Adverb", "Adjective")
            and token.get("pos") == "Verb"
            and base_suffix is not None
            and token.get("lemma") == surface + "る"
            and clause_final
        ):
            token["lemma"] = surface[:-1] + base_suffix
            changed = True
    return changed


def postprocess_difficulty_adjective_stem(tokens: list[dict]) -> bool:
    """Normalize にく before さ as the productive difficulty adjective stem."""
    changed = False
    for idx, token in enumerate(tokens[:-1]):
        if token.get("surface") != "にく" or tokens[idx + 1].get("surface") != "さ":
            continue
        token["pos"] = "Adjective"
        token["lemma"] = "にくい"
        changed = True
    return changed


@reports_mutation
def postprocess_bound_derived_adjective(tokens: list[dict]) -> bool:
    """Rejoin the bound suffix がまし〜 when it was split at its first mora.

    がまし〜 derives an i-adjective from a nominal host (未練がましい, 恩着せがましく).
    The reference dictionary knows a few of those adjectives lexically and keeps
    them whole, but for every other host it falls back to the case particle が
    plus a remainder that is not a word at all, so the same suffix is analyzed
    two ways depending on which host it sits on.

    Only an adjective cell licenses the merge: the nominal まし takes the copula
    instead (こちらの方がましだ), and that が really is the subject marker. Runs at
    the very end of the pipeline because the compound merges that assemble the
    host come first, and they read the same が.
    """
    cells = ("ましい", "ましく", "ましかっ", "ましけれ", "ましかろ")
    nominalized_cell = "まし"
    changed = False
    idx = 1
    while idx + 1 < len(tokens):
        host = tokens[idx - 1]
        particle = tokens[idx]
        suffix = tokens[idx + 1]
        follower = tokens[idx + 2].get("surface") if idx + 2 < len(tokens) else None
        licensed = suffix.get("surface", "") in cells or (
            suffix.get("surface", "") == nominalized_cell and follower == "さ"
        )
        if host.get("pos") not in ("Noun", "Verb") or particle.get("surface") != "が" or not licensed:
            idx += 1
            continue
        host["surface"] = host.get("surface", "") + particle.get("surface", "") + suffix.get("surface", "")
        host["pos"] = "Adjective"
        host["lemma"] = host["surface"].removesuffix(suffix.get("surface", "")) + "ましい"
        del tokens[idx : idx + 2]
        changed = True
    return changed


def postprocess_adverbial_na_adjective(tokens: list[dict]) -> bool:
    """Tag a degree word as an adjective in the cells its copula supplies.

    A word such as 大変 is an adverb and an adjectival noun at once. The
    reference dictionary already tags the adjectival reading before the
    attributive な, but keeps the adverb tag before the terminal だ, so one
    paradigm is split across two parts of speech by cell rather than by
    grammar. Only the copula licenses the change; a directly modified predicate
    keeps the adverb (大変おいしい). The conjunction tag is admitted for the
    same reason as the adverb one: neither class can be the subject of a
    copula, so a word from the set carrying it in that cell is the adjectival
    reading (もっとも+です).
    """
    from .constants import ADVERBIAL_NA_ADJECTIVES

    changed = False
    for idx, token in enumerate(tokens[:-1]):
        follower = tokens[idx + 1]
        if (
            token.get("surface") not in ADVERBIAL_NA_ADJECTIVES
            or token.get("pos") not in ("Adverb", "Conjunction")
            or follower.get("pos") != "Auxiliary"
            or follower.get("surface") not in ("だ", "です", "な", "でし", "だっ", "なら")
        ):
            continue
        token["pos"] = "Adjective"
        changed = True
    return changed


_EASE_ADJECTIVE_HEADS = ("やすい", "にくい", "づらい", "がたい")


def postprocess_verb_ease_adjective(tokens: list[dict]) -> bool:
    """Split a one-kanji verb continuative off a lexicalized ease adjective.

    The reference holds 見やすい and 見にくい as headwords while it analyzes
    着+やすい and 読み+やすい, so the same continuative+やすい derivation comes
    out two ways.  A one-kanji Ichidan continuative is split like the rest.
    """
    changed = False
    for idx in range(1, len(tokens) - 1):
        # The reference also reads the stem やす as an auxiliary before そう+だ
        # (読み+やす+そう+だ, but 読み+やす(やすい)+そう+な) and before a final
        # さ, which it then reads as the particle (読み+やす+さ, but 見+にく+さ
        # with the nominalizer).  The honorific imperative お読みやす is a
        # different auxiliary and has neither follower.
        token = tokens[idx]
        follower = tokens[idx + 1]
        if (
            token.get("surface") == "やす"
            and token.get("pos") == "Auxiliary"
            and tokens[idx - 1].get("pos") == "Verb"
            and follower.get("surface") in ("そう", "さ")
        ):
            token.update(pos="Adjective", lemma="やすい")
            if follower.get("surface") == "さ":
                follower.update(pos="Suffix", lemma="さ")
            changed = True
    idx = 0
    while idx < len(tokens):
        token = tokens[idx]
        lemma = token.get("lemma", "")
        surface = token.get("surface", "")
        idx += 1
        if token.get("pos") != "Adjective" or len(lemma) < 4 or lemma[1:] not in _EASE_ADJECTIVE_HEADS:
            continue
        if len(surface) < 2 or surface[0] != lemma[0] or not regex.match(r"\p{Han}", lemma[0]):
            continue
        probe = mecab_analyze(lemma[0] + "る")
        if len(probe) != 1 or probe[0].get("pos") != "動詞" or probe[0].get("lemma") != lemma[0] + "る":
            continue
        tokens[idx - 1 : idx] = [
            {"surface": surface[0], "pos": "Verb", "lemma": lemma[0] + "る"},
            {"surface": surface[1:], "pos": "Adjective", "lemma": lemma[1:]},
        ]
        idx += 1
        changed = True
    return changed


def postprocess_kusai_garu(tokens: list[dict]) -> bool:
    """Restore the -くさい adjective stem before がる (面倒くさ+がら+ず).

    The reference cuts the stem after く and reads the rest as さがる or as
    さ+がる; the stem is the adjective's, exactly as in 恥ずかし+がら.
    """
    from .split_rules import _is_single_i_adjective

    changed = False
    idx = -1
    while idx + 2 < len(tokens):
        idx += 1
        token, following = tokens[idx], tokens[idx + 1]
        surface = token.get("surface", "")
        if token.get("pos") != "Adjective" or not surface.endswith("く"):
            continue
        stem = surface + "さ"
        if following.get("pos") == "Verb" and following.get("lemma") == "さがる":
            garu = {"surface": following["surface"][1:], "pos": "Verb", "lemma": "がる"}
            span = 2
        elif following.get("surface") == "さ" and idx + 2 < len(tokens) and tokens[idx + 2].get("lemma") == "がる":
            garu = {"surface": tokens[idx + 2]["surface"], "pos": "Verb", "lemma": "がる"}
            span = 3
        else:
            continue
        if not _is_single_i_adjective(stem + "い"):
            continue
        tokens[idx : idx + span] = [{"surface": stem, "pos": "Adjective", "lemma": stem + "い"}, garu]
        changed = True
    return changed
