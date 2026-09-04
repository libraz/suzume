"""POS mapping and correction logic ported from SuzumeUtils.pm."""

import unicodedata

import regex

from .constants import (
    ADVERB_OVERRIDES,
    BENEFACTIVE_REQUEST_LEMMAS,
    CLASSICAL_KERI_CONJ_TYPE,
    CLASSICAL_KI_CONJ_TYPE,
    DERIVATIONAL_SUFFIX_VERB_LEMMAS,
    DIALECT_FINAL_PARTICLES,
    FINITE_PREDECESSOR_CONJ_FORM,
    HISTORICAL_KANA_RESPELLING,
    KEEP_AS_NOUN_NOT_ADJ,
    NA_ADJ_OVERRIDES,
    NOUN_AS_PRONOUN,
    PARTICLE_CORRECTIONS,
    POS_MAP,
    POS_NORM_MAP,
    PRONOUN_OVERRIDES,
    SUFFIX_AS_NOUN,
    SUZUME_POS_OVERRIDE,
    TEXT_SYMBOLS,
    VALID_POS,
    VERB_NOT_AUX_LEMMAS,
    is_all_kanji,
)
from .mecab import is_single_token_of_pos


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

    # correct_mecab_pos marks Unicode symbols and enclosed numerals as
    # その他 so the oracle retains the same OTHER tokens as the tokenizer.
    if pos == "その他":
        return "Other"

    # Rich token dict with subcategories
    pos_sub1 = token.get("pos_sub1", "")
    pos_sub2 = token.get("pos_sub2", "")
    surface = token.get("surface", "")

    # Surface-based adverb overrides. Historical kana is a one-for-one
    # respelling of the same word, so the override follows the word rather than
    # the orthography (いづれ as well as いずれ).
    if surface in ADVERB_OVERRIDES or surface.translate(HISTORICAL_KANA_RESPELLING) in ADVERB_OVERRIDES:
        return "Adverb"

    # よろしく: always Adverb in Suzume
    if surface == "よろしく":
        return "Adverb"

    # おめでとう: Interjection -> Adverb in Suzume
    if surface == "おめでとう":
        return "Adverb"

    # あかん: the Kansai counterpart of いけない. It is absent from the
    # reference dictionary, which falls back to an interjection, but it closes
    # an obligation or prohibition chain as a predicate.
    if surface == "あかん":
        return "Auxiliary"

    # 遥か: 副詞 -> Adjective (na-adjective used adverbially)
    if surface == "遥か" and pos == "副詞":
        return "Adjective"

    # どう: Suzume treats as ナ形容詞
    if surface == "どう" and pos == "副詞":
        return "Adjective"

    # Pronoun overrides
    if surface in PRONOUN_OVERRIDES and pos == "名詞":
        return "Pronoun"

    # にく (standalone hiragana): 形容詞 -> Noun
    if surface == "にく" and pos == "形容詞":
        token["lemma"] = "にく"
        return "Noun"

    # なら: 助動詞 -> Particle
    if surface == "なら" and pos == "助動詞":
        token["lemma"] = "なら"
        return "Particle"

    # おろか is a fixed adverbial particle, never an honorific-prefix noun.
    if surface == "おろか":
        token["lemma"] = "おろか"
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

    # 時々: 副詞 -> Noun
    if surface == "時々" and pos == "副詞":
        return "Noun"

    # 推し: 動詞 -> Noun
    if surface == "推し" and pos == "動詞":
        token["lemma"] = "推し"
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

    # 畳語副詞 (刻々, アア etc.): -> Noun (except 少々)
    if pos == "副詞":
        if len(surface) == 2 and surface[0] == surface[1]:
            if surface != "少々":
                return "Noun"
        if len(surface) == 2 and surface[1] == "\u3005":
            if surface != "少々":
                return "Noun"

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
        if token.get("lemma", "") in DERIVATIONAL_SUFFIX_VERB_LEMMAS:
            return "Verb"
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

    # Fixed quotative determiners: IPADIC files these closed adnominal units
    # under particles even though they modify the following nominal.
    if surface in ("という", "といった", "とかいう") and pos == "助詞":
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

        # 記号 tokens are dropped by the symbol filter in get_expected_tokens,
        # so a token that carries real text must not stay labelled 記号 or the
        # character disappears from the expected tokens entirely. Two cases:
        # IPADIC has no entry for many Unicode scripts and supplementary-plane
        # kanji, and labels them 記号. A surface made only of Unicode letters,
        # marks, or decimal digits is text rather than punctuation.
        carries_only_text = bool(surface) and regex.fullmatch(r"[\p{L}\p{M}\p{Nd}]+", surface) is not None
        if surface and (
            surface in TEXT_SYMBOLS
            or all(unicodedata.category(char).startswith("S") or unicodedata.category(char) == "No" for char in surface)
        ):
            t["pos"] = "その他"
            t["pos_sub1"] = ""
            continue
        if pos == "記号" and (t.get("pos_sub1", "") == "アルファベット" or is_all_kanji(surface) or carries_only_text):
            t["pos"] = "名詞"
            t["pos_sub1"] = "一般"
            continue

        # The classical perfect ぬ has no continuative entry, so the reference
        # dictionary reads the にけり chain as the case particle に followed by
        # the ra-row verb 蹴る spelled in kana. What actually follows there is the
        # classical past, whose cells are exactly the three spellings gated here,
        # and the perfect's continuative attaches to a predicate rather than to a
        # nominal — so a noun in front of the particle leaves the case reading
        # and its verb alone (犬にける). The kanji spelling never enters the rule,
        # so a real kick keeps its own reading (壁を蹴る).
        if (
            pos == "動詞"
            and t.get("lemma") == "ける"
            and surface in ("けり", "ける", "けれ")
            and idx > 0
            and tokens[idx - 1].get("surface") == "に"
            and tokens[idx - 1].get("pos") == "助詞"
            and (idx < 2 or tokens[idx - 2].get("pos") != "名詞")
        ):
            t.update({"pos": "助動詞", "pos_sub1": "*", "conj_type": CLASSICAL_KERI_CONJ_TYPE, "lemma": "けり"})
            continue

        # Fix kanji adverbs MeCab misclassifies as 名詞
        if surface in ("特段", "別段", "格段") and pos == "名詞":
            t["pos"] = "副詞"
            continue

        # A one-kanji Ichidan imperative plus the final particle よ is a
        # productive command (見ろよ, 出ろよ), even when the reference lexicon
        # lacks the verb and returns the two-mora span as an unknown noun.
        if (
            pos == "名詞"
            and regex.fullmatch(r"\p{Han}ろ", surface) is not None
            and idx + 1 < len(tokens)
            and tokens[idx + 1].get("surface") == "よ"
        ):
            t.update({"pos": "動詞", "pos_sub1": "自立", "lemma": surface[:-1] + "る"})
            continue

        # んじゃ is the nominalizer plus the contracted copula wherever a
        # predicate precedes it (読む+ん+じゃ+ない). Opening a fragment, the
        # nominalizer has no host and the analyzer falls back to the dialectal
        # conjunction; the morphemes are the same either way.
        if surface == "んじゃ" and pos == "接続詞":
            t.update({"surface": "ん", "pos": "名詞", "pos_sub1": "非自立", "lemma": "ん"})
            tokens.insert(idx + 1, {"surface": "じゃ", "pos": "助詞", "pos_sub1": "副助詞", "lemma": "じゃ"})
            continue

        # The kanji spelling of an interrogative quantity pronoun is absent from
        # the reference dictionary, which falls back to a plain noun; the kana
        # spelling of the same word is tagged 代名詞 (いくつ, いくら).
        if surface in ("幾つ", "幾ら") and pos == "名詞":
            t["pos_sub1"] = "代名詞"
            continue

        # A kanji adverb ending in に is a noun plus the case particle whenever a
        # kanji nominal precedes it: 確認次第に is 確認次第 + に, not 確認 followed
        # by the manner adverb 次第に. The same split happens later for the
        # standalone adverb, but by then the search-unit merge has already run
        # and the nominal it exposes can no longer join the noun in front of it.
        # The same holds for a bound suffix the dictionary does not know at all:
        # it comes back as an unknown noun with the case particle inside it
        # (線路+づたい+に). Three morae of kana keep the short nouns that really
        # do end in に out of it.
        unknown_kana_noun_with_case = (
            pos == "名詞"
            and t.get("pos_sub1") == "一般"
            and len(surface) >= 4
            and regex.fullmatch(r"\p{Hiragana}+に", surface) is not None
        )
        if ((pos == "副詞" and regex.fullmatch(r"[\p{Han}]+に", surface)) or unknown_kana_noun_with_case) and (
            idx > 0 and tokens[idx - 1].get("pos") == "名詞" and is_all_kanji(tokens[idx - 1].get("surface", ""))
        ):
            base = surface[:-1]
            # A kana chunk the dictionary does not know, bound to the noun in
            # front of it, is that noun's suffix; a known adverbial nominal
            # keeps its own class.
            sub1 = "接尾" if unknown_kana_noun_with_case else "副詞可能"
            t.update({"surface": base, "pos": "名詞", "pos_sub1": sub1, "lemma": base})
            tokens.insert(idx + 1, {"surface": "に", "pos": "助詞", "pos_sub1": "格助詞", "lemma": "に"})
            continue

        # The causal formal noun ゆえ/故 is in the reference dictionary, but a
        # predicate behind it makes the analyzer prefer a reading that fits the
        # slot better: a fabricated ru-verb for the kana form, the honorific
        # prefix 故 (the late ...) for the kanji one. Both readings are wrong in
        # 故あって参加する, where the noun heads its own clause.
        if surface in ("ゆえ", "故") and pos in ("動詞", "接頭詞"):
            t["pos"] = "名詞"
            t["pos_sub1"] = "非自立"
            t["lemma"] = surface
            # The predicate that followed it was read as a subsidiary of the
            # verb reading just withdrawn; behind a nominal it heads the clause,
            # and the reciprocal あう it was taken for is the existential ある.
            following = tokens[idx + 1] if idx + 1 < len(tokens) else None
            if following is not None and following.get("pos") == "動詞" and following.get("pos_sub1") == "非自立":
                following["pos_sub1"] = "自立"
                if following.get("lemma") == "あう":
                    following["lemma"] = "ある"
            continue

        # Fix adjective 連用形 (〜く): always 形容詞, not 副詞
        # Excludes pure hiragana adverbs: わくわく, せっかく, とにかく, etc.
        if surface.endswith("く") and pos == "副詞" and surface not in ADVERB_OVERRIDES:
            lemma = t.get("lemma", "")
            if lemma.endswith("い"):
                # The analyzer already resolved the continuative to its adjective.
                t["pos"] = "形容詞"
            elif (
                lemma == surface
                and regex.search(r"[\u4E00-\u9FFF]", surface)
                and is_single_token_of_pos(surface[:-1] + "\u3044", "\u5f62\u5bb9\u8a5e")
            ):
                # The lemma still spells the continuative, so the adjective it
                # belongs to has to be reconstructed. The kanji spelling says the
                # writer chose the adjective over a lexicalized adverb written in
                # kana (よろしく, いたく), but it does not say the adjective exists:
                # a fossilized ク語法 adverb is written the same way and reaches
                # nothing (恐らく would become 恐らい). Ask the dictionary for the
                # form before adopting it, so an adverb the analyzer already
                # tagged correctly is not overwritten with a non-word.
                t["pos"] = "形容詞"
                t["lemma"] = surface[:-1] + "い"

        # Fix じゃ: always 助動詞 (copula)
        if surface == "じゃ" and pos in ("助詞", "接続詞", "助動詞"):
            t["pos"] = "助動詞"
            t["lemma"] = "だ"

        # Fix っす: colloquial contraction of です; canonical base form is です
        if surface in ("っす", "っした", "っすか"):
            t["pos"] = "助動詞"
            t["lemma"] = "です"

        # Fix なかれ: 形容詞 -> 助動詞. The classical prohibitive is a bound form
        # of the negative that only appears after a terminal predicate
        # (確認するなかれ), so it belongs with the other classical auxiliaries
        # (けり, べし), which carry their own lemma rather than a modern base.
        if surface == "なかれ" and pos == "形容詞":
            t["pos"] = "助動詞"

        # Disambiguate まじ by its host. The classical negative conjectural shares
        # its surface with the colloquial na-adjective: the auxiliary needs a verb
        # in front of it (確認せまじ), while the adjective follows a nominal or opens
        # the clause (それはまじ, まじで困る). MeCab tags both 助動詞, so the reading
        # is decided here rather than mapped unconditionally.
        if surface == "まじ" and pos == "助動詞":
            if idx > 0 and tokens[idx - 1].get("pos") in ("動詞", "Verb"):
                t["lemma"] = "まじ"
            else:
                t["pos"] = "形容詞"

        # Fix ない/なかっ after じゃ: 形容詞 -> 助動詞
        if surface in ("ない", "なかっ") and pos == "形容詞":
            if idx > 0 and tokens[idx - 1].get("surface") == "じゃ":
                t["pos"] = "助動詞"
                t["lemma"] = "ない"

        # Fix ない after が (particle): 形容詞 -> 助動詞 (negation auxiliary)
        if surface == "ない" and pos == "形容詞":
            if (
                idx > 0
                and tokens[idx - 1].get("surface") == "が"
                and tokens[idx - 1].get("pos") in ("助詞", "Particle")
            ):
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
        if (
            surface in PARTICLE_CORRECTIONS
            and pos in ("名詞", "Noun", "感動詞", "Interjection")
            and t.get("pos_sub1") != "接尾"
        ):
            t["pos"] = PARTICLE_CORRECTIONS[surface]

        # Regional request forms of the benefactive くれる are read as unrelated
        # verbs by the reference dictionary (おくれ as 遅れる, けろ as ける). The
        # te-form conjunctive particle in front identifies the subsidiary
        # reading, which Suzume tags like every other benefactive.
        if surface in BENEFACTIVE_REQUEST_LEMMAS and idx > 0 and tokens[idx - 1].get("surface") in ("て", "で"):
            if tokens[idx - 1].get("pos") in ("Particle", "助詞"):
                t["pos"] = "Auxiliary"
                t["lemma"] = BENEFACTIVE_REQUEST_LEMMAS[surface]

        # A regional final particle is outside the reference dictionary, so it
        # arrives as a bare noun. It can close either an inflected predicate or
        # a nominal predicate (飲む+ばい, 本+ばい). A prefix in that slot is the
        # nominal head of the predicate, not a productive modifier.
        if surface in DIALECT_FINAL_PARTICLES and pos in ("Noun", "名詞", "Interjection", "感動詞") and idx > 0:
            preceding = tokens[idx - 1]
            if preceding.get("pos", "") in (
                "Verb",
                "動詞",
                "Auxiliary",
                "助動詞",
                "Adjective",
                "形容詞",
                "Noun",
                "名詞",
                "Prefix",
                "接頭詞",
            ):
                if preceding.get("pos") in ("Prefix", "接頭詞"):
                    preceding["pos"] = "Noun"
                t["pos"] = "Particle"

        # The regional causal き is read as the classical past auxiliary, which
        # it is homographic with. Only the form in front separates them: the
        # classical auxiliary attaches to a continuative, so a finite predicate
        # before it identifies the conjunctive particle instead.
        if (
            surface == "き"
            and t.get("conj_type") == CLASSICAL_KI_CONJ_TYPE
            and idx > 0
            and tokens[idx - 1].get("conj_form") == FINITE_PREDECESSOR_CONJ_FORM
        ):
            t["pos"] = "Particle"

        # Fix の (名詞,非自立) as Particle
        if surface == "の" and pos == "名詞" and t.get("pos_sub1") == "非自立":
            t["pos"] = "Particle"
