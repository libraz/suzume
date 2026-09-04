#include "char_type.h"

#include <algorithm>
#include <array>

#include "core/kana_constants.h"
#include "utf8.h"

namespace suzume::normalize {

namespace {

// The quantity-related predicates below overlap substantially (for example,
// every temporal counter is also a general counter). Keep their closed classes
// in one packed table so additions cannot make those predicates drift apart.
enum CharProperty : uint16_t {
  kCounter = 1 << 0,
  kDurationSuffix = 1 << 1,
  kTemporalRelationSuffix = 1 << 2,
  kTemporalCounter = 1 << 3,
  kQuantityPrefix = 1 << 4,
  kNumericApproxPrefix = 1 << 5,
  kTemporalSpanSuffix = 1 << 6,
  kDerivationalNounSuffix = 1 << 7,
  kQuantityPhraseSuffix = 1 << 8,
};

struct CharPropertyEntry {
  char32_t codepoint;
  uint16_t properties;
};

constexpr std::array<CharPropertyEntry, 91> kCharProperties = {
    {{U'丁', kCounter},
     {U'万', kCounter},
     {U'世', kCounter},
     {U'両', kCounter},
     {U'中', kDurationSuffix | kTemporalSpanSuffix},
     {U'人', kCounter},
     {U'件', kCounter},
     {U'位', kCounter},
     {U'何', kQuantityPrefix},
     {U'個', kCounter},
     {U'倍', kCounter},
     {U'億', kCounter},
     {U'兆', kCounter},
     {U'円', kCounter},
     {U'冊', kCounter},
     {U'分', kCounter | kDurationSuffix | kTemporalCounter},
     {U'前', kTemporalRelationSuffix},
     {U'割', kCounter},
     {U'勝', kCounter},
     {U'匹', kCounter},
     {U'半', kQuantityPrefix | kQuantityPhraseSuffix},
     {U'口', kCounter},
     {U'台', kCounter},
     {U'号', kCounter},
     {U'名', kCounter},
     {U'問', kCounter},
     {U'回', kCounter},
     {U'基', kCounter},
     {U'巻', kCounter},
     {U'席', kCounter},
     {U'年', kCounter | kTemporalCounter},
     {U'度', kCounter},
     {U'後', kTemporalRelationSuffix},
     {U'性', kDerivationalNounSuffix},
     {U'戦', kCounter},
     {U'戸', kCounter},
     {U'才', kCounter},
     {U'敗', kCounter},
     {U'数', kQuantityPrefix},
     {U'日', kCounter | kTemporalCounter},
     {U'時', kCounter | kTemporalCounter},
     {U'曲', kCounter},
     {U'月', kCounter | kTemporalCounter},
     {U'期', kCounter},
     {U'末', kTemporalSpanSuffix},
     {U'本', kCounter},
     {U'束', kCounter},
     {U'条', kCounter},
     {U'杯', kCounter},
     {U'枚', kCounter},
     {U'棟', kCounter},
     {U'機', kCounter},
     {U'次', kCounter},
     {U'歳', kCounter},
     {U'段', kCounter},
     {U'泊', kCounter},
     {U'点', kCounter},
     {U'版', kCounter},
     {U'番', kCounter},
     {U'畳', kCounter},
     {U'発', kCounter},
     {U'目', kQuantityPhraseSuffix},
     {U'着', kCounter},
     {U'票', kCounter},
     {U'秒', kCounter | kDurationSuffix | kTemporalCounter},
     {U'種', kCounter},
     {U'章', kCounter},
     {U'紀', kCounter},
     {U'約', kNumericApproxPrefix},
     {U'級', kCounter},
     {U'組', kCounter},
     {U'総', kNumericApproxPrefix},
     {U'羽', kCounter},
     {U'者', kDerivationalNounSuffix},
     {U'色', kCounter},
     {U'行', kCounter},
     {U'計', kNumericApproxPrefix},
     {U'話', kCounter},
     {U'足', kCounter},
     {U'軒', kCounter},
     {U'通', kCounter},
     {U'連', kCounter},
     {U'週', kCounter | kTemporalCounter},
     {U'部', kCounter},
     {U'銭', kCounter},
     {U'間', kCounter | kDurationSuffix | kTemporalCounter | kQuantityPhraseSuffix},
     {U'階', kCounter},
     {U'隻', kCounter},
     {U'面', kCounter},
     {U'頭', kCounter},
     {U'食', kCounter}}};

bool hasCharProperty(char32_t codepoint, CharProperty property) {
  size_t first = 0;
  size_t last = kCharProperties.size();
  while (first < last) {
    size_t middle = first + (last - first) / 2;
    if (kCharProperties[middle].codepoint < codepoint) {
      first = middle + 1;
    } else {
      last = middle;
    }
  }
  return first < kCharProperties.size() && kCharProperties[first].codepoint == codepoint &&
         (kCharProperties[first].properties & property) != 0;
}

bool isGreekLetter(char32_t codepoint) {
  return (codepoint >= 0x0370 && codepoint <= 0x0374) || (codepoint >= 0x0376 && codepoint <= 0x0377) ||
         (codepoint >= 0x037A && codepoint <= 0x037D) || codepoint == 0x037F || codepoint == 0x0386 ||
         (codepoint >= 0x0388 && codepoint <= 0x038A) || codepoint == 0x038C ||
         (codepoint >= 0x038E && codepoint <= 0x03A1) || (codepoint >= 0x03A3 && codepoint <= 0x03F5) ||
         (codepoint >= 0x03F7 && codepoint <= 0x03FF);
}

bool isGreekExtendedLetter(char32_t codepoint) {
  return (codepoint >= 0x1F00 && codepoint <= 0x1F15) || (codepoint >= 0x1F18 && codepoint <= 0x1F1D) ||
         (codepoint >= 0x1F20 && codepoint <= 0x1F45) || (codepoint >= 0x1F48 && codepoint <= 0x1F4D) ||
         (codepoint >= 0x1F50 && codepoint <= 0x1F57) || codepoint == 0x1F59 || codepoint == 0x1F5B ||
         codepoint == 0x1F5D || (codepoint >= 0x1F5F && codepoint <= 0x1F7D) ||
         (codepoint >= 0x1F80 && codepoint <= 0x1FB4) || (codepoint >= 0x1FB6 && codepoint <= 0x1FBC) ||
         codepoint == 0x1FBE || (codepoint >= 0x1FC2 && codepoint <= 0x1FC4) ||
         (codepoint >= 0x1FC6 && codepoint <= 0x1FCC) || (codepoint >= 0x1FD0 && codepoint <= 0x1FD3) ||
         (codepoint >= 0x1FD6 && codepoint <= 0x1FDB) || (codepoint >= 0x1FE0 && codepoint <= 0x1FEC) ||
         (codepoint >= 0x1FF2 && codepoint <= 0x1FF4) || (codepoint >= 0x1FF6 && codepoint <= 0x1FFC);
}

bool isUnicodeLetterOrMark(char32_t codepoint) {
  // Suzume does not ship the Unicode character database. Keep the major
  // scripts seen in Japanese text as compact letter/mark ranges, excluding
  // punctuation and digits that share their Unicode blocks.
  return (codepoint >= 0x00C0 && codepoint <= 0x00D6) ||  // Latin-1 letters
         (codepoint >= 0x00D8 && codepoint <= 0x00F6) ||
         (codepoint >= 0x00F8 && codepoint <= 0x02AF) ||  // Latin extensions and IPA
         (codepoint >= 0x0300 && codepoint <= 0x036F) ||  // Combining diacritical marks
         isGreekLetter(codepoint) || (codepoint >= 0x0400 && codepoint <= 0x0481) ||
         (codepoint >= 0x0483 && codepoint <= 0x052F) ||  // Cyrillic letters and marks
         (codepoint >= 0x0E01 && codepoint <= 0x0E3A) ||  // Thai letters and marks
         (codepoint >= 0x0E40 && codepoint <= 0x0E4E) || (codepoint >= 0x1100 && codepoint <= 0x11FF) ||
         (codepoint >= 0x1C80 && codepoint <= 0x1C8F) || isGreekExtendedLetter(codepoint) ||
         (codepoint >= 0x1D00 && codepoint <= 0x1EFF) ||    // Phonetic and Latin extensions
         (codepoint >= 0x2C60 && codepoint <= 0x2C7F) ||    // Latin Extended-C
         (codepoint >= 0x2DE0 && codepoint <= 0x2DFF) ||    // Cyrillic Extended-A
         (codepoint >= 0x3130 && codepoint <= 0x318F) ||    // Hangul compatibility Jamo
         (codepoint >= 0xA640 && codepoint <= 0xA69F) ||    // Cyrillic Extended-B
         (codepoint >= 0xA720 && codepoint <= 0xA7FF) ||    // Latin Extended-D
         (codepoint >= 0xA960 && codepoint <= 0xA97F) ||    // Hangul Jamo Extended-A
         (codepoint >= 0xAB30 && codepoint <= 0xAB6F) ||    // Latin Extended-E
         (codepoint >= 0xAC00 && codepoint <= 0xD7A3) ||    // Hangul syllables
         (codepoint >= 0xD7B0 && codepoint <= 0xD7FF) ||    // Hangul Jamo Extended-B
         (codepoint >= 0x10780 && codepoint <= 0x107BF) ||  // Latin Extended-F
         (codepoint >= 0x1DF00 && codepoint <= 0x1DFFF) ||  // Latin Extended-G
         (codepoint >= 0x1E030 && codepoint <= 0x1E08F);    // Cyrillic Extended-D
}

bool isUnicodeControlOrSpace(char32_t codepoint) {
  return codepoint <= 0x20 || (codepoint >= 0x7F && codepoint <= 0xA0) || codepoint == 0x1680 ||
         (codepoint >= 0x2000 && codepoint <= 0x200A) || (codepoint >= 0x2028 && codepoint <= 0x2029) ||
         codepoint == 0x202F || codepoint == 0x205F || codepoint == 0xFEFF;
}

bool isUnicodePunctuationOrSymbol(char32_t codepoint) {
  return (codepoint >= 0x00A1 && codepoint <= 0x00BF) || codepoint == 0x00D7 || codepoint == 0x00F7 ||
         (codepoint >= 0x200B && codepoint <= 0x206F) ||  // format controls and general punctuation
         (codepoint >= 0x20A0 && codepoint <= 0x20CF) ||  // currency symbols
         (codepoint >= 0x2100 && codepoint <= 0x214F) ||  // letterlike symbols
         (codepoint >= 0x2190 && codepoint <= 0x22FF) ||  // arrows and mathematical operators
         (codepoint >= 0x2300 && codepoint <= 0x24FF) ||  // technical and enclosed symbols
         (codepoint >= 0x2500 && codepoint <= 0x25FF) ||  // box, block, and geometric symbols
         (codepoint >= 0x27C0 && codepoint <= 0x2BFF) ||  // supplemental arrows and mathematical symbols
         (codepoint >= 0x2E00 && codepoint <= 0x2E7F) ||  // supplemental punctuation
         (codepoint >= 0xFF00 && codepoint <= 0xFF20) || (codepoint >= 0xFF3B && codepoint <= 0xFF40) ||
         (codepoint >= 0xFF5B && codepoint <= 0xFF65) || (codepoint >= 0xFFE0 && codepoint <= 0xFFEE) ||
         (codepoint >= 0xFFF0 && codepoint <= 0xFFFF);
}

// These Unicode blocks carry text content (currency, units, technical marks,
// arrows, enclosed characters, and geometric signs).  They are distinct from
// sentence punctuation: callers must retain them as searchable OTHER tokens
// instead of allowing the default symbol filter to erase their offsets.
bool isRetainedTextSymbol(char32_t codepoint) {
  return (codepoint == U'$' || codepoint == 0x00A5 || codepoint == 0x00A7 || codepoint == 0x00A9 ||
          codepoint == 0x00B0 || codepoint == 0x00B1 || codepoint == 0x00D7 || codepoint == 0x00F7) ||
         // Latin-1 text symbols
         (codepoint >= 0x20A0 && codepoint <= 0x20CF) ||  // currency symbols
         (codepoint >= 0x2100 && codepoint <= 0x214F) ||  // letterlike symbols
         (codepoint >= 0x2190 && codepoint <= 0x22FF) ||  // arrows and math
         (codepoint >= 0x2300 && codepoint <= 0x24FF) ||  // technical/enclosed
         (codepoint >= 0x2500 && codepoint <= 0x2BFF) ||  // geometric/dingbats
         (codepoint >= 0xFFE0 && codepoint <= 0xFFEE);    // full-width symbols
}

}  // namespace

CharType classifyChar(char32_t codepoint) {
  // Hiragana: U+3040-U+309F
  if (codepoint >= 0x3040 && codepoint <= 0x309F) {
    return CharType::Hiragana;
  }

  // Katakana: U+30A0-U+30FF, U+31F0-U+31FF (small), U+FF66-U+FF9F (half-width)
  if ((codepoint >= 0x30A0 && codepoint <= 0x30FF) || (codepoint >= 0x31F0 && codepoint <= 0x31FF) ||
      (codepoint >= 0xFF66 && codepoint <= 0xFF9F)) {
    return CharType::Katakana;
  }

  // Ideographic iteration mark (々) - treat as Kanji
  // U+3005 repeats preceding kanji: 人々, 日々, 痛々しい
  // Must check before CJK Symbols range (0x3000-0x303F)
  if (codepoint == 0x3005) {
    return CharType::Kanji;
  }

  // Ideographic closing mark (〆) - treat as Kanji so it remains part of
  // lexical compounds such as 〆切. Unicode assigns U+3006 the Ideographic
  // property even though it lives in the CJK Symbols block.
  if (codepoint == 0x3006) {
    return CharType::Kanji;
  }

  // Ideographic number zero (〇) - treat as Kanji so it joins numeral runs
  // (二〇二五年, 一〇〇). U+3007 lives in the CJK Symbols block, so it must be
  // caught before the symbol range below or it is dropped as a symbol.
  if (codepoint == 0x3007) {
    return CharType::Kanji;
  }

  // CJK Unified Ideographs and extensions (kanji)
  if (isKanjiCodepoint(codepoint)) {
    return CharType::Kanji;
  }

  // ASCII alphabet
  if ((codepoint >= 'A' && codepoint <= 'Z') || (codepoint >= 'a' && codepoint <= 'z')) {
    return CharType::Alphabet;
  }

  // Full-width alphabet
  if ((codepoint >= 0xFF21 && codepoint <= 0xFF3A) || (codepoint >= 0xFF41 && codepoint <= 0xFF5A)) {
    return CharType::Alphabet;
  }

  if (isUnicodeLetterOrMark(codepoint)) {
    return CharType::Alphabet;
  }

  // ASCII digits
  if (codepoint >= '0' && codepoint <= '9') {
    return CharType::Digit;
  }

  // Full-width digits
  if (codepoint >= 0xFF10 && codepoint <= 0xFF19) {
    return CharType::Digit;
  }

  // Thai digits
  if (codepoint >= 0x0E50 && codepoint <= 0x0E59) {
    return CharType::Digit;
  }

  if (isRetainedTextSymbol(codepoint)) {
    return CharType::Unknown;
  }

  // Controls, spacing characters, and common punctuation. Classify
  // them together so remove_symbols treats every non-word separator
  // consistently instead of preserving TAB/CR/NBSP as unknown text.
  if (isUnicodeControlOrSpace(codepoint) ||
      (codepoint >= 0x3000 && codepoint <= 0x303F) ||  // CJK Symbols and Punctuation
      (codepoint >= 0xFF00 && codepoint <= 0xFF0F) ||  // Full-width symbols
      (codepoint >= 0xFF61 && codepoint <= 0xFF65) ||  // Half-width CJK punctuation (｡｢｣､･)
      (codepoint >= 0x0020 && codepoint <= 0x002F) ||  // ASCII punctuation
      (codepoint >= 0x003A && codepoint <= 0x0040) ||  // ASCII punctuation
      (codepoint >= 0x005B && codepoint <= 0x0060) ||  // ASCII punctuation
      (codepoint >= 0x007B && codepoint <= 0x007E)) {  // ASCII punctuation
    return CharType::Symbol;
  }

  // Emoji ranges (comprehensive, Unicode 15.0+). Unknown-word generation
  // retains these as OTHER tokens rather than filtering them as punctuation.
  if ((codepoint >= 0x1F600 && codepoint <= 0x1F64F) ||  // Emoticons
      (codepoint >= 0x1F300 && codepoint <= 0x1F5FF) ||  // Misc Symbols and Pictographs
      (codepoint >= 0x1F680 && codepoint <= 0x1F6FF) ||  // Transport and Map
      (codepoint >= 0x1F700 && codepoint <= 0x1F77F) ||  // Alchemical Symbols
      (codepoint >= 0x1F780 && codepoint <= 0x1F7FF) ||  // Geometric Shapes Extended
      (codepoint >= 0x1F800 && codepoint <= 0x1F8FF) ||  // Supplemental Arrows-C
      (codepoint >= 0x1F900 && codepoint <= 0x1F9FF) ||  // Supplemental Symbols and Pictographs
      (codepoint >= 0x1FA00 && codepoint <= 0x1FA6F) ||  // Chess Symbols
      (codepoint >= 0x1FA70 && codepoint <= 0x1FAFF) ||  // Symbols and Pictographs Extended-A
      (codepoint >= 0x1FB00 && codepoint <= 0x1FBFF) ||  // Symbols for Legacy Computing
      (codepoint >= 0x2600 && codepoint <= 0x26FF) ||    // Misc symbols
      (codepoint >= 0x2700 && codepoint <= 0x27BF) ||    // Dingbats
      (codepoint >= 0x2300 && codepoint <= 0x23FF) ||    // Misc Technical (⌚⌛⏰ etc.)
      (codepoint >= 0x25A0 && codepoint <= 0x25FF) ||    // Geometric Shapes
      (codepoint >= 0x2B50 && codepoint <= 0x2B55) ||    // Stars and circles (⭐⭕ etc.)
      (codepoint >= 0x2934 && codepoint <= 0x2935) ||    // Arrows
      (codepoint >= 0x2614 && codepoint <= 0x2615) ||    // Umbrella, hot beverage
      (codepoint >= 0x2648 && codepoint <= 0x2653) ||    // Zodiac signs
      (codepoint >= 0x267F && codepoint <= 0x267F) ||    // Wheelchair
      (codepoint >= 0x2693 && codepoint <= 0x2693) ||    // Anchor
      (codepoint >= 0x26A1 && codepoint <= 0x26A1) ||    // High voltage
      (codepoint >= 0x26AA && codepoint <= 0x26AB) ||    // Circles
      (codepoint >= 0x26BD && codepoint <= 0x26BE) ||    // Sports balls
      (codepoint >= 0x26C4 && codepoint <= 0x26C5) ||    // Snowman, sun
      (codepoint >= 0x26CE && codepoint <= 0x26CE) ||    // Ophiuchus
      (codepoint >= 0x26D4 && codepoint <= 0x26D4) ||    // No entry
      (codepoint >= 0x26EA && codepoint <= 0x26EA) ||    // Church
      (codepoint >= 0x26F2 && codepoint <= 0x26F3) ||    // Fountain, golf
      (codepoint >= 0x26F5 && codepoint <= 0x26F5) ||    // Sailboat
      (codepoint >= 0x26FA && codepoint <= 0x26FA) ||    // Tent
      (codepoint >= 0x26FD && codepoint <= 0x26FD) ||    // Fuel pump
      (codepoint >= 0x231A && codepoint <= 0x231B) ||    // Watch, hourglass
      (codepoint >= 0x23E9 && codepoint <= 0x23F3) ||    // Media controls
      (codepoint >= 0x23F8 && codepoint <= 0x23FA) ||    // Media controls
      isEmojiModifier(codepoint) || isRegionalIndicator(codepoint)) {
    return CharType::Emoji;
  }

  if (isUnicodePunctuationOrSymbol(codepoint)) {
    return CharType::Symbol;
  }

  return CharType::Unknown;
}

std::string_view charTypeToString(CharType type) {
  switch (type) {
    case CharType::Kanji:
      return "KANJI";
    case CharType::Hiragana:
      return "HIRAGANA";
    case CharType::Katakana:
      return "KATAKANA";
    case CharType::Alphabet:
      return "ALPHABET";
    case CharType::Digit:
      return "DIGIT";
    case CharType::Symbol:
      return "SYMBOL";
    case CharType::Emoji:
      return "EMOJI";
    case CharType::Unknown:
    default:
      return "UNKNOWN";
  }
}

bool canCombine(CharType first_type, CharType second_type) {
  if (first_type == second_type) {
    return true;
  }

  // Alphabet + Digit can combine (e.g., "abc123")
  if ((first_type == CharType::Alphabet && second_type == CharType::Digit) ||
      (first_type == CharType::Digit && second_type == CharType::Alphabet)) {
    return true;
  }

  // Hiragana + Katakana can combine in some cases
  if ((first_type == CharType::Hiragana && second_type == CharType::Katakana) ||
      (first_type == CharType::Katakana && second_type == CharType::Hiragana)) {
    return false;  // Generally separate
  }

  return false;
}

bool isCommonParticle(char32_t ch) {
  // Common particles: を, が, は, に, へ, の
  return ch == U'を' || ch == U'が' || ch == U'は' || ch == U'に' || ch == U'へ' || ch == U'の';
}

bool isNeverVerbStemAfterKanji(char32_t ch) {
  // Common particles + も, や
  // These follow nouns as particles, not as verb conjugation starts
  // Note: か is excluded - can be part of verb conjugation (書かない, 動かす)
  return isCommonParticle(ch) || ch == U'も' || ch == U'や';
}

bool isNeverVerbStemAtStart(char32_t ch) {
  // Particles that never start verbs + よ (sentence-final particle)
  // Note: も, や are excluded - can start verbs (もらう, やる)
  // Note: ね is excluded - 寝る (neru, to sleep) is a common ichidan verb
  //       Connection rules will handle invalid ね(particle) + AUX patterns
  // Note: に is excluded - にげる (逃げる), にる (煮る), にぎる (握る) etc. are common verbs
  //       The particle use of に will be handled by scoring/dictionary
  // Note: わ is excluded - わかる, わたる, わける etc. are common verbs
  //       The sentence-final particle use of わ will be handled by scoring
  // Note: は is excluded - はじまる, はたらく, はなす, はしる, etc. are common verbs
  //       The particle use of は will be handled by scoring
  // を, が, へ, の are particles that never start verbs
  // Note: While のこる(残る), のむ(飲む) etc. exist as verbs, they are typically
  // written in kanji. Allowing の as verb stem creates too many false positives
  // (のよう→のる, のに→のにる, etc.) that hurt accuracy more than they help.
  return ch == U'を' || ch == U'が' || ch == U'へ' || ch == U'の' || ch == U'よ';
}

bool isDemonstrativeStart(char32_t first, char32_t second) {
  // Check for こ/そ/あ/ど + れ/こ/ち patterns (demonstrative pronouns)
  // Examples: これ, それ, あれ, どれ, ここ, そこ, あそこ, どこ, etc.
  return (first == U'こ' || first == U'そ' || first == U'あ' || first == U'ど') &&
         (second == U'れ' || second == U'こ' || second == U'ち');
}

bool isNeverAdjectiveStemAfterKanji(char32_t ch) {
  // Common particles + も, や + て, で (te-form particles)
  // て/で indicate te-form patterns (食べている), not adjective stems
  return isNeverVerbStemAfterKanji(ch) || ch == U'て' || ch == U'で';
}

bool isExtendedParticle(char32_t ch) {
  // Extended particle check for various contexts
  // Common particles: を, が, は, に, へ, の
  // Sentence-final: か, ね, よ, わ
  // Additional: で, と, も
  return isCommonParticle(ch) || ch == U'か' || ch == U'ね' || ch == U'よ' || ch == U'わ' || ch == U'で' ||
         ch == U'と' || ch == U'も';
}

bool isOpeningBracket(char32_t ch) {
  // ASCII/full-width parentheses used for furigana readings, plus the common
  // CJK opening brackets. Closing brackets, emoji, and other symbols are
  // excluded: text after them (犬🐕です, 本(重要)です) continues normally.
  return ch == U'(' || ch == U'（' || ch == U'「' || ch == U'『' || ch == U'【' || ch == U'〔' || ch == U'〈' ||
         ch == U'《' || ch == U'［' || ch == U'[' || ch == U'｛' || ch == U'{';
}

bool isClosingBracket(char32_t ch) {
  return ch == U')' || ch == U'）' || ch == U'」' || ch == U'』' || ch == U'】' || ch == U'〕' || ch == U'〉' ||
         ch == U'》' || ch == U'］' || ch == U']' || ch == U'｝' || ch == U'}';
}

bool isProlongedSoundMark(char32_t ch) {
  // U+30FC: Katakana-Hiragana Prolonged Sound Mark (ー)
  // Used in both katakana and colloquial hiragana (すごーい, やばーい)
  return ch == 0x30FC;
}

bool isEmojiModifier(char32_t ch) {
  // ZWJ (Zero Width Joiner) - combines emojis
  if (ch == 0x200D)
    return true;

  if (isVariationSelector(ch))
    return true;

  // Skin tone modifiers (Fitzpatrick scale)
  if (ch >= 0x1F3FB && ch <= 0x1F3FF)
    return true;

  // Combining Enclosing Keycap
  if (ch == 0x20E3)
    return true;

  // Tag characters (used in subdivision flags)
  if (ch >= 0xE0020 && ch <= 0xE007F)
    return true;

  return false;
}

bool isVariationSelector(char32_t ch) {
  return (ch >= 0xFE00 && ch <= 0xFE0F) || (ch >= 0xE0100 && ch <= 0xE01EF);
}

bool isTransparentFormatControl(char32_t ch) {
  return ch == 0x200B;  // Zero Width Space
}

bool isRegionalIndicator(char32_t ch) {
  // Regional Indicator Symbols (A-Z for country flags)
  return ch >= 0x1F1E6 && ch <= 0x1F1FF;
}

bool isIterationMark(char32_t ch) {
  // U+3005: IDEOGRAPHIC ITERATION MARK (々)
  // Repeats the preceding kanji in words like 人々, 日々, 堂々
  return ch == 0x3005;
}

// Hiragana vowel-row membership. The canonical character lists live in
// core/kana_constants.h (kana::is*RowCodepoint); these forward so the row
// membership has a single source of truth across the normalize and core layers.
bool isARowHiragana(char32_t ch) {
  return kana::isARowCodepoint(ch);
}

bool isIRowHiragana(char32_t ch) {
  return kana::isIRowCodepoint(ch);
}

bool isURowHiragana(char32_t ch) {
  return kana::isURowCodepoint(ch);
}

bool isERowHiragana(char32_t ch) {
  return kana::isERowCodepoint(ch);
}

bool isORowHiragana(char32_t ch) {
  return kana::isORowCodepoint(ch);
}

bool isKanjiCodepoint(char32_t ch) {
  // Delegate to the single kanji-range definition in core/kana_constants.h.
  return kana::isKanjiCodepoint(ch);
}

bool isCounterKanji(char32_t cp) {
  return hasCharProperty(cp, kCounter);
}

bool isDurationSuffixKanji(char32_t code_point) {
  return hasCharProperty(code_point, kDurationSuffix);
}

bool isTemporalRelationSuffixKanji(char32_t code_point) {
  return hasCharProperty(code_point, kTemporalRelationSuffix);
}

bool isTemporalCounterKanji(char32_t code_point) {
  return hasCharProperty(code_point, kTemporalCounter);
}

bool isQuantityPrefixKanji(char32_t code_point) {
  return hasCharProperty(code_point, kQuantityPrefix);
}

bool isNumericApproxPrefixKanji(char32_t code_point) {
  return hasCharProperty(code_point, kNumericApproxPrefix);
}

bool isIntervalCompoundSecondKanji(char32_t code_point) {
  // Kanji that form an 間-initial interval word (間隔). After a duration counter
  // a member kanji takes the interval reading (N年|間隔) while a non-member takes
  // the duration reading (N年間|続けた, N時間|半). Extend as the closed class needs.
  return code_point == U'隔';
}

bool isTemporalSpanSuffixKanji(char32_t code_point) {
  return hasCharProperty(code_point, kTemporalSpanSuffix);
}

bool isDurationCompoundHeadKanji(char32_t code_point) {
  // Period units that form a duration noun with 間 (時間, 期間, 週間, 年間). 期 is
  // not a temporal counter — nothing is counted in 期間 — so the set is spelled
  // out rather than derived from the counter properties.
  static constexpr std::array<char32_t, 8> kDurationHeads = {U'時', U'期', U'週', U'年', U'月', U'日', U'分', U'秒'};
  return std::find(kDurationHeads.begin(), kDurationHeads.end(), code_point) != kDurationHeads.end();
}

bool isDerivationalNounSuffixKanji(char32_t code_point) {
  return hasCharProperty(code_point, kDerivationalNounSuffix);
}

bool isQuantityPhraseSuffixKanji(char32_t code_point) {
  return hasCharProperty(code_point, kQuantityPhraseSuffix);
}

bool isFiscalYearBindingPair(char32_t stem_last, char32_t suffix) {
  // 年 + 度 binds as the lexical noun 年度 (fiscal year); the trailing 度 is not
  // the degree/frequency suffix here, so X年度 stays whole (今年度, 来年度).
  return stem_last == U'年' && suffix == U'度';
}

bool isTemporalAdverbialNounPair(char32_t first, char32_t second) {
  // Compositional: temporal prefix + temporal unit (今年, 昨日, 来週, 先月, 翌朝, 毎回)
  const bool prefix =
      (first == U'今' || first == U'来' || first == U'先' || first == U'昨' || first == U'翌' || first == U'毎');
  const bool unit = (second == U'日' || second == U'週' || second == U'月' || second == U'年' || second == U'回' ||
                     second == U'朝' || second == U'晩' || second == U'夜');
  if (prefix && unit) {
    return true;
  }
  // Closed residual set of 副詞可能 temporal nouns.
  static constexpr std::array<std::array<char32_t, 2>, 10> kAdverbialTemporalNouns = {{{U'現', U'在'},
                                                                                       {U'明', U'日'},
                                                                                       {U'本', U'日'},
                                                                                       {U'当', U'時'},
                                                                                       {U'従', U'来'},
                                                                                       {U'最', U'近'},
                                                                                       {U'将', U'来'},
                                                                                       {U'今', U'後'},
                                                                                       {U'過', U'去'},
                                                                                       {U'朝', U'晩'}}};
  for (const auto& pair : kAdverbialTemporalNouns) {
    if (pair[0] == first && pair[1] == second) {
      return true;
    }
  }
  return false;
}

bool continuesTemporalNounCompound(char32_t prefix, char32_t next) {
  if (isTemporalAdverbialNounPair(prefix, next) || isTemporalRelationSuffixKanji(next) ||
      isTemporalSpanSuffixKanji(next)) {
    return true;
  }
  // Occasion and period units outside the 副詞可能 pair set (今度, 今期, 毎時,
  // 今夏).  Object counters are deliberately absent: 本 counts cylinders, so
  // 今本 is the adverbial 今 plus its object, not a compound.
  static constexpr std::array<char32_t, 10> kTemporalCompoundUnits = {U'度', U'期', U'時', U'分', U'秒',
                                                                      U'春', U'夏', U'秋', U'冬', U'宵'};
  for (char32_t unit : kTemporalCompoundUnits) {
    if (unit == next) {
      return true;
    }
  }
  return false;
}

bool isNumeralCodepoint(char32_t code_point) {
  // Arabic numerals (half-width and full-width)
  if ((code_point >= U'0' && code_point <= U'9') || (code_point >= U'０' && code_point <= U'９')) {
    return true;
  }
  // Kanji numerals
  switch (code_point) {
    case U'〇':  // Ideographic number zero (U+3007)
    case U'一':
    case U'二':
    case U'三':
    case U'四':
    case U'五':
    case U'六':
    case U'七':
    case U'八':
    case U'九':
    case U'十':
    case U'百':
    case U'千':
    case U'万':
      return true;
    default:
      return false;
  }
}

bool isAllKatakana(std::string_view surface) {
  if (surface.empty()) {
    return false;
  }
  auto codepoints = toCodepoints(surface);
  if (codepoints.empty()) {
    return false;
  }
  for (char32_t cpt : codepoints) {
    if (classifyChar(cpt) != CharType::Katakana) {
      return false;
    }
  }
  return true;
}

}  // namespace suzume::normalize
