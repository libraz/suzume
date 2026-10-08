#include "bigram_table_internal.h"

namespace suzume::analysis {

namespace bigram_rules {

bool applyRules(BigramMatrix& table, const BigramRule* rules, size_t rule_count) {
  bool all_applied = true;
  for (size_t rule_index = 0; rule_index < rule_count; ++rule_index) {
    const BigramRule& rule = rules[rule_index];
    if (rule.cost == kUnsetCost || table[rule.prev][rule.next] != kUnsetCost) {
      all_applied = false;
      continue;
    }
    table[rule.prev][rule.next] = rule.cost;
  }
  return all_applied;
}

bool applyRuleTables(BigramMatrix& table) {
  // Evaluate every table even after a failure, so one bad rule cannot hide the rest.
  const bool verb_rules = setVerbAndAdjectiveCosts(table);
  const bool noun_rules = setAuxiliaryAndNounCosts(table);
  const bool particle_rules = setParticleAndLexicalCosts(table);
  return verb_rules && noun_rules && particle_rules;
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

  // A skipped rule leaves its cell to the later neutral fill; the unit test
  // over applyRuleTables() keeps the shipped tables free of skips.
  static_cast<void>(bigram_rules::applyRuleTables(table));
  // Quotative demonstrative adverbs are a semantic subtype. Their category
  // cost remains distinct, while their syntactic continuations stay complete
  // as the general adverb profile evolves.
  bigram_rules::inheritRuleProfile(table, core::ExtendedPOS::Adverb, core::ExtendedPOS::AdverbQuotative);
  // An interrogative adverb continues like any adverb except that it stands
  // directly on a nominal predicate (なぜ+だ, なぜ+です), so the copula cells
  // carry the connection of a neutral head instead of the manner-adverb penalty.
  {
    bigram_rules::inheritRuleProfile(table, core::ExtendedPOS::Adverb, core::ExtendedPOS::AdverbInterrogative);
    for (const auto copula : {core::ExtendedPOS::AuxCopulaDa, core::ExtendedPOS::AuxCopulaDesu}) {
      bigram_rules::setCost(table, core::ExtendedPOS::AdverbInterrogative, copula, bigram_cost::kMinorBonus);
    }
  }
  // The pejorative subsidiary やがる continues like the progressive auxiliary but
  // stands directly on a continuative instead of a て form.
  {
    bigram_rules::inheritRuleProfile(table, core::ExtendedPOS::AuxAspectIru, core::ExtendedPOS::AuxPejorativeYagaru);
    bigram_rules::setCost(table, core::ExtendedPOS::VerbRenyokei, core::ExtendedPOS::AuxPejorativeYagaru,
                          bigram_cost::kStrongBonus);
  }
  // The trace suffix っけ continues like any suffix, and stands on a continuative
  // firmly enough to outrank the homographic final particle (飾り+っけ).
  {
    bigram_rules::inheritRuleProfile(table, core::ExtendedPOS::Suffix, core::ExtendedPOS::SuffixTrace);
    bigram_rules::setCost(table, core::ExtendedPOS::VerbRenyokei, core::ExtendedPOS::SuffixTrace,
                          bigram_cost::kDoubleVeryStrongBonus);
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
  // the final か prohibits one. After a nominal the listing か (雨か雪) is the
  // choice か, but at a cost: a split-off か there must not outbid a longer
  // particle ending in か (どころか), yet it still has to beat an unclassified
  // fragment between two kana nouns (りんご+か+みかん).
  {
    bigram_rules::inheritRuleProfile(table, core::ExtendedPOS::ParticleFinal, core::ExtendedPOS::ParticleChoice);
    for (size_t idx = 0; idx < BigramTable::kSize; ++idx) {
      const auto other = static_cast<core::ExtendedPOS>(idx);
      if (core::isNounType(other) || other == core::ExtendedPOS::Pronoun ||
          other == core::ExtendedPOS::PronounInterrogative) {
        bigram_rules::setCost(table, core::ExtendedPOS::ParticleChoice, other, bigram_cost::kNeutral);
        bigram_rules::setCost(table, other, core::ExtendedPOS::ParticleChoice, bigram_cost::kRare);
      }
    }
    // An embedded question closes on the choice か and is the argument of the
    // predicate after it (起きるか+分から+ない, 高いか+安いか), so the final
    // か's bar on a following irrealis or adjective does not carry over.
    bigram_rules::setCost(table, core::ExtendedPOS::ParticleChoice, core::ExtendedPOS::VerbMizenkei,
                          bigram_cost::kNeutral);
    bigram_rules::setCost(table, core::ExtendedPOS::ParticleChoice, core::ExtendedPOS::AdjBasic, bigram_cost::kNeutral);
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
  bigram_rules::setCost(table, core::ExtendedPOS::ParticleFinal, core::ExtendedPOS::NounForeign, bigram_cost::kNeutral);
  // A greeting is an interjection that stands as its utterance's predicate, so
  // it takes the complements an exclamation cannot: a case-marked one
  // (みんな+に+ありがとう) and a connective clause (来て+くれ+て+ありがとう).
  bigram_rules::inheritRuleProfile(table, core::ExtendedPOS::Interjection, core::ExtendedPOS::InterjectionGreeting);
  for (const auto complement : {core::ExtendedPOS::ParticleCase, core::ExtendedPOS::ParticleConj}) {
    bigram_rules::setCost(table, complement, core::ExtendedPOS::InterjectionGreeting, bigram_cost::kNeutral);
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
    for (const auto irrealis :
         {core::ExtendedPOS::VerbMizenkei, core::ExtendedPOS::AdjMizenkei, core::ExtendedPOS::AuxTenseTa}) {
      bigram_rules::setCost(table, irrealis, core::ExtendedPOS::AuxClassicalConjectureTerminal,
                            bigram_cost::kAlmostNever);
    }
    bigram_rules::setCost(table, core::ExtendedPOS::VerbShuushikei, core::ExtendedPOS::AuxClassicalConjectureTerminal,
                          bigram_cost::kStrongBonus);
  }

  // A quotative demonstrative cannot directly complete an adjective stem.
  // Keep appearance そう on its auxiliary path (高+そう, キモ+そう).
  bigram_rules::setCost(table, core::ExtendedPOS::AdjStem, core::ExtendedPOS::AdverbQuotative, bigram_cost::kRare);
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
