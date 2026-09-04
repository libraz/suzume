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

// Negative auxiliaries (ない/ず/ね) and noun↔short-verb-renyokei disambiguation.
float computeNegativeAndNounVerbBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  float bonus{};  // value-init to 0

  // A volitional auxiliary is selected by an irrealis, so the vowel row its
  // host ends on decides whether the host has reached the cell that carries
  // it. Two spellings are gated, both because the row is the only evidence
  // available:
  //   - ん after the polite auxiliary. The e-row cell selects contracted
  //     negation (ませ+ん), and the homographic literary volitional needs a
  //     lexical irrealis stem instead; the o-row cell (ましょ+う) is untouched.
  //   - the one-mora う. Every paradigm spells the irrealis it follows on the
  //     o-row (書こ+う, だろ+う, でしょ+う, ましょ+う, なかろ+う, たろ+う), so a
  //     host ending elsewhere has had the う cut out of the word behind it
  //     (読ん+だ+う+え for 読ん+だ+うえ). The terminal copula shares its
  //     ExtendedPOS with its own irrealis, which is how the だろ+う bonus
  //     reached だ+う. The two-mora よう selects the ichidan and sa-hen stems
  //     and is not gated here.
  const bool volitional_host_row_mismatch =
      next.extended_pos == core::ExtendedPOS::AuxVolitional &&
      ((prev.extended_pos == core::ExtendedPOS::AuxTenseMasu && grammar::endsWithERow(prev.surface) &&
        grammar::isSingleHiragana(next.surface, core::hiragana::kN)) ||
       (grammar::isSingleHiragana(next.surface, U'う') &&
        grammar::getVowelForChar(utf8::decodeLastChar(prev.surface)) != U'お'));
  // The classical/contracted negative ん cannot be followed by the plain
  // copula だ. In an apparent …んだ sequence after a ma/ba/na-row verb, ん is
  // the verb's hatsuonbin and だ is the past auxiliary (膨らん+だ).
  const bool contracted_negative_before_copula =
      prev.extended_pos == core::ExtendedPOS::AuxNegativeNu && grammar::isSingleHiragana(prev.surface, U'ん') &&
      next.extended_pos == core::ExtendedPOS::AuxCopulaDa && grammar::isSingleHiragana(next.surface, U'だ');

  // A shape-verified mimetic adverb may follow a case/topic-marked nominal
  // directly (水滴が+ぽつり, 地図を+じっくり). Prefer that reading over
  // unrelated short verb and auxiliary fragments inside the mimetic.
  const bool marked_nominal_before_mimetic =
      (prev.extended_pos == core::ExtendedPOS::ParticleCase || prev.extended_pos == core::ExtendedPOS::ParticleTopic) &&
      prev.start > 0 && next.pos == core::PartOfSpeech::Adverb && next.origin == core::CandidateOrigin::Onomatopoeia;

  // A multi-character nominal compound ending in 中 can modify a predicate or
  // take a case particle as one searchable unit. Keep that reading ahead of
  // a stem-plus-suffix path; short two-character temporal forms remain split.
  const bool long_chuu_nominal =
      prev.extended_pos == core::ExtendedPOS::Noun && utf8::endsWith(prev.surface, "中") &&
      prev.surface.size() >= core::kThreeJapaneseCharBytes &&
      (next.extended_pos == core::ExtendedPOS::ParticleCase || next.extended_pos == core::ExtendedPOS::VerbShuushikei);
  // The exclusive しか construction uses the closed negative auxiliary in
  // the oracle contract. Other binding particles (さえ/すら/こそ) retain the
  // existential adjective ない, so this is deliberately surface-specific.
  const bool exclusive_binding_negative = prev.extended_pos == core::ExtendedPOS::ParticleBinding &&
                                          utf8::equalsAny(prev.surface, {"しか"}) &&
                                          next.extended_pos == core::ExtendedPOS::AuxNegativeNai;
  if (volitional_host_row_mismatch)
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  if (contracted_negative_before_copula)
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  if (marked_nominal_before_mimetic)
    SUZUME_CONNECTION_ADD(bonus, cost::kDoubleVeryStrongBonus);
  if (long_chuu_nominal)
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus + cost::kModerateBonus);
  if (exclusive_binding_negative)
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);

  // A topic particle cannot directly select the classical negative auxiliary.
  // In sequences such as 〜たはずだ, the apparent は+ず boundary is the formal
  // noun はず, not a predicate followed by negation.
  // Ichidan stems are both continuative and irrealis.  After the suru
  // continuative, their irrealis reading is licensed before a negative
  // auxiliary (確認し+終え+ない) rather than an unrelated unknown noun.
  const bool classical_negative_suru = prev.extended_pos == core::ExtendedPOS::AuxNegativeNu &&
                                       next.extended_pos == core::ExtendedPOS::VerbRenyokei && next.lemma == "する";
  const bool topic_before_classical_negative =
      prev.extended_pos == core::ExtendedPOS::ParticleTopic && next.extended_pos == core::ExtendedPOS::AuxNegativeNu;
  const bool sahen_ichidan_irrealis = prev.extended_pos == core::ExtendedPOS::VerbRenyokei && prev.lemma == "する" &&
                                      next.extended_pos == core::ExtendedPOS::VerbMizenkei &&
                                      next.conj_type == dictionary::ConjugationType::Ichidan;
  if (classical_negative_suru)
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  if (topic_before_classical_negative)
    SUZUME_CONNECTION_ADD(bonus, cost::kProhibitive);
  if (sahen_ichidan_irrealis)
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);

  // The copula's conjunctive form is で; an emphatic small-tsu variant cannot
  // take connective て/で. In なっ+て the onbin belongs to lexical なる, so
  // prevent a generated emphatic AuxCopulaDa edge from replacing that verb.
  if (prev.extended_pos == core::ExtendedPOS::AuxCopulaDa && utf8::endsWith(prev.surface, "っ") &&
      next.extended_pos == core::ExtendedPOS::ParticleConj && grammar::isTeDeSurface(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // Bonus for dict VERB_連用 → ない/なく/なかっ/なけれ (negative auxiliary)
  // VERB→ADJ bigram (0.8) is high, making split path lose to merged candidates
  // E.g., でき+なく should beat できなく, し+なく should beat しなく
  // Restrict to dictionary verbs (間違い+ない uses 間違い(NOUN), not 違い(VERB))
  // Exclude で (ambiguous: 出る VERB vs だ copula AUX → でない misanalysis)
  // Exclude godan mizenkei (a-dan ending): 走ら, 書か are mislabeled as VERB_連用
  // but are actually 未然形 — bonus would incorrectly boost 走ら+ない split
  const bool dictionary_renyokei_negative =
      prev.pos == core::PartOfSpeech::Verb && prev.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      prev.fromDictionary() && prev.surface != "で" && !grammar::endsWithARow(prev.surface) &&
      (next.pos == core::PartOfSpeech::Adjective || next.pos == core::PartOfSpeech::Auxiliary) &&
      utf8::equalsAny(next.surface, {"なく", "ない", "なかっ", "なけれ"});
  const bool adjective_conditional_negative = prev.extended_pos == core::ExtendedPOS::AdjRenyokei &&
                                              next.extended_pos == core::ExtendedPOS::AuxNegativeNai &&
                                              utf8::equalsAny(next.surface, {"なけれ"});
  if (dictionary_renyokei_negative || adjective_conditional_negative) {
    // The ExtendedPOS bigram already provides the primary grammatical
    // preference.  Keep this lexical tie-break modest: a full strong bonus
    // lets a verbal homograph erase an equally attested formal-noun reading.
    SUZUME_CONNECTION_ADD(bonus, cost::kMinorBonus);
  }

  // Bonus for ば(PART_接続) → なら/なり/なる/なれ(VERB) in -なければならない pattern
  // Prevents spurious ばなら verb candidate (ばなる godan-ra) from winning
  // over correct split ば(conditional) + なら(なる mizenkei)
  if (prev.extended_pos == core::ExtendedPOS::ParticleConj && prev.surface == "ば" &&
      next.pos == core::PartOfSpeech::Verb && utf8::equalsAny(next.surface, {"なら", "なり", "なる", "なれ", "なっ"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // Contracted obligation chains retain their grammatical boundaries:
  // mizenkei + なく + ちゃ + いけ + ない, and
  // mizenkei + なきゃ/なけりゃ + なら + ない.
  if (prev.extended_pos == core::ExtendedPOS::AuxNegativeNai && utf8::equalsAny(prev.surface, {"なく"}) &&
      next.extended_pos == core::ExtendedPOS::ParticleConj && utf8::equalsAny(next.surface, {"ちゃ"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // The change-of-state construction 〜なくなる retains なく as a negative
  // auxiliary; elsewhere before a connective, the competing adjective form
  // remains appropriate (読まれなくて困る).
  if (prev.extended_pos == core::ExtendedPOS::AuxNegativeNai && utf8::equalsAny(prev.surface, {"なく"}) &&
      next.extended_pos == core::ExtendedPOS::VerbShuushikei && utf8::equalsAny(next.surface, {"なる"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kExtremeBonus + cost::kMinorBonus);
  }

  // The case particle まで attaches directly to a terminal predicate
  // (食べるまで, 調べるまで). Keep this boundary before the particle's
  // leading mora can be absorbed into an unknown nominal candidate.
  if (prev.extended_pos == core::ExtendedPOS::VerbShuushikei && next.extended_pos == core::ExtendedPOS::ParticleCase &&
      utf8::equalsAny(next.surface, {"まで"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }
  if (prev.extended_pos == core::ExtendedPOS::AuxNegativeNai &&
      grammar::isColloquialConditionalNegativeSurface(prev.surface) &&
      next.extended_pos == core::ExtendedPOS::VerbMizenkei && utf8::equalsAny(next.surface, {"なら"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }
  if (prev.extended_pos == core::ExtendedPOS::AuxNegativeNai &&
      grammar::isColloquialConditionalNegativeSurface(prev.surface) &&
      next.extended_pos == core::ExtendedPOS::AuxPotential && utf8::equalsAny(next.surface, {"いけ"})) {
    // The lexical いける renyokei has a low candidate cost before ない. Give
    // this closed obligation connection a small additional preference so its
    // auxiliary analysis wins only after the contracted negative conditional.
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus + cost::kMinorBonus);
  }
  // The same obligation predicate follows the formal negative conditional
  // (なけれ+ば+いけ+ない) and the te-form topic construction
  // (なく+て+は+いけ+ない).  In either case the preceding closed-class
  // particle identifies the auxiliary reading over lexical いける.
  if ((prev.extended_pos == core::ExtendedPOS::ParticleConj && utf8::equalsAny(prev.surface, {"ば"})) ||
      (prev.extended_pos == core::ExtendedPOS::ParticleTopic && utf8::equalsAny(prev.surface, {"は"}))) {
    if (next.extended_pos == core::ExtendedPOS::AuxPotential && utf8::equalsAny(next.surface, {"いけ"})) {
      SUZUME_CONNECTION_ADD(bonus, cost::kDoubleVeryStrongBonus);
    }
  }
  // じゃ is the voiced member of the same contracted pair as ちゃ (読ん+じゃ+いけ+ない).
  if (prev.extended_pos == core::ExtendedPOS::ParticleConj && utf8::equalsAny(prev.surface, {"ちゃ", "じゃ"}) &&
      ((next.extended_pos == core::ExtendedPOS::AuxPotential && utf8::equalsAny(next.surface, {"いけ"})) ||
       (next.extended_pos == core::ExtendedPOS::VerbMizenkei && utf8::equalsAny(next.surface, {"なら"})))) {
    SUZUME_CONNECTION_ADD(bonus, cost::kDoubleVeryStrongBonus);
  }

  // Bonus for VERB_未然 → AUX_否定古(ず/ずに/ね) connection
  // Godan mizenkei + classical negative: 書かず, 抜かず, 行かず, 行かねば
  // The split path needs help to beat merged verb candidates (書かずに as single VERB)
  // because AUX_否定古 → next token connections have default (high) cost.
  // ね is the 已然形 of the same classical negative (行かねば, 死なねば) and competes
  // with the dict VERB reading of ね (連用形 of ねる=寝る) and the sentence-final
  // particle ね; this bonus is what lets the AUX reading win after a verb mizenkei.
  // Note: lexicalized forms like 思わず(ADV) are handled by the candidate generator
  // which skips mizenkei_zu generation when verb+ず is in the dictionary.
  if (prev.extended_pos == core::ExtendedPOS::VerbMizenkei && next.extended_pos == core::ExtendedPOS::AuxNegativeNu &&
      utf8::equalsAny(next.surface, {"ず", "ずに", "ざる", "ざれ", "ね"})) {
    SUZUME_CONNECTION_ADD(bonus,
                          utf8::endsWith(next.surface, "に") ? cost::kDoubleVeryStrongBonus : cost::kStrongBonus);
  }

  // Cancel the ichidan-oriented VerbRenyokei → AuxNegativeNu(ね) bonus (消えぬ pattern)
  // when prev is not a genuine renyokei/mizenkei form (i.e., doesn't end in an
  // i-row/e-row hiragana). Some godan verbs get a spurious VerbRenyokei-tagged
  // candidate for their dictionary shuushikei form (e.g., 行く), which would
  // otherwise hijack ね away from the sentence-final particle reading (行くね).
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.extended_pos == core::ExtendedPOS::AuxNegativeNu &&
      next.surface == "ね" && !grammar::endsWithRenyokeiMarker(prev.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kRare);
  }

  // Bonus for AUX_否定古(ずに) → VERB connection
  // ずに+帰る, ずに+済む etc. are natural patterns
  // Without this, split path ず+に+帰る wins due to PART_格→VERB having lower default cost
  if (prev.extended_pos == core::ExtendedPOS::AuxNegativeNu && prev.surface == "ずに" &&
      (next.pos == core::PartOfSpeech::Verb || next.pos == core::PartOfSpeech::Adjective)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // A nominalized predicate can attach to the continuative form of する.
  // This includes productive honorific-prefix constructions and ordinary
  // verbal-noun predicates, so prefer it over an unrelated lexical verb chain.
  if (prev.extended_pos == core::ExtendedPOS::Noun && next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      grammar::isSuruRenyokeiSurface(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus + cost::kMinorBonus);
  }

  // A derivational suffix can form the nominal base of する (重要+視+する,
  // 安定+化+する). Preserve the productive suffix boundary over a fused
  // unknown noun before the regular suru continuative form.
  if (prev.pos == core::PartOfSpeech::Suffix && next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      grammar::isSuruRenyokeiSurface(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // An interrogative pronoun directly preceding the continuative し forms a
  // productive question predicate (何+し+てる). This is an exception to the
  // general ban on interrogative-pronoun-to-verb attachment.
  if (prev.extended_pos == core::ExtendedPOS::PronounInterrogative &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei && grammar::isSuruRenyokeiSurface(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kDoubleVeryStrongBonus);
  }

  // Interrogative pronouns take the comparative case particle directly
  // (なに+より), rather than allowing a longer formal particle to absorb its
  // final syllable.
  if (prev.extended_pos == core::ExtendedPOS::PronounInterrogative && utf8::equalsAny(prev.surface, {"なに"}) &&
      next.extended_pos == core::ExtendedPOS::ParticleCase && utf8::equalsAny(next.surface, {"より"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kDoubleVeryStrongBonus);
  }

  // Surface-based penalty for Noun → short VerbRenyokei (compound verb protection)
  // Bigram table gives bonus for Noun→VerbRenyokei (for サ変動詞: 得+し, 損+し)
  // But this should NOT apply to compound verbs like 見+つけ→見つけ
  // E.g., 勘違い should be single token, not 勘違+い
  // E.g., 見つけた should be 見つけ+た, not 見+つけ+た
  // Exception: multi-kanji noun + でき should split (外出+でき+ない)
  // Single kanji NOUN often forms compound verbs with following verb stems
  // A continuative written with its own okurigana is a complete content word,
  // so a bare noun before it is a separate token (花+散り, 飯+食べ). A bare kanji
  // carries no such evidence and stays inside the compound (出+来 in 出来事).
  // That argument fails for one okurigana: し also completes the サ変 predicate of
  // the whole kanji run, so a productively guessed godan-sa continuative competes
  // with the kango's own reading instead of standing as a content word (一致した is
  // not 一+致し+た, 合致した not 合+致し+た). Attested verbs keep the exemption, as
  // do the other rows, whose okurigana is unambiguous (花+散り, 飯+食べ).
  const bool guessed_sahen_ambiguous_renyokei = utf8::endsWith(next.surface, "し") && !next.lemmaVerified();
  const bool renyokei_has_okurigana =
      grammar::startsWithKanji(next.surface) && !grammar::isAllKanji(next.surface) && !guessed_sahen_ambiguous_renyokei;
  // A longer kanji run gets the same treatment when the continuative is a single
  // mora of hiragana, because that is the shape of a run that swallowed the verb's
  // own kanji stem (三枚重+ね for 三枚+重ね). A genuine one-mora ichidan stem writes
  // that mora in kanji after kanji material (毎日+寝+て), so nothing legitimate has
  // this shape. Longer hiragana continuatives keep the exemption (外出+でき+ない).
  const bool bare_kanji_host = normalize::utf8Length(prev.surface) == 1 ||
                               (grammar::isAllKanji(prev.surface) && isSingleHiraganaVerbRenyokei(next));
  if (prev.extended_pos == core::ExtendedPOS::Noun && next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      !grammar::isSuruRenyokeiSurface(next.surface) && next.surface != "せ" && next.surface.size() <= 6 &&
      bare_kanji_host && !renyokei_has_okurigana) {
    SUZUME_CONNECTION_ADD(bonus, cost::kRare);  // Cancel the bigram bonus
  }

  // Penalty for Noun/ナ形容詞 → い (VerbRenyokei of いる); mirrors the
  // Noun→AuxAspectIru bigram severity so both readings of a bare-noun-plus-い
  // are rejected (彼が+いる needs a particle; 間続+い beaten by 間+続い).
  // E.g., 上手いし should be 上手い+し, not 上手+い+し. Must NOT block サ変+でき (外出+でき).
  if ((prev.extended_pos == core::ExtendedPOS::AdjNaAdj || prev.extended_pos == core::ExtendedPOS::Noun) &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei && next.surface == "い") {
    SUZUME_CONNECTION_ADD(bonus, cost::kSevere);
  }

  // Partial cancel for single-kanji NOUN + し pattern
  // E.g., 寒し (archaic adjective) should not split as 寒+し
  // But 得+し (suru-verb renyokei) should still split
  if (prev.extended_pos == core::ExtendedPOS::Noun && next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      grammar::isSuruRenyokeiSurface(next.surface) && normalize::utf8Length(prev.surface) == 1) {
    SUZUME_CONNECTION_ADD(bonus, cost::kUncommon);
  }

  return bonus;
}

}  // namespace suzume::analysis::connection_rules
