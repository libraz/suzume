"""Tests for tokenization test organization tools."""

import json
from importlib import import_module

from suzume_mcp.tools import test_tools as _test_tools  # noqa: F401
from tests.helpers import CORPUS_DIR, parse, run, write_suite

organization = import_module("suzume_mcp.tools._test_tools_organization")


def make_case(index: int, **extra) -> dict:
    case = {
        "id": f"case_{index}",
        "description": f"case {index}",
        "input": f"テスト{index}",
        "expected": [{"surface": "テスト", "pos": "Noun", "lemma": "テスト"}],
    }
    case.update(extra)
    return case


def test_audit_layout_reports_only_oversized_files(tmp_path, monkeypatch):
    write_suite(tmp_path, "small", [make_case(1)])
    write_suite(tmp_path, "large", [make_case(index) for index in range(3)])
    monkeypatch.setattr(organization, "PROJECT_ROOT", tmp_path)

    data = parse(run(organization.test_audit_layout(max_cases=2, max_bytes=1_000_000)))

    assert data["summary"] == {"files": 2, "cases": 4, "oversized_files": 1, "ambiguous_families": 0}
    assert [item["file"] for item in data["oversized"]] == ["large"]


def test_audit_layout_reports_a_base_file_beside_numbered_parts(tmp_path, monkeypatch):
    write_suite(tmp_path, "large", [make_case(1)])
    write_suite(tmp_path, "large_01", [make_case(2)])
    write_suite(tmp_path, "other_01", [make_case(3)])
    monkeypatch.setattr(organization, "PROJECT_ROOT", tmp_path)

    data = parse(run(organization.test_audit_layout()))

    assert data["ambiguous_families"] == ["large"]


def test_split_file_dry_run_does_not_write(tmp_path, monkeypatch):
    cases = [make_case(index) for index in range(5)]
    write_suite(tmp_path, "large", cases)
    monkeypatch.setattr(organization, "PROJECT_ROOT", tmp_path)

    data = parse(run(organization.test_split_file("large", max_cases=2)))

    test_dir = tmp_path / CORPUS_DIR
    assert data["applied"] is False
    assert [part["cases"] for part in data["parts"]] == [2, 2, 1]
    assert (test_dir / "large.json").exists()
    assert not list(test_dir.glob("large_*.json"))


def test_split_file_preserves_complete_cases_and_order(tmp_path, monkeypatch):
    cases = [
        make_case(1, tags=["regression"], accepted_diff={"reason": "intentional"}),
        make_case(2, suzume_expected=[{"surface": "テスト", "pos": "Noun", "lemma": "テスト"}]),
        make_case(3),
    ]
    write_suite(tmp_path, "large", cases)
    monkeypatch.setattr(organization, "PROJECT_ROOT", tmp_path)

    data = parse(run(organization.test_split_file("large", max_cases=2, apply=True)))

    test_dir = tmp_path / CORPUS_DIR
    assert data["applied"] is True
    assert not (test_dir / "large.json").exists()
    generated_cases = []
    for path in sorted(test_dir.glob("large_*.json")):
        generated_cases.extend(json.loads(path.read_text(encoding="utf-8"))["cases"])
    assert generated_cases == cases


def test_split_file_folds_parts_beside_the_base_into_new_parts(tmp_path, monkeypatch):
    cases = [make_case(index) for index in range(3)]
    write_suite(tmp_path, "large", cases)
    write_suite(tmp_path, "large_01", [make_case(10)])
    monkeypatch.setattr(organization, "PROJECT_ROOT", tmp_path)

    data = parse(run(organization.test_split_file("large", max_cases=2, apply=True)))

    test_dir = tmp_path / CORPUS_DIR
    assert data["folded"] == ["large_01"]
    assert not (test_dir / "large.json").exists()
    generated = [json.loads(path.read_text(encoding="utf-8")) for path in sorted(test_dir.glob("large_*.json"))]
    assert [case for part in generated for case in part["cases"]] == [*cases, make_case(10)]
    assert [part["description"] for part in generated] == ["large tests (part 1/2)", "large tests (part 2/2)"]


def test_split_file_folds_a_small_family_into_the_base(tmp_path, monkeypatch):
    write_suite(tmp_path, "large", [make_case(1)])
    write_suite(tmp_path, "large_01", [make_case(2)])
    monkeypatch.setattr(organization, "PROJECT_ROOT", tmp_path)

    data = parse(run(organization.test_split_file("large", max_cases=100, apply=True)))

    test_dir = tmp_path / CORPUS_DIR
    assert data["parts"] == [{"file": "large", "cases": 2, "bytes": data["parts"][0]["bytes"]}]
    assert not (test_dir / "large_01.json").exists()
    assert json.loads((test_dir / "large.json").read_text(encoding="utf-8"))["cases"] == [make_case(1), make_case(2)]


def test_partitioned_append_to_a_part_name_uses_the_existing_base(tmp_path):
    write_suite(tmp_path, "large", [make_case(1)])

    result = organization.append_cases_partitioned(tmp_path, "large_01", [make_case(2)])

    test_dir = tmp_path / CORPUS_DIR
    assert result["case_locations"][0]["file"] == "large"
    assert not (test_dir / "large_01.json").exists()


def test_split_file_rejects_sanitized_id_collision(tmp_path, monkeypatch):
    first = make_case(1)
    first["id"] = "same-id"
    second = make_case(2)
    second["id"] = "same id"
    write_suite(tmp_path, "large", [first, second, make_case(3)])
    monkeypatch.setattr(organization, "PROJECT_ROOT", tmp_path)

    data = parse(run(organization.test_split_file("large", max_cases=2)))

    assert data["status"] == "error"
    assert any("duplicates GoogleTest name" in problem for problem in data["problems"])


def test_partitioned_append_uses_available_space_in_last_part(tmp_path):
    first_cases = [make_case(1), make_case(2)]
    second_cases = [make_case(3)]
    write_suite(tmp_path, "large_01", first_cases)
    write_suite(tmp_path, "large_02", second_cases)
    new_case = make_case(4)

    result = organization.append_cases_partitioned(tmp_path, "large", [new_case], max_cases=2)

    test_dir = tmp_path / CORPUS_DIR
    assert result["case_locations"] == [{"id": "case_4", "input": "テスト4", "file": "large_02"}]
    assert len(json.loads((test_dir / "large_02.json").read_text(encoding="utf-8"))["cases"]) == 2
    assert not (test_dir / "large_03.json").exists()


def test_partitioned_append_creates_next_part_and_updates_descriptions(tmp_path):
    write_suite(tmp_path, "large_01", [make_case(1), make_case(2)])
    write_suite(tmp_path, "large_02", [make_case(3), make_case(4)])

    result = organization.append_cases_partitioned(tmp_path, "large_02", [make_case(5)], max_cases=2)

    test_dir = tmp_path / CORPUS_DIR
    assert result["case_locations"][0]["file"] == "large_03"
    for part_number in range(1, 4):
        data = json.loads((test_dir / f"large_{part_number:02}.json").read_text(encoding="utf-8"))
        assert data["description"].endswith(f"(part {part_number}/3)")


def test_partitioned_append_splits_full_base_file(tmp_path):
    original_cases = [make_case(1), make_case(2)]
    write_suite(tmp_path, "large", original_cases)
    new_case = make_case(3)

    result = organization.append_cases_partitioned(tmp_path, "large", [new_case], max_cases=2)

    test_dir = tmp_path / CORPUS_DIR
    assert result["split"] is True
    assert not (test_dir / "large.json").exists()
    generated_cases = []
    for path in sorted(test_dir.glob("large_*.json")):
        generated_cases.extend(json.loads(path.read_text(encoding="utf-8"))["cases"])
    assert generated_cases == [*original_cases, new_case]


def test_partitioned_append_dry_run_does_not_split_base_file(tmp_path):
    write_suite(tmp_path, "large", [make_case(1), make_case(2)])

    result = organization.append_cases_partitioned(tmp_path, "large", [make_case(3)], max_cases=2, apply=False)

    test_dir = tmp_path / CORPUS_DIR
    assert result["applied"] is False
    assert (test_dir / "large.json").exists()
    assert not list(test_dir.glob("large_*.json"))


def test_partitioned_append_splits_on_byte_limit(tmp_path):
    first = make_case(1, description="a" * 800)
    second = make_case(2, description="b" * 800)
    write_suite(tmp_path, "large", [first])

    result = organization.append_cases_partitioned(
        tmp_path,
        "large",
        [second],
        max_cases=100,
        max_bytes=1400,
    )

    test_dir = tmp_path / CORPUS_DIR
    assert result["split"] is True
    assert [item["cases"] for item in result["files"]] == [1, 1]
    assert all(path.stat().st_size <= 1400 for path in test_dir.glob("large_*.json"))
