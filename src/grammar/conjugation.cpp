/**
 * @file conjugation.cpp
 * @brief Japanese verb/adjective conjugation rules implementation
 */

#include "conjugation.h"

#include <iterator>

#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "normalize/utf8.h"

namespace suzume::grammar {

using normalize::encodeUtf8;

ConjForm conjFormFromExtendedPos(core::ExtendedPOS extended_pos, core::ExtendedPOS next_extended_pos,
                                 std::string_view next_lemma) {
  using core::ExtendedPOS;

  switch (extended_pos) {
    case ExtendedPOS::VerbShuushikei:
    case ExtendedPOS::VerbRentaikei:
    case ExtendedPOS::AdjBasic:
    case ExtendedPOS::AdjNaAdj:
      return ConjForm::Base;
    case ExtendedPOS::VerbRenyokei:
    case ExtendedPOS::AdjRenyokei:
    case ExtendedPOS::AdjStem:
      return ConjForm::Renyokei;
    case ExtendedPOS::VerbMizenkei:
      return next_extended_pos == ExtendedPOS::AuxVolitional && next_lemma == "う" ? ConjForm::Ishikei
                                                                                   : ConjForm::Mizenkei;
    case ExtendedPOS::AdjMizenkei:
      return ConjForm::Mizenkei;
    case ExtendedPOS::VerbOnbinkei:
    case ExtendedPOS::VerbTeForm:
    case ExtendedPOS::VerbTaForm:
    case ExtendedPOS::VerbTaraForm:
    case ExtendedPOS::AdjKatt:
      return ConjForm::Onbinkei;
    case ExtendedPOS::VerbKateikei:
    case ExtendedPOS::VerbContractedKateikei:
    case ExtendedPOS::AdjKeForm:
      return ConjForm::Kateikei;
    case ExtendedPOS::VerbMeireikei:
      return ConjForm::Meireikei;
    default:
      return ConjForm::Count_;
  }
}

Conjugation::Conjugation() = default;

const std::array<Conjugation::GodanEntry, Conjugation::kGodanRowCount>& Conjugation::getGodanRows() {
  // Conjugation cells of each Godan row.
  // base_vowel: 終止形語尾 (く, ぐ, す...)
  // a_row: 未然形 (か, が, さ...)
  // i_row: 連用形 (き, ぎ, し...)
  // e_row: 仮定形・命令形 (け, げ, せ...)
  // o_row: 意志形 (こ, ご, そ...)
  // onbin: 音便形 (い, っ, ん, "" for さ行)
  // voiced_ta: whether 連用形+た voices to だ
  static constexpr std::array<GodanEntry, kGodanRowCount> kGodanRows = {{
      {VerbType::GodanKa, {U'く', U'か', U'き', U'け', U'こ', "い", false}},
      {VerbType::GodanGa, {U'ぐ', U'が', U'ぎ', U'げ', U'ご', "い", true}},
      {VerbType::GodanSa, {U'す', U'さ', U'し', U'せ', U'そ', "", false}},
      {VerbType::GodanTa, {U'つ', U'た', U'ち', U'て', U'と', "っ", false}},
      {VerbType::GodanNa, {U'ぬ', U'な', U'に', U'ね', U'の', "ん", true}},
      {VerbType::GodanBa, {U'ぶ', U'ば', U'び', U'べ', U'ぼ', "ん", true}},
      {VerbType::GodanMa, {U'む', U'ま', U'み', U'め', U'も', "ん", true}},
      {VerbType::GodanRa, {U'る', U'ら', U'り', U'れ', U'ろ', "っ", false}},
      {VerbType::GodanWa, {U'う', U'わ', U'い', U'え', U'お', "っ", false}},
  }};
  return kGodanRows;
}

const Conjugation::GodanRow* Conjugation::getGodanRow(VerbType type) {
  for (const auto& [row_type, row] : getGodanRows()) {
    if (row_type == type) {
      return &row;
    }
  }
  return nullptr;
}

GodanOnbinRange Conjugation::getGodanTypesByOnbin(std::string_view onbin) {
  static constexpr std::array<GodanOnbinEntry, 2> kIOnbin = {{{VerbType::GodanKa, "く"}, {VerbType::GodanGa, "ぐ"}}};
  // 行く has irregular 促音便 (行っ), while normal GodanKa uses イ音便.
  static constexpr std::array<GodanOnbinEntry, 4> kSokuonbin = {
      {{VerbType::GodanKa, "く"}, {VerbType::GodanRa, "る"}, {VerbType::GodanTa, "つ"}, {VerbType::GodanWa, "う"}}};
  static constexpr std::array<GodanOnbinEntry, 3> kHatsuonbin = {
      {{VerbType::GodanMa, "む"}, {VerbType::GodanBa, "ぶ"}, {VerbType::GodanNa, "ぬ"}}};
  static constexpr std::array<GodanOnbinEntry, 1> kSaOnbin = {{{VerbType::GodanSa, "す"}}};
  static constexpr std::array<GodanOnbinEntry, 1> kUOnbin = {{{VerbType::GodanWa, "う"}}};
  if (onbin == "い") {
    return {kIOnbin.data(), kIOnbin.size()};
  }
  if (onbin == "っ") {
    return {kSokuonbin.data(), kSokuonbin.size()};
  }
  if (onbin == "ん") {
    return {kHatsuonbin.data(), kHatsuonbin.size()};
  }
  if (onbin.empty()) {
    return {kSaOnbin.data(), kSaOnbin.size()};
  }
  if (onbin == "う") {
    return {kUOnbin.data(), kUOnbin.size()};
  }
  return {kIOnbin.data(), 0};
}

GodanVowels encodeGodanVowels(const Conjugation::GodanRow& row) {
  return {encodeUtf8(row.base_vowel), encodeUtf8(row.a_row), encodeUtf8(row.i_row), encodeUtf8(row.e_row),
          encodeUtf8(row.o_row)};
}

namespace {

// カ変 is one irregular paradigm written three ways. The two kanji spellings
// are a closed lexical set — the old form 來る is still current in classical
// and pre-reform text — while every inflected cell is derived from them.
constexpr std::string_view kKuruKanjiBaseForms[] = {"来る", "來る"};

}  // namespace

KuruStemForms getKuruStemForms(const std::string& base_form) {
  // Every kanji spelling of カ変 inflects on a visible stem, so its cells
  // follow from dropping る. Only the membership of the set is lexical: the
  // old spelling 來る is the same verb, and the reference analyzer keeps it
  // under its own lemma rather than folding it onto the modern one.
  for (const std::string_view kanji_base : kKuruKanjiBaseForms) {
    if (base_form != kanji_base) {
      continue;
    }
    const std::string stem(normalize::utf8Substr(base_form, 0, normalize::utf8Length(base_form) - 1));
    return {base_form,
            stem,
            stem,
            stem,
            normalize::concat(stem, "れ"),
            normalize::concat(stem, "よ"),
            normalize::concat(stem, "い")};
  }
  // The kana spelling has an empty lexical stem: its こ/き/くれ sequence is
  // carried by the ending rather than by a visible stem.
  return {base_form, "こ", "き", "き", "くれ", "こよ", "こい"};
}

std::vector<KuruDictionaryForm> getKuruDictionaryForms() {
  const KuruStemForms kanji = getKuruStemForms("来る");
  const KuruStemForms old_kanji = getKuruStemForms("來る");
  const KuruStemForms kana = getKuruStemForms("くる");
  return {
      {kanji.base, old_kanji.base, kana.base, core::ExtendedPOS::VerbShuushikei},
      {kanji.renyokei, old_kanji.renyokei, kana.renyokei, core::ExtendedPOS::VerbRenyokei},
      {kanji.mizenkei, old_kanji.mizenkei, kana.mizenkei, core::ExtendedPOS::VerbMizenkei},
      {kanji.kateikei, old_kanji.kateikei, kana.kateikei, core::ExtendedPOS::VerbKateikei},
      {kanji.ishikei, old_kanji.ishikei, kana.ishikei, core::ExtendedPOS::VerbMizenkei},
      {kanji.meireikei, old_kanji.meireikei, kana.meireikei, core::ExtendedPOS::VerbMeireikei},
      // Standard potential/passive is a mizenkei + auxiliary chain (来+られる),
      // segmented like the causative below, so neither spelling becomes a
      // whole-word dictionary edge.
      {kanji.mizenkei + "られる", old_kanji.mizenkei + "られる", kana.mizenkei + "られる",
       core::ExtendedPOS::VerbShuushikei, /*emit_kanji=*/false, /*emit_kana=*/false},
      // The colloquial ra-nuki potential is a lexical terminal form. Its
      // kanji spelling is safe as a dictionary entry; its kana spelling is
      // generated as a context-gated irregular candidate to avoid reopening
      // demonstrative compounds such as これより.
      {kanji.mizenkei + "れる", old_kanji.mizenkei + "れる", kana.mizenkei + "れる", core::ExtendedPOS::VerbShuushikei,
       /*emit_kanji=*/true, /*emit_kana=*/false},
      // Causative is always segmented as the Kuru mizenkei plus させる. Keep
      // its surface in the canonical paradigm without creating a competing
      // whole-word dictionary edge.
      {kanji.mizenkei + "させる", old_kanji.mizenkei + "させる", kana.mizenkei + "させる",
       core::ExtendedPOS::VerbShuushikei, /*emit_kanji=*/false, /*emit_kana=*/false},
  };
}

bool isKuruStem(std::string_view stem) {
  if (stem.empty()) {
    return true;
  }
  for (const std::string_view kanji_base : kKuruKanjiBaseForms) {
    if (kanji_base.substr(0, kanji_base.size() - core::kJapaneseCharBytes) == stem) {
      return true;
    }
  }
  return false;
}

bool isKuruKanjiStem(char32_t code) {
  for (const std::string_view kanji_base : kKuruKanjiBaseForms) {
    size_t pos = 0;
    if (normalize::decodeUtf8(kanji_base, pos) == code) {
      return true;
    }
  }
  return false;
}

bool isKuruKanjiBaseForm(std::string_view base_form) {
  return utf8::equalsAny(base_form, kKuruKanjiBaseForms);
}

std::string kuruBaseFormOf(char32_t kanji_stem) {
  return normalize::concat(encodeUtf8(kanji_stem), "る");
}

bool isIkuStem(std::string_view stem) {
  return stem == "行" || stem == "い";
}

bool admitsSokuonbin(VerbType verb_type, std::string_view base_form) {
  return verb_type != VerbType::GodanKa || utf8::endsWith(base_form, "行く") || utf8::endsWith(base_form, "いく");
}

bool isUOnbinStem(std::string_view stem) {
  // Godan-Wa u-onbin is a closed lexical subclass, not a productive rule.
  // This list is the same kind of lexical irregularity as 行く's 促音便;
  // callers must not infer it from the final vowel alone.
  static constexpr std::string_view kUOnbinStems[] = {
      "問", "請", "乞", "厭", "慕", "添", "訪",
  };
  return utf8::equalsAny(stem, kUOnbinStems);
}

std::string onbinFormOf(const Conjugation::GodanRow& row) {
  // サ行 has no real onbin; its 連用形 (い段) doubles as the onbinkei form
  // (話し + た). Every other row has an explicit onbin surface (い/っ/ん).
  return row.onbin.empty() ? encodeUtf8(row.i_row) : std::string(row.onbin);
}

std::string godanOnbinForm(VerbType type, std::string_view stem) {
  if (type == VerbType::GodanKa && isIkuStem(stem)) {
    return "っ";
  }
  if (type == VerbType::GodanWa && isUOnbinStem(stem)) {
    return "う";
  }
  const Conjugation::GodanRow* row = Conjugation::getGodanRow(type);
  return row == nullptr ? "" : onbinFormOf(*row);
}

bool isGodanVerbType(VerbType type) {
  return Conjugation::getGodanRow(type) != nullptr;
}

std::string Conjugation::getStem(const std::string& base_form, VerbType type) {
  if (base_form.empty()) {
    return "";
  }

  size_t len = base_form.size();
  if (len < core::kJapaneseCharBytes) {
    return base_form;
  }

  // Suru is the only type whose stem is not "base minus its final kana":
  // Xする drops する (two chars) and bare する has an empty stem.
  if (type == VerbType::Suru) {
    if (base_form == "する") {
      return "";
    }
    // Xする → X (remove する = 6 bytes)
    if (len >= core::kTwoJapaneseCharBytes) {
      return base_form.substr(0, len - core::kTwoJapaneseCharBytes);
    }
    return "";
  }

  // Every other type (Ichidan, all Godan rows, IAdjective, Kuru) drops its
  // final kana (3 bytes in UTF-8).
  return base_form.substr(0, len - core::kJapaneseCharBytes);
}

VerbType Conjugation::detectType(const std::string& base_form) {
  if (base_form.size() < core::kJapaneseCharBytes) {
    return VerbType::Unknown;
  }

  const std::string_view last = utf8::lastChar(base_form);

  // Special verbs
  if (base_form == "する") {
    return VerbType::Suru;
  }
  if (utf8::equalsAny(base_form, {"来る", "くる"})) {
    return VerbType::Kuru;
  }

  // サ変複合動詞
  if (utf8::endsWith(base_form, "する")) {
    return VerbType::Suru;
  }

  // い形容詞
  if (last == "い") {
    return VerbType::IAdjective;
  }

  // An e-row (え段) or i-row (い段) hiragana before the final る marks an
  // Ichidan verb (食べ+る, 見え+る); a kanji or other kana ending falls
  // through to GodanRa. This is a heuristic, not always correct.
  if (last == "る") {
    const char32_t prev_cp = utf8::decodeLastChar(utf8::dropLastChar(base_form));
    if (kana::isERowCodepoint(prev_cp) || kana::isIRowCodepoint(prev_cp)) {
      return VerbType::Ichidan;
    }
    return VerbType::GodanRa;
  }

  // 五段: the final u-row kana (く/ぐ/す/つ/ぬ/ぶ/む/う) identifies the Godan row
  // by its base_vowel. る is resolved above (Ichidan vs GodanRa) and never reaches
  // here; deriving from getGodanRows() keeps this in sync with the single
  // Godan-row source of truth instead of a parallel hand-written branch chain.
  const char32_t last_cp = utf8::decodeLastChar(base_form);
  for (const auto& [type, row] : getGodanRows()) {
    if (row.base_vowel == last_cp) {
      return type;
    }
  }

  return VerbType::Unknown;
}

std::vector<Conjugation::DictionarySuffix> Conjugation::getDictionarySuffixes(VerbType type,
                                                                              std::string_view base_form) const {
  std::vector<DictionarySuffix> suffixes;

  if (const GodanRow* row_ptr = getGodanRow(type)) {
    const auto& row = *row_ptr;

    const auto [base, a, i, e, o] = encodeGodanVowels(row);

    // Base form
    suffixes.push_back({base, false, core::ExtendedPOS::VerbShuushikei});
    // Renyokei (for compound usage)
    suffixes.push_back({i, false, core::ExtendedPOS::VerbRenyokei});
    // Mizenkei (for ない split: 書か + ない → 書く)
    suffixes.push_back({a, false, core::ExtendedPOS::VerbMizenkei});

    // 音便形 (サ行以外) - standalone stem before a tense/conjunctive auxiliary
    // E.g., 書いた → 書い + た, 飲んだ → 飲ん + だ
    // The onbin form needs to be a separate candidate to enable the split
    const std::string lexical_stem = base_form.empty() ? "" : getStem(std::string(base_form), type);
    const std::string onbin = godanOnbinForm(type, lexical_stem);
    if (!onbin.empty()) {
      suffixes.push_back({onbin, false, core::ExtendedPOS::VerbOnbinkei});  // Onbin: 書い, 飲ん, 行っ, etc.
    }
    // Note: onbin + た/て/たら is handled by split path (connection rules)

    // Negative forms (mizenkei + ない)
    // 書かない excluded for MeCab compat: split as 書か + ない
    suffixes.push_back({a + "ん", false, core::ExtendedPOS::VerbMizenkei});  // Contracted: 書かん (mizenkei + ん)
    suffixes.push_back({a + "ぬ", false, core::ExtendedPOS::VerbMizenkei});  // Classical: 書かぬ (mizenkei + ぬ)
    // 書かなかった excluded for MeCab compat: split as 書か+なかっ+た

    // Conditional
    // Kateikei (仮定形) standalone before the conjunctive particle: 書け + ば.
    // Godan e-row form serves as both kateikei and meireikei; emit both
    // ExtendedPOS so the kateikei + ば split path can win over a merged form.
    suffixes.push_back({e, false, core::ExtendedPOS::VerbKateikei});  // Conditional stem: 書け

    // Volitional mizenkei before the auxiliary: 書こ + う.
    suffixes.push_back({o, false, core::ExtendedPOS::VerbMizenkei});  // Volitional mizenkei: 書こ

    // Imperative (exclude for Ka/Ga to avoid conflict with potential)
    if (type != VerbType::GodanKa && type != VerbType::GodanGa) {
      suffixes.push_back({e, false, core::ExtendedPOS::VerbMeireikei});  // Imperative: 待て
    }

    // Potential forms (五段 → え段 + る)
    suffixes.push_back({e + "る", true, core::ExtendedPOS::VerbShuushikei});  // Potential: 書ける
    // 書けない excluded for MeCab compat: split as 書け + ない
    // 書けなかった excluded for MeCab compat: split as 書け+なかっ+た
    return suffixes;
  }

  switch (type) {
    case VerbType::Ichidan:
      // 一段動詞: 食べる → 食べ + suffix
      // Note: ます系 excluded (should split as 食べ + ます)
      // た/て are excluded because they remain separate after 食べ.
      suffixes = {
          {"る", false, core::ExtendedPOS::VerbShuushikei},  // Base: 食べる
          {"", false, core::ExtendedPOS::VerbRenyokei},      // Renyokei: 食べ (for 降り+て → lemma=降りる)
          // {"た", false},     // Past: Excluded - split as 食べ + た
          // {"て", false},     // Te-form: Excluded - split as 食べ + て
          // {"ない", false},   // Negative: Excluded - split as 食べ + ない (MeCab compat)
          {"ん", false, core::ExtendedPOS::VerbMizenkei},  // Contracted negative: 食べん (mizenkei + ん)
          // {"なかった", ...}  // Excluded for MeCab compat: split as 食べ+なかっ+た
          {"れ", false, core::ExtendedPOS::VerbKateikei},  // Conditional stem: 食べれ (split: 食べれ + ば)
          // {"たら", false},   // Conditional: Excluded - split as 食べ + たら
          {"よ", false, core::ExtendedPOS::VerbMizenkei},  // Volitional mizenkei: 食べよ (splits as 食べよ + う)
          {"ろ", false, core::ExtendedPOS::VerbMeireikei},  // Imperative: 食べろ
      };
      break;

    case VerbType::Suru:
      // サ変: exclude compound forms whose auxiliaries are separate.
      // した → し + た, so exclude. But keep conditional/imperative.
      suffixes = {
          {"する", false, core::ExtendedPOS::VerbShuushikei},  // Base form
          {"すれば", false, core::ExtendedPOS::VerbKateikei},  // Conditional
          {"しろ", false, core::ExtendedPOS::VerbMeireikei},   // Imperative
          {"せよ", false, core::ExtendedPOS::VerbMeireikei},   // Imperative (classical)
          {"しよ", false, core::ExtendedPOS::VerbMizenkei},    // Volitional mizenkei: しよ (splits as しよ + う)
          {"せん", false, core::ExtendedPOS::VerbMizenkei},    // Contracted negative (mizenkei + ん)
          {"したら", false, core::ExtendedPOS::VerbTaraForm},  // Conditional past
      };
      break;

    case VerbType::Kuru:
      // カ変: 来る is irregular (stem changes く/き/こ), so it cannot be expanded
      // via stem+suffix concatenation here — getStem("来る") yields "来", and
      // "来"+"くる"/"き"/"こ" would produce broken surfaces like 来くる/来き.
      // The shared dictionary expansion API emits explicit kanji/kana pairs.
      break;

    default:
      break;
  }

  return suffixes;
}

namespace {

// Indexed by VerbType (Unknown is empty).
constexpr std::string_view kVerbTypeNames[] = {
    "",         "ichidan",  "godan-ka", "godan-ga", "godan-sa", "godan-ta", "godan-na",
    "godan-ba", "godan-ma", "godan-ra", "godan-wa", "suru",     "kuru",     "i-adj",
};
static_assert(std::size(kVerbTypeNames) == static_cast<size_t>(VerbType::IAdjective) + 1,
              "kVerbTypeNames must cover every VerbType value");

// Indexed by VerbType (Unknown is empty).
constexpr std::string_view kVerbTypeJapanese[] = {
    "",           "一段",       "五段・カ行", "五段・ガ行", "五段・サ行", "五段・タ行", "五段・ナ行",
    "五段・バ行", "五段・マ行", "五段・ラ行", "五段・ワ行", "サ変",       "カ変",       "形容詞",
};
static_assert(std::size(kVerbTypeJapanese) == std::size(kVerbTypeNames),
              "VerbType label tables must have the same length");

// Indexed by ConjForm; Onbinkei shares the renyokei label.
constexpr std::string_view kConjFormNames[] = {
    "base", "mizenkei", "renyokei", "onbinkei", "kateikei", "meireikei", "ishikei",
};
constexpr std::string_view kConjFormJapanese[] = {
    "終止形", "未然形", "連用形", "連用形", "仮定形", "命令形", "意志形",
};
static_assert(std::size(kConjFormNames) == static_cast<size_t>(ConjForm::Count_) &&
                  std::size(kConjFormJapanese) == static_cast<size_t>(ConjForm::Count_),
              "ConjForm label tables must cover every ConjForm value");

}  // namespace

std::string_view verbTypeToString(VerbType type) {
  const auto idx = static_cast<size_t>(type);
  return idx < std::size(kVerbTypeNames) ? kVerbTypeNames[idx] : "";
}

std::string_view verbTypeToJapanese(VerbType type) {
  const auto idx = static_cast<size_t>(type);
  return idx < std::size(kVerbTypeJapanese) ? kVerbTypeJapanese[idx] : "";
}

std::string_view conjFormToString(ConjForm form) {
  const auto idx = static_cast<size_t>(form);
  return idx < std::size(kConjFormNames) ? kConjFormNames[idx] : "";
}

std::string_view conjFormToJapanese(ConjForm form) {
  const auto idx = static_cast<size_t>(form);
  return idx < std::size(kConjFormJapanese) ? kConjFormJapanese[idx] : "";
}

}  // namespace suzume::grammar
