"""Test file utilities ported from TestFileUtils.pm."""

import json
from pathlib import Path


def get_test_data_dir(project_root: Path) -> Path:
    """Get the test data directory."""
    return project_root / "tests" / "data" / "tokenization"


def get_test_files(project_root: Path) -> list[Path]:
    """Get all test JSON files sorted by name."""
    test_dir = get_test_data_dir(project_root)
    return sorted(test_dir.glob("*.json"))


def load_json(path: Path) -> dict:
    """Load and parse a JSON file."""
    return json.loads(path.read_bytes())


def save_json(path: Path, data: dict) -> None:
    """Save data as JSON with consistent formatting."""
    content = json.dumps(data, ensure_ascii=False, indent=2, sort_keys=True)
    path.write_text(content + "\n", encoding="utf-8")


def find_test_by_input(project_root: Path, input_text: str) -> dict | None:
    """Find a test case by input text across all test files.

    Returns:
        Dict with keys: file, basename, index, case, data; or None.
    """
    for path in get_test_files(project_root):
        try:
            data = load_json(path)
        except Exception:
            continue

        cases = data.get("cases") or data.get("test_cases") or []
        for i, case in enumerate(cases):
            if case.get("input") == input_text:
                basename = path.stem
                return {
                    "file": path,
                    "basename": basename,
                    "index": i,
                    "case": case,
                    "data": data,
                }
    return None


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

    try:
        data = load_json(path)
    except Exception:
        return None

    cases = data.get("cases") or data.get("test_cases") or []

    # Try numeric index first
    if idx.isdigit():
        i = int(idx)
        if i < len(cases):
            return {
                "file": path,
                "basename": basename,
                "index": i,
                "case": cases[i],
                "data": data,
            }

    # Try matching by case id
    for i, case in enumerate(cases):
        if case.get("id", "") == idx:
            return {
                "file": path,
                "basename": basename,
                "index": i,
                "case": case,
                "data": data,
            }

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
        import re

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
    import re

    result = re.sub(r"[^\x00-\x7F]+", "_", result)
    result = re.sub(r"\s+", "_", result)
    result = re.sub(r"_+", "_", result)
    result = result.strip("_")
    result = result.lower()

    return result or "unnamed"
