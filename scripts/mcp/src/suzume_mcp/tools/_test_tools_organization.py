"""Safe inspection and reorganization of tokenization test files."""

import hashlib
import json
import math
import re
from pathlib import Path

from ..core.test_file_utils import (
    TEST_CASES_KEY,
    get_cases,
    get_test_data_dir,
    get_test_files,
    load_json,
    normalize_test_file_name,
    save_json,
)
from ..server import PROJECT_ROOT, mcp
from ._test_tools_common import _json_error, _json_result, _load_test_cases

MAX_TEST_CASES_PER_FILE = 100
MAX_TEST_FILE_BYTES = 131072
_PART_SUFFIX_RE = re.compile(r"^(?P<base>.+)_(?P<number>[0-9]{2,})$")
_PART_DESCRIPTION_RE = re.compile(r" \(part [0-9]+/[0-9]+\)$")


def _serialized_size(data: dict) -> int:
    content = json.dumps(data, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    return len(content.encode("utf-8"))


def _case_digest(cases: list[dict]) -> str:
    payload = json.dumps(cases, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()


def _sanitize_gtest_name(value: str) -> str:
    return re.sub(r"[^a-zA-Z0-9_]", "_", value)


def _suite_name(path: Path) -> str:
    name = _sanitize_gtest_name(path.stem)
    if name and name[0].islower():
        name = name[0].upper() + name[1:]
    return name


def _parameter_names(path: Path, cases: list[dict]) -> list[str]:
    suite = _suite_name(path)
    return [f"{suite}_{_sanitize_gtest_name(str(case.get('id', '')))}" for case in cases]


def _validate_cases(path: Path, cases: list[dict]) -> list[str]:
    problems: list[str] = []
    required = {"id", "input", "expected"}
    parameter_names = _parameter_names(path, cases)
    seen_names: set[str] = set()

    for index, case in enumerate(cases):
        missing = sorted(required - case.keys())
        if missing:
            problems.append(f"{path.name}[{index}] missing fields: {', '.join(missing)}")
        parameter_name = parameter_names[index]
        if not parameter_name or parameter_name.endswith("_"):
            problems.append(f"{path.name}[{index}] has an empty GoogleTest case ID")
        elif parameter_name in seen_names:
            problems.append(f"{path.name}[{index}] duplicates GoogleTest name: {parameter_name}")
        seen_names.add(parameter_name)

    return problems


def _balanced_chunks(cases: list[dict], max_cases: int) -> list[list[dict]]:
    part_count = math.ceil(len(cases) / max_cases)
    base_size, extra = divmod(len(cases), part_count)
    chunks: list[list[dict]] = []
    start = 0
    for part_index in range(part_count):
        size = base_size + (1 if part_index < extra else 0)
        chunks.append(cases[start : start + size])
        start += size
    return chunks


def _fits_limits(data: dict, cases: list[dict], max_cases: int, max_bytes: int) -> bool:
    candidate = {**data, TEST_CASES_KEY: cases}
    return len(cases) <= max_cases and _serialized_size(candidate) <= max_bytes


def _oversized_case_error(case: dict, max_bytes: int) -> ValueError:
    return ValueError(f"A single test case exceeds the {max_bytes}-byte file limit: {case.get('id', '')}")


def _limited_chunks(source_data: dict, cases: list[dict], max_cases: int, max_bytes: int) -> list[list[dict]]:
    chunks: list[list[dict]] = []
    current: list[dict] = []
    for case in cases:
        candidate = [*current, case]
        if _fits_limits(_part_data(source_data, candidate, 9999, 9999), candidate, max_cases, max_bytes):
            current = candidate
            continue
        if current:
            chunks.append(current)
        current = [case]
        if not _fits_limits(_part_data(source_data, current, 9999, 9999), current, max_cases, max_bytes):
            raise _oversized_case_error(case, max_bytes)
    if current:
        chunks.append(current)
    return chunks


def _part_path(test_dir: Path, family: str, part_number: int, part_count: int) -> Path:
    width = max(2, len(str(part_count)))
    return test_dir / f"{family}_{str(part_number).zfill(width)}.json"


def _part_data(source_data: dict, cases: list[dict], part_number: int, part_count: int) -> dict:
    data = {**source_data, TEST_CASES_KEY: cases}
    description = str(source_data.get("description", "Tokenization tests"))
    data["description"] = f"{description} (part {part_number}/{part_count})"
    return data


def _family_paths(test_dir: Path, file_name: str) -> tuple[str, list[tuple[int, Path]]]:
    match = _PART_SUFFIX_RE.fullmatch(file_name)
    requested_base = match.group("base") if match else file_name

    def numbered_parts(base: str) -> list[tuple[int, Path]]:
        pattern = re.compile(rf"^{re.escape(base)}_(?P<number>[0-9]{{2,}})\.json$")
        parts = []
        for path in test_dir.glob(f"{base}_*.json"):
            part_match = pattern.fullmatch(path.name)
            if part_match:
                parts.append((int(part_match.group("number")), path))
        return sorted(parts)

    parts = numbered_parts(requested_base)
    if parts:
        return requested_base, parts
    return file_name, numbered_parts(file_name)


def _base_description(data: dict, family: str) -> str:
    description = str(data.get("description", f"{family} tests"))
    return _PART_DESCRIPTION_RE.sub("", description)


def _planned_case_locations(planned: list[tuple[Path, dict]], new_cases: list[dict]) -> list[dict]:
    new_case_ids = {id(case) for case in new_cases}
    locations = []
    for path, data in planned:
        for case in data[TEST_CASES_KEY]:
            if id(case) in new_case_ids:
                locations.append({"id": case.get("id", ""), "input": case.get("input", ""), "file": path.stem})
    return locations


def _cross_file_collisions(project_root: Path, planned_names: set[str], skipped: set[Path]) -> list[str]:
    """Report generated GoogleTest names already used by files outside the plan."""
    problems = []
    for path in get_test_files(project_root):
        if path in skipped:
            continue
        other_cases = get_cases(load_json(path), str(path))
        for parameter_name in sorted(planned_names.intersection(_parameter_names(path, other_cases))):
            problems.append(f"GoogleTest name collides with {path.name}: {parameter_name}")
    return problems


def _apply_partition_plan(
    planned: list[tuple[Path, dict]],
    originals: dict[Path, dict],
    source_to_remove: Path | None,
) -> None:
    written: list[Path] = []
    try:
        for path, data in planned:
            save_json(path, data)
            written.append(path)
        if source_to_remove is not None:
            source_to_remove.unlink()
    except Exception:
        for path in written:
            if path in originals:
                save_json(path, originals[path])
            else:
                path.unlink(missing_ok=True)
        raise


def append_cases_partitioned(
    project_root: Path,
    file: str,
    new_cases: list[dict],
    max_cases: int = MAX_TEST_CASES_PER_FILE,
    max_bytes: int = MAX_TEST_FILE_BYTES,
    apply: bool = True,
) -> dict:
    """Append cases to a logical file family while enforcing a case limit."""
    if max_cases < 1 or max_bytes < 1:
        raise ValueError("max_cases and max_bytes must be positive")
    file_name = normalize_test_file_name(file)
    test_dir = get_test_data_dir(project_root)
    if apply:
        test_dir.mkdir(parents=True, exist_ok=True)
    family, numbered_paths = _family_paths(test_dir, file_name)
    base_path = test_dir / f"{family}.json"
    if numbered_paths and base_path.exists():
        raise ValueError(f"Ambiguous test family has both base and numbered files: {family}")

    planned: list[tuple[Path, dict]] = []
    originals: dict[Path, dict] = {}
    source_to_remove: Path | None = None

    if numbered_paths:
        if [number for number, _path in numbered_paths] != list(range(1, len(numbered_paths) + 1)):
            raise ValueError(f"Test family has non-contiguous part numbers: {family}")

        loaded: list[tuple[Path, dict]] = []
        for _number, path in numbered_paths:
            data = load_json(path)
            get_cases(data, str(path))  # rejects a part with an unsupported schema
            originals[path] = load_json(path)
            loaded.append((path, data))

        for case in new_cases:
            data = loaded[-1][1]
            candidate = [*data[TEST_CASES_KEY], case]
            if _fits_limits(data, candidate, max_cases, max_bytes):
                data[TEST_CASES_KEY] = candidate
                continue
            part_number = len(loaded) + 1
            data = {**loaded[0][1], TEST_CASES_KEY: [case]}
            if not _fits_limits(data, data[TEST_CASES_KEY], max_cases, max_bytes):
                raise _oversized_case_error(case, max_bytes)
            loaded.append((_part_path(test_dir, family, part_number, part_number), data))

        description = _base_description(loaded[0][1], family)
        for part_number, (path, data) in enumerate(loaded, start=1):
            data["description"] = f"{description} (part {part_number}/{len(loaded)})"
            planned.append((path, data))
    else:
        if base_path.exists():
            source_data = load_json(base_path)
            originals[base_path] = load_json(base_path)
        else:
            source_data = {"version": "1.0", "description": f"{family} tests", "cases": []}
        combined = [*get_cases(source_data, str(base_path)), *new_cases]
        if _fits_limits(source_data, combined, max_cases, max_bytes):
            planned.append((base_path, {**source_data, TEST_CASES_KEY: combined}))
        else:
            chunks = _limited_chunks(source_data, combined, max_cases, max_bytes)
            for part_number, chunk in enumerate(chunks, start=1):
                path = _part_path(test_dir, family, part_number, len(chunks))
                if path.exists():
                    raise ValueError(f"Destination file already exists: {path.name}")
                planned.append((path, _part_data(source_data, chunk, part_number, len(chunks))))
            source_to_remove = base_path if base_path.exists() else None

    problems = []
    planned_names: set[str] = set()
    for path, data in planned:
        cases = data[TEST_CASES_KEY]
        problems.extend(_validate_cases(path, cases))
        if len(cases) > max_cases:
            problems.append(f"{path.name} exceeds the {max_cases}-case limit")
        if _serialized_size(data) > max_bytes:
            problems.append(f"{path.name} exceeds the {max_bytes}-byte limit")
        for parameter_name in _parameter_names(path, cases):
            if parameter_name in planned_names:
                problems.append(f"Generated GoogleTest name collision: {parameter_name}")
            planned_names.add(parameter_name)
    skipped = {path for path, _data in planned} | {source_to_remove}
    problems.extend(_cross_file_collisions(project_root, planned_names, skipped))
    if problems:
        raise ValueError("Partition validation failed: " + "; ".join(problems))

    locations = _planned_case_locations(planned, new_cases)
    if len(locations) != len(new_cases):
        raise ValueError("Partition plan did not place every new test case")
    if apply:
        _apply_partition_plan(planned, originals, source_to_remove)

    return {
        "requested_file": file_name,
        "family": family,
        "files": [{"file": path.stem, "cases": len(data[TEST_CASES_KEY])} for path, data in planned],
        "case_locations": locations,
        "split": len(planned) > 1 or source_to_remove is not None,
        "applied": apply,
    }


@mcp.tool()
async def test_audit_layout(
    max_cases: int = MAX_TEST_CASES_PER_FILE,
    max_bytes: int = MAX_TEST_FILE_BYTES,
) -> str:
    """Report oversized tokenization test files without modifying them.

    Args:
        max_cases: Maximum preferred cases per file.
        max_bytes: Maximum preferred serialized file size in bytes.
    """
    if max_cases < 1 or max_bytes < 1:
        return _json_error("max_cases and max_bytes must be positive")

    loaded, error = _load_test_cases(get_test_files(PROJECT_ROOT))
    if error:
        return _json_error(error)

    files = []
    total_cases = 0
    for path, _data, cases in loaded:
        case_count = len(cases)
        byte_count = path.stat().st_size
        total_cases += case_count
        files.append(
            {
                "file": path.stem,
                "cases": case_count,
                "bytes": byte_count,
                "over_case_limit": case_count > max_cases,
                "over_byte_limit": byte_count > max_bytes,
            }
        )

    oversized = [item for item in files if item["over_case_limit"] or item["over_byte_limit"]]
    oversized.sort(key=lambda item: (item["bytes"], item["cases"]), reverse=True)
    return _json_result(
        {
            "status": "ok",
            "limits": {"max_cases": max_cases, "max_bytes": max_bytes},
            "summary": {
                "files": len(files),
                "cases": total_cases,
                "oversized_files": len(oversized),
            },
            "oversized": oversized,
        }
    )


@mcp.tool()
async def test_split_file(
    file: str,
    max_cases: int = 100,
    apply: bool = False,
) -> str:
    """Split one test file into balanced, stable numbered parts.

    Every valid case object and its supported metadata is preserved. The source
    file is removed only after all destination files have been written successfully.

    Args:
        file: Source test file basename, without .json.
        max_cases: Maximum cases in each generated part.
        apply: If True, apply the split. Default is dry-run.
    """
    if max_cases < 1:
        return _json_error("max_cases must be positive")
    try:
        file_name = normalize_test_file_name(file)
    except ValueError as exc:
        return _json_error(str(exc))

    test_dir = get_test_data_dir(PROJECT_ROOT)
    source_path = test_dir / f"{file_name}.json"
    if not source_path.exists():
        return _json_error(f"Test file not found: {file_name}")

    try:
        source_data = load_json(source_path)
    except Exception as exc:
        return _json_error(f"Failed to parse JSON file {source_path}: {exc}")
    cases = get_cases(source_data, str(source_path))
    if len(cases) <= max_cases:
        return _json_result(
            {
                "status": "ok",
                "source": file_name,
                "source_cases": len(cases),
                "parts": [],
                "applied": False,
                "message": "File is already within the case limit",
            }
        )

    chunks = _balanced_chunks(cases, max_cases)
    destinations = [
        (
            _part_path(test_dir, file_name, part_index, len(chunks)),
            _part_data(source_data, chunk, part_index, len(chunks)),
        )
        for part_index, chunk in enumerate(chunks, start=1)
    ]

    existing = [path.name for path, _data in destinations if path.exists()]
    if existing:
        return _json_error(f"Destination files already exist: {', '.join(existing)}")

    problems = _validate_cases(source_path, cases)
    planned_names: set[str] = set()
    for path, data in destinations:
        part_cases = data[TEST_CASES_KEY]
        problems.extend(_validate_cases(path, part_cases))
        for parameter_name in _parameter_names(path, part_cases):
            if parameter_name in planned_names:
                problems.append(f"Generated GoogleTest name collision: {parameter_name}")
            planned_names.add(parameter_name)
    problems.extend(_cross_file_collisions(PROJECT_ROOT, planned_names, {source_path}))

    source_digest = _case_digest(cases)
    combined_cases = [case for _path, data in destinations for case in data[TEST_CASES_KEY]]
    if len(combined_cases) != len(cases):
        problems.append("Generated parts do not preserve the source case count")
    if _case_digest(combined_cases) != source_digest:
        problems.append("Generated parts do not preserve source case content and order")
    if problems:
        return _json_error("Split validation failed", problems=problems)

    parts = [
        {"file": path.stem, "cases": len(data[TEST_CASES_KEY]), "bytes": _serialized_size(data)}
        for path, data in destinations
    ]
    result = {
        "status": "ok",
        "source": file_name,
        "source_cases": len(cases),
        "source_bytes": source_path.stat().st_size,
        "case_digest": source_digest,
        "parts": parts,
        "applied": apply,
    }
    if not apply:
        return _json_result(result)

    try:
        _apply_partition_plan(destinations, {}, source_path)
    except Exception as exc:
        return _json_error(f"Failed to apply split; generated files were rolled back: {exc}")

    return _json_result(result)
