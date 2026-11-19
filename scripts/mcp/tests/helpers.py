"""Shared helpers for the Suzume MCP tests."""

from __future__ import annotations

import asyncio
import importlib.util
import json
from collections.abc import Callable
from pathlib import Path
from types import ModuleType

SCRIPTS_DIR = Path(__file__).resolve().parents[2]

# Location of the tokenization corpus relative to a (fake) project root.
CORPUS_DIR = Path("tests/data/tokenization")


def run(coro):
    """Run a coroutine to completion."""
    return asyncio.run(coro)


def parse(result: str) -> dict:
    """Parse the JSON string returned by a tool function."""
    return json.loads(result)


def tok(surface: str, pos: str = "名詞", **extra) -> dict:
    """Build a token dict whose lemma defaults to its surface."""
    token = {"surface": surface, "pos": pos, "lemma": surface}
    token.update(extra)
    return token


def load_script(name: str) -> ModuleType:
    """Load scripts/<name>.py as a module by file path."""
    spec = importlib.util.spec_from_file_location(name, SCRIPTS_DIR / f"{name}.py")
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def write_suite(root: Path, name: str, cases: list[dict], *, header: dict | None = None, compact: bool = False) -> Path:
    """Write a corpus file under ``root`` and return its path.

    ``header`` holds the top-level keys written before ``cases``; by default a
    version and a description. ``compact`` writes single-line JSON without a
    trailing newline instead of the sorted, indented form.
    """
    path = root / CORPUS_DIR / f"{name}.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    if header is None:
        header = {"version": "1.0", "description": f"{name} tests"}
    payload = {**header, "cases": cases}
    if compact:
        text = json.dumps(payload, ensure_ascii=False)
    else:
        text = json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    path.write_text(text, encoding="utf-8")
    return path


def fake_native_analyze(monkeypatch, per_text: Callable[..., list[dict]]) -> None:
    """Answer native-CLI analysis with ``per_text(text, cli_path=..., skip_user_dict=...)``.

    A ``RuntimeError`` it raises becomes that text's in-place failure, as from the real child.
    """
    from suzume_mcp.core import suzume_cli

    def analyze_many(texts, cli_path, skip_user_dict):
        results = []
        for text in texts:
            try:
                results.append(per_text(text, cli_path=cli_path, skip_user_dict=skip_user_dict))
            except RuntimeError as exc:
                results.append(exc)
        return results

    monkeypatch.setattr(suzume_cli, "_analyze_many_sync", analyze_many)
