"""Suzume CLI interface - subprocess-based suzume-cli calls.

Normalization functions (get_expected_tokens etc.) are called via subprocess
to ensure code changes are reflected without MCP server restart.
Each subprocess invocation starts a fresh Python process, importing
the latest normalization code from disk.
"""

import asyncio
import json
import os
import re
import subprocess
import sys
from pathlib import Path

from ..config import PROJECT_ROOT

_CLI_ENV_VAR = "SUZUME_CLI_PATH"
_MULTI_CONFIG_NAMES = ("Debug", "Release", "RelWithDebInfo", "MinSizeRel")


def get_cli_path(project_root: Path | None = None) -> Path:
    """Locate the native Suzume CLI across single- and multi-config builds.

    ``SUZUME_CLI_PATH`` is authoritative when set. Relative override paths are
    resolved from the project root so the MCP server behaves consistently
    regardless of its current working directory.
    """
    root = project_root or PROJECT_ROOT
    if override := os.environ.get(_CLI_ENV_VAR):
        override_path = Path(override).expanduser()
        return override_path if override_path.is_absolute() else root / override_path

    build_dir = root / "build"
    cli_name = "suzume-cli.exe" if os.name == "nt" else "suzume-cli"
    candidates = [build_dir / "bin" / cli_name]
    candidates.extend(build_dir / "bin" / config / cli_name for config in _MULTI_CONFIG_NAMES)
    candidates.extend(build_dir / config / "bin" / cli_name for config in _MULTI_CONFIG_NAMES)

    return next((candidate for candidate in candidates if candidate.is_file()), candidates[0])


def _existing_cli(cli_path: Path | None) -> Path:
    cli = cli_path or get_cli_path()
    if not cli.exists():
        raise RuntimeError(f"Suzume CLI not found: {cli}")
    return cli


def _analyze_args(text: str, skip_user_dict: bool) -> list[str]:
    return ["analyze"] + (["--no-user-dict"] if skip_user_dict else []) + ["--", text]


def _cli_failure(stderr: str) -> RuntimeError:
    return RuntimeError(f"Suzume CLI failed: {stderr.strip() or 'non-zero exit'}")


def _surfaces_from_output(stdout: str) -> list[str]:
    surfaces = []
    for line in stdout.split("\n"):
        if not line or line == "EOS":
            continue
        surface = line.split("\t")[0]
        if surface:
            surfaces.append(surface)
    return surfaces


async def _run_cli_async(cli: Path, args: list[str], env: dict[str, str] | None = None) -> tuple[str, str]:
    """Run the CLI without blocking the event loop; raise on a non-zero exit."""
    proc = await asyncio.create_subprocess_exec(
        str(cli),
        *args,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        env=env,
        cwd=PROJECT_ROOT,
    )
    stdout, stderr = await proc.communicate()
    if proc.returncode != 0:
        raise _cli_failure(stderr.decode("utf-8"))
    return stdout.decode("utf-8"), stderr.decode("utf-8")


def get_suzume_surfaces(text: str, cli_path: Path | None = None, skip_user_dict: bool = False) -> list[str]:
    """Get surface tokens from Suzume CLI output (synchronous).

    Args:
        skip_user_dict: When True, pass --no-user-dict to match the C++ tokenization
            test runner oracle (skip_user_dictionary=true). When False (default), the
            CLI auto-loads user.dic, matching real-world CLI behavior — this is what
            thread checking wants so that dict_add fixes are reflected.
    """
    cli = _existing_cli(cli_path)
    result = subprocess.run(
        [str(cli), *_analyze_args(text, skip_user_dict)],
        capture_output=True,
        text=True,
        cwd=PROJECT_ROOT,
    )
    if result.returncode != 0:
        raise _cli_failure(result.stderr)
    return _surfaces_from_output(result.stdout)


async def get_suzume_surfaces_async(text: str, cli_path: Path | None = None, skip_user_dict: bool = False) -> list[str]:
    """Get surface tokens from Suzume CLI output (async).

    Args:
        skip_user_dict: When True, pass --no-user-dict to match the C++ tokenization
            test runner oracle. When False (default), the CLI auto-loads user.dic.
    """
    stdout, _ = await _run_cli_async(_existing_cli(cli_path), _analyze_args(text, skip_user_dict))
    return _surfaces_from_output(stdout)


async def get_suzume_debug_info(text: str, cli_path: Path | None = None, skip_user_dict: bool = True) -> dict:
    """Get debug info from Suzume CLI (SUZUME_DEBUG=2).

    Args:
        skip_user_dict: Defaults to True so test_show debug output matches the test
            oracle (the C++ runner uses skip_user_dictionary=true). Pass False to
            inspect real-world CLI behavior with user.dic loaded.
    """
    stdout, stderr = await _run_cli_async(
        _existing_cli(cli_path), _analyze_args(text, skip_user_dict), {**os.environ, "SUZUME_DEBUG": "2"}
    )
    output = stdout + stderr

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
    cli = get_cli_path()
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
        cwd=PROJECT_ROOT,
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


def _run_normalize_cli(texts: list[str], *, raw_mecab: bool = False) -> list[dict]:
    """Call normalization CLI via subprocess for fresh code.

    Returns list of {"tokens": [...], "source": str, "rule": str}.
    """
    cmd = [sys.executable, "-m", "suzume_mcp", "normalize", "--batch"]
    if raw_mecab:
        cmd.append("--mecab")
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
    if "error" in r:
        raise RuntimeError(f"normalization failed for {text!r}: {r['error']}")
    return r["tokens"], r["source"], r["rule"]


def _batch_subprocess(texts: list[str], *, raw_mecab: bool, failure: str) -> list[tuple[list[dict], str, str]]:
    results = _run_normalize_cli(texts, raw_mecab=raw_mecab)
    return [
        ([], "error", f"{failure} for {text!r}: {result['error']}")
        if "error" in result
        else (result["tokens"], result["source"], result["rule"])
        for text, result in zip(texts, results, strict=True)
    ]


def get_expected_tokens_batch_subprocess(texts: list[str]) -> list[tuple[list[dict], str, str]]:
    """Batch version: process multiple texts in one subprocess call.

    Returns list of (tokens, source, rule) tuples. A failed item is represented
    as ([], "error", message), preserving the batch's other results.
    """
    return _batch_subprocess(texts, raw_mecab=False, failure="normalization failed")


def get_mecab_tokens_batch_subprocess(texts: list[str]) -> list[tuple[list[dict], str, str]]:
    """Return raw MeCab tokens and oracle rule attribution in one batch.

    A failed item uses the same ([], "error", message) representation as the
    normalized batch API.
    """
    return _batch_subprocess(texts, raw_mecab=True, failure="MeCab comparison failed")
