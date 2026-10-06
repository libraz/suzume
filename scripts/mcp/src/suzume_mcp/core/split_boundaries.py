"""Reading and boundary helpers shared by the split rules: renyokei bases and lexicalized morpheme splits."""

import regex

from .constants import COPULAR_PREDICATE_HEADS, FIXED_FUNCTION_SEARCH_UNITS, katakana_to_hiragana
from .mecab import mecab_analyze

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


# The past auxiliary is the frame that settles the cell, because there the choice
# is made straight off the reference lexicon: an attested 〜せる headword keeps the
# span whole (泣かせ+た, 知らせ+た) and everything else exposes the causative
# boundary (飛ば+せ+た, 乾か+せ+た).  Asking that frame is what lets the conditional
# agree with it without a word list.
_PAST_FRAME_PROBE = "た"


_CAUSATIVE_AUXILIARY_LEMMAS = frozenset({"せる", "させる"})


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


def _splits_causative_cell_before_past(surface: str) -> bool:
    """Whether the reference analyzer exposes the cell's boundary in the past frame."""
    probe = mecab_analyze(f"{surface}{_PAST_FRAME_PROBE}")
    return (
        len(probe) > 2
        and probe[0].get("surface") == surface[:-1]
        and probe[1].get("surface") == surface[-1:]
        # The cell is tagged a verb at this point in the pipeline, so the lemma is
        # what identifies it as the causative rather than the word class.
        and probe[1].get("lemma") in _CAUSATIVE_AUXILIARY_LEMMAS
    )


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

    # A verb headword spelled as a te-form plus a subsidiary verb (やってくる,
    # やって来る) is the same productive chain as 持って+き: the host is a
    # continuative verb on its own, and the reference itself reads the tail as
    # a subsidiary verb after any other te-form (し+て+くる).
    te_compound = regex.fullmatch(r"(.+?)([てで])(.+)", lemma) if pos == "動詞" else None
    if te_compound is not None:
        host, te, subsidiary_lemma = te_compound.groups()
        host_tokens = _reanalyze_exact(host)
        probe = _reanalyze_exact("して" + subsidiary_lemma)
        if (
            surface.startswith(host + te)
            and len(surface) > len(host + te)
            and host_tokens is not None
            and len(host_tokens) == 1
            and host_tokens[0].get("pos") == "動詞"
            and str(host_tokens[0].get("conj_form", "")).startswith("連用")
            and probe is not None
            and len(probe) == 3
            and probe[2].get("pos") == "動詞"
            and probe[2].get("pos_sub1") == "非自立"
            and probe[2].get("lemma") == subsidiary_lemma
        ):
            subsidiary = dict(token)
            subsidiary["surface"] = surface[len(host + te) :]
            subsidiary["lemma"] = subsidiary_lemma
            subsidiary["pos_sub1"] = "非自立"
            return [
                dict(host_tokens[0]),
                {"surface": te, "pos": "助詞", "pos_sub1": "接続助詞", "lemma": te},
                subsidiary,
            ]

    # A kanji-spelled negative adjective the reference lexicalized (済まない)
    # takes the analysis of its kana spelling, which the same dictionary reads
    # as the verb irrealis plus the negative auxiliary (すま+ない).
    if pos == "形容詞" and lemma.endswith("ない") and regex.search(r"\p{Han}", surface):
        isolated = _reanalyze_exact(surface)
        reading = token.get("reading") or (isolated[0].get("reading", "") if isolated and len(isolated) == 1 else "")
        kana = katakana_to_hiragana(reading)
        reanalyzed = _reanalyze_exact(kana)
        if (
            reanalyzed is not None
            and len(reanalyzed) == 2
            and reanalyzed[0].get("pos") == "動詞"
            and reanalyzed[1].get("pos") == "助動詞"
            and reanalyzed[1].get("lemma") == "ない"
        ):
            tail = reanalyzed[1]["surface"]
            stem = surface[: -len(tail)]
            base = base_from_mizenkei(stem)
            if surface.endswith(tail) and base is not None:
                return [
                    {"surface": stem, "pos": "動詞", "lemma": base},
                    {"surface": tail, "pos": "助動詞", "lemma": "ない"},
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


def _split_quotative_headword(token: dict, following: list[dict]) -> list[dict] | None:
    """Restore the quotative particle and verb inside a headword with no head."""
    surface = token.get("surface", "")
    if token.get("pos") != "助詞" or surface != token.get("lemma", surface):
        return None
    # Only an attributive position licenses the headword, and what stands there
    # is a nominal, a further attributive or a prefix. A particle or auxiliary
    # behind it closes the quotation instead, and the verb inside inflects
    # through that slot (と+いえ+ば, と+いっ+た).
    if not following or following[0].get("pos") not in ("助詞", "助動詞"):
        return None
    for split in range(1, len(surface)):
        head, tail = surface[:split], surface[split:]
        head_tokens = _reanalyze_exact(head)
        if head_tokens is None or len(head_tokens) != 1 or head_tokens[0].get("pos") != "助詞":
            continue
        tail_tokens = _reanalyze_exact(tail)
        if (
            tail_tokens is None
            or len(tail_tokens) != 1
            or tail_tokens[0].get("pos") != "動詞"
            or tail_tokens[0].get("conj_form") != "基本形"
        ):
            continue
        return [_as_independent_token(head_tokens[0]), _as_independent_token(tail_tokens[0])]
    return None


def _reads_as_one_noun(surface: str) -> bool:
    """Whether the reference dictionary reads `surface` alone as one noun."""
    tokens = mecab_analyze(surface)
    return len(tokens) == 1 and tokens[0].get("pos") == "名詞"
