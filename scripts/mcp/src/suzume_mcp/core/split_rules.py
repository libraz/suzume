"""Split rules ported from SuzumeUtils.pm apply_suzume_split()."""

import regex

from . import split_boundaries
from .constants import (
    COMPOUND_VERB_V2_ICHIDAN,
    COPULAR_PREDICATE_HEADS,
    DERIVED_VERB_SUFFIX_LEMMAS,
    FIXED_FUNCTION_SEARCH_UNITS,
    FIXED_LEADING_SEARCH_UNITS,
    LEXICALIZED_CAUSATIVE_SU_LEMMAS,
    LEXICALIZED_DERIVED_VERBS,
    LITERARY_VOLITIONAL_PARTICLE_COMPOUNDS,
    NOUN_NAI_COMPOUND_ADJECTIVES,
    STATE_NOUN_SUFFIXES,
    TTARA_STEMS,
    TTEBA_STEMS,
    USER_DICT_COMPOUNDS,
)
from .core_lexicon import core_headwords
from .mecab import is_single_token_of_pos, mecab_analyze
from .split_boundaries import (
    _emit_split,
    _reads_as_one_noun,
    _reanalyze_exact,
    _split_lexicalized_morpheme_boundaries,
    _split_quotative_headword,
    _splits_causative_cell_before_past,
)

# Exports from split_boundaries.
_is_single_i_adjective = split_boundaries._is_single_i_adjective
base_from_mizenkei = split_boundaries.base_from_mizenkei
base_from_renyokei = split_boundaries.base_from_renyokei
bases_from_renyokei = split_boundaries.bases_from_renyokei


# A plain 名詞-一般 host for re-reading a copula span. It carries no reading
# that could fuse with what follows, so whatever the probe returns after it is
# the copula frame alone.
_COPULA_HOST_PROBE = "学生"


# A case particle followed by an inflected lexical predicate that a reference
# dictionary lexicalizes as one 連語, inconsistently: the same surface is split
# in some contexts and kept whole in others.  The internal boundary is a real
# inflection boundary (もっ = 持つ continuative, すれ = する conditional), so it is
# always restored.
_LEXICALIZED_PREDICATE_COMPOUNDS: dict[str, tuple[dict, ...]] = {
    "をもって": (
        {"surface": "を", "pos": "助詞", "lemma": "を"},
        {"surface": "もっ", "pos": "動詞", "lemma": "もつ"},
        {"surface": "て", "pos": "助詞", "lemma": "て"},
    ),
    "とすれば": (
        {"surface": "と", "pos": "助詞", "lemma": "と"},
        {"surface": "すれ", "pos": "動詞", "lemma": "する"},
        {"surface": "ば", "pos": "助詞", "lemma": "ば"},
    ),
}


_COMPLETIVE_TSUKUSU_FORMS = frozenset({"尽くさ", "尽くし", "尽くす", "尽くせ", "尽くそ"})


# Two causative auxiliaries share these cells, and which one a cell belongs to is
# not a choice this rule gets to make: wherever the reference analyzer splits the
# boundary itself it reads the e-row cell as the modern ichidan せる (書か+せ+て,
# 待た+せ+て, 書か+せ+ば) and the a-row cell before the passive as the four-grade す
# (書か+さ+れる).  Restoring the boundary must not change the lemma along with it,
# or the same auxiliary comes back with two headwords depending only on whether
# the host happened to be lexicalized.
_CAUSATIVE_SU = "す"


_CAUSATIVE_TAIL_LEMMAS = {"せ": "せる"}


# The conditional particle is where the reference analyzer prefers to read an
# e-row cell as the hypothetical of a godan-す verb, which is what makes the same
# host come back whole here and split before the past auxiliary.
_HYPOTHETICAL_PARTICLE = "ば"


# A predicate closed by a volitional auxiliary cannot host a case particle, so
# a として that follows one is the quotative と plus the te-form of する -- in the
# modern spelling and in the classical む alike.
_VOLITIONAL_AUXILIARIES = frozenset({"う", "よう", "まい", "む", "ん"})


# An adjective spelled as a げ noun plus a cell of ない.
_GE_NAI_ADJECTIVE = regex.compile(r"(.+げ)(な(?:い|く|かっ|けれ|さ|き))")


def apply_suzume_split(tokens: list[dict]) -> tuple[list[dict], str | None]:
    """Apply Suzume split rules to MeCab tokens.

    Returns:
        Tuple of (split tokens, applied rule name or None).
    """
    result: list[dict] = []
    applied_rule: str | None = None
    causative_volitional_u_pending = False

    for token_index, t in enumerate(tokens):
        surface = t.get("surface", "")

        if causative_volitional_u_pending:
            causative_volitional_u_pending = False
            if surface == "う":
                result.append({"surface": "よう", "pos": "助動詞", "lemma": "よう"})
                continue

        lexicalized_parts = _split_lexicalized_morpheme_boundaries(t)
        if lexicalized_parts is not None:
            result.extend(lexicalized_parts)
            if applied_rule is None:
                applied_rule = "lexicalized-morpheme-boundary"
            continue

        # 無くなる is a lexicalized reference headword, but its adjective
        # continuative plus なる boundary is productive (良くなる, 高くなる).
        if t.get("pos") == "動詞" and t.get("lemma") in ("無くなる", "なくなる"):
            stem = surface[:-2] if surface.endswith("なる") else surface[:-2] if surface.endswith("なっ") else ""
            tail = surface[len(stem) :]
            if stem and tail in ("なる", "なっ"):
                adjective_lemma = "無い" if stem.startswith("無") else "ない"
                result.append({"surface": stem, "pos": "形容詞", "lemma": adjective_lemma})
                result.append({"surface": tail, "pos": "動詞", "lemma": "なる"})
                if applied_rule is None:
                    applied_rule = "nakunaru-inflection-boundary"
                continue

        # IPADIC lexicalizes this entire interrogative nominal phrase as an
        # adverb. Suzume keeps its productive pronoun/particle/noun boundaries;
        # the final か is the indefinite adverbial particle.
        if t.get("pos") == "副詞" and surface == "いつの間にか":
            result.extend(
                [
                    {"surface": "いつ", "pos": "名詞", "pos_sub1": "代名詞", "lemma": "いつ"},
                    {"surface": "の", "pos": "助詞", "lemma": "の"},
                    {"surface": "間", "pos": "名詞", "lemma": "間"},
                    {"surface": "に", "pos": "助詞", "lemma": "に"},
                    {"surface": "か", "pos": "助詞", "lemma": "か"},
                ]
            )
            if applied_rule is None:
                applied_rule = "interrogative-nominal-adverb-boundary"
            continue

        # 従う is intransitive and reaches its complement through に, so a bare
        # noun sitting directly in front of it cannot be that complement. What
        # the span actually spells is the sa-hen continuative plus the
        # desiderative (確認+し+たがっ+て+いる). The reference analyzer already
        # reads it that way in every cell whose surface does not collide with
        # an onbin form of 従う (確認+し+たがる).
        if (
            t.get("pos") == "動詞"
            and t.get("lemma") in ("従う", "したがう")
            and surface.startswith("し")
            and len(surface) > 1
            and token_index > 0
            and tokens[token_index - 1].get("pos") == "名詞"
        ):
            result.extend(
                [
                    {"surface": "し", "pos": "動詞", "lemma": "する"},
                    {"surface": surface[1:], "pos": "助動詞", "lemma": "たがる"},
                ]
            )
            if applied_rule is None:
                applied_rule = "sahen-desiderative-boundary"
            continue

        # ます is an auxiliary, so it is never part of a particle, and it selects
        # a continuative. A compound particle lexicalized together with its polite
        # form (に関しまして, に際しまして) therefore spells the case particle plus
        # the verb continuative plus ます; re-reading the head with ます restored
        # recovers that continuative (に+関し+まし+て).
        if t.get("pos") == "助詞" and surface.endswith("まして") and len(surface) > 3:
            head = surface[:-3]
            head_tokens = [{"surface": head, "pos": "助詞", "lemma": head}]
            # ます selects a continuative, so even a head the dictionary lists as
            # one particle (に関し) is the case particle plus the verb here.
            polite = mecab_analyze(head + "ます")
            if (
                len(polite) > 1
                and polite[-1].get("surface") == "ます"
                and "".join(part.get("surface", "") for part in polite) == head + "ます"
            ):
                head_tokens = [dict(part) for part in polite[:-1]]
            result.extend(
                [
                    *head_tokens,
                    {"surface": "まし", "pos": "助動詞", "lemma": "ます"},
                    {"surface": "て", "pos": "助詞", "lemma": "て"},
                ]
            )
            if applied_rule is None:
                applied_rule = "polite-compound-particle-boundary"
            continue

        # たり is a productive classical auxiliary, so its attributive cell is a
        # morpheme boundary no matter which stem carries it. The reference
        # dictionary lexicalizes some of these stems as whole adnominals
        # (堂々たる, 確固たる) while reading the identically built 純然たる as
        # stem plus auxiliary, which makes one paradigm tokenize two ways.
        # A stem the dictionary reads as one content word is the evidence that
        # the たる is the auxiliary and not part of a lexical adnominal
        # (名だたる keeps its たる because 名だ is not one word).
        if t.get("pos") == "連体詞" and surface.endswith("たる") and len(surface) > 3:
            stem_analysis = mecab_analyze(surface[:-2])
            if (
                len(stem_analysis) == 1
                and stem_analysis[0].get("surface") == surface[:-2]
                and stem_analysis[0].get("pos") in ("名詞", "副詞")
            ):
                result.extend(
                    [
                        dict(stem_analysis[0]),
                        {"surface": "たる", "pos": "助動詞", "lemma": "たり"},
                    ]
                )
                if applied_rule is None:
                    applied_rule = "tari-attributive-boundary"
                continue

        # An auxiliary closes a predicate, so nothing nominal can attach to it
        # directly. A noun-labelled token sitting right after one and opening
        # with だ is the copula plus its conjunctive particle, which the
        # reference dictionary only reads that way when the host is a plain
        # noun (べき+だし comes back as the seasoning, 学生+だし does not).
        # Re-reading the same span on a nominal host restores the boundary
        # without naming the particles it can carry.
        if (
            t.get("pos") == "名詞"
            and surface.startswith("だ")
            and len(surface) > 1
            and token_index > 0
            and tokens[token_index - 1].get("pos") == "助動詞"
        ):
            probe = mecab_analyze(_COPULA_HOST_PROBE + surface)
            if (
                len(probe) > 2
                and probe[0].get("surface") == _COPULA_HOST_PROBE
                and probe[1].get("surface") == "だ"
                and probe[1].get("pos") == "助動詞"
                and "".join(part.get("surface", "") for part in probe[1:]) == surface
            ):
                result.extend(dict(part) for part in probe[1:])
                if applied_rule is None:
                    applied_rule = "copula-after-auxiliary-boundary"
                continue

        lexicalized_compound = _LEXICALIZED_PREDICATE_COMPOUNDS.get(surface)
        if lexicalized_compound is not None and t.get("pos") in ("助詞", "接続詞"):
            result.extend(dict(part) for part in lexicalized_compound)
            if applied_rule is None:
                applied_rule = "lexicalized-particle-predicate-boundary"
            continue

        # A productive causative may be lexicalized as a Godan-す verb
        # (待たさ/行かさ) even though its a-row host plus す auxiliary is the
        # same boundary that the reference analyzer exposes for 書かさ.  The
        # host is recoverable from the a-row stem, so this is not a word list.
        # Before the passive the same boundary is productive for any host the
        # a-row stem reconstructs, so the frame decides -- except where the す
        # form is a lexical transitive in its own right (動かす). The core
        # lexicon carries those, and the analysis they license (動かす + passive)
        # is the one a search unit wants.
        following_token = tokens[token_index + 1] if token_index + 1 < len(tokens) else None
        precedes_passive = following_token is not None and following_token.get("lemma") in ("れる", "られる")
        causative_su_lemma = t.get("lemma") or ""
        productive_causative = causative_su_lemma.endswith("す") and causative_su_lemma not in core_headwords(
            "verbs.tsv"
        )
        productive_causative_passive = precedes_passive and productive_causative
        # The conditional is the one frame that reads this cell against the grain
        # of the past frame, so the boundary is restored there on the same terms:
        # the す form must not be a lexical transitive of its own, and the past
        # frame must be where the reference analyzer actually exposes the cell.
        productive_causative_conditional = (
            following_token is not None
            and following_token.get("surface") == _HYPOTHETICAL_PARTICLE
            and productive_causative
            and _splits_causative_cell_before_past(surface)
        )
        if t.get("pos") == "動詞" and (
            causative_su_lemma in LEXICALIZED_CAUSATIVE_SU_LEMMAS
            or productive_causative_passive
            or productive_causative_conditional
        ):
            causative_tail = next((form for form in ("さ", "し", "す", "せ") if surface.endswith(form)), "")
            causative_stem = surface[: -len(causative_tail)] if causative_tail else ""
            causative_base = base_from_mizenkei(causative_stem)
            base_tokens = _reanalyze_exact(causative_base) if causative_base is not None else None
            if base_tokens is not None and len(base_tokens) == 1 and base_tokens[0].get("pos") == "動詞":
                result.append({"surface": causative_stem, "pos": "動詞", "lemma": causative_base})
                # Before ば the e-row cell is the hypothetical of す itself (飛ば+せ+ば);
                # せる's hypothetical is せれ.
                tail_lemma = (
                    _CAUSATIVE_SU
                    if productive_causative_conditional
                    else _CAUSATIVE_TAIL_LEMMAS.get(causative_tail, _CAUSATIVE_SU)
                )
                result.append({"surface": causative_tail, "pos": "助動詞", "lemma": tail_lemma})
                if applied_rule is None:
                    applied_rule = "productive-causative-su-boundary"
                continue

        # The productive Godan causative volitional is mizenkei + せ + よう.
        # A reference dictionary can split its tail as the unrelated サ変
        # imperative せよ + う, or lexicalize the whole causative stem.  Both
        # spellings preserve the same auxiliary boundary once the preceding
        # a-row stem reconstructs to a Godan base.
        has_following_volitional_u = token_index + 1 < len(tokens) and tokens[token_index + 1].get("surface") == "う"
        lexicalized_causative_stem = surface[:-2] if surface.endswith("せよ") else ""
        lexicalized_base = base_from_mizenkei(lexicalized_causative_stem)
        previous_surface = tokens[token_index - 1].get("surface", "") if token_index > 0 else ""
        standalone_causative_tail = (
            surface == "せよ" and t.get("pos") == "動詞" and base_from_mizenkei(previous_surface) is not None
        )
        if has_following_volitional_u and (lexicalized_base is not None or standalone_causative_tail):
            if lexicalized_base is not None:
                result.append({"surface": lexicalized_causative_stem, "pos": "動詞", "lemma": lexicalized_base})
            result.append({"surface": "せ", "pos": "助動詞", "lemma": "せる"})
            causative_volitional_u_pending = True
            if applied_rule is None:
                applied_rule = "productive-causative-volitional-boundary"
            continue

        # A causative conditional is the host's Godan mizenkei followed by the
        # auxiliary せる in katei-kei.  Reference dictionaries may lexicalize
        # the pair as one verb (遊ばせれ), but its internal boundary remains
        # productive for every derivable a-row stem.
        if t.get("pos") == "動詞" and surface.endswith("せれ") and len(surface) > 2:
            causative_stem = surface[:-2]
            base = base_from_mizenkei(causative_stem)
            if base is not None:
                result.append({"surface": causative_stem, "pos": "動詞", "lemma": base})
                result.append({"surface": "せれ", "pos": "助動詞", "lemma": "せる"})
                if applied_rule is None:
                    applied_rule = "productive-causative-conditional-boundary"
                continue

        # Productive negative auxiliaries keep their boundary even when a
        # reference dictionary lexicalizes the entire compound.  Restrict the
        # reconstruction to the closed V2 class, leaving ordinary lexical
        # adjectives ending in ない untouched.
        if t.get("pos") == "形容詞" and surface.endswith("ない"):
            negative_stem = surface[:-2]
            split_compound_negative = False
            for v2_base in COMPOUND_VERB_V2_ICHIDAN:
                v2_stem = v2_base[:-1] if v2_base.endswith("る") else ""
                if v2_stem and negative_stem.endswith(v2_stem) and len(negative_stem) > len(v2_stem):
                    result.append({"surface": negative_stem, "pos": "動詞", "lemma": negative_stem + "る"})
                    result.append({"surface": "ない", "pos": "助動詞", "lemma": "ない"})
                    if applied_rule is None:
                        applied_rule = "productive-compound-negative"
                    split_compound_negative = True
                    break
            if split_compound_negative:
                continue

        # Classical negative ぬ is a separate auxiliary after a derivable
        # Godan irrealis stem, including lexicalized attributive spellings.
        if surface.endswith("ぬ") and len(surface) > 1:
            negative_stem = surface[:-1]
            base = base_from_mizenkei(negative_stem)
            if base is not None:
                result.append({"surface": negative_stem, "pos": "動詞", "lemma": base})
                result.append({"surface": "ぬ", "pos": "助動詞", "lemma": "ぬ"})
                if applied_rule is None:
                    applied_rule = "classical-negative-boundary"
                continue

        # Productive renyokei + 尽くす keeps the subsidiary-verb boundary.
        # Reference dictionaries may lexicalize the whole expression, but the
        # same closed completive paradigm attaches to arbitrary verb stems.
        completive_form = next(
            (form for form in _COMPLETIVE_TSUKUSU_FORMS if surface.endswith(form) and len(surface) > len(form)),
            "",
        )
        if completive_form:
            stem = surface[: -len(completive_form)]
            lemma = base_from_renyokei(stem)
            if lemma is not None:
                result.append({"surface": stem, "pos": "動詞", "lemma": lemma})
                result.append({"surface": completive_form, "pos": "助動詞", "lemma": "尽くす"})
                if applied_rule is None:
                    applied_rule = "productive-completive-tsukusu"
                continue

        # Productive renyokei + たて (freshly completed).  Reference
        # dictionaries inconsistently lexicalize the whole expression as a
        # noun or as a compound verb.  Recover the same grammatical boundary
        # for any derivable continuative stem instead of listing host verbs.
        following_surface = tokens[token_index + 1].get("surface", "") if token_index + 1 < len(tokens) else ""
        verb_inflection_followers = frozenset({"て", "た", "たり", "ない", "なかっ", "ぬ", "ます", "まし"})
        if (
            surface.endswith("たて")
            and len(surface) > len("たて")
            and following_surface not in verb_inflection_followers
        ):
            stem = surface[: -len("たて")]
            lemma = base_from_renyokei(stem)
            if lemma is not None:
                result.append({"surface": stem, "pos": "動詞", "lemma": lemma})
                result.append({"surface": "たて", "pos": "接尾辞", "pos_sub1": "接尾", "lemma": "たて"})
                if applied_rule is None:
                    applied_rule = "productive-tate-suffix"
                continue

        # Noun-forming state suffix (泥/まみれ, 開け/っぱなし). The reference
        # dictionary holds the lexicalized hosts as one token and splits every
        # other host, but the suffix is productive and nothing else ends in it,
        # so the boundary is always there.
        state_suffix = next((suf for suf in STATE_NOUN_SUFFIXES if surface.endswith(suf)), "")
        if t.get("pos") == "名詞" and state_suffix and len(surface) > len(state_suffix):
            stem = surface[: -len(state_suffix)]
            result.append({"surface": stem, "pos": "名詞", "lemma": stem})
            result.append({"surface": state_suffix, "pos": "名詞", "pos_sub1": "接尾", "lemma": state_suffix})
            if applied_rule is None:
                applied_rule = "state-noun-suffix"
            continue

        # Degree suffix げ over an adjective stem or an adjectival noun
        # (楽し/げ, おぼろ/げ). Lexicalized hosts (誇らしげ, 得意げ) reach us as
        # one 形容動詞語幹 token; the suffix is the same productive one, so the
        # host keeps its own search boundary.
        adjective_ge = (
            surface.endswith("しげ") and len(surface) > len("しげ") and _is_single_i_adjective(surface[:-1] + "い")
        )
        if (
            t.get("pos") == "名詞"
            and surface.endswith("げ")
            and len(surface) > 1
            and (t.get("pos_sub1") == "形容動詞語幹" or adjective_ge)
        ):
            stem = surface[:-1]
            if stem.endswith("し"):
                result.append({"surface": stem, "pos": "形容詞", "lemma": stem + "い"})
            else:
                result.append({"surface": stem, "pos": "名詞", "lemma": stem})
            result.append({"surface": "げ", "pos": "名詞", "pos_sub1": "接尾", "lemma": "げ"})
            if applied_rule is None:
                applied_rule = "degree-suffix-ge"
            continue

        # A closed leading modifier/adverb can be swallowed by a following
        # noun in the reference dictionary. Restore the grammatical search
        # boundary without enumerating the open-class noun on the right.
        leading_unit = next(
            (unit for unit in sorted(FIXED_LEADING_SEARCH_UNITS, key=len, reverse=True) if surface.startswith(unit)),
            "",
        )
        if leading_unit and len(surface) > len(leading_unit):
            remainder = surface[len(leading_unit) :]
            result.append(
                {
                    "surface": leading_unit,
                    "pos": FIXED_LEADING_SEARCH_UNITS[leading_unit],
                    "lemma": leading_unit,
                }
            )
            result.append({"surface": remainder, "pos": "名詞", "lemma": remainder})
            if applied_rule is None:
                applied_rule = "fixed-leading-search-unit"
            continue

        # 0a. Split a kanji nominal head from adverbial に regardless of the
        # reference dictionary's POS coverage (次に, 滅多に). A verb's
        # continuative (死に) has no particle inside it.
        if surface not in FIXED_FUNCTION_SEARCH_UNITS and t.get("pos") != "動詞":
            m = regex.match(r"^([\p{Han}]+)(に)$", surface)
            if m:
                base = m.group(1)
                result.append({"surface": base, "pos": "名詞", "lemma": base})
                result.append({"surface": "に", "pos": "助詞", "lemma": "に"})
                if applied_rule is None:
                    applied_rule = "adverb-ni-split"
                continue

        # 0ab. A noun + derivational suffix verb the reference holds as one
        # headword (春めく, 大人ぶる) keeps the host/suffix boundary like the
        # productive rest (謎/めく, 学者/ぶる).
        suffix_lemma = next(
            (lemma for lemma in DERIVED_VERB_SUFFIX_LEMMAS if (t.get("lemma") or "").endswith(lemma)),
            "",
        )
        host = (t.get("lemma") or "")[: -len(suffix_lemma)] if suffix_lemma else ""
        if (
            t.get("pos") == "動詞"
            and host
            and surface.startswith(host)
            and len(surface) > len(host)
            and not regex.fullmatch(r"\p{Hiragana}+", host)
            and t.get("lemma") not in LEXICALIZED_DERIVED_VERBS
            and is_single_token_of_pos(host, "名詞")
        ):
            result.append({"surface": host, "pos": "名詞", "lemma": host})
            result.append({**t, "surface": surface[len(host) :], "lemma": suffix_lemma, "pos_sub1": "自立"})
            if applied_rule is None:
                applied_rule = "derived-verb-suffix-split"
            continue

        # The reference tags the suffix itself as a dependent verb when it is
        # already split (謎+めい), which would read it as an auxiliary; it is
        # the derived verb's head.
        if (
            t.get("pos") == "動詞"
            and t.get("lemma") in DERIVED_VERB_SUFFIX_LEMMAS
            and t.get("pos_sub1") == "非自立"
            and result
            and result[-1].get("pos") == "名詞"
        ):
            # The host is nominal even where the reference lists the word as
            # a na-adjective stem (皮肉+めい).
            if result[-1].get("pos_sub1") == "形容動詞語幹":
                result[-1] = {**result[-1], "pos_sub1": "一般"}
            result.append({**t, "pos_sub1": "自立"})
            if applied_rule is None:
                applied_rule = "derived-verb-suffix-split"
            continue

        # The suffix ぶる after a bare noun is read as the verb ぶつ in its onbin
        # cell (学者+ぶっ+て); ぶつ needs its object marked, so this is ぶる.
        if (
            t.get("pos") == "動詞"
            and surface == "ぶっ"
            and t.get("lemma") == "ぶつ"
            and result
            and result[-1].get("pos") in ("名詞", "形容詞")
        ):
            result.append({**t, "lemma": "ぶる"})
            if applied_rule is None:
                applied_rule = "derived-verb-suffix-split"
            continue

        # 0aa. After a locative に the kana いた- is いる plus the desiderative
        # (そばにいたい, 家にいたかった), not the adjective 痛い, which takes its
        # experiencer with が.
        if (
            t.get("pos") == "形容詞"
            and t.get("lemma") == "いたい"
            and surface.startswith("いた")
            and result
            and result[-1].get("surface") == "に"
            and result[-1].get("pos") == "助詞"
        ):
            result.append({"surface": "い", "pos": "動詞", "lemma": "いる", "conj_form": "連用形"})
            result.append({"surface": surface[1:], "pos": "助動詞", "lemma": "たい"})
            if applied_rule is None:
                applied_rule = "locative-iru-desiderative"
            continue

        # 0. Plural suffix ら
        m = regex.match(r"^(彼女|彼|僕|奴|我)ら$", surface)
        if m:
            result.append({"surface": m.group(1), "pos": "名詞", "pos_sub1": "代名詞", "lemma": m.group(1)})
            result.append({"surface": "ら", "pos": "名詞", "pos_sub1": "接尾", "lemma": "ら"})
            if applied_rule is None:
                applied_rule = "ra-suffix-split"
            continue

        # 0b. たって/だって after a host that cannot take a particle directly is
        # the auxiliary plus the quotative, not one particle. The binding particle
        # attaches to a nominal that already stands on its own (子供だって分かる);
        # a 形容動詞語幹 needs a copula before anything can attach to it (無理|だ|って),
        # and a 連用タ接続 cell needs the past auxiliary (言っ|た|って, 脱い|だ|って).
        if surface in ("たって", "だって") and t.get("pos") == "助詞" and token_index > 0:
            previous = tokens[token_index - 1]
            after_stem = (
                surface == "だって" and previous.get("pos") == "名詞" and previous.get("pos_sub1") == "形容動詞語幹"
            )
            after_ta_cell = previous.get("pos") in ("動詞", "形容詞") and previous.get("conj_form") == "連用タ接続"
            if after_stem or after_ta_cell:
                result.append({"surface": surface[0], "pos": "助動詞", "lemma": surface[0]})
                result.append({"surface": "って", "pos": "助詞", "lemma": "って"})
                if applied_rule is None:
                    applied_rule = "copula-quotative-split"
                continue

        # 1. ったら topic particle
        m = regex.match(r"^(.+)(ったら)$", surface)
        if m and len(m.group(1)) >= 3:
            stem = m.group(1)
            if stem in TTARA_STEMS:
                result.append({"surface": stem, "pos": "名詞", "lemma": stem})
                result.append({"surface": "ったら", "pos": "助詞", "lemma": "ったら"})
                if applied_rule is None:
                    applied_rule = "ttara-split"
                continue

        # 2. ってば emphatic particle
        m = regex.match(r"^(.+)(ってば)$", surface)
        if m and len(m.group(1)) >= 2:
            stem = m.group(1)
            if stem in TTEBA_STEMS:
                result.append({"surface": stem, "pos": "副詞", "lemma": stem})
                result.append({"surface": "ってば", "pos": "助詞", "lemma": "ってば"})
                if applied_rule is None:
                    applied_rule = "tteba-split"
                continue

        # 3. Unnatural kanji compounds
        m = regex.match(r"^(時分)(学校)$", surface)
        if m:
            result.append({"surface": m.group(1), "pos": "名詞", "lemma": m.group(1)})
            result.append({"surface": m.group(2), "pos": "名詞", "lemma": m.group(2)})
            if applied_rule is None:
                applied_rule = "compound-split"
            continue

        # 4. ねたい adjective -> ね|たい
        if surface == "ねたい" and t.get("pos") == "形容詞":
            result.append({"surface": "ね", "pos": "動詞", "lemma": "ねる"})
            result.append({"surface": "たい", "pos": "助動詞", "lemma": "たい"})
            if applied_rule is None:
                applied_rule = "netai-split"
            continue

        # 5. Compound nouns with dictionary words at start
        m = regex.match(r"^(自然)(言語処理.+)$", surface)
        if m:
            result.append({"surface": m.group(1), "pos": "名詞", "lemma": m.group(1)})
            result.append({"surface": m.group(2), "pos": "名詞", "lemma": m.group(2)})
            if applied_rule is None:
                applied_rule = "compound-dict-split"
            continue

        # 6. Prefecture + city compounds
        m = regex.match(r"^(.+県)(.+市)$", surface)
        if m:
            result.append({"surface": m.group(1), "pos": "名詞", "lemma": m.group(1)})
            result.append({"surface": m.group(2), "pos": "名詞", "lemma": m.group(2)})
            if applied_rule is None:
                applied_rule = "prefecture-city-split"
            continue

        # 7. Kanji + Katakana compound nouns; an ordinal 目 closing a quantity
        # stays its own token after the split (三+ページ+目).
        if t.get("pos") == "名詞" and surface not in USER_DICT_COMPOUNDS:
            m = regex.match(r"^([\p{Han}]+)([\u30A0-\u30FFー]+)(目)?$", surface)
            if m:
                result.append({"surface": m.group(1), "pos": "名詞", "lemma": m.group(1)})
                result.append({"surface": m.group(2), "pos": "名詞", "lemma": m.group(2)})
                if m.group(3):
                    result.append({"surface": "目", "pos": "名詞", "pos_sub1": "接尾", "lemma": "目"})
                if applied_rule is None:
                    applied_rule = "kanji-katakana-split"
                continue

        # 8a. Kango + として adverbs: 依然として → 依然と|し|て
        # MeCab treats these as single adverbs, but they are taru-adjective
        # adverb forms (漢語 + と) + する conjugation (し + て)
        if t.get("pos") == "副詞":
            m = regex.match(r"^([\p{Han}]{2,}と)(して)$", surface)
            if m:
                adv_part = m.group(1)
                result.append({"surface": adv_part, "pos": "副詞", "lemma": adv_part[:-1]})
                result.append({"surface": "し", "pos": "動詞", "lemma": "する"})
                result.append({"surface": "て", "pos": "助詞", "lemma": "て"})
                if applied_rule is None:
                    applied_rule = "kango-toshite-split"
                continue

        # The productive quotative + する te-form keeps all grammatical
        # boundaries even when the reference dictionary emits として as one
        # particle token (考えよう+と+し+て+も).
        if (
            surface == "として"
            and token_index > 0
            and tokens[token_index - 1].get("surface") in _VOLITIONAL_AUXILIARIES
            and tokens[token_index - 1].get("pos") == "助動詞"
        ):
            result.append({"surface": "と", "pos": "助詞", "lemma": "と"})
            result.append({"surface": "し", "pos": "動詞", "lemma": "する"})
            result.append({"surface": "て", "pos": "助詞", "lemma": "て"})
            if applied_rule is None:
                applied_rule = "quotative-suru-te-split"
            continue

        # The exemplification particle でも is the copula's continuative で plus
        # the binding particle も, and the reference dictionary carries only the
        # fused spelling — では has no entry and is therefore always emitted in
        # its parts. That lexicalization decides the boundary for hosts that
        # cannot take exemplification at all: after a formal noun or the
        # nominalizer の, in front of the copula's own supporting verb, ではない
        # comes back decomposed while でもない does not (ほか+で+は+ない against
        # ほか+でも+ない). The same reference splits はず+で+も+ない, so the
        # fused reading is an artifact of the entry rather than a reading of the
        # construction. A referential host keeps the fusion (学生でもない), which
        # is why the host set stays closed here.
        if (
            surface == "でも"
            and t.get("pos") == "助詞"
            and token_index > 0
            and tokens[token_index - 1].get("surface") in COPULAR_PREDICATE_HEADS
            and token_index + 1 < len(tokens)
            and tokens[token_index + 1].get("lemma") in ("ある", "ない")
        ):
            result.append({"surface": "で", "pos": "助動詞", "lemma": "だ"})
            result.append({"surface": "も", "pos": "助詞", "pos_sub1": "係助詞", "lemma": "も"})
            if applied_rule is None:
                applied_rule = "copular-head-demo-split"
            continue

        # The quotative headword というか is one word only where an attributive
        # can stand. The reference dictionary already reads the standard
        # spelling in its parts in front of a particle (本+と+いう+か) while
        # keeping the colloquial one whole, which puts the same construction in
        # two shapes; the attributive position is what its entry is for, so it
        # keeps the headword there (本+という+物) and loses it elsewhere. The
        # head has to be a particle of its own, which is what separates the
        # quotative from the demonstrative attributives built on the same verb
        # (そういう, こういう).
        quotative_parts = _split_quotative_headword(t, tokens[token_index + 1 :])
        if quotative_parts is not None:
            result.extend(quotative_parts)
            if applied_rule is None:
                applied_rule = "quotative-headword-split"
            continue

        # 8. Copula negation: じゃない -> じゃ|ない
        if surface == "じゃない" and t.get("pos") == "助動詞":
            result.append({"surface": "じゃ", "pos": "助動詞", "lemma": "だ"})
            result.append({"surface": "ない", "pos": "助動詞", "lemma": "ない"})
            if applied_rule is None:
                applied_rule = "copula-negation-split"
            continue

        # Productive na-adjective nominalization.  The reference dictionary
        # may lexicalize the result as a noun, but the suffix boundary remains
        # visible for arbitrary stems with the characteristic -か ending.
        if t.get("pos") == "名詞" and surface.endswith("かさ") and len(surface) > len("かさ"):
            stem = surface[:-1]
            result.append({"surface": stem, "pos": "形容詞", "lemma": stem})
            result.append({"surface": "さ", "pos": "接尾辞", "pos_sub1": "接尾", "lemma": "さ"})
            if applied_rule is None:
                applied_rule = "na-adjective-sa-suffix"
            continue

        # 9. Onomatopoeia + っと + する conjugation (single MeCab token)
        # MeCab may treat ぷるんっとした as one token; Suzume splits as ぷるんっと+し+た
        m = regex.match(
            r"^([\p{Hiragana}\p{Katakana}ー]{1,6}っと)(し|した|して|する|すれ|しろ|せ|さ|される|された|させ|させる)$",
            surface,
        )
        if m:
            adv_part = m.group(1)
            verb_part = m.group(2)
            result.append({"surface": adv_part, "pos": "副詞", "lemma": adv_part})
            # Split する conjugation further: した→し+た, して→し+て, etc.
            if verb_part in ("した", "して", "しろ"):
                result.append({"surface": verb_part[:1], "pos": "動詞", "lemma": "する"})
                result.append(
                    {
                        "surface": verb_part[1:],
                        "pos": "助動詞" if verb_part[1:] == "た" else "助詞",
                        "lemma": verb_part[1:],
                    }
                )
            elif verb_part in ("される", "された", "させ", "させる"):
                result.append({"surface": "さ", "pos": "動詞", "lemma": "する"})
                rest = verb_part[1:]
                result.append(
                    {"surface": rest, "pos": "動詞", "lemma": rest + ("る" if not rest.endswith("る") else "")}
                )
            else:
                result.append({"surface": verb_part, "pos": "動詞", "lemma": "する"})
            if applied_rule is None:
                applied_rule = "onomatopoeia-tto-suru-split"
            continue

        # 10b. A dictionary headword spelling a kanji noun plus 行く (道行く) is a
        # noun with its case particle dropped and the verb 行く; both keep their
        # meaning apart (道を行く), so they stay two search units.
        lemma = t.get("lemma") or ""
        noun_head = lemma[: -len("行く")] if t.get("pos") == "動詞" and lemma.endswith("行く") else ""
        if (
            regex.fullmatch(r"\p{Han}+", noun_head)
            and surface.startswith(noun_head)
            and len(surface) > len(noun_head)
            and _reads_as_one_noun(noun_head)
        ):
            result.append({"surface": noun_head, "pos": "名詞", "lemma": noun_head})
            result.append({**t, "surface": surface[len(noun_head) :], "lemma": "行く"})
            if applied_rule is None:
                applied_rule = "noun-iku-headword-split"
            continue

        # 10a. An adjective Xげない (危なげない, 危なげなく) is the noun Xげ plus the
        # supplementary ない when Xげ stands as a noun of its own (危なげ+が+ない).
        # One the core lexicon lists whole has a bound stem (さりげない).
        ge_nai = _GE_NAI_ADJECTIVE.fullmatch(surface) if t.get("pos") == "形容詞" else None
        if (
            ge_nai is not None
            and _reads_as_one_noun(ge_nai.group(1))
            and ge_nai.group(1) + "ない" not in core_headwords("adjectives.tsv")
        ):
            result.append({"surface": ge_nai.group(1), "pos": "名詞", "lemma": ge_nai.group(1)})
            result.append({"surface": ge_nai.group(2), "pos": "形容詞", "lemma": "ない"})
            if applied_rule is None:
                applied_rule = "noun-nai-compound-split"
            continue

        # 10. Nominal + ない lexical adjective split. A Godan negative would
        # require the a-row mizenkei, so the i-row surface is a deverbal noun.
        if t.get("pos") == "形容詞" and surface in NOUN_NAI_COMPOUND_ADJECTIVES:
            noun_part = surface[: -len("ない")]
            result.append({"surface": noun_part, "pos": "名詞", "lemma": noun_part})
            result.append({"surface": "ない", "pos": "形容詞", "lemma": "ない"})
            if applied_rule is None:
                applied_rule = "noun-nai-compound-split"
            continue

        # 11. Literary volitional ん: verb+ん → verb + ん
        # MeCab merges ichidan verb + ん (literary volitional =む/よう)
        # as single token with conj_form 体言接続特殊
        # e.g., 乗り越えん → 乗り越え + ん, 越えん → 越え + ん
        # The same form is the colloquial る→ん contraction when a nominalizer
        # or final particle follows (帰んなよ, わかんのよ): it stays one verb.
        # Before the explanatory copula the ん is the nominalizer の with the
        # る dropped (どうすんだろう), as rule 11a reads すん+だ.
        following = tokens[token_index + 1] if token_index + 1 < len(tokens) else {}
        following_surface = following.get("surface", "")
        is_contracted_form = t.get("pos") == "動詞" and len(surface) >= 2 and t.get("conj_form") == "体言接続特殊"
        if is_contracted_form and surface.endswith("ん") and following_surface.startswith(("な", "の", "ね", "よ")):
            result.append(t)
            continue
        if (
            is_contracted_form
            and surface.endswith("ん")
            and following.get("pos") == "助動詞"
            and following_surface.startswith(("だ", "です", "でしょ"))
        ):
            result.append({"surface": surface[:-1], "pos": "動詞", "lemma": t.get("lemma", "")})
            result.append({"surface": "ん", "pos": "助詞", "pos_sub1": "準体助詞", "lemma": "の"})
            if applied_rule is None:
                applied_rule = "contracted-explanatory-n-split"
            continue
        if (
            t.get("pos") == "動詞"
            and surface.endswith("ん")
            and len(surface) >= 2
            and t.get("conj_form") == "体言接続特殊"
        ):
            verb_part = surface[: -len("ん")]
            lemma = t.get("lemma", "")
            result.append({"surface": verb_part, "pos": "動詞", "lemma": lemma})
            result.append({"surface": "ん", "pos": "助動詞", "lemma": "ん"})
            if applied_rule is None:
                applied_rule = "literary-volitional-n-split"
            continue

        # 11b. Literary perfective り after a サ変 noun (静止せる物体): the
        # reference reads せる as the causative suffix, but the causative takes
        # させる after a verbal noun, and an attributive せる before a noun is
        # the サ変 stem せ plus the attributive る of り.
        if (
            t.get("pos") == "動詞"
            and t.get("pos_sub1") == "接尾"
            and surface == "せる"
            and result
            and result[-1].get("pos") == "名詞"
            and result[-1].get("pos_sub1") == "サ変接続"
            and token_index + 1 < len(tokens)
            and tokens[token_index + 1].get("pos") == "名詞"
        ):
            result.append({"surface": "せ", "pos": "動詞", "lemma": "する"})
            result.append({"surface": "る", "pos": "助動詞", "lemma": "り"})
            if applied_rule is None:
                applied_rule = "suru-noun-perfective-ri"
            continue

        # 11a. The same ん cell hides behind an onbin reading when the copula
        # follows. Before a nominal the dictionary reads すん as the contracted
        # サ変 stem plus ん (そうすんのか), but before だ it prefers the ま-row
        # onbin continuative of an unrelated verb, so one surface gets two
        # analyses from its follower alone. Probing the same surface with a
        # nominal after it asks the dictionary whether the ん cell exists at
        # all: a genuine onbin past (読ん, 頼ん, のん, たのん) has no such cell
        # and keeps its reading.
        if (
            t.get("pos") == "動詞"
            and surface.endswith("ん")
            and len(surface) >= 2
            and t.get("conj_form") == "連用タ接続"
            and token_index + 1 < len(tokens)
            and tokens[token_index + 1].get("pos") == "助動詞"
            # The copula may already carry a fused sentence-final tail (だべ).
            and tokens[token_index + 1].get("surface", "").startswith(("だ", "です"))
        ):
            probe = mecab_analyze(surface + "の")
            if (
                len(probe) >= 2
                and probe[0].get("surface") == surface
                and probe[0].get("pos") == "動詞"
                and probe[0].get("conj_form") == "体言接続特殊"
            ):
                result.append({"surface": surface[:-1], "pos": "動詞", "lemma": probe[0].get("lemma", surface[:-1])})
                result.append({"surface": "ん", "pos": "助詞", "pos_sub1": "準体助詞", "lemma": "の"})
                if applied_rule is None:
                    applied_rule = "contracted-explanatory-n-split"
                continue

        # 12. Literary volitional auxiliary + quotative particle.  Keep the
        # two closed-class grammatical units searchable even when MeCab emits
        # a fused noun token (見むと → 見 + む + と).
        compound = LITERARY_VOLITIONAL_PARTICLE_COMPOUNDS.get(surface)
        if compound is not None:
            auxiliary, particle = compound
            applied_rule = _emit_split(
                result,
                (
                    {"surface": auxiliary, "pos": "助動詞", "lemma": auxiliary},
                    {"surface": particle, "pos": "助詞", "lemma": particle},
                ),
                applied_rule,
                "literary-volitional-particle-split",
            )
            continue

        # 13. An excessive auxiliary remains a separate search unit. MeCab can
        # lexicalize a kanji V1 plus 過ぎる into one verb token in any cell
        # (行き過ぎ, 通り過ぎる, 通り過ぎれ), while Suzume consistently exposes the
        # productive V1 + 過ぎる boundary.
        excessive_at = surface.rfind("過ぎ")
        if t.get("pos") == "動詞" and excessive_at > 0 and t.get("lemma", "").endswith("過ぎる"):
            verb_part = surface[:excessive_at]
            verb_lemma = base_from_renyokei(verb_part)
            if verb_lemma is not None:
                result.append({"surface": verb_part, "pos": "動詞", "lemma": verb_lemma})
                result.append({"surface": surface[excessive_at:], "pos": "動詞", "lemma": "過ぎる"})
                if applied_rule is None:
                    applied_rule = "excessive-auxiliary-split"
                continue

        # 14. The failure subsidiary 損なう remains searchable after its V1.
        # MeCab may lexicalize the whole compound, including the bare one-kanji
        # ichidan stem used before a kanji-written subsidiary.
        if t.get("pos") == "動詞" and surface.endswith("損なう"):
            verb_part = surface[: -len("損なう")]
            verb_lemma = base_from_renyokei(verb_part)
            if verb_lemma is None and regex.fullmatch(r"\p{Han}", verb_part):
                verb_lemma = verb_part + "る"
            if verb_lemma is not None:
                result.append({"surface": verb_part, "pos": "動詞", "lemma": verb_lemma})
                result.append({"surface": "損なう", "pos": "動詞", "lemma": "損なう"})
                if applied_rule is None:
                    applied_rule = "failure-subsidiary-split"
                continue

        if t.get("pos") == "動詞" and surface.endswith("そびれる"):
            verb_part = surface[: -len("そびれる")]
            verb_lemma = base_from_renyokei(verb_part)
            if verb_lemma is None and regex.fullmatch(r"\p{Han}", verb_part):
                verb_lemma = verb_part + "る"
            if verb_lemma is not None:
                result.append({"surface": verb_part, "pos": "動詞", "lemma": verb_lemma})
                result.append({"surface": "そびれる", "pos": "助動詞", "lemma": "そびれる"})
                if applied_rule is None:
                    applied_rule = "failure-subsidiary-split"
                continue

        if t.get("pos") == "動詞" and surface.endswith("かねる"):
            verb_part = surface[: -len("かねる")]
            verb_lemma = base_from_renyokei(verb_part)
            if verb_lemma is not None:
                result.append({"surface": verb_part, "pos": "動詞", "lemma": verb_lemma})
                result.append({"surface": "かねる", "pos": "助動詞", "lemma": "かねる"})
                if applied_rule is None:
                    applied_rule = "inability-subsidiary-split"
                continue

        # No split needed
        result.append(t)

    return result, applied_rule
