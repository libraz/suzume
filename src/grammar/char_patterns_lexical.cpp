/**
 * @file char_patterns_lexical.cpp
 * @brief Lexical character-pattern predicates
 */

#include <array>

#include "char_patterns.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"

namespace suzume {
namespace grammar {

namespace {

/// The codepoint the text consists of, or 0 when it is not exactly one codepoint.
char32_t soleCodepoint(std::string_view text) {
  size_t byte_pos = 0;
  const char32_t codepoint = normalize::decodeUtf8(text, byte_pos);
  return !text.empty() && byte_pos == text.size() ? codepoint : 0;
}

}  // namespace

bool isQuotativeSuruTeCompoundParticle(std::string_view surface) {
  return surface == "として";
}

bool isAttributiveCompoundParticleSurface(std::string_view surface) {
  return normalize::utf8Length(surface) >= 3 && utf8::endsWith(surface, "る");
}

bool isSuruRenyokeiSurface(std::string_view surface) {
  return surface == "し";
}

bool isSuruMizenkeiSurface(std::string_view surface) {
  return surface == "せ";
}

bool startsWithCollectiveQuantifier(std::string_view surface) {
  if (surface.empty()) {
    return false;
  }
  const char32_t first = utf8::decodeFirstChar(surface);
  return first == U'両' || first == U'全' || first == U'双' || first == U'各';
}

bool isEpistolaryTerminalSurface(std::string_view surface) {
  return surface == "候";
}

bool isSuruBaseForm(std::string_view surface) {
  return surface == "する";
}

bool isSuruVolitionalStemSurface(std::string_view surface) {
  return surface == "しよ";
}

bool isSuruImperativeSurface(std::string_view surface) {
  return surface == "せよ" || surface == "しろ";
}

bool isConjunctiveParticleShi(std::string_view surface) {
  return surface == "し";
}

bool isDemonstrativeUAdverb(std::string_view surface) {
  return surface == "こう" || surface == "そう" || surface == "どう";
}

bool isHonorificPrefix(std::string_view surface) {
  return surface == "お" || surface == "ご";
}

bool isSinoHonorificPrefix(std::string_view surface) {
  return soleCodepoint(surface) == U'ご';
}

bool startsWithSuperlativePrefix(std::string_view surface) {
  size_t byte_pos = 0;
  return normalize::decodeUtf8(surface, byte_pos) == U'最';
}

bool isSuperlativePrefix(std::string_view surface) {
  return soleCodepoint(surface) == U'最';
}

bool isBoundVerbPrefix(std::string_view surface) {
  // Kanji that only ever open a compound verb. They have no standalone nominal
  // use that could stand as the verb's argument, so the split a free noun would
  // license (血+浴びる) is not available to them (仕上げる, 片付ける).
  const char32_t prefix = soleCodepoint(surface);
  return prefix == U'仕' || prefix == U'片';
}

bool isLeftBranchingPrefixKanji(char32_t code) {
  // Kanji that only ever open a modification. They scope rightward over whatever
  // follows and have no compound-final use of their own, so a nominal run that
  // closes on one has crossed a word boundary (仕事|超|忙しい, 会議|各部署).
  // Kanji that also end compounds (完全, 過激, 究極) are deliberately absent: for
  // those the run-final position is a real reading, not a boundary error.
  constexpr std::array<char32_t, 3> kLeftBranchingPrefixes = {U'超', U'各', U'諸'};
  return kana::isCodepointIn(kLeftBranchingPrefixes, code);
}

bool isKanjiHonorificTitle(std::string_view surface) {
  return surface == "様" || surface == "氏";
}

bool isAttributiveCopulaNa(std::string_view surface) {
  return surface == "な";
}

bool startsPredicativeCopula(std::string_view surface) {
  return utf8::startsWith(surface, "だ") || utf8::startsWith(surface, "です") || utf8::startsWith(surface, "である");
}

char32_t copulaFusedConjunctionParticle(std::string_view surface) {
  size_t byte_pos = 0;
  if (normalize::decodeUtf8(surface, byte_pos) != U'で') {
    return 0;
  }
  const char32_t binding_particle = normalize::decodeUtf8(surface, byte_pos);
  if (byte_pos != surface.size() || (binding_particle != U'も' && binding_particle != U'は')) {
    return 0;
  }
  return binding_particle;
}

bool isCopulaFusedConjunction(std::string_view surface) {
  return copulaFusedConjunctionParticle(surface) != 0;
}

bool isCopulaPeriphrasisCell(std::string_view surface) {
  size_t byte_pos = 0;
  if (normalize::decodeUtf8(surface, byte_pos) != U'あ') {
    return false;
  }
  const char32_t cell_ending = normalize::decodeUtf8(surface, byte_pos);
  return byte_pos == surface.size() && (cell_ending == U'る' || cell_ending == U'り' || cell_ending == U'っ' ||
                                        cell_ending == U'ろ' || cell_ending == U'れ');
}

bool isConditionalToConjunction(std::string_view surface) {
  return isPureHiragana(surface) && utf8::endsWith(surface, "と");
}

bool isBenefactiveFormalNoun(std::string_view surface) {
  size_t byte_pos = 0;
  return normalize::decodeUtf8(surface, byte_pos) == U'お' && normalize::decodeUtf8(surface, byte_pos) == U'か' &&
         normalize::decodeUtf8(surface, byte_pos) == U'げ' && byte_pos == surface.size();
}

bool isSubstantiveFormalNoun(std::string_view surface) {
  return utf8::equalsAny(surface, {"もの", "物", "こと", "事"});
}

bool isCopularPredicateFormalNoun(std::string_view surface) {
  return utf8::equalsAny(surface, {"わけ", "はず", "つもり"});
}

bool endsInTerminalCellKana(std::string_view surface) {
  const char32_t last = utf8::decodeLastChar(surface);
  return last == U'る' || last == U'い';
}

bool selectsVerbContinuative(std::string_view surface) {
  return utf8::equalsAny(surface, {"よう", "様", "もの", "物", "ごろ", "どき", "どころ", "仕方", "しかた"});
}

bool isIndependentNegativeAdjective(std::string_view surface) {
  return surface == "ない";
}

bool isAruHypotheticalStem(std::string_view surface) {
  return surface == "あれ";
}

bool isAruHypotheticalSurface(std::string_view surface) {
  return surface == "あれば";
}

bool isAruContinuativeSurface(std::string_view surface) {
  return surface == "あり";
}

bool endsWithNegativeNai(std::string_view surface) {
  return utf8::endsWith(surface, "ない");
}

bool isClassicalCausativeAuxiliaryLemma(std::string_view lemma) {
  return lemma == "す";
}

bool isContractedNegativeAuxiliaryLemma(std::string_view lemma) {
  return lemma == "ん";
}

bool isTeDeSurface(std::string_view surface) {
  return surface == "て" || surface == "で";
}

bool formsPoliteCopulaDesu(std::string_view left, std::string_view right) {
  return left == "で" && right == "す";
}

bool isDirectAttachmentTemporalSuffix(std::string_view surface) {
  return surface == "後";
}

bool isContractedProgressiveSurface(std::string_view surface) {
  return isTeDeSurface(surface) || utf8::equalsAny(surface, {"てる", "てろ"});
}

bool isDialectalOruContractionLemma(std::string_view lemma) {
  return lemma == "とる" || lemma == "どる" || lemma == "とう" || lemma == "どう";
}

bool isRenyokeiPotentialAuxiliaryLemma(std::string_view lemma) {
  return lemma == "える" || lemma == "うる" || lemma == "得る";
}

bool isTeFormCompletiveAuxiliaryLemma(std::string_view lemma) {
  return lemma == "しまう" || lemma == "仕舞う" || lemma == "ちゃう" || lemma == "じゃう";
}

bool isPassiveAuxiliaryLemma(std::string_view lemma) {
  return lemma == "られる";
}

bool isAccusativeParticleWoSurface(std::string_view surface) {
  return surface == "を";
}

bool isConcessiveParticleTomoSurface(std::string_view surface) {
  return surface == "とも";
}

bool isClauseDependentConjunctiveParticle(std::string_view surface) {
  return utf8::equalsAny(surface, {"たり", "なり"});
}

bool isHypotheticalSelectingConjunctiveParticle(std::string_view surface) {
  return utf8::equalsAny(surface, {"ば", "ど", "ども", "り"});
}

bool isContinuativeSelectingConjunctiveParticle(std::string_view surface) {
  return utf8::equalsAny(surface, {"ながら", "つつ"});
}

bool spellsHypotheticalAuxiliaryCell(std::string_view surface) {
  return utf8::endsWithAny(surface, {"れれ", "たれ", "るれ"});
}

bool spellsClassicalPerfectContinuative(std::string_view surface) {
  return utf8::equalsAny(surface, {"に"});
}

bool spellsClassicalPastIrrealis(std::string_view surface) {
  return utf8::equalsAny(surface, {"せ"});
}

bool isColloquialConditionalNegativeSurface(std::string_view surface) {
  return surface == "なきゃ" || surface == "なけりゃ";
}

bool isPastMarkerTaDaSurface(std::string_view surface) {
  return surface == "た" || surface == "だ";
}

bool isParallelTogetherAdverb(std::string_view surface) {
  return surface == "ともに";
}

bool isStateDurationSuffix(std::string_view surface) {
  return surface == "中";
}

bool isDeverbalNominalSuffix(std::string_view surface) {
  return utf8::equalsAny(surface, {"事"});
}

bool isFormalNounConjunctiveParticle(std::string_view surface) {
  return utf8::equalsAny(surface, {"ものの"});
}

bool isGeminateAssimilatedCopula(std::string_view surface) {
  return utf8::equalsAny(surface, {"ちゃ"});
}

bool isContractedTeWaParticle(std::string_view surface) {
  return utf8::equalsAny(surface, {"ちゃ", "じゃ"});
}

bool isDurationPredicateKakaru(std::string_view surface) {
  return surface == "かかる";
}

bool isFinalParticleStackTail(std::string_view surface) {
  // The tail slot of a final-particle stack carries the modality: the
  // confirmation-seeking ね/な/よ and the question か (じゃん+か, よ+ね, か+な).
  // Propositional content is always the first member, so nothing else stacks.
  // A final particle lengthens by repeating its own vowel, and the lengthened
  // form fills the same slot (かな/かなあ, よね/よねえ). Reading the tail off
  // the first mora covers both without listing the variants.
  size_t byte_pos = 0;
  const char32_t head = normalize::decodeUtf8(surface, byte_pos);
  if (head != U'ね' && head != U'な' && head != U'よ' && head != U'か') {
    return false;
  }
  while (byte_pos < surface.size()) {
    const char32_t lengthening = normalize::decodeUtf8(surface, byte_pos);
    // The vowel that lengthens a mora is the one its own row carries,
    // written full-size or small (かなあ, かなぁ).
    const bool matches_row = (kana::isARowCodepoint(head) && (lengthening == U'あ' || lengthening == U'ぁ')) ||
                             (kana::isERowCodepoint(head) && (lengthening == U'え' || lengthening == U'ぇ')) ||
                             (kana::isORowCodepoint(head) && (lengthening == U'お' || lengthening == U'ぉ'));
    // So does the emphatic sokuon that closes the utterance (よ+ねっ).
    const bool emphatic_close = lengthening == U'っ' && byte_pos == surface.size();
    if (lengthening != U'ー' && !matches_row && !emphatic_close) {
      return false;
    }
  }
  return true;
}

bool isAmbiguousFinalParticleStackHead(std::string_view surface) {
  return utf8::equalsAny(surface, {"か", "よ", "わ"});
}

bool endsWithAdministrativeSuffix(std::string_view surface) {
  switch (utf8::decodeLastChar(surface)) {
    case U'県':
    case U'都':
    case U'府':
    case U'道':
    case U'市':
    case U'区':
    case U'町':
    case U'村':
      return true;
    default:
      return false;
  }
}

bool startsClassicalDesiderativeSequence(std::string_view surface) {
  return utf8::startsWith(surface, "まほし");
}

bool isClassicalDesiderativeMarker(std::string_view surface) {
  return surface == "ま";
}

bool startsClassicalHonorificSequence(std::string_view surface) {
  return utf8::startsWith(surface, "まふ");
}

bool startsClassicalHonorificAuxiliaryChain(std::string_view surface) {
  return utf8::startsWith(surface, "たまふ");
}

bool isClassicalHonorificComponent(std::string_view surface) {
  return surface == "ま" || surface == "ふ";
}

bool isClassicalFuruTerminal(std::string_view surface) {
  return surface == "ふ";
}

bool startsClassicalAraNLimit(std::string_view surface) {
  return utf8::startsWith(surface, "あらん限り");
}

bool isCausalParticleBeforeTopic(std::string_view particle_surface, std::string_view following_surface) {
  return particle_surface == "ので" && utf8::startsWith(following_surface, "は");
}

namespace {

// The quotative particle has two spellings, と and the colloquial って. Both
// introduce reported speech, so a final particle standing in front of either is
// the same construction.
bool startsQuotativeParticle(std::string_view surface) {
  return utf8::startsWithAny(surface, {"って", "と"});
}

// Whether a closed final particle is followed by the quotative.
bool startsFinalParticleBeforeQuote(std::string_view surface, std::string_view particle) {
  return utf8::startsWith(surface, particle) && startsQuotativeParticle(surface.substr(particle.size()));
}

}  // namespace

bool startsSentenceParticleKanaQuote(std::string_view surface) {
  return startsFinalParticleBeforeQuote(surface, "かな");
}

bool startsContractedDemonstrativeBeforeKa(std::string_view surface) {
  constexpr std::string_view kContraction = "そっか";
  if (!utf8::startsWithAny(surface, {kContraction, "こっか"})) {
    return false;
  }
  // か must close the word: a kana that continues a lexeme (こっかい) is not
  // the contracted question.
  const std::string_view rest = surface.substr(kContraction.size());
  if (rest.empty()) {
    return true;
  }
  size_t pos = 0;
  const char32_t next = normalize::decodeUtf8(rest, pos);
  return normalize::classifyChar(next) != normalize::CharType::Hiragana ||
         utf8::startsWithAny(rest, {"あ", "ぁ", "ね", "な", "よ", "と", "も", "の", "さ", "って"});
}

bool startsInterrogativeQuoteIntroduction(std::string_view surface) {
  return utf8::startsWith(surface, "かというと");
}

bool isNominalHostFinalParticle(char32_t mora) {
  return mora == U'ね' || mora == U'よ' || mora == U'さ' || mora == U'な';
}

bool startsClassicalConjecturalAuxiliary(std::string_view surface) {
  return utf8::startsWith(surface, "けむ");
}

bool startsClosedTemporalNominal(std::string_view surface) {
  return utf8::startsWithAny(surface, {"前", "後", "時", "頃", "ころ", "ごろ", "どき"});
}

std::string_view longFinalParticleBeforeQuote(std::string_view surface) {
  constexpr std::string_view kParticles[] = {"なあ", "ねえ"};
  for (const auto particle : kParticles) {
    if (startsFinalParticleBeforeQuote(surface, particle)) {
      return particle;
    }
  }
  return {};
}

}  // namespace grammar
}  // namespace suzume
