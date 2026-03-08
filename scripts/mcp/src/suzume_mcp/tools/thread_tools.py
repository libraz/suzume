"""Thread check tools ported from thread_check.pl - MCP tool registration."""

import contextlib
import json
import re
import unicodedata
from pathlib import Path

from ..core.suzume_cli import get_suzume_surfaces
from ..core.suzume_utils import get_expected_tokens
from ..server import PROJECT_ROOT, mcp

SKILL_DIR = PROJECT_ROOT / ".claude" / "skills" / "thread-quality-check"
PROGRESS_FILE = SKILL_DIR / ".thread_check_progress"
ISSUES_FILE = SKILL_DIR / "thread_issues.txt"
BUGS_DIR = SKILL_DIR / "bugs"
DEFAULT_FILE = SKILL_DIR / "thread_names.txt"


def _json_result(obj: dict) -> str:
    """Serialize result dict to JSON string."""
    return json.dumps(obj, ensure_ascii=False, indent=2)


# ============================================================================
# Diff classification
# ============================================================================


def classify_diff(expected: str, suzume: str) -> str:
    """Classify the type of difference between expected and suzume output.

    Returns one of:
        over-split   : Suzume splits a token that expected keeps together
        under-split  : Suzume merges tokens that expected splits
        boundary     : Same number of tokens but different boundaries
        mecab-better : MeCab's split looks more reasonable
        suzume-better: Suzume's split looks more reasonable
        minor        : Small difference (fullwidth/halfwidth, etc.)
    """
    exp_tokens = expected.split()
    suz_tokens = suzume.split()

    if not exp_tokens or not suz_tokens:
        return "empty"

    # Check fullwidth/halfwidth normalization diff only
    exp_norm = _normalize_width(expected)
    suz_norm = _normalize_width(suzume)
    if exp_norm == suz_norm:
        return "minor"

    exp_len = len(exp_tokens)
    suz_len = len(suz_tokens)

    if suz_len > exp_len:
        return "over-split"
    elif suz_len < exp_len:
        return "under-split"
    else:
        return "boundary"


def _normalize_width(text: str) -> str:
    """Normalize fullwidth alphanumeric to halfwidth for comparison."""
    return unicodedata.normalize("NFKC", text)


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
    PROGRESS_FILE.write_text(
        f"file={progress['file']}\n"
        f"last_checked={progress['last_checked']}\n"
        f"problems_found={progress['problems_found']}\n",
        encoding="utf-8",
    )


# ============================================================================
# Core comparison
# ============================================================================


def _compare_surfaces(text: str) -> dict:
    """Compare suzume CLI output vs expected surfaces."""
    suzume_surfaces = get_suzume_surfaces(text)
    tokens, _, _ = get_expected_tokens(text)
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


def _append_issue(line_num: int, text: str, result: dict) -> None:
    """Append issue to issues file."""
    ISSUES_FILE.parent.mkdir(parents=True, exist_ok=True)
    diff_type = result.get("diff_type", "unknown")
    with open(ISSUES_FILE, "a", encoding="utf-8") as f:
        f.write(f"L{line_num}\t{text}\t{diff_type}\n")
        f.write(f"  expected: {result['expected']}\n")
        f.write(f"  suzume:   {result['suzume']}\n")


def _process_lines(
    all_lines: list[str],
    start: int,
    limit: int,
    progress: dict,
    verbose: bool = False,
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

        try:
            result = _compare_surfaces(line)
        except Exception as e:
            issues.append(
                {
                    "line_num": current_line,
                    "text": line,
                    "error": str(e),
                    "diff_type": "error",
                }
            )
            max_line = current_line
            processed += 1
            continue

        max_line = current_line
        processed += 1

        if result["match"]:
            if verbose:
                issues.append(
                    {
                        "line_num": current_line,
                        "text": line,
                        "match": True,
                    }
                )
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
            _append_issue(current_line, line, result)

    return issues, processed, problems, skipped, max_line


# ============================================================================
# MCP tools
# ============================================================================


@mcp.tool()
async def thread_status(input_file: str = "") -> str:
    """Show thread check progress stats.

    Args:
        input_file: Path to thread names file. Defaults to backup/thread_names.txt.
    """
    filepath = Path(input_file) if input_file else DEFAULT_FILE
    if not filepath.exists():
        return _json_result({"status": "error", "message": f"Input file not found: {filepath}"})

    progress = _load_progress(str(filepath))
    total = sum(1 for _ in filepath.read_text(encoding="utf-8").splitlines())

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
) -> str:
    """Batch scan thread names for surface differences (compact output).

    Args:
        count: Number of lines to process.
        from_line: Start from specific line number (0 = continue from last).
        input_file: Path to thread names file. Defaults to backup/thread_names.txt.
    """
    filepath = Path(input_file) if input_file else DEFAULT_FILE
    if not filepath.exists():
        return _json_result({"status": "error", "message": f"Input file not found: {filepath}"})

    progress = _load_progress(str(filepath))
    start = from_line if from_line > 0 else progress["last_checked"] + 1

    all_lines = filepath.read_text(encoding="utf-8").splitlines()
    issues, processed, problems, skipped, max_line = _process_lines(
        all_lines,
        start,
        count,
        progress,
        verbose=False,
    )

    # Build issue list for JSON (only problems)
    json_issues = []
    for issue in issues:
        if issue.get("error"):
            json_issues.append(
                {
                    "line_num": issue["line_num"],
                    "text": issue["text"],
                    "diff_type": "error",
                    "expected": "",
                    "suzume": "",
                }
            )
        elif not issue.get("match", True):
            json_issues.append(
                {
                    "line_num": issue["line_num"],
                    "text": issue["text"],
                    "diff_type": issue["diff_type"],
                    "expected": issue["expected"],
                    "suzume": issue["suzume"],
                }
            )

    if max_line > 0:
        progress["last_checked"] = max_line
        _save_progress(progress)

    type_counts = summarize_diffs([i for i in issues if not i.get("match", True)])

    result = {
        "range": {"from": start, "to": max_line},
        "processed": processed,
        "problems": problems,
        "skipped": skipped,
        "issues": json_issues,
        "diff_types": type_counts,
        "total_problems": progress["problems_found"],
    }

    return _json_result(result)


@mcp.tool()
async def thread_next(
    count: int = 20,
    from_line: int = 0,
    input_file: str = "",
) -> str:
    """Process next N lines interactively, showing problem details with diff classification.

    Args:
        count: Number of lines to process.
        from_line: Start from specific line number (0 = continue from last).
        input_file: Path to thread names file. Defaults to backup/thread_names.txt.
    """
    filepath = Path(input_file) if input_file else DEFAULT_FILE
    if not filepath.exists():
        return _json_result({"status": "error", "message": f"Input file not found: {filepath}"})

    progress = _load_progress(str(filepath))
    start = from_line if from_line > 0 else progress["last_checked"] + 1

    all_lines = filepath.read_text(encoding="utf-8").splitlines()
    issues, processed, problems, skipped, max_line = _process_lines(
        all_lines,
        start,
        count,
        progress,
        verbose=True,
    )

    # Build results list for JSON (all entries including matches)
    results = []
    for issue in issues:
        if issue.get("error"):
            results.append(
                {
                    "line_num": issue["line_num"],
                    "text": issue["text"],
                    "match": False,
                    "diff_type": "error",
                    "expected": "",
                    "suzume": "",
                }
            )
        elif issue.get("match"):
            results.append(
                {
                    "line_num": issue["line_num"],
                    "text": issue["text"],
                    "match": True,
                }
            )
        else:
            results.append(
                {
                    "line_num": issue["line_num"],
                    "text": issue["text"],
                    "match": False,
                    "diff_type": issue.get("diff_type", "unknown"),
                    "expected": issue["expected"],
                    "suzume": issue["suzume"],
                }
            )

    if max_line > 0:
        progress["last_checked"] = max_line
        _save_progress(progress)

    type_counts = summarize_diffs([i for i in issues if not i.get("match", True)])

    result = {
        "range": {"from": start, "to": max_line},
        "processed": processed,
        "problems": problems,
        "skipped": skipped,
        "results": results,
        "diff_types": type_counts,
        "total_problems": progress["problems_found"],
    }

    return _json_result(result)


@mcp.tool()
async def thread_issues_summary(limit: int = 50) -> str:
    """Summarize accumulated issues from thread_issues.txt with diff type breakdown.

    Args:
        limit: Max number of recent issues to show.
    """
    if not ISSUES_FILE.exists():
        return _json_result(
            {"status": "error", "message": "No issues file found. Run thread_scan or thread_next first."}
        )

    raw = ISSUES_FILE.read_text(encoding="utf-8")
    blocks: list[dict] = []
    current: dict | None = None

    for line in raw.splitlines():
        m = re.match(r"^L(\d+)\t(.+?)(?:\t(\S+))?$", line)
        if m:
            if current:
                blocks.append(current)
            current = {
                "line_num": int(m.group(1)),
                "text": m.group(2),
                "diff_type": m.group(3) or "unknown",
            }
        elif current and line.startswith("  expected: "):
            current["expected"] = line[len("  expected: ") :]
        elif current and line.startswith("  suzume:   "):
            current["suzume"] = line[len("  suzume:   ") :]
    if current:
        blocks.append(current)

    type_counts = summarize_diffs(blocks)
    total = len(blocks)

    recent_issues = []
    for issue in blocks[-limit:]:
        entry = {
            "line_num": issue["line_num"],
            "text": issue["text"],
            "diff_type": issue.get("diff_type", "unknown"),
        }
        if "expected" in issue:
            entry["expected"] = issue["expected"]
        if "suzume" in issue:
            entry["suzume"] = issue["suzume"]
        recent_issues.append(entry)

    result = {
        "total": total,
        "diff_types": type_counts,
        "recent_issues": recent_issues,
    }

    return _json_result(result)


@mcp.tool()
async def thread_reset_progress() -> str:
    """Reset thread check progress to start from the beginning."""
    if PROGRESS_FILE.exists():
        PROGRESS_FILE.unlink()
    return _json_result({"status": "ok", "message": "Progress reset"})


@mcp.tool()
async def thread_clear_issues() -> str:
    """Clear accumulated issues file."""
    if ISSUES_FILE.exists():
        ISSUES_FILE.unlink()
    return _json_result({"status": "ok", "message": "Issues file cleared"})


# ============================================================================
# Bug reporting tools (JSON per-case in bugs/ directory)
# ============================================================================


def _next_bug_id() -> int:
    """Get next sequential bug ID."""
    if not BUGS_DIR.exists():
        return 1
    existing = sorted(BUGS_DIR.glob("*.json"))
    if not existing:
        return 1
    # Extract numeric prefix from filename
    for f in reversed(existing):
        m = re.match(r"^(\d+)", f.stem)
        if m:
            return int(m.group(1)) + 1
    return 1


def _sanitize_filename(text: str, max_len: int = 30) -> str:
    """Create a safe filename fragment from text."""
    # Keep only safe chars
    safe = re.sub(r"[^\w\u3040-\u309F\u30A0-\u30FF\u4E00-\u9FFF]", "_", text)
    safe = re.sub(r"_+", "_", safe).strip("_")
    return safe[:max_len]


def _list_bugs() -> list[dict]:
    """Load all bug JSON files, sorted by ID."""
    if not BUGS_DIR.exists():
        return []
    bugs = []
    for f in sorted(BUGS_DIR.glob("*.json")):
        try:
            data = json.loads(f.read_text(encoding="utf-8"))
            data["_file"] = f.name
            bugs.append(data)
        except (json.JSONDecodeError, OSError):
            continue
    return bugs


@mcp.tool()
async def thread_report_bug(
    text: str,
    expected: str,
    suzume: str,
    description: str = "",
    line_num: int = 0,
    diff_type: str = "",
) -> str:
    """Report a grammar bug found during thread scanning.

    Creates a JSON file in bugs/ directory. Delete the file to mark as resolved.

    Args:
        text: Original input text.
        expected: Expected tokenization (space-separated).
        suzume: Suzume's actual tokenization (space-separated).
        description: Optional description of the bug.
        line_num: Optional line number in thread_names.txt.
        diff_type: Optional diff type (over-split, under-split, boundary).
    """
    BUGS_DIR.mkdir(parents=True, exist_ok=True)

    if not diff_type:
        diff_type = classify_diff(expected, suzume)

    bug_id = _next_bug_id()
    slug = _sanitize_filename(text)
    filename = f"{bug_id:03d}_{diff_type}_{slug}.json"

    data = {
        "id": bug_id,
        "text": text,
        "expected": expected,
        "suzume": suzume,
        "diff_type": diff_type,
    }
    if description:
        data["description"] = description
    if line_num > 0:
        data["line_num"] = line_num

    filepath = BUGS_DIR / filename
    filepath.write_text(
        json.dumps(data, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )

    count = len(list(BUGS_DIR.glob("*.json")))

    result = {
        "status": "ok",
        "bug_id": bug_id,
        "filename": filename,
        "diff_type": diff_type,
        "total_bugs": count,
    }

    return _json_result(result)


@mcp.tool()
async def thread_bugs_list(limit: int = 50) -> str:
    """List reported grammar bugs.

    Args:
        limit: Max number of bugs to show.
    """
    bugs = _list_bugs()
    if not bugs:
        return _json_result({"total": 0, "diff_types": {}, "bugs": []})

    total = len(bugs)

    # Summary by diff type
    type_counts: dict[str, int] = {}
    for b in bugs:
        dt = b.get("diff_type", "unknown")
        type_counts[dt] = type_counts.get(dt, 0) + 1

    shown = bugs[-limit:]
    bug_entries = []
    for bug in shown:
        entry = {
            "file": bug.get("_file", "?"),
            "diff_type": bug.get("diff_type", "unknown"),
            "text": bug["text"],
            "expected": bug.get("expected", "?"),
            "suzume": bug.get("suzume", "?"),
        }
        if "description" in bug:
            entry["description"] = bug["description"]
        bug_entries.append(entry)

    result = {
        "total": total,
        "diff_types": type_counts,
        "bugs": bug_entries,
    }

    return _json_result(result)


@mcp.tool()
async def thread_bugs_clear() -> str:
    """Clear all reported bugs (delete bugs/ directory)."""
    if BUGS_DIR.exists():
        import shutil

        shutil.rmtree(BUGS_DIR)
    return _json_result({"status": "ok", "message": "Bugs directory cleared"})


@mcp.tool()
async def thread_bugs_resolve(filename: str) -> str:
    """Mark a bug as resolved by deleting its JSON file.

    Args:
        filename: Bug filename (e.g. "001_over-split_テスト.json") or bug ID number.
    """
    # Allow passing just the ID number
    if filename.isdigit():
        bug_id = int(filename)
        matches = list(BUGS_DIR.glob(f"{bug_id:03d}_*.json"))
        if not matches:
            return _json_result({"status": "error", "message": f"No bug file found with ID {bug_id}"})
        filepath = matches[0]
    else:
        filepath = BUGS_DIR / filename

    if not filepath.exists():
        return _json_result({"status": "error", "message": f"Bug file not found: {filepath.name}"})

    resolved_name = filepath.name
    filepath.unlink()
    remaining = len(list(BUGS_DIR.glob("*.json"))) if BUGS_DIR.exists() else 0

    result = {
        "status": "ok",
        "resolved": resolved_name,
        "remaining": remaining,
    }

    return _json_result(result)
