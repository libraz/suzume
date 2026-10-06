#!/usr/bin/env python3
"""Generate the compact WASM representation of the source-defined L1 entries."""

from __future__ import annotations

import argparse
import ast
import re
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Entry:
    surface: str
    pos: str
    extended_pos: str
    lemma: str


HELPERS = {
    "particle": ("POS::Particle", None),
    "aux": ("POS::Auxiliary", None),
    "verb": ("POS::Verb", None),
    "adj": ("POS::Adjective", None),
    "na_adj": ("POS::Adjective", "EPOS::AdjNaAdj"),
    "det": ("POS::Determiner", "EPOS::Determiner"),
    "quotative_det": ("POS::Determiner", "EPOS::DeterminerQuotative"),
    "formal_noun": ("POS::Noun", "EPOS::NounFormal"),
    "noun": ("POS::Noun", "EPOS::Noun"),
    "noun_number": ("POS::Noun", "EPOS::NounNumber"),
    "conj": ("POS::Conjunction", "EPOS::Conjunction"),
    "adv": ("POS::Adverb", "EPOS::Adverb"),
    "quotative_adv": ("POS::Adverb", "EPOS::AdverbQuotative"),
    "suffix_trace": ("POS::Suffix", "EPOS::SuffixTrace"),
    "interrogative_adv": ("POS::Adverb", "EPOS::AdverbInterrogative"),
    "suffix": ("POS::Suffix", "EPOS::Suffix"),
    "suffix_recent_completion": ("POS::Suffix", "EPOS::SuffixRecentCompletion"),
    "suffix_tendency": ("POS::Suffix", "EPOS::SuffixTendency"),
    "prefix": ("POS::Prefix", "EPOS::Prefix"),
    "pronoun": ("POS::Pronoun", "EPOS::Pronoun"),
    "pronoun_interrogative": ("POS::Pronoun", "EPOS::PronounInterrogative"),
    "intj": ("POS::Interjection", "EPOS::Interjection"),
}


def strip_comments(source: str) -> str:
    return re.sub(r"//[^\n]*", "", source)


def split_arguments(text: str) -> list[str]:
    result: list[str] = []
    begin = 0
    quoted = False
    escaped = False
    for index, char in enumerate(text):
        if quoted:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                quoted = False
        elif char == '"':
            quoted = True
        elif char == ",":
            result.append(text[begin:index].strip())
            begin = index + 1
    result.append(text[begin:].strip())
    return result


def parse_string(value: str) -> str:
    return ast.literal_eval(value)


def parse_entries(path: Path) -> list[Entry]:
    source = strip_comments(path.read_text(encoding="utf-8"))
    matches: list[tuple[int, Entry]] = []
    pattern = "|".join(HELPERS)
    for match in re.finditer(rf"\b({pattern})\s*\(([^()]*)\)\s*,", source, re.DOTALL):
        helper = match.group(1)
        args = split_arguments(match.group(2))
        pos, fixed_epos = HELPERS[helper]
        if not args or not args[0].startswith('"'):
            raise ValueError(f"{path}: invalid {helper} entry: {match.group(0)!r}")
        surface = parse_string(args[0])
        if helper == "particle":
            if len(args) != 2:
                raise ValueError(f"{path}: expected two arguments for particle: {match.group(0)!r}")
            lemma = ""
            epos = args[1]
        elif fixed_epos is None:
            if len(args) != 3:
                raise ValueError(f"{path}: expected three arguments for {helper}: {match.group(0)!r}")
            lemma = parse_string(args[1])
            epos = args[2]
        else:
            if len(args) not in (1, 2):
                raise ValueError(f"{path}: invalid {helper} arguments: {match.group(0)!r}")
            lemma = parse_string(args[1]) if len(args) == 2 else ""
            epos = fixed_epos
        matches.append((match.start(), Entry(surface, pos, epos, lemma)))

    for match in re.finditer(
        r'\{\s*("(?:[^"\\]|\\.)*")\s*,\s*(POS::\w+)\s*,\s*(EPOS::\w+)\s*,\s*("(?:[^"\\]|\\.)*")\s*\}',
        source,
    ):
        matches.append(
            (
                match.start(),
                Entry(parse_string(match.group(1)), match.group(2), match.group(3), parse_string(match.group(4))),
            )
        )
    return [entry for _, entry in sorted(matches, key=lambda item: item[0])]


def append_string(pool: bytearray, offsets: dict[str, int], value: str) -> int:
    if value not in offsets:
        offsets[value] = len(pool)
        pool.extend(value.encode("utf-8"))
        pool.append(0)
    return offsets[value]


def format_bytes(data: bytes) -> str:
    return "\n".join(
        "    " + ", ".join(f"0x{value:02x}" for value in data[start : start + 16]) + ","
        for start in range(0, len(data), 16)
    )


def generate(entries: list[Entry]) -> str:
    ordered = sorted(enumerate(entries), key=lambda item: item[1].surface.encode("utf-8"))
    pool = bytearray(b"\0")
    offsets = {"": 0}
    records = []
    for _, entry in ordered:
        surface_offset = append_string(pool, offsets, entry.surface)
        lemma_offset = append_string(pool, offsets, entry.lemma)
        if max(surface_offset, lemma_offset) > 0xFFFF:
            raise ValueError("L1 string pool exceeds the packed 16-bit offset range")
        pos = entry.pos.replace("POS::", "core::PartOfSpeech::")
        extended_pos = entry.extended_pos.replace("EPOS::", "core::ExtendedPOS::")
        records.append(
            f"    {{{surface_offset}, {lemma_offset}, static_cast<uint8_t>({pos}), "
            f"static_cast<uint8_t>({extended_pos})}},"
        )
    return f"""// Generated by scripts/generate_packed_core_entries.py. Do not edit.
#include "dictionary/entries/packed_core_entries.h"

#include "core/types.h"

namespace suzume::dictionary::entries {{
namespace {{

constexpr uint8_t kStringData[] = {{
{format_bytes(pool)}
}};

constexpr PackedCoreEntry kEntries[] = {{
{chr(10).join(records)}
}};

}}  // namespace

PackedCoreEntryRange getPackedCoreEntries() {{
  return {{kEntries, sizeof(kEntries) / sizeof(kEntries[0]), kStringData}};
}}

}}  // namespace suzume::dictionary::entries
"""


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("inputs", type=Path, nargs="+")
    args = parser.parse_args()
    entries = [entry for path in args.inputs for entry in parse_entries(path)]
    if not entries:
        raise ValueError("No L1 entries found")
    source = generate(entries)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding="utf-8") != source:
        args.output.write_text(source, encoding="utf-8")


if __name__ == "__main__":
    main()
