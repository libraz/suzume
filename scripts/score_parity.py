#!/usr/bin/env python3
"""Dump per-morpheme scores from a suzume-cli build, or compare two dumps exactly.

The WASM module and the Python wheel compile the debug features out and are
optimised, while the default native build keeps them; a score that depends on
either difference changes shipped rankings without failing any test. Two builds
of the same commit must therefore print identical scores for the same input.

The input is every distinct single-line `input` of the git-tracked
tests/data/tokenization/*.json files, sorted, fed to one `-f json` run, so two
dumps taken from the same checkout always analyse the same text.

Usage:
  scripts/score_parity.py dump <suzume-cli> <out.tsv>
  scripts/score_parity.py compare <a.tsv> <b.tsv>
"""

import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TEST_GLOB = "tests/data/tokenization/*.json"
SHOWN_DIFFERENCES = 20


def parity_inputs() -> list[str]:
    files = subprocess.run(
        ["git", "ls-files", TEST_GLOB], cwd=ROOT, capture_output=True, text=True, check=True
    ).stdout.split()
    texts = set()
    for name in files:
        for case in json.loads((ROOT / name).read_text(encoding="utf-8"))["cases"]:
            text = case["input"]
            if text and "\n" not in text:
                texts.add(text)
    return sorted(texts)


def dump(cli: str, out: str) -> None:
    texts = parity_inputs()
    result = subprocess.run(
        [cli, "-f", "json"], cwd=ROOT, input="\n".join(texts), capture_output=True, text=True, check=True
    )
    analysis = json.loads(result.stdout)
    normalized = analysis["normalized_text"]
    lines = []
    for morpheme in analysis["morphemes"]:
        line = normalized.count("\n", 0, morpheme["start"])
        fields = (
            line,
            json.dumps(texts[line], ensure_ascii=False),
            json.dumps(morpheme["surface"], ensure_ascii=False),
        )
        lines.append("{}\t{}\t{}\t{!r}\n".format(*fields, morpheme["score"]))
    Path(out).write_text("".join(lines), encoding="utf-8")
    print(f"{len(texts)} inputs, {len(lines)} morphemes -> {out}")


def compare(path_a: str, path_b: str) -> int:
    rows_a = Path(path_a).read_text(encoding="utf-8").split("\n")[:-1]
    rows_b = Path(path_b).read_text(encoding="utf-8").split("\n")[:-1]
    differing = [(row_a, row_b) for row_a, row_b in zip(rows_a, rows_b, strict=False) if row_a != row_b]
    if len(rows_a) == len(rows_b) and not differing:
        print(f"score parity: {len(rows_a)} morphemes identical")
        return 0
    print(f"score parity FAILED: {len(rows_a)} vs {len(rows_b)} morphemes, {len(differing)} rows differ")
    print(f"first differences (line, input, surface, score) — {path_a} | {path_b}:")
    for row_a, row_b in differing[:SHOWN_DIFFERENCES]:
        print(f"  {row_a}\n  {row_b}\n")
    return 1


def main(argv: list[str]) -> int:
    if len(argv) == 3 and argv[0] == "dump":
        dump(argv[1], argv[2])
        return 0
    if len(argv) == 3 and argv[0] == "compare":
        return compare(argv[1], argv[2])
    print(__doc__.split("Usage:")[1], file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
