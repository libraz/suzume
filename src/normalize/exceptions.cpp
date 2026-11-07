#include "normalize/exceptions.h"

#include "core/utf8_constants.h"

namespace suzume::normalize {

namespace {

// Multi-kana particles that should not be treated as verb endings when
// generating verb candidates; single-kana ones live in isParticleCodepoint().
constexpr std::string_view kParticleStrings[] = {
    // Compound particles (複合助詞)
    "から",
    "まで",
    "より",
    "ほど",
};

// Copula and auxiliary patterns that should not be treated as verb endings.
constexpr std::string_view kCopulaStrings[] = {
    // Basic copula (基本形)
    "だ",
    "です",
    // Past forms (過去形)
    "だった",
    "でした",
    // Partial forms (途中形)
    "でし",
    // Formal copula (文語形)
    "である",
};

// Case, binding and compound particles that should not be treated as verb endings.
bool isParticle(std::string_view surface) {
  if (surface.size() == core::kJapaneseCharBytes && isParticleCodepoint(utf8::decodeFirstChar(surface))) {
    return true;
  }
  return utf8::equalsAny(surface, kParticleStrings);
}

bool isCopula(std::string_view surface) {
  return utf8::equalsAny(surface, kCopulaStrings);
}

}  // namespace

bool isParticleOrCopula(std::string_view surface) {
  return isParticle(surface) || isCopula(surface);
}

bool isFormalNounSurface(std::string_view surface) {
  return surface.size() == core::kJapaneseCharBytes && isFormalNounCodepoint(utf8::decodeFirstChar(surface));
}

bool isFormalNounCodepoint(char32_t ch) {
  switch (ch) {
    case U'所':
    case U'物':
    case U'事':
    case U'時':
    case U'方':
    case U'為':
      return true;
    default:
      return false;
  }
}

bool isParticleCodepoint(char32_t ch) {
  switch (ch) {
    case U'が':
    case U'を':
    case U'に':
    case U'で':
    case U'と':
    case U'へ':
    case U'の':
    case U'は':
    case U'も':
    case U'や':
    case U'か':
      return true;
    default:
      return false;
  }
}

bool isClauseChainingComma(char32_t code) {
  return code == U'、' || code == U'，' || code == U',';
}

}  // namespace suzume::normalize
