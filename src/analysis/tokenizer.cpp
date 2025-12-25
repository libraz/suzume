#include "analysis/tokenizer.h"

#include "grammar/inflection.h"
#include "normalize/utf8.h"

namespace suzume::analysis {

namespace {

// V2 Subsidiary verbs for compound verb joining
// These verbs commonly appear as the second part of compound verbs
// Format: {surface, conj_type_suffix for detection}
struct SubsidiaryVerb {
  const char* surface;
  const char* base_ending;  // Base form ending for verb type detection
};

// List of V2 verbs that can form compound verbs
// Order matters: longer patterns should come first
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
// Maps: 連用形 ending -> base form ending
struct RenyokeiPattern {
  char32_t renyokei;   // 連用形 ending (e.g., き, み, び)
  char32_t base;       // Base form ending (e.g., く, む, ぶ)
};

const RenyokeiPattern kGodanRenyokei[] = {
    {U'き', U'く'},  // 書き → 書く (カ行)
    {U'ぎ', U'ぐ'},  // 泳ぎ → 泳ぐ (ガ行)
    {U'し', U'す'},  // 話し → 話す (サ行)
    {U'ち', U'つ'},  // 持ち → 持つ (タ行)
    {U'に', U'ぬ'},  // 死に → 死ぬ (ナ行)
    {U'び', U'ぶ'},  // 飛び → 飛ぶ (バ行)
    {U'み', U'む'},  // 読み → 読む (マ行)
    {U'り', U'る'},  // 取り → 取る (ラ行)
    {U'い', U'う'},  // 思い → 思う (ワ行)
};

// Cost bonus for compound verb joining (lower = preferred)
constexpr float kCompoundVerbBonus = -0.8F;

// Additional bonus when V1 base form is in dictionary
constexpr float kVerifiedV1Bonus = -0.3F;

}  // namespace

Tokenizer::Tokenizer(const dictionary::DictionaryManager& dict_manager,
                     const Scorer& scorer,
                     const UnknownWordGenerator& unknown_gen)
    : dict_manager_(dict_manager),
      scorer_(scorer),
      unknown_gen_(unknown_gen) {}

core::Lattice Tokenizer::buildLattice(
    std::string_view text, const std::vector<char32_t>& codepoints,
    const std::vector<normalize::CharType>& char_types) const {
  core::Lattice lattice(codepoints.size());

  // Process each position
  for (size_t pos = 0; pos < codepoints.size(); ++pos) {
    // Add dictionary candidates
    addDictionaryCandidates(lattice, text, codepoints, pos);

    // Add unknown word candidates
    addUnknownCandidates(lattice, text, codepoints, pos, char_types);

    // Add mixed script joining candidates (Web開発, APIリクエスト, etc.)
    addMixedScriptCandidates(lattice, text, codepoints, pos, char_types);

    // Add compound noun split candidates (人工知能 → 人工 + 知能)
    addCompoundSplitCandidates(lattice, text, codepoints, pos, char_types);

    // Add noun+verb split candidates (本買った → 本 + 買った)
    addNounVerbSplitCandidates(lattice, text, codepoints, pos, char_types);

    // Add compound verb join candidates (飛び + 込む → 飛び込む)
    addCompoundVerbJoinCandidates(lattice, text, codepoints, pos, char_types);

    // Add prefix + noun join candidates (お + 水 → お水)
    addPrefixNounJoinCandidates(lattice, text, codepoints, pos, char_types);

    // Add te-form + auxiliary verb candidates (学んで + いく → 学んで + いきたい)
    addTeFormAuxiliaryCandidates(lattice, text, codepoints, pos, char_types);
  }

  return lattice;
}

size_t Tokenizer::charPosToBytePos(const std::vector<char32_t>& codepoints, size_t char_pos) {
  size_t byte_pos = 0;
  for (size_t idx = 0; idx < char_pos && idx < codepoints.size(); ++idx) {
    // Calculate UTF-8 byte length for this codepoint
    char32_t code = codepoints[idx];
    if (code < 0x80) {
      byte_pos += 1;
    } else if (code < 0x800) {
      byte_pos += 2;
    } else if (code < 0x10000) {
      byte_pos += 3;
    } else {
      byte_pos += 4;
    }
  }
  return byte_pos;
}

void Tokenizer::addDictionaryCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos) const {
  // Convert to byte position for dictionary lookup
  size_t byte_pos = charPosToBytePos(codepoints, start_pos);

  // Lookup in dictionary
  auto results = dict_manager_.lookup(text, byte_pos);

  for (const auto& result : results) {
    if (result.entry == nullptr) {
      continue;
    }

    // Calculate end position in characters
    size_t end_pos = start_pos + result.length;

    // Create edge
    uint8_t flags = core::LatticeEdge::kFromDictionary;
    if (result.entry->is_formal_noun) {
      flags |= core::LatticeEdge::kIsFormalNoun;
    }
    if (result.entry->is_low_info) {
      flags |= core::LatticeEdge::kIsLowInfo;
    }

    lattice.addEdge(result.entry->surface,
                    static_cast<uint32_t>(start_pos),
                    static_cast<uint32_t>(end_pos), result.entry->pos,
                    result.entry->cost, flags, result.entry->lemma,
                    result.entry->conj_type);
  }
}

void Tokenizer::addUnknownCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  // Check for dictionary entries at this position to penalize longer unknown
  // words. This prevents "明日雨" from being preferred over "明日" + "雨".
  size_t byte_pos = charPosToBytePos(codepoints, start_pos);
  auto dict_results = dict_manager_.lookup(text, byte_pos);

  size_t max_dict_length = 0;
  for (const auto& result : dict_results) {
    if (result.entry != nullptr) {
      max_dict_length = std::max(max_dict_length, result.length);
    }
  }

  // Generate unknown word candidates
  auto candidates =
      unknown_gen_.generate(text, codepoints, start_pos, char_types);

  for (const auto& candidate : candidates) {
    uint8_t flags = core::LatticeEdge::kIsUnknown;
    float adjusted_cost = candidate.cost;

    // Penalize unknown words that extend beyond dictionary entries.
    // When a dictionary entry exists (e.g., "明日"), unknown words that
    // include it but extend further (e.g., "明日雨") should be penalized
    // to prefer the dictionary-guided segmentation.
    //
    // The penalty must be large enough to overcome single_kanji_penalty (2.0)
    // applied to following unknown characters. For example:
    //   - "明日" (dict, 0.5) + "雨" (unk, 1.4 + single_kanji_penalty 2.0) = 3.9
    //   - "明日雨" (unk, 1.0 + penalty) must be > 3.9
    // So penalty needs to be at least 3.0 to make dict-guided splits preferred.
    if (max_dict_length > 0 &&
        candidate.end - candidate.start > max_dict_length) {
      // Apply strong penalty to prefer dictionary-guided segmentation
      adjusted_cost += 3.5F;
    }

    // For verb candidates, check if the hiragana suffix is a known particle
    // If so, the noun+particle path should be preferred over the verb path
    if (candidate.pos == core::PartOfSpeech::Verb &&
        candidate.end > candidate.start) {
      // Find the hiragana suffix portion
      size_t hiragana_start = candidate.start;
      while (hiragana_start < candidate.end &&
             hiragana_start < char_types.size() &&
             char_types[hiragana_start] != normalize::CharType::Hiragana) {
        ++hiragana_start;
      }

      if (hiragana_start < candidate.end) {
        // Look up the hiragana suffix in dictionary
        size_t suffix_byte_pos = charPosToBytePos(codepoints, hiragana_start);
        auto suffix_results = dict_manager_.lookup(text, suffix_byte_pos);

        for (const auto& result : suffix_results) {
          if (result.entry != nullptr &&
              result.entry->pos == core::PartOfSpeech::Particle) {
            // Check if the particle covers the full hiragana portion
            size_t suffix_len = candidate.end - hiragana_start;
            if (result.length == suffix_len) {
              // The hiragana suffix is a known particle - penalize verb path
              adjusted_cost += 1.5F;
              break;
            }
          }
        }
      }
    }

    // Need to store the surface string
    std::string surface_str(candidate.surface);

    lattice.addEdge(surface_str, static_cast<uint32_t>(candidate.start),
                    static_cast<uint32_t>(candidate.end), candidate.pos,
                    adjusted_cost, flags, "");
  }
}

namespace {

// Cost bonuses for mixed script joining (lower cost = preferred)
constexpr float kAlphaKanjiBonus = -0.3F;    // Web開発, AI研究
constexpr float kAlphaKatakanaBonus = -0.3F; // APIリクエスト
constexpr float kDigitKanjiBonus = -0.2F;    // 3月, 100人

// Maximum lengths for mixed script segments
constexpr size_t kMaxAlphaLen = 12;  // Reasonable limit for English words
constexpr size_t kMaxDigitLen = 8;   // Reasonable limit for numbers
constexpr size_t kMaxJapaneseLen = 8; // Reasonable limit for Japanese part

}  // namespace

void Tokenizer::addMixedScriptCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  using CharType = normalize::CharType;

  if (start_pos >= char_types.size()) {
    return;
  }

  CharType first_type = char_types[start_pos];

  // Only start from Alphabet or Digit
  if (first_type != CharType::Alphabet && first_type != CharType::Digit) {
    return;
  }

  // Find the end of the first segment (continuous same type)
  size_t first_end = start_pos + 1;
  size_t max_first_len =
      (first_type == CharType::Alphabet) ? kMaxAlphaLen : kMaxDigitLen;

  while (first_end < char_types.size() &&
         first_end - start_pos < max_first_len &&
         char_types[first_end] == first_type) {
    ++first_end;
  }

  // Check if there's a second segment to join with
  if (first_end >= char_types.size()) {
    return;
  }

  CharType second_type = char_types[first_end];

  // Determine if this is a valid mixed script pattern
  float bonus = 0.0F;
  if (first_type == CharType::Alphabet) {
    if (second_type == CharType::Kanji) {
      bonus = kAlphaKanjiBonus;
    } else if (second_type == CharType::Katakana) {
      bonus = kAlphaKatakanaBonus;
    } else {
      return;  // Not a valid pattern
    }
  } else if (first_type == CharType::Digit) {
    if (second_type == CharType::Kanji) {
      bonus = kDigitKanjiBonus;
    } else {
      return;  // Not a valid pattern
    }
  }

  // Find the end of the second segment
  size_t second_end = first_end + 1;
  while (second_end < char_types.size() &&
         second_end - first_end < kMaxJapaneseLen &&
         char_types[second_end] == second_type) {
    ++second_end;
  }

  // Generate merged candidate
  size_t start_byte = charPosToBytePos(codepoints, start_pos);
  size_t end_byte = charPosToBytePos(codepoints, second_end);

  std::string surface(text.substr(start_byte, end_byte - start_byte));

  // Base cost from POS prior, plus bonus for mixed script joining
  float base_cost = scorer_.posPrior(core::PartOfSpeech::Noun);
  float final_cost = base_cost + bonus;

  uint8_t flags = core::LatticeEdge::kIsUnknown;
  lattice.addEdge(surface, static_cast<uint32_t>(start_pos),
                  static_cast<uint32_t>(second_end), core::PartOfSpeech::Noun,
                  final_cost, flags, "");
}

namespace {

// Minimum kanji sequence length for compound splitting
constexpr size_t kMinCompoundLen = 4;

// Maximum kanji sequence length to consider
constexpr size_t kMaxCompoundLen = 10;

// Bonus for dictionary-guided splits (lower = preferred)
constexpr float kDictSplitBonus = -0.5F;

// Base cost for split candidates
constexpr float kSplitBaseCost = 1.0F;

}  // namespace

void Tokenizer::addCompoundSplitCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  using CharType = normalize::CharType;

  if (start_pos >= char_types.size()) {
    return;
  }

  // Only for kanji sequences
  if (char_types[start_pos] != CharType::Kanji) {
    return;
  }

  // Find the end of the kanji sequence
  size_t kanji_end = start_pos + 1;
  while (kanji_end < char_types.size() &&
         kanji_end - start_pos < kMaxCompoundLen &&
         char_types[kanji_end] == CharType::Kanji) {
    ++kanji_end;
  }

  size_t kanji_len = kanji_end - start_pos;

  // Only generate split candidates for 4+ character sequences
  if (kanji_len < kMinCompoundLen) {
    return;
  }

  // Get byte positions
  size_t start_byte = charPosToBytePos(codepoints, start_pos);

  // Try different split points
  // For "人工知能" (4 chars), try 2+2
  // For "人工知能研究所" (7 chars), try 2+5, 3+4, 4+3, etc.

  for (size_t split_point = 2; split_point < kanji_len; ++split_point) {
    size_t first_end = start_pos + split_point;
    size_t first_end_byte = charPosToBytePos(codepoints, first_end);

    // Check if the first part matches a dictionary entry
    auto first_results = dict_manager_.lookup(text, start_byte);
    bool first_in_dict = false;
    bool first_is_formal_noun = false;
    float first_cost = kSplitBaseCost;

    for (const auto& result : first_results) {
      if (result.entry != nullptr && result.length == split_point) {
        first_in_dict = true;
        first_cost = result.entry->cost + kDictSplitBonus;
        first_is_formal_noun = result.entry->is_formal_noun;
        break;
      }
    }

    // Check if the second part matches a dictionary entry
    auto second_results = dict_manager_.lookup(text, first_end_byte);
    bool second_in_dict = false;

    for (const auto& result : second_results) {
      if (result.entry != nullptr && result.length == kanji_len - split_point) {
        second_in_dict = true;
        break;
      }
    }

    // Only add split candidate if at least one part is in dictionary
    // This avoids creating many low-quality candidates
    if (first_in_dict || second_in_dict) {
      // Add the first part as a candidate
      std::string first_surface(text.substr(start_byte, first_end_byte - start_byte));
      uint8_t flags = first_in_dict ? core::LatticeEdge::kFromDictionary
                                     : core::LatticeEdge::kIsUnknown;
      // Preserve is_formal_noun flag to prevent postprocessor from merging
      if (first_is_formal_noun) {
        flags |= core::LatticeEdge::kIsFormalNoun;
      }

      lattice.addEdge(first_surface, static_cast<uint32_t>(start_pos),
                      static_cast<uint32_t>(first_end), core::PartOfSpeech::Noun,
                      first_cost, flags, "");

      // The second part will be handled when we process that position
      // But if both parts are in dictionary, give extra bonus to encourage this split
      if (first_in_dict && second_in_dict) {
        // Add another candidate with even lower cost to strongly prefer this split
        float bonus_cost = first_cost - 0.2F;
        lattice.addEdge(first_surface, static_cast<uint32_t>(start_pos),
                        static_cast<uint32_t>(first_end), core::PartOfSpeech::Noun,
                        bonus_cost, flags, "");
      }
    }
  }
}

namespace {

// Cost bonus for noun+verb splits (lower = preferred)
// This needs to be aggressive enough to compete with:
// 1. The unknown word generator which produces combined noun+verb tokens (~0.5)
// 2. The connection cost from Viterbi (NOUN→VERB = 0.5)
// For a split to win: noun_cost + verb_cost + conn_cost < unsplit_cost
// Example: X + 0.5 + 0.5 < 0.5 => X < -0.5
constexpr float kNounVerbSplitBonus = -1.0F;

// Additional bonus when verb base form is verified in dictionary
// This is the strongest signal that the split is correct
constexpr float kVerifiedVerbBonus = -0.8F;

// Maximum noun length (in characters) to consider for splitting
constexpr size_t kMaxNounLen = 6;

// Maximum hiragana length for verb suffix (to avoid matching too much)
// Typical verb endings: った(2), ます(2), ている(4), ていました(5)
constexpr size_t kMaxVerbHiraganaLen = 8;

}  // namespace

void Tokenizer::addNounVerbSplitCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  using CharType = normalize::CharType;

  if (start_pos >= char_types.size()) {
    return;
  }

  // Only for kanji-starting sequences
  if (char_types[start_pos] != CharType::Kanji) {
    return;
  }

  // Find the extent of kanji sequence
  size_t kanji_end = start_pos + 1;
  while (kanji_end < char_types.size() &&
         kanji_end - start_pos < kMaxNounLen + 3 &&  // Extra chars for verb stem
         char_types[kanji_end] == CharType::Kanji) {
    ++kanji_end;
  }

  // Need at least 2 kanji to consider noun+verb split
  if (kanji_end - start_pos < 2) {
    return;
  }

  // Check if hiragana follows (potential verb ending)
  if (kanji_end >= char_types.size() ||
      char_types[kanji_end] != CharType::Hiragana) {
    return;
  }

  // Find the maximum extent of hiragana sequence
  size_t max_hiragana_end = kanji_end;
  while (max_hiragana_end < char_types.size() &&
         max_hiragana_end - kanji_end < 10 &&
         char_types[max_hiragana_end] == CharType::Hiragana) {
    ++max_hiragana_end;
  }

  // Need at least 1 hiragana for verb ending
  if (max_hiragana_end <= kanji_end) {
    return;
  }

  // Use inflection analysis to check if verb part looks conjugated
  static grammar::Inflection inflection;  // Static to avoid repeated initialization

  size_t start_byte = charPosToBytePos(codepoints, start_pos);

  // Try different noun lengths (1 kanji, 2 kanji, etc.)
  for (size_t noun_len = 1; noun_len < kanji_end - start_pos; ++noun_len) {
    size_t verb_start = start_pos + noun_len;
    size_t verb_start_byte = charPosToBytePos(codepoints, verb_start);

    // Check if noun part is in dictionary
    auto noun_results = dict_manager_.lookup(text, start_byte);
    bool noun_in_dict = false;
    float noun_cost = 1.0F;

    for (const auto& result : noun_results) {
      if (result.entry != nullptr && result.length == noun_len) {
        noun_in_dict = true;
        noun_cost = result.entry->cost;
        break;
      }
    }

    // Try different hiragana lengths for verb ending
    // This allows matching "買った" even when followed by more hiragana like "ばかり"
    size_t hiragana_extent = max_hiragana_end - kanji_end;
    size_t max_try_len = std::min(hiragana_extent, kMaxVerbHiraganaLen);

    for (size_t hira_len = 1; hira_len <= max_try_len; ++hira_len) {
      size_t verb_end = kanji_end + hira_len;
      size_t verb_end_byte = charPosToBytePos(codepoints, verb_end);

      // Extract the potential verb part (kanji+hiragana after noun)
      std::string verb_part(text.substr(verb_start_byte, verb_end_byte - verb_start_byte));

      // Check if the verb part looks like a conjugated verb using inflection analysis
      bool looks_like_verb = false;
      auto candidates = inflection.analyze(verb_part);
      for (const auto& cand : candidates) {
        if (cand.confidence > 0.5F) {
          looks_like_verb = true;
          break;
        }
      }

      // Check if any candidate's base form is in dictionary
      bool base_in_dict = false;
      for (const auto& cand : candidates) {
        if (cand.confidence < 0.5F) {
          continue;  // Skip low confidence candidates
        }
        auto base_results = dict_manager_.lookup(cand.base_form, 0);
        for (const auto& result : base_results) {
          if (result.entry != nullptr &&
              result.entry->surface == cand.base_form &&
              result.entry->pos == core::PartOfSpeech::Verb) {
            base_in_dict = true;
            break;
          }
        }
        if (base_in_dict) {
          break;
        }
      }

      // Generate split candidates if either:
      // 1. Noun is in dictionary and verb looks conjugated
      // 2. Verb base form is in dictionary (strongest signal)
      if ((noun_in_dict && looks_like_verb) || base_in_dict) {
        // Add noun candidate with adjusted cost
        std::string noun_surface(text.substr(start_byte, verb_start_byte - start_byte));
        float final_noun_cost = noun_cost + kNounVerbSplitBonus;

        // Give extra bonus if verb base form is verified in dictionary
        // This is the strongest signal that the split is correct
        if (base_in_dict) {
          final_noun_cost += kVerifiedVerbBonus;
        }

        // Give additional bonus if noun is also in dictionary
        if (noun_in_dict && base_in_dict) {
          final_noun_cost -= 0.2F;  // Extra bonus for high-confidence splits
        }

        uint8_t noun_flags = noun_in_dict ? core::LatticeEdge::kFromDictionary
                                           : core::LatticeEdge::kIsUnknown;

        lattice.addEdge(noun_surface, static_cast<uint32_t>(start_pos),
                        static_cast<uint32_t>(verb_start), core::PartOfSpeech::Noun,
                        final_noun_cost, noun_flags, "");

        // Found a valid split for this noun_len, no need to try longer hiragana
        break;
      }
    }
  }
}

void Tokenizer::addCompoundVerbJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
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
  // For Ichidan verbs, the stem directly precedes V2 (e.g., 見 + つける)
  bool is_ichidan = (base_ending == 0);

  // Position after 連用形 (for Godan) or after stem (for Ichidan)
  size_t v2_start = is_ichidan ? kanji_end : kanji_end + 1;

  if (v2_start >= codepoints.size()) {
    return;
  }

  // Get byte positions
  size_t start_byte = charPosToBytePos(codepoints, start_pos);
  size_t v2_start_byte = charPosToBytePos(codepoints, v2_start);

  // Look for V2 (subsidiary verb) at this position
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

    // Found a matching V2! Now verify V1 is a valid verb

    // Build the V1 base form for verification
    std::string v1_base;
    size_t v1_end_byte = is_ichidan ? v2_start_byte : charPosToBytePos(codepoints, kanji_end);
    v1_base = std::string(text.substr(start_byte, v1_end_byte - start_byte));

    if (!is_ichidan) {
      // For Godan verbs, append the base ending
      v1_base += normalize::utf8::encode({base_ending});
    } else {
      // For Ichidan verbs, append る
      v1_base += "る";
    }

    // Check if V1 base form is in dictionary
    auto v1_results = dict_manager_.lookup(v1_base, 0);
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
    float base_cost = scorer_.posPrior(core::PartOfSpeech::Verb);
    float final_cost = base_cost + kCompoundVerbBonus;

    if (v1_in_dict) {
      final_cost += kVerifiedV1Bonus;
    }

    // Add compound verb candidate to lattice
    uint8_t flags = core::LatticeEdge::kFromDictionary;  // Treat as dictionary entry

    lattice.addEdge(compound_surface, static_cast<uint32_t>(start_pos),
                    static_cast<uint32_t>(compound_end_pos), core::PartOfSpeech::Verb,
                    final_cost, flags, v1_base);  // lemma = V1 base form

    // Only add one compound verb candidate per position
    // (prefer the first/longest match)
    return;
  }
}

namespace {

// Productive prefixes for prefix+noun joining
// These are common prefixes that productively combine with nouns
struct ProductivePrefix {
  char32_t codepoint;
  float bonus;       // Cost bonus (lower = more preferred)
  bool needs_kanji;  // True if the noun part should start with kanji
};

// List of productive prefixes
// Format: {codepoint, bonus, needs_kanji}
const ProductivePrefix kProductivePrefixes[] = {
    // Honorific prefixes (お, ご) - very common, productive patterns
    {U'お', -0.5F, true},   // お水, お金, お店
    {U'ご', -0.5F, true},   // ご確認, ご連絡, ご注意
    {U'御', -0.5F, true},   // 御 (formal version of お/ご)

    // Negation prefixes (不, 未, 非, 無) - common in formal/written Japanese
    {U'不', -0.4F, true},   // 不安, 不要, 不便, 不可能
    {U'未', -0.4F, true},   // 未経験, 未確認, 未知
    {U'非', -0.4F, true},   // 非常, 非公開
    {U'無', -0.4F, true},   // 無理, 無料, 無視

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

// Maximum noun length (in characters) to consider for joining
constexpr size_t kMaxNounLenForPrefix = 6;

// Additional bonus when noun part is in dictionary
constexpr float kVerifiedNounBonus = -0.3F;

}  // namespace

void Tokenizer::addPrefixNounJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
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
    return;  // Not a prefix
  }

  // Check if there's a noun part following
  size_t noun_start = start_pos + 1;
  if (noun_start >= codepoints.size()) {
    return;  // No noun part
  }

  // For most prefixes, the noun part should start with kanji
  if (matched_prefix->needs_kanji) {
    if (char_types[noun_start] != CharType::Kanji) {
      return;  // Not followed by kanji
    }
  } else {
    // Allow kanji or katakana
    if (char_types[noun_start] != CharType::Kanji &&
        char_types[noun_start] != CharType::Katakana) {
      return;
    }
  }

  // Find the end of the noun part
  // For kanji, extend until non-kanji
  // For katakana, extend until non-katakana
  CharType noun_type = char_types[noun_start];
  size_t noun_end = noun_start + 1;

  while (noun_end < codepoints.size() &&
         noun_end - noun_start < kMaxNounLenForPrefix &&
         char_types[noun_end] == noun_type) {
    ++noun_end;
  }

  // Need at least 1 character in the noun part
  if (noun_end <= noun_start) {
    return;
  }

  // Check dictionary for compound nouns that may include hiragana
  // E.g., 買い物 (kanji + hiragana + kanji) should be found as a single noun
  size_t noun_start_byte = charPosToBytePos(codepoints, noun_start);
  auto noun_results = dict_manager_.lookup(text, noun_start_byte);
  bool noun_in_dict = false;
  size_t dict_noun_end = noun_end;  // May be extended by dictionary lookup

  for (const auto& result : noun_results) {
    if (result.entry != nullptr &&
        result.entry->pos == core::PartOfSpeech::Noun) {
      // Found a noun entry - check if it extends our noun_end
      if (result.length > dict_noun_end - noun_start) {
        dict_noun_end = noun_start + result.length;
        noun_in_dict = true;
      } else if (result.length == noun_end - noun_start) {
        noun_in_dict = true;
      }
    }
  }

  // Use the dictionary-extended noun end if available
  if (dict_noun_end > noun_end) {
    noun_end = dict_noun_end;
  } else {
    // Skip single-kanji noun when followed by hiragana (and not in dictionary)
    // This is likely a verb stem: お + 伝(kanji) + え(hiragana) = お伝え (verb renyokei)
    // Common patterns: お伝え, お待ち, お送り, お知らせ (honorific verb forms)
    if (noun_end - noun_start == 1 && noun_end < codepoints.size()) {
      if (char_types[noun_end] == CharType::Hiragana) {
        return;  // Skip - this is likely a verb pattern
      }
    }
  }

  // Also check if the combined form is already in dictionary
  // If so, let the dictionary candidate handle it
  size_t start_byte = charPosToBytePos(codepoints, start_pos);
  auto combined_results = dict_manager_.lookup(text, start_byte);

  for (const auto& result : combined_results) {
    if (result.entry != nullptr &&
        result.length == noun_end - start_pos) {
      // Already in dictionary, no need to add join candidate
      return;
    }
  }

  // Generate joined candidate
  size_t end_byte = charPosToBytePos(codepoints, noun_end);
  std::string surface(text.substr(start_byte, end_byte - start_byte));

  // Calculate cost
  float base_cost = scorer_.posPrior(core::PartOfSpeech::Noun);
  float final_cost = base_cost + matched_prefix->bonus;

  // Give extra bonus if noun part is in dictionary
  if (noun_in_dict) {
    final_cost += kVerifiedNounBonus;
  }

  // Add the joined candidate
  uint8_t flags = core::LatticeEdge::kIsUnknown;

  lattice.addEdge(surface, static_cast<uint32_t>(start_pos),
                  static_cast<uint32_t>(noun_end), core::PartOfSpeech::Noun,
                  final_cost, flags, "");
}

namespace {

// Te-form auxiliary verb patterns
// These are common auxiliary verbs that follow て-form
// Format: {hiragana stem, base form for lemma}
struct TeFormAuxiliary {
  const char* stem;      // Stem that appears after て/で (e.g., "い" for いく)
  const char* base_form; // Base form of the auxiliary (e.g., "いく")
};

// List of te-form auxiliary verbs
// These verbs commonly follow て-form to express various aspects
// Note: "ある" is excluded because で+ある is the copula である, not te-form+aux
const TeFormAuxiliary kTeFormAuxiliaries[] = {
    {"い", "いく"},      // 〜ていく (progressive movement away)
    {"く", "くる"},      // 〜てくる (progressive movement toward)
    {"み", "みる"},      // 〜てみる (try doing)
    {"お", "おく"},      // 〜ておく (do in advance)
    {"しま", "しまう"},  // 〜てしまう (complete/regrettably)
    {"ちゃ", "しまう"},  // 〜ちゃう (colloquial しまう)
    {"じゃ", "しまう"},  // 〜じゃう (colloquial しまう after で)
    {"もら", "もらう"},  // 〜てもらう (receive favor)
    {"くれ", "くれる"},  // 〜てくれる (give favor to me)
    {"あげ", "あげる"},  // 〜てあげる (give favor to other)
    {"や", "やる"},      // 〜てやる (do for someone, casual)
};

// Cost bonus for te-form + auxiliary pattern (lower = preferred)
constexpr float kTeFormAuxBonus = -0.8F;

}  // namespace

void Tokenizer::addTeFormAuxiliaryCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  using CharType = normalize::CharType;

  if (start_pos >= codepoints.size()) {
    return;
  }

  // Look for て or で at this position
  // て: godan verbs (書いて, 読んで) or ichidan verbs (食べて)
  // で: godan verbs with certain endings (読んで, 飲んで)
  char32_t current = codepoints[start_pos];
  if (current != U'て' && current != U'で') {
    return;
  }

  // Check if there's hiragana following (potential auxiliary verb)
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

  // Use inflection analysis to check potential auxiliary verbs
  static grammar::Inflection inflection;

  // Try each auxiliary pattern
  for (const auto& aux : kTeFormAuxiliaries) {
    std::string_view stem(aux.stem);

    // Check if the text after て/で starts with this stem
    std::string_view text_after_te = text.substr(aux_start_byte);
    if (text_after_te.size() < stem.size()) {
      continue;
    }

    // Check if text starts with the auxiliary stem
    if (text_after_te.substr(0, stem.size()) != stem) {
      continue;
    }

    // Found a potential auxiliary! Now try to find the full conjugated form
    // For example, for いく: い, いき, いく, いった, いきたい, etc.
    size_t stem_char_len = normalize::utf8::decode(std::string(stem)).size();

    // Try different lengths after the stem to find valid conjugations
    for (size_t aux_end = aux_start + stem_char_len;
         aux_end <= hiragana_end && aux_end <= aux_start + 8; ++aux_end) {
      size_t aux_end_byte = charPosToBytePos(codepoints, aux_end);
      std::string aux_surface(text.substr(aux_start_byte,
                                          aux_end_byte - aux_start_byte));

      // Check if this looks like a conjugated form of the auxiliary
      auto best = inflection.getBest(aux_surface);
      if (best.confidence > 0.4F && best.base_form == aux.base_form) {
        // Found a valid auxiliary verb conjugation!
        // Add the て/で particle as a candidate that ends before the auxiliary
        // This allows Viterbi to choose: [verb-te] + [auxiliary]

        // First, verify there's a verb ending in て/で before this position
        // by checking if the previous lattice has a verb candidate ending here
        // (This is implicit in Viterbi - we just need to provide the split point)

        // Add a low-cost edge for the て/で + auxiliary combination
        // to encourage splitting at て/で boundary
        size_t combo_end_byte = aux_end_byte;
        std::string combo_surface(text.substr(te_byte, combo_end_byte - te_byte));

        // Calculate cost - give strong bonus to encourage this split
        float final_cost = scorer_.posPrior(core::PartOfSpeech::Verb) +
                           kTeFormAuxBonus;

        uint8_t flags = core::LatticeEdge::kIsUnknown;

        // Add the auxiliary verb part starting from て/で
        lattice.addEdge(combo_surface, static_cast<uint32_t>(start_pos),
                        static_cast<uint32_t>(aux_end), core::PartOfSpeech::Verb,
                        final_cost, flags, aux.base_form);

        // Found a valid pattern, no need to try longer forms
        break;
      }
    }
  }
}

}  // namespace suzume::analysis
