/**
 * @file tokenizer_dictionary_special.cpp
 * @brief Special grammar edges alongside dictionary candidates
 */

#include "analysis/candidate_constants.h"
#include "analysis/category_cost.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "tokenizer_dictionary_internal.h"

namespace suzume::analysis::tokenizer_dictionary_detail {

namespace {

// ことに is the evaluative adverb only before a nominal predicate with an
// explicit copula (ことに重要だ).  Restricting the context to that predicate
// shape preserves productive formal-noun phrases such as ことに関する説明 and
// 読むことにする.
bool startsEvaluativeKotoni(const std::vector<char32_t>& codepoints, size_t start_pos) {
  constexpr size_t kKotoniLength = 3;
  if (start_pos + kKotoniLength >= codepoints.size() || codepoints[start_pos] != U'こ' ||
      codepoints[start_pos + 1] != U'と' || codepoints[start_pos + 2] != U'に') {
    return false;
  }

  size_t predicate_end = start_pos + kKotoniLength;
  while (predicate_end < codepoints.size() &&
         normalize::classifyChar(codepoints[predicate_end]) == normalize::CharType::Kanji) {
    ++predicate_end;
  }
  return predicate_end > start_pos + kKotoniLength && predicate_end < codepoints.size() &&
         codepoints[predicate_end] == U'だ';
}

}  // namespace

void appendSpecialGrammarCandidates(core::Lattice& lattice, std::string_view text,
                                    const std::vector<char32_t>& codepoints, size_t start_pos, size_t byte_pos) {
  // 一方 is a conjunction only in its contrastive connective use before で.
  // Elsewhere it remains the ordinary noun (一方を選ぶ).
  if (start_pos + 2 < codepoints.size() && codepoints[start_pos] == U'一' && codepoints[start_pos + 1] == U'方' &&
      codepoints[start_pos + 2] == U'で') {
    lattice.addEdge("一方", static_cast<uint32_t>(start_pos), static_cast<uint32_t>(start_pos + 2),
                    core::PartOfSpeech::Conjunction, analysis::getCategoryCost(core::ExtendedPOS::Conjunction),
                    core::LatticeEdge::kFromDictionary, "一方", dictionary::ConjugationType::None,
                    core::CandidateOrigin::Dictionary, candidate::kDictionaryOriginConfidence, {},
                    core::ExtendedPOS::Conjunction, "contrastive_ippou");
  }

  // Note: ことに+なる carries no adverb candidate. こと+に+なる is the same
  // productive clause nominalization as こと+に+する, which has always split,
  // and an adverb reading of just one of the pair made the construction
  // tokenize two ways for no grammatical reason.

  if (startsEvaluativeKotoni(codepoints, start_pos)) {
    lattice.addEdge("ことに", static_cast<uint32_t>(start_pos), static_cast<uint32_t>(start_pos + 3),
                    core::PartOfSpeech::Adverb, candidate::kEvaluativeKotoniCandidateCost, 0, "ことに",
                    dictionary::ConjugationType::None, core::CandidateOrigin::Unknown,
                    candidate::kDictionaryOriginConfidence, {}, core::ExtendedPOS::Adverb, "evaluative_kotoni");
  }

  // あらん限り is the classical existential mizenkei あら plus the
  // euphonic ん form of conjectural む.  The following formal noun makes this
  // reading distinct from colloquial negative ん.
  if (grammar::startsClassicalAraNLimit(text.substr(byte_pos))) {
    lattice.addEdge("あら", static_cast<uint32_t>(start_pos), static_cast<uint32_t>(start_pos + 2),
                    core::PartOfSpeech::Verb, candidate::kClassicalAraNLimitCost, 0, "ある",
                    dictionary::ConjugationType::GodanRa, core::CandidateOrigin::Unknown,
                    candidate::kDictionaryOriginConfidence, {}, core::ExtendedPOS::VerbMizenkei,
                    "classical_ara_n_limit");
    lattice.addEdge(
        "ん", static_cast<uint32_t>(start_pos + 2), static_cast<uint32_t>(start_pos + 3), core::PartOfSpeech::Auxiliary,
        candidate::kClassicalAraNLimitCost, 0, "ん", dictionary::ConjugationType::None, core::CandidateOrigin::Unknown,
        candidate::kDictionaryOriginConfidence, {}, core::ExtendedPOS::AuxVolitional, "classical_ara_n_limit");
  }

  // A quoted final-particle pair (かなと) retains the two searchable
  // particles. The context avoids changing copular な or non-final かな…
  // sequences elsewhere.
  if (grammar::startsSentenceParticleKanaQuote(text.substr(byte_pos))) {
    lattice.addEdge("か", static_cast<uint32_t>(start_pos), static_cast<uint32_t>(start_pos + 1),
                    core::PartOfSpeech::Particle, candidate::kSentenceParticleQuoteCost, 0, "か",
                    dictionary::ConjugationType::None, core::CandidateOrigin::Unknown,
                    candidate::kDictionaryOriginConfidence, {}, core::ExtendedPOS::ParticleFinal,
                    "sentence_particle_kana_quote");
    lattice.addEdge("な", static_cast<uint32_t>(start_pos + 1), static_cast<uint32_t>(start_pos + 2),
                    core::PartOfSpeech::Particle, candidate::kSentenceParticleQuoteCost, 0, "な",
                    dictionary::ConjugationType::None, core::CandidateOrigin::Unknown,
                    candidate::kDictionaryOriginConfidence, {}, core::ExtendedPOS::ParticleFinal,
                    "sentence_particle_kana_quote");
  }

  const std::string_view long_final_particle = grammar::longFinalParticleBeforeQuote(text.substr(byte_pos));
  if (!long_final_particle.empty()) {
    const uint32_t particle_end = static_cast<uint32_t>(start_pos + normalize::utf8Length(long_final_particle));
    lattice.addEdge(long_final_particle, static_cast<uint32_t>(start_pos), particle_end, core::PartOfSpeech::Particle,
                    candidate::kLongSentenceParticleQuoteCost, 0, long_final_particle,
                    dictionary::ConjugationType::None, core::CandidateOrigin::Unknown,
                    candidate::kDictionaryOriginConfidence, {}, core::ExtendedPOS::ParticleFinal,
                    "long_sentence_particle_quote");
  }

  // そっか/こっか is the contracted demonstrative adverb (そう/こう) plus the
  // question particle か, not one kana noun.
  if (grammar::startsContractedDemonstrativeBeforeKa(text.substr(byte_pos))) {
    const std::string lemma = codepoints[start_pos] == U'そ' ? "そう" : "こう";
    lattice.addEdge(normalize::utf8Substr(text.substr(byte_pos), 0, 2), static_cast<uint32_t>(start_pos),
                    static_cast<uint32_t>(start_pos + 2), core::PartOfSpeech::Adverb,
                    candidate::kContractedDemonstrativeCost, 0, lemma, dictionary::ConjugationType::None,
                    core::CandidateOrigin::Unknown, candidate::kDictionaryOriginConfidence, {},
                    core::ExtendedPOS::Adverb, "contracted_demonstrative_ka");
  }

  // Edition 版 is a suffix only after a numeral or ordinal component
  // (第3版, 第三版).  Elsewhere it retains the independent noun reading
  // (新しい版), so do not register it as an unconditional dictionary suffix.
  if (start_pos > 0 && codepoints[start_pos] == U'版' && normalize::isNumeralCodepoint(codepoints[start_pos - 1])) {
    const float cost = analysis::getCategoryCost(core::ExtendedPOS::Suffix);
    lattice.addEdge("版", static_cast<uint32_t>(start_pos), static_cast<uint32_t>(start_pos + 1),
                    core::PartOfSpeech::Suffix, cost, core::LatticeEdge::kFromDictionary, "版",
                    dictionary::ConjugationType::None, core::CandidateOrigin::Dictionary,
                    candidate::kDictionaryOriginConfidence, {}, core::ExtendedPOS::Suffix, "ordinal_edition_suffix");
  }
}

}  // namespace suzume::analysis::tokenizer_dictionary_detail
