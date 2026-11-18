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


def test_pos_filter_accepts_names() -> None:
    with Suzume() as sz:
        by_names = sz.generate_tags("美味しいラーメンを食べた", pos_filter=["noun"])
        by_bitmask = sz.generate_tags("美味しいラーメンを食べた", pos_filter=1)
    assert by_names
    assert [(t.tag, t.pos) for t in by_names] == [(t.tag, t.pos) for t in by_bitmask]
    assert all(t.pos == "NOUN" for t in by_names)


def test_pos_filter_rejects_unknown_name() -> None:
    with Suzume() as sz, pytest.raises(ValueError):
        sz.generate_tags("東京", pos_filter=["bogus"])


def test_pos_filter_can_select_particles_and_auxiliaries() -> None:
    with Suzume() as sz:
        particles = sz.generate_tags(
            "りんごについて", pos_filter=["particle"], exclude_particles=False, min_length=1
        )
        auxiliaries = sz.generate_tags(
            "歩きます", pos_filter=["auxiliary"], exclude_auxiliaries=False, min_length=1
        )
    assert [(tag.tag, tag.pos) for tag in particles] == [("について", "PARTICLE")]
    assert [(tag.tag, tag.pos) for tag in auxiliaries] == [("ます", "AUX")]
