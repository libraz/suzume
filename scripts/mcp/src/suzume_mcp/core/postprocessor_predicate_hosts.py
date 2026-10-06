"""Host-dependent readings of negative, sou, adverb, and predicate cells, plus frame repairs."""

import regex

from .mecab import is_single_token_of_pos
from .split_rules import base_from_mizenkei, bases_from_renyokei

# Auxiliaries that do not conjugate like a verb. The negative auxiliary ない
# selects a verbal irrealis, so after any of these ない is the adjective.
_NON_VERBAL_AUXILIARY_LEMMAS = frozenset(
    {
        "だ",
        "です",
        "たい",
        "ない",
        "らしい",
        "ます",
        "た",
        "う",
        "よう",
        "まい",
        "ぬ",
        "ん",
        "ず",
        "べし",
        "そう",
        "みたい",
    }
)


def postprocess_negative_host(tokens: list[dict]) -> bool:
    """Tag ない by its host: auxiliary after a verbal irrealis, adjective elsewhere.

    The negative auxiliary attaches only to a verb or a verb-conjugating
    auxiliary (食べ+ない, 食べ+させ+ない, 見+て+い+ない), and to the te-form whose
    いる has been dropped (見+て+ない). After a particle, the copula, an adjective
    continuative or an adjective-type auxiliary it is the (supplementary)
    adjective: 時間+が+ない, 本+で+は+ない, 本+じゃ+ない, 水+しか+ない,
    高く+ない, 食べ+たく+ない.
    """
    changed = False
    for idx, token in enumerate(tokens):
        if token.get("lemma") != "ない" or token.get("pos") not in ("Auxiliary", "Adjective"):
            continue
        previous = tokens[idx - 1] if idx > 0 else None
        verbal_host = previous is not None and (
            previous.get("pos") == "Verb"
            or (previous.get("pos") == "Auxiliary" and previous.get("lemma") not in _NON_VERBAL_AUXILIARY_LEMMAS)
            or (previous.get("pos") == "Particle" and previous.get("surface") in ("て", "で"))
        )
        wanted = "Auxiliary" if verbal_host else "Adjective"
        if token.get("pos") != wanted:
            token["pos"] = wanted
            changed = True
    return changed


def postprocess_sou_host(tokens: list[dict]) -> bool:
    """Tag そう by its host: auxiliary after a predicate, adverb elsewhere.

    The appearance auxiliary follows a continuative or stem (降り+そう, おいし+
    そう, 食べ+な+さ+そう) and the hearsay auxiliary a terminal form (降る+そう,
    教師+だ+そう, 読ん+だ+そう); both are the auxiliary そうだ whatever follows.
    With no predicate in front, そう is the demonstrative adverb (そう+だ,
    まさに+そう+だ, 彼+も+そう+言っ+た).
    """
    changed = False
    for idx, token in enumerate(tokens):
        if token.get("surface") != "そう" or token.get("pos") not in ("Adjective", "Auxiliary", "Adverb"):
            continue
        previous = tokens[idx - 1] if idx > 0 else None
        following = tokens[idx + 1] if idx + 1 < len(tokens) else None
        # A noun takes the appearance そう only as a na-adjective stem (不安+そう);
        # before a verb it is the adverb after a temporal noun (明日+そう+する).
        nominal_stem_host = (
            previous is not None
            and previous.get("pos") == "Noun"
            and (following is None or following.get("pos") != "Verb")
        )
        predicate_host = previous is not None and (
            previous.get("pos") in ("Verb", "Adjective", "Auxiliary")
            or (previous.get("pos") == "Suffix" and previous.get("surface") == "さ")
            or nominal_stem_host
        )
        wanted = "Auxiliary" if predicate_host else "Adverb"
        if token.get("pos") != wanted:
            token["pos"] = wanted
            token["lemma"] = "そう"
            changed = True
    return changed


def postprocess_adverb_host_context(tokens: list[dict]) -> bool:
    """Retag an adverb by the slot its neighbours give it.

    An adverb modifies a predicate and takes no case of its own, so:
    - before the attributive な and a noun it is a na-adjective stem
      (さすが+な+人, さんざん+な+結果);
    - before が/を/の it is a nominal (ゆめ+の+話, すぐ+の+返事);
    - a kanji-spelled く-form whose base form is an i-adjective is that
      adjective's continuative (険しく, 長く), however the reference tagged it;
      a kana one can belong to a homographic adverb (いたく is 甚く).
    """
    changed = False
    for idx, token in enumerate(tokens):
        if token.get("pos") != "Adverb":
            continue
        surface = token.get("surface", "")
        following = tokens[idx + 1] if idx + 1 < len(tokens) else {}
        after = tokens[idx + 2] if idx + 2 < len(tokens) else {}
        if following.get("surface") == "な" and following.get("lemma") == "だ" and after.get("pos") == "Noun":
            token["pos"] = "Adjective"
            changed = True
        elif following.get("pos") == "Particle" and following.get("surface") in ("が", "を", "の"):
            token["pos"] = "Noun"
            changed = True
        elif (
            surface.endswith("く")
            and regex.search(r"\p{Han}", surface)
            and is_single_token_of_pos(surface[:-1] + "い", "形容詞")
        ):
            token["pos"] = "Adjective"
            token["lemma"] = surface[:-1] + "い"
            changed = True
    return changed


_EASE_ADJECTIVE_STEMS = {"やす": "やすい", "にく": "にくい", "づら": "づらい", "がた": "がたい"}


def postprocess_predicate_cell_lemmas(tokens: list[dict]) -> bool:
    """Repair cell readings a predicate frame decides.

    - た between a continuative and そう is the stem of たい (読み+た+そう);
    - an ease stem between a continuative and そう is that adjective's stem
      (書き+にく+そう, like 読み+やす+そう);
    - じゃろ is the copula だ, like だろ;
    - a one-kanji continuative in し is the サ変 verb when that verb exists
      (要し is 要する, as 要する itself is);
    - とれ after a continuative is the contracted ておる (食べ+とれ+ば);
    - a suffix cannot open a clause (がち+で is the slang noun).
    """
    changed = False
    for idx, token in enumerate(tokens):
        surface = token.get("surface", "")
        previous = tokens[idx - 1] if idx > 0 else {}
        following = tokens[idx + 1] if idx + 1 < len(tokens) else {}
        after_continuative = previous.get("pos") == "Verb"
        # Only a godan continuative settles た+そう: its past takes the onbin
        # (読ん+だ), whereas 食べ+た+そう is also the hearsay past.
        previous_surface = previous.get("surface", "")
        previous_lemma = previous.get("lemma", "")
        godan_continuative = (
            after_continuative
            and not previous_surface.endswith(("っ", "ん", "い"))
            and not (previous_lemma.endswith("る") and previous_surface == previous_lemma[:-1])
        )
        if (
            surface == "た"
            and godan_continuative
            and following.get("surface") == "そう"
            and token.get("lemma") != "たい"
        ):
            token.update(pos="Auxiliary", lemma="たい")
            changed = True
        elif surface in _EASE_ADJECTIVE_STEMS and after_continuative and following.get("surface") == "そう":
            if token.get("pos") != "Adjective":
                token.update(pos="Adjective", lemma=_EASE_ADJECTIVE_STEMS[surface])
                changed = True
        elif (
            surface == "せ"
            and token.get("lemma") == "せる"
            and following.get("surface") == "ば"
            and after_continuative
            and previous.get("surface", "")[-1:] in "かさたなまらわがばぱ"
        ):
            # せる's hypothetical is せれ; before ば the cell is the す causative's.
            token["lemma"] = "す"
            changed = True
        elif surface == "じゃろ" and token.get("lemma") != "だ":
            token["lemma"] = "だ"
            changed = True
        elif (
            token.get("pos") == "Verb"
            and len(surface) == 2
            and surface.endswith("し")
            and regex.fullmatch(r"\p{Han}", surface[0])
            and token.get("lemma") == surface[0] + "す"
            and is_single_token_of_pos(surface[0] + "する", "動詞")
        ):
            token["lemma"] = surface[0] + "する"
            changed = True
        elif surface == "とれ" and after_continuative and token.get("pos") == "Verb":
            token.update(pos="Auxiliary", lemma="とる")
            changed = True
        elif idx == 0 and token.get("pos") == "Suffix" and len(tokens) > 1:
            token["pos"] = "Noun"
            changed = True
    return changed


_CONTRACTED_DEMONSTRATIVES = {"こん": "これ", "そん": "それ", "あん": "あれ", "どん": "どれ"}


def postprocess_frame_repairs(tokens: list[dict]) -> bool:
    """Repair readings the surrounding frame rules out.

    - で+しか before a verb is the case particle (窓口+で+しか+受付ける); the
      copula で+しか only precedes the adjective ない (本+で+しか+ない);
    - で+は before ござる is the copula, as it is after a na-adjective;
    - the adverb ことに cannot complement なる; that is こと+に+なる;
    - より opening a clause has no comparand and is the adverb (より+いっそう);
    - 何だ after a word is the pronoun plus the copula (いったい+何+だ);
    - 先 after a pronoun is the noun, not a suffix (それ+先+に).
    """
    changed = False
    idx = 0
    while idx < len(tokens):
        token = tokens[idx]
        surface = token.get("surface", "")
        previous = tokens[idx - 1] if idx > 0 else {}
        following = tokens[idx + 1] if idx + 1 < len(tokens) else {}
        after = tokens[idx + 2] if idx + 2 < len(tokens) else {}
        if surface == "で" and following.get("surface") == "しか" and after.get("pos") == "Verb":
            if token.get("pos") != "Particle":
                token.update(pos="Particle", lemma="で")
                changed = True
        elif surface == "で" and following.get("surface") == "は" and after.get("lemma") == "ござる":
            if token.get("pos") != "Auxiliary":
                token.update(pos="Auxiliary", lemma="だ")
                changed = True
        elif surface == "ことに" and token.get("pos") == "Adverb" and following.get("lemma") == "なる":
            tokens[idx : idx + 1] = [
                {"surface": "こと", "pos": "Noun", "lemma": "こと"},
                {"surface": "に", "pos": "Particle", "lemma": "に"},
            ]
            changed = True
        elif surface == "より" and idx == 0 and token.get("pos") == "Particle" and len(tokens) > 1:
            token.update(pos="Adverb", lemma="より")
            changed = True
        elif surface == "何だ" and idx > 0 and token.get("pos") == "Interjection":
            tokens[idx : idx + 1] = [
                {"surface": "何", "pos": "Pronoun", "lemma": "何"},
                {"surface": "だ", "pos": "Auxiliary", "lemma": "だ"},
            ]
            changed = True
        elif surface == "先" and token.get("pos") == "Suffix" and previous.get("pos") == "Pronoun":
            token["pos"] = "Noun"
            changed = True
        elif surface == "来さ" and token.get("lemma") == "来る" and following.get("surface", "").startswith("せ"):
            # 来る's causative is こ+させる; 来さ is no cell of 来る.
            tokens[idx : idx + 2] = [
                {"surface": "来", "pos": "Verb", "lemma": "来る"},
                {"surface": "さ" + following["surface"], "pos": "Auxiliary", "lemma": "させる"},
            ]
            changed = True
        elif surface == "来ら" and token.get("lemma") == "来る" and following.get("surface", "").startswith("れ"):
            # Likewise its passive/potential is こ+られる; 来ら is no cell of 来る.
            tokens[idx : idx + 2] = [
                {"surface": "来", "pos": "Verb", "lemma": "来る"},
                {"surface": "ら" + following["surface"], "pos": "Auxiliary", "lemma": "られる"},
            ]
            changed = True
        elif token.get("pos") == "Verb" and surface.startswith(("しそこな", "しそこね", "しそびれ")):
            # する's continuative plus the closed subsidiary, as in やり+そこなう.
            tokens[idx : idx + 1] = [
                {"surface": "し", "pos": "Verb", "lemma": "する"},
                {"surface": surface[1:], "pos": "Auxiliary", "lemma": token.get("lemma", surface)[1:]},
            ]
            changed = True
        elif (
            token.get("pos") == "Adverb"
            and surface.endswith("ず")
            and following.get("surface") == "に"
            and base_from_mizenkei(surface[:-1]) is not None
        ):
            # The adverb 思わず cannot take に; 思わ+ず+に is the negative continuative.
            tokens[idx : idx + 1] = [
                {"surface": surface[:-1], "pos": "Verb", "lemma": base_from_mizenkei(surface[:-1])},
                {"surface": "ず", "pos": "Auxiliary", "lemma": "ぬ"},
            ]
            changed = True
        elif surface == "こう" and token.get("pos") == "Adverb" and previous.get("surface") in ("て", "で"):
            # A te-form takes the contracted volitional of ていく (食べ+て+こ+う).
            tokens[idx : idx + 1] = [
                {"surface": "こ", "pos": "Auxiliary", "lemma": "いく"},
                {"surface": "う", "pos": "Auxiliary", "lemma": "う"},
            ]
            changed = True
        elif (
            token.get("pos") == "Noun"
            and following.get("surface") == "に"
            and after.get("lemma") in ("行く", "来る", "いく", "くる", "ゆく")
            and any(is_single_token_of_pos(base, "動詞") for base in bases_from_renyokei(surface))
        ):
            # Purpose 連用+に+motion takes the verb continuative (読み+に+行く).
            token.update(pos="Verb", lemma=bases_from_renyokei(surface)[0])
            changed = True
        elif (
            token.get("pos") == "Noun"
            and previous.get("surface") == "お"
            and previous.get("pos") == "Prefix"
            and following.get("lemma") == "する"
            and bases_from_renyokei(surface)
            and any(is_single_token_of_pos(base, "動詞") for base in bases_from_renyokei(surface))
        ):
            # The humble お+連用+する takes the verb continuative, as お+連用+いたす does.
            base = next(base for base in bases_from_renyokei(surface) if is_single_token_of_pos(base, "動詞"))
            token.update(pos="Verb", lemma=base)
            changed = True
        elif surface in _CONTRACTED_DEMONSTRATIVES and following.get("surface") == "だけ":
            # こん/そん/あん/どん before だけ are the contracted これ/それ/あれ/どれ.
            token.update(pos="Pronoun", lemma=_CONTRACTED_DEMONSTRATIVES[surface])
            changed = True
        elif (
            surface == "ん"
            and previous.get("pos") == "Verb"
            and following.get("surface") in ("す", "し")
            and following.get("lemma") == "する"
        ):
            # The courtesan polite んす (あり+んす = あります), not a negative plus する.
            tokens[idx : idx + 2] = [{"surface": "ん" + following["surface"], "pos": "Auxiliary", "lemma": "ます"}]
            changed = True
        elif (
            surface == "や"
            and token.get("pos") == "Particle"
            and previous.get("pos") in ("Noun", "Pronoun", "Adverb")
            and (not following or (following.get("pos") == "Particle" and following.get("surface") != "の"))
        ):
            # A coordinating や needs a following noun; at a clause end after a
            # nominal it is the western copula (そう+や+で, 雨+や+な). After an
            # adjective it is the final particle (いい+や).
            token.update(pos="Auxiliary", lemma="だ")
            changed = True
        elif (
            surface == "それで"
            and token.get("pos") == "Conjunction"
            and (following.get("lemma") in ("いい", "ええ", "よい", "ござる"))
        ):
            # Directly before the predicate it complements, それで is それ+で (それ+で+いい).
            tokens[idx : idx + 1] = [
                {"surface": "それ", "pos": "Pronoun", "lemma": "それ"},
                {"surface": "で", "pos": "Auxiliary", "lemma": "だ"},
            ]
            changed = True
        elif surface == "マジ" and following.get("surface") == "で":
            # The katakana spelling of まじ takes the same adjectival reading (まじ+で).
            token.update(pos="Adjective", lemma="まじ")
            following.update(pos="Auxiliary", lemma="だ")
            changed = True
        elif surface == "まして" and token.get("pos") == "Conjunction" and previous.get("surface") == "も":
            # Xにもまして is the te-form of 増す; the conjunction cannot follow にも.
            tokens[idx : idx + 1] = [
                {"surface": "まし", "pos": "Verb", "lemma": "増す"},
                {"surface": "て", "pos": "Particle", "lemma": "て"},
            ]
            changed = True
        idx += 1
    return changed
