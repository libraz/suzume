"""Counter, compound, family-term, and honorific tables that define search-unit boundaries."""

# Counter/unit patterns (unused - kept for reference)
COUNTER_UNITS: list[str] = [
    "人",
    "個",
    "本",
    "枚",
    "台",
    "回",
    "件",
    "円",
    "年",
    "月",
    "日",
    "時",
    "分",
    "秒",
    "階",
    "番",
    "号",
    "歳",
    "才",
    "目",
    "ページ",
    "頁",
    "時間",
    "分間",
    "秒間",
    "年間",
    "日間",
    "週間",
    "か月",
    "ヶ月",
    "カ月",
]


# Productive kana quantity readings owned by Suzume's finite L1 classes.  The
# stems are NounNumber entries and the tails are quantitative Suffix entries;
# their Cartesian product is closed and can therefore repair MeCab's arbitrary
# syllable splits without enumerating open-class nouns.
KANA_NUMBER_STEMS: frozenset[str] = frozenset({"いち", "ふた", "よん"})


KANA_COUNTER_SUFFIXES: frozenset[str] = frozenset({"まい", "にん", "月"})


# Closed-class construction/composition suffixes that remain independent search
# units after a numeral+counter phrase (二階|建て, 二本|立て).
QUANTITY_BOUND_SUFFIXES: frozenset[str] = frozenset({"建て", "立て"})


# One-kanji general suffixes that stay their own token after a noun: the
# relational and purpose suffixes (業務|用, 期間|内, 使用|後) the core lists as
# bound suffixes. Every other general suffix forms one search unit with its
# host (改正案, 交通費, 説明会).
KANJI_SUFFIXES_KEPT_SEPARATE: frozenset[str] = frozenset("用内中後間視名目方半強弱")


# Letter openers and closers (拝啓, 前略, 敬具 ...) are formulaic words that stand
# outside the sentence they frame, so they never join a kanji compound.
LETTER_FORMULAS: frozenset[str] = frozenset(("拝啓", "謹啓", "前略", "草々", "敬具", "追伸"))


# The address closing 御中 stands after the addressee like 様, so it never joins the
# name before it (営業部|御中); unlike the formulas above it is a noun.
ADDRESS_CLOSING_NOUNS: frozenset[str] = frozenset(("御中",))


# The regional causal conjunctive particle き is homographic with the classical
# past auxiliary, and the reference dictionary only knows the latter. The two
# are told apart by what they attach to: the classical auxiliary takes a
# continuative (あり+き), while the causal particle follows a finite predicate
# — a terminal-form verb or the past auxiliary (書く+き, 飲ん+だ+き).
CLASSICAL_KI_CONJ_TYPE: str = "文語・キ"


CLASSICAL_KERI_CONJ_TYPE: str = "文語・ケリ"


FINITE_PREDECESSOR_CONJ_FORM: str = "基本形"


# Regional request forms of the benefactive くれる, mapped to the dictionary
# form Suzume gives them. Each is homographic with an unrelated verb the
# reference dictionary does know, so only a preceding te-form selects them.
BENEFACTIVE_REQUEST_LEMMAS: dict[str, str] = {"おくれ": "おくれる", "けろ": "けろ"}


# Greetings are one interjection (おつかれ, not お + the verb 疲れる). The
# reference lists only some of them and cuts the rest into a prefix plus a
# verb or a filler plus a verb.
GREETING_INTERJECTIONS: tuple[str, ...] = ("おつかれさま", "おやすみなさい", "おつかれ", "あざす")


# Hiragana compounds that MeCab splits but should stay together
HIRAGANA_COMPOUNDS: dict[str, str] = {
    "ふともも": "名詞",
    "おもち": "名詞",
    "おかし": "名詞",
    "おととい": "名詞",
    "たまご": "名詞",
    "ひこうき": "名詞",
    "みっつ": "名詞",
    "よっつ": "名詞",
    "ふとん": "名詞",
    # Coinages stay whole: neither piece of the reference's split carries the
    # meaning.
    "うれぴい": "形容詞",
    "うれぴ": "名詞",
}


# Closed function words and fixed formal/search units whose internal split in
# the reference analyzer is not a Suzume morpheme boundary.  Open-class words
# do not belong here; this table is intentionally limited to finite lexical
# classes that can be normalized without inspecting the current Suzume output.
FIXED_FUNCTION_SEARCH_UNITS: dict[str, str] = {
    "然程": "副詞",
    "更に": "副詞",
    # 特 and 常 have no adverbial use of their own, so the に is part of the word.
    "特に": "副詞",
    "常に": "副詞",
    # Lexicalized adverbs whose stem never stands alone in this sense. 実 is
    # the fruit or the truth, not the degree, and しも is an archaic particle
    # with no independent use left.
    "実に": "副詞",
    "折しも": "副詞",
    "更なる": "連体詞",
    "どのみち": "副詞",
    "ふいに": "副詞",
    "ほどなく": "副詞",
    "そんなら": "接続詞",
    "ありさま": "名詞",
    "おそれ": "名詞",
    "おかげ": "名詞",
    "おのれ": "代名詞",
    # Literary interrogative pronoun; the analyzer has no entry and reads い+ず+こ.
    "いずこ": "代名詞",
    "だけ": "助詞",
    "だに": "助詞",
    # 即時の接続助詞. The analyzer reads its middle mora as the noun 否, which
    # never stands alone here; the compound has no internal boundary.
    "や否や": "助詞",
    "がてら": "助詞",
    # Registered as one compound case particle in the tokenizer's L1
    # (src/dictionary/entries/compound_particles.cpp), so the oracle keeps it whole too.
    "にわたる": "助詞",
    # Regional predicate tails. The reference analyzer has no entry for them and
    # guesses an internal boundary (だ+べ, やん+け), but each is one closed
    # copular or final form.
    "だべ": "助動詞",
    "けん": "助詞",
    "やんけ": "助詞",
    "やねん": "助動詞",
    "だっちゃ": "助動詞",
    # Lexicalized compound particles the reference analyzer keeps whole after a
    # noun but takes apart after a verb attributive (確認に際して vs
    # 確認するに際して). The unit does not change with what precedes it, and the
    # continuative member is the one that carries the polite form (に際し+まし+て).
    "に際して": "助詞",
    "に際し": "助詞",
    "につき": "助詞",
}


# Closed inflected function forms that the reference analyzer can split into
# pieces before a following auxiliary.  The follower gate prevents consuming
# the same prefix from a longer finite lexical form.
FIXED_INFLECTED_FUNCTION_UNITS: dict[str, tuple[str, str, tuple[str, ...]]] = {
    "いただけ": ("動詞", "いただける", ("ます", "ませ", "ない", "なかっ")),
}


# Closed units that a reference dictionary can absorb into a following noun.
# The normalizer splits only the leading unit and leaves the productive noun
# boundary intact (わが|国, ひととおり|目).
FIXED_LEADING_SEARCH_UNITS: dict[str, str] = {
    "以下": "接尾辞",
    "程度": "接尾辞",
    "ひととおり": "副詞",
    "わが": "連体詞",
}


# Temporal prefixes and the right-hand elements that continue a temporal noun.
# The prefix heads a temporal compound (今週, 今回, 毎時, 今夏), never an arbitrary
# one, so an ordinary noun after it starts a new word: 今紙, 今水, 今大会.
TEMPORAL_PREFIX_KANJI: frozenset[str] = frozenset({"今", "来", "先", "昨", "翌", "毎"})


TEMPORAL_COMPOUND_UNITS: frozenset[str] = frozenset(
    {
        "日",
        "週",
        "月",
        "年",
        "回",
        "朝",
        "晩",
        "夜",
        "度",
        "期",
        "時",
        "分",
        "秒",
        "春",
        "夏",
        "秋",
        "冬",
        "宵",
        "前",
        "後",
        "中",
        "末",
    }
)


# Productive second elements that derive an i-adjective from a nominal host
# (めんどくさい, うそくさい, 素人くさい, 汗臭い). The derivation is not a predication over
# the host, so the two are one search unit. The reference dictionary lists a few
# of these compounds (面倒くさい, 古くさい, ばかくさい) and leaves the rest split,
# which is a lexical gap in its lexicon rather than a morpheme boundary.
DERIVED_ADJECTIVE_SUFFIX_LEMMAS: frozenset[str] = frozenset({"くさい", "臭い"})


# Search-unit compounds: kanji+okurigana words MeCab splits but Suzume keeps as one token
SEARCH_UNIT_COMPOUNDS: dict[str, str] = {}


# Kanji prefix compounds: MeCab splits kanji prefix (接頭詞) + kana-containing noun/verb.
# Maps prefix kanji → set of following token surfaces that form a valid compound.
# Used to merge 微+笑み → 微笑み, 微+笑む → 微笑む, etc.
KANJI_PREFIX_COMPOUNDS: dict[str, set[str]] = {
    "微": {"笑み", "笑む", "笑ん", "笑え", "笑っ", "笑わ", "笑い"},
}


# Family/honorific lexemes used both with and without an お prefix
_HONORIFIC_FAMILY_TERMS: set[str] = {
    "兄ちゃん",
    "姉ちゃん",
    "兄さん",
    "姉さん",
    "嬢さん",
    "嬢様",
    "父さん",
    "母さん",
    "爺さん",
    "婆さん",
    "嫁さん",
    "客様",
    "客さん",
}


# Colloquial tails that only become family terms after adding お.
_COLLOQUIAL_FAMILY_TAILS: set[str] = {
    "じさん",
    "ばさん",
    "じいさん",
    "ばあさん",
    "にいさん",
    "ねえさん",
    "とうさん",
    "かあさん",
    "もちゃ",
    "っさん",
}


# Family/honorific terms that merge with お prefix.
FAMILY_TERMS: set[str] = _HONORIFIC_FAMILY_TERMS | _COLLOQUIAL_FAMILY_TAILS


_PREFIXED_FAMILY_TERMS = {f"お{term}" for term in FAMILY_TERMS}


# Godan rows used to spell out a derivational suffix's paradigm. The く row
# carries the onbin continuative い alongside its regular forms.
_GODAN_ROWS: dict[str, str] = {
    "く": "かきくけこい",
    "す": "さしすせそ",
}


def _godan_suffix_forms(lemma: str) -> set[str]:
    """Every inflected surface of a godan derivational suffix."""
    stem, ending = lemma[:-1], lemma[-1]
    return {stem + kana for kana in _GODAN_ROWS[ending]}


# Verb-forming derivational suffixes. A noun takes them freely (春めく, 謎めく,
# 冗談めかす) and the host and the suffix stay separate search units. The
# reference dictionary sometimes cuts the suffix itself into fragments, so its
# paradigm is spelled out to find it again.
DERIVED_VERB_SUFFIX_FORMS: dict[str, str] = {
    form: lemma for lemma in ("めく", "めかす") for form in _godan_suffix_forms(lemma)
}


# Lemmas of the verb-forming derivational suffixes. The reference holds some
# host + suffix verbs as single headwords (春めく, 大人ぶる); they are split the
# same way as the productive rest.
DERIVED_VERB_SUFFIX_LEMMAS: tuple[str, ...] = ("めかす", "めく", "ぶる")


# Headwords whose host no longer carries its own meaning inside the verb
# (時めく is to throb or to prosper, not to seem like a time), so they stay one
# word. Hiragana hosts (ときめく, きらめく) are mimetic roots and never split.
LEXICALIZED_DERIVED_VERBS: frozenset[str] = frozenset({"時めく", "色めく", "荒ぶる"})


# How many tokens a cut-up derived verb can be spread over. The suffix itself is
# at most three morae, and the fragment carrying its tail is one more token.
DERIVED_VERB_FRAGMENT_SPAN: int = 3


# Noun-forming state suffixes. Nothing else ends in these, so a token carrying
# one always has the suffix boundary inside it (泥/まみれ, 開け/っぱなし).
STATE_NOUN_SUFFIXES: tuple[str, ...] = ("まみれ", "っぱなし")


# Colloquial pronouns to merge
COLLOQUIAL_PRONOUNS: list[str] = ["どいつ", "こいつ", "そいつ", "あいつ"]


# Honorific suffixes restored from inside a single reference token. 君 and 殿
# are honorifics too, but the analyzer already separates them after a name or
# title (佐藤+君, 先生+殿); a single token ending in either is a lexical compound
# (主君, 暴君, 貴殿, 御殿, 宮殿) that must stay whole.
HONORIFIC_SUFFIXES: list[str] = ["さん", "ちゃん", "様", "さま"]


# Words where honorific suffix is part of the lexeme
HONORIFIC_EXCEPTIONS: set[str] = (
    _HONORIFIC_FAMILY_TERMS
    | {f"お{term}" for term in _HONORIFIC_FAMILY_TERMS}
    | {
        "皆様",
        "皆さん",
    }
)


# Predicate tails that close the humble/honorific frame お/ご + verb stem + tail.
# The frame is what makes a kana-only stem (おかけする) a separable prefix plus
# a verb stem rather than one lexeme.
HONORIFIC_FRAME_TAILS: set[str] = {
    "する",
    "し",
    "しろ",
    "せよ",
    "され",
    "いたし",
    "いたす",
    "ください",
    "くださる",
    "くださっ",
    "なさる",
    "なさっ",
    "なさい",
}


# Words where お/ご/御 is part of the lexeme (not separable prefix): the
# remainder does not keep the word's meaning on its own.
# Only kanji-bearing lexemes need listing: an all-hiragana remainder separates
# solely inside the honorific frame above.
PREFIX_EXCEPTIONS: set[str] = _PREFIXED_FAMILY_TERMS | {
    "お出で",
    "お疲れ様",
    "お金",
    "お前",
    "ご免",
    "御免",
    "御座",
    "御殿",
    "御所",
    "御中",
    "御曹司",
}


# User-dict registered kanji+katakana compounds (skip splitting)
USER_DICT_COMPOUNDS: set[str] = {"東京テスト"}


# Unicode currency, unit, and legal-mark symbols are meaningful input rather
# than punctuation. IPADIC has no entries for many of them and calls them
# 記号, which would otherwise make the oracle's symbol filter discard them.
TEXT_SYMBOLS: frozenset[str] = frozenset("￥€＄$℃°№℡§±™©")
