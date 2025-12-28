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
  const char* surface;      // Kanji form (or hiragana if no kanji)
  const char* reading;      // Hiragana reading (nullptr if same as surface)
  const char* base_ending;  // Base form ending for verb type detection
  const char* base_form;    // Base form for lemma generation
};

// List of V2 verbs that can form compound verbs
// Includes both base forms and renyokei forms for auxiliary attachment
// Reading field enables matching both kanji and hiragana patterns
const SubsidiaryVerb kSubsidiaryVerbs[] = {
    // Base forms (終止形)
    {"込む", "こむ", "む", "込む"},          // 読み込む, 飛び込む, 飛びこむ
    {"出す", "だす", "す", "出す"},          // 呼び出す, 書き出す, 走りだす
    {"始める", "はじめる", "める", "始める"}, // 読み始める, 読みはじめる
    {"続ける", "つづける", "ける", "続ける"}, // 読み続ける, 読みつづける
    {"続く", "つづく", "く", "続く"},        // 引き続く
    {"返す", "かえす", "す", "返す"},        // 繰り返す, 繰りかえす
    {"返る", "かえる", "る", "返る"},        // 振り返る, 振りかえる
    {"つける", nullptr, "ける", "つける"},  // 見つける (already hiragana)
    {"つかる", nullptr, "る", "つかる"},    // 見つかる (already hiragana)
    {"替える", "かえる", "える", "替える"}, // 切り替える
    {"換える", "かえる", "える", "換える"}, // 入れ換える
    {"合う", "あう", "う", "合う"},          // 話し合う, 話しあう
    {"合わせる", "あわせる", "せる", "合わせる"}, // 組み合わせる
    {"消す", "けす", "す", "消す"},          // 取り消す
    {"過ぎる", "すぎる", "る", "過ぎる"},    // 読み過ぎる, 読みすぎる
    {"直す", "なおす", "す", "直す"},        // やり直す, やりなおす
    {"終わる", "おわる", "る", "終わる"},    // 読み終わる, 読みおわる
    {"終える", "おえる", "える", "終える"},  // 読み終える, 読みおえる (ichidan)
    {"切る", "きる", "る", "切る"},          // 締め切る, 締めきる
    {"切れる", "きれる", "れる", "切れる"},  // 使い切れる (ichidan)
    {"出る", "でる", "る", "出る"},          // 飛び出る (ichidan)
    // Renyokei forms (連用形) for たい/たくなかった/etc. attachment
    {"込み", "こみ", "む", "込む"},          // 読み込みたい, 飛びこみたい
    {"出し", "だし", "す", "出す"},          // 走り出したい, 走りだしたい
    {"始め", "はじめ", "める", "始める"},    // 読み始めたい
    {"続け", "つづけ", "ける", "続ける"},    // 読み続けたい
    {"続き", "つづき", "く", "続く"},        // 引き続きたい
    {"返し", "かえし", "す", "返す"},        // 繰り返したい
    {"返り", "かえり", "る", "返る"},        // 振り返りたい
    {"つけ", nullptr, "ける", "つける"},    // 見つけたい
    {"つかり", nullptr, "る", "つかる"},    // 見つかりたい
    {"替え", "かえ", "える", "替える"},      // 切り替えたい
    {"換え", "かえ", "える", "換える"},      // 入れ換えたい
    {"合い", "あい", "う", "合う"},          // 話し合いたい
    {"合わせ", "あわせ", "せる", "合わせる"}, // 組み合わせたい
    {"消し", "けし", "す", "消す"},          // 取り消したい
    {"過ぎ", "すぎ", "る", "過ぎる"},        // 読み過ぎたい, 読みすぎたい
    {"直し", "なおし", "す", "直す"},        // やり直したい
    {"終わり", "おわり", "る", "終わる"},    // 読み終わりたい
    {"終え", "おえ", "える", "終える"},      // 読み終えたい (ichidan renyokei)
    {"切り", "きり", "る", "切る"},          // 締め切りたい
    {"切れ", "きれ", "れる", "切れる"},      // 使い切れたい (ichidan renyokei)
    // Note: "出" (で) renyokei is NOT added because it conflicts with particle で
    // 飛び出る forms like 飛び出たい are handled by the base form "出る" entry
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
    // Note: Honorific prefixes お, ご, 御 are NOT included here.
    // They should be tokenized separately as PREFIX + NOUN.
    // E.g., お水 → お(PREFIX) + 水(NOUN), not お水(NOUN)

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
  bool is_benefactive;  // Benefactive verbs should not form negative compounds
};

const TeFormAuxiliary kTeFormAuxiliaries[] = {
    {"い", "いく", false},      // 〜ていく
    {"く", "くる", false},      // 〜てくる
    {"み", "みる", false},      // 〜てみる
    {"お", "おく", false},      // 〜ておく
    {"しま", "しまう", false},  // 〜てしまう
    {"ちゃ", "しまう", false},  // 〜ちゃう (colloquial)
    {"じゃ", "しまう", false},  // 〜じゃう (colloquial)
    {"もら", "もらう", true},   // 〜てもらう (benefactive)
    {"くれ", "くれる", true},   // 〜てくれる (benefactive)
    {"あげ", "あげる", true},   // 〜てあげる (benefactive)
    {"や", "やる", true},       // 〜てやる (benefactive)
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
    std::string_view v2_reading(v2_verb.reading ? v2_verb.reading : "");

    // Check if text at v2_start matches this V2 verb (kanji or reading)
    bool matched_kanji = false;
    bool matched_reading = false;
    size_t matched_len = 0;

    // Try kanji match first
    if (v2_start_byte + v2_surface.size() <= text.size()) {
      std::string_view text_at_v2 = text.substr(v2_start_byte, v2_surface.size());
      if (text_at_v2 == v2_surface) {
        matched_kanji = true;
        matched_len = v2_surface.size();
      }
    }

    // Try reading (hiragana) match if kanji didn't match
    if (!matched_kanji && !v2_reading.empty() &&
        v2_start_byte + v2_reading.size() <= text.size()) {
      std::string_view text_at_v2 = text.substr(v2_start_byte, v2_reading.size());
      if (text_at_v2 == v2_reading) {
        matched_reading = true;
        matched_len = v2_reading.size();
      }
    }

    if (!matched_kanji && !matched_reading) {
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

    // Calculate compound verb end position using matched length
    size_t compound_end_byte = v2_start_byte + matched_len;

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

    // Build compound verb base form (V1 renyokei + V2 base form)
    // e.g., 走り + 出す = 走り出す, 走り + だす = 走り出す
    std::string compound_base;
    size_t v1_renyokei_end = is_ichidan ? v2_start_byte : charPosToBytePos(codepoints, kanji_end + 1);
    compound_base = std::string(text.substr(start_byte, v1_renyokei_end - start_byte));
    // Use the pre-defined base_form for V2 (always in kanji form for consistency)
    compound_base += v2_verb.base_form;

    // Calculate cost
    float base_cost = scorer.posPrior(core::PartOfSpeech::Verb);
    float final_cost = base_cost + kCompoundVerbBonus;

    if (v1_in_dict) {
      final_cost += kVerifiedV1Bonus;
    }

    uint8_t flags = core::LatticeEdge::kFromDictionary;

    lattice.addEdge(compound_surface, static_cast<uint32_t>(start_pos),
                    static_cast<uint32_t>(compound_end_pos), core::PartOfSpeech::Verb,
                    final_cost, flags, compound_base);

    return;
  }
}

void addHiraganaCompoundVerbJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const dictionary::DictionaryManager& dict_manager,
    const Scorer& scorer) {
  using CharType = normalize::CharType;

  if (start_pos >= char_types.size()) {
    return;
  }

  // Must start with hiragana (for all-hiragana compound verbs like やりなおす)
  if (char_types[start_pos] != CharType::Hiragana) {
    return;
  }

  // Get byte position for start
  size_t start_byte = charPosToBytePos(codepoints, start_pos);

  // For each V2 subsidiary verb, check if it appears after a potential V1
  for (const auto& v2_verb : kSubsidiaryVerbs) {
    // Only consider V2 with readings (hiragana patterns)
    if (v2_verb.reading == nullptr) {
      continue;
    }
    std::string_view v2_reading(v2_verb.reading);

    // For hiragana compound verbs, V1 must be at least 2 characters
    // Try different V1 lengths (2-4 characters)
    for (size_t v1_len = 2; v1_len <= 4; ++v1_len) {
      size_t v2_start = start_pos + v1_len;
      if (v2_start >= codepoints.size()) {
        break;
      }

      // All characters in V1 must be hiragana
      bool all_hiragana = true;
      for (size_t idx = start_pos; idx < v2_start; ++idx) {
        if (char_types[idx] != CharType::Hiragana) {
          all_hiragana = false;
          break;
        }
      }
      if (!all_hiragana) {
        continue;
      }

      size_t v2_start_byte = charPosToBytePos(codepoints, v2_start);

      // Check if V2 reading matches at v2_start
      if (v2_start_byte + v2_reading.size() > text.size()) {
        continue;
      }
      std::string_view text_at_v2 = text.substr(v2_start_byte, v2_reading.size());
      if (text_at_v2 != v2_reading) {
        continue;
      }

      // Extract V1 portion and determine its base form
      std::string_view v1_surface = text.substr(start_byte, v2_start_byte - start_byte);

      // Get the last character of V1 to determine verb type
      char32_t last_char = codepoints[v2_start - 1];

      // Check if it's a valid renyokei ending
      char32_t base_ending = 0;
      for (const auto& pattern : kGodanRenyokei) {
        if (pattern.renyokei == last_char) {
          base_ending = pattern.base;
          break;
        }
      }

      // Build V1 base form
      std::string v1_base;
      bool is_ichidan = (base_ending == 0);

      if (!is_ichidan) {
        // Godan: replace last char with base ending
        v1_base = std::string(v1_surface.substr(0, v1_surface.size() - 3));  // Remove last hiragana (3 bytes)
        v1_base += normalize::utf8::encode({base_ending});
      } else {
        // Ichidan: add る
        v1_base = std::string(v1_surface) + "る";
      }

      // Verify V1 is in dictionary as a verb
      auto v1_results = dict_manager.lookup(v1_base, 0);
      bool v1_in_dict = false;
      for (const auto& result : v1_results) {
        if (result.entry != nullptr && result.entry->surface == v1_base &&
            result.entry->pos == core::PartOfSpeech::Verb) {
          v1_in_dict = true;
          break;
        }
      }

      if (!v1_in_dict) {
        continue;  // V1 must be a known verb for hiragana compounds
      }

      // Calculate compound verb end position
      size_t compound_end_byte = v2_start_byte + v2_reading.size();

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

      // Build compound verb surface and base form
      std::string compound_surface(text.substr(start_byte, compound_end_byte - start_byte));

      // Compound base = V1 renyokei + V2 base form (in kanji)
      std::string compound_base = std::string(v1_surface) + v2_verb.base_form;

      // Calculate cost
      float base_cost = scorer.posPrior(core::PartOfSpeech::Verb);
      float final_cost = base_cost + kCompoundVerbBonus + kVerifiedV1Bonus;

      uint8_t flags = core::LatticeEdge::kFromDictionary;

      lattice.addEdge(compound_surface, static_cast<uint32_t>(start_pos),
                      static_cast<uint32_t>(compound_end_pos), core::PartOfSpeech::Verb,
                      final_cost, flags, compound_base);

      return;  // Found a match, stop searching
    }
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
        // Skip negative forms of benefactive verbs
        // E.g., てあげない should be split as て + あげない, not combined
        // This allows proper analysis of patterns like 教えてあげない
        if (aux.is_benefactive) {
          // Check if the surface ends with negative patterns
          bool is_negative = (aux_surface.size() >= 6 &&
                              (aux_surface.substr(aux_surface.size() - 6) == "ない" ||
                               aux_surface.substr(aux_surface.size() - 6) == "なく"));
          if (aux_surface.size() >= 12) {
            std::string_view suffix = std::string_view(aux_surface).substr(
                aux_surface.size() - 12);
            if (suffix == "なかった" || suffix == "なくて") {
              is_negative = true;
            }
          }
          if (is_negative) {
            continue;  // Don't create compound for benefactive negative
          }
        }

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
