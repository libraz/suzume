"""Tests for test file utilities."""

import json

from suzume_mcp.core.test_file_utils import (
    cases_key,
    find_test_by_id,
    find_tests_by_input,
    generate_id,
    get_failures_from_test_output,
    get_test_files,
    load_json,
    normalize_test_file_name,
    save_json,
)
from tests.helpers import CORPUS_DIR


def test_cases_key_accepts_only_cpp_loader_schema():
    assert cases_key({"cases": []}) == "cases"

    for invalid in ({"test_cases": []}, {}, {"cases": {}}):
        try:
            cases_key(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError("Expected a non-canonical test schema to be rejected")


def test_generate_id_hiragana():
    assert generate_id("たべる") == "taberu"


def test_generate_id_mixed():
    # 食 is kanji (becomes _), べている is hiragana
    assert generate_id("食べている") == "beteiru"


def test_generate_id_empty():
    assert generate_id("") == "unnamed"


def test_generate_id_ascii():
    assert generate_id("hello world") == "hello_world"


def test_generate_id_kanji():
    # Kanji should become underscores
    result = generate_id("日本語")
    assert "_" in result or result.isascii()


def test_normalize_test_file_name_strips_repeated_suffixes():
    assert normalize_test_file_name("basic.json.json") == "basic"


def test_normalize_test_file_name_rejects_path():
    try:
        normalize_test_file_name("nested/basic.json")
    except ValueError as exc:
        assert "not a path" in str(exc)
    else:
        raise AssertionError("Expected invalid test filename to be rejected")


def test_load_save_json(tmp_path):
    data = {"cases": [{"input": "テスト", "expected": []}]}
    path = tmp_path / "test.json"
    save_json(path, data)
    loaded = load_json(path)
    assert loaded == data


def test_save_json_format(tmp_path):
    data = {"b": 2, "a": 1}
    path = tmp_path / "test.json"
    save_json(path, data)
    content = path.read_text()
    # Should be sorted keys and have trailing newline
    assert content.endswith("\n")
    assert content.index('"a"') < content.index('"b"')


def test_find_tests_by_input(tmp_path):
    test_dir = tmp_path / CORPUS_DIR
    test_dir.mkdir(parents=True)
    data = {"cases": [{"input": "テスト", "id": "test1"}]}
    (test_dir / "basic.json").write_text(json.dumps(data), encoding="utf-8")

    [result] = find_tests_by_input(tmp_path, "テスト")
    assert result["basename"] == "basic"
    assert result["index"] == 0
    assert result["case"]["input"] == "テスト"


def test_find_tests_by_input_returns_every_match(tmp_path):
    test_dir = tmp_path / CORPUS_DIR
    test_dir.mkdir(parents=True)
    for name in ("first", "second"):
        data = {"cases": [{"input": "テスト", "id": name}]}
        (test_dir / f"{name}.json").write_text(json.dumps(data), encoding="utf-8")

    matches = find_tests_by_input(tmp_path, "テスト")
    assert sorted(match["basename"] for match in matches) == ["first", "second"]


def test_find_tests_by_input_not_found(tmp_path):
    test_dir = tmp_path / CORPUS_DIR
    test_dir.mkdir(parents=True)
    data = {"cases": [{"input": "テスト"}]}
    (test_dir / "basic.json").write_text(json.dumps(data), encoding="utf-8")

    assert find_tests_by_input(tmp_path, "不存在") == []


def test_find_test_by_id_numeric(tmp_path):
    test_dir = tmp_path / CORPUS_DIR
    test_dir.mkdir(parents=True)
    data = {"cases": [{"input": "a"}, {"input": "b"}]}
    (test_dir / "basic.json").write_text(json.dumps(data), encoding="utf-8")

    result = find_test_by_id(tmp_path, "basic/1")
    assert result is not None
    assert result["case"]["input"] == "b"


def test_find_test_by_id_string(tmp_path):
    test_dir = tmp_path / CORPUS_DIR
    test_dir.mkdir(parents=True)
    data = {"cases": [{"input": "a", "id": "first"}, {"input": "b", "id": "second"}]}
    (test_dir / "basic.json").write_text(json.dumps(data), encoding="utf-8")

    result = find_test_by_id(tmp_path, "basic/second")
    assert result is not None
    assert result["case"]["input"] == "b"


def test_get_test_files(tmp_path):
    test_dir = tmp_path / CORPUS_DIR
    test_dir.mkdir(parents=True)
    (test_dir / "b.json").write_text("{}")
    (test_dir / "a.json").write_text("{}")
    (test_dir / "c.txt").write_text("")

    files = get_test_files(tmp_path)
    assert len(files) == 2
    assert files[0].name == "a.json"
    assert files[1].name == "b.json"


def test_get_failures_from_test_output(tmp_path):
    output_file = tmp_path / "test.txt"
    output_file.write_text(
        "Input: 食べています\n"
        "[  FAILED  ] Tokenize/Verb_ichidan.Test (0 ms) GetParam() = verb_ichidan/tabeteiru\n"
        "Input: 走った\n"
        "[  FAILED  ] Tokenize/Verb_godan.Test (0 ms) GetParam() = verb_godan/hashitta\n",
        encoding="utf-8",
    )

    failures = get_failures_from_test_output(str(output_file))
    assert len(failures) == 2
    assert failures[0]["input"] == "食べています"
    assert failures[0]["id"] == "verb_ichidan/tabeteiru"
    assert failures[1]["input"] == "走った"
