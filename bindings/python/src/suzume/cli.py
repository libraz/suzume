"""Command-line interface for the Suzume Python package."""

from __future__ import annotations

import argparse
import json
import sys
from collections.abc import Sequence
from pathlib import Path
from typing import Any, TextIO

from . import Mode, Morpheme, Suzume, SuzumeError, Tag, version

_FORMATS = ("morpheme", "tags", "json", "tsv", "chasen")
_TAG_POS = ("noun", "verb", "adjective", "adverb")
_OPTIONS_WITH_VALUES = {
    "-d",
    "--dict",
    "-m",
    "--mode",
    "-f",
    "--format",
    "--tag-pos",
    "--tag-min-length",
    "--tag-max-tags",
}
# Short options accept an attached value (``-dFILE``); long ones only ``--opt=value``.
_OPTIONS_WITH_ATTACHED_VALUES = tuple(
    option if not option.startswith("--") else f"{option}=" for option in _OPTIONS_WITH_VALUES
)
_FLAG_OPTIONS = {
    "--normalize-vu",
    "--lowercase",
    "--preserve-symbols",
    "--no-lemmatize",
    "--merge-compounds",
    "--no-core-dict",
    "--no-user-dict",
    "--skip-env-config",
    "--tag-exclude-basic",
    "--tag-use-surface",
    "--include-particles",
    "--include-auxiliaries",
    "--include-formal-nouns",
    "--include-low-info",
    "--tag-keep-duplicates",
}


def _nonnegative_int(value: str) -> int:
    parsed = int(value)
    if parsed < 0:
        raise argparse.ArgumentTypeError("must be zero or greater")
    return parsed


def _add_analysis_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("text", nargs="*", help="Text to analyze (reads stdin when omitted)")
    parser.add_argument(
        "-d",
        "--dict",
        dest="dictionary_paths",
        action="append",
        default=[],
        metavar="PATH",
        help="Load a text or compiled .dic user dictionary (repeatable)",
    )
    parser.add_argument(
        "-m",
        "--mode",
        choices=[mode.value for mode in Mode],
        default=Mode.NORMAL.value,
        help="Analysis mode (default: normal)",
    )
    parser.add_argument(
        "-f",
        "--format",
        choices=_FORMATS,
        default="morpheme",
        help="Output format (default: morpheme)",
    )
    parser.add_argument(
        "--normalize-vu",
        action="store_true",
        help="Normalize ヴ variants instead of preserving them",
    )
    parser.add_argument(
        "--lowercase",
        action="store_true",
        help="Normalize ASCII letters to lowercase",
    )
    parser.add_argument(
        "--preserve-symbols",
        action="store_true",
        help="Keep symbols and emoji in the output",
    )
    parser.add_argument(
        "--no-lemmatize",
        action="store_true",
        help="Keep source lemmas without post-analysis correction",
    )
    parser.add_argument(
        "--merge-compounds",
        action="store_true",
        help="Merge consecutive noun compounds",
    )
    parser.add_argument(
        "--no-core-dict",
        action="store_true",
        help="Disable the bundled core dictionary",
    )
    parser.add_argument(
        "--no-user-dict",
        action="store_true",
        help="Disable the bundled user dictionary",
    )
    parser.add_argument(
        "--skip-env-config",
        action="store_true",
        help="Ignore scorer configuration environment variables",
    )

    tag_group = parser.add_argument_group("tag output options")
    tag_group.add_argument(
        "--tag-pos",
        action="append",
        choices=_TAG_POS,
        default=[],
        metavar="POS",
        help="Include a POS in tag output (repeatable)",
    )
    tag_group.add_argument(
        "--tag-exclude-basic",
        action="store_true",
        help="Exclude basic hiragana words",
    )
    tag_group.add_argument(
        "--tag-use-surface",
        action="store_true",
        help="Use surface forms instead of lemmas",
    )
    tag_group.add_argument(
        "--tag-min-length",
        type=_nonnegative_int,
        default=2,
        metavar="N",
        help="Minimum tag length (default: 2)",
    )
    tag_group.add_argument(
        "--tag-max-tags",
        type=_nonnegative_int,
        default=0,
        metavar="N",
        help="Maximum number of tags (default: unlimited)",
    )
    tag_group.add_argument(
        "--include-particles",
        action="store_true",
        help="Include particles in tag output",
    )
    tag_group.add_argument(
        "--include-auxiliaries",
        action="store_true",
        help="Include auxiliaries in tag output",
    )
    tag_group.add_argument(
        "--include-formal-nouns",
        action="store_true",
        help="Include formal nouns in tag output",
    )
    tag_group.add_argument(
        "--include-low-info",
        action="store_true",
        help="Include low-information words in tag output",
    )
    tag_group.add_argument(
        "--tag-keep-duplicates",
        action="store_true",
        help="Keep duplicate tags",
    )


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="suzume",
        description="Suzume Japanese tokenizer (Python CLI)",
        epilog=(
            "Morpheme start/end offsets refer to normalized text; "
            "JSON includes it as normalized_text."
        ),
        allow_abbrev=False,
    )
    parser.add_argument(
        "-v",
        "--version",
        action="version",
        version=f"suzume {version()}",
    )
    _add_analysis_arguments(parser)
    return parser


def _without_explicit_command(argv: Sequence[str]) -> list[str]:
    """Remove a leading command token while preserving text after ``--``."""
    args = list(argv)
    idx = 0
    while idx < len(args):
        arg = args[idx]
        if arg == "--":
            return args
        if arg in _OPTIONS_WITH_VALUES:
            idx += 2
            continue
        if arg.startswith(_OPTIONS_WITH_ATTACHED_VALUES):
            idx += 1
            continue
        if arg in _FLAG_OPTIONS:
            idx += 1
            continue
        if arg == "analyze":
            del args[idx]
        elif arg == "version":
            args[idx] = "--version"
        return args
    return args


def _has_standalone_version_option(argv: Sequence[str]) -> bool:
    """Return whether a version flag occurs as an option, not text or a value."""
    index = 0
    while index < len(argv):
        argument = argv[index]
        if argument == "--":
            return False
        if argument in _OPTIONS_WITH_VALUES:
            index += 2
            continue
        if argument.startswith(_OPTIONS_WITH_ATTACHED_VALUES):
            index += 1
            continue
        if argument in {"--version", "-v"}:
            return True
        index += 1
    return False


def _read_text(parts: Sequence[str], stream: TextIO) -> str:
    if parts:
        text = " ".join(parts)
    elif stream.isatty():
        return ""
    else:
        text = stream.read()

    if text.startswith("\ufeff"):
        text = text[1:]
    if text.endswith("\r\n"):
        return text[:-2]
    if text.endswith("\n"):
        return text[:-1]
    return text


def _load_dictionaries(analyzer: Suzume, paths: Sequence[str]) -> None:
    for raw_path in paths:
        path = Path(raw_path)
        if path.suffix.lower() == ".dic":
            analyzer.load_binary_dict(path.read_bytes())
        else:
            analyzer.load_user_dict(path.read_text(encoding="utf-8"))


def _morpheme_dict(morpheme: Morpheme) -> dict[str, Any]:
    return {
        "surface": morpheme.surface,
        "pos": morpheme.pos,
        "lemma": morpheme.base_form,
        "start": morpheme.start,
        "end": morpheme.end,
        "extended_pos": morpheme.extended_pos,
        "is_user_dict": morpheme.is_user_dict,
        "is_formal_noun": morpheme.is_formal_noun,
        "is_low_info": morpheme.is_low_info,
        "is_unknown": morpheme.is_unknown,
        "is_from_dictionary": morpheme.is_from_dictionary,
        "score": float(format(morpheme.score, ".9g")),
    }


def _write_json(value: object) -> None:
    json.dump(value, sys.stdout, ensure_ascii=False, indent=2)
    sys.stdout.write("\n")


def _write_tags(tags: Sequence[Tag]) -> None:
    for tag in tags:
        print(f"{_tab_escape(tag.tag)}\t{tag.pos}")


def _tab_escape(value: str) -> str:
    """Escape backslash and record delimiters in one-line TAB output."""
    return (
        value.replace("\\", "\\\\").replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n")
    )


def _write_morphemes(morphemes: Sequence[Morpheme], output_format: str) -> None:
    if output_format in {"morpheme", "tsv"}:
        for morpheme in morphemes:
            print(
                f"{_tab_escape(morpheme.surface)}\t{morpheme.pos}"
                f"\t{_tab_escape(morpheme.base_form)}"
                f"\t{morpheme.start}\t{morpheme.end}"
            )
        return
    if output_format == "chasen":
        for morpheme in morphemes:
            conj_type = morpheme.conj_type or "*"
            conj_form = morpheme.conj_form or "*"
            print(
                f"{_tab_escape(morpheme.surface)}\t*\t"
                f"{_tab_escape(morpheme.base_form)}\t{morpheme.pos_ja}"
                f"\t{conj_type}\t{conj_form}"
            )
        print("EOS")


def _run_analysis(args: argparse.Namespace, parser: argparse.ArgumentParser) -> int:
    text = _read_text(args.text, sys.stdin)
    if not text:
        print(f"{parser.prog}: error: no input text provided", file=sys.stderr)
        return 1

    with Suzume(
        mode=args.mode,
        preserve_vu=not args.normalize_vu,
        preserve_case=not args.lowercase,
        preserve_symbols=args.preserve_symbols,
        lemmatize=not args.no_lemmatize,
        merge_compounds=args.merge_compounds,
        skip_core_dictionary=args.no_core_dict,
        skip_user_dictionary=args.no_user_dict,
        skip_env_config=args.skip_env_config,
    ) as analyzer:
        for warning in analyzer.dictionary_warnings:
            print(f"warning: {warning}", file=sys.stderr)
        _load_dictionaries(analyzer, args.dictionary_paths)
        if args.format == "tags":
            tags = analyzer.generate_tags(
                text,
                pos_filter=args.tag_pos,
                exclude_basic=args.tag_exclude_basic,
                use_lemma=not args.tag_use_surface,
                min_length=args.tag_min_length,
                max_tags=args.tag_max_tags,
                exclude_particles=not args.include_particles,
                exclude_auxiliaries=not args.include_auxiliaries,
                exclude_formal_nouns=not args.include_formal_nouns,
                exclude_low_info=not args.include_low_info,
                remove_duplicates=not args.tag_keep_duplicates,
            )
            _write_tags(tags)
            return 0

        analysis = analyzer.analyze_with_normalized_text(text)
        morphemes = analysis.morphemes
        if args.format == "json":
            _write_json(
                {
                    "input": text,
                    "normalized_text": analysis.normalized_text,
                    "morphemes": [_morpheme_dict(morpheme) for morpheme in morphemes],
                }
            )
        else:
            _write_morphemes(morphemes, args.format)
    return 0


def main(argv: Sequence[str] | None = None) -> int:
    """Run the CLI and return a process exit code."""
    parser = _build_parser()
    normalized_args = _without_explicit_command(sys.argv[1:] if argv is None else argv)
    if _has_standalone_version_option(normalized_args) and len(normalized_args) != 1:
        parser.error("version accepts no other arguments")
    args = parser.parse_args(normalized_args)
    try:
        return _run_analysis(args, parser)
    except (OSError, UnicodeError, SuzumeError, ValueError) as error:
        print(f"suzume: error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
