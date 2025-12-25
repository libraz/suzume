/**
 * @file join_candidates.cpp
 * @brief Join-based candidate generation for tokenizer
 */

#include "join_candidates.h"

#include "grammar/inflection.h"
#include "normalize/utf8.h"
#include "tokenizer_utils.h"

namespace suzume::analysis {

namespace {

// V2 Subsidiary verbs for compound verb joining
struct SubsidiaryVerb {
  const char* surface;
  const char* base_ending;  // Base form ending for verb type detection
};

// List of V2 verbs that can form compound verbs
const SubsidiaryVerb kSubsidiaryVerbs[] = {
    {"込む", "む"},      // 読み込む, 飛び込む
    {"出す", "す"},      // 呼び出す, 書き出す
    {"始める", "める"},  // 読み始める, 書き始める
    {"続ける", "ける"},  // 読み続ける, 走り続ける
    {"続く", "く"},      // 引き続く
    {"返す", "す"},      // 繰り返す
    {"返る", "る"},      // 振り返る
    {"つける", "ける"},  // 見つける
    {"つかる", "る"},    // 見つかる
    {"替える", "える"},  // 切り替える
    {"換える", "える"},  // 入れ換える
    {"合う", "う"},      // 話し合う
    {"合わせる", "せる"}, // 組み合わせる
    {"消す", "す"},      // 取り消す
    {"過ぎる", "る"},    // 読み過ぎる
    {"直す", "す"},      // やり直す
    {"終わる", "る"},    // 読み終わる
    {"切る", "る"},      // 締め切る
};

// 連用形 (continuative form) endings for Godan verbs
struct RenyokeiPattern {
  char32_t renyokei;   // 連用形 ending
  char32_t base;       // Base form ending
};

const RenyokeiPattern kGodanRenyokei[] = {
    {U'き', U'く'},  // 書き → 書く
    {U'ぎ', U'ぐ'},  // 泳ぎ → 泳ぐ
    {U'し', U'す'},  // 話し → 話す
    {U'ち', U'つ'},  // 持ち → 持つ
    {U'に', U'ぬ'},  // 死に → 死ぬ
    {U'び', U'ぶ'},  // 飛び → 飛ぶ
    {U'み', U'む'},  // 読み → 読む
    {U'り', U'る'},  // 取り → 取る
    {U'い', U'う'},  // 思い → 思う
};

// Cost bonus for compound verb joining (lower = preferred)
constexpr float kCompoundVerbBonus = -0.8F;

// Additional bonus when V1 base form is in dictionary
constexpr float kVerifiedV1Bonus = -0.3F;

// Productive prefixes for prefix+noun joining
struct ProductivePrefix {
  char32_t codepoint;
  float bonus;
  bool needs_kanji;
};

const ProductivePrefix kProductivePrefixes[] = {
    // Honorific prefixes
    {U'お', -0.5F, true},   // お水, お金, お店
    {U'ご', -0.5F, true},   // ご確認, ご連絡, ご注意
    {U'御', -0.5F, true},   // 御 (formal version)

    // Negation prefixes
    {U'不', -0.4F, true},   // 不安, 不要, 不便
    {U'未', -0.4F, true},   // 未経験, 未確認
    {U'非', -0.4F, true},   // 非常, 非公開
    {U'無', -0.4F, true},   // 無理, 無料

    // Degree/quantity prefixes
    {U'超', -0.3F, true},   // 超人, 超高速
    {U'再', -0.4F, true},   // 再開, 再確認
    {U'準', -0.4F, true},   // 準備, 準決勝
    {U'副', -0.4F, true},   // 副社長, 副作用
    {U'総', -0.4F, true},   // 総合, 総数
    {U'各', -0.4F, true},   // 各地, 各種
    {U'両', -0.4F, true},   // 両方, 両手
    {U'最', -0.4F, true},   // 最高, 最新
    {U'全', -0.4F, true},   // 全部, 全員
    {U'半', -0.4F, true},   // 半分, 半額
};

constexpr size_t kNumPrefixes = sizeof(kProductivePrefixes) / sizeof(kProductivePrefixes[0]);

// Maximum noun length for prefix joining
constexpr size_t kMaxNounLenForPrefix = 6;

// Additional bonus when noun part is in dictionary
constexpr float kVerifiedNounBonus = -0.3F;

// Te-form auxiliary verb patterns
struct TeFormAuxiliary {
  const char* stem;
  const char* base_form;
};

const TeFormAuxiliary kTeFormAuxiliaries[] = {
    {"い", "いく"},      // 〜ていく
    {"く", "くる"},      // 〜てくる
    {"み", "みる"},      // 〜てみる
    {"お", "おく"},      // 〜ておく
    {"しま", "しまう"},  // 〜てしまう
    {"ちゃ", "しまう"},  // 〜ちゃう (colloquial)
    {"じゃ", "しまう"},  // 〜じゃう (colloquial)
    {"もら", "もらう"},  // 〜てもらう
    {"くれ", "くれる"},  // 〜てくれる
    {"あげ", "あげる"},  // 〜てあげる
    {"や", "やる"},      // 〜てやる
};

// Cost bonus for te-form + auxiliary pattern
constexpr float kTeFormAuxBonus = -0.8F;

}  // namespace

void addCompoundVerbJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const dictionary::DictionaryManager& dict_manager,
    const Scorer& scorer) {
  using CharType = normalize::CharType;

  if (start_pos >= char_types.size()) {
    return;
  }

  // Must start with kanji (V1 verb stem)
  if (char_types[start_pos] != CharType::Kanji) {
    return;
  }

  // Find the kanji portion (V1 stem)
  size_t kanji_end = start_pos + 1;
  while (kanji_end < char_types.size() && kanji_end - start_pos < 4 &&
         char_types[kanji_end] == CharType::Kanji) {
    ++kanji_end;
  }

  // Next must be hiragana (連用形 ending)
  if (kanji_end >= char_types.size() ||
      char_types[kanji_end] != CharType::Hiragana) {
    return;
  }

  // Get the hiragana character (potential 連用形 ending)
  char32_t renyokei_char = codepoints[kanji_end];

  // Check if it's a valid 連用形 ending
  char32_t base_ending = 0;
  for (const auto& pattern : kGodanRenyokei) {
    if (pattern.renyokei == renyokei_char) {
      base_ending = pattern.base;
      break;
    }
  }

  // If not a 連用形 ending, this might be an Ichidan verb
  bool is_ichidan = (base_ending == 0);

  // Position after 連用形 (for Godan) or after stem (for Ichidan)
  size_t v2_start = is_ichidan ? kanji_end : kanji_end + 1;

  if (v2_start >= codepoints.size()) {
    return;
  }

  // Get byte positions
  size_t start_byte = charPosToBytePos(codepoints, start_pos);
  size_t v2_start_byte = charPosToBytePos(codepoints, v2_start);

  // Look for V2 (subsidiary verb)
  for (const auto& v2_verb : kSubsidiaryVerbs) {
    std::string_view v2_surface(v2_verb.surface);

    // Check if text at v2_start matches this V2 verb
    if (v2_start_byte + v2_surface.size() > text.size()) {
      continue;
    }

    std::string_view text_at_v2 = text.substr(v2_start_byte, v2_surface.size());
    if (text_at_v2 != v2_surface) {
      continue;
    }

    // Build the V1 base form for verification
    std::string v1_base;
    size_t v1_end_byte = is_ichidan ? v2_start_byte : charPosToBytePos(codepoints, kanji_end);
    v1_base = std::string(text.substr(start_byte, v1_end_byte - start_byte));

    if (!is_ichidan) {
      v1_base += normalize::utf8::encode({base_ending});
    } else {
      v1_base += "る";
    }

    // Check if V1 base form is in dictionary
    auto v1_results = dict_manager.lookup(v1_base, 0);
    bool v1_in_dict = false;
    for (const auto& result : v1_results) {
      if (result.entry != nullptr && result.entry->surface == v1_base &&
          result.entry->pos == core::PartOfSpeech::Verb) {
        v1_in_dict = true;
        break;
      }
    }

    // Calculate compound verb end position
    size_t compound_end_byte = v2_start_byte + v2_surface.size();

    // Find character position for compound end
    size_t compound_end_pos = v2_start;
    size_t byte_count = v2_start_byte;
    while (compound_end_pos < codepoints.size() && byte_count < compound_end_byte) {
      char32_t code = codepoints[compound_end_pos];
      if (code < 0x80) {
        byte_count += 1;
      } else if (code < 0x800) {
        byte_count += 2;
      } else if (code < 0x10000) {
        byte_count += 3;
      } else {
        byte_count += 4;
      }
      ++compound_end_pos;
    }

    // Build the compound verb surface
    std::string compound_surface(text.substr(start_byte, compound_end_byte - start_byte));

    // Calculate cost
    float base_cost = scorer.posPrior(core::PartOfSpeech::Verb);
    float final_cost = base_cost + kCompoundVerbBonus;

    if (v1_in_dict) {
      final_cost += kVerifiedV1Bonus;
    }

    uint8_t flags = core::LatticeEdge::kFromDictionary;

    lattice.addEdge(compound_surface, static_cast<uint32_t>(start_pos),
                    static_cast<uint32_t>(compound_end_pos), core::PartOfSpeech::Verb,
                    final_cost, flags, v1_base);

    return;
  }
}

void addPrefixNounJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const dictionary::DictionaryManager& dict_manager,
    const Scorer& scorer) {
  using CharType = normalize::CharType;

  if (start_pos >= codepoints.size()) {
    return;
  }

  // Check if current character is a productive prefix
  char32_t current_char = codepoints[start_pos];
  const ProductivePrefix* matched_prefix = nullptr;

  for (size_t idx = 0; idx < kNumPrefixes; ++idx) {
    if (kProductivePrefixes[idx].codepoint == current_char) {
      matched_prefix = &kProductivePrefixes[idx];
      break;
    }
  }

  if (matched_prefix == nullptr) {
    return;
  }

  // Check if there's a noun part following
  size_t noun_start = start_pos + 1;
  if (noun_start >= codepoints.size()) {
    return;
  }

  // For most prefixes, the noun part should start with kanji
  if (matched_prefix->needs_kanji) {
    if (char_types[noun_start] != CharType::Kanji) {
      return;
    }
  } else {
    if (char_types[noun_start] != CharType::Kanji &&
        char_types[noun_start] != CharType::Katakana) {
      return;
    }
  }

  // Find the end of the noun part
  CharType noun_type = char_types[noun_start];
  size_t noun_end = noun_start + 1;

  while (noun_end < codepoints.size() &&
         noun_end - noun_start < kMaxNounLenForPrefix &&
         char_types[noun_end] == noun_type) {
    ++noun_end;
  }

  if (noun_end <= noun_start) {
    return;
  }

  // Check dictionary for compound nouns
  size_t noun_start_byte = charPosToBytePos(codepoints, noun_start);
  auto noun_results = dict_manager.lookup(text, noun_start_byte);
  bool noun_in_dict = false;
  size_t dict_noun_end = noun_end;

  for (const auto& result : noun_results) {
    if (result.entry != nullptr &&
        result.entry->pos == core::PartOfSpeech::Noun) {
      if (result.length > dict_noun_end - noun_start) {
        dict_noun_end = noun_start + result.length;
        noun_in_dict = true;
      } else if (result.length == noun_end - noun_start) {
        noun_in_dict = true;
      }
    }
  }

  if (dict_noun_end > noun_end) {
    noun_end = dict_noun_end;
  } else {
    // Skip single-kanji noun when followed by hiragana (likely verb pattern)
    if (noun_end - noun_start == 1 && noun_end < codepoints.size()) {
      if (char_types[noun_end] == CharType::Hiragana) {
        return;
      }
    }
  }

  // Check if the combined form is already in dictionary
  size_t start_byte = charPosToBytePos(codepoints, start_pos);
  auto combined_results = dict_manager.lookup(text, start_byte);

  for (const auto& result : combined_results) {
    if (result.entry != nullptr &&
        result.length == noun_end - start_pos) {
      return;  // Already in dictionary
    }
  }

  // Generate joined candidate
  size_t end_byte = charPosToBytePos(codepoints, noun_end);
  std::string surface(text.substr(start_byte, end_byte - start_byte));

  float base_cost = scorer.posPrior(core::PartOfSpeech::Noun);
  float final_cost = base_cost + matched_prefix->bonus;

  if (noun_in_dict) {
    final_cost += kVerifiedNounBonus;
  }

  uint8_t flags = core::LatticeEdge::kIsUnknown;

  lattice.addEdge(surface, static_cast<uint32_t>(start_pos),
                  static_cast<uint32_t>(noun_end), core::PartOfSpeech::Noun,
                  final_cost, flags, "");
}

void addTeFormAuxiliaryCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const Scorer& scorer) {
  using CharType = normalize::CharType;

  if (start_pos >= codepoints.size()) {
    return;
  }

  // Look for て or で at this position
  char32_t current = codepoints[start_pos];
  if (current != U'て' && current != U'で') {
    return;
  }

  // Check if there's hiragana following
  size_t aux_start = start_pos + 1;
  if (aux_start >= codepoints.size() ||
      char_types[aux_start] != CharType::Hiragana) {
    return;
  }

  // Get byte positions
  size_t te_byte = charPosToBytePos(codepoints, start_pos);
  size_t aux_start_byte = charPosToBytePos(codepoints, aux_start);

  // Find the extent of hiragana following て/で
  size_t hiragana_end = aux_start;
  while (hiragana_end < codepoints.size() &&
         hiragana_end - aux_start < 10 &&
         char_types[hiragana_end] == CharType::Hiragana) {
    ++hiragana_end;
  }

  // Use inflection analysis
  static grammar::Inflection inflection;

  // Try each auxiliary pattern
  for (const auto& aux : kTeFormAuxiliaries) {
    std::string_view stem(aux.stem);

    std::string_view text_after_te = text.substr(aux_start_byte);
    if (text_after_te.size() < stem.size()) {
      continue;
    }

    if (text_after_te.substr(0, stem.size()) != stem) {
      continue;
    }

    size_t stem_char_len = normalize::utf8::decode(std::string(stem)).size();

    // Try different lengths after the stem
    for (size_t aux_end = aux_start + stem_char_len;
         aux_end <= hiragana_end && aux_end <= aux_start + 8; ++aux_end) {
      size_t aux_end_byte = charPosToBytePos(codepoints, aux_end);
      std::string aux_surface(text.substr(aux_start_byte,
                                          aux_end_byte - aux_start_byte));

      auto best = inflection.getBest(aux_surface);
      if (best.confidence > 0.4F && best.base_form == aux.base_form) {
        size_t combo_end_byte = aux_end_byte;
        std::string combo_surface(text.substr(te_byte, combo_end_byte - te_byte));

        float final_cost = scorer.posPrior(core::PartOfSpeech::Verb) +
                           kTeFormAuxBonus;

        uint8_t flags = core::LatticeEdge::kIsUnknown;

        lattice.addEdge(combo_surface, static_cast<uint32_t>(start_pos),
                        static_cast<uint32_t>(aux_end), core::PartOfSpeech::Verb,
                        final_cost, flags, aux.base_form);

        break;
      }
    }
  }
}

}  // namespace suzume::analysis
