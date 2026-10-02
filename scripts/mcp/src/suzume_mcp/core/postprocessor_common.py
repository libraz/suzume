"""Shared mutation reporting and reference-analysis helpers."""

from collections.abc import Callable
from functools import wraps

from .mecab import mecab_analyze


def reports_mutation(processor: Callable[[list[dict]], object]) -> Callable[[list[dict]], bool]:
    """Adapt a mutating postprocessor to the shared changed/not-changed contract."""

    @wraps(processor)
    def wrapped(tokens: list[dict]) -> bool:
        before = [token.copy() for token in tokens]
        processor(tokens)
        return tokens != before

    return wrapped


def _raw_analysis(text: str) -> tuple[int, dict[int, dict]]:
    """Analyze the untouched text, returning its token count and start index."""
    index: dict[int, dict] = {}
    offset = 0
    count = 0
    for token in mecab_analyze(text):
        index[offset] = token
        offset += len(token.get("surface", ""))
        count += 1
    return count, index
