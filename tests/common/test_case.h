// Test case data structures for data-driven testing.

#pragma once

#include <ostream>
#include <string>
#include <vector>

#include "core/types.h"

namespace suzume::test {

// Expected morpheme in a test case
struct ExpectedMorpheme {
  std::string surface;
  std::string pos;      // String representation of POS (e.g., "Noun", "Verb")
  std::string lemma;    // Optional: empty if not checked

  // Convert string POS to PartOfSpeech enum
  core::PartOfSpeech posEnum() const;
};

// A single test case
struct TestCase {
  std::string id;                           // Unique identifier
  std::string input;                        // Input text to analyze
  std::vector<ExpectedMorpheme> expected;   // Expected morphemes
  std::vector<std::string> tags;            // Tags for filtering (e.g., "verb", "basic")
  std::string description;                  // Optional description

  // Check if this test case has a specific tag
  bool hasTag(const std::string& tag) const;
};

// Collection of test cases
struct TestSuite {
  std::string version;
  std::vector<TestCase> cases;

  // Filter cases by tag
  std::vector<TestCase> filterByTag(const std::string& tag) const;
};

// Google Test printing support
inline void PrintTo(const ExpectedMorpheme& m, std::ostream* os) {
  *os << "{surface: \"" << m.surface << "\"";
  if (!m.pos.empty()) {
    *os << ", pos: \"" << m.pos << "\"";
  }
  if (!m.lemma.empty()) {
    *os << ", lemma: \"" << m.lemma << "\"";
  }
  *os << "}";
}

inline void PrintTo(const TestCase& tc, std::ostream* os) {
  *os << tc.id << ": \"" << tc.input << "\"";
}

}  // namespace suzume::test
