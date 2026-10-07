/**
 * @file tokenizer_dictionary_boundary_guards.cpp
 * @brief Dictionary readings that cut into a word or chain the context already establishes
 */

#include <algorithm>

#include "analysis/dictionary_probe.h"
#include "candidate_constants.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "normalize/utf8.h"
#include "tokenizer_dictionary_internal.h"
#include "verb_candidates_auxiliary_patterns.h"
#include "verb_candidates_dictionary_probes.h"

namespace suzume::analysis::tokenizer_dictionary_detail {

namespace {

// Whether a listed noun ending at @p start_pos opens a listed verb that ends at
// @p end_pos, so the span is the verb's tail rather than a word of its own.
bool splitsListedVerbAtNoun(const dictionary::DictionaryManager& dict_manager, const std::vector<char32_t>& codepoints,
                            size_t start_pos, size_t end_pos) {
  constexpr size_t kMaxNounChars = 4;
  const size_t scan_start = lookbehindStart(start_pos, kMaxNounChars);
  for (size_t host_start = scan_start; host_start < start_pos; ++host_start) {
    if (lookupEntryInRange(dict_manager, codepoints, host_start, start_pos, core::PartOfSpeech::Noun) != nullptr &&
        lookupEntryInRange(dict_manager, codepoints, host_start, end_pos, core::PartOfSpeech::Verb) != nullptr) {
      return true;
    }
  }
  return false;
}

bool canSegmentAsParticles(const dictionary::DictionaryManager& dict_manager, const std::vector<char32_t>& codepoints,
                           size_t start_pos, size_t end_pos) {
  return maximalSegmentCount(dict_manager, codepoints, start_pos, end_pos, core::PartOfSpeech::Particle) > 0;
}

// A dictionary adverb may begin at the terminal い of an already complete
// i-adjective and consume the following particle sequence (惜し+いとも).  The
// overlap is not a morpheme boundary: keep the adjective and the independently
// searchable particles.  Requiring an adjective edge that crosses the start
// and a fully particle-decomposable remainder leaves clause-initial uses of
// the same adverb untouched.
// "Complete" is decided by the mora in front of the terminal い. An i-adjective
// written with okurigana has one (惜し+い), and its stem is spelled out whether
// or not the adverb is taken. A kanji or katakana run running straight into い
// has none: that い is the adverb's own first mora (人々+い, 学生+い, テスト+い),
// so the adjective it completes exists only because the adverb was not taken,
// and it would retire the adverb wherever a nominal precedes it.
bool overlapsCompleteIAdjectiveBeforeParticles(const core::Lattice& lattice,
                                               const dictionary::DictionaryManager& dict_manager,
                                               const std::vector<char32_t>& codepoints, size_t start_pos,
                                               size_t end_pos) {
  if (start_pos == 0 || start_pos + 1 >= end_pos || codepoints[start_pos] != U'い' ||
      !kana::isHiraganaCodepoint(codepoints[start_pos - 1]) ||
      !canSegmentAsParticles(dict_manager, codepoints, start_pos + 1, end_pos)) {
    return false;
  }
  return core::anyEdgeEndingAt(lattice, start_pos + 1, [start_pos](const core::LatticeEdge& edge) {
    return edge.start < start_pos && edge.pos == core::PartOfSpeech::Adjective &&
           edge.extended_pos == core::ExtendedPOS::AdjBasic && edge.origin == core::CandidateOrigin::AdjectiveI;
  });
}

// A verified compound candidate can span a productive completive auxiliary
// boundary (食べ+ちゃい+ます). Keep that boundary only when its left and
// right contexts independently license the closed auxiliary paradigm.
bool startsClosedCompletiveContinuation(const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                        const ByteOffsets& byte_offsets, size_t start_pos) {
  if (start_pos + 1 >= byte_offsets.size()) {
    return false;
  }
  for (const auto& result : dict_manager.lookup(text, byteOffsetAt(byte_offsets, start_pos))) {
    if (result.entry == nullptr) {
      continue;
    }
    switch (result.entry->extended_pos) {
      case core::ExtendedPOS::AuxTenseTa:
      case core::ExtendedPOS::AuxNegativeNai:
      case core::ExtendedPOS::AuxTenseMasu:
      case core::ExtendedPOS::AuxDesireTai:
      case core::ExtendedPOS::AuxVolitional:
      case core::ExtendedPOS::AuxAppearanceSou:
      case core::ExtendedPOS::ParticleConj:
        return true;
      default:
        break;
    }
  }
  return false;
}

bool isLicensedCompletiveAuxiliaryBoundary(const core::Lattice& lattice,
                                           const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                           const ByteOffsets& byte_offsets, size_t candidate_start,
                                           size_t candidate_end, core::ExtendedPOS candidate_epos) {
  if (candidate_epos != core::ExtendedPOS::AuxAspectShimau) {
    return false;
  }
  const bool follows_verb_host = hasPrecedingExtendedPOS(
      lattice, candidate_start, {core::ExtendedPOS::VerbRenyokei, core::ExtendedPOS::VerbOnbinkei});
  return follows_verb_host && startsClosedCompletiveContinuation(dict_manager, text, byte_offsets, candidate_end);
}

// A dictionary-verified lexical compound owns every boundary inside its
// active inflectional span. Short dictionary verbs and closed function words
// may be accidental homographs of that interior (思い出+し, 見落+として).
// Require the explicit LemmaVerified flag: compound join candidates also carry
// FromDictionary for their generation evidence, which alone does not attest
// the complete compound lemma.
bool conflictsWithVerifiedCompoundBoundary(const core::Lattice& lattice,
                                           const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                           const ByteOffsets& byte_offsets, const std::vector<char32_t>& codepoints,
                                           size_t candidate_start, size_t candidate_end,
                                           core::PartOfSpeech candidate_pos, core::ExtendedPOS candidate_epos) {
  const bool is_grammatical_candidate =
      candidate_pos == core::PartOfSpeech::Verb || candidate_pos == core::PartOfSpeech::Particle ||
      candidate_pos == core::PartOfSpeech::Auxiliary || candidate_pos == core::PartOfSpeech::Suffix;
  if (!is_grammatical_candidate) {
    return false;
  }
  // A verified compound's inflected whole-span candidate must not hide a
  // productive verb-to-auxiliary boundary.  In particular, the irrealis is
  // the required host for negative, causative, and passive auxiliaries; the
  // auxiliary remains grammatical even when a longer compound candidate also
  // crosses the same position.
  if (candidate_pos == core::PartOfSpeech::Auxiliary &&
      hasPrecedingExtendedPOS(
          lattice, candidate_start,
          {core::ExtendedPOS::VerbMizenkei, core::ExtendedPOS::VerbRenyokei, core::ExtendedPOS::VerbOnbinkei})) {
    return false;
  }
  const size_t compound_end = verifiedCompoundEndCovering(lattice, candidate_start);
  if (compound_end != 0 && candidate_end <= compound_end &&
      !isLicensedCompletiveAuxiliaryBoundary(lattice, dict_manager, text, byte_offsets, candidate_start, candidate_end,
                                             candidate_epos)) {
    return true;
  }
  const size_t onbinkei_end = dictionarySokuonbinEndCovering(lattice, candidate_start);
  if (onbinkei_end != 0 && candidate_end <= onbinkei_end && onbinkei_end < codepoints.size() &&
      (codepoints[onbinkei_end] == U'た' || codepoints[onbinkei_end] == U'て')) {
    return true;
  }
  if (candidate_pos != core::PartOfSpeech::Particle) {
    return false;
  }
  // A structurally valid compound does not need lexical registration to
  // protect its connective boundary from a larger particle that begins in
  // its interior. Requiring the outside remainder itself to be a particle
  // keeps ordinary compound-particle uses available at real boundaries.
  const size_t structural_compound_end = compoundVerbEndCovering(lattice, candidate_start);
  if (structural_compound_end == 0 || candidate_end <= structural_compound_end) {
    return false;
  }
  return lookupEntryInRange(dict_manager, codepoints, structural_compound_end, candidate_end,
                            core::PartOfSpeech::Particle) != nullptr;
}

// An L2 noun can begin with another L2 noun by accident (は+にわ inside
// はにわ).  At sentence start the longer registered noun owns the span; after
// a completed nominal, the same first mora can instead be a productive topic
// particle and the suffix noun remains available.
bool startsInsideSentenceInitialDictionaryNoun(const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                               size_t start_pos) {
  if (start_pos == 0) {
    return false;
  }
  const auto sentence_initial = dict_manager.lookup(text, 0);
  return lookupResultsHaveLongerPartOfSpeech(sentence_initial, partOfSpeechMask(core::PartOfSpeech::Noun), start_pos);
}

bool hasPrecedingQuantityEdge(const core::Lattice& lattice, size_t end_pos) {
  return core::anyEdgeEndingAt(lattice, end_pos, [](const core::LatticeEdge& edge) {
    return edge.extended_pos == core::ExtendedPOS::NounNumber || edge.origin == core::CandidateOrigin::Counter;
  });
}

// Whether a verb continuative built on a kanji stem reaches past this position.
// Its okurigana starts on the same i-row mora two of the kana numerals do
// (思い|つつ against 思|いつつ, 読み|つつ against 読|みっつ), so a numeral opening
// there would be opening inside a word.  A stem is at most one mora shorter than
// the run it heads, so probing back that far reaches every such continuative.
bool insideKanjiVerbOkurigana(const core::Lattice& lattice, size_t start_pos) {
  constexpr size_t kStemProbeChars = 4;
  const size_t probe_start = lookbehindStart(start_pos, kStemProbeChars);
  for (size_t stem_start = probe_start; stem_start < start_pos; ++stem_start) {
    const bool spans = core::anyEdgeStartingAt(lattice, stem_start, [start_pos](const core::LatticeEdge& edge) {
      return edge.pos == core::PartOfSpeech::Verb && edge.extended_pos == core::ExtendedPOS::VerbRenyokei &&
             edge.end > start_pos;
    });
    if (spans) {
      return true;
    }
  }
  return false;
}

// Whether an edge ending at pos satisfies edge_pred while an edge ending at
// that edge's start satisfies host_pred.
template <typename EdgePred, typename HostPred>
bool followsHostedEdge(const core::Lattice& lattice, size_t pos, EdgePred edge_pred, HostPred host_pred) {
  return core::anyEdgeEndingAt(lattice, pos, [&](const core::LatticeEdge& edge) {
    return edge_pred(edge) && core::anyEdgeEndingAt(lattice, edge.start, host_pred);
  });
}

// A formal noun after the negative-quote frame (…ん+と) must not hide a
// dictionary verb irrealis plus the following negative auxiliary.  This is a
// structural ambiguity: the formal-noun edge has no predicate host there,
// while the split supplies one.  Keep ordinary formal-noun uses (結果いかんで)
// and unrelated quotative phrases available.
bool followsNegativeQuote(const core::Lattice& lattice, size_t start_pos) {
  return followsHostedEdge(
      lattice, start_pos,
      [](const core::LatticeEdge& quote) {
        // と is lexically ambiguous between a quotation and a case particle.
        // In this frame the preceding negative predicate supplies the quoted
        // clause, so either dictionary label represents the same boundary.
        return quote.extended_pos == core::ExtendedPOS::ParticleQuote ||
               (quote.extended_pos == core::ExtendedPOS::ParticleCase &&
                grammar::isSingleHiragana(quote.surface, core::hiragana::kTo));
      },
      [](const core::LatticeEdge& negative) { return negative.extended_pos == core::ExtendedPOS::AuxNegativeNu; });
}

// A nasal onbin happens to contain a competing one-mora ん entry (読ん+どく).
// It is a negative only when it has an actual irrealis host, as in
// 確認せ+ん+と.  Checking the immediate lattice predecessor keeps this guard
// structural instead of suppressing every accidental ん edge.
bool followsNegativeAuxiliary(const core::Lattice& lattice, size_t start_pos) {
  return followsHostedEdge(
      lattice, start_pos,
      [](const core::LatticeEdge& negative) { return negative.extended_pos == core::ExtendedPOS::AuxNegativeNu; },
      [](const core::LatticeEdge& host) { return host.extended_pos == core::ExtendedPOS::VerbMizenkei; });
}

// The contracted explanatory nominalizer in …てん/…でん follows a
// conjunctive te-form.  Classical negative ん instead requires a verb
// irrealis host, so retaining that homograph here can only fabricate an
// impossible analysis (読ん+で+ん+の).  Checking the preceding lattice edge
// makes this a grammatical boundary guard rather than a surface exception.
// The boundary is explanatory only when the conjunctive particle itself
// follows a predicate.  A kana inside an Ichidan host (慌て+ずに) also has a
// competing one-mora て particle edge, but its left neighbor is a noun
// fragment rather than the te-form's predicate.
bool followsConjunctiveTeDe(const core::Lattice& lattice, size_t start_pos) {
  return followsHostedEdge(
      lattice, start_pos,
      [](const core::LatticeEdge& edge) {
        return edge.extended_pos == core::ExtendedPOS::ParticleConj && grammar::isTeDeSurface(edge.surface);
      },
      [](const core::LatticeEdge& host) { return host.pos == core::PartOfSpeech::Verb; });
}

// A candidate span whose last character is the contracted negative ん competes
// with a dictionary irrealis one character shorter.  Both the formal-noun and
// the irrealis reading of the span are decided by the same evidence, so they
// ask this one question rather than each carrying its own scan.
bool hasShorterMizenkeiBeforeNegative(const std::vector<dictionary::LookupResult>& alternatives,
                                      const std::vector<char32_t>& codepoints, size_t start_pos,
                                      size_t candidate_length) {
  if (candidate_length < 2 || start_pos + candidate_length > codepoints.size() ||
      codepoints[start_pos + candidate_length - 1] != U'ん') {
    return false;
  }
  for (const auto& alternative : alternatives) {
    if (alternative.entry != nullptr && alternative.entry->extended_pos == core::ExtendedPOS::VerbMizenkei &&
        alternative.length + 1 == candidate_length) {
      return true;
    }
  }
  return false;
}

// In a negative-quote frame, a one-mora verbal edge can be the prefix of a
// longer dictionary verb that is immediately followed by the negative
// auxiliary. The longer predicate supplies the only complete grammatical
// chain, while the shorter edge would leave its remaining kana to a particle.
// Compare dictionary spans rather than surfaces so the rule applies to every
// homographic verb pair with this structure.
bool hasLongerVerbBeforeNegative(const std::vector<dictionary::LookupResult>& alternatives,
                                 const std::vector<char32_t>& codepoints, size_t start_pos, size_t candidate_length) {
  for (const auto& alternative : alternatives) {
    if (alternative.entry == nullptr || alternative.entry->pos != core::PartOfSpeech::Verb ||
        alternative.length <= candidate_length || start_pos + alternative.length >= codepoints.size()) {
      continue;
    }
    if (codepoints[start_pos + alternative.length] == U'ん') {
      return true;
    }
  }
  return false;
}

}  // namespace

// Whether a dictionary verb ends exactly at @p end_pos while starting before
// @p start_pos, i.e. the span in question is the tail of a longer headword.
bool endsDictionaryVerbSpanningBack(const dictionary::DictionaryManager& dict_manager,
                                    const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  // A headword reaching back further than this is not a contraction host.
  constexpr size_t kMaxHostChars = 4;
  const size_t scan_start = lookbehindStart(start_pos, kMaxHostChars);
  for (size_t host_start = scan_start; host_start < start_pos; ++host_start) {
    if (lookupEntryInRange(dict_manager, codepoints, host_start, end_pos, core::PartOfSpeech::Verb) != nullptr) {
      return true;
    }
  }
  return false;
}

bool crossesEstablishedBoundary(const DictionaryCandidateContext& ctx, const core::Lattice& lattice,
                                const dictionary::LookupResult& result, size_t end_pos) {
  const auto& dict_manager = ctx.dict_manager;
  const auto& inflection = ctx.inflection;
  const std::string_view text = ctx.text;
  const auto& codepoints = ctx.codepoints;
  const auto& byte_offsets = ctx.byte_offsets;
  const size_t start_pos = ctx.start_pos;
  const auto& lookup_results = ctx.lookup_results;
  const bool has_attributive_temporal_ma = ctx.has_attributive_temporal_ma;
  const bool starts_shortened_causative_passive = ctx.starts_shortened_causative_passive;
  const size_t longest_noun = ctx.longest_noun;
  // A closed kana numeral cannot open immediately after a completed
  // quantity. Repeated/distributive quantities are owned by the dedicated
  // counter candidate, while this position otherwise begins a particle or
  // predicate (一つ+と+おもう, not 一つ+とお+も+う).
  if (result.entry->extended_pos == core::ExtendedPOS::NounNumber &&
      (hasPrecedingQuantityEdge(lattice, start_pos) || insideKanjiVerbOkurigana(lattice, start_pos))) {
    return true;
  }

  // A finite predicate immediately before 間 establishes the productive
  // attributive formal-noun construction.  In that context an otherwise
  // valid lexical adverb beginning at the same position must not swallow
  // the grammatical 間 boundary.
  if (has_attributive_temporal_ma && result.entry->pos == core::PartOfSpeech::Adverb) {
    return true;
  }
  if ((result.entry->extended_pos == core::ExtendedPOS::NounFormal ||
       result.entry->extended_pos == core::ExtendedPOS::VerbMizenkei) &&
      followsNegativeQuote(lattice, start_pos) &&
      hasShorterMizenkeiBeforeNegative(lookup_results, codepoints, start_pos, result.length)) {
    return true;
  }
  if (result.entry->pos == core::PartOfSpeech::Verb && followsNegativeQuote(lattice, start_pos) &&
      hasLongerVerbBeforeNegative(lookup_results, codepoints, start_pos, result.length)) {
    return true;
  }
  // An aspect auxiliary attaches to a te-form, never directly after the
  // contracted negative.  Keep the intervening quotative particle in
  // dialectal obligation frames (…せん+と+いけ+ん).
  if (result.entry->extended_pos == core::ExtendedPOS::AuxAspectOku && followsNegativeAuxiliary(lattice, start_pos)) {
    return true;
  }
  if (result.entry->extended_pos == core::ExtendedPOS::AuxNegativeNu && followsConjunctiveTeDe(lattice, start_pos)) {
    return true;
  }

  // A one-mora verb or auxiliary homograph at the tail of the polite copula
  // is not a morpheme boundary. Keeping it would
  // split the polite copula in ですって as で+すっ+て.  Ask the dictionary
  // directly rather than relying on lattice insertion order.
  if ((result.entry->pos == core::PartOfSpeech::Verb || result.entry->pos == core::PartOfSpeech::Auxiliary) &&
      verb_helpers::startsInsideDictionaryAuxiliary(codepoints, start_pos, &dict_manager)) {
    return true;
  }

  // A regional aspect contraction does not span a word the dictionary
  // carries. 〜とる after an onbin is the ておる contraction (知っ+とる), but
  // the same two morae also close ordinary lexical verbs (のっとる, もどる),
  // and there the contraction is a coincidence of spelling that the
  // productive chain would otherwise win on connection bonuses alone.
  // A final particle that ends on a u-row mora is spelled like a verb
  // terminal in the same way (考えた+なう against 損なう, 行なう).
  if (((result.entry->extended_pos == core::ExtendedPOS::AuxAspectIru &&
        grammar::isDialectalOruContractionLemma(result.entry->lemma)) ||
       (result.entry->extended_pos == core::ExtendedPOS::ParticleFinal &&
        kana::isURowCodepoint(codepoints[end_pos - 1]))) &&
      endsDictionaryVerbSpanningBack(dict_manager, codepoints, start_pos, end_pos)) {
    return true;
  }
  // Nor does the one-mora サ変 terminal す split a listed verb into the
  // listed noun it opens with plus す (思い出す, not 思い出+す; 提出+す stays).
  if (result.length == 1 && result.entry->extended_pos == core::ExtendedPOS::VerbShuushikei &&
      result.entry->lemma == "する" && splitsListedVerbAtNoun(dict_manager, codepoints, start_pos, end_pos)) {
    return true;
  }

  // なら is the irrealis of the classical copula なり only before the
  // classical negative (静か+なら+ず). In every other context this surface
  // is the modern conditional particle, so do not let the homograph replace
  // its stable POS/lemma analysis.
  if (result.entry->extended_pos == core::ExtendedPOS::AuxClassicalNari &&
      utf8::equalsAny(result.entry->surface, {"なら"}) &&
      (end_pos >= codepoints.size() || codepoints[end_pos] != U'ず')) {
    return true;
  }

  // A one-kanji formal-noun homograph cannot claim the tail of an ongoing
  // kanji compound immediately before an adverbial particle
  // (行為+やら, 当時+やら, 室内+やら). The particle identifies the complete
  // nominal boundary, while genuine productive formal-noun uses such as
  // 年度+末 and 期間+内 remain available in their ordinary contexts.
  if (result.entry->extended_pos == core::ExtendedPOS::NounFormal && result.length == 1 && start_pos > 0 &&
      normalize::isKanjiCodepoint(codepoints[start_pos - 1]) && end_pos < codepoints.size() &&
      lookupResultsHaveExtendedPOS(dict_manager.lookup(text, byteOffsetAt(byte_offsets, end_pos)),
                                   core::ExtendedPOS::ParticleAdverbial)) {
    return true;
  }

  // A registered word is no more entitled to the material in front of the
  // nominalizer っこ than a constructed one is (で+きっ+こない for できっこない).
  if (verb_helpers::startsInsideGaMashiiSuffix(codepoints, start_pos)) {
    return true;
  }
  if (verb_helpers::crossesKkoNominalizer(codepoints, start_pos, end_pos)) {
    return true;
  }

  if (result.entry->pos == core::PartOfSpeech::Adverb &&
      overlapsCompleteIAdjectiveBeforeParticles(lattice, dict_manager, codepoints, start_pos, end_pos)) {
    return true;
  }

  // Once the surrounding lattice proves the shortened causative-passive,
  // the homographic suru mizenkei is not grammatical at this boundary.
  // Removing it also prevents a list-particle reading of やら from reaching
  // the passive through the otherwise cheap する+れる connection.
  if (starts_shortened_causative_passive && result.length == 1 &&
      result.entry->extended_pos == core::ExtendedPOS::VerbMizenkei && grammar::isSuruBaseForm(result.entry->lemma)) {
    return true;
  }

  // A context-licensed particle must not absorb the beginning of a complete
  // following adverb (裏+で+しばらく, 時+は+すでに). Restricting the guard to
  // an observed left content/predicate edge avoids kana homographs inside
  // open words such as adjectives.
  if (result.entry->pos != core::PartOfSpeech::Particle &&
      !isLicensedCompletiveAuxiliaryBoundary(lattice, dict_manager, text, byte_offsets, start_pos, end_pos,
                                             result.entry->extended_pos) &&
      joinsParticleToDictionaryAdverb(lattice, dict_manager, text, byte_offsets, start_pos, end_pos,
                                      result.entry->extended_pos)) {
    return true;
  }

  // A dictionary terminal verb must yield to a longer, structurally valid
  // i-onbin stem immediately selected by て/で. This recovers open Godan-ka/
  // Godan-ga forms such as あるい+て without registering the lexical verb.
  if (result.entry->pos == core::PartOfSpeech::Verb && end_pos + 1 < codepoints.size() &&
      codepoints[end_pos] == U'い' && (codepoints[end_pos + 1] == U'て' || codepoints[end_pos + 1] == U'で')) {
    const auto& longer_analyses = analysesInRange(inflection, codepoints, start_pos, end_pos + 1);
    const bool has_longer_ionbin =
        std::any_of(longer_analyses.begin(), longer_analyses.end(), [&](const grammar::InflectionCandidate& candidate) {
          return (candidate.verb_type == grammar::VerbType::GodanKa ||
                  candidate.verb_type == grammar::VerbType::GodanGa) &&
                 candidate.base_form != result.entry->lemma &&
                 candidate.confidence >= candidate::kParticleVerbBoundaryMinConfidence;
        });
    if (has_longer_ionbin) {
      return true;
    }
  }

  // Exact dictionary nouns are tokenizer search units.  If multiple noun
  // entries share a start, keep the longest one instead of letting the
  // negative lexical costs of two shorter noun edges defeat it.  Competing
  // grammatical categories remain available, so this changes only the
  // ownership relation among exact Noun homographs.
  // An all-kana formal noun is exempt. It is a closed-class grammatical
  // element, and a kana homograph starting at the same place carries no
  // orthographic boundary of its own, so length alone cannot say which
  // morpheme is present — the connection has to (ことば vs こと+ばかり).
  // A formal noun spelled with kanji is not exempt: the script change marks
  // the boundary, and the longer registered entry is a real search unit
  // (当たり障り, not 当たり+障り).
  const bool kana_formal_noun =
      result.entry->extended_pos == core::ExtendedPOS::NounFormal && grammar::isPureHiragana(result.entry->surface);
  if (result.entry->pos == core::PartOfSpeech::Noun && result.length < longest_noun && !kana_formal_noun) {
    return true;
  }
  if (result.entry->pos == core::PartOfSpeech::Noun &&
      startsInsideSentenceInitialDictionaryNoun(dict_manager, text, start_pos)) {
    return true;
  }
  // A registered noun beginning with a topic-particle homograph can absorb
  // that productive boundary after another nominal (そこ+は+にわ).  Require
  // both closed-class evidence for the first mora and L2 noun evidence for
  // the suffix, so a standalone lexical noun (はにわ) remains whole.
  if (result.entry->pos == core::PartOfSpeech::Noun && result.length > 1 && hasPrecedingNominal(lattice, start_pos) &&
      lookupResultsHaveExtendedPOS(lookup_results, core::ExtendedPOS::ParticleTopic, 1) &&
      lookupEntryInRange(dict_manager, codepoints, start_pos + 1, end_pos, core::PartOfSpeech::Noun) != nullptr) {
    return true;
  }

  // A one-kanji na-adjective entry cannot begin inside a contiguous kanji
  // run. In that position it is the tail of the surrounding lexical noun
  // (音楽, 喜怒哀楽), not an independent predicate. At a real adjective
  // boundary the same entry begins the run (楽だ, 楽な仕事).
  if (result.entry->extended_pos == core::ExtendedPOS::AdjNaAdj && result.length == 1 && start_pos > 0 &&
      normalize::isKanjiCodepoint(codepoints[start_pos - 1])) {
    return true;
  }

  if (conflictsWithVerifiedCompoundBoundary(lattice, dict_manager, text, byte_offsets, codepoints, start_pos, end_pos,
                                            result.entry->pos, result.entry->extended_pos)) {
    return true;
  }
  return false;
}

}  // namespace suzume::analysis::tokenizer_dictionary_detail
