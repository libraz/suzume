/**
 * @file honorific_verbs.cpp
 * @brief Closed-class honorific, benefactive, modal, and aspectual subsidiary verb data
 */

#include "grammar/honorific_verbs.h"

#include <algorithm>
#include <string>
#include <vector>

#include "core/utf8_constants.h"
#include "grammar/conjugation.h"
#include "grammar/verb_endings.h"

namespace suzume::grammar {

using ::utf8::equalsAny;
using ::utf8::startsWithAny;

namespace {

// The kanji orthography of いたす belongs to the same closed class: 確認致します
// is the same construction as 確認いたします, and without the variant the
// continuative is absorbed into the preceding kanji run (確認致 + し).
constexpr std::string_view kHumbleHonorificRenyokei[] = {"いたし", "致し", "くださ", "いただき"};
// 給ふ and 候ふ are the classical members of the same class: they follow a
// continuative or an auxiliary exactly as くださる does (読ませ+給へ, 書かせ+候ふ).
constexpr std::string_view kHumbleHonorificLemmas[] = {"いたす", "致す",       "くださる", "いただく", "なさる",
                                                       "はする", "申し上げる", "給ふ",     "候ふ"};
constexpr std::string_view kBenefactiveRenyokei[] = {"もらい", "あげ"};
constexpr std::string_view kBenefactiveLemmas[] = {"もらう", "もらえる", "くれる", "あげる", "やる"};
constexpr std::string_view kPotentialBenefactiveLemmas[] = {"いただける", "もらえる"};
constexpr std::string_view kModalSubsidiaryRenyokei[] = {"かね"};
// 続ける is deliberately absent: productive V1+続ける compounds are themselves the
// search unit, so it belongs to the lexical V2 lexicon rather than to this class.
constexpr std::string_view kAspectualSubsidiaryLemmas[] = {"始める", "はじめる", "終わる", "おわる",
                                                           "終える", "おえる",   "過ぎる", "すぎる"};
// Verbs that exist only as a derivational suffix on a nominal host (形式ばる,
// 芝居がかる). Their conjugation lives in the dictionary; the host requirement
// cannot, so callers gate the entry on it.
constexpr std::string_view kBoundDerivationalSuffixVerbLemmas[] = {"ばる", "がかる", "じみる"};
// Suffixes that address or name a person. 様/氏 are absent on purpose: their
// kanji orthography cannot be confused with predicate material, so no caller
// needs a host check for them.
constexpr std::string_view kPersonalAddressSuffixes[] = {"さん", "ちゃん", "くん", "さま", "たん", "にゃん"};

}  // namespace

bool isHumbleHonorificRenyokei(std::string_view surface) {
  return equalsAny(surface, kHumbleHonorificRenyokei);
}

bool isHumbleHonorificLemma(std::string_view lemma) {
  return equalsAny(lemma, kHumbleHonorificLemmas);
}

bool isPotentialBenefactiveLemma(std::string_view lemma) {
  return equalsAny(lemma, kPotentialBenefactiveLemmas);
}

bool isBenefactiveLemma(std::string_view lemma) {
  return equalsAny(lemma, kBenefactiveLemmas);
}

bool isSubsidiaryHonorificRenyokei(std::string_view surface) {
  return isHumbleHonorificRenyokei(surface) || equalsAny(surface, kBenefactiveRenyokei);
}

bool isModalSubsidiaryRenyokei(std::string_view surface) {
  return equalsAny(surface, kModalSubsidiaryRenyokei);
}

bool startsHonorificSubsidiaryVerb(std::string_view surface) {
  return startsWithAny(surface, {"くださ", "いただ", "いたし", "いたす", "致し", "致す"});
}

bool isAspectualSubsidiaryLemma(std::string_view lemma) {
  return equalsAny(lemma, kAspectualSubsidiaryLemmas);
}

bool isBoundDerivationalSuffixVerbLemma(std::string_view lemma) {
  return equalsAny(lemma, kBoundDerivationalSuffixVerbLemmas);
}

bool spellsBoundDerivationalSuffixCell(std::string_view okurigana) {
  // The cells come from the same reverse-lookup ending table the inflection
  // analyzer reads, so this predicate accepts exactly the spellings that
  // analyzer can attribute to one of these lemmas. Building them from the
  // paradigm generators instead would spell the identical set while linking
  // every verb class's generator for the two classes this closed set uses.
  static const std::vector<std::string> kCells = [] {
    std::vector<std::string> cells;
    for (const std::string_view lemma : kBoundDerivationalSuffixVerbLemmas) {
      const std::string base(lemma);
      cells.push_back(base);
      const VerbType type = Conjugation::detectType(base);
      const std::string stem = Conjugation::getStem(base, type);
      for (const ConjForm form : kAllVerbConjForms) {
        for (const auto& ending : getVerbEndingsByForm(form)) {
          if (ending.verb_type == type) {
            cells.push_back(stem + ending.suffix);
          }
        }
      }
    }
    return cells;
  }();
  return std::any_of(kCells.begin(), kCells.end(), [okurigana](const std::string& cell) { return cell == okurigana; });
}

bool isPersonalAddressSuffix(std::string_view surface) {
  return equalsAny(surface, kPersonalAddressSuffixes);
}

}  // namespace suzume::grammar
