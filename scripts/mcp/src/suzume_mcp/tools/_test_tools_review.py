"""Test corpus review, validation, and coverage MCP tools."""

import re
from pathlib import Path

from ..core.suzume_cli import (
    get_expected_tokens_subprocess as get_expected_tokens,
)
from ..core.test_file_suggestions import suggest_test_files
from ..core.test_file_utils import (
    find_test_by_id,
    find_test_by_input,
    find_tests_by_input,
    generate_id,
    save_json,
)
from ..server import PROJECT_ROOT, mcp
from ._test_tools_common import (
    BANNED_ORACLE_KEYS,
    ORACLE_OVERRIDE_REMEDIATION,
    _case_id,
    _get_test_files_filtered,
    _json_error,
    _json_result,
    _load_test_cases,
)


def _banned_keys_in(case: dict) -> list[str]:
    return [key for key in BANNED_ORACLE_KEYS if case.get(key)]


def _case_location(found: dict) -> str:
    return f"{found['basename']}/{_case_id(found['case'], found['index'])}"


def _resolve_single_test(input_text: str, test_id: str) -> tuple[dict | None, str]:
    """Resolve a test case without silently choosing among duplicate inputs."""
    if test_id:
        found = find_test_by_id(PROJECT_ROOT, test_id)
        if not found:
            return None, f"No test found for id: {test_id}"
        if input_text and found["case"].get("input") != input_text:
            return None, f"Input does not match test id: {test_id}"
        return found, ""

    matches = find_tests_by_input(PROJECT_ROOT, input_text)
    if not matches:
        return None, f"No test found for input: {input_text}"
    if len(matches) > 1:
        locations = ", ".join(_case_location(found) for found in matches)
        return None, f"Input matches multiple tests; specify test_id: {locations}"
    return matches[0], ""


@mcp.tool()
async def test_accept_diff(
    input_text: str = "",
    reason: str = "",
    category: str = "pos-limitation",
    all_failed: bool = False,
    test_output_file: str = "/tmp/test.txt",
    apply: bool = False,
    test_id: str = "",
) -> str:
    """Disabled: refuses to promote Suzume's current output to a test expectation.

    This tool used to write a per-case oracle override. It is kept registered, and
    kept refusing, so that reaching for it returns the correct path instead of
    nothing. The arguments are accepted only to reach the refusal.

    A failing case means either the implementation is wrong (fix it) or the oracle
    is wrong (fix the normalization rule). There is no third option.
    """
    return _json_error(f"test_accept_diff is disabled by design. {ORACLE_OVERRIDE_REMEDIATION}")


@mcp.tool()
async def test_reset_suzume(
    input_text: str = "",
    all_tests: bool = False,
    file: str = "",
    apply: bool = False,
    test_id: str = "",
) -> str:
    """Strip banned oracle-override fields (suzume_expected / accepted_diff) from cases.

    Removal is unconditional: the fields are banned outright, so this never refuses
    on the grounds that Suzume still differs from the oracle. Re-sync expectations
    afterwards with test_needs_suzume_update(apply=True).

    Args:
        input_text: Input text to strip (mutually exclusive with all_tests).
        test_id: Stable basename/id selector. Required when input_text is duplicated.
        all_tests: If True, strip every case carrying a banned field.
        file: Optional test file filter (without .json), used with all_tests.
        apply: If True, apply changes. Default is dry-run.
    """
    if all_tests:
        loaded, error = _load_test_cases(_get_test_files_filtered(file or "all"))
        if error:
            return _json_error(error)
        to_reset = [
            {"file": path, "data": data, "case": case, "index": idx, "basename": path.stem}
            for path, data, cases in loaded
            for idx, case in enumerate(cases)
            if _banned_keys_in(case)
        ]
    elif input_text or test_id:
        found, error = _resolve_single_test(input_text, test_id)
        if not found:
            return _json_error(error)
        if not _banned_keys_in(found["case"]):
            return _json_error(f"Test has no oracle override to strip: {_case_location(found)}")
        to_reset = [found]
    else:
        return _json_error("Either input_text or all_tests=True, or test_id is required.")

    reset_entries = [
        {
            "id": _case_location(found),
            "file": found["basename"],
            "index": found["index"],
            "input": found["case"].get("input", ""),
            "removed": _banned_keys_in(found["case"]),
        }
        for found in to_reset
    ]

    if apply:
        files_to_save: dict[Path, dict] = {}
        for found in to_reset:
            for key in BANNED_ORACLE_KEYS:
                found["case"].pop(key, None)
            files_to_save[found["file"]] = found["data"]
        for path, data in files_to_save.items():
            save_json(path, data)

    return _json_result(
        {
            "status": "ok",
            "reset": reset_entries,
            "total": len(reset_entries),
            "applied": apply,
            "next_step": ("test_needs_suzume_update(apply=True) to re-sync expectations" if reset_entries else ""),
        }
    )


@mcp.tool()
async def test_validate_ids(
    file: str = "",
    apply: bool = False,
) -> str:
    """Detect and fix non-ASCII or duplicate test case IDs.

    Args:
        file: Optional test file filter (without .json), or empty for all.
        apply: If True, fix invalid IDs. Default is report-only.
    """
    files = _get_test_files_filtered(file or "all")
    if not files:
        return _json_error("No test files found")

    loaded, error = _load_test_cases(files)
    if error:
        return _json_error(error)

    problems = []
    for path, data, cases in loaded:
        file_ids: set[str] = set()

        for idx, case in enumerate(cases):
            case_id = case.get("id", "")
            inp = case.get("input", "")

            # Sanitize ID
            sanitized = re.sub(r"[^a-zA-Z0-9_]", "_", case_id)
            sanitized = re.sub(r"_+", "_", sanitized)
            sanitized = sanitized.strip("_")

            has_non_ascii = bool(re.search(r"[^\x00-\x7F]", case_id))
            becomes_empty = not sanitized or sanitized == "_"
            is_dup = sanitized in file_ids

            if has_non_ascii or becomes_empty or is_dup:
                new_id = generate_id(inp)
                suffix = 1
                base_id = new_id
                while new_id in file_ids:
                    new_id = f"{base_id}_{suffix}"
                    suffix += 1

                reason = (
                    "non-ASCII" if has_non_ascii else "empty after sanitize" if becomes_empty else "duplicate in file"
                )
                problems.append(
                    {
                        "path": path,
                        "data": data,
                        "case": case,
                        "out": {
                            "file": path.stem,
                            "index": idx,
                            "old_id": case_id,
                            "new_id": new_id,
                            "reason": reason,
                        },
                    }
                )
                file_ids.add(new_id)
            else:
                file_ids.add(sanitized)

    applied = apply and bool(problems)
    if applied:
        files_to_save: dict[Path, dict] = {}
        for prob in problems:
            prob["case"]["id"] = prob["out"]["new_id"]
            files_to_save[prob["path"]] = prob["data"]
        for path, data in files_to_save.items():
            save_json(path, data)

    return _json_result({"problems": [prob["out"] for prob in problems], "total": len(problems), "applied": applied})


@mcp.tool()
async def test_check_coverage(inputs: list[str]) -> str:
    """Check which inputs have existing tests.

    Args:
        inputs: List of Japanese input texts to check.
    """
    existing = []
    missing = []

    for inp in inputs:
        found = find_test_by_input(PROJECT_ROOT, inp)
        if found:
            existing.append({"input": inp, "location": f"{found['basename']}/{found['index']}"})
        else:
            missing.append(inp)

    return _json_result(
        {
            "existing": existing,
            "missing": missing,
            "summary": {"existing": len(existing), "missing": len(missing)},
        }
    )


@mcp.tool()
async def test_suggest_file(input_text: str) -> str:
    """Suggest which test file an input should go into based on MeCab analysis.

    Args:
        input_text: Japanese text to analyze.
    """
    tokens, _, _ = get_expected_tokens(input_text)
    if not tokens:
        return _json_error("No tokens found for input")

    return _json_result(
        {
            "input": input_text,
            "tokens": [{"surface": tok["surface"], "pos": tok["pos"]} for tok in tokens],
            "suggestions": suggest_test_files(input_text, tokens),
        }
    )
