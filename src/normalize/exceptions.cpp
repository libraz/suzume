#include "normalize/exceptions.h"

// Note: Core particle definitions are also in core/kana_constants.h
// (kCaseParticles, kBindingParticles, kFinalParticles, kConjunctiveParticles)
// These sets maintain their own definitions for type compatibility with
// std::unordered_set, while kana_constants.h uses constexpr arrays.

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
    // Pronouns (代名詞) - very common standalone kanji
    "私", "僕", "俺", "君", "彼", "我",
};

// =============================================================================
// Single Hiragana Exceptions
// =============================================================================
// Particles, auxiliaries, and other grammatical elements
// that are valid as single hiragana tokens.
// Note: Case/binding particles overlap with kParticleStrings below.
// See also: kana::kCaseParticles, kana::kBindingParticles in kana_constants.h

const std::unordered_set<std::string_view> kSingleHiraganaExceptions = {
    // Case particles (格助詞) - also in kParticleStrings, kana::kCaseParticles
    "が", "を", "に", "で", "と", "へ", "の",
    // Binding particles (係助詞) - also in kParticleStrings, kana::kBindingParticles
    "は", "も",
    // Final particles (終助詞) - see kana::kFinalParticles
    "か", "な", "ね", "よ", "わ",
    // Auxiliary (助動詞)
    "だ", "た",
    // Conjunctive particles (接続助詞) - see kana::kConjunctiveParticles
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

// =============================================================================
// Particle Strings
// =============================================================================
// Particles that should not be treated as verb endings when generating
// verb candidates from kanji + hiragana patterns.
// Note: Case/binding particles overlap with kSingleHiraganaExceptions above.
// See also: kana::kCaseParticles, kana::kBindingParticles in kana_constants.h

const std::unordered_set<std::string_view> kParticleStrings = {
    // Case particles (格助詞) - also in kSingleHiraganaExceptions, kana::kCaseParticles
    "が", "を", "に", "で", "と", "へ", "の",
    // Binding particles (係助詞) - also in kSingleHiraganaExceptions, kana::kBindingParticles
    "は", "も",
    // Other particles (副助詞・接続助詞)
    "や", "か",
    // Compound particles (複合助詞)
    "から", "まで", "より", "ほど",
};

// =============================================================================
// Copula Strings
// =============================================================================
// Copula and auxiliary verb patterns that should not be treated as verb
// endings when generating verb candidates from kanji + hiragana patterns.

const std::unordered_set<std::string_view> kCopulaStrings = {
    // Basic copula (基本形)
    "だ", "です",
    // Past forms (過去形)
    "だった", "でした",
    // Partial forms (途中形) - for mid-word positions
    "でし",
    // Formal copula (文語形)
    "である",
};

// =============================================================================
// Formal Noun Strings (形式名詞)
// =============================================================================
// Single kanji nouns with abstract grammatical functions.
// These are often used in compound patterns like 所在する, 時間, etc.
// When followed by kanji, they may form compound words.

const std::unordered_set<std::string_view> kFormalNounStrings = {
    "所",  // tokoro - place (所在, 所持)
    "物",  // mono - thing (物事, 物語)
    "事",  // koto - matter (事実, 事件)
    "時",  // toki - time (時間, 時代)
    "方",  // kata/hou - direction/person (方法, 方向)
    "為",  // tame - sake/benefit (為替)
};

// =============================================================================
// Particle Codepoints
// =============================================================================
// Case and binding particles as char32_t codepoints for character-level checks.
// Used to filter strings starting with particles during candidate generation.

const std::unordered_set<char32_t> kParticleCodepoints = {
    // Case particles (格助詞)
    U'が', U'を', U'に', U'で', U'と', U'へ', U'の',
    // Binding particles (係助詞)
    U'は', U'も',
    // Other particles (副助詞)
    U'や', U'か',
};

}  // namespace suzume::normalize
