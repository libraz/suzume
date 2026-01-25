#include "analysis/scorer.h"

#include <cmath>

#include "analysis/bigram_table.h"
#include "analysis/category_cost.h"
#include "core/debug.h"
#include "core/kana_constants.h"
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
  // Also applies to kanji-containing particles (において, に関して, に際して, に対して)
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Particle) {
    size_t char_len = suzume::normalize::utf8Length(edge.surface);
    // Compound particles are 3+ chars (について, によって, として, において, etc.)
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

  // Bonus for dictionary entries starting with negation prefixes (非/不/無/未)
  // E.g., 不可能, 非常, 無理, 無限, 無鉄砲 - these are single lexical items
  // Without this bonus, PREFIX+NOUN split path wins due to strong connection bonus (-2)
  // Dictionary entries should take precedence over compositional analysis
  if (edge.fromDictionary() &&
      (edge.pos == core::PartOfSpeech::Adjective || edge.pos == core::PartOfSpeech::Noun) &&
      edge.surface.size() >= 6 &&  // At least 2 chars (prefix + something)
      (edge.surface.compare(0, 3, "非") == 0 ||
       edge.surface.compare(0, 3, "不") == 0 ||
       edge.surface.compare(0, 3, "無") == 0 ||
       edge.surface.compare(0, 3, "未") == 0)) {
    constexpr float kNegationPrefixDictBonus = -3.0F;
    cost += kNegationPrefixDictBonus;
  }

  // Bonus for hiragana+kanji mixed nouns from dictionary (e.g., なし崩し, みじん切り, お茶)
  // These are idiomatic expressions that should not be split
  // E.g., なし崩し should not be split as な+し+崩し (AUX+PARTICLE+NOUN)
  // Requires 3+ chars with both hiragana and kanji
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Noun) {
    size_t char_len = suzume::normalize::utf8Length(edge.surface);
    if (char_len >= 3) {
      bool has_hiragana = false;
      bool has_kanji = false;
      for (char32_t cp : suzume::normalize::utf8::decode(edge.surface)) {
        if (suzume::kana::isHiraganaCodepoint(cp)) {
          has_hiragana = true;
        } else if (suzume::kana::isKanjiCodepoint(cp)) {
          has_kanji = true;
        }
        if (has_hiragana && has_kanji) break;
      }
      if (has_hiragana && has_kanji) {
        // Base bonus -0.5, stronger for longer words
        constexpr float kMixedNounBonus = -0.5F;
        cost += kMixedNounBonus;
      }
    }
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

  // Penalty for pure-hiragana hatsuonbin (撥音便) verb forms not in dictionary
  // E.g., "おさん" as 撥音便 of "おさむ" is rare and likely mis-segmentation
  // Valid hatsuonbin usually has kanji stem (読ん, 飲ん, 呼ん)
  // This helps prevent が+おさん misanalysis (should be がお+さん)
  if (!edge.fromDictionary() && edge.pos == core::PartOfSpeech::Verb &&
      edge.extended_pos == core::ExtendedPOS::VerbOnbinkei &&
      grammar::isPureHiragana(edge.surface) &&
      edge.surface.size() >= 6) {  // 2+ chars (avoid single-char like ん)
    constexpr float kPureHiraganaOnbinPenalty = 2.5F;
    cost += kPureHiraganaOnbinPenalty;
  }

  // Penalty for pure-hiragana verb forms containing "さん" pattern
  // E.g., "おさんで" as te-form of "おさむ" is likely name+さん+で misanalysis
  // E.g., "さんで" as te-form of "さむ" is likely さん+で misanalysis
  // Patterns: xさん, xさんで, さんで where x is short hiragana (likely name)
  // This complements the hatsuonbin penalty above for other verb forms
  if (!edge.fromDictionary() && edge.pos == core::PartOfSpeech::Verb &&
      grammar::isPureHiragana(edge.surface) &&
      (utf8::contains(edge.surface, "さん"))) {
    size_t san_pos = edge.surface.find("さん");
    if (san_pos != std::string::npos) {
      // Penalize if:
      // 1. さん appears after 0-2 hiragana chars (likely name+さん or just さん)
      // 2. The verb form is short enough to be a misanalysis
      if (san_pos <= 6 && edge.surface.size() <= 15) {  // 0-2 chars before, up to 5 total
        constexpr float kSanPatternVerbPenalty = 2.5F;
        cost += kSanPatternVerbPenalty;
      }
    }
  }

  // Penalty for pure-hiragana ichidan verb renyokei starting with に
  // E.g., "につけ" as renyokei of "につける" is spurious
  // Should be に|つけ (particle + verb), not につけ (verb)
  // Valid verbs like "につける" don't exist; this is a mis-analysis
  if (!edge.fromDictionary() && edge.pos == core::PartOfSpeech::Verb &&
      edge.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      grammar::isPureHiragana(edge.surface) &&
      utf8::startsWith(edge.surface, "に") &&
      edge.surface.size() >= 6 && edge.surface.size() <= 12) {  // 2-4 chars
    constexpr float kNiPrefixVerbPenalty = 2.0F;
    cost += kNiPrefixVerbPenalty;
  }

  // Penalty for very long pure-hiragana verb candidates not in dictionary
  // E.g., "ございませんでし" as verb renyokei is spurious
  // Should be ござい|ませ|ん|でし (aux chain), not ございませんでし (verb)
  // Valid long verbs typically have kanji stems
  if (!edge.fromDictionary() && edge.pos == core::PartOfSpeech::Verb &&
      grammar::isPureHiragana(edge.surface) &&
      edge.surface.size() >= 18) {  // 6+ hiragana chars (6*3=18 bytes)
    constexpr float kVeryLongPureHiraganaVerbPenalty = 3.5F;
    cost += kVeryLongPureHiraganaVerbPenalty;
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

  // Penalty for kanji compound NOUN ending with 中 (chuu/juu suffix)
  // E.g., "一日中" should be split as 一日|中 (noun + suffix)
  // Registered compounds like "世界中" will also split (accepted difference from MeCab)
  // This helps Suffix 中 candidates win over NOUN compounds
  if (!edge.fromDictionary() && edge.pos == core::PartOfSpeech::Noun &&
      utf8::endsWith(edge.surface, "中") &&
      grammar::isAllKanji(edge.surface) &&
      edge.surface.size() >= 6) {  // 2+ kanji (at least N中)
    constexpr float kKanjiChuuCompoundPenalty = 0.5F;
    cost += kKanjiChuuCompoundPenalty;
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

  // Bonus for て → い (Auxiliary) pattern
  // E.g., して+い+ます, 食べて+い+た (MeCab-compatible: い is auxiliary, not verb)
  // The auxiliary い (from いる) should win over verb renyokei い
  if (prev.surface == "て" &&
      prev.extended_pos == core::ExtendedPOS::ParticleConj &&
      next.surface == "い" &&
      next.extended_pos == core::ExtendedPOS::AuxAspectIru) {
    surface_bonus += -0.5F;  // Bonus to prefer Auxiliary over Verb
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

  // Surface-based penalty for Noun → short VerbRenyokei (compound verb protection)
  // Bigram table gives -1.0 bonus for Noun→VerbRenyokei (for サ変動詞: 得+し, 損+し)
  // But this should NOT apply to compound verbs like 見+つけ→見つけ
  // E.g., 勘違い should be single token, not 勘違+い
  // E.g., 見つけた should be 見つけ+た, not 見+つけ+た
  // Exception: multi-kanji noun + でき should split (外出+でき+ない)
  // Single kanji NOUN often forms compound verbs with following verb stems
  bool is_single_kanji_noun = (prev.surface.size() == 3);  // UTF-8: 1 kanji = 3 bytes
  if (prev.extended_pos == core::ExtendedPOS::Noun &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.surface != "し" && next.surface != "せ" &&
      next.surface.size() <= 6 && is_single_kanji_noun) {
    surface_bonus += 1.0F;  // Cancel the bigram bonus
  }

  // Penalty for Noun/ナ形容詞 → い (VerbRenyokei)
  // 漢字名詞やナ形容詞語幹の後に「い」(いる連用形)が来ることは稀
  // E.g., 上手いし should be 上手い+し, not 上手+い+し
  // Exception: This should NOT block patterns like サ変動詞+でき (外出+でき)
  if ((prev.extended_pos == core::ExtendedPOS::AdjNaAdj ||
       prev.extended_pos == core::ExtendedPOS::Noun) &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.surface == "い") {
    surface_bonus += 2.0F;  // Strong penalty to prevent over-split
  }

  // Partial cancel for single-kanji NOUN + し pattern
  // E.g., 寒し (archaic adjective) should not split as 寒+し
  // But 得+し (suru-verb renyokei) should still split
  // Single kanji = 3 bytes in UTF-8
  if (prev.extended_pos == core::ExtendedPOS::Noun &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.surface == "し" && prev.surface.size() == 3) {
    surface_bonus += 0.5F;  // Partial cancel: -1.0 + 0.5 = -0.5 effective bonus
  }

  // Penalty for single-char case particle → very short pure-hiragana verb pattern
  // E.g., が+おさ is likely mis-segmentation (should be がお+さん)
  // Valid patterns usually have longer verbs (3+ chars) or kanji stems
  // Single-char particles: が, を, に, へ, と, で, から, etc.
  // Only penalize very short verbs (2 chars or less) to avoid affecting なくし, etc.
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase &&
      prev.surface.size() <= 3 &&  // Single hiragana char (3 bytes in UTF-8)
      next.pos == core::PartOfSpeech::Verb &&
      !next.fromDictionary() &&
      grammar::isPureHiragana(next.surface) &&
      next.surface.size() <= 6) {  // 2 chars or less (6 bytes in UTF-8)
    surface_bonus += 2.5F;  // Strong penalty for this unlikely pattern
  }

  // Penalty for case particle → final particle pattern
  // E.g., を+な in をなくした should not split as を+な+くし+た
  // Final particles (な, ね, よ) don't follow case particles directly
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase &&
      next.extended_pos == core::ExtendedPOS::ParticleFinal) {
    surface_bonus += 3.0F;  // Strong penalty for this unlikely pattern
  }

  // Penalty for PREFIX → short pure-hiragana verb pattern
  // E.g., お+い in において should not happen (お is prefix, い is not a verb here)
  // Valid お+verb patterns: お待ち, お願い (longer, often with kanji)
  // Note: 「い」 is in L1 dictionary as verb renyokei, so don't check fromDictionary
  if (prev.pos == core::PartOfSpeech::Prefix &&
      next.pos == core::PartOfSpeech::Verb &&
      grammar::isPureHiragana(next.surface) &&
      next.surface.size() <= 6) {  // 2 chars or less
    surface_bonus += 2.5F;  // Strong penalty
  }

  // Penalty for ADV → short pure-hiragana verb renyokei pattern
  // E.g., はなはだ+し should not happen (はなはだしい is an adjective)
  // Valid ADV+verb patterns: ゆっくり+歩く (verb is longer/has kanji)
  // This prevents split like はなはだ+し+い when はなはだしい exists in dict
  if (prev.pos == core::PartOfSpeech::Adverb &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      grammar::isPureHiragana(next.surface) &&
      next.surface.size() <= 3) {  // 1 char only (し, み, etc.)
    surface_bonus += 1.5F;  // Moderate penalty
  }

  // Penalty for SYMBOL → PARTICLE pattern (furigana in parentheses)
  // E.g., 東京（とうきょう） should not split と+う+きょう
  // Particles don't normally follow opening parentheses directly
  if (prev.pos == core::PartOfSpeech::Symbol &&
      next.pos == core::PartOfSpeech::Particle) {
    surface_bonus += 3.0F;  // Strong penalty
  }

  // Bonus for SYMBOL → long pure-hiragana OTHER (furigana pattern)
  // E.g., 東京（とうきょう） - the hiragana in parentheses is reading/furigana
  // Long hiragana sequences after symbols should stay as single tokens
  if (prev.pos == core::PartOfSpeech::Symbol &&
      next.pos == core::PartOfSpeech::Other &&
      grammar::isPureHiragana(next.surface) &&
      next.surface.size() >= 12) {  // 4+ chars (12 bytes in UTF-8)
    surface_bonus += -3.0F;  // Strong bonus for furigana pattern
  }

  // Penalty for SYMBOL → short hiragana → AUX pattern
  // E.g., （+と+う should not happen (furigana shouldn't split into particles/aux)
  if (prev.pos == core::PartOfSpeech::Symbol &&
      next.pos == core::PartOfSpeech::Auxiliary) {
    surface_bonus += 2.0F;  // Penalty
  }

  // Penalty for AuxCopulaDa(で) + ParticleTopic(も) pattern
  // This prevents 雨+で+も split when 雨+でも (副助詞) is correct
  // But allows 何+で+も split (で=copula連用形, も=係助詞)
  // The difference: 何(Pronoun) vs 雨(Noun) - Pronoun should split
  if (prev.extended_pos == core::ExtendedPOS::AuxCopulaDa &&
      prev.surface == "で" &&
      next.extended_pos == core::ExtendedPOS::ParticleTopic &&
      next.surface == "も") {
    surface_bonus += 1.5F;  // Penalty to prefer でも as single token
  }

  // Penalty for specific Pronoun + でも patterns
  // MeCab splits "何でもあり" as 何+で+も+あり, "彼女でもない" as 彼女+で+も+ない
  // But MeCab keeps "誰でも" "どこでも" as single でも token
  // Only apply to 何 and 彼女 (limited pronoun list)
  if (prev.pos == core::PartOfSpeech::Pronoun &&
      next.surface == "でも" &&
      (prev.surface == "何" || prev.surface == "彼女")) {
    surface_bonus += 3.5F;  // Strong penalty to force で+も split
  }

  // Penalty for pronoun-like NOUN + でも pattern (limited)
  // When 何/彼女 etc. are recognized as NOUN (unknown candidate),
  // NOUN→PART_副(でも) gets -1.5 bonus from bigram table.
  // NOUN→CONJ(でも) has negative cost (-1.6) making it preferred.
  // This penalty forces で+も split for specific pronoun-like surfaces.
  // Note: 誰/どこ/いつ are excluded - "誰でも知っている" keeps でも as one token
  // Only 何 and 彼女 need splitting (何でもあり, 彼女でもない patterns)
  if (prev.pos == core::PartOfSpeech::Noun &&
      next.surface == "でも" &&
      (next.extended_pos == core::ExtendedPOS::ParticleAdverbial ||
       next.extended_pos == core::ExtendedPOS::Conjunction) &&
      (prev.surface == "何" || prev.surface == "彼女")) {
    surface_bonus += 2.5F;  // Moderate penalty to prefer で+も split
  }

  // Penalty for でも (PART_副 or CONJ) → ない/な pattern
  // MeCab splits "彼女でもない" as 彼女+で+も+ない, not 彼女+でも+ない
  // MeCab splits "雨でもない" as 雨+で+も+ない, not 雨+でも+ない
  // The pattern: NOUN+でも+ない should split でも to で+も
  // Note: Sentence-initial "でも、ない" (CONJ with punctuation) is different
  // Penalize both でも+ない and でも+な (to prevent な+い split)
  if (prev.surface == "でも" &&
      (prev.extended_pos == core::ExtendedPOS::ParticleAdverbial ||
       prev.extended_pos == core::ExtendedPOS::Conjunction) &&
      (next.surface == "ない" || next.surface == "な")) {
    surface_bonus += 3.5F;  // Strong penalty to force で+も+ない split
  }

  // Note: Removed penalty for PARTICLE と → VERB_音便 いっ pattern
  // The dictionary entry "といった" (determiner) handles that case
  // For といって pattern, we want と+いっ+て split (MeCab compatible)

  // Penalty for single-char VerbRenyokei → single-char AuxPassive
  // Bigram table gives -2.5 bonus for VerbRenyokei→AuxPassive (for 知らせ+られ)
  // But し+れ in かもしれない should not get this bonus
  // (れ is part of しれる, not passive auxiliary)
  // Valid patterns like 知らせ+られ have longer surfaces
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.extended_pos == core::ExtendedPOS::AuxPassive &&
      prev.surface.size() <= 3 && next.surface.size() <= 3) {  // Both single hiragana
    surface_bonus += 2.5F;  // Cancel the kStrongBonus
  }

  // Penalty for VerbOnbinkei ending in いい → AuxTenseTa pattern
  // E.g., 願いい+た should be 願い+いたし+ます, not 願いい (願いく) + た
  // Valid onbin forms are: 書い, 泳い, etc. (single い after kanji)
  // Invalid: 願いい (連用形い + さらにい) - this suggests wrong verb base
  if (prev.extended_pos == core::ExtendedPOS::VerbOnbinkei &&
      next.extended_pos == core::ExtendedPOS::AuxTenseTa &&
      prev.surface.size() >= 6 &&  // At least 2 hiragana (6 bytes)
      prev.surface.compare(prev.surface.size() - 6, 6, "いい") == 0) {
    surface_bonus += 3.0F;  // Strong penalty to prevent this split
  }

  // Bonus for VerbRenyokei/VerbOnbinkei → VerbRenyokei (honorific verb patterns)
  // E.g., 願い+いたし (お願いいたします), 報告+いたし (ご報告いたします)
  // Common honorific verb renyokei: いたし, おり, くださ, いただき, もらい, あげ
  // Include VerbOnbinkei since 願い is often recognized as onbin form of 願う
  if ((prev.extended_pos == core::ExtendedPOS::VerbRenyokei ||
       prev.extended_pos == core::ExtendedPOS::VerbOnbinkei) &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      (next.surface == "いたし" || next.surface == "おり" ||
       next.surface == "くださ" || next.surface == "いただき" ||
       next.surface == "もらい" || next.surface == "あげ")) {
    surface_bonus += -2.5F;  // Strong bonus for honorific verb patterns
  }

  // Bonus for honorific verb renyokei → AuxTenseMasu (ます)
  // E.g., いただき+ます (いただきます), いたし+ます (いたします)
  // This helps いただき beat い+た+だき pattern
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.extended_pos == core::ExtendedPOS::AuxTenseMasu &&
      (prev.surface == "いただき" || prev.surface == "いたし" ||
       prev.surface == "おり" || prev.surface == "くださ")) {
    surface_bonus += -2.0F;  // Strong bonus for honorific → ます pattern
  }

  // Penalty for single い → AuxTenseTa pattern (いただきます problem)
  // い+た+だき should lose to いただき+ます
  // But て+い+た is valid (食べていた)
  // We penalize い→た only when prev is OTHER (sentence start) or NOUN
  // NOT when prev comes from て-form (VerbTeForm)
  if (prev.surface == "い" &&
      prev.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.extended_pos == core::ExtendedPOS::AuxTenseTa) {
    surface_bonus += 2.0F;  // Penalty for い+た at sentence start
  }

  // Penalty for PARTICLE て → VerbTaForm いた pattern
  // MeCab splits て+い+た, not て+いた
  // いた as verb た-form should not follow て directly
  if (prev.extended_pos == core::ExtendedPOS::ParticleConj &&
      prev.surface == "て" &&
      next.extended_pos == core::ExtendedPOS::VerbTaForm &&
      next.surface == "いた") {
    surface_bonus += 2.5F;  // Strong penalty to force い+た split
  }

  // Penalty for PREFIX ご → VerbRenyokei ざい pattern
  // E.g., ございます should be ござい+ます, not ご+ざい+ます
  // The prefix ご is for nouns (ご報告), not for splitting ござる
  if (prev.extended_pos == core::ExtendedPOS::Prefix &&
      prev.surface == "ご" &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.surface.size() >= 6 &&
      next.surface.compare(0, 6, "ざい") == 0) {
    surface_bonus += 3.5F;  // Strong penalty to prevent ご+ざい split
  }

  // Surface-based bonus for AdjStem → すぎ pattern
  // E.g., 高+すぎる, 美味し+すぎた (MeCab-compatible split)
  // AdjStem→Verb has prohibitive penalty (3.5) to prevent な+い splits
  // But AdjStem+すぎ is valid grammar (i-adjective stem + すぎる)
  // Exclude VerbTeForm (すぎて) - should split as すぎ+て
  if (prev.extended_pos == core::ExtendedPOS::AdjStem &&
      next.extended_pos != core::ExtendedPOS::VerbTeForm &&
      next.surface.size() >= 6 &&  // "すぎ" is 6 bytes
      next.surface.compare(0, 6, "すぎ") == 0) {
    // Strong bonus to overcome AdjStem→Verb prohibitive penalty (3.5)
    surface_bonus += -4.0F;
  }

  // Surface-based bonus for AdjNaAdj → すぎ pattern
  // E.g., シンプル+すぎない, 静か+すぎる (na-adjective + sugiru)
  // NOUN→VERB_連用 has -1 bonus from bigram table, which can beat ADJ_NA path
  // This helps dictionary ADJ_NA entries beat unknown NOUN candidates
  if (prev.extended_pos == core::ExtendedPOS::AdjNaAdj &&
      next.surface.size() >= 6 &&
      next.surface.compare(0, 6, "すぎ") == 0) {
    surface_bonus += -1.5F;  // Extra bonus for na-adjective + sugiru
  }

  // Penalty for AuxAppearanceSou (そう) → かも to favor か+も split
  // MeCab splits "そうかもしれない" as そう+か+も+しれ+ない
  // But keeps "雨かも" as 雨+かも (noun + かも)
  // The pattern: AUX_様態 (そう) → かも should be penalized
  if (prev.extended_pos == core::ExtendedPOS::AuxAppearanceSou &&
      next.surface == "かも") {
    surface_bonus += 1.5F;  // Penalty to prefer か+も split
  }

  // Penalty for ParticleFinal(か) → ADV(もし) in かもしれない pattern
  // "もし" is a valid adverb, but not in "かもしれない" context
  // This prevents か+もし+れ split, favoring か+も+しれ
  if (prev.extended_pos == core::ExtendedPOS::ParticleFinal &&
      prev.surface == "か" &&
      next.pos == core::PartOfSpeech::Adverb &&
      next.surface == "もし") {
    surface_bonus += 2.0F;  // Strong penalty to avoid this path
  }

  // Penalty for short hiragana verb mizenkei + ん pattern
  // E.g., が+おさ+ん should be がお+さん (name + honorific suffix)
  // Short hiragana verbs followed by ん are often mis-segmented names
  // Valid patterns like 押さ+ん (kanji verb) have non-hiragana stems
  // ん can be AUX_否定古 or PART_準体, both should be penalized
  if (prev.extended_pos == core::ExtendedPOS::VerbMizenkei &&
      grammar::isPureHiragana(prev.surface) &&
      prev.surface.size() <= 6 &&  // 2 chars or less (6 bytes in UTF-8)
      next.surface == "ん") {
    surface_bonus += 3.0F;  // Strong penalty to avoid this path
  }

  // Penalty for dictionary verb inflection + ず (classical negative)
  // E.g., 思わ+ず should be 思わず (lexicalized adverb), not 思わ (dict) + ず (aux)
  // Dictionary-generated verb forms + ず rarely occur; 〜ず forms are lexicalized
  if (prev.pos == core::PartOfSpeech::Verb &&
      prev.fromDictionary() &&
      next.extended_pos == core::ExtendedPOS::AuxNegativeNu) {
    surface_bonus += 2.5F;  // Strong penalty to prefer lexicalized form
  }

  // Penalty for single-kanji noun + dictionary verb renyokei/onbinkei
  // E.g., 勘+違い should be 勘違い (compound noun), not 勘 (noun) + 違い (dict verb)
  // Single-kanji nouns rarely form valid noun+verb compounds
  // Check both dict and non-dict verb candidates that follow dictionary verbs
  if (prev.pos == core::PartOfSpeech::Noun &&
      prev.surface.size() == 3 &&  // Single kanji (3 bytes UTF-8)
      next.pos == core::PartOfSpeech::Verb &&
      (next.extended_pos == core::ExtendedPOS::VerbRenyokei ||
       next.extended_pos == core::ExtendedPOS::VerbOnbinkei)) {
    surface_bonus += 2.0F;  // Strong penalty
  }

  // Penalty for single-kanji NOUN → verbal auxiliary patterns
  // E.g., 合+う should be 合う (verb), not 合 (noun) + う (volitional)
  // E.g., 揺+れる should be 揺れる (verb), not 揺 (noun) + れる (passive)
  // Single-kanji nouns rarely take verbal auxiliaries directly
  if (prev.extended_pos == core::ExtendedPOS::Noun &&
      prev.surface.size() == 3 &&  // Single kanji (3 bytes in UTF-8)
      (next.extended_pos == core::ExtendedPOS::AuxVolitional ||
       next.extended_pos == core::ExtendedPOS::AuxPassive ||
       next.extended_pos == core::ExtendedPOS::AuxPotential ||
       next.extended_pos == core::ExtendedPOS::AuxCausative)) {
    surface_bonus += 1.5F;  // Moderate penalty
  }

  // Penalty for PARTICLE → もあり/もありだ verb candidates
  // "もあり" is mis-recognized as godan verb (もある) or suru verb (もありする)
  // In "何でもありだな", "もありだ" should be も+あり+だ, not もありする+た
  // This pattern only appears after particles (で in でもあり)
  if (prev.pos == core::PartOfSpeech::Particle &&
      next.pos == core::PartOfSpeech::Verb &&
      (next.surface == "もあり" || next.surface == "もありだ" ||
       next.surface == "もある" || next.surface == "もあっ")) {
    surface_bonus += 3.5F;  // Strong penalty to prefer も+あり split
  }

  // Bonus for PART_格 → い (VerbRenyokei of いる)
  // E.g., 家にいた → 家+に+い+た (not 家+にいた)
  // "にいた" is mis-recognized as godan verb (にく) past tense
  // い is the renyokei of いる (to exist/be), very common after particles
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.surface == "い") {
    surface_bonus += -0.5F;  // Bonus to prefer に+い split
  }

  // Bonus for VerbRenyokei → し (サ変 する renyokei)
  // E.g., お願いします → お+願い+し+ます (not お+願いし+ます)
  // "願いし" is mis-recognized as godan-sa verb (願いす)
  // This pattern is common for サ変複合動詞: 願い+する, 案内+する
  // Exclude single-char "い" which is いる renyokei (interferes with 上手い+し)
  // Exclude "で" which is でる renyokei (interferes with んでした → ん+でし+た)
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.surface == "し" && prev.fromDictionary() &&
      prev.surface != "い" && prev.surface != "で") {
    surface_bonus += -2.0F;  // Strong bonus to prefer 願い+し split
  }

  // Penalty for kanji+sokuon+kanji NOUN → し(VerbRenyokei) pattern
  // E.g., 引っ越+し should be 引っ越し (compound verb), not NOUN + suru renyokei
  // These patterns are usually compound verbs registered in dictionary
  // The pattern: 漢字+っ+漢字 (kanji + sokuon + kanji) as NOUN → し(する連用形)
  if (prev.pos == core::PartOfSpeech::Noun && !prev.fromDictionary() &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.surface == "し" &&
      prev.surface.size() >= 9 &&  // At least 3 chars (2 kanji + っ)
      utf8::contains(prev.surface, "っ")) {
    // Check if it's kanji+っ+kanji pattern
    bool has_sokuon_between_kanji = false;
    std::vector<char32_t> codepoints;
    for (char32_t cp : suzume::normalize::utf8::decode(prev.surface)) {
      codepoints.push_back(cp);
    }
    for (size_t i = 1; i + 1 < codepoints.size(); ++i) {
      if (codepoints[i] == U'っ' &&
          suzume::normalize::isKanjiCodepoint(codepoints[i - 1]) &&
          suzume::normalize::isKanjiCodepoint(codepoints[i + 1])) {
        has_sokuon_between_kanji = true;
        break;
      }
    }
    if (has_sokuon_between_kanji) {
      surface_bonus += 2.0F;  // Penalty to prefer dictionary compound verb
    }
  }

  // Penalty for PART_準体(の) → AUX_断定(で) pattern
  // This prevents 遅れているので → 遅れ|て|いる|の|で (over-split)
  // "ので" is a conjunction particle in L1 dictionary and should be 1 token
  // But "のだ" → の|だ is correct (の+copula だ for emphasis)
  // Only penalize when next.surface == "で" to preserve の|だ split
  if (prev.extended_pos == core::ExtendedPOS::ParticleNo &&
      prev.surface == "の" &&
      next.extended_pos == core::ExtendedPOS::AuxCopulaDa &&
      next.surface == "で") {
    surface_bonus += 2.0F;  // Penalty to prefer ので as single token
  }

  // Penalty for negation PREFIX (非/不/無/未) → single-kanji NOUN
  // E.g., 非常 → 非|常 is wrong (非常 is a single word, not prefix+noun)
  // E.g., 不可能 → 不|可能 is wrong (不可能 is a single word)
  // But お|茶, ご|報告 are valid (honorific prefix + noun)
  // Only penalize negation prefixes followed by single kanji
  if (prev.extended_pos == core::ExtendedPOS::Prefix &&
      (prev.surface == "非" || prev.surface == "不" ||
       prev.surface == "無" || prev.surface == "未") &&
      next.extended_pos == core::ExtendedPOS::Noun &&
      next.surface.size() == 3) {  // Single kanji (3 bytes in UTF-8)
    surface_bonus += 2.5F;  // Strong penalty to prevent this split
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
