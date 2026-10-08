"""Slang stems, word-level exception tables, and particle corrections for MeCab preprocessing."""

# Slang adjective stems -> standard replacement for MeCab preprocessing.
# The class is open and is normally found in the analysis rather than listed
# (see _stranded_adjective_stems). What stays here is the hiragana spellings,
# where the analysis breaks up so far that no two-mora nominal is left to find:
# うざかった comes back as う + ざかった and きもい as き + も + い, so the stem
# has to be named before the text is analyzed at all.
# A stem is only substituted where the untouched analysis leaves it unresolved
# (see _accept_slang_match), which is what keeps the kana spellings from firing
# inside ordinary words such as ください / 聞いたか / 答えも.
SLANG_ADJ_STEMS: dict[str, str] = {
    "えも": "赤",
    "きも": "赤",
    "うざ": "赤",
    "ださ": "赤",
    "いた": "赤",
    "やば": "赤",
}


# What may follow a slang adjective stem: an inflection kana, or the stem-taking
# excess/appearance suffixes (うざすぎ, うざそう) that attach to the bare stem.
SLANG_ADJ_FOLLOWER = r"(?:[いかくけさ]|すぎ|そう)"


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
#
# A stem ending in 然 is derived from its shape, and a reduplicated stem MeCab
# already reads as an adverb is derived from that tag, so neither needs a line
# here. What is left is the reduplicated stems MeCab has no adverbial reading
# for: 煌々 is listed only as a plain noun, and 黒々 is not one token at all.
TARI_ADVERB_STEMS: list[str] = [
    "煌々",
    "黒々",
    "堂々",
    "悠々",
    "淡々",
    "粛々",
    "颯爽",
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
    "やる",
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


PRODUCTIVE_COMPOUND_V2: frozenset[str] = frozenset(COMPOUND_VERB_V2_GODAN + COMPOUND_VERB_V2_ICHIDAN)


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
    "おつかれさま": "お願い",
    "日付け": "日付",
    "再確認": "確認",
    "ますっっ": "ます",
    "ようけ": "たくさん",
    # Spellings the reference lacks, read through a known word of the same
    # inflection class so every cell follows: ぶっちゃけ is the continuative of
    # the ichidan ぶっちゃける, 頑 the stem of the na-adjective 頑な.
    "ぶっちゃけ": "打ち明け",
    "頑な": "静か",
}


# A lexical replacement is safe only at a word boundary. These followers extend
# the exception's surface into an inflected word, so preprocessing must leave
# the raw verb or quotative sequence available to MeCab instead.
WORD_EXCEPTION_BLOCKED_FOLLOWERS: dict[str, tuple[str, ...]] = {
    "打ち合わせ": ("る", "た", "て", "ます", "まし", "ない", "なかっ", "ず", "ぬ", "ん", "れ", "ろ", "よう", "ば"),
    # The volitional よう before けど/けれど/けん spells the same kana.
    "ようけ": ("ど", "れ", "ん"),
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


# Role-language and regional sentence-final particles the reference dictionary
# lacks. Each closes a predicate as one particle (走る+もふ, 走る+っぴ), but
# the dictionary invents words for the kana and can pull the predicate's last
# mora into them.
CHARACTER_SPEECH_FINAL_PARTICLES: tuple[str, ...] = (
    "ぴょん",
    "ピョン",
    "ぞい",
    "のう",
    "もふ",
    "わん",
    "っぴ",
)


# Role-language copulas the reference splits into homographic pieces (ざま+す,
# やん+す, っ+ス), mapped to the dictionary form the tokenizer's L1 gives them.
# A connective で in front is the copula's own continuative and stays a token
# of its own (で+やんす).
CHARACTER_SPEECH_COPULAS: dict[str, str] = {
    "ござんす": "ござる",
    "ざます": "ざます",
    "ざんす": "ある",
    "やんす": "やんす",
    "っス": "です",
}


# Regional sentence-final particles. They are absent from the reference
# dictionary, so they surface as a bare noun or an interjection and are told
# apart from those only by the predicate in front of them.
DIALECT_FINAL_PARTICLES: frozenset[str] = frozenset({"ばい", "え"})
