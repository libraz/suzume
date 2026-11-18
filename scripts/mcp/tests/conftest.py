"""Shared fixtures for Suzume MCP tests."""

import shutil
import sys
from pathlib import Path

import pytest

REFERENCE_ANALYZER_MISSING = "reference analyzer (mecab) is not installed"
NATIVE_CLI_MISSING = "native Suzume CLI is not built"


@pytest.fixture
def project_root() -> Path:
    """Return the project root directory."""
    # tests/ -> mcp/ -> scripts/ -> project root
    return Path(__file__).resolve().parent.parent.parent.parent


@pytest.fixture
def test_data_dir(project_root: Path) -> Path:
    """Return the test data directory."""
    return project_root / "tests" / "data" / "tokenization"


@pytest.fixture
def tmp_dict_dir(tmp_path: Path) -> Path:
    """Return a temporary directory for dictionary tests."""
    return tmp_path


@pytest.fixture(autouse=True)
def current_cli(monkeypatch: pytest.MonkeyPatch) -> None:
    """Judge every tool call as answered by a current CLI.

    Whether the workstation's build lags its sources must not decide a tool test;
    the stale-CLI behavior itself is covered with an injected staleness in
    test_cli_freshness.py.
    """
    from suzume_mcp.tools import _cli_freshness

    monkeypatch.setattr(_cli_freshness, "cli_staleness", lambda: None)


@pytest.fixture(autouse=True)
def reference_analyzer(monkeypatch: pytest.MonkeyPatch) -> None:
    """Skip, with one reason, every test that reaches the reference analyzer on a host without it.

    Only a test that actually calls the analyzer is skipped, in-process or through
    the normalize subprocess; a test that stubs it, or never needs it, still runs.
    """
    if shutil.which("mecab") is not None:
        return
    from suzume_mcp.core import mecab, suzume_cli

    def missing(*_args: object, **_kwargs: object) -> tuple[dict, ...]:
        pytest.skip(REFERENCE_ANALYZER_MISSING)

    run_normalize = suzume_cli._run_normalize_cli

    def normalize_or_skip(*args: object, **kwargs: object) -> list[dict]:
        results = run_normalize(*args, **kwargs)
        if any(mecab.MECAB_MISSING_MESSAGE in str(result.get("error", "")) for result in results):
            pytest.skip(REFERENCE_ANALYZER_MISSING)
        return results

    monkeypatch.setattr(mecab, "_mecab_tokens", missing)
    monkeypatch.setattr(suzume_cli, "_run_normalize_cli", normalize_or_skip)


@pytest.fixture(autouse=True)
def no_oracle_recompute(monkeypatch: pytest.MonkeyPatch) -> None:
    """Report no expectation drift after a dictionary write instead of recomputing the real suite.

    The drift check itself is exercised end to end in test_dict_oracle_check.py,
    which restores the real computation against a throwaway project.
    """
    from suzume_mcp.tools import _dict_tools_common

    monkeypatch.setattr(
        _dict_tools_common,
        "oracle_drift",
        lambda *_args, **_kwargs: ({"entries": [], "by_rule": {}, "errors": []}, None),
    )


@pytest.fixture(autouse=True)
def native_cli(monkeypatch: pytest.MonkeyPatch) -> None:
    """Skip, with one reason, every test that reaches the native CLI on a checkout without a build.

    Every module bound to ``existing_cli`` gets a version that skips when asked for
    the default CLI; a test that passes its own path or stubs the CLI still runs.
    """
    from suzume_mcp.core import suzume_cli

    if suzume_cli.get_cli_path().exists():
        return
    resolve = suzume_cli.existing_cli

    def existing_or_skip(cli_path: Path | None = None) -> Path:
        if cli_path is None:
            pytest.skip(NATIVE_CLI_MISSING)
        return resolve(cli_path)

    for name, module in list(sys.modules.items()):
        if name.startswith("suzume_mcp") and getattr(module, "existing_cli", None) is resolve:
            monkeypatch.setattr(module, "existing_cli", existing_or_skip)
