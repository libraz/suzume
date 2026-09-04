/**
 * @file tokenizer_unknown.cpp
 * @brief Unknown-word candidate generation for the tokenizer
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
#include "normalize/utf8.h"
#include "split_candidates.h"
#include "suffix_candidates.h"
#include "tokenizer_utils.h"
#include "verb_candidates_helpers.h"

namespace suzume::analysis {

namespace {

// True if every char position in [start, end) has CharType `type`. When
// `allow_choon` is set, the prolonged sound mark (ー) is also accepted as part
// of the run (colloquial すごーい, katakana loanwords). Bounds-checked against
// both char_types and codepoints so callers can pass raw candidate ranges.
bool allCharsAre(const std::vector<normalize::CharType>& char_types, const std::vector<char32_t>& codepoints,
                 size_t start, size_t end, normalize::CharType type, bool allow_choon) {
  for (size_t idx = start; idx < end && idx < char_types.size(); ++idx) {
    if (char_types[idx] == type) {
      continue;
    }
    if (allow_choon && idx < codepoints.size() && normalize::isProlongedSoundMark(codepoints[idx])) {
      continue;
    }
    return false;
  }
  return true;
}

// A copular irrealis form followed by the volitional auxiliary is a
// grammatical auxiliary sequence, not an unknown lexical verb.  Keeping the
// sequence visible prevents a short pure-hiragana verb candidate from hiding
// a dictionary-backed copula + volitional analysis.
bool isCopulaVolitionalSequence(const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                const ByteOffsets& byte_offsets, size_t start, size_t end) {
  constexpr size_t kMinimumMorphemeCount = 2;
  if (end - start < kMinimumMorphemeCount) {
    return false;
  }

  const std::string_view prefix = textRange(text, byte_offsets, start, end - 1);
  const std::string_view suffix = textRange(text, byte_offsets, end - 1, end);
  const auto* copula = dict_manager.lookupExact(prefix, core::PartOfSpeech::Auxiliary);
  const auto* volitional = dict_manager.lookupExact(suffix, core::PartOfSpeech::Auxiliary);
  return copula != nullptr && copula->extended_pos == core::ExtendedPOS::AuxCopulaDa && volitional != nullptr &&
         volitional->extended_pos == core::ExtendedPOS::AuxVolitional;
}

using suzume::analysis::verb_helpers::isProductiveShiiAdjectiveTerminal;

bool crossesPeriodEndNominalBoundary(const std::vector<char32_t>& codepoints,
                                     const std::vector<normalize::CharType>& char_types,
                                     const UnknownCandidate& candidate) {
  constexpr size_t kMinimumSpanLength = 3;
  if (candidate.end - candidate.start < kMinimumSpanLength || candidate.end > codepoints.size() ||
      candidate.end > char_types.size()) {
    return false;
  }
  return candidate.extended_pos == core::ExtendedPOS::VerbRenyokei && codepoints[candidate.end - 3] == U'末' &&
         char_types[candidate.end - 2] == normalize::CharType::Kanji &&
         char_types[candidate.end - 1] == normalize::CharType::Hiragana;
}

bool hasContentEdgeEndingAt(const core::Lattice& lattice, size_t boundary) {
  return core::anyEdgeEndingAt(lattice, boundary,
                               [](const core::LatticeEdge& edge) { return core::isContentWord(edge.pos); });
}

// A binding particle after a terminal predicate closes that predicate. An
// unverified candidate beginning at the particle cannot instead be a new
// lexical word (渡る+も+いとわない, not 渡る+もいとわ+ない).
bool startsAtBindingParticleAfterTerminalVerb(const core::Lattice& lattice,
                                              const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                              const ByteOffsets& byte_offsets, const UnknownCandidate& candidate) {
  if (candidate.start == 0 || candidate.lemma_verified) {
    return false;
  }
  const auto* particle = dict_manager.lookupExact(textRange(text, byte_offsets, candidate.start, candidate.start + 1),
                                                  core::PartOfSpeech::Particle);
  return particle != nullptr && particle->extended_pos == core::ExtendedPOS::ParticleBinding &&
         hasPrecedingExtendedPOS(lattice, candidate.start, core::ExtendedPOS::VerbShuushikei);
}

// The left bracket of a post-particle noun rescue is whatever can fill the
// argument slot the particle marks. That is wider than the taggable content
// words: a pronoun heads a phrase exactly as a noun does (これ|は|りんご), and it
// is outside isContentWord only because tagging does not emit a pronoun tag.
// Widening isContentWord itself would change tagging, so the nominal-head
// notion stays local to this bracket test.
bool hasNominalHeadEdgeEndingAt(const core::Lattice& lattice, size_t boundary) {
  return core::anyEdgeEndingAt(lattice, boundary, [](const core::LatticeEdge& edge) {
    return core::isContentWord(edge.pos) || edge.pos == core::PartOfSpeech::Pronoun;
  });
}

bool hasAttributiveEdgeEndingAt(const core::Lattice& lattice, size_t boundary) {
  return core::anyEdgeEndingAt(lattice, boundary, [](const core::LatticeEdge& edge) {
    return (edge.pos == core::PartOfSpeech::Verb && (edge.extended_pos == core::ExtendedPOS::VerbShuushikei ||
                                                     edge.extended_pos == core::ExtendedPOS::VerbRentaikei ||
                                                     edge.extended_pos == core::ExtendedPOS::VerbTaForm)) ||
           (edge.pos == core::PartOfSpeech::Adjective && edge.extended_pos == core::ExtendedPOS::AdjBasic);
  });
}

// A finite predicate followed by the closed negative-conjecture auxiliary owns
// that boundary.  Unknown content candidates can otherwise start just before
// it (forgetting the terminal kana of an Ichidan verb) or at the auxiliary and
// absorb its following particle.  Require the actual lattice predecessor, so
// lexical homographs ending in the same kana remain available elsewhere.
bool overlapsPredicativeNegativeConjecture(const core::Lattice& lattice,
                                           const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                           const std::vector<char32_t>& codepoints, const ByteOffsets& byte_offsets,
                                           size_t candidate_start, size_t candidate_end) {
  for (size_t aux_start = candidate_start; aux_start < candidate_end; ++aux_start) {
    const size_t probe_end = std::min(codepoints.size(), aux_start + static_cast<size_t>(3));
    const std::string_view probe = textRange(text, byte_offsets, aux_start, probe_end);
    for (const auto& match : dict_manager.lookup(probe, 0)) {
      if (match.entry == nullptr || match.entry->extended_pos != core::ExtendedPOS::AuxNegativeMai) {
        continue;
      }
      const size_t aux_end = aux_start + match.length;
      if (aux_end > candidate_end || (candidate_start == aux_start && candidate_end == aux_end)) {
        continue;
      }
      if (core::anyEdgeEndingAt(lattice, aux_start, [](const core::LatticeEdge& edge) {
            return edge.pos == core::PartOfSpeech::Verb && (edge.extended_pos == core::ExtendedPOS::VerbShuushikei ||
                                                            edge.extended_pos == core::ExtendedPOS::VerbMizenkei ||
                                                            edge.extended_pos == core::ExtendedPOS::VerbRenyokei);
          })) {
        return true;
      }
    }
  }
  return false;
}

// Shared evidence for the two closed suffixes below: the candidate span ends
// with a registered auxiliary of the given kind whose own left boundary is
// already closed by a dictionary-backed irrealis stem.  Demanding the
// candidate's lemma as well narrows the match to the very word the candidate
// claims to be, which the passive case deliberately does not require.
bool coversRegisteredAuxiliaryOnVerifiedMizenkei(const core::Lattice& lattice,
                                                 const dictionary::DictionaryManager& dict_manager,
                                                 std::string_view text, const ByteOffsets& byte_offsets,
                                                 const UnknownCandidate& candidate,
                                                 core::ExtendedPOS auxiliary_extended_pos, bool require_same_lemma) {
  for (size_t auxiliary_start = candidate.start + 1; auxiliary_start < candidate.end; ++auxiliary_start) {
    const std::string_view suffix = textRange(text, byte_offsets, auxiliary_start, candidate.end);
    const auto* auxiliary = dict_manager.lookupExact(suffix, core::PartOfSpeech::Auxiliary);
    if (auxiliary == nullptr || auxiliary->extended_pos != auxiliary_extended_pos) {
      continue;
    }
    for (size_t predecessor_start = candidate.start; predecessor_start < auxiliary_start; ++predecessor_start) {
      if (core::anyEdgeStartingAt(lattice, predecessor_start, [&](const core::LatticeEdge& predecessor) {
            return predecessor.end == auxiliary_start && predecessor.fromDictionary() &&
                   predecessor.extended_pos == core::ExtendedPOS::VerbMizenkei &&
                   (!require_same_lemma || predecessor.lemma == candidate.lemma);
          })) {
        return true;
      }
    }
  }
  return false;
}

// A generated verb cannot absorb a closed classical negative when the same
// lemma already supplies a dictionary-backed irrealis stem immediately to its
// left.  This is stronger evidence than merely finding an auxiliary-looking
// substring: both the closed suffix and its licensed predecessor are proven,
// while exact lexical homographs remain exempt.
bool absorbsVerifiedClassicalNegative(const core::Lattice& lattice, const dictionary::DictionaryManager& dict_manager,
                                      std::string_view text, const ByteOffsets& byte_offsets,
                                      const UnknownCandidate& candidate) {
  const auto* exact_verb = dict_manager.lookupExact(candidate.surface, core::PartOfSpeech::Verb);
  if (candidate.pos != core::PartOfSpeech::Verb || candidate.lemma.empty() ||
      (exact_verb != nullptr && exact_verb->lemma == exact_verb->surface)) {
    return false;
  }
  return coversRegisteredAuxiliaryOnVerifiedMizenkei(lattice, dict_manager, text, byte_offsets, candidate,
                                                     core::ExtendedPOS::AuxNegativeNu, true);
}

// Before closed negation, do not replace a proven irrealis + passive chain
// with an open Ichidan-like stem spanning both morphemes (さ+れ+ない, 見+られ+ない).
// Requiring the right-hand negative as well as the dictionary predecessor
// avoids treating incidental れ inside ordinary lexical stems as passive.
bool absorbsPassiveBeforeNegative(const core::Lattice& lattice, const dictionary::DictionaryManager& dict_manager,
                                  std::string_view text, const std::vector<char32_t>& codepoints,
                                  const ByteOffsets& byte_offsets, const UnknownCandidate& candidate) {
  if (candidate.pos != core::PartOfSpeech::Verb || candidate.extended_pos != core::ExtendedPOS::VerbMizenkei ||
      candidate.end >= codepoints.size() ||
      dict_manager.lookupExact(candidate.surface, core::PartOfSpeech::Verb) != nullptr) {
    return false;
  }

  const size_t probe_end = std::min(codepoints.size(), candidate.end + static_cast<size_t>(3));
  const std::string_view following = textRange(text, byte_offsets, candidate.end, probe_end);
  const bool followed_by_negative =
      lookupResultsHaveExtendedPOS(dict_manager.lookup(following, 0), core::ExtendedPOS::AuxNegativeNai);
  if (!followed_by_negative) {
    return false;
  }
  return coversRegisteredAuxiliaryOnVerifiedMizenkei(lattice, dict_manager, text, byte_offsets, candidate,
                                                     core::ExtendedPOS::AuxPassive, false);
}

// A complete multi-kanji nominal stem owns its full span before a closed する
// inflection. Unknown verbs starting inside that noun must not absorb the last
// kanji together with する (勉強+すれ+ば, not 勉+強すれ+ば). This is the
// productive Sahen boundary, so the noun itself need not be registered.
bool startsInsideVerifiedNounAndAbsorbsSuru(const core::Lattice& lattice,
                                            const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                            const ByteOffsets& byte_offsets, const UnknownCandidate& candidate) {
  if (candidate.pos != core::PartOfSpeech::Verb || candidate.start == 0) {
    return false;
  }
  // A closed humble subsidiary verb is its own morpheme after the nominal
  // (確認 + 致し + ます). Its continuative also ends in し, so the enclosing
  // kanji-run noun looks verified when it is in fact the fabricated reading.
  if (grammar::isHumbleHonorificRenyokei(candidate.surface)) {
    return false;
  }
  for (size_t suru_start = candidate.start + 1; suru_start < candidate.end; ++suru_start) {
    const std::string_view suffix = textRange(text, byte_offsets, suru_start, candidate.end);
    if (!hasCompleteVerbLemma(dict_manager, suffix, candidate.end - suru_start, "する")) {
      continue;
    }
    if (core::anyEdgeEndingAt(lattice, suru_start, [&candidate](const core::LatticeEdge& noun) {
          return noun.start < candidate.start && noun.pos == core::PartOfSpeech::Noun &&
                 normalize::utf8Length(noun.surface) >= 2 && grammar::isAllKanji(noun.surface);
        })) {
      return true;
    }
  }
  return false;
}

// The same productive boundary can be obscured by a candidate that consumes
// only the initial す of the conditional すれ (提出す+れ+ば). Confirm the
// multi-kanji nominal alternative in the current generation batch and the
// complete closed する form on the right before discarding that path.
bool consumesInitialSuruConditional(const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                    const std::vector<char32_t>& codepoints, const ByteOffsets& byte_offsets,
                                    const std::vector<UnknownCandidate>& batch_candidates,
                                    const UnknownCandidate& candidate) {
  if (candidate.pos != core::PartOfSpeech::Verb || candidate.end <= candidate.start ||
      candidate.end + 1 >= codepoints.size() || codepoints[candidate.end - 1] != U'す') {
    return false;
  }
  const std::string_view conditional = textRange(text, byte_offsets, candidate.end - 1, candidate.end + 1);
  const bool is_suru_conditional = hasCompleteVerbLemma(dict_manager, conditional, 2, "する");
  const auto* conditional_particle = dict_manager.lookupExact(
      textRange(text, byte_offsets, candidate.end + 1, candidate.end + 2), core::PartOfSpeech::Particle);
  if (!is_suru_conditional || conditional_particle == nullptr ||
      conditional_particle->extended_pos != core::ExtendedPOS::ParticleConj) {
    return false;
  }
  return std::any_of(batch_candidates.begin(), batch_candidates.end(), [&](const UnknownCandidate& noun) {
    return noun.pos == core::PartOfSpeech::Noun && noun.start == candidate.start && noun.end + 1 == candidate.end &&
           normalize::utf8Length(noun.surface) >= 2 && grammar::isAllKanji(noun.surface);
  });
}

bool verbFormLicensesAuxiliary(core::ExtendedPOS verb_epos, core::ExtendedPOS auxiliary_epos) {
  return ((auxiliary_epos == core::ExtendedPOS::AuxDesireTai || auxiliary_epos == core::ExtendedPOS::AuxTenseMasu ||
           auxiliary_epos == core::ExtendedPOS::AuxExcessive) &&
          verb_epos == core::ExtendedPOS::VerbRenyokei) ||
         ((auxiliary_epos == core::ExtendedPOS::AuxNegativeNai || auxiliary_epos == core::ExtendedPOS::AuxNegativeNu ||
           auxiliary_epos == core::ExtendedPOS::AuxPassive || auxiliary_epos == core::ExtendedPOS::AuxCausative ||
           auxiliary_epos == core::ExtendedPOS::AuxVolitional) &&
          verb_epos == core::ExtendedPOS::VerbMizenkei) ||
         (auxiliary_epos == core::ExtendedPOS::AuxClassicalBeshi && verb_epos == core::ExtendedPOS::VerbShuushikei);
}

// An open verb candidate cannot restart inside a complete verb form and then
// absorb the closed auxiliary selected by that form (確かめ+たい, しかる+べく).
// Both the overlapping left edge and its EPOS-to-auxiliary connection are
// required, so an incidental auxiliary homograph does not suppress a lexical
// verb elsewhere. The left edge also has to be a complete verb form rather than
// merely be shaped like one: a reconstruction whose base form is attested
// nowhere carries no more evidence than the candidate it would veto, and one
// starting a mora too early vetoes exactly the reading that would have exposed
// it (the non-word なきゃわく removing わかん from なきゃ+わかん+ない).
bool reopensObservedVerbAuxiliaryBoundary(const core::Lattice& lattice,
                                          const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                          const ByteOffsets& byte_offsets, const UnknownCandidate& candidate) {
  if (candidate.pos != core::PartOfSpeech::Verb || candidate.lemma_verified || candidate.start == 0) {
    return false;
  }
  for (size_t split = candidate.start + 1; split < candidate.end; ++split) {
    const auto* auxiliary =
        dict_manager.lookupExact(textRange(text, byte_offsets, split, candidate.end), core::PartOfSpeech::Auxiliary);
    if (auxiliary == nullptr) {
      continue;
    }
    if (core::anyEdgeEndingAt(lattice, split, [&](const core::LatticeEdge& edge) {
          return edge.start < candidate.start && edge.pos == core::PartOfSpeech::Verb && edge.lemmaVerified() &&
                 verbFormLicensesAuxiliary(edge.extended_pos, auxiliary->extended_pos);
        })) {
      return true;
    }
  }
  return false;
}

// A closed causative auxiliary following a nominal head starts the productive
// サ変 chain (勉強+さ+せ+ない). Do not reinterpret the same kana span as an
// unattested lexical verb merely because the nominal host has no dictionary
// entry. Single-kanji verb stems remain safe: their lattice path uses the
// closed causative edge itself (見+させ+ない), not this unknown verb edge.
bool shadowsClosedCausativeAfterNominalHead(const core::Lattice& lattice,
                                            const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                            const ByteOffsets& byte_offsets, const UnknownCandidate& candidate) {
  if (candidate.pos != core::PartOfSpeech::Verb || candidate.lemma_verified ||
      candidate.extended_pos != core::ExtendedPOS::VerbMizenkei || candidate.start == 0 ||
      !hasNominalHeadEdgeEndingAt(lattice, candidate.start)) {
    return false;
  }
  const auto* auxiliary = dict_manager.lookupExact(textRange(text, byte_offsets, candidate.start, candidate.end),
                                                   core::PartOfSpeech::Auxiliary);
  return auxiliary != nullptr && auxiliary->extended_pos == core::ExtendedPOS::AuxCausative;
}

// A head proven nominal by both a left selector and a right nominal particle
// owns its complete span. Do not let an internal predicate plus a homographic
// closed auxiliary reopen that head (谷の向こうに → 向こう, not 向こ+う).
bool isInternalPredicateOfSelectedNominalHead(const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                              const ByteOffsets& byte_offsets,
                                              const std::vector<UnknownCandidate>& batch_candidates,
                                              const UnknownCandidate& candidate) {
  if (candidate.pos != core::PartOfSpeech::Verb) {
    return false;
  }
  return std::any_of(batch_candidates.begin(), batch_candidates.end(), [&](const UnknownCandidate& head) {
    if (head.origin != core::CandidateOrigin::SelectedNominalHead || head.pos != core::PartOfSpeech::Noun ||
        head.start != candidate.start || head.end <= candidate.end) {
      return false;
    }
    const auto* auxiliary =
        dict_manager.lookupExact(textRange(text, byte_offsets, candidate.end, head.end), core::PartOfSpeech::Auxiliary);
    return auxiliary != nullptr && verbFormLicensesAuxiliary(candidate.extended_pos, auxiliary->extended_pos);
  });
}

// A generated continuative must not be promoted to a deverbal noun when its
// entire span already decomposes into a verified left constituent and a
// complete closed right constituent (本+なし, す+べき, こ+なく, る+うち).
// The ordinary productive nominalizations 隔たり+を and 読み+が have no such
// internal two-edge proof and remain eligible.
bool hasCompleteInternalConstituentBoundary(const core::Lattice& lattice,
                                            const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                            const ByteOffsets& byte_offsets,
                                            const std::vector<UnknownCandidate>& batch_candidates,
                                            const UnknownCandidate& candidate) {
  for (size_t split = candidate.start + 1; split < candidate.end; ++split) {
    const size_t right_probe_end =
        std::min(byte_offsets.size() - 1, std::max(candidate.end, split + static_cast<size_t>(4)));
    const std::string_view right_probe = textRange(text, byte_offsets, split, right_probe_end);
    bool complete_right = false;
    bool right_is_auxiliary = false;
    bool right_is_adjective = false;
    bool right_is_formal_noun = false;
    bool right_is_particle = false;
    bool right_is_connective_particle = false;
    bool right_is_nominal_particle = false;
    bool right_is_suffix = false;
    core::ExtendedPOS right_auxiliary_epos = core::ExtendedPOS::Unknown;
    for (const auto& match : dict_manager.lookup(right_probe, 0)) {
      // A constituent may end exactly with the fabricated continuative
      // (見+て) or continue across its right edge (いる+あいだ). Both prove
      // that the continuative span cuts through a stronger grammatical
      // boundary.
      if (match.entry == nullptr || match.length < candidate.end - split) {
        continue;
      }
      right_is_auxiliary = right_is_auxiliary || match.entry->pos == core::PartOfSpeech::Auxiliary;
      if (match.entry->pos == core::PartOfSpeech::Auxiliary) {
        right_auxiliary_epos = match.entry->extended_pos;
      }
      right_is_adjective = right_is_adjective || match.entry->pos == core::PartOfSpeech::Adjective;
      right_is_formal_noun = right_is_formal_noun || (match.entry->pos == core::PartOfSpeech::Noun &&
                                                      match.entry->extended_pos == core::ExtendedPOS::NounFormal);
      right_is_particle = right_is_particle || match.entry->pos == core::PartOfSpeech::Particle;
      right_is_connective_particle =
          right_is_connective_particle || match.entry->extended_pos == core::ExtendedPOS::ParticleConj;
      right_is_nominal_particle = right_is_nominal_particle || (match.entry->pos == core::PartOfSpeech::Particle &&
                                                                isNominalForcingParticle(match.entry->extended_pos));
      right_is_suffix = right_is_suffix || match.entry->pos == core::PartOfSpeech::Suffix;
      complete_right = right_is_auxiliary || right_is_adjective || right_is_formal_noun ||
                       right_is_connective_particle || right_is_nominal_particle || right_is_suffix;
    }
    if (!complete_right || (right_is_particle && !right_is_connective_particle && !right_is_nominal_particle)) {
      continue;
    }

    // A prefix that ends in kanji is a structurally valid nominal host for a
    // closed suffix.  This proof must not depend on edge insertion order:
    // same-type noun candidates may be materialized after the inflectional
    // candidate currently being considered.  A registered lexical noun for
    // the whole span is protected by the caller.
    // The host has to end at the kanji run: a prefix that merely contains a
    // kanji ends in the very kana whose analysis is in question (草む of
    // 草むら, 花び of 花びら), and reading that kana as the tail of a nominal
    // assumes the split it is supposed to prove.  When such a prefix really is
    // a nominal, an edge or a candidate says so and the licensing check below
    // finds it.
    if (right_is_suffix && grammar::endsWithKanji(textRange(text, byte_offsets, candidate.start, split))) {
      return true;
    }

    const auto left_licenses_right = [&](core::PartOfSpeech left_pos, core::ExtendedPOS left_epos, bool left_verified,
                                         bool overlaps_candidate_start, std::string_view left_surface) {
      const bool licenses_adjective = right_is_adjective && left_pos == core::PartOfSpeech::Noun &&
                                      (left_verified || !grammar::isPureHiragana(left_surface));
      const bool structurally_licenses_auxiliary = overlaps_candidate_start && left_pos == core::PartOfSpeech::Verb &&
                                                   verbFormLicensesAuxiliary(left_epos, right_auxiliary_epos);
      const bool licenses_auxiliary =
          right_is_auxiliary &&
          (left_pos == core::PartOfSpeech::Auxiliary ||
           (left_verified && (left_pos == core::PartOfSpeech::Verb || left_pos == core::PartOfSpeech::Adjective)) ||
           structurally_licenses_auxiliary);
      const bool licenses_formal_noun =
          right_is_formal_noun &&
          (left_pos == core::PartOfSpeech::Auxiliary ||
           (left_verified && (left_pos == core::PartOfSpeech::Verb || left_pos == core::PartOfSpeech::Adjective)));
      const bool licenses_connective_particle =
          right_is_connective_particle && left_pos == core::PartOfSpeech::Verb &&
          (left_epos == core::ExtendedPOS::VerbRenyokei || left_epos == core::ExtendedPOS::VerbOnbinkei);
      const bool licenses_nominal_particle =
          right_is_nominal_particle && left_verified &&
          (left_pos == core::PartOfSpeech::Noun || left_pos == core::PartOfSpeech::Pronoun);
      // A closed suffix after a nominal is an explicit internal morpheme
      // boundary.  This prevents the generic kanji+hiragana nominalizer from
      // swallowing arbitrary hosts (家庭/初心者/読者 + 向け) without naming any
      // member of the open host class.
      const bool licenses_suffix = right_is_suffix && left_pos == core::PartOfSpeech::Noun &&
                                   (left_verified || !grammar::isPureHiragana(left_surface));
      return licenses_adjective || licenses_auxiliary || licenses_formal_noun || licenses_connective_particle ||
             licenses_nominal_particle || licenses_suffix;
    };
    bool complete_left = false;
    for (const uint32_t edge_id : lattice.edgeIdsEndingAt(split)) {
      const auto& edge = lattice.getEdge(edge_id);
      if (edge.start <= candidate.start && left_licenses_right(edge.pos, edge.extended_pos, edge.lemmaVerified(),
                                                               edge.start < candidate.start, edge.surface)) {
        complete_left = true;
        break;
      }
    }
    if (!complete_left) {
      complete_left = std::any_of(batch_candidates.begin(), batch_candidates.end(), [&](const auto& alternative) {
        return alternative.start == candidate.start && alternative.end == split &&
               left_licenses_right(alternative.pos, alternative.extended_pos, alternative.lemma_verified, false,
                                   alternative.surface);
      });
    }
    if (complete_left) {
      return true;
    }
  }
  return false;
}

// A generated predicate cannot consume a multi-mora final particle after a
// nominal head. The final particle closes the nominal predicate (本+ばい), and
// the ordinary open-class candidate must leave that closed boundary intact.
bool endsWithFinalParticleAfterNominalHead(const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                           const ByteOffsets& byte_offsets,
                                           const std::vector<UnknownCandidate>& batch_candidates,
                                           const UnknownCandidate& candidate) {
  if (candidate.lemma_verified || candidate.end <= candidate.start + 2) {
    return false;
  }
  constexpr size_t kMaxFinalParticleChars = 4;
  const size_t earliest =
      candidate.end > kMaxFinalParticleChars ? candidate.end - kMaxFinalParticleChars : candidate.start + 1;
  for (size_t particle_start = earliest; particle_start < candidate.end - 1; ++particle_start) {
    const auto* particle = dict_manager.lookupExact(textRange(text, byte_offsets, particle_start, candidate.end),
                                                    core::PartOfSpeech::Particle);
    if (particle == nullptr || particle->extended_pos != core::ExtendedPOS::ParticleFinal) {
      continue;
    }
    const bool has_nominal_prefix =
        std::any_of(batch_candidates.begin(), batch_candidates.end(), [&](const UnknownCandidate& alternative) {
          return alternative.start == candidate.start && alternative.end == particle_start &&
                 alternative.pos == core::PartOfSpeech::Noun;
        });
    if (has_nominal_prefix) {
      return true;
    }
  }
  return false;
}

// The closed adverbial sequence Noun + ながら + に exposes a morpheme
// boundary inside an otherwise plausible unknown verb continuative
// (涙ながらに, not a deverbal noun 涙ながら + に). Require all three pieces:
// a noun spanning the candidate's left side, the exact dictionary conjunctive
// particle, and the following case particle. Before の the same sequence is an
// adnominal search unit (昔ながらの, 生まれながらの) and stays whole, which is
// why the following particle is part of the test rather than an afterthought.
bool hasNounNagaraNiBoundary(const core::Lattice& lattice, const dictionary::DictionaryManager& dict_manager,
                             std::string_view text, const std::vector<char32_t>& codepoints,
                             const ByteOffsets& byte_offsets, const std::vector<UnknownCandidate>& batch_candidates,
                             const UnknownCandidate& candidate) {
  constexpr size_t kNagaraLength = 3;
  if (candidate.end < candidate.start + kNagaraLength + 1 || candidate.end >= codepoints.size() ||
      codepoints[candidate.end] != U'に') {
    return false;
  }

  const size_t nagara_start = candidate.end - kNagaraLength;
  if (codepoints[nagara_start] != U'な' || codepoints[nagara_start + 1] != U'が' ||
      codepoints[nagara_start + 2] != U'ら') {
    return false;
  }

  const auto* nagara = dict_manager.lookupExact(textRange(text, byte_offsets, nagara_start, candidate.end),
                                                core::PartOfSpeech::Particle);
  const auto* case_particle = dict_manager.lookupExact(textRange(text, byte_offsets, candidate.end, candidate.end + 1),
                                                       core::PartOfSpeech::Particle);
  if (nagara == nullptr || nagara->extended_pos != core::ExtendedPOS::ParticleConj || case_particle == nullptr ||
      case_particle->extended_pos != core::ExtendedPOS::ParticleCase) {
    return false;
  }

  if (core::anyEdgeStartingAt(lattice, candidate.start, [nagara_start](const core::LatticeEdge& edge) {
        return edge.end == nagara_start && edge.pos == core::PartOfSpeech::Noun;
      })) {
    return true;
  }
  return std::any_of(batch_candidates.begin(), batch_candidates.end(), [&](const UnknownCandidate& alternative) {
    return alternative.start == candidate.start && alternative.end == nagara_start &&
           alternative.pos == core::PartOfSpeech::Noun;
  });
}

/**
 * @brief Whether a span is a numeral plus a counter written with okurigana
 *
 * A deverbal counter carries its okurigana into the quantity phrase (一切れ,
 * 三重ね), so the span always extends past the numeral+kanji prefix that a
 * dictionary entry happens to cover (the adverb 一切, the noun 三重).  The
 * registered continuative behind the counter is what licenses the extra kana, so
 * the coincidental prefix must not price the phrase out.
 */
bool isNumeralOkuriganaCounterPhrase(const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                     const std::vector<size_t>& byte_offsets, const std::vector<char32_t>& codepoints,
                                     const std::vector<normalize::CharType>& char_types, size_t start_pos,
                                     size_t end_pos) {
  if (end_pos < start_pos + 3 || end_pos > codepoints.size()) {
    return false;
  }
  const size_t counter_pos = end_pos - 2;
  if (char_types[counter_pos] != normalize::CharType::Kanji ||
      char_types[end_pos - 1] != normalize::CharType::Hiragana) {
    return false;
  }
  for (size_t pos = start_pos; pos < counter_pos; ++pos) {
    if (!normalize::isNumeralCodepoint(codepoints[pos])) {
      return false;
    }
  }
  return hasExactPartOfSpeech(dict_manager, textRange(text, byte_offsets, counter_pos, end_pos),
                              partOfSpeechMask(core::PartOfSpeech::Verb));
}

}  // namespace

void Tokenizer::addUnknownCandidates(core::Lattice& lattice, std::string_view text,
                                     const std::vector<char32_t>& codepoints, const ByteOffsets& byte_offsets,
                                     size_t start_pos, const std::vector<normalize::CharType>& char_types) const {
  // A pure-hiragana sequence enclosed by brackets is a parenthetical reading
  // (東京（とうきょう）). It is annotation text, so retain it as one searchable
  // content token instead of a sequence of incidental particles and auxiliaries.
  if (start_pos > 0 && normalize::isOpeningBracket(codepoints[start_pos - 1])) {
    size_t reading_end = start_pos;
    while (reading_end < codepoints.size() && reading_end - start_pos < candidate::kParentheticalReadingMaxLength &&
           char_types[reading_end] == normalize::CharType::Hiragana) {
      ++reading_end;
    }
    if (reading_end > start_pos && reading_end < codepoints.size() &&
        normalize::isClosingBracket(codepoints[reading_end])) {
      lattice.addEdge(textRange(text, byte_offsets, start_pos, reading_end), static_cast<uint32_t>(start_pos),
                      static_cast<uint32_t>(reading_end), core::PartOfSpeech::Noun,
                      candidate::kParentheticalReadingCandidateCost, core::LatticeEdge::kIsUnknown, {},
                      dictionary::ConjugationType::None, core::CandidateOrigin::Unknown, candidate::kNoOriginConfidence,
                      {}, core::ExtendedPOS::Noun, "parenthetical_reading");
    }
  }

  // Check for dictionary entries at this position to penalize longer unknown words
  size_t byte_pos = byteOffsetAt(byte_offsets, start_pos);
  auto dict_results = dict_manager_.lookup(text, byte_pos);

  size_t max_dict_length = 0;
  for (const auto& result : dict_results) {
    // Closed classes mark grammatical boundaries but are not lexical evidence
    // against a longer unknown content word beginning at the same character.
    // In particular, a one-kanji suffix such as 内 must not make an entire
    // Sino compound pay the dictionary-length penalty while arbitrary shorter
    // fragments stay cheap.
    if (result.entry != nullptr && core::isContentWord(result.entry->pos) &&
        result.entry->extended_pos != core::ExtendedPOS::NounFormal) {
      max_dict_length = std::max(max_dict_length, result.length);
    }
  }

  // Generate unknown word candidates
  auto candidates = unknown_gen_.generate(text, codepoints, start_pos, char_types);
  const size_t kanji_end =
      start_pos < char_types.size() && char_types[start_pos] == normalize::CharType::Kanji
          ? findCharRegionEnd(char_types, start_pos, char_types.size() - start_pos, normalize::CharType::Kanji)
          : start_pos;
  const size_t following_verb_start = kanji_end - start_pos >= 3
                                          ? longestNominalVerbContinuativeStart(codepoints, char_types, start_pos,
                                                                                kanji_end, inflection_, &dict_manager_)
                                          : kanji_end;

  for (const auto& candidate : candidates) {
    const bool conjunction_before_short_nominal =
        candidate.pos == core::PartOfSpeech::Noun && candidate.end == candidate.start + 1 &&
        hasPrecedingPartOfSpeech(lattice, candidate.start, partOfSpeechMask(core::PartOfSpeech::Conjunction));
    if (conjunction_before_short_nominal &&
        std::any_of(candidates.begin(), candidates.end(), [&](const UnknownCandidate& alternative) {
          return alternative.start == candidate.start && alternative.end > candidate.end &&
                 alternative.pos == core::PartOfSpeech::Verb;
        })) {
      continue;
    }
    if (candidate.pos != core::PartOfSpeech::Particle &&
        startsAtBindingParticleAfterTerminalVerb(lattice, dict_manager_, text, byte_offsets, candidate)) {
      continue;
    }
    if ((candidate.pos == core::PartOfSpeech::Verb || candidate.pos == core::PartOfSpeech::Adjective) &&
        endsWithFinalParticleAfterNominalHead(dict_manager_, text, byte_offsets, candidates, candidate)) {
      continue;
    }
    if (following_verb_start < kanji_end && candidate.pos == core::PartOfSpeech::Noun &&
        candidate.end > following_verb_start && candidate.end <= kanji_end) {
      continue;
    }
    if (candidate.requires_left_content_edge &&
        (candidate.start == 0 || !hasNominalHeadEdgeEndingAt(lattice, candidate.start - 1))) {
      continue;
    }
    if (candidate.requires_left_attributive_edge && !hasAttributiveEdgeEndingAt(lattice, candidate.start)) {
      continue;
    }
    if (candidate.rejects_preceding_content_edge && hasContentEdgeEndingAt(lattice, candidate.start)) {
      continue;
    }
    // A longer content word from any other generator supersedes the rescue, but
    // not another rescue: the generator offers both the maximal run and the run
    // that stops in front of a trailing auxiliary, and those two are meant to
    // compete in the lattice (りんご + だ against りんごだ) rather than one
    // silencing the other before scoring sees them. A predicate whose base form
    // is attested nowhere carries no more evidence than the rescue does, so it
    // does not supersede it either: the irrealis-shaped reconstruction りんごだる
    // would otherwise decide りんごだった before scoring weighed the copula
    // reading. Predicates that kept their dictionary base form still win here,
    // which is what a lexical reading spanning the run is for.
    const auto is_unattested_predicate = [](const UnknownCandidate& alternative) {
      return !alternative.lemma_verified &&
             (alternative.pos == core::PartOfSpeech::Verb || alternative.pos == core::PartOfSpeech::Adjective);
    };
    if (candidate.bracketed_noun_rescue &&
        std::any_of(candidates.begin(), candidates.end(), [&](const UnknownCandidate& alternative) {
          return alternative.start == candidate.start && alternative.end > candidate.end &&
                 !alternative.bracketed_noun_rescue && core::isContentWord(alternative.pos) &&
                 !is_unattested_predicate(alternative);
        })) {
      continue;
    }
    const bool selected_by_following_copula =
        candidate.pos == core::PartOfSpeech::Noun && candidate.end - candidate.start == 2 &&
        candidate.end < codepoints.size() &&
        grammar::startsPredicativeCopula(text.substr(byteOffsetAt(byte_offsets, candidate.end)));
    if (!selected_by_following_copula && candidate.pos != core::PartOfSpeech::Particle &&
        joinsParticleToDictionaryAdverb(lattice, dict_manager_, text, byte_offsets, candidate.start, candidate.end,
                                        candidate.extended_pos)) {
      continue;
    }
    if (overlapsPredicativeNegativeConjecture(lattice, dict_manager_, text, codepoints, byte_offsets, candidate.start,
                                              candidate.end)) {
      continue;
    }
    if (absorbsVerifiedClassicalNegative(lattice, dict_manager_, text, byte_offsets, candidate)) {
      continue;
    }
    if (absorbsPassiveBeforeNegative(lattice, dict_manager_, text, codepoints, byte_offsets, candidate)) {
      continue;
    }
    if (startsInsideVerifiedNounAndAbsorbsSuru(lattice, dict_manager_, text, byte_offsets, candidate)) {
      continue;
    }
    if (consumesInitialSuruConditional(dict_manager_, text, codepoints, byte_offsets, candidates, candidate)) {
      continue;
    }
    if (reopensObservedVerbAuxiliaryBoundary(lattice, dict_manager_, text, byte_offsets, candidate)) {
      continue;
    }
    if (shadowsClosedCausativeAfterNominalHead(lattice, dict_manager_, text, byte_offsets, candidate)) {
      continue;
    }
    if (isInternalPredicateOfSelectedNominalHead(dict_manager_, text, byte_offsets, candidates, candidate)) {
      continue;
    }
    // A mixed-script unknown noun cannot cover a fully evidenced inflectional
    // boundary (知ら+ず).  This is the nominal counterpart of the verb guard
    // below: the left predicate lemma and its EPOS-selected closed auxiliary
    // jointly own the span.  Genuine mixed nouns such as 手がかり have no such
    // two-constituent proof and remain eligible.
    if (candidate.pos == core::PartOfSpeech::Noun &&
        candidate.origin == core::CandidateOrigin::KanjiHiraganaNominalCompound &&
        (isProductiveShiiAdjectiveTerminal(candidate.surface, inflection_) ||
         hasCompleteInternalConstituentBoundary(lattice, dict_manager_, text, byte_offsets, candidates, candidate))) {
      continue;
    }
    // A temporal boundary noun ending in 末 remains complete before a bare
    // continuative (月末|締め). Suppress the fabricated whole-span verb as well
    // as the nominalized-noun fallback; an exact L2 noun still has its own
    // dictionary edge and therefore remains available.
    if (candidate.pos == core::PartOfSpeech::Verb &&
        crossesPeriodEndNominalBoundary(codepoints, char_types, candidate)) {
      continue;
    }
    if (candidate.pos == core::PartOfSpeech::Verb && !candidate.lemma_verified && candidate.start > 0 &&
        hasCompleteInternalConstituentBoundary(lattice, dict_manager_, text, byte_offsets, candidates, candidate)) {
      if (hasPrecedingExtendedPOS(lattice, candidate.start, core::ExtendedPOS::AuxNegativeMai)) {
        continue;
      }
    }
    // Do not reopen a dictionary-evidenced lexical compound from an interior
    // kana and absorb the closed auxiliary/particle immediately outside it
    // (思い出さ+せ, 見落とし+て). The complete compound remains an active edge;
    // this only removes an overlapping alternative that cannot be a morpheme
    // boundary under that lexical analysis.
    const size_t compound_end = verifiedCompoundEndCovering(lattice, candidate.start);
    if (compound_end != 0) {
      bool conflicts_with_compound = candidate.end <= compound_end;
      if (!conflicts_with_compound && candidate.end > compound_end) {
        const std::string_view outside_suffix = textRange(text, byte_offsets, compound_end, candidate.end);
        constexpr PartOfSpeechMask kFunctionWordMask =
            partOfSpeechMask(core::PartOfSpeech::Auxiliary) | partOfSpeechMask(core::PartOfSpeech::Particle);
        conflicts_with_compound = hasExactPartOfSpeech(dict_manager_, outside_suffix, kFunctionWordMask);
      }
      if (conflicts_with_compound) {
        continue;
      }
    }

    // A dictionary-derived onbin span is closed only when its selecting
    // past/connective morpheme follows immediately.  This protects lexical
    // compounds such as 言い損なっ+た without owning unrelated 〜んと or
    // conditional boundaries.
    const size_t dictionary_onbinkei_end = dictionarySokuonbinEndCovering(lattice, candidate.start);
    if (dictionary_onbinkei_end != 0 && candidate.end <= dictionary_onbinkei_end &&
        dictionary_onbinkei_end < codepoints.size() &&
        (codepoints[dictionary_onbinkei_end] == U'た' || codepoints[dictionary_onbinkei_end] == U'て')) {
      continue;
    }

    // A closed determiner is a complete morpheme and cannot be the lexical
    // prefix of an i-adjective. If the remainder independently forms the same
    // full-span adjective, keep that compositional boundary (その+薄暗い), not
    // an unknown adjective spanning both. Validate the remainder through the
    // ordinary generator so productive open-class adjectives need no entries.
    if (candidate.pos == core::PartOfSpeech::Adjective) {
      bool absorbs_determiner = false;
      for (const auto& prefix : dict_results) {
        if (prefix.entry == nullptr || prefix.entry->pos != core::PartOfSpeech::Determiner || prefix.length == 0 ||
            prefix.length >= candidate.end - candidate.start) {
          continue;
        }
        const size_t adjective_start = candidate.start + prefix.length;
        const auto remainder_candidates = unknown_gen_.generate(text, codepoints, adjective_start, char_types);
        absorbs_determiner =
            std::any_of(remainder_candidates.begin(), remainder_candidates.end(), [&](const auto& remainder) {
              return remainder.pos == core::PartOfSpeech::Adjective && remainder.end == candidate.end;
            });
        if (absorbs_determiner) {
          break;
        }
      }
      if (absorbs_determiner) {
        continue;
      }
    }

    // A dictionary kanji-containing i-adjective must not be shadowed by an
    // identical unknown noun fallback. The fallback can otherwise pair with a
    // following suffix (美しい+方) and erase the adjective's grammatical
    // attributive boundary. Pure-hiragana adjectives such as ない remain
    // context-sensitive, so preserve their existing alternative paths.
    // Preserve genuinely ambiguous dictionary surfaces by keeping the fallback
    // when the dictionary also supplies an exact noun entry.
    const bool is_exact_noun_fallback = candidate.pos == core::PartOfSpeech::Noun &&
                                        candidate.end - candidate.start > 1 &&
                                        grammar::containsKanji(candidate.surface);
    if (is_exact_noun_fallback) {
      bool has_exact_adjective = false;
      bool has_exact_noun = false;
      for (const auto& result : dict_results) {
        if (result.entry == nullptr || result.length != candidate.end - candidate.start) {
          continue;
        }
        const bool is_exact_i_adjective = result.entry->pos == core::PartOfSpeech::Adjective &&
                                          result.entry->extended_pos != core::ExtendedPOS::AdjNaAdj;
        has_exact_adjective = has_exact_adjective || is_exact_i_adjective;
        has_exact_noun = has_exact_noun || result.entry->pos == core::PartOfSpeech::Noun;
      }
      if (has_exact_adjective && !has_exact_noun) {
        continue;
      }
    }

    bool is_conjunction_prefix = false;
    for (const auto& result : dict_results) {
      if (result.entry != nullptr && result.entry->pos == core::PartOfSpeech::Conjunction &&
          candidate.end - candidate.start <= result.length) {
        is_conjunction_prefix = true;
        break;
      }
    }
    if (is_conjunction_prefix) {
      continue;
    }

    if (candidate.pos == core::PartOfSpeech::Adjective && utf8::endsWith(candidate.lemma, "がましい")) {
      const bool has_longer_host =
          std::any_of(lattice.edgeIdsEndingAt(candidate.end).begin(), lattice.edgeIdsEndingAt(candidate.end).end(),
                      [&](const uint32_t edge_id) {
                        const auto& edge = lattice.getEdge(edge_id);
                        return edge.start < candidate.start && edge.pos == core::PartOfSpeech::Adjective &&
                               utf8::endsWith(edge.lemma, "がましい");
                      });
      if (has_longer_host) {
        continue;
      }
    }

    if (verb_helpers::startsInsideGaMashiiSuffix(codepoints, candidate.start)) {
      continue;
    }
    if (verb_helpers::crossesKkoNominalizer(codepoints, candidate.start, candidate.end)) {
      continue;
    }

    uint8_t flags = core::LatticeEdge::kIsUnknown;
    float adjusted_cost = candidate.cost;

    // Penalize unknown words that extend beyond dictionary entries
    bool skip_penalty = false;
    [[maybe_unused]] const char* skip_reason = nullptr;

    // Skip penalty for adverbs (onomatopoeia like わくわく)
    if (candidate.pos == core::PartOfSpeech::Adverb) {
      skip_penalty = true;
      skip_reason = "adverb";
    }

    if (!skip_penalty && candidate.pos == core::PartOfSpeech::Noun &&
        isNumeralOkuriganaCounterPhrase(dict_manager_, text, byte_offsets, codepoints, char_types, candidate.start,
                                        candidate.end)) {
      skip_penalty = true;
      skip_reason = "numeral_okurigana_counter";
    }

    if (!skip_penalty &&
        (candidate.pos == core::PartOfSpeech::Verb || candidate.pos == core::PartOfSpeech::Adjective)) {
      // Exception: Don't skip verb candidates ending with ず (adverbialized negatives)
      // e.g., 思わず, 絶えず - these are lexicalized adverbs from verb + ず
      bool ends_with_zu =
          (candidate.surface.size() >= 3 && candidate.surface.substr(candidate.surface.size() - 3) == "ず");
      for (const auto& result : dict_results) {
        if (result.entry != nullptr) {
          // Case 1: Dictionary entry is also a verb/adjective
          // But allow ず-ending candidates (adverbialized forms)
          // Case 1: Dictionary entry is also a verb/adjective
          // But allow ず-ending candidates (adverbialized forms)
          // An explicitly generated irrealis stem is already licensed by a
          // following closed-class inflection (negative, causative, or
          // passive).  A shorter dictionary verb/adjective must not suppress
          // that productive boundary: 確かめ+させる is not 確か+めさせる.
          const bool is_explicit_mizenkei = candidate.origin == CandidateOrigin::VerbKanji &&
                                            candidate.extended_pos == core::ExtendedPOS::VerbMizenkei;
          if ((result.entry->pos == core::PartOfSpeech::Verb || result.entry->pos == core::PartOfSpeech::Adjective) &&
              !ends_with_zu && !candidate.lemma_verified && !is_explicit_mizenkei) {
            skip_penalty = true;
            skip_reason = "dict_has_verb_adj";
            break;
          }
          // Case 2: Pure hiragana verb candidate vs short dictionary entry
          // Also allow prolonged sound mark (ー) as part of hiragana sequence
          // for colloquial patterns like すごーい, やばーい, かわいー
          if (result.length <= 2 && candidate.end - candidate.start >= 3) {
            if (allCharsAre(char_types, codepoints, candidate.start, candidate.end, normalize::CharType::Hiragana,
                            /*allow_choon=*/true) &&
                !isCopulaVolitionalSequence(dict_manager_, text, byte_offsets, candidate.start, candidate.end)) {
              skip_penalty = true;
              skip_reason = "pure_hiragana_verb";
              break;
            }
          }
        }
      }
    }

    // Case 3: Colloquial verb contraction (ておく→っとく)
    // っとく is a valid compound verb ending that shouldn't be penalized for length
    // Note: っちゃう/っじゃう are handled by Case 6 (revoke skip for ちゃう endings)
    if (!skip_penalty && candidate.pos == core::PartOfSpeech::Verb) {
      std::string_view surface = candidate.surface;
      if (utf8::endsWith(surface, "っとく")) {
        skip_penalty = true;
        skip_reason = "colloquial_contraction";
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
        if (allCharsAre(char_types, codepoints, candidate.start, candidate.end, normalize::CharType::Hiragana,
                        /*allow_choon=*/false)) {
          // Check if ends with て or で (te-form markers)
          std::string_view last_char = utf8::lastChar(surface);
          if (grammar::isTeDeSurface(last_char)) {
            skip_penalty = true;
            skip_reason = "short_te_form";
          }
        }
      }
    }

    // Case 6: Revoke skip for long hiragana verbs ending with ちゃう/ちゃっ/ちゃい
    // These are auxiliary chains (e.g., されちゃう = さ+れ+ちゃう,
    // なっちゃう = なっ+ちゃう, やっちゃう = やっ+ちゃう) that should split.
    if (skip_penalty && candidate.pos == core::PartOfSpeech::Verb && candidate.end - candidate.start >= 4) {
      std::string_view surface = candidate.surface;
      bool ends_chau =
          utf8::endsWith(surface, "ちゃう") || utf8::endsWith(surface, "ちゃっ") || utf8::endsWith(surface, "ちゃい");
      if (ends_chau) {
        // Check if all hiragana
        if (allCharsAre(char_types, codepoints, candidate.start, candidate.end, normalize::CharType::Hiragana,
                        /*allow_choon=*/false)) {
          skip_penalty = false;
          skip_reason = nullptr;
        }
      }
    }

    // Case 4: Pure hiragana OTHER (likely readings/furigana)
    // Reduce penalty for long varied hiragana sequences
    // Also allow prolonged sound mark (ー) as part of hiragana sequence
    bool reduced_penalty = false;
    bool skip_dict_penalty = false;
    [[maybe_unused]] const char* skip_dict_reason = nullptr;
    if (candidate.origin == CandidateOrigin::SuffixPattern) {
      skip_dict_penalty = true;
      skip_dict_reason = "verified_suffix_construction";
    } else if (candidate.origin == CandidateOrigin::KanjiHiraganaNominalCompound) {
      skip_dict_penalty = true;
      skip_dict_reason = "nominal_context_compound";
    }
    if (!skip_penalty && candidate.pos == core::PartOfSpeech::Other && candidate.end - candidate.start >= 4) {
      if (allCharsAre(char_types, codepoints, candidate.start, candidate.end, normalize::CharType::Hiragana,
                      /*allow_choon=*/true)) {
        // Reduce penalty only for varied sequences, not runs of one repeated
        // char (ーーーー, ああああ) which are usually noise.
        bool all_same = true;
        char32_t first_cp = 0;
        for (size_t idx = candidate.start; idx < candidate.end && idx < codepoints.size(); ++idx) {
          if (idx == candidate.start) {
            first_cp = codepoints[idx];
          } else if (codepoints[idx] != first_cp) {
            all_same = false;
            break;
          }
        }
        if (!all_same) {
          reduced_penalty = true;
        }
      }
    }

    // Skip dict length penalty for katakana sequences (loanwords)
    // Loanwords like マスカラ, デスクトップ often exceed dictionary coverage
    if (!skip_penalty && candidate.pos == core::PartOfSpeech::Noun && candidate.end - candidate.start >= 3) {
      if (allCharsAre(char_types, codepoints, candidate.start, candidate.end, normalize::CharType::Katakana,
                      /*allow_choon=*/true)) {
        skip_dict_penalty = true;
        skip_dict_reason = "all_katakana";
      }
    }

    // Skip dict length penalty for kanji compound sequences (2-6 chars)
    // Common compounds like 人工知能, 自然言語処理 may not be in dictionary
    // Keep compounds connected - splitting should be driven by PREFIX/SUFFIX
    // markers or dictionary entries, not length heuristics
    if (!skip_penalty && !skip_dict_penalty && candidate.pos == core::PartOfSpeech::Noun) {
      size_t len = candidate.end - candidate.start;
      if (len >= 2 && len <= 6) {
        if (allCharsAre(char_types, codepoints, candidate.start, candidate.end, normalize::CharType::Kanji,
                        /*allow_choon=*/false)) {
          skip_dict_penalty = true;
          skip_dict_reason = "all_kanji_compound";

          // When a dictionary entry exists as a proper prefix of this compound,
          // add a moderate penalty to prefer the dict-split path.
          // E.g., 第一(dict) + 毛 should beat 第一毛(compound)
          // Only when the prefix covers a significant portion (>= half)
          // to avoid splitting 自然言語処理 at 自然(2/6).
          for (const auto& result : dict_results) {
            const auto prefix_codepoints =
                normalize::toCodepoints(result.entry != nullptr ? result.entry->surface : "");
            const bool is_ordinal_noun_prefix =
                result.entry != nullptr && result.entry->pos == core::PartOfSpeech::Noun &&
                prefix_codepoints.size() >= 2 && prefix_codepoints.front() == U'第' &&
                std::all_of(prefix_codepoints.begin() + 1, prefix_codepoints.end(), normalize::isNumeralCodepoint);
            if (result.entry != nullptr && result.length >= 2 && result.length < len && result.length * 2 >= len &&
                (result.entry->pos != core::PartOfSpeech::Noun || is_ordinal_noun_prefix)) {
              // Exception: na-adjective stem + productive noun-forming suffix
              // (性, 的, etc.) is a genuine compound word (重要性, 必要性),
              // not an accidental dict-prefix overlap like その後(ADV)+猫.
              // The productive suffix mechanism (getSuffixEntries/getNaAdjSuffixes)
              // already scores this pattern on its own merits, so skip the
              // generic dict-prefix penalty here.
              if (result.entry->pos == core::PartOfSpeech::Adjective &&
                  result.entry->extended_pos == core::ExtendedPOS::AdjNaAdj) {
                const std::string_view tail_surface =
                    textRange(text, byte_offsets, candidate.start + result.length, candidate.end);
                bool tail_is_productive_suffix = false;
                for (const auto& suffix_entry : getSuffixEntries()) {
                  if (tail_surface == suffix_entry.suffix) {
                    tail_is_productive_suffix = true;
                    break;
                  }
                }
                if (!tail_is_productive_suffix) {
                  for (const auto& na_suffix : getNaAdjSuffixes()) {
                    if (tail_surface == na_suffix) {
                      tail_is_productive_suffix = true;
                      break;
                    }
                  }
                }
                // A na-adjective stem also forms a lexical comparison compound
                // with 以上 (必要以上, 予想以上). Numeral+counter expressions
                // retain their dedicated split candidates in the counter layer.
                bool tail_is_comparison_bound = (tail_surface == "以上");
                if (tail_is_productive_suffix || tail_is_comparison_bound) {
                  continue;
                }
              }
              constexpr float kDictPrefixPenalty = 1.5F;
              adjusted_cost += kDictPrefixPenalty;
              SUZUME_DEBUG_LOG_VERBOSE("[TOK_UNK] \"" << candidate.surface << "\" (NOUN): +" << kDictPrefixPenalty
                                                      << " (kanji_compound_dict_prefix, dict=\""
                                                      << result.entry->surface << "\")\n");
              break;
            }
          }

          // When a non-NOUN dict entry from a prior position overlaps with
          // this compound's first character, penalize the compound.
          // E.g., その後(dict ADV, pos=0, len=3) overlaps with 後猫(pos=2)
          // → penalize 後猫 to prefer その後+猫 split.
          constexpr size_t kMaxLookback = 4;
          bool found_overlap = false;
          for (size_t back = 1; back <= kMaxLookback && back <= start_pos && !found_overlap; ++back) {
            size_t prev_pos = start_pos - back;
            size_t prev_byte = byteOffsetAt(byte_offsets, prev_pos);
            auto prev_results = dict_manager_.lookup(text, prev_byte);
            for (const auto& result : prev_results) {
              if (result.entry != nullptr && result.length >= 2 && result.length > back &&
                  result.entry->pos != core::PartOfSpeech::Noun && result.entry->pos != core::PartOfSpeech::Pronoun) {
                constexpr float kDictOverlapPenalty = 1.5F;
                adjusted_cost += kDictOverlapPenalty;
                SUZUME_DEBUG_LOG_VERBOSE("[TOK_UNK] \"" << candidate.surface << "\" (NOUN): +" << kDictOverlapPenalty
                                                        << " (kanji_compound_dict_overlap, dict=\""
                                                        << result.entry->surface << "\")\n");
                found_overlap = true;
                break;
              }
            }
          }
        }
      }
    }

    // Skip exceeds_dict_length penalty for suffix pattern candidates
    // These are morphologically recognized patterns (e.g., がち, っぽい)
    // that should not be penalized for exceeding dictionary coverage
    // Also skip for katakana loanwords (マスカラ, デスクトップ)
    // Also skip for Suru verb candidates (所在する, 延期する) - these are productive
    bool is_suru_verb =
        (candidate.pos == core::PartOfSpeech::Verb && candidate.conj_type == dictionary::ConjugationType::Suru);

    // Check for pure hiragana verb (e.g., ねる, もらう, あげる)
    // These should not be penalized heavily - they are legitimate verb forms
    bool is_pure_hiragana_verb = false;
    if (candidate.pos == core::PartOfSpeech::Verb && candidate.end - candidate.start >= 2) {
      const size_t candidate_length = candidate.end - candidate.start;
      const bool is_short_form = candidate_length <= 4;
      const bool has_left_predicate_boundary =
          candidate.start == 0 || char_types[candidate.start - 1] == normalize::CharType::Symbol ||
          dict_manager_.lookupExact(textRange(text, byte_offsets, candidate.start - 1, candidate.start),
                                    core::PartOfSpeech::Particle) != nullptr;
      const bool has_right_predicate_boundary =
          candidate.end == codepoints.size() || char_types[candidate.end] == normalize::CharType::Symbol ||
          char_types[candidate.end] == normalize::CharType::Kanji ||
          dict_manager_.lookupExact(textRange(text, byte_offsets, candidate.end, candidate.end + 1),
                                    core::PartOfSpeech::Particle) != nullptr;
      const bool is_bounded_terminal_form = candidate_length <= 8 && candidate.surface.compare(candidate.lemma) == 0 &&
                                            has_left_predicate_boundary && has_right_predicate_boundary;
      if ((is_short_form || is_bounded_terminal_form) &&
          allCharsAre(char_types, codepoints, candidate.start, candidate.end, normalize::CharType::Hiragana,
                      /*allow_choon=*/false)) {
        is_pure_hiragana_verb = true;
      }
    }
    if (is_pure_hiragana_verb &&
        isCopulaVolitionalSequence(dict_manager_, text, byte_offsets, candidate.start, candidate.end)) {
      is_pure_hiragana_verb = false;
    }

    // Check for single-kanji stem + hiragana verb (e.g., 残って, 通る, 飛ぶ)
    // Single-kanji verb stems are common in Japanese (残る, 立つ, 打つ, etc.)
    // These should not be penalized for exceeding dict length
    bool is_kanji_stem_verb = false;
    if (candidate.pos == core::PartOfSpeech::Verb && candidate.end - candidate.start >= 2 &&
        candidate.start < char_types.size() && char_types[candidate.start] == normalize::CharType::Kanji) {
      // Check: first char is kanji, rest are hiragana
      if (allCharsAre(char_types, codepoints, candidate.start + 1, candidate.end, normalize::CharType::Hiragana,
                      /*allow_choon=*/false)) {
        is_kanji_stem_verb = true;
      }
    }

    bool exceeds_dict = (max_dict_length > 0 && candidate.end - candidate.start > max_dict_length);
    bool absorbs_suru_imperative = false;
    if (candidate.pos == core::PartOfSpeech::Verb && candidate.end - candidate.start >= 4) {
      for (size_t split_pos = candidate.start + 2; split_pos < candidate.end; ++split_pos) {
        if (!allCharsAre(char_types, codepoints, candidate.start, split_pos, normalize::CharType::Kanji,
                         /*allow_choon=*/false)) {
          continue;
        }
        if (grammar::isSuruImperativeSurface(textRange(text, byte_offsets, split_pos, candidate.end))) {
          absorbs_suru_imperative = true;
          break;
        }
      }
    }
    if (absorbs_suru_imperative) {
      continue;
    }
    if (exceeds_dict) {
      if (skip_penalty) {
        SUZUME_DEBUG_LOG_VERBOSE("[TOK_SKIP] \"" << candidate.surface << "\" (" << core::posToString(candidate.pos)
                                                 << "): "
                                                 << "skip exceeds_dict_length (" << skip_reason << ")\n");
      } else if (skip_dict_penalty) {
        SUZUME_DEBUG_LOG_VERBOSE("[TOK_SKIP] \"" << candidate.surface << "\" (" << core::posToString(candidate.pos)
                                                 << "): "
                                                 << "skip exceeds_dict_length (" << skip_dict_reason << ")\n");
      } else if (is_suru_verb) {
        SUZUME_DEBUG_LOG_VERBOSE("[TOK_SKIP] \"" << candidate.surface << "\" (" << core::posToString(candidate.pos)
                                                 << "): "
                                                 << "skip exceeds_dict_length (suru_verb)\n");
      } else if (candidate.has_suffix) {
        SUZUME_DEBUG_LOG_VERBOSE("[TOK_SKIP] \"" << candidate.surface << "\" (" << core::posToString(candidate.pos)
                                                 << "): "
                                                 << "skip exceeds_dict_length (has_suffix)\n");
      } else if (is_pure_hiragana_verb) {
        SUZUME_DEBUG_LOG_VERBOSE("[TOK_SKIP] \"" << candidate.surface << "\" (" << core::posToString(candidate.pos)
                                                 << "): "
                                                 << "skip exceeds_dict_length (pure_hiragana_verb)\n");
      } else if (is_kanji_stem_verb) {
        SUZUME_DEBUG_LOG_VERBOSE("[TOK_SKIP] \"" << candidate.surface << "\" (" << core::posToString(candidate.pos)
                                                 << "): "
                                                 << "skip exceeds_dict_length (kanji_stem_verb)\n");
      } else {
        float penalty = reduced_penalty ? 1.0F : 3.5F;
        adjusted_cost += penalty;
        SUZUME_DEBUG_LOG_VERBOSE("[TOK_UNK] \"" << candidate.surface << "\" (" << core::posToString(candidate.pos)
                                                << "): +" << penalty << " (exceeds_dict_length"
                                                << (reduced_penalty ? ", pure_hiragana" : "")
                                                << ", dict_max=" << max_dict_length << ")\n");
      }
    }

    // For verb candidates, check if the hiragana suffix is a known particle
    if (candidate.pos == core::PartOfSpeech::Verb && candidate.end > candidate.start) {
      size_t hiragana_start = candidate.start;
      while (hiragana_start < candidate.end && hiragana_start < char_types.size() &&
             char_types[hiragana_start] != normalize::CharType::Hiragana) {
        ++hiragana_start;
      }

      if (hiragana_start < candidate.end) {
        size_t suffix_byte_start = byteOffsetAt(byte_offsets, hiragana_start);
        size_t suffix_byte_end = byteOffsetAt(byte_offsets, candidate.end);
        std::string_view hiragana_suffix = text.substr(suffix_byte_start, suffix_byte_end - suffix_byte_start);

        // Don't penalize verb conjugation endings
        // - te-form: て/で/って/んで/いて/いで
        // - renyoukei し: extremely common for suru/godan verbs (分割し, 話し)
        bool is_verb_ending = utf8::equalsAny(hiragana_suffix, {"て", "で", "って", "んで", "いて", "いで", "し"}) ||
                              candidate.extended_pos == core::ExtendedPOS::VerbRenyokei;

        // Skip penalty if:
        // - Known verb conjugation ending (te-form, renyoukei)
        // - Candidate has has_suffix flag (mizenkei for ぬ/れべき patterns)
        if (!is_verb_ending && !candidate.has_suffix) {
          if (dict_manager_.lookupExact(hiragana_suffix, core::PartOfSpeech::Particle) != nullptr) {
            adjusted_cost += 1.5F;
            SUZUME_DEBUG_LOG_VERBOSE("[TOK_UNK] \"" << candidate.surface << "\": +1.5 (particle_suffix=\""
                                                    << hiragana_suffix << "\")\n");
          }
        }
      }
    }

    std::string surface_str(candidate.surface);

    // Relay dict-verified-lemma marking so the scorer can exempt genuine verb
    // onbin forms from the spurious-onbin penalty.
    if (candidate.lemma_verified) {
      flags |= static_cast<uint8_t>(core::EdgeFlags::LemmaVerified);
    }

    // A continuative predicate immediately selected by a nominal particle is
    // productively usable as a deverbal noun (隔たり+を, 読み+が). Preserve
    // that POS alternative without registering each open-class nominalization.
    if (candidate.pos == core::PartOfSpeech::Verb && candidate.extended_pos == core::ExtendedPOS::VerbRenyokei &&
        candidate.end < codepoints.size()) {
      const bool nominal_particle = hasNominalForcingParticleContinuation(codepoints, candidate.end, &dict_manager_);
      // A particle homograph must not hide a longer dependent predicate or
      // closed derivational suffix beginning at the same boundary
      // (理解+し+がたい, 読み+やすい, 遅刻+し+がち).
      // Longest closed-class evidence takes priority over the nominalized
      // renyokei alternative; a standalone が/を still licenses it.
      bool longer_dependent_follows = false;
      const size_t probe_end = std::min(codepoints.size(), candidate.end + static_cast<size_t>(4));
      const std::string_view following_probe = textRange(text, byte_offsets, candidate.end, probe_end);
      for (const auto& result : dict_manager_.lookup(following_probe, 0)) {
        if (result.entry != nullptr && result.length > 1 &&
            (result.entry->pos == core::PartOfSpeech::Adjective || result.entry->pos == core::PartOfSpeech::Auxiliary ||
             result.entry->extended_pos == core::ExtendedPOS::SuffixTendency)) {
          longer_dependent_follows = true;
          break;
        }
      }
      const auto same_surface_entries = dict_manager_.lookup(surface_str, 0);
      const bool has_lexical_nonverb_reading =
          std::any_of(same_surface_entries.begin(), same_surface_entries.end(), [&](const auto& match) {
            return match.entry != nullptr && match.length == candidate.end - candidate.start &&
                   match.entry->pos != core::PartOfSpeech::Verb;
          });
      const bool is_complete_shii_adjective = isProductiveShiiAdjectiveTerminal(surface_str, inflection_);
      // The one-mora と is case-particle-shaped in the dictionary, but after a
      // complete predicate it marks quotation. In that position even a
      // productive adjective whose lemma equals its terminal surface is enough
      // to reject a fabricated deverbal-noun homograph. Other nominal particles
      // retain the ambiguity needed by おもい+が/を.
      const bool follows_predicate_quote =
          nominal_particle && candidate.end < codepoints.size() && codepoints[candidate.end] == core::hiragana::kTo;
      const auto* following_particle = candidate.end < codepoints.size()
                                           ? lookupEntryInRange(dict_manager_, codepoints, candidate.end,
                                                                candidate.end + 1, core::PartOfSpeech::Particle)
                                           : nullptr;
      const bool follows_nominalizer =
          following_particle != nullptr && following_particle->extended_pos == core::ExtendedPOS::ParticleNo;
      // A finished adjective reading of the same span is a complete inflectional
      // analysis, so a deverbal re-reading built on a fabricated verb must stand
      // down: 高かれ+と is the カリ 命令形 of 高い, not a noun from 高かれる. The
      // lemma normally has to differ from the surface, unless the following
      // quotation particle supplies the predicate-position evidence.
      const bool same_span_adjective_analysis =
          std::any_of(candidates.begin(), candidates.end(), [&](const UnknownCandidate& other) {
            return other.pos == core::PartOfSpeech::Adjective && other.extended_pos == core::ExtendedPOS::AdjBasic &&
                   other.start == candidate.start && other.end == candidate.end &&
                   (other.lemma != other.surface || follows_predicate_quote || follows_nominalizer);
          });
      const bool crosses_complete_internal_boundary =
          hasCompleteInternalConstituentBoundary(lattice, dict_manager_, text, byte_offsets, candidates, candidate);
      const bool crosses_noun_nagara_ni_boundary =
          hasNounNagaraNiBoundary(lattice, dict_manager_, text, codepoints, byte_offsets, candidates, candidate);
      // The nominalization is a POS re-reading of an accepted continuative, not
      // independent evidence for the span. Once the verb path prices the span
      // worse than an outright unknown word (秋来ぬ, charged as the fabricated
      // サ変 秋来する across a clause boundary), there is no continuative left to
      // re-read, so the bonus must not resurrect it. Ordinary deverbal nouns
      // (身なり, 足取り) stay well inside the unknown-word band.
      const bool verb_reading_rejected = adjusted_cost > getCategoryCost(core::ExtendedPOS::Unknown);
      const bool bound_suffix_after_host =
          verb_helpers::isBoundSuffixAfterNominalHost(&dict_manager_, codepoints, candidate.start, candidate.surface);
      // A particle is a closed boundary, never the first mora of a deverbal
      // nominalization.  This prevents an unverified kana verb hypothesis
      // from absorbing the genitive in productive sequences such as の+わり
      // and の+うち.
      const bool starts_with_closed_particle =
          lookupEntryInRange(dict_manager_, codepoints, candidate.start, candidate.start + 1,
                             core::PartOfSpeech::Particle) != nullptr;
      // A deverbal noun re-reads the continuative cell on its own. An analysis
      // that had to match an auxiliary chain to reach the lemma describes a
      // complete predicate instead (ためさ + ない), so nominalizing that span
      // would bury the auxiliary — here the negation — inside the noun. The
      // productive nominalizations reach their lemma from the bare cell and
      // carry no chain (読み, 身なり, 隔たり).
      // A quotation closes a clause, so a span in front of it is read as a
      // predicate wherever it can be one. When the continuative's own tail
      // spells a complete multi-mora auxiliary and its lemma is a fabrication,
      // that auxiliary is the predicate: 金なり+と is 金 + なり (the classical
      // copula) under quotation, not a deverbal noun from the non-word 金なる.
      // The other nominal particles select an argument instead of closing a
      // clause, so they keep the ambiguity that 身なり+を needs.
      bool auxiliary_tail_before_quote = false;
      if (follows_predicate_quote && !verb_helpers::isVerbInDictionary(&dict_manager_, candidate.lemma)) {
        // A one-mora tail is also the last mora of ordinary words, so only
        // multi-mora auxiliaries count; none of the closed class is longer than
        // four morae.
        constexpr size_t kMaxAuxiliaryLen = 4;
        const size_t max_len = std::min(kMaxAuxiliaryLen, candidate.end - candidate.start - 1);
        for (size_t tail_len = 2; tail_len <= max_len; ++tail_len) {
          if (lookupEntryInRange(dict_manager_, codepoints, candidate.end - tail_len, candidate.end,
                                 core::PartOfSpeech::Auxiliary) != nullptr) {
            auxiliary_tail_before_quote = true;
            break;
          }
        }
      }
      const auto& span_analyses = inflection_.analyze(surface_str);
      const bool carries_auxiliary_chain = std::any_of(
          span_analyses.begin(), span_analyses.end(),
          [&](const auto& analysis) { return analysis.base_form == candidate.lemma && !analysis.morphemes.empty(); });
      if (nominal_particle && !longer_dependent_follows && !has_lexical_nonverb_reading &&
          !is_complete_shii_adjective && !same_span_adjective_analysis && !crosses_complete_internal_boundary &&
          !crosses_noun_nagara_ni_boundary && !verb_reading_rejected && !bound_suffix_after_host &&
          !candidate.has_suffix && !starts_with_closed_particle && !carries_auxiliary_chain &&
          !auxiliary_tail_before_quote) {
        lattice.addEdge(surface_str, static_cast<uint32_t>(candidate.start), static_cast<uint32_t>(candidate.end),
                        core::PartOfSpeech::Noun,
                        getCategoryCost(core::ExtendedPOS::NounVerbal) + candidate::kNominalizedNounParticleBonus,
                        flags, surface_str, dictionary::ConjugationType::None, candidate.origin,
                        candidate::kNoOriginConfidence, "nominalized_renyokei_before_particle",
                        core::ExtendedPOS::NounVerbal, "nominalized_renyokei_before_particle");
      }
    }

    lattice.addEdge(surface_str, static_cast<uint32_t>(candidate.start), static_cast<uint32_t>(candidate.end),
                    candidate.pos, adjusted_cost, flags, candidate.lemma, candidate.conj_type, candidate.origin,
#ifdef SUZUME_DEBUG_INFO
                    candidate.confidence, candidate.pattern, candidate.extended_pos, candidate.epos_source);
#else
                    0.0F, {}, candidate.extended_pos);
#endif
  }
}

}  // namespace suzume::analysis
