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

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/types.h"
#include "dictionary/dictionary.h"

namespace suzume::grammar {

/**
 * @brief Verb conjugation type (活用型)
 */
enum class VerbType : uint8_t {
  Unknown = 0,
  Ichidan,     // 一段: 食べる、見る
  GodanKa,     // 五段か行: 書く
  GodanGa,     // 五段が行: 泳ぐ
  GodanSa,     // 五段さ行: 話す
  GodanTa,     // 五段た行: 持つ
  GodanNa,     // 五段な行: 死ぬ
  GodanBa,     // 五段ば行: 遊ぶ
  GodanMa,     // 五段ま行: 読む
  GodanRa,     // 五段ら行: 取る
  GodanWa,     // 五段わ行: 買う
  Suru,        // サ変: する
  Kuru,        // カ変: 来る
  IAdjective,  // い形容詞: 高い
};

using GodanOnbinEntry = std::pair<VerbType, std::string_view>;

/**
 * @brief Non-owning view over a fixed Godan onbin candidate table.
 */
class GodanOnbinRange {
 public:
  constexpr GodanOnbinRange(const GodanOnbinEntry* data, size_t size) : data_(data), size_(size) {}

  constexpr const GodanOnbinEntry* begin() const { return data_; }
  constexpr const GodanOnbinEntry* end() const { return data_ + size_; }
  constexpr size_t size() const { return size_; }
  constexpr bool empty() const { return size_ == 0; }

 private:
  const GodanOnbinEntry* data_;
  size_t size_;
};

/**
 * @brief Conjugation form (活用形)
 */
enum class ConjForm : uint8_t {
  Base = 0,   // 終止形/基本形: 書く
  Mizenkei,   // 未然形: 書か
  Renyokei,   // 連用形: 書き
  Onbinkei,   // 音便形: 書い
  Kateikei,   // 仮定形: 書け
  Meireikei,  // 命令形: 書け
  Ishikei,    // 意志形: 書こ
  Count_,
};

/**
 * @brief Convert a lattice ExtendedPOS decision to its conjugation form.
 *
 * Returns Count_ when the ExtendedPOS does not encode an inflection cell.
 * The next morpheme disambiguates modern volition from ordinary mizenkei.
 */
ConjForm conjFormFromExtendedPos(core::ExtendedPOS extended_pos,
                                 core::ExtendedPOS next_extended_pos = core::ExtendedPOS::Unknown,
                                 std::string_view next_lemma = {});

static_assert(static_cast<uint8_t>(ConjForm::Base) == 0 && static_cast<uint8_t>(ConjForm::Mizenkei) == 1 &&
                  static_cast<uint8_t>(ConjForm::Renyokei) == 2 && static_cast<uint8_t>(ConjForm::Onbinkei) == 3 &&
                  static_cast<uint8_t>(ConjForm::Kateikei) == 4 && static_cast<uint8_t>(ConjForm::Meireikei) == 5 &&
                  static_cast<uint8_t>(ConjForm::Ishikei) == 6 && static_cast<uint8_t>(ConjForm::Count_) == 7,
              "ConjForm ABI values must remain stable");

/**
 * @brief Conjugation engine
 */
class Conjugation {
 public:
  Conjugation();

  /**
   * @brief Godan conjugation row data (五段活用の行パターン)
   *
   * Each row contains all vowel variants for a Godan verb type.
   * Used for reverse inflection analysis and verb ending generation.
   */
  struct GodanRow {
    char32_t base_vowel;     // 終止形母音: く, ぐ, す, etc.
    char32_t a_row;          // あ段 (未然形): か, が, さ, etc.
    char32_t i_row;          // い段 (連用形): き, ぎ, し, etc.
    char32_t e_row;          // え段 (仮定/命令形): け, げ, せ, etc.
    char32_t o_row;          // お段 (意志形): こ, ご, そ, etc.
    std::string_view onbin;  // 音便形: い, っ, ん, "" (empty for サ行)
    bool voiced_ta;          // た→だ: true for が/な/ば/ま行
  };

  using GodanEntry = std::pair<VerbType, GodanRow>;
  static constexpr size_t kGodanRowCount = 9;

  /**
   * @brief Get Godan row data for a specific verb type
   * @param type Verb type (must be GodanKa through GodanWa)
   * @return Pointer to GodanRow, or nullptr if not a Godan type
   */
  static const GodanRow* getGodanRow(VerbType type);

  /**
   * @brief Get all Godan rows in deterministic verb-type order
   * @return Const reference to the fixed VerbType/GodanRow table
   */
  static const std::array<GodanEntry, kGodanRowCount>& getGodanRows();

  /**
   * @brief Get Godan verb types that use a specific onbin pattern.
   *
   * Onbin patterns:
   * - "い" (ikuon) -> GodanKa, GodanGa
   * - "っ" (sokuon) -> GodanKa (行く irregular), GodanRa, GodanTa, GodanWa
   * - "ん" (hatsuonbin) -> GodanMa, GodanBa, GodanNa
   * - "" (none) -> GodanSa
   * - "う" (u-onbin) -> lexical GodanWa subclass (問う, 請う, ...)
   *
   * @param onbin Onbin pattern to match ("い", "っ", "ん", or "")
   * @return Non-owning range of (VerbType, base suffix) pairs in deterministic preference order.
   */
  static GodanOnbinRange getGodanTypesByOnbin(std::string_view onbin);

  /**
   * @brief Suffix info for dictionary expansion
   */
  struct DictionarySuffix {
    std::string suffix;              // Suffix to add to stem: った
    bool is_potential;               // True if this is a potential form
    core::ExtendedPOS extended_pos;  // ExtendedPOS for this form
  };

  /**
   * @brief Generate lexical suffixes for dictionary expansion
   *
   * Returns only forms that should NOT be split by MeCab:
   * - Excludes ます系 (should be split as 連用形 + ます)
   * - Excludes compound forms like ている (should be split)
   * - Includes colloquial contractions (ん negative)
   *
   * @param type Verb type
   * @return Vector of suffixes to generate dictionary entries
   */
  std::vector<DictionarySuffix> getDictionarySuffixes(VerbType type, std::string_view base_form = {}) const;

  /**
   * @brief Get verb stem from base form
   */
  static std::string getStem(const std::string& base_form, VerbType type);

  /**
   * @brief Detect verb type from base form (heuristic)
   */
  static VerbType detectType(const std::string& base_form);
};

/**
 * @brief UTF-8 encoded vowel-row surfaces for a Godan verb row.
 *
 * Holds the terminal (base) and あ/い/え/お-row kana of a Godan row as UTF-8
 * strings, ready for stem concatenation.
 */
struct GodanVowels {
  std::string base;  // 終止形母音: く, ぐ, す...
  std::string a;     // あ段 (未然形): か, が, さ...
  std::string i;     // い段 (連用形): き, ぎ, し...
  std::string e;     // え段 (仮定/命令形): け, げ, せ...
  std::string o;     // お段 (意志形): こ, ご, そ...
};

/**
 * @brief Canonical stem surfaces for the irregular カ変 verb.
 *
 * Kana and kanji spellings use different visible stems while sharing the same
 * inflection slots. Both the full-form generator and stem generator consume
 * this record to keep their output in sync. The old kanji spelling 來る is a
 * third spelling of the same paradigm, not a separate verb, and it keeps its
 * own lemma the way the kana spelling does.
 */
struct KuruStemForms {
  std::string base;
  std::string mizenkei;
  std::string renyokei;
  std::string onbinkei;
  std::string kateikei;
  std::string ishikei;
  std::string meireikei;
};

/**
 * @brief Return the canonical カ変 stem surfaces for a base form.
 */
KuruStemForms getKuruStemForms(const std::string& base_form);

/**
 * @brief Dictionary form surfaces for both spellings of カ変.
 */
struct KuruDictionaryForm {
  std::string kanji_surface;
  std::string old_kanji_surface;
  std::string kana_surface;
  core::ExtendedPOS extended_pos;
  bool emit_kanji{true};
  bool emit_kana{true};
};

/**
 * @brief Derive dictionary forms for カ変 from the canonical stem surfaces.
 */
std::vector<KuruDictionaryForm> getKuruDictionaryForms();

/**
 * @brief Whether a reverse-analysis stem can belong to カ変.
 *
 * The kana spelling has an empty lexical stem because its visible こ/き/くれ
 * sequence is carried by the ending. A kanji spelling keeps its own character
 * as the stem.
 */
bool isKuruStem(std::string_view stem);

/**
 * @brief Whether a codepoint is the single-kanji stem of カ変.
 *
 * The kanji spellings of カ変 inflect on one visible character, so candidate
 * generation asks about the codepoint rather than the surface. Both the modern
 * 来 and the old 來 answer yes; keeping the question here is what stops the set
 * from being re-listed at every generation site.
 */
bool isKuruKanjiStem(char32_t code);

/**
 * @brief Whether a base form is a kanji spelling of カ変.
 *
 * The kana spelling is deliberately excluded: rules that ask this question are
 * about the unambiguous kanji lemma, and admitting くる would widen them.
 */
bool isKuruKanjiBaseForm(std::string_view base_form);

/**
 * @brief The カ変 base form belonging to a single-kanji stem.
 *
 * A candidate generated off the stem carries the lemma of the spelling it was
 * read from, so an old-form surface does not report a modern-form lemma.
 */
std::string kuruBaseFormOf(char32_t kanji_stem);

/**
 * @brief Whether a base form is the irregular 促音便 verb 行く/いく.
 */
bool isIkuBaseForm(std::string_view base_form);

/**
 * @brief Whether a Godan-Ka stem is the irregular 促音便 stem 行/い.
 */
bool isIkuStem(std::string_view stem);

/**
 * @brief Whether a GodanWa stem has the lexical う音便 (問うた), not 促音便.
 *
 * Most ワ行五段 verbs use 促音便 (買った).  A small closed lexical subclass
 * instead retains う before た/て; keep that distinction in the shared
 * conjugation layer so generation and reverse analysis agree.
 */
bool isUOnbinStem(std::string_view stem);

/**
 * @brief Encode a Godan row's vowel codepoints into UTF-8 strings.
 * @param row Godan row data
 * @return GodanVowels with base/a/i/e/o surfaces
 */
GodanVowels encodeGodanVowels(const Conjugation::GodanRow& row);

/**
 * @brief The onbinkei (音便形) kana surface for a Godan row.
 *
 * Returns the row's explicit onbin surface (い/っ/ん), or — for サ行, which has
 * no real onbin — the い段 (連用形) kana that doubles as the onbinkei form.
 *
 * @param row Godan row data
 * @return UTF-8 onbinkei surface
 */
std::string onbinFormOf(const Conjugation::GodanRow& row);

/**
 * @brief Return a Godan verb's onbin surface, including 行く/いく irregularity.
 *
 * Normal Godan-Ka uses イ音便 (書い), while the lexical stems 行/い use
 * 促音便 (行っ/いっ). Other rows use their canonical GodanRow value.
 */
std::string godanOnbinForm(VerbType type, std::string_view stem);

/**
 * @brief Whether a verb type is one of the nine Godan conjugation rows.
 *
 * Derived from Conjugation::getGodanRow() so the set stays in sync with the
 * single Godan-row source of truth.
 *
 * @param type Verb type to test
 * @return True if @p type has a Godan row
 */
bool isGodanVerbType(VerbType type);

// The verb/adjective portion of ConjugationType deliberately shares VerbType's
// serialized values. Keep this boundary checked here, rather than maintaining
// two mirrored switches in every consumer TU.
static_assert(static_cast<uint8_t>(dictionary::ConjugationType::None) == static_cast<uint8_t>(VerbType::Unknown));
static_assert(static_cast<uint8_t>(dictionary::ConjugationType::IAdjective) ==
              static_cast<uint8_t>(VerbType::IAdjective));

/**
 * @brief Convert ConjugationType to VerbType
 */
constexpr VerbType conjTypeToVerbType(dictionary::ConjugationType conj_type) {
  const uint8_t type_value = static_cast<uint8_t>(conj_type);
  return type_value <= static_cast<uint8_t>(VerbType::IAdjective) ? static_cast<VerbType>(type_value)
                                                                  : VerbType::Unknown;
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
constexpr dictionary::ConjugationType verbTypeToConjType(VerbType verb_type) {
  const uint8_t type_value = static_cast<uint8_t>(verb_type);
  return type_value <= static_cast<uint8_t>(VerbType::IAdjective) ? static_cast<dictionary::ConjugationType>(type_value)
                                                                  : dictionary::ConjugationType::None;
}

}  // namespace suzume::grammar
#endif  // SUZUME_GRAMMAR_CONJUGATION_H_
