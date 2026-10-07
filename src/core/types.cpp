#include "types.h"

#include <iterator>

#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"

namespace suzume::core {

namespace {

struct PosAlias {
  std::string_view name;
  PartOfSpeech pos;
};

// Canonical short forms, long aliases, and Japanese names. PROPN maps to Noun
// (no dedicated proper-noun POS); OTHER/PHRASE map to Other.
constexpr PosAlias kPosAliases[] = {
    {"NOUN", PartOfSpeech::Noun},
    {"名詞", PartOfSpeech::Noun},
    {"PROPN", PartOfSpeech::Noun},
    {"PROPER_NOUN", PartOfSpeech::Noun},
    {"VERB", PartOfSpeech::Verb},
    {"動詞", PartOfSpeech::Verb},
    {"ADJ", PartOfSpeech::Adjective},
    {"ADJECTIVE", PartOfSpeech::Adjective},
    {"形容詞", PartOfSpeech::Adjective},
    {"ADV", PartOfSpeech::Adverb},
    {"ADVERB", PartOfSpeech::Adverb},
    {"副詞", PartOfSpeech::Adverb},
    {"PARTICLE", PartOfSpeech::Particle},
    {"助詞", PartOfSpeech::Particle},
    {"AUX", PartOfSpeech::Auxiliary},
    {"AUXILIARY", PartOfSpeech::Auxiliary},
    {"助動詞", PartOfSpeech::Auxiliary},
    {"CONJ", PartOfSpeech::Conjunction},
    {"CONJUNCTION", PartOfSpeech::Conjunction},
    {"接続詞", PartOfSpeech::Conjunction},
    {"DET", PartOfSpeech::Determiner},
    {"DETERMINER", PartOfSpeech::Determiner},
    {"ADNOMINAL", PartOfSpeech::Determiner},
    {"連体詞", PartOfSpeech::Determiner},
    {"PRON", PartOfSpeech::Pronoun},
    {"PRONOUN", PartOfSpeech::Pronoun},
    {"代名詞", PartOfSpeech::Pronoun},
    {"PREFIX", PartOfSpeech::Prefix},
    {"接頭辞", PartOfSpeech::Prefix},
    {"SUFFIX", PartOfSpeech::Suffix},
    {"接尾辞", PartOfSpeech::Suffix},
    {"INTJ", PartOfSpeech::Interjection},
    {"INTERJECTION", PartOfSpeech::Interjection},
    {"感動詞", PartOfSpeech::Interjection},
    {"SYMBOL", PartOfSpeech::Symbol},
    {"SYM", PartOfSpeech::Symbol},
    {"記号", PartOfSpeech::Symbol},
    {"OTHER", PartOfSpeech::Other},
    {"PHRASE", PartOfSpeech::Other},
    {"その他", PartOfSpeech::Other},
};

}  // namespace

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
  for (const auto& alias : kPosAliases) {
    if (str == alias.name) {
      return alias.pos;
    }
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
  static constexpr const char* kNames[] = {
      "unknown",
      "dict",
      "verb_kanji",
      "verb_hira",
      "verb_hira_passive_renyo",
      "verb_hira_negative_renyo",
      "verb_hira_inflected_renyo",
      "verb_kata",
      "verb_compound",
      "adj_i",
      "adj_i_hira",
      "adj_na",
      "noun_nominalized",
      "suffix",
      "same_type",
      "alphanum",
      "onomatopoeia",
      "char_speech",
      "split",
      "join",
      "kanji_hira_compound",
      "kanji_hira_nominal_compound",
      "selected_nominal_head",
      "bracketed_noun",
      "counter",
      "prefix_compound",
  };
  static_assert(std::size(kNames) == static_cast<size_t>(CandidateOrigin::PrefixCompound) + 1,
                "kNames must cover every CandidateOrigin value");
  const auto idx = static_cast<size_t>(origin);
  return idx < std::size(kNames) ? kNames[idx] : "unknown";
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
    case ExtendedPOS::AuxClassicalConjectureTerminal:
      return "AUX_現在推量";

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
    case ExtendedPOS::SuffixTrace:
      return "SUFFIX_気配";
    case ExtendedPOS::DeterminerQuotative:
      return "DET_引用";

    // Auxiliaries - Aspect
    case ExtendedPOS::AuxAspectIru:
      return "AUX_継続";
    case ExtendedPOS::AuxPejorativeYagaru:
      return "AUX_卑罵";
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
    case ExtendedPOS::AuxConjectureRashiiStem:
      return "AUX_推定語幹";
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
    case ExtendedPOS::ParticleChoice:
      return "PART_選択";
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
    case ExtendedPOS::NounForeign:
      return "NOUN_非日本語";
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
    case ExtendedPOS::AdverbInterrogative:
      return "ADV_疑問";
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
    case ExtendedPOS::InterjectionGreeting:
      return "INTJ_挨拶";
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
    case ExtendedPOS::AdverbInterrogative:
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
    case ExtendedPOS::SuffixTrace:
      return PartOfSpeech::Suffix;
    case ExtendedPOS::Symbol:
      return PartOfSpeech::Symbol;
    case ExtendedPOS::Interjection:
    case ExtendedPOS::InterjectionGreeting:
      return PartOfSpeech::Interjection;
    case ExtendedPOS::Other:
    case ExtendedPOS::Unknown:
    case ExtendedPOS::Count_:
    default:
      return PartOfSpeech::Other;
  }
}

ExtendedPOS posToExtendedPos(PartOfSpeech pos) {
  // Indexed by PartOfSpeech; defaults pick the most common form per POS.
  static constexpr ExtendedPOS kDefaults[] = {
      ExtendedPOS::Other,           // Unknown
      ExtendedPOS::Noun,            // Noun
      ExtendedPOS::VerbShuushikei,  // Verb: dictionary form
      ExtendedPOS::AdjBasic,        // Adjective: basic form
      ExtendedPOS::Adverb,          // Adverb
      ExtendedPOS::ParticleCase,    // Particle: case particle
      ExtendedPOS::AuxTenseTa,      // Auxiliary: た (most common)
      ExtendedPOS::Conjunction,     // Conjunction
      ExtendedPOS::Determiner,      // Determiner
      ExtendedPOS::Pronoun,         // Pronoun
      ExtendedPOS::Prefix,          // Prefix
      ExtendedPOS::Suffix,          // Suffix
      ExtendedPOS::Interjection,    // Interjection
      ExtendedPOS::Symbol,          // Symbol
      ExtendedPOS::Other,           // Other
  };
  static_assert(std::size(kDefaults) == static_cast<size_t>(PartOfSpeech::Count_),
                "kDefaults must cover every PartOfSpeech value");
  const auto idx = static_cast<size_t>(pos);
  return idx < std::size(kDefaults) ? kDefaults[idx] : ExtendedPOS::Other;
}

// =============================================================================
// Verb Form Detection Helpers
// =============================================================================

namespace {

struct SuffixForm {
  std::string_view suffix;
  ExtendedPOS form;
};

// Verb suffix-chain endings, ordered: たら/だら before た/だ so the longer ending wins.
constexpr SuffixForm kVerbSuffixForms[] = {
    {"たら", ExtendedPOS::VerbTaraForm}, {"だら", ExtendedPOS::VerbTaraForm}, {"た", ExtendedPOS::VerbTaForm},
    {"だ", ExtendedPOS::VerbTaForm},     {"て", ExtendedPOS::VerbTeForm},     {"で", ExtendedPOS::VerbTeForm},
    {"ば", ExtendedPOS::VerbKateikei},   {"ます", ExtendedPOS::VerbRenyokei}, {"まし", ExtendedPOS::VerbRenyokei},
    {"ませ", ExtendedPOS::VerbRenyokei}, {"ない", ExtendedPOS::VerbMizenkei}, {"なかっ", ExtendedPOS::VerbMizenkei},
    {"れる", ExtendedPOS::VerbMizenkei}, {"られ", ExtendedPOS::VerbMizenkei}, {"せる", ExtendedPOS::VerbMizenkei},
    {"させ", ExtendedPOS::VerbMizenkei},
};

// i-adjective endings, ordered: かっ/けれ/かろ before the bare く/い.
constexpr SuffixForm kAdjSuffixForms[] = {
    {"かっ", ExtendedPOS::AdjKatt},     {"けれ", ExtendedPOS::AdjKeForm}, {"きゃ", ExtendedPOS::AdjKeForm},
    {"かろ", ExtendedPOS::AdjMizenkei}, {"く", ExtendedPOS::AdjRenyokei}, {"い", ExtendedPOS::AdjBasic},
};

}  // namespace

// This detector assigns ExtendedPOS while candidates are built. Postprocessing
// treats that selected ExtendedPOS as authoritative when exposing ConjForm;
// its surface heuristics are only a fallback for legacy morphemes without one.
ExtendedPOS detectVerbForm(std::string_view surface, std::string_view suffix, bool godan_imperative_hint,
                           bool godan_i_onbin_hint) {
  // Empty surface defaults to shuushi
  if (surface.empty()) {
    return ExtendedPOS::VerbShuushikei;
  }

  // Suffix chain first, for more accurate form detection (first match wins).
  if (!suffix.empty()) {
    for (const auto& rule : kVerbSuffixForms) {
      if (utf8::endsWith(suffix, rule.suffix)) {
        return rule.form;
      }
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
  if (utf8::endsWith(surface, "い")) {
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
      kana::isGodanERowCodepoint(utf8::decodeLastChar(surface))) {
    return ExtendedPOS::VerbMeireikei;
  }

  // て/で form
  if (utf8::endsWithAny(surface, {"て", "で"})) {
    return ExtendedPOS::VerbTeForm;
  }

  // ば form (conditional)
  if (utf8::endsWith(surface, "ば")) {
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
  if (kana::isGodanTerminalCodepoint(utf8::decodeLastChar(surface))) {
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

  for (const auto& rule : kAdjSuffixForms) {
    if (utf8::endsWith(surface, rule.suffix)) {
      return rule.form;
    }
  }

  // Stem forms (for ガル接続): 美し, 高
  // These are identified by not ending in い/く/かっ/けれ
  // But we can't reliably detect this without knowing the full word
  // Default to stem for short forms that don't match above patterns
  return ExtendedPOS::AdjStem;
}

}  // namespace suzume::core
