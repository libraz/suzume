"""Every text argument rejects an unpaired surrogate with the same error code."""

from __future__ import annotations

import pytest

from suzume import ErrorCode, Suzume, SuzumeError


def test_user_dictionary_rejects_unpaired_surrogate() -> None:
    with Suzume() as sz, pytest.raises(SuzumeError) as caught:
        sz.load_user_dict("\ud800\tNOUN\n")
    assert caught.value.code is ErrorCode.INVALID_UTF8


@pytest.mark.parametrize("scorer_options", ["\ud800", {"verb": "\ud800"}])
def test_scorer_options_reject_unpaired_surrogate(scorer_options: str | dict[str, object]) -> None:
    with pytest.raises(SuzumeError) as caught:
        Suzume(scorer_options=scorer_options)
    assert caught.value.code is ErrorCode.INVALID_UTF8
