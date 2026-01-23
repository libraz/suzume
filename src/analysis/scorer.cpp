#include "analysis/scorer.h"

#include <cmath>

#include "analysis/bigram_table.h"
#include "analysis/category_cost.h"
#include "core/debug.h"
#include "core/types.h"
#include "core/utf8_constants.h"
#include "normalize/utf8.h"
#include "grammar/char_patterns.h"

using suzume::core::CandidateOrigin;

namespace {

// Convert POS to array index
constexpr size_t posToIndex(suzume::core::PartOfSpeech pos) {
  switch (pos) {
    case suzume::core::PartOfSpeech::Noun:
      return 0;
    case suzume::core::PartOfSpeech::Verb:
      return 1;
    case suzume::core::PartOfSpeech::Adjective:
      return 2;
    case suzume::core::PartOfSpeech::Adverb:
      return 3;
    case suzume::core::PartOfSpeech::Particle:
      return 4;
    case suzume::core::PartOfSpeech::Auxiliary:
      return 5;
    case suzume::core::PartOfSpeech::Conjunction:
      return 6;
    case suzume::core::PartOfSpeech::Determiner:
      return 7;
    case suzume::core::PartOfSpeech::Pronoun:
      return 8;
    case suzume::core::PartOfSpeech::Prefix:
      return 9;
    case suzume::core::PartOfSpeech::Suffix:
      return 10;
    case suzume::core::PartOfSpeech::Symbol:
      return 11;
    case suzume::core::PartOfSpeech::Other:
    case suzume::core::PartOfSpeech::Unknown:
      return 12;
  }
  return 12;
}

// Bigram cost table [prev][next]
// Scale reference: kTrivial=0.2, kMinor=0.5, kModerate=1.0, kStrong=1.5
// Negative values = bonus (encourages connection)
// clang-format off
constexpr float kBigramCostTable[13][13] = {
    //        Noun  Verb  Adj   Adv   Part  Aux   Conj  Det   Pron  Pref  Suff  Sym   Other
    /* Noun */ {0.0F, 0.5F, 0.5F, 0.3F, 0.0F, 0.0F, 0.5F, 0.5F, 0.5F, 1.0F,-0.8F, 0.5F, 0.5F},
    /* Verb */ {0.2F, 0.8F, 0.8F, 0.5F, 0.0F, 0.0F, 0.5F, 0.5F, 0.2F, 1.0F, 1.5F, 0.5F, 0.5F},  // Suff: 0.8→1.5 (知ってる人: NOUN優先)
    /* Adj  */ {0.2F, 0.5F, 0.8F, 0.3F, 0.0F, 0.5F, 0.5F, 0.5F, 0.2F, 1.0F, 0.8F, 0.5F, 0.5F},  // Keep 0.5 (P3-2 causes side effects)
    /* Adv  */ {0.0F, 0.3F, 0.0F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.0F, 1.0F, 0.8F, 0.5F, 0.5F},
    /* Part */ {0.0F, 0.2F, 0.2F, 0.3F, 0.5F, 0.5F, 0.5F, 0.3F, 0.0F, 0.3F, 1.0F, 0.5F, 0.5F},  // Pref: 1.0→0.3 (何番線: は→何PREFIX)
    /* Aux  */ {0.5F, 0.5F, 0.5F, 0.5F, 0.0F, 0.3F, 0.5F, 0.5F, 0.5F, 1.0F, 0.8F, 0.5F, 0.5F},
    /* Conj */ {0.0F, 0.2F, 0.2F, 0.2F, 0.3F, 0.5F, 0.5F, 0.2F, 0.0F, 0.3F, 1.0F, 0.3F, 0.3F},
    /* Det  */ {0.0F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.8F, 0.0F, 1.0F, 1.5F, 0.5F, 0.5F},  // Suff: 0.8→1.5 (あんな人: NOUN優先)
    /* Pron */ {0.0F, 0.5F, 0.5F, 0.3F, 0.0F, 0.2F, 0.5F, 0.5F, 0.5F, 1.0F, 0.0F, 0.5F, 0.5F},  // P3-1: Aux 1.0→0.2 (私だ is basic)
    /* Pref */{-0.5F,-0.5F, 0.0F, 0.5F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F},
    /* Suff */ {0.5F, 0.8F, 0.8F, 0.5F, 0.0F, 0.5F, 0.5F, 0.5F, 0.5F, 1.0F, 0.3F, 0.5F, 0.5F},
    /* Sym  */ {0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.5F, 0.0F, 0.2F},
    /* Other*/ {0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.5F, 0.5F, 0.2F, 0.2F},
};
// clang-format on

}  // namespace

namespace suzume::analysis {

// static
void Scorer::logAdjustment(float amount, const char* reason) {
  if (amount != 0.0F) {
    SUZUME_DEBUG_LOG_VERBOSE("  " << reason << ": "
                            << (amount > 0 ? "+" : "") << amount << "\n");
  }
}

Scorer::Scorer(const ScorerOptions& options) : options_(options) {}

float Scorer::posPrior(core::PartOfSpeech pos) const {
  switch (pos) {
    case core::PartOfSpeech::Noun:
      return options_.noun_prior;
    case core::PartOfSpeech::Verb:
      return options_.verb_prior;
    case core::PartOfSpeech::Adjective:
      return options_.adj_prior;
    case core::PartOfSpeech::Adverb:
      return options_.adv_prior;
    case core::PartOfSpeech::Particle:
      return options_.particle_prior;
    case core::PartOfSpeech::Auxiliary:
      return options_.aux_prior;
    case core::PartOfSpeech::Pronoun:
      return options_.pronoun_prior;
    default:
      return 0.5F;
  }
}

float Scorer::bigramCost(core::PartOfSpeech prev, core::PartOfSpeech next) const {
  // Check for BigramOverrides first (NaN = use default)
  const auto& bg = options_.bigram;

  // Helper lambda to check if override is set
  auto isSet = [](float v) { return !std::isnan(v); };

  // Check specific pair overrides
  using POS = core::PartOfSpeech;
  if (prev == POS::Noun && next == POS::Suffix && isSet(bg.noun_to_suffix))
    return bg.noun_to_suffix;
  if (prev == POS::Prefix && next == POS::Noun && isSet(bg.prefix_to_noun))
    return bg.prefix_to_noun;
  if (prev == POS::Prefix && next == POS::Verb && isSet(bg.prefix_to_verb))
    return bg.prefix_to_verb;
  if (prev == POS::Pronoun && next == POS::Auxiliary && isSet(bg.pron_to_aux))
    return bg.pron_to_aux;
  if (prev == POS::Verb && next == POS::Verb && isSet(bg.verb_to_verb))
    return bg.verb_to_verb;
  if (prev == POS::Verb && next == POS::Noun && isSet(bg.verb_to_noun))
    return bg.verb_to_noun;
  if (prev == POS::Verb && next == POS::Auxiliary && isSet(bg.verb_to_aux))
    return bg.verb_to_aux;
  if (prev == POS::Adjective && next == POS::Auxiliary && isSet(bg.adj_to_aux))
    return bg.adj_to_aux;
  if (prev == POS::Adjective && next == POS::Verb && isSet(bg.adj_to_verb))
    return bg.adj_to_verb;
  if (prev == POS::Adjective && next == POS::Adjective && isSet(bg.adj_to_adj))
    return bg.adj_to_adj;
  if (prev == POS::Particle && next == POS::Verb && isSet(bg.part_to_verb))
    return bg.part_to_verb;
  if (prev == POS::Particle && next == POS::Noun && isSet(bg.part_to_noun))
    return bg.part_to_noun;
  if (prev == POS::Auxiliary && next == POS::Particle && isSet(bg.aux_to_part))
    return bg.aux_to_part;
  if (prev == POS::Auxiliary && next == POS::Auxiliary && isSet(bg.aux_to_aux))
    return bg.aux_to_aux;

  // Fall back to default table
  return kBigramCostTable[posToIndex(prev)][posToIndex(next)];
}

float Scorer::wordCost(const core::LatticeEdge& edge) const {
  // v0.8: Base cost from ExtendedPOS category
  float category_cost = getCategoryCost(edge.extended_pos);

  // Use edge.cost if explicitly set (non-zero), otherwise use category cost
  // This allows candidates (e.g., adjective stem + すぎる) to have custom costs
  float cost = (edge.cost != 0.0F) ? edge.cost : category_cost;

  // User dictionary bonus (still needed for user customization)
  if (edge.fromUserDict()) {
    cost += options_.user_dict_bonus;
  }

  // Bonus for hiragana i-adjectives from dictionary
  // Prevents misanalysis as verb+たい (e.g., つめたい → つめ+たい)
  // or as adverb+verb+aux (e.g., はなはだしい → はなはだ+し+い)
  // Longer adjectives get stronger bonus to beat split paths
  // Exclude AdjStem (語幹) as it's not a complete i-adjective
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Adjective &&
      edge.extended_pos != core::ExtendedPOS::AdjStem &&
      grammar::isPureHiragana(edge.surface)) {
    size_t char_len = suzume::normalize::utf8Length(edge.surface);
    // Base bonus -2.5, plus 0.5 per character beyond 3
    float bonus = (char_len <= 3) ? -2.5F
                                  : -2.5F - static_cast<float>(char_len - 3) * 0.5F;
    cost += bonus;
  }

  // Bonus for hiragana adverbs from dictionary
  // Prevents misanalysis as verb+ん (e.g., たくさん → たくさ+ん)
  // and compound splits (e.g., どうして → どう+し+て)
  // Longer adverbs get stronger bonus to beat split paths
  // Short adverbs (2 chars) get weaker bonus to avoid false matches in patterns
  // like かもしれない (should not be か+もし+れない)
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Adverb &&
      grammar::isPureHiragana(edge.surface)) {
    size_t char_len = suzume::normalize::utf8Length(edge.surface);
    // Short adverbs (2 chars) get weaker bonus
    // Longer adverbs get stronger bonus (0.5 per character beyond 2)
    float bonus = (char_len <= 2) ? -1.0F
                                  : -2.5F - static_cast<float>(char_len - 2) * 0.5F;
    cost += bonus;
  }

  // Bonus for hiragana interjections/greetings from dictionary
  // Prevents misanalysis like さようなら → さ+よう+なら (volitional pattern)
  // These are fixed expressions that should remain as single tokens
  // Longer interjections get stronger bonus to beat common split patterns
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Other &&
      grammar::isPureHiragana(edge.surface)) {
    size_t char_len = suzume::normalize::utf8Length(edge.surface);
    // Stronger bonus for longer interjections (common greetings are 4-5 chars)
    float bonus = (char_len <= 3) ? -1.5F
                                  : -2.0F - static_cast<float>(char_len - 3) * 0.5F;
    cost += bonus;
  }

  // Bonus for hiragana conjunctions from dictionary (たとえば, それから, etc.)
  // Prevents misanalysis like たとえば → たとえ+ば (adverb + particle)
  // These are fixed expressions that should remain as single tokens
  // Needs to beat adverb bonus path, so use stronger bonus
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Conjunction &&
      grammar::isPureHiragana(edge.surface)) {
    size_t char_len = suzume::normalize::utf8Length(edge.surface);
    // Stronger bonus for conjunctions to beat adverb+particle splits
    // Adverb 3-char gets -3.0, plus particle gets bonus, so we need > -3.5
    float bonus = (char_len <= 3) ? -2.0F
                                  : -3.5F - static_cast<float>(char_len - 3) * 0.5F;
    cost += bonus;
  }

  // Bonus for compound particles from dictionary (について, によって, として, etc.)
  // These are multi-character particles that should not be split into verb+て patterns
  // Helps compound particles compete with high-bonus splits like し+て (-1 connection bonus)
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Particle &&
      grammar::isPureHiragana(edge.surface)) {
    size_t char_len = suzume::normalize::utf8Length(edge.surface);
    // Compound particles are 3+ chars (について, によって, として)
    // Give strong bonus to beat verb+て split paths
    if (char_len >= 3) {
      constexpr float kCompoundParticleBonus = -1.0F;
      cost += kCompoundParticleBonus;
    }
  }

  // Bonus for みたい (conjecture auxiliary) from dictionary
  // Works together with bigram bonuses and spurious verb penalty
  if (edge.fromDictionary() && edge.extended_pos == core::ExtendedPOS::AuxConjectureMitai) {
    constexpr float kMitaiDictBonus = -1.0F;
    cost += kMitaiDictBonus;
  }

  // Bonus for short hiragana verbs from dictionary (e.g., なる, ある, いる, する)
  // These compete with L1 function word entries (DET, AUX) which have lower category costs.
  // Dictionary registration indicates standalone verb usage should take precedence.
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Verb &&
      grammar::isPureHiragana(edge.surface) && edge.surface.length() <= 6) {  // ≤2 chars
    constexpr float kShortHiraganaVerbBonus = -0.3F;
    cost += kShortHiraganaVerbBonus;
  }

  // Penalty for spurious kanji+hiragana verb renyokei not in dictionary
  // These are often false positives like 学生み (学生みる doesn't exist)
  // Prevents misanalysis like 学生みたい → 学生み+たい
  // Only apply to surfaces with 2+ kanji (e.g., 学生み) to avoid penalizing
  // legitimate verb renyokei like 行き, 読み, 書き
  if (!edge.fromDictionary() && edge.pos == core::PartOfSpeech::Verb &&
      edge.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      edge.surface.length() >= 9) {  // ≥3 chars (at least 2 kanji + 1 hiragana)
    // Count kanji characters
    size_t kanji_count = 0;
    for (char32_t cp : suzume::normalize::utf8::decode(edge.surface)) {
      if (suzume::normalize::isKanjiCodepoint(cp)) {
        ++kanji_count;
      }
    }
    if (kanji_count >= 2) {
      constexpr float kSpuriousVerbRenyokeiPenalty = 1.5F;
      cost += kSpuriousVerbRenyokeiPenalty;
    }
  }

  // Bonus for compound adjectives from dictionary (e.g., 男らしい, 女らしい)
  // These compete with noun+らしい split which has -1.5 connection bonus.
  // Dictionary registration indicates compound adjective should take precedence.
  // Pattern: kanji stem + hiragana suffix forming an i-adjective
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Adjective &&
      edge.surface.length() >= 12) {  // ≥4 chars (kanji + ひらがな suffix)
    // Check if surface contains kanji and ends with い (compound adjective pattern)
    if (grammar::containsKanji(edge.surface) && utf8::endsWith(edge.surface, "い")) {
      constexpr float kCompoundAdjDictBonus = -0.5F;
      cost += kCompoundAdjDictBonus;
    }
  }

  // Debug output - show which cost was used and candidate origin (verbose level)
  SUZUME_DEBUG_VERBOSE_BLOCK {
    // Base source type
    const char* source = edge.fromDictionary() ? "dict" :
                         edge.isUnknown() ? "unk" : "infl";
    const char* cost_from = (edge.cost != 0.0F) ? "edge" : "category";

    SUZUME_DEBUG_STREAM << "[WORD] \"" << edge.surface << "\" (" << source;
#ifdef SUZUME_DEBUG_INFO
    // Show detailed origin if available (e.g., "dict:adj_i", "unk:verb_kanji")
    if (edge.origin != CandidateOrigin::Unknown) {
      SUZUME_DEBUG_STREAM << ":" << core::originToString(edge.origin);
    }
    if (!edge.origin_detail.empty()) {
      SUZUME_DEBUG_STREAM << "/" << edge.origin_detail;
    }
#endif
    SUZUME_DEBUG_STREAM << ") cost=" << cost << " (from " << cost_from << ")";
    SUZUME_DEBUG_STREAM << " [cat=" << category_cost;
    if (edge.cost != 0.0F) {
      SUZUME_DEBUG_STREAM << " edge=" << edge.cost;
    }
    // Show epos with source indicator
    SUZUME_DEBUG_STREAM << " epos=" << core::extendedPosToString(edge.extended_pos);
    // Check if epos matches default for this POS (indicates derived vs explicit)
    core::ExtendedPOS default_epos = core::posToExtendedPos(edge.pos);
    if (edge.extended_pos == core::ExtendedPOS::Unknown) {
      SUZUME_DEBUG_STREAM << "(!UNKNOWN)";  // Warning: missing mapping
    } else if (edge.extended_pos != default_epos) {
#ifdef SUZUME_DEBUG_INFO
      // Show where the ExtendedPOS was set (e.g., "binary_dict", "l1_dict", "candidate_gen")
      if (!edge.epos_source.empty()) {
        SUZUME_DEBUG_STREAM << "(from:" << edge.epos_source << ")";
      } else {
        SUZUME_DEBUG_STREAM << "(explicit)";
      }
#else
      SUZUME_DEBUG_STREAM << "(explicit)";  // Explicitly set, different from default
#endif
    }
    SUZUME_DEBUG_STREAM << "]";
    // Highlight non-dictionary candidates (potential spurious entries)
    if (!edge.fromDictionary()) {
      SUZUME_DEBUG_STREAM << " [non-dict]";
    }
    SUZUME_DEBUG_STREAM << "\n";
  }
  return cost;
}

float Scorer::connectionCost(const core::LatticeEdge& prev,
                             const core::LatticeEdge& next) const {
  float base_cost = bigramCost(prev.pos, next.pos);

  // ExtendedPOS bigram cost (replaces all check functions)
  float extended_cost = BigramTable::getCost(prev.extended_pos, next.extended_pos);

  // Surface-based bonus for VerbRenyokei → すぎ pattern
  // E.g., 読み+すぎる, 書き+すぎた, 食べ+すぎ (MeCab-compatible split)
  // The default VERB→VERB penalty (0.8) should not apply to auxiliary verbs
  float surface_bonus = 0.0F;
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.surface.size() >= 6 &&  // "すぎ" is 6 bytes
      next.surface.compare(0, 6, "すぎ") == 0) {
    // Strong bonus to overcome VERB→VERB penalty
    surface_bonus = -0.5F;
  }

  // Bonus for longer causative forms (させ over さ+せ, させられ over さ+せ+られ)
  // MeCab treats させる as a single causative auxiliary for ichidan verbs
  // E.g., 食べ+させ+られ+た (not 食べ+さ+せ+られ+た)
  // Apply bonus when connecting to AuxCausative with surface starting with させ
  if ((prev.extended_pos == core::ExtendedPOS::VerbRenyokei ||
       prev.extended_pos == core::ExtendedPOS::VerbMizenkei) &&
      next.extended_pos == core::ExtendedPOS::AuxCausative &&
      next.surface.size() >= 6 &&  // "させ" is 6 bytes
      next.surface.compare(0, 6, "させ") == 0) {
    // Bonus to prefer させ over さ+せ split
    surface_bonus += -1.0F;
  }

  // Surface-based bonus for VerbRenyokei → た (ichidan/irregular た-form)
  // E.g., 食べ+た, 来+た (MeCab-compatible split)
  // Must be surface == "た" to distinguish from て (particle)
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.surface == "た" &&
      next.extended_pos == core::ExtendedPOS::AuxTenseTa) {
    surface_bonus += -1.5F;  // Strong bonus for た-form split
  }

  // Surface-based penalty for Noun → VerbRenyokei when surface is not サ変 form
  // Bigram table gives -1.0 bonus for Noun→VerbRenyokei (for サ変動詞: 得+し, 損+し)
  // But this should NOT apply to "い" (いる連用形) after noun
  // E.g., 勘違い should be single token, not 勘違+い
  if (prev.extended_pos == core::ExtendedPOS::Noun &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.surface != "し" && next.surface != "せ" &&
      next.surface.size() <= 3) {  // Single hiragana character
    surface_bonus += 1.0F;  // Cancel the bigram bonus
  }

  float total = base_cost + extended_cost + surface_bonus;

  SUZUME_DEBUG_VERBOSE_BLOCK {
    SUZUME_DEBUG_STREAM << "[CONN] \"" << prev.surface << "\" ("
                    << core::posToString(prev.pos) << "/"
                    << core::extendedPosToString(prev.extended_pos) << ") → \""
                    << next.surface << "\" ("
                    << core::posToString(next.pos) << "/"
                    << core::extendedPosToString(next.extended_pos) << "): "
                    << "bigram=" << base_cost << " epos_adj=" << extended_cost;
    if (surface_bonus != 0.0F) {
      SUZUME_DEBUG_STREAM << " surface_bonus=" << surface_bonus;
    }
    if (extended_cost != 0.0F) {
      // Show rule name: PrevEPOS→NextEPOS
      SUZUME_DEBUG_STREAM << " (rule=" << core::extendedPosToString(prev.extended_pos)
                          << "→" << core::extendedPosToString(next.extended_pos) << ")";
    } else if (surface_bonus == 0.0F) {
      SUZUME_DEBUG_STREAM << " (default)";
    }
    SUZUME_DEBUG_STREAM << " total=" << total << "\n";
  }

  return total;
}

}  // namespace suzume::analysis
