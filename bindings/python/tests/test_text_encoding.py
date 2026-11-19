"""Every text argument rejects an unpaired surrogate with a ValueError naming it."""

from __future__ import annotations

import pytest

from suzume import Suzume


def test_user_dictionary_rejects_unpaired_surrogate() -> None:
    with (
        Suzume() as sz,
        pytest.raises(ValueError, match="^invalid dictionary: contains an unpaired surrogate$"),
    ):
        sz.load_user_dictionary("\ud800\tNOUN\n")


@pytest.mark.parametrize("scorer_options", ["\ud800", {"verb": {"name": "\ud800"}}, {"\udc00": 1}])
def test_scorer_options_reject_unpaired_surrogate(scorer_options: str | dict[str, object]) -> None:
    with pytest.raises(
        ValueError, match="^invalid scorer_options: contains an unpaired surrogate$"
    ):
        Suzume(scorer_options=scorer_options)
