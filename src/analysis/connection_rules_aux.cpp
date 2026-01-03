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

bool isShimauAuxiliary(std::string_view surface) {
  // Full forms (五段ワ行活用)
  if (surface == "しまう" || surface == "しまった" || surface == "しまって" ||
      surface == "しまいます" || surface == "しまいました" ||
      surface == "しまいません" || surface == "しまわない" ||
      surface == "しまわなかった" || surface == "しまえば") {
    return true;
  }
  // Contracted forms: ちゃう/じゃう = てしまう/でしまう
  if (surface == "ちゃう" || surface == "ちゃった" || surface == "ちゃって" ||
      surface == "ちゃいます" || surface == "ちゃいました" ||
      surface == "じゃう" || surface == "じゃった" || surface == "じゃって" ||
      surface == "じゃいます" || surface == "じゃいました") {
    return true;
  }
  return false;
}

bool isVerbSpecificAuxiliary(std::string_view surface, std::string_view lemma) {
  // ます form auxiliaries (require masu-stem)
  if (surface.size() >= core::kTwoJapaneseCharBytes) {
    std::string_view first6 = surface.substr(0, core::kTwoJapaneseCharBytes);
    if (first6 == scorer::kLemmaMasu ||
        first6 == "まし" || first6 == "ませ") {  // masu conjugations
      return true;
    }
  }
  // Check lemma for ます
  if (lemma == scorer::kLemmaMasu) {
    return true;
  }
  // たい form (desire) - always verb-specific
  if (lemma == scorer::kSuffixTai) {
    return true;
  }
  // そう form (appearance auxiliary) - requires verb renyokei
  // PARTICLE + そう(AUX) is invalid; そう as adverb is the correct interpretation
  // E.g., にそう should be に + そう(ADV), not PARTICLE + そう(AUX)
  if (lemma == scorer::kSuffixSou && surface == scorer::kSuffixSou) {
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
                                          const core::LatticeEdge& next,
                                          const ConnectionOptions& opts) {
  if (!isNounToAux(prev, next)) return {};

  if (!isIruAuxiliary(next.surface)) return {};

  return {ConnectionPattern::IruAuxAfterNoun, opts.penalty_iru_aux_after_noun,
          "iru aux after noun (should be verb)"};
}

// Rule 16: Te-form VERB + いる/います (AUX) bonus
// Progressive aspect pattern: 食べている, 走っています
ConnectionRuleResult checkIruAuxAfterTeForm(const core::LatticeEdge& prev,
                                            const core::LatticeEdge& next,
                                            const ConnectionOptions& opts) {
  if (!isVerbToAux(prev, next)) return {};

  if (!isIruAuxiliary(next.surface)) return {};

  if (!endsWithTeForm(prev.surface)) return {};

  // Bonus (negative value) for te-form + iru pattern
  return {ConnectionPattern::IruAuxAfterTeForm, -opts.bonus_iru_aux_after_te_form,
          "te-form verb + iru aux (progressive)"};
}

// Rule: Te-form VERB + しまう/しまった (AUX) bonus
// Completive/Regretful aspect pattern: 食べてしまった, 忘れてしまった
ConnectionRuleResult checkShimauAuxAfterTeForm(const core::LatticeEdge& prev,
                                               const core::LatticeEdge& next,
                                               const ConnectionOptions& opts) {
  if (!isVerbToAux(prev, next)) return {};

  if (!isShimauAuxiliary(next.surface)) return {};

  if (!endsWithTeForm(prev.surface)) return {};

  // Bonus (negative value) for te-form + shimau pattern
  return {ConnectionPattern::ShimauAuxAfterTeForm,
          -opts.bonus_shimau_aux_after_te_form,
          "te-form verb + shimau aux (completive/regretful)"};
}

// Rule: VERB renyokei + そう (AUX) bonus
// Appearance auxiliary pattern: 降りそう (looks like falling), 切れそう (looks like breaking)
// Gives bonus to help AUX beat ADV when preceded by verb renyokei form
ConnectionRuleResult checkSouAuxAfterVerbRenyokei(const core::LatticeEdge& prev,
                                                   const core::LatticeEdge& next,
                                                   const ConnectionOptions& opts) {
  if (!isVerbToAux(prev, next)) return {};

  if (next.surface != scorer::kSuffixSou || next.lemma != scorer::kSuffixSou) return {};

  // Verb must end with renyokei marker (i-row, e-row for ichidan, or onbin markers)
  if (!endsWithRenyokeiMarker(prev.surface)) return {};

  // For し-ending verbs that are NOT verified (hasSuffix=false), apply reduced
  // bonus so that i-adjective candidates (美味しそう→美味しい) can compete.
  // Verified verbs (hasSuffix=true) get the full bonus.
  // Unverified し-ending verbs might be fake suru-verbs (美味する) that should
  // lose to adjective candidates.
  bool ends_with_shi = utf8::endsWith(prev.surface, scorer::kSuffixShi);
  bool is_shi_producing_verb = (prev.conj_type == dictionary::ConjugationType::Suru ||
                                 prev.conj_type == dictionary::ConjugationType::GodanSa);
  bool is_unverified = !prev.hasSuffix();

  if (ends_with_shi && is_shi_producing_verb && is_unverified) {
    // Reduced bonus for unverified し-ending verbs
    // Balance: AUX must beat ADV (-0.044), but ADJ (-0.165) must beat AUX
    // With bonus=0.25: AUX=0.156-0.25=-0.094 beats ADV (-0.044 > -0.094)
    // And ADJ beats AUX: -0.165 < -0.094
    return {ConnectionPattern::SouAfterRenyokei, -0.25F,
            "verb renyokei + sou aux (unverified shi-verb, reduced)"};
  }

  // Full bonus for verified verbs and non-し patterns
  return {ConnectionPattern::SouAfterRenyokei, -opts.bonus_sou_aux_after_renyokei,
          "verb renyokei + sou aux (appearance)"};
}

// Rule 17: Te-form VERB + invalid single-char AUX penalty
// Single-character AUX like "る" after te-form is usually wrong
// E.g., "してる" should NOT be split as "して" + "る"
// Valid single-char patterns: only when part of proper iru contraction
ConnectionRuleResult checkInvalidTeFormAux(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next,
                                           const ConnectionOptions& opts) {
  if (!isVerbToAux(prev, next)) return {};

  if (!endsWithTeForm(prev.surface)) return {};

  // Check if next is single-character hiragana AUX that's NOT valid iru auxiliary
  if (next.surface.size() == core::kJapaneseCharBytes) {
    // Single-character auxiliary after te-form
    // Only valid patterns: part of contracted forms handled elsewhere
    // Invalid: standalone る, た that should be part of てる/てた
    if (next.surface == scorer::kFormRu) {
      return {ConnectionPattern::InvalidTeFormAux,
              opts.penalty_invalid_single_char_aux,
              "invalid single-char aux after te-form"};
    }
    // た after te-form is likely contracted -ていた form (買ってた, 見てた)
    // Add penalty to prefer unified てた analysis over split て + た
    // This handles the colloquial contraction ている → てる, ていた → てた
    if (next.surface == scorer::kFormTa) {
      return {ConnectionPattern::InvalidTeFormAux,
              opts.penalty_te_form_ta_contraction,
              "te-form + ta (likely contracted teita)"};
    }
  }

  return {};
}

// Rule 13: AUX(ません形) + で(PARTICLE) split penalty
// Prevents ございません + で + した from being preferred over ございません + でした
// The で after negative polite forms should be part of でした (copula past)
ConnectionRuleResult checkMasenDeSplit(const core::LatticeEdge& prev,
                                       const core::LatticeEdge& next,
                                       const ConnectionOptions& opts) {
  if (!isAuxToParticle(prev, next)) return {};

  if (next.surface != scorer::kFormDe) return {};

  // Check if prev ends with ません (negative polite form)
  // UTF-8: ません = 9 bytes (3 hiragana chars)
  if (prev.surface.size() < core::kThreeJapaneseCharBytes) {
    return {};
  }
  std::string_view last9 = prev.surface.substr(prev.surface.size() - core::kThreeJapaneseCharBytes);
  if (last9 != scorer::kSuffixMasen) {
    return {};
  }

  return {ConnectionPattern::MasenDeSplit, opts.penalty_masen_de_split,
          "masen + de split (should be masen + deshita)"};
}

// Rule: NOUN + verb-specific AUX penalty
// Verb auxiliaries like ます/ましょう/たい require verb stem, not nouns
// E.g., 行き(NOUN) + ましょう is invalid - should be 行き(VERB) + ましょう
ConnectionRuleResult checkNounBeforeVerbAux(const core::LatticeEdge& prev,
                                            const core::LatticeEdge& next,
                                            const ConnectionOptions& opts) {
  if (!isNounToAux(prev, next)) return {};

  if (!isVerbSpecificAuxiliary(next.surface, next.lemma)) return {};

  return {ConnectionPattern::NounBeforeVerbAux, opts.penalty_noun_before_verb_aux,
          "noun before verb-specific aux"};
}

// Rule: NOUN + まい(AUX) penalty
// まい (negative conjecture) attaches to verb stems:
// - Godan 終止形: 行くまい, 書くまい
// - Ichidan 未然形: 食べまい, 出来まい
// NOUN + まい is grammatically invalid - should be VERB stem + まい
ConnectionRuleResult checkMaiAfterNoun(const core::LatticeEdge& prev,
                                       const core::LatticeEdge& next,
                                       const ConnectionOptions& opts) {
  if (!isNounToAux(prev, next)) return {};

  if (next.surface != scorer::kLemmaMai) return {};

  // Penalty to prefer verb stem + まい over noun + まい
  return {ConnectionPattern::NounBeforeVerbAux, opts.penalty_noun_mai,
          "mai aux after noun (should be verb stem)"};
}

// Rule: NOUN (i-row ending) + る/て/た(AUX) penalty
// When a noun ends with i-row hiragana (じ, み, び, etc.) and is followed by
// る/て/た(AUX), it's likely a misanalyzed ichidan verb (e.g., 感じ+る → 感じる).
// Nouns cannot take verb conjugation suffixes - this is grammatically invalid.
// Exception: だ/です copula is valid after nouns (handled by separate rule)
ConnectionRuleResult checkNounIRowToVerbAux(const core::LatticeEdge& prev,
                                            const core::LatticeEdge& next,
                                            const ConnectionOptions& opts) {
  if (!isNounToAux(prev, next)) return {};

  // Target verb conjugation markers: る (terminal), て (te-form), た (past)
  // These are verb suffixes that nouns cannot take
  if (next.surface != scorer::kFormRu &&
      next.surface != scorer::kFormTe &&
      next.surface != scorer::kFormTa) {
    return {};
  }

  // Check if noun ends with i-row hiragana (ichidan stem pattern)
  if (!endsWithIRow(prev.surface)) return {};

  // Strong penalty to prefer verb interpretation over NOUN + る/て/た split
  return {ConnectionPattern::NounBeforeVerbAux, opts.penalty_noun_irow_to_verb_aux,
          "noun (i-row) + ru/te/ta aux (likely ichidan verb)"};
}

// Check for invalid PARTICLE + AUX pattern
// Auxiliaries (助動詞) attach to verb/adjective stems, not particles
// PARTICLE + AUX is grammatically invalid in most cases
// Examples of invalid patterns:
//   と + う (particle + volitional)
//   に + た (particle + past)
//   を + ない (particle + negative)
//   ね + たい (particle + desire) - should be 寝たい (want to sleep)
// Note: Long dictionary AUX (like なかった, である) after particles can be valid
ConnectionRuleResult checkAuxAfterParticle(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next,
                                           const ConnectionOptions& opts) {
  if (!isParticleToAux(prev, next)) return {};

  // Verb-specific auxiliaries (たい, ます, etc.) require verb 連用形
  // These are ALWAYS invalid after particles, even if from dictionary
  // e.g., ね + たい is invalid (should be 寝たい from verb 寝る)
  if (isVerbSpecificAuxiliary(next.surface, next.lemma)) {
    return {ConnectionPattern::ParticleBeforeAux,
            opts.penalty_short_aux_after_particle,
            "verb-specific aux after particle (grammatically invalid)"};
  }

  // Don't penalize long dictionary AUX (2+ chars) - valid patterns
  // e.g., は + なかった, で + ある
  if (next.fromDictionary() && next.surface.size() > core::kJapaneseCharBytes) {
    return {};
  }

  // Penalize short/unknown AUX after particle
  return {ConnectionPattern::ParticleBeforeAux,
          opts.penalty_short_aux_after_particle,
          "short/unknown aux after particle (likely split)"};
}

// Check for NOUN/VERB → みたい (ADJ) pattern
// みたい is a na-adjective meaning "like ~" or "seems like ~"
// Valid patterns:
//   - NOUN + みたい: 猫みたい (like a cat)
//   - VERB終止形 + みたい: 食べるみたい (seems like eating)
// Without this bonus, unknown words like "猫みたい" may be parsed as a single VERB
ConnectionRuleResult checkMitaiAfterNounOrVerb(const core::LatticeEdge& prev,
                                               const core::LatticeEdge& next,
                                               const ConnectionOptions& opts) {
  if (next.pos != core::PartOfSpeech::Adjective || next.surface != "みたい") {
    return {};
  }

  // Bonus for NOUN + みたい (strong bonus to beat unknown verb analysis)
  if (prev.pos == core::PartOfSpeech::Noun) {
    return {ConnectionPattern::NounBeforeNaAdj, -opts.bonus_noun_mitai,
            "noun + mitai (resemblance pattern)"};
  }

  // Bonus for VERB + みたい (終止形/連体形)
  if (prev.pos == core::PartOfSpeech::Verb) {
    return {ConnectionPattern::VerbBeforeNaAdj, -opts.bonus_verb_mitai,
            "verb + mitai (hearsay/appearance pattern)"};
  }

  return {};
}

}  // namespace connection_rules
}  // namespace suzume::analysis
