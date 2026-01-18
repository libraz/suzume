#include "char_type.h"

namespace suzume::normalize {

CharType classifyChar(char32_t codepoint) {
  // Hiragana: U+3040-U+309F
  if (codepoint >= 0x3040 && codepoint <= 0x309F) {
    return CharType::Hiragana;
  }

  // Katakana: U+30A0-U+30FF, U+31F0-U+31FF (small), U+FF66-U+FF9F (half-width)
  if ((codepoint >= 0x30A0 && codepoint <= 0x30FF) ||
      (codepoint >= 0x31F0 && codepoint <= 0x31FF) ||
      (codepoint >= 0xFF66 && codepoint <= 0xFF9F)) {
    return CharType::Katakana;
  }

  // CJK Unified Ideographs (kanji)
  if ((codepoint >= 0x4E00 && codepoint <= 0x9FFF) ||    // CJK Unified Ideographs
      (codepoint >= 0x3400 && codepoint <= 0x4DBF) ||    // CJK Extension A
      (codepoint >= 0x20000 && codepoint <= 0x2A6DF) ||  // CJK Extension B
      (codepoint >= 0x2A700 && codepoint <= 0x2B73F) ||  // CJK Extension C
      (codepoint >= 0x2B740 && codepoint <= 0x2B81F) ||  // CJK Extension D
      (codepoint >= 0xF900 && codepoint <= 0xFAFF) ||    // CJK Compatibility Ideographs
      (codepoint >= 0x2F00 && codepoint <= 0x2FDF)) {    // Kangxi Radicals
    return CharType::Kanji;
  }

  // ASCII alphabet
  if ((codepoint >= 'A' && codepoint <= 'Z') ||
      (codepoint >= 'a' && codepoint <= 'z')) {
    return CharType::Alphabet;
  }

  // Full-width alphabet
  if ((codepoint >= 0xFF21 && codepoint <= 0xFF3A) ||
      (codepoint >= 0xFF41 && codepoint <= 0xFF5A)) {
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

  // Common punctuation and symbols
  if ((codepoint >= 0x3000 && codepoint <= 0x303F) ||  // CJK Symbols and Punctuation
      (codepoint >= 0xFF00 && codepoint <= 0xFF0F) ||  // Full-width symbols
      (codepoint >= 0x0020 && codepoint <= 0x002F) ||  // ASCII punctuation
      (codepoint >= 0x003A && codepoint <= 0x0040) ||  // ASCII punctuation
      (codepoint >= 0x005B && codepoint <= 0x0060) ||  // ASCII punctuation
      (codepoint >= 0x007B && codepoint <= 0x007E)) {  // ASCII punctuation
    return CharType::Symbol;
  }

  // Emoji ranges (comprehensive, Unicode 15.0+)
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
      (codepoint >= 0x2B50 && codepoint <= 0x2B55) ||    // Stars and circles (⭐⭕ etc.)
      (codepoint >= 0x2934 && codepoint <= 0x2935) ||    // Arrows
      (codepoint >= 0x25AA && codepoint <= 0x25AB) ||    // Squares
      (codepoint >= 0x25B6 && codepoint <= 0x25C0) ||    // Triangles
      (codepoint >= 0x25FB && codepoint <= 0x25FE) ||    // Squares
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
      (codepoint >= 0x200D && codepoint <= 0x200D) ||    // ZWJ (Zero Width Joiner)
      (codepoint >= 0xFE0E && codepoint <= 0xFE0F) ||    // Variation selectors
      (codepoint >= 0x20E3 && codepoint <= 0x20E3) ||    // Combining enclosing keycap
      (codepoint >= 0xE0020 && codepoint <= 0xE007F) ||  // Tag characters (flags)
      (codepoint >= 0x1F1E6 && codepoint <= 0x1F1FF) ||  // Regional Indicator Symbols
      (codepoint >= 0x1F3FB && codepoint <= 0x1F3FF)) {  // Skin tone modifiers
    return CharType::Emoji;
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
  return ch == U'を' || ch == U'が' || ch == U'は' ||
         ch == U'に' || ch == U'へ' || ch == U'の';
}

bool isNeverVerbStemAfterKanji(char32_t ch) {
  // Common particles + も, や
  // These follow nouns as particles, not as verb conjugation starts
  // Note: か is excluded - can be part of verb conjugation (書かない, 動かす)
  return isCommonParticle(ch) || ch == U'も' || ch == U'や';
}

bool isNeverVerbStemAtStart(char32_t ch) {
  // Particles that never start verbs + よ, わ (sentence-final particles)
  // Note: も, や are excluded - can start verbs (もらう, やる)
  // Note: ね is excluded - 寝る (neru, to sleep) is a common ichidan verb
  //       Connection rules will handle invalid ね(particle) + AUX patterns
  // Note: に is excluded - にげる (逃げる), にる (煮る), にぎる (握る) etc. are common verbs
  //       The particle use of に will be handled by scoring/dictionary
  // を, が, は, へ, の are particles that never start verbs
  return ch == U'を' || ch == U'が' || ch == U'は' ||
         ch == U'へ' || ch == U'の' || ch == U'よ' || ch == U'わ';
}

bool isDemonstrativeStart(char32_t first, char32_t second) {
  // Check for こ/そ/あ/ど + れ/こ/ち patterns (demonstrative pronouns)
  // Examples: これ, それ, あれ, どれ, ここ, そこ, あそこ, どこ, etc.
  return (first == U'こ' || first == U'そ' ||
          first == U'あ' || first == U'ど') &&
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
  return isCommonParticle(ch) || ch == U'か' || ch == U'ね' ||
         ch == U'よ' || ch == U'わ' || ch == U'で' ||
         ch == U'と' || ch == U'も';
}

bool isProlongedSoundMark(char32_t ch) {
  // U+30FC: Katakana-Hiragana Prolonged Sound Mark (ー)
  // Used in both katakana and colloquial hiragana (すごーい, やばーい)
  return ch == 0x30FC;
}

bool isEmojiModifier(char32_t ch) {
  // ZWJ (Zero Width Joiner) - combines emojis
  if (ch == 0x200D) return true;

  // Variation Selectors (text vs emoji presentation)
  if (ch >= 0xFE0E && ch <= 0xFE0F) return true;

  // Skin tone modifiers (Fitzpatrick scale)
  if (ch >= 0x1F3FB && ch <= 0x1F3FF) return true;

  // Combining Enclosing Keycap
  if (ch == 0x20E3) return true;

  // Tag characters (used in subdivision flags)
  if (ch >= 0xE0020 && ch <= 0xE007F) return true;

  return false;
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

bool isARowHiragana(char32_t ch) {
  // A-row (あ段): あ, か, が, さ, ざ, た, だ, な, は, ば, ぱ, ま, や, ら, わ
  return ch == U'あ' || ch == U'か' || ch == U'が' ||
         ch == U'さ' || ch == U'ざ' || ch == U'た' ||
         ch == U'だ' || ch == U'な' || ch == U'は' ||
         ch == U'ば' || ch == U'ぱ' || ch == U'ま' ||
         ch == U'や' || ch == U'ら' || ch == U'わ';
}

bool isIRowHiragana(char32_t ch) {
  // I-row (い段): い, き, ぎ, し, じ, ち, ぢ, に, ひ, び, ぴ, み, り
  return ch == U'い' || ch == U'き' || ch == U'ぎ' ||
         ch == U'し' || ch == U'じ' || ch == U'ち' ||
         ch == U'ぢ' || ch == U'に' || ch == U'ひ' ||
         ch == U'び' || ch == U'ぴ' || ch == U'み' || ch == U'り';
}

bool isURowHiragana(char32_t ch) {
  // U-row (う段): う, く, ぐ, す, ず, つ, づ, ぬ, ふ, ぶ, ぷ, む, ゆ, る
  return ch == U'う' || ch == U'く' || ch == U'ぐ' ||
         ch == U'す' || ch == U'ず' || ch == U'つ' ||
         ch == U'づ' || ch == U'ぬ' || ch == U'ふ' ||
         ch == U'ぶ' || ch == U'ぷ' || ch == U'む' ||
         ch == U'ゆ' || ch == U'る';
}

bool isERowHiragana(char32_t ch) {
  // E-row (え段): え, け, げ, せ, ぜ, て, で, ね, へ, べ, ぺ, め, れ
  return ch == U'え' || ch == U'け' || ch == U'げ' ||
         ch == U'せ' || ch == U'ぜ' || ch == U'て' ||
         ch == U'で' || ch == U'ね' || ch == U'へ' ||
         ch == U'べ' || ch == U'ぺ' || ch == U'め' || ch == U'れ';
}

bool isORowHiragana(char32_t ch) {
  // O-row (お段): お, こ, ご, そ, ぞ, と, ど, の, ほ, ぼ, ぽ, も, よ, ろ, を
  return ch == U'お' || ch == U'こ' || ch == U'ご' ||
         ch == U'そ' || ch == U'ぞ' || ch == U'と' ||
         ch == U'ど' || ch == U'の' || ch == U'ほ' ||
         ch == U'ぼ' || ch == U'ぽ' || ch == U'も' ||
         ch == U'よ' || ch == U'ろ' || ch == U'を';
}

bool isKanjiCodepoint(char32_t ch) {
  // CJK Unified Ideographs and extensions
  return (ch >= 0x4E00 && ch <= 0x9FFF) ||    // CJK Unified Ideographs
         (ch >= 0x3400 && ch <= 0x4DBF) ||    // CJK Extension A
         (ch >= 0x20000 && ch <= 0x2A6DF) ||  // CJK Extension B
         (ch >= 0x2A700 && ch <= 0x2B73F) ||  // CJK Extension C
         (ch >= 0x2B740 && ch <= 0x2B81F) ||  // CJK Extension D
         (ch >= 0xF900 && ch <= 0xFAFF) ||    // CJK Compatibility Ideographs
         (ch >= 0x2F00 && ch <= 0x2FDF);      // Kangxi Radicals
}

}  // namespace suzume::normalize
