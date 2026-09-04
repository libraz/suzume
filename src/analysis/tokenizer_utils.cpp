/**
 * @file tokenizer_utils.cpp
 * @brief Utility functions for tokenizer
 */

#include "tokenizer_utils.h"

#include <algorithm>

#include "analysis/dictionary_probe.h"
#include "candidate_constants.h"
#include "core/utf8_constants.h"
#include "dictionary/dictionary.h"
#include "grammar/char_patterns.h"
#include "grammar/inflection.h"
#include "normalize/utf8.h"
#include "verb_candidates_helpers.h"

namespace suzume::analysis {

size_t findCharRegionEnd(const std::vector<normalize::CharType>& char_types, size_t start_pos, size_t max_len,
                         normalize::CharType target_type) {
  size_t end = start_pos;
  while (end < char_types.size() && end - start_pos < max_len && char_types[end] == target_type) {
    ++end;
  }
  return end;
}

bool hasKanjiSuruPredicateAt(const std::vector<char32_t>& codepoints,
                             const std::vector<normalize::CharType>& char_types, size_t start_pos,
                             size_t minimum_kanji_count) {
  const size_t predicate_end = findCharRegionEnd(
      char_types, start_pos, char_types.size() - std::min(start_pos, char_types.size()), normalize::CharType::Kanji);
  return predicate_end - start_pos >= minimum_kanji_count && predicate_end + 1 < codepoints.size() &&
         codepoints[predicate_end] == U'す' && codepoints[predicate_end + 1] == U'る';
}

bool headsKanjiSuruPredicateAt(const dictionary::DictionaryManager& dict_manager,
                               const std::vector<char32_t>& codepoints,
                               const std::vector<normalize::CharType>& char_types, size_t start_pos,
                               size_t minimum_kanji_count) {
  if (hasKanjiSuruPredicateAt(codepoints, char_types, start_pos, minimum_kanji_count)) {
    return true;
  }
  const size_t predicate_end = findCharRegionEnd(
      char_types, start_pos, char_types.size() - std::min(start_pos, char_types.size()), normalize::CharType::Kanji);
  if (predicate_end - start_pos < minimum_kanji_count || predicate_end + 1 >= codepoints.size() ||
      !grammar::isSuruRenyokeiSurface(extractSubstring(codepoints, predicate_end, predicate_end + 1))) {
    return false;
  }
  if (char_types[predicate_end + 1] != normalize::CharType::Hiragana) {
    return false;
  }
  // A particle opening with し follows a complete nominal, so it is evidence
  // against the predicate reading rather than for it (五人組しか, 一番星しも).
  return !hasExactPartOfSpeech(dict_manager, extractSubstring(codepoints, predicate_end, predicate_end + 2),
                               partOfSpeechMask(core::PartOfSpeech::Particle));
}

size_t longestNominalVerbContinuativeStart(const std::vector<char32_t>& codepoints,
                                           const std::vector<normalize::CharType>& char_types, size_t kanji_start,
                                           size_t kanji_end, const grammar::Inflection& inflection,
                                           const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || kanji_start >= kanji_end || kanji_end >= codepoints.size()) {
    return kanji_end;
  }

  constexpr size_t kMaximumOkuriganaLength = 2;
  size_t longest_start = kanji_end;
  size_t continuative_end = kanji_end;
  while (continuative_end < codepoints.size() && continuative_end - kanji_end < kMaximumOkuriganaLength &&
         char_types[continuative_end] == normalize::CharType::Hiragana) {
    ++continuative_end;
    const char32_t ending = codepoints[continuative_end - 1];
    const std::string_view godan_ending = grammar::godanBaseSuffixFromIRow(ending);
    const size_t okurigana_length = continuative_end - kanji_end;
    const bool is_single_mora_continuative = !godan_ending.empty() || grammar::isERowCodepoint(ending);
    const bool is_supported_two_mora_continuative =
        okurigana_length == 2 && (ending == U'げ' || ending == U'け' || ending == U'り' || ending == U'え' ||
                                  ending == U'し' || ending == U'み');
    if ((okurigana_length == 1 && !is_single_mora_continuative) ||
        (okurigana_length == 2 && !is_supported_two_mora_continuative)) {
      continue;
    }
    // The i-row spelling い is also the terminal of productive compound
    // i-adjectives (間近い). Require a dictionary-backed final godan verb for
    // this otherwise ambiguous nominalization (払い→払う, 洗い→洗う).
    if (okurigana_length == 1 && ending == U'い' &&
        !verb_helpers::isVerbInDictionary(
            dict_manager, normalize::concat(normalize::encodeUtf8(codepoints[kanji_end - 1]), godan_ending))) {
      continue;
    }
    const std::string continuation =
        extractSubstring(codepoints, continuative_end, std::min(codepoints.size(), continuative_end + 3));
    if (okurigana_length == 2) {
      const std::string okurigana = extractSubstring(codepoints, kanji_end, continuative_end);
      const bool is_auxiliary = dict_manager->lookupExact(okurigana, core::PartOfSpeech::Auxiliary) != nullptr;
      const bool is_closed_grammar_surface =
          dict_manager->lookupExact(okurigana, core::PartOfSpeech::Particle) != nullptr ||
          (is_auxiliary && !grammar::startsPredicativeCopula(continuation)) ||
          dict_manager->lookupExact(okurigana, core::PartOfSpeech::Adjective) != nullptr ||
          dict_manager->lookupExact(okurigana, core::PartOfSpeech::Suffix) != nullptr;
      if (is_closed_grammar_surface) {
        continue;
      }
    }

    // A continuative-derived noun can close only where a nominal is selected.
    // This excludes finite predicates and auxiliary chains such as 到着します,
    // 子供らしくない, while retaining 見直し, 見知りです and 暮らしだ.
    const bool starts_light_verb =
        continuative_end + 1 < codepoints.size() &&
        (grammar::isSuruBaseForm(extractSubstring(codepoints, continuative_end, continuative_end + 2)) ||
         (grammar::isSuruRenyokeiSurface(extractSubstring(codepoints, continuative_end, continuative_end + 1)) &&
          verb_helpers::isSuruAuxiliaryStarter(codepoints[continuative_end + 1])));
    const bool nominal_position = continuative_end >= codepoints.size() ||
                                  char_types[continuative_end] == normalize::CharType::Symbol ||
                                  startsNominalForcingParticle(codepoints, continuative_end) ||
                                  grammar::startsPredicativeCopula(continuation) || starts_light_verb;
    if (!nominal_position) {
      continue;
    }

    // A closed suffix owns its own boundary even when the preceding noun plus
    // suffix resembles a constructed Ichidan stem (家庭|向け, not 家|庭向け).
    size_t closed_suffix_start = kanji_end;
    const bool ends_in_dictionary_verb_continuative =
        (!godan_ending.empty() && verb_helpers::isVerbInDictionary(
                                      dict_manager, extractSubstring(codepoints, kanji_end - 1, continuative_end - 1) +
                                                        std::string(godan_ending))) ||
        (grammar::isERowCodepoint(ending) &&
         verb_helpers::isVerbInDictionary(dict_manager, extractSubstring(codepoints, kanji_end - 1, continuative_end) +
                                                            normalize::encodeUtf8(core::hiragana::kRu)));
    for (size_t suffix_start = kanji_start + 1; suffix_start < kanji_end; ++suffix_start) {
      if (lookupEntryInRange(*dict_manager, codepoints, suffix_start, continuative_end, core::PartOfSpeech::Suffix) !=
          nullptr) {
        if (ends_in_dictionary_verb_continuative && suffix_start + 2 >= continuative_end) {
          continue;
        }
        closed_suffix_start = suffix_start;
        break;
      }
    }
    if (closed_suffix_start < kanji_end) {
      longest_start = std::min(longest_start, closed_suffix_start);
      continue;
    }

    for (size_t verb_start = kanji_start; verb_start < kanji_end; ++verb_start) {
      const std::string continuative = extractSubstring(codepoints, verb_start, continuative_end);
      const auto& inflections = inflection.analyze(continuative);
      const bool names_adjective = std::any_of(
          inflections.begin(), inflections.end(), [](const grammar::InflectionCandidate& inflection_candidate) {
            return inflection_candidate.verb_type == grammar::VerbType::IAdjective &&
                   inflection_candidate.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence;
          });
      bool names_verb =
          !names_adjective &&
          std::any_of(inflections.begin(), inflections.end(),
                      [](const grammar::InflectionCandidate& inflection_candidate) {
                        return inflection_candidate.verb_type != grammar::VerbType::Unknown &&
                               inflection_candidate.verb_type != grammar::VerbType::IAdjective &&
                               inflection_candidate.verb_type != grammar::VerbType::Suru &&
                               inflection_candidate.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence;
                      });
      // Adjacent-kanji compounds such as 見直す and 見知る have a low full-form
      // inflection confidence because the productive V1/V2 boundary is not
      // written. Verify the same closed single-kanji Ichidan V1 used by the
      // compound-verb joiner, plus the dictionary-backed visible V2.
      if (!names_verb && kanji_end - verb_start == 2) {
        const bool left_ichidan = verb_helpers::isSingleKanjiIchidan(codepoints[verb_start]);
        bool right_verb = false;
        if (!godan_ending.empty()) {
          right_verb = verb_helpers::isVerbInDictionary(
              dict_manager,
              normalize::concat(extractSubstring(codepoints, verb_start + 1, continuative_end - 1), godan_ending));
        }
        if (!right_verb && grammar::isERowCodepoint(ending)) {
          right_verb = verb_helpers::isVerbInDictionary(dict_manager,
                                                        extractSubstring(codepoints, verb_start + 1, continuative_end) +
                                                            normalize::encodeUtf8(core::hiragana::kRu));
        }
        names_verb = left_ichidan && right_verb;
      }
      if (names_verb) {
        longest_start = std::min(longest_start, verb_start);
        break;
      }
    }
  }
  return longest_start;
}

ByteOffsets buildByteOffsets(const std::vector<char32_t>& codepoints) {
  ByteOffsets byte_offsets;
  byte_offsets.reserve(codepoints.size() + 1);
  byte_offsets.push_back(0);
  for (char32_t codepoint : codepoints) {
    byte_offsets.push_back(byte_offsets.back() + core::utf8ByteLength(codepoint));
  }
  return byte_offsets;
}

std::string_view textRange(std::string_view text, const ByteOffsets& byte_offsets, size_t start, size_t end) {
  if (start >= end || end >= byte_offsets.size()) {
    return {};
  }
  const size_t byte_start = byte_offsets[start];
  const size_t byte_end = byte_offsets[end];
  if (byte_start > byte_end || byte_end > text.size()) {
    return {};
  }
  return text.substr(byte_start, byte_end - byte_start);
}

size_t advanceCharsToBytePos(const std::vector<char32_t>& codepoints, size_t start_char, size_t start_byte,
                             size_t target_byte) {
  size_t char_pos = start_char;
  size_t byte_count = start_byte;
  while (char_pos < codepoints.size() && byte_count < target_byte) {
    byte_count += core::utf8ByteLength(codepoints[char_pos]);
    ++char_pos;
  }
  return char_pos;
}

std::string extractSubstring(const std::vector<char32_t>& codepoints, size_t start, size_t end) {
  return normalize::encodeRange(codepoints, start, end);
}

std::string extractClosedClassProbe(const std::vector<char32_t>& codepoints, size_t start) {
  return extractSubstring(codepoints, start, std::min(codepoints.size(), start + kClosedClassProbeChars));
}

bool startsNominalForcingParticle(const std::vector<char32_t>& codepoints, size_t pos) {
  if (pos >= codepoints.size()) {
    return false;
  }
  switch (codepoints[pos]) {
    case U'を':
    case U'が':
    case U'に':
    case U'で':
    case U'と':
    case U'へ':
    case U'は':
    case U'も':
    case U'の':
      return true;
    case U'か':
      return pos + 1 < codepoints.size() && codepoints[pos + 1] == U'ら';
    case U'ま':
      return pos + 1 < codepoints.size() && codepoints[pos + 1] == U'で';
    case U'よ':
      return pos + 1 < codepoints.size() && codepoints[pos + 1] == U'り';
    default:
      return false;
  }
}

// A non-particle entry whose own surface is also a listed particle claims the
// span no more strongly than the particle does, so it is not evidence that the
// position is non-particle (より is both the case particle and the continuative
// of a kana-spelled verb).
static bool isParticleHomograph(const dictionary::DictionaryManager& dict_manager,
                                const dictionary::DictionaryEntry& entry) {
  return entry.pos != core::PartOfSpeech::Particle &&
         dict_manager.lookupExact(entry.surface, core::PartOfSpeech::Particle) != nullptr;
}

bool isNominalForcingParticle(core::ExtendedPOS extended_pos) {
  switch (extended_pos) {
    case core::ExtendedPOS::ParticleCase:
    case core::ExtendedPOS::ParticleTopic:
    case core::ExtendedPOS::ParticleAdverbial:
    case core::ExtendedPOS::ParticleNo:
    case core::ExtendedPOS::ParticleBinding:
      return true;
    default:
      return false;
  }
}

bool hasNominalForcingParticleContinuation(const std::vector<char32_t>& codepoints, size_t pos,
                                           const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || pos >= codepoints.size()) {
    return false;
  }

  const size_t probe_end = std::min(codepoints.size(), pos + static_cast<size_t>(4));
  bool has_particle = false;
  for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, pos, probe_end)) {
    if (match.entry == nullptr) {
      continue;
    }
    if (match.entry->pos == core::PartOfSpeech::Particle && isNominalForcingParticle(match.entry->extended_pos)) {
      has_particle = true;
    } else if (normalize::utf8Length(match.entry->surface) > 1 && !isParticleHomograph(*dict_manager, *match.entry)) {
      return false;
    }
  }
  return has_particle;
}

bool startsLongerNonParticleEntry(const std::vector<char32_t>& codepoints, size_t start_pos,
                                  const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || start_pos >= codepoints.size()) {
    return false;
  }
  const size_t probe_end = std::min(codepoints.size(), start_pos + static_cast<size_t>(4));
  for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, start_pos, probe_end)) {
    if (match.entry != nullptr && match.entry->pos != core::PartOfSpeech::Particle &&
        normalize::utf8Length(match.entry->surface) > 1 && !isParticleHomograph(*dict_manager, *match.entry)) {
      return true;
    }
  }
  return false;
}

bool hasExactPartOfSpeech(const dictionary::DictionaryManager& dict_manager, std::string_view surface,
                          PartOfSpeechMask pos_mask) {
  for (uint8_t pos_value = 0; pos_mask != 0; ++pos_value, pos_mask >>= 1) {
    if ((pos_mask & 1U) != 0 &&
        dict_manager.lookupExact(surface, static_cast<core::PartOfSpeech>(pos_value)) != nullptr) {
      return true;
    }
  }
  return false;
}

bool lookupResultsHavePartOfSpeech(const std::vector<dictionary::LookupResult>& results, PartOfSpeechMask pos_mask,
                                   size_t length) {
  return std::any_of(results.begin(), results.end(), [=](const auto& result) {
    return result.entry != nullptr && (length == 0 || result.length == length) &&
           (pos_mask & partOfSpeechMask(result.entry->pos)) != 0;
  });
}

bool lookupResultsHaveExtendedPOS(const std::vector<dictionary::LookupResult>& results, core::ExtendedPOS extended_pos,
                                  size_t length) {
  return std::any_of(results.begin(), results.end(), [=](const auto& result) {
    return result.entry != nullptr && (length == 0 || result.length == length) &&
           result.entry->extended_pos == extended_pos;
  });
}

int maximalSegmentCount(const dictionary::DictionaryManager& dict_manager, const std::vector<char32_t>& codepoints,
                        size_t start_pos, size_t end_pos, core::PartOfSpeech pos) {
  if (start_pos >= end_pos) {
    return -1;
  }
  const size_t span = end_pos - start_pos;
  std::vector<int> part_count(span + 1, -1);
  part_count[0] = 0;
  for (size_t relative_start = 0; relative_start < span; ++relative_start) {
    if (part_count[relative_start] < 0) {
      continue;
    }
    for (size_t relative_end = relative_start + 1; relative_end <= span; ++relative_end) {
      if (lookupEntryInRange(dict_manager, codepoints, start_pos + relative_start, start_pos + relative_end, pos) !=
          nullptr) {
        part_count[relative_end] = std::max(part_count[relative_end], part_count[relative_start] + 1);
      }
    }
  }
  return part_count.back();
}

bool hasDictionaryEntryEndingAt(const dictionary::DictionaryManager& dict_manager,
                                const std::vector<char32_t>& codepoints, size_t scan_start, size_t end_pos,
                                PartOfSpeechMask pos_mask) {
  if (end_pos > codepoints.size()) {
    return false;
  }
  for (size_t start = scan_start; start < end_pos; ++start) {
    if (hasExactPartOfSpeech(dict_manager, extractSubstring(codepoints, start, end_pos), pos_mask)) {
      return true;
    }
  }
  return false;
}

bool hasDictionarySplit(const dictionary::DictionaryManager& dict_manager, const std::vector<char32_t>& codepoints,
                        size_t start_pos, size_t end_pos, PartOfSpeechMask left_mask, PartOfSpeechMask right_mask) {
  if (end_pos > codepoints.size()) {
    return false;
  }
  for (size_t split = start_pos + 1; split < end_pos; ++split) {
    if (hasExactPartOfSpeech(dict_manager, extractSubstring(codepoints, start_pos, split), left_mask) &&
        hasExactPartOfSpeech(dict_manager, extractSubstring(codepoints, split, end_pos), right_mask)) {
      return true;
    }
  }
  return false;
}

bool hasCompleteVerbLemma(const dictionary::DictionaryManager& dict_manager, std::string_view surface,
                          size_t char_length, std::string_view lemma) {
  for (const auto& match : dict_manager.lookup(surface, 0)) {
    if (match.entry != nullptr && match.length == char_length && match.entry->pos == core::PartOfSpeech::Verb &&
        match.entry->lemma == lemma) {
      return true;
    }
  }
  return false;
}

bool startsInsideRegisteredNoun(const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                const ByteOffsets& byte_offsets, size_t start_pos) {
  if (start_pos == 0) {
    return false;
  }
  const size_t scan_start = start_pos > kDictionaryLookbehindChars ? start_pos - kDictionaryLookbehindChars : 0;
  for (size_t noun_start = scan_start; noun_start < start_pos; ++noun_start) {
    for (const auto& result : dict_manager.lookup(text, byteOffsetAt(byte_offsets, noun_start))) {
      if (result.entry != nullptr && result.entry->pos == core::PartOfSpeech::Noun &&
          noun_start + result.length > start_pos) {
        return true;
      }
    }
  }
  return false;
}

bool hasPrecedingPartOfSpeech(const core::Lattice& lattice, size_t end_pos, PartOfSpeechMask pos_mask) {
  return core::anyEdgeEndingAt(lattice, end_pos, [pos_mask](const core::LatticeEdge& edge) {
    return (pos_mask & partOfSpeechMask(edge.pos)) != 0;
  });
}

bool hasPrecedingExtendedPOS(const core::Lattice& lattice, size_t end_pos, core::ExtendedPOS extended_pos) {
  return core::anyEdgeEndingAt(
      lattice, end_pos, [extended_pos](const core::LatticeEdge& edge) { return edge.extended_pos == extended_pos; });
}

namespace {

size_t compoundEndCovering(const core::Lattice& lattice, size_t pos, bool require_lexical_evidence) {
  size_t covering_end = 0;
  const size_t scan_start = pos > kDictionaryLookbehindChars ? pos - kDictionaryLookbehindChars : 0;
  for (size_t edge_start = scan_start; edge_start < pos; ++edge_start) {
    for (const uint32_t edge_id : lattice.edgeIdsAt(edge_start)) {
      const auto& edge = lattice.getEdge(edge_id);
      const bool is_open_compound_stem =
          edge.extended_pos == core::ExtendedPOS::VerbRenyokei || edge.extended_pos == core::ExtendedPOS::VerbMizenkei;
      const bool is_compound_stem =
          edge.origin == core::CandidateOrigin::VerbCompound && is_open_compound_stem &&
          (!require_lexical_evidence || core::hasFlag(edge.flags, core::EdgeFlags::LemmaVerified));
      if (is_compound_stem && edge.start < pos && edge.end > pos) {
        covering_end = std::max(covering_end, static_cast<size_t>(edge.end));
      }
    }
  }
  return covering_end;
}

}  // namespace

size_t verifiedCompoundEndCovering(const core::Lattice& lattice, size_t pos) {
  return compoundEndCovering(lattice, pos, true);
}

size_t dictionarySokuonbinEndCovering(const core::Lattice& lattice, size_t pos) {
  size_t covering_end = 0;
  const size_t scan_start = pos > kDictionaryLookbehindChars ? pos - kDictionaryLookbehindChars : 0;
  for (size_t edge_start = scan_start; edge_start < pos; ++edge_start) {
    for (const uint32_t edge_id : lattice.edgeIdsAt(edge_start)) {
      const auto& edge = lattice.getEdge(edge_id);
      if (edge.origin == core::CandidateOrigin::Dictionary && edge.pos == core::PartOfSpeech::Verb &&
          edge.extended_pos == core::ExtendedPOS::VerbOnbinkei && utf8::endsWith(edge.surface, "っ") &&
          edge.start < pos && edge.end > pos) {
        covering_end = std::max(covering_end, static_cast<size_t>(edge.end));
      }
    }
  }
  return covering_end;
}

size_t compoundVerbEndCovering(const core::Lattice& lattice, size_t pos) {
  return compoundEndCovering(lattice, pos, false);
}

bool joinsParticleToDictionaryAdverb(const core::Lattice& lattice, const dictionary::DictionaryManager& dict_manager,
                                     std::string_view text, const ByteOffsets& byte_offsets, size_t candidate_start,
                                     size_t candidate_end, core::ExtendedPOS candidate_extended_pos) {
  if (candidate_start == 0 || candidate_start + 1 >= candidate_end || byte_offsets.empty()) {
    return false;
  }

  constexpr PartOfSpeechMask kLeftContextMask =
      partOfSpeechMask(core::PartOfSpeech::Noun) | partOfSpeechMask(core::PartOfSpeech::Pronoun) |
      partOfSpeechMask(core::PartOfSpeech::Verb) | partOfSpeechMask(core::PartOfSpeech::Adjective) |
      partOfSpeechMask(core::PartOfSpeech::Auxiliary);
  if (!hasPrecedingPartOfSpeech(lattice, candidate_start, kLeftContextMask)) {
    return false;
  }

  // The mizenkei of the contracted preparative auxiliary selects the
  // volitional う after a verb onbin form (買っ+とこ+う).  Its first mora is
  // homographic with the quotation particle, but this closed inflectional
  // chain is not a particle followed by the dictionary adverb こう.
  // The contraction is て + おく, so the cell it attaches to is whichever one
  // that て selects: the onbin form for a Godan verb, the plain continuative
  // for an Ichidan or サ変 one (見+とこ+う, 作成し+とこ+う).
  if (candidate_extended_pos == core::ExtendedPOS::AuxAspectOku &&
      (hasPrecedingExtendedPOS(lattice, candidate_start, core::ExtendedPOS::VerbOnbinkei) ||
       hasPrecedingExtendedPOS(lattice, candidate_start, core::ExtendedPOS::VerbRenyokei)) &&
      lookupResultsHaveExtendedPOS(dict_manager.lookup(text, byteOffsetAt(byte_offsets, candidate_end)),
                                   core::ExtendedPOS::AuxVolitional, 1)) {
    return false;
  }

  for (size_t split_pos = candidate_start + 1; split_pos < candidate_end; ++split_pos) {
    const size_t prefix_byte_end = byteOffsetAt(byte_offsets, split_pos);
    const std::string_view particle = textRange(text, byte_offsets, candidate_start, split_pos);
    const auto* particle_entry = dict_manager.lookupExact(particle, core::PartOfSpeech::Particle);
    if (particle_entry == nullptr || (particle_entry->extended_pos != core::ExtendedPOS::ParticleCase &&
                                      particle_entry->extended_pos != core::ExtendedPOS::ParticleTopic &&
                                      particle_entry->extended_pos != core::ExtendedPOS::ParticleConj)) {
      continue;
    }
    for (const auto& result : dict_manager.lookup(text, prefix_byte_end)) {
      if (result.entry == nullptr || result.entry->pos != core::PartOfSpeech::Adverb || result.length < 2) {
        continue;
      }
      if (candidate_end < split_pos + result.length) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace suzume::analysis
