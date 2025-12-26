/**
 * @file auxiliaries.cpp
 * @brief Auxiliary verb entries for inflection analysis
 */

#include "auxiliaries.h"

#include "auxiliary_generator.h"

namespace suzume::grammar {

const std::vector<AuxiliaryEntry>& getAuxiliaries() {
  static const std::vector<AuxiliaryEntry> kAuxiliaries = generateAllAuxiliaries();
  return kAuxiliaries;
}

}  // namespace suzume::grammar
