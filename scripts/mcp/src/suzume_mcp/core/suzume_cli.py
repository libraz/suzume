"""Suzume CLI interface - subprocess-based suzume-cli calls.

Normalization functions (get_expected_tokens etc.) are called via subprocess
to ensure code changes are reflected without MCP server restart.
Each subprocess invocation starts a fresh Python process, importing
the latest normalization code from disk.
"""

import asyncio
import json
import math
import os
import re
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime
from pathlib import Path

from ..config import PROJECT_ROOT
from .pos_mapping import normalize_pos

_CLI_ENV_VAR = "SUZUME_CLI_PATH"
_MULTI_CONFIG_NAMES = ("Debug", "Release", "RelWithDebInfo", "MinSizeRel")

# A hung child would otherwise block the tool call that spawned it forever.
_ANALYZE_TIMEOUT_SECONDS = 30.0
# CLI processes kept in flight when one call analyzes many texts.
_ANALYZE_CONCURRENCY = 8
_DICT_COMPILE_TIMEOUT_SECONDS = 120.0
_NORMALIZE_TIMEOUT_SECONDS = 60.0
_NORMALIZE_TIMEOUT_PER_TEXT_SECONDS = 1.0
# A full test-suite batch spends minutes in per-text reference-analyzer calls.
# Four processes is the machine's per-job cap for CPU-heavy work; a batch too
# small to amortize the interpreter start-up stays in one process.
_NORMALIZE_WORKERS = 4
_NORMALIZE_MIN_TEXTS_PER_WORKER = 200

# Directories whose files are compiled into the CLI. A binary older than any of
# them answers for code that is no longer in the tree.
_CLI_SOURCE_DIRS = ("src", "include", "tools/cli")
_ALLOW_STALE_ENV_VAR = "SUZUME_ALLOW_STALE_CLI"
_SOURCE_SCAN_TTL_SECONDS = 2.0
_source_scan: dict[Path, tuple[float, tuple[Path, int] | None]] = {}


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


def _newest_source(root: Path) -> tuple[Path, int] | None:
    """Return the most recently modified CLI source file and its mtime."""
    cached = _source_scan.get(root)
    now = time.monotonic()
    if cached is not None and now - cached[0] < _SOURCE_SCAN_TTL_SECONDS:
        return cached[1]
    newest: tuple[Path, int] | None = None
    for directory in _CLI_SOURCE_DIRS:
        for path in (root / directory).rglob("*"):
            if not path.is_file():
                continue
            modified = path.stat().st_mtime_ns
            if newest is None or modified > newest[1]:
                newest = (path, modified)
    _source_scan[root] = (now, newest)
    return newest


def cli_staleness(cli_path: Path | None = None, project_root: Path | None = None) -> dict | None:
    """Describe how the CLI lags its sources, or return None when it is current or absent.

    A stale binary answers for code that is no longer in the tree. Callers that only
    show its output pass this along; callers that persist it refuse (see
    ``stale_cli_refusal``).
    """
    root = project_root or PROJECT_ROOT
    cli = cli_path or get_cli_path(root)
    if not cli.exists():
        return None
    newest = _newest_source(root)
    built = cli.stat().st_mtime_ns
    if newest is None or newest[1] <= built:
        return None
    return {
        "message": "Suzume CLI is older than its sources; this output reflects the last build, not the working tree.",
        "newest_source": str(newest[0].relative_to(root)),
        "source_modified": _local_time(newest[1]),
        "cli": str(cli.relative_to(root)) if cli.is_relative_to(root) else str(cli),
        "cli_built": _local_time(built),
    }


def stale_cli_refusal(staleness: dict | None) -> str | None:
    """Return why output from this CLI must not be persisted, or None when it may be."""
    if staleness is None or os.environ.get(_ALLOW_STALE_ENV_VAR):
        return None
    return (
        f"Refusing to persist output from a stale Suzume CLI: {staleness['newest_source']} changed after "
        f"{staleness['cli']} was built. Rebuild, or set {_ALLOW_STALE_ENV_VAR}=1 to write it anyway."
    )


def _local_time(mtime_ns: int) -> str:
    return datetime.fromtimestamp(mtime_ns / 1e9).isoformat(timespec="seconds")


def existing_cli(cli_path: Path | None = None) -> Path:
    """Return the CLI to analyze with, raising when it has not been built."""
    cli = cli_path or get_cli_path()
    if not cli.exists():
        raise RuntimeError(f"Suzume CLI not found: {cli}")
    return cli


def _timed_out(what: str, timeout: float) -> RuntimeError:
    return RuntimeError(f"{what} timed out after {timeout:g}s")


def _analyze_args(text: str, skip_user_dict: bool) -> list[str]:
    return ["analyze"] + (["--no-user-dict"] if skip_user_dict else []) + ["--", text]


def _cli_failure(stderr: str) -> RuntimeError:
    return RuntimeError(f"Suzume CLI failed: {stderr.strip() or 'non-zero exit'}")


def tokens_from_output(stdout: str) -> list[dict]:
    """Parse ``analyze`` output into tokens with a normalized POS; a missing lemma is the surface."""
    tokens = []
    for line in stdout.split("\n"):
        if not line or line == "EOS":
            continue
        parts = line.split("\t")
        surface = parts[0]
        if not surface:
            continue
        pos = normalize_pos(parts[1]) if len(parts) > 1 else "Other"
        tokens.append({"surface": surface, "pos": pos, "lemma": parts[2] if len(parts) > 2 else surface})
    return tokens


def _surfaces_from_output(stdout: str) -> list[str]:
    return [token["surface"] for token in tokens_from_output(stdout)]


async def _run_cli_async(
    cli: Path, args: list[str], env: dict[str, str] | None = None, timeout: float = _ANALYZE_TIMEOUT_SECONDS
) -> tuple[str, str]:
    """Run the CLI without blocking the event loop; raise on a non-zero exit or a timeout."""
    proc = await asyncio.create_subprocess_exec(
        str(cli),
        *args,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        env=env,
        cwd=PROJECT_ROOT,
    )
    try:
        stdout, stderr = await asyncio.wait_for(proc.communicate(), timeout)
    except TimeoutError:
        proc.kill()
        await proc.wait()
        raise _timed_out("Suzume CLI", timeout) from None
    if proc.returncode != 0:
        raise _cli_failure(stderr.decode("utf-8"))
    return stdout.decode("utf-8"), stderr.decode("utf-8")


def get_suzume_surfaces(text: str, cli_path: Path | None = None, skip_user_dict: bool = False) -> list[str]:
    """Get surface tokens from Suzume CLI output (synchronous).

    Args:
        skip_user_dict: When True, pass --no-user-dict to match the C++ tokenization
            test runner oracle (skip_bundled_user_dictionary=true). When False (default), the
            CLI auto-loads user.dic, matching real-world CLI behavior — this is what
            thread checking wants so that dict_add fixes are reflected.
    """
    cli = existing_cli(cli_path)
    try:
        result = subprocess.run(
            [str(cli), *_analyze_args(text, skip_user_dict)],
            capture_output=True,
            text=True,
            cwd=PROJECT_ROOT,
            timeout=_ANALYZE_TIMEOUT_SECONDS,
        )
    except subprocess.TimeoutExpired:
        raise _timed_out("Suzume CLI", _ANALYZE_TIMEOUT_SECONDS) from None
    if result.returncode != 0:
        raise _cli_failure(result.stderr)
    return _surfaces_from_output(result.stdout)


async def get_suzume_tokens_async(text: str, cli_path: Path | None = None, skip_user_dict: bool = False) -> list[dict]:
    """Get tokens with POS and lemma from Suzume CLI output (async).

    Args:
        skip_user_dict: When True, pass --no-user-dict to match the C++ tokenization
            test runner oracle. When False (default), the CLI auto-loads user.dic.
    """
    stdout, _ = await _run_cli_async(existing_cli(cli_path), _analyze_args(text, skip_user_dict))
    return tokens_from_output(stdout)


async def get_suzume_tokens_many(texts: list[str], *, skip_user_dict: bool) -> list[list[dict] | BaseException]:
    """Analyze many texts with bounded concurrency; a failed text yields its exception in place."""
    semaphore = asyncio.Semaphore(_ANALYZE_CONCURRENCY)

    async def one(text: str) -> list[dict]:
        async with semaphore:
            return await get_suzume_tokens_async(text, skip_user_dict=skip_user_dict)

    return await asyncio.gather(*(one(text) for text in texts), return_exceptions=True)


async def get_suzume_debug_info(text: str, cli_path: Path | None = None, skip_user_dict: bool = True) -> dict:
    """Get debug info from Suzume CLI (SUZUME_DEBUG=2).

    Args:
        skip_user_dict: Defaults to True so test_show debug output matches the test
            oracle (the C++ runner uses skip_bundled_user_dictionary=true). Pass False to
            inspect real-world CLI behavior with user.dic loaded.
    """
    stdout, stderr = await _run_cli_async(
        existing_cli(cli_path), _analyze_args(text, skip_user_dict), {**os.environ, "SUZUME_DEBUG": "2"}
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

    try:
        await _run_cli_async(cli, ["dict", "compile", glob_pattern, output_path], timeout=_DICT_COMPILE_TIMEOUT_SECONDS)
    except RuntimeError:
        return False
    return True


# ---------------------------------------------------------------------------
# Normalization via subprocess (hot-reloadable)
#
# These functions call `python -m suzume_mcp normalize` in a subprocess so
# that changes to normalization code (suzume_utils.py, postprocessors.py,
# merge_rules.py, split_rules.py, pos_mapping.py, constants.py) take effect
# immediately without restarting the MCP server.
# ---------------------------------------------------------------------------

_MCP_PROJECT_DIR = str(Path(__file__).resolve().parent.parent.parent.parent)


def _run_module_cli(args: list[str], payload: list) -> list:
    """Run ``python -m suzume_mcp <args>`` with a JSON array on stdin, in a fresh process."""
    command = " ".join(args[:1])
    timeout = _NORMALIZE_TIMEOUT_SECONDS + _NORMALIZE_TIMEOUT_PER_TEXT_SECONDS * len(payload)
    try:
        result = subprocess.run(
            [sys.executable, "-m", "suzume_mcp", *args],
            input=json.dumps(payload, ensure_ascii=False),
            capture_output=True,
            text=True,
            cwd=_MCP_PROJECT_DIR,
            timeout=timeout,
        )
    except subprocess.TimeoutExpired:
        raise _timed_out(f"{command} CLI", timeout) from None
    if result.returncode != 0:
        raise RuntimeError(f"{command} CLI failed: {result.stderr}")
    return json.loads(result.stdout)


def _run_normalize_cli(texts: list[str], *, raw_mecab: bool = False) -> list[dict]:
    """Call normalization CLI via subprocess for fresh code.

    Each text is normalized independently, so a large batch is split into
    contiguous chunks run in parallel processes and concatenated in order.

    Returns list of {"tokens": [...], "source": str, "rule": str}.
    """
    args = ["normalize", "--batch", *(["--mecab"] if raw_mecab else [])]
    workers = min(_NORMALIZE_WORKERS, max(1, len(texts) // _NORMALIZE_MIN_TEXTS_PER_WORKER))
    if workers == 1:
        return _run_module_cli(args, texts)
    size = math.ceil(len(texts) / workers)
    chunks = [texts[start : start + size] for start in range(0, len(texts), size)]
    with ThreadPoolExecutor(max_workers=len(chunks)) as pool:
        parts = list(pool.map(lambda chunk: _run_module_cli(args, chunk), chunks))
    return [result for part in parts for result in part]


def get_suzume_rules_subprocess(texts: list[str]) -> list[str]:
    """Name the normalization rule each text falls under ("" when none), from the latest code."""
    return _run_module_cli(["rule"], texts)


def map_mecab_pos_subprocess(tokens: list[dict]) -> list[str]:
    """Map raw MeCab tokens to Suzume POS names with the latest mapping code."""
    return _run_module_cli(["map-pos"], tokens)


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
