"""Split rules ported from SuzumeUtils.pm apply_suzume_split()."""

import regex

from .constants import TTARA_STEMS, TTEBA_STEMS, USER_DICT_COMPOUNDS, VERB_NAI_COMPOUND_ADJECTIVES



# Mapping from MeCab causative lemma ending to base verb dictionary form ending
_CAUSATIVE_LEMMA_ENDINGS: dict[str, str] = {
    "ます": "む",  # 飲ます → 飲む
    "かす": "く",  # 書かす → 書く
    "がす": "ぐ",  # 泳がす → 泳ぐ
    "たす": "つ",  # 待たす → 待つ
    "なす": "ぬ",  # 死なす → 死ぬ
    "らす": "る",  # 降らす → 降る
    "さす": "す",  # 話さす → 話す
    "わす": "う",  # 使わす → 使う
    "ばす": "ぶ",  # 飛ばす → 飛ぶ
    "はす": "ふ",  # rare archaic row
}


def _split_causative_passive(tokens: list[dict]) -> tuple[list[dict], bool]:
    """Pre-pass: split MeCab's merged causative さ when followed by passive れ.

    MeCab sometimes merges godan verb mizenkei + causative さ into one token
    (e.g., 飲まさ from 飲まされた), while splitting correctly in other cases
    (e.g., 読まされた → 読ま + さ). This normalizes the inconsistency.

    Pattern: verb token ending in さ + next token れ (passive auxiliary)
    → split verb token into [verb_without_さ] + [さ]

    Returns:
        Tuple of (processed tokens, was_applied).
    """
    result: list[dict] = []
    applied = False
    i = 0
    while i < len(tokens):
        t = tokens[i]
        surface = t.get("surface", "")
        pos = t.get("pos", "")

        # Check: verb token ending in さ with length >= 3 (at least 2 chars before さ)
        if (
            pos == "動詞"
            and surface.endswith("さ")
            and len(surface) >= 3
            and i + 1 < len(tokens)
            and tokens[i + 1].get("surface") == "れ"
        ):
            # The preceding chars form the verb mizenkei
            verb_part = surface[:-1]
            # Reconstruct base verb lemma from MeCab causative lemma
            mecab_lemma = t.get("lemma", surface)
            verb_lemma = verb_part  # fallback: use surface
            for ending, replacement in _CAUSATIVE_LEMMA_ENDINGS.items():
                if mecab_lemma.endswith(ending):
                    verb_lemma = mecab_lemma[: -len(ending)] + replacement
                    break

            result.append({"surface": verb_part, "pos": "動詞", "lemma": verb_lemma})
            result.append({"surface": "さ", "pos": "動詞", "lemma": "する"})
            applied = True
            i += 1
            continue

        result.append(t)
        i += 1

    return result, applied

def apply_suzume_split(tokens: list[dict]) -> tuple[list[dict], str | None]:
    """Apply Suzume split rules to MeCab tokens.

    Returns:
        Tuple of (split tokens, applied rule name or None).
    """
    # Pre-pass: split merged causative さ before passive れ
    tokens, causative_applied = _split_causative_passive(tokens)

    result: list[dict] = []
    applied_rule: str | None = "causative-passive-split" if causative_applied else None

    for t in tokens:
        surface = t.get("surface", "")

        # 0a. Split MeCab single-token kanji adverbs ending in に
        # e.g., 次に → 次+に, 滅多に → 滅多+に
        if t.get("pos") == "副詞":
            m = regex.match(r"^([\p{Han}]+)(に)$", surface)
            if m:
                base = m.group(1)
                result.append({"surface": base, "pos": "名詞", "lemma": base})
                result.append({"surface": "に", "pos": "助詞", "lemma": "に"})
                if applied_rule is None:
                    applied_rule = "adverb-ni-split"
                continue

        # 0. Plural suffix ら
        m = regex.match(r"^(彼女|彼|僕|奴|我)ら$", surface)
        if m:
            result.append({"surface": m.group(1), "pos": "名詞", "pos_sub1": "代名詞", "lemma": m.group(1)})
            result.append({"surface": "ら", "pos": "名詞", "pos_sub1": "接尾", "lemma": "ら"})
            if applied_rule is None:
                applied_rule = "ra-suffix-split"
            continue

        # 1. ったら topic particle
        m = regex.match(r"^(.+)(ったら)$", surface)
        if m and len(m.group(1)) >= 3:
            stem = m.group(1)
            if stem in TTARA_STEMS:
                result.append({"surface": stem, "pos": "名詞", "lemma": stem})
                result.append({"surface": "ったら", "pos": "助詞", "lemma": "ったら"})
                if applied_rule is None:
                    applied_rule = "ttara-split"
                continue

        # 2. ってば emphatic particle
        m = regex.match(r"^(.+)(ってば)$", surface)
        if m and len(m.group(1)) >= 2:
            stem = m.group(1)
            if stem in TTEBA_STEMS:
                result.append({"surface": stem, "pos": "副詞", "lemma": stem})
                result.append({"surface": "ってば", "pos": "助詞", "lemma": "ってば"})
                if applied_rule is None:
                    applied_rule = "tteba-split"
                continue

        # 3. Unnatural kanji compounds
        m = regex.match(r"^(時分)(学校)$", surface)
        if m:
            result.append({"surface": m.group(1), "pos": "名詞", "lemma": m.group(1)})
            result.append({"surface": m.group(2), "pos": "名詞", "lemma": m.group(2)})
            if applied_rule is None:
                applied_rule = "compound-split"
            continue

        # 4. ねたい adjective -> ね|たい
        if surface == "ねたい" and t.get("pos") == "形容詞":
            result.append({"surface": "ね", "pos": "動詞", "lemma": "ねる"})
            result.append({"surface": "たい", "pos": "助動詞", "lemma": "たい"})
            if applied_rule is None:
                applied_rule = "netai-split"
            continue

        # 5. Compound nouns with dictionary words at start
        m = regex.match(r"^(自然)(言語処理.+)$", surface)
        if m:
            result.append({"surface": m.group(1), "pos": "名詞", "lemma": m.group(1)})
            result.append({"surface": m.group(2), "pos": "名詞", "lemma": m.group(2)})
            if applied_rule is None:
                applied_rule = "compound-dict-split"
            continue

        # 6. Prefecture + city compounds
        m = regex.match(r"^(.+県)(.+市)$", surface)
        if m:
            result.append({"surface": m.group(1), "pos": "名詞", "lemma": m.group(1)})
            result.append({"surface": m.group(2), "pos": "名詞", "lemma": m.group(2)})
            if applied_rule is None:
                applied_rule = "prefecture-city-split"
            continue

        # 7. Kanji + Katakana compound nouns
        if t.get("pos") == "名詞" and surface not in USER_DICT_COMPOUNDS:
            m = regex.match(r"^([\p{Han}]+)([\u30A0-\u30FFー]+)$", surface)
            if m:
                result.append({"surface": m.group(1), "pos": "名詞", "lemma": m.group(1)})
                result.append({"surface": m.group(2), "pos": "名詞", "lemma": m.group(2)})
                if applied_rule is None:
                    applied_rule = "kanji-katakana-split"
                continue

        # 8a. Kango + として adverbs: 依然として → 依然と|し|て
        # MeCab treats these as single adverbs, but they are taru-adjective
        # adverb forms (漢語 + と) + する conjugation (し + て)
        if t.get("pos") == "副詞":
            m = regex.match(r"^([\p{Han}]{2,}と)(して)$", surface)
            if m:
                adv_part = m.group(1)
                result.append({"surface": adv_part, "pos": "副詞", "lemma": adv_part})
                result.append({"surface": "し", "pos": "動詞", "lemma": "する"})
                result.append({"surface": "て", "pos": "助詞", "lemma": "て"})
                if applied_rule is None:
                    applied_rule = "kango-toshite-split"
                continue

        # 8. Copula negation: じゃない -> じゃ|ない
        if surface == "じゃない" and t.get("pos") == "助動詞":
            result.append({"surface": "じゃ", "pos": "助動詞", "lemma": "だ"})
            result.append({"surface": "ない", "pos": "助動詞", "lemma": "ない"})
            if applied_rule is None:
                applied_rule = "copula-negation-split"
            continue

        # 9. Onomatopoeia + っと + する conjugation (single MeCab token)
        # MeCab may treat ぷるんっとした as one token; Suzume splits as ぷるんっと+し+た
        m = regex.match(
            r"^([\p{Hiragana}\p{Katakana}ー]{1,6}っと)(し|した|して|する|すれ|しろ|せ|さ|される|された|させ|させる)$",
            surface,
        )
        if m:
            adv_part = m.group(1)
            verb_part = m.group(2)
            result.append({"surface": adv_part, "pos": "副詞", "lemma": adv_part})
            # Split する conjugation further: した→し+た, して→し+て, etc.
            if verb_part in ("した", "して", "しろ"):
                result.append({"surface": verb_part[:1], "pos": "動詞", "lemma": "する"})
                result.append(
                    {"surface": verb_part[1:], "pos": "助動詞" if verb_part[1:] == "た" else "助詞", "lemma": verb_part[1:]}
                )
            elif verb_part in ("される", "された", "させ", "させる"):
                result.append({"surface": "さ", "pos": "動詞", "lemma": "する"})
                rest = verb_part[1:]
                result.append({"surface": rest, "pos": "動詞", "lemma": rest + ("る" if not rest.endswith("る") else "")})
            else:
                result.append({"surface": verb_part, "pos": "動詞", "lemma": "する"})
            if applied_rule is None:
                applied_rule = "onomatopoeia-tto-suru-split"
            continue

        # 10. Verb+ない compound adjective split
        # MeCab merges verb renyokei + ない as single adjective token
        # (e.g., 揺るぎない, 何気ない), but Suzume correctly splits them
        if t.get("pos") == "形容詞" and surface in VERB_NAI_COMPOUND_ADJECTIVES:
            verb_part = surface[:-len("ない")]
            result.append({"surface": verb_part, "pos": "動詞", "lemma": verb_part})
            result.append({"surface": "ない", "pos": "助動詞", "lemma": "ない"})
            if applied_rule is None:
                applied_rule = "verb-nai-compound-split"
            continue

        # 11. Literary volitional ん: verb+ん → verb + ん
        # MeCab merges ichidan verb + ん (literary volitional =む/よう)
        # as single token with conj_form 体言接続特殊
        # e.g., 乗り越えん → 乗り越え + ん, 越えん → 越え + ん
        if (
            t.get("pos") == "動詞"
            and surface.endswith("ん")
            and len(surface) >= 2
            and t.get("conj_form") == "体言接続特殊"
        ):
            verb_part = surface[:-len("ん")]
            lemma = t.get("lemma", "")
            result.append({"surface": verb_part, "pos": "動詞", "lemma": lemma})
            result.append({"surface": "ん", "pos": "助動詞", "lemma": "ん"})
            if applied_rule is None:
                applied_rule = "literary-volitional-n-split"
            continue

        # No split needed
        result.append(t)

    return result, applied_rule
