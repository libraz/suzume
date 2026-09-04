"""Main orchestration module ported from SuzumeUtils.pm get_expected_tokens() etc."""

import unicodedata

import regex

from .constants import SLANG_ADJ_STEMS, TEXT_SYMBOLS
from .mecab import mecab_analyze
from .merge_rules import apply_suzume_merge
from .pos_mapping import correct_mecab_pos, map_mecab_pos, normalize_pos
from .postprocessors import (
    postprocess_mecab_tokens,
    postprocessor_rules,
    preprocess_for_mecab,
    repair_kanji_prefix_before_kana_noun,
    repair_kko_nominalizer,
    split_transparent_suru_te_adverb,
)
from .split_rules import apply_suzume_split


def _oracle_text(text: str) -> str:
    """Return the single coordinate space shared by MeCab and merge rules."""
    compact = "".join(char for char in text if not char.isspace())
    normalized = []
    for char in compact:
        # Keep this coordinate space aligned with Normalizer::normalize(): a
        # zero-width space is formatting noise, not a token boundary or a
        # surface character available to the oracle.
        if char == "\u200b":
            continue
        char = char.translate(_FULLWIDTH_TABLE)
        if "\uff66" <= char <= "\uff9d":
            # Mirror Normalizer::halfwidthKatakanaToFullwidth. The two
            # half-width voiced marks (FF9E/FF9F) deliberately remain outside
            # this range because the native table does not map them.
            char = unicodedata.normalize("NFKC", char)
        if char in ("\u309b", "\u309c", "\uff9e", "\uff9f") and normalized:
            combining = "\u3099" if char in ("\u309b", "\uff9e") else "\u309a"
            combined = unicodedata.normalize("NFC", normalized[-1] + combining)
            if len(combined) == 1:
                normalized[-1] = combined
                continue
        normalized.append(char)
    return unicodedata.normalize("NFC", "".join(normalized))


def _is_deliberately_removed_symbol(surface: str) -> bool:
    """Return whether C++ default options deliberately remove punctuation.

    Content symbols and emoji are emitted as OTHER by the tokenizer, so they
    must stay in the expected-token stream as well.  Only punctuation is a
    removable boundary under the default options.
    """
    return (
        surface not in TEXT_SYMBOLS
        and bool(surface)
        and all(unicodedata.category(char).startswith("P") for char in surface)
    )


def _is_emoji_cluster_component(char: str) -> bool:
    """Mirror the emoji and joiner code points grouped by the C++ tokenizer."""
    codepoint = ord(char)
    return (
        0x1F300 <= codepoint <= 0x1FBFF
        or codepoint == 0x200D
        or 0xFE00 <= codepoint <= 0xFE0F
        or codepoint == 0x20E3
        or 0xE0020 <= codepoint <= 0xE007F
    )


def _merge_emoji_clusters(tokens: list[dict]) -> None:
    """Coalesce emoji ZWJ sequences into the tokenizer's single OTHER token."""
    merged: list[dict] = []
    index = 0
    while index < len(tokens):
        token = tokens[index]
        surface = token.get("surface", "")
        if not surface or not all(_is_emoji_cluster_component(char) for char in surface):
            merged.append(token)
            index += 1
            continue

        cluster = [surface]
        index += 1
        while index < len(tokens):
            following = tokens[index].get("surface", "")
            if not following or not all(_is_emoji_cluster_component(char) for char in following):
                break
            cluster.append(following)
            index += 1
        combined = "".join(cluster)
        merged.append({"surface": combined, "pos": "その他", "pos_sub1": "", "lemma": combined})
    tokens[:] = merged


def _merge_ideographic_variation_selectors(tokens: list[dict]) -> None:
    """Keep an ideographic variation selector inside its surrounding kanji word."""
    merged: list[dict] = []
    index = 0
    while index < len(tokens):
        selector = tokens[index].get("surface", "")
        if (
            merged
            and selector
            and all(0xE0100 <= ord(char) <= 0xE01EF for char in selector)
            and index + 1 < len(tokens)
            and regex.fullmatch(r"\p{Han}+", merged[-1].get("surface", ""))
            and regex.fullmatch(r"\p{Han}+", tokens[index + 1].get("surface", ""))
        ):
            following = tokens[index + 1].get("surface", "")
            combined = merged.pop().get("surface", "") + selector + following
            merged.append({"surface": combined, "pos": "名詞", "pos_sub1": "一般", "lemma": combined})
            index += 2
            continue
        merged.append(tokens[index])
        index += 1
    tokens[:] = merged


def get_mecab_tokens(text: str) -> list[dict]:
    """Get MeCab tokens with slang handling and POS mapping."""
    normalized_text = _oracle_text(text)
    processed_text, replacements, _ = preprocess_for_mecab(normalized_text)
    raw_tokens = mecab_analyze(processed_text)

    tokens = []
    for t in raw_tokens:
        tokens.append(
            {
                "surface": t["surface"],
                "pos": map_mecab_pos(t),
                "lemma": t["lemma"] if t.get("lemma") and t["lemma"] != "*" else t["surface"],
            }
        )

    postprocess_mecab_tokens(tokens, normalized_text, replacements)
    return tokens


def _reject_lossy_symbol_drop(surface: str, text: str) -> None:
    """Fail when the symbol filter is about to discard real text.

    Dropping punctuation is intended; dropping a word is not. MeCab labels
    anything IPADIC does not know as 記号, so a character outside its dictionary
    would silently vanish from the expected tokens and leave a test asserting a
    segmentation of text that is not the input. Correct the token's POS in
    correct_mecab_pos instead of letting it reach this filter.
    """
    if _is_deliberately_removed_symbol(surface):
        return
    carries_text = [char for char in surface if not char.isspace()]
    if carries_text:
        raise RuntimeError(
            f"symbol filter would drop {''.join(carries_text)!r} from {text!r} "
            f"(token {surface!r}): MeCab labelled real text as 記号. "
            "Add a correct_mecab_pos rule for it rather than losing the surface."
        )


def _reject_surface_mismatch(
    tokens: list[dict],
    text: str,
    surface_rule: str | None,
    *,
    normalize_fullwidth: bool = False,
) -> None:
    """Fail when normalization duplicates or loses an unexplained surface."""
    reconstructed = "".join(token.get("surface", "") for token in tokens)
    expected = "".join(char for char in text if not char.isspace())
    canonical_prolonged = regex.sub(r"ー+", "ー", expected)
    if reconstructed == canonical_prolonged:
        # The long-vowel rule may fire after another merge rule.  Its public
        # label records only the first rule, so use the reconstructed surface
        # itself—not that lossy label—to recognize this one declared change.
        expected = canonical_prolonged
    if normalize_fullwidth:
        expected = expected.translate(_FULLWIDTH_TABLE)
    if reconstructed != expected:
        raise RuntimeError(
            f"normalized token surfaces do not reconstruct the input: "
            f"expected {expected!r}, got {reconstructed!r} for {text!r}"
        )


def get_expected_tokens(text: str, suzume_tokens: list[dict] | None = None) -> tuple[list[dict], str, str]:
    """Get expected tokens: MeCab + Suzume rule corrections.

    Returns:
        Tuple of (tokens, source_label, applied_rule).
    """
    # Get raw MeCab tokens
    normalized_text = _oracle_text(text)
    processed_text, replacements, preprocess_rules = preprocess_for_mecab(normalized_text)
    raw_tokens = mecab_analyze(processed_text)
    postprocess_mecab_tokens(raw_tokens, normalized_text, replacements)
    repair_kko_nominalizer(raw_tokens)
    _merge_ideographic_variation_selectors(raw_tokens)

    # Fix MeCab POS errors (before POS mapping)
    correct_mecab_pos(raw_tokens)
    _merge_emoji_clusters(raw_tokens)
    # Restore inflection boundaries the dictionary lexicalized away.  Runs here
    # because the decision needs the reading, which the merge pass drops.
    split_transparent_suru_te_adverb(raw_tokens)
    repair_kanji_prefix_before_kana_noun(raw_tokens)

    # Apply Suzume merge rules
    merged, merge_rule = apply_suzume_merge(raw_tokens, normalized_text)

    # Apply Suzume split rules
    split_tokens, split_rule = apply_suzume_split(merged)
    _reject_surface_mismatch(split_tokens, normalized_text, merge_rule)

    # Combine rule names
    applied_rule = merge_rule or split_rule
    if merge_rule and split_rule:
        applied_rule = f"{merge_rule}+{split_rule}"
    if preprocess_rules:
        applied_rule = "+".join((*preprocess_rules, *(rule for rule in (applied_rule,) if rule)))

    # Map POS and filter symbols
    tokens = []
    removed_symbol = False
    retained_surface_parts = []
    for t in split_tokens:
        pos = normalize_pos(map_mecab_pos(t))
        surface = t.get("surface", "")
        if pos == "Symbol" or _is_deliberately_removed_symbol(surface):
            _reject_lossy_symbol_drop(t.get("surface", ""), text)
            removed_symbol = True
            continue
        retained_surface_parts.append(surface)
        tokens.append(
            {
                "surface": t.get("surface", ""),
                "pos": pos,
                "pos_sub1": t.get("pos_sub1"),
                "pos_sub2": t.get("pos_sub2"),
                "lemma": t["lemma"] if t.get("lemma") and t["lemma"] != "*" else t.get("surface", ""),
            }
        )
    if removed_symbol and applied_rule is None:
        applied_rule = "symbol-filter"

    # Postprocessors mutate the same token list in a deliberately fixed order.
    # Preserve the first change as the public applied-rule label.
    for label, postprocessor in postprocessor_rules():
        if postprocessor(tokens) and applied_rule is None:
            applied_rule = label

    # Normalize full-width alphanumeric to half-width
    fullwidth_applied = False
    for t in tokens:
        for key in ("surface", "lemma"):
            val = t.get(key)
            if val is None:
                continue
            new_val = val.translate(_FULLWIDTH_TABLE)
            if new_val != val:
                t[key] = new_val
                fullwidth_applied = True

    if fullwidth_applied and applied_rule is None:
        applied_rule = "fullwidth-normalize"

    # Merge/split validation above catches structural rules, while this second
    # gate protects every context-dependent postprocessor as well. The only
    # permitted surface changes after that first gate are the public symbol
    # filter and full-width alphanumeric normalization.
    _reject_surface_mismatch(
        tokens,
        "".join(retained_surface_parts),
        merge_rule,
        normalize_fullwidth=True,
    )

    if applied_rule:
        return tokens, "MeCab+SuzumeRules", applied_rule

    return tokens, "MeCab", ""


def tokens_match(a: list[dict], b: list[dict], *, compare_lemma: bool = True) -> bool:
    """Compare two token arrays for equality (surface, pos, and lemma by default)."""
    if len(a) != len(b):
        return False
    for ta, tb in zip(a, b, strict=True):
        if ta.get("surface", "") != tb.get("surface", ""):
            return False
        pos_a = normalize_pos(ta.get("pos", ""))
        pos_b = normalize_pos(tb.get("pos", ""))
        if pos_a != pos_b:
            return False
        if compare_lemma:
            lemma_a = ta.get("lemma") or ta.get("surface", "")
            lemma_b = tb.get("lemma") or tb.get("surface", "")
            if lemma_a != lemma_b:
                return False
    return True


def format_expected(tokens: list[dict]) -> list[dict]:
    """Format tokens for JSON output. Always includes lemma."""
    result = []
    for t in tokens:
        entry: dict = {"surface": t["surface"], "pos": t["pos"]}
        lemma = t.get("lemma", "")
        entry["lemma"] = lemma if lemma else t["surface"]
        result.append(entry)
    return result


def get_char_types(s: str) -> list[str]:
    """Get character types present in a string."""
    types: set[str] = set()
    for ch in s:
        code = ord(ch)
        if 0x3040 <= code <= 0x309F:
            types.add("hiragana")
        elif 0x30A0 <= code <= 0x30FF:
            types.add("katakana")
        elif 0x4E00 <= code <= 0x9FFF:
            types.add("kanji")
        elif 0x0041 <= code <= 0x007A:
            types.add("alpha")
        elif 0x0030 <= code <= 0x0039:
            types.add("digit")
    return list(types)


def get_suzume_rule(text: str) -> str:
    """Check if text matches Suzume normalization rules."""
    from .constants import NAI_ADJECTIVES, TARI_ADVERB_STEMS

    for adj in NAI_ADJECTIVES:
        if adj in text:
            return "nai-adjective"

    if regex.search(r"\d+[^\d\s]", text):
        return "number+unit"

    if regex.search(r"\d+年\d+月\d+日", text):
        return "date"

    for stem in SLANG_ADJ_STEMS:
        if regex.search(regex.escape(stem) + r"[いかくけさ]", text):
            return "slang-adjective"

    if regex.search(r"\p{Han}+然と", text):
        return "tari-adverb"
    for stem in TARI_ADVERB_STEMS:
        if stem + "と" in text:
            return "tari-adverb"

    if regex.search(r"\p{Han}{2,}", text):
        return "kanji-compound"

    if regex.search(r"[\u30A0-\u30FF]{4,}", text):
        return "katakana-compound"

    return ""


# Full-width ASCII to ASCII translation table. This intentionally includes
# punctuation as well as digits and letters: Normalizer::fullwidthToHalfwidth
# is the sole width-folding boundary before pre-tokenization.
_FULLWIDTH_TABLE = str.maketrans(
    "".join(chr(codepoint) for codepoint in range(0xFF01, 0xFF5F)),
    "".join(chr(codepoint) for codepoint in range(0x21, 0x7F)),
)
