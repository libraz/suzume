"""Names and shapes removed from the public surface stay removed."""

from __future__ import annotations

import dataclasses
import importlib.metadata

import pytest

import suzume
from suzume import AnalysisResult, Morpheme, Suzume, Tag


@pytest.mark.parametrize("name", ["load_user_dict", "load_binary_dict"])
def test_short_loader_names_are_gone(name: str) -> None:
    assert not hasattr(Suzume, name)


def test_skip_user_dictionary_keyword_is_gone() -> None:
    with pytest.raises(TypeError, match="skip_user_dictionary"):
        Suzume(skip_user_dictionary=True)  # type: ignore[call-arg]


def test_use_lemma_keyword_is_gone() -> None:
    with Suzume() as sz, pytest.raises(TypeError, match="use_lemma"):
        sz.generate_tags("東京", use_lemma=False)  # type: ignore[call-arg]


def test_renamed_keywords_are_accepted() -> None:
    with Suzume(skip_bundled_user_dictionary=True) as sz:
        surface = sz.generate_tags("食べた", use_base_form=False, min_length=1)
        base = sz.generate_tags("食べた", use_base_form=True, min_length=1)
    assert [tag.tag for tag in surface] == ["食べ"]
    assert [tag.tag for tag in base] == ["食べる"]


def test_pos_filter_defaults_to_none() -> None:
    import inspect

    assert inspect.signature(Suzume.generate_tags).parameters["pos_filter"].default is None


def test_results_are_tuples() -> None:
    with Suzume() as sz:
        assert isinstance(sz.analyze("東京に行く"), tuple)
        assert isinstance(sz.analyze_with_normalized_text("東京").morphemes, tuple)
        assert isinstance(sz.generate_tags("東京に行く"), tuple)
        assert isinstance(sz.dictionary_warnings, tuple)


@pytest.mark.parametrize("cls", [Morpheme, AnalysisResult, Tag])
def test_result_dataclasses_are_frozen_slotted_and_keyword_only(cls: type) -> None:
    params = cls.__dataclass_params__  # type: ignore[attr-defined]
    assert params.frozen
    assert "__slots__" in cls.__dict__
    assert all(field.kw_only for field in dataclasses.fields(cls))


def test_tag_rejects_positional_construction() -> None:
    with pytest.raises(TypeError):
        Tag("東京", "NOUN")  # type: ignore[call-arg]


def test_dunder_version_is_the_distribution_version() -> None:
    assert suzume.__version__ == importlib.metadata.version("suzume")
