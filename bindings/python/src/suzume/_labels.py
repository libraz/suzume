"""Binding-local presentation labels and C-ABI flag bits."""

from __future__ import annotations

POS_ENGLISH = (
    "OTHER",
    "NOUN",
    "VERB",
    "ADJ",
    "ADV",
    "PARTICLE",
    "AUX",
    "CONJ",
    "DET",
    "PRON",
    "PREFIX",
    "SUFFIX",
    "INTJ",
    "SYMBOL",
    "OTHER",
)
POS_JAPANESE = (
    "その他",
    "名詞",
    "動詞",
    "形容詞",
    "副詞",
    "助詞",
    "助動詞",
    "接続詞",
    "連体詞",
    "代名詞",
    "接頭辞",
    "接尾辞",
    "感動詞",
    "記号",
    "その他",
)

FLAG_USER_DICT = 1 << 0
FLAG_FORMAL_NOUN = 1 << 1
FLAG_LOW_INFO = 1 << 2
FLAG_UNKNOWN = 1 << 3
FLAG_FROM_DICTIONARY = 1 << 4
FLAG_CONJUGATABLE = 1 << 5


def _lookup(values: tuple[str, ...], code: int, fallback: str) -> str:
    return values[code] if 0 <= code < len(values) else fallback


def pos_japanese(code: int) -> str:
    return _lookup(POS_JAPANESE, code, "その他")
