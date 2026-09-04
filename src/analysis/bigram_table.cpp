#include "bigram_table_internal.h"

namespace suzume::analysis {

namespace bigram_rules {

bool applyRules(BigramMatrix& table, const BigramRule* rules, size_t rule_count) {
  for (size_t rule_index = 0; rule_index < rule_count; ++rule_index) {
    const BigramRule& rule = rules[rule_index];
    if (rule.cost == kUnsetCost || table[rule.prev][rule.next] != kUnsetCost) {
      return false;
    }
    table[rule.prev][rule.next] = rule.cost;
  }
  return true;
}

void inheritRuleProfile(BigramMatrix& table, core::ExtendedPOS source, core::ExtendedPOS target) {
  const size_t source_idx = static_cast<size_t>(source);
  const size_t target_idx = static_cast<size_t>(target);
  for (size_t idx = 0; idx < BigramTable::kSize; ++idx) {
    table[target_idx][idx] = table[source_idx][idx];
    table[idx][target_idx] = table[idx][source_idx];
  }
  // The row copy makes target→source equal source→source; use it to complete
  // the target→target intersection after the column copy.
  table[target_idx][target_idx] = table[source_idx][source_idx];
}

}  // namespace bigram_rules

float BigramTable::getCost(core::ExtendedPOS prev, core::ExtendedPOS next) {
  size_t prev_idx = static_cast<size_t>(prev);
  size_t next_idx = static_cast<size_t>(next);
  if (prev_idx >= kSize || next_idx >= kSize) {
    return 0.0F;
  }
  return bigram_rules::decodeCost(table_[prev_idx][next_idx]);
}

BigramTable::EncodedTable BigramTable::initTable() {
  bigram_rules::BigramMatrix table{};
  for (auto& row : table) {
    row.fill(bigram_rules::kUnsetCost);
  }

  bigram_rules::setVerbAndAdjectiveCosts(table);
  bigram_rules::setAuxiliaryAndNounCosts(table);
  bigram_rules::setParticleAndLexicalCosts(table);
  // Quotative demonstrative adverbs are a semantic subtype. Their category
  // cost remains distinct, while their syntactic continuations stay complete
  // as the general adverb profile evolves.
  bigram_rules::inheritRuleProfile(table, core::ExtendedPOS::Adverb, core::ExtendedPOS::AdverbQuotative);
  // The colloquial contraction of the hypothetical is a single word that closes
  // a conditional clause. What may follow it is therefore what may follow the
  // conjunctive particle it absorbed, while what may precede it is what may
  // precede any finite verb. Inheriting each half from its own source keeps the
  // form complete as either profile evolves.
  bigram_rules::inheritRuleProfile(table, core::ExtendedPOS::ParticleConj, core::ExtendedPOS::VerbContractedKateikei);
  for (size_t idx = 0; idx < BigramTable::kSize; ++idx) {
    table[idx][static_cast<size_t>(core::ExtendedPOS::VerbContractedKateikei)] =
        table[idx][static_cast<size_t>(core::ExtendedPOS::VerbShuushikei)];
  }
  // A nominal in Latin letters or digits is a noun in every respect the general
  // category covers, so it inherits that profile whole rather than restating
  // it. The one cell that differs is set below.
  bigram_rules::inheritRuleProfile(table, core::ExtendedPOS::Noun, core::ExtendedPOS::NounForeign);
  // Japanese attributive modification does not reach across a script change:
  // the run is its own orthographic unit, so nothing in front of it is reading
  // as its modifier. The prohibition that keeps a sentence-final particle from
  // heading a nominal is about exactly that modification (the final な against
  // the copular attributive な), and it has no purchase here — while a final
  // particle before a Latin run is the ordinary way a colloquial clause ends.
  table[static_cast<size_t>(core::ExtendedPOS::ParticleFinal)][static_cast<size_t>(core::ExtendedPOS::NounForeign)] =
      bigram_rules::encodeCost(bigram_cost::kNeutral);

  // A quotative demonstrative cannot directly complete an adjective stem.
  // Keep appearance そう on its auxiliary path (高+そう, キモ+そう).
  table[static_cast<size_t>(core::ExtendedPOS::AdjStem)][static_cast<size_t>(core::ExtendedPOS::AdverbQuotative)] =
      bigram_rules::encodeCost(bigram_cost::kRare);
  for (auto& row : table) {
    for (uint8_t& encoded_cost : row) {
      if (encoded_cost == bigram_rules::kUnsetCost) {
        encoded_cost = bigram_rules::encodeCost(bigram_cost::kNeutral);
      }
    }
  }
  return table;
}

const BigramTable::EncodedTable BigramTable::table_ = BigramTable::initTable();

}  // namespace suzume::analysis
