"""Project configuration constants (no circular-import risk)."""

import os
from pathlib import Path

# Resolve project root (scripts/mcp/../../ = project root)
_SERVER_DIR = Path(__file__).resolve().parent.parent.parent  # src/suzume_mcp -> mcp/
PROJECT_ROOT = _SERVER_DIR.parent.parent  # mcp/ -> scripts/ -> project root

# Allow override via environment variable
if env_root := os.environ.get("SUZUME_PROJECT_ROOT"):
    PROJECT_ROOT = Path(env_root)
