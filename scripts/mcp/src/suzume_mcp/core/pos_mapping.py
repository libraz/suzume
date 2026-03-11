"""POS mapping and correction logic ported from SuzumeUtils.pm."""

import regex

from .constants import (
    ADVERB_OVERRIDES,
    KEEP_AS_NOUN_NOT_ADJ,
    NA_ADJ_OVERRIDES,
    NOUN_AS_PRONOUN,
    PARTICLE_CORRECTIONS,
    POS_MAP,
    POS_NORM_MAP,
    PRONOUN_OVERRIDES,
    SUFFIX_AS_NOUN,
    SUZUME_POS_OVERRIDE,
    VALID_POS,
    VERB_NOT_AUX_LEMMAS,
)


def _is_katakana_onomatopoeia(surface: str) -> bool:
    """Check if a katakana string is an onomatopoeia (reduplication pattern)."""
    n = len(surface)
    if n >= 4 and n % 2 == 0:
        half = n // 2
        if surface[:half] == surface[half:]:
            return True
    # Reduplication with elongation
    return bool(regex.match(r"^(.+ー)\1$", surface))


def map_mecab_pos(token: dict | str) -> str:
    """Map MeCab POS to Suzume POS.

    Args:
        token: Either a token dict with 'pos' key, or a raw POS string.

    Returns:
        Mapped Suzume POS string.
    """
    if isinstance(token, str):
        pos = token
        if pos in VALID_POS:
            return pos
        return POS_MAP.get(pos, "Other")

    pos = token.get("pos", "")

    # Already an English POS
    if pos in VALID_POS:
        return pos

    # Rich token dict with subcategories
    pos_sub1 = token.get("pos_sub1", "")
    pos_sub2 = token.get("pos_sub2", "")
    surface = token.get("surface", "")

    # Surface-based adverb overrides
    if surface in ADVERB_OVERRIDES:
        return "Adverb"

    # よろしく: always Adverb in Suzume
    if surface == "よろしく":
        return "Adverb"

    # おめでとう: Interjection -> Adverb in Suzume
    if surface == "おめでとう":
        return "Adverb"

    # 遥か: 副詞 -> Adjective (na-adjective used adverbially)
    if surface == "遥か" and pos == "副詞":
        return "Adjective"

    # どう: Suzume treats as ナ形容詞
    if surface == "どう" and pos == "副詞":
        return "Adjective"

    # Pronoun overrides
    if surface in PRONOUN_OVERRIDES and pos == "名詞":
        return "Pronoun"

    # なん: 代名詞 -> Noun
    if surface == "なん" and pos == "名詞" and pos_sub1 == "代名詞":
        return "Noun"

    # にく (standalone hiragana): 形容詞 -> Noun
    if surface == "にく" and pos == "形容詞":
        token["lemma"] = "にく"
        return "Noun"

    # なら: 助動詞 -> Particle
    if surface == "なら" and pos == "助動詞":
        token["lemma"] = "なら"
        return "Particle"

    # ん: context-dependent
    if surface == "ん":
        if pos == "名詞":
            token["lemma"] = "の"
            return "Particle"
        if pos == "助動詞":
            token["lemma"] = "ん"
            return "Auxiliary"

    # Punctuation/brackets -> Symbol
    if regex.match(r"^[()（）\[\]【】「」『』｢｣\u3008-\u300F！？!?…‥・♥♪★☆※]$", surface):
        return "Symbol"

    # Katakana onomatopoeia -> Adverb
    if pos == "名詞" and regex.match(r"^[\u30A0-\u30FF]{2,}$", surface) and _is_katakana_onomatopoeia(surface):
        return "Adverb"

    # 違い: Noun -> Verb
    if surface == "違い" and pos == "名詞":
        token["lemma"] = "違う"
        return "Verb"

    # 時々: 副詞 -> Noun
    if surface == "時々" and pos == "副詞":
        return "Noun"

    # 推し: 動詞 -> Noun
    if surface == "推し" and pos == "動詞":
        token["lemma"] = "推し"
        return "Noun"

    # 寒し: 形容詞 -> Noun
    if surface == "寒し" and pos == "形容詞":
        token["lemma"] = "寒し"
        return "Noun"

    # 超: 接頭詞 -> Noun
    if surface == "超" and pos == "接頭詞":
        return "Noun"

    # びっくり: 名詞 -> Adverb
    if surface == "びっくり" and pos == "名詞":
        return "Adverb"

    # なんて: 副詞 -> Particle
    if surface == "なんて" and pos == "副詞":
        return "Particle"

    # 大変: -> Adverb (postprocessor handles 大変+な -> Adjective)
    if surface == "大変" and pos in ("名詞", "副詞"):
        return "Adverb"

    # っていう: -> Determiner
    if surface == "っていう" and pos == "助詞":
        return "Determiner"

    # じゃん: -> Particle
    if surface == "じゃん" and pos == "助動詞":
        token["lemma"] = "じゃん"
        return "Particle"

    # まじ: -> Adjective
    if surface == "まじ" and pos == "助動詞":
        return "Adjective"


    # や (Kansai copula): 助動詞 -> Particle
    if surface == "や" and pos == "助動詞":
        token["lemma"] = "や"
        return "Particle"

    # で (verb): lemma fix
    if surface == "で" and pos == "動詞" and token.get("lemma") == "でる":
        token["lemma"] = "出る"

    # いくら: 副詞/名詞 -> Pronoun (疑問代名詞)
    if surface == "いくら" and pos in ("副詞", "名詞"):
        return "Pronoun"

    # まして: 副詞 -> Conjunction (接続詞用法)
    if surface == "まして" and pos == "副詞":
        return "Conjunction"

    # いわば: -> Conjunction
    if surface == "いわば" and pos == "副詞":
        token["lemma"] = "言わば"
        return "Conjunction"

    # ふっくら: -> Other
    if surface == "ふっくら" and pos == "副詞":
        return "Other"

    # 畳語副詞 (刻々, アア etc.): -> Noun (except 少々)
    if pos == "副詞":
        if len(surface) == 2 and surface[0] == surface[1]:
            if surface != "少々":
                return "Noun"
        if len(surface) == 2 and surface[1] == "\u3005":
            if surface != "少々":
                return "Noun"

    # むしろ: -> Other
    if surface == "むしろ" and pos == "副詞":
        return "Other"

    # 何時: 代名詞 -> Noun
    if surface == "何時" and pos == "名詞" and pos_sub1 == "代名詞":
        return "Noun"

    # お疲れ様: -> Interjection
    if surface == "お疲れ様":
        return "Interjection"

    # Na-adjective overrides
    if surface in NA_ADJ_OVERRIDES and pos == "名詞":
        return "Adjective"

    # 名詞,接尾,助動詞語幹 -> Auxiliary
    if pos == "名詞" and pos_sub1 == "接尾" and pos_sub2 == "助動詞語幹":
        return "Auxiliary"

    # 名詞,特殊,助動詞語幹 -> Auxiliary
    if pos == "名詞" and pos_sub1 == "特殊" and pos_sub2 == "助動詞語幹":
        return "Auxiliary"

# 名詞,非自立,形容動詞語幹 (みたい) -> Auxiliary
    if pos == "名詞" and pos_sub1 == "非自立" and pos_sub2 == "形容動詞語幹":
        return "Auxiliary"

    # 動詞,非自立 -> Auxiliary (with exceptions)
    if pos == "動詞" and pos_sub1 == "非自立":
        lemma = token.get("lemma", "")
        if lemma in VERB_NOT_AUX_LEMMAS:
            return "Verb"
        # 来(kanji surface) -> Verb; くる(hiragana) -> Auxiliary
        # Suzume treats kanji 来 as Verb, hiragana くる as Auxiliary
        if lemma == "来る" and surface and not surface[0].isascii() and ord(surface[0]) >= 0x4E00:
            return "Verb"
        return "Auxiliary"

    # 動詞,接尾 -> Auxiliary
    if pos == "動詞" and pos_sub1 == "接尾":
        return "Auxiliary"

    # 名詞,代名詞 -> Pronoun
    if pos == "名詞" and pos_sub1 == "代名詞":
        return "Pronoun"

    # Noun as Pronoun overrides
    if pos == "名詞" and surface in NOUN_AS_PRONOUN:
        return "Pronoun"

    # 名詞,形容動詞語幹 -> Adjective (with exceptions)
    if pos == "名詞" and pos_sub1 == "形容動詞語幹":
        if surface in KEEP_AS_NOUN_NOT_ADJ:
            return "Noun"
        return "Adjective"

    # 嫌い: 動詞 -> Adjective
    if surface == "嫌い" and pos == "動詞":
        token["lemma"] = "嫌い"
        return "Adjective"

    # 名詞,非自立 の -> Particle
    if surface == "の" and pos == "名詞" and pos_sub1 == "非自立":
        return "Particle"

    # Suffix -> Noun exceptions
    if pos == "名詞" and pos_sub1 == "接尾" and surface in SUFFIX_AS_NOUN:
        return "Noun"

    # 名詞,接尾 -> Suffix (中 is Noun exception)
    if pos == "名詞" and pos_sub1 == "接尾":
        if surface == "中":
            return "Noun"
        return "Suffix"

    # よく: 副詞 -> Adjective
    if surface == "よく" and pos == "副詞":
        token["lemma"] = "よい"
        return "Adjective"

    # 無い/無く: 助動詞 -> Adjective
    if regex.match(r"^無[いく]$", surface) and pos == "助動詞":
        token["lemma"] = "無い"
        return "Adjective"

    # という: 助詞 -> Determiner
    if surface == "という" and pos == "助詞":
        return "Determiner"

    # Default: map from Japanese POS
    return POS_MAP.get(pos, "Other")


def normalize_pos(pos: str) -> str:
    """Normalize Suzume POS variations and apply Suzume-specific mappings."""
    normalized = POS_NORM_MAP.get(pos.upper(), pos)
    return SUZUME_POS_OVERRIDE.get(normalized, normalized)


def correct_mecab_pos(tokens: list[dict]) -> None:
    """Correct MeCab POS misclassifications (mutates tokens in-place)."""
    for idx, t in enumerate(tokens):
        surface = t.get("surface", "")
        pos = t.get("pos", "")

        # Fix kanji adverbs MeCab misclassifies as 名詞
        if surface in ("特段", "別段", "格段") and pos == "名詞":
            t["pos"] = "副詞"
            continue

        # Fix adjective 連用形 (〜く): always 形容詞, not 副詞
        # Only when lemma ends in い, or surface contains kanji (正しく etc.)
        # Excludes pure hiragana adverbs: わくわく, せっかく, とにかく, etc.
        if surface.endswith("く") and pos == "副詞":
            lemma = t.get("lemma", "")
            if lemma.endswith("い"):
                t["pos"] = "形容詞"
            elif lemma == surface and regex.search(r"[\u4E00-\u9FFF]", surface):
                # Kanji-containing surface with lemma==surface: adj 連用形
                t["pos"] = "形容詞"
                t["lemma"] = surface[:-1] + "い"

        # Fix じゃ: always 助動詞 (copula)
        if surface == "じゃ" and pos in ("助詞", "接続詞", "助動詞"):
            t["pos"] = "助動詞"
            t["lemma"] = "だ"

        # Fix ない/なかっ after じゃ: 形容詞 -> 助動詞
        if surface in ("ない", "なかっ") and pos == "形容詞":
            if idx > 0 and tokens[idx - 1].get("surface") == "じゃ":
                t["pos"] = "助動詞"
                t["lemma"] = "ない"

        # Fix ない after が (particle): 形容詞 -> 助動詞 (negation auxiliary)
        if surface == "ない" and pos == "形容詞":
            if idx > 0 and tokens[idx - 1].get("surface") == "が" and tokens[idx - 1].get("pos") in ("助詞", "Particle"):
                t["pos"] = "助動詞"
                t["lemma"] = "ない"

        # Fix な after じゃ: 助詞 -> 助動詞
        if surface == "な" and pos == "助詞":
            if idx > 0 and tokens[idx - 1].get("surface") == "じゃ":
                t["pos"] = "助動詞"
                t["lemma"] = "だ"

        # Fix 得 before し/する: Suzume treats as 得る(ichidan) renyokei
        # MeCab treats as sahen noun (得する), Suzume has 得る in dict
        if surface == "得" and pos == "名詞":
            if idx + 1 < len(tokens) and tokens[idx + 1].get("surface") in (
                "し",
                "する",
                "さ",
                "せ",
                "でき",
            ):
                t["pos"] = "動詞"
                t["lemma"] = "得る"

        # Fix particles misclassified as Noun
        if surface in PARTICLE_CORRECTIONS and pos == "Noun":
            t["pos"] = PARTICLE_CORRECTIONS[surface]

        # Fix の (名詞,非自立) as Particle
        if surface == "の" and pos == "名詞" and t.get("pos_sub1") == "非自立":
            t["pos"] = "Particle"
