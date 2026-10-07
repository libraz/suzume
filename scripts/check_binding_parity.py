#!/usr/bin/env python3
"""Compare native C ABI, Python, and WASM at their public API boundaries."""

from __future__ import annotations

import ctypes
import json
import subprocess
from collections.abc import Iterator
from contextlib import contextmanager
from pathlib import Path
from typing import Any, TypedDict, cast

from suzume import Suzume
from suzume._ffi import (
    SuzumeExtendedOptions,
    SuzumeTagOptions,
    load_library,
)


class MorphemeRecord(TypedDict):
    surface: str
    pos: str
    lemma: str
    conj_type: str | None
    conj_form: str | None
    extended_pos: str
    start: int
    end: int
    flags: dict[str, bool]
    score: float


class AnalysisRecord(TypedDict):
    normalized_text: str
    morphemes: list[MorphemeRecord]


class TagRecord(TypedDict):
    tag: str
    pos: str


class AnalysisCase(TypedDict):
    name: str
    text: str
    options: dict[str, Any]


class TagCase(TypedDict):
    name: str
    text: str
    options: dict[str, Any]


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
NATIVE_CLI = REPOSITORY_ROOT / "build" / "bin" / "suzume-cli"
WASM_RUNNER = REPOSITORY_ROOT / "scripts" / "binding_parity.mjs"

# Each non-default analysis option has a dedicated case. The tag case varies
# every generator option at once, which also covers the POS-filter bit mapping.
ANALYSIS_CASES: list[AnalysisCase] = [
    {"name": "default-unicode", "text": "ｶﾀｶﾅとＡＢＣ１２３家族👨‍👩‍👧", "options": {}},
    {"name": "mode-search", "text": "東京駅前", "options": {"mode": "search"}},
    {"name": "mode-split", "text": "東京駅前", "options": {"mode": "split"}},
    {"name": "normalize-vu", "text": "ヴァイオリン", "options": {"preserve_vu": False}},
    {"name": "normalize-case", "text": "ＡＢＣ東京", "options": {"preserve_case": False}},
    {"name": "preserve-symbols", "text": "東京👨‍👩‍👧", "options": {"preserve_symbols": True}},
    {"name": "source-lemma", "text": "食べました", "options": {"lemmatize": False}},
    {"name": "merge-compounds", "text": "東京駅前", "options": {"merge_compounds": True}},
]
TAG_CASES: list[TagCase] = [
    {
        "name": "all-non-default-tag-options",
        "text": "りんごを食べるりんごを食べる",
        "options": {
            "pos_filter": ["noun", "verb"],
            "exclude_basic": True,
            "use_lemma": False,
            "min_length": 1,
            "max_tags": 3,
            "exclude_particles": False,
            "exclude_auxiliaries": False,
            "exclude_formal_nouns": False,
            "exclude_low_info": False,
            "remove_duplicates": False,
        },
    }
]

_POS_FILTER_BITS = {"noun": 1, "verb": 2, "adjective": 4, "adverb": 8}
_MODE_CODES = {"normal": 0, "search": 1, "split": 2}
# Bit positions in the native morpheme flags word, in record order.
_FLAG_BITS = {
    "user_dict": 0,
    "formal_noun": 1,
    "low_info": 2,
    "unknown": 3,
    "from_dictionary": 4,
    "conjugatable": 5,
}
_lib = load_library()


def _decode(value: bytes | None) -> str:
    return value.decode("utf-8") if value else ""


def _flags(value: int) -> dict[str, bool]:
    return {name: bool(value & (1 << bit)) for name, bit in _FLAG_BITS.items()}


@contextmanager
def _native_handle(handle: Any) -> Iterator[Any]:
    """Own a freshly created native handle, destroying it on exit."""
    if not handle:
        raise RuntimeError(_decode(_lib.suzume_last_error()))
    try:
        yield handle
    finally:
        _lib.suzume_destroy(handle)


@contextmanager
def _native_result(result: Any, free: Any) -> Iterator[Any]:
    """Own a native result pointer, yielding its contents and freeing it on exit."""
    if not result:
        raise RuntimeError(_decode(_lib.suzume_last_error()))
    try:
        yield result.contents
    finally:
        free(result)


def _morpheme_record(
    surface: str,
    pos: str,
    lemma: str,
    conj_type: str | None,
    conj_form: str | None,
    extended_pos: str,
    start: int,
    end: int,
    flags: dict[str, bool],
    score: float,
) -> MorphemeRecord:
    return {
        "surface": surface,
        "pos": pos,
        "lemma": lemma,
        "conj_type": conj_type,
        "conj_form": conj_form,
        "extended_pos": extended_pos,
        "start": start,
        "end": end,
        "flags": flags,
        "score": score,
    }


def _native_cli_records() -> list[list[dict[str, object]]]:
    """Keep the shipped CLI JSON contract in the parity gate unchanged."""
    records: list[list[dict[str, object]]] = []
    for case in ANALYSIS_CASES[:1]:
        completed = subprocess.run(
            [str(NATIVE_CLI), "--format", "json", case["text"]],
            cwd=REPOSITORY_ROOT,
            check=True,
            capture_output=True,
            text=True,
        )
        payload = json.loads(completed.stdout)
        records.append(
            [{key: item[key] for key in ("surface", "pos", "lemma", "start", "end")} for item in payload["morphemes"]]
        )
    return records


def _native_options(options: dict[str, Any]) -> SuzumeExtendedOptions:
    native = SuzumeExtendedOptions()
    _lib.suzume_init_extended_options(ctypes.byref(native))
    for field in (
        "preserve_vu",
        "preserve_case",
        "preserve_symbols",
        "lemmatize",
        "merge_compounds",
    ):
        if field in options:
            setattr(native, field, int(options[field]))
    if "mode" in options:
        native.mode = _MODE_CODES[options["mode"]]
    return native


def _native_analysis_records() -> list[AnalysisRecord]:
    records: list[AnalysisRecord] = []
    for case in ANALYSIS_CASES:
        options = _native_options(case["options"])
        payload = case["text"].encode("utf-8")
        with (
            _native_handle(_lib.suzume_create_with_extended_options(ctypes.byref(options))) as handle,
            _native_result(_lib.suzume_analyze_n(handle, payload, len(payload)), _lib.suzume_result_free) as raw,
        ):
            morphemes: list[MorphemeRecord] = []
            for index in range(raw.count):
                morpheme = raw.morphemes[index]
                flags = _flags(int(morpheme.flags))
                morphemes.append(
                    _morpheme_record(
                        ctypes.string_at(morpheme.surface, morpheme.surface_size).decode("utf-8"),
                        _decode(_lib.suzume_pos_label(morpheme.pos)) or "OTHER",
                        ctypes.string_at(morpheme.base_form, morpheme.base_form_size).decode("utf-8"),
                        _decode(_lib.suzume_conjugation_type_label(morpheme.conjugation_type)) or None
                        if flags["conjugatable"]
                        else None,
                        _decode(_lib.suzume_conjugation_form_label(morpheme.conjugation_form)) or None
                        if flags["conjugatable"]
                        else None,
                        _decode(_lib.suzume_extended_pos_label(morpheme.extended_pos)) or "UNKNOWN",
                        int(morpheme.start),
                        int(morpheme.end),
                        flags,
                        float(morpheme.score),
                    )
                )
            records.append(
                {
                    "normalized_text": ctypes.string_at(raw.normalized_text, raw.normalized_text_size).decode("utf-8"),
                    "morphemes": morphemes,
                }
            )
    return records


def _native_tag_records() -> list[list[TagRecord]]:
    records: list[list[TagRecord]] = []
    for case in TAG_CASES:
        options = SuzumeTagOptions()
        _lib.suzume_init_tag_options(ctypes.byref(options))
        for field, value in case["options"].items():
            if field == "pos_filter":
                options.pos_filter = sum(_POS_FILTER_BITS[name] for name in value)
            else:
                setattr(options, field, int(value))
        payload = case["text"].encode("utf-8")
        with (
            _native_handle(_lib.suzume_create()) as handle,
            _native_result(
                _lib.suzume_generate_tags_with_options_n(handle, payload, len(payload), ctypes.byref(options)),
                _lib.suzume_tags_free,
            ) as raw,
        ):
            records.append(
                [
                    {
                        "tag": _decode(raw.tags[index]),
                        "pos": _decode(_lib.suzume_pos_label(raw.pos[index])),
                    }
                    for index in range(raw.count)
                ]
            )
    return records


def _python_flags(item: Any) -> dict[str, bool]:
    flags = {name: bool(getattr(item, f"is_{name}")) for name in _FLAG_BITS if name != "conjugatable"}
    flags["conjugatable"] = item.conj_form is not None
    return flags


def _python_analysis_records() -> list[AnalysisRecord]:
    records: list[AnalysisRecord] = []
    for case in ANALYSIS_CASES:
        with Suzume(**case["options"]) as analyzer:
            result = analyzer.analyze_with_normalized_text(case["text"])
        records.append(
            {
                "normalized_text": result.normalized_text,
                "morphemes": [
                    _morpheme_record(
                        item.surface,
                        item.pos,
                        item.base_form,
                        item.conj_type,
                        item.conj_form,
                        item.extended_pos,
                        item.start,
                        item.end,
                        _python_flags(item),
                        item.score,
                    )
                    for item in result.morphemes
                ],
            }
        )
    return records


def _python_tag_records() -> list[list[TagRecord]]:
    records: list[list[TagRecord]] = []
    for case in TAG_CASES:
        with Suzume() as analyzer:
            records.append(
                [{"tag": item.tag, "pos": item.pos} for item in analyzer.generate_tags(case["text"], **case["options"])]
            )
    return records


def _wasm_records() -> dict[str, list[Any]]:
    completed = subprocess.run(
        ["node", str(WASM_RUNNER)],
        cwd=REPOSITORY_ROOT,
        input=json.dumps({"analysis": ANALYSIS_CASES, "tags": TAG_CASES}, ensure_ascii=False),
        capture_output=True,
        check=True,
        text=True,
    )
    return cast(dict[str, list[Any]], json.loads(completed.stdout))


def _validate_surface_ranges(record: AnalysisRecord, text: str, surface: str) -> None:
    normalized_codepoints = list(record["normalized_text"])
    for morpheme in record["morphemes"]:
        extracted = "".join(normalized_codepoints[morpheme["start"] : morpheme["end"]])
        if extracted != morpheme["surface"]:
            raise AssertionError(f"{surface} range mismatch for {text!r}: {morpheme['surface']!r} != {extracted!r}")


def main() -> None:
    native_cli = _native_cli_records()
    native = _native_analysis_records()
    native_tags = _native_tag_records()
    python = _python_analysis_records()
    python_tags = _python_tag_records()
    wasm = _wasm_records()

    for index, case in enumerate(ANALYSIS_CASES):
        text = case["text"]
        if native[index] != python[index] or native[index] != wasm["analysis"][index]:
            raise AssertionError(
                f"analysis mismatch for {case['name']!r}:\n"
                f"native={json.dumps(native[index], ensure_ascii=False, indent=2)}\n"
                f"python={json.dumps(python[index], ensure_ascii=False, indent=2)}\n"
                f"wasm={json.dumps(wasm['analysis'][index], ensure_ascii=False, indent=2)}"
            )
        _validate_surface_ranges(native[index], text, "C ABI")
        _validate_surface_ranges(python[index], text, "Python")
        _validate_surface_ranges(cast(AnalysisRecord, wasm["analysis"][index]), text, "WASM")

    for index, case in enumerate(TAG_CASES):
        if native_tags[index] != python_tags[index] or native_tags[index] != wasm["tags"][index]:
            raise AssertionError(
                f"tag mismatch for {case['name']!r}: native={native_tags[index]!r}, "
                f"python={python_tags[index]!r}, wasm={wasm['tags'][index]!r}"
            )

    cli_default = [
        {key: morpheme[key] for key in ("surface", "pos", "lemma", "start", "end")}
        for morpheme in native[0]["morphemes"]
    ]
    if native_cli[0] != cli_default:
        raise AssertionError(f"native CLI/C ABI mismatch: {native_cli[0]!r} != {cli_default!r}")
    print(
        f"Binding parity passed: {len(ANALYSIS_CASES)} analysis-option cases and "
        f"{len(TAG_CASES)} tag-option cases across CLI, C ABI, Python, and WASM"
    )


if __name__ == "__main__":
    main()
