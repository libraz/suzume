"""Stale-CLI handling shared by every tool that runs the native CLI.

A tool that only shows or compares the CLI's output warns and continues: its
result gains a leading ``stale_cli`` field. A tool that persists that output, or a
verdict derived from it, refuses instead, because a stale observation written to
a store outlives the build that produced it.
"""

from __future__ import annotations

import functools
import inspect
import json
from collections.abc import Awaitable, Callable
from typing import Any

from ..core.json_utils import json_error, json_result
from ..core.suzume_cli import cli_staleness, stale_cli_refusal

ArgumentTest = Callable[[dict[str, Any]], bool]


def _always(_: dict[str, Any]) -> bool:
    return True


def _never(_: dict[str, Any]) -> bool:
    return False


def with_stale_notice(result: str, staleness: dict | None) -> str:
    """Put ``stale_cli`` first in a JSON object result; leave anything else unchanged."""
    if staleness is None:
        return result
    try:
        value = json.loads(result)
    except ValueError:
        return result
    if not isinstance(value, dict):
        return result
    return json_result({"stale_cli": staleness, **value})


def stale_cli_guard(
    *, persists: ArgumentTest = _never, uses_cli: ArgumentTest = _always
) -> Callable[[Callable[..., Awaitable[str]]], Callable[..., Awaitable[str]]]:
    """Warn on, or refuse, a call answered by a CLI older than its sources.

    Args:
        persists: Given the call's bound arguments, whether this call writes the
            CLI's output or a verdict from it. Such a call is refused on a stale CLI.
        uses_cli: Whether this call runs the CLI at all; a call that does not is
            passed through without a notice.
    """

    def decorate(tool: Callable[..., Awaitable[str]]) -> Callable[..., Awaitable[str]]:
        signature = inspect.signature(tool)

        @functools.wraps(tool)
        async def guarded(*args: Any, **kwargs: Any) -> str:
            bound = signature.bind(*args, **kwargs)
            bound.apply_defaults()
            arguments = dict(bound.arguments)
            if not uses_cli(arguments):
                return await tool(*args, **kwargs)
            staleness = cli_staleness()
            if persists(arguments):
                refusal = stale_cli_refusal(staleness)
                if refusal is not None:
                    return json_error(refusal, stale_cli=staleness)
            return with_stale_notice(await tool(*args, **kwargs), staleness)

        return guarded

    return decorate


def stale_banner() -> str:
    """Return a Markdown line flagging a stale CLI, or an empty string, for written reports."""
    staleness = cli_staleness()
    if staleness is None:
        return ""
    return (
        f"> **Stale CLI**: {staleness['message']} `{staleness['newest_source']}` ({staleness['source_modified']}) "
        f"is newer than `{staleness['cli']}` ({staleness['cli_built']}).\n\n"
    )
