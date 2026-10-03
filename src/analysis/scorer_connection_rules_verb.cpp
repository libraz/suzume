#include <cmath>

#include "analysis/bigram_table.h"
#include "analysis/category_cost.h"
#include "analysis/scorer.h"
#include "analysis/scorer_connection_rules.h"
#include "analysis/scorer_connection_rules_internal.h"
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

// Past-tense た/たり, でした copula, た→ら, and volitional う rules.
float computeTaFormVolitionalBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  float bonus{};  // value-init to 0

  // Surface-based bonus for VerbRenyokei → た/たら (ichidan/irregular
  // past and conditional-past forms). E.g., 食べ+た, 見+たら.
  // Guard: require kanji or dictionary-attested lexical evidence.  A generated
  // pure-hiragana Ichidan stem is also usable when it starts after an observed
  // token boundary: the following て/た validates its inflectional shape while
  // the left context keeps a sentence-initial compound from being split into a
  // fabricated lemma plus the past auxiliary.
  const bool lexical_renyokei_past =
      prev.extended_pos == core::ExtendedPOS::VerbRenyokei && utf8::equalsAny(next.surface, {"た", "たら"}) &&
      next.extended_pos == core::ExtendedPOS::AuxTenseTa &&
      (grammar::containsKanji(prev.surface) || prev.lemmaVerified() ||
       (prev.start > 0 && prev.origin == core::CandidateOrigin::VerbHiraganaInflectedRenyokei));
  const bool hiragana_onbin_past =
      prev.extended_pos == core::ExtendedPOS::VerbOnbinkei && prev.origin == core::CandidateOrigin::VerbHiragana &&
      utf8::endsWith(prev.surface, "い") && next.extended_pos == core::ExtendedPOS::AuxTenseTa;
  if (lexical_renyokei_past || hiragana_onbin_past) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // The i-onbin cell has the same closed past boundary as a dictionary
  // continuative: い+た/て resolves the ka row and い+だ/で the ga row.  Its
  // candidate is intentionally dictionary-free, so grant the boundary bonus
  // from the inflectional shape rather than lexical presence.
  // Except for s-row godan verbs (話し+た), a godan continuative form cannot
  // take the past auxiliary directly: the past attaches to its euphonic form
  // (取り→取っ+た, 書き→書い+た).  Rejecting that impossible shortcut also
  // preserves a verified compound continuative before a following て.
  const grammar::VerbType prev_verb_type = grammar::conjTypeToVerbType(prev.conj_type);
  const bool non_sa_godan_renyokei =
      (grammar::isGodanVerbType(prev_verb_type) && prev_verb_type != grammar::VerbType::GodanSa) ||
      (isGodanRenyokeiOfLemma(prev.surface, prev.lemma) && !utf8::endsWith(prev.lemma, "す"));
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.extended_pos == core::ExtendedPOS::AuxTenseTa &&
      non_sa_godan_renyokei) {
    SUZUME_CONNECTION_ADD(bonus, sc::kPenaltyIncompatibleInflection);
  }

  // An onbin form is selected precisely for て/た attachment and cannot be
  // followed directly by a case particle.  Penalize that impossible sequence
  // so a registered nominal search unit wins in overlaps such as a particle
  // followed by a noun whose suffix is also a dictionary verb form.
  if (prev.extended_pos == core::ExtendedPOS::VerbOnbinkei && next.extended_pos == core::ExtendedPOS::ParticleCase) {
    SUZUME_CONNECTION_ADD(bonus, sc::kPenaltyIncompatibleInflection);
  }

  // An euphonic cell closes on て/た, unless the following continuative is a
  // recognized subsidiary predicate.  Otherwise a short homograph can split a
  // dictionary continuative before polite ます (おっしゃい+ます).
  if (prev.extended_pos == core::ExtendedPOS::VerbOnbinkei && next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      grammar::isPureHiragana(next.surface) && !grammar::isSubsidiaryHonorificRenyokei(next.surface) &&
      !grammar::isModalSubsidiaryRenyokei(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kSevere);
  }

  // Bonus for VerbRenyokei/VerbOnbinkei → たり/だり (parallel listing particle)
  // E.g., 食べ+たり+する, 飲ん+だり+食べ+たり+する
  // Without this, た(AuxTenseTa) wins over たり(ParticleConj) due to strong た bonus
  // Accept dictionary-backed hiragana forms too (なっ+たり), while excluding
  // unknown pure-hiragana sound-symbolic forms (まっ+たり).
  if ((prev.extended_pos == core::ExtendedPOS::VerbRenyokei || prev.extended_pos == core::ExtendedPOS::VerbOnbinkei) &&
      (next.surface == "たり" || next.surface == "だり") && next.extended_pos == core::ExtendedPOS::ParticleConj &&
      (grammar::containsKanji(prev.surface) || prev.fromDictionary())) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // Penalty for godan passive/causative-passive renyokei (～Aれ for A-row) → た
  // MeCab splits these as 言わ+れ+た, not 言われ+た
  // E.g., 言われた → 言わ+れ+た, 売られた → 売ら+れ+た, 書かれた → 書か+れ+た
  // A godan passive stem is a godan mizenkei a-row mora (か/が/さ/た/な/ば/ま/ら/わ)
  // followed by れ; verbTypeFromARowCodepoint recognizes exactly that set (and
  // rejects non-godan a-row like は so 晴れた stays 晴れ+た). This cancels the
  // VerbRenyokei→た bonus for godan passive forms.
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      prev.surface.size() >= core::kTwoJapaneseCharBytes &&  // At least 2 chars (Aれ, e.g. かれ)
      utf8::endsWith(prev.surface, "れ") && next.surface == "た" &&
      next.extended_pos == core::ExtendedPOS::AuxTenseTa && grammar::containsKanji(prev.surface)) {
    const std::string_view before_re = utf8::dropLastChar(prev.surface);
    if (grammar::verbTypeFromARowCodepoint(utf8::decodeLastChar(before_re)) != grammar::VerbType::Unknown) {
      SUZUME_CONNECTION_ADD(bonus, cost::kSevere);  // Cancel VerbRenyokei→た bonus
    }
  }

  // Surface-based bonus for でし → た/たら (polite past copula / conditional)
  // 本でした should be 本+でし+た, not 本+で+し+た
  // でしたら should be でし+たら (conditional), not でし+た+ら
  // The competing path is Noun→で(PARTICLE)→し(VERB)→た with VerbRenyokei→た bonus
  if (prev.surface == "でし" && prev.extended_pos == core::ExtendedPOS::AuxCopulaDesu &&
      (next.surface == "た" || next.surface == "たら") && next.extended_pos == core::ExtendedPOS::AuxTenseTa) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // Penalty for た(AuxTenseTa) → ら(Suffix) pattern
  // This discourages splitting たら into た+ら
  // たら is a conditional form of た and should stay together
  if (prev.surface == "た" && prev.extended_pos == core::ExtendedPOS::AuxTenseTa && next.surface == "ら" &&
      next.extended_pos == core::ExtendedPOS::Suffix) {
    SUZUME_CONNECTION_ADD(bonus, cost::kSevere);  // Penalty to discourage た+ら split
  }

  // The volitional auxiliary is realized as bare う only after an o-row mizenkei
  // (書こ+う, 泳ご+う, しよ+う; the ichidan form is the 2-mora 食べ+よう). A bare
  // う after any non-o-row verb ending is impossible Japanese — an a-row mizenkei
  // (つか of つく) or a u-row shuushikei (す of する) never takes the volitional う
  // — yet the spurious split would otherwise beat the real godan-wa verb
  // (つかう/使う, あらう/洗う, すう/吸う). Penalize so the whole-verb reading wins.
  // This applies only to the bare う surface. The literary volitional ん is
  // likewise one mora, but follows an a-row irrealis stem in 読まんとする.
  const bool bare_volitional_after_non_o_row = next.extended_pos == core::ExtendedPOS::AuxVolitional &&
                                               grammar::isSingleHiragana(next.surface, core::hiragana::kU) &&
                                               prev.pos == core::PartOfSpeech::Verb &&
                                               !grammar::endsWithORow(prev.surface);

  // An o-row irrealis form followed by bare う is the productive modern
  // volitional (書こ+う, 泳ご+う, しよ+う). Prefer it over a homographic
  // continuative-form plus formal-noun path.
  const bool modern_volitional =
      next.extended_pos == core::ExtendedPOS::AuxVolitional &&
      grammar::isSingleHiragana(next.surface, core::hiragana::kU) &&
      prev.extended_pos == core::ExtendedPOS::VerbMizenkei && grammar::endsWithORow(prev.surface) &&
      prev.lemma != "いく" &&
      (grammar::isGodanVerbType(grammar::conjTypeToVerbType(prev.conj_type)) ||
       prev.conj_type == dictionary::ConjugationType::Suru || prev.lemma == "する" ||
       (prev.conj_type == dictionary::ConjugationType::Ichidan && utf8::endsWith(prev.surface, "よ")));

  // The directional subsidiary retains the same volitional boundary after a
  // connective form (読んで+いこ+う), rather than yielding to the
  // demonstrative adverb こう.
  const bool directional_volitional = prev.extended_pos == core::ExtendedPOS::AuxAspectIku &&
                                      next.extended_pos == core::ExtendedPOS::AuxVolitional &&
                                      grammar::isSingleHiragana(next.surface, core::hiragana::kU);

  // Passive auxiliaries have an explicit o-row volitional stem
  // (書か+れよ+う, 食べ+られよ+う).  Prefer that registered form over
  // reinterpreting よう as a formal noun before a final particle.
  const bool passive_volitional = prev.extended_pos == core::ExtendedPOS::AuxPassive &&
                                  utf8::endsWith(prev.surface, "よ") &&
                                  next.extended_pos == core::ExtendedPOS::AuxVolitional &&
                                  grammar::isSingleHiragana(next.surface, core::hiragana::kU);
  // The converse: a godan o-row irrealis exists only for that う, so a
  // negative cannot follow it (噛む's かも+ねえ is the particle かも+ねえ).
  const bool negative_after_godan_volitional_stem =
      (next.extended_pos == core::ExtendedPOS::AuxNegativeNai ||
       next.extended_pos == core::ExtendedPOS::AuxNegativeNu) &&
      prev.extended_pos == core::ExtendedPOS::VerbMizenkei && grammar::endsWithORow(prev.surface) &&
      grammar::isGodanVerbType(grammar::conjTypeToVerbType(prev.conj_type));
  if (bare_volitional_after_non_o_row || negative_after_godan_volitional_stem)
    SUZUME_CONNECTION_ADD(bonus, cost::kSevere);
  if (modern_volitional)
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus + cost::kStrongBonus);
  if (directional_volitional)
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  if (passive_volitional)
    SUZUME_CONNECTION_ADD(bonus, cost::kDoubleVeryStrongBonus);

  // The literary volitional む selects an irrealis form, never a
  // continuative. Reject a fabricated kanji+し renyokei before む while
  // retaining the licensed 読ま+む-style mizenkei boundary.
  if (next.extended_pos == core::ExtendedPOS::AuxVolitional && utf8::equalsAny(next.surface, {"む"}) &&
      prev.extended_pos == core::ExtendedPOS::VerbRenyokei) {
    SUZUME_CONNECTION_ADD(bonus, cost::kSevere);
  }

  return bonus;
}

// Suffix (さ/kanji-suffix) splits, ADV→でも/だけど, and short verb-renyokei→aux rules.

}  // namespace suzume::analysis::connection_rules
