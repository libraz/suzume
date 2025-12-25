/**
 * @file connection.h
 * @brief Morpheme connection rules and connection ID constants
 *
 * Design: MeCab-style connection system for morpheme boundaries.
 * Uses connection IDs to efficiently look up valid connections
 * and compute connection costs.
 */

#ifndef SUZUME_GRAMMAR_CONNECTION_H_
#define SUZUME_GRAMMAR_CONNECTION_H_

#include <algorithm>
#include <cstdint>
#include <vector>

namespace suzume::grammar {

/**
 * @brief Connection ID constants
 *
 * Organized by grammatical category:
 * - 0x00xx: Sentence boundaries
 * - 0x01xx: Verb stem endings
 * - 0x02xx: Auxiliary verb inputs (left connection)
 * - 0x03xx: Auxiliary verb outputs (right connection)
 * - 0x04xx: Particles
 * - 0x05xx: Nouns
 */
namespace conn {

// === Sentence boundaries (0x00xx) ===
constexpr uint16_t kBOS = 0x0000;  // Beginning of sentence
constexpr uint16_t kEOS = 0x0001;  // End of sentence

// === Verb stem endings (0x01xx) ===
constexpr uint16_t kVerbBase = 0x0100;      // 終止形: 書く
constexpr uint16_t kVerbMizenkei = 0x0101;  // 未然形: 書か
constexpr uint16_t kVerbRenyokei = 0x0102;  // 連用形: 書き
constexpr uint16_t kVerbOnbinkei = 0x0103;  // 音便形: 書い (te/ta-ready)
constexpr uint16_t kVerbPotential = 0x0104; // 可能形語幹: 書け (e-row)
constexpr uint16_t kIAdjStem = 0x0105;      // い形容詞語幹: 美し (ku-form ready)
constexpr uint16_t kVerbVolitional = 0x0106;  // 意志形: 書こう, 食べよう
constexpr uint16_t kVerbKatei = 0x0107;       // 仮定形: 書け (e-row for Godan)

// === Auxiliary inputs - what they require (0x02xx) ===
constexpr uint16_t kAuxMasu = 0x0200;      // ます (requires 連用形)
constexpr uint16_t kAuxNai = 0x0201;       // ない (requires 未然形)
constexpr uint16_t kAuxTa = 0x0202;        // た/だ (requires 音便形)
constexpr uint16_t kAuxTe = 0x0203;        // て/で (requires 音便形)
constexpr uint16_t kAuxTeiru = 0x0204;     // いる (requires て形)
constexpr uint16_t kAuxTeshimau = 0x0205;  // しまう (requires て形)
constexpr uint16_t kAuxTeoku = 0x0206;     // おく (requires て形)
constexpr uint16_t kAuxTekuru = 0x0207;    // くる (requires て形)
constexpr uint16_t kAuxTeiku = 0x0208;     // いく (requires て形)
constexpr uint16_t kAuxTemiru = 0x0209;    // みる (requires て形)
constexpr uint16_t kAuxTemorau = 0x020A;   // もらう (requires て形)
constexpr uint16_t kAuxTekureru = 0x020B;  // くれる (requires て形)
constexpr uint16_t kAuxTeageru = 0x020C;   // あげる (requires て形)
constexpr uint16_t kAuxTai = 0x020D;       // たい (requires 連用形)
constexpr uint16_t kAuxReru = 0x020E;      // れる/られる (requires 未然形)
constexpr uint16_t kAuxSeru = 0x020F;      // せる/させる (requires 未然形)
constexpr uint16_t kAuxRenyokei = 0x0210;  // 連用形 compounds (すぎる, etc.)
constexpr uint16_t kAuxSou = 0x0211;       // そう (looks like, requires 連用形)
constexpr uint16_t kAuxCopula = 0x0212;    // だ/です/である (requires noun/na-adj)

// === Auxiliary outputs - what they provide (0x03xx) ===
constexpr uint16_t kAuxOutBase = 0x0300;  // Auxiliary in base form
constexpr uint16_t kAuxOutMasu = 0x0301;  // Auxiliary in ます form
constexpr uint16_t kAuxOutTa = 0x0302;    // Auxiliary in た form
constexpr uint16_t kAuxOutTe = 0x0303;    // Auxiliary in て form

// === Particles (0x04xx) ===
constexpr uint16_t kParticle = 0x0400;

// === Nouns (0x05xx) ===
constexpr uint16_t kNoun = 0x0500;

}  // namespace conn

/**
 * @brief Connection cost between morphemes
 *
 * Sparse matrix implementation using sorted vector with binary search.
 * WASM-compatible (no std::unordered_map).
 */
class ConnectionMatrix {
 public:
  ConnectionMatrix();

  /**
   * @brief Get connection cost between two morphemes
   * @param left_right_id Right ID of left morpheme
   * @param right_left_id Left ID of right morpheme
   * @return Connection cost (kInfinite if not connectable)
   */
  int16_t getCost(uint16_t left_right_id, uint16_t right_left_id) const;

  /**
   * @brief Check if connection is valid
   */
  bool canConnect(uint16_t left_right_id, uint16_t right_left_id) const;

  static constexpr int16_t kInfinite = 32767;
  static constexpr int16_t kDefaultCost = 0;

 private:
  struct ConnectionEntry {
    uint32_t key;  // (left_id << 16) | right_id
    int16_t cost;

    bool operator<(const ConnectionEntry& other) const {
      return key < other.key;
    }
  };

  std::vector<ConnectionEntry> entries_;

  void initRules();
  void addRule(uint16_t left_id, uint16_t right_id, int16_t cost = 0);
};

/**
 * @brief Get global connection matrix instance
 */
const ConnectionMatrix& getConnectionMatrix();

}  // namespace suzume::grammar

#endif  // SUZUME_GRAMMAR_CONNECTION_H_
