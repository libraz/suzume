/**
 * @file auxiliary_generator.cpp
 * @brief Auto-generation of auxiliary verb conjugation patterns
 */

#include "auxiliary_generator.h"

#include <algorithm>

namespace suzume::grammar {

namespace {

// UTF-8 helper: drop last character (assumes valid UTF-8 hiragana)
std::string dropLastChar(const std::string& s) {
  if (s.size() >= 3) {
    return s.substr(0, s.size() - 3);
  }
  return s;
}

// Conjugation suffix with output connection ID
struct ConjSuffix {
  std::string suffix;
  uint16_t right_id;
};

// === Ichidan conjugation (一段活用) ===
// いる → いた, いて, います, いない, いなかった...
std::vector<AuxiliaryEntry> generateIchidanForms(const AuxiliaryBase& base) {
  std::string stem = dropLastChar(base.surface);
  std::string reading_stem = dropLastChar(base.reading);

  static const std::vector<ConjSuffix> kSuffixes = {
      {"る", conn::kAuxOutBase},
      {"た", conn::kAuxOutTa},
      {"たら", conn::kAuxOutBase},  // conditional
      {"て", conn::kAuxOutTe},
      {"ます", conn::kAuxOutMasu},
      {"ました", conn::kAuxOutTa},
      {"ない", conn::kAuxOutBase},
      {"なかった", conn::kAuxOutTa},
      {"なくて", conn::kAuxOutTe},
  };

  std::vector<AuxiliaryEntry> result;
  for (const auto& suf : kSuffixes) {
    result.push_back({stem + suf.suffix, reading_stem + suf.suffix,
                      base.surface, base.left_id, suf.right_id,
                      base.required_conn});
  }
  return result;
}

// === Ichidan conjugation for te-form attachments (limited forms) ===
// くれる → くれた, くれます (NO negative forms)
// Te-form attachments like てくれる should NOT generate negative forms
// because the negation should apply to the main verb, not the auxiliary.
// E.g., 待ってくれない → 待って + くれない (two morphemes)
//       NOT 待ってくれない as single inflected form
std::vector<AuxiliaryEntry> generateTeAttachmentIchidanForms(
    const AuxiliaryBase& base) {
  std::string stem = dropLastChar(base.surface);
  std::string reading_stem = dropLastChar(base.reading);

  // Limited forms: base, past, past-conditional, te, polite, polite-past
  // NO: ない, なかった, なくて (these cause over-matching)
  static const std::vector<ConjSuffix> kSuffixes = {
      {"る", conn::kAuxOutBase},
      {"た", conn::kAuxOutTa},
      {"たら", conn::kAuxOutBase},
      {"て", conn::kAuxOutTe},
      {"ます", conn::kAuxOutMasu},
      {"ました", conn::kAuxOutTa},
  };

  std::vector<AuxiliaryEntry> result;
  for (const auto& suf : kSuffixes) {
    result.push_back({stem + suf.suffix, reading_stem + suf.suffix,
                      base.surface, base.left_id, suf.right_id,
                      base.required_conn});
  }
  return result;
}

// === Godan-Wa conjugation for te-form attachments (limited forms) ===
// もらう → もらった, もらいます (NO negative forms)
std::vector<AuxiliaryEntry> generateTeAttachmentGodanWaForms(
    const AuxiliaryBase& base) {
  std::string stem = dropLastChar(base.surface);
  std::string reading_stem = dropLastChar(base.reading);

  static const std::vector<ConjSuffix> kSuffixes = {
      {"う", conn::kAuxOutBase},
      {"った", conn::kAuxOutTa},
      {"ったら", conn::kAuxOutBase},
      {"って", conn::kAuxOutTe},
      {"います", conn::kAuxOutMasu},
      {"いました", conn::kAuxOutTa},
  };

  std::vector<AuxiliaryEntry> result;
  for (const auto& suf : kSuffixes) {
    result.push_back({stem + suf.suffix, reading_stem + suf.suffix,
                      base.surface, base.left_id, suf.right_id,
                      base.required_conn});
  }
  return result;
}

// === Godan-Wa conjugation (五段わ行活用) ===
// しまう → しまった, しまって, しまいます, しまわない...
std::vector<AuxiliaryEntry> generateGodanWaForms(const AuxiliaryBase& base) {
  std::string stem = dropLastChar(base.surface);  // しま
  std::string reading_stem = dropLastChar(base.reading);

  static const std::vector<ConjSuffix> kSuffixes = {
      {"う", conn::kAuxOutBase},
      {"った", conn::kAuxOutTa},
      {"ったら", conn::kAuxOutBase},  // conditional
      {"って", conn::kAuxOutTe},
      {"います", conn::kAuxOutMasu},
      {"いました", conn::kAuxOutTa},
      {"わない", conn::kAuxOutBase},
      {"わなかった", conn::kAuxOutTa},
      {"わなくて", conn::kAuxOutTe},
  };

  std::vector<AuxiliaryEntry> result;
  for (const auto& suf : kSuffixes) {
    result.push_back({stem + suf.suffix, reading_stem + suf.suffix,
                      base.surface, base.left_id, suf.right_id,
                      base.required_conn});
  }
  return result;
}

// === Godan-Ka conjugation for te-form attachments (limited forms) ===
// おく → おいた, おいて, おきます (NO negative forms)
std::vector<AuxiliaryEntry> generateTeAttachmentGodanKaForms(
    const AuxiliaryBase& base) {
  std::string stem = dropLastChar(base.surface);
  std::string reading_stem = dropLastChar(base.reading);

  static const std::vector<ConjSuffix> kSuffixes = {
      {"く", conn::kAuxOutBase},
      {"いた", conn::kAuxOutTa},
      {"いたら", conn::kAuxOutBase},
      {"いて", conn::kAuxOutTe},
      {"きます", conn::kAuxOutMasu},
      {"きました", conn::kAuxOutTa},
  };

  std::vector<AuxiliaryEntry> result;
  for (const auto& suf : kSuffixes) {
    result.push_back({stem + suf.suffix, reading_stem + suf.suffix,
                      base.surface, base.left_id, suf.right_id,
                      base.required_conn});
  }
  return result;
}

// === Godan-Ka conjugation (五段か行活用) ===
// おく → おいた, おいて, おきます, おかない...
std::vector<AuxiliaryEntry> generateGodanKaForms(const AuxiliaryBase& base) {
  std::string stem = dropLastChar(base.surface);  // お
  std::string reading_stem = dropLastChar(base.reading);

  static const std::vector<ConjSuffix> kSuffixes = {
      {"く", conn::kAuxOutBase},
      {"いた", conn::kAuxOutTa},
      {"いたら", conn::kAuxOutBase},  // conditional
      {"いて", conn::kAuxOutTe},
      {"きます", conn::kAuxOutMasu},
      {"きました", conn::kAuxOutTa},
      {"かない", conn::kAuxOutBase},
      {"かなかった", conn::kAuxOutTa},
      {"かなくて", conn::kAuxOutTe},
  };

  std::vector<AuxiliaryEntry> result;
  for (const auto& suf : kSuffixes) {
    result.push_back({stem + suf.suffix, reading_stem + suf.suffix,
                      base.surface, base.left_id, suf.right_id,
                      base.required_conn});
  }
  return result;
}

// === Godan-Sa conjugation (五段さ行活用) ===
// 出す → 出した, 出して, 出します, 出さない...
std::vector<AuxiliaryEntry> generateGodanSaForms(const AuxiliaryBase& base) {
  std::string stem = dropLastChar(base.surface);
  std::string reading_stem = dropLastChar(base.reading);

  static const std::vector<ConjSuffix> kSuffixes = {
      {"す", conn::kAuxOutBase},
      {"した", conn::kAuxOutTa},
      {"したら", conn::kAuxOutBase},  // conditional
      {"して", conn::kAuxOutTe},
      {"します", conn::kAuxOutMasu},
      {"しました", conn::kAuxOutTa},
      {"さない", conn::kAuxOutBase},
      {"さなかった", conn::kAuxOutTa},
      {"さなくて", conn::kAuxOutTe},
  };

  std::vector<AuxiliaryEntry> result;
  for (const auto& suf : kSuffixes) {
    result.push_back({stem + suf.suffix, reading_stem + suf.suffix,
                      base.surface, base.left_id, suf.right_id,
                      base.required_conn});
  }
  return result;
}

// === Godan-Ra conjugation (五段ら行活用) ===
// 終わる → 終わった, 終わって, 終わります, 終わらない...
std::vector<AuxiliaryEntry> generateGodanRaForms(const AuxiliaryBase& base) {
  std::string stem = dropLastChar(base.surface);
  std::string reading_stem = dropLastChar(base.reading);

  static const std::vector<ConjSuffix> kSuffixes = {
      {"る", conn::kAuxOutBase},
      {"った", conn::kAuxOutTa},
      {"ったら", conn::kAuxOutBase},  // conditional
      {"って", conn::kAuxOutTe},
      {"ります", conn::kAuxOutMasu},
      {"りました", conn::kAuxOutTa},
      {"らない", conn::kAuxOutBase},
      {"らなかった", conn::kAuxOutTa},
      {"らなくて", conn::kAuxOutTe},
  };

  std::vector<AuxiliaryEntry> result;
  for (const auto& suf : kSuffixes) {
    result.push_back({stem + suf.suffix, reading_stem + suf.suffix,
                      base.surface, base.left_id, suf.right_id,
                      base.required_conn});
  }
  return result;
}

// === Kuru conjugation (カ変活用) ===
// === Kuru conjugation for te-form attachments (limited forms) ===
// くる → きた, きて, きます (NO negative forms)
std::vector<AuxiliaryEntry> generateTeAttachmentKuruForms(
    const AuxiliaryBase& base) {
  static const std::vector<std::pair<std::string, uint16_t>> kForms = {
      {"くる", conn::kAuxOutBase},
      {"きた", conn::kAuxOutTa},
      {"きたら", conn::kAuxOutBase},
      {"きて", conn::kAuxOutTe},
      {"きます", conn::kAuxOutMasu},
      {"きました", conn::kAuxOutTa},
  };

  std::vector<AuxiliaryEntry> result;
  for (const auto& [form, right_id] : kForms) {
    result.push_back({form, form, base.surface,
                      base.left_id, right_id, base.required_conn});
  }
  return result;
}

// くる → きた, きて, きます, こない...
// NOTE: Kuru is irregular - forms are complete (no stem + suffix)
std::vector<AuxiliaryEntry> generateKuruForms(const AuxiliaryBase& base) {
  // Kuru is completely irregular - use full forms directly
  static const std::vector<std::pair<std::string, uint16_t>> kForms = {
      {"くる", conn::kAuxOutBase},
      {"きた", conn::kAuxOutTa},
      {"きたら", conn::kAuxOutBase},  // conditional
      {"きて", conn::kAuxOutTe},
      {"きます", conn::kAuxOutMasu},
      {"きました", conn::kAuxOutTa},
      {"こない", conn::kAuxOutBase},
      {"こなかった", conn::kAuxOutTa},
      {"こなくて", conn::kAuxOutTe},
  };

  std::vector<AuxiliaryEntry> result;
  for (const auto& [form, right_id] : kForms) {
    // For Kuru, the form is complete - no stem prefix needed
    result.push_back({form, form, base.surface,
                      base.left_id, right_id, base.required_conn});
  }
  return result;
}

// === I-Adjective conjugation (い形容詞活用) ===
// ない → なかった, なくて, なければ...
// たい → たかった, たくて, たくない...
std::vector<AuxiliaryEntry> generateIAdjectiveForms(const AuxiliaryBase& base) {
  std::string stem = dropLastChar(base.surface);  // な, た
  std::string reading_stem = dropLastChar(base.reading);

  static const std::vector<ConjSuffix> kSuffixes = {
      {"い", conn::kAuxOutBase},
      {"かった", conn::kAuxOutTa},
      {"くて", conn::kAuxOutTe},
      {"くない", conn::kAuxOutBase},
      {"くなかった", conn::kAuxOutTa},
      {"ければ", conn::kAuxOutBase},
      {"く", conn::kAuxOutBase},  // adverbial form
  };

  std::vector<AuxiliaryEntry> result;
  for (const auto& suf : kSuffixes) {
    result.push_back({stem + suf.suffix, reading_stem + suf.suffix,
                      base.surface, base.left_id, suf.right_id,
                      base.required_conn});
  }
  return result;
}

// === Masu conjugation (ます活用) ===
// ます → ました, ません, ましょう...
std::vector<AuxiliaryEntry> generateMasuForms(const AuxiliaryBase& base) {
  static const std::vector<std::pair<std::string, uint16_t>> kForms = {
      {"ます", conn::kAuxOutMasu},
      {"ました", conn::kAuxOutTa},
      {"ません", conn::kAuxOutBase},
      {"ましょう", conn::kAuxOutBase},
      {"ませんでした", conn::kAuxOutTa},
  };

  std::vector<AuxiliaryEntry> result;
  for (const auto& [form, right_id] : kForms) {
    result.push_back({form, form, "ます", base.left_id, right_id,
                      base.required_conn});
  }
  return result;
}

// === No conjugation (活用なし) ===
// た, て, たら, etc. - single form only
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

  // === Volitional ===
  entries.push_back({"う", "う", "う", kAuxNai, kAuxOutBase, kVerbVolitional});
  entries.push_back({"よう", "よう", "よう", kAuxNai, kAuxOutBase, kVerbVolitional});

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
  entries.push_back({"ちゃう", "ちゃう", "しまう", kAuxTeshimau, kAuxOutBase, kVerbOnbinkei});
  entries.push_back({"ちゃった", "ちゃった", "しまう", kAuxTeshimau, kAuxOutTa, kVerbOnbinkei});
  entries.push_back({"ちゃって", "ちゃって", "しまう", kAuxTeshimau, kAuxOutTe, kVerbOnbinkei});
  entries.push_back({"じゃう", "じゃう", "しまう", kAuxTeshimau, kAuxOutBase, kVerbOnbinkei});
  entries.push_back({"じゃった", "じゃった", "しまう", kAuxTeshimau, kAuxOutTa, kVerbOnbinkei});
  entries.push_back({"じゃって", "じゃって", "しまう", kAuxTeshimau, kAuxOutTe, kVerbOnbinkei});

  // === Colloquial ておく contraction ===
  entries.push_back({"とく", "とく", "おく", kAuxTeoku, kAuxOutBase, kAuxOutTe});
  entries.push_back({"といた", "といた", "おく", kAuxTeoku, kAuxOutTa, kAuxOutTe});

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
      if (is_benefactive) {
        return generateTeAttachmentIchidanForms(base);
      }
      return generateIchidanForms(base);
    case VerbType::GodanWa:
      if (is_benefactive) {
        return generateTeAttachmentGodanWaForms(base);
      }
      return generateGodanWaForms(base);
    case VerbType::GodanKa:
      // GodanKa te-attachments (おく, いく) should keep full forms
      return generateGodanKaForms(base);
    case VerbType::GodanSa:
      return generateGodanSaForms(base);
    case VerbType::GodanRa:
      return generateGodanRaForms(base);
    case VerbType::Kuru:
      // Kuru te-attachment (てくる) should keep full forms
      return generateKuruForms(base);
    case VerbType::IAdjective:
      return generateIAdjectiveForms(base);
    case VerbType::Unknown:
      // Special handling for ます
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
