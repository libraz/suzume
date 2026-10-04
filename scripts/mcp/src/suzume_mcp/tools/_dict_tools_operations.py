"""Dictionary lookup and single-entry mutation MCP tools."""

import re

from ..core.mecab import mecab_analyze
from ..core.pos_mapping import map_mecab_pos
from ..core.suzume_utils import get_suzume_rule
from ..server import PROJECT_ROOT, mcp
from ._dict_tools_common import (
    DISABLED_PREFIX,
    POS_TO_FILE,
    USER_CATEGORIES,
    VALID_CONJ,
    VALID_POS,
    _canonical_pos,
    _dict_add_command,
    _dict_files,
    _entry_line,
    _files_with_pos,
    _group_by_pos,
    _invalid_value,
    _json_error,
    _json_result,
    _load_all_entries,
    _load_dictionary,
    _map_conj_type,
    _recompile_core_dic,
    _recompile_response,
    _recompile_user_dic,
    _to_dict_pos,
    _token_to_dict,
    _user_file,
    _validate_surface,
    _with_appended_lines,
    _write_files_and_recompile,
)


@mcp.tool()
async def dict_check(word: str) -> str:
    """Check if a word can be added to the dictionary (MeCab analysis + duplicates).

    Args:
        word: Japanese word to check.
    """
    result: dict = {"word": word, "suzume_rule": get_suzume_rule(word) or None}

    tokens = mecab_analyze(word)
    is_single = len(tokens) == 1
    result["mecab"] = {
        "is_single": is_single,
        "is_base_form": is_single and tokens[0].get("conj_form", "") == "基本形",
        "tokens": [_token_to_dict(t) for t in tokens],
    }

    # Dictionary check (core + user)
    _, by_surface = _load_all_entries()
    result["in_dictionary"] = word in by_surface
    result["existing_entries"] = [
        f"{e['file']}: {e['surface']}\t{e['pos']}\t{e['conj_type']}" for e in by_surface.get(word, [])
    ]
    result["suggestion"] = None
    if word in by_surface:
        return _json_result(result)

    if is_single:
        suggested_pos = _to_dict_pos(map_mecab_pos(tokens[0]))
        suggested_conj = _map_conj_type(tokens[0])
        if suggested_pos:
            result["suggestion"] = {
                "entry": _entry_line(word, suggested_pos, suggested_conj),
                "command": _dict_add_command(word, suggested_pos, suggested_conj),
            }
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
        return _json_error("Unknown word (not in MeCab dictionary)")

    if len(tokens) > 1:
        return _json_result(
            {
                "word": word,
                "is_split": True,
                "tokens": [_token_to_dict(t) for t in tokens],
                "mecab": None,
                "suggestion": {"pos": "NOUN", "conj_type": None, "command": _dict_add_command(word, "NOUN")},
            }
        )

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
        result["suggestion"] = {"pos": pos, "conj_type": conj or None, "command": _dict_add_command(word, pos, conj)}
    else:
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
        return _json_error(_invalid_value("POS", pos, VALID_POS))
    surface_error = _validate_surface(word)
    if surface_error:
        return _json_error(surface_error, word=word)
    if conj_type and conj_type not in VALID_CONJ:
        return _json_error(_invalid_value("conj_type", conj_type, VALID_CONJ))
    if user and user not in USER_CATEGORIES:
        return _json_error(_invalid_value("user category", user, USER_CATEGORIES, "Valid categories"))

    # Cross-dictionary duplicate check (core + user)
    _, by_surface = _load_all_entries()
    duplicate_files = _files_with_pos(by_surface.get(word, []), pos)
    if duplicate_files:
        return _json_error(f"DUPLICATE: '{word}' already has POS {_canonical_pos(pos)} in {', '.join(duplicate_files)}")

    tokens = mecab_analyze(word)
    suzume_rule = get_suzume_rule(word)
    if len(tokens) > 1 and not force and pos != "PROPER_NOUN" and not suzume_rule:
        token_dicts = [_token_to_dict(t) for t in tokens]
        if len(tokens) == 2 and tokens[1]["pos"] in ("助動詞", "助詞"):
            return _json_error(
                f"REJECT: This appears to be a conjugated form. Register '{tokens[0].get('lemma', '')}' instead, or use PROPER_NOUN.",
                word=word,
                tokens=token_dicts,
            )
        return _json_error(
            f"MeCab splits '{word}' into {len(tokens)} tokens. To add anyway, use force=True.",
            word=word,
            tokens=token_dicts,
        )

    entry_line = _entry_line(word, pos, conj_type)
    target_rel = _user_file(user) if user else POS_TO_FILE.get(pos, "data/core/nouns.tsv")
    fields = {"word": word, "entry": entry_line, "file": target_rel}
    if dry_run:
        return _json_result({"status": "ok", **fields, "dry_run": True})

    target_file = PROJECT_ROOT / target_rel
    recompile_status, error = await _write_files_and_recompile(
        {target_file: _with_appended_lines(target_file, [entry_line])},
        _recompile_user_dic if user else _recompile_core_dic,
    )
    return _recompile_response(fields, recompile_status, error)


@mcp.tool()
async def dict_remove(word: str, pos: str = "", user: str = "", dry_run: bool = False) -> str:
    """Remove a word from the dictionary.

    Args:
        word: Word to remove.
        pos: POS of the entry to remove. Required when the surface has multiple POS entries.
        user: User dictionary category (empty for core dict).
        dry_run: Preview only.
    """
    if pos and pos not in VALID_POS:
        return _json_error(_invalid_value("POS", pos, VALID_POS))

    _, by_surface = _load_all_entries()
    candidates = list(by_surface.get(word, []))
    if user:
        candidates = [entry for entry in candidates if entry["file"] == _user_file(user)]
    else:
        candidates = [entry for entry in candidates if entry["file"].startswith("data/core/")]
    if pos:
        canonical = _canonical_pos(pos)
        candidates = [entry for entry in candidates if _canonical_pos(entry.get("pos", "")) == canonical]

    if not candidates:
        suffix = f" with POS {_canonical_pos(pos)}" if pos else ""
        return _json_error(f"Word not found in dictionary: {word}{suffix}")
    if len(candidates) > 1:
        entries = [f"{entry['file']}: {entry['surface']}\t{entry['pos']}" for entry in candidates]
        return _json_error(f"AMBIGUOUS: '{word}' has multiple entries; specify pos", entries=entries)

    selected = candidates[0]
    file_rel = selected["file"]
    filepath = PROJECT_ROOT / file_rel
    selected_pos = _canonical_pos(selected.get("pos", ""))

    new_lines = []
    removed = None
    for line in filepath.read_text(encoding="utf-8").splitlines():
        if not line.startswith("#") and line.strip():
            fields = line.split("\t")
            if fields[0] == word and _canonical_pos(fields[1] if len(fields) > 1 else "") == selected_pos:
                removed = line
                continue
        new_lines.append(line)

    if not removed:
        return _json_error(f"Word not found: {word}")
    fields = {"word": word, "removed_entry": removed, "file": file_rel}
    if dry_run:
        return _json_result({"status": "ok", **fields, "dry_run": True})

    recompile_status, error = await _write_files_and_recompile(
        {filepath: "\n".join(new_lines) + ("\n" if new_lines else "")},
        _recompile_user_dic if user else _recompile_core_dic,
    )
    return _recompile_response(fields, recompile_status, error)


def _select_toggle_entry(word: str, pos: str, user: str, *, disabled: bool) -> tuple[dict | None, str | None]:
    """Select exactly one active or disabled entry for a state toggle."""
    if pos and pos not in VALID_POS:
        return None, _invalid_value("POS", pos, VALID_POS)
    if user and user not in USER_CATEGORIES:
        return None, _invalid_value("user category", user, USER_CATEGORIES)

    matches = []
    for file_rel in _dict_files(user):
        filepath = PROJECT_ROOT / file_rel
        if not filepath.exists():
            continue
        lines = filepath.read_text(encoding="utf-8").splitlines()
        for index, line in enumerate(lines):
            is_disabled = line.startswith(DISABLED_PREFIX)
            if is_disabled != disabled:
                continue
            entry = line[len(DISABLED_PREFIX) :] if is_disabled else line
            if not entry.strip() or entry.startswith("#"):
                continue
            fields = entry.split("\t")
            if fields[0] != word:
                continue
            entry_pos = fields[1] if len(fields) > 1 else ""
            if pos and _canonical_pos(entry_pos) != _canonical_pos(pos):
                continue
            matches.append({"file": file_rel, "path": filepath, "lines": lines, "index": index, "pos": entry_pos})

    if not matches:
        suffix = f" with POS {_canonical_pos(pos)}" if pos else ""
        return None, f"Word not found in dictionary: {word}{suffix}"
    if len(matches) > 1:
        entries = [f"{entry['file']}: {word}\t{entry['pos']}" for entry in matches]
        return None, f"AMBIGUOUS: '{word}' has multiple entries; specify pos ({'; '.join(entries)})"
    return matches[0], None


async def _toggle_entry(word: str, pos: str, user: str, dry_run: bool, *, disable: bool) -> str:
    """Comment an entry out with the disabled marker, or restore a disabled one."""
    selected, error = _select_toggle_entry(word, pos, user, disabled=not disable)
    if error:
        return _json_error(error)

    lines = selected["lines"]
    line = lines[selected["index"]]
    fields = {"word": word, "file": selected["file"]}
    if dry_run:
        return _json_result({"status": "ok", **fields, "entry": line, "dry_run": True})

    lines[selected["index"]] = DISABLED_PREFIX + line if disable else line[len(DISABLED_PREFIX) :]
    recompile_status, error = await _write_files_and_recompile(
        {selected["path"]: "\n".join(lines) + "\n"},
        _recompile_user_dic if user else _recompile_core_dic,
    )
    return _recompile_response(fields, recompile_status, error)


@mcp.tool()
async def dict_disable(word: str, pos: str = "", user: str = "", dry_run: bool = False) -> str:
    """Disable a dictionary entry by commenting it out (keeps in file but inactive).

    Args:
        word: Word to disable.
        pos: POS of the entry to disable. Required when the surface has multiple POS entries.
        user: User dictionary category (empty for core dict).
        dry_run: Preview only.
    """
    return await _toggle_entry(word, pos, user, dry_run, disable=True)


@mcp.tool()
async def dict_enable(word: str, pos: str = "", user: str = "", dry_run: bool = False) -> str:
    """Re-enable a disabled dictionary entry.

    Args:
        word: Word to enable.
        pos: POS of the entry to enable. Required when the surface has multiple POS entries.
        user: User dictionary category (empty for core dict).
        dry_run: Preview only.
    """
    return await _toggle_entry(word, pos, user, dry_run, disable=False)


@mcp.tool()
async def dict_validate(fix: bool = False) -> str:
    """Validate dictionary for issues (conjugated forms, duplicates).

    Args:
        fix: If True, remove problematic entries. Default is report-only.
    """
    entries, by_surface = _load_dictionary()

    conjugated_forms = []
    compound_words = 0
    for entry in entries:
        tokens = mecab_analyze(entry["surface"])
        if len(tokens) > 1:
            reason = _is_conjugated_form(tokens, entry)
            if reason:
                conjugated_forms.append({"entry": entry, "lemma": tokens[0].get("lemma", ""), "reason": reason})
            else:
                compound_words += 1

    duplicates = [
        {"surface": surface, "pos": canonical_pos, "entries": pos_entries}
        for surface, entries_list in by_surface.items()
        for canonical_pos, pos_entries in _group_by_pos(entries_list).items()
        if len(pos_entries) > 1
    ]

    # Cross-dictionary duplicates (core vs user)
    _, all_by_surface = _load_all_entries()
    cross_duplicates = []
    for surface, all_entries in all_by_surface.items():
        for canonical_pos, pos_entries in _group_by_pos(all_entries).items():
            files = {entry["file"] for entry in pos_entries}
            if any(f.startswith("data/core/") for f in files) and any(f.startswith("data/user/") for f in files):
                cross_duplicates.append({"surface": surface, "pos": canonical_pos, "files": sorted(files)})

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
        "duplicates": [
            {"surface": dup["surface"], "pos": dup["pos"], "count": len(dup["entries"])} for dup in duplicates
        ],
        "cross_duplicates": cross_duplicates,
        "compound_words": compound_words,
        "fixed": False,
    }

    issues = len(conjugated_forms) + len(duplicates) + len(cross_duplicates)
    if issues > 0 and fix:
        # Remove lines (conjugated + duplicate extras)
        lines_to_remove = {(cf["entry"]["file"], cf["entry"]["line_num"]) for cf in conjugated_forms}
        lines_to_remove.update((entry["file"], entry["line_num"]) for dup in duplicates for entry in dup["entries"][1:])

        updates = {}
        for file_rel in set(f for f, _ in lines_to_remove):
            filepath = PROJECT_ROOT / file_rel
            file_lines = filepath.read_text(encoding="utf-8").splitlines()
            remove_nums = {ln for f, ln in lines_to_remove if f == file_rel}
            new_lines = [line for idx, line in enumerate(file_lines, 1) if idx not in remove_nums]
            updates[filepath] = "\n".join(new_lines) + "\n"

        recompile_status, error = await _write_files_and_recompile(updates, _recompile_core_dic)
        result["fixed"] = error is None
        result["recompile"] = recompile_status
        if error:
            result["status"] = "error"
            result["message"] = error
            result["rolled_back"] = True
        else:
            result["removed_count"] = len(lines_to_remove)

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
