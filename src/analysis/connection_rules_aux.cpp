#include "analysis/connection_rules_internal.h"

#include <algorithm>

namespace suzume::analysis {
namespace connection_rules {

// =============================================================================
// Anonymous Namespace: Form Lists and Helper
// =============================================================================

namespace {

/**
 * @brief Check if surface matches any form in the given array
 * @param surface Surface string to check
 * @param forms Array of form strings
 * @param count Number of elements in forms array
 * @return true if surface matches any form
 */
template <std::size_t N>
bool isInFormList(std::string_view surface,
                  const std::string_view (&forms)[N]) {
  return std::any_of(std::begin(forms), std::end(forms),
                     [surface](std::string_view form) {
                       return surface == form;
                     });
}

// いる auxiliary forms (progressive aspect)
// Full forms: いる, います, いました, いません, いない, いなかった, いれば
// Contracted forms: てる/でる = ている contraction
constexpr std::string_view kIruForms[] = {
    // Full forms
    "いる", "います", "いました", "いません", "いない", "いなかった", "いれば",
    // Contracted forms (てる/でる = ている contraction)
    "てる", "てた", "てて", "てない", "てなかった",
    "でる", "でた", "でて", "でない", "でなかった"
};

// しまう auxiliary forms (completive/regretful aspect)
// Full forms: 五段ワ行活用
// Contracted forms: ちゃう/じゃう = てしまう/でしまう
constexpr std::string_view kShimauForms[] = {
    // Full forms (五段ワ行活用)
    "しまう", "しまった", "しまって", "しまいます", "しまいました",
    "しまいません", "しまわない", "しまわなかった", "しまえば",
    // Contracted forms: ちゃう/じゃう = てしまう/でしまう
    "ちゃう", "ちゃった", "ちゃって", "ちゃいます", "ちゃいました",
    "じゃう", "じゃった", "じゃって", "じゃいます", "じゃいました"
};

// おく auxiliary forms (preparatory aspect)
// Full forms: 五段カ行活用
// Contracted forms: とく/どく = ておく/でおく
constexpr std::string_view kOkuForms[] = {
    // Full forms (五段カ行活用)
    "おく", "おいた", "おいて", "おきます", "おきました",
    "おかない", "おかなかった", "おけば",
    // Contracted forms: とく/どく = ておく/でおく
    "とく", "といた", "といて", "ときます", "ときました",
    "どく", "どいた", "どいて", "どきます", "どきました"
};

/**
 * @brief Check if surface is おく auxiliary form (internal use only)
 * Includes full forms (おく, おいた) and contracted forms (とく, どく)
 */
bool isOkuAuxiliary(std::string_view surface) {
  return isInFormList(surface, kOkuForms);
}

// =============================================================================
// AUX連用形 + た/ん チェック用設定と汎用関数
// =============================================================================

/**
 * @brief Configuration for AUX renyokei + た pattern checking
 */
struct RenyokeiToTaConfig {
  std::string_view surface1;        // Primary surface (まし, なかっ, etc.)
  std::string_view surface2;        // Secondary surface (ませ for ます, empty for others)
  std::string_view lemma;           // Required lemma (ます, ない, etc.)
  bool allow_n;                     // Allow next surface "ん" in addition to "た"
  ConnectionPattern pattern;
  const char* description;
};

/**
 * @brief Generic check for AUX renyokei + た/ん patterns
 * Used by checkMasuRenyokeiToTa, checkNaiRenyokeiToTa, checkTaiRenyokeiToTa, checkDesuRenyokeiToTa
 */
ConnectionRuleResult checkAuxRenyokeiToTaGeneric(
    const core::LatticeEdge& prev,
    const core::LatticeEdge& next,
    const ConnectionOptions& opts,
    const RenyokeiToTaConfig& config) {
  if (!isAuxToAux(prev, next)) return {};

  // Check lemma
  if (prev.lemma != config.lemma) return {};

  // Check surface (primary or secondary)
  if (prev.surface != config.surface1 &&
      (config.surface2.empty() || prev.surface != config.surface2)) {
    return {};
  }

  // Check next is た (past) or optionally ん (negative)
  if (next.surface != scorer::kFormTa &&
      (!config.allow_n || next.surface != "ん")) {
    return {};
  }

  // Give bonus (negative value) to prefer this MeCab-compatible split
  return {config.pattern, -opts.bonus_masu_renyokei_to_ta, config.description};
}

}  // namespace

// =============================================================================
// Helper Functions
// =============================================================================

bool isIruAuxiliary(std::string_view surface) {
  return isInFormList(surface, kIruForms);
}

bool isShimauAuxiliary(std::string_view surface) {
  return isInFormList(surface, kShimauForms);
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
// Exception: Bare suru te-form "して" should NOT get bonus - MeCab splits as し+て
ConnectionRuleResult checkIruAuxAfterTeForm(const core::LatticeEdge& prev,
                                            const core::LatticeEdge& next,
                                            const ConnectionOptions& opts) {
  if (!isVerbToAux(prev, next)) return {};

  if (!isIruAuxiliary(next.surface)) return {};

  if (!endsWithTeForm(prev.surface)) return {};

  // Don't give bonus for bare suru te-form "して" - should be split as し+て
  // MeCab: している → し + て + いる (3 tokens)
  if (isBareSuruTeForm(prev)) {
    return {};
  }

  // Bonus (negative value) for te-form + iru pattern
  return {ConnectionPattern::IruAuxAfterTeForm, -opts.bonus_iru_aux_after_te_form,
          "te-form verb + iru aux (progressive)"};
}

// Rule: Te-form VERB + しまう/しまった (AUX) bonus
// Completive/Regretful aspect pattern: 食べてしまった, 忘れてしまった
// Exception: Bare suru te-form "して" should NOT get bonus - MeCab splits as し+て
ConnectionRuleResult checkShimauAuxAfterTeForm(const core::LatticeEdge& prev,
                                               const core::LatticeEdge& next,
                                               const ConnectionOptions& opts) {
  if (!isVerbToAux(prev, next)) return {};

  if (!isShimauAuxiliary(next.surface)) return {};

  if (!endsWithTeForm(prev.surface)) return {};

  // Don't give bonus for bare suru te-form "して" - should be split as し+て
  // MeCab: してしまう → し + て + しまう (3 tokens)
  // This also applies to して forms from 漢字+する compound verbs (勉強して, etc.)
  // since those should split as 勉強 + し + て for MeCab compatibility
  if (isBareSuruTeForm(prev)) {
    return {};
  }

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
    return {ConnectionPattern::SouAfterRenyokei, -scorer::kBonusSouAfterRenyokeiSmall,
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
    // た after te-form: depends on context
    // Case 1: prev is standalone て/で (VERB from てる/でる) → MeCab splits as て + た
    //         E.g., 見てた → 見 + て + た (MeCab compatible)
    //         Give BONUS to make this path competitive
    // Case 2: prev is full te-form like 食べて → should be unified as 食べてた
    //         Add PENALTY to prefer unified analysis
    if (next.surface == scorer::kFormTa) {
      // Check if prev is standalone て/で from てる/でる
      bool is_teru_renyokei = (prev.surface == scorer::kFormTe && prev.lemma == "てる") ||
                              (prev.surface == scorer::kFormDe && prev.lemma == "でる");
      if (is_teru_renyokei) {
        // Give strong bonus to make 見 + て + た path win over 見 + てた
        return {ConnectionPattern::TeruRenyokeiToTa, -scorer::kBonusTeruRenyokeiToTa,
                "teru renyokei + ta (MeCab-compatible split)"};
      }
      // Regular te-form + た: penalize to prefer unified form
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

// Rule: AUX(まし/ませ) → AUX(た/ん) bonus
// MeCab-compatible split: しました → し + まし + た
ConnectionRuleResult checkMasuRenyokeiToTa(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next,
                                           const ConnectionOptions& opts) {
  static constexpr RenyokeiToTaConfig config{
      "まし", "ませ", scorer::kLemmaMasu, true,
      ConnectionPattern::MasuRenyokeiToTa, "masu-renyokei + ta/n (MeCab-compatible split)"};
  return checkAuxRenyokeiToTaGeneric(prev, next, opts, config);
}

// Rule: AUX(なかっ) → AUX(た) bonus
// MeCab-compatible split: なかった → なかっ + た
ConnectionRuleResult checkNaiRenyokeiToTa(const core::LatticeEdge& prev,
                                          const core::LatticeEdge& next,
                                          const ConnectionOptions& opts) {
  static constexpr RenyokeiToTaConfig config{
      "なかっ", "", "ない", false,
      ConnectionPattern::NaiRenyokeiToTa, "nai-renyokei + ta (MeCab-compatible split)"};
  return checkAuxRenyokeiToTaGeneric(prev, next, opts, config);
}

// Rule: AUX(たかっ) → AUX(た) bonus
// MeCab-compatible split: たかった → たかっ + た
ConnectionRuleResult checkTaiRenyokeiToTa(const core::LatticeEdge& prev,
                                          const core::LatticeEdge& next,
                                          const ConnectionOptions& opts) {
  static constexpr RenyokeiToTaConfig config{
      "たかっ", "", "たい", false,
      ConnectionPattern::TaiRenyokeiToTa, "tai-renyokei + ta (MeCab-compatible split)"};
  return checkAuxRenyokeiToTaGeneric(prev, next, opts, config);
}

// Rule: AUX(でし) → AUX(た) bonus
// MeCab-compatible split: でした → でし + た
ConnectionRuleResult checkDesuRenyokeiToTa(const core::LatticeEdge& prev,
                                           const core::LatticeEdge& next,
                                           const ConnectionOptions& opts) {
  static constexpr RenyokeiToTaConfig config{
      "でし", "", "です", false,
      ConnectionPattern::DesuRenyokeiToTa, "desu-renyokei + ta (MeCab-compatible split)"};
  return checkAuxRenyokeiToTaGeneric(prev, next, opts, config);
}

// Rule: AUX(た) → AUX(い) penalty
// Prevents でたい → で+た+い, should be で+たい
// This penalty makes the たい path win over the た+い path
ConnectionRuleResult checkInvalidTaToI(const core::LatticeEdge& prev,
                                       const core::LatticeEdge& next,
                                       const ConnectionOptions& /* opts */) {
  if (!isAuxToAux(prev, next)) return {};

  // Check if prev is た (past auxiliary)
  if (prev.surface != scorer::kFormTa) return {};

  // Check if next is い (surface only - may be unknown word with empty lemma)
  if (next.surface != "い") return {};

  // Apply strong penalty to prevent this invalid split
  return {ConnectionPattern::InvalidTaToI, scorer::scale::kProhibitive,
          "invalid ta + i (should be tai)"};
}

// Rule: AUX(れ/られ) → AUX(ない/た) bonus
// MeCab-compatible split: 言われない → 言わ + れ + ない
// This bonus helps the 3-token path beat 2-token paths like 言われ(VERB) + ない
// Without this, the VERB→ない bonus makes 言われ+ない win over 言わ+れ+ない
ConnectionRuleResult checkPassiveAuxToNaiTa(const core::LatticeEdge& prev,
                                            const core::LatticeEdge& next,
                                            const ConnectionOptions& opts) {
  if (!isAuxToAux(prev, next)) return {};

  // Check if prev is passive auxiliary stem (れ/られ with lemma れる/られる)
  if (prev.lemma != "れる" && prev.lemma != "られる") return {};

  // Check if prev is stem form (れ/られ, not conjugated forms)
  if (prev.surface != "れ" && prev.surface != "られ") return {};

  // Check if next is ない/た/ます (negative/past/polite)
  if (next.surface != "ない" && next.surface != scorer::kFormTa &&
      next.surface != "ます" && next.lemma != scorer::kLemmaMasu) return {};

  // Give bonus (negative value) to prefer this MeCab-compatible split
  // Use same bonus as masu conjugation split
  return {ConnectionPattern::PassiveAuxToNaiTa, -opts.bonus_masu_renyokei_to_ta,
          "passive-aux + nai/ta (MeCab-compatible split)"};
}

// Rule: VERB → AUX(とく/どく/ちゃう/じゃう) bonus
// MeCab-compatible split: 見とく → 見 + とく, 読んどく → 読ん + どく
// This bonus helps the split path beat the integrated contraction form
// MeCab treats these as: VERB + 動詞,非自立
// Exception: Bare suru te-form "して" should NOT get bonus - MeCab splits as し+て
ConnectionRuleResult checkVerbToOkuChauContraction(const core::LatticeEdge& prev,
                                                   const core::LatticeEdge& next,
                                                   const ConnectionOptions& opts) {
  if (!isVerbToAux(prev, next)) return {};

  // Check if next is おく/とく/どく contraction or しまう/ちゃう/じゃう contraction
  if (!isOkuAuxiliary(next.surface) && !isShimauAuxiliary(next.surface)) {
    return {};
  }

  // Don't give bonus for bare suru te-form "して" - should be split as し+て
  // MeCab: してしまう → し + て + しまう (3 tokens)
  if (isBareSuruTeForm(prev)) {
    return {};
  }

  // Verify prev is verb renyokei/onbin form:
  // - Ichidan renyokei: ends with e-row (べ, て, め, etc.) for 2+ char verbs
  // - Ichidan single kanji: 見, 出, etc.
  // - Godan onbin: ends with ん (撥音便), っ (促音便), い (イ音便)
  bool is_ichidan_renyokei = grammar::endsWithERow(prev.surface);
  bool is_single_kanji = (prev.surface.size() == core::kJapaneseCharBytes);
  bool is_onbin = grammar::endsWithOnbin(prev.surface);

  if (!is_ichidan_renyokei && !is_single_kanji && !is_onbin) {
    return {};
  }

  // Give strong bonus to prefer split over integrated form
  return {ConnectionPattern::VerbToOkuChauContraction,
          -opts.bonus_verb_to_contraction_aux,
          "verb + toku/chau (MeCab-compatible split)"};
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

// Check for で(PARTICLE) → くる活用形 (きます, きた, きて etc.)
// This is usually a misparse of できる (can do).
// Example: できます → で(PARTICLE) + きます(AUX,くる) is wrong
//          Should be: でき(VERB,できる) + ます(AUX)
// We add a penalty to prefer the できる analysis.
ConnectionRuleResult checkParticleDeToKuruAux(const core::LatticeEdge& prev,
                                              const core::LatticeEdge& next,
                                              const ConnectionOptions& /* opts */) {
  // Only check PARTICLE → AUX or PARTICLE → VERB pattern
  if (prev.pos != core::PartOfSpeech::Particle) {
    return {};
  }
  if (next.pos != core::PartOfSpeech::Auxiliary &&
      next.pos != core::PartOfSpeech::Verb) {
    return {};
  }

  // Check if prev is で(PARTICLE)
  if (prev.surface != "で") {
    return {};
  }

  // Check if next is a くる conjugation form (lemma = くる)
  if (next.lemma != "くる") {
    return {};
  }

  // Apply strong penalty to disfavor this pattern
  // This helps できる to be recognized correctly
  return {ConnectionPattern::ParticleDeToKuruAux, scorer::kPenaltyDeToKuruAux,
          "de(particle) + kuru-aux penalty (likely dekiru misparse)"};
}

// =============================================================================
// Copula で(AUX) → くる活用形 penalty
// =============================================================================
// Prevents できます from being misparsed as で(AUX,だ) + きます(AUX,くる)
// when で(AUX, lemma=だ) is added to support na-adjective patterns.
ConnectionRuleResult checkCopulaDeToKuruAux(const core::LatticeEdge& prev,
                                            const core::LatticeEdge& next,
                                            const ConnectionOptions& /*opts*/) {
  // Only check AUX → AUX or AUX → VERB pattern
  if (prev.pos != core::PartOfSpeech::Auxiliary) {
    return {};
  }
  if (next.pos != core::PartOfSpeech::Auxiliary &&
      next.pos != core::PartOfSpeech::Verb) {
    return {};
  }

  // Check if prev is で(AUX, lemma=だ)
  if (prev.surface != "で" || prev.lemma != "だ") {
    return {};
  }

  // Check if next is a くる conjugation form (lemma = くる)
  if (next.lemma != "くる") {
    return {};
  }

  // Apply strong penalty to disfavor this pattern
  return {ConnectionPattern::CopulaDeToKuruAux, scorer::kPenaltyDeToKuruAux,
          "de(aux,da) + kuru-aux penalty (likely dekiru misparse)"};
}

// =============================================================================
// NOUN/ADJ → で(AUX, lemma=だ) bonus
// =============================================================================
// Supports na-adjective copula negation pattern (嫌でない, 好きでない, etc.)
// MeCab: 嫌 + で(AUX,だ) + ない(AUX)
ConnectionRuleResult checkNaAdjToCopulaDe(const core::LatticeEdge& prev,
                                          const core::LatticeEdge& next,
                                          const ConnectionOptions& /*opts*/) {
  // Only check NOUN/ADJ → AUX pattern
  if (prev.pos != core::PartOfSpeech::Noun &&
      prev.pos != core::PartOfSpeech::Adjective) {
    return {};
  }
  if (next.pos != core::PartOfSpeech::Auxiliary) {
    return {};
  }

  // Check if next is で(AUX, lemma=だ)
  if (next.surface != "で" || next.lemma != "だ") {
    return {};
  }

  // Apply bonus to favor this pattern for na-adjective copula
  return {ConnectionPattern::NaAdjToCopulaDe, scorer::kBonusNaAdjToCopulaDe,
          "noun/adj + de(aux,da) bonus (na-adj copula pattern)"};
}

// =============================================================================
// NOUN/ADJ → でない(VERB, lemma=できる) penalty
// =============================================================================
// Prevents na-adjective copula pattern from being misparsed as できる negation.
// Example: 嫌でない → should be 嫌 + で(AUX) + ない(AUX), NOT 嫌 + でない(VERB,できる)
ConnectionRuleResult checkNaAdjToDekinaiVerb(const core::LatticeEdge& prev,
                                             const core::LatticeEdge& next,
                                             const ConnectionOptions& /*opts*/) {
  // Only check NOUN/ADJ → VERB pattern
  if (prev.pos != core::PartOfSpeech::Noun &&
      prev.pos != core::PartOfSpeech::Adjective) {
    return {};
  }
  if (next.pos != core::PartOfSpeech::Verb) {
    return {};
  }

  // Check if next is でない(VERB, lemma=できる)
  if (next.surface != "でない" || next.lemma != "できる") {
    return {};
  }

  // Apply strong penalty to prevent this misparse
  // The dictionary entry has cost -2.0, so we need a strong penalty to overcome it
  return {ConnectionPattern::NaAdjToDekinaiVerb, scorer::kPenaltyNaAdjToDekinaiVerb,
          "noun/adj + denai(dekiru) penalty (should be copula pattern)"};
}

// =============================================================================
// で(AUX, lemma=だ) → ない(AUX) bonus
// =============================================================================
// Supports na-adjective copula negation pattern (嫌でない, 好きでない, etc.)
// MeCab: 嫌 + で(AUX,だ) + ない(AUX)
ConnectionRuleResult checkCopulaDeToNai(const core::LatticeEdge& prev,
                                        const core::LatticeEdge& next,
                                        const ConnectionOptions& /*opts*/) {
  // Only check AUX → AUX pattern
  if (prev.pos != core::PartOfSpeech::Auxiliary) {
    return {};
  }
  if (next.pos != core::PartOfSpeech::Auxiliary) {
    return {};
  }

  // Check if prev is で(AUX, lemma=だ)
  if (prev.surface != "で" || prev.lemma != "だ") {
    return {};
  }

  // Check if next is ない(AUX)
  if (next.surface != "ない" || next.lemma != "ない") {
    return {};
  }

  // Apply bonus to favor this pattern for na-adjective copula negation
  return {ConnectionPattern::CopulaDeToNai, -scorer::kBonusCopulaDeToNai,
          "de(aux,da) + nai(aux) bonus (na-adj copula negation)"};
}

}  // namespace connection_rules
}  // namespace suzume::analysis
