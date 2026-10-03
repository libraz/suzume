"""Subsidiary verbs, auxiliaries, contractions, and honorific frames."""

from itertools import pairwise

import regex

from .core_lexicon import core_headwords
from .postprocessor_common import reports_mutation
from .split_rules import base_from_renyokei


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
        # Western とう/どう (〜ておる) after an onbin stem: the desiderative
        # たい's う-onbin attaches to the plain continuative, never to an onbin.
        if (
            token.get("surface") in ("とう", "どう")
            and previous.get("pos") == "Verb"
            and previous.get("surface", "").endswith(("っ", "ん", "い"))
            and (token.get("surface") == "どう") == previous.get("surface", "").endswith("ん")
        ):
            token["pos"] = "Auxiliary"
            token["lemma"] = token["surface"]
            changed = True
            continue
        if token.get("surface") != "でる" or previous.get("pos") != "Verb":
            continue
        if not previous.get("surface", "").endswith("ん"):
            continue
        # でる is the voiced てる after a nasal onbin; like どく it keeps its own headword.
        token["pos"] = "Auxiliary"
        token["lemma"] = "でる"
        changed = True
    # Before の the る of てる contracts to ん (食べ+てん+の = 食べてるの); a
    # nominalizer ん directly before the nominalizer の is not a reading.
    idx = 1
    while idx + 1 < len(tokens):
        te, contracted, nominalizer = tokens[idx], tokens[idx + 1], tokens[idx + 2] if idx + 2 < len(tokens) else {}
        if (
            te.get("surface") in ("て", "で")
            and tokens[idx - 1].get("pos") == "Verb"
            and contracted.get("surface") == "ん"
            and nominalizer.get("surface") == "の"
        ):
            tokens[idx : idx + 2] = [
                {"surface": te["surface"] + "ん", "pos": "Auxiliary", "lemma": te["surface"] + "る"}
            ]
            changed = True
        idx += 1
    return changed


@reports_mutation
def postprocess_miru_aux(tokens: list[dict]) -> bool:
    """Classify trial みる after a te-form boundary as Auxiliary."""
    trial_surfaces = {"み", "みる", "みれ", "みろ", "みよ"}
    # 食べてみな is the trial み plus the imperative な, not the pronoun みな.
    for idx in range(len(tokens) - 1, 0, -1):
        if (
            tokens[idx].get("surface") == "みな"
            and tokens[idx].get("pos") == "Pronoun"
            and tokens[idx - 1].get("surface") in ("て", "で")
        ):
            tokens[idx : idx + 1] = [
                {"surface": "み", "pos": "Auxiliary", "lemma": "みる"},
                {"surface": "な", "pos": "Particle", "lemma": "な"},
            ]
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
        | {head + "ま" + ending for head in ("ち", "じ") for ending in "うわいっえお"}
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
            and (not surface.startswith("じ") or previous.get("surface", "").endswith("ん"))
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
        # Only お takes a native continuative; ご takes a Sino-Japanese verbal
        # noun even when its kana spelling ends in i/e (ご+あんない+いたし).
        if (
            prefix.get("pos") == "Prefix"
            and prefix.get("surface") == "お"
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


def postprocess_verb_negative_aux(tokens: list[dict]) -> bool:
    """Tag the contracted conditional なきゃ/なけりゃ after a verb as the auxiliary.

    The reference flips 帰んなきゃ between the negative auxiliary and the
    adjective depending on the following punctuation.
    """
    changed = False
    for idx in range(1, len(tokens)):
        token = tokens[idx]
        if token.get("pos") != "Adjective" or token.get("surface") not in ("なきゃ", "なけりゃ"):
            continue
        if tokens[idx - 1].get("pos") != "Verb":
            continue
        token["pos"] = "Auxiliary"
        changed = True
    return changed


def postprocess_copula_negative_nee(tokens: list[dict]) -> bool:
    """Tag ねえ after the copula's じゃ as the negative auxiliary (じゃねえか).

    The reference reads it as the final particle there, while 知らねえ gets the
    negative; じゃ+ない is the same chain in its plain spelling.
    """
    changed = False
    for idx in range(1, len(tokens)):
        token = tokens[idx]
        previous = tokens[idx - 1]
        if (
            token.get("surface") == "ねえ"
            and token.get("pos") == "Particle"
            and previous.get("surface") == "じゃ"
            and previous.get("lemma") == "だ"
        ):
            token["pos"] = "Auxiliary"
            token["lemma"] = "ない"
            changed = True
    return changed


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
        # ず is the terminal cell of the same negative auxiliary as ぬ, so the
        # なら in front of it is the same irrealis (ほかならず beside ほかならぬ).
        if nxt_surface in ("ない", "なく", "なかっ", "ぬ", "ず"):
            t["pos"] = "Verb"
            t["lemma"] = "なる"


# Fabricated readings the reference analyzer gives the remainder of a
# contracted とく/どく when it fails to see the auxiliary: と + けっ(ける) + て,
# と + いて(いてる), と + こう(adverb).
_TOKU_FABRICATED_TAILS: dict[tuple[str, str], tuple[str, str, str]] = {
    ("けっ", "Verb"): ("け", "って", "Particle"),
    ("いて", "Verb"): ("い", "て", "Particle"),
    ("こう", "Adverb"): ("こ", "う", "Auxiliary"),
}


def postprocess_contracted_toku_aux(tokens: list[dict]) -> bool:
    """Restore the contracted auxiliary とく (〜ておく) the reference misreads.

    A continuative or onbin verb cannot take the case or conditional と, so a
    verb host followed by と/ど and one of the fabricated tails above is the
    contracted auxiliary: 書い+とけ+って, 書い+とい+て, 書い+とこ+う.  A host
    the reference turned into a noun (やめ) is read back as its verb.
    """
    from .merge_postprocessors import _probe_continuative

    changed = False
    idx = 0
    while idx + 2 < len(tokens):
        host, particle, tail = tokens[idx], tokens[idx + 1], tokens[idx + 2]
        spec = _TOKU_FABRICATED_TAILS.get((tail.get("surface", ""), tail.get("pos", "")))
        host_surface = host.get("surface", "")
        if (
            spec is None
            or particle.get("pos") != "Particle"
            or particle.get("surface") not in ("と", "ど")
            or not host_surface
        ):
            idx += 1
            continue
        if host.get("pos") == "Verb":
            if host_surface[-1] in "うくぐすずつづぬぶむるただ":
                idx += 1
                continue
        elif host.get("pos") == "Noun":
            probed = _probe_continuative(host_surface)
            if probed is None or len(probed) != 1:
                idx += 1
                continue
            host["pos"] = "Verb"
            host["lemma"] = probed[0].get("lemma") or host_surface
        else:
            idx += 1
            continue
        head, rest, rest_pos = spec
        aux_lemma = particle["surface"] + "く"
        replacement = [{"surface": particle["surface"] + head, "pos": "Auxiliary", "lemma": aux_lemma}]
        consumed = 3
        if rest == "って":
            following = tokens[idx + 3] if idx + 3 < len(tokens) else None
            if following is None or following.get("surface") != "て":
                idx += 1
                continue
            consumed = 4
        replacement.append({"surface": rest, "pos": rest_pos, "lemma": rest})
        tokens[idx + 1 : idx + consumed] = replacement
        changed = True
        idx += 2
    return changed


def postprocess_obligation_nan_naru(tokens: list[dict]) -> bool:
    """Read なん+ない after a conditional negative as the contracted ならない.

    なきゃ/なくちゃ leave the obligation open for ならない, whose colloquial
    なんない the reference reads as the pronoun なん plus the adjective ない.
    """
    changed = False
    for idx in range(1, len(tokens) - 1):
        previous, token, following = tokens[idx - 1], tokens[idx], tokens[idx + 1]
        if (
            previous.get("pos") == "Auxiliary"
            and previous.get("lemma") == "ない"
            and previous.get("surface", "").endswith(("ゃ", "ば"))
            and token.get("surface") == "なん"
            and token.get("pos") == "Pronoun"
            and following.get("surface", "").startswith("な")
            and following.get("lemma") == "ない"
        ):
            token.update(pos="Verb", lemma="なる")
            following.update(pos="Auxiliary")
            changed = True
    return changed


_TE_CONTINUATION_HEADS = ("ら", "る", "た", "ない", "い", "ちゃ", "お")


def postprocess_koto_suru_te(tokens: list[dict]) -> bool:
    """Restore こと+し+て where the reference reads a kana こ or ことし.

    Before a te-continuation (らんない, る, た, ない) the kana こ+として and
    ことし+てる are the formal noun こと, the verb する and its te-form; the
    year noun ことし and the particle として cannot take those tails.
    """
    changed = False
    idx = 0
    while idx + 1 < len(tokens):
        token, following = tokens[idx], tokens[idx + 1]
        after = tokens[idx + 2] if idx + 2 < len(tokens) else None
        koto = {"surface": "こと", "pos": "Noun", "lemma": "こと"}
        shi = {"surface": "し", "pos": "Verb", "lemma": "する"}
        if (
            token.get("surface") == "こ"
            and token.get("pos") == "Noun"
            and following.get("surface") == "として"
            and after is not None
            and after.get("surface", "").startswith(_TE_CONTINUATION_HEADS)
        ):
            tokens[idx : idx + 2] = [koto, shi, {"surface": "て", "pos": "Particle", "lemma": "て"}]
            changed = True
            idx += 3
            continue
        if (
            token.get("surface") == "ことし"
            and token.get("pos") == "Noun"
            and following.get("surface", "").startswith("て")
            and following.get("pos") in ("Verb", "Auxiliary", "Particle")
        ):
            tokens[idx : idx + 1] = [koto, shi]
            if following.get("surface") in ("てる", "てた"):
                following.update(pos="Auxiliary", lemma="てる")
            changed = True
            idx += 2
            continue
        idx += 1
    return changed


def postprocess_humble_o_itasu(tokens: list[dict]) -> bool:
    """Split a humble お+continuative+いたす the reference lexicalized whole.

    The frame is productive (お伝え+いたし, お届け+いたし), and a lexicalized
    headword (おかけいたす) hides the same three parts.
    """
    from .merge_postprocessors import _probe_continuative

    changed = False
    idx = 0
    while idx < len(tokens):
        token = tokens[idx]
        surface, lemma = token.get("surface", ""), token.get("lemma", "")
        cut = surface.find("いた")
        if not (
            token.get("pos") == "Verb"
            and surface.startswith("お")
            and lemma.startswith("お")
            and lemma.endswith("いたす")
            and cut > 1
        ):
            idx += 1
            continue
        middle = surface[1:cut]
        probed = _probe_continuative(middle)
        if probed is None or len(probed) != 1:
            idx += 1
            continue
        tokens[idx : idx + 1] = [
            {"surface": "お", "pos": "Prefix", "lemma": "お"},
            {"surface": middle, "pos": "Verb", "lemma": probed[0].get("lemma") or middle},
            {"surface": surface[cut:], "pos": "Verb", "lemma": "いたす"},
        ]
        changed = True
        idx += 3
    return changed


def postprocess_teru_te_kureru(tokens: list[dict]) -> bool:
    """Re-cut て+てく+ん/れ+ない as てる's て, て and くん/くれ (待っててくんない).

    The reference reads the doubled て as て + contracted ていく, which leaves
    the benefactive くれる's colloquial くん (or plain くれ) as a nominalizer or a
    passive.  The paradigm is the same as 待ってて+くれ.
    """
    changed = False
    idx = 0
    while idx + 3 < len(tokens):
        te, teku, tail, nai = tokens[idx : idx + 4]
        if (
            te.get("surface") == "て"
            and te.get("pos") == "Particle"
            and teku.get("surface") == "てく"
            and tail.get("surface") in ("ん", "れ")
            and nai.get("surface", "").startswith("な")
            and nai.get("lemma") == "ない"
        ):
            tokens[idx : idx + 4] = [
                {"surface": "て", "pos": "Auxiliary", "lemma": "てる"},
                {"surface": "て", "pos": "Particle", "lemma": "て"},
                {"surface": "く" + tail["surface"], "pos": "Auxiliary", "lemma": "くれる"},
                {**nai, "pos": "Auxiliary"},
            ]
            changed = True
            idx += 4
            continue
        idx += 1
    return changed


@reports_mutation
def postprocess_teru_te_order(tokens: list[dict]) -> bool:
    """Keep てる's て before the connective て in a doubled て (待っててよ).

    The reference reads the pair as て + てる's continuative when a final
    particle or the end of the text follows, and the other way round before a
    benefactive. A continuative cannot close the clause, so the doubled て is
    てる's て followed by the connective in both positions.
    """
    changed = False
    for idx in range(1, len(tokens) - 1):
        te, teru = tokens[idx], tokens[idx + 1]
        following = tokens[idx + 2] if idx + 2 < len(tokens) else None
        if (
            tokens[idx - 1].get("pos") == "Verb"
            and te.get("surface") == "て"
            and te.get("pos") == "Particle"
            and teru.get("surface") == "て"
            and teru.get("lemma") == "てる"
            and (following is None or following.get("pos") == "Particle")
        ):
            tokens[idx] = {"surface": "て", "pos": "Auxiliary", "lemma": "てる"}
            tokens[idx + 1] = {"surface": "て", "pos": "Particle", "lemma": "て"}
            changed = True
    return changed


def postprocess_excessive_after_verb(tokens: list[dict]) -> bool:
    """Tag すぎ after a verb continuative as the verb すぎる (食べ+すぎ).

    The reference calls the same cell a nominal suffix when its host was an
    unknown stem it had to repair, so one construction carried two tags.
    """
    changed = False
    for previous, token in pairwise(tokens):
        if token.get("surface") != "すぎ" or token.get("pos") != "Suffix":
            continue
        # A godan continuative host comes back as a noun (読み+すぎ) where the
        # ichidan one stays a verb (食べ+すぎ); both are the same construction.
        if previous.get("pos") == "Noun":
            base = base_from_renyokei(previous.get("surface", ""))
            if base is None:
                continue
            previous.update(pos="Verb", lemma=base)
        if previous.get("pos") == "Verb":
            token.update(pos="Verb", lemma="すぎる")
            changed = True
    return changed


def postprocess_contracted_iku_lemma(tokens: list[dict]) -> bool:
    """Give the contracted ていく cells (持ってく, 持ってった, 見てかない) the lemma いく.

    The reference names the clipped く/っ after て by its own surface, which
    is no headword; the paradigm is that of the subsidiary いく.
    """
    changed = False
    # The reference fuses て with the irrealis after an onbin (行っ+てか); it is
    # the same te-form plus the contracted cell (行っ+て+か).
    idx = 0
    while idx < len(tokens):
        token = tokens[idx]
        if token.get("pos") == "Auxiliary" and token.get("lemma") == "てく" and token.get("surface", "")[:1] in "てで":
            surface = token["surface"]
            tokens[idx : idx + 1] = [
                {"surface": surface[0], "pos": "Particle", "lemma": surface[0]},
                {"surface": surface[1:], "pos": "Auxiliary", "lemma": "く"},
            ]
            changed = True
        idx += 1
    for previous, token in pairwise(tokens):
        if (
            token.get("pos") == "Auxiliary"
            and token.get("lemma") == "く"
            and token.get("surface") in ("く", "っ", "か")
            and previous.get("surface") in ("て", "で")
        ):
            token["lemma"] = "いく"
            changed = True
    return changed


# Auxiliaries that do not conjugate like a verb. The negative auxiliary ない
# selects a verbal irrealis, so after any of these ない is the adjective.
_NON_VERBAL_AUXILIARY_LEMMAS = frozenset(
    {
        "だ",
        "です",
        "たい",
        "ない",
        "らしい",
        "ます",
        "た",
        "う",
        "よう",
        "まい",
        "ぬ",
        "ん",
        "ず",
        "べし",
        "そう",
        "みたい",
    }
)


def postprocess_negative_host(tokens: list[dict]) -> bool:
    """Tag ない by its host: auxiliary after a verbal irrealis, adjective elsewhere.

    The negative auxiliary attaches only to a verb or a verb-conjugating
    auxiliary (食べ+ない, 食べ+させ+ない, 見+て+い+ない), and to the te-form whose
    いる has been dropped (見+て+ない). After a particle, the copula, an adjective
    continuative or an adjective-type auxiliary it is the (supplementary)
    adjective: 時間+が+ない, 本+で+は+ない, 本+じゃ+ない, 水+しか+ない,
    高く+ない, 食べ+たく+ない.
    """
    changed = False
    for idx, token in enumerate(tokens):
        if token.get("lemma") != "ない" or token.get("pos") not in ("Auxiliary", "Adjective"):
            continue
        previous = tokens[idx - 1] if idx > 0 else None
        verbal_host = previous is not None and (
            previous.get("pos") == "Verb"
            or (previous.get("pos") == "Auxiliary" and previous.get("lemma") not in _NON_VERBAL_AUXILIARY_LEMMAS)
            or (previous.get("pos") == "Particle" and previous.get("surface") in ("て", "で"))
        )
        wanted = "Auxiliary" if verbal_host else "Adjective"
        if token.get("pos") != wanted:
            token["pos"] = wanted
            changed = True
    return changed


def postprocess_sou_host(tokens: list[dict]) -> bool:
    """Tag そう by its host: auxiliary after a predicate, adverb elsewhere.

    The appearance auxiliary follows a continuative or stem (降り+そう, おいし+
    そう, 食べ+な+さ+そう) and the hearsay auxiliary a terminal form (降る+そう,
    教師+だ+そう, 読ん+だ+そう); both are the auxiliary そうだ whatever follows.
    With no predicate in front, そう is the demonstrative adverb (そう+だ,
    まさに+そう+だ, 彼+も+そう+言っ+た).
    """
    changed = False
    for idx, token in enumerate(tokens):
        if token.get("surface") != "そう" or token.get("pos") not in ("Adjective", "Auxiliary", "Adverb"):
            continue
        previous = tokens[idx - 1] if idx > 0 else None
        following = tokens[idx + 1] if idx + 1 < len(tokens) else None
        # A noun takes the appearance そう only as a na-adjective stem (不安+そう);
        # before a verb it is the adverb after a temporal noun (明日+そう+する).
        nominal_stem_host = (
            previous is not None
            and previous.get("pos") == "Noun"
            and (following is None or following.get("pos") != "Verb")
        )
        predicate_host = previous is not None and (
            previous.get("pos") in ("Verb", "Adjective", "Auxiliary")
            or (previous.get("pos") == "Suffix" and previous.get("surface") == "さ")
            or nominal_stem_host
        )
        wanted = "Auxiliary" if predicate_host else "Adverb"
        if token.get("pos") != wanted:
            token["pos"] = wanted
            token["lemma"] = "そう"
            changed = True
    return changed
