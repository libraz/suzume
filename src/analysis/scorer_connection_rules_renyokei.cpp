#include <cmath>

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

namespace suzume::analysis::connection_rules {

// Surface-based connection rules, extracted from connectionCost for readability.
// Each helper accumulates the `surface_bonus +=` contributions of a thematically
// related group of rules and returns their sum. Helpers are self-contained: they
// recompute any needed locals from prev/next and never read caller state. Because
// every contribution is additive, the order among these helpers does not affect the
// total; call sites are kept at their original positions for readability.

// VerbRenyokei attachment to adjectives, auxiliaries, and subsidiary verbs:
// すぎ/AdjBasic, し-conjunction, causative さ, compound-particle→topic, て→い,
// し→てる, and ゆく/いく subsidiary verbs.
float computeVerbRenyokeiEarlyBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  float bonus{};  // value-init to 0

  // A generated compound predicate retains its lexical unit through its
  // connective continuation (思い出し+て). This prevents a dictionary
  // nominalization plus a short homographic verb from winning solely through
  // an unrelated lexical word cost.  The same semantic group covers two
  // closed inflectional continuations: 係結び+仮定形 and 〜ておく+べき.
  // し is excluded because it is not one of those continuations: it coordinates
  // finite clauses and so selects a terminal form (寒いし、暗い), which a
  // continuative is not. After one, that mora can only be the literary past き
  // in its attributive cell (燃えさかり+し炎), and paying the compound to keep
  // its unit through a particle it cannot take hands the boundary to the wrong
  // reading.
  const bool compound_connective = prev.origin == core::CandidateOrigin::VerbCompound &&
                                   next.extended_pos == core::ExtendedPOS::ParticleConj &&
                                   !utf8::equalsAny(next.surface, {"し"});
  // The hypothetical a binding particle takes has to belong to a verb that
  // exists. A pure-hiragana cell the dictionary does not know is the one shape
  // that can be assembled out of the neighbouring closed-class morae, and the
  // particle's own first mora is what makes those available: シク活用 adjectives
  // spell しか at the boundary between stem and 補助活用, so 正しかりければ splits
  // into 正 + しか + a hypothetical of the non-word りける and outbids the カリ活用
  // cell it actually is. A kanji stem cannot be assembled that way (こそ+定まれ),
  // and the productive kana cases are dictionary verbs (さえ+すれ+ば).
  const bool binding_hypothetical = prev.extended_pos == core::ExtendedPOS::ParticleBinding &&
                                    next.extended_pos == core::ExtendedPOS::VerbKateikei &&
                                    (next.lemmaVerified() || !grammar::isPureHiragana(next.surface));
  const bool preparatory_obligation =
      prev.extended_pos == core::ExtendedPOS::AuxAspectOku && next.extended_pos == core::ExtendedPOS::AuxClassicalBeshi;
  if (compound_connective)
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus + cost::kMinorBonus);
  if (binding_hypothetical)
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  if (preparatory_obligation)
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);

  // A sufficiently long pure-hiragana sokuonbin is a complete verbal stem.
  // Before the connective te particle, retain that stem instead of splitting
  // off a shorter verb and treating the remaining syllables as an aspectual
  // auxiliary.
  if (prev.extended_pos == core::ExtendedPOS::VerbOnbinkei && next.extended_pos == core::ExtendedPOS::ParticleConj &&
      grammar::isSingleHiragana(next.surface, U'て') && grammar::isPureHiragana(prev.surface) &&
      prev.surface.size() >= sc::kLongPureHiraganaOnbinMinChars * core::kJapaneseCharBytes) {
    const bool begins_with_particle_homograph = normalize::isNeverVerbStemAtStart(utf8::decodeFirstChar(prev.surface));
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus +
                                     (begins_with_particle_homograph ? cost::kVeryStrongBonus : cost::kNeutral) +
                                     (prev.lemmaVerified() ? cost::kStrongBonus : cost::kNeutral));
  }

  // A Godan continuative — identified by the i-row mora that ends it — keeps its
  // boundary before a closed morpheme that selects a predicate. A multi-mora one
  // can be topicalized before an auxiliary predicate (減り+は+しない), while
  // one-mora verb homographs stay available for closed particles such as しも.
  // Before ぬ the reading can only be the literary perfect: the negative ぬ
  // selects the irrealis, and for a Godan verb the two cells are distinct (摘ま
  // vs 摘み). That boundary needs a decisive weight because the registered
  // siblings win it only through their dictionary edge (積み+ぬ), which an
  // unregistered base verb has no access to. Ichidan stems are outside both
  // rules — their irrealis and continuative are syncretic, so 消え+ぬ is either
  // reading — as is the e-row passive れ, which must still split (行か+れ+ぬ).
  const bool godan_continuative = prev.extended_pos == core::ExtendedPOS::VerbRenyokei &&
                                  grammar::isIRowCodepoint(utf8::decodeLastChar(prev.surface));
  const bool topicalized_continuative = godan_continuative && next.extended_pos == core::ExtendedPOS::ParticleTopic &&
                                        prev.surface.size() >= core::kTwoJapaneseCharBytes;
  const bool literary_perfect = godan_continuative && next.extended_pos == core::ExtendedPOS::AuxNegativeNu &&
                                utf8::equalsAny(next.surface, {"ぬ"});
  if (topicalized_continuative)
    SUZUME_CONNECTION_ADD(bonus, cost::kModerateBonus);
  if (literary_perfect)
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);

  // A continuative can form a productive compound predicate with the
  // irregular suru predicate. Keep this connection available beside the
  // homographic verbal-noun analysis rather than forcing every such phrase
  // through a noun boundary.
  // A sahen continuative may be followed by a dictionary-verified lexical V2
  // while retaining its search boundary (確認し+間違える).  This is distinct
  // from an arbitrary unknown verb sequence, so the lexical gate keeps the
  // connection from promoting fabricated kanji fragments.
  const bool renyokei_suru_compound = prev.extended_pos == core::ExtendedPOS::VerbRenyokei &&
                                      next.extended_pos == core::ExtendedPOS::VerbRenyokei && next.lemma == "する";
  const bool sahen_lexical_v2 = prev.extended_pos == core::ExtendedPOS::VerbRenyokei && prev.lemma == "する" &&
                                next.extended_pos == core::ExtendedPOS::VerbShuushikei && next.fromDictionary();
  const bool verified_v1_v2_mizenkei = prev.extended_pos == core::ExtendedPOS::VerbRenyokei &&
                                       next.extended_pos == core::ExtendedPOS::VerbMizenkei && prev.lemmaVerified() &&
                                       next.lemmaVerified();
  // If the right edge is itself a generated finite compound, splitting a
  // verified continuative immediately before it creates a nested suffix
  // compound (追い+かけ回す). The matcher also emits the maximal compound
  // spanning the same right boundary, so mildly penalize this interior cut.
  const bool nested_finite_compound = prev.extended_pos == core::ExtendedPOS::VerbRenyokei && prev.lemmaVerified() &&
                                      next.origin == core::CandidateOrigin::VerbCompound &&
                                      next.extended_pos == core::ExtendedPOS::VerbShuushikei;
  if (renyokei_suru_compound)
    SUZUME_CONNECTION_ADD(bonus, cost::kMinorBonus);
  if (sahen_lexical_v2)
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  if (verified_v1_v2_mizenkei)
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  if (nested_finite_compound)
    SUZUME_CONNECTION_ADD(bonus, cost::kMinor);

  // Demonstrative manner adverbs form closed compound adverbs with して
  // (こうして, そうして, どうして). Prefer the dictionary compound over a
  // fabricated adverb plus suru-verb sequence.
  if (prev.pos == core::PartOfSpeech::Adverb && grammar::isDemonstrativeUAdverb(prev.surface) &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei && grammar::isSuruRenyokeiSurface(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kMinor);
  }

  // A continuative verb followed by the desiderative ending starts an
  // auxiliary chain (帰り+たい+らしい), not an adjective that absorbs it.
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.pos == core::PartOfSpeech::Adjective &&
      utf8::startsWith(next.surface, "たい")) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrong);
  }

  // Adverbs can modify a negative predicate directly (何とも+思わ+ない,
  // 全く+分から+ない). This is the same productive relation as the existing
  // adverb-to-finite-verb connections.
  // Adverbs also directly modify continuative predicates before their
  // inflectional continuation (ちゃっかり+得+し+た). This keeps the verbal
  // reading ahead of a homographic nominal candidate.
  // Adverbs and adjective continuatives directly modify an imperative or
  // conditional predicate as well. Give those finite e-row forms enough
  // support to beat a fabricated Ichidan continuative with the same surface
  // (少し待て, 強く打て).
  const bool adverb_to_mizenkei =
      prev.pos == core::PartOfSpeech::Adverb && next.extended_pos == core::ExtendedPOS::VerbMizenkei;
  const bool adverb_to_renyokei =
      prev.pos == core::PartOfSpeech::Adverb && next.extended_pos == core::ExtendedPOS::VerbRenyokei;
  const bool modifier_to_finite_e_row =
      (prev.pos == core::PartOfSpeech::Adverb || prev.extended_pos == core::ExtendedPOS::AdjRenyokei) &&
      (next.extended_pos == core::ExtendedPOS::VerbMeireikei || next.extended_pos == core::ExtendedPOS::VerbKateikei);
  if (adverb_to_mizenkei)
    SUZUME_CONNECTION_ADD(bonus, cost::kModerateBonus);
  if (adverb_to_renyokei)
    SUZUME_CONNECTION_ADD(bonus, cost::kMinorBonus);
  if (modifier_to_finite_e_row)
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);

  // A one-mora euphonic verb after an adverb is generally a fabricated split
  // of an inflected predicate. Lexical past forms retain their kanji stem,
  // while the productive adjective inflection remains available as one unit.
  if (prev.pos == core::PartOfSpeech::Adverb && next.extended_pos == core::ExtendedPOS::VerbOnbinkei &&
      grammar::isPureHiragana(next.surface) && next.surface.size() == core::kTwoJapaneseCharBytes) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrong);
  }

  // A dictionary adverb may directly modify a pronoun-headed phrase
  // (時として+己). Prefer the lexical adverb over an unrelated formal noun
  // plus case-particle segmentation.
  if (prev.pos == core::PartOfSpeech::Adverb && prev.fromDictionary() &&
      next.extended_pos == core::ExtendedPOS::Pronoun) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus + cost::kMinorBonus);
  }

  // A sentence-initial conjunction can directly introduce an adjective
  // predicate (でも+よい). Mid-sentence conjunction candidates retain the
  // ordinary score so nominal copular negation remains decomposed.
  if (prev.start == 0 && prev.extended_pos == core::ExtendedPOS::Conjunction &&
      next.extended_pos == core::ExtendedPOS::AdjBasic) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // An adverbial particle can introduce an adjective predicate
  // (何+でも+いい, 学生+でも+ない). The following open-class predicate does
  // not change the particle's grammatical boundary.
  if (prev.extended_pos == core::ExtendedPOS::ParticleAdverbial && next.extended_pos == core::ExtendedPOS::AdjBasic &&
      (next.lemma != "ない" || utf8::equalsAny(prev.surface, {"でも"}))) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus + cost::kMinorBonus);
  }

  // A nominal or na-adjectival copula can be followed by the continuative
  // negative adjective in the change-of-state construction (本で+なく+なっ
  // +た, 静かで+なく+なる). This preserves the copular reading of で.
  if (prev.extended_pos == core::ExtendedPOS::AuxCopulaDa && next.extended_pos == core::ExtendedPOS::AdjRenyokei &&
      next.lemma == "ない") {
    SUZUME_CONNECTION_ADD(bonus, cost::kDoubleVeryStrongBonus);
  }

  // The conditional copular negative retains the closed inflectional chain
  // で+なけれ+ば. Prefer its negative auxiliary over an adjective stem plus
  // the homographic classical auxiliary.
  if (prev.extended_pos == core::ExtendedPOS::AuxCopulaDa && next.extended_pos == core::ExtendedPOS::AuxNegativeNai &&
      utf8::equalsAny(next.surface, {"なけれ"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kDoubleVeryStrongBonus);
  }

  // A dictionary-attested euphonic verb form followed by the connective
  // particle is the productive te-form boundary (読ん+で, 書い+て).  Keep
  // this lexical gate so unknown hiragana fragments remain conservative.
  if (prev.extended_pos == core::ExtendedPOS::VerbOnbinkei && prev.lemmaVerified() &&
      prev.origin != core::CandidateOrigin::VerbCompound && prev.origin != core::CandidateOrigin::Join &&
      next.extended_pos == core::ExtendedPOS::ParticleConj && grammar::isTeDeSurface(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // A compound verb or dictionary-verified humble auxiliary onbin stem takes
  // the conjunctive て (使いきっ+て+しまう, いただい+ている).  The lexical
  // gate keeps the general onbin rule conservative for mimetic fragments.
  const bool is_compound_onbin =
      prev.origin == core::CandidateOrigin::VerbCompound || prev.origin == core::CandidateOrigin::Join;
  const bool is_humble_auxiliary_onbin = prev.fromDictionary() && grammar::isHumbleHonorificLemma(prev.lemma);
  if ((is_compound_onbin || is_humble_auxiliary_onbin) && prev.extended_pos == core::ExtendedPOS::VerbOnbinkei &&
      next.extended_pos == core::ExtendedPOS::ParticleConj && grammar::isTeDeSurface(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // A dictionary-attested continuative directly before て/で is likewise a
  // complete te-form boundary.  This prevents a nominalized homograph from
  // taking precedence over a verified lexical verb (押しのけ+て).
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && prev.fromDictionary() &&
      next.extended_pos == core::ExtendedPOS::ParticleConj && grammar::isTeDeSurface(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kModerateBonus);
  }

  // The connective te-form introduces a subsidiary honorific predicate
  // (見+て+いらっしゃる, 読ん+で+なさる). Keep this productive boundary ahead
  // of a fused te-form candidate, including one-kanji Ichidan stems.
  if (prev.extended_pos == core::ExtendedPOS::ParticleConj && next.extended_pos == core::ExtendedPOS::AuxHonorific) {
    SUZUME_CONNECTION_ADD(bonus, cost::kDoubleVeryStrongBonus);
  }

  // The directional auxiliary いく retains its auxiliary analysis in the
  // conditional -けれ+ば form. The inflected surface gate keeps bare く+て
  // available for its ordinary lexical and auxiliary interpretations.
  if (prev.extended_pos == core::ExtendedPOS::AuxAspectIku && next.extended_pos == core::ExtendedPOS::ParticleConj &&
      utf8::equalsAny(next.surface, {"ば"}) && utf8::endsWith(prev.surface, "けれ")) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus + cost::kStrongBonus);
  }

  // Conditional なら is shared by nouns and na-adjectives. Scope the bonus to
  // this closed-class surface so the generic ParticleConj category does not
  // accidentally promote te-form splits after arbitrary nouns (決し+て).
  if (next.extended_pos == core::ExtendedPOS::ParticleConj && utf8::equalsAny(next.surface, {"なら"})) {
    if (prev.extended_pos == core::ExtendedPOS::AdjNaAdj) {
      SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
    } else if (prev.extended_pos == core::ExtendedPOS::Noun || prev.extended_pos == core::ExtendedPOS::Suffix) {
      // A nominal suffix takes the same conditional copula as its host
      // (作業中+なら). Cancel the generic suffix→conjunctive penalty.
      SUZUME_CONNECTION_ADD(
          bonus, prev.extended_pos == core::ExtendedPOS::Suffix ? cost::kDoubleVeryStrongBonus : cost::kModerateBonus);
    } else if (prev.extended_pos == core::ExtendedPOS::Adverb ||
               prev.extended_pos == core::ExtendedPOS::AdverbQuotative) {
      // Demonstrative adverb + conditional (そう+なら) is a productive
      // conditional construction, not the mizenkei of なる.
      SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
    }
  }

  // The formal conditional ならば is a sequence of two conjunctive particles.
  // Prefer it over the homographic mizenkei of なる followed by ば.
  if (prev.extended_pos == core::ExtendedPOS::ParticleConj && prev.surface == "なら" &&
      next.extended_pos == core::ExtendedPOS::ParticleConj && next.surface == "ば") {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // A te/de connective must attach to a predicate or an explicitly licensed
  // auxiliary; two connective particles cannot be adjacent. Keep this
  // surface-scoped so the productive conditional なら+ば remains available.
  if (prev.extended_pos == core::ExtendedPOS::ParticleConj && next.extended_pos == core::ExtendedPOS::ParticleConj &&
      grammar::isTeDeSurface(prev.surface) && grammar::isTeDeSurface(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, sc::kPenaltyInvalidConjunctiveSequence);
  }

  // Classical past conjecture attaches to a continuative verb form
  // (行き+けむ). It shares AuxVolitional with modern う/よう, which select the
  // irrealis instead, so the bigram cell for that pair is a penalty; this
  // surface-scoped rule cancels it and restores the continuative bonus.
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.extended_pos == core::ExtendedPOS::AuxVolitional &&
      utf8::equalsAny(next.surface, {"けむ"})) {
    SUZUME_CONNECTION_ADD(bonus, sc::kBonusClassicalPastConjecture);
  }

  // Classical completion たり follows the continuative form, and so does the
  // 已然形 of the same auxiliary (記録し+たれ+ども) and the perfect つ (書き+つ).
  // Each terminal surface is also something else — the modern listing particle,
  // the tail of a godan-ta verb — so this left-side rule selects the literary
  // auxiliary in its complete predicate construction (行き+たり). The perfect り
  // is deliberately absent: it takes the realis, and after a continuative the
  // same mora is the far more frequent deverbal-noun ending (書き+まくり).
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.extended_pos == core::ExtendedPOS::AuxClassicalPerfect && utf8::equalsAny(next.surface, {"たり", "たれ"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }
  // The perfect つ needs the narrower margin: a one-mora continuative in front
  // of it is also the opening kana of ordinary words (いつ, かつ).
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.extended_pos == core::ExtendedPOS::AuxClassicalPerfect && utf8::equalsAny(next.surface, {"つ"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // Recent-completion たて attaches directly to a verb's continuative form
  // (焼き+たて、作り+たて). Its dedicated suffix category must overcome the
  // generic Verb→Suffix cost as well as the homographic past-plus-te path.
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.extended_pos == core::ExtendedPOS::SuffixRecentCompletion) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // Classical perfect たり can be followed by the past auxiliary けり
  // (語り+たり+けり). This context disambiguates たり from the modern
  // listing particle without changing ordinary parallel-predicate analysis.
  if (prev.extended_pos == core::ExtendedPOS::AuxClassicalPerfect && utf8::equalsAny(prev.surface, {"たり"}) &&
      next.extended_pos == core::ExtendedPOS::AuxClassicalKeri) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // The classical continuative auxiliary り attaches to the 已然形 of a
  // predicate (行け+り). It is a distinct form from the two-character past
  // auxiliary けり and therefore needs its own boundary preference.
  if (prev.extended_pos == core::ExtendedPOS::VerbKateikei &&
      next.extended_pos == core::ExtendedPOS::AuxClassicalPerfect && utf8::equalsAny(next.surface, {"り"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // A modern hypothetical verb form is followed by the conditional particle
  // ば (食べれ+ば, 書け+ば), and the classical concessive ど/ども selects the
  // 已然形 the same paradigm slot spells (飲め+ど, 読め+ども). Other conjunctive
  // particles cannot complete this inflection, so they must not receive the
  // bonus: without it the continuative of a homographic potential verb
  // (飲める) takes the span and the lemma with it. The i-adjective's け形 fills
  // the same paradigm slot and selects the same particles (寒けれ+ば,
  // めんどくさけれ+ば), so the preference belongs to the slot rather than to the
  // part of speech.
  const bool fills_hypothetical_slot =
      prev.extended_pos == core::ExtendedPOS::VerbKateikei || prev.extended_pos == core::ExtendedPOS::AdjKeForm;
  // The colloquial contraction fills the same slot but has already absorbed
  // its particle into the inflection (行きゃ, 早けりゃ), so it closes the
  // conditional clause instead of opening a place for one. Left alone it keeps
  // the slot's pull on a conjunctive particle and takes the leading mora of the
  // following predicate with it (やりゃ|で|きる for やりゃ|できる).
  const bool absorbed_conjunctive_particle = utf8::endsWith(prev.surface, "ゃ");
  if (fills_hypothetical_slot && !absorbed_conjunctive_particle &&
      next.extended_pos == core::ExtendedPOS::ParticleConj &&
      grammar::isHypotheticalSelectingConjunctiveParticle(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // The conjunctive negative ざり can be followed by classical past けり
  // (行か+ざり+けり).
  if (prev.extended_pos == core::ExtendedPOS::AuxNegativeNu && utf8::equalsAny(prev.surface, {"ざり"}) &&
      next.extended_pos == core::ExtendedPOS::AuxClassicalKeri) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // A finite predicate followed by なり is the closed conjunctive-particle
  // construction expressing immediate succession (鳴る+なり), not the
  // renyokei of lexical なる.
  if (prev.extended_pos == core::ExtendedPOS::VerbShuushikei && next.extended_pos == core::ExtendedPOS::ParticleConj &&
      utf8::equalsAny(next.surface, {"なり"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // The closed causal particle ので owns both morae after a finite predicate.
  // Give the registered compound enough weight to beat a competing
  // nominalizer の + で path even when that path is headed by a strongly
  // selected attributive construction (と+いう+ので).
  const bool finite_predicate_before_node =
      prev.extended_pos == core::ExtendedPOS::VerbShuushikei || core::isAspectAuxiliaryType(prev.extended_pos) ||
      prev.extended_pos == core::ExtendedPOS::AuxExcessive || prev.extended_pos == core::ExtendedPOS::AuxGaru;
  const bool causal_node_after_predicate = finite_predicate_before_node &&
                                           next.extended_pos == core::ExtendedPOS::ParticleConj &&
                                           utf8::equalsAny(next.surface, {"ので"});

  // Before a binding も, the same letters are the nominalizer and copula
  // (の+で+も), not the causal particle followed by a topic marker.
  const bool causal_node_before_binding =
      prev.extended_pos == core::ExtendedPOS::ParticleConj && utf8::equalsAny(prev.surface, {"ので"}) &&
      next.extended_pos == core::ExtendedPOS::ParticleTopic && utf8::equalsAny(next.surface, {"も"});
  if (causal_node_after_predicate || causal_node_before_binding) {
    SUZUME_CONNECTION_ADD(bonus, causal_node_after_predicate ? cost::kTripleVeryStrongBonus : cost::kProhibitive);
  }

  // とともに is a closed compound particle covering both accompaniment
  // (家族とともに) and concurrent-predicate (読むとともに書く) constructions.
  // Its component と + ともに path is otherwise favored by the frequent adverb
  // candidate, so keep the grammatical compound boundary intact.
  if (next.extended_pos == core::ExtendedPOS::ParticleConj && utf8::equalsAny(next.surface, {"とともに"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kDoubleVeryStrongBonus);
  }

  // The object marker cannot normally follow a finite predicate. Keep this
  // restriction surface-scoped because other ParticleCase homographs also act
  // as valid quotation or conditional particles after a finite verb (〜る+と).
  // A dictionary-backed verb receives a smaller penalty so a rare
  // zero-nominalized object clause can retain its lexical predicate reading.
  if (prev.extended_pos == core::ExtendedPOS::VerbShuushikei && next.extended_pos == core::ExtendedPOS::ParticleCase &&
      grammar::isAccusativeParticleWoSurface(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, prev.fromDictionary() ? cost::kRare : cost::kStrong);
  }

  // A predicate-final に can introduce a continuative form only when that
  // form was generated with a following negative auxiliary (読むに+たえ+ない).
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase && utf8::equalsAny(prev.surface, {"に"}) &&
      next.origin == core::CandidateOrigin::VerbHiraganaNegativeRenyokei) {
    SUZUME_CONNECTION_ADD(bonus, cost::kDoubleVeryStrongBonus);
  }

  if (prev.origin == core::CandidateOrigin::VerbHiraganaNegativeRenyokei &&
      next.extended_pos == core::ExtendedPOS::AuxNegativeNai) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // The contracted negative past is a single auxiliary stem (読ま+んかっ+た).
  // Its complete form must outrank a nominalizer followed by an unrelated
  // sokuon-bin verb.
  if (prev.extended_pos == core::ExtendedPOS::VerbMizenkei && next.extended_pos == core::ExtendedPOS::AuxNegativeNai &&
      utf8::equalsAny(next.surface, {"んかっ"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kModerateBonus);
  }

  // The quotative particle followed by 言う's hypothetical form is the
  // productive concessive/conditional construction と+いえ(ども/ば). Preserve
  // this boundary over a generated verb that absorbs the noun and quotation.
  if (prev.start > 0 && prev.extended_pos == core::ExtendedPOS::ParticleCase &&
      grammar::isSingleHiragana(prev.surface, core::hiragana::kTo) &&
      next.extended_pos == core::ExtendedPOS::VerbKateikei && next.lemma == "いう") {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // A quotative particle followed by finite 言う forms the productive
  // explanatory boundary used in introductions such as か+というと.
  if (prev.start > 0 && prev.extended_pos == core::ExtendedPOS::ParticleCase &&
      grammar::isSingleHiragana(prev.surface, core::hiragana::kTo) &&
      next.extended_pos == core::ExtendedPOS::VerbShuushikei && next.lemma == "いう") {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // The finite quotative verb introduces an explanatory consequence with と
  // (というと). Keep that predicate boundary ahead of the unrelated い+う
  // auxiliary sequence.
  if (prev.extended_pos == core::ExtendedPOS::VerbShuushikei && prev.lemma == "いう" &&
      next.extended_pos == core::ExtendedPOS::ParticleCase &&
      grammar::isSingleHiragana(next.surface, core::hiragana::kTo)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // A continuative verb followed by に can express a purpose construction
  // (読みに行く) or the honorific お+連用形+に+なる construction. The generic
  // VerbRenyokei→ParticleCase cost favors nominal readings for object markers;
  // restore this distinct grammatical boundary without changing を/で/etc.
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.extended_pos == core::ExtendedPOS::ParticleCase &&
      utf8::equalsAny(next.surface, {"に"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // Unsupported continuative attachments lose to their grammatical
  // homographs. A bare continuative verb cannot directly modify an arbitrary
  // adjective, except for the closed derivational suffix class
  // にくい/やすい/がたい/づらい (including the kanji spelling 難い). Likewise,
  // directional へ selects a nominal goal, so an apparent i-adjective
  // continuative before it is the spatial/temporal noun (近く+へ, 遠く+へ).
  const bool verb_before_adjective =
      prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.extended_pos == core::ExtendedPOS::AdjBasic;
  const bool adjective_before_directional_case = prev.extended_pos == core::ExtendedPOS::AdjRenyokei &&
                                                 next.extended_pos == core::ExtendedPOS::ParticleCase &&
                                                 utf8::equalsAny(next.surface, {"へ"});
  if (verb_before_adjective || adjective_before_directional_case) {
    if (verb_before_adjective && utf8::equalsAny(next.lemma, {"にくい", "やすい", "がたい", "づらい", "難い"})) {
      SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
    } else {
      SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
    }
  }

  // An unverified Godan-wa continuative ending in い is locally homographic
  // with an i-adjective.  It cannot form an unmarked noun+predicate or
  // verb+predicate sequence merely by adjacency; productive compound verbs
  // have their own verified V2 rules below. Penalize only those unsupported
  // left connections so the adjective/nominal readings remain available.
  if (next.extended_pos == core::ExtendedPOS::VerbRenyokei && next.conj_type == dictionary::ConjugationType::GodanWa &&
      !next.lemmaVerified() &&
      (prev.pos == core::PartOfSpeech::Noun || prev.extended_pos == core::ExtendedPOS::VerbRenyokei)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrong);
  }

  // An attested euphonic lexical verb followed by the past auxiliary remains
  // one search unit (言い損なっ+た), ahead of a productive subsidiary split.
  if (prev.fromDictionary() && prev.extended_pos == core::ExtendedPOS::VerbOnbinkei &&
      next.extended_pos == core::ExtendedPOS::AuxTenseTa) {
    SUZUME_CONNECTION_ADD(bonus, cost::kModerateBonus);
  }

  // The uncontracted preparation subsidiary is written after an explicit
  // connective particle (食べ+て+おく). Its lexical homograph おく must not win
  // merely because both candidates share the same surface. Contracted とく/どく
  // instead attaches directly to a verb stem or onbin form and deliberately does
  // not receive this extra particle-boundary bonus.
  if (prev.extended_pos == core::ExtendedPOS::ParticleConj && next.extended_pos == core::ExtendedPOS::AuxAspectOku &&
      utf8::startsWith(next.surface, "お")) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // The quotative conditional とあれば retains the finite irregular form as
  // one search unit. Other particle contexts can use the regular あれ+ば
  // boundary (さえあれば, こそあれば).
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase &&
      grammar::isSingleHiragana(prev.surface, core::hiragana::kTo) &&
      next.extended_pos == core::ExtendedPOS::VerbKateikei && next.lemma == "ある" &&
      grammar::isAruHypotheticalSurface(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // The continuative allomorph of ある can be followed by a topic marker in
  // emphatic negation (あり+は+しない). Keep its auxiliary analysis ahead of
  // the homographic lexical verb candidate.
  if (prev.extended_pos == core::ExtendedPOS::AuxCopulaDa && grammar::isAruContinuativeSurface(prev.surface) &&
      next.extended_pos == core::ExtendedPOS::ParticleTopic) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // The potential form of the humble receiving verb is a benefactive
  // auxiliary after a te-form (見て+いただける, 読んで+いただける).  Its
  // dictionary entries retain a verbal ExtendedPOS until postprocessing
  // assigns the auxiliary POS, so select every attested inflection in this
  // closed class (いただける／いただけない／いただければ).
  const bool is_potential_benefactive_inflection = next.extended_pos == core::ExtendedPOS::VerbShuushikei ||
                                                   next.extended_pos == core::ExtendedPOS::VerbRenyokei ||
                                                   next.extended_pos == core::ExtendedPOS::VerbKateikei;
  if (prev.extended_pos == core::ExtendedPOS::ParticleConj && is_potential_benefactive_inflection &&
      next.fromDictionary() && grammar::isPotentialBenefactiveLemma(next.lemma)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kDoubleVeryStrongBonus);
  }

  // The directional subsidiary いく retains a verbal dictionary category, but
  // after the connective particle it forms the productive aspectual sequence
  // て+いく rather than an unrelated coordinate predicate.
  if (prev.extended_pos == core::ExtendedPOS::ParticleConj && next.extended_pos == core::ExtendedPOS::VerbShuushikei &&
      next.fromDictionary() && next.lemma == "いく") {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // Penalty for VerbRenyokei + し(conjunction) with kanji verb
  // In modern Japanese, conjunction し follows shuushikei (行く+し), not renyoukei (行き+し).
  // VerbRenyokei + し is usually a false split of godan-sa renyoukei (尽く+し → 尽くし).
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && grammar::isConjunctiveParticleShi(next.surface) &&
      next.extended_pos == core::ExtendedPOS::ParticleConj && grammar::containsKanji(prev.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kMinor);  // Penalty to discourage false split
  }

  // VerbRenyokei (A-row ending) → VerbMizenkei(さ) causative pattern
  // E.g., やら+さ+れ+た — hiragana verb mizenkei + causative さ
  // VerbRenyokei is used for short hiragana verbs (EPOS can't distinguish mizenkei)
  // Need strong bonus to overcome VerbRenyokei→VerbMizenkei bigram penalty (1.8)
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.extended_pos == core::ExtendedPOS::VerbMizenkei &&
      next.surface == "さ" && grammar::endsWithARow(prev.surface)) {
    SUZUME_CONNECTION_ADD(bonus, sc::kBonusVerbCausativePattern);
  }

  // Compound particle (≥2 chars) → topic/binding particle (は, も, が)
  // E.g., まで+も, より+も, にとって+も, について+は
  // Excludes one-char particles to avoid boosting て+も, し+は, で+も.
  // Needs to overcome ADV→NOUN bonus advantage in competing paths
  if ((prev.extended_pos == core::ExtendedPOS::ParticleConj || prev.extended_pos == core::ExtendedPOS::ParticleCase) &&
      next.extended_pos == core::ExtendedPOS::ParticleTopic && prev.surface.size() >= core::kTwoJapaneseCharBytes) {
    SUZUME_CONNECTION_ADD(bonus, sc::kBonusCompoundParticleToTopic);
  }

  // An attributive compound case particle ending in る modifies the following
  // noun (に関する問題, に対する答え, における資料). Prefer that grammatical
  // boundary over an unrelated Ichidan renyokei candidate at the noun's start.
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase && next.extended_pos == core::ExtendedPOS::Noun &&
      prev.surface.size() >= core::kThreeJapaneseCharBytes && utf8::endsWith(prev.surface, "る")) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus + cost::kModerateBonus);
  }

  // A case-marked object can be followed by a numeral-counter phrase
  // (品物を一つ). Prefer the quantity over a fabricated verb whose first kanji
  // happens to have a matching Godan reading.
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase && next.extended_pos == core::ExtendedPOS::Noun &&
      normalize::isNumeralCodepoint(utf8::decodeFirstChar(next.surface))) {
    SUZUME_CONNECTION_ADD(bonus, cost::kModerateBonus);
  }

  // A multi-mora case-particle candidate after an explicit volitional auxiliary
  // would swallow the quotative と and following verb (書こ+う+として). Keep the
  // one-mora と connection licensed, but reject compound-particle attachment so
  // the productive う+と+し+て boundary remains available.
  const bool volitional_before_compound_case = prev.extended_pos == core::ExtendedPOS::AuxVolitional &&
                                               next.extended_pos == core::ExtendedPOS::ParticleCase &&
                                               next.surface.size() >= core::kTwoJapaneseCharBytes;

  // A volitional auxiliary followed by the quotative particle is the productive
  // intent construction (食べよう+とする). Keep it ahead of the homographic
  // renyokei + formal-noun よう path, whose case-particle continuations are
  // instead が/に (読みようがない, 書きようによって).
  const bool volitional_before_quotative =
      prev.extended_pos == core::ExtendedPOS::AuxVolitional && next.extended_pos == core::ExtendedPOS::ParticleCase &&
      grammar::isSingleHiragana(next.surface, core::hiragana::kTo) && utf8::equalsAny(prev.surface, {"う", "よう"});

  // A finite volitional clause can be followed by a closed conjunctive
  // particle (行こう+とも, 遠かろう+とも). Prefer the complete particle over
  // reopening it as the independently valid one-mora と + も sequence.
  const bool volitional_before_concessive = prev.extended_pos == core::ExtendedPOS::AuxVolitional &&
                                            next.extended_pos == core::ExtendedPOS::ParticleConj &&
                                            grammar::isConcessiveParticleTomoSurface(next.surface);

  // The quotative determiner remains a single search unit after a volitional
  // auxiliary (だろ+う+という+見込み), just as it does after a finite verb.
  // Without this connection, the very productive intent sequence う+と gives
  // the competing と+いう path an unrelated advantage.
  const bool volitional_before_quotative_determiner = prev.extended_pos == core::ExtendedPOS::AuxVolitional &&
                                                      next.extended_pos == core::ExtendedPOS::DeterminerQuotative;

  // The one-mora literary volitional ん is selected only in the quotative
  // construction ～んとする. Elsewhere the homographic contracted negative
  // remains the productive modern analysis (読まん、食べん).
  const bool literary_volitional_outside_quotative = prev.extended_pos == core::ExtendedPOS::AuxVolitional &&
                                                     grammar::isSingleHiragana(prev.surface, core::hiragana::kN) &&
                                                     !(next.extended_pos == core::ExtendedPOS::ParticleCase &&
                                                       grammar::isSingleHiragana(next.surface, core::hiragana::kTo));
  if (volitional_before_compound_case || volitional_before_quotative || volitional_before_concessive ||
      volitional_before_quotative_determiner || literary_volitional_outside_quotative) {
    SUZUME_CONNECTION_ADD(
        bonus, (volitional_before_compound_case ? cost::kSevere : cost::kNeutral) +
                   (volitional_before_quotative ? cost::kDoubleVeryStrongBonus : cost::kNeutral) +
                   (volitional_before_concessive ? cost::kDoubleVeryStrongBonus + cost::kDoubleVeryStrongBonus
                                                 : cost::kNeutral) +
                   (volitional_before_quotative_determiner ? cost::kDoubleVeryStrongBonus + cost::kMinorBonus
                                                           : cost::kNeutral) +
                   (literary_volitional_outside_quotative ? cost::kSevere : cost::kNeutral));
  }

  // The conjunctive-particle homograph なり cannot follow an i-adjective's
  // adverbial form. 高くなり is 高く+なり(なる), whereas 鳴るなり uses the
  // particle after a finite verb.
  if (prev.extended_pos == core::ExtendedPOS::AdjRenyokei && next.extended_pos == core::ExtendedPOS::ParticleConj &&
      utf8::equalsAny(next.surface, {"なり"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrong);
  }

  // Pronouns and nouns can form the concessive/adverbial construction
  // われ+ながら. Prefer that closed-class boundary over a generated verb
  // homograph such as われ(われる).
  if (prev.extended_pos == core::ExtendedPOS::Pronoun && next.extended_pos == core::ExtendedPOS::ParticleConj &&
      utf8::equalsAny(next.surface, {"ながら"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // Bonus for て → い (Auxiliary) pattern
  // E.g., して+い+ます, 食べて+い+た (aspectual い is auxiliary, not a lexical verb)
  // The auxiliary い (from いる) should win over verb renyokei い
  if (prev.surface == "て" && prev.extended_pos == core::ExtendedPOS::ParticleConj && next.surface == "い" &&
      next.extended_pos == core::ExtendedPOS::AuxAspectIru) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // Penalty for し (PART_接続) → てる (AuxAspectIru) pattern
  // E.g., 何してる should be 何+し(VERB)+てる, not 何+し(PART)+てる
  // The reasoning conjunction し should not be directly followed by progressive てる
  // This cancels the ParticleConj→AuxAspectIru bonus (-0.8) for this specific case
  if (grammar::isConjunctiveParticleShi(prev.surface) && prev.extended_pos == core::ExtendedPOS::ParticleConj &&
      next.surface == "てる" && next.extended_pos == core::ExtendedPOS::AuxAspectIru) {
    SUZUME_CONNECTION_ADD(bonus, cost::kRare);  // Cancel the -0.8 bonus
  }

  // Bonus for VerbRenyokei → subsidiary verb ゆく/いく (補助動詞)
  // V1 連用形 + ゆく forms the literary compound-verb construction (散り+ゆく, 消え+ゆく, 暮れ+ゆく).
  // The generic VerbRenyokei→VerbShuushikei penalty (0.8) guards against false
  // splits, but ゆく/いく after 連用形 is grammatical and beats the NOUN fallback.
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.pos == core::PartOfSpeech::Verb &&
      (next.surface == "ゆく" || next.surface == "いく")) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // A kanji godan-wa renyokei may head a productive compound predicate
  // (沿い+進む, 担い+進む). Its conjugation type distinguishes it from
  // other i-ending renyokei forms and from the competing unknown i-adjective.
  // Require a dictionary-verified following verb so ordinary kanji compounds
  // are not promoted as predicate chains.
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.extended_pos == core::ExtendedPOS::VerbShuushikei &&
      grammar::containsKanji(prev.surface) && prev.conj_type == dictionary::ConjugationType::GodanWa &&
      next.lemmaVerified()) {
    // Multi-kanji stems incur the general unregistered-renyokei penalty.
    // In this verified compound-predicate environment that penalty would
    // otherwise let an unrelated i-adjective candidate win.
    const bool needs_unregistered_stem_offset = normalize::utf8Length(prev.surface) >= 3;
    SUZUME_CONNECTION_ADD(
        bonus, cost::kStrongBonus + cost::kModerateBonus +
                   (needs_unregistered_stem_offset ? cost::kStrongBonus + cost::kMinorBonus + cost::kMinorBonus
                                                   : cost::kNeutral));
  }

  // A dictionary-attested single-kanji する verb can use its bare 連用形 as
  // literary/written coordination (反し+進める).  Restrict this to the
  // verified lexical Suru shape so ordinary compositional Sahen predicates
  // keep their noun+し search-unit boundary.
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.extended_pos == core::ExtendedPOS::VerbShuushikei &&
      prev.conj_type == dictionary::ConjugationType::Suru && prev.lemmaVerified() && next.lemmaVerified() &&
      normalize::utf8Length(prev.surface) == 2 && grammar::containsKanji(prev.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus + cost::kModerateBonus);
  }

  return bonus;
}

}  // namespace suzume::analysis::connection_rules
