"""Tests for thread check tools."""

import asyncio
import json
import shutil

import pytest

from suzume_mcp.tools.thread_tools import (
    _is_japanese,
    _list_bugs,
    _load_progress,
    _next_bug_id,
    _normalize_width,
    _process_lines,
    _save_progress,
    classify_diff,
    summarize_diffs,
    thread_bugs_clear,
    thread_bugs_list,
    thread_bugs_sweep,
    thread_next,
    thread_report_bug,
    thread_reset_progress,
    thread_scan,
    thread_status,
)

pytestmark = pytest.mark.skipif(
    shutil.which("mecab") is None,
    reason="MeCab not installed",
)


def run(coro):
    return asyncio.run(coro)


def parse_json(result_str: str) -> dict:
    """Parse JSON result from tool function."""
    return json.loads(result_str)


# ============================================================================
# classify_diff
# ============================================================================


class TestClassifyDiff:
    def test_over_split(self):
        # Suzume has more tokens than expected
        assert classify_diff("デカチンポ 画像", "デカ チンポ 画像") == "over-split"

    def test_under_split(self):
        # Suzume has fewer tokens than expected
        assert classify_diff("二次 エロ 画像", "二次エロ 画像") == "under-split"

    def test_boundary(self):
        # Same count but different boundaries
        assert classify_diff("恒例二 次", "恒例 二次") == "boundary"

    def test_minor_fullwidth(self):
        # Only fullwidth/halfwidth difference
        assert classify_diff("２次", "2次") == "minor"

    def test_exact_match_not_called(self):
        # classify_diff is only called when there IS a diff, but test edge case
        result = classify_diff("食べ て いる", "食べ て いる")
        # Same tokens → same count → boundary (but in practice won't be called)
        assert result in ("boundary", "minor")

    def test_empty(self):
        assert classify_diff("", "foo") == "empty"
        assert classify_diff("foo", "") == "empty"


class TestSummarizeDiffs:
    def test_summary(self):
        issues = [
            {"diff_type": "over-split"},
            {"diff_type": "over-split"},
            {"diff_type": "under-split"},
            {"diff_type": "boundary"},
        ]
        result = summarize_diffs(issues)
        assert result == {"over-split": 2, "under-split": 1, "boundary": 1}

    def test_empty(self):
        assert summarize_diffs([]) == {}

    def test_missing_key(self):
        issues = [{"foo": "bar"}, {"diff_type": "over-split"}]
        result = summarize_diffs(issues)
        assert result == {"unknown": 1, "over-split": 1}


# ============================================================================
# Helper functions
# ============================================================================


class TestIsJapanese:
    def test_hiragana(self):
        assert _is_japanese("たべる")

    def test_katakana(self):
        assert _is_japanese("カタカナ")

    def test_kanji(self):
        assert _is_japanese("漢字")

    def test_ascii_only(self):
        assert not _is_japanese("hello world")

    def test_mixed(self):
        assert _is_japanese("Hello世界")


class TestNormalizeWidth:
    def test_fullwidth_digits(self):
        assert _normalize_width("２次元") == "2次元"

    def test_fullwidth_alpha(self):
        assert _normalize_width("Ｈｅｌｌｏ") == "Hello"

    def test_already_halfwidth(self):
        assert _normalize_width("2次元") == "2次元"


# ============================================================================
# Progress tracking
# ============================================================================


class TestProgress:
    def test_save_and_load(self, tmp_path, monkeypatch):
        progress_file = tmp_path / ".progress"
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.PROGRESS_FILE", progress_file)

        progress = {"file": "/some/file.txt", "last_checked": 42, "problems_found": 5}
        _save_progress(progress)

        loaded = _load_progress("/some/file.txt")
        assert loaded["last_checked"] == 42
        assert loaded["problems_found"] == 5

    def test_reset_on_file_change(self, tmp_path, monkeypatch):
        progress_file = tmp_path / ".progress"
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.PROGRESS_FILE", progress_file)

        progress = {"file": "/old/file.txt", "last_checked": 100, "problems_found": 10}
        _save_progress(progress)

        loaded = _load_progress("/new/file.txt")
        assert loaded["last_checked"] == 0
        assert loaded["problems_found"] == 0

    def test_no_file(self, tmp_path, monkeypatch):
        progress_file = tmp_path / ".nonexistent"
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.PROGRESS_FILE", progress_file)

        loaded = _load_progress("/any/file.txt")
        assert loaded["last_checked"] == 0


# ============================================================================
# process_lines (unit test with mock data)
# ============================================================================


class TestProcessLines:
    def test_skip_empty_and_ascii(self):
        lines = ["", "hello", "テスト"]
        progress = {"problems_found": 0}
        issues, processed, problems, skipped, max_line = _process_lines(
            lines,
            1,
            10,
            progress,
            verbose=True,
        )
        assert processed == 3
        assert skipped == 2  # empty + ascii-only
        assert max_line == 3

    def test_limit(self):
        lines = ["テスト1", "テスト2", "テスト3", "テスト4"]
        progress = {"problems_found": 0}
        issues, processed, problems, skipped, max_line = _process_lines(
            lines,
            1,
            2,
            progress,
            verbose=False,
        )
        assert processed == 2
        assert max_line == 2

    def test_from_offset(self):
        lines = ["テスト1", "テスト2", "テスト3"]
        progress = {"problems_found": 0}
        issues, processed, problems, skipped, max_line = _process_lines(
            lines,
            3,
            10,
            progress,
            verbose=True,
        )
        assert processed == 1
        assert max_line == 3


# ============================================================================
# MCP tool integration tests
# ============================================================================


class TestThreadStatus:
    def test_file_not_found(self):
        result = parse_json(run(thread_status(input_file="/nonexistent/file.txt")))
        assert result["status"] == "error"
        assert "not found" in result["message"]

    def test_with_temp_file(self, tmp_path, monkeypatch):
        test_file = tmp_path / "thread_names.txt"
        test_file.write_text("テスト1\nテスト2\nテスト3\n", encoding="utf-8")
        progress_file = tmp_path / ".progress"
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.PROGRESS_FILE", progress_file)

        result = parse_json(run(thread_status(input_file=str(test_file))))
        assert result["total_lines"] == 3
        assert result["last_checked"] == 0


class TestThreadNext:
    def test_file_not_found(self):
        result = parse_json(run(thread_next(input_file="/nonexistent/file.txt")))
        assert result["status"] == "error"
        assert "not found" in result["message"]

    def test_small_batch(self, tmp_path, monkeypatch):
        test_file = tmp_path / "thread_names.txt"
        test_file.write_text("食べている\nhello\n漢字テスト\n", encoding="utf-8")
        progress_file = tmp_path / ".progress"
        bugs_dir = tmp_path / "bugs"
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.PROGRESS_FILE", progress_file)
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.BUGS_DIR", bugs_dir)

        result = parse_json(run(thread_next(count=10, input_file=str(test_file))))
        assert result["processed"] == 3
        assert result["skipped"] == 1  # "hello" is ascii-only


class TestThreadScan:
    def test_file_not_found(self):
        result = parse_json(run(thread_scan(input_file="/nonexistent/file.txt")))
        assert result["status"] == "error"
        assert "not found" in result["message"]

    def test_small_batch(self, tmp_path, monkeypatch):
        test_file = tmp_path / "thread_names.txt"
        test_file.write_text("食べている\n走っている\n", encoding="utf-8")
        progress_file = tmp_path / ".progress"
        bugs_dir = tmp_path / "bugs"
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.PROGRESS_FILE", progress_file)
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.BUGS_DIR", bugs_dir)

        result = parse_json(run(thread_scan(count=10, input_file=str(test_file))))
        assert result["processed"] == 2


class TestThreadResetProgress:
    def test_reset(self, tmp_path, monkeypatch):
        progress_file = tmp_path / ".progress"
        progress_file.write_text("last_checked=100\n", encoding="utf-8")
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.PROGRESS_FILE", progress_file)

        result = parse_json(run(thread_reset_progress()))
        assert result["status"] == "ok"
        assert "reset" in result["message"].lower()
        assert not progress_file.exists()


# ============================================================================
# Bug reporting tools
# ============================================================================


class TestNextBugId:
    def test_empty_dir(self, tmp_path, monkeypatch):
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.BUGS_DIR", tmp_path / "bugs")
        assert _next_bug_id() == 1

    def test_sequential(self, tmp_path, monkeypatch):
        bugs_dir = tmp_path / "bugs"
        bugs_dir.mkdir()
        (bugs_dir / "001_over-split.json").write_text("{}")
        (bugs_dir / "002_boundary.json").write_text("{}")
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.BUGS_DIR", bugs_dir)
        assert _next_bug_id() == 3


class TestThreadReportBug:
    def test_report_basic(self, tmp_path, monkeypatch):
        bugs_dir = tmp_path / "bugs"
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.BUGS_DIR", bugs_dir)

        result = parse_json(
            run(
                thread_report_bug(
                    text="おしっこしてる",
                    expected="おしっこ し てる",
                    suzume="お しっこし てる",
                    diff_type="boundary",
                )
            )
        )
        assert result["status"] == "ok"
        assert result["bug_id"] == 1
        assert result["total_bugs"] == 1

        files = list(bugs_dir.glob("*.json"))
        assert len(files) == 1
        data = json.loads(files[0].read_text(encoding="utf-8"))
        assert data["text"] == "おしっこしてる"
        assert data["diff_type"] == "boundary"

    def test_report_with_description(self, tmp_path, monkeypatch):
        bugs_dir = tmp_path / "bugs"
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.BUGS_DIR", bugs_dir)

        run(
            thread_report_bug(
                text="テスト文",
                expected="テスト 文",
                suzume="テスト文",
                description="missing candidate",
                line_num=42,
            )
        )

        files = list(bugs_dir.glob("*.json"))
        data = json.loads(files[0].read_text(encoding="utf-8"))
        assert data["description"] == "missing candidate"
        assert data["line_num"] == 42

    def test_report_auto_classify(self, tmp_path, monkeypatch):
        bugs_dir = tmp_path / "bugs"
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.BUGS_DIR", bugs_dir)

        run(
            thread_report_bug(
                text="テスト",
                expected="テスト 文",
                suzume="テスト文",
            )
        )
        files = list(bugs_dir.glob("*.json"))
        data = json.loads(files[0].read_text(encoding="utf-8"))
        assert data["diff_type"] == "under-split"

    def test_report_multiple(self, tmp_path, monkeypatch):
        bugs_dir = tmp_path / "bugs"
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.BUGS_DIR", bugs_dir)

        run(thread_report_bug(text="A", expected="a b", suzume="ab"))
        result = parse_json(run(thread_report_bug(text="B", expected="c d", suzume="cd")))
        assert result["total_bugs"] == 2
        assert len(list(bugs_dir.glob("*.json"))) == 2


class TestThreadBugsList:
    def test_no_dir(self, tmp_path, monkeypatch):
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.BUGS_DIR", tmp_path / "nope")
        result = parse_json(run(thread_bugs_list()))
        assert result["total"] == 0
        assert result["bugs"] == []

    def test_with_bugs(self, tmp_path, monkeypatch):
        bugs_dir = tmp_path / "bugs"
        bugs_dir.mkdir()
        (bugs_dir / "001_boundary.json").write_text(
            json.dumps(
                {
                    "id": 1,
                    "text": "おしっこしてる",
                    "expected": "おしっこ し てる",
                    "suzume": "お しっこし てる",
                    "diff_type": "boundary",
                    "description": "sokuon false positive",
                },
                ensure_ascii=False,
            )
        )
        (bugs_dir / "002_under-split.json").write_text(
            json.dumps(
                {
                    "id": 2,
                    "text": "テスト文",
                    "expected": "テスト 文",
                    "suzume": "テスト文",
                    "diff_type": "under-split",
                },
                ensure_ascii=False,
            )
        )
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.BUGS_DIR", bugs_dir)

        result = parse_json(run(thread_bugs_list()))
        assert result["total"] == 2
        assert "boundary" in result["diff_types"]
        assert "under-split" in result["diff_types"]
        # Check description is present in bug entries
        descriptions = [b.get("description", "") for b in result["bugs"]]
        assert "sokuon false positive" in descriptions


class TestThreadBugsClear:
    def test_clear(self, tmp_path, monkeypatch):
        bugs_dir = tmp_path / "bugs"
        bugs_dir.mkdir()
        (bugs_dir / "001_test.json").write_text("{}")
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.BUGS_DIR", bugs_dir)

        result = parse_json(run(thread_bugs_clear()))
        assert result["status"] == "ok"
        assert "cleared" in result["message"].lower()
        assert not bugs_dir.exists()

    def test_clear_nonexistent(self, tmp_path, monkeypatch):
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.BUGS_DIR", tmp_path / "nope")
        result = parse_json(run(thread_bugs_clear()))
        assert result["status"] == "ok"
        assert "cleared" in result["message"].lower()


class TestListBugs:
    def test_list(self, tmp_path, monkeypatch):
        bugs_dir = tmp_path / "bugs"
        bugs_dir.mkdir()
        (bugs_dir / "001_test.json").write_text(
            json.dumps(
                {
                    "id": 1,
                    "text": "a",
                    "diff_type": "over-split",
                }
            )
        )
        (bugs_dir / "002_test.json").write_text(
            json.dumps(
                {
                    "id": 2,
                    "text": "b",
                    "diff_type": "boundary",
                }
            )
        )
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.BUGS_DIR", bugs_dir)

        bugs = _list_bugs()
        assert len(bugs) == 2
        assert bugs[0]["id"] == 1
        assert bugs[1]["id"] == 2

    def test_empty(self, tmp_path, monkeypatch):
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.BUGS_DIR", tmp_path / "nope")
        assert _list_bugs() == []


class TestThreadReportBugPattern:
    def test_pattern_field(self, tmp_path, monkeypatch):
        bugs_dir = tmp_path / "bugs"
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.BUGS_DIR", bugs_dir)

        run(
            thread_report_bug(
                text="ぶっこわす",
                expected="ぶっこわす",
                suzume="ぶっ こわす",
                diff_type="over-split",
                pattern="sokuonbin",
            )
        )

        files = list(bugs_dir.glob("*.json"))
        assert len(files) == 1
        data = json.loads(files[0].read_text(encoding="utf-8"))
        assert data["pattern"] == "sokuonbin"

    def test_no_pattern(self, tmp_path, monkeypatch):
        bugs_dir = tmp_path / "bugs"
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.BUGS_DIR", bugs_dir)

        run(
            thread_report_bug(
                text="テスト",
                expected="テスト 文",
                suzume="テスト文",
            )
        )

        files = list(bugs_dir.glob("*.json"))
        data = json.loads(files[0].read_text(encoding="utf-8"))
        assert "pattern" not in data


class TestThreadBugsListFilter:
    def _setup_bugs(self, tmp_path, monkeypatch):
        bugs_dir = tmp_path / "bugs"
        bugs_dir.mkdir()
        for idx, (dt, pat) in enumerate(
            [
                ("over-split", "sokuonbin"),
                ("over-split", "compound-verb"),
                ("under-split", "sokuonbin"),
                ("boundary", ""),
            ],
            start=1,
        ):
            data = {"id": idx, "text": f"test{idx}", "diff_type": dt}
            if pat:
                data["pattern"] = pat
            (bugs_dir / f"{idx:03d}_{dt}_test{idx}.json").write_text(
                json.dumps(data, ensure_ascii=False)
            )
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.BUGS_DIR", bugs_dir)
        return bugs_dir

    def test_filter_by_diff_type(self, tmp_path, monkeypatch):
        self._setup_bugs(tmp_path, monkeypatch)
        result = parse_json(run(thread_bugs_list(diff_type="over-split")))
        assert result["filtered"] == 2
        assert all(b["diff_type"] == "over-split" for b in result["bugs"])
        # diff_types summary shows all types
        assert "under-split" in result["diff_types"]

    def test_filter_by_pattern(self, tmp_path, monkeypatch):
        self._setup_bugs(tmp_path, monkeypatch)
        result = parse_json(run(thread_bugs_list(pattern="sokuonbin")))
        assert result["filtered"] == 2
        assert all(b.get("pattern") == "sokuonbin" for b in result["bugs"])

    def test_filter_both(self, tmp_path, monkeypatch):
        self._setup_bugs(tmp_path, monkeypatch)
        result = parse_json(run(thread_bugs_list(diff_type="over-split", pattern="sokuonbin")))
        assert result["filtered"] == 1
        assert result["bugs"][0]["diff_type"] == "over-split"
        assert result["bugs"][0]["pattern"] == "sokuonbin"

    def test_no_filter(self, tmp_path, monkeypatch):
        self._setup_bugs(tmp_path, monkeypatch)
        result = parse_json(run(thread_bugs_list()))
        assert result["total"] == 4
        assert len(result["bugs"]) == 4


class TestAppendIssueToBugs:
    def test_auto_scan_creates_bug_json(self, tmp_path, monkeypatch):
        from suzume_mcp.tools.thread_tools import _append_issue

        bugs_dir = tmp_path / "bugs"
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.BUGS_DIR", bugs_dir)

        _append_issue(42, "テスト文", {"expected": "テスト 文", "suzume": "テスト文", "diff_type": "under-split"})

        files = list(bugs_dir.glob("*.json"))
        assert len(files) == 1
        data = json.loads(files[0].read_text(encoding="utf-8"))
        assert data["text"] == "テスト文"
        assert data["diff_type"] == "under-split"
        assert data["source"] == "auto-scan"
        assert data["line_num"] == 42

    def test_auto_scan_sequential_ids(self, tmp_path, monkeypatch):
        from suzume_mcp.tools.thread_tools import _append_issue

        bugs_dir = tmp_path / "bugs"
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.BUGS_DIR", bugs_dir)

        _append_issue(1, "a", {"expected": "a b", "suzume": "ab", "diff_type": "under-split"})
        _append_issue(2, "b", {"expected": "c d", "suzume": "cd", "diff_type": "over-split"})

        files = sorted(bugs_dir.glob("*.json"))
        assert len(files) == 2
        data1 = json.loads(files[0].read_text(encoding="utf-8"))
        data2 = json.loads(files[1].read_text(encoding="utf-8"))
        assert data1["id"] == 1
        assert data2["id"] == 2


class TestThreadBugsSweep:
    def _make_bug(self, bugs_dir, bug_id, text, diff_type="over-split"):
        data = {"id": bug_id, "text": text, "diff_type": diff_type,
                "expected": "x", "suzume": "y"}
        filepath = bugs_dir / f"{bug_id:03d}_{diff_type}_{text[:10]}.json"
        filepath.write_text(json.dumps(data, ensure_ascii=False), encoding="utf-8")
        return filepath

    def test_sweep_resolves_fixed(self, tmp_path, monkeypatch):
        bugs_dir = tmp_path / "bugs"
        bugs_dir.mkdir()
        f1 = self._make_bug(bugs_dir, 1, "テスト")
        f2 = self._make_bug(bugs_dir, 2, "残るバグ")
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.BUGS_DIR", bugs_dir)

        # Mock _compare_surfaces: first call matches, second doesn't
        call_count = [0]
        def mock_compare(text):
            call_count[0] += 1
            if text == "テスト":
                return {"match": True}
            return {"match": False, "diff_type": "over-split"}

        monkeypatch.setattr("suzume_mcp.tools.thread_tools._compare_surfaces", mock_compare)

        result = parse_json(run(thread_bugs_sweep()))
        assert result["resolved_count"] == 1
        assert result["remaining"] == 1
        assert not f1.exists()
        assert f2.exists()

    def test_sweep_empty(self, tmp_path, monkeypatch):
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.BUGS_DIR", tmp_path / "nope")
        result = parse_json(run(thread_bugs_sweep()))
        assert result["resolved_count"] == 0
        assert result["remaining"] == 0

    def test_sweep_all_resolved(self, tmp_path, monkeypatch):
        bugs_dir = tmp_path / "bugs"
        bugs_dir.mkdir()
        self._make_bug(bugs_dir, 1, "a")
        self._make_bug(bugs_dir, 2, "b")
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.BUGS_DIR", bugs_dir)
        monkeypatch.setattr("suzume_mcp.tools.thread_tools._compare_surfaces",
                            lambda text: {"match": True})

        result = parse_json(run(thread_bugs_sweep()))
        assert result["resolved_count"] == 2
        assert result["remaining"] == 0
        assert len(list(bugs_dir.glob("*.json"))) == 0
