"""Tests for test tools (MCP tool functions)."""

import asyncio
import importlib
import json

from suzume_mcp.tools import test_tools as _tt

mutation_tools = importlib.import_module("suzume_mcp.tools._test_tools_mutation")


def run(coro):
    return asyncio.run(coro)


def parse(result: str) -> dict:
    """Parse JSON result from tool function."""
    return json.loads(result)


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


class TestTestDelete:
    @staticmethod
    def write_suite(tmp_path):
        target = tmp_path / "tests/data/tokenization/sample.json"
        target.parent.mkdir(parents=True)
        target.write_text(
            json.dumps(
                {
                    "version": "1.0",
                    "cases": [
                        {
                            "id": "keep",
                            "input": "東京",
                            "expected": [{"surface": "東京", "pos": "Noun"}],
                        },
                        {
                            "id": "remove",
                            "input": "りんご",
                            "expected": [{"surface": "りんご", "pos": "Noun"}],
                        },
                    ],
                },
                ensure_ascii=False,
            ),
            encoding="utf-8",
        )
        return target

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
