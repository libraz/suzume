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

#include "join_candidates.h"
#include "split_candidates.h"
#include "tokenizer_utils.h"

namespace suzume::analysis {

Tokenizer::Tokenizer(const dictionary::DictionaryManager& dict_manager,
                     const Scorer& scorer,
                     const UnknownWordGenerator& unknown_gen)
    : dict_manager_(dict_manager),
      scorer_(scorer),
      unknown_gen_(unknown_gen) {}

core::Lattice Tokenizer::buildLattice(
    std::string_view text, const std::vector<char32_t>& codepoints,
    const std::vector<normalize::CharType>& char_types) const {
  core::Lattice lattice(codepoints.size());

  // Process each position
  for (size_t pos = 0; pos < codepoints.size(); ++pos) {
    // Add dictionary candidates
    addDictionaryCandidates(lattice, text, codepoints, pos);

    // Add unknown word candidates
    addUnknownCandidates(lattice, text, codepoints, pos, char_types);

    // Add mixed script joining candidates (Web開発, APIリクエスト, etc.)
    addMixedScriptCandidates(lattice, text, codepoints, pos, char_types);

    // Add compound noun split candidates (人工知能 → 人工 + 知能)
    addCompoundSplitCandidates(lattice, text, codepoints, pos, char_types);

    // Add noun+verb split candidates (本買った → 本 + 買った)
    addNounVerbSplitCandidates(lattice, text, codepoints, pos, char_types);

    // Add compound verb join candidates (飛び + 込む → 飛び込む)
    addCompoundVerbJoinCandidates(lattice, text, codepoints, pos, char_types);

    // Add prefix + noun join candidates (お + 水 → お水)
    addPrefixNounJoinCandidates(lattice, text, codepoints, pos, char_types);

    // Add te-form + auxiliary verb candidates (学んで + いく → 学んで + いきたい)
    addTeFormAuxiliaryCandidates(lattice, text, codepoints, pos, char_types);
  }

  return lattice;
}

size_t Tokenizer::charPosToBytePos(const std::vector<char32_t>& codepoints, size_t char_pos) {
  return analysis::charPosToBytePos(codepoints, char_pos);
}

void Tokenizer::addDictionaryCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos) const {
  // Convert to byte position for dictionary lookup
  size_t byte_pos = charPosToBytePos(codepoints, start_pos);

  // Lookup in dictionary
  auto results = dict_manager_.lookup(text, byte_pos);

  for (const auto& result : results) {
    if (result.entry == nullptr) {
      continue;
    }

    // Calculate end position in characters
    size_t end_pos = start_pos + result.length;

    // Create edge
    uint8_t flags = core::LatticeEdge::kFromDictionary;
    if (result.entry->is_formal_noun) {
      flags |= core::LatticeEdge::kIsFormalNoun;
    }
    if (result.entry->is_low_info) {
      flags |= core::LatticeEdge::kIsLowInfo;
    }

    lattice.addEdge(result.entry->surface,
                    static_cast<uint32_t>(start_pos),
                    static_cast<uint32_t>(end_pos), result.entry->pos,
                    result.entry->cost, flags, result.entry->lemma,
                    result.entry->conj_type);
  }
}

void Tokenizer::addUnknownCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  // Check for dictionary entries at this position to penalize longer unknown words
  size_t byte_pos = charPosToBytePos(codepoints, start_pos);
  auto dict_results = dict_manager_.lookup(text, byte_pos);

  size_t max_dict_length = 0;
  for (const auto& result : dict_results) {
    if (result.entry != nullptr) {
      max_dict_length = std::max(max_dict_length, result.length);
    }
  }

  // Generate unknown word candidates
  auto candidates =
      unknown_gen_.generate(text, codepoints, start_pos, char_types);

  for (const auto& candidate : candidates) {
    uint8_t flags = core::LatticeEdge::kIsUnknown;
    float adjusted_cost = candidate.cost;

    // Penalize unknown words that extend beyond dictionary entries
    bool skip_penalty = false;
    if (candidate.pos == core::PartOfSpeech::Verb ||
        candidate.pos == core::PartOfSpeech::Adjective) {
      for (const auto& result : dict_results) {
        if (result.entry != nullptr) {
          // Case 1: Dictionary entry is also a verb/adjective
          if (result.entry->pos == core::PartOfSpeech::Verb ||
              result.entry->pos == core::PartOfSpeech::Adjective) {
            skip_penalty = true;
            break;
          }
          // Case 2: Pure hiragana verb candidate vs short dictionary entry
          if (result.length <= 2 &&
              candidate.end - candidate.start >= 3) {
            bool all_hiragana = true;
            for (size_t idx = candidate.start; idx < candidate.end && idx < char_types.size(); ++idx) {
              if (char_types[idx] != normalize::CharType::Hiragana) {
                all_hiragana = false;
                break;
              }
            }
            if (all_hiragana) {
              skip_penalty = true;
              break;
            }
          }
        }
      }
    }
    if (!skip_penalty && max_dict_length > 0 &&
        candidate.end - candidate.start > max_dict_length) {
      adjusted_cost += 3.5F;
    }

    // For verb candidates, check if the hiragana suffix is a known particle
    if (candidate.pos == core::PartOfSpeech::Verb &&
        candidate.end > candidate.start) {
      size_t hiragana_start = candidate.start;
      while (hiragana_start < candidate.end &&
             hiragana_start < char_types.size() &&
             char_types[hiragana_start] != normalize::CharType::Hiragana) {
        ++hiragana_start;
      }

      if (hiragana_start < candidate.end) {
        size_t suffix_byte_start = charPosToBytePos(codepoints, hiragana_start);
        size_t suffix_byte_end = charPosToBytePos(codepoints, candidate.end);
        std::string_view hiragana_suffix =
            text.substr(suffix_byte_start, suffix_byte_end - suffix_byte_start);

        // Don't penalize te-form endings
        bool is_te_form = (hiragana_suffix == "て" || hiragana_suffix == "で");

        if (!is_te_form) {
          size_t suffix_byte_pos = charPosToBytePos(codepoints, hiragana_start);
          auto suffix_results = dict_manager_.lookup(text, suffix_byte_pos);

          for (const auto& result : suffix_results) {
            if (result.entry != nullptr &&
                result.entry->pos == core::PartOfSpeech::Particle) {
              size_t suffix_len = candidate.end - hiragana_start;
              if (result.length == suffix_len) {
                adjusted_cost += 1.5F;
                break;
              }
            }
          }
        }
      }
    }

    std::string surface_str(candidate.surface);

    lattice.addEdge(surface_str, static_cast<uint32_t>(candidate.start),
                    static_cast<uint32_t>(candidate.end), candidate.pos,
                    adjusted_cost, flags, "");
  }
}

void Tokenizer::addMixedScriptCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  analysis::addMixedScriptCandidates(lattice, text, codepoints, start_pos,
                                      char_types, scorer_);
}

void Tokenizer::addCompoundSplitCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  analysis::addCompoundSplitCandidates(lattice, text, codepoints, start_pos,
                                        char_types, dict_manager_);
}

void Tokenizer::addNounVerbSplitCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  analysis::addNounVerbSplitCandidates(lattice, text, codepoints, start_pos,
                                        char_types, dict_manager_);
}

void Tokenizer::addCompoundVerbJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  analysis::addCompoundVerbJoinCandidates(lattice, text, codepoints, start_pos,
                                           char_types, dict_manager_, scorer_);
}

void Tokenizer::addPrefixNounJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  analysis::addPrefixNounJoinCandidates(lattice, text, codepoints, start_pos,
                                         char_types, dict_manager_, scorer_);
}

void Tokenizer::addTeFormAuxiliaryCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  analysis::addTeFormAuxiliaryCandidates(lattice, text, codepoints, start_pos,
                                          char_types, scorer_);
}

}  // namespace suzume::analysis
