"""Tag-generation behavior of the Python binding."""

from __future__ import annotations

import pytest

from suzume import Suzume, Tag


def test_generate_tags_returns_tags() -> None:
    with Suzume() as sz:
        tags = sz.generate_tags("東京都に住んでいます")
    assert tags
    assert all(isinstance(t, Tag) for t in tags)
    assert all(t.tag for t in tags)


def test_min_length_filters_short_tags() -> None:
    with Suzume() as sz:
        tags = sz.generate_tags("東京都に住む", min_length=2)
    assert tags
    assert any(t.tag == "東京都" for t in tags)
    assert all(len(t.tag) >= 2 for t in tags)


def test_max_tags_limits_count() -> None:
    with Suzume() as sz:
        tags = sz.generate_tags("東京都に住んでいます", max_tags=1)
    assert len(tags) == 1


def test_remove_duplicates() -> None:
    with Suzume() as sz:
        tags = sz.generate_tags("東京東京東京", remove_duplicates=True)
    assert tags
    texts = [t.tag for t in tags]
    assert len(texts) == len(set(texts))


def test_pos_filter_can_select_particles_and_auxiliaries() -> None:
    with Suzume() as sz:
        particles = sz.generate_tags("りんごについて", pos_filter="PARTICLE", min_length=1)
        auxiliaries = sz.generate_tags("歩きます", pos_filter="AUX", min_length=1)
    assert [(tag.tag, tag.pos) for tag in particles] == [("について", "PARTICLE")]
    assert [(tag.tag, tag.pos) for tag in auxiliaries] == [("ます", "AUX")]


@pytest.mark.parametrize("option", ["min_length", "max_tags"])
@pytest.mark.parametrize("value", [-1, 2**64])
def test_size_options_reject_values_outside_size_t(option: str, value: int) -> None:
    with Suzume() as sz, pytest.raises(ValueError, match=option):
        sz.generate_tags("東京", **{option: value})
