"""Thread corpus scanning tools - MCP tool registration.

Defect records themselves live in the shared store: see ``defect_tools`` for
listing, rechecking, and closing what a scan files here.
"""

import contextlib
import re
from collections.abc import Callable
from pathlib import Path

from ..core import bug_store
from ..core.diff_utils import classify_surface_diff
from ..core.file_utils import atomic_write_text
from ..core.json_utils import json_error as _json_error
from ..core.json_utils import json_result as _json_result
from ..core.suzume_cli import get_expected_tokens_subprocess, get_suzume_surfaces
from ..server import PROJECT_ROOT, mcp

SKILL_DIR = PROJECT_ROOT / ".claude" / "skills" / "thread-quality-check"
PROGRESS_FILE = SKILL_DIR / ".thread_check_progress"
DEFAULT_FILE = SKILL_DIR / "thread_names.txt"

# Store that auto-detected scan issues are filed into.
SCAN_SOURCE = "thread"


# ============================================================================
# Diff classification
# ============================================================================


def classify_diff(expected: str, suzume: str) -> str:
    """Classify the type of difference between expected and suzume output.

    Returns one of:
        match        : Token surfaces are identical
        over-split   : Suzume splits a token that expected keeps together
        under-split  : Suzume merges tokens that expected splits
        boundary     : Same number of tokens but different boundaries
        minor        : Small difference (fullwidth/halfwidth, etc.)
        empty        : Either side has no tokens
    """
    return classify_surface_diff(expected.split(), suzume.split())


def summarize_diffs(issues: list[dict]) -> dict[str, int]:
    """Summarize diff types from a list of issues."""
    counts: dict[str, int] = {}
    for issue in issues:
        diff_type = issue.get("diff_type", "unknown")
        counts[diff_type] = counts.get(diff_type, 0) + 1
    return counts


# ============================================================================
# Progress tracking
# ============================================================================


def _load_progress(input_file: str) -> dict:
    """Load progress from file."""
    progress = {"file": input_file, "last_checked": 0, "problems_found": 0}
    if PROGRESS_FILE.exists():
        for line in PROGRESS_FILE.read_text(encoding="utf-8").splitlines():
            m = re.match(r"^(\w+)=(.*)$", line)
            if m:
                key, val = m.group(1), m.group(2)
                if key in ("last_checked", "problems_found"):
                    with contextlib.suppress(ValueError):
                        progress[key] = int(val)
                else:
                    progress[key] = val
        # Reset if file changed
        if progress.get("file", "") != input_file:
            progress = {"file": input_file, "last_checked": 0, "problems_found": 0}
    return progress


def _save_progress(progress: dict) -> None:
    """Save progress to file."""
    atomic_write_text(
        PROGRESS_FILE,
        f"file={progress['file']}\n"
        f"last_checked={progress['last_checked']}\n"
        f"problems_found={progress['problems_found']}\n",
    )


# ============================================================================
# Core comparison
# ============================================================================


def _compare_surfaces(text: str) -> dict:
    """Compare suzume CLI output vs expected surfaces."""
    # Thread checking runs WITH user.dic (skip_user_dict=False) so that entries
    # added via dict_add are reflected and defect_recheck can close the record.
    suzume_surfaces = get_suzume_surfaces(text, skip_user_dict=False)
    tokens, _, _ = get_expected_tokens_subprocess(text)
    expected_surfaces = [t["surface"] for t in tokens]

    suzume_joined = " ".join(suzume_surfaces)
    expected_joined = " ".join(expected_surfaces)

    result = {
        "match": suzume_joined == expected_joined,
        "suzume": suzume_joined,
        "expected": expected_joined,
    }

    if not result["match"]:
        result["diff_type"] = classify_diff(expected_joined, suzume_joined)

    return result


def _is_japanese(line: str) -> bool:
    """Check if line contains Japanese characters."""
    return bool(re.search(r"[\u3040-\u309F\u30A0-\u30FF\u4E00-\u9FFF]", line))


def _append_issue(
    line_num: int,
    text: str,
    result: dict,
    filed_texts: set[str] | None = None,
    next_record_id: list[int] | None = None,
) -> None:
    """File an auto-detected scan issue into the thread store."""
    if filed_texts is None:
        if bug_store.find_by_text(SCAN_SOURCE, text) is not None:
            return
    elif text in filed_texts:
        return
    bug_store.create(
        SCAN_SOURCE,
        text=text,
        expected=result["expected"],
        suzume=result["suzume"],
        diff_type=result.get("diff_type", ""),
        pattern="auto-scan",
        line_num=line_num,
        record_id=next_record_id[0] if next_record_id is not None else None,
    )
    if filed_texts is not None:
        filed_texts.add(text)
    if next_record_id is not None:
        next_record_id[0] += 1


def _process_lines(
    all_lines: list[str],
    start: int,
    limit: int,
    progress: dict,
    verbose: bool = False,
    record_issues: bool = True,
    filed_texts: set[str] | None = None,
    next_record_id: list[int] | None = None,
) -> tuple[list[dict], int, int, int, int]:
    """Process a range of lines and return results.

    Returns:
        (issues, processed, problems, skipped, max_line)
    """
    issues: list[dict] = []
    processed = 0
    problems = 0
    skipped = 0
    max_line = 0

    for current_line in range(start, len(all_lines) + 1):
        if processed >= limit:
            break
        line = all_lines[current_line - 1] if current_line <= len(all_lines) else ""

        if not line or len(line) < 2 or not _is_japanese(line):
            skipped += 1
            max_line = current_line
            processed += 1
            continue

        max_line = current_line
        processed += 1
        try:
            result = _compare_surfaces(line)
        except Exception as e:
            issues.append({"line_num": current_line, "text": line, "error": str(e), "diff_type": "error"})
            continue

        if result["match"]:
            if verbose:
                issues.append({"line_num": current_line, "text": line, "match": True})
        else:
            problems += 1
            progress["problems_found"] += 1
            issue = {
                "line_num": current_line,
                "text": line,
                "match": False,
                "expected": result["expected"],
                "suzume": result["suzume"],
                "diff_type": result.get("diff_type", "unknown"),
            }
            issues.append(issue)
            if record_issues:
                _append_issue(current_line, line, result, filed_texts, next_record_id)

    return issues, processed, problems, skipped, max_line


# ============================================================================
# MCP tools
# ============================================================================


def _input_path(input_file: str) -> Path:
    return Path(input_file) if input_file else DEFAULT_FILE


def _scan(
    count: int,
    from_line: int,
    input_file: str,
    dry_run: bool,
    *,
    verbose: bool,
    rows_key: str,
    row: Callable[[dict], dict | None],
) -> str:
    """Compare the next lines, file new issues unless dry-run, and shape the rows."""
    filepath = _input_path(input_file)
    if not filepath.exists():
        return _json_error(f"Input file not found: {filepath}")

    progress = _load_progress(str(filepath))
    if dry_run:
        progress = progress.copy()
    start = from_line if from_line > 0 else progress["last_checked"] + 1

    all_lines = filepath.read_text(encoding="utf-8").splitlines()
    filed_texts = None
    next_record_id = None
    if not dry_run:
        known = bug_store.load_open(SCAN_SOURCE) + bug_store.load_resolved(SCAN_SOURCE)
        filed_texts = {(record.get("text") or "").strip() for record in known}
        next_record_id = [bug_store.next_id(SCAN_SOURCE)]
    issues, processed, problems, skipped, max_line = _process_lines(
        all_lines,
        start,
        count,
        progress,
        verbose=verbose,
        record_issues=not dry_run,
        filed_texts=filed_texts,
        next_record_id=next_record_id,
    )

    if max_line > 0 and not dry_run:
        progress["last_checked"] = max_line
        _save_progress(progress)

    return _json_result(
        {
            "range": {"from": start, "to": max_line},
            "processed": processed,
            "problems": problems,
            "skipped": skipped,
            rows_key: [shaped for issue in issues if (shaped := row(issue)) is not None],
            "diff_types": summarize_diffs([i for i in issues if not i.get("match", True)]),
            "total_problems": progress["problems_found"],
            "dry_run": dry_run,
        }
    )


def _problem_row(issue: dict) -> dict | None:
    """A scan row: problems and errors only."""
    if issue.get("error"):
        return {
            "line_num": issue["line_num"],
            "text": issue["text"],
            "diff_type": "error",
            "expected": "",
            "suzume": "",
        }
    if not issue.get("match", True):
        return {
            "line_num": issue["line_num"],
            "text": issue["text"],
            "diff_type": issue["diff_type"],
            "expected": issue["expected"],
            "suzume": issue["suzume"],
        }
    return None


def _result_row(issue: dict) -> dict:
    """An interactive row: every checked line, including matches."""
    if issue.get("error"):
        return {
            "line_num": issue["line_num"],
            "text": issue["text"],
            "match": False,
            "diff_type": "error",
            "expected": "",
            "suzume": "",
        }
    if issue.get("match"):
        return {"line_num": issue["line_num"], "text": issue["text"], "match": True}
    return {
        "line_num": issue["line_num"],
        "text": issue["text"],
        "match": False,
        "diff_type": issue.get("diff_type", "unknown"),
        "expected": issue["expected"],
        "suzume": issue["suzume"],
    }


@mcp.tool()
async def thread_status(input_file: str = "") -> str:
    """Show thread check progress stats.

    Args:
        input_file: Path to thread names file. Defaults to the repository's
            thread-quality-check corpus.
    """
    filepath = _input_path(input_file)
    if not filepath.exists():
        return _json_error(f"Input file not found: {filepath}")

    progress = _load_progress(str(filepath))
    total = len(filepath.read_text(encoding="utf-8").splitlines())

    result = {
        "file": str(filepath),
        "total_lines": total,
        "last_checked": progress["last_checked"],
        "problems_found": progress["problems_found"],
    }

    if total > 0:
        result["progress_pct"] = round((progress["last_checked"] / total) * 100, 1)
        result["remaining"] = total - progress["last_checked"]

    return _json_result(result)


@mcp.tool()
async def thread_scan(
    count: int = 100,
    from_line: int = 0,
    input_file: str = "",
    dry_run: bool = False,
) -> str:
    """Batch scan thread names for surface differences (compact output).

    Args:
        count: Number of lines to process.
        from_line: Start from specific line number (0 = continue from last).
        input_file: Path to thread names file. Defaults to the repository's
            thread-quality-check corpus.
        dry_run: Inspect lines without writing issue JSON or updating progress.
    """
    return _scan(count, from_line, input_file, dry_run, verbose=False, rows_key="issues", row=_problem_row)


@mcp.tool()
async def thread_next(
    count: int = 20,
    from_line: int = 0,
    input_file: str = "",
    dry_run: bool = False,
) -> str:
    """Process next N lines interactively, showing problem details with diff classification.

    Args:
        count: Number of lines to process.
        from_line: Start from specific line number (0 = continue from last).
        input_file: Path to thread names file. Defaults to the repository's
            thread-quality-check corpus.
        dry_run: Inspect lines without writing issue JSON or updating progress.
    """
    return _scan(count, from_line, input_file, dry_run, verbose=True, rows_key="results", row=_result_row)


@mcp.tool()
async def thread_reset_progress() -> str:
    """Reset thread check progress to start from the beginning."""
    if PROGRESS_FILE.exists():
        PROGRESS_FILE.unlink()
    return _json_result({"status": "ok", "message": "Progress reset"})
