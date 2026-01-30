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

// Accepted difference metadata for cases where Suzume differs from MeCab
struct AcceptedDiff {
  std::string reason;    // Why this difference is accepted
  std::string category;  // Category of difference (e.g., "pos-limitation", "lemma-diff")
};

// A single test case
struct TestCase {
  std::string id;                           // Unique identifier
  std::string input;                        // Input text to analyze
  std::vector<ExpectedMorpheme> expected;   // Expected morphemes (MeCab-compatible)
  std::vector<ExpectedMorpheme> suzume_expected;  // Suzume's expected output (if different)
  AcceptedDiff accepted_diff;               // Reason for accepted difference
  std::vector<std::string> tags;            // Tags for filtering (e.g., "verb", "basic")
  std::string description;                  // Optional description

  // Check if this test case has a specific tag
  bool hasTag(const std::string& tag) const;

  // Get the expected morphemes to use for testing
  // Returns suzume_expected if non-empty, otherwise expected
  const std::vector<ExpectedMorpheme>& getTestExpected() const {
    return suzume_expected.empty() ? expected : suzume_expected;
  }

  // Check if this test case has an accepted difference
  bool hasAcceptedDiff() const {
    return !suzume_expected.empty();
  }
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
