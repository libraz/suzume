"""Native suzume-cli invocation, loaded only in a fresh ``python -m suzume_mcp native`` process.

Everything that spells the native CLI's command line (subcommands, flags, the
debug environment variable) and parses its output lives here, so a CLI rename
lands in the next tool call instead of stranding long-lived MCP servers that
imported the old spelling. The server side passes only an operation and its
data; see ``suzume_cli._run_native``.

Request (JSON object on stdin), keyed by ``op``:
    analyze       {cli, texts, skip_user_dict, timeout, concurrency} -> [{"tokens"} | {"error"}]
    debug         {cli, text, skip_user_dict, timeout}               -> {"info"} | {"error"}
    dict-compile  {cli, glob, output, timeout}                       -> {"ok": true} | {"error"}
"""

import asyncio
import os
import re
from pathlib import Path

from ..config import PROJECT_ROOT
from .pos_mapping import normalize_pos


def analyze_args(text: str, skip_user_dict: bool) -> list[str]:
    return ["analyze"] + (["--skip-bundled-user-dictionary"] if skip_user_dict else []) + ["--", text]


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


async def run_cli(cli: Path, args: list[str], timeout: float, env: dict[str, str] | None = None) -> tuple[str, str]:
    """Run the CLI with an argv list (no shell); raise on a non-zero exit, or kill it and raise on a timeout."""
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
        raise RuntimeError(f"Suzume CLI timed out after {timeout:g}s") from None
    if proc.returncode != 0:
        raise RuntimeError(f"Suzume CLI failed: {stderr.decode('utf-8').strip() or 'non-zero exit'}")
    return stdout.decode("utf-8"), stderr.decode("utf-8")


async def _analyze(request: dict) -> list[dict]:
    cli = Path(request["cli"])
    semaphore = asyncio.Semaphore(request["concurrency"])

    async def one(text: str) -> dict:
        async with semaphore:
            try:
                stdout, _ = await run_cli(cli, analyze_args(text, request["skip_user_dict"]), request["timeout"])
            except RuntimeError as exc:
                return {"error": str(exc)}
            return {"tokens": tokens_from_output(stdout)}

    return list(await asyncio.gather(*(one(text) for text in request["texts"])))


def parse_debug_output(output: str) -> dict:
    """Extract the best path, connection costs and word costs from ``SUZUME_DEBUG=2`` output."""
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


async def _debug(request: dict) -> dict:
    try:
        stdout, stderr = await run_cli(
            Path(request["cli"]),
            analyze_args(request["text"], request["skip_user_dict"]),
            request["timeout"],
            {**os.environ, "SUZUME_DEBUG": "2"},
        )
    except RuntimeError as exc:
        return {"error": str(exc)}
    return {"info": parse_debug_output(stdout + stderr)}


async def _dict_compile(request: dict) -> dict:
    try:
        await run_cli(Path(request["cli"]), ["dict", "compile", request["glob"], request["output"]], request["timeout"])
    except RuntimeError as exc:
        return {"error": str(exc)}
    return {"ok": True}


OPERATIONS = {"analyze": _analyze, "debug": _debug, "dict-compile": _dict_compile}


def run(request: dict) -> object:
    """Execute one request; an unknown op raises KeyError so the process exits non-zero."""
    return asyncio.run(OPERATIONS[request["op"]](request))
