"""Merge steps for adjective forms, vowel repetition, and emphatic or prolonged lengthening."""

import regex

from .constants import CHARACTER_SPEECH_COPULAS, DERIVED_ADJECTIVE_SUFFIX_LEMMAS, NAI_ADJECTIVES
from .core_lexicon import kana_i_adjective_lemmas
from .mecab import mecab_analyze
from .merge_postprocessors import reads_as_continuative
from .merge_rules_helpers import _UTTERANCE_FINAL_PARTICLE_HEADS, _consume_span, _covered_length
from .merge_rules_state import MergeState

_CHARACTER_SPEECH_COPULAS = tuple(sorted(CHARACTER_SPEECH_COPULAS, key=len, reverse=True))


_COPULA_HOST_POS = frozenset({"名詞", "副詞", "動詞", "形容詞", "助動詞"})


def _closes_utterance(following: str) -> bool:
    """Whether nothing but punctuation, the text end or a final particle follows."""
    return (
        not following
        or regex.match(r"[\p{P}\p{S}\p{Z}]", following) is not None
        or following[0] in _UTTERANCE_FINAL_PARTICLE_HEADS
    )


# Cells of an i-adjective after its stem, longest first so かっ wins over か.
_I_ADJECTIVE_CELL_ENDINGS = ("かっ", "けれ", "かろ", "く", "い", "き")


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


def _merge_adjective_forms(state: MergeState) -> bool:
    """Nai-adjectives, elongated adjectives, and predicate closers."""
    tokens = state.tokens
    result = state.result
    i = state.i
    t = state.t
    merged = state.merged
    applied_rule = state.applied_rule
    remaining = state.remaining
    try:
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
    finally:
        state.i = i
        state.merged = merged
        state.applied_rule = applied_rule
    return False


def _merge_vowel_repetition_and_contractions(state: MergeState) -> bool:
    """Vowel repetition, emphatic sokuon, and しまう contraction."""
    tokens = state.tokens
    result = state.result
    i = state.i
    t = state.t
    merged = state.merged
    applied_rule = state.applied_rule
    remaining = state.remaining
    try:
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
    finally:
        state.i = i
        state.merged = merged
        state.applied_rule = applied_rule
    return False


def _merge_emphatic_lengthening(state: MergeState) -> bool:
    """Adjective suffixes and emphatic or prolonged lengthening."""
    tokens = state.tokens
    text = state.text
    result = state.result
    i = state.i
    t = state.t
    merged = state.merged
    applied_rule = state.applied_rule
    remaining = state.remaining
    pos_in_text = state.pos_in_text
    try:
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
                            # A mark inside a two-mora X+ん is the mimetic (しーん,
                            # ずーん): an unlexicalized word that is its own lemma,
                            # not the noun it homographs (芯).
                            mimetic = len(plain) == 2 and plain.endswith("ん")
                            result.append(
                                {
                                    "surface": candidate,
                                    "pos": "その他" if mimetic else base.get("pos", ""),
                                    "lemma": candidate if mimetic else base.get("lemma") or plain,
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
    finally:
        state.i = i
        state.merged = merged
        state.applied_rule = applied_rule
    return False
