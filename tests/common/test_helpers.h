// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT
//
// Common test helper functions and fixtures for Suzume analyzer tests.
// These utilities reduce code duplication across test files.

#pragma once

#include <string>
#include <vector>

#include "core/morpheme.h"

namespace suzume::test {

// Check if a morpheme with the given surface and Particle POS exists
bool hasParticle(const std::vector<core::Morpheme>& result,
                 const std::string& surface);

// Check if a morpheme with the given surface exists (any POS)
bool hasSurface(const std::vector<core::Morpheme>& result,
                const std::string& surface);

// Extract all surface forms from morpheme vector
std::vector<std::string> getSurfaces(const std::vector<core::Morpheme>& result);

}  // namespace suzume::test
