"""Merge rules ported from SuzumeUtils.pm apply_suzume_merge()."""

from .constants import FIXED_FUNCTION_LEMMAS
from .merge_postprocessor_common import _continuative_verb_tokens
from .merge_postprocessors import apply_merge_postprocessors
from .merge_rules_closed_units import (
    _merge_closed_function_units,
    _merge_fixed_search_units,
    _merge_negation_and_intensifier_units,
)
from .merge_rules_compounds import (
    _merge_colloquial_forms,
    _merge_name_and_script_compounds,
    _merge_tari_adverbs,
    _merge_verb_derived_nouns,
)
from .merge_rules_elongation import (
    _merge_adjective_forms,
    _merge_emphatic_lengthening,
    _merge_vowel_repetition_and_contractions,
)
from .merge_rules_quantities import (
    _merge_counter_expressions,
    _merge_dates_and_literals,
    _merge_numbered_and_prefixed_nouns,
    _merge_reduplications_and_derived_verbs,
)
from .merge_rules_state import MergeState
from .merge_rules_verbs import _merge_compound_verbs, _merge_lexicalized_words, _merge_te_form_units


def _separate_counter_case_particles(tokens: list[dict]) -> list[dict]:
    """Expose a case particle lexicalized onto a counter token."""
    separated: list[dict] = []
    for index, token in enumerate(tokens):
        surface = token.get("surface", "")
        is_counter = (
            token.get("pos") == "名詞" and token.get("pos_sub1") == "接尾" and token.get("pos_sub2") == "助数詞"
        )
        following_is_numeral = (
            index + 1 < len(tokens)
            and tokens[index + 1].get("pos") == "名詞"
            and tokens[index + 1].get("pos_sub1") == "数"
        )
        if not is_counter or not surface.endswith("の") or len(surface) == 1 or following_is_numeral:
            separated.append(token)
            continue
        counter = token.copy()
        counter["surface"] = surface[:-1]
        if counter.get("lemma") == surface:
            counter["lemma"] = surface[:-1]
        separated.extend((counter, {"surface": "の", "pos": "助詞", "pos_sub1": "格助詞", "lemma": "の"}))
    return separated


_SOUROU = "候"


def _retag_classical_sourou(tokens: list[dict]) -> list[dict]:
    """Read a standalone 候 as the epistolary verb 候ふ unless something modifies it.

    The dictionary knows 候 only as the noun of 時候 (新緑の候), which is a head
    that takes an adnominal modifier.  Everywhere else the character is the
    classical verb: after a continuative (申し+候), after a nominal predicate
    (御座+候), or opening a clause (候+て, 候+間).  A continuative the dictionary
    filed as a noun (存じ, 見え) is read back as the verb it is.
    """
    retagged: list[dict] = []
    for token in tokens:
        previous = retagged[-1] if retagged else None
        modified = previous is not None and (
            previous.get("pos") == "連体詞" or (previous.get("pos") == "助詞" and previous.get("pos_sub1") == "連体化")
        )
        # A suffix needs a nominal host, so behind the verb it is the formal noun
        # the predicate modifies (候+間).
        if previous is not None and previous.get("lemma") == "候ふ" and token.get("pos_sub1") == "接尾":
            retagged.append({**token, "pos_sub1": "非自立"})
            continue
        if token.get("surface") != _SOUROU or token.get("pos") != "名詞" or modified:
            retagged.append(token)
            continue
        if previous is not None and previous.get("pos") == "名詞":
            reading = _continuative_verb_tokens(previous.get("surface", ""))
            if reading is not None and len(reading) == 1:
                retagged[-1] = {**previous, **reading[0], "conj_form": "連用形"}
        retagged.append({"surface": _SOUROU, "pos": "動詞", "pos_sub1": "自立", "conj_form": "基本形", "lemma": "候ふ"})
    return retagged


def _begin_token(state: MergeState) -> None:
    """Load the current token and its source offset."""
    tokens = state.tokens
    text = state.text
    standalone_noun_indexes = state.standalone_noun_indexes
    i = state.i
    try:
        t = tokens[i].copy()
        if i in standalone_noun_indexes:
            t["pos"] = "名詞"
            t["pos_sub1"] = "一般"
            t["pos_sub2"] = None
        merged = False

        # Calculate position in text
        pos_in_text = sum(len(tokens[k].get("surface", "")) for k in range(i))
        remaining = text[pos_in_text:] if pos_in_text < len(text) else ""
    finally:
        state.t = t
        state.merged = merged
        state.remaining = remaining
        state.pos_in_text = pos_in_text


def _pass_through(state: MergeState) -> None:
    """Copy the current token unchanged."""
    result = state.result
    i = state.i
    t = state.t
    merged = state.merged
    try:
        # No merge: pass through
        if not merged:
            lemma = t.get("lemma") or t.get("surface", "")
            lemma = FIXED_FUNCTION_LEMMAS.get(t.get("surface", ""), lemma)
            result.append(
                {
                    "surface": t.get("surface", ""),
                    "pos": t.get("pos", ""),
                    "pos_sub1": t.get("pos_sub1"),
                    "pos_sub2": t.get("pos_sub2"),
                    "conj_type": t.get("conj_type"),
                    "conj_form": t.get("conj_form"),
                    "lemma": lemma,
                }
            )
            i += 1
    finally:
        state.i = i


_MERGE_STEPS = (
    _merge_closed_function_units,
    _merge_fixed_search_units,
    _merge_negation_and_intensifier_units,
    _merge_dates_and_literals,
    _merge_reduplications_and_derived_verbs,
    _merge_counter_expressions,
    _merge_numbered_and_prefixed_nouns,
    _merge_adjective_forms,
    _merge_vowel_repetition_and_contractions,
    _merge_emphatic_lengthening,
    _merge_tari_adverbs,
    _merge_verb_derived_nouns,
    _merge_name_and_script_compounds,
    _merge_colloquial_forms,
    _merge_compound_verbs,
    _merge_lexicalized_words,
    _merge_te_form_units,
)


def apply_suzume_merge(tokens: list[dict], text: str) -> tuple[list[dict], str | None]:
    """Apply Suzume merge rules to MeCab tokens.

    Returns:
        Tuple of (merged tokens, applied rule name or None).
    """
    tokens = _retag_classical_sourou(_separate_counter_case_particles(tokens))
    state = MergeState(tokens=tokens, text=text)

    while state.i < len(tokens):
        _begin_token(state)
        if any(step(state) for step in _MERGE_STEPS):
            continue
        _pass_through(state)

    return apply_merge_postprocessors(state.result, state.applied_rule)
