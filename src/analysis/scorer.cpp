#include "analysis/scorer.h"

#include "analysis/scorer_constants.h"
#include "core/debug.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"

namespace suzume::analysis {

const std::unordered_set<std::string_view> kSingleKanjiExceptions = {
    "人", "日", "月", "年", "時", "分", "秒", "本", "冊", "個",
    "枚", "台", "回", "件", "円", "点", "度", "番", "階", "歳",
    "国", "市", "県", "区", "町", "村", "上", "下", "中", "外",
    "内", "前", "後", "東", "西", "南", "北", "春", "夏", "秋",
    "冬", "朝", "昼", "夜",
};

// Single hiragana functional words that should not be penalized
// These are common particles, auxiliaries, and other grammatical elements
const std::unordered_set<std::string_view> kSingleHiraganaExceptions = {
    // Case particles (格助詞)
    "が", "を", "に", "で", "と", "へ", "の",
    // Binding particles (係助詞)
    "は", "も",
    // Final particles (終助詞)
    "か", "な", "ね", "よ", "わ",
    // Auxiliary (助動詞)
    "だ", "た",
    // Conjunctive particles (接続助詞)
    "て", "ば",
};

namespace {

// Convert POS to array index
constexpr size_t posToIndex(core::PartOfSpeech pos) {
  switch (pos) {
    case core::PartOfSpeech::Noun:
      return 0;
    case core::PartOfSpeech::Verb:
      return 1;
    case core::PartOfSpeech::Adjective:
      return 2;
    case core::PartOfSpeech::Adverb:
      return 3;
    case core::PartOfSpeech::Particle:
      return 4;
    case core::PartOfSpeech::Auxiliary:
      return 5;
    case core::PartOfSpeech::Conjunction:
      return 6;
    case core::PartOfSpeech::Determiner:
      return 7;
    case core::PartOfSpeech::Pronoun:
      return 8;
    case core::PartOfSpeech::Symbol:
      return 9;
    case core::PartOfSpeech::Other:
    case core::PartOfSpeech::Unknown:
      return 10;
  }
  return 10;
}

// Bigram cost table [prev][next]
// clang-format off
constexpr float kBigramCostTable[11][11] = {
    //        Noun  Verb  Adj   Adv   Part  Aux   Conj  Det   Pron  Sym   Other
    /* Noun */ {0.0F, 0.5F, 0.5F, 0.3F, 0.0F, 0.0F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F},
    /* Verb */ {0.2F, 0.8F, 0.8F, 0.5F, 0.0F, 0.0F, 0.5F, 0.5F, 0.2F, 0.5F, 0.5F},
    /* Adj  */ {0.2F, 0.5F, 0.8F, 0.3F, 0.0F, 0.5F, 0.5F, 0.5F, 0.2F, 0.5F, 0.5F},
    /* Adv  */ {0.0F, 0.3F, 0.0F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.0F, 0.5F, 0.5F},
    /* Part */ {0.0F, 0.2F, 0.2F, 0.3F, 0.5F, 0.5F, 0.5F, 0.3F, 0.0F, 0.5F, 0.5F},
    /* Aux  */ {0.5F, 0.5F, 0.5F, 0.5F, 0.0F, 0.3F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F},
    /* Conj */ {0.0F, 0.2F, 0.2F, 0.2F, 0.3F, 0.5F, 0.5F, 0.2F, 0.0F, 0.3F, 0.3F},
    /* Det  */ {0.0F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.8F, 0.0F, 0.5F, 0.5F},
    /* Pron */ {0.0F, 0.5F, 0.5F, 0.3F, 0.0F, 1.0F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F},
    /* Sym  */ {0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.0F, 0.2F},
    /* Other*/ {0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F, 0.2F},
};
// clang-format on

}  // namespace

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
  float cost = edge.cost + posPrior(edge.pos);

  // Dictionary bonus
  if (edge.fromDictionary()) {
    cost += options_.dictionary_bonus;
  }
  if (edge.fromUserDict()) {
    cost += options_.user_dict_bonus;
  }

  // Formal noun / low info penalty
  if (edge.isFormalNoun()) {
    cost += options_.formal_noun_penalty;
  }
  if (edge.isLowInfo()) {
    cost += options_.low_info_penalty;
  }

  // Single character penalty
  size_t char_len = edge.end - edge.start;
  if (char_len == 1 && !edge.surface.empty()) {
    // Decode first character to check type
    auto codepoints = normalize::utf8::decode(edge.surface);
    if (!codepoints.empty()) {
      normalize::CharType ctype = normalize::classifyChar(codepoints[0]);
      if (ctype == normalize::CharType::Kanji) {
        // Check if single kanji exception
        if (kSingleKanjiExceptions.find(edge.surface) ==
            kSingleKanjiExceptions.end()) {
          cost += options_.single_kanji_penalty;
        }
      } else if (ctype == normalize::CharType::Hiragana) {
        // Check if single hiragana exception (functional words)
        if (kSingleHiraganaExceptions.find(edge.surface) ==
            kSingleHiraganaExceptions.end()) {
          cost += options_.single_hiragana_penalty;
        }
      }
    }
  }

  // Optimal length bonus
  if (isOptimalLength(edge)) {
    cost += options_.optimal_length_bonus;
  }

  // Penalize verbs ending with そう (likely verb renyokei + そう auxiliary)
  // E.g., 降りそう should split as 降り + そう, not single verb
  // Note: Adjectives like おいしそう should remain as single token
  if (edge.pos == core::PartOfSpeech::Verb && edge.surface.size() >= 6) {
    std::string_view surface = edge.surface;
    if (surface.size() >= 6 &&
        surface.substr(surface.size() - 6) == "そう") {
      cost += scorer::kPenaltyVerbSou;
    }
    // Also penalize verbs ending with そうです (verb + そう + です)
    // E.g., 食べそうです should split as 食べそう + です
    if (surface.size() >= 12 &&
        surface.substr(surface.size() - 12) == "そうです") {
      cost += scorer::kPenaltyVerbSouDesu;
    }
  }

  // Penalize unknown adjectives ending with そう with invalid lemma
  // E.g., 食べそう should not be ADJ with lemma 食べい (invalid)
  // Valid: おいしそう with lemma おいしい (ends with しい)
  // Valid: 難しそう with lemma 難しい (ends with しい)
  if (edge.pos == core::PartOfSpeech::Adjective &&
      !edge.fromDictionary() &&
      edge.surface.size() >= 6) {
    std::string_view surface = edge.surface;
    if (surface.size() >= 6 &&
        surface.substr(surface.size() - 6) == "そう") {
      // Check if lemma ends with しい (valid i-adjective pattern)
      bool valid_adj_lemma = false;
      if (edge.lemma.size() >= 6 &&
          edge.lemma.substr(edge.lemma.size() - 6) == "しい") {
        valid_adj_lemma = true;
      }
      if (!valid_adj_lemma) {
        cost += scorer::kPenaltyInvalidAdjSou;
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
      edge.lemma.size() >= 6) {  // たい = 6 bytes
    std::string_view lemma = edge.lemma;
    // Check if lemma ends with たい
    if (lemma.size() >= 6 &&
        lemma.substr(lemma.size() - 6) == "たい") {
      // Get the stem before たい
      std::string_view stem = lemma.substr(0, lemma.size() - 6);
      // Decode stem to count codepoints
      auto stem_str = std::string(stem);
      auto codepoints = normalize::utf8::decode(stem_str);

      // Very short stems (1 char) are usually invalid unless they're known verb stems
      // Known single-char verb stems: し(する), 見(見る), 来(来る), い(いる), 出(出る), etc.
      if (codepoints.size() == 1) {
        char32_t ch = codepoints[0];
        // Allow known single-character verb stems
        bool valid_single_stem =
            (ch == U'し' || ch == U'見' || ch == U'来' || ch == U'い' ||
             ch == U'出' || ch == U'寝' || ch == U'得' || ch == U'経' ||
             ch == U'着' || ch == U'居');
        if (!valid_single_stem) {
          cost += scorer::kPenaltyInvalidTaiPattern;
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
    }
  }

  // Penalize しまい/じまい as adjective - these are verb しまう renyokei
  // E.g., 食べてしまい should not have しまい parsed as adjective
  if (edge.pos == core::PartOfSpeech::Adjective &&
      !edge.fromDictionary()) {
    if (edge.surface == "しまい" || edge.surface == "じまい") {
      cost += scorer::kPenaltyShimaiAsAdj;
    }
  }

  // Penalize unknown adjectives with lemma ending in ない where stem looks like verb mizenkei
  // E.g., 走らなければ with lemma 走らない is likely verb+aux, not true adjective
  // True adjectives ending in ない: 少ない, 危ない, つまらない (stem doesn't end in あ段)
  // Verb+ない patterns: 走らない, 書かない, 読まない (stem ends in あ段 = verb mizenkei)
  if (edge.pos == core::PartOfSpeech::Adjective &&
      !edge.fromDictionary() &&
      edge.lemma.size() >= 9) {  // ない = 6 bytes, need at least 1 char before
    std::string_view lemma = edge.lemma;
    // Check if lemma ends with ない
    if (lemma.size() >= 9 &&
        lemma.substr(lemma.size() - 6) == "ない") {
      // Get the stem before ない
      std::string_view stem = lemma.substr(0, lemma.size() - 6);
      // Check the last character of stem
      if (stem.size() >= 3) {
        std::string_view last3 = stem.substr(stem.size() - 3);
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
        }
      }
    }
  }

  return cost;
}

float Scorer::connectionCost(const core::LatticeEdge& prev,
                             const core::LatticeEdge& next) const {
  float base_cost = bigramCost(prev.pos, next.pos);
  float penalty = 0.0F;
  const char* penalty_reason = nullptr;

  // Copula だ/です cannot follow verbs
  // e.g., 食べただ ✗, 食べましただ ✗
  // Valid: 学生だ ○, 静かだ ○
  // Exception: 〜そう + です is valid (e.g., しそうです, 食べそうです)
  if (prev.pos == core::PartOfSpeech::Verb &&
      next.pos == core::PartOfSpeech::Auxiliary) {
    if (next.surface == "だ" || next.surface == "です") {
      // Check if prev ends with そう (aspectual auxiliary pattern)
      bool is_sou_pattern = false;
      if (prev.surface.size() >= 6 &&
          prev.surface.substr(prev.surface.size() - 6) == "そう") {
        is_sou_pattern = true;
      }
      if (!is_sou_pattern) {
        penalty += scorer::kPenaltyCopulaAfterVerb;
        penalty_reason = "copula after verb";
      }
    }
  }

  // Penalize verb renyokei + て/てV for Ichidan verbs
  // Pattern: 教え + て should be 教えて (te-form), not renyokei + particle
  // Pattern: 教え + てくれた should be 教えて + くれた, not renyokei + てV compound
  // Ichidan verb renyokei ends with e-row hiragana (べ, め, せ, け, etc.)
  // This prevents incorrect splits like 教え+て+あげない or 教え+てくれた
  if (prev.pos == core::PartOfSpeech::Verb) {
    bool is_te_pattern = false;
    // Check if next starts with て (particle or compound verb starting with て)
    if (next.pos == core::PartOfSpeech::Particle && next.surface == "て") {
      is_te_pattern = true;
    } else if (next.pos == core::PartOfSpeech::Verb &&
               next.surface.size() >= 3 &&
               next.surface.substr(0, 3) == "て") {
      // Compound verb starting with て (てくれた, てもらう, etc.)
      is_te_pattern = true;
    }

    if (is_te_pattern && !prev.surface.empty() && prev.surface.size() >= 3) {
      std::string_view last3 = std::string_view(prev.surface).substr(
          prev.surface.size() - 3);
      // E-row hiragana (Ichidan verb renyokei endings)
      if (last3 == "べ" || last3 == "め" || last3 == "せ" ||
          last3 == "け" || last3 == "げ" || last3 == "て" ||
          last3 == "ね" || last3 == "れ" || last3 == "え" ||
          last3 == "で" || last3 == "ぜ" || last3 == "へ" ||
          last3 == "ぺ") {
        penalty += scorer::kPenaltyIchidanRenyokeiTe;
        penalty_reason = "ichidan renyokei + te pattern";
      }
    }
  }

  // Penalize Noun/Verb + て for te-form split patterns
  // This handles both Godan 音便 and Ichidan patterns when prev is Noun
  // (Ichidan with Verb is handled above)
  //
  // Godan 音便 patterns:
  //   書い + て should be 書いて (te-form), not split
  //   走っ + て should be 走って (te-form), not split
  //   読ん + で should be 読んで (te-form), not split
  // Ichidan patterns (when prev is Noun due to lattice ambiguity):
  //   教え + て should be 教えて (te-form), not split
  if ((prev.pos == core::PartOfSpeech::Noun ||
       prev.pos == core::PartOfSpeech::Verb) &&
      next.pos == core::PartOfSpeech::Particle &&
      (next.surface == "て" || next.surface == "で")) {
    if (!prev.surface.empty() && prev.surface.size() >= 3) {
      std::string_view last3 = std::string_view(prev.surface).substr(
          prev.surface.size() - 3);
      // Godan 音便 endings: い (イ音便), っ (促音便), ん (撥音便)
      // Ichidan endings: e-row hiragana (べ, め, せ, け, げ, て, ね, れ, え, etc.)
      if (last3 == "い" || last3 == "っ" || last3 == "ん" ||
          last3 == "べ" || last3 == "め" || last3 == "せ" ||
          last3 == "け" || last3 == "げ" || last3 == "て" ||
          last3 == "ね" || last3 == "れ" || last3 == "え" ||
          last3 == "で" || last3 == "ぜ" || last3 == "へ" ||
          last3 == "ぺ") {
        penalty += scorer::kPenaltyTeFormSplit;
        penalty_reason = "te-form split pattern";
      }
    }
  }

  // Boost たい-pattern adjectives following verb renyokei
  // Pattern: 走り出し + たくなってきた should connect well (desiderative auxiliary)
  // たい patterns conjugate like i-adjectives but semantically attach to verb renyokei
  // The ADJ is generated from auxiliary たい patterns: たく, たくない, たくなる, etc.
  if (prev.pos == core::PartOfSpeech::Verb &&
      next.pos == core::PartOfSpeech::Adjective &&
      next.lemma == "たい" && next.surface.size() >= 6) {
    // Check if prev ends with i-row hiragana (verb renyokei ending)
    // Godan renyokei: み, き, ぎ, し, ち, び, り, い, に (e.g., 書き, 読み, 話し)
    // Ichidan renyokei: ends with e-row or just the stem (e.g., 食べ, 見)
    // Suru renyokei: し
    if (!prev.surface.empty() && prev.surface.size() >= 3) {
      std::string_view last3 = std::string_view(prev.surface).substr(
          prev.surface.size() - 3);
      // I-row hiragana (common renyokei endings)
      if (last3 == "み" || last3 == "き" || last3 == "ぎ" ||
          last3 == "し" || last3 == "ち" || last3 == "び" ||
          last3 == "り" || last3 == "い" || last3 == "に" ||
          // E-row hiragana (ichidan renyokei)
          last3 == "べ" || last3 == "め" || last3 == "せ" ||
          last3 == "け" || last3 == "げ" || last3 == "て" ||
          last3 == "ね" || last3 == "れ" || last3 == "え") {
        penalty -= scorer::kBonusTaiAfterRenyokei;
        penalty_reason = "tai-pattern after verb renyokei";
      }
    }
  }

  // Penalize 安い (やすい) following noun that looks like verb renyokei
  // Pattern: 読み + やすい should be 読みやすい (easy to read), not 読み + 安い (cheap)
  // Verb renyokei typically ends with i-row hiragana (み, き, ぎ, し, ち, び, り, い, に)
  if (prev.pos == core::PartOfSpeech::Noun &&
      next.pos == core::PartOfSpeech::Adjective &&
      next.surface == "やすい" && next.lemma == "安い") {
    // Check if prev ends with i-row hiragana (likely verb renyokei)
    if (!prev.surface.empty() && prev.surface.size() >= 3) {
      std::string_view last3 = std::string_view(prev.surface).substr(
          prev.surface.size() - 3);
      if (last3 == "み" || last3 == "き" || last3 == "ぎ" ||
          last3 == "し" || last3 == "ち" || last3 == "び" ||
          last3 == "り" || last3 == "い" || last3 == "に") {
        penalty += scorer::kPenaltyYasuiAfterRenyokei;
        penalty_reason = "yasui adj after renyokei-like noun";
      }
    }
  }

  // Penalize VERB + ながら (PARTICLE) split when verb is in renyokei form
  // Pattern: 飲み + ながら should be 飲みながら (single token), not split
  // This prevents incorrect splits when longer forms exist in the lattice
  if (prev.pos == core::PartOfSpeech::Verb &&
      next.pos == core::PartOfSpeech::Particle &&
      next.surface == "ながら") {
    if (!prev.surface.empty() && prev.surface.size() >= 3) {
      std::string_view last3 = std::string_view(prev.surface).substr(
          prev.surface.size() - 3);
      // I-row hiragana (Godan renyokei) and e-row (Ichidan renyokei)
      if (last3 == "み" || last3 == "き" || last3 == "ぎ" ||
          last3 == "し" || last3 == "ち" || last3 == "び" ||
          last3 == "り" || last3 == "い" || last3 == "に" ||
          last3 == "べ" || last3 == "め" || last3 == "せ" ||
          last3 == "け" || last3 == "げ" || last3 == "て" ||
          last3 == "ね" || last3 == "れ" || last3 == "え") {
        penalty += scorer::kPenaltyNagaraSplit;
        penalty_reason = "nagara split after renyokei verb";
      }
    }
  }

  // Penalize NOUN + そう (appearance auxiliary) when noun looks like verb renyokei
  // Pattern: 話し + そう should be verb renyokei + そう auxiliary, not noun + adverb
  // Verb renyokei typically ends with i-row hiragana
  if (prev.pos == core::PartOfSpeech::Noun &&
      next.pos == core::PartOfSpeech::Adverb &&
      next.surface == "そう") {
    if (!prev.surface.empty() && prev.surface.size() >= 3) {
      std::string_view last3 = std::string_view(prev.surface).substr(
          prev.surface.size() - 3);
      // I-row hiragana (Godan renyokei) and e-row (Ichidan renyokei)
      if (last3 == "み" || last3 == "き" || last3 == "ぎ" ||
          last3 == "し" || last3 == "ち" || last3 == "び" ||
          last3 == "り" || last3 == "い" || last3 == "に" ||
          last3 == "べ" || last3 == "め" || last3 == "せ" ||
          last3 == "け" || last3 == "げ" || last3 == "て" ||
          last3 == "ね" || last3 == "れ" || last3 == "え") {
        penalty += scorer::kPenaltySouAfterRenyokei;
        penalty_reason = "sou aux after renyokei-like noun";
      }
    }
  }

  // Penalize AUX だ/です + character speech suffix (にゃ, にゃん, わ, etc.)
  // Pattern: だ + にゃ should be だにゃ (single token), not split
  // Character speech variants like だにゃ, ですわ should be single tokens
  if (prev.pos == core::PartOfSpeech::Auxiliary &&
      next.pos == core::PartOfSpeech::Auxiliary) {
    if ((prev.surface == "だ" || prev.surface == "です") &&
        (next.surface == "にゃ" || next.surface == "にゃん" ||
         next.surface == "わ" || next.surface == "のだ" ||
         next.surface == "よ" || next.surface == "ね" ||
         next.surface == "ぞ" || next.surface == "さ")) {
      penalty += scorer::kPenaltyCharacterSpeechSplit;
      penalty_reason = "split character speech pattern";
    }
  }

  // Boost ADJ(連用形・く) → VERB(なる/なり) pattern
  // Pattern: 美しく + なる/なりたかった should connect well (to become beautiful)
  // This is a very common grammatical pattern in Japanese
  if (prev.pos == core::PartOfSpeech::Adjective &&
      next.pos == core::PartOfSpeech::Verb) {
    // Check if prev ends with く (adjective adverbial form)
    if (!prev.surface.empty() && prev.surface.size() >= 3) {
      std::string_view last3 = std::string_view(prev.surface).substr(
          prev.surface.size() - 3);
      if (last3 == "く") {
        // Check if next is なる or starts with なり
        if (next.lemma == "なる" ||
            (next.surface.size() >= 6 &&
             next.surface.substr(0, 6) == "なり")) {
          penalty -= scorer::kBonusAdjKuNaru;
          penalty_reason = "adj-ku + naru pattern";
        }
      }
    }
  }

  // Penalize NOUN + compound verb auxiliary when noun looks like verb renyokei
  // Pattern: 読み + 終わる should be verb renyokei + auxiliary, not noun + verb
  // Compound verb auxiliaries: 終わる, 始める, 過ぎる, 続ける, 直す, 合う, 出す, 込む, 切る, etc.
  // Check by surface pattern since lemma may not be set during lattice construction
  if (prev.pos == core::PartOfSpeech::Noun &&
      next.pos == core::PartOfSpeech::Verb &&
      next.surface.size() >= 3) {
    // Check if next starts with compound verb auxiliary kanji
    // Get first 3 bytes (one kanji character in UTF-8)
    std::string_view first_char = next.surface.substr(0, 3);
    bool is_compound_aux = (first_char == "終" || first_char == "始" ||
                            first_char == "過" || first_char == "続" ||
                            first_char == "直" || first_char == "合" ||
                            first_char == "出" || first_char == "込" ||
                            first_char == "切" || first_char == "損" ||
                            first_char == "返" || first_char == "忘" ||
                            first_char == "残" || first_char == "掛");
    if (is_compound_aux) {
      // Check if prev ends with i-row hiragana (likely verb renyokei)
      if (!prev.surface.empty() && prev.surface.size() >= 3) {
        std::string_view last3 = std::string_view(prev.surface).substr(
            prev.surface.size() - 3);
        if (last3 == "み" || last3 == "き" || last3 == "ぎ" ||
            last3 == "し" || last3 == "ち" || last3 == "び" ||
            last3 == "り" || last3 == "い" || last3 == "に" ||
            // E-row for ichidan renyokei
            last3 == "べ" || last3 == "め" || last3 == "せ" ||
            last3 == "け" || last3 == "げ" || last3 == "て" ||
            last3 == "ね" || last3 == "れ" || last3 == "え") {
          penalty += scorer::kPenaltyCompoundAuxAfterRenyokei;
          penalty_reason = "compound aux after renyokei-like noun";
        }
      }
    }
  }

  float total = base_cost + penalty;

  SUZUME_DEBUG_CONN("[CONN] \"" << prev.surface << "\" ("
                    << core::posToString(prev.pos) << ") → \""
                    << next.surface << "\" ("
                    << core::posToString(next.pos) << "): "
                    << "base=" << base_cost);
  if (penalty > 0.0F && penalty_reason != nullptr) {
    SUZUME_DEBUG_CONN(" + penalty=" << penalty << " (" << penalty_reason << ")");
  }
  SUZUME_DEBUG_CONN(" = " << total << "\n");

  return total;
}

}  // namespace suzume::analysis
