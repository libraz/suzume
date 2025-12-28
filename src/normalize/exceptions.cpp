#include "normalize/exceptions.h"

namespace suzume::normalize {

// =============================================================================
// Single Kanji Exceptions
// =============================================================================
// Counters, temporal nouns, and other common standalone kanji
// that should not receive single-character penalties.

const std::unordered_set<std::string_view> kSingleKanjiExceptions = {
    // Counters (助数詞)
    "人", "日", "月", "年", "時", "分", "秒", "本", "冊", "個",
    "枚", "台", "回", "件", "円", "点", "度", "番", "階", "歳",
    // Administrative units (行政単位)
    "国", "市", "県", "区", "町", "村",
    // Common standalone nouns (基本名詞)
    "家", "駅", "店", "道", "海", "山", "川", "森", "空", "雨",
    // Spatial relations (空間)
    "上", "下", "中", "外", "内", "前", "後",
    // Directions (方角)
    "東", "西", "南", "北",
    // Seasons (季節)
    "春", "夏", "秋", "冬",
    // Times of day (時間帯)
    "朝", "昼", "夜",
    // Interrogatives (疑問詞)
    "何", "誰",
};

// =============================================================================
// Single Hiragana Exceptions
// =============================================================================
// Particles, auxiliaries, and other grammatical elements
// that are valid as single hiragana tokens.

const std::unordered_set<std::string_view> kSingleHiraganaExceptions = {
    // Case particles (格助詞)
    "が", "を", "に", "で", "と", "へ", "の",
    // Binding particles (係助詞)
    "は", "も",
    // Final particles (終助詞)
    "か", "な", "ね", "よ", "わ",
    // Auxiliary (助動詞)
    "だ", "た",
    // Conjunctive particles (接続助詞)
    "て", "ば",
};

// =============================================================================
// Valid Single Character Verb Stems
// =============================================================================
// These are valid verb stems when followed by たい, etc.
// Primarily Ichidan verbs and irregular verbs.

const std::unordered_set<char32_t> kValidSingleCharVerbStems = {
    // Irregular verbs
    U'し',  // する (suru) - renyokei
    U'来',  // 来る (kuru) - stem (kunyomi: き/こ)
    // Ichidan verbs with single-kanji stems
    U'見',  // 見る (miru)
    U'居',  // 居る (iru)
    U'い',  // いる (iru) - hiragana form
    U'出',  // 出る (deru)
    U'寝',  // 寝る (neru)
    U'得',  // 得る (eru/uru)
    U'経',  // 経る (heru)
    U'着',  // 着る (kiru)
};

// =============================================================================
// Compound Verb Auxiliary First Characters
// =============================================================================
// Kanji that start compound verb auxiliaries.
// Used to detect verb + auxiliary patterns like 読み終わる.

const std::unordered_set<std::string_view> kCompoundVerbAuxFirstChars = {
    "終",  // 終わる (owaru) - to finish
    "始",  // 始める (hajimeru) - to begin
    "過",  // 過ぎる (sugiru) - too much
    "続",  // 続ける (tsuzukeru) - to continue
    "直",  // 直す (naosu) - to redo
    "合",  // 合う (au) - mutual action
    "出",  // 出す (dasu) - to start doing
    "込",  // 込む (komu) - to do thoroughly
    "切",  // 切る (kiru) - to do completely
    "損",  // 損なう (sokonau) - to fail to do
    "返",  // 返す (kaesu) - to do back
    "忘",  // 忘れる (wasureru) - to forget to do
    "残",  // 残す (nokosu) - to leave undone
    "掛",  // 掛ける (kakeru) - to start doing
};

}  // namespace suzume::normalize
