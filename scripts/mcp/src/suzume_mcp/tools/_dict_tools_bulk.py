"""Bulk dictionary mutation MCP tools."""

from ..core.mecab import mecab_analyze
from ..core.suzume_utils import get_suzume_rule
from ..server import PROJECT_ROOT, mcp
from ._dict_tools_common import (
    POS_TO_FILE,
    USER_CATEGORIES,
    VALID_CONJ,
    VALID_POS,
    _canonical_pos,
    _entry_line,
    _files_with_pos,
    _invalid_value,
    _json_error,
    _json_result,
    _load_all_entries,
    _recompile_core_dic,
    _recompile_response,
    _recompile_user_dic,
    _user_file,
    _validate_surface,
    _with_appended_lines,
    _write_files_and_recompile,
)


@mcp.tool()
async def dict_bulk_add(
    words: str,
    pos: str = "NOUN",
    conj_type: str = "",
    user: str = "",
    force: bool = False,
    dry_run: bool = False,
) -> str:
    """Add multiple words to the dictionary at once.

    Args:
        words: Newline-separated list of words to add.
        pos: POS value for all words (default: NOUN).
        conj_type: Conjugation type required by VERB and ADJECTIVE entries.
        user: User dictionary category.
        force: Allow adding even if MeCab splits words.
        dry_run: Preview only.
    """
    if pos not in VALID_POS:
        return _json_error(_invalid_value("POS", pos, VALID_POS))
    if conj_type and conj_type not in VALID_CONJ:
        return _json_error(_invalid_value("conj_type", conj_type, VALID_CONJ))
    if pos in ("VERB", "ADJECTIVE") and not conj_type:
        return _json_error(f"conj_type is required for {pos} entries.")
    if user and user not in USER_CATEGORIES:
        return _json_error(_invalid_value("user category", user, USER_CATEGORIES, "Valid categories"))

    word_list = [line.strip() for line in words.split("\n") if line.strip()]
    if not word_list:
        return _json_error("No words provided.")

    # Load all entries once for duplicate checking
    _, by_surface = _load_all_entries()

    added = []
    skipped = []
    lines_to_append = []
    skip_conj_check = pos == "PROPER_NOUN"

    for word in word_list:
        surface_error = _validate_surface(word)
        if surface_error:
            skipped.append({"word": word, "reason": surface_error})
            continue

        duplicate_files = _files_with_pos(by_surface.get(word, []), pos)
        if duplicate_files:
            skipped.append(
                {
                    "word": word,
                    "reason": f"DUPLICATE: POS {_canonical_pos(pos)} already exists in {', '.join(duplicate_files)}",
                }
            )
            continue

        # MeCab split check
        if not force and not skip_conj_check:
            tokens = mecab_analyze(word)
            suzume_rule = get_suzume_rule(word)
            if len(tokens) > 1 and not suzume_rule:
                if len(tokens) == 2 and tokens[1]["pos"] in ("助動詞", "助詞"):
                    skipped.append(
                        {
                            "word": word,
                            "reason": f"REJECT: conjugated form. Register '{tokens[0].get('lemma', '')}' instead.",
                        }
                    )
                    continue
                skipped.append(
                    {"word": word, "reason": f"MeCab splits into {len(tokens)} tokens. Use force=True to override."}
                )
                continue

        entry_line = _entry_line(word, pos, conj_type)
        lines_to_append.append(entry_line)
        added.append({"word": word, "entry": entry_line})
        # Track as added so subsequent duplicates within the batch are caught
        by_surface.setdefault(word, []).append({"surface": word, "pos": pos, "file": "pending"})

    fields = {"added": added, "skipped": skipped, "total_added": len(added), "total_skipped": len(skipped)}
    if dry_run:
        return _json_result({"status": "ok", **fields, "dry_run": True})
    if not lines_to_append:
        return _json_result({"status": "ok", **fields})

    target_rel = _user_file(user) if user else POS_TO_FILE.get(pos, "data/core/nouns.tsv")
    target_file = PROJECT_ROOT / target_rel
    recompile_status, error = await _write_files_and_recompile(
        {target_file: _with_appended_lines(target_file, lines_to_append)},
        _recompile_user_dic if user else _recompile_core_dic,
    )
    if error:
        fields["total_added"] = 0
    return _recompile_response({**fields, "file": target_rel}, recompile_status, error)


@mcp.tool()
async def dict_bulk_move(
    words: str,
    from_user: str,
    to_user: str,
    dry_run: bool = True,
) -> str:
    """Move entries between user dictionary categories.

    Args:
        words: Newline-separated list of words to move. Use "*" to move all entries.
        from_user: Source user dictionary category.
        to_user: Destination user dictionary category.
        dry_run: If True (default), preview only.
    """
    if from_user not in USER_CATEGORIES:
        return _json_error(_invalid_value("source category", from_user, USER_CATEGORIES, "Valid categories"))
    if to_user not in USER_CATEGORIES:
        return _json_error(_invalid_value("destination category", to_user, USER_CATEGORIES, "Valid categories"))
    if from_user == to_user:
        return _json_error("Source and destination categories must differ.")

    source_rel = _user_file(from_user)
    dest_rel = _user_file(to_user)
    source_file = PROJECT_ROOT / source_rel
    dest_file = PROJECT_ROOT / dest_rel
    if not source_file.exists():
        return _json_error(f"Source file not found: {source_rel}")

    move_all = words.strip() == "*"
    target_words = {line.strip() for line in words.split("\n") if line.strip()}
    if not move_all and not target_words:
        return _json_error("No words provided.")

    remaining_lines = []
    matched_entries = []
    for line in source_file.read_text(encoding="utf-8").splitlines():
        if line.startswith("#") or not line.strip():
            remaining_lines.append(line)
            continue
        surface = line.split("\t")[0]
        if move_all or surface in target_words:
            matched_entries.append(line)
        else:
            remaining_lines.append(line)

    moved = [line.split("\t")[0] for line in matched_entries]
    not_found = [] if move_all else sorted(target_words - set(moved))

    fields = {"moved": moved, "not_found": not_found, "total_moved": len(moved), "from": source_rel, "to": dest_rel}
    if dry_run:
        return _json_result({"status": "ok", **fields, "dry_run": True})
    if not matched_entries:
        return _json_result({"status": "ok", **fields})

    source_content = "\n".join(remaining_lines) + ("\n" if remaining_lines else "")
    recompile_status, error = await _write_files_and_recompile(
        {source_file: source_content, dest_file: _with_appended_lines(dest_file, matched_entries)},
        _recompile_user_dic,
    )
    if error:
        fields["total_moved"] = 0
    return _recompile_response(fields, recompile_status, error)
