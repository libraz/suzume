"""Spelling substitutions, restoration, and repairs around MeCab analysis."""

from itertools import pairwise

import regex

from . import postprocessor_mecab_contractions, postprocessor_mecab_nominal_repairs
from .constants import (
    CHARACTER_SPEECH_FINAL_PARTICLES,
    FINITE_PREDECESSOR_CONJ_FORM,
    KANJI_VERB_ROW_FRAMES,
    KYUJITAI_TO_SHINJITAI,
    SLANG_ADJ_FOLLOWER,
    SLANG_ADJ_STEMS,
    SLANG_VERB_STEMS,
    UNUSUAL_NAMES,
    WORD_EXCEPTION_BLOCKED_FOLLOWERS,
    WORD_EXCEPTIONS,
)
from .mecab import is_single_token_of_pos, mecab_analyze
from .merge_postprocessor_spelling import _MORA_VOWEL
from .postprocessor_common import _raw_analysis

# Exports from postprocessor_mecab_contractions.
repair_adjective_stem_before_suffix = postprocessor_mecab_contractions.repair_adjective_stem_before_suffix
repair_continuative_before_manner_suffix = postprocessor_mecab_contractions.repair_continuative_before_manner_suffix
repair_contracted_iika = postprocessor_mecab_contractions.repair_contracted_iika
repair_contracted_quotative = postprocessor_mecab_contractions.repair_contracted_quotative
repair_contracted_rareru = postprocessor_mecab_contractions.repair_contracted_rareru
repair_contracted_volitional = postprocessor_mecab_contractions.repair_contracted_volitional
repair_euphonic_adjective_adverb = postprocessor_mecab_contractions.repair_euphonic_adjective_adverb
repair_interrogative_nande = postprocessor_mecab_contractions.repair_interrogative_nande
repair_kamo_quotative = postprocessor_mecab_contractions.repair_kamo_quotative
repair_lengthened_negative = postprocessor_mecab_contractions.repair_lengthened_negative
repair_regional_imperative = postprocessor_mecab_contractions.repair_regional_imperative
repair_sentence_final_godan_imperative = postprocessor_mecab_contractions.repair_sentence_final_godan_imperative
split_reason_nde = postprocessor_mecab_contractions.split_reason_nde

# Exports from postprocessor_mecab_nominal_repairs.
merge_conjunction_with_rashii = postprocessor_mecab_nominal_repairs.merge_conjunction_with_rashii
merge_honorific_kana_verbal_noun = postprocessor_mecab_nominal_repairs.merge_honorific_kana_verbal_noun
repair_adjective_yo_quotative = postprocessor_mecab_nominal_repairs.repair_adjective_yo_quotative
repair_assimilated_koto_copula = postprocessor_mecab_nominal_repairs.repair_assimilated_koto_copula
repair_kanji_prefix_before_kana_noun = postprocessor_mecab_nominal_repairs.repair_kanji_prefix_before_kana_noun
repair_kko_nominalizer = postprocessor_mecab_nominal_repairs.repair_kko_nominalizer
repair_mimetic_n_to_suru = postprocessor_mecab_nominal_repairs.repair_mimetic_n_to_suru
repair_productive_causative = postprocessor_mecab_nominal_repairs.repair_productive_causative
split_demonstrative_dake = postprocessor_mecab_nominal_repairs.split_demonstrative_dake
split_predicate_tokoro_ga = postprocessor_mecab_nominal_repairs.split_predicate_tokoro_ga
split_transparent_suru_te_adverb = postprocessor_mecab_nominal_repairs.split_transparent_suru_te_adverb


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
    stripped = original.rstrip("".join(_EMPHATIC_SOKUON_MARKS) + _EMPHATIC_SMALL_VOWELS)
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


# Small vowels that draw out the mora before them (ますぅ, だよぉ).
_EMPHATIC_SMALL_VOWELS = "ぁぃぅぇぉ"


def _emphatic_small_vowels(text: str) -> list[int]:
    """Where a small vowel repeating the previous mora's vowel closes a phrase."""
    return [
        m.start()
        for m in regex.finditer(r"(?<=\p{Hiragana})[ぁぃぅぇぉ](?=[\p{P}\p{S}\p{Z}]|$)", text)
        if text[m.start() - 1] not in _EMPHATIC_SMALL_VOWELS
        and _MORA_VOWEL.get(text[m.start() - 1]) == _MORA_VOWEL.get(text[m.start()])
    ]


def _emphatic_sokuons(text: str) -> list[int]:
    """Where a sokuon closes an utterance or a phrase, with a host in front of it.

    The phrase ends where punctuation, a symbol, a space or the text end
    follows; nothing is left behind the mark for a sokuon-onbin to carry.
    """
    return [m.start() for m in regex.finditer(r"(?<=[^\s\p{P}\p{S}っッ])[っッ](?=[\p{P}\p{S}\p{Z}]|$)", text)]


# The standard final particle analyzed in place of a role-language one.
_STANDARD_FINAL_PARTICLE = "よ"


_PREDICATE_POS = frozenset({"動詞", "形容詞", "助動詞"})


def _character_speech_particles(text: str, raw: tuple[int, dict[int, dict]]) -> dict[int, str]:
    """Where a role-language final particle closes a predicate the dictionary misreads.

    The particle must close the utterance or a phrase, and the text with a
    standard final particle in its place must read as a terminal-form predicate
    followed by that particle, so a kana word merely ending in the same mora
    does not count. One the dictionary already reads as a final particle
    (走る+のう) is left alone.
    """
    found: dict[int, str] = {}
    for tail in CHARACTER_SPEECH_FINAL_PARTICLES:
        for m in regex.finditer(r"(?<=[^\s\p{P}\p{S}])" + regex.escape(tail) + r"(?=[\p{P}\p{S}\p{Z}]|$)", text):
            start = m.start()
            existing = raw[1].get(start)
            if existing is not None and existing.get("surface") == tail and existing.get("pos") == "助詞":
                continue
            probe = _raw_analysis(text[:start] + _STANDARD_FINAL_PARTICLE + text[m.end() :])[1]
            landed = probe.get(start)
            if landed is None or landed.get("surface") != _STANDARD_FINAL_PARTICLE or landed.get("pos") != "助詞":
                continue
            host_starts = [at for at in probe if at < start]
            host = probe[max(host_starts)] if host_starts else {}
            if host.get("pos") not in _PREDICATE_POS or host.get("conj_form") != FINITE_PREDECESSOR_CONJ_FORM:
                continue
            found[start] = tail
    return found


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
    # An unknown token glued around the mark is a guess as well, unless the
    # mark is all there is to it.
    if lemma in ("", "*"):
        return token.get("surface") not in _EMPHATIC_SOKUON_MARKS
    return lemma != token.get("surface")


def _splits_closing_particle(text: str, raw: tuple[int, dict[int, dict]], position: int) -> bool:
    """Whether a held small vowel tears its host mora off a clause-closing particle.

    The dictionary reads the host mora plus the vowel as a word of its own
    (け + どぉ), so nothing is invented at the mark itself. Without the mark the
    host mora closes a particle that starts earlier (けど, から, のに).
    """
    host = position - 1
    if raw[1].get(host) is None:
        return False
    probe = _raw_analysis(text[:position] + text[position + 1 :])[1]
    starts = [start for start in probe if start <= host]
    if not starts:
        return False
    start = max(starts)
    token = probe[start]
    return start < host and start + len(token.get("surface", "")) == position and token.get("pos") == "助詞"


def _joins_word_across_sokuon(text: str, raw: tuple[int, dict[int, dict]], position: int) -> bool:
    """Whether a medial っ the dictionary cut out alone hides one word around it."""
    bare = raw[1].get(position)
    if bare is None or bare.get("surface") != "っ" or bare.get("lemma") == "っ":
        return False
    probe = _raw_analysis(text[:position] + text[position + 1 :])[1]
    host = probe.get(position - 1)
    return host is not None and len(host.get("surface", "")) > 1 and host.get("pos") in ("名詞", "動詞", "形容詞")


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


# The e-row kana of a fused long e and the kana it fused from: the stem's last
# mora on the a row (たかい → たけえ) or on the o row (すごい → すげえ).
_FUSED_E_SOURCES: dict[str, tuple[str, ...]] = {
    "え": ("あ", "お"),
    "け": ("か", "こ"),
    "げ": ("が", "ご"),
    "せ": ("さ", "そ"),
    "ぜ": ("ざ", "ぞ"),
    "て": ("た", "と"),
    "で": ("だ", "ど"),
    "ね": ("な", "の"),
    "へ": ("は", "ほ"),
    "べ": ("ば", "ぼ"),
    "ぺ": ("ぱ", "ぽ"),
    "め": ("ま", "も"),
    "れ": ("ら", "ろ"),
}


def _vowel_fused_adjectives(text: str) -> dict[int, tuple[str, str]]:
    """Spans spelling an i-adjective (or a pronoun) with its ending fused into a long e.

    The length mark is written え or ー (すげえ, すげー). Keyed by start; the value is the written span and the standard form. The
    stem is the longest kana run before the e-row kana for which the standard
    form is one adjective.
    """
    spans: dict[int, tuple[str, str]] = {}
    for m in regex.finditer(r"(?<=\p{Hiragana})[えけげせぜてでねへべぺめれ](?=[えー])", text):
        fused_at = m.start()
        run_start = fused_at
        while run_start > 0 and fused_at - run_start < 4 and regex.fullmatch(r"\p{Hiragana}", text[run_start - 1]):
            run_start -= 1
        for start in range(run_start, fused_at):
            stem = text[start:fused_at]
            standard = next(
                (
                    stem + source + "い"
                    for source in _FUSED_E_SOURCES[text[fused_at]]
                    if is_single_token_of_pos(stem + source + "い", "形容詞")
                ),
                "",
            )
            # The same fusion contracts a pronoun's a+e (おまえ → おめえ); that
            # reading wins over a homographic adjective (おもい).
            pronoun = _fused_pronoun(stem, text[fused_at])
            if pronoun or standard:
                spans[start] = (text[start : fused_at + 2], pronoun or standard)
                break
    return spans


def _fused_pronoun(stem: str, fused: str) -> str:
    """The pronoun whose a+e the long e contracts, or an empty string."""
    source = stem + _FUSED_E_SOURCES[fused][0] + "え"
    tokens = mecab_analyze(source)
    return source if len(tokens) == 1 and tokens[0].get("pos_sub1") == "代名詞" else ""


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

    # A sokuon closing an utterance or a phrase (before punctuation) is
    # emphasis rather than a morpheme: the real
    # sokuon-onbin exists only to carry the past and conjunctive suffixes, and
    # nothing follows this one for it to carry. The dictionary still has to place
    # the kana and invents a word for it — a bare っ read as the adjective's
    # continuative auxiliary, an いっ read as 言う, a てっ read as てる — so the
    # mark is dropped before the analysis and put back into the surface
    # afterwards. A final sokuon the dictionary already reads as a word in its
    # own right (あっ, えっ) has itself for a lemma and is left alone.
    # A held vowel (small vowel or ー) can also tear its host mora off a
    # closing particle (け + どぉ) without inventing a word at the mark.
    sokuons = _emphatic_sokuons(text)
    held_marks = [m.start() for m in regex.finditer(r"(?<=\p{Hiragana})ー(?=[\p{P}\p{S}\p{Z}]|$)", text)]
    for emphatic_sokuon in [*sokuons, *_emphatic_small_vowels(text), *held_marks]:
        if raw is None:
            raw = _raw_analysis(text)
        if (emphatic_sokuon not in held_marks and _invents_a_word_for(raw, emphatic_sokuon)) or (
            emphatic_sokuon not in sokuons and _splits_closing_particle(text, raw, emphatic_sokuon)
        ):
            replacements[(emphatic_sokuon - 1, "emphatic_sokuon")] = {
                "original": text[emphatic_sokuon - 1 : emphatic_sokuon + 1],
                "replacement": text[emphatic_sokuon - 1],
                "length": 2,
            }

    # An emphatic sokuon inside a word (大っ嫌い) has no entry either: the
    # dictionary cuts it out as a bare っ with an invented lemma. Taken only
    # when the text without it reads the kanji and what follows as one word,
    # so a lexicalized form (取っ手, 吹っ飛ぶ) never reaches this.
    for m in regex.finditer(r"(?<=\p{Han})っ(?=[\p{Han}\p{Hiragana}])", text):
        if raw is None:
            raw = _raw_analysis(text)
        if _joins_word_across_sokuon(text, raw, m.start()):
            replacements[(m.start() - 1, "emphatic_sokuon")] = {
                "original": text[m.start() - 1 : m.end()],
                "replacement": text[m.start() - 1],
                "length": 2,
            }

    # Role-language and regional final particles (もふ, ぴょん, っぴ) are not
    # in the dictionary, which invents words for their kana and can pull the
    # predicate's last mora into them. A standard final particle is analyzed in
    # their place, and restoration puts the surface back as the lemma.
    if raw is None:
        raw = _raw_analysis(text)
    for start, tail in _character_speech_particles(text, raw).items():
        replacements[(start, "character_speech")] = {
            "original": tail,
            "replacement": _STANDARD_FINAL_PARTICLE,
            "length": len(tail),
        }

    # A colloquial i-adjective fuses its last vowel with い into a long e
    # (すごい → すげえ, うまい → うめえ). The dictionary reads the e-row kana
    # plus え as a verb, a noun or a filler, so the standard form is put back in
    # its place when it reads as one adjective, and the surface is restored
    # afterwards with the standard lemma kept. A negative auxiliary already
    # read there (知らねえ) is the same fusion on ない and is left alone.
    for start, (original, standard) in _vowel_fused_adjectives(text).items():
        if raw is None:
            raw = _raw_analysis(text)
        fused_at = start + len(original) - 2
        covering = [token for at, token in raw[1].items() if at <= fused_at < at + len(token.get("surface", ""))]
        if covering and covering[0].get("pos") == "助動詞":
            continue
        replacements[(start, "vowel_fused_adjective")] = {
            "original": original,
            "replacement": standard,
            "length": len(original),
            "keeps_lemma": True,
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

    return _apply_replacements(text, replacements)


_REPLACEMENT_RULE_NAMES = {
    "slang_adj": "slang-adjective",
    "slang_verb": "slang-verb",
    "unusual_name": "unusual-name",
    "word_exception": "word-exception",
    "emphatic_sokuon": "emphatic-sokuon",
    "kyujitai": "kyujitai-fold",
    "kanji_verb_frame": "kanji-verb-frame",
    "vowel_fused_adjective": "vowel-fused-adjective",
    "character_speech": "character-speech",
}


def _apply_replacements(
    text: str, replacements: dict[tuple[int, str], dict]
) -> tuple[str, dict[tuple[int, str], dict], tuple[str, ...]]:
    """Rewrite `text` with disjoint replacements and record their processed spans."""
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

    rules = tuple(dict.fromkeys(_REPLACEMENT_RULE_NAMES[category] for _, category in replacements))
    return text, replacements, rules


def analyze_preprocessed(text: str) -> tuple[list[dict], dict[tuple[int, str], dict], tuple[str, ...]]:
    """Analyze `text` through the preprocessing replacements.

    A replacement is restored token by token, so one the analysis does not keep
    inside a single token cannot be put back (にゃー read as ね + ゃー). Such a
    replacement is dropped and the text analyzed again without it, rather than
    leaving the restoration to guess where its characters went.
    """
    processed_text, replacements, rules = preprocess_for_mecab(text)
    while True:
        raw_tokens = mecab_analyze(processed_text)
        starts = []
        position = 0
        for token in raw_tokens:
            starts.append((position, position + len(token.get("surface", ""))))
            position += len(token.get("surface", ""))
        straddling = [
            key
            for key, replacement in replacements.items()
            if not any(
                token_start <= replacement["processed_start"]
                and replacement["processed_start"] + replacement["processed_length"] <= token_end
                for token_start, token_end in starts
            )
        ]
        if not straddling:
            return raw_tokens, replacements, rules
        kept = {
            key: {k: v for k, v in replacement.items() if not k.startswith("processed_")}
            for key, replacement in replacements.items()
            if key not in straddling
        }
        processed_text, replacements, rules = _apply_replacements(text, kept)


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
                        replacement.get("keeps_lemma", False),
                    )
                )

        if not patches:
            continue
        surface = token.get("surface", "")
        for local_start, local_end, original, _, _ in sorted(patches, reverse=True):
            surface = surface[:local_start] + original + surface[local_end:]
        token["surface"] = surface

        lemma = token.get("lemma", "")
        for _, _, original, standard, keeps_lemma in patches:
            # An emphatic spelling keeps the dictionary form as its lemma: かわいーー is
            # still かわいい. Restoring the original there would make the lemma a non-word
            # and contradict the lengthening rules, which already yield the plain form.
            # A colloquial sound change (すげえ for すごい) is the same word too.
            if keeps_lemma or _is_emphatic_spelling(original, standard, token.get("pos", "")):
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
