"""Analysis behavior of the Python binding."""

from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor

import pytest

import suzume
import suzume._ffi as ffi
from suzume import ErrorCode, Mode, Morpheme, Suzume, SuzumeError
from suzume._ffi import load_library


def test_extended_pos_labels_match_the_serialized_range() -> None:
    lib = load_library()
    assert lib.suzume_extended_pos_label(32).decode() == "AUX_開始"
    assert lib.suzume_extended_pos_label(82).decode() == "AUX_文語過去キ"
    assert lib.suzume_extended_pos_label(83).decode() == "VERB_仮定縮約"
    assert lib.suzume_extended_pos_label(84).decode() == "PART_接続終止"
    assert lib.suzume_extended_pos_label(85) is None


def test_conjugation_labels_match_the_serialized_range() -> None:
    lib = load_library()
    assert lib.suzume_conjugation_type_label(0) is None
    assert lib.suzume_conjugation_type_label(17).decode() == "固有名詞・名"
    assert lib.suzume_conjugation_type_label(18) is None
    assert lib.suzume_conjugation_form_label(6).decode() == "意志形"
    assert lib.suzume_conjugation_form_label(7) is None


def test_version_is_nonempty() -> None:
    assert suzume.version()


def test_shallow_package_layout_falls_through_without_index_error(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(ffi, "__file__", "/app/suzume/_ffi.py")
    monkeypatch.delenv("SUZUME_LIB_PATH", raising=False)
    monkeypatch.setattr(ffi.ctypes.util, "find_library", lambda _name: None)
    with pytest.raises(OSError, match="shared library"):
        ffi._find_library()


def test_bundled_dictionaries_precede_external_environment(
    monkeypatch: pytest.MonkeyPatch, tmp_path: object
) -> None:
    monkeypatch.setenv("SUZUME_DATA_DIR", str(tmp_path))
    with Suzume() as sz:
        assert sz.has_core_dictionary is True
        assert not any("external dictionary directory" in item for item in sz.dictionary_warnings)


def test_analyze_returns_morphemes() -> None:
    text = "東京都に住む"
    with Suzume() as sz:
        result = sz.analyze(text)
    assert result
    assert all(isinstance(m, Morpheme) for m in result)
    assert "".join(m.surface for m in result) == text
    # Particle に should be tagged as a particle somewhere in the stream.
    assert any(m.surface == "に" and m.pos == "PARTICLE" for m in result)


@pytest.mark.parametrize(
    "text", ["ﾊﾞｽに乗る", "ＡＢＣ１２３", "か\u3099", "東京、大阪", "👨\u200d👩\u200d👧"]
)
def test_offsets_slice_normalized_text(text: str) -> None:
    with Suzume(preserve_symbols=True) as sz:
        result = sz.analyze_with_normalized_text(text)
    for morpheme in result.morphemes:
        assert 0 <= morpheme.start < morpheme.end <= len(result.normalized_text)
        assert result.normalized_text[morpheme.start : morpheme.end] == morpheme.surface
    for previous, current in zip(result.morphemes, result.morphemes[1:], strict=False):
        assert current.start == previous.end


def test_offsets_can_have_gaps_when_symbols_are_not_preserved() -> None:
    with Suzume() as sz:
        result = sz.analyze_with_normalized_text("東京、大阪")
    assert all(
        result.normalized_text[morpheme.start : morpheme.end] == morpheme.surface
        for morpheme in result.morphemes
    )
    assert any(
        current.start > previous.end
        for previous, current in zip(result.morphemes, result.morphemes[1:], strict=False)
    )


def test_conjugation_fields_are_none_for_non_conjugating_words() -> None:
    with Suzume() as sz:
        result = sz.analyze("東京都に住む")
    for m in result:
        assert m.conj_type is None or isinstance(m.conj_type, str)
        assert m.conj_form is None or isinstance(m.conj_form, str)
    # A particle such as に does not conjugate, so its fields are None (not "").
    particles = [m for m in result if m.surface == "に"]
    assert particles
    assert all(p.conj_type is None and p.conj_form is None for p in particles)
    verb = next(m for m in result if m.surface == "住む")
    assert verb.conj_type == "五段・マ行"
    assert verb.conj_form == "終止形"


def test_auxiliary_conjugation_form_is_exposed() -> None:
    with Suzume() as sz:
        result = sz.analyze("書かなかった")
    negative = next(m for m in result if m.surface == "なかっ")
    assert negative.pos == "AUX"
    assert negative.conj_type is None
    assert negative.conj_form == "終止形"


def test_empty_string_yields_no_morphemes() -> None:
    with Suzume() as sz:
        assert sz.analyze("") == []


def test_length_aware_analysis_preserves_embedded_null_and_normalized_text() -> None:
    with Suzume(preserve_symbols=True) as sz:
        result = sz.analyze_with_normalized_text("東京\0大阪")
        tags = sz.generate_tags("東京\0大阪", min_length=1)
    assert "大阪" in result.normalized_text
    assert any(m.surface == "\0" and m.base_form == "\0" for m in result.morphemes)
    assert any(m.surface == "大阪" for m in result.morphemes)
    assert any(tag.tag == "大阪" for tag in tags)


def test_invalid_unicode_reports_stable_error_code() -> None:
    with Suzume() as sz:
        try:
            sz.analyze("\ud800")
        except SuzumeError as error:
            assert error.code is ErrorCode.INVALID_UTF8
        else:  # pragma: no cover
            raise AssertionError("expected invalid UTF-8 error")


def test_extended_options_and_clear_are_public() -> None:
    with Suzume(
        skip_user_dictionary=True,
        skip_core_dictionary=True,
        scorer_options={"unary": {"noun_prior": 0.25}},
    ) as sz:
        assert sz.dictionary_warnings == []
        assert sz.has_core_dictionary is False
        assert sz.load_user_dict("検査する\tVERB\tSURU\n") > 1
        assert sz.load_user_dict("ignored-record\n検査語\tNOUN\n") == 1
        assert any("line 1" in warning for warning in sz.dictionary_warnings)
        with pytest.raises(SuzumeError) as error:
            sz.load_user_dict("ignored-record\n")
        assert error.value.code is ErrorCode.PARSE
        sz.clear_user_dictionaries()


def test_clear_user_dictionaries_retains_bundled_dictionary() -> None:
    with Suzume() as sz:
        before = sz.analyze("コーヒー豆")
        assert len(before) == 1
        assert before[0].is_user_dict is True

        assert sz.load_user_dict("検査語\tNOUN\n") == 1
        assert any(m.is_user_dict for m in sz.analyze("検査語"))
        sz.clear_user_dictionaries()

        assert not any(m.is_user_dict for m in sz.analyze("検査語"))
        after = sz.analyze("コーヒー豆")
        assert len(after) == 1
        assert after[0].is_user_dict is True


def test_invalid_scorer_json_reports_parse_error() -> None:
    try:
        Suzume(scorer_options="{")
    except SuzumeError as error:
        assert error.code is ErrorCode.PARSE
    else:  # pragma: no cover
        raise AssertionError("expected scorer parse error")


def test_program_scorer_options_override_environment(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("SUZUME_SCORER_INFL_confidence_ceiling", "0")
    with Suzume(
        skip_user_dictionary=True,
        skip_core_dictionary=True,
        scorer_options={"inflection": {"confidence_ceiling": 0.95}},
    ) as sz:
        assert sz.analyze("歩いています")[0].surface == "歩い"


def test_scorer_options_change_analysis() -> None:
    with Suzume(
        skip_user_dictionary=True,
        skip_core_dictionary=True,
        skip_env_config=True,
        scorer_options={"inflection": {"confidence_ceiling": 0}},
    ) as sz:
        assert sz.analyze("歩いています")[0].surface == "歩"


def test_environment_scorer_config_can_be_disabled(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("SUZUME_SCORER_INFL_confidence_ceiling", "0")
    with Suzume(
        skip_user_dictionary=True,
        skip_core_dictionary=True,
        skip_env_config=True,
    ) as sz:
        assert sz.analyze("歩いています")[0].surface == "歩い"


def test_active_scorer_config_is_reported_through_warnings() -> None:
    with Suzume(
        report_scorer_config=True,
        scorer_options={"unary": {"noun_prior": 0.25}},
    ) as sz:
        assert any("Scorer configuration active" in warning for warning in sz.dictionary_warnings)


def test_search_mode_constructs() -> None:
    with Suzume(mode=Mode.SEARCH) as sz:
        assert sz.analyze("東京都に住む")


def test_string_mode_alias() -> None:
    with Suzume(mode="split") as sz:
        assert sz.analyze("東京都に住む")


def test_mode_property_switches_an_existing_handle() -> None:
    with Suzume() as sz:
        normal = sz.analyze("API開発")
        assert sz.mode is Mode.NORMAL
        sz.mode = "split"
        assert sz.mode is Mode.SPLIT
        assert len(sz.analyze("API開発")) > len(normal)


def test_use_after_close_raises() -> None:
    sz = Suzume()
    sz.close()
    try:
        sz.analyze("東京")
    except SuzumeError:
        pass
    else:  # pragma: no cover
        raise AssertionError("expected SuzumeError after close")


def test_close_is_idempotent() -> None:
    sz = Suzume()
    sz.close()
    sz.close()


def test_one_instance_serializes_concurrent_native_calls() -> None:
    with Suzume() as sz, ThreadPoolExecutor(max_workers=8) as executor:
        results = list(executor.map(sz.analyze, ["東京でテストする"] * 64))

    assert all(
        "".join(morpheme.surface for morpheme in result) == "東京でテストする" for result in results
    )
