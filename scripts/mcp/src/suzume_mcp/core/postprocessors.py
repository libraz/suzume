"""Postprocessors ported from SuzumeUtils.pm _postprocess_* functions."""

import regex

from .constants import (
    EMPHATIC_SOKUON,
    INTERROGATIVES,
    SLANG_ADJ_STEMS,
    SLANG_VERB_STEMS,
    UNUSUAL_NAMES,
    WORD_EXCEPTIONS,
)
from .pos_mapping import _is_katakana_onomatopoeia


def preprocess_for_mecab(text: str) -> tuple[str, dict[int, dict]]:
    """Replace slang stems with standard ones before MeCab analysis.

    Returns:
        Tuple of (processed text, replacements dict keyed by position).
    """
    replacements: dict[int, dict] = {}

    # Slang adjectives
    for slang, standard in SLANG_ADJ_STEMS.items():
        for m in regex.finditer(regex.escape(slang) + r"[いかくけさ]", text):
            replacements[m.start()] = {
                "original": slang,
                "replacement": standard,
                "length": len(slang),
            }

    # Slang verbs
    for slang, standard in SLANG_VERB_STEMS.items():
        for m in regex.finditer(regex.escape(slang) + r"[らりるれろっ]", text):
            replacements[m.start()] = {
                "original": slang,
                "replacement": standard,
                "length": len(slang),
            }

    # Unusual names
    for name, standard in UNUSUAL_NAMES.items():
        for m in regex.finditer(regex.escape(name) + r"(さん|ちゃん|様|君|殿)", text):
            replacements[m.start()] = {
                "original": name,
                "replacement": standard,
                "length": len(name),
            }

    # Word exceptions
    for word, standard in WORD_EXCEPTIONS.items():
        for m in regex.finditer(regex.escape(word), text):
            replacements[m.start()] = {
                "original": word,
                "replacement": standard,
                "length": len(word),
            }

    # Emphatic sokuon
    for pattern, standard in EMPHATIC_SOKUON.items():
        for m in regex.finditer(regex.escape(pattern) + r"(?!て)", text):
            replacements[m.start()] = {
                "original": pattern,
                "replacement": standard,
                "length": len(pattern),
            }

    # Apply replacements in reverse position order
    for pos in sorted(replacements.keys(), reverse=True):
        r = replacements[pos]
        text = text[:pos] + r["replacement"] + text[pos + r["length"] :]

    return text, replacements


def postprocess_mecab_tokens(tokens: list[dict], original_text: str, replacements: dict[int, dict]) -> list[dict]:
    """Restore slang terms in tokens after MeCab processing."""
    if not replacements:
        return tokens

    # Pattern-based restoration
    for t in tokens:
        surface = t.get("surface", "")
        for r in replacements.values():
            if r["replacement"] in surface:
                surface = surface.replace(r["replacement"], r["original"])
                t["surface"] = surface
            lemma = t.get("lemma", "")
            if lemma and r["replacement"] in lemma:
                t["lemma"] = lemma.replace(r["replacement"], r["original"])

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


def postprocess_sou(tokens: list[dict]) -> None:
    """Context-dependent そう normalization."""
    for i, t in enumerate(tokens):
        if t.get("surface") != "そう":
            continue

        # 伝聞そう before copula -> Adjective
        # Only when preceded by Auxiliary (だ/た etc.), not by Verb (様態そう)
        pos = t.get("pos", "")
        if pos in ("Adverb", "Auxiliary"):
            if i < len(tokens) - 1 and i > 0:
                nxt = tokens[i + 1].get("surface", "")
                prev_pos = tokens[i - 1].get("pos", "")
                if regex.match(r"^(?:だ|です|でし|じゃ|じゃろ)", nxt) and prev_pos != "Verb":
                    t["pos"] = "Adjective"
                # そう before で+ある (copula である)
                elif nxt == "で" and i < len(tokens) - 2:
                    nxt2 = tokens[i + 2].get("surface", "")
                    if nxt2 in ("ある", "あり", "あれ", "あっ"):
                        t["pos"] = "Adjective"
            elif i == 0:  # Sentence-initial そう before copula
                if i < len(tokens) - 1:
                    nxt = tokens[i + 1].get("surface", "")
                    if regex.match(r"^(?:だ|です|でし|じゃ|じゃろ)", nxt):
                        t["pos"] = "Adjective"
                    # そう before で+ある (copula である)
                    elif nxt == "で" and i < len(tokens) - 2:
                        nxt2 = tokens[i + 2].get("surface", "")
                        if nxt2 in ("ある", "あり", "あれ", "あっ"):
                            t["pos"] = "Adjective"

        # Katakana adjective stem + そう: Noun -> Adjective
        if i > 0:
            prev = tokens[i - 1]
            prev_surface = prev.get("surface", "")
            if (
                prev.get("pos") == "Noun"
                and regex.match(r"^[\u30A0-\u30FF]+$", prev_surface)
                and not _is_katakana_onomatopoeia(prev_surface)
            ):
                prev["pos"] = "Adjective"
                prev["lemma"] = prev_surface + "い"


def postprocess_ikaga(tokens: list[dict]) -> None:
    """Context-dependent いかが normalization."""
    for i, t in enumerate(tokens):
        if t.get("surface") != "いかが":
            continue
        has_copula = False
        if i < len(tokens) - 1:
            nxt = tokens[i + 1].get("surface", "")
            if regex.match(r"^(?:です|でし|だ|だっ|でしょ)", nxt):
                has_copula = True
        if not has_copula:
            t["pos"] = "Adverb"


def postprocess_demo(tokens: list[dict]) -> None:
    """Context-dependent でも normalization."""
    for i, t in enumerate(tokens):
        if t.get("surface") != "でも" or t.get("pos") != "Particle":
            continue
        if i > 0 and tokens[i - 1].get("surface", "") in INTERROGATIVES:
            continue  # Keep as Particle
        t["pos"] = "Conjunction"


def postprocess_ii(tokens: list[dict]) -> None:
    """Fix いい: Verb(いう) -> Adjective when not followed by verb."""
    for i, t in enumerate(tokens):
        if t.get("surface") != "いい":
            continue
        if t.get("pos") != "Verb" or t.get("lemma") != "いう":
            continue
        next_is_verb = False
        if i < len(tokens) - 1:
            next_is_verb = tokens[i + 1].get("pos") == "Verb"
        if not next_is_verb:
            t["pos"] = "Adjective"
            t["lemma"] = "いい"


def postprocess_iru_aux(tokens: list[dict]) -> None:
    """Fix い/いる after て: Verb -> Auxiliary."""
    for i in range(1, len(tokens)):
        t = tokens[i]
        surface = t.get("surface", "")
        if surface not in ("い", "いる", "いれ", "いた", "いない"):
            continue
        if t.get("pos") != "Verb":
            continue
        prev_surface = tokens[i - 1].get("surface", "")
        if prev_surface in ("て", "で"):
            t["pos"] = "Auxiliary"
            t["lemma"] = "いる"


def postprocess_de_particle(tokens: list[dict]) -> None:
    """Fix で as copula -> Particle in certain contexts."""
    for i, t in enumerate(tokens):
        if t.get("surface") != "で" or t.get("pos") != "Auxiliary":
            continue
        # で followed by は/も
        if i < len(tokens) - 1:
            nxt = tokens[i + 1].get("surface", "")
            if nxt in ("は", "も"):
                t["pos"] = "Particle"
                t["lemma"] = "で"


def postprocess_na_adj_noun(tokens: list[dict]) -> None:
    """No-op: kept for import compatibility. Adjective stems before すぎる stay Adjective."""
    pass


def postprocess_tsuke_noun(tokens: list[dict]) -> None:
    """Fix 付け: Suffix -> Noun."""
    for t in tokens:
        if t.get("surface") == "付け" and t.get("pos") == "Suffix":
            t["pos"] = "Noun"
            t["lemma"] = "付け"


def postprocess_copula_neg(tokens: list[dict]) -> None:
    """Fix なく after じゃ/で: Auxiliary -> Adjective."""
    for i in range(1, len(tokens)):
        t = tokens[i]
        if t.get("surface") != "なく" or t.get("pos") != "Auxiliary":
            continue
        prev = tokens[i - 1].get("surface", "")
        if prev in ("じゃ", "で"):
            t["pos"] = "Adjective"
            t["lemma"] = "ない"


def postprocess_te(tokens: list[dict]) -> None:
    """Fix て/で after verb: Particle -> Auxiliary."""
    for i in range(1, len(tokens)):
        t = tokens[i]
        surface = t.get("surface", "")
        if surface not in ("て", "で") or t.get("pos") != "Particle":
            continue
        prev_pos = tokens[i - 1].get("pos", "")
        if prev_pos in ("Verb", "Auxiliary", "Adjective"):
            t["pos"] = "Auxiliary"
            t["lemma"] = "てる" if surface == "て" else "でる"


def postprocess_de_aru(tokens: list[dict]) -> None:
    """Fix copula で+ある/あり/あっ pattern based on context."""
    for i in range(len(tokens)):
        t = tokens[i]
        if t.get("surface") != "で":
            continue
        if i >= len(tokens) - 1:
            continue

        nxt = tokens[i + 1]
        nxt_surface = nxt.get("surface", "")

        if nxt_surface not in ("ある", "あり", "あれ", "あっ"):
            continue

        prev_pos = tokens[i - 1].get("pos", "") if i > 0 else ""
        is_past = nxt_surface == "あっ"

        if is_past and prev_pos in ("Noun", "Pronoun", "Suffix"):
            # N+であった: で→Particle, あっ→Verb
            t["pos"] = "Particle"
            t["lemma"] = "で"
            if nxt.get("pos") == "Auxiliary":
                nxt["pos"] = "Verb"
                nxt["lemma"] = "ある"
        elif is_past:
            # Na-adj+であった or other: keep as-is (copula chain)
            pass
        else:
            # Present/continuous forms: で→Auxiliary(だ), ある→Verb
            t["pos"] = "Auxiliary"
            t["lemma"] = "だ"
            if nxt.get("pos") == "Auxiliary":
                nxt["pos"] = "Verb"
                nxt["lemma"] = "ある"


def postprocess_taihen(tokens: list[dict]) -> None:
    """Fix 大変 before な: Adverb -> Adjective (na-adjective use)."""
    for i, t in enumerate(tokens):
        if t.get("surface") == "大変" and t.get("pos") == "Adverb":
            if i < len(tokens) - 1 and tokens[i + 1].get("surface") == "な":
                t["pos"] = "Adjective"


def postprocess_gozai_verb(tokens: list[dict]) -> None:
    """Fix ござい after おめでとう: Auxiliary -> Verb."""
    for i in range(1, len(tokens)):
        t = tokens[i]
        if t.get("surface") == "ござい" and t.get("pos") == "Auxiliary":
            if tokens[i - 1].get("surface") == "おめでとう":
                t["pos"] = "Verb"
                t["lemma"] = "ござる"


def postprocess_you_noun(tokens: list[dict]) -> None:
    """Fix よう: Auxiliary -> Noun (形式名詞)."""
    for t in tokens:
        if t.get("surface") == "よう" and t.get("pos") == "Auxiliary":
            t["pos"] = "Noun"
            t["lemma"] = "よう"


def postprocess_sou_aux(tokens: list[dict]) -> None:
    """Fix そう after Auxiliary (しまい etc.): Adverb -> Auxiliary (様態)."""
    for i in range(1, len(tokens)):
        t = tokens[i]
        if t.get("surface") != "そう" or t.get("pos") != "Adverb":
            continue
        prev_pos = tokens[i - 1].get("pos", "")
        if prev_pos == "Auxiliary":
            t["pos"] = "Auxiliary"


def postprocess_n_kuruwa(tokens: list[dict]) -> None:
    """Fix ん in kuruwa dialect (after あり): Particle -> Auxiliary."""
    for i in range(1, len(tokens)):
        t = tokens[i]
        if t.get("surface") != "ん" or t.get("pos") != "Particle":
            continue
        prev = tokens[i - 1]
        if prev.get("surface") in ("あり", "あっ"):
            t["pos"] = "Auxiliary"
            t["lemma"] = "ん"


def postprocess_nai_context(tokens: list[dict]) -> None:
    """No-op: ない POS is too context-dependent for simple rules."""
    pass


def postprocess_nara_verb(tokens: list[dict]) -> None:
    """Fix なら before ない/なく/なかっ: Auxiliary -> Verb(なる)."""
    for i in range(len(tokens) - 1):
        t = tokens[i]
        if t.get("surface") != "なら":
            continue
        if t.get("pos") not in ("Auxiliary", "Particle"):
            continue
        nxt_surface = tokens[i + 1].get("surface", "")
        if nxt_surface in ("ない", "なく", "なかっ"):
            t["pos"] = "Verb"
            t["lemma"] = "なる"
