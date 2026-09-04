"""Split rules ported from SuzumeUtils.pm apply_suzume_split()."""

import regex

from .constants import (
    COMPOUND_VERB_V2_ICHIDAN,
    COPULAR_PREDICATE_HEADS,
    FIXED_FUNCTION_SEARCH_UNITS,
    FIXED_LEADING_SEARCH_UNITS,
    LEXICALIZED_CAUSATIVE_SU_LEMMAS,
    LITERARY_VOLITIONAL_PARTICLE_COMPOUNDS,
    NOUN_NAI_COMPOUND_ADJECTIVES,
    STATE_NOUN_SUFFIXES,
    TTARA_STEMS,
    TTEBA_STEMS,
    USER_DICT_COMPOUNDS,
)
from .core_lexicon import core_headwords
from .mecab import is_single_token_of_pos, mecab_analyze

# A plain 名詞-一般 host for re-reading a copula span. It carries no reading
# that could fuse with what follows, so whatever the probe returns after it is
# the copula frame alone.
_COPULA_HOST_PROBE = "学生"

_GODAN_RENYOKEI_TO_BASE: dict[str, str] = {
    "い": "う",
    "き": "く",
    "ぎ": "ぐ",
    "し": "す",
    "ち": "つ",
    "に": "ぬ",
    "び": "ぶ",
    "み": "む",
    "り": "る",
}
_GODAN_MIZENKEI_TO_BASE: dict[str, str] = {
    "わ": "う",
    "か": "く",
    "が": "ぐ",
    "さ": "す",
    "た": "つ",
    "な": "ぬ",
    "ば": "ぶ",
    "ま": "む",
    "ら": "る",
}
_ICHIDAN_RENYOKEI_ENDINGS = frozenset("えけげせぜてでねへべめれ")

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

# A predicate closed by a volitional auxiliary cannot host a case particle, so
# a として that follows one is the quotative と plus the te-form of する -- in the
# modern spelling and in the classical む alike.
_VOLITIONAL_AUXILIARIES = frozenset({"う", "よう", "まい", "む", "ん"})


def _emit_split(
    result: list[dict],
    split_tokens: tuple[dict, ...],
    applied_rule: str | None,
    rule: str,
) -> str:
    """Emit a complete split and retain the first rule name for reporting."""
    result.extend(split_tokens)
    return applied_rule or rule


def base_from_renyokei(stem: str) -> str | None:
    """Reconstruct a dictionary form from a productive renyokei surface."""
    if not stem:
        return None
    ending = stem[-1]
    if ending in _GODAN_RENYOKEI_TO_BASE:
        return stem[:-1] + _GODAN_RENYOKEI_TO_BASE[ending]
    if ending in _ICHIDAN_RENYOKEI_ENDINGS:
        return stem + "る"
    return None


def bases_from_renyokei(stem: str) -> tuple[str, ...]:
    """Every dictionary form a renyokei surface can reconstruct to.

    An i-row ending belongs to both conjugation classes (落ち is 落つ or 落ちる,
    起き is 起く or 起きる), so a caller that resolves the reading against a closed
    verb class needs both readings rather than the Godan one alone.
    """
    godan = base_from_renyokei(stem)
    if godan is None:
        return ()
    if stem[-1] in _GODAN_RENYOKEI_TO_BASE:
        return (godan, stem + "る")
    return (godan,)


def base_from_mizenkei(stem: str) -> str | None:
    """Reconstruct a Godan dictionary form from an a-row irrealis stem."""
    if not stem:
        return None
    ending = _GODAN_MIZENKEI_TO_BASE.get(stem[-1])
    return stem[:-1] + ending if ending is not None else None


def _reanalyze_exact(text: str) -> list[dict] | None:
    """Analyze one internal span only when the analyzer preserves it exactly."""
    if not text:
        return None
    tokens = mecab_analyze(text)
    if "".join(token.get("surface", "") for token in tokens) != text:
        return None
    return tokens


def _is_single_i_adjective(text: str) -> bool:
    """Whether a reconstructed dictionary form is one i-adjective."""
    tokens = _reanalyze_exact(text)
    return bool(tokens and len(tokens) == 1 and tokens[0].get("pos") == "形容詞")


def _kanji_noun_token(surface: str) -> dict | None:
    """Read `surface` and return it when it is exactly one kanji-bearing noun."""
    if not surface or not regex.search(r"\p{Han}", surface):
        return None
    tokens = _reanalyze_exact(surface)
    if tokens is None or len(tokens) != 1 or tokens[0].get("pos") != "名詞":
        return None
    return tokens[0]


def _as_independent_token(token: dict) -> dict:
    """Drop context-bound subcategories when a span is an independent morpheme."""
    return {
        "surface": token.get("surface", ""),
        "pos": token.get("pos", ""),
        "lemma": token.get("lemma") or token.get("surface", ""),
    }


# A clause hidden inside one headword needs at least a two-kana dictionary-form
# verb, a particle and a two-kana closing predicate.
_MIN_CLAUSE_HEADWORD_LENGTH = 5
# Every verb dictionary form ends on the u row.
_DICTIONARY_FORM_ENDINGS: frozenset[str] = frozenset("うくぐすつぬぶむる")


def _split_lexicalized_morpheme_boundaries(token: dict) -> list[dict] | None:
    """Restore productive particle and inflection boundaries hidden by a headword."""
    surface = token.get("surface", "")
    pos = token.get("pos", "")
    lemma = token.get("lemma", surface)

    genitive = regex.fullmatch(r"(.+)(の)(.+)", surface)
    if genitive is not None:
        host_surface = genitive.group(1)
        host_tokens = _reanalyze_exact(host_surface)
        head_tokens = _reanalyze_exact(genitive.group(3))
        formal_noun_genitive = host_surface in COPULAR_PREDICATE_HEADS and len(genitive.group(3)) >= 2
        kanji_genitive = (
            regex.fullmatch(r"\p{Han}+", host_surface) is not None
            and regex.fullmatch(r"\p{Han}+", genitive.group(3)) is not None
        )
        host_is_noun = formal_noun_genitive or (
            kanji_genitive and host_tokens is not None and len(host_tokens) == 1 and host_tokens[0].get("pos") == "名詞"
        )
        head_is_noun = formal_noun_genitive or (
            kanji_genitive and head_tokens is not None and len(head_tokens) == 1 and head_tokens[0].get("pos") == "名詞"
        )
        if host_is_noun and head_is_noun:
            host_token = (
                _as_independent_token(host_tokens[0])
                if host_tokens is not None and len(host_tokens) == 1
                else {"surface": host_surface, "pos": "名詞", "lemma": host_surface}
            )
            return [
                host_token,
                {"surface": "の", "pos": "助詞", "lemma": "の"},
                (
                    _as_independent_token(head_tokens[0])
                    if head_tokens is not None and len(head_tokens) == 1
                    else {"surface": genitive.group(3), "pos": "名詞", "lemma": genitive.group(3)}
                ),
            ]

    if pos in ("名詞", "副詞") and surface.endswith("ず"):
        isolated = _reanalyze_exact(surface)
        if (
            isolated is not None
            and len(isolated) >= 2
            # The negative ず takes the irrealis of an adjective as readily as a
            # verb's (悪しから+ず, 少なから+ず), and a headword covering either is
            # hiding the same boundary.
            and isolated[0].get("pos") in ("動詞", "形容詞")
            and isolated[-1].get("surface") == "ず"
            and isolated[-1].get("pos") == "助動詞"
        ):
            return isolated
        # A fixed phrase can also arrive as one adverb the analyzer never
        # decomposes (悪しからず). The supplementary conjugation still shows
        # through: its 未然形 cell resolves to a modern headword, and the ず
        # behind it is the same negative auxiliary as above.
        from .merge_postprocessors import classical_adjective_lemma

        irrealis_lemma = classical_adjective_lemma(surface[:-1])
        if irrealis_lemma is not None:
            return [
                {"surface": surface[:-1], "pos": "形容詞", "lemma": irrealis_lemma},
                {"surface": "ず", "pos": "助動詞", "lemma": "ぬ"},
            ]

    # A headword may also cover a whole clause: a finite predicate, a particle
    # chain, and a closing predicate (言う+まで+も+ない). Nothing about that
    # sequence is lexicalized — each piece inflects and combines productively —
    # so the boundaries stay, the same way they do for the productive
    # particle-plus-predicate chains below.
    if pos in ("動詞", "形容詞", "副詞", "名詞") and len(surface) >= _MIN_CLAUSE_HEADWORD_LENGTH:
        for split in range(2, len(surface) - 1):
            # A dictionary-form verb closes on the u row, so only those split
            # points can end the clause's finite predicate. Checking the kana
            # first keeps this off the analyzer for every other position.
            if surface[split - 1] not in _DICTIONARY_FORM_ENDINGS:
                continue
            head_tokens = _reanalyze_exact(surface[:split])
            if (
                head_tokens is None
                or len(head_tokens) != 1
                or head_tokens[0].get("pos") != "動詞"
                or head_tokens[0].get("conj_form") != "基本形"
            ):
                continue
            tail_tokens = _reanalyze_exact(surface[split:])
            if (
                tail_tokens is None
                or len(tail_tokens) < 2
                or tail_tokens[0].get("pos") != "助詞"
                or any(part.get("pos") not in ("助詞", "助動詞", "形容詞") for part in tail_tokens)
            ):
                continue
            return [_as_independent_token(head_tokens[0]), *(_as_independent_token(part) for part in tail_tokens)]

    # A 連語 headword that swallows a case particle is not one word: に従う is に + 従う
    # and 上と下 is 上 + と + 下. The reference dictionary lists them as a single 助詞 or
    # 名詞 only in some positions, and reading the headword on its own already produces
    # the boundaries. Closed-class compounds (けれども) stay merged because their
    # decomposition carries no independent word.
    if pos == "助詞" and surface == lemma and lemma not in FIXED_FUNCTION_SEARCH_UNITS:
        # A particle headword that is a case particle plus a plain-form verb is not a
        # function word: に従う is に + 従う. Reading the headword on its own already
        # produces the boundary. The lexicalized て-form compounds (に従って, を通じて)
        # keep their own surface, so surface != lemma leaves them untouched.
        isolated = _reanalyze_exact(lemma) or []
        if (
            len(isolated) == 2
            and isolated[0].get("pos") == "助詞"
            and isolated[1].get("pos") == "動詞"
            and isolated[1].get("conj_form") == "基本形"
        ):
            return [_as_independent_token(part) for part in isolated]

    if pos == "名詞" and surface == lemma:
        # A headword the reference dictionary holds whole (上と下) never splits on
        # re-analysis, so try the boundary directly: a bare case particle between two
        # kanji-bearing nouns is a phrase, not a word. Requiring kanji on both sides
        # keeps ordinary kana nouns (まとめ, ひとで) out.
        for match in regex.finditer(r"[とにをへがで]", lemma):
            host, particle, tail = lemma[: match.start()], match.group(0), lemma[match.end() :]
            host_noun = _kanji_noun_token(host)
            tail_noun = _kanji_noun_token(tail)
            if host_noun is None or tail_noun is None:
                continue
            # A case particle is free only when both sides are independent
            # words. The reference dictionary holds 我が as an adnominal of its
            # own and reads the identical frame that way wherever it has no
            # whole-phrase headword to prefer (我が子, 我が身の上), so in 我が身
            # and 我が国 the particle belongs to that determiner rather than to
            # a phrase, and the boundary falls after it.
            adnominal = _reanalyze_exact(host + particle)
            if adnominal is not None and len(adnominal) == 1 and adnominal[0].get("pos") == "連体詞":
                return [_as_independent_token(adnominal[0]), _as_independent_token(tail_noun)]
            # Built directly rather than re-read: a lone case particle comes back
            # from the analyzer as a filler.
            return [
                _as_independent_token(host_noun),
                {"surface": particle, "pos": "助詞", "lemma": particle},
                _as_independent_token(tail_noun),
            ]

    if pos not in ("動詞", "形容詞", "副詞"):
        return None
    for match in regex.finditer(r"[にを]", lemma):
        particle = match.group(0)
        host = lemma[: match.start()]
        predicate_lemma = lemma[match.end() :]
        boundary = host + particle
        if not host or not predicate_lemma or not surface.startswith(boundary):
            continue

        host_tokens = _reanalyze_exact(host)
        if host_tokens is None or len(host_tokens) != 1 or host_tokens[0].get("pos") not in ("名詞", "動詞"):
            continue

        predicate_surface = surface[len(boundary) :]
        if pos == "動詞":
            predicate_tokens = _reanalyze_exact(predicate_lemma)
            if (
                predicate_surface
                and predicate_tokens is not None
                and len(predicate_tokens) == 1
                and predicate_tokens[0].get("pos") == "動詞"
            ):
                predicate = dict(token)
                predicate["surface"] = predicate_surface
                predicate["lemma"] = predicate_lemma
                return [
                    _as_independent_token(host_tokens[0]),
                    {"surface": particle, "pos": "助詞", "lemma": particle},
                    predicate,
                ]
            continue

        predicate_tokens = _reanalyze_exact(predicate_surface)
        if (
            predicate_tokens is not None
            and len(predicate_tokens) >= 2
            and predicate_tokens[0].get("pos") == "動詞"
            and all(part.get("pos") in ("助動詞", "助詞") for part in predicate_tokens[1:])
        ):
            return [
                _as_independent_token(host_tokens[0]),
                {"surface": particle, "pos": "助詞", "lemma": particle},
                *predicate_tokens,
            ]
    return None


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

        # ます is an auxiliary, so it is never part of a particle. A compound
        # particle lexicalized together with its polite form (に関しまして,
        # に際しまして) hides the auxiliary boundary that the plain form
        # (に関し, に際し) keeps, which makes the same closed unit tokenize two
        # different ways depending only on politeness.
        #
        # Stripping the politeness does not by itself make the head a particle.
        # A case particle followed by an autonomous continuative (をもち, にたいし,
        # をつうじ) is a productive chain, and the morpheme boundaries inside it
        # stay — only a head the reference dictionary reads as one particle is a
        # closed unit. The plain polite form is the transparent spelling of the
        # same chain, so re-reading the head with ます restored recovers the
        # continuative that the lexicalized entry hid.
        if t.get("pos") == "助詞" and surface.endswith("まして") and len(surface) > 3:
            head = surface[:-3]
            head_tokens = [{"surface": head, "pos": "助詞", "lemma": head}]
            if not is_single_token_of_pos(head, "助詞"):
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
        following_passive = tokens[token_index + 1] if token_index + 1 < len(tokens) else None
        precedes_passive = following_passive is not None and following_passive.get("lemma") in ("れる", "られる")
        causative_su_lemma = t.get("lemma") or ""
        productive_causative_passive = (
            precedes_passive
            and causative_su_lemma.endswith("す")
            and causative_su_lemma not in core_headwords("verbs.tsv")
        )
        if t.get("pos") == "動詞" and (
            causative_su_lemma in LEXICALIZED_CAUSATIVE_SU_LEMMAS or productive_causative_passive
        ):
            causative_tail = next((form for form in ("さ", "し", "す", "せ") if surface.endswith(form)), "")
            causative_stem = surface[: -len(causative_tail)] if causative_tail else ""
            causative_base = base_from_mizenkei(causative_stem)
            base_tokens = _reanalyze_exact(causative_base) if causative_base is not None else None
            if base_tokens is not None and len(base_tokens) == 1 and base_tokens[0].get("pos") == "動詞":
                result.append({"surface": causative_stem, "pos": "動詞", "lemma": causative_base})
                result.append({"surface": causative_tail, "pos": "助動詞", "lemma": "す"})
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

        # In the closed ずに+は frame, a reference adjective ending in ない is
        # the productive verb mizenkei + negative auxiliary chain.  Derive the
        # host from its inflection instead of naming the open-class verb.
        if (
            t.get("pos") == "形容詞"
            and surface.endswith("ない")
            and token_index >= 2
            and tokens[token_index - 1].get("surface") == "は"
            and tokens[token_index - 2].get("surface") == "ずに"
        ):
            stem = surface[: -len("ない")]
            lemma = base_from_mizenkei(stem)
            if lemma is not None:
                result.append({"surface": stem, "pos": "動詞", "lemma": lemma})
                result.append({"surface": "ない", "pos": "助動詞", "lemma": "ない"})
                if applied_rule is None:
                    applied_rule = "zu-ni-wa-negative-auxiliary"
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
        # reference dictionary's POS coverage (次に, 滅多に).
        if surface not in FIXED_FUNCTION_SEARCH_UNITS:
            m = regex.match(r"^([\p{Han}]+)(に)$", surface)
            if m:
                base = m.group(1)
                result.append({"surface": base, "pos": "名詞", "lemma": base})
                result.append({"surface": "に", "pos": "助詞", "lemma": "に"})
                if applied_rule is None:
                    applied_rule = "adverb-ni-split"
                continue

        # 0. Plural suffix ら
        m = regex.match(r"^(彼女|彼|僕|奴|我)ら$", surface)
        if m:
            result.append({"surface": m.group(1), "pos": "名詞", "pos_sub1": "代名詞", "lemma": m.group(1)})
            result.append({"surface": "ら", "pos": "名詞", "pos_sub1": "接尾", "lemma": "ら"})
            if applied_rule is None:
                applied_rule = "ra-suffix-split"
            continue

        # 0b. だって after a na-adjective stem is the copula plus the quotative,
        # not the binding particle. The binding particle attaches to a nominal
        # that already stands on its own (子供だって分かる); a 形容動詞語幹 needs a
        # copula before anything can attach to it at all, so reading the whole
        # thing as one particle deletes the predicate's assertion (無理|だ|って).
        if surface == "だって" and t.get("pos") == "助詞" and token_index > 0:
            previous = tokens[token_index - 1]
            if previous.get("pos") == "名詞" and previous.get("pos_sub1") == "形容動詞語幹":
                result.append({"surface": "だ", "pos": "助動詞", "lemma": "だ"})
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

        # 7. Kanji + Katakana compound nouns
        if t.get("pos") == "名詞" and surface not in USER_DICT_COMPOUNDS:
            m = regex.match(r"^([\p{Han}]+)([\u30A0-\u30FFー]+)$", surface)
            if m:
                result.append({"surface": m.group(1), "pos": "名詞", "lemma": m.group(1)})
                result.append({"surface": m.group(2), "pos": "名詞", "lemma": m.group(2)})
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
            and tokens[token_index + 1].get("surface") in ("だ", "です")
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
        # lexicalize a kanji V1 plus 過ぎ into one verb token (行き過ぎ), while
        # Suzume consistently exposes the productive V1 + 過ぎ boundary.
        if t.get("pos") == "動詞" and surface.endswith("過ぎ") and t.get("lemma", "").endswith("過ぎる"):
            verb_part = surface[: -len("過ぎ")]
            verb_lemma = base_from_renyokei(verb_part)
            if verb_lemma is not None:
                result.append({"surface": verb_part, "pos": "動詞", "lemma": verb_lemma})
                result.append({"surface": "過ぎ", "pos": "助動詞", "lemma": "過ぎる"})
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
