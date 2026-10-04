"""Dictionary search, sorting, and cleanup MCP tools."""

import re
import subprocess

import regex

from ..core.constants import katakana_to_hiragana
from ..core.mecab import mecab_analyze
from ..core.suzume_cli import get_cli_path
from ..core.suzume_utils import get_char_types
from ..server import PROJECT_ROOT, mcp
from ._dict_tools_common import (
    DISABLED_PREFIX,
    USER_CATEGORIES,
    VALID_CONJ,
    VALID_POS,
    _apply_recompile_outcome,
    _atomic_write_text,
    _dict_files,
    _invalid_value,
    _iter_dictionary_lines,
    _json_error,
    _json_result,
    _recompile_core_dic,
    _recompile_user_dic,
    _user_file,
    _write_files_and_recompile,
)

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


def _matching_entries(pattern: str, user: str, search_all: bool = False) -> tuple[list[dict] | None, str | None]:
    """Return active entries whose surface matches *pattern*, or a validation error."""
    if user and user not in USER_CATEGORIES:
        return None, _invalid_value("user category", user, USER_CATEGORIES, "Valid")
    try:
        rx = re.compile(pattern)
    except re.error as exc:
        return None, f"Invalid regex: {exc}"
    matches = []
    for file_rel, _, line in _iter_dictionary_lines(_dict_files(user, search_all)):
        surface = line.split("\t")[0]
        if rx.search(surface):
            matches.append({"file": file_rel, "surface": surface, "entry": line})
    return matches, None


def _group_order(entries: list[dict], key: str, canonical_order) -> list[str]:
    """Known values in canonical order, then unknown values, then the empty group."""
    order = [value for value in canonical_order if any(ent[key] == value for ent in entries)]
    for ent in entries:
        if ent[key] and ent[key] not in order:
            order.append(ent[key])
    if any(not ent[key] for ent in entries):
        order.append("")
    return order


@mcp.tool()
async def dict_grep(pattern: str, user: str = "", search_all: bool = False) -> str:
    """Search dictionary entries matching a regex pattern (surface match).

    Args:
        pattern: Regex pattern to match against entry surfaces.
        user: Search specific user category dict. Empty for core dict.
        search_all: Search both core and all user dicts.
    """
    matches, error = _matching_entries(pattern, user, search_all)
    if error:
        return _json_error(error)
    return _json_result(
        {
            "pattern": pattern,
            "matches": [{"file": match["file"], "entry": match["entry"]} for match in matches],
            "total": len(matches),
        }
    )


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
            return _json_error(_invalid_value("user category", user, USER_CATEGORIES, "Valid"))
        target_rel = _user_file(user)
    elif file:
        target_rel = file
    else:
        return _json_error("Specify file (e.g. 'data/core/verbs.tsv') or user category.")

    filepath = PROJECT_ROOT / target_rel
    if not filepath.exists():
        return _json_error(f"File not found: {target_rel}")

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
    pending_comments = []
    for line in raw_lines[data_start:]:
        if not line.strip():
            continue
        if line.startswith(DISABLED_PREFIX):
            disabled_entries.append({"raw": line, "comments": pending_comments})
            pending_comments = []
            continue
        if line.startswith("#"):
            if not (line.startswith("# --- ") and line.endswith(" ---")):
                pending_comments.append(line)
            continue
        fields = line.split("\t")
        entries.append(
            {
                "surface": fields[0],
                "pos": fields[1] if len(fields) > 1 else "",
                "conj_type": fields[2] if len(fields) > 2 else "",
                "raw": line,
                "comments": pending_comments,
            }
        )
        pending_comments = []
    trailing_comments = pending_comments

    if not entries:
        return _json_error(f"No entries found in {target_rel}")

    # Get readings for sorting (あいうえお order)
    reading_cache: dict[str, str] = {}
    for entry in entries:
        surface = entry["surface"]
        if surface not in reading_cache:
            reading_cache[surface] = await _get_reading(surface)

    # Group by conj_type (verbs.tsv, adjectives.tsv), else by POS (user dicts,
    # expressions.tsv); a single POS without conj_type is one alphabetical group.
    if any(ent["conj_type"] for ent in entries):
        group_key = "conj_type"
        group_order = _group_order(entries, group_key, VALID_CONJ)
    elif len(set(ent["pos"] for ent in entries)) > 1:
        group_key = "pos"
        group_order = _group_order(entries, group_key, VALID_POS)
    else:
        group_key = None
        group_order = [None]

    # Deduplicate exact grammatical entries while preserving legitimate
    # homographs that differ by POS or conjugation type.
    seen_keys: set[tuple[str, str, str]] = set()
    duplicates_removed = []
    deduped_entries = []
    for ent in entries:
        key = (ent["surface"], ent["pos"], ent["conj_type"])
        if key in seen_keys:
            duplicates_removed.append({"surface": ent["surface"], "pos": ent["pos"], "conj_type": ent["conj_type"]})
            continue
        seen_keys.add(key)
        deduped_entries.append(ent)
    entries = deduped_entries

    groups: dict[str | None, list[dict]] = {}
    for ent in entries:
        key = ent.get(group_key, None) if group_key else None
        groups.setdefault(key, []).append(ent)

    # Build sorted output
    output_lines = list(header_lines)
    group_stats = []
    for key in group_order:
        group_entries = groups.get(key, [])
        if not group_entries:
            continue

        # Sort by reading
        group_entries.sort(key=lambda ent: reading_cache.get(ent["surface"], ent["surface"]))

        if group_key:
            labels = _CONJ_LABELS if group_key == "conj_type" else _POS_LABELS
            label = labels.get(key, key or "Other")
            output_lines.append(f"\n# --- {label} ---")
            group_stats.append({"name": label, "count": len(group_entries)})

        for ent in group_entries:
            output_lines.extend(ent["comments"])
            output_lines.append(ent["raw"])

    # Append disabled entries at the end
    if disabled_entries:
        output_lines.append("\n# --- Disabled entries ---")
        for disabled in disabled_entries:
            output_lines.extend(disabled["comments"])
            output_lines.append(disabled["raw"])
    output_lines.extend(trailing_comments)

    sorted_content = "\n".join(output_lines) + "\n"

    result: dict = {
        "file": target_rel,
        "total_entries": len(entries),
        "duplicates_removed": len(duplicates_removed),
        "duplicate_entries": duplicates_removed,
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

    recompile_status, error = await _write_files_and_recompile(
        {filepath: sorted_content},
        _recompile_user_dic if target_rel.startswith("data/user/") else _recompile_core_dic,
    )
    return _apply_recompile_outcome(result, recompile_status, error)


async def _get_reading(surface: str) -> str:
    """Get hiragana reading for a surface using MeCab, for sort ordering."""
    if regex.fullmatch(r"[\p{Hiragana}\p{Katakana}ー]+", surface):
        return katakana_to_hiragana(surface)

    tokens = mecab_analyze(surface)
    if not tokens:
        return surface
    return "".join(
        katakana_to_hiragana(tok["reading"]) if tok.get("reading", "") not in ("", "*") else tok["surface"]
        for tok in tokens
    )


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
    matches, error = _matching_entries(pattern, user)
    if error:
        return _json_error(error)
    if not matches:
        return _json_result({"pattern": pattern, "matches": [], "total": 0, "applied": False})

    result: dict = {"pattern": pattern, "matches": matches, "total": len(matches), "applied": False}
    if dry_run:
        result["dry_run"] = True
        return _json_result(result)

    # Group by file and remove
    by_file: dict[str, set[str]] = {}
    for match in matches:
        by_file.setdefault(match["file"], set()).add(match["surface"])

    updates = {}
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
        updates[filepath] = "\n".join(new_lines) + "\n"

    recompile_status, error = await _write_files_and_recompile(
        updates,
        _recompile_user_dic if user else _recompile_core_dic,
    )
    return _apply_recompile_outcome(result, recompile_status, error)


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
    filepath = (PROJECT_ROOT / input_file).resolve()
    data_dir = (PROJECT_ROOT / "data").resolve()
    if not filepath.is_relative_to(data_dir):
        return _json_error("input_file must be inside data/")
    if not filepath.exists() or not filepath.is_file():
        return _json_error(f"File not found: {input_file}")

    cli = get_cli_path()
    if not cli.exists():
        return _json_error("suzume-cli not found (build first)")

    def is_fixed_expression(surface: str) -> bool:
        return bool(regex.search(r"[\p{Han}][\p{Hiragana}][\p{Han}\p{Hiragana}\p{Katakana}]", surface))

    def is_split_by_suzume(surface: str) -> bool:
        # --no-user-dict: judge whether suzume splits the word on its OWN merits,
        # not because of the very user-dict entry we are evaluating for removal.
        result = subprocess.run(
            [str(cli), "--no-user-dict", surface],
            capture_output=True,
            text=True,
            timeout=10,
            cwd=PROJECT_ROOT,
        )
        if result.returncode != 0:
            detail = result.stderr.strip() or result.stdout.strip() or "no diagnostic output"
            raise RuntimeError(f"suzume-cli exited with {result.returncode}: {detail}")
        lines = [line for line in result.stdout.strip().split("\n") if line and line != "EOS"]
        return len(lines) > 1

    keep_lines = []
    noise_lines = []
    total = 0
    kept = 0
    details = []

    try:
        input_lines = filepath.read_text(encoding="utf-8").splitlines()
        for line in input_lines:
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
    except (OSError, subprocess.SubprocessError, RuntimeError) as exc:
        return _json_error(f"Cleanup analysis failed: {exc}")

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

    output_dir = PROJECT_ROOT / "backup" / "dict-cleanup"
    output_dir.mkdir(parents=True, exist_ok=True)
    keep_path = output_dir / f"{filepath.stem}_keep.tsv"
    noise_path = output_dir / f"{filepath.stem}_noise.tsv"

    _atomic_write_text(keep_path, "\n".join(keep_lines) + "\n")
    _atomic_write_text(noise_path, "\n".join(noise_lines) + "\n")

    result["applied"] = True
    result["keep_file"] = str(keep_path.relative_to(PROJECT_ROOT))
    result["noise_file"] = str(noise_path.relative_to(PROJECT_ROOT))
    return _json_result(result)
