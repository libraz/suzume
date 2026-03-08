"""Test tools ported from test_tool.pl - MCP tool registration."""

import json
import re
from pathlib import Path

from ..core.constants import TARI_ADVERB_STEMS
from ..core.pos_mapping import normalize_pos
from ..core.suzume_cli import get_suzume_debug_info
from ..core.suzume_utils import (
    format_expected,
    get_expected_tokens,
    get_suzume_rule,
    tokens_match,
)
from ..core.test_file_utils import (
    find_test_by_id,
    find_test_by_input,
    generate_id,
    get_failures_from_test_output,
    get_test_data_dir,
    get_test_files,
    load_json,
    save_json,
)
from ..server import PROJECT_ROOT, mcp


def _json_result(obj: dict | list) -> str:
    """Serialize result to JSON string."""
    return json.dumps(obj, ensure_ascii=False, indent=2)


def _json_error(message: str) -> str:
    """Return JSON error string."""
    return _json_result({"status": "error", "message": message})


def _get_suzume_tokens(text: str) -> list[dict]:
    """Get Suzume CLI tokens with POS and lemma."""
    cli = PROJECT_ROOT / "build" / "bin" / "suzume-cli"
    if not cli.exists():
        return []

    import subprocess

    result = subprocess.run(
        [str(cli), text],
        capture_output=True,
        text=True,
        timeout=30,
    )

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


def _get_test_files_filtered(file_filter: str = "") -> list[Path]:
    """Get test files, optionally filtered by name."""
    if file_filter and file_filter != "all":
        path = get_test_data_dir(PROJECT_ROOT) / f"{file_filter}.json"
        return [path] if path.exists() else []
    return get_test_files(PROJECT_ROOT)


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
    diff_type = "match"
    diff_details = []

    if not surface_match and suzume_surfaces:
        e_count = len(expected_surfaces)
        s_count = len(suzume_surfaces)
        if s_count < e_count:
            diff_type = "under-split"
        elif s_count > e_count:
            diff_type = "over-split"
        else:
            diff_type = "boundary"
        max_len = max(e_count, s_count)
        for idx in range(max_len):
            exp = expected_surfaces[idx] if idx < e_count else ""
            suz = suzume_surfaces[idx] if idx < s_count else ""
            if exp != suz:
                diff_details.append({"index": idx, "expected": exp, "suzume": suz, "type": "surface"})
    elif surface_match and not full_match:
        diff_type = "pos-lemma"
        for idx in range(len(expected_tokens)):
            exp = expected_tokens[idx]
            suz = suzume_tokens[idx]
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

    # JSON mode - keep as-is (already returns JSON)
    if mode == "json":
        return json.dumps(format_expected(expected_tokens), ensure_ascii=False, indent=2)

    # Build result dict for default/brief/debug modes
    test_exists = None
    if found:
        test_exists = {
            "file": found["basename"],
            "id": found["case"].get("id", str(found["index"])),
        }

    result = {
        "input": input_text,
        "expected": expected_surfaces,
        "suzume": suzume_surfaces if suzume_surfaces else None,
        "match": full_match,
        "diff_type": diff_type,
        "diff_details": diff_details,
        "rule": rule,
        "test_exists": test_exists,
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
            data = load_json(path)
            cases = data.get("cases") or data.get("test_cases") or []
            count = len(cases)
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

    matches = []
    for path in get_test_files(PROJECT_ROOT):
        try:
            data = load_json(path)
        except Exception:
            continue
        basename = path.stem
        cases = data.get("cases") or data.get("test_cases") or []
        for idx, case in enumerate(cases):
            case_id = case.get("id", str(idx))
            inp = case.get("input", "")
            surfaces = " ".join(t.get("surface", "") for t in (case.get("expected") or []))
            if regex.search(inp) or regex.search(surfaces) or regex.search(str(case_id)):
                entry = {
                    "file": basename,
                    "index": idx,
                    "id": case_id,
                    "input": inp,
                    "expected": surfaces,
                }
                matches.append(entry)

    if limit > 0:
        matches_limited = matches[:limit]
    else:
        matches_limited = matches

    return _json_result(
        {
            "pattern": pattern,
            "matches": matches_limited,
            "total": len(matches),
        }
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
        return _json_result(
            {
                "source": test_output_file,
                "failures": [],
                "total": 0,
            }
        )

    if grep:
        try:
            rxp = re.compile(grep)
        except re.error:
            return _json_error(f"Invalid grep pattern: {grep}")
        failures = [f for f in failures if rxp.search(f["input"]) or rxp.search(f["id"])]

    if limit > 0:
        failures_limited = failures[:limit]
    else:
        failures_limited = failures

    return _json_result(
        {
            "source": test_output_file,
            "failures": [{"input": f["input"], "id": f["id"]} for f in failures_limited],
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

    improved = sorted([{"id": key, "input": before[key]} for key in before if key not in after], key=lambda x: x["id"])
    regressed = sorted([{"id": key, "input": after[key]} for key in after if key not in before], key=lambda x: x["id"])

    delta = len(before) - len(after)

    return _json_result(
        {
            "before_failures": len(before),
            "after_failures": len(after),
            "improved": improved,
            "regressed": regressed,
            "net_change": -delta if delta > 0 else abs(delta),
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
    if not failures:
        return _json_result(
            {
                "categories": {"matches_correct": [], "segmentation": {}, "pos_only": []},
                "summary": {
                    "matches_correct": 0,
                    "segmentation": 0,
                    "pos_only": 0,
                    "total_failures": 0,
                    "processed": 0,
                },
            }
        )

    total_failures = len(failures)
    max_process = limit * 5 if limit > 0 else 0

    categories: dict[str, list[dict]] = {"matches_correct": [], "segmentation": [], "pos_only": []}
    processed = 0

    for failure in failures:
        if max_process and processed >= max_process:
            break
        found = find_test_by_id(PROJECT_ROOT, failure["id"])
        if not found:
            continue

        test_expected = found["case"].get("expected") or []
        correct_tokens, source, rule = get_expected_tokens(failure["input"])
        suzume_tokens = _get_suzume_tokens(failure["input"])
        processed += 1

        test_str = "|".join(t.get("surface", "") for t in test_expected)
        suz_str = "|".join(t["surface"] for t in suzume_tokens)
        cor_str = "|".join(t["surface"] for t in correct_tokens)

        entry = {
            "id": failure["id"],
            "input": failure["input"],
            "test_expected": test_str,
            "suzume": suz_str,
            "correct": cor_str,
            "source": source,
            "rule": rule,
        }

        if tokens_match(correct_tokens, suzume_tokens):
            categories["matches_correct"].append(entry)
        elif cor_str != suz_str and len(cor_str.split("|")) != len(suz_str.split("|")):
            categories["segmentation"].append(entry)
        else:
            categories["pos_only"].append(entry)

    # Group segmentation by pattern
    seg_patterns: dict[str, list[dict]] = {}
    for entry in categories["segmentation"]:
        pat = _detect_segmentation_pattern(entry["correct"], entry["suzume"], entry["input"])
        seg_patterns.setdefault(pat, []).append(entry)

    per_cat = limit if limit > 0 else 0

    # Build output categories
    matches_correct_out = categories["matches_correct"][:per_cat] if per_cat else categories["matches_correct"]
    pos_only_out = categories["pos_only"][:per_cat] if per_cat else categories["pos_only"]

    seg_out = {}
    for pat in sorted(seg_patterns.keys(), key=lambda p: -len(seg_patterns[p])):
        entries = seg_patterns[pat]
        seg_out[pat] = {
            "count": len(entries),
            "examples": [
                {"id": ent["id"], "input": ent["input"], "correct": ent["correct"], "suzume": ent["suzume"]}
                for ent in (entries[:per_cat] if per_cat else entries)
            ],
        }

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
                    for ent in matches_correct_out
                ],
                "segmentation": seg_out,
                "pos_only": [
                    {"id": ent["id"], "input": ent["input"], "correct": ent["correct"], "suzume": ent["suzume"]}
                    for ent in pos_only_out
                ],
            },
            "summary": {
                "matches_correct": len(categories["matches_correct"]),
                "segmentation": len(categories["segmentation"]),
                "pos_only": len(categories["pos_only"]),
                "total_failures": total_failures,
                "processed": processed,
            },
        }
    )


@mcp.tool()
async def test_diff_mecab(file: str = "") -> str:
    """Find tests where expected differs from MeCab output, categorized by type.

    Args:
        file: Optional test file to check (without .json), or empty for all.
    """
    from ..core.suzume_utils import get_mecab_tokens as get_mecab_toks

    files = _get_test_files_filtered(file)
    if not files:
        return _json_error("No test files found")

    categories: dict[str, list[dict]] = {
        "intentional": [],
        "segmentation": [],
        "pos_only": [],
        "lemma_only": [],
    }
    total_cases = 0
    mecab_compatible = 0

    for path in files:
        try:
            data = load_json(path)
        except Exception:
            continue
        basename = path.stem
        cases = data.get("cases") or data.get("test_cases") or []
        for idx, case in enumerate(cases):
            inp = case.get("input", "")
            if not inp:
                continue
            total_cases += 1
            expected = case.get("expected") or []
            mecab = get_mecab_toks(inp)

            if tokens_match(expected, mecab):
                mecab_compatible += 1
                continue

            rule = get_suzume_rule(inp)
            case_id = case.get("id", str(idx))
            exp_str = "|".join(t.get("surface", "") for t in expected)
            mec_str = "|".join(t["surface"] for t in mecab)

            entry = {"id": f"{basename}/{case_id}", "input": inp, "expected": exp_str, "mecab": mec_str, "rule": rule}

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

    if total_cases == 0:
        return _json_error("No test cases found")

    incompatible = total_cases - mecab_compatible

    # Limit each category to 20 items in output
    cat_out: dict[str, list[dict]] = {}
    for cat_name in ("intentional", "segmentation", "pos_only", "lemma_only"):
        cat_out[cat_name] = categories[cat_name][:20]

    return _json_result(
        {
            "categories": cat_out,
            "summary": {
                "total_cases": total_cases,
                "mecab_compatible": mecab_compatible,
                "mecab_compatible_pct": round(100.0 * mecab_compatible / total_cases, 1) if total_cases else 0,
                "incompatible": incompatible,
                "intentional": len(categories["intentional"]),
                "segmentation": len(categories["segmentation"]),
                "pos_only": len(categories["pos_only"]),
                "lemma_only": len(categories["lemma_only"]),
            },
        }
    )


@mcp.tool()
async def test_needs_suzume_update(file: str = "", apply: bool = False) -> str:
    """Find tests where expected doesn't match MeCab+SuzumeRules.

    Args:
        file: Optional test file (without .json), or empty for all.
        apply: If True, update test expectations. Default is dry-run.
    """
    files = _get_test_files_filtered(file)
    if not files:
        return _json_error("No test files found")

    needs_update = []
    by_rule: dict[str, list[dict]] = {}

    for path in files:
        try:
            data = load_json(path)
        except Exception:
            continue
        basename = path.stem
        cases_key = "cases" if "cases" in data else "test_cases"
        cases = data.get(cases_key) or []

        for idx, case in enumerate(cases):
            inp = case.get("input", "")
            if not inp:
                continue
            expected = case.get("expected") or []
            correct, source, rule = get_expected_tokens(inp)
            rule = rule or ""

            if not tokens_match(expected, correct):
                case_id = case.get("id", str(idx))
                exp_str = "|".join(t.get("surface", "") for t in expected)
                cor_str = "|".join(t["surface"] for t in correct)
                exp_pos = "|".join(t.get("pos", "") for t in expected)
                cor_pos = "|".join(t["pos"] for t in correct)
                diff_type = "surface" if exp_str != cor_str else ("pos" if exp_pos != cor_pos else "lemma")

                entry = {
                    "id": f"{basename}/{case_id}",
                    "file": path,
                    "basename": basename,
                    "index": idx,
                    "input": inp,
                    "rule": rule or "mecab-only",
                    "expected": exp_str,
                    "correct": cor_str,
                    "expected_pos": exp_pos,
                    "correct_pos": cor_pos,
                    "diff_type": diff_type,
                    "correct_tokens": correct,
                    "data": data,
                    "cases_key": cases_key,
                }
                needs_update.append(entry)
                by_rule.setdefault(rule or "mecab-only", []).append(entry)

    if not needs_update:
        return _json_result(
            {
                "needs_update": [],
                "by_rule": {},
                "total": 0,
                "applied": False,
            }
        )

    # Build output entries (without internal data/path objects)
    output_entries = []
    for entry in needs_update:
        output_entries.append(
            {
                "id": entry["id"],
                "input": entry["input"],
                "rule": entry["rule"],
                "diff_type": entry["diff_type"],
                "expected": entry["expected"],
                "correct": entry["correct"],
            }
        )

    by_rule_out: dict[str, list[str]] = {}
    for rule_name in sorted(by_rule.keys()):
        by_rule_out[rule_name] = [ent["id"] for ent in by_rule[rule_name]]

    if not apply:
        return _json_result(
            {
                "needs_update": output_entries,
                "by_rule": by_rule_out,
                "total": len(needs_update),
                "applied": False,
            }
        )

    # Apply updates
    files_to_save: dict[Path, dict] = {}
    for entry in needs_update:
        formatted = format_expected(entry["correct_tokens"])
        entry["data"][entry["cases_key"]][entry["index"]]["expected"] = formatted
        files_to_save[entry["file"]] = entry["data"]

    for path, data in files_to_save.items():
        save_json(path, data)

    return _json_result(
        {
            "needs_update": output_entries,
            "by_rule": by_rule_out,
            "total": len(needs_update),
            "applied": True,
        }
    )


@mcp.tool()
async def test_add(
    input_text: str,
    file: str,
    case_id: str = "",
    description: str = "",
    use_suzume: bool = False,
) -> str:
    """Add a new test case to a test file.

    Args:
        input_text: Japanese text for the test case.
        file: Target test file name (without .json).
        case_id: Optional custom ID (auto-generated if empty).
        description: Optional description.
        use_suzume: If True, use Suzume output instead of MeCab for expected.
    """
    # Check for duplicates
    existing = find_test_by_input(PROJECT_ROOT, input_text)
    if existing:
        return _json_result(
            {
                "status": "error",
                "message": "Duplicate input rejected",
                "existing": {"file": existing["basename"], "index": existing["index"]},
            }
        )

    if use_suzume:
        suzume_tokens = _get_suzume_tokens(input_text)
        if not suzume_tokens:
            return _json_error("Suzume CLI not available")
        tokens, source, rule = suzume_tokens, "Suzume", "forced"
    else:
        tokens, source, rule = get_expected_tokens(input_text)

    expected = format_expected(tokens)
    test_id = case_id or generate_id(input_text)

    path = get_test_data_dir(PROJECT_ROOT) / f"{file}.json"
    if path.exists():
        data = load_json(path)
    else:
        data = {"version": "1.0", "description": f"{file} tests", "cases": []}

    cases_key = "cases" if "cases" in data else "test_cases"
    data.setdefault(cases_key, [])

    # Deduplicate ID
    if not case_id:
        existing_ids = {c.get("id") for c in data[cases_key]}
        if test_id in existing_ids:
            suffix = 2
            while f"{test_id}_{suffix}" in existing_ids:
                suffix += 1
            test_id = f"{test_id}_{suffix}"

    new_case = {
        "id": test_id,
        "description": description or f"{input_text} - auto-generated",
        "input": input_text,
        "expected": expected,
    }

    data[cases_key].append(new_case)
    save_json(path, data)

    return _json_result(
        {
            "status": "ok",
            "file": file,
            "input": input_text,
            "id": test_id,
            "source": source,
            "rule": rule,
        }
    )


@mcp.tool()
async def test_update(
    input_text: str = "",
    test_id: str = "",
    use_suzume: bool = False,
) -> str:
    """Update an existing test case's expected value.

    Args:
        input_text: Input text to find and update (mutually exclusive with test_id).
        test_id: Test ID to update (format: file/index or file/id_string).
        use_suzume: If True, use Suzume output for expected.
    """
    if test_id:
        found = find_test_by_id(PROJECT_ROOT, test_id)
        if not found:
            return _json_error(f"Test not found: {test_id}")
        input_text = found["case"]["input"]
    elif input_text:
        found = find_test_by_input(PROJECT_ROOT, input_text)
        if not found:
            return _json_error(f"No test found for input: {input_text}")
    else:
        return _json_error("Either input_text or test_id is required.")

    if use_suzume:
        suzume_tokens = _get_suzume_tokens(input_text)
        if not suzume_tokens:
            return _json_error("Suzume CLI not available")
        tokens, source, rule = suzume_tokens, "Suzume", "forced"
    else:
        tokens, source, rule = get_expected_tokens(input_text)

    expected = format_expected(tokens)
    cases_key = "cases" if "cases" in found["data"] else "test_cases"
    old_expected = found["case"].get("expected", [])

    found["data"][cases_key][found["index"]]["expected"] = expected
    save_json(found["file"], found["data"])

    old_surfaces = "|".join(t.get("surface", "") for t in old_expected)
    new_surfaces = "|".join(t["surface"] for t in expected)

    return _json_result(
        {
            "status": "ok",
            "file": found["basename"],
            "index": found["index"],
            "input": input_text,
            "old_surfaces": old_surfaces,
            "new_surfaces": new_surfaces,
            "source": source,
        }
    )


@mcp.tool()
async def test_delete(
    input_text: str = "",
    test_id: str = "",
) -> str:
    """Delete a test case.

    Args:
        input_text: Input text to find and delete.
        test_id: Test ID to delete (format: file/index or file/id_string).
    """
    if test_id:
        found = find_test_by_id(PROJECT_ROOT, test_id)
        if not found:
            return _json_error(f"Test not found: {test_id}")
    elif input_text:
        found = find_test_by_input(PROJECT_ROOT, input_text)
        if not found:
            return _json_error(f"No test found for input: {input_text}")
    else:
        return _json_error("Either input_text or test_id is required.")

    case = found["case"]
    case_id = case.get("id", found["index"])
    surfaces = " ".join(t.get("surface", "") for t in (case.get("expected") or []))

    cases_key = "cases" if "cases" in found["data"] else "test_cases"
    del found["data"][cases_key][found["index"]]
    save_json(found["file"], found["data"])

    return _json_result(
        {
            "status": "ok",
            "file": found["basename"],
            "id": case_id,
            "input": case.get("input", ""),
            "surfaces": surfaces,
        }
    )


@mcp.tool()
async def test_batch_add(
    file: str,
    inputs: list[str],
    apply: bool = False,
    use_suzume: bool = False,
) -> str:
    """Batch add multiple test cases.

    Args:
        file: Target test file name (without .json).
        inputs: List of Japanese input texts to add.
        apply: If True, actually add. Default is dry-run preview.
        use_suzume: If True, use Suzume output for expected.
    """
    to_add = []
    skipped = []

    for inp in inputs:
        existing = find_test_by_input(PROJECT_ROOT, inp)
        if existing:
            skipped.append({"input": inp, "reason": f"exists at {existing['basename']}/{existing['index']}"})
            continue
        if use_suzume:
            suzume_tokens = _get_suzume_tokens(inp)
            if suzume_tokens:
                tokens, source, rule = suzume_tokens, "Suzume", "forced"
            else:
                tokens, source, rule = get_expected_tokens(inp)
        else:
            tokens, source, rule = get_expected_tokens(inp)
        expected = format_expected(tokens)
        to_add.append(
            {
                "input": inp,
                "id": generate_id(inp),
                "surfaces": "|".join(e["surface"] for e in expected),
                "source": source,
                "rule": rule,
                "_expected": expected,
            }
        )

    if not apply:
        # Strip internal fields for output
        to_add_out = [
            {
                "input": item["input"],
                "id": item["id"],
                "surfaces": item["surfaces"],
                "source": item["source"],
                "rule": item["rule"],
            }
            for item in to_add
        ]
        return _json_result(
            {
                "file": file,
                "to_add": to_add_out,
                "skipped": skipped,
                "applied": False,
            }
        )

    path = get_test_data_dir(PROJECT_ROOT) / f"{file}.json"
    if path.exists():
        data = load_json(path)
    else:
        data = {"version": "1.0", "description": f"{file} tests", "cases": []}

    cases_key = "cases" if "cases" in data else "test_cases"
    data.setdefault(cases_key, [])

    for item in to_add:
        data[cases_key].append(
            {
                "id": item["id"],
                "description": f"{item['input']} - regression test",
                "input": item["input"],
                "expected": item["_expected"],
            }
        )

    save_json(path, data)

    to_add_out = [
        {
            "input": item["input"],
            "id": item["id"],
            "surfaces": item["surfaces"],
            "source": item["source"],
            "rule": item["rule"],
        }
        for item in to_add
    ]
    return _json_result(
        {
            "file": file,
            "to_add": to_add_out,
            "skipped": skipped,
            "applied": True,
        }
    )


@mcp.tool()
async def test_replace_pos(
    old_pos: str,
    new_pos: str,
    file: str = "",
    apply: bool = False,
) -> str:
    """Replace POS in all test files (dry-run by default).

    Args:
        old_pos: POS value to replace.
        new_pos: New POS value.
        file: Optional test file filter (without .json), or empty for all.
        apply: If True, apply changes. Default is dry-run.
    """
    files = _get_test_files_filtered(file or "all")
    if not files:
        return _json_error("No test files found")

    changes = []
    for path in files:
        try:
            data = load_json(path)
        except Exception:
            continue
        cases_key = "cases" if "cases" in data else "test_cases"
        cases = data.get(cases_key) or []
        file_changes = 0

        for case in cases:
            for token in case.get("expected") or []:
                if token.get("pos") == old_pos:
                    changes.append({"file": path.stem, "case_id": case.get("id", ""), "surface": token["surface"]})
                    if apply:
                        token["pos"] = new_pos
                    file_changes += 1

        if apply and file_changes > 0:
            save_json(path, data)

    if not changes:
        return _json_result(
            {
                "old_pos": old_pos,
                "new_pos": new_pos,
                "changes": [],
                "total": 0,
                "applied": apply,
            }
        )

    return _json_result(
        {
            "old_pos": old_pos,
            "new_pos": new_pos,
            "changes": changes,
            "total": len(changes),
            "applied": apply,
        }
    )


@mcp.tool()
async def test_map_pos(
    surface: str,
    old_pos: str,
    new_pos: str,
    file: str = "",
    apply: bool = False,
) -> str:
    """Replace POS only for a specific surface (dry-run by default).

    Args:
        surface: Token surface to match.
        old_pos: Current POS value to replace.
        new_pos: New POS value.
        file: Optional test file filter.
        apply: If True, apply changes.
    """
    files = _get_test_files_filtered(file or "all")
    if not files:
        return _json_error("No test files found")

    changes = []
    for path in files:
        try:
            data = load_json(path)
        except Exception:
            continue
        cases_key = "cases" if "cases" in data else "test_cases"
        cases = data.get(cases_key) or []
        file_changes = 0

        for case in cases:
            for token in case.get("expected") or []:
                if token.get("surface") == surface and token.get("pos") == old_pos:
                    changes.append({"file": path.stem, "case_id": case.get("id", ""), "surface": surface})
                    if apply:
                        token["pos"] = new_pos
                    file_changes += 1

        if apply and file_changes > 0:
            save_json(path, data)

    if not changes:
        return _json_result(
            {
                "old_pos": old_pos,
                "new_pos": new_pos,
                "surface": surface,
                "changes": [],
                "total": 0,
                "applied": apply,
            }
        )

    return _json_result(
        {
            "old_pos": old_pos,
            "new_pos": new_pos,
            "surface": surface,
            "changes": changes,
            "total": len(changes),
            "applied": apply,
        }
    )


@mcp.tool()
async def test_list_pos(file: str = "") -> str:
    """List all POS values used in test expectations.

    Args:
        file: Optional test file filter (without .json).
    """
    files = _get_test_files_filtered(file or "all")
    if not files:
        return _json_error("No test files found")

    pos_counts: dict[str, int] = {}
    pos_examples: dict[str, list[str]] = {}

    for path in files:
        try:
            data = load_json(path)
        except Exception:
            continue
        cases = data.get("cases") or data.get("test_cases") or []
        for case in cases:
            for token in case.get("expected") or []:
                pos = token.get("pos", "UNKNOWN")
                pos_counts[pos] = pos_counts.get(pos, 0) + 1
                if pos not in pos_examples:
                    pos_examples[pos] = []
                if len(pos_examples[pos]) < 3:
                    pos_examples[pos].append(token.get("surface", ""))

    pos_values = []
    for pos in sorted(pos_counts.keys(), key=lambda p: -pos_counts[p]):
        pos_values.append(
            {
                "pos": pos,
                "count": pos_counts[pos],
                "examples": pos_examples[pos],
            }
        )

    return _json_result({"pos_values": pos_values})


@mcp.tool()
async def test_accept_diff(
    input_text: str = "",
    reason: str = "",
    category: str = "pos-limitation",
    all_failed: bool = False,
    test_output_file: str = "/tmp/test.txt",
    apply: bool = False,
) -> str:
    """Accept Suzume's current output as valid by adding suzume_expected field.

    Keeps MeCab-compatible expected for reference but marks Suzume's output as acceptable.

    Args:
        input_text: Input text to accept diff for (mutually exclusive with all_failed).
        reason: Reason for accepting the diff (required).
        category: Diff category (default: pos-limitation).
        all_failed: If True, process all failed tests from test output file.
        test_output_file: Path to ctest output file (used with all_failed).
        apply: If True, apply changes. Default is dry-run.
    """
    if not reason:
        return _json_error("reason is required. Explain why this diff is acceptable.")

    if not input_text and not all_failed:
        return _json_error("Either input_text or all_failed=True is required.")

    to_update = []

    if all_failed:
        failures = get_failures_from_test_output(test_output_file)
        if not failures:
            return _json_result(
                {
                    "status": "ok",
                    "reason": reason,
                    "category": category,
                    "updates": [],
                    "applied": False,
                }
            )

        for failure in failures:
            found = find_test_by_input(PROJECT_ROOT, failure["input"])
            if not found:
                continue
            if found["case"].get("suzume_expected"):
                continue
            to_update.append({"input": failure["input"], "found": found})
    else:
        found = find_test_by_input(PROJECT_ROOT, input_text)
        if not found:
            return _json_error(f"No test found for input: {input_text}")
        if found["case"].get("suzume_expected"):
            return _json_error(f"Test already has suzume_expected: {found['basename']}/{found['index']}")
        to_update.append({"input": input_text, "found": found})

    if not to_update:
        return _json_result(
            {
                "status": "ok",
                "reason": reason,
                "category": category,
                "updates": [],
                "applied": False,
            }
        )

    updates = []
    for item in to_update:
        inp = item["input"]
        found = item["found"]
        suzume_tokens = _get_suzume_tokens(inp)
        suzume_expected = format_expected(suzume_tokens)

        exp_surfaces = "|".join(t.get("surface", "") for t in (found["case"].get("expected") or []))
        suz_surfaces = "|".join(t["surface"] for t in suzume_expected)

        updates.append(
            {
                "file": found["basename"],
                "index": found["index"],
                "input": inp,
                "mecab_expected": exp_surfaces,
                "suzume_expected": suz_surfaces,
            }
        )

        if apply:
            cases_key = "cases" if "cases" in found["data"] else "test_cases"
            found["data"][cases_key][found["index"]]["suzume_expected"] = suzume_expected
            found["data"][cases_key][found["index"]]["accepted_diff"] = {
                "reason": reason,
                "category": category,
            }
            save_json(found["file"], found["data"])

    return _json_result(
        {
            "status": "ok",
            "reason": reason,
            "category": category,
            "updates": updates,
            "applied": apply,
        }
    )


@mcp.tool()
async def test_reset_suzume(
    input_text: str = "",
    all_tests: bool = False,
    file: str = "",
    apply: bool = False,
) -> str:
    """Remove stale suzume_expected field from test cases.

    Use when Suzume now matches expected (no override needed).

    Args:
        input_text: Input text to reset (mutually exclusive with all_tests).
        all_tests: If True, find all tests with suzume_expected.
        file: Optional test file filter (without .json).
        apply: If True, apply changes. Default is dry-run.
    """
    to_reset = []

    if all_tests:
        files = _get_test_files_filtered(file or "all")
        for path in files:
            try:
                data = load_json(path)
            except Exception:
                continue
            basename = path.stem
            cases_key = "cases" if "cases" in data else "test_cases"
            cases = data.get(cases_key) or []
            for idx, case in enumerate(cases):
                if case.get("suzume_expected"):
                    to_reset.append(
                        {
                            "input": case.get("input", ""),
                            "found": {
                                "file": path,
                                "data": data,
                                "case": case,
                                "index": idx,
                                "basename": basename,
                            },
                        }
                    )
    elif input_text:
        found = find_test_by_input(PROJECT_ROOT, input_text)
        if not found:
            return _json_error(f"No test found for input: {input_text}")
        if not found["case"].get("suzume_expected"):
            return _json_error(f"Test has no suzume_expected: {found['basename']}/{found['index']}")
        to_reset.append({"input": input_text, "found": found})
    else:
        return _json_error("Either input_text or all_tests=True is required.")

    if not to_reset:
        return _json_result(
            {
                "status": "ok",
                "reset": [],
                "total": 0,
                "applied": False,
            }
        )

    reset_entries = []
    files_to_save: dict[Path, dict] = {}
    removed = 0

    for item in to_reset:
        found = item["found"]
        reset_entries.append(
            {
                "file": found["basename"],
                "index": found["index"],
                "input": item["input"],
            }
        )

        if apply:
            cases_key = "cases" if "cases" in found["data"] else "test_cases"
            case = found["data"][cases_key][found["index"]]
            case.pop("suzume_expected", None)
            case.pop("accepted_diff", None)
            files_to_save[found["file"]] = found["data"]
            removed += 1

    if apply:
        for path, data in files_to_save.items():
            save_json(path, data)

    return _json_result(
        {
            "status": "ok",
            "reset": reset_entries,
            "total": len(to_reset),
            "applied": apply,
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

    problems = []

    for path in files:
        try:
            data = load_json(path)
        except Exception:
            continue
        basename = path.stem
        cases_key = "cases" if "cases" in data else "test_cases"
        cases = data.get(cases_key) or []
        file_ids: dict[str, bool] = {}

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
                        "file": path,
                        "basename": basename,
                        "index": idx,
                        "old_id": case_id,
                        "new_id": new_id,
                        "input": inp,
                        "reason": reason,
                        "data": data,
                        "cases_key": cases_key,
                    }
                )
                file_ids[new_id] = True
            else:
                file_ids[sanitized] = True

    if not problems:
        return _json_result(
            {
                "problems": [],
                "total": 0,
                "applied": False,
            }
        )

    problems_out = [
        {
            "file": p["basename"],
            "index": p["index"],
            "old_id": p["old_id"],
            "new_id": p["new_id"],
            "reason": p["reason"],
        }
        for p in problems
    ]

    if not apply:
        return _json_result(
            {
                "problems": problems_out,
                "total": len(problems),
                "applied": False,
            }
        )

    files_to_save: dict[Path, dict] = {}
    for prob in problems:
        prob["data"][prob["cases_key"]][prob["index"]]["id"] = prob["new_id"]
        files_to_save[prob["file"]] = prob["data"]

    for path, data in files_to_save.items():
        save_json(path, data)

    return _json_result(
        {
            "problems": problems_out,
            "total": len(problems),
            "applied": True,
        }
    )


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

    pos_count: dict[str, int] = {}
    for tok in tokens:
        pos_count[tok["pos"]] = pos_count.get(tok["pos"], 0) + 1

    suggestions = []
    first_pos = tokens[0]["pos"]
    has_verb = "Verb" in pos_count
    has_aux = "Auxiliary" in pos_count
    has_adj = "Adjective" in pos_count

    if has_adj and first_pos == "Adjective":
        if input_text.endswith("そう"):
            suggestions.append("adjective_i_compound")
        elif "く" in input_text:
            suggestions.append("adjective_i_ku")
        elif "かっ" in input_text:
            suggestions.append("adjective_i_katta")
        else:
            suggestions.append("adjective_i_basic")

    if has_verb:
        for tok in tokens:
            if tok["pos"] == "Verb":
                lemma = tok.get("lemma", "")
                if lemma.endswith("する"):
                    suggestions.append("verb_suru")
                elif re.search(r"(来る|くる)$", lemma):
                    suggestions.append("verb_irregular")
                else:
                    suggestions.append("verb_godan_misc")
                break
        if any(tok["surface"] in ("て", "で") for tok in tokens):
            suggestions.append("verb_te_ta")

    if has_aux and not has_verb and not has_adj:
        suggestions.append("auxiliary_modality")

    if first_pos == "Noun":
        suggestions.append("noun_general")

    if re.search(r"(です|ます)", input_text):
        suggestions.append("auxiliary_politeness")
    if input_text.endswith("ない"):
        suggestions.append("auxiliary_negation")
    if re.search(r"(だ|である)", input_text):
        suggestions.append("copula")

    if not suggestions:
        suggestions.append("basic")

    # Deduplicate while preserving order
    seen: set[str] = set()
    unique = []
    for sug in suggestions:
        if sug not in seen:
            seen.add(sug)
            unique.append(sug)

    token_list = [{"surface": tok["surface"], "pos": tok["pos"]} for tok in tokens]

    return _json_result(
        {
            "input": input_text,
            "tokens": token_list,
            "suggestions": unique,
        }
    )
