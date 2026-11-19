"""Command-line interface for the Suzume Python package.

The command mirrors the native ``suzume-cli`` analyze command: the same option
table, usage-error messages, exit codes and output bytes, with only the program
name differing. ``_OPTIONS`` is the single flag registry; the parser, the
missing-value check and the help text all read it.
"""

from __future__ import annotations

import json
import sys
from collections.abc import Callable, Sequence
from dataclasses import dataclass, field
from pathlib import Path
from typing import BinaryIO

from . import _POS_FILTER_BITS, Mode, Morpheme, Suzume, SuzumeError, Tag, version

_PROG = "suzume"
_FORMATS = ("tsv", "json", "chasen", "tags")
_COMMANDS = ("analyze", "version", "help")
_CONJUGATING_POS = frozenset({"VERB", "ADJ", "AUX"})
_UTF8_BOM = b"\xef\xbb\xbf"
_SIZE_MAX = 2**64 - 1


class _UsageError(Exception):
    """A command-line mistake: reported on one line with exit status 2."""


@dataclass
class _Args:
    command: str = ""
    text: list[str] = field(default_factory=list)
    help: bool = False
    version: bool = False
    dictionary_paths: list[str] = field(default_factory=list)
    mode: str = Mode.NORMAL.value
    format: str = "tsv"
    preserve_vu: bool = True
    preserve_case: bool = True
    preserve_symbols: bool = False
    lemmatize: bool = True
    merge_compounds: bool = False
    skip_env_config: bool = False
    skip_core_dictionary: bool = False
    skip_bundled_user_dictionary: bool = False
    tag_exclude_particles: bool = True
    tag_exclude_auxiliaries: bool = True
    tag_exclude_formal_nouns: bool = True
    tag_exclude_low_info: bool = True
    tag_exclude_basic: bool = False
    tag_remove_duplicates: bool = True
    tag_use_base_form: bool = True
    tag_pos_filter: list[str] = field(default_factory=list)
    tag_min_length: int = 2
    tag_max_tags: int = 0
    tag_option_given: bool = False


def _choice(value: str, choices: Sequence[str], label: str) -> str:
    if value not in choices:
        expected = ", ".join(choices[:-1]) + f", or {choices[-1]}"
        raise _UsageError(f"invalid {label}: {value} (expected {expected})")
    return value


def _size(option: str, value: str) -> int:
    if not value.isascii() or not value.isdigit() or int(value) > _SIZE_MAX:
        raise _UsageError(f"invalid {option} value: {value}")
    return int(value)


def _flag(name: str, set_to: bool) -> Callable[[_Args, str], None]:
    return lambda args, _value: setattr(args, name, set_to)


@dataclass(frozen=True)
class _Option:
    long_name: str
    short_name: str
    metavar: str
    help: str
    apply: Callable[[_Args, str], None]

    @property
    def takes_value(self) -> bool:
        return bool(self.metavar)


_MODES = tuple(mode.value for mode in Mode)
_TAG_NAMES = tuple(_POS_FILTER_BITS)

_OPTIONS = (
    _Option("--help", "-h", "", "Show this help", _flag("help", True)),
    _Option("--version", "-v", "", "Show the version", _flag("version", True)),
    _Option(
        "--dictionary",
        "-d",
        "PATH",
        "Load a user dictionary; .dic is binary, anything else source (repeatable)",
        lambda args, value: args.dictionary_paths.append(value),
    ),
    _Option(
        "--mode",
        "-m",
        "MODE",
        "Analysis mode: normal, search, split",
        lambda args, value: setattr(args, "mode", _choice(value, _MODES, "mode")),
    ),
    _Option(
        "--format",
        "-f",
        "FMT",
        "Output format: tsv (default), json, chasen, tags",
        lambda args, value: setattr(args, "format", _choice(value, _FORMATS, "format")),
    ),
    _Option(
        "--skip-core-dictionary",
        "",
        "",
        "Skip the auto-loaded core.dic",
        _flag("skip_core_dictionary", True),
    ),
    _Option(
        "--skip-bundled-user-dictionary",
        "",
        "",
        "Skip only the bundled user.dic; -d dictionaries still load",
        _flag("skip_bundled_user_dictionary", True),
    ),
    _Option(
        "--skip-env-config",
        "",
        "",
        "Ignore scorer configuration environment variables",
        _flag("skip_env_config", True),
    ),
    _Option(
        "--no-preserve-vu",
        "",
        "",
        "Normalize ヴ to ビ etc. (default: preserve)",
        _flag("preserve_vu", False),
    ),
    _Option(
        "--no-preserve-case",
        "",
        "",
        "Convert ASCII to lowercase (default: preserve)",
        _flag("preserve_case", False),
    ),
    _Option(
        "--preserve-symbols",
        "",
        "",
        "Keep punctuation-like SYMBOL tokens (emoji and content symbols stay OTHER either way)",
        _flag("preserve_symbols", True),
    ),
    _Option(
        "--no-lemmatize",
        "",
        "",
        "Keep dictionary base forms without post-analysis correction",
        _flag("lemmatize", False),
    ),
    _Option(
        "--merge-compounds",
        "",
        "",
        "Merge consecutive noun compounds",
        _flag("merge_compounds", True),
    ),
    _Option(
        "--tag-no-exclude-particles",
        "",
        "",
        "Keep particles in tag output",
        _flag("tag_exclude_particles", False),
    ),
    _Option(
        "--tag-no-exclude-auxiliaries",
        "",
        "",
        "Keep auxiliaries in tag output",
        _flag("tag_exclude_auxiliaries", False),
    ),
    _Option(
        "--tag-no-exclude-formal-nouns",
        "",
        "",
        "Keep formal nouns in tag output",
        _flag("tag_exclude_formal_nouns", False),
    ),
    _Option(
        "--tag-no-exclude-low-info",
        "",
        "",
        "Keep low-information words in tag output",
        _flag("tag_exclude_low_info", False),
    ),
    _Option(
        "--tag-exclude-basic",
        "",
        "",
        "Exclude hiragana-only basic words",
        _flag("tag_exclude_basic", True),
    ),
    _Option(
        "--tag-no-remove-duplicates",
        "",
        "",
        "Keep duplicate tags",
        _flag("tag_remove_duplicates", False),
    ),
    _Option(
        "--tag-no-use-base-form",
        "",
        "",
        "Use the surface instead of the base form for tags",
        _flag("tag_use_base_form", False),
    ),
    _Option(
        "--tag-pos-filter",
        "",
        "NAME",
        "Keep only NOUN, VERB, ADJ, ADV, PARTICLE, or AUX (repeatable)",
        lambda args, value: args.tag_pos_filter.append(
            _choice(value, _TAG_NAMES, "--tag-pos-filter value")
        ),
    ),
    _Option(
        "--tag-min-length",
        "",
        "N",
        "Minimum tag length (default: 2)",
        lambda args, value: setattr(args, "tag_min_length", _size("--tag-min-length", value)),
    ),
    _Option(
        "--tag-max-tags",
        "",
        "N",
        "Maximum tags (default: 0, unlimited)",
        lambda args, value: setattr(args, "tag_max_tags", _size("--tag-max-tags", value)),
    ),
)
_OPTIONS_BY_NAME = {
    name: option for option in _OPTIONS for name in (option.long_name, option.short_name) if name
}


def _help_text() -> str:
    def label(option: _Option) -> str:
        names = (
            f"{option.short_name}, {option.long_name}" if option.short_name else option.long_name
        )
        return f"{names} {option.metavar}".rstrip()

    width = max(len(label(option)) for option in _OPTIONS) + 2
    options = "\n".join(f"  {label(option):<{width}}{option.help}" for option in _OPTIONS)
    return f"""{_PROG} - Tokenize Japanese text

Usage:
  {_PROG} [analyze] [options] [text...]
  {_PROG} version

Text operands are joined with spaces; with none, text is read from stdin.

Options:
{options}

Long options that take a value also accept --option=value.
--tag-* options require -f tags.

Output Formats:
  tsv      surface TAB pos TAB base_form TAB start TAB end (default)
  json     JSON format
  chasen   ChaSen-like format (Japanese POS, conjugation info)
  tags     tag TAB pos, one per line
  TAB formats escape backslash, TAB, CR, and LF as \\\\, \\t, \\r, and \\n
  Morpheme start/end offsets refer to normalized text; JSON includes it as normalized_text
"""


def _match(arg: str) -> tuple[_Option | None, str | None]:
    """Return the option ``arg`` names and its attached ``--name=value`` value, if any."""
    option = _OPTIONS_BY_NAME.get(arg)
    if option is not None or not arg.startswith("--") or "=" not in arg:
        return option, None
    name, value = arg.split("=", 1)
    option = _OPTIONS_BY_NAME.get(name)
    if option is None or not option.takes_value:
        return None, None
    return option, value


def _parse(argv: Sequence[str]) -> _Args:
    args = _Args()
    idx = 0
    while idx < len(argv):
        arg = argv[idx]
        idx += 1
        if arg == "--":
            args.command = args.command or "analyze"
            args.text.extend(argv[idx:])
            break
        if not arg.startswith("-"):
            if not args.command and arg in _COMMANDS:
                args.command = arg
            else:
                args.command = args.command or "analyze"
                args.text.append(arg)
            continue

        option, value = _match(arg)
        if option is None:
            raise _UsageError(f"unknown option: {arg}")
        if value is not None:
            if not value:
                raise _UsageError(f"missing value for {option.long_name}")
        elif option.takes_value:
            if idx >= len(argv) or argv[idx] == "--" or argv[idx] in _OPTIONS_BY_NAME:
                raise _UsageError(f"missing value for {arg}")
            value = argv[idx]
            idx += 1
        option.apply(args, value or "")
        if option.long_name.startswith("--tag-"):
            args.tag_option_given = True

    args.command = args.command or "analyze"
    if (
        args.command == "analyze"
        and not args.help
        and args.tag_option_given
        and args.format != "tags"
    ):
        raise _UsageError("--tag-* options require -f tags")
    return args


def _input_bytes(parts: Sequence[str], stream: BinaryIO, is_terminal: bool) -> bytes:
    if parts:
        # Undecodable argv bytes arrive as surrogate escapes; restore them for validation.
        data = " ".join(parts).encode("utf-8", "surrogateescape")
    elif is_terminal:
        raise _UsageError("no input text provided")
    else:
        data = stream.read()
    data = data.removeprefix(_UTF8_BOM)
    if data.endswith(b"\n"):
        data = data[:-1]
        if data.endswith(b"\r"):
            data = data[:-1]
    return data


def _tab_escape(value: str) -> str:
    """Escape backslash and record delimiters in one-line TAB output."""
    return (
        value.replace("\\", "\\\\").replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n")
    )


def _conjugation(morpheme: Morpheme) -> tuple[str | None, str | None]:
    """Return the conjugation labels of a VERB/ADJ/AUX morpheme; ``None`` for every other POS."""
    if morpheme.pos not in _CONJUGATING_POS:
        return None, None
    return morpheme.conj_type or None, morpheme.conj_form or None


def _json_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=False)


def _json_morpheme(morpheme: Morpheme) -> str:
    """Render one morpheme as the single-line JSON object both CLIs print."""
    conj_type, conj_form = _conjugation(morpheme)
    fields = (
        ("surface", _json_string(morpheme.surface)),
        ("pos", _json_string(morpheme.pos)),
        ("base_form", _json_string(morpheme.base_form)),
        ("pos_ja", _json_string(morpheme.pos_ja)),
        ("conj_type", "null" if conj_type is None else _json_string(conj_type)),
        ("conj_form", "null" if conj_form is None else _json_string(conj_form)),
        ("extended_pos", _json_string(morpheme.extended_pos)),
        ("start", str(morpheme.start)),
        ("end", str(morpheme.end)),
        ("is_user_dict", json.dumps(morpheme.is_user_dict)),
        ("is_formal_noun", json.dumps(morpheme.is_formal_noun)),
        ("is_low_info", json.dumps(morpheme.is_low_info)),
        ("is_unknown", json.dumps(morpheme.is_unknown)),
        ("is_from_dictionary", json.dumps(morpheme.is_from_dictionary)),
        ("score", format(morpheme.score, ".9g")),
    )
    return "{" + ", ".join(f'"{key}": {value}' for key, value in fields) + "}"


def _json_document(text: str, normalized_text: str, morphemes: Sequence[Morpheme]) -> str:
    rows = ",\n".join(f"    {_json_morpheme(morpheme)}" for morpheme in morphemes)
    return (
        "{\n"
        f'  "input": {_json_string(text)},\n'
        f'  "normalized_text": {_json_string(normalized_text)},\n'
        '  "morphemes": [\n' + (rows + "\n" if rows else "") + "  ]\n}\n"
    )


def _tsv_line(morpheme: Morpheme) -> str:
    return (
        f"{_tab_escape(morpheme.surface)}\t{morpheme.pos}\t{_tab_escape(morpheme.base_form)}"
        f"\t{morpheme.start}\t{morpheme.end}"
    )


def _chasen_line(morpheme: Morpheme) -> str:
    conj_type, conj_form = _conjugation(morpheme)
    return (
        f"{_tab_escape(morpheme.surface)}\t*\t{_tab_escape(morpheme.base_form)}"
        f"\t{morpheme.pos_ja}\t{conj_type or '*'}\t{conj_form or '*'}"
    )


def _tag_line(tag: Tag) -> str:
    return f"{_tab_escape(tag.tag)}\t{tag.pos}"


class _CommandError(Exception):
    """A runtime failure: reported on one line with exit status 1."""


def _load_dictionary(analyzer: Suzume, path: str) -> None:
    binary = path.lower().endswith(".dic")
    try:
        data = Path(path).read_bytes()
    except OSError:
        kind = "binary dictionary" if binary else "dictionary"
        detail = f"Failed to open {kind} file: {path}"
    else:
        try:
            if binary:
                analyzer.load_binary_dictionary(data)
            else:
                analyzer.load_user_dictionary(data.decode("utf-8", "surrogateescape"))
            return
        except (SuzumeError, ValueError) as error:
            detail = str(error)
    raise _CommandError(f"failed to load dictionary: {path}: {detail}")


def _analyze(args: _Args, data: bytes) -> str:
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError:
        raise _CommandError("invalid UTF-8 input") from None
    if not text:
        return ""

    with Suzume(
        mode=args.mode,
        preserve_vu=args.preserve_vu,
        preserve_case=args.preserve_case,
        preserve_symbols=args.preserve_symbols,
        lemmatize=args.lemmatize,
        merge_compounds=args.merge_compounds,
        skip_env_config=args.skip_env_config,
        skip_core_dictionary=args.skip_core_dictionary,
        skip_bundled_user_dictionary=args.skip_bundled_user_dictionary,
    ) as analyzer:
        # Warnings accumulate across loads; print each once, after the load that produced it.
        printed = 0

        def print_new_warnings() -> None:
            nonlocal printed
            warnings = analyzer.dictionary_warnings
            for warning in warnings[printed:]:
                print(f"warning: {warning}", file=sys.stderr)
            printed = len(warnings)

        print_new_warnings()
        for path in args.dictionary_paths:
            _load_dictionary(analyzer, path)
            print_new_warnings()

        if args.format == "tags":
            tags = analyzer.generate_tags(
                text,
                pos_filter=tuple(args.tag_pos_filter),
                exclude_particles=args.tag_exclude_particles,
                exclude_auxiliaries=args.tag_exclude_auxiliaries,
                exclude_formal_nouns=args.tag_exclude_formal_nouns,
                exclude_low_info=args.tag_exclude_low_info,
                exclude_basic=args.tag_exclude_basic,
                remove_duplicates=args.tag_remove_duplicates,
                use_base_form=args.tag_use_base_form,
                min_length=args.tag_min_length,
                max_tags=args.tag_max_tags,
            )
            return "".join(f"{_tag_line(tag)}\n" for tag in tags)

        analysis = analyzer.analyze_with_normalized_text(text)
    morphemes = analysis.morphemes
    if args.format == "json":
        return _json_document(text, analysis.normalized_text, morphemes)
    if args.format == "chasen":
        return "".join(f"{_chasen_line(morpheme)}\n" for morpheme in morphemes) + "EOS\n"
    return "".join(f"{_tsv_line(morpheme)}\n" for morpheme in morphemes)


def main(argv: Sequence[str] | None = None) -> int:
    """Run the CLI and return a process exit code."""
    try:
        args = _parse(sys.argv[1:] if argv is None else argv)
    except _UsageError as error:
        print(f"{_PROG}: error: {error}", file=sys.stderr)
        return 2
    if args.version or args.command == "version":
        print(f"{_PROG} {version()}")
        return 0
    if args.help or args.command == "help":
        sys.stdout.write(_help_text())
        return 0

    try:
        data = _input_bytes(args.text, sys.stdin.buffer, sys.stdin.isatty())
    except _UsageError as error:
        print(f"{_PROG}: error: {error}", file=sys.stderr)
        return 2
    try:
        sys.stdout.write(_analyze(args, data))
    except (_CommandError, OSError, SuzumeError, ValueError) as error:
        print(f"{_PROG}: error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
