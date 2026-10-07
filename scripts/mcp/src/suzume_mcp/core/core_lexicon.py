"""Read the source L2 lexicon for normalization rules shared with the core."""

from functools import lru_cache
from pathlib import Path

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
    return frozenset(entry[0] for entry in core_entries(filename) if entry)


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


def adjective_garu_stems() -> dict[str, str]:
    """Map L2 adjective stems that license productive がる to their lemma."""
    stems: dict[str, str] = {}
    for entry in core_entries("adjectives.tsv"):
        if len(entry) < 3 or entry[1] != "ADJECTIVE":
            continue
        surface, _, conjugation = entry[:3]
        if conjugation == "NA_ADJ":
            stems[surface] = surface
        elif conjugation == "I_ADJ" and surface.endswith("い"):
            stems[surface[:-1]] = surface
    return stems


def kana_i_adjective_lemmas() -> tuple[str, ...]:
    """Return the L2 i-adjectives spelled wholly in hiragana, longest first."""
    lemmas = (
        entry[0]
        for entry in core_entries("adjectives.tsv")
        if len(entry) >= 3
        and entry[1] == "ADJECTIVE"
        and entry[2] == "I_ADJ"
        and entry[0].endswith("い")
        and all("ぁ" <= char <= "ゖ" for char in entry[0])
    )
    return tuple(sorted(lemmas, key=len, reverse=True))


def listed_interjections() -> frozenset[str]:
    """Return the L2 interjection headwords (おはよう, おめでとう)."""
    return frozenset(
        entry[0] for entry in core_entries("expressions.tsv") if len(entry) >= 2 and entry[1] == "INTERJECTION"
    )


_O_ROW = frozenset("おこごそぞとどのほぼぽもよろ")


def clipped_greetings() -> dict[str, str]:
    """Map the clipped spelling of a listed kana greeting to its headword (ありがと → ありがとう).

    Casual speech drops the long vowel う after the greeting's closing o-row mora.
    """
    return {
        entry[0][:-1]: entry[0]
        for entry in core_entries("expressions.tsv")
        if len(entry) >= 2
        and entry[1] == "INTERJECTION"
        and len(entry[0]) >= 3
        and entry[0].endswith("う")
        and entry[0][-2] in _O_ROW
        and all("ぁ" <= char <= "ゖ" for char in entry[0])
    }
