/**
 * @file patterns.cpp
 * @brief Verb/adjective pattern detection utilities
 */

#include "patterns.h"

namespace suzume::grammar {

bool endsWithVerbNegative(std::string_view surface) {
  // Minimum size: Xない = 9 bytes (hiragana 3 bytes × 3)
  if (surface.size() < 9) {
    return false;
  }

  std::string_view last9 = surface.substr(surface.size() - 9);

  // Godan verb mizenkei + ない (a-row + ない)
  // か(ka), が(ga), さ(sa), た(ta), ば(ba), ま(ma), な(na), ら(ra), わ(wa)
  if (last9 == "かない" || last9 == "がない" || last9 == "さない" ||
      last9 == "たない" || last9 == "ばない" || last9 == "まない" ||
      last9 == "らない" || last9 == "わない" || last9 == "なない") {
    return true;
  }

  // Ichidan verb + ない (e-row/i-row stem ending + ない)
  // 食べない → べない, 見ない → みない, etc.
  // Note: These patterns can appear when a kanji is followed by hiragana
  if (last9 == "べない" || last9 == "めない" || last9 == "せない" ||
      last9 == "てない" || last9 == "ねない" || last9 == "けない" ||
      last9 == "げない" || last9 == "れない") {
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
  if (surface.size() >= 12 &&
      surface.substr(surface.size() - 12) == "させなく") {
    return true;
  }

  // されなく (12 bytes): passive + negative renyokei
  if (surface.size() >= 12 &&
      surface.substr(surface.size() - 12) == "されなく") {
    return true;
  }

  // られなく (12 bytes): passive/potential + negative renyokei
  if (surface.size() >= 12 &&
      surface.substr(surface.size() - 12) == "られなく") {
    return true;
  }

  // せなく (9 bytes): short causative + negative renyokei
  if (surface.size() >= 9 && surface.substr(surface.size() - 9) == "せなく") {
    return true;
  }

  // れなく (9 bytes): short passive/potential + negative renyokei
  if (surface.size() >= 9 && surface.substr(surface.size() - 9) == "れなく") {
    return true;
  }

  return false;
}

bool endsWithNegativeBecomePattern(std::string_view surface) {
  // Check from longest to shortest patterns

  // させられなくなった (27 bytes): causative-passive + negative + become + past
  if (surface.size() >= 27 &&
      surface.substr(surface.size() - 27) == "させられなくなった") {
    return true;
  }

  // せられなくなった (24 bytes): short causative-passive + negative + become
  if (surface.size() >= 24 &&
      surface.substr(surface.size() - 24) == "せられなくなった") {
    return true;
  }

  // られなくなった (21 bytes): passive/potential + negative + become + past
  if (surface.size() >= 21 &&
      surface.substr(surface.size() - 21) == "られなくなった") {
    return true;
  }

  // れなくなった (18 bytes): short passive/potential + negative + become + past
  if (surface.size() >= 18 &&
      surface.substr(surface.size() - 18) == "れなくなった") {
    return true;
  }

  return false;
}

bool endsWithGodanNegativeRenyokei(std::string_view surface) {
  // かなく (9 bytes): godan ka-row negative renyokei
  // E.g., いかなく = いく + ない連用形
  if (surface.size() >= 9 && surface.substr(surface.size() - 9) == "かなく") {
    return true;
  }

  return false;
}

}  // namespace suzume::grammar
