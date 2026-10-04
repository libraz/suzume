/**
 * @file patterns.cpp
 * @brief Verb/adjective pattern detection utilities
 */

#include "patterns.h"

#include "core/kana_constants.h"
#include "core/utf8_constants.h"

namespace suzume::grammar {

bool endsWithVerbNegative(std::string_view surface) {
  // Minimum size: Xない = 9 bytes (hiragana 3 bytes × 3)
  std::string_view last9 = utf8::last3Chars(surface);
  if (last9.empty()) {
    return false;
  }

  // Godan verb mizenkei + ない (a-row + ない)
  if (last9.substr(core::kJapaneseCharBytes) == "ない" &&
      utf8::equalsAny(last9.substr(0, core::kJapaneseCharBytes), kana::kMizenkeiEndings)) {
    return true;
  }

  // Ichidan verb + ない (e-row stem ending + ない)
  // 食べない → べない, 眺めない → めない, etc.
  // Only e-row endings are matched; i-row Ichidan negatives (見ない → みない)
  // are not covered here. These patterns can appear when a kanji is followed
  // by hiragana.
  if (utf8::equalsAny(last9, {"えない", "けない", "げない", "せない", "ぜない", "てない", "でない", "ねない", "へない",
                              "べない", "ぺない", "めない", "れない"})) {
    return true;
  }

  // Suru verb + ない
  return last9 == "しない";
}

bool endsWithPassiveCausativeNegativeRenyokei(std::string_view surface) {
  // Every causative and passive/potential variant ends in せなく or れなく.
  return utf8::endsWithAny(surface, {"せなく", "れなく"});
}

bool endsWithNegativeBecomePattern(std::string_view surface) {
  // Every passive/potential and causative-passive variant ends in れなくなった.
  return utf8::endsWith(surface, "れなくなった");
}

bool endsWithGodanNegativeRenyokei(std::string_view surface) {
  // Mizenkei ending + なく, with at least one character before it
  if (surface.size() <= core::kThreeJapaneseCharBytes || !utf8::endsWith(surface, "なく")) {
    return false;
  }
  const std::string_view ending =
      surface.substr(surface.size() - core::kThreeJapaneseCharBytes, core::kJapaneseCharBytes);
  return utf8::equalsAny(ending, kana::kMizenkeiEndings);
}

}  // namespace suzume::grammar
