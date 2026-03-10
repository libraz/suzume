"""Suzume CLI interface - subprocess-based suzume-cli calls.

Normalization functions (get_expected_tokens etc.) are called via subprocess
to ensure code changes are reflected without MCP server restart.
Each subprocess invocation starts a fresh Python process, importing
the latest normalization code from disk.
"""

import asyncio
import json
import os
import shlex
import subprocess
import sys
from pathlib import Path

from ..config import PROJECT_ROOT


def _get_cli_path() -> Path:
    """Get path to suzume-cli binary."""
    return PROJECT_ROOT / "build" / "bin" / "suzume-cli"


def get_suzume_surfaces(text: str, cli_path: Path | None = None) -> list[str]:
    """Get surface tokens from Suzume CLI output (synchronous)."""
    import subprocess

    cli = cli_path or _get_cli_path()
    if not cli.exists():
        return []

    escaped = shlex.quote(text)
    result = subprocess.run(
        f"'{cli}' {escaped}",
        shell=True,
        capture_output=True,
        text=True,
    )
    surfaces = []
    for line in result.stdout.split("\n"):
        if not line or line == "EOS":
            continue
        surface = line.split("\t")[0]
        if surface:
            surfaces.append(surface)
    return surfaces


async def get_suzume_surfaces_async(text: str, cli_path: Path | None = None) -> list[str]:
    """Get surface tokens from Suzume CLI output (async)."""
    cli = cli_path or _get_cli_path()
    if not cli.exists():
        return []

    proc = await asyncio.create_subprocess_exec(
        str(cli),
        text,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
    )
    stdout, _ = await proc.communicate()
    surfaces = []
    for line in stdout.decode("utf-8").split("\n"):
        if not line or line == "EOS":
            continue
        surface = line.split("\t")[0]
        if surface:
            surfaces.append(surface)
    return surfaces


async def get_suzume_debug_info(text: str, cli_path: Path | None = None) -> dict:
    """Get debug info from Suzume CLI (SUZUME_DEBUG=2)."""
    import re

    cli = cli_path or _get_cli_path()
    if not cli.exists():
        return {}

    env = os.environ.copy()
    env["SUZUME_DEBUG"] = "2"

    proc = await asyncio.create_subprocess_exec(
        str(cli),
        text,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        env=env,
    )
    stdout, stderr = await proc.communicate()
    output = stdout.decode("utf-8") + stderr.decode("utf-8")

    info: dict = {"best_path": "", "total_cost": 0, "margin": 0, "tokens": [], "connections": [], "word_costs": []}

    m = re.search(r"\[VITERBI\] Best path \(cost=([-\d.]+)\): (.+?) \[margin=([-\d.]+)\]", output)
    if m:
        info["total_cost"] = float(m.group(1))
        info["best_path"] = m.group(2)
        info["margin"] = float(m.group(3))

    for part in info["best_path"].split(" → "):
        m2 = re.match(r'"(.+?)"\((\w+)/(\w+)\)', part)
        if m2:
            info["tokens"].append({"surface": m2.group(1), "pos": m2.group(2), "epos": m2.group(3)})

    for m3 in re.finditer(
        r'\[CONN\] "(.+?)" \((\w+)/\w+\) → "(.+?)" \((\w+)/\w+\): bigram=([-\d.]+) epos_adj=([-\d.]+) \(([^)]+)\) total=([-\d.]+)',
        output,
    ):
        info["connections"].append(
            {
                "from_surface": m3.group(1),
                "from_pos": m3.group(2),
                "to_surface": m3.group(3),
                "to_pos": m3.group(4),
                "bigram": float(m3.group(5)),
                "epos_adj": float(m3.group(6)),
                "reason": m3.group(7),
                "total": float(m3.group(8)),
            }
        )

    for m4 in re.finditer(
        r'\[WORD\] "(.+?)" \(([^)]+)\) cost=([-\d.]+) \(from edge\) \[cat=([-\d.]+) edge=([-\d.]+) epos=([^\]]+)\]',
        output,
    ):
        info["word_costs"].append(
            {
                "surface": m4.group(1),
                "source": m4.group(2),
                "cost": float(m4.group(3)),
                "cat_cost": float(m4.group(4)),
                "edge_cost": float(m4.group(5)),
                "epos": m4.group(6),
            }
        )

    return info


async def recompile_dic(glob_pattern: str, output_path: str) -> bool:
    """Recompile dictionary using suzume-cli dict compile."""
    cli = _get_cli_path()
    if not cli.exists():
        return False

    proc = await asyncio.create_subprocess_exec(
        str(cli),
        "dict",
        "compile",
        glob_pattern,
        output_path,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
    )
    _, _ = await proc.communicate()
    return proc.returncode == 0


# ---------------------------------------------------------------------------
# Normalization via subprocess (hot-reloadable)
#
# These functions call `python -m suzume_mcp normalize` in a subprocess so
# that changes to normalization code (suzume_utils.py, postprocessors.py,
# merge_rules.py, split_rules.py, pos_mapping.py, constants.py) take effect
# immediately without restarting the MCP server.
# ---------------------------------------------------------------------------

_MCP_PROJECT_DIR = str(Path(__file__).resolve().parent.parent.parent.parent)


def _run_normalize_cli(texts: list[str]) -> list[dict]:
    """Call normalization CLI via subprocess for fresh code.

    Returns list of {"tokens": [...], "source": str, "rule": str}.
    """
    cmd = [sys.executable, "-m", "suzume_mcp", "normalize", "--batch"]
    result = subprocess.run(
        cmd,
        input=json.dumps(texts, ensure_ascii=False),
        capture_output=True,
        text=True,
        cwd=_MCP_PROJECT_DIR,
    )
    if result.returncode != 0:
        raise RuntimeError(f"normalize CLI failed: {result.stderr}")
    return json.loads(result.stdout)


def get_expected_tokens_subprocess(text: str) -> tuple[list[dict], str, str]:
    """get_expected_tokens via subprocess (always uses latest code).

    Returns same (tokens, source, rule) tuple as suzume_utils.get_expected_tokens.
    """
    results = _run_normalize_cli([text])
    r = results[0]
    return r["tokens"], r["source"], r["rule"]


def get_expected_tokens_batch_subprocess(texts: list[str]) -> list[tuple[list[dict], str, str]]:
    """Batch version: process multiple texts in one subprocess call.

    Returns list of (tokens, source, rule) tuples.
    """
    results = _run_normalize_cli(texts)
    return [(r["tokens"], r["source"], r["rule"]) for r in results]


def format_expected_from_tokens(tokens: list[dict]) -> list[dict]:
    """Format tokens for JSON output (subprocess-compatible version).

    The CLI already returns formatted tokens (lemma omitted if == surface),
    so this is a pass-through. Kept for API compatibility with callers.
    """
    return tokens
