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
import math
import os
import threading
import weakref
from collections.abc import Callable, Iterable
from dataclasses import dataclass
from decimal import Decimal
from enum import Enum, IntEnum
from pathlib import Path
from types import TracebackType
from typing import Concatenate, Literal, ParamSpec, TypeVar

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
    "Operation",
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
_POS_FALLBACK = "OTHER"
_EXTENDED_POS_FALLBACK = "UNKNOWN"

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
_POS_FILTER_ALL = sum(_POS_FILTER_BITS.values())
_SIZE_OPTION_MAX = 0xFFFFFFFF
_SAFE_INTEGER_MAX = 2**53
_RELEASED_MESSAGE = "Suzume instance has been released"

Operation = Literal[
    "create",
    "analyze",
    "generate_tags",
    "load_user_dictionary",
    "load_binary_dictionary",
    "clear_user_dictionaries",
    "mode",
    "version",
    "dictionary_warnings",
    "has_core_dictionary",
    "wasm_memory_bytes",
]
"""The call that raised a :class:`SuzumeError`; shared with the other bindings."""

_JSON_ESCAPES = {
    '"': '\\"',
    "\\": "\\\\",
    "\b": "\\b",
    "\t": "\\t",
    "\n": "\\n",
    "\f": "\\f",
    "\r": "\\r",
}


def _render_string(value: str) -> str:
    """Quote ``value`` the way ECMAScript ``JSON.stringify`` does (ES2019)."""
    out = []
    for char in value:
        code = ord(char)
        if char in _JSON_ESCAPES:
            out.append(_JSON_ESCAPES[char])
        elif code < 0x20 or 0xD800 <= code <= 0xDFFF:
            out.append(f"\\u{code:04x}")
        else:
            out.append(char)
    return '"' + "".join(out) + '"'


def _render_number(value: float) -> str:
    """Format a finite non-integral float as ECMAScript ``Number::toString`` would."""
    sign = "-" if value < 0 else ""
    digits_tuple, exponent = Decimal(repr(abs(value))).normalize().as_tuple()[1:]
    digits = "".join(str(digit) for digit in digits_tuple)
    count = len(digits)
    point = count + int(exponent)
    if count <= point <= 21:
        body = digits + "0" * (point - count)
    elif 0 < point <= 21:
        body = digits[:point] + "." + digits[point:]
    elif -6 < point <= 0:
        body = "0." + "0" * (-point) + digits
    else:
        shown = point - 1
        mantissa = digits if count == 1 else digits[0] + "." + digits[1:]
        body = f"{mantissa}e{'+' if shown >= 0 else '-'}{abs(shown)}"
    return sign + body


def _render_value(value: object) -> str:
    """Render a rejected value identically to the other bindings."""
    if isinstance(value, bool):
        return "true" if value else "false"
    if value is None:
        return "null"
    if isinstance(value, str):
        return _render_string(value)
    if isinstance(value, int):
        if abs(value) <= _SAFE_INTEGER_MAX:
            return str(value)
        return _render_value(float(value))
    if isinstance(value, float):
        if math.isnan(value):
            return "NaN"
        if math.isinf(value):
            return "Infinity" if value > 0 else "-Infinity"
        if value.is_integer() and abs(value) <= _SAFE_INTEGER_MAX:
            return str(int(value))
        return _render_number(value)
    if isinstance(value, (list, tuple)):
        return "array"
    if isinstance(value, dict):
        return "object"
    return "value"


def _size_option(name: str, value: object) -> int:
    """Return ``value`` as the native 32-bit size field ``name``; integral floats are accepted."""
    if (
        isinstance(value, bool)
        or not isinstance(value, (int, float))
        or (isinstance(value, float) and not value.is_integer())
        or not 0 <= value <= _SIZE_OPTION_MAX
    ):
        raise ValueError(
            f"invalid {name}: {_render_value(value)} "
            f"(expected an integer from 0 to {_SIZE_OPTION_MAX})"
        )
    return int(value)


def _bool_option(name: str, value: object) -> int:
    if not isinstance(value, bool):
        raise ValueError(f"invalid {name}: {_render_value(value)} (expected a boolean)")
    return int(value)


def _mode_option(value: object) -> Mode:
    if isinstance(value, Mode):
        return value
    if isinstance(value, str):
        try:
            return Mode(value)
        except ValueError:
            pass
    expected = ", ".join(mode.value for mode in Mode)
    raise ValueError(f"invalid mode: {_render_value(value)} (expected one of: {expected})")


def _encode_argument(name: str, value: object) -> bytes:
    """Encode the string argument ``name`` as UTF-8, rejecting non-strings and lone surrogates."""
    if not isinstance(value, str):
        raise ValueError(f"invalid {name}: {_render_value(value)} (expected a string)")
    return _encode_text(name, value)


def _resolve_pos_filter(pos_filter: int | Iterable[str]) -> int:
    """Resolve a ``pos_filter`` argument to a native bitmask.

    Accepts a raw bitmask ``int`` within the known POS bits, a single POS name,
    or an iterable of POS names (``"noun"``, ``"verb"``, ``"adjective"``,
    ``"adverb"``, ``"particle"``, ``"auxiliary"``) whose bits are OR-ed together.
    """
    if isinstance(pos_filter, int):
        if not 0 <= pos_filter <= _POS_FILTER_ALL:
            raise ValueError(f"pos_filter bitmask must be between 0 and {_POS_FILTER_ALL}")
        return pos_filter
    if isinstance(pos_filter, str):
        pos_filter = (pos_filter,)
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
    """Stable error categories: 0-7 mirror the C ABI; ``UNKNOWN`` (255) is reserved for bindings."""

    SUCCESS = 0
    INVALID_UTF8 = 1
    DICTIONARY_LOAD_FAILED = 2
    FILE_NOT_FOUND = 3
    PARSE = 4
    OUT_OF_MEMORY = 5
    INVALID_INPUT = 6
    INTERNAL = 7
    UNKNOWN = 255


class SuzumeError(RuntimeError):
    """Raised when a native suzume call fails, or when a released instance is used.

    ``message`` is the library's own text (``"<operation> failed"`` when it
    gave none); ``code`` is an :class:`ErrorCode`, any other value becoming
    ``UNKNOWN``; ``operation`` names the call that raised it. Invalid
    arguments and options raise :class:`ValueError` instead.
    """

    def __init__(
        self,
        message: str,
        code: ErrorCode | int = ErrorCode.INTERNAL,
        *,
        operation: Operation | None = None,
    ) -> None:
        super().__init__(message)
        try:
            self.code = ErrorCode(code)
        except ValueError:
            self.code = ErrorCode.UNKNOWN
        self.operation = operation


class Mode(str, Enum):
    """Analysis mode passed to :class:`Suzume`."""

    NORMAL = "normal"
    SEARCH = "search"
    SPLIT = "split"

    @property
    def _code(self) -> int:
        return _MODE_CODES[self]


_MODE_CODES = {Mode.NORMAL: 0, Mode.SEARCH: 1, Mode.SPLIT: 2}
_MODES_BY_CODE = {code: mode for mode, code in _MODE_CODES.items()}


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


def _pos_label(code: int) -> str:
    return (
        _cached_native_label(_POS_LABELS, _lib.suzume_pos_label, code, _POS_FALLBACK)
        or _POS_FALLBACK
    )


def _extended_pos_label(code: int) -> str:
    return (
        _cached_native_label(
            _EXTENDED_POS_LABELS, _lib.suzume_extended_pos_label, code, _EXTENDED_POS_FALLBACK
        )
        or _EXTENDED_POS_FALLBACK
    )


def _conjugation_type_label(code: int) -> str | None:
    return _cached_native_label(
        _CONJUGATION_TYPE_LABELS, _lib.suzume_conjugation_type_label, code, None
    )


def _conjugation_form_label(code: int) -> str | None:
    return _cached_native_label(
        _CONJUGATION_FORM_LABELS, _lib.suzume_conjugation_form_label, code, None
    )


def _encode_text(name: str, text: str) -> bytes:
    """Encode ``text`` as UTF-8; a lone surrogate is a ValueError naming ``name``."""
    try:
        return text.encode("utf-8")
    except UnicodeEncodeError as error:
        raise ValueError(f"invalid {name}: contains an unpaired surrogate") from error


def version() -> str:
    """Return the native library version string."""
    return _decode(_lib.suzume_version())


def _native_error(operation: Operation) -> SuzumeError:
    """Error for a native call that reported failure: its own text and code.

    Falls back to ``"<operation> failed"`` when the library left no text, and
    to ``INTERNAL`` when the code reads ``SUCCESS`` (a broken contract).
    """
    code = _lib.suzume_last_error_code()
    fallback = f"{operation} failed"
    if code == ErrorCode.SUCCESS:
        return SuzumeError(fallback, ErrorCode.INTERNAL, operation=operation)
    return SuzumeError(_decode(_lib.suzume_last_error()) or fallback, code, operation=operation)


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


def _encode_scorer_options(scorer_options: object) -> bytes | None:
    if scorer_options is None:
        return None
    if isinstance(scorer_options, str):
        text = scorer_options
    else:
        try:
            text = json.dumps(scorer_options, ensure_ascii=False, allow_nan=False)
        except (TypeError, ValueError, RecursionError) as error:
            raise ValueError("invalid scorer_options: not serializable as JSON") from error
    payload = _encode_text("scorer_options", text)
    if b"\x00" in payload:
        raise ValueError("invalid scorer_options: contains a NUL character")
    return payload


def _destroy_handle(handle: int) -> None:
    _lib.suzume_destroy(handle)


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
        self._handle: int | None = None
        opts = SuzumeExtendedOptions()
        _lib.suzume_init_extended_options(ctypes.byref(opts))
        opts.mode = _mode_option(mode)._code
        opts.preserve_vu = _bool_option("preserve_vu", preserve_vu)
        opts.preserve_case = _bool_option("preserve_case", preserve_case)
        opts.preserve_symbols = _bool_option("preserve_symbols", preserve_symbols)
        opts.lemmatize = _bool_option("lemmatize", lemmatize)
        opts.merge_compounds = _bool_option("merge_compounds", merge_compounds)
        opts.skip_user_dictionary = _bool_option("skip_user_dictionary", skip_user_dictionary)
        opts.skip_core_dictionary = _bool_option("skip_core_dictionary", skip_core_dictionary)
        opts.skip_env_config = _bool_option("skip_env_config", skip_env_config)
        opts.report_scorer_config = _bool_option("report_scorer_config", report_scorer_config)
        opts.scorer_options_json = _encode_scorer_options(scorer_options)
        data_directory = os.fsencode(_BUNDLED_DATA_DIR) if _BUNDLED_DATA_DIR is not None else None
        opts.data_directory = data_directory

        handle = _lib.suzume_create_with_extended_options(ctypes.byref(opts))
        if not handle:
            raise _native_error("create")
        self._handle = handle
        self._finalizer = weakref.finalize(self, _destroy_handle, handle)

    # --- lifecycle ---

    @_locked
    def close(self) -> None:
        """Release the native handle. Idempotent; any later member use raises."""
        if self._handle is not None:
            self._handle = None
            self._finalizer()

    def __enter__(self) -> Suzume:
        return self

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc: BaseException | None,
        tb: TracebackType | None,
    ) -> None:
        self.close()

    def _require_handle(self, operation: Operation) -> int:
        """Return the live handle; a released instance raises before any argument is checked."""
        if self._handle is None:
            raise SuzumeError(_RELEASED_MESSAGE, ErrorCode.INVALID_INPUT, operation=operation)
        return self._handle

    @property
    @_locked
    def mode(self) -> Mode:
        """Current analysis mode for this handle."""
        code = _lib.suzume_mode(self._require_handle("mode"))
        try:
            return _MODES_BY_CODE[code]
        except KeyError:  # pragma: no cover - native ABI invariant
            raise _native_error("mode") from None

    @mode.setter
    @_locked
    def mode(self, value: Mode | str) -> None:
        """Change analysis mode without reloading dictionaries."""
        handle = self._require_handle("mode")
        if not _lib.suzume_set_mode(handle, _mode_option(value)._code):
            raise _native_error("mode")

    # --- analysis ---

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
        handle = self._require_handle("analyze")
        payload = _encode_argument("text", text)
        result = _lib.suzume_analyze_n(handle, payload, len(payload))
        if not result:
            raise _native_error("analyze")
        try:
            data = result.contents
            out: list[Morpheme] = []
            for idx in range(data.count):
                m = data.morphemes[idx]
                flags = m.flags
                conjugates = bool(flags & FLAG_CONJUGATABLE)
                out.append(
                    Morpheme(
                        surface=_decode_sized(m.surface, m.surface_size),
                        pos=_pos_label(m.pos),
                        base_form=_decode_sized(m.base_form, m.base_form_size),
                        pos_ja=pos_japanese(m.pos),
                        conj_type=_conjugation_type_label(m.conjugation_type)
                        if conjugates
                        else None,
                        conj_form=_conjugation_form_label(m.conjugation_form)
                        if conjugates
                        else None,
                        extended_pos=_extended_pos_label(m.extended_pos),
                        start=m.start,
                        end=m.end,
                        is_user_dict=bool(flags & FLAG_USER_DICT),
                        is_formal_noun=bool(flags & FLAG_FORMAL_NOUN),
                        is_low_info=bool(flags & FLAG_LOW_INFO),
                        is_unknown=bool(flags & FLAG_UNKNOWN),
                        is_from_dictionary=bool(flags & FLAG_FROM_DICTIONARY),
                        score=m.score,
                    )
                )
            return AnalysisResult(
                normalized_text=_decode_sized(data.normalized_text, data.normalized_text_size),
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

        ``pos_filter`` selects which parts of speech to keep. Pass a raw
        bitmask ``int`` (1=noun, 2=verb, 4=adjective, 8=adverb, 16=particle,
        32=auxiliary; 0=all), a single POS name, or an iterable of POS names,
        e.g. ``["noun", "verb"]``, whose bits are OR-ed together. An unknown
        name, an out-of-range bitmask, or a ``min_length`` or ``max_tags``
        outside the native ``size_t`` range raises ``ValueError``. All other
        flags map directly to the native tag options.
        """
        handle = self._require_handle("generate_tags")
        payload = _encode_argument("text", text)
        opts = SuzumeTagOptions()
        _lib.suzume_init_tag_options(ctypes.byref(opts))
        opts.pos_filter = _resolve_pos_filter(pos_filter)
        opts.exclude_basic = _bool_option("exclude_basic", exclude_basic)
        opts.use_lemma = _bool_option("use_lemma", use_lemma)
        opts.min_length = _size_option("min_length", min_length)
        opts.max_tags = _size_option("max_tags", max_tags)
        opts.exclude_particles = _bool_option("exclude_particles", exclude_particles)
        opts.exclude_auxiliaries = _bool_option("exclude_auxiliaries", exclude_auxiliaries)
        opts.exclude_formal_nouns = _bool_option("exclude_formal_nouns", exclude_formal_nouns)
        opts.exclude_low_info = _bool_option("exclude_low_info", exclude_low_info)
        opts.remove_duplicates = _bool_option("remove_duplicates", remove_duplicates)
        result = _lib.suzume_generate_tags_with_options_n(
            handle, payload, len(payload), ctypes.byref(opts)
        )
        if not result:
            raise _native_error("generate_tags")
        try:
            data = result.contents
            return [
                Tag(tag=_decode(data.tags[idx]), pos=_pos_label(data.pos[idx]))
                for idx in range(data.count)
            ]
        finally:
            _lib.suzume_tags_free(result)

    # --- dictionaries ---

    @_locked
    def load_user_dictionary(self, dictionary: str) -> int:
        """Load a TSV (or legacy CSV) user dictionary and return the installed entry count.

        A source that installs nothing raises :class:`SuzumeError`, so the
        returned count is always at least 1.
        """
        handle = self._require_handle("load_user_dictionary")
        payload = _encode_argument("dictionary", dictionary)
        count = int(_lib.suzume_load_user_dict_count(handle, payload, len(payload)))
        if count == 0:
            raise _native_error("load_user_dictionary")
        return count

    @_locked
    def load_binary_dictionary(self, dictionary: bytes | bytearray | memoryview) -> None:
        """Load a compiled binary (.dic) dictionary from memory."""
        handle = self._require_handle("load_binary_dictionary")
        if not isinstance(dictionary, (bytes, bytearray, memoryview)):
            raise ValueError(
                f"invalid dictionary: {_render_value(dictionary)} (expected binary data)"
            )
        data = bytes(dictionary)
        buf = (ctypes.c_uint8 * len(data)).from_buffer_copy(data)
        if not _lib.suzume_load_binary_dict(handle, buf, len(data)):
            raise _native_error("load_binary_dictionary")

    @_locked
    def clear_user_dictionaries(self) -> None:
        """Remove caller-loaded dictionaries while retaining the bundled user dictionary."""
        handle = self._require_handle("clear_user_dictionaries")
        if not _lib.suzume_clear_user_dictionaries(handle):
            raise _native_error("clear_user_dictionaries")

    @property
    @_locked
    def dictionary_warnings(self) -> list[str]:
        """Return dictionary-loading, parsing, and scorer-configuration diagnostics."""
        handle = self._require_handle("dictionary_warnings")
        count = _lib.suzume_dictionary_warning_count(handle)
        warnings = (_decode(_lib.suzume_dictionary_warning(handle, idx)) for idx in range(count))
        return [warning for warning in warnings if warning]

    @property
    @_locked
    def has_core_dictionary(self) -> bool:
        """Whether the L2 core binary dictionary is loaded."""
        return bool(_lib.suzume_has_core_dictionary(self._require_handle("has_core_dictionary")))
