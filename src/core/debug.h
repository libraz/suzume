/**
 * @file debug.h
 * @brief Debug output infrastructure for morphological analysis
 *
 * Controlled via environment variables:
 *   SUZUME_DEBUG=1       - Enable all debug output
 *   SUZUME_DEBUG_AUX=1   - Auxiliary matching details
 *   SUZUME_DEBUG_INFL=1  - Inflection analysis
 *   SUZUME_DEBUG_CONN=1  - Connection cost details
 *   SUZUME_DEBUG_VITERBI=1 - Viterbi algorithm steps
 *   SUZUME_DEBUG_SCORE=1 - Word cost details (bonuses/penalties)
 */

#ifndef SUZUME_CORE_DEBUG_H_
#define SUZUME_CORE_DEBUG_H_

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace suzume::core {

/**
 * @brief Debug output control
 */
class Debug {
 public:
  // Check if specific debug category is enabled
  static bool isEnabled() {
    static const bool enabled = (std::getenv("SUZUME_DEBUG") != nullptr);
    return enabled;
  }

  static bool isAuxEnabled() {
    static const bool enabled =
        isEnabled() || (std::getenv("SUZUME_DEBUG_AUX") != nullptr);
    return enabled;
  }

  static bool isInflectionEnabled() {
    static const bool enabled =
        isEnabled() || (std::getenv("SUZUME_DEBUG_INFL") != nullptr);
    return enabled;
  }

  static bool isConnectionEnabled() {
    static const bool enabled =
        isEnabled() || (std::getenv("SUZUME_DEBUG_CONN") != nullptr);
    return enabled;
  }

  static bool isViterbiEnabled() {
    static const bool enabled =
        isEnabled() || (std::getenv("SUZUME_DEBUG_VITERBI") != nullptr);
    return enabled;
  }

  static bool isScoreEnabled() {
    static const bool enabled =
        isEnabled() || (std::getenv("SUZUME_DEBUG_SCORE") != nullptr);
    return enabled;
  }

  // Print helpers
  static void printHeader(std::string_view title) {
    std::cerr << "\n=== " << title << " ===\n";
  }

  static void printSection(std::string_view title) {
    std::cerr << "\n[" << title << "]\n";
  }

  static std::ostream& log() { return std::cerr; }
};

// Convenience macros for conditional debug output
#define SUZUME_DEBUG_LOG(category, expr) \
  do {                                   \
    if (suzume::core::Debug::is##category##Enabled()) { \
      suzume::core::Debug::log() << expr; \
    }                                    \
  } while (0)

#define SUZUME_DEBUG_AUX(expr) SUZUME_DEBUG_LOG(Aux, expr)
#define SUZUME_DEBUG_INFL(expr) SUZUME_DEBUG_LOG(Inflection, expr)
#define SUZUME_DEBUG_CONN(expr) SUZUME_DEBUG_LOG(Connection, expr)
#define SUZUME_DEBUG_VITERBI(expr) SUZUME_DEBUG_LOG(Viterbi, expr)
#define SUZUME_DEBUG_SCORE(expr) SUZUME_DEBUG_LOG(Score, expr)

}  // namespace suzume::core

#endif  // SUZUME_CORE_DEBUG_H_
