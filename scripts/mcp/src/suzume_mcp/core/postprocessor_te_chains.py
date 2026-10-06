"""Contracted te-form auxiliaries and te-form request, humble, and benefactive chains."""

from itertools import pairwise

from .postprocessor_common import reports_mutation
from .split_rules import base_from_renyokei

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


def postprocess_contracted_iku_tte_na(tokens: list[dict]) -> bool:
    """Split the reference's compound particle ってな into its cells.

    After a te-form (持って+ってな) it is the contracted ていく cell っ, the
    connective て and the final particle な; elsewhere it is the quotative って
    plus な. Neither reading is one particle.
    """
    changed = False
    idx = 0
    while idx < len(tokens):
        token = tokens[idx]
        if token.get("surface") == "ってな" and token.get("pos") == "Particle":
            after_te_form = idx > 0 and tokens[idx - 1].get("surface") in ("て", "で")
            if after_te_form:
                cells = [
                    {"surface": "っ", "pos": "Auxiliary", "lemma": "く"},
                    {"surface": "て", "pos": "Particle", "lemma": "て"},
                ]
            else:
                cells = [{"surface": "って", "pos": "Particle", "lemma": "って"}]
            cells.append({"surface": "な", "pos": "Particle", "lemma": "な"})
            tokens[idx : idx + 1] = cells
            idx += len(cells)
            changed = True
            continue
        idx += 1
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
