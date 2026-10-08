"""Shared probes and conjugation helpers for merge post-processing."""

from .conjugation_rows import GODAN_A_ROW_TO_BASE
from .mecab import mecab_analyze

_A_ROW_TO_U_ROW = GODAN_A_ROW_TO_BASE


_CONTINUATIVE_PROBE_AUXILIARY = "ます"


def _plain(token: dict) -> dict:
    return {"surface": token.get("surface", ""), "pos": token.get("pos", ""), "lemma": token.get("lemma", "")}


def _probe_continuative(surface: str) -> list[dict] | None:
    """The reference dictionary's reading of a surface followed by 〜ます."""
    tokens = mecab_analyze(surface + _CONTINUATIVE_PROBE_AUXILIARY)
    if len(tokens) < 2 or tokens[-1].get("surface") != _CONTINUATIVE_PROBE_AUXILIARY:
        return None
    head = tokens[:-1]
    if head[-1].get("pos") != "動詞":
        return None
    if "".join(token.get("surface", "") for token in head) != surface:
        return None
    return [_plain(token) for token in head]


def reads_as_continuative(surface: str) -> bool:
    """Whether a surface is one verb standing in its continuative cell.

    The reference dictionary gives a continuative the verb tag only while what
    follows selects it, and calls the same surface a nominal otherwise. The
    polite auxiliary selects that cell for any verb, so appending it recovers
    the reading a construction needs when the tag has already been lost.
    """
    probed = _probe_continuative(surface)
    return probed is not None and len(probed) == 1


def _continuative_verb_tokens(surface: str) -> list[dict] | None:
    """Read a nominal back as the verb continuative it spells, if it is one.

    The reference dictionary drops the verb reading of a continuative when what
    follows is a form it does not know, and calls the run a noun instead. The
    polite auxiliary is a form it does know and selects exactly that cell, so
    appending it recovers the reading — and the boundary with it, since a
    nominal that fused a modifier in front comes back as its parts (雨降り as
    雨 + 降り). A genuine noun keeps its own reading under the same probe.

    A compound the dictionary holds as a headword survives the probe whole
    (山見, 月見), because the entry outranks the reading its parts would get.
    Probing each tail in turn recovers the boundary there: the tail is the verb
    and the head is what modifies it.
    """
    whole = _probe_continuative(surface)
    if whole is not None:
        return whole
    for split in range(1, len(surface)):
        tail = _probe_continuative(surface[split:])
        if tail is None or len(tail) != 1:
            continue
        head = mecab_analyze(surface[:split])
        if len(head) == 1 and head[0].get("pos") == "名詞" and head[0].get("surface") == surface[:split]:
            return [_plain(head[0]), *tail]
    return None
