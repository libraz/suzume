"""Shared fixtures for Suzume MCP tests."""

from pathlib import Path

import pytest


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
