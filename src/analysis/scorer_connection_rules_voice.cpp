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
#include "grammar/conjugation.h"
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

// Passive/humble attachment, youon boundary, plural ら, らし→い, and suru-verb
// causative/mizenkei/imperative (させ, さ, せよ/しろ) rules.
float computePassiveCausativeBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  float bonus{};  // value-init to 0

  // A quotative と can introduce the continuative stem of an Ichidan verb
  // immediately before passive/potential られる (本と+み+られる). This
  // origin is emitted only by the productive Ichidan-plus-passive candidate
  // generator, so it does not weaken the protected と+いう determiner path.
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase &&
      grammar::isSingleHiragana(prev.surface, core::hiragana::kTo) &&
      next.origin == core::CandidateOrigin::VerbHiraganaPassiveRenyokei) {
    SUZUME_CONNECTION_ADD(bonus, cost::kDoubleVeryStrongBonus + cost::kVeryStrongBonus);
  }

  // The quote-licensed stem must connect to passive られる rather than be
  // treated as a standalone verb.  Its specialized origin is assigned only
  // when the generator observes the preceding quotative と.
  if (prev.origin == core::CandidateOrigin::VerbHiraganaPassiveRenyokei &&
      next.extended_pos == core::ExtendedPOS::AuxPassive) {
    SUZUME_CONNECTION_ADD(bonus, cost::kDoubleVeryStrongBonus + cost::kVeryStrongBonus);
  }

  // The カ変未然形 来ら takes the potential/passive auxiliary directly
  // (来ら+れる). Its full lexical form competes with this decomposition, so
  // preserve the inflectional boundary once the irregular lemma is known.
  if (prev.extended_pos == core::ExtendedPOS::VerbMizenkei && grammar::isKuruKanjiBaseForm(prev.lemma) &&
      next.extended_pos == core::ExtendedPOS::AuxPassive && utf8::equalsAny(next.surface, {"れる"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kDoubleVeryStrongBonus);
  }

  // A verb mizenkei connects to inflectional auxiliaries (ない, れる, せる,
  // よう), not directly to an adjective. Without this grammatical guard,
  // passive + desire chains can fabricate an i-adjective spanning both
  // auxiliaries: 行か+れたく instead of 行か+れ+たく.
  if (prev.extended_pos == core::ExtendedPOS::VerbMizenkei && next.pos == core::PartOfSpeech::Adjective) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // The サ変 irrealis is split across cells: さ exists solely to host a voice
  // auxiliary (さ+せる, さ+れる) while せ/しよ carry the rest of the paradigm
  // (せ+ず, しよ+う). Stating the さ cell's requirement positively — its
  // follower must be a voice auxiliary — subsumes the connective-particle and
  // aspectual rejections for that cell, and keeps a lone さ between a nominal
  // host and an independent predicate from reading as する (紙さ書く). The
  // other cells reject the connective and the aspectual, whose own host is the
  // continuative し (し+て, し+てる). The hypothetical particles are the one
  // exception: they select an irrealis, and this is the cell that spells it
  // (話を+せ+ば, 恋+せ+ども), so barring them would leave that construction with
  // no reading at all.
  if (prev.extended_pos == core::ExtendedPOS::VerbMizenkei && grammar::isSuruBaseForm(prev.lemma)) {
    const bool follows_voice_auxiliary =
        next.extended_pos == core::ExtendedPOS::AuxCausative || next.extended_pos == core::ExtendedPOS::AuxPassive;
    const bool follows_rejected_host = (next.extended_pos == core::ExtendedPOS::ParticleConj &&
                                        !grammar::isHypotheticalSelectingConjunctiveParticle(next.surface)) ||
                                       next.extended_pos == core::ExtendedPOS::AuxAspectIru;
    const bool is_voice_only_cell = grammar::isSingleHiragana(prev.surface, U'さ');
    if (is_voice_only_cell ? !follows_voice_auxiliary : follows_rejected_host) {
      SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
    }
  }

  // A compound candidate emitted directly in mizenkei has already verified
  // both the V1 renyokei and the V2 conjugation (誘い合わ+せる).  Preserve that
  // closed inflectional chain ahead of the surface-identical alternative that
  // splits before a dictionary verb ending in せる (誘い+合わせる).  The
  // specialized origin keeps ordinary lexical verb sequences unaffected.
  if (prev.origin == core::CandidateOrigin::VerbCompound && prev.extended_pos == core::ExtendedPOS::VerbMizenkei &&
      next.extended_pos == core::ExtendedPOS::AuxCausative) {
    return cost::kStrongBonus;
  }

  // The classical causative す attaches to an a-row irrealis stem
  // (いら+し+て). A non-a-row homograph cannot supply that inflectional
  // context, so it must not create a fabricated voice chain such as
  // かも+し+れ+ない.
  if (next.extended_pos == core::ExtendedPOS::AuxCausative && grammar::isClassicalCausativeAuxiliaryLemma(next.lemma) &&
      (prev.extended_pos != core::ExtendedPOS::VerbMizenkei || !grammar::endsWithARow(prev.surface))) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // A causative auxiliary may be followed by another auxiliary (notably the
  // passive in 書か+せ+られる), but it does not open a lexical word of its own.
  // No verb attaches to it directly, and a nominal attaches only to its
  // attributive cell (行か+せる+人): the irrealis/continuative stem せ/させ is an
  // unfinished predicate, so a noun behind it means the split landed inside a
  // lexical word (合わ+せ+技 for 合わせ技).
  if (prev.extended_pos == core::ExtendedPOS::AuxCausative) {
    const bool opens_lexical_verb =
        next.pos == core::PartOfSpeech::Verb && !grammar::isHumbleHonorificLemma(next.lemma);
    const bool modifies_nominal_from_stem = next.pos == core::PartOfSpeech::Noun && !utf8::endsWith(prev.surface, "る");
    if (opens_lexical_verb || modifies_nominal_from_stem) {
      SUZUME_CONNECTION_ADD(bonus, cost::kRare);
    }
  }

  // The conditional form of a causative auxiliary attaches directly to ば:
  // 読ま+せれ+ば, 食べ+させれ+ば. Prefer the inflected auxiliary over a
  // spurious causative-plus-passive chain (せ+れ+ば).
  if (prev.extended_pos == core::ExtendedPOS::AuxCausative && utf8::endsWith(prev.surface, "れ") &&
      next.extended_pos == core::ExtendedPOS::ParticleConj && utf8::endsWith(next.surface, "ば")) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus + cost::kModerateBonus);
  }

  // A causative auxiliary retains its boundary before the actual past marker
  // (読ま+せ+た). Restrict the bonus to た/だ so homographic colloquial
  // auxiliaries tagged AuxTenseTa do not turn し+てる into a causative chain.
  if (prev.extended_pos == core::ExtendedPOS::AuxCausative && next.extended_pos == core::ExtendedPOS::AuxTenseTa &&
      grammar::isPastMarkerTaDaSurface(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // The conditional form of a passive auxiliary also attaches directly to
  // ば (書か+れれ+ば, 食べ+られれ+ば). Its stem ends in れれ, which
  // distinguishes this inflection from the ordinary passive continuative.
  if (prev.extended_pos == core::ExtendedPOS::AuxPassive && utf8::endsWith(prev.surface, "れれ") &&
      next.extended_pos == core::ExtendedPOS::ParticleConj && utf8::endsWith(next.surface, "ば")) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus + cost::kModerateBonus + cost::kMinorBonus);
  }

  // Penalty for VerbRenyokei → れ (AuxPassive) pattern
  // The passive auxiliary れる attaches to godan 未然形 (VerbMizenkei), never to
  // 連用形; the VerbRenyokei→AuxPassive strong bonus exists for られ (ichidan/
  // kuru passive: 食べ+られ, 来+られ). A bare れ after 連用形 is a false split
  // (来れば → 来+れ+ば instead of 来れ(仮定形)+ば). Hiragana a-row endings are
  // exempt: short hiragana 未然形 carries VerbRenyokei EPOS (やら+れ+た).
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.extended_pos == core::ExtendedPOS::AuxPassive &&
      next.surface == "れ" && !grammar::endsWithARow(prev.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kRare);  // Cancel the -0.8 bonus
  }

  // An unverified verb candidate ending in the causative stem させ must
  // decompose before the passive auxiliary.  The lexical alternative stays
  // available when its surface is dictionary-backed (任せ+られ), while this
  // blocks a fabricated tail such as 確か+めさせ+られ.
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.extended_pos == core::ExtendedPOS::AuxPassive &&
      utf8::endsWith(prev.surface, "させ") && !prev.fromDictionary()) {
    return cost::kAlmostNever;
  }

  // A られる passive followed by the past marker is a closed inflectional
  // chain (知らせ+られ+た, 話しかけ+られ+た). Give that completed auxiliary
  // sequence enough weight to outrank a homographic continuative whose only
  // support is the same past marker. The bare れ auxiliary remains subject to
  // its preceding stem's inflection, so おく+れ+た cannot imitate おくれ+た.
  if (prev.extended_pos == core::ExtendedPOS::AuxPassive && next.extended_pos == core::ExtendedPOS::AuxTenseTa &&
      utf8::startsWith(prev.surface, "ら") && grammar::isPastMarkerTaDaSurface(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, sc::kBonusClosedInflectionalChain);
  }

  // A humble/honorific subsidiary may follow a connective particle, another
  // verb's renyokei/onbinkei, or a verbal noun. Favor its dictionary-confirmed
  // lemma over paths that split the subsidiary into short homographs. This
  // covers both ordinary te-form attachment and honorific-prefix constructions.
  const bool can_precede_humble_subsidiary =
      prev.pos == core::PartOfSpeech::Particle || prev.extended_pos == core::ExtendedPOS::VerbRenyokei ||
      prev.extended_pos == core::ExtendedPOS::VerbOnbinkei || prev.extended_pos == core::ExtendedPOS::AuxCausative ||
      prev.pos == core::PartOfSpeech::Noun;
  if (can_precede_humble_subsidiary && next.pos == core::PartOfSpeech::Verb &&
      grammar::isHumbleHonorificLemma(next.lemma)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus + cost::kModerateBonus);
  }

  // Penalty for splitting unknown words after youon (拗音: ょ, ゃ, ゅ)
  // Youon are always part of the preceding mora (きょ, しゃ, ちゅ)
  // Splitting after them produces invalid word boundaries
  // E.g., くしょん should stay as one token, not くしょ+ん
  // Only apply to non-dictionary tokens (dict entries like でしょ are valid boundaries)
  if (!prev.fromDictionary() && prev.pos == core::PartOfSpeech::Other) {
    std::string_view last_char = utf8::lastChar(prev.surface);
    if (grammar::isSmallKana(last_char)) {
      SUZUME_CONNECTION_ADD(bonus, cost::kStrong);
    }
  }

  // Penalty for non-pronoun → ら(SUFFIX)
  // Plural suffix ら only naturally follows pronouns (彼ら, 僕ら)
  // Without this, NOUN→SUFFIX bonus (-0.8) causes false splits (かし+ら, 自+ら)
  if (prev.pos != core::PartOfSpeech::Pronoun && next.pos == core::PartOfSpeech::Suffix && next.surface == "ら") {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrong);
  }

  // Penalty for VerbRenyokei ending in らし → い (AuxAspectIru) pattern
  // 春らしい should be 春 + らしい, not 春らし (verb) + い (auxiliary)
  // The らし ending is typically from らしい (conjecture aux), not a verb renyokei
  // Verbs ending in らし are rare (探らし from 探る is the main exception)
  // Single-kanji noun + らしい is a common pattern that should be protected
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      prev.surface.size() >= core::kTwoJapaneseCharBytes &&  // At least 2 chars (kanji + らし)
      utf8::endsWith(prev.surface, "らし") && next.extended_pos == core::ExtendedPOS::AuxAspectIru) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // Bonus for longer causative forms (させ over さ+せ, させられ over さ+せ+られ)
  // MeCab treats させる as a single causative auxiliary for ichidan verbs
  // E.g., 食べ+させ+られ+た (not 食べ+さ+せ+られ+た).  The irregular Kuru
  // form has its own L1 mizenkei connection (来さ+せ), so it is excluded.
  if ((prev.extended_pos == core::ExtendedPOS::VerbRenyokei || prev.extended_pos == core::ExtendedPOS::VerbMizenkei) &&
      !grammar::isKuruKanjiBaseForm(prev.lemma) && next.extended_pos == core::ExtendedPOS::AuxCausative &&
      utf8::startsWith(next.surface, "させ")) {
    SUZUME_CONNECTION_ADD(bonus, cost::kDoubleVeryStrongBonus);
  }

  // Penalty for Noun → AuxCausative starting with させ (サ変動詞は さ+せ に分割)
  // E.g., 勉強させる should be 勉強+さ+せる, not 勉強+させる
  // MeCab treats サ変動詞 causative as Noun + さ(suru_mizen) + せる(causative)
  // Exception: Single-kanji ichidan verb stems should connect directly to させ
  // E.g., 見させる = 見+させる (ichidan 見る), not 見+さ+せる
  if (prev.pos == core::PartOfSpeech::Noun && next.extended_pos == core::ExtendedPOS::AuxCausative &&
      utf8::startsWith(next.surface, "させ")) {
    // Check if prev is a single-kanji ichidan verb stem (見, 寝, 着, etc.)
    bool is_single_kanji_ichidan = verb_helpers::isSingleKanjiIchidanSurface(prev.surface);
    if (is_single_kanji_ichidan) {
      // Bonus for single-kanji ichidan verb → させ (見+させる, 寝+させる)
      SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
    } else {
      // Penalty for multi-kanji noun → させ (サ変動詞は さ+せ に分割)
      SUZUME_CONNECTION_ADD(bonus, cost::kVeryRare);
    }
  }

  // Bonus for Noun → the passive or volitional mizenkei of する.
  // E.g., 反映される should be 反映+さ+れる (not 反映+される)
  // MeCab treats サ変動詞 passive as Noun + さ(suru_mizen) + れる(passive)
  // This enables the split: 反映+さ+れ+ます
  // The modern volitional (実現+しよ+う) needs the same support so formal noun
  // よう cannot steal the middle of the chain.
  const bool is_suru_passive_stem = grammar::isSingleHiragana(next.surface, U'さ');
  const bool is_suru_volitional_stem =
      grammar::isSuruVolitionalStemSurface(next.surface) && grammar::isSuruBaseForm(next.lemma);
  if (prev.pos == core::PartOfSpeech::Noun && next.extended_pos == core::ExtendedPOS::VerbMizenkei &&
      (is_suru_passive_stem || is_suru_volitional_stem)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // Bonus for Noun → VerbMeireikei "せよ"/"しろ" (サ変動詞の命令形)
  // E.g., 勉強せよ → 勉強+せよ, 運動しろ → 運動+しろ
  // Without this, default-AUX char_speech candidates for せよ/しろ can beat
  // the legitimate dict VERB imperative entry. Restricted to the suru-imperative
  // surfaces so godan imperative forms (柿+食え) are not falsely boosted.
  if (prev.pos == core::PartOfSpeech::Noun && next.extended_pos == core::ExtendedPOS::VerbMeireikei &&
      grammar::isSuruImperativeSurface(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  return bonus;
}

}  // namespace suzume::analysis::connection_rules
