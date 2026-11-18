#ifndef SUZUME_CORE_EDGE_FLAGS_H_
#define SUZUME_CORE_EDGE_FLAGS_H_

#include <cstdint>

namespace suzume::core {

/** Flags carried from a lattice candidate through postprocessing. */
enum class EdgeFlags : uint8_t {
  None = 0,
  FromDictionary = 1 << 0,
  FromUserDict = 1 << 1,
  IsFormalNoun = 1 << 2,
  // Bit 3 is reserved for the removed IsLowInfo flag. Low-information status
  // is derived exclusively from ExtendedPOS.
  IsUnknown = 1 << 4,
  // Composed by a rule from dictionary-attested parts (a compound of a
  // listed V1 and a closed-lexicon V2) without being an entry itself.
  DictionaryComposed = 1 << 5,
  HasCustomCost = 1 << 6,
  LemmaVerified = 1 << 7,
};

inline EdgeFlags operator|(EdgeFlags lhs, EdgeFlags rhs) {
  return static_cast<EdgeFlags>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

inline EdgeFlags operator&(EdgeFlags lhs, EdgeFlags rhs) {
  return static_cast<EdgeFlags>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}

inline EdgeFlags withoutFlag(EdgeFlags flags, EdgeFlags flag) {
  return static_cast<EdgeFlags>(static_cast<uint8_t>(flags) & ~static_cast<uint8_t>(flag));
}

inline bool hasFlag(EdgeFlags flags, EdgeFlags flag) {
  return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(flag)) != 0;
}

}  // namespace suzume::core

#endif  // SUZUME_CORE_EDGE_FLAGS_H_
