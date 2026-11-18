#!/bin/sh
# Pre-commit hook: check formatting of staged files across every language in the
# repo (C++ core, WASM binding, Python binding, MCP server), mirroring the
# check-only fan-out of `make format-check` but scoped to what is being committed.
#
# Installed into .git/hooks/pre-commit by simple-git-hooks (see the
# `simple-git-hooks` field in bindings/wasm/package.json). Git runs hooks from
# the repository top-level directory.
#
# Auto-fix everything with `make format`. Bypass this hook with
# SKIP_SIMPLE_GIT_HOOKS=1 (e.g. mid-refactor when toolchains are unavailable).

if [ "$SKIP_SIMPLE_GIT_HOOKS" = "1" ]; then
    echo "[pre-commit] SKIP_SIMPLE_GIT_HOOKS=1, skipping."
    exit 0
fi

root=$(git rev-parse --show-toplevel) || exit 1
cd "$root" || exit 1

staged=$(git diff --cached --name-only --diff-filter=ACM)
[ -z "$staged" ] && exit 0

status=0

# --- C++ core and native tooling (clang-format, scoped to staged files) ---
cpp=$(printf '%s\n' "$staged" | grep -E '^(src|include|tools|tests|examples)/.*\.(cpp|h|hpp|c)$')
if [ -n "$cpp" ]; then
    if ! command -v clang-format >/dev/null 2>&1; then
        echo "[pre-commit] clang-format is required for staged C/C++ files."
        status=1
    elif ! printf '%s\n' "$cpp" | xargs clang-format --dry-run --Werror; then
        echo "[pre-commit] C++ formatting issues (clang-format)."
        status=1
    fi
fi

# --- WASM binding (biome, whole sub-project when any of its files are staged) ---
if printf '%s\n' "$staged" | grep -qE '^bindings/wasm/(js|tests)/.*\.(ts|js)$'; then
    if ! command -v yarn >/dev/null 2>&1; then
        echo "[pre-commit] yarn is required for staged WASM binding files."
        status=1
    elif [ ! -d bindings/wasm/node_modules ]; then
        echo "[pre-commit] WASM dependencies are missing; run: cd bindings/wasm && yarn install --immutable"
        status=1
    elif ! (cd bindings/wasm && yarn lint >/dev/null 2>&1); then
        echo "[pre-commit] WASM binding lint issues (biome)."
        status=1
    fi
fi

# --- Python binding + MCP/repository scripts (ruff, whole sub-project each) ---
python_binding=$(printf '%s\n' "$staged" | grep -E '^bindings/python/.*\.py$')
repository_scripts=$(printf '%s\n' "$staged" | grep -E '^scripts/.*\.py$')
if [ -n "$python_binding$repository_scripts" ]; then
    if ! command -v uv >/dev/null 2>&1; then
        echo "[pre-commit] uv is required for staged Python files."
        status=1
    else
        if [ -n "$python_binding" ]; then
            if ! (cd bindings/python && uv run --locked ruff format --check . \
                && uv run --locked ruff check .) >/dev/null 2>&1; then
                echo "[pre-commit] Python binding formatting/lint issues (ruff)."
                status=1
            fi
        fi
        if [ -n "$repository_scripts" ]; then
            if ! (cd scripts/mcp && uv run --locked ruff format --check . ../../scripts/*.py \
                && uv run --locked ruff check . ../../scripts/*.py) >/dev/null 2>&1; then
                echo "[pre-commit] MCP server/repository script formatting or lint issues (ruff)."
                status=1
            fi
        fi
    fi
fi

if [ "$status" -ne 0 ]; then
    echo "[pre-commit] Fix with:  make format"
    echo "[pre-commit] Or bypass with:  SKIP_SIMPLE_GIT_HOOKS=1 git commit ..."
fi

exit "$status"
