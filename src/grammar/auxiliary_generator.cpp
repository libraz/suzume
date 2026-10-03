/**
 * @file auxiliary_generator.cpp
 * @brief Auto-generation of auxiliary verb conjugation patterns
 */

#include "auxiliary_generator.h"

#include <algorithm>
#include <iterator>
#include <string_view>
#include <utility>

#include "conjugation.h"
#include "connection.h"
#include "core/utf8_constants.h"

namespace suzume::grammar {

namespace {

enum class AuxiliaryFormFamily : uint8_t {
  Standard,
  TeAttachment,
  Progressive,
  Sokuonbin,
  Masu,
  Positive,
};

struct AuxiliaryBase {
  std::string_view base_form;
  VerbType conj_type;
  uint16_t required_conn;
  AuxiliaryFormFamily form_family = AuxiliaryFormFamily::Standard;
};

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
// Note: Te-form entries are kept for inflection analysis but skipped during generation
// (te-form boundary: VERB renyokei/onbinkei + て(PARTICLE))
constexpr ConjSuffix kIchidanFull[] = {
    {"る", conn::kAuxOutBase},   {"た", conn::kAuxOutTa},       {"たら", conn::kAuxOutBase},
    {"て", conn::kAuxOutTe},     {"ます", conn::kAuxOutMasu},   {"ました", conn::kAuxOutTa},
    {"ない", conn::kAuxOutBase}, {"なかった", conn::kAuxOutTa}, {"なくて", conn::kAuxOutTe},
};

// Leading subsets of kIchidanFull, taken by count:
// - Te-attachment forms stop after て (no negative, no masu) so the verb and
//   politeness auxiliary stay separate.
// - Progressive いる forms stop after ました (no negative); the negative is
//   compositional い(mizenkei) + ない(AUX), e.g., 食べていない → 食べ+て+い+ない.
constexpr size_t kIchidanTeAttachCount = 4;
constexpr size_t kIchidanProgressiveCount = 6;

// Godan suffix tables (Wa/Ka/Sa/Ra and the いく 促音便 irregular) are derived at
// startup from Conjugation::getGodanRow() — see appendGodanWithStem() below — so
// the per-row phonology (onbin surface, 濁点 on た, い段/あ段 kana) lives only in
// getGodanRows() instead of being hand-copied into six parallel tables here.

// Kuru (カ変) - irregular, full forms
constexpr ConjSuffix kKuruFull[] = {
    {"くる", conn::kAuxOutBase},   {"きた", conn::kAuxOutTa},       {"きたら", conn::kAuxOutBase},
    {"きて", conn::kAuxOutTe},     {"きます", conn::kAuxOutMasu},   {"きました", conn::kAuxOutTa},
    {"こない", conn::kAuxOutBase}, {"こなかった", conn::kAuxOutTa}, {"こなくて", conn::kAuxOutTe},
};

// I-adjective (い形容詞)
constexpr ConjSuffix kIAdjective[] = {
    {"い", conn::kAuxOutBase},     {"かった", conn::kAuxOutTa},     {"くて", conn::kAuxOutTe},
    {"くない", conn::kAuxOutBase}, {"くなかった", conn::kAuxOutTa}, {"ければ", conn::kAuxOutBase},
    {"く", conn::kAuxOutBase},  // adverbial
};

// Masu (ます) - special (no stem)
constexpr ConjSuffix kMasu[] = {
    {"ます", conn::kAuxOutMasu},     {"ました", conn::kAuxOutTa},       {"ません", conn::kAuxOutBase},
    {"ましょう", conn::kAuxOutBase}, {"ませんでした", conn::kAuxOutTa},
};

// =============================================================================
// Table-Driven Generation (単一ジェネレータ関数)
// =============================================================================

// Generate forms using stem + suffix pattern
// Note: All entries including te-form are generated for inflection analysis.
// Connection scoring makes the grammatical path
// VERB(renyokei/onbinkei) + て(PARTICLE) win over a unified te-form.
void appendWithStem(const AuxiliaryBase& base, const ConjSuffix* suffixes, size_t suffix_count,
                    std::vector<AuxiliaryEntry>& result) {
  const std::string stem(utf8::dropLastChar(base.base_form));
  result.reserve(result.size() + suffix_count);
  for (size_t suffix_index = 0; suffix_index < suffix_count; ++suffix_index) {
    const ConjSuffix& suf = suffixes[suffix_index];
    result.push_back({stem + suf.suffix, suf.right_id, base.required_conn});
  }
}

// Append a godan base's suffixes in the fixed order the handwritten tables
// used: base, ta, tara, te, [masu, mashita, nai, nakatta, nakute].
// @param te_attach_only  Benefactive bases stop after te (no masu/negative), the
//                        old kGodanWaTeAttach subset.
// @param force_sokuonbin いく-type 促音便 irregular: onbin becomes っ (った/って);
//                        ます系 and 未然形 still follow the regular row.
void appendGodanWithStem(const AuxiliaryBase& base, bool te_attach_only, bool force_sokuonbin, bool include_negative,
                         std::vector<AuxiliaryEntry>& result) {
  const Conjugation::GodanRow* row_ptr = Conjugation::getGodanRow(base.conj_type);
  if (row_ptr == nullptr) {
    return;
  }
  Conjugation::GodanRow row = *row_ptr;
  if (force_sokuonbin) {
    row.onbin = "っ";
  }
  const GodanVowels vowels = encodeGodanVowels(row);
  const std::string onbin = onbinFormOf(row);
  const std::string ta_kana = row.voiced_ta ? "だ" : "た";
  const std::string te_kana = row.voiced_ta ? "で" : "て";
  const std::string stem(utf8::dropLastChar(base.base_form));

  result.reserve(result.size() + (te_attach_only ? 4 : (include_negative ? 9 : 6)));
  result.push_back({stem + vowels.base, conn::kAuxOutBase, base.required_conn});
  result.push_back({stem + onbin + ta_kana, conn::kAuxOutTa, base.required_conn});
  result.push_back({stem + onbin + ta_kana + "ら", conn::kAuxOutBase, base.required_conn});
  result.push_back({stem + onbin + te_kana, conn::kAuxOutTe, base.required_conn});
  if (te_attach_only) {
    return;
  }
  result.push_back({stem + vowels.i + "ます", conn::kAuxOutMasu, base.required_conn});
  result.push_back({stem + vowels.i + "ました", conn::kAuxOutTa, base.required_conn});
  if (!include_negative) {
    return;
  }
  result.push_back({stem + vowels.a + "ない", conn::kAuxOutBase, base.required_conn});
  result.push_back({stem + vowels.a + "なかった", conn::kAuxOutTa, base.required_conn});
  result.push_back({stem + vowels.a + "なくて", conn::kAuxOutTe, base.required_conn});
}

// Generate forms using full forms (no stem, for irregular verbs)
void appendFullForms(const AuxiliaryBase& base, const ConjSuffix* forms, size_t form_count,
                     std::vector<AuxiliaryEntry>& result) {
  result.reserve(result.size() + form_count);
  for (size_t form_index = 0; form_index < form_count; ++form_index) {
    const ConjSuffix& form = forms[form_index];
    result.push_back({form.suffix, form.right_id, base.required_conn});
  }
}

// No conjugation - single form only
void appendNoConjForm(const AuxiliaryBase& base, std::vector<AuxiliaryEntry>& result) {
  result.push_back({std::string(base.base_form), conn::kAuxOutBase, base.required_conn});
}

// Add special patterns that cannot be auto-generated
struct SpecialPattern {
  const char* surface;
  uint16_t right_id;
  uint16_t required_conn;
};

void addSpecialPatterns(std::vector<AuxiliaryEntry>& entries) {
  using namespace conn;
  static constexpr SpecialPattern kPatterns[] = {

      // === Past/Conditional た系 (voiced variants) ===
      {"た", kAuxOutTa, kVerbOnbinkei},
      {"だ", kAuxOutTa, kVerbOnbinkei},
      {"たら", kAuxOutBase, kVerbOnbinkei},
      {"だら", kAuxOutBase, kVerbOnbinkei},

      // === Te-form (voiced variants) ===
      // These entries are needed for inflection analysis (matching て/で after onbin stems).
      // For tokenization, the PARTICLE て/で in entries.cpp competes with these AUXILIARY entries.
      // Connection rules give the conjunctive-particle path a grammatical bonus.
      {"て", kAuxOutTe, kVerbOnbinkei},
      {"で", kAuxOutTe, kVerbOnbinkei},

      // === Tari form ===
      {"たり", kAuxOutBase, kVerbOnbinkei},
      {"だり", kAuxOutBase, kVerbOnbinkei},
      {"たりする", kAuxOutBase, kVerbOnbinkei},
      {"だりする", kAuxOutBase, kVerbOnbinkei},
      {"たりした", kAuxOutTa, kVerbOnbinkei},
      {"だりした", kAuxOutTa, kVerbOnbinkei},
      {"たりして", kAuxOutTe, kVerbOnbinkei},
      {"だりして", kAuxOutTe, kVerbOnbinkei},

      // === Conditional ば ===
      {"ば", kAuxOutBase, kVerbKatei},

      // === Classical negation ず (古語否定) - connects to mizenkei ===
      // 尽きず, せず, 知らず etc.
      {"ず", kAuxOutBase, kVerbMizenkei},
      {"ずに", kAuxOutBase, kVerbMizenkei},
      {"ずとも", kAuxOutBase, kVerbMizenkei},

      // === Classical negation ぬ (文語否定 連体形) - connects to mizenkei ===
      // 消えぬ炎, 揃わぬ意見, 知れぬ心 etc.
      {"ぬ", kAuxOutBase, kVerbMizenkei},

      // === Volitional ===
      {"う", kAuxOutBase, kVerbVolitional},
      {"よう", kAuxOutBase, kVerbVolitional},

      // === Negative conjecture まい (打消推量) ===
      // まい attaches to:
      // - Godan 終止形: 行くまい, 書くまい, 言うまい
      // - Ichidan 未然形: 食べまい, 見まい, 出来まい (でき + まい)
      // - Kuru 未然形: こまい
      // - Suru 未然形: しまい
      {"まい", kAuxOutBase, kVerbBase},
      {"まい", kAuxOutBase, kVerbMizenkei},

      // Removed: Volitional + とする (うとする, ようとする, etc.)
      // These are multi-word constructions (volitional + quotative と + する) that
      // should be split as う+と+する, not absorbed as single auxiliary suffixes.
      // See also: DoesNotGenerateMultiWordConstructions test.

      // === Renyokei compounds ===
      {"ながら", kAuxOutBase, kVerbRenyokei},

      // === Sou form (appearance) ===
      {"そう", kAuxOutBase, kVerbRenyokei},
      {"そうだ", kAuxOutBase, kVerbRenyokei},
      {"そうだった", kAuxOutTa, kVerbRenyokei},
      {"そうです", kAuxOutBase, kVerbRenyokei},
      {"そうでした", kAuxOutTa, kVerbRenyokei},

      // === Potential stem endings ===
      {"る", kAuxOutBase, kVerbPotential},
      {"た", kAuxOutTa, kVerbPotential},
      {"て", kAuxOutTe, kVerbPotential},
      {"ない", kAuxOutBase, kVerbPotential},
      {"なかった", kAuxOutTa, kVerbPotential},
      {"ます", kAuxOutMasu, kVerbPotential},
      {"ました", kAuxOutTa, kVerbPotential},
      {"ません", kAuxOutBase, kVerbPotential},
      {"ませんでした", kAuxOutTa, kVerbPotential},

      // === Contracted negative (ん) ===
      // Colloquial contraction: ない → ん (e.g., 書かない → 書かん, わからない → わからん)
      {"ん", kAuxOutBase, kVerbMizenkei},

      // === Negative te-form ===
      {"ないで", kAuxOutTe, kVerbMizenkei},
      {"ないでいる", kAuxOutBase, kVerbMizenkei},
      {"ないでいた", kAuxOutTa, kVerbMizenkei},

      // === Obligation patterns ===
      {"ないといけない", kAuxOutBase, kVerbMizenkei},
      {"なければならない", kAuxOutBase, kVerbMizenkei},
      {"なくてはいけない", kAuxOutBase, kVerbMizenkei},
      {"なきゃいけない", kAuxOutBase, kVerbMizenkei},
      {"なくちゃ", kAuxOutBase, kVerbMizenkei},
      // Note: bare なきゃ/なけりゃ (colloquial contractions of なければ) are NOT
      // auxiliary suffixes here. They are standalone dictionary auxiliaries
      // (entries.cpp: AuxNegativeNai), so verbs split as mizenkei + なきゃ,
      // mirroring the なければ → mizenkei + なけれ + ば split. Registering them
      // here would fuse kanji-stem godan verbs into a single token (書かなきゃ).

      // === I-adjective endings (stem attachments) ===
      {"い", kAuxOutBase, kIAdjStem},
      {"かった", kAuxOutTa, kIAdjStem},
      {"くない", kAuxOutBase, kIAdjStem},
      {"くなかった", kAuxOutTa, kIAdjStem},
      {"くて", kAuxOutTe, kIAdjStem},
      {"ければ", kAuxOutBase, kIAdjStem},
      {"く", kAuxOutBase, kIAdjStem},
      {"かったら", kAuxOutBase, kIAdjStem},
      {"くなる", kAuxOutBase, kIAdjStem},
      {"くなった", kAuxOutTa, kIAdjStem},
      {"くなって", kAuxOutTe, kIAdjStem},
      {"さ", kAuxOutBase, kIAdjStem},
      {"そう", kAuxOutBase, kIAdjStem},
      {"そうだ", kAuxOutBase, kIAdjStem},
      {"そうな", kAuxOutBase, kIAdjStem},
      {"そうに", kAuxOutBase, kIAdjStem},

      // === I-adjective + すぎる (from stem) ===
      {"すぎる", kAuxOutBase, kIAdjStem},
      {"すぎた", kAuxOutTa, kIAdjStem},
      {"すぎて", kAuxOutTe, kIAdjStem},
      {"すぎます", kAuxOutMasu, kIAdjStem},

      // === Causative-passive (させられる, せられる, される) ===
      {"させられる", kAuxOutBase, kVerbMizenkei},
      {"させられた", kAuxOutTa, kVerbMizenkei},
      {"させられて", kAuxOutTe, kVerbMizenkei},
      {"させられない", kAuxOutBase, kVerbMizenkei},
      {"させられます", kAuxOutMasu, kVerbMizenkei},
      {"せられる", kAuxOutBase, kVerbMizenkei},
      {"せられた", kAuxOutTa, kVerbMizenkei},
      {"せられて", kAuxOutTe, kVerbMizenkei},
      {"せられない", kAuxOutBase, kVerbMizenkei},
      {"せられます", kAuxOutMasu, kVerbMizenkei},
      {"される", kAuxOutBase, kVerbMizenkei},
      {"された", kAuxOutTa, kVerbMizenkei},
      {"されて", kAuxOutTe, kVerbMizenkei},
      {"されない", kAuxOutBase, kVerbMizenkei},
      {"されます", kAuxOutMasu, kVerbMizenkei},
      // Classical べき pattern for Suru passive (装飾されべき → 装飾 + されべき)
      {"されべき", kAuxOutBase, kVerbMizenkei},

      // === なくなる patterns ===
      {"なくなる", kAuxOutBase, kVerbMizenkei},
      {"なくなった", kAuxOutTa, kVerbMizenkei},
      {"なくなって", kAuxOutTe, kVerbMizenkei},
      {"なくなってしまう", kAuxOutBase, kVerbMizenkei},
      {"なくなってしまった", kAuxOutTa, kVerbMizenkei},

      // === Potential + なくなる ===
      {"なくなる", kAuxOutBase, kVerbPotential},
      {"なくなった", kAuxOutTa, kVerbPotential},
      {"なくなって", kAuxOutTe, kVerbPotential},

      // === Passive + なくなる ===
      {"れなくなる", kAuxOutBase, kVerbMizenkei},
      {"れなくなった", kAuxOutTa, kVerbMizenkei},
      {"られなくなる", kAuxOutBase, kVerbMizenkei},
      {"られなくなった", kAuxOutTa, kVerbMizenkei},

      // === Passive + たい (desiderative) ===
      // E.g., 食べられたい (want to be eaten/able to eat), 見られたい (want to be seen)
      {"られたい", kAuxOutBase, kVerbMizenkei},
      {"られたかった", kAuxOutTa, kVerbMizenkei},
      {"られたくて", kAuxOutTe, kVerbMizenkei},
      {"られたくない", kAuxOutBase, kVerbMizenkei},
      {"られたくなかった", kAuxOutTa, kVerbMizenkei},
      // Godan passive + たい (e.g., 読まれたい, 書かれたい)
      {"れたい", kAuxOutBase, kVerbMizenkei},
      {"れたかった", kAuxOutTa, kVerbMizenkei},
      {"れたくて", kAuxOutTe, kVerbMizenkei},
      {"れたくない", kAuxOutBase, kVerbMizenkei},
      {"れたくなかった", kAuxOutTa, kVerbMizenkei},
      // Passive + べき (classical obligation: 書かれべき, 読まれべき)
      {"れべき", kAuxOutBase, kVerbMizenkei},
      {"られべき", kAuxOutBase, kVerbMizenkei},

      // === Colloquial てしまう contractions ===
      // Note: Connect to both kVerbOnbinkei (for Godan) and kVerbRenyokei (for Ichidan)
      // because ちゃう replaces てしまう, and て connects to onbin for Godan but renyokei for Ichidan
      // Godan: 書いちゃった = 書い (onbin) + ちゃった
      // Ichidan: 食べちゃった = 食べ (renyokei) + ちゃった
      {"ちゃう", kAuxOutBase, kVerbOnbinkei},
      {"ちゃった", kAuxOutTa, kVerbOnbinkei},
      {"ちゃって", kAuxOutTe, kVerbOnbinkei},
      {"じゃう", kAuxOutBase, kVerbOnbinkei},
      {"じゃった", kAuxOutTa, kVerbOnbinkei},
      {"じゃって", kAuxOutTe, kVerbOnbinkei},
      // Renyokei versions for Ichidan verbs (ちゃう only, not じゃう)
      // じゃう is for Godan voiced onbin (読んで→読んじゃ), not Ichidan
      // Ichidan uses unvoiced て (食べて→食べちゃ)
      {"ちゃう", kAuxOutBase, kVerbRenyokei},
      {"ちゃった", kAuxOutTa, kVerbRenyokei},
      {"ちゃって", kAuxOutTe, kVerbRenyokei},

      // === Colloquial ておく contraction ===
      // Godan onbin: やっとく, 書いとく - connects to 音便形
      {"とく", kAuxOutBase, kVerbOnbinkei},
      {"といた", kAuxOutTa, kVerbOnbinkei},
      {"といて", kAuxOutTe, kVerbOnbinkei},
      // Ichidan renyokei: 見とく, 食べとく - connects to 連用形
      {"とく", kAuxOutBase, kVerbRenyokei},
      {"といた", kAuxOutTa, kVerbRenyokei},
      {"といて", kAuxOutTe, kVerbRenyokei},
      // Voiced onbin: 読んどく, 飲んどく, 死んどく - で→ど contraction
      // Pattern is: 読ん (onbin stem) + どく (voiced contraction)
      // Same structure as でる/でた for ている contraction
      {"どく", kAuxOutBase, kVerbOnbinkei},
      {"どいた", kAuxOutTa, kVerbOnbinkei},
      {"どいて", kAuxOutTe, kVerbOnbinkei},

      // === Colloquial ている contraction (てる) ===
      // してる, 食べてる, 見てる - contracts ている to てる
      // These connect after te-form verbs (kAuxOutTe)
      {"てる", kAuxOutBase, kAuxOutTe},
      {"てた", kAuxOutTa, kAuxOutTe},
      {"てて", kAuxOutTe, kAuxOutTe},
      {"てない", kAuxOutBase, kAuxOutTe},
      {"てなかった", kAuxOutTa, kAuxOutTe},
      // Ichidan renyokei versions: 見てた = 見(renyokei) + てた
      // The て is part of the contracted aux, not a separate particle
      {"てる", kAuxOutBase, kVerbRenyokei},
      {"てた", kAuxOutTa, kVerbRenyokei},
      {"てない", kAuxOutBase, kVerbRenyokei},
      // でる/でた for voiced te-form (読んでる, 遊んでた)
      {"でる", kAuxOutBase, kAuxOutTe},
      {"でた", kAuxOutTa, kAuxOutTe},
      {"でて", kAuxOutTe, kAuxOutTe},
      {"でない", kAuxOutBase, kAuxOutTe},
      {"でなかった", kAuxOutTa, kAuxOutTe},
      // Godan onbin versions: 読んでた = 読ん(onbin) + でた (voiced sokuonbin)
      {"でる", kAuxOutBase, kVerbOnbinkei},
      {"でた", kAuxOutTa, kVerbOnbinkei},
      {"でない", kAuxOutBase, kVerbOnbinkei},
      // Godan sokuonbin versions: 知ってる = 知っ(sokuonbin stem) + てる
      // For GodanRa (知る→知っ), GodanTa (持つ→持っ), GodanWa (買う→買っ)
      // Note: aux is "てる" not "ってる" so that stem remains "知っ" ending with っ
      {"てる", kAuxOutBase, kVerbOnbinkei},
      {"てた", kAuxOutTa, kVerbOnbinkei},
      {"てない", kAuxOutBase, kVerbOnbinkei},
      {"てなかった", kAuxOutTa, kVerbOnbinkei},

      // === Suru-verb specific ている contractions ===
      // してる = し + ている contraction, full patterns for suru-verbs
      // Note: These use empty stem (stem="") for suru-verb matching
      {"してる", kAuxOutBase, kVerbOnbinkei},
      {"してた", kAuxOutTa, kVerbOnbinkei},
      {"してない", kAuxOutBase, kVerbOnbinkei},
      {"してなかった", kAuxOutTa, kVerbOnbinkei},

      // === Polite forms ===
      {"おる", kAuxOutBase, kAuxOutTe},
      {"おった", kAuxOutTa, kAuxOutTe},
      {"おります", kAuxOutMasu, kAuxOutTe},
      {"おりました", kAuxOutTa, kAuxOutTe},

      // === ていただく ===
      {"いただく", kAuxOutBase, kAuxOutTe},
      {"いただいた", kAuxOutTa, kAuxOutTe},
      {"いただいて", kAuxOutTe, kAuxOutTe},
      {"いただきます", kAuxOutMasu, kAuxOutTe},
      {"いただきました", kAuxOutTa, kAuxOutTe},
      {"いただける", kAuxOutBase, kAuxOutTe},
      {"いただけます", kAuxOutMasu, kAuxOutTe},

      // === てくださる ===
      {"くださる", kAuxOutBase, kAuxOutTe},
      {"くださった", kAuxOutTa, kAuxOutTe},
      {"くださって", kAuxOutTe, kAuxOutTe},
      {"ください", kAuxOutBase, kAuxOutTe},
      {"下さい", kAuxOutBase, kAuxOutTe},
      {"くださいます", kAuxOutMasu, kAuxOutTe},

      // === てほしい ===
      {"ほしい", kAuxOutBase, kAuxOutTe},
      {"ほしかった", kAuxOutTa, kAuxOutTe},
      {"ほしくない", kAuxOutBase, kAuxOutTe},

      // === Complex たい patterns ===
      {"たくなる", kAuxOutBase, kVerbRenyokei},
      {"たくなった", kAuxOutTa, kVerbRenyokei},
      {"たくなって", kAuxOutTe, kVerbRenyokei},
      {"たくなります", kAuxOutMasu, kVerbRenyokei},
      // たい + くなる + てくる compounds
      {"たくなってきた", kAuxOutTa, kVerbRenyokei},
      {"たくなってきて", kAuxOutTe, kVerbRenyokei},
      {"たくなってくる", kAuxOutBase, kVerbRenyokei},
      {"たくなってきます", kAuxOutMasu, kVerbRenyokei},

      // Removed: ことができる/ことができた/ことができない — multi-word constructions
      // (こと+が+でき+る), not conjugation suffixes. Causes false verb absorption
      // (e.g., 忘れることができなかった → single VERB token).

      // === ようになる ===
      {"ようになる", kAuxOutBase, kAuxOutBase},
      {"ようになった", kAuxOutTa, kAuxOutBase},
      {"ようになって", kAuxOutTe, kAuxOutBase},

      // === Explanatory のだ/んだ ===
      // Removed: のだ/んだ/のです/んです are discourse-level constructions,
      // not conjugation suffixes. Including them extends verb candidate surfaces
      // (e.g., 窺うのだ as single verb), preventing proper tokenization.
      // Verb base forms are still detected from shorter substrings.

      // Removed: はいけない, はならない, もいい, もいいですか — multi-word constructions
      // (V-て+は+いけない etc.), not conjugation suffixes. Causes false verb absorption.

      // === べき patterns ===
      // Note: べきだ/べきだった/べきではない/べきです removed — MeCab splits as
      // べき+だ, べき+だっ+た, etc. The L1 entry for べき (AuxVolitional) handles
      // the independent token. Compound suffix chains caused false merging
      // (e.g., 聞くべきだ → single VERB token instead of 聞く+べき+だ).

      // Removed: ところだ/ばかりだ/っぱなし/ざるを得ない/ずにはいられない/
      // わけにはいかない/うとしている/ようとしている/ようになっている — all are
      // multi-word constructions (formal noun+copula, particle chains, etc.) that
      // should be split into individual tokens, not absorbed as auxiliary suffixes.

      // === Causative-passive + たい (させられ) ===
      {"させられたい", kAuxOutBase, kVerbMizenkei},
      {"させられたかった", kAuxOutTa, kVerbMizenkei},
      {"させられたくて", kAuxOutTe, kVerbMizenkei},
      {"させられたくない", kAuxOutBase, kVerbMizenkei},
      {"させられたくなかった", kAuxOutTa, kVerbMizenkei},
      {"させられなくて", kAuxOutTe, kVerbMizenkei},
      {"させられなくなる", kAuxOutBase, kVerbMizenkei},
      {"させられなくなった", kAuxOutTa, kVerbMizenkei},
      {"させられなくなって", kAuxOutTe, kVerbMizenkei},

      // === Causative-passive + たい (せられ) ===
      {"せられたい", kAuxOutBase, kVerbMizenkei},
      {"せられたかった", kAuxOutTa, kVerbMizenkei},
      {"せられたくて", kAuxOutTe, kVerbMizenkei},
      {"せられたくない", kAuxOutBase, kVerbMizenkei},
      {"せられたくなかった", kAuxOutTa, kVerbMizenkei},
      {"せられなくて", kAuxOutTe, kVerbMizenkei},
      {"せられなくなる", kAuxOutBase, kVerbMizenkei},
      {"せられなくなった", kAuxOutTa, kVerbMizenkei},
      {"せられなくなって", kAuxOutTe, kVerbMizenkei},
      {"せられました", kAuxOutTa, kVerbMizenkei},
      {"せられません", kAuxOutBase, kVerbMizenkei},

      // === される extended forms ===
      {"されなかった", kAuxOutTa, kVerbMizenkei},
      {"されなくて", kAuxOutTe, kVerbMizenkei},
      {"されました", kAuxOutTa, kVerbMizenkei},
      {"されません", kAuxOutBase, kVerbMizenkei},

      // === Passive + なくなって ===
      {"れなくなって", kAuxOutTe, kVerbMizenkei},
      {"られなくなって", kAuxOutTe, kVerbMizenkei},
      {"られなくなってしまう", kAuxOutBase, kVerbMizenkei},
      {"られなくなってしまった", kAuxOutTa, kVerbMizenkei},

      // === Obligation patterns (past forms) ===
      {"ないといけなかった", kAuxOutTa, kVerbMizenkei},
      {"なければならなかった", kAuxOutTa, kVerbMizenkei},
      {"なくてはいけなかった", kAuxOutTa, kVerbMizenkei},
      {"なきゃならない", kAuxOutBase, kVerbMizenkei},

      // Removed: はいけなかった, はだめだ, はならなかった, べきではなかった,
      // もかまわない, もかまわなかった, ばかりなのに, っぱなしにする,
      // ざるを得ません, ずにはいられなかった
      // (extended forms of removed multi-word constructions above)

      // === てみる conditional ===
      {"みれば", kAuxOutBase, kAuxOutTe},

      // === Explanatory んだ variants (removed) ===
      // んだもの/んだもん removed along with のだ/んだ entries above.

      // === Polite request forms ===
      {"いただけますか", kAuxOutMasu, kAuxOutTe},
      {"くださいました", kAuxOutTa, kAuxOutTe},
      {"おりまして", kAuxOutTe, kAuxOutTe},

      // Removed: ことができて/ことができなかった — multi-word constructions
      // (see ことができる removal above).

      // ばかりなのに removed (multi-word construction)

      // っぱなしにする removed (multi-word construction)

      // ざるを得ません removed (multi-word construction)

      // ずにはいられなかった removed (multi-word construction)

      // === ている extended for compound verbs ===
      {"すぎている", kAuxOutBase, kVerbRenyokei},
      {"かけている", kAuxOutBase, kVerbRenyokei},
      {"続けている", kAuxOutBase, kVerbRenyokei},
      {"直している", kAuxOutBase, kVerbRenyokei},

      // Note: ていく forms いった/いって/いったら are generated from the いく
      // AuxiliaryBase with force_sokuonbin (AuxiliaryFormFamily::Sokuonbin), so they
      // are intentionally not duplicated here.

      // === Imperative forms for te-form compounds ===
      // てこい (持ってこい, やってこい) - kuru imperative after te-form
      {"こい", kAuxOutBase, kAuxOutTe},
  };

  entries.reserve(entries.size() + std::size(kPatterns));
  for (const auto& pattern : kPatterns) {
    entries.push_back({pattern.surface, pattern.right_id, pattern.required_conn});
  }
}

const auto& auxiliaryBases() {
  using namespace conn;
  static constexpr AuxiliaryBase kBases[] = {
      // === Te-form attachments (て形接続) ===
      {"いる", VerbType::Ichidan, kAuxOutTe, AuxiliaryFormFamily::Progressive},
      // てある has no regular 〜あらない negative paradigm. Its suppletive
      // negative is represented compositionally by the closed negative forms.
      {"ある", VerbType::GodanRa, kAuxOutTe, AuxiliaryFormFamily::Positive},
      {"しまう", VerbType::GodanWa, kAuxOutTe},
      {"おく", VerbType::GodanKa, kAuxOutTe},
      {"くる", VerbType::Kuru, kAuxOutTe},
      {"いく", VerbType::GodanKa, kAuxOutTe, AuxiliaryFormFamily::Sokuonbin},
      {"みる", VerbType::Ichidan, kAuxOutTe},
      {"もらう", VerbType::GodanWa, kAuxOutTe, AuxiliaryFormFamily::TeAttachment},
      {"くれる", VerbType::Ichidan, kAuxOutTe, AuxiliaryFormFamily::TeAttachment},
      {"あげる", VerbType::Ichidan, kAuxOutTe, AuxiliaryFormFamily::TeAttachment},

      // === Mizenkei attachments (未然形接続) ===
      {"ない", VerbType::IAdjective, kVerbMizenkei},
      {"れる", VerbType::Ichidan, kVerbMizenkei},
      {"られる", VerbType::Ichidan, kVerbMizenkei},
      {"せる", VerbType::Ichidan, kVerbMizenkei},
      {"させる", VerbType::Ichidan, kVerbMizenkei},

      // === Renyokei attachments (連用形接続) ===
      {"ます", VerbType::Unknown, kVerbRenyokei, AuxiliaryFormFamily::Masu},
      {"たい", VerbType::IAdjective, kVerbRenyokei},
      {"やすい", VerbType::IAdjective, kVerbRenyokei},
      {"にくい", VerbType::IAdjective, kVerbRenyokei},
      {"すぎる", VerbType::Ichidan, kVerbRenyokei},
      {"かける", VerbType::Ichidan, kVerbRenyokei},
      {"出す", VerbType::GodanSa, kVerbRenyokei},
      {"終わる", VerbType::GodanRa, kVerbRenyokei},
      {"終える", VerbType::Ichidan, kVerbRenyokei},
      {"続ける", VerbType::Ichidan, kVerbRenyokei},
      {"直す", VerbType::GodanSa, kVerbRenyokei},

      // === Base form attachments (終止形接続) ===
      // らしい: conjecture auxiliary (食べるらしい, 食べないらしい)
      {"らしい", VerbType::IAdjective, kAuxOutBase},
  };
  return kBases;
}

void appendAuxiliaryBase(const AuxiliaryBase& base, std::vector<AuxiliaryEntry>& result) {
  switch (base.conj_type) {
    case VerbType::Ichidan:
      if (base.form_family == AuxiliaryFormFamily::TeAttachment) {
        appendWithStem(base, kIchidanFull, kIchidanTeAttachCount, result);
        return;
      }
      if (base.form_family == AuxiliaryFormFamily::Progressive) {
        appendWithStem(base, kIchidanFull, kIchidanProgressiveCount, result);
        return;
      }
      appendWithStem(base, kIchidanFull, std::size(kIchidanFull), result);
      return;
    case VerbType::GodanWa:
    case VerbType::GodanKa:
    case VerbType::GodanSa:
    case VerbType::GodanRa:
      appendGodanWithStem(base, base.form_family == AuxiliaryFormFamily::TeAttachment,
                          base.form_family == AuxiliaryFormFamily::Sokuonbin,
                          base.form_family != AuxiliaryFormFamily::Positive, result);
      return;
    case VerbType::Kuru:
      appendFullForms(base, kKuruFull, std::size(kKuruFull), result);
      return;
    case VerbType::IAdjective:
      appendWithStem(base, kIAdjective, std::size(kIAdjective), result);
      return;
    case VerbType::Unknown:
      if (base.form_family == AuxiliaryFormFamily::Masu) {
        appendFullForms(base, kMasu, std::size(kMasu), result);
        return;
      }
      appendNoConjForm(base, result);
      return;
    default:
      appendNoConjForm(base, result);
      return;
  }
}

std::vector<AuxiliaryEntry> orderBySurfaceLength(std::vector<AuxiliaryEntry> entries) {
  size_t max_length = 0;
  for (const auto& entry : entries) {
    if (entry.surface.size() > max_length) {
      max_length = entry.surface.size();
    }
  }

  // Counting-sort by byte length. UTF-8 suffix matching also uses byte length,
  // so no character decoding is needed. Equal-length entries retain generation
  // order, making their ordering deterministic.
  std::vector<size_t> offsets(max_length + 1);
  for (const auto& entry : entries) {
    ++offsets[entry.surface.size()];
  }
  size_t offset = 0;
  for (size_t length = max_length + 1; length > 0;) {
    --length;
    const size_t count = offsets[length];
    offsets[length] = offset;
    offset += count;
  }

  std::vector<AuxiliaryEntry> ordered(entries.size());
  for (auto& entry : entries) {
    const size_t length = entry.surface.size();
    ordered[offsets[length]++] = std::move(entry);
  }
  return ordered;
}

}  // namespace

bool attachesToVerbRenyokei(std::string_view base_form) {
  const auto& bases = auxiliaryBases();
  return std::any_of(std::begin(bases), std::end(bases), [&](const AuxiliaryBase& base) {
    return base.base_form == base_form && base.required_conn == conn::kVerbRenyokei;
  });
}

std::vector<AuxiliaryEntry> generateAllAuxiliaries() {
  std::vector<AuxiliaryEntry> result;

  // Expand all base definitions
  for (const auto& base : auxiliaryBases()) {
    appendAuxiliaryBase(base, result);
  }

  // Add special patterns that cannot be auto-generated
  addSpecialPatterns(result);

  return orderBySurfaceLength(std::move(result));
}

}  // namespace suzume::grammar
