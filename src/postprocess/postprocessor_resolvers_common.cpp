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

// そう after a predicate is the auxiliary そうだ, appearance after a stem
// (降り+そう, おいし+そう, 食べ+な+さ+そう) and hearsay after a terminal form
// (降る+そう, 教師+だ+そう). A noun hosts it only as a na-adjective stem
// (不安+そう) unless a verb follows (明日+そう+する). With no predicate in front
// it is the demonstrative adverb (そう+だ, まさに+そう+だ).
void resolveSouHost(std::vector<core::Morpheme>& result) {
  for (size_t idx = 0; idx < result.size(); ++idx) {
    auto& sou = result[idx];
    if (sou.lemma != "そう" || (sou.pos != core::PartOfSpeech::Adjective && sou.pos != core::PartOfSpeech::Auxiliary &&
                                sou.pos != core::PartOfSpeech::Adverb)) {
      continue;
    }
    const core::Morpheme* host = idx > 0 ? &result[idx - 1] : nullptr;
    const core::Morpheme* following = idx + 1 < result.size() ? &result[idx + 1] : nullptr;
    const bool nominal_stem_host = host != nullptr && host->pos == core::PartOfSpeech::Noun &&
                                   (following == nullptr || following->pos != core::PartOfSpeech::Verb);
    const bool predicate_host =
        host != nullptr && (host->pos == core::PartOfSpeech::Verb || host->pos == core::PartOfSpeech::Adjective ||
                            host->pos == core::PartOfSpeech::Auxiliary ||
                            (host->pos == core::PartOfSpeech::Suffix && host->lemma == "さ") || nominal_stem_host);
    if (predicate_host && sou.pos != core::PartOfSpeech::Auxiliary) {
      retagAppearanceSou(sou);
    } else if (!predicate_host && sou.pos != core::PartOfSpeech::Adverb) {
      retagAdverbialSou(sou);
    }
  }
}

// The written-style copula is the continuative で plus the auxiliary verb ある,
// analyzed the same in every cell whatever the tense (事実+で+ある, 事実+で+
// あっ+た, 本+で+あろ+う). After an onbin stem the で is the te-form instead
// (読ん+で+ある), while a terminal verb takes the copula like any predicate.
void resolveCopulaAru(std::vector<core::Morpheme>& result) {
  for (size_t idx = 0; idx + 1 < result.size(); ++idx) {
    auto& copula = result[idx];
    auto& aru = result[idx + 1];
    if (!utf8::equalsAny(copula.surface, {"で"}) || aru.getLemma() != "ある" ||
        (aru.pos != core::PartOfSpeech::Verb && aru.pos != core::PartOfSpeech::Auxiliary)) {
      continue;
    }
    if (idx > 0 && result[idx - 1].pos == core::PartOfSpeech::Verb &&
        result[idx - 1].conj_form != grammar::ConjForm::Base) {
      continue;
    }
    if (copula.extended_pos != core::ExtendedPOS::AuxCopulaDa) {
      retagCopulaDa(copula);
    }
    if (aru.pos == core::PartOfSpeech::Verb) {
      continue;
    }
    const char32_t cell = utf8::decodeLastChar(aru.surface);
    core::ExtendedPOS extended_pos = core::ExtendedPOS::VerbShuushikei;
    grammar::ConjForm conj_form = grammar::ConjForm::Base;
    switch (cell) {
      case U'っ':
        extended_pos = core::ExtendedPOS::VerbOnbinkei;
        conj_form = grammar::ConjForm::Onbinkei;
        break;
      case U'り':
        extended_pos = core::ExtendedPOS::VerbRenyokei;
        conj_form = grammar::ConjForm::Renyokei;
        break;
      case U'ろ':
      case U'ら':
        extended_pos = core::ExtendedPOS::VerbMizenkei;
        conj_form = grammar::ConjForm::Mizenkei;
        break;
      case U'れ':
        extended_pos = core::ExtendedPOS::VerbKateikei;
        conj_form = grammar::ConjForm::Kateikei;
        break;
      default:
        break;
    }
    retag(aru, core::PartOfSpeech::Verb, extended_pos, "ある", dictionary::ConjugationType::GodanRa, conj_form);
  }
}

// Before the nominalizer の the る of てる contracts to ん (食べ+てん+の =
// 食べてるの); a nominalizer ん directly before the nominalizer の is no reading.
void mergeContractedTeruBeforeNominalizer(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx + 2 < result.size(); ++idx) {
    auto& te = result[idx];
    const auto& contracted = result[idx + 1];
    const auto& nominalizer = result[idx + 2];
    if (result[idx - 1].pos != core::PartOfSpeech::Verb || !utf8::equalsAny(te.surface, {"て", "で"}) ||
        !utf8::equalsAny(contracted.surface, {"ん"}) || nominalizer.getLemma() != "の" || te.end != contracted.start) {
      continue;
    }
    const std::string headword = te.surface + "る";
    mergeInto(te, contracted);
    retagUninflected(te, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxAspectIru, headword);
    result.erase(result.begin() + static_cast<std::ptrdiff_t>(idx + 1));
  }
}

// The classical perfect たり takes a plain continuative (咲き+たり); an onbin
// stem only takes the listing particle (行っ+たり, だっ+たり, 読ん+だり), and so
// does the second member of a pair already opened in the sentence.
void resolveListingTari(std::vector<core::Morpheme>& result) {
  bool listing_opened = false;
  for (size_t idx = 0; idx < result.size(); ++idx) {
    auto& tari = result[idx];
    if (!utf8::equalsAny(tari.surface, {"たり", "だり"})) {
      continue;
    }
    const bool onbin_host =
        idx > 0 && (utf8::endsWith(result[idx - 1].surface, "っ") || utf8::endsWith(result[idx - 1].surface, "ん") ||
                    utf8::endsWith(result[idx - 1].surface, "い"));
    if ((onbin_host || listing_opened) && tari.pos != core::PartOfSpeech::Particle) {
      retagUninflected(tari, core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleConj, tari.surface);
    }
    listing_opened = true;
  }
}

// The excessive すぎ/過ぎ after a verb continuative heads a noun phrase when a
// nominal-selecting element follows (使い+過ぎ+に+注意, 読み+すぎ+を+防ぐ,
// 食べ+過ぎ+だ): a continuative cannot take を/が/の, a non-motion に, or the
// copula, so it is the deverbal noun, as a plain continuative is (読み+に).
void resolveExcessiveDeverbalNoun(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx + 1 < result.size(); ++idx) {
    auto& excess = result[idx];
    if (excess.pos != core::PartOfSpeech::Verb || !utf8::equalsAny(excess.surface, {"すぎ", "過ぎ"}) ||
        result[idx - 1].pos != core::PartOfSpeech::Verb) {
      continue;
    }
    const auto& following = result[idx + 1];
    const auto* after = idx + 2 < result.size() ? &result[idx + 2] : nullptr;
    const bool case_particle =
        following.pos == core::PartOfSpeech::Particle && utf8::equalsAny(following.surface, {"を", "が", "の"});
    const bool non_motion_ni =
        following.pos == core::PartOfSpeech::Particle && utf8::equalsAny(following.surface, {"に"}) &&
        (after == nullptr || !utf8::equalsAny(after->getLemma(), {"行く", "来る", "いく", "くる", "ゆく"}));
    const bool copula =
        following.pos == core::PartOfSpeech::Auxiliary && utf8::equalsAny(following.getLemma(), {"だ", "です"});
    if (case_particle || non_motion_ni || copula) {
      retagNounSurface(excess);
    }
  }
}

// The continuative たまひ of the classical honorific takes the classical past
// き (書き+たまひ+し); the conjunctive particle し follows a terminal form.
void resolveHonorificContinuativePast(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx < result.size(); ++idx) {
    const auto& honorific = result[idx - 1];
    auto& past = result[idx];
    if (honorific.getLemma() != "たまふ" || !utf8::endsWith(honorific.surface, "ひ") ||
        !utf8::equalsAny(past.surface, {"し"}) || past.pos == core::PartOfSpeech::Auxiliary) {
      continue;
    }
    retagUninflected(past, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxClassicalKi, "き");
  }
}

}  // namespace suzume::postprocess::resolver
