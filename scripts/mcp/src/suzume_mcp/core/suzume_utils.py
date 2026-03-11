"""Main orchestration module ported from SuzumeUtils.pm get_expected_tokens() etc."""

import regex

from .constants import SLANG_ADJ_STEMS
from .mecab import mecab_analyze
from .merge_rules import apply_suzume_merge
from .pos_mapping import correct_mecab_pos, map_mecab_pos, normalize_pos
from .postprocessors import (
    postprocess_copula_neg,
    postprocess_de_aru,
    postprocess_de_particle,
    postprocess_gozai_verb,
    postprocess_n_kuruwa,
    postprocess_nai_context,
    postprocess_taihen,
    postprocess_demo,
    postprocess_ii,
    postprocess_ikaga,
    postprocess_iru_aux,
    postprocess_mecab_tokens,
    postprocess_na_adj_noun,
    postprocess_nara_verb,
    postprocess_sou,
    postprocess_sou_aux,
    postprocess_tsuke_noun,
    postprocess_you_noun,
    preprocess_for_mecab,
)
from .split_rules import apply_suzume_split


def get_mecab_tokens(text: str) -> list[dict]:
    """Get MeCab tokens with slang handling and POS mapping."""
    processed_text, replacements = preprocess_for_mecab(text)
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

    postprocess_mecab_tokens(tokens, text, replacements)
    return tokens


def get_expected_tokens(text: str, suzume_tokens: list[dict] | None = None) -> tuple[list[dict], str, str]:
    """Get expected tokens: MeCab + Suzume rule corrections.

    Returns:
        Tuple of (tokens, source_label, applied_rule).
    """
    # Get raw MeCab tokens
    processed_text, replacements = preprocess_for_mecab(text)
    raw_tokens = mecab_analyze(processed_text)
    postprocess_mecab_tokens(raw_tokens, text, replacements)

    # Fix MeCab POS errors (before POS mapping)
    correct_mecab_pos(raw_tokens)

    # Apply Suzume merge rules
    merged, merge_rule = apply_suzume_merge(raw_tokens, text)

    # Apply Suzume split rules
    split_tokens, split_rule = apply_suzume_split(merged)

    # Combine rule names
    applied_rule = merge_rule or split_rule
    if merge_rule and split_rule:
        applied_rule = f"{merge_rule}+{split_rule}"

    # Map POS and filter symbols
    tokens = []
    for t in split_tokens:
        pos = normalize_pos(map_mecab_pos(t))
        if pos == "Symbol":
            continue
        tokens.append(
            {
                "surface": t.get("surface", ""),
                "pos": pos,
                "lemma": t["lemma"] if t.get("lemma") and t["lemma"] != "*" else t.get("surface", ""),
            }
        )

    # Post-processing: context-dependent POS normalization
    postprocess_sou(tokens)
    postprocess_ikaga(tokens)
    postprocess_demo(tokens)
    postprocess_ii(tokens)
    postprocess_iru_aux(tokens)
    postprocess_de_particle(tokens)
    postprocess_de_aru(tokens)
    postprocess_gozai_verb(tokens)
    postprocess_taihen(tokens)
    postprocess_na_adj_noun(tokens)
    postprocess_tsuke_noun(tokens)
    postprocess_copula_neg(tokens)
    postprocess_you_noun(tokens)
    postprocess_sou_aux(tokens)
    postprocess_nara_verb(tokens)
    postprocess_n_kuruwa(tokens)
    postprocess_nai_context(tokens)

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

    if applied_rule:
        return tokens, "MeCab+SuzumeRules", applied_rule or ""

    # Check for slang adjective rule
    for slang in SLANG_ADJ_STEMS:
        if regex.search(regex.escape(slang) + r"[いかくけさ]", text):
            return tokens, "MeCab", "slang-adjective"

    return tokens, "MeCab", ""


def tokens_match(a: list[dict], b: list[dict]) -> bool:
    """Compare two token arrays for equality (surface, pos). Lemma skipped."""
    if len(a) != len(b):
        return False
    for ta, tb in zip(a, b, strict=True):
        if ta.get("surface", "") != tb.get("surface", ""):
            return False
        pos_a = normalize_pos(ta.get("pos", ""))
        pos_b = normalize_pos(tb.get("pos", ""))
        if pos_a != pos_b:
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
    if regex.search(r"やばい|やばかっ|やばく", text):
        return "slang-adjective"

    for stem in TARI_ADVERB_STEMS:
        if stem + "と" in text:
            return "tari-adverb"

    if regex.search(r"\p{Han}{2,}", text):
        return "kanji-compound"

    if regex.search(r"[\u30A0-\u30FF]{4,}", text):
        return "katakana-compound"

    return ""


# Full-width to half-width translation table
_FULLWIDTH_TABLE = str.maketrans(
    "０１２３４５６７８９ＡＢＣＤＥＦＧＨＩＪＫＬＭＮＯＰＱＲＳＴＵＶＷＸＹＺａｂｃｄｅｆｇｈｉｊｋｌｍｎｏｐｑｒｓｔｕｖｗｘｙｚ",
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
)
