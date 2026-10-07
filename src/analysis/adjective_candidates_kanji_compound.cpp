/**
 * @file adjective_candidates_kanji_compound.cpp
 * @brief Compound i-adjective candidates for kanji stems
 */

#include <algorithm>
#include <string>
#include <utility>

#include "adjective_candidates_internal.h"
#include "analysis/candidate_constants.h"
#include "analysis/dictionary_probe.h"
#include "core/debug.h"
#include "dictionary/source_parser.h"
#include "grammar/dictionary_expansion.h"
#include "grammar/inflection.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "tokenizer_utils.h"
#include "unknown.h"
#include "verb_candidates_absorption_guards.h"
#include "verb_candidates_auxiliary_patterns.h"
#include "verb_candidates_dictionary_probes.h"

namespace suzume::analysis {

namespace {

void appendLexicalCompoundIAdj(std::vector<UnknownCandidate>& candidates, const std::string& surface, size_t start_pos,
                               size_t end_pos, const std::string& lemma, float confidence, const char* pattern) {
  auto adjective =
      adj_detail::makeIAdjCandidate(surface, start_pos, end_pos, lemma, adj_detail::lexicalCompoundIAdjCost(confidence),
                                    CandidateOrigin::AdjectiveI, confidence, pattern);
  adjective.has_suffix = true;
  candidates.push_back(std::move(adjective));
}

// A duration/formal-noun kanji may begin a compound adjective only when its
// tail is independently an i-adjective, never merely a Godan continuative.
bool hasValidDurationCompoundTail(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                  char32_t first_hira, const grammar::Inflection& inflection,
                                  const dictionary::DictionaryManager* dict_manager) {
  const std::string tail_adj = extractSubstring(codepoints, start_pos + 1, kanji_end) + "い";
  const bool tail_is_dict_adj = verb_helpers::isAdjectiveInDictionary(dict_manager, tail_adj);
  bool tail_is_i_adj = tail_is_dict_adj;
  float tail_adj_confidence = candidate::kNoOriginConfidence;
  if (!tail_is_i_adj && !(first_hira == U'い' && adj_detail::isVerbOnbinContextAfterI(codepoints, kanji_end + 1)) &&
      !verb_helpers::isNounInDictionary(dict_manager, tail_adj) &&
      !verb_helpers::hasDictionaryEntry(dict_manager, tail_adj, core::PartOfSpeech::Verb)) {
    for (const auto& tail_res : inflection.analyze(tail_adj)) {
      if (tail_res.verb_type == grammar::VerbType::IAdjective &&
          tail_res.confidence >= candidate::kCompoundAdjConfMin) {
        tail_is_i_adj = true;
        tail_adj_confidence = std::max(tail_adj_confidence, tail_res.confidence);
      }
    }
  }
  if (tail_is_i_adj && !tail_is_dict_adj) {
    const std::string tail_surface = extractSubstring(codepoints, start_pos + 1, kanji_end + 1);
    for (const auto& tail_res : inflection.analyze(tail_surface)) {
      if (grammar::isGodanVerbType(tail_res.verb_type) && tail_res.base_form == tail_surface &&
          tail_res.confidence > tail_adj_confidence) {
        return false;
      }
    }
  }
  return tail_is_i_adj;
}

bool isIndependentPredicateTail(const std::string& tail_surface, const grammar::Inflection& inflection,
                                const dictionary::DictionaryManager* dict_manager) {
  bool has_adjective_evidence = false;
  bool has_complete_verb_evidence = false;
  for (const auto& analysis : inflection.analyze(tail_surface)) {
    if (analysis.verb_type == grammar::VerbType::IAdjective) {
      has_adjective_evidence = has_adjective_evidence ||
                               verb_helpers::isAdjectiveInDictionary(dict_manager, analysis.base_form) ||
                               adj_detail::isCompoundFormingAdjective(analysis.base_form);
      continue;
    }
    const bool complete_unknown_predicate =
        analysis.base_form == tail_surface && analysis.confidence >= candidate::kIAdjConfMin;
    has_complete_verb_evidence = has_complete_verb_evidence || complete_unknown_predicate ||
                                 verb_helpers::isVerbInDictionary(dict_manager, analysis.base_form);
  }
  return has_complete_verb_evidence && !has_adjective_evidence;
}

void appendRenyokeiHostCompound(const std::vector<char32_t>& codepoints, size_t start_pos, size_t hiragana_end,
                                const grammar::Inflection& inflection,
                                const dictionary::DictionaryManager* dict_manager,
                                std::vector<UnknownCandidate>& candidates) {
  const bool has_renyokei_host =
      std::any_of(candidates.begin(), candidates.end(), [&](const UnknownCandidate& candidate) {
        return candidate.start == start_pos && candidate.end == hiragana_end &&
               candidate.pos == core::PartOfSpeech::Verb && candidate.extended_pos == core::ExtendedPOS::VerbRenyokei;
      });
  if (!has_renyokei_host || hiragana_end >= codepoints.size() ||
      !normalize::isKanjiCodepoint(codepoints[hiragana_end])) {
    return;
  }

  constexpr size_t kMaxSecondElementKanjiLength = 2;
  size_t tail_kanji_end = hiragana_end;
  while (tail_kanji_end < codepoints.size() && tail_kanji_end - hiragana_end < kMaxSecondElementKanjiLength &&
         normalize::isKanjiCodepoint(codepoints[tail_kanji_end])) {
    ++tail_kanji_end;
  }
  constexpr size_t kMaxSecondElementInflectionLength = 5;
  size_t tail_end = tail_kanji_end;
  while (tail_end < codepoints.size() && tail_end - tail_kanji_end < kMaxSecondElementInflectionLength &&
         normalize::classifyChar(codepoints[tail_end]) == normalize::CharType::Hiragana) {
    ++tail_end;
  }

  for (size_t end_pos = tail_end; end_pos > tail_kanji_end; --end_pos) {
    for (const auto& analysis : analysesInRange(inflection, codepoints, hiragana_end, end_pos)) {
      if (analysis.verb_type != grammar::VerbType::IAdjective || analysis.confidence < candidate::kCompoundAdjConfMin ||
          !adj_detail::isCompoundFormingAdjective(analysis.base_form)) {
        continue;
      }
      const std::string surface = extractSubstring(codepoints, start_pos, end_pos);
      if (verb_helpers::isCompoundAdjectivePattern(surface)) {
        continue;
      }
      const std::string lemma = extractSubstring(codepoints, start_pos, hiragana_end) + analysis.base_form;
      if (adj_detail::isLexicalCompoundHeadAdjective(analysis.base_form) &&
          !verb_helpers::isAdjectiveInDictionary(dict_manager, lemma)) {
        continue;
      }
      appendLexicalCompoundIAdj(candidates, surface, start_pos, end_pos, lemma, analysis.confidence,
                                "renyokei_host_compound");
      return;
    }
  }
}

// Compound adjective: set has_suffix on existing kanji-stem ADJ candidates
// to skip exceeds_dict_length penalty in tokenizer (薄暗い, 物悲しく, etc.)
// Guards prevent false positives on suru-verb patterns (遅刻しそう, 確認して):
//  1. First hiragana must be valid i-adj inflection char (い,く,け,か,し)
//  2. Hiragana portion must be short (≤5 chars)
void appendKanjiStemCompoundIAdjCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                           size_t hiragana_end, const grammar::Inflection& inflection,
                                           const dictionary::DictionaryManager* dict_manager,
                                           std::vector<UnknownCandidate>& candidates, size_t candidate_start) {
  if (kanji_end <= start_pos + 1 || kanji_end >= codepoints.size()) {
    return;
  }
  const char32_t first_hira = codepoints[kanji_end];
  // A period/duration formal-noun suffix (間/分/秒/中) must not head an
  // i-adjective compound stem: "3分間続いた" would split as 3分 + 間続い(fake
  // ADJ) instead of 3分間 + 続い(verb), and "長い間続いた" likewise severs 間.
  // Allow only when the second kanji itself forms a genuine i-adjective
  // (間近い → 近い, 分厚い → 厚い), otherwise the compound is masking a verb
  // renyokei (間続い ← 続く). Common tail adjectives are open-class and
  // rule-derived, so a dictionary hit alone is too narrow: accept the tail
  // by rule when it is not a dictionary noun/verb form itself (勢い, 洗い,
  // 違い are nominalizations, not adjectives), inflection recognizes
  // kanji+い as an i-adjective, and the compound's い is not a verb-onbin
  // surface (間続いた, 分置いて).
  const char32_t head_char = codepoints[start_pos];
  if (normalize::isDurationSuffixKanji(head_char) &&
      !hasValidDurationCompoundTail(codepoints, start_pos, kanji_end, first_hira, inflection, dict_manager)) {
    SUZUME_DEBUG_LOG_VERBOSE("[ADJ_SKIP] duration-suffix head \"" << head_char << "\" not an i-adj compound\n");
    return;
  }
  // For し: must be followed by い/く/け/か (しい-adj conjugation),
  // NOT そ/な/て/た (suru verb + auxiliary)
  bool valid_adj_start = (first_hira == U'い' || first_hira == U'く' || first_hira == U'け' || first_hira == U'か');
  if (first_hira == U'し' && kanji_end + 1 < codepoints.size()) {
    const char32_t second_hira = codepoints[kanji_end + 1];
    valid_adj_start = (second_hira == U'い' || second_hira == U'く' || second_hira == U'け' || second_hira == U'か');
  }
  if (!valid_adj_start) {
    return;
  }
  constexpr size_t kMaxHiraganaLen = 5;
  // Mark this generator's candidates with has_suffix if they fit the
  // compound pattern. Only the [candidate_start, size) sub-range belongs
  // to this generator; earlier generators may have shared the buffer.
  for (size_t idx = candidate_start; idx < candidates.size(); ++idx) {
    UnknownCandidate& cand = candidates[idx];
    if (cand.pos == core::PartOfSpeech::Adjective && cand.end - kanji_end <= kMaxHiraganaLen) {
      cand.has_suffix = true;
      cand.cost += candidate::kCompoundIAdjectiveLexicalBonus;
    }
  }
  // Generate new compound candidate if this generator's main loop did not
  // produce one. The 2-kanji penalty drops inflection confidence below the
  // main loop's 0.5 threshold for compound adjectives like 薄暗い, 物悲しく.
  // Use tighter hiragana limits for い/く/か/け (max 2).
  if (candidates.size() != candidate_start) {
    return;
  }
  // くさい is itself a productive adjective-forming second element,
  // so its past and conditional cells need the same scan width as the
  // しい family (面倒くさかっ+た).  Other く heads remain at the short
  // limit to avoid swallowing an ordinary kanji-verb continuative.
  const bool kusai_derivation =
      first_hira == U'く' && kanji_end + 1 < codepoints.size() && codepoints[kanji_end + 1] == U'さ';
  const size_t hira_limit = (first_hira == U'し' || kusai_derivation) ? kMaxHiraganaLen : 2;
  const size_t max_end = std::min(hiragana_end, kanji_end + hira_limit);
  for (size_t end_pos = max_end; end_pos > kanji_end; --end_pos) {
    const std::string surface = extractSubstring(codepoints, start_pos, end_pos);
    // A terminal い closes its predicate: the te/ta series attaches to a
    // stem, never to a closed cell, so an adjective never stands in front
    // of て/た. The same kana is the Godan ka/ga euphonic stem, and that
    // is what it is here (置い+て ← 置く). The single-kanji scan already
    // reads the context this way; the compound scan needs it because its
    // own verb-tail guard has to reconstruct a lemma, and the lemma of an
    // onbin stem alone is unrecoverable (置い analyzes as the continuative
    // of the non-word 置う, so the guard only fires for the verbs L2
    // happens to carry).
    // @see fabricated closed-class absorption guards (verb_candidates_absorption_guards.h)
    if (codepoints[end_pos - 1] == U'い' && adj_detail::isVerbOnbinContextAfterI(codepoints, end_pos)) {
      SUZUME_DEBUG_LOG_VERBOSE("[ADJ_SKIP] \"" << surface << "\" い is a verb onbin stem here\n");
      continue;
    }
    // A focus particle is not an adjective conjugation: noun + しか(…ない)
    // shares its kana with the しい-adjective paradigm.
    // @see fabricated closed-class absorption guards (verb_candidates_absorption_guards.h)
    if (!verb_helpers::isAdjectiveInDictionary(dict_manager, surface) &&
        verb_helpers::guardIsWired(verb_helpers::GuardMember::FocusParticleHead,
                                   verb_helpers::GuardOrigin::KanjiCompoundAdjective) &&
        verb_helpers::startsWithFocusParticleHead(dict_manager, codepoints, kanji_end, end_pos)) {
      SUZUME_DEBUG_LOG_VERBOSE("[ADJ_SKIP] \"" << surface << "\" hiragana head is a focus particle\n");
      continue;
    }
    // The compound stem must not be masking a verb: when its final
    // kanji plus the hiragana tail is itself a dictionary verb, the
    // sequence is a noun followed by that verb with the case particle
    // omitted (紙書く → 紙 + 書く), not a fabricated i-adjective.
    // A genuine compound adjective's tail is adjectival (力強く,
    // 薄暗く), so this leaves it alone.
    // @see fabricated closed-class absorption guards (verb_candidates_absorption_guards.h)
    // The whole span spelling a dictionary verb cell (手伝い ← 手伝う) is that
    // verb as well.
    const std::string tail_surface = extractSubstring(codepoints, kanji_end - 1, end_pos);
    if ((dict_manager != nullptr && (dict_manager->lookupExact(tail_surface, core::PartOfSpeech::Verb) != nullptr ||
                                     dict_manager->lookupExact(surface, core::PartOfSpeech::Verb) != nullptr)) ||
        isIndependentPredicateTail(tail_surface, inflection, dict_manager)) {
      SUZUME_DEBUG_LOG_VERBOSE("[ADJ_SKIP] \"" << surface << "\" tail \"" << tail_surface << "\" is a verb\n");
      continue;
    }
    // The same holds for an adjectival tail: when the final kanji plus
    // the hiragana is itself a complete dictionary i-adjective, the
    // sequence is a noun and the adjective predicating over it (本 重い,
    // 紙 薄い) — the default reading, which needs no compound invented
    // for it. A lexicalized compound (力強い, 薄暗い) is a lexical fact
    // rather than a derivable one, so the dictionary carries it.
    // @see fabricated closed-class absorption guards (verb_candidates_absorption_guards.h)
    std::string productive_tail_base;
    float productive_tail_confidence = candidate::kNoOriginConfidence;
    bool tail_is_independent_adjective = false;
    for (const auto& tail_res : inflection.analyze(tail_surface)) {
      if (tail_res.verb_type != grammar::VerbType::IAdjective) {
        continue;
      }
      if (adj_detail::isCompoundFormingAdjective(tail_res.base_form) && productive_tail_base.empty()) {
        productive_tail_base = tail_res.base_form;
        productive_tail_confidence = tail_res.confidence;
      }
      if (verb_helpers::isAdjectiveInDictionary(dict_manager, tail_res.base_form)) {
        tail_is_independent_adjective = true;
      }
    }
    // A derivational tail that is also a headword in its own right
    // (深い, 強い) only derives from a host the grammar can point at —
    // a continuative, not a bare kanji run. Over a bare run the default
    // reading is the noun and the adjective predicating over it
    // (谷 深く, 知識 深く), and the lexicalized compounds that contradict
    // it are lexical facts the dictionary carries, the same way it
    // already carries 名高い and 薄暗い for the tails not on the list.
    if (tail_is_independent_adjective) {
      SUZUME_DEBUG_LOG_VERBOSE("[ADJ_SKIP] \"" << surface << "\" tail \"" << tail_surface
                                               << "\" is a dictionary adjective\n");
      continue;
    }
    for (const auto& ic : inflection.analyze(surface)) {
      if (ic.confidence >= candidate::kCompoundAdjConfMin && ic.verb_type == grammar::VerbType::IAdjective) {
        float cost = candidate::confidenceScaledCost(candidate::kCompoundAdjBaseCost, ic.confidence,
                                                     candidate::kKanjiAdjConfScale);
        if (!productive_tail_base.empty()) {
          cost += candidate::kCompoundIAdjectiveLexicalBonus;
        }
        SUZUME_DEBUG_LOG_VERBOSE("[ADJ_COMPOUND] \"" << surface << "\" cost=" << cost << " conf=" << ic.confidence
                                                     << "\n");
        auto adj_cand =
            adj_detail::makeIAdjCandidate(surface, start_pos, end_pos, ic.base_form, cost, CandidateOrigin::AdjectiveI,
                                          ic.confidence, "i_adjective_compound");
        adj_cand.has_suffix = true;
        candidates.push_back(std::move(adj_cand));
        return;
      }
    }
    if (!productive_tail_base.empty()) {
      const std::string lemma = extractSubstring(codepoints, start_pos, kanji_end - 1) + productive_tail_base;
      appendLexicalCompoundIAdj(candidates, surface, start_pos, end_pos, lemma, productive_tail_confidence,
                                "productive_second_element_compound");
      return;
    }
  }
}

}  // namespace

void adj_detail::appendKanjiCompoundIAdjCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                                   size_t kanji_end, size_t hiragana_end,
                                                   const grammar::Inflection& inflection,
                                                   const dictionary::DictionaryManager* dict_manager,
                                                   std::vector<UnknownCandidate>& candidates, size_t candidate_start) {
  appendKanjiStemCompoundIAdjCandidates(codepoints, start_pos, kanji_end, hiragana_end, inflection, dict_manager,
                                        candidates, candidate_start);

  // A verb continuative may contain okurigana before the adjective-forming
  // second element (粘り+強い). The ordinary kanji-adjective scan stops at that
  // intervening kanji, so bridge it only when another generator has already
  // established the entire host as a 連用形.
  appendRenyokeiHostCompound(codepoints, start_pos, hiragana_end, inflection, dict_manager, candidates);
}

// A nominal or continuative host plus a host-fused suffix adjective (汗+臭い,
// 紙+くさい, 照れ+くさい) is one adjective, and every cell the suffix inflects
// to belongs to it (汗臭かっ+た, 照れくさく+て). The host is a kanji run that is
// not a listed non-noun (an adverb in front predicates instead: 全然+くさく), or
// a continuative another generator has already proposed.
void adj_detail::appendHostFusedSuffixAdjective(const std::vector<char32_t>& codepoints, size_t start_pos,
                                                const dictionary::DictionaryManager* dict_manager,
                                                std::vector<UnknownCandidate>& candidates) {
  constexpr size_t kMaxHostLength = 6;
  constexpr size_t kMaxTailLength = 6;
  std::vector<size_t> host_ends;
  for (size_t pos = start_pos + 1;
       pos < codepoints.size() && pos - start_pos <= kMaxHostLength && normalize::isKanjiCodepoint(codepoints[pos - 1]);
       ++pos) {
    const std::string host = extractSubstring(codepoints, start_pos, pos);
    bool listed_non_noun = false;
    if (dict_manager != nullptr) {
      for (const auto& match : dict_manager->lookup(host, 0)) {
        listed_non_noun = listed_non_noun || (match.entry != nullptr && match.length == pos - start_pos &&
                                              match.entry->pos != core::PartOfSpeech::Noun);
      }
    }
    if (!listed_non_noun) {
      host_ends.push_back(pos);
    }
  }
  for (const auto& candidate : candidates) {
    if (candidate.start == start_pos && candidate.pos == core::PartOfSpeech::Verb &&
        candidate.extended_pos == core::ExtendedPOS::VerbRenyokei) {
      host_ends.push_back(candidate.end);
    }
  }
  // The suffix's cells are the dictionary's expansion of it (臭かっ, くさく).
  const auto emit = [&](size_t host_end, size_t end_pos, const std::string& suffix_lemma, core::ExtendedPOS epos) {
    auto adjective = adj_detail::makeIAdjCellCandidate(
        extractSubstring(codepoints, start_pos, end_pos), start_pos, end_pos,
        extractSubstring(codepoints, start_pos, host_end) + suffix_lemma, epos,
        candidate::kCompoundAdjBaseCost + candidate::kCompoundIAdjectiveLexicalBonus, CandidateOrigin::AdjectiveI,
        candidate::kDictFallbackAdjConfidence, "host_fused_suffix_adjective");
    adjective.has_suffix = true;
    candidates.push_back(std::move(adjective));
  };
  for (const size_t host_end : host_ends) {
    const size_t tail_limit = std::min(codepoints.size(), host_end + kMaxTailLength);
    if (dict_manager != nullptr) {
      for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, host_end, tail_limit)) {
        const auto* entry = match.entry;
        if (entry != nullptr && entry->pos == core::PartOfSpeech::Adjective &&
            adj_detail::isHostFusedSuffixAdjective(entry->lemma.empty() ? entry->surface : entry->lemma)) {
          emit(host_end, host_end + match.length, entry->lemma.empty() ? entry->surface : entry->lemma,
               entry->extended_pos);
        }
      }
    }
    // A spelling the dictionary does not list is expanded the way the
    // dictionary expands a listed one, so both spellings get the same cells.
    for (const std::string_view suffix : adj_detail::kHostFusedSuffixAdjectives) {
      if (verb_helpers::isAdjectiveInDictionary(dict_manager, suffix)) {
        continue;
      }
      dictionary::SourceEntry source;
      source.surface = std::string(suffix);
      source.pos = core::PartOfSpeech::Adjective;
      source.conj_type = dictionary::ConjugationType::IAdjective;
      for (const auto& cell : grammar::expandDictionarySourceEntry(source)) {
        const size_t cell_length = normalize::utf8Length(cell.surface);
        if (host_end + cell_length <= codepoints.size() &&
            extractSubstring(codepoints, host_end, host_end + cell_length) == cell.surface) {
          emit(host_end, host_end + cell_length, std::string(suffix), cell.extended_pos);
        }
      }
    }
  }
}

}  // namespace suzume::analysis
