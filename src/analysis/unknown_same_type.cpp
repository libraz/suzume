/**
 * @file unknown_same_type.cpp
 * @brief Same-type sequence candidate generation for unknown words
 *
 * Houses UnknownWordGenerator::generateBySameType, which opens a run of one
 * character type, finds its extent, and hands it to the candidate phases:
 * per-length candidates (unknown_same_type_prefixes.cpp) and the bracketed
 * hiragana noun rescue (unknown_bracketed_noun.cpp).
 */

#include <algorithm>
#include <cstdint>

#include "analysis/dictionary_probe.h"
#include "analysis/scorer_constants.h"
#include "analysis/unknown.h"
#include "analysis/unknown_same_type_internal.h"
#include "candidate_constants.h"
#include "core/kana_constants.h"
#include "normalize/char_type.h"
#include "tokenizer_utils.h"

namespace suzume::analysis {

namespace same_type_detail {

bool opensOnParticleBeforePronoun(const dictionary::DictionaryManager* dict_manager,
                                  const std::vector<char32_t>& codepoints, size_t start, size_t end) {
  return dict_manager != nullptr && end > start + 2 &&
         lookupEntryInRange(*dict_manager, codepoints, start, start + 1, core::PartOfSpeech::Particle) != nullptr &&
         lookupEntryInRange(*dict_manager, codepoints, start + 1, end, core::PartOfSpeech::Pronoun) != nullptr;
}

BoundAuxiliary boundAuxiliaryAt(const std::vector<char32_t>& codepoints, size_t pos,
                                const dictionary::DictionaryManager* dict_manager, bool clause_final_counts) {
  if (dict_manager == nullptr || pos >= codepoints.size()) {
    return {};
  }
  // Widest window an auxiliary and its continuation can occupy in this scan
  // (だっ + た, でしょ + う). The lookup matches inflected forms, so the copula's
  // onbin cell is found under its own length rather than its headword's.
  constexpr size_t kAuxiliaryWindow = 4;
  auto auxiliaries_at = [&](size_t at) {
    const size_t window_end = std::min(codepoints.size(), at + kAuxiliaryWindow);
    std::vector<std::pair<size_t, bool>> found;
    for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, at, window_end)) {
      if (match.entry != nullptr && match.entry->pos == core::PartOfSpeech::Auxiliary) {
        const bool is_copula = match.entry->extended_pos == core::ExtendedPOS::AuxCopulaDa ||
                               match.entry->extended_pos == core::ExtendedPOS::AuxCopulaDesu;
        found.emplace_back(match.length, is_copula);
      }
    }
    return found;
  };
  // Every cell of the auxiliary is considered, not just the longest: the past
  // copula matches both as one form reaching the clause end (だった) and as the
  // onbin cell that selects た (だっ). Only the latter shows a continuation, and
  // taking the longest match alone would hide it.
  for (const auto& [length, is_copula] : auxiliaries_at(pos)) {
    const size_t after = pos + length;
    if (after >= codepoints.size()) {
      // At the clause end only the copula is evidence, and only where a
      // particle or the clause start brackets the run on the left. The copula is the one
      // auxiliary that selects a nominal, so its presence says the kana in
      // front of it closed a noun (これ|は|りんご|だ). Every other auxiliary
      // selects a predicate cell, and its kana are indistinguishable from a
      // noun's last mora at that position (まばたき, たたずむ).
      if (clause_final_counts && is_copula) {
        return {length, is_copula};
      }
      continue;
    }
    if (isRightBoundaryParticle(codepoints[after]) || !auxiliaries_at(after).empty()) {
      return {length, is_copula};
    }
  }
  return {};
}

}  // namespace same_type_detail

namespace {

using same_type_detail::boundAuxiliaryAt;
using same_type_detail::isInternalParticleChar;
using same_type_detail::isNonWordType;

// A hiragana nominalized continuative ending in -み can precede the
// independent adjective continuative なく (よどみなく, たゆみなく).  Emit the
// productive nominal boundary instead of letting an unknown-verb candidate
// absorb the suffix.  The -み condition excludes ordinary i-adjective
// continuatives such as かたくなく, which remain on the adjective path.
bool hasHiraganaNominalNakuEnding(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  constexpr size_t kNakuLength = 2;
  constexpr size_t kMinimumNominalLength = 3;
  if (end_pos - start_pos < kMinimumNominalLength + kNakuLength || codepoints[end_pos - 2] != U'な' ||
      codepoints[end_pos - 1] != U'く') {
    return false;
  }
  return codepoints[end_pos - kNakuLength - 1] == U'み';
}

}  // namespace

void UnknownWordGenerator::generateBySameType(const std::vector<char32_t>& codepoints, size_t start_pos,
                                              const std::vector<normalize::CharType>& char_types,
                                              std::vector<UnknownCandidate>& candidates) const {
  SameTypeRun run;
  if (!openSameTypeRun(codepoints, start_pos, char_types, run, candidates)) {
    return;
  }
  findSameTypeRunEnd(codepoints, char_types, run);
  appendSameTypePrefixCandidates(codepoints, char_types, run, candidates);
  appendHiraganaNominalNakuCandidate(codepoints, run, candidates);
  appendBracketedHiraganaNounCandidates(codepoints, char_types, run, candidates);
}

bool UnknownWordGenerator::openSameTypeRun(const std::vector<char32_t>& codepoints, size_t start_pos,
                                           const std::vector<normalize::CharType>& char_types, SameTypeRun& run,
                                           std::vector<UnknownCandidate>& candidates) const {
  if (start_pos >= char_types.size()) {
    return false;
  }

  normalize::CharType start_type = char_types[start_pos];
  const bool starts_non_word_run = isNonWordType(start_type);
  // Only a run of the SAME non-word class subsumes this position. Symbol and Emoji
  // are separate classes: they carry different parts of speech, and a mixed run
  // would take the POS of its first character.
  if (starts_non_word_run && start_pos > 0 && char_types[start_pos - 1] == start_type) {
    return false;
  }

  // Track if sequence starts with a particle character
  // These sequences may be valid nouns (はし, はな, etc.) despite starting with particles
  bool started_with_particle = false;

  // For hiragana starting with common particle characters (は, に, へ, の),
  // we still generate candidates but with a penalty, as they could be nouns.
  // Examples: はし (橋/箸), はな (花/鼻), にく (肉), へや (部屋), のり (海苔), etc.
  // Note: を, が are excluded - they almost never start nouns
  // Note: よ, わ are excluded - they are sentence-final particles
  if (start_type == normalize::CharType::Hiragana) {
    char32_t first_char = codepoints[start_pos];
    // Case particles を/が are valid standalone dictionary tokens, but they
    // normally cannot begin a multi-character native unknown word.  A listed
    // nominal suffix or adjective stem immediately selected by an inflected
    // copula is the structural exception: the copula proves that the
    // particle-homographic onset belongs to a nominal (…|がた|だっ|た).
    // Emit only that closed two-mora reading, then return so fallbacks such as
    // をよぎった still cannot swallow the particle and predicate together.
    if (first_char == U'を' || first_char == U'が') {
      constexpr size_t kCopulaSelectedHomographLength = 2;
      const size_t homograph_end = start_pos + kCopulaSelectedHomographLength;
      if (dict_manager_ != nullptr && homograph_end <= codepoints.size() &&
          boundAuxiliaryAt(codepoints, homograph_end, dict_manager_, false).is_copula) {
        const std::string homograph = extractSubstring(codepoints, start_pos, homograph_end);
        const auto* suffix = dict_manager_->lookupExact(homograph, core::PartOfSpeech::Suffix);
        const auto* adjective = dict_manager_->lookupExact(homograph, core::PartOfSpeech::Adjective);
        if (suffix != nullptr || (adjective != nullptr && adjective->extended_pos == core::ExtendedPOS::AdjStem)) {
          auto noun_candidate = makeCandidate(homograph, start_pos, homograph_end, core::PartOfSpeech::Noun,
                                              getCostForType(start_type, kCopulaSelectedHomographLength) +
                                                  candidate::kPostParticleNounPenalty + scorer::scale::kStrongBonus,
                                              /*has_suffix=*/true, CandidateOrigin::SameType);
#ifdef SUZUME_DEBUG_INFO
          noun_candidate.pattern = "copula_selected_hiragana_homograph";
#endif
          candidates.push_back(std::move(noun_candidate));
        }
      }
      return false;
    }
    // A genitive の closes before a dictionary formal noun. Do not manufacture
    // a particle-homographic unknown noun across that boundary (不変+の+もの).
    if (first_char == U'の' && dict_manager_ != nullptr && start_pos + 1 < codepoints.size()) {
      const size_t probe_end = std::min(codepoints.size(), start_pos + static_cast<size_t>(5));
      for (const auto& match : lookupResultsInRange(*dict_manager_, codepoints, start_pos + 1, probe_end)) {
        if (match.entry != nullptr && match.entry->extended_pos == core::ExtendedPOS::NounFormal) {
          return false;
        }
      }
    }
    // Only は, に, へ, の can start hiragana nouns
    if (first_char == U'は' || first_char == U'に' || first_char == U'へ' || first_char == U'の') {
      started_with_particle = true;  // Generate but with penalty
    }

    // Skip small kana (拗音・促音) - Japanese words don't start with these
    // ゃゅょぁぃぅぇぉっ are always part of compound sounds (e.g., きょう not ょう)
    if (kana::isSmallKanaCodepoint(first_char)) {
      return false;  // Phonologically impossible word start
    }

    // Skip if starting with demonstrative pronouns (これ, それ, あれ, どれ, etc.)
    // These should be recognized by dictionary lookup, not generated as unknown words.
    if (start_pos + 1 < codepoints.size()) {
      char32_t second_char = codepoints[start_pos + 1];
      if (normalize::isDemonstrativeStart(first_char, second_char)) {
        return false;
      }
    }
  }

  run.start_pos = start_pos;
  run.start_type = start_type;
  run.starts_non_word_run = starts_non_word_run;
  run.started_with_particle = started_with_particle;
  return true;
}

void UnknownWordGenerator::findSameTypeRunEnd(const std::vector<char32_t>& codepoints,
                                              const std::vector<normalize::CharType>& char_types,
                                              SameTypeRun& run) const {
  const size_t start_pos = run.start_pos;
  const normalize::CharType start_type = run.start_type;
  const bool starts_non_word_run = run.starts_non_word_run;
  const bool started_with_particle = run.started_with_particle;

  const size_t max_len = starts_non_word_run ? char_types.size() - start_pos : getMaxLength(start_type);

  // Position of a single particle character the hiragana scan was allowed to
  // cross (SIZE_MAX = none). Candidates extending past it get a penalty below.
  size_t crossed_particle_pos = SIZE_MAX;

  // A keycap emoji is an ASCII digit, #, or * followed by an optional emoji
  // variation selector and U+20E3.  Its base has a text character type, so
  // preserve this grapheme cluster instead of splitting the enclosing keycap
  // off as a standalone emoji.
  const bool keycap_base = (codepoints[start_pos] >= U'0' && codepoints[start_pos] <= U'9') ||
                           codepoints[start_pos] == U'#' || codepoints[start_pos] == U'*';

  // Find end of same-type sequence
  size_t end_pos = start_pos + 1;
  while (end_pos < char_types.size() && end_pos - start_pos < max_len) {
    normalize::CharType curr_type = char_types[end_pos];
    char32_t curr_char = codepoints[end_pos];

    // Check if current character matches the sequence type. Symbol and Emoji do not
    // join each other: the run takes the POS of its first character, so absorbing an
    // emoji into a symbol run would tag it SYMBOL and the symbol filter would delete
    // text-bearing input.
    bool matches_type = curr_type == start_type;

    // Variation selectors and invisible word-internal format controls modify
    // the surrounding text rather than opening a new token.
    if (!matches_type &&
        (normalize::isVariationSelector(curr_char) || normalize::isTransparentFormatControl(curr_char))) {
      matches_type = true;
    }

    if (!matches_type && keycap_base && curr_char == 0x20E3) {
      matches_type = true;
    }

    // Special handling for prolonged sound mark (ー) in hiragana sequences
    // Colloquial expressions like すごーい, やばーい, かわいー use ー in hiragana
    // Also handle consecutive prolonged marks: すごーーい, やばーーーい
    if (!matches_type && start_type == normalize::CharType::Hiragana && normalize::isProlongedSoundMark(curr_char)) {
      // Check if followed by hiragana, another ー, or end of text (かわいー)
      if (end_pos + 1 >= char_types.size() || char_types[end_pos + 1] == normalize::CharType::Hiragana ||
          normalize::isProlongedSoundMark(codepoints[end_pos + 1])) {
        matches_type = true;  // Treat ー as part of hiragana sequence
      }
    }

    // Special handling for emoji modifiers (ZWJ, variation selectors, skin tones)
    // These should always be grouped with the preceding emoji
    if (!matches_type && start_type == normalize::CharType::Emoji && normalize::isEmojiModifier(curr_char)) {
      matches_type = true;  // Treat modifiers as part of emoji sequence
    }

    // Special handling for regional indicators (country flags)
    // Two regional indicators together form a flag emoji (e.g., 🇯🇵)
    if (!matches_type && start_type == normalize::CharType::Emoji && normalize::isRegionalIndicator(curr_char)) {
      matches_type = true;  // Treat regional indicators as part of emoji sequence
    }

    // Special handling for ideographic iteration mark (々) in kanji sequences
    // e.g., 人々, 日々, 堂々, 時々 should be grouped as single tokens
    // The iteration mark U+3005 is classified as Symbol, but it should be
    // treated as part of the kanji sequence when following kanji
    if (!matches_type && start_type == normalize::CharType::Kanji && normalize::isIterationMark(curr_char)) {
      matches_type = true;  // Treat 々 as part of kanji sequence
    }

    // Special handling for ヶ/ケ in kanji sequences (place names, counters)
    // e.g., 姉ヶ崎, 市ヶ谷, 霞ヶ関 should be grouped as single tokens
    // ヶ (U+30F6) is classified as Katakana, but in these contexts it functions
    // as a kanji-like character connecting surrounding kanji
    if (!matches_type && start_type == normalize::CharType::Kanji && (curr_char == U'ヶ' || curr_char == U'ケ') &&
        end_pos + 1 < char_types.size() && char_types[end_pos + 1] == normalize::CharType::Kanji) {
      matches_type = true;  // Treat ヶ/ケ as part of kanji sequence
    }

    if (!matches_type) {
      break;
    }

    // For hiragana, break at common particle characters to avoid
    // swallowing particles into unknown words (e.g., don't create "ぎをみじん")
    if (start_type == normalize::CharType::Hiragana) {
      // を cannot occur within a native hiragana word.
      if (curr_char == U'を') {
        break;
      }
      // For non-particle starts, particle characters usually mark word
      // boundaries. However, genuine hiragana nouns can contain one such
      // character word-internally (こども, おとな, ひとつ), so allow the scan
      // to cross a single particle character; candidates extending past it
      // receive a penalty in the generation loop below.
      //
      // Crossing is restricted to keep particle chains intact:
      // - の always breaks: genitive の marks a compound boundary in
      //   hiragana noun+noun patterns (みせ+の+まえ, こころ+の+こえ)
      // - A second particle character breaks (likely a real particle chain)
      // - At most one character may follow the crossed particle: native
      //   words with a word-internal particle character are short (こども,
      //   おとな, ひとつ); longer tails just absorb a genuine particle
      if (!started_with_particle) {
        if (crossed_particle_pos != SIZE_MAX && end_pos > crossed_particle_pos + 1) {
          break;  // Already extended one char past the crossed particle
        }
        // Genitive の: always a word boundary
        if (curr_char == U'の') {
          break;
        }
        // The nominative が behaves like the other particle characters: it is
        // word-internal in a few native nouns (ひがし, かがみ) and a boundary
        // everywhere else, so it is crossed at most once and penalized. Leaving
        // it out let an opaque run swallow a subject marker whole (見|るが).
        if (isInternalParticleChar(curr_char)) {
          if (crossed_particle_pos != SIZE_MAX) {
            break;  // Stop before the particle character
          }
          crossed_particle_pos = end_pos;  // Cross one, penalized per length
        }
      }
    }
    ++end_pos;
  }

  run.end_pos = end_pos;
  run.crossed_particle_pos = crossed_particle_pos;
}

void UnknownWordGenerator::appendHiraganaNominalNakuCandidate(const std::vector<char32_t>& codepoints,
                                                              const SameTypeRun& run,
                                                              std::vector<UnknownCandidate>& candidates) const {
  const size_t start_pos = run.start_pos;
  const normalize::CharType start_type = run.start_type;
  const size_t end_pos = run.end_pos;

  // Productive hiragana nominal + adjective-continuative boundary.  This is
  // deliberately independent of particle bracketing: literary adverbials
  // commonly occur at the beginning of a clause (よどみなく話す).
  if (start_type == normalize::CharType::Hiragana && hasHiraganaNominalNakuEnding(codepoints, start_pos, end_pos)) {
    const size_t nominal_end = end_pos - 2;
    const size_t nominal_len = nominal_end - start_pos;
    auto noun_cand =
        makeCandidate(codepoints, start_pos, nominal_end, core::PartOfSpeech::Noun,
                      getCostForType(start_type, nominal_len) + candidate::kHiraganaNominalNakuCandidateBonus,
                      /*has_suffix=*/true, CandidateOrigin::SameType);
#ifdef SUZUME_DEBUG_INFO
    noun_cand.pattern = "hiragana_nominal_naku";
#endif
    candidates.push_back(noun_cand);
  }
}

}  // namespace suzume::analysis
