"""Read the source L2 lexicon for normalization rules shared with the core."""

from collections.abc import Callable
from functools import lru_cache
from pathlib import Path
from typing import Any

from suzume_mcp.config import PROJECT_ROOT


@lru_cache(maxsize=16)
def _read_entries(path_text: str, modified_ns: int) -> tuple[tuple[str, ...], ...]:
    del modified_ns  # Part of the cache key; the file contents are read below.
    path = Path(path_text)
    entries: list[tuple[str, ...]] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        entries.append(tuple(stripped.split("\t")))
    return tuple(entries)


def core_entries(filename: str) -> tuple[tuple[str, ...], ...]:
    """Return current entries from one data/core TSV, hot-reloading on change."""
    path = PROJECT_ROOT / "data" / "core" / filename
    return _read_entries(str(path), path.stat().st_mtime_ns)


def core_headwords(filename: str) -> frozenset[str]:
    """Return the surfaces registered in one source L2 lexicon."""
    path = PROJECT_ROOT / "data" / "core" / filename
    return _read_headwords(str(path), path.stat().st_mtime_ns)


@lru_cache(maxsize=16)
def _read_headwords(path_text: str, modified_ns: int) -> frozenset[str]:
    """Return cached headwords for one lexicon revision."""
    return frozenset(entry[0] for entry in _read_entries(path_text, modified_ns) if entry)


@lru_cache(maxsize=16)
def _read_headwords_by_length(path_text: str, modified_ns: int) -> tuple[str, ...]:
    """Return cached longest-first headwords for one lexicon revision."""
    return tuple(
        sorted(
            (entry[0] for entry in _read_entries(path_text, modified_ns) if entry),
            key=len,
            reverse=True,
        )
    )


def core_headwords_by_length(filename: str) -> tuple[str, ...]:
    """Return current L2 surfaces longest-first, hot-reloading on change."""
    path = PROJECT_ROOT / "data" / "core" / filename
    return _read_headwords_by_length(str(path), path.stat().st_mtime_ns)


# Values derived from one revision of core_entries' result, keyed by builder.
# The entries tuple is cached per file revision, so its identity names the
# revision; holding it keeps the id from being reused.
_DERIVED: dict[tuple[Callable, int], tuple[tuple[tuple[str, ...], ...], Any]] = {}


def _derived(filename: str, build: Callable[[tuple[tuple[str, ...], ...]], Any]) -> Any:
    """Return ``build(core_entries(filename))``, rebuilt only when the entries change."""
    entries = core_entries(filename)
    key = (build, id(entries))
    cached = _DERIVED.get(key)
    if cached is None or cached[0] is not entries:
        cached = (entries, build(entries))
        _DERIVED[key] = cached
    return cached[1]


def _is_hiragana(text: str) -> bool:
    return all("ぁ" <= char <= "ゖ" for char in text)


def _garu_stems(entries: tuple[tuple[str, ...], ...]) -> dict[str, str]:
    stems: dict[str, str] = {}
    for entry in entries:
        if len(entry) < 3 or entry[1] != "ADJECTIVE":
            continue
        surface, _, conjugation = entry[:3]
        if conjugation == "NA_ADJ":
            stems[surface] = surface
        elif conjugation == "I_ADJ" and surface.endswith("い"):
            stems[surface[:-1]] = surface
    return stems


def adjective_garu_stems() -> dict[str, str]:
    """Map L2 adjective stems that license productive がる to their lemma."""
    return _derived("adjectives.tsv", _garu_stems)


def _kana_lemmas(pos: str, conjugation: str, ending: str) -> Callable[[tuple[tuple[str, ...], ...]], tuple[str, ...]]:
    """Build the longest-first hiragana lemmas of one POS and conjugation class."""

    def build(entries: tuple[tuple[str, ...], ...]) -> tuple[str, ...]:
        lemmas = (
            entry[0]
            for entry in entries
            if len(entry) >= 3
            and entry[1] == pos
            and entry[2] == conjugation
            and entry[0].endswith(ending)
            and _is_hiragana(entry[0])
        )
        return tuple(sorted(lemmas, key=len, reverse=True))

    return build


_KANA_I_ADJECTIVES = _kana_lemmas("ADJECTIVE", "I_ADJ", "い")
_KANA_ICHIDAN_VERBS = _kana_lemmas("VERB", "ICHIDAN", "る")


def kana_i_adjective_lemmas() -> tuple[str, ...]:
    """Return the L2 i-adjectives spelled wholly in hiragana, longest first."""
    return _derived("adjectives.tsv", _KANA_I_ADJECTIVES)


def kana_ichidan_verbs() -> tuple[str, ...]:
    """Return the L2 ichidan verbs spelled wholly in hiragana, longest first."""
    return _derived("verbs.tsv", _KANA_ICHIDAN_VERBS)


def _interjections(entries: tuple[tuple[str, ...], ...]) -> frozenset[str]:
    return frozenset(entry[0] for entry in entries if len(entry) >= 2 and entry[1] == "INTERJECTION")


def listed_interjections() -> frozenset[str]:
    """Return the L2 interjection headwords (おはよう, おめでとう)."""
    return _derived("expressions.tsv", _interjections)


_O_ROW = frozenset("おこごそぞとどのほぼぽもよろ")


def _clipped_greetings(entries: tuple[tuple[str, ...], ...]) -> dict[str, str]:
    clipped = {
        entry[0][:-1]: entry[0]
        for entry in entries
        if len(entry) >= 2
        and entry[1] == "INTERJECTION"
        and len(entry[0]) >= 3
        and entry[0].endswith("う")
        and entry[0][-2] in _O_ROW
        and _is_hiragana(entry[0])
    }
    return {clip: clipped[clip] for clip in sorted(clipped, key=len, reverse=True)}


def clipped_greetings() -> dict[str, str]:
    """Map the clipped spelling of a listed kana greeting to its headword (ありがと → ありがとう), longest first.

    Casual speech drops the long vowel う after the greeting's closing o-row mora.
    """
    return _derived("expressions.tsv", _clipped_greetings)
