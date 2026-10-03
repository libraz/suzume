#include <string_view>

#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "postprocess/postprocessor_resolvers_internal.h"

namespace suzume::postprocess::resolver {

void retag(core::Morpheme& morpheme, core::PartOfSpeech pos, core::ExtendedPOS extended_pos, std::string_view lemma,
           dictionary::ConjugationType conj_type, grammar::ConjForm conj_form) {
  morpheme.pos = pos;
  morpheme.extended_pos = extended_pos;
  morpheme.lemma = lemma;
  morpheme.conj_type = conj_type;
  morpheme.conj_form = conj_form;
}

void retagUninflected(core::Morpheme& morpheme, core::PartOfSpeech pos, core::ExtendedPOS extended_pos,
                      std::string_view lemma) {
  retag(morpheme, pos, extended_pos, lemma, dictionary::ConjugationType::None, grammar::ConjForm::Base);
}

void retagAppearanceSou(core::Morpheme& sou) {
  retagUninflected(sou, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxAppearanceSou, "そう");
}

void retagNaAdjectivalSou(core::Morpheme& sou) {
  retagUninflected(sou, core::PartOfSpeech::Adjective, core::ExtendedPOS::AdjNaAdj, "そう");
}

void retagAdverbialSou(core::Morpheme& sou) {
  retagUninflected(sou, core::PartOfSpeech::Adverb, core::ExtendedPOS::Adverb, "そう");
}

void retagCopulaDa(core::Morpheme& copula) {
  retagUninflected(copula, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxCopulaDa, "だ");
}

void retagNegativeNai(core::Morpheme& negative) {
  retagUninflected(negative, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxNegativeNai, "ない");
}

void retagBasicNegativeAdjective(core::Morpheme& negative) {
  retag(negative, core::PartOfSpeech::Adjective, core::ExtendedPOS::AdjBasic, "ない",
        dictionary::ConjugationType::IAdjective, grammar::ConjForm::Base);
}

void retagNegativeAdjectiveCell(core::Morpheme& negative) {
  const core::ExtendedPOS cell = core::detectAdjForm(negative.surface, /*is_na_adj=*/false);
  grammar::ConjForm conj_form = grammar::ConjForm::Base;
  switch (cell) {
    case core::ExtendedPOS::AdjKatt:
    case core::ExtendedPOS::AdjRenyokei:
      conj_form = grammar::ConjForm::Renyokei;
      break;
    case core::ExtendedPOS::AdjKeForm:
      conj_form = grammar::ConjForm::Kateikei;
      break;
    case core::ExtendedPOS::AdjMizenkei:
      conj_form = grammar::ConjForm::Mizenkei;
      break;
    default:
      break;
  }
  retag(negative, core::PartOfSpeech::Adjective, cell, "ない", dictionary::ConjugationType::IAdjective, conj_form);
}

void retagNounSurface(core::Morpheme& morpheme) {
  retagUninflected(morpheme, core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, morpheme.surface);
}

void retagNaAdjectiveSurface(core::Morpheme& morpheme) {
  retag(morpheme, core::PartOfSpeech::Adjective, core::ExtendedPOS::AdjNaAdj, morpheme.surface,
        dictionary::ConjugationType::NaAdjective, grammar::ConjForm::Base);
}

void mergeInto(core::Morpheme& head, const core::Morpheme& tail) {
  head.surface += tail.surface;
  head.end = tail.end;
  head.flags = core::withoutFlag(head.flags, core::EdgeFlags::FromDictionary);
  head.flags = core::withoutFlag(head.flags, core::EdgeFlags::FromUserDict);
}

bool followsTeFormConnective(const core::Morpheme& morpheme) {
  return morpheme.extended_pos == core::ExtendedPOS::ParticleConj && grammar::isTeDeSurface(morpheme.surface);
}

bool isVerbalPredicateBeforeSou(const core::Morpheme& morpheme) {
  return morpheme.extended_pos == core::ExtendedPOS::VerbRenyokei ||
         morpheme.extended_pos == core::ExtendedPOS::VerbShuushikei ||
         morpheme.extended_pos == core::ExtendedPOS::AdjStem ||
         morpheme.extended_pos == core::ExtendedPOS::AuxAspectShimau ||
         morpheme.extended_pos == core::ExtendedPOS::AuxAspectIru;
}

// Recover a Godan dictionary form from an i-row continuative stem.  Some
// callers intentionally preserve the lattice-provided conjugation form while
// others require an explicit renyokei form, so that policy stays at the call
// site.
bool retagGodanRenyokeiFromIRow(core::Morpheme& stem, bool set_conj_form) {
  const char32_t stem_last = utf8::decodeLastChar(stem.surface);
  if (!grammar::isIRowCodepoint(stem_last)) {
    return false;
  }
  const std::string_view base_suffix = grammar::godanBaseSuffixFromIRow(stem_last);
  const grammar::VerbType verb_type = grammar::verbTypeFromIRowCodepoint(stem_last);
  if (base_suffix.empty() || verb_type == grammar::VerbType::Unknown) {
    return false;
  }
  stem.pos = core::PartOfSpeech::Verb;
  stem.extended_pos = core::ExtendedPOS::VerbRenyokei;
  stem.lemma = normalize::concat(utf8::dropLastChar(stem.surface), base_suffix);
  stem.conj_type = grammar::verbTypeToConjType(verb_type);
  if (set_conj_form) {
    stem.conj_form = grammar::ConjForm::Renyokei;
  }
  return true;
}

// Check if a surface is a counter/duration quantity that a temporal 後 attaches to
// as a suffix (2時間, 10日, 5分, 数日, 半年): first codepoint is a numeral or inexact
// quantity prefix (数/半/何) and the last is a counter kanji. MeCab tags 後 after such
// a quantity as 名詞,接尾 (Suffix), whereas 後 after an ordinary noun (食事の後) stays a
// plain noun.
bool isCounterDurationNoun(const std::string& surface) {
  if (surface.empty()) {
    return false;
  }
  size_t pos = 0;
  const char32_t first = normalize::decodeUtf8(surface, pos);
  if (!normalize::isNumeralCodepoint(first) && !normalize::isQuantityPrefixKanji(first)) {
    return false;
  }
  char32_t last = first;
  while (pos < surface.size()) {
    last = normalize::decodeUtf8(surface, pos);
  }
  return normalize::isCounterKanji(last);
}

// The negative auxiliary ない selects a verbal irrealis: a verb, a
// verb-conjugating auxiliary (させ+ない, い+ない) or a te-form whose いる has
// been dropped (見て+ない). Anywhere else ない is the (supplementary) adjective:
// after a particle (時間が, 本では, 水しか), the copula (本じゃ), an adjective
// continuative (高く) or an adjective-type auxiliary (食べたく).
void resolveNegativeHost(std::vector<core::Morpheme>& result) {
  for (size_t idx = 0; idx < result.size(); ++idx) {
    auto& negative = result[idx];
    if (negative.lemma != "ない" ||
        (negative.pos != core::PartOfSpeech::Auxiliary && negative.pos != core::PartOfSpeech::Adjective)) {
      continue;
    }
    const core::Morpheme* host = idx > 0 ? &result[idx - 1] : nullptr;
    const bool verbal_host =
        host != nullptr &&
        (host->pos == core::PartOfSpeech::Verb ||
         (host->pos == core::PartOfSpeech::Auxiliary &&
          !utf8::equalsAny(host->lemma, {"だ", "です", "たい", "ない", "らしい", "ます", "た", "う", "よう", "まい",
                                         "ぬ", "ん", "ず", "べし", "そう", "みたい"})) ||
         (host->pos == core::PartOfSpeech::Particle && utf8::equalsAny(host->surface, {"て", "で"})));
    if (verbal_host && negative.pos == core::PartOfSpeech::Adjective) {
      retagNegativeNai(negative);
    } else if (!verbal_host && negative.pos == core::PartOfSpeech::Auxiliary) {
      retagNegativeAdjectiveCell(negative);
    }
  }
}

}  // namespace suzume::postprocess::resolver
