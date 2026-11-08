#include "core/lattice.h"

#include <gtest/gtest.h>

#include <limits>
#include <utility>

namespace suzume::core {
namespace {

constexpr size_t kRejectedEdge = std::numeric_limits<size_t>::max();

TEST(LatticeTest, RejectsInvalidSpansAndCategoriesWithoutMutatingIndexes) {
  Lattice lattice(3);

  EXPECT_EQ(lattice.addEdge("", 0, 0, PartOfSpeech::Noun, 0.0F, 0), kRejectedEdge);
  EXPECT_EQ(lattice.addEdge("x", 3, 4, PartOfSpeech::Noun, 0.0F, 0), kRejectedEdge);
  EXPECT_EQ(lattice.addEdge("x", 0, 1, PartOfSpeech::Count_, 0.0F, 0), kRejectedEdge);
  EXPECT_EQ(lattice.addEdge("x", 0, 1, PartOfSpeech::Noun, 0.0F, 0, {}, dictionary::ConjugationType::None,
                            CandidateOrigin::Unknown, 0.0F, {}, ExtendedPOS::Count_),
            kRejectedEdge);

  EXPECT_TRUE(lattice.edgeIdsAt(0).empty());
  EXPECT_TRUE(lattice.edgeIdsEndingAt(1).empty());
}

TEST(LatticeTest, EndingIndexTracksAddAndMove) {
  Lattice lattice(3);
  const auto first = lattice.addEdge("ab", 0, 2, PartOfSpeech::Noun, 0.0F, 0);
  const auto second = lattice.addEdge("b", 1, 2, PartOfSpeech::Noun, 0.0F, 0);
  const auto third = lattice.addEdge("c", 2, 3, PartOfSpeech::Noun, 0.0F, 0);

  ASSERT_EQ(lattice.edgeIdsEndingAt(2).size(), 2U);
  EXPECT_EQ(lattice.edgeIdsEndingAt(2)[0], first);
  EXPECT_EQ(lattice.edgeIdsEndingAt(2)[1], second);
  ASSERT_EQ(lattice.edgeIdsEndingAt(3).size(), 1U);
  EXPECT_EQ(lattice.edgeIdsEndingAt(3)[0], third);
  EXPECT_TRUE(lattice.edgeIdsEndingAt(4).empty());

  Lattice moved(std::move(lattice));
  ASSERT_EQ(moved.edgeIdsEndingAt(2).size(), 2U);
  EXPECT_EQ(moved.getEdge(moved.edgeIdsEndingAt(2)[0]).surface, "ab");
}

}  // namespace
}  // namespace suzume::core
