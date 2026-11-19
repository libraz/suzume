"""Tests for test tools (MCP tool functions)."""

import importlib
import json

import pytest

from suzume_mcp.tools import test_tools as _tt
from tests.helpers import CORPUS_DIR, parse, run, write_suite

mutation_tools = importlib.import_module("suzume_mcp.tools._test_tools_mutation")
review_tools = importlib.import_module("suzume_mcp.tools._test_tools_review")


class TestTestShow:
    def test_default_mode(self):
        result = run(_tt.test_show("食べる"))
        data = parse(result)
        assert data["input"] == "食べる"
        assert isinstance(data["expected"], list)
        assert isinstance(data["match"], bool)
        assert "diff_type" in data
        assert "diff_details" in data
        assert "rule" in data

    def test_tsv_mode(self):
        result = run(_tt.test_show("食べる", mode="tsv"))
        # TSV mode still returns plain text
        lines = result.strip().split("\n")
        assert len(lines) >= 1
        assert "\t" in lines[0]

    def test_brief_mode(self):
        result = run(_tt.test_show("食べる", mode="brief"))
        data = parse(result)
        assert data["mode"] == "brief"
        assert "expected" in data

    def test_json_mode(self):
        result = run(_tt.test_show("食べる", mode="json"))
        data = json.loads(result)
        assert isinstance(data, list)
        assert all("surface" in t and "pos" in t for t in data)

    def test_core_dictionary_is_independent_of_caller_cwd(self, tmp_path, monkeypatch):
        monkeypatch.chdir(tmp_path)
        data = parse(run(_tt.test_show("教えてもらった")))
        assert "もらっ" in data["suzume"]
        assert "も" not in data["suzume"]


class TestTestList:
    def test_list(self):
        result = run(_tt.test_list())
        data = parse(result)
        assert "files" in data
        assert "total" in data
        assert isinstance(data["files"], list)
        assert data["total"] > 0


class TestTestSearch:
    def test_search_found(self):
        result = run(_tt.test_search("食べ"))
        data = parse(result)
        assert data["total"] > 0
        assert len(data["matches"]) > 0

    def test_search_not_found(self):
        result = run(_tt.test_search("zzzznonexistent"))
        data = parse(result)
        assert data["total"] == 0
        assert data["matches"] == []

    def test_search_with_limit(self):
        result = run(_tt.test_search("食べ", limit=2))
        data = parse(result)
        assert len(data["matches"]) <= 2


class TestTestFailed:
    def test_no_file(self):
        result = run(_tt.test_failed(test_output_file="/nonexistent/file.txt"))
        data = parse(result)
        assert data["total"] == 0
        assert data["failures"] == []

    def test_with_file(self, tmp_path):
        output_file = tmp_path / "test.txt"
        output_file.write_text(
            "Input: 食べています\n[  FAILED  ] Tokenize/Verb.Test (0 ms) GetParam() = verb_ichidan/tabeteiru\n",
            encoding="utf-8",
        )
        result = run(_tt.test_failed(test_output_file=str(output_file)))
        data = parse(result)
        assert data["total"] == 1
        assert data["failures"][0]["input"] == "食べています"


class TestTestCompare:
    def test_compare(self, tmp_path):
        before = tmp_path / "before.txt"
        after = tmp_path / "after.txt"
        before.write_text(
            "Input: 食べる\n[  FAILED  ] Tokenize/verb.Test (0 ms)\n"
            "Input: 走った\n[  FAILED  ] Tokenize/verb2.Test (0 ms)\n",
            encoding="utf-8",
        )
        after.write_text(
            "Input: 走った\n[  FAILED  ] Tokenize/verb2.Test (0 ms)\n",
            encoding="utf-8",
        )
        result = run(_tt.test_compare(str(before), str(after)))
        data = parse(result)
        assert data["before_failures"] == 2
        assert data["after_failures"] == 1


class TestTestListPos:
    def test_list_pos(self):
        result = run(_tt.test_list_pos())
        data = parse(result)
        assert "pos_values" in data
        pos_names = [p["pos"] for p in data["pos_values"]]
        assert "Noun" in pos_names or "Verb" in pos_names


class TestTestAcceptDiff:
    def test_refuses_regardless_of_arguments(self):
        for kwargs in (
            {"input_text": "食べる"},
            {"reason": "test"},
            {"input_text": "食べる", "reason": "test", "apply": True},
        ):
            data = parse(run(_tt.test_accept_diff(**kwargs)))
            assert data["status"] == "error"
            assert "disabled by design" in data["message"]

    def test_points_at_the_normalization_pipeline(self):
        data = parse(run(_tt.test_accept_diff(input_text="食べる", reason="test")))
        assert "suzume_mcp/core" in data["message"]
        assert "test_needs_suzume_update" in data["message"]


class TestUseSuzumeIsRefused:
    """use_suzume writes Suzume's output into `expected`, leaving no trace in the case."""

    def test_add_refuses(self):
        data = parse(run(_tt.test_add(input_text="テスト", use_suzume=True)))
        assert data["status"] == "error"
        assert "use_suzume is disabled by design" in data["message"]

    def test_update_refuses(self):
        data = parse(run(_tt.test_update(input_text="テスト", use_suzume=True)))
        assert data["status"] == "error"
        assert "use_suzume is disabled by design" in data["message"]

    def test_batch_add_refuses(self):
        data = parse(run(_tt.test_batch_add(file="", inputs=["テスト"], apply=True, use_suzume=True)))
        assert data["status"] == "error"
        assert "use_suzume is disabled by design" in data["message"]


PIN_NOUN = [
    {"surface": "がくしゃ", "pos": "Noun", "lemma": "がくしゃ"},
    {"surface": "だ", "pos": "Auxiliary", "lemma": "だ"},
]
PIN_VERB = [
    {"surface": "がくしゃ", "pos": "Verb", "lemma": "がくす"},
    {"surface": "だ", "pos": "Auxiliary", "lemma": "だ"},
]
PIN_POS_AND_LEMMA = [
    {"index": 0, "type": "pos", "surface": "がくしゃ", "expected": "Noun", "suzume": "Verb"},
    {"index": 0, "type": "lemma", "surface": "がくしゃ", "expected": "がくしゃ", "suzume": "がくす"},
]


class TestPinVerdict:
    """A registration states whether Suzume passes it on surface, POS and lemma."""

    @pytest.fixture
    def engine(self, tmp_path, monkeypatch):
        cli: dict[str, list[dict]] = {}

        async def fake_cli(text, cli_path=None, skip_user_dict=False):
            assert skip_user_dict, "pins are judged the way the C++ runner runs, without the user dictionary"
            if text not in cli:
                raise RuntimeError("Suzume CLI failed: boom")
            return cli[text]

        monkeypatch.setattr(mutation_tools, "PROJECT_ROOT", tmp_path)
        monkeypatch.setattr(mutation_tools, "get_expected_tokens", lambda _text: (PIN_NOUN, "reference", ""))
        monkeypatch.setattr(
            mutation_tools,
            "get_expected_tokens_batch_subprocess",
            lambda texts: [(PIN_NOUN, "reference", "") for _text in texts],
        )
        monkeypatch.setattr("suzume_mcp.core.suzume_cli.get_suzume_tokens_async", fake_cli)
        return cli

    def test_a_green_pin_says_it_passes(self, engine):
        engine["がくしゃだ"] = PIN_NOUN

        data = parse(run(_tt.test_add(input_text="がくしゃだ", file="noun_general")))

        assert data["status"] == "ok"
        assert data["passes"] is True
        assert "warning" not in data and "differences" not in data

    def test_a_red_pin_is_registered_but_flagged_with_the_differing_fields(self, engine, tmp_path):
        engine["がくしゃだ"] = PIN_VERB

        data = parse(run(_tt.test_add(input_text="がくしゃだ", file="noun_general")))

        assert data["status"] == "ok"
        assert data["passes"] is False
        assert data["differences"] == PIN_POS_AND_LEMMA
        assert "does not pass" in data["warning"]
        stored = json.loads((tmp_path / CORPUS_DIR / "noun_general.json").read_text(encoding="utf-8"))
        assert [case["input"] for case in stored["cases"]] == ["がくしゃだ"]

    def test_a_failed_check_is_reported_as_unknown_not_as_passing(self, engine):
        data = parse(run(_tt.test_add(input_text="がくしゃだ", file="noun_general")))

        assert data["status"] == "ok"
        assert data["passes"] is None
        assert "boom" in data["check_error"]
        assert "warning" in data

    @pytest.mark.parametrize("apply", [False, True], ids=["preview", "applied"])
    def test_batch_add_gives_a_verdict_per_case_and_lists_the_red_ones(self, engine, apply):
        engine["がくしゃだ"] = PIN_VERB
        engine["きしゃだ"] = PIN_NOUN

        data = parse(run(_tt.test_batch_add(file="noun_general", inputs=["がくしゃだ", "きしゃだ"], apply=apply)))

        by_input = {item["input"]: item for item in data["to_add"]}
        assert by_input["がくしゃだ"]["passes"] is False
        assert by_input["がくしゃだ"]["differences"] == PIN_POS_AND_LEMMA
        assert by_input["きしゃだ"]["passes"] is True
        assert "differences" not in by_input["きしゃだ"]
        assert data["red"] == ["がくしゃだ"]
        assert "1 of 2" in data["warning"]

    def test_batch_add_without_red_cases_carries_no_warning(self, engine):
        engine["きしゃだ"] = PIN_NOUN

        data = parse(run(_tt.test_batch_add(file="noun_general", inputs=["きしゃだ"], apply=False)))

        assert data["red"] == []
        assert "warning" not in data

    def test_update_reports_the_verdict_for_the_rewritten_expectation(self, engine, tmp_path):
        write_suite(tmp_path, "noun_general", [{"id": "gakusha_da", "input": "がくしゃだ", "expected": PIN_NOUN}])
        engine["がくしゃだ"] = PIN_VERB

        data = parse(run(_tt.test_update(test_id="noun_general/gakusha_da")))

        assert data["status"] == "ok"
        assert data["passes"] is False
        assert data["differences"] == PIN_POS_AND_LEMMA


class TestTestDelete:
    @staticmethod
    def write_suite(tmp_path):
        cases = [
            {"id": "keep", "input": "東京", "expected": [{"surface": "東京", "pos": "Noun"}]},
            {"id": "remove", "input": "りんご", "expected": [{"surface": "りんご", "pos": "Noun"}]},
        ]
        return write_suite(tmp_path, "sample", cases, header={"version": "1.0"}, compact=True)

    def test_delete_by_stable_id_applies_real_write(self, tmp_path, monkeypatch):
        target = self.write_suite(tmp_path)
        monkeypatch.setattr(mutation_tools, "PROJECT_ROOT", tmp_path)

        result = parse(run(_tt.test_delete(test_id="sample/remove")))
        assert result["status"] == "ok"
        assert result["id"] == "remove"
        remaining = json.loads(target.read_text(encoding="utf-8"))["cases"]
        assert [case["id"] for case in remaining] == ["keep"]

    def test_delete_reports_missing_selector_and_unknown_case(self, tmp_path, monkeypatch):
        self.write_suite(tmp_path)
        monkeypatch.setattr(mutation_tools, "PROJECT_ROOT", tmp_path)

        missing_selector = parse(run(_tt.test_delete()))
        assert missing_selector["status"] == "error"
        assert "required" in missing_selector["message"]
        missing_case = parse(run(_tt.test_delete(test_id="sample/missing")))
        assert missing_case["status"] == "error"
        assert "not found" in missing_case["message"].lower()


class TestCaseResolutionByInput:
    """An input shared by several cases never resolves to the first hit."""

    @pytest.fixture
    def corpus(self, tmp_path, monkeypatch):
        noun = [{"surface": "りんご", "pos": "Noun"}]
        write_suite(
            tmp_path,
            "first",
            [
                {"id": "shared", "input": "りんご", "expected": noun, "suzume_expected": noun},
                {"id": "solo", "input": "東京", "expected": [{"surface": "東京", "pos": "Noun"}]},
            ],
        )
        write_suite(tmp_path, "second", [{"id": "shared_too", "input": "りんご", "expected": noun}])
        for module in (mutation_tools, review_tools):
            monkeypatch.setattr(module, "PROJECT_ROOT", tmp_path)
        return tmp_path

    @staticmethod
    def snapshot(root):
        return {path.name: path.read_text(encoding="utf-8") for path in sorted(root.rglob("*.json"))}

    @pytest.mark.parametrize(
        "call",
        [
            lambda: _tt.test_update(input_text="りんご"),
            lambda: _tt.test_delete(input_text="りんご"),
            lambda: _tt.test_reset_suzume(input_text="りんご", apply=True),
        ],
        ids=["update", "delete", "reset_suzume"],
    )
    def test_an_ambiguous_input_is_an_error_naming_every_candidate(self, corpus, call):
        before = self.snapshot(corpus)

        result = parse(run(call()))

        assert result["status"] == "error"
        assert "first/shared" in result["message"] and "second/shared_too" in result["message"]
        assert self.snapshot(corpus) == before

    def test_a_unique_input_resolves_its_case(self, corpus):
        result = parse(run(_tt.test_delete(input_text="東京")))

        assert result["status"] == "ok"
        assert result["id"] == "solo"
        remaining = json.loads((corpus / CORPUS_DIR / "first.json").read_text(encoding="utf-8"))["cases"]
        assert [case["id"] for case in remaining] == ["shared"]

    def test_a_file_id_selects_one_of_the_duplicates(self, corpus):
        result = parse(run(_tt.test_delete(test_id="second/shared_too")))

        assert result["status"] == "ok"
        assert self.snapshot(corpus)["second.json"].count("りんご") == 0
        assert "りんご" in self.snapshot(corpus)["first.json"]

    def test_an_id_whose_input_differs_is_refused(self, corpus):
        result = parse(run(_tt.test_delete(input_text="りんご", test_id="first/solo")))

        assert result["status"] == "error"
        assert "does not match" in result["message"]

    def test_reports_list_every_location(self, corpus):
        coverage = parse(run(_tt.test_check_coverage(inputs=["りんご", "東京"])))
        duplicate = parse(run(_tt.test_add(input_text="りんご")))

        assert coverage["existing"] == [
            {"input": "りんご", "locations": ["first/shared", "second/shared_too"]},
            {"input": "東京", "locations": ["first/solo"]},
        ]
        assert duplicate["status"] == "error"
        assert duplicate["existing"] == ["first/shared", "second/shared_too"]


class TestTestResetSuzume:
    def test_missing_args(self):
        result = run(_tt.test_reset_suzume())
        data = parse(result)
        assert data["status"] == "error"
        assert "Either input_text or all_tests" in data["message"]

    def test_not_found(self):
        result = run(_tt.test_reset_suzume(input_text="zzzznonexistent"))
        data = parse(result)
        assert data["status"] == "error"
        assert "No test found" in data["message"]


class TestTestValidateIds:
    def test_validate(self):
        result = run(_tt.test_validate_ids())
        data = parse(result)
        assert "problems" in data
        assert "total" in data


class TestTestCheckCoverage:
    def test_coverage(self):
        result = run(_tt.test_check_coverage(inputs=["食べる", "zzzznonexistent"]))
        data = parse(result)
        assert "existing" in data
        assert "missing" in data
        assert "summary" in data
        assert data["summary"]["missing"] >= 1


class TestTestSuggestFile:
    def test_verb(self):
        result = run(_tt.test_suggest_file("食べている"))
        data = parse(result)
        assert "suggestions" in data
        assert len(data["suggestions"]) > 0

    def test_adjective(self):
        result = run(_tt.test_suggest_file("美しい"))
        data = parse(result)
        assert "suggestions" in data
        assert len(data["suggestions"]) > 0
