"""Spelling substitutions, restoration, and repairs around MeCab analysis."""

from itertools import pairwise

import regex

from .constants import (
    CLOSED_HONORIFIC_SERU_LEMMAS,
    KANJI_VERB_ROW_FRAMES,
    KYUJITAI_TO_SHINJITAI,
    SLANG_ADJ_FOLLOWER,
    SLANG_ADJ_STEMS,
    SLANG_VERB_STEMS,
    UNUSUAL_NAMES,
    WORD_EXCEPTION_BLOCKED_FOLLOWERS,
    WORD_EXCEPTIONS,
)
from .core_lexicon import core_headwords
from .mecab import is_single_token_of_pos, mecab_analyze
from .postprocessor_common import _raw_analysis


def _is_emphatic_spelling(original: str, standard: str, host_pos: str = "") -> bool:
    """Whether `original` is `standard` written with emphatic lengthening or repetition.

    Scoped to the marks that spell emphasis rather than a morpheme: the prolonged
    sound mark, the utterance-final sokuon, and a repeated final vowel. Removing
    them and collapsing runs of the same character reduces かわいーー, すごーーい,
    すごいいいい and ですっ to their dictionary forms, while a genuine lexical
    substitution (にゃー → ねえ) stays distinct. None of them changes the word,
    so an auxiliary is reduced like any other host.
    """
    del host_pos  # Emphasis reduces the same way whatever the host.
    if original == standard:
        return False
    stripped = original.rstrip("".join(_EMPHATIC_SOKUON_MARKS))
    if stripped == standard:
        return True
    if "ー" not in stripped and not (
        stripped.startswith(standard) and set(stripped[len(standard) :]) <= {standard[-1:]}
    ):
        return False
    reduced = regex.sub(r"(.)\1+", r"\1", regex.sub(r"ー+", "", stripped))
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
      own right — やばい, 痛い and 甚く spelled in kana — and is left as it stands,
      as is a stem the analysis already reads in its native class (やば+すぎ).
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
        return len(token["surface"]) <= len(stem) and token.get("pos") != native_pos

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
        # An unknown katakana run (マジヤバ) glues an intensifier onto the stem;
        # its last two morae are the stem the inflection belongs to.
        unknown_katakana = regex.fullmatch(r"\p{Katakana}{3,}", surface) is not None and token.get("lemma") in (
            None,
            "*",
        )
        if token.get("pos") != "名詞" or not (
            regex.fullmatch(r"[\p{Hiragana}\p{Katakana}]{2}", surface) or unknown_kanji or unknown_katakana
        ):
            continue
        if not tail or tail[0] not in _ADJECTIVE_INFLECTION_KANA or len(tail) > 2:
            continue
        # A tail that is itself a real adjective (けち+くさ of くさい) is the
        # compound's second half, not an invented ending.
        if following.get("pos") == "形容詞" and len(following.get("lemma", "")) >= 3:
            continue
        if unknown_katakana:
            stems[start + len(surface) - 2] = surface[-2:]
        else:
            stems[start] = surface
    return stems


def _kana_adjective_stems_before_suffix(text: str, raw: tuple[int, dict[int, dict]]) -> dict[int, str]:
    """Kana adjective stems the analysis tore up before excess すぎ / appearance そう.

    Both suffixes take the bare stem, and a kana stem the dictionary does not
    expect there comes back as mora pieces (ねこ+か+わい+すぎ). The stem is the
    longest kana run R in front of the suffix for which R+い is one adjective,
    taken only where R starts and ends on token edges and is not already read
    as an adjective, so a lexeme that runs on through the suffix (かわいそう)
    stays whole. A run behind a predicate is that predicate's own suffix
    (読み+やす+そう), not a free stem.
    """
    ends = {start + len(token.get("surface", "")): token for start, token in raw[1].items()}
    stems: dict[int, str] = {}
    for m in regex.finditer(r"(?<=\p{Hiragana}{2})(?:すぎ|そう)", text):
        suffix_start = m.start()
        if suffix_start not in raw[1]:
            continue
        run_start = suffix_start
        while run_start > 0 and regex.fullmatch(r"\p{Hiragana}", text[run_start - 1]):
            run_start -= 1
        for start in range(run_start, suffix_start - 1):
            stem = text[start:suffix_start]
            token = raw[1].get(start)
            if token is None or token.get("pos") == "形容詞":
                continue
            if ends.get(start, {}).get("pos") in ("動詞", "形容詞", "助動詞"):
                continue
            if is_single_token_of_pos(stem + "い", "形容詞"):
                stems[start] = stem
                break
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
        ("slang_adj", SLANG_ADJ_STEMS, SLANG_ADJ_FOLLOWER, "形容詞"),
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
    for start, stem in _kana_adjective_stems_before_suffix(text, raw).items():
        # The dictionary leaves a stem before these suffixes as a noun, which
        # the stem repair later reads as the adjective; the substitute only has
        # to land as one token directly in front of the suffix.
        probe_index = _raw_analysis(text[:start] + SLANG_ADJ_SUBSTITUTE + text[start + len(stem) :])[1]
        landed = probe_index.get(start)
        if landed is None or landed.get("surface") != SLANG_ADJ_SUBSTITUTE or start + 1 not in probe_index:
            continue
        replacements[(start, "slang_adj")] = {
            "original": stem,
            "replacement": SLANG_ADJ_SUBSTITUTE,
            "length": len(stem),
        }
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

    # A kanji verb spelling the dictionary lacks (断じる, 俟つ, 失くす) leaves the
    # kanji stranded as a noun before its okurigana. Its conjugation row shows in
    # the kana behind it, so a frame kanji of a known verb on that row stands in
    # for it; the substitution is kept only when the frame reads as a verb and
    # the analysis gets strictly shorter, i.e. the okurigana rejoins its stem.
    for start, head in raw[1].items():
        surface = head.get("surface", "")
        frame = KANJI_VERB_ROW_FRAMES.get(text[start + 1 : start + 2])
        if (
            frame is None
            or len(surface) != 1
            or regex.fullmatch(r"\p{Han}", surface) is None
            or head.get("pos") == "動詞"
            or (start, "kanji_verb_frame") in replacements
        ):
            continue
        probe_count, probe_index = _raw_analysis(text[:start] + frame + text[start + 1 :])
        landed = probe_index.get(start)
        if landed is None or landed["pos"] != "動詞" or len(landed["surface"]) < 2 or probe_count >= raw[0]:
            continue
        replacements[(start, "kanji_verb_frame")] = {"original": surface, "replacement": frame, "length": 1}

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
        "kanji_verb_frame": "kanji-verb-frame",
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


def repair_assimilated_koto_copula(tokens: list[dict]) -> None:
    """Rebuild こっ+ちゃ, the formal noun こと assimilated to the copula じゃ.

    えらいこっちゃ and 知ったこっちゃない are ことじゃ with the copula's voiced
    onset pulled into a geminate. The reference dictionary reads the pair as
    the verb 凝る plus the contracted ては, or as the pronoun こっち plus an
    unknown run that starts with ゃ. Only a modifier can host a formal noun, so
    a particle or the start of the clause in front keeps the verb reading
    (肩がこっちゃ). The tokens match what ことじゃ already yields.
    """
    idx = 1
    while idx < len(tokens) - 1:
        token, following = tokens[idx], tokens[idx + 1]
        host_pos = tokens[idx - 1].get("pos")
        verb_reading = (
            token.get("surface") == "こっ" and token.get("pos") == "動詞" and following.get("surface") == "ちゃ"
        )
        pronoun_reading = token.get("surface") == "こっち" and following.get("surface", "").startswith("ゃ")
        if host_pos in ("助詞", "記号") or not (verb_reading or pronoun_reading):
            idx += 1
            continue
        rest = following.get("surface", "")[1:] if pronoun_reading else ""
        tail_end = idx + 2
        # A negative read against ては is the adjective; against the copula it
        # is the auxiliary, as in ことじゃない.
        if (
            tail_end < len(tokens)
            and tokens[tail_end].get("pos") == "形容詞"
            and tokens[tail_end].get("lemma") == "ない"
        ):
            rest += tokens[tail_end].get("surface", "")
            tail_end += 1
        tokens[idx:tail_end] = [
            {
                "surface": "こっ",
                "pos": "名詞",
                "pos_sub1": "非自立",
                "pos_sub2": "一般",
                "lemma": "こと",
                "reading": "コッ",
            },
            {
                "surface": "ちゃ",
                "pos": "助動詞",
                "pos_sub1": "*",
                "conj_type": "特殊・ダ",
                "conj_form": "連用形",
                "lemma": "だ",
                "reading": "チャ",
            },
            # The tail is read after ことじゃ so it keeps the copula's context.
            *(mecab_analyze("ことじゃ" + rest)[2:] if rest else []),
        ]
        idx += 2


def repair_contracted_volitional(tokens: list[dict]) -> None:
    """Rebuild the volitional う contracted to っ before the question particle.

    行こっか, 食べよっか and 帰っとこっか are 行こう, 食べよう and 帰っとこう
    with the う pulled into a geminate by か. The reference dictionary reads the
    geminate as a verb of its own (よっ as よる, こっ as こう), so the prefix is
    re-analyzed with the plain う, which selects the volitional cell, and the
    geminate takes that う's place.
    """
    idx = 1
    repaired_end = 0
    while idx < len(tokens):
        token, previous = tokens[idx], tokens[idx - 1]
        if (
            token.get("surface") != "か"
            or token.get("pos") != "助詞"
            or previous.get("pos") != "動詞"
            or not previous.get("surface", "").endswith("っ")
        ):
            idx += 1
            continue
        prefix = "".join(t.get("surface", "") for t in tokens[repaired_end:idx])[:-1]
        volitional = mecab_analyze(prefix + "う")
        if (
            len(volitional) < 2
            or volitional[-1].get("surface") != "う"
            or volitional[-1].get("pos") != "助動詞"
            or volitional[-2].get("conj_form") != "未然ウ接続"
        ):
            idx += 1
            continue
        geminate = {**volitional[-1], "surface": "っ", "reading": "ッ"}
        tokens[repaired_end:idx] = [*volitional[:-1], geminate]
        repaired_end += len(volitional)
        idx = repaired_end + 1


def repair_regional_imperative(tokens: list[dict]) -> None:
    """Rebuild two regional imperatives the reference dictionary does not know.

    The Chugoku んさい is なさい contracted (食べんさい, 書きんさい): one honorific
    imperative on the continuative, read instead as an attributive verb in ん
    plus the noun さい. The Kansai continuative imperative of とく drawn out by
    its own vowel (見ときい, しときい) is read as と plus a noun きい; holding a
    final vowel keeps it one word, as for any other lengthening.
    """
    idx = 1
    repaired_end = 0
    while idx < len(tokens):
        token, previous = tokens[idx], tokens[idx - 1]
        following = tokens[idx + 1] if idx + 1 < len(tokens) else None
        clause_end = following is None or following.get("pos") == "記号"
        if (
            token.get("surface") == "さい"
            and token.get("pos") == "名詞"
            and previous.get("pos") == "動詞"
            and previous.get("surface", "").endswith("ん")
        ):
            prefix = "".join(t.get("surface", "") for t in tokens[repaired_end:idx])[:-1]
            continuative = mecab_analyze(prefix + "ます")
            if continuative and continuative[-1].get("surface") == "ます":
                honorific = {
                    "surface": "んさい",
                    "pos": "助動詞",
                    "pos_sub1": "*",
                    "lemma": "んさる",
                    "reading": "ンサイ",
                }
                tokens[repaired_end : idx + 1] = [*continuative[:-1], honorific]
                repaired_end += len(continuative)
                idx = repaired_end
                continue
        if (
            idx >= 2
            and token.get("surface") == "さい"
            and token.get("pos") == "名詞"
            and previous.get("surface") == "ん"
            and tokens[idx - 2].get("pos") == "動詞"
            and tokens[idx - 2].get("conj_form") == "連用形"
        ):
            tokens[idx - 1 : idx + 1] = [
                {"surface": "んさい", "pos": "助動詞", "pos_sub1": "*", "lemma": "んさる", "reading": "ンサイ"}
            ]
            continue
        if (
            idx >= 2
            and token.get("surface") == "きい"
            and previous.get("surface") == "と"
            and previous.get("pos") == "助詞"
            and tokens[idx - 2].get("pos") == "動詞"
            and clause_end
        ):
            tokens[idx - 1 : idx + 1] = [
                {
                    "surface": "ときい",
                    "pos": "動詞",
                    "pos_sub1": "非自立",
                    "lemma": "とく",
                    "conj_type": "五段・カ行イ音便",
                    "conj_form": "連用形",
                    "reading": "トキイ",
                }
            ]
            continue
        idx += 1


def repair_contracted_quotative(tokens: list[dict]) -> None:
    """Rebuild って+いう contracted to っ+つー (そうだっつーの, やるっつったら).

    The quotative shrinks to its geminate and いう takes the つ onset. The
    reference dictionary reads the geminate as part of the host (だっ, いっ) or
    as the verb く, and the rest as つ or つる. The host is re-analyzed in front
    of the plain quotative と, which fixes its own boundary, and the remainder
    is いう in the cell its spelling shows: つー/つう terminal, つっ onbin.
    """
    idx = 1
    repaired_end = 0
    while idx < len(tokens):
        token, previous = tokens[idx], tokens[idx - 1]
        following = tokens[idx + 1] if idx + 1 < len(tokens) else None
        surface = token.get("surface", "")
        if not previous.get("surface", "").endswith("っ") or not surface.startswith("つ"):
            idx += 1
            continue
        consumed = 1
        if surface == "つ" and following is not None and following.get("surface") == "ー":
            verb = {"surface": "つー", "conj_form": "基本形"}
            consumed = 2
        elif surface in ("つう", "つー"):
            verb = {"surface": surface, "conj_form": "基本形"}
        elif surface == "つっ":
            verb = {"surface": surface, "conj_form": "連用タ接続"}
        else:
            idx += 1
            continue
        prefix = "".join(t.get("surface", "") for t in tokens[repaired_end:idx])[:-1]
        host = mecab_analyze(prefix + "と") if prefix else []
        if not host or host[-1].get("surface") != "と" or host[-1].get("pos") != "助詞":
            idx += 1
            continue
        quote = {
            "surface": "っ",
            "pos": "助詞",
            "pos_sub1": "格助詞",
            "pos_sub2": "引用",
            "lemma": "って",
            "reading": "ッ",
        }
        verb.update({"pos": "動詞", "pos_sub1": "自立", "lemma": "いう", "conj_type": "五段・ワ行促音便"})
        tokens[repaired_end : idx + consumed] = [*host[:-1], quote, verb]
        repaired_end += len(host) + 1
        idx = repaired_end + 1


def repair_euphonic_adjective_adverb(tokens: list[dict]) -> None:
    """Read an adjective's euphonic continuative as the adjective it inflects.

    早う is 早く with the く softened to う, the same cell as 高う in 高うございます;
    the reference dictionary lists some of these as adverbs of their own, so a
    う-final adverb is re-analyzed before ございます, which selects that cell.
    """
    for idx, token in enumerate(tokens):
        surface = token.get("surface", "")
        if token.get("pos") != "副詞" or len(surface) < 2 or not surface.endswith("う"):
            continue
        analyzed = mecab_analyze(surface + "ございます")
        if (
            analyzed
            and analyzed[0].get("surface") == surface
            and analyzed[0].get("pos") == "形容詞"
            and analyzed[0].get("conj_form", "").startswith("連用ゴザイ")
        ):
            tokens[idx] = analyzed[0]


def repair_continuative_before_manner_suffix(tokens: list[dict]) -> None:
    """Read the host of the manner suffix よう as the verb continuative it is.

    よう in 読みようがない attaches to a continuative, and the reference
    dictionary analyzes やめ+よう that way; it lists あきらめ and 慰め as nouns of
    their own, though, so the host is re-analyzed before ます, which selects the
    continuative, and adopted when it reads as one.
    """
    for idx in range(len(tokens) - 1):
        token, suffix = tokens[idx], tokens[idx + 1]
        if (
            token.get("pos") != "名詞"
            or token.get("pos_sub1") != "一般"
            or suffix.get("surface") != "よう"
            or suffix.get("pos_sub1") != "接尾"
        ):
            continue
        surface = token.get("surface", "")
        analyzed = mecab_analyze(surface + "ます")
        if (
            analyzed
            and analyzed[0].get("surface") == surface
            and analyzed[0].get("pos") == "動詞"
            and analyzed[0].get("conj_form", "").startswith("連用")
        ):
            tokens[idx] = analyzed[0]


def repair_adjective_stem_before_suffix(tokens: list[dict]) -> None:
    """Read a bare noun before excess すぎ / appearance そう as the adjective stem.

    Utterance-final 高すぎ comes back as noun+suffix although 高すぎた is the
    adjective stem plus the continuative of すぎる; the host is re-analyzed
    before すぎた and adopted, with its すぎ, when it reads as that stem.
    """
    for idx in range(len(tokens) - 1):
        token, suffix = tokens[idx], tokens[idx + 1]
        if (
            token.get("pos") != "名詞"
            or token.get("pos_sub1") != "一般"
            or suffix.get("surface") not in ("すぎ", "そう")
            or suffix.get("pos_sub1") != "接尾"
        ):
            continue
        surface = token.get("surface", "")
        analyzed = mecab_analyze(surface + "すぎた")
        if (
            len(analyzed) >= 2
            and analyzed[0].get("surface") == surface
            and analyzed[0].get("pos") == "形容詞"
            and analyzed[1].get("surface") == "すぎ"
        ):
            tokens[idx] = analyzed[0]
            if suffix.get("surface") == "すぎ":
                tokens[idx + 1] = analyzed[1]


def repair_interrogative_nande(tokens: list[dict]) -> None:
    """Rejoin なん + copula で into the adverb なんで before a copula.

    A copula continuative cannot take another copula, so なん+で+です is the
    adverb なんで the dictionary already gives in なんでだよ.
    """
    idx = 0
    while idx < len(tokens) - 2:
        head, copula, following = tokens[idx], tokens[idx + 1], tokens[idx + 2]
        if (
            head.get("surface") == "なん"
            and head.get("pos") == "名詞"
            and copula.get("surface") == "で"
            and copula.get("pos") == "助動詞"
            and following.get("pos") == "助動詞"
            and following.get("lemma") in ("です", "だ")
        ):
            tokens[idx : idx + 2] = [{"surface": "なんで", "pos": "副詞", "pos_sub1": "一般", "lemma": "なんで"}]
        idx += 1


_REASON_NDE_BLOCKERS = frozenset({"は", "も", "ある", "あり", "あっ", "ござい", "ござる"})


def split_reason_nde(tokens: list[dict]) -> None:
    """Read the reason んで closing a clause as nominalizer ん + conjunctive で.

    The contraction of ので keeps both morae apart, as 好きなんです keeps ん+です;
    the reference fuses them after な (雨なんで) but reads the で after a verb as
    the copula (行くんで). A following topic or existence verb (行くんではない)
    keeps the copula reading.
    """
    idx = 0
    while idx < len(tokens):
        token = tokens[idx]
        if token.get("surface") == "んで" and token.get("pos") == "助詞" and idx > 0:
            tokens[idx : idx + 1] = [
                {"surface": "ん", "pos": "名詞", "pos_sub1": "非自立", "lemma": "ん"},
                {"surface": "で", "pos": "助詞", "pos_sub1": "接続助詞", "lemma": "で"},
            ]
            idx += 2
            continue
        de = tokens[idx + 1] if idx + 1 < len(tokens) else None
        following = tokens[idx + 2] if idx + 2 < len(tokens) else None
        if (
            idx > 0
            and de is not None
            and token.get("surface") == "ん"
            and token.get("pos") == "名詞"
            and token.get("pos_sub1") == "非自立"
            and de.get("surface") == "で"
            and de.get("pos") == "助動詞"
            and (
                following is None
                or following.get("pos") == "記号"
                or (
                    following.get("pos") in ("動詞", "形容詞")
                    and following.get("pos_sub1") == "自立"
                    and following.get("surface") not in _REASON_NDE_BLOCKERS
                )
            )
        ):
            tokens[idx + 1] = {"surface": "で", "pos": "助詞", "pos_sub1": "接続助詞", "lemma": "で"}
        idx += 1


_CONTRACTED_NEGATIVES = frozenset({"ない", "ねえ", "ねぇ", "ねー"})


def repair_contracted_rareru(tokens: list[dict]) -> None:
    """Read らん before a negative as the contracted potential られ.

    The reference has only the noun らん (蘭), so 見てらんない and 信じらんない
    come back as a noun plus an independent ない. After a continuative or て
    and before a negative it is られ with its row nasalized.
    """
    for idx in range(1, len(tokens) - 1):
        host, token, negative = tokens[idx - 1], tokens[idx], tokens[idx + 1]
        if (
            token.get("surface") == "らん"
            and token.get("pos") == "名詞"
            and negative.get("surface") in _CONTRACTED_NEGATIVES
            and (
                # The te-particle may come fused into a misread として (こ+として).
                (host.get("pos") == "助詞" and host.get("surface", "").endswith(("て", "で")))
                or (host.get("pos") == "動詞" and host.get("conj_form", "").startswith("連用"))
            )
        ):
            tokens[idx] = {
                "surface": "らん",
                "pos": "動詞",
                "pos_sub1": "接尾",
                "conj_type": "一段",
                "conj_form": "未然形",
                "lemma": "られる",
            }
            tokens[idx + 1] = {
                "surface": negative.get("surface", ""),
                "pos": "助動詞",
                "conj_type": "特殊・ナイ",
                "conj_form": "基本形",
                "lemma": "ない",
            }


def _evaluates_iika(previous: dict) -> bool:
    """Whether the word before いっか frames an evaluation (まあ, これで, それは)."""
    return previous.get("pos") == "副詞" or (
        previous.get("pos") == "助詞" and previous.get("surface") in ("で", "は", "も")
    )


def repair_contracted_iika(tokens: list[dict]) -> None:
    """Split a clause-final kana いっか into いい contracted before か.

    いいか shortens to いっか (まあいっか, これでいっか), which the reference
    reads as the noun 一家 or 一過; those are written in kanji, and a kana
    noun does not close an utterance on its own after an adverb or particle.
    """
    for idx, token in enumerate(tokens):
        following = tokens[idx + 1] if idx + 1 < len(tokens) else None
        after = tokens[idx + 2] if idx + 2 < len(tokens) else None
        # An onbin いっ cannot carry the final か either (もういっか。).
        if (
            idx > 0
            and token.get("surface") == "いっ"
            and token.get("pos") == "動詞"
            and _evaluates_iika(tokens[idx - 1])
            and following is not None
            and following.get("surface") == "か"
            and following.get("pos_sub1", "").endswith("終助詞")
            and (after is None or after.get("pos") == "記号")
        ):
            tokens[idx] = {"surface": "いっ", "pos": "形容詞", "pos_sub1": "自立", "lemma": "いい"}
            continue
        if (
            idx > 0
            and token.get("surface") == "いっか"
            and token.get("pos") == "名詞"
            and _evaluates_iika(tokens[idx - 1])
            and (following is None or following.get("pos") == "記号" or following.get("pos_sub1") == "終助詞")
        ):
            tokens[idx : idx + 1] = [
                {"surface": "いっ", "pos": "形容詞", "pos_sub1": "自立", "lemma": "いい"},
                {"surface": "か", "pos": "助詞", "pos_sub1": "終助詞", "lemma": "か"},
            ]


def repair_lengthened_negative(tokens: list[dict]) -> None:
    """Read ねー after an irrealis or an adjective's く as the negative ない.

    The reference already reads ねぇ and ねえ there as the colloquial ない, but
    takes the ー spelling for the final particle ね; a final particle cannot
    follow a bare irrealis or continuative.
    """
    for idx in range(1, len(tokens)):
        host, token = tokens[idx - 1], tokens[idx]
        if (
            token.get("surface") == "ねー"
            and token.get("pos") == "助詞"
            and (
                (host.get("pos") == "動詞" and host.get("conj_form") == "未然形")
                or (host.get("pos") == "形容詞" and host.get("conj_form", "").startswith("連用"))
            )
        ):
            tokens[idx] = {
                "surface": "ねー",
                "pos": "助動詞",
                "conj_type": "特殊・ナイ",
                "conj_form": "音便基本形",
                "lemma": "ない",
            }


def repair_kamo_quotative(tokens: list[dict]) -> None:
    """Rebuild か + もって as the particle かも + quotative って.

    もって (以て) opens a clause, so it cannot follow the particle か; the
    reference reaches it only when the host before か is an adverb (そうかもって),
    and reads 雨かもって as かも + って.
    """
    for idx in range(len(tokens) - 1):
        ka, motte = tokens[idx], tokens[idx + 1]
        if ka.get("surface") == "か" and ka.get("pos") == "助詞" and motte.get("surface") == "もって":
            tokens[idx : idx + 2] = [
                {"surface": "かも", "pos": "助詞", "pos_sub1": "副助詞", "lemma": "かも"},
                {"surface": "って", "pos": "助詞", "pos_sub1": "格助詞", "pos_sub2": "連語", "lemma": "って"},
            ]


def merge_honorific_kana_verbal_noun(tokens: list[dict]) -> None:
    """Rebuild ご + a fragmented kana verbal noun + する as one noun + する.

    The reference cuts the kana spelling of a Sino-Japanese verbal noun into
    kanji-readable pieces after the prefix ご (ご+れん+らく+し, ご+あん+ない+
    し(助詞)), while it keeps ご+あんしん whole; the run between ご and する (or
    its humble いたす) in any cell is one noun.
    """

    def closes_frame(position: int) -> bool:
        token = tokens[position]
        if token.get("pos") == "動詞" and token.get("lemma") in ("する", "いたす"):
            return True
        return (
            token.get("surface") == "し" and position + 1 < len(tokens) and tokens[position + 1].get("pos") == "助動詞"
        )

    idx = 0
    while idx < len(tokens) - 3:
        if tokens[idx].get("surface") != "ご" or tokens[idx].get("pos") != "接頭詞":
            idx += 1
            continue
        end = idx + 1
        while end < len(tokens) and regex.fullmatch(r"\p{Hiragana}+", tokens[end].get("surface", "")):
            if closes_frame(end):
                break
            end += 1
        if end - idx - 1 >= 2 and end < len(tokens) and closes_frame(end):
            noun = "".join(token.get("surface", "") for token in tokens[idx + 1 : end])
            verb = tokens[end]
            if verb.get("pos") != "動詞":
                verb = {"surface": "し", "pos": "動詞", "pos_sub1": "自立", "lemma": "する"}
            tokens[idx + 1 : end + 1] = [
                {"surface": noun, "pos": "名詞", "pos_sub1": "サ変接続", "lemma": noun},
                verb,
            ]
        idx += 1


def split_demonstrative_dake(tokens: list[dict]) -> None:
    """Split the reference's adverb これだけ/それだけ into pronoun + だけ.

    The reference splits それ+だけ before the copula (それ+だけ+だ) but keeps
    one adverb before a final particle or a verb (それだけ+よ); the pronoun
    and the adverbial particle are separate words in both positions.
    """
    idx = 0
    while idx < len(tokens):
        token = tokens[idx]
        surface = token.get("surface", "")
        if token.get("pos") == "副詞" and regex.fullmatch(r"[こそあど]れだけ", surface):
            tokens[idx : idx + 1] = [
                {"surface": surface[:2], "pos": "名詞", "pos_sub1": "代名詞", "lemma": surface[:2]},
                {"surface": "だけ", "pos": "助詞", "pos_sub1": "副助詞", "lemma": "だけ"},
            ]
            idx += 2
            continue
        idx += 1


def repair_mimetic_n_to_suru(tokens: list[dict]) -> None:
    """Rebuild a two-mora kana noun in ん + と + する as the mimetic adverb.

    The reference reads 部屋がしんとした as the noun しん (芯) + と, yet
    returns しんと as one adverb before a content verb; a CVん mora pair
    marked by と before する is the mimetic manner adverb (ぽつんと+した).
    """
    idx = 0
    while idx < len(tokens) - 2:
        noun, to, verb = tokens[idx], tokens[idx + 1], tokens[idx + 2]
        surface = noun.get("surface", "")
        if (
            noun.get("pos") == "名詞"
            and regex.fullmatch(r"\p{Hiragana}ん", surface)
            and to.get("surface") == "と"
            and to.get("pos") == "助詞"
            and verb.get("lemma") == "する"
        ):
            tokens[idx : idx + 2] = [
                {"surface": surface + "と", "pos": "副詞", "pos_sub1": "一般", "lemma": surface + "と"}
            ]
        idx += 1


def repair_adjective_yo_quotative(tokens: list[dict]) -> None:
    """Split a kana verb in よっ before て that is an adjective plus よ + って.

    The reference reads いいよって as the kana spelling of 言い寄って; the
    kana run is the finished adjective いい, the final particle よ and the
    quotative って, as いいかって already comes back.
    """
    idx = 0
    while idx < len(tokens) - 1:
        token, te = tokens[idx], tokens[idx + 1]
        surface = token.get("surface", "")
        host = surface[:-2]
        if (
            token.get("pos") == "動詞"
            and surface.endswith("よっ")
            and regex.fullmatch(r"\p{Hiragana}+", surface)
            and te.get("surface") == "て"
            and host
        ):
            analyzed = mecab_analyze(host)
            if len(analyzed) == 1 and analyzed[0].get("pos") == "形容詞" and analyzed[0].get("conj_form") == "基本形":
                tokens[idx : idx + 2] = [
                    analyzed[0],
                    {"surface": "よ", "pos": "助詞", "pos_sub1": "終助詞", "lemma": "よ"},
                    {"surface": "って", "pos": "助詞", "pos_sub1": "格助詞", "lemma": "って"},
                ]
                idx += 3
                continue
        idx += 1


def merge_conjunction_with_rashii(tokens: list[dict]) -> None:
    """Rebuild an adjective in らしい the reference dictionary reads as a conjunction.

    A conjunction takes no auxiliary, so もっとも+らしい is not もっとも plus the
    conjectural; it is the adjective もっともらしい the dictionary lacks, and the
    らしい token already carries its inflection.
    """
    idx = 0
    while idx < len(tokens) - 1:
        head, tail = tokens[idx], tokens[idx + 1]
        if head.get("pos") != "接続詞" or tail.get("pos") != "助動詞" or tail.get("lemma") != "らしい":
            idx += 1
            continue
        tokens[idx : idx + 2] = [
            {
                **tail,
                "surface": head.get("surface", "") + tail.get("surface", ""),
                "pos": "形容詞",
                "pos_sub1": "自立",
                "lemma": head.get("surface", "") + "らしい",
            }
        ]
        idx += 1


def repair_productive_causative(tokens: list[dict]) -> None:
    """Split a productive causative the reference lexicon lists as one verb.

    笑わせ+ない and 泣かせ+た are the irrealis of 笑う and 泣く plus the causative
    せる, as 飛ば+せ+た already is; the reference dictionary keeps some of them
    whole only because it carries the ichidan headword, and only in some cells
    (笑わ+せる but 笑わせ+ない). Suzume's own lexicon decides which are
    lexicalized words (知らせる, 合わせる, and compounds ending in one such as
    組み合わせる), and a stem that is not the irrealis of a verb is no causative
    at all (見せる).
    """
    from .split_rules import base_from_mizenkei

    idx = 0
    while idx < len(tokens):
        token = tokens[idx]
        lemma = token.get("lemma") or ""
        surface = token.get("surface", "")
        stem = lemma[:-2]
        base = base_from_mizenkei(stem) if lemma.endswith("せる") and len(lemma) > 3 else None
        if (
            token.get("pos") != "動詞"
            or token.get("conj_type") != "一段"
            or base is None
            or not surface.startswith(stem)
            or len(surface) == len(stem)
            or lemma in CLOSED_HONORIFIC_SERU_LEMMAS
            or any(lemma[start:] in core_headwords("verbs.tsv") for start in range(len(lemma) - 2))
            or not is_single_token_of_pos(base, "動詞")
        ):
            idx += 1
            continue
        tokens[idx : idx + 1] = [
            {"surface": stem, "pos": "動詞", "pos_sub1": "自立", "lemma": base, "conj_form": "未然形"},
            {**token, "surface": surface[len(stem) :], "lemma": "せる", "pos_sub1": "接尾"},
        ]
        idx += 2
