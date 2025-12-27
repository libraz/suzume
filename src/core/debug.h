/**
 * @file debug.h
 * @brief Debug output infrastructure for morphological analysis
 *
 * Set SUZUME_DEBUG=1 to enable all debug output.
 */

#ifndef SUZUME_CORE_DEBUG_H_
#define SUZUME_CORE_DEBUG_H_

#include <cstdlib>
#include <iostream>

namespace suzume::core {

/**
 * @brief Debug output control
 */
class Debug {
 public:
  static bool isEnabled() {
    static const bool enabled = (std::getenv("SUZUME_DEBUG") != nullptr);
    return enabled;
  }

  static std::ostream& log() { return std::cerr; }
};

#define SUZUME_DEBUG_LOG(expr) \
  do {                         \
    if (suzume::core::Debug::isEnabled()) { \
      suzume::core::Debug::log() << expr; \
    }                          \
  } while (0)

}  // namespace suzume::core

#endif  // SUZUME_CORE_DEBUG_H_
