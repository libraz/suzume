/**
 * @file auxiliary_generator.cpp
 * @brief Auto-generation of auxiliary verb conjugation patterns
 */

#include "auxiliary_generator.h"

#include <algorithm>

#include "core/utf8_constants.h"

namespace suzume::grammar {

namespace {

// UTF-8 helper: drop last character (assumes valid UTF-8 hiragana)
std::string dropLastChar(const std::string& s) {
  if (s.size() >= core::kJapaneseCharBytes) {
    return s.substr(0, s.size() - core::kJapaneseCharBytes);
  }
  return s;
}

// Conjugation suffix with output connection ID
struct ConjSuffix {
  const char* suffix;
  uint16_t right_id;
};

// =============================================================================
// Suffix Tables (テーブル駆動活用パターン)
// =============================================================================

// Full forms with negative (9 suffixes)
// Pattern: base, ta, tara, te, masu, mashita, nai, nakatta, nakute
constexpr ConjSuffix kIchidanFull[] = {
    {"る", conn::kAuxOutBase},   {"た", conn::kAuxOutTa},
    {"たら", conn::kAuxOutBase}, {"て", conn::kAuxOutTe},
    {"ます", conn::kAuxOutMasu}, {"ました", conn::kAuxOutTa},
    {"ない", conn::kAuxOutBase}, {"なかった", conn::kAuxOutTa},
    {"なくて", conn::kAuxOutTe},
};

// Te-attachment limited forms (6 suffixes, no negative)
constexpr ConjSuffix kIchidanTeAttach[] = {
    {"る", conn::kAuxOutBase},   {"た", conn::kAuxOutTa},
    {"たら", conn::kAuxOutBase}, {"て", conn::kAuxOutTe},
    {"ます", conn::kAuxOutMasu}, {"ました", conn::kAuxOutTa},
};

// Godan-Wa (五段わ行) - full
constexpr ConjSuffix kGodanWaFull[] = {
    {"う", conn::kAuxOutBase},       {"った", conn::kAuxOutTa},
    {"ったら", conn::kAuxOutBase},   {"って", conn::kAuxOutTe},
    {"います", conn::kAuxOutMasu},   {"いました", conn::kAuxOutTa},
    {"わない", conn::kAuxOutBase},   {"わなかった", conn::kAuxOutTa},
    {"わなくて", conn::kAuxOutTe},
};

// Godan-Wa te-attachment
constexpr ConjSuffix kGodanWaTeAttach[] = {
    {"う", conn::kAuxOutBase},     {"った", conn::kAuxOutTa},
    {"ったら", conn::kAuxOutBase}, {"って", conn::kAuxOutTe},
    {"います", conn::kAuxOutMasu}, {"いました", conn::kAuxOutTa},
};

// Godan-Ka (五段か行) - full
constexpr ConjSuffix kGodanKaFull[] = {
    {"く", conn::kAuxOutBase},     {"いた", conn::kAuxOutTa},
    {"いたら", conn::kAuxOutBase}, {"いて", conn::kAuxOutTe},
    {"きます", conn::kAuxOutMasu}, {"きました", conn::kAuxOutTa},
    {"かない", conn::kAuxOutBase}, {"かなかった", conn::kAuxOutTa},
    {"かなくて", conn::kAuxOutTe},
};

// Godan-Sa (五段さ行) - full
constexpr ConjSuffix kGodanSaFull[] = {
    {"す", conn::kAuxOutBase},     {"した", conn::kAuxOutTa},
    {"したら", conn::kAuxOutBase}, {"して", conn::kAuxOutTe},
    {"します", conn::kAuxOutMasu}, {"しました", conn::kAuxOutTa},
    {"さない", conn::kAuxOutBase}, {"さなかった", conn::kAuxOutTa},
    {"さなくて", conn::kAuxOutTe},
};

// Godan-Ra (五段ら行) - full
constexpr ConjSuffix kGodanRaFull[] = {
    {"る", conn::kAuxOutBase},     {"った", conn::kAuxOutTa},
    {"ったら", conn::kAuxOutBase}, {"って", conn::kAuxOutTe},
    {"ります", conn::kAuxOutMasu}, {"りました", conn::kAuxOutTa},
    {"らない", conn::kAuxOutBase}, {"らなかった", conn::kAuxOutTa},
    {"らなくて", conn::kAuxOutTe},
};

// Kuru (カ変) - irregular, full forms
constexpr ConjSuffix kKuruFull[] = {
    {"くる", conn::kAuxOutBase},     {"きた", conn::kAuxOutTa},
    {"きたら", conn::kAuxOutBase},   {"きて", conn::kAuxOutTe},
    {"きます", conn::kAuxOutMasu},   {"きました", conn::kAuxOutTa},
    {"こない", conn::kAuxOutBase},   {"こなかった", conn::kAuxOutTa},
    {"こなくて", conn::kAuxOutTe},
};

// I-adjective (い形容詞)
constexpr ConjSuffix kIAdjective[] = {
    {"い", conn::kAuxOutBase},       {"かった", conn::kAuxOutTa},
    {"くて", conn::kAuxOutTe},       {"くない", conn::kAuxOutBase},
    {"くなかった", conn::kAuxOutTa}, {"ければ", conn::kAuxOutBase},
    {"く", conn::kAuxOutBase},  // adverbial
};

// Masu (ます) - special (no stem)
constexpr ConjSuffix kMasu[] = {
    {"ます", conn::kAuxOutMasu},       {"ました", conn::kAuxOutTa},
    {"ません", conn::kAuxOutBase},     {"ましょう", conn::kAuxOutBase},
    {"ませんでした", conn::kAuxOutTa},
};

// =============================================================================
// Table-Driven Generation (単一ジェネレータ関数)
// =============================================================================

// Generate forms using stem + suffix pattern
template <size_t N>
std::vector<AuxiliaryEntry> generateWithStem(
    const AuxiliaryBase& base, const ConjSuffix (&suffixes)[N]) {
  std::string stem = dropLastChar(base.surface);
  std::string reading_stem = dropLastChar(base.reading);

  std::vector<AuxiliaryEntry> result;
  result.reserve(N);
  for (const auto& suf : suffixes) {
    result.push_back({stem + suf.suffix, reading_stem + suf.suffix,
                      base.surface, base.left_id, suf.right_id,
                      base.required_conn});
  }
  return result;
}

// Generate forms using full forms (no stem, for irregular verbs)
template <size_t N>
std::vector<AuxiliaryEntry> generateFullForms(
    const AuxiliaryBase& base, const ConjSuffix (&forms)[N]) {
  std::vector<AuxiliaryEntry> result;
  result.reserve(N);
  for (const auto& form : forms) {
    result.push_back({form.suffix, form.suffix, base.surface,
                      base.left_id, form.right_id, base.required_conn});
  }
  return result;
}

// Generate Masu forms (special: fixed lemma "ます")
std::vector<AuxiliaryEntry> generateMasuForms(const AuxiliaryBase& base) {
  std::vector<AuxiliaryEntry> result;
  result.reserve(std::size(kMasu));
  for (const auto& form : kMasu) {
    result.push_back({form.suffix, form.suffix, "ます",
                      base.left_id, form.right_id, base.required_conn});
  }
  return result;
}

// No conjugation - single form only
std::vector<AuxiliaryEntry> generateNoConjForms(const AuxiliaryBase& base) {
  return {{base.surface, base.reading, base.surface, base.left_id,
           conn::kAuxOutBase, base.required_conn}};
}

// Add special patterns that cannot be auto-generated
void addSpecialPatterns(std::vector<AuxiliaryEntry>& entries) {
  using namespace conn;

  // === Past/Conditional た系 (voiced variants) ===
  entries.push_back({"た", "た", "た", kAuxTa, kAuxOutTa, kVerbOnbinkei});
  entries.push_back({"だ", "だ", "た", kAuxTa, kAuxOutTa, kVerbOnbinkei});
  entries.push_back({"たら", "たら", "たら", kAuxTa, kAuxOutBase, kVerbOnbinkei});
  entries.push_back({"だら", "だら", "たら", kAuxTa, kAuxOutBase, kVerbOnbinkei});

  // === Te-form (voiced variants) ===
  entries.push_back({"て", "て", "て", kAuxTe, kAuxOutTe, kVerbOnbinkei});
  entries.push_back({"で", "で", "て", kAuxTe, kAuxOutTe, kVerbOnbinkei});

  // === Tari form ===
  entries.push_back({"たり", "たり", "たり", kAuxTa, kAuxOutBase, kVerbOnbinkei});
  entries.push_back({"だり", "だり", "たり", kAuxTa, kAuxOutBase, kVerbOnbinkei});
  entries.push_back({"たりする", "たりする", "たり", kAuxTa, kAuxOutBase, kVerbOnbinkei});
  entries.push_back({"だりする", "だりする", "たり", kAuxTa, kAuxOutBase, kVerbOnbinkei});
  entries.push_back({"たりした", "たりした", "たり", kAuxTa, kAuxOutTa, kVerbOnbinkei});
  entries.push_back({"だりした", "だりした", "たり", kAuxTa, kAuxOutTa, kVerbOnbinkei});
  entries.push_back({"たりして", "たりして", "たり", kAuxTa, kAuxOutTe, kVerbOnbinkei});
  entries.push_back({"だりして", "だりして", "たり", kAuxTa, kAuxOutTe, kVerbOnbinkei});

  // === Conditional ば ===
  entries.push_back({"ば", "ば", "ば", kAuxNai, kAuxOutBase, kVerbKatei});

  // === Classical negation ず (古語否定) - connects to mizenkei ===
  // 尽きず, せず, 知らず etc.
  entries.push_back({"ず", "ず", "ず", kAuxNai, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"ずに", "ずに", "ず", kAuxNai, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"ずとも", "ずとも", "ず", kAuxNai, kAuxOutBase, kVerbMizenkei});

  // === Volitional ===
  entries.push_back({"う", "う", "う", kAuxNai, kAuxOutBase, kVerbVolitional});
  entries.push_back({"よう", "よう", "よう", kAuxNai, kAuxOutBase, kVerbVolitional});

  // === Negative conjecture まい (打消推量) ===
  // まい attaches to:
  // - Godan 終止形: 行くまい, 書くまい, 言うまい
  // - Ichidan 未然形: 食べまい, 見まい, 出来まい (でき + まい)
  // - Kuru 未然形: こまい
  // - Suru 未然形: しまい
  entries.push_back({"まい", "まい", "まい", kAuxNai, kAuxOutBase, kVerbBase});
  entries.push_back({"まい", "まい", "まい", kAuxNai, kAuxOutBase, kVerbMizenkei});

  // === Volitional + とする ===
  entries.push_back({"うとする", "うとする", "とする", kAuxNai, kAuxOutBase, kVerbVolitional});
  entries.push_back({"うとした", "うとした", "とする", kAuxNai, kAuxOutTa, kVerbVolitional});
  entries.push_back({"うとして", "うとして", "とする", kAuxNai, kAuxOutTe, kVerbVolitional});
  entries.push_back({"ようとする", "ようとする", "とする", kAuxNai, kAuxOutBase, kVerbVolitional});
  entries.push_back({"ようとした", "ようとした", "とする", kAuxNai, kAuxOutTa, kVerbVolitional});
  entries.push_back({"ようとして", "ようとして", "とする", kAuxNai, kAuxOutTe, kVerbVolitional});

  // === Renyokei compounds ===
  entries.push_back({"ながら", "ながら", "ながら", kAuxRenyokei, kAuxOutBase, kVerbRenyokei});

  // === Sou form (appearance) ===
  entries.push_back({"そう", "そう", "そう", kAuxSou, kAuxOutBase, kVerbRenyokei});
  entries.push_back({"そうだ", "そうだ", "そう", kAuxSou, kAuxOutBase, kVerbRenyokei});
  entries.push_back({"そうだった", "そうだった", "そう", kAuxSou, kAuxOutTa, kVerbRenyokei});
  entries.push_back({"そうです", "そうです", "そう", kAuxSou, kAuxOutBase, kVerbRenyokei});
  entries.push_back({"そうでした", "そうでした", "そう", kAuxSou, kAuxOutTa, kVerbRenyokei});

  // === Potential stem endings ===
  entries.push_back({"る", "る", "る", kAuxReru, kAuxOutBase, kVerbPotential});
  entries.push_back({"た", "た", "る", kAuxReru, kAuxOutTa, kVerbPotential});
  entries.push_back({"て", "て", "る", kAuxReru, kAuxOutTe, kVerbPotential});
  entries.push_back({"ない", "ない", "る", kAuxReru, kAuxOutBase, kVerbPotential});
  entries.push_back({"なかった", "なかった", "る", kAuxReru, kAuxOutTa, kVerbPotential});
  entries.push_back({"ます", "ます", "る", kAuxReru, kAuxOutMasu, kVerbPotential});
  entries.push_back({"ました", "ました", "る", kAuxReru, kAuxOutTa, kVerbPotential});
  entries.push_back({"ません", "ません", "る", kAuxReru, kAuxOutBase, kVerbPotential});
  entries.push_back({"ませんでした", "ませんでした", "る", kAuxReru, kAuxOutTa, kVerbPotential});

  // === Negative te-form ===
  entries.push_back({"ないで", "ないで", "ないで", kAuxNai, kAuxOutTe, kVerbMizenkei});
  entries.push_back({"ないでいる", "ないでいる", "ないで", kAuxNai, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"ないでいた", "ないでいた", "ないで", kAuxNai, kAuxOutTa, kVerbMizenkei});

  // === Obligation patterns ===
  entries.push_back({"ないといけない", "ないといけない", "ないといけない", kAuxNai, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"なければならない", "なければならない", "なければならない", kAuxNai, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"なくてはいけない", "なくてはいけない", "なくてはいけない", kAuxNai, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"なきゃいけない", "なきゃいけない", "なきゃいけない", kAuxNai, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"なくちゃ", "なくちゃ", "なくちゃ", kAuxNai, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"なきゃ", "なきゃ", "なきゃ", kAuxNai, kAuxOutBase, kVerbMizenkei});

  // === I-adjective endings (stem attachments) ===
  entries.push_back({"い", "い", "い", kAuxNai, kAuxOutBase, kIAdjStem});
  entries.push_back({"かった", "かった", "い", kAuxNai, kAuxOutTa, kIAdjStem});
  entries.push_back({"くない", "くない", "い", kAuxNai, kAuxOutBase, kIAdjStem});
  entries.push_back({"くなかった", "くなかった", "い", kAuxNai, kAuxOutTa, kIAdjStem});
  entries.push_back({"くて", "くて", "い", kAuxNai, kAuxOutTe, kIAdjStem});
  entries.push_back({"ければ", "ければ", "い", kAuxNai, kAuxOutBase, kIAdjStem});
  entries.push_back({"く", "く", "い", kAuxNai, kAuxOutBase, kIAdjStem});
  entries.push_back({"かったら", "かったら", "い", kAuxNai, kAuxOutBase, kIAdjStem});
  entries.push_back({"くなる", "くなる", "い", kAuxNai, kAuxOutBase, kIAdjStem});
  entries.push_back({"くなった", "くなった", "い", kAuxNai, kAuxOutTa, kIAdjStem});
  entries.push_back({"くなって", "くなって", "い", kAuxNai, kAuxOutTe, kIAdjStem});
  entries.push_back({"さ", "さ", "い", kAuxNai, kAuxOutBase, kIAdjStem});
  entries.push_back({"そう", "そう", "い", kAuxNai, kAuxOutBase, kIAdjStem});
  entries.push_back({"そうだ", "そうだ", "い", kAuxNai, kAuxOutBase, kIAdjStem});
  entries.push_back({"そうな", "そうな", "い", kAuxNai, kAuxOutBase, kIAdjStem});
  entries.push_back({"そうに", "そうに", "い", kAuxNai, kAuxOutBase, kIAdjStem});

  // === I-adjective + すぎる (from stem) ===
  entries.push_back({"すぎる", "すぎる", "い", kAuxRenyokei, kAuxOutBase, kIAdjStem});
  entries.push_back({"すぎた", "すぎた", "い", kAuxRenyokei, kAuxOutTa, kIAdjStem});
  entries.push_back({"すぎて", "すぎて", "い", kAuxRenyokei, kAuxOutTe, kIAdjStem});
  entries.push_back({"すぎます", "すぎます", "い", kAuxRenyokei, kAuxOutMasu, kIAdjStem});

  // === Causative-passive (させられる, せられる, される) ===
  entries.push_back({"させられる", "させられる", "させられる", kAuxSeru, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"させられた", "させられた", "させられる", kAuxSeru, kAuxOutTa, kVerbMizenkei});
  entries.push_back({"させられて", "させられて", "させられる", kAuxSeru, kAuxOutTe, kVerbMizenkei});
  entries.push_back({"させられない", "させられない", "させられる", kAuxSeru, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"させられます", "させられます", "させられる", kAuxSeru, kAuxOutMasu, kVerbMizenkei});

  entries.push_back({"せられる", "せられる", "せられる", kAuxSeru, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"せられた", "せられた", "せられる", kAuxSeru, kAuxOutTa, kVerbMizenkei});
  entries.push_back({"せられて", "せられて", "せられる", kAuxSeru, kAuxOutTe, kVerbMizenkei});
  entries.push_back({"せられない", "せられない", "せられる", kAuxSeru, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"せられます", "せられます", "せられる", kAuxSeru, kAuxOutMasu, kVerbMizenkei});

  entries.push_back({"される", "される", "される", kAuxSeru, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"された", "された", "される", kAuxSeru, kAuxOutTa, kVerbMizenkei});
  entries.push_back({"されて", "されて", "される", kAuxSeru, kAuxOutTe, kVerbMizenkei});
  entries.push_back({"されない", "されない", "される", kAuxSeru, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"されます", "されます", "される", kAuxSeru, kAuxOutMasu, kVerbMizenkei});

  // === なくなる patterns ===
  entries.push_back({"なくなる", "なくなる", "なくなる", kAuxNai, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"なくなった", "なくなった", "なくなる", kAuxNai, kAuxOutTa, kVerbMizenkei});
  entries.push_back({"なくなって", "なくなって", "なくなる", kAuxNai, kAuxOutTe, kVerbMizenkei});
  entries.push_back({"なくなってしまう", "なくなってしまう", "なくなる", kAuxNai, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"なくなってしまった", "なくなってしまった", "なくなる", kAuxNai, kAuxOutTa, kVerbMizenkei});

  // === Potential + なくなる ===
  entries.push_back({"なくなる", "なくなる", "なくなる", kAuxNai, kAuxOutBase, kVerbPotential});
  entries.push_back({"なくなった", "なくなった", "なくなる", kAuxNai, kAuxOutTa, kVerbPotential});
  entries.push_back({"なくなって", "なくなって", "なくなる", kAuxNai, kAuxOutTe, kVerbPotential});

  // === Passive + なくなる ===
  entries.push_back({"れなくなる", "れなくなる", "れる", kAuxReru, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"れなくなった", "れなくなった", "れる", kAuxReru, kAuxOutTa, kVerbMizenkei});
  entries.push_back({"られなくなる", "られなくなる", "られる", kAuxReru, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"られなくなった", "られなくなった", "られる", kAuxReru, kAuxOutTa, kVerbMizenkei});

  // === Colloquial てしまう contractions ===
  // Note: Connect to both kVerbOnbinkei (for Godan) and kVerbRenyokei (for Ichidan)
  // because ちゃう replaces てしまう, and て connects to onbin for Godan but renyokei for Ichidan
  // Godan: 書いちゃった = 書い (onbin) + ちゃった
  // Ichidan: 食べちゃった = 食べ (renyokei) + ちゃった
  entries.push_back({"ちゃう", "ちゃう", "しまう", kAuxTeshimau, kAuxOutBase, kVerbOnbinkei});
  entries.push_back({"ちゃった", "ちゃった", "しまう", kAuxTeshimau, kAuxOutTa, kVerbOnbinkei});
  entries.push_back({"ちゃって", "ちゃって", "しまう", kAuxTeshimau, kAuxOutTe, kVerbOnbinkei});
  entries.push_back({"じゃう", "じゃう", "しまう", kAuxTeshimau, kAuxOutBase, kVerbOnbinkei});
  entries.push_back({"じゃった", "じゃった", "しまう", kAuxTeshimau, kAuxOutTa, kVerbOnbinkei});
  entries.push_back({"じゃって", "じゃって", "しまう", kAuxTeshimau, kAuxOutTe, kVerbOnbinkei});
  // Renyokei versions for Ichidan verbs (ちゃう only, not じゃう)
  // じゃう is for Godan voiced onbin (読んで→読んじゃ), not Ichidan
  // Ichidan uses unvoiced て (食べて→食べちゃ)
  entries.push_back({"ちゃう", "ちゃう", "しまう", kAuxTeshimau, kAuxOutBase, kVerbRenyokei});
  entries.push_back({"ちゃった", "ちゃった", "しまう", kAuxTeshimau, kAuxOutTa, kVerbRenyokei});
  entries.push_back({"ちゃって", "ちゃって", "しまう", kAuxTeshimau, kAuxOutTe, kVerbRenyokei});

  // === Colloquial ておく contraction ===
  // Godan onbin: やっとく, 書いとく - connects to 音便形
  entries.push_back({"とく", "とく", "おく", kAuxTeoku, kAuxOutBase, kVerbOnbinkei});
  entries.push_back({"といた", "といた", "おく", kAuxTeoku, kAuxOutTa, kVerbOnbinkei});
  entries.push_back({"といて", "といて", "おく", kAuxTeoku, kAuxOutTe, kVerbOnbinkei});
  // Ichidan renyokei: 見とく, 食べとく - connects to 連用形
  entries.push_back({"とく", "とく", "おく", kAuxTeoku, kAuxOutBase, kVerbRenyokei});
  entries.push_back({"といた", "といた", "おく", kAuxTeoku, kAuxOutTa, kVerbRenyokei});
  entries.push_back({"といて", "といて", "おく", kAuxTeoku, kAuxOutTe, kVerbRenyokei});
  // Voiced onbin: 読んどく, 飲んどく, 死んどく - で→ど contraction
  // Pattern is: 読ん (onbin stem) + どく (voiced contraction)
  // Same structure as でる/でた for ている contraction
  entries.push_back({"どく", "どく", "おく", kAuxTeoku, kAuxOutBase, kVerbOnbinkei});
  entries.push_back({"どいた", "どいた", "おく", kAuxTeoku, kAuxOutTa, kVerbOnbinkei});
  entries.push_back({"どいて", "どいて", "おく", kAuxTeoku, kAuxOutTe, kVerbOnbinkei});

  // === Colloquial ている contraction (てる) ===
  // してる, 食べてる, 見てる - contracts ている to てる
  // These connect after te-form verbs (kAuxOutTe)
  entries.push_back({"てる", "てる", "いる", kAuxTeiru, kAuxOutBase, kAuxOutTe});
  entries.push_back({"てた", "てた", "いる", kAuxTeiru, kAuxOutTa, kAuxOutTe});
  entries.push_back({"てて", "てて", "いる", kAuxTeiru, kAuxOutTe, kAuxOutTe});
  entries.push_back({"てない", "てない", "いる", kAuxTeiru, kAuxOutBase, kAuxOutTe});
  entries.push_back({"てなかった", "てなかった", "いる", kAuxTeiru, kAuxOutTa, kAuxOutTe});
  // Ichidan renyokei versions: 見てた = 見(renyokei) + てた
  // The て is part of the contracted aux, not a separate particle
  entries.push_back({"てる", "てる", "いる", kAuxTeiru, kAuxOutBase, kVerbRenyokei});
  entries.push_back({"てた", "てた", "いる", kAuxTeiru, kAuxOutTa, kVerbRenyokei});
  entries.push_back({"てない", "てない", "いる", kAuxTeiru, kAuxOutBase, kVerbRenyokei});
  // でる/でた for voiced te-form (読んでる, 遊んでた)
  entries.push_back({"でる", "でる", "いる", kAuxTeiru, kAuxOutBase, kAuxOutTe});
  entries.push_back({"でた", "でた", "いる", kAuxTeiru, kAuxOutTa, kAuxOutTe});
  entries.push_back({"でて", "でて", "いる", kAuxTeiru, kAuxOutTe, kAuxOutTe});
  entries.push_back({"でない", "でない", "いる", kAuxTeiru, kAuxOutBase, kAuxOutTe});
  entries.push_back({"でなかった", "でなかった", "いる", kAuxTeiru, kAuxOutTa, kAuxOutTe});
  // Godan onbin versions: 読んでた = 読ん(onbin) + でた (voiced sokuonbin)
  entries.push_back({"でる", "でる", "いる", kAuxTeiru, kAuxOutBase, kVerbOnbinkei});
  entries.push_back({"でた", "でた", "いる", kAuxTeiru, kAuxOutTa, kVerbOnbinkei});
  entries.push_back({"でない", "でない", "いる", kAuxTeiru, kAuxOutBase, kVerbOnbinkei});
  // Godan sokuonbin versions: 知ってる = 知っ(sokuonbin stem) + てる
  // For GodanRa (知る→知っ), GodanTa (持つ→持っ), GodanWa (買う→買っ)
  // Note: aux is "てる" not "ってる" so that stem remains "知っ" ending with っ
  entries.push_back({"てる", "てる", "いる", kAuxTeiru, kAuxOutBase, kVerbOnbinkei});
  entries.push_back({"てた", "てた", "いる", kAuxTeiru, kAuxOutTa, kVerbOnbinkei});
  entries.push_back({"てない", "てない", "いる", kAuxTeiru, kAuxOutBase, kVerbOnbinkei});
  entries.push_back({"てなかった", "てなかった", "いる", kAuxTeiru, kAuxOutTa, kVerbOnbinkei});

  // === Suru-verb specific ている contractions ===
  // してる = し + ている contraction, full patterns for suru-verbs
  // Note: These use empty stem (stem="") for suru-verb matching
  entries.push_back({"してる", "してる", "いる", kAuxTeiru, kAuxOutBase, kVerbOnbinkei});
  entries.push_back({"してた", "してた", "いる", kAuxTeiru, kAuxOutTa, kVerbOnbinkei});
  entries.push_back({"してない", "してない", "いる", kAuxTeiru, kAuxOutBase, kVerbOnbinkei});
  entries.push_back({"してなかった", "してなかった", "いる", kAuxTeiru, kAuxOutTa, kVerbOnbinkei});

  // === Polite forms ===
  entries.push_back({"おる", "おる", "おる", kAuxTeiru, kAuxOutBase, kAuxOutTe});
  entries.push_back({"おった", "おった", "おる", kAuxTeiru, kAuxOutTa, kAuxOutTe});
  entries.push_back({"おります", "おります", "おる", kAuxTeiru, kAuxOutMasu, kAuxOutTe});
  entries.push_back({"おりました", "おりました", "おる", kAuxTeiru, kAuxOutTa, kAuxOutTe});

  // === ていただく ===
  entries.push_back({"いただく", "いただく", "いただく", kAuxTemorau, kAuxOutBase, kAuxOutTe});
  entries.push_back({"いただいた", "いただいた", "いただく", kAuxTemorau, kAuxOutTa, kAuxOutTe});
  entries.push_back({"いただいて", "いただいて", "いただく", kAuxTemorau, kAuxOutTe, kAuxOutTe});
  entries.push_back({"いただきます", "いただきます", "いただく", kAuxTemorau, kAuxOutMasu, kAuxOutTe});
  entries.push_back({"いただきました", "いただきました", "いただく", kAuxTemorau, kAuxOutTa, kAuxOutTe});
  entries.push_back({"いただける", "いただける", "いただく", kAuxTemorau, kAuxOutBase, kAuxOutTe});
  entries.push_back({"いただけます", "いただけます", "いただく", kAuxTemorau, kAuxOutMasu, kAuxOutTe});

  // === てくださる ===
  entries.push_back({"くださる", "くださる", "くださる", kAuxTekureru, kAuxOutBase, kAuxOutTe});
  entries.push_back({"くださった", "くださった", "くださる", kAuxTekureru, kAuxOutTa, kAuxOutTe});
  entries.push_back({"くださって", "くださって", "くださる", kAuxTekureru, kAuxOutTe, kAuxOutTe});
  entries.push_back({"ください", "ください", "くださる", kAuxTekureru, kAuxOutBase, kAuxOutTe});
  entries.push_back({"くださいます", "くださいます", "くださる", kAuxTekureru, kAuxOutMasu, kAuxOutTe});

  // === てほしい ===
  entries.push_back({"ほしい", "ほしい", "ほしい", kAuxTai, kAuxOutBase, kAuxOutTe});
  entries.push_back({"ほしかった", "ほしかった", "ほしい", kAuxTai, kAuxOutTa, kAuxOutTe});
  entries.push_back({"ほしくない", "ほしくない", "ほしい", kAuxTai, kAuxOutBase, kAuxOutTe});

  // === てある ===
  entries.push_back({"ある", "ある", "ある", kAuxTeiru, kAuxOutBase, kAuxOutTe});
  entries.push_back({"あった", "あった", "ある", kAuxTeiru, kAuxOutTa, kAuxOutTe});
  entries.push_back({"あります", "あります", "ある", kAuxTeiru, kAuxOutMasu, kAuxOutTe});

  // === Complex たい patterns ===
  entries.push_back({"たくなる", "たくなる", "たい", kAuxTai, kAuxOutBase, kVerbRenyokei});
  entries.push_back({"たくなった", "たくなった", "たい", kAuxTai, kAuxOutTa, kVerbRenyokei});
  entries.push_back({"たくなって", "たくなって", "たい", kAuxTai, kAuxOutTe, kVerbRenyokei});
  entries.push_back({"たくなります", "たくなります", "たい", kAuxTai, kAuxOutMasu, kVerbRenyokei});
  // たい + くなる + てくる compounds
  entries.push_back({"たくなってきた", "たくなってきた", "たい", kAuxTai, kAuxOutTa, kVerbRenyokei});
  entries.push_back({"たくなってきて", "たくなってきて", "たい", kAuxTai, kAuxOutTe, kVerbRenyokei});
  entries.push_back({"たくなってくる", "たくなってくる", "たい", kAuxTai, kAuxOutBase, kVerbRenyokei});
  entries.push_back({"たくなってきます", "たくなってきます", "たい", kAuxTai, kAuxOutMasu, kVerbRenyokei});

  // === Ability patterns ===
  entries.push_back({"ことができる", "ことができる", "ことができる", kAuxNai, kAuxOutBase, kVerbBase});
  entries.push_back({"ことができた", "ことができた", "ことができる", kAuxNai, kAuxOutTa, kVerbBase});
  entries.push_back({"ことができない", "ことができない", "ことができる", kAuxNai, kAuxOutBase, kVerbBase});

  // === ようになる ===
  entries.push_back({"ようになる", "ようになる", "ようになる", kAuxNai, kAuxOutBase, kAuxOutBase});
  entries.push_back({"ようになった", "ようになった", "ようになる", kAuxNai, kAuxOutTa, kAuxOutBase});
  entries.push_back({"ようになって", "ようになって", "ようになる", kAuxNai, kAuxOutTe, kAuxOutBase});

  // === Explanatory のだ/んだ ===
  entries.push_back({"んだ", "んだ", "のだ", kAuxNai, kAuxOutBase, kVerbBase});
  entries.push_back({"んです", "んです", "のだ", kAuxNai, kAuxOutMasu, kVerbBase});
  entries.push_back({"のだ", "のだ", "のだ", kAuxNai, kAuxOutBase, kVerbBase});
  entries.push_back({"のです", "のです", "のだ", kAuxNai, kAuxOutMasu, kVerbBase});

  // === Prohibition/Permission ===
  entries.push_back({"はいけない", "はいけない", "はいけない", kAuxNai, kAuxOutBase, kAuxOutTe});
  entries.push_back({"はならない", "はならない", "はならない", kAuxNai, kAuxOutBase, kAuxOutTe});
  entries.push_back({"もいい", "もいい", "もいい", kAuxNai, kAuxOutBase, kAuxOutTe});
  entries.push_back({"もいいですか", "もいいですか", "もいい", kAuxNai, kAuxOutBase, kAuxOutTe});

  // === べき patterns ===
  entries.push_back({"べきだ", "べきだ", "べきだ", kAuxNai, kAuxOutBase, kVerbBase});
  entries.push_back({"べきだった", "べきだった", "べきだ", kAuxNai, kAuxOutTa, kVerbBase});
  entries.push_back({"べきではない", "べきではない", "べきだ", kAuxNai, kAuxOutBase, kVerbBase});
  entries.push_back({"べきです", "べきです", "べきだ", kAuxNai, kAuxOutMasu, kVerbBase});

  // === ところだ (connects from various forms) ===
  // From base form (終止形): 食べるところだ
  entries.push_back({"ところだ", "ところだ", "ところだ", kAuxNai, kAuxOutBase, kVerbBase});
  entries.push_back({"ところだった", "ところだった", "ところだ", kAuxNai, kAuxOutTa, kVerbBase});
  entries.push_back({"ところです", "ところです", "ところだ", kAuxNai, kAuxOutMasu, kVerbBase});
  // From た form (past): 食べたところだ, いたところだった
  entries.push_back({"ところだ", "ところだ", "ところだ", kAuxNai, kAuxOutBase, kAuxOutTa});
  entries.push_back({"ところだった", "ところだった", "ところだ", kAuxNai, kAuxOutTa, kAuxOutTa});
  entries.push_back({"ところです", "ところです", "ところだ", kAuxNai, kAuxOutMasu, kAuxOutTa});
  entries.push_back({"ところでした", "ところでした", "ところだ", kAuxNai, kAuxOutTa, kAuxOutTa});
  // From auxiliary base form: 読んでいるところだ (ている形 + ところだ)
  entries.push_back({"ところだ", "ところだ", "ところだ", kAuxNai, kAuxOutBase, kAuxOutBase});
  entries.push_back({"ところだった", "ところだった", "ところだ", kAuxNai, kAuxOutTa, kAuxOutBase});
  entries.push_back({"ところです", "ところです", "ところだ", kAuxNai, kAuxOutMasu, kAuxOutBase});
  entries.push_back({"ところでした", "ところでした", "ところだ", kAuxNai, kAuxOutTa, kAuxOutBase});

  // === ばかりだ ===
  entries.push_back({"ばかりだ", "ばかりだ", "ばかりだ", kAuxNai, kAuxOutBase, kAuxOutTa});
  entries.push_back({"ばかりだった", "ばかりだった", "ばかりだ", kAuxNai, kAuxOutTa, kAuxOutTa});
  entries.push_back({"ばかりです", "ばかりです", "ばかりだ", kAuxNai, kAuxOutMasu, kAuxOutTa});

  // === っぱなし ===
  entries.push_back({"っぱなしだ", "っぱなしだ", "っぱなし", kAuxNai, kAuxOutBase, kVerbRenyokei});
  entries.push_back({"っぱなしで", "っぱなしで", "っぱなし", kAuxNai, kAuxOutTe, kVerbRenyokei});

  // === ざるを得ない ===
  entries.push_back({"ざるを得ない", "ざるをえない", "ざるを得ない", kAuxNai, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"ざるを得なかった", "ざるをえなかった", "ざるを得ない", kAuxNai, kAuxOutTa, kVerbMizenkei});

  // === ずにはいられない ===
  entries.push_back({"ずにはいられない", "ずにはいられない", "ずにはいられない", kAuxNai, kAuxOutBase, kVerbMizenkei});

  // === わけにはいかない ===
  // From verb base form: 行くわけにはいかない
  entries.push_back({"わけにはいかない", "わけにはいかない", "わけにはいかない", kAuxNai, kAuxOutBase, kVerbBase});
  entries.push_back({"わけにはいかなかった", "わけにはいかなかった", "わけにはいかない", kAuxNai, kAuxOutTa, kVerbBase});
  entries.push_back({"わけにはいきません", "わけにはいきません", "わけにはいかない", kAuxNai, kAuxOutMasu, kVerbBase});
  // From auxiliary base form: 書かないわけにはいかない (ない形 + わけにはいかない)
  entries.push_back({"わけにはいかない", "わけにはいかない", "わけにはいかない", kAuxNai, kAuxOutBase, kAuxOutBase});
  entries.push_back({"わけにはいかなかった", "わけにはいかなかった", "わけにはいかない", kAuxNai, kAuxOutTa, kAuxOutBase});
  entries.push_back({"わけにはいきません", "わけにはいきません", "わけにはいかない", kAuxNai, kAuxOutMasu, kAuxOutBase});

  // === Volitional + ている ===
  entries.push_back({"うとしている", "うとしている", "とする", kAuxNai, kAuxOutBase, kVerbVolitional});
  entries.push_back({"うとしていた", "うとしていた", "とする", kAuxNai, kAuxOutTa, kVerbVolitional});
  entries.push_back({"ようとしている", "ようとしている", "とする", kAuxNai, kAuxOutBase, kVerbVolitional});
  entries.push_back({"ようとしていた", "ようとしていた", "とする", kAuxNai, kAuxOutTa, kVerbVolitional});

  // === ようになる + ている/てくる ===
  entries.push_back({"ようになっている", "ようになっている", "ようになる", kAuxNai, kAuxOutBase, kAuxOutBase});
  entries.push_back({"ようになってきた", "ようになってきた", "ようになる", kAuxNai, kAuxOutTa, kAuxOutBase});

  // === Causative-passive + たい (させられ) ===
  entries.push_back({"させられたい", "させられたい", "させられる", kAuxSeru, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"させられたかった", "させられたかった", "させられる", kAuxSeru, kAuxOutTa, kVerbMizenkei});
  entries.push_back({"させられたくて", "させられたくて", "させられる", kAuxSeru, kAuxOutTe, kVerbMizenkei});
  entries.push_back({"させられたくない", "させられたくない", "させられる", kAuxSeru, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"させられたくなかった", "させられたくなかった", "させられる", kAuxSeru, kAuxOutTa, kVerbMizenkei});
  entries.push_back({"させられなくて", "させられなくて", "させられる", kAuxSeru, kAuxOutTe, kVerbMizenkei});
  entries.push_back({"させられなくなる", "させられなくなる", "させられる", kAuxSeru, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"させられなくなった", "させられなくなった", "させられる", kAuxSeru, kAuxOutTa, kVerbMizenkei});
  entries.push_back({"させられなくなって", "させられなくなって", "させられる", kAuxSeru, kAuxOutTe, kVerbMizenkei});

  // === Causative-passive + たい (せられ) ===
  entries.push_back({"せられたい", "せられたい", "せられる", kAuxSeru, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"せられたかった", "せられたかった", "せられる", kAuxSeru, kAuxOutTa, kVerbMizenkei});
  entries.push_back({"せられたくて", "せられたくて", "せられる", kAuxSeru, kAuxOutTe, kVerbMizenkei});
  entries.push_back({"せられたくない", "せられたくない", "せられる", kAuxSeru, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"せられたくなかった", "せられたくなかった", "せられる", kAuxSeru, kAuxOutTa, kVerbMizenkei});
  entries.push_back({"せられなくて", "せられなくて", "せられる", kAuxSeru, kAuxOutTe, kVerbMizenkei});
  entries.push_back({"せられなくなる", "せられなくなる", "せられる", kAuxSeru, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"せられなくなった", "せられなくなった", "せられる", kAuxSeru, kAuxOutTa, kVerbMizenkei});
  entries.push_back({"せられなくなって", "せられなくなって", "せられる", kAuxSeru, kAuxOutTe, kVerbMizenkei});
  entries.push_back({"せられました", "せられました", "せられる", kAuxSeru, kAuxOutTa, kVerbMizenkei});
  entries.push_back({"せられません", "せられません", "せられる", kAuxSeru, kAuxOutBase, kVerbMizenkei});

  // === される extended forms ===
  entries.push_back({"されなかった", "されなかった", "される", kAuxSeru, kAuxOutTa, kVerbMizenkei});
  entries.push_back({"されなくて", "されなくて", "される", kAuxSeru, kAuxOutTe, kVerbMizenkei});
  entries.push_back({"されました", "されました", "される", kAuxSeru, kAuxOutTa, kVerbMizenkei});
  entries.push_back({"されません", "されません", "される", kAuxSeru, kAuxOutBase, kVerbMizenkei});

  // === Passive + なくなって ===
  entries.push_back({"れなくなって", "れなくなって", "れる", kAuxReru, kAuxOutTe, kVerbMizenkei});
  entries.push_back({"られなくなって", "られなくなって", "られる", kAuxReru, kAuxOutTe, kVerbMizenkei});
  entries.push_back({"られなくなってしまう", "られなくなってしまう", "られる", kAuxReru, kAuxOutBase, kVerbMizenkei});
  entries.push_back({"られなくなってしまった", "られなくなってしまった", "られる", kAuxReru, kAuxOutTa, kVerbMizenkei});

  // === Obligation patterns (past forms) ===
  entries.push_back({"ないといけなかった", "ないといけなかった", "ないといけない", kAuxNai, kAuxOutTa, kVerbMizenkei});
  entries.push_back({"なければならなかった", "なければならなかった", "なければならない", kAuxNai, kAuxOutTa, kVerbMizenkei});
  entries.push_back({"なくてはいけなかった", "なくてはいけなかった", "なくてはいけない", kAuxNai, kAuxOutTa, kVerbMizenkei});
  entries.push_back({"なきゃならない", "なきゃならない", "なきゃならない", kAuxNai, kAuxOutBase, kVerbMizenkei});

  // === Prohibition/Permission (past forms) ===
  entries.push_back({"はいけなかった", "はいけなかった", "はいけない", kAuxNai, kAuxOutTa, kAuxOutTe});
  entries.push_back({"はだめだ", "はだめだ", "はだめだ", kAuxNai, kAuxOutBase, kAuxOutTe});
  entries.push_back({"はならなかった", "はならなかった", "はならない", kAuxNai, kAuxOutTa, kAuxOutTe});
  entries.push_back({"べきではなかった", "べきではなかった", "べきだ", kAuxNai, kAuxOutTa, kVerbBase});
  entries.push_back({"もかまわない", "もかまわない", "もかまわない", kAuxNai, kAuxOutBase, kAuxOutTe});
  entries.push_back({"もかまわなかった", "もかまわなかった", "もかまわない", kAuxNai, kAuxOutTa, kAuxOutTe});

  // === てみる conditional ===
  entries.push_back({"みれば", "みれば", "みる", kAuxTemiru, kAuxOutBase, kAuxOutTe});

  // === Explanatory んだ variants ===
  // Connects to base form: 食べるんだもん
  entries.push_back({"んだもの", "んだもの", "のだ", kAuxNai, kAuxOutBase, kVerbBase});
  entries.push_back({"んだもん", "んだもん", "のだ", kAuxNai, kAuxOutBase, kVerbBase});
  // Connects to た form: 書いたんだもん
  entries.push_back({"んだもの", "んだもの", "のだ", kAuxNai, kAuxOutBase, kAuxOutTa});
  entries.push_back({"んだもん", "んだもん", "のだ", kAuxNai, kAuxOutBase, kAuxOutTa});

  // === Polite request forms ===
  entries.push_back({"いただけますか", "いただけますか", "いただく", kAuxTemorau, kAuxOutMasu, kAuxOutTe});
  entries.push_back({"くださいました", "くださいました", "くださる", kAuxTekureru, kAuxOutTa, kAuxOutTe});
  entries.push_back({"おりまして", "おりまして", "おる", kAuxTeiru, kAuxOutTe, kAuxOutTe});

  // === ことができる extended ===
  entries.push_back({"ことができて", "ことができて", "ことができる", kAuxNai, kAuxOutTe, kVerbBase});
  entries.push_back({"ことができなかった", "ことができなかった", "ことができる", kAuxNai, kAuxOutTa, kVerbBase});

  // === ばかり extended ===
  entries.push_back({"ばかりなのに", "ばかりなのに", "ばかりだ", kAuxNai, kAuxOutBase, kAuxOutTa});

  // === っぱなし extended ===
  entries.push_back({"っぱなしにする", "っぱなしにする", "っぱなし", kAuxNai, kAuxOutBase, kVerbRenyokei});

  // === ざるを得ない polite ===
  entries.push_back({"ざるを得ません", "ざるをえません", "ざるを得ない", kAuxNai, kAuxOutMasu, kVerbMizenkei});

  // === ずにはいられない past ===
  entries.push_back({"ずにはいられなかった", "ずにはいられなかった", "ずにはいられない", kAuxNai, kAuxOutTa, kVerbMizenkei});

  // === ている extended for compound verbs ===
  entries.push_back({"すぎている", "すぎている", "すぎる", kAuxRenyokei, kAuxOutBase, kVerbRenyokei});
  entries.push_back({"かけている", "かけている", "かける", kAuxRenyokei, kAuxOutBase, kVerbRenyokei});
  entries.push_back({"続けている", "つづけている", "続ける", kAuxRenyokei, kAuxOutBase, kVerbRenyokei});
  entries.push_back({"直している", "なおしている", "直す", kAuxRenyokei, kAuxOutBase, kVerbRenyokei});

  // === てくる/ていく extended (いった, いって) ===
  entries.push_back({"いった", "いった", "いく", kAuxTeiku, kAuxOutTa, kAuxOutTe});
  entries.push_back({"いって", "いって", "いく", kAuxTeiku, kAuxOutTe, kAuxOutTe});

  // === Imperative forms for te-form compounds ===
  // てこい (持ってこい, やってこい) - kuru imperative after te-form
  entries.push_back({"こい", "こい", "くる", kAuxTekuru, kAuxOutBase, kAuxOutTe});
}

}  // namespace

const std::vector<AuxiliaryBase>& getAuxiliaryBases() {
  using namespace conn;

  static const std::vector<AuxiliaryBase> kBases = {
      // === Te-form attachments (て形接続) ===
      {"いる", "いる", VerbType::Ichidan, kAuxTeiru, kAuxOutTe},
      {"しまう", "しまう", VerbType::GodanWa, kAuxTeshimau, kAuxOutTe},
      {"おく", "おく", VerbType::GodanKa, kAuxTeoku, kAuxOutTe},
      {"くる", "くる", VerbType::Kuru, kAuxTekuru, kAuxOutTe},
      {"いく", "いく", VerbType::GodanKa, kAuxTeiku, kAuxOutTe},
      {"みる", "みる", VerbType::Ichidan, kAuxTemiru, kAuxOutTe},
      {"もらう", "もらう", VerbType::GodanWa, kAuxTemorau, kAuxOutTe},
      {"くれる", "くれる", VerbType::Ichidan, kAuxTekureru, kAuxOutTe},
      {"あげる", "あげる", VerbType::Ichidan, kAuxTeageru, kAuxOutTe},

      // === Mizenkei attachments (未然形接続) ===
      {"ない", "ない", VerbType::IAdjective, kAuxNai, kVerbMizenkei},
      {"れる", "れる", VerbType::Ichidan, kAuxReru, kVerbMizenkei},
      {"られる", "られる", VerbType::Ichidan, kAuxReru, kVerbMizenkei},
      {"せる", "せる", VerbType::Ichidan, kAuxSeru, kVerbMizenkei},
      {"させる", "させる", VerbType::Ichidan, kAuxSeru, kVerbMizenkei},

      // === Renyokei attachments (連用形接続) ===
      {"ます", "ます", VerbType::Unknown, kAuxMasu, kVerbRenyokei},  // Special
      {"たい", "たい", VerbType::IAdjective, kAuxTai, kVerbRenyokei},
      {"やすい", "やすい", VerbType::IAdjective, kAuxRenyokei, kVerbRenyokei},
      {"にくい", "にくい", VerbType::IAdjective, kAuxRenyokei, kVerbRenyokei},
      {"すぎる", "すぎる", VerbType::Ichidan, kAuxRenyokei, kVerbRenyokei},
      {"かける", "かける", VerbType::Ichidan, kAuxRenyokei, kVerbRenyokei},
      {"出す", "だす", VerbType::GodanSa, kAuxRenyokei, kVerbRenyokei},
      {"終わる", "おわる", VerbType::GodanRa, kAuxRenyokei, kVerbRenyokei},
      {"終える", "おえる", VerbType::Ichidan, kAuxRenyokei, kVerbRenyokei},
      {"続ける", "つづける", VerbType::Ichidan, kAuxRenyokei, kVerbRenyokei},
      {"直す", "なおす", VerbType::GodanSa, kAuxRenyokei, kVerbRenyokei},

      // === Base form attachments (終止形接続) ===
      // らしい: conjecture auxiliary (食べるらしい, 食べないらしい)
      {"らしい", "らしい", VerbType::IAdjective, kAuxRenyokei, kAuxOutBase},
  };

  return kBases;
}

std::vector<AuxiliaryEntry> expandAuxiliaryBase(const AuxiliaryBase& base) {
  // Benefactive te-attachments (てくれる, てもらう, てあげる) use limited forms
  // to avoid over-matching like 待ってくれない → 待つ (wrong)
  // Other te-attachments (ている, てしまう, etc.) keep full forms
  // because they form grammaticalized compound verbs (食べていない = not eating)
  bool is_benefactive = (base.left_id == conn::kAuxTemorau ||
                         base.left_id == conn::kAuxTekureru ||
                         base.left_id == conn::kAuxTeageru);

  switch (base.conj_type) {
    case VerbType::Ichidan:
      return is_benefactive ? generateWithStem(base, kIchidanTeAttach)
                            : generateWithStem(base, kIchidanFull);
    case VerbType::GodanWa:
      return is_benefactive ? generateWithStem(base, kGodanWaTeAttach)
                            : generateWithStem(base, kGodanWaFull);
    case VerbType::GodanKa:
      return generateWithStem(base, kGodanKaFull);
    case VerbType::GodanSa:
      return generateWithStem(base, kGodanSaFull);
    case VerbType::GodanRa:
      return generateWithStem(base, kGodanRaFull);
    case VerbType::Kuru:
      return generateFullForms(base, kKuruFull);
    case VerbType::IAdjective:
      return generateWithStem(base, kIAdjective);
    case VerbType::Unknown:
      if (base.surface == "ます") {
        return generateMasuForms(base);
      }
      return generateNoConjForms(base);
    default:
      return generateNoConjForms(base);
  }
}

std::vector<AuxiliaryEntry> generateAllAuxiliaries() {
  std::vector<AuxiliaryEntry> result;

  // Expand all base definitions
  for (const auto& base : getAuxiliaryBases()) {
    auto expanded = expandAuxiliaryBase(base);
    result.insert(result.end(), expanded.begin(), expanded.end());
  }

  // Add special patterns that cannot be auto-generated
  addSpecialPatterns(result);

  // Sort by surface length (longest first) for greedy matching
  std::sort(result.begin(), result.end(),
            [](const AuxiliaryEntry& lhs, const AuxiliaryEntry& rhs) {
              return lhs.surface.size() > rhs.surface.size();
            });

  return result;
}

}  // namespace suzume::grammar
