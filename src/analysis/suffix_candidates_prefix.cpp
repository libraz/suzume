/**
 * @file suffix_candidates_prefix.cpp
 * @brief Suffix-based unknown word candidate generation
 */
#include "analysis/dictionary_probe.h"
#include "candidate_constants.h"
#include "core/debug.h"
#include "core/utf8_constants.h"
#include "dictionary/dictionary.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "grammar/inflection.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "tokenizer_utils.h"
#include "unknown.h"
#include "verb_candidates_helpers.h"

namespace suzume::analysis {

// =============================================================================
// Prefix + Single Kanji Compound Candidates (接頭的複合語)
// =============================================================================

bool isPrefixLikeKanji(char32_t cp) {
  // 本, 全, 各, 両, 諸 are excluded because they require additional context.
  switch (cp) {
    case U'今':  // 今日, 今週, 今月, 今年, 今朝, 今晩, 今夜
    case U'来':  // 来日, 来週, 来月, 来年
    case U'先':  // 先日, 先週, 先月, 先年
    case U'昨':  // 昨日, 昨年
    case U'翌':  // 翌日, 翌週, 翌月, 翌年
    case U'毎':  // 毎日, 毎週, 毎月, 毎年
      return true;
    default:
      return false;
  }
}

bool isInterrogativeKanji(char32_t cp) {
  return cp == U'何' || cp == U'誰' || cp == U'幾';
}

bool isNominalUsePrefix(char32_t cp) {
  return cp == U'再' || cp == U'未' || cp == U'不';
}

void generatePrefixCompoundCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                      const std::vector<normalize::CharType>& char_types,
                                      const grammar::Inflection& inflection,
                                      const dictionary::DictionaryManager* dict_manager,
                                      std::vector<UnknownCandidate>& candidates) {
  // Need at least 2 kanji characters
  if (start_pos + 1 >= codepoints.size()) {
    return;
  }

  // First character must be kanji
  if (start_pos >= char_types.size() || char_types[start_pos] != normalize::CharType::Kanji) {
    return;
  }

  // Check if first character is a prefix-like kanji
  char32_t first_char = codepoints[start_pos];
  // 再・未・不 are closed kango prefixes.  A following single kanji plus
  // nominal 用 is a productive lexical search unit (再利用, 未使用,
  // 不使用), unlike an arbitrary three-kanji sequence.  Restrict the shape
  // to the suffix 用 and an actual word boundary so this cannot absorb a
  // longer compound such as 不安要素.
  if (isNominalUsePrefix(first_char) && start_pos + 2 < codepoints.size() && start_pos + 2 < char_types.size() &&
      char_types[start_pos + 1] == normalize::CharType::Kanji && codepoints[start_pos + 2] == U'用' &&
      (start_pos + 3 >= char_types.size() || char_types[start_pos + 3] != normalize::CharType::Kanji)) {
    std::string surface = extractSubstring(codepoints, start_pos, start_pos + 3);
    candidates.push_back(makeCandidate(surface, start_pos, start_pos + 3, core::PartOfSpeech::Noun,
                                       candidate::kPrefixNominalUseBonus, false, CandidateOrigin::PrefixCompound));
    return;
  }

  if (!isPrefixLikeKanji(first_char)) {
    return;
  }

  // Suppress the compound when it would strand a LONE preceding kanji: that kanji
  // plus this prefix char form a kango noun whose tail this is (将+来, 従+来,
  // 一+昨), so emitting the discounted compound would carve the noun apart
  // (将|来性, 一|昨日). A two-or-more-kanji left context is instead a real
  // preceding word (佐藤+先生, 山田+先生), where the compound correctly splits it
  // off — so only bail when exactly one kanji precedes.
  if (start_pos > 0 && char_types[start_pos - 1] == normalize::CharType::Kanji &&
      (start_pos < 2 || char_types[start_pos - 2] != normalize::CharType::Kanji)) {
    return;
  }

  // Second character must also be kanji
  if (start_pos + 1 >= char_types.size() || char_types[start_pos + 1] != normalize::CharType::Kanji) {
    return;
  }

  // Skip if second character is an interrogative (何, 誰, etc.)
  // These act as anchors and should not form compounds with prefix
  char32_t second_char = codepoints[start_pos + 1];
  if (isInterrogativeKanji(second_char)) {
    return;  // Don't generate compound, let dictionary anchor win
  }

  // A prefix that is itself a standalone noun (今) heads a temporal compound and
  // nothing else, so only a temporal unit or a counter/span suffix continues it
  // (今週, 今回, 今後). Before an ordinary noun it is the free adverbial 今 and
  // the noun is its own word (今|紙, 今|水). The sibling prefixes are bound — 先
  // is a suffix and 来 a verb stem — so their compound stays available and keeps
  // marking the boundary before it (佐藤|先生).
  if (dict_manager != nullptr && !normalize::continuesTemporalNounCompound(first_char, second_char)) {
    const auto* head =
        lookupEntryInRange(*dict_manager, codepoints, start_pos, start_pos + 1, core::PartOfSpeech::Noun);
    if (head != nullptr && head->extended_pos == core::ExtendedPOS::Noun) {
      return;
    }
  }

  // Generate 2-character compound (prefix + single kanji) ONLY when:
  // - Not followed by more kanji, OR
  // - Followed by a temporal-span suffix kanji 中/末, which binds to the
  //   prefix-formed temporal noun (今月|中, 今月|末) rather than extending the
  //   kanji compound.
  // This prevents invalid splits like 翌営|業日 (should be 翌営業日)
  bool followed_by_kanji =
      (start_pos + 2 < char_types.size() && char_types[start_pos + 2] == normalize::CharType::Kanji);
  bool followed_by_span_suffix = (followed_by_kanji && start_pos + 2 < codepoints.size() &&
                                  normalize::isTemporalSpanSuffixKanji(codepoints[start_pos + 2]));

  // Suppress the compound when the second kanji heads a verb that continues into
  // the following hiragana. Temporal-unit kanji (日/週/月/年/朝/晩/夜) are never
  // verb stems, so this only fires for verb-stem kanji: 今食べてる must split as
  // 今|食べ|てる, not 今食|べてる. Probe the second kanji plus a growing hiragana
  // window (食べ → 食べる ichidan) since the full run (食べてる) may include a
  // colloquial aux the inflection analyzer cannot peel.
  if (!followed_by_kanji && start_pos + 2 < char_types.size() &&
      char_types[start_pos + 2] == normalize::CharType::Hiragana) {
    size_t hira_end = start_pos + 2;
    while (hira_end < char_types.size() && char_types[hira_end] == normalize::CharType::Hiragana) {
      ++hira_end;
    }
    for (size_t probe_end = start_pos + 3; probe_end <= hira_end; ++probe_end) {
      std::string verb_probe = extractSubstring(codepoints, start_pos + 1, probe_end);
      grammar::InflectionCandidate best = inflection.getBest(verb_probe);
      if (best.verb_type != grammar::VerbType::Unknown && best.confidence >= candidate::kPrefixCompoundVerbStemConf) {
        return;
      }
    }
  }

  if (!followed_by_kanji || followed_by_span_suffix) {
    std::string surface = extractSubstring(codepoints, start_pos, start_pos + 2);
    if (!surface.empty()) {
      // Strong bonus to prefer compound over split
      // Must beat: single_kanji(1.4+2) + single_kanji(1.4+2) = 6.8
      // And compete with dictionary entries
      auto cand = makeCandidate(surface, start_pos, start_pos + 2, core::PartOfSpeech::Noun, -1.0F, false,
                                CandidateOrigin::PrefixCompound);
#ifdef SUZUME_DEBUG_INFO
      cand.confidence = 0.9F;
      cand.pattern = "prefix_single_kanji";
#endif
      candidates.push_back(cand);
    }
  }

  // Note: N中 compounds (今日中, 一日中, 世界中) are now split per MeCab:
  // 今日中 → 今日 + 中 (noun + suffix)
  // The 中 suffix is registered in L1 dictionary (entries.cpp)
}

void generateTemporalNounBoundaryCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                            const std::vector<normalize::CharType>& char_types,
                                            std::vector<UnknownCandidate>& candidates) {
  // A lexicalized 間もなく begins at the final 間 of a 3+-kanji run
  // (終了|間もなく).  Preserve the noun boundary before it.  Requiring two
  // kanji before 間 excludes ordinary one-kanji compounds followed by
  // も+なく (時間|も|なく), where 間 belongs to the noun on the left.
  size_t kanji_end = start_pos;
  while (kanji_end < char_types.size() && char_types[kanji_end] == normalize::CharType::Kanji) {
    ++kanji_end;
  }
  if (kanji_end >= start_pos + 3 && codepoints[kanji_end - 1] == U'間' && kanji_end + 2 < codepoints.size() &&
      extractSubstring(codepoints, kanji_end - 1, kanji_end + 3) == "間もなく") {
    const size_t noun_end = kanji_end - 1;
    const std::string noun = extractSubstring(codepoints, start_pos, noun_end);
    auto boundary = makeCandidate(noun, start_pos, noun_end, core::PartOfSpeech::Noun,
                                  candidate::kTemporalNounBoundarySplitBonus, false, CandidateOrigin::PrefixCompound);
    boundary.lemma = noun;
#ifdef SUZUME_DEBUG_INFO
    boundary.confidence = candidate::kHighOriginConfidence;
    boundary.pattern = "before_ma_mo_naku";
#endif
    candidates.push_back(std::move(boundary));
    return;
  }

  // A standalone temporal 今 followed by an explicit numeral+counter starts a
  // new quantity phrase (今|一度, 今|三回).  This differs from prefix compounds
  // such as 今回, which have no intervening numeral.  Emit only the temporal
  // head; the counter generator owns the complete quantity on the right.
  if (start_pos + 2 < codepoints.size() && start_pos + 2 < char_types.size() && codepoints[start_pos] == U'今' &&
      normalize::isNumeralCodepoint(codepoints[start_pos + 1]) &&
      normalize::isCounterKanji(codepoints[start_pos + 2])) {
    auto temporal = makeCandidate("今", start_pos, start_pos + 1, core::PartOfSpeech::Noun,
                                  candidate::kTemporalNounBoundarySplitBonus, false, CandidateOrigin::PrefixCompound);
    temporal.lemma = "今";
#ifdef SUZUME_DEBUG_INFO
    temporal.confidence = candidate::kHighOriginConfidence;
    temporal.pattern = "temporal_before_number_counter";
#endif
    candidates.push_back(std::move(temporal));
    return;
  }

  // Need temporal 2-kanji + at least 2 more trailing kanji (gate against lexical
  // 1-kanji suffixes: 現在地/将来性 must stay whole).
  if (start_pos + 3 >= codepoints.size() || start_pos + 3 >= char_types.size()) {
    return;
  }
  for (size_t offset = 0; offset < 4; ++offset) {
    if (char_types[start_pos + offset] != normalize::CharType::Kanji) {
      return;
    }
  }

  const bool has_temporal_reference_suffix =
      codepoints[start_pos + 2] == U'以' && (codepoints[start_pos + 3] == U'来' || codepoints[start_pos + 3] == U'降');
  if (!normalize::isTemporalAdverbialNounPair(codepoints[start_pos], codepoints[start_pos + 1]) &&
      !has_temporal_reference_suffix) {
    return;
  }

  // The 3rd kanji being a span/relation suffix is handled elsewhere (今月|中/末,
  // …後/前) — don't compete there.
  if (normalize::isTemporalSpanSuffixKanji(codepoints[start_pos + 2]) ||
      normalize::isTemporalRelationSuffixKanji(codepoints[start_pos + 2])) {
    return;
  }

  const size_t candidate_end = has_temporal_reference_suffix ? start_pos + 4 : start_pos + 2;
  std::string surface = extractSubstring(codepoints, start_pos, candidate_end);
  if (!surface.empty()) {
    auto cand = makeCandidate(surface, start_pos, candidate_end, core::PartOfSpeech::Noun,
                              candidate::kTemporalNounBoundarySplitBonus, false, CandidateOrigin::PrefixCompound);
#ifdef SUZUME_DEBUG_INFO
    cand.confidence = candidate::kHighOriginConfidence;
    cand.pattern = "temporal_noun_boundary";
#endif
    candidates.push_back(cand);
  }
}

}  // namespace suzume::analysis
