#!/usr/bin/env python3
"""Verify that public numeric contracts match the canonical C/C++ definitions.

The C ABI transports compact numeric codes. The bindings use canonical native
label functions where available and retain mirrors only for POS labels. This
guard checks those mirrors, the native conjugation and extended-POS label
tables, and error codes, flags, modes, and tag-filter bits.

Usage:
  scripts/check_binding_labels.py
"""

from __future__ import annotations

import ast
import os
import re
import sys
from dataclasses import dataclass
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
CPP_TYPES = "src/core/types.cpp"
CPP_CONJUGATION = "src/grammar/conjugation.cpp"
C_ABI = "src/suzume_c.cpp"
CPP_WRAPPER = "include/suzume/suzume.hpp"
WASM_LABELS = "bindings/wasm/js/abi_labels.ts"
PYTHON_LABELS = "bindings/python/src/suzume/_labels.py"
C_HEADER = "include/suzume/suzume_c.h"
WASM_INDEX = "bindings/wasm/js/index.ts"
PYTHON_API = "bindings/python/src/suzume/__init__.py"

DEFAULT_FIELDS = {
    "extended": (
        "preserve_vu",
        "preserve_case",
        "preserve_symbols",
        "mode",
        "lemmatize",
        "merge_compounds",
        "skip_user_dictionary",
        "skip_core_dictionary",
        "report_scorer_config",
        "skip_env_config",
        "scorer_options_json",
        "data_directory",
    ),
    "tag": (
        "pos_filter",
        "exclude_basic",
        "use_lemma",
        "min_length",
        "max_tags",
        "exclude_particles",
        "exclude_auxiliaries",
        "exclude_formal_nouns",
        "exclude_low_info",
        "remove_duplicates",
    ),
}

WASM_DEFAULT_NAMES = {
    "extended": {
        "preserve_vu": "preserveVu",
        "preserve_case": "preserveCase",
        "preserve_symbols": "preserveSymbols",
        "mode": "mode",
        "lemmatize": "lemmatize",
        "merge_compounds": "mergeCompounds",
        "skip_user_dictionary": "skipUserDictionary",
        "skip_core_dictionary": "skipCoreDictionary",
        "report_scorer_config": "reportScorerConfig",
        "skip_env_config": "skipEnvConfig",
        "scorer_options_json": "scorerOptions",
        "data_directory": "dataDirectory",
    },
    "tag": {
        "pos_filter": "posFilter",
        "exclude_basic": "excludeBasic",
        "use_lemma": "useLemma",
        "min_length": "minLength",
        "max_tags": "maxTags",
        "exclude_particles": "excludeParticles",
        "exclude_auxiliaries": "excludeAuxiliaries",
        "exclude_formal_nouns": "excludeFormalNouns",
        "exclude_low_info": "excludeLowInfo",
        "remove_duplicates": "removeDuplicates",
    },
}


@dataclass(frozen=True)
class EnumSpec:
    header: str
    sentinel: str | None


# These enums define serialized public label domains. Only PartOfSpeech retains
# binding-side mirrors; the others are decoded by canonical C ABI functions.
MIRRORED_ENUMS = {
    "PartOfSpeech": EnumSpec("src/core/types.h", "Count_"),
    "ExtendedPOS": EnumSpec("src/core/types.h", "Count_"),
    "ConjugationType": EnumSpec("src/dictionary/dictionary.h", None),
    "ConjForm": EnumSpec("src/grammar/conjugation.h", "Count_"),
}

# Keep snapshots keyed by enum member so additions and reorderings fail closed
# even though bindings now obtain these labels from the C ABI.
CONJUGATION_TYPE_LABELS = {
    "None": None,
    "Ichidan": "一段",
    "GodanKa": "五段・カ行",
    "GodanGa": "五段・ガ行",
    "GodanSa": "五段・サ行",
    "GodanTa": "五段・タ行",
    "GodanNa": "五段・ナ行",
    "GodanBa": "五段・バ行",
    "GodanMa": "五段・マ行",
    "GodanRa": "五段・ラ行",
    "GodanWa": "五段・ワ行",
    "Suru": "サ変",
    "Kuru": "カ変",
    "IAdjective": "形容詞",
    "NaAdjective": "ナ形容詞",
    "Interjection": "感動詞",
    "ProperFamily": "固有名詞・姓",
    "ProperGiven": "固有名詞・名",
}

CONJUGATION_FORM_LABELS = {
    "Base": "終止形",
    "Mizenkei": "未然形",
    "Renyokei": "連用形",
    "Onbinkei": "連用形",
    "Kateikei": "仮定形",
    "Meireikei": "命令形",
    "Ishikei": "意志形",
}


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", text)


def enclosed(text: str, opening: int, left: str, right: str, source: str) -> str:
    """Return the contents of the balanced delimiter at ``opening``."""
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == left:
            depth += 1
        elif text[index] == right:
            depth -= 1
            if depth == 0:
                return text[opening + 1 : index]
    raise SystemExit(f"❌ {source}: unterminated `{left}`")


def enum_members(name: str, spec: EnumSpec) -> list[str]:
    """Return enum members in their serialized numeric order."""
    text = Path(spec.header).read_text()
    match = re.search(rf"enum class {name}\s*:\s*\w+\s*\{{", text)
    if match is None:
        raise SystemExit(f"❌ {spec.header}: cannot find `enum class {name}`")
    body = strip_comments(enclosed(text, match.end() - 1, "{", "}", spec.header))

    values: dict[int, str] = {}
    next_value = 0
    for item in body.split(","):
        item = item.strip()
        if not item:
            continue
        parts = [part.strip() for part in item.split("=", 1)]
        member = parts[0]
        if len(parts) == 2:
            try:
                next_value = int(parts[1], 0)
            except ValueError as error:
                raise SystemExit(f"❌ {spec.header}: `{name}::{member}` needs a literal numeric value") from error
        if next_value in values:
            raise SystemExit(f"❌ {spec.header}: duplicate `{name}` value {next_value}")
        values[next_value] = member
        next_value += 1

    ordered = [values[index] for index in range(len(values))]
    if spec.sentinel is not None:
        if not ordered or ordered[-1] != spec.sentinel:
            raise SystemExit(f"❌ {spec.header}: `{name}` no longer ends with `{spec.sentinel}`")
        ordered.pop()
    return ordered


def function_body(path: str, name: str) -> str:
    text = Path(path).read_text()
    match = re.search(rf"\b{name}\s*\([^)]*\)\s*\{{", text)
    if match is None:
        raise SystemExit(f"❌ {path}: cannot find function `{name}`")
    return enclosed(text, match.end() - 1, "{", "}", path)


def cpp_switch_labels(path: str, function: str, enum_name: str, members: list[str]) -> list[str]:
    """Read a string-returning enum switch, including grouped fallback cases."""
    body = strip_comments(function_body(path, function))
    labels: dict[str, str] = {}
    pattern = rf"((?:\s*case\s+{enum_name}::\w+\s*:\s*)+)(?:default\s*:\s*)?return\s+\"([^\"]*)\"\s*;"
    for match in re.finditer(pattern, body):
        for member in re.findall(rf"{enum_name}::(\w+)", match.group(1)):
            labels[member] = match.group(2)

    missing = [member for member in members if member not in labels]
    if missing:
        raise SystemExit(f"❌ {path}: `{function}` has no labels for {', '.join(missing)}")
    return [labels[member] for member in members]


def literal_values(body: str, quote_pattern: str, null_tokens: tuple[str, ...]) -> list[str | None]:
    tokens = re.findall(quote_pattern + "|" + "|".join(map(re.escape, null_tokens)), body)
    values: list[str | None] = []
    for token in tokens:
        if token in null_tokens:
            values.append(None)
        else:
            value = ast.literal_eval(token)
            values.append(value if value != "" else None)
    return values


def named_table(
    path: str,
    name: str,
    left: str,
    right: str,
    quote_pattern: str,
    nulls: tuple[str, ...],
) -> list[str | None]:
    text = Path(path).read_text()
    match = re.search(rf"\b{name}\b[^=]*=\s*{re.escape(left)}", text)
    if match is None:
        raise SystemExit(f"❌ {path}: cannot find table `{name}`")
    body = enclosed(text, match.end() - 1, left, right, path)
    return literal_values(body, quote_pattern, nulls)


def cpp_local_table(path: str, function: str, table: str) -> list[str | None]:
    body = function_body(path, function)
    match = re.search(rf"\b{table}\b\s*=\s*\{{", body)
    if match is None:
        raise SystemExit(f"❌ {path}: `{function}` has no table `{table}`")
    values = enclosed(body, match.end() - 1, "{", "}", path)
    return literal_values(values, r'"(?:\\.|[^"\\])*"', ("nullptr",))


def compare(
    table_path: str,
    table_name: str,
    members: list[str],
    expected: list[str | None],
    actual: list[str | None],
) -> bool:
    if actual == expected:
        return False

    print(f"❌ {table_path}: `{table_name}` does not match the C++ enum labels")
    if len(actual) != len(expected):
        print(f"   length: expected {len(expected)}, got {len(actual)}")
    for index, member in enumerate(members):
        got = actual[index] if index < len(actual) else "<missing>"
        if got != expected[index]:
            print(f"   [{index}] {member}: expected {expected[index]!r}, got {got!r}")
    if len(actual) > len(expected):
        print(f"   extra values: {actual[len(expected) :]!r}")
    return True


def check_distinct_labels(table_path: str, table_name: str, members: list[str], labels: list[str]) -> bool:
    """A label is the only public name of its code, so it must identify one member."""
    failed = False
    owners: dict[str, str] = {}
    for member, label in zip(members, labels, strict=True):
        if not label:
            print(f"❌ {table_path}: `{table_name}` has an empty label for {member}")
            failed = True
        elif label in owners:
            print(f"❌ {table_path}: `{table_name}` gives {owners[label]} and {member} the same label {label!r}")
            failed = True
        else:
            owners[label] = member
    return failed


def check_canonical_calls() -> bool:
    """Every label accessor must decode through the one native owner of that label."""
    failed = False
    for path, function, callee in (
        (CPP_WRAPPER, "posLabel", "suzume_pos_label"),
        (CPP_WRAPPER, "conjugationTypeLabel", "suzume_conjugation_type_label"),
        (CPP_WRAPPER, "conjugationFormLabel", "suzume_conjugation_form_label"),
        (CPP_WRAPPER, "extendedPosLabel", "suzume_extended_pos_label"),
        (C_ABI, "suzume_pos_label", "posToString"),
        (C_ABI, "suzume_extended_pos_label", "extendedPosToString"),
        (C_ABI, "suzume_conjugation_form_label", "conjFormToJapanese"),
    ):
        if callee not in function_body(path, function):
            print(f"❌ {path}: `{function}` must call `{callee}`")
            failed = True

    wasm = Path(WASM_INDEX).read_text()
    python = Path(PYTHON_API).read_text()
    for label in ("pos", "conjugation_type", "conjugation_form", "extended_pos"):
        for path, text, call in (
            (WASM_INDEX, wasm, f"this.module._suzume_{label}_label"),
            (PYTHON_API, python, f"_lib.suzume_{label}_label"),
        ):
            if call not in text:
                print(f"❌ {path}: the {label} label must be decoded by `{call}`")
                failed = True
    return failed


def normalized_name(name: str) -> str:
    return name.replace("_", "").lower()


def c_numeric_constants(prefix: str) -> dict[str, int]:
    text = Path(C_HEADER).read_text()
    values: dict[str, int] = {}
    for name, expression in re.findall(rf"\b{prefix}([A-Z0-9_]+)\s*=\s*([^,\n]+)", text):
        expression = expression.strip()
        shift = re.fullmatch(r"1U?\s*<<\s*(\d+)U?", expression)
        literal = re.fullmatch(r"(\d+)U?", expression)
        if shift:
            values[normalized_name(name)] = 1 << int(shift.group(1))
        elif literal:
            values[normalized_name(name)] = int(literal.group(1))
    return values


def parse_default_literal(expression: str, source: str) -> bool | int | None:
    expression = expression.strip()
    if expression == "nullptr" or expression == "null":
        return None
    if expression == "true":
        return True
    if expression == "false":
        return False
    if expression == "Mode::Normal" or expression == "'normal'":
        return 0
    if re.fullmatch(r"\d+", expression):
        return int(expression)
    raise SystemExit(f"❌ {source}: unsupported default expression `{expression}`")


def c_initializer_defaults(function: str, fields: tuple[str, ...]) -> dict[str, bool | int | None]:
    body = function_body(C_ABI, function)
    defaults = {
        field: parse_default_literal(expression, C_ABI)
        for field, expression in re.findall(r"options->(\w+)\s*=\s*([^;]+);", body)
    }
    if set(defaults) != set(fields):
        raise SystemExit(f"❌ {C_ABI}: `{function}` does not initialize exactly {fields}")
    return defaults


def cpp_struct_defaults(name: str, fields: tuple[str, ...]) -> dict[str, bool | int | None]:
    text = Path(CPP_WRAPPER).read_text()
    match = re.search(rf"struct {name}\s*\{{", text)
    if match is None:
        raise SystemExit(f"❌ {CPP_WRAPPER}: cannot find `struct {name}`")
    body = strip_comments(enclosed(text, match.end() - 1, "{", "}", CPP_WRAPPER))
    defaults: dict[str, bool | int | None] = {}
    for statement in body.split(";"):
        match = re.search(r"\b(\w+)\s*(?:=\s*([^;]+))?$", statement.strip())
        if match is None:
            continue
        field, expression = match.groups()
        if field not in fields:
            continue
        defaults[field] = None if expression is None else parse_default_literal(expression, CPP_WRAPPER)
    if set(defaults) != set(fields):
        raise SystemExit(f"❌ {CPP_WRAPPER}: `{name}` does not declare defaults for exactly {fields}")
    return defaults


def ts_default_object(name: str) -> dict[str, bool | int | None]:
    text = Path(WASM_INDEX).read_text()
    match = re.search(rf"\b{name}\b\s*=\s*\{{", text)
    if match is None:
        raise SystemExit(f"❌ {WASM_INDEX}: cannot find `{name}`")
    body = enclosed(text, match.end() - 1, "{", "}", WASM_INDEX)
    return {
        field: parse_default_literal(expression.rstrip(","), WASM_INDEX)
        for field, expression in re.findall(r"\b(\w+)\s*:\s*([^,\n]+)", body)
    }


def python_default_value(node: ast.expr) -> bool | int | None:
    if isinstance(node, ast.Constant) and (node.value is None or isinstance(node.value, (bool, int))):
        return node.value
    if (
        isinstance(node, ast.Attribute)
        and isinstance(node.value, ast.Name)
        and node.value.id == "Mode"
        and node.attr == "NORMAL"
    ):
        return 0
    raise SystemExit(f"❌ {PYTHON_API}: unsupported Python default `{ast.unparse(node)}`")


def python_method_defaults(class_name: str, method: str) -> dict[str, bool | int | None]:
    tree = ast.parse(Path(PYTHON_API).read_text())
    for class_node in tree.body:
        if isinstance(class_node, ast.ClassDef) and class_node.name == class_name:
            for node in class_node.body:
                if isinstance(node, ast.FunctionDef) and node.name == method:
                    args = [*node.args.posonlyargs, *node.args.args]
                    defaults = [None] * (len(args) - len(node.args.defaults)) + list(node.args.defaults)
                    args.extend(node.args.kwonlyargs)
                    defaults.extend(node.args.kw_defaults)
                    return {
                        argument.arg: python_default_value(default)
                        for argument, default in zip(args, defaults, strict=True)
                        if default is not None and argument.arg != "self"
                    }
    raise SystemExit(f"❌ {PYTHON_API}: cannot find `{class_name}.{method}`")


def check_default_contracts() -> bool:
    canonical = {
        "extended": c_initializer_defaults("suzume_init_extended_options", DEFAULT_FIELDS["extended"]),
        "tag": c_initializer_defaults("suzume_init_tag_options", DEFAULT_FIELDS["tag"]),
    }
    cpp = {
        "extended": cpp_struct_defaults("Options", DEFAULT_FIELDS["extended"]),
        "tag": cpp_struct_defaults("TagOptions", DEFAULT_FIELDS["tag"]),
    }
    wasm_raw = {
        "extended": ts_default_object("EXTENDED_OPTION_DEFAULTS"),
        "tag": ts_default_object("TAG_OPTION_DEFAULTS"),
    }
    wasm = {
        kind: {field: wasm_raw[kind][name] for field, name in names.items()}
        for kind, names in WASM_DEFAULT_NAMES.items()
    }
    python_extended = python_method_defaults("Suzume", "__init__")
    python_tag = python_method_defaults("Suzume", "generate_tags")
    python = {
        "extended": {
            field: python_extended[field]
            for field in DEFAULT_FIELDS["extended"]
            if field not in {"scorer_options_json", "data_directory"}
        },
        "tag": {field: python_tag[field] for field in DEFAULT_FIELDS["tag"]},
    }

    failed = False
    for kind, expected in canonical.items():
        mirrors = {
            "C++": cpp[kind],
            "WASM": wasm[kind],
            "Python": python[kind],
        }
        for surface, actual in mirrors.items():
            comparable = (
                expected
                if surface != "Python" or kind != "extended"
                else {
                    field: value
                    for field, value in expected.items()
                    if field not in {"scorer_options_json", "data_directory"}
                }
            )
            if actual != comparable:
                print(f"❌ {surface} {kind} option defaults drift: expected {comparable}, got {actual}")
                failed = True

    # Python's package-local data directory is intentionally resolved at runtime
    # so wheels find their bundled dictionaries. Its no-bundle fallback must
    # remain the C ABI's nullptr default rather than another static mirror.
    python_text = Path(PYTHON_API).read_text()
    if not re.search(r"data_directory\s*=\s*os\.fsencode\(_BUNDLED_DATA_DIR\).*?else None", python_text):
        print("❌ Python data_directory must retain a None fallback to the C ABI default")
        failed = True
    return failed


def check_numeric_contracts() -> bool:
    failed = False
    wasm = Path(WASM_INDEX).read_text()
    wasm_labels = Path(WASM_LABELS).read_text()
    python = Path(PYTHON_API).read_text()
    python_labels = Path(PYTHON_LABELS).read_text()

    canonical_errors = c_numeric_constants("SUZUME_ERROR_")
    wasm_error_body = enclosed(wasm, wasm.index("{", wasm.index("export enum ErrorCode")), "{", "}", WASM_INDEX)
    wasm_errors = {
        normalized_name(name): int(value) for name, value in re.findall(r"\b(\w+)\s*=\s*(\d+)", wasm_error_body)
    }
    python_errors: dict[str, int] = {}
    for node in ast.parse(python).body:
        if isinstance(node, ast.ClassDef) and node.name == "ErrorCode":
            for statement in node.body:
                if (
                    isinstance(statement, ast.Assign)
                    and len(statement.targets) == 1
                    and isinstance(statement.targets[0], ast.Name)
                    and isinstance(statement.value, ast.Constant)
                    and isinstance(statement.value.value, int)
                ):
                    python_errors[normalized_name(statement.targets[0].id)] = statement.value.value

    canonical_flags = c_numeric_constants("SUZUME_MORPHEME_")
    wasm_flag_body = enclosed(
        wasm_labels,
        wasm_labels.index("{", wasm_labels.index("export const MORPHEME_FLAG")),
        "{",
        "}",
        WASM_LABELS,
    )
    wasm_flags = {
        normalized_name(name): 1 << int(shift)
        for name, shift in re.findall(r"\b(\w+)\s*:\s*1\s*<<\s*(\d+)", wasm_flag_body)
    }
    python_flags = {
        normalized_name(name): 1 << int(shift)
        for name, shift in re.findall(r"\bFLAG_([A-Z_]+)\s*=\s*1\s*<<\s*(\d+)", python_labels)
    }

    canonical_tags = c_numeric_constants("SUZUME_TAG_POS_")
    wasm_tag_body = enclosed(wasm, wasm.index("{", wasm.index("TAG_POS_FILTER_BITS")), "{", "}", WASM_INDEX)
    wasm_tags = {normalized_name(name): int(value) for name, value in re.findall(r"\b(\w+)\s*:\s*(\d+)", wasm_tag_body)}
    python_tag_match = re.search(r"_POS_FILTER_BITS\s*=\s*\{(?P<body>.*?)\}", python, re.DOTALL)
    python_tags = (
        {
            normalized_name(name): int(value)
            for name, value in re.findall(r'"(\w+)"\s*:\s*(\d+)', python_tag_match.group("body"))
        }
        if python_tag_match
        else {}
    )

    canonical_modes = c_numeric_constants("SUZUME_MODE_")
    canonical_modes.pop("invalid", None)

    wrapper = Path(CPP_WRAPPER).read_text()
    mode_match = re.search(r"enum class Mode[^{]*\{(?P<body>.*?)\}", wrapper, re.DOTALL)
    wrapper_modes = (
        {
            normalized_name(name): canonical_modes.get(normalized_name(c_constant), -1)
            for name, c_constant in re.findall(r"\b(\w+)\s*=\s*SUZUME_MODE_(\w+)", mode_match.group("body"))
        }
        if mode_match
        else {}
    )
    wasm_mode_match = re.search(r"const ANALYSIS_MODE_CODES[^=]*=\s*\{(?P<body>.*?)\}", wasm, re.DOTALL)
    wasm_modes = (
        {
            normalized_name(name): int(value)
            for name, value in re.findall(r"\b(\w+)\s*:\s*(\d+)", wasm_mode_match.group("body"))
        }
        if wasm_mode_match
        else {}
    )
    python_modes = {
        normalized_name(name): int(value) for name, value in re.findall(r"Mode\.([A-Z]+)\s*:\s*(\d+)", python)
    }

    for label, canonical, mirrors in (
        ("error codes", canonical_errors, (wasm_errors, python_errors)),
        ("morpheme flags", canonical_flags, (wasm_flags, python_flags)),
        ("tag POS bits", canonical_tags, (wasm_tags, python_tags)),
        ("analysis modes", canonical_modes, (wrapper_modes, wasm_modes, python_modes)),
    ):
        for mirror in mirrors:
            if mirror != canonical:
                print(f"❌ {label} drift: expected {canonical}, got {mirror}")
                failed = True
    return failed


def main() -> int:
    os.chdir(REPOSITORY_ROOT)

    members = {name: enum_members(name, spec) for name, spec in MIRRORED_ENUMS.items()}
    expected = {
        "pos_en": cpp_switch_labels(CPP_TYPES, "posToString", "PartOfSpeech", members["PartOfSpeech"]),
        "pos_ja": cpp_switch_labels(CPP_TYPES, "posToJapanese", "PartOfSpeech", members["PartOfSpeech"]),
        "extended": cpp_switch_labels(CPP_TYPES, "extendedPosToString", "ExtendedPOS", members["ExtendedPOS"]),
        "conj_type": [CONJUGATION_TYPE_LABELS[name] for name in members["ConjugationType"]],
        "conj_form": [CONJUGATION_FORM_LABELS[name] for name in members["ConjForm"]],
    }

    checks = [
        (
            CPP_WRAPPER,
            "posLabel::japanese_labels",
            members["PartOfSpeech"],
            expected["pos_ja"],
            cpp_local_table(CPP_WRAPPER, "posLabel", "japanese_labels"),
        ),
        (
            C_ABI,
            "suzume_conjugation_type_label::labels",
            members["ConjugationType"],
            expected["conj_type"],
            cpp_local_table(C_ABI, "suzume_conjugation_type_label", "labels"),
        ),
        (
            CPP_CONJUGATION,
            "kConjFormJapanese",
            members["ConjForm"],
            expected["conj_form"],
            named_table(CPP_CONJUGATION, "kConjFormJapanese", "{", "}", r'"(?:\\.|[^"\\])*"', ("nullptr",)),
        ),
        (
            WASM_LABELS,
            "POS_ENGLISH",
            members["PartOfSpeech"],
            expected["pos_en"],
            named_table(WASM_LABELS, "POS_ENGLISH", "[", "]", r"'(?:\\.|[^'\\])*'", ("null",)),
        ),
        (
            WASM_LABELS,
            "POS_JAPANESE",
            members["PartOfSpeech"],
            expected["pos_ja"],
            named_table(WASM_LABELS, "POS_JAPANESE", "[", "]", r"'(?:\\.|[^'\\])*'", ("null",)),
        ),
        (
            PYTHON_LABELS,
            "POS_ENGLISH",
            members["PartOfSpeech"],
            expected["pos_en"],
            named_table(PYTHON_LABELS, "POS_ENGLISH", "(", ")", r'"(?:\\.|[^"\\])*"', ("None",)),
        ),
        (
            PYTHON_LABELS,
            "POS_JAPANESE",
            members["PartOfSpeech"],
            expected["pos_ja"],
            named_table(PYTHON_LABELS, "POS_JAPANESE", "(", ")", r'"(?:\\.|[^"\\])*"', ("None",)),
        ),
    ]

    # Evaluate every group before combining so one failure cannot hide another.
    results = [compare(*check) for check in checks]
    results.append(
        check_distinct_labels(CPP_TYPES, "extendedPosToString", members["ExtendedPOS"], expected["extended"])
    )
    results.append(check_canonical_calls())
    results.append(check_numeric_contracts())
    results.append(check_default_contracts())
    if any(results):
        print()
        print("A mislabeled numeric code changes public output. Update every mirror above in enum order.")
        return 1

    print("✅ C++, WASM, and Python labels and numeric contracts match")
    return 0


if __name__ == "__main__":
    sys.exit(main())
