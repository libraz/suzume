/**
 * @file unknown_bracketed_noun_admission.cpp
 * @brief Admit and emit bracketed hiragana noun rescue candidates.
 */

#include <algorithm>
#include <cstddef>

#include "analysis/dictionary_probe.h"
#include "analysis/scorer_constants.h"
#include "analysis/unknown_bracketed_noun_internal.h"
#include "analysis/unknown_same_type_internal.h"
#include "candidate_constants.h"
#include "core/kana_constants.h"
#include "grammar/char_patterns.h"
#include "normalize/char_type.h"
#include "suffix_candidates.h"
#include "tokenizer_utils.h"
#include "verb_candidates.h"

namespace suzume {
namespace analysis {
namespace bracketed_noun_detail {

using same_type_detail::isRightBoundaryParticle;
using same_type_detail::opensOnParticleBeforePronoun;

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
  // A volitional stem is a cell only before its う, so elsewhere it is no
  // predicate to cut (私+も|とっさ is not もつ's もと).
  bool opens_inside_hosted_predicate = false;
  constexpr size_t kPredicateLookbehind = 3;
  for (size_t probe = start_pos > kPredicateLookbehind ? start_pos - kPredicateLookbehind : 0;
       probe < start_pos && !opens_inside_hosted_predicate && dict_manager != nullptr; ++probe) {
    if (probe == 0 || ctx.char_types[probe - 1] == normalize::CharType::Hiragana ||
        ctx.char_types[probe] != normalize::CharType::Hiragana) {
      continue;
    }
    for (size_t probe_end = start_pos + 1; probe_end <= scan; ++probe_end) {
      const auto* hosted = lookupEntryInRange(*dict_manager, codepoints, probe, probe_end, core::PartOfSpeech::Verb);
      const bool stranded_volitional_stem = hosted != nullptr &&
                                            hosted->extended_pos == core::ExtendedPOS::VerbMizenkei &&
                                            kana::isORowCodepoint(codepoints[probe_end - 1]) &&
                                            (probe_end >= codepoints.size() || codepoints[probe_end] != U'う');
      if (hosted != nullptr && !stranded_volitional_stem) {
        opens_inside_hosted_predicate = true;
        break;
      }
    }
  }
  // Nor may it carry the tail of a kana verb terminal that opens in front of
  // it on into the nominalizer (ある|く+の+が is あるく+の+が). The kana in front
  // must spell a registered verb, as a particle opens none (りんご+が+つの).
  bool carries_left_terminal_into_nominalizer = false;
  for (size_t opening = start_pos; opening > 0 && start_pos - opening < kPredicateLookbehind &&
                                   ctx.char_types[opening - 1] == normalize::CharType::Hiragana &&
                                   !carries_left_terminal_into_nominalizer && dict_manager != nullptr;
       --opening) {
    if (!hasExactPartOfSpeech(*dict_manager, codepoints, opening - 1, start_pos,
                              partOfSpeechMask(core::PartOfSpeech::Verb))) {
      continue;
    }
    for (size_t nominalizer = start_pos + 1; nominalizer < scan; ++nominalizer) {
      if (codepoints[nominalizer] == U'の' &&
          readsAsVerbTerminal(ctx.inflection, extractSubstring(codepoints, opening - 1, nominalizer))) {
        carries_left_terminal_into_nominalizer = true;
        break;
      }
    }
  }
  // Nor may it absorb a registered irrealis and the auxiliary that selects
  // it (あら+ん+や): that is a finished predicate, not a noun, when only
  // particles follow it (せ+ん+せい is the noun せんせい).
  bool opens_on_irrealis_chain = false;
  for (size_t stem_end = start_pos + 1; stem_end < scan && !opens_on_irrealis_chain && dict_manager != nullptr;
       ++stem_end) {
    const auto* irrealis = lookupEntryInRange(*dict_manager, codepoints, start_pos, stem_end, core::PartOfSpeech::Verb);
    const auto* auxiliary =
        lookupEntryInRange(*dict_manager, codepoints, stem_end, stem_end + 1, core::PartOfSpeech::Auxiliary);
    opens_on_irrealis_chain =
        irrealis != nullptr && auxiliary != nullptr && irrealis->extended_pos == core::ExtendedPOS::VerbMizenkei &&
        (auxiliary->extended_pos == core::ExtendedPOS::AuxNegativeNu ||
         auxiliary->extended_pos == core::ExtendedPOS::AuxVolitional) &&
        (stem_end + 1 == scan ||
         maximalSegmentCount(*dict_manager, codepoints, stem_end + 1, scan, core::PartOfSpeech::Particle) > 0);
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
          continuesAuxiliaryChain(*dict_manager, codepoints, auxiliary->extended_pos, aux_end, scan);
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
  // A run spelled wholly by two or more auxiliaries, each licensing the next
  // (い+です), is a predicate tail. The classical perfect is left out, as in the function-word chain
  // check: its one-mora cells are admitted only inside their own chain and
  // would otherwise decompose ordinary nouns (に+おい).
  const bool spells_auxiliary_chain =
      dict_manager != nullptr && promoted.len >= 3 &&
      maximalSegmentCount(*dict_manager, codepoints, start_pos, scan, core::PartOfSpeech::Auxiliary,
                          core::ExtendedPOS::AuxClassicalPerfect, !particleMoraLacksHost(codepoints, start_pos)) >= 2 &&
      continuesAuxiliaryChain(*dict_manager, codepoints, core::ExtendedPOS::Count_, start_pos, scan);
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
         !carries_left_terminal_into_nominalizer && !opens_on_irrealis_chain &&
         !promoted.has_inflected_predicate_reading && !opens_on_sino_prefix && !absorbs_trailing_suffix &&
         !opens_on_te_connective && !spans_laugh && !closes_registered_word_predicate && !finishes_auxiliary_chain &&
         !spells_contracted_hypothetical && !steals_formal_noun_head && !promoted.absorbs_copula_before_sokuon_final &&
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

namespace {
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

}  // namespace

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
  const bool selected_nominal = ((((promoted.right_particle && !promoted.right_particle_opens_copula) ||
                                   auxiliary_bracket_before_particle || promoted.right_suffix) &&
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
  // A run spelled as an i-adjective stem and the nominalizer さ (やさし+さ)
  // has a derivation that owns the span the way a lexical reading does, so
  // the rescue offers the noun but does not select it. The nominalizer み
  // derives a whole noun (たのしみ, かなしみ), which is the rescued run itself.
  const bool spells_adjective_nominalization = std::any_of(
      promoted.promoted_inflections->begin(), promoted.promoted_inflections->end(),
      [](const grammar::InflectionCandidate& inflection_candidate) {
        return inflection_candidate.verb_type == grammar::VerbType::IAdjective && inflection_candidate.suffix == "さ" &&
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
  // with a left bracket of its own, so on its own the preference is small —
  // enough to settle a run the fabricated-verb reading also covers (くつ|だっ|た,
  // where くつ is equally a godan dictionary form). A non-genitive particle on
  // the left closes the frame from both sides and weighs more (ねこ+が+たのしみ+だ).
  if (promoted.right_copula && !exact_reading_owns_context && !selected_nominal) {
    noun_cost += bracketed.left_particle_bracket && !bracketed.left_genitive_bracket ? scorer::scale::kStrongBonus
                                                                                     : scorer::scale::kMinorBonus;
  }
  // A manner mimetic before the quotative と is bracketed the same way but is
  // the adverb, not a noun (うっとり+と, どきり+と).
  const bool spells_mimetic_before_quotative = scan < codepoints.size() && codepoints[scan] == core::hiragana::kTo &&
                                               spellsRiMimeticShape(codepoints, start_pos, scan);
  auto noun_cand =
      makeCandidate(promoted.promoted_surface, start_pos, scan,
                    spells_mimetic_before_quotative ? core::PartOfSpeech::Adverb : core::PartOfSpeech::Noun, noun_cost,
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

}  // namespace bracketed_noun_detail
}  // namespace analysis
}  // namespace suzume
