#ifndef SUZUME_ANALYSIS_CONNECTION_RULES_INTERNAL_H_
#define SUZUME_ANALYSIS_CONNECTION_RULES_INTERNAL_H_

// =============================================================================
// Internal Header for Connection Rules
// =============================================================================
// This header declares internal functions used across connection rule files.
// It is NOT part of the public API.
// =============================================================================

#include "analysis/connection_rule_options.h"
#include "analysis/connection_rules.h"
#include "analysis/scorer_constants.h"
#include "core/lattice.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"

namespace suzume::analysis {
namespace connection_rules {

// =============================================================================
// POS Pair Matcher Template
// =============================================================================
// Single template replacing 23 individual inline functions.
// Usage: isPosMatch<POS::Verb, POS::Aux>(prev, next)

namespace POS {
using core::PartOfSpeech;
constexpr auto Verb = PartOfSpeech::Verb;
constexpr auto Noun = PartOfSpeech::Noun;
constexpr auto Adj = PartOfSpeech::Adjective;
constexpr auto Adv = PartOfSpeech::Adverb;
constexpr auto Aux = PartOfSpeech::Auxiliary;
constexpr auto Particle = PartOfSpeech::Particle;
constexpr auto Prefix = PartOfSpeech::Prefix;
constexpr auto Suffix = PartOfSpeech::Suffix;
constexpr auto Symbol = PartOfSpeech::Symbol;
constexpr auto Other = PartOfSpeech::Other;
}  // namespace POS

template <core::PartOfSpeech P1, core::PartOfSpeech P2>
inline constexpr bool isPosMatch(const core::LatticeEdge& prev,
                                 const core::LatticeEdge& next) {
  return prev.pos == P1 && next.pos == P2;
}

// =============================================================================
// Connection Rule Function Type
// =============================================================================
// All connection rule functions share this signature.
using RuleFunc = ConnectionRuleResult(const core::LatticeEdge& prev,
                                      const core::LatticeEdge& next,
                                      const ConnectionOptions& opts);

// =============================================================================
// Helper Functions (shared across rule files)
// =============================================================================

/**
 * @brief Check if surface is いる auxiliary form
 */
bool isIruAuxiliary(std::string_view surface);

/**
 * @brief Check if surface is しまう auxiliary form
 * Includes full forms (しまう, しまった) and contracted forms (ちゃう, じゃう)
 */
bool isShimauAuxiliary(std::string_view surface);

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

/**
 * @brief Check if edge is bare suru te-form "して"
 * MeCab splits suru te-form: している → し + て + いる
 * This helper identifies the bare "して" pattern that should not get
 * te-form connection bonuses.
 */
inline bool isBareSuruTeForm(const core::LatticeEdge& edge) {
  return edge.surface == "して" && edge.lemma == scorer::kLemmaSuru;
}

/**
 * @brief Check if surface starts with a CJK kanji character
 * Checks UTF-8 first byte range 0xE4-0xE9 which covers most CJK Unified
 * Ideographs used in Japanese (U+4E00-U+9FFF).
 */
inline bool startsWithKanji(std::string_view surface) {
  if (surface.empty()) return false;
  auto first_byte = static_cast<unsigned char>(surface[0]);
  return first_byte >= 0xE4 && first_byte <= 0xE9;
}

// =============================================================================
// Verb Conjugation Rules (connection_rules_verb.cpp)
// =============================================================================
RuleFunc checkCopulaAfterVerb;
RuleFunc checkOnbinkeiToVoicedTa;
RuleFunc checkOnbinkeiToTara;
RuleFunc checkOnbinkeiToTa;
RuleFunc checkIchidanRenyokeiTe;
RuleFunc checkTeFormSplit;
RuleFunc checkTaiAfterRenyokei;
RuleFunc checkTaAfterRenyokei;
RuleFunc checkYasuiAfterRenyokei;
RuleFunc checkNagaraSplit;
RuleFunc checkKataAfterRenyokei;
RuleFunc checkSouAfterRenyokei;
RuleFunc checkCompoundAuxAfterRenyokei;
RuleFunc checkTakuteAfterRenyokei;
RuleFunc checkTakuTeSplit;
RuleFunc checkConditionalVerbToVerb;
RuleFunc checkVerbRenyokeiCompoundAux;
RuleFunc checkTeFormVerbToVerb;
RuleFunc checkPrefixBeforeVerb;
RuleFunc checkTokuContractionSplit;
RuleFunc checkTekuReMissegmentation;

// =============================================================================
// Auxiliary Connection Rules (connection_rules_aux.cpp)
// =============================================================================
RuleFunc checkIruAuxAfterNoun;
RuleFunc checkIruAuxAfterTeForm;
RuleFunc checkShimauAuxAfterTeForm;
RuleFunc checkSouAuxAfterVerbRenyokei;
RuleFunc checkMasenDeSplit;
RuleFunc checkNounBeforeVerbAux;
RuleFunc checkMaiAfterNoun;
RuleFunc checkNounIRowToVerbAux;
RuleFunc checkAuxAfterParticle;
RuleFunc checkMitaiAfterNounOrVerb;
RuleFunc checkInvalidTeFormAux;

// =============================================================================
// Other Connection Rules (connection_rules_other.cpp)
// =============================================================================
RuleFunc checkAdjKuNaru;
RuleFunc checkAdjKuToTeParticle;
RuleFunc checkAdjKuToNai;
RuleFunc checkIAdjToDesu;
RuleFunc checkAdjStemToSugiruVerb;
RuleFunc checkAdjStemToSouAux;
RuleFunc checkVerbRenyokeiToSouAux;
RuleFunc checkPrefixToHiraganaAdj;
RuleFunc checkCharacterSpeechSplit;
RuleFunc checkYoruNightAfterNi;
RuleFunc checkFormalNounBeforeKanji;
RuleFunc checkSameParticleRepeated;
RuleFunc checkSuspiciousParticleSequence;
RuleFunc checkHiraganaNounStartsWithParticle;
RuleFunc checkSuffixAfterSymbol;
RuleFunc checkSuffixAfterNaParticle;
RuleFunc checkParticleBeforeHiraganaOther;
RuleFunc checkParticleBeforeHiraganaVerb;
RuleFunc checkShiParticleConnection;
RuleFunc checkRashiiAfterPredicate;
RuleFunc checkVerbToCaseParticle;
RuleFunc checkSuruRenyokeiToTeVerb;
RuleFunc checkNaParticleAfterKanjiNoun;
RuleFunc checkKuraiAdjectiveAfterPredicate;
RuleFunc checkParticleNiToIruVerb;
RuleFunc checkMasuRenyokeiToTa;
RuleFunc checkNaiRenyokeiToTa;
RuleFunc checkTaiRenyokeiToTa;
RuleFunc checkDesuRenyokeiToTa;
RuleFunc checkInvalidTaToI;             // Penalty for AUX(た) → AUX(い)
RuleFunc checkNaiAfterVerbMizenkei;
RuleFunc checkPassiveAfterVerbMizenkei;
RuleFunc checkShireruToMasuNai;
RuleFunc checkRenyokeiToContractedVerb;
RuleFunc checkRenyokeiToTeParticle;
RuleFunc checkTeParticleToAuxVerb;
RuleFunc checkTeParticleToInaiVerb;
RuleFunc checkTeVerbToAuxNegative;
RuleFunc checkPassiveAuxToNaiTa;
RuleFunc checkVerbToOkuChauContraction;
RuleFunc checkParticleDeToKuruAux;      // Penalty: で(PARTICLE) → くる活用形
RuleFunc checkCopulaDeToKuruAux;        // Penalty: で(AUX,だ) → くる活用形
RuleFunc checkNaAdjToCopulaDe;          // Bonus: ADJ → で(AUX,だ)
RuleFunc checkNaAdjToDekinaiVerb;       // Penalty: NOUN/ADJ → でない(VERB)
RuleFunc checkCopulaDeToNai;            // Bonus: で(AUX,だ) → ない(AUX)
RuleFunc checkCopulaDeToGozaru;         // Bonus: で(AUX,だ) → ござる(AUX)
RuleFunc checkCopulaDeToAru;            // Bonus: で(AUX,だ) → ある/あり(VERB)
RuleFunc checkQuotativeAdvToIu;         // Bonus: ADV(そう/こう/etc) → いっ(VERB, lemma=いう)
RuleFunc checkNiParticleToIku;          // Bonus: に(PARTICLE) → いって/いった(VERB, lemma=いく)
RuleFunc checkSentenceFinalParticleSeq; // Bonus: 終助詞 → 終助詞 (よ+ね, よ+わ, etc.)

// =============================================================================
// POS-based Dispatch Helpers
// =============================================================================
// These functions group rules by prev.pos for efficient dispatch.
// Each function only evaluates rules that can match the given prev.pos.

/**
 * @brief Evaluate rules where prev.pos == VERB
 * Rules: CopulaAfterVerb, IchidanRenyokeiTe, TeFormSplit, TaiAfterRenyokei,
 *        NagaraSplit, TakuteAfterRenyokei, TakuTeSplit, ConditionalVerbToVerb,
 *        VerbRenyokeiCompoundAux, TeFormVerbToVerb, TokuContractionSplit,
 *        RashiiAfterPredicate, VerbToCaseParticle, IruAuxAfterTeForm,
 *        InvalidTeFormAux, ShiParticleConnection, MitaiAfterNounOrVerb
 */
void evaluateVerbRules(const core::LatticeEdge& prev,
                       const core::LatticeEdge& next,
                       const ConnectionOptions& opts,
                       ConnectionRuleResult& accumulated);

/**
 * @brief Evaluate rules where prev.pos == NOUN
 * Rules: TeFormSplit, YasuiAfterRenyokei, SouAfterRenyokei,
 *        CompoundAuxAfterRenyokei, IruAuxAfterNoun, NounBeforeVerbAux,
 *        MaiAfterNoun, HiraganaNounStartsWithParticle, MitaiAfterNounOrVerb,
 *        ShiParticleConnection
 */
void evaluateNounRules(const core::LatticeEdge& prev,
                       const core::LatticeEdge& next,
                       const ConnectionOptions& opts,
                       ConnectionRuleResult& accumulated);

/**
 * @brief Evaluate rules where prev.pos == ADJECTIVE
 * Rules: TakuTeSplit, RashiiAfterPredicate, AdjKuNaru, ShiParticleConnection
 */
void evaluateAdjRules(const core::LatticeEdge& prev,
                      const core::LatticeEdge& next,
                      const ConnectionOptions& opts,
                      ConnectionRuleResult& accumulated);

/**
 * @brief Evaluate rules where prev.pos == AUXILIARY
 * Rules: TaiAfterRenyokei, CharacterSpeechSplit, MasenDeSplit,
 *        ShiParticleConnection
 */
void evaluateAuxRules(const core::LatticeEdge& prev,
                      const core::LatticeEdge& next,
                      const ConnectionOptions& opts,
                      ConnectionRuleResult& accumulated);

/**
 * @brief Evaluate rules where prev.pos == PARTICLE
 * Rules: AuxAfterParticle, YoruNightAfterNi, SameParticleRepeated,
 *        ParticleBeforeHiraganaOther, ParticleBeforeHiraganaVerb,
 *        ParticleBeforeHiraganaAdj
 */
void evaluateParticleRules(const core::LatticeEdge& prev,
                           const core::LatticeEdge& next,
                           const ConnectionOptions& opts,
                           ConnectionRuleResult& accumulated);

/**
 * @brief Evaluate rules where prev.pos == PREFIX
 * Rules: PrefixBeforeVerb, PrefixToHiraganaAdj
 */
void evaluatePrefixRules(const core::LatticeEdge& prev,
                         const core::LatticeEdge& next,
                         const ConnectionOptions& opts,
                         ConnectionRuleResult& accumulated);

/**
 * @brief Evaluate rules where prev.pos == SYMBOL
 * Rules: SuffixAfterSymbol
 */
void evaluateSymbolRules(const core::LatticeEdge& prev,
                         const core::LatticeEdge& next,
                         const ConnectionOptions& opts,
                         ConnectionRuleResult& accumulated);

}  // namespace connection_rules
}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_CONNECTION_RULES_INTERNAL_H_
