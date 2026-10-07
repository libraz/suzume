"""Low-level ctypes wrapper for the suzume shared C-ABI library.

This module owns library discovery, the ``ctypes.Structure`` mirrors of the
public C ABI (``include/suzume/suzume_c.h``), and the typed function
signatures. Nothing here is part of the public Python API; consumers use
:mod:`suzume`.
"""

from __future__ import annotations

import ctypes
import ctypes.util
import os
import platform
from pathlib import Path
from typing import Any

# --- ABI revision --------------------------------------------------------------
#
# The revision of include/suzume/suzume_c.h that the mirrors below were written
# against. ctypes loads whatever libsuzume it discovers, so a library built from
# a different layout would be read at the wrong offsets rather than failing to
# link. tests/test_abi_layout.py asserts the loaded library reports this value.

ABI_VERSION = 1

# --- ctypes structure mirrors of include/suzume/suzume_c.h ---------------------
#
# Field order and types MUST match the C ABI exactly. tests/test_abi_layout.py
# asserts every sizeof/offsetof against the suzume_sizeof_*/suzume_offsetof_*
# helpers exported by the library, so silent drift here is a caught error rather
# than a segfault.


class SuzumeMorpheme(ctypes.Structure):
    _fields_ = [
        ("surface", ctypes.POINTER(ctypes.c_char)),
        ("base_form", ctypes.POINTER(ctypes.c_char)),
        ("start", ctypes.c_uint32),
        ("end", ctypes.c_uint32),
        ("score", ctypes.c_float),
        ("pos", ctypes.c_uint8),
        ("extended_pos", ctypes.c_uint8),
        ("conjugation_type", ctypes.c_uint8),
        ("conjugation_form", ctypes.c_uint8),
        ("flags", ctypes.c_uint8),
        ("surface_size", ctypes.c_size_t),
        ("base_form_size", ctypes.c_size_t),
    ]


class SuzumeResult(ctypes.Structure):
    _fields_ = [
        ("morphemes", ctypes.POINTER(SuzumeMorpheme)),
        ("count", ctypes.c_size_t),
        ("normalized_text", ctypes.POINTER(ctypes.c_char)),
        ("normalized_text_size", ctypes.c_size_t),
    ]


class SuzumeTags(ctypes.Structure):
    _fields_ = [
        ("tags", ctypes.POINTER(ctypes.c_char_p)),
        ("pos", ctypes.POINTER(ctypes.c_uint8)),
        ("count", ctypes.c_size_t),
    ]


class SuzumeExtendedOptions(ctypes.Structure):
    _fields_ = [
        ("preserve_vu", ctypes.c_uint8),
        ("preserve_case", ctypes.c_uint8),
        ("preserve_symbols", ctypes.c_uint8),
        ("mode", ctypes.c_uint8),
        ("lemmatize", ctypes.c_uint8),
        ("merge_compounds", ctypes.c_uint8),
        ("skip_user_dictionary", ctypes.c_uint8),
        ("skip_core_dictionary", ctypes.c_uint8),
        ("report_scorer_config", ctypes.c_uint8),
        ("skip_env_config", ctypes.c_uint8),
        ("scorer_options_json", ctypes.c_char_p),
        ("data_directory", ctypes.c_char_p),
    ]


class SuzumeTagOptions(ctypes.Structure):
    _fields_ = [
        ("pos_filter", ctypes.c_uint8),
        ("exclude_basic", ctypes.c_uint8),
        ("use_lemma", ctypes.c_uint8),
        ("min_length", ctypes.c_size_t),
        ("max_tags", ctypes.c_size_t),
        ("exclude_particles", ctypes.c_uint8),
        ("exclude_auxiliaries", ctypes.c_uint8),
        ("exclude_formal_nouns", ctypes.c_uint8),
        ("exclude_low_info", ctypes.c_uint8),
        ("remove_duplicates", ctypes.c_uint8),
    ]


# --- Library discovery ---------------------------------------------------------


def _lib_filename() -> str:
    system = platform.system()
    if system == "Darwin":
        return "libsuzume.dylib"
    if system == "Windows":
        return "suzume.dll"
    return "libsuzume.so"


def _find_library() -> str:
    """Locate the suzume shared library.

    Search order:
        1. ``SUZUME_LIB_PATH`` environment variable (explicit override).
        2. Fresh build under the project root (editable/source checkouts).
        3. Package-adjacent copy (the layout inside a built wheel).
        4. The system library path.
    """
    env_path = os.environ.get("SUZUME_LIB_PATH")
    if env_path and Path(env_path).exists():
        return env_path

    pkg_dir = Path(__file__).parent
    lib_name = _lib_filename()

    # Source checkout: bindings/python/src/suzume/_ffi.py -> project root is 4 up.
    project_root = pkg_dir.parents[3] if len(pkg_dir.parents) > 3 else None
    if project_root is not None and (project_root / "CMakeLists.txt").exists():
        # `make python-test` refreshes build-shared. Prefer that canonical
        # developer build over a possibly stale wheel workspace.
        for build_dir in ("build-shared", "build-python", "build"):
            build_path = project_root / build_dir / "lib" / lib_name
            if build_path.exists():
                return str(build_path)

    candidate = pkg_dir / lib_name
    if candidate.exists():
        return str(candidate)

    found = ctypes.util.find_library("suzume")
    if found:
        return found

    raise OSError(
        f"suzume shared library ({lib_name}) not found. Set SUZUME_LIB_PATH, or "
        "build it with: cmake -B build-shared -DBUILD_SHARED=ON && "
        "cmake --build build-shared --target suzume_shared"
    )


_HANDLE = ctypes.c_void_p
_TEXT = ctypes.c_char_p
_SIZE = ctypes.c_size_t
_CODE = ctypes.c_uint8
_RESULT_PTR = ctypes.POINTER(SuzumeResult)
_TAGS_PTR = ctypes.POINTER(SuzumeTags)
_EXTENDED_OPTIONS_PTR = ctypes.POINTER(SuzumeExtendedOptions)
_TAG_OPTIONS_PTR = ctypes.POINTER(SuzumeTagOptions)

# name -> (restype, argtypes)
_SIGNATURES: dict[str, tuple[Any, list[Any]]] = {
    "suzume_create": (_HANDLE, []),
    "suzume_init_extended_options": (None, [_EXTENDED_OPTIONS_PTR]),
    "suzume_create_with_extended_options": (_HANDLE, [_EXTENDED_OPTIONS_PTR]),
    "suzume_destroy": (None, [_HANDLE]),
    "suzume_set_mode": (ctypes.c_int, [_HANDLE, _CODE]),
    "suzume_mode": (_CODE, [_HANDLE]),
    "suzume_analyze": (_RESULT_PTR, [_HANDLE, _TEXT]),
    "suzume_analyze_n": (_RESULT_PTR, [_HANDLE, _TEXT, _SIZE]),
    "suzume_result_free": (None, [_RESULT_PTR]),
    "suzume_generate_tags": (_TAGS_PTR, [_HANDLE, _TEXT]),
    "suzume_generate_tags_n": (_TAGS_PTR, [_HANDLE, _TEXT, _SIZE]),
    "suzume_init_tag_options": (None, [_TAG_OPTIONS_PTR]),
    "suzume_generate_tags_with_options": (_TAGS_PTR, [_HANDLE, _TEXT, _TAG_OPTIONS_PTR]),
    "suzume_generate_tags_with_options_n": (
        _TAGS_PTR,
        [_HANDLE, _TEXT, _SIZE, _TAG_OPTIONS_PTR],
    ),
    "suzume_tags_free": (None, [_TAGS_PTR]),
    "suzume_load_user_dict": (ctypes.c_int, [_HANDLE, _TEXT, _SIZE]),
    "suzume_load_user_dict_count": (_SIZE, [_HANDLE, _TEXT, _SIZE]),
    "suzume_load_binary_dict": (ctypes.c_int, [_HANDLE, ctypes.POINTER(ctypes.c_uint8), _SIZE]),
    "suzume_clear_user_dictionaries": (ctypes.c_int, [_HANDLE]),
    "suzume_has_core_dictionary": (ctypes.c_int, [_HANDLE]),
    "suzume_version": (_TEXT, []),
    "suzume_abi_version": (ctypes.c_uint32, []),
    "suzume_last_error": (_TEXT, []),
    "suzume_last_error_code": (_CODE, []),
    "suzume_conjugation_type_label": (_TEXT, [_CODE]),
    "suzume_extended_pos_label": (_TEXT, [_CODE]),
    "suzume_conjugation_form_label": (_TEXT, [_CODE]),
    "suzume_pos_label": (_TEXT, [_CODE]),
    "suzume_dictionary_warning_count": (_SIZE, [_HANDLE]),
    "suzume_dictionary_warning": (_TEXT, [_HANDLE, _SIZE]),
    # ABI layout oracles (used by the layout-guard test).
    "suzume_sizeof_result": (_SIZE, []),
    "suzume_sizeof_morpheme": (_SIZE, []),
    "suzume_sizeof_tags": (_SIZE, []),
    "suzume_sizeof_tag_options": (_SIZE, []),
    "suzume_sizeof_extended_options": (_SIZE, []),
}


def _configure_signatures(lib: ctypes.CDLL) -> None:
    """Attach argtypes/restype to every function the binding calls."""
    for name, (restype, argtypes) in _SIGNATURES.items():
        fn = getattr(lib, name)
        fn.restype = restype
        fn.argtypes = argtypes
    for name in (
        "suzume_offsetof_result",
        "suzume_offsetof_morpheme",
        "suzume_offsetof_tags",
        "suzume_offsetof_tag_options",
        "suzume_offsetof_extended_options",
    ):
        fn = getattr(lib, name)
        fn.restype = ctypes.c_size_t
        fn.argtypes = [ctypes.c_uint32]


def _bundled_data_dir(pkg_dir: Path) -> Path | None:
    """Return the package dir if it ships compiled dictionaries, else None."""
    if (pkg_dir / "core.dic").exists() or (pkg_dir / "user.dic").exists():
        return pkg_dir
    return None


def load_library(lib_path: str | None = None) -> ctypes.CDLL:
    """Load the suzume shared library with configured signatures.

    Dictionary discovery is configured per analyzer, after loading the library,
    so importing this module never mutates ``SUZUME_DATA_DIR``.
    """
    path = lib_path or _find_library()

    lib = ctypes.CDLL(path)
    _configure_signatures(lib)
    return lib
