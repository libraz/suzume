
// Tests for CoreDictionary verb conjugation expansion

#include "dictionary/core_dict.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

namespace suzume::dictionary {
namespace {

class CoreDictVerbExpansionTest : public ::testing::Test {
 protected:
  CoreDictionary dict_;

  // Helper to check if a surface form exists in dictionary with expected lemma
  bool hasEntry(const std::string& surface, const std::string& expected_lemma) {
    auto results = dict_.lookup(surface, 0);
    for (const auto& result : results) {
      if (result.entry != nullptr && result.entry->surface == surface) {
        // Check lemma if provided
        if (!expected_lemma.empty()) {
          std::string lemma = result.entry->lemma.empty()
                                  ? result.entry->surface
                                  : result.entry->lemma;
          if (lemma == expected_lemma) {
            return true;
          }
        } else {
          return true;
        }
      }
    }
    return false;
  }

  // Helper to check if surface exists as a verb
  bool hasVerbEntry(const std::string& surface,
                    const std::string& expected_lemma) {
    auto results = dict_.lookup(surface, 0);
    for (const auto& result : results) {
      if (result.entry != nullptr && result.entry->surface == surface &&
          result.entry->pos == core::PartOfSpeech::Verb) {
        std::string lemma = result.entry->lemma.empty()
                                ? result.entry->surface
                                : result.entry->lemma;
        if (lemma == expected_lemma) {
          return true;
        }
      }
    }
    return false;
  }
};

// =============================================================================
// Ichidan verb expansion (できる)
// =============================================================================

TEST_F(CoreDictVerbExpansionTest, Ichidan_BaseForm) {
  EXPECT_TRUE(hasVerbEntry("できる", "できる"));
}

TEST_F(CoreDictVerbExpansionTest, Ichidan_PastForm) {
  EXPECT_TRUE(hasVerbEntry("できた", "できる"));
}

TEST_F(CoreDictVerbExpansionTest, Ichidan_TeForm) {
  EXPECT_TRUE(hasVerbEntry("できて", "できる"));
}

TEST_F(CoreDictVerbExpansionTest, Ichidan_NegativeForm) {
  EXPECT_TRUE(hasVerbEntry("できない", "できる"));
}

TEST_F(CoreDictVerbExpansionTest, Ichidan_ConditionalBa) {
  EXPECT_TRUE(hasVerbEntry("できれば", "できる"));
}

TEST_F(CoreDictVerbExpansionTest, Ichidan_ConditionalTara) {
  EXPECT_TRUE(hasVerbEntry("できたら", "できる"));
}

// =============================================================================
// Godan-Ra verb expansion (わかる)
// =============================================================================

TEST_F(CoreDictVerbExpansionTest, GodanRa_BaseForm) {
  EXPECT_TRUE(hasVerbEntry("わかる", "わかる"));
}

TEST_F(CoreDictVerbExpansionTest, GodanRa_Renyokei) {
  EXPECT_TRUE(hasVerbEntry("わかり", "わかる"));
}

TEST_F(CoreDictVerbExpansionTest, GodanRa_PastForm) {
  EXPECT_TRUE(hasVerbEntry("わかった", "わかる"));
}

TEST_F(CoreDictVerbExpansionTest, GodanRa_TeForm) {
  EXPECT_TRUE(hasVerbEntry("わかって", "わかる"));
}

TEST_F(CoreDictVerbExpansionTest, GodanRa_NegativeForm) {
  EXPECT_TRUE(hasVerbEntry("わからない", "わかる"));
}

// =============================================================================
// Godan-Wa verb expansion (もらう)
// =============================================================================

TEST_F(CoreDictVerbExpansionTest, GodanWa_BaseForm) {
  EXPECT_TRUE(hasVerbEntry("もらう", "もらう"));
}

TEST_F(CoreDictVerbExpansionTest, GodanWa_Renyokei) {
  EXPECT_TRUE(hasVerbEntry("もらい", "もらう"));
}

TEST_F(CoreDictVerbExpansionTest, GodanWa_PastForm) {
  EXPECT_TRUE(hasVerbEntry("もらった", "もらう"));
}

TEST_F(CoreDictVerbExpansionTest, GodanWa_TeForm) {
  EXPECT_TRUE(hasVerbEntry("もらって", "もらう"));
}

TEST_F(CoreDictVerbExpansionTest, GodanWa_NegativeForm) {
  EXPECT_TRUE(hasVerbEntry("もらわない", "もらう"));
}

TEST_F(CoreDictVerbExpansionTest, GodanWa_PotentialForm) {
  EXPECT_TRUE(hasVerbEntry("もらえる", "もらう"));
}

TEST_F(CoreDictVerbExpansionTest, GodanWa_PotentialNegative) {
  EXPECT_TRUE(hasVerbEntry("もらえない", "もらう"));
}

// =============================================================================
// Godan-Sa verb expansion (いたす)
// =============================================================================

TEST_F(CoreDictVerbExpansionTest, GodanSa_BaseForm) {
  EXPECT_TRUE(hasVerbEntry("いたす", "いたす"));
}

TEST_F(CoreDictVerbExpansionTest, GodanSa_Renyokei) {
  EXPECT_TRUE(hasVerbEntry("いたし", "いたす"));
}

// =============================================================================
// Suru verb expansion (する)
// =============================================================================

TEST_F(CoreDictVerbExpansionTest, Suru_BaseForm) {
  EXPECT_TRUE(hasVerbEntry("する", "する"));
}

// NOTE: した, して, しない are now split (し+た, し+て, し+ない)
// per MeCab-compatible design. These tests are removed.

TEST_F(CoreDictVerbExpansionTest, Suru_Stem) {
  // し is now registered as a verb stem for MeCab-compatible splitting
  // The stem itself may not be in the dictionary expansion,
  // but the tokenizer handles it via entries.cpp
}

TEST_F(CoreDictVerbExpansionTest, Suru_ConditionalBa) {
  EXPECT_TRUE(hasVerbEntry("すれば", "する"));
}

TEST_F(CoreDictVerbExpansionTest, Suru_ConditionalTara) {
  EXPECT_TRUE(hasVerbEntry("したら", "する"));
}

TEST_F(CoreDictVerbExpansionTest, Suru_Volitional) {
  EXPECT_TRUE(hasVerbEntry("しよう", "する"));
}

// NOTE: している is now split (し+て+いる) per MeCab-compatible design.
// This test is removed.

// =============================================================================
// Essential verbs expansion (伴う - GodanWa from essential_verbs.h)
// =============================================================================

TEST_F(CoreDictVerbExpansionTest, EssentialVerb_Tomonau_Base) {
  EXPECT_TRUE(hasVerbEntry("伴う", "伴う"));
}

TEST_F(CoreDictVerbExpansionTest, EssentialVerb_Tomonau_Renyokei) {
  EXPECT_TRUE(hasVerbEntry("伴い", "伴う"));
}

TEST_F(CoreDictVerbExpansionTest, EssentialVerb_Tomonau_Past) {
  EXPECT_TRUE(hasVerbEntry("伴った", "伴う"));
}

// =============================================================================
// Noun entries are NOT expanded
// =============================================================================

TEST_F(CoreDictVerbExpansionTest, NounNotExpanded) {
  // できあがり is a noun, should exist as-is without conjugation
  EXPECT_TRUE(hasEntry("できあがり", "できあがり"));

  // Should NOT have verb conjugations of できあがり
  auto results = dict_.lookup("できあがります", 0);
  bool has_noun_conjugation = false;
  for (const auto& result : results) {
    if (result.entry != nullptr &&
        result.entry->surface == "できあがります" &&
        result.entry->lemma == "できあがり") {
      has_noun_conjugation = true;
    }
  }
  EXPECT_FALSE(has_noun_conjugation)
      << "Noun できあがり should not have conjugated forms";
}

}  // namespace
}  // namespace suzume::dictionary
