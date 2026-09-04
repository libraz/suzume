/**
 * @file join_noun.cpp
 * @brief Prefix+noun and verb-renyokei+suffix join candidate generation
 */

#include <algorithm>

#include "bigram_table.h"
#include "candidate_constants.h"
#include "core/debug.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/inflection.h"
#include "join_candidates.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "scorer_constants.h"
#include "tokenizer_utils.h"
#include "verb_candidates_helpers.h"

namespace suzume::analysis {

namespace {

using CharType = normalize::CharType;

// Bound nouns that head a verb-continuative compound (使い道, 待ち時間, 読み方).
// This is a closed class, so it lives in a table: a new head is data, not another
// comparison branch. `requires_verified_renyokei` admits only a dictionary-backed
// continuative form before the head, which keeps a coincidental kanji+hiragana run
// from being absorbed; the looser heads predate that check and keep their behavior.
// `closes_kanji_run` says the head is the right edge of its compound, so a kanji
// following it starts a word of its own (走れ|目標, 使い|道具, 書き|方向,
// 置き|場所). 物 is the exception: it is itself a productive nominalizer that
// takes a further derivational kanji (食べ物|屋, 飲み物|代, 落し物|係), so for
// it the following kanji proves nothing.
struct DeverbalHeadNoun {
  std::string_view surface;
  bool requires_verified_renyokei;
  bool closes_kanji_run;
};

const DeverbalHeadNoun kDeverbalHeadNouns[] = {
    {"物", false, false},  // 食べ物, 飲み物
    {"方", false, true},   // 読み方, やり方
    {"所", false, true},   // 居場所
    {"目", false, true},   // 割れ目, 切れ目, 裂け目
    {"手", true, true},    // 読み手, 書き手, 受け手
    {"場", true, true},    // 売り場, 買い場
    {"道", true, true},    // 使い道, 帰り道
    {"時間", true, true},  // 待ち時間
};

// Longest head in the table, in codepoints.
constexpr size_t kMaxDeverbalHeadLength = 2;

// Productive prefixes for prefix+noun joining
struct ProductivePrefix {
  char32_t codepoint;
  float bonus;
  bool needs_kanji;
};

const ProductivePrefix kProductivePrefixes[] = {
    // Note: Honorific prefixes お, ご, 御 are NOT included here.
    // They should be tokenized separately as PREFIX + NOUN.
    // E.g., お水 → お(PREFIX) + 水(NOUN), not お水(NOUN)

    // Negation prefixes
    {U'不', candidate::kProductivePrefixJoinBonus, true},  // 不安, 不要, 不便
    {U'未', candidate::kProductivePrefixJoinBonus, true},  // 未経験, 未確認
    {U'非', candidate::kProductivePrefixJoinBonus, true},  // 非常, 非公開
    {U'無', candidate::kProductivePrefixJoinBonus, true},  // 無理, 無料

    // Degree/quantity prefixes
    {U'超', candidate::kIntensifierPrefixJoinBonus, true},  // 超人, 超高速
    {U'再', candidate::kProductivePrefixJoinBonus, true},   // 再開, 再確認
    {U'準', candidate::kProductivePrefixJoinBonus, true},   // 準備, 準決勝
    {U'副', candidate::kProductivePrefixJoinBonus, true},   // 副社長, 副作用
    {U'総', candidate::kProductivePrefixJoinBonus, true},   // 総合, 総数
    {U'各', candidate::kProductivePrefixJoinBonus, true},   // 各地, 各種
    {U'両', candidate::kProductivePrefixJoinBonus, true},   // 両方, 両手
    {U'最', candidate::kProductivePrefixJoinBonus, true},   // 最高, 最新
    {U'全', candidate::kProductivePrefixJoinBonus, true},   // 全部, 全員
    {U'半', candidate::kProductivePrefixJoinBonus, true},   // 半分, 半額
};

constexpr size_t kNumPrefixes = sizeof(kProductivePrefixes) / sizeof(kProductivePrefixes[0]);

// Maximum noun length for prefix joining
constexpr size_t kMaxNounLenForPrefix = 6;

// A productive prefix compound can be a na-adjective only when its base has
// an adjective entry. A following copula alone is not sufficient evidence:
// ordinary nouns such as 最中 also occur before だ.
bool hasNaAdjectiveContinuation(const std::vector<char32_t>& codepoints, size_t pos) {
  if (pos >= codepoints.size()) {
    return false;
  }
  if (codepoints[pos] == U'だ' || codepoints[pos] == U'で' || codepoints[pos] == U'な') {
    return true;
  }
  return pos + 1 < codepoints.size() && codepoints[pos] == U'そ' && codepoints[pos + 1] == U'う';
}

// Cost bonus imported from candidate_constants.h:
// candidate::kVerifiedNounBonus

bool isHiraganaHonorificPrefix(char32_t codepoint) {
  return codepoint == U'お' || codepoint == U'ご';
}

// Whether a kanji stem plus its kana tail spells an attested i-adjective rather
// than a verb continuative. The modern 終止/連体 い is the surface itself
// (高い所); the classical 連体 き inflects off the same base, so the dictionary
// carries the stem plus い instead (高き所, 美しき所). Both endings are equally
// legitimate godan continuatives (言い方, 読み方, 巻き物), which is why the
// dictionary decides the reading rather than the kana.
bool isAttestedAdjectiveBeforeHead(const dictionary::DictionaryManager& dict_manager,
                                   const std::vector<char32_t>& codepoints, size_t start_pos, size_t hiragana_end) {
  const char32_t final_kana = codepoints[hiragana_end - 1];
  const std::string span = extractSubstring(codepoints, start_pos, hiragana_end);
  if (final_kana == U'い') {
    return verb_helpers::isAdjectiveInDictionary(&dict_manager, span);
  }
  if (final_kana != U'き') {
    return false;
  }
  const std::string base = normalize::concat(span.substr(0, span.size() - core::kJapaneseCharBytes), "い");
  return verb_helpers::isAdjectiveInDictionary(&dict_manager, base);
}

bool isCaseParticleCodepoint(char32_t codepoint) {
  switch (codepoint) {
    case U'に':
    case U'で':
    case U'と':
    case U'を':
    case U'が':
    case U'は':
    case U'へ':
    case U'も':
    case U'か':
    case U'や':
      return true;
    default:
      return false;
  }
}

void addHonorificSamaNounJoinCandidate(core::Lattice& lattice, std::string_view text,
                                       const std::vector<char32_t>& codepoints, const ByteOffsets& byte_offsets,
                                       size_t start_pos, const std::vector<normalize::CharType>& char_types,
                                       const Scorer& scorer) {
  if (start_pos + 2 >= codepoints.size() || !isHiraganaHonorificPrefix(codepoints[start_pos]) ||
      char_types[start_pos + 1] != CharType::Kanji) {
    return;
  }

  size_t end_pos = start_pos + 1;
  while (end_pos < codepoints.size() && char_types[end_pos] == CharType::Kanji) {
    ++end_pos;
  }

  // A productive honorific noun needs a lexical kanji base before the closed
  // honorific suffix. This preserves the ordinary prefix analysis for お様.
  if (end_pos - start_pos < 3 || codepoints[end_pos - 1] != U'様') {
    return;
  }

  std::string surface(textRange(text, byte_offsets, start_pos, end_pos));
  const float cost = scorer.posPrior(core::PartOfSpeech::Noun) + candidate::kHonorificSamaNounBonus;
  lattice.addEdge(surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(end_pos), core::PartOfSpeech::Noun,
                  cost, core::LatticeEdge::kIsUnknown);
}

void addStandaloneHonorificPrefixInterjectionCandidate(core::Lattice& lattice, std::string_view text,
                                                       const std::vector<char32_t>& codepoints,
                                                       const ByteOffsets& byte_offsets, size_t start_pos,
                                                       const std::vector<normalize::CharType>& char_types,
                                                       const Scorer& scorer) {
  if (start_pos + 1 != codepoints.size() ||
      !grammar::isHonorificPrefix(extractSubstring(codepoints, start_pos, start_pos + 1))) {
    return;
  }
  // Standalone means both sides are free: the prefix has lost its host on the
  // right, and nothing runs into it on the left. Ending the input is not
  // enough — with only the right condition this cheap one-mora interjection
  // detaches the final mora of any hiragana noun that happens to end in お or
  // ご (いちご read as いち + ご).
  if (start_pos > 0 && char_types[start_pos - 1] != normalize::CharType::Symbol) {
    return;
  }

  std::string surface(textRange(text, byte_offsets, start_pos, start_pos + 1));
  const float cost =
      scorer.posPrior(core::PartOfSpeech::Interjection) + candidate::kStandaloneHonorificPrefixInterjectionBonus;
  lattice.addEdge(surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(start_pos + 1),
                  core::PartOfSpeech::Interjection, cost, core::LatticeEdge::kIsUnknown);
}

}  // namespace

void addPrefixNounJoinCandidates(core::Lattice& lattice, std::string_view text, const std::vector<char32_t>& codepoints,
                                 const ByteOffsets& byte_offsets, size_t start_pos,
                                 const std::vector<normalize::CharType>& char_types,
                                 const dictionary::DictionaryManager& dict_manager, const Scorer& scorer,
                                 const grammar::Inflection& inflection) {
  if (start_pos >= codepoints.size()) {
    return;
  }

  addHonorificSamaNounJoinCandidate(lattice, text, codepoints, byte_offsets, start_pos, char_types, scorer);
  addStandaloneHonorificPrefixInterjectionCandidate(lattice, text, codepoints, byte_offsets, start_pos, char_types,
                                                    scorer);

  // Check if current character is a productive prefix
  char32_t current_char = codepoints[start_pos];
  const ProductivePrefix* matched_prefix = nullptr;

  for (size_t idx = 0; idx < kNumPrefixes; ++idx) {
    if (kProductivePrefixes[idx].codepoint == current_char) {
      matched_prefix = &kProductivePrefixes[idx];
      break;
    }
  }

  if (matched_prefix == nullptr) {
    return;
  }

  // Check if there's a noun part following
  size_t noun_start = start_pos + 1;
  if (noun_start >= codepoints.size()) {
    return;
  }

  // For most prefixes, the noun part should start with kanji
  if (matched_prefix->needs_kanji) {
    if (char_types[noun_start] != CharType::Kanji) {
      return;
    }
  } else {
    if (char_types[noun_start] != CharType::Kanji && char_types[noun_start] != CharType::Katakana) {
      return;
    }
  }

  // Find the end of the noun part
  CharType noun_type = char_types[noun_start];
  size_t noun_end = findCharRegionEnd(char_types, noun_start, kMaxNounLenForPrefix, noun_type);
  if (noun_type == CharType::Kanji) {
    const size_t following_verb_start =
        longestNominalVerbContinuativeStart(codepoints, char_types, start_pos, noun_end, inflection, &dict_manager);
    if (following_verb_start > start_pos + 1 && following_verb_start < noun_end) {
      noun_end = following_verb_start;
    }
  }

  if (noun_end <= noun_start) {
    return;
  }

  // Check dictionary for compound nouns
  size_t noun_start_byte = byteOffsetAt(byte_offsets, noun_start);
  auto noun_results = dict_manager.lookup(text, noun_start_byte);
  bool noun_in_dict = false;
  bool adjective_in_dict = false;
  size_t dict_noun_end = noun_end;

  for (const auto& result : noun_results) {
    if (result.entry == nullptr) {
      continue;
    }
    if (result.entry->extended_pos == core::ExtendedPOS::AdjNaAdj) {
      if (result.length >= noun_end - noun_start) {
        adjective_in_dict = true;
      }
      if (result.length > dict_noun_end - noun_start) {
        dict_noun_end = noun_start + result.length;
      }
    }
    if (result.entry->pos == core::PartOfSpeech::Noun) {
      if (result.length > dict_noun_end - noun_start) {
        dict_noun_end = noun_start + result.length;
        noun_in_dict = true;
      } else if (result.length == noun_end - noun_start) {
        noun_in_dict = true;
      }
    }
  }

  if (dict_noun_end > noun_end) {
    noun_end = dict_noun_end;
  } else {
    // Skip single-kanji noun when followed by hiragana (likely verb pattern)
    if (noun_end - noun_start == 1 && noun_end < codepoints.size()) {
      if (char_types[noun_end] == CharType::Hiragana && !hasNaAdjectiveContinuation(codepoints, noun_end)) {
        return;
      }
    }
  }

  // Check if the combined form is already in dictionary.
  std::string surface(textRange(text, byte_offsets, start_pos, noun_end));

  if (dict_manager.lookupExact(surface) != nullptr) {
    return;
  }

  // Generate joined candidate
  float base_cost = scorer.posPrior(core::PartOfSpeech::Noun);
  float final_cost = base_cost + matched_prefix->bonus;

  // Apply length penalty to prevent over-concatenation
  // Prefix + noun should be 2-3 chars total for most verified cases
  // (e.g., 全員=2, 再開=2, 不安=2)
  // Longer unverified combinations should be split
  size_t total_len = noun_end - start_pos;
  const bool has_copular_na_adjective_continuation =
      noun_end < codepoints.size() &&
      (codepoints[noun_end] == U'だ' || codepoints[noun_end] == U'で' || codepoints[noun_end] == U'な');
  const bool has_predicative_adjective_evidence =
      adjective_in_dict || normalize::isNumeralCodepoint(codepoints[noun_start]);
  const bool is_predicative_negation_compound = scorer::startsWithNegationPrefix(surface) &&
                                                has_copular_na_adjective_continuation &&
                                                has_predicative_adjective_evidence;
  if (total_len >= 4 && !noun_in_dict) {
    // Strong penalty for unverified 4+ char combinations
    // Must overcome: prefix_bonus(-0.4) + optimal_length_bonus(-0.5) = -0.9
    // Target: make final cost higher than split path (~1.0)
    // Penalty: +2.0 base, +0.5 per extra char
    final_cost += candidate::kUnverifiedPrefixJoinLongBasePenalty +
                  candidate::kUnverifiedPrefixJoinLongPerCharPenalty * static_cast<float>(total_len - 4);
  } else if (total_len == 3 && !noun_in_dict && !is_predicative_negation_compound) {
    // Penalty for 3-char unverified so the join cannot beat the plain
    // 2-char kanji_seq noun split (e.g. 全部食 vs 全部|食 from 全部食べちゃった).
    // A negation-prefix compound before a copula or adjectival continuation is
    // a complete predicative unit (不十分だ, 不確かではない), even when its
    // open-class base has no dictionary entry.
    final_cost += candidate::kUnverifiedPrefixJoin3charPenalty;
  }

  if (noun_in_dict) {
    final_cost += scorer.joinOpts().verified_noun_bonus;
  }

  uint8_t flags = core::LatticeEdge::kIsUnknown;

  lattice.addEdge(surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(noun_end), core::PartOfSpeech::Noun,
                  final_cost, flags, "");

  // Capability compounds ending in 可能 are nominal expressions (再利用可能、
  // 使用可能). Their following な is the attributive copula, not evidence that
  // the whole productive prefix compound should be reclassified as an
  // adjective.
  bool is_nominal_capability_compound = utf8::endsWith(surface, "可能");
  // A dictionary-backed adjective after a productive prefix remains the
  // predicate head (超|簡単, 最|重要); the prefix+noun join must not turn that
  // host into a larger adjective merely because the same surface can also be
  // read nominally. Negation compounds are different: the prefix creates the
  // productive adjectival unit itself (不十分, 不確か), even when that whole
  // unit is absent from the compact dictionary. Keep the nominal path too so
  // non-adjectival contexts can still select it.
  if (!is_nominal_capability_compound && is_predicative_negation_compound) {
    float adjective_cost = scorer.posPrior(core::PartOfSpeech::Adjective) + matched_prefix->bonus;
    if (is_predicative_negation_compound) {
      adjective_cost += candidate::kPredicativeNegationPrefixAdjectiveBonus;
    }
    lattice.addEdge(surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(noun_end),
                    core::PartOfSpeech::Adjective, adjective_cost, flags, surface, dictionary::ConjugationType::None,
                    core::CandidateOrigin::PrefixCompound, candidate::kNoOriginConfidence, "prefix_na_adjective",
                    core::ExtendedPOS::AdjNaAdj);
  }
}

void addPronounPluralJoinCandidates(core::Lattice& lattice, std::string_view text,
                                    const std::vector<char32_t>& codepoints, const ByteOffsets& byte_offsets,
                                    size_t start_pos, const dictionary::DictionaryManager& dict_manager,
                                    const Scorer& scorer) {
  if (start_pos >= codepoints.size()) {
    return;
  }

  const size_t start_byte = byteOffsetAt(byte_offsets, start_pos);
  for (const auto& result : dict_manager.lookup(text, start_byte)) {
    if (result.entry == nullptr || result.entry->pos != core::PartOfSpeech::Pronoun) {
      continue;
    }
    const size_t suffix_pos = start_pos + result.length;
    if (suffix_pos >= codepoints.size() || codepoints[suffix_pos] != U'ら') {
      continue;
    }

    const size_t end_pos = suffix_pos + 1;
    std::string surface(textRange(text, byte_offsets, start_pos, end_pos));
    const float cost = scorer.posPrior(core::PartOfSpeech::Pronoun) + candidate::kVerifiedNounBonus;
    lattice.addEdge(surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(end_pos),
                    core::PartOfSpeech::Pronoun, cost, core::LatticeEdge::kFromDictionary, surface);
  }
}

void addDestinationSuffixNounJoinCandidates(core::Lattice& lattice, std::string_view text,
                                            const std::vector<char32_t>& codepoints, const ByteOffsets& byte_offsets,
                                            size_t start_pos, const dictionary::DictionaryManager& dict_manager,
                                            const Scorer& scorer) {
  constexpr size_t kDestinationSuffixLength = 2;
  const size_t end_pos = start_pos + kDestinationSuffixLength;
  if (start_pos == 0 || end_pos > codepoints.size() || codepoints[start_pos] != U'行' ||
      codepoints[start_pos + 1] != U'き') {
    return;
  }

  const std::string_view suffix_surface = textRange(text, byte_offsets, start_pos, end_pos);
  const auto* suffix_entry = dict_manager.lookupExact(suffix_surface, core::PartOfSpeech::Verb);
  if (suffix_entry == nullptr || suffix_entry->extended_pos != core::ExtendedPOS::VerbRenyokei ||
      suffix_entry->lemma != "行く") {
    return;
  }

  // The bound destination reading is nominal. At the end of input or before a
  // non-hiragana token it is complete by construction. Before hiragana, require
  // a following particle or a noun-selecting auxiliary; polite/desiderative
  // continuations instead prove the independent verb reading (学校行きます).
  bool nominal_right_context = end_pos == codepoints.size();
  if (!nominal_right_context) {
    nominal_right_context = normalize::classifyChar(codepoints[end_pos]) != normalize::CharType::Hiragana;
  }
  if (!nominal_right_context) {
    const size_t following_byte = byteOffsetAt(byte_offsets, end_pos);
    for (const auto& result : dict_manager.lookup(text, following_byte)) {
      if (result.entry == nullptr) {
        continue;
      }
      const auto extended_pos = result.entry->extended_pos;
      if (result.entry->pos == core::PartOfSpeech::Particle || extended_pos == core::ExtendedPOS::AuxCopulaDa ||
          extended_pos == core::ExtendedPOS::AuxCopulaDesu || extended_pos == core::ExtendedPOS::AuxConjectureRashii ||
          extended_pos == core::ExtendedPOS::AuxConjectureMitai) {
        nominal_right_context = true;
        break;
      }
    }
  }
  if (!nominal_right_context) {
    return;
  }

  // Keep the best noun analysis for each possible host start. The host can be
  // dictionary-backed or productively generated; this is what makes the rule
  // apply to arbitrary destinations without a place-name list.
  struct HostCandidate {
    size_t start;
    float cost;
  };
  std::vector<HostCandidate> hosts;
  for (size_t edge_start = 0; edge_start < start_pos; ++edge_start) {
    for (const uint32_t edge_id : lattice.edgeIdsAt(edge_start)) {
      const auto& edge = lattice.getEdge(edge_id);
      if (edge.end != start_pos || edge.pos != core::PartOfSpeech::Noun || edge.isFormalNoun()) {
        continue;
      }
      const float host_cost = scorer.wordCost(edge);
      auto host = std::find_if(hosts.begin(), hosts.end(),
                               [edge_start](const HostCandidate& candidate) { return candidate.start == edge_start; });
      if (host == hosts.end()) {
        hosts.push_back({edge_start, host_cost});
      } else {
        host->cost = std::min(host->cost, host_cost);
      }
    }
  }

  for (const HostCandidate& host : hosts) {
    const std::string_view surface = textRange(text, byte_offsets, host.start, end_pos);
    const float cost = host.cost + bigram_cost::kVeryStrongBonus;
    lattice.addEdge(surface, static_cast<uint32_t>(host.start), static_cast<uint32_t>(end_pos),
                    core::PartOfSpeech::Noun, cost, core::LatticeEdge::kIsUnknown | core::LatticeEdge::kHasCustomCost,
                    surface, dictionary::ConjugationType::None, core::CandidateOrigin::Join,
                    candidate::kNoOriginConfidence, "destination_suffix_noun", core::ExtendedPOS::Noun,
                    "destination_suffix_noun");
  }
}

void addDeverbalNounBeforeIndependentNakuCandidates(core::Lattice& lattice, std::string_view text,
                                                    const std::vector<char32_t>& codepoints,
                                                    const ByteOffsets& byte_offsets, size_t start_pos,
                                                    const dictionary::DictionaryManager& dict_manager,
                                                    const Scorer& scorer) {
  if (start_pos == 0 || start_pos >= codepoints.size()) {
    return;
  }

  // Select the adjective entry by grammatical identity rather than treating
  // every なく surface as independent; the homographic auxiliary remains a
  // separate candidate in the lattice.
  size_t naku_end = start_pos;
  const size_t start_byte = byteOffsetAt(byte_offsets, start_pos);
  for (const auto& result : dict_manager.lookup(text, start_byte)) {
    if (result.entry != nullptr && result.entry->pos == core::PartOfSpeech::Adjective &&
        result.entry->extended_pos == core::ExtendedPOS::AdjRenyokei &&
        grammar::isIndependentNegativeAdjective(result.entry->lemma)) {
      naku_end = start_pos + result.length;
      break;
    }
  }
  if (naku_end == start_pos) {
    return;
  }

  // A following particle/auxiliary belongs to the inflectional negative chain
  // (食べ + なく + て). With no such dependent continuation, the adjective is
  // the independent adverbial predicate in the "without X" construction.
  if (naku_end < codepoints.size()) {
    const size_t following_byte = byteOffsetAt(byte_offsets, naku_end);
    for (const auto& result : dict_manager.lookup(text, following_byte)) {
      if (result.entry != nullptr &&
          (result.entry->pos == core::PartOfSpeech::Particle || result.entry->pos == core::PartOfSpeech::Auxiliary)) {
        return;
      }
    }
  }

  struct DeverbalNounCandidate {
    size_t start;
    std::string_view surface;
  };
  std::vector<DeverbalNounCandidate> candidates;
  for (size_t edge_start = 0; edge_start < start_pos; ++edge_start) {
    bool has_noun = false;
    const core::LatticeEdge* renyokei = nullptr;
    for (const uint32_t edge_id : lattice.edgeIdsAt(edge_start)) {
      const auto& edge = lattice.getEdge(edge_id);
      if (edge.end != start_pos) {
        continue;
      }
      has_noun = has_noun || edge.pos == core::PartOfSpeech::Noun;
      if (edge.pos == core::PartOfSpeech::Verb && edge.extended_pos == core::ExtendedPOS::VerbRenyokei) {
        renyokei = &edge;
      }
    }
    if (!has_noun && renyokei != nullptr) {
      candidates.push_back({edge_start, renyokei->surface});
    }
  }

  for (const DeverbalNounCandidate& nominal : candidates) {
    lattice.addEdge(nominal.surface, static_cast<uint32_t>(nominal.start), static_cast<uint32_t>(start_pos),
                    core::PartOfSpeech::Noun, scorer.posPrior(core::PartOfSpeech::Noun), core::LatticeEdge::kIsUnknown,
                    nominal.surface, dictionary::ConjugationType::None, core::CandidateOrigin::NominalizedNoun,
                    candidate::kNoOriginConfidence, "renyokei_nominal_before_independent_naku",
                    core::ExtendedPOS::NounVerbal, "renyokei_nominal_before_independent_naku");
  }
}

void addVerbSuffixNounJoinCandidates(core::Lattice& lattice, std::string_view text,
                                     const std::vector<char32_t>& codepoints, const ByteOffsets& byte_offsets,
                                     size_t start_pos, const std::vector<normalize::CharType>& char_types,
                                     const dictionary::DictionaryManager& dict_manager, const Scorer& scorer,
                                     [[maybe_unused]] const grammar::Inflection& inflection) {
  if (start_pos >= codepoints.size()) {
    return;
  }

  // Hiragana-only stem + 方 (やり方, あり方) — V連用形 written entirely in hiragana.
  // Only emit when we're at a word boundary (start of input or preceded by
  // non-kanji), so we don't emit しい方 (from 美しい方) or similar adjective tails.
  if (char_types[start_pos] == CharType::Hiragana) {
    if (start_pos > 0 && char_types[start_pos - 1] == CharType::Kanji) {
      return;
    }
    if (start_pos + 2 >= codepoints.size() || codepoints[start_pos + 2] != U'方') {
      return;
    }
    if (char_types[start_pos + 1] != CharType::Hiragana) {
      return;
    }
    // A complete adjective (including adjectival ない) before 方 is an
    // attributive predicate, not a pure-hiragana verb continuative
    // (ない+方法, not ない方+法).
    const std::string stem = extractSubstring(codepoints, start_pos, start_pos + 2);
    if (dict_manager.lookupExact(stem, core::PartOfSpeech::Adjective) != nullptr) {
      return;
    }
    // Allow godan-ra renyokei (り: やる→やり, ある→あり) and godan-wa renyokei
    // (い: 言う→いい). 言う often has its stem-only renyokei written as いい
    // even when the kanji 言 is omitted.
    char32_t c1 = codepoints[start_pos + 1];
    if (c1 != U'り' && c1 != U'い') {
      return;
    }
    // Verbs are an open class, but a two-mora hiragana continuative is short
    // enough to be a coincidence: the tail of an i-adjective written across a
    // kanji stem has the same shape (望まし+い方 out of 望ましい+方向). Require
    // the reconstructed base to be attested before joining.
    const std::string continuative_base =
        normalize::concat(extractSubstring(codepoints, start_pos, start_pos + 1), grammar::godanBaseSuffixFromIRow(c1));
    if (!verb_helpers::isVerbInDictionary(&dict_manager, continuative_base)) {
      return;
    }
    size_t end_pos = start_pos + 3;
    std::string surface(textRange(text, byte_offsets, start_pos, end_pos));
    float base_cost = scorer.posPrior(core::PartOfSpeech::Noun);
    float final_cost = base_cost + candidate::kVerbSuffixNounJoinBonus;
    uint8_t flags = core::LatticeEdge::kFromDictionary;
    lattice.addEdge(surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(end_pos), core::PartOfSpeech::Noun,
                    final_cost, flags, surface);
    return;
  }

  // Must start with kanji (verb stem)
  if (char_types[start_pos] != CharType::Kanji) {
    return;
  }

  // Look for patterns: Kanji + Hiragana + Suffix(物/方/所)
  // Examples: 食べ物, 飲み物, 読み方, 居場所
  size_t pos = start_pos;

  // Find kanji portion (verb stem)
  size_t kanji_end = pos;
  while (kanji_end < codepoints.size() && char_types[kanji_end] == CharType::Kanji) {
    ++kanji_end;
  }

  if (kanji_end == pos) {
    return;  // No kanji found
  }

  // Look for optional hiragana (verb renyokei suffix like べ, み, き)
  size_t hiragana_end = kanji_end;
  while (hiragana_end < codepoints.size() && char_types[hiragana_end] == CharType::Hiragana) {
    // Only allow 1-2 hiragana characters for renyokei
    if (hiragana_end - kanji_end >= 2) {
      break;
    }
    ++hiragana_end;
  }
  // Reject if hiragana ends with な (na-adjective 連体形, not verb renyokei)
  // e.g., 効率的な方 should NOT become a compound noun (it's 効率+的+な+方)
  if (hiragana_end > kanji_end && codepoints[hiragana_end - 1] == U'な') {
    return;
  }

  // Reject if hiragana ends with た (past form, not verb renyokei)
  // e.g., 書いた方 should NOT become a compound noun (it's 書い+た+方)
  // Correct patterns: 歩き方, 食べ方 (V連用形+方)
  if (hiragana_end > kanji_end && (codepoints[hiragana_end - 1] == U'た' || codepoints[hiragana_end - 1] == U'だ')) {
    return;
  }

  // Reject if hiragana ends with の (genitive particle, not verb renyokei)
  // e.g., 今後の方針 should NOT become 今後の方 + 針 (it's 今後+の+方針)
  // の is a case particle, not a verb renyokei ending
  if (hiragana_end > kanji_end && codepoints[hiragana_end - 1] == U'の') {
    return;
  }

  // Reject if hiragana ends with い AND hiragana run is 2+ chars (i-adjective).
  // e.g., 美しい方 (kanji+しい) is an adjective + noun, not a compound.
  // But 言い方 (kanji+い, single hiragana い) is godan-wa V連用形 + 方 — valid.
  if (hiragana_end > kanji_end + 1 && codepoints[hiragana_end - 1] == U'い') {
    return;
  }

  // Reject if hiragana ends with る (verb rentaikei/dictionary form, not renyokei)
  // e.g., 見渡せる所 should NOT become a compound noun (it's 見渡せる + 所)
  // Valid patterns: 食べ物, 居場所 (verb renyokei + suffix)
  if (hiragana_end > kanji_end && codepoints[hiragana_end - 1] == U'る') {
    return;
  }

  // A case particle cannot be the final mora of a verb continuative.  The
  // scan admits up to two hiragana, so this must cover both one- and two-mora
  // spans: otherwise 高みを目 and 越えを目 are fabricated as deverbal nouns.
  // Valid forms such as 食べ物, 割れ目, and 読み方 end in the continuative,
  // never in a case particle.
  if (hiragana_end > kanji_end && isCaseParticleCodepoint(codepoints[hiragana_end - 1])) {
    return;
  }

  // The two-mora case particles から・より・まで can otherwise look like a
  // kanji verb stem ending in ら/り.  They are grammatical boundaries, so
  // never use them as the renyokei portion of a deverbal 手/場 noun.
  const std::string hiragana_portion = extractSubstring(codepoints, kanji_end, hiragana_end);
  if (utf8::equalsAny(hiragana_portion, {"から", "より", "まで"})) {
    return;
  }
  if (hiragana_end > kanji_end && !grammar::isIRowCodepoint(codepoints[hiragana_end - 1]) &&
      !grammar::isERowCodepoint(codepoints[hiragana_end - 1])) {
    return;
  }

  // Match the bound noun that heads the compound. Longest match first, so a
  // multi-kanji head (待ち時間) wins over a prefix of it.
  if (hiragana_end >= codepoints.size()) {
    return;
  }

  const DeverbalHeadNoun* head_noun = nullptr;
  size_t head_length = 0;
  for (size_t length = kMaxDeverbalHeadLength; length >= 1; --length) {
    if (hiragana_end + length > codepoints.size()) {
      continue;
    }
    const std::string candidate_head = extractSubstring(codepoints, hiragana_end, hiragana_end + length);
    for (const auto& entry : kDeverbalHeadNouns) {
      if (candidate_head == entry.surface) {
        head_noun = &entry;
        head_length = length;
        break;
      }
    }
    if (head_noun != nullptr) {
      break;
    }
  }

  if (head_noun == nullptr) {
    return;
  }

  // A head that closes its compound has to be the right edge of its own kanji
  // run. When another kanji follows, that run is a word of its own and the head
  // is its first character rather than a bound suffix (走れ|目標, 使い|道具).
  if (head_noun->closes_kanji_run && hiragana_end + head_length < codepoints.size() &&
      char_types[hiragana_end + head_length] == CharType::Kanji) {
    return;
  }

  // An attested i-adjective keeps its attributive boundary before every
  // ordinary suffix-homograph noun (古い物, 高い所, 高き所, 美しき所). Longer
  // -い tails were rejected above; the remaining one-mora い and the classical
  // き are ambiguous with a godan continuative, so consult the dictionary.
  if (hiragana_end > kanji_end && isAttestedAdjectiveBeforeHead(dict_manager, codepoints, start_pos, hiragana_end)) {
    return;
  }

  // We need at least some hiragana between kanji and suffix (verb renyokei ending)
  // Exception: single kanji + suffix is allowed for some patterns
  if (hiragana_end == kanji_end && kanji_end - start_pos < 2) {
    return;  // Too short without hiragana
  }

  // A strict head forms a productive deverbal noun only after a dictionary-backed
  // continuative verb form. Verify the reconstructed terminal form before
  // adding the joined search unit, so a coincidental kanji+hira sequence (for
  // example an adjective stem) cannot be absorbed merely because it is
  // followed by such a head.
  if (head_noun->requires_verified_renyokei) {
    if (hiragana_end == kanji_end) {
      return;
    }
    const std::string renyokei = extractSubstring(codepoints, start_pos, hiragana_end);
    const bool has_verb_reading = dict_manager.lookupExact(renyokei, core::PartOfSpeech::Verb) != nullptr;
    constexpr PartOfSpeechMask kModifierMask =
        partOfSpeechMask(core::PartOfSpeech::Determiner) | partOfSpeechMask(core::PartOfSpeech::Adjective);
    const bool is_closed_modifier = hasExactPartOfSpeech(dict_manager, renyokei, kModifierMask);
    if (is_closed_modifier && !has_verb_reading) {
      return;
    }
    const char32_t final_kana = codepoints[hiragana_end - 1];
    const std::string_view godan_ending = grammar::godanBaseSuffixFromIRow(final_kana);
    if (!godan_ending.empty()) {
      const std::string base_form =
          normalize::concat(renyokei.substr(0, renyokei.size() - core::kJapaneseCharBytes), godan_ending);
      if (dict_manager.lookupExact(base_form, core::PartOfSpeech::Verb) == nullptr) {
        return;
      }
    } else {
      // A kanji+e-row surface can be either an Ichidan continuative form
      // (受け→受ける) or an i-adjective conditional fragment (高け→高い).
      // The latter cannot form a deverbal 手/場 compound, so reject it when
      // the reconstructed i-adjective is attested; otherwise retain the
      // productive Ichidan reading.
      const std::string adjective_base = renyokei.substr(0, renyokei.size() - core::kJapaneseCharBytes) + "い";
      if (verb_helpers::isAdjectiveInDictionary(&dict_manager, adjective_base)) {
        return;
      }
    }
  }

  // Build the compound noun surface
  size_t end_pos = hiragana_end + head_length;  // Include the bound noun head

  std::string surface(textRange(text, byte_offsets, start_pos, end_pos));

  // Calculate cost with bonus for compound noun pattern
  float base_cost = scorer.posPrior(core::PartOfSpeech::Noun);
  float final_cost = base_cost + candidate::kVerbSuffixNounJoinBonus;

  uint8_t flags = core::LatticeEdge::kFromDictionary;

  lattice.addEdge(surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(end_pos), core::PartOfSpeech::Noun,
                  final_cost, flags, surface);  // lemma = surface for compound nouns
}

}  // namespace suzume::analysis
