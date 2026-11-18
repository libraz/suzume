"""scripts/score_parity.py: dump layout and exact comparison."""

from __future__ import annotations

import importlib.util
import json
import sys
from pathlib import Path
from types import ModuleType

import pytest


@pytest.fixture(scope="module")
def parity() -> ModuleType:
    path = Path(__file__).resolve().parents[2] / "score_parity.py"
    spec = importlib.util.spec_from_file_location("score_parity", path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _fake_cli(tmp_path: Path, morphemes: list[dict], normalized: str) -> Path:
    cli = tmp_path / "fake-cli"
    payload = json.dumps({"normalized_text": normalized, "morphemes": morphemes}, ensure_ascii=False)
    cli.write_text(f"#!{sys.executable}\nimport sys\nsys.stdin.read()\nprint({payload!r})\n", encoding="utf-8")
    cli.chmod(0o755)
    return cli


def test_dump_attributes_each_morpheme_to_its_input_line(parity, tmp_path, monkeypatch):
    monkeypatch.setattr(parity, "parity_inputs", lambda: ["東京", "りんご"])
    morphemes = [
        {"surface": "東京", "start": 0, "score": 1},
        {"surface": "りん", "start": 3, "score": 0.200000003},
        {"surface": "ご", "start": 5, "score": -0.5},
    ]
    out = tmp_path / "scores.tsv"

    parity.dump(str(_fake_cli(tmp_path, morphemes, "東京\nりんご")), str(out))

    assert out.read_text(encoding="utf-8").split("\n") == [
        '0\t"東京"\t"東京"\t1',
        '1\t"りんご"\t"りん"\t0.200000003',
        '1\t"りんご"\t"ご"\t-0.5',
        "",
    ]


def test_compare_accepts_identical_dumps(parity, tmp_path, capsys):
    dump = tmp_path / "a.tsv"
    dump.write_text('0\t"東京"\t"東京"\t1\n', encoding="utf-8")

    assert parity.compare(str(dump), str(dump)) == 0
    assert "1 morphemes identical" in capsys.readouterr().out


def test_compare_prints_the_first_differing_rows(parity, tmp_path, capsys):
    debug = tmp_path / "debug.tsv"
    nodebug = tmp_path / "nodebug.tsv"
    debug.write_text('0\t"東京"\t"東京"\t0.200000003\n', encoding="utf-8")
    nodebug.write_text('0\t"東京"\t"東京"\t0.200000018\n', encoding="utf-8")

    assert parity.compare(str(debug), str(nodebug)) == 1
    out = capsys.readouterr().out
    assert "1 rows differ" in out
    assert "0.200000003" in out and "0.200000018" in out


def test_compare_fails_when_one_dump_is_shorter(parity, tmp_path):
    longer = tmp_path / "a.tsv"
    shorter = tmp_path / "b.tsv"
    longer.write_text('0\t"a"\t"a"\t1\n1\t"b"\t"b"\t1\n', encoding="utf-8")
    shorter.write_text('0\t"a"\t"a"\t1\n', encoding="utf-8")

    assert parity.compare(str(longer), str(shorter)) == 1


def test_parity_inputs_are_sorted_distinct_single_lines(parity):
    texts = parity.parity_inputs()

    assert texts == sorted(set(texts))
    assert all(text and "\n" not in text for text in texts)
