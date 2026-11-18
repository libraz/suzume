#!/usr/bin/env python3
"""Keep the oracle's compound-verb V2 class in step with the core's lexicon.

The tokenizer joins V1+V2 compounds from a closed V2 allowlist in
src/analysis/join_compound_verb_lexicon.cpp. The normalization pipeline mirrors
that allowlist so an expectation never splits a compound the tokenizer joins.
Nothing in either suite notices when a V2 is added to the core alone: the
tokenizer starts merging a compound whose expectation still splits it, and the
mismatch surfaces as an unexplained failure in whatever case happens to contain
that verb.

This compares the two tables, including the joining restrictions the core rows
carry as flags.

Usage:
  scripts/check_compound_v2_sync.py
"""

from __future__ import annotations

import os
import re
import sys
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
LEXICON = "src/analysis/join_compound_verb_lexicon.cpp"
CONSTANTS = "scripts/mcp/src/suzume_mcp/core/constants.py"

# Readings held back from the oracle because the hiragana form is a productive
# contraction rather than a lexical V2 there. Each compound still merges in its
# kanji spelling, and constants.py records the reason.
READINGS_NOT_MIRRORED = {"とく", "とる", "はる"}

ROW = re.compile(
    r'\{"([^"]+)",\s*(?:"([^"]*)"|nullptr),\s*"[^"]*",\s*V2VerbType::(Godan|Ichidan)((?:\s*,\s*(?:true|false))*)\}'
)


def core_lexicon() -> tuple[dict[str, set[str]], set[str], set[str]]:
    """Core V2 forms per conjugation class, plus the two flag-restricted sets."""
    forms: dict[str, set[str]] = {"Godan": set(), "Ichidan": set()}
    not_after_suru: set[str] = set()
    suru_only: set[str] = set()
    rows = 0
    for line in Path(LEXICON).read_text().splitlines():
        match = ROW.search(line)
        if match is None:
            continue
        rows += 1
        surface, reading, kind, flags = match.groups()
        bools = [flag.strip() == "true" for flag in flags.split(",") if flag.strip()]
        joins_suru = bools[0] if len(bools) > 0 else True
        joins_general = bools[1] if len(bools) > 1 else True
        joins_reading = bools[2] if len(bools) > 2 else True
        joins_surface = bools[3] if len(bools) > 3 else True
        entry: set[str] = set()
        if joins_surface:
            entry.add(surface)
        if reading and joins_reading:
            entry.add(reading)
        forms[kind] |= entry
        if not joins_suru:
            not_after_suru |= entry
        if not joins_general:
            suru_only |= entry
    if rows == 0:
        raise SystemExit(f"❌ {LEXICON}: no V2 rows parsed — has the table's shape changed?")
    return forms, not_after_suru, suru_only


def main() -> int:
    os.chdir(REPOSITORY_ROOT)

    sys.path.insert(0, "scripts/mcp/src")
    from suzume_mcp.core.constants import (
        COMPOUND_VERB_V2_GODAN,
        COMPOUND_VERB_V2_ICHIDAN,
        COMPOUND_VERB_V2_NOT_AFTER_SURU,
        COMPOUND_VERB_V2_SURU_ONLY,
    )

    core, core_not_after_suru, core_suru_only = core_lexicon()
    oracle = {"Godan": set(COMPOUND_VERB_V2_GODAN), "Ichidan": set(COMPOUND_VERB_V2_ICHIDAN)}

    failed = False
    for kind, forms in core.items():
        missing = sorted(forms - oracle[kind] - READINGS_NOT_MIRRORED)
        if missing:
            print(f"❌ {CONSTANTS}: COMPOUND_VERB_V2_{kind.upper()} is missing {' '.join(missing)}")
            failed = True
        stale = sorted(oracle[kind] - forms - READINGS_NOT_MIRRORED)
        if stale:
            print(
                f"❌ {CONSTANTS}: COMPOUND_VERB_V2_{kind.upper()} retains "
                f"{' '.join(stale)}, which the core no longer joins"
            )
            failed = True

    for name, core_set, oracle_set in (
        ("COMPOUND_VERB_V2_NOT_AFTER_SURU", core_not_after_suru, set(COMPOUND_VERB_V2_NOT_AFTER_SURU)),
        ("COMPOUND_VERB_V2_SURU_ONLY", core_suru_only, set(COMPOUND_VERB_V2_SURU_ONLY)),
    ):
        if core_set - oracle_set:
            print(f"❌ {CONSTANTS}: {name} is missing {' '.join(sorted(core_set - oracle_set))}")
            failed = True
        if oracle_set - core_set:
            print(f"❌ {CONSTANTS}: {name} restricts {' '.join(sorted(oracle_set - core_set))}, the core does not")
            failed = True

    if failed:
        print()
        print("An expectation must not split a compound the tokenizer joins. Mirror the")
        print(f"{LEXICON} rows above into {CONSTANTS},")
        print("then re-sync expectations with test_needs_suzume_update(apply=True).")
        return 1

    print("✅ the oracle's compound V2 class mirrors the core lexicon")
    return 0


if __name__ == "__main__":
    sys.exit(main())
