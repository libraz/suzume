"""Dictionary tools ported from dict_tool.pl - MCP tool registration."""

import json
import re
from pathlib import Path

from ..core.mecab import mecab_analyze
from ..core.pos_mapping import map_mecab_pos
from ..core.suzume_utils import get_char_types, get_suzume_rule
from ..server import PROJECT_ROOT, mcp

# User dictionary categories
USER_CATEGORIES = [
    "entertainment",
    "music",
    "internet",
    "names",
    "people",
    "places",
    "brands",
    "organizations",
    "adult",
]

# POS → dictionary file mapping
POS_TO_FILE = {
    "ADJECTIVE": "data/core/adjectives.tsv",
    "ADVERB": "data/core/adverbs.tsv",
    "VERB": "data/core/verbs.tsv",
    "NOUN": "data/core/nouns.tsv",
    "PROPER_NOUN": "data/core/nouns.tsv",
    "PRONOUN": "data/core/expressions.tsv",
    "INTERJECTION": "data/core/expressions.tsv",
    "CONJUNCTION": "data/core/expressions.tsv",
    "PARTICLE": "data/core/expressions.tsv",
    "SUFFIX": "data/core/suffixes.tsv",
    "PREFIX": "data/core/suffixes.tsv",
    "OTHER": "data/core/expressions.tsv",
    "PHRASE": "data/core/expressions.tsv",
    "AUX": "data/core/expressions.tsv",
}

ALL_DICT_FILES = [
    "data/core/adjectives.tsv",
    "data/core/adverbs.tsv",
    "data/core/verbs.tsv",
    "data/core/nouns.tsv",
    "data/core/expressions.tsv",
    "data/core/suffixes.tsv",
]

VALID_POS = [
    "ADJECTIVE",
    "ADVERB",
    "VERB",
    "NOUN",
    "PROPER_NOUN",
    "PRONOUN",
    "PREFIX",
    "SUFFIX",
    "INTERJECTION",
    "ADNOMINAL",
    "CONJUNCTION",
    "PARTICLE",
    "AUX",
    "OTHER",
]

VALID_CONJ = [
    "I_ADJ",
    "NA_ADJ",
    "GODAN_KA",
    "GODAN_GA",
    "GODAN_SA",
    "GODAN_TA",
    "GODAN_NA",
    "GODAN_BA",
    "GODAN_MA",
    "GODAN_RA",
    "GODAN_WA",
    "ICHIDAN",
    "SURU",
    "KURU",
    "IRREGULAR",
]

# POS mapping: SuzumeUtils POS → dictionary format
_DICT_POS_MAP = {
    "Noun": "NOUN",
    "Verb": "VERB",
    "Adjective": "ADJECTIVE",
    "Adverb": "ADVERB",
    "Prefix": "PREFIX",
    "Suffix": "SUFFIX",
    "Pronoun": "PRONOUN",
    "Interjection": "INTERJECTION",
    "Adnominal": "ADNOMINAL",
    "Conjunction": "CONJUNCTION",
    "Particle": "PARTICLE",
    "Auxiliary": "AUXILIARY",
    "Symbol": "SYMBOL",
    "Filler": "FILLER",
    "Other": "OTHER",
}


def _to_dict_pos(pos: str) -> str:
    return _DICT_POS_MAP.get(pos, pos.upper())


def _map_conj_type(token: dict) -> str:
    ctype = token.get("conj_type", "")
    if "一段" in ctype:
        return "ICHIDAN"
    for row, name in [
        ("カ行", "GODAN_KA"),
        ("ガ行", "GODAN_GA"),
        ("サ行", "GODAN_SA"),
        ("タ行", "GODAN_TA"),
        ("ナ行", "GODAN_NA"),
        ("バ行", "GODAN_BA"),
        ("マ行", "GODAN_MA"),
        ("ラ行", "GODAN_RA"),
        ("ワ行", "GODAN_WA"),
    ]:
        if f"五段・{row}" in ctype:
            return name
    if "サ変" in ctype:
        return "SURU"
    if "カ変" in ctype:
        return "KURU"
    if "形容詞" in ctype:
        return "I_ADJ"
    return ""


def _load_dictionary() -> tuple[list[dict], dict[str, list[dict]]]:
    """Load all core dictionary entries."""
    entries = []
    by_surface: dict[str, list[dict]] = {}
    for file_rel in ALL_DICT_FILES:
        filepath = PROJECT_ROOT / file_rel
        if not filepath.exists():
            continue
        for line_num, line in enumerate(filepath.read_text(encoding="utf-8").splitlines(), 1):
            if line.startswith("#") or not line.strip():
                continue
            fields = line.split("\t")
            entry = {
                "surface": fields[0],
                "pos": fields[1] if len(fields) > 1 else "",
                "conj_type": fields[2] if len(fields) > 2 else "",
                "line_num": line_num,
                "raw": line,
                "file": file_rel,
            }
            entries.append(entry)
            by_surface.setdefault(fields[0], []).append(entry)
    return entries, by_surface


def _find_word_in_files(word: str) -> str | None:
    """Find word in core dict files, return file path or None."""
    for file_rel in ALL_DICT_FILES:
        filepath = PROJECT_ROOT / file_rel
        if not filepath.exists():
            continue
        for line in filepath.read_text(encoding="utf-8").splitlines():
            if line.startswith("#") or not line.strip():
                continue
            surface = line.split("\t")[0]
            if surface == word:
                return file_rel
            if line.startswith("#DISABLED# "):
                disabled_surface = line[len("#DISABLED# ") :].split("\t")[0]
                if disabled_surface == word:
                    return file_rel
    return None


def _find_word_in_user_files(word: str) -> str | None:
    """Find word in user dict files."""
    for cat in USER_CATEGORIES:
        filepath = PROJECT_ROOT / f"data/user/{cat}.tsv"
        if not filepath.exists():
            continue
        for line in filepath.read_text(encoding="utf-8").splitlines():
            if line.startswith("#") or not line.strip():
                continue
            surface = line.split("\t")[0]
            if surface == word:
                return f"data/user/{cat}.tsv"
    return None


def _json_result(obj: dict) -> str:
    """Serialize result dict to JSON string."""
    return json.dumps(obj, ensure_ascii=False, indent=2)


def _token_to_dict(token: dict) -> dict:
    """Convert a MeCab token dict to a clean JSON-serializable dict."""
    return {
        "surface": token.get("surface", ""),
        "pos": token.get("pos", ""),
        "pos_sub1": token.get("pos_sub1", ""),
        "conj_form": token.get("conj_form", ""),
        "lemma": token.get("lemma", ""),
    }


async def _recompile_core_dic() -> str:
    """Recompile core dictionary."""
    import subprocess

    cli = PROJECT_ROOT / "build" / "bin" / "suzume-cli"
    if not cli.exists():
        return "not_found"

    # Concatenate all TSV files
    import tempfile

    with tempfile.NamedTemporaryFile(mode="w", suffix=".tsv", delete=False, encoding="utf-8") as f:
        for file_rel in ALL_DICT_FILES:
            filepath = PROJECT_ROOT / file_rel
            if filepath.exists():
                f.write(filepath.read_text(encoding="utf-8"))
        tmp_path = f.name

    try:
        result = subprocess.run(
            [str(cli), "dict", "compile", tmp_path, str(PROJECT_ROOT / "data/core.dic")],
            capture_output=True,
            text=True,
            timeout=30,
        )
        if result.returncode == 0:
            return "ok"
        return "failed"
    finally:
        Path(tmp_path).unlink(missing_ok=True)


async def _recompile_user_dic() -> str:
    """Recompile user dictionary."""
    import subprocess

    cli = PROJECT_ROOT / "build" / "bin" / "suzume-cli"
    if not cli.exists():
        return "not_found"

    import tempfile

    with tempfile.NamedTemporaryFile(mode="w", suffix=".tsv", delete=False, encoding="utf-8") as f:
        for cat in USER_CATEGORIES:
            filepath = PROJECT_ROOT / f"data/user/{cat}.tsv"
            if filepath.exists():
                f.write(filepath.read_text(encoding="utf-8"))
        tmp_path = f.name

    try:
        result = subprocess.run(
            [str(cli), "dict", "compile", tmp_path, str(PROJECT_ROOT / "data/user.dic")],
            capture_output=True,
            text=True,
            timeout=30,
        )
        if result.returncode == 0:
            return "ok"
        return "failed"
    finally:
        Path(tmp_path).unlink(missing_ok=True)


@mcp.tool()
async def dict_check(word: str) -> str:
    """Check if a word can be added to the dictionary (MeCab analysis + duplicates).

    Args:
        word: Japanese word to check.
    """
    result: dict = {"word": word}

    # Suzume rule check
    suzume_rule = get_suzume_rule(word)
    result["suzume_rule"] = suzume_rule if suzume_rule else None

    # MeCab analysis
    tokens = mecab_analyze(word)
    is_single = len(tokens) == 1
    is_base = is_single and tokens[0].get("conj_form", "") == "基本形"

    result["mecab"] = {
        "is_single": is_single,
        "is_base_form": is_base,
        "tokens": [_token_to_dict(t) for t in tokens],
    }

    # Dictionary check
    _, by_surface = _load_dictionary()
    result["in_dictionary"] = word in by_surface
    if word in by_surface:
        result["existing_entries"] = [f"{e['surface']}\t{e['pos']}\t{e['conj_type']}" for e in by_surface[word]]
        result["suggestion"] = None
        return _json_result(result)
    else:
        result["existing_entries"] = []

    # Suggestion
    result["suggestion"] = None
    if is_single:
        suggested_pos = _to_dict_pos(map_mecab_pos(tokens[0]))
        suggested_conj = _map_conj_type(tokens[0])
        if suggested_pos:
            entry = f"{word}\t{suggested_pos}"
            if suggested_conj:
                entry += f"\t{suggested_conj}"
            cmd = f'dict_add word="{word}" pos="{suggested_pos}"'
            if suggested_conj:
                cmd += f' conj_type="{suggested_conj}"'
            result["suggestion"] = {"entry": entry, "command": cmd}
    elif len(tokens) > 1:
        result["suggestion"] = {
            "entry": f"{word}\tNOUN",
            "command": f'dict_add word="{word}" pos="NOUN" force=True',
        }

    return _json_result(result)


@mcp.tool()
async def dict_suggest(word: str) -> str:
    """Suggest POS and conjugation type for a word based on MeCab analysis.

    Args:
        word: Japanese word to analyze.
    """
    tokens = mecab_analyze(word)
    if not tokens:
        return _json_result({"status": "error", "message": "Unknown word (not in MeCab dictionary)"})

    if len(tokens) > 1:
        result = {
            "word": word,
            "is_split": True,
            "tokens": [_token_to_dict(t) for t in tokens],
            "mecab": None,
            "suggestion": {
                "pos": "NOUN",
                "conj_type": None,
                "command": f'dict_add word="{word}" pos="NOUN"',
            },
        }
        return _json_result(result)

    tok = tokens[0]
    pos = _to_dict_pos(map_mecab_pos(tok))
    conj = _map_conj_type(tok)

    result: dict = {
        "word": word,
        "is_split": False,
        "mecab": {
            "pos": tok.get("pos", ""),
            "pos_sub1": tok.get("pos_sub1", ""),
            "lemma": tok.get("lemma", ""),
            "conj_form": tok.get("conj_form", ""),
        },
        "suggestion": None,
    }

    if pos:
        suggestion: dict = {"pos": pos, "conj_type": conj if conj else None}
        cmd = f'dict_add word="{word}" pos="{pos}"'
        if conj:
            cmd += f' conj_type="{conj}"'
        suggestion["command"] = cmd
        result["suggestion"] = suggestion
    else:
        result["suggestion"] = None
        result["message"] = f"Cannot suggest POS for: {tok['pos']}. This may be a closed-class word (use L1 instead)."

    return _json_result(result)


@mcp.tool()
async def dict_add(
    word: str,
    pos: str,
    conj_type: str = "",
    user: str = "",
    force: bool = False,
    dry_run: bool = False,
) -> str:
    """Add a word to the dictionary with safety checks.

    Args:
        word: Word to add.
        pos: POS value (NOUN, VERB, ADJECTIVE, ADVERB, PROPER_NOUN, etc.).
        conj_type: Conjugation type (I_ADJ, NA_ADJ, GODAN_KA, ICHIDAN, etc.).
        user: User dictionary category (entertainment, adult, etc.). Empty for core dict.
        force: Allow adding even if MeCab splits the word.
        dry_run: Preview only, don't modify files.
    """
    if pos not in VALID_POS:
        return _json_result({"status": "error", "message": f"Invalid POS: {pos}. Valid values: {', '.join(VALID_POS)}"})
    if conj_type and conj_type not in VALID_CONJ:
        return _json_result(
            {"status": "error", "message": f"Invalid conj_type: {conj_type}. Valid values: {', '.join(VALID_CONJ)}"}
        )
    if user and user not in USER_CATEGORIES:
        return _json_result(
            {
                "status": "error",
                "message": f"Invalid user category: {user}. Valid categories: {', '.join(USER_CATEGORIES)}",
            }
        )

    # Duplicate check
    _, by_surface = _load_dictionary()
    if word in by_surface:
        return _json_result({"status": "error", "message": f"DUPLICATE: '{word}' already exists in dictionary"})

    # MeCab check
    tokens = mecab_analyze(word)
    skip_conj_check = pos == "PROPER_NOUN"
    suzume_rule = get_suzume_rule(word)

    if len(tokens) > 1 and not force and not skip_conj_check:
        if suzume_rule:
            pass  # OK, suzume keeps as 1 token
        else:
            if len(tokens) == 2 and tokens[1]["pos"] in ("助動詞", "助詞"):
                return _json_result(
                    {
                        "status": "error",
                        "message": f"REJECT: This appears to be a conjugated form. Register '{tokens[0].get('lemma', '')}' instead, or use PROPER_NOUN.",
                        "word": word,
                        "tokens": [_token_to_dict(t) for t in tokens],
                    }
                )
            return _json_result(
                {
                    "status": "error",
                    "message": f"MeCab splits '{word}' into {len(tokens)} tokens. To add anyway, use force=True.",
                    "word": word,
                    "tokens": [_token_to_dict(t) for t in tokens],
                }
            )

    entry_line = f"{word}\t{pos}"
    if conj_type:
        entry_line += f"\t{conj_type}"

    if user:
        # User dictionary
        dup = _find_word_in_user_files(word)
        if dup:
            return _json_result(
                {"status": "error", "message": f"DUPLICATE: '{word}' already exists in user dictionary ({dup})"}
            )

        target_file = PROJECT_ROOT / f"data/user/{user}.tsv"
        target_rel = f"data/user/{user}.tsv"
        if dry_run:
            return _json_result(
                {"status": "ok", "word": word, "entry": entry_line, "file": target_rel, "dry_run": True}
            )

        with open(target_file, "a", encoding="utf-8") as f:
            f.write(f"{entry_line}\n")

        recompile_status = await _recompile_user_dic()
        return _json_result(
            {"status": "ok", "word": word, "entry": entry_line, "file": target_rel, "recompile": recompile_status}
        )

    # Core dictionary
    target_file_rel = POS_TO_FILE.get(pos, "data/core/nouns.tsv")
    target_file = PROJECT_ROOT / target_file_rel

    if dry_run:
        return _json_result(
            {"status": "ok", "word": word, "entry": entry_line, "file": target_file_rel, "dry_run": True}
        )

    with open(target_file, "a", encoding="utf-8") as f:
        f.write(f"{entry_line}\n")

    recompile_status = await _recompile_core_dic()
    return _json_result(
        {"status": "ok", "word": word, "entry": entry_line, "file": target_file_rel, "recompile": recompile_status}
    )


@mcp.tool()
async def dict_remove(word: str, user: str = "", dry_run: bool = False) -> str:
    """Remove a word from the dictionary.

    Args:
        word: Word to remove.
        user: User dictionary category (empty for core dict).
        dry_run: Preview only.
    """
    if user:
        filepath = PROJECT_ROOT / f"data/user/{user}.tsv"
        file_rel = f"data/user/{user}.tsv"
        if not filepath.exists():
            return _json_result({"status": "error", "message": f"User dict file not found: {file_rel}"})
    else:
        file_rel = _find_word_in_files(word)
        if not file_rel:
            return _json_result({"status": "error", "message": f"Word not found in dictionary: {word}"})
        filepath = PROJECT_ROOT / file_rel

    lines = filepath.read_text(encoding="utf-8").splitlines()
    new_lines = []
    removed = None
    for line in lines:
        if not line.startswith("#") and line.strip():
            surface = line.split("\t")[0]
            if surface == word:
                removed = line
                continue
        new_lines.append(line)

    if not removed:
        return _json_result({"status": "error", "message": f"Word not found: {word}"})
    if dry_run:
        return _json_result({"status": "ok", "word": word, "removed_entry": removed, "file": file_rel, "dry_run": True})

    filepath.write_text("\n".join(new_lines) + ("\n" if new_lines else ""), encoding="utf-8")
    recompile_status = await (_recompile_user_dic() if user else _recompile_core_dic())
    return _json_result(
        {"status": "ok", "word": word, "removed_entry": removed, "file": file_rel, "recompile": recompile_status}
    )


@mcp.tool()
async def dict_disable(word: str, dry_run: bool = False) -> str:
    """Disable a dictionary entry by commenting it out (keeps in file but inactive).

    Args:
        word: Word to disable.
        dry_run: Preview only.
    """
    file_rel = _find_word_in_files(word)
    if not file_rel:
        return _json_result({"status": "error", "message": f"Word not found in dictionary: {word}"})

    filepath = PROJECT_ROOT / file_rel
    lines = filepath.read_text(encoding="utf-8").splitlines()
    found = False
    for idx, line in enumerate(lines):
        if not line.startswith("#") and line.strip():
            surface = line.split("\t")[0]
            if surface == word:
                found = True
                if dry_run:
                    return _json_result(
                        {"status": "ok", "word": word, "file": file_rel, "entry": line, "dry_run": True}
                    )
                lines[idx] = f"#DISABLED# {line}"
                break

    if not found:
        return _json_result({"status": "error", "message": f"Word not found: {word}"})

    filepath.write_text("\n".join(lines) + "\n", encoding="utf-8")
    recompile_status = await _recompile_core_dic()
    return _json_result({"status": "ok", "word": word, "file": file_rel, "recompile": recompile_status})


@mcp.tool()
async def dict_enable(word: str, dry_run: bool = False) -> str:
    """Re-enable a disabled dictionary entry.

    Args:
        word: Word to enable.
        dry_run: Preview only.
    """
    file_rel = _find_word_in_files(word)
    if not file_rel:
        return _json_result({"status": "error", "message": f"Disabled word not found: {word}"})

    filepath = PROJECT_ROOT / file_rel
    lines = filepath.read_text(encoding="utf-8").splitlines()
    found = False
    for idx, line in enumerate(lines):
        if line.startswith("#DISABLED# "):
            disabled_surface = line[len("#DISABLED# ") :].split("\t")[0]
            if disabled_surface == word:
                found = True
                if dry_run:
                    return _json_result(
                        {"status": "ok", "word": word, "file": file_rel, "entry": line, "dry_run": True}
                    )
                lines[idx] = line[len("#DISABLED# ") :]
                break

    if not found:
        return _json_result({"status": "error", "message": f"Disabled word not found: {word}"})

    filepath.write_text("\n".join(lines) + "\n", encoding="utf-8")
    recompile_status = await _recompile_core_dic()
    return _json_result({"status": "ok", "word": word, "file": file_rel, "recompile": recompile_status})


@mcp.tool()
async def dict_validate(fix: bool = False) -> str:
    """Validate dictionary for issues (conjugated forms, duplicates).

    Args:
        fix: If True, remove problematic entries. Default is report-only.
    """
    entries, by_surface = _load_dictionary()

    conjugated_forms = []
    duplicates = []
    mecab_split = []

    for entry in entries:
        tokens = mecab_analyze(entry["surface"])
        if len(tokens) > 1:
            # Check for conjugated forms
            is_conj = _is_conjugated_form(tokens, entry)
            if is_conj:
                conjugated_forms.append(
                    {
                        "entry": entry,
                        "tokens": tokens,
                        "lemma": tokens[0].get("lemma", ""),
                        "reason": is_conj,
                    }
                )
            else:
                mecab_split.append({"entry": entry, "tokens": tokens})

    for surface, entries_list in by_surface.items():
        if len(entries_list) > 1:
            duplicates.append({"surface": surface, "entries": entries_list})

    result: dict = {
        "total_entries": len(entries),
        "conjugated_forms": [
            {
                "surface": cf["entry"]["surface"],
                "line_num": cf["entry"]["line_num"],
                "reason": cf["reason"],
                "lemma": cf["lemma"],
            }
            for cf in conjugated_forms
        ],
        "duplicates": [{"surface": dup["surface"], "count": len(dup["entries"])} for dup in duplicates],
        "compound_words": len(mecab_split),
        "fixed": False,
    }

    issues = len(conjugated_forms) + len(duplicates)
    if issues > 0 and fix:
        # Remove lines (conjugated + duplicate extras)
        lines_to_remove = set()
        for cf in conjugated_forms:
            lines_to_remove.add((cf["entry"]["file"], cf["entry"]["line_num"]))
        for dup in duplicates:
            for entry in dup["entries"][1:]:
                lines_to_remove.add((entry["file"], entry["line_num"]))

        for file_rel in set(f for f, _ in lines_to_remove):
            filepath = PROJECT_ROOT / file_rel
            file_lines = filepath.read_text(encoding="utf-8").splitlines()
            remove_nums = {ln for f, ln in lines_to_remove if f == file_rel}
            new_lines = [line for idx, line in enumerate(file_lines, 1) if idx not in remove_nums]
            filepath.write_text("\n".join(new_lines) + "\n", encoding="utf-8")

        recompile_status = await _recompile_core_dic()
        result["fixed"] = True
        result["removed_count"] = len(lines_to_remove)
        result["recompile"] = recompile_status

    return _json_result(result)


def _is_conjugated_form(tokens: list[dict], entry: dict) -> str:
    """Check if a multi-token result is a conjugated form that shouldn't be in dictionary."""
    pos = entry.get("pos", "")
    surface = entry.get("surface", "")

    # Fixed expressions whitelist
    fixed = {"申し訳ない", "仕方ない", "仕方がない", "違いない", "やむを得ない"}
    if surface in fixed:
        return ""

    suzume_rule = get_suzume_rule(surface)
    if suzume_rule:
        return ""

    # Skip certain POS
    if pos in ("ADVERB", "INTERJECTION", "CONJUNCTION", "PROPER_NOUN"):
        return ""

    if len(tokens) == 2:
        t1, t2 = tokens
        if t2["pos"] == "助動詞" and re.match(r"^(ない|た|だ|です|ます|れる|られる|せる|させる|ぬ|ん)$", t2["surface"]):
            return f"verb/adj + {t2['surface']} (auxiliary)"
        if t2["surface"] == "て" and "連用" in t1.get("conj_form", ""):
            return "renyokei + te"
        if t2["surface"] == "ば" and t1["pos"] == "形容詞":
            return "adjective + ba (conditional)"
        if t2["surface"] == "た" and "連用" in t1.get("conj_form", ""):
            return "renyokei + ta"

    return ""


@mcp.tool()
async def dict_grep(pattern: str, user: str = "", search_all: bool = False) -> str:
    """Search dictionary entries matching a regex pattern (surface match).

    Args:
        pattern: Regex pattern to match against entry surfaces.
        user: Search specific user category dict. Empty for core dict.
        search_all: Search both core and all user dicts.
    """
    try:
        rx = re.compile(pattern)
    except re.error as exc:
        return _json_result({"status": "error", "message": f"Invalid regex: {exc}"})

    files: list[str] = []
    if user:
        files.append(f"data/user/{user}.tsv")
    elif search_all:
        files = list(ALL_DICT_FILES) + [f"data/user/{cat}.tsv" for cat in USER_CATEGORIES]
    else:
        files = list(ALL_DICT_FILES)

    matches = []
    for file_rel in files:
        filepath = PROJECT_ROOT / file_rel
        if not filepath.exists():
            continue
        for line in filepath.read_text(encoding="utf-8").splitlines():
            if not line.strip() or line.startswith("#"):
                continue
            surface = line.split("\t")[0]
            if rx.search(surface):
                matches.append({"file": file_rel, "entry": line})

    return _json_result({"pattern": pattern, "matches": matches, "total": len(matches)})


@mcp.tool()
async def dict_sort(
    file: str = "",
    user: str = "",
    dry_run: bool = True,
) -> str:
    """Sort dictionary entries by conjugation type and あいうえお order.

    Groups entries by conj_type (for verbs/adjectives) or POS, then sorts
    within each group by Japanese reading (using MeCab for kanji readings).

    Args:
        file: Specific dict file path relative to project root (e.g. "data/core/verbs.tsv").
        user: User dictionary category to sort (entertainment, adult, etc.).
        dry_run: If True (default), preview the sorted result. Set False to apply.
    """
    if user:
        if user not in USER_CATEGORIES:
            return _json_result(
                {"status": "error", "message": f"Invalid user category: {user}. Valid: {', '.join(USER_CATEGORIES)}"}
            )
        target_rel = f"data/user/{user}.tsv"
    elif file:
        target_rel = file
    else:
        return _json_result(
            {"status": "error", "message": "Specify file (e.g. 'data/core/verbs.tsv') or user category."}
        )

    filepath = PROJECT_ROOT / target_rel
    if not filepath.exists():
        return _json_result({"status": "error", "message": f"File not found: {target_rel}"})

    content = filepath.read_text(encoding="utf-8")
    raw_lines = content.splitlines()

    # Extract header comments (contiguous block at the top)
    header_lines = []
    data_start = 0
    for idx, line in enumerate(raw_lines):
        if line.startswith("#") or not line.strip():
            header_lines.append(line)
            data_start = idx + 1
        else:
            break

    # Parse entries (skip inline comments and disabled entries)
    entries = []
    disabled_entries = []
    inline_comments = []  # non-header comments interspersed with data
    for line in raw_lines[data_start:]:
        if not line.strip():
            continue
        if line.startswith("#DISABLED# "):
            disabled_entries.append(line)
            continue
        if line.startswith("#"):
            # Inline section comments — we'll regenerate them
            inline_comments.append(line)
            continue
        fields = line.split("\t")
        entries.append(
            {
                "surface": fields[0],
                "pos": fields[1] if len(fields) > 1 else "",
                "conj_type": fields[2] if len(fields) > 2 else "",
                "raw": line,
            }
        )

    if not entries:
        return _json_result({"status": "error", "message": f"No entries found in {target_rel}"})

    # Get readings for sorting (あいうえお order)
    reading_cache: dict[str, str] = {}
    for entry in entries:
        surface = entry["surface"]
        if surface not in reading_cache:
            reading_cache[surface] = await _get_reading(surface)

    # Determine grouping strategy
    has_conj = any(ent["conj_type"] for ent in entries)
    has_mixed_pos = len(set(ent["pos"] for ent in entries)) > 1

    if has_conj:
        # Group by conj_type (verbs.tsv, adjectives.tsv)
        group_key = "conj_type"
        # Order: defined conjugation types first, then ungrouped
        group_order = [ctype for ctype in VALID_CONJ if any(ent["conj_type"] == ctype for ent in entries)]
        # Add any conj_type not in VALID_CONJ
        for ent in entries:
            if ent["conj_type"] and ent["conj_type"] not in group_order:
                group_order.append(ent["conj_type"])
        # Entries without conj_type go last
        if any(not ent["conj_type"] for ent in entries):
            group_order.append("")
    elif has_mixed_pos:
        # Group by POS (user dicts, expressions.tsv)
        group_key = "pos"
        group_order = [pos for pos in VALID_POS if any(ent["pos"] == pos for ent in entries)]
        for ent in entries:
            if ent["pos"] and ent["pos"] not in group_order:
                group_order.append(ent["pos"])
        if any(not ent["pos"] for ent in entries):
            group_order.append("")
    else:
        # Single POS, no conj_type — just sort alphabetically
        group_key = None
        group_order = [None]

    # Group and sort
    groups: dict[str | None, list[dict]] = {}
    for ent in entries:
        key = ent.get(group_key, None) if group_key else None
        groups.setdefault(key, []).append(ent)

    _CONJ_LABELS = {
        "I_ADJ": "I-adjectives (い形容詞)",
        "NA_ADJ": "NA-adjectives (な形容詞)",
        "GODAN_KA": "Godan-KA verbs (カ行五段)",
        "GODAN_GA": "Godan-GA verbs (ガ行五段)",
        "GODAN_SA": "Godan-SA verbs (サ行五段)",
        "GODAN_TA": "Godan-TA verbs (タ行五段)",
        "GODAN_NA": "Godan-NA verbs (ナ行五段)",
        "GODAN_BA": "Godan-BA verbs (バ行五段)",
        "GODAN_MA": "Godan-MA verbs (マ行五段)",
        "GODAN_RA": "Godan-RA verbs (ラ行五段)",
        "GODAN_WA": "Godan-WA verbs (ワ行五段)",
        "ICHIDAN": "Ichidan verbs (一段動詞)",
        "SURU": "Suru verbs (サ変)",
        "KURU": "Kuru verbs (カ変)",
        "IRREGULAR": "Irregular verbs (不規則)",
    }

    _POS_LABELS = {
        "ADJECTIVE": "Adjectives (形容詞)",
        "ADVERB": "Adverbs (副詞)",
        "VERB": "Verbs (動詞)",
        "NOUN": "Nouns (名詞)",
        "PROPER_NOUN": "Proper Nouns (固有名詞)",
        "PRONOUN": "Pronouns (代名詞)",
        "PREFIX": "Prefixes (接頭辞)",
        "SUFFIX": "Suffixes (接尾辞)",
        "INTERJECTION": "Interjections (感動詞)",
        "ADNOMINAL": "Adnominals (連体詞)",
        "CONJUNCTION": "Conjunctions (接続詞)",
        "PARTICLE": "Particles (助詞)",
        "AUX": "Auxiliaries (助動詞)",
        "OTHER": "Other (その他)",
        "PHRASE": "Phrases (フレーズ)",
    }

    # Build sorted output
    output_lines = list(header_lines)
    group_stats = []
    for key in group_order:
        group_entries = groups.get(key, [])
        if not group_entries:
            continue

        # Sort by reading
        group_entries.sort(key=lambda ent: reading_cache.get(ent["surface"], ent["surface"]))

        # Section comment
        if group_key:
            if group_key == "conj_type":
                label = _CONJ_LABELS.get(key, key or "Other")
            else:
                label = _POS_LABELS.get(key, key or "Other")
            output_lines.append(f"\n# --- {label} ---")
            group_stats.append({"name": label, "count": len(group_entries)})

        for ent in group_entries:
            output_lines.append(ent["raw"])

    # Append disabled entries at the end
    if disabled_entries:
        output_lines.append("\n# --- Disabled entries ---")
        for dis in disabled_entries:
            output_lines.append(dis)

    sorted_content = "\n".join(output_lines) + "\n"

    result: dict = {
        "file": target_rel,
        "total_entries": len(entries),
        "groups": group_stats,
        "disabled": len(disabled_entries),
        "applied": False,
    }

    if dry_run:
        result["dry_run"] = True
        preview_lines = sorted_content.splitlines()
        if len(preview_lines) > 100:
            preview = "\n".join(preview_lines[:100]) + f"\n... ({len(preview_lines) - 100} more lines)"
        else:
            preview = sorted_content
        result["preview"] = preview
        return _json_result(result)

    filepath.write_text(sorted_content, encoding="utf-8")
    is_user = target_rel.startswith("data/user/")
    recompile_status = await (_recompile_user_dic() if is_user else _recompile_core_dic())
    result["applied"] = True
    result["recompile"] = recompile_status
    return _json_result(result)


async def _get_reading(surface: str) -> str:
    """Get hiragana reading for a surface using MeCab, for sort ordering."""
    import regex

    # If already all hiragana/katakana, convert to hiragana for sorting
    if regex.fullmatch(r"[\p{Hiragana}\p{Katakana}ー]+", surface):
        # Convert katakana to hiragana
        return "".join(chr(ord(c) - 0x60) if "\u30a1" <= c <= "\u30f6" else c for c in surface)

    # Use MeCab to get reading
    tokens = mecab_analyze(surface)
    if tokens:
        readings = []
        for tok in tokens:
            reading = tok.get("reading", "")
            if reading and reading != "*":
                # Convert katakana reading to hiragana
                readings.append("".join(chr(ord(c) - 0x60) if "\u30a1" <= c <= "\u30f6" else c for c in reading))
            else:
                readings.append(tok["surface"])
        return "".join(readings)

    return surface


@mcp.tool()
async def dict_remove_matching(
    pattern: str,
    user: str = "",
    dry_run: bool = True,
) -> str:
    """Bulk remove dictionary entries matching a regex pattern.

    Args:
        pattern: Regex pattern to match against entry surfaces.
        user: User dictionary category (empty for core dict).
        dry_run: If True (default), preview without removing. Set False to apply.
    """
    try:
        rx = re.compile(pattern)
    except re.error as exc:
        return _json_result({"status": "error", "message": f"Invalid regex: {exc}"})

    files: list[str] = []
    if user:
        files.append(f"data/user/{user}.tsv")
    else:
        files = list(ALL_DICT_FILES)

    matches = []
    for file_rel in files:
        filepath = PROJECT_ROOT / file_rel
        if not filepath.exists():
            continue
        for line in filepath.read_text(encoding="utf-8").splitlines():
            if not line.strip() or line.startswith("#"):
                continue
            surface = line.split("\t")[0]
            if rx.search(surface):
                matches.append({"file": file_rel, "surface": surface, "entry": line})

    if not matches:
        return _json_result({"pattern": pattern, "matches": [], "total": 0, "applied": False})

    result: dict = {
        "pattern": pattern,
        "matches": matches,
        "total": len(matches),
        "applied": False,
    }

    if dry_run:
        result["dry_run"] = True
        return _json_result(result)

    # Group by file and remove
    by_file: dict[str, set[str]] = {}
    for match in matches:
        by_file.setdefault(match["file"], set()).add(match["surface"])

    for file_rel, surfaces_to_remove in by_file.items():
        filepath = PROJECT_ROOT / file_rel
        file_lines = filepath.read_text(encoding="utf-8").splitlines()
        new_lines = []
        for line in file_lines:
            if line.strip() and not line.startswith("#"):
                surface = line.split("\t")[0]
                if surface in surfaces_to_remove:
                    continue
            new_lines.append(line)
        filepath.write_text("\n".join(new_lines) + "\n", encoding="utf-8")

    recompile_status = await (_recompile_user_dic() if user else _recompile_core_dic())
    result["applied"] = True
    result["recompile"] = recompile_status
    return _json_result(result)


@mcp.tool()
async def dict_cleanup(
    input_file: str,
    dry_run: bool = True,
) -> str:
    """Analyze dictionary entries and separate needed/unneeded ones.

    Checks each entry against Suzume to determine if it's still needed
    (i.e., if Suzume would split the word without the dictionary entry).

    Args:
        input_file: Path to TSV dictionary file to analyze (relative to project root).
        dry_run: If True (default), only report. Set False to write keep/noise files.
    """
    import subprocess

    filepath = PROJECT_ROOT / input_file
    if not filepath.exists():
        return _json_result({"status": "error", "message": f"File not found: {input_file}"})

    cli = PROJECT_ROOT / "build" / "bin" / "suzume-cli"
    if not cli.exists():
        return _json_result({"status": "error", "message": "suzume-cli not found (build first)"})

    def is_fixed_expression(surface: str) -> bool:
        import regex

        return bool(regex.search(r"[\p{Han}][\p{Hiragana}][\p{Han}\p{Hiragana}\p{Katakana}]", surface))

    def is_split_by_suzume(surface: str) -> bool:
        try:
            result = subprocess.run(
                [str(cli), surface],
                capture_output=True,
                text=True,
                timeout=10,
            )
            lines = [line for line in result.stdout.strip().split("\n") if line and line != "EOS"]
            return len(lines) > 1
        except Exception:
            return True

    keep_lines = []
    noise_lines = []
    total = 0
    kept = 0
    details = []

    for line in filepath.read_text(encoding="utf-8").splitlines():
        if line.startswith("#") or not line.strip():
            keep_lines.append(line)
            continue

        fields = line.split("\t")
        if len(fields) < 2:
            keep_lines.append(line)
            continue

        surface = fields[0]
        total += 1

        # Determine if entry is needed
        reason = ""
        keep = True

        if len(surface) <= 2:
            keep = False
            reason = "too_short"
        elif not is_split_by_suzume(surface):
            keep = False
            reason = "handled_by_suzume"
        elif is_fixed_expression(surface):
            reason = "fixed_expression"
        elif len(get_char_types(surface)) > 1:
            reason = "mixed_chartype_compound"
        elif get_char_types(surface) and get_char_types(surface)[0] == "kanji" and len(surface) >= 4:
            reason = "kanji_compound"
        else:
            reason = "split_other"

        action = "KEEP" if keep else "DROP"
        details.append({"surface": surface, "action": action, "reason": reason})

        if keep:
            keep_lines.append(line)
            kept += 1
        else:
            noise_lines.append(f"{line}\t# {reason}")

    result: dict = {
        "file": input_file,
        "total": total,
        "keep": kept,
        "drop": total - kept,
        "details": details,
        "applied": False,
    }

    if dry_run:
        result["dry_run"] = True
        return _json_result(result)

    base = str(filepath).rsplit(".", 1)[0]
    keep_path = Path(f"{base}_keep.tsv")
    noise_path = Path(f"{base}_noise.tsv")

    keep_path.write_text("\n".join(keep_lines) + "\n", encoding="utf-8")
    noise_path.write_text("\n".join(noise_lines) + "\n", encoding="utf-8")

    result["applied"] = True
    result["keep_file"] = str(keep_path.relative_to(PROJECT_ROOT))
    result["noise_file"] = str(noise_path.relative_to(PROJECT_ROOT))
    return _json_result(result)
