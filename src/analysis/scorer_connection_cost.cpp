#include "analysis/bigram_table.h"
#include "analysis/category_cost.h"
#include "analysis/scorer.h"
#include "analysis/scorer_connection_rules.h"
#include "analysis/scorer_constants.h"
#include "analysis/verb_candidates_helpers.h"
#include "core/debug.h"
#include "core/kana_constants.h"
#include "core/types.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/honorific_verbs.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"

namespace cost = suzume::analysis::bigram_cost;
namespace sc = suzume::analysis::scorer;

// Surface-based adjustments use cost:: namespace directly from bigram_cost.
// See bigram_table.h and scorer_constants.h for constant values.

namespace suzume::analysis {

namespace {

float computeLateLexicalBoundaryBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  float bonus{};

  // AuxAspectIru requires a te-form; regional ておる / でおる contractions
  // are the productive direct-attachment exception.
  const bool is_dialectal_oru_contraction = grammar::isDialectalOruContractionLemma(next.lemma);
  const bool invalid_aspect_iru_attachment =
      next.extended_pos == core::ExtendedPOS::AuxAspectIru && !grammar::isContractedProgressiveSurface(next.surface) &&
      !is_dialectal_oru_contraction &&
      (prev.extended_pos == core::ExtendedPOS::ParticleConj || prev.extended_pos == core::ExtendedPOS::VerbRenyokei) &&
      !grammar::isTeDeSurface(prev.surface);
  // The directional aspect auxiliary いく likewise requires the connective
  // て/で.  A generic ParticleConj→AuxAspectIku bonus must not license an
  // unrelated connective such as し followed by the one-mora verb homograph
  // く inside an adjective.
  const bool invalid_aspect_iku_attachment = prev.extended_pos == core::ExtendedPOS::ParticleConj &&
                                             next.extended_pos == core::ExtendedPOS::AuxAspectIku &&
                                             !grammar::isTeDeSurface(prev.surface);
  // A one-mora potential auxiliary is a nonterminal stem.  Before punctuation
  // it is usually the tail of a complete lexical renyokei (踏まえ、), not an
  // independent auxiliary following a fabricated mizenkei (踏ま+え、).
  const bool incomplete_potential_before_symbol = prev.extended_pos == core::ExtendedPOS::AuxPotential &&
                                                  normalize::utf8Length(prev.surface) == 1 &&
                                                  next.extended_pos == core::ExtendedPOS::Symbol;
  // が after a terminal verb has two readings, and neither is open to a
  // fabricated predicate.  The case particle requires a nominal on its left, so
  // it is barred there outright and the adversative conjunctive particle takes
  // the position instead.  That connective in turn requires a clause, which an
  // unregistered verb candidate has not established: it is what a kana run
  // looks like when it is cut at a plausible terminal ending (ゆうがた as
  // ゆう+が+ただ).  A registered predicate keeps the connective reading free.
  const bool terminal_verb_before_ga =
      prev.extended_pos == core::ExtendedPOS::VerbShuushikei &&
      ((next.extended_pos == core::ExtendedPOS::ParticleCase && grammar::isSingleHiragana(next.surface, U'が')) ||
       (next.extended_pos == core::ExtendedPOS::ParticleConjFinite && !prev.fromDictionary()));
  // The assertive copula predicates over a nominal.  A conditional verb form or
  // a potential auxiliary directly before it is therefore an accidental
  // homograph chain.  Restrict this to the two nonterminal readings that can
  // fabricate い|え|だっ and いえ|だっ; terminal/modal predicates have valid
  // productive copular continuations.
  const bool nonterminal_predicate_before_assertive_copula =
      next.extended_pos == core::ExtendedPOS::AuxCopulaDa &&
      (prev.extended_pos == core::ExtendedPOS::VerbKateikei || prev.extended_pos == core::ExtendedPOS::AuxPotential);
  // Colloquial emphatic extension may add っ to an adverb at a clause edge, but
  // not immediately before the past auxiliary.  In that position the sokuon is
  // the copula's onbin (名詞+だっ+た), not part of the adverb.
  const bool emphatic_adverb_before_past = prev.pos == core::PartOfSpeech::Adverb &&
                                           utf8::endsWith(prev.surface, "っ") &&
                                           next.extended_pos == core::ExtendedPOS::AuxTenseTa;
  // The literary register hosts the conjectural on a nominal predicate whose
  // copula is elided (確認+らむ), but a single unregistered kanji is not that
  // nominal: it is what is left when a terminal verb is cut at its own final
  // mora and the mora reads as the volitional (言うとも as 言+う+とも).
  const bool volitional_after_stray_kanji = next.extended_pos == core::ExtendedPOS::AuxVolitional &&
                                            prev.pos == core::PartOfSpeech::Noun && !prev.fromDictionary() &&
                                            normalize::utf8Length(prev.surface) == 1;
  if (invalid_aspect_iru_attachment || invalid_aspect_iku_attachment || incomplete_potential_before_symbol ||
      terminal_verb_before_ga || nonterminal_predicate_before_assertive_copula || emphatic_adverb_before_past ||
      volitional_after_stray_kanji) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }
  if ((prev.extended_pos == core::ExtendedPOS::VerbRenyokei || prev.extended_pos == core::ExtendedPOS::VerbOnbinkei) &&
      next.extended_pos == core::ExtendedPOS::AuxAspectIru && is_dialectal_oru_contraction) {
    SUZUME_CONNECTION_ADD(bonus, cost::kExtremeBonus);
  }

  // Dictionary-backed i-adjectives form a reliable nominal predicate boundary.
  if (prev.pos == core::PartOfSpeech::Noun && next.extended_pos == core::ExtendedPOS::AdjBasic &&
      next.fromDictionary()) {
    SUZUME_CONNECTION_ADD(bonus, cost::kModerateBonus);
  }

  // Conjunctions cannot usually start from a bare token or an unknown
  // hiragana noun; after a conjunction, hiragana で is not 出る's renyokei.
  if (next.pos == core::PartOfSpeech::Conjunction && prev.pos != core::PartOfSpeech::Symbol &&
      prev.pos != core::PartOfSpeech::Particle && prev.pos != core::PartOfSpeech::Auxiliary &&
      normalize::utf8Length(prev.surface) == 1) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }
  const bool unknown_hiragana_conjunction_host = !prev.fromDictionary() &&
                                                 prev.origin != core::CandidateOrigin::BracketedNoun &&
                                                 grammar::isPureHiragana(prev.surface);
  // A conjunction that spells a te-form cannot be introduced by a bare noun:
  // with no boundary marker between them its opening mora is the continuative
  // that the noun's own predicate needs (確認+し+たがっ+て+いる, not
  // 確認+したがって+いる). A conjunction reached through a particle or a
  // punctuation mark is unaffected, which is where the clause-opening reading
  // actually occurs.
  const bool te_form_conjunction =
      grammar::isPureHiragana(next.surface) && utf8::endsWithAny(next.surface, {"て", "で"});
  // A conjunction opening with だ is barred from the same slot for the mirror
  // reason: directly after a noun that だ is the copula predicating it
  // (確認+だ+から+こそ), and only a clause boundary in front of the conjunction
  // makes the listed reading available.
  const bool copula_initial_conjunction = utf8::startsWith(next.surface, "だ");
  // Same reasoning at the other end: every listed と-final conjunction spells a
  // predicate plus the conditional と (すると, そうすると, さもないと), so
  // directly after a noun that と is the particle on the noun's own predicate
  // (しばらく+する+と, not しばらく+すると). The bar is deliberately shaped on the
  // conjunction rather than on the host being an unknown hiragana noun: a
  // coordinating conjunction joins two nominals with no clause boundary
  // anywhere, and barring it there shatters an all-hiragana coordination
  // (りんご+または+みかん).
  const bool conditional_to_conjunction = grammar::isConditionalToConjunction(next.surface);
  // The conditional と reaches one host class further than the other two: an
  // adverb in that slot modifies the very predicate the conjunction spells
  // (もしか+する+と), so it is no more a clause boundary than a bare noun is.
  const bool conjunction_host_slot =
      prev.pos == core::PartOfSpeech::Noun || (prev.pos == core::PartOfSpeech::Adverb && conditional_to_conjunction);
  // An auxiliary closes the predicate it sits on exactly as a noun heads the
  // phrase it sits in, so a だ-initial conjunction directly after one is the
  // copula predicating it and its own conjunctive particle (確認+す+べき+だ+が,
  // not 確認+す+べき+だが). The listed conjunction reading needs the clause
  // boundary that a punctuation mark or a particle supplies.
  const bool barred_conjunction_host =
      next.pos == core::PartOfSpeech::Conjunction &&
      ((conjunction_host_slot && (te_form_conjunction || copula_initial_conjunction || conditional_to_conjunction)) ||
       (prev.pos == core::PartOfSpeech::Auxiliary && copula_initial_conjunction));
  // A focus particle marks a phrase the analyzer has already identified. An
  // unknown hiragana noun is the fallback for material it could not, so the
  // pairing means the boundary landed inside a word rather than after one
  // (昔+の+まま+で, where のま+まで invents a host for まで).
  const bool barred_focus_particle_host = next.extended_pos == core::ExtendedPOS::ParticleAdverbial &&
                                          prev.pos == core::PartOfSpeech::Noun && unknown_hiragana_conjunction_host;
  // A formal noun whose canonical spelling is kana (こと, もの) is not one when
  // it appears as its single kanji directly after another kanji: there it is the
  // tail of that compound (果物, 仕事, 記事). The kana lemma behind the kanji
  // entry is what marks this class, so bound suffixes that really are written in
  // kanji (年度末) keep their boundary. Without the bar the formal noun claims
  // the compound's last kanji to collect the bonus its own followers carry.
  const bool kana_spelled_formal_noun = next.extended_pos == core::ExtendedPOS::NounFormal &&
                                        normalize::utf8Length(next.surface) == 1 &&
                                        normalize::isKanjiCodepoint(utf8::decodeFirstChar(next.surface)) &&
                                        next.lemma != next.surface && grammar::isPureHiragana(next.lemma);
  const bool barred_kanji_formal_noun = kana_spelled_formal_noun && prev.pos == core::PartOfSpeech::Noun &&
                                        !prev.surface.empty() &&
                                        normalize::isKanjiCodepoint(utf8::decodeLastChar(prev.surface));
  if (barred_conjunction_host || barred_focus_particle_host || barred_kanji_formal_noun) {
    SUZUME_CONNECTION_ADD(bonus, cost::kProhibitive);
  }
  const bool conjunction_before_hiragana_de = prev.pos == core::PartOfSpeech::Conjunction &&
                                              next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
                                              next.surface == "で";
  // A hiragana conjunction ending in a connective te-form cannot directly
  // govern existential いる.  Whatever the conjunction is listed as, that
  // ending is the productive te-form of the predicate inside it and いる is
  // the aspectual auxiliary the te-form selects (そう+し+て+いる,
  // 確認+し+たがっ+て+いる).
  const bool conjunction_shite_before_iru = prev.pos == core::PartOfSpeech::Conjunction &&
                                            grammar::isPureHiragana(prev.surface) &&
                                            utf8::endsWithAny(prev.surface, {"て", "で"}) &&
                                            ((next.pos == core::PartOfSpeech::Verb && next.lemma == "いる") ||
                                             next.extended_pos == core::ExtendedPOS::AuxAspectIru);
  // A closed conjunction/adverb ending in a te-form cannot directly govern
  // the directional subsidiary いく/くる. This would otherwise prefer the
  // lexical entries もって and かえって over the productive V-て + subsidiary
  // analysis. The gate is grammatical rather than surface-specific, so real
  // conjunctions before ordinary lexical verbs remain available.
  const bool closed_te_form_before_directional_aux =
      (prev.pos == core::PartOfSpeech::Conjunction || prev.pos == core::PartOfSpeech::Adverb) &&
      grammar::isPureHiragana(prev.surface) && utf8::endsWithAny(prev.surface, {"て", "で"}) &&
      utf8::equalsAny(next.surface, {"いく", "くる"});
  if (conjunction_before_hiragana_de || conjunction_shite_before_iru || closed_te_form_before_directional_aux) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // Dictionary compound adverbs and formal-noun constructions retain their
  // lexical boundary, unlike the corresponding accidental short-token splits.
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase && next.pos == core::PartOfSpeech::Adverb &&
      next.fromDictionary() && grammar::containsKanji(next.surface) &&
      next.surface.find("の") != std::string_view::npos) {
    SUZUME_CONNECTION_ADD(bonus, cost::kModerateBonus);
  }
  if (prev.extended_pos == core::ExtendedPOS::NounFormal && prev.fromDictionary() &&
      normalize::utf8Length(prev.surface) == 1 &&
      (next.pos == core::PartOfSpeech::Noun || next.pos == core::PartOfSpeech::Suffix) &&
      normalize::utf8Length(next.surface) == 1 && grammar::containsKanji(prev.surface) &&
      grammar::containsKanji(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrong);
  }
  // Binding particles productively follow the connective て/で (読んで+さえ,
  // 見て+こそ). Other ParticleConj homographs do not inherit the bonus.
  const bool te_de_before_binding = prev.extended_pos == core::ExtendedPOS::ParticleConj &&
                                    next.extended_pos == core::ExtendedPOS::ParticleBinding &&
                                    grammar::isTeDeSurface(prev.surface);
  const bool formal_noun_before_mizenkei = prev.extended_pos == core::ExtendedPOS::NounFormal &&
                                           next.extended_pos == core::ExtendedPOS::VerbMizenkei &&
                                           normalize::utf8Length(next.surface) >= 2;
  if (te_de_before_binding || formal_noun_before_mizenkei) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // Only quotative と can follow a sentence-final particle as a case phrase.
  if (prev.extended_pos == core::ExtendedPOS::ParticleFinal && next.extended_pos == core::ExtendedPOS::ParticleCase &&
      !grammar::isSingleHiragana(next.surface, core::hiragana::kTo)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // Keep the colloquial sa-row contract, duration-counter predicate, and
  // generated past-marked noun guards separate from ordinary lexical edges.
  const bool colloquial_sa_row_boundary = prev.extended_pos == core::ExtendedPOS::VerbMizenkei &&
                                          utf8::endsWith(prev.surface, "しゃ") &&
                                          next.extended_pos == core::ExtendedPOS::VerbRenyokei && next.lemma == "する";
  if (prev.extended_pos == core::ExtendedPOS::NounNumber && prev.origin == core::CandidateOrigin::Counter &&
      utf8::endsWith(prev.surface, "間") && next.pos == core::PartOfSpeech::Verb) {
    SUZUME_CONNECTION_ADD(bonus, cost::kModerateBonus);
  }
  const bool generated_nominal_before_particle =
      prev.pos == core::PartOfSpeech::Noun && prev.origin == core::CandidateOrigin::KanjiHiraganaNominalCompound &&
      next.pos == core::PartOfSpeech::Particle &&
      (next.extended_pos == core::ExtendedPOS::ParticleCase || next.extended_pos == core::ExtendedPOS::ParticleTopic ||
       next.extended_pos == core::ExtendedPOS::ParticleNo);
  if (colloquial_sa_row_boundary || generated_nominal_before_particle) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }
  if (next.pos == core::PartOfSpeech::Conjunction && prev.pos == core::PartOfSpeech::Noun &&
      prev.origin == core::CandidateOrigin::KanjiHiraganaCompound &&
      grammar::isPastMarkerTaDaSurface(utf8::lastChar(prev.surface))) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }
  return bonus;
}

}  // namespace

float Scorer::connectionCost(const core::LatticeEdge& prev, const core::LatticeEdge& next) const {
  // The colloquial contraction of the hypothetical has absorbed the conjunctive
  // particle into its inflection, so it attaches rightward the way that
  // particle does: what follows opens a main clause (やりゃ + できる). Charging
  // it the verb-to-verb boundary cost instead would keep the following
  // predicate from starting, and its ExtendedPOS row already inherits the
  // particle's continuations.
  const core::PartOfSpeech left_pos =
      prev.extended_pos == core::ExtendedPOS::VerbContractedKateikei ? core::PartOfSpeech::Particle : prev.pos;
  float base_cost = bigramCost(left_pos, next.pos);

  // ExtendedPOS bigram cost (replaces all check functions)
  float extended_cost = BigramTable::getCost(prev.extended_pos, next.extended_pos);

  // This pair adds to its existing static BigramTable bonus, so it cannot be
  // represented as a replacement table entry without changing the total.
  const bool renyokei_before_excessive =
      prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.extended_pos == core::ExtendedPOS::AuxExcessive;
  const bool na_adjective_before_adverbial_ni = prev.extended_pos == core::ExtendedPOS::AdjNaAdj &&
                                                next.extended_pos == core::ExtendedPOS::ParticleCase &&
                                                grammar::isSingleHiragana(next.surface, U'に');
  float surface_bonus =
      renyokei_before_excessive || na_adjective_before_adverbial_ni ? cost::kVeryStrongBonus : cost::kNeutral;

  SUZUME_CONNECTION_ADD(surface_bonus, connection_rules::computeVerbRenyokeiEarlyBonus(prev, next));

  SUZUME_CONNECTION_ADD(surface_bonus,
                        sc::compoundVerbSplitBonus(prev.extended_pos, prev.surface, next.extended_pos, next.surface));

  SUZUME_CONNECTION_ADD(surface_bonus, connection_rules::computePassiveCausativeBonus(prev, next));

  SUZUME_CONNECTION_ADD(surface_bonus, connection_rules::computeTaFormVolitionalBonus(prev, next));

  SUZUME_CONNECTION_ADD(surface_bonus, connection_rules::computeNegativeAndNounVerbBonus(prev, next));

  SUZUME_CONNECTION_ADD(surface_bonus, connection_rules::computeParticleDeterminerBonus(prev, next));

  SUZUME_CONNECTION_ADD(surface_bonus, connection_rules::computePrefixSymbolBonus(prev, next));
  // What a multi-mora particle may govern: both rules key on the predicate
  // form the particle was lexicalized from, and their contributions are
  // additive, so they share one accumulation.
  SUZUME_CONNECTION_ADD(surface_bonus, connection_rules::computeCompoundParticlePoliteBonus(prev, next) +
                                           connection_rules::computeConjunctiveParticleCopulaPenalty(prev, next) +
                                           connection_rules::computeAdverbialNiAfterPredicatePenalty(prev, next));

  // Note: Removed penalty for Pronoun + でも patterns
  // MeCab behavior is context-dependent:
  // - "何でもいい" → keeps でも together (副助詞)
  // - "何でもあり" (standalone) → keeps でも together
  // - "何でもありだな" → splits で+も
  // This context-sensitivity can't be captured in bigram scorer.
  // Let other scoring mechanisms handle the distinction.

  SUZUME_CONNECTION_ADD(surface_bonus, connection_rules::computeSuffixShortVerbBonus(prev, next));

  SUZUME_CONNECTION_ADD(surface_bonus, connection_rules::computeParticleQuoteBonus(prev, next));

  SUZUME_CONNECTION_ADD(surface_bonus, connection_rules::computeCompoundNominalizationBonus(prev, next));

  SUZUME_CONNECTION_ADD(surface_bonus, connection_rules::computeProgressiveHonorificBonus(prev, next));

  SUZUME_CONNECTION_ADD(surface_bonus, connection_rules::computeSugiFinalParticleBonus(prev, next));
  SUZUME_CONNECTION_ADD(surface_bonus, connection_rules::computeCopulaConditionalBonus(prev, next));
  SUZUME_CONNECTION_ADD(surface_bonus, connection_rules::computePastConditionalVerbBonus(prev, next));
  SUZUME_CONNECTION_ADD(surface_bonus, connection_rules::computeExistentialAruNominalPredicateBonus(prev, next));
  SUZUME_CONNECTION_ADD(surface_bonus, connection_rules::computeCompletionAuxiliaryBonus(prev, next));
  SUZUME_CONNECTION_ADD(surface_bonus, connection_rules::computeAdjectiveTePredicatePenalty(prev, next));
  SUZUME_CONNECTION_ADD(surface_bonus, connection_rules::computeClassicalNegativeBoundaryPenalty(prev, next) +
                                           connection_rules::computeAdjectiveDerivationHostPenalty(prev, next));

  SUZUME_CONNECTION_ADD(surface_bonus, connection_rules::computeBarePotentialRenyokeiPenalty(prev, next));

  SUZUME_CONNECTION_ADD(surface_bonus, computeLateLexicalBoundaryBonus(prev, next));

  // A past auxiliary cannot normally be followed directly by the conjunctive
  // particle で. This otherwise lets た+で+す outrank the closed-class polite
  // copula た+です because the general past→conjunction bonus is intentionally
  // strong for forms such as たものの.
  bool invalid_closed_conjunctive = prev.extended_pos == core::ExtendedPOS::AuxTenseTa &&
                                    next.extended_pos == core::ExtendedPOS::ParticleConj &&
                                    utf8::equalsAny(next.surface, {"で"});

  // The polite auxiliary has only two productive conjunctive continuations:
  // まし+て and the literary conditional ますれ+ば.  The POS-level bigram is
  // deliberately strong for those forms, so reject homographic particles in
  // every other surface pairing (ございます+でしょ must not become
  // ございます+で+しょう).
  if (prev.extended_pos == core::ExtendedPOS::AuxTenseMasu && next.extended_pos == core::ExtendedPOS::ParticleConj) {
    const bool polite_te = utf8::equalsAny(prev.surface, {"まし"}) && utf8::equalsAny(next.surface, {"て"});
    const bool polite_conditional = utf8::equalsAny(prev.surface, {"ますれ"}) && utf8::equalsAny(next.surface, {"ば"});
    invalid_closed_conjunctive = invalid_closed_conjunctive || (!polite_te && !polite_conditional);
  }
  if (invalid_closed_conjunctive) {
    SUZUME_CONNECTION_ADD(surface_bonus, sc::kPenaltyClosedClassBoundary);
  }

  // The continuative stem い of the aspect auxiliary cannot introduce a
  // quotative determiner; the terminal いる does so. This rejects a fabricated
  // 〜な+い(いる)+という chain while retaining complete progressive predicates.
  if (prev.extended_pos == core::ExtendedPOS::AuxAspectIru && utf8::equalsAny(prev.surface, {"い"}) &&
      next.extended_pos == core::ExtendedPOS::DeterminerQuotative) {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kAlmostNever);
  }

  // The conditional past forms たら/だら followed by し are normally the
  // conjecture auxiliary らしい (読んだ+らしい), not a conditional boundary
  // followed by a connective particle.
  if (prev.extended_pos == core::ExtendedPOS::AuxTenseTa && utf8::endsWith(prev.surface, "ら") &&
      next.extended_pos == core::ExtendedPOS::ParticleConj && grammar::isConjunctiveParticleShi(next.surface)) {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kStrong);
  }

  // The dictionary conjunction まして is not a continuation of a verb or
  // deverbal noun. In polite inflection (読み+まし+て, 書かれ+まし+て), its
  // low lexical cost can otherwise beat the valid auxiliary chain before the
  // following て is considered. Nominal coordination remains governed by the
  // ordinary Noun→Conjunction rule for actual coordinating conjunctions.
  if (next.extended_pos == core::ExtendedPOS::Conjunction && next.surface == "まして" &&
      (prev.extended_pos == core::ExtendedPOS::VerbRenyokei || prev.pos == core::PartOfSpeech::Noun)) {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kAlmostNever);
  }

  // Note: "かも" is kept as single token per SuzumeUtils.pm normalization
  // (か+も → かも merge rule). No penalty for AUX → かも.

  // Penalty for ParticleFinal(か) → ADV(もし) in かもしれない pattern
  // "もし" is a valid adverb, but not in "かもしれない" context
  // This prevents か+もし+れ split, favoring か+も+しれ
  if (prev.extended_pos == core::ExtendedPOS::ParticleFinal && prev.surface == "か" &&
      next.pos == core::PartOfSpeech::Adverb && next.surface == "もし") {
    SUZUME_CONNECTION_ADD(surface_bonus, sc::kPenaltyGeneratedInternalBoundary);
  }

  // Penalty for a fabricated short hiragana irrealis before ん. Two morae of
  // kana are too little to establish an irrealis on their own, and a name plus
  // an honorific spells the same run (が+おさ+ん is がお+さん). What settles it
  // is whether the cell is spelled by a dictionary entry: the irrealis of a
  // real verb is (せ+ん, いか+ん, あら+ん), while the misreading only ever comes
  // out of the hiragana candidate generator. Kanji stems are unaffected either
  // way (押さ+ん). ん can be AUX_否定古 or PART_準体, both are penalized.
  if (prev.extended_pos == core::ExtendedPOS::VerbMizenkei && !prev.fromDictionary() &&
      grammar::isPureHiragana(prev.surface) && prev.surface.size() <= 6 &&  // 2 chars or less (6 bytes in UTF-8)
      next.surface == "ん") {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kAlmostNever);
  }

  // Penalty for short pure-hiragana dict verb + ず (classical negative)
  // E.g., おか+ず should be おかず (noun), not おか (dict verb おく) + ず (aux)
  // Short pure-hiragana verbs + ず are likely false parses of nouns/adverbs
  // Long verbs (かかわら+ず) and kanji verbs (表さ+ず) are productive grammar
  // Lexicalized forms like 思わず have their own dict entries (ADV) that win anyway
  // Only the ず cells are named, rather than every other cell being excluded: the
  // noun collision this rule exists for is theirs alone, and an exclusion list
  // silently captures each cell added to the paradigm afterwards (連用形 ざり).
  if (prev.pos == core::PartOfSpeech::Verb && prev.fromDictionary() && grammar::isPureHiragana(prev.surface) &&
      prev.surface.size() <= 9 &&  // ≤3 hiragana chars (9 bytes)
      next.extended_pos == core::ExtendedPOS::AuxNegativeNu &&
      !(prev.extended_pos == core::ExtendedPOS::VerbMizenkei && prev.lemma == "する") && prev.lemma != "ある" &&
      prev.lemma != "なる" && utf8::equalsAny(next.surface, {"ず", "ずに"})) {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kAlmostNever);
  }

  // Penalty for single-kanji noun + hiragana verb renyokei/onbinkei
  // E.g., 勘+違い should be 勘違い (compound noun), not 勘 (noun) + 違い (dict verb)
  // Single-kanji nouns rarely form valid noun+verb compounds with hiragana verbs
  // Exception: し (suru renyokei) is valid for サ変 pattern (得+し, 得する), but only
  //            for a registered host. A fabricated single-kanji noun has no verbal-noun
  //            reading to license it, so 押しボタン must keep its continuative 押し.
  // Exception: Katakana verbs (バズっ, ググっ) are valid after nouns (超バズった)
  // Exception: Kanji-initial verbs (本+買っ) are valid noun+verb (dropped を)
  // Exception: NounNumber quantity tokens (半 split off a duration-counter run)
  //            legitimately precede verbs directly (三時間|半|かかった)
  if (prev.pos == core::PartOfSpeech::Noun && prev.extended_pos != core::ExtendedPOS::NounNumber &&
      normalize::utf8Length(prev.surface) == 1 &&  // Single char
      next.pos == core::PartOfSpeech::Verb &&
      (next.extended_pos == core::ExtendedPOS::VerbRenyokei || next.extended_pos == core::ExtendedPOS::VerbOnbinkei) &&
      !(grammar::isSuruRenyokeiSurface(next.surface) && prev.fromDictionary()) &&   // サ変動詞パターン
      !kana::isKatakanaCodepoint(utf8::decodeFirstChar(next.surface)) &&            // Exclude katakana verbs
      !suzume::normalize::isKanjiCodepoint(utf8::decodeFirstChar(next.surface))) {  // Exclude kanji verbs
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kVeryRare);
  }

  // An unverified one-kanji fragment inside a kanji run is not a nominal host
  // for a dictionary terminal verb. Keep the compound candidate intact
  // (提+出す, 外+出す) while preserving genuine particleless noun predicates,
  // whose nominal head is dictionary-attested.
  if (prev.pos == core::PartOfSpeech::Noun && !prev.fromDictionary() && normalize::utf8Length(prev.surface) == 1 &&
      next.pos == core::PartOfSpeech::Verb && next.fromDictionary() &&
      next.extended_pos == core::ExtendedPOS::VerbShuushikei && normalize::utf8Length(next.surface) == 2 &&
      suzume::normalize::isKanjiCodepoint(utf8::decodeFirstChar(next.surface))) {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kVeryRare);
  }

  // Penalty for single-kanji NOUN → verbal auxiliary patterns
  // E.g., 合+う should be 合う (verb), not 合 (noun) + う (volitional)
  // E.g., 揺+れる should be 揺れる (verb), not 揺 (noun) + れる (passive)
  // Single-kanji nouns rarely take verbal auxiliaries directly
  // Exception: Single-kanji ichidan verb stems + causative させ (見+させる, etc.)
  if (prev.extended_pos == core::ExtendedPOS::Noun && normalize::utf8Length(prev.surface) == 1 &&
      (next.extended_pos == core::ExtendedPOS::AuxVolitional || next.extended_pos == core::ExtendedPOS::AuxPassive ||
       next.extended_pos == core::ExtendedPOS::AuxPotential || next.extended_pos == core::ExtendedPOS::AuxCausative ||
       next.extended_pos == core::ExtendedPOS::AuxClassicalBeshi)) {
    // Check if this is single-kanji ichidan + causative させ (should be allowed)
    bool is_ichidan_causative = false;
    if (next.extended_pos == core::ExtendedPOS::AuxCausative && utf8::startsWith(next.surface, "させ")) {
      is_ichidan_causative = verb_helpers::isSingleKanjiIchidanSurface(prev.surface);
    }
    if (!is_ichidan_causative) {
      SUZUME_CONNECTION_ADD(surface_bonus, cost::kSevere);
    }
  }

  // Penalty for PARTICLE → もあり/もありだ verb candidates
  // "もあり" is mis-recognized as godan verb (もある) or suru verb (もありする)
  // In "何でもありだな", "もありだ" should be も+あり+だ, not もありする+た
  // This pattern only appears after particles (で in でもあり)
  if (prev.pos == core::PartOfSpeech::Particle && next.pos == core::PartOfSpeech::Verb &&
      utf8::equalsAny(next.surface, {"もあり", "もありだ", "もある", "もあっ"})) {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kAlmostNever);
  }

  // Bonus for PART_格 → い (VerbRenyokei of いる)
  // E.g., 家にいた → 家+に+い+た (not 家+にいた)
  // "にいた" is mis-recognized as godan verb (にく) past tense
  // い is the renyokei of いる (to exist/be), very common after particles
  // Exclude と→い because という is a common determiner that should stay as one token
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase && next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.surface == "い" && prev.surface != "と") {  // Exclude と to protect という determiner
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kStrongBonus);
  }

  // Penalty for と → いう pattern to protect という determiner
  // E.g., という名前 → という+名前 (not と+いう+名前)
  // という is a common quotative determiner that should stay as one token
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase && prev.surface == "と" &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei && (next.surface == "いう" || next.surface == "いっ")) {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kUncommon);
  }

  // Bonus for VerbRenyokei → し (サ変 する renyokei)
  // E.g., お願いします → お+願い+し+ます (not お+願いし+ます)
  // "願いし" is mis-recognized as godan-sa verb (願いす)
  // This pattern is common for サ変複合動詞: 願い+する, 案内+する
  // Exclude single-char "い" which is いる renyokei (interferes with 上手い+し)
  // Exclude "で" which is でる renyokei (interferes with んでした → ん+でし+た)
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      grammar::isSuruRenyokeiSurface(next.surface) && prev.fromDictionary() && prev.surface != "い" &&
      prev.surface != "で") {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kStrongBonus);
  }

  // Penalty for kanji+sokuon+kanji NOUN → し(VerbRenyokei) pattern
  // E.g., 引っ越+し should be 引っ越し (compound verb), not NOUN + suru renyokei
  // These patterns are usually compound verbs registered in dictionary
  // The pattern: 漢字+っ+漢字 (kanji + sokuon + kanji) as NOUN → し(する連用形)
  if (prev.pos == core::PartOfSpeech::Noun && !prev.fromDictionary() &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei && grammar::isSuruRenyokeiSurface(next.surface) &&
      prev.surface.size() >= 9 &&  // At least 3 chars (2 kanji + っ)
      utf8::contains(prev.surface, "っ")) {
    // Check if it's kanji+っ+kanji pattern
    bool has_sokuon_between_kanji = false;
    auto codepoints = normalize::toCodepoints(prev.surface);
    for (size_t idx = 1; idx + 1 < codepoints.size(); ++idx) {
      if (codepoints[idx] == U'っ' && suzume::normalize::isKanjiCodepoint(codepoints[idx - 1]) &&
          suzume::normalize::isKanjiCodepoint(codepoints[idx + 1])) {
        has_sokuon_between_kanji = true;
        break;
      }
    }
    if (has_sokuon_between_kanji) {
      SUZUME_CONNECTION_ADD(surface_bonus, cost::kVeryRare);
    }
  }

  // Penalty for pure-hiragana dict NOUN → し(VerbRenyokei) pattern
  // E.g., はな+し should be はなし (verb), not はな(NOUN) + し(する連用形)
  // はな is a dict NOUN but not a suru-noun, so はな+し is not a valid suru compound
  // This does not affect kanji nouns (勉強+し is valid suru compound)
  // Use small penalty (0.08) to tip balance: はなし gap=0.013, なんし gap=0.102
  if (prev.pos == core::PartOfSpeech::Noun && prev.fromDictionary() &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei && grammar::isSuruRenyokeiSurface(next.surface) &&
      grammar::isPureHiragana(prev.surface)) {
    SUZUME_CONNECTION_ADD(surface_bonus, sc::kPenaltyHiraganaNounToSuruTip);
  }

  // Bonus for multi-kanji NOUN → せ(VerbMizenkei) sahen pattern
  // 認識+せ, 期待+せ: favors split over merged 認識せ/期待せ verb candidate
  // Only for 2+ kanji nouns (sahen-compatible), not single-kanji like 下+さ
  if (prev.pos == core::PartOfSpeech::Noun && next.extended_pos == core::ExtendedPOS::VerbMizenkei &&
      next.surface == "せ" && prev.surface.size() >= 6) {  // 2+ kanji = 6+ bytes in UTF-8
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kVeryStrongBonus);
  }

  // Penalty for PART_準体(の) → で to prevent ので splitting
  // High penalty keeps ので merged in conjunctive use (寒いので出かけた)
  // For のではない pattern, the downstream で→は surface bonus overcomes this
  if (prev.extended_pos == core::ExtendedPOS::ParticleNo && prev.surface == "の" && next.surface == "で") {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kVeryRare);  // 1.8
  }

  // The attributive copula な takes a noun or nominalizer. It cannot directly
  // precede a case particle, nor can it precede an arbitrary conjunction: only
  // the nominalizer-led なので/なのに forms are valid. This keeps a noun whose
  // last mora is な intact before a case particle (ひらがな+を), and keeps a
  // closed particle such as など from being split into な+ど while preserving
  // productive copular constructions.
  if (prev.extended_pos == core::ExtendedPOS::AuxCopulaDa && grammar::isAttributiveCopulaNa(prev.surface) &&
      (next.extended_pos == core::ExtendedPOS::ParticleCase ||
       (next.extended_pos == core::ExtendedPOS::ParticleConj && !utf8::startsWith(next.surface, "の")))) {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kVeryRare);  // Cancel the -0.8 bonus and add penalty
  }

  // Penalty for AuxCopulaDa(で) → し pattern (VerbRenyokei or ParticleConj)
  // 本でした should be 本+でし+た, not 本+で+し+た
  // で as copula te-form followed by し is grammatically unusual
  // し can be recognized as VerbRenyokei (suru) or PARTICLE_接続 (parallel particle)
  // Neither is correct in this context - the でし is the renyokei of です copula
  // This ensures でし (AuxCopulaDesu renyokei) wins over で+し split
  if (prev.extended_pos == core::ExtendedPOS::AuxCopulaDa && prev.surface == "で" &&
      (next.extended_pos == core::ExtendedPOS::VerbRenyokei || next.extended_pos == core::ExtendedPOS::ParticleConj)) {
    const bool is_suru_or_conjunctive_shi =
        (next.extended_pos == core::ExtendedPOS::VerbRenyokei && grammar::isSuruRenyokeiSurface(next.surface)) ||
        (next.extended_pos == core::ExtendedPOS::ParticleConj && grammar::isConjunctiveParticleShi(next.surface));
    if (is_suru_or_conjunctive_shi) {
      SUZUME_CONNECTION_ADD(surface_bonus, cost::kAlmostNever);
    }
  }

  // Penalty for PART_接続(し) → て pattern (PART_接続 or AUX_継続)
  // E.g., をなくして should be を+なくし+て, not を+なく+し+て
  // "し" as conjunctive particle (reason-listing) rarely followed by "て" directly
  // This prevents adjective renyokei (なく) + し (particle) + て from winning
  // over verb renyokei (なくし) + て pattern
  // Note: "て" can be either ParticleConj or AuxAspectIru (ている/てる aspect)
  if (prev.extended_pos == core::ExtendedPOS::ParticleConj && grammar::isConjunctiveParticleShi(prev.surface) &&
      next.surface == "て" &&
      (next.extended_pos == core::ExtendedPOS::ParticleConj || next.extended_pos == core::ExtendedPOS::AuxAspectIru)) {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kVeryRare);
  }

  // Penalty for short hiragana VERB_連用 → し/き (single-char verb renyokei)
  // E.g., おかしを → おかし+を, not おか+し+を
  // Short verb renyokei (2-3 chars) followed by し or き often indicates
  // over-segmentation of a noun or longer verb
  // Exclude ば (valid conditional: よれ+ば), て (te-form), etc.
  const bool next_is_shi =
      (next.extended_pos == core::ExtendedPOS::VerbRenyokei && grammar::isSuruRenyokeiSurface(next.surface)) ||
      (next.extended_pos == core::ExtendedPOS::ParticleConj && grammar::isConjunctiveParticleShi(next.surface));
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && prev.surface.size() >= 6 && prev.surface.size() <= 9 &&
      (next_is_shi || next.surface == "き")) {  // 2-3 hiragana
    // Check prev is all hiragana
    if (grammar::isPureHiragana(prev.surface) && (next.extended_pos == core::ExtendedPOS::VerbRenyokei ||
                                                  next.extended_pos == core::ExtendedPOS::ParticleConj)) {
      SUZUME_CONNECTION_ADD(surface_bonus, cost::kStrong);
    }
  }

  // Penalty for negation PREFIX (非/不/無/未) → single-kanji NOUN
  // E.g., 非常 → 非|常 is wrong (非常 is a single word, not prefix+noun)
  // E.g., 不可能 → 不|可能 is wrong (不可能 is a single word)
  // But お|茶, ご|報告 are valid (honorific prefix + noun)
  // Only penalize negation prefixes followed by single kanji
  if (prev.extended_pos == core::ExtendedPOS::Prefix && sc::isNegationPrefix(prev.surface) &&
      next.extended_pos == core::ExtendedPOS::Noun && normalize::utf8Length(next.surface) == 1) {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kAlmostNever);
  }

  // Penalty for AuxCopulaDa(で) → Symbol/EOS pattern
  // E.g., あとで。 should be NOUN+PART_格+。, not NOUN+AUX_断定+。
  // Copula 「で」 at sentence end is unusual; 格助詞「で」 is more natural
  // Note: 「だ」+Symbol is valid (学生だ。), so only penalize 「で」
  if (prev.extended_pos == core::ExtendedPOS::AuxCopulaDa && prev.surface == "で" &&
      next.extended_pos == core::ExtendedPOS::Symbol) {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kStrong);
  }

  // Penalty for NOUN → で(AUX_断定): copula で after noun is uncommon
  // Most NOUN+copula uses だ directly; で is mainly in で+ある/で+は/で+も
  // This counteracts the Noun→AuxCopulaDa bigram bonus (-0.5) for で
  // E.g., あとで, 爆速で, きっかけで, 電車で → NOUN+PART_格 preferred
  // Note: NOUN→だ(AUX_断定) is NOT affected (学生だ is correct)
  if ((prev.pos == core::PartOfSpeech::Noun || prev.pos == core::PartOfSpeech::Pronoun) &&
      next.extended_pos == core::ExtendedPOS::AuxCopulaDa && next.surface == "で") {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kRare);  // 1.0: must exceed kModerateBonus (-0.5)
  }

  // Bonus for で(AuxCopulaDa) → ある/あっ/あろ (formal copula pattern)
  // のである, ではある, であった are standard literary/formal expressions
  // AuxCopulaDa→VerbShuushikei has kMinor (0.5) bigram + kVeryRare (1.8) の→で surface
  // Total penalty to overcome: ~2.3
  if (prev.extended_pos == core::ExtendedPOS::AuxCopulaDa && prev.surface == "で" &&
      next.pos == core::PartOfSpeech::Verb && utf8::equalsAny(next.surface, {"ある", "あっ", "あろ", "あり"})) {
    SUZUME_CONNECTION_ADD(surface_bonus, sc::kBonusDoubleVeryStrong);  // -3.2 to overcome ~2.3 total penalty
  }

  // Bonus for で(AuxCopulaDa) → は(ParticleTopic) surface connection
  // Helps のではない/のではなく pattern split の+で+は despite の→で penalty (1.8)
  // Only fires when で is split from の (not when ので stays as single token)
  // Safe: で(AuxCopulaDa)+は is always correct when it occurs (ではない, ではなく)
  if (prev.extended_pos == core::ExtendedPOS::AuxCopulaDa && prev.surface == "で" &&
      next.extended_pos == core::ExtendedPOS::ParticleTopic && next.surface == "は") {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kVeryStrongBonus);  // -1.6
  }

  // Penalty for で(VERB_連用 of 出る) → ない(AUX_否定): copula でない is more common
  // でない = "is not" (copula) vs "doesn't come out" (verb 出る)
  // Without context (を/から before で), copula interpretation should win
  // E.g., 正式でない, 必要でない → で(AUX_断定) + ない(AUX_否定)
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && prev.surface == "で" &&
      next.extended_pos == core::ExtendedPOS::AuxNegativeNai) {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kStrong);
  }

  // Bonus for ParticleTopic → なかろ (AuxNegativeNai volitional stem)
  // Helps は+なかろ+う split beat fake godan-ra verb candidate なかろう
  // Only targets なかろ — other forms (ない, なかっ, なけれ) can be ADJ or AUX
  if (prev.extended_pos == core::ExtendedPOS::ParticleTopic && next.extended_pos == core::ExtendedPOS::AuxNegativeNai &&
      next.surface == "なかろ") {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kModerateBonus);
  }

  // Penalty for NOUN/PRON → で(VERB_連用 of 出る): verb で after noun is rare
  // Most NOUN+で patterns use particle or copula, not verb 出る
  // E.g., あとで, 爆速で → NOUN+PART_格, not NOUN+VERB(出る)
  if ((prev.pos == core::PartOfSpeech::Noun || prev.pos == core::PartOfSpeech::Pronoun) &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei && next.surface == "で") {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kRare);  // 1.0: must exceed NOUN→VERB_連用 bonus
  }

  // Penalty for VerbRenyokei → で as a conjunctive particle.
  // Ichidan te-form only uses て (食べ+て, 見+て), NOT で
  // Godan te-form with で uses onbinkei (飲ん+で, 読ん+で), not renyokei
  // The parallel copula rule is category-wide in bigram_table_verb_adj.cpp.
  // Without this, kanji+り nouns like 夏祭り get falsely parsed as VERB_連用
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && utf8::startsWith(next.surface, "で") &&
      next.extended_pos == core::ExtendedPOS::ParticleConj) {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kMinor);  // +0.5 to cancel the -0.5 bigram bonus
  }

  // Penalty for single-kanji ADJ_語幹 → AuxGaru
  // E.g., 挙+がっ should be 挙がっ (verb onbin), not 挙(adj stem)+がっ(がる suffix)
  // Multi-char adj stems + がる are valid (可愛+がる, 怖+がる via dict)
  // But single-kanji adj stems are usually false positives from the candidate generator
  if (prev.extended_pos == core::ExtendedPOS::AdjStem && suzume::normalize::utf8Length(prev.surface) == 1 &&
      next.extended_pos == core::ExtendedPOS::AuxGaru) {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kStrong);
  }

  // Penalty for single-hiragana VERB_連用 → を(PART_格)
  // E.g., おかしを → おかし+を, not おか+し+を
  // Single hiragana verb renyokei (し, き, み, etc.) rarely takes を directly
  // Nominalized verb renyokei like 読み, 書き take を (読みを深める) but those
  // are multi-char and should be recognized as NOUN, not single-char VERB_連用
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && normalize::utf8Length(prev.surface) == 1 &&
      next.extended_pos == core::ExtendedPOS::ParticleCase && next.surface == "を") {
    // Check if single char is hiragana
    auto decoded = normalize::utf8::decode(prev.surface);
    auto it = decoded.begin();
    if (it != decoded.end() && kana::isHiraganaCodepoint(*it)) {
      SUZUME_CONNECTION_ADD(surface_bonus, cost::kStrong);
    }
  }

  // Penalty for で(VerbRenyokei of 出る) → Particle (except て)
  // で as 出る renyokei should only be followed by auxiliaries (たい, ます) or て
  // 彼女でも → 彼女+で(PART)+も, not 彼女+で(VERB 出る)+も
  // But でて → で+て is valid (出る renyokei + te-form)
  // The verb interpretation is only valid before auxiliaries like たい/ます or て
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && prev.surface == "で" &&
      next.pos == core::PartOfSpeech::Particle && next.surface != "て") {  // Exclude て to allow でて → で+て split
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kStrong);
  }

  // Bonus for dictionary NOUN → dictionary NOUN connection
  // E.g., 明日+雨, 毎日+電車 should beat 明日雨, 毎日電車 (kanji_seq)
  // When both nouns are in dictionary, the split path is more accurate
  // This helps time nouns (明日, 今日, 毎日) + common nouns (雨, 電車)
  if (prev.pos == core::PartOfSpeech::Noun && prev.fromDictionary() && next.pos == core::PartOfSpeech::Noun &&
      next.fromDictionary() && !prev.isFormalNoun() && !next.isFormalNoun()) {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kModerateBonus);  // Prefer dict+dict split over kanji_seq
  }

  // Penalty for identical hiragana NOUN → NOUN sequence
  // E.g., もも|もも is less likely than もも|も|もも (particle between)
  // This prevents すもももも... from being split as もも|もも|もの
  if (prev.pos == core::PartOfSpeech::Noun && next.pos == core::PartOfSpeech::Noun && prev.surface == next.surface &&
      grammar::isPureHiragana(prev.surface)) {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kVeryRare);
  }

  // Bonus for dictionary hiragana NOUN → single-char particle も/の pattern
  // E.g., すもも|も|もも should beat すもも|もも (particle interpretation)
  // E.g., もも|の|うち should beat もの|うち (particle interpretation)
  // This helps famous test sentence: すもももももももものうち
  if (prev.pos == core::PartOfSpeech::Noun && prev.fromDictionary() && grammar::isPureHiragana(prev.surface) &&
      next.pos == core::PartOfSpeech::Particle && (next.surface == "も" || next.surface == "の")) {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kModerateBonus);
  }

  // Penalty for NOUN/PRON → pure-hiragana VERB_た形 (non-dict) pattern
  // E.g., 家にいた should be 家+に+い+た, not 家+にいた
  // E.g., ここにいた should be ここ+に+い+た, not ここ+にいた
  // "にいた" is mis-recognized as godan verb (にく) past tense
  // Pure-hiragana た-form verbs after NOUN/PRON are typically particle+aux sequences
  // Valid kanji+hiragana た-forms like 食べた are not affected (not pure hiragana)
  if ((prev.pos == core::PartOfSpeech::Noun || prev.pos == core::PartOfSpeech::Pronoun) &&
      next.extended_pos == core::ExtendedPOS::VerbTaForm && !next.fromDictionary() &&
      grammar::isPureHiragana(next.surface) &&
      next.surface.size() <= core::kFourJapaneseCharBytes) {  // 4 chars or less (12 bytes in UTF-8)
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kVeryRare);
  }

  // Bonus for か (particle) → dictionary verb mizenkei
  // In quotative patterns like かどうか分からない, か is followed by verb directly
  // Override the PART_終→VERB_未然 penalty for dictionary verbs
  if (prev.surface == "か" && prev.extended_pos == core::ExtendedPOS::ParticleFinal &&
      next.extended_pos == core::ExtendedPOS::VerbMizenkei && next.fromDictionary()) {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kStrongBonus);  // -0.8 to reduce the 1.8 penalty
  }

  // Penalty for single-kanji NOUN → pure-hiragana VERB_未然 (non-dict)
  // E.g., 分+から should be 分から (single verb), not 分(NOUN) + から(VERB かる)
  // When a dictionary entry exists for combined form, penalize the split
  if (prev.pos == core::PartOfSpeech::Noun && normalize::utf8Length(prev.surface) == 1 &&  // Single char
      grammar::isAllKanji(prev.surface) && next.extended_pos == core::ExtendedPOS::VerbMizenkei &&
      !next.fromDictionary() && grammar::isPureHiragana(next.surface)) {
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kVeryRare);  // Penalize split to favor combined dict verb
  }

  // Penalty for demonstrative-based CONJ → kanji VERB pattern
  // E.g., それで帰った should be それ+で+帰っ+た, not それで(CONJ)+帰った
  // Demonstrative-origin conjunctions (それで, そこで, ここで) can be split
  // when followed directly by kanji verb (no comma/pause)
  // "それで" as pure conjunction prefers comma/pause before verb
  // But "それでございます" (それで+ござっ) is valid - ござっ is honorific
  // Apply to both VerbOnbinkei (帰っ) and VerbTaForm (帰った)
  if (prev.pos == core::PartOfSpeech::Conjunction &&
      (prev.surface == "それで" || prev.surface == "そこで" || prev.surface == "ここで") &&
      (next.extended_pos == core::ExtendedPOS::VerbOnbinkei || next.extended_pos == core::ExtendedPOS::VerbTaForm) &&
      grammar::startsWithKanji(next.surface) && !utf8::startsWith(next.surface, "ござ")) {  // Exclude honorific ござる
    SUZUME_CONNECTION_ADD(surface_bonus, cost::kAlmostNever);
  }

  float total = base_cost + extended_cost + surface_bonus;

  SUZUME_DEBUG_VERBOSE_BLOCK {
    SUZUME_DEBUG_STREAM << "[CONN] \"" << prev.surface << "\" (" << core::posToString(prev.pos) << "/"
                        << core::extendedPosToString(prev.extended_pos) << ") → \"" << next.surface << "\" ("
                        << core::posToString(next.pos) << "/" << core::extendedPosToString(next.extended_pos) << "): "
                        << "bigram=" << base_cost << " epos_adj=" << extended_cost;
    if (surface_bonus != 0.0F) {
      SUZUME_DEBUG_STREAM << " surface_bonus=" << surface_bonus;
    }
    if (extended_cost != 0.0F) {
      // Show rule name: PrevEPOS→NextEPOS
      SUZUME_DEBUG_STREAM << " (rule=" << core::extendedPosToString(prev.extended_pos) << "→"
                          << core::extendedPosToString(next.extended_pos) << ")";
    } else if (surface_bonus == 0.0F) {
      SUZUME_DEBUG_STREAM << " (default)";
    }
    SUZUME_DEBUG_STREAM << " total=" << total << "\n";
  }

  return total;
}

}  // namespace suzume::analysis
