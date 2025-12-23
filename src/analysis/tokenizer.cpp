#include "analysis/tokenizer.h"

#include "grammar/inflection.h"
#include "normalize/utf8.h"

namespace suzume::analysis {

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
                    result.entry->cost, flags, result.entry->lemma);
  }
}

void Tokenizer::addUnknownCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  // Generate unknown word candidates
  auto candidates =
      unknown_gen_.generate(text, codepoints, start_pos, char_types);

  for (const auto& candidate : candidates) {
    uint8_t flags = core::LatticeEdge::kIsUnknown;

    // Need to store the surface string
    std::string surface_str(candidate.surface);

    lattice.addEdge(surface_str, static_cast<uint32_t>(candidate.start),
                    static_cast<uint32_t>(candidate.end), candidate.pos,
                    candidate.cost, flags, "");
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
    float first_cost = kSplitBaseCost;

    for (const auto& result : first_results) {
      if (result.entry != nullptr && result.length == split_point) {
        first_in_dict = true;
        first_cost = result.entry->cost + kDictSplitBonus;
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

  // Find the extent of hiragana sequence
  size_t hiragana_end = kanji_end;
  while (hiragana_end < char_types.size() &&
         hiragana_end - kanji_end < 10 &&
         char_types[hiragana_end] == CharType::Hiragana) {
    ++hiragana_end;
  }

  // Need at least 1 hiragana for verb ending
  if (hiragana_end <= kanji_end) {
    return;
  }

  // Use inflection analysis to check if verb part looks conjugated
  static grammar::Inflection inflection;  // Static to avoid repeated initialization

  size_t start_byte = charPosToBytePos(codepoints, start_pos);

  // Try different noun lengths (1 kanji, 2 kanji, etc.)
  for (size_t noun_len = 1; noun_len < kanji_end - start_pos; ++noun_len) {
    size_t verb_start = start_pos + noun_len;
    size_t verb_start_byte = charPosToBytePos(codepoints, verb_start);
    size_t verb_end_byte = charPosToBytePos(codepoints, hiragana_end);

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
    }
  }
}

}  // namespace suzume::analysis
