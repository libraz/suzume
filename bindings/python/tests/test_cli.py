"""End-to-end contract tests for the installed Python console command."""

from __future__ import annotations

import json
import os
import subprocess
import sysconfig
from importlib import resources
from io import StringIO
from pathlib import Path
from typing import TypedDict, cast

import pytest

import suzume
from suzume import cli

_JSON_MORPHEME_KEYS = [
    "surface",
    "pos",
    "lemma",
    "start",
    "end",
    "extended_pos",
    "is_user_dict",
    "is_formal_noun",
    "is_low_info",
    "is_unknown",
    "is_from_dictionary",
    "score",
]


class _JsonPayload(TypedDict):
    input: str
    normalized_text: str
    morphemes: list[dict[str, object]]


def _console_script() -> str:
    executable = "suzume.exe" if os.name == "nt" else "suzume"
    script = Path(sysconfig.get_path("scripts")) / executable
    assert script.is_file(), "the active Python environment has no suzume console script"
    return str(script)


def _bundled_user_dictionary() -> bytes:
    return resources.files("suzume").joinpath("user.dic").read_bytes()


def _run_cli(
    *args: str,
    input_text: str | None = None,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [_console_script(), *args],
        input=input_text,
        text=True,
        capture_output=True,
        check=False,
    )


def _run_cli_bytes(*args: str, input_bytes: bytes) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        [_console_script(), *args],
        input=input_bytes,
        capture_output=True,
        check=False,
    )


def _json_result(*args: str, input_text: str | None = None) -> _JsonPayload:
    result = _run_cli("--format", "json", *args, input_text=input_text)
    assert result.returncode == 0, result.stderr
    return cast(_JsonPayload, json.loads(result.stdout))


def test_console_script_comes_from_active_environment() -> None:
    script = Path(_console_script()).resolve()
    scripts_dir = Path(sysconfig.get_path("scripts")).resolve()
    assert script.parent == scripts_dir


@pytest.mark.parametrize("alias", [["--version"], ["-v"], ["version"]])
def test_version_aliases(alias: list[str]) -> None:
    result = _run_cli(*alias)
    assert result.returncode == 0
    assert result.stdout.strip() == f"suzume {suzume.version()}"


@pytest.mark.parametrize(
    "args",
    [
        ["version", "extra"],
        ["version", "--unknown"],
        ["--version", "extra"],
        ["-v", "extra"],
    ],
)
def test_version_aliases_reject_every_extra_argument(args: list[str]) -> None:
    result = _run_cli(*args)
    assert result.returncode == 2
    assert result.stdout == ""
    assert "version accepts no other arguments" in result.stderr


def test_top_level_help_contains_analysis_options() -> None:
    result = _run_cli("--help")
    assert result.returncode == 0
    for option in (
        "--dict",
        "--mode",
        "--format",
        "--normalize-vu",
        "--lowercase",
        "--preserve-symbols",
        "--no-lemmatize",
        "--merge-compounds",
        "--no-core-dict",
        "--no-user-dict",
        "--tag-pos",
        "--tag-min-length",
        "--tag-max-tags",
    ):
        assert option in result.stdout


def test_implicit_and_explicit_analyze_have_exact_same_input() -> None:
    implicit = _json_result("東京", "公園")
    command_first = _json_result("analyze", "東京", "公園")
    command_after_options = _run_cli(
        "--mode", "search", "analyze", "--format", "json", "東京", "公園"
    )
    assert command_after_options.returncode == 0, command_after_options.stderr

    after_options_payload = json.loads(command_after_options.stdout)
    assert implicit["input"] == "東京 公園"
    assert command_first["input"] == "東京 公園"
    assert after_options_payload["input"] == "東京 公園"
    assert command_first["morphemes"] == implicit["morphemes"]


def test_double_dash_preserves_analyze_and_option_text_literally() -> None:
    payload = _json_result("--", "analyze", "--format")
    assert payload["input"] == "analyze --format"

    explicit = _json_result("analyze", "--", "analyze")
    assert explicit["input"] == "analyze"


def test_json_output_matches_native_schema_exactly() -> None:
    result = _run_cli("analyze", "--format", "json", "東京の公園")
    assert result.returncode == 0
    payload = json.loads(result.stdout)
    assert list(payload) == ["input", "normalized_text", "morphemes"]
    assert payload["input"] == "東京の公園"
    assert payload["normalized_text"] == "東京の公園"
    assert payload["morphemes"]
    assert list(payload["morphemes"][0]) == _JSON_MORPHEME_KEYS
    assert "\\u6771" not in result.stdout


def test_json_scores_use_native_float_max_digits10_precision() -> None:
    morpheme = suzume.Morpheme(
        surface="東京",
        pos="NOUN",
        base_form="東京",
        pos_ja="名詞",
        extended_pos="NOUN",
        start=0,
        end=2,
        is_user_dict=False,
        is_formal_noun=False,
        is_low_info=False,
        is_unknown=True,
        is_from_dictionary=False,
        score=0.20000000298023224,
    )
    assert cli._morpheme_dict(morpheme)["score"] == 0.200000003


@pytest.mark.parametrize(
    ("stdin_text", "expected"),
    [
        ("東京", "東京"),
        ("東京\n", "東京"),
        ("東京\n公園\n", "東京\n公園"),
        ("東京\n\n", "東京\n"),
        ("\ufeff東京\n", "東京"),
        ("東京\ufeff公園\n", "東京\ufeff公園"),
    ],
)
def test_stdin_normalization_matches_native(stdin_text: str, expected: str) -> None:
    payload = _json_result(input_text=stdin_text)
    assert payload["input"] == expected


@pytest.mark.parametrize(
    ("argument_text", "expected"),
    [
        ("\ufeff東京", "東京"),
        ("東京\n", "東京"),
        ("東京\n公園\n", "東京\n公園"),
        ("東京\n\n", "東京\n"),
    ],
)
def test_argument_normalization_matches_stdin(argument_text: str, expected: str) -> None:
    argument_payload = _json_result(argument_text)
    stdin_payload = _json_result(input_text=argument_text)
    assert argument_payload["input"] == stdin_payload["input"] == expected


def test_crlf_stdin_normalization() -> None:
    assert cli._read_text([], StringIO("\ufeff東京\r\n公園\r\n")) == "東京\r\n公園"


def test_interactive_stdin_is_never_read() -> None:
    class InteractiveInput(StringIO):
        def isatty(self) -> bool:
            return True

        def read(self, size: int | None = -1) -> str:
            raise AssertionError("interactive stdin must not be read")

    assert cli._read_text([], InteractiveInput()) == ""


@pytest.mark.parametrize("mode", ["normal", "search", "split"])
def test_all_analysis_modes_parse(mode: str) -> None:
    parser = cli._build_parser()
    args = parser.parse_args(cli._without_explicit_command(["--mode", mode, "analyze", "東京"]))
    assert args.mode == mode
    assert args.text == ["東京"]


def test_normalization_lemmatize_compound_and_tag_options_are_wired() -> None:
    parser = cli._build_parser()
    args = parser.parse_args(
        cli._without_explicit_command(
            [
                "--normalize-vu",
                "--lowercase",
                "--preserve-symbols",
                "--no-lemmatize",
                "--merge-compounds",
                "--no-core-dict",
                "--no-user-dict",
                "--tag-pos",
                "noun",
                "--tag-pos",
                "verb",
                "--tag-exclude-basic",
                "--tag-use-surface",
                "--tag-min-length",
                "1",
                "--tag-max-tags",
                "3",
                "--include-particles",
                "--include-auxiliaries",
                "--include-formal-nouns",
                "--include-low-info",
                "--tag-keep-duplicates",
                "analyze",
                "東京",
            ]
        )
    )
    assert args.normalize_vu
    assert args.lowercase
    assert args.preserve_symbols
    assert args.no_lemmatize
    assert args.merge_compounds
    assert args.no_core_dict
    assert args.no_user_dict
    assert args.tag_pos == ["noun", "verb"]
    assert args.tag_exclude_basic
    assert args.tag_use_surface
    assert args.tag_min_length == 1
    assert args.tag_max_tags == 3
    assert args.include_particles
    assert args.include_auxiliaries
    assert args.include_formal_nouns
    assert args.include_low_info
    assert args.tag_keep_duplicates
    assert args.text == ["東京"]


def test_normalization_and_symbol_options_change_analysis() -> None:
    preserved = _json_result("--preserve-symbols", "TEST。")
    normalized = _json_result("--lowercase", "--preserve-symbols", "TEST。")
    assert preserved["input"] == normalized["input"] == "TEST。"
    assert preserved["morphemes"] != normalized["morphemes"]
    assert any(morpheme["surface"] == "。" for morpheme in preserved["morphemes"])


@pytest.mark.parametrize(
    ("input_text", "expected_normalized"),
    [("ﾊﾞｽに乗る", "バスに乗る"), ("ＡＢＣ１２３", "ABC123")],
)
def test_json_offsets_slice_normalized_text(input_text: str, expected_normalized: str) -> None:
    payload = _json_result(input_text)
    assert payload["input"] == input_text
    assert payload["normalized_text"] == expected_normalized
    for morpheme in payload["morphemes"]:
        start = cast(int, morpheme["start"])
        end = cast(int, morpheme["end"])
        assert payload["normalized_text"][start:end] == morpheme["surface"]


def test_tag_and_chasen_outputs() -> None:
    tags = _run_cli(
        "--format",
        "tags",
        "--tag-pos",
        "noun",
        "--tag-min-length",
        "1",
        "東京の公園",
    )
    assert tags.returncode == 0
    assert "\tNOUN" in tags.stdout

    chasen = _run_cli("--format", "chasen", "りんごを食べる")
    assert chasen.returncode == 0
    assert chasen.stdout == (
        "りんご\t*\tりんご\t名詞\t*\t*\n"
        "を\t*\tを\t助詞\t*\t*\n"
        "食べる\t*\t食べる\t動詞\t一段\t終止形\n"
        "EOS\n"
    )


def test_tab_output_escapes_record_delimiters() -> None:
    assert cli._tab_escape("a\\b\tc\r\nd") == "a\\\\b\\tc\\r\\nd"


@pytest.mark.parametrize("output_format", ["tsv", "chasen"])
def test_console_tab_outputs_escape_embedded_delimiters(output_format: str) -> None:
    result = _run_cli("--preserve-symbols", "--format", output_format, "東京\t大阪")
    assert result.returncode == 0, result.stderr
    assert "\\t\t" in result.stdout
    assert "\n\t" not in result.stdout


def test_tag_max_tags_zero_is_unlimited() -> None:
    unlimited = _run_cli(
        "--format",
        "tags",
        "--tag-min-length",
        "1",
        "--tag-max-tags",
        "0",
        "東京の公園でりんごを食べる",
    )
    limited = _run_cli(
        "--format",
        "tags",
        "--tag-min-length",
        "1",
        "--tag-max-tags",
        "1",
        "東京の公園でりんごを食べる",
    )
    assert unlimited.returncode == limited.returncode == 0
    assert len(unlimited.stdout.splitlines()) > 1
    assert len(limited.stdout.splitlines()) == 1


def test_repeatable_text_dictionaries_affect_analysis(tmp_path: Path) -> None:
    first = tmp_path / "first.tsv"
    second = tmp_path / "second.tsv"
    first.write_text("青空庭園\tNOUN\n", encoding="utf-8")
    second.write_text("東京果樹園\tNOUN\n", encoding="utf-8")
    result = _run_cli(
        "--dict",
        str(first),
        "--dict",
        str(second),
        "--format",
        "json",
        "青空庭園",
        "東京果樹園",
    )
    assert result.returncode == 0, result.stderr
    morphemes = json.loads(result.stdout)["morphemes"]
    user_surfaces = {item["surface"] for item in morphemes if item["is_user_dict"]}
    assert user_surfaces == {"青空庭園", "東京果樹園"}


def test_relative_dictionary_path_and_uppercase_binary_extension(tmp_path: Path) -> None:
    relative_dictionary = tmp_path / "relative.tsv"
    relative_dictionary.write_text("青空庭園\tNOUN\n", encoding="utf-8")
    relative_path = os.path.relpath(relative_dictionary, Path.cwd())
    relative_result = _run_cli("--dict", relative_path, "--format", "json", "青空庭園")
    assert relative_result.returncode == 0, relative_result.stderr
    assert any(
        morpheme["surface"] == "青空庭園" and morpheme["is_user_dict"]
        for morpheme in json.loads(relative_result.stdout)["morphemes"]
    )

    uppercase_binary = tmp_path / "user.DIC"
    uppercase_binary.write_bytes(_bundled_user_dictionary())
    uppercase_result = _run_cli("--dict", str(uppercase_binary), "--format", "json", "東京")
    assert uppercase_result.returncode == 0, uppercase_result.stderr


def test_missing_and_invalid_text_dictionaries_fail(tmp_path: Path) -> None:
    missing = _run_cli("--dict", str(tmp_path / "missing.tsv"), "東京")
    assert missing.returncode == 1
    assert "error" in missing.stderr

    invalid = tmp_path / "invalid.csv"
    invalid.write_text('"東京,NOUN,0.5\n', encoding="utf-8")
    malformed = _run_cli("--dict", str(invalid), "東京")
    assert malformed.returncode == 1
    assert "Invalid legacy CSV quoting" in malformed.stderr


def test_invalid_binary_dictionary_fails(tmp_path: Path) -> None:
    dictionary = tmp_path / "invalid.dic"
    dictionary.write_bytes(b"invalid")
    result = _run_cli("--dict", str(dictionary), "東京")
    assert result.returncode == 1
    assert "Dictionary file too small" in result.stderr


def test_multiple_binary_dictionaries_load_additively(tmp_path: Path) -> None:
    first = tmp_path / "first.dic"
    second = tmp_path / "second.dic"
    first.write_bytes(_bundled_user_dictionary())
    second.write_bytes(_bundled_user_dictionary())
    result = _run_cli("--dict", str(first), "--dict", str(second), "東京")
    assert result.returncode == 0, result.stderr


def test_version_marker_after_double_dash_is_analyzed_as_text() -> None:
    result = _run_cli("--", "-v")
    assert result.returncode == 0, result.stderr
    assert result.stdout


@pytest.mark.parametrize(
    "args",
    [
        ["--tag-min-length", "-1", "東京"],
        ["--tag-max-tags", "-1", "東京"],
        ["--mode"],
        ["--format"],
        ["--dict"],
    ],
)
def test_negative_and_missing_option_values_are_syntax_errors(args: list[str]) -> None:
    result = _run_cli(*args)
    assert result.returncode == 2
    assert "error" in result.stderr


@pytest.mark.parametrize("abbreviation", ["--for", "--mo", "--tag-max"])
def test_long_options_do_not_accept_argparse_abbreviations(abbreviation: str) -> None:
    result = _run_cli(abbreviation, "json", "東京")
    assert result.returncode == 2
    assert "unrecognized arguments" in result.stderr


def test_missing_input_returns_one_and_uses_stderr() -> None:
    result = _run_cli(input_text="")
    assert result.returncode == 1
    assert result.stdout == ""
    assert "no input text provided" in result.stderr


def test_dictionary_warnings_are_written_to_stderr(
    monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    class WarningAnalyzer:
        dictionary_warnings = ["dictionary unavailable"]

        def __init__(self, **kwargs: object) -> None:
            pass

        def __enter__(self) -> WarningAnalyzer:
            return self

        def __exit__(self, *args: object) -> None:
            pass

        def analyze_with_normalized_text(self, text: str) -> suzume.AnalysisResult:
            return suzume.AnalysisResult(normalized_text=text, morphemes=[])

    monkeypatch.setattr(cli, "Suzume", WarningAnalyzer)
    parser = cli._build_parser()
    args = parser.parse_args(["東京"])
    assert cli._run_analysis(args, parser) == 0
    assert "warning: dictionary unavailable" in capsys.readouterr().err


def test_dictionary_disable_flags_are_passed_to_analyzer(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    captured: dict[str, object] = {}

    class CapturingAnalyzer:
        dictionary_warnings: list[str] = []

        def __init__(self, **kwargs: object) -> None:
            captured.update(kwargs)

        def __enter__(self) -> CapturingAnalyzer:
            return self

        def __exit__(self, *args: object) -> None:
            pass

        def analyze_with_normalized_text(self, text: str) -> suzume.AnalysisResult:
            return suzume.AnalysisResult(normalized_text=text, morphemes=[])

    monkeypatch.setattr(cli, "Suzume", CapturingAnalyzer)
    parser = cli._build_parser()
    args = parser.parse_args(["--no-core-dict", "--no-user-dict", "--skip-env-config", "東京"])
    assert cli._run_analysis(args, parser) == 0
    assert captured["skip_core_dictionary"] is True
    assert captured["skip_user_dictionary"] is True
    assert captured["skip_env_config"] is True


def test_invalid_utf8_stdin_fails_cleanly() -> None:
    result = _run_cli_bytes("--format", "json", input_bytes=b"\xff\n")
    assert result.returncode == 1
    assert b"error" in result.stderr
