#ifndef SUZUME_DICTIONARY_ENTRIES_ENTRIES_INTERNAL_H_
#define SUZUME_DICTIONARY_ENTRIES_ENTRIES_INTERNAL_H_

#include "core/types.h"
#include "dictionary/entries/entry_spec.h"

namespace suzume::dictionary::entries {

using POS = core::PartOfSpeech;
using EPOS = core::ExtendedPOS;

// =============================================================================
// Helper functions for concise DictionaryEntry construction (v0.8 simplified)
//
// Design: No cost parameter - cost is derived from ExtendedPOS via getCategoryCost()
// =============================================================================

// Particle helper: creates PARTICLE entry
// Usage: particle("が", EPOS::ParticleCase)
constexpr EntrySpec particle(const char* s, EPOS epos) {
  return {s, POS::Particle, epos, ""};
}

// Auxiliary helper: creates AUXILIARY entry
// Usage: aux("た", "た", EPOS::AuxTenseTa)
constexpr EntrySpec aux(const char* s, const char* lemma, EPOS epos) {
  return {s, POS::Auxiliary, epos, lemma};
}

// Verb helper: creates VERB entry
// Usage: verb("し", "する", EPOS::VerbRenyokei)
constexpr EntrySpec verb(const char* s, const char* lemma, EPOS epos) {
  return {s, POS::Verb, epos, lemma};
}

// Adjective helper: creates ADJECTIVE entry
// Usage: adj("美しい", "", EPOS::AdjBasic)
constexpr EntrySpec adj(const char* s, const char* lemma, EPOS epos) {
  return {s, POS::Adjective, epos, lemma};
}

// Na-adjective helper: creates NA_ADJECTIVE entry
// Usage: na_adj("静か", "")
constexpr EntrySpec na_adj(const char* s, const char* lemma = "") {
  return {s, POS::Adjective, EPOS::AdjNaAdj, lemma};
}

// Determiner helper: creates DETERMINER entry
// Usage: det("この")
constexpr EntrySpec det(const char* s, const char* lemma = "") {
  return {s, POS::Determiner, EPOS::Determiner, lemma};
}

constexpr EntrySpec quotative_det(const char* s, const char* lemma = "") {
  return {s, POS::Determiner, EPOS::DeterminerQuotative, lemma};
}

// Formal noun helper: creates FORMAL_NOUN entry (形式名詞)
// Usage: formal_noun("こと")
constexpr EntrySpec formal_noun(const char* s, const char* lemma = "") {
  return {s, POS::Noun, EPOS::NounFormal, lemma};
}

// General-noun helper.  Use only for closed lexical nominal classes whose
// part of speech cannot be supplied by a contextual grammar candidate.
constexpr EntrySpec noun(const char* s, const char* lemma = "") {
  return {s, POS::Noun, EPOS::Noun, lemma};
}

// Closed native numeral expressions (ひとつ, みっつ, …).
constexpr EntrySpec noun_number(const char* s, const char* lemma = "") {
  return {s, POS::Noun, EPOS::NounNumber, lemma};
}

// Conjunction helper: creates CONJUNCTION entry
// Usage: conj("しかし")
constexpr EntrySpec conj(const char* s, const char* lemma = "") {
  return {s, POS::Conjunction, EPOS::Conjunction, lemma};
}

// Adverb helper: creates ADVERB entry
// Usage: adv("とても")
constexpr EntrySpec adv(const char* s, const char* lemma = "") {
  return {s, POS::Adverb, EPOS::Adverb, lemma};
}

// Demonstrative adverbs used as quotative or manner pro-forms.
constexpr EntrySpec quotative_adv(const char* s, const char* lemma = "") {
  return {s, POS::Adverb, EPOS::AdverbQuotative, lemma};
}

// Interrogative adverbs that ask for a reason (なぜ, なんで, どうして).
constexpr EntrySpec interrogative_adv(const char* s, const char* lemma = "") {
  return {s, POS::Adverb, EPOS::AdverbInterrogative, lemma};
}

// Suffix helper: creates SUFFIX entry
// Usage: suffix("さん")
constexpr EntrySpec suffix(const char* s, const char* lemma = "") {
  return {s, POS::Suffix, EPOS::Suffix, lemma};
}

// Recent-completion suffix helper: creates a suffix with its own connection class.
constexpr EntrySpec suffix_recent_completion(const char* s, const char* lemma = "") {
  return {s, POS::Suffix, EPOS::SuffixRecentCompletion, lemma};
}

// Tendency suffix helper: creates a verb-continuative suffix with its own connection class.
constexpr EntrySpec suffix_tendency(const char* s, const char* lemma = "") {
  return {s, POS::Suffix, EPOS::SuffixTendency, lemma};
}

// Prefix helper: creates PREFIX entry
// Usage: prefix("お"), prefix("ご")
constexpr EntrySpec prefix(const char* s, const char* lemma = "") {
  return {s, POS::Prefix, EPOS::Prefix, lemma};
}

// Pronoun helper: creates PRONOUN entry
// Usage: pronoun("私")
constexpr EntrySpec pronoun(const char* s, const char* lemma = "") {
  return {s, POS::Pronoun, EPOS::Pronoun, lemma};
}

// Interrogative pronoun helper: creates a PRONOUN entry with its own
// connection class (何、誰、どこ、いつ).
constexpr EntrySpec pronoun_interrogative(const char* s, const char* lemma = "") {
  return {s, POS::Pronoun, EPOS::PronounInterrogative, lemma};
}

// Interjection helper: creates INTERJECTION entry
// Usage: intj("えっ")
constexpr EntrySpec intj(const char* s, const char* lemma = "") {
  return {s, POS::Interjection, EPOS::Interjection, lemma};
}

}  // namespace suzume::dictionary::entries

#endif  // SUZUME_DICTIONARY_ENTRIES_ENTRIES_INTERNAL_H_
