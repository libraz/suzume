#ifndef SUZUME_ANALYSIS_CONNECTION_RULES_INTERNAL_H_
#define SUZUME_ANALYSIS_CONNECTION_RULES_INTERNAL_H_

// =============================================================================
// Internal Header for Connection Rules
// =============================================================================
// This header declares internal functions used across connection rule files.
// It is NOT part of the public API.
// =============================================================================

#include "analysis/connection_rules.h"
#include "analysis/scorer_constants.h"
#include "core/lattice.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "normalize/exceptions.h"

namespace suzume::analysis {
namespace connection_rules {

// =============================================================================
// POS Pair Matchers (Step 0-0: Code reduction helpers)
// =============================================================================
// These inline functions reduce repetitive POS condition checks across
// connection rule files. Each replaces a 3-line condition block with 1 line.

// Verb connection patterns
inline bool isVerbToAux(const core::LatticeEdge& prev,
                        const core::LatticeEdge& next) {
  return prev.pos == core::PartOfSpeech::Verb &&
         next.pos == core::PartOfSpeech::Auxiliary;
}

inline bool isVerbToVerb(const core::LatticeEdge& prev,
                         const core::LatticeEdge& next) {
  return prev.pos == core::PartOfSpeech::Verb &&
         next.pos == core::PartOfSpeech::Verb;
}

inline bool isVerbToParticle(const core::LatticeEdge& prev,
                             const core::LatticeEdge& next) {
  return prev.pos == core::PartOfSpeech::Verb &&
         next.pos == core::PartOfSpeech::Particle;
}

inline bool isVerbToAdj(const core::LatticeEdge& prev,
                        const core::LatticeEdge& next) {
  return prev.pos == core::PartOfSpeech::Verb &&
         next.pos == core::PartOfSpeech::Adjective;
}

// Noun connection patterns
inline bool isNounToAux(const core::LatticeEdge& prev,
                        const core::LatticeEdge& next) {
  return prev.pos == core::PartOfSpeech::Noun &&
         next.pos == core::PartOfSpeech::Auxiliary;
}

inline bool isNounToVerb(const core::LatticeEdge& prev,
                         const core::LatticeEdge& next) {
  return prev.pos == core::PartOfSpeech::Noun &&
         next.pos == core::PartOfSpeech::Verb;
}

inline bool isNounToAdj(const core::LatticeEdge& prev,
                        const core::LatticeEdge& next) {
  return prev.pos == core::PartOfSpeech::Noun &&
         next.pos == core::PartOfSpeech::Adjective;
}

inline bool isNounToAdv(const core::LatticeEdge& prev,
                        const core::LatticeEdge& next) {
  return prev.pos == core::PartOfSpeech::Noun &&
         next.pos == core::PartOfSpeech::Adverb;
}

inline bool isNounToNoun(const core::LatticeEdge& prev,
                         const core::LatticeEdge& next) {
  return prev.pos == core::PartOfSpeech::Noun &&
         next.pos == core::PartOfSpeech::Noun;
}

// Adjective connection patterns
inline bool isAdjToVerb(const core::LatticeEdge& prev,
                        const core::LatticeEdge& next) {
  return prev.pos == core::PartOfSpeech::Adjective &&
         next.pos == core::PartOfSpeech::Verb;
}

// Auxiliary connection patterns
inline bool isAuxToAux(const core::LatticeEdge& prev,
                       const core::LatticeEdge& next) {
  return prev.pos == core::PartOfSpeech::Auxiliary &&
         next.pos == core::PartOfSpeech::Auxiliary;
}

inline bool isAuxToParticle(const core::LatticeEdge& prev,
                            const core::LatticeEdge& next) {
  return prev.pos == core::PartOfSpeech::Auxiliary &&
         next.pos == core::PartOfSpeech::Particle;
}

// Particle connection patterns
inline bool isParticleToAux(const core::LatticeEdge& prev,
                            const core::LatticeEdge& next) {
  return prev.pos == core::PartOfSpeech::Particle &&
         next.pos == core::PartOfSpeech::Auxiliary;
}

inline bool isParticleToNoun(const core::LatticeEdge& prev,
                             const core::LatticeEdge& next) {
  return prev.pos == core::PartOfSpeech::Particle &&
         next.pos == core::PartOfSpeech::Noun;
}

inline bool isParticleToOther(const core::LatticeEdge& prev,
                              const core::LatticeEdge& next) {
  return prev.pos == core::PartOfSpeech::Particle &&
         next.pos == core::PartOfSpeech::Other;
}

inline bool isParticleToParticle(const core::LatticeEdge& prev,
                                 const core::LatticeEdge& next) {
  return prev.pos == core::PartOfSpeech::Particle &&
         next.pos == core::PartOfSpeech::Particle;
}

// Prefix connection patterns
inline bool isPrefixToVerb(const core::LatticeEdge& prev,
                           const core::LatticeEdge& next) {
  return prev.pos == core::PartOfSpeech::Prefix &&
         next.pos == core::PartOfSpeech::Verb;
}

inline bool isPrefixToAdj(const core::LatticeEdge& prev,
                          const core::LatticeEdge& next) {
  return prev.pos == core::PartOfSpeech::Prefix &&
         next.pos == core::PartOfSpeech::Adjective;
}

// Symbol connection patterns
inline bool isSymbolToSuffix(const core::LatticeEdge& prev,
                             const core::LatticeEdge& next) {
  return prev.pos == core::PartOfSpeech::Symbol &&
         next.pos == core::PartOfSpeech::Suffix;
}

// =============================================================================
// Helper Functions (shared across rule files)
// =============================================================================

/**
 * @brief Check if surface is いる auxiliary form
 */
bool isIruAuxiliary(std::string_view surface);

/**
 * @brief Check if verb is an auxiliary verb pattern (補助動詞)
 * These should be treated as Auxiliary, not independent Verb
 */
bool isAuxiliaryVerbPattern(std::string_view surface, std::string_view lemma);

/**
 * @brief Check if auxiliary is verb-specific (requires verb stem, not nouns)
 * Verb-specific: ます/ましょう/ました, たい/たかった
 */
bool isVerbSpecificAuxiliary(std::string_view surface, std::string_view lemma);

// =============================================================================
// Verb Conjugation Rules (connection_rules_verb.cpp)
// =============================================================================

ConnectionRuleResult checkCopulaAfterVerb(const core::LatticeEdge& prev,
                                          const core::LatticeEdge& next);

ConnectionRuleResult checkIchidanRenyokeiTe(const core::LatticeEdge& prev,
                                            const core::LatticeEdge& next);

ConnectionRuleResult checkTeFormSplit(const core::LatticeEdge& prev,
                                      const core::LatticeEdge& next);

ConnectionRuleResult checkTaiAfterRenyokei(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next);

ConnectionRuleResult checkYasuiAfterRenyokei(const core::LatticeEdge& prev,
                                             const core::LatticeEdge& next);

ConnectionRuleResult checkNagaraSplit(const core::LatticeEdge& prev,
                                      const core::LatticeEdge& next);

ConnectionRuleResult checkSouAfterRenyokei(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next);

ConnectionRuleResult checkCompoundAuxAfterRenyokei(const core::LatticeEdge& prev,
                                                   const core::LatticeEdge& next);

ConnectionRuleResult checkTakuteAfterRenyokei(const core::LatticeEdge& prev,
                                              const core::LatticeEdge& next);

ConnectionRuleResult checkTakuTeSplit(const core::LatticeEdge& prev,
                                      const core::LatticeEdge& next);

ConnectionRuleResult checkConditionalVerbToVerb(const core::LatticeEdge& prev,
                                                const core::LatticeEdge& next);

ConnectionRuleResult checkVerbRenyokeiCompoundAux(const core::LatticeEdge& prev,
                                                  const core::LatticeEdge& next);

ConnectionRuleResult checkTeFormVerbToVerb(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next);

ConnectionRuleResult checkPrefixBeforeVerb(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next);

ConnectionRuleResult checkTokuContractionSplit(const core::LatticeEdge& prev,
                                               const core::LatticeEdge& next);

// =============================================================================
// Auxiliary Connection Rules (connection_rules_aux.cpp)
// =============================================================================

ConnectionRuleResult checkIruAuxAfterNoun(const core::LatticeEdge& prev,
                                          const core::LatticeEdge& next);

ConnectionRuleResult checkIruAuxAfterTeForm(const core::LatticeEdge& prev,
                                            const core::LatticeEdge& next);

ConnectionRuleResult checkMasenDeSplit(const core::LatticeEdge& prev,
                                       const core::LatticeEdge& next);

ConnectionRuleResult checkNounBeforeVerbAux(const core::LatticeEdge& prev,
                                            const core::LatticeEdge& next);

ConnectionRuleResult checkMaiAfterNoun(const core::LatticeEdge& prev,
                                       const core::LatticeEdge& next);

ConnectionRuleResult checkAuxAfterParticle(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next);

ConnectionRuleResult checkMitaiAfterNounOrVerb(const core::LatticeEdge& prev,
                                               const core::LatticeEdge& next);

ConnectionRuleResult checkInvalidTeFormAux(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next);

// =============================================================================
// Other Connection Rules (connection_rules_other.cpp)
// =============================================================================

ConnectionRuleResult checkAdjKuNaru(const core::LatticeEdge& prev,
                                    const core::LatticeEdge& next);

ConnectionRuleResult checkPrefixToShortStemHiraganaAdj(
    const core::LatticeEdge& prev, const core::LatticeEdge& next);

ConnectionRuleResult checkCharacterSpeechSplit(const core::LatticeEdge& prev,
                                               const core::LatticeEdge& next);

ConnectionRuleResult checkYoruNightAfterNi(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next);

ConnectionRuleResult checkFormalNounBeforeKanji(const core::LatticeEdge& prev,
                                                const core::LatticeEdge& next);

ConnectionRuleResult checkSameParticleRepeated(const core::LatticeEdge& prev,
                                               const core::LatticeEdge& next);

ConnectionRuleResult checkHiraganaNounStartsWithParticle(
    const core::LatticeEdge& prev, const core::LatticeEdge& next);

ConnectionRuleResult checkSuffixAfterSymbol(const core::LatticeEdge& prev,
                                            const core::LatticeEdge& next);

ConnectionRuleResult checkParticleBeforeHiraganaOther(
    const core::LatticeEdge& prev, const core::LatticeEdge& next);

ConnectionRuleResult checkShiParticleConnection(const core::LatticeEdge& prev,
                                                const core::LatticeEdge& next);

ConnectionRuleResult checkRashiiAfterPredicate(const core::LatticeEdge& prev,
                                               const core::LatticeEdge& next);

}  // namespace connection_rules
}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_CONNECTION_RULES_INTERNAL_H_
