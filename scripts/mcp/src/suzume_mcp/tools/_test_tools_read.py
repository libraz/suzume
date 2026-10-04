"""Read-only test inspection and comparison MCP tools."""

import re
from itertools import zip_longest
from pathlib import Path

from ..core.diff_utils import classify_surface_diff
from ..core.pos_mapping import normalize_pos
from ..core.suzume_cli import (
    get_expected_tokens_batch_subprocess,
    get_mecab_tokens_batch_subprocess,
    get_suzume_debug_info,
)
from ..core.suzume_cli import (
    get_expected_tokens_subprocess as get_expected_tokens,
)
from ..core.suzume_utils import tokens_match
from ..core.test_file_utils import (
    find_test_by_id,
    find_test_by_input,
    get_cases,
    get_failures_from_test_output,
    get_test_data_dir,
    get_test_files,
    load_json,
    save_json,
)
from ..server import PROJECT_ROOT, mcp
from ._test_tools_common import (
    _case_id,
    _detect_segmentation_pattern,
    _format_expected_checked,
    _get_suzume_tokens,
    _get_test_files_filtered,
    _json_error,
    _json_result,
    _load_test_cases,
)


def _surfaces(tokens: list[dict]) -> str:
    return "|".join(t.get("surface", "") for t in tokens)


@mcp.tool()
async def test_show(
    input_text: str,
    mode: str = "default",
) -> str:
    """Compare MeCab expected vs Suzume output for a given input.

    Args:
        input_text: Japanese text to analyze.
        mode: Output mode - "default", "brief", "tsv", "debug", or "json".

    Returns:
        Comparison result showing expected tokens, Suzume tokens, and diff classification.
    """
    expected_tokens, source, rule = get_expected_tokens(input_text)
    expected_surfaces = [t["surface"] for t in expected_tokens]

    suzume_tokens = _get_suzume_tokens(input_text)
    suzume_surfaces = [t["surface"] for t in suzume_tokens]

    found = find_test_by_input(PROJECT_ROOT, input_text)

    # Diff classification
    surface_match = "|".join(expected_surfaces) == "|".join(suzume_surfaces)
    full_match = surface_match and tokens_match(expected_tokens, suzume_tokens)
    diff_type = classify_surface_diff(expected_surfaces, suzume_surfaces)
    diff_details = []

    if not surface_match:
        for idx, (exp, suz) in enumerate(zip_longest(expected_surfaces, suzume_surfaces, fillvalue="")):
            if exp != suz:
                diff_details.append({"index": idx, "expected": exp, "suzume": suz, "type": "surface"})
    elif not full_match:
        diff_type = "pos-lemma"
        for idx, (exp, suz) in enumerate(zip(expected_tokens, suzume_tokens, strict=True)):
            e_pos = normalize_pos(exp.get("pos", ""))
            s_pos = normalize_pos(suz.get("pos", ""))
            e_lemma = exp.get("lemma", exp["surface"])
            s_lemma = suz.get("lemma", suz["surface"])
            if e_pos != s_pos:
                diff_details.append(
                    {"index": idx, "surface": exp["surface"], "expected": e_pos, "suzume": s_pos, "type": "pos"}
                )
            if e_lemma != s_lemma:
                diff_details.append(
                    {"index": idx, "surface": exp["surface"], "expected": e_lemma, "suzume": s_lemma, "type": "lemma"}
                )

    # TSV mode - keep plain text for copy-paste
    if mode == "tsv":
        lines = []
        for tok in expected_tokens:
            line = f"{tok['surface']}\t{tok['pos']}"
            if tok.get("lemma") and tok["lemma"] != tok["surface"]:
                line += f"\t{tok['lemma']}"
            lines.append(line)
        return "\n".join(lines)

    if mode == "json":
        return _json_result(expected_tokens)

    result = {
        "input": input_text,
        "expected": expected_surfaces,
        "suzume": suzume_surfaces or None,
        "match": full_match,
        "diff_type": diff_type,
        "diff_details": diff_details,
        "rule": rule,
        "test_exists": {"file": found["basename"], "id": _case_id(found["case"], found["index"])} if found else None,
    }

    if mode == "brief":
        result["mode"] = "brief"

    # Debug info
    if mode == "debug":
        info = await get_suzume_debug_info(input_text)
        if info.get("best_path"):
            result["scoring"] = {
                "total_cost": info["total_cost"],
                "margin": info["margin"],
                "best_path": info["best_path"],
            }

    return _json_result(result)


@mcp.tool()
async def test_list() -> str:
    """List all test files with case counts."""
    test_dir = get_test_data_dir(PROJECT_ROOT)
    if not test_dir.exists():
        return _json_error("Test data directory not found")

    files = []
    total = 0
    for path in sorted(test_dir.glob("*.json")):
        try:
            count = len(get_cases(load_json(path), str(path)))
        except Exception:
            count = 0
        total += count
        files.append({"name": path.stem, "count": count})

    return _json_result({"files": files, "total": total})


@mcp.tool()
async def test_search(pattern: str, limit: int = 0) -> str:
    """Search test cases by regex pattern (matches input, surfaces, and ID).

    Args:
        pattern: Regex pattern to search for.
        limit: Max results to show (0 = unlimited).
    """
    try:
        regex = re.compile(pattern, re.IGNORECASE)
    except re.error as exc:
        return _json_error(f"Invalid regex: {exc}")

    loaded, error = _load_test_cases(get_test_files(PROJECT_ROOT))
    if error:
        return _json_error(error)

    matches = []
    for path, _data, cases in loaded:
        for idx, case in enumerate(cases):
            case_id = _case_id(case, idx)
            inp = case.get("input", "")
            surfaces = " ".join(t.get("surface", "") for t in (case.get("expected") or []))
            if regex.search(inp) or regex.search(surfaces) or regex.search(str(case_id)):
                matches.append({"file": path.stem, "index": idx, "id": case_id, "input": inp, "expected": surfaces})

    return _json_result(
        {"pattern": pattern, "matches": matches[:limit] if limit > 0 else matches, "total": len(matches)}
    )


@mcp.tool()
async def test_failed(
    test_output_file: str = "/tmp/test.txt",
    limit: int = 0,
    verbose: bool = False,
    grep: str = "",
) -> str:
    """List failed test inputs from test output file.

    Args:
        test_output_file: Path to ctest output file.
        limit: Max results (0 = unlimited).
        verbose: Show test IDs alongside inputs.
        grep: Filter pattern for inputs/IDs.
    """
    failures = get_failures_from_test_output(test_output_file)
    if not failures:
        return _json_result({"source": test_output_file, "failures": [], "total": 0})

    if grep:
        try:
            rxp = re.compile(grep)
        except re.error:
            return _json_error(f"Invalid grep pattern: {grep}")
        failures = [f for f in failures if rxp.search(f["input"]) or rxp.search(f["id"])]

    shown = failures[:limit] if limit > 0 else failures
    return _json_result(
        {
            "source": test_output_file,
            "failures": [{"input": f["input"], "id": f["id"]} for f in shown],
            "total": len(failures),
        }
    )


@mcp.tool()
async def test_compare(before_file: str, after_file: str) -> str:
    """Compare two test outputs to show improved/regressed cases.

    Args:
        before_file: Path to before test output.
        after_file: Path to after test output.
    """

    def extract_failures(filepath: str) -> dict[str, str]:
        failures = {}
        inp = ""
        for line in Path(filepath).read_text(encoding="utf-8").splitlines():
            m_inp = re.search(r"Input:\s*(.+)", line)
            if m_inp:
                inp = m_inp.group(1)
            m_fail = re.search(r"FAILED.*Tokenize/([^,]+)", line)
            if m_fail and inp:
                failures[m_fail.group(1)] = inp
                inp = ""
        return failures

    before = extract_failures(before_file)
    after = extract_failures(after_file)

    return _json_result(
        {
            "before_failures": len(before),
            "after_failures": len(after),
            "improved": [{"id": key, "input": before[key]} for key in sorted(before.keys() - after.keys())],
            "regressed": [{"id": key, "input": after[key]} for key in sorted(after.keys() - before.keys())],
            "net_change": len(after) - len(before),
        }
    )


@mcp.tool()
async def test_diff_suzume(
    limit: int = 10,
    test_output_file: str = "/tmp/test.txt",
) -> str:
    """Analyze test failures by category (segmentation, POS-only, matches-correct).

    Args:
        limit: Max items per category to show (0 = all). Also limits processing to limit*5 failures.
        test_output_file: Path to ctest output file.
    """
    failures = get_failures_from_test_output(test_output_file)
    max_process = limit * 5 if limit > 0 else 0

    def capped(items):
        return items[:limit] if limit > 0 else items

    categories: dict[str, list[dict]] = {"matches_correct": [], "segmentation": [], "pos_only": []}
    processed = 0

    for failure in failures:
        if max_process and processed >= max_process:
            break
        found = find_test_by_id(PROJECT_ROOT, failure["id"])
        if not found:
            continue

        correct_tokens, source, rule = get_expected_tokens(failure["input"])
        suzume_tokens = _get_suzume_tokens(failure["input"])
        processed += 1

        entry = {
            "id": failure["id"],
            "input": failure["input"],
            "test_expected": _surfaces(found["case"].get("expected") or []),
            "suzume": _surfaces(suzume_tokens),
            "correct": _surfaces(correct_tokens),
            "source": source,
            "rule": rule,
        }

        if tokens_match(correct_tokens, suzume_tokens):
            categories["matches_correct"].append(entry)
        elif entry["correct"] != entry["suzume"] and len(entry["correct"].split("|")) != len(
            entry["suzume"].split("|")
        ):
            categories["segmentation"].append(entry)
        else:
            categories["pos_only"].append(entry)

    seg_patterns: dict[str, list[dict]] = {}
    for entry in categories["segmentation"]:
        pat = _detect_segmentation_pattern(entry["correct"], entry["suzume"], entry["input"])
        seg_patterns.setdefault(pat, []).append(entry)

    def brief(ent: dict) -> dict:
        return {"id": ent["id"], "input": ent["input"], "correct": ent["correct"], "suzume": ent["suzume"]}

    return _json_result(
        {
            "categories": {
                "matches_correct": [
                    {
                        "id": ent["id"],
                        "input": ent["input"],
                        "test_expected": ent["test_expected"],
                        "suzume": ent["suzume"],
                        "correct": ent["correct"],
                        "rule": ent["rule"],
                        "source": ent["source"],
                    }
                    for ent in capped(categories["matches_correct"])
                ],
                "segmentation": {
                    pat: {"count": len(entries), "examples": [brief(ent) for ent in capped(entries)]}
                    for pat, entries in sorted(seg_patterns.items(), key=lambda item: -len(item[1]))
                },
                "pos_only": [brief(ent) for ent in capped(categories["pos_only"])],
            },
            "summary": {
                "matches_correct": len(categories["matches_correct"]),
                "segmentation": len(categories["segmentation"]),
                "pos_only": len(categories["pos_only"]),
                "total_failures": len(failures),
                "processed": processed,
            },
        }
    )


def _cases_with_input(loaded: list[tuple[Path, dict, list[dict]]]) -> list[dict]:
    """Every case that carries an input, with its qualified basename/id."""
    return [
        {"path": path, "data": data, "case": case, "input": case["input"], "id": f"{path.stem}/{_case_id(case, idx)}"}
        for path, data, cases in loaded
        for idx, case in enumerate(cases)
        if case.get("input", "")
    ]


@mcp.tool()
async def test_diff_mecab(file: str = "") -> str:
    """Find tests where expected differs from MeCab output, categorized by type.

    Args:
        file: Optional test file to check (without .json), or empty for all.
    """
    files = _get_test_files_filtered(file)
    if not files:
        return _json_error("No test files found")
    loaded, error = _load_test_cases(files)
    if error:
        return _json_error(error)

    categories: dict[str, list[dict]] = {"intentional": [], "segmentation": [], "pos_only": [], "lemma_only": []}
    mecab_compatible = 0
    errors: list[dict] = []
    metas = _cases_with_input(loaded)

    mecab_results = get_mecab_tokens_batch_subprocess([meta["input"] for meta in metas])
    for meta, (mecab, source, rule) in zip(metas, mecab_results, strict=True):
        if source == "error":
            errors.append({"id": meta["id"], "input": meta["input"], "error": rule})
            continue
        expected = meta["case"].get("expected") or []

        if tokens_match(expected, mecab):
            mecab_compatible += 1
            continue

        exp_str = _surfaces(expected)
        mec_str = _surfaces(mecab)
        entry = {"id": meta["id"], "input": meta["input"], "expected": exp_str, "mecab": mec_str, "rule": rule}

        if exp_str == mec_str:
            exp_pos = "|".join(t.get("pos", "") for t in expected)
            mec_pos = "|".join(t.get("pos", "") for t in mecab)
            if exp_pos == mec_pos:
                entry["expected_full"] = "|".join(
                    f"{t.get('surface', '')}/{t.get('pos', '')}/{t.get('lemma', t.get('surface', ''))}"
                    for t in expected
                )
                entry["mecab_full"] = "|".join(
                    f"{t['surface']}/{t.get('pos', '')}/{t.get('lemma', t['surface'])}" for t in mecab
                )
                categories["intentional" if rule else "lemma_only"].append(entry)
            else:
                entry["expected_pos"] = exp_pos
                entry["mecab_pos"] = mec_pos
                categories["intentional" if rule else "pos_only"].append(entry)
        else:
            categories["intentional" if rule else "segmentation"].append(entry)

    total_cases = len(metas)
    if total_cases == 0:
        return _json_error("No test cases found")

    processed = total_cases - len(errors)
    return _json_result(
        {
            "categories": {name: entries[:20] for name, entries in categories.items()},
            "errors": errors[:20],
            "summary": {
                "total_cases": total_cases,
                "processed": processed,
                "errors": len(errors),
                "mecab_compatible": mecab_compatible,
                "mecab_compatible_pct": round(100.0 * mecab_compatible / processed, 1) if processed else 0,
                "incompatible": processed - mecab_compatible,
                **{name: len(entries) for name, entries in categories.items()},
            },
        }
    )


@mcp.tool()
async def test_needs_suzume_update(
    file: str = "",
    apply: bool = False,
    test_ids: list[str] | None = None,
) -> str:
    """Find tests where expected doesn't match MeCab+SuzumeRules.

    Args:
        file: Optional test file (without .json), or empty for all.
        apply: If True, update test expectations. Default is dry-run.
        test_ids: Optional stable basename/id selectors for a safe partial sync.
    """
    selected_ids = set(test_ids or [])
    if file and selected_ids:
        return _json_error("file and test_ids cannot be combined")

    files = _get_test_files_filtered(file)
    if not files:
        return _json_error("No test files found")
    loaded, error = _load_test_cases(files)
    if error:
        return _json_error(error)

    metas = [meta for meta in _cases_with_input(loaded) if not selected_ids or meta["id"] in selected_ids]
    if selected_ids:
        missing_ids = sorted(selected_ids - {meta["id"] for meta in metas})
        if missing_ids:
            return _json_error(f"No tests found for ids: {', '.join(missing_ids)}")

    # One oracle subprocess for every input.
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

    if apply and needs_update:
        files_to_save: dict[Path, dict] = {}
        for entry in needs_update:
            try:
                formatted = _format_expected_checked(entry["correct_tokens"], entry["rule"])
            except RuntimeError as exc:
                return _json_error(str(exc))
            entry["meta"]["case"]["expected"] = formatted
            files_to_save[entry["meta"]["path"]] = entry["meta"]["data"]
        for path, data in files_to_save.items():
            save_json(path, data)

    return _json_result(
        {
            "needs_update": [entry["out"] for entry in needs_update],
            "by_rule": dict(sorted(by_rule.items())),
            "total": len(needs_update),
            "errors": normalization_errors,
            "applied": apply and bool(needs_update),
        }
    )
