#include "analysis/tokenizer_utils.h"

#include <gtest/gtest.h>

#include <string_view>
#include <vector>

#include "normalize/utf8.h"

namespace suzume::analysis {
namespace {

bool spellsShape(std::string_view text) {
  const std::vector<char32_t> codepoints = normalize::toCodepoints(text);
  return spellsRiMimeticShape(codepoints, 0, codepoints.size());
}

TEST(SpellsRiMimeticShapeTest, AcceptsShortAndSokuonManner) {
  EXPECT_TRUE(spellsShape("どさり"));
  EXPECT_TRUE(spellsShape("しっかり"));
  EXPECT_TRUE(spellsShape("ぐったり"));
  EXPECT_TRUE(spellsShape("うっとり"));
}

TEST(SpellsRiMimeticShapeTest, ReadsAYoonDigraphAsOneMora) {
  // しゃ is one mora, so しゃべり is AB+り spelled in four codepoints, not Aっ+B+り.
  EXPECT_FALSE(spellsShape("しゃべり"));
  EXPECT_FALSE(spellsShape("ちょびり"));
}

TEST(SpellsRiMimeticShapeTest, RequiresTheRiEnding) {
  EXPECT_FALSE(spellsShape("しっかと"));
  EXPECT_FALSE(spellsShape("どさ"));
}

}  // namespace
}  // namespace suzume::analysis
