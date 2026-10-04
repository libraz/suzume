#include <string_view>

#include "core/debug.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "grammar/inflection_scorer_constants.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "postprocess/postprocessor.h"
#include "postprocess/postprocessor_resolvers_internal.h"

namespace suzume::postprocess {

// These stages deliberately preserve the resolver order in process().  The
// second stage follows PREFIX+VERB nominalization because its rules inspect
// the resulting noun/verb category.
void resolvePrePrefixMorphemeRoles(std::vector<core::Morpheme>& result,
                                   const dictionary::DictionaryManager* dict_manager) {
  resolver::resolveDeverbalStemBeforeDependentAuxiliary(result);
  resolver::resolveQuotativeParticleRoles(result);
  resolver::resolveAmbiguousInflections(result);

  // A small closed class of kanji+i surfaces is an Ichidan continuative stem
  // (強いる, 用いる, 率いる, ...), while some members are also i-adjectives.
  // The passive auxiliary makes the verbal reading unambiguous.
  for (size_t idx = 0; idx + 1 < result.size(); ++idx) {
    auto& morpheme = result[idx];
    if (morpheme.pos == core::PartOfSpeech::Adjective &&
        grammar::inflection::isValidKanjiIStemException(morpheme.surface) &&
        result[idx + 1].extended_pos == core::ExtendedPOS::AuxPassive) {
      resolver::retag(morpheme, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, morpheme.surface + "る",
                      dictionary::ConjugationType::Ichidan, grammar::ConjForm::Renyokei);
    }
  }

  resolver::resolveNegativeAppearanceChain(result);
  resolver::resolveAdjectivalStemBeforeGaru(result);
  resolver::resolveAdjectiveNominalizerSa(result);
  resolver::resolveAppearanceSouPredicate(result);
  resolver::resolveProgressiveContractionNominalizer(result);
  resolver::resolveNominalPredicateNai(result);
  resolver::mergeSplitCopularNegative(result);
  resolver::mergeSplitFormalNounNegativeRenyokei(result);
  resolver::splitFormalNounCopularDemo(result);
  resolver::resolveInitialNegativeAdjective(result);
  resolver::resolveHonorificVerbInflection(result);
  resolver::resolvePreparatoryVolitional(result);
  resolver::resolveAttributiveDeverbalNoun(result, dict_manager);
  resolver::resolveCopularForms(result);
  resolver::resolveGozaruPoliteAuxiliary(result);
  resolver::resolveNominalCaseDe(result);
  resolver::resolveCopularForms(result);
  resolver::resolveTendencySuffixCopula(result);
  resolver::resolveCopularAro(result);
  resolver::resolveNominalDeAru(result);
  resolver::resolveComparisonNoun(result);
  resolver::resolveNegativeRenyokei(result);
  resolver::resolveVerbTeParticle(result);
  resolver::resolveObligationNaranai(result);
  resolver::resolveTearuAuxiliary(result);
  resolver::resolveEchoedConditionalCopula(result);
  resolver::resolveParticleHostedDearu(result);
  resolver::resolveKuruwaPoliteAru(result);

  // At sentence start, なり+ける is the continuative form of lexical なる
  // followed by the classical past auxiliary.  The copular なり reading is
  // retained when it follows a nominal predicate (春なりける).
  if (result.size() >= 2 && utf8::equalsAny(result[0].surface, {"なり"}) &&
      result[0].extended_pos == core::ExtendedPOS::AuxClassicalNari &&
      result[1].extended_pos == core::ExtendedPOS::AuxClassicalKeri && utf8::equalsAny(result[1].surface, {"ける"})) {
    resolver::retag(result[0], core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "なる",
                    dictionary::ConjugationType::GodanRa, grammar::ConjForm::Renyokei);
  }

  resolver::resolveParticleAruOnbin(result);
  resolver::resolveDemonstrativeMiseru(result);
  resolver::resolveBenefactivePotential(result);
  resolver::resolveBindingParticleNegative(result);
  resolver::resolveInitialInabilityVerb(result);
  resolver::resolveDependentVerbHomographs(result);
  resolver::resolveProgressiveIru(result);
  resolver::resolveSahenRenyokei(result);
  resolver::resolveDemonstrativeQuotativeOnbin(result);
  resolver::resolvePoliteSuruRenyokei(result);
  resolver::resolveIndefiniteCaseDe(result);
  resolver::resolveIndefiniteExistentialIru(result);
  resolver::resolveNominalizedRenyokeiPredicate(result);
}

void resolvePostPrefixMorphemeRoles(std::vector<core::Morpheme>& result) {
  resolver::resolveCompoundAdjectiveRenyokei(result);
  resolver::resolveAdverbExplanatoryCopula(result);
  resolver::resolveSimilitudeYou(result);
  resolver::resolveNominalConditionalNara(result);

  for (size_t idx = 0; idx + 1 < result.size(); ++idx) {
    if (result[idx].surface == "付け" && result[idx].pos == core::PartOfSpeech::Verb &&
        result[idx + 1].surface == "で" && result[idx + 1].pos == core::PartOfSpeech::Particle) {
      resolver::retagNounSurface(result[idx]);
    }
  }
  for (size_t idx = 1; idx + 1 < result.size(); ++idx) {
    if (result[idx - 1].surface == "あり" && result[idx].surface == "ん" && result[idx + 1].surface == "す") {
      auto& suru = result[idx + 1];
      resolver::retag(suru, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbShuushikei, "する",
                      dictionary::ConjugationType::Suru, grammar::ConjForm::Base);
    }
  }

  // Temporal 後 after a counter/duration quantity is a suffix, not a standalone
  // noun (2時間+後, 10日+後, 5分+後 → 名詞,接尾). Runs after numeric merging so the
  // quantity token is final. 前 is never a suffix in MeCab, so this is 後-only.
  for (size_t idx = 1; idx < result.size(); ++idx) {
    if (result[idx].surface == "後" && result[idx].pos == core::PartOfSpeech::Noun &&
        result[idx - 1].pos == core::PartOfSpeech::Noun && resolver::isCounterDurationNoun(result[idx - 1].surface)) {
      result[idx].pos = core::PartOfSpeech::Suffix;
      result[idx].extended_pos = core::ExtendedPOS::Suffix;
      result[idx].lemma = "後";
    }
  }

  // Kanji 過ぎ/過ぎる directly after a verb 連用形 is the excessive subsidiary,
  // which keeps the verb POS of its kana spelling (食べ+過ぎ like 食べ+すぎ).
  // Repairs the bare-renyokei fake godan reading 過ぐ → 過ぎる.
  for (size_t idx = 1; idx < result.size(); ++idx) {
    if (utf8::equalsAny(result[idx].surface, {"過ぎ", "過ぎる"}) && result[idx].pos == core::PartOfSpeech::Verb &&
        result[idx - 1].pos == core::PartOfSpeech::Verb &&
        result[idx - 1].extended_pos == core::ExtendedPOS::VerbRenyokei) {
      result[idx].extended_pos = core::ExtendedPOS::AuxExcessive;
      result[idx].lemma = "過ぎる";
    }
  }

  // A verb continuative directly followed by the negative form ない requires
  // the auxiliary reading. Keep the selected verb lemma: a compound
  // continuative such as 差し支え must retain its dictionary form 差し支える.
  // The irrealis-plus-negative construction already selects the auxiliary and
  // therefore does not reach this recovery step.
  for (size_t idx = 1; idx < result.size(); ++idx) {
    if (result[idx].surface == "ない" && result[idx].pos == core::PartOfSpeech::Adjective &&
        result[idx - 1].pos == core::PartOfSpeech::Verb &&
        result[idx - 1].extended_pos == core::ExtendedPOS::VerbRenyokei &&
        result[idx - 1].conj_type != dictionary::ConjugationType::Suru) {
      result[idx].pos = core::PartOfSpeech::Auxiliary;
      result[idx].extended_pos = core::ExtendedPOS::AuxNegativeNai;
      result[idx].lemma = "ない";
    }
  }

  // A continuative used as the head of a nominal phrase is a 連用形転成名詞,
  // not a verb: 答え**は**, 笑い**で**, 読み**だけ**.  A direct argument or
  // focus marker is structural evidence for that role, irrespective of whether
  // the preceding verb is simplex or compound.  The purpose construction
  // V連用形+に+V is deliberately exempt: its following predicate keeps the
  // continuative verbal.
  // Runs after lemmatizeAll, so overwriting lemma = surface is final.  A preceding
  // continuative can also be the first item in a parallel deverbal-noun sequence
  // (上がり+下がりを); require the next item to have already been selected as
  // NounVerbal and to be case/topic marked before extending the same retag.
  for (size_t idx = 0; idx < result.size(); ++idx) {
    const bool direct_nominal_particle = idx + 1 < result.size() &&
                                         result[idx + 1].pos == core::PartOfSpeech::Particle &&
                                         (resolver::isNominalForcingParticle(result[idx + 1]) ||
                                          result[idx + 1].extended_pos == core::ExtendedPOS::ParticleAdverbial ||
                                          grammar::isSingleHiragana(result[idx + 1].surface, U'で'));
    const bool purpose_construction = direct_nominal_particle &&
                                      grammar::isSingleHiragana(result[idx + 1].surface, U'に') &&
                                      idx + 2 < result.size() && result[idx + 2].pos == core::PartOfSpeech::Verb;
    const bool direct_nominal_context = direct_nominal_particle && !purpose_construction;
    const bool compound_sentence_final =
        idx + 1 == result.size() && resolver::isCompoundRenyokeiShape(result[idx].surface);
    const bool parallel_nominal_context = idx + 2 < result.size() && result[idx + 1].pos == core::PartOfSpeech::Noun &&
                                          result[idx + 1].extended_pos == core::ExtendedPOS::NounVerbal &&
                                          normalize::utf8Length(result[idx].surface) >= 3 &&
                                          normalize::utf8Length(result[idx + 1].surface) >= 3 &&
                                          resolver::isNominalForcingParticle(result[idx + 2]);
    if (result[idx].pos == core::PartOfSpeech::Verb && result[idx].extended_pos == core::ExtendedPOS::VerbRenyokei &&
        (direct_nominal_context || compound_sentence_final || parallel_nominal_context)) {
      resolver::retagUninflected(result[idx], core::PartOfSpeech::Noun, core::ExtendedPOS::NounVerbal,
                                 result[idx].surface);
    }
  }
}

// These repairs run only after merges and filtering have finalized the local
// token context.  Moving them earlier would change the grammatical evidence
// each resolver observes.
void resolveFinalMorphemeRoles(std::vector<core::Morpheme>& result, const dictionary::DictionaryManager* dict_manager) {
  resolver::resolveDurationPredicateKakaru(result);
  resolver::resolveDeverbalNominalSuffix(result);
  resolver::resolveClosedInflectionalChains(result);

  // A dictionary-confirmed na-adjective keeps its adjectival role before a
  // predicative copula or the productive excessive auxiliary. The lattice can
  // otherwise prefer a homographic unknown noun/verb when ん+だ resembles an
  // explanatory chain, or when material after すぎる changes its path
  // (盛ん+だ, 複雑+すぎる+の+です).
  for (size_t idx = 0; idx + 1 < result.size(); ++idx) {
    auto& stem = result[idx];
    auto& follower = result[idx + 1];
    const bool copula_follows = stem.pos == core::PartOfSpeech::Verb && utf8::equalsAny(follower.surface, {"だ"});
    const bool suffix_ge_follows =
        follower.pos == core::PartOfSpeech::Suffix && utf8::equalsAny(follower.surface, {"げ"});
    const bool excessive_follows = utf8::startsWith(follower.surface, "すぎ");
    if (!copula_follows && !suffix_ge_follows && !excessive_follows) {
      continue;
    }
    const auto* adjective =
        dict_manager != nullptr ? dict_manager->lookupExact(stem.surface, core::PartOfSpeech::Adjective) : nullptr;
    if (adjective == nullptr || adjective->extended_pos != core::ExtendedPOS::AdjNaAdj) {
      continue;
    }
    if (copula_follows) {
      resolver::retagNaAdjectiveSurface(stem);
      resolver::retagCopulaDa(follower);
      continue;
    }
    if (suffix_ge_follows) {
      resolver::retagNounSurface(stem);
      continue;
    }
    resolver::retagNaAdjectiveSurface(stem);
    follower.pos = core::PartOfSpeech::Verb;
    follower.extended_pos = core::ExtendedPOS::AuxExcessive;
    follower.lemma = "すぎる";
  }

  // A pure-hiragana n-onbin V2 can carry inflection evidence that contradicts
  // an overlapping closed V2 reading (折り+たたん, not the fabricated
  // 折りたつ). Direct adjacency to V1 licenses the compound search unit
  // without registering that open-class V2 in the closed table.
  for (size_t idx = 0; idx + 1 < result.size();) {
    auto& v1 = result[idx];
    const auto& v2 = result[idx + 1];
    if (v1.pos != core::PartOfSpeech::Verb || v1.extended_pos != core::ExtendedPOS::VerbRenyokei ||
        v2.pos != core::PartOfSpeech::Verb || v2.extended_pos != core::ExtendedPOS::VerbOnbinkei ||
        !grammar::isPureHiragana(v2.surface) || !utf8::endsWith(v2.surface, "ん") ||
        !utf8::endsWithAny(v2.lemma, {"む", "ぶ", "ぬ"}) || v1.end != v2.start) {
      ++idx;
      continue;
    }
    const std::string v1_surface = v1.surface;
    resolver::mergeInto(v1, v2);
    v1.lemma = v1_surface + v2.lemma;
    v1.pos = v2.pos;
    v1.extended_pos = v2.extended_pos;
    v1.conj_type = v2.conj_type;
    v1.conj_form = v2.conj_form;
    resolver::eraseAfter(result, idx);
  }

  // A continuative between a period-end noun and the closed following-period
  // modifier is a deverbal schedule noun (月末+締め+翌月).  Both anchors are
  // grammatical/temporal classes, so arbitrary open-class continuatives are
  // handled without registering individual payment terms.
  for (size_t idx = 1; idx + 1 < result.size(); ++idx) {
    auto& current = result[idx];
    if (result[idx - 1].pos == core::PartOfSpeech::Noun && utf8::endsWith(result[idx - 1].surface, "末") &&
        current.pos == core::PartOfSpeech::Verb && current.extended_pos == core::ExtendedPOS::VerbRenyokei &&
        result[idx + 1].pos == core::PartOfSpeech::Noun && utf8::startsWith(result[idx + 1].surface, "翌")) {
      resolver::retagNounSurface(current);
    }
  }

  // After the connective て/で, an aspect verb is the productive subsidiary
  // (進んで+いく, 読んで+いれば, 読んで+おいた), not an independent predicate.
  // The lattice intentionally keeps its verbal inflection shape; the closed
  // local connection supplies the public auxiliary role without host words.
  // Match on the lemma so every inflected form takes that role — the terminal
  // form must not be the only one that reads as an auxiliary.
  struct TeFormSubsidiary {
    std::string_view lemma;
    core::ExtendedPOS extended_pos;
  };
  static const TeFormSubsidiary kTeFormSubsidiaries[] = {
      {"いく", core::ExtendedPOS::AuxAspectIku},
      {"行く", core::ExtendedPOS::AuxAspectIku},
      {"いる", core::ExtendedPOS::AuxAspectIru},
      {"おく", core::ExtendedPOS::AuxAspectOku},
  };
  for (size_t idx = 1; idx < result.size(); ++idx) {
    auto& previous = result[idx - 1];
    auto& current = result[idx];
    if (!resolver::followsTeFormConnective(previous) || current.pos != core::PartOfSpeech::Verb) {
      continue;
    }
    for (const auto& subsidiary : kTeFormSubsidiaries) {
      if (current.lemma == subsidiary.lemma) {
        current.pos = core::PartOfSpeech::Auxiliary;
        current.extended_pos = subsidiary.extended_pos;
        break;
      }
    }
  }

  // Conditional たら/だら is the inflected past auxiliary after a predicate,
  // even when its homographic particle or unknown edge wins before a following
  // clause (泣い+たら+子供が..., 読ん+だら+あかん).
  for (size_t idx = 1; idx < result.size(); ++idx) {
    auto& previous = result[idx - 1];
    auto& current = result[idx];
    const bool unresolved_conditional =
        current.pos == core::PartOfSpeech::Particle || current.pos == core::PartOfSpeech::Other;
    if (previous.pos == core::PartOfSpeech::Verb && unresolved_conditional &&
        utf8::equalsAny(current.surface, {"たら", "だら"})) {
      current.pos = core::PartOfSpeech::Auxiliary;
      current.extended_pos = core::ExtendedPOS::AuxTenseTa;
      current.lemma = current.surface == "だら" ? "だ" : "た";
    }
  }

  // Resolve productive homographs from their closed grammatical follower.
  // These are inflectional patterns, not word lists: 形容詞語幹+げ/すぎる,
  // 形容詞仮定形+ば, and nominal+的+な are locally unambiguous.
  for (size_t idx = 0; idx + 1 < result.size(); ++idx) {
    auto& current = result[idx];
    auto& next = result[idx + 1];
    // A finite i-adjective modifies the independent temporal noun 間
    // (長い+間, 短い+間). The suffix reading is reserved for direct nominal
    // duration attachment and cannot follow an attributive predicate.
    if (current.extended_pos == core::ExtendedPOS::AdjBasic && next.surface == "間" &&
        next.pos == core::PartOfSpeech::Suffix) {
      resolver::retagNounSurface(next);
    }
    // A lexical adverb homograph followed by accusative を is in nominal use
    // (一切を). Genitive の is not included: Japanese also permits fixed
    // adverbial expressions such as まったくの and いつかの.
    if (current.pos == core::PartOfSpeech::Adverb && next.pos == core::PartOfSpeech::Particle && next.surface == "を") {
      resolver::retagNounSurface(current);
    }
    // A bare na-adjective stem cannot directly take an argument particle as
    // an adjective.  In this closed syntactic context it is the nominal use
    // (平静を保つ, 安全が第一, 健康のため), while adjectival uses retain な/に/だ.
    if (current.pos == core::PartOfSpeech::Adjective && current.extended_pos == core::ExtendedPOS::AdjNaAdj &&
        next.pos == core::PartOfSpeech::Particle && utf8::equalsAny(next.surface, {"を", "が", "は", "も", "の"})) {
      resolver::retagNounSurface(current);
    }
    // A non-lexical noun candidate ending in い cannot take conjectural だろ
    // as an attributive noun marker. The closed follower licenses the finite
    // i-adjective reading (遠い+だろ+う) without touching lexical nouns.
    if (!current.fromDictionary() && current.pos == core::PartOfSpeech::Noun && utf8::endsWith(current.surface, "い") &&
        next.extended_pos == core::ExtendedPOS::AuxCopulaDa && next.surface == "だろ") {
      resolver::retag(current, core::PartOfSpeech::Adjective, core::ExtendedPOS::AdjBasic, current.surface,
                      dictionary::ConjugationType::IAdjective, grammar::ConjForm::Base);
    }
    if (next.surface == "なき" && current.pos == core::PartOfSpeech::Adjective) {
      resolver::retagNounSurface(current);
    }
    // Once Viterbi has selected 的 as an independent token after a noun, its
    // role is the productive derivational suffix.  Whole words containing 的
    // are untouched because they have no separate boundary.
    if (current.pos == core::PartOfSpeech::Noun && next.pos == core::PartOfSpeech::Noun && next.surface == "的") {
      resolver::retagUninflected(next, core::PartOfSpeech::Suffix, core::ExtendedPOS::Suffix, "的");
    }
    if (current.pos != core::PartOfSpeech::Suffix && utf8::endsWith(current.surface, "的") && next.surface == "な") {
      resolver::retagNaAdjectiveSurface(current);
      resolver::retagCopulaDa(next);
    }
    // A predicate selected by attributive copular な before a noun is a
    // na-adjective stem, even when its dictionary homograph is an adverb
    // (さすが+な+人, 散々+な+結果): an adverb takes no copula.
    if (idx + 2 < result.size() && current.pos == core::PartOfSpeech::Adverb && next.surface == "な" &&
        next.extended_pos == core::ExtendedPOS::AuxCopulaDa &&
        (result[idx + 2].pos == core::PartOfSpeech::Noun ||
         result[idx + 2].extended_pos == core::ExtendedPOS::NounFormal)) {
      resolver::retagNaAdjectiveSurface(current);
    }
    // A kana く-continuative directly modifying another adjective takes the
    // lexical adverb reading, since kana cannot tell 甚く from 痛く; a kanji
    // spelling is the adjective's own continuative (恐ろしく+高い). The
    // independent negative adjective remains a conjugational continuation
    // (高く+なくて), not a degree predicate.
    if (current.pos == core::PartOfSpeech::Adjective && current.extended_pos == core::ExtendedPOS::AdjRenyokei &&
        utf8::endsWith(current.surface, "く") && !grammar::containsKanji(current.surface) &&
        next.pos == core::PartOfSpeech::Adjective && next.lemma != "ない") {
      resolver::retagUninflected(current, core::PartOfSpeech::Adverb, core::ExtendedPOS::Adverb, current.surface);
    }
  }

  // A focused continuative in V-連用形+は+する remains verbal (減りはしない),
  // unlike a deverbal noun independently marked by は.  Reconstruct the Godan
  // base from the i-row ending instead of registering open-class verbs.
  for (size_t idx = 1; idx + 2 < result.size(); ++idx) {
    auto& stem = result[idx];
    const auto& focus = result[idx + 1];
    const auto& suru = result[idx + 2];
    const auto& left_context = result[idx - 1];
    if (stem.pos == core::PartOfSpeech::Noun && !stem.fromDictionary() && grammar::containsKanji(stem.surface) &&
        left_context.pos == core::PartOfSpeech::Particle && focus.surface == "は" &&
        focus.pos == core::PartOfSpeech::Particle && suru.pos == core::PartOfSpeech::Verb && suru.lemma == "する") {
      resolver::retagGodanRenyokeiFromIRow(stem, true);
    }
  }

  // 本 is the closed demonstrative prefix before a katakana product/service
  // head.  The rule depends on the following script class, not on product
  // names, so arbitrary open-class heads remain supported.
  if (result.size() >= 2 && result[0].surface == "本" && result[0].pos == core::PartOfSpeech::Noun &&
      result[1].pos == core::PartOfSpeech::Noun && grammar::isPureKatakana(result[1].surface)) {
    resolver::retagUninflected(result[0], core::PartOfSpeech::Prefix, core::ExtendedPOS::Prefix, "本");
  }

  // The registered formal noun 他 carries its kana lemma in the productive
  // adnominal frame 他+の, even if an unknown noun edge won the lattice.
  for (size_t idx = 0; idx + 1 < result.size(); ++idx) {
    if (result[idx].surface == "他" && result[idx].pos == core::PartOfSpeech::Noun && result[idx + 1].surface == "の") {
      result[idx].lemma = "ほか";
      result[idx].extended_pos = core::ExtendedPOS::NounFormal;
    }
  }

  // Sentence-initial demonstratives are also interjections, but before a
  // nominal head only the attributive reading is grammatical.
  if (result.size() >= 2 && result[0].pos == core::PartOfSpeech::Interjection &&
      utf8::equalsAny(result[0].surface, {"あの", "その"}) &&
      (result[1].pos == core::PartOfSpeech::Noun || result[1].isFormalNoun() || result[0].surface == "その")) {
    resolver::retagUninflected(result[0], core::PartOfSpeech::Determiner, core::ExtendedPOS::Determiner,
                               result[0].surface);
  }

  for (size_t idx = 0; idx < result.size(); ++idx) {
    auto& current = result[idx];
    const bool has_auxiliary_host =
        idx == 0 || result[idx - 1].pos == core::PartOfSpeech::Noun ||
        result[idx - 1].pos == core::PartOfSpeech::Verb || result[idx - 1].pos == core::PartOfSpeech::Adjective ||
        result[idx - 1].pos == core::PartOfSpeech::Auxiliary || result[idx - 1].pos == core::PartOfSpeech::Adverb ||
        result[idx - 1].pos == core::PartOfSpeech::Conjunction;
    if (current.surface == "らしく" && has_auxiliary_host) {
      resolver::retag(current, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxConjectureRashii, "らしい",
                      dictionary::ConjugationType::IAdjective, grammar::ConjForm::Renyokei);
    }
  }

  for (size_t idx = 1; idx < result.size(); ++idx) {
    auto& current = result[idx];
    const auto& previous = result[idx - 1];
    // In the temporal chain いま+なお, なお means "still" and is adverbial,
    // not the clause-linking conjunction used sentence-initially.
    if (previous.pos == core::PartOfSpeech::Adverb && current.pos == core::PartOfSpeech::Conjunction &&
        current.surface == "なお") {
      resolver::retagUninflected(current, core::PartOfSpeech::Adverb, core::ExtendedPOS::Adverb, "なお");
    }
    if (current.surface == "たく" && current.lemma == "たい" && current.pos == core::PartOfSpeech::Adjective &&
        (previous.pos == core::PartOfSpeech::Verb || previous.pos == core::PartOfSpeech::Auxiliary)) {
      resolver::retag(current, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxDesireTai, "たい",
                      dictionary::ConjugationType::IAdjective, grammar::ConjForm::Renyokei);
    }
    if (current.surface == "より" && previous.pos == core::PartOfSpeech::Verb) {
      resolver::retagUninflected(current, core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleCase, "より");
    }
    if (current.surface == "が" && previous.surface == "まで") {
      resolver::retagUninflected(current, core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleCase, "が");
    }
    if (current.surface == "て" && previous.surface == "と") {
      resolver::retagUninflected(current, core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleConj, "て");
    }
    if ((current.surface == "はじめ" || current.surface == "そこね") &&
        previous.extended_pos == core::ExtendedPOS::VerbRenyokei) {
      const auto epos =
          current.surface == "はじめ" ? core::ExtendedPOS::AuxAspectHajimeru : core::ExtendedPOS::AuxInability;
      // The inceptive keeps the verb category its kanji spelling has (始め).
      const auto pos = current.surface == "はじめ" ? core::PartOfSpeech::Verb : core::PartOfSpeech::Auxiliary;
      resolver::retag(current, pos, epos, current.surface + "る", dictionary::ConjugationType::Ichidan,
                      grammar::ConjForm::Renyokei);
    }
    if (current.surface == "な" && previous.pos == core::PartOfSpeech::Adjective &&
        (previous.conj_type == dictionary::ConjugationType::NaAdjective ||
         previous.extended_pos == core::ExtendedPOS::AdjNaAdj)) {
      resolver::retagCopulaDa(current);
    }
  }

  if (!result.empty() && result[0].surface == "より") {
    resolver::retagUninflected(result[0], core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleCase, "より");
  }

  if (!result.empty() && result[0].surface == "何ら") {
    resolver::retagUninflected(result[0], core::PartOfSpeech::Adverb, core::ExtendedPOS::Adverb, "何ら");
  }

  // An i-adjective renyokei keeps its lexical lemma before the independent
  // negative auxiliary (明るく+なかっ), rather than acquiring a synthetic
  // compound lemma 明るくない.
  for (size_t idx = 0; idx + 1 < result.size(); ++idx) {
    auto& current = result[idx];
    if (current.pos == core::PartOfSpeech::Adjective && utf8::endsWith(current.surface, "く") &&
        result[idx + 1].surface == "なかっ") {
      current.lemma = normalize::concat(utf8::dropLastChar(current.surface), "い");
      current.extended_pos = core::ExtendedPOS::AdjRenyokei;
      current.conj_type = dictionary::ConjugationType::IAdjective;
      current.conj_form = grammar::ConjForm::Renyokei;
    }
    if (current.pos == core::PartOfSpeech::Verb && grammar::endsWithERow(current.surface) &&
        result[idx + 1].extended_pos == core::ExtendedPOS::AuxNegativeNai) {
      current.lemma = current.surface + "る";
      current.conj_type = dictionary::ConjugationType::Ichidan;
      current.conj_form = grammar::ConjForm::Mizenkei;
    }
  }

  // When an adjective renyokei is independently focused by は, the following
  // ない is the lexical negative adjective rather than a predicate-attached
  // negative auxiliary (強く+は+なかっ+た).  Preserve the past-stem
  // inflection selected for なかっ so its POS agrees with that syntax.
  for (size_t idx = 2; idx < result.size(); ++idx) {
    auto& negative = result[idx];
    const auto& focus = result[idx - 1];
    const auto& adjective = result[idx - 2];
    if (negative.surface == "なかっ" && negative.extended_pos == core::ExtendedPOS::AuxNegativeNai &&
        focus.surface == "は" && adjective.pos == core::PartOfSpeech::Adjective &&
        adjective.extended_pos == core::ExtendedPOS::AdjRenyokei) {
      resolver::retag(negative, core::PartOfSpeech::Adjective, core::ExtendedPOS::AdjKatt, "ない",
                      dictionary::ConjugationType::IAdjective, grammar::ConjForm::Renyokei);
    }
  }

  // The regional causal き is homographic with the classical past auxiliary,
  // which attaches to a continuative (あり+き). After a finite predicate — a
  // terminal form or the past auxiliary — no classical reading is available,
  // so the residual unknown token is the conjunctive particle (書く+き).
  for (size_t idx = 1; idx < result.size(); ++idx) {
    auto& causal = result[idx];
    const auto& host = result[idx - 1];
    if (causal.pos != core::PartOfSpeech::Other || !grammar::isSingleHiragana(causal.surface, U'き')) {
      continue;
    }
    if (host.extended_pos == core::ExtendedPOS::VerbShuushikei || host.extended_pos == core::ExtendedPOS::AuxTenseTa) {
      resolver::retagUninflected(causal, core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleConj, "き");
    }
  }

  // Sentence-final ね is the final particle after a completed predicate or
  // after any particle, not the homographic verb/negative auxiliary.  No
  // particle licenses a following predicate stem, so a trailing ね behind one
  // is clause-final regardless of the particle's own class (本だがね, 東京にね).
  if (result.size() >= 2 && result.back().surface == "ね") {
    auto& previous = result[result.size() - 2];
    auto& final_ne = result.back();
    // An irrealis stem is not a completed predicate: it exists only to carry an
    // auxiliary, so a trailing ね behind one is that auxiliary (飲ま+ね) rather
    // than the final particle it would be after a finite form (飲む+ね).
    const bool stacked_particles = grammar::isAmbiguousFinalParticleStackHead(previous.surface);
    const bool irrealis_host = previous.extended_pos == core::ExtendedPOS::VerbMizenkei ||
                               previous.extended_pos == core::ExtendedPOS::AdjMizenkei;
    const bool final_context =
        !irrealis_host && (stacked_particles || previous.pos == core::PartOfSpeech::Particle ||
                           previous.pos == core::PartOfSpeech::Verb || previous.pos == core::PartOfSpeech::Adjective ||
                           previous.pos == core::PartOfSpeech::Auxiliary);
    if (final_context) {
      if (stacked_particles) {
        resolver::retagUninflected(previous, core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleFinal,
                                   previous.surface);
      }
      resolver::retagUninflected(final_ne, core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleFinal, "ね");
    }
  }

  // A demonstrative identification with an omitted copula ends in a noun:
  // これが答え。  The lattice can select the homographic Ichidan continuative
  // 答え, so resolve the closed demonstrative + case-particle frame before the
  // clause-final imperative repair below.  This is deliberately narrower than
  // a generic PARTICLE+e-row rule: 君が急げ remains a possible imperative.
  if (result.size() >= 3) {
    auto& final = result.back();
    const auto& case_particle = result[result.size() - 2];
    const auto& demonstrative = result[result.size() - 3];
    const auto demonstrative_codepoints = normalize::toCodepoints(demonstrative.surface);
    const bool is_demonstrative =
        demonstrative.pos == core::PartOfSpeech::Pronoun && demonstrative_codepoints.size() >= 2 &&
        normalize::isDemonstrativeStart(demonstrative_codepoints[0], demonstrative_codepoints[1]);
    const bool is_ga_case =
        case_particle.extended_pos == core::ExtendedPOS::ParticleCase && case_particle.surface == "が";
    const bool is_bare_ichidan = final.pos == core::PartOfSpeech::Verb &&
                                 final.extended_pos == core::ExtendedPOS::VerbRenyokei &&
                                 final.lemma == final.surface + "る";
    if (is_demonstrative && is_ga_case && is_bare_ichidan) {
      resolver::retagUninflected(final, core::PartOfSpeech::Noun, core::ExtendedPOS::NounVerbal, final.surface);
    }
  }

  // A clause-final e-row verb after an argument particle can be the Godan
  // imperative (急げ→急ぐ), but an Ichidan continuative legitimately ends in
  // the same row (まとめ→まとめる). A dictionary-confirmed Godan base makes
  // the argument-particle frame an imperative; without that evidence, preserve
  // the lattice judgment rather than manufacturing a lexical lemma. This also
  // gives the imperative priority when the productive potential form is in the
  // dictionary, because the clause-final syntax is the disambiguating evidence.
  // The shared row table handles every Godan class without lexical enumeration.
  if (!result.empty()) {
    auto& final = result.back();
    const char32_t tail = utf8::decodeLastChar(final.surface);
    const std::string_view base_suffix = grammar::godanBaseSuffixFromERow(tail);
    if (dict_manager != nullptr && final.pos == core::PartOfSpeech::Verb && !base_suffix.empty() &&
        result.size() >= 2 && result[result.size() - 2].pos == core::PartOfSpeech::Particle) {
      const std::string godan_base = normalize::concat(utf8::dropLastChar(final.surface), base_suffix);
      const bool has_godan = dict_manager->lookupExact(godan_base, core::PartOfSpeech::Verb) != nullptr;
      SUZUME_DEBUG_VERBOSE_BLOCK {
        const bool has_ichidan =
            dict_manager->lookupExact(normalize::concat(final.surface, "る"), core::PartOfSpeech::Verb) != nullptr;
        SUZUME_DEBUG_STREAM << "[POSTPROC] clause-final e-row evidence: ichidan=" << has_ichidan
                            << ", godan=" << has_godan << "\n";
      }
      if (has_godan) {
        final.lemma = godan_base;
        final.extended_pos = core::ExtendedPOS::VerbMeireikei;
        final.conj_type =
            grammar::verbTypeToConjType(grammar::verbTypeFromBaseCodepoint(utf8::decodeLastChar(godan_base)));
        final.conj_form = grammar::ConjForm::Meireikei;
      }
    }
  }

  // The interrogative どう is the adverb of the こう/そう/ああ/どう series
  // whichever predicate follows (どう+なった, どう+する, どう+だ), like そう.
  for (auto& morpheme : result) {
    if (utf8::equalsAny(morpheme.surface, {"どう"}) && morpheme.pos == core::PartOfSpeech::Adjective) {
      resolver::retagUninflected(morpheme, core::PartOfSpeech::Adverb, core::ExtendedPOS::Adverb, "どう");
    }
  }
  for (size_t idx = 0; idx + 3 < result.size(); ++idx) {
    const auto& stem = result[idx];
    auto& sou = result[idx + 1];
    auto& na = result[idx + 2];
    auto& noun = result[idx + 3];
    if (stem.extended_pos != core::ExtendedPOS::AdjStem || sou.surface != "そう" || na.surface != "な" ||
        noun.pos != core::PartOfSpeech::Adjective || !grammar::containsKanji(noun.surface)) {
      continue;
    }
    resolver::retagAppearanceSou(sou);
    resolver::retagCopulaDa(na);
    resolver::retagNounSurface(noun);
  }

  // A one-kanji Ichidan stem has the same visible form before the negative
  // auxiliary. Resolve its lemma from the final token sequence, after all
  // ambiguity-specific retagging has completed.
  for (size_t idx = 0; idx + 1 < result.size(); ++idx) {
    auto& stem = result[idx];
    const auto& negative = result[idx + 1];
    if (stem.pos == core::PartOfSpeech::Verb && stem.surface.size() == core::kJapaneseCharBytes &&
        grammar::isAllKanji(stem.surface) && negative.surface == "ない") {
      stem.lemma = stem.surface + "る";
    }
  }

  // Surface たっ is ambiguous between the past auxiliary and the onbin form
  // of たつ.  The auxiliary reading requires a predicate immediately on its
  // left; after a case particle (いつまで+たっ+て, 朝から+たっ+て) it is
  // grammatically impossible, so restore the independent verb.  Predicate+
  // たって (昨日来+たっ+て) and copular だって never satisfy this gate.  The
  // one-mora stem also reaches this slot as the productive wa-row
  // reconstruction たう, which is not a word; the same gate names its base.
  for (size_t idx = 1; idx + 1 < result.size(); ++idx) {
    auto& onbin = result[idx];
    const auto& previous = result[idx - 1];
    const auto& connective = result[idx + 1];
    const bool ambiguous_ta_onbin = onbin.extended_pos == core::ExtendedPOS::AuxTenseTa ||
                                    (onbin.pos == core::PartOfSpeech::Verb && onbin.lemma == "たう");
    if (onbin.surface == "たっ" && ambiguous_ta_onbin && previous.extended_pos == core::ExtendedPOS::ParticleCase &&
        connective.surface == "て" && connective.extended_pos == core::ExtendedPOS::ParticleConj) {
      resolver::retag(onbin, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbOnbinkei, "たつ",
                      dictionary::ConjugationType::GodanTa, grammar::ConjForm::Onbinkei);
    }
  }

  // An otherwise unresolved lexical item directly quantified by a complete
  // native number phrase is the nominal head of that phrase (まばたき+
  // ひとつ).  Resolve only Other here; established adverbs, predicates, and
  // pronouns keep their own readings, and verbal まばたき+する is untouched.
  for (size_t idx = 0; idx + 1 < result.size(); ++idx) {
    auto& head = result[idx];
    const auto& quantity = result[idx + 1];
    if (head.pos == core::PartOfSpeech::Other && quantity.extended_pos == core::ExtendedPOS::NounNumber) {
      resolver::retagNounSurface(head);
    }
  }

  // A kanji-plus-て continuative before the past auxiliary or connective
  // particle is an Ichidan stem (立て+て, 棄て+た). The lattice also has a
  // homographic analysis that drops the e-row mora from the lemma; restore
  // the productive Ichidan dictionary form after contextual disambiguation.
  for (size_t idx = 0; idx + 1 < result.size(); ++idx) {
    auto& stem = result[idx];
    const auto& next = result[idx + 1];
    const bool inflection_follows =
        next.extended_pos == core::ExtendedPOS::AuxTenseTa || next.extended_pos == core::ExtendedPOS::ParticleConj;
    if (!inflection_follows || stem.pos != core::PartOfSpeech::Verb ||
        stem.extended_pos != core::ExtendedPOS::VerbRenyokei) {
      continue;
    }
    const auto codepoints = normalize::toCodepoints(stem.surface);
    const bool is_ichidan_te_stem =
        codepoints.size() == 2 && normalize::isKanjiCodepoint(codepoints.front()) && codepoints.back() == U'て';
    if (is_ichidan_te_stem) {
      stem.lemma = stem.surface + "る";
      stem.conj_type = dictionary::ConjugationType::Ichidan;
      stem.conj_form = grammar::ConjForm::Renyokei;
    }
  }

  // A suffix between the genitive particle and a case particle heads its own
  // nominal phrase (穴の中から, 月の末に). Retag it as a noun; a true bound
  // suffix instead remains directly attached to its nominal stem.
  for (size_t idx = 1; idx + 1 < result.size(); ++idx) {
    auto& candidate = result[idx];
    const auto& previous = result[idx - 1];
    const auto& next = result[idx + 1];
    if (candidate.pos == core::PartOfSpeech::Suffix &&
        (previous.extended_pos == core::ExtendedPOS::ParticleNo ||
         previous.extended_pos == core::ExtendedPOS::ParticleCase || previous.pos == core::PartOfSpeech::Particle) &&
        next.extended_pos == core::ExtendedPOS::ParticleCase) {
      candidate.pos = core::PartOfSpeech::Noun;
      candidate.extended_pos = core::ExtendedPOS::Noun;
      candidate.conj_type = dictionary::ConjugationType::None;
      candidate.conj_form = grammar::ConjForm::Base;
    }
  }
}

}  // namespace suzume::postprocess
