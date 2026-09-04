/**
 * @file char_patterns.cpp
 * @brief Character pattern utilities for Japanese verb/adjective analysis
 */

#include "char_patterns.h"

#include <algorithm>
#include <array>

#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "normalize/utf8.h"

namespace suzume::grammar {

using normalize::encodeUtf8;

namespace {

// =============================================================================
// Character Iteration Templates
// =============================================================================

/**
 * @brief Check if all characters in string match a predicate
 * Iterates through 3-byte UTF-8 sequences (Japanese characters)
 */
template <typename Predicate>
bool allCharsMatch(std::string_view str, Predicate pred) {
  if (str.empty())
    return false;
  size_t pos = 0;
  while (pos < str.size()) {
    if (!utf8::is3ByteUtf8At(str, pos))
      return false;
    char32_t cp = utf8::decode3ByteUtf8At(str, pos);
    if (!pred(cp))
      return false;
    pos += core::kJapaneseCharBytes;
  }
  return true;
}

/**
 * @brief Check if any character in string matches a predicate
 * Handles mixed-byte strings (skips non-3-byte sequences)
 */
template <typename Predicate>
bool anyCharMatches(std::string_view str, Predicate pred) {
  if (str.empty())
    return false;
  size_t pos = 0;
  while (pos + core::kJapaneseCharBytes <= str.size()) {
    if (utf8::is3ByteUtf8At(str, pos)) {
      char32_t cp = utf8::decode3ByteUtf8At(str, pos);
      if (pred(cp))
        return true;
      pos += core::kJapaneseCharBytes;
    } else {
      pos += 1;
    }
  }
  return false;
}

}  // namespace

bool endsWithIRow(std::string_view stem) {
  const char32_t codepoint = utf8::decodeLastChar(stem);
  return codepoint != 0 && kana::isIRowCodepoint(codepoint);
}

bool endsWithERow(std::string_view stem) {
  const char32_t codepoint = utf8::decodeLastChar(stem);
  return codepoint != 0 && kana::isERowCodepoint(codepoint);
}

bool endsWithOnbin(std::string_view stem) {
  const char32_t codepoint = utf8::decodeLastChar(stem);
  return codepoint != 0 && kana::isOnbinCodepoint(codepoint);
}

bool endsWithRenyokeiMarker(std::string_view stem) {
  return endsWithIRow(stem) || endsWithERow(stem);
}

bool isERowCodepoint(char32_t cp) {
  return kana::isERowCodepoint(cp);
}

bool isIRowCodepoint(char32_t cp) {
  return kana::isIRowCodepoint(cp);
}

bool isARowCodepoint(char32_t cp) {
  return kana::isARowCodepoint(cp);
}

bool isORowCodepoint(char32_t cp) {
  return kana::isORowCodepoint(cp);
}

bool endsWithChar(std::string_view stem, const char* const chars[], size_t count) {
  if (stem.size() < core::kJapaneseCharBytes) {
    return false;
  }
  std::string_view last = utf8::lastChar(stem);
  for (size_t idx = 0; idx < count; ++idx) {
    if (last == chars[idx]) {
      return true;
    }
  }
  return false;
}

bool isAllKanji(std::string_view stem) {
  return allCharsMatch(stem, kana::isKanjiCodepoint);
}

bool endsWithKanji(std::string_view stem) {
  char32_t cp = utf8::decodeLastChar(stem);
  return cp != 0 && kana::isKanjiCodepoint(cp);
}

bool startsWithKanji(std::string_view stem) {
  char32_t cp = utf8::decodeFirstChar(stem);
  return cp != 0 && kana::isKanjiCodepoint(cp);
}

bool containsKanji(std::string_view stem) {
  return anyCharMatches(stem, kana::isKanjiCodepoint);
}

bool isPureHiragana(std::string_view stem) {
  return allCharsMatch(stem, kana::isHiraganaCodepoint);
}

bool isQuotativeSuruTeCompoundParticle(std::string_view surface) {
  return surface == "として";
}

bool isSuruRenyokeiSurface(std::string_view surface) {
  return surface == "し";
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
  size_t byte_pos = 0;
  return normalize::decodeUtf8(surface, byte_pos) == U'ご' && byte_pos == surface.size();
}

bool isBoundVerbPrefix(std::string_view surface) {
  // Kanji that only ever open a compound verb. They have no standalone nominal
  // use that could stand as the verb's argument, so the split a free noun would
  // license (血+浴びる) is not available to them (仕上げる, 片付ける).
  size_t byte_pos = 0;
  const char32_t prefix = normalize::decodeUtf8(surface, byte_pos);
  return byte_pos == surface.size() && (prefix == U'仕' || prefix == U'片');
}

bool isLeftBranchingPrefixKanji(char32_t code) {
  // Kanji that only ever open a modification. They scope rightward over whatever
  // follows and have no compound-final use of their own, so a nominal run that
  // closes on one has crossed a word boundary (仕事|超|忙しい, 会議|各部署).
  // Kanji that also end compounds (完全, 過激, 究極) are deliberately absent: for
  // those the run-final position is a real reading, not a boundary error.
  constexpr std::array<char32_t, 3> kLeftBranchingPrefixes = {U'超', U'各', U'諸'};
  return std::find(kLeftBranchingPrefixes.begin(), kLeftBranchingPrefixes.end(), code) != kLeftBranchingPrefixes.end();
}

bool isBigradeTerminalKana(char32_t code) {
  // す is left out: サ行 bigrade is marginal, while する is the light verb every
  // sahen nominal takes, so the row would claim that construction instead.
  constexpr std::array<char32_t, 11> kBigradeTerminals = {U'う', U'く', U'ぐ', U'つ', U'づ', U'ぬ',
                                                          U'ふ', U'ぶ', U'む', U'ゆ', U'る'};
  return std::find(kBigradeTerminals.begin(), kBigradeTerminals.end(), code) != kBigradeTerminals.end();
}

bool isModernGodanTerminalKana(char32_t code) {
  constexpr std::array<char32_t, 9> kGodanTerminals = {U'う', U'く', U'ぐ', U'す', U'つ', U'ぬ', U'ぶ', U'む', U'る'};
  return std::find(kGodanTerminals.begin(), kGodanTerminals.end(), code) != kGodanTerminals.end();
}

bool isMonogradeStemFinalKana(char32_t code) {
  constexpr std::array<char32_t, 2> kShiftedRow = {U'ひ', U'へ'};
  return (isIRowCodepoint(code) || isERowCodepoint(code)) &&
         std::find(kShiftedRow.begin(), kShiftedRow.end(), code) == kShiftedRow.end();
}

bool isClassicalAuxiliaryHomographKana(char32_t code) {
  constexpr std::array<char32_t, 6> kAuxiliaryHomographs = {U'す', U'つ', U'ぬ', U'ふ', U'む', U'る'};
  return std::find(kAuxiliaryHomographs.begin(), kAuxiliaryHomographs.end(), code) != kAuxiliaryHomographs.end();
}

bool isKanjiHonorificTitle(std::string_view surface) {
  return surface == "様" || surface == "氏";
}

bool isAttributiveCopulaNa(std::string_view surface) {
  return surface == "な";
}

bool startsPredicativeCopula(std::string_view surface) {
  return surface.rfind("だ", 0) == 0 || surface.rfind("です", 0) == 0 || surface.rfind("である", 0) == 0;
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
  return isTeDeSurface(surface) || surface == "てる";
}

bool isDialectalOruContractionLemma(std::string_view lemma) {
  return lemma == "とる" || lemma == "どる";
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

bool isListingParticleTariSurface(std::string_view surface) {
  return surface == "たり";
}

bool isHypotheticalSelectingConjunctiveParticle(std::string_view surface) {
  return utf8::equalsAny(surface, {"ば", "ど", "ども", "り"});
}

bool spellsHypotheticalAuxiliaryCell(std::string_view surface) {
  return utf8::endsWithAny(surface, {"れれ", "たれ", "るれ"});
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
    // The vowel that lengthens a mora is the one its own row carries.
    const bool matches_row = (kana::isARowCodepoint(head) && lengthening == U'あ') ||
                             (kana::isERowCodepoint(head) && lengthening == U'え') ||
                             (kana::isORowCodepoint(head) && lengthening == U'お');
    if (lengthening != U'ー' && !matches_row) {
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

bool startsInterrogativeQuoteIntroduction(std::string_view surface) {
  return utf8::startsWith(surface, "かというと");
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

bool startsContractedNjaNegative(std::string_view surface) {
  return utf8::startsWith(surface, "んじゃない");
}

bool isPureKatakana(std::string_view stem) {
  return allCharsMatch(stem, kana::isKatakanaCodepoint);
}

bool isSmallKana(std::string_view ch) {
  char32_t cp = utf8::decodeFirstChar(ch);
  return cp != 0 && kana::isSmallKanaCodepoint(cp);
}

// A-row (あ段) endings for Godan mizenkei detection.
// This is a DELIBERATE subset of the full phonological a-row recognized by
// kana::isARowCodepoint, which also carries だ/ざ/は/ぱ/や — none of which are
// Godan mizenkei endings.
// In particular だ (copula) and は must NOT match here, so this cannot be
// replaced by the kana::isARowCodepoint predicate the way endsWithORow uses
// isORowCodepoint. The curated list is the source of truth for this grammar.
const char* kARowEndings[] = {"あ", "か", "が", "さ", "た", "な", "ば", "ま", "ら", "わ"};
const size_t kARowCount = 10;

bool endsWithARow(std::string_view stem) {
  return endsWithChar(stem, kARowEndings, kARowCount);
}

// O-row (お段) ending: the mizenkei a Godan verb takes before volitional う.
// Shares the kana::isORowCodepoint source of truth.
bool endsWithORow(std::string_view stem) {
  char32_t cp = utf8::decodeLastChar(stem);
  return cp != 0 && kana::isORowCodepoint(cp);
}

bool isSingleHiragana(std::string_view text, char32_t codepoint) {
  return text.size() == core::kJapaneseCharBytes && utf8::decode3ByteUtf8At(text, 0) == codepoint;
}

char32_t getVowelForChar(char32_t ch) {
  if (kana::isARowCodepoint(ch)) {
    return U'あ';
  }
  if (kana::isIRowCodepoint(ch)) {
    return U'い';
  }
  if (kana::isURowCodepoint(ch)) {
    return U'う';
  }
  if (kana::isERowCodepoint(ch)) {
    return U'え';
  }
  if (kana::isORowCodepoint(ch)) {
    return U'お';
  }

  // Small kana (ゃゅょ) - treat as their base vowel
  if (ch == U'ゃ')
    return U'あ';
  if (ch == U'ゅ')
    return U'う';
  if (ch == U'ょ')
    return U'お';

  // Default to the character itself if not recognized
  return ch;
}

namespace {

enum class GodanColumn : uint8_t { Base, A, I, E };

using EncodedGodanRow = std::array<std::string, 4>;

const std::array<EncodedGodanRow, Conjugation::kGodanRowCount>& encodedGodanRows() {
  static const std::array<EncodedGodanRow, Conjugation::kGodanRowCount> kEncodedRows = []() {
    std::array<EncodedGodanRow, Conjugation::kGodanRowCount> rows;
    size_t index = 0;
    for (const auto& [type, row] : Conjugation::getGodanRows()) {
      (void)type;
      rows[index++] = {encodeUtf8(row.base_vowel), encodeUtf8(row.a_row), encodeUtf8(row.i_row), encodeUtf8(row.e_row)};
    }
    return rows;
  }();
  return kEncodedRows;
}

char32_t codepointAt(const Conjugation::GodanRow& row, GodanColumn column) {
  switch (column) {
    case GodanColumn::Base:
      return row.base_vowel;
    case GodanColumn::A:
      return row.a_row;
    case GodanColumn::I:
      return row.i_row;
    case GodanColumn::E:
      return row.e_row;
  }
  return 0;
}

size_t columnIndex(GodanColumn column) {
  return static_cast<size_t>(column);
}

std::string_view lookupGodanSuffix(char32_t key, GodanColumn key_column, GodanColumn result_column) {
  const auto& rows = Conjugation::getGodanRows();
  const auto& encoded_rows = encodedGodanRows();
  for (size_t index = 0; index < rows.size(); ++index) {
    if (codepointAt(rows[index].second, key_column) == key) {
      return encoded_rows[index][columnIndex(result_column)];
    }
  }
  return {};
}

VerbType lookupGodanType(char32_t key, GodanColumn key_column) {
  for (const auto& [type, row] : Conjugation::getGodanRows()) {
    if (codepointAt(row, key_column) == key) {
      return type;
    }
  }
  return VerbType::Unknown;
}

}  // namespace

std::string_view godanARowSuffixFromURow(char32_t u_row_cp) {
  return lookupGodanSuffix(u_row_cp, GodanColumn::Base, GodanColumn::A);
}

std::string_view godanIRowSuffixFromURow(char32_t u_row_cp) {
  return lookupGodanSuffix(u_row_cp, GodanColumn::Base, GodanColumn::I);
}

std::string_view godanBaseSuffixFromARow(char32_t a_row_cp) {
  return lookupGodanSuffix(a_row_cp, GodanColumn::A, GodanColumn::Base);
}

VerbType verbTypeFromARowCodepoint(char32_t a_row_cp) {
  return lookupGodanType(a_row_cp, GodanColumn::A);
}

std::string_view godanBaseSuffixFromIRow(char32_t i_row_cp) {
  return lookupGodanSuffix(i_row_cp, GodanColumn::I, GodanColumn::Base);
}

std::string_view godanBaseSuffixFromERow(char32_t e_row_cp) {
  return lookupGodanSuffix(e_row_cp, GodanColumn::E, GodanColumn::Base);
}

VerbType verbTypeFromIRowCodepoint(char32_t i_row_cp) {
  return lookupGodanType(i_row_cp, GodanColumn::I);
}

VerbType verbTypeFromBaseCodepoint(char32_t base_cp) {
  const VerbType verb_type = lookupGodanType(base_cp, GodanColumn::Base);
  // Dictionary-form る is ambiguous between GodanRa and Ichidan, and the
  // dictionary entry intentionally carries no conjugation type. Do not guess.
  return verb_type == VerbType::GodanRa ? VerbType::Unknown : verb_type;
}

bool isMixedHiraganaKanji(std::string_view stem) {
  bool has_hiragana = false;
  bool has_kanji = false;
  size_t pos = 0;
  while (pos + core::kJapaneseCharBytes <= stem.size()) {
    if (utf8::is3ByteUtf8At(stem, pos)) {
      char32_t cp = utf8::decode3ByteUtf8At(stem, pos);
      if (kana::isHiraganaCodepoint(cp)) {
        has_hiragana = true;
      } else if (kana::isKanjiCodepoint(cp)) {
        has_kanji = true;
      }
      if (has_hiragana && has_kanji)
        return true;
      pos += core::kJapaneseCharBytes;
    } else {
      pos += 1;
    }
  }
  return false;
}

bool isRenyokeiNominalizingSuffix(std::string_view suffix) {
  return suffix == "気味";
}

}  // namespace suzume::grammar
