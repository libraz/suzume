"""Shared helpers for the test corpus MCP tools: oracle access, case loading and drift detection."""

import re
import subprocess
from collections.abc import Iterable
from pathlib import Path

from ..config import PROJECT_ROOT
from ..core.json_utils import json_error as _json_error  # noqa: F401
from ..core.json_utils import json_result as _json_result  # noqa: F401
from ..core.suzume_cli import existing_cli, get_expected_tokens_batch_subprocess, tokens_from_output
from ..core.suzume_utils import tokens_match
from ..core.test_file_utils import (
    find_test_by_id,
    find_tests_by_input,
    get_cases,
    get_test_data_dir,
    get_test_files,
    load_json,
    normalize_test_file_name,
    save_json,
)

# Per-case oracle overrides. Banned: a case that carries its own expectation for
# Suzume silences that one case instead of generalizing the rule.
BANNED_ORACLE_KEYS = ("suzume_expected", "accepted_diff")

# Mirrored verbatim from kOracleOverrideRemediation in tests/common/test_case.h and
# scripts/check_oracle_overrides.py so the remediation reads the same in every layer.
ORACLE_OVERRIDE_REMEDIATION = (
    "A test case must not carry its own oracle. Encode the intentional MeCab difference as a "
    "normalization rule under scripts/mcp/src/suzume_mcp/core/ (merge_rules.py, split_rules.py, "
    "postprocessors.py, pos_mapping.py), then sync expectations with "
    "test_needs_suzume_update(apply=True) and drop the field with test_reset_suzume(apply=True). "
    "See AGENTS.md section 7 (Tokenization Design)."
)


def _get_suzume_tokens(text: str) -> list[dict]:
    """Get Suzume CLI tokens with POS and lemma.

    Uses --no-user-dict to match the C++ tokenization test runner oracle.
    """
    cli = existing_cli()

    result = subprocess.run(
        [str(cli), "analyze", "--no-user-dict", "--", text],
        capture_output=True,
        text=True,
        timeout=30,
        cwd=PROJECT_ROOT,
    )
    if result.returncode != 0:
        raise RuntimeError(f"Suzume CLI failed: {result.stderr.strip() or 'non-zero exit'}")

    return tokens_from_output(result.stdout)


def _format_expected_checked(tokens: list[dict], source: str) -> list[dict]:
    """Return the oracle tokens to store as `expected`, rejecting empty output before any write."""
    if not tokens:
        raise RuntimeError(f"{source} produced no tokens; refusing to write empty expected output")
    return tokens


def _get_test_files_filtered(file_filter: str = "") -> list[Path]:
    """Get test files, optionally filtered by name."""
    if file_filter and file_filter != "all":
        try:
            file_name = normalize_test_file_name(file_filter)
        except ValueError:
            # Callers report an empty result as a rejected filter. Crucially,
            # never turn an invalid filter into a path outside the corpus.
            return []
        path = get_test_data_dir(PROJECT_ROOT) / f"{file_name}.json"
        return [path] if path.exists() else []
    return get_test_files(PROJECT_ROOT)


def _load_test_cases(files: list[Path]) -> tuple[list[tuple[Path, dict, list[dict]]], str | None]:
    """Load each file's case array, or report the first file that does not parse."""
    loaded = []
    for path in files:
        try:
            data = load_json(path)
        except Exception as exc:
            return [], f"Failed to parse JSON file {path}: {exc}"
        loaded.append((path, data, get_cases(data, str(path))))
    return loaded, None


def _load_selected_cases(
    files: list[Path],
) -> tuple[list[tuple[Path, dict, list[dict]]], str | None]:
    """Load the cases of an already filtered file list, reporting an empty list as an error."""
    if not files:
        return [], "No test files found"
    return _load_test_cases(files)


def _save_changed(changed: Iterable[tuple[Path, dict]]) -> None:
    """Write each changed file once, in first-seen order."""
    for path, data in dict(changed).items():
        save_json(path, data)


def _case_id(case: dict, index: int) -> str:
    return case.get("id", str(index))


def _case_location(found: dict) -> str:
    """The file/id selector that names one case."""
    return f"{found['basename']}/{_case_id(found['case'], found['index'])}"


def _case_locations(project_root: Path, input_text: str) -> list[str]:
    """The file/id selector of every case whose input is ``input_text``."""
    return [_case_location(found) for found in find_tests_by_input(project_root, input_text)]


def _find_case(project_root: Path, input_text: str, test_id: str) -> tuple[dict | None, str]:
    """Resolve exactly one case by file/id, else by input, or return an error message.

    An input shared by several cases is an error naming each of them, never the first hit.
    """
    if test_id:
        found = find_test_by_id(project_root, test_id)
        if not found:
            return None, f"Test not found: {test_id}"
        if input_text and found["case"].get("input") != input_text:
            return None, f"Input does not match test id: {test_id}"
        return found, ""
    if input_text:
        matches = find_tests_by_input(project_root, input_text)
        if not matches:
            return None, f"No test found for input: {input_text}"
        if len(matches) > 1:
            locations = ", ".join(_case_location(found) for found in matches)
            return None, f"Input matches multiple tests; pass test_id as one of: {locations}"
        return matches[0], ""
    return None, "Either input_text or test_id is required."


def _detect_segmentation_pattern(correct_str: str, suzume_str: str, input_text: str) -> str:
    """Detect segmentation failure pattern."""
    correct = correct_str.split("|")
    suzume = suzume_str.split("|")

    pos = 0
    correct_spans = []
    for tok in correct:
        length = len(tok)
        correct_spans.append({"token": tok, "start": pos, "end": pos + length})
        pos += length

    pos = 0
    for suz_tok in suzume:
        suz_len = len(suz_tok)
        suz_end = pos + suz_len
        covered = [s for s in correct_spans if s["start"] >= pos and s["end"] <= suz_end]
        if len(covered) > 1:
            merged = "".join(s["token"] for s in covered)
            if re.search(r"く.?ない$", merged):
                return "くない未分割"
            if re.search(r".?ん$", merged) and len(covered) >= 2:
                return "ん未分割"
            if any(s["token"] == "て" for s in covered):
                return "て形未分割"
            if re.search(r"て.?(?:い|いる|いた)$", merged):
                return "ている未分割"
            if any(s["token"] == "たい" for s in covered):
                return "たい未分割"
            if any(re.match(r"^ま[すせし]", s["token"]) for s in covered):
                return "ます未分割"
            if any(re.match(r"^[たっだ]$", s["token"]) for s in covered):
                return "た/だ未分割"
            if any(s["token"] in ("ない", "なかっ") for s in covered):
                return "ない未分割"
            if any(re.match(r"^[らりれろ]れ", s["token"]) or re.match(r"^れ[るた]", s["token"]) for s in covered):
                return "れる/られる未分割"
            if any(re.match(r"^さ?せ", s["token"]) for s in covered):
                return "せる/させる未分割"
            pattern = "+".join(s["token"] for s in covered)
            return f"未分割({pattern})"
        pos = suz_end

    if len(suzume) > len(correct):
        return "過分割"
    return "その他"


def _surfaces(tokens: list[dict]) -> str:
    return "|".join(t.get("surface", "") for t in tokens)


def _cases_with_input(loaded: list[tuple[Path, dict, list[dict]]]) -> list[dict]:
    """Every case that carries an input, with its qualified basename/id."""
    return [
        {"path": path, "data": data, "case": case, "input": case["input"], "id": f"{path.stem}/{_case_id(case, idx)}"}
        for path, data, cases in loaded
        for idx, case in enumerate(cases)
        if case.get("input", "")
    ]


def oracle_drift(file: str = "", test_ids: list[str] | None = None) -> tuple[dict | None, str | None]:
    """Compare stored expectations with a full oracle recompute of every selected case.

    Returns ({"entries", "by_rule", "errors"}, None), or (None, message) when the
    selection is invalid. Each entry carries the case, its fresh tokens and the
    reportable "out" row; nothing is written.
    """
    selected_ids = set(test_ids or [])
    if file and selected_ids:
        return None, "file and test_ids cannot be combined"

    loaded, error = _load_selected_cases(_get_test_files_filtered(file))
    if error:
        return None, error

    metas = [meta for meta in _cases_with_input(loaded) if not selected_ids or meta["id"] in selected_ids]
    if selected_ids:
        missing_ids = sorted(selected_ids - {meta["id"] for meta in metas})
        if missing_ids:
            return None, f"No tests found for ids: {', '.join(missing_ids)}"

    # One full oracle recompute over every selected input.
    batch_results = get_expected_tokens_batch_subprocess([meta["input"] for meta in metas]) if metas else []

    needs_update = []
    by_rule: dict[str, list[str]] = {}
    normalization_errors: list[dict] = []
    for meta, (correct, source, rule) in zip(metas, batch_results, strict=True):
        if source == "error":
            normalization_errors.append({"id": meta["id"], "input": meta["input"], "error": rule})
            continue
        expected = meta["case"].get("expected") or []
        if tokens_match(expected, correct):
            continue

        exp_str = _surfaces(expected)
        cor_str = _surfaces(correct)
        exp_pos = "|".join(t.get("pos", "") for t in expected)
        cor_pos = "|".join(t["pos"] for t in correct)
        rule_label = rule or "mecab-only"
        needs_update.append(
            {
                "meta": meta,
                "rule": rule_label,
                "correct_tokens": correct,
                "out": {
                    "id": meta["id"],
                    "input": meta["input"],
                    "rule": rule_label,
                    "diff_type": "surface" if exp_str != cor_str else ("pos" if exp_pos != cor_pos else "lemma"),
                    "expected": exp_str,
                    "correct": cor_str,
                },
            }
        )
        by_rule.setdefault(rule_label, []).append(meta["id"])

    return {"entries": needs_update, "by_rule": dict(sorted(by_rule.items())), "errors": normalization_errors}, None
