/**
 * @file tokenizer.cpp
 * @brief Tokenizer that builds lattice from text
 *
 * This file orchestrates candidate generation for tokenization:
 * - Dictionary candidates (direct lookup)
 * - Unknown word candidates (delegated to UnknownWordGenerator)
 * - Split candidates (delegated to split_candidates.h)
 * - Join candidates (delegated to join_candidates.h)
 */

#include "analysis/tokenizer.h"

#include <algorithm>
#include <utility>

#include "analysis/category_cost.h"
#include "candidate_constants.h"
#include "core/debug.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "join_candidates.h"
#include "normalize/utf8.h"
#include "split_candidates.h"
#include "suffix_candidates.h"
#include "tokenizer_utils.h"
#include "verb_candidates_helpers.h"

namespace suzume::analysis {

namespace {

/// Candidate families derived from surface shape alone, with no lexical
/// attestation behind them: repeated-mora and character-speech runs, and the
/// same-script / alphanumeric fallbacks. Every other origin is grounded in a
/// dictionary entry, a conjugation paradigm or a counter lexicon.
bool isShapeDerivedOrigin(core::CandidateOrigin origin) {
  return origin == core::CandidateOrigin::Onomatopoeia || origin == core::CandidateOrigin::CharacterSpeech ||
         origin == core::CandidateOrigin::SameType || origin == core::CandidateOrigin::Alphanumeric;
}

bool isClosedClassEdge(const core::LatticeEdge& edge) {
  return core::isParticleType(edge.extended_pos) || edge.pos == core::PartOfSpeech::Auxiliary ||
         edge.pos == core::PartOfSpeech::Conjunction || edge.pos == core::PartOfSpeech::Determiner ||
         edge.pos == core::PartOfSpeech::Pronoun || edge.pos == core::PartOfSpeech::Prefix ||
         edge.pos == core::PartOfSpeech::Suffix;
}

void diagnoseGeneratedSpanBoundaries(const core::Lattice& lattice) {
  SUZUME_DEBUG_BLOCK {
    for (size_t pos = 0; pos < lattice.textLength(); ++pos) {
      for (uint32_t candidate_id : lattice.edgeIdsAt(pos)) {
        const core::LatticeEdge& candidate = lattice.getEdge(candidate_id);
        if (candidate.fromDictionary()) {
          continue;
        }
        for (size_t inner_pos = candidate.start; inner_pos < candidate.end; ++inner_pos) {
          for (uint32_t inner_id : lattice.edgeIdsAt(inner_pos)) {
            const core::LatticeEdge& inner = lattice.getEdge(inner_id);
            if (inner.id == candidate.id || !inner.fromDictionary() || !isClosedClassEdge(inner) ||
                inner.start <= candidate.start || inner.end >= candidate.end) {
              continue;
            }
            SUZUME_DEBUG_STREAM << "[SPAN_VIOLATION] generated=\"" << candidate.surface << "\" span=" << candidate.start
                                << "-" << candidate.end << " contains=" << inner.surface << "\" "
                                << core::posToString(inner.pos) << " span=" << inner.start << "-" << inner.end << "\n";
          }
        }
        if (candidate.conj_type != dictionary::ConjugationType::None && !candidate.lemma.empty() &&
            candidate.lemma == candidate.surface) {
          SUZUME_DEBUG_STREAM << "[SPAN_VIOLATION] inflected=\"" << candidate.surface << "\" has identical lemma\n";
        }
      }
    }
  }
}

}  // namespace

Tokenizer::Tokenizer(const dictionary::DictionaryManager& dict_manager, const Scorer& scorer,
                     const UnknownWordGenerator& unknown_gen, core::AnalysisMode mode)
    : dict_manager_(dict_manager),
      scorer_(scorer),
      unknown_gen_(unknown_gen),
      inflection_(unknown_gen.inflection()),
      mode_(mode) {}

core::Lattice Tokenizer::buildLattice(std::string_view text, const std::vector<char32_t>& codepoints,
                                      const std::vector<normalize::CharType>& char_types) const {
  core::Lattice lattice(codepoints.size());
  const ByteOffsets byte_offsets = buildByteOffsets(codepoints);
  std::vector<dictionary::LookupResult> dictionary_lookup_results;

  // Process each position
  for (size_t pos = 0; pos < codepoints.size(); ++pos) {
    // These run at every position
    addDictionaryCandidates(lattice, text, codepoints, byte_offsets, pos, dictionary_lookup_results);
    addUnknownCandidates(lattice, text, codepoints, byte_offsets, pos, char_types, dictionary_lookup_results);
    if (mode_ != core::AnalysisMode::Split) {
      addPronounPluralJoinCandidates(lattice, text, codepoints, byte_offsets, pos, dictionary_lookup_results, scorer_);
      addDestinationSuffixNounJoinCandidates(lattice, text, codepoints, byte_offsets, pos, dict_manager_, scorer_);
      addDeverbalNounBeforeIndependentNakuCandidates(lattice, text, codepoints, byte_offsets, pos,
                                                     dictionary_lookup_results, dict_manager_, scorer_);
      addMixedScriptCandidates(lattice, text, codepoints, byte_offsets, pos, char_types, scorer_, dict_manager_);
    }

    // CharType-based dispatch: skip generators that can't match at this position
    auto ct = char_types[pos];
    if (ct == normalize::CharType::Kanji) {
      addCompoundSplitCandidates(lattice, text, byte_offsets, pos, char_types, dict_manager_, scorer_);
      addNounVerbSplitCandidates(lattice, text, codepoints, byte_offsets, pos, dictionary_lookup_results, char_types,
                                 dict_manager_, scorer_, inflection_);
      if (mode_ != core::AnalysisMode::Split) {
        addCompoundVerbJoinCandidates(lattice, text, codepoints, byte_offsets, pos, char_types, dict_manager_, scorer_,
                                      inflection_);
        addPrefixNounJoinCandidates(lattice, text, codepoints, byte_offsets, pos, char_types, dict_manager_, scorer_,
                                    inflection_);
        addTaruAdjectiveJoinCandidates(lattice, text, codepoints, byte_offsets, pos, char_types, dict_manager_,
                                       scorer_);
        addVerbSuffixNounJoinCandidates(lattice, text, codepoints, byte_offsets, pos, char_types, dict_manager_,
                                        scorer_, inflection_);
      }
    } else if (ct == normalize::CharType::Hiragana) {
      if (mode_ != core::AnalysisMode::Split) {
        addPrefixNounJoinCandidates(lattice, text, codepoints, byte_offsets, pos, char_types, dict_manager_, scorer_,
                                    inflection_);
        addHiraganaCompoundVerbJoinCandidates(lattice, text, codepoints, byte_offsets, pos, char_types, dict_manager_,
                                              scorer_, inflection_);
        addVerbSuffixNounJoinCandidates(lattice, text, codepoints, byte_offsets, pos, char_types, dict_manager_,
                                        scorer_, inflection_);
      }
    }

    SUZUME_DEBUG_LOG("[LATTICE] pos=" << pos << " candidates=" << lattice.edgeIdsAt(pos).size() << "\n");
  }

  // Fallback: ensure every position has at least one edge
  // This prevents the lattice from becoming invalid when no candidates are generated
  // (e.g., positions starting with small kana like っ, ゃ, ゅ, ょ)
  for (size_t pos = 0; pos < codepoints.size(); ++pos) {
    if (lattice.edgeIdsAt(pos).empty()) {
      // Generate a single-character fallback candidate with high penalty
      size_t byte_start = byteOffsetAt(byte_offsets, pos);
      size_t byte_end = byteOffsetAt(byte_offsets, pos + 1);
      const std::string_view surface = text.substr(byte_start, byte_end - byte_start);

      lattice.addEdge(surface, static_cast<uint32_t>(pos), static_cast<uint32_t>(pos + 1), core::PartOfSpeech::Other,
                      candidate::kFallbackCandidateCost, core::LatticeEdge::kIsUnknown);
    }
  }

  clampHeuristicBonusesInUserDictSpans(lattice);
  diagnoseGeneratedSpanBoundaries(lattice);

  return lattice;
}

void Tokenizer::clampHeuristicBonusesInUserDictSpans(core::Lattice& lattice) {
  // Keep, for each start position, the furthest user-dictionary span ending
  // there. A prefix maximum below answers containment in O(1) per lattice
  // edge rather than scanning every registered span for every candidate.
  std::vector<uint32_t> furthest_end_at_start;
  bool has_user_span = false;
  for (size_t pos = 0; pos <= lattice.textLength(); ++pos) {
    for (uint32_t edge_id : lattice.edgeIdsAt(pos)) {
      const core::LatticeEdge& edge = lattice.getEdge(edge_id);
      if (edge.fromUserDict()) {
        if (!has_user_span) {
          furthest_end_at_start.assign(lattice.textLength() + 1, 0);
        }
        furthest_end_at_start[edge.start] = std::max(furthest_end_at_start[edge.start], edge.end);
        has_user_span = true;
      }
    }
  }
  if (!has_user_span) {
    return;
  }

  uint32_t furthest_covering_end = 0;
  for (uint32_t& end : furthest_end_at_start) {
    furthest_covering_end = std::max(furthest_covering_end, end);
    end = furthest_covering_end;
  }

  for (size_t pos = 0; pos <= lattice.textLength(); ++pos) {
    for (uint32_t edge_id : lattice.edgeIdsAt(pos)) {
      const core::LatticeEdge& edge = lattice.getEdge(edge_id);
      const bool is_inside_registration = furthest_end_at_start[edge.start] >= edge.end;
      if (!isShapeDerivedOrigin(edge.origin) || edge.fromDictionary() || !is_inside_registration) {
        continue;
      }
      // Back to the plain category cost: the shape bonus is dropped, no penalty
      // is added, and the candidate stays selectable on grammatical grounds.
      const float category_cost = getCategoryCost(edge.extended_pos);
      if (edge.cost < category_cost) {
        SUZUME_DEBUG_LOG("[USERDICT] clamp \"" << edge.surface << "\" " << edge.cost << " -> " << category_cost
                                               << "\n");
        lattice.setEdgeCost(edge_id, category_cost);
      }
    }
  }
}

}  // namespace suzume::analysis
