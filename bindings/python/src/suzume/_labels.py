"""C-ABI morpheme flag bits (``SUZUME_MORPHEME_*`` in ``suzume_c.h``)."""

from __future__ import annotations

FLAG_USER_DICT = 1 << 0
FLAG_FORMAL_NOUN = 1 << 1
FLAG_LOW_INFO = 1 << 2
FLAG_UNKNOWN = 1 << 3
FLAG_FROM_DICTIONARY = 1 << 4
FLAG_CONJUGATABLE = 1 << 5
