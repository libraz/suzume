/**
 * @file adjective_candidates_kanji.cpp
 * @brief Kanji i-adjective and na-adjective candidate generation
 */

#include <algorithm>
#include <array>

#include "adjective_candidates.h"
#include "adjective_candidates_internal.h"
#include "analysis/candidate_constants.h"
#include "analysis/dictionary_probe.h"
#include "analysis/scorer_constants.h"
#include "core/debug.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/patterns.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "tokenizer_utils.h"
#include "unknown.h"
#include "verb_candidates_helpers.h"

namespace suzume::analysis {

using verb_helpers::addEmphaticVariants;
using verb_helpers::findCharRegionEnd;
using verb_helpers::isAdjectiveInDictionary;
using verb_helpers::isVerbInDictionary;

using adj_detail::isCompoundFormingAdjective;
using adj_detail::makeIAdjCandidate;
using adj_detail::makeNaAdjCandidate;

namespace {

/// Base form the inflection analyzer reports for every cell of the negative
/// predicate (ない/なく/なかっ/なけれ).
constexpr const char* kNegativeAdjectiveBase = "ない";

/// Stem of the derivational suffix that builds an i-adjective from a nominal
/// (未練がましい, 恩着せがましさ); its cells differ only past this point.
constexpr const char* kGaMashiiStem = "がまし";
constexpr size_t kGaMashiiStemLength = 3;
constexpr size_t kMaxGaMashiiInflectionLength = 8;

// A generated i-adjective cannot contain a complete auxiliary followed by a
// final particle.  That sequence closes an independently inflected predicate
// (終わっ+た+ばい), while a real adjective keeps its own inflection before any
// following particle.  Dictionary adjectives are checked by the caller and
// remain eligible for their lexical readings.
bool containsAuxiliaryFinalParticleBoundary(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                            const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || end_pos <= start_pos + 2) {
    return false;
  }
  for (size_t auxiliary_start = start_pos + 1; auxiliary_start + 1 < end_pos; ++auxiliary_start) {
    for (size_t auxiliary_end = auxiliary_start + 1; auxiliary_end < end_pos; ++auxiliary_end) {
      if (lookupEntryInRange(*dict_manager, codepoints, auxiliary_start, auxiliary_end,
                             core::PartOfSpeech::Auxiliary) == nullptr) {
        continue;
      }
      const auto* particle =
          lookupEntryInRange(*dict_manager, codepoints, auxiliary_end, end_pos, core::PartOfSpeech::Particle);
      if (particle != nullptr && particle->extended_pos == core::ExtendedPOS::ParticleFinal) {
        return true;
      }
    }
  }
  return false;
}

bool endsWithMultiMoraFinalParticle(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                    const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || end_pos <= start_pos + 2) {
    return false;
  }
  constexpr size_t kMaxFinalParticleChars = 4;
  const size_t earliest = end_pos > kMaxFinalParticleChars ? end_pos - kMaxFinalParticleChars : start_pos + 1;
  for (size_t particle_start = earliest; particle_start < end_pos - 1; ++particle_start) {
    const auto* particle =
        lookupEntryInRange(*dict_manager, codepoints, particle_start, end_pos, core::PartOfSpeech::Particle);
    if (particle != nullptr && particle->extended_pos == core::ExtendedPOS::ParticleFinal) {
      return true;
    }
  }
  return false;
}

// An unverified adjective cannot span an independently dictionary-backed verb
// within its putative stem (複数|見つかっ|た, not 複|数見つかった).  Check every
// internal span because the verb may be followed by its own auxiliary inside
// the adjective-shaped surface.
bool containsDictionaryVerbBoundary(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                    const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || end_pos <= start_pos + 2) {
    return false;
  }
  constexpr size_t kMinimumVerbLength = 2;
  for (size_t verb_start = start_pos + 1; verb_start + 1 < end_pos; ++verb_start) {
    if (hasDictionaryEntryFrom(dict_manager, codepoints, verb_start, kMinimumVerbLength, end_pos - 1 - verb_start,
                               core::PartOfSpeech::Verb, nullptr)) {
      return true;
    }
  }
  return false;
}

// =============================================================================
// Pattern Skip Helpers for I-Adjective Candidate Generation
// =============================================================================

/**
 * @brief Check if a pattern should be skipped based on simple pattern matching
 *
 * Checks for patterns that are clearly NOT i-adjectives:
 * - Empty surface
 * - Single kanji + single hiragana い (godan verb renyokei like 伴い, 用い)
 * - Patterns starting with っ (te-form contractions like 待ってく)
 * - Patterns ending with んでい/でい (te-form + auxiliary like 学んでい)
 * - Passive/causative negative renyokei (られなく, させなく)
 * - Negative become patterns (れなくなった)
 * - なく followed by なった/なる (verb negative + become)
 * - Causative stem patterns (べさ, べさせ)
 * - Godan verb renyokei + そう (飲みそう, 降りそう)
 *
 * @param surface Full surface string (kanji + hiragana)
 * @param hiragana_part Hiragana portion only
 * @param codepoints Full text codepoints
 * @param start_pos Start position in codepoints
 * @param kanji_end End of kanji portion
 * @param end_pos Current end position being checked
 * @return true if the pattern should be skipped
 */
bool shouldSkipSimplePatterns(const std::string& surface, const std::string& hiragana_part,
                              const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                              size_t end_pos) {
  // Empty surface
  if (surface.empty()) {
    return true;
  }

  // A terminal ない preceded by an a-row mora is a productive verb
  // mizenkei + negative auxiliary (止ま+ない), not one i-adjective. Likewise,
  // a closed case-particle mora inside the pre-negative tail proves that the
  // candidate crosses a grammatical boundary (今+まで+に+ない).
  if (utf8::endsWith(hiragana_part, "ない") && hiragana_part.size() > core::kTwoJapaneseCharBytes) {
    const std::string_view pre_negative =
        std::string_view(hiragana_part).substr(0, hiragana_part.size() - core::kTwoJapaneseCharBytes);
    const char32_t pre_negative_tail = utf8::decodeFirstChar(utf8::lastChar(pre_negative));
    if (grammar::isARowCodepoint(pre_negative_tail) ||
        utf8::containsAny(pre_negative, {"に", "を", "が", "の", "へ"})) {
      return true;
    }
  }

  // Once ない has completed, following particles/nominalizers are separate
  // morphemes (止ま+ない+の+か), never part of a longer adjective surface.
  const size_t negative_pos = hiragana_part.find("ない");
  if (negative_pos != std::string::npos && negative_pos + core::kTwoJapaneseCharBytes < hiragana_part.size()) {
    return true;
  }

  // Single-kanji + single hiragana い patterns - likely godan verb renyokei
  // Real single-kanji i-adjectives (怖い, 酸い) should be in dictionary
  if (kanji_end == start_pos + 1 && end_pos == kanji_end + 1) {
    return true;
  }

  // Copula negation patterns (kanji + じゃな...): 嫌じゃない, 嫌じゃなかった
  // These are na-adjective + じゃ(copula) + ない(negation), not i-adjectives
  if (utf8::startsWith(hiragana_part, "じゃな")) {
    return true;
  }

  // Patterns starting with っ (te-form contractions like 待ってく = 待っていく)
  if (utf8::startsWith(hiragana_part, "っ")) {
    return true;
  }

  // Patterns ending with んでい or でい (te-form + auxiliary like 学んでいく)
  if (utf8::endsWith(hiragana_part, "んでい") || utf8::endsWith(hiragana_part, "でい")) {
    return true;
  }

  // Passive/potential/causative negative renyokei (られなく, させなく, etc.)
  if (grammar::endsWithPassiveCausativeNegativeRenyokei(hiragana_part)) {
    return true;
  }

  // Negative become pattern (れなくなった)
  if (grammar::endsWithNegativeBecomePattern(hiragana_part)) {
    return true;
  }

  // なく followed by なった/なる/なって (verb negative + become)
  if (utf8::endsWith(hiragana_part, "なく") && end_pos < codepoints.size()) {
    if (codepoints[end_pos] == U'な') {
      return true;
    }
  }

  // Causative stem patterns (べさ, べさせ, etc.) - ichidan causative
  if (utf8::equalsAny(hiragana_part, {"べさ", "べさせ", "べさせら", "べさせられ"})) {
    return true;
  }

  // Godan verb renyokei + そう patterns (飲みそう, 降りそう, etc.)
  // Single kanji + renyokei suffix (i-row: み/ぎ/ち/び/り/に) + そう
  // Note: し and き are handled separately with dictionary validation
  if (kanji_end == start_pos + 1 && hiragana_part.size() >= core::kThreeJapaneseCharBytes) {
    std::string_view renyokei_char = std::string_view(hiragana_part).substr(0, core::kJapaneseCharBytes);
    if (utf8::equalsAny(renyokei_char, {"み", "ぎ", "ち", "び", "り", "に"}) &&
        hiragana_part.substr(core::kJapaneseCharBytes, core::kTwoJapaneseCharBytes) == scorer::kSuffixSou) {
      return true;
    }
  }

  return false;
}

}  // namespace

void generateAdjectiveCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                 const std::vector<normalize::CharType>& char_types,
                                 const grammar::Inflection& inflection,
                                 const dictionary::DictionaryManager* dict_manager,
                                 std::vector<UnknownCandidate>& candidates) {
  const size_t candidate_start = candidates.size();

  // Lexicalized adverbial adjective 間もなく (連用形 of 間もない, "soon"). A single
  // preceding kanji forms a compact noun with 間 (時間 / 居間), while a longer kanji
  // phrase can end before the adverb (終了間もなく). The base form 間もない is deliberately
  // not lexicalized (MeCab splits it as 間|も|ない), so only the 連用形 is recognized here.
  bool follows_single_kanji_compound = start_pos > 0 && start_pos - 1 < char_types.size() &&
                                       char_types[start_pos - 1] == normalize::CharType::Kanji &&
                                       (start_pos < 2 || char_types[start_pos - 2] != normalize::CharType::Kanji);
  if (start_pos + 3 < codepoints.size() && codepoints[start_pos] == U'間' && codepoints[start_pos + 1] == U'も' &&
      codepoints[start_pos + 2] == U'な' && codepoints[start_pos + 3] == U'く' && !follows_single_kanji_compound) {
    auto candidate =
        makeIAdjCandidate("間もなく", start_pos, start_pos + 4, "間もない", candidate::kLexicalizedAdverbialAdjCost,
                          CandidateOrigin::AdjectiveI, candidate::kDictFallbackAdjConfidence, "ma_mo_naku");
    candidate.has_suffix = true;
    candidates.push_back(std::move(candidate));
  }

  if (start_pos >= char_types.size() || char_types[start_pos] != normalize::CharType::Kanji) {
    return;
  }

  // The ideographic iteration mark always belongs to the preceding kanji
  // (時々, 人々).  It cannot start an adjective stem, even when the following
  // kana look like an i-adjective ending.
  if (normalize::isIterationMark(codepoints[start_pos])) {
    return;
  }

  // A productive adjective-forming second element may follow a multi-kanji
  // nominal host (用心+深い, 我慢+強い). Extend the ordinary two-kanji scan only
  // when the final kanji plus its okurigana is itself such a second element.
  // This keeps unrelated noun + predicate sequences (半数+近く, 一人+歩いた)
  // out of the whole-span adjective path.
  constexpr size_t kMaxKanjiAdjectiveStemLength = 6;
  size_t kanji_end = findCharRegionEnd(char_types, start_pos, 2, normalize::CharType::Kanji);
  const size_t extended_kanji_end =
      findCharRegionEnd(char_types, start_pos, kMaxKanjiAdjectiveStemLength, normalize::CharType::Kanji);
  if (extended_kanji_end > kanji_end && extended_kanji_end < char_types.size() &&
      char_types[extended_kanji_end] == normalize::CharType::Hiragana) {
    const size_t tail_hiragana_end =
        findCharRegionEnd(char_types, extended_kanji_end, 5, normalize::CharType::Hiragana);
    for (size_t end_pos = tail_hiragana_end; end_pos > extended_kanji_end; --end_pos) {
      const std::string tail_surface = extractSubstring(codepoints, extended_kanji_end - 1, end_pos);
      const auto& tail_analyses = inflection.analyze(tail_surface);
      const bool has_productive_tail =
          std::any_of(tail_analyses.begin(), tail_analyses.end(), [](const grammar::InflectionCandidate& candidate) {
            return candidate.verb_type == grammar::VerbType::IAdjective &&
                   adj_detail::isCompoundFormingAdjective(candidate.base_form);
          });
      if (has_productive_tail) {
        kanji_end = extended_kanji_end;
        break;
      }
    }
  }
  if (verb_helpers::isReduplicatedShiiAdjectiveHead(codepoints, start_pos) &&
      findCharRegionEnd(char_types, start_pos, 4, normalize::CharType::Kanji) == start_pos + 4) {
    kanji_end = start_pos + 4;
  }

  if (kanji_end == start_pos) {
    return;
  }

  // Look for hiragana after kanji (adjective endings like い, かった, くない)
  // Note: Some adjectives have hiragana in the stem (美しい, 楽しい, 涼しい, etc.)
  // so we allow any hiragana and let the inflection module decide
  if (kanji_end >= char_types.size() || char_types[kanji_end] != normalize::CharType::Hiragana) {
    return;
  }

  // Check if first hiragana is a particle that can NEVER be part of an adjective
  // Note: て is the te-form particle (接続助詞), not part of adjective stems
  // This prevents "来てい" from being parsed as an adjective (来ている = verb)
  char32_t first_hiragana = codepoints[kanji_end];
  if (normalize::isNeverAdjectiveStemAfterKanji(first_hiragana)) {
    // Exception: medial も in a lexical i-adjective (頼もしい, 好もしい, and their
    // conjugations 頼もしく/頼もしかっ…). The adjective-forming stem consonant し
    // immediately follows も; the 係助詞 reading (本もない = 本 + も + ない) never
    // places し after も, so require it here. する cases that also read も+し
    // (見もしない) are dropped downstream by the loop's verb-negative filter.
    bool medial_mo_adjective = first_hiragana == U'も' && kanji_end == start_pos + 1 &&
                               kanji_end + 1 < codepoints.size() && codepoints[kanji_end + 1] == U'し';
    // Exception: the derivational suffix がまし〜, which turns a nominal into an
    // i-adjective (未練がましい, 言い訳がましく, 恩着せがましさ). Its が is part of
    // the suffix, never the case particle, and the suffix is closed — so require
    // the whole stem of it, not just its opening mora.
    const bool ga_mashii_derivation =
        first_hiragana == U'が' && kanji_end + kGaMashiiStemLength <= codepoints.size() &&
        extractSubstring(codepoints, kanji_end, kanji_end + kGaMashiiStemLength) == kGaMashiiStem;
    if (!medial_mo_adjective && !ga_mashii_derivation) {
      return;  // These particles follow nouns/verbs, not adjective stems
    }
  }

  size_t hiragana_end = findCharRegionEnd(char_types, kanji_end, 8, normalize::CharType::Hiragana);

  if (hiragana_end <= kanji_end) {
    return;
  }

  if (adj_detail::appendKanjiIAdjSpecialCandidates(codepoints, start_pos, kanji_end, hiragana_end, char_types,
                                                   inflection, dict_manager, candidates)) {
    return;
  }

  // Try different ending lengths
  for (size_t end_pos = hiragana_end; end_pos > kanji_end; --end_pos) {
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);
    std::string hiragana_part = extractSubstring(codepoints, kanji_end, end_pos);

    // Skip patterns that are clearly not i-adjectives
    if (shouldSkipSimplePatterns(surface, hiragana_part, codepoints, start_pos, kanji_end, end_pos)) {
      continue;
    }

    // A focus particle opening the okurigana is not adjective morphology: the
    // run is [noun] + particle, and the い that looks like an adjective ending
    // starts the next word (水とか + いう read as the non-word 水とかい).
    // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
    if (!isAdjectiveInDictionary(dict_manager, surface) &&
        verb_helpers::guardIsWired(verb_helpers::GuardMember::FocusParticleHead,
                                   verb_helpers::GuardOrigin::KanjiAdjective) &&
        verb_helpers::startsWithFocusParticleHead(dict_manager, codepoints, kanji_end, end_pos)) {
      SUZUME_DEBUG_LOG_VERBOSE("[ADJ_SKIP] \"" << surface << "\" hiragana head is a focus particle\n");
      continue;
    }

    // For single kanji + ければ patterns (叩ければ, 引ければ, etc.),
    // check if the kanji + く is a verb. If so, this is likely verb potential + conditional,
    // not an adjective pattern.
    // 叩ければ → 叩く (verb exists) → skip adjective (叩い is not a real adjective)
    // 寒ければ → 寒い (adjective) - handled separately as hiragana_part starts with け
    if (kanji_end == start_pos + 1 && hiragana_part == "ければ") {
      std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
      std::string verb_form = kanji_stem + "く";
      if (isVerbInDictionary(dict_manager, verb_form)) {
        continue;  // Verb exists, this is verb potential-conditional (叩ける + ば)
      }
    }

    // Skip patterns that are clearly verb negatives, not adjectives
    // 〜かない, 〜がない, etc. are Godan verb mizenkei + ない patterns
    // 〜しない is Suru verb + ない, 〜べない is Ichidan verb + ない
    // Exception: dictionary-confirmed adjectives (情けない, 味気ない, etc.)
    // These are genuine i-adjectives whose okurigana coincidentally matches
    // verb negative patterns but must not be blocked.
    if (grammar::endsWithVerbNegative(hiragana_part)) {
      if (!isAdjectiveInDictionary(dict_manager, surface)) {
        continue;  // Skip - verb negative pattern, not adjective
      }
      SUZUME_DEBUG_LOG_VERBOSE("[ADJ_NAI] dict-confirmed nai-adj: \"" << surface << "\"\n");
    }

    // A 2+ consecutive-kanji stem directly followed by nothing but the negative
    // predicate is a noun plus ない (問題+ない, 資金+なくして, 問題+なかっ+た),
    // never a newly constructed i-adjective. The negative has a full paradigm,
    // and every cell of it attaches to the noun the same way, so recognize the
    // form through the inflection analyzer instead of naming its surfaces —
    // otherwise the past cell escapes and its trimmed かっ variant inherits an
    // unpenalized cost. Suppress the fused candidate so the noun and
    // negative-adjective path wins. Dictionary-confirmed multi-kanji
    // nai-adjectives (味気ない etc.) keep their fused form, in any cell.
    bool is_bare_nai_form = false;
    for (const auto& nai_cand : inflection.analyze(hiragana_part)) {
      if (nai_cand.verb_type == grammar::VerbType::IAdjective && nai_cand.base_form == kNegativeAdjectiveBase) {
        is_bare_nai_form = true;
        break;
      }
    }
    if (is_bare_nai_form && (kanji_end - start_pos) >= 2 && !isAdjectiveInDictionary(dict_manager, surface) &&
        !isAdjectiveInDictionary(dict_manager,
                                 extractSubstring(codepoints, start_pos, kanji_end) + kNegativeAdjectiveBase)) {
      continue;
    }

    // Skip patterns that are サ変動詞 + て + auxiliary
    // E.g., 説明してほしい = 説明(noun) + し(suru renyokei) + て + ほしい
    // These should split, not be treated as single adjectives
    if (surface.size() >= 15 &&  // At least 5 chars (kanji + してほしい)
        surface.find("してほしい") != std::string::npos) {
      continue;  // Skip - suru verb + te + hoshii pattern
    }

    // Skip surfaces that are known dictionary verbs
    // E.g., 下さい(=ください) is a verb (くださる), not an i-adjective
    if (isVerbInDictionary(dict_manager, surface)) {
      continue;
    }

    // An unregistered adjective hypothesis must not swallow a dictionary verb
    // sitting at its tail (一枚+ください, not the fabricated adjective 一枚ください).
    // Mirrors the equivalent guard on the verb candidate paths. Single-kana tails
    // are excluded because they are inflectional endings, not lexical verbs.
    bool tail_is_dict_verb = false;
    for (size_t split = start_pos + 1; split + 1 < end_pos; ++split) {
      if (isVerbInDictionary(dict_manager, extractSubstring(codepoints, split, end_pos))) {
        tail_is_dict_verb = true;
        break;
      }
    }

    // Check all candidates for IAdjective, not just the best one
    // This handles cases like 美味しそう where Suru (美味する) may have higher
    // confidence than IAdjective (美味しい), but we still want to generate
    // an adjective candidate for the lattice to choose from
    const auto& all_candidates = inflection.analyze(surface);

    for (const auto& cand : all_candidates) {
      // A dictionary-backed adjective remains eligible as a second component
      // of a compound (山 / 高し). Only an unverified hypothesis that crosses
      // an independently attested verb boundary is suppressed.
      if (!isAdjectiveInDictionary(dict_manager, cand.base_form) &&
          containsDictionaryVerbBoundary(codepoints, start_pos, end_pos, dict_manager)) {
        continue;
      }
      // A long unregistered i-adjective in its uninflected form is recognizable
      // wherever no following kana can continue the inflection: before a content
      // word (面倒くさい作業) and equally at the end of the sentence (面倒くさい).
      // A following kana is excluded because it may be an auxiliary that selects
      // a different analysis (〜いた, 〜いて).
      const bool uninflected_continuation_follows =
          end_pos < codepoints.size() && !normalize::isKanjiCodepoint(codepoints[end_pos]) &&
          normalize::classifyChar(codepoints[end_pos]) != normalize::CharType::Katakana;
      const bool long_uninflected_base = cand.verb_type == grammar::VerbType::IAdjective && cand.base_form == surface &&
                                         normalize::utf8Length(surface) >= 4 && !uninflected_continuation_follows &&
                                         !tail_is_dict_verb;
      const bool complete_past_form =
          cand.verb_type == grammar::VerbType::IAdjective && utf8::endsWith(surface, "かった") &&
          cand.confidence >= candidate::kCompoundAdjConfMin &&
          (!utf8::endsWith(cand.base_form, "ない") || isAdjectiveInDictionary(dict_manager, cand.base_form));
      // Require confidence >= 0.5 for i-adjectives
      // Base forms like 寒い get exactly 0.5, conjugated forms like 美しかった get 0.68+
      if ((cand.confidence >= candidate::kIAdjConfMin || complete_past_form ||
           (long_uninflected_base && cand.confidence >= candidate::kCompoundAdjConfMin)) &&
          cand.verb_type == grammar::VerbType::IAdjective) {
        // Filter out false positives: いたす honorific pattern
        // Invalid patterns (all have た after the candidate):
        //   - サ変名詞 + いたす: 検討いたします, 勉強いたしました
        //   - Verb renyokei + いたす: 伝えいたします, 申しいたします
        // Valid patterns:
        //   - 面白いな (next char is な)
        //   - 寒いよ (next char is よ)
        //   - 面白い (end of text)
        // Key insight: if minimum confidence (0.5) and next char is た, skip
        if (cand.confidence <= candidate::kIAdjConfMin) {
          if (end_pos < codepoints.size() && codepoints[end_pos] == U'た') {
            continue;  // Skip - likely いたす honorific pattern
          }
        }

        // Skip verb renyokei + たい patterns (desiderative auxiliary)
        // The inflection engine may identify verb+たい conjugations as i-adjectives:
        //   行きたくなかった → base_form=行きたくない (contains たくない)
        //   行きたかった → base_form=行きたい (ends with たい)
        // Real adjective: 冷たくなかった → base_form=冷たくない, char before たくない is 冷 (kanji)
        // False adjective: 行きたくなかった → base_form=行きたくない, char before たくない is き (hiragana)
        {
          std::string_view base_sv(cand.base_form);
          // Determine the stem before the たい-related suffix
          std::string_view before_tai;
          if (utf8::endsWith(base_sv, "たくない") && base_sv.size() > 4 * core::kJapaneseCharBytes) {
            // Negative form: 行きたくない → check char before たくない
            before_tai = base_sv.substr(0, base_sv.size() - 4 * core::kJapaneseCharBytes);
          } else if (utf8::endsWith(base_sv, "たい") && base_sv.size() > 2 * core::kJapaneseCharBytes) {
            // Base form: 行きたい → check char before たい
            before_tai = base_sv.substr(0, base_sv.size() - 2 * core::kJapaneseCharBytes);
          }
          if (!before_tai.empty()) {
            auto last_cp = utf8::decodeFirstChar(utf8::lastChar(before_tai));
            if (last_cp != 0 && kana::isHiraganaCodepoint(last_cp)) {
              continue;  // Verb renyokei + たい, not a real adjective
            }
          }
        }

        // 様態 そう span guard: an i-adjective never inflects through そう —
        // そう is always a separate appearance auxiliary. When the inflection
        // engine reconstructed the base by reading そう(な/だ/に…) as an
        // adjective ending (surface = stem + そう…, base = stem + い), the span
        // over-reaches the stem: the AdjStem generator emits the bare stem
        // (優し, 高, 大き) and そう attaches as its own token. This also covers
        // verb renyokei + そう (書きそう, 遅刻しそう), whose hypothesized base
        // stem + い is a non-word.
        {
          std::string_view base_sv(cand.base_form);
          std::string_view surf_sv(surface);
          if (utf8::endsWith(base_sv, "い")) {
            std::string_view stem_sv = base_sv.substr(0, base_sv.size() - core::kJapaneseCharBytes);
            if (surf_sv.size() > stem_sv.size() && utf8::startsWith(surf_sv, stem_sv) &&
                utf8::startsWith(surf_sv.substr(stem_sv.size()), scorer::kSuffixSou)) {
              SUZUME_DEBUG_LOG_VERBOSE("[ADJ_SKIP] \"" << surface << "\" spans 様態そう, stem path handles split\n");
              continue;
            }
          }
        }

        // Lower base cost (0.2F) to beat verb candidates after POS prior adjustment
        // ADJ prior (0.3) is higher than VERB prior (0.2), so we need lower edge cost
        float cost = candidate::confidenceScaledCost(candidate::kKanjiAdjBaseCost, cand.confidence,
                                                     candidate::kKanjiAdjConfScale);
        if (verb_helpers::isReduplicatedShiiAdjectiveHead(codepoints, start_pos) && kanji_end == start_pos + 4) {
          cost += candidate::kReduplicatedShiiAdjBonus;
        }
        const bool ga_mashii_derivation = utf8::endsWith(surface, "がましい");
        bool meka_shii_derivation = utf8::endsWith(surface, "めかしい");
        if (meka_shii_derivation) {
          const std::string stem = surface.substr(0, surface.size() - std::string_view("めかしい").size());
          const std::string adjective_base = stem + "い";
          meka_shii_derivation =
              isAdjectiveInDictionary(dict_manager, adjective_base) ||
              adj_detail::firstConfidenceAtLeast(inflection.analyze(adjective_base), grammar::VerbType::IAdjective,
                                                 candidate::kIAdjConfMin) != candidate::kNoOriginConfidence;
        }
        if (ga_mashii_derivation || meka_shii_derivation) {
          cost = std::min(cost, candidate::kDerivedSuffixAdjectiveCost);
        }
        // A bare さ ending is not an inflected i-adjective form.  The generic
        // inflection analyzer can still hypothesize a fake Xい base for it
        // (語りぐさ -> 語りぐい), which lets a whole-span adjective edge beat
        // the noun/nominalizer analyses.  No dictionary exception is needed:
        // さ is a nominalizing suffix, never a finite i-adjective ending.
        if (utf8::endsWith(surface, "さ")) {
          SUZUME_DEBUG_LOG_VERBOSE("[ADJ_SKIP] \"" << surface << "\" bare sa ending is not an i-adjective form\n");
          continue;
        }
        // Penalty for compound adjective patterns (verb renyokei + やすい/にくい/がたい)
        // MeCab splits these: 使いにくい → 使い + にくい
        // Must be non-dictionary adjectives with >= 4 characters to avoid penalizing
        // standalone やすい/にくい/がたい which are in the dictionary
        if (surface.size() >= 4 * core::kJapaneseCharBytes && verb_helpers::isCompoundAdjectivePattern(surface)) {
          cost += candidate::kAdjSplitForcePenalty;  // Force split
          SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface << "\" +2.0 (compound_adj_penalty)\n");
        }
        // Penalty for く + なる patterns (i-adjective adverbial + なる verb)
        // MeCab splits these: 良くなる → 良く + なる, 高くなった → 高く + なっ + た.
        // Scan the whole surface (not just its end) so trailing auxiliaries after
        // the absorbed なる (寒くなってきた = 寒く+なっ+て+き+た) are still caught.
        // Must have at least 2 chars before くなる to avoid penalizing standalone patterns
        if (surface.size() >= 3 * core::kJapaneseCharBytes) {
          if (verb_helpers::containsKuNaruPattern(surface)) {
            cost += candidate::kAdjSplitForcePenalty;  // Force adj く-form + なる split
            SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface << "\" +2.0 (ku_naru_split)\n");
          }
        }
        // Penalty for とい/という endings (noun + quotative patterns, not adjectives)
        // E.g., 友人という → 友人 + という (determiner), not 友人とい(adj) + う
        if (surface.size() >= 3 * core::kJapaneseCharBytes) {
          if (utf8::endsWith(surface, "とい") || utf8::endsWith(surface, "という")) {
            cost += candidate::kAdjSplitForcePenalty;  // Protect NOUN + という pattern
            SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface << "\" +2.0 (toiu_pattern)\n");
          }
        }
        // Penalty for らしい endings (adj + conjecture auxiliary patterns)
        // E.g., 美しいらしい → 美しい + らしい, not 美しいらし(adj) + い
        // 春らしい → 春 + らしい, not 春らし(adj) + い
        // Must have at least 2 chars before らしい to avoid penalizing standalone らしい
        if (surface.size() >= 3 * core::kJapaneseCharBytes) {
          // Also match the らしく + negative forms (らしくない/らしくなかっ/らしくなかった):
          // their surface ends in the negative, not らしく, so the ku-form trimmed
          // variant would otherwise inherit an unpenalized cost and keep 子供らしく
          // merged (子供らしくない → 子供 + らしく + ない). A genuine adjective whose
          // stem before らしく is a non-word (素晴らしい) stays merged because splitting
          // it off leaves the costly non-word 素晴.
          if (utf8::endsWith(surface, "らしい") || utf8::endsWith(surface, "らしく") ||
              utf8::endsWith(surface, "らしかっ") || utf8::endsWith(surface, "らしくない") ||
              utf8::endsWith(surface, "らしくなかっ") || utf8::endsWith(surface, "らしくなかった")) {
            cost += candidate::kAdjModeratePenalty;  // Promote adj/noun + らしい split
            SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface << "\" +1.5 (rashii_conjecture)\n");
          }
        }
        // Penalty for まい endings (verb + negative volitional auxiliary)
        // E.g., 知るまい → 知る + まい, 出来まい → 出来 + まい
        // まい is an auxiliary attached to verb dictionary form, not an i-adjective suffix
        if (surface.size() >= 2 * core::kJapaneseCharBytes) {
          if (utf8::endsWith(surface, "まい")) {
            cost += candidate::kAdjSplitForcePenalty;  // Promote verb + まい split
            SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface << "\" +2.0 (mai_auxiliary)\n");
          }
        }
        // Skip a fake i-adjective that is really [noun] + a dictionary verb whose
        // onbin tail reconstructs a non-word かい/たい-shaped base: 手間+かかった →
        // 手間かい, 2時間半+かかった → 半かい. These share the [X]+かい shape with
        // genuine adjectives (細かい), so distinguish by dictionary — skip only when
        // the base is not a dictionary adjective, its stem ends in hiragana (excludes
        // pure-kanji stems like 高い), and the hiragana tail is itself a complete
        // conjugation of a dictionary verb (かかった → かかる). [noun][verb] is not
        // an adjective, so skip rather than penalize (mirrors the ゆく/いく case).
        if (!isAdjectiveInDictionary(dict_manager, cand.base_form)) {
          if (containsAuxiliaryFinalParticleBoundary(codepoints, start_pos, end_pos, dict_manager) ||
              endsWithMultiMoraFinalParticle(codepoints, start_pos, end_pos, dict_manager)) {
            SUZUME_DEBUG_LOG_VERBOSE("[ADJ_SKIP] \"" << surface << "\" crosses auxiliary + final-particle boundary\n");
            continue;
          }
          std::string_view base_sv(cand.base_form);
          if (base_sv.size() > core::kJapaneseCharBytes && utf8::endsWith(base_sv, "い")) {
            std::string_view stem = base_sv.substr(0, base_sv.size() - core::kJapaneseCharBytes);
            char32_t stem_last = utf8::decodeFirstChar(utf8::lastChar(stem));
            if (stem_last != 0 && kana::isHiraganaCodepoint(stem_last)) {
              bool tail_is_dict_verb = false;
              for (const auto& vres : inflection.analyze(hiragana_part)) {
                if (vres.verb_type == grammar::VerbType::IAdjective) {
                  continue;
                }
                if (isVerbInDictionary(dict_manager, vres.base_form)) {
                  tail_is_dict_verb = true;
                  break;
                }
              }
              if (tail_is_dict_verb) {
                SUZUME_DEBUG_LOG_VERBOSE("[ADJ_SKIP] \"" << surface
                                                         << "\" tail is dict verb, skipping fake adjective\n");
                continue;
              }
            }
          }
        }
        // A dictionary-verified sokuonbin verb followed by た is not an
        // i-adjective past.  Inflection can otherwise fabricate an adjective
        // base solely from the shared かった ending (見つかっ+た → 見つい),
        // even though the preceding っ is already a productive verb form.
        // The dictionary gate preserves genuine open-class adjectives while
        // rejecting this structural homograph for every verified verb.
        if (!isAdjectiveInDictionary(dict_manager, cand.base_form) && utf8::endsWith(surface, "かった") &&
            surface.size() > core::kJapaneseCharBytes) {
          std::string sokuonbin_surface = surface.substr(0, surface.size() - core::kJapaneseCharBytes);
          if (isVerbInDictionary(dict_manager, sokuonbin_surface)) {
            SUZUME_DEBUG_LOG_VERBOSE("[ADJ_SKIP] \"" << surface
                                                     << "\" has dictionary sokuonbin verb, skipping fake adjective\n");
            continue;
          }
        }
        // Skip a span that merely prefixes a complete dictionary adjective.
        // A whole-span candidate over [modifier][dictionary adjective] fabricates
        // a lemma for a compound nobody wrote (超難しい, 激冷たい) and destroys the
        // adjective as a search unit; the material before it is its own word.
        // Lexicalized compounds are dictionary entries themselves (力強い, 心細い)
        // and are exempt, as is the case where the adjective spans everything.
        // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
        if (!isAdjectiveInDictionary(dict_manager, cand.base_form)) {
          bool prefixes_dictionary_adjective = false;
          for (size_t tail_start = start_pos + 1; tail_start < end_pos; ++tail_start) {
            for (const auto& tail_res : inflection.analyze(extractSubstring(codepoints, tail_start, end_pos))) {
              if (tail_res.verb_type == grammar::VerbType::IAdjective &&
                  isAdjectiveInDictionary(dict_manager, tail_res.base_form) &&
                  !isCompoundFormingAdjective(tail_res.base_form)) {
                prefixes_dictionary_adjective = true;
                break;
              }
            }
            if (prefixes_dictionary_adjective) {
              break;
            }
          }
          if (prefixes_dictionary_adjective) {
            SUZUME_DEBUG_LOG_VERBOSE("[ADJ_SKIP] \"" << surface << "\" ends on a dictionary adjective\n");
            continue;
          }
        }
        // Skip subsidiary-verb ゆく/いく compounds misread as i-adjectives.
        // Verb 連用形 + ゆく (散りゆく, 消えゆく) ends in く, so inflection
        // hypothesizes a fake i-adjective base (散りゆい). When the base is
        // not a dictionary adjective and the part before ゆく/いく is itself
        // a dictionary verb form, this is the compound-verb construction —
        // leave it to the verb paths (散り + ゆく).
        if (surface.size() > 2 * core::kJapaneseCharBytes &&
            (utf8::endsWith(surface, "ゆく") || utf8::endsWith(surface, "いく")) &&
            !isAdjectiveInDictionary(dict_manager, cand.base_form)) {
          std::string v1_prefix = surface.substr(0, surface.size() - 2 * core::kJapaneseCharBytes);
          // The prefix is a verb 連用形 when it is a dictionary surface itself
          // (散り) or when inflection confidently reconstructs a verb from it
          // (消え → 消える, 過ぎ → 過ぎる). Dictionary verification lowers the
          // bar; a confident inflection hypothesis alone is also accepted since
          // the competing i-adjective base (Xゆい) is already known to be fake.
          bool prefix_is_verb = verb_helpers::hasDictionaryEntry(dict_manager, v1_prefix, core::PartOfSpeech::Verb);
          if (!prefix_is_verb) {
            // Low bar: the preconditions (ゆく/いく ending, fake adjective base)
            // already exclude real adjectives, so any plausible verb hypothesis
            // (消え → 消える 0.74, 暮れ → 暮れる 0.3 after e-row ambiguity
            // penalty) marks the prefix as a 連用形.
            const auto& v1_results = inflection.analyze(v1_prefix);
            for (const auto& v1_res : v1_results) {
              if (v1_res.verb_type == grammar::VerbType::IAdjective) {
                continue;
              }
              if (isVerbInDictionary(dict_manager, v1_res.base_form) ||
                  v1_res.confidence >= candidate::kV1PrefixMinConfidence) {
                prefix_is_verb = true;
                break;
              }
            }
          }
          if (prefix_is_verb) {
            SUZUME_DEBUG_LOG_VERBOSE("[ADJ_SKIP] \"" << surface << "\" is verb renyokei + subsidiary ゆく/いく\n");
            continue;  // Skip - compound verb, not adjective
          }
        }
        // Skip a verb plus the negative auxiliary ない misread as one adjective.
        // The negative ends in い, so inflection hypothesizes an adjective base
        // spelled exactly like the surface (借りない, 足りない, 錆びない) and that
        // fabrication outscores the 未然形 + ない split for any verb the
        // dictionary does not carry — which makes the reading depend on lexical
        // coverage rather than on form. Lexical ない-adjectives are dictionary
        // entries (少ない, 情けない, もったいない) and keep their reading; so does a
        // registered adjective's own negative, whose base is the adjective.
        if (surface.size() > 2 * core::kJapaneseCharBytes && utf8::endsWith(surface, "ない") &&
            !isAdjectiveInDictionary(dict_manager, cand.base_form)) {
          const std::string negated = surface.substr(0, surface.size() - 2 * core::kJapaneseCharBytes);
          bool negated_is_verb = verb_helpers::hasDictionaryEntry(dict_manager, negated, core::PartOfSpeech::Verb);
          if (!negated_is_verb) {
            for (const auto& negated_res : inflection.analyze(negated)) {
              if (negated_res.verb_type == grammar::VerbType::IAdjective) {
                continue;
              }
              if (isVerbInDictionary(dict_manager, negated_res.base_form) ||
                  negated_res.confidence >= candidate::kV1PrefixMinConfidence) {
                negated_is_verb = true;
                break;
              }
            }
          }
          if (negated_is_verb) {
            SUZUME_DEBUG_LOG_VERBOSE("[ADJ_SKIP] \"" << surface << "\" is verb + negative ない\n");
            continue;
          }
        }
        // Skip さそう endings (adj nominalization + appearance auxiliary)
        // E.g., 気持ちよさそうに → 気持ちよ + さ + そう + に
        //        なさそう → な + さ + そう (handled separately in hiragana adj)
        // adj-stem + さ(nominalizer) + そう(appearance) should be split
        if (surface.size() >= 3 * core::kJapaneseCharBytes) {
          if (utf8::endsWith(surface, "さそう") || utf8::endsWith(surface, "さそうに") ||
              utf8::endsWith(surface, "さそうな") || utf8::endsWith(surface, "さそうだ")) {
            continue;  // Skip - force adj + さ + そう split
          }
        }
        // Set lemma to base form from inflection analysis (e.g., 使いやすく → 使いやすい)
        auto adj_cand = makeIAdjCandidate(surface, start_pos, end_pos, cand.base_form, cost,
                                          CandidateOrigin::AdjectiveI, cand.confidence, "i_adjective");
        adj_cand.has_suffix = ga_mashii_derivation || meka_shii_derivation;
        // Note: 2-kanji stem compound adjectives (薄暗い, 物悲しく) need
        // has_suffix to skip exceeds_dict_length penalty. This is handled
        // in the compound adjective section below (with tighter guards).
        candidates.push_back(std::move(adj_cand));
        break;  // Only add one adjective candidate per surface
      }
    }
  }

  adj_detail::appendKanjiCompoundIAdjCandidates(codepoints, start_pos, kanji_end, hiragana_end, inflection,
                                                dict_manager, candidates, candidate_start);

  adj_detail::appendKanjiIAdjPostVariants(codepoints, start_pos, kanji_end, hiragana_end, inflection, dict_manager,
                                          candidates, candidate_start);
  verb_helpers::sortCandidatesByCost(candidates, candidate_start);

  return;
}

void generateGaMashiiHostAdjectiveCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                             const std::vector<normalize::CharType>& char_types,
                                             const grammar::Inflection& inflection,
                                             std::vector<UnknownCandidate>& candidates) {
  const size_t noun_candidate_count = candidates.size();
  for (size_t candidate_idx = 0; candidate_idx < noun_candidate_count; ++candidate_idx) {
    const auto& host = candidates[candidate_idx];
    if (host.start != start_pos || host.pos != core::PartOfSpeech::Noun ||
        host.end + kGaMashiiStemLength >= codepoints.size() ||
        extractSubstring(codepoints, host.end, host.end + kGaMashiiStemLength) != kGaMashiiStem) {
      continue;
    }

    const size_t hiragana_end =
        findCharRegionEnd(char_types, host.end, kMaxGaMashiiInflectionLength, normalize::CharType::Hiragana);
    for (size_t end_pos = hiragana_end; end_pos > host.end + kGaMashiiStemLength; --end_pos) {
      const std::string surface = extractSubstring(codepoints, start_pos, end_pos);
      const auto& inflection_candidates = inflection.analyze(surface);
      const auto inflection_candidate =
          std::find_if(inflection_candidates.begin(), inflection_candidates.end(), [](const auto& candidate) {
            return candidate.verb_type == grammar::VerbType::IAdjective &&
                   candidate.confidence >= candidate::kDerivedSuffixAdjectiveConfidence &&
                   utf8::endsWith(candidate.base_form, "がましい");
          });
      if (inflection_candidate == inflection_candidates.end()) {
        continue;
      }

      // The derivational host may be followed by the productive nominalizer
      // さ. Inflection analysis accepts the full nominalized span as an
      // i-adjective form, but the tokenizer must keep the adjective stem and
      // suffix as separate search units.
      const bool ends_with_nominalizer = grammar::isSingleHiragana(inflection_candidate->suffix, core::hiragana::kSa);
      const size_t adjective_end = ends_with_nominalizer ? end_pos - 1 : end_pos;
      const std::string adjective_surface = extractSubstring(codepoints, start_pos, adjective_end);
      const bool already_generated =
          std::any_of(candidates.begin(), candidates.end(), [&](const UnknownCandidate& candidate) {
            return candidate.start == start_pos && candidate.end == adjective_end &&
                   candidate.pos == core::PartOfSpeech::Adjective && candidate.lemma == inflection_candidate->base_form;
          });
      if (!already_generated) {
        auto adjective = makeIAdjCandidate(adjective_surface, start_pos, adjective_end, inflection_candidate->base_form,
                                           candidate::kDerivedSuffixAdjectiveCost, CandidateOrigin::AdjectiveI,
                                           inflection_candidate->confidence, "ga_mashii_nominal_host");
        adjective.has_suffix = true;
        candidates.push_back(std::move(adjective));
      }
      break;
    }
  }
}

}  // namespace suzume::analysis
