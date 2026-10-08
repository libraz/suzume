/**
 * @file unknown_bracketed_noun_scan.cpp
 * @brief Scan and classify bracketed hiragana noun rescue runs.
 */

#include <algorithm>
#include <array>
#include <string>

#include "analysis/dictionary_probe.h"
#include "analysis/scorer_constants.h"
#include "analysis/unknown_bracketed_noun_internal.h"
#include "analysis/unknown_same_type_internal.h"
#include "candidate_constants.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "tokenizer_utils.h"
#include "verb_candidates.h"
#include "verb_candidates_dictionary_probes.h"

namespace suzume {
namespace analysis {
namespace bracketed_noun_detail {

using same_type_detail::BoundAuxiliary;
using same_type_detail::boundAuxiliaryAt;
using same_type_detail::isInternalParticleChar;
using same_type_detail::isNonWordType;
using same_type_detail::isRightBoundaryParticle;
using same_type_detail::opensOnParticleBeforePronoun;

namespace {
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
}  // namespace

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

namespace {
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
}  // namespace

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

// Whether the analyzer reads a whole kana span as a verb's terminal form.
bool readsAsVerbTerminal(const grammar::Inflection& inflection, const std::string& surface) {
  const auto& analyses = inflection.analyze(surface);
  return std::any_of(analyses.begin(), analyses.end(), [&](const grammar::InflectionCandidate& analysis) {
    return analysis.verb_type != grammar::VerbType::Unknown && analysis.verb_type != grammar::VerbType::IAdjective &&
           analysis.base_form == surface && analysis.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence;
  });
}

// Whether codepoints[start, end) is a chain of registered auxiliaries each of
// which the word before it licenses, starting from @p previous. Count_ opens
// the chain with no word in front of it.
bool continuesAuxiliaryChain(const dictionary::DictionaryManager& dict_manager, const std::vector<char32_t>& codepoints,
                             core::ExtendedPOS previous, size_t start, size_t end) {
  if (start == end) {
    return true;
  }
  for (size_t next_end = start + 1; next_end <= end; ++next_end) {
    const auto* auxiliary =
        lookupEntryInRange(dict_manager, codepoints, start, next_end, core::PartOfSpeech::Auxiliary);
    if (auxiliary != nullptr &&
        (previous == core::ExtendedPOS::Count_ ||
         BigramTable::getCost(previous, auxiliary->extended_pos) < scorer::scale::kStrong) &&
        continuesAuxiliaryChain(dict_manager, codepoints, auxiliary->extended_pos, next_end, end)) {
      return true;
    }
  }
  return false;
}

bool endsOnPredicateTail(const BracketedNounContext& ctx, size_t run_end) {
  const auto& codepoints = ctx.codepoints;
  const size_t start_pos = ctx.start_pos;
  const auto* dict_manager = ctx.dict_manager;
  // The nominalizer ん, or the の it contracts, closes an attributive
  // predicate, so a run ending on it is that predicate plus the particle,
  // never one unregistered noun (できる+ん+じゃ+ない, みる+の+が). A registered
  // predicate in front of it is the evidence, and so is an i-adjective
  // terminal, which no noun is before ん (つらい+ん+だ); runs whose kana merely
  // happen to spell a particle keep their whole-run candidate (りんご,
  // たなばた). A na-adjective stem is no such predicate, because it takes な
  // before ん (どうん is no どう+ん). Before の a verb terminal of two morae or
  // more is evidence as well; ん is left out, as it also ends mimetics (ぐうん).
  if (dict_manager != nullptr && run_end > start_pos + 1 &&
      (codepoints[run_end - 1] == U'ん' || codepoints[run_end - 1] == U'の')) {
    const std::string predicate = extractSubstring(codepoints, start_pos, run_end - 1);
    if (hasExactPartOfSpeech(*dict_manager, codepoints, start_pos, run_end - 1,
                             partOfSpeechMask(core::PartOfSpeech::Verb)) ||
        verb_helpers::isIAdjectiveInDictionary(dict_manager, predicate) ||
        verb_helpers::readsAsIAdjectiveTerminal(predicate, ctx.inflection) ||
        (run_end >= start_pos + 3 && codepoints[run_end - 1] == U'の' &&
         readsAsVerbTerminal(ctx.inflection, predicate))) {
      return true;
    }
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
    if (lookupResultsHaveExtendedPOS(lookupResultsInRange(*dict_manager, codepoints, scan, particle_end),
                                     core::ExtendedPOS::ParticleFinal)) {
      promoted.right_sokuon_final_particle = true;
    }
  }
  // An emoji closes the clause the way punctuation does (いたずら😂).
  promoted.right_clause =
      (scan == codepoints.size()) || (scan < codepoints.size() && isNonWordType(ctx.char_types[scan]));
  // An auxiliary is bound leftward, so it brackets the run in front of it just
  // as a particle does. It does not select the run the way a case particle
  // does, so it only makes the candidate available. One a noun cannot host
  // brackets no noun (せんしゅ+う of the volitional).
  const auto* right_auxiliary_entry =
      dict_manager != nullptr && scan < codepoints.size()
          ? lookupEntryInRange(*dict_manager, codepoints, scan, scan + 1, core::PartOfSpeech::Auxiliary)
          : nullptr;
  promoted.right_auxiliary =
      right_auxiliary_entry != nullptr &&
      BigramTable::getCost(core::ExtendedPOS::Noun, right_auxiliary_entry->extended_pos) < scorer::scale::kStrong;
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
  // The particle-shaped first mora of a longer copula is the copula's (で of です).
  if (dict_manager != nullptr && scan < codepoints.size() && isRightBoundaryParticle(codepoints[scan])) {
    constexpr size_t kCopulaWindow = 4;
    for (const auto& match :
         lookupResultsInRange(*dict_manager, codepoints, scan, std::min(codepoints.size(), scan + kCopulaWindow))) {
      if (match.entry != nullptr && match.length > 1 &&
          (match.entry->extended_pos == core::ExtendedPOS::AuxCopulaDa ||
           match.entry->extended_pos == core::ExtendedPOS::AuxCopulaDesu)) {
        promoted.right_particle_opens_copula = true;
        break;
      }
    }
  }
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
  // An attributive predicate opens it the same way (はしる|ひと|。); the
  // candidate is then admitted only behind an attributive lattice edge. A run
  // opening on a function word is the clause's next morpheme (おおきい+の+が).
  const bool left_attributive_only = bracketed.left_attributive_bracket && !bracketed.left_particle_bracket &&
                                     !bracketed.left_determiner_bracket && !left_clause_bracket &&
                                     !bracketed.left_te_bracket && dict_manager != nullptr &&
                                     !hasExactPartOfSpeech(*dict_manager, codepoints, start_pos, start_pos + 1,
                                                           partOfSpeechMask(core::PartOfSpeech::Particle) |
                                                               partOfSpeechMask(core::PartOfSpeech::Auxiliary));
  promoted.unread_short_run_bracketed =
      unread_short_run && (((left_clause_bracket || bracketed.left_determiner_bracket || left_attributive_only) &&
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

}  // namespace bracketed_noun_detail
}  // namespace analysis
}  // namespace suzume
