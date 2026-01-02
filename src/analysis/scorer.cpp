#include "analysis/scorer.h"

#include <cmath>

#include "analysis/connection_rules.h"
#include "analysis/scorer_constants.h"
#include "core/debug.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"

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
    SUZUME_DEBUG_LOG("  " << reason << ": "
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

bool Scorer::isOptimalLength(const core::LatticeEdge& edge) const {
  size_t length = edge.end - edge.start;
  const auto& opt = options_.optimal_length;

  switch (edge.pos) {
    case core::PartOfSpeech::Noun: {
      // Use katakana limits for katakana sequences (longer foreign words)
      auto codepoints = normalize::utf8::decode(edge.surface);
      if (!codepoints.empty() &&
          normalize::classifyChar(codepoints[0]) == normalize::CharType::Katakana) {
        return length >= opt.katakana_min && length <= opt.katakana_max;
      }
      return length >= opt.noun_min && length <= opt.noun_max;
    }
    case core::PartOfSpeech::Verb:
      return length >= opt.verb_min && length <= opt.verb_max;
    case core::PartOfSpeech::Adjective:
      return length >= opt.adj_min && length <= opt.adj_max;
    default:
      return false;
  }
}

float Scorer::wordCost(const core::LatticeEdge& edge) const {
  float base_cost = edge.cost;
  float pos_prior = posPrior(edge.pos);
  float cost = base_cost + pos_prior;

  // Debug output with origin info
  SUZUME_DEBUG_BLOCK {
    SUZUME_DEBUG_STREAM << "[WORD] \"" << edge.surface << "\" ("
                        << core::posToString(edge.pos) << ") lemma=\""
                        << edge.lemma << "\" ";
    if (edge.fromDictionary()) {
      SUZUME_DEBUG_STREAM << "[dict]";
    } else if (edge.isUnknown()) {
#ifdef SUZUME_DEBUG_INFO
      SUZUME_DEBUG_STREAM << "[unk:" << core::originToString(edge.origin);
      if (!edge.origin_detail.empty()) {
        SUZUME_DEBUG_STREAM << " " << edge.origin_detail;
      }
      if (edge.origin_confidence > 0.0F) {
        SUZUME_DEBUG_STREAM << " conf=" << edge.origin_confidence;
      }
      SUZUME_DEBUG_STREAM << "]";
#else
      SUZUME_DEBUG_STREAM << "[unk]";
#endif
    } else {
      SUZUME_DEBUG_STREAM << "[infl]";
    }
    SUZUME_DEBUG_STREAM << ": base=" << base_cost << " pos=" << pos_prior << "\n";
  }

  // Dictionary bonus
  if (edge.fromDictionary()) {
    cost += options_.dictionary_bonus;
    logAdjustment(options_.dictionary_bonus, "dictionary");
  }
  if (edge.fromUserDict()) {
    cost += options_.user_dict_bonus;
    logAdjustment(options_.user_dict_bonus, "user_dict");
  }

  // Formal noun / low info penalty
  if (edge.isFormalNoun()) {
    cost += options_.formal_noun_penalty;
    logAdjustment(options_.formal_noun_penalty, "formal_noun");
  }
  if (edge.isLowInfo()) {
    cost += options_.low_info_penalty;
    logAdjustment(options_.low_info_penalty, "low_info");
  }

  // Single character penalty
  // Note: SUFFIX and PREFIX are exempt from single-character penalties
  // because they are grammatically expected to be single characters
  // (e.g., 様, 氏 as suffix; お, ご as prefix)
  size_t char_len = edge.end - edge.start;
  if (char_len == 1 && !edge.surface.empty() &&
      edge.pos != core::PartOfSpeech::Suffix &&
      edge.pos != core::PartOfSpeech::Prefix) {
    // Decode first character to check type
    auto codepoints = normalize::utf8::decode(edge.surface);
    if (!codepoints.empty()) {
      normalize::CharType ctype = normalize::classifyChar(codepoints[0]);
      if (ctype == normalize::CharType::Kanji) {
        // Check if single kanji exception
        // Skip penalty for:
        // - Words in kSingleKanjiExceptions (common standalone kanji)
        // - Verb stems with hasSuffix flag (見+られべき, 着+られる)
        // - Dictionary adjectives (na-adj stems like 妙, 楽 are often single kanji)
        bool skip_penalty = normalize::isSingleKanjiException(edge.surface) ||
                            (edge.pos == core::PartOfSpeech::Verb && edge.hasSuffix()) ||
                            (edge.pos == core::PartOfSpeech::Adjective && edge.fromDictionary());
        if (!skip_penalty) {
          cost += options_.single_kanji_penalty;
          logAdjustment(options_.single_kanji_penalty, "single_kanji");
        }
      } else if (ctype == normalize::CharType::Hiragana) {
        // Check if single hiragana exception (functional words)
        if (!normalize::isSingleHiraganaException(edge.surface)) {
          cost += options_.single_hiragana_penalty;
          logAdjustment(options_.single_hiragana_penalty, "single_hiragana");
        }
      }
    }
  }

  // Optimal length bonus
  if (isOptimalLength(edge)) {
    cost += options_.optimal_length_bonus;
    logAdjustment(options_.optimal_length_bonus, "optimal_length");
  }

  // Note: kPenaltyVerbSou was removed to unify verb+そう as single token
  // Previously: 走りそう → 走り + そう (2 tokens)
  // Now: 走りそう → 走る (1 token), matching 食べそう → 食べる (1 token)
  // サ変+そう (遅刻しそう) remains 2 tokens because 遅刻 is a dictionary noun
  // See: backup/technical_debt_action_plan.md section 3.3

  // Penalize unknown adjectives ending with そう with invalid lemma
  // E.g., 食べそう should not be ADJ with lemma 食べい (invalid)
  // Valid patterns (common i-adjective endings):
  //   しい: おいしい, 難しい, 美しい, 楽しい, etc. (productive pattern)
  //   さい: 小さい (small - common adjective)
  //   きい: 大きい (validated at candidate generation via verb lookup)
  // Invalid patterns (verb renyoukei + い):
  //   べい: 食べい (食べる), みい: 読みい (読む), etc.
  // Note: きい patterns are validated in adjective_candidates.cpp by checking
  // if stem + く exists as a verb (e.g., 書く exists → 書きい invalid)
  if (edge.pos == core::PartOfSpeech::Adjective &&
      !edge.fromDictionary() &&
      edge.surface.size() >= core::kTwoJapaneseCharBytes) {
    std::string_view surface = edge.surface;
    if (surface.size() >= core::kTwoJapaneseCharBytes &&
        surface.substr(surface.size() - core::kTwoJapaneseCharBytes) == scorer::kSuffixSou) {
      // Check if lemma ends with valid i-adjective pattern
      bool valid_adj_lemma = false;
      if (edge.lemma.size() >= core::kTwoJapaneseCharBytes) {
        std::string_view ending = edge.lemma.substr(edge.lemma.size() - core::kTwoJapaneseCharBytes);
        // Accept しい, さい, きい as valid i-adjective endings
        // Note: きい is validated at candidate generation (verb stem check)
        if (ending == scorer::kAdjEndingShii ||
            ending == scorer::kAdjEndingSai ||
            ending == scorer::kAdjEndingKii) {
          valid_adj_lemma = true;
        }
      }
      if (!valid_adj_lemma) {
        cost += edgeOpts().penalty_invalid_adj_sou;
        logAdjustment(edgeOpts().penalty_invalid_adj_sou, "invalid_adj_sou");
      }
    }
  }

  // Penalize unknown adjectives with lemma ending in たい where stem is invalid
  // E.g., りたかった with lemma りたい is invalid (り is not a valid verb stem)
  // Valid: 食べたかった with lemma 食べたい (食べ is valid verb stem)
  // Valid: したかった with lemma したい (し is suru stem)
  // Valid: 見たかった with lemma 見たい (見 is miru stem)
  // Valid: 来たかった with lemma 来たい (来 is kuru stem)
  if (edge.pos == core::PartOfSpeech::Adjective &&
      !edge.fromDictionary() &&
      edge.lemma.size() >= core::kTwoJapaneseCharBytes) {
    std::string_view lemma = edge.lemma;
    // Check if lemma ends with たい
    if (lemma.size() >= core::kTwoJapaneseCharBytes &&
        lemma.substr(lemma.size() - core::kTwoJapaneseCharBytes) == scorer::kSuffixTai) {
      // Get the stem before たい
      std::string_view stem = lemma.substr(0, lemma.size() - core::kTwoJapaneseCharBytes);
      // Decode stem to count codepoints
      auto stem_str = std::string(stem);
      auto codepoints = normalize::utf8::decode(stem_str);

      // Very short stems (1 char) are usually invalid unless they're known verb stems
      // Known single-char verb stems: し(する), 見(見る), 来(来る), い(いる), 出(出る), etc.
      if (codepoints.size() == 1) {
        char32_t ch = codepoints[0];
        // Allow known single-character verb stems
        if (!normalize::isValidSingleCharVerbStem(ch)) {
          cost += edgeOpts().penalty_invalid_tai_pattern;
          logAdjustment(edgeOpts().penalty_invalid_tai_pattern, "invalid_tai_pattern");
        }
      }
    }
  }

  // Penalize adjectives with lemma containing verb contraction patterns
  // E.g., 読んどい (yondoi), 飲んどい (nondoi), 見とい (mitoi) - invalid adjectives
  // These suggest verb + とく/どく contraction misanalyzed as adjective
  if (edge.pos == core::PartOfSpeech::Adjective &&
      !edge.fromDictionary() &&
      edge.lemma.size() >= core::kTwoJapaneseCharBytes) {
    // Check for patterns that look like verb contractions
    // んどい/んとい: verb onbin + contraction (読んどい, 飲んどい)
    // 〜とい: verb renyokei + contraction (見とい, 食べとい)
    bool is_ndoi_pattern = (edge.lemma.find(scorer::kPatternNdoi) != std::string::npos ||
                            edge.lemma.find(scorer::kPatternNtoi) != std::string::npos);
    bool is_toi_pattern = false;
    // Check if lemma ends with とい and stem doesn't look natural for adjective
    if (edge.lemma.size() >= core::kTwoJapaneseCharBytes &&
        edge.lemma.substr(edge.lemma.size() - core::kTwoJapaneseCharBytes) == scorer::kPatternToi) {
      // 〜とい pattern: likely verb renyokei + といて contraction
      // Exception: dictionary adjectives (none known with とい ending)
      is_toi_pattern = true;
    }
    if (is_ndoi_pattern || is_toi_pattern) {
      cost += edgeOpts().penalty_verb_onbin_as_adj;
      logAdjustment(edgeOpts().penalty_verb_onbin_as_adj, "verb_contraction_as_adj");
    }
  }

  // Penalize unknown adjectives containing verb+auxiliary patterns in surface
  // E.g., 食べすぎてしまい should be verb+しまう, not adjective
  // These patterns are also checked in inflection_scorer.cpp but the confidence floor
  // (0.5) limits the penalty effect. This scorer penalty ensures lattice cost is affected.
  if (edge.pos == core::PartOfSpeech::Adjective &&
      !edge.fromDictionary() &&
      edge.surface.size() >= core::kFourJapaneseCharBytes) {
    std::string_view surface = edge.surface;
    // Check for auxiliary patterns: てしま, でしま, ている, etc.
    // P4-6: Expanded with additional patterns (てみる, ていく, てくる, etc.)
    if (surface.find(scorer::kPatternTeShima) != std::string_view::npos ||
        surface.find(scorer::kPatternDeShima) != std::string_view::npos ||
        surface.find(scorer::kPatternTeIru) != std::string_view::npos ||
        surface.find(scorer::kPatternDeIru) != std::string_view::npos ||
        surface.find(scorer::kPatternTeMiru) != std::string_view::npos ||
        surface.find(scorer::kPatternDeMiru) != std::string_view::npos ||
        surface.find(scorer::kPatternTeIku) != std::string_view::npos ||
        surface.find(scorer::kPatternDeIku) != std::string_view::npos ||
        surface.find(scorer::kPatternTeKuru) != std::string_view::npos ||
        surface.find(scorer::kPatternDeKuru) != std::string_view::npos) {
      cost += edgeOpts().penalty_verb_aux_in_adj;
      logAdjustment(edgeOpts().penalty_verb_aux_in_adj, "verb_aux_in_adj");
    }
  }

  // Penalize しまい/じまい as adjective - these are verb しまう renyokei
  // E.g., 食べてしまい should not have しまい parsed as adjective
  if (edge.pos == core::PartOfSpeech::Adjective &&
      !edge.fromDictionary()) {
    if (edge.surface == scorer::kSurfaceShimai || edge.surface == scorer::kSurfaceJimai) {
      cost += edgeOpts().penalty_shimai_as_adj;
      logAdjustment(edgeOpts().penalty_shimai_as_adj, "shimai_as_adj");
    }
  }

  // Penalize unknown verbs ending with たいらしい - these should be split
  // E.g., 帰りたいらしい should be 帰りたい + らしい, not a single verb
  // This pattern indicates verb+たい+らしい compound that should be tokenized
  if (edge.pos == core::PartOfSpeech::Verb &&
      !edge.fromDictionary() &&
      edge.surface.size() >= core::kFiveJapaneseCharBytes) {
    std::string_view surface = edge.surface;
    std::string_view last15 = surface.substr(surface.size() - core::kFiveJapaneseCharBytes);
    if (last15 == scorer::kSuffixTaiRashii) {
      cost += edgeOpts().penalty_verb_tai_rashii;
      logAdjustment(edgeOpts().penalty_verb_tai_rashii, "verb_tai_rashii_split");
    }
  }

  // Bonus for unified verb forms containing te-form + auxiliary verb patterns
  // E.g., 言ってしまった, 教えてもらった, 食べている - these should stay unified
  // This bonus helps unified forms beat split paths where the te-form has a dictionary entry
  if (edge.pos == core::PartOfSpeech::Verb &&
      !edge.fromDictionary() &&
      edge.surface.size() >= core::kFiveJapaneseCharBytes) {
    std::string_view surface = edge.surface;
    // P4-6: Expanded auxiliary verb pattern list
    if (surface.find(scorer::kPatternTeShima) != std::string_view::npos ||
        surface.find(scorer::kPatternDeShima) != std::string_view::npos ||
        surface.find(scorer::kPatternTeIru) != std::string_view::npos ||
        surface.find(scorer::kPatternDeIru) != std::string_view::npos ||
        surface.find(scorer::kPatternTeMora) != std::string_view::npos ||
        surface.find(scorer::kPatternDeMora) != std::string_view::npos ||
        surface.find(scorer::kPatternTeOku) != std::string_view::npos ||
        surface.find(scorer::kPatternDeOku) != std::string_view::npos ||
        surface.find(scorer::kPatternTeAge) != std::string_view::npos ||
        surface.find(scorer::kPatternDeAge) != std::string_view::npos ||
        surface.find(scorer::kPatternTeKure) != std::string_view::npos ||
        surface.find(scorer::kPatternDeKure) != std::string_view::npos ||
        surface.find(scorer::kPatternTeMiru) != std::string_view::npos ||
        surface.find(scorer::kPatternDeMiru) != std::string_view::npos ||
        surface.find(scorer::kPatternTeIku) != std::string_view::npos ||
        surface.find(scorer::kPatternDeIku) != std::string_view::npos ||
        surface.find(scorer::kPatternTeKuru) != std::string_view::npos ||
        surface.find(scorer::kPatternDeKuru) != std::string_view::npos ||
        surface.find(scorer::kPatternTeAru) != std::string_view::npos ||
        surface.find(scorer::kPatternDeAru) != std::string_view::npos ||
        surface.find(scorer::kPatternTeOru) != std::string_view::npos ||
        surface.find(scorer::kPatternDeOru) != std::string_view::npos) {
      cost -= edgeOpts().bonus_unified_verb_aux;
      logAdjustment(-edgeOpts().bonus_unified_verb_aux, "unified_verb_aux");
    }
  }

  // Note: Short-stem hiragana adjective penalty moved to connectionCost
  // It only applies after PREFIX (e.g., お+いしい) not standalone (e.g., まずい)

  // Penalize unknown adjectives with lemma ending in ない where stem looks like verb mizenkei
  // E.g., 走らなければ with lemma 走らない is likely verb+aux, not true adjective
  // True adjectives ending in ない: 少ない, 危ない, つまらない (stem doesn't end in あ段)
  // Verb+ない patterns: 走らない, 書かない, 読まない (stem ends in あ段 = verb mizenkei)
  if (edge.pos == core::PartOfSpeech::Adjective &&
      !edge.fromDictionary() &&
      edge.lemma.size() >= core::kThreeJapaneseCharBytes) {  // ない + at least 1 char before
    std::string_view lemma = edge.lemma;
    // Check if lemma ends with ない
    if (lemma.size() >= core::kThreeJapaneseCharBytes &&
        lemma.substr(lemma.size() - core::kTwoJapaneseCharBytes) == scorer::kSuffixNai) {
      // Get the stem before ない
      std::string_view stem = lemma.substr(0, lemma.size() - core::kTwoJapaneseCharBytes);
      // Check if stem ends with verb mizenkei marker (あ段) or ichidan renyokei marker (え段)
      // あ段: Godan verb mizenkei (走らない, 書かない)
      // え段: Ichidan verb mizenkei (食べない, 見ない - rare, but still verb pattern)
      if (grammar::endsWithARow(stem) || grammar::endsWithERow(stem)) {
        cost += edgeOpts().penalty_verb_nai_pattern;
        logAdjustment(edgeOpts().penalty_verb_nai_pattern, "verb_nai_pattern");
      }
    }
  }

  // Penalize verbs ending with さん (contracted negative さ+ん) where stem looks nominal
  // E.g., 田中さん should be NOUN + SUFFIX, not VERB (田中する contracted negative)
  // This targets Suru verbs (田中する→田中さん) and GodanSa verbs with nominal stems
  if (edge.pos == core::PartOfSpeech::Verb &&
      edge.surface.size() >= core::kTwoJapaneseCharBytes) {
    std::string_view surface = edge.surface;
    std::string_view lemma = edge.lemma;
    // Check if surface ends with さん (6 bytes = 2 Japanese chars)
    if (surface.size() >= core::kTwoJapaneseCharBytes &&
        surface.substr(surface.size() - core::kTwoJapaneseCharBytes) == scorer::kSuffixSan) {
      // Get the stem before さん
      std::string_view stem = surface.substr(0, surface.size() - core::kTwoJapaneseCharBytes);
      // Penalize if:
      // 1. Lemma ends with する (Suru verb like 田中する) - almost always noun + honorific
      // 2. Stem ends with kanji (nominal stem)
      // 3. Lemma ends with す (GodanSa) AND stem is pure hiragana (おねえさん→おねえす)
      //    Pure hiragana GodanSa verbs ending in さん are rare; usually noun + さん
      bool is_suru_verb = (lemma.size() >= core::kTwoJapaneseCharBytes &&
                           lemma.substr(lemma.size() - core::kTwoJapaneseCharBytes) ==
                               scorer::kLemmaSuru);
      bool stem_ends_kanji = (!stem.empty() && grammar::endsWithKanji(stem));
      bool is_godan_sa_hiragana =
          (lemma.size() >= core::kJapaneseCharBytes &&
           lemma.substr(lemma.size() - core::kJapaneseCharBytes) == "す" &&
           !stem.empty() && grammar::isPureHiragana(stem));
      if (is_suru_verb || stem_ends_kanji || is_godan_sa_hiragana) {
        cost += edgeOpts().penalty_verb_san_honorific;
        logAdjustment(edgeOpts().penalty_verb_san_honorific, "verb_san_honorific");
      }
    }
  }

  // Penalize verbs ending with ん (contracted negative) with very short stem
  // E.g., いん (from いる) is rare and often misanalysis in patterns like ないんだ
  // Valid forms like わからん (4 chars) or くだらん (4 chars) are not penalized
  if (edge.pos == core::PartOfSpeech::Verb &&
      edge.surface.size() == core::kTwoJapaneseCharBytes) {  // Exactly 2 Japanese chars
    std::string_view surface = edge.surface;
    // Check if surface ends with ん
    if (surface.size() >= core::kJapaneseCharBytes &&
        surface.substr(surface.size() - core::kJapaneseCharBytes) == scorer::kSuffixN) {
      // 2-char verb ending with ん: stem is only 1 char (e.g., いん from いる)
      // Also verify it's hiragana (not kanji verbs like 見ん which are rare but exist)
      std::string_view stem = surface.substr(0, surface.size() - core::kJapaneseCharBytes);
      if (grammar::isPureHiragana(stem)) {
        cost += edgeOpts().penalty_verb_contracted_neg_short_stem;
        logAdjustment(edgeOpts().penalty_verb_contracted_neg_short_stem,
                      "verb_contracted_neg_short_stem");
      }
    }
  }

  SUZUME_DEBUG_LOG("[WORD] → total=" << cost << "\n");
  return cost;
}

float Scorer::connectionCost(const core::LatticeEdge& prev,
                             const core::LatticeEdge& next) const {
  float base_cost = bigramCost(prev.pos, next.pos);

  // Evaluate connection rules
  ConnectionRuleResult rule_result = evaluateConnectionRules(prev, next, connOpts());
  float penalty = rule_result.adjustment;
  const char* penalty_reason = rule_result.description;

  float total = base_cost + penalty;

  SUZUME_DEBUG_LOG("[CONN] \"" << prev.surface << "\" ("
                    << core::posToString(prev.pos) << ") → \""
                    << next.surface << "\" ("
                    << core::posToString(next.pos) << "): "
                    << "base=" << base_cost);
  if (penalty != 0.0F && penalty_reason != nullptr) {
    if (penalty > 0.0F) {
      SUZUME_DEBUG_LOG(" + penalty=" << penalty << " (" << penalty_reason << ")");
    } else {
      SUZUME_DEBUG_LOG(" + bonus=" << -penalty << " (" << penalty_reason << ")");
    }
  }
  SUZUME_DEBUG_LOG(" = " << total << "\n");

  return total;
}

}  // namespace suzume::analysis
