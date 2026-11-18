#!/bin/bash
# Install one wheel into a clean environment outside the checkout and exercise
# the public Python and console entry points.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/wheel_env.sh"
resolve_wheel_argument "$@"
SMOKE_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/suzume-wheel-smoke.XXXXXX")"
trap 'rm -rf "$SMOKE_ROOT"' EXIT

uv venv "$SMOKE_ROOT/venv"
uv pip install --python "$SMOKE_ROOT/venv/bin/python" --no-deps "$WHEEL"
mkdir "$SMOKE_ROOT/work"
cd "$SMOKE_ROOT/work"

export PYTHONNOUSERSITE=1
unset PYTHONPATH

"$SMOKE_ROOT/venv/bin/python" - <<'PY'
from importlib import metadata, resources

import suzume

package_version = metadata.version("suzume")
assert suzume.version() == package_version

package_dir = resources.files("suzume")
core_dictionary = package_dir.joinpath("core.dic")
user_dictionary = package_dir.joinpath("user.dic")
for dictionary in (core_dictionary, user_dictionary):
    assert dictionary.is_file(), f"missing bundled dictionary: {dictionary.name}"
    assert len(dictionary.read_bytes()) > 0, f"empty bundled dictionary: {dictionary.name}"

with suzume.Suzume() as analyzer:
    morphemes = analyzer.analyze("東京へ行く")
    assert morphemes
    assert any(morpheme.is_from_dictionary for morpheme in morphemes)

with suzume.Suzume() as analyzer:
    analyzer.load_binary_dict(core_dictionary.read_bytes())
    assert analyzer.analyze("りんごを食べる")
PY

PACKAGE_VERSION="$("$SMOKE_ROOT/venv/bin/python" -c 'from importlib.metadata import version; print(version("suzume"))')"
CLI_VERSION="$("$SMOKE_ROOT/venv/bin/suzume" --version)"
if [[ "$CLI_VERSION" != "suzume $PACKAGE_VERSION" ]]; then
    echo "Error: console version '$CLI_VERSION' does not match package version '$PACKAGE_VERSION'" >&2
    exit 1
fi

FIRST_TEXT_DICTIONARY="$SMOKE_ROOT/work/first.tsv"
SECOND_TEXT_DICTIONARY="$SMOKE_ROOT/work/second.tsv"
printf '青空庭園\tNOUN\n' > "$FIRST_TEXT_DICTIONARY"
printf '東京果樹園\tNOUN\n' > "$SECOND_TEXT_DICTIONARY"

BUNDLED_USER="$(
    "$SMOKE_ROOT/venv/bin/python" -c \
        'from importlib.resources import files; print(files("suzume").joinpath("user.dic"))'
)"
"$SMOKE_ROOT/venv/bin/suzume" \
    --dict "$FIRST_TEXT_DICTIONARY" \
    --dict "$BUNDLED_USER" \
    --dict "$SECOND_TEXT_DICTIONARY" \
    --format json \
    "青空庭園 コーヒー豆 東京果樹園" > "$SMOKE_ROOT/dictionary-analysis.json"

"$SMOKE_ROOT/venv/bin/suzume" analyze --format json "東京へ行く" > "$SMOKE_ROOT/analysis.json"
"$SMOKE_ROOT/venv/bin/python" \
    - "$SMOKE_ROOT/analysis.json" "$SMOKE_ROOT/dictionary-analysis.json" <<'PY'
import json
import sys
from pathlib import Path

analysis = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
assert analysis["input"] == "東京へ行く"
assert analysis["morphemes"]

dictionary_analysis = json.loads(Path(sys.argv[2]).read_text(encoding="utf-8"))
assert dictionary_analysis["input"] == "青空庭園 コーヒー豆 東京果樹園"
assert dictionary_analysis["morphemes"]

for result in (analysis, dictionary_analysis):
    assert all("surface" in morpheme and "pos" in morpheme for morpheme in result["morphemes"])

by_surface = {
    morpheme["surface"]: morpheme for morpheme in dictionary_analysis["morphemes"]
}
for expected_surface in ("青空庭園", "東京果樹園", "コーヒー豆"):
    assert expected_surface in by_surface, (expected_surface, dictionary_analysis["morphemes"])
    assert by_surface[expected_surface]["is_user_dict"] is True
PY

echo "Wheel smoke test passed: $(basename "$WHEEL")"
