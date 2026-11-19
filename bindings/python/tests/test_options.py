"""``pos_filter`` takes the output POS labels, exactly as ``Tag.pos`` prints them.

The rejection cases come from the ``pos_filter`` group of
bindings/conformance/errors.json, shared with the npm suite.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

import pytest

from suzume import Suzume

_FIXTURE = Path(__file__).resolve().parents[2] / "conformance" / "errors.json"
_CASES = [
    case
    for case in json.loads(_FIXTURE.read_text(encoding="utf-8"))["cases"]
    if case["group"] == "pos_filter" and case["only"] != "npm"
]
_TEXT = "彼らは美味しいラーメンを食べた"
_NAMES = "NOUN, VERB, ADJ, ADV, PARTICLE, AUX"
_KEEP_ALL = {"min_length": 1, "exclude_low_info": False, "exclude_formal_nouns": False}


def _build_value(value: Any) -> Any:
    if isinstance(value, dict) and value.get("$special") == "object":
        return {"key": 1}
    return value


@pytest.fixture(scope="module")
def sz() -> Any:
    with Suzume() as instance:
        yield instance


@pytest.mark.parametrize("case", _CASES, ids=[case["id"] for case in _CASES])
def test_conformance(case: dict[str, Any], sz: Suzume) -> None:
    with pytest.raises(ValueError) as caught:
        sz.generate_tags("東京", pos_filter=_build_value(case["value"]))
    assert str(caught.value) == case["message"]


def test_fixture_has_pos_filter_cases() -> None:
    assert len(_CASES) >= 10


@pytest.mark.parametrize(
    ("value", "rendered"),
    [
        ((name for name in ["NOUN"]), "value"),
        (b"NOUN", "value"),
        (["NOUN", b"VERB"], "value"),
        (2.5, "2.5"),
        (True, "true"),
    ],
)
def test_other_kinds_render_per_the_shared_table(value: object, rendered: str, sz: Suzume) -> None:
    expected = f"invalid pos_filter: {rendered} (expected one of: {_NAMES})"
    with pytest.raises(ValueError) as caught:
        sz.generate_tags("東京", pos_filter=value)
    assert str(caught.value) == expected


@pytest.mark.parametrize("name", ["NOUN", "VERB", "ADJ", "ADV", "PARTICLE", "AUX"])
def test_every_output_label_is_accepted(name: str, sz: Suzume) -> None:
    tags = sz.generate_tags(_TEXT, pos_filter=name, exclude_particles=False, **_KEEP_ALL)
    assert all(tag.pos == name or (name == "NOUN" and tag.pos == "PRON") for tag in tags)


def test_a_label_printed_by_tag_pos_filters_back_to_itself(sz: Suzume) -> None:
    verbs = [tag for tag in sz.generate_tags(_TEXT) if tag.pos == "VERB"]
    assert verbs
    assert list(sz.generate_tags(_TEXT, pos_filter=verbs[0].pos)) == verbs


def test_noun_filter_admits_pronouns(sz: Suzume) -> None:
    tags = sz.generate_tags(_TEXT, pos_filter="NOUN", **_KEEP_ALL)
    assert {tag.pos for tag in tags} == {"NOUN", "PRON"}


@pytest.mark.parametrize(
    "container",
    [["NOUN", "VERB"], ("NOUN", "VERB"), {"NOUN", "VERB"}, frozenset({"NOUN", "VERB"})],
)
def test_container_kinds_select_the_union(container: object, sz: Suzume) -> None:
    tags = sz.generate_tags(_TEXT, pos_filter=container)
    assert {tag.pos for tag in tags} == {"NOUN", "VERB"}


def test_duplicate_names_have_set_semantics(sz: Suzume) -> None:
    assert sz.generate_tags(_TEXT, pos_filter=["NOUN", "NOUN"]) == sz.generate_tags(
        _TEXT, pos_filter="NOUN"
    )


@pytest.mark.parametrize("empty", [None, [], (), set(), frozenset()])
def test_none_and_empty_containers_mean_no_filter(empty: object, sz: Suzume) -> None:
    assert sz.generate_tags(_TEXT, pos_filter=empty) == sz.generate_tags(_TEXT)


def test_naming_particle_admits_particles_under_the_default_exclusion(sz: Suzume) -> None:
    tags = sz.generate_tags("りんごについて", pos_filter="PARTICLE", min_length=1)
    assert [(tag.tag, tag.pos) for tag in tags] == [("について", "PARTICLE")]


def test_released_check_runs_before_pos_filter_validation() -> None:
    sz = Suzume()
    sz.close()
    with pytest.raises(Exception) as caught:
        sz.generate_tags("東京", pos_filter="bogus")
    assert type(caught.value).__name__ == "SuzumeError"
