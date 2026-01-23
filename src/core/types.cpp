#include "types.h"

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
    case PartOfSpeech::Symbol:
      return "SYMBOL";
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
    case PartOfSpeech::Symbol:
      return "記号";
    case PartOfSpeech::Other:
    case PartOfSpeech::Unknown:
    default:
      return "その他";
  }
}

PartOfSpeech stringToPos(std::string_view str) {
  // English names
  if (str == "NOUN" || str == "名詞") {
    return PartOfSpeech::Noun;
  }
  if (str == "VERB" || str == "動詞") {
    return PartOfSpeech::Verb;
  }
  if (str == "ADJ" || str == "形容詞") {
    return PartOfSpeech::Adjective;
  }
  if (str == "ADV" || str == "副詞") {
    return PartOfSpeech::Adverb;
  }
  if (str == "PARTICLE" || str == "助詞") {
    return PartOfSpeech::Particle;
  }
  if (str == "AUX" || str == "助動詞") {
    return PartOfSpeech::Auxiliary;
  }
  if (str == "CONJ" || str == "接続詞") {
    return PartOfSpeech::Conjunction;
  }
  if (str == "DET" || str == "連体詞") {
    return PartOfSpeech::Determiner;
  }
  if (str == "PRON" || str == "代名詞") {
    return PartOfSpeech::Pronoun;
  }
  if (str == "PREFIX" || str == "接頭辞") {
    return PartOfSpeech::Prefix;
  }
  if (str == "SUFFIX" || str == "接尾辞") {
    return PartOfSpeech::Suffix;
  }
  if (str == "SYMBOL" || str == "記号") {
    return PartOfSpeech::Symbol;
  }
  return PartOfSpeech::Other;
}

bool isTaggable(PartOfSpeech pos) {
  return pos == PartOfSpeech::Noun || pos == PartOfSpeech::Verb || pos == PartOfSpeech::Adjective ||
         pos == PartOfSpeech::Adverb;
}

bool isContentWord(PartOfSpeech pos) {
  return pos == PartOfSpeech::Noun || pos == PartOfSpeech::Verb || pos == PartOfSpeech::Adjective ||
         pos == PartOfSpeech::Adverb;
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
    case ExtendedPOS::VerbShuushikei: return "VERB_終止";
    case ExtendedPOS::VerbRenyokei: return "VERB_連用";
    case ExtendedPOS::VerbMizenkei: return "VERB_未然";
    case ExtendedPOS::VerbOnbinkei: return "VERB_音便";
    case ExtendedPOS::VerbTeForm: return "VERB_て形";
    case ExtendedPOS::VerbKateikei: return "VERB_仮定";
    case ExtendedPOS::VerbMeireikei: return "VERB_命令";
    case ExtendedPOS::VerbRentaikei: return "VERB_連体";
    case ExtendedPOS::VerbTaForm: return "VERB_た形";
    case ExtendedPOS::VerbTaraForm: return "VERB_たら形";

    // Adjective forms
    case ExtendedPOS::AdjBasic: return "ADJ_終止";
    case ExtendedPOS::AdjRenyokei: return "ADJ_連用";
    case ExtendedPOS::AdjStem: return "ADJ_語幹";
    case ExtendedPOS::AdjKatt: return "ADJ_かっ";
    case ExtendedPOS::AdjKeForm: return "ADJ_け形";
    case ExtendedPOS::AdjNaAdj: return "ADJ_NA";

    // Auxiliaries - Tense
    case ExtendedPOS::AuxTenseTa: return "AUX_過去";
    case ExtendedPOS::AuxTenseMasu: return "AUX_丁寧";

    // Auxiliaries - Negation
    case ExtendedPOS::AuxNegativeNai: return "AUX_否定";
    case ExtendedPOS::AuxNegativeNu: return "AUX_否定古";

    // Auxiliaries - Desire/Volition
    case ExtendedPOS::AuxDesireTai: return "AUX_願望";
    case ExtendedPOS::AuxVolitional: return "AUX_意志";

    // Auxiliaries - Voice
    case ExtendedPOS::AuxPassive: return "AUX_受身";
    case ExtendedPOS::AuxCausative: return "AUX_使役";
    case ExtendedPOS::AuxPotential: return "AUX_可能";

    // Auxiliaries - Aspect
    case ExtendedPOS::AuxAspectIru: return "AUX_継続";
    case ExtendedPOS::AuxAspectShimau: return "AUX_完了";
    case ExtendedPOS::AuxAspectOku: return "AUX_準備";
    case ExtendedPOS::AuxAspectMiru: return "AUX_試行";
    case ExtendedPOS::AuxAspectIku: return "AUX_進行";
    case ExtendedPOS::AuxAspectKuru: return "AUX_接近";

    // Auxiliaries - Appearance/Conjecture
    case ExtendedPOS::AuxAppearanceSou: return "AUX_様態";
    case ExtendedPOS::AuxConjectureRashii: return "AUX_推定";
    case ExtendedPOS::AuxConjectureMitai: return "AUX_みたい";

    // Auxiliaries - Copula
    case ExtendedPOS::AuxCopulaDa: return "AUX_断定";
    case ExtendedPOS::AuxCopulaDesu: return "AUX_丁寧断定";

    // Auxiliaries - Other
    case ExtendedPOS::AuxHonorific: return "AUX_尊敬";
    case ExtendedPOS::AuxGozaru: return "AUX_丁重";

    // Particles
    case ExtendedPOS::ParticleCase: return "PART_格";
    case ExtendedPOS::ParticleTopic: return "PART_係";
    case ExtendedPOS::ParticleFinal: return "PART_終";
    case ExtendedPOS::ParticleConj: return "PART_接続";
    case ExtendedPOS::ParticleQuote: return "PART_引用";
    case ExtendedPOS::ParticleAdverbial: return "PART_副";
    case ExtendedPOS::ParticleNo: return "PART_準体";
    case ExtendedPOS::ParticleBinding: return "PART_係結";

    // Nouns
    case ExtendedPOS::Noun: return "NOUN";
    case ExtendedPOS::NounFormal: return "NOUN_形式";
    case ExtendedPOS::NounVerbal: return "NOUN_転成";
    case ExtendedPOS::NounProper: return "NOUN_固有";
    case ExtendedPOS::NounNumber: return "NOUN_数";

    // Pronouns
    case ExtendedPOS::Pronoun: return "PRON";
    case ExtendedPOS::PronounInterrogative: return "PRON_疑問";

    // Others
    case ExtendedPOS::Adverb: return "ADV";
    case ExtendedPOS::AdverbQuotative: return "ADV_引用";
    case ExtendedPOS::Conjunction: return "CONJ";
    case ExtendedPOS::Determiner: return "DET";
    case ExtendedPOS::Prefix: return "PREFIX";
    case ExtendedPOS::Suffix: return "SUFFIX";
    case ExtendedPOS::Symbol: return "SYMBOL";
    case ExtendedPOS::Interjection: return "INTJ";
    case ExtendedPOS::Other: return "OTHER";

    case ExtendedPOS::Unknown:
    case ExtendedPOS::Count_:
    default:
      return "UNKNOWN";
  }
}

PartOfSpeech extendedPosToPos(ExtendedPOS epos) {
  // Verb forms -> Verb
  if (epos >= ExtendedPOS::VerbShuushikei && epos <= ExtendedPOS::VerbTaraForm) {
    return PartOfSpeech::Verb;
  }
  // Adjective forms -> Adjective
  if (epos >= ExtendedPOS::AdjBasic && epos <= ExtendedPOS::AdjNaAdj) {
    return PartOfSpeech::Adjective;
  }
  // Auxiliary types -> Auxiliary
  if (epos >= ExtendedPOS::AuxTenseTa && epos <= ExtendedPOS::AuxGozaru) {
    return PartOfSpeech::Auxiliary;
  }
  // Particle types -> Particle
  if (epos >= ExtendedPOS::ParticleCase && epos <= ExtendedPOS::ParticleBinding) {
    return PartOfSpeech::Particle;
  }
  // Noun types -> Noun
  if (epos >= ExtendedPOS::Noun && epos <= ExtendedPOS::NounNumber) {
    return PartOfSpeech::Noun;
  }
  // Pronoun types -> Pronoun
  if (epos >= ExtendedPOS::Pronoun && epos <= ExtendedPOS::PronounInterrogative) {
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
      return PartOfSpeech::Determiner;
    case ExtendedPOS::Prefix:
      return PartOfSpeech::Prefix;
    case ExtendedPOS::Suffix:
      return PartOfSpeech::Suffix;
    case ExtendedPOS::Symbol:
      return PartOfSpeech::Symbol;
    case ExtendedPOS::Interjection:
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
    case PartOfSpeech::Symbol:
      return ExtendedPOS::Symbol;
    case PartOfSpeech::Other:
    case PartOfSpeech::Unknown:
    default:
      return ExtendedPOS::Other;
  }
}

// =============================================================================
// Verb Form Detection Helpers
// =============================================================================

namespace {

// Helper to check if string ends with any of the given patterns
bool endsWithAny(std::string_view s, std::initializer_list<const char*> patterns) {
  for (const char* p : patterns) {
    size_t plen = std::string_view(p).size();
    if (s.size() >= plen && s.substr(s.size() - plen) == p) {
      return true;
    }
  }
  return false;
}

}  // namespace

ExtendedPOS detectVerbForm(std::string_view surface, std::string_view suffix) {
  // Empty surface defaults to shuushi
  if (surface.empty()) {
    return ExtendedPOS::VerbShuushikei;
  }

  // Check suffix chain first for more accurate form detection
  if (!suffix.empty()) {
    // たら/だら forms (conditional past)
    if (endsWithAny(suffix, {"たら", "だら"})) {
      return ExtendedPOS::VerbTaraForm;
    }
    // た/だ forms (past) - but not た that follows っ/ん (onbin)
    if (endsWithAny(suffix, {"た", "だ"})) {
      // Check if た/だ is preceded by っ/ん in the full suffix
      if (suffix.size() > 3) {  // More than just た/だ
        std::string_view pre = suffix.substr(0, suffix.size() - 3);
        if (endsWithAny(pre, {"っ", "ん", "い"})) {
          return ExtendedPOS::VerbTaForm;  // 書いた, 読んだ
        }
      }
      return ExtendedPOS::VerbTaForm;
    }
    // て/で forms
    if (endsWithAny(suffix, {"て", "で"})) {
      return ExtendedPOS::VerbTeForm;
    }
    // ば forms (conditional)
    if (endsWithAny(suffix, {"ば"})) {
      return ExtendedPOS::VerbKateikei;
    }
    // ます forms indicate renyokei connection
    if (endsWithAny(suffix, {"ます", "まし", "ませ"})) {
      return ExtendedPOS::VerbRenyokei;
    }
    // ない/なかっ forms indicate mizenkei connection (for godan) or renyokei (ichidan)
    if (endsWithAny(suffix, {"ない", "なかっ"})) {
      return ExtendedPOS::VerbMizenkei;
    }
    // れる/られる forms indicate mizenkei connection
    if (endsWithAny(suffix, {"れる", "られ", "せる", "させ"})) {
      return ExtendedPOS::VerbMizenkei;
    }
  }

  // Check surface endings for forms without explicit suffix chain
  // Onbin forms (っ, ん, い at end - before た/て)
  if (endsWithAny(surface, {"っ", "ん"})) {
    return ExtendedPOS::VerbOnbinkei;
  }
  // Also check for い-onbin (書い from 書く)
  // Need to distinguish from renyokei ending in い
  // い-onbin is specifically kanji + い (書い, 泳い) for godan verbs
  // All-hiragana surfaces ending in い are ichidan renyokei (食べ, につい, etc.)
  if (surface.size() >= 3 && endsWithAny(surface, {"い"})) {
    // Check if surface contains kanji - only then classify as onbinkei
    bool has_kanji = false;
    for (char32_t cp : suzume::normalize::utf8::decode(surface)) {
      if (suzume::normalize::isKanjiCodepoint(cp)) {
        has_kanji = true;
        break;
      }
    }
    if (has_kanji) {
      return ExtendedPOS::VerbOnbinkei;
    }
    // All-hiragana い-ending verbs are renyokei (ichidan stems)
    return ExtendedPOS::VerbRenyokei;
  }

  // て/で form
  if (endsWithAny(surface, {"て", "で"})) {
    return ExtendedPOS::VerbTeForm;
  }

  // ば form (conditional)
  if (endsWithAny(surface, {"ば"})) {
    return ExtendedPOS::VerbKateikei;
  }

  // た/だ form (past)
  if (endsWithAny(surface, {"た", "だ"})) {
    return ExtendedPOS::VerbTaForm;
  }

  // たら/だら form (conditional past)
  if (endsWithAny(surface, {"たら", "だら"})) {
    return ExtendedPOS::VerbTaraForm;
  }

  // 命令形 checks - ろ/れ/え for various verb types
  if (endsWithAny(surface, {"ろ", "よ"})) {
    // Ichidan imperative: 食べろ, 見ろ
    return ExtendedPOS::VerbMeireikei;
  }

  // る ending - likely shuushi (dictionary form) for ichidan
  if (endsWithAny(surface, {"る"})) {
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
  if (endsWithAny(surface, {"かっ"})) {
    return ExtendedPOS::AdjKatt;
  }

  // けれ form (conditional stem): 美しけれ, 高けれ
  if (endsWithAny(surface, {"けれ", "きゃ"})) {
    return ExtendedPOS::AdjKeForm;
  }

  // く form (adverbial/renyokei): 美しく, 高く
  if (endsWithAny(surface, {"く"})) {
    return ExtendedPOS::AdjRenyokei;
  }

  // い form (basic/shuushi): 美しい, 高い
  if (endsWithAny(surface, {"い"})) {
    return ExtendedPOS::AdjBasic;
  }

  // Stem forms (for ガル接続): 美し, 高
  // These are identified by not ending in い/く/かっ/けれ
  // But we can't reliably detect this without knowing the full word
  // Default to stem for short forms that don't match above patterns
  return ExtendedPOS::AdjStem;
}

}  // namespace suzume::core
