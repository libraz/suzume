"""Canonical Godan conjugation row mappings shared by oracle rules."""

GODAN_A_ROW_TO_BASE: dict[str, str] = {
    "わ": "う",
    "か": "く",
    "が": "ぐ",
    "さ": "す",
    "た": "つ",
    "な": "ぬ",
    "ば": "ぶ",
    "ま": "む",
    "ら": "る",
}

GODAN_E_ROW_TO_BASE: dict[str, str] = {
    "え": "う",
    "け": "く",
    "げ": "ぐ",
    "せ": "す",
    "て": "つ",
    "ね": "ぬ",
    "べ": "ぶ",
    "め": "む",
    "れ": "る",
}


def takes_continuative(token: dict) -> bool:
    """Whether ``token`` attaches only to a verb continuative: た, て, ます."""
    return token.get("surface") in ("た", "たら", "たり", "て") or token.get("lemma") == "ます"
