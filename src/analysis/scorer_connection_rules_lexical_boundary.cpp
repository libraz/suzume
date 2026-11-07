/**
 * @file scorer_connection_rules_lexical_boundary.cpp
 * @brief Late lexical boundary rules: impossible attachments, barred hosts, retained
 *        boundaries, closed-clause particles and generated-edge guards
 */

#include "analysis/bigram_table.h"
#include "analysis/scorer.h"
#include "analysis/scorer_connection_rules.h"
#include "analysis/scorer_connection_rules_internal.h"
#include "analysis/scorer_constants.h"
#include "analysis/verb_candidates_verb_stems.h"
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

namespace {

// The late lexical boundary groups share one accumulator, added in call order.

// Function words placed on a host they cannot attach to, and the regional
// ておる/でおる contraction that attaches without one.
void addImpossibleAttachmentRules(const core::LatticeEdge& prev, const core::LatticeEdge& next, float& bonus) {
  // AuxAspectIru requires a te-form; regional ておる / でおる contractions
  // are the productive direct-attachment exception.
  // The uninflected とう/どう share their spelling with the desiderative's onbin
  // after a continuative (食べとうない), so only an onbin host proves them.
  const bool is_dialectal_oru_contraction =
      next.extended_pos == core::ExtendedPOS::AuxAspectIru && grammar::isDialectalOruContractionLemma(next.lemma) &&
      (prev.extended_pos == core::ExtendedPOS::VerbOnbinkei || !utf8::endsWith(next.lemma, "う"));
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
  // The attributive copula な likewise cannot stand before punctuation (それな、).
  const bool incomplete_potential_before_symbol =
      next.extended_pos == core::ExtendedPOS::Symbol &&
      ((prev.extended_pos == core::ExtendedPOS::AuxPotential && normalize::utf8Length(prev.surface) == 1) ||
       (prev.extended_pos == core::ExtendedPOS::AuxCopulaDa && grammar::isAttributiveCopulaNa(prev.surface)));
  // が after a terminal verb has two readings, and neither is open to a
  // fabricated predicate.  The case particle requires a nominal on its left, so
  // it is barred there outright and the adversative conjunctive particle takes
  // the position instead.  That connective in turn requires a clause, which an
  // unregistered verb candidate has not established: it is what a kana run
  // looks like when it is cut at a plausible terminal ending (ゆうがた as
  // ゆう+が+ただ).  A registered predicate keeps the connective reading free.
  // The same nominal requirement bars the case particle after the other finite
  // cells — an adjective, the past, the negative and the copulas (寒い+が,
  // 学生だ+が, 行った+が) — where が can only be the adversative. A terminal
  // whose lemma is attested nowhere is no nominalized clause before に/を
  // either: it is a kana noun cut at a terminal-looking mora (こたつ+に).
  const bool finite_predicate_cell =
      prev.extended_pos == core::ExtendedPOS::AdjBasic || prev.extended_pos == core::ExtendedPOS::AuxTenseTa ||
      prev.extended_pos == core::ExtendedPOS::AuxNegativeNai || prev.extended_pos == core::ExtendedPOS::AuxCopulaDa ||
      prev.extended_pos == core::ExtendedPOS::AuxCopulaDesu || prev.extended_pos == core::ExtendedPOS::AuxTenseMasu;
  const bool terminal_verb_before_ga =
      ((prev.extended_pos == core::ExtendedPOS::VerbShuushikei || finite_predicate_cell) &&
       next.extended_pos == core::ExtendedPOS::ParticleCase && grammar::isSingleHiragana(next.surface, U'が')) ||
      (prev.extended_pos == core::ExtendedPOS::VerbShuushikei &&
       ((next.extended_pos == core::ExtendedPOS::ParticleConjFinite && !prev.fromDictionary()) ||
        (next.extended_pos == core::ExtendedPOS::ParticleCase && !prev.lemmaVerified() &&
         utf8::equalsAny(next.surface, {"に", "を"}))));
  // The assertive copula predicates over a nominal.  A conditional verb form or
  // a potential auxiliary directly before it is therefore an accidental
  // homograph chain.  Restrict this to the two nonterminal readings that can
  // fabricate い|え|だっ and いえ|だっ; terminal/modal predicates have valid
  // productive copular continuations. A continuative (食べ, 見て+み) takes the
  // imperative particle な and a terminal (帰る, 帰ん) the prohibitive one,
  // never the attributive copula; nor does the negative ぬ/ん, which is
  // attributive itself (見ん+な is the prohibitive, 知らん+な the final particle).
  const bool nonterminal_predicate_before_assertive_copula =
      next.extended_pos == core::ExtendedPOS::AuxCopulaDa &&
      (prev.extended_pos == core::ExtendedPOS::VerbKateikei || prev.extended_pos == core::ExtendedPOS::AuxPotential ||
       ((prev.extended_pos == core::ExtendedPOS::VerbRenyokei ||
         prev.extended_pos == core::ExtendedPOS::AuxAspectMiru ||
         prev.extended_pos == core::ExtendedPOS::VerbShuushikei ||
         prev.extended_pos == core::ExtendedPOS::AuxNegativeNu) &&
        grammar::isAttributiveCopulaNa(next.surface)));
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
  // The connective て/で takes an i-adjective only through its continuative
  // くて, never its terminal (せんとい+て is a fabricated adjective).
  const bool terminal_adjective_before_te = prev.extended_pos == core::ExtendedPOS::AdjBasic &&
                                            next.extended_pos == core::ExtendedPOS::ParticleConj &&
                                            grammar::isTeDeSurface(next.surface);
  // The clipped polite copula す stands only on the nominalizer ん (行くん+す).
  const bool clipped_desu_off_nominalizer = next.extended_pos == core::ExtendedPOS::AuxCopulaDesu &&
                                            normalize::utf8Length(next.surface) == 1 &&
                                            prev.extended_pos != core::ExtendedPOS::ParticleNo;
  // The listing や joins nominals, and its literary use closes a terminal or
  // attributive predicate (語るや, たるや); the past, an i-adjective-like
  // terminal (including the negative ない) or a bare continuative in front of
  // it is the start of やん/いや/やつ instead.
  const bool listing_ya_after_predicate =
      next.extended_pos == core::ExtendedPOS::ParticleConj && utf8::equalsAny(next.surface, {"や"}) &&
      (prev.extended_pos == core::ExtendedPOS::AuxTenseTa || prev.extended_pos == core::ExtendedPOS::AdjBasic ||
       prev.extended_pos == core::ExtendedPOS::AuxNegativeNai || prev.extended_pos == core::ExtendedPOS::VerbRenyokei);
  // The copular ある completes で(+binding particle)+ある; directly on a nominal
  // it is the existential verb with its case particle dropped (ことある).
  const bool copular_aru_on_nominal = next.extended_pos == core::ExtendedPOS::AuxCopulaDa &&
                                      prev.pos == core::PartOfSpeech::Noun &&
                                      grammar::isCopulaPeriphrasisCell(next.surface);
  // The concessive のに closes its clause; before a binding particle the
  // sequence is the nominalizer plus purposive に (好きなの+に+は).
  const bool concessive_noni_before_binding = prev.extended_pos == core::ExtendedPOS::ParticleConj &&
                                              utf8::equalsAny(prev.surface, {"のに"}) &&
                                              next.extended_pos == core::ExtendedPOS::ParticleTopic;
  // The attributive な needs a nominal head, which a final particle is not;
  // か+な in front of a predicate is an irrealis cut at its stem (行+か+な).
  const bool attributive_na_after_final_particle = prev.extended_pos == core::ExtendedPOS::ParticleFinal &&
                                                   next.extended_pos == core::ExtendedPOS::AuxCopulaDa &&
                                                   grammar::isAttributiveCopulaNa(next.surface);
  // The continuative でし of です exists only before the past (でし+た/たら) and
  // the connective (でし+て); elsewhere で+し is a case particle and a kana run.
  const bool polite_copula_continuative_stranded =
      prev.extended_pos == core::ExtendedPOS::AuxCopulaDesu && utf8::equalsAny(prev.surface, {"でし"}) &&
      next.extended_pos != core::ExtendedPOS::AuxTenseTa &&
      !(next.extended_pos == core::ExtendedPOS::ParticleConj && grammar::isTeDeSurface(next.surface));
  // A nominal or na-stem takes the attributive copula, so an adverb spelled
  // from it there is the copula chain instead (好き+な+ん+で, not 好き+なんで).
  const bool copula_spelled_adverb_on_nominal =
      (core::isNounType(prev.extended_pos) || prev.extended_pos == core::ExtendedPOS::AdjNaAdj) &&
      next.pos == core::PartOfSpeech::Adverb && utf8::startsWith(next.surface, "な");
  // The contracted nominalizer ん closes a nominal; unlike の it never marks
  // the subject of a following clause (が+ん+ばり is がんばり cut apart), and it
  // does not host the conjunctive なり, which takes a verb terminal (す+ん+なり
  // is すんなり cut apart).
  const bool contracted_nominalizer_before_continuative =
      prev.extended_pos == core::ExtendedPOS::ParticleNo && utf8::equalsAny(prev.surface, {"ん"}) &&
      (next.extended_pos == core::ExtendedPOS::VerbRenyokei ||
       (next.extended_pos == core::ExtendedPOS::ParticleConj && utf8::equalsAny(next.surface, {"なり"})));
  // The contracted nominalizer ん closes an attributive predicate; an adverb
  // has no attributive form to host it (ふと+ん is ふとん).
  const bool contracted_nominalizer_after_adverb = prev.pos == core::PartOfSpeech::Adverb &&
                                                   next.extended_pos == core::ExtendedPOS::ParticleNo &&
                                                   utf8::equalsAny(next.surface, {"ん"});
  // The geminate こっ is こと assimilated to the copula after it, and that ちゃ
  // is じゃ assimilated to the geminate (えらい+こっ+ちゃ): neither stands alone,
  // and like any formal noun こっ needs a modifier rather than a particle
  // before it (肩が+こっ+ちゃ is the verb 凝る).
  const bool geminate_formal_noun =
      prev.extended_pos == core::ExtendedPOS::NounFormal && utf8::endsWith(prev.surface, "っ");
  const bool assimilated_copula =
      next.extended_pos == core::ExtendedPOS::AuxCopulaDa && grammar::isGeminateAssimilatedCopula(next.surface);
  const bool unpaired_assimilated_copula =
      geminate_formal_noun != assimilated_copula ||
      (prev.pos == core::PartOfSpeech::Particle && next.extended_pos == core::ExtendedPOS::NounFormal &&
       utf8::endsWith(next.surface, "っ"));
  // A final particle spelled like a verb terminal (なう) closes an auxiliary
  // or a word written outside the kana run; after a bare kana piece, even one
  // that is itself a verb, it is the tail of a kana verb (うし+なう, つぐ+なう).
  // The light verb する is the exception: it closes the noun that hosts it
  // and opens no longer kana verb (参加+する+もふ).
  const bool terminal_spelled_final_particle_in_kana_run =
      next.extended_pos == core::ExtendedPOS::ParticleFinal && grammar::endsWithURow(next.surface) &&
      prev.pos != core::PartOfSpeech::Auxiliary && !grammar::isSuruBaseForm(prev.lemma) &&
      grammar::isPureHiragana(prev.surface);
  // The contracted volitional っ is a cell of a predicate's volitional; a
  // nominal in front of it is a kana run cut apart (そこ+っ+か).
  const bool contracted_volitional_off_predicate =
      next.extended_pos == core::ExtendedPOS::AuxVolitional && utf8::equalsAny(next.surface, {"っ"}) &&
      prev.extended_pos != core::ExtendedPOS::VerbMizenkei && prev.pos != core::PartOfSpeech::Auxiliary;
  // Contracted ては/では fuses a te-form, so an adverb cannot host it
  // (もっと+ちゃんと is not もっと+ちゃ+ん+と).
  const bool contracted_te_wa_off_adverb = prev.pos == core::PartOfSpeech::Adverb &&
                                           next.extended_pos == core::ExtendedPOS::ParticleConj &&
                                           grammar::isContractedTeWaParticle(next.surface);
  // An adverb spelling a geminate past (たった) modifies a quantity after it;
  // in front of an auxiliary it is the verb's past instead (たっ+た+だろ+う).
  const bool geminate_past_adverb_before_auxiliary = prev.pos == core::PartOfSpeech::Adverb && prev.fromDictionary() &&
                                                     utf8::endsWith(prev.surface, "った") &&
                                                     next.pos == core::PartOfSpeech::Auxiliary;
  // The dubitative かも closes a terminal or a nominal; a bare continuative in
  // front of it is the conjunction cut apart (し+かも is しかも).
  const bool dubitative_kamo_after_continuative = prev.extended_pos == core::ExtendedPOS::VerbRenyokei &&
                                                  next.extended_pos == core::ExtendedPOS::ParticleAdverbial &&
                                                  utf8::equalsAny(next.surface, {"かも"});
  // A kana geminate onbin before an adverbial particle, which follows the past
  // (行っ+た+きり), or an unverified one before the continuative cell of the
  // contraction, is an AっBり mimetic cut apart (すっ+きり, うっ+とり).
  const bool kana_geminate_onbin = prev.extended_pos == core::ExtendedPOS::VerbOnbinkei &&
                                   grammar::isPureHiragana(prev.surface) && utf8::endsWith(prev.surface, "っ");
  const bool mimetic_cut_at_geminate =
      kana_geminate_onbin && (next.extended_pos == core::ExtendedPOS::ParticleAdverbial ||
                              (next.extended_pos == core::ExtendedPOS::AuxAspectIru && is_dialectal_oru_contraction &&
                               !prev.fromDictionary() && !prev.lemmaVerified() && grammar::endsWithIRow(next.surface)));
  // A final particle closes the utterance, so no conjunctive particle hangs
  // off it (行こう+け+ど is 行こう+けど).
  const bool conjunctive_after_final_particle =
      prev.extended_pos == core::ExtendedPOS::ParticleFinal && next.extended_pos == core::ExtendedPOS::ParticleConj;
  // Contracted じゃ (では, or ては after a voiced onbin) selects a predicate,
  // and the ai→ee negative ねえ is spelled like the lengthened final particle
  // (そうじゃ+ねえ).
  const bool final_nee_after_contracted_copula = utf8::equalsAny(prev.surface, {"じゃ"}) &&
                                                 next.extended_pos == core::ExtendedPOS::ParticleFinal &&
                                                 utf8::equalsAny(next.surface, {"ねえ", "ねー", "ねぇ"});
  if (invalid_aspect_iru_attachment || invalid_aspect_iku_attachment || incomplete_potential_before_symbol ||
      terminal_verb_before_ga || nonterminal_predicate_before_assertive_copula || emphatic_adverb_before_past ||
      volitional_after_stray_kanji || terminal_adjective_before_te || clipped_desu_off_nominalizer ||
      listing_ya_after_predicate || copular_aru_on_nominal || concessive_noni_before_binding ||
      attributive_na_after_final_particle || polite_copula_continuative_stranded || copula_spelled_adverb_on_nominal ||
      contracted_nominalizer_before_continuative || contracted_nominalizer_after_adverb ||
      unpaired_assimilated_copula || terminal_spelled_final_particle_in_kana_run ||
      contracted_volitional_off_predicate || conjunctive_after_final_particle || contracted_te_wa_off_adverb ||
      dubitative_kamo_after_continuative || geminate_past_adverb_before_auxiliary || mimetic_cut_at_geminate ||
      final_nee_after_contracted_copula) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }
  if ((prev.extended_pos == core::ExtendedPOS::VerbRenyokei || prev.extended_pos == core::ExtendedPOS::VerbOnbinkei) &&
      next.extended_pos == core::ExtendedPOS::AuxAspectIru && is_dialectal_oru_contraction &&
      !mimetic_cut_at_geminate) {
    SUZUME_CONNECTION_ADD(bonus, cost::kExtremeBonus);
  }
}

// Conjunctions, focus particles and kana-lemma formal nouns on a host that
// supplies no clause or phrase boundary in front of them.
void addBarredHostRules(const core::LatticeEdge& prev, const core::LatticeEdge& next, float& bonus) {
  // Conjunctions cannot usually start from a bare token or an unknown
  // hiragana noun; after a conjunction, hiragana で is not 出る's renyokei.
  if (next.pos == core::PartOfSpeech::Conjunction && prev.pos != core::PartOfSpeech::Symbol &&
      prev.pos != core::PartOfSpeech::Particle && prev.pos != core::PartOfSpeech::Auxiliary &&
      normalize::utf8Length(prev.surface) == 1) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }
  bool barred_conjunction_host = false;
  if (next.pos == core::PartOfSpeech::Conjunction) {
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
    barred_conjunction_host =
        (conjunction_host_slot && (te_form_conjunction || copula_initial_conjunction || conditional_to_conjunction)) ||
        (prev.pos == core::PartOfSpeech::Auxiliary && copula_initial_conjunction);
  }
  // A focus particle marks a phrase the analyzer has already identified. An
  // unknown hiragana noun is the fallback for material it could not, so the
  // pairing means the boundary landed inside a word rather than after one
  // (昔+の+まま+で, where のま+まで invents a host for まで).
  const bool barred_focus_particle_host = next.extended_pos == core::ExtendedPOS::ParticleAdverbial &&
                                          prev.pos == core::PartOfSpeech::Noun && !prev.fromDictionary() &&
                                          prev.origin != core::CandidateOrigin::BracketedNoun &&
                                          grammar::isPureHiragana(prev.surface);
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
  // the directional subsidiary いく/ゆく/くる. This would otherwise prefer the
  // lexical entries もって and かえって over the productive V-て + subsidiary
  // analysis. The gate is grammatical rather than surface-specific, so real
  // conjunctions before ordinary lexical verbs remain available.
  const bool closed_te_form_before_directional_aux =
      (prev.pos == core::PartOfSpeech::Conjunction || prev.pos == core::PartOfSpeech::Adverb) &&
      grammar::isPureHiragana(prev.surface) && utf8::endsWithAny(prev.surface, {"て", "で"}) &&
      utf8::equalsAny(next.surface, {"いく", "ゆく", "くる"});
  if (conjunction_before_hiragana_de || conjunction_shite_before_iru || closed_te_form_before_directional_aux) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }
}

void addLexicalBoundaryRetentionRules(const core::LatticeEdge& prev, const core::LatticeEdge& next, float& bonus) {
  // Dictionary compound adverbs and formal-noun constructions retain their
  // lexical boundary, unlike the corresponding accidental short-token splits.
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase && next.pos == core::PartOfSpeech::Adverb &&
      next.fromDictionary() && grammar::containsKanji(next.surface) && utf8::contains(next.surface, "の")) {
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
}

// Particles after a clause already closed by a final particle or a finite predicate.
void addClosedClauseParticleRules(const core::LatticeEdge& prev, const core::LatticeEdge& next, float& bonus) {
  // Only quotative と can follow a sentence-final particle as a case phrase.
  const bool case_after_final_particle = prev.extended_pos == core::ExtendedPOS::ParticleFinal &&
                                         next.extended_pos == core::ExtendedPOS::ParticleCase &&
                                         !grammar::isSingleHiragana(next.surface, core::hiragana::kTo);
  // Nor can a binding particle follow a finite predicate that closed its
  // clause. The copula's bonus before one is for its continuative で (でこそ,
  // でさえ), not the terminal だ; the binding ぞ takes a nominal or a
  // continuative (花+ぞ, 散り+ぞ+する), so after a finite form it is the final
  // particle (そうだ+ぞい, 行く+ぞい). しか after a terminal verb stays
  // (する+しか+ない).
  const bool finite_predicate = prev.extended_pos == core::ExtendedPOS::VerbShuushikei ||
                                prev.extended_pos == core::ExtendedPOS::AdjBasic ||
                                prev.extended_pos == core::ExtendedPOS::AuxTenseTa;
  const bool binding_after_finite =
      next.extended_pos == core::ExtendedPOS::ParticleBinding &&
      ((prev.extended_pos == core::ExtendedPOS::AuxCopulaDa && grammar::isSingleHiragana(prev.surface, U'だ')) ||
       (finite_predicate && grammar::isSingleHiragana(next.surface, U'ぞ')));
  if (case_after_final_particle || binding_after_finite) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }
}

// Keep the colloquial sa-row contract, duration-counter predicate, and
// generated past-marked noun guards separate from ordinary lexical edges.
void addGeneratedEdgeGuardRules(const core::LatticeEdge& prev, const core::LatticeEdge& next, float& bonus) {
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
  // A formal noun built on a demonstrative (そのもの, このもの) is a complete noun
  // phrase, which an adverb may follow like any other (そのもの+ずばり).
  const bool demonstrative_formal_noun_before_adverb =
      prev.extended_pos == core::ExtendedPOS::NounFormal && next.pos == core::PartOfSpeech::Adverb &&
      utf8::startsWithAny(prev.surface, {"その", "この", "あの", "どの"});
  if (colloquial_sa_row_boundary || generated_nominal_before_particle || demonstrative_formal_noun_before_adverb) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }
  if (next.pos == core::PartOfSpeech::Conjunction && prev.pos == core::PartOfSpeech::Noun &&
      prev.origin == core::CandidateOrigin::KanjiHiraganaCompound &&
      grammar::isPastMarkerTaDaSurface(utf8::lastChar(prev.surface))) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }
}

}  // namespace

float computeLateLexicalBoundaryBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  float bonus{};
  addImpossibleAttachmentRules(prev, next, bonus);

  // Dictionary-backed i-adjectives form a reliable nominal predicate boundary.
  if (prev.pos == core::PartOfSpeech::Noun && next.extended_pos == core::ExtendedPOS::AdjBasic &&
      next.fromDictionary()) {
    SUZUME_CONNECTION_ADD(bonus, cost::kModerateBonus);
  }

  addBarredHostRules(prev, next, bonus);
  addLexicalBoundaryRetentionRules(prev, next, bonus);
  addClosedClauseParticleRules(prev, next, bonus);
  addGeneratedEdgeGuardRules(prev, next, bonus);
  return bonus;
}

}  // namespace suzume::analysis::connection_rules
