// Copyright 2024 Libraz Team
// SPDX-License-Identifier: MIT

#include "test_case.h"

#include <algorithm>

namespace suzume::test {

core::PartOfSpeech ExpectedMorpheme::posEnum() const {
  if (pos == "Noun") return core::PartOfSpeech::Noun;
  if (pos == "Verb") return core::PartOfSpeech::Verb;
  if (pos == "Adjective") return core::PartOfSpeech::Adjective;
  if (pos == "Adverb") return core::PartOfSpeech::Adverb;
  if (pos == "Particle") return core::PartOfSpeech::Particle;
  if (pos == "Auxiliary") return core::PartOfSpeech::Auxiliary;
  if (pos == "Conjunction") return core::PartOfSpeech::Conjunction;
  if (pos == "Determiner") return core::PartOfSpeech::Determiner;
  if (pos == "Pronoun") return core::PartOfSpeech::Pronoun;
  if (pos == "Symbol") return core::PartOfSpeech::Symbol;
  if (pos == "Other") return core::PartOfSpeech::Other;
  return core::PartOfSpeech::Unknown;
}

bool TestCase::hasTag(const std::string& tag) const {
  return std::find(tags.begin(), tags.end(), tag) != tags.end();
}

std::vector<TestCase> TestSuite::filterByTag(const std::string& tag) const {
  std::vector<TestCase> filtered;
  for (const auto& tc : cases) {
    if (tc.hasTag(tag)) {
      filtered.push_back(tc);
    }
  }
  return filtered;
}

}  // namespace suzume::test
