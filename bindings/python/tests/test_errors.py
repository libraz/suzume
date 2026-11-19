"""Shared error conformance over bindings/conformance/errors.json.

Every case except the pos_filter group (test_options.py) must raise the decided
exception type, code, operation and byte-identical message through the public API.
"""

from __future__ import annotations

import inspect
import json
import math
from pathlib import Path
from typing import Any

import pytest

from suzume import ErrorCode, Suzume, SuzumeError

_FIXTURE = Path(__file__).resolve().parents[2] / "conformance" / "errors.json"
_CASES = [
    case
    for case in json.loads(_FIXTURE.read_text(encoding="utf-8"))["cases"]
    if case["group"] != "pos_filter" and case["only"] != "npm" and case["call"]["python"]
]


def _build_value(value: Any) -> Any:
    if not isinstance(value, dict) or "$special" not in value:
        return value
    special = value["$special"]
    if special == "NaN":
        return math.nan
    if special == "Infinity":
        return math.inf
    if special == "-Infinity":
        return -math.inf
    if special == "negzero":
        return -0.0
    if special == "lone_surrogate":
        return "\ud800"
    if special == "float":
        return float(value["repr"])
    if special == "array":
        return [1]
    if special == "object":
        return {"key": 1}
    if special == "cycle":
        cyclic: dict[str, object] = {}
        cyclic["self"] = cyclic
        return cyclic
    raise AssertionError(f"unknown $special in fixture: {special}")


def _invoke(case: dict[str, Any], live: Suzume) -> None:
    call = case["call"]["python"]
    value = _build_value(case["value"])
    option = case["option"]["python"] if case["option"] else None
    member = call["member"]
    if member == "Suzume":
        Suzume(**({option: value} if option else {})).close()
        return
    target = live
    if case["released"]:
        target = Suzume()
        target.close()
    if call["kind"] == "get":
        getattr(target, member)
    elif call["kind"] == "set":
        setattr(target, member, value)
    elif member == "generate_tags":
        target.generate_tags("東京", **({option: value} if option else {}))
    else:
        method = getattr(target, member)
        if inspect.signature(method).parameters:
            method(value)
        else:
            method()


@pytest.fixture(scope="module")
def live() -> Any:
    with Suzume() as sz:
        yield sz


def test_fixture_covers_every_non_pos_filter_group() -> None:
    groups = {case["group"] for case in _CASES}
    assert groups >= {
        "error-model",
        "released",
        "unknown-code",
        "size",
        "mode",
        "surrogate",
        "scorer",
        "boolean",
        "argument",
        "render",
    }


@pytest.mark.parametrize("case", _CASES, ids=[case["id"] for case in _CASES])
def test_conformance(case: dict[str, Any], live: Suzume) -> None:
    expected = case["expect"]["python"]
    if case["call"]["python"]["kind"] == "construct":
        error = SuzumeError(case["message"], _build_value(case["value"]))
        assert isinstance(error, Exception)
        assert error.code == expected["code"]
        assert str(error) == case["message"]
        assert error.operation is None
        return

    with pytest.raises(Exception) as caught:
        _invoke(case, live)
    error = caught.value
    assert type(error).__name__ == expected["type"], repr(error)
    if expected["type"] == "SuzumeError":
        assert isinstance(error, SuzumeError)
        assert error.code == expected["code"]
        assert error.operation == expected["operation"]
    if case["message"] is not None:
        assert str(error) == case["message"]


def test_error_code_unknown_is_255() -> None:
    assert ErrorCode.UNKNOWN == 255
    assert SuzumeError("boom").code is ErrorCode.INTERNAL


def test_unserializable_scorer_options_chain_the_cause() -> None:
    cyclic: dict[str, object] = {}
    cyclic["self"] = cyclic
    with pytest.raises(ValueError) as caught:
        Suzume(scorer_options=cyclic)
    assert caught.value.__cause__ is not None


def test_native_failures_have_no_cause(live: Suzume) -> None:
    with pytest.raises(SuzumeError) as caught:
        live.load_user_dictionary("")
    assert caught.value.__cause__ is None


def test_with_block_after_close_raises_only_on_member_use() -> None:
    sz = Suzume()
    sz.close()
    with sz, pytest.raises(SuzumeError) as caught:
        sz.analyze("東京")
    assert caught.value.operation == "analyze"


def test_loaders_return_counts_and_raise_on_failure(live: Suzume) -> None:
    assert live.load_user_dictionary("検査語\tNOUN\n") == 1
    with pytest.raises(SuzumeError) as caught:
        live.load_binary_dictionary(b"\x00\x01\x02\x03")
    assert caught.value.code is ErrorCode.DICTIONARY_LOAD_FAILED
    assert caught.value.operation == "load_binary_dictionary"


def test_size_options_accept_integral_floats(live: Suzume) -> None:
    assert live.generate_tags("東京タワー", min_length=2.0) == live.generate_tags("東京タワー")
    assert live.generate_tags("東京", max_tags=-0.0) == live.generate_tags("東京", max_tags=0)
