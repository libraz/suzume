"""Test corpus mutation MCP tools."""

from ..app import mcp
from ..config import PROJECT_ROOT
from ..core.constants import VALID_POS
from ..core.suzume_cli import (
    get_expected_tokens_batch_subprocess,
    get_suzume_tokens_many,
)
from ..core.suzume_cli import (
    get_expected_tokens_subprocess as get_expected_tokens,
)
from ..core.suzume_utils import token_differences
from ..core.test_file_suggestions import suggest_test_files
from ..core.test_file_utils import (
    generate_id,
    get_cases,
    get_test_data_dir,
    load_json,
    normalize_test_file_name,
    save_json,
)
from ._cli_freshness import stale_cli_guard
from ._test_tools_common import (
    ORACLE_OVERRIDE_REMEDIATION,
    _case_locations,
    _find_case,
    _format_expected_checked,
    _get_test_files_filtered,
    _json_error,
    _json_result,
    _load_selected_cases,
)
from ._test_tools_organization import append_cases_partitioned

# Writing Suzume's own output into `expected` is the same banned per-case oracle as
# suzume_expected, only harder to see: the stored case keeps no trace of where the
# expectation came from, so nothing downstream can flag it.
_USE_SUZUME_REFUSAL = f"use_suzume is disabled by design. {ORACLE_OVERRIDE_REMEDIATION}"


_RED_PIN = "Suzume does not pass this expectation on surface, POS and lemma; see differences"
_UNCHECKED_PIN = "whether Suzume passes this expectation could not be checked; see check_error"


async def _pin_verdicts(cases: list[tuple[str, list[dict]]]) -> list[dict]:
    """Say, per (input, expected) case, whether Suzume passes it the way the C++ runner judges it."""
    outputs = await get_suzume_tokens_many([text for text, _expected in cases], skip_user_dict=True)
    verdicts = []
    for (_text, expected), output in zip(cases, outputs, strict=True):
        if isinstance(output, BaseException):
            verdicts.append({"passes": None, "check_error": str(output)})
            continue
        differences = token_differences(expected, output)
        verdicts.append({"passes": False, "differences": differences} if differences else {"passes": True})
    return verdicts


def _single_pin_report(verdict: dict) -> dict:
    """The verdict fields of a one-case registration, with a warning unless it passes."""
    if verdict["passes"] is True:
        return verdict
    reason = _RED_PIN if verdict["passes"] is False else _UNCHECKED_PIN
    return {**verdict, "warning": f"Registered, but {reason}."}


def _deduplicate_generated_ids(items: list[dict]) -> None:
    existing_ids = set()
    test_dir = get_test_data_dir(PROJECT_ROOT)
    for path in test_dir.glob("*.json"):
        existing_ids.update(str(case.get("id", "")) for case in get_cases(load_json(path), str(path)))

    for item in items:
        base_id = item["id"]
        test_id = base_id
        suffix = 2
        while test_id in existing_ids:
            test_id = f"{base_id}_{suffix}"
            suffix += 1
        item["id"] = test_id
        existing_ids.add(test_id)


@mcp.tool()
@stale_cli_guard()
async def test_add(
    input_text: str,
    file: str = "",
    case_id: str = "",
    description: str = "",
    use_suzume: bool = False,
) -> str:
    """Add a new test case to a test file.

    The result says whether Suzume currently passes the registered expectation
    on surface, POS and lemma (`passes`), with the differing token fields under
    `differences` and a `warning` when it does not. A red case is still
    registered, since pinning before a fix is sometimes intended.

    Args:
        input_text: Japanese text for the test case.
        file: Optional logical test file name. Empty uses the highest-ranked automatic suggestion.
        case_id: Optional custom ID (auto-generated if empty).
        description: Optional description.
        use_suzume: Rejected. Kept only so the refusal is reached instead of a schema error.
    """
    if use_suzume:
        return _json_error(_USE_SUZUME_REFUSAL)

    # Check for duplicates
    existing = _case_locations(PROJECT_ROOT, input_text)
    if existing:
        return _json_error("Duplicate input rejected", existing=existing)

    try:
        tokens, source, rule = get_expected_tokens(input_text)
        expected = _format_expected_checked(tokens, source)
    except RuntimeError as exc:
        return _json_error(str(exc))
    test_id = case_id or generate_id(input_text)

    if file:
        try:
            file_name = normalize_test_file_name(file)
        except ValueError as exc:
            return _json_error(str(exc))
        placement_source = "requested"
    else:
        file_name = suggest_test_files(input_text, tokens)[0]
        placement_source = "suggested"

    if not case_id:
        id_holder = {"id": test_id}
        _deduplicate_generated_ids([id_holder])
        test_id = id_holder["id"]

    new_case = {
        "id": test_id,
        "description": description or f"{input_text} - auto-generated",
        "input": input_text,
        "expected": expected,
    }

    try:
        placement = append_cases_partitioned(PROJECT_ROOT, file_name, [new_case])
    except (OSError, ValueError) as exc:
        return _json_error(str(exc))
    actual_file = placement["case_locations"][0]["file"]
    (verdict,) = await _pin_verdicts([(input_text, expected)])

    return _json_result(
        {
            "status": "ok",
            "file": actual_file,
            "requested_file": file_name,
            "placement_source": placement_source,
            "split": placement["split"],
            "input": input_text,
            "id": test_id,
            "source": source,
            "rule": rule,
            **_single_pin_report(verdict),
        }
    )


@mcp.tool()
@stale_cli_guard()
async def test_update(
    input_text: str = "",
    test_id: str = "",
    use_suzume: bool = False,
) -> str:
    """Update an existing test case's expected value.

    Like `test_add`, the result says whether Suzume passes the rewritten
    expectation (`passes`, `differences`, `warning`).

    Args:
        input_text: Input text to find and update. An input shared by several cases is
            refused with their file/id selectors; pass test_id instead.
        test_id: Test ID to update (format: file/index or file/id_string).
        use_suzume: Rejected. Kept only so the refusal is reached instead of a schema error.
    """
    if use_suzume:
        return _json_error(_USE_SUZUME_REFUSAL)

    found, error = _find_case(PROJECT_ROOT, input_text, test_id)
    if not found:
        return _json_error(error)
    if test_id:
        input_text = found["case"]["input"]

    try:
        tokens, source, _rule = get_expected_tokens(input_text)
        expected = _format_expected_checked(tokens, source)
    except RuntimeError as exc:
        return _json_error(str(exc))
    old_expected = found["case"].get("expected", [])

    found["case"]["expected"] = expected
    save_json(found["file"], found["data"])
    (verdict,) = await _pin_verdicts([(input_text, expected)])

    return _json_result(
        {
            "status": "ok",
            "file": found["basename"],
            "index": found["index"],
            "input": input_text,
            "old_surfaces": "|".join(t.get("surface", "") for t in old_expected),
            "new_surfaces": "|".join(t["surface"] for t in expected),
            "source": source,
            **_single_pin_report(verdict),
        }
    )


@mcp.tool()
async def test_delete(
    input_text: str = "",
    test_id: str = "",
) -> str:
    """Delete a test case.

    Args:
        input_text: Input text to find and delete. An input shared by several cases is
            refused with their file/id selectors; pass test_id instead.
        test_id: Test ID to delete (format: file/index or file/id_string).
    """
    found, error = _find_case(PROJECT_ROOT, input_text, test_id)
    if not found:
        return _json_error(error)

    case = found["case"]
    case_id = case.get("id", found["index"])
    surfaces = " ".join(t.get("surface", "") for t in (case.get("expected") or []))

    del get_cases(found["data"], str(found["file"]))[found["index"]]
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
@stale_cli_guard()
async def test_batch_add(
    file: str,
    inputs: list[str],
    apply: bool = False,
    use_suzume: bool = False,
) -> str:
    """Batch add multiple test cases.

    Every case in `to_add` says whether Suzume currently passes it on surface,
    POS and lemma (`passes`, plus `differences` when it does not), in the
    preview as well as on apply. `red` lists the inputs that do not pass and a
    `warning` names how many; red cases are still added.

    Args:
        file: Logical test file name. An empty string groups inputs by their highest-ranked suggestions.
        inputs: List of Japanese input texts to add.
        apply: If True, actually add. Default is dry-run preview.
        use_suzume: Rejected. Kept only so the refusal is reached instead of a schema error.
    """
    if use_suzume:
        return _json_error(_USE_SUZUME_REFUSAL)

    if file:
        try:
            file_name = normalize_test_file_name(file)
        except ValueError as exc:
            return _json_error(str(exc))
    else:
        file_name = ""

    to_add = []
    skipped = []

    # First pass: filter existing, collect new inputs
    new_inputs: list[str] = []
    seen_inputs: set[str] = set()
    for inp in inputs:
        if inp in seen_inputs:
            skipped.append({"input": inp, "reason": "duplicate within request"})
            continue
        seen_inputs.add(inp)
        existing = _case_locations(PROJECT_ROOT, inp)
        if existing:
            skipped.append({"input": inp, "reason": f"exists at {', '.join(existing)}"})
            continue
        new_inputs.append(inp)

    batch_results = get_expected_tokens_batch_subprocess(new_inputs) if new_inputs else []
    for inp, (tokens, source, rule) in zip(new_inputs, batch_results, strict=True):
        if source == "error":
            skipped.append({"input": inp, "reason": rule})
            continue
        try:
            expected = _format_expected_checked(tokens, source)
        except RuntimeError as exc:
            skipped.append({"input": inp, "reason": str(exc)})
            continue
        to_add.append(
            {
                "input": inp,
                "id": generate_id(inp),
                "surfaces": "|".join(e["surface"] for e in expected),
                "source": source,
                "rule": rule,
                "_expected": expected,
                "_file": file_name or suggest_test_files(inp, tokens)[0],
            }
        )

    _deduplicate_generated_ids(to_add)
    for item, verdict in zip(
        to_add, await _pin_verdicts([(item["input"], item["_expected"]) for item in to_add]), strict=True
    ):
        item.update(verdict)

    placements = []
    grouped_items: dict[str, list[dict]] = {}
    for item in to_add:
        grouped_items.setdefault(item["_file"], []).append(item)
    for logical_file, items in grouped_items.items():
        new_cases = [
            {
                "id": item["id"],
                "description": f"{item['input']} - regression test",
                "input": item["input"],
                "expected": item["_expected"],
            }
            for item in items
        ]
        try:
            placement = append_cases_partitioned(PROJECT_ROOT, logical_file, new_cases, apply=apply)
        except (OSError, ValueError) as exc:
            return _json_error(str(exc))
        placements.append(placement)
        for item, location in zip(items, placement["case_locations"], strict=True):
            item["file"] = location["file"]

    shown = ("input", "id", "surfaces", "source", "rule", "file", "passes", "differences", "check_error")
    red = [item["input"] for item in to_add if item["passes"] is False]
    unchecked = [item["input"] for item in to_add if item["passes"] is None]
    result = {
        "file": file_name,
        "files": [entry for placement in placements for entry in placement["files"]],
        "to_add": [{key: item[key] for key in shown if key in item} for item in to_add],
        "red": red,
        "skipped": skipped,
        "applied": apply,
    }
    if red or unchecked:
        verb = "are registered" if apply else "would be registered"
        notes = []
        if red:
            notes.append(f"{len(red)} of {len(to_add)} cases {verb} red: {_RED_PIN}")
        if unchecked:
            notes.append(f"{len(unchecked)} of {len(to_add)} cases could not be checked: see check_error")
        result["warning"] = ". ".join(notes) + "."
    return _json_result(result)


_ORACLE_OWNED_POS = (
    "POS expectations are oracle-owned and cannot be edited case-by-case. "
    "Update the normalization pipeline, then run test_needs_suzume_update(apply=True)."
)


def _preview_pos_occurrences(old_pos: str, new_pos: str, file: str, apply: bool, surface: str | None) -> str:
    """List expected tokens tagged *old_pos* (optionally only for *surface*); never writes."""
    if new_pos not in VALID_POS:
        return _json_error(f"Invalid new_pos: {new_pos}. Valid values: {', '.join(sorted(VALID_POS))}")
    if apply:
        return _json_error(_ORACLE_OWNED_POS)

    loaded, error = _load_selected_cases(_get_test_files_filtered(file))
    if error:
        return _json_error(error)

    changes = [
        {"file": path.stem, "case_id": case.get("id", ""), "surface": token["surface"]}
        for path, _data, cases in loaded
        for case in cases
        for token in case.get("expected") or []
        if token.get("pos") == old_pos and (surface is None or token.get("surface") == surface)
    ]
    result = {"old_pos": old_pos, "new_pos": new_pos}
    if surface is not None:
        result["surface"] = surface
    return _json_result({**result, "changes": changes, "total": len(changes), "applied": apply})


@mcp.tool()
async def test_replace_pos(
    old_pos: str,
    new_pos: str,
    file: str = "",
    apply: bool = False,
) -> str:
    """Preview POS occurrences in test expectations.

    Args:
        old_pos: POS value to replace.
        new_pos: New POS value.
        file: Optional test file filter (without .json), or empty for all.
        apply: Reserved for compatibility. POS expectations are oracle-owned
            and can never be changed by this tool.
    """
    return _preview_pos_occurrences(old_pos, new_pos, file, apply, surface=None)


@mcp.tool()
async def test_map_pos(
    surface: str,
    old_pos: str,
    new_pos: str,
    file: str = "",
    apply: bool = False,
) -> str:
    """Preview POS occurrences for a specific surface.

    Args:
        surface: Token surface to match.
        old_pos: Current POS value to replace.
        new_pos: New POS value.
        file: Optional test file filter.
        apply: Reserved for compatibility. POS expectations are oracle-owned
            and can never be changed by this tool.
    """
    return _preview_pos_occurrences(old_pos, new_pos, file, apply, surface=surface)


@mcp.tool()
async def test_list_pos(file: str = "") -> str:
    """List all POS values used in test expectations.

    Args:
        file: Optional test file filter (without .json).
    """
    loaded, error = _load_selected_cases(_get_test_files_filtered(file))
    if error:
        return _json_error(error)

    pos_counts: dict[str, int] = {}
    pos_examples: dict[str, list[str]] = {}
    for _path, _data, cases in loaded:
        for case in cases:
            for token in case.get("expected") or []:
                pos = token.get("pos", "UNKNOWN")
                pos_counts[pos] = pos_counts.get(pos, 0) + 1
                examples = pos_examples.setdefault(pos, [])
                if len(examples) < 3:
                    examples.append(token.get("surface", ""))

    return _json_result(
        {
            "pos_values": [
                {"pos": pos, "count": pos_counts[pos], "examples": pos_examples[pos]}
                for pos in sorted(pos_counts, key=lambda p: -pos_counts[p])
            ]
        }
    )
