"""Merge steps for tari adverbs, verb-derived nouns, name and script compounds, and colloquial forms."""

import regex

from .constants import (
    ADVERBIAL_NA_ADJECTIVES,
    COLLOQUIAL_PRONOUNS,
    DERIVED_VERB_FRAGMENT_SPAN,
    DERIVED_VERB_SUFFIX_FORMS,
    TARI_ADVERB_STEMS,
)
from .core_lexicon import core_headwords
from .mecab import mecab_analyze
from .merge_postprocessors import reads_as_continuative
from .merge_rules_helpers import _covered_length, _kanji_noun_run
from .merge_rules_state import MergeState
from .split_rules import base_from_renyokei

# The cells of がる after the や of やがる (やがら, やがり, やがる, やがれ, やがろ, やがっ).
_YAGARU_TAIL = regex.compile(r"が[らりるれろっ]")


# Characters a hashtag body may contain. A tag ends at whitespace, punctuation or any
# other symbol, so the body class is exactly "word text" in any script.
_HASHTAG_BODY_CLASS = r"[\p{Han}\p{Hiragana}\p{Katakana}\p{Latin}\p{Nd}_\u30FC\u3005]"


HASHTAG_BODY_CHAR = regex.compile(_HASHTAG_BODY_CLASS)


HASHTAG_BODY_RUN = regex.compile(_HASHTAG_BODY_CLASS + "+")


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


def _merge_tari_adverbs(state: MergeState) -> bool:
    """Tari-conjugation adverbs and their reduplicated stems."""
    tokens = state.tokens
    result = state.result
    i = state.i
    t = state.t
    merged = state.merged
    applied_rule = state.applied_rule
    remaining = state.remaining
    try:
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
    finally:
        state.i = i
        state.merged = merged
        state.applied_rule = applied_rule
    return False


def _merge_verb_derived_nouns(state: MergeState) -> bool:
    """Nouns and verbs derived from a verb continuative."""
    tokens = state.tokens
    result = state.result
    i = state.i
    t = state.t
    merged = state.merged
    applied_rule = state.applied_rule
    try:
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

        # 5a-adj. A bare i-adjective stem is not a free word, so before a kanji
        # continuative it is the head of a deverbal compound noun (甘噛み), the
        # same shape the reference lists whole (長生き, 早起き). The frame after
        # it has to select a nominal: する or a case particle, never a
        # continuative's own auxiliaries (高+すぎ+た stays a verb chain).
        if (
            not merged
            and t.get("pos") == "形容詞"
            and t.get("conj_form") == "ガル接続"
            and i + 2 < len(tokens)
            and tokens[i + 1].get("pos") == "動詞"
            and tokens[i + 1].get("pos_sub1") == "自立"
            and tokens[i + 1].get("conj_form") == "連用形"
            and regex.match(r"\p{Han}", tokens[i + 1].get("surface", ""))
            and (
                (tokens[i + 2].get("pos") == "動詞" and tokens[i + 2].get("lemma") == "する")
                or (tokens[i + 2].get("pos") == "助詞" and tokens[i + 2].get("surface") in {"を", "が", "の"})
            )
        ):
            compound = t.get("surface", "") + tokens[i + 1].get("surface", "")
            result.append({"surface": compound, "pos": "名詞", "pos_sub1": "サ変接続", "lemma": compound})
            i += 2
            merged = True
            if applied_rule is None:
                applied_rule = "adjective-stem+continuative-noun"

        # 5a-adj2. A noun the reference cuts off a listed adjective leaves a
        # kana adjective of its own behind (愛 + おしい for 愛おしい). The joined
        # spelling is the listed word, inflected as the kana half is.
        if (
            not merged
            and t.get("pos") == "名詞"
            and i + 1 < len(tokens)
            and tokens[i + 1].get("pos") == "形容詞"
            and regex.fullmatch(r"\p{Hiragana}+", tokens[i + 1].get("surface", ""))
            and t.get("surface", "") + tokens[i + 1].get("lemma", "") in core_headwords("adjectives.tsv")
        ):
            adjective = tokens[i + 1]
            result.append(
                {
                    **adjective,
                    "surface": t.get("surface", "") + adjective.get("surface", ""),
                    "lemma": t.get("surface", "") + adjective.get("lemma", ""),
                }
            )
            i += 2
            merged = True
            if applied_rule is None:
                applied_rule = "noun+kana-adjective-listed"

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
    finally:
        state.i = i
        state.merged = merged
        state.applied_rule = applied_rule
    return False


def _merge_name_and_script_compounds(state: MergeState) -> bool:
    """Region, kanji, katakana, identifier, mention, and hashtag compounds."""
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
    finally:
        state.i = i
        state.merged = merged
        state.applied_rule = applied_rule
    return False


def _merge_colloquial_forms(state: MergeState) -> bool:
    """Colloquial pronouns and character speech."""
    tokens = state.tokens
    result = state.result
    i = state.i
    t = state.t
    merged = state.merged
    applied_rule = state.applied_rule
    remaining = state.remaining
    try:
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

        # 8d'. The same auxiliary after a one-kanji ichidan stem comes back as the
        # continuative of やる (見+や+がっ+て as 見や+がっ+て, from 見やる) plus a
        # がる suffix; the や belongs to the auxiliary, not to the verb.
        if (
            not merged
            and t.get("pos") == "動詞"
            and len(t.get("surface", "")) == 2
            and t["surface"].endswith("や")
            and (t.get("lemma") or "").endswith("やる")
            and i + 1 < len(tokens)
            and _YAGARU_TAIL.fullmatch(tokens[i + 1].get("surface", ""))
        ):
            stem = t["surface"][:-1]
            result.append({"surface": stem, "pos": "動詞", "lemma": base_from_renyokei(stem) or stem + "る"})
            result.append({"surface": "や" + tokens[i + 1]["surface"], "pos": "助動詞", "lemma": "やがる"})
            i += 2
            merged = True
            if applied_rule is None:
                applied_rule = "pejorative-yagaru"
    finally:
        state.i = i
        state.t = t
        state.merged = merged
        state.applied_rule = applied_rule
    return False
