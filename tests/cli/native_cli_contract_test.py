"""Real-process contract checks for the native Suzume CLI."""

from __future__ import annotations

import json
import os
import pty
import re
import select
import subprocess
import sys
import tempfile
from pathlib import Path


def run_cli(
    cli: Path,
    *args: str,
    input_text: str | None = None,
    input_bytes: bytes | None = None,
    env: dict[str, str] | None = None,
    cwd: Path | None = None,
    expect_success: bool = True,
) -> subprocess.CompletedProcess[str]:
    if input_text is not None and input_bytes is not None:
        raise ValueError("input_text and input_bytes are mutually exclusive")
    process_input = input_bytes
    if input_text is not None:
        process_input = input_text.encode("utf-8")
    raw_result = subprocess.run(
        [str(cli), *args],
        input=process_input,
        capture_output=True,
        check=False,
        env=env,
        cwd=cwd,
    )
    result = subprocess.CompletedProcess(
        raw_result.args,
        raw_result.returncode,
        raw_result.stdout.decode("utf-8"),
        raw_result.stderr.decode("utf-8"),
    )
    if expect_success and result.returncode != 0:
        raise AssertionError(
            f"{' '.join(args)} failed with {result.returncode}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
    if not expect_success and result.returncode == 0:
        raise AssertionError(f"{' '.join(args)} unexpectedly succeeded\nstdout:\n{result.stdout}")
    return result


def read_pty_until(master: int, needle: bytes) -> bytes:
    output = bytearray()
    while needle not in output:
        readable, _, _ = select.select([master], [], [], 5)
        if not readable:
            raise AssertionError(f"PTY did not produce {needle!r}; got {bytes(output)!r}")
        try:
            chunk = os.read(master, 4096)
        except OSError as exc:
            raise AssertionError(f"PTY closed before producing {needle!r}: {bytes(output)!r}") from exc
        if not chunk:
            raise AssertionError(f"PTY closed before producing {needle!r}: {bytes(output)!r}")
        output.extend(chunk)
    return bytes(output)


def assert_terminal_eof_saves_edits(cli: Path, source: Path) -> None:
    master, slave = pty.openpty()
    process = subprocess.Popen([str(cli), "dict", "-i", str(source)], stdin=slave, stdout=slave, stderr=slave)
    os.close(slave)
    try:
        os.write(master, "add 青空りんご園 NOUN\n".encode("utf-8"))
        read_pty_until(master, b"suzume*> ")
        os.write(master, b"\x04")
        output = read_pty_until(master, b"Saved unsaved changes at EOF.")
        assert process.wait(timeout=5) == 0, output.decode("utf-8", errors="replace")
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)
        os.close(master)

    assert "青空りんご園\tNOUN" in source.read_text(encoding="utf-8")


def assert_invalid_arguments(cli: Path) -> None:
    invalid_commands = (
        ("analyze", "--mode", "wide", "東京"),
        ("analyze", "--format", "yaml", "東京"),
        ("analyze", "--dictionary"),
        ("analyze", "--dictionary", "--format", "json", "東京"),
        ("analyze", "-f", "tags", "--tag-pos-filter", "other", "東京"),
        ("analyze", "-f", "tags", "--tag-min-length"),
        ("analyze", "--unknown-analysis-flag", "東京"),
    )
    for command in invalid_commands:
        result = run_cli(cli, *command, expect_success=False)
        assert result.returncode == 2, (command, result.returncode)
        assert result.stdout == "", (command, result.stdout)
        assert result.stderr.startswith("suzume-cli: error: "), (command, result.stderr)


def assert_version_contract(cli: Path) -> None:
    short = run_cli(cli, "-v")
    long = run_cli(cli, "--version")
    command = run_cli(cli, "version")
    assert short.stdout == long.stdout == command.stdout
    assert re.fullmatch(r"suzume-cli [0-9]+\.[0-9]+\.[0-9]+\S*\n", short.stdout), short.stdout

    invalid = run_cli(cli, "-v", "--mode", "invalid", "東京", expect_success=False)
    assert invalid.returncode == 2
    assert invalid.stderr == "suzume-cli: error: invalid mode: invalid (expected one of: normal, search, split)\n"


def assert_help_contract(cli: Path) -> None:
    top_level = run_cli(cli, "--help")
    analyze = run_cli(cli, "analyze", "--help")
    dictionary = run_cli(cli, "dict", "--help")
    assert "Commands:" in top_level.stdout
    assert "Output Formats:" in top_level.stdout
    assert "Output Formats:" in analyze.stdout
    commands_block = top_level.stdout.split("Commands:\n", 1)[1].split("\n\n", 1)[0]
    assert "Use 'suzume-cli [command] --help' for command-specific help." in commands_block
    for help_text in (top_level.stdout, analyze.stdout):
        assert "-v, --version                     Show the version" in help_text
        assert "--tag-* options require -f tags." in help_text
        assert "Text operands are joined with spaces; with none, text is read from stdin." in help_text
    assert "Commands:" not in analyze.stdout
    assert "-VV, --very-verbose" in analyze.stdout
    assert "--debug" in top_level.stdout
    assert "--debug" in analyze.stdout
    assert "SUZUME_DEBUG" in top_level.stdout
    assert "SUZUME_SCORER_{SECTION}_{KEY}" in analyze.stdout
    assert "tsv                    surface TAB pos TAB base_form TAB start TAB end (default)" in analyze.stdout
    assert "offsets refer to normalized text" in analyze.stdout
    assert "compile <in1.tsv> <in2.tsv>... <out.dic>" in dictionary.stdout
    assert "--filter-trivial" in dictionary.stdout
    for value in ("CONJUNCTION", "DETERMINER", "PRONOUN", "PREFIX", "SUFFIX", "INTERJECTION"):
        assert value in dictionary.stdout
    assert "FAMILY, GIVEN" in dictionary.stdout
    assert "info <file>" in dictionary.stdout
    assert "validate <file>" in dictionary.stdout
    assert "Long options that take a value also accept --option=value." in top_level.stdout
    assert "Long options that take a value also accept --option=value." in analyze.stdout

    default = run_cli(cli, "analyze", "東京")
    tsv = run_cli(cli, "analyze", "--format", "tsv", "東京")
    assert tsv.stdout == default.stdout
    for help_text in (top_level.stdout, analyze.stdout):
        assert re.search(r"\blemma\b", help_text) is None
        assert (
            "--preserve-symbols                Keep punctuation-like SYMBOL tokens "
            "(emoji and content symbols stay OTHER either way)"
        ) in help_text
        assert (
            "--no-lemmatize                    Keep dictionary base forms without post-analysis correction"
        ) in help_text
        assert (
            "--skip-bundled-user-dictionary    Skip only the bundled user.dic; -d dictionaries still load"
        ) in help_text
        assert re.search(r"\bmorpheme\b", help_text) is None


def assert_stdin_contract(cli: Path) -> None:
    cases = {
        "東京\n": "東京",
        "東京\n公園\n": "東京\n公園",
        "東京\n\n": "東京\n",
        "\ufeff東京": "東京",
        "\ufeff東京\r\n": "東京",
    }
    for input_text, expected in cases.items():
        result = run_cli(cli, "analyze", "--format", "json", input_text=input_text)
        payload = json.loads(result.stdout)
        assert payload["input"] == expected, (repr(input_text), repr(payload["input"]))


def assert_invalid_utf8_rejected(cli: Path) -> None:
    invalid_inputs = {
        "stray byte": b"\xff",
        "overlong scalar": b"\xc0\xaf",
        "surrogate": b"\xed\xa0\x80",
        "out-of-range scalar": b"\xf4\x90\x80\x80",
    }
    for label, input_bytes in invalid_inputs.items():
        result = run_cli(
            cli,
            "analyze",
            "--format",
            "json",
            input_bytes=input_bytes,
            expect_success=False,
        )
        assert result.stdout == "", (label, result.stdout)
        assert result.returncode == 1, (label, result.returncode)
        assert result.stderr == "suzume-cli: error: invalid UTF-8 input\n", (label, result.stderr)


def assert_json_schema(cli: Path) -> None:
    result = run_cli(cli, "analyze", "--format", "json", "東京へ行く")
    payload = json.loads(result.stdout)
    assert set(payload) == {"input", "normalized_text", "morphemes"}
    assert payload["input"] == payload["normalized_text"] == "東京へ行く"
    assert payload["morphemes"]
    ordered = json.loads(result.stdout, object_pairs_hook=lambda pairs: pairs)
    morpheme_pairs = dict(ordered)["morphemes"]
    for pairs in morpheme_pairs:
        assert [key for key, _ in pairs] == [
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
    by_surface = {morpheme["surface"]: morpheme for morpheme in payload["morphemes"]}
    assert by_surface["東京"]["pos_ja"] == "名詞"
    assert by_surface["東京"]["conj_type"] is None
    assert by_surface["東京"]["conj_form"] is None
    assert by_surface["行く"]["base_form"] == "行く"
    assert by_surface["行く"]["pos_ja"] == "動詞"
    assert by_surface["行く"]["conj_type"] == "五段・カ行"
    assert by_surface["行く"]["conj_form"] == "終止形"
    assert '"conj_type": null, "conj_form": null' in result.stdout
    lines = result.stdout.splitlines()
    assert lines[0] == "{"
    assert lines[1] == '  "input": "東京へ行く",'
    assert lines[2] == '  "normalized_text": "東京へ行く",'
    assert lines[3] == '  "morphemes": ['
    assert lines[4].startswith('    {"surface": "東京", "pos": "NOUN", "base_form": "東京", "pos_ja": "名詞", ')
    assert lines[-2:] == ["  ]", "}"]

    auxiliary = json.loads(run_cli(cli, "analyze", "--format", "json", "書かなかった").stdout)
    nakat = next(morpheme for morpheme in auxiliary["morphemes"] if morpheme["surface"] == "なかっ")
    assert nakat["pos"] == "AUX"
    assert nakat["conj_type"] is None
    assert nakat["conj_form"] == "連用形"

    escaped = run_cli(cli, "analyze", "--format", "json", "東京\x01\x7fé")
    assert '"input": "東京\\u0001\x7fé",' in escaped.stdout


def assert_offsets_slice_normalized_text(cli: Path) -> None:
    for input_text, expected_normalized in (("ﾊﾞｽに乗る", "バスに乗る"), ("ＡＢＣ１２３", "ABC123")):
        result = run_cli(cli, "analyze", "--format", "json", input_text)
        payload = json.loads(result.stdout)
        assert payload["input"] == input_text
        assert payload["normalized_text"] == expected_normalized
        for morpheme in payload["morphemes"]:
            assert payload["normalized_text"][morpheme["start"] : morpheme["end"]] == morpheme["surface"]


def assert_control_characters_do_not_corrupt_tsv(cli: Path) -> None:
    result = run_cli(
        cli,
        "analyze",
        "--preserve-symbols",
        "--format",
        "tsv",
        "東京\tに\n行く\\確認",
    )
    rows = [line.split("\t") for line in result.stdout.splitlines()]
    assert rows
    assert all(len(row) == 5 for row in rows), rows
    assert "\\t" in result.stdout
    assert "\\n" in result.stdout
    assert "\\\\" in result.stdout


def assert_score_precision(cli: Path) -> None:
    result = run_cli(
        cli,
        "analyze",
        "--skip-core-dictionary",
        "--skip-bundled-user-dictionary",
        "--format",
        "json",
        "へ",
    )
    payload = json.loads(result.stdout)
    assert re.findall(r'"score": ([^,}\s]+)', result.stdout) == ["0.200000003"]
    assert [morpheme["score"] for morpheme in payload["morphemes"]] == [0.200000003]


def assert_tag_filters(cli: Path) -> None:
    nouns = run_cli(
        cli,
        "analyze",
        "--format",
        "tags",
        "--tag-pos-filter",
        "NOUN",
        "--tag-min-length",
        "1",
        "東京を走る",
    )
    noun_lines = [line.split("\t") for line in nouns.stdout.splitlines()]
    assert noun_lines
    assert {parts[1] for parts in noun_lines} == {"NOUN"}

    nouns_and_verbs = run_cli(
        cli,
        "analyze",
        "--format",
        "tags",
        "--tag-pos-filter",
        "NOUN",
        "--tag-pos-filter",
        "VERB",
        "--tag-min-length",
        "1",
        "東京を走る",
    )
    selected_pos = {line.rsplit("\t", 1)[1] for line in nouns_and_verbs.stdout.splitlines()}
    assert selected_pos == {"NOUN", "VERB"}

    no_basic = run_cli(
        cli,
        "analyze",
        "--format",
        "tags",
        "--tag-exclude-basic",
        "--tag-min-length",
        "1",
        "りんご 東京",
    )
    tags = {line.split("\t", 1)[0] for line in no_basic.stdout.splitlines()}
    assert "東京" in tags
    assert "りんご" not in tags


def assert_chasen_contract(cli: Path) -> None:
    result = run_cli(cli, "analyze", "--format", "chasen", "りんごを食べる")
    assert result.stdout == (
        "りんご\t*\tりんご\t名詞\t*\t*\n"
        "を\t*\tを\t助詞\t*\t*\n"
        "食べる\t*\t食べる\t動詞\t一段\t終止形\n"
        "EOS\n"
    )

    na_adjective = run_cli(cli, "analyze", "--format", "chasen", "論理的な説明")
    assert "論理的\t*\t論理的\t形容詞\tナ形容詞\t終止形\n" in na_adjective.stdout

    auxiliary = run_cli(cli, "analyze", "--format", "chasen", "書かなかった")
    assert "なかっ\t*\tない\t助動詞\t*\t連用形\n" in auxiliary.stdout


def assert_debug_and_compare_preserve_output_formats(cli: Path) -> None:
    text = "2024年12月23日にhttps://example.com/abcを見た。価格は1,000円。"
    for output_format in ("tsv", "json", "chasen", "tags"):
        production = run_cli(cli, "analyze", "--format", output_format, text)
        debug = run_cli(cli, "analyze", "--debug", "--format", output_format, text)
        assert debug.stdout == production.stdout
        assert "=== Debug Mode ===" in debug.stderr
        assert "=== Lattice Candidates ===" in debug.stderr


def assert_single_test_contract(cli: Path) -> None:
    result = run_cli(cli, "test", "--verbose", "東京へ", "--expect", "東京")
    assert "PASS: 東京へ" in result.stdout, result.stdout
    run_cli(
        cli,
        "test",
        "--tag-min-length",
        "1",
        "--tag-no-exclude-particles",
        "東京へ",
        "--expect",
        "東京,へ",
    )


def assert_cli_failure_and_structural_diff_contract(cli: Path) -> None:

    no_dict_args = run_cli(cli, "dict", expect_success=False)
    assert no_dict_args.returncode == 2
    assert no_dict_args.stdout == ""
    assert "Usage:" in no_dict_args.stderr

    unknown_test = run_cli(
        cli,
        "test",
        "東京",
        "--expect",
        "東京",
        "--unexpected",
        expect_success=False,
    )
    assert unknown_test.returncode == 2
    assert "unknown test option" in unknown_test.stderr
    unknown_benchmark = run_cli(
        cli,
        "test",
        "benchmark",
        "--iterrations=1",
        expect_success=False,
    )
    assert unknown_benchmark.returncode == 2
    assert "unknown benchmark option" in unknown_benchmark.stderr
    missing_test_file = run_cli(cli, "test", "--file", expect_success=False)
    assert missing_test_file.returncode == 2
    assert "missing value for --file" in missing_test_file.stderr

    with tempfile.TemporaryDirectory(prefix="suzume-cli-contract-") as temp_name:
        temp_dir = Path(temp_name)
        dictionary = temp_dir / "lemma.tsv"
        dictionary.write_text("東京\tNOUN\t\tとうきょう\n", encoding="utf-8")
        production = run_cli(cli, "analyze", "--dictionary", str(dictionary), "--format", "json", "東京")
        compared = run_cli(cli, "analyze", "--compare", "--dictionary", str(dictionary), "--format", "json", "東京")
        assert compared.stdout == production.stdout
        assert "[Diff]" in compared.stderr
        assert "- 0:" in compared.stderr
        assert "+ 0:" in compared.stderr

        crlf_tests = temp_dir / "tests.tsv"
        crlf_tests.write_bytes("東京へ\t東京\r\n".encode())
        crlf_result = run_cli(cli, "test", "--file", str(crlf_tests))
        assert "1 passed, 0 failed" in crlf_result.stdout

        directory_result = run_cli(cli, "dict", "info", str(temp_dir), expect_success=False)
        assert "not a regular file" in directory_result.stderr

        (temp_dir / "first.tsv").write_text("東京\tNOUN\n", encoding="utf-8")
        (temp_dir / "second.tsv").write_text("りんご\tNOUN\n", encoding="utf-8")
        bare_glob = run_cli(
            cli,
            "dict",
            "compile",
            "*.tsv",
            cwd=temp_dir,
            expect_success=False,
        )
        assert "requires an explicit output" in bare_glob.stderr

        shard_dir = temp_dir / "shards"
        shard_dir.mkdir()
        (shard_dir / "alpha.tsv").write_text("青空庭園\tNOUN\n", encoding="utf-8")
        (shard_dir / "beta.tsv").write_text("東京果樹園\tNOUN\n", encoding="utf-8")
        inferred_output = temp_dir / "shards.dic"
        inferred_output.write_bytes(b"must remain untouched")
        subset_glob = run_cli(
            cli,
            "dict",
            "compile",
            str(shard_dir / "alpha*.tsv"),
            expect_success=False,
        )
        assert "subset glob requires an explicit output" in subset_glob.stderr
        assert inferred_output.read_bytes() == b"must remain untouched"
        explicit_subset_output = temp_dir / "subset.dic"
        run_cli(cli, "dict", "compile", str(shard_dir / "alpha*.tsv"), str(explicit_subset_output))
        assert explicit_subset_output.is_file()
        inferred_output.unlink()
        run_cli(cli, "dict", "compile", str(shard_dir / "*.tsv"))
        assert inferred_output.is_file()
        inferred_single_output = shard_dir / "alpha.dic"
        inferred_single_output.write_bytes(b"must remain untouched")
        single_source = run_cli(cli, "dict", "compile", str(shard_dir / "alpha.tsv"), expect_success=False)
        assert "refusing to overwrite inferred dictionary output" in single_source.stderr
        assert inferred_single_output.read_bytes() == b"must remain untouched"


def assert_bundled_user_dictionary_contract(cli: Path) -> None:
    result = run_cli(cli, "analyze", "--format", "json", "セーラー服を着る")
    morphemes = json.loads(result.stdout)["morphemes"]
    assert morphemes[0]["surface"] == "セーラー服"
    assert morphemes[0]["is_user_dict"] is True

    engine_owned_cases = {
        "キャミソールとタンクトップを選ぶ": ["キャミソール", "と", "タンクトップ", "を", "選ぶ"],
        "いつの間にか終わった": ["いつ", "の", "間", "に", "か", "終わっ", "た"],
        "つめあわせを作る": ["つめあわせ", "を", "作る"],
        "病みかわが好き": ["病みかわ", "が", "好き"],
    }
    for input_text, expected_surfaces in engine_owned_cases.items():
        analyzed = run_cli(cli, "analyze", "--format", "json", input_text)
        analyzed_morphemes = json.loads(analyzed.stdout)["morphemes"]
        assert [morpheme["surface"] for morpheme in analyzed_morphemes] == expected_surfaces
        assert all(not morpheme["is_user_dict"] for morpheme in analyzed_morphemes)


def assert_dictionary_contract(cli: Path) -> None:
    for word, required_forms in (
        ("あきらめる", ("あきらめよ- (意志形)", "あきらめれ- (仮定形)", "あきらめろ- (命令形)")),
        ("する", ("しよ- (意志形)", "すれ- (仮定形)", "しろ- (命令形)", "せよ- (命令形)")),
        ("来る", ("来よ- (意志形)", "来れ- (仮定形)", "来い- (命令形)")),
    ):
        lookup = run_cli(cli, "dict", "lookup", word)
        for form in required_forms:
            assert form in lookup.stdout

    with tempfile.TemporaryDirectory(prefix="suzume-native-cli-") as temp_name:
        temp_dir = Path(temp_name)
        source = temp_dir / "compiled.tsv"
        compiled = temp_dir / "compiled.dic"
        source.write_text("東京テスト\tNOUN\n", encoding="utf-8")

        created = temp_dir / "created.tsv"
        run_cli(cli, "dict", "new", str(created))
        created_header = created.read_text(encoding="utf-8")
        assert "# Format: surface<TAB>pos<TAB>conj_type<TAB>base_form\n" in created_header
        assert "reading<TAB>cost" not in created_header
        for value in ("CONJUNCTION", "DETERMINER", "PRONOUN", "PREFIX", "SUFFIX", "INTERJECTION"):
            assert value in created_header
        assert "FAMILY, GIVEN" in created_header
        created.write_text(created_header + "東京公園\tNOUN\n", encoding="utf-8")
        run_cli(cli, "dict", "compile", str(created), str(temp_dir / "created.dic"))

        filter_source = temp_dir / "filter.tsv"
        filter_binary = temp_dir / "filter.dic"
        filter_source.write_text("東京公園\tNOUN\nりん\tNOUN\n", encoding="utf-8")
        filtered = run_cli(
            cli,
            "dict",
            "compile",
            "--filter-trivial",
            str(filter_source),
            str(filter_binary),
        )
        assert "filtered 1 trivial entries (kept 1)" in filtered.stderr

        iku_lookup = run_cli(cli, "dict", "lookup", "行く")
        assert "行っ- (音便形)" in iku_lookup.stdout
        assert "行い- (音便形)" not in iku_lookup.stdout

        run_cli(cli, "dict", "compile", str(source), str(compiled))
        assert compiled.is_file()
        for listed in (run_cli(cli, "dict", "list", str(source)), run_cli(cli, "dict", "list", str(compiled))):
            rows = [line for line in listed.stdout.splitlines() if not line.startswith("(")]
            assert rows
            assert all(len(row.split("\t")) == 3 for row in rows), listed.stdout
        binary_search = run_cli(cli, "dict", "search", str(compiled), "東京*")
        assert "東京テスト\tNOUN" in binary_search.stdout
        assert "(1 matches)" in binary_search.stdout

        uppercase_binary = temp_dir / "uppercase.DIC"
        run_cli(cli, "dict", "compile", str(source), str(uppercase_binary))
        uppercase_list = run_cli(cli, "dict", "list", str(uppercase_binary))
        assert "東京テスト\tNOUN" in uppercase_list.stdout
        uppercase_search = run_cli(cli, "dict", "search", str(uppercase_binary), "東京*")
        assert "東京テスト\tNOUN" in uppercase_search.stdout

        default_dump = compiled.with_suffix(".dump.tsv")
        run_cli(cli, "dict", "decompile", str(compiled))
        assert default_dump.is_file()
        assert source.read_text(encoding="utf-8") == "東京テスト\tNOUN\n"
        refused_dump = run_cli(cli, "dict", "decompile", str(compiled), expect_success=False)
        assert "refusing to overwrite" in refused_dump.stderr
        run_cli(cli, "dict", "decompile", str(compiled), "--force")

        explicit_dump = temp_dir / "explicit.tsv"
        explicit_dump.write_text("preserve me\n", encoding="utf-8")
        refused_explicit = run_cli(
            cli,
            "dict",
            "decompile",
            str(compiled),
            str(explicit_dump),
            expect_success=False,
        )
        assert explicit_dump.read_text(encoding="utf-8") == "preserve me\n"
        assert "use --force" in refused_explicit.stderr
        run_cli(cli, "dict", "decompile", str(compiled), str(explicit_dump), "--force")
        assert explicit_dump.read_text(encoding="utf-8") != "preserve me\n"

        binary_test = run_cli(
            cli,
            "test",
            "--verbose",
            "--dictionary",
            str(compiled),
            "東京テスト",
            "--expect",
            "東京テスト",
        )
        assert "PASS: 東京テスト" in binary_test.stdout, binary_test.stdout

        result = run_cli(cli, "analyze", "--dictionary", str(compiled), "--format", "json", "東京テスト")
        morphemes = json.loads(result.stdout)["morphemes"]
        matching = [morpheme for morpheme in morphemes if morpheme["surface"] == "東京テスト"]
        assert matching
        assert matching[0]["is_user_dict"] is True

        adjective_source = temp_dir / "adjective.tsv"
        adjective_source.write_text("静か\tADJECTIVE\tNA_ADJ\n", encoding="utf-8")
        adjective_list = run_cli(cli, "dict", "list", str(adjective_source), "--pos=ADJECTIVE")
        assert "静か\tADJ" in adjective_list.stdout
        interactive_list = run_cli(
            cli,
            "dict",
            "-i",
            str(adjective_source),
            input_text="list --pos=ADJECTIVE\nlist --pos=NOT_A_POS\nquit\n",
        )
        assert "静か\tADJ" in interactive_list.stdout
        assert "invalid --pos value: NOT_A_POS" in interactive_list.stderr
        invalid_pos = run_cli(
            cli,
            "dict",
            "list",
            str(adjective_source),
            "--pos=NOT_A_POS",
            expect_success=False,
        )
        assert invalid_pos.returncode == 2
        assert invalid_pos.stderr == "suzume-cli: error: invalid --pos value: NOT_A_POS\n", invalid_pos.stderr
        spaced_list = run_cli(cli, "dict", "list", str(adjective_source), "--pos", "ADJECTIVE", "--limit", "1")
        assert spaced_list.stdout == adjective_list.stdout, spaced_list.stdout
        for option in ("--pos", "--pattern", "--limit"):
            missing = run_cli(cli, "dict", "list", str(adjective_source), option, expect_success=False)
            assert missing.returncode == 2
            assert missing.stderr == f"suzume-cli: error: missing value for {option}\n", missing.stderr

        second_binary_source = temp_dir / "second-binary.tsv"
        second_binary = temp_dir / "second-binary.dic"
        second_binary_source.write_text("テスト公園\tNOUN\n", encoding="utf-8")
        run_cli(cli, "dict", "compile", str(second_binary_source), str(second_binary))
        additive_binary = run_cli(
            cli,
            "analyze",
            "--dictionary",
            str(compiled),
            "--dictionary",
            str(second_binary),
            "--format",
            "json",
            "東京テスト テスト公園",
        )
        additive_surfaces = {
            morpheme["surface"] for morpheme in json.loads(additive_binary.stdout)["morphemes"]
        }
        assert {"東京テスト", "テスト公園"} <= additive_surfaces

        first_text = temp_dir / "first.tsv"
        second_text = temp_dir / "second.tsv"
        first_text.write_text("りんご東京\tNOUN\n", encoding="utf-8")
        second_text.write_text("東京りんご\tNOUN\n", encoding="utf-8")
        mixed = run_cli(
            cli,
            "analyze",
            "--dictionary",
            str(first_text),
            "--dictionary",
            str(compiled),
            "--dictionary",
            str(second_text),
            "--format",
            "json",
            "東京テスト りんご東京 東京りんご",
        )
        mixed_morphemes = json.loads(mixed.stdout)["morphemes"]
        by_surface = {morpheme["surface"]: morpheme for morpheme in mixed_morphemes}
        for surface in ("東京テスト", "りんご東京", "東京りんご"):
            assert surface in by_surface, (surface, mixed_morphemes)
            assert by_surface[surface]["is_user_dict"] is True

        round_trip_source = temp_dir / "names.tsv"
        round_trip_source.write_text(
            "東京\tNOUN\tFAMILY\n"
            "テスト\tNOUN\tGIVEN\n",
            encoding="utf-8",
        )
        run_cli(cli, "dict", "-i", str(round_trip_source), input_text="save\nquit\n")
        saved_lines = {
            line
            for line in round_trip_source.read_text(encoding="utf-8").splitlines()
            if line and not line.startswith("#")
        }
        assert saved_lines == {
            "東京\tNOUN\tFAMILY",
            "テスト\tNOUN\tGIVEN",
        }
        run_cli(cli, "dict", "compile", str(round_trip_source), str(temp_dir / "names.dic"))

        tsv_list = run_cli(cli, "dict", "list", str(round_trip_source))
        binary_list = run_cli(cli, "dict", "list", str(temp_dir / "names.dic"))
        assert all(len(line.split("\t")) == 3 for line in tsv_list.stdout.splitlines() if "\t" in line)
        assert all(len(line.split("\t")) == 3 for line in binary_list.stdout.splitlines() if "\t" in line)

        interactive_help = run_cli(
            cli,
            "dict",
            "-i",
            str(round_trip_source),
            input_text="help\nquit\n",
        )
        assert "saves modified entries non-interactively" in interactive_help.stdout
        assert "confirms before discarding them in a terminal" in interactive_help.stdout

        rejected_import = run_cli(
            cli,
            "dict",
            "-i",
            str(round_trip_source),
            input_text=(
                f"import {round_trip_source} --skip-duplicate\n"
                f"import {round_trip_source} extra.tsv\n"
                "quit\n"
            ),
        )
        assert "unknown import option: --skip-duplicate" in rejected_import.stderr
        assert "unknown import option: extra.tsv" in rejected_import.stderr

        protected_source = temp_dir / "protected.dic"
        protected_content = "東京\tNOUN\n"
        protected_source.write_text(protected_content, encoding="utf-8")
        protected = run_cli(
            cli,
            "dict",
            "-i",
            str(protected_source),
            input_text=f"compile {protected_source}\nquit\n",
        )
        assert "refusing to overwrite dictionary source" in protected.stderr
        assert protected_source.read_text(encoding="utf-8") == protected_content

        wrong_extension = run_cli(
            cli,
            "dict",
            "-i",
            str(protected_source),
            input_text=f"compile {temp_dir / 'protected.tsv'}\nquit\n",
        )
        assert "output file must have .dic extension" in wrong_extension.stderr

        session_source = temp_dir / "session.tsv"
        session = run_cli(
            cli,
            "dict",
            "-i",
            str(session_source),
            input_text=(
                "add 青空りんご園 NOUN\n"
                "analyze 青空りんご園\n"
                "add 東京 VERB\n"
                "add 東京 NOUN FAMILY\n"
                "add テスト NOUN GIVEN\n"
                "list --limit=0\n"
                "quit\n"
            ),
        )
        assert "青空りんご園\tNOUN\t青空りんご園" in session.stdout
        assert "missing conjugation type: 東京" in session.stderr
        saved_session = session_source.read_text(encoding="utf-8")
        assert "東京\tNOUN\tFAMILY" in saved_session
        assert "テスト\tNOUN\tGIVEN" in saved_session

        eof_source = temp_dir / "eof.tsv"
        eof = run_cli(
            cli,
            "dict",
            "-i",
            str(eof_source),
            input_text="add 東京公園 NOUN\n",
        )
        assert "Saved unsaved changes at EOF" in eof.stderr
        assert "東京公園\tNOUN" in eof_source.read_text(encoding="utf-8")

        terminal_eof_source = temp_dir / "terminal-eof.tsv"
        assert_terminal_eof_saves_edits(cli, terminal_eof_source)

        quoted_source = temp_dir / "quoted.tsv"
        quoted = run_cli(
            cli,
            "dict",
            "-i",
            str(quoted_source),
            input_text="add '' NOUN\nadd '東京 NOUN\nquit\n",
        )
        assert "surface must not be empty" in quoted.stderr
        assert "unterminated quoted argument" in quoted.stderr
        assert not quoted_source.exists()

        metachar_surface = r"記号.^$+()[]{}|\終"
        wildcard_source = temp_dir / "wildcards.tsv"
        wildcard_source.write_text(
            f"{metachar_surface}\tNOUN\n"
            "{a\tNOUN\n"
            "a}\tNOUN\n",
            encoding="utf-8",
        )
        literal_match = run_cli(cli, "dict", "search", str(wildcard_source), metachar_surface)
        assert f"{metachar_surface}\tNOUN" in literal_match.stdout
        wildcard_match = run_cli(cli, "dict", "search", str(wildcard_source), "記号*終")
        assert f"{metachar_surface}\tNOUN" in wildcard_match.stdout
        for brace_surface in ("{a", "a}"):
            brace_match = run_cli(cli, "dict", "search", str(wildcard_source), brace_surface)
            assert f"{brace_surface}\tNOUN" in brace_match.stdout

        excessive_pattern = "*" * 65
        excessive_search = run_cli(
            cli,
            "dict",
            "search",
            str(wildcard_source),
            excessive_pattern,
            expect_success=False,
        )
        assert excessive_search.returncode != -6, excessive_search
        assert "too many '*'" in excessive_search.stderr

        interactive_excessive = run_cli(
            cli,
            "dict",
            "-i",
            str(wildcard_source),
            input_text=f"list --pattern={excessive_pattern}\nsearch {excessive_pattern}\nquit\n",
        )
        assert interactive_excessive.returncode == 0
        assert interactive_excessive.stderr.count("too many '*'") == 2

        data_root = temp_dir / "data-root"
        source_dir = data_root / "core"
        source_dir.mkdir(parents=True)
        source_lookup = source_dir / "lookup.tsv"
        source_lookup.write_text("東京テスト公園\tNOUN\n", encoding="utf-8")
        user_dir = data_root / "user"
        user_dir.mkdir()
        user_lookup = user_dir / "lookup.tsv"
        user_lookup.write_text("東京果樹園\tNOUN\n", encoding="utf-8")
        malformed_lookup = source_dir / "malformed.tsv"
        malformed_lookup.write_text("不正\tNOT_A_POS\n", encoding="utf-8")
        lookup_env = os.environ.copy()
        lookup_env["SUZUME_DATA_DIR"] = str(data_root)
        source_result = run_cli(
            cli,
            "dict",
            "lookup",
            "東京テスト公園",
            env=lookup_env,
        )
        assert "東京テスト公園\tNOUN" in source_result.stdout
        assert "failed to parse source TSV" in source_result.stderr
        assert malformed_lookup.name in source_result.stderr

        user_result = run_cli(cli, "dict", "lookup", "東京果樹園", env=lookup_env)
        assert "東京果樹園\tNOUN" in user_result.stdout
        binary_result = run_cli(cli, "dict", "lookup", "東京テスト", str(compiled), env=lookup_env)
        assert f"東京テスト\tNOUN [{compiled.name}]" in binary_result.stdout

        dash_named_source = temp_dir / "-dictionary.tsv"
        dash_named_source.write_text("東京辞書\tNOUN\n", encoding="utf-8")
        dash_named_result = run_cli(
            cli,
            "analyze",
            "--skip-bundled-user-dictionary",
            "--skip-core-dictionary",
            f"--dictionary={dash_named_source.name}",
            "--format",
            "json",
            "東京辞書",
            cwd=temp_dir,
        )
        dash_named_morphemes = json.loads(dash_named_result.stdout)["morphemes"]
        assert any(
            morpheme["surface"] == "東京辞書" and morpheme["is_user_dict"]
            for morpheme in dash_named_morphemes
        )


REMOVED_FLAGS = (
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
)

TAG_OPTIONS = (
    ("--tag-no-exclude-particles",),
    ("--tag-no-exclude-auxiliaries",),
    ("--tag-no-exclude-formal-nouns",),
    ("--tag-no-exclude-low-info",),
    ("--tag-exclude-basic",),
    ("--tag-no-remove-duplicates",),
    ("--tag-no-use-base-form",),
    ("--tag-min-length", "1"),
    ("--tag-max-tags", "3"),
    ("--tag-pos-filter", "NOUN"),
)


def tag_lines(result: subprocess.CompletedProcess[str]) -> list[tuple[str, str]]:
    return [tuple(line.split("\t")) for line in result.stdout.splitlines()]


def assert_flag_table_contract(cli: Path) -> None:
    for removed in REMOVED_FLAGS:
        result = run_cli(cli, removed, "東京", expect_success=False)
        assert result.returncode == 2, (removed, result.returncode)
        assert result.stderr == f"suzume-cli: error: unknown option: {removed}\n", result.stderr
    for removed in (("--tag-pos", "noun"), ("--dict", "user.tsv"), ("-f", "morpheme")):
        result = run_cli(cli, "-f", "tags", *removed, "東京", expect_success=False)
        assert result.returncode == 2, (removed, result.returncode)

    assert run_cli(cli, "--no-preserve-vu", "ヴァイオリン").stdout.startswith("バイオリン\t")
    assert run_cli(cli, "ヴァイオリン").stdout.startswith("ヴァイオリン\t")
    assert run_cli(cli, "--no-preserve-case", "ABC").stdout.startswith("abc\t")
    assert run_cli(cli, "ABC").stdout.startswith("ABC\t")
    for flag in ("--preserve-symbols", "--no-lemmatize", "--merge-compounds", "--skip-env-config"):
        run_cli(cli, flag, "東京へ行く")
    bare = run_cli(cli, "--skip-core-dictionary", "--skip-bundled-user-dictionary", "東京へ行く")
    assert bare.stdout

    base_forms = tag_lines(run_cli(cli, "-f", "tags", "東京へ走った"))
    surfaces = tag_lines(run_cli(cli, "-f", "tags", "--tag-no-use-base-form", "東京へ走った"))
    assert ("走る", "VERB") in base_forms
    assert ("走っ", "VERB") in surfaces

    particles = tag_lines(
        run_cli(cli, "-f", "tags", "--tag-min-length", "1", "--tag-no-exclude-particles", "東京へ")
    )
    assert ("へ", "PARTICLE") in particles
    assert ("へ", "PARTICLE") not in tag_lines(run_cli(cli, "-f", "tags", "--tag-min-length", "1", "東京へ"))

    duplicates = tag_lines(run_cli(cli, "-f", "tags", "--tag-no-remove-duplicates", "東京と東京"))
    assert duplicates.count(("東京", "NOUN")) == 2
    assert tag_lines(run_cli(cli, "-f", "tags", "東京と東京")).count(("東京", "NOUN")) == 1

    repeated = run_cli(cli, "-f", "tags", "--tag-pos-filter", "NOUN", "--tag-pos-filter=NOUN", "東京を走る")
    assert {pos for _, pos in tag_lines(repeated)} == {"NOUN"}
    for name in ("noun", "adjective", "Adj", ""):
        result = run_cli(cli, "-f", "tags", "--tag-pos-filter", name, "東京", expect_success=False)
        assert result.returncode == 2
        assert result.stderr == (
            f"suzume-cli: error: invalid --tag-pos-filter value: {name} "
            "(expected one of: NOUN, VERB, ADJ, ADV, PARTICLE, AUX)\n"
        ), result.stderr
    for options in TAG_OPTIONS:
        run_cli(cli, "-f", "tags", *options, "東京を走る")

    for name in ("Search", "fast"):
        result = run_cli(cli, "--mode", name, "東京", expect_success=False)
        assert result.returncode == 2


def assert_tag_options_require_tags_format(cli: Path) -> None:
    expected = "suzume-cli: error: --tag-* options require -f tags\n"
    for options in TAG_OPTIONS:
        for format_args in (("-f", "json"), ("-f", "tsv"), ("-f", "chasen"), ()):
            result = run_cli(cli, "analyze", *format_args, *options, "東京", expect_success=False)
            assert result.returncode == 2, (options, format_args)
            assert result.stdout == ""
            assert result.stderr == expected, (options, format_args, result.stderr)
    run_cli(cli, "test", "--tag-min-length", "1", "東京へ", "--expect", "東京")


def assert_input_contract(cli: Path) -> None:
    master, slave = pty.openpty()
    try:
        tty_result = subprocess.run([str(cli)], stdin=slave, capture_output=True, check=False, timeout=10)
    finally:
        os.close(slave)
        os.close(master)
    assert tty_result.returncode == 2, tty_result
    assert tty_result.stdout == b""
    assert tty_result.stderr == b"suzume-cli: error: no input text provided\n", tty_result.stderr

    for output_format in ("tsv", "json", "chasen", "tags"):
        operand = run_cli(cli, "-f", output_format, "")
        assert operand.stdout == "" and operand.stderr == "", (output_format, operand)
        for empty_input in ("", "\n", "\r\n", "\ufeff"):
            piped = run_cli(cli, "-f", output_format, input_text=empty_input)
            assert piped.stdout == "" and piped.stderr == "", (output_format, repr(empty_input), piped)

    dash_text = run_cli(cli, "--", "-東京")
    assert dash_text.stdout
    no_tags = run_cli(cli, "-f", "tags", "を")
    assert no_tags.stdout == ""


def assert_dictionary_loading_contract(cli: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="suzume-cli-load-") as temp_name:
        temp_dir = Path(temp_name)
        missing = temp_dir / "missing.tsv"
        failed = run_cli(cli, "-d", str(missing), "東京", expect_success=False)
        assert failed.returncode == 1
        assert failed.stdout == ""
        prefix = f"suzume-cli: error: failed to load dictionary: {missing}: "
        error_line = failed.stderr.splitlines()[-1]
        assert error_line.startswith(prefix) and len(error_line) > len(prefix), failed.stderr
        assert failed.stderr.count("suzume-cli: error:") == 1, failed.stderr

        first = temp_dir / "first.tsv"
        first.write_text("東京公園\tNOUN\nてすとの\tPARTICLE\n", encoding="utf-8")
        second = temp_dir / "second.tsv"
        second.write_text("てすとだ\tAUXILIARY\n", encoding="utf-8")
        loaded = run_cli(
            cli,
            "--skip-bundled-user-dictionary",
            "-d",
            str(first),
            "--dictionary",
            str(second),
            "東京公園",
        )
        warnings = [line for line in loaded.stderr.splitlines() if "closed-class POS" in line]
        assert len(warnings) == 2, loaded.stderr
        assert warnings[0].startswith("warning: Dictionary entry at line 2 uses closed-class POS PARTICLE")
        assert warnings[1].startswith("warning: Dictionary entry at line 1 uses closed-class POS AUX")


def assert_subcommand_option_contract(cli: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="suzume-cli-sub-") as temp_name:
        temp_dir = Path(temp_name)
        cases = temp_dir / "cases.tsv"
        cases.write_text("東京へ\t東京\n", encoding="utf-8")
        run_cli(cli, "test", "--file", str(cases))
        run_cli(cli, "test", f"--file={cases}")
        dash_f = run_cli(cli, "test", "-f", str(cases), expect_success=False)
        assert dash_f.returncode == 2
        assert dash_f.stderr == (
            f"suzume-cli: error: invalid format: {cases} (expected one of: tsv, json, chasen, tags)\n"
        ), dash_f.stderr

        corpus = temp_dir / "corpus.txt"
        corpus.write_text("東京へ行く\n", encoding="utf-8")
        run_cli(cli, "test", "benchmark", "--iterations=1", "--samples=1", "--warmup=0", "--file", str(corpus))
        run_cli(cli, "test", "benchmark", "--iterations", "1", "--samples", "1", "--warmup", "0", "--file", str(corpus))
        for bad_count in (("--iterations", "0"), ("--samples", "x"), ("--warmup", "-1")):
            bad = run_cli(cli, "test", "benchmark", f"{bad_count[0]}={bad_count[1]}", expect_success=False)
            assert bad.returncode == 2
            assert bad.stderr == f"suzume-cli: error: invalid {bad_count[0]} value: {bad_count[1]}\n", bad.stderr

        source = temp_dir / "interactive.tsv"
        source.write_text("東京公園\tNOUN\n", encoding="utf-8")
        run_cli(cli, "dict", "--interactive", str(source), input_text="quit\n")

        lookup = run_cli(cli, "dict", "lookup", "でし")
        assert "でし\tAUX\t(base_form: です)\n" in lookup.stdout, lookup.stdout
        assert "(lemma:" not in lookup.stdout


def assert_usage_error(cli: Path, args: tuple[str, ...], message: str) -> None:
    result = run_cli(cli, *args, expect_success=False)
    assert result.returncode == 2, (args, result.returncode, result.stderr)
    assert result.stdout == "", (args, result.stdout)
    assert result.stderr.splitlines()[0] == f"suzume-cli: error: {message}", (args, result.stderr)


def assert_usage_error_contract(cli: Path) -> None:
    """Every usage error in every command exits 2, and value options share one missing-value rule."""
    # A value option never takes a next argument that reads as an option.
    assert_usage_error(cli, ("-d", "--mode", "normal", "東京"), "missing value for -d")
    assert_usage_error(cli, ("--dictionary", "-x.tsv", "東京"), "missing value for --dictionary")
    assert_usage_error(cli, ("--dictionary", "--", "東京"), "missing value for --dictionary")
    assert_usage_error(cli, ("--dictionary=", "東京"), "missing value for --dictionary")
    assert_usage_error(cli, ("-f", "tags", "--tag-min-length", "-1", "東京"), "missing value for --tag-min-length")
    assert_usage_error(cli, ("test", "東京", "--expect", "--file"), "missing value for --expect")
    assert_usage_error(cli, ("test", "benchmark", "--iterations", "--samples=1"), "missing value for --iterations")
    # A bare "-" and an attached "--option=value" are values.
    for args, path in ((("-d", "-"), "-"), (("--dictionary=-x.tsv",), "-x.tsv")):
        result = run_cli(cli, *args, "東京", expect_success=False)
        assert result.returncode == 1, (args, result.returncode)
        assert result.stderr.splitlines()[-1].startswith(f"suzume-cli: error: failed to load dictionary: {path}: ")

    # Tag sizes cross the C ABI as uint32_t.
    run_cli(cli, "-f", "tags", "--tag-max-tags", "4294967295", "--tag-min-length=4294967295", "東京")
    for option in ("--tag-min-length", "--tag-max-tags"):
        assert_usage_error(cli, ("-f", "tags", option, "4294967296", "東京"), f"invalid {option} value: 4294967296")
        assert_usage_error(
            cli, ("-f", "tags", f"{option}=18446744073709551616", "東京"), f"invalid {option} value: 18446744073709551616"
        )

    assert_usage_error(cli, ("test", "--expect", "東京"), "no input text provided")
    assert_usage_error(cli, ("test", "東京"), "no expected tags provided (use --expect)")
    assert_usage_error(cli, ("dict", "info"), "usage: suzume-cli dict info <file>")
    assert_usage_error(cli, ("dict", "decompile", "a.dic", "b.tsv", "--bogus"), "unknown decompile option: --bogus")
    assert_usage_error(cli, ("dict", "compile", "--bogus", "a.tsv"), "unknown compile option: --bogus")
    assert_usage_error(cli, ("dict", "add", "東京"), "dict add is available only in interactive mode (use 'suzume-cli dict -i [file.tsv]')")
    # Interactive mode has one spelling, -i / --interactive.
    for alias in ("interactive", "edit"):
        assert_usage_error(cli, ("dict", alias), f"unknown dict subcommand: {alias}")


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {Path(sys.argv[0]).name} /path/to/suzume-cli", file=sys.stderr)
        return 2
    cli = Path(sys.argv[1]).resolve()
    if not cli.is_file():
        print(f"native CLI does not exist: {cli}", file=sys.stderr)
        return 2

    assert_invalid_arguments(cli)
    assert_version_contract(cli)
    assert_help_contract(cli)
    assert_stdin_contract(cli)
    assert_invalid_utf8_rejected(cli)
    assert_json_schema(cli)
    assert_offsets_slice_normalized_text(cli)
    assert_control_characters_do_not_corrupt_tsv(cli)
    assert_score_precision(cli)
    assert_tag_filters(cli)
    assert_chasen_contract(cli)
    assert_debug_and_compare_preserve_output_formats(cli)
    assert_single_test_contract(cli)
    assert_cli_failure_and_structural_diff_contract(cli)
    assert_bundled_user_dictionary_contract(cli)
    assert_dictionary_contract(cli)
    assert_flag_table_contract(cli)
    assert_tag_options_require_tags_format(cli)
    assert_input_contract(cli)
    assert_dictionary_loading_contract(cli)
    assert_subcommand_option_contract(cli)
    assert_usage_error_contract(cli)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
