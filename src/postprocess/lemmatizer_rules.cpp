#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "grammar/inflection_scorer_constants.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "postprocess/lemmatizer.h"
#include "postprocess/lemmatizer_internal.h"

namespace suzume::postprocess {
namespace lemmatizer_detail {

// Potential verb (可能動詞) ending: godan a-row stem + れる
// E.g., 泊まれる (とまれる)
// Single token 〜れる verbs are treated as potential (ichidan), not passive.
// Passive forms are split (読ま+れる), so single token 〜れる is likely potential.
bool endsWithPotentialVerbSuffix(std::string_view surface) {
  if (surface.size() < core::kThreeJapaneseCharBytes || !utf8::endsWith(surface, "れる")) {
    return false;
  }
  const std::string_view stem = utf8::dropLast2Chars(surface);
  const std::string_view a_row = utf8::lastChar(stem);
  return !grammar::godanBaseSuffixFromARow(utf8::decodeFirstChar(a_row)).empty() || grammar::isPureKatakana(stem);
}

// Verb endings and their base forms
struct VerbEnding {
  std::string_view suffix;
  // Empty only for canonical Godan row reversal; fixed rules store their base.
  std::string_view base;
};

// Common verb conjugation endings (simplified)
// NOTE: Order matters - longer patterns should come first
const VerbEnding kVerbEndings[] = {
    // Polite humble forms with おります (longest first)
    {"しております", "する"},          // している polite humble
    {"しておりました", "する"},        // していた polite humble
    {"いたしております", "いたす"},    // している super polite
    {"いたしておりました", "いたす"},  // していた super polite
    {"ております", "おる"},            // ている polite humble
    {"ておりました", "おる"},          // ていた polite humble
    {"おります", "おる"},              // いる polite humble

    // Suru-verb te-form + subsidiary verbs (longest first)
    // These are compound patterns where [noun]して[subsidiary] → [noun]する
    // Progressive forms of subsidiary verbs (補助動詞進行形) - longest first
    {"してもらっています", "する"},
    {"してもらっていた", "する"},
    {"してもらっている", "する"},
    {"してあげています", "する"},
    {"してあげていた", "する"},
    {"してあげている", "する"},
    {"してくれています", "する"},
    {"してくれていた", "する"},
    {"してくれている", "する"},
    {"してきています", "する"},
    {"してきていた", "する"},
    {"してきている", "する"},
    {"していっています", "する"},
    {"していっていた", "する"},
    {"していっている", "する"},
    // Base forms of subsidiary verbs (補助動詞基本形)
    {"してもらう", "する"},
    {"してもらった", "する"},
    {"してもらって", "する"},
    {"してあげる", "する"},
    {"してあげた", "する"},
    {"してあげて", "する"},
    {"してみる", "する"},
    {"してみた", "する"},
    {"してみて", "する"},
    {"してくれる", "する"},
    {"してくれた", "する"},
    {"してくれて", "する"},
    {"していく", "する"},
    {"していった", "する"},
    {"していって", "する"},
    {"してくる", "する"},
    {"してきた", "する"},
    {"してきて", "する"},
    {"しておく", "する"},
    {"しておいた", "する"},
    {"しておいて", "する"},
    {"してしまう", "する"},
    {"してしまった", "する"},
    {"してしまって", "する"},

    // Suru-verb colloquial contractions (サ変動詞口語縮約形)
    // してしまう → しちゃう/しちまう
    {"しちゃいます", "する"},
    {"しちゃう", "する"},
    {"しちゃった", "する"},
    {"しちゃって", "する"},
    {"しちまう", "する"},
    {"しちまった", "する"},
    {"しちまって", "する"},
    // しておく → しとく
    {"しときます", "する"},
    {"しとく", "する"},
    {"しといた", "する"},
    {"しといて", "する"},
    // している → してる
    {"してました", "する"},
    {"してます", "する"},
    {"してる", "する"},
    {"してた", "する"},
    // Negative te-form (否定て形)
    {"しなくて", "する"},
    {"しないで", "する"},

    // Colloquial とく/どく contractions (ておく → とく)
    // Ichidan: stem + とく → stem + る
    {"とく", "る"},    // 見とく → 見る, 食べとく → 食べる
    {"といた", "る"},  // 見といた → 見る
    {"といて", "る"},  // 見といて → 見る
    // Godan onbinkei: stem + んどく → stem + む/ぶ/ぬ
    {"んどく", "む"},    // 読んどく → 読む
    {"んどいた", "む"},  // 読んどいた → 読む
    {"んどいて", "む"},  // 読んどいて → 読む
    // Godan i-row onbinkei: stem + いとく → stem + く
    {"いとく", "く"},    // 書いとく → 書く
    {"いといた", "く"},  // 書いといた → 書く
    {"いといて", "く"},  // 書いといて → 書く
    // Godan sokuon + とく: stem + っとく → stem + う/つ/る
    {"っとく", "う"},    // 買っとく → 買う
    {"っといた", "う"},  // 買っといた → 買う
    {"っといて", "う"},  // 買っといて → 買う

    // Colloquial てる/でる contractions (ている → てる)
    // Godan sokuon: stem + ってる → stem + う/つ/る
    {"ってる", "う"},  // 買ってる → 買う, 待ってる → 待つ
    {"ってた", "う"},  // 買ってた → 買う
    // Godan i-row: stem + いてる → stem + く
    {"いてる", "く"},  // 書いてる → 書く
    {"いてた", "く"},  // 書いてた → 書く
    // Godan n-row: stem + んでる → stem + む/ぶ/ぬ
    {"んでる", "む"},  // 読んでる → 読む
    {"んでた", "む"},  // 読んでた → 読む
    // Ichidan: stem + てる → stem + る
    {"てる", "る"},  // 見てる → 見る, 食べてる → 食べる
    {"てた", "る"},  // 見てた → 見る

    // Volitional form (意志形)
    // Ichidan: stem + よう → stem + る
    {"めよう", "める"},  // 始めよう → 始める (avoid false positive on godan)
    {"べよう", "べる"},  // 食べよう → 食べる
    {"ねよう", "ねる"},  // 寝よう → 寝る

    // Compound verbs (longest first)
    {"ってしまった", "う"},
    {"いてしまった", "く"},
    {"んでしまった", "む"},
    {"てしまった", "る"},

    {"っておいた", "う"},
    {"いておいた", "く"},
    {"んでおいた", "む"},
    {"ておいた", "る"},

    {"ってみた", "う"},
    {"いてみた", "く"},
    {"んでみた", "む"},
    {"てみた", "る"},

    {"ってきた", "う"},
    {"いてきた", "く"},
    {"んできた", "む"},
    {"てきた", "る"},

    {"っていった", "う"},
    {"いていった", "く"},
    {"んでいった", "む"},
    {"ていった", "る"},

    // =========================================================================
    // Passive forms (受身形): 五段未然形 + suffix.
    // Pattern: 未然形 + れる/れた/れて/れない/れます/れました/れている
    // =========================================================================
    {"れている", ""},  // Progressive (longest first)
    {"れました", ""},  // Polite past
    {"れない", ""},    // Negative
    {"れます", ""},    // Polite
    {"れる", ""},      // Dictionary
    {"れた", ""},      // Past
    {"れて", ""},      // Te-form

    // =========================================================================
    // Causative forms (使役形): 五段未然形 + suffix.
    // Pattern: 未然形 + せる/せた/せて
    // =========================================================================
    {"せる", ""},  // Dictionary
    {"せた", ""},  // Past
    {"せて", ""},  // Te-form

    // Godan verbs (onbin forms)
    {"った", "う"},
    {"いた", "く"},
    {"いだ", "ぐ"},
    {"んだ", "む"},
    {"した", "す"},

    // Te-form
    {"って", "う"},
    {"いて", "く"},
    {"いで", "ぐ"},
    {"んで", "む"},
    {"して", "す"},

    // Masu-form
    {"ます", ""},

    // Nai-form: 五段未然形 + ない
    {"ない", ""},

    // Potential: 五段え段 + る
    {"る", ""},

    // Passive/Causative (ichidan)
    {"られる", "る"},
    {"させる", "する"},
};

// Adjective endings
const VerbEnding kAdjectiveEndings[] = {
    {"そうだった", "い"}, {"そうです", "い"}, {"そうだ", "い"}, {"そうに", "い"}, {"そうな", "い"}, {"そう", "い"},
    {"くなかった", "い"}, {"くない", "い"},   {"かった", "い"}, {"くて", "い"},   {"く", "い"},     {"さ", "い"},
};

struct ContractedVerbEnding {
  enum class Onbin : uint8_t {
    None,
    I,
    Sokuon,
    Hatsuon,
  };

  std::string_view suffix;
  Onbin onbin;
};

constexpr std::string_view kOnbinSurfaces[] = {"", "い", "っ", "ん"};

using Onbin = ContractedVerbEnding::Onbin;
const ContractedVerbEnding kContractedVerbEndings[] = {
    {"ってしまった", Onbin::Sokuon},
    {"いてしまった", Onbin::I},
    {"んでしまった", Onbin::Hatsuon},
    {"してしまった", Onbin::None},
    {"っておいた", Onbin::Sokuon},
    {"いておいた", Onbin::I},
    {"んでおいた", Onbin::Hatsuon},
    {"しておいた", Onbin::None},
    {"ってみた", Onbin::Sokuon},
    {"いてみた", Onbin::I},
    {"んでみた", Onbin::Hatsuon},
    {"してみた", Onbin::None},
    {"ってきた", Onbin::Sokuon},
    {"いてきた", Onbin::I},
    {"んできた", Onbin::Hatsuon},
    {"してきた", Onbin::None},
    {"っていった", Onbin::Sokuon},
    {"いていった", Onbin::I},
    {"んでいった", Onbin::Hatsuon},
    {"していった", Onbin::None},
    {"っとく", Onbin::Sokuon},
    {"っといた", Onbin::Sokuon},
    {"っといて", Onbin::Sokuon},
    {"いとく", Onbin::I},
    {"いといた", Onbin::I},
    {"いといて", Onbin::I},
    {"んどく", Onbin::Hatsuon},
    {"んどいた", Onbin::Hatsuon},
    {"んどいて", Onbin::Hatsuon},
    {"ってる", Onbin::Sokuon},
    {"ってた", Onbin::Sokuon},
    {"いてる", Onbin::I},
    {"いてた", Onbin::I},
    {"んでる", Onbin::Hatsuon},
    {"んでた", Onbin::Hatsuon},
    {"った", Onbin::Sokuon},
    {"って", Onbin::Sokuon},
    {"いた", Onbin::I},
    {"いて", Onbin::I},
    {"いだ", Onbin::I},
    {"いで", Onbin::I},
    {"んだ", Onbin::Hatsuon},
    {"んで", Onbin::Hatsuon},
    {"した", Onbin::None},
    {"して", Onbin::None},
};

const std::string_view kSuruPassiveEndings[] = {
    "されている", "されました", "されます", "されない", "される", "された", "されて",
};

bool hasExactVerbEntry(const dictionary::DictionaryManager* dict_manager, std::string_view surface) {
  if (dict_manager == nullptr) {
    return false;
  }
  return dict_manager->lookupExact(surface, core::PartOfSpeech::Verb) != nullptr;
}

// =============================================================================
// Shared lemma-correction helpers
//
// These encapsulate corrections applied identically from both lemmatize()
// (single morpheme) and lemmatizeAll() (vector pass) so there is one source of
// truth. Each returns the corrected lemma, or an empty string when the pattern
// does not apply (meaning: leave the existing lemma unchanged). The POS guard
// (Verb / Adverb) stays at the call site.
// =============================================================================

// サ変動詞 classical form: 漢字2文字以上+す → 漢字+する
// e.g., 確認す → 確認する, 運動す → 運動する.
// GodanSa is rejected from this shared helper so every caller preserves lexical
// multi-kanji verbs such as 見出す as well as single-kanji stems such as 出す.
std::string fixSuruClassical(std::string_view lemma, dictionary::ConjugationType conj_type) {
  if (conj_type == dictionary::ConjugationType::GodanSa || !utf8::endsWith(lemma, "す") ||
      utf8::endsWith(lemma, "する")) {
    return "";
  }
  const std::string_view stem = utf8::dropLastChar(lemma);
  if (stem.size() >= core::kTwoJapaneseCharBytes && grammar::isAllKanji(stem)) {
    return normalize::concat(stem, "する");
  }
  return "";
}

// Compound verbs analyzed as ichidan but actually サ変/godan-sa: [stem]しる → [stem]する/す
// e.g., 対しる → 対する, 関しる → 関する (single kanji → サ変),
//       やりなおしる → やりなおす (multi-char → godan-sa).
// Note: 応じる, 存じる, 信じる, 感じる are genuine ichidan (じる, not しる) and unaffected.
//
// The single-kanji threshold here is the INVERSE of fixSuruClassical's: there a
// single kanji + す is left alone (出す/消す are real GodanSa), while here a single
// kanji + しる is corrected to する (サ変). The two thresholds are deliberately
// opposite because they key off which conjugation the surface actually witnesses
// (す-shuushi vs しる-misanalysis); do NOT unify them into one rule.
//
// Caveat: the multi-char `stem + "す"` branch is UNVERIFIED against the dictionary,
// so a stem that shadows a GODAN_WA verb yields a non-word (あらしる→あらす where the
// real verb is 洗う) — the same surface-indistinguishable ambiguity as an
// あらって→あらる misanalysis. Kept as the best available default; adding a dict check here
// is an open question, not a bug to silently "fix".
// A surface that itself ends in しる witnesses the ru-row (サ変 ends in する,
// godan-sa in す), so its lemma is left alone (はしる stays はしる).
std::string fixShiru(std::string_view lemma, std::string_view surface) {
  if (!utf8::endsWith(lemma, "しる") || utf8::endsWith(surface, "しる")) {
    return "";
  }
  const std::string_view stem = utf8::dropLast2Chars(lemma);
  if (stem.size() < core::kJapaneseCharBytes) {
    return "";
  }
  if (stem.size() == core::kJapaneseCharBytes && grammar::isAllKanji(stem)) {
    return normalize::concat(stem, "する");
  }
  return normalize::concat(stem, "す");
}

// Special ra-row (ラ行特殊活用) verbs: renyokei ends in い (not り), so the
// inflection analyzer reconstructs ~いる as the base form. Correct ~いる → ~る
// when the ~る form exists as a Verb in the dictionary (ござい → ございる → ござる).
// Returns the corrected lemma, or empty when the pattern does not apply. The
// POS (Verb) guard stays at the call site.
std::string fixSpecialRaRowLemma(std::string_view lemma, const dictionary::DictionaryManager* dict) {
  if (!utf8::endsWith(lemma, "いる") || lemma.size() < core::kThreeJapaneseCharBytes || dict == nullptr) {
    return "";
  }
  std::string ru_form = normalize::concat(utf8::dropLast2Chars(lemma), "る");
  if (hasExactVerbEntry(dict, ru_form)) {
    return ru_form;
  }
  return "";
}

// Ichidan renyokei misread as a godan base: a bare renyokei directly followed
// by a bare て/た cannot be godan — every godan verb takes onbin before て/た
// (借る→借って, 過ぐ→過いで), so 借り+て must be ichidan 借りる, not godan-ra 借る.
// Exclusions: し-ending renyokei (godan-sa takes bare て with no onbin: 話し+て
// → 話す) and い-ending surfaces (書い+て is a godan-ka onbin stem, surface-
// indistinguishable from kami-ichidan 老い+て). じ is omitted because it has no
// plain godan う-row counterpart, so it never carries a wrong godan lemma.
// Fires only when the current lemma IS the wrong godan reconstruction of this
// renyokei (stem + う-row kana of the same gyo); any other lemma source (a
// dictionary lemma, an already-correct ichidan base) is left untouched. The
// POS/ExtendedPOS (Verb 連用形) guard stays at the call site.
std::string fixGodanRenyokeiBeforeLiteraryTe(std::string_view surface, std::string_view lemma,
                                             std::string_view next_surface,
                                             const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || next_surface != "て" || lemma != normalize::concat(surface, "る")) {
    return "";
  }
  // The same surface can be an ordinary modern Ichidan continuative and a
  // literary Godan continuative: 降り+て is modern 降りる, while 読み+て is
  // literary 読む.  When the candidate's Ichidan lemma is independently
  // attested, retain it; otherwise the dictionary-confirmed Godan lemma below
  // supplies the missing literary interpretation.
  if (hasExactVerbEntry(dict_manager, lemma)) {
    return "";
  }
  const std::string_view godan_ending = grammar::godanBaseSuffixFromIRow(utf8::decodeLastChar(surface));
  if (godan_ending.empty()) {
    return "";
  }
  std::string candidate = normalize::concat(utf8::dropLastChar(surface), godan_ending);
  return hasExactVerbEntry(dict_manager, candidate) ? candidate : "";
}

std::string fixIchidanRenyokeiBeforeTe(std::string_view surface, std::string_view lemma, std::string_view next_surface,
                                       const dictionary::DictionaryManager* dict_manager) {
  if (!utf8::equalsAny(next_surface, {"て", "た"}) || surface.size() < core::kJapaneseCharBytes) {
    return "";
  }
  // い-row kana → う-row kana of the same gyo (し/い/じ excluded, see above).
  const std::string_view tail = utf8::lastChar(surface);
  if (utf8::equalsAny(tail, {"し", "い", "じ"})) {
    return "";
  }
  std::string_view godan_ending = grammar::godanBaseSuffixFromIRow(utf8::decodeFirstChar(tail));
  // These historical rows are outside the modern Godan table but remain valid
  // for this fallback's same-row ambiguity check.
  if (tail == "ぢ") {
    godan_ending = "づ";
  } else if (tail == "ひ") {
    godan_ending = "ふ";
  } else if (tail == "ぴ") {
    godan_ending = "ぷ";
  }
  if (godan_ending.empty()) {
    return "";
  }

  std::string godan_base = normalize::concat(utf8::dropLastChar(surface), godan_ending);
  if (lemma != godan_base) {
    return "";
  }
  // A dictionary-confirmed godan base is not the fabricated reading this
  // correction targets (走り+て, 読み+て). The remaining ambiguous cases
  // are genuine ichidan renyokei (借り+て, 過ぎ+て).
  if (hasExactVerbEntry(dict_manager, godan_base)) {
    return "";
  }
  return normalize::concat(surface, "る");
}

// Potential verb (可能動詞): single-token 五段あ段+れる keeps lemma = surface.
// Passive forms are split (読ま+れる), so a single 〜れる token is treated as
// potential (ichidan), whose lemma is the surface itself.
std::string fixPotentialVerb(const core::Morpheme& morpheme) {
  if (morpheme.pos == core::PartOfSpeech::Verb && endsWithPotentialVerbSuffix(morpheme.surface)) {
    return morpheme.surface;
  }
  return "";
}

// Tari-adjective adverb: strip trailing と, e.g., 颯爽と → 颯爽, 堂々と → 堂々.
// Pattern: exactly 漢字2文字 + と. MeCab uses the stem as lemma.
std::string fixTariAdverb(std::string_view surface) {
  if (!utf8::endsWith(surface, "と")) {
    return "";
  }
  const auto codepoints = normalize::toCodepoints(surface);
  if (codepoints.size() != 3 || codepoints.back() != U'と') {
    return "";
  }
  if (normalize::isNumeralCodepoint(codepoints.front())) {
    return "";
  }
  const std::string_view stem = utf8::dropLastChar(surface);
  if (grammar::isAllKanji(stem) || codepoints[1] == U'々') {
    return std::string(stem);
  }
  return "";
}

// 撥音便 godan base from a stem whose original onbin form ended in ん.
// Tries the canonical ん音便 rows against the dictionary; falls back to む
// (most common 撥音便) when the stem is all kanji. Returns empty if nothing applies.
// e.g., 読ん → 読む, 学ん → 学ぶ, 死ん → 死ぬ.
std::string fixHatsuonbin(std::string_view stem, const dictionary::DictionaryManager* dict_manager) {
  if (stem.empty()) {
    return "";
  }
  if (dict_manager != nullptr) {
    for (const auto& [verb_type, ending] : grammar::Conjugation::getGodanTypesByOnbin("ん")) {
      (void)verb_type;
      std::string base = normalize::concat(stem, ending);
      if (hasExactVerbEntry(dict_manager, base)) {
        return base;
      }
    }
  }
  // Kanji-fallback guard: assume む for kanji stems when no dictionary match is found.
  if (grammar::isAllKanji(stem)) {
    return normalize::concat(stem, "む");
  }
  return "";
}

std::string lemmatizeGodanEnding(std::string_view surface, const VerbEnding& ending) {
  const std::string_view stem = surface.substr(0, surface.size() - ending.suffix.size());
  const char32_t row_codepoint = utf8::decodeLastChar(stem);
  std::string_view base;
  if (ending.suffix == "る") {
    base = grammar::godanBaseSuffixFromERow(row_codepoint);
  } else if (ending.suffix == "ます") {
    base = grammar::godanBaseSuffixFromIRow(row_codepoint);
  } else {
    base = grammar::godanBaseSuffixFromARow(row_codepoint);
  }
  if (base.empty()) {
    // Ichidan 未然・連用形 share the bare stem. The former duplicate rows
    // attempted this fallback after a failed Godan reversal, but identical
    // suffixes made that decision order-dependent.
    if (ending.suffix == "ない" || ending.suffix == "ます") {
      return normalize::concat(stem, "る");
    }
    return "";
  }
  return normalize::concat(utf8::dropLastChar(stem), base);
}

std::string lemmatizeVerbFallback(std::string_view surface) {
  // The table intentionally contains overlapping productive endings (れる /
  // られる, せる / させる).  Choose the longest matching rule rather than
  // depending on declaration order: that makes a shorter row unable to hide
  // a more specific paradigm cell when the table is extended.
  const VerbEnding* best_ending = nullptr;
  for (const auto& ending : kVerbEndings) {
    if (utf8::endsWith(surface, ending.suffix) &&
        (best_ending == nullptr || ending.suffix.size() > best_ending->suffix.size())) {
      best_ending = &ending;
    }
  }
  if (best_ending == nullptr) {
    return std::string(surface);
  }
  if (best_ending->base.empty()) {
    if (best_ending->suffix == "る") {
      // The bare る rule reverses Godan potential.  Applying it to an
      // Ichidan terminal (食べる) constructs a non-word such as 食ぶ.  An
      // e-row terminal is structurally ambiguous with Godan potential, and
      // this dictionary-free fallback must prefer preserving a possible word
      // over inventing a lemma. Dictionary-backed paths retain the precise
      // Godan analysis before they reach this last-resort rule.
      const std::string_view stem = utf8::dropLastChar(surface);
      if (kana::isERowCodepoint(utf8::decodeLastChar(stem))) {
        return std::string(surface);
      }
    }
    if (std::string result = lemmatizeGodanEnding(surface, *best_ending); !result.empty()) {
      return result;
    }
    return std::string(surface);
  }
  return normalize::concat(surface.substr(0, surface.size() - best_ending->suffix.size()), best_ending->base);
}

}  // namespace lemmatizer_detail

using namespace lemmatizer_detail;

std::string Lemmatizer::lemmatizeVerb(std::string_view surface) {
  return lemmatizeVerbFallback(surface);
}

std::string Lemmatizer::lemmatizeAdjective(std::string_view surface) {
  // Preserve the ない lemma in the adjective + さ + そう pattern.
  // なさそう = ない + さ + そう (looks like there isn't)
  // Without this, lemmatizer would incorrectly return なさい (from そう → い rule)
  // This pattern also covers: なさそうな, なさそうに, なさそうだ, etc.
  if (surface.find("なさそう") == 0) {
    // Replace なさそう... with ない
    return "ない";
  }
  // Also handle なさ alone (noun form of ない)
  if (surface == "なさ") {
    return "ない";
  }

  for (const auto& ending : kAdjectiveEndings) {
    if (utf8::endsWith(surface, ending.suffix)) {
      return normalize::concat(surface.substr(0, surface.size() - ending.suffix.size()), ending.base);
    }
  }
  return std::string(surface);
}

bool Lemmatizer::verifyCandidateWithDictionary(const grammar::InflectionCandidate& candidate) const {
  if (dict_manager_ == nullptr) {
    return false;
  }

  // Accept if base_form exists as a verb/adjective in the dictionary. A type
  // mismatch is acceptable: the dictionary presence validates the base form
  // itself (見せられた → 見せる with a wrong GodanRa type is still accepted).
  for (const auto& result : dict_manager_->lookup(candidate.base_form, 0)) {
    if (result.entry == nullptr || result.entry->surface != candidate.base_form) {
      continue;
    }
    if (result.entry->pos == core::PartOfSpeech::Verb || result.entry->pos == core::PartOfSpeech::Adjective) {
      return true;
    }
  }
  return false;
}

namespace lemmatizer_detail {
std::string lemmatizeContractedVerbWithDictionary(std::string_view surface,
                                                  const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr) {
    return "";
  }

  for (const auto& ending : kContractedVerbEndings) {
    if (!utf8::endsWith(surface, ending.suffix)) {
      continue;
    }

    const std::string_view stem = surface.substr(0, surface.size() - ending.suffix.size());
    const std::string_view onbin = kOnbinSurfaces[static_cast<size_t>(ending.onbin)];
    for (const auto& [verb_type, base_suffix] : grammar::Conjugation::getGodanTypesByOnbin(onbin)) {
      (void)verb_type;
      std::string base_form = normalize::concat(stem, base_suffix);
      if (hasExactVerbEntry(dict_manager, base_form)) {
        return base_form;
      }
    }
  }

  return "";
}

std::string lemmatizeSuruPassiveWithDictionary(std::string_view surface,
                                               const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr) {
    return "";
  }

  for (std::string_view ending : kSuruPassiveEndings) {
    if (!utf8::endsWith(surface, ending)) {
      continue;
    }

    const std::string_view stem = surface.substr(0, surface.size() - ending.size());
    if (stem.empty()) {
      continue;
    }
    std::string base_form = normalize::concat(stem, "する");
    if (hasExactVerbEntry(dict_manager, base_form)) {
      return base_form;
    }
  }

  return "";
}
}  // namespace lemmatizer_detail
}  // namespace suzume::postprocess
