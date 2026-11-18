"""Failure paths of scripts/check_version_mirror.sh, run in a throwaway repository."""

from __future__ import annotations

import shutil
import subprocess
from pathlib import Path

import pytest

SCRIPT = Path(__file__).resolve().parents[2] / "check_version_mirror.sh"

LOCK_TEMPLATE = """version = 1
revision = 3
requires-python = ">=3.10"

[[package]]
name = "mypy"
version = "2.4.1"

[[package]]
name = "suzume"
version = "{version}"
source = {{ editable = "." }}

[[package]]
name = "tomli"
version = "2.3.0"
"""


WORKFLOW_TEMPLATE = """name: {name}

env:
  UV_VERSION: "{uv}"

jobs:
  job:
    steps:
      - uses: astral-sh/setup-uv@v10
        with:
          version: ${{{{ env.UV_VERSION }}}}
"""


def _repository(
    tmp_path: Path,
    cmake: str,
    wasm: str,
    pyproject: str,
    lock: str | None,
    ci_uv: str | None = "0.10.0",
    publish_uv: str | None = "0.10.0",
) -> Path:
    root = tmp_path / "repo"
    (root / "scripts").mkdir(parents=True)
    (root / ".github" / "workflows").mkdir(parents=True)
    for name, uv in (("ci", ci_uv), ("publish", publish_uv)):
        if uv is not None:
            (root / ".github" / "workflows" / f"{name}.yml").write_text(
                WORKFLOW_TEMPLATE.format(name=name, uv=uv), encoding="utf-8"
            )
    (root / "bindings" / "wasm").mkdir(parents=True)
    (root / "bindings" / "python").mkdir(parents=True)
    shutil.copy(SCRIPT, root / "scripts" / SCRIPT.name)
    (root / "CMakeLists.txt").write_text(f"project(suzume VERSION {cmake} LANGUAGES CXX)\n", encoding="utf-8")
    (root / "bindings" / "wasm" / "package.json").write_text(f'{{\n  "version": "{wasm}"\n}}\n', encoding="utf-8")
    (root / "bindings" / "python" / "pyproject.toml").write_text(
        f'[project]\nname = "suzume"\nversion = "{pyproject}"\n', encoding="utf-8"
    )
    if lock is not None:
        (root / "bindings" / "python" / "uv.lock").write_text(LOCK_TEMPLATE.format(version=lock), encoding="utf-8")
    subprocess.run(["git", "init", "-q"], cwd=root, check=True)
    return root


def _run(root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(["bash", f"scripts/{SCRIPT.name}"], cwd=root, capture_output=True, text=True, check=False)


def test_consistent_versions_pass(tmp_path: Path) -> None:
    result = _run(_repository(tmp_path, "1.2.3", "1.2.3", "1.2.3", "1.2.3"))
    assert result.returncode == 0, result.stdout
    assert "python uv.lock: 1.2.3" in result.stdout


@pytest.mark.parametrize(
    ("wasm", "pyproject", "lock", "label"),
    [
        ("1.2.3", "1.2.3", "1.2.2", "python uv.lock"),
        ("1.2.3", "1.2.2", "1.2.3", "python pyproject.toml"),
        ("1.2.2", "1.2.3", "1.2.3", "wasm package.json"),
    ],
)
def test_a_stale_mirror_fails(tmp_path: Path, wasm: str, pyproject: str, lock: str, label: str) -> None:
    result = _run(_repository(tmp_path, "1.2.3", wasm, pyproject, lock))
    assert result.returncode == 1
    assert f"❌ {label}" in result.stdout


def test_a_lock_without_the_project_fails(tmp_path: Path) -> None:
    root = _repository(tmp_path, "1.2.3", "1.2.3", "1.2.3", "1.2.3")
    lock = root / "bindings" / "python" / "uv.lock"
    lock.write_text(lock.read_text(encoding="utf-8").replace('name = "suzume"', 'name = "other"'), encoding="utf-8")
    result = _run(root)
    assert result.returncode == 1
    assert "python uv.lock (bindings/python/uv.lock): missing != 1.2.3" in result.stdout


def test_a_missing_lock_fails(tmp_path: Path) -> None:
    result = _run(_repository(tmp_path, "1.2.3", "1.2.3", "1.2.3", None))
    assert result.returncode == 1
    assert "python uv.lock (bindings/python/uv.lock): missing != 1.2.3" in result.stdout


def test_matching_uv_versions_pass_whatever_the_value(tmp_path: Path) -> None:
    result = _run(_repository(tmp_path, "1.2.3", "1.2.3", "1.2.3", "1.2.3", ci_uv="9.8.7", publish_uv="9.8.7"))
    assert result.returncode == 0, result.stdout
    assert "publish.yml UV_VERSION: 9.8.7" in result.stdout


def test_a_publish_uv_version_that_differs_from_ci_fails(tmp_path: Path) -> None:
    result = _run(_repository(tmp_path, "1.2.3", "1.2.3", "1.2.3", "1.2.3", ci_uv="0.11.0", publish_uv="0.10.0"))
    assert result.returncode == 1
    assert "❌ publish.yml UV_VERSION (.github/workflows/publish.yml): 0.10.0 != 0.11.0" in result.stdout


def test_a_publish_workflow_without_uv_version_fails(tmp_path: Path) -> None:
    result = _run(_repository(tmp_path, "1.2.3", "1.2.3", "1.2.3", "1.2.3", publish_uv=None))
    assert result.returncode == 1
    assert "❌ publish.yml UV_VERSION (.github/workflows/publish.yml): missing != 0.10.0" in result.stdout


def test_a_ci_workflow_without_uv_version_fails(tmp_path: Path) -> None:
    result = _run(_repository(tmp_path, "1.2.3", "1.2.3", "1.2.3", "1.2.3", ci_uv=None))
    assert result.returncode == 1
    assert "❌ ci.yml UV_VERSION (.github/workflows/ci.yml): missing" in result.stdout
