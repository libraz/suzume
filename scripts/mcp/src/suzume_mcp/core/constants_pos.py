"""POS normalization, lemma, and homograph override tables."""

# POS normalization map (uppercase/variations -> canonical form)
POS_NORM_MAP: dict[str, str] = {
    "NOUN": "Noun",
    "VERB": "Verb",
    "ADJ": "Adjective",
    "ADJECTIVE": "Adjective",
    "ADV": "Adverb",
    "ADVERB": "Adverb",
    "PARTICLE": "Particle",
    "PART": "Particle",
    "AUX": "Auxiliary",
    "AUXILIARY": "Auxiliary",
    "CONJ": "Conjunction",
    "CONJUNCTION": "Conjunction",
    "INTJ": "Interjection",
    "INTERJECTION": "Interjection",
    "SYMBOL": "Symbol",
    "OTHER": "Other",
    "PREFIX": "Prefix",
    "SUFFIX": "Suffix",
    "DET": "Determiner",
    "DETERMINER": "Determiner",
    "ADNOMINAL": "Adnominal",
    "PRON": "Pronoun",
    "PRONOUN": "Pronoun",
    "FILLER": "Filler",
}


# Suzume-specific POS overrides
SUZUME_POS_OVERRIDE: dict[str, str] = {
    "Adnominal": "Determiner",
    "Filler": "Other",
}


# Inflected forms of the copula. Its negation takes the supplementary
# adjective, unlike a verbal auxiliary's, so the two are told apart by surface.
COPULA_SURFACES: frozenset[str] = frozenset({"だ", "だっ", "で", "です", "でし", "でしょ", "な", "なら"})


# Historical kana that modern orthography respells one-for-one. は and を are
# left out: their historical use is confined to the particles, which already
# carry the modern spelling.
HISTORICAL_KANA_RESPELLING = str.maketrans({"づ": "ず", "ぢ": "じ", "ゐ": "い", "ゑ": "え"})


# Adverb overrides (words MeCab misclassifies)
ADVERB_OVERRIDES: set[str] = {
    "全く",
    "間もなく",
    "一切",
    "一切合切",
    "いっさい",
    "いま",
    "このほど",
    "たかだか",
    "むしろ",
    "いずれ",
    "いつか",
    "しどろもどろ",
    "その後",
    "なるほど",
    "たくさん",
    "ようけ",
    "ぎょうさん",
    "かく",
    "あらまし",
    "めちゃ",
}


# Adverb/noun homographs whose nominal reading is selected by an overt case,
# topic, or genitive particle. This finite set mirrors closed lexical entries;
# ordinary adverbs such as まったく remain adverbs before の.
ADVERB_NOMINAL_HOMOGRAPHS: frozenset[str] = frozenset({"一切", "一切合切", "いっさい", "いま", "このほど", "むしろ"})


# Fixed function-word lemmas that differ from a reference analyzer's legacy
# inflectional analysis.  These are lexical entries, not productive rules.
FIXED_FUNCTION_LEMMAS: dict[str, str] = {
    "全く": "全く",
    "あるいは": "或いは",
    # Regional copulas. Their dictionary form is the standard copula they
    # stand in for, not the regional surface.
    "だべ": "だ",
    "やねん": "だ",
    "だっちゃ": "だ",
    # The classical prohibitive is its own dictionary form, like the other
    # classical auxiliaries, rather than the modern negative it descends from.
    "なかれ": "なかれ",
}


# Modern headwords the reference dictionary spells with a classical stem the
# word has since lost.  あしい is not a word -- the modern reflex of 悪し is 悪い
# -- but the dictionary carries both entries, so the inflected cells of the
# classical paradigm resolve to the wrong one.
CLASSICAL_ADJECTIVE_LEMMA_OVERRIDES: dict[str, str] = {"悪しい": "悪い"}


# Pronoun overrides (名詞 -> Pronoun)
PRONOUN_OVERRIDES: set[str] = {
    "皆",
    "みんな",
    "みな",
    "某",
    "あなた",
    "あんた",
    "拙者",
    "我輩",
}


# Kana personal pronouns the reference dictionary does not list as one word.
KANA_PERSONAL_PRONOUNS: tuple[str, ...] = ("かのじょ",)


# Archaic personal pronouns the reference does not list. なんじ is left out: in
# kana it is just as often 何時.
ARCHAIC_PERSONAL_PRONOUNS: tuple[str, ...] = ("そなた", "おぬし", "わて", "それがし", "わらわ", "拙僧", "そち")


# Na-adjective overrides (名詞 -> Adjective)
NA_ADJ_OVERRIDES: set[str] = {
    "しんちょう",
    "しずか",
    "おだやか",
    "げんき",
    "きれい",
    "ありきたり",
    "無限",
    "滅多",
}


# Degree words that are an adverb and an adjectival noun at once. They modify a
# predicate directly (大変おいしい) and also inflect through the copula
# (大変だ, 大変な問題); the reference dictionary tags the copula cell Adverb for
# some of them and Adjective for others (そうだ), so the paradigm is completed here.
ADVERBIAL_NA_ADJECTIVES: frozenset[str] = frozenset({"大変", "たいへん", "もっとも", "当然"})


# Words to keep as Noun despite 形容動詞語幹 classification
KEEP_AS_NOUN_NOT_ADJ: set[str] = {
    "マジ",
    "乙",
    "公式",
    "積極",
    "傍若無人",
}


# Noun -> Pronoun overrides
NOUN_AS_PRONOUN: set[str] = {"彼女", "奴", "我", "わし"}


# Suffix -> Noun overrides
SUFFIX_AS_NOUN: set[str] = {"様", "末", "ごろ", "行き", "毛"}


# Canonical spellings accepted by tests/common/test_case.cpp::posEnum().
# Aliases such as NOUN are useful at external API boundaries, but test
# expectations must use exactly these values or the C++ loader returns Unknown.
VALID_POS: frozenset[str] = frozenset(
    {
        "Noun",
        "Verb",
        "Adjective",
        "Adverb",
        "Particle",
        "Auxiliary",
        "Conjunction",
        "Determiner",
        "Pronoun",
        "Prefix",
        "Suffix",
        "Interjection",
        "Symbol",
        "Other",
    }
)


# Interrogatives for でも context detection
# Interrogative pronouns and quantifiers. Followed by でも they build the
# indefinite (誰でも, どちらでも), which is one particle rather than the copula
# plus 係助詞, whichever predicate comes next.
INTERROGATIVES: set[str] = {
    "何",
    "なに",
    "誰",
    "だれ",
    "どこ",
    "どちら",
    "どなた",
    "いつ",
    "どれ",
    "いくら",
    "どんな",
}


# Non-自立 verb lemmas that stay as Verb (not Auxiliary)
VERB_NOT_AUX_LEMMAS: set[str] = {
    "すぎる",
    # The kanji spelling of the same excessive verb; the reference tags it as
    # an auxiliary only because of its own lexicon (読みすぎる, 読み過ぎる).
    "過ぎる",
    # The inceptive verb in kana; its kanji spelling 始める is already a verb.
    "はじめる",
    "くださる",
    "下さる",
    "いたす",
    # Same humble verb as いたす, only written in kanji. The reference analyzer
    # tags one as a verb and the other as an auxiliary purely because of its own
    # lexicon, which would make the oracle contradict itself for 確認いたします
    # and 確認致します.
    "致す",
    # The potential of the same humble verb いただく keeps its POS.
    "いただける",
    "頂ける",
    "頂く",
    "あげる",
    "くれる",
    "もらう",
    "始める",
    "続ける",
    # Inceptive かける is the same aspectual verb as 始める (書か+さ+れ+かけ+た).
    "かける",
    "終わる",
    "終える",
    "出す",
    "直す",
    "合う",
    "込む",
    "いく",
    "いる",
    "ほしい",
    "いただく",
    "ちゃう",
    "ちまう",
    "いらっしゃる",
}


# 動詞,接尾 lemmas that stay as Verb. Kept separate from VERB_NOT_AUX_LEMMAS
# because that set is consulted only for 動詞,非自立, where its members carry a
# different reading. がかる inflects as a full godan verb, and the reference
# analyzer applies the 接尾 tag only to the hosts it knows lexically, which
# contradicts the rejoined form produced for every other host.
DERIVATIONAL_SUFFIX_VERB_LEMMAS: set[str] = {"がかる"}


# Cells of a bound derivational suffix verb that the reference analyzer loses to
# a homographic noun. It reads the suffix correctly wherever the spelling is not
# a word of its own (形式ばって -> ばっ/ばる), so the suffix reading is already
# established and only these cells contradict it: ばった is also the insect.
# Each entry gives the suffix surface, its lemma, and the auxiliary the noun
# reading swallowed.
BOUND_SUFFIX_VERB_NOUN_CELLS: dict[str, tuple[str, str, str]] = {
    "ばった": ("ばっ", "ばる", "た"),
}


# ったら split pronoun stems
TTARA_STEMS: set[str] = {
    "あなた",
    "おまえ",
    "きみ",
    "君",
    "彼",
    "彼女",
    "あいつ",
    "こいつ",
    "誰",
    "何",
    "これ",
    "それ",
    "あれ",
}


# ってば split stems
TTEBA_STEMS: set[str] = {"もう", "いい", "だめ", "ダメ", "嫌", "やだ"}
