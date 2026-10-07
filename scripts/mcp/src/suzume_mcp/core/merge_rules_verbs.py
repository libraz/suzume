"""Merge steps for compound verbs, lexicalized words, and te-form units."""

import regex

from .constants import (
    COMPOUND_VERB_V2_GODAN,
    COMPOUND_VERB_V2_ICHIDAN,
    COMPOUND_VERB_V2_NOT_AFTER_SURU,
    COMPOUND_VERB_V2_SURU_ONLY,
    HIRAGANA_COMPOUNDS,
)
from .core_lexicon import kana_ichidan_verbs
from .mecab import mecab_analyze
from .merge_rules_helpers import _FIXED_INFLECTED_FUNCTION_UNITS
from .merge_rules_state import MergeState
from .split_rules import _is_single_i_adjective, base_from_renyokei, bases_from_renyokei


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


_PRODUCTIVE_COMPOUND_V2 = frozenset(COMPOUND_VERB_V2_GODAN + COMPOUND_VERB_V2_ICHIDAN)


_NOMINALIZING_PARTICLES = frozenset({"を", "は", "が", "の", "に", "で", "へ", "と", "も"})


def _merge_compound_verbs(state: MergeState) -> bool:
    """Compound verbs and nominalized V1+V2 continuatives."""
    tokens = state.tokens
    result = state.result
    i = state.i
    t = state.t
    merged = state.merged
    applied_rule = state.applied_rule
    remaining = state.remaining
    try:
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
    finally:
        state.i = i
        state.merged = merged
        state.applied_rule = applied_rule
    return False


def _merge_lexicalized_words(state: MergeState) -> bool:
    """Lexicalized こもる compounds, hiragana words, and だの coordination."""
    tokens = state.tokens
    result = state.result
    i = state.i
    t = state.t
    merged = state.merged
    applied_rule = state.applied_rule
    remaining = state.remaining
    try:
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

        # 10b. Kana ichidan verbs of the core lexicon. The reference dictionary
        # lacks them and spells their stem as other morphemes (もた+れる as the
        # passive of もつ, も+たれ as a particle and 垂れる), so a run of two or
        # more tokens that spells exactly the stem, or the stem plus its terminal
        # る, is the listed verb. A particle right after a predicate is a real
        # conjunctive particle (読ん+で+き), so the run cannot start on it.
        after_predicate_particle = (
            t.get("pos") == "助詞" and bool(result) and result[-1].get("pos") in ("動詞", "形容詞", "助動詞")
        )
        if (
            not merged
            and not after_predicate_particle
            and t.get("surface", "")[:1]
            and "ぁ" <= t.get("surface", "")[0] <= "ゖ"
        ):
            for lemma in kana_ichidan_verbs():
                stem = lemma[:-1]
                if not remaining.startswith(stem):
                    continue
                consumed = ""
                j = i
                while j < len(tokens) and len(consumed) < len(stem):
                    consumed += tokens[j].get("surface", "")
                    j += 1
                if j - i < 2 or consumed not in (stem, lemma):
                    continue
                result.append(
                    {
                        "surface": consumed,
                        "pos": "動詞",
                        "pos_sub1": "自立",
                        "conj_type": "一段",
                        "conj_form": "基本形" if consumed == lemma else "連用形",
                        "lemma": lemma,
                    }
                )
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "kana-ichidan-lexicon"
                break

        # 10c. A kanji-led verb cell spelled twice in a row is one reduplicated
        # adverb (代わる代わる, 見る見る); the reference lists only some of them
        # as headwords and splits the rest into two predicates.
        if (
            not merged
            and t.get("pos") == "動詞"
            and i + 1 < len(tokens)
            and tokens[i + 1].get("surface") == t.get("surface")
            and tokens[i + 1].get("pos") == "動詞"
            and regex.match(r"^\p{Han}+\p{Hiragana}+$", t.get("surface", ""))
        ):
            doubled = t["surface"] * 2
            result.append({"surface": doubled, "pos": "副詞", "lemma": doubled})
            i += 2
            merged = True
            if applied_rule is None:
                applied_rule = "reduplicated-verb-adverb"

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
    finally:
        state.i = i
        state.merged = merged
        state.applied_rule = applied_rule
    return False


def _merge_te_form_units(state: MergeState) -> bool:
    """The たて suffix and the て + ある boundary."""
    tokens = state.tokens
    result = state.result
    i = state.i
    t = state.t
    merged = state.merged
    applied_rule = state.applied_rule
    try:
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
    finally:
        state.i = i
        state.merged = merged
        state.applied_rule = applied_rule
    return False
