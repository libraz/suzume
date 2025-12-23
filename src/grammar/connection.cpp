/**
 * @file connection.cpp
 * @brief Connection matrix implementation
 */

#include "connection.h"

namespace suzume::grammar {

ConnectionMatrix::ConnectionMatrix() { initRules(); }

void ConnectionMatrix::addRule(uint16_t left_id, uint16_t right_id,
                               int16_t cost) {
  uint32_t key = (static_cast<uint32_t>(left_id) << 16) | right_id;
  entries_.push_back({key, cost});
}

void ConnectionMatrix::initRules() {
  using namespace conn;

  // === Verb stem → Auxiliary connections ===

  // 連用形 → ます系
  addRule(kVerbRenyokei, kAuxMasu, 0);

  // 未然形 → ない, れる, せる
  addRule(kVerbMizenkei, kAuxNai, 0);
  addRule(kVerbMizenkei, kAuxReru, 0);
  addRule(kVerbMizenkei, kAuxSeru, 0);

  // 音便形 → た, て
  addRule(kVerbOnbinkei, kAuxTa, 0);
  addRule(kVerbOnbinkei, kAuxTe, 0);

  // 連用形 → たい
  addRule(kVerbRenyokei, kAuxTai, 0);

  // === て形 → 補助動詞 connections ===
  addRule(kAuxOutTe, kAuxTeiru, 0);
  addRule(kAuxOutTe, kAuxTeshimau, 0);
  addRule(kAuxOutTe, kAuxTeoku, 0);
  addRule(kAuxOutTe, kAuxTekuru, 0);
  addRule(kAuxOutTe, kAuxTeiku, 0);
  addRule(kAuxOutTe, kAuxTemiru, 0);
  addRule(kAuxOutTe, kAuxTemorau, 0);
  addRule(kAuxOutTe, kAuxTekureru, 0);
  addRule(kAuxOutTe, kAuxTeageru, 0);

  // === Auxiliary outputs → further auxiliaries ===
  // 補助動詞(base) → ます
  addRule(kAuxOutBase, kAuxMasu, 0);

  // 補助動詞 can take た
  addRule(kAuxOutBase, kAuxTa, 100);  // Slightly penalized

  // === Sentence boundaries ===
  // 終止形 → EOS
  addRule(kVerbBase, kEOS, 0);
  addRule(kAuxOutBase, kEOS, 0);
  addRule(kAuxOutMasu, kEOS, 0);
  addRule(kAuxOutTa, kEOS, 0);

  // BOS → Verbs, Nouns
  addRule(kBOS, kVerbBase, 0);
  addRule(kBOS, kNoun, 0);

  // Noun → Particle
  addRule(kNoun, kParticle, 0);

  // Particle → Verb
  addRule(kParticle, kVerbBase, 0);

  // Sort for binary search
  std::sort(entries_.begin(), entries_.end());
}

int16_t ConnectionMatrix::getCost(uint16_t left_right_id,
                                  uint16_t right_left_id) const {
  uint32_t key = (static_cast<uint32_t>(left_right_id) << 16) | right_left_id;

  ConnectionEntry target{key, 0};
  auto iter =
      std::lower_bound(entries_.begin(), entries_.end(), target);

  if (iter != entries_.end() && iter->key == key) {
    return iter->cost;
  }
  return kInfinite;
}

bool ConnectionMatrix::canConnect(uint16_t left_right_id,
                                  uint16_t right_left_id) const {
  return getCost(left_right_id, right_left_id) != kInfinite;
}

namespace {
ConnectionMatrix g_connection_matrix;
}  // namespace

const ConnectionMatrix& getConnectionMatrix() { return g_connection_matrix; }

}  // namespace suzume::grammar
