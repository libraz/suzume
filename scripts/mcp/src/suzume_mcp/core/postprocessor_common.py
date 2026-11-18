"""Shared mutation reporting and reference-analysis helpers."""

from collections.abc import Callable
from functools import wraps

from .mecab import mecab_analyze

# Parts of speech whose continuative takes the conjunctive て/で.
TE_FORM_HOST_POS = frozenset({"Verb", "Adjective", "Auxiliary"})


def follows_te_form(tokens: list[dict], te_idx: int) -> bool:
    """Whether ``tokens[te_idx]`` is the conjunctive て/で of a predicate.

    After a nominal or an adverb, で is a case particle (東京でみた,
    ただでもらった), and after a na-adjective stem it is the copula (静かでいる),
    so what follows it is not a subsidiary.
    """
    return (
        0 < te_idx < len(tokens)
        and tokens[te_idx].get("surface") in ("て", "で")
        and tokens[te_idx].get("pos") == "Particle"
        and tokens[te_idx - 1].get("pos") in TE_FORM_HOST_POS
    )


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
