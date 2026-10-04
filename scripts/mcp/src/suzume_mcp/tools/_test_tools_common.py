"""Test tools ported from test_tool.pl - MCP tool registration."""

import re
import subprocess
from pathlib import Path

from ..core.constants import TARI_ADVERB_STEMS
from ..core.json_utils import json_error as _json_error  # noqa: F401
from ..core.json_utils import json_result as _json_result  # noqa: F401
from ..core.pos_mapping import normalize_pos
from ..core.suzume_cli import get_cli_path
from ..core.test_file_utils import (
    find_test_by_id,
    find_test_by_input,
    get_cases,
    get_test_data_dir,
    get_test_files,
    load_json,
    normalize_test_file_name,
)
from ..server import PROJECT_ROOT

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
    cli = get_cli_path()
    if not cli.exists():
        raise RuntimeError(f"Suzume CLI not found: {cli}")

    result = subprocess.run(
        [str(cli), "analyze", "--no-user-dict", "--", text],
        capture_output=True,
        text=True,
        timeout=30,
        cwd=PROJECT_ROOT,
    )
    if result.returncode != 0:
        raise RuntimeError(f"Suzume CLI failed: {result.stderr.strip() or 'non-zero exit'}")

    tokens = []
    for line in result.stdout.split("\n"):
        if not line or line == "EOS":
            continue
        parts = line.split("\t")
        surface = parts[0]
        pos = normalize_pos(parts[1]) if len(parts) > 1 else "Other"
        lemma = parts[2] if len(parts) > 2 else surface
        # Tari adverb lemma normalization
        for stem in TARI_ADVERB_STEMS:
            if surface == f"{stem}と" and lemma == f"{stem}と":
                lemma = stem
                break
        tokens.append({"surface": surface, "pos": pos, "lemma": lemma})
    return tokens


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


def _case_id(case: dict, index: int) -> str:
    return case.get("id", str(index))


def _find_case(project_root: Path, input_text: str, test_id: str) -> tuple[dict | None, str]:
    """Find one case by id, else by input, returning an error message when absent."""
    if test_id:
        found = find_test_by_id(project_root, test_id)
        return found, "" if found else f"Test not found: {test_id}"
    if input_text:
        found = find_test_by_input(project_root, input_text)
        return found, "" if found else f"No test found for input: {input_text}"
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
