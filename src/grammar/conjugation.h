/**
 * @file conjugation.h
 * @brief Japanese verb/adjective conjugation rules as logic
 *
 * Design: Rule-based conjugation generation
 * - VerbType determines conjugation pattern
 * - ConjugationForm determines which suffix to apply
 * - Onbin (音便) rules handled automatically
 */

#ifndef SUZUME_GRAMMAR_CONJUGATION_H_
#define SUZUME_GRAMMAR_CONJUGATION_H_

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "dictionary/dictionary.h"

namespace suzume::grammar {

/**
 * @brief Verb conjugation type (活用型)
 */
enum class VerbType : uint8_t {
  Unknown = 0,
  Ichidan,    // 一段: 食べる、見る
  GodanKa,    // 五段か行: 書く
  GodanGa,    // 五段が行: 泳ぐ
  GodanSa,    // 五段さ行: 話す
  GodanTa,    // 五段た行: 持つ
  GodanNa,    // 五段な行: 死ぬ
  GodanBa,    // 五段ば行: 遊ぶ
  GodanMa,    // 五段ま行: 読む
  GodanRa,    // 五段ら行: 取る
  GodanWa,    // 五段わ行: 買う
  Suru,       // サ変: する
  Kuru,       // カ変: 来る
  IAdjective, // い形容詞: 高い
};

/**
 * @brief Conjugation form (活用形)
 */
enum class ConjForm : uint8_t {
  Base = 0,     // 終止形/基本形: 書く
  Mizenkei,     // 未然形: 書か
  Renyokei,     // 連用形: 書き
  Onbinkei,     // 音便形: 書い
  Kateikei,     // 仮定形: 書け
  Meireikei,    // 命令形: 書け
  Ishikei,      // 意志形: 書こ
};

/**
 * @brief Generated conjugation form
 */
struct ConjugatedForm {
  std::string surface;    // 活用形: 書いた
  std::string base_form;  // 基本形: 書く
  std::string stem;       // 語幹: 書
  VerbType verb_type;     // 活用型
  std::string suffix;     // 付加語尾: た
};

/**
 * @brief Conjugation engine
 *
 * Generates all conjugated forms from base form + verb type
 */
class Conjugation {
 public:
  Conjugation();

  /**
   * @brief Generate all conjugated forms for a verb
   * @param base_form Base form (終止形): 書く
   * @param type Verb type: GodanKa
   * @return All conjugated surface forms with info
   */
  std::vector<ConjugatedForm> generate(const std::string& base_form,
                                       VerbType type) const;

  /**
   * @brief Get verb stem from base form
   */
  static std::string getStem(const std::string& base_form, VerbType type);

  /**
   * @brief Detect verb type from base form (heuristic)
   */
  static VerbType detectType(const std::string& base_form);

 private:
  // 五段活用の行ごとのパターン
  struct GodanRow {
    char32_t base_vowel;    // 終止形母音: く
    char32_t a_row;         // あ段: か
    char32_t i_row;         // い段: き
    char32_t e_row;         // え段: け
    char32_t o_row;         // お段: こ
    std::string onbin;      // 音便形: い, っ, ん
    bool voiced_ta;         // た→だ: true for が/な/ば/ま行
  };

  std::unordered_map<VerbType, GodanRow> godan_rows_;

  void initGodanRows();
  std::vector<ConjugatedForm> generateGodan(const std::string& stem,
                                            const std::string& base_form,
                                            VerbType type) const;
  static std::vector<ConjugatedForm> generateIchidan(const std::string& stem, const std::string& base_form);
  static std::vector<ConjugatedForm> generateSuru(const std::string& stem, const std::string& base_form);
  static std::vector<ConjugatedForm> generateKuru(const std::string& stem, const std::string& base_form);
  static std::vector<ConjugatedForm> generateIAdjective(const std::string& stem, const std::string& base_form);
};

/**
 * @brief Convert ConjugationType to VerbType
 */
inline VerbType conjTypeToVerbType(dictionary::ConjugationType conj_type) {
  switch (conj_type) {
    case dictionary::ConjugationType::None:
      return VerbType::Unknown;
    case dictionary::ConjugationType::Ichidan:
      return VerbType::Ichidan;
    case dictionary::ConjugationType::GodanKa:
      return VerbType::GodanKa;
    case dictionary::ConjugationType::GodanGa:
      return VerbType::GodanGa;
    case dictionary::ConjugationType::GodanSa:
      return VerbType::GodanSa;
    case dictionary::ConjugationType::GodanTa:
      return VerbType::GodanTa;
    case dictionary::ConjugationType::GodanNa:
      return VerbType::GodanNa;
    case dictionary::ConjugationType::GodanBa:
      return VerbType::GodanBa;
    case dictionary::ConjugationType::GodanMa:
      return VerbType::GodanMa;
    case dictionary::ConjugationType::GodanRa:
      return VerbType::GodanRa;
    case dictionary::ConjugationType::GodanWa:
      return VerbType::GodanWa;
    case dictionary::ConjugationType::Suru:
      return VerbType::Suru;
    case dictionary::ConjugationType::Kuru:
      return VerbType::Kuru;
    case dictionary::ConjugationType::IAdjective:
      return VerbType::IAdjective;
    case dictionary::ConjugationType::NaAdjective:
      return VerbType::Unknown;  // VerbType doesn't have NaAdjective
  }
  return VerbType::Unknown;
}

/**
 * @brief Convert VerbType to string (English)
 */
std::string_view verbTypeToString(VerbType type);

/**
 * @brief Convert VerbType to Japanese string
 */
std::string_view verbTypeToJapanese(VerbType type);

/**
 * @brief Convert ConjForm to string (English)
 */
std::string_view conjFormToString(ConjForm form);

/**
 * @brief Convert ConjForm to Japanese string
 */
std::string_view conjFormToJapanese(ConjForm form);

/**
 * @brief Convert VerbType to ConjugationType
 */
inline dictionary::ConjugationType verbTypeToConjType(VerbType verb_type) {
  switch (verb_type) {
    case VerbType::Unknown:
      return dictionary::ConjugationType::None;
    case VerbType::Ichidan:
      return dictionary::ConjugationType::Ichidan;
    case VerbType::GodanKa:
      return dictionary::ConjugationType::GodanKa;
    case VerbType::GodanGa:
      return dictionary::ConjugationType::GodanGa;
    case VerbType::GodanSa:
      return dictionary::ConjugationType::GodanSa;
    case VerbType::GodanTa:
      return dictionary::ConjugationType::GodanTa;
    case VerbType::GodanNa:
      return dictionary::ConjugationType::GodanNa;
    case VerbType::GodanBa:
      return dictionary::ConjugationType::GodanBa;
    case VerbType::GodanMa:
      return dictionary::ConjugationType::GodanMa;
    case VerbType::GodanRa:
      return dictionary::ConjugationType::GodanRa;
    case VerbType::GodanWa:
      return dictionary::ConjugationType::GodanWa;
    case VerbType::Suru:
      return dictionary::ConjugationType::Suru;
    case VerbType::Kuru:
      return dictionary::ConjugationType::Kuru;
    case VerbType::IAdjective:
      return dictionary::ConjugationType::IAdjective;
  }
  return dictionary::ConjugationType::None;
}

}  // namespace suzume::grammar
#endif  // SUZUME_GRAMMAR_CONJUGATION_H_
