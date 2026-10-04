"""Tests for thread check tools."""

import asyncio
import json
import shutil

import pytest

from suzume_mcp.core import bug_store
from suzume_mcp.core.diff_utils import normalize_width as _normalize_width
from suzume_mcp.tools.thread_tools import (
    _append_issue,
    _is_japanese,
    _load_progress,
    _process_lines,
    _save_progress,
    classify_diff,
    summarize_diffs,
    thread_next,
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
        assert classify_diff("東京駅 画像", "東京 駅 画像") == "over-split"

    def test_under_split(self):
        # Suzume has fewer tokens than expected
        assert classify_diff("東京 テスト 画像", "東京テスト 画像") == "under-split"

    def test_boundary(self):
        # Same count but different boundaries
        assert classify_diff("恒例二 次", "恒例 二次") == "boundary"

    def test_minor_fullwidth(self):
        # Only fullwidth/halfwidth difference
        assert classify_diff("２次", "2次") == "minor"

    def test_exact_match_not_called(self):
        assert classify_diff("食べ て いる", "食べ て いる") == "match"

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
        monkeypatch.setitem(bug_store._SOURCE_DIRS, "thread", bugs_dir)

        result = parse_json(run(thread_next(count=10, input_file=str(test_file))))
        assert result["processed"] == 3
        assert result["skipped"] == 1  # "hello" is ascii-only

    def test_dry_run_does_not_write_progress_or_bugs(self, tmp_path, monkeypatch):
        test_file = tmp_path / "thread_names.txt"
        test_file.write_text("テスト文\n", encoding="utf-8")
        progress_file = tmp_path / ".progress"
        original_progress = f"file={test_file}\nlast_checked=0\nproblems_found=7\n"
        progress_file.write_text(original_progress, encoding="utf-8")
        bugs_dir = tmp_path / "bugs"
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.PROGRESS_FILE", progress_file)
        monkeypatch.setitem(bug_store._SOURCE_DIRS, "thread", bugs_dir)
        monkeypatch.setattr(
            "suzume_mcp.tools.thread_tools._compare_surfaces",
            lambda text: {"match": False, "expected": "テスト 文", "suzume": "テスト文", "diff_type": "under-split"},
        )

        result = parse_json(run(thread_next(count=1, input_file=str(test_file), dry_run=True)))

        assert result["dry_run"] is True
        assert result["problems"] == 1
        assert progress_file.read_text(encoding="utf-8") == original_progress
        assert not bugs_dir.exists()


class TestThreadScan:
    def test_batch_scan_reuses_one_store_index_for_every_new_issue(self, tmp_path, monkeypatch):
        test_file = tmp_path / "thread_names.txt"
        test_file.write_text("テスト文一\nテスト文二\n", encoding="utf-8")
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.PROGRESS_FILE", tmp_path / ".progress")
        monkeypatch.setitem(bug_store._SOURCE_DIRS, "thread", tmp_path / "bugs")
        monkeypatch.setattr(
            "suzume_mcp.tools.thread_tools._compare_surfaces",
            lambda text: {"match": False, "expected": f"{text} A", "suzume": text, "diff_type": "under-split"},
        )
        monkeypatch.setattr(
            "suzume_mcp.tools.thread_tools.bug_store.find_by_text",
            lambda *_args, **_kwargs: pytest.fail("batch scan performed a per-issue store scan"),
        )

        result = parse_json(run(thread_scan(count=10, input_file=str(test_file))))

        assert result["problems"] == 2
        assert [record["id"] for record in bug_store.load_open("thread")] == [1, 2]

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
        monkeypatch.setitem(bug_store._SOURCE_DIRS, "thread", bugs_dir)

        result = parse_json(run(thread_scan(count=10, input_file=str(test_file))))
        assert result["processed"] == 2

    def test_dry_run_does_not_write_progress_or_bugs(self, tmp_path, monkeypatch):
        test_file = tmp_path / "thread_names.txt"
        test_file.write_text("テスト文\n", encoding="utf-8")
        progress_file = tmp_path / ".progress"
        original_progress = f"file={test_file}\nlast_checked=0\nproblems_found=7\n"
        progress_file.write_text(original_progress, encoding="utf-8")
        bugs_dir = tmp_path / "bugs"
        monkeypatch.setattr("suzume_mcp.tools.thread_tools.PROGRESS_FILE", progress_file)
        monkeypatch.setitem(bug_store._SOURCE_DIRS, "thread", bugs_dir)
        monkeypatch.setattr(
            "suzume_mcp.tools.thread_tools._compare_surfaces",
            lambda text: {"match": False, "expected": "テスト 文", "suzume": "テスト文", "diff_type": "under-split"},
        )

        result = parse_json(run(thread_scan(count=1, input_file=str(test_file), dry_run=True)))

        assert result["dry_run"] is True
        assert result["problems"] == 1
        assert progress_file.read_text(encoding="utf-8") == original_progress
        assert not bugs_dir.exists()


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
# Scan issues are filed into the shared store
# ============================================================================


class TestAppendIssue:
    def test_auto_scan_files_a_record(self, tmp_path, monkeypatch):
        bugs_dir = tmp_path / "bugs"
        monkeypatch.setitem(bug_store._SOURCE_DIRS, "thread", bugs_dir)

        _append_issue(42, "テスト文", {"expected": "テスト 文", "suzume": "テスト文", "diff_type": "under-split"})

        records = bug_store.load_open("thread")
        assert len(records) == 1
        assert records[0]["text"] == "テスト文"
        assert records[0]["diff_type"] == "under-split"
        assert records[0]["pattern"] == "auto-scan"
        assert records[0]["line_num"] == 42

    def test_auto_scan_numbers_records_sequentially(self, tmp_path, monkeypatch):
        bugs_dir = tmp_path / "bugs"
        monkeypatch.setitem(bug_store._SOURCE_DIRS, "thread", bugs_dir)

        _append_issue(1, "あ", {"expected": "a b", "suzume": "ab", "diff_type": "under-split"})
        _append_issue(2, "い", {"expected": "c d", "suzume": "cd", "diff_type": "over-split"})

        assert [rec["id"] for rec in bug_store.load_open("thread")] == [1, 2]

    def test_auto_scan_skips_a_text_already_filed(self, tmp_path, monkeypatch):
        bugs_dir = tmp_path / "bugs"
        monkeypatch.setitem(bug_store._SOURCE_DIRS, "thread", bugs_dir)
        issue = {"expected": "テスト 文", "suzume": "テスト文", "diff_type": "under-split"}

        _append_issue(1, "テスト文", issue)
        _append_issue(9, "テスト文", issue)

        assert len(bug_store.load_open("thread")) == 1
