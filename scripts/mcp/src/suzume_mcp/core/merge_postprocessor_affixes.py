"""Affix and search-unit post-processing passes."""

import regex

from .constants import (
    BOUND_SUFFIX_VERB_NOUN_CELLS,
    SEARCH_UNIT_COMPOUNDS,
)

# 物 is the one dependent nominal that is also a bound intensifying prefix on an
# adjective (物悲しい, 物寂しい, 物珍しい, 物足りない). The other members of its
# tag take an adjective as a predicate over themselves instead, which is a
# different construction and keeps its boundary (こと+なく, ためし+が+ない).
_BOUND_ADJECTIVE_PREFIX = "物"


def _postprocess_bound_prefix_adjective(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Join the bound prefix 物 to the adjective it intensifies.

    In front of an adjective the morpheme is the prefix and the result is one
    adjective; the dependent-noun reading needs something modifying it, which an
    adjective behind it does not supply. The reference dictionary reaches the
    prefix reading only when the pair is one of its headwords, which left one
    spelling of the same word whole and split its variant.
    """
    merged: list[dict] = []
    for token in result:
        host = merged[-1] if merged else None
        if (
            host is not None
            and host.get("surface") == _BOUND_ADJECTIVE_PREFIX
            and host.get("pos") == "名詞"
            and host.get("pos_sub1") == "非自立"
            and token.get("pos") == "形容詞"
            and token.get("pos_sub1") == "自立"
        ):
            combined = host.get("surface", "") + token.get("surface", "")
            lemma = host.get("surface", "") + (token.get("lemma") or token.get("surface", ""))
            merged[-1] = {"surface": combined, "pos": "形容詞", "pos_sub1": "自立", "lemma": lemma}
            if applied_rule is None:
                applied_rule = "bound-prefix-adjective"
            continue
        merged.append(token)
    return merged, applied_rule


def _postprocess_search_unit_split(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Re-split kanji-merged tokens that absorbed part of a search-unit compound.

    Example: kanji-merge produces AB+C, but BC should be one token.
    This splits AB → A+B, then merges B+C → BC.
    """
    new_result: list[dict] = []
    skip_next = False
    for j, curr in enumerate(result):
        if skip_next:
            skip_next = False
            continue
        if j < len(result) - 1:
            nxt = result[j + 1]
            curr_surface = curr.get("surface", "")
            nxt_surface = nxt.get("surface", "")
            for word, word_pos in SEARCH_UNIT_COMPOUNDS.items():
                # Check if word spans across curr (ending) + nxt (beginning)
                for split_pos in range(1, len(word)):
                    prefix = word[:split_pos]
                    suffix = word[split_pos:]
                    if curr_surface.endswith(prefix) and nxt_surface == suffix:
                        head = curr_surface[: -len(prefix)]
                        if head:
                            new_result.append({"surface": head, "pos": curr.get("pos", ""), "lemma": head})
                        new_result.append({"surface": word, "pos": word_pos, "lemma": word})
                        skip_next = True
                        if applied_rule is None:
                            applied_rule = "search-unit-split"
                        break
                if skip_next:
                    break
        if not skip_next or j < len(result) - 1:
            if not skip_next:
                new_result.append(curr)
    # Handle last token if not skipped
    if not skip_next and len(result) > 0:
        pass  # Already appended in the loop
    return new_result, applied_rule


def _postprocess_nominal_zukeru(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Merge a kanji nominal host with the productive Ichidan suffix づける."""
    normalized: list[dict] = []
    idx = 0
    zukeru_forms = ("づける", "づけ", "づけれ", "づけよ", "づけろ")
    while idx < len(result):
        current = result[idx]
        if (
            idx + 1 < len(result)
            and current.get("pos") == "名詞"
            and regex.fullmatch(r"[\p{Han}]{2,}", current.get("surface", ""))
            and result[idx + 1].get("surface") in zukeru_forms
            and result[idx + 1].get("pos") == "動詞"
        ):
            surface = current.get("surface", "") + result[idx + 1].get("surface", "")
            normalized.append(
                {
                    "surface": surface,
                    "pos": "動詞",
                    "lemma": current.get("surface", "") + "づける",
                }
            )
            idx += 2
            if applied_rule is None:
                applied_rule = "nominal-zukeru"
            continue
        normalized.append(current)
        idx += 1
    return normalized, applied_rule


def _is_quantity_unit(surface: str) -> bool:
    """Return whether surface is a productive numeral+kanji unit."""
    numeric = regex.match(r"^[0-9０-９一二三四五六七八九十百千万億兆]+", surface)
    if numeric is None:
        return False
    unit = surface[numeric.end() :]
    return bool(unit and regex.fullmatch(r"[\p{Han}]+", unit))


def _postprocess_distributive_quantity(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Merge repeated numeral+unit phrases such as 一語一語 structurally."""
    normalized: list[dict] = []
    idx = 0
    while idx < len(result):
        surface = result[idx].get("surface", "")
        split_prefix = ""
        for width in range(2, len(surface) // 2 + 1):
            unit = surface[:width]
            if surface.startswith(unit + unit) and _is_quantity_unit(unit):
                split_prefix = unit + unit
                break
        if result[idx].get("pos") == "名詞" and split_prefix:
            normalized.append({"surface": split_prefix, "pos": "名詞", "lemma": split_prefix})
            remainder = surface[len(split_prefix) :]
            if remainder:
                normalized.append({"surface": remainder, "pos": "名詞", "lemma": remainder})
            idx += 1
            if applied_rule is None:
                applied_rule = "distributive-quantity"
            continue
        if (
            idx + 1 < len(result)
            and result[idx].get("pos") == "名詞"
            and result[idx + 1].get("pos") == "名詞"
            and result[idx + 1].get("surface") == surface
            and _is_quantity_unit(surface)
        ):
            combined = surface + surface
            normalized.append({"surface": combined, "pos": "名詞", "lemma": combined})
            idx += 2
            if applied_rule is None:
                applied_rule = "distributive-quantity"
            continue
        normalized.append(result[idx])
        idx += 1
    return normalized, applied_rule


def _postprocess_bound_voiced_suffix(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Rejoin the bound voiced suffix がかる when it was split at its first mora.

    がかる is a bound suffix on a nominal host (芝居がかった, 紫がかって). The
    reference analyzer knows a few hosts lexically and splits the rest into the
    case particle が plus a remainder that is not a word on its own, so the same
    suffix is analyzed two different ways depending on the host. Rejoin the split
    form so the oracle treats every host alike.

    がましい is handled at the end of the pipeline instead: the compound merges
    that build its host run later than this pass, and rejoining the suffix here
    would consume the が they use to recognize that host.
    """
    tails = ("かっ", "かる", "かり", "かれ", "から", "かろ")
    new_result: list[dict] = []
    idx = 0
    while idx < len(result):
        token = result[idx]
        following = result[idx + 1] if idx + 1 < len(result) else None
        host_is_nominal = bool(new_result) and new_result[-1].get("pos") in ("名詞", "Noun", "動詞", "Verb")
        if (
            host_is_nominal
            and token.get("surface") == "が"
            and following is not None
            and following.get("surface", "") in tails
        ):
            merged = token.get("surface", "") + following.get("surface", "")
            pos = "動詞"
            lemma = "がかる"
            new_result.append({"surface": merged, "pos": pos, "lemma": lemma})
            if applied_rule is None:
                applied_rule = "bound-voiced-suffix"
            idx += 2
            continue
        new_result.append(token)
        idx += 1
    return new_result, applied_rule


def _postprocess_bound_suffix_noun_cell(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Restore a bound suffix verb cell the reference dictionary read as a noun.

    形式ばった / 四角ばった are the host plus the derivational suffix ばる in its
    past form, and the analyzer already reads that suffix as a verb wherever its
    spelling is not a word (形式ばって -> ばっ/ばる). Only the cells that spell a
    known noun break, and they break silently: ばった becomes the insect. A
    nominal host is required, which is the environment the suffix takes.
    """
    new_result: list[dict] = []
    for token in result:
        cell = BOUND_SUFFIX_VERB_NOUN_CELLS.get(token.get("surface", ""))
        host_is_nominal = bool(new_result) and new_result[-1].get("pos") in ("名詞", "Noun")
        if cell is None or not host_is_nominal or token.get("pos") not in ("名詞", "Noun"):
            new_result.append(token)
            continue
        suffix_surface, lemma, auxiliary = cell
        new_result.append({"surface": suffix_surface, "pos": "動詞", "lemma": lemma})
        new_result.append({"surface": auxiliary, "pos": "助動詞", "lemma": auxiliary})
        if applied_rule is None:
            applied_rule = "bound-suffix-noun-cell"
    return new_result, applied_rule
