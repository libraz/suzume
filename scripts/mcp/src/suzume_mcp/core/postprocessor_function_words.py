"""Context-dependent particles, copulas, and closed function words."""

from .constants import (
    INTERROGATIVES,
)
from .postprocessor_common import _raw_analysis, reports_mutation


@reports_mutation
def postprocess_demo(tokens: list[dict]) -> bool:
    """Resolve でも by clause position rather than its reference POS tag."""
    for idx, t in enumerate(tokens):
        if t.get("surface") != "でも":
            continue
        at_clause_boundary = idx == 0 or tokens[idx - 1].get("pos") == "Symbol"
        t["pos"] = "Conjunction" if at_clause_boundary else "Particle"
        t["lemma"] = "でも"


def postprocess_closed_function_words(tokens: list[dict]) -> bool:
    """Normalize finite conjunction and pronoun classes mislabelled by MeCab."""
    conjunctions = frozenset({"しかるに", "もって"})
    pronouns = frozenset({"各々", "各自", "あれこれ", "何かしら"})
    adverbial_ambiguities = frozenset({"また", "やや", "およそ", "すこぶる", "おおいに", "つとめて"})
    changed = False
    for idx, token in enumerate(tokens):
        surface = token.get("surface", "")
        target_pos = (
            "Conjunction"
            if surface in conjunctions
            else "Pronoun"
            if surface in pronouns
            else "Adverb"
            if surface in adverbial_ambiguities
            else ""
        )
        if not target_pos or token.get("pos") == target_pos:
            continue
        token["pos"] = target_pos
        token["lemma"] = surface
        changed = True
        # MeCab can carry a suffix reading across the following content word
        # after a formal adverb (つとめて水を...).  At this proven adverbial
        # boundary the following lexical token is an ordinary noun.
        if (
            idx + 2 < len(tokens)
            and tokens[idx + 1].get("pos") == "Suffix"
            and tokens[idx + 2].get("pos") == "Particle"
        ):
            tokens[idx + 1]["pos"] = "Noun"
            tokens[idx + 1]["lemma"] = tokens[idx + 1].get("surface", "")
    return changed


def postprocess_renyokei_compound_particle(tokens: list[dict]) -> bool:
    """Keep closed particle plus continuative-form expressions as search units."""
    compound_particles = {
        ("に", "つれ", "て"): "につれて",
        ("に", "かけ", "て"): "にかけて",
    }
    changed = False
    index = 0
    while index + 2 < len(tokens):
        surfaces = tuple(tokens[index + offset].get("surface") for offset in range(3))
        compound = compound_particles.get(surfaces)
        if compound is None:
            index += 1
            continue
        tokens[index : index + 3] = [{"surface": compound, "pos": "Particle", "lemma": compound}]
        if (
            compound == "にかけて"
            and index + 1 < len(tokens)
            and tokens[index + 1].get("surface") == "続く"
            and tokens[index + 1].get("pos") == "Auxiliary"
        ):
            tokens[index + 1]["pos"] = "Verb"
            tokens[index + 1]["lemma"] = "続く"
        changed = True
        index += 1
    return changed


def postprocess_compound_case_particle_aru(tokens: list[dict]) -> bool:
    """Classify ある after an adverbial or a non-subject particle.

    A lexical adverb or closed compound case particle ending in て is not the
    conjunctive particle of a resultative 〜てある construction.  MeCab can
    nevertheless carry that dependent reading into the following cells of
    ある.  Restore the independent verb for inflected cells, while keeping the
    distinct pre-nominal ある reading as a determiner.
    """
    changed = False
    for index in range(1, len(tokens)):
        previous = tokens[index - 1]
        token = tokens[index]
        following = tokens[index + 1] if index + 1 < len(tokens) else None
        previous_is_productive_te = previous.get("pos") == "Particle" and previous.get("surface") in ("て", "で")
        if previous_is_productive_te and token.get("surface") == "ある" and token.get("lemma") == "ある":
            if token.get("pos") != "Verb":
                token["pos"] = "Verb"
                changed = True
            continue
        previous_is_adverbial = previous.get("pos") == "Adverb"
        previous_is_topic = previous.get("surface") == "は"
        previous_is_compound_case = (
            len(previous.get("surface", "")) > 1
            and previous.get("pos") == "Particle"
            and previous.get("pos_sub1") == "格助詞"
            and previous.get("pos_sub2") == "連語"
        )
        if not (previous_is_adverbial or previous_is_topic or previous_is_compound_case):
            continue
        if token.get("surface") == "ある" and following is not None and following.get("pos") in ("Noun", "Pronoun"):
            if token.get("pos") != "Determiner" or token.get("lemma") != "ある":
                token["pos"] = "Determiner"
                token["lemma"] = "ある"
                changed = True
        elif (
            (previous_is_adverbial or previous_is_compound_case)
            and token.get("surface") in ("ある", "あり", "あれ", "あっ")
            and token.get("lemma") == "ある"
        ):
            if token.get("pos") != "Verb":
                token["pos"] = "Verb"
                changed = True
    return changed


def postprocess_to_areba_conditional(tokens: list[dict]) -> bool:
    """Preserve the verb inflection and conditional-particle boundary in とあれば."""
    changed = False
    index = 0
    while index + 1 < len(tokens):
        if tokens[index].get("surface") == "と" and tokens[index + 1].get("surface") == "あれば":
            tokens[index + 1 : index + 2] = [
                {"surface": "あれ", "pos": "Verb", "lemma": "ある"},
                {"surface": "ば", "pos": "Particle", "lemma": "ば"},
            ]
            changed = True
            index += 3
            continue
        index += 1
    return changed


def postprocess_indefinite_ka(tokens: list[dict]) -> bool:
    """Separate indefinite か from a pronoun and restore existential いる."""
    indefinite_pronoun_stems = frozenset({"なに", "何", "だれ", "誰", "どこ", "どちら", "どれ", "どなた"})
    changed = False
    idx = 0
    while idx < len(tokens):
        token = tokens[idx]
        surface = token.get("surface", "")

        if idx > 0 and idx + 1 < len(tokens) and surface == "で" and tokens[idx + 1].get("surface") == "も":
            if tokens[idx - 1].get("surface") in INTERROGATIVES:
                tokens[idx : idx + 2] = [{"surface": "でも", "pos": "Particle", "lemma": "でも"}]
                changed = True
                continue
        stem = surface[:-1] if surface.endswith("か") else ""
        if stem in indefinite_pronoun_stems:
            tokens[idx : idx + 1] = [
                {"surface": stem, "pos": "Pronoun", "lemma": stem},
                {"surface": "か", "pos": "Particle", "lemma": "か"},
            ]
            changed = True
            idx += 1
        elif idx > 0 and tokens[idx - 1].get("pos") == "Pronoun" and surface == "かい":
            tokens[idx : idx + 1] = [
                {"surface": "か", "pos": "Particle", "lemma": "か"},
                {"surface": "い", "pos": "Verb", "lemma": "いる"},
            ]
            changed = True
            idx += 1
        idx += 1

    for idx in range(1, len(tokens)):
        if tokens[idx - 1].get("surface") == "か" and tokens[idx].get("surface") in ("い", "いる"):
            tokens[idx]["pos"] = "Verb"
            tokens[idx]["lemma"] = "いる"
            changed = True
    return changed


@reports_mutation
def postprocess_de_particle(tokens: list[dict]) -> bool:
    """Normalize copular で before a binding particle."""
    binding_surfaces = frozenset({"こそ", "さえ", "すら", "しか"})
    for idx in range(1, len(tokens) - 1):
        token = tokens[idx]
        if token.get("surface") != "で" or token.get("pos") != "Particle":
            continue
        if tokens[idx + 1].get("surface") not in binding_surfaces:
            continue
        if tokens[idx - 1].get("pos") not in ("Noun", "Pronoun", "Adjective"):
            continue
        token["pos"] = "Auxiliary"
        token["lemma"] = "だ"


@reports_mutation
def postprocess_de_after_nominal(tokens: list[dict]) -> bool:
    """Keep で the word it is when only punctuation stands beside it.

    Punctuation carries no morphology, so it cannot turn the locative case
    particle into the copula's continuative. The analyzer nonetheless does
    exactly that behind a non-independent noun — 空の下で休む keeps the particle
    while 空の下で、休む does not — which leaves the same slot holding two
    different words. Reading the run of words on its own settles it: where a
    comma was the only difference the reading flips back, and where none was
    involved the probe text is the original and nothing moves.

    A binding particle behind the で is the one environment that selects the
    copula on its own (本でしか, わけでも), and the rules that name it decide
    those positions; the probe stays out of them.
    """
    binding_surfaces = frozenset({"も", "は", "しか", "こそ", "さえ", "すら"})
    unpunctuated = "".join(token.get("surface", "") for token in tokens)
    offset = 0
    for idx, token in enumerate(tokens):
        surface = token.get("surface", "")
        start = offset
        offset += len(surface)
        if idx == 0 or surface != "で" or token.get("pos") != "Auxiliary":
            continue
        if tokens[idx - 1].get("pos") not in ("Noun", "Pronoun"):
            continue
        if idx + 1 < len(tokens) and tokens[idx + 1].get("surface") in binding_surfaces:
            continue
        probe = _raw_analysis(unpunctuated)[1].get(start)
        if probe is None or probe.get("surface") != "で" or probe.get("pos") != "助詞":
            continue
        token["pos"] = "Particle"
        token["lemma"] = "で"


def postprocess_te_form_contraction(tokens: list[dict]) -> bool:
    """Tag じゃ after an onbin verb as the te-form contraction, like ちゃ.

    ちゃ (= ては) and じゃ (= では) are one paradigm; only the voicing of the
    conjunctive particle differs, selected by the onbin stem in front of it
    (書い+ちゃ, 読ん+じゃ, 泳い+じゃ). MeCab already reads ちゃ as a particle but
    reads じゃ as the copula だ, which would put a copula straight onto a verb
    continuative. The copula reading stays untouched after a nominal (本じゃない).
    """
    changed = False
    onbin_tails = ("ん", "い")
    for idx in range(1, len(tokens)):
        token = tokens[idx]
        if token.get("surface") != "じゃ" or token.get("pos") != "Auxiliary":
            continue
        previous = tokens[idx - 1]
        if previous.get("pos") != "Verb" or not previous.get("surface", "").endswith(onbin_tails):
            continue
        token["pos"] = "Particle"
        token["lemma"] = "じゃ"
        changed = True
    return changed


@reports_mutation
def postprocess_dai_final_particle(tokens: list[dict]) -> bool:
    """Normalize closed sentence-final particles that MeCab labels as nouns."""
    if not tokens:
        return
    token = tokens[-1]
    final_particle_surfaces = frozenset({"だい", "ゲソ", "げそ"})
    if token.get("surface") in final_particle_surfaces and token.get("pos") == "Noun":
        token["pos"] = "Particle"
        token["lemma"] = token["surface"]


@reports_mutation
def postprocess_final_particle_quotative_tte(tokens: list[dict]) -> None:
    """Restore the final particle な before a quotative って.

    かな closes a clause with two final particles, and the quotative って that
    trails it belongs to the reported speech.  The reference analyzer instead
    reads な+っ as the euphonic stem of なる (行こうか+なっ+て) or swallows the か
    as well into 叶う (いい+かなっ+て) -- both of which need a nominal argument
    the position cannot supply.
    """
    # The verb readings need a subject phrase, so only a finite predicate in
    # front of the run identifies the final-particle chain.
    predicate_hosts = ("Adjective", "Auxiliary", "Verb")
    for idx in range(len(tokens) - 1, 0, -1):
        token = tokens[idx]
        if token.get("surface") != "て" or token.get("pos") != "Particle":
            continue
        stem = tokens[idx - 1]
        quotative = {"surface": "って", "pos": "Particle", "lemma": "って"}
        final_na = {"surface": "な", "pos": "Particle", "lemma": "な"}
        final_ka = {"surface": "か", "pos": "Particle", "lemma": "か"}
        if (
            stem.get("surface") == "なっ"
            and idx >= 2
            and tokens[idx - 2].get("surface") == "か"
            and tokens[idx - 2].get("pos") == "Particle"
        ):
            tokens[idx - 1 : idx + 1] = [final_na, quotative]
            continue
        if stem.get("surface") == "かなっ" and idx >= 2 and tokens[idx - 2].get("pos") in predicate_hosts:
            tokens[idx - 1 : idx + 1] = [final_ka, final_na, quotative]


@reports_mutation
def postprocess_tteba_emphatic_particle(tokens: list[dict]) -> None:
    """Keep the emphatic final particle ってば whole and off the predicate.

    って and ば are both closed function words and the pair carries no internal
    inflection boundary, so it is one search unit.  MeCab splits it, and after
    a na-adjective stem it goes further and reads だ+って as the adverbial
    particle だって -- which strands the host as a bare noun and loses the
    copula.  Restore the copula and merge the particle in both readings.
    """
    for idx in range(len(tokens) - 1, 0, -1):
        token = tokens[idx]
        # MeCab tags a clause-final ば as a noun when nothing follows it.
        if token.get("surface") != "ば" or token.get("pos") not in ("Particle", "Noun"):
            continue
        following = tokens[idx + 1] if idx + 1 < len(tokens) else None
        if (
            following is not None
            and following.get("pos") != "Symbol"
            and not (following.get("pos") == "Particle" and following.get("surface") in ("よ", "ね", "さ"))
        ):
            continue
        previous = tokens[idx - 1]
        head = previous.get("surface", "")
        if previous.get("pos") != "Particle" or head not in ("って", "だって"):
            continue
        emphatic = {"surface": "ってば", "pos": "Particle", "lemma": "ってば"}
        copula = {"surface": "だ", "pos": "Auxiliary", "lemma": "だ"}
        tokens[idx - 1 : idx + 1] = [copula, emphatic] if head == "だって" else [emphatic]


def postprocess_nanka_particle(tokens: list[dict]) -> bool:
    """Normalize the colloquial adverbial particle なんか from MeCab's filler tag."""
    changed = False
    for token in tokens:
        if token.get("surface") == "なんか" and token.get("pos") == "Other":
            token["pos"] = "Particle"
            token["lemma"] = "なんか"
            changed = True
    return changed


def postprocess_kiri_limited_particle(tokens: list[dict]) -> bool:
    """Normalize hiragana きり as the closed limiting particle."""
    changed = False
    for idx, token in enumerate(tokens):
        previous = tokens[idx - 1] if idx > 0 else None
        if token.get("surface") == "きり" and token.get("pos") == "Noun":
            if previous is not None and previous.get("pos") == "Adverb":
                previous["pos"] = "Noun"
            token["pos"] = "Particle"
            token["lemma"] = "きり"
            changed = True
    return changed


def postprocess_dewa_aru_boundary(tokens: list[dict]) -> bool:
    """Split copular で + binding は before lexical ある."""
    for idx in range(len(tokens) - 1):
        token = tokens[idx]
        following = tokens[idx + 1]
        if token.get("surface") != "では" or following.get("pos") != "Verb" or following.get("lemma") != "ある":
            continue
        tokens[idx : idx + 1] = [
            {"surface": "で", "pos": "Auxiliary", "lemma": "だ"},
            {"surface": "は", "pos": "Particle", "lemma": "は"},
        ]
        return True
    return False


def postprocess_nominal_conjunction_homograph(tokens: list[dict]) -> bool:
    """Read a conjunction homograph as a noun in a nominally selected slot."""
    changed = False
    assertive_copula = {"だ", "だっ"}
    for idx, token in enumerate(tokens):
        if token.get("pos") != "Conjunction":
            continue
        previous = tokens[idx - 1] if idx > 0 else {}
        following = tokens[idx + 1] if idx + 1 < len(tokens) else {}
        selected_by_genitive = previous.get("pos") == "Particle" and previous.get("surface") == "の"
        selected_by_copula = following.get("pos") == "Auxiliary" and following.get("surface") in assertive_copula
        if selected_by_genitive or selected_by_copula:
            token["pos"] = "Noun"
            token["lemma"] = token.get("surface", "")
            changed = True
    return changed


def postprocess_interjection_before_copula(tokens: list[dict]) -> bool:
    """Read an interjection homograph nominally when the copula predicates over it.

    An interjection is a complete utterance on its own, so the plain assertive
    copula cannot predicate over it and its presence identifies the nominal
    reading of a homograph (そらだった is そら the noun, not the exclamation).
    The polite copula is excluded because it also attaches to a fixed formulaic
    interjection as a politeness marker rather than as a predicate
    (すみませんでした, お疲れ様です), and so are で and な, which are equally
    spelled like particles an interjection may precede.
    """
    assertive_copula = ("だ", "だっ")
    changed = False
    for idx in range(len(tokens) - 1):
        token = tokens[idx]
        following = tokens[idx + 1]
        if token.get("pos") != "Interjection" or following.get("pos") != "Auxiliary":
            continue
        if following.get("surface") not in assertive_copula:
            continue
        token["pos"] = "Noun"
        token["lemma"] = token.get("surface", "")
        changed = True
    return changed


def postprocess_negative_conjunctive_de(tokens: list[dict]) -> bool:
    """Tag で after a negative auxiliary as the conjunctive particle.

    ないで and its contraction んで are one construction (行かないでいい,
    行かんでいい), but the reference reads で as the copula after ん and fuses
    でも after ない.  The negative cannot host a copula, so で is the particle
    in both and a following も/は stays a separate binding particle.  Before
    ござる/ある the で is the copula of a nominalized clause (知らぬでござる).
    """
    changed = False
    idx = 1
    while idx < len(tokens):
        previous, token = tokens[idx - 1], tokens[idx]
        following = tokens[idx + 1] if idx + 1 < len(tokens) else {}
        if (
            previous.get("pos") == "Auxiliary"
            and previous.get("lemma") in ("ない", "ん", "ぬ")
            and following.get("lemma") not in ("ござる", "ある")
        ):
            surface = token.get("surface")
            if surface == "で" and token.get("pos") == "Auxiliary":
                token.update(pos="Particle", lemma="で")
                changed = True
            elif surface == "でも" and token.get("pos") == "Particle":
                tokens[idx : idx + 1] = [
                    {"surface": "で", "pos": "Particle", "lemma": "で"},
                    {"surface": "も", "pos": "Particle", "lemma": "も"},
                ]
                changed = True
        idx += 1
    return changed


_INTERJECTION_PREDICATE_HOMOGRAPHS = {"ええ": "Adjective"}


def postprocess_interjection_after_te(tokens: list[dict]) -> bool:
    """Read an interjection homograph predicatively after the te-form.

    An interjection opens its own utterance, so it cannot complete a clause
    the conjunctive て/で left open; the predicate homograph is meant there
    (食べてええ is the permissive ええ, like 食べていい).
    """
    changed = False
    for idx in range(1, len(tokens)):
        token = tokens[idx]
        previous = tokens[idx - 1]
        pos = _INTERJECTION_PREDICATE_HOMOGRAPHS.get(token.get("surface", ""))
        if pos is None or token.get("pos") != "Interjection":
            continue
        if previous.get("pos") != "Particle" or previous.get("surface") not in ("て", "で"):
            continue
        token["pos"] = pos
        token["lemma"] = token["surface"]
        changed = True
    return changed


def postprocess_copula_neg(tokens: list[dict]) -> bool:
    """Normalize copular negative chains and their continuative adjective."""
    changed = False
    for idx in range(1, len(tokens) - 2):
        predicate = tokens[idx - 1]
        copula = tokens[idx]
        topic = tokens[idx + 1]
        negative = tokens[idx + 2]
        if (
            predicate.get("pos") in ("Noun", "Pronoun", "Adjective", "Particle", "Suffix", "Auxiliary")
            and predicate.get("lemma") not in ("ない", "ん", "ぬ")
            and copula.get("surface") == "で"
            and topic.get("surface") == "は"
            and negative.get("surface") in ("ない", "なく", "なかっ")
        ):
            copula["pos"] = "Auxiliary"
            copula["lemma"] = "だ"
            negative["pos"] = "Auxiliary"
            negative["lemma"] = "ない"
            changed = True

    for i in range(1, len(tokens)):
        t = tokens[i]
        if t.get("surface") != "なく" or t.get("pos") != "Auxiliary":
            continue
        prev = tokens[i - 1].get("surface", "")
        if prev in ("じゃ", "で"):
            t["pos"] = "Adjective"
            t["lemma"] = "ない"
            changed = True
    return changed


@reports_mutation
def postprocess_de_aru(tokens: list[dict]) -> bool:
    """Fix copula で+ある/あり/あっ pattern based on context."""
    for i in range(len(tokens)):
        t = tokens[i]
        if t.get("surface") != "で":
            continue
        if i >= len(tokens) - 1:
            continue

        nxt = tokens[i + 1]
        nxt_surface = nxt.get("surface", "")

        if nxt_surface not in ("ある", "あり", "あれ", "あっ"):
            continue

        prev_pos = tokens[i - 1].get("pos", "") if i > 0 else ""
        is_past = nxt_surface == "あっ"

        if is_past and prev_pos in ("Noun", "Pronoun", "Suffix"):
            # N+であった: で→Particle, あっ→Verb
            t["pos"] = "Particle"
            t["lemma"] = "で"
            if nxt.get("pos") == "Auxiliary":
                nxt["pos"] = "Verb"
                nxt["lemma"] = "ある"
        elif is_past:
            # Na-adj+であった or other: keep as-is (copula chain)
            pass
        else:
            # Present/continuous forms: で→Auxiliary(だ), ある→Verb
            t["pos"] = "Auxiliary"
            t["lemma"] = "だ"
            if nxt.get("pos") == "Auxiliary":
                nxt["pos"] = "Verb"
                nxt["lemma"] = "ある"


@reports_mutation
def postprocess_monono_conjunction(tokens: list[dict]) -> bool:
    """Normalize concessive ものの as a closed connective particle."""
    for idx, token in enumerate(tokens):
        if token.get("surface") == "ものの" and idx > 0:
            if tokens[idx - 1].get("pos") in ("Verb", "Auxiliary", "Adjective"):
                token["pos"] = "Particle"
                token["lemma"] = "ものの"


def postprocess_quotative_determiner_spelling(tokens: list[dict]) -> bool:
    """Move the boundary of なんと+いう onto the pronoun plus quotative determiner.

    The reference dictionary reads 何という as the interrogative pronoun plus the
    quotative 連体詞, but reads its kana spelling なんという as the exclamatory
    adverb なんと plus the verb いう. The construction is the same one; only the
    script differs, so the kana spelling inherits the kanji spelling's boundary.

    Only the uninflected いう directly before a noun qualifies. An inflected form
    is the genuine adverb-plus-verb reading (なんといっても), and so is いう before
    anything other than a noun (なんというか).
    """
    changed = False
    idx = 0
    while idx + 2 < len(tokens):
        adverb, verb, head = tokens[idx], tokens[idx + 1], tokens[idx + 2]
        if adverb.get("surface") != "なんと" or verb.get("surface") != "いう" or head.get("pos") != "Noun":
            idx += 1
            continue
        adverb["surface"] = "なん"
        adverb["pos"] = "Pronoun"
        adverb["lemma"] = "なん"
        verb["surface"] = "という"
        verb["pos"] = "Determiner"
        verb["lemma"] = "という"
        changed = True
        idx += 2
    return changed


def postprocess_nano_quotative(tokens: list[dict]) -> bool:
    """Undo the kana 名乗る the reference reads into な+の+って.

    After a nominal, adverb or adjectival host, なのっ+て is the attributive
    copula, the nominalizer and the quotative particle (どうなのって話).  The
    verb reading needs a subject-marked argument, so a host that ends a
    phrase with a particle keeps it (彼はなのって).
    """
    changed = False
    idx = 1
    while idx + 1 < len(tokens):
        token, following, previous = tokens[idx], tokens[idx + 1], tokens[idx - 1]
        if (
            token.get("surface") == "なのっ"
            and token.get("pos") == "Verb"
            and following.get("surface") == "て"
            and previous.get("pos") in ("Noun", "Pronoun", "Adverb", "Adjective")
        ):
            tokens[idx : idx + 2] = [
                {"surface": "な", "pos": "Auxiliary", "lemma": "だ"},
                {"surface": "の", "pos": "Particle", "lemma": "の"},
                {"surface": "って", "pos": "Particle", "lemma": "って"},
            ]
            changed = True
            idx += 3
            continue
        idx += 1
    return changed


def postprocess_nan_copula_nominalizer(tokens: list[dict]) -> bool:
    """Read なん after a predicate nominal as な+ん, and すか after ん as す+か.

    そうなんすか and おなじなんよ carry the attributive copula and the
    nominalizer exactly as そうなんですか does; the reference reads the pair
    as the pronoun なん when the polite copula is clipped or absent, and the
    clipped copula す+か as an unknown noun.
    """
    changed = False
    idx = 1
    while idx < len(tokens):
        token, previous = tokens[idx], tokens[idx - 1]
        following = tokens[idx + 1] if idx + 1 < len(tokens) else None
        if (
            token.get("surface") == "なん"
            and token.get("pos") == "Pronoun"
            and previous.get("pos") in ("Noun", "Adjective", "Determiner", "Adverb")
            and following is not None
            and following.get("surface", "").startswith(("す", "よ", "だ", "じゃ", "ね"))
        ):
            tokens[idx : idx + 1] = [
                {"surface": "な", "pos": "Auxiliary", "lemma": "だ"},
                {"surface": "ん", "pos": "Particle", "lemma": "の"},
            ]
            changed = True
            idx += 2
            continue
        if token.get("surface") == "すか" and token.get("pos") == "Noun" and previous.get("surface") == "ん":
            tokens[idx : idx + 1] = [
                {"surface": "す", "pos": "Auxiliary", "lemma": "です"},
                {"surface": "か", "pos": "Particle", "lemma": "か"},
            ]
            changed = True
            idx += 2
            continue
        idx += 1
    return changed


def postprocess_now_final_particle(tokens: list[dict]) -> bool:
    """Read the clause-final なう ("right now") as a final particle.

    考えたなう and 東京なう close a predicate or a nominal with the colloquial
    particle. The reference reads it as a colloquial ない, which this spelling
    never is, or as な plus an interjection う after an auxiliary.
    """
    changed = False
    idx = 0
    while idx < len(tokens):
        token = tokens[idx]
        following = tokens[idx + 1] if idx + 1 < len(tokens) else None
        if token.get("surface") == "なう" and token.get("pos") == "Adjective":
            tokens[idx] = {"surface": "なう", "pos": "Particle", "lemma": "なう"}
            changed = True
        elif (
            token.get("surface") == "な"
            and token.get("pos") == "Particle"
            and following is not None
            and following.get("surface") == "う"
            and following.get("pos") == "Interjection"
            and (idx + 2 == len(tokens) or tokens[idx + 2].get("pos") in ("Symbol", "Particle"))
        ):
            tokens[idx : idx + 2] = [{"surface": "なう", "pos": "Particle", "lemma": "なう"}]
            changed = True
        idx += 1
    return changed


_HELD_FINAL_PARTICLES = frozenset("さよぞわ")
_HELD_VOWELS = {"さ": "あぁー", "わ": "あぁー", "よ": "おぉー", "ぞ": "おぉー"}


def postprocess_kamo_before_final_particle(tokens: list[dict]) -> bool:
    """Tag a kana かも closed by a final particle as the adverbial particle.

    The reference reads a clause-opening かも as the noun 鴨 before よ but as
    the particle before ね/な; a bare final particle selects the elliptical
    conjecture in both, as it does after a host (雨かもよ).
    """
    changed = False
    for idx in range(len(tokens) - 1):
        token, following = tokens[idx], tokens[idx + 1]
        if (
            token.get("surface") == "かも"
            and token.get("pos") == "Noun"
            and following.get("pos") == "Particle"
            and following.get("surface", "")[:1] in _HELD_FINAL_PARTICLES | {"ね", "な"}
        ):
            token.update(pos="Particle", lemma="かも")
            changed = True
    return changed


def postprocess_held_final_particle(tokens: list[dict]) -> bool:
    """Tag a final particle drawn out on its own vowel as that particle.

    After a word, さあ/よお/よー are the particles さ/よ held at the clause end;
    the reference calls the same spelling an interjection or keeps the mark in
    the lemma depending on the vowel letter. Sentence-initial さあ stays the
    interjection.
    """
    changed = False
    for idx in range(1, len(tokens)):
        token, previous = tokens[idx], tokens[idx - 1]
        surface = token.get("surface", "")
        following = tokens[idx + 1] if idx + 1 < len(tokens) else None
        if (
            len(surface) == 2
            and surface[0] in _HELD_FINAL_PARTICLES
            and surface[1] in _HELD_VOWELS[surface[0]]
            and token.get("pos") in ("Interjection", "Particle")
            and previous.get("pos") not in ("Symbol", "Interjection", "Conjunction", "Other")
            and (following is None or following.get("pos") == "Symbol")
        ):
            token.update(pos="Particle", lemma=surface[0])
            changed = True
    return changed
