"""Productive search-unit and derivational boundary repairs."""

import regex

from .constants import (
    COPULA_SURFACES,
    COUNTER_UNITS,
    PRODUCTIVE_COMPOUND_V2,
    QUANTITY_BOUND_SUFFIXES,
)
from .split_rules import base_from_mizenkei, base_from_renyokei

_PRODUCTIVE_COMPOUND_V2 = PRODUCTIVE_COMPOUND_V2
_QUANTITY_DIGITS = "0-9０-９〇零一二三四五六七八九十百千万億兆"
_QUANTITY_COUNTER_PATTERN = "|".join(regex.escape(unit) for unit in sorted(COUNTER_UNITS, key=len, reverse=True))
_QUANTITY_SUFFIX_PATTERN = "|".join(regex.escape(suffix) for suffix in QUANTITY_BOUND_SUFFIXES)
_QUANTITY_ONLY_PATTERN = regex.compile(rf"^[{_QUANTITY_DIGITS}]+(?:{_QUANTITY_COUNTER_PATTERN})$")
_QUANTITY_BOUND_PATTERN = regex.compile(
    rf"^(?P<quantity>[{_QUANTITY_DIGITS}]+(?:{_QUANTITY_COUNTER_PATTERN}))"
    rf"(?P<suffix>{_QUANTITY_SUFFIX_PATTERN})$"
)


def postprocess_quantity_bound_suffix(tokens: list[dict]) -> bool:
    """Split a numeral+counter phrase from its closed-class bound suffix."""
    changed = False
    index = 0
    while index < len(tokens):
        token = tokens[index]
        # MeCab sometimes already supplies the quantity and suffix as separate
        # tokens but tags the homographic suffix as a verb stem (二本|立て).
        # The preceding quantity fixes the closed-class suffix reading.
        if (
            index > 0
            and token.get("surface") in QUANTITY_BOUND_SUFFIXES
            and _QUANTITY_ONLY_PATTERN.fullmatch(tokens[index - 1].get("surface", ""))
        ):
            token["pos"] = "Suffix"
            token["lemma"] = token["surface"]
            changed = True
            index += 1
            continue
        match = _QUANTITY_BOUND_PATTERN.fullmatch(token.get("surface", ""))
        if match is None:
            index += 1
            continue
        quantity = match.group("quantity")
        suffix = match.group("suffix")
        tokens[index : index + 1] = [
            {"surface": quantity, "pos": "Noun", "lemma": quantity},
            {"surface": suffix, "pos": "Suffix", "lemma": suffix},
        ]
        changed = True
        index += 2
    return changed


def postprocess_exclusion_suffix(tokens: list[dict]) -> bool:
    """Classify nominal X+抜き/ぬき constructions as exclusion suffixes."""
    changed = False
    for idx, token in enumerate(tokens):
        if idx == 0 or token.get("surface") not in ("抜き", "ぬき"):
            continue
        previous = tokens[idx - 1]
        if previous.get("pos") not in ("Noun", "Prefix"):
            continue
        if previous.get("surface") == "中" and previous.get("pos") == "Prefix":
            previous["pos"] = "Noun"
            changed = True
        if token.get("pos") != "Suffix":
            token["pos"] = "Suffix"
            changed = True
    return changed


def postprocess_admixture_suffix(tokens: list[dict]) -> bool:
    """Classify nominal X+まじり as the admixture suffix (冗談+まじり, 雪+まじり).

    The reference reads the kana as the continuative of まじる; after a noun
    and before a nominal frame it is the dependent suffix, not a predicate.
    """
    changed = False
    for idx, token in enumerate(tokens[1:], start=1):
        if token.get("surface") != "まじり" or tokens[idx - 1].get("pos") != "Noun":
            continue
        following = tokens[idx + 1] if idx + 1 < len(tokens) else None
        if following is not None and following.get("pos") not in ("Particle", "Symbol"):
            continue
        if token.get("pos") != "Suffix" or token.get("lemma") != "まじり":
            token["pos"] = "Suffix"
            token["lemma"] = "まじり"
            changed = True
    return changed


def postprocess_state_suffix(tokens: list[dict]) -> bool:
    """Classify nominal X+中 as a state suffix in nominal predicate positions."""
    changed = False
    for idx, token in enumerate(tokens[1:], start=1):
        if token.get("surface") != "中" or token.get("pos") != "Noun":
            continue
        if tokens[idx - 1].get("pos") != "Noun":
            continue
        following = tokens[idx + 1] if idx + 1 < len(tokens) else None
        if following is None or following.get("pos") == "Particle" or following.get("surface") in COPULA_SURFACES:
            token["pos"] = "Suffix"
            changed = True
    return changed


def postprocess_productive_verb_suffix_stem(tokens: list[dict]) -> bool:
    """Restore a verb continuative before a productive derivational suffix."""
    verb_suffixes = frozenset({"がち", "っぱなし", "たて", "まくり"})
    changed = False
    for idx in range(len(tokens) - 1):
        stem = tokens[idx]
        suffix = tokens[idx + 1]
        if suffix.get("surface") not in verb_suffixes:
            continue
        if suffix.get("surface") == "まくり" and stem.get("pos") == "Verb":
            if suffix.get("pos") != "Suffix" or suffix.get("lemma") != "まくり":
                suffix["pos"] = "Suffix"
                suffix["lemma"] = "まくり"
                changed = True
        if suffix.get("pos") != "Suffix":
            continue
        # A voice auxiliary's continuative hosts the suffix as it is (奪わ+れ+がち).
        if stem.get("pos") == "Auxiliary" or (stem.get("pos") == "Verb" and stem.get("lemma") != stem.get("surface")):
            continue
        lemma = base_from_renyokei(stem.get("surface", ""))
        if lemma is None:
            continue
        stem["pos"] = "Verb"
        stem["lemma"] = lemma
        changed = True
    return changed


def postprocess_productive_search_unit_boundaries(tokens: list[dict]) -> bool:
    """Align productive boundaries without enumerating open-class hosts.

    Every branch is licensed by a closed follower class or an inflectional
    shape.  The function therefore generalizes across arbitrary noun and verb
    hosts while retaining Suzume's search-unit compounds.
    """
    changed = False
    idx = 0
    while idx < len(tokens):
        token = tokens[idx]
        surface = token.get("surface", "")

        if idx + 1 < len(tokens) and surface == "ん" and tokens[idx + 1].get("surface") == "かっ":
            tokens[idx : idx + 2] = [{"surface": "んかっ", "pos": "Auxiliary", "lemma": "ない"}]
            changed = True
            continue

        if idx + 1 < len(tokens) and surface == "づく" and tokens[idx + 1].get("surface") == "め":
            tokens[idx : idx + 2] = [{"surface": "づくめ", "pos": "Suffix", "lemma": "づくめ"}]
            changed = True
            continue

        if idx + 1 < len(tokens) and regex.fullmatch(r"([あいうえお])\1+", surface):
            following = tokens[idx + 1].get("surface", "")
            if following and set(following) == {surface[0]}:
                token["surface"] = surface + following
                token["lemma"] = token["surface"]
                token["pos"] = "Adverb"
                del tokens[idx + 1]
                changed = True
                continue

        if idx + 1 < len(tokens) and surface == "うす" and tokens[idx + 1].get("pos") == "Adjective":
            following = tokens[idx + 1]
            combined = surface + following.get("surface", "")
            tokens[idx : idx + 2] = [{"surface": combined, "pos": "Adjective", "lemma": combined}]
            changed = True
            continue

        # A compound nominal host immediately selected by the closed
        # がましい construction is one search unit (X+V-renyokei + がましい).
        if (
            idx + 3 < len(tokens)
            and tokens[idx + 2].get("surface") == "が"
            and tokens[idx + 3].get("surface") == "ましい"
        ):
            following = tokens[idx + 1]
            if token.get("pos") == "Noun" and following.get("pos") in ("Noun", "Verb"):
                combined = surface + following.get("surface", "")
                tokens[idx : idx + 2] = [{"surface": combined, "pos": "Noun", "lemma": combined}]
                changed = True
                continue

        # Calendar heads bind to the closed 末/翌+counter units while a
        # following deverbal payment stem remains its own search unit.
        if idx + 1 < len(tokens) and tokens[idx + 1].get("surface") == "末締め":
            if surface in COUNTER_UNITS:
                tokens[idx : idx + 2] = [
                    {"surface": surface + "末", "pos": "Noun", "lemma": surface + "末"},
                    {"surface": "締め", "pos": "Noun", "lemma": "締め"},
                ]
                changed = True
                idx += 2
                continue

        if surface == "翌" and idx + 1 < len(tokens):
            following_surface = tokens[idx + 1].get("surface", "")
            unit = next((candidate for candidate in COUNTER_UNITS if following_surface.startswith(candidate)), "")
            remainder = following_surface[len(unit) :]
            if unit and remainder:
                tokens[idx : idx + 2] = [
                    {"surface": surface + unit, "pos": "Noun", "lemma": surface + unit},
                    {"surface": remainder, "pos": "Noun", "lemma": remainder},
                ]
                changed = True
                idx += 2
                continue

        # MeCab can analyze productive V1+合わせる as a causative chain
        # (見合わ+せる, つめあわ+せ).  The internal 合わ/あわ boundary
        # recovers the same closed V2 class without naming V1 hosts.  A bare
        # continuative directly selected by a nominal particle is a deverbal
        # compound noun; finite せる remains a compound verb.
        compound_alignment_stem = next(
            (ending for ending in ("合わ", "あわ") if surface.endswith(ending) and len(surface) > len(ending)),
            None,
        )
        if idx + 2 < len(tokens) and compound_alignment_stem is not None:
            following = tokens[idx + 1]
            nominal_particle = tokens[idx + 2]
            if (
                following.get("surface") == "せ"
                and nominal_particle.get("pos") == "Particle"
                and nominal_particle.get("surface") in {"を", "は", "が", "の", "に", "で", "へ", "と", "も"}
            ):
                combined = surface + "せ"
                tokens[idx : idx + 2] = [{"surface": combined, "pos": "Noun", "lemma": combined}]
                changed = True
                continue

        if idx + 1 < len(tokens) and compound_alignment_stem is not None:
            following = tokens[idx + 1]
            if following.get("surface") == "せる":
                combined = surface + "せる"
                lemma = surface[: -len(compound_alignment_stem)] + "合わせる"
                tokens[idx : idx + 2] = [{"surface": combined, "pos": "Verb", "lemma": lemma}]
                changed = True
                continue

        if idx + 1 < len(tokens) and token.get("pos") in ("Verb", "Noun"):
            following = tokens[idx + 1]
            if following.get("pos") == "Verb":
                v2_base = following.get("lemma", "")
                if v2_base not in _PRODUCTIVE_COMPOUND_V2 and following.get("surface", "").endswith("せる"):
                    potential_base = following.get("surface", "")[:-2] + "す"
                    if potential_base in _PRODUCTIVE_COMPOUND_V2:
                        v2_base = potential_base
                renyokei_base = base_from_renyokei(surface)
                # A subsidiary verb (いただき, み) heads no compound.
                if (
                    v2_base in _PRODUCTIVE_COMPOUND_V2
                    and token.get("pos") == "Verb"
                    and token.get("pos_sub1") != "非自立"
                    and renyokei_base == token.get("lemma")
                ):
                    combined = surface + following.get("surface", "")
                    compound_lemma = combined if following.get("surface", "").endswith("せる") else surface + v2_base
                    tokens[idx : idx + 2] = [{"surface": combined, "pos": "Verb", "lemma": compound_lemma}]
                    changed = True
                    continue

        # MeCab exposes the shortened causative mora on the host token
        # (やらさ+れ); Suzume keeps host+さ+れ as three morphemes.
        if idx + 1 < len(tokens) and token.get("pos") == "Verb" and surface.endswith("さ"):
            following = tokens[idx + 1]
            host = surface[:-1]
            host_lemma = base_from_mizenkei(host)
            is_regular_sa_row = token.get("lemma", "").endswith("す") and token.get("lemma", "")[:-1] == host
            if (
                host_lemma
                and not is_regular_sa_row
                and following.get("pos") == "Auxiliary"
                and following.get("surface", "").startswith("れ")
            ):
                tokens[idx : idx + 1] = [
                    {"surface": host, "pos": "Verb", "lemma": host_lemma},
                    {"surface": "さ", "pos": "Auxiliary", "lemma": "す"},
                ]
                changed = True
                idx += 2
                continue

        # Volitional よう is morphologically the o-row stem + auxiliary う.
        if (
            idx + 1 < len(tokens)
            and (token.get("pos") == "Verb" or (token.get("pos") == "Noun" and token.get("lemma") == "する"))
            and tokens[idx + 1].get("surface") == "よう"
            and tokens[idx + 1].get("pos") == "Auxiliary"
        ):
            token["pos"] = "Verb"
            token["surface"] = surface + "よ"
            tokens[idx + 1] = {"surface": "う", "pos": "Auxiliary", "lemma": "う"}
            changed = True
            idx += 2
            continue

        # Denominal colloquial verbs before progressive ている expose the
        # geminate on the noun in Suzume (過疎っ+て+いる).
        if idx + 2 < len(tokens) and token.get("pos") == "Noun" and regex.search(r"\p{Han}", surface):
            following = tokens[idx + 1]
            progressive = tokens[idx + 2]
            if following.get("surface") == "って" and progressive.get("lemma") == "いる":
                tokens[idx] = {"surface": surface + "っ", "pos": "Verb", "lemma": surface + "る"}
                tokens[idx + 1] = {"surface": "て", "pos": "Particle", "lemma": "て"}
                progressive["pos"] = "Auxiliary"
                changed = True

        if surface == "ましい" and idx > 0 and tokens[idx - 1].get("surface") == "が":
            token["pos"] = "Adjective"
            token["lemma"] = "ましい"
            changed = True

        if (
            idx > 0
            and idx + 1 < len(tokens)
            and surface == "あり"
            and tokens[idx - 1].get("surface") == "でも"
            and tokens[idx + 1].get("pos") == "Auxiliary"
        ):
            token["pos"] = "Noun"
            token["lemma"] = "あり"
            changed = True

        if surface == "他" and idx + 1 < len(tokens) and tokens[idx + 1].get("surface") == "の":
            token["lemma"] = "ほか"
            changed = True

        if surface == "ただ" and idx + 1 < len(tokens) and tokens[idx + 1].get("pos") == "Pronoun":
            token["pos"] = "Adverb"
            token["lemma"] = "ただ"
            changed = True

        if surface == "反し" and idx > 0 and tokens[idx - 1].get("surface") == "に":
            token["lemma"] = "反する"
            changed = True

        honorific_naru = (
            idx + 2 < len(tokens) and tokens[idx + 1].get("surface") == "に" and tokens[idx + 2].get("lemma") == "なる"
        )
        invitation_auxiliary = (
            idx + 1 < len(tokens)
            and tokens[idx + 1].get("surface") == "なんし"
            and tokens[idx + 1].get("lemma") == "ます"
        )
        if surface == "おいで" and (honorific_naru or invitation_auxiliary):
            token["pos"] = "Noun"
            token["lemma"] = "おいで"
            changed = True

        if token.get("pos") == "Adjective" and surface.endswith("く") and idx + 1 < len(tokens):
            following = tokens[idx + 1]
            if following.get("pos") == "Adjective" and not following.get("surface", "").startswith(
                ("ない", "なく", "なかっ", "なけれ")
            ):
                token["pos"] = "Adverb"
                token["lemma"] = surface
                changed = True

        if surface == "どう" and idx + 1 < len(tokens) and tokens[idx + 1].get("surface") == "か":
            token["pos"] = "Adverb"
            token["lemma"] = "どう"
            changed = True

        if surface == "で" and idx > 0:
            previous = tokens[idx - 1]
            if token.get("pos") == "Auxiliary" and (
                previous.get("surface") == "せい" or regex.fullmatch(r"\p{Katakana}+", previous.get("surface", ""))
            ):
                token["pos"] = "Particle"
                token["lemma"] = "で"
                changed = True
            elif token.get("pos") == "Particle" and previous.get("pos") == "Adjective":
                token["pos"] = "Auxiliary"
                token["lemma"] = "だ"
                changed = True

        if surface == "どき" and idx > 0 and tokens[idx - 1].get("pos") == "Noun":
            token["pos"] = "Noun"
            token["lemma"] = "どき"
            changed = True

        if token.get("pos") == "Verb" and surface.endswith("れる") and token.get("lemma") != surface:
            token["lemma"] = surface
            changed = True

        if surface == "行っ" and idx > 0 and tokens[idx - 1].get("surface") in {"に", "へ"}:
            token["lemma"] = "行く"
            changed = True

        if (
            token.get("pos") == "Noun"
            and idx > 0
            and tokens[idx - 1].get("pos") == "Prefix"
            # ご prefixes a Sino-Japanese verbal noun (ご+あんない+し), which
            # is not a native continuative even when it ends in i/e.
            and tokens[idx - 1].get("surface") != "ご"
            and idx + 1 < len(tokens)
            and tokens[idx + 1].get("surface") == "し"
        ):
            reconstructed = base_from_renyokei(surface)
            if reconstructed is not None:
                token["pos"] = "Verb"
                token["lemma"] = reconstructed
                changed = True

        if idx == len(tokens) - 1 and token.get("pos") == "Adjective" and regex.fullmatch(r"\p{Han}+よ", surface):
            token["pos"] = "Verb"
            token["lemma"] = surface[:-1] + "る"
            changed = True

        if token.get("pos") == "Adjective" and not token.get("lemma", "").endswith("い"):
            if surface.endswith("化"):
                token["pos"] = "Noun"
                token["lemma"] = surface
                changed = True

        idx += 1
    return changed


def postprocess_negative_appearance_suffix(tokens: list[dict]) -> bool:
    """Split the appearance suffix げ from the negative adjective stem な.

    A nominal host takes ない's stem plus げ (頼り+な+げ, 危+な+げ), which the
    reference dictionary reads as the verb なげる or as one adjective 危なげない.
    The stem keeps its own token and げ is the same suffix it is after 寂し+げ.
    The reference dictionary also reads なげ as a noun (申し訳+なげ).
    """
    changed = False
    idx = 0
    while idx < len(tokens):
        token = tokens[idx]
        surface = token.get("surface", "")
        if (
            surface == "なげ"
            and (token.get("pos"), token.get("lemma")) in (("Verb", "なげる"), ("Noun", "なげ"))
            and idx > 0
            and tokens[idx - 1].get("pos") in ("Noun", "Verb")
        ):
            # Verbal ない takes an irrealis, so a continuative before な+げ is
            # the deverbal noun (頼り), not a verb.
            if tokens[idx - 1].get("pos") == "Verb":
                tokens[idx - 1].update(pos="Noun", lemma=tokens[idx - 1].get("surface", ""))
            tokens[idx : idx + 1] = [
                {"surface": "な", "pos": "Adjective", "lemma": "ない"},
                {"surface": "げ", "pos": "Suffix", "lemma": "げ"},
            ]
            changed = True
            idx += 2
        elif (
            token.get("pos") == "Adjective"
            and token.get("lemma", "").endswith("なげない")
            and surface == token["lemma"][:-1]
        ):
            stem = token["lemma"][:-3]
            tokens[idx : idx + 1] = [
                {"surface": stem, "pos": "Adjective", "lemma": stem + "い"},
                {"surface": "げ", "pos": "Suffix", "lemma": "げ"},
                {"surface": "な", "pos": "Auxiliary", "lemma": "だ"},
            ]
            changed = True
            idx += 3
        else:
            idx += 1
            continue
        following = tokens[idx] if idx < len(tokens) else None
        if following is not None and following.get("surface") == "な" and following.get("pos") == "Particle":
            following.update(pos="Auxiliary", lemma="だ")
    return changed
