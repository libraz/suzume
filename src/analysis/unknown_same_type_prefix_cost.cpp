/**
 * @file unknown_same_type_prefix_cost.cpp
 * @brief Pricing and lexical shape helpers for same-type prefixes
 */

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

#include "analysis/dictionary_probe.h"
#include "analysis/scorer_constants.h"
#include "analysis/unknown.h"
#include "analysis/unknown_same_type_internal.h"
#include "candidate_constants.h"
#include "grammar/char_patterns.h"
#include "normalize/char_type.h"
#include "suffix_candidates.h"
#include "tokenizer_utils.h"
#include "verb_candidates_auxiliary_patterns.h"

namespace suzume {
namespace analysis {
namespace same_type_detail {

namespace {

bool startsClosedNativeNumber(const std::vector<char32_t>& codepoints, size_t pos) {
  constexpr std::array<std::u32string_view, 10> kClosedNativeNumbers{{U"ひとつ", U"ふたつ", U"みっつ", U"よっつ",
                                                                      U"いつつ", U"むっつ", U"ななつ", U"やっつ",
                                                                      U"ここのつ", U"とおのつ"}};
  return std::any_of(kClosedNativeNumbers.begin(), kClosedNativeNumbers.end(), [&](std::u32string_view word) {
    return pos + word.size() <= codepoints.size() && std::equal(word.begin(), word.end(), codepoints.begin() + pos);
  });
}

// A generated hiragana noun cannot consist solely of two or more closed
// particles. The dynamic program preserves a complete multi-mora particle as
// one grammatical unit while rejecting accidental noun rescue paths such as
// へ+と. Lexical dictionary readings remain separate candidates.
bool decomposesIntoMultipleParticles(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                     const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || end_pos <= start_pos + 1) {
    return false;
  }
  return maximalSegmentCount(*dict_manager, codepoints, start_pos, end_pos, core::PartOfSpeech::Particle) >= 2;
}

bool isFollowedByNominalParticle(const std::vector<char32_t>& codepoints, size_t end_pos,
                                 const dictionary::DictionaryManager* dict_manager) {
  // Longest nominal-selecting particle in the closed class is three codepoints.
  constexpr size_t kParticleProbe = 3;
  return hasDictionaryEntryFrom(
      dict_manager, codepoints, end_pos, 1, kParticleProbe, core::PartOfSpeech::Particle,
      [](const dictionary::DictionaryEntry& entry) { return isNominalForcingParticle(entry.extended_pos); });
}

}  // namespace

PrefixShape classifyPrefixShape(const PrefixScanContext& ctx, size_t len, size_t candidate_end,
                                const std::string& surface) {
  const auto& codepoints = ctx.codepoints;
  const size_t start_pos = ctx.start_pos;
  const normalize::CharType start_type = ctx.start_type;
  const size_t crossed_particle_pos = ctx.crossed_particle_pos;
  const bool crossed_particle_is_bracketed = start_type == normalize::CharType::Hiragana &&
                                             crossed_particle_pos != SIZE_MAX && candidate_end < codepoints.size() &&
                                             isRightBoundaryParticle(codepoints[candidate_end]);
  const bool closes_particle_bracketed_hiragana_noun =
      crossed_particle_is_bracketed && candidate_end == crossed_particle_pos + 1;
  // The same bracket one mora further in. A run that spells a particle
  // character word-internally and is closed by a real particle sits in a
  // nominal slot either way, so it takes the nominal category rather than
  // the opaque one and its length penalty (おとな + に, ひとつ + を).
  const bool brackets_medial_particle_crossing =
      crossed_particle_is_bracketed && !ctx.started_with_particle && candidate_end > crossed_particle_pos + 1;
  // After a past auxiliary, a following hiragana noun beginning with り
  // must remain available as a nominal host. Otherwise the preceding た
  // absorbs its first mora as the listing particle たり (買っ+た+りんご).
  const bool closes_past_tari_collision_noun =
      start_type == normalize::CharType::Hiragana && start_pos > 0 && len >= 2 && codepoints[start_pos - 1] == U'た' &&
      codepoints[start_pos] == U'り' &&
      (candidate_end == codepoints.size() ||
       (candidate_end < codepoints.size() && isRightBoundaryParticle(codepoints[candidate_end])));
  const bool has_verb_tail_after_ri = closes_past_tari_collision_noun && ctx.dict_manager != nullptr &&
                                      lookupEntryInRange(*ctx.dict_manager, codepoints, start_pos + 1, candidate_end,
                                                         core::PartOfSpeech::Verb) != nullptr;
  const bool has_inflected_verb_reading =
      closes_past_tari_collision_noun && std::any_of(ctx.inflection.analyze(surface).begin(),
                                                     ctx.inflection.analyze(surface).end(), [](const auto& analysis) {
                                                       return analysis.verb_type != grammar::VerbType::Unknown &&
                                                              analysis.verb_type != grammar::VerbType::IAdjective &&
                                                              !analysis.morphemes.empty() &&
                                                              analysis.confidence > candidate::kNoOriginConfidence;
                                                     });
  const bool has_contracted_progressive_tail =
      closes_past_tari_collision_noun && len >= 2 &&
      grammar::isContractedProgressiveSurface(extractSubstring(codepoints, candidate_end - 2, candidate_end));
  const bool selects_past_tari_collision_noun = closes_past_tari_collision_noun && !has_verb_tail_after_ri &&
                                                !has_inflected_verb_reading && !has_contracted_progressive_tail;
  const bool precedes_closed_native_number =
      start_type == normalize::CharType::Hiragana && startsClosedNativeNumber(codepoints, candidate_end);
  // What licenses the nominal reading is the genitive の with the negative
  // predicate behind it, so the negative has to be resolved from its own
  // paradigm rather than from its opening mora. That mora also opens the
  // conditional なら, which follows the nominalizer instead and leaves a
  // predicate in front of it — reading the run as a nominal there cuts an
  // ordinary verb in half (食べる|の|なら, not 食|べる|の|なら).
  const bool closes_genitive_negative_noun =
      start_type == normalize::CharType::Hiragana && candidate_end - start_pos >= 2 &&
      !ctx.starts_at_dictionary_verb_continuative && !ctx.starts_after_dictionary_adjective &&
      candidate_end + 1 < codepoints.size() && codepoints[candidate_end] == U'の' &&
      verb_helpers::naiNegativeFollowsAt(codepoints, candidate_end + 1);
  return {closes_particle_bracketed_hiragana_noun, brackets_medial_particle_crossing, selects_past_tari_collision_noun,
          precedes_closed_native_number, closes_genitive_negative_noun};
}

// Prices a kanji run against the words it would absorb; false when no run of
// this length is offered.
bool priceKanjiRun(const PrefixScanContext& ctx, size_t len, size_t candidate_end, float& cost) {
  const auto& codepoints = ctx.codepoints;
  const size_t start_pos = ctx.start_pos;
  const normalize::CharType start_type = ctx.start_type;
  // Penalize kanji sequences ending with honorific/title suffixes (様, 氏)
  // to encourage NOUN + SUFFIX separation (e.g., 田中様 → 田中 + 様)
  // 的 is not listed: the plain kanji-run cost already keeps 目的/知的 whole
  // while 論理+的 splits.
  // The host length carries no information here, unlike for the plural
  // honorific 方 below: 様 attaches to a bare surname, and a surname is an
  // open class that is routinely one kanji (辻様, 林様, 森様), so the
  // penalty has to reach a two-kanji run. The lexicalized compounds that
  // share the shape (異様, 同様, 王様, 神様, 殿様, 奥様) are a closed set and
  // are held in the dictionary instead.
  // 君 and 殿 are the same titles, but each also closes many two-kanji words
  // (主君, 暴君, 宮殿, 沈殿) that are not a closed set, so only a host of two
  // or more kanji is split off (佐藤君, 先生殿).
  if (start_type == normalize::CharType::Kanji && len >= 2) {
    char32_t last_char = codepoints[candidate_end - 1];
    const bool closes_title =
        last_char == U'様' || last_char == U'氏' || (len >= 3 && (last_char == U'君' || last_char == U'殿'));
    if (closes_title) {
      cost += 4.0F;  // Strong penalty to prefer NOUN + SUFFIX path
    }
  }

  // Penalize kanji sequences starting with the prefix kanji 御.
  // 御 is an L1 PREFIX entry and should split off as a productive prefix
  // (御 + 尽力, 御 + 挨拶, 御 + 協力). The +2.0 penalty makes the PREFIX path
  // win over any 2+ char kanji_seq starting with 御. Lexicalized 御-X nouns
  // (御者, 御所, 御曹司) come from the dictionary and get a separate bonus
  // in scorer.cpp to beat the prefix path.
  if (start_type == normalize::CharType::Kanji && len >= 2 && codepoints[start_pos] == U'御') {
    cost += 2.0F;
  }

  // Skip kanji sequences starting with iteration mark (々)
  // 々 always attaches to the preceding kanji (人々, 時々)
  // It can never start a word
  if (start_type == normalize::CharType::Kanji && normalize::isIterationMark(codepoints[start_pos])) {
    return false;
  }

  // Penalize kanji sequences that extend past iteration mark (々), except
  // for a complete double-reduplication X々Y々 (津々浦々, 様々). The latter
  // is a productive four-character compound shape and must retain its
  // whole-word candidate; a lone completed pair followed by another kanji
  // (時々妙) is still a boundary.
  if (start_type == normalize::CharType::Kanji && len >= 3) {
    bool is_double_reduplication = len == 4 && normalize::isIterationMark(codepoints[start_pos + 1]) &&
                                   normalize::isIterationMark(codepoints[start_pos + 3]);
    for (size_t i = start_pos + 1; i < candidate_end - 1; ++i) {
      if (!is_double_reduplication && normalize::isIterationMark(codepoints[i])) {
        // Found 々 in the middle - penalize extending past it
        cost += 5.0F;
        break;
      }
    }
  }

  // Penalize kanji sequences with interrogative kanji (何, 誰, 幾) at NON-initial position
  // e.g., 今何 should be split as 今 + 何, not kept as one compound
  // But 何日, 何人 (interrogative + counter) should stay together
  // Interrogatives are standalone words unless they're at the start (counter pattern)
  if (start_type == normalize::CharType::Kanji && len >= 2) {
    for (size_t i = start_pos + 1; i < candidate_end; ++i) {  // Skip first char
      if (isInterrogativeKanji(codepoints[i])) {
        // Heavy penalty to force split
        cost += 3.0F;
        break;
      }
    }
  }

  // A temporal prefix that is itself a standalone noun (今) heads a temporal
  // compound, never an arbitrary one: 今週/今回/今後 continue it, an ordinary
  // noun does not (今|紙, 今|本, 今|大会). Without this the generic run cost
  // (shorter than the sum of its parts) glues the adverbial 今 to whatever
  // object follows. The sibling prefixes are not standalone nouns — 先 is a
  // suffix and 来 a verb stem — so 先方/来客 keep their run.
  if (start_type == normalize::CharType::Kanji && len >= 2 && ctx.dict_manager != nullptr &&
      isPrefixLikeKanji(codepoints[start_pos]) &&
      !normalize::continuesTemporalNounCompound(codepoints[start_pos], codepoints[start_pos + 1]) &&
      !(len == 2 && normalize::isStyleSuffixKanji(codepoints[start_pos + 1]) &&
        (candidate_end >= codepoints.size() || ctx.char_types[candidate_end] != normalize::CharType::Kanji)) &&
      (start_pos == 0 || ctx.char_types[start_pos - 1] != normalize::CharType::Kanji)) {
    const auto* head =
        lookupEntryInRange(*ctx.dict_manager, codepoints, start_pos, start_pos + 1, core::PartOfSpeech::Noun);
    if (head != nullptr && head->extended_pos == core::ExtendedPOS::Noun) {
      return false;
    }
  }
  return true;
}

// Prices a hiragana run that spells a particle; false when no run of this
// length is offered.
bool priceHiraganaRun(const PrefixScanContext& ctx, size_t len, size_t candidate_end, const PrefixShape& shape,
                      float& cost) {
  const auto& codepoints = ctx.codepoints;
  const auto* dict_manager = ctx.dict_manager;
  const size_t crossed_particle_pos = ctx.crossed_particle_pos;
  // Penalize hiragana candidates that include an internal particle
  // character (see scan loop above). The penalty keeps particle splits
  // preferred when the prefix is a plausible word (ここ+で beats ここで),
  // while letting genuine nouns spanning a particle char (こども, ひとつ)
  // win when the split leaves an implausible fragment (こど+も, ひ+と+つ).
  // - 2-char candidates (single char + particle char) are skipped unless
  //   another nominal particle closes the run; that bracket proves the
  //   particle-like mora is word-internal (ひも+を)
  // - Particle-final candidates (こども) get a minor penalty
  // - Medial crossing (one char after the particle, e.g. ひとつ) is less
  //   plausible and gets a strong penalty; genuine words still win
  //   because their split path needs multiple unknown 1-char fragments
  if (ctx.start_type == normalize::CharType::Hiragana && !ctx.started_with_particle &&
      candidate_end > crossed_particle_pos) {
    if (len < 3 && !shape.closes_particle_bracketed_hiragana_noun) {
      return false;
    }
    // The bracket above is also what separates a native noun from a real
    // particle boundary here: without it the strong penalty made the noun
    // lose to a chain of one-character fragments whose own reading strands
    // the run's head (おとな|に|なる, not お|と|なに|なる).
    cost += (candidate_end > crossed_particle_pos + 1 && !shape.brackets_medial_particle_crossing)
                ? scorer::scale::kStrong
                : scorer::scale::kMinor;
    // The mora allowed past the crossed particle exists for native nouns
    // that spell a particle word-internally (こども, ひとつ). It must not be
    // taken from the front of a bound word instead: an auxiliary or a
    // particle attaches leftward, so its own left edge is a fixed boundary
    // and the run has to stop there (見|る|が|ごとく, not 見|るがご|とく).
    // An unbound word heading the same position proves nothing, since a
    // native noun may simply spell it (おとな + になる, not お + と + なに).
    constexpr size_t kStraddledWordProbe = 4;
    if (candidate_end > crossed_particle_pos + 1 && dict_manager != nullptr &&
        hasDictionaryEntryFrom(dict_manager, codepoints, candidate_end - 1, 2, kStraddledWordProbe,
                               core::PartOfSpeech::Unknown, [](const dictionary::DictionaryEntry& entry) {
                                 return entry.pos == core::PartOfSpeech::Auxiliary ||
                                        entry.pos == core::PartOfSpeech::Particle;
                               })) {
      return false;
    }
  }

  // The conditional particle ば cannot close an open nominal run. A listed
  // noun such as ことば retains its dictionary edge, while an unverified
  // hiragana fallback must leave the particle available at the boundary.
  if (ctx.start_type == normalize::CharType::Hiragana && !ctx.started_with_particle && dict_manager != nullptr &&
      candidate_end > ctx.start_pos + 1) {
    const auto* final_particle =
        lookupEntryInRange(*dict_manager, codepoints, candidate_end - 1, candidate_end, core::PartOfSpeech::Particle);
    if (final_particle != nullptr && final_particle->extended_pos == core::ExtendedPOS::ParticleConj &&
        codepoints[candidate_end - 1] == U'ば') {
      return false;
    }
  }
  return true;
}

void appendInterjectionNominalCandidate(const PrefixScanContext& ctx, size_t len, size_t candidate_end,
                                        const std::string& surface, std::vector<UnknownCandidate>& candidates) {
  const auto& codepoints = ctx.codepoints;
  const size_t start_pos = ctx.start_pos;
  const auto* dict_manager = ctx.dict_manager;
  // A closed interjection cannot host the nominalizing suffix さ. When
  // that shape occurs, retain a complete unknown noun candidate as the
  // only grammatical lexical reading (うわ+さ -> うわさ), rather than
  // letting the suffix expose the interjection as an independent token.
  if (ctx.start_type == normalize::CharType::Hiragana && !ctx.started_with_particle && len >= 3 &&
      codepoints[candidate_end - 1] == U'さ' && dict_manager != nullptr) {
    const bool follows_prefix = start_pos > 0 && lookupEntryInRange(*dict_manager, codepoints, start_pos - 1, start_pos,
                                                                    core::PartOfSpeech::Prefix) != nullptr;
    const auto* interjection =
        lookupEntryInRange(*dict_manager, codepoints, start_pos, candidate_end - 1, core::PartOfSpeech::Interjection);
    if (interjection != nullptr && !follows_prefix) {
      auto noun_candidate = makeCandidate(surface, start_pos, candidate_end, core::PartOfSpeech::Noun,
                                          candidate::kSelectedNominalShortHeadCost,
                                          /*has_suffix=*/true, CandidateOrigin::SameType);
#ifdef SUZUME_DEBUG_INFO
      noun_candidate.pattern = "interjection_nominal_boundary";
#endif
      candidates.push_back(std::move(noun_candidate));
    }
  }
}

// Penalize hiragana sequences starting with particle characters
// These could be nouns (はし, はな, にく, にゃんこ) but are less likely than
// the particle interpretation, unless the particle path has connection penalties
bool priceParticleStartedRun(const PrefixScanContext& ctx, size_t len, size_t candidate_end, float& cost,
                             bool& has_suffix) {
  const auto& codepoints = ctx.codepoints;
  const size_t start_pos = ctx.start_pos;
  if (ctx.started_with_particle) {
    if (len == 1) {
      return false;  // Single-char particle-start never forms a noun alone
    }
    // Check if this is a reduplicated pattern (same character repeated)
    // Reduplicated hiragana like はは (母), ちち (父) are likely real words
    bool is_reduplicated = (len == 2 && codepoints[start_pos] == codepoints[start_pos + 1]);
    if (is_reduplicated) {
      // Small bonus for reduplicated patterns - they're often real words
      cost -= 0.5F;
    } else if (len == 2) {
      // 2-char: light penalty — bigram penalties on unnatural particle chains
      // provide enough discouragement for false splits (は+し vs はし)
      cost += 0.5F;
    } else if (len == 3) {
      // 3-char: moderate penalty (にある, によれ are likely particle chains)
      cost += 0.8F;
    } else {
      // 4+ char: heavier penalty scaling with length
      // but still generated so words like にゃんこ have a chance
      cost += 1.0F + static_cast<float>(len - 3) * 0.5F;
    }
    // Mark as has_suffix to skip exceeds_dict_length penalty in tokenizer
    has_suffix = true;
  }
  if (ctx.started_with_particle && !isFollowedByNominalParticle(codepoints, candidate_end, ctx.dict_manager) &&
      decomposesIntoMultipleParticles(codepoints, start_pos, candidate_end, ctx.dict_manager)) {
    return false;
  }
  return true;
}

}  // namespace same_type_detail

}  // namespace analysis
}  // namespace suzume
