"""Context-dependent classical inflection and auxiliary restoration."""

from .mecab import mecab_analyze
from .postprocessor_common import reports_mutation
from .split_rules import base_from_renyokei


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


def _verb_continuative_reading(surface: str) -> dict | None:
    """The verb reading of a surface that spells a continuative, else None.

    The reference dictionary names the cell only in the raw analysis, which the
    mapped tokens no longer carry, so ask it in an environment that admits
    nothing else: the polite ます selects the continuative and leaves a finite
    form unchanged (見+ます against やる+ます). The probe stands on its own, so a
    surface the dictionary also carries as a nominalized headword (咲き, 香り)
    answers here even where the mapped token was given that noun reading.
    """

    probe = mecab_analyze(surface + "ます")
    if (
        len(probe) == 2
        and probe[0].get("surface") == surface
        and probe[0].get("pos") == "動詞"
        and probe[0].get("conj_form") == "連用形"
    ):
        return probe[0]
    return None


def _spells_verb_continuative(surface: str) -> bool:
    """Whether this surface is a verb's continuative rather than a finite cell."""
    return _verb_continuative_reading(surface) is not None


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


_CLASSICAL_RAMU = "らむ"
# The cells the present conjecture attaches to are terminals, and a terminal is a
# predicate of one of these classes.  A nominal in that slot is what the plural
# suffix of the same spelling follows instead (子供ら), which is why the host is
# tested rather than the two kana alone.
_RAMU_HOST_POS = ("Verb", "Adjective", "Auxiliary")


@reports_mutation
def postprocess_classical_ramu_boundary(tokens: list[dict]) -> None:
    """Repair the boundaries around the classical present conjecture らむ.

    The reference dictionary carries no entry for the auxiliary, so its two kana
    come apart wherever they stand.  Behind a one-kanji godan-ka stem they are
    absorbed into a verb that spells the boundary away (く+らむ read as くらむ);
    everywhere else they are left as a pair of nominals with no lemma between them
    (見る+ら+む, 楽しかる+ら+む), which contradicts the same auxiliary being read
    whole after the paradigms the dictionary does cover (行きたり+らむ).  The form
    selects a terminal, so the predicate in front is what identifies it in both
    shapes.
    """
    idx = 1
    while idx < len(tokens):
        token = tokens[idx]
        previous = tokens[idx - 1]
        if token.get("surface") == "くらむ" and token.get("pos") == "Verb":
            stem = previous.get("surface", "")
            if previous.get("pos") == "Noun" and len(stem) == 1:
                previous["surface"] = f"{stem}く"
                previous["pos"] = "Verb"
                previous["lemma"] = f"{stem}く"
                token["surface"] = _CLASSICAL_RAMU
                token["pos"] = "Auxiliary"
                token["lemma"] = _CLASSICAL_RAMU
        elif (
            token.get("surface") == "ら"
            and idx + 1 < len(tokens)
            and tokens[idx + 1].get("surface") == "む"
            and tokens[idx + 1].get("pos") == "Noun"
            and previous.get("pos") in _RAMU_HOST_POS
        ):
            token["surface"] = _CLASSICAL_RAMU
            token["pos"] = "Auxiliary"
            token["lemma"] = _CLASSICAL_RAMU
            del tokens[idx + 1]
        idx += 1


_DESIDERATIVE_MAHOSHI_TAILS = ("ほし", "ほしき", "ほしく", "ほしけれ", "ほしから", "ほしかり")


def postprocess_classical_desiderative_aux(tokens: list[dict]) -> bool:
    """Rebuild the classical desiderative まほし the analyzer splits as ま + ほし.

    まほし is one auxiliary in every cell (見+まほし, 見+まほしき+もの,
    行か+まほしけれ); its ほし is not the modern adjective ほしい.
    """
    changed = False
    idx = 1
    while idx + 1 < len(tokens):
        token, tail = tokens[idx], tokens[idx + 1]
        if (
            tokens[idx - 1].get("pos") == "Verb"
            and token.get("surface") == "ま"
            and tail.get("surface") in _DESIDERATIVE_MAHOSHI_TAILS
        ):
            tokens[idx : idx + 2] = [{"surface": "ま" + tail["surface"], "pos": "Auxiliary", "lemma": "まほし"}]
            changed = True
        idx += 1
    return changed


_HONORIFIC_TAMAFU_CELLS = ("たまふ", "たまひ", "たまへ", "たまは", "たまう", "たまい", "たまえ", "たまわ")


def _tamafu_lemma(cell: str) -> str:
    """The ha-row cells belong to classical たまふ, the wa/a-row ones to modern たまう."""
    return "たまふ" if cell[-1] in "ふひへは" else "たまう"


def postprocess_classical_honorific_aux(tokens: list[dict]) -> bool:
    """Rebuild the classical honorific auxiliary たまふ after a continuative.

    The analyzer splits its terminal as た + ま + ふ and reads the other cells
    as the lexical verb たまふ (give); after a verb continuative every cell is
    the one honorific auxiliary (知らせ+たまふ, 書き+たまひ+し).
    """
    changed = False
    idx = 1
    while idx < len(tokens):
        if tokens[idx - 1].get("pos") != "Verb":
            idx += 1
            continue
        surfaces = "".join(token.get("surface", "") for token in tokens[idx : idx + 3])
        if (
            idx + 2 < len(tokens)
            and tokens[idx].get("surface") == "た"
            and tokens[idx + 1].get("surface") in ("ま", "まひ", "まへ")
            and surfaces in _HONORIFIC_TAMAFU_CELLS
        ):
            tokens[idx : idx + 3] = [{"surface": surfaces, "pos": "Auxiliary", "lemma": _tamafu_lemma(surfaces)}]
            changed = True
        elif (
            idx + 1 < len(tokens)
            and tokens[idx].get("surface") in ("た", "たま")
            and (tokens[idx].get("surface", "") + tokens[idx + 1].get("surface", "")) in _HONORIFIC_TAMAFU_CELLS
        ):
            cell = tokens[idx]["surface"] + tokens[idx + 1]["surface"]
            tokens[idx : idx + 2] = [{"surface": cell, "pos": "Auxiliary", "lemma": _tamafu_lemma(cell)}]
            changed = True
        elif tokens[idx].get("surface") in _HONORIFIC_TAMAFU_CELLS and tokens[idx].get("pos") != "Auxiliary":
            tokens[idx]["pos"] = "Auxiliary"
            tokens[idx]["lemma"] = _tamafu_lemma(tokens[idx]["surface"])
            changed = True
        # The continuative たまひ takes the classical past き (書き+たまひ+し).
        if (
            tokens[idx].get("lemma") in ("たまふ", "たまう")
            and tokens[idx].get("surface", "").endswith("ひ")
            and idx + 1 < len(tokens)
            and tokens[idx + 1].get("surface") == "し"
            and tokens[idx + 1].get("pos") != "Auxiliary"
        ):
            tokens[idx + 1]["pos"] = "Auxiliary"
            tokens[idx + 1]["lemma"] = "き"
            changed = True
        idx += 1
    return changed


def _is_listing_tari(tokens: list[dict], idx: int) -> bool:
    """Whether たり/だり at @p idx is the listing particle rather than the perfect.

    The classical perfect たり takes a plain continuative (咲き+たり); an onbin
    stem (行っ+たり, だっ+たり, 読ん+だり) only takes the listing particle, and so
    does the second member of a pair already opened in the sentence.
    """
    if idx == 0:
        return False
    previous_surface = tokens[idx - 1].get("surface", "")
    if previous_surface.endswith(("っ", "ん", "い")):
        return True
    return any(token.get("surface") in ("たり", "だり") for token in tokens[:idx])


def postprocess_listing_tari(tokens: list[dict]) -> bool:
    """Tag the listing たり/だり as the particle wherever the frame identifies it."""
    changed = False
    for idx, token in enumerate(tokens):
        if token.get("surface") not in ("たり", "だり") or not _is_listing_tari(tokens, idx):
            continue
        if token.get("pos") != "Particle" or token.get("lemma") != token["surface"]:
            token["pos"] = "Particle"
            token["lemma"] = token["surface"]
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
        is_terminal_perfect = (
            surface == "たり"
            and (idx == len(tokens) - 1 or tokens[idx + 1].get("surface") == "けり")
            and not _is_listing_tari(tokens, idx)
        )
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


_CLASSICAL_PAST_KERI_CELLS = ("けり", "ける", "けれ")


def _classical_past_starts_at(tokens: list[dict], idx: int) -> bool:
    """Whether the classical past auxiliary begins at @p idx.

    The けり paradigm is identified by its cell, because the reference analyzer
    reads every one of them as the homographic 蹴る before this module retags
    them. The nominal reading of the same kana is excluded: it is the deverbal
    noun taking a case of its own (壁にけりを入れる), which is the one thing the
    auxiliary never does. The 終止形 き is only ever this auxiliary once
    something else has classified it, so it is taken on that classification.
    """
    if idx >= len(tokens):
        return False
    token = tokens[idx]
    if token.get("pos") == "Noun":
        return False
    if token.get("surface") in _CLASSICAL_PAST_KERI_CELLS:
        return True
    return token.get("surface") == "き" and token.get("pos") == "Auxiliary"


def _nominal_plus_continuative(surface: str) -> list[dict] | None:
    """Split a compound headword whose tail is a verb continuative, else None.

    The reference dictionary carries a handful of noun+continuative compounds as
    single headwords (雨降り, 山登り). They are nominals everywhere except in
    front of a predicate cell, where the tail is the verb the cell attaches to.
    The same ます probe as @ref _verb_continuative_reading recovers the parts,
    and the split is taken only when the probe reproduces the whole surface.
    """

    probe = mecab_analyze(surface + "ます")
    if len(probe) != 3 or probe[2].get("surface") != "ます":
        return None
    head, tail = probe[0], probe[1]
    if head.get("pos") != "名詞" or tail.get("pos") != "動詞" or tail.get("conj_form") != "連用形":
        return None
    if f"{head.get('surface', '')}{tail.get('surface', '')}" != surface:
        return None
    return [
        {"surface": head["surface"], "pos": "Noun", "lemma": head.get("lemma", head["surface"])},
        {"surface": tail["surface"], "pos": "Verb", "lemma": tail.get("lemma", tail["surface"])},
    ]


@reports_mutation
def postprocess_classical_perfect_ni(tokens: list[dict]) -> None:
    """Retag に between a continuative and the classical past as the perfect ぬ.

    けり and き attach to a continuative, so nothing stands between them and the
    predicate — least of all a case particle, which introduces an argument and
    would leave the clause without the predicate it marks. The mora is the
    continuative cell of the perfective ぬ (花散りにけり). The reference
    dictionary carries no such cell, reads the commonest case particle instead,
    and then, having made the preceding continuative that particle's host, hands
    the host back as a deverbal noun.
    """
    idx = 1
    while idx < len(tokens) - 1:
        token = tokens[idx]
        if token.get("surface") != "に" or token.get("pos") != "Particle":
            idx += 1
            continue
        if not _classical_past_starts_at(tokens, idx + 1):
            idx += 1
            continue
        previous = tokens[idx - 1]
        reading = _verb_continuative_reading(previous.get("surface", ""))
        if reading is not None:
            previous["pos"] = "Verb"
            previous["lemma"] = reading.get("lemma", previous.get("lemma"))
        else:
            parts = _nominal_plus_continuative(previous.get("surface", ""))
            if parts is not None:
                tokens[idx - 1 : idx] = parts
                idx += 1
                token = tokens[idx]
            # An auxiliary hosts the perfect from its own continuative
            # (飲ま+れ+に+けり) and needs no restoration; a particle host means
            # the reference split the predicate itself and neither side can
            # recover it, but the follower still identifies the cell (出+で+に).
            # A nominal host is excluded: there the reference is reading an
            # ordinary argument, and the kana after it is its case (壁にける).
            elif previous.get("pos") not in ("Auxiliary", "Particle"):
                idx += 1
                continue
        token["pos"] = "Auxiliary"
        token["lemma"] = "ぬ"
        idx += 1


@reports_mutation
def postprocess_classical_past_keri(tokens: list[dict]) -> None:
    """Restore the classical past けり after the perfective continuative に.

    に+けり closes a predicate with the perfective ぬ and the past けり.  The
    reference dictionary has no けり cell for that position, so an unpunctuated
    clause falls back to the homographic 蹴る -- which turns the preceding
    continuative into its object and leaves the clause without a tense.  The
    nominal reading of けり keeps its own 名詞 tag, so the verb tag alone
    identifies the fallback.  The continuative in front is either the case
    particle the reference reads or the perfective cell it actually spells,
    depending on whether the rule above could recover the predicate it hosts.
    """
    for idx in range(2, len(tokens)):
        token = tokens[idx]
        previous = tokens[idx - 1]
        if (
            token.get("surface") != "けり"
            or token.get("pos") != "Verb"
            or previous.get("surface") != "に"
            or previous.get("pos") not in ("Particle", "Auxiliary")
            or tokens[idx - 2].get("pos") not in ("Noun", "Verb", "Adjective")
        ):
            continue
        token["pos"] = "Auxiliary"
        token["lemma"] = "けり"


_VOICE_AUXILIARY_LEMMAS = frozenset({"れる", "られる", "せる", "させる"})


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
        host = tokens[idx - 1]
        # A voice auxiliary lends the same continuative (選ば+れ+し+者).
        voice_host = host.get("pos") == "Auxiliary" and host.get("lemma") in _VOICE_AUXILIARY_LEMMAS
        if host.get("pos") != "Verb" and not voice_host:
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
        reading = _verb_continuative_reading(previous.get("surface", ""))
        if reading is None:
            continue
        # The perfect attaches to a predicate, so the host is that continuative
        # whatever class the reference gave it. Where the dictionary also carries
        # the cell as a nominalized headword it hands back the noun (花+咲き),
        # which would leave the auxiliary hosted by something that cannot host it.
        previous["pos"] = "Verb"
        previous["lemma"] = reading.get("lemma", previous.get("lemma"))
        token["pos"] = "Auxiliary"
        token["lemma"] = "ぬ"


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
