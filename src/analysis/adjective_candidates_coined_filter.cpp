/**
 * @file adjective_candidates_coined_filter.cpp
 * @brief Drop coined adjective candidates that the dictionary already spells as other words
 */

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "adjective_candidates_internal.h"
#include "analysis/dictionary_probe.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "unknown.h"
#include "verb_candidates_dictionary_probes.h"

namespace suzume::analysis {

using verb_helpers::isAdjectiveInDictionary;

namespace {

// Whether a word can open at @p pos: the text starts there, the script
// changes, or a case or topic particle closes what stands in front.
bool opensWordAt(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                 size_t pos) {
  if (pos == 0 || normalize::classifyChar(codepoints[pos - 1]) != normalize::CharType::Hiragana) {
    return true;
  }
  const auto* particle = lookupEntryInRange(*dict_manager, codepoints, pos - 1, pos, core::PartOfSpeech::Particle);
  return particle != nullptr && (particle->extended_pos == core::ExtendedPOS::ParticleCase ||
                                 particle->extended_pos == core::ExtendedPOS::ParticleTopic);
}

// A coined adjective cannot share its end with a registered adjective that
// opens at a different mora: one that opens inside it leaves the morae in
// front to another word (ねこ+かわい, マジ+ヤバかっ, ごはん+うまーい), and one
// that opens before it makes the coinage a fragment of that word (か+わい of
// かわい+すぎ). The registered adjective is measured after the prolonged-sound
// mark is dropped and must span three morae, so the bound closed-class
// adjectives ない・いい・よい stay usable as productive tails.
bool sharesEndWithDictionaryAdjective(const dictionary::DictionaryManager* dict_manager,
                                      const std::vector<char32_t>& codepoints, const UnknownCandidate& cand) {
  if (dict_manager == nullptr || cand.lemma_verified || cand.pos != core::PartOfSpeech::Adjective) {
    return false;
  }
  constexpr size_t kMinInnerAdjectiveMorae = 3;
  constexpr size_t kOuterProbe = 3;
  const bool is_stem = cand.extended_pos == core::ExtendedPOS::AdjStem;
  const size_t outer_start = cand.start > kOuterProbe ? cand.start - kOuterProbe : 0;
  for (size_t opening = outer_start; opening < cand.end; ++opening) {
    if (opening == cand.start) {
      continue;
    }
    if (opening < cand.start && normalize::classifyChar(codepoints[opening]) != normalize::CharType::Hiragana) {
      continue;
    }
    // A one-mora head opening a word is a bound adjectival prefix (た+やすい),
    // and a kanji adjective behind a kana head is a compound the script
    // already marks (もの+悲しい, うら+寂しい); neither is a separate word in
    // front. A one-mora head inside a kana run opens no word (ね|こ+かわい).
    if (opening > cand.start &&
        ((opening - cand.start < 2 && opensWordAt(dict_manager, codepoints, cand.start)) ||
         normalize::classifyChar(codepoints[opening]) != normalize::classifyChar(codepoints[cand.start]))) {
      continue;
    }
    std::string span;
    size_t span_morae = 0;
    for (size_t pos = opening; pos < cand.end; ++pos) {
      if (codepoints[pos] != U'ー') {
        normalize::encodeUtf8(codepoints[pos], span);
        ++span_morae;
      }
    }
    if (span_morae + (is_stem ? 1 : 0) < kMinInnerAdjectiveMorae) {
      continue;
    }
    if (dict_manager->lookupExact(span, core::PartOfSpeech::Adjective) != nullptr ||
        (is_stem && isAdjectiveInDictionary(dict_manager, span + "い"))) {
      return true;
    }
  }
  return false;
}

// A coined adjective spelling a registered function word (ごとく), or a
// one-mora case particle or past auxiliary in front of one (が+ごとく), is
// those words, not an adjective.
bool spansClosedHeadAndClosedWord(const dictionary::DictionaryManager* dict_manager,
                                  const std::vector<char32_t>& codepoints, const UnknownCandidate& cand) {
  if (dict_manager == nullptr || cand.lemma_verified || cand.pos != core::PartOfSpeech::Adjective ||
      cand.end < cand.start + 2) {
    return false;
  }
  // Nor is a span the dictionary already carries as a function word of
  // another lemma (ごとく of ごとし read as ごとい).
  const auto* whole = lookupEntryInRange(*dict_manager, codepoints, cand.start, cand.end);
  if (whole != nullptr && (whole->pos == core::PartOfSpeech::Auxiliary || whole->pos == core::PartOfSpeech::Particle) &&
      !whole->lemma.empty() && whole->lemma != cand.lemma) {
    return true;
  }
  if (cand.end < cand.start + 3) {
    return false;
  }
  const auto* head = lookupEntryInRange(*dict_manager, codepoints, cand.start, cand.start + 1);
  const bool closed_head = head != nullptr && (head->extended_pos == core::ExtendedPOS::ParticleCase ||
                                               head->extended_pos == core::ExtendedPOS::AuxTenseTa);
  if (!closed_head) {
    return false;
  }
  const auto* rest = lookupEntryInRange(*dict_manager, codepoints, cand.start + 1, cand.end);
  return rest != nullptr && (rest->pos == core::PartOfSpeech::Auxiliary || rest->pos == core::PartOfSpeech::Particle);
}

// A coined adjective spelling a registered auxiliary cell with that cell's own
// lemma (らしかっ of らしい) is the auxiliary itself, read with the wrong class.
bool respellsAuxiliaryCell(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                           const UnknownCandidate& cand) {
  if (dict_manager == nullptr || cand.pos != core::PartOfSpeech::Adjective || cand.lemma.empty()) {
    return false;
  }
  const auto* auxiliary =
      lookupEntryInRange(*dict_manager, codepoints, cand.start, cand.end, core::PartOfSpeech::Auxiliary);
  return auxiliary != nullptr && auxiliary->lemma == cand.lemma;
}

}  // namespace

void adj_detail::dropCoinedAdjectivesOverDictionaryAdjective(const dictionary::DictionaryManager* dict_manager,
                                                             const std::vector<char32_t>& codepoints,
                                                             std::vector<UnknownCandidate>& candidates,
                                                             size_t candidate_start) {
  candidates.erase(std::remove_if(candidates.begin() + static_cast<std::ptrdiff_t>(candidate_start), candidates.end(),
                                  [&](const UnknownCandidate& cand) {
                                    return sharesEndWithDictionaryAdjective(dict_manager, codepoints, cand) ||
                                           spansClosedHeadAndClosedWord(dict_manager, codepoints, cand) ||
                                           respellsAuxiliaryCell(dict_manager, codepoints, cand);
                                  }),
                   candidates.end());
}

}  // namespace suzume::analysis
