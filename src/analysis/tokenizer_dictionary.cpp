/**
 * @file tokenizer_dictionary.cpp
 * @brief Dictionary-backed candidate generation for the tokenizer
 */

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

#include <algorithm>
#include <array>

#include "analysis/category_cost.h"
#include "analysis/dictionary_probe.h"
#include "analysis/tokenizer.h"
#include "candidate_constants.h"
#include "core/debug.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/honorific_verbs.h"
#include "join_candidates.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "split_candidates.h"
#include "suffix_candidates.h"
#include "tokenizer_dictionary_internal.h"
#include "tokenizer_utils.h"
#include "verb_candidates_helpers.h"

namespace suzume::analysis {

namespace {

// A kanji run ending in な is an attributive na-adjective candidate.  A
// preceding one-kanji formal noun remains a separate grammatical unit in this
// environment (時 + 不思議 + な), unlike an ordinary lexical kanji compound.
bool isKanjiRunFollowedByAttributiveNa(const std::vector<char32_t>& codepoints, size_t start_pos) {
  size_t pos = start_pos;
  while (pos < codepoints.size() && normalize::isKanjiCodepoint(codepoints[pos])) {
    ++pos;
  }
  return pos > start_pos && pos < codepoints.size() && codepoints[pos] == U'な';
}

// Whether a dictionary verb ends exactly at @p end_pos while starting before
// @p start_pos, i.e. the span in question is the tail of a longer headword.
bool endsDictionaryVerbSpanningBack(const dictionary::DictionaryManager& dict_manager,
                                    const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  // A headword reaching back further than this is not a contraction host.
  constexpr size_t kMaxHostChars = 4;
  const size_t scan_start = start_pos > kMaxHostChars ? start_pos - kMaxHostChars : 0;
  for (size_t host_start = scan_start; host_start < start_pos; ++host_start) {
    if (lookupEntryInRange(dict_manager, codepoints, host_start, end_pos, core::PartOfSpeech::Verb) != nullptr) {
      return true;
    }
  }
  return false;
}

// The passive される may follow a productive Sahen nominal, but an arbitrary
// one-kanji unknown noun is not enough evidence for that omitted する. A
// dictionary noun can establish the lexical exception (愛+さ+れる), while an
// unregistered multi-kanji Sino-Japanese run remains a productive Sahen host
// (反映+さ+れる). This leaves a one-kanji verb's own irrealis candidate to
// own the boundary in 許さ+れる.
bool hasPrecedingSahenNominal(const core::Lattice& lattice, size_t end_pos) {
  return core::anyEdgeEndingAt(lattice, end_pos, [](const core::LatticeEdge& edge) {
    // Sahen is productive over both nominal scripts: a kanji compound and a
    // loanword take する alike (実施する, キャンセルする), so an unregistered
    // katakana run heads the construction just as an unregistered kanji run
    // does. Requiring kanji made the passive boundary depend on the host's
    // script rather than on its class.
    constexpr size_t kMinSahenNominalLength = 2;
    return edge.pos == core::PartOfSpeech::Noun &&
           (edge.fromDictionary() || ((grammar::isAllKanji(edge.surface) || normalize::isAllKatakana(edge.surface)) &&
                                      normalize::utf8Length(edge.surface) >= kMinSahenNominalLength));
  });
}

// The emphatic interrogative construction (何と+し+て+も, 誰と+し+て+も)
// is compositional.  Its quoted particle and する te-form must not be hidden
// by the otherwise valid compound-particle candidate として.  Look for a
// dictionary-verified interrogative ending exactly at the candidate boundary;
// this keeps ordinary nominal uses such as 道具としても intact.
bool hasInterrogativeEndingAt(const dictionary::DictionaryManager& dict_manager, std::string_view text,
                              const ByteOffsets& byte_offsets, size_t end_pos) {
  const size_t scan_start = end_pos > kDictionaryLookbehindChars ? end_pos - kDictionaryLookbehindChars : 0;
  for (size_t start_pos = scan_start; start_pos < end_pos; ++start_pos) {
    const size_t byte_pos = byteOffsetAt(byte_offsets, start_pos);
    for (const auto& result : dict_manager.lookup(text, byte_pos)) {
      if (result.entry != nullptr && result.entry->extended_pos == core::ExtendedPOS::PronounInterrogative &&
          start_pos + result.length == end_pos) {
        return true;
      }
    }
  }
  return false;
}

// An indefinite か can close a short interrogative nominal phrase rather than
// only an immediately preceding pronoun: いつ+の+間+に+か, 誰+に+か. Walk
// backward over lattice edges that can stay inside such a phrase. The bounded
// reverse index keeps this proportional to the local candidate count.
bool hasInterrogativeNominalPhraseEndingAt(const core::Lattice& lattice, size_t end_pos) {
  const size_t scan_start = end_pos > kDictionaryLookbehindChars ? end_pos - kDictionaryLookbehindChars : 0;
  std::array<bool, kDictionaryLookbehindChars + 1> reachable{};
  reachable[end_pos - scan_start] = true;

  for (size_t boundary = end_pos; boundary > scan_start; --boundary) {
    if (!reachable[boundary - scan_start]) {
      continue;
    }
    for (const uint32_t edge_id : lattice.edgeIdsEndingAt(boundary)) {
      const auto& edge = lattice.getEdge(edge_id);
      if (edge.start < scan_start) {
        continue;
      }
      if (edge.extended_pos == core::ExtendedPOS::PronounInterrogative) {
        return true;
      }
      const bool stays_in_nominal_phrase =
          core::isNounType(edge.extended_pos) || edge.pos == core::PartOfSpeech::Suffix ||
          edge.extended_pos == core::ExtendedPOS::ParticleCase || edge.extended_pos == core::ExtendedPOS::ParticleNo ||
          edge.extended_pos == core::ExtendedPOS::ParticleBinding ||
          edge.extended_pos == core::ExtendedPOS::ParticleTopic;
      if (stays_in_nominal_phrase) {
        reachable[edge.start - scan_start] = true;
      }
    }
  }
  return false;
}

// A lexicalized noun beginning with お/ご can contain a suffix that happens to
// be a verb form.  Once the lattice has reached that suffix, prefer the whole
// dictionary noun and do not reopen it as a low-cost verb/auxiliary chain.
// The verb-tail check is essential: ordinary prefixed nouns such as おかし
// retain their independently searchable prefix + noun analysis.
bool startsHonorificPrefixedNounWithVerbTail(const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                             const std::vector<char32_t>& codepoints, const ByteOffsets& byte_offsets,
                                             size_t start_pos) {
  if (start_pos == 0 || !grammar::isHonorificPrefix(extractSubstring(codepoints, start_pos - 1, start_pos))) {
    return false;
  }

  const size_t prefix_pos = start_pos - 1;
  const size_t prefix_byte_pos = byteOffsetAt(byte_offsets, prefix_pos);
  for (const auto& result : dict_manager.lookup(text, prefix_byte_pos)) {
    if (result.entry == nullptr || result.entry->pos != core::PartOfSpeech::Noun || result.length <= 1) {
      continue;
    }

    const size_t noun_end = prefix_pos + result.length;
    if (noun_end <= start_pos || noun_end > codepoints.size()) {
      continue;
    }

    if (lookupEntryInRange(dict_manager, codepoints, start_pos, noun_end, core::PartOfSpeech::Verb) != nullptr) {
      return true;
    }
  }
  return false;
}

// A pure-hiragana na-adjective can share its surface with the interior of a
// kanji-led inflected verb. If a previously generated verb edge already
// crosses this position, the adjective cannot begin here without cutting the
// verb stem (読まれ, 生まれて, 止まれ). Scan only the immediately preceding
// kanji run; this keeps the check bounded and leaves genuine clause-initial or
// post-particle adjective uses available.
bool startsInsideKanjiLedVerb(const core::Lattice& lattice, const std::vector<char32_t>& codepoints, size_t start_pos) {
  if (start_pos == 0 || !normalize::isKanjiCodepoint(codepoints[start_pos - 1])) {
    return false;
  }

  size_t kanji_start = start_pos;
  while (kanji_start > 0 && normalize::isKanjiCodepoint(codepoints[kanji_start - 1])) {
    --kanji_start;
  }
  for (size_t pos = kanji_start; pos < start_pos; ++pos) {
    if (core::anyEdgeStartingAt(lattice, pos, [start_pos](const core::LatticeEdge& edge) {
          return edge.pos == core::PartOfSpeech::Verb && edge.end > start_pos && edge.lemmaVerified();
        })) {
      return true;
    }
  }
  return false;
}

// A dictionary adverb cannot begin inside an already verified inflected
// predicate.  Short literary adverbs can be homographic with the tail of an
// adjective or auxiliary followed by a particle (ない+と, らしい+と).  Keep
// the adverb available at a real boundary while protecting the longer
// grammatical edge that crosses this position.
//
// A predicate only counts when its own right edge could be a word boundary.
// Small kana cannot open a word, so an edge that ends just before one has not
// finished the word it belongs to and is in no position to claim the span:
// のめ (the potential stem of 飲む) ends before the っ of のめっちゃ, and
// letting it suppress the adverb hands those morae to a fragment instead.
bool startsInsideVerifiedPredicate(const core::Lattice& lattice, const std::vector<char32_t>& codepoints,
                                   size_t start_pos) {
  const size_t scan_start = start_pos > kDictionaryLookbehindChars ? start_pos - kDictionaryLookbehindChars : 0;
  for (size_t edge_start = scan_start; edge_start < start_pos; ++edge_start) {
    if (core::anyEdgeStartingAt(lattice, edge_start, [&codepoints, start_pos](const core::LatticeEdge& edge) {
          return edge.end > start_pos && edge.lemmaVerified() &&
                 (edge.pos == core::PartOfSpeech::Verb || edge.pos == core::PartOfSpeech::Adjective ||
                  edge.pos == core::PartOfSpeech::Auxiliary) &&
                 (edge.end >= codepoints.size() || !kana::isSmallKanaCodepoint(codepoints[edge.end]));
        })) {
      return true;
    }
  }
  return false;
}

// A kana determiner can begin at the final mora of a productive verb
// continuative (たなびき+たる, not たなび+きたる).  Probe the predicate run
// following the nearest particle that can introduce a predicate, and permit a
// suffix probe because the confidence scorer deliberately discounts long
// all-hiragana stems while still recognizing their productive tail.  An
// immediately preceding particle means the determiner starts at a real
// boundary and must remain available (そして+きたる).
bool hasProductiveContinuativeCrossingDeterminer(const core::Lattice& lattice, const grammar::Inflection& inflection,
                                                 const dictionary::DictionaryManager& dict_manager,
                                                 const std::vector<char32_t>& codepoints, size_t determiner_start) {
  if (determiner_start == 0 || !grammar::isIRowCodepoint(codepoints[determiner_start])) {
    return false;
  }

  size_t host_start = determiner_start > kDictionaryLookbehindChars ? determiner_start - kDictionaryLookbehindChars : 0;
  for (size_t boundary = determiner_start; boundary > host_start; --boundary) {
    const bool follows_predicate_introducing_particle =
        core::anyEdgeEndingAt(lattice, boundary, [](const core::LatticeEdge& edge) {
          return edge.extended_pos == core::ExtendedPOS::ParticleCase ||
                 edge.extended_pos == core::ExtendedPOS::ParticleNo ||
                 edge.extended_pos == core::ExtendedPOS::ParticleTopic ||
                 edge.extended_pos == core::ExtendedPOS::ParticleBinding ||
                 edge.extended_pos == core::ExtendedPOS::ParticleAdverbial;
        });
    if (follows_predicate_introducing_particle) {
      host_start = boundary;
      break;
    }
  }
  if (host_start == determiner_start) {
    return false;
  }

  for (size_t probe_start = host_start; probe_start < determiner_start; ++probe_start) {
    const std::string continuative = extractSubstring(codepoints, probe_start, determiner_start + 1);
    if (dict_manager.lookupExact(continuative, core::PartOfSpeech::Verb) != nullptr) {
      return true;
    }
    const auto inflection_candidates = inflection.analyze(continuative);
    if (std::any_of(inflection_candidates.begin(), inflection_candidates.end(),
                    [](const grammar::InflectionCandidate& inflection_candidate) {
                      return inflection_candidate.verb_type != grammar::VerbType::IAdjective &&
                             !inflection_candidate.suffix.empty() &&
                             inflection_candidate.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence;
                    })) {
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

// A dictionary adverb may open on the last mora of a longer content word and
// carry an independent particle along with it (事実+に read as 事+実に, 勢い+と
// as 勢+いと, 勢い+とも as 勢+いとも). The same adverb stays available at a real
// boundary (実に+難しい, いとも+簡単に), so the guard is not about the entry but
// about the offset: reject it only when the mora it opens on completes a
// content edge that starts earlier, and what remains of the adverb after that
// mora is itself a registered particle. Both halves of the competing reading
// are then lexically attested, which the adverb's own span is not.
// This uses lattice structure rather than enumerating open-class words.
bool opensOnContentWordTailBeforeParticle(const core::Lattice& lattice,
                                          const dictionary::DictionaryManager& dict_manager,
                                          const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  if (start_pos == 0 || end_pos <= start_pos + 1 || end_pos > codepoints.size()) {
    return false;
  }
  if (lookupEntryInRange(dict_manager, codepoints, start_pos + 1, end_pos, core::PartOfSpeech::Particle) == nullptr) {
    return false;
  }
  const size_t content_end = start_pos + 1;
  for (size_t content_start = 0; content_start < start_pos; ++content_start) {
    if (core::anyEdgeStartingAt(lattice, content_start, [content_end](const core::LatticeEdge& edge) {
          return edge.end == content_end &&
                 (edge.pos == core::PartOfSpeech::Noun || edge.pos == core::PartOfSpeech::Adjective);
        })) {
      return true;
    }
  }
  return false;
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
  const bool follows_verb_host = hasPrecedingExtendedPOS(lattice, candidate_start, core::ExtendedPOS::VerbRenyokei) ||
                                 hasPrecedingExtendedPOS(lattice, candidate_start, core::ExtendedPOS::VerbOnbinkei);
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
      (hasPrecedingExtendedPOS(lattice, candidate_start, core::ExtendedPOS::VerbMizenkei) ||
       hasPrecedingExtendedPOS(lattice, candidate_start, core::ExtendedPOS::VerbRenyokei) ||
       hasPrecedingExtendedPOS(lattice, candidate_start, core::ExtendedPOS::VerbOnbinkei))) {
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

// The temporal adverb いま overlaps the full polite forms of いる
// (います/いました/いません/…).  At a clause boundary the closed inflectional
// chain is more specific than the accidental いま+verb path.  Do not apply
// this inside a longer lexical continuation: いますぐ remains いま+すぐ.
bool startsIruPoliteFormAt(const std::vector<char32_t>& codepoints, size_t start_pos) {
  if (start_pos >= codepoints.size() || codepoints[start_pos] != U'い') {
    return false;
  }
  const size_t masu_length = verb_helpers::finiteMasuFormLengthAt(codepoints, start_pos + 1);
  if (masu_length == 0) {
    return false;
  }
  const size_t end_pos = start_pos + 1 + masu_length;
  if (end_pos >= codepoints.size()) {
    return true;
  }
  const char32_t following = codepoints[end_pos];
  return normalize::isExtendedParticle(following) || following == U'。' || following == U'、' || following == U'」' ||
         following == U'）';
}

// The literary conjunctive expression ～につけ attaches to a preceding finite
// predicate and introduces a following clause (聞くにつけ、思い出す). It must
// not compete with the unrelated verb つける in sentence-initial につけて or
// in a construction such as 順位につけている, so require both the preceding
// lattice verb boundary and the clause-separating comma.
bool startsLiteraryNitsukeAt(const core::Lattice& lattice, const std::vector<char32_t>& codepoints, size_t start_pos) {
  constexpr size_t kNitsukeLength = 3;
  if (start_pos == 0 || start_pos + kNitsukeLength >= codepoints.size() || codepoints[start_pos] != U'に' ||
      codepoints[start_pos + 1] != U'つ' || codepoints[start_pos + 2] != U'け' ||
      codepoints[start_pos + kNitsukeLength] != U'、') {
    return false;
  }
  return hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbShuushikei);
}

// The method suffix 方 attaches to a kanji-containing deverbal noun
// (打ち合わせ+方). The unknown-word path can create the deverbal noun before
// the suffix position but has no all-kanji suffix rule to supply 方 itself.
bool hasPrecedingDeverbalNoun(const core::Lattice& lattice, size_t start_pos) {
  bool has_noun = false;
  bool has_renyokei = false;
  for (const uint32_t edge_id : lattice.edgeIdsEndingAt(start_pos)) {
    const auto& edge = lattice.getEdge(edge_id);
    if (grammar::containsKanji(edge.surface) && edge.pos == core::PartOfSpeech::Noun) {
      has_noun = true;
    }
    if (grammar::containsKanji(edge.surface) && edge.extended_pos == core::ExtendedPOS::VerbRenyokei) {
      has_renyokei = true;
    }
  }
  return has_noun && has_renyokei;
}

// A polite-auxiliary homograph is not a real boundary when it begins inside a
// longer, dictionary-verified verb renyokei ending at the same position
// (醒まし/て, さまし/て).  Requiring both the shared end and verified lemma
// keeps ordinary polite chains such as 食べ/まし/て and 読み/まし/て intact.
bool hasCoveringVerifiedVerbRenyokei(const core::Lattice& lattice, size_t interior_start, size_t shared_end) {
  return core::anyEdgeEndingAt(lattice, shared_end, [interior_start](const core::LatticeEdge& edge) {
    return edge.start < interior_start && edge.extended_pos == core::ExtendedPOS::VerbRenyokei && edge.lemmaVerified();
  });
}

// An intentional auxiliary is structurally meaningful here only when it
// closes the verb form selected by that auxiliary. Looking merely for any
// candidate ending at start_pos mistakes homographic word endings for an
// independent auxiliary and suppresses the following particle.
bool hasPrecedingVerbVolitionalChain(const core::Lattice& lattice, size_t start_pos) {
  for (const uint32_t edge_id : lattice.edgeIdsEndingAt(start_pos)) {
    const auto& edge = lattice.getEdge(edge_id);
    if (edge.extended_pos != core::ExtendedPOS::AuxVolitional &&
        edge.extended_pos != core::ExtendedPOS::AuxNegativeMai) {
      continue;
    }
    const bool licensed = core::anyEdgeEndingAt(lattice, edge.start, [&edge](const core::LatticeEdge& verb) {
      const bool licenses_volitional =
          edge.extended_pos == core::ExtendedPOS::AuxVolitional && verb.extended_pos == core::ExtendedPOS::VerbMizenkei;
      const bool licenses_negative_intent = edge.extended_pos == core::ExtendedPOS::AuxNegativeMai &&
                                            verb.extended_pos == core::ExtendedPOS::VerbShuushikei;
      return licenses_volitional || licenses_negative_intent;
    });
    if (licensed) {
      return true;
    }
  }
  return false;
}

bool hasPrecedingNominal(const core::Lattice& lattice, size_t start_pos) {
  constexpr PartOfSpeechMask kNominalMask =
      partOfSpeechMask(core::PartOfSpeech::Noun) | partOfSpeechMask(core::PartOfSpeech::Pronoun);
  return hasPrecedingPartOfSpeech(lattice, start_pos, kNominalMask);
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
  return std::any_of(sentence_initial.begin(), sentence_initial.end(), [start_pos](const auto& result) {
    return result.entry != nullptr && result.entry->pos == core::PartOfSpeech::Noun && result.length > start_pos;
  });
}

// A two-mora conjunction candidate can straddle the productive boundary in
// AdjNaAdj + な + お/ご + nominal (重要+な+お+知らせ). A preceding
// dictionary-verified na-adjective and a kanji/katakana head on the right make
// that structure explicit, including open-class heads absent from the
// dictionary, so the discourse-conjunction homograph is unavailable. Requiring
// dictionary evidence keeps incidental unknown adjective candidates from
// suppressing a real conjunction after a completed clause.
bool crossesAttributiveNaHonorificNominal(const core::Lattice& lattice, const std::vector<char32_t>& codepoints,
                                          size_t start_pos, size_t end_pos) {
  if (end_pos != start_pos + 2 || end_pos >= codepoints.size() || codepoints[start_pos] != U'な' ||
      !grammar::isHonorificPrefix(extractSubstring(codepoints, start_pos + 1, end_pos)) ||
      (!normalize::isKanjiCodepoint(codepoints[end_pos]) &&
       normalize::classifyChar(codepoints[end_pos]) != normalize::CharType::Katakana)) {
    return false;
  }

  return core::anyEdgeEndingAt(lattice, start_pos, [](const core::LatticeEdge& edge) {
    return edge.extended_pos == core::ExtendedPOS::AdjNaAdj && edge.fromDictionary();
  });
}

// The temporal noun 間 is licensed after a completed attributive predicate.
// Generate it only at that boundary instead of registering a global one-kanji
// noun that would reopen 間もなく, 時間, or 間違える internally.
bool hasPrecedingAttributivePredicate(const core::Lattice& lattice, size_t start_pos) {
  return hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbShuushikei) ||
         hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbRentaikei) ||
         hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::AdjBasic) ||
         hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::AuxTenseTa) ||
         hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::AuxTenseMasu) ||
         hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::AuxNegativeNai) ||
         hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::AuxCopulaDa) ||
         hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::AuxCopulaDesu);
}

// generateTemporalNounBoundaryCandidates() marks the left side of a
// lexicalized all-kanji + 間もなく sequence as a PrefixCompound noun.  Use
// that structural edge to distinguish 終了|間もなく from a candidate that
// would reopen the interior of 時間 (時|間もなく).
bool hasPrecedingTemporalCompoundBoundary(const core::Lattice& lattice, size_t start_pos) {
  return core::anyEdgeEndingAt(lattice, start_pos, [](const core::LatticeEdge& edge) {
    return edge.pos == core::PartOfSpeech::Noun && edge.origin == core::CandidateOrigin::PrefixCompound;
  });
}

bool startsFormalNounParticleAfterPredicate(const core::Lattice& lattice,
                                            const dictionary::DictionaryManager& dict_manager,
                                            const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  constexpr PartOfSpeechMask kPredicateMask =
      partOfSpeechMask(core::PartOfSpeech::Verb) | partOfSpeechMask(core::PartOfSpeech::Adjective);
  if (!hasPrecedingPartOfSpeech(lattice, start_pos, kPredicateMask)) {
    return false;
  }
  for (size_t split = start_pos + 1; split < end_pos; ++split) {
    const auto* noun = lookupEntryInRange(dict_manager, codepoints, start_pos, split, core::PartOfSpeech::Noun);
    if (noun == nullptr || noun->extended_pos != core::ExtendedPOS::NounFormal) {
      continue;
    }
    if (lookupEntryInRange(dict_manager, codepoints, split, end_pos, core::PartOfSpeech::Particle) != nullptr) {
      return true;
    }
  }
  return false;
}

// A case particle immediately before an ABAB mimetic is a stronger boundary
// than a homographic multi-mora dictionary entry beginning at that particle
// (鈴+が+りんりんと, not がり+んりんと).
bool startsParticleBeforeReduplicatedMimetic(const std::vector<char32_t>& codepoints, size_t start_pos) {
  if (start_pos + 5 >= codepoints.size() || !normalize::isParticleCodepoint(codepoints[start_pos])) {
    return false;
  }
  const size_t rest = start_pos + 1;
  return codepoints[rest] == codepoints[rest + 2] && codepoints[rest + 1] == codepoints[rest + 3] &&
         codepoints[rest + 4] == U'と';
}

// Kyoto honorific やす is a closed auxiliary, but its two-mora surface is also
// the stem of the productive difficulty adjective やすい. Admit the honorific
// only after the local honorific construction that licenses it: お/ご plus a
// verb continuative, or a benefactive request form. This keeps 読み+やすかっ
// as an adjective while preserving お+見+やす and 読んで+おくれ+やす.
bool followsKyotoHonorificYasuHost(const core::Lattice& lattice, size_t start_pos) {
  for (const uint32_t edge_id : lattice.edgeIdsEndingAt(start_pos)) {
    const auto& edge = lattice.getEdge(edge_id);
    if (edge.extended_pos == core::ExtendedPOS::AuxBenefactive) {
      return true;
    }
    if (edge.extended_pos != core::ExtendedPOS::VerbRenyokei) {
      continue;
    }
    if (core::anyEdgeEndingAt(lattice, edge.start, [](const core::LatticeEdge& prefix) {
          return prefix.extended_pos == core::ExtendedPOS::Prefix && grammar::isHonorificPrefix(prefix.surface);
        })) {
      return true;
    }
  }
  return false;
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
  const size_t probe_start = start_pos > kStemProbeChars ? start_pos - kStemProbeChars : 0;
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

// A formal noun after the negative-quote frame (…ん+と) must not hide a
// dictionary verb irrealis plus the following negative auxiliary.  This is a
// structural ambiguity: the formal-noun edge has no predicate host there,
// while the split supplies one.  Keep ordinary formal-noun uses (結果いかんで)
// and unrelated quotative phrases available.
bool followsNegativeQuote(const core::Lattice& lattice, size_t start_pos) {
  for (const uint32_t quote_id : lattice.edgeIdsEndingAt(start_pos)) {
    const auto& quote = lattice.getEdge(quote_id);
    // と is lexically ambiguous between a quotation and a case particle.  In
    // this frame the preceding negative predicate supplies the quoted clause,
    // so either dictionary label represents the same boundary.
    if (quote.extended_pos != core::ExtendedPOS::ParticleQuote &&
        !(quote.extended_pos == core::ExtendedPOS::ParticleCase &&
          grammar::isSingleHiragana(quote.surface, core::hiragana::kTo))) {
      continue;
    }
    if (core::anyEdgeEndingAt(lattice, quote.start, [](const core::LatticeEdge& negative) {
          return negative.extended_pos == core::ExtendedPOS::AuxNegativeNu;
        })) {
      return true;
    }
  }
  return false;
}

bool followsNegativeAuxiliary(const core::Lattice& lattice, size_t start_pos) {
  for (const uint32_t negative_id : lattice.edgeIdsEndingAt(start_pos)) {
    const auto& negative = lattice.getEdge(negative_id);
    if (negative.extended_pos != core::ExtendedPOS::AuxNegativeNu) {
      continue;
    }
    // A nasal onbin happens to contain a competing one-mora ん entry
    // (読ん+どく).  It is a negative only when it has an actual irrealis host,
    // as in 確認せ+ん+と.  Checking the immediate lattice predecessor keeps
    // this guard structural instead of suppressing every accidental ん edge.
    if (core::anyEdgeEndingAt(lattice, negative.start, [](const core::LatticeEdge& host) {
          return host.extended_pos == core::ExtendedPOS::VerbMizenkei;
        })) {
      return true;
    }
  }
  return false;
}

// The contracted explanatory nominalizer in …てん/…でん follows a
// conjunctive te-form.  Classical negative ん instead requires a verb
// irrealis host, so retaining that homograph here can only fabricate an
// impossible analysis (読ん+で+ん+の).  Checking the preceding lattice edge
// makes this a grammatical boundary guard rather than a surface exception.
bool followsConjunctiveTeDe(const core::Lattice& lattice, size_t start_pos) {
  for (const uint32_t edge_id : lattice.edgeIdsEndingAt(start_pos)) {
    const auto& edge = lattice.getEdge(edge_id);
    if (edge.extended_pos != core::ExtendedPOS::ParticleConj || !grammar::isTeDeSurface(edge.surface)) {
      continue;
    }
    // The boundary is explanatory only when the conjunctive particle itself
    // follows a predicate.  A kana inside an Ichidan host (慌て+ずに) also has
    // a competing one-mora て particle edge, but its left neighbor is a noun
    // fragment rather than the te-form's predicate.
    if (core::anyEdgeEndingAt(lattice, edge.start,
                              [](const core::LatticeEdge& host) { return host.pos == core::PartOfSpeech::Verb; })) {
      return true;
    }
  }
  return false;
}

// A candidate span whose last character is the contracted negative ん competes
// with a dictionary irrealis one character shorter.  Both the formal-noun and
// the irrealis reading of the span are decided by the same evidence, so they
// ask this one question rather than each carrying its own scan.
bool hasShorterMizenkeiBeforeNegative(const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                      size_t byte_offset, const std::vector<char32_t>& codepoints, size_t start_pos,
                                      size_t candidate_length) {
  if (candidate_length < 2 || start_pos + candidate_length > codepoints.size() ||
      codepoints[start_pos + candidate_length - 1] != U'ん') {
    return false;
  }
  for (const auto& alternative : dict_manager.lookup(text, byte_offset)) {
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
bool hasLongerVerbBeforeNegative(const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                 size_t byte_offset, const std::vector<char32_t>& codepoints, size_t start_pos,
                                 size_t candidate_length) {
  for (const auto& alternative : dict_manager.lookup(text, byte_offset)) {
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

// A closed determiner may happen to share a whole surface with a dictionary
// Godan onbin + past form.  At a sentence boundary the finite predicate owns
// that construction: the determiner requires a following nominal, whereas
// the attested verb stem and its matching past allomorph form a complete
// clause.  Resolve this from the conjugation table and lexical base evidence,
// never from a particular homographic surface.
// Both the past and the connective suffix select their voiced allomorph from
// the same Godan row, so the two forms differ only in which kana pair closes
// the sequence.
bool isDictionaryOnbinBefore(const dictionary::DictionaryManager& dict_manager, std::string_view surface,
                             std::string_view unvoiced, std::string_view voiced) {
  const std::string_view suffix = utf8::lastChar(surface);
  if (suffix != unvoiced && suffix != voiced) {
    return false;
  }
  const std::string_view onbin_stem = utf8::dropLastChar(surface);
  const std::string_view onbin = utf8::lastChar(onbin_stem);
  const std::string_view lexical_stem = utf8::dropLastChar(onbin_stem);
  if (lexical_stem.empty()) {
    return false;
  }
  const auto match = verb_helpers::firstGodanOnbinDictBase(&dict_manager, lexical_stem, onbin);
  if (!match.matched) {
    return false;
  }
  const auto* row = grammar::Conjugation::getGodanRow(match.verb_type);
  return row != nullptr && (row->voiced_ta ? suffix == voiced : suffix == unvoiced);
}

bool isDictionaryOnbinPast(const dictionary::DictionaryManager& dict_manager, std::string_view surface) {
  return isDictionaryOnbinBefore(dict_manager, surface, "た", "だ");
}

bool isDictionaryOnbinTeForm(const dictionary::DictionaryManager& dict_manager, std::string_view surface) {
  return isDictionaryOnbinBefore(dict_manager, surface, "て", "で");
}

bool startsKuruConditional(const std::vector<char32_t>& codepoints, size_t start_pos) {
  return start_pos + 2 < codepoints.size() && codepoints[start_pos] == U'く' && codepoints[start_pos + 1] == U'れ' &&
         codepoints[start_pos + 2] == U'ば';
}

struct ContextualDictionaryCandidateState {
  bool has_attributive_temporal_ma{false};
  bool starts_shortened_causative_passive{false};
};

ContextualDictionaryCandidateState addContextualDictionaryCandidates(
    core::Lattice& lattice, const dictionary::DictionaryManager& dict_manager, std::string_view text,
    const std::vector<char32_t>& codepoints, const ByteOffsets& byte_offsets, size_t start_pos) {
  ContextualDictionaryCandidateState state;

  if (startsLiteraryNitsukeAt(lattice, codepoints, start_pos)) {
    lattice.addEdge("につけ", static_cast<uint32_t>(start_pos), static_cast<uint32_t>(start_pos + 3),
                    core::PartOfSpeech::Particle, getCategoryCost(core::ExtendedPOS::ParticleConj),
                    core::LatticeEdge::kFromDictionary, "につけ", dictionary::ConjugationType::None,
                    core::CandidateOrigin::Dictionary, candidate::kDictionaryOriginConfidence, {},
                    core::ExtendedPOS::ParticleConj, "literary_nitsuke");
  }

  const bool starts_kyoto_honorific_yasu = start_pos + 1 < codepoints.size() && codepoints[start_pos] == U'や' &&
                                           codepoints[start_pos + 1] == U'す' &&
                                           followsKyotoHonorificYasuHost(lattice, start_pos);
  if (starts_kyoto_honorific_yasu) {
    lattice.addEdge("やす", static_cast<uint32_t>(start_pos), static_cast<uint32_t>(start_pos + 2),
                    core::PartOfSpeech::Auxiliary, getCategoryCost(core::ExtendedPOS::AuxHonorific),
                    core::LatticeEdge::kFromDictionary, "やす", dictionary::ConjugationType::None,
                    core::CandidateOrigin::Dictionary, candidate::kDictionaryOriginConfidence, {},
                    core::ExtendedPOS::AuxHonorific, "kyoto_honorific_yasu");
  }

  // The regional causal き is indistinguishable from an ordinary
  // continuative in isolation, so it has no global L1 entry. A completed past
  // auxiliary supplies the only unambiguous host (飲ん+だ+き), allowing this
  // context-licensed particle edge without cutting き out of lexical verbs.
  if (codepoints[start_pos] == U'き' && hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::AuxTenseTa)) {
    constexpr auto causal_epos = core::ExtendedPOS::ParticleConj;
    lattice.addEdge("き", static_cast<uint32_t>(start_pos), static_cast<uint32_t>(start_pos + 1),
                    core::PartOfSpeech::Particle, getCategoryCost(causal_epos), core::LatticeEdge::kFromDictionary,
                    "き", dictionary::ConjugationType::None, core::CandidateOrigin::Dictionary,
                    candidate::kDictionaryOriginConfidence, {}, causal_epos, "regional_causal_ki");
  }

  if (codepoints[start_pos] == U'方' && hasPrecedingDeverbalNoun(lattice, start_pos)) {
    lattice.addEdge("方", static_cast<uint32_t>(start_pos), static_cast<uint32_t>(start_pos + 1),
                    core::PartOfSpeech::Suffix, candidate::kDeverbalMethodSuffixCost,
                    core::LatticeEdge::kFromDictionary, "方", dictionary::ConjugationType::None,
                    core::CandidateOrigin::SuffixPattern, candidate::kDictionaryOriginConfidence, {},
                    core::ExtendedPOS::Suffix, "deverbal_method_suffix");
  }

  state.has_attributive_temporal_ma =
      codepoints[start_pos] == U'間' && hasPrecedingAttributivePredicate(lattice, start_pos);
  if (state.has_attributive_temporal_ma) {
    constexpr auto temporal_epos = core::ExtendedPOS::NounFormal;
    lattice.addEdge("間", static_cast<uint32_t>(start_pos), static_cast<uint32_t>(start_pos + 1),
                    core::PartOfSpeech::Noun, getCategoryCost(temporal_epos),
                    core::LatticeEdge::kFromDictionary | core::LatticeEdge::kIsFormalNoun, "間",
                    dictionary::ConjugationType::None, core::CandidateOrigin::Dictionary,
                    candidate::kDictionaryOriginConfidence, {}, temporal_epos, "attributive_temporal_ma");
  }

  if (codepoints[start_pos] == U'か' && (hasInterrogativeEndingAt(dict_manager, text, byte_offsets, start_pos) ||
                                         hasInterrogativeNominalPhraseEndingAt(lattice, start_pos))) {
    lattice.addEdge("か", static_cast<uint32_t>(start_pos), static_cast<uint32_t>(start_pos + 1),
                    core::PartOfSpeech::Particle, getCategoryCost(core::ExtendedPOS::ParticleAdverbial),
                    core::LatticeEdge::kFromDictionary, "か", dictionary::ConjugationType::None,
                    core::CandidateOrigin::Dictionary, candidate::kDictionaryOriginConfidence, {},
                    core::ExtendedPOS::ParticleAdverbial, "indefinite_particle_ka");
  }

  // Keep the first/last か of a closed interrogative frame available when it
  // follows a finite predicate (読めるかどうか).
  const bool opens_interrogative_frame = codepoints[start_pos] == U'か' && start_pos + 3 < codepoints.size() &&
                                         codepoints[start_pos + 1] == U'ど' && codepoints[start_pos + 2] == U'う' &&
                                         codepoints[start_pos + 3] == U'か' &&
                                         hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbShuushikei);
  const bool closes_interrogative_frame =
      codepoints[start_pos] == U'か' && start_pos >= 3 && codepoints[start_pos - 3] == U'か' &&
      codepoints[start_pos - 2] == U'ど' && codepoints[start_pos - 1] == U'う' &&
      hasPrecedingExtendedPOS(lattice, start_pos - 3, core::ExtendedPOS::VerbShuushikei);
  if (opens_interrogative_frame || closes_interrogative_frame) {
    constexpr auto frame_epos = core::ExtendedPOS::ParticleAdverbial;
    lattice.addEdge("か", static_cast<uint32_t>(start_pos), static_cast<uint32_t>(start_pos + 1),
                    core::PartOfSpeech::Particle, getCategoryCost(frame_epos), core::LatticeEdge::kFromDictionary, "か",
                    dictionary::ConjugationType::None, core::CandidateOrigin::Dictionary,
                    candidate::kDictionaryOriginConfidence, {}, frame_epos, "interrogative_frame_ka");
  }

  // In shortened causative-passive, さ retains the lexical verb's mizenkei
  // boundary (読ま + さ + れ + た), rather than becoming a global する form.
  // A quotative predicate followed by される is する's irrealis plus the
  // passive auxiliary (…と + さ + れる), never the shortened causative
  // auxiliary.  Do not use the preceding character alone here: an irrealis
  // such as 書か is itself commonly also a particle character.
  const bool starts_quoted_passive =
      start_pos > 0 && codepoints[start_pos] == core::hiragana::kSa && start_pos + 1 < codepoints.size() &&
      codepoints[start_pos + 1] == U'れ' &&
      // The closed entry for と is POS-tagged as a case particle; its
      // quotative role is determined by this passive continuation.
      hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::ParticleCase) &&
      // A Godan irrealis ending here outranks the particle reading of its own
      // okurigana: 急が+さ+れ+た is the shortened causative-passive of 急ぐ, and
      // reading the が as a case particle turns the auxiliary into する. Only an
      // a-row kana can be that okurigana, which keeps a genuine quotative と in
      // the same position (確認したと+さ+れ+て).
      !(grammar::isARowCodepoint(codepoints[start_pos - 1]) &&
        hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbMizenkei)) &&
      verb_helpers::isPassiveAuxContinuation(codepoints, start_pos + 2, /*strict_masu=*/true);
  if (starts_quoted_passive) {
    lattice.addEdge(
        "さ", static_cast<uint32_t>(start_pos), static_cast<uint32_t>(start_pos + 1), core::PartOfSpeech::Verb,
        getCategoryCost(core::ExtendedPOS::VerbMizenkei) + candidate::verb_cost::kQuotedPassiveSuruBonus,
        core::LatticeEdge::kFromDictionary | core::LatticeEdge::kHasCustomCost, "する",
        dictionary::ConjugationType::Suru, core::CandidateOrigin::Dictionary, candidate::kDictionaryOriginConfidence,
        "quoted_passive_suru", core::ExtendedPOS::VerbMizenkei, "quoted_passive_suru");
  }
  // A sahen nominal immediately before される supplies する's irrealis;
  // preserve its independent passive boundary (反映+さ+れ+ます).
  const bool starts_sahen_passive =
      !starts_quoted_passive && start_pos > 0 && codepoints[start_pos] == core::hiragana::kSa &&
      start_pos + 1 < codepoints.size() && codepoints[start_pos + 1] == U'れ' &&
      hasPrecedingSahenNominal(lattice, start_pos) &&
      verb_helpers::isPassiveAuxContinuation(codepoints, start_pos + 2, /*strict_masu=*/true);
  if (starts_sahen_passive) {
    lattice.addEdge("さ", static_cast<uint32_t>(start_pos), static_cast<uint32_t>(start_pos + 1),
                    core::PartOfSpeech::Verb,
                    getCategoryCost(core::ExtendedPOS::VerbMizenkei) + candidate::verb_cost::kSahenPassiveSuruBonus,
                    core::LatticeEdge::kFromDictionary | core::LatticeEdge::kHasCustomCost, "する",
                    dictionary::ConjugationType::Suru, core::CandidateOrigin::Dictionary,
                    candidate::kDictionaryOriginConfidence, {}, core::ExtendedPOS::VerbMizenkei, "sahen_passive_suru");
  }
  state.starts_shortened_causative_passive =
      start_pos > 0 && codepoints[start_pos] == core::hiragana::kSa && start_pos + 1 < codepoints.size() &&
      !starts_quoted_passive && codepoints[start_pos + 1] == U'れ' &&
      hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbMizenkei) &&
      verb_helpers::isPassiveAuxContinuation(codepoints, start_pos + 2, /*strict_masu=*/true);
  if (state.starts_shortened_causative_passive) {
    lattice.addEdge(
        "さ", static_cast<uint32_t>(start_pos), static_cast<uint32_t>(start_pos + 1), core::PartOfSpeech::Auxiliary,
        getCategoryCost(core::ExtendedPOS::AuxCausative) + candidate::verb_cost::kShortenedCausativePassiveBonus,
        core::LatticeEdge::kFromDictionary | core::LatticeEdge::kHasCustomCost, "す",
        dictionary::ConjugationType::GodanSa, core::CandidateOrigin::Dictionary, candidate::kDictionaryOriginConfidence,
        "shortened_causative_passive", core::ExtendedPOS::AuxCausative, "shortened_causative_passive");
  }

  return state;
}

}  // namespace

void Tokenizer::addDictionaryCandidates(core::Lattice& lattice, std::string_view text,
                                        const std::vector<char32_t>& codepoints, const ByteOffsets& byte_offsets,
                                        size_t start_pos, std::vector<dictionary::LookupResult>& lookup_results) const {
  // Convert to byte position for dictionary lookup
  size_t byte_pos = byteOffsetAt(byte_offsets, start_pos);

  // Lookup in dictionary
  dict_manager_.lookupInto(text, byte_pos, lookup_results);
  const bool suppress_prefixed_noun_interior =
      startsHonorificPrefixedNounWithVerbTail(dict_manager_, text, codepoints, byte_offsets, start_pos);

  const ContextualDictionaryCandidateState contextual_candidates =
      addContextualDictionaryCandidates(lattice, dict_manager_, text, codepoints, byte_offsets, start_pos);
  const bool has_attributive_temporal_ma = contextual_candidates.has_attributive_temporal_ma;
  const bool starts_shortened_causative_passive = contextual_candidates.starts_shortened_causative_passive;

  size_t longest_conjunction = 0;
  size_t longest_fixed_conjunction = 0;
  size_t longest_interjection = 0;
  size_t longest_adverb = 0;
  size_t longest_noun = 0;
  size_t longest_potential_benefactive = 0;
  for (const auto& result : lookup_results) {
    if (result.entry != nullptr && result.entry->pos == core::PartOfSpeech::Conjunction) {
      longest_conjunction = std::max(longest_conjunction, result.length);
      // A conjunction whose surface also spells a productive chain does not own
      // its span the way a fixed expression does: でも and では are the copula
      // continuative with a binding particle, and the と-final members are a
      // predicate plus the conditional と. Both readings compete for the same
      // characters at a sentence start too (ではあるまいか is で+は+ある+まい+か),
      // so these must not suppress the shorter auxiliary prefix below. The word
      // scorer excludes the same two classes from the fixed-expression bonus.
      if (!grammar::isCopulaFusedConjunction(result.entry->surface) &&
          !grammar::isConditionalToConjunction(result.entry->surface)) {
        longest_fixed_conjunction = std::max(longest_fixed_conjunction, result.length);
      }
    }
    if (result.entry != nullptr && result.entry->pos == core::PartOfSpeech::Interjection) {
      longest_interjection = std::max(longest_interjection, result.length);
    }
    const size_t result_end = start_pos + result.length;
    const bool adverb_absorbs_quoted_question =
        result.entry != nullptr && result.entry->pos == core::PartOfSpeech::Adverb && result.length > 1 &&
        codepoints[result_end - 1] == U'か' && result_end + 2 < codepoints.size() && codepoints[result_end] == U'と' &&
        codepoints[result_end + 1] == U'い' && codepoints[result_end + 2] == U'う';
    if (result.entry != nullptr && result.entry->pos == core::PartOfSpeech::Adverb && !has_attributive_temporal_ma &&
        !adverb_absorbs_quoted_question) {
      longest_adverb = std::max(longest_adverb, result.length);
    }
    if (result.entry != nullptr && result.entry->pos == core::PartOfSpeech::Noun) {
      longest_noun = std::max(longest_noun, result.length);
    }
    if (result.entry != nullptr && grammar::isPotentialBenefactiveLemma(result.entry->lemma)) {
      longest_potential_benefactive = std::max(longest_potential_benefactive, result.length);
    }
  }

  for (const auto& result : lookup_results) {
    if (result.entry == nullptr) {
      continue;
    }

    // A closed kana numeral cannot open immediately after a completed
    // quantity. Repeated/distributive quantities are owned by the dedicated
    // counter candidate, while this position otherwise begins a particle or
    // predicate (一つ+と+おもう, not 一つ+とお+も+う).
    if (result.entry->extended_pos == core::ExtendedPOS::NounNumber &&
        (hasPrecedingQuantityEdge(lattice, start_pos) || insideKanjiVerbOkurigana(lattice, start_pos))) {
      continue;
    }

    // A finite predicate immediately before 間 establishes the productive
    // attributive formal-noun construction.  In that context an otherwise
    // valid lexical adverb beginning at the same position must not swallow
    // the grammatical 間 boundary.
    if (has_attributive_temporal_ma && result.entry->pos == core::PartOfSpeech::Adverb) {
      continue;
    }

    // Calculate end position in characters before context-sensitive candidate
    // guards below inspect the following lexical head.
    size_t end_pos = start_pos + result.length;

    if (result.entry->extended_pos == core::ExtendedPOS::NounFormal && followsNegativeQuote(lattice, start_pos) &&
        hasShorterMizenkeiBeforeNegative(dict_manager_, text, byteOffsetAt(byte_offsets, start_pos), codepoints,
                                         start_pos, result.length)) {
      continue;
    }
    if (result.entry->pos == core::PartOfSpeech::Verb && followsNegativeQuote(lattice, start_pos) &&
        hasLongerVerbBeforeNegative(dict_manager_, text, byteOffsetAt(byte_offsets, start_pos), codepoints, start_pos,
                                    result.length)) {
      continue;
    }
    if (result.entry->extended_pos == core::ExtendedPOS::VerbMizenkei && followsNegativeQuote(lattice, start_pos) &&
        hasShorterMizenkeiBeforeNegative(dict_manager_, text, byteOffsetAt(byte_offsets, start_pos), codepoints,
                                         start_pos, result.length)) {
      continue;
    }
    // An aspect auxiliary attaches to a te-form, never directly after the
    // contracted negative.  Keep the intervening quotative particle in
    // dialectal obligation frames (…せん+と+いけ+ん).
    if (result.entry->extended_pos == core::ExtendedPOS::AuxAspectOku && followsNegativeAuxiliary(lattice, start_pos)) {
      continue;
    }
    if (result.entry->extended_pos == core::ExtendedPOS::AuxNegativeNu && followsConjunctiveTeDe(lattice, start_pos)) {
      continue;
    }

    // A one-mora verb or auxiliary homograph at the tail of the polite copula
    // is not a morpheme boundary. Keeping it would
    // split the polite copula in ですって as で+すっ+て.  Ask the dictionary
    // directly rather than relying on lattice insertion order.
    if ((result.entry->pos == core::PartOfSpeech::Verb || result.entry->pos == core::PartOfSpeech::Auxiliary) &&
        verb_helpers::startsInsideDictionaryAuxiliary(codepoints, start_pos, &dict_manager_)) {
      continue;
    }

    // A regional aspect contraction does not span a word the dictionary
    // carries. 〜とる after an onbin is the ておる contraction (知っ+とる), but
    // the same two morae also close ordinary lexical verbs (のっとる, もどる),
    // and there the contraction is a coincidence of spelling that the
    // productive chain would otherwise win on connection bonuses alone.
    if (result.entry->extended_pos == core::ExtendedPOS::AuxAspectIru &&
        grammar::isDialectalOruContractionLemma(result.entry->lemma) &&
        endsDictionaryVerbSpanningBack(dict_manager_, codepoints, start_pos, end_pos)) {
      continue;
    }

    // なら is the irrealis of the classical copula なり only before the
    // classical negative (静か+なら+ず). In every other context this surface
    // is the modern conditional particle, so do not let the homograph replace
    // its stable POS/lemma analysis.
    if (result.entry->extended_pos == core::ExtendedPOS::AuxClassicalNari &&
        utf8::equalsAny(result.entry->surface, {"なら"}) &&
        (end_pos >= codepoints.size() || codepoints[end_pos] != U'ず')) {
      continue;
    }

    // A one-kanji formal-noun homograph cannot claim the tail of an ongoing
    // kanji compound immediately before an adverbial particle
    // (行為+やら, 当時+やら, 室内+やら). The particle identifies the complete
    // nominal boundary, while genuine productive formal-noun uses such as
    // 年度+末 and 期間+内 remain available in their ordinary contexts.
    if (result.entry->extended_pos == core::ExtendedPOS::NounFormal && result.length == 1 && start_pos > 0 &&
        normalize::isKanjiCodepoint(codepoints[start_pos - 1]) && end_pos < codepoints.size() &&
        lookupResultsHaveExtendedPOS(dict_manager_.lookup(text, byteOffsetAt(byte_offsets, end_pos)),
                                     core::ExtendedPOS::ParticleAdverbial)) {
      continue;
    }

    // A registered word is no more entitled to the material in front of the
    // nominalizer っこ than a constructed one is (で+きっ+こない for できっこない).
    if (verb_helpers::startsInsideGaMashiiSuffix(codepoints, start_pos)) {
      continue;
    }
    if (verb_helpers::crossesKkoNominalizer(codepoints, start_pos, end_pos)) {
      continue;
    }

    if (result.entry->pos == core::PartOfSpeech::Adverb &&
        overlapsCompleteIAdjectiveBeforeParticles(lattice, dict_manager_, codepoints, start_pos, end_pos)) {
      continue;
    }

    // Once the surrounding lattice proves the shortened causative-passive,
    // the homographic suru mizenkei is not grammatical at this boundary.
    // Removing it also prevents a list-particle reading of やら from reaching
    // the passive through the otherwise cheap する+れる connection.
    if (starts_shortened_causative_passive && result.length == 1 &&
        result.entry->extended_pos == core::ExtendedPOS::VerbMizenkei && grammar::isSuruBaseForm(result.entry->lemma)) {
      continue;
    }

    // A context-licensed particle must not absorb the beginning of a complete
    // following adverb (裏+で+しばらく, 時+は+すでに). Restricting the guard to
    // an observed left content/predicate edge avoids kana homographs inside
    // open words such as adjectives.
    if (result.entry->pos != core::PartOfSpeech::Particle &&
        !isLicensedCompletiveAuxiliaryBoundary(lattice, dict_manager_, text, byte_offsets, start_pos, end_pos,
                                               result.entry->extended_pos) &&
        joinsParticleToDictionaryAdverb(lattice, dict_manager_, text, byte_offsets, start_pos, end_pos,
                                        result.entry->extended_pos)) {
      continue;
    }

    // A dictionary terminal verb must yield to a longer, structurally valid
    // i-onbin stem immediately selected by て/で. This recovers open Godan-ka/
    // Godan-ga forms such as あるい+て without registering the lexical verb.
    if (result.entry->pos == core::PartOfSpeech::Verb && end_pos + 1 < codepoints.size() &&
        codepoints[end_pos] == U'い' && (codepoints[end_pos + 1] == U'て' || codepoints[end_pos + 1] == U'で')) {
      const std::string longer_stem = extractSubstring(codepoints, start_pos, end_pos + 1);
      const auto& longer_analyses = inflection_.analyze(longer_stem);
      const bool has_longer_ionbin = std::any_of(
          longer_analyses.begin(), longer_analyses.end(), [&](const grammar::InflectionCandidate& candidate) {
            return (candidate.verb_type == grammar::VerbType::GodanKa ||
                    candidate.verb_type == grammar::VerbType::GodanGa) &&
                   candidate.base_form != result.entry->lemma &&
                   candidate.confidence >= candidate::kParticleVerbBoundaryMinConfidence;
          });
      if (has_longer_ionbin) {
        continue;
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
      continue;
    }
    if (result.entry->pos == core::PartOfSpeech::Noun &&
        startsInsideSentenceInitialDictionaryNoun(dict_manager_, text, start_pos)) {
      continue;
    }
    // A registered noun beginning with a topic-particle homograph can absorb
    // that productive boundary after another nominal (そこ+は+にわ).  Require
    // both closed-class evidence for the first mora and L2 noun evidence for
    // the suffix, so a standalone lexical noun (はにわ) remains whole.
    if (result.entry->pos == core::PartOfSpeech::Noun && result.length > 1 && hasPrecedingNominal(lattice, start_pos) &&
        lookupResultsHaveExtendedPOS(lookup_results, core::ExtendedPOS::ParticleTopic, 1) &&
        lookupEntryInRange(dict_manager_, codepoints, start_pos + 1, end_pos, core::PartOfSpeech::Noun) != nullptr) {
      continue;
    }

    // A one-kanji na-adjective entry cannot begin inside a contiguous kanji
    // run. In that position it is the tail of the surrounding lexical noun
    // (音楽, 喜怒哀楽), not an independent predicate. At a real adjective
    // boundary the same entry begins the run (楽だ, 楽な仕事).
    if (result.entry->extended_pos == core::ExtendedPOS::AdjNaAdj && result.length == 1 && start_pos > 0 &&
        normalize::isKanjiCodepoint(codepoints[start_pos - 1])) {
      continue;
    }

    if (conflictsWithVerifiedCompoundBoundary(lattice, dict_manager_, text, byte_offsets, codepoints, start_pos,
                                              end_pos, result.entry->pos, result.entry->extended_pos)) {
      continue;
    }

    // A determiner must introduce a nominal constituent.  If a closed case
    // particle starts exactly where this candidate ends, the homographic
    // surface belongs to a compositional predicate instead (と+いう+より),
    // so do not admit the fused determiner path at all.  This is a category
    // constraint, independent of the individual determiner or particle.
    const bool ends_at_sentence_boundary =
        end_pos >= codepoints.size() || normalize::classifyChar(codepoints[end_pos]) == normalize::CharType::Symbol;
    if (result.entry->pos == core::PartOfSpeech::Determiner && ends_at_sentence_boundary &&
        isDictionaryOnbinPast(dict_manager_, result.entry->surface)) {
      continue;
    }
    if (result.entry->pos == core::PartOfSpeech::Determiner && ends_at_sentence_boundary) {
      constexpr PartOfSpeechMask kPredicateMask =
          partOfSpeechMask(core::PartOfSpeech::Verb) | partOfSpeechMask(core::PartOfSpeech::Adjective);
      const bool has_same_span_predicate = lookupResultsHavePartOfSpeech(lookup_results, kPredicateMask, result.length);
      if (has_same_span_predicate) {
        continue;
      }
    }
    if (result.entry->pos == core::PartOfSpeech::Determiner && ends_at_sentence_boundary &&
        hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::AuxCopulaDa) && start_pos >= 2 &&
        codepoints[start_pos - 2] == U'の' &&
        hasPrecedingExtendedPOS(lattice, start_pos - 1, core::ExtendedPOS::ParticleNo)) {
      continue;
    }
    if (result.entry->pos == core::PartOfSpeech::Determiner && end_pos < codepoints.size()) {
      const size_t following_byte_pos = byteOffsetAt(byte_offsets, end_pos);
      const auto following_results = dict_manager_.lookup(text, following_byte_pos);
      const bool followed_by_case_particle =
          lookupResultsHaveExtendedPOS(following_results, core::ExtendedPOS::ParticleCase);
      if (followed_by_case_particle) {
        continue;
      }
    }

    // When the same dictionary span has both noun and adverb readings, a
    // following nominal particle selects the noun use (一切+の/は/を).  Keep
    // the adverb when it directly modifies a predicate (一切+確認しない).
    if (result.entry->pos == core::PartOfSpeech::Adverb && end_pos < codepoints.size()) {
      const bool has_same_span_noun =
          lookupResultsHavePartOfSpeech(lookup_results, partOfSpeechMask(core::PartOfSpeech::Noun), result.length);
      const auto following_results = dict_manager_.lookup(text, byteOffsetAt(byte_offsets, end_pos));
      const bool followed_by_nominal_particle =
          std::any_of(following_results.begin(), following_results.end(), [](const auto& following) {
            if (following.entry == nullptr) {
              return false;
            }
            const auto extended_pos = following.entry->extended_pos;
            return extended_pos == core::ExtendedPOS::ParticleNo || extended_pos == core::ExtendedPOS::ParticleTopic ||
                   extended_pos == core::ExtendedPOS::ParticleCase;
          });
      const bool follows_genitive = hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::ParticleNo);
      if (has_same_span_noun && (followed_by_nominal_particle || follows_genitive)) {
        continue;
      }

      // An interrogative pronoun followed by a closed adverbial particle is
      // compositional before the genitive/nominalizer の (どれ+ほど+の...).
      // The same full-span adverb remains valid when it directly modifies a
      // predicate, so require both internal dictionary categories and the
      // right-hand nominal particle instead of naming any lexical surface.
      const bool followed_by_no = lookupResultsHaveExtendedPOS(following_results, core::ExtendedPOS::ParticleNo);
      if (followed_by_no) {
        bool has_interrogative_particle_split = false;
        for (const auto& prefix : lookup_results) {
          if (prefix.entry == nullptr || prefix.length >= result.length ||
              prefix.entry->extended_pos != core::ExtendedPOS::PronounInterrogative) {
            continue;
          }
          const size_t suffix_pos = start_pos + prefix.length;
          const auto suffix_results = dict_manager_.lookup(text, byteOffsetAt(byte_offsets, suffix_pos));
          has_interrogative_particle_split = lookupResultsHaveExtendedPOS(
              suffix_results, core::ExtendedPOS::ParticleAdverbial, result.length - prefix.length);
          if (has_interrogative_particle_split) {
            break;
          }
        }
        if (has_interrogative_particle_split) {
          continue;
        }
      }
    }

    // A formal-noun/adverb homograph directly before a predicate is the
    // adverbial reading unless an attributive predicate on the left licenses
    // the formal noun (考えすぎた+あまり+眠れない).  Retain the nominal
    // reading before case/topic/genitive particles and copulas so independent
    // noun uses remain available.  This resolves the grammatical category by
    // its two constructional environments rather than by lexical surface.
    if (result.entry->extended_pos == core::ExtendedPOS::NounFormal && end_pos < codepoints.size()) {
      const bool has_same_span_adverb =
          lookupResultsHavePartOfSpeech(lookup_results, partOfSpeechMask(core::PartOfSpeech::Adverb), result.length);
      const bool follows_genitive = hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::ParticleNo);
      const bool follows_non_genitive_nominal_particle =
          hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::ParticleCase) ||
          hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::ParticleTopic) ||
          hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::ParticleBinding) ||
          hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::ParticleAdverbial);
      const bool has_formal_noun_left_context =
          follows_genitive ||
          (hasPrecedingAttributivePredicate(lattice, start_pos) && !follows_non_genitive_nominal_particle);
      if (has_same_span_adverb && !has_formal_noun_left_context) {
        const auto following_results = dict_manager_.lookup(text, byteOffsetAt(byte_offsets, end_pos));
        const bool followed_by_nominal_marker =
            std::any_of(following_results.begin(), following_results.end(), [](const auto& following) {
              if (following.entry == nullptr) {
                return false;
              }
              const auto extended_pos = following.entry->extended_pos;
              return extended_pos == core::ExtendedPOS::ParticleNo ||
                     extended_pos == core::ExtendedPOS::ParticleTopic ||
                     extended_pos == core::ExtendedPOS::ParticleCase ||
                     extended_pos == core::ExtendedPOS::ParticleBinding ||
                     extended_pos == core::ExtendedPOS::ParticleAdverbial ||
                     (extended_pos == core::ExtendedPOS::AuxCopulaDa &&
                      !grammar::isSingleHiragana(following.entry->surface, core::hiragana::kNa)) ||
                     extended_pos == core::ExtendedPOS::AuxCopulaDesu;
            });
        if (!followed_by_nominal_marker) {
          continue;
        }
      }
    }

    if (result.entry->extended_pos == core::ExtendedPOS::AuxTenseMasu &&
        utf8::equalsAny(result.entry->surface, {"まし"}) && end_pos < codepoints.size() &&
        codepoints[end_pos] == U'て' && hasCoveringVerifiedVerbRenyokei(lattice, start_pos, end_pos)) {
      continue;
    }

    // A closed interval suffix can be homographic with a verb continuative
    // (1時間+おき).  After a verified number expression, select the suffix
    // only in a nominal environment; an auxiliary continuation such as
    // 1時間+おき+ます keeps the verb candidate.
    if (result.entry->pos == core::PartOfSpeech::Verb &&
        hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::NounNumber)) {
      const bool has_same_span_suffix =
          lookupResultsHavePartOfSpeech(lookup_results, partOfSpeechMask(core::PartOfSpeech::Suffix), result.length);
      bool has_nominal_right_context = end_pos >= codepoints.size();
      if (!has_nominal_right_context && normalize::classifyChar(codepoints[end_pos]) == normalize::CharType::Symbol) {
        has_nominal_right_context = true;
      }
      if (!has_nominal_right_context) {
        const auto following_results = dict_manager_.lookup(text, byteOffsetAt(byte_offsets, end_pos));
        has_nominal_right_context =
            lookupResultsHavePartOfSpeech(following_results, partOfSpeechMask(core::PartOfSpeech::Particle));
      }
      if (has_same_span_suffix && has_nominal_right_context) {
        continue;
      }
    }

    // Nominalizing/final-particle homographs of さ cannot occur between a verb
    // mizenkei and a passive auxiliary.  In a causative-passive chain
    // (読ま+さ+れ, 考え込ま+さ+れ), keeping either homograph creates a
    // spurious adjective path which can defeat the generated verb candidate.
    if (result.entry->extended_pos != core::ExtendedPOS::VerbMizenkei && result.length == 1 &&
        codepoints[start_pos] == core::hiragana::kSa && result.entry->pos != core::PartOfSpeech::Verb &&
        hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbMizenkei) && end_pos < codepoints.size()) {
      const size_t following_byte_pos = byteOffsetAt(byte_offsets, end_pos);
      const auto following_results = dict_manager_.lookup(text, following_byte_pos);
      const bool followed_by_passive = lookupResultsHaveExtendedPOS(following_results, core::ExtendedPOS::AuxPassive);
      if (followed_by_passive) {
        continue;
      }
    }

    // Resolve dictionary homographs from a closed na-adjective continuation.
    // When the same full surface has an AdjNaAdj entry, attributive な,
    // adverbial に, and appearance そう select that entry rather than the noun
    // homograph. Noun-only words remain untouched.
    if (result.entry->pos == core::PartOfSpeech::Noun && end_pos < codepoints.size()) {
      const bool has_same_surface_na_adjective =
          lookupResultsHaveExtendedPOS(lookup_results, core::ExtendedPOS::AdjNaAdj, result.length);
      const bool na_adjective_continuation =
          codepoints[end_pos] == U'に' ||
          (codepoints[end_pos] == U'な' && (end_pos + 1 >= codepoints.size() || codepoints[end_pos + 1] != U'ら')) ||
          (end_pos + 1 < codepoints.size() && codepoints[end_pos] == U'そ' && codepoints[end_pos + 1] == U'う');
      if (has_same_surface_na_adjective && na_adjective_continuation) {
        continue;
      }
    }
    // The reverse side of the same lexical homograph contract: when a surface
    // is explicitly registered as both a noun and a na-adjective, a predicative
    // copula selects its nominal reading. Adjective-only entries remain
    // adjectives before the same copula.
    if (result.entry->extended_pos == core::ExtendedPOS::AdjNaAdj && end_pos < codepoints.size()) {
      const bool has_same_surface_noun =
          lookupResultsHavePartOfSpeech(lookup_results, partOfSpeechMask(core::PartOfSpeech::Noun), result.length);
      if (has_same_surface_noun && grammar::startsPredicativeCopula(text.substr(byteOffsetAt(byte_offsets, end_pos)))) {
        continue;
      }
    }

    // A shorter adverb prefix cannot split a longer dictionary na-adjective
    // immediately before attributive な (めちゃくちゃな, もっともな).
    if (result.entry->pos == core::PartOfSpeech::Adverb) {
      const bool longer_attributive_na_adjective =
          std::any_of(lookup_results.begin(), lookup_results.end(), [&](const auto& other) {
            const size_t other_end = start_pos + other.length;
            return other.entry != nullptr && other.length > result.length &&
                   other.entry->extended_pos == core::ExtendedPOS::AdjNaAdj && other_end < codepoints.size() &&
                   codepoints[other_end] == U'な';
          });
      if (longer_attributive_na_adjective) {
        continue;
      }
    }

    const bool follows_volitional = hasPrecedingVerbVolitionalChain(lattice, start_pos);
    const auto starts_aspectual_iru = [&](size_t pos) {
      if (pos >= codepoints.size()) {
        return false;
      }
      const auto following = dict_manager_.lookup(text, byteOffsetAt(byte_offsets, pos));
      return std::any_of(following.begin(), following.end(), [](const auto& candidate) {
        return candidate.entry != nullptr && candidate.entry->lemma == "いる";
      });
    };

    // Prefer the longest member only within the same particle class. This
    // keeps closed concessives such as ども/けれども intact without
    // suppressing productive boundaries whose shorter member has another
    // grammatical role (で+も, と+も).
    if (result.entry->pos == core::PartOfSpeech::Particle) {
      // A compound case particle ends an adpositional phrase and cannot host
      // the aspectual いる. When that continuation is
      // present, keep the shorter internal case-particle boundary so the
      // adjoining verb te-form can carry the auxiliary (目を+通し+て+いる).
      const bool compound_particle_before_aspect = result.entry->extended_pos == core::ExtendedPOS::ParticleCase &&
                                                   result.length > 1 && starts_aspectual_iru(end_pos);
      if (compound_particle_before_aspect) {
        continue;
      }
      // After an explicit volitional auxiliary, a multi-mora case particle
      // would hide the productive quotative + suru sequence
      // (書こ+う+と+し+て). Keep the one-mora quotative candidate even when a
      // longer case-particle entry shares its prefix. Only an entry opening on
      // that same quotative mora can hide it; a compound particle beginning
      // anywhere else shares nothing with the sequence and stays available
      // (いかん+によって, where the ん also reads as the literary volitional).
      if (follows_volitional && result.entry->extended_pos == core::ExtendedPOS::ParticleCase && result.length > 1 &&
          utf8::decodeFirstChar(result.entry->surface) == core::hiragana::kTo) {
        continue;
      }
      const bool has_longer_same_class =
          std::any_of(lookup_results.begin(), lookup_results.end(), [&](const auto& other) {
            const size_t other_end = start_pos + other.length;
            return other.entry != nullptr && other.entry->pos == core::PartOfSpeech::Particle &&
                   other.entry->extended_pos == result.entry->extended_pos && other.length > result.length &&
                   !(other.entry->extended_pos == core::ExtendedPOS::ParticleCase && other.length > 1 &&
                     starts_aspectual_iru(other_end));
          });
      const bool keep_interrogative_quotative =
          result.entry->extended_pos == core::ExtendedPOS::ParticleCase && result.length == 1 &&
          grammar::isSingleHiragana(result.entry->surface, core::hiragana::kTo) &&
          std::any_of(lookup_results.begin(), lookup_results.end(),
                      [&](const auto& other) {
                        const size_t other_end = start_pos + other.length;
                        return other.entry != nullptr && other.entry->extended_pos == core::ExtendedPOS::ParticleCase &&
                               grammar::isQuotativeSuruTeCompoundParticle(other.entry->surface) &&
                               other_end < codepoints.size() && codepoints[other_end] == U'も';
                      }) &&
          hasInterrogativeEndingAt(dict_manager_, text, byte_offsets, start_pos);
      const bool keep_volitional_quotative =
          follows_volitional && result.entry->extended_pos == core::ExtendedPOS::ParticleCase && result.length == 1;
      if (has_longer_same_class && !keep_volitional_quotative && !keep_interrogative_quotative) {
        continue;
      }
    }

    if (result.entry->pos == core::PartOfSpeech::Adverb && result.length > 1 && codepoints[end_pos - 1] == U'か' &&
        end_pos + 2 < codepoints.size() && codepoints[end_pos] == U'と' && codepoints[end_pos + 1] == U'い' &&
        codepoints[end_pos + 2] == U'う') {
      continue;
    }

    // A lexical adverb homographic with a dictionary-verified verb te-form
    // cannot govern the progressive auxiliary いる. Preserve the verb stem +
    // connective boundary in that environment while leaving ordinary adverb
    // uses untouched.
    if (result.entry->pos == core::PartOfSpeech::Adverb && end_pos + 1 < codepoints.size() &&
        codepoints[end_pos] == U'い' && codepoints[end_pos + 1] == U'る' &&
        utf8::endsWithAny(result.entry->surface, {"て", "で"})) {
      bool is_verified_verb_te_form = false;
      for (const auto& inflection_candidate : inflection_.analyze(result.entry->surface)) {
        if (inflection_candidate.verb_type != grammar::VerbType::IAdjective &&
            (verb_helpers::isVerbInDictionary(&dict_manager_, inflection_candidate.base_form) ||
             inflection_candidate.confidence >= candidate::kAdverbVerbTeHomographMinConfidence)) {
          is_verified_verb_te_form = true;
          break;
        }
      }
      if (is_verified_verb_te_form) {
        continue;
      }
    }

    // A conjunction that is also a productive verb+particle sequence is
    // lexical only at a clause boundary. Inside a phrase, keep the ordinary
    // predicate boundary (もしか+する+と).
    if (result.entry->pos == core::PartOfSpeech::Conjunction && start_pos > 0) {
      if (crossesAttributiveNaHonorificNominal(lattice, codepoints, start_pos, end_pos)) {
        continue;
      }
      bool decomposes_as_verb_particle = false;
      for (size_t split = 1; split < result.length; ++split) {
        if (lookupEntryInRange(dict_manager_, codepoints, start_pos, start_pos + split, core::PartOfSpeech::Verb) !=
                nullptr &&
            lookupEntryInRange(dict_manager_, codepoints, start_pos + split, end_pos, core::PartOfSpeech::Particle) !=
                nullptr) {
          decomposes_as_verb_particle = true;
          break;
        }
      }
      const bool coordinates_nominals = hasPrecedingNominal(lattice, start_pos) && end_pos < codepoints.size() &&
                                        (normalize::isKanjiCodepoint(codepoints[end_pos]) ||
                                         normalize::classifyChar(codepoints[end_pos]) == normalize::CharType::Katakana);
      const bool follows_completed_clause = hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::AuxTenseTa);
      if (decomposes_as_verb_particle && !coordinates_nominals && !follows_completed_clause &&
          normalize::classifyChar(codepoints[start_pos - 1]) != normalize::CharType::Symbol) {
        continue;
      }
    }

    // A conjunction must not absorb a dictionary-verified te-form immediately
    // before the conditional directional 来る. The latter is a productive
    // predicate chain (持っ+て+くれ+ば), while the conjunction cannot govern
    // that auxiliary inflection.
    if (result.entry->pos == core::PartOfSpeech::Conjunction &&
        isDictionaryOnbinTeForm(dict_manager_, result.entry->surface) && startsKuruConditional(codepoints, end_pos)) {
      continue;
    }

    if (result.length > 1 && startsParticleBeforeReduplicatedMimetic(codepoints, start_pos) &&
        end_pos > start_pos + 1) {
      continue;
    }

    if (result.entry->pos == core::PartOfSpeech::Adverb && result.length == 2 &&
        startsIruPoliteFormAt(codepoints, start_pos)) {
      continue;
    }

    if (result.entry->pos == core::PartOfSpeech::Adverb &&
        startsInsideVerifiedPredicate(lattice, codepoints, start_pos)) {
      continue;
    }

    // The lexical temporal adverb can follow a verified compound boundary,
    // but must not reopen the final 間 of a shorter duration noun.
    if (result.entry->pos == core::PartOfSpeech::Adverb && codepoints[start_pos] == U'間' && start_pos > 0 &&
        normalize::isKanjiCodepoint(codepoints[start_pos - 1]) &&
        !hasPrecedingTemporalCompoundBoundary(lattice, start_pos)) {
      continue;
    }

    if (result.entry->pos == core::PartOfSpeech::Adverb &&
        opensOnContentWordTailBeforeParticle(lattice, dict_manager_, codepoints, start_pos, end_pos)) {
      continue;
    }

    // Do not reopen the interior of a kanji-led verb as a pure-hiragana
    // dictionary na-adjective. The same adjective remains available at a real
    // boundary (sentence start or after a particle).
    if (result.entry->extended_pos == core::ExtendedPOS::AdjNaAdj && grammar::isPureHiragana(result.entry->surface) &&
        startsInsideKanjiLedVerb(lattice, codepoints, start_pos)) {
      continue;
    }

    // A period suffix cannot head an interval compound.  In a numeral-led
    // expression such as 10分間隔, the counter generator already supplies
    // 10分 and the following lexical noun must remain 間隔, not 間+隔.
    if (result.entry->extended_pos == core::ExtendedPOS::Suffix && result.length == 1 &&
        start_pos + 1 < codepoints.size() && codepoints[start_pos] == U'間' &&
        normalize::isIntervalCompoundSecondKanji(codepoints[start_pos + 1])) {
      continue;
    }

    // 時間接尾辞「後」は終了+後・三日+後のように内容語へ直接接合
    // する。ひらがな活用や助詞の後では独立時間名詞なので、suffix
    // edgeを出さず既存のnoun候補へ任せる（食べた+後、ので+後）。
    if (result.entry->extended_pos == core::ExtendedPOS::Suffix &&
        grammar::isDirectAttachmentTemporalSuffix(result.entry->surface)) {
      if (start_pos == 0) {
        continue;
      }
      const auto preceding_type = normalize::classifyChar(codepoints[start_pos - 1]);
      const bool directly_attached_to_nominal =
          preceding_type == normalize::CharType::Kanji || preceding_type == normalize::CharType::Katakana ||
          preceding_type == normalize::CharType::Alphabet || preceding_type == normalize::CharType::Digit;
      if (!directly_attached_to_nominal) {
        continue;
      }
    }

    // A one-kanji formal noun cannot head an adjacent kanji compound.  The
    // formal reading remains available at a word boundary (ない+事), while a
    // lexical compound such as 事情 or 事実 keeps its complete search unit.
    if (result.entry->extended_pos == core::ExtendedPOS::NounFormal && result.length == 1 &&
        end_pos < codepoints.size() && normalize::isKanjiCodepoint(codepoints[end_pos]) &&
        !isKanjiRunFollowedByAttributiveNa(codepoints, end_pos)) {
      continue;
    }

    // わりに is an adverb at clause start, but after an attributive の or a
    // finite predicate it is the formal noun わり followed by the case
    // particle に (本の+わりに, 読む+わりに). Before an adjective it instead
    // forms the fixed comparative adverb (年齢の+わりに+若い).
    if (result.entry->pos == core::PartOfSpeech::Adverb && result.entry->lemma == "わりに" && start_pos > 0) {
      if (startsInsideKanjiLedVerb(lattice, codepoints, start_pos)) {
        continue;
      }
      const char32_t preceding = codepoints[start_pos - 1];
      const bool followed_by_adjective =
          end_pos < codepoints.size() &&
          lookupResultsHavePartOfSpeech(dict_manager_.lookup(text, byteOffsetAt(byte_offsets, end_pos)),
                                        partOfSpeechMask(core::PartOfSpeech::Adjective));
      if (!followed_by_adjective &&
          (preceding == U'の' || preceding == U'る' || preceding == U'く' || preceding == U'む' || preceding == U'ぶ' ||
           preceding == U'ぬ' || preceding == U'す' || preceding == U'つ' || preceding == U'ぐ')) {
        continue;
      }
    }

    if (result.entry->pos == core::PartOfSpeech::Adverb &&
        startsFormalNounParticleAfterPredicate(lattice, dict_manager_, codepoints, start_pos, end_pos)) {
      continue;
    }

    const bool fused_demo_after_te_form = result.length == 2 && codepoints[start_pos] == U'で' &&
                                          codepoints[start_pos + 1] == U'も' &&
                                          hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbOnbinkei);
    if (fused_demo_after_te_form) {
      continue;
    }

    if (suppress_prefixed_noun_interior) {
      continue;
    }

    // Prefer the maximal closed-class conjunction at this position: 又は,
    // not 又+は. Shorter prefixes remain available when no longer conjunction
    // matches the input.
    if (result.entry->pos == core::PartOfSpeech::Conjunction && result.length < longest_conjunction) {
      continue;
    }

    // Members of the closed adverb lexicon use maximal matching within their
    // own class (必ずしも, どうしても). Shorter dictionary adverbs remain
    // available whenever no longer adverb actually covers the input.
    if (result.entry->pos == core::PartOfSpeech::Adverb && result.length < longest_adverb) {
      continue;
    }

    // A complete member of the closed potential-benefactive paradigm is
    // authoritative over shorter homographs starting at the same position.
    // This keeps いただけ(る/ない/ます) from reopening as い+た+だけ while
    // leaving every position without that exact closed-class match untouched.
    if (result.length < longest_potential_benefactive) {
      continue;
    }

    // At sentence start, a longer closed-class conjunction takes precedence
    // over a homographic auxiliary prefix.  After a topic/focus particle,
    // suppress only the polite auxiliary prefix: ます requires a verb
    // renyokei, so に+も+まし+て cannot be a polite chain.  Other auxiliaries
    // remain available (本+も+だ+けど).
    const bool sentence_initial_auxiliary = start_pos == 0 && result.entry->pos == core::PartOfSpeech::Auxiliary;
    const bool unlicensed_polite_after_topic =
        result.entry->extended_pos == core::ExtendedPOS::AuxTenseMasu &&
        hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::ParticleTopic);
    if ((sentence_initial_auxiliary || unlicensed_polite_after_topic) && result.length < longest_fixed_conjunction) {
      continue;
    }

    // A bound derivational suffix verb has no independent use, so without a
    // nominal host in front the entry is not a candidate at all.
    if (result.entry->pos == core::PartOfSpeech::Verb &&
        grammar::isBoundDerivationalSuffixVerbLemma(result.entry->lemma) &&
        !verb_helpers::hasNominalHostBefore(codepoints, start_pos)) {
      continue;
    }

    // Past た/だ is an auxiliary boundary, not part of a dictionary verb
    // token.  Inflected dictionary entries still provide the stem/onbin edge;
    // discard only the fused full-past alternative.
    if (result.entry->extended_pos == core::ExtendedPOS::VerbTaForm &&
        utf8::endsWithAny(result.entry->surface, {"た", "だ"})) {
      continue;
    }

    if (result.entry->pos == core::PartOfSpeech::Verb && utf8::endsWith(result.entry->surface, "ぬ") &&
        result.entry->lemma != result.entry->surface) {
      const std::string stem_surface = std::string(utf8::dropLastChar(result.entry->surface));
      lattice.addEdge(stem_surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(end_pos - 1),
                      core::PartOfSpeech::Verb, getCategoryCost(core::ExtendedPOS::VerbMizenkei),
                      core::LatticeEdge::kFromDictionary, result.entry->lemma, dictionary::ConjugationType::None,
                      core::CandidateOrigin::Dictionary, candidate::kDictionaryOriginConfidence, {},
                      core::ExtendedPOS::VerbMizenkei, "dictionary_classical_negative_stem");
      continue;
    }

    if ((result.entry->pos == core::PartOfSpeech::Verb || result.entry->pos == core::PartOfSpeech::Adjective ||
         result.entry->pos == core::PartOfSpeech::Noun) &&
        result.length > 1 && codepoints[start_pos] == U'は' && codepoints[end_pos - 1] == U'な' &&
        end_pos + 1 < codepoints.size() && codepoints[end_pos] == U'か' && codepoints[end_pos + 1] == U'っ') {
      continue;
    }

    if (result.entry->pos == core::PartOfSpeech::Noun && end_pos < codepoints.size() &&
        codepoints[end_pos - 1] == U'し' && codepoints[end_pos] == U'て') {
      const std::string verb_base = normalize::concat(utf8::dropLastChar(result.entry->surface), "す");
      const auto* verb = dict_manager_.lookupExact(verb_base, core::PartOfSpeech::Verb);
      if (verb != nullptr) {
        lattice.addEdge(result.entry->surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(end_pos),
                        core::PartOfSpeech::Verb,
                        getCategoryCost(core::ExtendedPOS::VerbRenyokei) + candidate::kVerifiedTailCompoundVerbBonus +
                            candidate::kVerifiedVerbBonus,
                        core::LatticeEdge::kFromDictionary, verb_base, dictionary::ConjugationType::GodanSa,
                        core::CandidateOrigin::Dictionary, candidate::kDictionaryOriginConfidence, {},
                        core::ExtendedPOS::VerbRenyokei, "dictionary_godan_sa_renyokei");
      }
    }

    if (result.entry->pos == core::PartOfSpeech::Noun && end_pos < codepoints.size() &&
        normalize::isKanjiCodepoint(codepoints[end_pos]) && grammar::isIRowCodepoint(codepoints[end_pos - 1])) {
      const std::string_view base_suffix = grammar::godanBaseSuffixFromIRow(codepoints[end_pos - 1]);
      if (!base_suffix.empty()) {
        const std::string verb_base = normalize::concat(utf8::dropLastChar(result.entry->surface), base_suffix);
        const auto* verb = dict_manager_.lookupExact(verb_base, core::PartOfSpeech::Verb);
        if (verb != nullptr) {
          const auto conj_type = grammar::verbTypeToConjType(
              grammar::verbTypeFromBaseCodepoint(utf8::decodeFirstChar(utf8::lastChar(verb_base))));
          lattice.addEdge(result.entry->surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(end_pos),
                          core::PartOfSpeech::Verb, getCategoryCost(core::ExtendedPOS::VerbRenyokei),
                          core::LatticeEdge::kFromDictionary, verb_base, conj_type, core::CandidateOrigin::Dictionary,
                          candidate::kDictionaryOriginConfidence, {}, core::ExtendedPOS::VerbRenyokei,
                          "dictionary_godan_renyokei_before_predicate");
        }
      }
    }

    if (result.entry->extended_pos == core::ExtendedPOS::AuxInability &&
        !hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbRenyokei)) {
      continue;
    }

    // The one-mora classical desiderative auxiliary ま is valid only as the
    // first component of まほしき.  Keeping it context-gated prevents a
    // common temporal adverb such as いま from being split as い+ま.
    if (result.entry->extended_pos == core::ExtendedPOS::AuxDesireTai &&
        grammar::isClassicalDesiderativeMarker(result.entry->surface) &&
        !grammar::startsClassicalDesiderativeSequence(text.substr(byteOffsetAt(byte_offsets, start_pos)))) {
      continue;
    }

    // The classical honorific たまふ is represented as た+ま+ふ.  Its
    // one-mora pieces are admitted only inside that exact auxiliary chain.
    if (result.entry->extended_pos == core::ExtendedPOS::AuxHonorific &&
        grammar::isClassicalHonorificComponent(result.entry->surface)) {
      const bool is_marker = grammar::isClassicalDesiderativeMarker(result.entry->surface);
      const bool has_honorific_start =
          grammar::startsClassicalHonorificSequence(text.substr(byteOffsetAt(byte_offsets, start_pos)));
      const bool follows_honorific_marker = start_pos > 0 && grammar::isClassicalDesiderativeMarker(extractSubstring(
                                                                 codepoints, start_pos - 1, start_pos));
      if ((is_marker && !has_honorific_start) || (!is_marker && !follows_honorific_marker)) {
        continue;
      }
    }

    // The classical past keeps only its 連体形 し and 已然形 しか, so each has
    // exactly one environment: し modifies a following nominal or closes the
    // clause (読みし人, 読まざりし。) and しか takes the conditional particle
    // (見しかば).  Anywhere else the same kana is the サ変 continuative
    // (消し+ます, 落ち+し+て).
    // The classical perfect たり contributes its own 已然形 たれ, which needs the
    // same conjunctive particle (記録したれ+ども).
    const bool classical_perfect_izenkei = result.entry->extended_pos == core::ExtendedPOS::AuxClassicalPerfect &&
                                           grammar::spellsHypotheticalAuxiliaryCell(result.entry->surface);
    const bool is_classical_izenkei = classical_perfect_izenkei || end_pos - start_pos > 1;
    // 係り結び leaves the 已然形 as the clause's own predicate, so the cell also
    // stands with no particle after it at all (雨こそ降りたれ, 月を見しか). What
    // marks it there is the continuative it attaches to, not the follower: a
    // case particle in that slot leaves the same kana as the ordinary noun it
    // introduces (料理に+たれ, 背も+たれ), and requiring the binding particle
    // itself would reject the same cell wherever the clause carries no 係助詞,
    // which is the reading the oracle takes (彼が知り+たれ). The continuative may
    // belong to an auxiliary rather than the verb, because a voice auxiliary
    // hosts the perfect from the same cell (開か+れ+たれ).
    const bool follows_continuative =
        hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbRenyokei) ||
        hasPrecedingPartOfSpeech(lattice, start_pos, partOfSpeechMask(core::PartOfSpeech::Auxiliary));
    const bool izenkei_closes_clause =
        is_classical_izenkei && verb_helpers::clauseEndsAt(codepoints, end_pos) && follows_continuative;
    // The 連体形 also nominalizes, and the nominal it forms takes a particle of
    // its own (告げぬべかりし+に, 読みし+を). The host separates that from the サ変
    // continuative the same kana spells: the classical past attaches to a
    // continuative, while the サ変 verb takes the nominal it turns into a
    // predicate, or the particle that introduces one (話を+し+に行く).
    const bool rentaikei_nominalizes = !is_classical_izenkei && follows_continuative &&
                                       verb_helpers::caseParticleFollowsAt(dict_manager_, codepoints, end_pos);
    if ((result.entry->extended_pos == core::ExtendedPOS::AuxClassicalKi || classical_perfect_izenkei) &&
        !izenkei_closes_clause && !rentaikei_nominalizes &&
        !verb_helpers::classicalPastEnvironmentFollows(dict_manager_, codepoints, end_pos, is_classical_izenkei)) {
      continue;
    }

    // A one-mora classical perfect is the tail of far more words than it is an
    // auxiliary (待つ, 一つ, いつの間にか), so it is admitted only where the
    // paradigm cell it attaches to actually precedes it. The realis is evidence
    // enough on its own, because り is the only auxiliary that takes it
    // (行け+り). A continuative precedes half the lattice, so the terminal つ
    // additionally needs the clause end its form implies (書き+つ).
    if (result.entry->extended_pos == core::ExtendedPOS::AuxClassicalPerfect && end_pos == start_pos + 1 &&
        !hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbKateikei) &&
        !(hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbRenyokei) &&
          verb_helpers::classicalPastEnvironmentFollows(dict_manager_, codepoints, end_pos, false))) {
      continue;
    }

    // An interjection is an utterance of its own, closed by punctuation or by a
    // change of script rather than continued by more kana. Where its surface is
    // also the irrealis of a dictionary verb, that verb owns the paradigm behind
    // it (あら、素敵ね keeps the interjection; あらう, あらば, あらゆる stay with
    // ある). Interjections with no such reading are unaffected.
    if (result.entry->pos == core::PartOfSpeech::Interjection && end_pos < codepoints.size() &&
        end_pos > start_pos + 1 && normalize::classifyChar(codepoints[end_pos]) == normalize::CharType::Hiragana) {
      const std::string_view base_suffix = grammar::godanBaseSuffixFromARow(codepoints[end_pos - 1]);
      if (!base_suffix.empty() &&
          dict_manager_.lookupExact(
              normalize::concat(extractSubstring(codepoints, start_pos, end_pos - 1), base_suffix),
              core::PartOfSpeech::Verb) != nullptr) {
        continue;
      }
    }

    // A 終助詞 closes its clause, so the nominalizer cannot follow it. The な in
    // そう+な+ん+です is the copula's attributive form instead; the indefinite
    // stack the bigram favors (いくつ+か+の) uses the の spelling and is untouched.
    if (result.entry->extended_pos == core::ExtendedPOS::ParticleFinal && end_pos < codepoints.size() &&
        codepoints[end_pos] == U'ん') {
      continue;
    }

    // The contracted directional く is the いく renyokei with its い elided, so
    // it exists only directly after a te-form (読ん+で+く).  Anywhere else the
    // same single kana is an adjective continuative or a stem fragment, and
    // admitting the auxiliary there splits the negative continuative (な+く for
    // 書か+なく+ない).
    if (result.entry->extended_pos == core::ExtendedPOS::AuxAspectIku && end_pos - start_pos == 1 &&
        (start_pos == 0 || (codepoints[start_pos - 1] != U'て' && codepoints[start_pos - 1] != U'で'))) {
      continue;
    }

    // A 副助詞 attaches to a 体言 and a 接続詞 opens a clause; neither follows a
    // verb onbin stem.  Where one that begins with だ appears to (読ん+だって,
    // 読ん+だから), the だ is the voiced past auxiliary and the rest is its own
    // word (読ん+だ+って).  The hatsuonbin shape is kanji + ん, which keeps an
    // ordinary noun ending in ん (みかん+だって) and every other left context
    // untouched.
    if ((result.entry->extended_pos == core::ExtendedPOS::ParticleAdverbial ||
         result.entry->pos == core::PartOfSpeech::Conjunction) &&
        start_pos >= 2 && codepoints[start_pos] == U'だ' && codepoints[start_pos - 1] == U'ん' &&
        normalize::isKanjiCodepoint(codepoints[start_pos - 2])) {
      continue;
    }

    // A pure-hiragana adnominal begins with a kana that is also an inflectional
    // ending, so it cannot start where a productive verb continuative already
    // straddles the boundary (書き+たる, たなびき+たる).  A real boundary
    // immediately before the determiner and unrelated kana contexts remain
    // untouched.
    if (result.entry->pos == core::PartOfSpeech::Determiner &&
        hasProductiveContinuativeCrossingDeterminer(lattice, inflection_, dict_manager_, codepoints, start_pos)) {
      continue;
    }

    // The historical terminal component ふ is meaningful only after a kanji
    // stem.  The positional gate retains separations such as 候+ふ and 思+ふ
    // without admitting a free one-mora verb in ordinary hiragana text.
    if (result.entry->pos == core::PartOfSpeech::Verb &&
        result.entry->extended_pos == core::ExtendedPOS::VerbShuushikei &&
        grammar::isClassicalFuruTerminal(result.entry->surface) &&
        (start_pos == 0 || !normalize::isKanjiCodepoint(codepoints[start_pos - 1]))) {
      continue;
    }

    // A dictionary noun homographic with a verb renyokei (知らせ) cannot
    // precede the closed classical honorific auxiliary chain たまふ.  Keep the
    // verb boundary available in that grammatical environment.
    if (result.entry->pos == core::PartOfSpeech::Noun &&
        grammar::startsClassicalHonorificAuxiliaryChain(text.substr(byteOffsetAt(byte_offsets, end_pos)))) {
      continue;
    }

    // In an interrogative emphatic sequence, として is not the viewpoint
    // compound particle: it is と+し+て before the focus particle も.
    if (result.entry->extended_pos == core::ExtendedPOS::ParticleCase &&
        grammar::isQuotativeSuruTeCompoundParticle(result.entry->surface) && end_pos < codepoints.size() &&
        codepoints[end_pos] == U'も' && hasInterrogativeEndingAt(dict_manager_, text, byte_offsets, start_pos)) {
      continue;
    }

    // The contracted preparative auxiliary has a genuine mizenkei+volitional
    // cell (とこ+う / どこ+う), but those spellings are also ordinary lexical
    // words. Emit them only in their complete verb-onbin auxiliary context.
    // Other AuxAspectOku forms before う are the invalid とい+う path.
    if (result.entry->extended_pos == core::ExtendedPOS::AuxAspectOku) {
      const bool follows_volitional = end_pos < codepoints.size() && codepoints[end_pos] == U'う';
      // The contraction is て + おく, so its host is whichever cell that て
      // selects: the onbin form of a Godan verb (書い+とこう) but the plain
      // continuative of an Ichidan or サ変 one (見+とこう, 作成し+とこう).
      // Admitting only the onbin cell left the other two conjugations to fall
      // back on the case particle plus the homographic adverb.
      const bool contracted_volitional =
          utf8::equalsAny(result.entry->surface, {"とこ", "どこ"}) && follows_volitional &&
          (hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbOnbinkei) ||
           hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbRenyokei));
      if ((utf8::equalsAny(result.entry->surface, {"とこ", "どこ"}) && !contracted_volitional) ||
          (follows_volitional && !contracted_volitional)) {
        continue;
      }
    }

    // At the beginning of a clause, a one-mora continuative cannot steal the
    // first mora of a longer dictionary conjunction (しかも, しかし). The
    // conjunction is already a complete closed-class candidate at this
    // boundary; letting its prefix reach a following particle manufactures a
    // predicate with no host.
    if (start_pos == 0 && result.entry->extended_pos == core::ExtendedPOS::VerbRenyokei && result.length == 1) {
      const auto same_start_entries = dict_manager_.lookup(text, byteOffsetAt(byte_offsets, start_pos));
      const bool has_longer_conjunction =
          std::any_of(same_start_entries.begin(), same_start_entries.end(), [&](const auto& candidate) {
            return candidate.entry != nullptr && candidate.entry->pos == core::PartOfSpeech::Conjunction &&
                   candidate.length > result.length;
          });
      if (has_longer_conjunction) {
        continue;
      }
    }

    // A conjunction introduces a new predicate. Do not start that predicate
    // with a one-character nominal/suffix homograph when a longer dictionary
    // verb begins at the same boundary (しかも+間違えた, not しかも+間+違えた).
    if ((result.entry->pos == core::PartOfSpeech::Noun || result.entry->pos == core::PartOfSpeech::Suffix) &&
        result.length == 1 &&
        hasPrecedingPartOfSpeech(lattice, start_pos, partOfSpeechMask(core::PartOfSpeech::Conjunction))) {
      const auto same_start_entries = dict_manager_.lookup(text, byteOffsetAt(byte_offsets, start_pos));
      const bool has_longer_verb =
          std::any_of(same_start_entries.begin(), same_start_entries.end(), [&](const auto& candidate) {
            return candidate.entry != nullptr && candidate.entry->pos == core::PartOfSpeech::Verb &&
                   candidate.length > result.length;
          });
      if (has_longer_verb) {
        continue;
      }
    }

    if (result.entry->pos == core::PartOfSpeech::Particle && utf8::equalsAny(result.entry->surface, {"だの"}) &&
        end_pos < codepoints.size() && codepoints[end_pos] == U'は' &&
        hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbOnbinkei)) {
      continue;
    }

    // The contrastive nominal construction のでは keeps the nominalizer,
    // copular connective, and topic particle independently searchable.  The
    // causal compound particle ので cannot consume its initial two morae.
    if (result.entry->extended_pos == core::ExtendedPOS::ParticleConj &&
        grammar::isCausalParticleBeforeTopic(result.entry->surface, text.substr(byteOffsetAt(byte_offsets, end_pos)))) {
      continue;
    }

    // Skip a dictionary adjective ending in double い when its final い is the
    // leading い of the receptive auxiliary いただく: the adjective reading
    // would fuse a wa-row renyokei's い with the auxiliary's onset
    // (お使いいただく → 使い+いただく, not 使+いい+ただく). Plain いい in
    // predicate/attributive position is untouched (no ただ+inflection follows).
    if (result.entry->pos == core::PartOfSpeech::Adjective && result.length >= 2 && codepoints[end_pos - 1] == U'い' &&
        codepoints[end_pos - 2] == U'い' && verb_helpers::itadakuParadigmStartsAt(codepoints, end_pos - 1)) {
      continue;
    }

    // Create edge
    // v0.8: flags derived from extended_pos, cost from getCategoryCost()
    uint8_t flags = core::LatticeEdge::kFromDictionary;
    if (result.from_user_dict) {
      flags |= core::LatticeEdge::kFromUserDict;
    }
    if (result.entry->extended_pos == core::ExtendedPOS::NounFormal) {
      flags |= core::LatticeEdge::kIsFormalNoun;
    }
    // Note: is_low_info removed - can be derived from extended_pos if needed

    // Cost is now derived from ExtendedPOS via getCategoryCost()
    float cost = analysis::getCategoryCost(result.entry->extended_pos);

    if (result.entry->pos == core::PartOfSpeech::Noun && result.length >= 2 &&
        grammar::isAllKanji(result.entry->surface)) {
      cost += candidate::kVerifiedMultiCharacterNounBonus;
      flags |= core::LatticeEdge::kHasCustomCost;
    }

    if (result.entry->extended_pos == core::ExtendedPOS::PronounInterrogative &&
        result.length >= longest_interjection) {
      cost += candidate::kInterrogativePronounBonus;
      flags |= core::LatticeEdge::kHasCustomCost;
    }

    if (result.entry->pos == core::PartOfSpeech::Verb &&
        result.entry->extended_pos == core::ExtendedPOS::VerbShuushikei &&
        utf8::endsWith(result.entry->surface, "せる")) {
      cost += candidate::kLexicalSeruBaseBonus;
      flags |= core::LatticeEdge::kHasCustomCost;
    }

    if (result.entry->extended_pos == core::ExtendedPOS::NounFormal && end_pos + 1 < codepoints.size() &&
        codepoints[end_pos] == U'で' && (codepoints[end_pos + 1] == U'は' || codepoints[end_pos + 1] == U'も')) {
      cost += candidate::kFormalNounCopularTopicBonus;
      flags |= core::LatticeEdge::kHasCustomCost;
    }

    if (result.entry->pos == core::PartOfSpeech::Adverb && end_pos + 1 < codepoints.size() &&
        codepoints[end_pos] == U'な' && codepoints[end_pos + 1] == U'の') {
      cost += candidate::kAdverbExplanatoryCopulaBonus;
      flags |= core::LatticeEdge::kHasCustomCost;
    }

    // In the explanatory interrogative opener, an adverb ends before the
    // sentence-final question particle and quotative predicate (なぜ+かというと).
    // Keep this productive boundary available instead of preferring an
    // accidental lexicalized adverb that absorbs か.
    if (result.entry->pos == core::PartOfSpeech::Adverb &&
        grammar::startsInterrogativeQuoteIntroduction(text.substr(byteOffsetAt(byte_offsets, end_pos)))) {
      cost += candidate::kInterrogativeQuoteIntroductionBonus;
      flags |= core::LatticeEdge::kHasCustomCost;
    }

    // A dictionary-backed mixed-script noun can be a lexicalized compound
    // containing an inflected verbal segment. Prefer that registered search
    // unit over a coincidental inflection path.
    if (result.entry->pos == core::PartOfSpeech::Noun && result.length >= 3) {
      bool has_kanji = false;
      bool has_hiragana = false;
      for (size_t idx = start_pos; idx < end_pos; ++idx) {
        has_kanji = has_kanji || normalize::isKanjiCodepoint(codepoints[idx]);
        has_hiragana = has_hiragana || kana::isHiraganaCodepoint(codepoints[idx]);
      }
      const bool ichidan_predicate_continuation =
          has_kanji && has_hiragana && end_pos < codepoints.size() &&
          dict_manager_.lookupExact(result.entry->surface + "る", core::PartOfSpeech::Verb) != nullptr &&
          (codepoints[end_pos] == U'て' ||
           (end_pos + 1 < codepoints.size() && codepoints[end_pos] == U'ら' && codepoints[end_pos + 1] == U'れ'));
      if (has_kanji && has_hiragana && !ichidan_predicate_continuation) {
        cost += candidate::kLexicalizedMixedScriptNounBonus;
        flags |= core::LatticeEdge::kHasCustomCost;
      }
    }

    const bool is_fused_demo = result.length == 2 && end_pos >= 2 && codepoints[end_pos - 2] == U'で' &&
                               codepoints[end_pos - 1] == U'も' &&
                               result.entry->extended_pos == core::ExtendedPOS::ParticleAdverbial;
    if (is_fused_demo && verb_helpers::naiNegativeFollowsAt(codepoints, end_pos) &&
        hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::AdjNaAdj)) {
      continue;
    }

    // A bare え-row dict-verb imperative closing a clause (書け, 止まれ) is the 命令形 of the
    // base verb, not the potential-verb renyokei; without this the spurious 未然+受身れ split
    // (止ま+れ, lemma 止む) wins. Gated so any auxiliary/ば continuation (走れます/走れば/止まれる)
    // leaves the connection scores byte-identical.
    if (result.entry->pos == core::PartOfSpeech::Verb &&
        (result.entry->extended_pos == core::ExtendedPOS::VerbKateikei ||
         result.entry->extended_pos == core::ExtendedPOS::VerbMeireikei) &&
        grammar::containsKanji(result.entry->surface)) {
      const bool continues = end_pos < codepoints.size() &&
                             (codepoints[end_pos] == U'ば' ||
                              verb_helpers::isPassiveAuxContinuation(codepoints, end_pos, /*strict_masu=*/true));
      if (!continues) {
        cost += candidate::verb_cost::kImperativeFinalBonus;
        // Flag the tuned cost so the scorer honours it even when it lands on exactly 0.0
        // (0.0 is otherwise read as "unset" and falls back to the category cost).
        flags |= core::LatticeEdge::kHasCustomCost;
      }
    }

    // A single-token godan potential (読める) is analyzed as an independent ichidan verb, so its
    // lemma is its surface. The boost lets that dict form beat an unrelated ichidan reading. Excluded: independent
    // ichidan verbs (割れる==割れる have lemma == surface, and 自他 pairs like 切れる are registered
    // as ICHIDAN so no potential form is generated); られる passive/potential (来られる); and
    // irregular L1 forms whose lemma differs for other reasons (す→する) that do not end え-row + る.
    const bool is_godan_potential =
        result.entry->pos == core::PartOfSpeech::Verb &&
        result.entry->extended_pos == core::ExtendedPOS::VerbShuushikei &&
        std::string_view(result.entry->lemma) != std::string_view(result.entry->surface) &&
        utf8::endsWith(result.entry->surface, "る") && !utf8::endsWith(result.entry->surface, "られる") &&
        grammar::endsWithERow(
            std::string_view(result.entry->surface).substr(0, result.entry->surface.size() - core::kJapaneseCharBytes));
    if (is_godan_potential) {
      cost += candidate::verb_cost::kImperativeFinalBonus;
      flags |= core::LatticeEdge::kHasCustomCost;
    }

    const std::string_view lemma =
        is_godan_potential ? std::string_view(result.entry->surface) : std::string_view(result.entry->lemma);
    dictionary::ConjugationType conj_type = dictionary::ConjugationType::None;
    // Dictionary entries deliberately omit conjugation metadata. For a verb
    // whose dictionary-form ending uniquely identifies a Godan row, preserve
    // that information on the lattice edge so a low-cost dictionary match does
    // not discard the type carried by an equivalent generated candidate.
    if (result.entry->pos == core::PartOfSpeech::Verb && !lemma.empty()) {
      const char32_t final_cp = utf8::decodeFirstChar(utf8::lastChar(lemma));
      conj_type = grammar::verbTypeToConjType(grammar::verbTypeFromBaseCodepoint(final_cp));
    }

    // A godan e-row form followed by past た cannot be a conditional or an
    // imperative; it is the continuative stem of the derived potential verb
    // (書け+た, 見渡せ+た). Keep the dictionary's conditional edge for ば,
    // and add this context-licensed potential edge without registering every
    // productive potential form as a separate verb.
    if (result.entry->pos == core::PartOfSpeech::Verb &&
        result.entry->extended_pos == core::ExtendedPOS::VerbKateikei && end_pos < codepoints.size() &&
        codepoints[end_pos] == U'た' && grammar::endsWithERow(result.entry->surface)) {
      lattice.addEdge(result.entry->surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(end_pos),
                      core::PartOfSpeech::Verb, getCategoryCost(core::ExtendedPOS::VerbRenyokei),
                      core::LatticeEdge::kFromDictionary, normalize::concat(result.entry->surface, "る"),
                      dictionary::ConjugationType::Ichidan, core::CandidateOrigin::Dictionary,
                      candidate::kDictionaryOriginConfidence, {}, core::ExtendedPOS::VerbRenyokei,
                      "dictionary_potential_renyokei_before_past");
    }
    // An auxiliary cell spelled with a final sokuon is an onbin form, and what
    // it can connect to follows from the paradigm it belongs to. The past た is
    // always available. The connective て needs a paradigm that has a te-form at
    // all: an auxiliary inflected as a Godan verb does (たがっ+て), while the
    // copula's continuative is で and it has no such cell, so its onbin before て
    // is really the plain form plus the quotative (無理|だ|って, not 無理|だっ|て).
    const bool auxiliary_inflects_as_godan =
        grammar::isModernGodanTerminalKana(utf8::decodeLastChar(result.entry->lemma));
    const bool unlicensed_auxiliary_onbin =
        result.entry->pos == core::PartOfSpeech::Auxiliary && utf8::endsWith(result.entry->surface, "っ") &&
        end_pos < codepoints.size() && !auxiliary_inflects_as_godan &&
        !utf8::equalsAny(extractSubstring(codepoints, end_pos, end_pos + 1), {"た"});
    if (!unlicensed_auxiliary_onbin) {
      lattice.addEdge(result.entry->surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(end_pos),
                      result.entry->pos, cost, flags, lemma, conj_type, core::CandidateOrigin::Dictionary, 1.0F, {},
                      result.entry->extended_pos, "dict");
    }

    // Extend predicates and adverbs with colloquial emphasis
    // (ですっ, 行くーー, きたあああ). Unknown candidates use the same matcher.
    if (end_pos < codepoints.size() &&
        (result.entry->pos == core::PartOfSpeech::Verb || result.entry->pos == core::PartOfSpeech::Auxiliary ||
         result.entry->pos == core::PartOfSpeech::Adjective || result.entry->pos == core::PartOfSpeech::Adverb)) {
      // A dictionary irrealis stem cannot absorb っ before て/た as emphasis:
      // 染まっ+て belongs to the GodanRa verb 染まる, not 染ま(染む)+っ+て.
      // The hypothetical stem is barred for the same reason, and it is where
      // the productive potential forms are registered: かえ is the ichidan stem
      // of かえる (the potential of 買う), which has no sokuonbin at all, so
      // かえっ+て can only belong to the godan かえる and must keep that lemma.
      // An auxiliary cannot either: っ+て after one is the concessive particle
      // って (書い+た+って), and every genuine auxiliary onbin cell (だっ, たかっ,
      // じゃっ) is a dictionary entry in its own right.
      const bool sokuon_before_te_or_ta =
          end_pos + 1 < codepoints.size() && codepoints[end_pos] == core::hiragana::kSmallTsu &&
          (codepoints[end_pos + 1] == core::hiragana::kTe || codepoints[end_pos + 1] == core::hiragana::kTa) &&
          (result.entry->extended_pos == core::ExtendedPOS::VerbMizenkei ||
           result.entry->extended_pos == core::ExtendedPOS::VerbKateikei ||
           result.entry->pos == core::PartOfSpeech::Auxiliary);
      const auto emphatic = sokuon_before_te_or_ta
                                ? verb_helpers::EmphaticSuffixMatch{}
                                : verb_helpers::matchEmphaticSuffix(codepoints, end_pos, result.entry->pos,
                                                                    verb_helpers::SokuonOnsetPolicy::DictionaryEntry);
      // A bare sokuon after a predicate is one of two things: the genuine 促音便,
      // which needs て/た/で/だ behind it (と+いっ+て), or colloquial emphasis, which
      // closes the clause (行くっ！). Before any other kana it is neither, and taking
      // it eats the opening mora of the following word (にらめっ+こ for にらめっこ).
      const bool bare_sokuon = emphatic.suffix == "っ";
      // Only a verb continuative owns a 促音便 cell, so only it can license the
      // sokuon in front of the connective. An i-adjective closes its terminal on
      // い and builds its own onbin elsewhere (忙し|かっ|た), and a na-adjective
      // stem has no inflection at all, so a っ after either is the emphatic —
      // which needs a clause end, not a following word (忙しい|っていう, not
      // 忙しいっ|ていう).
      const bool host_owns_sokuonbin_cell = result.entry->pos == core::PartOfSpeech::Verb &&
                                            (result.entry->extended_pos == core::ExtendedPOS::VerbRenyokei ||
                                             result.entry->extended_pos == core::ExtendedPOS::VerbOnbinkei);
      const bool unlicensed_bare_sokuon =
          bare_sokuon && emphatic.end < codepoints.size() &&
          normalize::classifyChar(codepoints[emphatic.end]) == normalize::CharType::Hiragana &&
          !(host_owns_sokuonbin_cell &&
            utf8::equalsAny(extractSubstring(codepoints, emphatic.end, emphatic.end + 1), {"て", "た", "で", "だ"}));
      if (!emphatic.empty() && !unlicensed_bare_sokuon) {
        // Determine extended_pos for emphatic form
        // Sokuon-ending verb forms should be VerbOnbinkei (音便形)
        core::ExtendedPOS emphatic_epos = result.entry->extended_pos;
        if (result.entry->pos == core::PartOfSpeech::Verb && emphatic.suffix == "っ") {
          // E.g., い(連用形) + っ → いっ(音便形) for と+いっ+て pattern
          emphatic_epos = core::ExtendedPOS::VerbOnbinkei;
        }

        const std::string emphatic_surface = result.entry->surface + emphatic.suffix;
        const bool preserves_emphatic_surface =
            result.entry->pos == core::PartOfSpeech::Auxiliary ||
            (result.entry->pos == core::PartOfSpeech::Adjective &&
             (emphatic.standard_char_count >= 2 || emphatic.repeated_vowel_count >= 3));
        const std::string_view dictionary_lemma = result.entry->lemma.empty() ? std::string_view(result.entry->surface)
                                                                              : std::string_view(result.entry->lemma);
        const std::string_view emphatic_lemma =
            preserves_emphatic_surface ? std::string_view(emphatic_surface) : dictionary_lemma;
        lattice.addEdge(emphatic_surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(emphatic.end),
                        result.entry->pos, cost + verb_helpers::emphaticCostAdjustment(emphatic), flags, emphatic_lemma,
                        dictionary::ConjugationType::None, core::CandidateOrigin::Dictionary, 1.0F, {}, emphatic_epos,
                        "dict_emphatic");
      }
    }
  }

  tokenizer_dictionary_detail::appendSpecialGrammarCandidates(lattice, text, codepoints, start_pos, byte_pos);
}

}  // namespace suzume::analysis
