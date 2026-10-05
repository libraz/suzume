"""Suzume — lightweight Japanese tokenizer.

Dictionary-independent tokenizer with part-of-speech tagging, distributed as a
thin ctypes binding over the native suzume C-ABI library.

Example:
    >>> from suzume import Suzume
    >>> with Suzume() as sz:
    ...     for m in sz.analyze("東京都に住む"):
    ...         print(m.surface, m.pos)
"""

from __future__ import annotations

import ctypes
import functools
import json
import os
import threading
from collections.abc import Callable, Iterable
from dataclasses import dataclass
from enum import Enum, IntEnum
from pathlib import Path
from types import TracebackType
from typing import Concatenate, ParamSpec, TypeVar

from ._ffi import (
    SuzumeExtendedOptions,
    SuzumeTagOptions,
    _bundled_data_dir,
    load_library,
)
from ._labels import (
    FLAG_CONJUGATABLE,
    FLAG_FORMAL_NOUN,
    FLAG_FROM_DICTIONARY,
    FLAG_LOW_INFO,
    FLAG_UNKNOWN,
    FLAG_USER_DICT,
    pos_japanese,
)

__all__ = [
    "Suzume",
    "Morpheme",
    "AnalysisResult",
    "Tag",
    "Mode",
    "SuzumeError",
    "ErrorCode",
    "version",
]

_lib = load_library()
_BUNDLED_DATA_DIR = _bundled_data_dir(Path(__file__).parent)

# Native label tables are immutable for the lifetime of the loaded library.
# Cache their UTF-8 conversion by numeric ABI code instead of crossing ctypes
# for every morpheme in an analysis result.
_POS_LABELS: dict[int, str | None] = {}
_CONJUGATION_TYPE_LABELS: dict[int, str | None] = {}
_CONJUGATION_FORM_LABELS: dict[int, str | None] = {}
_EXTENDED_POS_LABELS: dict[int, str | None] = {}

# POS names accepted by ``generate_tags(pos_filter=...)``, mapped to their
# bitmask value. Names and casing mirror the WASM binding's TagOptions.pos.
_POS_FILTER_BITS = {
    "noun": 1,
    "verb": 2,
    "adjective": 4,
    "adverb": 8,
    "particle": 16,
    "auxiliary": 32,
}


def _resolve_pos_filter(pos_filter: int | Iterable[str]) -> int:
    """Resolve a ``pos_filter`` argument to a native bitmask.

    Accepts a raw bitmask ``int`` (passed through unchanged) or an iterable of
    POS names (``"noun"``, ``"verb"``, ``"adjective"``, ``"adverb"``,
    ``"particle"``, ``"auxiliary"``) whose bits are OR-ed together.
    """
    if isinstance(pos_filter, int):
        return pos_filter
    mask = 0
    for name in pos_filter:
        try:
            mask |= _POS_FILTER_BITS[name]
        except KeyError:
            raise ValueError(
                f"unknown POS filter name: {name!r} (expected one of {sorted(_POS_FILTER_BITS)})"
            ) from None
    return mask


class ErrorCode(IntEnum):
    """Stable native error categories."""

    SUCCESS = 0
    INVALID_UTF8 = 1
    DICTIONARY_LOAD_FAILED = 2
    FILE_NOT_FOUND = 3
    PARSE = 4
    OUT_OF_MEMORY = 5
    INVALID_INPUT = 6
    INTERNAL = 7


class SuzumeError(RuntimeError):
    """Raised when a native suzume call fails."""

    def __init__(self, message: str, code: ErrorCode | int = ErrorCode.INTERNAL) -> None:
        super().__init__(message)
        self.code = ErrorCode(code)


class Mode(str, Enum):
    """Analysis mode passed to :class:`Suzume`."""

    NORMAL = "normal"
    SEARCH = "search"
    SPLIT = "split"

    @property
    def _code(self) -> int:
        return {Mode.NORMAL: 0, Mode.SEARCH: 1, Mode.SPLIT: 2}[self]


@dataclass(frozen=True, kw_only=True)
class Morpheme:
    """A single analyzed token."""

    surface: str
    pos: str
    base_form: str
    pos_ja: str
    conj_type: str | None = None
    conj_form: str | None = None
    extended_pos: str
    start: int
    end: int
    is_user_dict: bool
    is_formal_noun: bool
    is_low_info: bool
    is_unknown: bool
    is_from_dictionary: bool
    score: float


@dataclass(frozen=True)
class AnalysisResult:
    """Normalized input together with its morphemes."""

    normalized_text: str
    morphemes: list[Morpheme]


@dataclass(frozen=True)
class Tag:
    """A generated tag: the keyword text plus its part of speech."""

    tag: str
    pos: str


def _decode(value: bytes | None) -> str:
    return value.decode("utf-8") if value else ""


def _decode_sized(value: ctypes._Pointer[ctypes.c_char], size: int) -> str:
    return ctypes.string_at(value, size).decode("utf-8")


def _cached_native_label(
    cache: dict[int, str | None],
    function: Callable[[int], bytes | None],
    code: int,
    fallback: str | None,
) -> str | None:
    if code not in cache:
        cache[code] = _decode(function(code)) or fallback
    return cache[code]


def version() -> str:
    """Return the native library version string."""
    return _decode(_lib.suzume_version())


def _native_error(fallback: str) -> SuzumeError:
    return SuzumeError(
        _decode(_lib.suzume_last_error()) or fallback,
        _lib.suzume_last_error_code(),
    )


_MethodParams = ParamSpec("_MethodParams")
_MethodResult = TypeVar("_MethodResult")


def _locked(
    method: Callable[Concatenate[Suzume, _MethodParams], _MethodResult],
) -> Callable[Concatenate[Suzume, _MethodParams], _MethodResult]:
    """Serialize native-handle access for one ``Suzume`` instance."""

    @functools.wraps(method)
    def wrapper(
        self: Suzume, /, *args: _MethodParams.args, **kwargs: _MethodParams.kwargs
    ) -> _MethodResult:
        with self._lock:
            return method(self, *args, **kwargs)

    return wrapper


class Suzume:
    """A reusable analyzer instance.

    Instances hold a native handle; use as a context manager or call
    :meth:`close` when done. Calls on one instance are serialized, so it is
    safe to share an instance between threads; use separate instances for
    parallel native analysis.
    """

    def __init__(
        self,
        *,
        mode: Mode | str = Mode.NORMAL,
        preserve_vu: bool = True,
        preserve_case: bool = True,
        preserve_symbols: bool = False,
        lemmatize: bool = True,
        merge_compounds: bool = False,
        skip_user_dictionary: bool = False,
        skip_core_dictionary: bool = False,
        skip_env_config: bool = False,
        report_scorer_config: bool = False,
        scorer_options: str | dict[str, object] | None = None,
    ) -> None:
        self._lock = threading.RLock()
        mode = Mode(mode)
        opts = SuzumeExtendedOptions()
        _lib.suzume_init_extended_options(ctypes.byref(opts))
        opts.preserve_vu = int(preserve_vu)
        opts.preserve_case = int(preserve_case)
        opts.preserve_symbols = int(preserve_symbols)
        opts.mode = mode._code
        opts.lemmatize = int(lemmatize)
        opts.merge_compounds = int(merge_compounds)
        opts.skip_user_dictionary = int(skip_user_dictionary)
        opts.skip_core_dictionary = int(skip_core_dictionary)
        opts.skip_env_config = int(skip_env_config)
        opts.report_scorer_config = int(report_scorer_config)
        scorer_json = (
            scorer_options
            if isinstance(scorer_options, str)
            else json.dumps(scorer_options, ensure_ascii=False)
            if scorer_options is not None
            else None
        )
        scorer_payload = scorer_json.encode("utf-8") if scorer_json is not None else None
        opts.scorer_options_json = scorer_payload
        data_directory = os.fsencode(_BUNDLED_DATA_DIR) if _BUNDLED_DATA_DIR is not None else None
        opts.data_directory = data_directory

        handle = _lib.suzume_create_with_extended_options(ctypes.byref(opts))
        if not handle:
            raise _native_error("failed to create Suzume instance")
        self._handle: int | None = handle

    # --- lifecycle ---

    @_locked
    def close(self) -> None:
        """Release the native handle. Idempotent."""
        if getattr(self, "_handle", None) is not None:
            _lib.suzume_destroy(self._handle)
            self._handle = None

    def __del__(self) -> None:
        self.close()

    def __enter__(self) -> Suzume:
        return self

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc: BaseException | None,
        tb: TracebackType | None,
    ) -> None:
        self.close()

    def _require_handle(self) -> int:
        if self._handle is None:
            raise SuzumeError("Suzume instance has been closed")
        return self._handle

    @property
    @_locked
    def mode(self) -> Mode:
        """Current analysis mode for this handle."""
        code = int(_lib.suzume_mode(self._require_handle()))
        try:
            return Mode(("normal", "search", "split")[code])
        except IndexError as error:  # pragma: no cover - native ABI invariant
            raise _native_error("failed to read analysis mode") from error

    @mode.setter
    @_locked
    def mode(self, value: Mode | str) -> None:
        """Change analysis mode without reloading dictionaries."""
        mode = Mode(value)
        if not _lib.suzume_set_mode(self._require_handle(), mode._code):
            raise _native_error("failed to set analysis mode")

    # --- analysis ---

    @_locked
    def analyze(self, text: str) -> list[Morpheme]:
        """Analyze ``text`` into a list of :class:`Morpheme`."""
        return self.analyze_with_normalized_text(text).morphemes

    @_locked
    def analyze_with_normalized_text(self, text: str) -> AnalysisResult:
        """Analyze text and return the exact normalized text used for offsets.

        The Python surface accepts :class:`str`, not raw bytes. Python's strict
        UTF-8 encoder therefore rejects lone surrogates before the C ABI is
        called; arbitrary invalid byte sequences are only reachable through
        the length-aware native API.
        """
        handle = self._require_handle()
        try:
            payload = text.encode("utf-8")
        except UnicodeEncodeError as error:
            raise SuzumeError(str(error), ErrorCode.INVALID_UTF8) from error
        result = _lib.suzume_analyze_n(handle, payload, len(payload))
        if not result:
            raise _native_error("analysis failed")
        try:
            data = result.contents
            out: list[Morpheme] = []
            for idx in range(data.count):
                m = data.morphemes[idx]
                conjugates = bool(m.flags & FLAG_CONJUGATABLE)
                out.append(
                    Morpheme(
                        surface=_decode_sized(m.surface, m.surface_size),
                        pos=_cached_native_label(_POS_LABELS, _lib.suzume_pos_label, m.pos, "OTHER")
                        or "OTHER",
                        base_form=_decode_sized(m.base_form, m.base_form_size),
                        pos_ja=pos_japanese(m.pos),
                        conj_type=(
                            _cached_native_label(
                                _CONJUGATION_TYPE_LABELS,
                                _lib.suzume_conjugation_type_label,
                                m.conjugation_type,
                                None,
                            )
                            if conjugates
                            else None
                        ),
                        conj_form=(
                            _cached_native_label(
                                _CONJUGATION_FORM_LABELS,
                                _lib.suzume_conjugation_form_label,
                                m.conjugation_form,
                                None,
                            )
                            if conjugates
                            else None
                        ),
                        extended_pos=_cached_native_label(
                            _EXTENDED_POS_LABELS,
                            _lib.suzume_extended_pos_label,
                            m.extended_pos,
                            "UNKNOWN",
                        )
                        or "UNKNOWN",
                        start=int(m.start),
                        end=int(m.end),
                        is_user_dict=bool(m.flags & FLAG_USER_DICT),
                        is_formal_noun=bool(m.flags & FLAG_FORMAL_NOUN),
                        is_low_info=bool(m.flags & FLAG_LOW_INFO),
                        is_unknown=bool(m.flags & FLAG_UNKNOWN),
                        is_from_dictionary=bool(m.flags & FLAG_FROM_DICTIONARY),
                        score=float(m.score),
                    )
                )
            return AnalysisResult(
                normalized_text=ctypes.string_at(
                    data.normalized_text, data.normalized_text_size
                ).decode("utf-8"),
                morphemes=out,
            )
        finally:
            _lib.suzume_result_free(result)

    @_locked
    def generate_tags(
        self,
        text: str,
        *,
        pos_filter: int | Iterable[str] = 0,
        exclude_basic: bool = False,
        use_lemma: bool = True,
        min_length: int = 2,
        max_tags: int = 0,
        exclude_particles: bool = True,
        exclude_auxiliaries: bool = True,
        exclude_formal_nouns: bool = True,
        exclude_low_info: bool = True,
        remove_duplicates: bool = True,
    ) -> list[Tag]:
        """Generate keyword tags from ``text``.

        ``pos_filter`` selects which parts of speech to keep. Pass either a raw
        bitmask ``int`` (1=noun, 2=verb, 4=adjective, 8=adverb, 16=particle,
        32=auxiliary; 0=all) or an
        iterable of POS names, e.g. ``["noun", "verb"]``, whose bits are OR-ed
        together. All other flags map directly to the native tag options.
        """
        handle = self._require_handle()
        try:
            payload = text.encode("utf-8")
        except UnicodeEncodeError as error:
            raise SuzumeError(str(error), ErrorCode.INVALID_UTF8) from error
        opts = SuzumeTagOptions()
        _lib.suzume_init_tag_options(ctypes.byref(opts))
        opts.pos_filter = _resolve_pos_filter(pos_filter)
        opts.exclude_basic = int(exclude_basic)
        opts.use_lemma = int(use_lemma)
        opts.min_length = min_length
        opts.max_tags = max_tags
        opts.exclude_particles = int(exclude_particles)
        opts.exclude_auxiliaries = int(exclude_auxiliaries)
        opts.exclude_formal_nouns = int(exclude_formal_nouns)
        opts.exclude_low_info = int(exclude_low_info)
        opts.remove_duplicates = int(remove_duplicates)
        result = _lib.suzume_generate_tags_with_options_n(
            handle, payload, len(payload), ctypes.byref(opts)
        )
        if not result:
            raise _native_error("tag generation failed")
        try:
            data = result.contents
            out: list[Tag] = []
            for idx in range(data.count):
                out.append(
                    Tag(
                        tag=_decode(data.tags[idx]),
                        pos=_decode(_lib.suzume_pos_label(data.pos[idx])) or "OTHER",
                    )
                )
            return out
        finally:
            _lib.suzume_tags_free(result)

    # --- dictionaries ---

    @_locked
    def load_user_dict(self, data: str) -> int:
        """Load current TSV (or legacy CSV) and return the installed expanded-entry count."""
        handle = self._require_handle()
        payload = data.encode("utf-8")
        count = int(_lib.suzume_load_user_dict_count(handle, payload, len(payload)))
        if count == 0:
            raise _native_error("failed to load user dictionary")
        return count

    @_locked
    def load_binary_dict(self, data: bytes) -> None:
        """Load a compiled binary (.dic) dictionary from memory."""
        handle = self._require_handle()
        buf = (ctypes.c_uint8 * len(data)).from_buffer_copy(data)
        if not _lib.suzume_load_binary_dict(handle, buf, len(data)):
            raise _native_error("failed to load binary dictionary")

    @_locked
    def clear_user_dictionaries(self) -> None:
        """Remove caller-loaded dictionaries while retaining the bundled user dictionary."""
        handle = self._require_handle()
        if not _lib.suzume_clear_user_dictionaries(handle):
            raise _native_error("failed to clear user dictionaries")

    @property
    @_locked
    def dictionary_warnings(self) -> list[str]:
        """Return dictionary-loading, parsing, and scorer-configuration diagnostics."""
        handle = self._require_handle()
        count = _lib.suzume_dictionary_warning_count(handle)
        return [_decode(_lib.suzume_dictionary_warning(handle, idx)) for idx in range(count)]

    @property
    @_locked
    def has_core_dictionary(self) -> bool:
        """Whether the L2 core binary dictionary is loaded."""
        return bool(_lib.suzume_has_core_dictionary(self._require_handle()))
