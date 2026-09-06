#include <string_view>
#include <utility>

#include "core/debug.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "grammar/honorific_verbs.h"
#include "grammar/inflection_scorer_constants.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "postprocess/postprocessor.h"
#include "postprocess/postprocessor_resolvers_internal.h"

namespace suzume::postprocess::resolver {

// An Ichidan stem is surface-identical in 未然形 and 連用形, and its lattice
// noun homograph can win before a short dependent auxiliary.  The follower
// resolves the category without lexical enumeration: 食べ+ん is negative,
// while 食べ+とく is the preparatory subsidiary.
void resolveDeverbalStemBeforeDependentAuxiliary(std::vector<core::Morpheme>& result) {
  for (size_t idx = 0; idx + 1 < result.size(); ++idx) {
    auto& stem = result[idx];
    auto& auxiliary = result[idx + 1];
    const bool negative_nai = auxiliary.extended_pos == core::ExtendedPOS::AuxNegativeNai;
    const bool nominal_stem = stem.pos == core::PartOfSpeech::Noun;
    if ((!nominal_stem || (!negative_nai && stem.extended_pos != core::ExtendedPOS::NounVerbal)) ||
        !grammar::isERowCodepoint(utf8::decodeLastChar(stem.surface))) {
      continue;
    }
    const bool negative_n = auxiliary.surface == "ん" && (auxiliary.extended_pos == core::ExtendedPOS::AuxNegativeNu ||
                                                          auxiliary.extended_pos == core::ExtendedPOS::ParticleNo);
    const bool preparatory = auxiliary.extended_pos == core::ExtendedPOS::AuxAspectOku;
    if (!negative_n && !negative_nai && !preparatory) {
      continue;
    }
    retag(stem, core::PartOfSpeech::Verb,
          (negative_n || negative_nai) ? core::ExtendedPOS::VerbMizenkei : core::ExtendedPOS::VerbRenyokei,
          stem.surface + "る", dictionary::ConjugationType::Ichidan,
          (negative_n || negative_nai) ? grammar::ConjForm::Mizenkei : grammar::ConjForm::Renyokei);
    if (negative_n) {
      retag(auxiliary, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxNegativeNu, "ん",
            dictionary::ConjugationType::None, grammar::ConjForm::Base);
    }
  }
}

// Assign quotation roles that require the selected clause context. A final
// particle after a finite predicate closes a quoted clause (行く+か+と+尋ねる),
// whereas an interrogative pronoun remains a case phrase (誰+か+と+話す).
// A finite conjecture can likewise head a quoted clause even when its omitted
// subject leaves it at sentence start (らしい+と+聞く).
void resolveQuotativeParticleRoles(std::vector<core::Morpheme>& result) {
  for (size_t idx = 2; idx + 1 < result.size(); ++idx) {
    const auto& predicate_tail = result[idx - 2];
    const auto& final_particle = result[idx - 1];
    auto& quote = result[idx];
    const auto& reporting_predicate = result[idx + 1];
    const bool finite_predicate_tail = predicate_tail.pos == core::PartOfSpeech::Verb ||
                                       predicate_tail.pos == core::PartOfSpeech::Adjective ||
                                       predicate_tail.pos == core::PartOfSpeech::Auxiliary;
    if (!finite_predicate_tail || final_particle.extended_pos != core::ExtendedPOS::ParticleFinal ||
        quote.surface != "と" || quote.extended_pos != core::ExtendedPOS::ParticleCase ||
        reporting_predicate.pos != core::PartOfSpeech::Verb) {
      continue;
    }
    retag(quote, core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleQuote, "と",
          dictionary::ConjugationType::None, grammar::ConjForm::Base);
  }

  for (size_t idx = 0; idx + 2 < result.size(); ++idx) {
    auto& conjecture = result[idx];
    auto& quote = result[idx + 1];
    const auto& predicate = result[idx + 2];
    if (conjecture.surface != "らしい" || quote.surface != "と" || predicate.pos != core::PartOfSpeech::Verb) {
      continue;
    }
    retag(conjecture, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxConjectureRashii, "らしい",
          dictionary::ConjugationType::IAdjective, grammar::ConjForm::Base);
    retag(quote, core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleQuote, "と",
          dictionary::ConjugationType::None, grammar::ConjForm::Base);
  }
}

// Some productive inflection candidates are homographic across adjective and
// verb paradigms.  Their immediate grammatical continuation resolves the
// ambiguity without adding lexical exceptions: an adverbial -く before a
// continuative verb is an i-adjective, while -さ before passive れ is the
// irrealis of a Godan-sa verb.
void resolveAmbiguousInflections(std::vector<core::Morpheme>& result) {
  for (size_t idx = 0; idx + 1 < result.size(); ++idx) {
    auto& morpheme = result[idx];
    const auto& next = result[idx + 1];
    if (morpheme.pos == core::PartOfSpeech::Verb && morpheme.lemma == morpheme.surface &&
        utf8::endsWith(morpheme.surface, "く") && next.extended_pos == core::ExtendedPOS::VerbRenyokei) {
      retag(morpheme, core::PartOfSpeech::Adjective, core::ExtendedPOS::AdjRenyokei,
            normalize::concat(utf8::dropLastChar(morpheme.surface), "い"), dictionary::ConjugationType::IAdjective,
            grammar::ConjForm::Renyokei);
      continue;
    }
    if (morpheme.pos == core::PartOfSpeech::Suffix && morpheme.surface == "さ" &&
        next.extended_pos == core::ExtendedPOS::AuxPassive) {
      retag(morpheme, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbMizenkei, "する",
            dictionary::ConjugationType::Suru, grammar::ConjForm::Mizenkei);
      continue;
    }
    if (morpheme.pos == core::PartOfSpeech::Adjective && utf8::endsWith(morpheme.surface, "さ") &&
        next.extended_pos == core::ExtendedPOS::AuxPassive) {
      retag(morpheme, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbMizenkei,
            normalize::concat(utf8::dropLastChar(morpheme.surface), "す"), dictionary::ConjugationType::GodanSa,
            grammar::ConjForm::Mizenkei);
    }
  }

  // In the closed 五段未然形+使役せ+否定ない chain, the A-row ending
  // determines the underlying godan class.  The lattice may otherwise keep
  // the stem as a nominal homograph and せ as an independent verb.
  for (size_t idx = 0; idx + 2 < result.size(); ++idx) {
    auto& stem = result[idx];
    auto& causative = result[idx + 1];
    const auto& negative = result[idx + 2];
    if (causative.surface != "せ" || negative.extended_pos != core::ExtendedPOS::AuxNegativeNai) {
      continue;
    }
    // The dictionary-backed する irrealis is already fully identified by its
    // lemma. The same one-mora surface also spells a Godan-sa A-row ending, but
    // the generic repair below must not overwrite the irregular paradigm.
    if (grammar::isSuruBaseForm(stem.lemma)) {
      continue;
    }
    const char32_t a_row = utf8::decodeLastChar(stem.surface);
    const std::string_view base_suffix = grammar::godanBaseSuffixFromARow(a_row);
    const grammar::VerbType verb_type = grammar::verbTypeFromARowCodepoint(a_row);
    if (base_suffix.empty() || verb_type == grammar::VerbType::Unknown) {
      continue;
    }
    retag(stem, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbMizenkei,
          normalize::concat(utf8::dropLastChar(stem.surface), base_suffix), grammar::verbTypeToConjType(verb_type),
          grammar::ConjForm::Mizenkei);
    retag(causative, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxCausative, "せる",
          dictionary::ConjugationType::Ichidan, grammar::ConjForm::Mizenkei);
  }

  // A finite godan predicate before sentence-final さ cannot be an
  // i-adjective: i-adjectives end in い.  Repair the homographic generated
  // candidate and the role of the closed final particle together.
  if (result.size() >= 2) {
    auto& predicate = result[result.size() - 2];
    auto& final_sa = result.back();
    const std::string_view a_row = grammar::godanARowSuffixFromURow(utf8::decodeLastChar(predicate.surface));
    if (final_sa.surface == "さ" && final_sa.pos == core::PartOfSpeech::Suffix &&
        predicate.pos == core::PartOfSpeech::Adjective && predicate.lemma == predicate.surface + "い" &&
        !a_row.empty()) {
      const grammar::VerbType verb_type = grammar::verbTypeFromARowCodepoint(utf8::decodeFirstChar(a_row));
      retag(predicate, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbShuushikei, predicate.surface,
            grammar::verbTypeToConjType(verb_type), grammar::ConjForm::Base);
      retag(final_sa, core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleFinal, "さ",
            dictionary::ConjugationType::None, grammar::ConjForm::Base);
    }
  }
}

// A selected honorific auxiliary before the negative or te-form is an
// inflected lexical honorific verb (いらっしゃら+ない, いらっしゃっ+て,
// なさら+ない). Keeping its dictionary candidate auxiliary-shaped preserves
// the boundary after a preceding te-form; resolve the public grammatical
// category once its own continuation is known.
void resolveHonorificVerbInflection(std::vector<core::Morpheme>& result) {
  for (size_t idx = 0; idx + 1 < result.size(); ++idx) {
    auto& honorific = result[idx];
    const auto& continuation = result[idx + 1];
    const bool is_subsidiary_nasaru = honorific.lemma == "なさる" && idx > 0;
    const bool negative_follows =
        continuation.surface == "ない" && continuation.extended_pos == core::ExtendedPOS::AuxNegativeNai;
    const bool te_follows =
        continuation.surface == "て" && continuation.extended_pos == core::ExtendedPOS::ParticleConj;
    if (honorific.extended_pos != core::ExtendedPOS::AuxHonorific || is_subsidiary_nasaru ||
        (!negative_follows && !te_follows)) {
      continue;
    }
    honorific.pos = core::PartOfSpeech::Verb;
    honorific.conj_type = dictionary::ConjugationType::GodanRa;
    honorific.extended_pos = negative_follows ? core::ExtendedPOS::VerbMizenkei : core::ExtendedPOS::VerbOnbinkei;
    honorific.conj_form = negative_follows ? grammar::ConjForm::Mizenkei : grammar::ConjForm::Onbinkei;
  }
}

// In a te-form followed by a volitional, おこ is the mizenkei of the
// preparatory auxiliary おく (して+おこ+う), not an independent lexical verb.
void resolvePreparatoryVolitional(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx + 1 < result.size(); ++idx) {
    const auto& te = result[idx - 1];
    auto& oku = result[idx];
    const auto& volitional = result[idx + 1];
    if (te.surface != "て" || te.extended_pos != core::ExtendedPOS::ParticleConj || oku.surface != "おこ" ||
        volitional.extended_pos != core::ExtendedPOS::AuxVolitional) {
      continue;
    }
    retag(oku, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxAspectOku, "おく",
          dictionary::ConjugationType::GodanKa, grammar::ConjForm::Mizenkei);
  }
}

// The potential humble receiving verb is a benefactive auxiliary after a
// te-form or honorific renyokei (読んで+いただける, お待ち+いただける).
// Its inflection does not change that dependent role: いただけ+ます/ない and
// いただけれ+ば remain auxiliary uses. An object-marked independent use has
// neither licensed predecessor and therefore remains a lexical verb.
void resolveBenefactivePotential(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx < result.size(); ++idx) {
    const auto& predecessor = result[idx - 1];
    auto& benefactive = result[idx];
    const bool follows_te_form = followsTeFormConnective(predecessor);
    const bool potential_benefactive = grammar::isPotentialBenefactiveLemma(benefactive.lemma);
    if (follows_te_form && benefactive.pos == core::PartOfSpeech::Verb &&
        (grammar::isBenefactiveLemma(benefactive.lemma) || potential_benefactive)) {
      benefactive.pos = core::PartOfSpeech::Auxiliary;
      benefactive.extended_pos = core::ExtendedPOS::AuxBenefactive;
      continue;
    }
    const bool dependent_predecessor =
        predecessor.extended_pos == core::ExtendedPOS::VerbRenyokei ||
        (idx >= 2 && result[idx - 2].pos == core::PartOfSpeech::Prefix &&
         grammar::isHonorificPrefix(result[idx - 2].surface) && predecessor.pos == core::PartOfSpeech::Noun);
    if (!dependent_predecessor || !potential_benefactive) {
      continue;
    }
    benefactive.pos = core::PartOfSpeech::Auxiliary;
    benefactive.extended_pos = core::ExtendedPOS::AuxBenefactive;
    benefactive.conj_type = dictionary::ConjugationType::Ichidan;
  }
}

// The closed subsidiary みせる expresses resolve after a te-form
// (確認し+て+みせる). Candidate generation already marks only that contextual
// path as AuxAspectMiru, so expose its auxiliary POS without changing the
// homographic lexical verb in 絵を見せる.
void resolveDemonstrativeMiseru(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx < result.size(); ++idx) {
    const auto& connective = result[idx - 1];
    auto& subsidiary = result[idx];
    if (connective.extended_pos != core::ExtendedPOS::ParticleConj || subsidiary.lemma != "みせる") {
      continue;
    }
    subsidiary.pos = core::PartOfSpeech::Auxiliary;
    subsidiary.extended_pos = core::ExtendedPOS::AuxAspectMiru;
  }
}

// A clause-initial inability form has no predicate to attach to, so it is the
// lexical verb rather than a subsidiary auxiliary (かね+ない).  The lattice
// retains the closed-class candidate because the continuation alone is also
// valid for an auxiliary; the missing predecessor supplies the distinction.
void resolveInitialInabilityVerb(std::vector<core::Morpheme>& result) {
  for (size_t idx = 0; idx + 1 < result.size(); ++idx) {
    const bool starts_clause = idx == 0 || result[idx - 1].pos == core::PartOfSpeech::Symbol;
    auto& inability = result[idx];
    const auto& negative = result[idx + 1];
    if (!starts_clause || inability.extended_pos != core::ExtendedPOS::AuxInability ||
        negative.extended_pos != core::ExtendedPOS::AuxNegativeNai) {
      continue;
    }
    inability.pos = core::PartOfSpeech::Verb;
    inability.extended_pos = core::ExtendedPOS::VerbMizenkei;
    inability.conj_form = grammar::ConjForm::Mizenkei;
  }
}

// A finite いる directly after the connective te/de particle is the
// progressive auxiliary. The lattice preserves the lexical verb candidate so
// existential uses remain available, then this complete local context assigns
// the dependent reading (遅れて+いる, 読んで+いる).
void resolveProgressiveIru(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx < result.size(); ++idx) {
    auto& iru = result[idx];
    const auto& immediate = result[idx - 1];
    const bool direct_te_form = followsTeFormConnective(immediate);
    const bool focused_te_form = idx >= 2 && immediate.extended_pos == core::ExtendedPOS::ParticleBinding &&
                                 result[idx - 2].extended_pos == core::ExtendedPOS::ParticleConj &&
                                 grammar::isTeDeSurface(result[idx - 2].surface);
    const bool finite_iru_form =
        iru.extended_pos == core::ExtendedPOS::VerbShuushikei || iru.extended_pos == core::ExtendedPOS::VerbRenyokei;
    if ((!direct_te_form && !focused_te_form) || iru.lemma != "いる" || !finite_iru_form) {
      continue;
    }
    iru.pos = core::PartOfSpeech::Auxiliary;
    iru.extended_pos = core::ExtendedPOS::AuxAspectIru;
    iru.conj_type = dictionary::ConjugationType::Ichidan;
    if (iru.surface == "いる") {
      iru.conj_form = grammar::ConjForm::Base;
    }
  }
}

// A lexical verb and its subsidiary use share surface and lemma. The selected
// predecessor supplies the missing role evidence: a verb continuative selects
// potential 得る, while connective て/で selects completive しまう. Object-
// marked lexical uses (利益を得る, 荷物をしまう) have neither predecessor and
// therefore remain verbs.
void resolveDependentVerbHomographs(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx < result.size(); ++idx) {
    const auto& predecessor = result[idx - 1];
    auto& dependent = result[idx];
    if (dependent.pos != core::PartOfSpeech::Verb) {
      continue;
    }
    if (predecessor.extended_pos == core::ExtendedPOS::VerbRenyokei &&
        grammar::isRenyokeiPotentialAuxiliaryLemma(dependent.lemma)) {
      dependent.pos = core::PartOfSpeech::Auxiliary;
      dependent.extended_pos = core::ExtendedPOS::AuxPotential;
      continue;
    }
    const bool follows_te_form = followsTeFormConnective(predecessor);
    if (follows_te_form && grammar::isTeFormCompletiveAuxiliaryLemma(dependent.lemma)) {
      dependent.pos = core::PartOfSpeech::Auxiliary;
      dependent.extended_pos = core::ExtendedPOS::AuxAspectShimau;
      if (dependent.lemma == "仕舞う") {
        dependent.lemma = "しまう";
      }
      continue;
    }
    const bool followed_by_negative =
        idx + 1 < result.size() && result[idx + 1].extended_pos == core::ExtendedPOS::AuxNegativeNai;
    if (predecessor.pos == core::PartOfSpeech::Auxiliary && followed_by_negative &&
        grammar::isPassiveAuxiliaryLemma(dependent.lemma)) {
      retag(dependent, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxPassive, dependent.lemma,
            dictionary::ConjugationType::Ichidan, grammar::ConjForm::Mizenkei);
    }
  }
}

// Some lattice homographs become unambiguous only after the complete selected
// token chain is visible. Recover productive predicates from closed
// inflectional followers rather than from an open-class surface list.
void resolveClosedInflectionalChains(std::vector<core::Morpheme>& result) {
  const auto restore_renyokei = [](core::Morpheme& stem) {
    if (grammar::endsWithERow(stem.surface)) {
      retag(stem, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, stem.surface + "る",
            dictionary::ConjugationType::Ichidan, grammar::ConjForm::Renyokei);
      return true;
    }
    return retagGodanRenyokeiFromIRow(stem, true);
  };

  // A past auxiliary cannot attach directly to an adverb or noun. Its selected
  // surface licenses the productive continuative reading of the preceding
  // token. If lemmatization already restored the verb but the lattice selected
  // the OTHER homograph for た/だ, close that role too.
  for (size_t idx = 0; idx + 1 < result.size(); ++idx) {
    auto& stem = result[idx];
    auto& past = result[idx + 1];
    const bool past_surface = grammar::isPastMarkerTaDaSurface(past.surface);
    // Surface だ is also the copula after nouns and formal nouns; it can only
    // close an already-verbal onbin chain below, never license conversion of a
    // non-predicate predecessor by itself.
    if (past.surface == "た" && stem.pos != core::PartOfSpeech::Verb && stem.pos != core::PartOfSpeech::Auxiliary &&
        stem.pos != core::PartOfSpeech::Adjective) {
      restore_renyokei(stem);
    }
    const bool accepts_past =
        stem.extended_pos == core::ExtendedPOS::VerbRenyokei || stem.extended_pos == core::ExtendedPOS::VerbOnbinkei ||
        stem.extended_pos == core::ExtendedPOS::AdjKatt || stem.extended_pos == core::ExtendedPOS::AuxNegativeNai;
    if (past_surface && accepts_past && past.extended_pos != core::ExtendedPOS::AuxTenseTa) {
      retag(past, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxTenseTa, past.surface,
            dictionary::ConjugationType::None, grammar::ConjForm::Base);
    }
  }

  // Regional ておる/でおる contractions retain the ordinary Godan-ra
  // paradigm. A selected conditional plus ば supplies closed evidence for
  // the preceding continuative. The finite contraction itself keeps the Verb
  // category required by the public token contract.
  for (size_t idx = 0; idx + 2 < result.size(); ++idx) {
    auto& stem = result[idx];
    auto& contraction = result[idx + 1];
    const auto& conditional = result[idx + 2];
    if (stem.pos == core::PartOfSpeech::Verb || !grammar::isDialectalOruContractionLemma(contraction.lemma) ||
        contraction.extended_pos != core::ExtendedPOS::VerbKateikei ||
        conditional.extended_pos != core::ExtendedPOS::ParticleConj || conditional.surface != "ば") {
      continue;
    }
    restore_renyokei(stem);
  }

  // The independent negative adjective ない has a closed conditional form
  // なけれ+ば.  Its lemma and the conjunctive follower distinguish it from
  // ordinary verb conditionals and from dependent negative auxiliaries.
  for (size_t idx = 0; idx + 1 < result.size(); ++idx) {
    auto& negative = result[idx];
    const auto& conditional = result[idx + 1];
    if (negative.pos == core::PartOfSpeech::Verb && negative.extended_pos == core::ExtendedPOS::VerbKateikei &&
        negative.lemma == "ない" && conditional.extended_pos == core::ExtendedPOS::ParticleConj &&
        conditional.surface == "ば") {
      retag(negative, core::PartOfSpeech::Adjective, core::ExtendedPOS::AdjKeForm, "ない",
            dictionary::ConjugationType::IAdjective, grammar::ConjForm::Kateikei);
    }
  }

  // In existential ある+は+する+ない, the complete emphatic-negative chain
  // selects lexical ある rather than the copular homograph.  Requiring all
  // four roles leaves copular であります and nominal X+は+しない intact.
  for (size_t idx = 0; idx + 3 < result.size(); ++idx) {
    auto& aru = result[idx];
    const auto& binding = result[idx + 1];
    const auto& suru = result[idx + 2];
    const auto& negative = result[idx + 3];
    if (aru.pos == core::PartOfSpeech::Auxiliary && aru.extended_pos == core::ExtendedPOS::AuxCopulaDa &&
        aru.lemma == "ある" && binding.pos == core::PartOfSpeech::Particle && binding.surface == "は" &&
        suru.pos == core::PartOfSpeech::Verb && suru.lemma == "する" && negative.pos == core::PartOfSpeech::Auxiliary &&
        negative.lemma == "ない") {
      retag(aru, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "ある",
            dictionary::ConjugationType::GodanRa, grammar::ConjForm::Renyokei);
    }
  }
}

// The binding particle しか selects a following modern negative predicate.
// Its ない-family forms remain dependent auxiliaries even when the lattice's
// adjective homograph wins locally (歩く+しか+なかっ+た).
void resolveBindingParticleNegative(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx < result.size(); ++idx) {
    const auto& binding = result[idx - 1];
    auto& negative = result[idx];
    if (binding.extended_pos != core::ExtendedPOS::ParticleBinding || binding.surface != "しか" ||
        negative.lemma != "ない" || negative.pos != core::PartOfSpeech::Adjective) {
      continue;
    }
    negative.pos = core::PartOfSpeech::Auxiliary;
    negative.extended_pos = core::ExtendedPOS::AuxNegativeNai;
  }
}

// A te-form followed by the continuative of ある is the resultative auxiliary
// construction (確認+し+て+あり+ます), not an existential verb.  The lattice
// cannot decide the homograph until the connective particle is selected.
void resolveTearuAuxiliary(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx < result.size(); ++idx) {
    const auto& connective = result[idx - 1];
    auto& aru = result[idx];
    if (!followsTeFormConnective(connective) || aru.lemma != "ある" ||
        aru.extended_pos != core::ExtendedPOS::VerbRenyokei) {
      continue;
    }
    aru.pos = core::PartOfSpeech::Auxiliary;
    aru.extended_pos = core::ExtendedPOS::AuxCopulaDa;
    aru.conj_type = dictionary::ConjugationType::GodanRa;
    aru.conj_form = grammar::ConjForm::Renyokei;
  }
}

// The Kuruwa-kotoba polite ending ありんす contains the continuative lexical
// verb あり followed by the archaic auxiliary ん and the verb す. When it
// follows copular で, the lattice can otherwise reinterpret あり as a copula
// and ん as a nominalizer.
void resolveKuruwaPoliteAru(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx + 2 < result.size(); ++idx) {
    const auto& de = result[idx - 1];
    auto& aru = result[idx];
    auto& n = result[idx + 1];
    const auto& su = result[idx + 2];
    if (de.surface != "で" || de.extended_pos != core::ExtendedPOS::AuxCopulaDa || aru.surface != "あり" ||
        n.surface != "ん" || su.surface != "す" || su.lemma != "する") {
      continue;
    }
    retag(aru, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "ある", dictionary::ConjugationType::GodanRa,
          grammar::ConjForm::Renyokei);
    retag(n, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxNegativeNu, "ん", dictionary::ConjugationType::None,
          grammar::ConjForm::Base);
  }
}

// A limiting particle followed by あって is the lexical verb ある in its
// sokuonbin form (読むだけあって).  The copular あっ plus auxiliary てる
// alternative is not a valid auxiliary chain.
void resolveParticleAruOnbin(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx + 1 < result.size(); ++idx) {
    const auto& particle = result[idx - 1];
    auto& aru = result[idx];
    auto& te = result[idx + 1];
    if (particle.extended_pos != core::ExtendedPOS::ParticleAdverbial || aru.surface != "あっ" ||
        aru.extended_pos != core::ExtendedPOS::AuxCopulaDa || te.surface != "て" ||
        te.extended_pos != core::ExtendedPOS::AuxAspectIru) {
      continue;
    }
    retag(aru, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbOnbinkei, "ある", dictionary::ConjugationType::GodanRa,
          grammar::ConjForm::Onbinkei);
    retag(te, core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleConj, "て", dictionary::ConjugationType::None,
          grammar::ConjForm::Base);
  }
}

// A verb's continuative or onbin form followed by て/で is the connective
// particle.  Its surface overlaps with several auxiliary entries, whose POS
// is only disambiguated once the preceding selected verb form is available.
void resolveVerbTeParticle(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx < result.size(); ++idx) {
    const auto& verb = result[idx - 1];
    auto& te = result[idx];
    const bool is_connective_verb_form =
        verb.extended_pos == core::ExtendedPOS::VerbOnbinkei || verb.extended_pos == core::ExtendedPOS::VerbRenyokei ||
        verb.extended_pos == core::ExtendedPOS::VerbTeForm || verb.extended_pos == core::ExtendedPOS::AuxAspectOku;
    if (!is_connective_verb_form || !grammar::isTeDeSurface(te.surface)) {
      continue;
    }
    const bool contracted_progressive_before_past = te.extended_pos == core::ExtendedPOS::AuxAspectIru &&
                                                    idx + 1 < result.size() &&
                                                    result[idx + 1].extended_pos == core::ExtendedPOS::AuxTenseTa;
    if (contracted_progressive_before_past) {
      continue;
    }
    retag(te, core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleConj, te.surface,
          dictionary::ConjugationType::None, grammar::ConjForm::Base);
  }
}

// Productive compound adjectives attach to a verb renyokei (読み+やすい、
// 使い+にくい).  The stem is also a deverbal noun, so recover its verbal
// category from the closed adjective suffix after the boundary is selected.
void resolveCompoundAdjectiveRenyokei(std::vector<core::Morpheme>& result) {
  for (size_t idx = 0; idx + 1 < result.size(); ++idx) {
    auto& stem = result[idx];
    const auto& adjective = result[idx + 1];
    const bool has_renyokei = stem.extended_pos == core::ExtendedPOS::VerbRenyokei;
    if ((stem.pos != core::PartOfSpeech::Noun && !has_renyokei) || adjective.pos != core::PartOfSpeech::Adjective ||
        adjective.extended_pos == core::ExtendedPOS::AdjStem ||
        !utf8::equalsAny(adjective.lemma, {"やすい", "にくい", "がたい"})) {
      continue;
    }
    if (!has_renyokei) {
      retagGodanRenyokeiFromIRow(stem, true);
    }
  }

  // In nominalized compounds (読み+にく+さ、読み+がた+さ), the adjective
  // reaches the selected path as its stem and can retain a suffix tag.  The
  // closed nominalizer さ supplies the missing evidence for both the verbal
  // renyokei and the productive adjective reading.
  for (size_t idx = 0; idx + 2 < result.size(); ++idx) {
    auto& stem = result[idx];
    auto& adjective_stem = result[idx + 1];
    const auto& nominalizer = result[idx + 2];
    const bool has_renyokei = stem.extended_pos == core::ExtendedPOS::VerbRenyokei;
    if ((stem.pos != core::PartOfSpeech::Noun && !has_renyokei) || nominalizer.surface != "さ" ||
        nominalizer.pos != core::PartOfSpeech::Suffix ||
        !utf8::equalsAny(adjective_stem.surface, {"やす", "にく", "がた"})) {
      continue;
    }
    if (!has_renyokei && !retagGodanRenyokeiFromIRow(stem, true)) {
      continue;
    }
    const std::string lemma = adjective_stem.surface + "い";
    retag(adjective_stem, core::PartOfSpeech::Adjective, core::ExtendedPOS::AdjStem, lemma,
          dictionary::ConjugationType::IAdjective, grammar::ConjForm::Base);
  }

  // The same suffixes can occur in their stem forms before appearance そう.
  // Their lexical readings (やすい adjective / にく verb) are then unavailable:
  // the preceding i-row stem fixes the productive compound construction.
  for (size_t idx = 0; idx + 2 < result.size(); ++idx) {
    auto& stem = result[idx];
    auto& suffix = result[idx + 1];
    auto& sou = result[idx + 2];
    const bool has_renyokei = stem.extended_pos == core::ExtendedPOS::VerbRenyokei;
    if ((stem.pos != core::PartOfSpeech::Noun && !has_renyokei) || sou.surface != "そう" ||
        !utf8::equalsAny(suffix.surface, {"やす", "にく"}) ||
        (!has_renyokei && !retagGodanRenyokeiFromIRow(stem, true))) {
      continue;
    }

    if (suffix.surface == "やす") {
      retag(suffix, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::Unknown, "やす",
            dictionary::ConjugationType::None, grammar::ConjForm::Base);
      if (idx + 3 < result.size() && isPredicativeCopula(result[idx + 3])) {
        sou.pos = core::PartOfSpeech::Adjective;
        sou.extended_pos = core::ExtendedPOS::AdjNaAdj;
      }
      continue;
    }

    retag(suffix, core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, "にく", dictionary::ConjugationType::None,
          grammar::ConjForm::Base);
    if (idx + 3 < result.size() && result[idx + 3].surface == "な") {
      retagAppearanceSou(sou);
      auto& na = result[idx + 3];
      retagCopulaDa(na);
    }
  }
}

// A sahen noun followed by the ambiguous し takes the productive する
// reading before an independent verb or negative auxiliary. The lattice may
// retain either a connective-particle or lexical-verb candidate, so resolve
// the complete grammatical context after boundary selection.
void resolveSahenRenyokei(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx + 1 < result.size(); ++idx) {
    const auto& noun = result[idx - 1];
    auto& suru = result[idx];
    auto& following = result[idx + 1];
    const bool can_be_suru =
        suru.extended_pos == core::ExtendedPOS::ParticleConj || suru.extended_pos == core::ExtendedPOS::VerbRenyokei;
    const bool negative_follows = following.extended_pos == core::ExtendedPOS::AuxNegativeNai ||
                                  (following.pos == core::PartOfSpeech::Adjective && following.surface == "ない");
    const bool completes_sahen = following.pos == core::PartOfSpeech::Verb || negative_follows;
    if (noun.pos != core::PartOfSpeech::Noun || !grammar::isAllKanji(noun.surface) ||
        normalize::utf8Length(noun.surface) < 2 || !grammar::isSuruRenyokeiSurface(suru.surface) || !can_be_suru ||
        !completes_sahen) {
      continue;
    }
    retag(suru, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "する", dictionary::ConjugationType::Suru,
          grammar::ConjForm::Renyokei);
    if (negative_follows && following.surface == "ない") {
      retagNegativeNai(following);
    }
  }
}

// A quotative particle or demonstrative adverb followed by いっ and a te/past
// continuation is the verb 言う in euphonic form. The same bare surface remains
// ambiguous with 行く, so the left context supplies the evidence.
void resolveDemonstrativeQuotativeOnbin(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx + 1 < result.size(); ++idx) {
    const auto& demonstrative = result[idx - 1];
    auto& verb = result[idx];
    const auto& continuation = result[idx + 1];
    const bool is_te_or_past = continuation.extended_pos == core::ExtendedPOS::ParticleConj ||
                               continuation.extended_pos == core::ExtendedPOS::AuxTenseTa;
    const bool demonstrative_context = (demonstrative.extended_pos == core::ExtendedPOS::Adverb ||
                                        demonstrative.extended_pos == core::ExtendedPOS::AdverbQuotative) &&
                                       grammar::isDemonstrativeUAdverb(demonstrative.surface);
    // とか introduces a quotation as と does, hedged or listed rather than plain
    // (本とか言って), so it carries the same evidence for the euphonic form.
    const bool quotative_context =
        demonstrative.pos == core::PartOfSpeech::Particle && utf8::equalsAny(demonstrative.surface, {"と", "とか"});
    if ((!demonstrative_context && !quotative_context) || verb.surface != "いっ" || !is_te_or_past) {
      continue;
    }
    retag(verb, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbOnbinkei, "いう", dictionary::ConjugationType::GodanWa,
          grammar::ConjForm::Onbinkei);
  }
}

// The causative homograph し before polite ます is the renyokei of する
// after an honorific verb stem or a nominalized stem (お+願い+し+ます).
// A real causative uses a mizenkei before せ/させ, so this finite polite
// continuation resolves the grammatical role without changing boundaries.
void resolvePoliteSuruRenyokei(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx + 1 < result.size(); ++idx) {
    const auto& stem = result[idx - 1];
    auto& suru = result[idx];
    const auto& masu = result[idx + 1];
    if ((stem.pos != core::PartOfSpeech::Noun && stem.extended_pos != core::ExtendedPOS::VerbRenyokei) ||
        !grammar::isSuruRenyokeiSurface(suru.surface) || suru.extended_pos != core::ExtendedPOS::AuxCausative ||
        masu.extended_pos != core::ExtendedPOS::AuxTenseMasu) {
      continue;
    }
    retag(suru, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "する", dictionary::ConjugationType::Suru,
          grammar::ConjForm::Renyokei);
  }
}

// An interrogative plus final particle forms an indefinite subject in
// 誰かいますか. The following いる is the existential main verb before ます,
// not the progressive auxiliary that appears after a verb te-form.
void resolveIndefiniteExistentialIru(std::vector<core::Morpheme>& result) {
  for (size_t idx = 2; idx + 1 < result.size(); ++idx) {
    const auto& interrogative = result[idx - 2];
    const auto& final_particle = result[idx - 1];
    auto& iru = result[idx];
    const auto& masu = result[idx + 1];
    if (interrogative.extended_pos != core::ExtendedPOS::PronounInterrogative ||
        final_particle.extended_pos != core::ExtendedPOS::ParticleFinal ||
        iru.extended_pos != core::ExtendedPOS::AuxAspectIru || masu.extended_pos != core::ExtendedPOS::AuxTenseMasu) {
      continue;
    }
    retag(iru, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "いる", dictionary::ConjugationType::Ichidan,
          grammar::ConjForm::Renyokei);
  }
}

}  // namespace suzume::postprocess::resolver

namespace suzume::postprocess {

void Postprocessor::convertPrefixVerbToNoun(std::vector<core::Morpheme>& morphemes) {
  if (morphemes.size() < 2) {
    return;
  }

  for (size_t i = 0; i < morphemes.size(); ++i) {
    core::Morpheme& morpheme = morphemes[i];

    // Check if previous morpheme was PREFIX (お or ご)
    if (i > 0 && morphemes[i - 1].pos == core::PartOfSpeech::Prefix) {
      const std::string& prefix_surface = morphemes[i - 1].surface;
      // Only for honorific prefixes お and ご
      if (utf8::equalsAny(prefix_surface, {"お", "ご", "御"})) {
        // Restore a nominally homographic stem when a following humble or
        // honorific subsidiary supplies decisive verbal context:
        // お+知らせ+いたす, お+使い+いただく. This mirrors the preservation
        // rule below for stems that already reached the lattice as verbs.
        if (morpheme.pos == core::PartOfSpeech::Noun && i + 1 < morphemes.size() &&
            (grammar::isHumbleHonorificLemma(morphemes[i + 1].lemma) ||
             grammar::isPotentialBenefactiveLemma(morphemes[i + 1].lemma))) {
          const char32_t morpheme_last = utf8::decodeLastChar(morpheme.surface);
          if (grammar::isIRowCodepoint(morpheme_last)) {
            resolver::retagGodanRenyokeiFromIRow(morpheme, false);
          } else if (grammar::isERowCodepoint(morpheme_last)) {
            morpheme.lemma = morpheme.surface + "る";
            morpheme.conj_type = dictionary::ConjugationType::Ichidan;
            morpheme.pos = core::PartOfSpeech::Verb;
            morpheme.extended_pos = core::ExtendedPOS::VerbRenyokei;
          }
        }
        if (morpheme.pos == core::PartOfSpeech::Noun && i + 1 < morphemes.size() &&
            morphemes[i + 1].extended_pos == core::ExtendedPOS::VerbRenyokei && morphemes[i + 1].surface == "し" &&
            morphemes[i + 1].lemma == "する") {
          const char32_t stem_last = utf8::decodeLastChar(morpheme.surface);
          if (grammar::isIRowCodepoint(stem_last)) {
            resolver::retagGodanRenyokeiFromIRow(morpheme, true);
          } else if (grammar::isERowCodepoint(stem_last)) {
            resolver::retag(morpheme, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei,
                            morpheme.surface + "る", dictionary::ConjugationType::Ichidan, grammar::ConjForm::Renyokei);
          }
        }
        // Convert VERB to NOUN (renyoukei nominalization)
        // e.g., 願い(VERB) → 願い(NOUN) after お
        // Exception: when followed by causative auxiliary (せ/させ),
        // the verb is part of a causative construction (お聞かせ, お知らせ)
        // and should remain as VERB
        // Nominalization after an honorific prefix applies to a continuative
        // stem, not to a finite verb.  Keeping this distinction preserves
        // literary terminal forms such as お+はす as verbs.
        const bool honorific_nominal_stem = morpheme.extended_pos == core::ExtendedPOS::VerbRenyokei ||
                                            (morpheme.extended_pos == core::ExtendedPOS::VerbKateikei &&
                                             grammar::isERowCodepoint(utf8::decodeLastChar(morpheme.surface)));
        if (morpheme.pos == core::PartOfSpeech::Verb && honorific_nominal_stem) {
          bool preserves_verbal_reading = false;
          if (i + 1 < morphemes.size()) {
            const auto& next = morphemes[i + 1];
            preserves_verbal_reading = grammar::isHumbleHonorificLemma(next.lemma) ||
                                       grammar::isPotentialBenefactiveLemma(next.lemma) ||
                                       next.extended_pos == core::ExtendedPOS::AuxCausative ||
                                       // An honorific subsidiary takes a continuative, not a
                                       // nominal, so it keeps the stem verbal (お読み+やす).
                                       next.extended_pos == core::ExtendedPOS::AuxHonorific;

            // An actual hypothetical e-row form keeps its verbal analysis
            // before ば (お届け+ば). Without ば, the same dictionary edge is
            // homographic with the honorific nominal stem (お+届け).
            preserves_verbal_reading = preserves_verbal_reading ||
                                       (morpheme.extended_pos == core::ExtendedPOS::VerbKateikei &&
                                        next.extended_pos == core::ExtendedPOS::ParticleConj && next.surface == "ば");

            // In the productive honorific construction お/ご+連用形+する,
            // the stem remains a verb.  The closed continuation する supplies
            // the grammatical evidence; requiring a particular open-class
            // argument or stem would turn this into an unbounded word list.
            bool follows_suru =
                next.extended_pos == core::ExtendedPOS::VerbRenyokei && next.surface == "し" && next.lemma == "する";
            preserves_verbal_reading = preserves_verbal_reading || follows_suru;

            // An e-row continuative before する is an ichidan verb in the
            // productive honorific construction (お見せする), not a nominal
            // renyokei such as お待ちする.
            if (grammar::isERowCodepoint(utf8::decodeLastChar(morpheme.surface)) &&
                next.pos == core::PartOfSpeech::Verb && next.lemma == "する") {
              preserves_verbal_reading = true;
            }

            // In the productive honorific お/ご+連用形+に+なる
            // construction, the stem remains verbal even when it is
            // homographic with a nominalized renyokei (お聞きになる).
            if (next.extended_pos == core::ExtendedPOS::ParticleCase && next.surface == "に" &&
                i + 2 < morphemes.size()) {
              const auto& following = morphemes[i + 2];
              preserves_verbal_reading =
                  preserves_verbal_reading || (following.pos == core::PartOfSpeech::Verb && following.lemma == "なる");
            }
          }
          if (!preserves_verbal_reading) {
            resolver::retagNounSurface(morpheme);
            SUZUME_DEBUG_LOG_VERBOSE("[POSTPROC] Nominalized: " << morpheme.surface << " (VERB → NOUN after "
                                                                << prefix_surface << ")\n");
          }
        }
      }
    }
  }
}

}  // namespace suzume::postprocess
