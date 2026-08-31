/**
 * @file unknown_same_type.cpp
 * @brief Same-type sequence candidate generation for unknown words
 *
 * Split from unknown.cpp: houses UnknownWordGenerator::generateBySameType and the
 * particle-boundary helpers used exclusively by it.
 */

#include <algorithm>
#include <cstdint>

#include "adjective_candidates.h"
#include "analysis/dictionary_probe.h"
#include "analysis/scorer_constants.h"
#include "analysis/unknown.h"
#include "candidate_constants.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "tokenizer_utils.h"
#include "verb_candidates.h"
#include "verb_candidates_helpers.h"

namespace suzume::analysis {

namespace {

bool isNonWordType(normalize::CharType type) {
  return type == normalize::CharType::Symbol || type == normalize::CharType::Emoji;
}

// Particle that can immediately PRECEDE a content noun (私は…, 本を…, 犬が…). Used as
// the left bracket of a post-particle noun promotion.
bool isLeftBoundaryParticle(char32_t code_point) {
  switch (code_point) {
    case U'は':
    case U'が':
    case U'を':
    case U'に':
    case U'で':
    case U'へ':
    case U'と':
    case U'も':
    case U'の':
      return true;
    default:
      return false;
  }
}

// Particle that can immediately FOLLOW a content noun (…を, …が, …は). Used as the
// right bracket. の and end-of-input are excluded: の frequently follows a verb
// nominalization (食べるの) and would over-promote.
bool isRightBoundaryParticle(char32_t code_point) {
  switch (code_point) {
    case U'を':
    case U'が':
    case U'は':
    case U'も':
    case U'に':
    case U'で':
    case U'へ':
    case U'と':
    case U'ば':
      return true;
    default:
      return false;
  }
}

// Hiragana that reads as a particle when it appears WORD-INTERNALLY during a
// bracketed-noun scan. A native noun may span at most one of these (こども, ともだち);
// a second one marks a genuine particle chain and stops the scan. を remains a
// hard stop; が/の may occur inside native words (つながり, かけがえ) and are
// admitted only under the same one-internal-particle cap.
bool isInternalParticleChar(char32_t code_point) {
  switch (code_point) {
    case U'は':
    case U'に':
    case U'へ':
    case U'で':
    case U'と':
    case U'も':
    case U'か':
    case U'が':
    case U'の':
      return true;
    default:
      return false;
  }
}

bool startsClosedNativeNumber(const std::vector<char32_t>& codepoints, size_t pos) {
  if (pos + 2 >= codepoints.size()) {
    return false;
  }
  const char32_t first = codepoints[pos];
  const char32_t second = codepoints[pos + 1];
  const char32_t third = codepoints[pos + 2];
  if ((first == U'ひ' && second == U'と' && third == U'つ') || (first == U'ふ' && second == U'た' && third == U'つ') ||
      (first == U'み' && second == U'っ' && third == U'つ') || (first == U'よ' && second == U'っ' && third == U'つ') ||
      (first == U'い' && second == U'つ' && third == U'つ') || (first == U'む' && second == U'っ' && third == U'つ') ||
      (first == U'な' && second == U'な' && third == U'つ') || (first == U'や' && second == U'っ' && third == U'つ')) {
    return true;
  }
  return pos + 3 < codepoints.size() &&
         ((first == U'こ' && second == U'こ' && third == U'の' && codepoints[pos + 3] == U'つ') ||
          (first == U'と' && second == U'お' && third == U'の' && codepoints[pos + 3] == U'つ'));
}

// Length of a dictionary auxiliary starting at @p pos that is itself bound on
// its right, and 0 when there is none. An auxiliary is bound leftward, so it
// always brackets the kana in front of it; what it does not always show is that
// those kana ended a word, because a clause-final auxiliary is spelled exactly
// like the last mora of a noun (からだ, ありがち + だ). A boundary particle or a
// further auxiliary behind it removes that ambiguity: the auxiliary carries its
// own continuation, so it heads a predicate rather than closing a noun
// (りんご + だっ + た, りんご + だ + と).
struct BoundAuxiliary {
  size_t length{0};
  bool is_copula{false};
};

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
    for (const auto& match : dict_manager->lookup(extractSubstring(codepoints, at, window_end), 0)) {
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
      // particle already brackets the run on the left. The copula is the one
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

// True when [start_pos, end_pos) is itself a listed content word. Function
// words are excluded: a run that has so far spelled only a particle or an
// auxiliary has not ended a word, which is exactly the position where a
// following particle char is still word-internal (と in ともだち).
bool closesContentWord(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                       const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || end_pos <= start_pos) {
    return false;
  }
  const auto* entry = dict_manager->lookupExact(extractSubstring(codepoints, start_pos, end_pos));
  if (entry == nullptr) {
    return false;
  }
  return entry->pos != core::PartOfSpeech::Particle && entry->pos != core::PartOfSpeech::Auxiliary;
}

// A generated hiragana noun cannot consist solely of two or more closed
// particles. The dynamic program preserves a complete multi-mora particle as
// one grammatical unit while rejecting accidental noun rescue paths such as
// へ+と. Lexical dictionary readings remain separate candidates.
bool decomposesIntoMultipleParticles(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                     const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || end_pos <= start_pos + 1) {
    return false;
  }
  return maximalSegmentCount(*dict_manager, codepoints, start_pos, end_pos, core::PartOfSpeech::Particle) >= 2;
}

bool isFollowedByNominalParticle(const std::vector<char32_t>& codepoints, size_t end_pos,
                                 const dictionary::DictionaryManager* dict_manager) {
  // Longest nominal-selecting particle in the closed class is three codepoints.
  constexpr size_t kParticleProbe = 3;
  return hasDictionaryEntryFrom(dict_manager, codepoints, end_pos, 1, kParticleProbe, core::PartOfSpeech::Particle,
                                [](const dictionary::DictionaryEntry& entry) {
                                  return entry.extended_pos == core::ExtendedPOS::ParticleCase ||
                                         entry.extended_pos == core::ExtendedPOS::ParticleTopic ||
                                         entry.extended_pos == core::ExtendedPOS::ParticleAdverbial ||
                                         entry.extended_pos == core::ExtendedPOS::ParticleBinding ||
                                         entry.extended_pos == core::ExtendedPOS::ParticleNo;
                                });
}

// Phonologically impossible hiragana word starts: small kana (拗音・促音), the
// moraic nasal ん, and the case particles を/が which never begin a native word.
bool isImpossibleHiraganaStart(char32_t code_point) {
  // Small kana (拗音・促音) share the single kana:: source of truth; ん and the case
  // particles を/が never begin a native hiragana word. Callers gate on hiragana,
  // so the katakana half of isSmallKanaCodepoint is never reached here.
  return kana::isSmallKanaCodepoint(code_point) || code_point == U'ん' || code_point == U'を' || code_point == U'が';
}

// A one-kanji formal noun followed by an attributive na-adjective is a word
// boundary (この時妙なもの, その事不思議な結末). The whole kanji run determines
// this boundary so that shorter fabricated prefixes such as 時不 do not evade it.
bool hasFormalNounNaAdjectiveBoundary(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                      normalize::CharType start_type) {
  if (start_type != normalize::CharType::Kanji || kanji_end <= start_pos + 1 || kanji_end >= codepoints.size() ||
      codepoints[kanji_end] != U'な') {
    return false;
  }
  if (kanji_end + 1 < codepoints.size() && codepoints[kanji_end + 1] == U'ら') {
    return false;
  }

  std::string first_char;
  normalize::encodeUtf8(codepoints[start_pos], first_char);
  return normalize::isFormalNounSurface(first_char);
}

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

// A hiragana run may begin on the okurigana of the preceding kanji verb
// continuative. That ownership is evidence against reusing the mora as the head
// of a longer particle-final unknown word before a nominal selector
// (組み|ひも|を, not 組|みひも|を). Both productive paradigms are reconstructed
// from their final mora.
bool startsAtDictionaryVerbContinuative(const std::vector<char32_t>& codepoints,
                                        const std::vector<normalize::CharType>& char_types, size_t start_pos,
                                        const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || start_pos == 0 || start_pos >= codepoints.size() ||
      char_types[start_pos - 1] != normalize::CharType::Kanji) {
    return false;
  }
  const std::string kanji = extractSubstring(codepoints, start_pos - 1, start_pos);
  const std::string_view godan_ending = grammar::godanBaseSuffixFromIRow(codepoints[start_pos]);
  if (!godan_ending.empty() && verb_helpers::isVerbInDictionary(dict_manager, normalize::concat(kanji, godan_ending))) {
    return true;
  }
  return grammar::isERowCodepoint(codepoints[start_pos]) &&
         verb_helpers::isVerbInDictionary(dict_manager, kanji + normalize::encodeUtf8(codepoints[start_pos]) +
                                                            normalize::encodeUtf8(core::hiragana::kRu));
}

// A same-type run may begin at the final okurigana of a dictionary terminal
// verb rather than its renyokei (書く+だべ).  Once that verb is present, a
// following closed auxiliary belongs to it and cannot be absorbed into the
// run.  Limit the probe to the local candidate window used by this generator.
bool startsAfterDictionaryVerb(const std::vector<char32_t>& codepoints,
                               const std::vector<normalize::CharType>& char_types, size_t start_pos, size_t run_end,
                               const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || start_pos == 0 || start_pos >= run_end ||
      char_types[start_pos - 1] != normalize::CharType::Kanji) {
    return false;
  }
  return hasDictionaryEntryFrom(dict_manager, codepoints, start_pos - 1, 2, run_end - start_pos + 1,
                                core::PartOfSpeech::Verb, nullptr);
}

// A same-type run can also begin inside a dictionary adjective's okurigana
// (静か+な). Keep the copular cell available instead of promoting that tail to
// an unknown noun solely because a following nominalizer resembles a noun
// frame.
bool startsAfterDictionaryAdjective(const std::vector<char32_t>& codepoints,
                                    const std::vector<normalize::CharType>& char_types, size_t start_pos,
                                    size_t run_end, const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || start_pos == 0 || start_pos >= run_end ||
      char_types[start_pos - 1] != normalize::CharType::Kanji) {
    return false;
  }
  return hasDictionaryEntryFrom(dict_manager, codepoints, start_pos - 1, 2, run_end - start_pos + 1,
                                core::PartOfSpeech::Adjective, nullptr);
}

}  // namespace

void UnknownWordGenerator::generateBySameType(const std::vector<char32_t>& codepoints, size_t start_pos,
                                              const std::vector<normalize::CharType>& char_types,
                                              std::vector<UnknownCandidate>& candidates) const {
  if (start_pos >= char_types.size()) {
    return;
  }

  normalize::CharType start_type = char_types[start_pos];
  const bool starts_non_word_run = isNonWordType(start_type);
  if (starts_non_word_run && start_pos > 0 && isNonWordType(char_types[start_pos - 1])) {
    return;
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
      return;
    }
    // A genitive の closes before a dictionary formal noun. Do not manufacture
    // a particle-homographic unknown noun across that boundary (不変+の+もの).
    if (first_char == U'の' && dict_manager_ != nullptr && start_pos + 1 < codepoints.size()) {
      const size_t probe_end = std::min(codepoints.size(), start_pos + static_cast<size_t>(5));
      for (const auto& match : dict_manager_->lookup(extractSubstring(codepoints, start_pos + 1, probe_end), 0)) {
        if (match.entry != nullptr && match.entry->extended_pos == core::ExtendedPOS::NounFormal) {
          return;
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
      return;  // Phonologically impossible word start
    }

    // Skip if starting with demonstrative pronouns (これ, それ, あれ, どれ, etc.)
    // These should be recognized by dictionary lookup, not generated as unknown words.
    if (start_pos + 1 < codepoints.size()) {
      char32_t second_char = codepoints[start_pos + 1];
      if (normalize::isDemonstrativeStart(first_char, second_char)) {
        return;
      }
    }
  }

  const size_t max_len = starts_non_word_run ? char_types.size() - start_pos : getMaxLength(start_type);

  // Position of a single particle character the hiragana scan was allowed to
  // cross (SIZE_MAX = none). Candidates extending past it get a penalty below.
  size_t crossed_particle_pos = SIZE_MAX;

  // Find end of same-type sequence
  size_t end_pos = start_pos + 1;
  while (end_pos < char_types.size() && end_pos - start_pos < max_len) {
    normalize::CharType curr_type = char_types[end_pos];
    char32_t curr_char = codepoints[end_pos];

    // Check if current character matches the sequence type
    bool matches_type = curr_type == start_type || (starts_non_word_run && isNonWordType(curr_type));

    // Variation selectors and invisible word-internal format controls modify
    // the surrounding text rather than opening a new token.
    if (!matches_type &&
        (normalize::isVariationSelector(curr_char) || normalize::isTransparentFormatControl(curr_char))) {
      matches_type = true;
    }

    // A keycap emoji is an ASCII digit, #, or * followed by an optional emoji
    // variation selector and U+20E3.  Its base has a text character type, so
    // preserve this grapheme cluster instead of splitting the enclosing keycap
    // off as a standalone emoji.
    const bool keycap_base = (codepoints[start_pos] >= U'0' && codepoints[start_pos] <= U'9') ||
                             codepoints[start_pos] == U'#' || codepoints[start_pos] == U'*';
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
      // - Sequences starting with を/が never cross: those characters never
      //   start words (see hard break above), so such a sequence is already
      //   a particle chain and must not absorb a following particle
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
        // Common particles は, に, へ + で, と, も, か, が (word boundaries).
        // The nominative が behaves like the rest: it is word-internal in a few
        // native nouns (ひがし, かがみ) and a boundary everywhere else, so it is
        // crossed at most once and penalized. Leaving it out let an opaque run
        // swallow a subject marker whole (見|るが, 分かり|みが).
        // Note: Don't include「や」as it's also the stem of「やる」verb
        if (curr_char == U'は' || curr_char == U'に' || curr_char == U'へ' || curr_char == U'で' ||
            curr_char == U'と' || curr_char == U'も' || curr_char == U'か' || curr_char == U'が') {
          char32_t seq_first_char = codepoints[start_pos];
          if (crossed_particle_pos != SIZE_MAX || seq_first_char == U'を' || seq_first_char == U'が') {
            break;  // Stop before the particle character
          }
          crossed_particle_pos = end_pos;  // Cross one, penalized per length
        }
      }
    }
    ++end_pos;
  }

  // Generate candidates for different lengths
  const bool has_formal_noun_na_adjective_boundary =
      hasFormalNounNaAdjectiveBoundary(codepoints, start_pos, end_pos, start_type);
  const size_t first_candidate_length = starts_non_word_run ? end_pos - start_pos : 1;
  const bool starts_at_dictionary_verb_continuative =
      start_type == normalize::CharType::Hiragana &&
      (startsAtDictionaryVerbContinuative(codepoints, char_types, start_pos, dict_manager_) ||
       startsAfterDictionaryVerb(codepoints, char_types, start_pos, end_pos, dict_manager_));
  const bool starts_after_dictionary_adjective =
      start_type == normalize::CharType::Hiragana &&
      startsAfterDictionaryAdjective(codepoints, char_types, start_pos, end_pos, dict_manager_);
  for (size_t len = first_candidate_length; len <= end_pos - start_pos; ++len) {
    size_t candidate_end = start_pos + len;
    std::string surface = extractSubstring(codepoints, start_pos, candidate_end);

    if (!surface.empty()) {
      if (starts_at_dictionary_verb_continuative && len > 1 &&
          normalize::isParticleCodepoint(codepoints[candidate_end - 1]) && candidate_end < codepoints.size() &&
          isRightBoundaryParticle(codepoints[candidate_end])) {
        continue;
      }
      // A kana run beginning on the okurigana of a dictionary verb cannot
      // absorb a following closed auxiliary.  The first mora belongs to the
      // preceding continuative/terminal verb (書く+だべ, 書く+き); retaining an
      // opaque same-type edge would erase both independently licensed
      // boundaries.
      if (starts_at_dictionary_verb_continuative && len > 1 && dict_manager_ != nullptr) {
        bool embeds_closed_auxiliary = false;
        for (size_t split = start_pos + 1; split < candidate_end; ++split) {
          if (dict_manager_->lookupExact(extractSubstring(codepoints, split, candidate_end),
                                         core::PartOfSpeech::Auxiliary) != nullptr) {
            embeds_closed_auxiliary = true;
            break;
          }
        }
        if (embeds_closed_auxiliary) {
          continue;
        }
      }
      const bool crossed_particle_is_bracketed =
          start_type == normalize::CharType::Hiragana && crossed_particle_pos != SIZE_MAX &&
          candidate_end < codepoints.size() && isRightBoundaryParticle(codepoints[candidate_end]);
      const bool closes_particle_bracketed_hiragana_noun =
          crossed_particle_is_bracketed && candidate_end == crossed_particle_pos + 1;
      // The same bracket one mora further in. A run that spells a particle
      // character word-internally and is closed by a real particle sits in a
      // nominal slot either way, so it takes the nominal category rather than
      // the opaque one and its length penalty (おとな + に, ひとつ + を).
      const bool brackets_medial_particle_crossing =
          crossed_particle_is_bracketed && !started_with_particle && candidate_end > crossed_particle_pos + 1;
      // After a past auxiliary, a following hiragana noun beginning with り
      // must remain available as a nominal host. Otherwise the preceding た
      // absorbs its first mora as the listing particle たり (買っ+た+りんご).
      const bool closes_past_tari_collision_noun =
          start_type == normalize::CharType::Hiragana && start_pos > 0 && len >= 2 &&
          codepoints[start_pos - 1] == U'た' && codepoints[start_pos] == U'り' &&
          (candidate_end == codepoints.size() ||
           (candidate_end < codepoints.size() && isRightBoundaryParticle(codepoints[candidate_end])));
      const bool has_verb_tail_after_ri =
          closes_past_tari_collision_noun && dict_manager_ != nullptr &&
          dict_manager_->lookupExact(extractSubstring(codepoints, start_pos + 1, candidate_end),
                                     core::PartOfSpeech::Verb) != nullptr;
      const bool has_inflected_verb_reading =
          closes_past_tari_collision_noun && std::any_of(inflection_.analyze(surface).begin(),
                                                         inflection_.analyze(surface).end(), [](const auto& analysis) {
                                                           return analysis.verb_type != grammar::VerbType::Unknown &&
                                                                  analysis.verb_type != grammar::VerbType::IAdjective &&
                                                                  !analysis.morphemes.empty() &&
                                                                  analysis.confidence > candidate::kNoOriginConfidence;
                                                         });
      const bool has_contracted_progressive_tail =
          closes_past_tari_collision_noun && len >= 2 &&
          grammar::isContractedProgressiveSurface(extractSubstring(codepoints, candidate_end - 2, candidate_end));
      const bool precedes_closed_native_number =
          start_type == normalize::CharType::Hiragana && startsClosedNativeNumber(codepoints, candidate_end);
      const bool closes_genitive_negative_noun =
          start_type == normalize::CharType::Hiragana && candidate_end - start_pos >= 2 &&
          !starts_at_dictionary_verb_continuative && !starts_after_dictionary_adjective &&
          candidate_end + 1 < codepoints.size() && codepoints[candidate_end] == U'の' &&
          codepoints[candidate_end + 1] == U'な';
      // Particle-start hiragana sequences are potential nouns (はし, はな, にく)
      // Use NOUN POS instead of OTHER to avoid exceeds_dict_length penalty
      core::PartOfSpeech pos =
          (started_with_particle || closes_particle_bracketed_hiragana_noun || brackets_medial_particle_crossing ||
           (closes_past_tari_collision_noun && !has_verb_tail_after_ri && !has_inflected_verb_reading &&
            !has_contracted_progressive_tail) ||
           precedes_closed_native_number || closes_genitive_negative_noun)
              ? core::PartOfSpeech::Noun
              : getPosForType(start_type);
      float cost = getCostForType(start_type, len);
      if (closes_past_tari_collision_noun && !has_verb_tail_after_ri && !has_inflected_verb_reading &&
          !has_contracted_progressive_tail) {
        cost = candidate::kSelectedNominalShortHeadCost;
      }
      if (precedes_closed_native_number || closes_genitive_negative_noun) {
        cost = candidate::kSelectedNominalShortHeadCost + bigram_cost::kDoubleVeryStrongBonus;
      }
      if (has_formal_noun_na_adjective_boundary && len >= 2) {
        cost += candidate::kFormalNounNaAdjectiveBoundaryPenalty;
      }

      // Penalize kanji sequences ending with honorific/title suffixes (様, 氏)
      // to encourage NOUN + SUFFIX separation (e.g., 客様 → 客 + 様, 田中様 → 田中 + 様)
      // Note: 的 was removed — kanji_seq cost 1.0 with 1-char prefix (目+的 = 1.1)
      // naturally keeps 目的/動的/知的/射的 as 1 token while 論理+的 still splits
      // (2-char prefix gives 論理(1.0)+的(SUFFIX 0.5)-0.8 = 0.7 < 1.0).
      if (start_type == normalize::CharType::Kanji && len >= 2) {
        char32_t last_char = codepoints[candidate_end - 1];
        if (last_char == U'様' || last_char == U'氏') {
          cost += 4.0F;  // Strong penalty to prefer NOUN + SUFFIX path
        }
      }

      // Penalize kanji sequences starting with the prefix kanji 御.
      // 御 is an L1 PREFIX entry and should split off as a productive prefix
      // (御 + 尽力, 御 + 挨拶, 御 + 協力). The +2.0 penalty makes the PREFIX path
      // win over any 2+ char kanji_seq starting with 御. Lexicalized 御-X nouns
      // (御者, 御所, 御曹司) come from the dictionary and get a separate bonus
      // in scorer.cpp to beat the prefix path.
      if (start_type == normalize::CharType::Kanji && len >= 2 && codepoints[start_pos] == U'御') {
        cost += 2.0F;
      }

      // Skip kanji sequences starting with iteration mark (々)
      // 々 always attaches to the preceding kanji (人々, 時々)
      // It can never start a word
      if (start_type == normalize::CharType::Kanji && normalize::isIterationMark(codepoints[start_pos])) {
        continue;
      }

      // Penalize kanji sequences that extend past iteration mark (々), except
      // for a complete double-reduplication X々Y々 (津々浦々, 様々). The latter
      // is a productive four-character compound shape and must retain its
      // whole-word candidate; a lone completed pair followed by another kanji
      // (時々妙) is still a boundary.
      if (start_type == normalize::CharType::Kanji && len >= 3) {
        bool is_double_reduplication = len == 4 && normalize::isIterationMark(codepoints[start_pos + 1]) &&
                                       normalize::isIterationMark(codepoints[start_pos + 3]);
        for (size_t i = start_pos + 1; i < candidate_end - 1; ++i) {
          if (!is_double_reduplication && normalize::isIterationMark(codepoints[i])) {
            // Found 々 in the middle - penalize extending past it
            cost += 5.0F;
            break;
          }
        }
      }

      // Penalize kanji sequences with interrogative kanji (何, 誰, 幾) at NON-initial position
      // e.g., 今何 should be split as 今 + 何, not kept as one compound
      // But 何日, 何人 (interrogative + counter) should stay together
      // Interrogatives are standalone words unless they're at the start (counter pattern)
      if (start_type == normalize::CharType::Kanji && len >= 2) {
        for (size_t i = start_pos + 1; i < candidate_end; ++i) {  // Skip first char
          if (isInterrogativeKanji(codepoints[i])) {
            // Heavy penalty to force split
            cost += 3.0F;
            break;
          }
        }
      }

      // A temporal prefix that is itself a standalone noun (今) heads a temporal
      // compound, never an arbitrary one: 今週/今回/今後 continue it, an ordinary
      // noun does not (今|紙, 今|本, 今|大会). Without this the generic run cost
      // (shorter than the sum of its parts) glues the adverbial 今 to whatever
      // object follows. The sibling prefixes are not standalone nouns — 先 is a
      // suffix and 来 a verb stem — so 先方/来客 keep their run.
      if (start_type == normalize::CharType::Kanji && len >= 2 && dict_manager_ != nullptr &&
          isPrefixLikeKanji(codepoints[start_pos]) &&
          !normalize::continuesTemporalNounCompound(codepoints[start_pos], codepoints[start_pos + 1]) &&
          (start_pos == 0 || char_types[start_pos - 1] != normalize::CharType::Kanji)) {
        const auto* head = dict_manager_->lookupExact(extractSubstring(codepoints, start_pos, start_pos + 1),
                                                      core::PartOfSpeech::Noun);
        if (head != nullptr && head->extended_pos == core::ExtendedPOS::Noun) {
          continue;
        }
      }

      // Penalize hiragana candidates that include an internal particle
      // character (see scan loop above). The penalty keeps particle splits
      // preferred when the prefix is a plausible word (ここ+で beats ここで),
      // while letting genuine nouns spanning a particle char (こども, ひとつ)
      // win when the split leaves an implausible fragment (こど+も, ひ+と+つ).
      // - 2-char candidates (single char + particle char) are skipped unless
      //   another nominal particle closes the run; that bracket proves the
      //   particle-like mora is word-internal (ひも+を)
      // - Particle-final candidates (こども) get a minor penalty
      // - Medial crossing (one char after the particle, e.g. ひとつ) is less
      //   plausible and gets a strong penalty; genuine words still win
      //   because their split path needs multiple unknown 1-char fragments
      if (start_type == normalize::CharType::Hiragana && !started_with_particle &&
          candidate_end > crossed_particle_pos) {
        if (len < 3 && !closes_particle_bracketed_hiragana_noun) {
          continue;
        }
        // The bracket above is also what separates a native noun from a real
        // particle boundary here: without it the strong penalty made the noun
        // lose to a chain of one-character fragments whose own reading strands
        // the run's head (おとな|に|なる, not お|と|なに|なる).
        cost += (candidate_end > crossed_particle_pos + 1 && !brackets_medial_particle_crossing)
                    ? scorer::scale::kStrong
                    : scorer::scale::kMinor;
        // The mora allowed past the crossed particle exists for native nouns
        // that spell a particle word-internally (こども, ひとつ). It must not be
        // taken from the front of a bound word instead: an auxiliary or a
        // particle attaches leftward, so its own left edge is a fixed boundary
        // and the run has to stop there (見|る|が|ごとく, not 見|るがご|とく).
        // An unbound word heading the same position proves nothing, since a
        // native noun may simply spell it (おとな + になる, not お + と + なに).
        constexpr size_t kStraddledWordProbe = 4;
        if (candidate_end > crossed_particle_pos + 1 && dict_manager_ != nullptr &&
            hasDictionaryEntryFrom(dict_manager_, codepoints, candidate_end - 1, 2, kStraddledWordProbe,
                                   core::PartOfSpeech::Unknown, [](const dictionary::DictionaryEntry& entry) {
                                     return entry.pos == core::PartOfSpeech::Auxiliary ||
                                            entry.pos == core::PartOfSpeech::Particle;
                                   })) {
          continue;
        }
      }

      // The conditional particle ば cannot close an open nominal run. A listed
      // noun such as ことば retains its dictionary edge, while an unverified
      // hiragana fallback must leave the particle available at the boundary.
      if (start_type == normalize::CharType::Hiragana && !started_with_particle && dict_manager_ != nullptr &&
          candidate_end > start_pos + 1) {
        const auto* final_particle = dict_manager_->lookupExact(
            extractSubstring(codepoints, candidate_end - 1, candidate_end), core::PartOfSpeech::Particle);
        if (final_particle != nullptr && final_particle->extended_pos == core::ExtendedPOS::ParticleConj &&
            codepoints[candidate_end - 1] == U'ば') {
          continue;
        }
      }

      // A closed interjection cannot host the nominalizing suffix さ. When
      // that shape occurs, retain a complete unknown noun candidate as the
      // only grammatical lexical reading (うわ+さ -> うわさ), rather than
      // letting the suffix expose the interjection as an independent token.
      if (start_type == normalize::CharType::Hiragana && !started_with_particle && len >= 3 &&
          codepoints[candidate_end - 1] == U'さ' && dict_manager_ != nullptr) {
        const bool follows_prefix =
            start_pos > 0 && dict_manager_->lookupExact(extractSubstring(codepoints, start_pos - 1, start_pos),
                                                        core::PartOfSpeech::Prefix) != nullptr;
        const auto* interjection = dict_manager_->lookupExact(
            extractSubstring(codepoints, start_pos, candidate_end - 1), core::PartOfSpeech::Interjection);
        if (interjection != nullptr && !follows_prefix) {
          auto noun_candidate = makeCandidate(surface, start_pos, candidate_end, core::PartOfSpeech::Noun,
                                              candidate::kSelectedNominalShortHeadCost,
                                              /*has_suffix=*/true, CandidateOrigin::SameType);
#ifdef SUZUME_DEBUG_INFO
          noun_candidate.pattern = "interjection_nominal_boundary";
#endif
          candidates.push_back(std::move(noun_candidate));
        }
      }

      // Penalize hiragana sequences starting with particle characters
      // These could be nouns (はし, はな, にく, にゃんこ) but are less likely than
      // the particle interpretation, unless the particle path has connection penalties
      bool has_suffix = closes_past_tari_collision_noun && !has_verb_tail_after_ri && !has_inflected_verb_reading &&
                        !has_contracted_progressive_tail;
      if (started_with_particle) {
        if (len == 1) {
          continue;  // Single-char particle-start never forms a noun alone
        }
        // Check if this is a reduplicated pattern (same character repeated)
        // Reduplicated hiragana like はは (母), ちち (父) are likely real words
        bool is_reduplicated = (len == 2 && codepoints[start_pos] == codepoints[start_pos + 1]);
        if (is_reduplicated) {
          // Small bonus for reduplicated patterns - they're often real words
          cost -= 0.5F;
        } else if (len == 2) {
          // 2-char: light penalty — bigram penalties on unnatural particle chains
          // provide enough discouragement for false splits (は+し vs はし)
          cost += 0.5F;
        } else if (len == 3) {
          // 3-char: moderate penalty (にある, によれ are likely particle chains)
          cost += 0.8F;
        } else {
          // 4+ char: heavier penalty scaling with length
          // but still generated so words like にゃんこ have a chance
          cost += 1.0F + static_cast<float>(len - 3) * 0.5F;
        }
        // Mark as has_suffix to skip exceeds_dict_length penalty in tokenizer
        has_suffix = true;
      }
      if (started_with_particle && !isFollowedByNominalParticle(codepoints, candidate_end, dict_manager_) &&
          decomposesIntoMultipleParticles(codepoints, start_pos, candidate_end, dict_manager_)) {
        continue;
      }
      // A kanji run must not end on the kanji that heads a bound suffix: the
      // suffix owns that character together with its okurigana (画面|越し,
      // 条件|付き), so a run reaching into it is a boundary error.
      if (start_type == normalize::CharType::Kanji && dict_manager_ != nullptr && candidate_end < codepoints.size() &&
          char_types[candidate_end] == normalize::CharType::Hiragana) {
        constexpr size_t kSuffixProbe = 3;
        const size_t probe_end = std::min(codepoints.size(), candidate_end + kSuffixProbe);
        bool heads_bound_suffix = false;
        for (size_t suffix_end = candidate_end + 1; suffix_end <= probe_end; ++suffix_end) {
          if (dict_manager_->lookupExact(extractSubstring(codepoints, candidate_end - 1, suffix_end),
                                         core::PartOfSpeech::Suffix) != nullptr) {
            heads_bound_suffix = true;
            break;
          }
        }
        if (heads_bound_suffix && len > 1) {
          continue;
        }
      }
      // A quantity head cannot be joined to the one-kanji stem of a following
      // sokuonbin predicate. The sequence is a noun phrase plus a verb
      // (二件|残っ, 複数|残っ), never an unknown compound noun ending at the
      // predicate stem. This also leaves the っ available to the verb edge.
      if (start_type == normalize::CharType::Kanji && len >= 3 && candidate_end < codepoints.size() &&
          codepoints[candidate_end] == U'っ') {
        const size_t head_end = candidate_end - 1;
        const bool numeral_counter_head = head_end >= start_pos + 2 &&
                                          normalize::isCounterKanji(codepoints[head_end - 1]) &&
                                          normalize::isNumeralCodepoint(codepoints[head_end - 2]);
        const bool quantity_noun_head = head_end >= start_pos + 2 && codepoints[head_end - 1] == U'数' &&
                                        normalize::isKanjiCodepoint(codepoints[head_end - 2]);
        if (numeral_counter_head || quantity_noun_head) {
          continue;
        }
      }
      // The mirror boundary: a kanji run must not open with a registered
      // multi-kanji formal noun (以来|問題, 途中|経過). A formal noun is a bound
      // right-hand element, so the material after it starts a new phrase rather
      // than continuing one compound. One-kanji formal nouns are excluded — 内,
      // 中, 手, 先 head ordinary kango (内容, 中止, 手法, 先方).
      if (start_type == normalize::CharType::Kanji && dict_manager_ != nullptr && len > 2) {
        bool opens_with_formal_noun = false;
        for (size_t head_end = start_pos + 2; head_end < candidate_end; ++head_end) {
          const auto* entry =
              dict_manager_->lookupExact(extractSubstring(codepoints, start_pos, head_end), core::PartOfSpeech::Noun);
          if (entry != nullptr && entry->extended_pos == core::ExtendedPOS::NounFormal) {
            opens_with_formal_noun = true;
            break;
          }
        }
        if (opens_with_formal_noun) {
          continue;
        }
      }
      // A formal noun is a bound right-hand element carrying its own edge, so
      // an opaque hiragana run must not close on one. Closing there hides the
      // boundary in front of it and buries a closed-class morpheme inside a
      // fabricated noun (見|た|こと|の|ない, not 見|たこと|の|ない). A run that
      // *is* the formal noun keeps its candidate, since nothing precedes it
      // inside the span.
      if (start_type == normalize::CharType::Hiragana && !started_with_particle && len > 1 &&
          dict_manager_ != nullptr) {
        bool closes_on_formal_noun = false;
        for (size_t split = start_pos + 1; split < candidate_end; ++split) {
          const auto* tail =
              dict_manager_->lookupExact(extractSubstring(codepoints, split, candidate_end), core::PartOfSpeech::Noun);
          if (tail != nullptr && tail->extended_pos == core::ExtendedPOS::NounFormal) {
            closes_on_formal_noun = true;
            break;
          }
        }
        if (closes_on_formal_noun) {
          continue;
        }
      }
      // A hiragana fallback run has no analysis of its own, so it must not
      // cross a case particle that closes a registered word: that boundary is
      // proven (見る|が|ごとし, not 見|るが|ごとし). A run whose kana merely
      // spell a particle keeps its candidate, because nothing ends in front of
      // it (ひがし, たまご).
      if (start_type == normalize::CharType::Hiragana && len > 1 && dict_manager_ != nullptr) {
        bool crosses_proven_particle = false;
        for (size_t particle_pos = start_pos; particle_pos < candidate_end; ++particle_pos) {
          const auto* particle = dict_manager_->lookupExact(
              extractSubstring(codepoints, particle_pos, particle_pos + 1), core::PartOfSpeech::Particle);
          if (particle == nullptr || particle->extended_pos != core::ExtendedPOS::ParticleCase) {
            continue;
          }
          const size_t scan_start =
              particle_pos > kDictionaryLookbehindChars ? particle_pos - kDictionaryLookbehindChars : 0;
          if (hasDictionaryEntryEndingAt(*dict_manager_, codepoints, scan_start, particle_pos,
                                         partOfSpeechMask(core::PartOfSpeech::Verb) |
                                             partOfSpeechMask(core::PartOfSpeech::Adjective) |
                                             partOfSpeechMask(core::PartOfSpeech::Noun))) {
            crosses_proven_particle = true;
            break;
          }
        }
        if (crosses_proven_particle) {
          continue;
        }
      }
      auto cand = makeCandidate(surface, start_pos, candidate_end, pos, cost, has_suffix, CandidateOrigin::SameType);
#ifdef SUZUME_DEBUG_INFO
      cand.confidence = started_with_particle ? 0.7F : 1.0F;
      switch (start_type) {
        case normalize::CharType::Kanji:
          cand.pattern = "kanji_seq";
          break;
        case normalize::CharType::Katakana:
          cand.pattern = "kata_seq";
          break;
        case normalize::CharType::Hiragana:
          cand.pattern = started_with_particle ? "hira_noun_seq" : "hira_seq";
          break;
        case normalize::CharType::Alphabet:
          cand.pattern = "alpha_seq";
          break;
        case normalize::CharType::Digit:
          cand.pattern = "digit_seq";
          break;
        default:
          cand.pattern = "other_seq";
          break;
      }
#endif
      candidates.push_back(cand);

      // Emit a standalone SUFFIX candidate for plural-honorific 方 when it sits
      // at the tail of a kanji_seq (i.e., preceded by another kanji). Enables
      // splits like 皆様(NOUN) + 方(SUFFIX) for 皆様方. Restricting to "prev is
      // kanji" avoids false splits like その方(NOUN), 北の方(NOUN) where 方 is
      // a standalone noun, not a plural-honorific suffix.
      if (start_type == normalize::CharType::Kanji && len == 1 && codepoints[start_pos] == U'方' && start_pos > 0 &&
          char_types[start_pos - 1] == normalize::CharType::Kanji) {
        auto suffix_cand = makeCandidate(surface, start_pos, candidate_end, core::PartOfSpeech::Suffix, 0.5F,
                                         /*has_suffix=*/true, CandidateOrigin::SameType);
#ifdef SUZUME_DEBUG_INFO
        suffix_cand.confidence = 1.0F;
        suffix_cand.pattern = "tail_suffix_方";
#endif
        candidates.push_back(suffix_cand);
      }
    }
  }

  // Productive hiragana nominal + adjective-continuative boundary.  This is
  // deliberately independent of particle bracketing: literary adverbials
  // commonly occur at the beginning of a clause (よどみなく話す).
  if (start_type == normalize::CharType::Hiragana && hasHiraganaNominalNakuEnding(codepoints, start_pos, end_pos)) {
    const size_t nominal_end = end_pos - 2;
    const size_t nominal_len = nominal_end - start_pos;
    std::string surface = extractSubstring(codepoints, start_pos, nominal_end);
    auto noun_cand =
        makeCandidate(surface, start_pos, nominal_end, core::PartOfSpeech::Noun,
                      getCostForType(start_type, nominal_len) + candidate::kHiraganaNominalNakuCandidateBonus,
                      /*has_suffix=*/true, CandidateOrigin::SameType);
#ifdef SUZUME_DEBUG_INFO
    noun_cand.pattern = "hiragana_nominal_naku";
#endif
    candidates.push_back(noun_cand);
  }

  // Bracketed hiragana noun promotion. A short hiragana run genuinely bracketed by
  // particles (私は|たばこ|を, 彼は|ともだち|と) reads as a content noun, but the
  // same-type scan above truncates at the first internal particle character and a
  // particle-initial run (にんじん) is only ever emitted as a penalized particle-noun,
  // so the correct whole-run candidate never reaches the lattice. This dedicated
  // scan is independent of that truncation and emits an ADDITIVE Noun candidate; the
  // Other/particle candidates remain and any real dictionary/verb/adverb reading of
  // the span still outranks the ~1.8 Noun, so it wins only when nothing better spans
  // the bracket and never shatters the run. Left bracket: a boundary particle
  // preceded by a non-hiragana content word (私は…, 彼は…). Right bracket: a boundary
  // particle. の is excluded on both sides (genitive marks a compound boundary).
  // Left bracket: a boundary particle after a non-hiragana content word (私は…), or a
  // clause boundary — sentence start / a preceding symbol (punctuation). の is not a
  // left boundary here (genitive marks a compound boundary).
  bool left_particle_bracket = start_pos >= 1 && isLeftBoundaryParticle(codepoints[start_pos - 1]);
  const auto* left_particle = dict_manager_ != nullptr && start_pos >= 1
                                  ? dict_manager_->lookupExact(extractSubstring(codepoints, start_pos - 1, start_pos),
                                                               core::PartOfSpeech::Particle)
                                  : nullptr;
  const bool left_genitive_bracket = left_particle != nullptr &&
                                     left_particle->extended_pos == core::ExtendedPOS::ParticleNo &&
                                     codepoints[start_pos - 1] == U'の';
  bool left_determiner_bracket = false;
  if (dict_manager_ != nullptr) {
    const size_t lookback = std::min(start_pos, static_cast<size_t>(4));
    for (size_t length = 1; length <= lookback; ++length) {
      const std::string preceding = extractSubstring(codepoints, start_pos - length, start_pos);
      const auto* entry = dict_manager_->lookupExact(preceding, core::PartOfSpeech::Determiner);
      if (entry != nullptr) {
        left_determiner_bracket = true;
        break;
      }
    }
  }
  bool left_clause_bracket =
      (start_pos == 0) || (start_pos >= 1 && char_types[start_pos - 1] == normalize::CharType::Symbol);
  const bool left_attributive_bracket = start_pos > 0;
  if (start_type == normalize::CharType::Hiragana &&
      (left_particle_bracket || left_determiner_bracket || left_clause_bracket || left_attributive_bracket) &&
      !isImpossibleHiraganaStart(codepoints[start_pos])) {
    constexpr size_t kDefaultBracketedNounLength = 4;
    constexpr size_t kLongDeverbalNounLength = 5;
    const bool long_deverbal_object_shape =
        getMaxLength(start_type) >= kLongDeverbalNounLength &&
        start_pos + kLongDeverbalNounLength < codepoints.size() &&
        std::all_of(char_types.begin() + static_cast<std::ptrdiff_t>(start_pos),
                    char_types.begin() + static_cast<std::ptrdiff_t>(start_pos + kLongDeverbalNounLength),
                    [](normalize::CharType type) { return type == normalize::CharType::Hiragana; }) &&
        grammar::isERowCodepoint(codepoints[start_pos + kLongDeverbalNounLength - 1]) &&
        codepoints[start_pos + kLongDeverbalNounLength] == U'を';
    const size_t bracketed_noun_limit =
        long_deverbal_object_shape ? kLongDeverbalNounLength : kDefaultBracketedNounLength;
    bool particle_initial =
        (codepoints[start_pos] == U'は' || codepoints[start_pos] == U'に' || codepoints[start_pos] == U'へ');
    size_t max_internal = particle_initial ? 0 : 2;
    size_t internal_particles = 0;
    // A multi-char L1 particle (ながら, まで, から, だけ, …) beginning at a position is a
    // real right boundary: terminate the run there rather than swallowing its head into
    // the noun (…およぎ|ながら, never およぎな|がら where ながら's が is mistaken for a bracket).
    // A coordinating conjunction brackets the run the same way (りんご|または|みかん);
    // without it the run runs on past the conjunction's head and the whole
    // all-hiragana coordination shatters into closed-class fragments. It needs one
    // more mora than a particle because the two-mora conjunctions share their kana
    // with word-internal sequences (あまた, したがって).
    auto multi_char_function_word_at = [&](size_t pos) -> bool {
      if (dict_manager_ == nullptr || pos >= codepoints.size()) {
        return false;
      }
      size_t win_end = pos + 4 < codepoints.size() ? pos + 4 : codepoints.size();
      std::string window = extractSubstring(codepoints, pos, win_end);
      for (const auto& res : dict_manager_->lookup(window, 0)) {
        if (res.entry == nullptr) {
          continue;
        }
        if (res.entry->pos == core::PartOfSpeech::Particle && res.length >= 2) {
          return true;
        }
        if (res.entry->pos == core::PartOfSpeech::Conjunction && res.length >= 3) {
          return true;
        }
      }
      return false;
    };
    auto lies_inside_formal_noun_negative_predicate = [&](size_t pos) -> bool {
      if (dict_manager_ == nullptr) {
        return false;
      }
      for (size_t predicate_start = start_pos + 1; predicate_start < pos; ++predicate_start) {
        const std::string prefix = extractSubstring(codepoints, start_pos, predicate_start);
        const auto* noun = dict_manager_->lookupExact(prefix, core::PartOfSpeech::Noun);
        if (noun == nullptr || noun->extended_pos != core::ExtendedPOS::NounFormal) {
          continue;
        }
        const auto predicates = analysis::generateHiraganaVerbCandidates(
            codepoints, predicate_start, char_types, inflection_, dict_manager_, options_.verb_candidate_options);
        for (const auto& predicate : predicates) {
          if (predicate.extended_pos != core::ExtendedPOS::VerbMizenkei || predicate.end <= pos ||
              predicate.end >= codepoints.size()) {
            continue;
          }
          const size_t probe_end = std::min(codepoints.size(), predicate.end + static_cast<size_t>(2));
          const std::string following = extractSubstring(codepoints, predicate.end, probe_end);
          for (const auto& match : dict_manager_->lookup(following, 0)) {
            if (match.entry != nullptr && (match.entry->extended_pos == core::ExtendedPOS::AuxNegativeNu ||
                                           match.entry->extended_pos == core::ExtendedPOS::AuxNegativeNai)) {
              return true;
            }
          }
        }
        const size_t auxiliary_limit = std::min(codepoints.size(), pos + static_cast<size_t>(5));
        for (size_t auxiliary_start = pos + 1; auxiliary_start < auxiliary_limit; ++auxiliary_start) {
          const std::string window = extractSubstring(codepoints, auxiliary_start, auxiliary_limit);
          for (const auto& match : dict_manager_->lookup(window, 0)) {
            if (match.entry == nullptr || (match.entry->extended_pos != core::ExtendedPOS::AuxNegativeNu &&
                                           match.entry->extended_pos != core::ExtendedPOS::AuxNegativeNai)) {
              continue;
            }
            const std::string full_predicate =
                extractSubstring(codepoints, predicate_start, auxiliary_start + match.length);
            for (const auto& analysis : inflection_.analyze(full_predicate)) {
              if (analysis.verb_type != grammar::VerbType::Unknown &&
                  analysis.verb_type != grammar::VerbType::IAdjective && !analysis.morphemes.empty() &&
                  analysis.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence) {
                return true;
              }
            }
          }
        }
      }
      return false;
    };
    bool crossed_verified_predicate = false;
    size_t scan = start_pos + 1;
    while (scan < codepoints.size() && scan - start_pos < bracketed_noun_limit &&
           char_types[scan] == normalize::CharType::Hiragana) {
      char32_t curr = codepoints[scan];
      if (curr == U'を') {
        break;  // accusative を does not sit inside a native hiragana noun
      }
      // Once a substantive three-mora run has formed, a case/topic or genitive
      // particle starts the right bracket even when the next word is also
      // hiragana (あたり|は|すっかり, となり|の|いえ). Shorter offsets remain
      // eligible as genuine word-internal homographs.
      const auto* single_particle =
          dict_manager_ != nullptr
              ? dict_manager_->lookupExact(extractSubstring(codepoints, scan, scan + 1), core::PartOfSpeech::Particle)
              : nullptr;
      const bool is_genitive_particle =
          single_particle != nullptr && single_particle->extended_pos == core::ExtendedPOS::ParticleNo && curr == U'の';
      // In …のの…, the first の may be the final mora of the preceding
      // hiragana noun while the second is the genitive marker. Keep scanning
      // through that first mora so the noun candidate can claim it; the
      // particle-particle bigram remains a safety net for malformed paths.
      const bool repeats_genitive =
          is_genitive_particle && scan + 1 < codepoints.size() && codepoints[scan + 1] == U'の';
      if (scan - start_pos >= 3 && (isRightBoundaryParticle(curr) || (is_genitive_particle && !repeats_genitive))) {
        break;
      }
      // A multi-character particle immediately after the first mora can be
      // part of a native hiragana noun (こども).  Require a substantive
      // preceding run before treating it as an internal word boundary.
      if (scan - start_pos >= 2 && multi_char_function_word_at(scan)) {
        if (lies_inside_formal_noun_negative_predicate(scan)) {
          crossed_verified_predicate = true;
        } else {
          break;  // stop before a multi-char particle boundary
        }
      }
      if (isInternalParticleChar(curr)) {
        // A particle char followed by a fresh (non-hiragana) word is a trailing case
        // particle (…およぎ|に|行く): stop before it so the right-bracket test sees it.
        // At the run's end it is word-final (こども), so keep it, capped by max_internal.
        bool word_follows = scan + 1 < codepoints.size() && char_types[scan + 1] != normalize::CharType::Hiragana;
        // A particle is only word-internal where no word has ended yet. Once the
        // run so far is itself a listed content word, the particle attaches to
        // that word (ただ+で), and swallowing it invents a nominal that then
        // outscores the real adverb.
        if (word_follows || internal_particles >= max_internal ||
            closesContentWord(codepoints, start_pos, scan, dict_manager_)) {
          break;
        }
        ++internal_particles;
      }
      ++scan;
    }
    auto emit_promoted_run = [&](size_t run_end) {
      size_t len = run_end - start_pos;
      // The nominalizer ん closes an attributive predicate, so a run ending on
      // it is that predicate plus the particle, never one unregistered noun
      // (できる+ん+じゃ+ない). A registered predicate in front of it is the
      // evidence; runs whose kana merely happen to spell a particle keep their
      // whole-run candidate (りんご, たなばた).
      if (dict_manager_ != nullptr && run_end > start_pos + 1 && codepoints[run_end - 1] == U'ん' &&
          hasExactPartOfSpeech(
              *dict_manager_, extractSubstring(codepoints, start_pos, run_end - 1),
              partOfSpeechMask(core::PartOfSpeech::Verb) | partOfSpeechMask(core::PartOfSpeech::Adjective))) {
        return;
      }
      // Right bracket: a single boundary particle, a multi-char particle start, or a
      // clause boundary (sentence end / symbol).
      const size_t scan = run_end;
      const bool right_genitive_after_internal_particle =
          scan < codepoints.size() && codepoints[scan] == U'の' && internal_particles > 0;
      // A genitive の is normally no bracket at all, because it just as often
      // marks a boundary inside the run. Once the run reaches the substantive
      // three-mora length it does delimit the modifier, exactly as a case
      // particle does at the same length. Without this an unregistered hiragana
      // noun before の has no whole-run candidate and shatters into a chain of
      // closed-class fragments (りんごの色).
      const bool right_genitive_after_substantive_run =
          scan < codepoints.size() && codepoints[scan] == U'の' && scan - start_pos >= 3;
      bool right_particle = (scan < codepoints.size() && isRightBoundaryParticle(codepoints[scan])) ||
                            multi_char_function_word_at(scan) || right_genitive_after_internal_particle ||
                            right_genitive_after_substantive_run;
      bool right_sokuon_final_particle = false;
      if (dict_manager_ != nullptr && scan < codepoints.size() && codepoints[scan] == U'っ') {
        const size_t particle_end = std::min(codepoints.size(), scan + static_cast<size_t>(4));
        for (const auto& match : dict_manager_->lookup(extractSubstring(codepoints, scan, particle_end), 0)) {
          if (match.entry != nullptr && match.entry->extended_pos == core::ExtendedPOS::ParticleFinal) {
            right_sokuon_final_particle = true;
            break;
          }
        }
      }
      bool right_clause =
          (scan == codepoints.size()) || (scan < codepoints.size() && char_types[scan] == normalize::CharType::Symbol);
      // An auxiliary is bound leftward, so it brackets the run in front of it just
      // as a particle does. It does not select the run the way a case particle
      // does, so it only makes the candidate available.
      const bool right_auxiliary = dict_manager_ != nullptr && scan < codepoints.size() &&
                                   dict_manager_->lookupExact(extractSubstring(codepoints, scan, scan + 1),
                                                              core::PartOfSpeech::Auxiliary) != nullptr;
      // A two-mora run is safe when particles bracket it (私は|はし|を), or
      // when an unambiguous single case particle selects an otherwise
      // unregistered run at a clause boundary (さき|に). Quotative と and
      // multi-mora particles select predicates too and do not qualify.
      // Without that evidence, a run leaning on a clause boundary still needs
      // length >= 3, so short isolated hiragana — usually adverbs/particles
      // (もう, すぐ, ため) — are not promoted.
      // A copula that heads its own predicate is the same kind of right bracket: it
      // selects a nominal, so what stands in front of it is a noun however short it is
      // (きのう|は|あめ|だっ|た). That selection does not depend on what stands to the
      // left, so the copula licenses the short run at a clause boundary as well
      // (くつ|だっ|た) — but only for a run the dictionary does not already read,
      // because at two morae the words standing there are overwhelmingly closed or
      // adverbial (から|だ|を, まじ|で, そう|じゃろう) and the rescue exists for the
      // nouns that have no reading at all. After a particle the left bracket
      // supplies that evidence itself. Any other auxiliary only makes the candidate
      // available, because its own kana could equally be the run's last mora.
      const BoundAuxiliary right_bound = boundAuxiliaryAt(codepoints, scan, dict_manager_, left_particle_bracket);
      const bool right_copula = right_bound.length > 0 && right_bound.is_copula;
      const std::string promoted_surface = extractSubstring(codepoints, start_pos, scan);
      const auto* promoted_dictionary_reading =
          dict_manager_ != nullptr ? dict_manager_->lookupExact(promoted_surface) : nullptr;
      const auto* absorbed_auxiliary =
          dict_manager_ != nullptr && scan > start_pos
              ? dict_manager_->lookupExact(extractSubstring(codepoints, scan - 1, scan), core::PartOfSpeech::Auxiliary)
              : nullptr;
      // A sokuon-initial final particle may follow a completed nominal, but it
      // cannot license a noun candidate that has swallowed the copula
      // immediately before it. That boundary is inflectional
      // (りんご|だっ|たら), so the copula must remain available to the auxiliary
      // path instead of becoming the noun's last mora.
      const bool absorbs_copula_before_sokuon_final =
          right_sokuon_final_particle && absorbed_auxiliary != nullptr &&
          absorbed_auxiliary->extended_pos == core::ExtendedPOS::AuxCopulaDa;
      constexpr PartOfSpeechMask kPredicateMask = partOfSpeechMask(core::PartOfSpeech::Verb) |
                                                  partOfSpeechMask(core::PartOfSpeech::Adjective) |
                                                  partOfSpeechMask(core::PartOfSpeech::Auxiliary);
      const bool has_exact_noun =
          dict_manager_ != nullptr && dict_manager_->lookupExact(promoted_surface, core::PartOfSpeech::Noun) != nullptr;
      const bool has_competing_exact_predicate =
          dict_manager_ != nullptr && hasExactPartOfSpeech(*dict_manager_, promoted_surface, kPredicateMask);
      const auto* exact_verb =
          dict_manager_ != nullptr ? dict_manager_->lookupExact(promoted_surface, core::PartOfSpeech::Verb) : nullptr;
      const bool has_exact_conditional_verb =
          exact_verb != nullptr && exact_verb->extended_pos == core::ExtendedPOS::VerbKateikei;
      // A direct copula cannot select a verb or auxiliary.  At clause start,
      // therefore, a two-mora conditional verb homograph before an inflected
      // copula is positive evidence for the otherwise unknown nominal reading
      // (いえ|だっ|た), not a reason to suppress it. Other exact predicates
      // such as na-adjectives and modal auxiliaries can directly take a copula
      // and must retain their dictionary reading.
      const bool copula_selected_predicate_homograph =
          right_copula && left_clause_bracket && len == 2 && !has_exact_noun && has_exact_conditional_verb;
      const auto* short_right_particle =
          dict_manager_ != nullptr && scan < codepoints.size()
              ? dict_manager_->lookupExact(extractSubstring(codepoints, scan, scan + 1), core::PartOfSpeech::Particle)
              : nullptr;
      const bool short_bos_case_particle_bracket =
          left_clause_bracket && promoted_dictionary_reading == nullptr && short_right_particle != nullptr &&
          short_right_particle->extended_pos == core::ExtendedPOS::ParticleCase && codepoints[scan] != U'と';
      const bool short_run_bracketed =
          (left_particle_bracket && (right_particle || right_copula)) || short_bos_case_particle_bracket ||
          (left_genitive_bracket && right_clause && !normalize::isExtendedParticle(codepoints[start_pos])) ||
          (right_copula && left_clause_bracket && promoted_dictionary_reading == nullptr) ||
          copula_selected_predicate_homograph;
      size_t min_len = short_run_bracketed ? 2 : 3;
      const bool short_bos_preparatory_homograph =
          start_pos == 0 && len == 2 && right_particle && promoted_dictionary_reading != nullptr &&
          promoted_dictionary_reading->extended_pos == core::ExtendedPOS::AuxAspectOku;
      const auto& promoted_inflections = inflection_.analyze(promoted_surface);
      const bool has_deverbal_noun_shape_before_genitive =
          right_genitive_after_substantive_run && grammar::isIRowCodepoint(codepoints[scan - 1]) &&
          std::any_of(promoted_inflections.begin(), promoted_inflections.end(),
                      [](const grammar::InflectionCandidate& inflection_candidate) {
                        return inflection_candidate.verb_type != grammar::VerbType::Unknown &&
                               inflection_candidate.verb_type != grammar::VerbType::IAdjective &&
                               !inflection_candidate.suffix.empty();
                      });
      // Before a genitive the run is a modifier, so an inflected predicate
      // reading of the whole span is the modifier (おおきい|の, 楽しい|の) and the
      // nominal promotion must stand down, just as it does at a clause boundary.
      const bool has_inflected_predicate_reading =
          ((right_clause && !(left_genitive_bracket && len == 2)) ||
           (right_genitive_after_substantive_run && !has_deverbal_noun_shape_before_genitive)) &&
          std::any_of(promoted_inflections.begin(), promoted_inflections.end(),
                      [](const grammar::InflectionCandidate& inflection_candidate) {
                        return !inflection_candidate.suffix.empty() &&
                               inflection_candidate.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence;
                      });
      // The colloquial contraction of the hypothetical is a predicate reading
      // of the whole run even though the contracted surface itself does not
      // analyze as a conjugation, because the conjunctive particle has fused
      // into the inflection (やりゃ = やれば). The rescue exists for runs with
      // no predicate reading, so it stands down here whatever brackets the run.
      const bool spells_contracted_hypothetical =
          spellsContractedHypothetical(codepoints, start_pos, scan, inflection_, dict_manager_);
      // The rescue may not stop part-way through a registered predicate that
      // begins inside the run. ゆえあ|って cuts the onbin stem あっ in half, and
      // what is left of the te-form then looks like the quotative particle that
      // brackets it (ゆえ|あっ|て).
      bool cuts_into_predicate = false;
      for (size_t probe = start_pos + 1; probe < scan && !cuts_into_predicate && dict_manager_ != nullptr; ++probe) {
        constexpr size_t kOverhangProbe = 2;
        const size_t probe_limit = std::min(codepoints.size(), scan + kOverhangProbe);
        for (size_t probe_end = scan + 1; probe_end <= probe_limit; ++probe_end) {
          if (hasExactPartOfSpeech(*dict_manager_, extractSubstring(codepoints, probe, probe_end),
                                   partOfSpeechMask(core::PartOfSpeech::Verb))) {
            cuts_into_predicate = true;
            break;
          }
        }
      }
      // A case particle can complete a formal noun whose first mora was
      // accidentally absorbed by this rescue candidate (くる+こと, おく+こと).
      // The right formal noun is closed-class evidence, so it wins over an
      // otherwise unverified hiragana noun hypothesis.
      bool steals_formal_noun_head = false;
      if (dict_manager_ != nullptr && right_particle && scan > start_pos) {
        const size_t formal_start = scan - 1;
        const size_t formal_probe_end = std::min(codepoints.size(), scan + static_cast<size_t>(3));
        const std::string formal_probe = extractSubstring(codepoints, formal_start, formal_probe_end);
        for (const auto& match : dict_manager_->lookup(formal_probe, 0)) {
          if (match.entry != nullptr && match.entry->pos == core::PartOfSpeech::Noun &&
              match.entry->extended_pos == core::ExtendedPOS::NounFormal && match.length > 1) {
            steals_formal_noun_head = true;
            break;
          }
        }
      }
      if ((len >= min_len || short_bos_preparatory_homograph) && (right_particle || right_clause || right_auxiliary) &&
          !crossed_verified_predicate && !cuts_into_predicate && !has_inflected_predicate_reading &&
          !spells_contracted_hypothetical && !steals_formal_noun_head && !absorbs_copula_before_sokuon_final &&
          (!hasAuxiliaryParticleDecomposition(codepoints, start_pos, scan, dict_manager_) ||
           has_deverbal_noun_shape_before_genitive || copula_selected_predicate_homograph) &&
          (!hasFunctionWordChainDecomposition(codepoints, start_pos, scan, dict_manager_) ||
           has_deverbal_noun_shape_before_genitive || copula_selected_predicate_homograph)) {
        float noun_cost = getCostForType(start_type, len) + candidate::kPostParticleNounPenalty;
        // A genitive right bracket is weaker evidence than a case particle, so it
        // only makes the whole-run candidate available; it does not select it.
        // A run bracketed by an auxiliary and then a particle stands in the same
        // nominal position as one the particle brackets directly, because the
        // auxiliary is the predicate built on the run rather than part of it
        // (りんご+だ+と). Without this the maximal run collects the bonus for a
        // bracket that belongs to the copula's clause and swallows the copula. A
        // clause-final auxiliary is not that evidence — it is indistinguishable
        // from word-final kana (ありがち+だ against あり+がち+だ).
        const bool auxiliary_bracket_before_particle =
            right_auxiliary && scan + 1 < codepoints.size() && isRightBoundaryParticle(codepoints[scan + 1]);
        const bool has_terminal_i_adjective_reading =
            std::any_of(promoted_inflections.begin(), promoted_inflections.end(),
                        [](const grammar::InflectionCandidate& inflection_candidate) {
                          return inflection_candidate.verb_type == grammar::VerbType::IAdjective &&
                                 !inflection_candidate.suffix.empty() &&
                                 inflection_candidate.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence;
                        });
        // と can close a nominal phrase, but it is also the unique one-mora
        // case-particle homograph that quotes a finished predicate. When the
        // run has a credible terminal i-adjective analysis, it may make the noun
        // rescue available but must not select it. Ordinary nominal と phrases
        // such as ともだち+と retain the selection evidence.
        const bool right_predicate_quote = right_particle && scan < codepoints.size() &&
                                           codepoints[scan] == core::hiragana::kTo && has_terminal_i_adjective_reading;
        const bool selected_nominal =
            (right_particle || auxiliary_bracket_before_particle) && !right_genitive_after_substantive_run &&
            !right_predicate_quote &&
            (left_determiner_bracket || left_clause_bracket || (start_pos > 0 && codepoints[start_pos - 1] == U'の'));
        // This is an unknown-noun rescue path.  Keep the homographic noun
        // candidate when an exact lexical reading exists, but do not give it
        // the rescue bonus that would erase the dictionary POS (きれい, しかれ,
        // かしら).  Grammatical right context can then select either reading.
        const auto* exact_dictionary_reading = promoted_dictionary_reading;
        const bool quoted_final_particle =
            exact_dictionary_reading != nullptr &&
            exact_dictionary_reading->extended_pos == core::ExtendedPOS::ParticleFinal && scan < codepoints.size() &&
            grammar::isSingleHiragana(extractSubstring(codepoints, scan, scan + 1), U'と');
        const bool exact_reading_owns_context =
            exact_dictionary_reading != nullptr &&
            (exact_dictionary_reading->pos != core::PartOfSpeech::Particle || quoted_final_particle) &&
            !(has_exact_noun && has_competing_exact_predicate) &&
            !(right_particle && exact_dictionary_reading->pos == core::PartOfSpeech::Auxiliary) &&
            !copula_selected_predicate_homograph;
        if (selected_nominal && !exact_reading_owns_context) {
          noun_cost += scorer::kBonusDoubleVeryStrong;
        }
        // A substantive hiragana run at a clause boundary or immediately
        // before genitive の is a complete nominal head when no lexical or
        // inflected predicate analysis owns the same surface (りんご、
        // たなばたの夜). This is weaker than a two-sided case-particle frame,
        // but it must still outrank the fallback Other-token alternative.
        const bool closes_unverified_nominal_head = (right_clause || right_genitive_after_substantive_run) &&
                                                    !exact_reading_owns_context && !has_inflected_predicate_reading;
        if (closes_unverified_nominal_head) {
          noun_cost += scorer::scale::kStrongBonus;
        }
        if (right_genitive_after_internal_particle) {
          noun_cost += scorer::scale::kStrongBonus;
        }
        if (left_genitive_bracket && right_clause && len == 2) {
          noun_cost += scorer::scale::kVeryStrongBonus;
        }
        // A bound copula selects a nominal, so it is evidence for the run being
        // a noun and not only a bracket that makes the candidate available. It
        // is weaker evidence than the selecting case particle above, which comes
        // with a left bracket of its own, so the preference is correspondingly
        // small — enough to settle a run the fabricated-verb reading also covers
        // (くつ|だっ|た, where くつ is equally a godan dictionary form).
        if (right_copula && !exact_reading_owns_context && !selected_nominal) {
          noun_cost += scorer::scale::kMinorBonus;
        }
        auto noun_cand = makeCandidate(promoted_surface, start_pos, scan, core::PartOfSpeech::Noun, noun_cost,
                                       /*has_suffix=*/true, CandidateOrigin::BracketedNoun);
        noun_cand.bracketed_noun_rescue = !copula_selected_predicate_homograph;
        noun_cand.requires_left_content_edge = left_particle_bracket;
        noun_cand.requires_left_attributive_edge =
            left_attributive_bracket && !left_particle_bracket && !left_determiner_bracket && !left_clause_bracket;
#ifdef SUZUME_DEBUG_INFO
        noun_cand.pattern = "bracketed_hira_noun";
#endif
        candidates.push_back(noun_cand);
      }
    };
    emit_promoted_run(scan);
    // The run that stops in front of a trailing auxiliary is offered beside the
    // maximal one, so the copula after an unregistered hiragana noun has
    // something to attach to (りんご|だ|と instead of りんごだ|と, which the scan
    // cannot reach because it only breaks at particle-shaped boundaries). The
    // auxiliary has to carry its own continuation to count: a clause-final one is
    // indistinguishable from word-final kana (たたずむ, まばたき, ありがち), while
    // anything bound behind it shows the auxiliary heading its own predicate.
    // The continuation is a boundary particle or a further auxiliary, because an
    // inflected auxiliary selects the next one and the pair is then the whole
    // predicate (りんご + だっ + た). Requiring the auxiliary to end where the run
    // ends would miss exactly that case, since the scan stops at its length limit
    // in the middle of the auxiliary. Offering both runs rather than moving the
    // break is what keeps a noun that merely spans those kana intact (からだ,
    // たなばた).
    // Every such position is offered rather than only the first: a one-mora
    // auxiliary can also sit word-internally in front of the real break (みか|ん|
    // だ|と against みかん|だ|と), and stopping there would hide the run the
    // copula actually brackets.
    for (size_t trimmed = start_pos + 1; trimmed < scan; ++trimmed) {
      if (boundAuxiliaryAt(codepoints, trimmed, dict_manager_, left_particle_bracket).length > 0) {
        emit_promoted_run(trimmed);
      }
    }
  }
}

}  // namespace suzume::analysis
