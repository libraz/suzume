"""Mutable scan state shared by the ordered merge steps."""

from dataclasses import dataclass, field


@dataclass
class MergeState:
    """Mutable scan state shared by the ordered merge steps."""

    tokens: list[dict]
    text: str
    result: list[dict] = field(default_factory=list)
    standalone_noun_indexes: set[int] = field(default_factory=set)
    i: int = 0
    t: dict = field(default_factory=dict)
    merged: bool = False
    applied_rule: str | None = None
    remaining: str = ""
    pos_in_text: int = 0
