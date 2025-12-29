/**
 * @file conjugator.cpp
 * @brief Dynamic conjugation stem generator implementation
 */

#include "conjugator.h"

#include "core/utf8_constants.h"

namespace suzume::grammar {

namespace {

// UTF-8 encode a single codepoint
std::string encodeUtf8(char32_t codepoint) {
  std::string result;
  if (codepoint < 0x80) {
    result.push_back(static_cast<char>(codepoint));
  } else if (codepoint < 0x800) {
    result.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
    result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else if (codepoint < 0x10000) {
    result.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
    result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  }
  return result;
}

// Godan row data
struct GodanRow {
  char32_t base_char;   // 終止形: く
  char32_t a_row;       // 未然形: か
  char32_t i_row;       // 連用形: き
  std::string onbin;    // 音便: い, っ, ん
  bool voiced;          // た→だ
};

const GodanRow* getGodanRow(VerbType type) {
  static const GodanRow kRows[] = {
      {U'く', U'か', U'き', "い", false},  // GodanKa
      {U'ぐ', U'が', U'ぎ', "い", true},   // GodanGa
      {U'す', U'さ', U'し', "", false},    // GodanSa
      {U'つ', U'た', U'ち', "っ", false},  // GodanTa
      {U'ぬ', U'な', U'に', "ん", true},   // GodanNa
      {U'ぶ', U'ば', U'び', "ん", true},   // GodanBa
      {U'む', U'ま', U'み', "ん", true},   // GodanMa
      {U'る', U'ら', U'り', "っ", false},  // GodanRa
      {U'う', U'わ', U'い', "っ", false},  // GodanWa
  };

  int idx = static_cast<int>(type) - static_cast<int>(VerbType::GodanKa);
  if (idx < 0 || idx >= 9) {
    return nullptr;
  }
  return &kRows[idx];
}

}  // namespace

Conjugator::Conjugator() = default;

std::string Conjugator::getStem(const std::string& base_form,
                                VerbType type) const {
  return conjugation_.getStem(base_form, type);
}

VerbType Conjugator::detectType(const std::string& base_form) const {
  return conjugation_.detectType(base_form);
}

std::vector<StemForm> Conjugator::generateStems(const std::string& base_form,
                                                VerbType type) const {
  std::string stem = getStem(base_form, type);

  // Get base suffix (e.g., く for GodanKa)
  std::string base_suffix;
  if (base_form.size() >= core::kJapaneseCharBytes) {
    base_suffix = base_form.substr(base_form.size() - core::kJapaneseCharBytes);
  }

  switch (type) {
    case VerbType::Ichidan:
      return generateIchidanStems(stem, base_form);

    case VerbType::GodanKa:
    case VerbType::GodanGa:
    case VerbType::GodanSa:
    case VerbType::GodanTa:
    case VerbType::GodanNa:
    case VerbType::GodanBa:
    case VerbType::GodanMa:
    case VerbType::GodanRa:
    case VerbType::GodanWa:
      return generateGodanStems(stem, base_form, type);

    case VerbType::Suru:
      return generateSuruStems(stem, base_form);

    case VerbType::Kuru:
      return generateKuruStems(stem, base_form);

    default:
      return {};
  }
}

std::vector<StemForm> Conjugator::generateGodanStems(
    const std::string& stem, const std::string& base_form,
    VerbType type) const {
  std::vector<StemForm> forms;
  const auto* row = getGodanRow(type);
  if (row == nullptr) {
    return forms;
  }

  std::string base_suffix = encodeUtf8(row->base_char);
  std::string a_suffix = encodeUtf8(row->a_row);
  std::string i_suffix = encodeUtf8(row->i_row);

  // 終止形 (Base)
  forms.push_back({base_form, type, base_suffix, conn::kVerbBase});

  // 未然形 (Mizenkei): 書か
  forms.push_back({stem + a_suffix, type, base_suffix, conn::kVerbMizenkei});

  // 連用形 (Renyokei): 書き
  forms.push_back({stem + i_suffix, type, base_suffix, conn::kVerbRenyokei});

  // 音便形 (Onbinkei): 書い, 読ん, 持っ
  if (!row->onbin.empty()) {
    forms.push_back({stem + row->onbin, type, base_suffix, conn::kVerbOnbinkei});
  } else {
    // サ行: 連用形が音便形を兼ねる (話し + た)
    forms.push_back({stem + i_suffix, type, base_suffix, conn::kVerbOnbinkei});
  }

  return forms;
}

std::vector<StemForm> Conjugator::generateIchidanStems(
    const std::string& stem, const std::string& base_form) const {
  std::vector<StemForm> forms;
  VerbType type = VerbType::Ichidan;

  // 一段動詞: 語幹 = 食べ (食べる - る)
  // 終止形
  forms.push_back({base_form, type, "る", conn::kVerbBase});

  // 未然形・連用形・音便形はすべて語幹と同じ
  forms.push_back({stem, type, "る", conn::kVerbMizenkei});
  forms.push_back({stem, type, "る", conn::kVerbRenyokei});
  forms.push_back({stem, type, "る", conn::kVerbOnbinkei});

  return forms;
}

std::vector<StemForm> Conjugator::generateSuruStems(
    const std::string& stem, const std::string& base_form) const {
  std::vector<StemForm> forms;
  VerbType type = VerbType::Suru;

  // する: し (連用形・音便形), さ (未然形), せ (未然形/可能)
  forms.push_back({base_form, type, "する", conn::kVerbBase});
  forms.push_back({stem + "し", type, "する", conn::kVerbRenyokei});
  forms.push_back({stem + "し", type, "する", conn::kVerbOnbinkei});
  forms.push_back({stem + "さ", type, "する", conn::kVerbMizenkei});

  return forms;
}

std::vector<StemForm> Conjugator::generateKuruStems(
    const std::string& stem, const std::string& base_form) const {
  std::vector<StemForm> forms;
  VerbType type = VerbType::Kuru;

  // 来る: き (連用形), こ (未然形)
  forms.push_back({base_form, type, "くる", conn::kVerbBase});
  forms.push_back({stem + "き", type, "くる", conn::kVerbRenyokei});
  forms.push_back({stem + "き", type, "くる", conn::kVerbOnbinkei});
  forms.push_back({stem + "こ", type, "くる", conn::kVerbMizenkei});

  return forms;
}

}  // namespace suzume::grammar
