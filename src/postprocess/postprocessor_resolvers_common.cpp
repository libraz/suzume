#include <iterator>
#include <string_view>
#include <utility>

#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "postprocess/postprocessor_resolvers_internal.h"

namespace suzume::postprocess::resolver {

namespace {

bool isMotionVerbLemma(std::string_view lemma) {
  return utf8::equalsAny(lemma, {"行く", "来る", "いく", "くる", "ゆく"});
}

// Expanded demonstrative of a contracted こん/そん/あん/どん (before だけ), or empty.
std::string_view expandContractedDemonstrative(std::string_view surface) {
  static constexpr std::pair<std::string_view, std::string_view> kContracted[] = {
      {"こん", "これ"}, {"そん", "それ"}, {"あん", "あれ"}, {"どん", "どれ"}};
  for (const auto& [contracted, expanded] : kContracted) {
    if (utf8::equalsAny(surface, {contracted})) {
      return expanded;
    }
  }
  return {};
}

}  // namespace

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

void insertAfter(std::vector<core::Morpheme>& result, size_t idx, const core::Morpheme& morpheme) {
  result.insert(result.begin() + static_cast<std::ptrdiff_t>(idx + 1), morpheme);
}

void eraseAfter(std::vector<core::Morpheme>& result, size_t idx) {
  result.erase(result.begin() + static_cast<std::ptrdiff_t>(idx + 1));
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
  if (!kana::isIRowCodepoint(stem_last)) {
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
    const core::Morpheme* host = morphemeBefore(result, idx);
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
    const core::Morpheme* host = morphemeBefore(result, idx);
    const core::Morpheme* following = morphemeAfter(result, idx, 1);
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
    eraseAfter(result, idx);
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
    const auto* after = morphemeAfter(result, idx, 2);
    const bool case_particle =
        following.pos == core::PartOfSpeech::Particle && utf8::equalsAny(following.surface, {"を", "が", "の"});
    const bool non_motion_ni = following.pos == core::PartOfSpeech::Particle &&
                               utf8::equalsAny(following.surface, {"に"}) &&
                               (after == nullptr || !isMotionVerbLemma(after->getLemma()));
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

// An adverb takes no case of its own, so one before が/を/の is a nominal
// (たくさん+の+本, いつも+の+店, ゆめ+の+話).
void resolveAdverbBeforeCase(std::vector<core::Morpheme>& result) {
  for (size_t idx = 0; idx + 1 < result.size(); ++idx) {
    auto& adverb = result[idx];
    const auto& particle = result[idx + 1];
    if (adverb.pos == core::PartOfSpeech::Adverb && particle.pos == core::PartOfSpeech::Particle &&
        utf8::equalsAny(particle.surface, {"が", "を", "の"})) {
      retagNounSurface(adverb);
    }
  }
}

// Cell readings a predicate frame decides, mirroring the reference repairs:
// - た between a godan continuative and そう is the stem of たい (読み+た+そう);
//   an ichidan one is also the hearsay past (食べ+た+そう) and is left alone;
// - an ease stem between a continuative and そう is that adjective (書き+にく+そう);
// - じゃろ is the copula だ, like だろ;
// - a one-kanji continuative directly before する is a サ変 noun (得+し+た);
// - とれ after a continuative is the contracted ておる (食べ+とれ+ば);
// - a suffix cannot open a clause (がち+で).
void resolvePredicateCellLemmas(std::vector<core::Morpheme>& result) {
  for (size_t idx = 0; idx < result.size(); ++idx) {
    auto& cell = result[idx];
    const core::Morpheme* previous = morphemeBefore(result, idx);
    const core::Morpheme* following = morphemeAfter(result, idx, 1);
    const bool after_verb = previous != nullptr && previous->pos == core::PartOfSpeech::Verb;
    const bool before_sou = following != nullptr && utf8::equalsAny(following->surface, {"そう"});
    if (utf8::equalsAny(cell.surface, {"た"}) && after_verb && before_sou &&
        previous->conj_type != dictionary::ConjugationType::Ichidan &&
        previous->extended_pos == core::ExtendedPOS::VerbRenyokei) {
      cell.lemma = "たい";
    } else if (after_verb && before_sou && utf8::equalsAny(cell.surface, {"にく", "づら", "がた", "やす"}) &&
               cell.pos != core::PartOfSpeech::Adjective) {
      retag(cell, core::PartOfSpeech::Adjective, core::ExtendedPOS::AdjStem, cell.surface + "い",
            dictionary::ConjugationType::IAdjective, grammar::ConjForm::Base);
    } else if (utf8::equalsAny(cell.surface, {"じゃろ"})) {
      cell.lemma = "だ";
    } else if (cell.pos == core::PartOfSpeech::Verb && grammar::isSingleKanjiSurface(cell.surface) &&
               following != nullptr && following->getLemma() == "する" &&
               (previous == nullptr || previous->pos != core::PartOfSpeech::Prefix)) {
      retagNounSurface(cell);
    } else if (utf8::equalsAny(cell.surface, {"とれ"}) && after_verb && cell.pos == core::PartOfSpeech::Verb) {
      retagUninflected(cell, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxAspectIru, "とる");
    } else if (idx == 0 && result.size() > 1 && cell.pos == core::PartOfSpeech::Suffix) {
      retagNounSurface(cell);
    }
  }
}

namespace {

// Split a Japanese-script @p head after @p head_codepoints characters, the tail
// becoming a new morpheme after it.
core::Morpheme splitTail(core::Morpheme& head, size_t head_codepoints) {
  core::Morpheme tail = head;
  const size_t head_bytes = head_codepoints * core::kJapaneseCharBytes;
  tail.surface = head.surface.substr(head_bytes);
  head.surface = head.surface.substr(0, head_bytes);
  head.end = head.start + head_codepoints;
  tail.start = head.end;
  return tail;
}

}  // namespace

// Readings the surrounding frame rules out, mirroring the reference repairs:
// で+しか before a verb is the case particle; で+は before ござる is the copula;
// the adverb ことに cannot complement なる (こと+に+なる); より opening a clause
// is the adverb; 何だ after a word is 何+だ; 先 after a pronoun is the noun.
void resolveFrameRepairs(std::vector<core::Morpheme>& result) {
  for (size_t idx = 0; idx < result.size(); ++idx) {
    auto& token = result[idx];
    const core::Morpheme* previous = morphemeBefore(result, idx);
    const core::Morpheme* following = morphemeAfter(result, idx, 1);
    const core::Morpheme* after = morphemeAfter(result, idx, 2);
    const std::string_view expanded_demonstrative = expandContractedDemonstrative(token.surface);
    if (utf8::equalsAny(token.surface, {"で"}) && following != nullptr &&
        utf8::equalsAny(following->surface, {"しか"}) && after != nullptr && after->pos == core::PartOfSpeech::Verb) {
      retagUninflected(token, core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleCase, "で");
    } else if (utf8::equalsAny(token.surface, {"で"}) && following != nullptr &&
               utf8::equalsAny(following->surface, {"は"}) && after != nullptr && after->getLemma() == "ござる") {
      retagCopulaDa(token);
    } else if (utf8::equalsAny(token.surface, {"ことに"}) && token.pos == core::PartOfSpeech::Adverb &&
               following != nullptr && following->getLemma() == "なる") {
      core::Morpheme particle = splitTail(token, 2);
      retagUninflected(token, core::PartOfSpeech::Noun, core::ExtendedPOS::NounFormal, "こと");
      retagUninflected(particle, core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleCase, "に");
      insertAfter(result, idx, particle);
    } else if (idx == 0 && result.size() > 1 && utf8::equalsAny(token.surface, {"より"}) &&
               token.pos == core::PartOfSpeech::Particle) {
      retagUninflected(token, core::PartOfSpeech::Adverb, core::ExtendedPOS::Adverb, "より");
    } else if (idx > 0 && utf8::equalsAny(token.surface, {"何だ"}) && token.pos == core::PartOfSpeech::Interjection) {
      core::Morpheme copula = splitTail(token, 1);
      retagUninflected(token, core::PartOfSpeech::Pronoun, core::ExtendedPOS::PronounInterrogative, "何");
      retagCopulaDa(copula);
      insertAfter(result, idx, copula);
    } else if (utf8::equalsAny(token.surface, {"先"}) && token.pos == core::PartOfSpeech::Suffix &&
               previous != nullptr && previous->pos == core::PartOfSpeech::Pronoun) {
      retagNounSurface(token);
    } else if (following != nullptr && utf8::equalsAny(following->surface, {"だけ"}) &&
               !expanded_demonstrative.empty()) {
      // The contracted demonstratives これ/それ/あれ/どれ before だけ.
      retagUninflected(token, core::PartOfSpeech::Pronoun, core::ExtendedPOS::Pronoun, expanded_demonstrative);
    } else if (utf8::equalsAny(token.surface, {"ん"}) && previous != nullptr &&
               previous->pos == core::PartOfSpeech::Verb && following != nullptr &&
               utf8::equalsAny(following->surface, {"す", "し"}) && following->getLemma() == "する") {
      // The courtesan polite んす (あり+んす = あります), not a negative plus する.
      mergeInto(token, *following);
      retagUninflected(token, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxTenseMasu, "ます");
      eraseAfter(result, idx);
    } else if (utf8::equalsAny(token.surface, {"や"}) && token.pos == core::PartOfSpeech::Particle &&
               previous != nullptr &&
               (previous->pos == core::PartOfSpeech::Noun || previous->pos == core::PartOfSpeech::Pronoun ||
                previous->pos == core::PartOfSpeech::Adverb) &&
               (following == nullptr ||
                (following->pos == core::PartOfSpeech::Particle && !utf8::equalsAny(following->surface, {"の"})))) {
      // A coordinating や needs a following noun; at a clause end after a
      // nominal it is the western copula (そう+や+で, 雨+や+な).
      retagCopulaDa(token);
    } else if (utf8::equalsAny(token.surface, {"それで"}) && token.pos == core::PartOfSpeech::Conjunction &&
               following != nullptr && utf8::equalsAny(following->getLemma(), {"いい", "ええ", "よい", "ござる"})) {
      // Directly before the predicate it complements, それで is それ+で.
      core::Morpheme copula = splitTail(token, 2);
      retagUninflected(token, core::PartOfSpeech::Pronoun, core::ExtendedPOS::Pronoun, "それ");
      retagCopulaDa(copula);
      insertAfter(result, idx, copula);
    } else if (utf8::equalsAny(token.surface, {"マジ"}) && following != nullptr &&
               utf8::equalsAny(following->surface, {"で"})) {
      // The katakana spelling of まじ takes the same adjectival reading (まじ+で).
      retagUninflected(token, core::PartOfSpeech::Adjective, core::ExtendedPOS::AdjNaAdj, "まじ");
      retagCopulaDa(result[idx + 1]);
    }
  }
}

void retagIchidanContinuative(core::Morpheme& stem) {
  stem.pos = core::PartOfSpeech::Verb;
  stem.extended_pos = core::ExtendedPOS::VerbRenyokei;
  stem.conj_type = dictionary::ConjugationType::Ichidan;
  stem.lemma = stem.surface + "る";
}

bool retagContinuativeAsVerb(core::Morpheme& stem) {
  if (retagGodanRenyokeiFromIRow(stem, true)) {
    return true;
  }
  if (grammar::endsWithERow(stem.surface)) {
    retag(stem, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, stem.surface + "る",
          dictionary::ConjugationType::Ichidan, grammar::ConjForm::Renyokei);
    return true;
  }
  return false;
}

namespace {

// Godan dictionary form from an a-row irrealis stem (思わ → 思う), or empty.
std::string godanBaseFromIrrealis(std::string_view stem) {
  const std::string_view suffix = grammar::godanBaseSuffixFromARow(utf8::decodeLastChar(stem));
  if (suffix.empty()) {
    return "";
  }
  return normalize::concat(utf8::dropLastChar(stem), suffix);
}

}  // namespace

// Verb-frame readings the surrounding frame decides, mirroring the reference:
// 思わず before に is 思わ+ず; する+closed subsidiary splits (し+そこなう);
// て+こう is the contracted volitional of ていく (て+こ+う); a continuative noun
// before に and a motion verb, or after お before する, is the verb continuative;
// にもまして is the te-form of 増す; ます after に関し makes it に+関し; the す
// causative's hypothetical before ば is す's own cell; 来る's causative is
// 来+させ.
void resolveVerbFrameRepairs(std::vector<core::Morpheme>& result) {
  for (size_t idx = 0; idx < result.size(); ++idx) {
    auto& token = result[idx];
    const core::Morpheme* previous = morphemeBefore(result, idx);
    const core::Morpheme* following = morphemeAfter(result, idx, 1);
    const core::Morpheme* after = morphemeAfter(result, idx, 2);
    const size_t length = normalize::utf8Length(token.surface);
    if (token.pos == core::PartOfSpeech::Adverb && utf8::endsWith(token.surface, "ず") && following != nullptr &&
        utf8::equalsAny(following->surface, {"に"}) && length >= 2) {
      const std::string stem(utf8::dropLastChar(token.surface));
      const std::string base = godanBaseFromIrrealis(stem);
      if (!base.empty()) {
        core::Morpheme negative = splitTail(token, length - 1);
        retag(token, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbMizenkei, base, dictionary::ConjugationType::None,
              grammar::ConjForm::Mizenkei);
        retagUninflected(negative, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxNegativeNu, "ぬ");
        insertAfter(result, idx, negative);
      }
    } else if (token.pos == core::PartOfSpeech::Verb && length >= 3 &&
               (utf8::startsWith(token.surface, "しそこな") || utf8::startsWith(token.surface, "しそこね") ||
                utf8::startsWith(token.surface, "しそびれ"))) {
      const std::string lemma = token.lemma;
      core::Morpheme subsidiary = splitTail(token, 1);
      retag(token, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "する", dictionary::ConjugationType::Suru,
            grammar::ConjForm::Renyokei);
      subsidiary.pos = core::PartOfSpeech::Auxiliary;
      subsidiary.lemma = lemma.substr(core::kJapaneseCharBytes);
      insertAfter(result, idx, subsidiary);
    } else if (utf8::equalsAny(token.surface, {"こう"}) && token.pos == core::PartOfSpeech::Adverb &&
               previous != nullptr && utf8::equalsAny(previous->surface, {"て", "で"})) {
      core::Morpheme volitional = splitTail(token, 1);
      retagUninflected(token, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxAspectIku, "いく");
      retagUninflected(volitional, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxVolitional, "う");
      insertAfter(result, idx, volitional);
    } else if (token.pos == core::PartOfSpeech::Noun && following != nullptr &&
               utf8::equalsAny(following->surface, {"に"}) && after != nullptr &&
               isMotionVerbLemma(after->getLemma()) &&
               (token.extended_pos == core::ExtendedPOS::NounVerbal || grammar::isPureHiragana(token.surface)) &&
               kana::isIRowCodepoint(utf8::decodeLastChar(token.surface))) {
      retagContinuativeAsVerb(token);
    } else if (token.pos == core::PartOfSpeech::Noun && previous != nullptr &&
               previous->pos == core::PartOfSpeech::Prefix && utf8::equalsAny(previous->surface, {"お"}) &&
               following != nullptr && following->getLemma() == "する" && !grammar::isAllKanji(token.surface)) {
      retagContinuativeAsVerb(token);
    } else if (utf8::equalsAny(token.surface, {"まして"}) && token.pos == core::PartOfSpeech::Conjunction &&
               previous != nullptr && utf8::equalsAny(previous->surface, {"も"})) {
      core::Morpheme te = splitTail(token, 2);
      retag(token, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "増す",
            dictionary::ConjugationType::GodanSa, grammar::ConjForm::Renyokei);
      retagUninflected(te, core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleConj, "て");
      insertAfter(result, idx, te);
    } else if (token.pos == core::PartOfSpeech::Particle && utf8::startsWith(token.surface, "に") && length >= 3 &&
               utf8::endsWith(token.surface, "し") && following != nullptr &&
               following->extended_pos == core::ExtendedPOS::AuxTenseMasu) {
      core::Morpheme verb = splitTail(token, 1);
      retagUninflected(token, core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleCase, "に");
      retag(verb, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei,
            normalize::concat(utf8::dropLastChar(verb.surface), "する"), dictionary::ConjugationType::Suru,
            grammar::ConjForm::Renyokei);
      insertAfter(result, idx, verb);
    } else if (utf8::equalsAny(token.surface, {"せ"}) && token.pos == core::PartOfSpeech::Auxiliary &&
               token.getLemma() == "せる" && following != nullptr && utf8::equalsAny(following->surface, {"ば"}) &&
               previous != nullptr && previous->pos == core::PartOfSpeech::Verb &&
               kana::isARowCodepoint(utf8::decodeLastChar(previous->surface))) {
      token.lemma = "す";
    } else if (utf8::equalsAny(token.surface, {"来さ"}) && token.pos == core::PartOfSpeech::Verb &&
               following != nullptr && utf8::startsWith(following->surface, "せ")) {
      core::Morpheme sa = splitTail(token, 1);
      result[idx + 1].surface = sa.surface + result[idx + 1].surface;
      result[idx + 1].start = sa.start;
      result[idx + 1].lemma = "させる";
    }
  }
}

}  // namespace suzume::postprocess::resolver
