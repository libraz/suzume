"""Character-class helpers, copular predicate heads, and old-to-new kanji form pairs."""

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


def katakana_to_hiragana(text: str) -> str:
    """Fold katakana ァ..ヶ onto hiragana, leaving every other character as is."""
    return "".join(chr(ord(char) - 0x60) if "ァ" <= char <= "ヶ" else char for char in text)


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


# Frame kanji of a known verb, keyed by the first okurigana kana of its
# conjugation row: a kanji the dictionary has no verb spelling for is probed in
# the frame's place (俟+た as 待+た, 失+く as 無+く, 断+じ as 論+じ).
KANJI_VERB_ROW_FRAMES: dict[str, str] = {
    "た": "待",
    "く": "無",
    "じ": "論",
}
