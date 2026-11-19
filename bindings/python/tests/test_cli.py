"""Contract tests for the ``suzume`` console command.

The command shares its flags, messages, exit codes and output bytes with the
native ``suzume-cli`` analyze command; only the program name differs.
"""

from __future__ import annotations

import io
import json
import os
import re
import subprocess
import sysconfig
from importlib import resources
from pathlib import Path
from typing import Any, TypedDict, cast

import pytest

import suzume
from suzume import cli

_JSON_MORPHEME_KEYS = [
    "surface",
    "pos",
    "base_form",
    "pos_ja",
    "conj_type",
    "conj_form",
    "extended_pos",
    "start",
    "end",
    "is_user_dict",
    "is_formal_noun",
    "is_low_info",
    "is_unknown",
    "is_from_dictionary",
    "score",
]
_TAG_NAMES = "NOUN, VERB, ADJ, ADV, PARTICLE, AUX"

# Every boolean option of the analyze command: (flag, constructor or tag keyword, value it sets).
_ANALYZER_FLAGS = [
    ("--no-preserve-vu", "preserve_vu", False),
    ("--no-preserve-case", "preserve_case", False),
    ("--preserve-symbols", "preserve_symbols", True),
    ("--no-lemmatize", "lemmatize", False),
    ("--merge-compounds", "merge_compounds", True),
    ("--skip-env-config", "skip_env_config", True),
    ("--skip-core-dictionary", "skip_core_dictionary", True),
    ("--skip-bundled-user-dictionary", "skip_bundled_user_dictionary", True),
]
_TAG_FLAGS = [
    ("--tag-no-exclude-particles", "exclude_particles", False),
    ("--tag-no-exclude-auxiliaries", "exclude_auxiliaries", False),
    ("--tag-no-exclude-formal-nouns", "exclude_formal_nouns", False),
    ("--tag-no-exclude-low-info", "exclude_low_info", False),
    ("--tag-exclude-basic", "exclude_basic", True),
    ("--tag-no-remove-duplicates", "remove_duplicates", False),
    ("--tag-no-use-base-form", "use_base_form", False),
]
_TAG_VALUE_OPTIONS = [
    ("--tag-pos-filter", "NOUN"),
    ("--tag-min-length", "1"),
    ("--tag-max-tags", "1"),
]
_REMOVED_FLAGS = [
    "--normalize-vu",
    "--lowercase",
    "--no-core-dict",
    "--no-user-dict",
    "--include-particles",
    "--include-auxiliaries",
    "--include-formal-nouns",
    "--include-low-info",
    "--tag-keep-duplicates",
    "--tag-use-surface",
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


def _run_cli(*args: str, input_text: str | None = "") -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [_console_script(), *args],
        input=input_text,
        text=True,
        capture_output=True,
        check=False,
    )


def _run_cli_bytes(
    *args: str | bytes, input_bytes: bytes = b""
) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        [_console_script(), *args],
        input=input_bytes,
        capture_output=True,
        check=False,
    )


def _json_result(*args: str, input_text: str | None = "") -> _JsonPayload:
    result = _run_cli("--format", "json", *args, input_text=input_text)
    assert result.returncode == 0, result.stderr
    return cast(_JsonPayload, json.loads(result.stdout))


def _assert_usage_error(result: subprocess.CompletedProcess[str], message: str) -> None:
    assert result.returncode == 2, result
    assert result.stdout == ""
    assert result.stderr == f"suzume: error: {message}\n"


def test_console_script_comes_from_active_environment() -> None:
    script = Path(_console_script()).resolve()
    scripts_dir = Path(sysconfig.get_path("scripts")).resolve()
    assert script.parent == scripts_dir


# --- version and help ---


@pytest.mark.parametrize("alias", [["--version"], ["-v"], ["version"]])
def test_version_prints_exactly_one_line(alias: list[str]) -> None:
    result = _run_cli(*alias)
    assert result.returncode == 0
    assert result.stdout == f"suzume {suzume.version()}\n"
    assert result.stderr == ""


def test_a_usage_error_wins_over_version() -> None:
    _assert_usage_error(
        _run_cli("-v", "--mode", "invalid", "東京"),
        "invalid mode: invalid (expected one of: normal, search, split)",
    )


def test_help_lists_every_option_in_the_parser_table() -> None:
    result = _run_cli("--help")
    assert result.returncode == 0
    listed = set(re.findall(r"^  (?:-\w+, )?(--[a-z-]+)", result.stdout, re.MULTILINE))
    assert listed == {option.long_name for option in cli._OPTIONS}
    for removed in [*_REMOVED_FLAGS, "--dict ", "--tag-pos "]:
        assert removed not in result.stdout


def test_help_describes_symbols_and_base_form() -> None:
    result = _run_cli("--help")
    assert (
        "Keep punctuation-like SYMBOL tokens (emoji and content symbols stay OTHER either way)"
        in result.stdout
    )
    assert "surface TAB pos TAB base_form TAB start TAB end" in result.stdout
    assert re.search(r"\blemmas?\b", result.stdout) is None


def test_analyze_help_matches_top_level_help() -> None:
    assert _run_cli("analyze", "--help").stdout == _run_cli("--help").stdout


# --- flag vocabulary ---


@pytest.mark.parametrize("flag", _REMOVED_FLAGS)
def test_removed_flags_are_unknown(flag: str) -> None:
    _assert_usage_error(_run_cli(flag, "東京"), f"unknown option: {flag}")


@pytest.mark.parametrize("flag", ["--dict", "--tag-pos"])
def test_removed_value_options_are_unknown(flag: str) -> None:
    _assert_usage_error(_run_cli(flag, "x", "東京"), f"unknown option: {flag}")


@pytest.mark.parametrize("abbreviation", ["--for", "--mo", "--tag-max"])
def test_long_options_are_never_abbreviated(abbreviation: str) -> None:
    _assert_usage_error(_run_cli(abbreviation, "json", "東京"), f"unknown option: {abbreviation}")


def test_morpheme_format_is_removed_and_tsv_is_the_default() -> None:
    _assert_usage_error(
        _run_cli("-f", "morpheme", "東京"),
        "invalid format: morpheme (expected one of: tsv, json, chasen, tags)",
    )
    default = _run_cli("東京の公園")
    assert default.returncode == 0
    assert default.stdout == _run_cli("-f", "tsv", "東京の公園").stdout


@pytest.mark.parametrize(
    ("args", "message"),
    [
        (["--mode"], "missing value for --mode"),
        (["--format"], "missing value for --format"),
        (["-d"], "missing value for -d"),
        (["--dictionary"], "missing value for --dictionary"),
        (["--mode="], "missing value for --mode"),
        (["--mode", "--format", "json", "東京"], "missing value for --mode"),
        (["-f", "tags", "--tag-min-length=-1", "東京"], "invalid --tag-min-length value: -1"),
        (
            ["-f", "tags", "--tag-min-length", "4294967296", "東京"],
            "invalid --tag-min-length value: 4294967296",
        ),
        (
            ["-f", "tags", "--tag-max-tags=18446744073709551615", "東京"],
            "invalid --tag-max-tags value: 18446744073709551615",
        ),
        (["-f", "tags", "--tag-min-length", "-1", "東京"], "missing value for --tag-min-length"),
        (["-d", "-x", "東京"], "missing value for -d"),
        (["--dictionary", "--", "東京"], "missing value for --dictionary"),
        (["--mode", "-f", "tsv", "東京"], "missing value for --mode"),
        (["-f", "tags", "--tag-max-tags", "1.5", "東京"], "invalid --tag-max-tags value: 1.5"),
        (
            ["-f", "tags", "--tag-pos-filter", "noun", "東京"],
            f"invalid --tag-pos-filter value: noun (expected one of: {_TAG_NAMES})",
        ),
        (["--mode", "fast", "東京"], "invalid mode: fast (expected one of: normal, search, split)"),
        (["--unknown-flag", "東京"], "unknown option: --unknown-flag"),
        (["-dfile.tsv", "東京"], "unknown option: -dfile.tsv"),
    ],
)
def test_usage_errors_print_one_line_and_exit_two(args: list[str], message: str) -> None:
    _assert_usage_error(_run_cli(*args), message)


def test_size_options_accept_the_full_unsigned_32_bit_range() -> None:
    for value in ("0", "4294967295"):
        result = _run_cli("-f", "tags", "--tag-min-length", value, "--tag-max-tags", value, "東京")
        assert result.returncode == 0, result.stderr


def test_a_bare_dash_is_a_value_and_the_equals_form_passes_a_dash_value() -> None:
    _assert_usage_error(
        _run_cli("--mode", "-", "東京"), "invalid mode: - (expected one of: normal, search, split)"
    )
    _assert_usage_error(
        _run_cli("--mode=-x", "東京"), "invalid mode: -x (expected one of: normal, search, split)"
    )


@pytest.mark.parametrize(
    "tag_args",
    [[flag] for flag, _, _ in _TAG_FLAGS] + [[flag, value] for flag, value in _TAG_VALUE_OPTIONS],
)
@pytest.mark.parametrize("format_args", [[], ["-f", "json"], ["-f", "tsv"], ["-f", "chasen"]])
def test_tag_options_require_the_tags_format(tag_args: list[str], format_args: list[str]) -> None:
    _assert_usage_error(
        _run_cli(*format_args, *tag_args, "東京"), "--tag-* options require -f tags"
    )


def test_long_options_accept_an_attached_value() -> None:
    attached = _run_cli("--format=json", "--mode=search", "東京")
    separate = _run_cli("--format", "json", "--mode", "search", "東京")
    assert attached.returncode == 0, attached.stderr
    assert attached.stdout == separate.stdout


# --- option wiring ---


class _StubAnalyzer:
    """Stand-in for ``Suzume`` that records its arguments and grows warnings per load."""

    instances: list[_StubAnalyzer] = []
    initial_warnings: tuple[str, ...] = ()

    def __init__(self, **kwargs: object) -> None:
        self.kwargs = kwargs
        self.tag_kwargs: dict[str, object] = {}
        self.warnings = list(type(self).initial_warnings)
        self.loads: list[bytes] = []
        type(self).instances.append(self)

    def __enter__(self) -> _StubAnalyzer:
        return self

    def __exit__(self, *args: object) -> None:
        pass

    @property
    def dictionary_warnings(self) -> tuple[str, ...]:
        return tuple(self.warnings)

    def load_user_dictionary(self, dictionary: bytes) -> int:
        self.loads.append(dictionary)
        self.warnings.append(f"warning from load {len(self.loads)}")
        return 1

    def load_binary_dictionary(self, dictionary: bytes) -> int:
        self.loads.append("<binary>")
        return 1

    def analyze_with_normalized_text(self, text: str) -> suzume.AnalysisResult:
        return suzume.AnalysisResult(normalized_text=text, morphemes=())

    def generate_tags(self, text: str, **kwargs: object) -> tuple[suzume.Tag, ...]:
        self.tag_kwargs = kwargs
        return ()


@pytest.fixture
def stub(monkeypatch: pytest.MonkeyPatch) -> type[_StubAnalyzer]:
    monkeypatch.setattr(_StubAnalyzer, "instances", [])
    monkeypatch.setattr(_StubAnalyzer, "initial_warnings", ())
    monkeypatch.setattr(cli, "Suzume", _StubAnalyzer)
    return _StubAnalyzer


def test_analyzer_defaults(stub: type[_StubAnalyzer]) -> None:
    assert cli.main(["東京"]) == 0
    assert stub.instances[0].kwargs == {
        "mode": "normal",
        "preserve_vu": True,
        "preserve_case": True,
        "preserve_symbols": False,
        "lemmatize": True,
        "merge_compounds": False,
        "skip_env_config": False,
        "skip_core_dictionary": False,
        "skip_bundled_user_dictionary": False,
    }


@pytest.mark.parametrize(("flag", "keyword", "value"), _ANALYZER_FLAGS)
def test_each_analyzer_flag_sets_its_option(
    flag: str, keyword: str, value: bool, stub: type[_StubAnalyzer]
) -> None:
    assert cli.main([flag, "東京"]) == 0
    assert stub.instances[0].kwargs[keyword] is value


def test_tag_defaults(stub: type[_StubAnalyzer]) -> None:
    assert cli.main(["-f", "tags", "東京"]) == 0
    assert stub.instances[0].tag_kwargs == {
        "pos_filter": (),
        "exclude_particles": True,
        "exclude_auxiliaries": True,
        "exclude_formal_nouns": True,
        "exclude_low_info": True,
        "exclude_basic": False,
        "remove_duplicates": True,
        "use_base_form": True,
        "min_length": 2,
        "max_tags": 0,
    }


@pytest.mark.parametrize(("flag", "keyword", "value"), _TAG_FLAGS)
def test_each_tag_flag_sets_its_option(
    flag: str, keyword: str, value: bool, stub: type[_StubAnalyzer]
) -> None:
    assert cli.main(["-f", "tags", flag, "東京"]) == 0
    assert stub.instances[0].tag_kwargs[keyword] is value


def test_tag_value_options_are_passed_through(stub: type[_StubAnalyzer]) -> None:
    argv = ["-f", "tags", "--tag-min-length", "5", "--tag-max-tags", "3"]
    argv += ["--tag-pos-filter", "VERB", "--tag-pos-filter", "NOUN", "--tag-pos-filter", "VERB"]
    assert cli.main([*argv, "東京"]) == 0
    kwargs = stub.instances[0].tag_kwargs
    assert kwargs["min_length"] == 5
    assert kwargs["max_tags"] == 3
    assert kwargs["pos_filter"] == ("VERB", "NOUN", "VERB")


@pytest.mark.parametrize("mode", ["normal", "search", "split"])
def test_every_mode_is_passed_through(mode: str, stub: type[_StubAnalyzer]) -> None:
    assert cli.main(["-m", mode, "東京"]) == 0
    assert stub.instances[0].kwargs["mode"] == mode


def test_options_may_follow_the_command_and_the_text(stub: type[_StubAnalyzer]) -> None:
    assert cli.main(["東京", "analyze", "--merge-compounds", "公園"]) == 0
    assert stub.instances[0].kwargs["merge_compounds"] is True


def test_warnings_print_once_after_the_load_that_produced_them(
    stub: type[_StubAnalyzer],
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
) -> None:
    stub.initial_warnings = ("bundled warning",)
    first = tmp_path / "first.tsv"
    second = tmp_path / "second.tsv"
    first.write_text("一\tNOUN\n", encoding="utf-8")
    second.write_text("二\tNOUN\n", encoding="utf-8")
    assert cli.main(["-d", str(first), "--dictionary", str(second), "東京"]) == 0
    assert capsys.readouterr().err == (
        "warning: bundled warning\nwarning: warning from load 1\nwarning: warning from load 2\n"
    )
    assert stub.instances[0].loads == ["一\tNOUN\n".encode(), "二\tNOUN\n".encode()]


def test_dictionary_option_accepts_an_attached_value(
    stub: type[_StubAnalyzer], tmp_path: Path
) -> None:
    source = tmp_path / "words.tsv"
    source.write_text("一\tNOUN\n", encoding="utf-8")
    assert cli.main([f"--dictionary={source}", "東京"]) == 0
    assert stub.instances[0].loads == ["一\tNOUN\n".encode()]


# --- input ---


def _stdin(data: bytes, *, tty: bool = False) -> io.TextIOWrapper:
    class _Bytes(io.BytesIO):
        def isatty(self) -> bool:
            return tty

    return io.TextIOWrapper(_Bytes(data), encoding="utf-8")


def test_no_operand_on_a_terminal_is_a_usage_error(
    monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    monkeypatch.setattr("sys.stdin", _stdin(b"never read", tty=True))
    assert cli.main([]) == 2
    captured = capsys.readouterr()
    assert captured.out == ""
    assert captured.err == "suzume: error: no input text provided\n"


@pytest.mark.parametrize(
    ("args", "stdin"),
    [
        ([""], ""),
        ([], ""),
        ([], "\n"),
        ([], "\r\n"),
        ([], "\ufeff"),
        (["-f", "json", ""], ""),
        (["-f", "json"], "\n"),
        (["-f", "tags"], ""),
    ],
)
def test_empty_input_prints_nothing_and_succeeds(args: list[str], stdin: str) -> None:
    result = _run_cli(*args, input_text=stdin)
    assert (result.returncode, result.stdout, result.stderr) == (0, "", "")


def test_tags_format_with_no_tags_prints_nothing() -> None:
    result = _run_cli("-f", "tags", "の")
    assert (result.returncode, result.stdout, result.stderr) == (0, "", "")


def test_operands_after_double_dash_are_text() -> None:
    assert _json_result("--", "analyze", "--format")["input"] == "analyze --format"
    assert _json_result("analyze", "--", "analyze")["input"] == "analyze"
    assert _json_result("--", "-v")["input"] == "-v"


def test_implicit_and_explicit_analyze_read_the_same_text() -> None:
    implicit = _json_result("東京", "公園")
    explicit = _json_result("analyze", "東京", "公園")
    assert implicit == explicit
    assert implicit["input"] == "東京 公園"


@pytest.mark.parametrize(
    ("stdin", "expected"),
    [
        (b"\xe6\x9d\xb1\xe4\xba\xac", "東京"),
        ("東京\n".encode(), "東京"),
        ("東京\n公園\n".encode(), "東京\n公園"),
        ("東京\n\n".encode(), "東京\n"),
        ("\ufeff東京\n".encode(), "東京"),
        ("東京\ufeff公園\n".encode(), "東京\ufeff公園"),
        ("\ufeff東京\r\n公園\r\n".encode(), "東京\r\n公園"),
    ],
)
def test_stdin_bytes_are_read_as_given(stdin: bytes, expected: str) -> None:
    result = _run_cli_bytes("-f", "json", input_bytes=stdin)
    assert result.returncode == 0, result.stderr
    assert json.loads(result.stdout)["input"] == expected


@pytest.mark.parametrize(
    ("argument", "expected"),
    [("\ufeff東京", "東京"), ("東京\n", "東京"), ("東京\r\n", "東京"), ("東京\n\n", "東京\n")],
)
def test_operands_are_trimmed_like_stdin(argument: str, expected: str) -> None:
    assert _json_result(argument)["input"] == expected


@pytest.mark.parametrize(
    ("args", "stdin"),
    [(["-f", "json"], b"\xff\n"), (["-f", "json", b"\xe6\x9d"], b"")],
    ids=["stdin", "operand"],
)
def test_invalid_utf8_input_fails_with_one_line(args: list[str | bytes], stdin: bytes) -> None:
    result = _run_cli_bytes(*args, input_bytes=stdin)
    assert result.returncode == 1
    assert result.stdout == b""
    assert result.stderr == b"suzume: error: invalid UTF-8 input\n"


# --- output ---


def _expected_json(text: str) -> str:
    """Build the D10 byte layout independently of the CLI's own writer."""
    with suzume.Suzume() as analyzer:
        analysis = analyzer.analyze_with_normalized_text(text)
    lines = []
    for morpheme in analysis.morphemes:
        conjugates = morpheme.pos in {"VERB", "ADJ", "AUX"}
        fields: list[tuple[str, Any]] = [
            ("surface", morpheme.surface),
            ("pos", morpheme.pos),
            ("base_form", morpheme.base_form),
            ("pos_ja", morpheme.pos_ja),
            ("conj_type", morpheme.conj_type if conjugates else None),
            ("conj_form", morpheme.conj_form if conjugates else None),
            ("extended_pos", morpheme.extended_pos),
            ("start", morpheme.start),
            ("end", morpheme.end),
            ("is_user_dict", morpheme.is_user_dict),
            ("is_formal_noun", morpheme.is_formal_noun),
            ("is_low_info", morpheme.is_low_info),
            ("is_unknown", morpheme.is_unknown),
            ("is_from_dictionary", morpheme.is_from_dictionary),
        ]
        body = ", ".join(
            f'"{key}": {json.dumps(value, ensure_ascii=False)}' for key, value in fields
        )
        lines.append(f'    {{{body}, "score": {format(morpheme.score, ".9g")}}}')
    return (
        "{\n"
        f'  "input": {json.dumps(text, ensure_ascii=False)},\n'
        f'  "normalized_text": {json.dumps(analysis.normalized_text, ensure_ascii=False)},\n'
        '  "morphemes": [\n' + ",\n".join(lines) + "\n  ]\n}\n"
    )


@pytest.mark.parametrize("text", ["東京の公園", "書かなかった", "ＡＢＣ１２３", '東京\x01\x7fé"\\'])
def test_json_output_has_the_shared_byte_layout(text: str) -> None:
    result = _run_cli("-f", "json", text)
    assert result.returncode == 0, result.stderr
    assert result.stdout == _expected_json(text)


def test_json_keys_follow_the_morpheme_field_order() -> None:
    payload = _json_result("書かなかった")
    for morpheme in payload["morphemes"]:
        assert list(morpheme) == _JSON_MORPHEME_KEYS
    verb = payload["morphemes"][0]
    assert verb["pos"] == "VERB"
    assert verb["conj_type"] is not None


def test_json_conjugation_is_null_outside_verb_adj_aux() -> None:
    morpheme = suzume.Morpheme(
        surface="東京",
        pos="NOUN",
        base_form="東京",
        pos_ja="名詞",
        conj_type="五段・カ行",
        conj_form="連用形",
        extended_pos="NOUN",
        start=0,
        end=2,
        is_user_dict=False,
        is_formal_noun=False,
        is_low_info=False,
        is_unknown=True,
        is_from_dictionary=False,
        score=3.0,
    )
    line = cli._json_morpheme(morpheme)
    assert '"conj_type": null, "conj_form": null' in line
    assert line.endswith('"score": 3}')
    assert cli._chasen_line(morpheme) == "東京\t*\t東京\t名詞\t*\t*"


def test_json_offsets_slice_normalized_text() -> None:
    payload = _json_result("ﾊﾞｽに乗る")
    assert payload["normalized_text"] == "バスに乗る"
    for morpheme in payload["morphemes"]:
        start, end = cast(int, morpheme["start"]), cast(int, morpheme["end"])
        assert payload["normalized_text"][start:end] == morpheme["surface"]


def test_chasen_output() -> None:
    result = _run_cli("-f", "chasen", "りんごを食べる")
    assert result.returncode == 0
    assert result.stdout == (
        "りんご\t*\tりんご\t名詞\t*\t*\n"
        "を\t*\tを\t助詞\t*\t*\n"
        "食べる\t*\t食べる\t動詞\t一段\t終止形\n"
        "EOS\n"
    )


def test_tsv_output() -> None:
    result = _run_cli("-f", "tsv", "東京に行く")
    assert result.returncode == 0
    assert (
        result.stdout == "東京\tNOUN\t東京\t0\t2\nに\tPARTICLE\tに\t2\t3\n行く\tVERB\t行く\t3\t5\n"
    )


@pytest.mark.parametrize("output_format", ["tsv", "chasen"])
def test_tab_outputs_escape_embedded_delimiters(output_format: str) -> None:
    result = _run_cli("--preserve-symbols", "-f", output_format, "東京\t大阪")
    assert result.returncode == 0, result.stderr
    assert "\\t\t" in result.stdout
    assert "\n\t" not in result.stdout


def test_tag_output_and_filters() -> None:
    nouns = _run_cli(
        "-f", "tags", "--tag-pos-filter", "NOUN", "--tag-min-length", "1", "東京の公園"
    )
    assert nouns.returncode == 0
    assert nouns.stdout == "東京\tNOUN\n公園\tNOUN\n"
    particles = _run_cli(
        "-f", "tags", "--tag-pos-filter", "PARTICLE", "--tag-min-length", "1", "りんごについて"
    )
    assert particles.stdout == "について\tPARTICLE\n"


def test_tag_max_tags_zero_is_unlimited() -> None:
    text = "東京の公園でりんごを食べる"
    unlimited = _run_cli("-f", "tags", "--tag-min-length", "1", "--tag-max-tags", "0", text)
    limited = _run_cli("-f", "tags", "--tag-min-length", "1", "--tag-max-tags", "1", text)
    assert len(unlimited.stdout.splitlines()) > 1
    assert len(limited.stdout.splitlines()) == 1


# --- dictionaries ---


def test_repeatable_text_dictionaries_affect_analysis(tmp_path: Path) -> None:
    first = tmp_path / "first.tsv"
    second = tmp_path / "second.tsv"
    first.write_text("青空庭園\tNOUN\n", encoding="utf-8")
    second.write_text("東京果樹園\tNOUN\n", encoding="utf-8")
    payload = _json_result("-d", str(first), "--dictionary", str(second), "青空庭園", "東京果樹園")
    user_surfaces = {item["surface"] for item in payload["morphemes"] if item["is_user_dict"]}
    assert user_surfaces == {"青空庭園", "東京果樹園"}


def test_binary_dictionaries_load_with_any_extension_case(tmp_path: Path) -> None:
    upper = tmp_path / "user.DIC"
    lower = tmp_path / "user.dic"
    upper.write_bytes(_bundled_user_dictionary())
    lower.write_bytes(_bundled_user_dictionary())
    result = _run_cli("-d", str(upper), "-d", str(lower), "東京")
    assert result.returncode == 0, result.stderr


@pytest.mark.parametrize(
    ("name", "native_text"),
    [
        ("missing.tsv", "Failed to open dictionary file: {path}"),
        ("missing.dic", "Failed to open binary dictionary file: {path}"),
    ],
)
def test_missing_dictionary_reports_the_path(tmp_path: Path, name: str, native_text: str) -> None:
    path = str(tmp_path / name)
    result = _run_cli("-d", path, "東京")
    assert result.returncode == 1
    assert result.stdout == ""
    detail = native_text.format(path=path)
    assert result.stderr == f"suzume: error: failed to load dictionary: {path}: {detail}\n"


@pytest.mark.parametrize(
    ("name", "content", "native_text"),
    [
        ("invalid.dic", b"invalid", "Dictionary file too small"),
        ("invalid.csv", '"東京,NOUN,0.5\n'.encode(), "Invalid quoting"),
        ("sjis.tsv", b"\x93\x8c\x8b\x9e\tNOUN\n", "Dictionary entry is not valid UTF-8 at line 1"),
    ],
)
def test_malformed_dictionary_reports_the_library_text(
    tmp_path: Path, name: str, content: bytes, native_text: str
) -> None:
    path = tmp_path / name
    path.write_bytes(content)
    result = _run_cli("-d", str(path), "東京")
    assert result.returncode == 1
    assert result.stderr.startswith(f"suzume: error: failed to load dictionary: {path}: ")
    assert native_text in result.stderr
    assert result.stderr.count("\n") == 1
