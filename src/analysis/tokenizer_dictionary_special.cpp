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

void addClosedClassEdge(core::Lattice& lattice, std::string_view surface, size_t start_pos, size_t end_pos,
                        core::PartOfSpeech pos, core::ExtendedPOS epos, std::string_view pattern, uint8_t flags) {
  lattice.addEdge(surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(end_pos), pos,
                  analysis::getCategoryCost(epos), flags, surface, dictionary::ConjugationType::None,
                  core::CandidateOrigin::Dictionary, candidate::kDictionaryOriginConfidence, {}, epos, pattern);
}

void addGrammarEdge(core::Lattice& lattice, std::string_view surface, size_t start_pos, size_t end_pos,
                    core::PartOfSpeech pos, float cost, core::ExtendedPOS epos, std::string_view pattern,
                    std::string_view lemma, dictionary::ConjugationType conj_type) {
  lattice.addEdge(surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(end_pos), pos, cost, 0,
                  lemma.empty() ? surface : lemma, conj_type, core::CandidateOrigin::Unknown,
                  candidate::kDictionaryOriginConfidence, {}, epos, pattern);
}

void appendSpecialGrammarCandidates(core::Lattice& lattice, std::string_view text,
                                    const std::vector<char32_t>& codepoints, size_t start_pos, size_t byte_pos) {
  // 一方 is a conjunction only in its contrastive connective use before で.
  // Elsewhere it remains the ordinary noun (一方を選ぶ).
  if (start_pos + 2 < codepoints.size() && codepoints[start_pos] == U'一' && codepoints[start_pos + 1] == U'方' &&
      codepoints[start_pos + 2] == U'で') {
    addClosedClassEdge(lattice, "一方", start_pos, start_pos + 2, core::PartOfSpeech::Conjunction,
                       core::ExtendedPOS::Conjunction, "contrastive_ippou");
  }

  // Note: ことに+なる carries no adverb candidate. こと+に+なる is the same
  // productive clause nominalization as こと+に+する, which has always split,
  // and an adverb reading of just one of the pair made the construction
  // tokenize two ways for no grammatical reason.

  if (startsEvaluativeKotoni(codepoints, start_pos)) {
    addGrammarEdge(lattice, "ことに", start_pos, start_pos + 3, core::PartOfSpeech::Adverb,
                   candidate::kEvaluativeKotoniCandidateCost, core::ExtendedPOS::Adverb, "evaluative_kotoni");
  }

  // あらん限り is the classical existential mizenkei あら plus the
  // euphonic ん form of conjectural む.  The following formal noun makes this
  // reading distinct from colloquial negative ん.
  if (grammar::startsClassicalAraNLimit(text.substr(byte_pos))) {
    addGrammarEdge(lattice, "あら", start_pos, start_pos + 2, core::PartOfSpeech::Verb,
                   candidate::kClassicalAraNLimitCost, core::ExtendedPOS::VerbMizenkei, "classical_ara_n_limit", "ある",
                   dictionary::ConjugationType::GodanRa);
    addGrammarEdge(lattice, "ん", start_pos + 2, start_pos + 3, core::PartOfSpeech::Auxiliary,
                   candidate::kClassicalAraNLimitCost, core::ExtendedPOS::AuxVolitional, "classical_ara_n_limit");
  }

  // A quoted final-particle pair (かなと) retains the two searchable
  // particles. The context avoids changing copular な or non-final かな…
  // sequences elsewhere.
  if (grammar::startsSentenceParticleKanaQuote(text.substr(byte_pos))) {
    addGrammarEdge(lattice, "か", start_pos, start_pos + 1, core::PartOfSpeech::Particle,
                   candidate::kSentenceParticleQuoteCost, core::ExtendedPOS::ParticleFinal,
                   "sentence_particle_kana_quote");
    addGrammarEdge(lattice, "な", start_pos + 1, start_pos + 2, core::PartOfSpeech::Particle,
                   candidate::kSentenceParticleQuoteCost, core::ExtendedPOS::ParticleFinal,
                   "sentence_particle_kana_quote");
  }

  const std::string_view long_final_particle = grammar::longFinalParticleBeforeQuote(text.substr(byte_pos));
  if (!long_final_particle.empty()) {
    const size_t particle_end = start_pos + normalize::utf8Length(long_final_particle);
    addGrammarEdge(lattice, long_final_particle, start_pos, particle_end, core::PartOfSpeech::Particle,
                   candidate::kLongSentenceParticleQuoteCost, core::ExtendedPOS::ParticleFinal,
                   "long_sentence_particle_quote");
  }

  // そっか/こっか is the contracted demonstrative adverb (そう/こう) plus the
  // question particle か, not one kana noun.
  if (grammar::startsContractedDemonstrativeBeforeKa(text.substr(byte_pos))) {
    const std::string lemma = codepoints[start_pos] == U'そ' ? "そう" : "こう";
    addGrammarEdge(lattice, normalize::utf8Substr(text.substr(byte_pos), 0, 2), start_pos, start_pos + 2,
                   core::PartOfSpeech::Adverb, candidate::kContractedDemonstrativeCost, core::ExtendedPOS::Adverb,
                   "contracted_demonstrative_ka", lemma);
  }

  // Edition 版 is a suffix only after a numeral or ordinal component
  // (第3版, 第三版).  Elsewhere it retains the independent noun reading
  // (新しい版), so do not register it as an unconditional dictionary suffix.
  if (start_pos > 0 && codepoints[start_pos] == U'版' && normalize::isNumeralCodepoint(codepoints[start_pos - 1])) {
    addClosedClassEdge(lattice, "版", start_pos, start_pos + 1, core::PartOfSpeech::Suffix, core::ExtendedPOS::Suffix,
                       "ordinal_edition_suffix");
  }
}

}  // namespace suzume::analysis::tokenizer_dictionary_detail
