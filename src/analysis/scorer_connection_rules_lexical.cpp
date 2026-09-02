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

namespace {

bool isRecentCompletionCompoundNounVerbal(std::string_view surface) {
  return utf8::endsWith(surface, "たて") || utf8::endsWith(surface, "済み");
}

}  // namespace

bool isSingleHiraganaVerbRenyokei(const core::LatticeEdge& edge) {
  return edge.extended_pos == core::ExtendedPOS::VerbRenyokei && normalize::utf8Length(edge.surface) == 1 &&
         grammar::isPureHiragana(edge.surface);
}

float computeAdjectiveDerivationHostPenalty(const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  // The productive observation suffix がる selects an adjective stem. A
  // lexical ordinary noun may also carry a nominalized adjectival stem
  // (不安+がる), while a formal noun or another closed class cannot host it.
  // A multi-kanji nominal has the shape of a 形容動詞語幹 (面倒, 不安, 迷惑)
  // whether or not the compact dictionary lists it, and the closed classes this
  // rule excludes are never spelled that way. Without it the run is split so
  // that がる's kana can be read as a fabricated verb's okurigana (面+倒がる).
  const bool kanji_compound_host = prev.extended_pos == core::ExtendedPOS::Noun &&
                                   normalize::utf8Length(prev.surface) >= 2 && grammar::isAllKanji(prev.surface);
  const bool dictionary_nominal_host =
      prev.extended_pos == core::ExtendedPOS::Noun && (prev.fromDictionary() || kanji_compound_host);
  if (next.extended_pos == core::ExtendedPOS::AuxGaru && prev.extended_pos != core::ExtendedPOS::AdjStem &&
      prev.extended_pos != core::ExtendedPOS::AdjNaAdj && !dictionary_nominal_host) {
    return cost::kAlmostNever;
  }

  return cost::kNeutral;
}

float computeParticleQuoteBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  float bonus{};

  // The focus particle も attaches to a complete lexical adverb or
  // conjunction (あまりに+も, だけど+も).  Its dictionary EPOS is
  // ParticleTopic, so rules for ParticleBinding cannot express this
  // connection.  Require a complete dictionary modifier on the left; noun
  // case stacks (本+に+も) and generated adjectival adverbials remain outside
  // this preference.
  if (next.extended_pos == core::ExtendedPOS::ParticleTopic && grammar::isSingleHiragana(next.surface, U'も') &&
      prev.fromDictionary() &&
      (prev.pos == core::PartOfSpeech::Adverb || prev.pos == core::PartOfSpeech::Conjunction)) {
    return cost::kMinorBonus;
  }

  // The comparative case particle follows an adverbial reference point
  // (かねて+より, 以前+より). Keep this relation ahead of its homographic
  // continuative verb without changing other adverb-to-case boundaries. An
  // adverb built on the case particle に is already case-marked and cannot host
  // a second one, so the reference point is the noun inside it instead
  // (ことに+より is こと+により).
  if (prev.pos == core::PartOfSpeech::Adverb && !utf8::endsWith(prev.surface, "に") &&
      next.extended_pos == core::ExtendedPOS::ParticleCase && utf8::equalsAny(next.surface, {"より"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // A final particle can be quoted as a complete utterance (かしら+と
  // 思う, かな+と考える). This relation is specific to the quotative case
  // particle; applying it to every case particle incorrectly favors paths
  // such as ADV+わ+から over an ordinary following predicate.
  if (prev.extended_pos == core::ExtendedPOS::ParticleFinal && next.extended_pos == core::ExtendedPOS::ParticleCase &&
      utf8::equalsAny(next.surface, {"と"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // A stack of sentence-final particles is licensed by its second member,
  // which is where the modality sits: the confirmation-seeking ね/な/よ and
  // the question か take another final particle in front of them (か+な,
  // よ+ね, わ+ね, っけ+ね, ぜ+よ, じゃん+か), while what stands first carries
  // the proposition and is unrestricted. Grant those the exemption from the
  // general final-particle-to-final-particle penalty.
  if (prev.extended_pos == core::ExtendedPOS::ParticleFinal && next.extended_pos == core::ExtendedPOS::ParticleFinal &&
      grammar::isFinalParticleStackTail(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kExtremeBonus);
  }

  return bonus;
}

float computeCompoundNominalizationBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  // A verb's continuative followed by its particle-marked deverbal noun
  // reading is a productive serial nominalization (打ち+鳴らしを). Prefer
  // that relation over reclassifying the preceding continuative as an
  // unrelated noun. The generated noun is restricted at creation time to a
  // direct particle continuation, so finite and derivational uses remain out
  // of scope.
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.extended_pos == core::ExtendedPOS::NounVerbal &&
      next.origin == core::CandidateOrigin::NominalizedNoun) {
    return cost::kModerateBonus;
  }

  // A deverbal-noun continuative followed by の heads a nominal phrase
  // (書きかけの紙, 書きたての文). Prefer the noun reading emitted alongside
  // the compound-verb edge; otherwise a verb-only bonus would incorrectly
  // discard that grammatical category while keeping the same search unit.
  if (prev.origin == core::CandidateOrigin::VerbCompound && prev.extended_pos == core::ExtendedPOS::NounVerbal &&
      next.extended_pos == core::ExtendedPOS::ParticleNo && utf8::equalsAny(next.surface, {"の"})) {
    if (isRecentCompletionCompoundNounVerbal(prev.surface)) {
      return cost::kAlmostNever;
    }
    return cost::kTripleVeryStrongBonus + cost::kModerateBonus;
  }

  // A productive renyokei+suffix nominalization before the accusative marker
  // should remain split as verb + suffix + を, rather than collapsing to a
  // closed deverbal noun.
  if (prev.origin == core::CandidateOrigin::VerbCompound && prev.extended_pos == core::ExtendedPOS::NounVerbal &&
      next.extended_pos == core::ExtendedPOS::ParticleCase && utf8::equalsAny(next.surface, {"を"})) {
    if (isRecentCompletionCompoundNounVerbal(prev.surface)) {
      return cost::kAlmostNever;
    }
  }

  // がてら attaches to a complete noun (買い物がてら、仕事がてら). A formal
  // noun after an independent renyokei is instead a competing internal split
  // of a lexical noun, so keep that path behind its complete-noun candidate.
  if (prev.extended_pos == core::ExtendedPOS::NounFormal && next.extended_pos == core::ExtendedPOS::ParticleConj &&
      utf8::equalsAny(next.surface, {"がてら"})) {
    return cost::kStrong;
  }
  return cost::kNeutral;
}

// Progressive/contracted て, dialectal やで, 付け-で formal noun, honorific
// renyokei (いたし/いただき), and い/た/だ auxiliary attachment rules.
float computeSugiFinalParticleBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  float bonus{};  // value-init to 0

  // A one-kanji noun followed by a generated multi-mora hiragana te-form is
  // normally an artificial split through that verb's okurigana (食+べて,
  // 考+えて).  Preserve the complete kanji-verb candidate; genuine verb
  // boundaries in this position begin with a content kanji or an attested
  // lexical form, rather than this unverified tail.
  if (prev.pos == core::PartOfSpeech::Noun && normalize::utf8Length(prev.surface) == 1 &&
      next.origin == core::CandidateOrigin::VerbHiragana && !next.fromDictionary() &&
      next.extended_pos == core::ExtendedPOS::VerbTeForm && next.surface.size() >= core::kTwoJapaneseCharBytes) {
    return cost::kAlmostNever;
  }

  // A standalone suru continuative is not followed directly by a degree
  // adverb.  Without this boundary, an i-adjective ending in しい can be
  // split into a fabricated noun+suru stem and an adverb beginning with い
  // (恐+し+いとも).  A real coordination uses the conjunctive-particle
  // reading of し instead.
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && grammar::isSuruRenyokeiSurface(prev.surface) &&
      next.pos == core::PartOfSpeech::Adverb) {
    return cost::kAlmostNever;
  }

  // A generated renyokei ending in ず is a fused classical-negative path.
  // Before an independent finite predicate, the productive analysis is
  // mizenkei + ず + predicate (種類を問わ|ず|進む), not a fabricated verb
  // connection. Dictionary words retain their lexical reading.
  if (!prev.fromDictionary() && prev.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      utf8::endsWith(prev.surface, "ず") && next.extended_pos == core::ExtendedPOS::VerbShuushikei) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // A dictionary noun that carries a particle-like extended POS is a surface
  // homograph, not a grammatical binding particle. Do not let it replace the
  // topic-particle boundary in nominal predicates such as 本|は|ない.
  const bool noun_before_binding_homograph = prev.pos == core::PartOfSpeech::Noun &&
                                             next.pos == core::PartOfSpeech::Noun &&
                                             next.extended_pos == core::ExtendedPOS::ParticleBinding;

  // A complete dictionary noun followed by another nominal head is a normal
  // compound-noun connection.  Do not charge it the generic unknown
  // Noun→Noun penalty: otherwise a shorter internal number/formal-noun path
  // can win merely by summing two negative lexical costs (一人+ひとり versus
  // the exact search unit 一人ひとり).
  const bool long_dictionary_noun_compound =
      prev.origin == core::CandidateOrigin::Dictionary && prev.pos == core::PartOfSpeech::Noun &&
      next.pos == core::PartOfSpeech::Noun && normalize::utf8Length(prev.surface) >= 4;
  if (noun_before_binding_homograph)
    SUZUME_CONNECTION_ADD(bonus, cost::kStrong);
  if (long_dictionary_noun_compound)
    SUZUME_CONNECTION_ADD(bonus, cost::kModerateBonus + cost::kMinorBonus);

  // A contracted negative ん cannot be followed by an independent かっ verb.
  // The colloquial past is represented by the closed auxiliary んかっ, so
  // reject the fabricated ん + かっ verb chain.
  if (prev.extended_pos == core::ExtendedPOS::AuxNegativeNu && next.extended_pos == core::ExtendedPOS::VerbOnbinkei &&
      next.surface == "かっ") {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // A generated verb-onbin candidate whose reconstructed lemma ends in ぬ
  // is a contracted negative (読まん, 書かん), not a lexical onbin form.
  // Before connective で, keep the productive mizenkei + ん + でも chain.
  if (prev.extended_pos == core::ExtendedPOS::VerbOnbinkei && utf8::endsWith(prev.lemma, "ぬ") &&
      utf8::startsWith(next.surface, "で") &&
      (next.extended_pos == core::ExtendedPOS::ParticleConj ||
       next.extended_pos == core::ExtendedPOS::ParticleAdverbial ||
       next.extended_pos == core::ExtendedPOS::Conjunction)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrong);
  }

  // Penalty for VerbOnbinkei(ん) → Verb(でる) pattern
  // After ん音便, でる is almost always the contracted ている, not the verb 出る
  // E.g., 並んでる = 並んでいる (progressive), やんでる = 病んでいる
  // Force the で(PART_接続) + る path instead
  if (prev.extended_pos == core::ExtendedPOS::VerbOnbinkei && utf8::endsWith(prev.surface, "ん") &&
      next.pos == core::PartOfSpeech::Verb && next.surface == "でる") {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrong);
  }

  // The completion auxiliary after the renyokei homograph of 出る belongs
  // to the voiced te-form chain (読ん+で+しまう), not to a lexical verb.
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && utf8::equalsAny(prev.lemma, {"出る", "でる"}) &&
      next.extended_pos == core::ExtendedPOS::AuxAspectShimau) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrong);
  }

  // The voiced progressive contraction follows an n-onbin: 読んでる,
  // 飲んでる. Outside this environment でる retains its lexical-verb reading.
  if (prev.extended_pos == core::ExtendedPOS::VerbOnbinkei && utf8::endsWith(prev.surface, "ん") &&
      next.extended_pos == core::ExtendedPOS::AuxAspectIru && next.surface == "でる") {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // The irregular potential できる can follow either a verbal noun or a
  // compound adverbial particle.  Keep it ahead of an accidental で + きる
  // split while preserving the stronger particle boundary evidence.
  if (next.pos == core::PartOfSpeech::Verb && next.surface == "できる") {
    if (prev.pos == core::PartOfSpeech::Noun) {
      SUZUME_CONNECTION_ADD(bonus, cost::kModerateBonus);
    } else if (prev.extended_pos == core::ExtendedPOS::ParticleAdverbial) {
      SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
    }
  }

  // Penalty for PARTICLE て → VerbTaForm いた pattern
  // MeCab splits て+い+た, not て+いた
  // いた as verb た-form should not follow て directly
  if (prev.extended_pos == core::ExtendedPOS::ParticleConj && prev.surface == "て" &&
      next.extended_pos == core::ExtendedPOS::VerbTaForm && next.surface == "いた") {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // Penalty for PREFIX ご → VerbRenyokei ざい pattern
  // E.g., ございます should be ござい+ます, not ご+ざい+ます
  // The prefix ご is for nouns (ご報告), not for splitting ござる
  if (prev.extended_pos == core::ExtendedPOS::Prefix && grammar::isHonorificPrefix(prev.surface) &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei && utf8::startsWith(next.surface, "ざい")) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // Surface-based bonus for AdjStem → すぎ pattern
  // E.g., 高+すぎる, 美味し+すぎた: adjective stem plus excessive auxiliary.
  // AdjStem→Verb has prohibitive penalty to prevent な+い splits
  // But AdjStem+すぎ is valid grammar (i-adjective stem + すぎる)
  // Exclude VerbTeForm (すぎて) - should split as すぎ+て
  if (prev.extended_pos == core::ExtendedPOS::AdjStem && next.extended_pos != core::ExtendedPOS::VerbTeForm &&
      utf8::startsWith(next.surface, "すぎ")) {
    // Strong bonus to overcome AdjStem→Verb prohibitive penalty
    SUZUME_CONNECTION_ADD(bonus, sc::kBonusDoubleVeryStrong);
  }

  // Surface-based bonus for AdjNaAdj → すぎ pattern
  // E.g., シンプル+すぎない, 静か+すぎる (na-adjective + sugiru)
  // NOUN→VERB_連用 has bonus from bigram table, which can beat ADJ_NA path
  // This helps dictionary ADJ_NA entries beat unknown NOUN candidates
  if (prev.extended_pos == core::ExtendedPOS::AdjNaAdj && utf8::startsWith(next.surface, "すぎ")) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // Surface-based bonus for all-kanji NOUN → すぎ pattern
  // E.g., 最高+すぎ, 贅沢+すぎ, 美人+すぎ (kanji compound + sugiru "too much")
  // Without this, multi-kanji nouns get split: 最高→最+高(ADJ_語幹)+すぎ
  // because ADJ_語幹→すぎ has a very strong surface bonus (-3.2)
  // Only apply to all-kanji surfaces (not katakana/verb renyokei)
  if (prev.pos == core::PartOfSpeech::Noun && prev.surface.size() >= 6 &&  // 2+ chars (6+ bytes)
      grammar::isAllKanji(prev.surface) && utf8::startsWith(next.surface, "すぎ")) {
    SUZUME_CONNECTION_ADD(bonus, sc::kBonusDoubleVeryStrong);
  }

  // A sokuonbin copula followed by たら uses the hypothetical form of the
  // past auxiliary: 静か+だっ+たら. The homographic conjunctive particle
  // cannot attach directly to the copula's だっ form.
  if (prev.extended_pos == core::ExtendedPOS::AuxCopulaDa && utf8::endsWith(prev.surface, "っ") &&
      next.extended_pos == core::ExtendedPOS::AuxTenseTa && utf8::equalsAny(next.surface, {"たら"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // Penalty for an unsokuonized だ/な auxiliary → ParticleFinal(ったら)
  // pattern.
  // The final particle is valid after a noun (あなた+ったら), but after the
  // copula these surfaces belong to a different inflectional boundary:
  // だっ+たら (copula conditional) or なっ+たら (なる conditional). The
  // homographic past auxiliary だ follows the same boundary after a voiced
  // onbin stem (読ん+だっ+たら).
  if ((prev.extended_pos == core::ExtendedPOS::AuxCopulaDa || prev.extended_pos == core::ExtendedPOS::AuxTenseTa) &&
      utf8::equalsAny(prev.surface, {"だ", "な"}) && next.extended_pos == core::ExtendedPOS::ParticleFinal &&
      utf8::startsWith(next.surface, "った")) {
    SUZUME_CONNECTION_ADD(bonus,
                          prev.extended_pos == core::ExtendedPOS::AuxTenseTa ? cost::kAlmostNever : cost::kStrong);
  }

  // Penalty for ParticleFinal → VerbRenyokei pattern
  // E.g., いいよね should be いい+よ+ね(PART), not いい+よ+ね(VERB 寝る)
  // Final particles (よ, な, ね, わ) are rarely followed by verb renyokei
  // The short hiragana verb ね (寝る renyokei) competes with final particle ね
  // This penalty ensures particle interpretation wins in よね, なね, etc. patterns
  if (prev.extended_pos == core::ExtendedPOS::ParticleFinal && isSingleHiraganaVerbRenyokei(next)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kSevere);
  }

  // The surface か also marks an indefinite phrase (誰か来る, 何かいる).
  // If a verb actually follows, the edge is internal and therefore cannot be
  // sentence-final. Cancel the generic final-particle-to-verb penalty for this
  // homograph while retaining it for genuine final particles よ/ね/な/わ.
  if (prev.extended_pos == core::ExtendedPOS::ParticleFinal && utf8::equalsAny(prev.surface, {"か"}) &&
      next.pos == core::PartOfSpeech::Verb && next.fromDictionary() &&
      next.extended_pos != core::ExtendedPOS::VerbMizenkei) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // The indefinite-particle rescue above cannot introduce the empty-stem
  // サ変 imperative.  A causative volitional supplies the same spelling as
  // ...か+せよ+う, but its か is a Godan irrealis and the following せ is the
  // causative auxiliary; the standalone せよ reading is licensed only at a
  // clause boundary or after its nominal host.
  if (prev.extended_pos == core::ExtendedPOS::ParticleFinal && next.extended_pos == core::ExtendedPOS::VerbMeireikei &&
      grammar::isSuruImperativeSurface(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // Penalty for pure-hiragana Conjunction → bare single-hiragana non-particle
  // A conjunction is a complete word; a following lone hiragana verb/aux/unknown
  // is never a natural continuation. When the conjunction surface is a proper
  // prefix of a longer i-adjective, this is the fragment path that must lose:
  // ただしい → ただし(CONJ)+い must lose to the ただしい adjective.
  // Particles are exempt: they legitimately form compound conjunctions
  // (されど+も, だけど+も).
  if (prev.pos == core::PartOfSpeech::Conjunction && grammar::isPureHiragana(prev.surface) &&
      next.pos != core::PartOfSpeech::Particle && grammar::isPureHiragana(next.surface) &&
      next.surface.size() <= 3) {  // Single hiragana (3 bytes)
    SUZUME_CONNECTION_ADD(bonus, cost::kNever);
  }

  return bonus;
}

/// Penalty for a bare single-character potential auxiliary (え/得, renyokei of
/// える/得る) followed by anything other than a continuation morpheme.
/// The renyokei form only occurs in chains like あり+え+ない / 解決し+得+ない /
/// あり+え+て, so a following noun/verb/symbol means the え is a fragment of a
/// longer word (いいえ → いい+え, ねえ → ね+え). The multi-character
/// shuushikei える/うる legitimately ends a clause and is exempt.
float computeBarePotentialRenyokeiPenalty(const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  float penalty{};
  if (prev.extended_pos == core::ExtendedPOS::AuxPotential && prev.surface.size() <= 3 &&  // Single character (3 bytes)
      next.pos != core::PartOfSpeech::Auxiliary && next.pos != core::PartOfSpeech::Particle &&
      next.pos != core::PartOfSpeech::Suffix) {
    penalty += cost::kSevere;
  }
  return penalty;
}

float computeCopulaConditionalBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  // The literary concessive/conditional construction であれ(ば) is the
  // continuative copula followed by the hypothetical form of ある. Favor this
  // grammatical chain over the homographic case-particle + pronoun sequence.
  const bool copula_before_hypothetical =
      prev.extended_pos == core::ExtendedPOS::AuxCopulaDa && next.extended_pos == core::ExtendedPOS::VerbKateikei;
  const bool continuative_copula = grammar::isSingleHiragana(prev.surface, U'で');
  const bool aru_hypothetical_stem = grammar::isAruHypotheticalStem(next.surface) && next.lemma == "ある";
  if (copula_before_hypothetical && continuative_copula && aru_hypothetical_stem) {
    return sc::kBonusDoubleVeryStrong;
  }
  if (copula_before_hypothetical && aru_hypothetical_stem) {
    return cost::kVeryStrongBonus;
  }
  // Preserve the former broad で+lemma=ある fallback. Noncanonical surfaces
  // receive both the generic hypothetical penalty and its compensating bonus.
  if (copula_before_hypothetical && continuative_copula && next.lemma == "ある") {
    return cost::kStrong + cost::kVeryStrongBonus;
  }
  // A copula cannot directly take an unrelated lexical hypothetical form.
  // This also keeps the known で+あれ+ば chain split rather than selecting a
  // fabricated one-token verb candidate for あれば.
  if (copula_before_hypothetical) {
    return cost::kStrong;
  }
  // In であれ, the hypothetical ある can coordinate another nominal
  // predicate (本であれ水であれ) or introduce the following predicate
  // (本であれ読む). These continuations distinguish it from the pronoun あれ.
  if (prev.extended_pos == core::ExtendedPOS::VerbKateikei && prev.lemma == "ある" &&
      (next.pos == core::PartOfSpeech::Noun || next.extended_pos == core::ExtendedPOS::VerbShuushikei)) {
    return cost::kStrongBonus;
  }
  return {};
}

float computePastConditionalVerbBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  // The sahen renyokei takes the past conditional directly in 〜としたら.
  // This is a true inflectional chain, unlike a general renyokei followed by
  // a past auxiliary, and keeps たら as AUX rather than a conjunction.
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && prev.lemma == "する" &&
      next.extended_pos == core::ExtendedPOS::AuxTenseTa && utf8::equalsAny(next.surface, {"たら"})) {
    return cost::kVeryStrongBonus;
  }

  // The conditional forms of the past auxiliary introduce a following main
  // predicate (読ん+たら+進む, 読ま+せ+ん+でし+たら+進む). They are unlike a
  // completed-past た, which must not be followed by a bare verb.
  if (prev.extended_pos == core::ExtendedPOS::AuxTenseTa && utf8::equalsAny(prev.surface, {"たら", "だら"}) &&
      next.extended_pos == core::ExtendedPOS::VerbShuushikei) {
    return sc::kBonusConditionalPredicate;
  }
  // A past conditional can introduce a negative predicate as well as an
  // ordinary verb (読ん+だら+あかん). The following auxiliary is a complete
  // predicate here, not another inflectional suffix of the past form.
  if (prev.extended_pos == core::ExtendedPOS::AuxTenseTa && utf8::equalsAny(prev.surface, {"たら", "だら"}) &&
      next.extended_pos == core::ExtendedPOS::AuxNegativeNai) {
    return cost::kDoubleVeryStrongBonus;
  }

  // The conditional たら/だら attaches to a predicate, so behind a closed-class
  // word it is the past auxiliary's own conditional cell instead: the negative's
  // onbin form takes nothing else (読ま+なかっ+たら) and the te-form carries the
  // contracted progressive (飲ん+で+たら = 飲んでいたら).
  if (next.extended_pos == core::ExtendedPOS::AuxTenseTa && utf8::equalsAny(next.surface, {"たら", "だら"}) &&
      prev.pos == core::PartOfSpeech::Particle) {
    return sc::kBonusClosedInflectionalChain;
  }
  // The negative auxiliary's onbin is itself a predicate inflection.  Its
  // following たら is therefore the past auxiliary's conditional cell even
  // before an adjective predicate (読ま+なかっ+たら+いい), where the generic
  // ParticleConj→Adjective bonus would otherwise select the homograph.
  if (prev.extended_pos == core::ExtendedPOS::AuxNegativeNai && utf8::endsWith(prev.surface, "かっ") &&
      next.extended_pos == core::ExtendedPOS::AuxTenseTa && utf8::equalsAny(next.surface, {"たら", "だら"})) {
    return cost::kVeryStrongBonus;
  }
  // The negative's own onbin form admits nothing but that auxiliary, so the
  // conjunctive reading of the same kana cannot stand behind it.
  if (prev.extended_pos == core::ExtendedPOS::AuxNegativeNai && utf8::endsWith(prev.surface, "かっ") &&
      next.extended_pos == core::ExtendedPOS::ParticleConj) {
    return cost::kSevere;
  }

  // The concessive たって/だって is the past auxiliary plus って (書い+た+って,
  // 読ん+だ+って). Keep that boundary: without it the っ is read back into the
  // stem as a second onbin (書いたっ+て) or the whole tail becomes the adverbial
  // particle だって.
  if (prev.extended_pos == core::ExtendedPOS::AuxTenseTa && utf8::equalsAny(prev.surface, {"た", "だ"}) &&
      next.extended_pos == core::ExtendedPOS::ParticleQuote) {
    return cost::kVeryStrongBonus;
  }
  return {};
}

float computeExistentialAruNominalPredicateBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  // A bare noun can directly predicate the existential verb in literary and
  // fixed constructions (本あってこそ, 本あれば, 本ある限り). This is distinct
  // from the copular である sequence, whose preceding morpheme is auxiliary.
  if (prev.extended_pos == core::ExtendedPOS::Noun && next.pos == core::PartOfSpeech::Verb && next.fromDictionary() &&
      next.lemma == "ある") {
    // The generic noun-to-short-verb guards intentionally resist an unmarked
    // object+verb split. A dictionary-backed existential form is the
    // grammatical exception, so cancel those guards only for this paradigm.
    return cost::kDoubleVeryStrongBonus + cost::kStrongBonus;
  }
  return {};
}

float computeCompletionAuxiliaryBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  // A lexical renyokei can productively precede another lexical predicate in
  // a compound or coordinate verb (読み+終わら+ない, 聞き+逃さ+ない).  Keep the
  // search-unit boundary between the two verbs, but do not make the generated
  // mixed-script mizenkei lose to a spurious noun + one-mora verb split.
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.extended_pos == core::ExtendedPOS::VerbMizenkei &&
      grammar::containsKanji(next.surface) && next.surface.size() >= core::kTwoJapaneseCharBytes) {
    // Cancel the generic rare verb-to-verb transition without overwhelming a
    // complete joined compound edge when one is independently generated.
    return cost::kVeryStrongBonus;
  }
  return {};
}

float computeAdjectiveTePredicatePenalty(const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  // An i-adjective candidate ending in て/で cannot directly govern a new
  // lexical predicate. The final mora is the connective particle and must be
  // separated (嬉しく|て|なら|ない, 高く|て|なる). Auxiliary continuations are
  // represented by their own extended POS and are intentionally unaffected.
  if (prev.extended_pos == core::ExtendedPOS::AdjRenyokei &&
      (utf8::endsWith(prev.surface, "て") || utf8::endsWith(prev.surface, "で")) &&
      (next.extended_pos == core::ExtendedPOS::VerbMizenkei || next.extended_pos == core::ExtendedPOS::ParticleConj)) {
    return cost::kAlmostNever;
  }
  return {};
}

float computeClassicalNegativeBoundaryPenalty(const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  // A renyokei candidate ending in ぬ before a formal noun is a false fused
  // negative. The productive analysis is mizenkei + ぬ + formal noun.
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && utf8::endsWith(prev.surface, "ぬ") &&
      next.extended_pos == core::ExtendedPOS::NounFormal) {
    return cost::kStrong;
  }
  return {};
}

}  // namespace suzume::analysis::connection_rules
