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

  // Emoji ranges (simplified)
  if ((codepoint >= 0x1F600 && codepoint <= 0x1F64F) ||  // Emoticons
      (codepoint >= 0x1F300 && codepoint <= 0x1F5FF) ||  // Misc Symbols and Pictographs
      (codepoint >= 0x1F680 && codepoint <= 0x1F6FF) ||  // Transport and Map
      (codepoint >= 0x2600 && codepoint <= 0x26FF) ||    // Misc symbols
      (codepoint >= 0x2700 && codepoint <= 0x27BF)) {    // Dingbats
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
  // Common particles + ね, よ, わ (sentence-final particles)
  // Note: も, や are excluded - can start verbs (もらう, やる)
  return isCommonParticle(ch) || ch == U'ね' || ch == U'よ' || ch == U'わ';
}

bool isDemonstrativeStart(char32_t first, char32_t second) {
  // Check for こ/そ/あ/ど + れ/こ/ち patterns (demonstrative pronouns)
  // Examples: これ, それ, あれ, どれ, ここ, そこ, あそこ, どこ, etc.
  return (first == U'こ' || first == U'そ' ||
          first == U'あ' || first == U'ど') &&
         (second == U'れ' || second == U'こ' || second == U'ち');
}

}  // namespace suzume::normalize
