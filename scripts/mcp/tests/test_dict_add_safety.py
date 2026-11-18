"""Safety tests for single-entry dictionary mutation."""

import asyncio
import json
import re
from importlib import import_module
from pathlib import Path

import pytest

from suzume_mcp.tools.dict_tools import dict_add

common = import_module("suzume_mcp.tools._dict_tools_common")
operations = import_module("suzume_mcp.tools._dict_tools_operations")
VALID_CONJ = common.VALID_CONJ


def run(coro):
    return asyncio.run(coro)


def parse(result_str: str) -> dict:
    return json.loads(result_str)


class TestDictAdd:
    @pytest.fixture(autouse=True)
    def isolate_tool_dependencies(self, monkeypatch, tmp_path):
        monkeypatch.setattr(operations, "PROJECT_ROOT", tmp_path)
        monkeypatch.setattr(operations, "_load_all_entries", lambda: ([], {}))
        monkeypatch.setattr(
            operations,
            "mecab_analyze",
            lambda word: [{"surface": word, "pos": "名詞", "lemma": word, "conj_form": "基本形"}],
        )
        monkeypatch.setattr(operations, "get_suzume_rules_subprocess", lambda words: [""] * len(words))

    def test_conjugation_types_match_cpp_canonical_spellings(self, project_root):
        alias_source = (project_root / "src/dictionary/dictionary.cpp").read_text(encoding="utf-8")
        cpp_canonical = set(re.findall(r'ConjugationType::\w+, "([^"]*)"', alias_source))
        cpp_tsv_accepted = cpp_canonical - {"", "INTJ"}

        assert set(VALID_CONJ) == cpp_tsv_accepted
        assert "FAMILY" in VALID_CONJ
        assert "GIVEN" in VALID_CONJ
        assert "IRREGULAR" not in VALID_CONJ

    @pytest.mark.parametrize("conj_type", ["FAMILY", "GIVEN"])
    def test_proper_name_markers_are_accepted(self, conj_type):
        result = parse(run(dict_add("東京", "PROPER_NOUN", conj_type=conj_type, dry_run=True)))
        assert result["status"] == "ok"
        assert result["entry"] == f"東京\tPROPER_NOUN\t{conj_type}"

    def test_existing_surface_with_different_pos_is_accepted(self, monkeypatch):
        existing = {
            "最悪": [
                {
                    "surface": "最悪",
                    "pos": "NOUN",
                    "file": "data/core/nouns.tsv",
                }
            ]
        }
        monkeypatch.setattr(operations, "_load_all_entries", lambda: (existing["最悪"], existing))

        result = parse(run(dict_add("最悪", "ADJECTIVE", conj_type="NA_ADJ", dry_run=True)))

        assert result["status"] == "ok"
        assert result["entry"] == "最悪\tADJECTIVE\tNA_ADJ"

    def test_existing_surface_with_same_canonical_pos_is_rejected(self, monkeypatch):
        existing = {
            "東京": [
                {
                    "surface": "東京",
                    "pos": "PROPN",
                    "file": "data/core/proper_nouns.tsv",
                }
            ]
        }
        monkeypatch.setattr(operations, "_load_all_entries", lambda: (existing["東京"], existing))

        result = parse(run(dict_add("東京", "PROPER_NOUN", dry_run=True)))

        assert result["status"] == "error"
        assert "DUPLICATE" in result["message"]

    def test_failed_recompile_rolls_back_append(self, monkeypatch, tmp_path):
        target = tmp_path / "data/core/nouns.tsv"
        target.parent.mkdir(parents=True)
        original = "# nouns\n既存\tNOUN\n"
        target.write_text(original, encoding="utf-8")

        async def fail_recompile():
            return "failed"

        monkeypatch.setattr(operations, "_recompile_core_dic", fail_recompile)
        result = parse(run(dict_add("東京", "NOUN")))

        assert result["status"] == "error"
        assert result["recompile"] == "failed"
        assert result["rolled_back"] is True
        assert target.read_text(encoding="utf-8") == original

    def test_failed_compile_does_not_replace_live_binary(self, monkeypatch, tmp_path):
        data_dir = tmp_path / "data"
        core_dir = data_dir / "core"
        core_dir.mkdir(parents=True)
        (core_dir / "nouns.tsv").write_text("東京\tNOUN\n", encoding="utf-8")
        live_binary = data_dir / "core.dic"
        live_binary.write_bytes(b"known-good")
        fake_cli = tmp_path / "suzume-cli"
        fake_cli.touch()

        async def fail_compile(glob_pattern, output_path):
            assert glob_pattern == "data/core/*.tsv"
            assert output_path != str(live_binary)
            Path(output_path).write_bytes(b"partial-output")
            return False

        monkeypatch.setattr(common, "PROJECT_ROOT", tmp_path)
        monkeypatch.setattr(common, "ALL_DICT_FILES", ["data/core/nouns.tsv"])
        monkeypatch.setattr(common, "get_cli_path", lambda: fake_cli)
        monkeypatch.setattr(common, "recompile_dic", fail_compile)

        assert run(common._recompile_core_dic()) == "failed"
        assert live_binary.read_bytes() == b"known-good"
