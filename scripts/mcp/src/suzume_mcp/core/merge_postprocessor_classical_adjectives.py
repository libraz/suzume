"""Classical adjective and adjective-like conjugation passes."""

from .constants import (
    CLASSICAL_ADJECTIVE_LEMMA_OVERRIDES,
)
from .mecab import is_single_token_of_pos, mecab_analyze
from .merge_postprocessor_common import _A_ROW_TO_U_ROW


def _postprocess_adj_bungo(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Fix archaic adjective form: 恐し + いとも → 恐しい + と + も.

    When 形容詞 in 文語基本形 is followed by a token starting with い + particles,
    merge い back into the adjective and split out the particles.
    """
    new_result: list[dict] = []
    skip_next = False
    for j, curr in enumerate(result):
        if skip_next:
            skip_next = False
            continue
        if curr.get("pos") == "形容詞" and curr.get("conj_form") == "文語基本形" and j + 1 < len(result):
            nxt = result[j + 1]
            ns = nxt.get("surface", "")
            if ns.startswith("い"):
                adj_surface = curr["surface"] + "い"
                # The reference analyzer may normalize the lemma to another
                # modern spelling.  Suzume preserves the observed productive
                # -しい base, so the oracle must do the same after restoring
                # the split terminal い.
                new_result.append({"surface": adj_surface, "pos": "形容詞", "lemma": adj_surface})
                rest = ns[1:]
                for ch in rest:
                    new_result.append({"surface": ch, "pos": "助詞", "lemma": ch})
                skip_next = True
                if applied_rule is None:
                    applied_rule = "adj-bungo-fix"
                continue
        new_result.append(curr)
    return new_result, applied_rule


# Modern sentence-final particles. A clause closed by one is colloquial, so a
# し before it is the listing particle, not a classical adjective terminal.
_MODERN_FINAL_PARTICLES = frozenset(("さ", "よ", "ね", "な", "わ"))


def _predicate_before_shi(host: str, stem: str) -> tuple[list[dict], list[dict]] | None:
    """The reference's reading of ``host`` + ``stem`` when the stem ends in a finite predicate.

    The host is analyzed together with the stem so each is read in context;
    only a reading with a token boundary exactly where the host ends counts.
    Returns the host tokens and the stem tokens.
    """
    if not stem:
        return None
    tokens = mecab_analyze(host + stem)
    consumed = 0
    for index, token in enumerate(tokens):
        if consumed == len(host):
            tail = tokens[index:]
            if (
                "".join(token.get("surface", "") for token in tail) == stem
                and tail[-1].get("pos") in ("動詞", "形容詞", "助動詞")
                and tail[-1].get("conj_form") == "基本形"
            ):
                return tokens[:index], tail
            return None
        consumed += len(token.get("surface", ""))
    return None


def _postprocess_predicate_shi(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Restore predicate + listing し where the reference fused し into one word.

    Two fusions occur: a finite predicate + し before a modern final particle is
    read as a classical adjective terminal (なる+し → なるし, lemma なるい), and an
    auxiliary + し after a continuative is read as an unrelated サ行 verb
    (行き+たいし, lemma たいす), sometimes with the continuative itself read as a
    noun. In both, the probe of the material before し must come back as a
    finite predicate, and in the second its first token must be an auxiliary.
    """
    new_result: list[dict] = []
    for index, curr in enumerate(result):
        surface = curr.get("surface", "")
        following = result[index + 1] if index + 1 < len(result) else {}
        previous = new_result[-1] if new_result else {}
        classical_terminal = (
            curr.get("pos") == "形容詞"
            and curr.get("conj_form") == "文語基本形"
            and following.get("surface") in _MODERN_FINAL_PARTICLES
        )
        sa_row_verb = curr.get("pos") == "動詞" and curr.get("lemma", "").endswith("す") and bool(previous)
        reading = None
        if surface.endswith("し") and (classical_terminal or sa_row_verb):
            reading = _predicate_before_shi(previous.get("surface", ""), surface[:-1])
            if reading and not classical_terminal:
                host_tokens, predicate = reading
                if not (
                    predicate[0].get("pos") == "助動詞"
                    and host_tokens
                    and host_tokens[-1].get("pos") == "動詞"
                    and host_tokens[-1].get("conj_form") == "連用形"
                ):
                    reading = None
        if reading is None:
            new_result.append(curr)
            continue
        host_tokens, predicate = reading
        if not classical_terminal and previous:
            new_result.pop()
            new_result.extend(dict(token) for token in host_tokens)
        new_result.extend(dict(token) for token in predicate)
        new_result.append({"surface": "し", "pos": "助詞", "pos_sub1": "接続助詞", "lemma": "し"})
        # The reference reads さ after the fused verb as the nominalizing suffix.
        if following.get("surface") == "さ" and following.get("pos") == "名詞":
            following.update(pos="助詞", pos_sub1="終助詞", lemma="さ")
        if applied_rule is None:
            applied_rule = "predicate-shi"
    return new_result, applied_rule


_KARI_TAILS = ("かり", "かる", "かれ")
KARI_MIZENKEI_CELL = "から"
# The supplementary conjugation offers two cells the reference dictionary
# carries, and each has a hole the other covers. から collides with the case
# particle, so a stem that is also a noun loses the adjective reading to
# 名詞+助詞 (赤から, where 青から and 黒から survive); かろ has no such homograph
# but is missing for the auxiliaries that inflect the same way (べかろ). Probing
# both is what makes the evidence lexicon-independent.
_KARI_PROBE_CELLS = (KARI_MIZENKEI_CELL, "かろ")
_KARI_MAX_TOKEN_RUN = 4


def _kari_probe_token(surface: str, cell: str, expected_pos: tuple[str, ...]) -> dict | None:
    """The single token the reference dictionary reads a probe cell as, if any."""
    probe = surface[: -len(KARI_MIZENKEI_CELL)] + cell
    tokens = mecab_analyze(probe)
    if len(tokens) != 1:
        return None
    token = tokens[0]
    if token.get("pos") not in expected_pos or token.get("surface") != probe:
        return None
    return token


def classical_adjective_lemma(mizenkei: str) -> str | None:
    """Read the modern headword of a classical adjective off its 未然形 cell.

    The reference dictionary conjugates the から cell itself (難しから -> 難しい,
    遅から -> 遅い), so asking it resolves the ク/シク split without having to
    guess whether a し belongs to the stem or to the ending.  Only its own stale
    headwords need correcting on the way out.
    """
    for cell in _KARI_PROBE_CELLS:
        token = _kari_probe_token(mizenkei, cell, ("形容詞",))
        if token is None:
            continue
        lemma = token.get("lemma")
        resolved = CLASSICAL_ADJECTIVE_LEMMA_OVERRIDES.get(lemma, lemma) or None
        if resolved:
            return resolved
    return None


_KARI_CELL_POS = ("形容詞", "助動詞")
_KARI_CONTINUATIVE = "かり"
# The cells of the classical past that a カリ continuative hosts, each mapped to
# the follower that identifies it, or None when the cell needs none.  Both are one
# mora and both spell a サ変 cell, which is what the reference dictionary reads
# them as once it has missed the かり token in front of them.  The 連体形 し stands
# on its own, modifying or nominalizing; the 未然形 せ exists only inside the
# counterfactual, so it is taken only in front of the conditional it ends in.
_KARI_CLASSICAL_PAST_CELLS = {"し": None, "せ": "ば"}


def _kari_cell_analysis(surface: str) -> tuple[str, str] | None:
    """Read the word class and lemma of a カリ cell off its 未然形.

    The supplementary conjugation belongs to the i-adjective and to every
    auxiliary that inflects like one (べし, たい, らしい), and the reference
    dictionary carries the 未然形 cell of both kinds (高から, べから) while losing
    the rest.  That cell is therefore the probe: a surface whose カリ ending can
    be swapped for one and still analyze as one word is a cell of the same
    paradigm, and the probe settles the word class along with the lemma.
    """
    for cell in _KARI_PROBE_CELLS:
        token = _kari_probe_token(surface, cell, _KARI_CELL_POS)
        if token is None:
            continue
        lemma = CLASSICAL_ADJECTIVE_LEMMA_OVERRIDES.get(token.get("lemma"), token.get("lemma"))
        if lemma:
            return (token.get("pos"), lemma)
    return None


def _postprocess_adj_kari(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Rebuild the classical supplementary (カリ) conjugation.

    から/かり/かる/かれ are cells of the word's own inflection table, not a stem
    plus an auxiliary: there is no 助動詞 かり in the classical inventory. The
    reference dictionary only carries the 未然形 cell, so the others fall back to
    unrelated verbs (高+かり as かりる, 冷+たかる as たかる, 小+さかり as さかる) and,
    for the auxiliaries that inflect like an adjective, to a sentence-final
    particle the position cannot host (べ+かり, where the 終助詞 べ closes a clause
    and so can never stand in front of anything). Restore each run as the one
    token the 未然形 cell already yields, word class and lemma together.
    """
    merged: list[dict] = []
    idx = 0
    while idx < len(result):
        run = ""
        matched_end = 0
        matched_cell = None
        for end in range(idx, min(idx + _KARI_MAX_TOKEN_RUN, len(result))):
            run += result[end].get("surface", "")
            # The rule repairs a split the dictionary got wrong, so a surface it
            # already analyzes as one word needs no repair (明かり stays a noun).
            if end == idx:
                continue
            if len(run) <= len(KARI_MIZENKEI_CELL) or not run.endswith(_KARI_TAILS):
                continue
            cell = _kari_cell_analysis(run)
            if cell is not None:
                matched_end = end + 1
                matched_cell = cell
                break
        if matched_cell is not None:
            cell_pos, cell_lemma = matched_cell
            surface = "".join(result[pos].get("surface", "") for pos in range(idx, matched_end))
            merged.append({"surface": surface, "pos": cell_pos, "lemma": cell_lemma})
            idx = matched_end
            if applied_rule is None:
                applied_rule = "adj-kari-conjugation"
            # The whole point of the かり cell is to carry a classical auxiliary,
            # so a one-mora サ変 cell behind it belongs to the past き instead.
            if surface.endswith(_KARI_CONTINUATIVE) and idx < len(result) and result[idx].get("pos") == "動詞":
                cell = result[idx].get("surface", "")
                follower = _KARI_CLASSICAL_PAST_CELLS.get(cell, "")
                next_surface = result[idx + 1].get("surface", "") if idx + 1 < len(result) else ""
                if cell in _KARI_CLASSICAL_PAST_CELLS and (follower is None or follower == next_surface):
                    merged.append({"surface": cell, "pos": "助動詞", "lemma": "き"})
                    idx += 1
            continue
        merged.append(result[idx])
        idx += 1
    return merged, applied_rule


_KU_NOMINALIZER = "く"


def _postprocess_ku_nominalization(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Rebuild the classical ク語法 nominalization as one noun.

    ク語法 turns a predicate into a noun by attaching く to the 未然形 (言わ+く,
    思わ+く).  The reference dictionary lists the fossilized members it happens to
    carry as whole nouns — 曰く, 老いらく — and fragments the rest, so the same
    formation is one search unit under one spelling and two or three tokens under
    another.  Both fragmentations are repaired here: the irrealis kana handed to a
    following verb (言 + わく, the shape 書 + かむ already takes), and the bare
    nominalizer left standing after the stem (思わ + く).
    """
    merged: list[dict] = []
    idx = 0
    while idx < len(result):
        token = result[idx]
        surface = token.get("surface", "")
        previous = merged[-1] if merged else None
        # 言 + わく: the irrealis kana went to whatever followed the stem, which
        # the dictionary reads as a verb or a noun depending on the sentence.
        if (
            previous is not None
            and token.get("pos") in ("動詞", "名詞")
            and len(surface) == 2
            and surface[0] in _A_ROW_TO_U_ROW
            and surface[1] == _KU_NOMINALIZER
        ):
            stem = previous.get("surface", "")
            if stem and is_single_token_of_pos(stem + _A_ROW_TO_U_ROW[surface[0]], "動詞"):
                combined = stem + surface
                merged[-1] = {"surface": combined, "pos": "名詞", "lemma": combined}
                idx += 1
                if applied_rule is None:
                    applied_rule = "ku-nominalization"
                continue
        # 思わ + く: the stem is already whole and く stands on its own.
        if (
            previous is not None
            and previous.get("pos") == "動詞"
            and previous.get("surface", "")[-1:] in _A_ROW_TO_U_ROW
            and surface == _KU_NOMINALIZER
        ):
            combined = previous.get("surface", "") + surface
            merged[-1] = {"surface": combined, "pos": "名詞", "lemma": combined}
            idx += 1
            if applied_rule is None:
                applied_rule = "ku-nominalization"
            continue
        merged.append(token)
        idx += 1
    return merged, applied_rule
