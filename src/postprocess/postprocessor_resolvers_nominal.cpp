#include <string_view>
#include <utility>

#include "core/debug.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "grammar/honorific_verbs.h"
#include "grammar/inflection_scorer_constants.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "postprocess/postprocessor.h"
#include "postprocess/postprocessor_resolvers_internal.h"

namespace suzume::postprocess::resolver {

namespace {

bool followsAttributiveModifier(const std::vector<core::Morpheme>& result, size_t idx) {
  if (idx == 0) {
    return false;
  }
  const auto& modifier = result[idx - 1];
  return modifier.extended_pos == core::ExtendedPOS::AdjBasic || modifier.pos == core::PartOfSpeech::Determiner ||
         (modifier.extended_pos == core::ExtendedPOS::AuxCopulaDa && grammar::isAttributiveCopulaNa(modifier.surface));
}

bool hasDictionaryGodanBaseFromIRow(const core::Morpheme& noun, const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || noun.surface.empty()) {
    return false;
  }
  const std::string_view base_suffix =
      grammar::godanBaseSuffixFromIRow(utf8::decodeFirstChar(utf8::lastChar(noun.surface)));
  if (base_suffix.empty()) {
    return false;
  }
  const std::string base_form = normalize::concat(utf8::dropLastChar(noun.surface), base_suffix);
  return dict_manager->lookupExact(base_form, core::PartOfSpeech::Verb) != nullptr;
}

}  // namespace

// A duration quantity directly followed by かかる requires the predicate
// reading. The homographic determiner is only available before a noun.
void resolveDurationPredicateKakaru(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx < result.size(); ++idx) {
    const auto& duration = result[idx - 1];
    auto& kakaru = result[idx];
    if (duration.pos != core::PartOfSpeech::Noun || !isCounterDurationNoun(duration.surface) ||
        kakaru.surface != "かかる" || kakaru.pos != core::PartOfSpeech::Determiner) {
      continue;
    }
    retag(kakaru, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbShuushikei, "かかる",
          dictionary::ConjugationType::GodanRa, grammar::ConjForm::Base);
  }
}

// An attributive adjective or determiner must select a nominal head. When that
// head is homographic with a verb continuative, the lattice can retain the
// verbal reading even though two predicates cannot be adjacent in this frame
// (正しい+行い, 静かな+違い). Retag the productive continuative as a deverbal
// noun. A closed formal-noun entry can mask the same ordinary reading; only
// replace it when its i-row surface reconstructs an independently attested
// Godan verb, preserving genuine formal nouns such as こと and もの.
void resolveAttributiveDeverbalNoun(std::vector<core::Morpheme>& result,
                                    const dictionary::DictionaryManager* dict_manager) {
  for (size_t idx = 1; idx < result.size(); ++idx) {
    auto& head = result[idx];
    if (!followsAttributiveModifier(result, idx)) {
      continue;
    }
    const bool deverbal_noun_shape =
        head.pos == core::PartOfSpeech::Verb &&
        (head.extended_pos == core::ExtendedPOS::VerbRenyokei || head.extended_pos == core::ExtendedPOS::VerbOnbinkei);
    if (deverbal_noun_shape) {
      retagNounSurface(head);
      continue;
    }
    if (head.extended_pos == core::ExtendedPOS::NounFormal && head.lemma != head.surface &&
        hasDictionaryGodanBaseFromIRow(head, dict_manager)) {
      retagNounSurface(head);
    }
  }
}

// A kana-spelled, dictionary-verified continuative before a kanji event-noun
// suffix exposes an explicit orthographic boundary (ねがい+事). The lattice
// selects the homographic formal noun because both readings have the same
// surface; repair only its grammatical role after the boundary is fixed.
void resolveDeverbalNominalSuffix(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx < result.size(); ++idx) {
    const auto& predecessor = result[idx - 1];
    auto& suffix = result[idx];
    if (predecessor.extended_pos != core::ExtendedPOS::VerbRenyokei || !predecessor.fromDictionary() ||
        !grammar::isPureHiragana(predecessor.surface) || suffix.extended_pos != core::ExtendedPOS::NounFormal ||
        !grammar::isDeverbalNominalSuffix(suffix.surface)) {
      continue;
    }
    retag(suffix, core::PartOfSpeech::Suffix, core::ExtendedPOS::Suffix, suffix.surface,
          dictionary::ConjugationType::None, grammar::ConjForm::Base);
  }
}

// A compound-verb 連用形 (積み重ね, 組み立て, 話し合い, 繰り返し) is systematically usable as
// a 連用形転成名詞. Its shape: starts with kanji, ends with hiragana, is composed only of
// kanji and hiragana, and contains at least two disjoint kanji runs (V1漢字…V2漢字…). The
// two-run gate is the compound-origin restriction: it excludes single-verb renyokei
// (読み/書き/続け — one kanji run, genuinely verbal far more often) and hiragana-stem forms
// (やり直し), keeping only V1+V2 compounds whose nominal reading is reliable.
bool isCompoundRenyokeiShape(const std::string& surface) {
  auto codepoints = normalize::toCodepoints(surface);
  if (codepoints.size() < 3) {
    return false;
  }
  if (normalize::classifyChar(codepoints.front()) != normalize::CharType::Kanji ||
      normalize::classifyChar(codepoints.back()) != normalize::CharType::Hiragana) {
    return false;
  }
  // A 連用形 ends in an i-row (godan 話し合い→い) or e-row (ichidan 積み重ね→ね, 組み立て→て)
  // mora; a base form (終止形) ends in a う-row godan terminal (呼び出す, 受け継ぐ) or ichidan
  // る. Reject base-form endings so a compound 終止形 the lattice still tags VerbRenyokei
  // (呼び出す, 着付け直す) is not wrongly nominalized — only true renyokei nominalize.
  const char32_t last_cp = codepoints.back();
  if (last_cp == U'う' || last_cp == U'く' || last_cp == U'ぐ' || last_cp == U'す' || last_cp == U'つ' ||
      last_cp == U'ぬ' || last_cp == U'ぶ' || last_cp == U'む' || last_cp == U'る') {
    return false;
  }
  size_t kanji_runs = 0;
  bool in_kanji_run = false;
  for (char32_t code : codepoints) {
    const normalize::CharType type = normalize::classifyChar(code);
    if (type != normalize::CharType::Kanji && type != normalize::CharType::Hiragana) {
      return false;
    }
    const bool is_kanji = (type == normalize::CharType::Kanji);
    if (is_kanji && !in_kanji_run) {
      ++kanji_runs;
    }
    in_kanji_run = is_kanji;
  }
  return kanji_runs >= 2;
}

// Right context that forces the nominal reading of a compound renyokei: a case particle
// (が/を/に/で/へ/…) or a topic/binding particle (は/も) marking it as an argument.
bool isNominalForcingParticle(const core::Morpheme& next) {
  return next.pos == core::PartOfSpeech::Particle && (next.extended_pos == core::ExtendedPOS::ParticleCase ||
                                                      next.extended_pos == core::ExtendedPOS::ParticleTopic);
}

// A noun followed by で is normally the case-particle reading (本で、電車で、
// 雪国であった). Copular continuations (本である、本ではない、本でしかない)
// retain the auxiliary reading, so resolve the ambiguity after both adjacent
// morphemes have been selected.
void resolveNominalCaseDe(std::vector<core::Morpheme>& result) {
  for (size_t idx = 0; idx < result.size(); ++idx) {
    auto& de = result[idx];
    if (de.surface != "で") {
      continue;
    }
    core::Morpheme* successor = idx + 1 < result.size() ? &result[idx + 1] : nullptr;
    if (idx == 0) {
      if (successor != nullptr && utf8::equalsAny(successor->surface, {"ござい", "ござる"})) {
        retagCopulaDa(de);
      }
      continue;
    }
    const auto& predecessor = result[idx - 1];
    const bool is_nominal = predecessor.pos == core::PartOfSpeech::Noun ||
                            predecessor.pos == core::PartOfSpeech::Pronoun ||
                            predecessor.pos == core::PartOfSpeech::Suffix;
    const bool is_na_adjective = predecessor.extended_pos == core::ExtendedPOS::AdjNaAdj;
    // The binding particle も after で names the copula only when the host
    // cannot take the exemplification particle でも in its place. A formal noun
    // heads a copular predicate rather than naming a thing, so it belongs with
    // the na-adjective stem and the adverb rather than with a referential noun.
    const bool is_formal_noun = predecessor.extended_pos == core::ExtendedPOS::NounFormal;
    const bool is_contracted_negative =
        predecessor.extended_pos == core::ExtendedPOS::AuxNegativeNu && predecessor.surface == "ん";
    const bool is_tendency_suffix = predecessor.extended_pos == core::ExtendedPOS::SuffixTendency;
    const bool is_adverbial_predicate = predecessor.pos == core::PartOfSpeech::Adverb;
    // The nominalizer turns the clause in front of it into the subject of a
    // copular predicate, which is the whole point of のである. It is the one
    // host that also licenses the onbin cell あっ: after a place noun で+あっ
    // is the case particle plus 有る (公園であった出来事), but a nominalized
    // clause has no such reading.
    const bool is_nominalized_clause = predecessor.extended_pos == core::ExtendedPOS::ParticleNo;
    if (!is_nominal && !is_na_adjective && !is_tendency_suffix && !is_adverbial_predicate && !is_contracted_negative &&
        !is_nominalized_clause) {
      continue;
    }
    const bool follows_negative =
        successor != nullptr && utf8::equalsAny(successor->surface, {"ない", "なく", "なかっ", "なかろ", "なけれ"});
    const bool topic_starts_copular_negative =
        successor != nullptr && successor->extended_pos == core::ExtendedPOS::ParticleTopic &&
        ((successor->surface == "は" && idx + 2 < result.size() &&
          (utf8::equalsAny(result[idx + 2].surface, {"ない", "なく", "なかっ", "なかろ"}) ||
           (is_na_adjective && (utf8::equalsAny(result[idx + 2].surface, {"ござい", "ござる"}) ||
                                result[idx + 2].extended_pos == core::ExtendedPOS::AuxGozaru)))) ||
         (successor->surface == "も" &&
          (((is_na_adjective || is_formal_noun || is_nominalized_clause) && idx + 2 < result.size() &&
            utf8::equalsAny(result[idx + 2].surface, {"ない", "なく", "なかっ"})) ||
           is_adverbial_predicate || is_contracted_negative)));
    const bool topic_starts_copular_aru =
        successor != nullptr && successor->extended_pos == core::ExtendedPOS::ParticleTopic &&
        idx + 2 < result.size() && utf8::equalsAny(result[idx + 2].surface, {"ある", "あり", "あろ", "あれ"});
    const bool binding_is_copular = successor != nullptr &&
                                    successor->extended_pos == core::ExtendedPOS::ParticleBinding &&
                                    (successor->surface != "も" || is_adverbial_predicate || is_contracted_negative);
    const bool is_copular_continuation =
        successor != nullptr &&
        (successor->extended_pos == core::ExtendedPOS::AuxGozaru || follows_negative || topic_starts_copular_negative ||
         topic_starts_copular_aru || binding_is_copular ||
         (successor->extended_pos == core::ExtendedPOS::AuxCopulaDa && successor->surface != "あっ") ||
         ((is_na_adjective || is_nominalized_clause) && successor->surface == "あっ") ||
         utf8::equalsAny(successor->surface, {"ある", "あり", "あろ", "あれ", "ござい", "ござる"}));
    const bool na_adjective_coordination =
        is_na_adjective && successor != nullptr && successor->extended_pos == core::ExtendedPOS::AdjBasic;
    // A na-adjective cannot take the case-particle reading of で. Its
    // continuative form remains the copula even before an independent noun
    // (無鉄砲で小供の時から).
    if (is_na_adjective || is_tendency_suffix || is_copular_continuation || na_adjective_coordination) {
      if (is_tendency_suffix) {
        auto& tendency = result[idx - 1];
        tendency.pos = core::PartOfSpeech::Suffix;
        tendency.lemma = "がち";
      }
      retagCopulaDa(de);
      if (topic_starts_copular_aru) {
        auto& aru = result[idx + 2];
        const bool followed_by_auxiliary =
            idx + 3 < result.size() && result[idx + 3].pos == core::PartOfSpeech::Auxiliary;
        if (aru.conj_form == grammar::ConjForm::Base && !followed_by_auxiliary) {
          retag(aru, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbShuushikei, "ある",
                dictionary::ConjugationType::GodanRa, grammar::ConjForm::Base);
        } else {
          aru.pos = core::PartOfSpeech::Auxiliary;
          aru.extended_pos = core::ExtendedPOS::AuxCopulaDa;
          aru.lemma = "ある";
          aru.conj_type = dictionary::ConjugationType::GodanRa;
        }
      }
      // The public token contract keeps the ある half of terminal である as a
      // lexical verb regardless of whether the nominal host is overt or the
      // nominalized clause の. Inflected copular cells retain their existing
      // auxiliary treatment.
      if (is_nominalized_clause && successor != nullptr &&
          utf8::equalsAny(successor->surface, {"ある", "あっ", "あり", "あろ", "あれ"})) {
        const bool followed_by_auxiliary =
            idx + 2 < result.size() && result[idx + 2].pos == core::PartOfSpeech::Auxiliary;
        if (successor->conj_form == grammar::ConjForm::Base && !followed_by_auxiliary) {
          retag(*successor, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbShuushikei, "ある",
                dictionary::ConjugationType::GodanRa, grammar::ConjForm::Base);
        } else {
          successor->pos = core::PartOfSpeech::Auxiliary;
          successor->extended_pos = core::ExtendedPOS::AuxCopulaDa;
          successor->lemma = "ある";
          successor->conj_type = dictionary::ConjugationType::GodanRa;
        }
      }
      if (successor != nullptr && successor->surface == "ござる") {
        auto& gozaru = *successor;
        retag(gozaru, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxGozaru, "ござる",
              dictionary::ConjugationType::GodanRa, grammar::ConjForm::Base);
      }
      continue;
    }
    retag(de, core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleCase, "で", dictionary::ConjugationType::None,
          grammar::ConjForm::Base);
  }
}

// In the formal conjectural copula であろう, あろ is the irrealis of the
// copular auxiliary ある. The same surface remains a lexical verb outside
// this directly preceding copula context.
void resolveCopularAro(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx < result.size(); ++idx) {
    const auto& copula = result[idx - 1];
    auto& aro = result[idx];
    if (copula.extended_pos != core::ExtendedPOS::AuxCopulaDa || copula.surface != "で" || aro.surface != "あろ" ||
        aro.extended_pos != core::ExtendedPOS::VerbMizenkei || aro.lemma != "ある") {
      continue;
    }
    aro.pos = core::PartOfSpeech::Auxiliary;
    aro.extended_pos = core::ExtendedPOS::AuxCopulaDa;
    aro.conj_type = dictionary::ConjugationType::GodanRa;
    aro.conj_form = grammar::ConjForm::Mizenkei;
  }
}

// A noun plus the conditional particle なら is not the mizenkei of なる.
// Resolve the shared surface after its nominal left context is available.
void resolveNominalConditionalNara(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx < result.size(); ++idx) {
    const auto& predecessor = result[idx - 1];
    auto& nara = result[idx];
    const bool follows_nominal_or_finite =
        predecessor.pos == core::PartOfSpeech::Noun || predecessor.extended_pos == core::ExtendedPOS::VerbShuushikei ||
        predecessor.extended_pos == core::ExtendedPOS::AdjBasic || predecessor.pos == core::PartOfSpeech::Auxiliary;
    const bool limiting_chain = predecessor.surface == "のみ" && idx + 1 < result.size() &&
                                result[idx + 1].extended_pos == core::ExtendedPOS::AuxNegativeNu;
    // The conditional particle opens a clause, while an irrealis is completed
    // by what follows it. A negative auxiliary directly behind なら is
    // therefore the verb's own cell and not a conditional at all — which holds
    // for a formal-noun host (ほか+なら+ない) exactly as it does for the
    // obligation chain (なきゃ+なら+ない) that used to be listed on its own.
    // The limiting のみならず keeps the particle reading its own rule selects.
    const bool completed_by_negative = !limiting_chain && idx + 1 < result.size() &&
                                       (result[idx + 1].extended_pos == core::ExtendedPOS::AuxNegativeNai ||
                                        result[idx + 1].extended_pos == core::ExtendedPOS::AuxNegativeNu);
    if ((!follows_nominal_or_finite && !limiting_chain) || nara.surface != "なら" ||
        nara.extended_pos != core::ExtendedPOS::VerbMizenkei || completed_by_negative) {
      continue;
    }
    retag(nara, core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleConj, "なら",
          dictionary::ConjugationType::None, grammar::ConjForm::Base);
  }
}

// After the nominal case-particle reading of で, あっ is the lexical verb
// ある in its sokuonbin form (雪国であった).  The lattice shares this surface
// with the formal-copula auxiliary, so retain the selected boundary and
// resolve the public POS from the preceding nominal construction.
void resolveNominalDeAru(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx + 1 < result.size(); ++idx) {
    const auto& de = result[idx - 1];
    auto& aru = result[idx];
    const auto& tense = result[idx + 1];
    if (de.extended_pos != core::ExtendedPOS::ParticleCase || de.surface != "で" || aru.surface != "あっ" ||
        tense.extended_pos != core::ExtendedPOS::AuxTenseTa) {
      continue;
    }
    retag(aru, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbOnbinkei, "ある", dictionary::ConjugationType::GodanRa,
          grammar::ConjForm::Onbinkei);
  }
}

// 以上 is a comparison-boundary noun in the public token contract (本である
// 以上、十倍以上). The lattice also emits an adverbial homograph, so restore the
// nominal category after token boundaries have been selected.
void resolveComparisonNoun(std::vector<core::Morpheme>& result) {
  for (size_t idx = 0; idx < result.size(); ++idx) {
    auto& morpheme = result[idx];
    // Quantity+近く is the approximate-count noun (百件+近く), whereas
    // predicate/adjective contexts keep the ordinary adjective 近い
    // (駅の近く, 近くない). The numeric ExtendedPOS supplies the local gate.
    if (morpheme.surface == "近く" && idx > 0 && result[idx - 1].extended_pos == core::ExtendedPOS::NounNumber) {
      retag(morpheme, core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, "近く", dictionary::ConjugationType::None,
            grammar::ConjForm::Base);
      continue;
    }
    if (morpheme.surface != "以上") {
      continue;
    }
    retag(morpheme, core::PartOfSpeech::Noun, core::ExtendedPOS::Noun, "以上", dictionary::ConjugationType::None,
          grammar::ConjForm::Base);
  }
}

// ござい followed by the polite auxiliary is either the lexical existential
// verb ござる (ございます, そこにございました) or the dependent honorific
// auxiliary in a copular/polite expression (でございます, ありがとうござい
// ます). The preceding boundary distinguishes the two readings.
void resolveGozaruPoliteAuxiliary(std::vector<core::Morpheme>& result) {
  for (size_t idx = 0; idx + 1 < result.size(); ++idx) {
    auto& gozai = result[idx];
    const auto& polite = result[idx + 1];
    if (gozai.surface != "ござい" || polite.extended_pos != core::ExtendedPOS::AuxTenseMasu) {
      continue;
    }
    const bool lexical_existence = idx == 0 || (result[idx - 1].surface == "に" &&
                                                result[idx - 1].extended_pos == core::ExtendedPOS::ParticleCase);
    if (lexical_existence) {
      retag(gozai, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbRenyokei, "ござる",
            dictionary::ConjugationType::GodanRa, grammar::ConjForm::Renyokei);
      continue;
    }
    retag(gozai, core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxGozaru, "ござる",
          dictionary::ConjugationType::None, grammar::ConjForm::Base);
  }
}

// The nominalizing さ follows an adjective stem (高+さ、儚+さ、っぽ+さ).
// It is homographic with the final particle, whose candidate can win before
// the preceding adjective's category is available to the lattice scorer.
void resolveAdjectiveNominalizerSa(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx < result.size(); ++idx) {
    auto& suffix = result[idx];
    const auto& adjective = result[idx - 1];
    if (suffix.surface != "さ" || adjective.pos != core::PartOfSpeech::Adjective) {
      continue;
    }
    if (idx + 1 < result.size() && result[idx + 1].extended_pos == core::ExtendedPOS::AuxPassive) {
      continue;
    }
    retag(suffix, core::PartOfSpeech::Suffix, core::ExtendedPOS::Suffix, "さ", dictionary::ConjugationType::None,
          grammar::ConjForm::Base);
    if (idx + 1 < result.size() && result[idx + 1].surface == "そう") {
      auto& sou = result[idx + 1];
      retagAppearanceSou(sou);
    }
  }
}

// The observation suffix がる selects an adjective stem, so an unattested
// nominal in front of it is a 形容動詞語幹 (面倒がる) and keeps the unknown-noun
// category it was generated with otherwise. A registered host is left alone:
// its entry states the category the dictionary means it to have, and several
// 形容動詞語幹 are registered as nouns for their equally common nominal use
// (不安がある).
void resolveAdjectivalStemBeforeGaru(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx < result.size(); ++idx) {
    auto& host = result[idx - 1];
    if (result[idx].extended_pos != core::ExtendedPOS::AuxGaru || host.pos != core::PartOfSpeech::Noun ||
        host.fromDictionary()) {
      continue;
    }
    retag(host, core::PartOfSpeech::Adjective, core::ExtendedPOS::AdjNaAdj, host.surface,
          dictionary::ConjugationType::NaAdjective, grammar::ConjForm::Base);
  }
}

// In an indefinite phrase followed by a content predicate, homographic で is
// the case particle (どこ+か+で+確認する), not the copular continuative.
// Keep the copular reading before auxiliaries and lexical ある (何かである).
void resolveIndefiniteCaseDe(std::vector<core::Morpheme>& result) {
  for (size_t idx = 2; idx + 1 < result.size(); ++idx) {
    const auto& interrogative = result[idx - 2];
    const auto& indefinite = result[idx - 1];
    auto& de = result[idx];
    const auto& predicate = result[idx + 1];
    if (interrogative.extended_pos != core::ExtendedPOS::PronounInterrogative ||
        indefinite.extended_pos != core::ExtendedPOS::ParticleAdverbial || indefinite.surface != "か" ||
        de.surface != "で") {
      continue;
    }
    const bool starts_copular_negative = idx + 2 < result.size() &&
                                         predicate.extended_pos == core::ExtendedPOS::ParticleTopic &&
                                         result[idx + 2].extended_pos == core::ExtendedPOS::AuxNegativeNai;
    if (starts_copular_negative) {
      retagCopulaDa(de);
      continue;
    }
    if (de.extended_pos != core::ExtendedPOS::AuxCopulaDa || predicate.pos == core::PartOfSpeech::Auxiliary ||
        (predicate.pos == core::PartOfSpeech::Verb && predicate.lemma == "ある")) {
      continue;
    }
    retag(de, core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleCase, "で", dictionary::ConjugationType::None,
          grammar::ConjForm::Base);
  }
}

// A continuative form is used as a nominal predicate before a copula, a
// nominalizing suffix, or the copular change construction 〜に+なる. The
// lattice keeps a verb candidate for the same surface, but these contexts
// require a nominal reading (疲れ気味だ、丸出しだ、丸出しになる). Honorific
// お/ご+連用形+に+なる remains verbal.
void resolveNominalizedRenyokeiPredicate(std::vector<core::Morpheme>& result) {
  for (size_t idx = 0; idx < result.size(); ++idx) {
    auto& stem = result[idx];
    if (stem.pos != core::PartOfSpeech::Verb || idx + 1 >= result.size()) {
      continue;
    }

    const auto& next = result[idx + 1];
    // A case particle can expose an impossible generated verb reading without
    // any lexical knowledge: a finite predicate cannot govern directional へ,
    // and an imperative cannot be followed directly by case から.  The same
    // surfaces remain verbal before conjunctive から or across punctuation.
    const bool case_forces_nominal =
        next.extended_pos == core::ExtendedPOS::ParticleCase &&
        ((next.surface == "へ" && (stem.extended_pos == core::ExtendedPOS::VerbShuushikei ||
                                   stem.extended_pos == core::ExtendedPOS::VerbMeireikei)) ||
         (next.surface == "から" && stem.extended_pos == core::ExtendedPOS::VerbMeireikei));
    if (case_forces_nominal) {
      retagNounSurface(stem);
      continue;
    }

    if (stem.extended_pos != core::ExtendedPOS::VerbRenyokei) {
      continue;
    }

    // Keep every plain-copula form handled by the existing resolver (な/なら/
    // だろ included), and add the polite copula class.  isPredicativeCopula()
    // is intentionally narrower and would drop attributive/conjectural forms.
    const bool before_copula =
        next.extended_pos == core::ExtendedPOS::AuxCopulaDa || next.extended_pos == core::ExtendedPOS::AuxCopulaDesu;
    const bool before_nominalizing_suffix =
        next.pos == core::PartOfSpeech::Suffix && grammar::isRenyokeiNominalizingSuffix(next.surface);
    const bool before_naru = next.extended_pos == core::ExtendedPOS::ParticleCase && next.surface == "に" &&
                             idx + 2 < result.size() && result[idx + 2].pos == core::PartOfSpeech::Verb &&
                             result[idx + 2].lemma == "なる";
    // A wa-row continuative directly after に cannot take the negative
    // auxiliary as a finite predicate. It is the nominal stem in the
    // certainty construction (N に + V連用形 + ない), so retain the noun
    // candidate rather than the homographic verb analysis.
    const bool nominal_negative_after_case =
        idx > 0 && result[idx - 1].extended_pos == core::ExtendedPOS::ParticleCase && result[idx - 1].surface == "に" &&
        next.extended_pos == core::ExtendedPOS::AuxNegativeNai &&
        stem.conj_type == dictionary::ConjugationType::GodanWa;
    const bool honorific_naru = before_naru && idx > 0 && result[idx - 1].pos == core::PartOfSpeech::Prefix &&
                                grammar::isHonorificPrefix(result[idx - 1].surface);
    if ((!before_copula && !before_nominalizing_suffix && !before_naru && !nominal_negative_after_case) ||
        honorific_naru) {
      continue;
    }

    retagNounSurface(stem);
  }
}

// The exemplification particle でも is the copula's continuative で fused with
// the binding particle も, and the lattice scores that fusion as one closed
// token. Only a referential host can take it: a formal noun names no thing, and
// neither does the nominalizer の — both head a copular predicate — so in front
// of the copula's own supporting verb (である → でもある, ではない → でもない)
// the compositional reading is the only one available and the fusion has to be
// undone.
//
// Both halves of that condition carry weight, which is why the repair belongs
// here rather than in the connection rules. A formal noun still takes the
// exemplification particle in an argument position (ものでも食べよう,
// 行ったはずでも探す), and a referential host keeps the fusion in front of the
// same verb (学生でもない, 大した問題でもない). The deciding window therefore
// spans three tokens, and the scorer sees an adjacent pair.
void splitFormalNounCopularDemo(std::vector<core::Morpheme>& result) {
  for (size_t idx = 1; idx + 1 < result.size(); ++idx) {
    const auto& host = result[idx - 1];
    auto& copula = result[idx];
    const auto& predicate = result[idx + 1];
    const bool supporting_verb =
        predicate.pos != core::PartOfSpeech::Determiner && utf8::equalsAny(predicate.getLemma(), {"ある", "ない"});
    const bool copular_head =
        host.extended_pos == core::ExtendedPOS::NounFormal || host.extended_pos == core::ExtendedPOS::ParticleNo;
    if (!copular_head || copula.extended_pos != core::ExtendedPOS::ParticleAdverbial ||
        !utf8::equalsAny(copula.surface, {"でも"}) || host.end != copula.start || copula.end != predicate.start ||
        !supporting_verb) {
      continue;
    }

    core::Morpheme focus = copula;
    copula.surface = "で";
    copula.end = copula.start + 1;
    retagCopulaDa(copula);

    focus.surface = "も";
    focus.start = copula.end;
    retag(focus, core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleTopic, "も",
          dictionary::ConjugationType::None, grammar::ConjForm::Base);
    result.insert(result.begin() + static_cast<std::ptrdiff_t>(idx + 1), focus);
    ++idx;
  }

  // A referential host keeps でも fused as the exemplification particle, but
  // its following terminal ある has the same lexical-verb role as the
  // supporting verb in the decomposed で+も+ある chain.
  for (size_t idx = 1; idx < result.size(); ++idx) {
    const auto& demo = result[idx - 1];
    auto& aru = result[idx];
    const bool followed_by_auxiliary = idx + 1 < result.size() && result[idx + 1].pos == core::PartOfSpeech::Auxiliary;
    if (demo.extended_pos == core::ExtendedPOS::ParticleAdverbial && demo.getLemma() == "でも" &&
        aru.getLemma() == "ある" && aru.conj_form == grammar::ConjForm::Base && !followed_by_auxiliary) {
      retag(aru, core::PartOfSpeech::Verb, core::ExtendedPOS::VerbShuushikei, "ある",
            dictionary::ConjugationType::GodanRa, grammar::ConjForm::Base);
    }
  }
}

}  // namespace suzume::postprocess::resolver
