/**
 * @file tokenizer_unknown_guards.cpp
 * @brief Unknown-word candidates rejected before they reach the lattice
 */

#include <algorithm>

#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/honorific_verbs.h"
#include "normalize/utf8.h"
#include "tokenizer_unknown_internal.h"
#include "verb_candidates_auxiliary_patterns.h"
#include "verb_candidates_dictionary_probes.h"

namespace suzume::analysis::tokenizer_unknown_detail {

namespace {

using suzume::analysis::verb_helpers::isProductiveShiiAdjectiveTerminal;

bool isMultiKanjiSurface(std::string_view surface) {
  return normalize::utf8Length(surface) >= 2 && grammar::isAllKanji(surface);
}

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
// notion stays local to this bracket test. The nominalizer の closes a noun
// phrase the same way (あるく+の|が|すき).
bool hasNominalHeadEdgeEndingAt(const core::Lattice& lattice, size_t boundary) {
  return core::anyEdgeEndingAt(lattice, boundary, [](const core::LatticeEdge& edge) {
    return core::isContentWord(edge.pos) || edge.pos == core::PartOfSpeech::Pronoun ||
           edge.extended_pos == core::ExtendedPOS::ParticleNo;
  });
}

// The past auxiliary closes the clause it marks and is attributive in that
// position (追わ+れ+た | ねずみ), exactly as the verb's own ta-form is when the
// tense is written inside the predicate. Leaving it out made the same bracket
// depend on whether the predicate happened to carry an auxiliary chain.
bool hasAttributiveEdgeEndingAt(const core::Lattice& lattice, size_t boundary) {
  return core::anyEdgeEndingAt(lattice, boundary, [](const core::LatticeEdge& edge) {
    return (edge.pos == core::PartOfSpeech::Verb && (edge.extended_pos == core::ExtendedPOS::VerbShuushikei ||
                                                     edge.extended_pos == core::ExtendedPOS::VerbRentaikei ||
                                                     edge.extended_pos == core::ExtendedPOS::VerbTaForm)) ||
           (edge.pos == core::PartOfSpeech::Adjective && edge.extended_pos == core::ExtendedPOS::AdjBasic) ||
           (edge.pos == core::PartOfSpeech::Auxiliary && edge.extended_pos == core::ExtendedPOS::AuxTenseTa);
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

// A fabricated predicate cannot end on the past auxiliary when the morpheme in
// front of that auxiliary is a closed auxiliary the lattice already proved
// (追わ + れ + た). The past marker closes the chain those two opened, so a base
// reconstructed across it is reading tense as okurigana — the classical
// perfective つ makes 追われた look like the irrealis of 追われつ as soon as a
// negative-shaped mora follows. Requiring a dictionary auxiliary rather than any
// left edge keeps ordinary irrealis cells that merely end in た (立た+ない)
// eligible, since nothing closed precedes their final mora.
bool absorbsPastAfterProvenAuxiliary(const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                     const ByteOffsets& byte_offsets, const UnknownCandidate& candidate) {
  if (candidate.pos != core::PartOfSpeech::Verb || candidate.lemma_verified || candidate.end < candidate.start + 3) {
    return false;
  }
  const size_t past_start = candidate.end - 1;
  const auto* past =
      dict_manager.lookupExact(textRange(text, byte_offsets, past_start, candidate.end), core::PartOfSpeech::Auxiliary);
  if (past == nullptr || past->extended_pos != core::ExtendedPOS::AuxTenseTa) {
    return false;
  }
  const auto* inner = dict_manager.lookupExact(textRange(text, byte_offsets, past_start - 1, past_start),
                                               core::PartOfSpeech::Auxiliary);
  return inner != nullptr;
}

// True unless the する form spelled by |suffix| is an irrealis that no
// auxiliary selecting an irrealis follows.
bool suruIrrealisHasSelectedAuxiliary(const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                      const ByteOffsets& byte_offsets, const UnknownCandidate& candidate,
                                      std::string_view suffix) {
  const size_t suffix_length = normalize::utf8Length(suffix);
  const auto suru_forms = dict_manager.lookup(suffix, 0);
  const bool is_irrealis = std::any_of(suru_forms.begin(), suru_forms.end(), [&](const auto& match) {
    return match.entry != nullptr && match.length == suffix_length && match.entry->pos == core::PartOfSpeech::Verb &&
           match.entry->extended_pos == core::ExtendedPOS::VerbMizenkei;
  });
  if (!is_irrealis) {
    return true;
  }
  const auto followers = dict_manager.lookup(text, byteOffsetAt(byte_offsets, candidate.end));
  return std::any_of(followers.begin(), followers.end(), [](const auto& match) {
    return match.entry != nullptr && match.entry->pos == core::PartOfSpeech::Auxiliary &&
           (match.entry->extended_pos == core::ExtendedPOS::AuxNegativeNu ||
            match.entry->extended_pos == core::ExtendedPOS::AuxPassive);
  });
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
    // The irrealis of する is only a host for the auxiliary it selects, so a
    // candidate ending there is a サ変 chain only when that auxiliary follows
    // (勉強+せ+ず); before た or て it is a different verb (全部+任せ+た).
    if (!suruIrrealisHasSelectedAuxiliary(dict_manager, text, byte_offsets, candidate, suffix)) {
      continue;
    }
    if (core::anyEdgeEndingAt(lattice, suru_start, [&candidate](const core::LatticeEdge& noun) {
          return noun.start < candidate.start && noun.pos == core::PartOfSpeech::Noun &&
                 isMultiKanjiSurface(noun.surface);
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
           isMultiKanjiSurface(noun.surface);
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
         ((auxiliary_epos == core::ExtendedPOS::AuxClassicalBeshi ||
           auxiliary_epos == core::ExtendedPOS::AuxClassicalConjectureTerminal) &&
          verb_epos == core::ExtendedPOS::VerbShuushikei);
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

// A generated predicate cannot consume a multi-mora final particle after a
// nominal head. The final particle closes the nominal predicate (本+ばい), and
// the ordinary open-class candidate must leave that closed boundary intact.
bool endsWithFinalParticleAfterNominalHead(const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                           const ByteOffsets& byte_offsets,
                                           const std::vector<UnknownCandidate>& batch_candidates,
                                           const UnknownCandidate& candidate) {
  // A cell closing on ん is the contracted negative or terminal of its own
  // stem (終わ+ん+ない, 終わ+ん+の ← 終わる), whose ん no final particle can claim.
  const bool contracted_cell = (candidate.extended_pos == core::ExtendedPOS::VerbMizenkei ||
                                candidate.extended_pos == core::ExtendedPOS::VerbShuushikei) &&
                               utf8::endsWith(candidate.surface, "ん");
  if (candidate.lemma_verified || candidate.end <= candidate.start + 2 || contracted_cell) {
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
    // One spelled like a godan terminal ends a kana verb instead (うし+なう is
    // the verb うしなう), as no script boundary separates it from the head.
    if (grammar::endsWithURow(particle->surface) &&
        grammar::isPureHiragana(textRange(text, byte_offsets, candidate.start, particle_start))) {
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

}  // namespace

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
      // An unverified host has to close on a script boundary to be evidence at
      // all: one ending in hiragana ends in the very mora whose analysis is in
      // question (花び of 花びら), so reading it as a nominal assumes the split
      // it is meant to prove.  This is the licensing counterpart of the same
      // test on the prefix above; a katakana or kanji host (テスト向け, 家庭向け)
      // is unaffected because neither ends in kana under analysis.
      const bool licenses_suffix = right_is_suffix && left_pos == core::PartOfSpeech::Noun &&
                                   (left_verified || !grammar::endsWithHiragana(left_surface));
      return licenses_adjective || licenses_auxiliary || licenses_formal_noun || licenses_connective_particle ||
             licenses_nominal_particle || licenses_suffix;
    };
    bool complete_left = core::anyEdgeEndingAt(lattice, split, [&](const core::LatticeEdge& edge) {
      return edge.start <= candidate.start && left_licenses_right(edge.pos, edge.extended_pos, edge.lemmaVerified(),
                                                                  edge.start < candidate.start, edge.surface);
    });
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

bool isRejectedUnknownCandidate(const UnknownCandidateContext& ctx, const core::Lattice& lattice,
                                const UnknownCandidate& candidate) {
  const auto& dict_manager = ctx.dict_manager;
  const auto& inflection = ctx.inflection;
  const auto& unknown_gen = ctx.unknown_gen;
  const std::string_view text = ctx.text;
  const auto& codepoints = ctx.codepoints;
  const auto& byte_offsets = ctx.byte_offsets;
  const auto& char_types = ctx.char_types;
  const auto& dict_results = ctx.dict_results;
  const auto& candidates = ctx.candidates;
  const size_t kanji_end = ctx.kanji_end;
  const size_t following_verb_start = ctx.following_verb_start;
  const bool conjunction_before_short_nominal =
      candidate.pos == core::PartOfSpeech::Noun && candidate.end == candidate.start + 1 &&
      hasPrecedingPartOfSpeech(lattice, candidate.start, partOfSpeechMask(core::PartOfSpeech::Conjunction));
  if (conjunction_before_short_nominal &&
      std::any_of(candidates.begin(), candidates.end(), [&](const UnknownCandidate& alternative) {
        return alternative.start == candidate.start && alternative.end > candidate.end &&
               alternative.pos == core::PartOfSpeech::Verb;
      })) {
    return true;
  }
  // A run whose continuative し is a listed Godan-sa verb (見逃し of 見逃す) is
  // that verb's stem; read as a noun it would take する, which the verb forecloses.
  if (candidate.pos == core::PartOfSpeech::Noun && candidate.end < codepoints.size() &&
      verb_helpers::isListedGodanSaContinuativeRun(&dict_manager, codepoints, candidate.start, candidate.end)) {
    return true;
  }
  if (candidate.pos != core::PartOfSpeech::Particle &&
      startsAtBindingParticleAfterTerminalVerb(lattice, dict_manager, text, byte_offsets, candidate)) {
    return true;
  }
  if ((candidate.pos == core::PartOfSpeech::Verb || candidate.pos == core::PartOfSpeech::Adjective) &&
      endsWithFinalParticleAfterNominalHead(dict_manager, text, byte_offsets, candidates, candidate)) {
    return true;
  }
  // A numeral+counter carries its own boundary evidence (一日+付け), so the
  // counter is not a noun cut into a continuative.
  if (following_verb_start < kanji_end && candidate.pos == core::PartOfSpeech::Noun &&
      candidate.origin != CandidateOrigin::Counter && candidate.end > following_verb_start &&
      candidate.end <= kanji_end) {
    return true;
  }
  if (candidate.requires_left_content_edge &&
      (candidate.start == 0 || !hasNominalHeadEdgeEndingAt(lattice, candidate.start - 1))) {
    return true;
  }
  if (candidate.requires_left_attributive_edge && !hasAttributiveEdgeEndingAt(lattice, candidate.start)) {
    return true;
  }
  if (candidate.rejects_preceding_content_edge && hasContentEdgeEndingAt(lattice, candidate.start)) {
    return true;
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
    return true;
  }
  const bool selected_by_following_copula =
      candidate.pos == core::PartOfSpeech::Noun && candidate.end - candidate.start == 2 &&
      candidate.end < codepoints.size() &&
      grammar::startsPredicativeCopula(text.substr(byteOffsetAt(byte_offsets, candidate.end)));
  if (!selected_by_following_copula && candidate.pos != core::PartOfSpeech::Particle &&
      joinsParticleToDictionaryAdverb(lattice, dict_manager, text, byte_offsets, candidate.start, candidate.end,
                                      candidate.extended_pos)) {
    return true;
  }
  if (overlapsPredicativeNegativeConjecture(lattice, dict_manager, text, codepoints, byte_offsets, candidate.start,
                                            candidate.end)) {
    return true;
  }
  if (absorbsVerifiedClassicalNegative(lattice, dict_manager, text, byte_offsets, candidate)) {
    return true;
  }
  if (absorbsPassiveBeforeNegative(lattice, dict_manager, text, codepoints, byte_offsets, candidate)) {
    return true;
  }
  if (absorbsPastAfterProvenAuxiliary(dict_manager, text, byte_offsets, candidate)) {
    return true;
  }
  if (startsInsideVerifiedNounAndAbsorbsSuru(lattice, dict_manager, text, byte_offsets, candidate)) {
    return true;
  }
  if (consumesInitialSuruConditional(dict_manager, text, codepoints, byte_offsets, candidates, candidate)) {
    return true;
  }
  // No native predicate opens on the ら column, so an unattested one starting
  // there inside a kana run has begun inside a word (あつく+るしい).
  if (is_unattested_predicate(candidate) && candidate.start > 0 &&
      char_types[candidate.start - 1] == normalize::CharType::Hiragana &&
      kana::isRaColumnCodepoint(codepoints[candidate.start])) {
    return true;
  }
  if (reopensObservedVerbAuxiliaryBoundary(lattice, dict_manager, text, byte_offsets, candidate)) {
    return true;
  }
  if (shadowsClosedCausativeAfterNominalHead(lattice, dict_manager, text, byte_offsets, candidate)) {
    return true;
  }
  if (isInternalPredicateOfSelectedNominalHead(dict_manager, text, byte_offsets, candidates, candidate)) {
    return true;
  }
  // A mixed-script unknown noun cannot cover a fully evidenced inflectional
  // boundary (知ら+ず).  This is the nominal counterpart of the verb guard
  // below: the left predicate lemma and its EPOS-selected closed auxiliary
  // jointly own the span.  Genuine mixed nouns such as 手がかり have no such
  // two-constituent proof and remain eligible.
  if (candidate.pos == core::PartOfSpeech::Noun &&
      candidate.origin == core::CandidateOrigin::KanjiHiraganaNominalCompound &&
      (isProductiveShiiAdjectiveTerminal(candidate.surface, inflection) ||
       hasCompleteInternalConstituentBoundary(lattice, dict_manager, text, byte_offsets, candidates, candidate))) {
    return true;
  }
  // A temporal boundary noun ending in 末 remains complete before a bare
  // continuative (月末|締め). Suppress the fabricated whole-span verb as well
  // as the nominalized-noun fallback; an exact L2 noun still has its own
  // dictionary edge and therefore remains available.
  if (candidate.pos == core::PartOfSpeech::Verb && crossesPeriodEndNominalBoundary(codepoints, char_types, candidate)) {
    return true;
  }
  if (candidate.pos == core::PartOfSpeech::Verb && !candidate.lemma_verified && candidate.start > 0 &&
      hasCompleteInternalConstituentBoundary(lattice, dict_manager, text, byte_offsets, candidates, candidate)) {
    if (hasPrecedingExtendedPOS(lattice, candidate.start, core::ExtendedPOS::AuxNegativeMai)) {
      return true;
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
    if (!conflicts_with_compound) {
      const std::string_view outside_suffix = textRange(text, byte_offsets, compound_end, candidate.end);
      constexpr PartOfSpeechMask kFunctionWordMask =
          partOfSpeechMask(core::PartOfSpeech::Auxiliary) | partOfSpeechMask(core::PartOfSpeech::Particle);
      conflicts_with_compound = hasExactPartOfSpeech(dict_manager, outside_suffix, kFunctionWordMask);
    }
    if (conflicts_with_compound) {
      return true;
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
    return true;
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
      const auto remainder_candidates = unknown_gen.generate(text, codepoints, adjective_start, char_types);
      absorbs_determiner =
          std::any_of(remainder_candidates.begin(), remainder_candidates.end(), [&](const auto& remainder) {
            return remainder.pos == core::PartOfSpeech::Adjective && remainder.end == candidate.end;
          });
      if (absorbs_determiner) {
        break;
      }
    }
    if (absorbs_determiner) {
      return true;
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
                                      candidate.end - candidate.start > 1 && grammar::containsKanji(candidate.surface);
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
      return true;
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
    return true;
  }

  if (candidate.pos == core::PartOfSpeech::Adjective && utf8::endsWith(candidate.lemma, "がましい")) {
    const bool has_longer_host = core::anyEdgeEndingAt(lattice, candidate.end, [&](const core::LatticeEdge& edge) {
      return edge.start < candidate.start && edge.pos == core::PartOfSpeech::Adjective &&
             utf8::endsWith(edge.lemma, "がましい");
    });
    if (has_longer_host) {
      return true;
    }
  }

  if (verb_helpers::startsInsideGaMashiiSuffix(codepoints, candidate.start)) {
    return true;
  }
  if (verb_helpers::crossesKkoNominalizer(codepoints, candidate.start, candidate.end)) {
    return true;
  }
  return false;
}

}  // namespace suzume::analysis::tokenizer_unknown_detail
