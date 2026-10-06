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
  // An interrogative adverb continues like any adverb except that it stands
  // directly on a nominal predicate (なぜ+だ, なぜ+です), so the copula cells
  // carry the connection of a neutral head instead of the manner-adverb penalty.
  {
    bigram_rules::inheritRuleProfile(table, core::ExtendedPOS::Adverb, core::ExtendedPOS::AdverbInterrogative);
    const size_t interrogative = static_cast<size_t>(core::ExtendedPOS::AdverbInterrogative);
    for (const auto copula : {core::ExtendedPOS::AuxCopulaDa, core::ExtendedPOS::AuxCopulaDesu}) {
      table[interrogative][static_cast<size_t>(copula)] = bigram_rules::encodeCost(bigram_cost::kMinorBonus);
    }
  }
  // The pejorative subsidiary やがる continues like the progressive auxiliary but
  // stands directly on a continuative instead of a て form.
  {
    bigram_rules::inheritRuleProfile(table, core::ExtendedPOS::AuxAspectIru, core::ExtendedPOS::AuxPejorativeYagaru);
    table[static_cast<size_t>(core::ExtendedPOS::VerbRenyokei)]
         [static_cast<size_t>(core::ExtendedPOS::AuxPejorativeYagaru)] =
             bigram_rules::encodeCost(bigram_cost::kStrongBonus);
  }
  // The trace suffix っけ continues like any suffix, and stands on a continuative
  // firmly enough to outrank the homographic final particle (飾り+っけ).
  {
    bigram_rules::inheritRuleProfile(table, core::ExtendedPOS::Suffix, core::ExtendedPOS::SuffixTrace);
    table[static_cast<size_t>(core::ExtendedPOS::VerbRenyokei)][static_cast<size_t>(core::ExtendedPOS::SuffixTrace)] =
        bigram_rules::encodeCost(bigram_cost::kDoubleVeryStrongBonus);
  }
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
  // The choice か stands where the final か does, and differs only in what it
  // hands on: it joins a following item, so a nominal head may follow it where
  // the final か prohibits one. After a nominal the final か already serves
  // (雨か雪), and a split-off か there would outbid a longer particle ending in
  // か (どころか), so only a predicate may precede the choice か.
  {
    bigram_rules::inheritRuleProfile(table, core::ExtendedPOS::ParticleFinal, core::ExtendedPOS::ParticleChoice);
    const size_t choice = static_cast<size_t>(core::ExtendedPOS::ParticleChoice);
    for (size_t idx = 0; idx < BigramTable::kSize; ++idx) {
      const auto other = static_cast<core::ExtendedPOS>(idx);
      if (core::isNounType(other) || other == core::ExtendedPOS::Pronoun ||
          other == core::ExtendedPOS::PronounInterrogative) {
        table[choice][idx] = bigram_rules::encodeCost(bigram_cost::kNeutral);
        table[idx][choice] = bigram_rules::encodeCost(bigram_cost::kNever);
      }
    }
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
  // A greeting is an interjection that stands as its utterance's predicate, so
  // it takes the complements an exclamation cannot: a case-marked one
  // (みんな+に+ありがとう) and a connective clause (来て+くれ+て+ありがとう).
  bigram_rules::inheritRuleProfile(table, core::ExtendedPOS::Interjection, core::ExtendedPOS::InterjectionGreeting);
  for (const auto complement : {core::ExtendedPOS::ParticleCase, core::ExtendedPOS::ParticleConj}) {
    table[static_cast<size_t>(complement)][static_cast<size_t>(core::ExtendedPOS::InterjectionGreeting)] =
        bigram_rules::encodeCost(bigram_cost::kNeutral);
  }

  // The bare stem of らしい is what the auxiliary's hosts precede, but it only
  // nominalizes (本+らし+さ, 子供+らし+げ); every other cell carries its own
  // ending, so nothing else may follow the stem (春+らし+かっ is no reading).
  {
    const size_t rashii = static_cast<size_t>(core::ExtendedPOS::AuxConjectureRashii);
    const size_t stem = static_cast<size_t>(core::ExtendedPOS::AuxConjectureRashiiStem);
    for (size_t idx = 0; idx < BigramTable::kSize; ++idx) {
      table[idx][stem] = table[idx][rashii];
      table[stem][idx] = bigram_rules::encodeCost(bigram_cost::kAlmostNever);
    }
    table[stem][static_cast<size_t>(core::ExtendedPOS::Suffix)] =
        table[rashii][static_cast<size_t>(core::ExtendedPOS::Suffix)];
  }

  // The terminal-selecting conjecture らむ continues like the volitional/
  // conjectural む, and takes the hosts む takes except for the cell: a terminal
  // (行く+らむ, 高かる+らむ) instead of an irrealis, and never the past た
  // (来+たら+む, not 来+た+らむ).
  {
    bigram_rules::inheritRuleProfile(table, core::ExtendedPOS::AuxVolitional,
                                     core::ExtendedPOS::AuxClassicalConjectureTerminal);
    const size_t ramu = static_cast<size_t>(core::ExtendedPOS::AuxClassicalConjectureTerminal);
    for (const auto irrealis :
         {core::ExtendedPOS::VerbMizenkei, core::ExtendedPOS::AdjMizenkei, core::ExtendedPOS::AuxTenseTa}) {
      table[static_cast<size_t>(irrealis)][ramu] = bigram_rules::encodeCost(bigram_cost::kAlmostNever);
    }
    table[static_cast<size_t>(core::ExtendedPOS::VerbShuushikei)][ramu] =
        bigram_rules::encodeCost(bigram_cost::kStrongBonus);
  }

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
