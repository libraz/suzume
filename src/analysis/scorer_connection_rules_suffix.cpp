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

bool startsWithCaseParticleMora(std::string_view surface) {
  switch (utf8::decodeFirstChar(surface)) {
    case U'が':
    case U'を':
    case U'に':
    case U'へ':
    case U'で':
    case U'と':
      return true;
    default:
      return false;
  }
}

}  // namespace

bool isGodanRenyokeiOfLemma(std::string_view surface, std::string_view lemma) {
  const char32_t surface_last_codepoint = utf8::decodeLastChar(surface);
  if (surface_last_codepoint == 0) {
    return false;
  }

  const std::string_view base_suffix = grammar::godanBaseSuffixFromIRow(surface_last_codepoint);
  if (base_suffix.empty()) {
    return false;
  }

  const std::string_view surface_last = utf8::lastChar(surface);
  const std::string_view lemma_last = utf8::lastChar(lemma);
  return lemma_last == base_suffix &&
         surface.substr(0, surface.size() - surface_last.size()) == lemma.substr(0, lemma.size() - lemma_last.size());
}

float computeSuffixShortVerbBonus(const core::LatticeEdge& prev, const core::LatticeEdge& next) {
  float bonus{};  // value-init to 0

  // A suffix spelling that starts with a case-particle mora can be the
  // particle plus the next predicate's first mora (雲+が+たなびく). It does
  // not earn the generic NOUN→SUFFIX bonus in that ambiguous shape; genuine
  // suffix constructions retain their lexical candidate and all other
  // noun-suffix connections keep the base preference.
  if (prev.pos == core::PartOfSpeech::Noun && next.pos == core::PartOfSpeech::Suffix &&
      normalize::utf8Length(next.surface) >= 2 && startsWithCaseParticleMora(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, sc::kPenaltyAmbiguousSuffixBoundary);
  }

  // A terminal predicate cannot directly select an unverified irrealis verb
  // beginning with the binding-particle mora も. In that environment も closes
  // the predicate and the following mora starts the next word (渡る+も+いとわ+ない).
  if (prev.extended_pos == core::ExtendedPOS::VerbShuushikei && next.extended_pos == core::ExtendedPOS::VerbMizenkei &&
      !next.fromDictionary() && utf8::startsWith(next.surface, "も")) {
    SUZUME_CONNECTION_ADD(bonus, sc::kPenaltyClosedClassBoundary);
  }

  // A conjunction may introduce a nominal phrase, but an unverified one-kanji
  // kanji noun is often only the prefix of the dictionary predicate beginning
  // at the same boundary (しかも+間違えた). Do not let the generic conjunction
  // bonus split that predicate; registered one-kanji nouns remain available.
  if (prev.extended_pos == core::ExtendedPOS::Conjunction && next.pos == core::PartOfSpeech::Noun &&
      !next.fromDictionary() && normalize::utf8Length(next.surface) == 1 && grammar::isAllKanji(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, sc::kPenaltyConjunctionInternalNoun);
  }

  // Nominalizing さ cannot attach to a verb continuative.  This blocks
  // fabricated predicates such as 静ける from exploiting the general
  // VERB_連用→SUFFIX bonus; causative さ is a Verb/Auxiliary candidate and is
  // therefore outside this rule.
  if (next.pos == core::PartOfSpeech::Suffix && grammar::isSingleHiragana(next.surface, U'さ') &&
      (prev.pos == core::PartOfSpeech::Verb || prev.origin == core::CandidateOrigin::NominalizedNoun)) {
    return cost::kProhibitive;
  }

  // A generated nominalized-noun reading of an i-row stem competes with the
  // productive verb-continuative reading before a bound suffix.  In that
  // context the latter is grammatically licensed (置き+っぱなし, 書き+方),
  // while the nominalized candidate has no independent evidence.  Keep this
  // narrowly scoped to generated candidates so dictionary nouns are unchanged.
  if (prev.origin == core::CandidateOrigin::NominalizedNoun && prev.pos == core::PartOfSpeech::Noun &&
      grammar::isIRowCodepoint(utf8::decodeLastChar(prev.surface)) && next.pos == core::PartOfSpeech::Suffix) {
    return cost::kMinor;
  }

  // The temporal suffix 中 cannot govern an accusative object.  In a noun
  // compound before を, retain the complete lexical noun (背中を) rather than
  // treating its final character as a duration suffix.
  if (prev.extended_pos == core::ExtendedPOS::Suffix && grammar::isStateDurationSuffix(prev.surface) &&
      next.extended_pos == core::ExtendedPOS::ParticleCase && grammar::isAccusativeParticleWoSurface(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrong);
  }

  // The literary formal noun いかん forms a case-marked conditional phrase
  // before a predicate (いかん+に+かかわらず). This closed nominal pattern
  // outranks the homographic いか + ん negative-verb analysis.
  if (prev.extended_pos == core::ExtendedPOS::NounFormal && prev.lemma == "いかん" &&
      next.extended_pos == core::ExtendedPOS::ParticleCase) {
    SUZUME_CONNECTION_ADD(bonus, cost::kDoubleVeryStrongBonus);
  }

  // The explanatory copula sequence のです must not be resegmented as the
  // conjunctive particle ので followed by the lexical verb す.  The latter
  // has no grammatical continuation, whereas の + です is productive after
  // any predicate.
  if (prev.extended_pos == core::ExtendedPOS::ParticleConj && utf8::equalsAny(prev.surface, {"ので"}) &&
      utf8::equalsAny(next.surface, {"す"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever + cost::kProhibitive);
  }

  // The closed polite copula です must not be reopened at its internal
  // boundary.  The first mora has particle, copula, and lexical-verb
  // homographs while す is independently a terminal verb candidate, but no
  // such shorter edge may consume only the prefix of the complete auxiliary.
  // A following も remains outside this rule (それほど+で+も+ない).
  if (grammar::formsPoliteCopulaDesu(prev.surface, next.surface)) {
    return cost::kAlmostNever;
  }

  // A closed morpheme whose host class is fixed, attached to the wrong class.
  // The sentence-final ったら attaches to a completed predicate (困ったら) and
  // never to a noun, which blocks a false parse such as 行 + ったら while leaving
  // ordinary noun-final questions (本か) untouched. A personal-address suffix is
  // the mirror image: it names its host, so an inflected predicate cannot fill
  // the slot. Without that half, 〜たん takes the continuative stem and the past
  // auxiliary with it (確認し+たん+だ+けど for 確認+し+た+ん+だ+けど), because the
  // concessive particle pays a large bonus to whatever past auxiliary precedes.
  const bool predicate_particle_on_nominal_host = prev.pos == core::PartOfSpeech::Noun &&
                                                  next.extended_pos == core::ExtendedPOS::ParticleFinal &&
                                                  utf8::equalsAny(next.surface, {"ったら"});
  const bool address_suffix_on_predicate_host =
      next.extended_pos == core::ExtendedPOS::Suffix && grammar::isPersonalAddressSuffix(next.surface) &&
      (prev.pos == core::PartOfSpeech::Verb || prev.pos == core::PartOfSpeech::Adjective ||
       prev.pos == core::PartOfSpeech::Auxiliary);
  if (predicate_particle_on_nominal_host || address_suffix_on_predicate_host) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // The fixed negative predicate ほかならない starts after a case-marked
  // nominal complement (原因に+ほかなら+ない). Keep its lexical verb stem
  // together instead of reopening the formal noun ほか before ならない.
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase && next.extended_pos == core::ExtendedPOS::VerbMizenkei &&
      next.lemma == "ほかなる") {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // The conditional allomorph たら is an auxiliary after a completed verb.
  // When another predicate follows, retain that analysis rather than the
  // homographic conjunctive-particle entry.
  if (prev.extended_pos == core::ExtendedPOS::AuxTenseTa && utf8::equalsAny(prev.surface, {"たら"}) &&
      next.pos == core::PartOfSpeech::Verb) {
    SUZUME_CONNECTION_ADD(bonus, cost::kTripleVeryStrongBonus);
  }

  // Classical focus なむ can precede a quotative と. The fused particle
  // provides a grammatical boundary that must outrank な + む auxiliaries.
  if (prev.extended_pos == core::ExtendedPOS::ParticleBinding && next.extended_pos == core::ExtendedPOS::ParticleCase &&
      utf8::equalsAny(prev.surface, {"なむ"}) && utf8::equalsAny(next.surface, {"と"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kTripleVeryStrongBonus);
  }

  // The adjacent copula and nominalizer spelling だの is the enumerative
  // particle, not a productive copula phrase.
  if (prev.extended_pos == core::ExtendedPOS::AuxCopulaDa && next.extended_pos == core::ExtendedPOS::ParticleNo &&
      utf8::equalsAny(prev.surface, {"だ"}) && utf8::equalsAny(next.surface, {"の"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // A bare noun followed by the adverbial negative なく is the formal
  // "without [noun]" construction (資金なくして). Prefer it over the
  // transitive verb なくす, whose object requires a case-marked noun (物を
  // なくして). The lemma gate keeps ordinary adjective renyokei attachments
  // untouched.
  if (prev.pos == core::PartOfSpeech::Noun && next.extended_pos == core::ExtendedPOS::AdjRenyokei &&
      next.lemma == "ない") {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // A deverbal noun ending in 違い before ない forms the lexicalized
  // negative-adjective predicate (間違い+ない). The suffix identifies the
  // productive noun family without changing ordinary nominal negatives.
  if (prev.extended_pos == core::ExtendedPOS::Noun && utf8::endsWith(prev.surface, "違い") &&
      next.extended_pos == core::ExtendedPOS::AdjBasic && next.lemma == "ない") {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // A personal or demonstrative pronoun cannot directly precede the
  // continuative し of する. This keeps それ+らしい distinct from the
  // unrelated plural-pronoun-plus-verb path while leaving interrogative
  // 何+し constructions to their dedicated rule.
  if (prev.extended_pos == core::ExtendedPOS::Pronoun && grammar::isConjunctiveParticleShi(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // A dictionary noun followed by a registered adverbial particle is a stable
  // nominal phrase (あん+だけ, そん+だけ). Keep that lexical boundary ahead
  // of a spurious copula plus sentence-final particle, but do not promote
  // generated noun readings such as the adjective ない before ほど.
  if (prev.pos == core::PartOfSpeech::Noun && prev.fromDictionary() &&
      next.extended_pos == core::ExtendedPOS::ParticleAdverbial) {
    SUZUME_CONNECTION_ADD(bonus, sc::kBonusHonorificGeneratedRenyokei);
  }

  // A registered two-mora adverbial particle beginning with the copula だ can
  // follow a lexical or generated noun. Preserve that closed particle over
  // the mechanically attractive だ(AUX)+case-particle path. The category,
  // dictionary, length, and initial-codepoint gates describe the collision
  // family without assigning a word-specific score.
  if (prev.pos == core::PartOfSpeech::Noun && next.extended_pos == core::ExtendedPOS::ParticleAdverbial &&
      next.fromDictionary() && normalize::utf8Length(next.surface) == 2 &&
      utf8::decodeFirstChar(next.surface) == U'だ' && utf8::decodeLastChar(next.surface) == U'に') {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // Determiners directly modify formal nouns (このこと, どういうこと, この時).
  // Preserve that productive adnominal boundary over a split into an
  // adverb/adjective and the independent verb いう. The pair cannot be priced
  // by the formal noun: そのように is as grammatical as このこと, so what makes
  // 夢かのように wrong is the literary determiner かの, not the head よう.
  if (prev.extended_pos == core::ExtendedPOS::Determiner && next.extended_pos == core::ExtendedPOS::NounFormal) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // The productive honorific construction お/ご + verb (お聞きになる,
  // おはす) must retain the dictionary-verified verbal reading ahead of a
  // homographic noun candidate. This covers renyokei requests and finite
  // literary honorific verbs; an irrealis stem cannot directly follow the
  // prefix and must instead take its auxiliary before the construction is
  // complete.
  if (prev.extended_pos == core::ExtendedPOS::Prefix && grammar::isHonorificPrefix(prev.surface) &&
      next.pos == core::PartOfSpeech::Verb && next.extended_pos != core::ExtendedPOS::VerbMizenkei &&
      next.fromDictionary()) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // The same productive honorific frame can contain an otherwise unlisted
  // kanji ichidan continuative. Its generated verbal candidate has stronger
  // inflection evidence than the homographic nominalization, whereas a bare
  // prefix plus noun remains available for ordinary honorific compounds.
  if (prev.extended_pos == core::ExtendedPOS::Prefix && grammar::isHonorificPrefix(prev.surface) &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei && next.origin == core::CandidateOrigin::VerbKanji) {
    SUZUME_CONNECTION_ADD(bonus, cost::kMinorBonus);
  }

  // The Sino-Japanese honorific ご takes a Sino-Japanese nominal, which is
  // written in kanji (ご確認, ご案内). A pure-hiragana host after it is a
  // native word, which takes お instead, so the pair is an accidental split of
  // a longer kana word (ごとき as ご+とき).
  if (prev.extended_pos == core::ExtendedPOS::Prefix && grammar::isSinoHonorificPrefix(prev.surface) &&
      next.pos == core::PartOfSpeech::Noun && grammar::isPureHiragana(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, sc::kPenaltyClosedClassBoundary);
  }

  // The parallel compound particle とともに must remain whole instead of
  // being reanalyzed as a quotative particle followed by the adverb ともに.
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase && next.pos == core::PartOfSpeech::Adverb &&
      grammar::isParallelTogetherAdverb(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kProhibitive);
  }

  // A non-quotative case particle can introduce a multi-mora renyokei in an
  // ordinary predicate. Keep this complete verb ahead of a fabricated
  // one-mora causative plus polite-auxiliary chain. The quotative と is
  // excluded because its following いう boundary is intentionally ambiguous.
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase && !utf8::equalsAny(prev.surface, {"と", "で"}) &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei && next.surface.size() > core::kJapaneseCharBytes) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // A finite verb directly follows the accusative particle を (しびれを切らす),
  // but not every case particle permits that boundary.  In particular, で
  // must leave the polite copula です intact.  The classical attributive is
  // finite in the same sense — the two cells merged in the modern paradigm, so
  // the boundary is the same one (道を+求むる+物).
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase && grammar::isAccusativeParticleWoSurface(prev.surface) &&
      (next.extended_pos == core::ExtendedPOS::VerbShuushikei ||
       next.extended_pos == core::ExtendedPOS::VerbRentaikei)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // A dictionary-backed multi-mora terminal verb after を is a complete
  // lexical predicate.  Give it a small preference over a competing V1 plus
  // productive subsidiary analysis, while leaving short productive forms
  // (読み+過ぎる) to their ordinary auxiliary path.
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase && grammar::isAccusativeParticleWoSurface(prev.surface) &&
      next.extended_pos == core::ExtendedPOS::VerbShuushikei && next.fromDictionary() &&
      normalize::utf8Length(next.surface) >= 4) {
    SUZUME_CONNECTION_ADD(bonus, sc::kBonusVerifiedTerminalAfterObject);
  }

  // A nominative case particle directly introduces a finite predicate. Keep a
  // dictionary verb at this boundary ahead of a shorter homographic split.
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase && utf8::equalsAny(prev.surface, {"が"}) &&
      next.extended_pos == core::ExtendedPOS::VerbShuushikei && next.fromDictionary()) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  if (prev.extended_pos == core::ExtendedPOS::AuxTenseTa && utf8::equalsAny(prev.surface, {"た"}) &&
      next.extended_pos == core::ExtendedPOS::ParticleConj && utf8::equalsAny(next.surface, {"ば"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // A conditional ば requires a hypothetical verb form (書け+ば), not a
  // terminal form. This blocks fragment paths such as す+ば+らしい.
  if (prev.extended_pos == core::ExtendedPOS::VerbShuushikei && next.extended_pos == core::ExtendedPOS::ParticleConj &&
      utf8::equalsAny(next.surface, {"ば"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // A connective し follows a terminal predicate, never an euphonic verb
  // form. The restriction preserves whole i-adjectives such as おいしく
  // against a fabricated おい+し route.
  if (prev.extended_pos == core::ExtendedPOS::VerbOnbinkei && next.extended_pos == core::ExtendedPOS::ParticleConj &&
      grammar::isConjunctiveParticleShi(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // A connective し also cannot attach to an interjection or an adverb. Those
  // paths split i-adjective stems (おい+し, バカバカ+し) without a predicate.
  if ((prev.pos == core::PartOfSpeech::Interjection || prev.extended_pos == core::ExtendedPOS::Adverb) &&
      next.extended_pos == core::ExtendedPOS::ParticleConj && grammar::isConjunctiveParticleShi(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // An adjective stem cannot take the past-conditional particle ったら.
  // The sequence is a verb's euphonic stem plus tense auxiliary (なっ+たら).
  if (prev.extended_pos == core::ExtendedPOS::AdjStem && next.extended_pos == core::ExtendedPOS::ParticleFinal &&
      utf8::equalsAny(next.surface, {"ったら"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // The concessive particle とも attaches to an adjective renyokei, the
  // classical negative ず, or a verb's own terminal form (言う+とも, 行く+とも),
  // then introduces a finite predicate. Keep this closed particle whole instead
  // of splitting it into quotative と plus も. Behind a terminal that competing
  // reading is among the most productive sequences in the language, so there
  // the preference has to be stronger than the ordinary one.
  const bool concessive_host_is_terminal = prev.extended_pos == core::ExtendedPOS::VerbShuushikei;
  if ((prev.extended_pos == core::ExtendedPOS::AdjRenyokei || prev.extended_pos == core::ExtendedPOS::AuxNegativeNu ||
       concessive_host_is_terminal) &&
      next.extended_pos == core::ExtendedPOS::ParticleConj && grammar::isConcessiveParticleTomoSurface(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus,
                          concessive_host_is_terminal ? sc::kBonusConcessiveTomoAfterTerminal : cost::kStrongBonus);
  }
  if (prev.extended_pos == core::ExtendedPOS::ParticleConj && grammar::isConcessiveParticleTomoSurface(prev.surface) &&
      (next.extended_pos == core::ExtendedPOS::VerbShuushikei || next.extended_pos == core::ExtendedPOS::AdjBasic)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // A case-marked object cannot be followed by an unsplit negative verb
  // candidate. Negative inflection is represented as a predicate stem plus
  // its auxiliary (を+え+ない), including past and conditional forms.
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase && grammar::isAccusativeParticleWoSurface(prev.surface) &&
      next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      utf8::endsWithAny(next.surface, {"ない", "なかっ", "なけれ"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase && grammar::isAccusativeParticleWoSurface(prev.surface) &&
      next.extended_pos == core::ExtendedPOS::VerbOnbinkei && utf8::endsWith(next.surface, "なかっ")) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // A candidate ending in te is not a terminal adjective. It must be analyzed
  // as an adjective renyokei plus the connective particle before a formal
  // noun, rather than as a malformed whole adjective (嬉しくてしかたがない).
  if (prev.extended_pos == core::ExtendedPOS::AdjBasic && utf8::endsWith(prev.surface, "て") &&
      next.extended_pos == core::ExtendedPOS::NounFormal) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // An adverbial particle can directly modify a kanji verb in euphonic form
  // before the past auxiliary (ずつ+配っ+た). A generated candidate must also
  // have an actual euphonic ending: its ExtendedPOS alone can be spuriously
  // assigned to an unsplit negative such as 帰らない. Keep the productive past
  // split ahead of a single VerbTaForm candidate; pure-hiragana copular forms
  // stay outside this rule.
  if (prev.extended_pos == core::ExtendedPOS::ParticleAdverbial &&
      next.extended_pos == core::ExtendedPOS::VerbOnbinkei && grammar::containsKanji(next.surface) &&
      (utf8::endsWithAny(next.surface, {"っ", "ん"}) ||
       (utf8::endsWith(next.surface, "い") && !utf8::endsWith(next.surface, "ない")))) {
    SUZUME_CONNECTION_ADD(bonus, cost::kExtremeBonus);
  }

  // A productive temporal compound ending in 時 (開始時, 緊急時) is a
  // complete search unit before a following case particle.  Keep that unit
  // ahead of the competing noun + formal-noun 時 path.
  if (prev.pos == core::PartOfSpeech::Noun && !prev.fromDictionary() && utf8::endsWith(prev.surface, "時") &&
      next.extended_pos == core::ExtendedPOS::ParticleCase) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // A one-kanji noun followed by a one-kanji formal noun is usually a lexical
  // compound (人物, 結末), not a productive formal-noun boundary.  Longer
  // nominal stems remain available for bound temporal/spatial forms such as
  // 年度+末 and 期間+内.
  if (prev.pos == core::PartOfSpeech::Noun && next.extended_pos == core::ExtendedPOS::NounFormal &&
      normalize::utf8Length(prev.surface) == 1 && normalize::utf8Length(next.surface) == 1 &&
      grammar::isAllKanji(prev.surface) && grammar::isAllKanji(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrong);
  }

  // A multi-kanji lexical stem followed by an all-kanji formal noun is normally
  // one compound search unit (不祥事, 出来事). Temporal endpoint suffixes retain
  // their productive boundary (年度+末, 学期+末).
  if (next.extended_pos == core::ExtendedPOS::NounFormal && normalize::utf8Length(prev.surface) >= 2 &&
      normalize::utf8Length(next.surface) == 1 && grammar::isAllKanji(prev.surface) &&
      grammar::isAllKanji(next.surface) && !normalize::isTemporalSpanSuffixKanji(utf8::decodeFirstChar(next.surface))) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrong);
  }

  // A numeral duration ending in 時間 can directly modify a following
  // predicate without a case particle (三時間かかる, 二時間待つ). Prefer the
  // complete NounNumber candidate to a spurious 時+間 suffix split.
  if (prev.extended_pos == core::ExtendedPOS::NounNumber && utf8::endsWith(prev.surface, "時間") &&
      next.pos == core::PartOfSpeech::Verb) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // A quantity suffix such as 半 follows a completed numeral-counter phrase;
  // it cannot be the kanji stem of a following compound verb.
  if (prev.extended_pos == core::ExtendedPOS::NounNumber && next.pos == core::PartOfSpeech::Verb &&
      normalize::isQuantityPrefixKanji(utf8::decodeFirstChar(next.surface))) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // A dictionary-verified long hiragana renyokei can lexicalize as a
  // discourse connective before a nominal predicate (さておき説明する).
  // Requiring four moras keeps short auxiliary stems such as おき outside the
  // rule while retaining the productive verb-form analysis and its lemma.
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && prev.fromDictionary() &&
      prev.surface.size() >= 4 * core::kJapaneseCharBytes && grammar::isPureHiragana(prev.surface) &&
      next.pos == core::PartOfSpeech::Noun) {
    SUZUME_CONNECTION_ADD(bonus, cost::kDoubleVeryStrongBonus);
  }

  // The concessive particle とも attaches to a predicate (読まずとも), not
  // directly to an adverb or a finite adjective. After either predicate-like
  // modifier, retain the productive quotation plus topic sequence instead
  // (そう+と+も言える, 恐しい+と+も思わない).
  if ((prev.pos == core::PartOfSpeech::Adverb || prev.extended_pos == core::ExtendedPOS::AdjBasic ||
       prev.extended_pos == core::ExtendedPOS::AdjNaAdj) &&
      next.extended_pos == core::ExtendedPOS::ParticleConj && grammar::isConcessiveParticleTomoSurface(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // The concessive とも can directly introduce an adjective predicate
  // (読まずともよい). Keep that closed grammatical connection ahead of the
  // unrelated quotative-particle path.
  const bool concessive_before_adjective = prev.extended_pos == core::ExtendedPOS::ParticleConj &&
                                           grammar::isConcessiveParticleTomoSurface(prev.surface) &&
                                           next.pos == core::PartOfSpeech::Adjective;

  // The conditional of an i-adjective is its けれ-form plus ば. Prefer that
  // productive inflection over unrelated short verb and classical-auxiliary
  // fragments inside a pure-hiragana adjective.
  const bool adjective_conditional = prev.extended_pos == core::ExtendedPOS::AdjKeForm &&
                                     next.extended_pos == core::ExtendedPOS::ParticleConj &&
                                     grammar::isSingleHiragana(next.surface, U'ば');
  // The i-adjective conditional cell of the desiderative is たけれ+ば.
  // Its surface is also a plausible fabricated godan kateikei, whose generic
  // kateikei+ば bonus otherwise wins. Limit the decisive preference to the
  // closed conditional cell so classical たし+て remains a lexical verb stem.
  const bool desiderative_conditional =
      prev.extended_pos == core::ExtendedPOS::AuxDesireTai && utf8::endsWith(prev.surface, "けれ") &&
      next.extended_pos == core::ExtendedPOS::ParticleConj && grammar::isSingleHiragana(next.surface, U'ば');
  if (concessive_before_adjective || adjective_conditional || desiderative_conditional) {
    SUZUME_CONNECTION_ADD(bonus, desiderative_conditional ? cost::kDoubleVeryStrongBonus : cost::kStrongBonus);
  }

  // An adverb ending in the connective mora て cannot directly introduce the
  // progressive auxiliary. Preserve the productive verb-onbin + て + いる
  // path (続い+て+いる) over a lexicalized adverb candidate (続いて).
  if (prev.pos == core::PartOfSpeech::Adverb && utf8::endsWith(prev.surface, "て") &&
      (next.extended_pos == core::ExtendedPOS::AuxAspectIru ||
       (next.pos == core::PartOfSpeech::Verb && next.lemma == "いる"))) {
    SUZUME_CONNECTION_ADD(bonus, cost::kSevere);
  }

  // The directional particle sequence へ+と is productive (次へと進む,
  // 都市へと向かう). Keep the two search units instead of promoting their
  // hiragana spelling to an unknown noun.
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase && next.extended_pos == core::ExtendedPOS::ParticleCase &&
      utf8::equalsAny(prev.surface, {"へ"}) && utf8::equalsAny(next.surface, {"と"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kVeryStrongBonus);
  }

  // A multi-mora delimitative/source/comparative particle can itself be
  // selected as the scope of が (誰まで+が, 誰から+が, 誰より+が).  This is
  // distinct from an invalid stack of two ordinary one-mora case markers.
  // Require the complete dictionary particle and the subject marker on the
  // right; sequences such as に+を and を+に remain under the general
  // ParticleCase→ParticleCase penalty.
  if (prev.extended_pos == core::ExtendedPOS::ParticleCase && next.extended_pos == core::ExtendedPOS::ParticleCase &&
      prev.fromDictionary() && prev.surface.size() >= core::kTwoJapaneseCharBytes &&
      grammar::isSingleHiragana(next.surface, U'が')) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrongBonus);
  }

  // 向け is a productive audience suffix after a nominal (読者+向けに,
  // 家庭+向け). Its following case particle makes the compositional boundary
  // preferable to an unknown noun candidate spanning the entire expression.
  if (prev.pos == core::PartOfSpeech::Noun && next.pos == core::PartOfSpeech::Suffix &&
      utf8::equalsAny(next.surface, {"向け"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kMinorBonus);
  }

  // Adjacent administrative units are separate search units (都道府県+市区町村).
  // Their dedicated suffix candidates retain this boundary over an arbitrary
  // long unknown kanji sequence.
  if (prev.origin == core::CandidateOrigin::SuffixPattern && next.origin == core::CandidateOrigin::SuffixPattern &&
      prev.pos == core::PartOfSpeech::Noun && next.pos == core::PartOfSpeech::Noun &&
      grammar::endsWithAdministrativeSuffix(prev.surface) && grammar::endsWithAdministrativeSuffix(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kMinorBonus);
  }

  // 中 is a productive state/duration suffix after a nominal stem (作業+中,
  // 今日+中). Prefer that grammatical boundary over an unregistered compound;
  // lexicalized compounds retain their dictionary candidate.
  if (prev.pos == core::PartOfSpeech::Noun && prev.extended_pos != core::ExtendedPOS::NounNumber &&
      !normalize::isNumeralCodepoint(utf8::decodeFirstChar(prev.surface)) && next.pos == core::PartOfSpeech::Suffix &&
      grammar::isStateDurationSuffix(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kMinorBonus);
  }

  // A written honorific title is a separate searchable unit after a multi-kanji
  // nominal stem. This preserves the boundary even when the title also has an
  // unknown noun candidate rather than a dictionary suffix entry.
  if (prev.pos == core::PartOfSpeech::Noun && prev.surface.size() >= 2 * core::kJapaneseCharBytes &&
      grammar::isAllKanji(prev.surface) && next.pos == core::PartOfSpeech::Noun &&
      grammar::isKanjiHonorificTitle(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kMinorBonus);
  }

  // A one-kanji formal noun followed by a one-kanji kanji suffix is normally
  // a lexical compound (時間, 期間), not a productive formal-noun boundary.
  // Leave kana suffixes and multi-character forms unaffected.
  if (prev.extended_pos == core::ExtendedPOS::NounFormal && next.pos == core::PartOfSpeech::Suffix &&
      normalize::utf8Length(prev.surface) == 1 && normalize::utf8Length(next.surface) == 1 &&
      grammar::isAllKanji(prev.surface) && grammar::isAllKanji(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kStrong);
  }

  // ところで is a discourse conjunction only at a clause boundary. Likewise,
  // ところが and ついで following a past form are formal-noun constructions
  // construction. After a formal noun or a past/copular auxiliary, keep the
  // compositional reading (読んだところで, 読んだついで) over a fused
  // conjunction candidate.
  if (next.extended_pos == core::ExtendedPOS::Conjunction &&
      utf8::equalsAny(next.surface, {"ところで", "ところが", "ついで"}) &&
      (prev.extended_pos == core::ExtendedPOS::NounFormal || prev.extended_pos == core::ExtendedPOS::AuxTenseTa ||
       prev.extended_pos == core::ExtendedPOS::AuxCopulaDa || prev.extended_pos == core::ExtendedPOS::VerbTaForm ||
       prev.extended_pos == core::ExtendedPOS::ParticleNo)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kProhibitive + cost::kSevere);
  }

  // ゆえに is a discourse conjunction at a clause boundary, but after a
  // nominal phrase it is the productive causal phrase ゆえ(形式名詞)+に.
  // A fused conjunction cannot follow a noun, pronoun, or genitive particle.
  if (next.extended_pos == core::ExtendedPOS::Conjunction && utf8::equalsAny(next.surface, {"ゆえに"}) &&
      (prev.pos == core::PartOfSpeech::Noun || prev.extended_pos == core::ExtendedPOS::Pronoun ||
       prev.extended_pos == core::ExtendedPOS::ParticleNo)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kProhibitive);
  }

  // The formal particle により cannot follow the copula's attributive form
  // な. Reject that accidental path so a following interrogative pronoun can
  // retain the productive comparison boundary (なに+より).
  if ((prev.extended_pos == core::ExtendedPOS::AuxCopulaDa || prev.extended_pos == core::ExtendedPOS::AdjStem) &&
      next.extended_pos == core::ExtendedPOS::ParticleCase && utf8::equalsAny(next.surface, {"により"})) {
    SUZUME_CONNECTION_ADD(bonus, cost::kProhibitive);
  }

  // The benefactive formal noun おかげ continues either with the instrumental
  // particle (おかげで) or the honorific suffix (おかげさまで). Both retain
  // the productive formal-noun boundary over their homographic alternatives.
  if (prev.extended_pos == core::ExtendedPOS::NounFormal && grammar::isBenefactiveFormalNoun(prev.surface) &&
      ((next.extended_pos == core::ExtendedPOS::ParticleCase && utf8::equalsAny(next.surface, {"で"})) ||
       (next.pos == core::PartOfSpeech::Suffix && utf8::equalsAny(next.surface, {"さま"})))) {
    SUZUME_CONNECTION_ADD(bonus,
                          next.pos == core::PartOfSpeech::Suffix ? cost::kDoubleVeryStrongBonus : cost::kStrongBonus);
  }

  // Penalty for SUFFIX(さ) + VERB starting with せ/させ pattern
  // E.g., 勉強 + さ(SUFFIX) + せられてい is wrong; should be 勉強 + さ(VERB_未然) + せ(AUX_使役)
  // This pattern indicates suru-verb causative form where さ is the verb stem, not suffix
  if (prev.pos == core::PartOfSpeech::Suffix && utf8::equalsAny(prev.surface, {"さ"}) &&
      next.pos == core::PartOfSpeech::Verb &&
      (utf8::startsWith(next.surface, "せ") || utf8::startsWith(next.surface, "させ"))) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);
  }

  // Penalty for a one-kanji-to-one-kanji noun/suffix transition, in either
  // direction. Such a pair is normally a two-kanji kango word that the L1
  // suffix inventory splits open: 正+式, 手+法, 結+論 when the suffix follows,
  // 用+紙, 用+品 when it leads. The NOUN→SUFFIX bigram bonus (-0.8) plus the
  // SUFFIX→PART_格 epos bonus otherwise makes the split path cheaper than the
  // compound path. Adjacent lattice edges are contiguous in the text, so two
  // all-kanji surfaces here always sit inside one kanji run — a run-final
  // suffix (学生+用, 科学+的) and a multi-kanji stem (学生+たち) never reach it.
  // Exceptions:
  // - 様/氏: handled by +4.0 kanji_seq penalty in unknown.cpp (always split)
  // - 的: removed from kanji_seq penalty; 1-char + 的 stays merged naturally
  //   (目的, 動的, 知的), 2+ char + 的 still splits via bigram bonus (論理+的)
  const bool one_kanji_noun_before_suffix =
      prev.pos == core::PartOfSpeech::Noun && next.pos == core::PartOfSpeech::Suffix &&
      grammar::isAllKanji(prev.surface) && !grammar::isKanjiHonorificTitle(next.surface);
  const bool one_kanji_suffix_before_noun = prev.pos == core::PartOfSpeech::Suffix &&
                                            next.pos == core::PartOfSpeech::Noun && grammar::isAllKanji(prev.surface) &&
                                            grammar::isAllKanji(next.surface);
  if (normalize::utf8Length(prev.surface) == 1 && normalize::utf8Length(next.surface) == 1 &&
      (one_kanji_noun_before_suffix || one_kanji_suffix_before_noun)) {
    // The split collects the -0.8 bigram bonus here and the SUFFIX→PART_格
    // bonus on the next edge (最+中+に), so a penalty that only offsets the
    // first one still leaves the broken kango cheaper.
    SUZUME_CONNECTION_ADD(bonus, cost::kStrong);
  }

  // Penalty for 年+度 lexical binding pattern (年度, fiscal year)
  // A kanji noun ending in 年 followed by the lone suffix 度 forms 年度, not a
  // NOUN+degree/frequency-suffix split. Without this, the NOUN→SUFFIX bigram
  // bonus (-0.8) makes 今年|度 cheaper than the whole 今年度.
  if (prev.pos == core::PartOfSpeech::Noun && next.pos == core::PartOfSpeech::Suffix &&
      normalize::utf8Length(prev.surface) >= 2 && grammar::isAllKanji(prev.surface) &&
      normalize::utf8Length(next.surface) == 1 && grammar::isAllKanji(next.surface)) {
    if (normalize::isFiscalYearBindingPair(utf8::decodeLastChar(prev.surface), utf8::decodeFirstChar(next.surface))) {
      SUZUME_CONNECTION_ADD(bonus, cost::kRare);  // +1.0 to neutralize -0.8 bigram bonus
    }
  }

  // Penalty for 3+ char non-dict kanji NOUN → 1-char SUFFIX pattern
  // For non-dict 3+ char kanji NOUN preceding a single-kanji suffix, the 4-char
  // input is often two 2-char kango compounds (新規 + 手法) rather than
  // a 3+1 stem+suffix split (新規手 + 法). Penalize to let the whole-word
  // (or 2+2 split) compete fairly. Dict-verified 3-char NOUNs (e.g., 政治学+者 if
  // 政治学 were in dict) keep the bonus, since they represent intended compounds.
  // A stem that already ends in a bound derivational suffix (利用者, 安全性) is a
  // derived noun, not the left half of a two-compound run, so it keeps the bonus.
  // A two-kanji stem whose second character heads a duration noun is a modifier
  // plus that noun (長+時間, 短+期間), not a stem plus the quantity-phrase suffix
  // 間, so it needs the same neutralizing penalty as the 3+ char case. Keyed on
  // the period unit rather than the length: after any other two-kanji noun the
  // suffix keeps its productive "between X" reading (部署|間, 世代|間).
  const size_t suffix_stem_len = normalize::utf8Length(prev.surface);
  const bool duration_noun_tail = suffix_stem_len == 2 &&
                                  normalize::isQuantityPhraseSuffixKanji(utf8::decodeFirstChar(next.surface)) &&
                                  normalize::isDurationCompoundHeadKanji(utf8::decodeLastChar(prev.surface));
  if (prev.pos == core::PartOfSpeech::Noun && next.pos == core::PartOfSpeech::Suffix && !prev.fromDictionary() &&
      (suffix_stem_len >= 3 || duration_noun_tail) && normalize::utf8Length(next.surface) == 1 &&
      grammar::isAllKanji(prev.surface) && grammar::isAllKanji(next.surface) &&
      !normalize::isDerivationalNounSuffixKanji(utf8::decodeLastChar(prev.surface))) {
    SUZUME_CONNECTION_ADD(bonus, cost::kRare);  // +1.0 to neutralize -0.8 bigram bonus
  }

  // A fused でも directly following a content word is the adverbial particle
  // ("even/as much as"), not the discourse conjunction.  Keep the conjunction
  // candidate for BOS and clause-boundary use, while preventing the generic
  // Conjunction→content-word bonus from relabeling nominal でも phrases.
  const bool content_before_discourse_demo =
      (core::isContentWord(prev.pos) || prev.pos == core::PartOfSpeech::Pronoun) &&
      next.extended_pos == core::ExtendedPOS::Conjunction && grammar::isCopulaFusedConjunction(next.surface);

  // The same conjunction opens a clause, and the existential directly behind it
  // opens nothing: there the copula continuative is building its periphrastic
  // form and the binding particle sits inside it (で+は+ある, the topic within
  // である). The focus member is exempt because it is a listed adverbial
  // particle in its own right and still spells one token in that slot, while
  // the topic member is a connective and nothing else.
  const bool fused_topic_before_existential =
      prev.extended_pos == core::ExtendedPOS::Conjunction &&
      grammar::copulaFusedConjunctionParticle(prev.surface) == core::hiragana::kHa &&
      grammar::isCopulaPeriphrasisCell(next.surface);

  // Penalty for ADV → でも (CONJ or PART_副) pattern
  // After adverbs, でも should split as で(copula)+も(particle)
  // e.g., それほどでもない → それほど+で+も+ない
  const bool adverb_before_fused_demo =
      prev.pos == core::PartOfSpeech::Adverb && grammar::isCopulaFusedConjunction(next.surface) &&
      (next.pos == core::PartOfSpeech::Conjunction || next.extended_pos == core::ExtendedPOS::ParticleAdverbial);
  const bool adverb_before_focus_mo =
      prev.pos == core::PartOfSpeech::Adverb && prev.fromDictionary() && normalize::utf8Length(prev.surface) == 2 &&
      grammar::containsKanji(prev.surface) && utf8::endsWith(prev.surface, "し") &&
      next.extended_pos == core::ExtendedPOS::ParticleTopic && grammar::isSingleHiragana(next.surface, U'も');
  const bool fused_conjunction_unavailable = content_before_discourse_demo || fused_topic_before_existential;
  if (fused_conjunction_unavailable || adverb_before_fused_demo || adverb_before_focus_mo) {
    SUZUME_CONNECTION_ADD(bonus,
                          adverb_before_fused_demo
                              ? cost::kAlmostNever
                              : (fused_conjunction_unavailable ? cost::kProhibitive : cost::kDoubleVeryStrongBonus));
  }

  // Penalty for predicate → copula-compound conjunction (だから/だけど/だが/…) pattern
  // These conjunctions are the copula だ fused with a particle (から/けど/が). They are
  // valid at a sentence/clause boundary, but after a copula-taking predicate (noun,
  // pronoun, adverb, na-adjective stem, or 様態 そう) they must split as だ(AUX) + PART:
  // 彼女だけど → 彼女+だ+けど, 静かだから → 静か+だ+から, 危なそうだから → 危な+そう+だ+から,
  // 遅刻しがちだが → がち(SUFFIX)+だ+が (nominal suffixes take the copula too).
  // Keyed on the だ onset rather than each surface so the rule generalizes across the set.
  // Other (unknown noun-like blobs) is included so the copula still splits when a
  // nominal suffix is demoted to Other in the presence of the fused conjunction:
  // がち would otherwise drop to Other purely to dodge this penalty (遅刻しがちだが).
  if (next.extended_pos == core::ExtendedPOS::Conjunction && utf8::startsWith(next.surface, "だ") &&
      (prev.pos == core::PartOfSpeech::Noun || prev.pos == core::PartOfSpeech::Pronoun ||
       prev.pos == core::PartOfSpeech::Adverb || prev.pos == core::PartOfSpeech::Adjective ||
       prev.pos == core::PartOfSpeech::Suffix || prev.pos == core::PartOfSpeech::Other ||
       prev.extended_pos == core::ExtendedPOS::AuxAppearanceSou)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kProhibitive);
  }

  // Note: Removed penalty for PARTICLE と → VERB_音便 いっ pattern
  // The dictionary entry "といった" (determiner) handles that case
  // For といって pattern, we want と+いっ+て split (MeCab compatible)

  // Penalty for VerbRenyokei → single-char char_speech AUX pattern
  // E.g., 食べろ should be 食べろ (imperative), not 食べ+ろ
  // The ろ is the ichidan imperative ending, not a character speech suffix
  // Character speech suffixes like ろ are valid after だ/です (だろ, でしょ)
  // but not after verb renyokei
  // Valid patterns after VerbRenyokei: た, て, ます, etc. (multi-char or dictionary)
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei && next.pos == core::PartOfSpeech::Auxiliary &&
      !next.fromDictionary() && next.surface.size() <= core::kJapaneseCharBytes) {  // Single char (3 bytes)
    SUZUME_CONNECTION_ADD(bonus, cost::kUncommon);
  }

  // Penalty for single-char hiragana VerbRenyokei → AuxPassive/AuxCausative
  // Bigram table gives bonus for VerbRenyokei→AuxPassive (for 知らせ+られ)
  // But single-char hiragana like せ+られ should prefer AuxCausative+AuxPassive path
  // Valid patterns like 知らせ+られ have longer surfaces (2+ chars)
  // This prevents せ(VERB連用) from being selected over せ(AuxCausative)
  // Exception: い+られ is valid (いる potential: いられない = cannot stay)
  if (prev.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      (next.extended_pos == core::ExtendedPOS::AuxPassive || next.extended_pos == core::ExtendedPOS::AuxCausative) &&
      prev.surface.size() <= 3 &&                                       // Single hiragana (3 bytes)
      grammar::isPureHiragana(prev.surface) && prev.surface != "い") {  // い+られ is valid (いる potential)
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);                   // Strongly discourage
  }

  // Penalty for a conjunctive particle → a single-character lexical verb stem.
  // Subsidiary verbs after a te-form must use their aspect-specific ExtendedPOS
  // (い→AuxAspectIru, み→AuxAspectMiru), not a fabricated lexical verb.
  // The light verb する is the one continuative this cannot reach: it stands for
  // a predicate rather than modifying one, so it has no aspect category to be
  // written with and follows any conjunctive particle (書面をもっ+て+し+た,
  // 食べたり+し+てる). Naming that continuative covers both particles at once,
  // where excepting たり/だり by surface left the same sequence banned after て.
  if (prev.extended_pos == core::ExtendedPOS::ParticleConj && next.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      next.surface.size() <= 3 &&  // Single hiragana (3 bytes)
      grammar::isPureHiragana(next.surface) && !grammar::isSuruRenyokeiSurface(next.surface)) {
    SUZUME_CONNECTION_ADD(bonus, cost::kAlmostNever);  // Strongly discourage
  }

  return bonus;
}

}  // namespace suzume::analysis::connection_rules
