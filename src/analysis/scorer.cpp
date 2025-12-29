#include "analysis/scorer.h"

#include "analysis/connection_rules.h"
#include "analysis/scorer_constants.h"
#include "core/debug.h"
#include "core/utf8_constants.h"
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
// clang-format off
constexpr float kBigramCostTable[13][13] = {
    //        Noun  Verb  Adj   Adv   Part  Aux   Conj  Det   Pron  Pref  Suff  Sym   Other
    /* Noun */ {0.0F, 0.5F, 0.5F, 0.3F, 0.0F, 0.0F, 0.5F, 0.5F, 0.5F, 1.0F,-0.8F, 0.5F, 0.5F},
    /* Verb */ {0.2F, 0.8F, 0.8F, 0.5F, 0.0F, 0.0F, 0.5F, 0.5F, 0.2F, 1.0F, 0.8F, 0.5F, 0.5F},
    /* Adj  */ {0.2F, 0.5F, 0.8F, 0.3F, 0.0F, 0.5F, 0.5F, 0.5F, 0.2F, 1.0F, 0.8F, 0.5F, 0.5F},
    /* Adv  */ {0.0F, 0.3F, 0.0F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.0F, 1.0F, 0.8F, 0.5F, 0.5F},
    /* Part */ {0.0F, 0.2F, 0.2F, 0.3F, 0.5F, 0.5F, 0.5F, 0.3F, 0.0F, 1.0F, 1.0F, 0.5F, 0.5F},
    /* Aux  */ {0.5F, 0.5F, 0.5F, 0.5F, 0.0F, 0.3F, 0.5F, 0.5F, 0.5F, 1.0F, 0.8F, 0.5F, 0.5F},
    /* Conj */ {0.0F, 0.2F, 0.2F, 0.2F, 0.3F, 0.5F, 0.5F, 0.2F, 0.0F, 0.3F, 1.0F, 0.3F, 0.3F},
    /* Det  */ {0.0F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.8F, 0.0F, 1.0F, 0.8F, 0.5F, 0.5F},
    /* Pron */ {0.0F, 0.5F, 0.5F, 0.3F, 0.0F, 1.0F, 0.5F, 0.5F, 0.5F, 1.0F, 0.0F, 0.5F, 0.5F},
    /* Pref */{-1.5F, -0.5F,-0.3F, 0.5F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F},
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

float Scorer::bigramCost(core::PartOfSpeech prev, core::PartOfSpeech next) {
  return kBigramCostTable[posToIndex(prev)][posToIndex(next)];
}

bool Scorer::isOptimalLength(const core::LatticeEdge& edge) const {
  size_t length = edge.end - edge.start;
  const auto& opt = options_.optimal_length;

  switch (edge.pos) {
    case core::PartOfSpeech::Noun:
      return length >= opt.noun_min && length <= opt.noun_max;
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
  if (core::Debug::isEnabled()) {
    core::Debug::log() << "[WORD] \"" << edge.surface << "\" ("
                       << core::posToString(edge.pos) << ") lemma=\""
                       << edge.lemma << "\" ";
    if (edge.fromDictionary()) {
      core::Debug::log() << "[dict]";
    } else if (edge.isUnknown()) {
#ifdef SUZUME_DEBUG_INFO
      core::Debug::log() << "[unk:" << core::originToString(edge.origin);
      if (!edge.origin_detail.empty()) {
        core::Debug::log() << " " << edge.origin_detail;
      }
      if (edge.origin_confidence > 0.0F) {
        core::Debug::log() << " conf=" << edge.origin_confidence;
      }
      core::Debug::log() << "]";
#else
      core::Debug::log() << "[unk]";
#endif
    } else {
      core::Debug::log() << "[infl]";
    }
    core::Debug::log() << ": base=" << base_cost << " pos=" << pos_prior << "\n";
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
        if (!normalize::isSingleKanjiException(edge.surface)) {
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
        surface.substr(surface.size() - core::kTwoJapaneseCharBytes) == "そう") {
      // Check if lemma ends with valid i-adjective pattern
      bool valid_adj_lemma = false;
      if (edge.lemma.size() >= core::kTwoJapaneseCharBytes) {
        std::string_view ending = edge.lemma.substr(edge.lemma.size() - core::kTwoJapaneseCharBytes);
        // Accept しい, さい, きい as valid i-adjective endings
        // Note: きい is validated at candidate generation (verb stem check)
        if (ending == "しい" || ending == "さい" || ending == "きい") {
          valid_adj_lemma = true;
        }
      }
      if (!valid_adj_lemma) {
        cost += scorer::kPenaltyInvalidAdjSou;
        logAdjustment(scorer::kPenaltyInvalidAdjSou, "invalid_adj_sou");
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
        lemma.substr(lemma.size() - core::kTwoJapaneseCharBytes) == "たい") {
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
          cost += scorer::kPenaltyInvalidTaiPattern;
          logAdjustment(scorer::kPenaltyInvalidTaiPattern, "invalid_tai_pattern");
        }
      }
    }
  }

  // Penalize unknown adjectives containing verb+auxiliary patterns in surface
  // E.g., 食べすぎてしまい should be verb+しまう, not adjective
  // These patterns are also checked in inflection_scorer.cpp but the confidence floor
  // (0.5) limits the penalty effect. This scorer penalty ensures lattice cost is affected.
  if (edge.pos == core::PartOfSpeech::Adjective &&
      !edge.fromDictionary() &&
      edge.surface.size() >= 12) {
    std::string_view surface = edge.surface;
    // Check for auxiliary patterns: てしま, でしま, ている, etc.
    if (surface.find("てしま") != std::string_view::npos ||
        surface.find("でしま") != std::string_view::npos ||
        surface.find("ている") != std::string_view::npos ||
        surface.find("でいる") != std::string_view::npos) {
      cost += scorer::kPenaltyVerbAuxInAdj;
      logAdjustment(scorer::kPenaltyVerbAuxInAdj, "verb_aux_in_adj");
    }
  }

  // Penalize しまい/じまい as adjective - these are verb しまう renyokei
  // E.g., 食べてしまい should not have しまい parsed as adjective
  if (edge.pos == core::PartOfSpeech::Adjective &&
      !edge.fromDictionary()) {
    if (edge.surface == "しまい" || edge.surface == "じまい") {
      cost += scorer::kPenaltyShimaiAsAdj;
      logAdjustment(scorer::kPenaltyShimaiAsAdj, "shimai_as_adj");
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
        lemma.substr(lemma.size() - core::kTwoJapaneseCharBytes) == "ない") {
      // Get the stem before ない
      std::string_view stem = lemma.substr(0, lemma.size() - core::kTwoJapaneseCharBytes);
      // Check the last character of stem
      if (stem.size() >= core::kJapaneseCharBytes) {
        std::string_view last3 = stem.substr(stem.size() - core::kJapaneseCharBytes);
        // あ段 hiragana (Godan verb mizenkei endings)
        // ら, か, が, さ, た, な, ば, ま, わ, あ
        if (last3 == "ら" || last3 == "か" || last3 == "が" ||
            last3 == "さ" || last3 == "た" || last3 == "ば" ||
            last3 == "ま" || last3 == "わ" || last3 == "あ" ||
            // Also check for ichidan-like patterns (e-row + ない is less common for adj)
            last3 == "べ" || last3 == "め" || last3 == "せ" ||
            last3 == "け" || last3 == "げ" || last3 == "て" ||
            last3 == "ね" || last3 == "れ" || last3 == "え") {
          cost += scorer::kPenaltyVerbNaiPattern;
          logAdjustment(scorer::kPenaltyVerbNaiPattern, "verb_nai_pattern");
        }
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
  ConnectionRuleResult rule_result = evaluateConnectionRules(prev, next);
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
