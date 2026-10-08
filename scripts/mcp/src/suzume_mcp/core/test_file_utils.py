"""Test file utilities ported from TestFileUtils.pm."""

import json
import re
from collections.abc import Iterator
from pathlib import Path

from .file_utils import atomic_write_text

TEST_CASES_KEY = "cases"


def cases_key(data: dict, source: str = "test data") -> str:
    """Return the canonical test-case key, rejecting schemas C++ cannot load."""
    if "test_cases" in data:
        raise ValueError(f"{source} uses unsupported 'test_cases'; rename it to '{TEST_CASES_KEY}'")
    if TEST_CASES_KEY not in data:
        raise ValueError(f"{source} is missing required '{TEST_CASES_KEY}' array")
    if not isinstance(data[TEST_CASES_KEY], list):
        raise ValueError(f"{source}.{TEST_CASES_KEY} must be an array")
    return TEST_CASES_KEY


def get_cases(data: dict, source: str = "test data") -> list[dict]:
    """Return the canonical case array after validating its schema."""
    return data[cases_key(data, source)]


def get_test_data_dir(project_root: Path) -> Path:
    """Get the test data directory."""
    return project_root / "tests" / "data" / "tokenization"


def normalize_test_file_name(file: str) -> str:
    """Return a safe test-file basename, without one or more .json suffixes."""
    name = file.strip()
    if not name or Path(name).name != name:
        raise ValueError("Test file must be a non-empty filename, not a path")

    while name.lower().endswith(".json"):
        name = name[:-5]
    if not name:
        raise ValueError("Test file basename cannot be empty")
    return name


def get_test_files(project_root: Path) -> list[Path]:
    """Get all test JSON files sorted by name."""
    test_dir = get_test_data_dir(project_root)
    return sorted(test_dir.glob("*.json"))


def load_json(path: Path) -> dict:
    """Load and parse a JSON file."""
    return json.loads(path.read_bytes())


def serialize_test_data(data: dict) -> str:
    """Serialize test data using the canonical on-disk JSON format."""
    return json.dumps(data, ensure_ascii=False, indent=2, sort_keys=True) + "\n"


def save_json(path: Path, data: dict) -> None:
    """Save data as JSON with consistent formatting."""
    atomic_write_text(path, serialize_test_data(data))


def _load_cases(path: Path) -> tuple[dict, list[dict]]:
    try:
        data = load_json(path)
    except Exception as exc:
        raise RuntimeError(f"Failed to parse JSON file: {path}") from exc
    return data, get_cases(data, str(path))


def _case_record(path: Path, basename: str, index: int, case: dict, data: dict) -> dict:
    return {"file": path, "basename": basename, "index": index, "case": case, "data": data}


def _iter_cases_by_input(project_root: Path, input_text: str) -> Iterator[dict]:
    """Yield every case with the given input, loading files lazily in name order."""
    for path in get_test_files(project_root):
        data, cases = _load_cases(path)
        for index, case in enumerate(cases):
            if case.get("input") == input_text:
                yield _case_record(path, path.stem, index, case, data)


def find_test_by_input(project_root: Path, input_text: str) -> dict | None:
    """Find a test case by input text across all test files.

    Returns:
        Dict with keys: file, basename, index, case, data; or None.
    """
    return next(_iter_cases_by_input(project_root, input_text), None)


def find_tests_by_input(project_root: Path, input_text: str) -> list[dict]:
    """Find every test case matching an input across all test files."""
    return list(_iter_cases_by_input(project_root, input_text))


def find_test_by_id(project_root: Path, test_id: str) -> dict | None:
    """Find a test case by ID (format: basename/index or basename/id_string).

    Returns:
        Dict with keys: file, basename, index, case, data; or None.
    """
    parts = test_id.split("/", 1)
    if len(parts) != 2:
        return None
    basename, idx = parts

    path = get_test_data_dir(project_root) / f"{basename.lower()}.json"
    if not path.exists():
        return None

    data, cases = _load_cases(path)

    # Try numeric index first
    if idx.isdigit() and int(idx) < len(cases):
        return _case_record(path, basename, int(idx), cases[int(idx)], data)

    # Try matching by case id
    for index, case in enumerate(cases):
        if case.get("id", "") == idx:
            return _case_record(path, basename, index, case, data)

    return None


def get_failures_from_test_output(test_output_file: str = "/tmp/test.txt") -> list[dict]:
    """Parse test output file for failures.

    Returns:
        List of dicts with keys: id, file, case_id, input.
    """
    path = Path(test_output_file)
    if not path.exists():
        return []

    failures = []
    current_input = ""

    for line in path.read_text(encoding="utf-8").splitlines():
        m_input = re.search(r"Input:\s*(.+)", line)
        if m_input:
            current_input = m_input.group(1)
            continue

        m_failed = re.search(r"FAILED.*GetParam\(\)\s*=\s*(\S+)/(\S+)", line)
        if m_failed and current_input:
            file_part = m_failed.group(1)
            case_id = m_failed.group(2)
            failures.append(
                {
                    "id": f"{file_part}/{case_id}",
                    "file": file_part,
                    "case_id": case_id,
                    "input": current_input,
                }
            )
            current_input = ""

    return failures


def generate_id(input_text: str) -> str:
    """Generate a test case ID from Japanese input text."""
    char_map = {
        "あ": "a",
        "い": "i",
        "う": "u",
        "え": "e",
        "お": "o",
        "か": "ka",
        "き": "ki",
        "く": "ku",
        "け": "ke",
        "こ": "ko",
        "さ": "sa",
        "し": "shi",
        "す": "su",
        "せ": "se",
        "そ": "so",
        "た": "ta",
        "ち": "chi",
        "つ": "tsu",
        "て": "te",
        "と": "to",
        "な": "na",
        "に": "ni",
        "ぬ": "nu",
        "ね": "ne",
        "の": "no",
        "は": "ha",
        "ひ": "hi",
        "ふ": "fu",
        "へ": "he",
        "ほ": "ho",
        "ま": "ma",
        "み": "mi",
        "む": "mu",
        "め": "me",
        "も": "mo",
        "や": "ya",
        "ゆ": "yu",
        "よ": "yo",
        "ら": "ra",
        "り": "ri",
        "る": "ru",
        "れ": "re",
        "ろ": "ro",
        "わ": "wa",
        "を": "wo",
        "ん": "n",
        "が": "ga",
        "ぎ": "gi",
        "ぐ": "gu",
        "げ": "ge",
        "ご": "go",
        "ざ": "za",
        "じ": "ji",
        "ず": "zu",
        "ぜ": "ze",
        "ぞ": "zo",
        "だ": "da",
        "ぢ": "di",
        "づ": "du",
        "で": "de",
        "ど": "do",
        "ば": "ba",
        "び": "bi",
        "ぶ": "bu",
        "べ": "be",
        "ぼ": "bo",
        "ぱ": "pa",
        "ぴ": "pi",
        "ぷ": "pu",
        "ぺ": "pe",
        "ぽ": "po",
        "っ": "tt",
        "ー": "_",
    }

    result = input_text
    for char, romaji in char_map.items():
        result = result.replace(char, romaji)

    # Replace remaining non-ASCII with underscore
    result = re.sub(r"[^\x00-\x7F]+", "_", result)
    result = re.sub(r"\s+", "_", result)
    result = re.sub(r"_+", "_", result)
    result = result.strip("_")
    result = result.lower()

    return result or "unnamed"
