/**
 * @file tokenizer_dictionary_homograph_guards.cpp
 * @brief Dictionary readings that lose to a homograph the neighbouring category selects
 */

#include <algorithm>

#include "analysis/dictionary_probe.h"
#include "candidate_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "normalize/utf8.h"
#include "tokenizer_dictionary_internal.h"

namespace suzume::analysis::tokenizer_dictionary_detail {

namespace {

// A polite-auxiliary homograph is not a real boundary when it begins inside a
// longer, dictionary-verified verb renyokei ending at the same position
// (醒まし/て, さまし/て).  Requiring both the shared end and verified lemma
// keeps ordinary polite chains such as 食べ/まし/て and 読み/まし/て intact.
bool hasCoveringVerifiedVerbRenyokei(const core::Lattice& lattice, size_t interior_start, size_t shared_end) {
  return core::anyEdgeEndingAt(lattice, shared_end, [interior_start](const core::LatticeEdge& edge) {
    return edge.start < interior_start && edge.extended_pos == core::ExtendedPOS::VerbRenyokei && edge.lemmaVerified();
  });
}

}  // namespace

bool losesHomographReading(const DictionaryCandidateContext& ctx, const core::Lattice& lattice,
                           const dictionary::LookupResult& result, size_t end_pos, std::string_view following_text) {
  const auto& dict_manager = ctx.dict_manager;
  const auto& inflection = ctx.inflection;
  const std::string_view text = ctx.text;
  const auto& codepoints = ctx.codepoints;
  const auto& byte_offsets = ctx.byte_offsets;
  const size_t start_pos = ctx.start_pos;
  const auto& lookup_results = ctx.lookup_results;
  // A determiner must introduce a nominal constituent.  If a closed case
  // particle starts exactly where this candidate ends, the homographic
  // surface belongs to a compositional predicate instead (と+いう+より),
  // so do not admit the fused determiner path at all.  This is a category
  // constraint, independent of the individual determiner or particle.
  const bool ends_at_sentence_boundary =
      end_pos >= codepoints.size() || normalize::classifyChar(codepoints[end_pos]) == normalize::CharType::Symbol;
  if (result.entry->pos == core::PartOfSpeech::Determiner && ends_at_sentence_boundary &&
      isDictionaryOnbinPast(dict_manager, result.entry->surface)) {
    return true;
  }
  if (result.entry->pos == core::PartOfSpeech::Determiner && ends_at_sentence_boundary) {
    const bool has_same_span_predicate =
        lookupResultsHavePartOfSpeech(lookup_results, kVerbAdjectiveMask, result.length);
    if (has_same_span_predicate) {
      return true;
    }
  }
  if (result.entry->pos == core::PartOfSpeech::Determiner && ends_at_sentence_boundary &&
      hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::AuxCopulaDa) && start_pos >= 2 &&
      codepoints[start_pos - 2] == U'の' &&
      hasPrecedingExtendedPOS(lattice, start_pos - 1, core::ExtendedPOS::ParticleNo)) {
    return true;
  }
  if (result.entry->pos == core::PartOfSpeech::Determiner && end_pos < codepoints.size()) {
    const size_t following_byte_pos = byteOffsetAt(byte_offsets, end_pos);
    const auto following_results = dict_manager.lookup(text, following_byte_pos);
    const bool followed_by_case_particle =
        lookupResultsHaveExtendedPOS(following_results, core::ExtendedPOS::ParticleCase);
    // A longer nominal headword starting there is what the determiner
    // modifies (そういう+ところ, こういう+とこ), not the particle.
    const bool followed_by_longer_nominal =
        lookupResultsHaveLongerPartOfSpeech(following_results, partOfSpeechMask(core::PartOfSpeech::Noun), 1);
    // So is an unregistered kana noun the rescue path offers there: a run
    // with no reading of its own, closed by the clause end or a particle
    // (この+へや+、, この+へや+で). Its first mora only spells the particle.
    const auto opens_unread_kana_noun = [&]() {
      constexpr size_t kMaxUnreadNounLength = 4;
      size_t run_end = end_pos;
      while (run_end < codepoints.size() && run_end - end_pos < kMaxUnreadNounLength &&
             normalize::classifyChar(codepoints[run_end]) == normalize::CharType::Hiragana) {
        ++run_end;
      }
      for (size_t noun_end = end_pos + 2; noun_end <= run_end; ++noun_end) {
        const bool closed = noun_end >= codepoints.size() ||
                            normalize::classifyChar(codepoints[noun_end]) != normalize::CharType::Hiragana ||
                            lookupEntryInRange(dict_manager, codepoints, noun_end, noun_end + 1,
                                               core::PartOfSpeech::Particle) != nullptr;
        if (!closed || lookupEntryInRange(dict_manager, codepoints, end_pos, noun_end) != nullptr) {
          continue;
        }
        const auto& readings = analysesInRange(inflection, codepoints, end_pos, noun_end);
        if (std::none_of(readings.begin(), readings.end(), [](const grammar::InflectionCandidate& reading) {
              return reading.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence;
            })) {
          return true;
        }
      }
      return false;
    };
    if (followed_by_case_particle && !followed_by_longer_nominal && !opens_unread_kana_noun()) {
      return true;
    }
  }

  // When the same dictionary span has both noun and adverb readings, a
  // following nominal particle selects the noun use (一切+の/は/を).  Keep
  // the adverb when it directly modifies a predicate (一切+確認しない).
  if (result.entry->pos == core::PartOfSpeech::Adverb && end_pos < codepoints.size()) {
    const bool has_same_span_noun =
        lookupResultsHavePartOfSpeech(lookup_results, partOfSpeechMask(core::PartOfSpeech::Noun), result.length);
    const auto following_results = dict_manager.lookup(text, byteOffsetAt(byte_offsets, end_pos));
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
      return true;
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
        const auto suffix_results = dict_manager.lookup(text, byteOffsetAt(byte_offsets, suffix_pos));
        has_interrogative_particle_split = lookupResultsHaveExtendedPOS(
            suffix_results, core::ExtendedPOS::ParticleAdverbial, result.length - prefix.length);
        if (has_interrogative_particle_split) {
          break;
        }
      }
      if (has_interrogative_particle_split) {
        return true;
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
        hasPrecedingExtendedPOS(lattice, start_pos,
                                {core::ExtendedPOS::ParticleCase, core::ExtendedPOS::ParticleTopic,
                                 core::ExtendedPOS::ParticleBinding, core::ExtendedPOS::ParticleAdverbial});
    const bool has_formal_noun_left_context =
        follows_genitive ||
        (hasPrecedingAttributivePredicate(lattice, start_pos) && !follows_non_genitive_nominal_particle);
    if (has_same_span_adverb && !has_formal_noun_left_context) {
      const auto following_results = dict_manager.lookup(text, byteOffsetAt(byte_offsets, end_pos));
      const bool followed_by_nominal_marker =
          std::any_of(following_results.begin(), following_results.end(), [](const auto& following) {
            if (following.entry == nullptr) {
              return false;
            }
            const auto extended_pos = following.entry->extended_pos;
            return isNominalForcingParticle(extended_pos) ||
                   (extended_pos == core::ExtendedPOS::AuxCopulaDa &&
                    !grammar::isSingleHiragana(following.entry->surface, core::hiragana::kNa)) ||
                   extended_pos == core::ExtendedPOS::AuxCopulaDesu;
          });
      if (!followed_by_nominal_marker) {
        return true;
      }
    }
  }

  if (result.entry->extended_pos == core::ExtendedPOS::AuxTenseMasu &&
      utf8::equalsAny(result.entry->surface, {"まし"}) && end_pos < codepoints.size() && codepoints[end_pos] == U'て' &&
      hasCoveringVerifiedVerbRenyokei(lattice, start_pos, end_pos)) {
    return true;
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
      const auto following_results = dict_manager.lookup(text, byteOffsetAt(byte_offsets, end_pos));
      has_nominal_right_context =
          lookupResultsHavePartOfSpeech(following_results, partOfSpeechMask(core::PartOfSpeech::Particle));
    }
    if (has_same_span_suffix && has_nominal_right_context) {
      return true;
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
    const auto following_results = dict_manager.lookup(text, following_byte_pos);
    const bool followed_by_passive = lookupResultsHaveExtendedPOS(following_results, core::ExtendedPOS::AuxPassive);
    if (followed_by_passive) {
      return true;
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
      return true;
    }
  }
  // The reverse side of the same lexical homograph contract: when a surface
  // is explicitly registered as both a noun and a na-adjective, a predicative
  // copula selects its nominal reading. Adjective-only entries remain
  // adjectives before the same copula.
  if (result.entry->extended_pos == core::ExtendedPOS::AdjNaAdj && end_pos < codepoints.size()) {
    const bool has_same_surface_noun =
        lookupResultsHavePartOfSpeech(lookup_results, partOfSpeechMask(core::PartOfSpeech::Noun), result.length);
    if (has_same_surface_noun && grammar::startsPredicativeCopula(following_text)) {
      return true;
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
      return true;
    }
  }
  return false;
}

}  // namespace suzume::analysis::tokenizer_dictionary_detail
