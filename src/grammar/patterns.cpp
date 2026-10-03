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
  if (last9 == "しない") {
    return true;
  }

  return false;
}

bool endsWithPassiveCausativeNegativeRenyokei(std::string_view surface) {
  // Check from longest to shortest patterns

  // させなく (12 bytes): causative + negative renyokei
  // されなく (12 bytes): passive + negative renyokei
  // られなく (12 bytes): passive/potential + negative renyokei
  if (utf8::endsWithAny(surface, {"させなく", "されなく", "られなく"})) {
    return true;
  }

  // せなく (9 bytes): short causative + negative renyokei
  // れなく (9 bytes): short passive/potential + negative renyokei
  if (utf8::endsWithAny(surface, {"せなく", "れなく"})) {
    return true;
  }

  return false;
}

bool endsWithNegativeBecomePattern(std::string_view surface) {
  // Check from longest to shortest patterns
  // させられなくなった (27 bytes): causative-passive + negative + become + past
  // せられなくなった (24 bytes): short causative-passive + negative + become
  // られなくなった (21 bytes): passive/potential + negative + become + past
  // れなくなった (18 bytes): short passive/potential + negative + become + past
  return utf8::endsWithAny(surface, {"させられなくなった", "せられなくなった", "られなくなった", "れなくなった"});
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
