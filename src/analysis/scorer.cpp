#include "analysis/scorer.h"

#include <cmath>

#include "analysis/bigram_table.h"
#include "analysis/category_cost.h"
#include "analysis/scorer_constants.h"
#include "analysis/verb_candidates_helpers.h"
#include "core/debug.h"
#include "core/kana_constants.h"
#include "core/types.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "normalize/utf8.h"

#ifdef SUZUME_DEBUG_INFO
using suzume::core::CandidateOrigin;
#endif
namespace cost = suzume::analysis::bigram_cost;
namespace sc = suzume::analysis::scorer;

// Surface-based adjustments use cost:: namespace directly from bigram_cost.
// See bigram_table.h and scorer_constants.h for constant values.

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
    case suzume::core::PartOfSpeech::Interjection:
    case suzume::core::PartOfSpeech::Other:
    case suzume::core::PartOfSpeech::Unknown:
    case suzume::core::PartOfSpeech::Count_:
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
    /* Adj  */ {0.2F, 0.5F, 0.8F, 0.3F, 0.0F, 0.0F, 0.5F, 0.5F, 0.2F, 1.0F, 0.8F, 0.5F, 0.5F},  // Aux: 0.5→0.0 for おいしそう (ADJ_STEM+AUX)
    /* Adv  */ {0.0F, 0.3F, 0.0F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.0F, 1.0F, 0.8F, 0.5F, 0.5F},
    /* Part */ {0.0F, 0.2F, 0.2F, 0.3F, 0.5F, 0.5F, 0.5F, 0.3F, 0.0F, 0.3F, 1.0F, 0.5F, 0.5F},  // Pref: 1.0→0.3 (何番線: は→何PREFIX)
    /* Aux  */ {0.5F, 0.5F, 0.5F, 0.5F, 0.0F, 0.3F, 0.5F, 0.5F, 0.5F, 1.0F, 0.8F, 0.5F, 0.5F},
    /* Conj */ {0.0F, 0.2F, 0.2F, 0.2F, 0.3F, 0.5F, 0.5F, 0.2F, 0.0F, 0.3F, 1.0F, 0.3F, 0.3F},
    /* Det  */ {0.0F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.8F, 0.0F, 1.0F, 1.5F, 0.5F, 0.5F},  // Suff: 0.8→1.5 (あんな人: NOUN優先)
    /* Pron */ {0.0F, 0.5F, 0.5F, 0.3F, 0.0F, 0.2F, 0.5F, 0.5F, 0.5F, 1.0F,-0.8F, 0.5F, 0.5F},  // P3-1: Aux 1.0→0.2 (私だ is basic); Suff: 0.0→-0.8 (彼女+ら)
    /* Pref */{-0.5F,-0.5F, 0.0F, 0.5F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F},
    /* Suff */ {0.5F, 0.8F, 0.8F, 0.5F, 0.0F, 0.5F, 0.5F, 0.5F, 0.5F, 1.0F, 0.3F, 0.5F, 0.5F},
    /* Sym  */ {0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.5F, 0.0F, 0.2F},
    /* Other*/ {0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.5F, 0.5F, 0.2F, 0.2F},
};
// clang-format on

}  // namespace

namespace suzume::analysis {

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

  // Length-scaled bonus helper: base + per_char * (char_len - min_len)
  auto lengthScaledBonus = [](float base, size_t char_len, size_t min_len, float per_char) {
    return base - static_cast<float>(char_len > min_len ? char_len - min_len : 0) * per_char;
  };

  // User dictionary bonus (still needed for user customization)
  if (edge.fromUserDict()) {
    cost += options_.user_dict_bonus;
  }

  // Bonus for hiragana i-adjectives from dictionary
  // Prevents misanalysis as verb+たい (e.g., つめたい → つめ+たい)
  // or as adverb+verb+aux (e.g., はなはだしい → はなはだ+し+い)
  // Longer adjectives get stronger bonus to beat split paths
  // Exclude AdjStem (語幹) as it's not a complete i-adjective
  // Exclude conditional forms ending in ければ (should split: よければ → よけれ + ば)
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Adjective &&
      edge.extended_pos != core::ExtendedPOS::AdjStem && grammar::isPureHiragana(edge.surface) &&
      !utf8::endsWith(edge.surface, "ければ") &&
      // Exclude ない/なく/なかっ - has auxiliary counterpart, context-dependent
      // Exclude そう - has auxiliary counterpart (様態), context-dependent
      edge.surface != "ない" && edge.surface != "なく" && edge.surface != "なかっ" && edge.surface != "そう") {
    size_t char_len = suzume::normalize::utf8Length(edge.surface);
    // Base bonus -2.5, plus 0.5 per character beyond 3
    float bonus = (char_len <= 3) ? -2.5F : -2.5F - static_cast<float>(char_len - 3) * 0.5F;
    cost += bonus;
  }

  // Bonus for kanji+い i-adjectives from dictionary
  // Prevents misanalysis as godan-wa verb (e.g., 暑い → 暑い(VERB wa-row renyokei))
  // Kanji i-adjectives are common (暑い, 寒い, 熱い, 高い, 安い, etc.)
  // The godan-wa verb candidate often beats the adjective due to connection bonuses
  // Surface pattern: 1 kanji + い (2 chars total)
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Adjective &&
      edge.extended_pos != core::ExtendedPOS::AdjStem &&
      edge.surface.size() == 6 &&  // 2 chars (1 kanji + い) = 6 bytes
      utf8::endsWith(edge.surface, "い") &&
      grammar::isAllKanji(std::string(edge.surface.substr(0, 3)))) {  // First char is kanji
    cost += cost::kModerateBonus;                                     // -0.5 to beat godan-wa verb candidate
  }

  // Bonus for kanji+okurigana i-adjectives from dictionary (情けない, etc.)
  // These compete with verb renyokei + ない split paths that get strong
  // VERB_連用→AUX_否定 connection bonus (-0.8).
  // Pattern: kanji-containing, 4+ chars, ending in い, from dictionary
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Adjective &&
      edge.extended_pos != core::ExtendedPOS::AdjStem && grammar::containsKanji(edge.surface) &&
      edge.surface.size() >= 4 * core::kJapaneseCharBytes && utf8::endsWith(edge.surface, "い")) {
    size_t char_len = suzume::normalize::utf8Length(edge.surface);
    cost += lengthScaledBonus(-1.5F, char_len, 4, 0.3F);
  }

  // Bonus for hiragana adverbs from dictionary
  // Prevents misanalysis as verb+ん (e.g., たくさん → たくさ+ん)
  // and compound splits (e.g., どうして → どう+し+て)
  // Longer adverbs get stronger bonus to beat split paths
  // Short adverbs (2 chars) get weaker bonus to avoid false matches in patterns
  // like かもしれない (should not be か+もし+れない)
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Adverb && grammar::isPureHiragana(edge.surface)) {
    size_t char_len = suzume::normalize::utf8Length(edge.surface);
    // Short adverbs (2 chars) get weaker bonus
    // Longer adverbs get stronger bonus (0.5 per character beyond 2)
    float bonus = (char_len <= 2) ? -1.0F : -2.5F - static_cast<float>(char_len - 2) * 0.5F;
    cost += bonus;
  }

  // Bonus for non-hiragana adverbs from dictionary (初めて, 大して, etc.)
  // These contain kanji so the pure-hiragana adverb bonus above doesn't apply.
  // They compete with verb renyokei + て split paths which get connection bonuses.
  // E.g., 初めて(ADV, cost=0.5) vs 初め(VERB_連用, -0.13) + て(PART, conn=-0.5)
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Adverb && !grammar::isPureHiragana(edge.surface)) {
    size_t char_len = suzume::normalize::utf8Length(edge.surface);
    if (char_len >= 3) {
      cost += lengthScaledBonus(-1.5F, char_len, 3, 0.3F);
    }
  }

  // Bonus for dictionary determiners/adnominals containing kanji (小さな, 大きな, etc.)
  // These compete with ADJ_語幹 + suffix split paths which get connection bonuses.
  // E.g., 小さな(DET, cost=0.4) vs 小(ADJ_語幹, -0.68) + さ(SUFFIX, 0) + な(AUX)
  // Only apply to kanji-containing entries to avoid boosting pure hiragana determiners
  // like といった which should remain as particles
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Determiner && grammar::containsKanji(edge.surface)) {
    size_t char_len = suzume::normalize::utf8Length(edge.surface);
    if (char_len >= 3) {
      cost += lengthScaledBonus(-1.5F, char_len, 3, 0.3F);
    }
  }

  // Bonus for longer hiragana nouns from dictionary (ふともも, ひとつ, etc.)
  // These compete with adverb+noun split paths that get adverb bonus + connection bonus.
  // E.g., ふともも(NOUN, 0.5) vs ふと(ADV, -0.5) + もも(NOUN, 0.5, conn=-0.5) = -0.5
  // Without bonus, the split path wins even though the longer dict match is better.
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Noun && grammar::isPureHiragana(edge.surface)) {
    size_t char_len = suzume::normalize::utf8Length(edge.surface);
    if (char_len >= 4) {
      cost += -1.5F;
    }
  }

  // Bonus for hiragana interjections/greetings from dictionary
  // Prevents misanalysis like さようなら → さ+よう+なら (volitional pattern)
  // or ありがとう → あり+が+とう (verb + particle + noun pattern)
  // These are fixed expressions that should remain as single tokens
  // Longer interjections get stronger bonus to beat common split patterns
  // Note: applies to both Interjection (L1/L2) and Other (legacy)
  if (edge.fromDictionary() &&
      (edge.pos == core::PartOfSpeech::Interjection || edge.pos == core::PartOfSpeech::Other) &&
      grammar::isPureHiragana(edge.surface)) {
    size_t char_len = suzume::normalize::utf8Length(edge.surface);
    // Stronger bonus for longer interjections (common greetings are 4-5 chars)
    float bonus = (char_len <= 2) ? -0.5F : (char_len <= 3) ? -1.5F : -2.0F - static_cast<float>(char_len - 3) * 0.5F;
    cost += bonus;
  }

  // Bonus for non-hiragana interjections from dictionary (お疲れ様, etc.)
  // Mixed script interjections also need bonus to beat split paths
  // E.g., お疲れ様 should not split as お+疲れ+様
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Interjection && !grammar::isPureHiragana(edge.surface)) {
    size_t char_len = suzume::normalize::utf8Length(edge.surface);
    // Moderate bonus for mixed interjections
    cost += lengthScaledBonus(-0.5F, char_len, 3, 0.3F);
  }

  // Bonus for hiragana conjunctions from dictionary (たとえば, それから, etc.)
  // Prevents misanalysis like たとえば → たとえ+ば (adverb + particle)
  // These are fixed expressions that should remain as single tokens
  // Needs to beat adverb bonus path, so use stronger bonus
  // Exclude でも - it has ambiguous interpretation (conjunction vs 副助詞)
  // and context-dependent splitting (彼女でもない → 彼女+で+も+ない)
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Conjunction && grammar::isPureHiragana(edge.surface) &&
      edge.surface != "でも") {
    size_t char_len = suzume::normalize::utf8Length(edge.surface);
    // Stronger bonus for conjunctions to beat adverb+particle splits
    // Adverb 3-char gets -3.0, plus particle gets bonus, so we need > -3.5
    float bonus = (char_len <= 3) ? -2.0F : -3.5F - static_cast<float>(char_len - 3) * 0.5F;
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
      cost += sc::kBonusCompoundParticle;
    }
  }

  // Bonus for みたい (conjecture auxiliary) from dictionary
  // Works together with bigram bonuses and spurious verb penalty
  if (edge.fromDictionary() && edge.extended_pos == core::ExtendedPOS::AuxConjectureMitai) {
    cost += sc::kBonusMitaiDict;
  }

  // Bonus for dictionary entries starting with negation prefixes (非/不/無/未)
  // E.g., 不可能, 非常, 無理, 無限, 無鉄砲 - these are single lexical items
  // Without this bonus, PREFIX+NOUN split path wins due to strong connection bonus (-2)
  // Dictionary entries should take precedence over compositional analysis
  // Scales with length so longer entries (無理やり) beat shorter ones (無理)
  if (edge.fromDictionary() &&
      (edge.pos == core::PartOfSpeech::Adjective || edge.pos == core::PartOfSpeech::Noun ||
       edge.pos == core::PartOfSpeech::Adverb) &&
      edge.surface.size() >= 6 &&  // At least 2 chars (prefix + something)
      (edge.surface.compare(0, 3, "非") == 0 || edge.surface.compare(0, 3, "不") == 0 ||
       edge.surface.compare(0, 3, "無") == 0 || edge.surface.compare(0, 3, "未") == 0)) {
    size_t char_len = suzume::normalize::utf8Length(edge.surface);
    // Base -3.0 for 2-char entries, -0.5 per additional char
    cost += lengthScaledBonus(-3.0F, char_len, 2, 0.5F);
  }

  // Bonus for dictionary NOUN entries starting with honorific prefix kanji 御
  // E.g., 御者, 御所, 御曹司 are lexicalized words where 御 is part of the noun,
  // not a separable prefix. Without this bonus, the strong PREFIX→NOUN bigram
  // bonus (-1.3) makes 御(PREFIX) + X split path cheaper than the dict entry.
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Noun && edge.surface.size() >= 6 &&
      edge.surface.compare(0, 3, "御") == 0) {
    size_t char_len = suzume::normalize::utf8Length(edge.surface);
    cost += lengthScaledBonus(-1.0F, char_len, 2, 0.3F);
  }

  // Bonus for hiragana+kanji mixed nouns from dictionary (e.g., なし崩し, みじん切り, お茶)
  // These are idiomatic expressions that should not be split
  // E.g., なし崩し should not be split as な+し+崩し (AUX+PARTICLE+NOUN)
  // Requires 3+ chars with both hiragana and kanji
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Noun) {
    size_t char_len = suzume::normalize::utf8Length(edge.surface);
    if (char_len >= 3 && grammar::isMixedHiraganaKanji(edge.surface)) {
      if (char_len >= 4) {
        // Length-scaled bonus for long mixed nouns (お兄ちゃん, お父さん, なし崩し)
        cost += lengthScaledBonus(sc::kBonusLongMixedNounBase, char_len, 4, -sc::kBonusLongMixedNounPerChar);
      } else {
        cost += sc::kBonusMixedNoun;
      }
    }
  }

  // Bonus for long all-kanji nouns from dictionary (4+ chars)
  // Split path gets dict+dict connection bonus (-0.5) and split_candidates
  // both-in-dict bonus (-0.2), making it -0.7 cheaper than 1-token path.
  // Length-scaled bonus ensures registered compounds beat split paths.
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Noun && grammar::isAllKanji(edge.surface)) {
    size_t char_len = suzume::normalize::utf8Length(edge.surface);
    if (char_len >= 4) {
      cost += lengthScaledBonus(sc::kBonusLongKanjiNounBase, char_len, 4, -sc::kBonusLongKanjiNounPerChar);
    }
  }

  // Bonus for multi-char hiragana suffixes from dictionary (e.g., まみれ, だらけ, ごと)
  // These are L1 closed-class morphemes that should beat false verb candidates
  // E.g., 血まみれ should be 血+まみれ(SUFFIX), not 血まみ(VERB)+れ(AUX)
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Suffix && grammar::isPureHiragana(edge.surface) &&
      edge.surface.size() >= 9) {  // 3+ chars (9+ bytes)
    cost += sc::kBonusLongSuffix;
  }

  // Bonus for short hiragana verbs from dictionary (e.g., なる, ある, いる, する)
  // These compete with L1 function word entries (DET, AUX) which have lower category costs.
  // Dictionary registration indicates standalone verb usage should take precedence.
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Verb && grammar::isPureHiragana(edge.surface) &&
      edge.surface.length() <= 6) {  // ≤2 chars
    cost += sc::kBonusShortHiraganaVerb;
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
      cost += sc::kPenaltySpuriousVerbRenyokei;
    }
  }

  // Penalty for pure-hiragana hatsuonbin (撥音便) verb forms not in dictionary
  // E.g., "おさん" as 撥音便 of "おさむ" is rare and likely mis-segmentation
  // Valid hatsuonbin usually has kanji stem (読ん, 飲ん, 呼ん)
  // This helps prevent が+おさん misanalysis (should be がお+さん)
  // Exception: short hiragana verbs (2-4 chars like もらっ, あげっ) get reduced penalty
  // as they are more likely to be legitimate verbs written in hiragana
  if (!edge.fromDictionary() && edge.pos == core::PartOfSpeech::Verb &&
      edge.extended_pos == core::ExtendedPOS::VerbOnbinkei && grammar::isPureHiragana(edge.surface) &&
      edge.surface.size() >= 6) {  // 2+ chars (avoid single-char like ん)
    // Reduced penalty for short forms (2-4 chars = 6-12 bytes)
    // to allow common hiragana verbs like もらっ, あげっ to compete
    cost += (edge.surface.size() <= 12) ? sc::kPenaltyHatsuonbinShort : sc::kPenaltyHatsuonbinLong;
  }

  // Penalty for pure-hiragana verb forms containing "さん" pattern
  // E.g., "おさんで" as te-form of "おさむ" is likely name+さん+で misanalysis
  // E.g., "さんで" as te-form of "さむ" is likely さん+で misanalysis
  // Patterns: xさん, xさんで, さんで where x is short hiragana (likely name)
  // This complements the hatsuonbin penalty above for other verb forms
  if (!edge.fromDictionary() && edge.pos == core::PartOfSpeech::Verb && grammar::isPureHiragana(edge.surface) &&
      (utf8::contains(edge.surface, "さん"))) {
    size_t san_pos = edge.surface.find("さん");
    if (san_pos != std::string::npos) {
      // Penalize if:
      // 1. さん appears after 0-2 hiragana chars (likely name+さん or just さん)
      // 2. The verb form is short enough to be a misanalysis
      if (san_pos <= 6 && edge.surface.size() <= 15) {  // 0-2 chars before, up to 5 total
        cost += sc::kPenaltySanPatternVerb;
      }
    }
  }

  // Penalty for pure-hiragana ichidan verb renyokei starting with に
  // E.g., "につけ" as renyokei of "につける" is spurious
  // Should be に|つけ (particle + verb), not につけ (verb)
  // Valid verbs like "につける" don't exist; this is a mis-analysis
  if (!edge.fromDictionary() && edge.pos == core::PartOfSpeech::Verb &&
      edge.extended_pos == core::ExtendedPOS::VerbRenyokei && grammar::isPureHiragana(edge.surface) &&
      utf8::startsWith(edge.surface, "に") && edge.surface.size() >= 6 && edge.surface.size() <= 12) {  // 2-4 chars
    cost += sc::kPenaltyNiPrefixVerb;
  }

  // Penalty for very long pure-hiragana verb candidates not in dictionary
  // E.g., "ございませんでし" as verb renyokei is spurious
  // Should be ござい|ませ|ん|でし (aux chain), not ございませんでし (verb)
  // Valid long verbs typically have kanji stems
  if (!edge.fromDictionary() && edge.pos == core::PartOfSpeech::Verb && grammar::isPureHiragana(edge.surface) &&
      edge.surface.size() >= 18) {  // 6+ hiragana chars (6*3=18 bytes)
    cost += sc::kPenaltyVeryLongHiraganaVerb;
  }

  // Penalty for 5-char pure-hiragana verb renyokei not in dictionary
  // E.g., "つるつるし" as godan-sa renyokei — should be つるつる(ADV) + し(する)
  // Only renyokei: base forms like "づけられる" (from づける) are legitimate
  if (!edge.fromDictionary() && edge.pos == core::PartOfSpeech::Verb &&
      edge.extended_pos == core::ExtendedPOS::VerbRenyokei && grammar::isPureHiragana(edge.surface) &&
      edge.surface.size() >= 15) {  // 5+ hiragana chars (5*3=15 bytes)
    cost += sc::kPenaltyVeryLongHiraganaVerb;
  }

  // Penalty for kanji+hiragana verb renyokei ending in いし pattern
  // E.g., "願いし" as renyokei of "願いす" is spurious
  // Should be 願い + し (願う renyokei + する renyokei)
  // The いし ending suggests the analyzer incorrectly merged a renyokei い
  // with the following し (suru renyokei)
  // Valid pattern: 漢字 + い (renyokei) vs invalid: 漢字 + いし (fake verb base 漢字いす)
  if (!edge.fromDictionary() && edge.pos == core::PartOfSpeech::Verb &&
      edge.extended_pos == core::ExtendedPOS::VerbRenyokei && grammar::containsKanji(edge.surface) &&
      utf8::endsWith(edge.surface, "いし") && edge.surface.size() >= 9) {  // At least 1 kanji + いし (3 + 6 bytes)
    cost += sc::kPenaltyIshiVerbRenyokei;
  }

  // Penalty for pure-hiragana verb candidates ending with そう
  // E.g., "なさそう" should be な + さ + そう, not なさそう (verb)
  // The そう ending is typically from そう (様態 auxiliary), not a verb stem
  // Valid verbs ending in そう are rare and usually have kanji stems
  if (!edge.fromDictionary() && edge.pos == core::PartOfSpeech::Verb && grammar::isPureHiragana(edge.surface) &&
      utf8::endsWith(edge.surface, "そう") && edge.surface.size() >= 9) {  // 3+ chars (at least xそう)
    cost += cost::kRare;
  }

  // Penalty for pure-hiragana verb candidates ending with てき
  // E.g., "なってき" should be なっ + て + き (来る), not なってき (verb)
  // The てき ending is almost always て (particle) + き/こ (来る auxiliary)
  // Exception: できる is valid but is in dictionary
  if (!edge.fromDictionary() && edge.pos == core::PartOfSpeech::Verb && grammar::isPureHiragana(edge.surface) &&
      utf8::endsWith(edge.surface, "てき") && edge.surface.size() >= 9) {  // 3+ chars (at least xてき)
    cost += cost::kVeryRare;
  }

  // Penalty for pure-hiragana verb candidates ending with まし
  // E.g., "しまし" should be し + まし (masu renyokei), not しまし (verb)
  // The まし ending is almost always ます (polite aux) renyokei form
  if (!edge.fromDictionary() && edge.pos == core::PartOfSpeech::Verb && grammar::isPureHiragana(edge.surface) &&
      utf8::endsWith(edge.surface, "まし") && edge.surface.size() >= 9) {  // 3+ chars (at least xまし)
    cost += cost::kVeryRare;
  }

  // Penalty for pure-hiragana verb candidates ending with てい
  // E.g., "させてい" should be させ + て + い (progressive), not させてい (verb)
  // The てい ending is almost always て (particle) + い (いる renyokei)
  if (!edge.fromDictionary() && edge.pos == core::PartOfSpeech::Verb && grammar::isPureHiragana(edge.surface) &&
      utf8::endsWith(edge.surface, "てい") && edge.surface.size() >= 9) {  // 3+ chars (at least xてい)
    cost += cost::kVeryRare;
  }

  // Penalty for pure-hiragana verb te-form candidates not in dictionary
  // E.g., "もらって" should be もらっ + て, not もらって (verb te-form)
  // E.g., "ねて" should be ね + て, not ねて (verb te-form)
  // MeCab splits pure-hiragana verb te-forms into verb + て particle
  // Exception: keep short forms (2 chars like して, きて) as they're common L1 entries
  if (!edge.fromDictionary() && edge.pos == core::PartOfSpeech::Verb &&
      edge.extended_pos == core::ExtendedPOS::VerbTeForm && grammar::isPureHiragana(edge.surface) &&
      edge.surface.size() >= 9) {  // 3+ chars (9 bytes) - allows して, きて
    cost += cost::kVeryRare;
  }

  // Penalty for kanji+hiragana verb te-form candidates (e.g., 押して, 泳いで)
  // MeCab splits these as 押し + て, 泳い + で
  // Single kanji + て/で pattern is most common: 押して, 待って, 書いて, etc.
  // This penalty encourages verb_stem + て particle split
  // Apply to both dict and non-dict candidates as some come from auto-generation
  if (edge.pos == core::PartOfSpeech::Verb && edge.extended_pos == core::ExtendedPOS::VerbTeForm &&
      grammar::containsKanji(edge.surface) &&
      (utf8::endsWith(edge.surface, "て") || utf8::endsWith(edge.surface, "で")) &&
      edge.surface.size() <= 12) {  // Short te-forms (1-2 kanji + て/で)
    cost += cost::kSevere;          // Very strong penalty to overcome negative costs
  }

  // Penalty for kanji+hiragana verb ta-form candidates (e.g., 書いた, 泳いだ)
  // MeCab splits these as 書い + た, 泳い + だ
  // Single kanji + いた/いだ pattern is most common (godan i-onbin + ta/da)
  // This penalty encourages verb_onbin + た/だ auxiliary split
  if (!edge.fromDictionary() && edge.pos == core::PartOfSpeech::Verb &&
      edge.extended_pos == core::ExtendedPOS::VerbTaForm && grammar::containsKanji(edge.surface) &&
      (utf8::endsWith(edge.surface, "いた") || utf8::endsWith(edge.surface, "いだ")) &&
      edge.surface.size() <= 12) {  // Short ta-forms (1-2 kanji + いた/いだ)
    cost += cost::kSevere;          // Strong penalty to prefer onbin + auxiliary split
  }

  // Bonus for compound adjectives from dictionary (e.g., 男らしい, 女らしい)
  // These compete with noun+らしい split which has -1.5 connection bonus.
  // Dictionary registration indicates compound adjective should take precedence.
  // Pattern: kanji stem + hiragana suffix forming an i-adjective
  if (edge.fromDictionary() && edge.pos == core::PartOfSpeech::Adjective &&
      edge.surface.length() >= 12) {  // ≥4 chars (kanji + ひらがな suffix)
    // Check if surface contains kanji — compound adjective from dictionary
    // Covers both base form (い) and inflected forms (く, かっ, けれ, etc.)
    if (grammar::containsKanji(edge.surface)) {
      // Longer compounds need stronger bonus to beat noun+adj split paths
      // Must overcome NOUN→dict_ADJ surface bonus (-0.5) on the split path
      size_t char_len = suzume::normalize::utf8Length(edge.surface);
      float bonus = sc::kBonusCompoundAdjBase -
                    static_cast<float>(char_len > 4 ? char_len - 4 : 0) * sc::kBonusCompoundAdjPerChar;
      cost += bonus;
    }
  }

  // Penalty for kanji compound NOUN ending with 中 (chuu/juu suffix)
  // E.g., "一日中" should be split as 一日|中 (noun + suffix)
  // Registered compounds like "世界中" will also split (accepted difference from MeCab)
  // This helps Suffix 中 candidates win over NOUN compounds
  if (!edge.fromDictionary() && edge.pos == core::PartOfSpeech::Noun && utf8::endsWith(edge.surface, "中") &&
      grammar::isAllKanji(edge.surface) && edge.surface.size() >= 6) {  // 2+ kanji (at least N中)
    cost += sc::kPenaltyKanjiChuuCompound;
  }

  // Debug output - show which cost was used and candidate origin (verbose level)
  SUZUME_DEBUG_VERBOSE_BLOCK {
    // Base source type
    const char* source = edge.fromDictionary() ? "dict" : edge.isUnknown() ? "unk" : "infl";
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

float Scorer::connectionCost(const core::LatticeEdge& prev, const core::LatticeEdge& next) const {
  float base_cost = bigramCost(prev.pos, next.pos);

  // ExtendedPOS bigram cost (replaces all check functions)
  float extended_cost = BigramTable::getCost(prev.extended_pos, next.extended_pos);

  // Surface-based bonus for VerbRenyokei → すぎ pattern
  // E.g., 読み+すぎる, 書き+すぎた, 食べ+すぎ (MeCab-compatible split)
  // The default VERB→VERB penalty should not apply to auxiliary verbs
  float surface_bonus = 0.0F;
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.surface.size() >= 6 &&  // "すぎ" is 6 bytes
      next.surface.compare(0, 6, "すぎ") == 0) {
    surface_bonus = cost::kStrongBonus;
  }

  // VerbRenyokei → AdjBasic bonus for kanji-containing verb + adjective
  // E.g., 尽くし+難い, 食べ+やすい — verb renyoukei + compound adjective
  // Only when verb contains kanji to prevent false splits in hiragana sequences
  // (e.g., おこがましい → おこ+がましい would be wrong)
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.extended_pos == core::ExtendedPOS::AdjBasic &&
      grammar::containsKanji(prev.surface)) {
    surface_bonus += cost::kExtraStrongBonus;
  }

  // Penalty for VerbRenyokei + し(conjunction) with kanji verb
  // In modern Japanese, conjunction し follows shuushikei (行く+し), not renyoukei (行き+し).
  // VerbRenyokei + し is usually a false split of godan-sa renyoukei (尽く+し → 尽くし).
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.surface == "し" &&
      next.extended_pos == core::ExtendedPOS::ParticleConj && grammar::containsKanji(prev.surface)) {
    surface_bonus += cost::kMinor;  // Penalty to discourage false split
  }

  // VerbRenyokei (A-row ending) → VerbMizenkei(さ) causative pattern
  // E.g., やら+さ+れ+た — hiragana verb mizenkei + causative さ
  // VerbRenyokei is used for short hiragana verbs (EPOS can't distinguish mizenkei)
  // Need strong bonus to overcome VerbRenyokei→VerbMizenkei bigram penalty (1.8)
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.extended_pos == core::ExtendedPOS::VerbMizenkei &&
      next.surface == "さ" && grammar::endsWithARow(prev.surface)) {
    surface_bonus += sc::kBonusVerbCausativePattern;
  }

  // Compound particle (≥3 chars) → topic/binding particle (は, も, が)
  // E.g., にとって+も, について+は, において+も, として+は
  // Only applies to long compound particles to avoid boosting て+も, し+は
  // Needs to overcome ADV→NOUN bonus advantage in competing paths
  if (prev.extended_pos == core::ExtendedPOS::ParticleConj && next.extended_pos == core::ExtendedPOS::ParticleTopic &&
      prev.surface.size() >= core::kThreeJapaneseCharBytes) {
    surface_bonus += sc::kBonusCompoundParticleToTopic;
  }

  // Bonus for て → い (Auxiliary) pattern
  // E.g., して+い+ます, 食べて+い+た (MeCab-compatible: い is auxiliary, not verb)
  // The auxiliary い (from いる) should win over verb renyokei い
  if (prev.surface == "て" && prev.extended_pos == core::ExtendedPOS::ParticleConj && next.surface == "い" &&
      next.extended_pos == core::ExtendedPOS::AuxAspectIru) {
    surface_bonus += cost::kStrongBonus;
  }

  // Penalty for し (PART_接続) → てる (AuxAspectIru) pattern
  // E.g., 何してる should be 何+し(VERB)+てる, not 何+し(PART)+てる
  // The reasoning conjunction し should not be directly followed by progressive てる
  // This cancels the ParticleConj→AuxAspectIru bonus (-0.8) for this specific case
  if (prev.surface == "し" && prev.extended_pos == core::ExtendedPOS::ParticleConj && next.surface == "てる" &&
      next.extended_pos == core::ExtendedPOS::AuxAspectIru) {
    surface_bonus += cost::kRare;  // Cancel the -0.8 bonus
  }

  // Bonus for て → いただき (humble auxiliary verb) pattern
  // E.g., 食べて+いただき+ます, して+いただけ+ます (MeCab-compatible split)
  // The て→い(AUX)→ただき path incorrectly splits いただき
  // いただく is a humble auxiliary verb that should not be split after て
  if (prev.surface == "て" && prev.extended_pos == core::ExtendedPOS::ParticleConj &&
      next.surface.size() >= 9 &&  // "いただ" is 9 bytes (3 hiragana × 3 bytes)
      next.surface.compare(0, 9, "いただ") == 0 && next.pos == core::PartOfSpeech::Verb) {
    surface_bonus += cost::kVeryStrongBonus;
  }

  // Penalty for splitting unknown words after youon (拗音: ょ, ゃ, ゅ)
  // Youon are always part of the preceding mora (きょ, しゃ, ちゅ)
  // Splitting after them produces invalid word boundaries
  // E.g., くしょん should stay as one token, not くしょ+ん
  // Only apply to non-dictionary tokens (dict entries like でしょ are valid boundaries)
  if (!prev.fromDictionary() && prev.pos == core::PartOfSpeech::Other) {
    std::string_view last_char = utf8::lastChar(prev.surface);
    if (grammar::isSmallKana(last_char)) {
      surface_bonus += cost::kStrong;
    }
  }

  // Penalty for non-pronoun → ら(SUFFIX)
  // Plural suffix ら only naturally follows pronouns (彼ら, 僕ら)
  // Without this, NOUN→SUFFIX bonus (-0.8) causes false splits (かし+ら, 自+ら)
  if (prev.pos != core::PartOfSpeech::Pronoun && next.pos == core::PartOfSpeech::Suffix && next.surface == "ら") {
    surface_bonus += cost::kStrong;
  }

  // Penalty for VerbRenyokei ending in らし → い (AuxAspectIru) pattern
  // 春らしい should be 春 + らしい, not 春らし (verb) + い (auxiliary)
  // The らし ending is typically from らしい (conjecture aux), not a verb renyokei
  // Verbs ending in らし are rare (探らし from 探る is the main exception)
  // Single-kanji noun + らしい is a common pattern that should be protected
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      prev.surface.size() >= 6 &&  // At least 2 chars (kanji + らし)
      utf8::endsWith(prev.surface, "らし") && next.extended_pos == core::ExtendedPOS::AuxAspectIru) {
    surface_bonus += cost::kAlmostNever;
  }

  // Bonus for longer causative forms (させ over さ+せ, させられ over さ+せ+られ)
  // MeCab treats させる as a single causative auxiliary for ichidan verbs
  // E.g., 食べ+させ+られ+た (not 食べ+さ+せ+られ+た)
  // Apply bonus when connecting to AuxCausative with surface starting with させ
  if ((prev.extended_pos == core::ExtendedPOS::VerbRenyokei || prev.extended_pos == core::ExtendedPOS::VerbMizenkei) &&
      next.extended_pos == core::ExtendedPOS::AuxCausative && next.surface.size() >= 6 &&  // "させ" is 6 bytes
      next.surface.compare(0, 6, "させ") == 0) {
    surface_bonus += cost::kVeryStrongBonus;
  }

  // Penalty for Noun → AuxCausative starting with させ (サ変動詞は さ+せ に分割)
  // E.g., 勉強させる should be 勉強+さ+せる, not 勉強+させる
  // MeCab treats サ変動詞 causative as Noun + さ(suru_mizen) + せる(causative)
  // Exception: Single-kanji ichidan verb stems should connect directly to させ
  // E.g., 見させる = 見+させる (ichidan 見る), not 見+さ+せる
  if (prev.pos == core::PartOfSpeech::Noun && next.extended_pos == core::ExtendedPOS::AuxCausative &&
      next.surface.size() >= 6 && next.surface.compare(0, 6, "させ") == 0) {
    // Check if prev is a single-kanji ichidan verb stem (見, 寝, 着, etc.)
    bool is_single_kanji_ichidan = false;
    if (prev.surface.size() == 3) {  // Single kanji (3 bytes in UTF-8)
      auto codepoints = normalize::toCodepoints(prev.surface);
      if (!codepoints.empty()) {
        is_single_kanji_ichidan = verb_helpers::isSingleKanjiIchidan(codepoints[0]);
      }
    }
    if (is_single_kanji_ichidan) {
      // Bonus for single-kanji ichidan verb → させ (見+させる, 寝+させる)
      surface_bonus += cost::kVeryStrongBonus;
    } else {
      // Penalty for multi-kanji noun → させ (サ変動詞は さ+せ に分割)
      surface_bonus += cost::kVeryRare;
    }
  }

  // Bonus for Noun → VerbMizenkei "さ" (サ変動詞の未然形)
  // E.g., 反映される should be 反映+さ+れる (not 反映+される)
  // MeCab treats サ変動詞 passive as Noun + さ(suru_mizen) + れる(passive)
  // This enables the split: 反映+さ+れ+ます
  if (prev.pos == core::PartOfSpeech::Noun && next.extended_pos == core::ExtendedPOS::VerbMizenkei &&
      next.surface == "さ") {
    surface_bonus += cost::kStrongBonus;
  }

  // Bonus for Noun → VerbMeireikei "せよ"/"しろ" (サ変動詞の命令形)
  // E.g., 勉強せよ → 勉強+せよ, 運動しろ → 運動+しろ
  // Without this, default-AUX char_speech candidates for せよ/しろ can beat
  // the legitimate dict VERB imperative entry. Restricted to the suru-imperative
  // surfaces so godan imperative forms (柿+食え) are not falsely boosted.
  if (prev.pos == core::PartOfSpeech::Noun && next.extended_pos == core::ExtendedPOS::VerbMeireikei &&
      (next.surface == "せよ" || next.surface == "しろ")) {
    surface_bonus += cost::kStrongBonus;
  }

  // Surface-based bonus for VerbRenyokei → た (ichidan/irregular た-form)
  // E.g., 食べ+た, 来+た (MeCab-compatible split)
  // Must be surface == "た" to distinguish from て (particle)
  // Guard: require kanji or dict origin to prevent false verbs like まし(ましる)
  // from stealing た bonus over AUX_丁寧 path (参加してきました)
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.surface == "た" &&
      next.extended_pos == core::ExtendedPOS::AuxTenseTa &&
      (grammar::containsKanji(prev.surface) || prev.fromDictionary())) {
    surface_bonus += cost::kVeryStrongBonus;
  }

  // Bonus for VerbRenyokei/VerbOnbinkei → たり/だり (parallel listing particle)
  // E.g., 食べ+たり+する, 飲ん+だり+食べ+たり+する
  // Without this, た(AuxTenseTa) wins over たり(ParticleConj) due to strong た bonus
  // Exclude pure hiragana onbin forms (ぴっ, ばっ) which are onomatopoeia, not verbs
  if ((prev.extended_pos == core::ExtendedPOS::VerbRenyokei || prev.extended_pos == core::ExtendedPOS::VerbOnbinkei) &&
      (next.surface == "たり" || next.surface == "だり") && next.extended_pos == core::ExtendedPOS::ParticleConj &&
      grammar::containsKanji(prev.surface)) {
    surface_bonus += cost::kVeryStrongBonus;
  }

  // Penalty for godan passive/causative-passive renyokei (～Aれ for A-row) → た
  // MeCab splits these as 言わ+れ+た, not 言われ+た
  // E.g., 言われた → 言わ+れ+た, 売られた → 売ら+れ+た, やらされた → やらさ+れ+た
  //       書かれた → 書か+れ+た, 読まれた → 読ま+れ+た, 叩かれた → 叩か+れ+た
  // This cancels the VerbRenyokei→た bonus for godan passive forms
  // Patterns: われ (wa-row), かれ (ka-row), され (sa-row), たれ (ta-row),
  //           なれ (na-row), まれ (ma-row), られ (ra-row), ばれ (ba-row), がれ (ga-row)
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      prev.surface.size() >= 6 &&  // At least 2 chars (kanji+Xれ)
      (utf8::endsWith(prev.surface, "われ") || utf8::endsWith(prev.surface, "かれ") ||
       utf8::endsWith(prev.surface, "され") || utf8::endsWith(prev.surface, "たれ") ||
       utf8::endsWith(prev.surface, "なれ") || utf8::endsWith(prev.surface, "まれ") ||
       utf8::endsWith(prev.surface, "られ") || utf8::endsWith(prev.surface, "ばれ") ||
       utf8::endsWith(prev.surface, "がれ")) &&
      next.surface == "た" && next.extended_pos == core::ExtendedPOS::AuxTenseTa) {
    surface_bonus += cost::kSevere;  // Cancel VerbRenyokei→た bonus
  }

  // Surface-based bonus for でし → た/たら (polite past copula / conditional)
  // 本でした should be 本+でし+た, not 本+で+し+た
  // でしたら should be でし+たら (conditional), not でし+た+ら
  // The competing path is Noun→で(PARTICLE)→し(VERB)→た with VerbRenyokei→た bonus
  if (prev.surface == "でし" && prev.extended_pos == core::ExtendedPOS::AuxCopulaDesu &&
      (next.surface == "た" || next.surface == "たら") && next.extended_pos == core::ExtendedPOS::AuxTenseTa) {
    surface_bonus += cost::kVeryStrongBonus;
  }

  // Penalty for た(AuxTenseTa) → ら(Suffix) pattern
  // This discourages splitting たら into た+ら
  // たら is a conditional form of た and should stay together
  if (prev.surface == "た" && prev.extended_pos == core::ExtendedPOS::AuxTenseTa && next.surface == "ら" &&
      next.extended_pos == core::ExtendedPOS::Suffix) {
    surface_bonus += cost::kSevere;  // Penalty to discourage た+ら split
  }

  // Surface-based bonus for しよ → う (suru verb volitional)
  // MeCab: 勉強しよう → 勉強|しよ|う (しよ=verb volitional base, う=volitional aux)
  // This bonus ensures the split path (しよ|う) beats the merged path (し|よう)
  if (prev.surface == "しよ" && prev.pos == core::PartOfSpeech::Verb && next.surface == "う" &&
      next.extended_pos == core::ExtendedPOS::AuxVolitional) {
    surface_bonus += cost::kVeryStrongBonus;
  }

  // Bonus for dict VERB_連用 → ない/なく/なかっ/なけれ (negative auxiliary)
  // VERB→ADJ bigram (0.8) is high, making split path lose to merged candidates
  // E.g., でき+なく should beat できなく, し+なく should beat しなく
  // Restrict to dictionary verbs (間違い+ない uses 間違い(NOUN), not 違い(VERB))
  // Exclude で (ambiguous: 出る VERB vs だ copula AUX → でない misanalysis)
  // Exclude godan mizenkei (a-dan ending): 走ら, 書か are mislabeled as VERB_連用
  // but are actually 未然形 — bonus would incorrectly boost 走ら+ない split
  if (prev.pos == core::PartOfSpeech::Verb && prev.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      prev.fromDictionary() && prev.surface != "で" && !grammar::endsWithARow(prev.surface) &&
      (next.pos == core::PartOfSpeech::Adjective || next.pos == core::PartOfSpeech::Auxiliary) &&
      (next.surface == "なく" || next.surface == "ない" || next.surface == "なかっ" || next.surface == "なけれ")) {
    surface_bonus += cost::kStrongBonus;
  }

  // Bonus for ば(PART_接続) → なら/なり/なる/なれ(VERB) in -なければならない pattern
  // Prevents spurious ばなら verb candidate (ばなる godan-ra) from winning
  // over correct split ば(conditional) + なら(なる mizenkei)
  if (prev.extended_pos == core::ExtendedPOS::ParticleConj && prev.surface == "ば" &&
      next.pos == core::PartOfSpeech::Verb &&
      (next.surface == "なら" || next.surface == "なり" || next.surface == "なる" || next.surface == "なれ" ||
       next.surface == "なっ")) {
    surface_bonus += cost::kStrongBonus;
  }

  // Bonus for VERB_未然 → AUX_否定古(ず/ずに) connection
  // Godan mizenkei + classical negative: 書かず, 抜かず, 行かず
  // The split path needs help to beat merged verb candidates (書かずに as single VERB)
  // because AUX_否定古 → next token connections have default (high) cost.
  // Note: lexicalized forms like 思わず(ADV) are handled by the candidate generator
  // which skips mizenkei_zu generation when verb+ず is in the dictionary.
  if (prev.extended_pos == core::ExtendedPOS::VerbMizenkei && next.extended_pos == core::ExtendedPOS::AuxNegativeNu &&
      (next.surface == "ず" || next.surface == "ずに" || next.surface == "ざる" || next.surface == "ざれ")) {
    surface_bonus += cost::kStrongBonus;
  }

  // Bonus for AUX_否定古(ずに) → VERB connection
  // ずに+帰る, ずに+済む etc. are natural patterns
  // Without this, split path ず+に+帰る wins due to PART_格→VERB having lower default cost
  if (prev.extended_pos == core::ExtendedPOS::AuxNegativeNu && prev.surface == "ずに" &&
      (next.pos == core::PartOfSpeech::Verb || next.pos == core::PartOfSpeech::Adjective)) {
    surface_bonus += cost::kModerateBonus;  // -0.5 to match PART_格→VERB cost
  }

  // Surface-based penalty for Noun → short VerbRenyokei (compound verb protection)
  // Bigram table gives bonus for Noun→VerbRenyokei (for サ変動詞: 得+し, 損+し)
  // But this should NOT apply to compound verbs like 見+つけ→見つけ
  // E.g., 勘違い should be single token, not 勘違+い
  // E.g., 見つけた should be 見つけ+た, not 見+つけ+た
  // Exception: multi-kanji noun + でき should split (外出+でき+ない)
  // Single kanji NOUN often forms compound verbs with following verb stems
  if (prev.extended_pos == core::ExtendedPOS::Noun && next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.surface != "し" && next.surface != "せ" && next.surface.size() <= 6 &&
      prev.surface.size() == core::kJapaneseCharBytes) {
    surface_bonus += cost::kRare;  // Cancel the bigram bonus
  }

  // Penalty for Noun/ナ形容詞 → い (VerbRenyokei)
  // 漢字名詞やナ形容詞語幹の後に「い」(いる連用形)が来ることは稀
  // E.g., 上手いし should be 上手い+し, not 上手+い+し
  // Exception: This should NOT block patterns like サ変動詞+でき (外出+でき)
  if ((prev.extended_pos == core::ExtendedPOS::AdjNaAdj || prev.extended_pos == core::ExtendedPOS::Noun) &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei && next.surface == "い") {
    surface_bonus += cost::kVeryRare;
  }

  // Partial cancel for single-kanji NOUN + し pattern
  // E.g., 寒し (archaic adjective) should not split as 寒+し
  // But 得+し (suru-verb renyokei) should still split
  // Single kanji = 3 bytes in UTF-8
  if (prev.extended_pos == core::ExtendedPOS::Noun && next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.surface == "し" && prev.surface.size() == 3) {
    surface_bonus += cost::kUncommon;
  }

  // Penalty for single-char case particle → very short pure-hiragana verb pattern
  // E.g., が+おさ is likely mis-segmentation (should be がお+さん)
  // Valid patterns usually have longer verbs (3+ chars) or kanji stems
  // Single-char particles: が, を, に, へ, と, で, から, etc.
  // Only penalize very short verbs (2 chars or less) to avoid affecting なくし, etc.
  // Exception: "い" (いる renyokei) has specific bonus rule below for PART_格→い pattern
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase &&
      prev.surface.size() <= 3 &&  // Single hiragana char (3 bytes in UTF-8)
      next.pos == core::PartOfSpeech::Verb && !next.fromDictionary() && grammar::isPureHiragana(next.surface) &&
      next.surface.size() <= 6 &&  // 2 chars or less (6 bytes in UTF-8)
      next.surface != "い") {      // Exclude い - has specific rule
    surface_bonus += cost::kAlmostNever;
  }

  // Penalty for は (topic) → short pure-hiragana verb pattern
  // E.g., は+し in はしがかかる should be はし (noun), not は+し (topic + する連用形)
  // Only applies to は — other topic particles (も, こそ) naturally precede し
  // (何もしない, 誰もしない are common patterns)
  // Exception: い (renyokei of いる) - valid in ずにはいられない pattern
  // Exception: し (renyokei of する) - valid in emphatic negation ありはしない pattern
  if (prev.extended_pos == core::ExtendedPOS::ParticleTopic && prev.surface == "は" &&
      next.pos == core::PartOfSpeech::Verb && grammar::isPureHiragana(next.surface) &&
      next.surface.size() <= 3 &&  // 1 char only (3 bytes in UTF-8)
      next.surface != "い" &&      // い+られ is valid (いる potential)
      next.surface != "し") {      // し+ない is valid (emphatic negation)
    surface_bonus += cost::kVeryRare;
  }

  // Penalty for pure-hiragana OTHER → single-char VerbRenyokei
  // E.g., ふんど+し should be ふんどし (one word), not noun+する連用形
  // Pure hiragana unknown sequences split before し/き/etc. are usually wrong
  // Does not apply when prev is a known particle/aux (those have specific EPOS)
  if (prev.pos == core::PartOfSpeech::Other && grammar::isPureHiragana(prev.surface) &&
      prev.surface.size() >= 6 &&                                                          // 2+ hiragana chars
      next.extended_pos == core::ExtendedPOS::VerbRenyokei && next.surface.size() <= 3) {  // Single char (し, き, etc.)
    surface_bonus += cost::kUncommon;
  }

  // Penalty for NOUN → single-hiragana OTHER
  // A single hiragana character classified as OTHER after a kanji NOUN is almost
  // always a misparse: the hiragana should be part of a verb (先+生きのこる) or
  // okurigana (読み+残す), not a standalone unknown token
  // E.g., 先生+き(OTHER) should lose to 先+生きのこる
  // Needs a very high penalty to overcome prefix compound bonus advantages
  if (prev.pos == core::PartOfSpeech::Noun && grammar::containsKanji(prev.surface) &&
      next.pos == core::PartOfSpeech::Other && next.surface.size() == 3 &&  // Single char = 3 bytes UTF-8
      grammar::isPureHiragana(next.surface)) {
    surface_bonus += cost::kAlmostNever;
  }

  // Penalty for DET → non-dict single-kanji NOUN
  // The DET→NOUN bigram bonus (-2.5) is too strong for unknown single-kanji tokens,
  // causing splits like こんな+伸+びる instead of こんな+伸びる
  // Valid DET+NOUN patterns (こんな+事, あんな+人) use dict nouns or multi-char nouns
  if (prev.pos == core::PartOfSpeech::Determiner && next.pos == core::PartOfSpeech::Noun && !next.fromDictionary() &&
      grammar::containsKanji(next.surface) && suzume::normalize::utf8Length(next.surface) == 1) {
    surface_bonus += cost::kStrong;
  }

  // Penalty for DET → non-dict kanji+hiragana NOUN (nominalized verb pattern)
  // The DET→NOUN bonus (-2.5) makes heuristic candidates like "先生き" (NOUN)
  // too attractive, preventing correct splits like 先+生きのこる
  // Valid DET+NOUN uses dict nouns or pure-kanji nouns; nominalized forms
  // (kanji + 1 trailing hiragana, e.g., 先生き, 出来事み) are rare after DET
  if (prev.pos == core::PartOfSpeech::Determiner && next.pos == core::PartOfSpeech::Noun && !next.fromDictionary()) {
    size_t char_len = suzume::normalize::utf8Length(next.surface);
    if (char_len >= 3 && grammar::containsKanji(next.surface) && !grammar::isAllKanji(next.surface)) {
      // Check if surface ends with exactly 1 hiragana (nominalized pattern)
      auto codepoints = normalize::toCodepoints(next.surface);
      if (!codepoints.empty() && kana::isHiraganaCodepoint(codepoints.back()) && codepoints.size() >= 2 &&
          normalize::isKanjiCodepoint(codepoints[codepoints.size() - 2])) {
        surface_bonus += cost::kAlmostNever;
      }
    }
  }

  // Penalty for case particle → final particle pattern
  // E.g., を+な in をなくした should not split as を+な+くし+た
  // Final particles (な, ね, よ) don't follow case particles directly
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase && next.extended_pos == core::ExtendedPOS::ParticleFinal) {
    surface_bonus += cost::kAlmostNever;
  }

  // Penalty for PREFIX → short pure-hiragana verb pattern
  // E.g., お+い in において should not happen (お is prefix, い is not a verb here)
  // Valid お+verb patterns: お待ち, お願い (longer, often with kanji)
  // Note: 「い」 is in L1 dictionary as verb renyokei, so don't check fromDictionary
  if (prev.pos == core::PartOfSpeech::Prefix && next.pos == core::PartOfSpeech::Verb &&
      grammar::isPureHiragana(next.surface) && next.surface.size() <= 6) {  // 2 chars or less
    surface_bonus += cost::kAlmostNever;
  }

  // Penalty for PREFIX → non-dictionary pure-hiragana verb pattern (3 chars)
  // E.g., お+はよう in おはよう - はよう is not a real verb
  // Valid patterns like お+待ち have kanji, お+召し would be in dictionary
  if (prev.pos == core::PartOfSpeech::Prefix && next.pos == core::PartOfSpeech::Verb && !next.fromDictionary() &&
      grammar::isPureHiragana(next.surface) && next.surface.size() == 9) {  // Exactly 3 chars (9 bytes)
    surface_bonus += cost::kAlmostNever;
  }

  // Penalty for ADV → short pure-hiragana verb renyokei pattern
  // E.g., はなはだ+し should not happen (はなはだしい is an adjective)
  // Valid ADV+verb patterns: ゆっくり+歩く (verb is longer/has kanji)
  // This prevents split like はなはだ+し+い when はなはだしい exists in dict
  // Exception: dictionary verbs like ね(寝る), み(見る), で(出る) are valid
  bool is_dict_verb_renyokei = core::hasFlag(next.flags, core::EdgeFlags::FromDictionary);
  if (prev.pos == core::PartOfSpeech::Adverb && next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      grammar::isPureHiragana(next.surface) && next.surface.size() <= 3 &&  // 1 char only (し, み, etc.)
      !is_dict_verb_renyokei) {
    surface_bonus += cost::kVeryRare;
  }

  // Penalty for SYMBOL → PARTICLE pattern (furigana in parentheses)
  // E.g., 東京（とうきょう） should not split と+う+きょう
  // Particles don't normally follow opening parentheses directly
  if (prev.pos == core::PartOfSpeech::Symbol && next.pos == core::PartOfSpeech::Particle) {
    surface_bonus += cost::kAlmostNever;
  }

  // Bonus for SYMBOL → long pure-hiragana OTHER (furigana pattern)
  // E.g., 東京（とうきょう） - the hiragana in parentheses is reading/furigana
  // Long hiragana sequences after symbols should stay as single tokens
  if (prev.pos == core::PartOfSpeech::Symbol && next.pos == core::PartOfSpeech::Other &&
      grammar::isPureHiragana(next.surface) && next.surface.size() >= 12) {  // 4+ chars (12 bytes in UTF-8)
    surface_bonus += cost::kVeryStrongBonus;
  }

  // Penalty for SYMBOL → short hiragana → AUX pattern
  // E.g., （+と+う should not happen (furigana shouldn't split into particles/aux)
  if (prev.pos == core::PartOfSpeech::Symbol && next.pos == core::PartOfSpeech::Auxiliary) {
    surface_bonus += cost::kVeryRare;
  }

  // Penalty for AuxCopulaDa(で) + ParticleTopic(も) pattern
  // This prevents 雨+で+も split when 雨+でも (副助詞) is correct
  // But allows 何+で+も split (で=copula連用形, も=係助詞)
  // The difference: 何(Pronoun) vs 雨(Noun) - Pronoun should split
  if (prev.extended_pos == core::ExtendedPOS::AuxCopulaDa && prev.surface == "で" &&
      next.extended_pos == core::ExtendedPOS::ParticleTopic && next.surface == "も") {
    surface_bonus += cost::kVeryRare;
  }

  // Note: Removed penalty for Pronoun + でも patterns
  // MeCab behavior is context-dependent:
  // - "何でもいい" → keeps でも together (副助詞)
  // - "何でもあり" (standalone) → keeps でも together
  // - "何でもありだな" → splits で+も
  // This context-sensitivity can't be captured in bigram scorer.
  // Let other scoring mechanisms handle the distinction.

  // Penalty for SUFFIX(さ) + VERB starting with せ/させ pattern
  // E.g., 勉強 + さ(SUFFIX) + せられてい is wrong; should be 勉強 + さ(VERB_未然) + せ(AUX_使役)
  // This pattern indicates suru-verb causative form where さ is the verb stem, not suffix
  if (prev.pos == core::PartOfSpeech::Suffix && prev.surface == "さ" && next.pos == core::PartOfSpeech::Verb &&
      (utf8::startsWith(next.surface, "せ") || utf8::startsWith(next.surface, "させ"))) {
    surface_bonus += cost::kAlmostNever;
  }

  // Penalty for single-kanji NOUN → single-kanji SUFFIX pattern
  // E.g., 正+式, 手+法, 結+論 are 2-kanji compound words being oversplit
  // because the second kanji is registered as SUFFIX in L1 dictionary.
  // The NOUN→SUFFIX bigram bonus (-0.8) + SUFFIX→PART_格 epos bonus
  // makes the split path cheaper than the compound path.
  // This penalty counteracts that for single-kanji-to-single-kanji transitions,
  // without affecting multi-kanji noun + suffix (e.g., 学生+たち, 科学+的).
  // Exceptions:
  // - 様/氏: handled by +4.0 kanji_seq penalty in unknown.cpp (always split)
  // - 的: removed from kanji_seq penalty; 1-char + 的 stays merged naturally
  //   (目的, 動的, 知的), 2+ char + 的 still splits via bigram bonus (論理+的)
  if (prev.pos == core::PartOfSpeech::Noun && next.pos == core::PartOfSpeech::Suffix &&
      prev.surface.size() == core::kJapaneseCharBytes && next.surface.size() == core::kJapaneseCharBytes &&
      grammar::isAllKanji(prev.surface) && next.surface != "様" && next.surface != "氏") {
    surface_bonus += cost::kRare;  // +1.0 to counteract -0.8 bonus
  }

  // Penalty for 3+ char non-dict kanji NOUN → 1-char SUFFIX pattern
  // For non-dict 3+ char kanji NOUN preceding a single-kanji suffix, the 4-char
  // input is often two 2-char kango compounds (新規 + 手法) rather than
  // a 3+1 stem+suffix split (新規手 + 法). Penalize to let the whole-word
  // (or 2+2 split) compete fairly. Dict-verified 3-char NOUNs (e.g., 政治学+者 if
  // 政治学 were in dict) keep the bonus, since they represent intended compounds.
  if (prev.pos == core::PartOfSpeech::Noun && next.pos == core::PartOfSpeech::Suffix &&
      !prev.fromDictionary() && prev.surface.size() >= 3 * core::kJapaneseCharBytes &&
      next.surface.size() == core::kJapaneseCharBytes && grammar::isAllKanji(prev.surface) &&
      grammar::isAllKanji(next.surface)) {
    surface_bonus += cost::kRare;  // +1.0 to neutralize -0.8 bigram bonus
  }

  // Penalty for pronoun-like NOUN + でも pattern (limited)
  // NOTE: This is a known limitation. MeCab's behavior is context-dependent:
  //   - 何でもあり → でも keeps together
  //   - 何でもありだな → で|も splits
  //   - 彼女でもない → で|も splits
  //   - 彼女でもいい → でも keeps together
  // Viterbi prunes the で|も path before we can evaluate context.
  // The でも→ない/な penalty (below) helps some cases but not all.
  // For now, we only penalize NOUN path; PRON path bypasses this.
  // See session 66/68 notes for detailed analysis.
  if (prev.pos == core::PartOfSpeech::Noun && next.surface == "でも" &&
      (next.extended_pos == core::ExtendedPOS::ParticleAdverbial ||
       next.extended_pos == core::ExtendedPOS::Conjunction) &&
      (prev.surface == "何" || prev.surface == "彼女")) {
    surface_bonus += cost::kAlmostNever;
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
    surface_bonus += cost::kAlmostNever;
  }

  // Penalty for ADV → でも (CONJ or PART_副) pattern
  // After adverbs, でも should split as で(copula)+も(particle)
  // e.g., それほどでもない → それほど+で+も+ない
  if (prev.pos == core::PartOfSpeech::Adverb && next.surface == "でも" &&
      (next.pos == core::PartOfSpeech::Conjunction || next.extended_pos == core::ExtendedPOS::ParticleAdverbial)) {
    surface_bonus += cost::kAlmostNever;
  }

  // Penalty for NOUN/PRONOUN → だけど (CONJ) pattern
  // MeCab splits "彼女だけどいいね" as 彼女+だ+けど+いい+ね
  // だけど as conjunction is valid at sentence start or after particles,
  // but after nouns/pronouns, it should be だ(AUX) + けど(PART)
  if ((prev.pos == core::PartOfSpeech::Noun || prev.pos == core::PartOfSpeech::Pronoun) && next.surface == "だけど" &&
      next.extended_pos == core::ExtendedPOS::Conjunction) {
    surface_bonus += cost::kAlmostNever;
  }

  // Note: Removed penalty for PARTICLE と → VERB_音便 いっ pattern
  // The dictionary entry "といった" (determiner) handles that case
  // For といって pattern, we want と+いっ+て split (MeCab compatible)

  // Penalty for VerbRenyokei → single-char char_speech AUX pattern
  // E.g., 食べろ should be 食べろ (imperative), not 食べ+ろ
  // The ろ is the ichidan imperative ending, not a character speech suffix
  // Character speech suffixes like ろ are valid after だ/です (だろ, でしょ)
  // but not after verb renyokei
  // Valid patterns after VerbRenyokei: た, て, ます, etc. (multi-char or dictionary)
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.pos == core::PartOfSpeech::Auxiliary &&
      !next.fromDictionary() && next.surface.size() <= core::kJapaneseCharBytes) {  // Single char (3 bytes)
    surface_bonus += cost::kUncommon;
  }

  // Penalty for single-char hiragana VerbRenyokei → AuxPassive/AuxCausative
  // Bigram table gives bonus for VerbRenyokei→AuxPassive (for 知らせ+られ)
  // But single-char hiragana like せ+られ should prefer AuxCausative+AuxPassive path
  // Valid patterns like 知らせ+られ have longer surfaces (2+ chars)
  // This prevents せ(VERB連用) from being selected over せ(AuxCausative)
  // Exception: い+られ is valid (いる potential: いられない = cannot stay)
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      (next.extended_pos == core::ExtendedPOS::AuxPassive || next.extended_pos == core::ExtendedPOS::AuxCausative) &&
      prev.surface.size() <= 3 &&                                       // Single hiragana (3 bytes)
      grammar::isPureHiragana(prev.surface) && prev.surface != "い") {  // い+られ is valid (いる potential)
    surface_bonus += cost::kAlmostNever;                                // Strongly discourage
  }

  // Penalty for て/で (ParticleConj) → single-char VerbRenyokei (い)
  // Progressive pattern: 食べて+い+ます should use い(AuxAspectIru), not い(VerbRenyokei)
  // This ensures て+いる patterns use the auxiliary form
  // Exception: たり/だり → し is valid (食べたり+し+てる)
  // Exception: み (みる auxiliary = "try") after て is valid (食べて+み+たい)
  if (prev.extended_pos == core::ExtendedPOS::ParticleConj && next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.surface.size() <= 3 &&  // Single hiragana (3 bytes)
      grammar::isPureHiragana(next.surface) && prev.surface != "たり" && prev.surface != "だり" &&
      next.surface != "み") {
    surface_bonus += cost::kAlmostNever;  // Strongly discourage
  }

  // Penalty for VerbOnbinkei/VerbRenyokei ending in いい → AuxTenseTa pattern
  // E.g., 願いい+た should be 願い+いたし+ます, not 願いい (願いく) + た
  // Valid forms are: 書い, 泳い, etc. (single い after kanji)
  // Invalid: 願いい (連用形い + さらにい) - this suggests wrong verb base
  // Include VerbRenyokei since 願いい is sometimes assigned as renyokei of 願いう
  if ((prev.extended_pos == core::ExtendedPOS::VerbOnbinkei || prev.extended_pos == core::ExtendedPOS::VerbRenyokei) &&
      next.extended_pos == core::ExtendedPOS::AuxTenseTa &&
      prev.surface.size() >= 6 &&  // At least 2 hiragana (6 bytes)
      prev.surface.compare(prev.surface.size() - 6, 6, "いい") == 0) {
    surface_bonus += cost::kAlmostNever;
  }

  // Bonus for VerbRenyokei/VerbOnbinkei → VerbRenyokei (honorific verb patterns)
  // E.g., 願い+いたし (お願いいたします), 報告+いたし (ご報告いたします)
  // Common honorific verb renyokei: いたし, おり, くださ, いただき, もらい, あげ
  // Include VerbOnbinkei since 願い is often recognized as onbin form of 願う
  if ((prev.extended_pos == core::ExtendedPOS::VerbRenyokei || prev.extended_pos == core::ExtendedPOS::VerbOnbinkei) &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      (next.surface == "いたし" || next.surface == "くださ" || next.surface == "いただき" || next.surface == "もらい" ||
       next.surface == "あげ")) {
    surface_bonus += cost::kVeryStrongBonus;
  }

  // Bonus for honorific verb renyokei → AuxTenseMasu (ます)
  // E.g., いただき+ます (いただきます), いたし+ます (いたします)
  // This helps いただき beat い+た+だき pattern
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.extended_pos == core::ExtendedPOS::AuxTenseMasu &&
      (prev.surface == "いただき" || prev.surface == "いたし" || prev.surface == "くださ")) {
    surface_bonus += cost::kVeryStrongBonus;
  }

  // Penalty for single い → AuxTenseTa pattern (いただきます problem)
  // い+た+だき should lose to いただき+ます
  // But て+い+た is valid (食べていた)
  // We penalize い→た only when prev is OTHER (sentence start) or NOUN
  // NOT when prev comes from て-form (VerbTeForm)
  if (prev.surface == "い" && prev.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.extended_pos == core::ExtendedPOS::AuxTenseTa) {
    surface_bonus += cost::kVeryRare;
  }

  // Penalty for AuxTenseTa → い pattern (たい over-split prevention)
  // 行きたい should be 行き+たい, not 行き+た+い
  // た (AuxTenseTa) should not be followed by standalone い
  // This fixes the issue where VerbRenyokei→た bonus (-1.6) beats たい (-0.8)
  if (prev.extended_pos == core::ExtendedPOS::AuxTenseTa && next.surface == "い") {
    surface_bonus += cost::kAlmostNever;
  }

  // Penalty for だ(AuxTenseTa) after non-ん/non-い words
  // だ as past tense follows ん-onbin (読んだ, 飲んだ) or い-onbin (泳いだ, 注いだ)
  // Without this, てる+だ(past) beats てる+だけ(adverbial particle)
  // because AuxAspectIru→AuxTenseTa bonus applies to both た and だ
  if (next.surface == "だ" && next.extended_pos == core::ExtendedPOS::AuxTenseTa &&
      !utf8::endsWith(prev.surface, "ん") && !utf8::endsWith(prev.surface, "い")) {
    surface_bonus += cost::kAlmostNever;
  }

  // Bonus for AuxNegativeNu(ん) → VerbOnbinkei(かっ) pattern
  // くだらん+かっ+た = contracted negative past (くだらなかった)
  // Without this, the adjective path (分からんかっ+た) beats the verb path
  // The かっ verb form (from かる) is specific to this contracted negative past pattern
  // Need very strong bonus because the かっ unknown verb has high cost (~2.7)
  if (prev.extended_pos == core::ExtendedPOS::AuxNegativeNu && next.extended_pos == core::ExtendedPOS::VerbOnbinkei &&
      next.surface == "かっ") {
    surface_bonus += cost::kVeryStrongBonus * 2 + cost::kMinorBonus;  // -3.45
  }

  // Penalty for VerbOnbinkei(ん) → Verb(でる) pattern
  // After ん音便, でる is almost always the contracted ている, not the verb 出る
  // E.g., 並んでる = 並んでいる (progressive), やんでる = 病んでいる
  // Force the で(PART_接続) + る path instead
  if (prev.extended_pos == core::ExtendedPOS::VerbOnbinkei && utf8::endsWith(prev.surface, "ん") &&
      next.pos == core::PartOfSpeech::Verb && next.surface == "でる") {
    surface_bonus += cost::kStrong;
  }

  // Bonus for NOUN → できる(VERB) pattern
  // 堪能できる, 表現できる, 想像できる etc.
  // できる (potential of する) commonly follows サ変 nouns
  // Without this, で(PART)+きる(VERB) path narrowly beats できる(VERB) path
  if (prev.pos == core::PartOfSpeech::Noun && next.pos == core::PartOfSpeech::Verb && next.surface == "できる") {
    surface_bonus += cost::kMinorBonus;
  }

  // Penalty for PARTICLE て → VerbTaForm いた pattern
  // MeCab splits て+い+た, not て+いた
  // いた as verb た-form should not follow て directly
  if (prev.extended_pos == core::ExtendedPOS::ParticleConj && prev.surface == "て" &&
      next.extended_pos == core::ExtendedPOS::VerbTaForm && next.surface == "いた") {
    surface_bonus += cost::kAlmostNever;
  }

  // Penalty for PREFIX ご → VerbRenyokei ざい pattern
  // E.g., ございます should be ござい+ます, not ご+ざい+ます
  // The prefix ご is for nouns (ご報告), not for splitting ござる
  if (prev.extended_pos == core::ExtendedPOS::Prefix && prev.surface == "ご" &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei && next.surface.size() >= 6 &&
      next.surface.compare(0, 6, "ざい") == 0) {
    surface_bonus += cost::kAlmostNever;
  }

  // Surface-based bonus for AdjStem → すぎ pattern
  // E.g., 高+すぎる, 美味し+すぎた (MeCab-compatible split)
  // AdjStem→Verb has prohibitive penalty to prevent な+い splits
  // But AdjStem+すぎ is valid grammar (i-adjective stem + すぎる)
  // Exclude VerbTeForm (すぎて) - should split as すぎ+て
  if (prev.extended_pos == core::ExtendedPOS::AdjStem && next.extended_pos != core::ExtendedPOS::VerbTeForm &&
      next.surface.size() >= 6 &&  // "すぎ" is 6 bytes
      next.surface.compare(0, 6, "すぎ") == 0) {
    // Strong bonus to overcome AdjStem→Verb prohibitive penalty
    surface_bonus += cost::kVeryStrongBonus * 2;
  }

  // Surface-based bonus for AdjNaAdj → すぎ pattern
  // E.g., シンプル+すぎない, 静か+すぎる (na-adjective + sugiru)
  // NOUN→VERB_連用 has bonus from bigram table, which can beat ADJ_NA path
  // This helps dictionary ADJ_NA entries beat unknown NOUN candidates
  if (prev.extended_pos == core::ExtendedPOS::AdjNaAdj && next.surface.size() >= 6 &&
      next.surface.compare(0, 6, "すぎ") == 0) {
    surface_bonus += cost::kStrongBonus;
  }

  // Surface-based bonus for all-kanji NOUN → すぎ pattern
  // E.g., 最高+すぎ, 贅沢+すぎ, 美人+すぎ (kanji compound + sugiru "too much")
  // Without this, multi-kanji nouns get split: 最高→最+高(ADJ_語幹)+すぎ
  // because ADJ_語幹→すぎ has a very strong surface bonus (-3.2)
  // Only apply to all-kanji surfaces (not katakana/verb renyokei)
  if (prev.pos == core::PartOfSpeech::Noun && prev.surface.size() >= 6 &&  // 2+ chars (6+ bytes)
      grammar::isAllKanji(std::string(prev.surface)) && next.surface.size() >= 6 &&
      next.surface.compare(0, 6, "すぎ") == 0) {
    surface_bonus += cost::kVeryStrongBonus * 2;
  }

  // Penalty for AuxCopulaDa(な) → ParticleFinal(ったら) pattern
  // な is attributive form of copula — cannot be followed by ったら particle
  // Valid: だ+ね, だ+よ. Invalid: な+ったら (should be なっ+たら from なる)
  if (prev.extended_pos == core::ExtendedPOS::AuxCopulaDa && prev.surface == "な" &&
      next.extended_pos == core::ExtendedPOS::ParticleFinal && utf8::startsWith(next.surface, "った")) {
    surface_bonus += cost::kRare;
  }

  // Penalty for ParticleFinal → VerbRenyokei pattern
  // E.g., いいよね should be いい+よ+ね(PART), not いい+よ+ね(VERB 寝る)
  // Final particles (よ, な, ね, わ) are rarely followed by verb renyokei
  // The short hiragana verb ね (寝る renyokei) competes with final particle ね
  // This penalty ensures particle interpretation wins in よね, なね, etc. patterns
  if (prev.extended_pos == core::ExtendedPOS::ParticleFinal && next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      grammar::isPureHiragana(next.surface) && next.surface.size() <= 3) {  // Single hiragana (3 bytes)
    surface_bonus += cost::kRare;
  }

  // Note: "かも" is kept as single token per SuzumeUtils.pm normalization
  // (か+も → かも merge rule). No penalty for AUX → かも.

  // Penalty for ParticleFinal(か) → ADV(もし) in かもしれない pattern
  // "もし" is a valid adverb, but not in "かもしれない" context
  // This prevents か+もし+れ split, favoring か+も+しれ
  if (prev.extended_pos == core::ExtendedPOS::ParticleFinal && prev.surface == "か" &&
      next.pos == core::PartOfSpeech::Adverb && next.surface == "もし") {
    surface_bonus += cost::kVeryRare;
  }

  // Penalty for short hiragana verb mizenkei + ん pattern
  // E.g., が+おさ+ん should be がお+さん (name + honorific suffix)
  // Short hiragana verbs followed by ん are often mis-segmented names
  // Valid patterns like 押さ+ん (kanji verb) have non-hiragana stems
  // ん can be AUX_否定古 or PART_準体, both should be penalized
  if (prev.extended_pos == core::ExtendedPOS::VerbMizenkei && grammar::isPureHiragana(prev.surface) &&
      prev.surface.size() <= 6 &&  // 2 chars or less (6 bytes in UTF-8)
      next.surface == "ん") {
    surface_bonus += cost::kAlmostNever;
  }

  // Penalty for short pure-hiragana dict verb + ず (classical negative)
  // E.g., おか+ず should be おかず (noun), not おか (dict verb おく) + ず (aux)
  // Short pure-hiragana verbs + ず are likely false parses of nouns/adverbs
  // Long verbs (かかわら+ず) and kanji verbs (表さ+ず) are productive grammar
  // Lexicalized forms like 思わず have their own dict entries (ADV) that win anyway
  // Note: ん, ぬ, まい, ざる, ざれ excluded — common productive patterns
  if (prev.pos == core::PartOfSpeech::Verb && prev.fromDictionary() && grammar::isPureHiragana(prev.surface) &&
      prev.surface.size() <= 9 &&  // ≤3 hiragana chars (9 bytes)
      next.extended_pos == core::ExtendedPOS::AuxNegativeNu && next.surface != "ん" && next.surface != "ぬ" &&
      next.surface != "まい" && next.surface != "ざる" && next.surface != "ざれ") {
    surface_bonus += cost::kAlmostNever;
  }

  // Penalty for single-kanji noun + hiragana verb renyokei/onbinkei
  // E.g., 勘+違い should be 勘違い (compound noun), not 勘 (noun) + 違い (dict verb)
  // Single-kanji nouns rarely form valid noun+verb compounds with hiragana verbs
  // Exception: し (suru renyokei) is valid for サ変 pattern (得+し, 得する)
  // Exception: Katakana verbs (バズっ, ググっ) are valid after nouns (超バズった)
  // Exception: Kanji-initial verbs (本+買っ) are valid noun+verb (dropped を)
  if (prev.pos == core::PartOfSpeech::Noun && prev.surface.size() == 3 &&  // Single kanji (3 bytes UTF-8)
      next.pos == core::PartOfSpeech::Verb &&
      (next.extended_pos == core::ExtendedPOS::VerbRenyokei || next.extended_pos == core::ExtendedPOS::VerbOnbinkei) &&
      next.surface != "し" &&  // Exclude suru renyokei (サ変動詞パターン)
      !kana::isKatakanaCodepoint(utf8::decodeFirstChar(next.surface)) &&            // Exclude katakana verbs
      !suzume::normalize::isKanjiCodepoint(utf8::decodeFirstChar(next.surface))) {  // Exclude kanji verbs
    surface_bonus += cost::kVeryRare;
  }

  // Penalty for single-kanji NOUN → verbal auxiliary patterns
  // E.g., 合+う should be 合う (verb), not 合 (noun) + う (volitional)
  // E.g., 揺+れる should be 揺れる (verb), not 揺 (noun) + れる (passive)
  // Single-kanji nouns rarely take verbal auxiliaries directly
  // Exception: Single-kanji ichidan verb stems + causative させ (見+させる, etc.)
  if (prev.extended_pos == core::ExtendedPOS::Noun && prev.surface.size() == 3 &&  // Single kanji (3 bytes in UTF-8)
      (next.extended_pos == core::ExtendedPOS::AuxVolitional || next.extended_pos == core::ExtendedPOS::AuxPassive ||
       next.extended_pos == core::ExtendedPOS::AuxPotential || next.extended_pos == core::ExtendedPOS::AuxCausative)) {
    // Check if this is single-kanji ichidan + causative させ (should be allowed)
    bool is_ichidan_causative = false;
    if (next.extended_pos == core::ExtendedPOS::AuxCausative && next.surface.size() >= 6 &&
        next.surface.compare(0, 6, "させ") == 0) {
      auto codepoints = normalize::toCodepoints(prev.surface);
      if (!codepoints.empty() && verb_helpers::isSingleKanjiIchidan(codepoints[0])) {
        is_ichidan_causative = true;
      }
    }
    if (!is_ichidan_causative) {
      surface_bonus += cost::kVeryRare;
    }
  }

  // Penalty for PARTICLE → もあり/もありだ verb candidates
  // "もあり" is mis-recognized as godan verb (もある) or suru verb (もありする)
  // In "何でもありだな", "もありだ" should be も+あり+だ, not もありする+た
  // This pattern only appears after particles (で in でもあり)
  if (prev.pos == core::PartOfSpeech::Particle && next.pos == core::PartOfSpeech::Verb &&
      (next.surface == "もあり" || next.surface == "もありだ" || next.surface == "もある" ||
       next.surface == "もあっ")) {
    surface_bonus += cost::kAlmostNever;
  }

  // Bonus for PART_格 → い (VerbRenyokei of いる)
  // E.g., 家にいた → 家+に+い+た (not 家+にいた)
  // "にいた" is mis-recognized as godan verb (にく) past tense
  // い is the renyokei of いる (to exist/be), very common after particles
  // Exclude と→い because という is a common determiner that should stay as one token
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase && next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.surface == "い" && prev.surface != "と") {  // Exclude と to protect という determiner
    surface_bonus += cost::kStrongBonus;
  }

  // Penalty for と → いう pattern to protect という determiner
  // E.g., という名前 → という+名前 (not と+いう+名前)
  // という is a common quotative determiner that should stay as one token
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase && prev.surface == "と" &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei && (next.surface == "いう" || next.surface == "いっ")) {
    surface_bonus += cost::kUncommon;
  }

  // Bonus for VerbRenyokei → し (サ変 する renyokei)
  // E.g., お願いします → お+願い+し+ます (not お+願いし+ます)
  // "願いし" is mis-recognized as godan-sa verb (願いす)
  // This pattern is common for サ変複合動詞: 願い+する, 案内+する
  // Exclude single-char "い" which is いる renyokei (interferes with 上手い+し)
  // Exclude "で" which is でる renyokei (interferes with んでした → ん+でし+た)
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.surface == "し" && prev.fromDictionary() && prev.surface != "い" && prev.surface != "で") {
    surface_bonus += cost::kVeryStrongBonus;
  }

  // Penalty for kanji+sokuon+kanji NOUN → し(VerbRenyokei) pattern
  // E.g., 引っ越+し should be 引っ越し (compound verb), not NOUN + suru renyokei
  // These patterns are usually compound verbs registered in dictionary
  // The pattern: 漢字+っ+漢字 (kanji + sokuon + kanji) as NOUN → し(する連用形)
  if (prev.pos == core::PartOfSpeech::Noun && !prev.fromDictionary() &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei && next.surface == "し" &&
      prev.surface.size() >= 9 &&  // At least 3 chars (2 kanji + っ)
      utf8::contains(prev.surface, "っ")) {
    // Check if it's kanji+っ+kanji pattern
    bool has_sokuon_between_kanji = false;
    auto codepoints = normalize::toCodepoints(prev.surface);
    for (size_t i = 1; i + 1 < codepoints.size(); ++i) {
      if (codepoints[i] == U'っ' && suzume::normalize::isKanjiCodepoint(codepoints[i - 1]) &&
          suzume::normalize::isKanjiCodepoint(codepoints[i + 1])) {
        has_sokuon_between_kanji = true;
        break;
      }
    }
    if (has_sokuon_between_kanji) {
      surface_bonus += cost::kVeryRare;
    }
  }

  // Penalty for pure-hiragana dict NOUN → し(VerbRenyokei) pattern
  // E.g., はな+し should be はなし (verb), not はな(NOUN) + し(する連用形)
  // はな is a dict NOUN but not a suru-noun, so はな+し is not a valid suru compound
  // This does not affect kanji nouns (勉強+し is valid suru compound)
  // Use small penalty (0.08) to tip balance: はなし gap=0.013, なんし gap=0.102
  if (prev.pos == core::PartOfSpeech::Noun && prev.fromDictionary() &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei && next.surface == "し" &&
      grammar::isPureHiragana(prev.surface)) {
    surface_bonus += sc::kPenaltyHiraganaNounToSuruTip;
  }

  // Bonus for multi-kanji NOUN → せ(VerbMizenkei) sahen pattern
  // 認識+せ, 期待+せ: favors split over merged 認識せ/期待せ verb candidate
  // Only for 2+ kanji nouns (sahen-compatible), not single-kanji like 下+さ
  if (prev.pos == core::PartOfSpeech::Noun && next.extended_pos == core::ExtendedPOS::VerbMizenkei &&
      next.surface == "せ" && prev.surface.size() >= 6) {  // 2+ kanji = 6+ bytes in UTF-8
    surface_bonus += cost::kStrongBonus;
  }

  // Penalty for PART_準体(の) → で to prevent ので splitting
  // High penalty keeps ので merged in conjunctive use (寒いので出かけた)
  // For のではない pattern, the downstream で→は surface bonus overcomes this
  if (prev.extended_pos == core::ExtendedPOS::ParticleNo && prev.surface == "の" && next.surface == "で") {
    surface_bonus += cost::kVeryRare;  // 1.8
  }

  // Penalty for AuxCopulaDa(な) → し(PART_接続) pattern
  // な is the adnominal form of copula だ, normally followed by a noun (きれいな人)
  // な+し as copula+conjunction is invalid - this prevents はなし → は+な+し
  // The bigram AuxCopulaDa→ParticleConj bonus (-0.8) is for だ+し, not な+し
  // BUT: な+のに and な+ので are valid (嫌なのに, 嫌なので) - only penalize し
  if (prev.extended_pos == core::ExtendedPOS::AuxCopulaDa && prev.surface == "な" &&
      next.extended_pos == core::ExtendedPOS::ParticleConj && next.surface == "し") {
    surface_bonus += cost::kVeryRare;  // Cancel the -0.8 bonus and add penalty
  }

  // Penalty for AuxCopulaDa(で) → し pattern (VerbRenyokei or ParticleConj)
  // 本でした should be 本+でし+た, not 本+で+し+た
  // で as copula te-form followed by し is grammatically unusual
  // し can be recognized as VerbRenyokei (suru) or PARTICLE_接続 (parallel particle)
  // Neither is correct in this context - the でし is the renyokei of です copula
  // This ensures でし (AuxCopulaDesu renyokei) wins over で+し split
  if (prev.extended_pos == core::ExtendedPOS::AuxCopulaDa && prev.surface == "で" && next.surface == "し" &&
      (next.extended_pos == core::ExtendedPOS::VerbRenyokei || next.extended_pos == core::ExtendedPOS::ParticleConj)) {
    surface_bonus += cost::kAlmostNever;
  }

  // Penalty for PART_接続(し) → て pattern (PART_接続 or AUX_継続)
  // E.g., をなくして should be を+なくし+て, not を+なく+し+て
  // "し" as conjunctive particle (reason-listing) rarely followed by "て" directly
  // This prevents adjective renyokei (なく) + し (particle) + て from winning
  // over verb renyokei (なくし) + て pattern
  // Note: "て" can be either ParticleConj or AuxAspectIru (ている/てる aspect)
  if (prev.extended_pos == core::ExtendedPOS::ParticleConj && prev.surface == "し" && next.surface == "て" &&
      (next.extended_pos == core::ExtendedPOS::ParticleConj || next.extended_pos == core::ExtendedPOS::AuxAspectIru)) {
    surface_bonus += cost::kStrong;
  }

  // Penalty for ADJ_連用(なく) → VERB_連用(し) pattern
  // E.g., なくした should be なくし+た, not なく+し+た
  // "なくす" (to lose) is a distinct verb from "なく+する" (to make not exist)
  // The AdjRenyokei→VerbRenyokei bonus (-0.8) for 美しく+なり pattern
  // incorrectly applies to なく+し, causing over-split of なくす verb
  // This penalty cancels the bonus specifically for ない形容詞 + する pattern
  if (prev.extended_pos == core::ExtendedPOS::AdjRenyokei && prev.surface == "なく" &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei && next.surface == "し") {
    surface_bonus += cost::kRare;  // Cancel the -0.8 bonus
  }

  // Penalty for short hiragana VERB_連用 → し/き (single-char verb renyokei)
  // E.g., おかしを → おかし+を, not おか+し+を
  // Short verb renyokei (2-3 chars) followed by し or き often indicates
  // over-segmentation of a noun or longer verb
  // Exclude ば (valid conditional: よれ+ば), て (te-form), etc.
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && prev.surface.size() >= 6 &&
      prev.surface.size() <= 9 &&  // 2-3 hiragana
      (next.surface == "し" || next.surface == "き")) {
    // Check prev is all hiragana
    bool prev_is_hira = true;
    for (char32_t cp : normalize::utf8::decode(prev.surface)) {
      if (!kana::isHiraganaCodepoint(cp)) {
        prev_is_hira = false;
        break;
      }
    }
    if (prev_is_hira && (next.extended_pos == core::ExtendedPOS::VerbRenyokei ||
                         next.extended_pos == core::ExtendedPOS::ParticleConj)) {
      surface_bonus += cost::kStrong;
    }
  }

  // Penalty for negation PREFIX (非/不/無/未) → single-kanji NOUN
  // E.g., 非常 → 非|常 is wrong (非常 is a single word, not prefix+noun)
  // E.g., 不可能 → 不|可能 is wrong (不可能 is a single word)
  // But お|茶, ご|報告 are valid (honorific prefix + noun)
  // Only penalize negation prefixes followed by single kanji
  if (prev.extended_pos == core::ExtendedPOS::Prefix &&
      (prev.surface == "非" || prev.surface == "不" || prev.surface == "無" || prev.surface == "未") &&
      next.extended_pos == core::ExtendedPOS::Noun && next.surface.size() == 3) {  // Single kanji (3 bytes in UTF-8)
    surface_bonus += cost::kAlmostNever;
  }

  // Penalty for AuxCopulaDa(で) → Symbol/EOS pattern
  // E.g., あとで。 should be NOUN+PART_格+。, not NOUN+AUX_断定+。
  // Copula 「で」 at sentence end is unusual; 格助詞「で」 is more natural
  // Note: 「だ」+Symbol is valid (学生だ。), so only penalize 「で」
  if (prev.extended_pos == core::ExtendedPOS::AuxCopulaDa && prev.surface == "で" &&
      next.extended_pos == core::ExtendedPOS::Symbol) {
    surface_bonus += cost::kStrong;
  }

  // Penalty for NOUN → で(AUX_断定): copula で after noun is uncommon
  // Most NOUN+copula uses だ directly; で is mainly in で+ある/で+は/で+も
  // This counteracts the Noun→AuxCopulaDa bigram bonus (-0.5) for で
  // E.g., あとで, 爆速で, きっかけで, 電車で → NOUN+PART_格 preferred
  // Note: NOUN→だ(AUX_断定) is NOT affected (学生だ is correct)
  if ((prev.pos == core::PartOfSpeech::Noun || prev.pos == core::PartOfSpeech::Pronoun) &&
      next.extended_pos == core::ExtendedPOS::AuxCopulaDa && next.surface == "で") {
    surface_bonus += cost::kRare;  // 1.0: must exceed kModerateBonus (-0.5)
  }

  // Bonus for で(AuxCopulaDa) → ある/あっ/あろ (formal copula pattern)
  // のである, ではある, であった are standard literary/formal expressions
  // AuxCopulaDa→VerbShuushikei has kMinor (0.5) bigram + kVeryRare (1.8) の→で surface
  // Total penalty to overcome: ~2.3
  if (prev.extended_pos == core::ExtendedPOS::AuxCopulaDa && prev.surface == "で" &&
      next.pos == core::PartOfSpeech::Verb &&
      (next.surface == "ある" || next.surface == "あっ" || next.surface == "あろ" || next.surface == "あり")) {
    surface_bonus += cost::kVeryStrongBonus * 2;  // -3.2 to overcome ~2.3 total penalty
  }

  // Bonus for で(AuxCopulaDa) → は(ParticleTopic) surface connection
  // Helps のではない/のではなく pattern split の+で+は despite の→で penalty (1.8)
  // Only fires when で is split from の (not when ので stays as single token)
  // Safe: で(AuxCopulaDa)+は is always correct when it occurs (ではない, ではなく)
  if (prev.extended_pos == core::ExtendedPOS::AuxCopulaDa && prev.surface == "で" &&
      next.extended_pos == core::ExtendedPOS::ParticleTopic && next.surface == "は") {
    surface_bonus += cost::kVeryStrongBonus;  // -1.6
  }

  // Penalty for で(VERB_連用 of 出る) → ない(AUX_否定): copula でない is more common
  // でない = "is not" (copula) vs "doesn't come out" (verb 出る)
  // Without context (を/から before で), copula interpretation should win
  // E.g., 正式でない, 必要でない → で(AUX_断定) + ない(AUX_否定)
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && prev.surface == "で" &&
      next.extended_pos == core::ExtendedPOS::AuxNegativeNai) {
    surface_bonus += cost::kStrong;
  }

  // Bonus for ParticleTopic → なかろ (AuxNegativeNai volitional stem)
  // Helps は+なかろ+う split beat fake godan-ra verb candidate なかろう
  // Only targets なかろ — other forms (ない, なかっ, なけれ) can be ADJ or AUX
  if (prev.extended_pos == core::ExtendedPOS::ParticleTopic && next.extended_pos == core::ExtendedPOS::AuxNegativeNai &&
      next.surface == "なかろ") {
    surface_bonus += cost::kModerateBonus;
  }

  // Penalty for NOUN/PRON → で(VERB_連用 of 出る): verb で after noun is rare
  // Most NOUN+で patterns use particle or copula, not verb 出る
  // E.g., あとで, 爆速で → NOUN+PART_格, not NOUN+VERB(出る)
  if ((prev.pos == core::PartOfSpeech::Noun || prev.pos == core::PartOfSpeech::Pronoun) &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei && next.surface == "で") {
    surface_bonus += cost::kRare;  // 1.0: must exceed NOUN→VERB_連用 bonus
  }

  // Penalty for VerbRenyokei → で (any interpretation)
  // Ichidan te-form only uses て (食べ+て, 見+て), NOT で
  // Godan te-form with で uses onbinkei (飲ん+で, 読ん+で), not renyokei
  // AUX_断定(で) only attaches to nouns/na-adj (静かで, 学生で), not verbs
  // Without this, kanji+り nouns like 夏祭り get falsely parsed as VERB_連用
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.surface.size() >= 3 &&
      next.surface.compare(0, 3, "で") == 0) {
    if (next.extended_pos == core::ExtendedPOS::ParticleConj) {
      surface_bonus += cost::kMinor;  // +0.5 to cancel the -0.5 bigram bonus
    } else if (next.extended_pos == core::ExtendedPOS::AuxCopulaDa) {
      surface_bonus += cost::kMinor;  // Penalize invalid VERB_連用→断定
    }
  }

  // Penalty for single-kanji ADJ_語幹 → AuxGaru
  // E.g., 挙+がっ should be 挙がっ (verb onbin), not 挙(adj stem)+がっ(がる suffix)
  // Multi-char adj stems + がる are valid (可愛+がる, 怖+がる via dict)
  // But single-kanji adj stems are usually false positives from the candidate generator
  if (prev.extended_pos == core::ExtendedPOS::AdjStem && suzume::normalize::utf8Length(prev.surface) == 1 &&
      next.extended_pos == core::ExtendedPOS::AuxGaru) {
    surface_bonus += cost::kStrong;
  }

  // Penalty for single-hiragana VERB_連用 → を(PART_格)
  // E.g., おかしを → おかし+を, not おか+し+を
  // Single hiragana verb renyokei (し, き, み, etc.) rarely takes を directly
  // Nominalized verb renyokei like 読み, 書き take を (読みを深める) but those
  // are multi-char and should be recognized as NOUN, not single-char VERB_連用
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      prev.surface.size() == 3 &&  // Single char (3 bytes = hiragana/katakana)
      next.extended_pos == core::ExtendedPOS::ParticleCase && next.surface == "を") {
    // Check if single char is hiragana
    auto decoded = normalize::utf8::decode(prev.surface);
    auto it = decoded.begin();
    if (it != decoded.end() && kana::isHiraganaCodepoint(*it)) {
      surface_bonus += cost::kStrong;
    }
  }

  // Penalty for で(VerbRenyokei of 出る) → Particle (except て)
  // で as 出る renyokei should only be followed by auxiliaries (たい, ます) or て
  // 彼女でも → 彼女+で(PART)+も, not 彼女+で(VERB 出る)+も
  // But でて → で+て is valid (出る renyokei + te-form)
  // The verb interpretation is only valid before auxiliaries like たい/ます or て
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && prev.surface == "で" &&
      next.pos == core::PartOfSpeech::Particle && next.surface != "て") {  // Exclude て to allow でて → で+て split
    surface_bonus += cost::kStrong;
  }

  // Bonus for dictionary NOUN → dictionary NOUN connection
  // E.g., 明日+雨, 毎日+電車 should beat 明日雨, 毎日電車 (kanji_seq)
  // When both nouns are in dictionary, the split path is more accurate
  // This helps time nouns (明日, 今日, 毎日) + common nouns (雨, 電車)
  if (prev.pos == core::PartOfSpeech::Noun && prev.fromDictionary() && next.pos == core::PartOfSpeech::Noun &&
      next.fromDictionary() && !prev.isFormalNoun() && !next.isFormalNoun()) {
    surface_bonus += cost::kModerateBonus;  // Prefer dict+dict split over kanji_seq
  }

  // Penalty for identical hiragana NOUN → NOUN sequence
  // E.g., もも|もも is less likely than もも|も|もも (particle between)
  // This prevents すもももも... from being split as もも|もも|もの
  if (prev.pos == core::PartOfSpeech::Noun && next.pos == core::PartOfSpeech::Noun && prev.surface == next.surface &&
      grammar::isPureHiragana(prev.surface)) {
    surface_bonus += cost::kVeryRare;
  }

  // Bonus for dictionary hiragana NOUN → single-char particle も/の pattern
  // E.g., すもも|も|もも should beat すもも|もも (particle interpretation)
  // E.g., もも|の|うち should beat もの|うち (particle interpretation)
  // This helps famous test sentence: すもももももももものうち
  if (prev.pos == core::PartOfSpeech::Noun && prev.fromDictionary() && grammar::isPureHiragana(prev.surface) &&
      next.pos == core::PartOfSpeech::Particle && (next.surface == "も" || next.surface == "の")) {
    surface_bonus += cost::kModerateBonus;
  }

  // Penalty for NOUN/PRON → pure-hiragana VERB_た形 (non-dict) pattern
  // E.g., 家にいた should be 家+に+い+た, not 家+にいた
  // E.g., ここにいた should be ここ+に+い+た, not ここ+にいた
  // "にいた" is mis-recognized as godan verb (にく) past tense
  // Pure-hiragana た-form verbs after NOUN/PRON are typically particle+aux sequences
  // Valid kanji+hiragana た-forms like 食べた are not affected (not pure hiragana)
  if ((prev.pos == core::PartOfSpeech::Noun || prev.pos == core::PartOfSpeech::Pronoun) &&
      next.extended_pos == core::ExtendedPOS::VerbTaForm && !next.fromDictionary() &&
      grammar::isPureHiragana(next.surface) && next.surface.size() <= 12) {  // 4 chars or less (12 bytes in UTF-8)
    surface_bonus += cost::kVeryRare;
  }

  // Bonus for か (particle) → dictionary verb mizenkei
  // In quotative patterns like かどうか分からない, か is followed by verb directly
  // Override the PART_終→VERB_未然 penalty for dictionary verbs
  if (prev.surface == "か" && prev.extended_pos == core::ExtendedPOS::ParticleFinal &&
      next.extended_pos == core::ExtendedPOS::VerbMizenkei && next.fromDictionary()) {
    surface_bonus += cost::kStrongBonus;  // -0.8 to reduce the 1.8 penalty
  }

  // Penalty for single-kanji NOUN → pure-hiragana VERB_未然 (non-dict)
  // E.g., 分+から should be 分から (single verb), not 分(NOUN) + から(VERB かる)
  // When a dictionary entry exists for combined form, penalize the split
  if (prev.pos == core::PartOfSpeech::Noun && prev.surface.size() == core::kJapaneseCharBytes &&  // Single kanji
      grammar::isAllKanji(prev.surface) && next.extended_pos == core::ExtendedPOS::VerbMizenkei &&
      !next.fromDictionary() && grammar::isPureHiragana(next.surface)) {
    surface_bonus += cost::kVeryRare;  // Penalize split to favor combined dict verb
  }

  // Penalty for demonstrative-based CONJ → kanji VERB pattern
  // E.g., それで帰った should be それ+で+帰っ+た, not それで(CONJ)+帰った
  // Demonstrative-origin conjunctions (それで, そこで, ここで) can be split
  // when followed directly by kanji verb (no comma/pause)
  // "それで" as pure conjunction prefers comma/pause before verb
  // But "それでございます" (それで+ござっ) is valid - ござっ is honorific
  // Apply to both VerbOnbinkei (帰っ) and VerbTaForm (帰った)
  if (prev.pos == core::PartOfSpeech::Conjunction &&
      (prev.surface == "それで" || prev.surface == "そこで" || prev.surface == "ここで") &&
      (next.extended_pos == core::ExtendedPOS::VerbOnbinkei || next.extended_pos == core::ExtendedPOS::VerbTaForm) &&
      next.surface.size() >= 3 &&  // At least 1 kanji (3 bytes)
      grammar::isAllKanji(std::string(next.surface.substr(0, 3))) &&
      !utf8::startsWith(next.surface, "ござ")) {  // Exclude honorific ござる
    surface_bonus += cost::kAlmostNever;
  }

  // Bonus for proper name sequence: Family → Given (姓→名)
  // E.g., 優木(FAMILY) + せつ菜(GIVEN) should strongly prefer staying together
  if (prev.extended_pos == core::ExtendedPOS::NounProperFamily &&
      next.extended_pos == core::ExtendedPOS::NounProperGiven) {
    surface_bonus += cost::kStrongBonus;  // -2.5 bonus
  }

  // Penalty for single-kana verb renyokei after adverb
  // Single-kana renyokei (で=出る, し=する) are ambiguous with copula/particles.
  // After adverbs, copula/particle interpretation dominates (それほどで+も+ない)
  // Exception: dict verbs (し=する) are valid after onomatopoeia ADV (じめじめ+し+た)
  // Exception: kanji verbs (見, 寝, 出) are unambiguous and valid after adverbs (初めて+見+た)
  if (prev.pos == core::PartOfSpeech::Adverb && next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.surface.size() <= 3 &&               // Single kana (3 bytes)
      grammar::isPureHiragana(next.surface) &&  // Only hiragana (で, し), not kanji (見, 出)
      !core::hasFlag(next.flags, core::EdgeFlags::FromDictionary)) {
    surface_bonus += cost::kVeryRare;
  }

  // Penalty for non-て/で particle/verb before い/いる auxiliary (AuxAspectIru)
  // AuxAspectIru (い/いる) requires て-form as prerequisite: V連用+て+いる
  // VERB_連用+い directly (し+い) or PART_接続(し)+い are grammatically invalid
  // Note: て/で themselves also have AuxAspectIru EPOS, so exclude them as next
  // Fixes: 一番美+し+い → 一番+美しい (wrongly split adjective 美しい)
  if (next.extended_pos == core::ExtendedPOS::AuxAspectIru && next.surface != "て" && next.surface != "で") {
    if (prev.extended_pos == core::ExtendedPOS::ParticleConj && prev.surface != "て" && prev.surface != "で") {
      surface_bonus += cost::kAlmostNever;
    } else if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && prev.surface != "て" && prev.surface != "で") {
      surface_bonus += cost::kAlmostNever;
    }
  }

  // Bonus for Noun → dict i-adjective (AdjBasic)
  // Dict adjectives are verified words — favor NOUN+ADJ split over false verb paths
  // e.g., 一番+美しい(dict ADJ) should beat 一番+美しい(false VERB)
  // Only for dict adjectives to avoid boosting false adj candidates (e.g., 払い)
  if (prev.pos == core::PartOfSpeech::Noun && next.extended_pos == core::ExtendedPOS::AdjBasic &&
      next.fromDictionary()) {
    surface_bonus += cost::kModerateBonus;
  }

  // Penalty for conjunction after single-char token
  // Conjunctions start clauses and don't follow bare single characters
  // in running text without punctuation. This prevents verb stems
  // (醒, 覚, 冷) from splitting before conjunctions (まして, etc.)
  if (next.pos == core::PartOfSpeech::Conjunction && prev.surface.size() <= 3) {  // Single CJK/kana character (3 bytes)
    surface_bonus += cost::kAlmostNever;
  }

  float total = base_cost + extended_cost + surface_bonus;

  SUZUME_DEBUG_VERBOSE_BLOCK {
    SUZUME_DEBUG_STREAM << "[CONN] \"" << prev.surface << "\" (" << core::posToString(prev.pos) << "/"
                        << core::extendedPosToString(prev.extended_pos) << ") → \"" << next.surface << "\" ("
                        << core::posToString(next.pos) << "/" << core::extendedPosToString(next.extended_pos) << "): "
                        << "bigram=" << base_cost << " epos_adj=" << extended_cost;
    if (surface_bonus != 0.0F) {
      SUZUME_DEBUG_STREAM << " surface_bonus=" << surface_bonus;
    }
    if (extended_cost != 0.0F) {
      // Show rule name: PrevEPOS→NextEPOS
      SUZUME_DEBUG_STREAM << " (rule=" << core::extendedPosToString(prev.extended_pos) << "→"
                          << core::extendedPosToString(next.extended_pos) << ")";
    } else if (surface_bonus == 0.0F) {
      SUZUME_DEBUG_STREAM << " (default)";
    }
    SUZUME_DEBUG_STREAM << " total=" << total << "\n";
  }

  return total;
}

}  // namespace suzume::analysis
