"""Subsidiary verbs, auxiliaries, contractions, and honorific frames."""

import regex

from . import postprocessor_predicate_hosts, postprocessor_te_chains
from .core_lexicon import core_headwords
from .postprocessor_common import reports_mutation
from .split_rules import base_from_renyokei

# Exports from postprocessor_predicate_hosts.
postprocess_adverb_host_context = postprocessor_predicate_hosts.postprocess_adverb_host_context
postprocess_frame_repairs = postprocessor_predicate_hosts.postprocess_frame_repairs
postprocess_negative_host = postprocessor_predicate_hosts.postprocess_negative_host
postprocess_predicate_cell_lemmas = postprocessor_predicate_hosts.postprocess_predicate_cell_lemmas
postprocess_sou_host = postprocessor_predicate_hosts.postprocess_sou_host

# Exports from postprocessor_te_chains.
postprocess_contracted_iku_lemma = postprocessor_te_chains.postprocess_contracted_iku_lemma
postprocess_contracted_iku_tte_na = postprocessor_te_chains.postprocess_contracted_iku_tte_na
postprocess_contracted_toku_aux = postprocessor_te_chains.postprocess_contracted_toku_aux
postprocess_excessive_after_verb = postprocessor_te_chains.postprocess_excessive_after_verb
postprocess_humble_o_itasu = postprocessor_te_chains.postprocess_humble_o_itasu
postprocess_koto_suru_te = postprocessor_te_chains.postprocess_koto_suru_te
postprocess_obligation_nan_naru = postprocessor_te_chains.postprocess_obligation_nan_naru
postprocess_teru_te_kureru = postprocessor_te_chains.postprocess_teru_te_kureru
postprocess_teru_te_order = postprocessor_te_chains.postprocess_teru_te_order


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
    for i in range(1, len(tokens)):
        t = tokens[i]
        surface = t.get("surface", "")
        if surface not in ("い", "いる", "いれ", "いた", "いない"):
            continue
        if t.get("pos") != "Verb":
            continue
        prev_surface = tokens[i - 1].get("surface", "")
        direct_te_form = prev_surface in ("て", "で")
        # A binding or adverbial particle may sit inside the frame (見て+は+いない,
        # 読んで+ばかり+いる) without making いる existential.
        focused_te_form = (
            surface in ("い", "いる")
            and tokens[i - 1].get("pos") == "Particle"
            and (tokens[i - 1].get("pos_sub1") or "").startswith(("係助詞", "副助詞"))
            and i >= 2
            and tokens[i - 2].get("surface") in ("て", "で")
        )
        if direct_te_form or focused_te_form:
            t["pos"] = "Auxiliary"
            t["lemma"] = "いる"


# What selects the nasalized terminal of くる (くん) after a te-form.
_CONTRACTED_KURU_FOLLOWERS = ("じゃん", "だ", "です", "でしょ", "の")


def postprocess_giving_aux(tokens: list[dict]) -> bool:
    """Classify productive て/で + giving/receiving verbs as auxiliaries."""
    auxiliary_lemmas = frozenset({"あげる", "くれる", "もらう"})
    changed = False
    for idx in range(1, len(tokens)):
        token = tokens[idx]
        following = tokens[idx + 1].get("surface", "") if idx + 1 < len(tokens) else ""
        # Before じゃん, the copula and the nominalizer, くん after a te-form is
        # くる with its る nasalized (走って+くん+の, 歩いて+くん+じゃん), as the
        # reference already reads it before の; くれる contracts to くん only
        # before its negative (待って+くん+ない).
        if (
            token.get("surface") == "くん"
            and tokens[idx - 1].get("surface") in ("て", "で")
            and following.startswith(_CONTRACTED_KURU_FOLLOWERS)
        ):
            if (token.get("pos"), token.get("lemma")) != ("Auxiliary", "くる"):
                token["pos"] = "Auxiliary"
                token["lemma"] = "くる"
                changed = True
            continue
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
        # An existential predicate (ある, ござる) takes the honorific nominal as
        # its subject (お変わりございません, お変わりありません).
        nominal_context = (
            idx + 1 == len(tokens)
            or tokens[idx + 1].get("pos") == "Particle"
            or tokens[idx + 1].get("lemma") in ("ある", "ござる", "ございる")
        )
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
            "致す",
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
