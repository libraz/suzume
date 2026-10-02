"""Spelling substitutions, restoration, and repairs around MeCab analysis."""

from itertools import pairwise

import regex

from .constants import (
    KYUJITAI_TO_SHINJITAI,
    SLANG_ADJ_STEMS,
    SLANG_VERB_STEMS,
    UNUSUAL_NAMES,
    WORD_EXCEPTION_BLOCKED_FOLLOWERS,
    WORD_EXCEPTIONS,
)
from .mecab import mecab_analyze
from .postprocessor_common import _raw_analysis


def _is_emphatic_spelling(original: str, standard: str, host_pos: str = "") -> bool:
    """Whether `original` is `standard` written with emphatic lengthening or repetition.

    Scoped to the marks that spell emphasis rather than a morpheme: the prolonged
    sound mark, and the utterance-final sokuon, which is dropped outright instead
    of being respelled. Removing them and collapsing runs of the same character
    reduces かわいーー and すごーーい to their dictionary forms, while a genuine
    lexical substitution (にゃー → ねえ) stays distinct. Vowel repetition without a mark
    (すごいいいい) is left alone — the tokenizer does not yet reduce it either.
    """
    if original == standard:
        return False
    if original[:-1] == standard and original[-1] in _EMPHATIC_SOKUON_MARKS:
        # An auxiliary closed on a glottal stop is a colloquial form in its own
        # right and keeps that spelling as its lemma (ですっ, ますっ). Holding a
        # vowel adds nothing to the word, so that case stays normalized.
        return host_pos != "助動詞"
    if "ー" not in original:
        return False
    reduced = regex.sub(r"(.)\1+", r"\1", regex.sub(r"ー+", "", original))
    return reduced == regex.sub(r"(.)\1+", r"\1", standard)


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


# The kana an i-adjective inflects into, and a stem the dictionary does know so
# the paradigm can be read off the substituted text.
_ADJECTIVE_INFLECTION_KANA = frozenset("いかくけさ")
SLANG_ADJ_SUBSTITUTE = "赤"


_EMPHATIC_SOKUON_MARKS = frozenset("っッ")


def _emphatic_final_sokuon(text: str) -> int | None:
    """Where a sokuon closes the utterance, with a host in front of it to carry."""
    body = regex.sub(r"[\p{P}\p{S}\p{Z}]+$", "", text)
    if len(body) < 2 or body[-1] not in _EMPHATIC_SOKUON_MARKS:
        return None
    return len(body) - 1 if body[-2] not in _EMPHATIC_SOKUON_MARKS else None


def _invents_a_word_for(raw: tuple[int, dict[int, dict]], position: int) -> bool:
    """Whether the token covering a position is read as a form of another word.

    An auxiliary is excluded: its own onbin cell is spelled with a sokuon (だっ,
    たかっ) and is a dictionary entry in its own right, so the mark there is a
    form the paradigm accounts for rather than one the reading had to invent.
    """
    starts = [start for start in raw[1] if start <= position]
    if not starts:
        return False
    token = raw[1][max(starts)]
    if token.get("pos") == "助動詞":
        return False
    lemma = token.get("lemma", "")
    return bool(lemma) and lemma != "*" and lemma != token.get("surface")


def _stranded_adjective_stems(raw: tuple[int, dict[int, dict]]) -> dict[int, str]:
    """Two-mora kana fragments that head an adjective the dictionary lacks.

    Without an entry for the stem the analysis has to place the inflection kana
    somewhere, and it invents a word for it — a bare い read as the continuative
    of いる, a かっ read as a verb nobody uses, a く given a headword くい. The
    invention is at most two morae and opens on an inflection kana, which a real
    word after a kana noun (バリ + かっこいい) is not. Whether the fragment is
    really an ending is then settled by putting a stem the dictionary does know
    in the nominal's place and seeing whether an adjective comes back.
    """
    ordered = sorted(raw[1].items())
    stems: dict[int, str] = {}
    for position, ((start, token), (_, following)) in enumerate(pairwise(ordered)):
        surface = token.get("surface", "")
        tail = following.get("surface", "")
        after = ordered[position + 2][1].get("surface", "") if position + 2 < len(ordered) else ""
        # A lone kanji the dictionary has no reading for (違 in 違くない) is
        # the same stranded stem written with a character instead of kana.
        # Its く must be the adjective's continuative before ない/て, which a
        # classical terminal (受く) is not.
        unknown_kanji = (
            regex.fullmatch(r"\p{Han}", surface)
            and token.get("lemma") in (None, "*")
            and (tail in ("かっ", "けれ") or (tail == "く" and after.startswith(("な", "て"))))
        )
        if token.get("pos") != "名詞" or not (
            regex.fullmatch(r"[\p{Hiragana}\p{Katakana}]{2}", surface) or unknown_kanji
        ):
            continue
        if not tail or tail[0] not in _ADJECTIVE_INFLECTION_KANA or len(tail) > 2:
            continue
        stems[start] = surface
    return stems


def _reads_as_adjective(text: str, start: int, stem: str, standard: str) -> bool:
    """Whether a known stem put in the fragment's place is read as an adjective."""
    probe = text[:start] + standard + text[start + len(stem) :]
    landed = _raw_analysis(probe)[1].get(start)
    return landed is not None and landed.get("pos") == "形容詞"


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

    # Slang adjectives the list above does not name. The class is open — every
    # season coins another one — so the stems are found in the analysis rather
    # than listed: a two-mora kana fragment the dictionary read as a noun, with
    # an inflection kana stranded behind it that it had to invent a word for.
    if raw is None:
        raw = _raw_analysis(text)
    for start, stem in _stranded_adjective_stems(raw).items():
        if (start, "slang_adj") in replacements:
            continue
        if not _reads_as_adjective(text, start, stem, SLANG_ADJ_SUBSTITUTE):
            continue
        replacements[(start, "slang_adj")] = {
            "original": stem,
            "replacement": SLANG_ADJ_SUBSTITUTE,
            "length": len(stem),
        }

    # Katakana slang verb stems (ディス+り, ミス+り) form an open class too. A
    # stem the dictionary does not know comes back broken inside the katakana
    # run; substituting a known godan stem must read as a verb and reunite the
    # fragments, which a katakana noun before ら/る (テストらしい) does not need.
    for m in regex.finditer(r"(?<![\p{Katakana}ー])[\p{Katakana}ー]{2,}(?=[らりるれろっ])", text):
        start, stem = m.start(), m.group()
        if (start, "slang_verb") in replacements:
            continue
        head = raw[1].get(start)
        if head is None or len(head["surface"]) >= len(stem):
            continue
        probe_count, probe_index = _raw_analysis(text[:start] + "走" + text[m.end() :])
        landed = probe_index.get(start)
        if landed is None or landed["pos"] != "動詞" or probe_count >= raw[0]:
            continue
        replacements[(start, "slang_verb")] = {"original": stem, "replacement": "走", "length": len(stem)}

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

    # An utterance-final sokuon is emphasis rather than a morpheme: the real
    # sokuon-onbin exists only to carry the past and conjunctive suffixes, and
    # nothing follows this one for it to carry. The dictionary still has to place
    # the kana and invents a word for it — a bare っ read as the adjective's
    # continuative auxiliary, an いっ read as 言う, a てっ read as てる — so the
    # mark is dropped before the analysis and put back into the surface
    # afterwards. A final sokuon the dictionary already reads as a word in its
    # own right (あっ, えっ) has itself for a lemma and is left alone.
    emphatic_sokuon = _emphatic_final_sokuon(text)
    if emphatic_sokuon is not None:
        if raw is None:
            raw = _raw_analysis(text)
        if _invents_a_word_for(raw, emphatic_sokuon):
            replacements[(emphatic_sokuon - 1, "emphatic_sokuon")] = {
                "original": text[emphatic_sokuon - 1 : emphatic_sokuon + 1],
                "replacement": text[emphatic_sokuon - 1],
                "length": 2,
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
            if _is_emphatic_spelling(original, standard, token.get("pos", "")):
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
