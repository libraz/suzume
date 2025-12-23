/**
 * @file conjugation.cpp
 * @brief Japanese verb/adjective conjugation rules implementation
 */

#include "conjugation.h"

#include <algorithm>

namespace suzume::grammar {

namespace {

// UTF-8 encode a single codepoint
std::string encodeUtf8(char32_t cp) {
  std::string result;
  if (cp < 0x80) {
    result.push_back(static_cast<char>(cp));
  } else if (cp < 0x800) {
    result.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp < 0x10000) {
    result.push_back(static_cast<char>(0xE0 | (cp >> 12)));
    result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
  return result;
}

// Helper to create ConjugatedForm
ConjugatedForm makeForm(const std::string& surface,
                        const std::string& base_form,
                        const std::string& stem, VerbType type,
                        const std::string& suffix) {
  return {surface, base_form, stem, type, suffix};
}

}  // namespace

Conjugation::Conjugation() { initGodanRows(); }

void Conjugation::initGodanRows() {
  // 五段動詞の各行の活用パターン
  // base_vowel: 終止形語尾 (く, ぐ, す...)
  // a_row: 未然形 (か, が, さ...)
  // i_row: 連用形 (き, ぎ, し...)
  // e_row: 仮定形・命令形 (け, げ, せ...)
  // o_row: 意志形 (こ, ご, そ...)
  // onbin: 音便形 (い, っ, ん, "" for さ行)
  // voiced_ta: 連用形+た が だ になるか

  godan_rows_ = {
      {VerbType::GodanKa, {U'く', U'か', U'き', U'け', U'こ', "い", false}},
      {VerbType::GodanGa, {U'ぐ', U'が', U'ぎ', U'げ', U'ご', "い", true}},
      {VerbType::GodanSa, {U'す', U'さ', U'し', U'せ', U'そ', "", false}},
      {VerbType::GodanTa, {U'つ', U'た', U'ち', U'て', U'と', "っ", false}},
      {VerbType::GodanNa, {U'ぬ', U'な', U'に', U'ね', U'の', "ん", true}},
      {VerbType::GodanBa, {U'ぶ', U'ば', U'び', U'べ', U'ぼ', "ん", true}},
      {VerbType::GodanMa, {U'む', U'ま', U'み', U'め', U'も', "ん", true}},
      {VerbType::GodanRa, {U'る', U'ら', U'り', U'れ', U'ろ', "っ", false}},
      {VerbType::GodanWa, {U'う', U'わ', U'い', U'え', U'お', "っ", false}},
  };
}

std::string Conjugation::getStem(const std::string& base_form, VerbType type) {
  if (base_form.empty()) {
    return "";
  }

  size_t len = base_form.size();
  if (len < 3) {
    return base_form;
  }

  switch (type) {
    case VerbType::Ichidan:
    case VerbType::GodanKa:
    case VerbType::GodanGa:
    case VerbType::GodanSa:
    case VerbType::GodanTa:
    case VerbType::GodanNa:
    case VerbType::GodanBa:
    case VerbType::GodanMa:
    case VerbType::GodanRa:
    case VerbType::GodanWa:
    case VerbType::IAdjective:
      // Remove last hiragana (3 bytes in UTF-8)
      return base_form.substr(0, len - 3);

    case VerbType::Suru:
      if (base_form == "する") {
        return "";
      }
      // Xする → X (remove する = 6 bytes)
      if (len >= 6) {
        return base_form.substr(0, len - 6);
      }
      return "";

    case VerbType::Kuru:
      // 来る → 来 (remove る = 3 bytes)
      return base_form.substr(0, len - 3);

    default:
      return base_form.substr(0, len - 3);
  }
}

VerbType Conjugation::detectType(const std::string& base_form) {
  if (base_form.empty() || base_form.size() < 3) {
    return VerbType::Unknown;
  }

  // Check last character
  std::string last = base_form.substr(base_form.size() - 3);

  // Special verbs
  if (base_form == "する") {
    return VerbType::Suru;
  }
  if (base_form == "来る" || base_form == "くる") {
    return VerbType::Kuru;
  }

  // サ変複合動詞
  if (base_form.size() >= 6 &&
      base_form.substr(base_form.size() - 6) == "する") {
    return VerbType::Suru;
  }

  // い形容詞
  if (last == "い") {
    // Check if second-to-last is a kanji (not hiragana)
    // Simple heuristic: if ends with かしい, たしい, etc. → adjective
    return VerbType::IAdjective;
  }

  // 一段 vs 五段 (heuristic based on ending)
  if (last == "る") {
    // If second-to-last char is え段 or い段, likely 一段
    // This is a heuristic - not always correct
    if (base_form.size() >= 6) {
      std::string prev = base_form.substr(base_form.size() - 6, 3);
      // え段: え, け, せ, て, ね, へ, め, れ, げ, ぜ, で, べ, ぺ
      // い段: い, き, し, ち, に, ひ, み, り, ぎ, じ, ぢ, び, ぴ
      if (prev == "べ" || prev == "め" || prev == "せ" || prev == "て" ||
          prev == "け" || prev == "れ" || prev == "え" || prev == "ね" ||
          prev == "げ" || prev == "ぜ" || prev == "で" || prev == "へ" ||
          prev == "み" || prev == "き" || prev == "し" || prev == "ち" ||
          prev == "に" || prev == "り" || prev == "い" || prev == "ひ" ||
          prev == "び" || prev == "ぎ" || prev == "じ") {
        return VerbType::Ichidan;
      }
    }
    return VerbType::GodanRa;
  }

  // 五段 based on ending
  if (last == "く") {
    return VerbType::GodanKa;
  }
  if (last == "ぐ") {
    return VerbType::GodanGa;
  }
  if (last == "す") {
    return VerbType::GodanSa;
  }
  if (last == "つ") {
    return VerbType::GodanTa;
  }
  if (last == "ぬ") {
    return VerbType::GodanNa;
  }
  if (last == "ぶ") {
    return VerbType::GodanBa;
  }
  if (last == "む") {
    return VerbType::GodanMa;
  }
  if (last == "う") {
    return VerbType::GodanWa;
  }

  return VerbType::Unknown;
}

std::vector<ConjugatedForm> Conjugation::generate(const std::string& base_form,
                                                  VerbType type) const {
  std::string stem = getStem(base_form, type);

  switch (type) {
    case VerbType::Ichidan:
      return generateIchidan(stem, base_form);

    case VerbType::GodanKa:
    case VerbType::GodanGa:
    case VerbType::GodanSa:
    case VerbType::GodanTa:
    case VerbType::GodanNa:
    case VerbType::GodanBa:
    case VerbType::GodanMa:
    case VerbType::GodanRa:
    case VerbType::GodanWa:
      return generateGodan(stem, base_form, type);

    case VerbType::Suru:
      return generateSuru(stem, base_form);

    case VerbType::Kuru:
      return generateKuru(stem, base_form);

    case VerbType::IAdjective:
      return generateIAdjective(stem, base_form);

    default:
      return {};
  }
}

std::vector<ConjugatedForm> Conjugation::generateGodan(
    const std::string& stem, const std::string& base_form,
    VerbType type) const {
  std::vector<ConjugatedForm> forms;
  auto it = godan_rows_.find(type);
  if (it == godan_rows_.end()) {
    return forms;
  }

  const auto& row = it->second;
  std::string ta = row.voiced_ta ? "だ" : "た";
  std::string te = row.voiced_ta ? "で" : "て";

  // 基本形
  forms.push_back(makeForm(base_form, base_form, stem, type, ""));

  std::string a = encodeUtf8(row.a_row);
  std::string i = encodeUtf8(row.i_row);
  std::string e = encodeUtf8(row.e_row);
  std::string o = encodeUtf8(row.o_row);

  // 未然形 + ない系
  forms.push_back(makeForm(stem + a + "ない", base_form, stem, type, "ない"));
  forms.push_back(makeForm(stem + a + "なかった", base_form, stem, type, "なかった"));
  forms.push_back(makeForm(stem + a + "れる", base_form, stem, type, "れる"));
  forms.push_back(makeForm(stem + a + "せる", base_form, stem, type, "せる"));

  // 連用形 + ます系
  forms.push_back(makeForm(stem + i + "ます", base_form, stem, type, "ます"));
  forms.push_back(makeForm(stem + i + "ました", base_form, stem, type, "ました"));
  forms.push_back(makeForm(stem + i + "ません", base_form, stem, type, "ません"));

  // 仮定形
  forms.push_back(makeForm(stem + e + "ば", base_form, stem, type, "ば"));

  // 意志形
  forms.push_back(makeForm(stem + o + "う", base_form, stem, type, "う"));

  // 命令形
  forms.push_back(makeForm(stem + e, base_form, stem, type, ""));

  // 音便形 + た/て系 (サ行以外)
  if (!row.onbin.empty()) {
    forms.push_back(makeForm(stem + row.onbin + ta, base_form, stem, type, ta));
    forms.push_back(makeForm(stem + row.onbin + te, base_form, stem, type, te));
    forms.push_back(
        makeForm(stem + row.onbin + ta + "ら", base_form, stem, type, ta + "ら"));

    // て形 + 補助動詞
    forms.push_back(
        makeForm(stem + row.onbin + te + "いる", base_form, stem, type, te + "いる"));
    forms.push_back(
        makeForm(stem + row.onbin + te + "いた", base_form, stem, type, te + "いた"));
    forms.push_back(
        makeForm(stem + row.onbin + te + "います", base_form, stem, type, te + "います"));
    forms.push_back(
        makeForm(stem + row.onbin + te + "いました", base_form, stem, type, te + "いました"));
    forms.push_back(
        makeForm(stem + row.onbin + te + "おく", base_form, stem, type, te + "おく"));
    forms.push_back(
        makeForm(stem + row.onbin + te + "ある", base_form, stem, type, te + "ある"));
    forms.push_back(
        makeForm(stem + row.onbin + te + "しまう", base_form, stem, type, te + "しまう"));
  } else {
    // サ行 (音便なし)
    forms.push_back(makeForm(stem + i + "た", base_form, stem, type, "た"));
    forms.push_back(makeForm(stem + i + "て", base_form, stem, type, "て"));
    forms.push_back(
        makeForm(stem + i + "ている", base_form, stem, type, "ている"));
    forms.push_back(
        makeForm(stem + i + "ています", base_form, stem, type, "ています"));
  }

  // 可能形 (五段 → え段 + る)
  forms.push_back(makeForm(stem + e + "る", base_form, stem, type, "る"));

  return forms;
}

std::vector<ConjugatedForm> Conjugation::generateIchidan(const std::string& stem, const std::string& base_form) {
  std::vector<ConjugatedForm> forms;
  VerbType type = VerbType::Ichidan;

  forms.push_back(makeForm(base_form, base_form, stem, type, ""));
  forms.push_back(makeForm(stem + "ない", base_form, stem, type, "ない"));
  forms.push_back(makeForm(stem + "なかった", base_form, stem, type, "なかった"));
  forms.push_back(makeForm(stem + "ます", base_form, stem, type, "ます"));
  forms.push_back(makeForm(stem + "ました", base_form, stem, type, "ました"));
  forms.push_back(makeForm(stem + "ません", base_form, stem, type, "ません"));
  forms.push_back(makeForm(stem + "た", base_form, stem, type, "た"));
  forms.push_back(makeForm(stem + "て", base_form, stem, type, "て"));
  forms.push_back(makeForm(stem + "ている", base_form, stem, type, "ている"));
  forms.push_back(makeForm(stem + "ていた", base_form, stem, type, "ていた"));
  forms.push_back(makeForm(stem + "ています", base_form, stem, type, "ています"));
  forms.push_back(makeForm(stem + "ていました", base_form, stem, type, "ていました"));
  forms.push_back(makeForm(stem + "ておく", base_form, stem, type, "ておく"));
  forms.push_back(makeForm(stem + "てある", base_form, stem, type, "てある"));
  forms.push_back(makeForm(stem + "れば", base_form, stem, type, "れば"));
  forms.push_back(makeForm(stem + "よう", base_form, stem, type, "よう"));
  forms.push_back(makeForm(stem + "ろ", base_form, stem, type, "ろ"));
  forms.push_back(makeForm(stem + "られる", base_form, stem, type, "られる"));
  forms.push_back(makeForm(stem + "させる", base_form, stem, type, "させる"));

  return forms;
}

std::vector<ConjugatedForm> Conjugation::generateSuru(const std::string& stem, const std::string& base_form) {
  std::vector<ConjugatedForm> forms;
  VerbType type = VerbType::Suru;

  forms.push_back(makeForm(base_form, base_form, stem, type, ""));
  forms.push_back(makeForm(stem + "しない", base_form, stem, type, "しない"));
  forms.push_back(makeForm(stem + "しなかった", base_form, stem, type, "しなかった"));
  forms.push_back(makeForm(stem + "します", base_form, stem, type, "します"));
  forms.push_back(makeForm(stem + "しました", base_form, stem, type, "しました"));
  forms.push_back(makeForm(stem + "しません", base_form, stem, type, "しません"));
  forms.push_back(makeForm(stem + "した", base_form, stem, type, "した"));
  forms.push_back(makeForm(stem + "して", base_form, stem, type, "して"));
  forms.push_back(makeForm(stem + "している", base_form, stem, type, "している"));
  forms.push_back(makeForm(stem + "していた", base_form, stem, type, "していた"));
  forms.push_back(makeForm(stem + "しています", base_form, stem, type, "しています"));
  forms.push_back(makeForm(stem + "していました", base_form, stem, type, "していました"));
  forms.push_back(makeForm(stem + "すれば", base_form, stem, type, "すれば"));
  forms.push_back(makeForm(stem + "しよう", base_form, stem, type, "しよう"));
  forms.push_back(makeForm(stem + "しろ", base_form, stem, type, "しろ"));
  forms.push_back(makeForm(stem + "せよ", base_form, stem, type, "せよ"));
  forms.push_back(makeForm(stem + "される", base_form, stem, type, "される"));
  forms.push_back(makeForm(stem + "させる", base_form, stem, type, "させる"));
  forms.push_back(makeForm(stem + "できる", base_form, stem, type, "できる"));

  return forms;
}

std::vector<ConjugatedForm> Conjugation::generateKuru(const std::string& stem, const std::string& base_form) {
  std::vector<ConjugatedForm> forms;
  VerbType type = VerbType::Kuru;

  // 来る is special: 語幹が変化する (来→こ/き)
  forms.push_back(makeForm(base_form, base_form, stem, type, ""));
  forms.push_back(makeForm(stem + "こない", base_form, stem, type, "こない"));
  forms.push_back(makeForm(stem + "こなかった", base_form, stem, type, "こなかった"));
  forms.push_back(makeForm(stem + "きます", base_form, stem, type, "きます"));
  forms.push_back(makeForm(stem + "きました", base_form, stem, type, "きました"));
  forms.push_back(makeForm(stem + "きません", base_form, stem, type, "きません"));
  forms.push_back(makeForm(stem + "きた", base_form, stem, type, "きた"));
  forms.push_back(makeForm(stem + "きて", base_form, stem, type, "きて"));
  forms.push_back(makeForm(stem + "きている", base_form, stem, type, "きている"));
  forms.push_back(makeForm(stem + "きています", base_form, stem, type, "きています"));
  forms.push_back(makeForm(stem + "くれば", base_form, stem, type, "くれば"));
  forms.push_back(makeForm(stem + "こよう", base_form, stem, type, "こよう"));
  forms.push_back(makeForm(stem + "こい", base_form, stem, type, "こい"));
  forms.push_back(makeForm(stem + "こられる", base_form, stem, type, "こられる"));
  forms.push_back(makeForm(stem + "こさせる", base_form, stem, type, "こさせる"));

  return forms;
}

std::vector<ConjugatedForm> Conjugation::generateIAdjective(const std::string& stem, const std::string& base_form) {
  std::vector<ConjugatedForm> forms;
  VerbType type = VerbType::IAdjective;

  forms.push_back(makeForm(base_form, base_form, stem, type, ""));
  forms.push_back(makeForm(stem + "くない", base_form, stem, type, "くない"));
  forms.push_back(makeForm(stem + "くなかった", base_form, stem, type, "くなかった"));
  forms.push_back(makeForm(stem + "かった", base_form, stem, type, "かった"));
  forms.push_back(makeForm(stem + "くて", base_form, stem, type, "くて"));
  forms.push_back(makeForm(stem + "ければ", base_form, stem, type, "ければ"));
  forms.push_back(makeForm(stem + "く", base_form, stem, type, "く"));
  forms.push_back(makeForm(stem + "さ", base_form, stem, type, "さ"));
  forms.push_back(makeForm(stem + "そう", base_form, stem, type, "そう"));

  return forms;
}

}  // namespace grammar
