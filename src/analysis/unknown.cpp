#include "analysis/unknown.h"

#include <algorithm>

#include "normalize/utf8.h"

namespace suzume::analysis {

namespace {

// Suffix list for kanji compounds
struct SuffixEntry {
  std::string_view suffix;
  core::PartOfSpeech pos;
};

// Na-adjective forming suffixes (〜的 patterns)
// These create な-adjectives that can take な (attributive) or に (adverbial)
const std::vector<std::string_view> kNaAdjSuffixes = {
    "的",  // 理性的, 論理的, etc.
};

const std::vector<SuffixEntry> kSuffixes = {
    {"化する", core::PartOfSpeech::Verb},
    {"化", core::PartOfSpeech::Other},   {"性", core::PartOfSpeech::Other},
    {"率", core::PartOfSpeech::Other},   {"法", core::PartOfSpeech::Other},
    {"論", core::PartOfSpeech::Other},   {"者", core::PartOfSpeech::Other},
    {"家", core::PartOfSpeech::Other},   {"員", core::PartOfSpeech::Other},
    {"式", core::PartOfSpeech::Other},   {"感", core::PartOfSpeech::Other},
    {"力", core::PartOfSpeech::Other},   {"度", core::PartOfSpeech::Other},
};

// Extract substring from codepoints to UTF-8
std::string extractSubstring(const std::vector<char32_t>& codepoints,
                             size_t start, size_t end) {
  if (start >= codepoints.size() || end > codepoints.size() || start >= end) {
    return "";
  }
  std::vector<char32_t> sub(codepoints.begin() + start,
                            codepoints.begin() + end);
  return normalize::utf8::encode(sub);
}

}  // namespace

UnknownWordGenerator::UnknownWordGenerator(
    const UnknownOptions& options,
    const dictionary::DictionaryManager* dict_manager)
    : options_(options), dict_manager_(dict_manager) {}

size_t UnknownWordGenerator::getMaxLength(normalize::CharType ctype) const {
  switch (ctype) {
    case normalize::CharType::Kanji:
      return options_.max_kanji_length;
    case normalize::CharType::Katakana:
      return options_.max_katakana_length;
    case normalize::CharType::Hiragana:
      return options_.max_hiragana_length;
    case normalize::CharType::Alphabet:
      return options_.max_alphabet_length;
    case normalize::CharType::Digit:
      return options_.max_alphanumeric_length;
    default:
      return options_.max_unknown_length;
  }
}

core::PartOfSpeech UnknownWordGenerator::getPosForType(normalize::CharType ctype) {
  switch (ctype) {
    case normalize::CharType::Kanji:
    case normalize::CharType::Katakana:
    case normalize::CharType::Alphabet:
    case normalize::CharType::Digit:
      return core::PartOfSpeech::Noun;
    case normalize::CharType::Hiragana:
      return core::PartOfSpeech::Other;
    default:
      return core::PartOfSpeech::Symbol;
  }
}

float UnknownWordGenerator::getCostForType(normalize::CharType ctype, size_t length) {
  float base_cost = 1.0F;

  switch (ctype) {
    case normalize::CharType::Kanji:
      // Kanji: prefer 2-4 characters, but single kanji is also common (家, 人, 本)
      // Single kanji should beat suffix entries (cost 1.5) in dictionary
      if (length >= 2 && length <= 4) {
        return base_cost;
      }
      if (length == 1) {
        return base_cost + 0.4F;  // 1.4: prefer over suffix entries (1.5)
      }
      return base_cost + 0.5F;

    case normalize::CharType::Katakana:
      // Katakana: prefer 3-8 characters
      if (length >= 3 && length <= 8) {
        return base_cost;
      }
      return base_cost + 0.3F;

    case normalize::CharType::Alphabet:
      // Alphabet: prefer longer sequences for identifiers/words
      // Longer sequences (like "getUserData") should not be penalized
      if (length >= 2 && length <= 20) {
        // Give bonus to longer sequences to prefer them over splits
        // This helps keep "getUserData" together vs "getUser" + "Data"
        float length_bonus = (length >= 8) ? -0.3F : 0.0F;
        return base_cost + 0.2F + length_bonus;
      }
      return base_cost + 0.5F;

    case normalize::CharType::Digit:
      // Digits: always reasonable
      return base_cost - 0.2F;

    case normalize::CharType::Hiragana:
      // Hiragana only: usually function words
      return base_cost + 1.0F;

    default:
      return base_cost + 1.5F;
  }
}

std::vector<UnknownCandidate> UnknownWordGenerator::generate(
    std::string_view text, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size()) {
    return candidates;
  }

  // Generate verb candidates (kanji + hiragana conjugation endings)
  if (char_types[start_pos] == normalize::CharType::Kanji) {
    auto verbs = generateVerbCandidates(text, codepoints, start_pos, char_types);
    candidates.insert(candidates.end(), verbs.begin(), verbs.end());

    // Generate compound verb candidates (kanji + hiragana + kanji + hiragana)
    // e.g., 恐れ入ります, 差し上げます, 申し上げます
    auto compound_verbs =
        generateCompoundVerbCandidates(text, codepoints, start_pos, char_types);
    candidates.insert(candidates.end(), compound_verbs.begin(),
                      compound_verbs.end());

    // Generate i-adjective candidates (kanji + hiragana conjugation endings)
    auto adjs = generateAdjectiveCandidates(text, codepoints, start_pos, char_types);
    candidates.insert(candidates.end(), adjs.begin(), adjs.end());

    // Generate na-adjective candidates (〜的 patterns)
    auto na_adjs =
        generateNaAdjectiveCandidates(text, codepoints, start_pos, char_types);
    candidates.insert(candidates.end(), na_adjs.begin(), na_adjs.end());

    // Generate nominalized noun candidates (kanji + short hiragana)
    // e.g., 手助け, 片付け, 引き上げ
    auto nom_nouns =
        generateNominalizedNounCandidates(text, codepoints, start_pos, char_types);
    candidates.insert(candidates.end(), nom_nouns.begin(), nom_nouns.end());
  }

  // Generate hiragana verb candidates (pure hiragana verbs like いく, くる)
  if (char_types[start_pos] == normalize::CharType::Hiragana) {
    auto hiragana_verbs =
        generateHiraganaVerbCandidates(text, codepoints, start_pos, char_types);
    candidates.insert(candidates.end(), hiragana_verbs.begin(),
                      hiragana_verbs.end());

    // Generate hiragana i-adjective candidates (まずい, おいしい, etc.)
    auto hiragana_adjs =
        generateHiraganaAdjectiveCandidates(text, codepoints, start_pos, char_types);
    candidates.insert(candidates.end(), hiragana_adjs.begin(),
                      hiragana_adjs.end());
  }

  // Generate by same type
  auto same_type = generateBySameType(text, codepoints, start_pos, char_types);
  candidates.insert(candidates.end(), same_type.begin(), same_type.end());

  // Generate alphanumeric sequences
  auto alphanum = generateAlphanumeric(text, codepoints, start_pos, char_types);
  candidates.insert(candidates.end(), alphanum.begin(), alphanum.end());

  // Generate with suffix separation for kanji
  if (options_.separate_suffix &&
      char_types[start_pos] == normalize::CharType::Kanji) {
    auto suffix = generateWithSuffix(text, codepoints, start_pos, char_types);
    candidates.insert(candidates.end(), suffix.begin(), suffix.end());
  }

  return candidates;
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateBySameType(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size()) {
    return candidates;
  }

  normalize::CharType start_type = char_types[start_pos];

  // For hiragana starting with single-character particles that are NEVER verb stems,
  // don't generate unknown candidates. These should be recognized by dictionary lookup.
  // Note: Exclude characters that CAN be verb stems:
  //   - な→なる/なくす
  //   - て→できる
  //   - や→やる (important: must NOT skip や)
  if (start_type == normalize::CharType::Hiragana) {
    char32_t first_char = codepoints[start_pos];
    if (first_char == U'を' || first_char == U'が' || first_char == U'は' ||
        first_char == U'に' || first_char == U'へ' || first_char == U'の' ||
        first_char == U'か' || first_char == U'ね' ||
        first_char == U'よ' || first_char == U'わ') {
      return candidates;  // Let dictionary handle these
    }

    // Skip if starting with demonstrative pronouns (これ, それ, あれ, どれ, etc.)
    // These should be recognized by dictionary lookup, not generated as unknown words.
    if (start_pos + 1 < codepoints.size()) {
      char32_t second_char = codepoints[start_pos + 1];
      // Check for こ/そ/あ/ど + れ/こ/ち patterns (demonstrative pronouns)
      if ((first_char == U'こ' || first_char == U'そ' ||
           first_char == U'あ' || first_char == U'ど') &&
          (second_char == U'れ' || second_char == U'こ' || second_char == U'ち')) {
        return candidates;
      }
    }
  }

  size_t max_len = getMaxLength(start_type);

  // Find end of same-type sequence
  size_t end_pos = start_pos + 1;
  while (end_pos < char_types.size() && end_pos - start_pos < max_len &&
         char_types[end_pos] == start_type) {
    // For hiragana, break at common particle characters to avoid
    // swallowing particles into unknown words (e.g., don't create "ぎをみじん")
    if (start_type == normalize::CharType::Hiragana) {
      char32_t curr_char = codepoints[end_pos];
      // Note: Don't include「や」as it's also the stem of「やる」verb
      if (curr_char == U'を' || curr_char == U'が' || curr_char == U'は' ||
          curr_char == U'に' || curr_char == U'へ' || curr_char == U'の' ||
          curr_char == U'で' || curr_char == U'と' || curr_char == U'も' ||
          curr_char == U'か') {
        break;  // Stop before the particle
      }
    }
    ++end_pos;
  }

  // Generate candidates for different lengths
  for (size_t len = 1; len <= end_pos - start_pos; ++len) {
    size_t candidate_end = start_pos + len;
    std::string surface = extractSubstring(codepoints, start_pos, candidate_end);

    if (!surface.empty()) {
      UnknownCandidate candidate;
      candidate.surface = surface;  // Note: This is temporary
      candidate.start = start_pos;
      candidate.end = candidate_end;
      candidate.pos = getPosForType(start_type);
      candidate.cost = getCostForType(start_type, len);
      candidate.has_suffix = false;

      // Store as string and update surface
      candidates.push_back(candidate);
    }
  }

  return candidates;
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateAlphanumeric(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size()) {
    return candidates;
  }

  normalize::CharType start_type = char_types[start_pos];

  // Only for alphabet or digit start
  if (start_type != normalize::CharType::Alphabet &&
      start_type != normalize::CharType::Digit) {
    return candidates;
  }

  // Find mixed alphanumeric sequence
  size_t end_pos = start_pos;
  bool has_alpha = false;
  bool has_digit = false;

  while (end_pos < char_types.size() &&
         end_pos - start_pos < options_.max_alphanumeric_length) {
    normalize::CharType ctype = char_types[end_pos];
    if (ctype == normalize::CharType::Alphabet) {
      has_alpha = true;
      ++end_pos;
    } else if (ctype == normalize::CharType::Digit) {
      has_digit = true;
      ++end_pos;
    } else {
      break;
    }
  }

  // Only add if mixed (pure sequences handled by generateBySameType)
  if (has_alpha && has_digit && end_pos > start_pos + 1) {
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);

    if (!surface.empty()) {
      UnknownCandidate candidate;
      candidate.surface = surface;
      candidate.start = start_pos;
      candidate.end = end_pos;
      candidate.pos = core::PartOfSpeech::Noun;
      candidate.cost = 0.8F;
      candidate.has_suffix = false;
      candidates.push_back(candidate);
    }
  }

  return candidates;
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateWithSuffix(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Kanji) {
    return candidates;
  }

  // Find kanji sequence
  size_t end_pos = start_pos;
  while (end_pos < char_types.size() &&
         end_pos - start_pos < options_.max_kanji_length &&
         char_types[end_pos] == normalize::CharType::Kanji) {
    ++end_pos;
  }

  if (end_pos <= start_pos + 1) {
    return candidates;
  }

  std::string kanji_seq = extractSubstring(codepoints, start_pos, end_pos);

  // Check for suffixes
  for (const auto& [suffix, suffix_pos] : kSuffixes) {
    if (kanji_seq.size() > suffix.size() &&
        kanji_seq.compare(kanji_seq.size() - suffix.size(), suffix.size(),
                          suffix) == 0) {
      // Calculate stem length in codepoints
      auto suffix_codepoints = normalize::utf8::decode(std::string(suffix));
      size_t stem_end = end_pos - suffix_codepoints.size();

      if (stem_end > start_pos + 1) {
        // Add stem candidate
        std::string stem_surface =
            extractSubstring(codepoints, start_pos, stem_end);

        UnknownCandidate stem;
        stem.surface = stem_surface;
        stem.start = start_pos;
        stem.end = stem_end;
        stem.pos = core::PartOfSpeech::Noun;
        stem.cost = 1.0F + options_.suffix_separation_bonus;
        stem.has_suffix = false;
        candidates.push_back(stem);

        // Add whole word candidate too
        UnknownCandidate whole;
        whole.surface = kanji_seq;
        whole.start = start_pos;
        whole.end = end_pos;
        whole.pos = core::PartOfSpeech::Noun;
        whole.cost = 1.2F;
        whole.has_suffix = true;
        candidates.push_back(whole);

        break;  // Use longest matching suffix
      }
    }
  }

  return candidates;
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateCompoundVerbCandidates(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  std::vector<UnknownCandidate> candidates;

  // Requires dictionary to verify base forms
  if (dict_manager_ == nullptr) {
    return candidates;
  }

  // Pattern: Kanji+ Hiragana(1-3) Kanji+ Hiragana+
  // e.g., 恐(K)れ(H)入(K)ります(H), 差(K)し(H)上(K)げます(H)
  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Kanji) {
    return candidates;
  }

  // Find first kanji portion (1-2 chars)
  size_t kanji1_end = start_pos;
  while (kanji1_end < char_types.size() &&
         kanji1_end - start_pos < 3 &&
         char_types[kanji1_end] == normalize::CharType::Kanji) {
    ++kanji1_end;
  }

  if (kanji1_end == start_pos || kanji1_end >= char_types.size()) {
    return candidates;
  }

  // Find first hiragana portion (1-3 chars, typically verb renyoukei ending)
  if (char_types[kanji1_end] != normalize::CharType::Hiragana) {
    return candidates;
  }

  size_t hira1_end = kanji1_end;
  while (hira1_end < char_types.size() &&
         hira1_end - kanji1_end < 4 &&
         char_types[hira1_end] == normalize::CharType::Hiragana) {
    ++hira1_end;
  }

  // Find second kanji portion (must exist for compound verb)
  if (hira1_end >= char_types.size() ||
      char_types[hira1_end] != normalize::CharType::Kanji) {
    return candidates;
  }

  size_t kanji2_end = hira1_end;
  while (kanji2_end < char_types.size() &&
         kanji2_end - hira1_end < 3 &&
         char_types[kanji2_end] == normalize::CharType::Kanji) {
    ++kanji2_end;
  }

  // Find second hiragana portion (conjugation ending)
  if (kanji2_end >= char_types.size() ||
      char_types[kanji2_end] != normalize::CharType::Hiragana) {
    return candidates;
  }

  size_t hira2_end = kanji2_end;
  while (hira2_end < char_types.size() &&
         hira2_end - kanji2_end < 10 &&
         char_types[hira2_end] == normalize::CharType::Hiragana) {
    ++hira2_end;
  }

  // Try different ending lengths
  for (size_t end_pos = hira2_end; end_pos > kanji2_end; --end_pos) {
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);
    if (surface.empty()) {
      continue;
    }

    // Use inflection analyzer to get potential base forms
    auto inflection_candidates = inflection_.analyze(surface);

    for (const auto& infl_cand : inflection_candidates) {
      if (infl_cand.confidence < 0.4F) {
        continue;
      }

      // Check if base form exists in dictionary as a verb
      auto results = dict_manager_->lookup(infl_cand.base_form, 0);
      for (const auto& result : results) {
        if (result.entry == nullptr) {
          continue;
        }
        if (result.entry->surface != infl_cand.base_form) {
          continue;
        }
        if (result.entry->pos != core::PartOfSpeech::Verb) {
          continue;
        }

        // Verify conjugation type matches
        auto dict_verb_type =
            grammar::conjTypeToVerbType(result.entry->conj_type);
        if (dict_verb_type == infl_cand.verb_type) {
          // Found a match! Generate candidate
          UnknownCandidate cand;
          cand.surface = surface;
          cand.start = start_pos;
          cand.end = end_pos;
          cand.pos = core::PartOfSpeech::Verb;
          // Low cost to prefer dictionary-verified compound verbs
          cand.cost = 0.3F;
          cand.has_suffix = false;
          candidates.push_back(cand);
          return candidates;  // Return first valid match
        }
      }
    }
  }

  return candidates;
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateVerbCandidates(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Kanji) {
    return candidates;
  }

  // Find kanji portion (typically 1-2 characters for verbs)
  size_t kanji_end = start_pos;
  while (kanji_end < char_types.size() &&
         kanji_end - start_pos < 3 &&  // Max 3 kanji for verb stem
         char_types[kanji_end] == normalize::CharType::Kanji) {
    ++kanji_end;
  }

  if (kanji_end == start_pos) {
    return candidates;
  }

  // Look for hiragana after kanji
  if (kanji_end >= char_types.size() ||
      char_types[kanji_end] != normalize::CharType::Hiragana) {
    return candidates;
  }

  // Check if first hiragana is a particle that can NEVER be part of a verb
  // E.g., "領収書を" - を is a particle, not part of a verb
  // Note about が and に:
  // - が can be part of verbs: 上がる, 下がる, 受かる, etc.
  // - に can be part of verbs: 煮る (niru), etc.
  // However, these verbs should be in the dictionary, not generated as unknown.
  // Including が/に here prevents false positives like 金がない→金ぐ, 家にいます→家にう
  // Note about か:
  // - か can be part of verbs: 書かない (negative), 動かす, etc.
  // - These are valid conjugation patterns that should be generated
  char32_t first_hiragana = codepoints[kanji_end];
  if (first_hiragana == U'を' || first_hiragana == U'が' ||
      first_hiragana == U'は' || first_hiragana == U'も' ||
      first_hiragana == U'へ' || first_hiragana == U'の' ||
      first_hiragana == U'に' || first_hiragana == U'や') {
    return candidates;  // Not a verb - these particles follow nouns
  }

  size_t hiragana_end = kanji_end;
  while (hiragana_end < char_types.size() &&
         hiragana_end - kanji_end < 12 &&  // Max 12 hiragana for conjugation + aux
         char_types[hiragana_end] == normalize::CharType::Hiragana) {
    // Note: We no longer break at particle-like characters here.
    // The inflection module will determine if the full string is a valid
    // conjugated verb. This allows patterns like "飲みながら" (nomi-nagara)
    // where "が" is part of the auxiliary "ながら", not a standalone particle.
    ++hiragana_end;
  }

  // Need at least some hiragana for a conjugated verb
  if (hiragana_end <= kanji_end) {
    return candidates;
  }

  // Try different stem lengths (kanji only, or kanji + 1 hiragana for ichidan)
  // This handles both godan (kanji stem) and ichidan (kanji + hiragana stem)
  for (size_t stem_end = kanji_end; stem_end <= kanji_end + 1 && stem_end < hiragana_end; ++stem_end) {
    // Try different ending lengths, starting from longest
    for (size_t end_pos = hiragana_end; end_pos > stem_end; --end_pos) {
      std::string surface = extractSubstring(codepoints, start_pos, end_pos);

      if (surface.empty()) {
        continue;
      }

      // Check for particle/copula patterns that should NOT be treated as verbs
      // Kanji + particle or copula (で, に, を, が, は, も, へ, と, や, か, の, etc.)
      std::string hiragana_part = extractSubstring(codepoints, kanji_end, end_pos);
      if (hiragana_part == "で" || hiragana_part == "に" || hiragana_part == "を" ||
          hiragana_part == "が" || hiragana_part == "は" || hiragana_part == "も" ||
          hiragana_part == "へ" || hiragana_part == "と" || hiragana_part == "や" ||
          hiragana_part == "か" || hiragana_part == "の" || hiragana_part == "から" ||
          hiragana_part == "まで" || hiragana_part == "より" || hiragana_part == "ほど" ||
          hiragana_part == "です" || hiragana_part == "だ" || hiragana_part == "だった" ||
          hiragana_part == "でした" || hiragana_part == "でし" || hiragana_part == "である") {
        continue;  // Skip particle/copula patterns
      }

      // Skip patterns where hiragana part is a known suffix in dictionary
      // (e.g., たち, さん, ら, etc.) - let NOUN+suffix split win instead
      // Only skip if kanji is 2+ characters, as single kanji + suffix
      // could be a valid verb stem (立ち → 立つ)
      // Note: Only skip for OTHER (suffixes), not VERB (する is a verb, not suffix)
      bool is_suffix_pattern = false;
      if (kanji_end - start_pos >= 2 && dict_manager_ != nullptr) {
        auto suffix_results = dict_manager_->lookup(hiragana_part, 0);
        for (const auto& result : suffix_results) {
          if (result.entry != nullptr &&
              result.entry->surface == hiragana_part &&
              result.entry->is_low_info &&
              result.entry->pos == core::PartOfSpeech::Other) {
            // This hiragana part is a registered suffix - skip verb candidate
            is_suffix_pattern = true;
            break;
          }
        }
      }
      if (is_suffix_pattern) {
        continue;
      }

      // Skip patterns that end with particles (noun renyokei + particle)
      // e.g., 切りに (切り + に), 飲みに (飲み + に), 行きに (行き + に)
      // These are nominalized verb stems followed by particles, not verb forms
      size_t hp_size = hiragana_part.size();
      if (hp_size >= 6) {  // At least 2 hiragana (6 bytes)
        // Get last hiragana character (particle candidate)
        char32_t last_char = codepoints[end_pos - 1];
        if (last_char == U'に' || last_char == U'で' || last_char == U'を' ||
            last_char == U'が' || last_char == U'は' || last_char == U'も' ||
            last_char == U'へ' || last_char == U'の' || last_char == U'と') {
          // Check if the preceding part could be a valid verb renyokei
          // Renyokei typically ends in い/り/き/ぎ/し/み/び/ち/に
          char32_t second_last_char = codepoints[end_pos - 2];
          if (second_last_char == U'い' || second_last_char == U'り' ||
              second_last_char == U'き' || second_last_char == U'ぎ' ||
              second_last_char == U'し' || second_last_char == U'み' ||
              second_last_char == U'び' || second_last_char == U'ち') {
            continue;  // Skip - likely nominalized noun + particle
          }
        }
      }

      // Check if this looks like a conjugated verb
      auto best = inflection_.getBest(surface);
      // Only accept verb types (not IAdjective) and require confidence > 0.48
      if (best.confidence > 0.48F &&
          best.verb_type != grammar::VerbType::IAdjective) {
        // Verify the stem matches
        std::string expected_stem = extractSubstring(codepoints, start_pos, stem_end);
        if (best.stem == expected_stem) {
          // Reject Godan verbs with stems ending in e-row hiragana
          // E-row endings (え,け,せ,て,ね,へ,め,れ) are typically ichidan stems
          // E.g., "伝えいた" falsely matches as GodanKa "伝えく" but 伝える is ichidan
          bool is_godan = (best.verb_type == grammar::VerbType::GodanKa ||
                           best.verb_type == grammar::VerbType::GodanGa ||
                           best.verb_type == grammar::VerbType::GodanSa ||
                           best.verb_type == grammar::VerbType::GodanTa ||
                           best.verb_type == grammar::VerbType::GodanNa ||
                           best.verb_type == grammar::VerbType::GodanBa ||
                           best.verb_type == grammar::VerbType::GodanMa ||
                           best.verb_type == grammar::VerbType::GodanRa ||
                           best.verb_type == grammar::VerbType::GodanWa);
          if (is_godan && stem_end > kanji_end && stem_end <= codepoints.size()) {
            // Check if the last character of the stem is e-row hiragana
            char32_t last_char = codepoints[stem_end - 1];
            if (last_char == U'え' || last_char == U'け' || last_char == U'せ' ||
                last_char == U'て' || last_char == U'ね' || last_char == U'へ' ||
                last_char == U'め' || last_char == U'れ') {
              continue;  // Skip - e-row stem is typically ichidan, not godan
            }
          }

          // Skip Suru verb renyokei (し) if followed by te/ta form particles
          // e.g., "勉強して" should be parsed as single token, not "勉強し" + "て"
          if (best.verb_type == grammar::VerbType::Suru &&
              hiragana_part == "し" && end_pos < codepoints.size()) {
            char32_t next_char = codepoints[end_pos];
            if (next_char == U'て' || next_char == U'た' ||
                next_char == U'で' || next_char == U'だ') {
              continue;  // Skip - let the longer te-form candidate win
            }
          }

          UnknownCandidate candidate;
          candidate.surface = surface;
          candidate.start = start_pos;
          candidate.end = end_pos;
          candidate.pos = core::PartOfSpeech::Verb;
          // Lower cost for higher confidence matches
          candidate.cost = 0.4F + (1.0F - best.confidence) * 0.3F;
          candidate.has_suffix = false;
          candidates.push_back(candidate);
          // Don't break - try other stem lengths too
        }
      }
    }
  }

  // Sort by cost and return best candidates
  std::sort(candidates.begin(), candidates.end(),
            [](const UnknownCandidate& lhs, const UnknownCandidate& rhs) {
              return lhs.cost < rhs.cost;
            });

  return candidates;
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateHiraganaVerbCandidates(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Hiragana) {
    return candidates;
  }

  // Skip if starting character is a particle that is NEVER a verb stem
  // Note: Exclude characters that CAN be verb stems:
  //   - な→なる/なくす
  //   - て→できる
  //   - や→やる (important: must NOT skip や)
  char32_t first_char = codepoints[start_pos];
  if (first_char == U'を' || first_char == U'が' || first_char == U'は' ||
      first_char == U'に' || first_char == U'へ' || first_char == U'の' ||
      first_char == U'か' || first_char == U'ね' ||
      first_char == U'よ' || first_char == U'わ') {
    return candidates;
  }

  // Skip if starting with demonstrative pronouns (これ, それ, あれ, どれ, etc.)
  // These are commonly mistaken for verbs (これる, それる, etc.)
  if (start_pos + 1 < codepoints.size()) {
    char32_t second_char = codepoints[start_pos + 1];
    // Check for こ/そ/あ/ど + れ/こ/ち patterns (demonstrative pronouns)
    if ((first_char == U'こ' || first_char == U'そ' ||
         first_char == U'あ' || first_char == U'ど') &&
        (second_char == U'れ' || second_char == U'こ' || second_char == U'ち')) {
      return candidates;
    }

    // Skip if starting with 「ない」(auxiliary verb/i-adjective for negation)
    // These should be recognized as AUX by dictionary, not as hiragana verbs.
    // E.g., 「ないんだ」→「ない」+「んだ」, not a single verb「ないむ」
    if (first_char == U'な' && second_char == U'い') {
      return candidates;
    }
  }

  // Find hiragana sequence, breaking at particle boundaries
  // Note: Be careful not to break at characters that are part of verb conjugations:
  //   - か can be part of なかった (negative past) or かった (i-adj past)
  //   - で can be part of んで (te-form for godan) or できる (potential verb)
  //   - も can be part of ても (even if) or もらう (receiving verb)
  size_t hiragana_end = start_pos;
  while (hiragana_end < char_types.size() &&
         hiragana_end - start_pos < 12 &&  // Max 12 hiragana for verb + endings
         char_types[hiragana_end] == normalize::CharType::Hiragana) {
    // Don't include particles that appear after the first hiragana character.
    // E.g., for "りにする", stop at "り" to not include "にする".
    if (hiragana_end > start_pos) {
      char32_t curr = codepoints[hiragana_end];

      // Check for particle-like characters
      if (curr == U'を' || curr == U'が' || curr == U'は' ||
          curr == U'に' || curr == U'へ' || curr == U'の' ||
          curr == U'と' || curr == U'や') {
        break;  // These are always particles in this context
      }

      // For か, で, も: check if they're part of verb conjugation patterns
      // Don't break if they appear in known conjugation contexts
      if (curr == U'か' || curr == U'で' || curr == U'も') {
        // Check the preceding character for conjugation patterns
        char32_t prev = codepoints[hiragana_end - 1];

        // か: OK if preceded by な (なかった = negative past)
        if (curr == U'か' && prev == U'な') {
          ++hiragana_end;
          continue;
        }

        // で: OK if preceded by ん (んで = te-form) or き (できる)
        if (curr == U'で' && (prev == U'ん' || prev == U'き')) {
          ++hiragana_end;
          continue;
        }

        // も: OK if preceded by て (ても = even if)
        if (curr == U'も' && prev == U'て') {
          ++hiragana_end;
          continue;
        }

        // Otherwise, treat as particle
        break;
      }
    }
    ++hiragana_end;
  }

  // Need at least 2 hiragana for a verb
  if (hiragana_end <= start_pos + 1) {
    return candidates;
  }

  // Try different lengths, starting from longest
  for (size_t end_pos = hiragana_end; end_pos > start_pos + 1; --end_pos) {
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);

    if (surface.empty()) {
      continue;
    }

    // Check if this looks like a conjugated verb
    auto best = inflection_.getBest(surface);
    // Only accept verb types (not IAdjective)
    if (best.confidence > 0.48F &&
        best.verb_type != grammar::VerbType::IAdjective) {
      // Lower cost for higher confidence matches
      float base_cost = 0.5F + (1.0F - best.confidence) * 0.3F;

      // Check if the base form exists in the dictionary
      // This gives a significant bonus for conjugations of known verbs
      // (e.g., できなくて → できる which is in the dictionary)
      bool is_dictionary_verb = false;
      if (dict_manager_ != nullptr && !best.base_form.empty()) {
        auto results = dict_manager_->lookup(best.base_form, 0);
        for (const auto& result : results) {
          if (result.entry != nullptr &&
              result.entry->surface == best.base_form &&
              result.entry->pos == core::PartOfSpeech::Verb) {
            is_dictionary_verb = true;
            break;
          }
        }
      }

      // Give significant bonus for dictionary-verified hiragana verbs
      // This helps them beat the particle+adj+particle split path
      // Only apply to longer forms (5+ chars) to avoid boosting short forms like
      // "あった" (ある) which can interfere with copula recognition (であった)
      size_t candidate_len = end_pos - start_pos;
      if (is_dictionary_verb && candidate_len >= 5) {
        base_cost = 0.1F + (1.0F - best.confidence) * 0.2F;
      }

      UnknownCandidate candidate;
      candidate.surface = surface;
      candidate.start = start_pos;
      candidate.end = end_pos;
      candidate.pos = core::PartOfSpeech::Verb;
      candidate.cost = base_cost;
      candidate.has_suffix = false;
      candidates.push_back(candidate);
    }
  }

  // Sort by cost
  std::sort(candidates.begin(), candidates.end(),
            [](const UnknownCandidate& lhs, const UnknownCandidate& rhs) {
              return lhs.cost < rhs.cost;
            });

  return candidates;
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateAdjectiveCandidates(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Kanji) {
    return candidates;
  }

  // Find kanji portion (typically 1-2 characters for i-adjectives)
  size_t kanji_end = start_pos;
  while (kanji_end < char_types.size() &&
         kanji_end - start_pos < 3 &&  // Max 3 kanji for adjective stem
         char_types[kanji_end] == normalize::CharType::Kanji) {
    ++kanji_end;
  }

  if (kanji_end == start_pos) {
    return candidates;
  }

  // Look for hiragana after kanji (adjective endings like い, かった, くない)
  // Note: Some adjectives have hiragana in the stem (美しい, 楽しい, 涼しい, etc.)
  // so we allow any hiragana and let the inflection module decide
  if (kanji_end >= char_types.size() ||
      char_types[kanji_end] != normalize::CharType::Hiragana) {
    return candidates;
  }

  // Check if first hiragana is a particle that can NEVER be part of an adjective
  // Note: て is the te-form particle (接続助詞), not part of adjective stems
  // This prevents "来てい" from being parsed as an adjective (来ている = verb)
  char32_t first_hiragana = codepoints[kanji_end];
  if (first_hiragana == U'を' || first_hiragana == U'が' ||
      first_hiragana == U'は' || first_hiragana == U'も' ||
      first_hiragana == U'へ' || first_hiragana == U'の' ||
      first_hiragana == U'に' || first_hiragana == U'や' ||
      first_hiragana == U'て' || first_hiragana == U'で') {
    return candidates;  // These particles follow nouns/verbs, not adjective stems
  }

  size_t hiragana_end = kanji_end;
  while (hiragana_end < char_types.size() &&
         hiragana_end - kanji_end < 8 &&
         char_types[hiragana_end] == normalize::CharType::Hiragana) {
    ++hiragana_end;
  }

  if (hiragana_end <= kanji_end) {
    return candidates;
  }

  // Try different ending lengths
  for (size_t end_pos = hiragana_end; end_pos > kanji_end; --end_pos) {
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);

    if (surface.empty()) {
      continue;
    }

    // Skip single-kanji + single hiragana "い" patterns
    // These are typically godan verb renyokei (伴い, 用い, 買い, 追い)
    // not i-adjectives. Real single-kanji i-adjectives (怖い, 酸い)
    // should be in the dictionary, not generated as unknown words.
    if (kanji_end == start_pos + 1 && end_pos == kanji_end + 1) {
      continue;  // Skip - likely godan verb renyokei, not i-adjective
    }

    // Skip patterns ending with っ + hiragana (〜てく, 〜てない, etc.)
    // These are te-form contractions (〜ていく→〜てく), not i-adjectives.
    // E.g., 待ってく (matte-ku) = 待っていく, not an adjective
    std::string hiragana_part = extractSubstring(codepoints, kanji_end, end_pos);
    if (hiragana_part.size() >= 6 &&  // At least 2 hiragana (っ + く = 6 bytes)
        hiragana_part.substr(0, 3) == "っ") {  // Starts with っ
      continue;  // Skip - likely te-form contraction, not i-adjective
    }

    // Skip patterns ending with 〜んでい or 〜でい
    // These are te-form + auxiliary verb patterns (〜んでいく, 〜ている, etc.)
    // not i-adjectives. E.g., 学んでい (manande-i) = 学んでいく, not adj
    if (hiragana_part.size() >= 9 &&  // んでい = 9 bytes
        hiragana_part.substr(hiragana_part.size() - 9) == "んでい") {
      continue;  // Skip - likely te-form + aux pattern
    }
    if (hiragana_part.size() >= 6 &&  // でい = 6 bytes
        hiragana_part.substr(hiragana_part.size() - 6) == "でい") {
      continue;  // Skip - likely te-form + aux pattern
    }

    // Skip patterns ending with verb passive/potential/causative negative renyokei
    // 〜られなく, 〜れなく, 〜させなく, 〜せなく, 〜されなく are all verb forms,
    // not i-adjectives. E.g., 食べられなく = 食べられる + ない (negative renyokei)
    if (hiragana_part.size() >= 12 &&  // られなく = 12 bytes
        hiragana_part.substr(hiragana_part.size() - 12) == "られなく") {
      continue;  // Skip - passive/potential negative renyokei
    }
    if (hiragana_part.size() >= 9 &&  // れなく = 9 bytes
        hiragana_part.substr(hiragana_part.size() - 9) == "れなく") {
      continue;  // Skip - passive/potential negative renyokei
    }
    if (hiragana_part.size() >= 12 &&  // させなく = 12 bytes
        hiragana_part.substr(hiragana_part.size() - 12) == "させなく") {
      continue;  // Skip - causative negative renyokei
    }
    if (hiragana_part.size() >= 9 &&  // せなく = 9 bytes
        hiragana_part.substr(hiragana_part.size() - 9) == "せなく") {
      continue;  // Skip - causative negative renyokei
    }
    if (hiragana_part.size() >= 12 &&  // されなく = 12 bytes
        hiragana_part.substr(hiragana_part.size() - 12) == "されなく") {
      continue;  // Skip - passive negative renyokei
    }

    auto best = inflection_.getBest(surface);
    // Require confidence >= 0.5 for i-adjectives
    // Base forms like 寒い get exactly 0.5, conjugated forms like 美しかった get 0.68+
    if (best.confidence >= 0.5F &&
        best.verb_type == grammar::VerbType::IAdjective) {
      // Filter out false positives: いたす honorific pattern
      // Invalid patterns (all have た after the candidate):
      //   - サ変名詞 + いたす: 検討いたします, 勉強いたしました
      //   - Verb renyokei + いたす: 伝えいたします, 申しいたします
      // Valid patterns:
      //   - 面白いな (next char is な)
      //   - 寒いよ (next char is よ)
      //   - 面白い (end of text)
      // Key insight: if minimum confidence (0.5) and next char is た, skip
      if (best.confidence <= 0.5F) {
        if (end_pos < codepoints.size() && codepoints[end_pos] == U'た') {
          continue;  // Skip - likely いたす honorific pattern
        }
      }

      UnknownCandidate candidate;
      candidate.surface = surface;
      candidate.start = start_pos;
      candidate.end = end_pos;
      candidate.pos = core::PartOfSpeech::Adjective;
      // Lower base cost (0.2F) to beat verb candidates after POS prior adjustment
      // ADJ prior (0.3) is higher than VERB prior (0.2), so we need lower edge cost
      candidate.cost = 0.2F + (1.0F - best.confidence) * 0.3F;
      candidate.has_suffix = false;
      candidates.push_back(candidate);
    }
  }

  std::sort(candidates.begin(), candidates.end(),
            [](const UnknownCandidate& lhs, const UnknownCandidate& rhs) {
              return lhs.cost < rhs.cost;
            });

  return candidates;
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateNaAdjectiveCandidates(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Kanji) {
    return candidates;
  }

  // Find kanji sequence
  size_t kanji_end = start_pos;
  while (kanji_end < char_types.size() &&
         kanji_end - start_pos < options_.max_kanji_length &&
         char_types[kanji_end] == normalize::CharType::Kanji) {
    ++kanji_end;
  }

  // Need at least 2 kanji (stem + 的)
  if (kanji_end - start_pos < 2) {
    return candidates;
  }

  std::string kanji_seq = extractSubstring(codepoints, start_pos, kanji_end);

  // Check for na-adjective suffixes (的)
  for (const auto& suffix : kNaAdjSuffixes) {
    // Check if kanji_seq ends with suffix
    if (kanji_seq.size() >= suffix.size()) {
      std::string_view kanji_suffix(kanji_seq.data() + kanji_seq.size() - suffix.size(),
                                     suffix.size());
      if (kanji_suffix == suffix) {
        // Found a na-adjective pattern like 理性的, 論理的
        UnknownCandidate candidate;
        candidate.surface = kanji_seq;
        candidate.start = start_pos;
        candidate.end = kanji_end;
        candidate.pos = core::PartOfSpeech::Adjective;
        // Low cost to prefer this over noun interpretation
        candidate.cost = 0.3F;
        candidate.has_suffix = true;
        candidates.push_back(candidate);
        break;  // Use first matching suffix
      }
    }
  }

  return candidates;
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateHiraganaAdjectiveCandidates(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Hiragana) {
    return candidates;
  }

  // Skip if starting character is a particle that is NEVER an adjective stem
  char32_t first_char = codepoints[start_pos];
  if (first_char == U'を' || first_char == U'が' || first_char == U'は' ||
      first_char == U'に' || first_char == U'へ' || first_char == U'の' ||
      first_char == U'か' || first_char == U'や' || first_char == U'ね' ||
      first_char == U'よ' || first_char == U'わ' || first_char == U'で' ||
      first_char == U'と' || first_char == U'も') {
    return candidates;
  }

  // Find hiragana sequence, breaking at particle boundaries
  // Note: For i-adjectives, we allow longer sequences because conjugation
  // patterns include か (かった), く (くない), etc.
  size_t hiragana_end = start_pos;
  while (hiragana_end < char_types.size() &&
         hiragana_end - start_pos < 10 &&  // Max 10 hiragana for adjective + endings
         char_types[hiragana_end] == normalize::CharType::Hiragana) {
    // Only break at strong particle boundaries (を, が, は, に, etc.)
    // after the first 3 hiragana (minimum adjective stem length)
    // This allows patterns like まずかった, おいしくない
    if (hiragana_end - start_pos >= 3) {
      char32_t curr = codepoints[hiragana_end];
      if (curr == U'を' || curr == U'が' || curr == U'は' ||
          curr == U'に' || curr == U'へ' || curr == U'の' ||
          curr == U'で' || curr == U'と' || curr == U'も' ||
          curr == U'や') {
        break;  // Stop before the particle
      }
    }
    ++hiragana_end;
  }

  // Need at least 3 hiragana for an i-adjective (e.g., あつい)
  if (hiragana_end <= start_pos + 2) {
    return candidates;
  }

  // Try different lengths, starting from longest
  for (size_t end_pos = hiragana_end; end_pos > start_pos + 2; --end_pos) {
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);

    if (surface.empty()) {
      continue;
    }

    // Check if this looks like a conjugated i-adjective
    auto best = inflection_.getBest(surface);
    if (best.confidence >= 0.5F &&
        best.verb_type == grammar::VerbType::IAdjective) {
      UnknownCandidate candidate;
      candidate.surface = surface;
      candidate.start = start_pos;
      candidate.end = end_pos;
      candidate.pos = core::PartOfSpeech::Adjective;
      // Lower cost for higher confidence matches
      candidate.cost = 0.3F + (1.0F - best.confidence) * 0.3F;
      candidate.has_suffix = false;
      candidates.push_back(candidate);
    }
  }

  // Sort by cost
  std::sort(candidates.begin(), candidates.end(),
            [](const UnknownCandidate& lhs, const UnknownCandidate& rhs) {
              return lhs.cost < rhs.cost;
            });

  return candidates;
}

std::vector<UnknownCandidate> UnknownWordGenerator::generateNominalizedNounCandidates(
    std::string_view /*text*/, const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Kanji) {
    return candidates;
  }

  // Find kanji portion (typically 1-3 characters for nominalized nouns)
  size_t kanji_end = start_pos;
  while (kanji_end < char_types.size() &&
         kanji_end - start_pos < 4 &&  // Max 4 kanji
         char_types[kanji_end] == normalize::CharType::Kanji) {
    ++kanji_end;
  }

  // Need at least 1 kanji
  if (kanji_end == start_pos) {
    return candidates;
  }

  // Look for 1-2 hiragana after kanji (nominalization endings)
  if (kanji_end >= char_types.size() ||
      char_types[kanji_end] != normalize::CharType::Hiragana) {
    return candidates;
  }

  // Check for common nominalization patterns:
  // - Single hiragana: け, げ, せ, い, り, ち, き, ぎ, し, み, び, に, etc.
  // - Double hiragana: 上げ, 下げ, 付け, 入り, etc.
  char32_t first_hiragana = codepoints[kanji_end];

  // Skip particles that never form nominalizations
  if (first_hiragana == U'を' || first_hiragana == U'が' ||
      first_hiragana == U'は' || first_hiragana == U'も' ||
      first_hiragana == U'へ' || first_hiragana == U'の' ||
      first_hiragana == U'に' || first_hiragana == U'で' ||
      first_hiragana == U'と' || first_hiragana == U'や' ||
      first_hiragana == U'か') {
    return candidates;
  }

  // Common nominalization endings (renyokei stems)
  // Single char: け(ける), げ(げる), せ(せる), い(う/く), り(る), ち(つ), etc.
  bool is_nominalization_ending = (
      first_hiragana == U'け' || first_hiragana == U'げ' ||
      first_hiragana == U'せ' || first_hiragana == U'い' ||
      first_hiragana == U'り' || first_hiragana == U'ち' ||
      first_hiragana == U'き' || first_hiragana == U'ぎ' ||
      first_hiragana == U'し' || first_hiragana == U'み' ||
      first_hiragana == U'び' || first_hiragana == U'え' ||
      first_hiragana == U'れ' || first_hiragana == U'め');

  if (!is_nominalization_ending) {
    return candidates;
  }

  // Check for 1 or 2 hiragana (e.g., け or 上げ)
  size_t hiragana_end = kanji_end + 1;

  // Check for 2-hiragana patterns if second char is also valid
  if (hiragana_end < char_types.size() &&
      char_types[hiragana_end] == normalize::CharType::Hiragana) {
    char32_t second_hiragana = codepoints[hiragana_end];
    // Common 2-char nominalization endings
    // 上げ, 下げ, 付け, 入り, 上り, 下り, etc.
    if (second_hiragana == U'げ' || second_hiragana == U'け' ||
        second_hiragana == U'り' || second_hiragana == U'い' ||
        second_hiragana == U'え' || second_hiragana == U'し') {
      // Generate 2-hiragana candidate
      std::string surface = extractSubstring(codepoints, start_pos, hiragana_end + 1);
      if (!surface.empty()) {
        UnknownCandidate candidate;
        candidate.surface = surface;
        candidate.start = start_pos;
        candidate.end = hiragana_end + 1;
        candidate.pos = core::PartOfSpeech::Noun;
        // Good cost to compete with kanji-only + hiragana-other split
        candidate.cost = 0.8F;
        candidate.has_suffix = false;
        candidates.push_back(candidate);
      }
    }
  }

  // Generate 1-hiragana candidate
  // Only if the next character is NOT a particle that would indicate verb+aux pattern
  // e.g., "飲みながら" should parse as verb, not "飲み" (noun) + "ながら" (particle)
  bool skip_single_char = false;
  if (kanji_end + 1 < char_types.size() &&
      char_types[kanji_end + 1] == normalize::CharType::Hiragana) {
    char32_t next_char = codepoints[kanji_end + 1];
    // Skip if followed by common auxiliary beginnings
    // ながら, ない, なく, etc.
    if (next_char == U'な') {
      skip_single_char = true;
    }
  }

  if (!skip_single_char) {
    std::string surface = extractSubstring(codepoints, start_pos, kanji_end + 1);
    if (!surface.empty()) {
      UnknownCandidate candidate;
      candidate.surface = surface;
      candidate.start = start_pos;
      candidate.end = kanji_end + 1;
      candidate.pos = core::PartOfSpeech::Noun;
      // Higher cost to avoid competing with verb candidates
      candidate.cost = 1.2F;
      candidate.has_suffix = false;
      candidates.push_back(candidate);
    }
  }

  return candidates;
}

}  // namespace suzume::analysis
