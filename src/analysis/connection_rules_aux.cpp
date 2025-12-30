#include "analysis/connection_rules_internal.h"

namespace suzume::analysis {
namespace connection_rules {

// =============================================================================
// Helper Functions
// =============================================================================

bool isIruAuxiliary(std::string_view surface) {
  // Full forms
  if (surface == "いる" || surface == "います" || surface == "いました" ||
      surface == "いません" || surface == "いない" ||
      surface == "いなかった" || surface == "いれば") {
    return true;
  }
  // Contracted forms (てる/でる = ている contraction)
  if (surface == "てる" || surface == "てた" || surface == "てて" ||
      surface == "てない" || surface == "てなかった" ||
      surface == "でる" || surface == "でた" || surface == "でて" ||
      surface == "でない" || surface == "でなかった") {
    return true;
  }
  return false;
}

bool isVerbSpecificAuxiliary(std::string_view surface, std::string_view lemma) {
  // ます form auxiliaries (require masu-stem)
  if (surface.size() >= core::kTwoJapaneseCharBytes) {
    std::string_view first6 = surface.substr(0, core::kTwoJapaneseCharBytes);
    if (first6 == "ます" || first6 == "まし" || first6 == "ませ") {
      return true;
    }
  }
  // Check lemma for ます
  if (lemma == "ます") {
    return true;
  }
  // たい form (desire) - always verb-specific
  if (lemma == "たい") {
    return true;
  }
  return false;
}

// =============================================================================
// Auxiliary Connection Rules
// =============================================================================

// Rule 15: NOUN + いる/います (AUX) penalty
// いる auxiliary should only follow te-form verbs, not nouns
ConnectionRuleResult checkIruAuxAfterNoun(const core::LatticeEdge& prev,
                                          const core::LatticeEdge& next) {
  if (!isNounToAux(prev, next)) return {};

  if (!isIruAuxiliary(next.surface)) return {};

  return {ConnectionPattern::IruAuxAfterNoun, scorer::kPenaltyIruAuxAfterNoun,
          "iru aux after noun (should be verb)"};
}

// Rule 16: Te-form VERB + いる/います (AUX) bonus
// Progressive aspect pattern: 食べている, 走っています
ConnectionRuleResult checkIruAuxAfterTeForm(const core::LatticeEdge& prev,
                                            const core::LatticeEdge& next) {
  if (!isVerbToAux(prev, next)) return {};

  if (!isIruAuxiliary(next.surface)) return {};

  // Check if prev ends with te-form (て or で)
  if (prev.surface.size() < core::kJapaneseCharBytes) {
    return {};
  }
  std::string_view last_char = prev.surface.substr(prev.surface.size() - core::kJapaneseCharBytes);
  if (last_char != "て" && last_char != "で") {
    return {};
  }

  // Bonus (negative value) for te-form + iru pattern
  return {ConnectionPattern::IruAuxAfterTeForm, -scorer::kBonusIruAuxAfterTeForm,
          "te-form verb + iru aux (progressive)"};
}

// Rule 17: Te-form VERB + invalid single-char AUX penalty
// Single-character AUX like "る" after te-form is usually wrong
// E.g., "してる" should NOT be split as "して" + "る"
// Valid single-char patterns: only when part of proper iru contraction
ConnectionRuleResult checkInvalidTeFormAux(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next) {
  if (!isVerbToAux(prev, next)) return {};

  // Check if prev ends with te-form (て or で)
  if (prev.surface.size() < core::kJapaneseCharBytes) {
    return {};
  }
  std::string_view last_char = prev.surface.substr(prev.surface.size() - core::kJapaneseCharBytes);
  if (last_char != "て" && last_char != "で") {
    return {};
  }

  // Check if next is single-character hiragana AUX that's NOT valid iru auxiliary
  if (next.surface.size() == core::kJapaneseCharBytes) {
    // Single-character auxiliary after te-form
    // Only valid patterns: part of contracted forms handled elsewhere
    // Invalid: standalone "る", "た" that should be part of てる/てた
    if (next.surface == "る") {
      return {ConnectionPattern::InvalidTeFormAux,
              scorer::kPenaltyInvalidSingleCharAux,
              "invalid single-char aux after te-form"};
    }
    // "た" after te-form is likely contracted -ていた form (買ってた, 見てた)
    // Add penalty to prefer unified てた analysis over split て + た
    // This handles the colloquial contraction ている → てる, ていた → てた
    if (next.surface == "た") {
      return {ConnectionPattern::InvalidTeFormAux,
              scorer::kPenaltyTeFormTaContraction,
              "te-form + ta (likely contracted teita)"};
    }
  }

  return {};
}

// Rule 13: AUX(ません形) + で(PARTICLE) split penalty
// Prevents ございません + で + した from being preferred over ございません + でした
// The で after negative polite forms should be part of でした (copula past)
ConnectionRuleResult checkMasenDeSplit(const core::LatticeEdge& prev,
                                       const core::LatticeEdge& next) {
  if (!isAuxToParticle(prev, next)) return {};

  if (next.surface != "で") return {};

  // Check if prev ends with ません (negative polite form)
  // UTF-8: ません = 9 bytes (3 hiragana chars)
  if (prev.surface.size() < core::kThreeJapaneseCharBytes) {
    return {};
  }
  std::string_view last9 = prev.surface.substr(prev.surface.size() - core::kThreeJapaneseCharBytes);
  if (last9 != "ません") {
    return {};
  }

  return {ConnectionPattern::MasenDeSplit, scorer::kPenaltyMasenDeSplit,
          "masen + de split (should be masen + deshita)"};
}

// Rule: NOUN + verb-specific AUX penalty
// Verb auxiliaries like ます/ましょう/たい require verb stem, not nouns
// E.g., 行き(NOUN) + ましょう is invalid - should be 行き(VERB) + ましょう
ConnectionRuleResult checkNounBeforeVerbAux(const core::LatticeEdge& prev,
                                            const core::LatticeEdge& next) {
  if (!isNounToAux(prev, next)) return {};

  if (!isVerbSpecificAuxiliary(next.surface, next.lemma)) return {};

  return {ConnectionPattern::NounBeforeVerbAux, scorer::kPenaltyNounBeforeVerbAux,
          "noun before verb-specific aux"};
}

// Rule: NOUN + まい(AUX) penalty
// まい (negative conjecture) attaches to verb stems:
// - Godan 終止形: 行くまい, 書くまい
// - Ichidan 未然形: 食べまい, 出来まい
// NOUN + まい is grammatically invalid - should be VERB stem + まい
ConnectionRuleResult checkMaiAfterNoun(const core::LatticeEdge& prev,
                                       const core::LatticeEdge& next) {
  if (!isNounToAux(prev, next)) return {};

  if (next.surface != "まい") return {};

  // Penalty to prefer verb stem + まい over noun + まい
  return {ConnectionPattern::NounBeforeVerbAux, scorer::kPenaltyNounMai,
          "mai aux after noun (should be verb stem)"};
}

// Check for invalid PARTICLE + AUX pattern
// Auxiliaries (助動詞) attach to verb/adjective stems, not particles
// PARTICLE + AUX is grammatically invalid in most cases
// Examples of invalid patterns:
//   と + う (particle + volitional)
//   に + た (particle + past)
//   を + ない (particle + negative)
// Note: Long dictionary AUX (like なかった) after particles is valid
ConnectionRuleResult checkAuxAfterParticle(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next) {
  if (!isParticleToAux(prev, next)) return {};

  // Don't penalize long dictionary AUX (2+ chars) - valid patterns
  // e.g., は + なかった, で + ある
  if (next.fromDictionary() && next.surface.size() > core::kJapaneseCharBytes) {
    return {};
  }

  // Penalize short/unknown AUX after particle
  return {ConnectionPattern::ParticleBeforeAux,
          scorer::kPenaltyShortAuxAfterParticle,
          "short/unknown aux after particle (likely split)"};
}

// Check for NOUN/VERB → みたい (ADJ) pattern
// みたい is a na-adjective meaning "like ~" or "seems like ~"
// Valid patterns:
//   - NOUN + みたい: 猫みたい (like a cat)
//   - VERB終止形 + みたい: 食べるみたい (seems like eating)
// Without this bonus, unknown words like "猫みたい" may be parsed as a single VERB
ConnectionRuleResult checkMitaiAfterNounOrVerb(const core::LatticeEdge& prev,
                                               const core::LatticeEdge& next) {
  if (next.pos != core::PartOfSpeech::Adjective || next.surface != "みたい") {
    return {};
  }

  // Bonus for NOUN + みたい (strong bonus to beat unknown verb analysis)
  if (prev.pos == core::PartOfSpeech::Noun) {
    return {ConnectionPattern::NounBeforeNaAdj, -scorer::kBonusNounMitai,
            "noun + mitai (resemblance pattern)"};
  }

  // Bonus for VERB + みたい (終止形/連体形)
  if (prev.pos == core::PartOfSpeech::Verb) {
    return {ConnectionPattern::VerbBeforeNaAdj, -scorer::kBonusVerbMitai,
            "verb + mitai (hearsay/appearance pattern)"};
  }

  return {};
}

}  // namespace connection_rules
}  // namespace suzume::analysis
