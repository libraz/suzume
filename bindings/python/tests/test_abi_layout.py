"""Guard the ctypes struct mirrors against the native ABI.

ctypes reads raw struct memory, so a field-offset or size mismatch between the
Python declarations in ``suzume._ffi`` and the compiled C ABI is a segfault
waiting to happen, not a compile error. The library exports sizeof/offsetof
oracles precisely so this test can assert byte-for-byte agreement.
"""

from __future__ import annotations

import ctypes

import pytest

import suzume
import suzume._ffi as ffi
from suzume._ffi import (
    ABI_VERSION,
    SuzumeMorpheme,
    SuzumeOptions,
    SuzumeResult,
    SuzumeTagOptions,
    SuzumeTags,
)

_lib = suzume._lib


def _offsets(struct: type[ctypes.Structure]) -> list[int]:
    return [getattr(struct, name).offset for name, *_ in struct._fields_]


def test_loaded_library_reports_the_expected_abi_revision() -> None:
    assert _lib.suzume_abi_version() == ABI_VERSION, (
        "The loaded libsuzume was built from a different C ABI revision than the "
        "ctypes mirrors in suzume._ffi. Rebuild the library (make python-build), or "
        "if the ABI change was deliberate, update the mirrors and set "
        "suzume._ffi.ABI_VERSION to the header's SUZUME_ABI_VERSION."
    )


def test_load_refuses_a_library_of_another_abi_revision(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(ffi, "ABI_VERSION", ABI_VERSION + 1)
    with pytest.raises(OSError, match=f"revision {ABI_VERSION}, but .* requires revision"):
        ffi.load_library()


def test_load_refuses_a_library_without_an_abi_revision(monkeypatch: pytest.MonkeyPatch) -> None:
    class _LibraryWithoutRevision:
        pass

    monkeypatch.setattr(ffi.ctypes, "CDLL", lambda path: _LibraryWithoutRevision())
    with pytest.raises(OSError, match="does not export suzume_abi_version"):
        ffi.load_library("libsuzume-old.so")


def test_sizeof_matches_native() -> None:
    assert ctypes.sizeof(SuzumeMorpheme) == _lib.suzume_sizeof_morpheme()
    assert ctypes.sizeof(SuzumeResult) == _lib.suzume_sizeof_result()
    assert ctypes.sizeof(SuzumeTags) == _lib.suzume_sizeof_tags()
    assert ctypes.sizeof(SuzumeTagOptions) == _lib.suzume_sizeof_tag_options()
    assert ctypes.sizeof(SuzumeOptions) == _lib.suzume_sizeof_options()


def test_morpheme_offsets_match_native() -> None:
    for idx, offset in enumerate(_offsets(SuzumeMorpheme)):
        assert offset == _lib.suzume_offsetof_morpheme(idx), f"morpheme field {idx}"


def test_result_offsets_match_native() -> None:
    for idx, offset in enumerate(_offsets(SuzumeResult)):
        assert offset == _lib.suzume_offsetof_result(idx), f"result field {idx}"


def test_tags_offsets_match_native() -> None:
    for idx, offset in enumerate(_offsets(SuzumeTags)):
        assert offset == _lib.suzume_offsetof_tags(idx), f"tags field {idx}"


def test_tag_options_offsets_match_native() -> None:
    for idx, offset in enumerate(_offsets(SuzumeTagOptions)):
        assert offset == _lib.suzume_offsetof_tag_options(idx), f"tag_options field {idx}"


def test_options_offsets_match_native() -> None:
    for idx, offset in enumerate(_offsets(SuzumeOptions)):
        assert offset == _lib.suzume_offsetof_options(idx), f"options field {idx}"
