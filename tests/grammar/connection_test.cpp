/**
 * @file connection_test.cpp
 * @brief Tests for connection matrix and connection ID lookups
 */

#include "grammar/connection.h"

#include <gtest/gtest.h>

namespace suzume {
namespace grammar {
namespace {

class ConnectionTest : public ::testing::Test {
 protected:
  const ConnectionMatrix& matrix_ = getConnectionMatrix();
};

// ============================================================================
// getConnectionMatrix tests
// ============================================================================

TEST_F(ConnectionTest, GlobalMatrixIsConsistent) {
  // Multiple calls should return the same reference
  const auto& matrix_a = getConnectionMatrix();
  const auto& matrix_b = getConnectionMatrix();
  EXPECT_EQ(&matrix_a, &matrix_b);
}

// ============================================================================
// canConnect tests - valid connections
// ============================================================================

TEST_F(ConnectionTest, VerbRenyokeiToMasu) {
  EXPECT_TRUE(matrix_.canConnect(conn::kVerbRenyokei, conn::kAuxMasu));
}

TEST_F(ConnectionTest, VerbMizenkeiToNai) {
  EXPECT_TRUE(matrix_.canConnect(conn::kVerbMizenkei, conn::kAuxNai));
}

TEST_F(ConnectionTest, VerbMizenkeiToReru) {
  EXPECT_TRUE(matrix_.canConnect(conn::kVerbMizenkei, conn::kAuxReru));
}

TEST_F(ConnectionTest, VerbMizenkeiToSeru) {
  EXPECT_TRUE(matrix_.canConnect(conn::kVerbMizenkei, conn::kAuxSeru));
}

TEST_F(ConnectionTest, VerbOnbinkeiToTa) {
  EXPECT_TRUE(matrix_.canConnect(conn::kVerbOnbinkei, conn::kAuxTa));
}

TEST_F(ConnectionTest, VerbOnbinkeiToTe) {
  EXPECT_TRUE(matrix_.canConnect(conn::kVerbOnbinkei, conn::kAuxTe));
}

TEST_F(ConnectionTest, VerbRenyokeiToTai) {
  EXPECT_TRUE(matrix_.canConnect(conn::kVerbRenyokei, conn::kAuxTai));
}

TEST_F(ConnectionTest, TeFormToSubsidiaryVerbs) {
  EXPECT_TRUE(matrix_.canConnect(conn::kAuxOutTe, conn::kAuxTeiru));
  EXPECT_TRUE(matrix_.canConnect(conn::kAuxOutTe, conn::kAuxTeshimau));
  EXPECT_TRUE(matrix_.canConnect(conn::kAuxOutTe, conn::kAuxTeoku));
  EXPECT_TRUE(matrix_.canConnect(conn::kAuxOutTe, conn::kAuxTekuru));
  EXPECT_TRUE(matrix_.canConnect(conn::kAuxOutTe, conn::kAuxTeiku));
  EXPECT_TRUE(matrix_.canConnect(conn::kAuxOutTe, conn::kAuxTemiru));
  EXPECT_TRUE(matrix_.canConnect(conn::kAuxOutTe, conn::kAuxTemorau));
  EXPECT_TRUE(matrix_.canConnect(conn::kAuxOutTe, conn::kAuxTekureru));
  EXPECT_TRUE(matrix_.canConnect(conn::kAuxOutTe, conn::kAuxTeageru));
}

TEST_F(ConnectionTest, AuxOutBaseToMasu) {
  EXPECT_TRUE(matrix_.canConnect(conn::kAuxOutBase, conn::kAuxMasu));
}

TEST_F(ConnectionTest, AuxOutBaseToTa) {
  EXPECT_TRUE(matrix_.canConnect(conn::kAuxOutBase, conn::kAuxTa));
}

TEST_F(ConnectionTest, SentenceBoundaries) {
  // Verb base -> EOS
  EXPECT_TRUE(matrix_.canConnect(conn::kVerbBase, conn::kEOS));
  // Aux outputs -> EOS
  EXPECT_TRUE(matrix_.canConnect(conn::kAuxOutBase, conn::kEOS));
  EXPECT_TRUE(matrix_.canConnect(conn::kAuxOutMasu, conn::kEOS));
  EXPECT_TRUE(matrix_.canConnect(conn::kAuxOutTa, conn::kEOS));
  // BOS -> Verb/Noun
  EXPECT_TRUE(matrix_.canConnect(conn::kBOS, conn::kVerbBase));
  EXPECT_TRUE(matrix_.canConnect(conn::kBOS, conn::kNoun));
}

TEST_F(ConnectionTest, NounParticleVerb) {
  EXPECT_TRUE(matrix_.canConnect(conn::kNoun, conn::kParticle));
  EXPECT_TRUE(matrix_.canConnect(conn::kParticle, conn::kVerbBase));
}

// ============================================================================
// canConnect tests - invalid connections
// ============================================================================

TEST_F(ConnectionTest, InvalidConnectionsReturnFalse) {
  // Onbinkei should not connect to ます
  EXPECT_FALSE(matrix_.canConnect(conn::kVerbOnbinkei, conn::kAuxMasu));
  // Mizenkei should not connect to た
  EXPECT_FALSE(matrix_.canConnect(conn::kVerbMizenkei, conn::kAuxTa));
  // Renyokei should not connect to た directly
  EXPECT_FALSE(matrix_.canConnect(conn::kVerbRenyokei, conn::kAuxTa));
  // Base form should not connect to ない
  EXPECT_FALSE(matrix_.canConnect(conn::kVerbBase, conn::kAuxNai));
  // Random unknown connection
  EXPECT_FALSE(matrix_.canConnect(0xFFFF, 0xFFFF));
}

// ============================================================================
// getCost tests
// ============================================================================

TEST_F(ConnectionTest, GetCostZeroForStandardConnections) {
  EXPECT_EQ(matrix_.getCost(conn::kVerbRenyokei, conn::kAuxMasu), 0);
  EXPECT_EQ(matrix_.getCost(conn::kVerbMizenkei, conn::kAuxNai), 0);
  EXPECT_EQ(matrix_.getCost(conn::kVerbOnbinkei, conn::kAuxTa), 0);
  EXPECT_EQ(matrix_.getCost(conn::kVerbOnbinkei, conn::kAuxTe), 0);
  EXPECT_EQ(matrix_.getCost(conn::kBOS, conn::kVerbBase), 0);
  EXPECT_EQ(matrix_.getCost(conn::kNoun, conn::kParticle), 0);
}

TEST_F(ConnectionTest, GetCostPenalizedConnection) {
  // AuxOutBase -> AuxTa has a slight penalty (100)
  EXPECT_EQ(matrix_.getCost(conn::kAuxOutBase, conn::kAuxTa), 100);
}

TEST_F(ConnectionTest, GetCostInfiniteForUnknown) {
  EXPECT_EQ(matrix_.getCost(0xFFFF, 0xFFFF),
            ConnectionMatrix::kInfinite);
  // Verb base cannot connect to ない
  EXPECT_EQ(matrix_.getCost(conn::kVerbBase, conn::kAuxNai),
            ConnectionMatrix::kInfinite);
}

// ============================================================================
// Connection constants sanity
// ============================================================================

TEST_F(ConnectionTest, ConnectionConstantsAreDistinct) {
  // Verify key connection IDs are unique
  uint16_t ids[] = {
      conn::kBOS,           conn::kEOS,
      conn::kVerbBase,      conn::kVerbMizenkei,
      conn::kVerbRenyokei,  conn::kVerbOnbinkei,
      conn::kAuxMasu,       conn::kAuxNai,
      conn::kAuxTa,         conn::kAuxTe,
      conn::kAuxOutBase,    conn::kAuxOutTe,
      conn::kParticle,      conn::kNoun,
  };
  size_t count = sizeof(ids) / sizeof(ids[0]);
  for (size_t idx = 0; idx < count; idx++) {
    for (size_t jdx = idx + 1; jdx < count; jdx++) {
      EXPECT_NE(ids[idx], ids[jdx])
          << "Connection IDs at index " << idx << " and " << jdx
          << " should be distinct";
    }
  }
}

}  // namespace
}  // namespace grammar
}  // namespace suzume
