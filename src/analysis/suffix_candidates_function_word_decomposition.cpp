/**
 * @file suffix_candidates_function_word_decomposition.cpp
 * @brief Whether a span decomposes entirely into closed-class function words
 */

#include <string>
#include <vector>

#include "analysis/bigram_table.h"
#include "analysis/dictionary_probe.h"
#include "dictionary/dictionary.h"
#include "grammar/char_patterns.h"
#include "suffix_candidates.h"
#include "tokenizer_utils.h"

namespace suzume::analysis {

bool hasAuxiliaryParticleDecomposition(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                       const dictionary::DictionaryManager* dict_manager) {
  // Same floor as hasAuxiliaryChainDecomposition below, for the same reason: at
  // two morae the decomposition is an accident of how many one-mora auxiliaries
  // and particles exist (く+も for くも, ひ+も for ひも), not evidence that the
  // span spells no word.
  if (dict_manager == nullptr || end_pos < start_pos + 3) {
    return false;
  }
  if (hasExactPartOfSpeech(*dict_manager, codepoints, start_pos, end_pos, kOpenClassPartOfSpeechMask)) {
    return false;
  }
  // The aspect auxiliaries attach only to a verb stem, so with no word in front
  // of the run, or behind a closing は/が/を/も, none of them opens it
  // (どうん is not どう+ん, 彼は+とっさ is not とっ+さ).
  const bool lacks_left_host = particleMoraLacksHost(codepoints, start_pos);
  // Behind the auxiliary stands one particle, or a final particle plus the
  // modal tail that licenses a stack (だ+よ+ね, but not う+わ+べ).
  const auto final_particle_at = [&](size_t from, size_t to) {
    const auto* particle = lookupEntryInRange(*dict_manager, codepoints, from, to, core::PartOfSpeech::Particle);
    return particle != nullptr && particle->extended_pos == core::ExtendedPOS::ParticleFinal ? particle : nullptr;
  };
  for (size_t split = start_pos + 1; split < end_pos; ++split) {
    const auto* auxiliary =
        lookupEntryInRange(*dict_manager, codepoints, start_pos, split, core::PartOfSpeech::Auxiliary);
    if (auxiliary == nullptr || (lacks_left_host && auxiliary->extended_pos == core::ExtendedPOS::AuxAspectIru)) {
      continue;
    }
    if (lookupEntryInRange(*dict_manager, codepoints, split, end_pos, core::PartOfSpeech::Particle) != nullptr) {
      return true;
    }
    for (size_t tail_start = split + 1; tail_start < end_pos; ++tail_start) {
      const auto* tail = final_particle_at(tail_start, end_pos);
      if (final_particle_at(split, tail_start) != nullptr && tail != nullptr &&
          grammar::isFinalParticleStackTail(tail->surface)) {
        return true;
      }
    }
  }
  return false;
}

bool hasAuxiliaryChainDecomposition(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                    const dictionary::DictionaryManager* dict_manager) {
  // Two morae are spelled by too many one-mora auxiliaries to be evidence of
  // anything: な+す and か+ぬ decompose that way and are ordinary verbs.
  if (dict_manager == nullptr || end_pos < start_pos + 3) {
    return false;
  }
  // The two auxiliaries must also chain: the attributive copula な does not
  // host the aspect でる (ゆっくり+なでる is no な+でる).
  for (size_t split = start_pos + 1; split < end_pos; ++split) {
    const auto* head = lookupEntryInRange(*dict_manager, codepoints, start_pos, split, core::PartOfSpeech::Auxiliary);
    const auto* tail = lookupEntryInRange(*dict_manager, codepoints, split, end_pos, core::PartOfSpeech::Auxiliary);
    if (head != nullptr && tail != nullptr &&
        BigramTable::getCost(head->extended_pos, tail->extended_pos) < bigram_cost::kSevere) {
      return true;
    }
  }
  return false;
}

bool hasFunctionWordChainDecomposition(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                       const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || end_pos < start_pos + 3) {
    return false;
  }
  if (hasExactPartOfSpeech(*dict_manager, codepoints, start_pos, end_pos, kOpenClassPartOfSpeechMask)) {
    return false;
  }
  constexpr PartOfSpeechMask kFunctionMask =
      partOfSpeechMask(core::PartOfSpeech::Particle) | partOfSpeechMask(core::PartOfSpeech::Auxiliary);
  for (size_t particle_start = start_pos + 2; particle_start < end_pos; ++particle_start) {
    if (lookupEntryInRange(*dict_manager, codepoints, particle_start, end_pos, core::PartOfSpeech::Particle) ==
        nullptr) {
      continue;
    }
    // Every cell of the classical perfect is admitted only inside the chain its
    // own form implies, so its presence in the dictionary says nothing about
    // what an ordinary hiragana run spells: the one-mora continuative alone
    // would decompose にんじん as に+ん+じ+ん.
    if (maximalSegmentCount(*dict_manager, codepoints, start_pos, particle_start, core::PartOfSpeech::Auxiliary,
                            core::ExtendedPOS::AuxClassicalPerfect,
                            !particleMoraLacksHost(codepoints, start_pos)) >= 1) {
      return true;
    }
  }

  // A stack of sentence-final particles is closed on both sides, so no unknown
  // noun is hiding in it however short its members are (かなあ for か+なあ).
  // Requiring both halves to be final particles keeps this away from the runs a
  // one-mora head would otherwise claim (よそう, かばん).
  for (size_t split = start_pos + 1; split < end_pos; ++split) {
    const auto* stack_head =
        lookupEntryInRange(*dict_manager, codepoints, start_pos, split, core::PartOfSpeech::Particle);
    const auto* stack_tail =
        lookupEntryInRange(*dict_manager, codepoints, split, end_pos, core::PartOfSpeech::Particle);
    if (stack_head != nullptr && stack_tail != nullptr &&
        stack_head->extended_pos == core::ExtendedPOS::ParticleFinal &&
        stack_tail->extended_pos == core::ExtendedPOS::ParticleFinal) {
      return true;
    }
  }

  for (size_t split = start_pos + 2; split < end_pos; ++split) {
    const std::string head_surface = extractSubstring(codepoints, start_pos, split);
    // A focus particle, a sentence-final particle, or a pronoun — the one
    // nominal that is itself closed class, so a run opening with one has no
    // unknown noun to recover either (なにが for なに+が, これから, それでも,
    // かなあ for か+なあ).
    const auto* head = dict_manager->lookupExact(head_surface, core::PartOfSpeech::Particle);
    const bool focus_particle_head = head != nullptr && (head->extended_pos == core::ExtendedPOS::ParticleAdverbial ||
                                                         head->extended_pos == core::ExtendedPOS::ParticleBinding ||
                                                         head->extended_pos == core::ExtendedPOS::ParticleFinal);
    if (!focus_particle_head &&
        !hasExactPartOfSpeech(*dict_manager, head_surface, partOfSpeechMask(core::PartOfSpeech::Pronoun))) {
      continue;
    }
    if (hasExactPartOfSpeech(*dict_manager, codepoints, split, end_pos, kFunctionMask)) {
      return true;
    }
  }

  // A determiner only ever stands in front of a nominal, never inside one, so a
  // run that opens with one is two words however the rest reads. Requiring the
  // remainder to be attested keeps this to runs that actually have a
  // decomposition (この+すな) rather than any run that starts with those morae.
  constexpr PartOfSpeechMask kAttestedTailMask = kOpenClassPartOfSpeechMask | kFunctionMask |
                                                 partOfSpeechMask(core::PartOfSpeech::Pronoun) |
                                                 partOfSpeechMask(core::PartOfSpeech::Determiner);
  for (size_t split = start_pos + 1; split < end_pos; ++split) {
    if (!hasExactPartOfSpeech(*dict_manager, codepoints, start_pos, split,
                              partOfSpeechMask(core::PartOfSpeech::Determiner))) {
      continue;
    }
    if (hasExactPartOfSpeech(*dict_manager, codepoints, split, end_pos, kAttestedTailMask)) {
      return true;
    }
  }
  return false;
}

}  // namespace suzume::analysis
