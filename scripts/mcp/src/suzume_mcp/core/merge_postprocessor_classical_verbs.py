"""Classical verb, auxiliary, and copula post-processing passes."""

from functools import cache

import regex

from .constants import (
    HISTORICAL_KANA_RESPELLING,
    KYUJITAI_TO_SHINJITAI,
    LITERARY_VOLITIONAL_PARTICLE_COMPOUNDS,
)
from .mecab import is_single_token_of_pos, mecab_analyze
from .merge_postprocessor_common import _A_ROW_TO_U_ROW, _continuative_verb_tokens, _plain

_CLASSICAL_IRREALIS_AUX = "む"
_HIRAGANA_TAIL = regex.compile(r"\p{Hiragana}$")
_SINGLE_KANJI = regex.compile(r"^\p{Han}$")


def _bare_kanji_vowel_stem_lemma(surface: str) -> str | None:
    """The 一段/カ変 lemma whose irrealis is this bare kanji, or None.

    む attaches to an irrealis, and every consonant stem writes that cell with an
    a-row okurigana the host ends in. A vowel stem spelled entirely in its kanji
    has no okurigana at all (見, 寝, 得, 来), so the hiragana tail that identifies
    every other host is simply absent and the class has to be read off the lemma
    instead. The dictionary supplies it, and only 一段 and カ変 spell the irrealis
    as the bare stem — which is what keeps the ラ行五段 homographs out (照 and 練
    are not the irrealis of 照る and 練る).

    The dictionary holds no pre-1946 spellings, so the probe reads the modern
    character while the lemma keeps the one that was written (來 → 來る), which
    is how every other kyujitai reading is reported.
    """
    if not _SINGLE_KANJI.match(surface):
        return None
    probe = KYUJITAI_TO_SHINJITAI.get(surface, surface) + "る"
    tokens = mecab_analyze(probe)
    if len(tokens) != 1 or tokens[0].get("pos") != "動詞" or tokens[0].get("surface") != probe:
        return None
    return surface + "る" if tokens[0].get("conj_type", "").startswith(("一段", "カ変")) else None


def _promote_bare_kanji_host(merged: list[dict], previous: dict, lemma: str) -> None:
    """Give a bare vowel stem back the verb reading its irrealis position implies.

    With no okurigana to conjugate, the dictionary guesses the kanji from what
    surrounds it and lands on a different part of speech in each position — a
    bound suffix mid-sentence (人来), the temporal prefix at the head of one (来
    as in 来週), a bare noun where neither fits (得). None of the three can host
    む, so the position settles the reading whichever guess arrived.
    """
    if previous.get("pos") != "動詞":
        merged[-1] = {"surface": previous.get("surface", ""), "pos": "動詞", "lemma": lemma}


def _retag_suffix_without_host(tokens: list[dict]) -> list[dict]:
    """Drop the 接尾 subtype from a noun that has no host to attach to.

    A suffix attaches to a nominal, and an auxiliary is not one, so a noun in
    that position is the head of the relative clause the auxiliary closes. The
    dictionary reaches the suffix reading only when the classical chain in front
    defeats it (せ+し+水 against 見+し+水, which it reads correctly), which is why
    the repair rides with the rule that rebuilt the chain.
    """
    for index, token in enumerate(tokens):
        if index == 0 or token.get("pos") != "名詞" or token.get("pos_sub1") != "接尾":
            continue
        if tokens[index - 1].get("pos") == "助動詞":
            tokens[index] = {**token, "pos_sub1": "一般"}
    return tokens


def _postprocess_classical_mu(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Restore the boundary of the classical conjectural む.

    む attaches to a verb's 未然形 (読ま+む, 書か+む), but the reference dictionary
    carries no such auxiliary. It therefore either hands the irrealis kana to a
    lexical verb spanning the boundary (書+かむ, read as 噛む) or swallows む into
    a longer idiom (読ま+むとする). Rebuild 未然形 + む and re-analyze the rest.
    """
    merged: list[dict] = []
    idx = 0
    while idx < len(result):
        token = result[idx]
        surface = token.get("surface", "")
        previous = merged[-1] if merged else None
        # The irrealis kana was handed to the following verb (書 + かむ).
        if (
            previous is not None
            and token.get("pos") == "動詞"
            and len(surface) == 2
            and surface[0] in _A_ROW_TO_U_ROW
            and surface[1] == _CLASSICAL_IRREALIS_AUX
        ):
            stem = previous.get("surface", "")
            lemma = stem + _A_ROW_TO_U_ROW[surface[0]]
            if stem and is_single_token_of_pos(lemma, "動詞"):
                merged[-1] = {"surface": stem + surface[0], "pos": "動詞", "lemma": lemma}
                merged.append({"surface": surface[1], "pos": "助動詞", "lemma": surface[1]})
                idx += 1
                if applied_rule is None:
                    applied_rule = "classical-mu-boundary"
                continue
        # む opened a longer idiom the dictionary lists as one word (むとする).
        # The stem it attaches to is a 未然形, which ends in an a-row kana for a
        # consonant stem (行か) and in an i-/e-row kana for a vowel stem
        # (見え, 流れ).  The dictionary holds no irrealis for the vowel-stem
        # class and reads that stem as a deverbal noun, so the POS cannot carry
        # the test — a content word ending in hiragana is the whole condition,
        # and the conjugation class drops out of it.  The one host with no
        # hiragana to test is the vowel stem written wholly in its kanji, which
        # the lemma identifies instead.  Any other kanji-final host is left out
        # on purpose: there the volitional split rule reads the tail in context
        # and keeps the case particle that re-analysis would lose.
        bare_kanji_lemma = _bare_kanji_vowel_stem_lemma(previous.get("surface", "")) if previous is not None else None
        if (
            previous is not None
            and len(surface) > 1
            and surface[0] == _CLASSICAL_IRREALIS_AUX
            and (
                bare_kanji_lemma is not None
                or (previous.get("pos") in ("動詞", "名詞") and _HIRAGANA_TAIL.search(previous.get("surface", "")))
            )
        ):
            # A noun cannot host む, so the deverbal reading the dictionary
            # produced for the vowel stem has to be undone as well: the stem plus
            # る is the vowel-stem verb it was cut from (流れ → 流れる).  Confirm
            # that lemma against the dictionary rather than assuming it, so a
            # nominal host the rule reached by another route keeps its POS.
            if bare_kanji_lemma is not None:
                _promote_bare_kanji_host(merged, previous, bare_kanji_lemma)
            elif previous.get("pos") == "名詞":
                stem = previous.get("surface", "")
                lemma = stem + "る"
                if is_single_token_of_pos(lemma, "動詞"):
                    merged[-1] = {"surface": stem, "pos": "動詞", "lemma": lemma}
            merged.append({"surface": surface[0], "pos": "助動詞", "lemma": surface[0]})
            # Re-analysis loses the context the fused token supplied, and a
            # closed-class tail read on its own comes back as a noun (と after
            # 見むと). Those sequences are already named as splits of the fused
            # compound, so take the reading from there and leave the analyzer to
            # the open-class tails it can read unaided (むとする).
            compound = LITERARY_VOLITIONAL_PARTICLE_COMPOUNDS.get(surface)
            if compound is not None:
                merged.append({"surface": compound[1], "pos": "助詞", "lemma": compound[1]})
            else:
                merged.extend(mecab_analyze(surface[1:]))
            idx += 1
            if applied_rule is None:
                applied_rule = "classical-mu-boundary"
            continue
        # The boundary already stands, but with no auxiliary entry to land on the
        # dictionary files the bare mora as an interjection (成ら+む, 待た+む).  An
        # interjection cannot follow an irrealis stem, so the position settles the
        # POS and the auxiliary keeps one reading across every context.
        if (
            previous is not None
            and surface == _CLASSICAL_IRREALIS_AUX
            and token.get("pos") != "助動詞"
            and (
                bare_kanji_lemma is not None
                or (previous.get("pos") == "動詞" and previous.get("surface", "")[-1:] in _A_ROW_TO_U_ROW)
            )
        ):
            if bare_kanji_lemma is not None:
                _promote_bare_kanji_host(merged, previous, bare_kanji_lemma)
            merged.append({"surface": surface, "pos": "助動詞", "lemma": surface})
            idx += 1
            if applied_rule is None:
                applied_rule = "classical-mu-boundary"
            continue
        merged.append(token)
        idx += 1
    if applied_rule == "classical-mu-boundary":
        _retag_suffix_without_host(merged)
    return merged, applied_rule


_E_ROW_TO_U_ROW = {
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
_CONCESSIVE_PARTICLES = ("ど", "ども")


_TOMO_CONCESSIVE_AUXILIARIES = frozenset({"ず"})


def _postprocess_tomo_particle(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Keep とも whole where it is one particle rather than と plus も.

    とも is a single particle in two frames: after a counted quantity it is the
    universal quantifier (二人とも), and after the negative auxiliary ず or an
    adjective adverbial form it is the concessive conjunctive particle
    (読まずとも, 少なくとも). The reference dictionary lexicalizes only
    the handful of quantities it happens to list and splits the rest, so the
    boundary is restored from the host instead. Every other host keeps the
    case particle と plus the binding particle も (願いとも違う).
    """
    merged: list[dict] = []
    skip_next = False
    for idx, token in enumerate(result):
        if skip_next:
            skip_next = False
            continue
        following = result[idx + 1] if idx + 1 < len(result) else None
        host = merged[-1] if merged else None
        if (
            following is not None
            and host is not None
            and token.get("surface") == "と"
            and token.get("pos") == "助詞"
            and following.get("surface") == "も"
            and following.get("pos") == "助詞"
        ):
            host_surface = host.get("surface", "")
            host_pos = host.get("pos", "")
            quantifier_host = host_pos == "名詞" and host.get("pos_sub1") == "数"
            concessive_host = (host_pos == "助動詞" and host_surface in _TOMO_CONCESSIVE_AUXILIARIES) or (
                host_pos == "形容詞" and host_surface.endswith("く")
            )
            if quantifier_host or concessive_host:
                merged.append({"surface": "とも", "pos": "助詞", "lemma": "とも"})
                skip_next = True
                if applied_rule is None:
                    applied_rule = "tomo-particle-boundary"
                continue
        merged.append(token)
    return merged, applied_rule


def _postprocess_izenkei_concessive(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Give the 已然形 before a concessive conjunction its plain verb lemma.

    ど/ども select the 已然形, which the modern paradigm spells like the
    hypothetical (書け+ど, 飲め+ど). The potential verb of the same stem reaches
    that conjunction only through its own 已然形 (書けれ+ど), so a bare e-row form
    here belongs to the plain verb. The reference dictionary splits the two
    readings by row, giving 飲む for one and 書ける for the other.
    """
    tagged: list[dict] = []
    for idx, token in enumerate(result):
        following = result[idx + 1] if idx + 1 < len(result) else None
        surface = token.get("surface", "")
        if (
            following is not None
            and following.get("surface") in _CONCESSIVE_PARTICLES
            and following.get("pos") == "助詞"
            and token.get("pos") == "動詞"
            and token.get("lemma") == surface + "る"
            and surface[-1:] in _E_ROW_TO_U_ROW
        ):
            plain = surface[:-1] + _E_ROW_TO_U_ROW[surface[-1]]
            if is_single_token_of_pos(plain, "動詞"):
                tagged.append({**token, "lemma": plain})
                if applied_rule is None:
                    applied_rule = "izenkei-concessive-lemma"
                continue
        tagged.append(token)
    return tagged, applied_rule


_CLASSICAL_CAUSATIVE_FORMS = ("しむ", "しめ", "しむる", "しむれ")


def _postprocess_classical_shimu(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Tag the classical causative しむ as an auxiliary.

    しむ conjugates 下二段 and attaches to the same 未然形 as the modern せる, but
    the reference dictionary has no such auxiliary and falls back to a lexical
    verb of the same spelling (書か+しむ). After an irrealis there is no verb
    reading available, so the cell is the auxiliary.
    """
    tagged: list[dict] = []
    for idx, token in enumerate(result):
        previous = result[idx - 1] if idx > 0 else None
        if (
            previous is not None
            and previous.get("pos") == "動詞"
            and previous.get("surface", "")[-1:] in _A_ROW_TO_U_ROW
            and token.get("surface") in _CLASSICAL_CAUSATIVE_FORMS
        ):
            tagged.append({**token, "pos": "助動詞", "lemma": "しむ"})
            if applied_rule is None:
                applied_rule = "classical-shimu-auxiliary"
            continue
        tagged.append(token)
    return tagged, applied_rule


_PAST_CONJECTURAL = "けむ"
# The auxiliary attaches to a continuative, which a preceding predicate or
# auxiliary already supplies; a nominal there has to be read back first.
_PAST_CONJECTURAL_HOST_POS = frozenset({"動詞", "助動詞"})


def _postprocess_classical_kemu(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Read けむ as the past-conjectural auxiliary it is.

    The reference dictionary has no cell for it and falls back on whatever the
    position suggests — a particle where the clause ends, a noun inside one —
    and neither can govern a verb. With the auxiliary reading gone the
    continuative in front loses its own verb reading too and is absorbed into a
    nominal, so the repair has to restore both.
    """
    normalized: list[dict] = []
    for token in result:
        if token.get("surface") != _PAST_CONJECTURAL or token.get("pos") == "助動詞" or not normalized:
            normalized.append(token)
            continue
        previous = normalized[-1]
        if previous.get("pos") not in _PAST_CONJECTURAL_HOST_POS:
            recovered = (
                _continuative_verb_tokens(previous.get("surface", "")) if previous.get("pos") == "名詞" else None
            )
            if recovered is None:
                normalized.append(token)
                continue
            normalized[-1:] = recovered
        normalized.append({"surface": _PAST_CONJECTURAL, "pos": "助動詞", "lemma": _PAST_CONJECTURAL})
        if applied_rule is None:
            applied_rule = "classical-past-conjectural"
    return normalized, applied_rule


_CLASSICAL_COPULA_BASE = "なる"
_CLASSICAL_COPULA_LEMMA = "なり"


def _postprocess_nominal_copula_naru(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Split a nominal off the ra-row copula the reference dictionary fused it to.

    The classical copula inflects like the ra-row godan verb なる, and the
    reference dictionary reads it as that lexical verb — fusing the nominal in
    front of it into one token whenever the pair is not a headword, and giving
    it a lemma that is not a word (ほかなる). Whether that happens is lexical
    accident: the same construction stays apart in kanji (外+なら) and before a
    different auxiliary (ほか+なり+ませ+ん).

    The lemma is what settles it. A real ra-row verb is a headword and comes
    back from the dictionary as one token (異なる, 重なる, 連なる); a fused one
    comes back as its parts, and that split is where the surface divides too.
    """
    normalized: list[dict] = []
    for token in result:
        lemma = token.get("lemma") or ""
        surface = token.get("surface", "")
        if token.get("pos") != "動詞" or lemma == _CLASSICAL_COPULA_BASE or not lemma.endswith(_CLASSICAL_COPULA_BASE):
            normalized.append(token)
            continue
        probe = mecab_analyze(lemma)
        head = probe[0].get("surface", "") if probe else ""
        if len(probe) < 2 or probe[0].get("pos") != "名詞" or not head or not surface.startswith(head):
            normalized.append(token)
            continue
        normalized.append(_plain(probe[0]))
        # Freed from the nominal, the cell is what it always was: the classical
        # copula after a bare nominal host, tagged exactly as the sibling rule
        # tags it when the dictionary managed to keep the two apart.
        normalized.append(
            {
                "surface": surface[len(head) :],
                "pos": "助動詞",
                "conj_type": "文語・ナリ",
                "lemma": _CLASSICAL_COPULA_LEMMA,
            },
        )
        if applied_rule is None:
            applied_rule = "nominal-copula-naru"
    return normalized, applied_rule


_PAST_KI = "き"


def _postprocess_classical_ki(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Restore the continuative the classical past き attaches to.

    The auxiliary closes its clause and selects a continuative, so what sits in
    front of it is a verb whatever the dictionary made of it. Which way that
    goes wrong depends only on whether the dictionary happens to hold the kanji
    pair as a noun: the same construction comes out as a nominal plus the
    auxiliary (山見+き), as a nominal plus a fabricated doubled kana (花咲+きき),
    or correctly (道行き+き). The auxiliary decides instead of the lexicon.
    """
    normalized: list[dict] = []
    index = 0
    while index < len(result):
        token = result[index]
        surface = token.get("surface", "")
        closes_clause = index + 1 == len(result) or result[index + 1].get("pos") == "記号"
        # The auxiliary is either its own token or the tail of a nominal that
        # swallowed it, in which case the mora in front of it belongs to the
        # continuative and rejoins the host.
        # Only a nominal in front of the auxiliary needs repairing. A predicate
        # reading already there is the analysis this rule would rebuild, and an
        # adjective's カリ cell in particular must not be reopened (遅かり+き),
        # even though its ending happens to probe as a verb of its own.
        previous_pos = normalized[-1].get("pos") if normalized else None
        if surface == _PAST_KI and token.get("pos") == "助動詞" and previous_pos == "名詞":
            carried = ""
        elif closes_clause and token.get("pos") == "名詞" and len(surface) > 1 and surface.endswith(_PAST_KI):
            carried = surface[:-1]
        else:
            normalized.append(token)
            index += 1
            continue
        host = (normalized[-1].get("surface", "") if normalized else "") + carried
        recovered = _continuative_verb_tokens(host) if host else None
        # A mora carried out of the nominal is the continuative's ending, so the
        # verb it belongs to has to reach back past it into the token in front.
        # One built entirely from the carried material is not a verb the nominal
        # was hiding — it is a new one, and the boundary that was already there
        # stands (ほんと|すき, where the probe is happy to read す as する).
        if recovered is not None and carried and len(recovered[-1].get("surface", "")) <= len(carried):
            recovered = None
        if recovered is None:
            normalized.append(token)
            index += 1
            continue
        if normalized:
            normalized.pop()
        normalized.extend(recovered)
        normalized.append({"surface": _PAST_KI, "pos": "助動詞", "lemma": _PAST_KI})
        index += 1
        if applied_rule is None:
            applied_rule = "classical-past-ki"
    return normalized, applied_rule


_HA_ROW_TAILS = ("は", "ひ", "ふ", "へ")
_HA_ROW_DETACHED_TAILS = ("ひ", "ふ", "へ")
_HA_ROW_STEM_POS = ("名詞", "動詞", "形容詞", "副詞", "接尾辞")


_HA_ROW_FRAME_STEM = "思"


def _ha_row_fabricated_ichidan(token: dict) -> bool:
    """Whether a verb token's lemma is the 一段 reading invented for a ハ行 cell."""
    surface = token.get("surface", "")
    return token.get("pos") == "動詞" and token.get("lemma") == surface + "る"


def _ha_row_cell_auxiliary(surface: str) -> dict | None:
    """Split a ハ行 cell from the auxiliary a fabricated verb swallowed it into.

    The cell kana does not always fall out on its own: where it opens a longer
    run, the reference dictionary reads the whole run as an unrelated 五段 verb
    (適+ひたる as 浸る). Writing the same run behind the one stem whose ハ行 row
    the dictionary does carry settles what the cell actually hosts, and that
    frame cannot be misread because the row is listed for it.
    """
    if len(surface) < 2 or surface[0] not in _HA_ROW_DETACHED_TAILS:
        return None
    probe = mecab_analyze(_HA_ROW_FRAME_STEM + surface)
    if len(probe) != 2 or probe[0].get("surface") != _HA_ROW_FRAME_STEM + surface[0]:
        return None
    if probe[0].get("pos") != "動詞" or probe[0].get("conj_type") != "四段・ハ行":
        return None
    return probe[1] if probe[1].get("pos") == "助動詞" else None


def _postprocess_ha_row_godan(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Rebuild the classical ハ行四段 conjugation (候ふ, 移ろひ, 思へ).

    ハ行四段 is the historical-kana spelling of the modern ワ行五段 row, so
    は/ひ/ふ/へ are cells of one verb whose terminal form is the lemma. The
    reference dictionary carries the row only for the handful of verbs it
    happens to list (思ふ); everywhere else the cell kana falls out as a separate
    one-mora verb with an invented 一段 lemma (候+ふ as ふる, 移ろ+ひ as ひる), or
    the whole cell keeps such a lemma (思へ as 思へる). Reattach the detached
    kana to its stem and give both the row's own terminal ふ.
    """
    merged: list[dict] = []
    idx = 0
    while idx < len(result):
        token = result[idx]
        following = result[idx + 1] if idx + 1 < len(result) else None
        if (
            following is not None
            and following.get("surface") in _HA_ROW_DETACHED_TAILS
            and _ha_row_fabricated_ichidan(following)
            and token.get("pos") in _HA_ROW_STEM_POS
        ):
            stem = token.get("surface", "")
            merged.append({"surface": stem + following["surface"], "pos": "動詞", "lemma": stem + "ふ"})
            idx += 2
            # The terminal cell in front of a nominal is the adnominal, and an
            # adnominal takes a head noun, so the following word is not the bound
            # counter the dictionary reads it as elsewhere (舞ふ+間 against 三+間).
            if (
                following["surface"] == "ふ"
                and idx < len(result)
                and result[idx].get("pos") == "名詞"
                and result[idx].get("pos_sub1") == "接尾"
            ):
                merged.append({**result[idx], "pos_sub1": "一般"})
                idx += 1
            if applied_rule is None:
                applied_rule = "ha-row-godan-conjugation"
            continue
        # A longer fabricated verb hides the same cell behind its own opening
        # kana, and what follows the cell there is an auxiliary (適+ひ+たる).
        if following is not None and following.get("pos") == "動詞" and token.get("pos") in _HA_ROW_STEM_POS:
            auxiliary = _ha_row_cell_auxiliary(following.get("surface", ""))
            if auxiliary is not None:
                stem = token.get("surface", "")
                merged.append({"surface": stem + following["surface"][0], "pos": "動詞", "lemma": stem + "ふ"})
                merged.append(auxiliary)
                idx += 2
                if applied_rule is None:
                    applied_rule = "ha-row-godan-conjugation"
                continue
        # The classical honorific stem is tagged as a suffix, and its imperative
        # cell then falls out as the direction particle (給+へ). A suffix never
        # takes that particle, so the pair is one 命令形 of the ハ行四段 verb.
        if (
            following is not None
            and token.get("surface") == "給"
            and token.get("pos") == "名詞"
            and token.get("pos_sub1") == "接尾"
            and following.get("surface") in _HA_ROW_DETACHED_TAILS
            and following.get("pos") == "助詞"
        ):
            stem = token.get("surface", "")
            merged.append({"surface": stem + following["surface"], "pos": "動詞", "lemma": stem + "ふ"})
            idx += 2
            if applied_rule is None:
                applied_rule = "ha-row-godan-conjugation"
            continue
        surface = token.get("surface", "")
        if len(surface) > 1 and surface.endswith(_HA_ROW_TAILS) and _ha_row_fabricated_ichidan(token):
            merged.append({**token, "lemma": surface[:-1] + "ふ"})
            idx += 1
            if applied_rule is None:
                applied_rule = "ha-row-godan-conjugation"
            continue
        merged.append(token)
        idx += 1
    return merged, applied_rule


# The 終止形 of a classical 二段 verb is its kanji stem plus one U-row kana.  The
# modern descendant is 一段, so the same stem takes the row's E-row or I-row kana
# plus る, and that headword is what the reference dictionary does carry.  The ダ行
# row is the one whose kana was absorbed into the kanji's own reading (出づ -> 出る),
# so there the modern headword is the bare stem plus る.
_NIDAN_TERMINAL_ROWS: dict[str, tuple[str, ...]] = {
    "う": ("え", "い"),
    "く": ("け", "き"),
    "ぐ": ("げ", "ぎ"),
    "す": ("せ", "し"),
    "つ": ("て", "ち"),
    "づ": ("で", "じ", ""),
    "ぬ": ("ね", "に"),
    "ふ": ("え", "い"),
    "ぶ": ("べ", "び"),
    "む": ("め", "み"),
    "ゆ": ("え", "い"),
    "る": ("れ", "り"),
}
# Those same kana also spell classical auxiliaries that attach to a 未然形 or a
# 連用形 (見|つ, 見|ぬ, 見|む).  After an inflected verb the kana is the auxiliary,
# so only a stem the dictionary did not inflect can head a 二段 terminal cell.
_NIDAN_AUXILIARY_HOMOGRAPHS = frozenset({"す", "つ", "ぬ", "ふ", "む", "る"})
_NIDAN_INFLECTED_STEM_FORMS = frozenset({"未然形", "連用形"})


_NIDAN_CELL = regex.compile(rf"^(\p{{Han}}[\p{{Han}}\p{{Hiragana}}]*?)([{''.join(_NIDAN_TERMINAL_ROWS)}])([るれ]?)$")
# The kana a 二段 終止形 ends in, for callers that only see a rebuilt cell.
NIDAN_TERMINAL_KANA = frozenset(_NIDAN_TERMINAL_ROWS)


@cache
def _nidan_terminal_lemma(stem: str, terminal: str) -> str | None:
    """Return the 終止形 when a stem plus a U-row kana is a classical 二段 verb."""
    for vowel in _NIDAN_TERMINAL_ROWS[terminal]:
        if is_single_token_of_pos(stem + vowel + "る", "動詞"):
            return stem + terminal
    return None


def _nidan_cell_match(token: dict, following: dict) -> tuple[regex.Match, str] | None:
    """Match a 二段 cell across a token pair, allowing an unanalyzed tail.

    Where the cell ends the phrase the dictionary reads its kana as one unknown
    noun and glues whatever follows onto that token (出|づるか, 出|づまじ).  An
    unknown word carries no analysis to preserve, so the cell may be matched
    against a prefix of it and the rest handed back to the dictionary.
    """
    head, tail = token.get("surface", ""), following.get("surface", "")
    cell = _NIDAN_CELL.match(head + tail)
    if cell is not None:
        return cell, ""
    if following.get("lemma") not in ("*", "", None):
        return None
    for width in (2, 1):  # the cell's kana tail is the terminal plus an optional る
        if width < len(tail) and (cell := _NIDAN_CELL.match(head + tail[:width])) is not None:
            return cell, tail[width:]
    return None


def nidan_cell(token: dict, following: dict | None) -> tuple[str, str, str] | None:
    """Return (surface, 終止形, remainder) when two tokens spell one 二段 finite cell.

    終止形 is the stem plus one U-row kana, 連体形 adds る and 已然形 adds れ.  The
    stem keeps whatever 送り仮名 the modern headword carries (聞こ|ゆ), so it is
    matched as a kanji head plus the kana that follow, shortest first: the longest
    stem would swallow the 連体形 る of 消|ゆ|る.  Asking the dictionary for the
    modern 一段 headword the same stem builds decides whether the pair is a verb,
    without listing the classical paradigm.  The remainder is whatever the window
    matched past the cell, and is left for the caller to re-analyze.
    """
    if following is None:
        return None
    matched = _nidan_cell_match(token, following)
    if matched is None:
        return None
    cell, remainder = matched
    stem, terminal, inflection = cell.groups()
    # The same kana spell classical auxiliaries that attach to a 未然形 or a
    # 連用形 (見|つ, 見|ぬ, 見|つる).  After a stem the dictionary inflected, the
    # kana is that auxiliary and not part of the verb — unless the stem's lemma is
    # the cell itself (流|るる is read as the 未然形 of 流る), where the kana is
    # that verb's own 送り仮名.
    if (
        terminal in _NIDAN_AUXILIARY_HOMOGRAPHS
        and token.get("pos") == "動詞"
        and token.get("conj_form") in _NIDAN_INFLECTED_STEM_FORMS
        and token.get("lemma") != stem + terminal
    ):
        return None
    lemma = _nidan_terminal_lemma(stem, terminal)
    return None if lemma is None else (lemma + inflection, lemma, remainder)


def _postprocess_nidan_cell(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Rebuild the finite cells of a classical 二段 verb (受く, 越ゆ, 求むる, 聞こゆれ).

    The reference dictionary carries only the modern 一段 headword, so the kana
    falls out as whatever else it can spell — an adjective stem (受+く as くい), a
    bare noun (越+ゆ), the カ変 くる, or the 完了 つ — and the kanji is left as a
    noun that is not a word on its own.  Where the two tokens fall is not fixed
    (求む|る but 見|ゆる), so the window is matched on its combined surface.
    """
    merged: list[dict] = []
    idx = 0
    while idx < len(result):
        token = result[idx]
        cell = nidan_cell(token, result[idx + 1] if idx + 1 < len(result) else None)
        if cell is not None:
            surface, lemma, remainder = cell
            merged.append({"surface": surface, "pos": "動詞", "lemma": lemma})
            if remainder:
                merged.extend(mecab_analyze(remainder))
            idx += 2
            if applied_rule is None:
                applied_rule = "classical-nidan-cell"
            continue
        merged.append(token)
        idx += 1
    return merged, applied_rule


# The closed set of 係助詞 a 係り結び opens with.  The reference dictionary
# lexicalizes one demonstrative + 係助詞 pair as an adverb, which buries the
# particle that governs the clause's final form.
_KAKARI_PARTICLES = ("ぞ", "こそ", "なむ", "や")


@cache
def _reads_as_pronoun(surface: str) -> bool:
    """Whether the reference dictionary reads a surface as exactly one pronoun."""
    tokens = mecab_analyze(surface)
    return len(tokens) == 1 and tokens[0].get("pos_sub1") == "代名詞" and tokens[0].get("surface") == surface


def _postprocess_kakari_pronoun_split(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Split a demonstrative that the dictionary fused with its 係助詞 (これぞ).

    A 係助詞 governs the form its clause ends in, so burying it inside a
    lexicalized adverb loses the only token that explains the 結び.  The
    demonstrative in front of it is a pronoun the dictionary carries on its own.
    """
    split: list[dict] = []
    for index, token in enumerate(result):
        surface = token.get("surface", "")
        # The same particles are read as interjections where the dictionary has
        # no entry for their 係助詞 use. A nominal in front of one is the phrase
        # it marks, which is the position an interjection never fills.
        previous = result[index - 1] if index > 0 else None
        if (
            token.get("pos") == "感動詞"
            and surface in _KAKARI_PARTICLES
            and previous is not None
            and previous.get("pos") == "名詞"
        ):
            split.append({"surface": surface, "pos": "助詞", "pos_sub1": "係助詞", "lemma": surface})
            if applied_rule is None:
                applied_rule = "kakari-pronoun-split"
            continue
        particle = next((p for p in _KAKARI_PARTICLES if surface.endswith(p)), None)
        head = surface[: -len(particle)] if particle else ""
        if token.get("pos") == "副詞" and head and _reads_as_pronoun(head):
            split.append({"surface": head, "pos": "名詞", "pos_sub1": "代名詞", "lemma": head})
            split.append({"surface": particle, "pos": "助詞", "pos_sub1": "係助詞", "lemma": particle})
            if applied_rule is None:
                applied_rule = "kakari-pronoun-split"
            continue
        split.append(token)
    return split, applied_rule


@cache
def _modern_kana_word(surface: str) -> dict | None:
    """Return the modern-spelling reading of a historical-kana surface, if it is one word."""
    modern = surface.translate(HISTORICAL_KANA_RESPELLING)
    if modern == surface:
        return None
    tokens = mecab_analyze(modern)
    if len(tokens) != 1 or tokens[0].get("surface") != modern or tokens[0].get("pos") == "動詞":
        return None
    return tokens[0]


def _postprocess_historical_kana_word(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Read a historical-kana word through its modern spelling (いづれ, まづ).

    The reference dictionary carries the modern orthography, so a word written
    with づ/ぢ/ゐ/ゑ falls back on whatever inflected cell those kana happen to
    complete — いづれ becomes the 已然形 of 出づ rather than the pronoun it
    spells. Respelling is a one-for-one substitution, so asking the dictionary
    for the modern form recovers the word's own class. A modern form that is
    itself a verb is left alone: there the fallback and the word coincide.
    """
    retagged: list[dict] = []
    for token in result:
        modern = _modern_kana_word(token.get("surface", "")) if token.get("pos") == "動詞" else None
        if modern is None:
            retagged.append(token)
            continue
        retagged.append({**modern, "surface": token["surface"], "lemma": token["surface"]})
        if applied_rule is None:
            applied_rule = "historical-kana-word"
    return retagged, applied_rule


def _postprocess_nominal_classical_copula(
    result: list[dict], applied_rule: str | None
) -> tuple[list[dict], str | None]:
    """Tag なる/なり directly after a nominal as the classical copula (道なる).

    Modern なる needs the case particle に in front of it, so a bare nominal
    host leaves only the literary copula. The reference dictionary carries the
    lexical verb for that spelling and reaches for it whenever its own headword
    list happens to miss the auxiliary reading.
    """
    tagged: list[dict] = []
    for index, token in enumerate(result):
        previous = result[index - 1] if index > 0 else None
        if (
            token.get("pos") == "動詞"
            and token.get("lemma") == "なる"
            and previous is not None
            and previous.get("pos") == "名詞"
        ):
            tagged.append({**token, "pos": "助動詞", "conj_type": "文語・ナリ", "lemma": "なり"})
            if applied_rule is None:
                applied_rule = "nominal-classical-copula"
            continue
        tagged.append(token)
    return tagged, applied_rule
