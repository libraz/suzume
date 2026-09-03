/**
 * @file compound_verb_forms.cpp
 * @brief Shared inflection-form generation for compound-verb candidates
 */

#include "join_compound_verb_internal.h"

namespace suzume::analysis::compound_verb_detail {

namespace {

const grammar::Conjugation::GodanEntry* findGodanRowByEnding(std::string_view base_ending) {
  if (base_ending.size() != core::kJapaneseCharBytes) {
    return nullptr;
  }

  const char32_t ending = utf8::decodeFirstChar(base_ending);
  for (const auto& entry : grammar::Conjugation::getGodanRows()) {
    if (entry.second.base_vowel == ending) {
      return &entry;
    }
  }
  return nullptr;
}

dictionary::ConjugationType toDictionaryConjugationType(grammar::VerbType verb_type) {
  switch (verb_type) {
    case grammar::VerbType::GodanKa:
      return dictionary::ConjugationType::GodanKa;
    case grammar::VerbType::GodanGa:
      return dictionary::ConjugationType::GodanGa;
    case grammar::VerbType::GodanSa:
      return dictionary::ConjugationType::GodanSa;
    case grammar::VerbType::GodanTa:
      return dictionary::ConjugationType::GodanTa;
    case grammar::VerbType::GodanNa:
      return dictionary::ConjugationType::GodanNa;
    case grammar::VerbType::GodanBa:
      return dictionary::ConjugationType::GodanBa;
    case grammar::VerbType::GodanMa:
      return dictionary::ConjugationType::GodanMa;
    case grammar::VerbType::GodanRa:
      return dictionary::ConjugationType::GodanRa;
    case grammar::VerbType::GodanWa:
      return dictionary::ConjugationType::GodanWa;
    default:
      return dictionary::ConjugationType::None;
  }
}

std::string replaceGodanEnding(std::string_view base, bool use_o_row) {
  if (base.size() < core::kJapaneseCharBytes) {
    return "";
  }

  const char32_t last_cp = utf8::decodeLastChar(base);
  for (const auto& [row_verb_type, row] : grammar::Conjugation::getGodanRows()) {
    (void)row_verb_type;
    if (row.base_vowel == last_cp) {
      std::string result(base.substr(0, base.size() - core::kJapaneseCharBytes));
      result += normalize::encodeUtf8(use_o_row ? row.o_row : row.e_row);
      return result;
    }
  }
  return "";
}

}  // namespace

dictionary::ConjugationType compoundConjugationType(V2VerbType verb_type, std::string_view base_ending) {
  if (verb_type == V2VerbType::Ichidan) {
    return dictionary::ConjugationType::Ichidan;
  }
  const auto* godan_entry = findGodanRowByEnding(base_ending);
  return godan_entry == nullptr ? dictionary::ConjugationType::None : toDictionaryConjugationType(godan_entry->first);
}

namespace {

// Renyokei and mizenkei are the same operation on the V2 base: an Ichidan verb
// drops its final mora, and a Godan verb moves that mora to another row of the
// same column. Only the target row differs between the two forms.
std::string generateGodanRowStem(std::string_view surface, std::string_view reading, V2VerbType verb_type,
                                 bool use_a_row) {
  std::string_view base = reading.empty() ? surface : reading;
  if (base.empty())
    return "";

  if (verb_type == V2VerbType::Ichidan) {
    return base.size() >= core::kJapaneseCharBytes ? std::string(base.substr(0, base.size() - core::kJapaneseCharBytes))
                                                   : "";
  }

  if (base.size() < core::kJapaneseCharBytes)
    return "";
  const char32_t final_mora = utf8::decodeLastChar(base);
  const std::string_view row =
      use_a_row ? grammar::godanARowSuffixFromURow(final_mora) : grammar::godanIRowSuffixFromURow(final_mora);
  if (row.empty())
    return "";
  std::string result(base.substr(0, base.size() - core::kJapaneseCharBytes));
  result += row;
  return result;
}

}  // namespace

std::string generateRenyokei(std::string_view surface, std::string_view reading, V2VerbType verb_type) {
  return generateGodanRowStem(surface, reading, verb_type, false);
}

std::string generateMizenkei(std::string_view surface, std::string_view reading, V2VerbType verb_type) {
  return generateGodanRowStem(surface, reading, verb_type, true);
}

std::string generateVolitionalStem(std::string_view surface, std::string_view reading, V2VerbType verb_type) {
  const std::string_view base = reading.empty() ? surface : reading;
  // An Ichidan verb forms its volitional from the bare stem plus よ, where a
  // Godan verb uses its o-row (続けよ+う against 出そ+う).
  if (verb_type == V2VerbType::Ichidan) {
    if (base.size() < core::kJapaneseCharBytes) {
      return "";
    }
    return normalize::concat(base.substr(0, base.size() - core::kJapaneseCharBytes), "よ");
  }
  return replaceGodanEnding(base, true);
}

std::string generateKateikei(std::string_view surface, std::string_view reading, V2VerbType verb_type) {
  const std::string_view base = reading.empty() ? surface : reading;
  if (base.size() < core::kJapaneseCharBytes) {
    return "";
  }

  if (verb_type == V2VerbType::Ichidan) {
    return normalize::concat(base.substr(0, base.size() - core::kJapaneseCharBytes), "れ");
  }

  return replaceGodanEnding(base, false);
}

std::string generateGodanPotential(std::string_view surface, std::string_view reading, V2VerbType verb_type) {
  if (verb_type != V2VerbType::Godan) {
    return "";
  }

  std::string result = replaceGodanEnding(reading.empty() ? surface : reading, false);
  if (!result.empty()) {
    result += "る";
  }
  return result;
}

TeFormType getTeFormType(std::string_view base_ending) {
  const auto* godan_entry = findGodanRowByEnding(base_ending);
  if (godan_entry == nullptr) {
    return TeFormType::Ichidan;
  }
  const std::string_view onbin = godan_entry->second.onbin;
  if (onbin == "い")
    return TeFormType::Ionbin;
  if (onbin == "っ")
    return TeFormType::Sokuonbin;
  if (onbin == "ん")
    return TeFormType::Hatsuonbin;
  return TeFormType::Renyokei;
}

std::pair<std::string, bool> generateTeFormStem(std::string_view surface, std::string_view reading,
                                                V2VerbType verb_type, std::string_view base_ending) {
  const std::string_view base = reading.empty() ? surface : reading;
  if (base.empty() || base.size() < core::kJapaneseCharBytes)
    return {"", false};

  if (verb_type == V2VerbType::Ichidan) {
    return {std::string(base.substr(0, base.size() - core::kJapaneseCharBytes)), false};
  }

  std::string result(base.substr(0, base.size() - core::kJapaneseCharBytes));
  switch (getTeFormType(base_ending)) {
    case TeFormType::Ionbin:
      result += "い";
      return {result, base_ending == "ぐ"};
    case TeFormType::Sokuonbin:
      result += "っ";
      return {result, false};
    case TeFormType::Hatsuonbin:
      result += "ん";
      return {result, true};
    case TeFormType::Renyokei:
      result += "し";
      return {result, false};
    default:
      return {"", false};
  }
}

std::string generateKanjiRenyokei(std::string_view kanji_surface, std::string_view reading, V2VerbType verb_type) {
  if (reading.empty()) {
    return generateRenyokei(kanji_surface, "", verb_type);
  }
  const std::string hiragana_renyokei = generateRenyokei(reading, "", verb_type);
  if (hiragana_renyokei.empty())
    return "";

  size_t kanji_bytes = 0;
  for (size_t scan_pos = 0; scan_pos < kanji_surface.size();) {
    size_t next_pos = scan_pos;
    if (!normalize::isKanjiCodepoint(normalize::decodeUtf8(kanji_surface, next_pos)))
      break;
    kanji_bytes = next_pos;
    scan_pos = next_pos;
  }
  if (kanji_bytes == 0)
    return "";

  std::string result(kanji_surface.substr(0, kanji_bytes));
  const size_t reading_kanji_len = reading.size() - (kanji_surface.size() - kanji_bytes);
  if (reading_kanji_len < hiragana_renyokei.size()) {
    result += hiragana_renyokei.substr(reading_kanji_len);
  }
  return result;
}

char32_t godanRenyokeiBaseCp(char32_t renyokei_cp) {
  const std::string_view base = grammar::godanBaseSuffixFromIRow(renyokei_cp);
  return base.empty() ? 0 : utf8::decodeFirstChar(base);
}

}  // namespace suzume::analysis::compound_verb_detail
