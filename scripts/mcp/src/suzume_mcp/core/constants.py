"""Constants ported from SuzumeUtils.pm."""

# MeCab POS to Suzume POS mapping (raw mapping from MeCab)
POS_MAP: dict[str, str] = {
    "名詞": "Noun",
    "動詞": "Verb",
    "形容詞": "Adjective",
    "副詞": "Adverb",
    "助詞": "Particle",
    "助動詞": "Auxiliary",
    "接続詞": "Conjunction",
    "感動詞": "Interjection",
    "連体詞": "Adnominal",
    "接頭詞": "Prefix",
    "接尾辞": "Suffix",
    "代名詞": "Pronoun",
    "記号": "Symbol",
    "フィラー": "Filler",
    "その他": "Other",
}

# ナイ形容詞 (adjectives ending in ない that are single lexical units)
NAI_ADJECTIVES: list[str] = [
    "だらしない",
    "つまらない",
    "しょうがない",
    "もったいない",
    "くだらない",
    "せわしない",
    "やるせない",
    "いたたまれない",
    "あどけない",
    "おぼつかない",
    "はしたない",
    "みっともない",
    "ろくでもない",
    "どうしようもない",
    "ものたりない",
    "こころもとない",
]

# Lexical adjectives that MeCab merges even though Suzume preserves the
# productive nominal + independent ない boundary.
NOUN_NAI_COMPOUND_ADJECTIVES: list[str] = [
    "揺るぎない",
]

# Colloquial causatives that IPADIC stores as independent Godan-す headwords,
# despite their productive a-row host plus causative-す boundary.
LEXICALIZED_CAUSATIVE_SU_LEMMAS: frozenset[str] = frozenset({"待たす", "行かす"})

# Classical volitional auxiliary followed by a quotative particle.  MeCab
# sometimes treats the closed-class sequence as one noun token; Suzume keeps
# both grammatical search units independent.
LITERARY_VOLITIONAL_PARTICLE_COMPOUNDS: dict[str, tuple[str, str]] = {
    "むと": ("む", "と"),
}

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

# Slang adjective stems -> standard replacement for MeCab preprocessing.
# Both spellings of every stem are listed: the reference dictionary knows none
# of them, so whichever spelling is missing here breaks into kana fragments.
# A stem is only substituted where the untouched analysis leaves it unresolved
# (see _accept_slang_match), which is what keeps the kana spellings from firing
# inside ordinary words such as ください / 聞いたか / 答えも.
SLANG_ADJ_STEMS: dict[str, str] = {
    "エモ": "赤",
    "えも": "赤",
    "キモ": "赤",
    "きも": "赤",
    "ウザ": "赤",
    "うざ": "赤",
    "ダサ": "赤",
    "ださ": "赤",
    "イタ": "赤",
    "いた": "赤",
    "ヤバ": "赤",
    "やば": "赤",
}

# Slang verb stems -> standard replacement for MeCab preprocessing
SLANG_VERB_STEMS: dict[str, str] = {
    "バズ": "走",
    "ばず": "走",
    "ググ": "走",
    "ぐぐ": "走",
    "パク": "走",
    "ぱく": "走",
}

# タリ活用副詞: stem + と -> Adverb
TARI_ADVERB_STEMS: list[str] = [
    "泰然",
    "堂々",
    "悠々",
    "淡々",
    "粛々",
    "颯爽",
    "毅然",
    "漫然",
    "茫然",
    "呆然",
    "唖然",
    "愕然",
    "断然",
    "俄然",
    "歴然",
    "整然",
    "雑然",
    "騒然",
    "憮然",
    "黙然",
    "昂然",
    "凛然",
    "厳然",
]

# Compound verb subsidiary verbs (V2)
#
# Mirrors the core's closed V2 lexicon (src/analysis/join_compound_verb_lexicon.cpp).
# An expectation must not split a compound the tokenizer joins, so every V2 the
# core can join appears here; scripts/check_compound_v2_sync.py fails when the two
# drift apart. Two hiragana readings are held back because they collide with a
# productive contraction or a grammatical homograph rather than naming a lexical
# V2 here: 解く's とく, which the core lexicon itself excludes; 取る's とる,
# which is the progressive 〜ている in 話しとる; and 張る's はる, which is the
# honorific auxiliary after a continuative. All three compounds still merge in
# their kanji spelling.
COMPOUND_VERB_V2_GODAN: list[str] = [
    "込む",
    "こむ",
    "出す",
    "だす",
    "続く",
    "つづく",
    "返す",
    "かえす",
    "戻す",
    "もどす",
    "返る",
    "かえる",
    "帰る",
    "変わる",
    "かわる",
    "替わる",
    "つかる",
    "合う",
    "あう",
    "扱う",
    "あつかう",
    "運ぶ",
    "はこぶ",
    "過ごす",
    "すごす",
    "消す",
    "けす",
    "直す",
    "なおす",
    "切る",
    "きる",
    "上がる",
    "あがる",
    "下がる",
    "さがる",
    "回す",
    "まわす",
    "回る",
    "まわる",
    "抜く",
    "ぬく",
    "掛かる",
    "かかる",
    "付く",
    "つく",
    "当たる",
    "あたる",
    "巡る",
    "めぐる",
    "飛ばす",
    "とばす",
    "交う",
    "かう",
    "潰す",
    "つぶす",
    "崩す",
    "くずす",
    "倒す",
    "たおす",
    "壊す",
    "こわす",
    "砕く",
    "くだく",
    "盛る",
    "さかる",
    "起こす",
    "おこす",
    "去る",
    "さる",
    "開く",
    "ひらく",
    "組む",
    "くむ",
    "上る",
    "のぼる",
    "こもる",
    "違う",
    "ちがう",
    "外す",
    "はずす",
    "計らう",
    "はからう",
    "悩む",
    "なやむ",
    "知る",
    "しる",
    "立つ",
    "たつ",
    "通す",
    "とおす",
    "持つ",
    "もつ",
    "流す",
    "ながす",
    "記す",
    "しるす",
    "巻く",
    "まく",
    "会う",
    "寄る",
    "よる",
    "迫る",
    "せまる",
    "延ばす",
    "のばす",
    "離す",
    "はなす",
    "渡す",
    "わたす",
    "表す",
    "あらわす",
    "残す",
    "のこす",
    "解く",
    "募る",
    "つのる",
    "ふける",
    "敷く",
    "しく",
    "払う",
    "はらう",
    "失う",
    "うしなう",
    "破る",
    "やぶる",
    "下ろす",
    "おろす",
    "送る",
    "おくる",
    "放す",
    "及ぶ",
    "およぶ",
    "かじる",
    "漏らす",
    "もらす",
    "写す",
    "うつす",
    "散らす",
    "ちらす",
    "囲む",
    "かこむ",
    "締まる",
    "しまる",
    "仕切る",
    "しきる",
    "次ぐ",
    "つぐ",
    "除く",
    "のぞく",
    "移る",
    "うつる",
    "散る",
    "ちる",
    "退く",
    "のく",
    "着く",
    "取る",
    "越す",
    "こす",
    "張る",
    "叫ぶ",
    "さけぶ",
    "注ぐ",
    "そそぐ",
    "継ぐ",
    "挟む",
    "はさむ",
    "招く",
    "まねく",
    "歩く",
    "あるく",
    "ほどく",
    "向く",
    "むく",
    "描く",
    "えがく",
    "誤る",
    "あやまる",
    "尽くす",
    "つくす",
    "聞かす",
    "きかす",
    "引く",
    "ひく",
    "向かう",
    "むかう",
    "並ぶ",
    "ならぶ",
    "果たす",
    "はたす",
    "こなす",
    "刺す",
    "さす",
    "望む",
    "のぞむ",
    "落とす",
    "おとす",
    "戻る",
    "もどる",
    "入る",
    "いる",
    "止まる",
    "とまる",
    "そこなう",
    "渡る",
    "わたる",
    "かざす",
    "置く",
    "おく",
    "足す",
    "たす",
    "直る",
    "なおる",
    "下す",
    "くだす",
    "交わす",
    "かわす",
    "添う",
    "そう",
    "混じる",
    "まじる",
    "籠る",
    "籠もる",
    "鳴らす",
    "ならす",
    "惜しむ",
    "おしむ",
]

COMPOUND_VERB_V2_ICHIDAN: list[str] = [
    "続ける",
    "つづける",
    "はてる",
    "まとめる",
    "つける",
    "替える",
    "かえる",
    "換える",
    "合わせる",
    "あわせる",
    "浮かべる",
    "うかべる",
    "切れる",
    "きれる",
    "間違える",
    "まちがえる",
    "出る",
    "でる",
    "上げる",
    "あげる",
    "下げる",
    "さげる",
    "抜ける",
    "ぬける",
    "越える",
    "こえる",
    "落ちる",
    "おちる",
    "掛ける",
    "かける",
    "がける",
    "付ける",
    "当てる",
    "あてる",
    "向ける",
    "むける",
    "遂げる",
    "とげる",
    "入れる",
    "いれる",
    "分ける",
    "わける",
    "立てる",
    "たてる",
    "重ねる",
    "かさねる",
    "広げる",
    "ひろげる",
    "支える",
    "ささえる",
    "受ける",
    "うける",
    "降りる",
    "おりる",
    "締める",
    "しめる",
    "止める",
    "とめる",
    "留める",
    "寄せる",
    "よせる",
    "伸べる",
    "のべる",
    "控える",
    "ひかえる",
    "逃れる",
    "のがれる",
    "聞かせる",
    "きかせる",
    "伏せる",
    "ふせる",
    "混ぜる",
    "まぜる",
    "詰める",
    "つめる",
    "求める",
    "もとめる",
    "捨てる",
    "すてる",
    "届ける",
    "とどける",
    "添える",
    "そえる",
    "揃える",
    "そろえる",
    "押さえる",
    "おさえる",
    "調べる",
    "しらべる",
    "違える",
    "ちがえる",
    "退ける",
    "のける",
    "遅れる",
    "おくれる",
    "忘れる",
    "わすれる",
    "起きる",
    "おきる",
    "下りる",
    "損じる",
    "そんじる",
    "乱れる",
    "みだれる",
]

# A Sahen continuative し joins most of the V2s above (確認し続ける) but not these,
# and そこなう joins that continuative only (確認しそこなう, not 読みそこなう). Both
# restrictions carry the same values as the core lexicon's own joining flags.
COMPOUND_VERB_V2_NOT_AFTER_SURU: frozenset[str] = frozenset(
    {
        "解く",
        "間違える",
        "まちがえる",
        "忘れる",
        "わすれる",
        "立つ",
        "たつ",
    }
)

COMPOUND_VERB_V2_SURU_ONLY: frozenset[str] = frozenset({"そこなう"})

# Fictional/unusual proper nouns -> standard name for MeCab preprocessing
UNUSUAL_NAMES: dict[str, str] = {
    "がお": "吉田",
}

# Words that MeCab incorrectly splits but should stay together
WORD_EXCEPTIONS: dict[str, str] = {
    "小供": "供給",
    "とうきょう": "瑠璃",
    "どさり": "ゆっくり",
    "にゃー": "ねえ",
    "打ち合わせ": "会議",
    "おいで": "お出で",
    "ほんわか": "ゆっくり",
    "ありきたり": "当たり前",
    "ばたり": "ゆっくり",
    "がたり": "ゆっくり",
    "すごいいいい": "すごい",
    "すごーーい": "すごい",
    "かわいーー": "かわいい",
    "もうってば": "もう",
    "あなたったら": "あなた",
    "無意識": "意識",
    "翌営業日": "明日",
    "お疲れ様": "お願い",
    "日付け": "日付",
    "再確認": "確認",
    "ですっ": "です",
    "ますっっ": "ます",
}

# A lexical replacement is safe only at a word boundary. These followers extend
# the exception's surface into an inflected word, so preprocessing must leave
# the raw verb or quotative sequence available to MeCab instead.
WORD_EXCEPTION_BLOCKED_FOLLOWERS: dict[str, tuple[str, ...]] = {
    "打ち合わせ": ("る", "た", "て", "ます", "まし", "ない", "なかっ", "ず", "ぬ", "ん", "れ", "ろ", "よう", "ば"),
    "ですっ": ("て",),
}

# Particles that MeCab may misclassify as Noun
PARTICLE_CORRECTIONS: dict[str, str] = {
    "の": "Particle",
    "が": "Particle",
    "を": "Particle",
    "に": "Particle",
    "へ": "Particle",
    "で": "Particle",
    "と": "Particle",
    "から": "Particle",
    "まで": "Particle",
    "より": "Particle",
    "ほど": "Particle",
    "は": "Particle",
    "も": "Particle",
    "か": "Particle",
    "な": "Particle",
    "ね": "Particle",
    "よ": "Particle",
    "わ": "Particle",
    "ぞ": "Particle",
    "さ": "Particle",
    "けど": "Particle",
    "けれど": "Particle",
    "し": "Particle",
    "のに": "Particle",
    "ので": "Particle",
    "ながら": "Particle",
    "ばかり": "Particle",
    "だけ": "Particle",
    "しか": "Particle",
    "くらい": "Particle",
    "ぐらい": "Particle",
    "など": "Particle",
    "なんか": "Particle",
    "なんて": "Particle",
    "って": "Particle",
}

# Regional sentence-final particles. They are absent from the reference
# dictionary, so they surface as a bare noun or an interjection and are told
# apart from those only by the predicate in front of them.
DIALECT_FINAL_PARTICLES: frozenset[str] = frozenset({"ばい", "え"})

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
}

# Closed function words and fixed formal/search units whose internal split in
# the reference analyzer is not a Suzume morpheme boundary.  Open-class words
# do not belong here; this table is intentionally limited to finite lexical
# classes that can be normalized without inspecting the current Suzume output.
FIXED_FUNCTION_SEARCH_UNITS: dict[str, str] = {
    "然程": "副詞",
    "更に": "副詞",
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
# (めんどくさい, うそくさい, 素人くさい). The derivation is not a predication over
# the host, so the two are one search unit. The reference dictionary lists a few
# of these compounds (面倒くさい, 古くさい, ばかくさい) and leaves the rest split,
# which is a lexical gap in its lexicon rather than a morpheme boundary.
DERIVED_ADJECTIVE_SUFFIX_LEMMAS: frozenset[str] = frozenset({"くさい"})

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
# 冗談めかす), but the reference dictionary holds only the lexicalized results
# as single tokens and splits the rest, so the boundary has to be restored from
# the paradigm rather than from a word list.
DERIVED_VERB_SUFFIX_FORMS: dict[str, str] = {
    form: lemma for lemma in ("めく", "めかす") for form in _godan_suffix_forms(lemma)
}

# Noun-forming state suffixes. Nothing else ends in these, so a token carrying
# one always has the suffix boundary inside it (泥/まみれ, 開け/っぱなし).
STATE_NOUN_SUFFIXES: tuple[str, ...] = ("まみれ", "っぱなし")

# Colloquial pronouns to merge
COLLOQUIAL_PRONOUNS: list[str] = ["どいつ", "こいつ", "そいつ", "あいつ"]

# Honorific suffixes regex pattern
HONORIFIC_SUFFIXES: list[str] = ["さん", "ちゃん", "様", "君", "殿", "さま"]

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

# Words where お/ご is part of the lexeme (not separable prefix).
# Only kanji-bearing lexemes need listing: an all-hiragana remainder separates
# solely inside the honorific frame above.
PREFIX_EXCEPTIONS: set[str] = _PREFIXED_FAMILY_TERMS | {
    "お出で",
    "お疲れ様",
    "お金",
    "お前",
}

# User-dict registered kanji+katakana compounds (skip splitting)
USER_DICT_COMPOUNDS: set[str] = {"東京テスト"}

# Unicode currency, unit, and legal-mark symbols are meaningful input rather
# than punctuation. IPADIC has no entries for many of them and calls them
# 記号, which would otherwise make the oracle's symbol filter discard them.
TEXT_SYMBOLS: frozenset[str] = frozenset("￥€＄$℃°№℡§±™©")

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

# Emphatic sokuon patterns for preprocessing
EMPHATIC_SOKUON: dict[str, str] = {
    "行くっ": "行く",
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
ADVERBIAL_NA_ADJECTIVES: frozenset[str] = frozenset({"大変", "たいへん", "もっとも"})

# Words to keep as Noun despite 形容動詞語幹 classification
KEEP_AS_NOUN_NOT_ADJ: set[str] = {
    "マジ",
    "不安",
    "不要",
    "乙",
    "不便",
    "公式",
    "可能",
    "容易",
    "積極",
    "健康",
    "傍若無人",
}

# Noun -> Pronoun overrides
NOUN_AS_PRONOUN: set[str] = {"彼氏", "彼女", "奴", "我", "わし"}

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
    "くださる",
    "下さる",
    "いたす",
    # Same humble verb as いたす, only written in kanji. The reference analyzer
    # tags one as a verb and the other as an auxiliary purely because of its own
    # lexicon, which would make the oracle contradict itself for 確認いたします
    # and 確認致します.
    "致す",
    "頂く",
    "あげる",
    "くれる",
    "もらう",
    "始める",
    "続ける",
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

# Plural suffix split targets
PLURAL_RA_STEMS: list[str] = ["彼女", "彼", "僕", "奴", "我"]

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

# Kanji codepoint ranges, mirroring kana::isKanjiCodepoint in
# src/core/kana_constants.h. The supplementary-plane ranges matter here:
# IPADIC has no entry for those characters and MeCab labels them 記号,
# which would otherwise send them through the symbol filter.
KANJI_RANGES: tuple[tuple[int, int], ...] = (
    (0x4E00, 0x9FFF),  # CJK Unified Ideographs
    (0x3400, 0x4DBF),  # CJK Extension A
    (0x20000, 0x2A6DF),  # CJK Extension B
    (0x2A700, 0x2B73F),  # CJK Extension C
    (0x2B740, 0x2B81F),  # CJK Extension D
    (0x2B820, 0x2CEAF),  # CJK Extension E
    (0x2CEB0, 0x2EBEF),  # CJK Extension F
    (0x2EBF0, 0x2EE5F),  # CJK Extension I
    (0x30000, 0x3134F),  # CJK Extension G
    (0x31350, 0x323AF),  # CJK Extension H
    (0x323B0, 0x3347F),  # CJK Extension J
    (0xF900, 0xFAFF),  # CJK Compatibility Ideographs
    (0x2F800, 0x2FA1F),  # CJK Compatibility Ideographs Supplement
    (0x2F00, 0x2FDF),  # Kangxi Radicals
)


def is_kanji(char: str) -> bool:
    """Whether a single character is a kanji."""
    code = ord(char)
    return any(low <= code <= high for low, high in KANJI_RANGES)


def is_all_kanji(surface: str) -> bool:
    """Whether a surface is non-empty and made entirely of kanji."""
    return bool(surface) and all(is_kanji(char) for char in surface)


# Heads that stand for a copular predicate rather than naming a thing: the
# formal nouns and the nominalizer の. Mirrors the NounFormal entries the
# tokenizer keeps in src/dictionary/entries/formal_nouns.cpp, which is the
# source of truth for the class.
COPULAR_PREDICATE_HEADS = frozenset(
    {
        "の",
        "事",
        "こと",
        "物",
        "もの",
        "もん",
        "為",
        "ため",
        "ところ",
        "どころ",
        "ころ",
        "時",
        "内",
        "末",
        "あいだ",
        "うち",
        "途中",
        "たび",
        "以来",
        "以降",
        "ごろ",
        "どき",
        "通り",
        "とおり",
        "限り",
        "かぎり",
        "付け",
        "当たり",
        "よう",
        "ほう",
        "うえ",
        "わり",
        "くせ",
        "かわり",
        "代わり",
        "ふう",
        "いかん",
        "わけ",
        "すべ",
        "よし",
        "ゆえ",
        "もと",
        "ちがい",
        "違い",
        "せい",
        "おそれ",
        "おかげ",
        "おしまい",
        "はず",
        "場合",
        "つもり",
        "あて",
        "ついで",
        "かたわら",
        "ふり",
        "とたん",
        "そば",
        "否や",
        "あげく",
        "あまり",
        "まま",
        "ほか",
        "他",
        "仕方",
        "しかた",
        "たたずまい",
        "うだつ",
    }
)


# Pre-1946 kanji forms paired with their modern equivalents, as one flat run of
# (old, new) characters.  Consulted only to re-read a character the reference
# dictionary itself returned as unknown, so a form it already holds keeps its
# own entry.
_KYUJITAI_PAIRS = (
    "亞亜惡悪壓圧圍囲醫医爲為飮飲隱隠榮栄營営衞衛驛駅圓円緣縁艷艶應応歐欧毆殴櫻桜奧奥橫横溫温穩穏假仮價価畫画會会繪絵"
    "擴拡殼殻覺覚學学嶽岳樂楽渴渇勸勧卷巻寬寛歡歓罐缶觀観關関陷陥巖巌歸帰氣気僞偽戲戯犧犠舊旧據拠擧挙虛虚峽峡挾挟狹狭"
    "鄕郷曉暁區区驅駆勳勲薰薫徑径惠恵揭掲溪渓經経繼継莖茎螢蛍輕軽藝芸缺欠儉倹劍剣圈圏檢検權権獻献硏研縣県險険顯顕驗験"
    "嚴厳效効廣広恆恒鑛鉱號号國国黑黒濟済碎砕齋斎劑剤雜雑產産慘惨贊賛殘残絲糸齒歯兒児辭辞濕湿實実舍舎寫写釋釈壽寿收収"
    "從従澁渋獸獣縱縦肅粛處処緖緒敍叙將将稱称涉渉燒焼奬奨條条狀状乘乗淨浄剩剰疊畳繩縄壤壌孃嬢讓譲釀醸觸触囑嘱眞真寢寝"
    "愼慎盡尽圖図粹粋醉酔隨随髓髄數数樞枢瀨瀬齊斉靜静攝摂竊窃說説淺浅戰戦纖繊禪禅雙双壯壮爭争莊荘搜捜插挿巢巣曾曽瘦痩"
    "總総藏蔵臟臓卽即屬属續続墮堕體体對対帶帯滯滞臺台瀧滝澤沢擇択單単擔担膽胆團団斷断彈弾遲遅癡痴蟲虫晝昼鑄鋳廳庁徵徴"
    "聽聴敕勅鎭鎮傳伝轉転點点黨党盜盗燈灯當当鬪闘德徳獨独讀読屆届貳弐惱悩腦脳霸覇拜拝廢廃賣売麥麦發発髮髪拔抜晚晩蠻蛮"
    "祕秘濱浜甁瓶拂払佛仏倂併竝並變変邊辺辨弁瓣弁辯弁舖舗步歩峯峰寶宝豐豊沒没飜翻每毎萬万滿満默黙彌弥譯訳藥薬與与譽誉"
    "搖揺樣様謠謡來来賴頼亂乱覽覧龍竜兩両獵猟綠緑淚涙壘塁勵励禮礼隸隷靈霊齡齢曆暦歷歴戀恋鍊錬爐炉勞労樓楼錄録灣湾亙亘"
)
KYUJITAI_TO_SHINJITAI: dict[str, str] = {
    _KYUJITAI_PAIRS[index]: _KYUJITAI_PAIRS[index + 1] for index in range(0, len(_KYUJITAI_PAIRS), 2)
}
