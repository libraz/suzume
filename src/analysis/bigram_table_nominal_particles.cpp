#include "bigram_table_internal.h"

namespace suzume::analysis::bigram_rules {

using EPOS = core::ExtendedPOS;
namespace cost = bigram_cost;

namespace {

struct NominalContinuation {
  EPOS next;
  float cost;
};

struct NominalHeadProfile {
  float case_cost;
  float adverbial_cost;
  bool accepts_final_particle;
};

constexpr std::array<NominalContinuation, 4> kCommonNominalContinuations = {{
    {EPOS::ParticleTopic, cost::kStrongBonus},
    {EPOS::ParticleBinding, cost::kVeryStrongBonus},
    {EPOS::AuxCopulaDa, cost::kExtraStrongBonus},
    {EPOS::ParticleCase, cost::kNeutral},
}};

constexpr bool isNominalHead(EPOS extended_pos) {
  // Formal nouns have their own continuation profile below. Every other
  // noun/pronoun category participates in the common nominal-head rules, so a
  // newly added nominal EPOS cannot silently miss the profile.
  return (core::isNounType(extended_pos) && extended_pos != EPOS::NounFormal) || core::isPronounType(extended_pos);
}

constexpr NominalHeadProfile nominalHeadProfile(EPOS head) {
  const bool is_pronoun = core::isPronounType(head);
  return {
      head == EPOS::Pronoun ? cost::kModerateBonus : cost::kNeutral,
      is_pronoun ? cost::kExtraStrongBonus : cost::kStrongBonus,
      head != EPOS::NounNumber && head != EPOS::PronounInterrogative,
  };
}

// Being usable in a constant expression is part of these helpers' contract, and
// only an actual constant evaluation enforces it. A constexpr function that
// reaches a non-constexpr one is ill-formed with no diagnostic required, so a
// compiler may accept it here and a stricter toolchain refuse the same file.
static_assert(isNominalHead(EPOS::Noun));
static_assert(!isNominalHead(EPOS::NounFormal));
static_assert(nominalHeadProfile(EPOS::Pronoun).accepts_final_particle);

void applyRule(BigramMatrix& table, EPOS head, EPOS next, float rule_cost) {
  const BigramRule rule{head, next, rule_cost};
  applyRules(table, &rule, 1);
}

void applyNominalHeadRules(BigramMatrix& table) {
  for (size_t idx = 0; idx < static_cast<size_t>(EPOS::Count_); ++idx) {
    const EPOS head = static_cast<EPOS>(idx);
    if (!isNominalHead(head)) {
      continue;
    }
    const NominalHeadProfile profile = nominalHeadProfile(head);
    for (const NominalContinuation& continuation : kCommonNominalContinuations) {
      const float rule_cost = continuation.next == EPOS::ParticleCase ? profile.case_cost : continuation.cost;
      applyRule(table, head, continuation.next, rule_cost);
    }
    applyRule(table, head, EPOS::ParticleAdverbial, profile.adverbial_cost);
    if (profile.accepts_final_particle) {
      // Ordinary/deverbal nouns and personal pronouns can form a
      // sentence-final nominal predicate. Number phrases and interrogative
      // pronouns need a following predicate;
      // rewarding their homographic final-particle path breaks 一昼夜+かけて
      // and the fixed indefinite pronoun 何かしら.
      applyRule(table, head, EPOS::ParticleFinal, cost::kModerateBonus);
    }
  }
}

}  // namespace

void setNominalParticleCosts(BigramMatrix& table) {
  applyNominalHeadRules(table);

  static constexpr BigramRule kRules[] = {
      // Nominal particle attachment and formal-noun continuation.
      {EPOS::Noun, EPOS::ParticleConj, cost::kStrong},
      {EPOS::NounNumber, EPOS::Suffix, cost::kStrongBonus},
      {EPOS::NounProperFamily, EPOS::NounProperGiven, cost::kStrongBonus},
      {EPOS::Noun, EPOS::Conjunction, cost::kDoubleVeryStrongBonus},
      {EPOS::Conjunction, EPOS::Noun, cost::kStrongBonus},
      {EPOS::Conjunction, EPOS::Pronoun, cost::kMinorBonus},
      {EPOS::ParticleCase, EPOS::ParticleBinding, cost::kStrongBonus},
      // An adverbial particle stacks with a binding particle just as a case
      // particle does (だけしか, までしか, ばかりしか). Without the row the chain
      // is dear enough that the binding particle is split at its own mora
      // boundary and its tail is glued to a following auxiliary.
      {EPOS::ParticleAdverbial, EPOS::ParticleBinding, cost::kStrongBonus},
      {EPOS::ParticleBinding, EPOS::AdjBasic, cost::kStrongBonus},
      {EPOS::ParticleBinding, EPOS::AdjRenyokei, cost::kStrongBonus},
      {EPOS::Noun, EPOS::AdjNaAdj, cost::kStrongBonus},
      {EPOS::NounFormal, EPOS::AdjNaAdj, cost::kModerateBonus},
      {EPOS::Noun, EPOS::NounFormal, cost::kMinorBonus},
      {EPOS::Noun, EPOS::Noun, cost::kMinor},
      {EPOS::NounFormal, EPOS::ParticleCase, cost::kModerateBonus},
      {EPOS::NounFormal, EPOS::ParticleTopic, cost::kModerateBonus},
      {EPOS::NounFormal, EPOS::Adverb, cost::kStrong},
      {EPOS::NounFormal, EPOS::ParticleBinding, cost::kVeryStrongBonus},
      {EPOS::NounFormal, EPOS::AuxCopulaDa, cost::kVeryStrongBonus},
      {EPOS::NounFormal, EPOS::AuxCopulaDesu, cost::kStrongBonus},
      {EPOS::NounFormal, EPOS::AuxNegativeNai, cost::kModerateBonus},
      {EPOS::NounFormal, EPOS::AdjBasic, cost::kDoubleVeryStrongBonus},
      // The adverbial and the binding particles are the two halves of one focus
      // class, so a formal noun takes them at the same rate. Pricing the
      // adverbial particle above its sibling made the formal noun outbid a whole
      // lexicalized span that happens to spell out as a determiner plus that
      // noun, and only for the particles on this side of the class.
      {EPOS::NounFormal, EPOS::ParticleAdverbial, cost::kVeryStrongBonus},
      {EPOS::ParticleNo, EPOS::NounFormal, cost::kStrongBonus},
      // A genitive particle also heads an adnominal predicate. Preserve the
      // continuative verb before a classical auxiliary (風の+吹き+たる) over
      // the homographic deverbal-noun reading.
      {EPOS::ParticleNo, EPOS::VerbRenyokei, cost::kStrongBonus},
      // Two genitive particles cannot be adjacent. When a hiragana noun ends
      // in の, the first mora belongs to that noun (あけぼの+の), not to a
      // duplicated particle chain.
      {EPOS::ParticleNo, EPOS::ParticleNo, cost::kAlmostNever},
      // Every aspectual auxiliary heads a finite clause, and a formal noun takes
      // that clause as its adnominal (読んでいる+よう, 暑くなってくる+よう,
      // 食べてみる+はず). Only the continuative had the row, which left the
      // others to be re-read as the lexical verb they came from just because the
      // terminal form carries this bonus and their own category does not.
      {EPOS::AuxAspectIru, EPOS::NounFormal, cost::kVeryStrongBonus},
      {EPOS::AuxAspectShimau, EPOS::NounFormal, cost::kVeryStrongBonus},
      {EPOS::AuxAspectOku, EPOS::NounFormal, cost::kVeryStrongBonus},
      {EPOS::AuxAspectMiru, EPOS::NounFormal, cost::kVeryStrongBonus},
      {EPOS::AuxAspectIku, EPOS::NounFormal, cost::kVeryStrongBonus},
      {EPOS::AuxAspectKuru, EPOS::NounFormal, cost::kVeryStrongBonus},
      {EPOS::AuxAspectHajimeru, EPOS::NounFormal, cost::kVeryStrongBonus},
      {EPOS::ParticleQuote, EPOS::NounFormal, cost::kVeryStrongBonus},
      // The obligation predicate after a quotation particle remains an
      // auxiliary chain (…ん+と+いけ+ん), rather than reopening い as an
      // independent continuative followed by the dialectal particle けん.
  };
  applyRules(table, kRules, sizeof(kRules) / sizeof(kRules[0]));
}

}  // namespace suzume::analysis::bigram_rules
