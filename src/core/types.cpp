#include "types.h"

#include "core/utf8_constants.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"

namespace suzume::core {

std::string_view posToString(PartOfSpeech pos) {
  switch (pos) {
    case PartOfSpeech::Noun:
      return "NOUN";
    case PartOfSpeech::Verb:
      return "VERB";
    case PartOfSpeech::Adjective:
      return "ADJ";
    case PartOfSpeech::Adverb:
      return "ADV";
    case PartOfSpeech::Particle:
      return "PARTICLE";
    case PartOfSpeech::Auxiliary:
      return "AUX";
    case PartOfSpeech::Conjunction:
      return "CONJ";
    case PartOfSpeech::Determiner:
      return "DET";
    case PartOfSpeech::Pronoun:
      return "PRON";
    case PartOfSpeech::Prefix:
      return "PREFIX";
    case PartOfSpeech::Suffix:
      return "SUFFIX";
    case PartOfSpeech::Interjection:
      return "INTJ";
    case PartOfSpeech::Symbol:
      return "SYMBOL";
    case PartOfSpeech::Count_:
    case PartOfSpeech::Other:
    case PartOfSpeech::Unknown:
    default:
      return "OTHER";
  }
}

std::string_view posToJapanese(PartOfSpeech pos) {
  switch (pos) {
    case PartOfSpeech::Noun:
      return "名詞";
    case PartOfSpeech::Verb:
      return "動詞";
    case PartOfSpeech::Adjective:
      return "形容詞";
    case PartOfSpeech::Adverb:
      return "副詞";
    case PartOfSpeech::Particle:
      return "助詞";
    case PartOfSpeech::Auxiliary:
      return "助動詞";
    case PartOfSpeech::Conjunction:
      return "接続詞";
    case PartOfSpeech::Determiner:
      return "連体詞";
    case PartOfSpeech::Pronoun:
      return "代名詞";
    case PartOfSpeech::Prefix:
      return "接頭辞";
    case PartOfSpeech::Suffix:
      return "接尾辞";
    case PartOfSpeech::Interjection:
      return "感動詞";
    case PartOfSpeech::Symbol:
      return "記号";
    case PartOfSpeech::Count_:
    case PartOfSpeech::Other:
    case PartOfSpeech::Unknown:
    default:
      return "その他";
  }
}

std::optional<PartOfSpeech> stringToPosStrict(std::string_view str) {
  // Canonical short forms, their long aliases, and Japanese names. PROPN maps
  // to Noun (no dedicated proper-noun POS); OTHER/PHRASE map to Other.
  if (str == "NOUN" || str == "名詞" || str == "PROPN" || str == "PROPER_NOUN") {
    return PartOfSpeech::Noun;
  }
  if (str == "VERB" || str == "動詞") {
    return PartOfSpeech::Verb;
  }
  if (str == "ADJ" || str == "ADJECTIVE" || str == "形容詞") {
    return PartOfSpeech::Adjective;
  }
  if (str == "ADV" || str == "ADVERB" || str == "副詞") {
    return PartOfSpeech::Adverb;
  }
  if (str == "PARTICLE" || str == "助詞") {
    return PartOfSpeech::Particle;
  }
  if (str == "AUX" || str == "AUXILIARY" || str == "助動詞") {
    return PartOfSpeech::Auxiliary;
  }
  if (str == "CONJ" || str == "CONJUNCTION" || str == "接続詞") {
    return PartOfSpeech::Conjunction;
  }
  if (str == "DET" || str == "DETERMINER" || str == "ADNOMINAL" || str == "連体詞") {
    return PartOfSpeech::Determiner;
  }
  if (str == "PRON" || str == "PRONOUN" || str == "代名詞") {
    return PartOfSpeech::Pronoun;
  }
  if (str == "PREFIX" || str == "接頭辞") {
    return PartOfSpeech::Prefix;
  }
  if (str == "SUFFIX" || str == "接尾辞") {
    return PartOfSpeech::Suffix;
  }
  if (str == "INTJ" || str == "INTERJECTION" || str == "感動詞") {
    return PartOfSpeech::Interjection;
  }
  if (str == "SYMBOL" || str == "SYM" || str == "記号") {
    return PartOfSpeech::Symbol;
  }
  if (str == "OTHER" || str == "PHRASE" || str == "その他") {
    return PartOfSpeech::Other;
  }
  return std::nullopt;
}

PartOfSpeech stringToPos(std::string_view str) {
  return stringToPosStrict(str).value_or(PartOfSpeech::Other);
}

ExtendedPOS posToDefaultExtendedPOS(PartOfSpeech pos) {
  // Same POS→EPOS mapping as posToExtendedPos; the only difference is the
  // fallback for unmapped POS (Unknown here vs Other there). No mapped POS
  // yields ExtendedPOS::Other, so remapping Other→Unknown is exact.
  ExtendedPOS epos = posToExtendedPos(pos);
  return epos == ExtendedPOS::Other ? ExtendedPOS::Unknown : epos;
}

bool isTaggable(PartOfSpeech pos) {
  return pos == PartOfSpeech::Noun || pos == PartOfSpeech::Verb || pos == PartOfSpeech::Adjective ||
         pos == PartOfSpeech::Adverb;
}

bool isContentWord(PartOfSpeech pos) {
  return isTaggable(pos);
}

bool isFunctionWord(PartOfSpeech pos) {
  return pos == PartOfSpeech::Particle || pos == PartOfSpeech::Auxiliary;
}

const char* originToString(CandidateOrigin origin) {
  switch (origin) {
    case CandidateOrigin::Dictionary:
      return "dict";
    case CandidateOrigin::VerbKanji:
      return "verb_kanji";
    case CandidateOrigin::VerbHiragana:
      return "verb_hira";
    case CandidateOrigin::VerbHiraganaPassiveRenyokei:
      return "verb_hira_passive_renyo";
    case CandidateOrigin::VerbHiraganaNegativeRenyokei:
      return "verb_hira_negative_renyo";
    case CandidateOrigin::VerbHiraganaInflectedRenyokei:
      return "verb_hira_inflected_renyo";
    case CandidateOrigin::VerbKatakana:
      return "verb_kata";
    case CandidateOrigin::VerbCompound:
      return "verb_compound";
    case CandidateOrigin::AdjectiveI:
      return "adj_i";
    case CandidateOrigin::AdjectiveIHiragana:
      return "adj_i_hira";
    case CandidateOrigin::AdjectiveNa:
      return "adj_na";
    case CandidateOrigin::NominalizedNoun:
      return "noun_nominalized";
    case CandidateOrigin::SuffixPattern:
      return "suffix";
    case CandidateOrigin::SameType:
      return "same_type";
    case CandidateOrigin::Alphanumeric:
      return "alphanum";
    case CandidateOrigin::Onomatopoeia:
      return "onomatopoeia";
    case CandidateOrigin::CharacterSpeech:
      return "char_speech";
    case CandidateOrigin::Split:
      return "split";
    case CandidateOrigin::Join:
      return "join";
    case CandidateOrigin::KanjiHiraganaCompound:
      return "kanji_hira_compound";
    case CandidateOrigin::KanjiHiraganaNominalCompound:
      return "kanji_hira_nominal_compound";
    case CandidateOrigin::SelectedNominalHead:
      return "selected_nominal_head";
    case CandidateOrigin::BracketedNoun:
      return "bracketed_noun";
    case CandidateOrigin::Counter:
      return "counter";
    case CandidateOrigin::PrefixCompound:
      return "prefix_compound";
    case CandidateOrigin::Unknown:
    default:
      return "unknown";
  }
}

// =============================================================================
// ExtendedPOS Helper Functions
// =============================================================================

std::string_view extendedPosToString(ExtendedPOS epos) {
  switch (epos) {
    // Verb forms
    case ExtendedPOS::VerbShuushikei:
      return "VERB_終止";
    case ExtendedPOS::VerbRenyokei:
      return "VERB_連用";
    case ExtendedPOS::VerbMizenkei:
      return "VERB_未然";
    case ExtendedPOS::VerbOnbinkei:
      return "VERB_音便";
    case ExtendedPOS::VerbTeForm:
      return "VERB_て形";
    case ExtendedPOS::VerbKateikei:
      return "VERB_仮定";
    case ExtendedPOS::VerbMeireikei:
      return "VERB_命令";
    case ExtendedPOS::VerbRentaikei:
      return "VERB_連体";
    case ExtendedPOS::VerbTaForm:
      return "VERB_た形";
    case ExtendedPOS::VerbTaraForm:
      return "VERB_たら形";
    case ExtendedPOS::VerbContractedKateikei:
      return "VERB_仮定縮約";

    // Adjective forms
    case ExtendedPOS::AdjBasic:
      return "ADJ_終止";
    case ExtendedPOS::AdjRenyokei:
      return "ADJ_連用";
    case ExtendedPOS::AdjStem:
      return "ADJ_語幹";
    case ExtendedPOS::AdjKatt:
      return "ADJ_かっ";
    case ExtendedPOS::AdjKeForm:
      return "ADJ_け形";
    case ExtendedPOS::AdjMizenkei:
      return "ADJ_未然";
    case ExtendedPOS::AdjNaAdj:
      return "ADJ_NA";

    // Auxiliaries - Tense
    case ExtendedPOS::AuxTenseTa:
      return "AUX_過去";
    case ExtendedPOS::AuxTenseMasu:
      return "AUX_丁寧";
    case ExtendedPOS::AuxKuruwaPolite:
      return "AUX_KURUWA_POLITE";

    // Auxiliaries - Negation
    case ExtendedPOS::AuxNegativeNai:
      return "AUX_否定";
    case ExtendedPOS::AuxNegativeNu:
      return "AUX_否定古";
    case ExtendedPOS::AuxNegativeMai:
      return "AUX_打消推量";
    case ExtendedPOS::AuxClassicalNari:
      return "AUX_文語断定";
    case ExtendedPOS::AuxClassicalKeri:
      return "AUX_文語過去";
    case ExtendedPOS::AuxClassicalTari:
      return "AUX_文語断定連体";
    case ExtendedPOS::AuxClassicalPerfect:
      return "AUX_文語完了";
    case ExtendedPOS::AuxClassicalKi:
      return "AUX_文語過去キ";
    case ExtendedPOS::AuxClassicalBeshi:
      return "AUX_文語当為";

    // Auxiliaries - Desire/Volition
    case ExtendedPOS::AuxDesireTai:
      return "AUX_願望";
    case ExtendedPOS::AuxVolitional:
      return "AUX_意志";

    // Auxiliaries - Voice
    case ExtendedPOS::AuxPassive:
      return "AUX_受身";
    case ExtendedPOS::AuxCausative:
      return "AUX_使役";
    case ExtendedPOS::AuxPotential:
      return "AUX_可能";
    case ExtendedPOS::AuxInability:
      return "AUX_不可能";
    case ExtendedPOS::AuxBenefactive:
      return "AUX_授受";
    case ExtendedPOS::SuffixRecentCompletion:
      return "SUFFIX_直後";
    case ExtendedPOS::SuffixTendency:
      return "SUFFIX_傾向";
    case ExtendedPOS::DeterminerQuotative:
      return "DET_引用";

    // Auxiliaries - Aspect
    case ExtendedPOS::AuxAspectIru:
      return "AUX_継続";
    case ExtendedPOS::AuxAspectShimau:
      return "AUX_完了";
    case ExtendedPOS::AuxAspectOku:
      return "AUX_準備";
    case ExtendedPOS::AuxAspectMiru:
      return "AUX_試行";
    case ExtendedPOS::AuxAspectIku:
      return "AUX_進行";
    case ExtendedPOS::AuxAspectKuru:
      return "AUX_接近";
    case ExtendedPOS::AuxAspectHajimeru:
      return "AUX_開始";

    // Auxiliaries - Appearance/Conjecture
    case ExtendedPOS::AuxAppearanceSou:
      return "AUX_様態";
    case ExtendedPOS::AuxConjectureRashii:
      return "AUX_推定";
    case ExtendedPOS::AuxConjectureMitai:
      return "AUX_みたい";
    case ExtendedPOS::AuxSimilitudeYou:
      return "AUX_よう";

    // Auxiliaries - Copula
    case ExtendedPOS::AuxCopulaDa:
      return "AUX_断定";
    case ExtendedPOS::AuxCopulaDesu:
      return "AUX_丁寧断定";

    // Auxiliaries - Other
    case ExtendedPOS::AuxHonorific:
      return "AUX_尊敬";
    case ExtendedPOS::AuxGozaru:
      return "AUX_丁重";
    case ExtendedPOS::AuxExcessive:
      return "AUX_過度";
    case ExtendedPOS::AuxGaru:
      return "AUX_ガル";

    // Particles
    case ExtendedPOS::ParticleCase:
      return "PART_格";
    case ExtendedPOS::ParticleTopic:
      return "PART_係";
    case ExtendedPOS::ParticleFinal:
      return "PART_終";
    case ExtendedPOS::ParticleConj:
      return "PART_接続";
    case ExtendedPOS::ParticleConjFinite:
      return "PART_接続終止";
    case ExtendedPOS::ParticleQuote:
      return "PART_引用";
    case ExtendedPOS::ParticleAdverbial:
      return "PART_副";
    case ExtendedPOS::ParticleNo:
      return "PART_準体";
    case ExtendedPOS::ParticleBinding:
      return "PART_係結";

    // Nouns
    case ExtendedPOS::Noun:
      return "NOUN";
    case ExtendedPOS::NounFormal:
      return "NOUN_形式";
    case ExtendedPOS::NounVerbal:
      return "NOUN_転成";
    case ExtendedPOS::NounProper:
      return "NOUN_固有";
    case ExtendedPOS::NounProperFamily:
      return "NOUN_姓";
    case ExtendedPOS::NounProperGiven:
      return "NOUN_名";
    case ExtendedPOS::NounNumber:
      return "NOUN_数";

    // Pronouns
    case ExtendedPOS::Pronoun:
      return "PRON";
    case ExtendedPOS::PronounInterrogative:
      return "PRON_疑問";

    // Others
    case ExtendedPOS::Adverb:
      return "ADV";
    case ExtendedPOS::AdverbQuotative:
      return "ADV_引用";
    case ExtendedPOS::Conjunction:
      return "CONJ";
    case ExtendedPOS::Determiner:
      return "DET";
    case ExtendedPOS::Prefix:
      return "PREFIX";
    case ExtendedPOS::Suffix:
      return "SUFFIX";
    case ExtendedPOS::Symbol:
      return "SYMBOL";
    case ExtendedPOS::Interjection:
      return "INTJ";
    case ExtendedPOS::Other:
      return "OTHER";

    case ExtendedPOS::Unknown:
    case ExtendedPOS::Count_:
    default:
      return "UNKNOWN";
  }
}

PartOfSpeech extendedPosToPos(ExtendedPOS epos) {
  if (isVerbForm(epos)) {
    return PartOfSpeech::Verb;
  }
  if (isAdjectiveForm(epos)) {
    return PartOfSpeech::Adjective;
  }
  // AuxExcessive (すぎる), AuxGaru (がる) -> Verb (MeCab: 動詞,非自立/接尾)
  // These are grammatically 補助動詞/接尾動詞, not 助動詞 (auxiliary)
  if (epos == ExtendedPOS::AuxExcessive || epos == ExtendedPOS::AuxGaru) {
    return PartOfSpeech::Verb;
  }
  if (isAuxiliaryType(epos)) {
    return PartOfSpeech::Auxiliary;
  }
  if (isParticleType(epos)) {
    return PartOfSpeech::Particle;
  }
  if (isNounType(epos)) {
    return PartOfSpeech::Noun;
  }
  if (isPronounType(epos)) {
    return PartOfSpeech::Pronoun;
  }

  // Individual mappings
  switch (epos) {
    case ExtendedPOS::Adverb:
    case ExtendedPOS::AdverbQuotative:
      return PartOfSpeech::Adverb;
    case ExtendedPOS::Conjunction:
      return PartOfSpeech::Conjunction;
    case ExtendedPOS::Determiner:
    case ExtendedPOS::DeterminerQuotative:
      return PartOfSpeech::Determiner;
    case ExtendedPOS::Prefix:
      return PartOfSpeech::Prefix;
    case ExtendedPOS::Suffix:
    case ExtendedPOS::SuffixRecentCompletion:
    case ExtendedPOS::SuffixTendency:
      return PartOfSpeech::Suffix;
    case ExtendedPOS::Symbol:
      return PartOfSpeech::Symbol;
    case ExtendedPOS::Interjection:
      return PartOfSpeech::Interjection;
    case ExtendedPOS::Other:
    case ExtendedPOS::Unknown:
    case ExtendedPOS::Count_:
    default:
      return PartOfSpeech::Other;
  }
}

ExtendedPOS posToExtendedPos(PartOfSpeech pos) {
  switch (pos) {
    case PartOfSpeech::Verb:
      return ExtendedPOS::VerbShuushikei;  // Default: dictionary form
    case PartOfSpeech::Adjective:
      return ExtendedPOS::AdjBasic;  // Default: basic form
    case PartOfSpeech::Auxiliary:
      return ExtendedPOS::AuxTenseTa;  // Default: た (most common)
    case PartOfSpeech::Particle:
      return ExtendedPOS::ParticleCase;  // Default: case particle
    case PartOfSpeech::Noun:
      return ExtendedPOS::Noun;
    case PartOfSpeech::Pronoun:
      return ExtendedPOS::Pronoun;
    case PartOfSpeech::Adverb:
      return ExtendedPOS::Adverb;
    case PartOfSpeech::Conjunction:
      return ExtendedPOS::Conjunction;
    case PartOfSpeech::Determiner:
      return ExtendedPOS::Determiner;
    case PartOfSpeech::Prefix:
      return ExtendedPOS::Prefix;
    case PartOfSpeech::Suffix:
      return ExtendedPOS::Suffix;
    case PartOfSpeech::Interjection:
      return ExtendedPOS::Interjection;
    case PartOfSpeech::Symbol:
      return ExtendedPOS::Symbol;
    case PartOfSpeech::Count_:
    case PartOfSpeech::Other:
    case PartOfSpeech::Unknown:
    default:
      return ExtendedPOS::Other;
  }
}

// =============================================================================
// Verb Form Detection Helpers
// =============================================================================

// This detector assigns ExtendedPOS while candidates are built. Postprocessing
// treats that selected ExtendedPOS as authoritative when exposing ConjForm;
// its surface heuristics are only a fallback for legacy morphemes without one.
ExtendedPOS detectVerbForm(std::string_view surface, std::string_view suffix, bool godan_imperative_hint,
                           bool godan_i_onbin_hint) {
  // Empty surface defaults to shuushi
  if (surface.empty()) {
    return ExtendedPOS::VerbShuushikei;
  }

  // Check suffix chain first for more accurate form detection
  if (!suffix.empty()) {
    // たら/だら forms (conditional past)
    if (utf8::endsWithAny(suffix, {"たら", "だら"})) {
      return ExtendedPOS::VerbTaraForm;
    }
    // た/だ forms (past), including onbin variants (書いた, 読んだ)
    if (utf8::endsWithAny(suffix, {"た", "だ"})) {
      return ExtendedPOS::VerbTaForm;
    }
    // て/で forms
    if (utf8::endsWithAny(suffix, {"て", "で"})) {
      return ExtendedPOS::VerbTeForm;
    }
    // ば forms (conditional)
    if (utf8::endsWithAny(suffix, {"ば"})) {
      return ExtendedPOS::VerbKateikei;
    }
    // ます forms indicate renyokei connection
    if (utf8::endsWithAny(suffix, {"ます", "まし", "ませ"})) {
      return ExtendedPOS::VerbRenyokei;
    }
    // ない/なかっ forms indicate mizenkei connection (for godan) or renyokei (ichidan)
    if (utf8::endsWithAny(suffix, {"ない", "なかっ"})) {
      return ExtendedPOS::VerbMizenkei;
    }
    // れる/られる forms indicate mizenkei connection
    if (utf8::endsWithAny(suffix, {"れる", "られ", "せる", "させ"})) {
      return ExtendedPOS::VerbMizenkei;
    }
  }

  // Check surface endings for forms without explicit suffix chain
  // Onbin forms (っ, ん, い at end - before た/て)
  if (utf8::endsWithAny(surface, {"っ", "ん"})) {
    return ExtendedPOS::VerbOnbinkei;
  }
  // Also check for い-onbin (書い from 書く).  The surface alone cannot
  // distinguish it from an ichidan continuative such as 老い/率い/用い, so
  // only candidate generation with known ka/ga-row conjugation may select it.
  if (utf8::endsWithAny(surface, {"い"})) {
    if (godan_i_onbin_hint) {
      return ExtendedPOS::VerbOnbinkei;
    }
    // Without conjugation evidence, preserve the conservative continuative
    // reading rather than fabricating an onbin edge from kanji spelling.
    return ExtendedPOS::VerbRenyokei;
  }

  // A bare e-row surface is ambiguous between a Godan imperative (待て) and
  // an Ichidan continuative (食べ). Candidate generators that know the
  // conjugation type provide the Godan hint; keep unknown forms conservative.
  if (godan_imperative_hint && normalize::utf8Length(surface) > 1 &&
      utf8::endsWithAny(surface, {"え", "け", "げ", "せ", "て", "ね", "べ", "め", "れ"})) {
    return ExtendedPOS::VerbMeireikei;
  }

  // て/で form
  if (utf8::endsWithAny(surface, {"て", "で"})) {
    return ExtendedPOS::VerbTeForm;
  }

  // ば form (conditional)
  if (utf8::endsWithAny(surface, {"ば"})) {
    return ExtendedPOS::VerbKateikei;
  }

  // た/だ form (past)
  if (utf8::endsWithAny(surface, {"た", "だ"})) {
    return ExtendedPOS::VerbTaForm;
  }

  // たら/だら form (conditional past)
  if (utf8::endsWithAny(surface, {"たら", "だら"})) {
    return ExtendedPOS::VerbTaraForm;
  }

  // 命令形 checks - ろ/れ/え for various verb types
  if (utf8::endsWithAny(surface, {"ろ", "よ"})) {
    // Ichidan imperative: 食べろ, 見ろ
    return ExtendedPOS::VerbMeireikei;
  }

  // Godan dictionary forms end in one of these nine u-row kana.  This also
  // covers the shared る ending used by Ichidan dictionary forms.
  if (utf8::endsWithAny(surface, {"う", "く", "ぐ", "す", "つ", "ぬ", "ぶ", "む", "る"})) {
    return ExtendedPOS::VerbShuushikei;
  }

  // Default to renyokei for short forms (verb stems)
  // This handles cases like 食べ, 見, 書き where the surface is just the stem
  return ExtendedPOS::VerbRenyokei;
}

ExtendedPOS detectAdjForm(std::string_view surface, bool is_na_adj) {
  // Na-adjectives always return AdjNaAdj
  if (is_na_adj) {
    return ExtendedPOS::AdjNaAdj;
  }

  // Empty surface defaults to basic form
  if (surface.empty()) {
    return ExtendedPOS::AdjBasic;
  }

  // Check for specific i-adjective endings

  // かっ form (past stem): 美しかっ, 高かっ
  if (utf8::endsWithAny(surface, {"かっ"})) {
    return ExtendedPOS::AdjKatt;
  }

  // けれ form (conditional stem): 美しけれ, 高けれ
  if (utf8::endsWithAny(surface, {"けれ", "きゃ"})) {
    return ExtendedPOS::AdjKeForm;
  }

  // かろ form (irrealis stem for 推量): 美しかろ, 高かろ
  if (utf8::endsWithAny(surface, {"かろ"})) {
    return ExtendedPOS::AdjMizenkei;
  }

  // く form (adverbial/renyokei): 美しく, 高く
  if (utf8::endsWithAny(surface, {"く"})) {
    return ExtendedPOS::AdjRenyokei;
  }

  // い form (basic/shuushi): 美しい, 高い
  if (utf8::endsWithAny(surface, {"い"})) {
    return ExtendedPOS::AdjBasic;
  }

  // Stem forms (for ガル接続): 美し, 高
  // These are identified by not ending in い/く/かっ/けれ
  // But we can't reliably detect this without knowing the full word
  // Default to stem for short forms that don't match above patterns
  return ExtendedPOS::AdjStem;
}

}  // namespace suzume::core
