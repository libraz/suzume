"""Postprocessors ported from SuzumeUtils.pm _postprocess_* functions."""

from collections.abc import Callable
from functools import wraps

import regex

from .constants import (
    ADVERB_NOMINAL_HOMOGRAPHS,
    COMPOUND_VERB_V2_GODAN,
    COMPOUND_VERB_V2_ICHIDAN,
    COPULA_SURFACES,
    COUNTER_UNITS,
    EMPHATIC_SOKUON,
    INTERROGATIVES,
    KYUJITAI_TO_SHINJITAI,
    QUANTITY_BOUND_SUFFIXES,
    SLANG_ADJ_STEMS,
    SLANG_VERB_STEMS,
    TEMPORAL_COMPOUND_UNITS,
    TEMPORAL_PREFIX_KANJI,
    UNUSUAL_NAMES,
    WORD_EXCEPTION_BLOCKED_FOLLOWERS,
    WORD_EXCEPTIONS,
)
from .core_lexicon import adjective_garu_stems, core_headwords
from .mecab import mecab_analyze
from .merge_postprocessors import NIDAN_TERMINAL_KANA
from .pos_mapping import _is_katakana_onomatopoeia
from .split_rules import base_from_mizenkei, base_from_renyokei

_PRODUCTIVE_COMPOUND_V2 = frozenset(COMPOUND_VERB_V2_GODAN + COMPOUND_VERB_V2_ICHIDAN)
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


def _is_emphatic_spelling(original: str, standard: str) -> bool:
    """Whether `original` is `standard` written with emphatic lengthening or repetition.

    Scoped to prolonged sound marks: removing them and collapsing runs of the same
    character reduces かわいーー and すごーーい to their dictionary forms, while a genuine
    lexical substitution (にゃー → ねえ) stays distinct. Vowel repetition without a mark
    (すごいいいい) is left alone — the tokenizer does not yet reduce it either.
    """
    if original == standard or "ー" not in original:
        return False
    reduced = regex.sub(r"(.)\1+", r"\1", regex.sub(r"ー+", "", original))
    return reduced == regex.sub(r"(.)\1+", r"\1", standard)


def reports_mutation(processor: Callable[[list[dict]], object]) -> Callable[[list[dict]], bool]:
    """Adapt a mutating postprocessor to the shared changed/not-changed contract."""

    @wraps(processor)
    def wrapped(tokens: list[dict]) -> bool:
        before = [token.copy() for token in tokens]
        processor(tokens)
        return tokens != before

    return wrapped


def _raw_analysis(text: str) -> tuple[int, dict[int, dict]]:
    """Analyze the untouched text, returning its token count and start index."""
    index: dict[int, dict] = {}
    offset = 0
    count = 0
    for token in mecab_analyze(text):
        index[offset] = token
        offset += len(token.get("surface", ""))
        count += 1
    return count, index


def _accept_slang_match(
    text: str, raw: tuple[int, dict[int, dict]], start: int, stem: str, standard: str, native_pos: str
) -> bool:
    """Decide whether a slang stem spelled at `start` is a real occurrence.

    A stem written in kana is also a substring of ordinary words (ださ inside
    ください, いた inside 聞いた, えも across 答え+も), so the surface match alone
    cannot carry the decision. The untouched analysis settles it structurally,
    on two counts:

    - The stem starts where a token starts, and the analysis stops there rather
      than reading on. A token that runs past the stem is already a word in its
      own right — やばい, 痛い and 甚く spelled in kana — and is left as it stands.
    - Otherwise the stem starts inside a token, which is where a kana spelling is
      most often a coincidence. Only a genuine occurrence was holding the rest of
      the sentence apart, so substituting it both reads cleanly in its own right
      and reunites neighbouring fragments into strictly fewer tokens (質/問う/ざい
      becomes 質問/赤い). A coincidental match fails one of the two: it either
      buries the substitute in an unknown blob (聞い/た becomes 聞赤) or breaks the
      word it was hiding in without repairing anything (ください becomes く/赤い).
    """
    raw_count, index = raw
    token = index.get(start)
    if token is not None:
        return len(token["surface"]) <= len(stem)

    probe = text[:start] + standard + text[start + len(stem) :]
    probe_count, probe_index = _raw_analysis(probe)
    landed = probe_index.get(start)
    return landed is not None and landed["pos"] == native_pos and probe_count < raw_count


def _non_overlapping_replacements(candidates: dict[tuple[int, str], dict]) -> dict[tuple[int, str], dict]:
    """Keep leftmost, longest pre-analysis replacements with disjoint spans."""
    selected: dict[tuple[int, str], dict] = {}
    covered_until = 0
    for key, replacement in sorted(candidates.items(), key=lambda item: (item[0][0], -item[1]["length"], item[0][1])):
        start = key[0]
        if start < covered_until:
            continue
        selected[key] = replacement
        covered_until = start + replacement["length"]
    return selected


def preprocess_for_mecab(text: str) -> tuple[str, dict[tuple[int, str], dict], tuple[str, ...]]:
    """Replace slang stems with standard ones before MeCab analysis.

    Returns:
        Tuple of (processed text, replacements dict, applied rule names).
        The replacements dict is keyed by (start position, category); keying on
        the category as well as the start position keeps replacements from
        different categories that happen to match at the same offset from
        silently overwriting one another. The rule names report only categories
        that actually replaced text, so callers do not mislabel altered MeCab
        output as a raw MeCab result.
    """
    replacements: dict[tuple[int, str], dict] = {}

    # Analyzed on first use: only a text that actually spells a slang stem pays
    # for the extra pass over the untouched string.
    raw: tuple[int, dict[int, dict]] | None = None

    slang_categories = (
        ("slang_adj", SLANG_ADJ_STEMS, r"[いかくけさ]", "形容詞"),
        ("slang_verb", SLANG_VERB_STEMS, r"[らりるれろっ]", "動詞"),
    )
    for category, stems, ending, native_pos in slang_categories:
        for slang, standard in stems.items():
            for m in regex.finditer(regex.escape(slang) + ending, text):
                if raw is None:
                    raw = _raw_analysis(text)
                if not _accept_slang_match(text, raw, m.start(), slang, standard, native_pos):
                    continue
                replacements[(m.start(), category)] = {
                    "original": slang,
                    "replacement": standard,
                    "length": len(slang),
                }

    # Unusual names
    for name, standard in UNUSUAL_NAMES.items():
        for m in regex.finditer(regex.escape(name) + r"(さん|ちゃん|様|君|殿)", text):
            replacements[(m.start(), "unusual_name")] = {
                "original": name,
                "replacement": standard,
                "length": len(name),
            }

    # Word exceptions
    for word, standard in WORD_EXCEPTIONS.items():
        for m in regex.finditer(regex.escape(word), text):
            blocked_followers = WORD_EXCEPTION_BLOCKED_FOLLOWERS.get(word, ())
            if any(text.startswith(follower, m.end()) for follower in blocked_followers):
                continue
            replacements[(m.start(), "word_exception")] = {
                "original": word,
                "replacement": standard,
                "length": len(word),
            }

    # Emphatic sokuon
    for pattern, standard in EMPHATIC_SOKUON.items():
        for m in regex.finditer(regex.escape(pattern) + r"(?!て)", text):
            replacements[(m.start(), "emphatic_sokuon")] = {
                "original": pattern,
                "replacement": standard,
                "length": len(pattern),
            }

    # Pre-1946 kanji forms the dictionary has no entry for come back as unknown
    # tokens (lemma "*"), and the compound rules then glue the run into a
    # non-word noun (心 + 亂 → 心亂), stranding the okurigana. Folding only the
    # characters that actually landed inside an unknown token lets the
    # dictionary read the word in its modern spelling; the offset bookkeeping
    # below restores the original character into the surface. A form the
    # dictionary already holds never reaches this branch, so its own entry keeps
    # deciding the analysis.
    if any(char in KYUJITAI_TO_SHINJITAI for char in text):
        if raw is None:
            raw = _raw_analysis(text)
        for start, token in raw[1].items():
            if token.get("lemma", "*") != "*":
                continue
            for offset, char in enumerate(token.get("surface", "")):
                modern = KYUJITAI_TO_SHINJITAI.get(char)
                if modern is None:
                    continue
                replacements[(start + offset, "kyujitai")] = {
                    "original": char,
                    "replacement": modern,
                    "length": 1,
                }

    # Multiple rule families can recognize overlapping text. Select a single,
    # leftmost longest match before either mutation or offset accounting so the
    # two passes always describe the same disjoint spans.
    replacements = _non_overlapping_replacements(replacements)

    # Apply replacements in reverse position order
    for key in sorted(replacements, key=lambda k: k[0], reverse=True):
        pos = key[0]
        r = replacements[key]
        text = text[:pos] + r["replacement"] + text[pos + r["length"] :]

    # Record each replacement's coordinate in the processed text. Later
    # restoration must target this exact span: replacement strings are often
    # ordinary words that can also occur elsewhere in the same sentence.
    offset_delta = 0
    for key in sorted(replacements, key=lambda item: item[0]):
        replacement = replacements[key]
        replacement["processed_start"] = key[0] + offset_delta
        replacement["processed_length"] = len(replacement["replacement"])
        offset_delta += replacement["processed_length"] - replacement["length"]

    rule_names = {
        "slang_adj": "slang-adjective",
        "slang_verb": "slang-verb",
        "unusual_name": "unusual-name",
        "word_exception": "word-exception",
        "emphatic_sokuon": "emphatic-sokuon",
        "kyujitai": "kyujitai-fold",
    }
    rules = tuple(dict.fromkeys(rule_names[category] for _, category in replacements))
    return text, replacements, rules


def postprocess_mecab_tokens(
    tokens: list[dict], original_text: str, replacements: dict[tuple[int, str], dict]
) -> list[dict]:
    """Restore slang terms in tokens after MeCab processing."""
    if not replacements:
        return tokens

    # Offset-based restoration. Compute token spans before mutating any surface
    # so length-changing replacements cannot move subsequent coordinates.
    token_spans = []
    processed_pos = 0
    for token in tokens:
        surface = token.get("surface", "")
        token_spans.append((processed_pos, processed_pos + len(surface)))
        processed_pos += len(surface)

    for token, (token_start, token_end) in zip(tokens, token_spans, strict=True):
        patches = []
        for replacement in replacements.values():
            replacement_start = replacement["processed_start"]
            replacement_end = replacement_start + replacement["processed_length"]
            if token_start <= replacement_start and replacement_end <= token_end:
                patches.append(
                    (
                        replacement_start - token_start,
                        replacement_end - token_start,
                        replacement["original"],
                        replacement["replacement"],
                    )
                )

        if not patches:
            continue
        surface = token.get("surface", "")
        for local_start, local_end, original, _ in sorted(patches, reverse=True):
            surface = surface[:local_start] + original + surface[local_end:]
        token["surface"] = surface

        lemma = token.get("lemma", "")
        for _, _, original, standard in patches:
            # An emphatic spelling keeps the dictionary form as its lemma: かわいーー is
            # still かわいい. Restoring the original there would make the lemma a non-word
            # and contradict the lengthening rules, which already yield the plain form.
            if _is_emphatic_spelling(original, standard):
                continue
            if lemma and standard in lemma:
                lemma = lemma.replace(standard, original, 1)
        if lemma:
            token["lemma"] = lemma

    # Surface realignment
    total_surface = sum(len(t.get("surface", "")) for t in tokens)
    if total_surface != len(original_text):
        pos = 0
        for idx, t in enumerate(tokens):
            orig_at_pos = original_text[pos:] if pos < len(original_text) else ""
            if not orig_at_pos.startswith(t.get("surface", "")):
                if idx > 0:
                    prev = tokens[idx - 1]
                    prev_pos = pos - len(prev.get("surface", ""))
                    for ext in range(1, 6):
                        try_len = len(prev.get("surface", "")) + ext
                        after = original_text[prev_pos + try_len :]
                        if after.startswith(t.get("surface", "")):
                            prev["surface"] = original_text[prev_pos : prev_pos + try_len]
                            pos = prev_pos + try_len
                            break
            pos += len(t.get("surface", ""))

    return tokens


def repair_kanji_prefix_before_kana_noun(tokens: list[dict]) -> None:
    """Rebuild the boundary where a stem's okurigana was read as a noun.

    A kanji prefix forms a compound noun with the noun it attaches to, so that
    host is written in kanji as well — the merge rule that joins the pair asks
    for exactly that. A bare-hiragana noun therefore never continues one, and
    when the analyzer emits that pair it has taken a stem's okurigana for the
    start of the following word: 抗いし者 comes back as 抗(接頭詞) + いし(名詞) +
    者, a split that leaves a lemma the sentence never contained and that the
    analyzer itself does not make when the same two morae end the input
    (抗いし alone is read 抗い + し).

    The dictionary says where the boundary belongs: the prefix plus the noun's
    first kana is a headword of its own, which is what makes the kanji a stem
    with okurigana rather than a prefix. Both pieces are re-analyzed from
    there, so the class each one lands in stays the dictionary's own.
    """
    for index in range(len(tokens) - 2, -1, -1):
        token = tokens[index]
        follower = tokens[index + 1]
        if token.get("pos") != "接頭詞" or token.get("pos_sub1") != "名詞接続":
            continue
        prefix = token.get("surface", "")
        noun = follower.get("surface", "")
        if not regex.fullmatch(r"\p{Han}+", prefix):
            continue
        if follower.get("pos") != "名詞" or not regex.fullmatch(r"\p{Hiragana}{2,}", noun):
            continue
        head = prefix + noun[0]
        analyzed = mecab_analyze(head)
        if len(analyzed) != 1 or analyzed[0].get("surface") != head:
            continue
        tokens[index : index + 2] = [analyzed[0], *mecab_analyze(noun[1:])]


def split_transparent_suru_te_adverb(tokens: list[dict]) -> None:
    """Split a lexical adverb that is transparently 名詞 + し + て.

    The reference dictionary files 心して as one adverb while the identically
    built 用心して and 安心して stay 名詞+し+て, which puts a lexical boundary across
    an inflecting stem and its conjunctive particle. Two of the dictionary's own
    marks decide which it is, and the fossilized adverbs fail one or the other:

    - The entry is still filed as taking particles (助詞類接続) rather than as a
      plain adverb, which already separates 心して from 概して, 決して and 大して.
    - Its reading is the noun's own reading followed by シテ. 決して reads ケッシテ
      rather than ケツ+シテ and 大して reads タイシテ rather than ダイ+シテ, while a
      kana stem (どうして, まして) never enters the rule at all.
    """
    for index in range(len(tokens) - 1, -1, -1):
        token = tokens[index]
        surface = token.get("surface", "")
        reading = token.get("reading", "")
        if token.get("pos") != "副詞" or token.get("pos_sub1") != "助詞類接続":
            continue
        if not surface.endswith("して") or len(surface) <= 2:
            continue
        stem = surface[:-2]
        if not regex.fullmatch(r"\p{Han}+", stem) or not reading.endswith("シテ"):
            continue
        analyzed = mecab_analyze(stem)
        if len(analyzed) != 1 or analyzed[0].get("pos") != "名詞":
            continue
        if analyzed[0].get("reading", "") != reading[:-2]:
            continue
        tokens[index : index + 1] = [
            analyzed[0],
            {
                "surface": "し",
                "pos": "動詞",
                "pos_sub1": "自立",
                "conj_type": "サ変・スル",
                "conj_form": "連用形",
                "lemma": "する",
                "reading": "シ",
            },
            {
                "surface": "て",
                "pos": "助詞",
                "pos_sub1": "接続助詞",
                "conj_type": "",
                "conj_form": "",
                "lemma": "て",
                "reading": "テ",
            },
        ]


# The ない-family cells a predicate can spell: ない / なく(て) / なかっ(た) /
# なけれ(ば) / なけりゃ / なきゃ. The analyzer cuts each of them at the tail, so
# only the head is matched here.
_NAI_NEGATIVE_HEAD = regex.compile(r"^な(い|く|かっ|けれ|けりゃ|きゃ)")


def repair_kko_nominalizer(tokens: list[dict]) -> None:
    """Rebuild the bound nominalizer っこ before a ない-family predicate.

    The reference dictionary has no entry for っこ, so it reads the two morae as
    the emphatic sokuon plus the irrealis of 来る and then reconstructs a verb
    around whatever is left: 負ける becomes 負/ける with the okurigana glued to the
    sokuon (負+けっ+こ), できる becomes で+きっ+こ, and a stem whose okurigana is
    already a full continuative simply keeps a standalone っ (分かり+っ+こ). Every
    host breaks, so the suffix is restored here rather than corrected per word.

    The continuative in front of the suffix is recovered by re-analyzing the
    prefix under ます, which selects that cell and nothing else, and the ない that
    follows is left as MeCab tagged it — it is the predicate of the construction.
    Re-analysis starts after the previous repair, because feeding an already
    repaired っこ back to the analyzer would only break it the same way again.
    """
    idx = 1
    repaired_end = 0
    while idx < len(tokens) - 1:
        token = tokens[idx]
        previous = tokens[idx - 1]
        if (
            token.get("surface") != "こ"
            or not previous.get("surface", "").endswith("っ")
            or not _NAI_NEGATIVE_HEAD.match(tokens[idx + 1].get("surface", ""))
        ):
            idx += 1
            continue
        prefix = "".join(t.get("surface", "") for t in tokens[repaired_end:idx])[:-1]
        continuative = mecab_analyze(prefix + "ます")
        if not continuative or continuative[-1].get("surface") != "ます":
            idx += 1
            continue
        suffix = {"surface": "っこ", "pos": "名詞", "pos_sub1": "接尾", "lemma": "っこ"}
        tokens[repaired_end : idx + 1] = [*continuative[:-1], suffix]
        repaired_end += len(continuative)
        idx = repaired_end
    return


@reports_mutation
def postprocess_sou(tokens: list[dict]) -> bool:
    """Context-dependent そう normalization."""
    for i, t in enumerate(tokens):
        if t.get("surface") != "そう":
            continue

        # 伝聞そう before copula -> Adjective
        # Only when preceded by Auxiliary (だ/た etc.), not by Verb (様態そう)
        pos = t.get("pos", "")
        if pos in ("Adverb", "Auxiliary"):
            if i < len(tokens) - 1 and i > 0:
                nxt = tokens[i + 1].get("surface", "")
                prev_pos = tokens[i - 1].get("pos", "")
                # The explanatory な+の/ん chain takes the same reading as the
                # bare copula: after a terminal verb, そう is the hearsay
                # auxiliary in both (読む+そう+だ, 読む+そう+な+ん+だ).
                if prev_pos != "Verb" and (
                    regex.match(r"^(?:だ|です|でし|じゃ|じゃろ)", nxt)
                    or (nxt == "な" and i + 2 < len(tokens) and tokens[i + 2].get("surface") in ("の", "ん"))
                ):
                    t["pos"] = "Adjective"
                elif nxt == "で" and i + 2 < len(tokens):
                    following = tokens[i + 2].get("surface", "")
                    after_topic = tokens[i + 3].get("surface", "") if i + 3 < len(tokens) else ""
                    if following in ("ある", "あり", "あれ", "あっ", "ない", "なく", "なかっ", "なけれ", "なかろ") or (
                        following == "は" and after_topic in ("ない", "なく", "なかっ", "なけれ", "なかろ")
                    ):
                        t["pos"] = "Adjective"
            elif i == 0:  # Sentence-initial そう before copula
                if i < len(tokens) - 1:
                    nxt = tokens[i + 1].get("surface", "")
                    if regex.match(r"^(?:だ|です|でし|じゃ|じゃろ)", nxt) or (
                        nxt == "な" and i + 2 < len(tokens) and tokens[i + 2].get("surface") in ("の", "ん")
                    ):
                        t["pos"] = "Adjective"
                    elif nxt == "で" and i + 2 < len(tokens):
                        following = tokens[i + 2].get("surface", "")
                        after_topic = tokens[i + 3].get("surface", "") if i + 3 < len(tokens) else ""
                        if following in (
                            "ある",
                            "あり",
                            "あれ",
                            "あっ",
                            "ない",
                            "なく",
                            "なかっ",
                            "なけれ",
                            "なかろ",
                        ) or (following == "は" and after_topic in ("ない", "なく", "なかっ", "なけれ", "なかろ")):
                            t["pos"] = "Adjective"

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


@reports_mutation
def postprocess_demo(tokens: list[dict]) -> bool:
    """Resolve でも by clause position rather than its reference POS tag."""
    for idx, t in enumerate(tokens):
        if t.get("surface") != "でも":
            continue
        at_clause_boundary = idx == 0 or tokens[idx - 1].get("pos") == "Symbol"
        t["pos"] = "Conjunction" if at_clause_boundary else "Particle"
        t["lemma"] = "でも"


def postprocess_closed_function_words(tokens: list[dict]) -> bool:
    """Normalize finite conjunction and pronoun classes mislabelled by MeCab."""
    conjunctions = frozenset({"しかるに", "もって"})
    pronouns = frozenset({"各々", "各自", "あれこれ", "何かしら"})
    adverbial_ambiguities = frozenset({"また", "やや", "およそ", "すこぶる", "おおいに", "つとめて"})
    changed = False
    for idx, token in enumerate(tokens):
        surface = token.get("surface", "")
        target_pos = (
            "Conjunction"
            if surface in conjunctions
            else "Pronoun"
            if surface in pronouns
            else "Adverb"
            if surface in adverbial_ambiguities
            else ""
        )
        if not target_pos or token.get("pos") == target_pos:
            continue
        token["pos"] = target_pos
        token["lemma"] = surface
        changed = True
        # MeCab can carry a suffix reading across the following content word
        # after a formal adverb (つとめて水を...).  At this proven adverbial
        # boundary the following lexical token is an ordinary noun.
        if (
            idx + 2 < len(tokens)
            and tokens[idx + 1].get("pos") == "Suffix"
            and tokens[idx + 2].get("pos") == "Particle"
        ):
            tokens[idx + 1]["pos"] = "Noun"
            tokens[idx + 1]["lemma"] = tokens[idx + 1].get("surface", "")
    return changed


def postprocess_closed_subsidiary_aux(tokens: list[dict]) -> bool:
    """Mirror the core's finite renyokei-attaching subsidiary class."""
    lemmas = {
        "かね": "かねる",
        "かねる": "かねる",
        "たまえ": "たまう",
        "そびれ": "そびれる",
        "そびれる": "そびれる",
        "あぐね": "あぐねる",
        "あぐねる": "あぐねる",
        "そこね": "そこねる",
        "そこない": "そこなう",
        "そこなう": "そこなう",
        "そこなっ": "そこなう",
        "そこなわ": "そこなう",
        "そこなえ": "そこなう",
    }
    changed = False
    for idx in range(1, len(tokens)):
        previous = tokens[idx - 1]
        token = tokens[idx]
        lemma = lemmas.get(token.get("surface", ""))
        if previous.get("pos") != "Verb" or lemma is None:
            continue
        token["pos"] = "Auxiliary"
        token["lemma"] = lemma
        changed = True
    for idx, token in enumerate(tokens):
        surface = token.get("surface", "")
        if surface == "じゃろ":
            if token.get("pos") != "Auxiliary" or token.get("lemma") != "だろ":
                token["pos"] = "Auxiliary"
                token["lemma"] = "だろ"
                changed = True
        elif surface == "ござら" and idx > 0 and tokens[idx - 1].get("surface") == "で":
            if token.get("pos") != "Auxiliary" or token.get("lemma") != "ござる":
                token["pos"] = "Auxiliary"
                token["lemma"] = "ござる"
                changed = True
    return changed


def postprocess_classical_focus_namu(tokens: list[dict]) -> bool:
    """Classify classical なむ before a quotation boundary as a particle."""
    changed = False
    for idx, token in enumerate(tokens[:-1]):
        if token.get("surface") != "なむ" or tokens[idx + 1].get("surface") != "と":
            continue
        if token.get("pos") != "Particle":
            token["pos"] = "Particle"
            token["lemma"] = "なむ"
            changed = True
    return changed


def postprocess_classical_copula_nari(tokens: list[dict]) -> bool:
    """Classify classical なり on a nominal host before a quotation as the copula.

    The reference analyzer already reads sentence-final なり after a noun as the
    classical copula (時は金なり), but tags the same word as a particle as soon as
    a quotation follows it. The host and the reading do not change: the quotative
    と closes a clause, so what precedes it is a predicate. The listing particle
    なり attaches to a terminal verb (行くなり来るなり), and the adverbial 〜なりに /
    〜なりの suffix is followed by に or の, so neither is reached here.
    """
    changed = False
    for idx in range(1, len(tokens) - 1):
        token = tokens[idx]
        if (
            token.get("surface") == "なり"
            and token.get("pos") == "Particle"
            and tokens[idx - 1].get("pos") == "Noun"
            and tokens[idx + 1].get("surface") == "と"
        ):
            token["pos"] = "Auxiliary"
            token["lemma"] = "なり"
            changed = True
    return changed


def _spells_verb_continuative(surface: str) -> bool:
    """Whether this surface is a verb's continuative rather than a finite cell.

    The reference dictionary names the cell only in the raw analysis, which the
    mapped tokens no longer carry, so ask it in an environment that admits
    nothing else: the polite ます selects the continuative and leaves a finite
    form unchanged (見+ます against やる+ます).
    """
    from .mecab import mecab_analyze

    probe = mecab_analyze(surface + "ます")
    return (
        len(probe) == 2
        and probe[0].get("surface") == surface
        and probe[0].get("pos") == "動詞"
        and probe[0].get("conj_form") == "連用形"
    )


def postprocess_classical_past_izenkei_shika(tokens: list[dict]) -> bool:
    """Classify しか after a verb continuative as the classical past auxiliary き.

    The reference analyzer already reads it that way when the conditional ば
    follows (見しかば), and tags the identical pair as the modern exclusive
    particle as soon as the clause ends there (月こそ見しか). Nothing about the
    host changes between the two: the exclusive particle attaches to a nominal
    and needs a negative to complete it (これしかない, 水しか飲まない), and a
    finite verb in front of it keeps that reading (見るしかない), so it is the
    continuative host that decides.
    """
    changed = False
    for idx in range(1, len(tokens)):
        token = tokens[idx]
        if (
            token.get("surface") == "しか"
            and token.get("pos") == "Particle"
            and tokens[idx - 1].get("pos") == "Verb"
            and _spells_verb_continuative(tokens[idx - 1].get("surface", ""))
        ):
            token["pos"] = "Auxiliary"
            token["lemma"] = "き"
            changed = True
    return changed


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


@reports_mutation
def postprocess_iru_aux(tokens: list[dict]) -> bool:
    """Fix dependent い/いる after a te-form: Verb -> Auxiliary."""
    focus_particles = frozenset({"さえ", "しか", "こそ", "も"})
    for i in range(1, len(tokens)):
        t = tokens[i]
        surface = t.get("surface", "")
        if surface not in ("い", "いる", "いれ", "いた", "いない"):
            continue
        if t.get("pos") != "Verb":
            continue
        prev_surface = tokens[i - 1].get("surface", "")
        direct_te_form = prev_surface in ("て", "で")
        focused_te_form = (
            surface in ("い", "いる")
            and prev_surface in focus_particles
            and i >= 2
            and tokens[i - 2].get("surface") in ("て", "で")
        )
        if direct_te_form or focused_te_form:
            t["pos"] = "Auxiliary"
            t["lemma"] = "いる"


def postprocess_giving_aux(tokens: list[dict]) -> bool:
    """Classify productive て/で + giving/receiving verbs as auxiliaries."""
    auxiliary_lemmas = frozenset({"あげる", "くれる", "もらう"})
    changed = False
    for idx in range(1, len(tokens)):
        token = tokens[idx]
        if token.get("lemma") not in auxiliary_lemmas:
            continue
        if tokens[idx - 1].get("surface") not in ("て", "で"):
            continue
        if token.get("pos") != "Auxiliary":
            token["pos"] = "Auxiliary"
            changed = True
    return changed


def postprocess_contracted_progressive_aux(tokens: list[dict]) -> bool:
    """Normalize the voiced progressive contraction after an onbin stem."""
    changed = False
    for idx in range(1, len(tokens)):
        token = tokens[idx]
        previous = tokens[idx - 1]
        if token.get("surface") != "でる" or previous.get("pos") != "Verb":
            continue
        if not previous.get("surface", "").endswith("ん"):
            continue
        token["pos"] = "Auxiliary"
        token["lemma"] = "いる"
        changed = True
    return changed


@reports_mutation
def postprocess_miru_aux(tokens: list[dict]) -> bool:
    """Classify trial みる after a te-form boundary as Auxiliary."""
    trial_surfaces = {"み", "みる", "みれ", "みろ", "みよ"}
    for idx in range(1, len(tokens)):
        token = tokens[idx]
        if token.get("surface") not in trial_surfaces:
            continue
        prev_idx = idx - 1
        if tokens[prev_idx].get("surface") == "も" and prev_idx > 0:
            prev_idx -= 1
        if tokens[prev_idx].get("surface") not in ("て", "で"):
            continue
        token["pos"] = "Auxiliary"
        token["lemma"] = "みる"


def postprocess_shimau_aux(tokens: list[dict]) -> bool:
    """Normalize the closed completive auxiliary paradigm.

    Besides the kanji spelling, repair MeCab's occasional analysis of the
    voiced contraction ``じゃう`` as the copula ``じゃ`` plus an
    interjection ``う``.  A preceding verbal nasal-onbin stem makes the
    completive reading grammatical and unambiguous.
    """
    shimau_forms = frozenset({"仕舞う", "仕舞わ", "仕舞い", "仕舞っ", "仕舞え", "仕舞お"})
    contracted_forms = frozenset(
        {
            "ちゃう",
            "ちゃわ",
            "ちゃい",
            "ちゃっ",
            "ちゃえ",
            "ちゃお",
            "じゃう",
            "じゃわ",
            "じゃい",
            "じゃっ",
            "じゃえ",
            "じゃお",
        }
    )
    contracted_endings = frozenset("うわいっえお")
    changed = False
    idx = 1
    while idx < len(tokens):
        token = tokens[idx]
        previous = tokens[idx - 1]
        if token.get("surface") in shimau_forms and previous.get("surface") in ("て", "で"):
            token["pos"] = "Auxiliary"
            token["lemma"] = "しまう"
            changed = True
            idx += 1
            continue

        surface = token.get("surface", "")
        previous_is_host = previous.get("pos") == "Verb" or (
            surface.startswith("ちゃ") and previous.get("pos") == "Auxiliary"
        )
        if (
            previous_is_host
            and surface in contracted_forms
            and (not surface.startswith("じゃ") or previous.get("surface", "").endswith("ん"))
        ):
            token["pos"] = "Auxiliary"
            token["lemma"] = surface[:2] + "う"
            changed = True
            idx += 1
            continue

        if (
            idx + 1 < len(tokens)
            and previous_is_host
            and surface in ("ちゃ", "じゃ")
            and tokens[idx + 1].get("surface") in contracted_endings
            and (surface != "じゃ" or previous.get("surface", "").endswith("ん"))
        ):
            contracted = surface + tokens[idx + 1]["surface"]
            tokens[idx : idx + 2] = [{"surface": contracted, "pos": "Auxiliary", "lemma": surface + "う"}]
            changed = True
        idx += 1
    return changed


def postprocess_quantity_bound_suffix(tokens: list[dict]) -> bool:
    """Split a numeral+counter phrase from its closed-class bound suffix."""
    counter_pattern = "|".join(regex.escape(unit) for unit in sorted(COUNTER_UNITS, key=len, reverse=True))
    suffix_pattern = "|".join(regex.escape(suffix) for suffix in QUANTITY_BOUND_SUFFIXES)
    quantity_only_pattern = regex.compile(rf"^[0-9０-９〇零一二三四五六七八九十百千万億兆]+(?:{counter_pattern})$")
    quantity_pattern = regex.compile(
        rf"^(?P<quantity>[0-9０-９〇零一二三四五六七八九十百千万億兆]+(?:{counter_pattern}))"
        rf"(?P<suffix>{suffix_pattern})$"
    )
    changed = False
    index = 0
    while index < len(tokens):
        token = tokens[index]
        # MeCab sometimes already supplies the quantity and suffix as separate
        # tokens but tags the homographic suffix as a verb stem (二本|立て).
        # The preceding quantity fixes the closed-class suffix reading.
        if (
            index > 0
            and token.get("surface") in QUANTITY_BOUND_SUFFIXES
            and quantity_only_pattern.fullmatch(tokens[index - 1].get("surface", ""))
        ):
            token["pos"] = "Suffix"
            token["lemma"] = token["surface"]
            changed = True
            index += 1
            continue
        match = quantity_pattern.fullmatch(token.get("surface", ""))
        if match is None:
            index += 1
            continue
        quantity = match.group("quantity")
        suffix = match.group("suffix")
        tokens[index : index + 1] = [
            {"surface": quantity, "pos": "Noun", "lemma": quantity},
            {"surface": suffix, "pos": "Suffix", "lemma": suffix},
        ]
        changed = True
        index += 2
    return changed


def postprocess_exclusion_suffix(tokens: list[dict]) -> bool:
    """Classify nominal X+抜き/ぬき constructions as exclusion suffixes."""
    changed = False
    for idx, token in enumerate(tokens):
        if idx == 0 or token.get("surface") not in ("抜き", "ぬき"):
            continue
        previous = tokens[idx - 1]
        if previous.get("pos") not in ("Noun", "Prefix"):
            continue
        if previous.get("surface") == "中" and previous.get("pos") == "Prefix":
            previous["pos"] = "Noun"
            changed = True
        if token.get("pos") != "Suffix":
            token["pos"] = "Suffix"
            changed = True
    return changed


def postprocess_state_suffix(tokens: list[dict]) -> bool:
    """Classify nominal X+中 as a state suffix in nominal predicate positions."""
    changed = False
    for idx, token in enumerate(tokens[1:], start=1):
        if token.get("surface") != "中" or token.get("pos") != "Noun":
            continue
        if tokens[idx - 1].get("pos") != "Noun":
            continue
        following = tokens[idx + 1] if idx + 1 < len(tokens) else None
        if following is None or following.get("pos") == "Particle" or following.get("surface") in COPULA_SURFACES:
            token["pos"] = "Suffix"
            changed = True
    return changed


def postprocess_productive_verb_suffix_stem(tokens: list[dict]) -> bool:
    """Restore a verb continuative before a productive derivational suffix."""
    verb_suffixes = frozenset({"がち", "っぱなし", "たて", "まくり"})
    changed = False
    for idx in range(len(tokens) - 1):
        stem = tokens[idx]
        suffix = tokens[idx + 1]
        if suffix.get("surface") not in verb_suffixes:
            continue
        if suffix.get("surface") == "まくり" and stem.get("pos") == "Verb":
            if suffix.get("pos") != "Suffix" or suffix.get("lemma") != "まくり":
                suffix["pos"] = "Suffix"
                suffix["lemma"] = "まくり"
                changed = True
        if suffix.get("pos") != "Suffix":
            continue
        if stem.get("pos") == "Verb" and stem.get("lemma") != stem.get("surface"):
            continue
        lemma = base_from_renyokei(stem.get("surface", ""))
        if lemma is None:
            continue
        stem["pos"] = "Verb"
        stem["lemma"] = lemma
        changed = True
    return changed


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


def postprocess_renyokei_compound_particle(tokens: list[dict]) -> bool:
    """Keep closed particle plus continuative-form expressions as search units."""
    compound_particles = {
        ("に", "つれ", "て"): "につれて",
        ("に", "かけ", "て"): "にかけて",
    }
    changed = False
    index = 0
    while index + 2 < len(tokens):
        surfaces = tuple(tokens[index + offset].get("surface") for offset in range(3))
        compound = compound_particles.get(surfaces)
        if compound is None:
            index += 1
            continue
        tokens[index : index + 3] = [{"surface": compound, "pos": "Particle", "lemma": compound}]
        if (
            compound == "にかけて"
            and index + 1 < len(tokens)
            and tokens[index + 1].get("surface") == "続く"
            and tokens[index + 1].get("pos") == "Auxiliary"
        ):
            tokens[index + 1]["pos"] = "Verb"
            tokens[index + 1]["lemma"] = "続く"
        changed = True
        index += 1
    return changed


def postprocess_compound_case_particle_aru(tokens: list[dict]) -> bool:
    """Classify ある after an adverbial or a non-subject particle.

    A lexical adverb or closed compound case particle ending in て is not the
    conjunctive particle of a resultative 〜てある construction.  MeCab can
    nevertheless carry that dependent reading into the following cells of
    ある.  Restore the independent verb for inflected cells, while keeping the
    distinct pre-nominal ある reading as a determiner.
    """
    changed = False
    for index in range(1, len(tokens)):
        previous = tokens[index - 1]
        token = tokens[index]
        following = tokens[index + 1] if index + 1 < len(tokens) else None
        previous_is_productive_te = previous.get("pos") == "Particle" and previous.get("surface") in ("て", "で")
        if previous_is_productive_te and token.get("surface") == "ある" and token.get("lemma") == "ある":
            if token.get("pos") != "Verb":
                token["pos"] = "Verb"
                changed = True
            continue
        previous_is_adverbial = previous.get("pos") == "Adverb"
        previous_is_topic = previous.get("surface") == "は"
        previous_is_compound_case = (
            len(previous.get("surface", "")) > 1
            and previous.get("pos") == "Particle"
            and previous.get("pos_sub1") == "格助詞"
            and previous.get("pos_sub2") == "連語"
        )
        if not (previous_is_adverbial or previous_is_topic or previous_is_compound_case):
            continue
        if token.get("surface") == "ある" and following is not None and following.get("pos") in ("Noun", "Pronoun"):
            if token.get("pos") != "Determiner" or token.get("lemma") != "ある":
                token["pos"] = "Determiner"
                token["lemma"] = "ある"
                changed = True
        elif (
            (previous_is_adverbial or previous_is_compound_case)
            and token.get("surface") in ("ある", "あり", "あれ", "あっ")
            and token.get("lemma") == "ある"
        ):
            if token.get("pos") != "Verb":
                token["pos"] = "Verb"
                changed = True
    return changed


def postprocess_to_areba_conditional(tokens: list[dict]) -> bool:
    """Preserve the verb inflection and conditional-particle boundary in とあれば."""
    changed = False
    index = 0
    while index + 1 < len(tokens):
        if tokens[index].get("surface") == "と" and tokens[index + 1].get("surface") == "あれば":
            tokens[index + 1 : index + 2] = [
                {"surface": "あれ", "pos": "Verb", "lemma": "ある"},
                {"surface": "ば", "pos": "Particle", "lemma": "ば"},
            ]
            changed = True
            index += 3
            continue
        index += 1
    return changed


def postprocess_tagaru_aux(tokens: list[dict]) -> bool:
    """Keep the desiderative-observation auxiliary たがる as one search unit."""
    tagaru_forms = frozenset({"がら", "がり", "がる", "がれ", "がろ", "がっ"})
    changed = False
    idx = 0
    while idx + 1 < len(tokens):
        if tokens[idx].get("surface") == "た" and tokens[idx + 1].get("surface") in tagaru_forms:
            surface = "た" + tokens[idx + 1].get("surface", "")
            tokens[idx : idx + 2] = [{"surface": surface, "pos": "Auxiliary", "lemma": "たがる"}]
            changed = True
        idx += 1
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


def postprocess_indefinite_ka(tokens: list[dict]) -> bool:
    """Separate indefinite か from a pronoun and restore existential いる."""
    indefinite_pronoun_stems = frozenset({"なに", "何", "だれ", "誰", "どこ", "どちら", "どれ", "どなた"})
    changed = False
    idx = 0
    while idx < len(tokens):
        token = tokens[idx]
        surface = token.get("surface", "")

        if idx > 0 and idx + 1 < len(tokens) and surface == "で" and tokens[idx + 1].get("surface") == "も":
            if tokens[idx - 1].get("surface") in INTERROGATIVES:
                tokens[idx : idx + 2] = [{"surface": "でも", "pos": "Particle", "lemma": "でも"}]
                changed = True
                continue
        stem = surface[:-1] if surface.endswith("か") else ""
        if stem in indefinite_pronoun_stems:
            tokens[idx : idx + 1] = [
                {"surface": stem, "pos": "Pronoun", "lemma": stem},
                {"surface": "か", "pos": "Particle", "lemma": "か"},
            ]
            changed = True
            idx += 1
        elif idx > 0 and tokens[idx - 1].get("pos") == "Pronoun" and surface == "かい":
            tokens[idx : idx + 1] = [
                {"surface": "か", "pos": "Particle", "lemma": "か"},
                {"surface": "い", "pos": "Verb", "lemma": "いる"},
            ]
            changed = True
            idx += 1
        idx += 1

    for idx in range(1, len(tokens)):
        if tokens[idx - 1].get("surface") == "か" and tokens[idx].get("surface") in ("い", "いる"):
            tokens[idx]["pos"] = "Verb"
            tokens[idx]["lemma"] = "いる"
            changed = True
    return changed


def postprocess_subsidiary_yuku(tokens: list[dict]) -> bool:
    """Treat literary 連用形 + ゆく/いく as a subsidiary verb."""
    changed = False
    for idx in range(1, len(tokens)):
        previous = tokens[idx - 1]
        token = tokens[idx]
        if token.get("lemma") not in ("行く", "いく", "ゆく") and token.get("surface") not in (
            "いこ",
            "ゆこ",
            "いく",
            "ゆく",
        ):
            continue
        connective_te_de = previous.get("surface") == "て" or (
            previous.get("surface") == "で" and idx >= 2 and tokens[idx - 2].get("pos") == "Verb"
        )
        if connective_te_de and previous.get("pos") == "Particle":
            if token.get("pos") != "Auxiliary":
                token["pos"] = "Auxiliary"
                changed = True
        elif previous.get("pos") == "Verb" and token.get("pos") != "Verb":
            token["pos"] = "Verb"
            changed = True
    return changed


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


def postprocess_short_hiragana_onbin(tokens: list[dict]) -> bool:
    """Normalize a short pure-hiragana 撥音便 immediately before だ/で."""
    changed = False
    for idx in range(len(tokens) - 1):
        token = tokens[idx]
        surface = token.get("surface", "")
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


def postprocess_honorific_request(tokens: list[dict]) -> bool:
    """Resolve honorific continuatives as predicates or nominal search units.

    Some analyzers classify a nominally homographic stem such as ``立ち`` as a
    noun even though ``ください``, ``いたす``, ``いただく``, or their potential
    benefactive supplies a verbal continuation. Productive continuative stems
    recover their dictionary form from conjugation structure without a lexical
    exception table.
    """
    changed = False
    for idx in range(1, len(tokens)):
        prefix = tokens[idx - 1]
        stem = tokens[idx]
        nominal_context = idx + 1 == len(tokens) or tokens[idx + 1].get("pos") == "Particle"
        if (
            prefix.get("pos") == "Prefix"
            and prefix.get("surface") in ("お", "ご")
            and stem.get("pos") == "Verb"
            and regex.search(r"\p{Han}", stem.get("surface", ""))
            and nominal_context
        ):
            stem["pos"] = "Noun"
            stem["lemma"] = stem.get("surface", "")
            changed = True
    for idx in range(1, len(tokens) - 1):
        prefix = tokens[idx - 1]
        stem = tokens[idx]
        continuation = tokens[idx + 1]
        surface = stem.get("surface", "")
        is_direct_honorific_continuation = continuation.get("pos") in ("Verb", "Auxiliary") and continuation.get(
            "lemma"
        ) in (
            "くださる",
            "いたす",
            "いただく",
            "いただける",
            "申し上げる",
        )
        is_honorific_naru = (
            continuation.get("surface") == "に"
            and continuation.get("pos") == "Particle"
            and idx + 2 < len(tokens)
            and tokens[idx + 2].get("pos") in ("Verb", "Auxiliary")
            and tokens[idx + 2].get("lemma") == "なる"
        )
        if (
            prefix.get("pos") == "Prefix"
            and prefix.get("surface") in ("お", "ご")
            and stem.get("pos") in ("Noun", "Suffix")
            and surface
            and (is_direct_honorific_continuation or is_honorific_naru)
        ):
            lemma = base_from_renyokei(surface)
            if lemma is not None:
                stem["pos"] = "Verb"
                stem["lemma"] = lemma
                changed = True
    return changed


def postprocess_honorific_oki_aux(tokens: list[dict]) -> bool:
    """Normalize the preparatory auxiliary in a polite request.

    In a noun + おき + ください request, おき is the renyokei of the
    subsidiary verb おく, not the independent ichidan verb おきる.  The rule
    is structural so other dictionary-form uses of おきる remain untouched.
    """
    changed = False
    for idx in range(1, len(tokens) - 1):
        previous = tokens[idx - 1]
        token = tokens[idx]
        following = tokens[idx + 1]
        if (
            previous.get("pos") == "Noun"
            and token.get("surface") == "おき"
            and token.get("pos") == "Verb"
            and token.get("lemma") == "おきる"
            and following.get("pos") == "Verb"
            and following.get("lemma") == "くださる"
        ):
            token["lemma"] = "おく"
            changed = True
    return changed


@reports_mutation
def postprocess_de_particle(tokens: list[dict]) -> bool:
    """Normalize copular で before a binding particle."""
    binding_surfaces = frozenset({"こそ", "さえ", "すら", "しか"})
    for idx in range(1, len(tokens) - 1):
        token = tokens[idx]
        if token.get("surface") != "で" or token.get("pos") != "Particle":
            continue
        if tokens[idx + 1].get("surface") not in binding_surfaces:
            continue
        if tokens[idx - 1].get("pos") not in ("Noun", "Pronoun", "Adjective"):
            continue
        token["pos"] = "Auxiliary"
        token["lemma"] = "だ"


def postprocess_te_form_contraction(tokens: list[dict]) -> bool:
    """Tag じゃ after an onbin verb as the te-form contraction, like ちゃ.

    ちゃ (= ては) and じゃ (= では) are one paradigm; only the voicing of the
    conjunctive particle differs, selected by the onbin stem in front of it
    (書い+ちゃ, 読ん+じゃ, 泳い+じゃ). MeCab already reads ちゃ as a particle but
    reads じゃ as the copula だ, which would put a copula straight onto a verb
    continuative. The copula reading stays untouched after a nominal (本じゃない).
    """
    changed = False
    onbin_tails = ("ん", "い")
    for idx in range(1, len(tokens)):
        token = tokens[idx]
        if token.get("surface") != "じゃ" or token.get("pos") != "Auxiliary":
            continue
        previous = tokens[idx - 1]
        if previous.get("pos") != "Verb" or not previous.get("surface", "").endswith(onbin_tails):
            continue
        token["pos"] = "Particle"
        token["lemma"] = "じゃ"
        changed = True
    return changed


@reports_mutation
def postprocess_dai_final_particle(tokens: list[dict]) -> bool:
    """Normalize closed sentence-final particles that MeCab labels as nouns."""
    if not tokens:
        return
    token = tokens[-1]
    final_particle_surfaces = frozenset({"だい", "ゲソ", "げそ"})
    if token.get("surface") in final_particle_surfaces and token.get("pos") == "Noun":
        token["pos"] = "Particle"
        token["lemma"] = token["surface"]


@reports_mutation
def postprocess_final_particle_quotative_tte(tokens: list[dict]) -> None:
    """Restore the final particle な before a quotative って.

    かな closes a clause with two final particles, and the quotative って that
    trails it belongs to the reported speech.  The reference analyzer instead
    reads な+っ as the euphonic stem of なる (行こうか+なっ+て) or swallows the か
    as well into 叶う (いい+かなっ+て) -- both of which need a nominal argument
    the position cannot supply.
    """
    # The verb readings need a subject phrase, so only a finite predicate in
    # front of the run identifies the final-particle chain.
    predicate_hosts = ("Adjective", "Auxiliary", "Verb")
    for idx in range(len(tokens) - 1, 0, -1):
        token = tokens[idx]
        if token.get("surface") != "て" or token.get("pos") != "Particle":
            continue
        stem = tokens[idx - 1]
        quotative = {"surface": "って", "pos": "Particle", "lemma": "って"}
        final_na = {"surface": "な", "pos": "Particle", "lemma": "な"}
        final_ka = {"surface": "か", "pos": "Particle", "lemma": "か"}
        if (
            stem.get("surface") == "なっ"
            and idx >= 2
            and tokens[idx - 2].get("surface") == "か"
            and tokens[idx - 2].get("pos") == "Particle"
        ):
            tokens[idx - 1 : idx + 1] = [final_na, quotative]
            continue
        if stem.get("surface") == "かなっ" and idx >= 2 and tokens[idx - 2].get("pos") in predicate_hosts:
            tokens[idx - 1 : idx + 1] = [final_ka, final_na, quotative]


@reports_mutation
def postprocess_tteba_emphatic_particle(tokens: list[dict]) -> None:
    """Keep the emphatic final particle ってば whole and off the predicate.

    って and ば are both closed function words and the pair carries no internal
    inflection boundary, so it is one search unit.  MeCab splits it, and after
    a na-adjective stem it goes further and reads だ+って as the adverbial
    particle だって -- which strands the host as a bare noun and loses the
    copula.  Restore the copula and merge the particle in both readings.
    """
    for idx in range(len(tokens) - 1, 0, -1):
        token = tokens[idx]
        if token.get("surface") != "ば" or token.get("pos") != "Particle":
            continue
        following = tokens[idx + 1] if idx + 1 < len(tokens) else None
        if following is not None and following.get("pos") != "Symbol":
            continue
        previous = tokens[idx - 1]
        head = previous.get("surface", "")
        if previous.get("pos") != "Particle" or head not in ("って", "だって"):
            continue
        emphatic = {"surface": "ってば", "pos": "Particle", "lemma": "ってば"}
        copula = {"surface": "だ", "pos": "Auxiliary", "lemma": "だ"}
        tokens[idx - 1 : idx + 1] = [copula, emphatic] if head == "だって" else [emphatic]


def postprocess_nanka_particle(tokens: list[dict]) -> bool:
    """Normalize the colloquial adverbial particle なんか from MeCab's filler tag."""
    changed = False
    for token in tokens:
        if token.get("surface") == "なんか" and token.get("pos") == "Other":
            token["pos"] = "Particle"
            token["lemma"] = "なんか"
            changed = True
    return changed


def postprocess_kiri_limited_particle(tokens: list[dict]) -> bool:
    """Normalize hiragana きり as the closed limiting particle."""
    changed = False
    for idx, token in enumerate(tokens):
        previous = tokens[idx - 1] if idx > 0 else None
        if token.get("surface") == "きり" and token.get("pos") == "Noun":
            if previous is not None and previous.get("pos") == "Adverb":
                previous["pos"] = "Noun"
            token["pos"] = "Particle"
            token["lemma"] = "きり"
            changed = True
    return changed


def postprocess_kuru_causative(tokens: list[dict]) -> bool:
    """Restore the irregular Kuru lemma in its split causative connection."""
    changed = False
    for idx, token in enumerate(tokens[:-1]):
        following = tokens[idx + 1]
        if (
            token.get("surface") == "来さ"
            and token.get("pos") == "Verb"
            and token.get("lemma") == "来す"
            and following.get("surface", "").startswith("せ")
            and following.get("pos") == "Auxiliary"
        ):
            token["lemma"] = "来る"
            changed = True
    return changed


def postprocess_onaji_predicate(tokens: list[dict]) -> bool:
    """Normalize predicative 同じ across the complete copula paradigm."""
    changed = False
    for idx, token in enumerate(tokens):
        following = tokens[idx + 1] if idx + 1 < len(tokens) else None
        if (
            token.get("surface") == "同じ"
            and token.get("pos") == "Determiner"
            and (following is None or following.get("surface") in COPULA_SURFACES)
        ):
            token["pos"] = "Adjective"
            token["lemma"] = "同じ"
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


def postprocess_dewa_aru_boundary(tokens: list[dict]) -> bool:
    """Split copular で + binding は before lexical ある."""
    for idx in range(len(tokens) - 1):
        token = tokens[idx]
        following = tokens[idx + 1]
        if token.get("surface") != "では" or following.get("pos") != "Verb" or following.get("lemma") != "ある":
            continue
        tokens[idx : idx + 1] = [
            {"surface": "で", "pos": "Auxiliary", "lemma": "だ"},
            {"surface": "は", "pos": "Particle", "lemma": "は"},
        ]
        return True
    return False


def _is_irrealis_before_negative(surface: str) -> bool:
    """Whether a surface is the irrealis stem the negative auxiliary selects.

    書か+なく+ない keeps its verb reading, because the negative attaches to the
    irrealis; 変わり+なく is the continuative that also serves as a deverbal noun.
    The reference dictionary names the difference in the probe's conjugated form.
    """
    from .mecab import mecab_analyze

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
    The polite conjectural copula likewise selects a nominal predicate
    (曇りでしょう), not a bare continuative verb.
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
        predicative_copula = following.get("pos") == "Auxiliary" and following.get("surface") in {"でしょ"}
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


def postprocess_nominal_conjunction_homograph(tokens: list[dict]) -> bool:
    """Read a conjunction homograph as a noun in a nominally selected slot."""
    changed = False
    assertive_copula = {"だ", "だっ"}
    for idx, token in enumerate(tokens):
        if token.get("pos") != "Conjunction":
            continue
        previous = tokens[idx - 1] if idx > 0 else {}
        following = tokens[idx + 1] if idx + 1 < len(tokens) else {}
        selected_by_genitive = previous.get("pos") == "Particle" and previous.get("surface") == "の"
        selected_by_copula = following.get("pos") == "Auxiliary" and following.get("surface") in assertive_copula
        if selected_by_genitive or selected_by_copula:
            token["pos"] = "Noun"
            token["lemma"] = token.get("surface", "")
            changed = True
    return changed


def postprocess_interjection_before_copula(tokens: list[dict]) -> bool:
    """Read an interjection homograph nominally when the copula predicates over it.

    An interjection is a complete utterance on its own, so the plain assertive
    copula cannot predicate over it and its presence identifies the nominal
    reading of a homograph (そらだった is そら the noun, not the exclamation).
    The polite copula is excluded because it also attaches to a fixed formulaic
    interjection as a politeness marker rather than as a predicate
    (すみませんでした, お疲れ様です), and so are で and な, which are equally
    spelled like particles an interjection may precede.
    """
    assertive_copula = ("だ", "だっ")
    changed = False
    for idx in range(len(tokens) - 1):
        token = tokens[idx]
        following = tokens[idx + 1]
        if token.get("pos") != "Interjection" or following.get("pos") != "Auxiliary":
            continue
        if following.get("surface") not in assertive_copula:
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


def postprocess_copula_neg(tokens: list[dict]) -> bool:
    """Normalize copular negative chains and their continuative adjective."""
    changed = False
    for idx in range(1, len(tokens) - 2):
        predicate = tokens[idx - 1]
        copula = tokens[idx]
        topic = tokens[idx + 1]
        negative = tokens[idx + 2]
        if (
            predicate.get("pos") in ("Noun", "Pronoun", "Adjective", "Particle", "Suffix")
            and copula.get("surface") == "で"
            and topic.get("surface") == "は"
            and negative.get("surface") == "ない"
        ):
            copula["pos"] = "Auxiliary"
            copula["lemma"] = "だ"
            negative["pos"] = "Auxiliary"
            negative["lemma"] = "ない"
            changed = True

    for i in range(1, len(tokens)):
        t = tokens[i]
        if t.get("surface") != "なく" or t.get("pos") != "Auxiliary":
            continue
        prev = tokens[i - 1].get("surface", "")
        if prev in ("じゃ", "で"):
            t["pos"] = "Adjective"
            t["lemma"] = "ない"
            changed = True
    return changed


@reports_mutation
def postprocess_de_aru(tokens: list[dict]) -> bool:
    """Fix copula で+ある/あり/あっ pattern based on context."""
    for i in range(len(tokens)):
        t = tokens[i]
        if t.get("surface") != "で":
            continue
        if i >= len(tokens) - 1:
            continue

        nxt = tokens[i + 1]
        nxt_surface = nxt.get("surface", "")

        if nxt_surface not in ("ある", "あり", "あれ", "あっ"):
            continue

        prev_pos = tokens[i - 1].get("pos", "") if i > 0 else ""
        is_past = nxt_surface == "あっ"

        if is_past and prev_pos in ("Noun", "Pronoun", "Suffix"):
            # N+であった: で→Particle, あっ→Verb
            t["pos"] = "Particle"
            t["lemma"] = "で"
            if nxt.get("pos") == "Auxiliary":
                nxt["pos"] = "Verb"
                nxt["lemma"] = "ある"
        elif is_past:
            # Na-adj+であった or other: keep as-is (copula chain)
            pass
        else:
            # Present/continuous forms: で→Auxiliary(だ), ある→Verb
            t["pos"] = "Auxiliary"
            t["lemma"] = "だ"
            if nxt.get("pos") == "Auxiliary":
                nxt["pos"] = "Verb"
                nxt["lemma"] = "ある"


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
def postprocess_classical_conjecture_aux(tokens: list[dict]) -> bool:
    """Treat classical けむ/らむ after a predicate as auxiliaries.

    The analyzer knows neither auxiliary. After a verb it at least keeps the two
    morae together; after a nominal predicate it splits them into a plural
    suffix plus an unknown noun (確認+ら+む), which is rejoined here.
    """
    for idx in range(len(tokens) - 1, 0, -1):
        token = tokens[idx]
        if token.get("surface") in ("けむ", "らむ") and tokens[idx - 1].get("pos") == "Verb":
            token["pos"] = "Auxiliary"
            token["lemma"] = token["surface"]
            continue
        if (
            idx >= 2
            and (tokens[idx - 1].get("surface"), token.get("surface")) in (("け", "む"), ("ら", "む"))
            and tokens[idx - 2].get("pos") in ("Noun", "Verb")
        ):
            merged = tokens[idx - 1].get("surface", "") + "む"
            tokens[idx - 1 : idx + 1] = [{"surface": merged, "pos": "Auxiliary", "lemma": merged}]


_KARI_RENYOKEI_CELL = "かり"
_KARI_HOST_POS = ("Adjective", "Auxiliary")


def postprocess_classical_kere_aux(tokens: list[dict]) -> bool:
    """Normalize the classical past 已然形 けれ behind its host.

    The reference analyzer reads the terminal of this paradigm as the auxiliary
    it is (花咲き+けり) and then reads its 已然形, in the very same position, as
    the 五段 verb 蹴る (信念あり+けれ).  One paradigm does not change word class
    cell by cell, and a continuative cannot be followed by another verb's own
    realis, so the reading the terminal already gets covers this cell too.

    The conditional ば settles the cell on its own, without the continuative
    probe: nothing else spells けれ in front of it.

    An adjective and an adjective-like auxiliary reach the same auxiliary through
    their supplementary continuative かり (心安かり+けり), and the reference
    analyzer splits that pair the same way — auxiliary for the terminal, the verb
    蹴る for the realis. That host counts too.
    """
    changed = False
    for idx in range(1, len(tokens)):
        token = tokens[idx]
        if token.get("surface") != "けれ":
            continue
        previous = tokens[idx - 1]
        previous_surface = previous.get("surface", "")
        is_verb_host = previous.get("pos") == "Verb"
        hosts_the_past = (is_verb_host and _spells_verb_continuative(previous_surface)) or (
            previous.get("pos") in _KARI_HOST_POS and previous_surface.endswith(_KARI_RENYOKEI_CELL)
        )
        follows_conditional = is_verb_host and idx + 1 < len(tokens) and tokens[idx + 1].get("surface") == "ば"
        if not hosts_the_past and not follows_conditional:
            continue
        token["pos"] = "Auxiliary"
        token["lemma"] = "けり"
        changed = True
    return changed


@reports_mutation
def postprocess_classical_ha_row_past(tokens: list[dict]) -> bool:
    """Restore a kana ha-row continuative before the classical past けり.

    The reference analyzer can split a historical ha-row stem as a case
    particle plus a one-mora modern verb and then attach the past adnominal
    ける to its final kana.  The three-token sequence has no grammatical
    boundary: the reconstructed continuative ends in ひ and selects けり.
    """
    for idx in range(2, len(tokens)):
        particle, stem, tail = tokens[idx - 2 : idx + 1]
        if (
            particle.get("pos") != "Particle"
            or stem.get("pos") != "Verb"
            or tail.get("surface") != "ひける"
            or tail.get("pos") != "Verb"
        ):
            continue
        surface = f"{particle.get('surface', '')}{stem.get('surface', '')}ひ"
        if len(surface) < 3:
            continue
        tokens[idx - 2 : idx + 1] = [
            {"surface": surface, "pos": "Verb", "lemma": f"{surface[:-1]}ふ"},
            {"surface": "ける", "pos": "Auxiliary", "lemma": "けり"},
        ]
        return True
    return False


@reports_mutation
def postprocess_classical_b_row_moteiku(tokens: list[dict]) -> bool:
    """Restore a B-row continuative in the classical 〜もて行けば chain.

    The reference analyzer may leave the B-row kana on the following compound
    and classify the whole tail as a noun.  The following もて+いけ+ば sequence
    selects a verbal compound, so the kana completes the preceding verb's
    continuative and the remaining cells have unambiguous grammatical roles.
    """
    terminal_by_continuative = {"び": "ぶ", "み": "む", "り": "る", "ち": "つ", "し": "す"}
    for idx in range(len(tokens) - 1):
        head, tail = tokens[idx : idx + 2]
        if head.get("pos") != "Verb" or tail.get("pos") not in ("Noun", "Other"):
            continue
        surface = tail.get("surface", "")
        if len(surface) != 6 or not surface.endswith("もていけば"):
            continue
        continuative = surface[0]
        terminal = terminal_by_continuative.get(continuative)
        if terminal is None:
            continue
        stem = f"{head.get('surface', '')}{continuative}"
        if len(stem) < 2:
            continue
        head["surface"] = stem
        head["lemma"] = f"{stem[:-1]}{terminal}"
        tokens[idx + 1 : idx + 2] = [
            {"surface": "もて", "pos": "Verb", "lemma": "もつ"},
            {"surface": "いけ", "pos": "Verb", "lemma": "いく"},
            {"surface": "ば", "pos": "Particle", "lemma": "ば"},
        ]
        return True
    return False


@reports_mutation
def postprocess_classical_ramu_boundary(tokens: list[dict]) -> bool:
    """Repair MeCab's one-kanji godan-ka plus らむ boundary."""
    for idx, token in enumerate(tokens):
        if idx == 0 or token.get("surface") != "くらむ" or token.get("pos") != "Verb":
            continue
        previous = tokens[idx - 1]
        stem = previous.get("surface", "")
        if previous.get("pos") != "Noun" or len(stem) != 1:
            continue
        previous["surface"] = f"{stem}く"
        previous["pos"] = "Verb"
        previous["lemma"] = f"{stem}く"
        token["surface"] = "らむ"
        token["pos"] = "Auxiliary"
        token["lemma"] = "らむ"


def postprocess_classical_desiderative_aux(tokens: list[dict]) -> bool:
    """Normalize the split classical desiderative ま + ほし chain.

    ほし is the terminal cell of the same adjective the attributive ほしき
    spells, and the analyzer splits both the same way.
    """
    changed = False
    for idx, token in enumerate(tokens[:-1]):
        if token.get("surface") != "ま" or tokens[idx + 1].get("surface") not in ("ほし", "ほしき"):
            continue
        token["pos"] = "Auxiliary"
        token["lemma"] = "まほし"
        changed = True
    return changed


def postprocess_classical_honorific_aux(tokens: list[dict]) -> bool:
    """Normalize the split classical honorific auxiliary た + ま + ふ."""
    changed = False
    for idx in range(len(tokens) - 2):
        first, second, third = tokens[idx : idx + 3]
        if (first.get("surface"), second.get("surface"), third.get("surface")) != ("た", "ま", "ふ"):
            continue
        for token in (second, third):
            token["pos"] = "Auxiliary"
            token["lemma"] = "たまふ"
        changed = True
    return changed


def postprocess_classical_perfect_aux(tokens: list[dict]) -> bool:
    """Normalize たり's terminal/adnominal cells and 已然形+り."""
    changed = False
    for idx, token in enumerate(tokens):
        if idx == 0:
            continue
        previous = tokens[idx - 1]
        surface = token.get("surface")
        is_terminal_perfect = surface == "たり" and (idx == len(tokens) - 1 or tokens[idx + 1].get("surface") == "けり")
        is_adnominal_perfect = surface == "たる" and token.get("pos") == "Auxiliary"
        if is_terminal_perfect or is_adnominal_perfect:
            if previous.get("pos") == "Noun":
                lemma = base_from_renyokei(previous.get("surface", ""))
                if lemma is not None:
                    previous["pos"] = "Verb"
                    previous["lemma"] = lemma
                    changed = True
            if previous.get("pos") == "Verb":
                if token.get("pos") != "Auxiliary" or token.get("lemma") != "たり":
                    token["pos"] = "Auxiliary"
                    token["lemma"] = "たり"
                    changed = True
        if surface == "り" and previous.get("pos") == "Verb":
            previous_surface = previous.get("surface", "")
            if previous_surface.endswith("け"):
                previous["lemma"] = f"{previous_surface[:-1]}く"
                token["pos"] = "Auxiliary"
                token["lemma"] = "り"
                changed = True
    return changed


@reports_mutation
def postprocess_classical_past_keri(tokens: list[dict]) -> None:
    """Restore the classical past けり after the perfective continuative に.

    に+けり closes a predicate with the perfective ぬ and the past けり.  The
    reference dictionary has no けり cell for that position, so an unpunctuated
    clause falls back to the homographic 蹴る -- which turns the preceding
    continuative into its object and leaves the clause without a tense.  The
    nominal reading of けり keeps its own 名詞 tag, so the verb tag alone
    identifies the fallback.
    """
    for idx in range(2, len(tokens)):
        token = tokens[idx]
        previous = tokens[idx - 1]
        if (
            token.get("surface") != "けり"
            or token.get("pos") != "Verb"
            or previous.get("surface") != "に"
            or previous.get("pos") != "Particle"
            or tokens[idx - 2].get("pos") not in ("Noun", "Verb", "Adjective")
        ):
            continue
        token["pos"] = "Auxiliary"
        token["lemma"] = "けり"


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
def postprocess_classical_past_shi(tokens: list[dict]) -> bool:
    """Retag the adnominal し between a continuative and a nominal as 過去の助動詞 き.

    The reference analyzer reads every bare し as the する continuative, but する
    attaches to a verbal noun, never to another verb's continuative. Standing
    between a verb and the nominal it modifies, the mora is the 連体形 of the
    classical past き (摘みし人, 見しこと).

    The modified nominal is promoted out of the suffix class for the same reason:
    an adnominal takes a head noun, so 人 there is the head rather than the bound
    counter it is elsewhere (三人).

    The 連体形 also nominalizes instead of modifying, and the nominal it forms
    fills an argument slot, so a case particle marks it (読みしに, 摘みしを). The
    reference analyzer reads that position as the する continuative too, which
    the same argument rules out.
    """
    for idx, token in enumerate(tokens):
        if idx == 0 or idx + 1 >= len(tokens):
            continue
        if token.get("surface") != "し" or token.get("pos") != "Verb":
            continue
        following = tokens[idx + 1]
        if tokens[idx - 1].get("pos") != "Verb":
            continue
        modifies_nominal = following.get("pos") in ("Noun", "Suffix")
        if not modifies_nominal and following.get("pos_sub1") != "格助詞":
            continue
        token["pos"] = "Auxiliary"
        token["lemma"] = "き"
        if modifies_nominal:
            following["pos"] = "Noun"


_KU_TERMINAL_CELL = "けし"


def _na_adjective_base_of_ku_terminal(surface: str) -> str | None:
    """Recover the modern base of a ク活用 terminal whose reflex is a na-adjective.

    The reference dictionary carries this paradigm wherever the modern base is an
    i-adjective, and reads its terminal as one (深し → 形容詞, 文語基本形, 深い).
    Where the reflex is a na-adjective the terminal is missing and comes back as
    an unknown noun instead, although the word itself is perfectly ordinary: the
    classical stem is the modern base with け in place of its final か. Probing
    that spelling settles the word class along with the lemma.
    """
    from .mecab import mecab_analyze

    if not surface.endswith(_KU_TERMINAL_CELL) or len(surface) <= len(_KU_TERMINAL_CELL):
        return None
    base = surface[: -len(_KU_TERMINAL_CELL)] + "か"
    tokens = mecab_analyze(base)
    if len(tokens) != 1 or tokens[0].get("surface") != base:
        return None
    if tokens[0].get("pos") != "名詞" or tokens[0].get("pos_sub1") != "形容動詞語幹":
        return None
    return base


@reports_mutation
def postprocess_classical_ku_terminal(tokens: list[dict]) -> bool:
    """Read a ク活用 terminal as the adjective it is rather than an unknown noun."""
    for token in tokens:
        if token.get("pos") != "Noun" or token.get("lemma") != token.get("surface"):
            continue
        base = _na_adjective_base_of_ku_terminal(token.get("surface", ""))
        if base is None:
            continue
        token["pos"] = "Adjective"
        token["lemma"] = base


_PERFECT_NU_CELLS = ("ぬる", "ぬれ")


@reports_mutation
def postprocess_classical_perfect_nu(tokens: list[dict]) -> bool:
    """Retag ぬる / ぬれ after a verb continuative as the classical perfect ぬ.

    The reference analyzer reads the terminal of this paradigm as the auxiliary
    it is (花散り+ぬ) and then, behind the very same continuative, reads the other
    two cells as unrelated lexical verbs — 五段 塗る for the adnominal and 一段
    濡れる for the realis. One paradigm does not change word class cell by cell,
    and a continuative cannot be followed by another verb's own terminal, so the
    reading the terminal already gets covers all three.
    """
    for idx, token in enumerate(tokens):
        if idx == 0 or token.get("pos") != "Verb" or token.get("surface") not in _PERFECT_NU_CELLS:
            continue
        previous = tokens[idx - 1]
        if previous.get("pos") != "Verb" or not _spells_verb_continuative(previous.get("surface", "")):
            continue
        token["pos"] = "Auxiliary"
        token["lemma"] = "ぬ"


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


@reports_mutation
def postprocess_itadakeru_aux(tokens: list[dict]) -> bool:
    """Treat potential いただける as a humble subsidiary verb after a predicate."""
    for idx, token in enumerate(tokens):
        if (
            token.get("lemma") != "いただける"
            or not token.get("surface", "").startswith("いただけ")
            or token.get("pos") != "Verb"
            or idx == 0
        ):
            continue
        previous = tokens[idx - 1]
        follows_te_form = previous.get("pos") == "Particle" and previous.get("surface") in {"て", "で"}
        follows_predicate = previous.get("pos") == "Verb"
        follows_honorific_nominal = (
            previous.get("pos") == "Noun"
            and idx > 1
            and tokens[idx - 2].get("pos") == "Prefix"
            and tokens[idx - 2].get("surface") in {"お", "ご", "御"}
        )
        if follows_te_form or follows_predicate or follows_honorific_nominal:
            token["pos"] = "Auxiliary"


@reports_mutation
def postprocess_monono_conjunction(tokens: list[dict]) -> bool:
    """Normalize concessive ものの as a closed connective particle."""
    for idx, token in enumerate(tokens):
        if token.get("surface") == "ものの" and idx > 0:
            if tokens[idx - 1].get("pos") in ("Verb", "Auxiliary", "Adjective"):
                token["pos"] = "Particle"
                token["lemma"] = "ものの"


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
    """Classify the productive nominalizing み on a verb stem as a suffix.

    The subsidiary verb みる selects a te-form and nothing else, so a み that
    follows a bare continuative cannot be one. It is the same nominalizer that
    already comes back tagged Suffix on an adjective stem (しんど + み), and
    leaving it as the subsidiary makes the token host a case particle no
    predicate could take (分かり + み + が).
    """
    changed = False
    for idx in range(1, len(tokens)):
        token = tokens[idx]
        previous = tokens[idx - 1]
        if (
            token.get("surface") != "み"
            or token.get("lemma") != "みる"
            or previous.get("pos") != "Verb"
            or previous.get("surface", "").endswith(("て", "で"))
        ):
            continue
        token["pos"] = "Suffix"
        token["lemma"] = "み"
        changed = True
    return changed


def postprocess_mu_verb_desiderative(tokens: list[dict]) -> bool:
    """Restore the continuative boundary in a む verb + たい read as a stem + みたい.

    The similative みたい selects a nominal or a terminal form, never an adjective
    stem, so `Adj stem + みたい` is not a possible analysis. It comes from the
    lexical みたい entry outscoring the continuative of the paired む verb on a
    short input. The verb has to be attested to keep the rule from inventing a
    lemma for every 〜しい adjective.
    """
    lexical_verbs = core_headwords("verbs.tsv")
    changed = False
    for idx in range(len(tokens) - 1):
        token = tokens[idx]
        following = tokens[idx + 1]
        lemma = token.get("lemma", "")
        surface = token.get("surface", "")
        if (
            token.get("pos") != "Adjective"
            or not lemma.endswith("しい")
            or surface != lemma[:-1]
            or following.get("surface") != "みたい"
            or following.get("pos") != "Auxiliary"
        ):
            continue
        verb_lemma = surface + "む"
        if verb_lemma not in lexical_verbs:
            continue
        tokens[idx : idx + 2] = [
            {"surface": surface + "み", "pos": "Verb", "lemma": verb_lemma},
            {"surface": "たい", "pos": "Auxiliary", "lemma": "たい"},
        ]
        changed = True
    return changed


def postprocess_shortened_causative_passive(tokens: list[dict]) -> bool:
    """Classify the bound さ in a Godan shortened causative-passive chain."""
    changed = False
    a_row_endings = frozenset("あかがさざただなはばぱまらわ")
    for idx in range(1, len(tokens) - 1):
        previous = tokens[idx - 1]
        token = tokens[idx]
        following = tokens[idx + 1]
        previous_surface = previous.get("surface", "")
        if (
            token.get("surface") != "さ"
            or token.get("pos") != "Verb"
            or token.get("lemma") != "する"
            or previous.get("pos") != "Verb"
            or not previous_surface
            or previous_surface[-1] not in a_row_endings
            or following.get("pos") != "Auxiliary"
            or not following.get("surface", "").startswith("れ")
        ):
            continue
        token["pos"] = "Auxiliary"
        token["lemma"] = "す"
        changed = True
    return changed


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


def postprocess_productive_search_unit_boundaries(tokens: list[dict]) -> bool:
    """Align productive boundaries without enumerating open-class hosts.

    Every branch is licensed by a closed follower class or an inflectional
    shape.  The function therefore generalizes across arbitrary noun and verb
    hosts while retaining Suzume's search-unit compounds.
    """
    changed = False
    idx = 0
    while idx < len(tokens):
        token = tokens[idx]
        surface = token.get("surface", "")

        if idx + 1 < len(tokens) and surface == "ん" and tokens[idx + 1].get("surface") == "かっ":
            tokens[idx : idx + 2] = [{"surface": "んかっ", "pos": "Auxiliary", "lemma": "ない"}]
            changed = True
            continue

        if idx + 1 < len(tokens) and surface == "づく" and tokens[idx + 1].get("surface") == "め":
            tokens[idx : idx + 2] = [{"surface": "づくめ", "pos": "Suffix", "lemma": "づくめ"}]
            changed = True
            continue

        if idx + 1 < len(tokens) and regex.fullmatch(r"([あいうえお])\1+", surface):
            following = tokens[idx + 1].get("surface", "")
            if following and set(following) == {surface[0]}:
                token["surface"] = surface + following
                token["lemma"] = token["surface"]
                token["pos"] = "Adverb"
                del tokens[idx + 1]
                changed = True
                continue

        if idx + 1 < len(tokens) and surface == "うす" and tokens[idx + 1].get("pos") == "Adjective":
            following = tokens[idx + 1]
            combined = surface + following.get("surface", "")
            tokens[idx : idx + 2] = [{"surface": combined, "pos": "Adjective", "lemma": combined}]
            changed = True
            continue

        # A compound nominal host immediately selected by the closed
        # がましい construction is one search unit (X+V-renyokei + がましい).
        if (
            idx + 3 < len(tokens)
            and tokens[idx + 2].get("surface") == "が"
            and tokens[idx + 3].get("surface") == "ましい"
        ):
            following = tokens[idx + 1]
            if token.get("pos") == "Noun" and following.get("pos") in ("Noun", "Verb"):
                combined = surface + following.get("surface", "")
                tokens[idx : idx + 2] = [{"surface": combined, "pos": "Noun", "lemma": combined}]
                changed = True
                continue

        # Calendar heads bind to the closed 末/翌+counter units while a
        # following deverbal payment stem remains its own search unit.
        if idx + 1 < len(tokens) and tokens[idx + 1].get("surface") == "末締め":
            if surface in COUNTER_UNITS:
                tokens[idx : idx + 2] = [
                    {"surface": surface + "末", "pos": "Noun", "lemma": surface + "末"},
                    {"surface": "締め", "pos": "Noun", "lemma": "締め"},
                ]
                changed = True
                idx += 2
                continue

        if surface == "翌" and idx + 1 < len(tokens):
            following_surface = tokens[idx + 1].get("surface", "")
            unit = next((candidate for candidate in COUNTER_UNITS if following_surface.startswith(candidate)), "")
            remainder = following_surface[len(unit) :]
            if unit and remainder:
                tokens[idx : idx + 2] = [
                    {"surface": surface + unit, "pos": "Noun", "lemma": surface + unit},
                    {"surface": remainder, "pos": "Noun", "lemma": remainder},
                ]
                changed = True
                idx += 2
                continue

        # MeCab can analyze productive V1+合わせる as a causative chain
        # (見合わ+せる, つめあわ+せ).  The internal 合わ/あわ boundary
        # recovers the same closed V2 class without naming V1 hosts.  A bare
        # continuative directly selected by a nominal particle is a deverbal
        # compound noun; finite せる remains a compound verb.
        compound_alignment_stem = next(
            (ending for ending in ("合わ", "あわ") if surface.endswith(ending) and len(surface) > len(ending)),
            None,
        )
        if idx + 2 < len(tokens) and compound_alignment_stem is not None:
            following = tokens[idx + 1]
            nominal_particle = tokens[idx + 2]
            if (
                following.get("surface") == "せ"
                and nominal_particle.get("pos") == "Particle"
                and nominal_particle.get("surface") in {"を", "は", "が", "の", "に", "で", "へ", "と", "も"}
            ):
                combined = surface + "せ"
                tokens[idx : idx + 2] = [{"surface": combined, "pos": "Noun", "lemma": combined}]
                changed = True
                continue

        if idx + 1 < len(tokens) and compound_alignment_stem is not None:
            following = tokens[idx + 1]
            if following.get("surface") == "せる":
                combined = surface + "せる"
                lemma = surface[: -len(compound_alignment_stem)] + "合わせる"
                tokens[idx : idx + 2] = [{"surface": combined, "pos": "Verb", "lemma": lemma}]
                changed = True
                continue

        if idx + 1 < len(tokens) and token.get("pos") in ("Verb", "Noun"):
            following = tokens[idx + 1]
            if following.get("pos") == "Verb":
                v2_base = following.get("lemma", "")
                if v2_base not in _PRODUCTIVE_COMPOUND_V2 and following.get("surface", "").endswith("せる"):
                    potential_base = following.get("surface", "")[:-2] + "す"
                    if potential_base in _PRODUCTIVE_COMPOUND_V2:
                        v2_base = potential_base
                renyokei_base = base_from_renyokei(surface)
                if (
                    v2_base in _PRODUCTIVE_COMPOUND_V2
                    and token.get("pos") == "Verb"
                    and renyokei_base == token.get("lemma")
                ):
                    combined = surface + following.get("surface", "")
                    compound_lemma = combined if following.get("surface", "").endswith("せる") else surface + v2_base
                    tokens[idx : idx + 2] = [{"surface": combined, "pos": "Verb", "lemma": compound_lemma}]
                    changed = True
                    continue

        # MeCab exposes the shortened causative mora on the host token
        # (やらさ+れ); Suzume keeps host+さ+れ as three morphemes.
        if idx + 1 < len(tokens) and token.get("pos") == "Verb" and surface.endswith("さ"):
            following = tokens[idx + 1]
            host = surface[:-1]
            host_lemma = base_from_mizenkei(host)
            is_regular_sa_row = token.get("lemma", "").endswith("す") and token.get("lemma", "")[:-1] == host
            if (
                host_lemma
                and not is_regular_sa_row
                and following.get("pos") == "Auxiliary"
                and following.get("surface", "").startswith("れ")
            ):
                tokens[idx : idx + 1] = [
                    {"surface": host, "pos": "Verb", "lemma": host_lemma},
                    {"surface": "さ", "pos": "Auxiliary", "lemma": "す"},
                ]
                changed = True
                idx += 2
                continue

        # Volitional よう is morphologically the o-row stem + auxiliary う.
        if (
            idx + 1 < len(tokens)
            and (token.get("pos") == "Verb" or (token.get("pos") == "Noun" and token.get("lemma") == "する"))
            and tokens[idx + 1].get("surface") == "よう"
            and tokens[idx + 1].get("pos") == "Auxiliary"
        ):
            token["pos"] = "Verb"
            token["surface"] = surface + "よ"
            tokens[idx + 1] = {"surface": "う", "pos": "Auxiliary", "lemma": "う"}
            changed = True
            idx += 2
            continue

        # Denominal colloquial verbs before progressive ている expose the
        # geminate on the noun in Suzume (過疎っ+て+いる).
        if idx + 2 < len(tokens) and token.get("pos") == "Noun" and regex.search(r"\p{Han}", surface):
            following = tokens[idx + 1]
            progressive = tokens[idx + 2]
            if following.get("surface") == "って" and progressive.get("lemma") == "いる":
                tokens[idx] = {"surface": surface + "っ", "pos": "Verb", "lemma": surface + "る"}
                tokens[idx + 1] = {"surface": "て", "pos": "Particle", "lemma": "て"}
                progressive["pos"] = "Auxiliary"
                changed = True

        if surface == "ましい" and idx > 0 and tokens[idx - 1].get("surface") == "が":
            token["pos"] = "Adjective"
            token["lemma"] = "ましい"
            changed = True

        if (
            idx > 0
            and idx + 1 < len(tokens)
            and surface == "あり"
            and tokens[idx - 1].get("surface") == "でも"
            and tokens[idx + 1].get("pos") == "Auxiliary"
        ):
            token["pos"] = "Noun"
            token["lemma"] = "あり"
            changed = True

        if surface == "他" and idx + 1 < len(tokens) and tokens[idx + 1].get("surface") == "の":
            token["lemma"] = "ほか"
            changed = True

        if surface == "ただ" and idx + 1 < len(tokens) and tokens[idx + 1].get("pos") == "Pronoun":
            token["pos"] = "Adverb"
            token["lemma"] = "ただ"
            changed = True

        if surface == "反し" and idx > 0 and tokens[idx - 1].get("surface") == "に":
            token["lemma"] = "反する"
            changed = True

        honorific_naru = (
            idx + 2 < len(tokens) and tokens[idx + 1].get("surface") == "に" and tokens[idx + 2].get("lemma") == "なる"
        )
        invitation_auxiliary = (
            idx + 1 < len(tokens)
            and tokens[idx + 1].get("surface") == "なんし"
            and tokens[idx + 1].get("lemma") == "ます"
        )
        if surface == "おいで" and (honorific_naru or invitation_auxiliary):
            token["pos"] = "Noun"
            token["lemma"] = "おいで"
            changed = True

        if token.get("pos") == "Adjective" and surface.endswith("く") and idx + 1 < len(tokens):
            following = tokens[idx + 1]
            if following.get("pos") == "Adjective" and not following.get("surface", "").startswith(
                ("ない", "なく", "なかっ", "なけれ")
            ):
                token["pos"] = "Adverb"
                token["lemma"] = surface
                changed = True

        if surface == "どう" and idx + 1 < len(tokens) and tokens[idx + 1].get("surface") == "か":
            token["pos"] = "Adverb"
            token["lemma"] = "どう"
            changed = True

        if surface == "で" and idx > 0:
            previous = tokens[idx - 1]
            if token.get("pos") == "Auxiliary" and (
                previous.get("surface") == "せい" or regex.fullmatch(r"\p{Katakana}+", previous.get("surface", ""))
            ):
                token["pos"] = "Particle"
                token["lemma"] = "で"
                changed = True
            elif token.get("pos") == "Particle" and previous.get("pos") == "Adjective":
                token["pos"] = "Auxiliary"
                token["lemma"] = "だ"
                changed = True

        if surface == "どき" and idx > 0 and tokens[idx - 1].get("pos") == "Noun":
            token["pos"] = "Noun"
            token["lemma"] = "どき"
            changed = True

        if token.get("pos") == "Verb" and surface.endswith("れる") and token.get("lemma") != surface:
            token["lemma"] = surface
            changed = True

        if surface == "行っ" and idx > 0 and tokens[idx - 1].get("surface") in {"に", "へ"}:
            token["lemma"] = "行く"
            changed = True

        if (
            token.get("pos") == "Noun"
            and idx > 0
            and tokens[idx - 1].get("pos") == "Prefix"
            and idx + 1 < len(tokens)
            and tokens[idx + 1].get("surface") == "し"
        ):
            reconstructed = base_from_renyokei(surface)
            if reconstructed is not None:
                token["pos"] = "Verb"
                token["lemma"] = reconstructed
                changed = True

        if idx == len(tokens) - 1 and token.get("pos") == "Adjective" and regex.fullmatch(r"\p{Han}+よ", surface):
            token["pos"] = "Verb"
            token["lemma"] = surface[:-1] + "る"
            changed = True

        if token.get("pos") == "Adjective" and not token.get("lemma", "").endswith("い"):
            if surface.endswith("化"):
                token["pos"] = "Noun"
                token["lemma"] = surface
                changed = True

        idx += 1
    return changed


def postprocess_binding_negative_aux(tokens: list[dict]) -> bool:
    """Keep the closed しか + negative predicate chain in auxiliary POS."""
    changed = False
    for idx in range(1, len(tokens)):
        token = tokens[idx]
        if token.get("surface") not in ("ない", "なく", "なかっ"):
            continue
        if tokens[idx - 1].get("surface") != "しか":
            continue
        if token.get("pos") != "Auxiliary":
            token["pos"] = "Auxiliary"
            token["lemma"] = "ない"
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
def postprocess_sou_aux(tokens: list[dict]) -> bool:
    """Fix そう after Auxiliary (しまい etc.): Adverb -> Auxiliary (様態)."""
    for i in range(1, len(tokens)):
        t = tokens[i]
        if t.get("surface") != "そう" or t.get("pos") != "Adverb":
            continue
        prev_pos = tokens[i - 1].get("pos", "")
        if prev_pos == "Auxiliary":
            t["pos"] = "Auxiliary"


@reports_mutation
def postprocess_n_kuruwa(tokens: list[dict]) -> bool:
    """Normalize closed kuruwa polite auxiliaries."""
    index = 0
    while index + 1 < len(tokens):
        if tokens[index].get("surface") == "なん" and tokens[index + 1].get("surface") == "し":
            tokens[index : index + 2] = [{"surface": "なんし", "pos": "Auxiliary", "lemma": "ます"}]
            continue
        index += 1

    for i in range(1, len(tokens)):
        t = tokens[i]
        if t.get("surface") != "ん" or t.get("pos") != "Particle":
            continue
        prev = tokens[i - 1]
        if prev.get("surface") in ("あり", "あっ"):
            t["pos"] = "Auxiliary"
            t["lemma"] = "ん"


@reports_mutation
def postprocess_nai_context(tokens: list[dict]) -> bool:
    """Correct ない/なく/なかっ POS: Auxiliary → Adjective after particles.

    Suzume treats standalone ない (doesn't exist) as Adjective, not Auxiliary.
    MeCab classifies it as 助動詞 in all contexts, but when ない follows
    particles like が/は/も, it's an existence adjective, not a negative auxiliary.

    Also handles sentence-initial ない and the continuative なく, whose reading
    follows from what it attaches to rather than from what follows it.
    """
    for idx, tok in enumerate(tokens):
        surface = tok.get("surface", "")
        pos = tok.get("pos", "")

        if surface not in ("ない", "なく", "なかっ") or pos != "Auxiliary":
            continue

        # The negative auxiliary attaches to a predicate stem, so its
        # continuative keeps that reading after a verb or a verbal auxiliary
        # whatever follows (飲ま+なく+ちゃ, 食べ+なく+て, れ+なく+なる). The
        # copula is excluded because its negation uses the supplementary
        # adjective (本+で+なく), except when the topical particle separates
        # the two (本+で+は+なく). Everything else follows a nominal or an
        # adjective continuative and takes the adjective (休み+なく, 明るく+なく).
        if surface == "なく" and idx > 0:
            prev_pos = tokens[idx - 1].get("pos", "")
            prev_surface = tokens[idx - 1].get("surface", "")
            is_copular_topic = (
                prev_pos == "Particle"
                and prev_surface == "は"
                and idx >= 2
                and tokens[idx - 2].get("surface") == "で"
                and tokens[idx - 2].get("pos") == "Auxiliary"
            )
            is_predicate_stem = prev_pos == "Verb" or (prev_pos == "Auxiliary" and prev_surface not in COPULA_SURFACES)
            if not (is_predicate_stem or is_copular_topic):
                tok["pos"] = "Adjective"
                tok["lemma"] = "ない"
            continue

        should_fix = False

        if idx == 0:
            # Sentence-initial ない/なく/なかっ → Adjective
            should_fix = True
        else:
            prev_pos = tokens[idx - 1].get("pos", "")
            prev_surface = tokens[idx - 1].get("surface", "")

            # After particle が/は/も → Adjective (existence negation)
            if prev_pos == "Particle" and prev_surface in ("が", "は", "も"):
                is_copular_negative = (
                    prev_surface == "は"
                    and idx >= 2
                    and tokens[idx - 2].get("surface") == "で"
                    and tokens[idx - 2].get("pos") == "Auxiliary"
                )
                should_fix = not is_copular_negative

            # A negative auxiliary cannot attach directly to a noun, nor to a
            # suffix that derives one. In a bare nominal predicate, ない is the
            # independent adjective with an omitted nominative marker (問題ない,
            # 関係ない, 負けっこない).
            elif prev_pos in ("Noun", "Suffix"):
                should_fix = True

        if should_fix:
            tok["pos"] = "Adjective"
            tok["lemma"] = "ない"


@reports_mutation
def postprocess_nara_verb(tokens: list[dict]) -> bool:
    """Normalize なら in negative predicates and the limiting 〜のみならず chain."""
    for i in range(len(tokens) - 1):
        t = tokens[i]
        if t.get("surface") != "なら":
            continue
        prev_surface = tokens[i - 1].get("surface", "") if i > 0 else ""
        if t.get("pos") not in ("Auxiliary", "Particle"):
            if not (prev_surface == "のみ" and tokens[i + 1].get("surface") == "ず"):
                continue
        nxt_surface = tokens[i + 1].get("surface", "")
        if prev_surface == "のみ" and nxt_surface == "ず":
            t["pos"] = "Particle"
            t["lemma"] = "なら"
            continue
        if nxt_surface in ("ない", "なく", "なかっ", "ぬ"):
            t["pos"] = "Verb"
            t["lemma"] = "なる"


@reports_mutation
def postprocess_classical_nari_kateikei(tokens: list[dict]) -> bool:
    """Restore the classical copula lemma in the 已然形 なれ+ば cell."""
    for idx, token in enumerate(tokens[:-1]):
        if token.get("surface") != "なれ" or tokens[idx + 1].get("surface") != "ば":
            continue
        if idx == 0 or tokens[idx - 1].get("pos") not in ("Noun", "Adjective"):
            continue
        token["pos"] = "Auxiliary"
        token["lemma"] = "なり"


#: Classical auxiliaries whose 連体形 ends in る (たる, なる, る).  They close a
#: predicate, so nothing can take them as an argument.
_CLASSICAL_ATTRIBUTIVE_AUX_LEMMAS = frozenset({"たり", "なり", "り"})


@reports_mutation
def postprocess_classical_nari_after_attributive(tokens: list[dict]) -> bool:
    """Keep なり the copula where it follows an attributive auxiliary.

    なり spells both the classical copula and the 連用形 of the verb なる, and a
    comma after it tips the reference dictionary to the verb (見つけたる|なり、).
    The verb needs a に/と-marked complement or an adjective continuative, and an
    attributive auxiliary can fill neither, so in that position the copula is the
    only reading — with or without the comma (散りたるなり keeps it already).
    """
    for idx, token in enumerate(tokens):
        if idx == 0 or token.get("surface") != "なり" or token.get("pos") != "Verb":
            continue
        previous = tokens[idx - 1]
        if (
            previous.get("pos") != "Auxiliary"
            or not previous.get("surface", "").endswith("る")
            or previous.get("lemma") not in _CLASSICAL_ATTRIBUTIVE_AUX_LEMMAS
        ):
            continue
        token["pos"] = "Auxiliary"
        token["lemma"] = "なり"


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


def postprocess_quotative_determiner_spelling(tokens: list[dict]) -> bool:
    """Move the boundary of なんと+いう onto the pronoun plus quotative determiner.

    The reference dictionary reads 何という as the interrogative pronoun plus the
    quotative 連体詞, but reads its kana spelling なんという as the exclamatory
    adverb なんと plus the verb いう. The construction is the same one; only the
    script differs, so the kana spelling inherits the kanji spelling's boundary.

    Only the uninflected いう directly before a noun qualifies. An inflected form
    is the genuine adverb-plus-verb reading (なんといっても), and so is いう before
    anything other than a noun (なんというか).
    """
    changed = False
    idx = 0
    while idx + 2 < len(tokens):
        adverb, verb, head = tokens[idx], tokens[idx + 1], tokens[idx + 2]
        if adverb.get("surface") != "なんと" or verb.get("surface") != "いう" or head.get("pos") != "Noun":
            idx += 1
            continue
        adverb["surface"] = "なん"
        adverb["pos"] = "Pronoun"
        adverb["lemma"] = "なん"
        verb["surface"] = "という"
        verb["pos"] = "Determiner"
        verb["lemma"] = "という"
        changed = True
        idx += 2
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


# The invocation order is semantic: postprocessors mutate the same token list
# and the first mutation supplies the public applied-rule label.  Keep that
# order next to the implementations rather than duplicating it as imports and
# one-off calls in suzume_utils.
POSTPROCESSORS: tuple[tuple[str, Callable[[list[dict]], bool]], ...] = (
    ("sou-context", postprocess_sou),
    ("ikaga-adverb", postprocess_ikaga),
    ("tada-context", postprocess_tada),
    ("demo-particle", postprocess_demo),
    ("hiragana-yaka-adverbial", postprocess_hiragana_yaka_adverbial),
    ("closed-function-word-pos", postprocess_closed_function_words),
    ("closed-subsidiary-aux", postprocess_closed_subsidiary_aux),
    ("classical-focus-namu", postprocess_classical_focus_namu),
    ("classical-copula-nari", postprocess_classical_copula_nari),
    ("classical-past-izenkei-shika", postprocess_classical_past_izenkei_shika),
    ("honorific-i-adjective", postprocess_honorific_i_adjective),
    ("i-adjective-upper-bound", postprocess_i_adjective_upper_bound),
    ("kadouka-adverb", postprocess_kadouka_adverb),
    ("ii-adjective", postprocess_ii),
    ("iru-aux", postprocess_iru_aux),
    ("giving-receiving-aux", postprocess_giving_aux),
    ("contracted-progressive-aux", postprocess_contracted_progressive_aux),
    ("itadakeru-aux", postprocess_itadakeru_aux),
    ("miru-aux", postprocess_miru_aux),
    ("monono-conjunction", postprocess_monono_conjunction),
    ("formal-noun-lemma", postprocess_formal_noun_lemma),
    ("adjective-nominalizer", postprocess_adjective_nominalizer),
    ("verbal-nominalizer-mi", postprocess_verbal_nominalizer_mi),
    ("mu-verb-desiderative", postprocess_mu_verb_desiderative),
    ("shortened-causative-passive", postprocess_shortened_causative_passive),
    ("modifier-godan-imperative", postprocess_modifier_godan_imperative),
    ("contracted-shimau-aux", postprocess_shimau_aux),
    ("quantity-bound-suffix", postprocess_quantity_bound_suffix),
    ("exclusion-suffix", postprocess_exclusion_suffix),
    ("state-suffix", postprocess_state_suffix),
    ("productive-verb-suffix-stem", postprocess_productive_verb_suffix_stem),
    ("teki-na-adjective", postprocess_teki_na_adjective),
    ("difficulty-adjective-stem", postprocess_difficulty_adjective_stem),
    ("renyokei-compound-particle", postprocess_renyokei_compound_particle),
    ("compound-case-particle-aru", postprocess_compound_case_particle_aru),
    ("to-areba-conditional", postprocess_to_areba_conditional),
    ("tagaru-search-unit", postprocess_tagaru_aux),
    ("l2-noun-context", postprocess_l2_noun_context),
    ("adjective-garu-pos", postprocess_adjective_garu),
    ("fuu-formal-noun", postprocess_fuu_formal_noun),
    ("indefinite-ka", postprocess_indefinite_ka),
    ("subsidiary-yuku", postprocess_subsidiary_yuku),
    ("hiragana-purpose-noun", postprocess_hiragana_purpose_noun),
    ("short-hiragana-onbin", postprocess_short_hiragana_onbin),
    ("hiragana-godan-wa-terminal", postprocess_hiragana_godan_wa_terminal),
    ("honorific-request-renyokei", postprocess_honorific_request),
    ("honorific-oki-aux", postprocess_honorific_oki_aux),
    ("de-particle", postprocess_de_particle),
    ("te-form-contraction-particle", postprocess_te_form_contraction),
    ("dai-final-particle", postprocess_dai_final_particle),
    ("tteba-emphatic-particle", postprocess_tteba_emphatic_particle),
    ("final-particle-quotative-tte", postprocess_final_particle_quotative_tte),
    ("chigai-negative-adjective", postprocess_chigai_negative_adjective),
    ("nanka-colloquial-particle", postprocess_nanka_particle),
    ("kiri-limiting-particle", postprocess_kiri_limited_particle),
    ("kuru-causative-lemma", postprocess_kuru_causative),
    ("onaji-predicative-na-adjective", postprocess_onaji_predicate),
    ("de-aru", postprocess_de_aru),
    ("dewa-aru-boundary", postprocess_dewa_aru_boundary),
    ("ka-suru-noun", postprocess_ka_suru_noun),
    ("taihen-context", postprocess_taihen),
    ("na-adjective-noun-use", postprocess_na_adj_noun),
    ("deverbal-noun-context", postprocess_deverbal_noun_context),
    ("attributive-mamonaku", postprocess_attributive_mamonaku),
    ("adverb-nominal-context", postprocess_adverb_nominal_context),
    ("nominal-conjunction-homograph", postprocess_nominal_conjunction_homograph),
    ("interjection-before-copula", postprocess_interjection_before_copula),
    ("temporal-nao-adverb", postprocess_temporal_nao),
    ("tsuke-noun", postprocess_tsuke_noun),
    ("copular-negative-pos", postprocess_copula_neg),
    ("you-noun", postprocess_you_noun),
    ("classical-ramu-boundary", postprocess_classical_ramu_boundary),
    ("classical-desiderative-aux", postprocess_classical_desiderative_aux),
    ("classical-honorific-aux", postprocess_classical_honorific_aux),
    ("classical-conjecture-aux", postprocess_classical_conjecture_aux),
    ("classical-kere-aux", postprocess_classical_kere_aux),
    ("classical-ha-row-past", postprocess_classical_ha_row_past),
    ("classical-b-row-moteiku", postprocess_classical_b_row_moteiku),
    ("classical-perfect-aux", postprocess_classical_perfect_aux),
    ("classical-past-keri", postprocess_classical_past_keri),
    ("classical-past-shi", postprocess_classical_past_shi),
    ("classical-perfect-nu", postprocess_classical_perfect_nu),
    ("classical-ku-terminal", postprocess_classical_ku_terminal),
    ("adverbial-temporal-prefix", postprocess_adverbial_temporal_prefix),
    ("prolonged-sound-noun", postprocess_prolonged_sound_noun),
    ("yoshi-formal-noun", postprocess_yoshi_formal_noun),
    ("sou-aux", postprocess_sou_aux),
    ("nara-verb", postprocess_nara_verb),
    ("classical-nari-kateikei", postprocess_classical_nari_kateikei),
    ("classical-nari-after-attributive", postprocess_classical_nari_after_attributive),
    ("n-kuruwa", postprocess_n_kuruwa),
    ("nai-context", postprocess_nai_context),
    ("binding-negative-aux", postprocess_binding_negative_aux),
    ("productive-search-unit-boundaries", postprocess_productive_search_unit_boundaries),
    ("bound-derived-adjective", postprocess_bound_derived_adjective),
    ("quotative-determiner-spelling", postprocess_quotative_determiner_spelling),
    ("adverbial-na-adjective", postprocess_adverbial_na_adjective),
)


def postprocessor_rules() -> tuple[tuple[str, Callable[[list[dict]], bool]], ...]:
    """Return the validated, ordered context-dependent postprocessors."""
    labels = [label for label, _ in POSTPROCESSORS]
    if len(labels) != len(set(labels)):
        raise RuntimeError("duplicate postprocessor rule label")

    registered = {processor for _, processor in POSTPROCESSORS}
    defined = {
        processor
        for name, processor in globals().items()
        if name.startswith("postprocess_") and name != "postprocess_mecab_tokens" and callable(processor)
    }
    if missing := defined - registered:
        names = ", ".join(sorted(processor.__name__ for processor in missing))
        raise RuntimeError(f"unregistered postprocessor: {names}")
    if extra := registered - defined:
        names = ", ".join(sorted(processor.__name__ for processor in extra))
        raise RuntimeError(f"unknown postprocessor: {names}")
    return POSTPROCESSORS
