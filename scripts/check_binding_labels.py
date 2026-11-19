#!/usr/bin/env python3
"""Verify that public numeric contracts match the canonical C/C++ definitions.

The C ABI transports compact numeric codes and owns every label for them. This
guard checks that each binding decodes labels through the canonical C function
and keeps no local label table of its own, that the native label tables match
their enums, and that error codes, flags, modes, tag-filter bits and option
defaults agree across the C ABI, C++, WASM and Python.

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
# Both binding suites must read error and option expectations from this one fixture,
# so a message or code change cannot be made to one binding's suite alone.
CONFORMANCE_FIXTURE = "bindings/conformance/errors.json"
CONFORMANCE_SUITES = (
    "bindings/wasm/tests/errors.test.ts",
    "bindings/wasm/tests/options.test.ts",
    "bindings/python/tests/test_errors.py",
    "bindings/python/tests/test_options.py",
)

DEFAULT_FIELDS = {
    "options": (
        "preserve_vu",
        "preserve_case",
        "preserve_symbols",
        "mode",
        "lemmatize",
        "merge_compounds",
        "skip_bundled_user_dictionary",
        "skip_core_dictionary",
        "report_scorer_config",
        "skip_env_config",
        "scorer_options_json",
        "data_directory",
    ),
    "tag": (
        "pos_filter",
        "exclude_basic",
        "use_base_form",
        "min_length",
        "max_tags",
        "exclude_particles",
        "exclude_auxiliaries",
        "exclude_formal_nouns",
        "exclude_low_info",
        "remove_duplicates",
    ),
}

# C field -> binding option name, where the binding spells it differently.
CPP_DEFAULT_NAMES = {
    "options": {"scorer_options_json": "scorer_options"},
    "tag": {},
}
PYTHON_DEFAULT_NAMES: dict[str, dict[str, str]] = {"options": {}, "tag": {}}
# Python takes POS filters by label only, so "no filter" is None where C has 0.
PYTHON_NONE_AS_ZERO = {"pos_filter"}
# C option fields a binding does not expose as an option. npm embeds its dictionaries, so it has
# no data directory; Python resolves its bundled data directory at runtime and passes scorer
# options through its own path (both checked separately below).
UNMIRRORED_DEFAULTS = {
    "WASM": {"options": {"data_directory"}, "tag": set()},
    "Python": {"options": {"scorer_options_json", "data_directory"}, "tag": set()},
}

WASM_DEFAULT_NAMES = {
    "options": {
        "preserve_vu": "preserveVu",
        "preserve_case": "preserveCase",
        "preserve_symbols": "preserveSymbols",
        "mode": "mode",
        "lemmatize": "lemmatize",
        "merge_compounds": "mergeCompounds",
        "skip_bundled_user_dictionary": "skipBundledUserDictionary",
        "skip_core_dictionary": "skipCoreDictionary",
        "report_scorer_config": "reportScorerConfig",
        "skip_env_config": "skipEnvConfig",
        "scorer_options_json": "scorerOptions",
    },
    "tag": {
        "pos_filter": "posFilter",
        "exclude_basic": "excludeBasic",
        "use_base_form": "useBaseForm",
        "min_length": "minLength",
        "max_tags": "maxTags",
        "exclude_particles": "excludeParticles",
        "exclude_auxiliaries": "excludeAuxiliaries",
        "exclude_formal_nouns": "excludeFormalNouns",
        "exclude_low_info": "excludeLowInfo",
        "remove_duplicates": "removeDuplicates",
    },
}


# Public names the 0.10 surface removed. Docs, examples and CLI help must not
# mention them; binding sources may still call the C names they mirror.
REMOVED_NAMES = (
    "loadUserDictionaryCount",
    "loadUserDictionaryOrThrow",
    "loadBinaryDictionaryOrThrow",
    "lastError",
    "lastErrorCode",
    "useLemma",
    "skipUserDictionary",
    "load_user_dict",
    "load_binary_dict",
    "use_lemma",
    "skip_user_dictionary",
    "--tag-pos",
    "--tag-use-surface",
    "--tag-keep-duplicates",
    "--include-particles",
    "--include-auxiliaries",
    "--include-formal-nouns",
    "--include-low-info",
    "--normalize-vu",
    "--lowercase",
    "--no-core-dict",
    "--no-user-dict",
    "--dict",
    "-f morpheme",
)
REMOVED_NAME_SCOPE = (
    "examples/**/*",
    "bindings/*/README.md",
    "bindings/python/src/suzume/cli.py",
    "tools/cli/cli_help.cpp",
)


@dataclass(frozen=True)
class EnumSpec:
    header: str
    sentinel: str | None


# These enums define serialized public label domains; every binding decodes
# them through the canonical C ABI label functions.
MIRRORED_ENUMS = {
    "PartOfSpeech": EnumSpec("src/core/types.h", "Count_"),
    "ExtendedPOS": EnumSpec("src/core/types.h", "Count_"),
    "ConjugationType": EnumSpec("src/dictionary/dictionary.h", None),
    "ConjForm": EnumSpec("src/grammar/conjugation.h", "Count_"),
}

# Keep snapshots keyed by enum member so additions, reorderings and relabelings
# fail closed even though bindings obtain these labels from the C ABI.
POS_LABELS = {
    "Unknown": ("OTHER", "その他"),
    "Noun": ("NOUN", "名詞"),
    "Verb": ("VERB", "動詞"),
    "Adjective": ("ADJ", "形容詞"),
    "Adverb": ("ADV", "副詞"),
    "Particle": ("PARTICLE", "助詞"),
    "Auxiliary": ("AUX", "助動詞"),
    "Conjunction": ("CONJ", "接続詞"),
    "Determiner": ("DET", "連体詞"),
    "Pronoun": ("PRON", "代名詞"),
    "Prefix": ("PREFIX", "接頭辞"),
    "Suffix": ("SUFFIX", "接尾辞"),
    "Interjection": ("INTJ", "感動詞"),
    "Symbol": ("SYMBOL", "記号"),
    "Other": ("OTHER", "その他"),
}

# Binding sources that must not carry a copy of a native label table.
BINDING_SOURCES = (
    CPP_WRAPPER,
    "bindings/wasm/js/*.ts",
    "bindings/python/src/suzume/*.py",
)
# POS labels that are not also tag-filter names, so a file quoting several of
# them is holding a label table rather than naming a filter.
TABLE_ONLY_POS_LABELS = {"CONJ", "DET", "PRON", "PREFIX", "SUFFIX", "INTJ", "SYMBOL"}
LOCAL_TABLE_THRESHOLD = 3

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
        (C_ABI, "suzume_pos_label", "posToString"),
        (C_ABI, "suzume_pos_label_ja", "posToJapanese"),
        (C_ABI, "suzume_extended_pos_label", "extendedPosToString"),
        (C_ABI, "suzume_conjugation_form_label", "conjFormToJapanese"),
    ):
        if callee not in function_body(path, function):
            print(f"❌ {path}: `{function}` must call `{callee}`")
            failed = True

    wrapper = strip_comments(Path(CPP_WRAPPER).read_text())
    wasm = Path(WASM_INDEX).read_text()
    python = Path(PYTHON_API).read_text()
    for label in ("pos", "pos_label_ja", "conjugation_type", "conjugation_form", "extended_pos"):
        for path, text, call in (
            (CPP_WRAPPER, wrapper, f"suzume_{native_name(label)}("),
            (WASM_INDEX, wasm, f"this.module._suzume_{native_name(label)}"),
            (PYTHON_API, python, f"_lib.suzume_{native_name(label)}"),
        ):
            if call not in text:
                print(f"❌ {path}: the {label} label must be decoded by `{call}`")
                failed = True
    return failed


def native_name(label: str) -> str:
    return label if label.startswith("pos_label") else f"{label}_label"


def ts_string_union(text: str, name: str) -> tuple[re.Match[str] | None, set[str]]:
    """The exported string-literal union type `name` and its members."""
    match = re.search(rf"export type {name}\s*=\s*(?P<body>(?:\s*\|?\s*'[^']*')+)\s*;", text)
    return match, set(re.findall(r"'([^']*)'", match.group("body"))) if match else set()


def check_label_type_unions(members: list[str], tag_labels: set[str]) -> bool:
    """The npm label types list exactly the labels the library prints and filters on."""
    text = strip_comments(Path(WASM_INDEX).read_text())
    failed = False
    for name, expected in (("PosLabel", {POS_LABELS[member][0] for member in members}), ("TagPosLabel", tag_labels)):
        match, found = ts_string_union(text, name)
        if match is None:
            print(f"❌ {WASM_INDEX}: export type {name} not found as a string-literal union")
            failed = True
        elif found != expected:
            print(f"❌ {WASM_INDEX}: {name} drift: expected {sorted(expected)}, got {sorted(found)}")
            failed = True
    return failed


def check_no_local_label_tables() -> bool:
    """A binding that quotes several POS labels has re-introduced a local label table."""
    japanese = {ja for _, ja in POS_LABELS.values()}
    failed = False
    for pattern in BINDING_SOURCES:
        for path in sorted(Path().glob(pattern)):
            text = strip_comments(path.read_text())
            if path.as_posix() == WASM_INDEX:
                # The exported PosLabel type is checked against the C table by check_label_type_unions.
                match, _ = ts_string_union(text, "PosLabel")
                if match is not None:
                    text = text[: match.start()] + text[match.end() :]
            literals = set(re.findall(r"""["']([^"'\\\n]+)["']""", text))
            found = sorted((literals & japanese) | (literals & TABLE_ONLY_POS_LABELS))
            if len(found) >= LOCAL_TABLE_THRESHOLD:
                print(f"❌ {path}: quotes POS labels {found}; decode them through suzume_pos_label/_ja instead")
                failed = True
    return failed


def removed_name_pattern(name: str) -> re.Pattern[str]:
    """A whole-word match where `-` counts as part of the word (--dict is not --dictionary)."""
    body = r"\s+".join(re.escape(part) for part in name.split())
    return re.compile(rf"(?<![\w-]){body}(?![\w-])")


def check_removed_names() -> bool:
    """Docs, examples and CLI help name none of the removed public names."""
    patterns = {name: removed_name_pattern(name) for name in REMOVED_NAMES}
    failed = False
    for pattern in REMOVED_NAME_SCOPE:
        for path in sorted(Path().glob(pattern)):
            if not path.is_file():
                continue
            try:
                text = path.read_text()
            except UnicodeDecodeError:
                continue
            for number, line in enumerate(text.splitlines(), start=1):
                for name, regex in patterns.items():
                    if regex.search(line):
                        print(f"❌ {path}:{number}: names removed `{name}`")
                        failed = True
    return failed


def ts_fixture_loaded(suite: Path, fixture: Path) -> bool:
    """True when the TS suite binds the fixture's URL to a name and JSON-parses that file."""
    text = re.sub(r"/\*.*?\*/", "", suite.read_text(encoding="utf-8"), flags=re.DOTALL)
    text = re.sub(r"(?m)^\s*//.*$", "", text)
    url = r"const\s+(\w+)\s*=\s*fileURLToPath\(\s*new URL\(\s*['\"]([^'\"]+)['\"]\s*,\s*import\.meta\.url\s*\)\s*\)"
    for match in re.finditer(url, text):
        name, relative = match.groups()
        if (suite.parent / relative).resolve() == fixture and re.search(
            rf"JSON\.parse\(\s*readFileSync\(\s*{name}\b", text
        ):
            return True
    return False


def python_fixture_path(suite: Path, value: ast.expr) -> Path | None:
    """Resolve ``Path(__file__).resolve().parents[N] / "a" / "b"`` statically, or None for any other shape."""
    parts: list[str] = []
    while isinstance(value, ast.BinOp) and isinstance(value.op, ast.Div):
        if not (isinstance(value.right, ast.Constant) and isinstance(value.right.value, str)):
            return None
        parts.insert(0, value.right.value)
        value = value.left
    if not (
        isinstance(value, ast.Subscript)
        and isinstance(value.value, ast.Attribute)
        and value.value.attr == "parents"
        and isinstance(value.slice, ast.Constant)
        and isinstance(value.slice.value, int)
    ):
        return None
    return suite.resolve().parents[value.slice.value].joinpath(*parts)


def python_fixture_loaded(suite: Path, fixture: Path) -> bool:
    """True when the Python suite binds the fixture path to a name and passes that file to ``json.loads``."""
    tree = ast.parse(suite.read_text(encoding="utf-8"))
    names = {
        target.id
        for node in ast.walk(tree)
        if isinstance(node, ast.Assign) and python_fixture_path(suite, node.value) == fixture
        for target in node.targets
        if isinstance(target, ast.Name)
    }
    return any(
        isinstance(node, ast.Call)
        and isinstance(node.func, ast.Attribute)
        and node.func.attr == "loads"
        and isinstance(node.func.value, ast.Name)
        and node.func.value.id == "json"
        and any(isinstance(inner, ast.Name) and inner.id in names for arg in node.args for inner in ast.walk(arg))
        for node in ast.walk(tree)
    )


def check_conformance_fixture_loaded() -> bool:
    """Both binding suites read their error and option cases from the one shared fixture."""
    fixture = Path(CONFORMANCE_FIXTURE).resolve()
    if not fixture.is_file():
        print(f"❌ {CONFORMANCE_FIXTURE} is missing")
        return True
    failed = False
    for suite in CONFORMANCE_SUITES:
        path = Path(suite)
        loaded = ts_fixture_loaded(path, fixture) if path.suffix == ".ts" else python_fixture_loaded(path, fixture)
        if not loaded:
            print(f"❌ {suite} does not load {CONFORMANCE_FIXTURE}; read its cases instead of local expectations")
            failed = True
    return failed


def normalized_name(name: str) -> str:
    return name.replace("_", "").lower()


def c_numeric_constants(prefix: str) -> dict[str, int]:
    text = Path(C_HEADER).read_text()
    values: dict[str, int] = {}
    for name, expression in re.findall(rf"\b{prefix}([A-Z0-9_]+)\s*=\s*([^,\n]+)", text):
        if name.startswith("FIELD_"):
            continue  # offsetof field indexes share the prefix but are not values
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


# A C++ member with no initializer (an empty string or container) stands for
# the C default of NULL or 0.
DEFAULT_CONSTRUCTED = object()


def cpp_struct_defaults(name: str, fields: tuple[str, ...], names: dict[str, str]) -> dict[str, object]:
    text = Path(CPP_WRAPPER).read_text()
    match = re.search(rf"struct {name}\s*\{{", text)
    if match is None:
        raise SystemExit(f"❌ {CPP_WRAPPER}: cannot find `struct {name}`")
    body = strip_comments(enclosed(text, match.end() - 1, "{", "}", CPP_WRAPPER))
    members = {names.get(field, field): field for field in fields}
    defaults: dict[str, object] = {}
    for statement in body.split(";"):
        match = re.search(r"\b(\w+)\s*(?:=\s*([^;]+))?$", statement.strip())
        if match is None:
            continue
        member, expression = match.groups()
        if member not in members:
            continue
        field = members[member]
        defaults[field] = DEFAULT_CONSTRUCTED if expression is None else parse_default_literal(expression, CPP_WRAPPER)
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
        "options": c_initializer_defaults("suzume_init_options", DEFAULT_FIELDS["options"]),
        "tag": c_initializer_defaults("suzume_init_tag_options", DEFAULT_FIELDS["tag"]),
    }
    cpp = {
        "options": cpp_struct_defaults("Options", DEFAULT_FIELDS["options"], CPP_DEFAULT_NAMES["options"]),
        "tag": cpp_struct_defaults("TagOptions", DEFAULT_FIELDS["tag"], CPP_DEFAULT_NAMES["tag"]),
    }
    wasm_raw = {
        "options": ts_default_object("EXTENDED_OPTION_DEFAULTS"),
        "tag": ts_default_object("TAG_OPTION_DEFAULTS"),
    }
    wasm = {
        kind: {field: wasm_raw[kind][name] for field, name in names.items()}
        for kind, names in WASM_DEFAULT_NAMES.items()
    }
    python_raw = {
        "options": python_method_defaults("Suzume", "__init__"),
        "tag": python_method_defaults("Suzume", "generate_tags"),
    }
    python = {
        kind: {
            field: (
                0
                if field in PYTHON_NONE_AS_ZERO and python_raw[kind][field] is None
                else python_raw[kind][PYTHON_DEFAULT_NAMES[kind].get(field, field)]
            )
            for field in fields
            if field not in UNMIRRORED_DEFAULTS["Python"][kind]
        }
        for kind, fields in DEFAULT_FIELDS.items()
    }

    failed = False
    for kind, expected in canonical.items():
        mirrors = {
            "C++": cpp[kind],
            "WASM": wasm[kind],
            "Python": python[kind],
        }
        for surface, raw in mirrors.items():
            actual = {
                field: expected[field] if value is DEFAULT_CONSTRUCTED and expected[field] in (None, 0) else value
                for field, value in raw.items()
            }
            unmirrored = UNMIRRORED_DEFAULTS.get(surface, {}).get(kind, set())
            comparable = {field: value for field, value in expected.items() if field not in unmirrored}
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


def tag_filter_labels(pos_members: list[str]) -> tuple[dict[str, int], bool]:
    """Each SUZUME_TAG_POS_* bit keyed by the POS label it selects: bit n is SUZUME_POS_* code n+1."""
    labels: dict[str, int] = {}
    failed = False
    for name, value in c_numeric_constants("SUZUME_TAG_POS_").items():
        member = pos_members[value.bit_length()]
        if normalized_name(member) != name:
            print(f"❌ SUZUME_TAG_POS_{name.upper()} selects POS code {value.bit_length()} ({member})")
            failed = True
        labels[POS_LABELS[member][0]] = value
    return labels, failed


def check_numeric_contracts(tag_labels: dict[str, int]) -> bool:
    failed = False
    wasm = Path(WASM_INDEX).read_text()
    wasm_labels = Path(WASM_LABELS).read_text()
    python = Path(PYTHON_API).read_text()
    python_labels = Path(PYTHON_LABELS).read_text()

    canonical_errors = c_numeric_constants("SUZUME_ERROR_")
    # 255 is reserved for the bindings: a code the binding does not know.
    binding_errors = {**canonical_errors, "unknown": 255}
    wasm_error_body = enclosed(wasm, wasm.index("{", wasm.index("export const ErrorCode")), "{", "}", WASM_INDEX)
    wasm_errors = {
        normalized_name(name): int(value) for name, value in re.findall(r"\b(\w+)\s*:\s*(\d+)", wasm_error_body)
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

    wrapper_text = Path(CPP_WRAPPER).read_text()
    wrapper_error_match = re.search(r"enum class ErrorCode[^{]*\{(?P<body>.*?)\}", wrapper_text, re.DOTALL)
    wrapper_errors: dict[str, int] = {}
    for name, value in re.findall(
        r"\b(\w+)\s*=\s*(\w+)", wrapper_error_match.group("body") if wrapper_error_match else ""
    ):
        if value.isdigit():
            wrapper_errors[normalized_name(name)] = int(value)
        elif value.startswith("SUZUME_ERROR_"):
            wrapper_errors[normalized_name(name)] = canonical_errors.get(
                normalized_name(value[len("SUZUME_ERROR_") :]), -1
            )
        else:
            wrapper_errors[normalized_name(name)] = -1

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

    # Tag filters are spelled with the output POS labels, so keys compare exactly.
    canonical_tags = tag_labels
    wasm_tag_body = enclosed(wasm, wasm.index("{", wasm.index("TAG_POS_FILTER_BITS")), "{", "}", WASM_INDEX)
    wasm_tags = {name: int(value) for name, value in re.findall(r"\b(\w+)\s*:\s*(\d+)", wasm_tag_body)}
    python_tag_match = re.search(r"_POS_FILTER_BITS\s*=\s*\{(?P<body>.*?)\}", python, re.DOTALL)
    python_tags = (
        {name: int(value) for name, value in re.findall(r'"(\w+)"\s*:\s*(\d+)', python_tag_match.group("body"))}
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
        ("error codes", binding_errors, (wrapper_errors, wasm_errors, python_errors)),
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
        "pos_en": [POS_LABELS[name][0] for name in members["PartOfSpeech"]],
        "pos_ja": [POS_LABELS[name][1] for name in members["PartOfSpeech"]],
        "extended": cpp_switch_labels(CPP_TYPES, "extendedPosToString", "ExtendedPOS", members["ExtendedPOS"]),
        "conj_type": [CONJUGATION_TYPE_LABELS[name] for name in members["ConjugationType"]],
        "conj_form": [CONJUGATION_FORM_LABELS[name] for name in members["ConjForm"]],
    }

    checks = [
        (
            CPP_TYPES,
            "posToString",
            members["PartOfSpeech"],
            expected["pos_en"],
            cpp_switch_labels(CPP_TYPES, "posToString", "PartOfSpeech", members["PartOfSpeech"]),
        ),
        (
            CPP_TYPES,
            "posToJapanese",
            members["PartOfSpeech"],
            expected["pos_ja"],
            cpp_switch_labels(CPP_TYPES, "posToJapanese", "PartOfSpeech", members["PartOfSpeech"]),
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
    ]

    # Evaluate every group before combining so one failure cannot hide another.
    results = [compare(*check) for check in checks]
    results.append(
        check_distinct_labels(CPP_TYPES, "extendedPosToString", members["ExtendedPOS"], expected["extended"])
    )
    results.append(check_canonical_calls())
    tag_labels, tag_failed = tag_filter_labels(members["PartOfSpeech"])
    results.append(tag_failed)
    results.append(check_no_local_label_tables())
    results.append(check_label_type_unions(members["PartOfSpeech"], set(tag_labels)))
    results.append(check_numeric_contracts(tag_labels))
    results.append(check_default_contracts())
    results.append(check_removed_names())
    results.append(check_conformance_fixture_loaded())
    if any(results):
        print()
        print("A mislabeled numeric code changes public output. Fix the native table or the binding above.")
        return 1

    print("✅ C++, WASM, and Python labels and numeric contracts match")
    return 0


if __name__ == "__main__":
    sys.exit(main())
