/**
 * @file unknown_bracketed_noun.cpp
 * @brief Bracketed hiragana noun rescue for unknown words
 *
 * A hiragana run bracketed by particles, the clause edges, a copula, a suffix
 * or a kanji head reads as a content noun even when the same-type scan cut it
 * short. This phase scans for such runs independently and offers additive
 * noun candidates for them.
 */

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

#include "analysis/dictionary_probe.h"
#include "analysis/scorer_constants.h"
#include "analysis/unknown.h"
#include "analysis/unknown_same_type_internal.h"
#include "candidate_constants.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "tokenizer_utils.h"
#include "verb_candidates.h"
#include "verb_candidates_dictionary_probes.h"

namespace suzume::analysis {

namespace {

using same_type_detail::BoundAuxiliary;
using same_type_detail::boundAuxiliaryAt;
using same_type_detail::isInternalParticleChar;
using same_type_detail::isNonWordType;
using same_type_detail::isRightBoundaryParticle;
using same_type_detail::opensOnParticleBeforePronoun;

// Particle that can immediately PRECEDE a content noun (私は…, 本を…, 犬が…). Used as
// the left bracket of a post-particle noun promotion.
bool isLeftBoundaryParticle(char32_t code_point) {
  return normalize::isCommonParticle(code_point) || code_point == U'で' || code_point == U'と' || code_point == U'も';
}

// True when [start_pos, end_pos) is itself a listed content word. Function
// words are excluded: a run that has so far spelled only a particle or an
// auxiliary has not ended a word, which is exactly the position where a
// following particle char is still word-internal (と in ともだち). So is a
// one-mora entry, which is as often a word's first mora (ふ of ふとん, the
// classical terminal ふ).
bool closesContentWord(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                       const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || end_pos < start_pos + 2) {
    return false;
  }
  const auto* entry = lookupEntryInRange(*dict_manager, codepoints, start_pos, end_pos);
  if (entry == nullptr) {
    return false;
  }
  return entry->pos != core::PartOfSpeech::Particle && entry->pos != core::PartOfSpeech::Auxiliary;
}

// The presumptive attaches to an irrealis cell and to nothing else, so a run
// that closes on one is a predicate however ordinary its kana look (だろ+う,
// でしょ+う). Burying it in an unregistered noun hides the inflection boundary
// the rest of the analysis depends on.
//
// The host must span more than one kana. A single kana that happens to spell a
// classical auxiliary carries no evidence about the run — く+つ decomposes that
// way only by accident.
bool spansPresumptiveAuxiliary(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                               const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || end_pos < start_pos + 3) {
    return false;
  }
  for (size_t host_end = start_pos + 2; host_end < end_pos; ++host_end) {
    if (lookupEntryInRange(*dict_manager, codepoints, start_pos, host_end, core::PartOfSpeech::Auxiliary) == nullptr) {
      continue;
    }
    const auto* presumptive =
        lookupEntryInRange(*dict_manager, codepoints, host_end, end_pos, core::PartOfSpeech::Auxiliary);
    if (presumptive != nullptr && (presumptive->extended_pos == core::ExtendedPOS::AuxVolitional ||
                                   presumptive->extended_pos == core::ExtendedPOS::AuxClassicalConjectureTerminal)) {
      return true;
    }
  }
  return false;
}

// Phonologically impossible hiragana word starts: small kana (拗音・促音), the
// moraic nasal ん, and the case particles を/が which never begin a native word.
bool isImpossibleHiraganaStart(char32_t code_point) {
  // Small kana (拗音・促音) share the single kana:: source of truth; ん and the case
  // particles を/が never begin a native hiragana word. Callers gate on hiragana,
  // so the katakana half of isSmallKanaCodepoint is never reached here.
  return kana::isSmallKanaCodepoint(code_point) || code_point == U'ん' || code_point == U'を' || code_point == U'が';
}

// Whether the one-mora particle at @p pos closes a short noun at the clause
// start: a case particle other than the quotative と and genitive の, or the
// topic は, whose word-final use is the particle alone (そら|は, さき|に).
bool closesShortNounAt(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                       size_t pos) {
  if (dict_manager == nullptr || pos >= codepoints.size() || codepoints[pos] == U'と' || codepoints[pos] == U'の') {
    return false;
  }
  const auto* particle = lookupEntryInRange(*dict_manager, codepoints, pos, pos + 1, core::PartOfSpeech::Particle);
  return particle != nullptr &&
         (particle->extended_pos == core::ExtendedPOS::ParticleCase ||
          (particle->extended_pos == core::ExtendedPOS::ParticleTopic && codepoints[pos] == U'は'));
}

// Inputs shared by every phase of the rescue at one run start.
struct BracketedNounContext {
  const std::vector<char32_t>& codepoints;
  const std::vector<normalize::CharType>& char_types;
  size_t start_pos;
  const dictionary::DictionaryManager* dict_manager;
  const grammar::Inflection& inflection;
  const VerbCandidateOptions& verb_options;
};

// How the run is bracketed on its left and how far the scan carried it; fixed
// for every run end the rescue offers.
struct BracketedScan {
  bool left_enumerating_bracket{false};
  bool left_particle_bracket{false};
  bool left_genitive_bracket{false};
  bool left_determiner_bracket{false};
  bool left_clause_bracket{false};
  bool left_attributive_bracket{false};
  bool left_te_bracket{false};
  size_t scan{0};
  size_t internal_particles{0};
  bool crossed_verified_predicate{false};
};

// The run [start_pos, scan) one emission weighs: what brackets it on the right
// and which readings its kana already have.
struct PromotedRun {
  size_t scan{0};
  size_t len{0};
  bool right_genitive_after_internal_particle{false};
  bool right_genitive_after_substantive_run{false};
  bool right_particle{false};
  bool right_sokuon_final_particle{false};
  bool right_clause{false};
  bool right_auxiliary{false};
  bool right_adjective_word{false};
  bool right_kanji_word{false};
  bool right_suffix{false};
  bool right_copula{false};
  std::string promoted_surface;
  const dictionary::DictionaryEntry* promoted_dictionary_reading{nullptr};
  const dictionary::DictionaryEntry* short_right_particle{nullptr};
  const std::vector<grammar::InflectionCandidate>* promoted_inflections{nullptr};
  bool absorbs_copula_before_sokuon_final{false};
  bool has_exact_noun{false};
  bool has_competing_exact_predicate{false};
  bool copula_selected_predicate_homograph{false};
  bool right_short_genitive{false};
  bool unread_short_run_bracketed{false};
  size_t min_len{0};
  bool short_bos_preparatory_homograph{false};
  bool has_deverbal_noun_shape_before_genitive{false};
  bool has_inflected_predicate_reading{false};
};

// A multi-char L1 particle (ながら, まで, から, だけ, …) beginning at a position is a
// real right boundary: terminate the run there rather than swallowing its head into
// the noun (…およぎ|ながら, never およぎな|がら where ながら's が is mistaken for a bracket).
// A coordinating conjunction brackets the run the same way (りんご|または|みかん);
// without it the run runs on past the conjunction's head and the whole
// all-hiragana coordination shatters into closed-class fragments. It needs one
// more mora than a particle because the two-mora conjunctions share their kana
// with word-internal sequences (あまた, したがって).
bool multiCharFunctionWordAt(const BracketedNounContext& ctx, size_t pos) {
  const auto& codepoints = ctx.codepoints;
  if (ctx.dict_manager == nullptr || pos >= codepoints.size()) {
    return false;
  }
  size_t win_end = pos + 4 < codepoints.size() ? pos + 4 : codepoints.size();
  for (const auto& res : lookupResultsInRange(*ctx.dict_manager, codepoints, pos, win_end)) {
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
}

bool liesInsideFormalNounNegativePredicate(const BracketedNounContext& ctx, size_t pos) {
  const auto& codepoints = ctx.codepoints;
  const auto* dict_manager = ctx.dict_manager;
  if (dict_manager == nullptr) {
    return false;
  }
  for (size_t predicate_start = ctx.start_pos + 1; predicate_start < pos; ++predicate_start) {
    const auto* noun =
        lookupEntryInRange(*dict_manager, codepoints, ctx.start_pos, predicate_start, core::PartOfSpeech::Noun);
    if (noun == nullptr || noun->extended_pos != core::ExtendedPOS::NounFormal) {
      continue;
    }
    const auto predicates = analysis::generateHiraganaVerbCandidates(codepoints, predicate_start, ctx.char_types,
                                                                     ctx.inflection, dict_manager, ctx.verb_options);
    for (const auto& predicate : predicates) {
      if (predicate.extended_pos != core::ExtendedPOS::VerbMizenkei || predicate.end <= pos ||
          predicate.end >= codepoints.size()) {
        continue;
      }
      const size_t probe_end = std::min(codepoints.size(), predicate.end + static_cast<size_t>(2));
      for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, predicate.end, probe_end)) {
        if (match.entry != nullptr && (match.entry->extended_pos == core::ExtendedPOS::AuxNegativeNu ||
                                       match.entry->extended_pos == core::ExtendedPOS::AuxNegativeNai)) {
          return true;
        }
      }
    }
    const size_t auxiliary_limit = std::min(codepoints.size(), pos + static_cast<size_t>(5));
    for (size_t auxiliary_start = pos + 1; auxiliary_start < auxiliary_limit; ++auxiliary_start) {
      for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, auxiliary_start, auxiliary_limit)) {
        if (match.entry == nullptr || (match.entry->extended_pos != core::ExtendedPOS::AuxNegativeNu &&
                                       match.entry->extended_pos != core::ExtendedPOS::AuxNegativeNai)) {
          continue;
        }
        const std::string full_predicate =
            extractSubstring(codepoints, predicate_start, auxiliary_start + match.length);
        for (const auto& analysis : ctx.inflection.analyze(full_predicate)) {
          if (analysis.verb_type != grammar::VerbType::Unknown && analysis.verb_type != grammar::VerbType::IAdjective &&
              !analysis.morphemes.empty() &&
              analysis.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

// Length of the longest registered suffix of two or more kana that opens at
// @p pos, or 0. A suffix is bound leftward to a nominal host.
size_t suffixLengthAt(const BracketedNounContext& ctx, size_t pos) {
  constexpr size_t kSuffixProbe = 3;
  return longestDictionaryEntryLengthFrom(ctx.dict_manager, ctx.codepoints, pos, 2, kSuffixProbe,
                                          core::PartOfSpeech::Suffix, nullptr);
}

// Length of the longest registered pronoun of two or more kana that opens
// at @p pos, or 0. A pronoun stands on its own and never closes a noun.
size_t pronounLengthAt(const BracketedNounContext& ctx, size_t pos) {
  constexpr size_t kPronounProbe = 4;
  return longestDictionaryEntryLengthFrom(ctx.dict_manager, ctx.codepoints, pos, 2, kPronounProbe,
                                          core::PartOfSpeech::Pronoun, nullptr);
}

// Whether the connective て/で at @p pos closes a te-form: the analyzer reads
// the host in front of it plus that mora as one (嬉しく+て, 言っ+て, 読ん+で).
// A host ending on a registered one-mora particle is no continuative
// (手+で|て+ぶくろ).
bool closesTeFormAt(const BracketedNounContext& ctx, size_t pos) {
  const auto& codepoints = ctx.codepoints;
  const auto* dict_manager = ctx.dict_manager;
  constexpr size_t kHostProbe = 5;
  if (dict_manager == nullptr || pos == 0 || pos >= codepoints.size() ||
      (codepoints[pos] != U'て' && codepoints[pos] != U'で') ||
      lookupEntryInRange(*dict_manager, codepoints, pos, pos + 1, core::PartOfSpeech::Particle) == nullptr ||
      lookupEntryInRange(*dict_manager, codepoints, pos - 1, pos, core::PartOfSpeech::Particle) != nullptr) {
    return false;
  }
  const std::string connective = normalize::encodeUtf8(codepoints[pos]);
  for (size_t host_start = pos; host_start-- > 0 && pos - host_start <= kHostProbe;) {
    if (isNonWordType(ctx.char_types[host_start])) {
      break;
    }
    for (const auto& reading : ctx.inflection.analyze(extractSubstring(codepoints, host_start, pos + 1))) {
      if (reading.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence &&
          utf8::endsWith(reading.suffix, connective)) {
        return true;
      }
    }
  }
  return false;
}

// A registered i-adjective of three morae or more opening inside the kana
// starts a predicate of its own, just as a kanji run does (ねこ+かわいい,
// ごはん+うまかった); its bare stem counts before the excessive すぎ or the
// appearance そう that select it (ねこ+かわい+すぎ).
bool registeredAdjectiveOpensAt(const BracketedNounContext& ctx, size_t pos) {
  const auto& codepoints = ctx.codepoints;
  const auto* dict_manager = ctx.dict_manager;
  constexpr size_t kMinAdjectiveMorae = 3;
  constexpr size_t kMaxAdjectiveProbe = 6;
  if (dict_manager == nullptr || pos >= codepoints.size() || ctx.char_types[pos] != normalize::CharType::Hiragana) {
    return false;
  }
  for (size_t adj_end = pos + 2; adj_end <= codepoints.size() && adj_end - pos <= kMaxAdjectiveProbe; ++adj_end) {
    if (adj_end - pos >= kMinAdjectiveMorae &&
        lookupEntryInRange(*dict_manager, codepoints, pos, adj_end, core::PartOfSpeech::Adjective) != nullptr) {
      return true;
    }
    const bool stem_selected =
        adj_end + 1 < codepoints.size() && ((codepoints[adj_end] == U'す' && codepoints[adj_end + 1] == U'ぎ') ||
                                            (codepoints[adj_end] == U'そ' && codepoints[adj_end + 1] == U'う'));
    if (stem_selected &&
        verb_helpers::isAdjectiveInDictionary(dict_manager, extractSubstring(codepoints, pos, adj_end) + "い")) {
      return true;
    }
  }
  return false;
}

BracketedScan classifyLeftBrackets(const BracketedNounContext& ctx) {
  const auto& codepoints = ctx.codepoints;
  const auto& char_types = ctx.char_types;
  const size_t start_pos = ctx.start_pos;
  const auto* dict_manager = ctx.dict_manager;
  // The enumerating や after a kanji or katakana noun lists the next noun (犬や+せみ).
  // After kana it may just as well end a word (おじや), so it brackets only here.
  const bool left_enumerating_bracket = start_pos >= 2 && codepoints[start_pos - 1] == U'や' &&
                                        (char_types[start_pos - 2] == normalize::CharType::Kanji ||
                                         char_types[start_pos - 2] == normalize::CharType::Katakana);
  bool left_particle_bracket =
      (start_pos >= 1 && isLeftBoundaryParticle(codepoints[start_pos - 1])) || left_enumerating_bracket;
  const auto* left_particle =
      dict_manager != nullptr && start_pos >= 1
          ? lookupEntryInRange(*dict_manager, codepoints, start_pos - 1, start_pos, core::PartOfSpeech::Particle)
          : nullptr;
  const bool left_genitive_bracket = left_particle != nullptr &&
                                     left_particle->extended_pos == core::ExtendedPOS::ParticleNo &&
                                     codepoints[start_pos - 1] == U'の';
  bool left_determiner_bracket = false;
  if (dict_manager != nullptr) {
    const size_t lookback = std::min(start_pos, static_cast<size_t>(4));
    for (size_t length = 1; length <= lookback; ++length) {
      const auto* entry =
          lookupEntryInRange(*dict_manager, codepoints, start_pos - length, start_pos, core::PartOfSpeech::Determiner);
      if (entry != nullptr) {
        left_determiner_bracket = true;
        break;
      }
    }
  }
  bool left_clause_bracket = (start_pos == 0) || (start_pos >= 1 && isNonWordType(char_types[start_pos - 1]));
  const bool left_attributive_bracket = start_pos > 0;
  // Every position has some left bracket: the clause bracket covers the start,
  // and the attributive one any later position.
  return {left_enumerating_bracket, left_particle_bracket, left_genitive_bracket,
          left_determiner_bracket,  left_clause_bracket,   left_attributive_bracket};
}

// Extends the run to its right bracket, counting the particle-shaped kana it spans.
void scanBracketedRun(const BracketedNounContext& ctx, size_t bracketed_noun_limit, BracketedScan& bracketed) {
  const auto& codepoints = ctx.codepoints;
  const auto& char_types = ctx.char_types;
  const size_t start_pos = ctx.start_pos;
  const auto* dict_manager = ctx.dict_manager;
  bool particle_initial =
      (codepoints[start_pos] == U'は' || codepoints[start_pos] == U'に' || codepoints[start_pos] == U'へ');
  size_t max_internal = particle_initial ? 0 : 2;
  size_t internal_particles = 0;
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
    const auto* single_particle = dict_manager != nullptr ? lookupEntryInRange(*dict_manager, codepoints, scan,
                                                                               scan + 1, core::PartOfSpeech::Particle)
                                                          : nullptr;
    const bool is_genitive_particle =
        single_particle != nullptr && single_particle->extended_pos == core::ExtendedPOS::ParticleNo && curr == U'の';
    // In …のの…, the first の may be the final mora of the preceding
    // hiragana noun while the second is the genitive marker. Keep scanning
    // through that first mora so the noun candidate can claim it; the
    // particle-particle bigram remains a safety net for malformed paths.
    const bool repeats_genitive = is_genitive_particle && scan + 1 < codepoints.size() && codepoints[scan + 1] == U'の';
    if (scan - start_pos >= 3 && (isRightBoundaryParticle(curr) || (is_genitive_particle && !repeats_genitive))) {
      break;
    }
    // A multi-character particle immediately after the first mora can be
    // part of a native hiragana noun (こども).  Require a substantive
    // preceding run before treating it as an internal word boundary.
    if (scan - start_pos >= 2 && multiCharFunctionWordAt(ctx, scan)) {
      if (liesInsideFormalNounNegativePredicate(ctx, scan)) {
        crossed_verified_predicate = true;
      } else {
        break;  // stop before a multi-char particle boundary
      }
    }
    // A particle-initial run's own first mora is not internal: the run already
    // carries its cap (max_internal 0) for the morae after it (へ+や).
    if ((scan > start_pos || !particle_initial) && isInternalParticleChar(curr)) {
      // A particle char followed by a fresh (non-hiragana) word is a trailing case
      // particle (…およぎ|に|行く): stop before it so the right-bracket test sees it.
      // At the run's end, before punctuation as at the end of the text, it is
      // word-final (こども、 like こども), so keep it, capped by max_internal.
      bool word_follows = scan + 1 < codepoints.size() && char_types[scan + 1] != normalize::CharType::Hiragana &&
                          !isNonWordType(char_types[scan + 1]);
      // A particle is only word-internal where no word has ended yet. Once the
      // run so far is itself a listed content word, the particle attaches to
      // that word (ただ+で), and swallowing it invents a nominal that then
      // outscores the real adverb.
      if (word_follows || internal_particles >= max_internal ||
          closesContentWord(codepoints, start_pos, scan, dict_manager)) {
        break;
      }
      ++internal_particles;
    }
    ++scan;
  }
  bracketed.scan = scan;
  bracketed.internal_particles = internal_particles;
  bracketed.crossed_verified_predicate = crossed_verified_predicate;
}

bool endsOnPredicateTail(const BracketedNounContext& ctx, size_t run_end) {
  const auto& codepoints = ctx.codepoints;
  const size_t start_pos = ctx.start_pos;
  const auto* dict_manager = ctx.dict_manager;
  // The nominalizer ん closes an attributive predicate, so a run ending on
  // it is that predicate plus the particle, never one unregistered noun
  // (できる+ん+じゃ+ない). A registered predicate in front of it is the
  // evidence, and so is an i-adjective terminal, which no noun is before ん
  // (つらい+ん+だ); runs whose kana merely happen to spell a particle keep
  // their whole-run candidate (りんご, たなばた). A na-adjective stem is no
  // such predicate, because it takes な before ん (どうん is no どう+ん).
  if (dict_manager != nullptr && run_end > start_pos + 1 && codepoints[run_end - 1] == U'ん' &&
      (hasExactPartOfSpeech(*dict_manager, codepoints, start_pos, run_end - 1,
                            partOfSpeechMask(core::PartOfSpeech::Verb)) ||
       hasExactAdjectiveOtherThanNaStem(*dict_manager, extractSubstring(codepoints, start_pos, run_end - 1)) ||
       verb_helpers::readsAsIAdjectiveTerminal(extractSubstring(codepoints, start_pos, run_end - 1), ctx.inflection))) {
    return true;
  }
  // No noun ends on a sokuon: a run closing on one carries the emphatic っ
  // of a final particle or a predicate (だよ+ねっ, つらい+もんねっ).
  if (run_end > start_pos && codepoints[run_end - 1] == U'っ') {
    return true;
  }
  // Nor on a small vowel drawing out the vowel of the mora in front of it:
  // that lengthening is a final particle's or a predicate's (や+だ+なぁ).
  constexpr std::array<char32_t, 5> kSmallVowels = {U'ぁ', U'ぃ', U'ぅ', U'ぇ', U'ぉ'};
  if (run_end > start_pos + 1 && kana::isCodepointIn(kSmallVowels, codepoints[run_end - 1]) &&
      grammar::getVowelForChar(codepoints[run_end - 2]) == codepoints[run_end - 1] + 1) {
    return true;
  }
  // A modal chain closing on the presumptive is a predicate, not an
  // unregistered noun. At a clause opening there is no left content edge to
  // expose its boundary, so the rescue path would bury the whole predicate
  // inside a fabricated nominal (だろ|う|に|ね).
  if (spansPresumptiveAuxiliary(codepoints, start_pos, run_end, dict_manager)) {
    return true;
  }
  return false;
}

PromotedRun classifyRightBrackets(const BracketedNounContext& ctx, const BracketedScan& bracketed, size_t run_end) {
  const auto& codepoints = ctx.codepoints;
  const size_t start_pos = ctx.start_pos;
  const auto* dict_manager = ctx.dict_manager;
  PromotedRun promoted;
  promoted.len = run_end - start_pos;
  // Right bracket: a single boundary particle, a multi-char particle start, or a
  // clause boundary (sentence end / symbol).
  const size_t scan = run_end;
  promoted.right_genitive_after_internal_particle =
      scan < codepoints.size() && codepoints[scan] == U'の' && bracketed.internal_particles > 0;
  // A genitive の is normally no bracket at all, because it just as often
  // marks a boundary inside the run. Once the run reaches the substantive
  // three-mora length it does delimit the modifier, exactly as a case
  // particle does at the same length. Without this an unregistered hiragana
  // noun before の has no whole-run candidate and shatters into a chain of
  // closed-class fragments (りんごの色).
  promoted.right_genitive_after_substantive_run =
      scan < codepoints.size() && codepoints[scan] == U'の' && scan - start_pos >= 3;
  promoted.right_particle = (scan < codepoints.size() && isRightBoundaryParticle(codepoints[scan])) ||
                            multiCharFunctionWordAt(ctx, scan) || promoted.right_genitive_after_internal_particle ||
                            promoted.right_genitive_after_substantive_run;
  if (dict_manager != nullptr && scan < codepoints.size() && codepoints[scan] == U'っ') {
    const size_t particle_end = std::min(codepoints.size(), scan + static_cast<size_t>(4));
    for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, scan, particle_end)) {
      if (match.entry != nullptr && match.entry->extended_pos == core::ExtendedPOS::ParticleFinal) {
        promoted.right_sokuon_final_particle = true;
        break;
      }
    }
  }
  // An emoji closes the clause the way punctuation does (いたずら😂).
  promoted.right_clause =
      (scan == codepoints.size()) || (scan < codepoints.size() && isNonWordType(ctx.char_types[scan]));
  // An auxiliary is bound leftward, so it brackets the run in front of it just
  // as a particle does. It does not select the run the way a case particle
  // does, so it only makes the candidate available.
  promoted.right_auxiliary =
      dict_manager != nullptr && scan < codepoints.size() &&
      lookupEntryInRange(*dict_manager, codepoints, scan, scan + 1, core::PartOfSpeech::Auxiliary) != nullptr;
  // A kanji run behind the kana starts a word of its own — okurigana attaches
  // to the right of its kanji and never to the left — so the script change
  // brackets what precedes it, and the run it closes is the modifier of that
  // head (りんご+栽培, みかん+農家). What stands in a modifier position may be
  // an adverb instead, so the promotion carries the same conditions the other
  // brackets impose: a registered reading and an inflected predicate reading
  // both keep it from firing.
  promoted.right_adjective_word = registeredAdjectiveOpensAt(ctx, scan);
  promoted.right_kanji_word =
      (scan < codepoints.size() && ctx.char_types[scan] == normalize::CharType::Kanji) || promoted.right_adjective_word;
  // A registered suffix brackets its host the same way: it needs a nominal in
  // front of it (かな+さん), so it both opens the run and selects it. A listed
  // predicate is the host instead when it closes the run (こと+ある+ごと) or
  // spans it into the suffix (とどく+ん+です); one that merely starts inside
  // the run, or a one-mora stem (か+な of ない), says nothing about it.
  const size_t right_suffix_len = promoted.len >= 2 ? suffixLengthAt(ctx, scan) : 0;
  bool predicate_closes_on_suffix = false;
  for (size_t pred_start = start_pos; pred_start < scan && right_suffix_len > 0 && !predicate_closes_on_suffix;
       ++pred_start) {
    const size_t probe_end = std::min(codepoints.size(), scan + right_suffix_len);
    for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, pred_start, probe_end)) {
      if (match.entry != nullptr && match.length >= 2 &&
          (match.entry->pos == core::PartOfSpeech::Verb || match.entry->pos == core::PartOfSpeech::Adjective) &&
          (pred_start + match.length == scan || (pred_start == start_pos && pred_start + match.length > scan))) {
        predicate_closes_on_suffix = true;
        break;
      }
    }
  }
  promoted.right_suffix = right_suffix_len > 0 && !predicate_closes_on_suffix;
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
  const BoundAuxiliary right_bound = boundAuxiliaryAt(codepoints, scan, dict_manager, bracketed.left_particle_bracket);
  promoted.right_copula = right_bound.length > 0 && right_bound.is_copula;
  promoted.scan = scan;
  return promoted;
}

void readPromotedSurface(const BracketedNounContext& ctx, const BracketedScan& bracketed, PromotedRun& promoted) {
  const auto& codepoints = ctx.codepoints;
  const size_t start_pos = ctx.start_pos;
  const auto* dict_manager = ctx.dict_manager;
  const size_t scan = promoted.scan;
  const size_t len = promoted.len;
  const bool left_clause_bracket = bracketed.left_clause_bracket;
  const std::string promoted_surface = extractSubstring(codepoints, start_pos, scan);
  promoted.promoted_dictionary_reading =
      dict_manager != nullptr ? dict_manager->lookupExact(promoted_surface) : nullptr;
  const auto* absorbed_auxiliary =
      dict_manager != nullptr && scan > start_pos
          ? lookupEntryInRange(*dict_manager, codepoints, scan - 1, scan, core::PartOfSpeech::Auxiliary)
          : nullptr;
  // A sokuon-initial final particle may follow a completed nominal, but it
  // cannot license a noun candidate that has swallowed the copula
  // immediately before it. That boundary is inflectional
  // (りんご|だっ|たら), so the copula must remain available to the auxiliary
  // path instead of becoming the noun's last mora.
  promoted.absorbs_copula_before_sokuon_final = promoted.right_sokuon_final_particle && absorbed_auxiliary != nullptr &&
                                                absorbed_auxiliary->extended_pos == core::ExtendedPOS::AuxCopulaDa;
  promoted.has_exact_noun =
      dict_manager != nullptr && dict_manager->lookupExact(promoted_surface, core::PartOfSpeech::Noun) != nullptr;
  promoted.has_competing_exact_predicate =
      dict_manager != nullptr && hasExactPartOfSpeech(*dict_manager, promoted_surface, kPredicateHostMask);
  const auto* exact_verb =
      dict_manager != nullptr ? dict_manager->lookupExact(promoted_surface, core::PartOfSpeech::Verb) : nullptr;
  const bool has_exact_conditional_verb =
      exact_verb != nullptr && exact_verb->extended_pos == core::ExtendedPOS::VerbKateikei;
  // A direct copula cannot select a verb or auxiliary.  At clause start,
  // therefore, a two-mora conditional verb homograph before an inflected
  // copula is positive evidence for the otherwise unknown nominal reading
  // (いえ|だっ|た), not a reason to suppress it. Other exact predicates
  // such as na-adjectives and modal auxiliaries can directly take a copula
  // and must retain their dictionary reading.
  promoted.copula_selected_predicate_homograph = promoted.right_copula && left_clause_bracket && len == 2 &&
                                                 !promoted.has_exact_noun && has_exact_conditional_verb;
  promoted.short_right_particle =
      dict_manager != nullptr && scan < codepoints.size()
          ? lookupEntryInRange(*dict_manager, codepoints, scan, scan + 1, core::PartOfSpeech::Particle)
          : nullptr;
  // A two-mora run with no reading of its own at all — neither registered
  // nor inflected — is a noun when the clause start or a determiner opens it
  // and the clause closes it (この|へや|、), or when it opens the clause and
  // a genitive の closes it (ねこ|の|いたずら). Any reading, even a fragment
  // one, keeps the short adverbs and particles out (もう, すぐ, ため).
  // The の must not be the last stem mora of a predicate inflecting right
  // after it and running to the end of the kana (ととの+わ+ない is
  // ととのわ+ない); a reading that stops inside the kana (ねこのい+た+ずら)
  // accounts for nothing.
  const auto predicate_spans_genitive = [&]() {
    constexpr size_t kPredicateProbe = 6;
    for (size_t reading_end = scan + 2;
         reading_end <= codepoints.size() && reading_end - start_pos <= kPredicateProbe &&
         ctx.char_types[reading_end - 1] == normalize::CharType::Hiragana;
         ++reading_end) {
      const bool closes_kana =
          reading_end == codepoints.size() || ctx.char_types[reading_end] != normalize::CharType::Hiragana ||
          (dict_manager != nullptr && lookupEntryInRange(*dict_manager, codepoints, reading_end, reading_end + 1,
                                                         core::PartOfSpeech::Particle) != nullptr);
      if (!closes_kana) {
        continue;
      }
      for (const auto& reading : ctx.inflection.analyze(extractSubstring(codepoints, start_pos, reading_end))) {
        if (reading.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence &&
            normalize::utf8Length(reading.stem) == len + 1) {
          return true;
        }
      }
    }
    return false;
  };
  promoted.right_short_genitive = len == 2 && left_clause_bracket && scan < codepoints.size() &&
                                  codepoints[scan] == U'の' && !predicate_spans_genitive();
  const bool unread_short_run = len == 2 && promoted.promoted_dictionary_reading == nullptr && [&]() {
    const auto& readings = ctx.inflection.analyze(promoted_surface);
    return std::none_of(readings.begin(), readings.end(), [](const grammar::InflectionCandidate& reading) {
      return reading.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence;
    });
  }();
  // The topic は closes only a run with no reading at all (して+は is a
  // te-form plus the topic, not a noun).
  const bool short_bos_case_particle_bracket = left_clause_bracket && promoted.promoted_dictionary_reading == nullptr &&
                                               closesShortNounAt(dict_manager, codepoints, scan) &&
                                               (codepoints[scan] != U'は' || unread_short_run);
  promoted.unread_short_run_bracketed =
      unread_short_run && (((left_clause_bracket || bracketed.left_determiner_bracket) &&
                            (promoted.right_clause || promoted.right_adjective_word)) ||
                           promoted.right_short_genitive);
  const bool short_run_bracketed =
      (bracketed.left_particle_bracket && (promoted.right_particle || promoted.right_copula)) ||
      (bracketed.left_enumerating_bracket && promoted.right_clause) || short_bos_case_particle_bracket ||
      promoted.unread_short_run_bracketed ||
      (bracketed.left_genitive_bracket && promoted.right_clause &&
       !normalize::isExtendedParticle(codepoints[start_pos])) ||
      (promoted.right_copula && left_clause_bracket && promoted.promoted_dictionary_reading == nullptr) ||
      (promoted.right_suffix && promoted.promoted_dictionary_reading == nullptr) ||
      promoted.copula_selected_predicate_homograph;
  promoted.min_len = short_run_bracketed ? 2 : 3;
  promoted.short_bos_preparatory_homograph =
      start_pos == 0 && len == 2 && promoted.right_particle && promoted.promoted_dictionary_reading != nullptr &&
      promoted.promoted_dictionary_reading->extended_pos == core::ExtendedPOS::AuxAspectOku;
  promoted.promoted_inflections = &ctx.inflection.analyze(promoted_surface);
  promoted.has_deverbal_noun_shape_before_genitive =
      promoted.right_genitive_after_substantive_run && kana::isIRowCodepoint(codepoints[scan - 1]) &&
      std::any_of(promoted.promoted_inflections->begin(), promoted.promoted_inflections->end(),
                  [](const grammar::InflectionCandidate& inflection_candidate) {
                    return inflection_candidate.verb_type != grammar::VerbType::Unknown &&
                           inflection_candidate.verb_type != grammar::VerbType::IAdjective &&
                           !inflection_candidate.suffix.empty();
                  });
  // Before a genitive or a kanji head the run is a modifier, so an inflected
  // predicate reading of the whole span is the modifier (おおきい|の, 楽しい|の)
  // and the nominal promotion must stand down, just as it does at a clause
  // boundary.
  // A copula or auxiliary behind the run takes an adjective terminal as
  // readily as a noun (おいしい+です), so there only the productive -しい
  // terminal counts: its suffix is grammatical evidence of the シク class,
  // while a bare い ending is as often a noun's last mora (ぶたい+です).
  // An enumerated item is nominal, so the predicate reading of its kana does not apply.
  promoted.has_inflected_predicate_reading =
      !bracketed.left_enumerating_bracket &&
          (((promoted.right_clause && !(bracketed.left_genitive_bracket && len == 2)) || promoted.right_kanji_word ||
            (promoted.right_genitive_after_substantive_run && !promoted.has_deverbal_noun_shape_before_genitive)) &&
           std::any_of(promoted.promoted_inflections->begin(), promoted.promoted_inflections->end(),
                       [](const grammar::InflectionCandidate& inflection_candidate) {
                         return !inflection_candidate.suffix.empty() &&
                                inflection_candidate.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence;
                       })) ||
      ((promoted.right_copula || promoted.right_auxiliary) &&
       verb_helpers::isProductiveShiiAdjectiveTerminal(promoted_surface, ctx.inflection));
  promoted.promoted_surface = promoted_surface;
}

bool admitsPromotedRun(const BracketedNounContext& ctx, const BracketedScan& bracketed, const PromotedRun& promoted) {
  const auto& codepoints = ctx.codepoints;
  const size_t start_pos = ctx.start_pos;
  const auto* dict_manager = ctx.dict_manager;
  const size_t scan = promoted.scan;
  // The colloquial contraction of the hypothetical is a predicate reading
  // of the whole run even though the contracted surface itself does not
  // analyze as a conjugation, because the conjunctive particle has fused
  // into the inflection (やりゃ = やれば). The rescue exists for runs with
  // no predicate reading, so it stands down here whatever brackets the run.
  const bool spells_contracted_hypothetical =
      spellsContractedHypothetical(codepoints, start_pos, scan, ctx.inflection, dict_manager);
  // The rescue may not stop part-way through a registered predicate or
  // determiner that begins inside the run. ゆえあ|って cuts the onbin stem あっ
  // in half, and what is left of the te-form then looks like the quotative
  // particle that brackets it (ゆえ|あっ|て); なくこ|の cuts この the same way.
  constexpr PartOfSpeechMask kOverhangWordMask = partOfSpeechMask(core::PartOfSpeech::Verb) |
                                                 partOfSpeechMask(core::PartOfSpeech::Adjective) |
                                                 partOfSpeechMask(core::PartOfSpeech::Determiner);
  bool cuts_into_predicate = false;
  for (size_t probe = start_pos + 1; probe < scan && !cuts_into_predicate && dict_manager != nullptr; ++probe) {
    constexpr size_t kOverhangProbe = 2;
    const size_t probe_limit = std::min(codepoints.size(), scan + kOverhangProbe);
    for (size_t probe_end = scan + 1; probe_end <= probe_limit; ++probe_end) {
      if (hasExactPartOfSpeech(*dict_manager, codepoints, probe, probe_end, kOverhangWordMask)) {
        cuts_into_predicate = true;
        break;
      }
    }
  }
  // Nor may it open inside a registered predicate that the word in front of
  // the kana run hosts (参加+す|る+もふ cuts the light verb する in half).
  bool opens_inside_hosted_predicate = false;
  constexpr size_t kPredicateLookbehind = 3;
  for (size_t probe = start_pos > kPredicateLookbehind ? start_pos - kPredicateLookbehind : 0;
       probe < start_pos && !opens_inside_hosted_predicate && dict_manager != nullptr; ++probe) {
    if (probe == 0 || ctx.char_types[probe - 1] == normalize::CharType::Hiragana ||
        ctx.char_types[probe] != normalize::CharType::Hiragana) {
      continue;
    }
    for (size_t probe_end = start_pos + 1; probe_end <= scan; ++probe_end) {
      if (hasExactPartOfSpeech(*dict_manager, codepoints, probe, probe_end,
                               partOfSpeechMask(core::PartOfSpeech::Verb))) {
        opens_inside_hosted_predicate = true;
        break;
      }
    }
  }
  // Nor may it absorb a registered irrealis and the auxiliary that selects
  // it (あら+ん+や): that is a finished predicate, not a noun.
  bool opens_on_irrealis_chain = false;
  for (size_t stem_end = start_pos + 1; stem_end < scan && !opens_on_irrealis_chain && dict_manager != nullptr;
       ++stem_end) {
    const auto* irrealis = lookupEntryInRange(*dict_manager, codepoints, start_pos, stem_end, core::PartOfSpeech::Verb);
    const auto* auxiliary =
        lookupEntryInRange(*dict_manager, codepoints, stem_end, stem_end + 1, core::PartOfSpeech::Auxiliary);
    opens_on_irrealis_chain = irrealis != nullptr && auxiliary != nullptr &&
                              irrealis->extended_pos == core::ExtendedPOS::VerbMizenkei &&
                              (auxiliary->extended_pos == core::ExtendedPOS::AuxNegativeNu ||
                               auxiliary->extended_pos == core::ExtendedPOS::AuxVolitional);
  }
  // A case particle can complete a formal noun whose first mora was
  // accidentally absorbed by this rescue candidate (くる+こと, おく+こと).
  // The right formal noun is closed-class evidence, so it wins over an
  // otherwise unverified hiragana noun hypothesis.
  bool steals_formal_noun_head = false;
  if (dict_manager != nullptr && promoted.right_particle && scan > start_pos) {
    const size_t formal_start = scan - 1;
    const size_t formal_probe_end = std::min(codepoints.size(), scan + static_cast<size_t>(3));
    for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, formal_start, formal_probe_end)) {
      if (match.entry != nullptr && match.entry->pos == core::PartOfSpeech::Noun &&
          match.entry->extended_pos == core::ExtendedPOS::NounFormal && match.length > 1) {
        steals_formal_noun_head = true;
        break;
      }
    }
  }
  // A registered word of two morae or more followed by the copula or by
  // auxiliaries it licenses is that word's predicate (なぜ+だ, これ+です),
  // not an unregistered noun. Particles and auxiliaries are excluded as the
  // word: their kana as often begin a noun (から+だ, く+つ+だっ+た).
  bool closes_registered_word_predicate = false;
  for (size_t word_end = start_pos + 2; word_end < scan && !closes_registered_word_predicate && dict_manager != nullptr;
       ++word_end) {
    const auto* word = lookupEntryInRange(*dict_manager, codepoints, start_pos, word_end);
    if (word == nullptr || word->pos == core::PartOfSpeech::Particle || word->pos == core::PartOfSpeech::Auxiliary) {
      continue;
    }
    for (size_t aux_end = word_end + 1; aux_end <= scan && !closes_registered_word_predicate; ++aux_end) {
      const auto* auxiliary =
          lookupEntryInRange(*dict_manager, codepoints, word_end, aux_end, core::PartOfSpeech::Auxiliary);
      closes_registered_word_predicate =
          auxiliary != nullptr &&
          (auxiliary->extended_pos == core::ExtendedPOS::AuxCopulaDa ||
           auxiliary->extended_pos == core::ExtendedPOS::AuxCopulaDesu ||
           BigramTable::getCost(word->extended_pos, auxiliary->extended_pos) < bigram_cost::kNeutral) &&
          (aux_end == scan ||
           maximalSegmentCount(*dict_manager, codepoints, aux_end, scan, core::PartOfSpeech::Auxiliary) > 0);
    }
  }
  // Nor may it finish an auxiliary that opens on the particle-shaped kana in
  // front of it when what is left is particles (なん+で|す+よ+ね is
  // なん+です+よ+ね): the run is the tail of a function-word chain.
  bool finishes_auxiliary_chain = false;
  for (size_t aux_end = start_pos + 1;
       aux_end <= scan && !finishes_auxiliary_chain && dict_manager != nullptr && start_pos >= 1; ++aux_end) {
    finishes_auxiliary_chain = lookupEntryInRange(*dict_manager, codepoints, start_pos - 1, aux_end,
                                                  core::PartOfSpeech::Auxiliary) != nullptr &&
                               (aux_end == scan || maximalSegmentCount(*dict_manager, codepoints, aux_end, scan,
                                                                       core::PartOfSpeech::Particle) > 0);
  }
  // A run spelled wholly by two or more auxiliaries (い+です) is a predicate
  // tail. The classical perfect is left out, as in the function-word chain
  // check: its one-mora cells are admitted only inside their own chain and
  // would otherwise decompose ordinary nouns (に+おい).
  const bool spells_auxiliary_chain =
      dict_manager != nullptr && promoted.len >= 3 &&
      maximalSegmentCount(*dict_manager, codepoints, start_pos, scan, core::PartOfSpeech::Auxiliary,
                          core::ExtendedPOS::AuxClassicalPerfect, hasLeftHost(codepoints, start_pos)) >= 2;
  // ご before kana is the Sino-Japanese honorific on a kana verbal noun
  // (ご+あんない+します); a rescue would swallow the prefix into the noun.
  // Nor may it open on the te-form connective that a continuative right in
  // front of it selects (嬉しく+て|うれぴ, 言っ+て, 読ん+で).
  const bool opens_on_te_connective = closesTeFormAt(ctx, start_pos);
  // A laugh is an interjection, which its own generator offers (あはは).
  const size_t laugh_len = laughterLengthAt(codepoints, start_pos);
  const bool spans_laugh = laugh_len > 0 && scan <= start_pos + laugh_len;
  const bool opens_on_sino_prefix =
      grammar::isSinoHonorificPrefix(extractSubstring(codepoints, start_pos, start_pos + 1));
  // Nor may it close on, or cut into, a registered suffix after a stem of
  // its own (かな+さん, not かなさん or かなさ+ん): the suffix attaches to the
  // noun before it, which the suffix bracket below offers instead. A
  // registered pronoun there is closed the same way (わて+ここ).
  bool absorbs_trailing_suffix = false;
  for (size_t suffix_start = start_pos + 2; suffix_start < scan && !absorbs_trailing_suffix; ++suffix_start) {
    const size_t suffix_len = std::max(suffixLengthAt(ctx, suffix_start), pronounLengthAt(ctx, suffix_start));
    absorbs_trailing_suffix = suffix_len > 0 && suffix_start + suffix_len >= scan;
  }
  absorbs_trailing_suffix =
      absorbs_trailing_suffix || opensOnParticleBeforePronoun(dict_manager, codepoints, start_pos, scan);
  return (promoted.len >= promoted.min_len || promoted.short_bos_preparatory_homograph) &&
         (promoted.right_particle || promoted.right_clause || promoted.right_auxiliary || promoted.right_kanji_word ||
          promoted.right_suffix || (promoted.right_short_genitive && promoted.unread_short_run_bracketed)) &&
         !bracketed.crossed_verified_predicate && !cuts_into_predicate && !opens_inside_hosted_predicate &&
         !opens_on_irrealis_chain && !promoted.has_inflected_predicate_reading && !opens_on_sino_prefix &&
         !absorbs_trailing_suffix && !opens_on_te_connective && !spans_laugh && !closes_registered_word_predicate &&
         !finishes_auxiliary_chain && !spells_contracted_hypothetical && !steals_formal_noun_head &&
         !promoted.absorbs_copula_before_sokuon_final &&
         ((!hasAuxiliaryParticleDecomposition(codepoints, start_pos, scan, dict_manager) && !spells_auxiliary_chain) ||
          promoted.has_deverbal_noun_shape_before_genitive || promoted.copula_selected_predicate_homograph) &&
         (!hasFunctionWordChainDecomposition(codepoints, start_pos, scan, dict_manager) ||
          promoted.has_deverbal_noun_shape_before_genitive || promoted.copula_selected_predicate_homograph);
}

// A reading from the same start that runs past the bracket contains the
// bracket's kana (ひと|しきり is ひとしきり, おい|しかっ is おいしかっ, まなざ|し
// is the rescue run まなざし|に), so the bracket does not select the shorter run:
// a reduplication, a registered content word, a verified i-adjective cell, or a
// longer rescue run that a boundary particle closes.
bool longerReadingSpansBracket(const BracketedNounContext& ctx, size_t run_end, size_t longer_rescue_end) {
  if (longer_rescue_end > run_end) {
    return true;
  }
  const auto& codepoints = ctx.codepoints;
  const size_t start_pos = ctx.start_pos;
  constexpr size_t kLongerReadingProbe = 8;
  size_t kana_end = run_end;
  while (kana_end < codepoints.size() && ctx.char_types[kana_end] == normalize::CharType::Hiragana &&
         kana_end - start_pos < kLongerReadingProbe) {
    ++kana_end;
  }
  // The run repeated right after itself (でれ|でれ, はる|ばる) is a reduplicated
  // mimetic whose second half the bracket opens.
  const size_t run_len = run_end - start_pos;
  if (run_len >= 2 && run_end + run_len <= kana_end &&
      (codepoints[run_end] == codepoints[start_pos] ||
       kana::isSequentialVoicingPair(codepoints[start_pos], codepoints[run_end])) &&
      std::equal(codepoints.begin() + static_cast<std::ptrdiff_t>(start_pos + 1),
                 codepoints.begin() + static_cast<std::ptrdiff_t>(run_end),
                 codepoints.begin() + static_cast<std::ptrdiff_t>(run_end + 1))) {
    return true;
  }
  if (ctx.dict_manager != nullptr) {
    for (const auto& match : lookupResultsInRange(*ctx.dict_manager, codepoints, start_pos, kana_end)) {
      if (match.entry != nullptr && start_pos + match.length > run_end &&
          match.entry->pos != core::PartOfSpeech::Particle && match.entry->pos != core::PartOfSpeech::Auxiliary &&
          match.entry->pos != core::PartOfSpeech::Prefix) {
        return true;
      }
    }
  }
  for (size_t reading_end = run_end + 1; reading_end <= kana_end; ++reading_end) {
    for (const auto& reading : ctx.inflection.analyze(extractSubstring(codepoints, start_pos, reading_end))) {
      if (reading.verb_type == grammar::VerbType::IAdjective && !reading.suffix.empty() &&
          reading.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence) {
        return true;
      }
    }
  }
  return false;
}

void appendPromotedRunCandidates(const BracketedNounContext& ctx, const BracketedScan& bracketed,
                                 const PromotedRun& promoted, float noun_cost, size_t longer_rescue_end,
                                 std::vector<UnknownCandidate>& candidates) {
  const auto& codepoints = ctx.codepoints;
  const size_t start_pos = ctx.start_pos;
  const size_t scan = promoted.scan;
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
      promoted.right_auxiliary && scan + 1 < codepoints.size() && isRightBoundaryParticle(codepoints[scan + 1]);
  const bool has_terminal_i_adjective_reading =
      std::any_of(promoted.promoted_inflections->begin(), promoted.promoted_inflections->end(),
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
  const bool right_predicate_quote = promoted.right_particle && scan < codepoints.size() &&
                                     codepoints[scan] == core::hiragana::kTo && has_terminal_i_adjective_reading;
  // The enumerating や asks for a nominal item whatever follows it.
  const bool selected_nominal =
      (((promoted.right_particle || auxiliary_bracket_before_particle || promoted.right_suffix) &&
        !promoted.right_genitive_after_substantive_run && !right_predicate_quote &&
        (bracketed.left_determiner_bracket || bracketed.left_clause_bracket ||
         (start_pos > 0 && codepoints[start_pos - 1] == U'の'))) ||
       (bracketed.left_enumerating_bracket &&
        (promoted.right_particle || promoted.right_clause || promoted.right_copula)));
  // This is an unknown-noun rescue path.  Keep the homographic noun
  // candidate when an exact lexical reading exists, but do not give it
  // the rescue bonus that would erase the dictionary POS (きれい, しかれ,
  // かしら).  Grammatical right context can then select either reading.
  const auto* exact_dictionary_reading = promoted.promoted_dictionary_reading;
  const bool quoted_final_particle = exact_dictionary_reading != nullptr &&
                                     exact_dictionary_reading->extended_pos == core::ExtendedPOS::ParticleFinal &&
                                     scan < codepoints.size() &&
                                     grammar::isSingleHiragana(extractSubstring(codepoints, scan, scan + 1), U'と');
  // A run spelled as an i-adjective stem and the nominalizer さ/み
  // (やさし+さ) has a derivation that owns the span the way a lexical
  // reading does, so the rescue offers the noun but does not select it.
  const bool spells_adjective_nominalization =
      std::any_of(promoted.promoted_inflections->begin(), promoted.promoted_inflections->end(),
                  [](const grammar::InflectionCandidate& inflection_candidate) {
                    return inflection_candidate.verb_type == grammar::VerbType::IAdjective &&
                           (inflection_candidate.suffix == "さ" || inflection_candidate.suffix == "み") &&
                           inflection_candidate.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence;
                  });
  const bool exact_reading_owns_context =
      spells_adjective_nominalization ||
      (exact_dictionary_reading != nullptr &&
       (exact_dictionary_reading->pos != core::PartOfSpeech::Particle || quoted_final_particle) &&
       !(promoted.has_exact_noun && promoted.has_competing_exact_predicate) &&
       !(promoted.right_particle && exact_dictionary_reading->pos == core::PartOfSpeech::Auxiliary) &&
       !promoted.copula_selected_predicate_homograph);
  if (selected_nominal && !exact_reading_owns_context && !longerReadingSpansBracket(ctx, scan, longer_rescue_end)) {
    noun_cost += scorer::kBonusDoubleVeryStrong;
  }
  // A substantive hiragana run at a clause boundary, immediately before
  // genitive の, or in front of a kanji head is a complete nominal head when
  // no lexical or inflected predicate analysis owns the same surface
  // (りんご、たなばたの夜, りんご栽培). This is weaker than a two-sided
  // case-particle frame, but it must still outrank the fallback Other-token
  // alternative.
  const bool closes_unverified_nominal_head =
      (promoted.right_clause || promoted.right_genitive_after_substantive_run || promoted.right_kanji_word ||
       (promoted.right_short_genitive && promoted.unread_short_run_bracketed)) &&
      !exact_reading_owns_context && !promoted.has_inflected_predicate_reading;
  if (closes_unverified_nominal_head) {
    noun_cost += scorer::scale::kStrongBonus;
  }
  if (promoted.right_genitive_after_internal_particle) {
    noun_cost += scorer::scale::kStrongBonus;
  }
  // An adjective or auxiliary reading of the run is what follows の as the
  // predicate (色+の+ない); only a nominal head is a two-mora noun there.
  const bool exact_adjectival_predicate =
      ctx.dict_manager != nullptr && hasExactPartOfSpeech(*ctx.dict_manager, promoted.promoted_surface,
                                                          partOfSpeechMask(core::PartOfSpeech::Adjective) |
                                                              partOfSpeechMask(core::PartOfSpeech::Auxiliary));
  if (bracketed.left_genitive_bracket && promoted.right_clause && promoted.len == 2 && !exact_adjectival_predicate) {
    noun_cost += scorer::scale::kVeryStrongBonus;
  }
  // A bound copula selects a nominal, so it is evidence for the run being
  // a noun and not only a bracket that makes the candidate available. It
  // is weaker evidence than the selecting case particle above, which comes
  // with a left bracket of its own, so the preference is correspondingly
  // small — enough to settle a run the fabricated-verb reading also covers
  // (くつ|だっ|た, where くつ is equally a godan dictionary form).
  if (promoted.right_copula && !exact_reading_owns_context && !selected_nominal) {
    noun_cost += scorer::scale::kMinorBonus;
  }
  auto noun_cand = makeCandidate(promoted.promoted_surface, start_pos, scan, core::PartOfSpeech::Noun, noun_cost,
                                 /*has_suffix=*/true, CandidateOrigin::BracketedNoun);
  noun_cand.bracketed_noun_rescue = !promoted.copula_selected_predicate_homograph;
  // A particle-shaped last mora of a determiner (その, この) is no particle,
  // so a run the determiner opens and the clause end or a case or topic
  // particle closes needs no content word in front of it (その+へや,
  // この+へや+で). Any other right bracket keeps the requirement
  // (その+もの|ず+ば+り is そのもの+ずばり).
  const bool right_case_or_topic = promoted.short_right_particle != nullptr &&
                                   (promoted.short_right_particle->extended_pos == core::ExtendedPOS::ParticleCase ||
                                    promoted.short_right_particle->extended_pos == core::ExtendedPOS::ParticleTopic);
  noun_cand.requires_left_content_edge =
      bracketed.left_particle_bracket && !bracketed.left_te_bracket &&
      !(bracketed.left_determiner_bracket && (promoted.right_clause || right_case_or_topic));
  noun_cand.requires_left_attributive_edge = bracketed.left_attributive_bracket && !bracketed.left_particle_bracket &&
                                             !bracketed.left_determiner_bracket && !bracketed.left_clause_bracket &&
                                             !bracketed.left_te_bracket;
#ifdef SUZUME_DEBUG_INFO
  noun_cand.pattern = "bracketed_hira_noun";
#endif
  candidates.push_back(noun_cand);
  // The beautifying お is productive on native nouns (お+ふろ, お+さんぽ),
  // so the noun behind it is offered with the same evidence. A
  // lexicalized お-word whose remainder means nothing alone (おなか,
  // おふくろ) carries a registered reading that owns the whole run. A run
  // ending on an i-row or e-row mora may be a deverbal continuative whose
  // first mora belongs to the verb stem (およぎ, おどり, おばけ), so the
  // prefix is not split off there.
  const char32_t last_mora = codepoints[scan - 1];
  const bool opens_on_native_prefix = codepoints[start_pos] == U'お' && promoted.len >= 3 &&
                                      promoted.promoted_dictionary_reading == nullptr &&
                                      !kana::isIRowCodepoint(last_mora) && !kana::isERowCodepoint(last_mora);
  if (opens_on_native_prefix) {
    auto host_cand = makeCandidate(codepoints, start_pos + 1, scan, core::PartOfSpeech::Noun, noun_cost,
                                   /*has_suffix=*/true, CandidateOrigin::BracketedNoun);
#ifdef SUZUME_DEBUG_INFO
    host_cand.pattern = "prefixed_hira_noun";
#endif
    candidates.push_back(host_cand);
  }
}

}  // namespace

void UnknownWordGenerator::appendBracketedHiraganaNounCandidates(const std::vector<char32_t>& codepoints,
                                                                 const std::vector<normalize::CharType>& char_types,
                                                                 const SameTypeRun& run,
                                                                 std::vector<UnknownCandidate>& candidates) const {
  const size_t start_pos = run.start_pos;
  const normalize::CharType start_type = run.start_type;
  const BracketedNounContext ctx{codepoints,    char_types,  start_pos,
                                 dict_manager_, inflection_, options_.verb_candidate_options};

  // Bracketed hiragana noun promotion. A short hiragana run genuinely bracketed by
  // particles (私は|たばこ|を, 彼は|ともだち|と) reads as a content noun, but the
  // same-type scan above truncates at the first internal particle character and a
  // particle-initial run (にんじん) is only ever emitted as a penalized particle-noun,
  // so the correct whole-run candidate never reaches the lattice. This dedicated
  // scan is independent of that truncation and emits an ADDITIVE Noun candidate; the
  // Other/particle candidates remain and any real dictionary/verb/adverb reading of
  // the span still outranks it, so it wins only when nothing better spans the
  // bracket and never shatters the run. Left bracket: a boundary particle after a
  // non-hiragana content word (私は…), or a clause boundary — sentence start / a
  // preceding symbol. Right bracket: a boundary particle. の is not a boundary
  // particle (genitive marks a compound boundary).
  BracketedScan bracketed = classifyLeftBrackets(ctx);
  if (start_type == normalize::CharType::Hiragana && !isImpossibleHiraganaStart(codepoints[start_pos])) {
    constexpr size_t kDefaultBracketedNounLength = 4;
    constexpr size_t kLongDeverbalNounLength = 5;
    const bool long_deverbal_object_shape =
        getMaxLength(start_type) >= kLongDeverbalNounLength &&
        start_pos + kLongDeverbalNounLength < codepoints.size() &&
        std::all_of(char_types.begin() + static_cast<std::ptrdiff_t>(start_pos),
                    char_types.begin() + static_cast<std::ptrdiff_t>(start_pos + kLongDeverbalNounLength),
                    [](normalize::CharType type) { return type == normalize::CharType::Hiragana; }) &&
        kana::isERowCodepoint(codepoints[start_pos + kLongDeverbalNounLength - 1]) &&
        codepoints[start_pos + kLongDeverbalNounLength] == U'を';
    const size_t bracketed_noun_limit =
        long_deverbal_object_shape ? kLongDeverbalNounLength : kDefaultBracketedNounLength;
    scanBracketedRun(ctx, bracketed_noun_limit, bracketed);
    // A te-form on the left closes its clause, so the run after it needs neither
    // a nominal nor an attributive in front of the connective (嬉しく+て+うれぴ).
    bracketed.left_te_bracket = start_pos > 0 && closesTeFormAt(ctx, start_pos - 1);
    // End of the maximal run when a boundary particle closes it; the shorter
    // runs offered below lie inside it.
    size_t longer_rescue_end = 0;
    auto emit_promoted_run = [&](size_t run_end) {
      if (endsOnPredicateTail(ctx, run_end)) {
        return false;
      }
      PromotedRun promoted = classifyRightBrackets(ctx, bracketed, run_end);
      readPromotedSurface(ctx, bracketed, promoted);
      if (!admitsPromotedRun(ctx, bracketed, promoted)) {
        return false;
      }
      float noun_cost = getCostForType(start_type, promoted.len) + candidate::kPostParticleNounPenalty;
      appendPromotedRunCandidates(ctx, bracketed, promoted, noun_cost, longer_rescue_end, candidates);
      return true;
    };
    const size_t scan = bracketed.scan;
    if (emit_promoted_run(scan) && scan < codepoints.size() && isRightBoundaryParticle(codepoints[scan])) {
      longer_rescue_end = scan;
    }
    // At the clause start a case particle or the topic は inside the scanned
    // run may equally close a short noun (そら|は|いつも, ねこ|が), so the run
    // that stops there is offered beside the maximal one and scoring weighs
    // them. Word-final は is read as the particle alone; も, か and the other
    // focus particles end native nouns as often as not (こども, くも).
    for (size_t particle_pos = start_pos + 2; bracketed.left_clause_bracket && particle_pos < scan; ++particle_pos) {
      if (closesShortNounAt(dict_manager_, codepoints, particle_pos)) {
        emit_promoted_run(particle_pos);
      }
    }
    // A registered adjective opening inside the scanned run closes the noun in
    // front of it, so that shorter run is offered too (ねこ|かわいすぎ).
    for (size_t adjective_pos = start_pos + 2; adjective_pos < scan; ++adjective_pos) {
      if (registeredAdjectiveOpensAt(ctx, adjective_pos)) {
        emit_promoted_run(adjective_pos);
        break;
      }
    }
    // The scan runs on through a genitive inside its first three morae, so the
    // two-mora run it closes at the clause start is offered on its own.
    constexpr size_t kShortRunLength = 2;
    if (bracketed.left_clause_bracket && start_pos + kShortRunLength < scan &&
        codepoints[start_pos + kShortRunLength] == U'の') {
      emit_promoted_run(start_pos + kShortRunLength);
    }
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
    // A non-copula auxiliary whose only continuation is the particle that
    // closes the maximal run is that run's last mora (まなざ|し|に is
    // まなざし|に): it selects a predicate cell, not the particle.
    for (size_t trimmed = start_pos + 1; trimmed < scan; ++trimmed) {
      const BoundAuxiliary bound = boundAuxiliaryAt(codepoints, trimmed, dict_manager_,
                                                    bracketed.left_particle_bracket || bracketed.left_clause_bracket);
      const bool auxiliary_is_last_mora =
          bound.length > 0 && !bound.is_copula && trimmed + bound.length == longer_rescue_end;
      if ((bound.length > 0 && !auxiliary_is_last_mora) ||
          (trimmed >= start_pos + 2 && (suffixLengthAt(ctx, trimmed) > 0 || pronounLengthAt(ctx, trimmed) > 0))) {
        emit_promoted_run(trimmed);
      }
    }
  }
}

}  // namespace suzume::analysis
