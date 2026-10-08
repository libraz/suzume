/**
 * @file verb_candidates_helpers_absorption.cpp
 * @brief Fabricated closed-class absorption guards for verb candidate helpers
 */

#include <algorithm>
#include <array>

#include "analysis/candidate_constants.h"
#include "analysis/dictionary_probe.h"
#include "analysis/tokenizer_utils.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "grammar/inflection.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "verb_candidates_absorption_guards.h"
#include "verb_candidates_dictionary_probes.h"
#include "verb_candidates_verb_stems.h"

namespace suzume::analysis::verb_helpers {

bool endsWithParticleTailOfPos(const dictionary::DictionaryManager* dict_manager,
                               const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                               core::ExtendedPOS particle_pos) {
  if (dict_manager == nullptr || end_pos <= start_pos || end_pos > codepoints.size()) {
    return false;
  }
  // Strip a trailing inflecting auxiliary. A focus particle can precede a
  // negative (本だけない) or a copula (本だけだ / 本だけだった); neither
  // sequence belongs inside a fabricated lexical candidate.
  size_t tail_end = end_pos;
  size_t total_len = end_pos - start_pos;
  if (total_len >= 4 && codepoints[end_pos - 4] == U'な' && codepoints[end_pos - 3] == U'か' &&
      codepoints[end_pos - 2] == U'っ' && codepoints[end_pos - 1] == U'た') {
    tail_end = end_pos - 4;
  } else if (total_len >= 3 && codepoints[end_pos - 3] == U'な' && codepoints[end_pos - 2] == U'か' &&
             codepoints[end_pos - 1] == U'っ') {
    tail_end = end_pos - 3;
  } else if (total_len >= 2 && codepoints[end_pos - 2] == U'な' && codepoints[end_pos - 1] == U'い') {
    tail_end = end_pos - 2;
  }
  total_len = tail_end - start_pos;
  if (total_len >= 2 && codepoints[tail_end - 2] == U'だ' && codepoints[tail_end - 1] == U'っ') {
    tail_end -= 2;
  } else if (total_len >= 1 && codepoints[tail_end - 1] == U'だ') {
    --tail_end;
  }
  // Probe particle suffixes of 2+ codepoints, keeping a non-empty prefix. A
  // prefix that is one particle mora with no host on its left is no word for
  // the particle to follow (、+はだけ+た).
  const bool bare_particle_mora_prefix =
      normalize::isParticleCodepoint(codepoints[start_pos]) && particleMoraLacksHost(codepoints, start_pos);
  for (size_t particle_len = 2; start_pos + particle_len < tail_end; ++particle_len) {
    if (bare_particle_mora_prefix && tail_end - particle_len == start_pos + 1) {
      continue;
    }
    const dictionary::DictionaryEntry* suffix_entry =
        lookupEntryInRange(*dict_manager, codepoints, tail_end - particle_len, tail_end);
    if (suffix_entry != nullptr && suffix_entry->extended_pos == particle_pos) {
      return true;
    }
  }
  return false;
}

bool endsWithFocusParticleTail(const dictionary::DictionaryManager* dict_manager,
                               const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  return endsWithParticleTailOfPos(dict_manager, codepoints, start_pos, end_pos,
                                   core::ExtendedPOS::ParticleAdverbial) ||
         endsWithParticleTailOfPos(dict_manager, codepoints, start_pos, end_pos, core::ExtendedPOS::ParticleBinding);
}

bool startsWithFocusParticleHead(const dictionary::DictionaryManager* dict_manager,
                                 const std::vector<char32_t>& codepoints, size_t hiragana_start, size_t end_pos) {
  if (dict_manager == nullptr || end_pos < hiragana_start + 2 || end_pos > codepoints.size()) {
    return false;
  }
  // Longest focus particle in the closed class is four codepoints (どころか).
  constexpr size_t kMaxParticleLen = 4;
  const size_t max_len = std::min(kMaxParticleLen, end_pos - hiragana_start);
  for (size_t particle_len = 2; particle_len <= max_len; ++particle_len) {
    const size_t particle_end = hiragana_start + particle_len;
    const dictionary::DictionaryEntry* entry =
        lookupEntryInRange(*dict_manager, codepoints, hiragana_start, particle_end);
    if (entry == nullptr || (entry->extended_pos != core::ExtendedPOS::ParticleAdverbial &&
                             entry->extended_pos != core::ExtendedPOS::ParticleBinding)) {
      continue;
    }
    // An adjective past keeps っ right after the coinciding kana (美味しかっ +
    // た), so that sequence is genuine okurigana rather than a particle.
    if (particle_end < end_pos && codepoints[particle_end] == U'っ') {
      continue;
    }
    return true;
  }
  return false;
}

namespace {

// Whether the mora at @p pos continues one of the derivational suffixes that
// open on が. がる conjugates through its own ra row (欲しがら/がり/がる/がれ/がろ)
// and its onbin form (欲しがって); がたい and がまし〜 take the remaining two.
bool opensDerivationalGaSuffix(const std::vector<char32_t>& codepoints, size_t pos) {
  if (pos >= codepoints.size()) {
    return false;
  }
  constexpr std::array<char32_t, 8> kGaSuffixSecondMorae = {U'ら', U'り', U'る', U'れ', U'ろ', U'っ', U'た', U'ま'};
  return std::find(kGaSuffixSecondMorae.begin(), kGaSuffixSecondMorae.end(), codepoints[pos]) !=
         kGaSuffixSecondMorae.end();
}

}  // namespace

bool embedsCaseParticle(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                        size_t start_pos, size_t end_pos, bool include_genitive_and_wa) {
  if (dict_manager == nullptr || end_pos < start_pos + 3 || end_pos > codepoints.size()) {
    return false;
  }
  // Longest case particle in the closed class is three codepoints (からの/より).
  constexpr size_t kMaxParticleLen = 3;
  for (size_t particle_start = start_pos + 1; particle_start + 1 < end_pos; ++particle_start) {
    // が opens the productive derivational suffixes がまし〜 / がる / がたい, which
    // attach straight to a nominal or a continuative and so put the same mora
    // inside a single derived word (未練がましい, 恩着せがましさ, 欲しがる). Their
    // ambiguity is lexical rather than structural, so those spellings are left
    // to the confidence model. Elsewhere が is the nominative particle and marks
    // an argument boundary like any other case particle (人 が めちゃめちゃ).
    if (codepoints[particle_start] == U'が' && opensDerivationalGaSuffix(codepoints, particle_start + 1)) {
      continue;
    }
    const size_t max_len = std::min(kMaxParticleLen, end_pos - particle_start - 1);
    const bool embeds =
        include_genitive_and_wa
            ? hasDictionaryEntryFrom(dict_manager, codepoints, particle_start, 1, max_len, core::PartOfSpeech::Unknown,
                                     [](const dictionary::DictionaryEntry& entry) {
                                       return entry.extended_pos == core::ExtendedPOS::ParticleCase ||
                                              entry.extended_pos == core::ExtendedPOS::ParticleNo ||
                                              (entry.extended_pos == core::ExtendedPOS::ParticleTopic &&
                                               grammar::isSingleHiragana(entry.surface, U'は'));
                                     })
            : hasDictionaryEntryFrom(dict_manager, codepoints, particle_start, 1, max_len, core::PartOfSpeech::Unknown,
                                     [](const dictionary::DictionaryEntry& entry) {
                                       return entry.extended_pos == core::ExtendedPOS::ParticleCase;
                                     });
    if (embeds) {
      return true;
    }
  }
  return false;
}

bool endsWithCaseParticleAfterContinuative(const dictionary::DictionaryManager* dict_manager,
                                           const grammar::Inflection& inflection,
                                           const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  // A deverbal noun is at least a stem plus its continuative mora, so the span
  // needs a host of two codepoints before the particle.
  if (dict_manager == nullptr || end_pos < start_pos + 3 || end_pos > codepoints.size()) {
    return false;
  }
  const size_t particle_pos = end_pos - 1;
  const auto* particle =
      lookupEntryInRange(*dict_manager, codepoints, particle_pos, end_pos, core::PartOfSpeech::Particle);
  if (particle == nullptr || particle->extended_pos != core::ExtendedPOS::ParticleCase) {
    return false;
  }
  // The host must look like a continuative: an i-row mora is what nominalizes a
  // godan stem. No godan verb spells its own irrealis with that mora before the
  // one this guard rejects, which is why the shape can be required here even
  // though the particle is a single mora (和らが, 揺るが keep their candidates).
  if (!kana::isIRowCodepoint(codepoints[particle_pos - 1])) {
    return false;
  }
  const std::string host = extractSubstring(codepoints, start_pos, particle_pos);
  const grammar::InflectionCandidate best = inflection.getBest(host);
  // A host that analyses as its own base form is a terminal, not a continuative,
  // and carries no evidence that the mora before the particle ends a word.
  return best.base_form != host && isVerifiedVerbBase(dict_manager, inflection, best.base_form,
                                                      candidate::verb_cost::kConstructedVerbMinConfidence, true);
}

bool spellsClassicalAuxiliaryEnding(const dictionary::DictionaryManager* dict_manager, std::string_view surface,
                                    std::string_view stem) {
  if (dict_manager == nullptr || stem.empty() || surface.size() <= stem.size() ||
      surface.compare(0, stem.size(), stem) != 0) {
    return false;
  }
  const auto* ending = dict_manager->lookupExact(surface.substr(stem.size()), core::PartOfSpeech::Auxiliary);
  return ending != nullptr && core::isClassicalAuxiliaryType(ending->extended_pos);
}

bool endsWithAuxiliaryAfterOkurigana(const dictionary::DictionaryManager* dict_manager,
                                     const std::vector<char32_t>& codepoints, size_t okurigana_start, size_t end_pos) {
  return auxiliaryClosingAfterOkurigana(dict_manager, codepoints, okurigana_start, end_pos) != nullptr;
}

bool closesOnPoliteCopula(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                          size_t okurigana_start, size_t end_pos) {
  const auto* auxiliary = auxiliaryClosingAfterOkurigana(dict_manager, codepoints, okurigana_start, end_pos);
  return auxiliary != nullptr && auxiliary->extended_pos == core::ExtendedPOS::AuxCopulaDesu;
}

bool closesOnTerminalAuxiliaryAndConjunctive(const dictionary::DictionaryManager* dict_manager,
                                             const std::vector<char32_t>& codepoints, size_t stem_end) {
  constexpr size_t kMinAuxiliaryLen = 2;
  constexpr size_t kMaxAuxiliaryLen = 4;
  if (dict_manager == nullptr || stem_end == 0 || stem_end > codepoints.size()) {
    return false;
  }
  const auto* particle =
      lookupEntryInRange(*dict_manager, codepoints, stem_end - 1, stem_end, core::PartOfSpeech::Particle);
  if (particle == nullptr || particle->extended_pos != core::ExtendedPOS::ParticleConj) {
    return false;
  }
  const size_t aux_end = stem_end - 1;
  for (size_t aux_len = kMinAuxiliaryLen; aux_len <= kMaxAuxiliaryLen && aux_len <= aux_end; ++aux_len) {
    const auto* auxiliary =
        lookupEntryInRange(*dict_manager, codepoints, aux_end - aux_len, aux_end, core::PartOfSpeech::Auxiliary);
    if (auxiliary != nullptr && auxiliary->lemma == extractSubstring(codepoints, aux_end - aux_len, aux_end)) {
      return true;
    }
  }
  return false;
}

bool closesOnTerminalVerbAndConjunctive(const dictionary::DictionaryManager* dict_manager,
                                        const std::vector<char32_t>& codepoints, size_t start_pos, size_t stem_end) {
  constexpr size_t kMinVerbLen = 2;
  constexpr size_t kMaxVerbLen = 4;
  if (dict_manager == nullptr || stem_end < start_pos + kMinVerbLen + 1 || stem_end > codepoints.size()) {
    return false;
  }
  // The run may open on the case particle itself (大人+になるし), which hands
  // the verb to the position after it.
  const auto* opening_case =
      lookupEntryInRange(*dict_manager, codepoints, start_pos, start_pos + 1, core::PartOfSpeech::Particle);
  if (opening_case != nullptr && opening_case->extended_pos == core::ExtendedPOS::ParticleCase) {
    return closesOnTerminalVerbAndConjunctive(dict_manager, codepoints, start_pos + 1, stem_end);
  }
  if (!followsCaseParticle(dict_manager, codepoints, start_pos)) {
    return false;
  }
  const auto* particle =
      lookupEntryInRange(*dict_manager, codepoints, stem_end - 1, stem_end, core::PartOfSpeech::Particle);
  if (particle == nullptr || particle->extended_pos != core::ExtendedPOS::ParticleConj) {
    return false;
  }
  // The verb must be the whole of what stands between the argument and the particle.
  const size_t verb_end = stem_end - 1;
  if (verb_end - start_pos < kMinVerbLen || verb_end - start_pos > kMaxVerbLen) {
    return false;
  }
  const auto* verb = lookupEntryInRange(*dict_manager, codepoints, start_pos, verb_end, core::PartOfSpeech::Verb);
  return verb != nullptr && verb->lemma == extractSubstring(codepoints, start_pos, verb_end);
}

bool spellsContinuativeBeforePolite(const dictionary::DictionaryManager* dict_manager,
                                    const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  const auto* auxiliary = auxiliaryClosingAfterOkurigana(dict_manager, codepoints, start_pos, end_pos);
  if (auxiliary == nullptr || auxiliary->extended_pos != core::ExtendedPOS::AuxTenseMasu) {
    return false;
  }
  const size_t aux_len = normalize::utf8Length(auxiliary->surface);
  return end_pos >= start_pos + aux_len + 1 &&
         isVerbContinuativeSpan(dict_manager, codepoints, start_pos, end_pos - aux_len);
}

const dictionary::DictionaryEntry* auxiliaryClosingAfterOkurigana(const dictionary::DictionaryManager* dict_manager,
                                                                  const std::vector<char32_t>& codepoints,
                                                                  size_t okurigana_start, size_t end_pos) {
  // The closed class tops out at four codepoints, and a one-mora tail is also
  // how ordinary verbs spell their own endings, so only multi-mora auxiliaries
  // are evidence here.
  constexpr size_t kMaxAuxiliaryLen = 4;
  if (dict_manager == nullptr || end_pos > codepoints.size() || end_pos < okurigana_start + 2) {
    return nullptr;
  }
  const size_t max_len = std::min(kMaxAuxiliaryLen, end_pos - okurigana_start - 1);
  for (size_t aux_len = 2; aux_len <= max_len; ++aux_len) {
    const auto* auxiliary =
        lookupEntryInRange(*dict_manager, codepoints, end_pos - aux_len, end_pos, core::PartOfSpeech::Auxiliary);
    // けり's izenkei is spelled like the hypothetical ending every i-adjective
    // carries (なけれ, 高けれ), so that cell alone is no evidence of a boundary.
    // The voiced ておく contraction stands only on a nasal onbin (読ん+どい), so
    // elsewhere its kana are an ordinary ending (ひどい).
    const bool voiced_oku_off_nasal =
        auxiliary != nullptr && auxiliary->extended_pos == core::ExtendedPOS::AuxAspectOku &&
        utf8::startsWith(auxiliary->surface, "ど") && codepoints[end_pos - aux_len - 1] != core::hiragana::kN;
    // After an i-row continuative that does not close a dictionary adjective
    // stem, the cell is けり's after all (咲き+けれ). し and じ are left out:
    // the シク adjectives end their stem on them (若々し+けれ, 凄まじ+けれ), far
    // more often than a continuative plus けり does.
    const size_t aux_start = end_pos - aux_len;
    size_t stem_start = okurigana_start;
    // The stem is the kanji run, iteration mark included, before the okurigana (若々, 美).
    while (stem_start > 0 &&
           (normalize::isKanjiCodepoint(codepoints[stem_start - 1]) || codepoints[stem_start - 1] == U'々')) {
      --stem_start;
    }
    const bool keri_after_continuative =
        auxiliary != nullptr && auxiliary->extended_pos == core::ExtendedPOS::AuxClassicalKeri &&
        kana::isIRowCodepoint(codepoints[aux_start - 1]) && codepoints[aux_start - 1] != U'し' &&
        codepoints[aux_start - 1] != U'じ' &&
        !isAdjectiveInDictionary(dict_manager, codepoints, stem_start, aux_start, "い");
    if (auxiliary != nullptr &&
        (auxiliary->extended_pos != core::ExtendedPOS::AuxClassicalKeri || keri_after_continuative) &&
        !voiced_oku_off_nasal) {
      return auxiliary;
    }
  }
  return nullptr;
}

size_t negativeAuxiliaryLengthAt(const dictionary::DictionaryManager* dict_manager,
                                 const std::vector<char32_t>& codepoints, size_t pos) {
  // Longest negative auxiliary in the closed class is four codepoints (なけりゃ).
  constexpr size_t kMaxAuxLen = 4;
  // The negative paradigm has one-mora members too (ぬ, ず, ね, ん, じ). They are
  // reported like the rest; a single mora is weaker evidence than a multi-mora
  // auxiliary because it is also spelled like the end of an ordinary word, so
  // callers that build a candidate on this answer price the one-mora case.
  constexpr size_t kMinAuxLen = 1;
  return longestDictionaryEntryLengthFrom(dict_manager, codepoints, pos, kMinAuxLen, kMaxAuxLen,
                                          core::PartOfSpeech::Auxiliary, [](const dictionary::DictionaryEntry& entry) {
                                            return entry.extended_pos == core::ExtendedPOS::AuxNegativeNai ||
                                                   entry.extended_pos == core::ExtendedPOS::AuxNegativeNu;
                                          });
}

bool closedPredicateEndsAt(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                           size_t start_pos, size_t end_pos, bool span_lemma_attested) {
  if (dict_manager == nullptr || end_pos > codepoints.size() || end_pos <= start_pos) {
    return false;
  }
  constexpr size_t kMaxPredicateLen = 5;
  const size_t first = lookbehindStart(end_pos, kMaxPredicateLen);
  const size_t last = span_lemma_attested ? start_pos : start_pos + 1;
  for (size_t cell_start = first; cell_start < last && cell_start < end_pos; ++cell_start) {
    const auto* entry = lookupEntryInRange(*dict_manager, codepoints, cell_start, end_pos, core::PartOfSpeech::Verb);
    if (entry == nullptr) {
      continue;
    }
    // A cell the span itself starts with only counts as a multi-mora
    // imperative; a one-mora classical terminal (ふ) is too weak a witness.
    const bool opens_inside = cell_start < start_pos;
    if ((opens_inside && entry->extended_pos == core::ExtendedPOS::VerbShuushikei) ||
        (entry->extended_pos == core::ExtendedPOS::VerbMeireikei && (opens_inside || end_pos - cell_start >= 2))) {
      return true;
    }
  }
  return false;
}

bool coinedVerbOpensOnArgumentParticle(const dictionary::DictionaryManager* dict_manager,
                                       const grammar::Inflection& inflection, const std::vector<char32_t>& codepoints,
                                       size_t start_pos, size_t end_pos, float own_confidence) {
  constexpr size_t kMinSpan = 4;
  constexpr size_t kMinTopicSpan = 3;
  if (dict_manager == nullptr || start_pos == 0 || end_pos < start_pos + kMinTopicSpan) {
    return false;
  }
  const auto* opener =
      lookupEntryInRange(*dict_manager, codepoints, start_pos, start_pos + 1, core::PartOfSpeech::Particle);
  if (opener == nullptr) {
    return false;
  }
  // The topic は after a case particle that ends its own argument (店に+は+あり)
  // is that particle whenever a registered verb follows; any other opener needs
  // a kanji host and the remainder's own confidence.
  const bool topic_after_case_particle = opener->extended_pos == core::ExtendedPOS::ParticleTopic &&
                                         followsCaseParticle(dict_manager, codepoints, start_pos);
  // A registered adverb hosts the quotative-adverbial と the same way
  // (ゆっくり+と+なぞる, not ゆっくり+となぞる).
  const bool kanji_host_run =
      end_pos >= start_pos + kMinSpan &&
      (normalize::isKanjiCodepoint(codepoints[start_pos - 1]) ||
       hasDictionaryEntryEndingAt(*dict_manager, codepoints, dictionaryLookbehindStart(start_pos), start_pos,
                                  partOfSpeechMask(core::PartOfSpeech::Adverb)));
  if (!topic_after_case_particle && !kanji_host_run) {
    return false;
  }
  float remainder_confidence{};
  for (const auto& analysis : analysesInRange(inflection, codepoints, start_pos + 1, end_pos)) {
    if (topic_after_case_particle && isVerbInDictionary(dict_manager, analysis.base_form)) {
      return true;
    }
    remainder_confidence = std::max(remainder_confidence, analysis.confidence);
  }
  return kanji_host_run && remainder_confidence >= own_confidence;
}

bool opensOnClosedClassWordTail(const dictionary::DictionaryManager* dict_manager,
                                const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  if (dict_manager == nullptr || start_pos == 0 || end_pos < start_pos + 2 || end_pos > codepoints.size()) {
    return false;
  }
  // The closed class holds nothing longer than a handful of morae, so the scan
  // back is bounded rather than running to the start of the sentence.
  constexpr size_t kMaxClosedClassLen = 5;
  const size_t scan_start = lookbehindStart(start_pos, kMaxClosedClassLen - 1);
  // A match opening inside a dictionary word that ends exactly at start_pos
  // (ちゃわ inside めっちゃ|わかる) is no tail: that word closes the left context.
  auto opens_inside_closed_word = [&](size_t word_start) {
    for (size_t left = lookbehindStart(start_pos, kMaxClosedClassLen); left < word_start; ++left) {
      if (lookupEntryInRange(*dict_manager, codepoints, left, start_pos) != nullptr) {
        return true;
      }
    }
    return false;
  };
  auto is_closed_class_word = [&](size_t word_start, size_t word_end) {
    return hasExactPartOfSpeech(
        *dict_manager, codepoints, word_start, word_end,
        partOfSpeechMask(core::PartOfSpeech::Auxiliary) | partOfSpeechMask(core::PartOfSpeech::Particle));
  };
  // The closed word only claims its morae when what it needs comes after it
  // (でしょ+う, でし+た); でし before ら is no copula (駅で+しらべる).
  auto continuation_follows = [&](size_t word_end) {
    constexpr size_t kContinuationProbeChars = 3;
    return hasDictionaryEntryFrom(dict_manager, codepoints, word_end, 1, kContinuationProbeChars,
                                  core::PartOfSpeech::Auxiliary, nullptr) ||
           hasDictionaryEntryFrom(dict_manager, codepoints, word_end, 1, kContinuationProbeChars,
                                  core::PartOfSpeech::Particle, nullptr);
  };
  for (size_t word_start = scan_start; word_start < start_pos; ++word_start) {
    // A span covered by the tail of a closed word and the closed word after it
    // (しょ of でしょ, then う) is those words whatever closes the left context.
    for (size_t word_end = start_pos + 1; word_end <= end_pos && word_end - word_start <= kMaxClosedClassLen;
         ++word_end) {
      if (is_closed_class_word(word_start, word_end) &&
          (word_end == end_pos || is_closed_class_word(word_end, end_pos))) {
        return true;
      }
    }
    if (opens_inside_closed_word(word_start)) {
      continue;
    }
    const size_t max_end = std::min(end_pos - 1, word_start + kMaxClosedClassLen);
    for (size_t word_end = start_pos + 1; word_end <= max_end; ++word_end) {
      if (is_closed_class_word(word_start, word_end) && continuation_follows(word_end)) {
        return true;
      }
    }
  }
  return false;
}

bool hasAuxiliaryNegativeBoundary(const dictionary::DictionaryManager* dict_manager,
                                  const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  if (dict_manager == nullptr || end_pos <= start_pos + 2 || end_pos > codepoints.size()) {
    return false;
  }
  auto has_exact_epos = [&](size_t span_start, size_t span_end, core::ExtendedPOS epos) {
    const auto* entry = lookupEntryInRange(*dict_manager, codepoints, span_start, span_end);
    return entry != nullptr && entry->extended_pos == epos;
  };
  for (size_t boundary = start_pos + 1; boundary + 1 < end_pos; ++boundary) {
    const auto* prefix_entry = lookupEntryInRange(*dict_manager, codepoints, start_pos, boundary);
    const bool is_closed_class_prefix =
        prefix_entry != nullptr && (prefix_entry->pos == core::PartOfSpeech::Auxiliary ||
                                    prefix_entry->extended_pos == core::ExtendedPOS::AuxExcessive);
    if (!is_closed_class_prefix) {
      continue;
    }
    for (size_t negative_end = boundary + 1; negative_end <= end_pos; ++negative_end) {
      if (!has_exact_epos(boundary, negative_end, core::ExtendedPOS::AuxNegativeNai)) {
        continue;
      }
      if (negative_end == end_pos || has_exact_epos(negative_end, end_pos, core::ExtendedPOS::AuxTenseTa)) {
        return true;
      }
    }
  }
  return false;
}

bool opensOnCompleteAuxiliary(const dictionary::DictionaryManager* dict_manager,
                              const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  if (dict_manager == nullptr || end_pos < start_pos + 2 || end_pos > codepoints.size()) {
    return false;
  }
  // An empty lemma is the dictionary's shorthand for "same as the surface",
  // so it marks a base form exactly as an equal lemma does.
  return hasDictionaryEntryFrom(
      dict_manager, codepoints, start_pos, 2, end_pos - start_pos - 1, core::PartOfSpeech::Auxiliary,
      [](const dictionary::DictionaryEntry& entry) { return !entry.lemma.empty() && entry.lemma != entry.surface; });
}

}  // namespace suzume::analysis::verb_helpers
