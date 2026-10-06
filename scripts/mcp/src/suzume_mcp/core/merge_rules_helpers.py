"""Span, run, and cell helpers shared by the merge step modules."""

import regex

from .constants import FIXED_INFLECTED_FUNCTION_UNITS, LETTER_FORMULAS
from .merge_postprocessors import nidan_cell


def _consume_span(tokens: list[dict], start: int, length: int) -> tuple[str, int]:
    """Join surfaces from `start` until they cover `length` characters; return them and the end index."""
    consumed, end = "", start
    while end < len(tokens) and len(consumed) < length:
        consumed += tokens[end].get("surface", "")
        end += 1
    return consumed, end


def _covered_length(tokens: list[dict], start: int, length: int) -> tuple[int, int]:
    """Like `_consume_span`, but return how many characters the joined surfaces cover."""
    consumed, end = _consume_span(tokens, start, length)
    return len(consumed), end


_UTTERANCE_FINAL_PARTICLE_HEADS = frozenset("よねなかぞぜわさ")


_FIXED_INFLECTED_FUNCTION_UNITS = tuple(sorted(FIXED_INFLECTED_FUNCTION_UNITS, key=len, reverse=True))


def _heads_nidan_cell(tokens: list[dict], index: int) -> bool:
    """Whether the token at ``index`` is the stem of a classical 二段 finite cell."""
    following = tokens[index + 1] if index + 1 < len(tokens) else None
    return nidan_cell(tokens[index], following) is not None


def _kanji_noun_run(tokens: list[dict], start: int) -> tuple[int, str]:
    """Return the complete mergeable kanji-noun run beginning at ``start``."""
    if start >= len(tokens):
        return start, ""
    token = tokens[start]
    if not (
        regex.match(r"^[\p{Han}]+$", token.get("surface", ""))
        and token.get("pos") == "名詞"
        and token.get("pos_sub1", "") not in ("接尾", "固有名詞", "副詞可能")
        and token.get("surface", "") not in LETTER_FORMULAS
    ):
        return start, ""
    # A kanji the dictionary reads as a bare noun is the stem of a classical 二段
    # verb when the kana after it completes a finite cell (老|ゆる, 絶|ゆれ).
    # Absorbing it into a compound would bury a verb inside a noun that is not a
    # word, so the run stops before such a stem.
    if _heads_nidan_cell(tokens, start):
        return start, ""

    index = start + 1
    combined = token.get("surface", "")
    while index < len(tokens):
        following = tokens[index]
        surface = following.get("surface", "")
        is_mergeable = (
            regex.match(r"^[\p{Han}]+$", surface)
            and following.get("pos") == "名詞"
            and following.get("pos_sub1", "") not in ("接尾", "固有名詞", "形容動詞語幹", "副詞可能", "数", "代名詞")
        )
        # A pronoun is a phrase of its own (俺|失敗, 彼|仕事); it opens a
        # compound only with a bound one-kanji element (何+気).
        if surface in LETTER_FORMULAS:
            is_mergeable = False
        if is_mergeable and index == start + 1 and token.get("pos_sub1", "") == "代名詞":
            is_mergeable = len(surface) == 1
        if not is_mergeable or _heads_nidan_cell(tokens, index):
            break
        combined += surface
        index += 1

    if index < len(tokens) and tokens[index].get("surface", "") in ("付け", "者", "人"):
        combined += tokens[index].get("surface", "")
        index += 1
    return index, combined
