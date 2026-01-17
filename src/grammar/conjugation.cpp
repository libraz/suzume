/**
 * @file conjugation.cpp
 * @brief Japanese verb/adjective conjugation rules implementation
 */

#include "conjugation.h"

#include <algorithm>

#include "core/utf8_constants.h"

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

Conjugation::Conjugation() = default;

const std::unordered_map<VerbType, Conjugation::GodanRow>&
Conjugation::getGodanRows() {
  // Static initialization: thread-safe in C++11+
  // 五段動詞の各行の活用パターン
  // base_vowel: 終止形語尾 (く, ぐ, す...)
  // a_row: 未然形 (か, が, さ...)
  // i_row: 連用形 (き, ぎ, し...)
  // e_row: 仮定形・命令形 (け, げ, せ...)
  // o_row: 意志形 (こ, ご, そ...)
  // onbin: 音便形 (い, っ, ん, "" for さ行)
  // voiced_ta: 連用形+た が だ になるか
  static const std::unordered_map<VerbType, GodanRow> kGodanRows = {
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
  return kGodanRows;
}

const Conjugation::GodanRow* Conjugation::getGodanRow(VerbType type) {
  const auto& rows = getGodanRows();
  auto it = rows.find(type);
  return it != rows.end() ? &it->second : nullptr;
}

std::string Conjugation::getStem(const std::string& base_form, VerbType type) {
  if (base_form.empty()) {
    return "";
  }

  size_t len = base_form.size();
  if (len < core::kJapaneseCharBytes) {
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
      return base_form.substr(0, len - core::kJapaneseCharBytes);

    case VerbType::Suru:
      if (base_form == "する") {
        return "";
      }
      // Xする → X (remove する = 6 bytes)
      if (len >= core::kTwoJapaneseCharBytes) {
        return base_form.substr(0, len - core::kTwoJapaneseCharBytes);
      }
      return "";

    case VerbType::Kuru:
      // 来る → 来 (remove る = 3 bytes)
      return base_form.substr(0, len - core::kJapaneseCharBytes);

    default:
      return base_form.substr(0, len - core::kJapaneseCharBytes);
  }
}

VerbType Conjugation::detectType(const std::string& base_form) {
  if (base_form.empty() || base_form.size() < core::kJapaneseCharBytes) {
    return VerbType::Unknown;
  }

  // Check last character
  std::string last = base_form.substr(base_form.size() - core::kJapaneseCharBytes);

  // Special verbs
  if (base_form == "する") {
    return VerbType::Suru;
  }
  if (base_form == "来る" || base_form == "くる") {
    return VerbType::Kuru;
  }

  // サ変複合動詞
  if (base_form.size() >= core::kTwoJapaneseCharBytes &&
      base_form.substr(base_form.size() - core::kTwoJapaneseCharBytes) == "する") {
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
    if (base_form.size() >= core::kTwoJapaneseCharBytes) {
      std::string prev = base_form.substr(base_form.size() - core::kTwoJapaneseCharBytes, core::kJapaneseCharBytes);
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
  const GodanRow* row_ptr = getGodanRow(type);
  if (row_ptr == nullptr) {
    return forms;
  }

  const auto& row = *row_ptr;
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

std::vector<Conjugation::DictionarySuffix> Conjugation::getDictionarySuffixes(
    VerbType type) const {
  std::vector<DictionarySuffix> suffixes;

  switch (type) {
    case VerbType::Ichidan:
      // 一段動詞: 食べる → 食べ + suffix
      // Note: ます系 excluded (should split as 食べ + ます)
      suffixes = {
          {"る", false},        // Base: 食べる
          {"た", false},        // Past: 食べた
          {"て", false},        // Te-form: 食べて
          {"ない", false},      // Negative: 食べない
          {"ん", false},        // Contracted negative: 食べん (colloquial)
          {"なかった", false},  // Past negative: 食べなかった
          {"れば", false},      // Conditional: 食べれば
          {"たら", false},      // Conditional: 食べたら
          {"よう", false},      // Volitional: 食べよう
          {"ろ", false},        // Imperative: 食べろ
      };
      break;

    case VerbType::GodanKa:
    case VerbType::GodanGa:
    case VerbType::GodanSa:
    case VerbType::GodanTa:
    case VerbType::GodanNa:
    case VerbType::GodanBa:
    case VerbType::GodanMa:
    case VerbType::GodanRa:
    case VerbType::GodanWa: {
      const GodanRow* row_ptr = getGodanRow(type);
      if (row_ptr == nullptr) {
        return suffixes;
      }
      const auto& row = *row_ptr;

      std::string base = encodeUtf8(row.base_vowel);
      std::string a = encodeUtf8(row.a_row);
      std::string i = encodeUtf8(row.i_row);
      std::string e = encodeUtf8(row.e_row);
      std::string o = encodeUtf8(row.o_row);
      std::string ta = row.voiced_ta ? "だ" : "た";
      std::string te = row.voiced_ta ? "で" : "て";

      // Base form
      suffixes.push_back({base, false});
      // Renyokei (for compound usage)
      suffixes.push_back({i, false});

      // 音便形 + ta/te (サ行以外)
      if (!row.onbin.empty()) {
        suffixes.push_back({row.onbin + ta, false});           // Past: 書いた
        suffixes.push_back({row.onbin + te, false});           // Te-form: 書いて
        suffixes.push_back({row.onbin + ta + "ら", false});    // Conditional: 書いたら
      } else {
        // サ行 (no onbin)
        suffixes.push_back({i + "た", false});   // Past: 話した
        suffixes.push_back({i + "て", false});   // Te-form: 話して
      }

      // Negative forms
      suffixes.push_back({a + "ない", false});        // Negative: 書かない
      suffixes.push_back({a + "ん", false});          // Contracted: 書かん
      suffixes.push_back({a + "ぬ", false});          // Classical: 書かぬ
      suffixes.push_back({a + "なかった", false});    // Past negative: 書かなかった

      // Conditional
      suffixes.push_back({e + "ば", false});          // Conditional: 書けば

      // Volitional
      suffixes.push_back({o + "う", false});          // Volitional: 書こう

      // Imperative (exclude for Ka/Ga to avoid conflict with potential)
      if (type != VerbType::GodanKa && type != VerbType::GodanGa) {
        suffixes.push_back({e, false});              // Imperative: 待て
      }

      // Potential forms (五段 → え段 + る)
      suffixes.push_back({e + "る", true});           // Potential: 書ける
      suffixes.push_back({e + "ない", true});         // Potential neg: 書けない
      suffixes.push_back({e + "なかった", true});     // Potential neg past: 書けなかった
      break;
    }

    case VerbType::Suru:
      // サ変: する (MeCab-compatible: exclude split forms)
      // した → し + た, so exclude. But keep conditional/imperative.
      suffixes = {
          {"する", false},      // Base form
          {"すれば", false},    // Conditional
          {"しろ", false},      // Imperative
          {"せよ", false},      // Imperative (classical)
          {"しよう", false},    // Volitional
          {"せん", false},      // Contracted negative (colloquial)
          {"したら", false},    // Conditional past
      };
      break;

    case VerbType::Kuru:
      // カ変: 来る (irregular - stem changes: く/き/こ)
      // For hiragana くる, prefix with appropriate stem change
      suffixes = {
          {"くる", false},        // Base form
          {"きた", false},        // Past
          {"きて", false},        // Te-form
          {"こない", false},      // Negative
          {"こなかった", false},  // Past negative
          {"くれば", false},      // Conditional
          {"きたら", false},      // Conditional
          {"こよう", false},      // Volitional
          {"こい", false},        // Imperative
          {"こられる", false},    // Potential (formal)
          {"これる", false},      // Potential (colloquial)
      };
      break;

    default:
      break;
  }

  return suffixes;
}

std::string_view verbTypeToString(VerbType type) {
  switch (type) {
    case VerbType::Ichidan:
      return "ichidan";
    case VerbType::GodanKa:
      return "godan-ka";
    case VerbType::GodanGa:
      return "godan-ga";
    case VerbType::GodanSa:
      return "godan-sa";
    case VerbType::GodanTa:
      return "godan-ta";
    case VerbType::GodanNa:
      return "godan-na";
    case VerbType::GodanBa:
      return "godan-ba";
    case VerbType::GodanMa:
      return "godan-ma";
    case VerbType::GodanRa:
      return "godan-ra";
    case VerbType::GodanWa:
      return "godan-wa";
    case VerbType::Suru:
      return "suru";
    case VerbType::Kuru:
      return "kuru";
    case VerbType::IAdjective:
      return "i-adj";
    case VerbType::Unknown:
    default:
      return "";
  }
}

std::string_view verbTypeToJapanese(VerbType type) {
  switch (type) {
    case VerbType::Ichidan:
      return "一段";
    case VerbType::GodanKa:
      return "五段・カ行";
    case VerbType::GodanGa:
      return "五段・ガ行";
    case VerbType::GodanSa:
      return "五段・サ行";
    case VerbType::GodanTa:
      return "五段・タ行";
    case VerbType::GodanNa:
      return "五段・ナ行";
    case VerbType::GodanBa:
      return "五段・バ行";
    case VerbType::GodanMa:
      return "五段・マ行";
    case VerbType::GodanRa:
      return "五段・ラ行";
    case VerbType::GodanWa:
      return "五段・ワ行";
    case VerbType::Suru:
      return "サ変";
    case VerbType::Kuru:
      return "カ変";
    case VerbType::IAdjective:
      return "形容詞";
    case VerbType::Unknown:
    default:
      return "";
  }
}

std::string_view conjFormToString(ConjForm form) {
  switch (form) {
    case ConjForm::Base:
      return "base";
    case ConjForm::Mizenkei:
      return "mizenkei";
    case ConjForm::Renyokei:
      return "renyokei";
    case ConjForm::Onbinkei:
      return "onbinkei";
    case ConjForm::Kateikei:
      return "kateikei";
    case ConjForm::Meireikei:
      return "meireikei";
    case ConjForm::Ishikei:
      return "ishikei";
    default:
      return "";
  }
}

std::string_view conjFormToJapanese(ConjForm form) {
  switch (form) {
    case ConjForm::Base:
      return "終止形";
    case ConjForm::Mizenkei:
      return "未然形";
    case ConjForm::Renyokei:
      return "連用形";
    case ConjForm::Onbinkei:
      return "連用形";
    case ConjForm::Kateikei:
      return "仮定形";
    case ConjForm::Meireikei:
      return "命令形";
    case ConjForm::Ishikei:
      return "意志形";
    default:
      return "";
  }
}

}  // namespace grammar
