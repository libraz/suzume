#include "core/types.h"

#include <gtest/gtest.h>

namespace suzume {
namespace core {
namespace {

// =============================================================================
// posToJapanese - all POS values
// =============================================================================

TEST(TypesExtendedTest, PosToJapaneseAllValues) {
  EXPECT_EQ(posToJapanese(PartOfSpeech::Noun), "名詞");
  EXPECT_EQ(posToJapanese(PartOfSpeech::Verb), "動詞");
  EXPECT_EQ(posToJapanese(PartOfSpeech::Adjective), "形容詞");
  EXPECT_EQ(posToJapanese(PartOfSpeech::Adverb), "副詞");
  EXPECT_EQ(posToJapanese(PartOfSpeech::Particle), "助詞");
  EXPECT_EQ(posToJapanese(PartOfSpeech::Auxiliary), "助動詞");
  EXPECT_EQ(posToJapanese(PartOfSpeech::Conjunction), "接続詞");
  EXPECT_EQ(posToJapanese(PartOfSpeech::Determiner), "連体詞");
  EXPECT_EQ(posToJapanese(PartOfSpeech::Pronoun), "代名詞");
  EXPECT_EQ(posToJapanese(PartOfSpeech::Prefix), "接頭辞");
  EXPECT_EQ(posToJapanese(PartOfSpeech::Suffix), "接尾辞");
  EXPECT_EQ(posToJapanese(PartOfSpeech::Interjection), "感動詞");
  EXPECT_EQ(posToJapanese(PartOfSpeech::Symbol), "記号");
  EXPECT_EQ(posToJapanese(PartOfSpeech::Other), "その他");
  EXPECT_EQ(posToJapanese(PartOfSpeech::Unknown), "その他");
}

// =============================================================================
// posToDefaultExtendedPOS - all POS values
// =============================================================================

TEST(TypesExtendedTest, PosToDefaultExtendedPOSAllValues) {
  EXPECT_EQ(posToDefaultExtendedPOS(PartOfSpeech::Noun), ExtendedPOS::Noun);
  EXPECT_EQ(posToDefaultExtendedPOS(PartOfSpeech::Verb), ExtendedPOS::VerbShuushikei);
  EXPECT_EQ(posToDefaultExtendedPOS(PartOfSpeech::Adjective), ExtendedPOS::AdjBasic);
  EXPECT_EQ(posToDefaultExtendedPOS(PartOfSpeech::Adverb), ExtendedPOS::Adverb);
  EXPECT_EQ(posToDefaultExtendedPOS(PartOfSpeech::Particle), ExtendedPOS::ParticleCase);
  EXPECT_EQ(posToDefaultExtendedPOS(PartOfSpeech::Auxiliary), ExtendedPOS::AuxTenseTa);
  EXPECT_EQ(posToDefaultExtendedPOS(PartOfSpeech::Pronoun), ExtendedPOS::Pronoun);
  EXPECT_EQ(posToDefaultExtendedPOS(PartOfSpeech::Conjunction), ExtendedPOS::Conjunction);
  EXPECT_EQ(posToDefaultExtendedPOS(PartOfSpeech::Determiner), ExtendedPOS::Determiner);
  EXPECT_EQ(posToDefaultExtendedPOS(PartOfSpeech::Prefix), ExtendedPOS::Prefix);
  EXPECT_EQ(posToDefaultExtendedPOS(PartOfSpeech::Suffix), ExtendedPOS::Suffix);
  EXPECT_EQ(posToDefaultExtendedPOS(PartOfSpeech::Symbol), ExtendedPOS::Symbol);
  EXPECT_EQ(posToDefaultExtendedPOS(PartOfSpeech::Interjection), ExtendedPOS::Interjection);
  EXPECT_EQ(posToDefaultExtendedPOS(PartOfSpeech::Other), ExtendedPOS::Unknown);
  EXPECT_EQ(posToDefaultExtendedPOS(PartOfSpeech::Unknown), ExtendedPOS::Unknown);
}

// =============================================================================
// isTaggable - all POS values
// =============================================================================

TEST(TypesExtendedTest, IsTaggableContentWords) {
  EXPECT_TRUE(isTaggable(PartOfSpeech::Noun));
  EXPECT_TRUE(isTaggable(PartOfSpeech::Verb));
  EXPECT_TRUE(isTaggable(PartOfSpeech::Adjective));
  EXPECT_TRUE(isTaggable(PartOfSpeech::Adverb));
}

TEST(TypesExtendedTest, IsTaggableFunctionWords) {
  EXPECT_FALSE(isTaggable(PartOfSpeech::Particle));
  EXPECT_FALSE(isTaggable(PartOfSpeech::Auxiliary));
  EXPECT_FALSE(isTaggable(PartOfSpeech::Conjunction));
  EXPECT_FALSE(isTaggable(PartOfSpeech::Determiner));
  EXPECT_FALSE(isTaggable(PartOfSpeech::Pronoun));
  EXPECT_FALSE(isTaggable(PartOfSpeech::Prefix));
  EXPECT_FALSE(isTaggable(PartOfSpeech::Suffix));
  EXPECT_FALSE(isTaggable(PartOfSpeech::Interjection));
  EXPECT_FALSE(isTaggable(PartOfSpeech::Symbol));
  EXPECT_FALSE(isTaggable(PartOfSpeech::Other));
  EXPECT_FALSE(isTaggable(PartOfSpeech::Unknown));
}

// =============================================================================
// isContentWord - all POS values
// =============================================================================

TEST(TypesExtendedTest, IsContentWordTrue) {
  EXPECT_TRUE(isContentWord(PartOfSpeech::Noun));
  EXPECT_TRUE(isContentWord(PartOfSpeech::Verb));
  EXPECT_TRUE(isContentWord(PartOfSpeech::Adjective));
  EXPECT_TRUE(isContentWord(PartOfSpeech::Adverb));
}

TEST(TypesExtendedTest, IsContentWordFalse) {
  EXPECT_FALSE(isContentWord(PartOfSpeech::Particle));
  EXPECT_FALSE(isContentWord(PartOfSpeech::Auxiliary));
  EXPECT_FALSE(isContentWord(PartOfSpeech::Conjunction));
  EXPECT_FALSE(isContentWord(PartOfSpeech::Determiner));
  EXPECT_FALSE(isContentWord(PartOfSpeech::Pronoun));
  EXPECT_FALSE(isContentWord(PartOfSpeech::Prefix));
  EXPECT_FALSE(isContentWord(PartOfSpeech::Suffix));
  EXPECT_FALSE(isContentWord(PartOfSpeech::Interjection));
  EXPECT_FALSE(isContentWord(PartOfSpeech::Symbol));
  EXPECT_FALSE(isContentWord(PartOfSpeech::Other));
  EXPECT_FALSE(isContentWord(PartOfSpeech::Unknown));
}

// =============================================================================
// isFunctionWord - all POS values
// =============================================================================

TEST(TypesExtendedTest, IsFunctionWordTrue) {
  EXPECT_TRUE(isFunctionWord(PartOfSpeech::Particle));
  EXPECT_TRUE(isFunctionWord(PartOfSpeech::Auxiliary));
}

TEST(TypesExtendedTest, IsFunctionWordFalse) {
  EXPECT_FALSE(isFunctionWord(PartOfSpeech::Noun));
  EXPECT_FALSE(isFunctionWord(PartOfSpeech::Verb));
  EXPECT_FALSE(isFunctionWord(PartOfSpeech::Adjective));
  EXPECT_FALSE(isFunctionWord(PartOfSpeech::Adverb));
  EXPECT_FALSE(isFunctionWord(PartOfSpeech::Conjunction));
  EXPECT_FALSE(isFunctionWord(PartOfSpeech::Determiner));
  EXPECT_FALSE(isFunctionWord(PartOfSpeech::Pronoun));
  EXPECT_FALSE(isFunctionWord(PartOfSpeech::Prefix));
  EXPECT_FALSE(isFunctionWord(PartOfSpeech::Suffix));
  EXPECT_FALSE(isFunctionWord(PartOfSpeech::Interjection));
  EXPECT_FALSE(isFunctionWord(PartOfSpeech::Symbol));
  EXPECT_FALSE(isFunctionWord(PartOfSpeech::Other));
  EXPECT_FALSE(isFunctionWord(PartOfSpeech::Unknown));
}

// =============================================================================
// originToString - all CandidateOrigin values
// =============================================================================

TEST(TypesExtendedTest, OriginToStringAllValues) {
  EXPECT_STREQ(originToString(CandidateOrigin::Unknown), "unknown");
  EXPECT_STREQ(originToString(CandidateOrigin::Dictionary), "dict");
  EXPECT_STREQ(originToString(CandidateOrigin::VerbKanji), "verb_kanji");
  EXPECT_STREQ(originToString(CandidateOrigin::VerbHiragana), "verb_hira");
  EXPECT_STREQ(originToString(CandidateOrigin::VerbKatakana), "verb_kata");
  EXPECT_STREQ(originToString(CandidateOrigin::VerbCompound), "verb_compound");
  EXPECT_STREQ(originToString(CandidateOrigin::AdjectiveI), "adj_i");
  EXPECT_STREQ(originToString(CandidateOrigin::AdjectiveIHiragana), "adj_i_hira");
  EXPECT_STREQ(originToString(CandidateOrigin::AdjectiveNa), "adj_na");
  EXPECT_STREQ(originToString(CandidateOrigin::NominalizedNoun), "noun_nominalized");
  EXPECT_STREQ(originToString(CandidateOrigin::SuffixPattern), "suffix");
  EXPECT_STREQ(originToString(CandidateOrigin::SameType), "same_type");
  EXPECT_STREQ(originToString(CandidateOrigin::Alphanumeric), "alphanum");
  EXPECT_STREQ(originToString(CandidateOrigin::Onomatopoeia), "onomatopoeia");
  EXPECT_STREQ(originToString(CandidateOrigin::CharacterSpeech), "char_speech");
  EXPECT_STREQ(originToString(CandidateOrigin::Split), "split");
  EXPECT_STREQ(originToString(CandidateOrigin::Join), "join");
  EXPECT_STREQ(originToString(CandidateOrigin::KanjiHiraganaCompound), "kanji_hira_compound");
  EXPECT_STREQ(originToString(CandidateOrigin::Counter), "counter");
  EXPECT_STREQ(originToString(CandidateOrigin::PrefixCompound), "prefix_compound");
}

// =============================================================================
// extendedPosToString - representative sample across categories
// =============================================================================

TEST(TypesExtendedTest, ExtendedPosToStringVerbForms) {
  EXPECT_EQ(extendedPosToString(ExtendedPOS::VerbShuushikei), "VERB_終止");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::VerbRenyokei), "VERB_連用");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::VerbMizenkei), "VERB_未然");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::VerbOnbinkei), "VERB_音便");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::VerbTeForm), "VERB_て形");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::VerbKateikei), "VERB_仮定");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::VerbMeireikei), "VERB_命令");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::VerbRentaikei), "VERB_連体");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::VerbTaForm), "VERB_た形");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::VerbTaraForm), "VERB_たら形");
}

TEST(TypesExtendedTest, ExtendedPosToStringAdjForms) {
  EXPECT_EQ(extendedPosToString(ExtendedPOS::AdjBasic), "ADJ_終止");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::AdjRenyokei), "ADJ_連用");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::AdjStem), "ADJ_語幹");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::AdjKatt), "ADJ_かっ");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::AdjKeForm), "ADJ_け形");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::AdjNaAdj), "ADJ_NA");
}

TEST(TypesExtendedTest, ExtendedPosToStringAuxiliaries) {
  EXPECT_EQ(extendedPosToString(ExtendedPOS::AuxTenseTa), "AUX_過去");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::AuxNegativeNai), "AUX_否定");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::AuxDesireTai), "AUX_願望");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::AuxPassive), "AUX_受身");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::AuxCausative), "AUX_使役");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::AuxAspectIru), "AUX_継続");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::AuxCopulaDa), "AUX_断定");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::AuxExcessive), "AUX_過度");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::AuxGaru), "AUX_ガル");
}

TEST(TypesExtendedTest, ExtendedPosToStringParticles) {
  EXPECT_EQ(extendedPosToString(ExtendedPOS::ParticleCase), "PART_格");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::ParticleTopic), "PART_係");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::ParticleFinal), "PART_終");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::ParticleConj), "PART_接続");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::ParticleNo), "PART_準体");
}

TEST(TypesExtendedTest, ExtendedPosToStringOther) {
  EXPECT_EQ(extendedPosToString(ExtendedPOS::Noun), "NOUN");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::NounFormal), "NOUN_形式");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::Pronoun), "PRON");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::Adverb), "ADV");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::Conjunction), "CONJ");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::Determiner), "DET");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::Symbol), "SYMBOL");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::Unknown), "UNKNOWN");
  EXPECT_EQ(extendedPosToString(ExtendedPOS::Count_), "UNKNOWN");
}

// =============================================================================
// extendedPosToPos - all ExtendedPOS to POS mappings
// =============================================================================

TEST(TypesExtendedTest, ExtendedPosToPosVerbForms) {
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::VerbShuushikei), PartOfSpeech::Verb);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::VerbRenyokei), PartOfSpeech::Verb);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::VerbMizenkei), PartOfSpeech::Verb);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::VerbOnbinkei), PartOfSpeech::Verb);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::VerbTeForm), PartOfSpeech::Verb);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::VerbKateikei), PartOfSpeech::Verb);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::VerbMeireikei), PartOfSpeech::Verb);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::VerbRentaikei), PartOfSpeech::Verb);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::VerbTaForm), PartOfSpeech::Verb);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::VerbTaraForm), PartOfSpeech::Verb);
}

TEST(TypesExtendedTest, ExtendedPosToPosAdjForms) {
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AdjBasic), PartOfSpeech::Adjective);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AdjRenyokei), PartOfSpeech::Adjective);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AdjStem), PartOfSpeech::Adjective);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AdjKatt), PartOfSpeech::Adjective);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AdjKeForm), PartOfSpeech::Adjective);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AdjNaAdj), PartOfSpeech::Adjective);
}

TEST(TypesExtendedTest, ExtendedPosToPosAuxiliaries) {
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AuxTenseTa), PartOfSpeech::Auxiliary);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AuxTenseMasu), PartOfSpeech::Auxiliary);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AuxNegativeNai), PartOfSpeech::Auxiliary);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AuxNegativeNu), PartOfSpeech::Auxiliary);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AuxDesireTai), PartOfSpeech::Auxiliary);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AuxVolitional), PartOfSpeech::Auxiliary);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AuxPassive), PartOfSpeech::Auxiliary);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AuxCausative), PartOfSpeech::Auxiliary);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AuxPotential), PartOfSpeech::Auxiliary);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AuxAspectIru), PartOfSpeech::Auxiliary);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AuxAspectShimau), PartOfSpeech::Auxiliary);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AuxAspectOku), PartOfSpeech::Auxiliary);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AuxAspectMiru), PartOfSpeech::Auxiliary);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AuxAspectIku), PartOfSpeech::Auxiliary);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AuxAspectKuru), PartOfSpeech::Auxiliary);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AuxAppearanceSou), PartOfSpeech::Auxiliary);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AuxConjectureRashii), PartOfSpeech::Auxiliary);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AuxConjectureMitai), PartOfSpeech::Auxiliary);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AuxCopulaDa), PartOfSpeech::Auxiliary);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AuxCopulaDesu), PartOfSpeech::Auxiliary);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AuxHonorific), PartOfSpeech::Auxiliary);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AuxGozaru), PartOfSpeech::Auxiliary);
}

TEST(TypesExtendedTest, ExtendedPosToPosAuxExcessiveAndGaruMapToVerb) {
  // AuxExcessive and AuxGaru are grammatically verbs, not auxiliaries
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AuxExcessive), PartOfSpeech::Verb);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AuxGaru), PartOfSpeech::Verb);
}

TEST(TypesExtendedTest, ExtendedPosToPosParticles) {
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::ParticleCase), PartOfSpeech::Particle);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::ParticleTopic), PartOfSpeech::Particle);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::ParticleFinal), PartOfSpeech::Particle);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::ParticleConj), PartOfSpeech::Particle);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::ParticleQuote), PartOfSpeech::Particle);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::ParticleAdverbial), PartOfSpeech::Particle);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::ParticleNo), PartOfSpeech::Particle);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::ParticleBinding), PartOfSpeech::Particle);
}

TEST(TypesExtendedTest, ExtendedPosToPosNouns) {
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::Noun), PartOfSpeech::Noun);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::NounFormal), PartOfSpeech::Noun);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::NounVerbal), PartOfSpeech::Noun);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::NounProper), PartOfSpeech::Noun);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::NounProperFamily), PartOfSpeech::Noun);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::NounProperGiven), PartOfSpeech::Noun);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::NounNumber), PartOfSpeech::Noun);
}

TEST(TypesExtendedTest, ExtendedPosToPosPronouns) {
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::Pronoun), PartOfSpeech::Pronoun);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::PronounInterrogative), PartOfSpeech::Pronoun);
}

TEST(TypesExtendedTest, ExtendedPosToPosOthers) {
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::Adverb), PartOfSpeech::Adverb);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::AdverbQuotative), PartOfSpeech::Adverb);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::Conjunction), PartOfSpeech::Conjunction);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::Determiner), PartOfSpeech::Determiner);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::Prefix), PartOfSpeech::Prefix);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::Suffix), PartOfSpeech::Suffix);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::Symbol), PartOfSpeech::Symbol);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::Interjection), PartOfSpeech::Interjection);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::Other), PartOfSpeech::Other);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::Unknown), PartOfSpeech::Other);
  EXPECT_EQ(extendedPosToPos(ExtendedPOS::Count_), PartOfSpeech::Other);
}

// =============================================================================
// posToExtendedPos - all POS values
// =============================================================================

TEST(TypesExtendedTest, PosToExtendedPosAllValues) {
  EXPECT_EQ(posToExtendedPos(PartOfSpeech::Noun), ExtendedPOS::Noun);
  EXPECT_EQ(posToExtendedPos(PartOfSpeech::Verb), ExtendedPOS::VerbShuushikei);
  EXPECT_EQ(posToExtendedPos(PartOfSpeech::Adjective), ExtendedPOS::AdjBasic);
  EXPECT_EQ(posToExtendedPos(PartOfSpeech::Adverb), ExtendedPOS::Adverb);
  EXPECT_EQ(posToExtendedPos(PartOfSpeech::Particle), ExtendedPOS::ParticleCase);
  EXPECT_EQ(posToExtendedPos(PartOfSpeech::Auxiliary), ExtendedPOS::AuxTenseTa);
  EXPECT_EQ(posToExtendedPos(PartOfSpeech::Pronoun), ExtendedPOS::Pronoun);
  EXPECT_EQ(posToExtendedPos(PartOfSpeech::Conjunction), ExtendedPOS::Conjunction);
  EXPECT_EQ(posToExtendedPos(PartOfSpeech::Determiner), ExtendedPOS::Determiner);
  EXPECT_EQ(posToExtendedPos(PartOfSpeech::Prefix), ExtendedPOS::Prefix);
  EXPECT_EQ(posToExtendedPos(PartOfSpeech::Suffix), ExtendedPOS::Suffix);
  EXPECT_EQ(posToExtendedPos(PartOfSpeech::Interjection), ExtendedPOS::Interjection);
  EXPECT_EQ(posToExtendedPos(PartOfSpeech::Symbol), ExtendedPOS::Symbol);
  EXPECT_EQ(posToExtendedPos(PartOfSpeech::Other), ExtendedPOS::Other);
  EXPECT_EQ(posToExtendedPos(PartOfSpeech::Unknown), ExtendedPOS::Other);
}

// =============================================================================
// Inline helper functions (isVerbForm, isAdjectiveForm, etc.)
// =============================================================================

TEST(TypesExtendedTest, IsVerbForm) {
  EXPECT_TRUE(isVerbForm(ExtendedPOS::VerbShuushikei));
  EXPECT_TRUE(isVerbForm(ExtendedPOS::VerbRenyokei));
  EXPECT_TRUE(isVerbForm(ExtendedPOS::VerbTaraForm));
  EXPECT_FALSE(isVerbForm(ExtendedPOS::AdjBasic));
  EXPECT_FALSE(isVerbForm(ExtendedPOS::Noun));
  EXPECT_FALSE(isVerbForm(ExtendedPOS::Unknown));
}

TEST(TypesExtendedTest, IsAdjectiveForm) {
  EXPECT_TRUE(isAdjectiveForm(ExtendedPOS::AdjBasic));
  EXPECT_TRUE(isAdjectiveForm(ExtendedPOS::AdjNaAdj));
  EXPECT_FALSE(isAdjectiveForm(ExtendedPOS::VerbShuushikei));
  EXPECT_FALSE(isAdjectiveForm(ExtendedPOS::Noun));
}

TEST(TypesExtendedTest, IsAuxiliaryType) {
  EXPECT_TRUE(isAuxiliaryType(ExtendedPOS::AuxTenseTa));
  EXPECT_TRUE(isAuxiliaryType(ExtendedPOS::AuxGozaru));
  // AuxExcessive and AuxGaru are outside the range
  EXPECT_FALSE(isAuxiliaryType(ExtendedPOS::AuxExcessive));
  EXPECT_FALSE(isAuxiliaryType(ExtendedPOS::AuxGaru));
  EXPECT_FALSE(isAuxiliaryType(ExtendedPOS::Noun));
}

TEST(TypesExtendedTest, IsParticleType) {
  EXPECT_TRUE(isParticleType(ExtendedPOS::ParticleCase));
  EXPECT_TRUE(isParticleType(ExtendedPOS::ParticleBinding));
  EXPECT_FALSE(isParticleType(ExtendedPOS::Noun));
  EXPECT_FALSE(isParticleType(ExtendedPOS::AuxTenseTa));
}

TEST(TypesExtendedTest, IsNounType) {
  EXPECT_TRUE(isNounType(ExtendedPOS::Noun));
  EXPECT_TRUE(isNounType(ExtendedPOS::NounNumber));
  EXPECT_FALSE(isNounType(ExtendedPOS::Pronoun));
  EXPECT_FALSE(isNounType(ExtendedPOS::VerbShuushikei));
}

// =============================================================================
// detectVerbForm - various surface/suffix combinations
// =============================================================================

TEST(TypesExtendedTest, DetectVerbFormEmptySurface) {
  EXPECT_EQ(detectVerbForm(""), ExtendedPOS::VerbShuushikei);
}

TEST(TypesExtendedTest, DetectVerbFormSuffixTaraForm) {
  EXPECT_EQ(detectVerbForm("食べ", "たら"), ExtendedPOS::VerbTaraForm);
  EXPECT_EQ(detectVerbForm("読ん", "だら"), ExtendedPOS::VerbTaraForm);
}

TEST(TypesExtendedTest, DetectVerbFormSuffixTaForm) {
  EXPECT_EQ(detectVerbForm("食べ", "た"), ExtendedPOS::VerbTaForm);
  EXPECT_EQ(detectVerbForm("読ん", "だ"), ExtendedPOS::VerbTaForm);
  // With onbin preceding ta/da in suffix
  EXPECT_EQ(detectVerbForm("書", "いた"), ExtendedPOS::VerbTaForm);
  EXPECT_EQ(detectVerbForm("走", "った"), ExtendedPOS::VerbTaForm);
  EXPECT_EQ(detectVerbForm("読", "んだ"), ExtendedPOS::VerbTaForm);
}

TEST(TypesExtendedTest, DetectVerbFormSuffixTeForm) {
  EXPECT_EQ(detectVerbForm("食べ", "て"), ExtendedPOS::VerbTeForm);
  EXPECT_EQ(detectVerbForm("読ん", "で"), ExtendedPOS::VerbTeForm);
}

TEST(TypesExtendedTest, DetectVerbFormSuffixBaForm) {
  EXPECT_EQ(detectVerbForm("食べれ", "ば"), ExtendedPOS::VerbKateikei);
}

TEST(TypesExtendedTest, DetectVerbFormSuffixMasuForm) {
  EXPECT_EQ(detectVerbForm("食べ", "ます"), ExtendedPOS::VerbRenyokei);
  EXPECT_EQ(detectVerbForm("食べ", "まし"), ExtendedPOS::VerbRenyokei);
  EXPECT_EQ(detectVerbForm("食べ", "ませ"), ExtendedPOS::VerbRenyokei);
}

TEST(TypesExtendedTest, DetectVerbFormSuffixNaiForm) {
  EXPECT_EQ(detectVerbForm("食べ", "ない"), ExtendedPOS::VerbMizenkei);
  EXPECT_EQ(detectVerbForm("書か", "なかっ"), ExtendedPOS::VerbMizenkei);
}

TEST(TypesExtendedTest, DetectVerbFormSuffixVoiceForm) {
  EXPECT_EQ(detectVerbForm("書か", "れる"), ExtendedPOS::VerbMizenkei);
  EXPECT_EQ(detectVerbForm("食べ", "られ"), ExtendedPOS::VerbMizenkei);
  EXPECT_EQ(detectVerbForm("書か", "せる"), ExtendedPOS::VerbMizenkei);
  EXPECT_EQ(detectVerbForm("食べ", "させ"), ExtendedPOS::VerbMizenkei);
}

TEST(TypesExtendedTest, DetectVerbFormSurfaceOnbin) {
  EXPECT_EQ(detectVerbForm("走っ"), ExtendedPOS::VerbOnbinkei);
  EXPECT_EQ(detectVerbForm("読ん"), ExtendedPOS::VerbOnbinkei);
}

TEST(TypesExtendedTest, DetectVerbFormSurfaceKanjiIOnbin) {
  // Kanji + い is onbinkei (godan i-onbin like 書い from 書く)
  EXPECT_EQ(detectVerbForm("書い"), ExtendedPOS::VerbOnbinkei);
}

TEST(TypesExtendedTest, DetectVerbFormSurfaceHiraganaIEnding) {
  // All-hiragana い-ending is renyokei (ichidan stems)
  EXPECT_EQ(detectVerbForm("たべい"), ExtendedPOS::VerbRenyokei);
}

TEST(TypesExtendedTest, DetectVerbFormSurfaceTeDeForm) {
  EXPECT_EQ(detectVerbForm("食べて"), ExtendedPOS::VerbTeForm);
  EXPECT_EQ(detectVerbForm("読んで"), ExtendedPOS::VerbTeForm);
}

TEST(TypesExtendedTest, DetectVerbFormSurfaceBaForm) {
  EXPECT_EQ(detectVerbForm("書けば"), ExtendedPOS::VerbKateikei);
}

TEST(TypesExtendedTest, DetectVerbFormSurfaceTaDaForm) {
  EXPECT_EQ(detectVerbForm("食べた"), ExtendedPOS::VerbTaForm);
  EXPECT_EQ(detectVerbForm("読んだ"), ExtendedPOS::VerbTaForm);
}

TEST(TypesExtendedTest, DetectVerbFormSurfaceImperative) {
  EXPECT_EQ(detectVerbForm("食べろ"), ExtendedPOS::VerbMeireikei);
  EXPECT_EQ(detectVerbForm("見よ"), ExtendedPOS::VerbMeireikei);
}

TEST(TypesExtendedTest, DetectVerbFormSurfaceRuEnding) {
  EXPECT_EQ(detectVerbForm("食べる"), ExtendedPOS::VerbShuushikei);
}

TEST(TypesExtendedTest, DetectVerbFormSurfaceStem) {
  // Short forms without known endings default to renyokei
  EXPECT_EQ(detectVerbForm("食べ"), ExtendedPOS::VerbRenyokei);
}

// =============================================================================
// detectAdjForm - various surface patterns
// =============================================================================

TEST(TypesExtendedTest, DetectAdjFormNaAdj) {
  EXPECT_EQ(detectAdjForm("静か", true), ExtendedPOS::AdjNaAdj);
}

TEST(TypesExtendedTest, DetectAdjFormEmptySurface) {
  EXPECT_EQ(detectAdjForm(""), ExtendedPOS::AdjBasic);
}

TEST(TypesExtendedTest, DetectAdjFormKattForm) {
  EXPECT_EQ(detectAdjForm("高かっ"), ExtendedPOS::AdjKatt);
}

TEST(TypesExtendedTest, DetectAdjFormKeForm) {
  EXPECT_EQ(detectAdjForm("高けれ"), ExtendedPOS::AdjKeForm);
  EXPECT_EQ(detectAdjForm("高きゃ"), ExtendedPOS::AdjKeForm);
}

TEST(TypesExtendedTest, DetectAdjFormRenyokei) {
  EXPECT_EQ(detectAdjForm("高く"), ExtendedPOS::AdjRenyokei);
}

TEST(TypesExtendedTest, DetectAdjFormBasicI) {
  EXPECT_EQ(detectAdjForm("高い"), ExtendedPOS::AdjBasic);
}

TEST(TypesExtendedTest, DetectAdjFormStemFallback) {
  // Surface not ending in known patterns falls back to stem
  EXPECT_EQ(detectAdjForm("高"), ExtendedPOS::AdjStem);
}

}  // namespace
}  // namespace core
}  // namespace suzume
