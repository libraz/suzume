"""Context-dependent nouns, adverbs, and nominalization."""

import regex

from .constants import (
    ADVERB_NOMINAL_HOMOGRAPHS,
    COPULA_SURFACES,
    TEMPORAL_COMPOUND_UNITS,
    TEMPORAL_PREFIX_KANJI,
)
from .core_lexicon import core_headwords
from .mecab import mecab_analyze
from .merge_postprocessors import NIDAN_TERMINAL_KANA
from .postprocessor_common import reports_mutation


@reports_mutation
def postprocess_ikaga(tokens: list[dict]) -> bool:
    """Context-dependent いかが normalization."""
    for i, t in enumerate(tokens):
        if t.get("surface") != "いかが":
            continue
        has_copula = False
        if i < len(tokens) - 1:
            nxt = tokens[i + 1].get("surface", "")
            if regex.match(r"^(?:です|でし|だ|だっ|でしょ)", nxt):
                has_copula = True
        if not has_copula:
            t["pos"] = "Adverb"


@reports_mutation
def postprocess_tada(tokens: list[dict]) -> bool:
    """Context-dependent ただ normalization.

    The reference dictionary defaults ただ to the clause-opening conjunction
    ("however"), but directly before で it is the adverbial noun meaning "free
    of charge" (ただで手に入る, ただでさえ, ただでは済まない). The conjunction
    reading needs a clause boundary, which shows up as punctuation, so gating on
    the immediately following で keeps it untouched.
    """
    for i, t in enumerate(tokens):
        if t.get("surface") != "ただ" or t.get("pos") != "Conjunction":
            continue
        if i + 1 < len(tokens) and tokens[i + 1].get("surface") == "で":
            t["pos"] = "Adverb"
            t["lemma"] = "ただ"


def postprocess_kadouka_adverb(tokens: list[dict]) -> bool:
    """Keep どう adverbial in the closed interrogative frame か+どう+か."""
    changed = False
    for idx in range(1, len(tokens) - 1):
        token = tokens[idx]
        if (
            token.get("surface") == "どう"
            and token.get("pos") == "Adjective"
            and tokens[idx - 1].get("surface") == "か"
            and tokens[idx + 1].get("surface") == "か"
        ):
            token["pos"] = "Adverb"
            changed = True
    return changed


def postprocess_l2_noun_context(tokens: list[dict]) -> bool:
    """Prefer an L2 noun homograph in contexts that select a nominal."""
    lexical_nouns = core_headwords("nouns.tsv")
    nominal_particles = frozenset({"を", "は", "が", "の", "に", "で", "へ", "と", "も"})
    changed = False
    for idx, token in enumerate(tokens):
        if token.get("surface") not in lexical_nouns or token.get("pos") != "Verb":
            continue
        following = tokens[idx + 1] if idx + 1 < len(tokens) else None
        if following is None:
            continue
        selected_by_particle = following.get("pos") == "Particle" and following.get("surface") in nominal_particles
        selected_by_copula = following.get("pos") == "Auxiliary" and following.get("surface") in COPULA_SURFACES
        if not selected_by_particle and not selected_by_copula:
            continue
        token["pos"] = "Noun"
        token["lemma"] = token["surface"]
        changed = True
    return changed


def postprocess_fuu_formal_noun(tokens: list[dict]) -> bool:
    """Normalize demonstrative + ふう + に as grammatical search units."""
    joined = "".join(token.get("surface", "") for token in tokens)
    match = regex.fullmatch(r"([こそあど]んな)ふうに", joined)
    if match:
        tokens[:] = [
            {"surface": match.group(1), "pos": "Determiner", "lemma": match.group(1)},
            {"surface": "ふう", "pos": "Noun", "lemma": "ふう"},
            {"surface": "に", "pos": "Particle", "lemma": "に"},
        ]
        return True
    for token in tokens:
        if token.get("surface") == "ふう" and token.get("pos") != "Noun":
            token["pos"] = "Noun"
            token["lemma"] = "ふう"
            return True
    return False


def postprocess_hiragana_purpose_noun(tokens: list[dict]) -> bool:
    """Use a nominal search unit for hiragana activity + に + motion verb.

    する is excluded because it names no activity of its own: it only verbalizes
    the nominal in front of it, which is already the search unit (買い物+し+に行く),
    and the bare mora it leaves behind is not a word.
    """
    changed = False
    motion_lemmas = {"行く", "来る", "帰る"}
    for idx in range(len(tokens) - 2):
        token = tokens[idx]
        if (
            token.get("pos") == "Verb"
            and token.get("lemma") != "する"
            and regex.fullmatch(r"\p{Hiragana}+", token.get("surface", ""))
            and tokens[idx + 1].get("surface") == "に"
            and tokens[idx + 2].get("lemma") in motion_lemmas
        ):
            token["pos"] = "Noun"
            token["lemma"] = token.get("surface")
            changed = True
    return changed


def _is_irrealis_before_negative(surface: str) -> bool:
    """Whether a surface is the irrealis stem the negative auxiliary selects.

    書か+なく+ない keeps its verb reading, because the negative attaches to the
    irrealis; 変わり+なく is the continuative that also serves as a deverbal noun.
    The reference dictionary names the difference in the probe's conjugated form.
    """

    probe = mecab_analyze(surface + "ない")
    return (
        len(probe) == 2
        and probe[0].get("surface") == surface
        and probe[0].get("pos") == "動詞"
        and probe[0].get("conj_form") == "未然形"
    )


def postprocess_deverbal_noun_context(tokens: list[dict]) -> bool:
    """Normalize a continuative verb used as the head of a noun phrase.

    A non-finite verb form cannot itself take を/が/の.  When MeCab emits a
    continuative surface immediately before one of those particles, the same
    surface is the productive deverbal noun (読みを, いとなみが, 書きかけの).
    Finite verbs such as 読むの and continuative verb chains remain unchanged.
    The copula likewise selects a nominal predicate (曇りでしょう, 押し付けだ),
    not a bare continuative verb.
    """
    changed = False
    for idx, token in enumerate(tokens[:-1]):
        if token.get("pos") != "Verb":
            continue
        surface = token.get("surface", "")
        lemma = token.get("lemma", surface)
        if not surface or not lemma or surface == lemma:
            continue
        # する derives no noun of its own: it verbalizes the nominal in front of
        # it, which already heads the phrase, so the mora it leaves behind is not
        # a word (勉強+し+に, 読みし+を, where the し is the classical past).
        if lemma == "する":
            continue
        # A classical 二段 連体形 (消ゆる|を) is a finite verb heading its own
        # clause, not the productive deverbal noun a 連用形 spells.
        if surface == lemma + "る" and lemma[-1:] in NIDAN_TERMINAL_KANA:
            continue
        # A terminal spelling, its own final kana or held, is finite too
        # (っ+つう+の for っていうの).
        if surface[-1:] in (lemma[-1:], "ー"):
            continue
        # So is the colloquial る→ん contraction of that terminal (分かん+の).
        if surface.endswith("ん") and lemma.endswith("る") and surface[:-1] == lemma[:-1]:
            continue
        following = tokens[idx + 1]
        honorific_naru = (
            idx > 0
            and tokens[idx - 1].get("pos") == "Prefix"
            and tokens[idx - 1].get("surface") in {"お", "ご", "御"}
            and following.get("surface") == "に"
            and idx + 2 < len(tokens)
            and tokens[idx + 2].get("pos") in {"Verb", "Auxiliary"}
            and tokens[idx + 2].get("lemma") == "なる"
        )
        if honorific_naru:
            continue
        nominal_particle = following.get("pos") == "Particle" and following.get("surface") in {"を", "が", "の"}
        if following.get("surface") == "に":
            after_particle = tokens[idx + 2] if idx + 2 < len(tokens) else None
            motion_lemmas = {"行く", "来る", "いく", "くる", "ゆく"}
            nominal_particle = after_particle is None or after_particle.get("lemma") not in motion_lemmas
        nominal_follower = following.get("surface") in {"方", "ひとつ"}
        # An onbin stem before だ is the voiced past (読ん+だ, 泳い+だ).
        onbin_stem = surface.endswith(("ん", "っ")) or (surface.endswith("い") and lemma.endswith(("く", "ぐ")))
        predicative_copula = following.get("pos") == "Auxiliary" and (
            following.get("surface") in {"でしょ"}
            or (
                not onbin_stem
                and following.get("lemma") in {"だ", "です"}
                and following.get("surface") in {"だ", "だっ", "です", "でし"}
            )
        )
        # ...and so does its topical negative (押し付け+で+は+なく).
        copular_negative = (
            not onbin_stem
            and following.get("surface") == "で"
            and idx + 3 < len(tokens)
            and tokens[idx + 2].get("surface") == "は"
            and tokens[idx + 3].get("surface") in {"ない", "なく", "なかっ"}
        )
        predicative_copula = predicative_copula or copular_negative
        nominal_negative = (
            following.get("pos") == "Adjective"
            and following.get("surface") == "なく"
            and following.get("lemma") == "ない"
            and not _is_irrealis_before_negative(surface)
        )
        if not nominal_particle and not nominal_follower and not predicative_copula and not nominal_negative:
            continue
        token["pos"] = "Noun"
        token["lemma"] = surface
        changed = True
    return changed


def postprocess_attributive_mamonaku(tokens: list[dict]) -> bool:
    """Split temporal 間+も+なく after an attributive predicate.

    Clause-initial 間もなく is a lexical adverb, while 休む間もなく contains
    an independently modified formal noun and two closed grammatical units.
    """
    for idx in range(1, len(tokens)):
        token = tokens[idx]
        if token.get("surface") != "間もなく" or token.get("pos") not in ("Adverb", "Adjective"):
            continue
        if tokens[idx - 1].get("pos") not in ("Verb", "Adjective", "Auxiliary"):
            continue
        tokens[idx : idx + 1] = [
            {"surface": "間", "pos": "Noun", "lemma": "間"},
            {"surface": "も", "pos": "Particle", "lemma": "も"},
            {"surface": "なく", "pos": "Adjective", "lemma": "ない"},
        ]
        return True
    return False


def postprocess_adverb_nominal_context(tokens: list[dict]) -> bool:
    """Restore nominal readings of adverb homographs in particle frames."""
    changed = False
    for idx in range(len(tokens) - 1):
        token = tokens[idx]
        following = tokens[idx + 1]
        if token.get("pos") != "Adverb" or following.get("pos") != "Particle":
            continue
        particle = following.get("surface")
        is_accusative = particle == "を"
        is_lexical_homograph_frame = token.get("surface") in ADVERB_NOMINAL_HOMOGRAPHS and particle in (
            "を",
            "の",
            "は",
            "が",
            "も",
            "に",
            "で",
        )
        if not is_accusative and not is_lexical_homograph_frame:
            continue
        token["pos"] = "Noun"
        token["lemma"] = token.get("surface", "")
        changed = True
    return changed


def postprocess_temporal_nao(tokens: list[dict]) -> bool:
    """Use adverbial なお after a temporal adverb (いまなお)."""
    changed = False
    for idx in range(1, len(tokens)):
        previous = tokens[idx - 1]
        token = tokens[idx]
        if previous.get("pos") == "Adverb" and token.get("surface") == "なお" and token.get("pos") == "Conjunction":
            token["pos"] = "Adverb"
            token["lemma"] = "なお"
            changed = True
    return changed


@reports_mutation
def postprocess_tsuke_noun(tokens: list[dict]) -> bool:
    """Fix 付け: Suffix -> Noun."""
    for t in tokens:
        if t.get("surface") == "付け" and t.get("pos") == "Suffix":
            t["pos"] = "Noun"
            t["lemma"] = "付け"


@reports_mutation
def postprocess_taihen(tokens: list[dict]) -> bool:
    """Fix 大変 before な: Adverb -> Adjective (na-adjective use)."""
    for i, t in enumerate(tokens):
        if t.get("surface") == "大変" and t.get("pos") == "Adverb":
            if i < len(tokens) - 1 and tokens[i + 1].get("surface") == "な":
                t["pos"] = "Adjective"


@reports_mutation
def postprocess_you_noun(tokens: list[dict]) -> bool:
    """Distinguish formal-noun よう from the true volitional auxiliary."""
    for idx, t in enumerate(tokens):
        if t.get("surface") != "よう":
            continue
        if idx > 0 and tokens[idx - 1].get("pos") == "Verb" and t.get("pos") == "Suffix":
            # A mizenkei immediately followed by よう is the ichidan
            # volitional auxiliary (見+よう, 着+よう), not the formal noun.
            # Other verb+よう sequences retain the formal-noun reading
            # (見る+ように, 読む+ようだ).
            if "未然" in (tokens[idx - 1].get("conj_form") or "") or (
                len(tokens[idx - 1].get("surface", "")) == 1
                and idx + 1 < len(tokens)
                and tokens[idx + 1].get("surface") == "に"
            ):
                t["pos"] = "Auxiliary"
            else:
                t["pos"] = "Noun"
        else:
            previous_surface = tokens[idx - 1].get("surface", "") if idx > 0 else ""
            following_surface = tokens[idx + 1].get("surface", "") if idx + 1 < len(tokens) else ""
            formal_context = previous_surface == "の" or following_surface in ("だ", "です", "で", "な", "に")
            if formal_context:
                t["pos"] = "Noun"
                t["lemma"] = "よう"


@reports_mutation
def postprocess_adverbial_temporal_prefix(tokens: list[dict]) -> bool:
    """Restore the adverbial temporal noun standing before an ordinary noun.

    A temporal prefix heads a temporal noun (今週, 今度, 毎時) and nothing else,
    so before an ordinary noun it is the free adverbial noun itself: 今|紙, 今|水.
    The reference analyzer instead reads the pair as a compound and marks its
    parts accordingly — the prefix as 接頭詞 and the following noun as the bound
    element of a compound — which neither part is here.
    """
    changed = False
    for idx, token in enumerate(tokens):
        if idx + 1 >= len(tokens):
            continue
        if token.get("surface") not in TEMPORAL_PREFIX_KANJI:
            continue
        following = tokens[idx + 1]
        if following.get("surface", "")[:1] in TEMPORAL_COMPOUND_UNITS:
            continue
        if token.get("pos") == "Prefix":
            token["pos"] = "Noun"
            changed = True
        if following.get("pos") == "Suffix":
            following["pos"] = "Noun"
            changed = True
    return changed


@reports_mutation
def postprocess_ka_suru_noun(tokens: list[dict]) -> bool:
    """Keep 化-derived suru-verb nouns out of the na-adjective class."""
    for idx, token in enumerate(tokens[:-1]):
        if token.get("pos") != "Adjective" or not token.get("surface", "").endswith("化"):
            continue
        following = tokens[idx + 1]
        if following.get("surface") == "し" and following.get("pos") == "Verb":
            token["pos"] = "Noun"
            token["lemma"] = token.get("surface")


@reports_mutation
def postprocess_prolonged_sound_noun(tokens: list[dict]) -> bool:
    """Keep a single-kanji lexical word after a prolonged mark out of suffix POS."""
    for idx, token in enumerate(tokens):
        if idx == 0 or token.get("pos") != "Suffix" or len(token.get("surface", "")) != 1:
            continue
        if tokens[idx - 1].get("surface", "").endswith("ー"):
            token["pos"] = "Noun"


@reports_mutation
def postprocess_yoshi_formal_noun(tokens: list[dict]) -> bool:
    """Normalize よし as a formal noun in the negative knowledge construction."""
    for idx, token in enumerate(tokens):
        if token.get("surface") != "よし" or token.get("pos") != "Adjective" or idx == 0:
            continue
        if tokens[idx - 1].get("pos") != "Verb" or idx + 2 >= len(tokens):
            continue
        if tokens[idx + 1].get("surface") == "も" and tokens[idx + 2].get("surface") == "ない":
            token["pos"] = "Noun"
            token["lemma"] = "よし"


def postprocess_formal_noun_lemma(tokens: list[dict]) -> bool:
    """Normalize productive formal nouns selected by closed grammar contexts."""
    canonical = {"事": "こと", "物": "もの"}
    changed = False
    for idx in range(len(tokens) - 1):
        if tokens[idx].get("surface") == "ため" and tokens[idx + 1].get("surface") == "しがない":
            tokens[idx : idx + 2] = [
                {"surface": "ためし", "pos": "Noun", "lemma": "ためし"},
                {"surface": "が", "pos": "Particle", "lemma": "が"},
                {"surface": "ない", "pos": "Auxiliary", "lemma": "ない"},
            ]
            changed = True
            break
    for idx, token in enumerate(tokens):
        if token.get("surface") == "どころ" and token.get("pos") == "Suffix":
            token["pos"] = "Noun"
            token["lemma"] = "どころ"
            changed = True
            continue
        if (
            idx > 0
            and token.get("surface") == "ため"
            and tokens[idx - 1].get("surface") == "が"
            and tokens[idx - 1].get("pos") == "Particle"
        ):
            if token.get("pos") != "Noun" or token.get("lemma") != "ため":
                token["pos"] = "Noun"
                token["lemma"] = "ため"
                changed = True
            continue
        lemma = canonical.get(token.get("surface"))
        if lemma is None or token.get("pos") != "Noun" or idx == 0:
            continue
        previous = tokens[idx - 1]
        if previous.get("surface") != "の" and previous.get("pos") not in (
            "Verb",
            "Auxiliary",
            "Adjective",
            "Determiner",
        ):
            continue
        if token.get("lemma") != lemma:
            token["lemma"] = lemma
            changed = True
    return changed


def postprocess_adjective_nominalizer(tokens: list[dict]) -> bool:
    """Classify productive adjective + さ nominalization as a suffix."""
    changed = False
    for idx in range(1, len(tokens)):
        token = tokens[idx]
        previous = tokens[idx - 1]
        following_surface = tokens[idx + 1].get("surface", "") if idx + 1 < len(tokens) else ""
        if (
            token.get("surface") != "さ"
            or previous.get("pos") not in ("Adjective", "Auxiliary")
            or not previous.get("lemma", "").endswith("い")
            or following_surface.startswith(("れ", "せ"))
        ):
            continue
        if token.get("pos") != "Suffix":
            token["pos"] = "Suffix"
            token["lemma"] = "さ"
            changed = True
    return changed


def postprocess_verbal_nominalizer_mi(tokens: list[dict]) -> bool:
    """Classify the productive nominalizing み that MeCab reads as みる.

    The subsidiary verb みる selects a te-form and nothing else, so a み that
    follows a bare continuative or an adjective stem cannot be one. On a verb
    continuative it is the nominalizing suffix (分かり + み + が). On an
    adjective stem it is the property nominal the merge pipeline already
    joins when MeCab tags it as a suffix (嬉しみ, 痛み), so it joins here too
    (しんどみ, 無理み).
    """
    changed = False
    idx = 1
    while idx < len(tokens):
        token = tokens[idx]
        previous = tokens[idx - 1]
        if (
            token.get("surface") != "み"
            or token.get("lemma") != "みる"
            or previous.get("pos") not in ("Verb", "Adjective")
            or previous.get("surface", "").endswith(("て", "で"))
        ):
            idx += 1
            continue
        if previous.get("pos") == "Adjective":
            combined = previous.get("surface", "") + "み"
            tokens[idx - 1 : idx + 1] = [{"surface": combined, "pos": "Noun", "lemma": combined}]
        else:
            token["pos"] = "Suffix"
            token["lemma"] = "み"
            idx += 1
        changed = True
    return changed


def postprocess_determiner_wake_noun(tokens: list[dict]) -> bool:
    """Read わけ after an adnominal as the noun, not the verb 分ける.

    A determiner (そんな, その) modifies a nominal, so そんな+わけ+ない is the
    noun わけ and the existential adjective ない.
    """
    changed = False
    for idx in range(1, len(tokens)):
        token, previous = tokens[idx], tokens[idx - 1]
        if token.get("surface") == "わけ" and token.get("pos") == "Verb" and previous.get("pos") == "Determiner":
            token.update(pos="Noun", lemma="わけ")
            following = tokens[idx + 1] if idx + 1 < len(tokens) else None
            if following is not None and following.get("lemma") == "ない" and following.get("pos") == "Auxiliary":
                following["pos"] = "Adjective"
            changed = True
    return changed
