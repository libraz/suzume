// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT

#include "test_helpers.h"

#include <string>
#include <vector>

namespace suzume::test {

bool hasParticle(const std::vector<core::Morpheme>& result,
                 const std::string& surface) {
  for (const auto& mor : result) {
    if (mor.surface == surface && mor.pos == core::PartOfSpeech::Particle) {
      return true;
    }
  }
  return false;
}

bool hasSurface(const std::vector<core::Morpheme>& result,
                const std::string& surface) {
  for (const auto& mor : result) {
    if (mor.surface == surface) {
      return true;
    }
  }
  return false;
}

std::vector<std::string> getSurfaces(
    const std::vector<core::Morpheme>& result) {
  std::vector<std::string> surfaces;
  surfaces.reserve(result.size());
  for (const auto& mor : result) {
    surfaces.push_back(mor.surface);
  }
  return surfaces;
}

}  // namespace suzume::test
