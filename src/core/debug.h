/**
 * @file debug.h
 * @brief Debug output infrastructure for morphological analysis
 *
 * Compile-time control:
 *   - Define SUZUME_DEBUG at compile time to enable debug infrastructure
 *   - Without SUZUME_DEBUG, all debug code is completely eliminated
 *   - Emscripten builds always eliminate stream-based debug infrastructure
 *
 * Runtime control (when SUZUME_DEBUG is defined):
 *   - SUZUME_DEBUG=1: Basic output (analysis results, key decisions)
 *   - SUZUME_DEBUG=2: Verbose output (cache hits, detailed scoring)
 *   - SUZUME_DEBUG=3: Trace output (all candidates, intermediate steps)
 */

#ifndef SUZUME_CORE_DEBUG_H_
#define SUZUME_CORE_DEBUG_H_

#if defined(SUZUME_DEBUG) && !defined(__EMSCRIPTEN__)

#include <cstdlib>
#include <iostream>

namespace suzume::core {

/**
 * @brief Debug output control (only compiled when SUZUME_DEBUG is defined)
 */
class Debug {
 public:
  static int getLevel() {
    static const int level = [] {
      const char* env = std::getenv("SUZUME_DEBUG");
      if (env == nullptr)
        return 0;
      int val = std::atoi(env);
      return val > 0 ? val : 1;  // "any value" defaults to 1
    }();
    return level;
  }

  static bool isEnabled() { return getLevel() >= 1; }
  static bool isVerbose() { return getLevel() >= 2; }
  static bool isTrace() { return getLevel() >= 3; }

  static std::ostream& log() { return std::cerr; }
};

}  // namespace suzume::core

#define SUZUME_DEBUG_LOG(expr)              \
  do {                                      \
    if (suzume::core::Debug::isEnabled()) { \
      suzume::core::Debug::log() << expr;   \
    }                                       \
  } while (0)

// Verbose (level 2+) - cache hits, detailed scoring
#define SUZUME_DEBUG_LOG_VERBOSE(expr)      \
  do {                                      \
    if (suzume::core::Debug::isVerbose()) { \
      suzume::core::Debug::log() << expr;   \
    }                                       \
  } while (0)

// Trace (level 3+) - all candidates, intermediate steps
#define SUZUME_DEBUG_LOG_TRACE(expr)      \
  do {                                    \
    if (suzume::core::Debug::isTrace()) { \
      suzume::core::Debug::log() << expr; \
    }                                     \
  } while (0)

// Additional macros for complex debug blocks
#define SUZUME_DEBUG_IF(cond) if (suzume::core::Debug::isEnabled() && (cond))
#define SUZUME_DEBUG_BLOCK if (suzume::core::Debug::isEnabled())
#define SUZUME_DEBUG_VERBOSE_BLOCK if (suzume::core::Debug::isVerbose())
#define SUZUME_DEBUG_TRACE_BLOCK if (suzume::core::Debug::isTrace())
#define SUZUME_DEBUG_STREAM suzume::core::Debug::log()

#else  // Debug disabled, including all Emscripten builds

#define SUZUME_DEBUG_LOG(expr) ((void)0)
#define SUZUME_DEBUG_LOG_VERBOSE(expr) ((void)0)
#define SUZUME_DEBUG_LOG_TRACE(expr) ((void)0)
#define SUZUME_DEBUG_IF(cond) if constexpr (false)
#define SUZUME_DEBUG_BLOCK if constexpr (false)
#define SUZUME_DEBUG_VERBOSE_BLOCK if constexpr (false)
#define SUZUME_DEBUG_TRACE_BLOCK if constexpr (false)

// Dummy stream that discards everything (for rare direct stream usage). The
// templated insertion accepts manipulators as ordinary values, so release
// consumers do not need to include the iostream hierarchy.
namespace suzume::core {
struct NullStream {
  template <typename T>
  NullStream& operator<<(const T&) {
    return *this;
  }
};
inline NullStream& nullStream() {
  static NullStream ns;
  return ns;
}
}  // namespace suzume::core
#define SUZUME_DEBUG_STREAM suzume::core::nullStream()

#endif  // defined(SUZUME_DEBUG) && !defined(__EMSCRIPTEN__)

// Candidate debug metadata (SUZUME_DEBUG_INFO builds only); arguments are not evaluated otherwise.
#ifdef SUZUME_DEBUG_INFO
#define SUZUME_DEBUG_CANDIDATE(cand, conf, pat) \
  do {                                          \
    (cand).confidence = (conf);                 \
    (cand).pattern = (pat);                     \
  } while (0)
#define SUZUME_DEBUG_CANDIDATE_PATTERN(cand, pat) ((cand).pattern = (pat))
#else
#define SUZUME_DEBUG_CANDIDATE(cand, conf, pat) ((void)0)
#define SUZUME_DEBUG_CANDIDATE_PATTERN(cand, pat) ((void)0)
#endif

#endif  // SUZUME_CORE_DEBUG_H_
