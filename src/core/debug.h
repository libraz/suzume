/**
 * @file debug.h
 * @brief Debug output infrastructure for morphological analysis
 *
 * Compile-time control:
 *   - Define SUZUME_DEBUG at compile time to enable debug infrastructure
 *   - Without SUZUME_DEBUG, all debug code is completely eliminated
 *
 * Runtime control (when SUZUME_DEBUG is defined):
 *   - Set SUZUME_DEBUG=1 environment variable to enable output
 */

#ifndef SUZUME_CORE_DEBUG_H_
#define SUZUME_CORE_DEBUG_H_

#ifdef SUZUME_DEBUG

#include <cstdlib>
#include <iostream>

namespace suzume::core {

/**
 * @brief Debug output control (only compiled when SUZUME_DEBUG is defined)
 */
class Debug {
 public:
  static bool isEnabled() {
    static const bool enabled = (std::getenv("SUZUME_DEBUG") != nullptr);
    return enabled;
  }

  static std::ostream& log() { return std::cerr; }
};

}  // namespace suzume::core

#define SUZUME_DEBUG_LOG(expr) \
  do {                         \
    if (suzume::core::Debug::isEnabled()) { \
      suzume::core::Debug::log() << expr; \
    }                          \
  } while (0)

// Additional macros for complex debug blocks
#define SUZUME_DEBUG_IF(cond) if (suzume::core::Debug::isEnabled() && (cond))
#define SUZUME_DEBUG_BLOCK if (suzume::core::Debug::isEnabled())
#define SUZUME_DEBUG_STREAM suzume::core::Debug::log()

#else  // SUZUME_DEBUG not defined - complete elimination

#define SUZUME_DEBUG_LOG(expr) ((void)0)
#define SUZUME_DEBUG_IF(cond) if constexpr (false)
#define SUZUME_DEBUG_BLOCK if constexpr (false)

// Dummy stream that discards everything (for rare direct stream usage)
namespace suzume::core {
struct NullStream {
  template<typename T>
  NullStream& operator<<(const T&) { return *this; }
  NullStream& operator<<(std::ostream& (*)(std::ostream&)) { return *this; }
};
inline NullStream& nullStream() { static NullStream ns; return ns; }
}  // namespace suzume::core
#define SUZUME_DEBUG_STREAM suzume::core::nullStream()

#endif  // SUZUME_DEBUG

#endif  // SUZUME_CORE_DEBUG_H_
