#include "analysis/scorer.h"

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
  if (prev.pos == core::PartOfSpeech::Verb &&
      next.pos == core::PartOfSpeech::Auxiliary) {
    if (next.surface == "だ" || next.surface == "です") {
      penalty += 3.0F;  // Strong penalty for invalid grammar
      penalty_reason = "copula after verb";
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
        penalty += 2.0F;  // Strong penalty - prefer verb + やすい auxiliary
        penalty_reason = "yasui adj after renyokei-like noun";
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
