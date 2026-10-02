#include <array>
#include <cstdint>
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

// The appearance construction is ambiguous at the lattice level: 形容詞語幹+
// そう can use either the appearance auxiliary or the na-adjectival そう
// candidate. An adjectival predicate with a predicative copula takes the
// na-adjectival reading, while a verbal predicate takes the appearance
// auxiliary. Attributive な is deliberately excluded because it is not a
// predicative copula onset.
bool isPredicativeCopula(const core::Morpheme& morpheme) {
  if (morpheme.extended_pos != core::ExtendedPOS::AuxCopulaDa &&
      morpheme.extended_pos != core::ExtendedPOS::AuxCopulaDesu) {
    return false;
  }
  return morpheme.surface == "だ" || morpheme.surface == "だっ" || morpheme.surface == "です" ||
         morpheme.surface == "でし";
}

void resolveAppearanceSouPredicate(std::vector<core::Morpheme>& result) {
  if (result.size() >= 3 && result[0].surface == "そう" && result[1].surface == "で" && result[2].surface == "ござい") {
    auto& sou = result[0];
    retagAdverbialSou(sou);
  }
  if (result.size() >= 2 && result[0].surface == "そう" && result[0].pos == core::PartOfSpeech::Adverb &&
      result[1].surface == "で" && (result.size() < 3 || result[2].surface != "ござい")) {
    auto& sou = result[0];
    retagNaAdjectivalSou(sou);
  }

  for (size_t idx = 1; idx < result.size(); ++idx) {
    const auto& predicate = result[idx - 1];
    auto& sou = result[idx];
    if (predicate.extended_pos != core::ExtendedPOS::AuxAspectShimau || sou.surface != "そう" ||
        sou.pos != core::PartOfSpeech::Adverb) {
      continue;
    }
    retagAppearanceSou(sou);
  }

  // A productive -しい adjective loses its final い before appearance そう.
  // The remaining ...し stem can be selected as a plain noun homograph, but
  // NounVerbal (話し+そう) must keep its verbal reading.  The closed そう
  // follower resolves only the plain-noun homograph without enumerating
  // adjective lexemes.
  for (size_t idx = 0; idx + 1 < result.size(); ++idx) {
    auto& stem = result[idx];
    const auto& sou = result[idx + 1];
    if (stem.extended_pos != core::ExtendedPOS::Noun || !utf8::endsWith(stem.surface, "し") || sou.surface != "そう") {
      continue;
    }
    retag(stem, core::PartOfSpeech::Adjective, core::ExtendedPOS::AdjStem, stem.surface + "い",
          dictionary::ConjugationType::IAdjective, grammar::ConjForm::Base);
  }

  for (size_t idx = 1; idx + 1 < result.size(); ++idx) {
    auto& sou = result[idx];
    auto& predicate = result[idx - 1];
    const auto& copula = result[idx + 1];
    if (sou.surface != "そう" || !isPredicativeCopula(copula)) {
      continue;
    }

    const bool follows_nominalized_adjective = predicate.pos == core::PartOfSpeech::Suffix &&
                                               predicate.surface == "さ" && idx >= 2 &&
                                               result[idx - 2].pos == core::PartOfSpeech::Adjective;
    if ((predicate.pos == core::PartOfSpeech::Adjective || follows_nominalized_adjective) &&
        sou.extended_pos == core::ExtendedPOS::AuxAppearanceSou) {
      retagNaAdjectivalSou(sou);
      continue;
    }
    if (isVerbalPredicateBeforeSou(predicate) && sou.extended_pos == core::ExtendedPOS::AdjNaAdj) {
      retagAppearanceSou(sou);
    }
    if (sou.pos == core::PartOfSpeech::Adverb) {
      if (isVerbalPredicateBeforeSou(predicate)) {
        retagAppearanceSou(sou);
      } else {
        retagNaAdjectivalSou(sou);
      }
    }
  }

  for (size_t idx = 1; idx + 1 < result.size(); ++idx) {
    const auto& preceding = result[idx - 1];
    auto& sou = result[idx];
    const auto& na = result[idx + 1];
    const bool has_lexical_predicate = isVerbalPredicateBeforeSou(preceding);
    if (sou.surface != "そう" || sou.pos != core::PartOfSpeech::Adverb || na.surface != "な" || has_lexical_predicate) {
      continue;
    }
    retagNaAdjectivalSou(sou);
    auto& copula = result[idx + 1];
    retagCopulaDa(copula);
  }

  for (size_t idx = 1; idx + 1 < result.size(); ++idx) {
    const auto& stem = result[idx - 1];
    auto& sou = result[idx];
    auto& na = result[idx + 1];
    if (sou.surface != "そう" || na.surface != "な" ||
        (stem.pos != core::PartOfSpeech::Noun && stem.pos != core::PartOfSpeech::Verb &&
         stem.pos != core::PartOfSpeech::Adjective)) {
      continue;
    }
    retagAppearanceSou(sou);
    retagCopulaDa(na);
  }

  for (size_t idx = 1; idx + 1 < result.size(); ++idx) {
    const auto& stem = result[idx - 1];
    auto& sou = result[idx];
    const auto& particle = result[idx + 1];
    if (stem.extended_pos != core::ExtendedPOS::AdjStem || sou.surface != "そう" || particle.surface != "に" ||
        sou.pos != core::PartOfSpeech::Adverb) {
      continue;
    }
    retagAppearanceSou(sou);
  }
}

void resolveProgressiveContractionNominalizer(std::vector<core::Morpheme>& result) {
  // A negative auxiliary opening the text has no predicate to negate, and the
  // copula behind it cannot supply one either. What the two spell there is the
  // nominalizer plus that copula (んじゃないか).
  if (result.size() >= 2 && result[0].extended_pos == core::ExtendedPOS::AuxNegativeNu &&
      result[1].extended_pos == core::ExtendedPOS::AuxCopulaDa) {
    retag(result[0], core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleNo, "の",
          dictionary::ConjugationType::None, grammar::ConjForm::Base);
    // What negates that copula is the auxiliary, as in any copular negative
    // chain, not the existence adjective.
    if (result.size() >= 3 && result[2].extended_pos == core::ExtendedPOS::AdjBasic) {
      retagNegativeNai(result[2]);
    }
  }
  for (size_t idx = 1; idx + 1 < result.size(); ++idx) {
    auto& contraction = result[idx];
    const auto& connective = result[idx - 1];
    const auto& continuation = result[idx + 1];
    if (contraction.surface != "ん" || contraction.extended_pos != core::ExtendedPOS::AuxNegativeNu ||
        connective.extended_pos != core::ExtendedPOS::ParticleConj || !grammar::isTeDeSurface(connective.surface) ||
        (continuation.extended_pos != core::ExtendedPOS::ParticleNo &&
         continuation.extended_pos != core::ExtendedPOS::AuxCopulaDa)) {
      continue;
    }
    retag(contraction, core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleNo, "の",
          dictionary::ConjugationType::None, grammar::ConjForm::Base);
  }
}

// In the predicative pattern XにYない, a regular noun Y takes the independent
// negative adjective (本に相違ない), not the verbal negative auxiliary. Keep
// renyokei-derived and specially classified nouns out of this rule: their
// auxiliary analyses remain available for expressions such as 間違いない.
void resolveNominalPredicateNai(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx < result.size(); ++idx) {
    auto& negative = result[idx];
    auto& predicate = result[idx - 1];
    // The syntax that picks the independent adjective over the auxiliary is the
    // host, not the cell, so accept every cell of the negative paradigm the
    // lattice produced (ない, なかっ, なく, なけれ) rather than the base form
    // alone — 問題+なかっ+た has the same nominal host as 問題+ない.
    if (negative.lemma != "ない") {
      continue;
    }

    const bool is_godan_renyokei = predicate.extended_pos == core::ExtendedPOS::VerbRenyokei &&
                                   predicate.conj_type >= dictionary::ConjugationType::GodanKa &&
                                   predicate.conj_type <= dictionary::ConjugationType::GodanWa &&
                                   grammar::endsWithIRow(predicate.surface);
    // A Godan continuative stem cannot select the negative auxiliary; Godan
    // negation requires the a-row mizenkei (頼ら+ない, not 頼り+ない). When
    // that impossible path is selected, the homographic stem is a deverbal
    // noun followed by the independent adjective.
    if (predicate.pos == core::PartOfSpeech::Verb && is_godan_renyokei &&
        negative.extended_pos == core::ExtendedPOS::AuxNegativeNai) {
      retagNounSurface(predicate);
      retagNegativeAdjectiveCell(negative);
      continue;
    }

    // The open-class kanji candidate generator can occasionally create an
    // unattested 〜る verb from a nominal predicate (相違→相違る). In XにYない,
    // its all-kanji, non-dictionary renyokei shape is unambiguously nominal.
    // Restore that reading before resolving the independent negative.
    if (idx >= 2 && predicate.pos == core::PartOfSpeech::Verb &&
        predicate.extended_pos == core::ExtendedPOS::VerbRenyokei && !predicate.fromDictionary() &&
        grammar::isAllKanji(predicate.surface) && utf8::endsWith(predicate.lemma, "る")) {
      const auto& marker = result[idx - 2];
      if (marker.extended_pos == core::ExtendedPOS::ParticleCase && marker.surface == "に") {
        retagNounSurface(predicate);
      }
    }

    if (predicate.pos != core::PartOfSpeech::Noun) {
      continue;
    }

    // The negative auxiliary attaches to inflecting predicates, never
    // directly to a noun. Bare nominal negatives therefore use the
    // independent adjective ない (問題+ない, 関係+ない).
    if (negative.extended_pos == core::ExtendedPOS::AuxNegativeNai &&
        predicate.extended_pos != core::ExtendedPOS::ParticleBinding) {
      retagNegativeAdjectiveCell(negative);
      continue;
    }

    // In an attributive nominal-negative expression (頼り+ない+返事,
    // 問題+ない+方法), ない is the independent basic adjective modifying the
    // following nominal, not a dependent verbal auxiliary.
    if (idx + 1 < result.size() && result[idx + 1].pos == core::PartOfSpeech::Noun &&
        negative.extended_pos == core::ExtendedPOS::AuxNegativeNai) {
      retagNegativeAdjectiveCell(negative);
      continue;
    }

    // A noun must not inherit the binding-particle category. When a legacy
    // dictionary entry does so, restore the negative auxiliary reading that
    // belongs to the nominalized predicate (間違い+ない).
    if (predicate.extended_pos == core::ExtendedPOS::ParticleBinding &&
        negative.extended_pos == core::ExtendedPOS::AdjBasic) {
      negative.pos = core::PartOfSpeech::Auxiliary;
      negative.extended_pos = core::ExtendedPOS::AuxNegativeNai;
      negative.lemma = "ない";
      continue;
    }

    if (idx < 2 || negative.extended_pos != core::ExtendedPOS::AuxNegativeNai ||
        predicate.extended_pos != core::ExtendedPOS::Noun || predicate.surface == "ちがい") {
      continue;
    }
    const auto& marker = result[idx - 2];
    if (marker.extended_pos != core::ExtendedPOS::ParticleCase || marker.surface != "に") {
      continue;
    }
    retagNegativeAdjectiveCell(negative);
  }
}

// A nominal predicate followed by ではない forms a copular negative chain
// (本+で+は+ない, 静か+で+は+ない, はず+で+は+ない). Preserve the
// lattice's copula reading and give the final ない its auxiliary reading; the
// generic topic→ない bias otherwise treats it as an existence adjective.
void resolveCopularNegative(std::vector<core::Morpheme>& result) {
  if (result.size() >= 2 && result[0].surface == "で" && result[1].surface == "ない") {
    auto& de = result[0];
    auto& negative = result[1];
    retagCopulaDa(de);
    retagNegativeNai(negative);
  }
  for (size_t idx = 1; idx + 2 < result.size(); ++idx) {
    const auto& de = result[idx];
    const auto& predicate = result[idx - 1];
    const auto& topic = result[idx + 1];
    auto& negative = result[idx + 2];
    const bool is_nominal_predicate =
        predicate.pos == core::PartOfSpeech::Noun || predicate.pos == core::PartOfSpeech::Pronoun ||
        predicate.pos == core::PartOfSpeech::Suffix || predicate.extended_pos == core::ExtendedPOS::AdjNaAdj ||
        predicate.extended_pos == core::ExtendedPOS::ParticleNo ||
        predicate.extended_pos == core::ExtendedPOS::ParticleAdverbial ||
        predicate.extended_pos == core::ExtendedPOS::ParticleBinding ||
        // An auxiliary that hosts the copula is a predicate nominal too (べき+で+は+ない).
        predicate.pos == core::PartOfSpeech::Auxiliary;
    if (!is_nominal_predicate || de.surface != "で" || de.extended_pos != core::ExtendedPOS::AuxCopulaDa ||
        topic.extended_pos != core::ExtendedPOS::ParticleTopic || topic.surface != "は" ||
        !utf8::equalsAny(negative.surface, {"ない", "なく", "なかっ"})) {
      continue;
    }
    retagNegativeNai(negative);
  }

  // The direct copular negative (静かでない, 好きでない) has no topical
  // particle, but its final negative is still the dependent auxiliary.
  for (size_t idx = 1; idx < result.size(); ++idx) {
    const auto& de = result[idx - 1];
    auto& negative = result[idx];
    if (de.surface != "で" || de.extended_pos != core::ExtendedPOS::AuxCopulaDa || negative.surface != "ない") {
      continue;
    }
    retagNegativeNai(negative);
  }
}

// A na-adjective followed by であった uses the formal copula in its
// sokuonbin form. The same あっ remains lexical after a nominal case particle.
void resolveNaAdjectiveCopularPast(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx + 1 < result.size(); ++idx) {
    const auto& de = result[idx];
    const auto& predicate = result[idx - 1];
    auto& aru = result[idx + 1];
    if (predicate.extended_pos != core::ExtendedPOS::AdjNaAdj || de.surface != "で" ||
        de.extended_pos != core::ExtendedPOS::AuxCopulaDa || aru.surface != "あっ") {
      continue;
    }
    retag(aru, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxCopulaDa, "ある",
          dictionary::ConjugationType::GodanRa, grammar::ConjForm::Onbinkei);
  }
}

// The conditional allomorph たら is the past auxiliary after the sokuonbin
// copula (本+だっ+たら, 静か+だっ+たら), not an independent conjunction.
void resolveCopularPastConditional(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx < result.size(); ++idx) {
    const auto& copula = result[idx - 1];
    auto& conditional = result[idx];
    if (copula.extended_pos != core::ExtendedPOS::AuxCopulaDa || !utf8::endsWith(copula.surface, "っ") ||
        conditional.surface != "たら" || conditional.extended_pos != core::ExtendedPOS::ParticleConj) {
      continue;
    }
    retag(conditional, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxTenseTa, "た",
          dictionary::ConjugationType::None, grammar::ConjForm::Base);
  }
}

// These forms can expose one another through local retagging.  Keep their
// ordering together so callers can deliberately run the same copular pass
// again after an intervening resolver changes the available context.
void resolveCopularForms(std::vector<core::Morpheme>& result) {
  resolveCopularNegative(result);
  resolveNaAdjectiveCopularPast(result);
  resolveCopularPastConditional(result);
}

// The closed suffix がち is a na-adjectival predicate before copular で and
// an i-adjective continuation (がちでうまい). The lattice also offers a noun
// homograph, so resolve the suffix category from the complete local chain.
void resolveTendencySuffixCopula(std::vector<core::Morpheme>& result) {
  for (size_t idx = 0; idx + 2 < result.size(); ++idx) {
    auto& tendency = result[idx];
    auto& de = result[idx + 1];
    const auto& adjective = result[idx + 2];
    if (tendency.surface != "がち" || de.surface != "で" || adjective.extended_pos != core::ExtendedPOS::AdjBasic) {
      continue;
    }
    retag(tendency, core::PartOfSpeech::Suffix, core::ExtendedPOS::SuffixTendency, "がち",
          dictionary::ConjugationType::None, grammar::ConjForm::Base);
    retagCopulaDa(de);
  }
}

// The closed-class negative ない can reach the lattice as the homographic
// sequence な+い. AuxCopulaDa(な) cannot be followed directly by
// AuxAspectIru(い), so this pair is always the split negative rather than a
// valid auxiliary chain. Restore its single auxiliary token before POS
// resolution.
void mergeSplitCopularNegative(std::vector<core::Morpheme>& result) {
  for (size_t idx = 0; idx + 1 < result.size(); ++idx) {
    auto& na = result[idx];
    const auto& i = result[idx + 1];
    if (na.extended_pos != core::ExtendedPOS::AuxCopulaDa || na.surface != "な" || na.end != i.start ||
        i.extended_pos != core::ExtendedPOS::AuxAspectIru || i.surface != "い") {
      continue;
    }
    mergeInto(na, i);
    retagNegativeNai(na);
    if (idx == 0 || result[idx - 1].pos == core::PartOfSpeech::Noun ||
        result[idx - 1].pos == core::PartOfSpeech::Particle) {
      na.pos = core::PartOfSpeech::Adjective;
      na.extended_pos = core::ExtendedPOS::AdjBasic;
      na.conj_type = dictionary::ConjugationType::IAdjective;
    }
    result.erase(result.begin() + static_cast<std::ptrdiff_t>(idx + 1));
  }
}

// At clause end, the negative renyokei after the formal noun こと can lose by
// a near tie to the impossible copula+aspect split な+く. Restore the closed
// grammatical form without affecting ordinary attributive copula な.
void mergeSplitFormalNounNegativeRenyokei(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx + 1 < result.size(); ++idx) {
    const auto& formal_noun = result[idx - 1];
    auto& na = result[idx];
    const auto& ku = result[idx + 1];
    if (formal_noun.surface != "こと" || formal_noun.extended_pos != core::ExtendedPOS::NounFormal ||
        na.surface != "な" || na.extended_pos != core::ExtendedPOS::AuxCopulaDa || ku.surface != "く" ||
        ku.pos != core::PartOfSpeech::Auxiliary || na.end != ku.start) {
      continue;
    }
    mergeInto(na, ku);
    na.lemma = "ない";
    na.pos = core::PartOfSpeech::Adjective;
    na.extended_pos = core::ExtendedPOS::AdjRenyokei;
    na.conj_type = dictionary::ConjugationType::IAdjective;
    na.conj_form = grammar::ConjForm::Renyokei;
    result.erase(result.begin() + static_cast<std::ptrdiff_t>(idx + 1));
  }
}

// At the beginning of a clause, ない followed by a concessive conjunction is
// the independent i-adjective (ないのに), rather than the verbal negative
// auxiliary.  The lattice cannot use that following conjunction to resolve the
// homograph while choosing the first token.
void resolveInitialNegativeAdjective(std::vector<core::Morpheme>& result) {
  if (result.size() < 2 || result.front().surface != "ない" || result[1].surface != "のに") {
    return;
  }
  auto& negative = result.front();
  retagBasicNegativeAdjective(negative);
}

bool isObligationVerbStem(const core::Morpheme& morpheme) {
  // The complete right-hand chain supplies the form evidence. Ichidan
  // mizenkei/renyokei homographs and some generated Godan stems do not yet
  // carry a stable ExtendedPOS at this resolver stage.  A subsidiary verb
  // carries the stem just as a lexical one does (書いて+おか+ないといけない), so
  // its auxiliary reading selects the chain too.
  return morpheme.pos == core::PartOfSpeech::Verb || morpheme.pos == core::PartOfSpeech::Auxiliary;
}

// The role a slot of an obligation chain plays once the whole construction is
// confirmed. Only the homographic slots carry a resolution.
enum class ObligationRole : std::uint8_t {
  kKeep,         // closed-class filler with no competing reading
  kNegativeNai,  // negative auxiliary homographic with the adjective ない
  kObligation,   // いけ/なら, homographic with lexical いける/なる
};

// A slot accepts one or two fixed surfaces, or — when te_connective is set —
// て/で in its conjunctive-particle reading.
struct ObligationSlot {
  const char* first;
  const char* second;
  bool te_connective;
  ObligationRole role;
};

constexpr size_t kMaxObligationSlots = 4;

struct ObligationPattern {
  size_t length;
  std::array<ObligationSlot, kMaxObligationSlots> slots;
};

// Every obligation/prohibition chain has the same shape: a selecting verb stem
// followed by a fixed run of closed-class slots. Adding a construction is a
// row here, never another hand-written scan.
constexpr std::array<ObligationPattern, 5> kObligationPatterns{{
    // V未然 + な + あかん — the Kansai chain, whose な is the contracted
    // negative rather than the homographic copula or final particle.
    {2,
     {{{"な", nullptr, false, ObligationRole::kNegativeNai},
       {"あかん", nullptr, false, ObligationRole::kKeep},
       {nullptr, nullptr, false, ObligationRole::kKeep},
       {nullptr, nullptr, false, ObligationRole::kKeep}}}},
    // V未然 + なきゃ/なけりゃ + いけ/なら + ない
    {3,
     {{{"なきゃ", "なけりゃ", false, ObligationRole::kNegativeNai},
       {"いけ", "なら", false, ObligationRole::kObligation},
       {"ない", nullptr, false, ObligationRole::kNegativeNai},
       {nullptr, nullptr, false, ObligationRole::kKeep}}}},
    // V未然 + なく + ちゃ + いけ/なら + ない
    {4,
     {{{"なく", nullptr, false, ObligationRole::kNegativeNai},
       {"ちゃ", nullptr, false, ObligationRole::kKeep},
       {"いけ", "なら", false, ObligationRole::kObligation},
       {"ない", nullptr, false, ObligationRole::kNegativeNai}}}},
    // V未然 + ない + と + いけ/なら + ない
    {4,
     {{{"ない", nullptr, false, ObligationRole::kNegativeNai},
       {"と", nullptr, false, ObligationRole::kKeep},
       {"いけ", "なら", false, ObligationRole::kObligation},
       {"ない", nullptr, false, ObligationRole::kNegativeNai}}}},
    // V音便 + て/で + は + いけ + ない
    {4,
     {{{nullptr, nullptr, true, ObligationRole::kKeep},
       {"は", nullptr, false, ObligationRole::kKeep},
       {"いけ", nullptr, false, ObligationRole::kObligation},
       {"ない", nullptr, false, ObligationRole::kNegativeNai}}}},
}};

bool matchesObligationSlot(const ObligationSlot& slot, const core::Morpheme& morpheme) {
  if (slot.te_connective) {
    return followsTeFormConnective(morpheme);
  }
  if (slot.first != nullptr && morpheme.surface == slot.first) {
    return true;
  }
  return slot.second != nullptr && morpheme.surface == slot.second;
}

void applyObligationRole(const ObligationSlot& slot, core::Morpheme& morpheme) {
  switch (slot.role) {
    case ObligationRole::kNegativeNai:
      retagNegativeNai(morpheme);
      break;
    case ObligationRole::kObligation:
      if (morpheme.surface == "いけ") {
        retag(morpheme, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxPotential, "いける",
              dictionary::ConjugationType::Ichidan, grammar::ConjForm::Mizenkei);
      } else {
        retag(morpheme, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbMizenkei, "なる",
              dictionary::ConjugationType::GodanRa, grammar::ConjForm::Mizenkei);
      }
      break;
    case ObligationRole::kKeep:
      break;
  }
}

// Contracted obligation/prohibition chains contain several lexical
// homographs. Resolve their roles only when the complete closed construction
// and its selecting verb stem are present. This leaves independent ない,
// conditional なら, and lexical いける untouched.
void resolveObligationNaranai(std::vector<core::Morpheme>& result) {
  for (const auto& pattern : kObligationPatterns) {
    for (size_t idx = 1; idx + pattern.length <= result.size(); ++idx) {
      if (!isObligationVerbStem(result[idx - 1])) {
        continue;
      }
      bool complete = true;
      for (size_t slot = 0; slot < pattern.length; ++slot) {
        if (!matchesObligationSlot(pattern.slots[slot], result[idx + slot])) {
          complete = false;
          break;
        }
      }
      if (!complete) {
        continue;
      }
      for (size_t slot = 0; slot < pattern.length; ++slot) {
        applyObligationRole(pattern.slots[slot], result[idx + slot]);
      }
    }
  }
}

// The sequence な+さ+そう is the negative adjective stem, nominalizer, and
// appearance auxiliary (なさそう、食べなさそう).  Each mora has a competing
// closed-class homograph, so resolve the complete inflectional chain after
// boundary selection.
void resolveNegativeAppearanceChain(std::vector<core::Morpheme>& result) {
  for (size_t idx = 0; idx + 2 < result.size(); ++idx) {
    auto& na = result[idx];
    auto& sa = result[idx + 1];
    auto& sou = result[idx + 2];
    if (na.surface != "な" || sa.surface != "さ" || sou.surface != "そう") {
      continue;
    }
    retag(na, core::PartOfSpeech::Adjective, core::ExtendedPOS::AdjStem, "ない",
          dictionary::ConjugationType::IAdjective, grammar::ConjForm::Renyokei);
    retag(sa, core::PartOfSpeech::Suffix, core::ExtendedPOS::Suffix, "さ", dictionary::ConjugationType::None,
          grammar::ConjForm::Base);
    retagAppearanceSou(sou);
  }
}

// A predicate stem is what the negative auxiliary can attach to: a verb or a
// verbal auxiliary. The copula is not one — its negation takes the
// supplementary adjective instead.
bool followsPredicateStem(const core::Morpheme& morpheme) {
  if (morpheme.extended_pos == core::ExtendedPOS::AuxCopulaDa ||
      morpheme.extended_pos == core::ExtendedPOS::AuxCopulaDesu) {
    return false;
  }
  return morpheme.pos == core::PartOfSpeech::Verb || morpheme.pos == core::PartOfSpeech::Auxiliary;
}

// The surface なく is the continuative adjective form of ない wherever it
// follows a nominal or an adjective continuative (なくとも, 休み+なく,
// 明るく+なく). After a predicate stem it stays the negative auxiliary.
// The lattice shares those categories while scoring, so resolve the inflected
// surface after boundaries have been selected.
void resolveNegativeRenyokei(std::vector<core::Morpheme>& result) {
  for (size_t idx = 0; idx < result.size(); ++idx) {
    auto& negative = result[idx];
    // In the colloquial V-renyokei + っ + こ + ない + noun construction, the
    // final negative is the auxiliary modifying the following noun rather
    // than the independent negative adjective.
    if (negative.surface == "ない" && idx >= 2 && idx + 1 < result.size() &&
        result[idx - 1].extended_pos == core::ExtendedPOS::AuxAspectKuru &&
        result[idx - 2].extended_pos == core::ExtendedPOS::AuxNegativeMai &&
        result[idx + 1].pos == core::PartOfSpeech::Noun) {
      retagNegativeNai(negative);
      continue;
    }
    if (negative.surface != "なく" || negative.extended_pos != core::ExtendedPOS::AuxNegativeNai) {
      continue;
    }
    // The negative auxiliary attaches to a predicate stem, so なく keeps that
    // reading after a verb or a verbal auxiliary whatever follows it
    // (飲ま+なく+ちゃ, 食べ+なく+て, 読ま+なく+は+ない, れ+なく+なる).  The
    // copula is excluded: its negation uses the supplementary adjective
    // (本+で+なく).  Everything else here follows a nominal or an adjective
    // continuative and takes the independent adjective (休み+なく、明るく+なく).
    if (idx >= 1 && followsPredicateStem(result[idx - 1])) {
      continue;
    }
    // ではなく keeps the auxiliary reading even though the topical particle
    // separates it from the copula it negates.
    const bool follows_copular_topic = idx >= 2 && result[idx - 1].extended_pos == core::ExtendedPOS::ParticleTopic &&
                                       result[idx - 2].extended_pos == core::ExtendedPOS::AuxCopulaDa;
    if (follows_copular_topic) {
      continue;
    }
    retag(negative, core::PartOfSpeech::Adjective, core::ExtendedPOS::AdjRenyokei, "ない",
          dictionary::ConjugationType::IAdjective, grammar::ConjForm::Renyokei);
  }
}

// In the explanatory or causal pattern ADV+な+の/ので, な is the attributive
// form of the copula, not the homographic sentence-final particle
// (なぜ+な+の、せっかく+な+ので). The lattice cannot use the following
// particle while scoring the ADV→な connection, so resolve the POS once the
// complete three-token context is available.
void resolveAdverbExplanatoryCopula(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx + 1 < result.size(); ++idx) {
    auto& copula = result[idx];
    const auto& adverb = result[idx - 1];
    const auto& nominalizer = result[idx + 1];
    const bool explanatory_no =
        nominalizer.surface == "の" && nominalizer.extended_pos == core::ExtendedPOS::ParticleNo;
    const bool causal_node =
        nominalizer.surface == "ので" && nominalizer.extended_pos == core::ExtendedPOS::ParticleConj;
    if (adverb.pos != core::PartOfSpeech::Adverb || copula.surface != "な" ||
        copula.pos != core::PartOfSpeech::Particle || (!explanatory_no && !causal_node)) {
      continue;
    }
    retagCopulaDa(copula);
  }
}

// Formal よう remains a noun in similitude/purpose constructions
// (読む+よう+だ, 次+の+よう+に, この+よう+な).  The lattice already
// distinguishes it from the true volitional auxiliary (見+よう).  Only the
// homographic following な may still need its copular role restored here.
void resolveSimilitudeYou(std::vector<core::Morpheme>& result) {
  // The polite auxiliary is already finite, so a following よう cannot be the
  // homographic volitional ending. It is the formal noun introducing a wished-
  // for state (お待ちくださいます+よう+お願いします).
  for (size_t idx = 1; idx < result.size(); ++idx) {
    const auto& polite = result[idx - 1];
    auto& purpose = result[idx];
    if (polite.extended_pos != core::ExtendedPOS::AuxTenseMasu || purpose.surface != "よう" ||
        purpose.extended_pos != core::ExtendedPOS::AuxVolitional) {
      continue;
    }
    retag(purpose, core::PartOfSpeech::Noun, core::ExtendedPOS::NounFormal, "よう", dictionary::ConjugationType::None,
          grammar::ConjForm::Base);
    purpose.flags = purpose.flags | core::EdgeFlags::IsFormalNoun;
  }

  for (size_t idx = 0; idx + 1 < result.size(); ++idx) {
    auto& similitude = result[idx];
    auto& next = result[idx + 1];
    if (similitude.surface != "よう" || similitude.extended_pos != core::ExtendedPOS::NounFormal) {
      continue;
    }
    const bool attributive_na_follows = next.surface == "な" && next.pos == core::PartOfSpeech::Particle;
    if (attributive_na_follows) {
      retagCopulaDa(next);
    }
  }
}

}  // namespace suzume::postprocess::resolver
