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

#include "core/debug.h"
#include "core/utf8_constants.h"
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

    // Add hiragana compound verb join candidates (やり + なおす → やりなおす)
    addHiraganaCompoundVerbJoinCandidates(lattice, text, codepoints, pos, char_types);

    // Add adjective + すぎる compound verb candidates (尊 + すぎる → 尊すぎる)
    addAdjectiveSugiruJoinCandidates(lattice, text, codepoints, pos, char_types);

    // Add katakana + すぎる compound verb candidates (ワンパターン + すぎる → ワンパターンすぎる)
    addKatakanaSugiruJoinCandidates(lattice, text, codepoints, pos, char_types);

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
                    result.entry->conj_type, result.entry->reading);
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

    // Skip penalty for adverbs (onomatopoeia like わくわく)
    if (candidate.pos == core::PartOfSpeech::Adverb) {
      skip_penalty = true;
    }

    if (!skip_penalty &&
        (candidate.pos == core::PartOfSpeech::Verb ||
         candidate.pos == core::PartOfSpeech::Adjective)) {
      for (const auto& result : dict_results) {
        if (result.entry != nullptr) {
          // Case 1: Dictionary entry is also a verb/adjective
          if (result.entry->pos == core::PartOfSpeech::Verb ||
              result.entry->pos == core::PartOfSpeech::Adjective) {
            skip_penalty = true;
            break;
          }
          // Case 2: Pure hiragana verb candidate vs short dictionary entry
          // Also allow prolonged sound mark (ー) as part of hiragana sequence
          // for colloquial patterns like すごーい, やばーい, かわいー
          if (result.length <= 2 &&
              candidate.end - candidate.start >= 3) {
            bool all_hiragana_or_choon = true;
            for (size_t idx = candidate.start; idx < candidate.end && idx < char_types.size(); ++idx) {
              bool is_valid = (char_types[idx] == normalize::CharType::Hiragana);
              // Allow prolonged sound mark (ー, U+30FC) as part of hiragana
              if (!is_valid && idx < codepoints.size() &&
                  normalize::isProlongedSoundMark(codepoints[idx])) {
                is_valid = true;
              }
              if (!is_valid) {
                all_hiragana_or_choon = false;
                break;
              }
            }
            if (all_hiragana_or_choon) {
              skip_penalty = true;
              break;
            }
          }
        }
      }
    }

    // Case 3: Colloquial verb contractions (ておく→っとく, てしまう→っちゃう/っじゃう)
    // These are valid verb endings that shouldn't be penalized for length
    if (!skip_penalty && candidate.pos == core::PartOfSpeech::Verb) {
      // Check if surface ends with colloquial contraction patterns
      std::string_view surface = candidate.surface;
      if (utf8::endsWith(surface, "っとく") ||
          utf8::endsWith(surface, "っちゃう") ||
          utf8::endsWith(surface, "っじゃう")) {
        skip_penalty = true;
      }
    }

    // Case 5: Short hiragana verb candidates ending with te/de-form
    // Handles cases like ねて (寝る), でて (出る), みて (見る) where
    // dictionary only has kanji form but surface is pure hiragana.
    // These 2-char patterns don't meet Case 2's ≥3 char threshold.
    if (!skip_penalty && candidate.pos == core::PartOfSpeech::Verb) {
      std::string_view surface = candidate.surface;
      size_t len = candidate.end - candidate.start;
      // Check for 2-char hiragana verbs ending in て/で
      if (len == 2 && surface.size() >= core::kJapaneseCharBytes) {
        bool all_hiragana = true;
        for (size_t idx = candidate.start; idx < candidate.end && idx < char_types.size(); ++idx) {
          if (char_types[idx] != normalize::CharType::Hiragana) {
            all_hiragana = false;
            break;
          }
        }
        if (all_hiragana) {
          // Check if ends with て or で (te-form markers)
          std::string_view last_char = utf8::lastChar(surface);
          if (last_char == "て" || last_char == "で") {
            skip_penalty = true;
          }
        }
      }
    }

    // Case 4: Pure hiragana OTHER (likely readings/furigana)
    // Reduce penalty for long varied hiragana sequences
    // Also allow prolonged sound mark (ー) as part of hiragana sequence
    bool reduced_penalty = false;
    bool skip_dict_penalty = false;
    if (!skip_penalty && candidate.pos == core::PartOfSpeech::Other &&
        candidate.end - candidate.start >= 4) {
      bool all_hiragana_or_choon = true;
      bool all_same = true;
      char32_t first_cp = 0;
      for (size_t idx = candidate.start;
           idx < candidate.end && idx < char_types.size(); ++idx) {
        bool is_valid = (char_types[idx] == normalize::CharType::Hiragana);
        // Allow prolonged sound mark (ー, U+30FC) as part of hiragana
        if (!is_valid && idx < codepoints.size() &&
            normalize::isProlongedSoundMark(codepoints[idx])) {
          is_valid = true;
        }
        if (!is_valid) {
          all_hiragana_or_choon = false;
          break;
        }
        if (idx < codepoints.size()) {
          if (idx == candidate.start) {
            first_cp = codepoints[idx];
          } else if (codepoints[idx] != first_cp) {
            all_same = false;
          }
        }
      }
      if (all_hiragana_or_choon && !all_same) {
        reduced_penalty = true;
      }
    }

    // Skip dict length penalty for katakana sequences (loanwords)
    // Loanwords like マスカラ, デスクトップ often exceed dictionary coverage
    if (!skip_penalty && candidate.pos == core::PartOfSpeech::Noun &&
        candidate.end - candidate.start >= 3) {
      bool all_katakana = true;
      for (size_t idx = candidate.start;
           idx < candidate.end && idx < char_types.size(); ++idx) {
        if (char_types[idx] != normalize::CharType::Katakana &&
            !(idx < codepoints.size() &&
              normalize::isProlongedSoundMark(codepoints[idx]))) {
          all_katakana = false;
          break;
        }
      }
      if (all_katakana) {
        skip_dict_penalty = true;
      }
    }

    // Skip dict length penalty for short kanji sequences (2-3 chars)
    // Common words like 人工, 知能, 処理 may not be in dictionary
    if (!skip_penalty && !skip_dict_penalty &&
        candidate.pos == core::PartOfSpeech::Noun) {
      size_t len = candidate.end - candidate.start;
      if (len >= 2 && len <= 3) {
        bool all_kanji = true;
        for (size_t idx = candidate.start;
             idx < candidate.end && idx < char_types.size(); ++idx) {
          if (char_types[idx] != normalize::CharType::Kanji) {
            all_kanji = false;
            break;
          }
        }
        if (all_kanji) {
          skip_dict_penalty = true;
        }
      }
    }

    // Skip exceeds_dict_length penalty for suffix pattern candidates
    // These are morphologically recognized patterns (e.g., がち, っぽい)
    // that should not be penalized for exceeding dictionary coverage
    // Also skip for katakana loanwords (マスカラ, デスクトップ)
    if (!skip_penalty && !skip_dict_penalty && !candidate.has_suffix &&
        max_dict_length > 0 &&
        candidate.end - candidate.start > max_dict_length) {
      float penalty = reduced_penalty ? 1.0F : 3.5F;
      adjusted_cost += penalty;
      SUZUME_DEBUG_LOG("[TOK_UNK] \"" << candidate.surface << "\": +"
                        << penalty << " (exceeds_dict_length"
                        << (reduced_penalty ? ", pure_hiragana" : "")
                        << ", dict_max=" << max_dict_length << ")\n");
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

        // Don't penalize verb conjugation endings
        // - te-form: て/で/って/んで/いて/いで
        // - renyoukei し: extremely common for suru/godan verbs (分割し, 話し)
        bool is_verb_ending =
            (hiragana_suffix == "て" || hiragana_suffix == "で" ||
             hiragana_suffix == "って" || hiragana_suffix == "んで" ||
             hiragana_suffix == "いて" || hiragana_suffix == "いで" ||
             hiragana_suffix == "し");

        if (!is_verb_ending) {
          size_t suffix_byte_pos = charPosToBytePos(codepoints, hiragana_start);
          auto suffix_results = dict_manager_.lookup(text, suffix_byte_pos);

          for (const auto& result : suffix_results) {
            if (result.entry != nullptr &&
                result.entry->pos == core::PartOfSpeech::Particle) {
              size_t suffix_len = candidate.end - hiragana_start;
              if (result.length == suffix_len) {
                adjusted_cost += 1.5F;
                SUZUME_DEBUG_LOG("[TOK_UNK] \"" << candidate.surface
                                  << "\": +1.5 (particle_suffix=\""
                                  << hiragana_suffix << "\")\n");
                break;
              }
            }
          }
        }
      }
    }

    std::string surface_str(candidate.surface);

#ifdef SUZUME_DEBUG_INFO
    lattice.addEdge(surface_str, static_cast<uint32_t>(candidate.start),
                    static_cast<uint32_t>(candidate.end), candidate.pos,
                    adjusted_cost, flags, candidate.lemma, candidate.conj_type,
                    {},  // reading
                    candidate.origin, candidate.confidence, candidate.pattern);
#else
    lattice.addEdge(surface_str, static_cast<uint32_t>(candidate.start),
                    static_cast<uint32_t>(candidate.end), candidate.pos,
                    adjusted_cost, flags, candidate.lemma, candidate.conj_type);
#endif
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
                                        char_types, dict_manager_, scorer_);
}

void Tokenizer::addNounVerbSplitCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  analysis::addNounVerbSplitCandidates(lattice, text, codepoints, start_pos,
                                        char_types, dict_manager_, scorer_);
}

void Tokenizer::addCompoundVerbJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  analysis::addCompoundVerbJoinCandidates(lattice, text, codepoints, start_pos,
                                           char_types, dict_manager_, scorer_);
}

void Tokenizer::addHiraganaCompoundVerbJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  analysis::addHiraganaCompoundVerbJoinCandidates(
      lattice, text, codepoints, start_pos, char_types, dict_manager_, scorer_);
}

void Tokenizer::addPrefixNounJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  analysis::addPrefixNounJoinCandidates(lattice, text, codepoints, start_pos,
                                         char_types, dict_manager_, scorer_);
}

void Tokenizer::addAdjectiveSugiruJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  analysis::addAdjectiveSugiruJoinCandidates(lattice, text, codepoints, start_pos,
                                              char_types, dict_manager_, scorer_);
}

void Tokenizer::addKatakanaSugiruJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  analysis::addKatakanaSugiruJoinCandidates(lattice, text, codepoints, start_pos,
                                             char_types, scorer_);
}

void Tokenizer::addTeFormAuxiliaryCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  analysis::addTeFormAuxiliaryCandidates(lattice, text, codepoints, start_pos,
                                          char_types, scorer_);
}

}  // namespace suzume::analysis
