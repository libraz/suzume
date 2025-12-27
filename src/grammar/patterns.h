/**
 * @file patterns.h
 * @brief Verb/adjective pattern detection utilities
 *
 * Provides functions to detect grammatical patterns that help
 * distinguish verb forms from adjective candidates.
 */

#ifndef SUZUME_GRAMMAR_PATTERNS_H_
#define SUZUME_GRAMMAR_PATTERNS_H_

#include <string_view>

namespace suzume::grammar {

/**
 * @brief Check if surface ends with verb negative pattern (mizenkei + ない)
 * @param surface The surface to check
 * @return True if the surface ends with verb negative pattern
 *
 * Matches:
 * - Godan: かない, がない, さない, たない, ばない, まない, なない, らない, わない
 * - Ichidan: べない, めない, せない, てない, ねない, けない, げない, れない
 * - Suru: しない
 *
 * Examples:
 * - 書かない → true (godan negative: 書く + ない)
 * - 食べない → true (ichidan negative: 食べる + ない)
 * - 勉強しない → true (suru negative: 勉強する + ない)
 * - 美味しくない → false (i-adjective negative, not verb)
 */
bool endsWithVerbNegative(std::string_view surface);

/**
 * @brief Check if surface ends with passive/potential/causative negative renyokei
 * @param surface The surface to check
 * @return True if the surface ends with passive/causative negative renyokei
 *
 * Matches:
 * - られなく (passive/potential + negative renyokei)
 * - れなく (short passive/potential + negative renyokei)
 * - させなく (causative + negative renyokei)
 * - せなく (short causative + negative renyokei)
 * - されなく (passive + negative renyokei)
 *
 * Examples:
 * - 食べられなく → true (食べる + られる + ない連用形)
 * - 使い切れなく → true (使い切る + れる + ない連用形)
 */
bool endsWithPassiveCausativeNegativeRenyokei(std::string_view surface);

/**
 * @brief Check if surface ends with verb negative + become pattern
 * @param surface The surface to check
 * @return True if the surface ends with negative + なった/なる pattern
 *
 * Matches:
 * - れなくなった, られなくなった
 * - させられなくなった, せられなくなった
 *
 * Examples:
 * - 読まれなくなった → true (読む + れる + なくなる + た)
 * - 食べられなくなった → true
 */
bool endsWithNegativeBecomePattern(std::string_view surface);

/**
 * @brief Check if surface ends with godan negative renyokei (mizenkei + なく)
 * @param surface The surface to check
 * @return True if the surface ends with godan negative renyokei
 *
 * Matches: かなく (godan negative renyokei of ka-row verbs)
 *
 * Examples:
 * - いかなく → true (いく + ない連用形)
 * - かかなく → true (かく + ない連用形)
 */
bool endsWithGodanNegativeRenyokei(std::string_view surface);

}  // namespace suzume::grammar

#endif  // SUZUME_GRAMMAR_PATTERNS_H_
