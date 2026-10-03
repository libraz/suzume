#ifndef SUZUME_POSTPROCESS_POSTPROCESSOR_RESOLVERS_INTERNAL_H_
#define SUZUME_POSTPROCESS_POSTPROCESSOR_RESOLVERS_INTERNAL_H_

#include <string>
#include <string_view>
#include <vector>

#include "core/morpheme.h"

namespace suzume::postprocess::resolver {

void retag(core::Morpheme& morpheme, core::PartOfSpeech pos, core::ExtendedPOS extended_pos, std::string_view lemma,
           dictionary::ConjugationType conj_type, grammar::ConjForm conj_form);

bool isCompoundRenyokeiShape(const std::string& surface);
bool isCounterDurationNoun(const std::string& surface);
bool followsTeFormConnective(const core::Morpheme& morpheme);
bool isVerbalPredicateBeforeSou(const core::Morpheme& morpheme);
bool isNominalForcingParticle(const core::Morpheme& next);
bool isPredicativeCopula(const core::Morpheme& morpheme);
void mergeInto(core::Morpheme& head, const core::Morpheme& tail);
void mergeSplitCopularNegative(std::vector<core::Morpheme>& result);
void mergeSplitFormalNounNegativeRenyokei(std::vector<core::Morpheme>& result);
void resolveAdjectivalStemBeforeGaru(std::vector<core::Morpheme>& result);
void resolveAdjectiveNominalizerSa(std::vector<core::Morpheme>& result);
void resolveAttributiveDeverbalNoun(std::vector<core::Morpheme>& result,
                                    const dictionary::DictionaryManager* dict_manager);
void resolveAdverbExplanatoryCopula(std::vector<core::Morpheme>& result);
void resolveAmbiguousInflections(std::vector<core::Morpheme>& result);
void resolveAppearanceSouPredicate(std::vector<core::Morpheme>& result);
void resolveBenefactivePotential(std::vector<core::Morpheme>& result);
void resolveBindingParticleNegative(std::vector<core::Morpheme>& result);
void resolveComparisonNoun(std::vector<core::Morpheme>& result);
void resolveCompoundAdjectiveRenyokei(std::vector<core::Morpheme>& result);
void resolveDeverbalStemBeforeDependentAuxiliary(std::vector<core::Morpheme>& result);
void resolveQuotativeParticleRoles(std::vector<core::Morpheme>& result);
void resolveCopularAro(std::vector<core::Morpheme>& result);
void resolveCopularForms(std::vector<core::Morpheme>& result);
void resolveDemonstrativeMiseru(std::vector<core::Morpheme>& result);
void resolveDemonstrativeQuotativeOnbin(std::vector<core::Morpheme>& result);
void resolveDurationPredicateKakaru(std::vector<core::Morpheme>& result);
void resolveDeverbalNominalSuffix(std::vector<core::Morpheme>& result);
void resolveGozaruPoliteAuxiliary(std::vector<core::Morpheme>& result);
void resolveHonorificVerbInflection(std::vector<core::Morpheme>& result);
void resolveIndefiniteCaseDe(std::vector<core::Morpheme>& result);
void resolveIndefiniteExistentialIru(std::vector<core::Morpheme>& result);
void resolveInitialInabilityVerb(std::vector<core::Morpheme>& result);
void resolveInitialNegativeAdjective(std::vector<core::Morpheme>& result);
void resolveKuruwaPoliteAru(std::vector<core::Morpheme>& result);
void resolveNegativeAppearanceChain(std::vector<core::Morpheme>& result);
void resolveNegativeRenyokei(std::vector<core::Morpheme>& result);
void resolveNominalCaseDe(std::vector<core::Morpheme>& result);
void resolveNominalConditionalNara(std::vector<core::Morpheme>& result);
void resolveNominalDeAru(std::vector<core::Morpheme>& result);
void resolveNominalPredicateNai(std::vector<core::Morpheme>& result);
void resolveNominalizedRenyokeiPredicate(std::vector<core::Morpheme>& result);
void resolveObligationNaranai(std::vector<core::Morpheme>& result);
void resolveParticleAruOnbin(std::vector<core::Morpheme>& result);
void resolvePoliteSuruRenyokei(std::vector<core::Morpheme>& result);
void resolvePreparatoryVolitional(std::vector<core::Morpheme>& result);
void resolveProgressiveContractionNominalizer(std::vector<core::Morpheme>& result);
void resolveProgressiveIru(std::vector<core::Morpheme>& result);
void resolveDependentVerbHomographs(std::vector<core::Morpheme>& result);
void resolveClosedInflectionalChains(std::vector<core::Morpheme>& result);
void resolveSahenRenyokei(std::vector<core::Morpheme>& result);
void splitFormalNounCopularDemo(std::vector<core::Morpheme>& result);
void resolveSimilitudeYou(std::vector<core::Morpheme>& result);
void resolveTearuAuxiliary(std::vector<core::Morpheme>& result);
void resolveEchoedConditionalCopula(std::vector<core::Morpheme>& result);
void resolveParticleHostedDearu(std::vector<core::Morpheme>& result);
void resolveTendencySuffixCopula(std::vector<core::Morpheme>& result);
void resolveVerbTeParticle(std::vector<core::Morpheme>& result);
void retagAdverbialSou(core::Morpheme& morpheme);
void retagAppearanceSou(core::Morpheme& morpheme);
void retagBasicNegativeAdjective(core::Morpheme& morpheme);

/**
 * @brief Retag a negative predicate as the independent adjective, in its own cell.
 *
 * Like retagBasicNegativeAdjective, but keeps the inflection the surface spells
 * (ない → 終止形, なかっ → 連用形, なけれ → 仮定形) so a nominal host resolves the
 * whole paradigm and not just the base form.
 */
void retagNegativeAdjectiveCell(core::Morpheme& morpheme);
void retagCopulaDa(core::Morpheme& morpheme);
bool retagGodanRenyokeiFromIRow(core::Morpheme& stem, bool set_conj_form);
void retagNaAdjectivalSou(core::Morpheme& morpheme);
void retagNegativeNai(core::Morpheme& morpheme);
void retagNounSurface(core::Morpheme& morpheme);

}  // namespace suzume::postprocess::resolver

#endif  // SUZUME_POSTPROCESS_POSTPROCESSOR_RESOLVERS_INTERNAL_H_
