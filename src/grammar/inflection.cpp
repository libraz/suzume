/**
 * @file inflection.cpp
 * @brief Connection-based reverse inflection analysis implementation
 */

#include "inflection.h"

#include <algorithm>
#include <utility>

#include "char_patterns.h"
#include "core/debug.h"
#include "core/utf8_constants.h"
#include "inflection_scorer.h"
#include "verb_endings.h"

namespace suzume::grammar {

namespace {

// Check if auxiliary starts with voiced te-form (で/だ)
// startsWith already covers the bare で/だ case, so no separate equalsAny check.
inline bool isVoicedAux(std::string_view aux) {
  return utf8::startsWith(aux, "で") || utf8::startsWith(aux, "だ");
}

// Check if auxiliary starts with unvoiced te-form (て/た)
inline bool isUnvoicedAux(std::string_view aux) {
  return utf8::startsWith(aux, "て") || utf8::startsWith(aux, "た");
}

void stableSortByConfidence(std::vector<InflectionCandidate>& candidates) {
  for (size_t idx = 1; idx < candidates.size(); ++idx) {
    InflectionCandidate candidate = std::move(candidates[idx]);
    size_t insert_at = idx;
    while (insert_at > 0 && candidates[insert_at - 1].confidence < candidate.confidence) {
      candidates[insert_at] = std::move(candidates[insert_at - 1]);
      --insert_at;
    }
    candidates[insert_at] = std::move(candidate);
  }
}

}  // namespace

std::vector<std::pair<const AuxiliaryEntry*, size_t>> Inflection::matchAuxiliaries(std::string_view surface) const {
  std::vector<std::pair<const AuxiliaryEntry*, size_t>> matches;
  matches.reserve(8);  // Typical max matches
  const auto& auxiliaries = getAuxiliaries();

  for (const auto& aux : auxiliaries) {
    if (surface.size() >= aux.surface.size()) {
      size_t start = surface.size() - aux.surface.size();
      if (surface.substr(start) == aux.surface) {
        matches.emplace_back(&aux, aux.surface.size());
        SUZUME_DEBUG_LOG_TRACE("  [AUX MATCH] \"" << surface << "\" ends with \"" << aux.surface << "\" (right_id=0x"
                                                  << std::hex << aux.right_id << ", requires=0x" << aux.required_conn
                                                  << std::dec << ")\n");
      }
    }
  }

  return matches;
}

std::vector<InflectionCandidate> Inflection::matchVerbStem(std::string_view remaining,
                                                           const std::vector<std::string>& aux_chain,
                                                           uint16_t required_conn) const {
  std::vector<InflectionCandidate> candidates;
  candidates.reserve(16);  // Typical max candidates
  const VerbEndingRange endings = getVerbEndingsByConn(required_conn);
  if (endings.empty()) {
    return candidates;
  }

  for (const auto& ending : endings) {
    // Check if remaining ends with this verb ending
    if (remaining.size() < ending.suffix.size()) {
      continue;
    }

    bool matches = true;
    if (!ending.suffix.empty()) {
      size_t start = remaining.size() - ending.suffix.size();
      matches = (remaining.substr(start) == ending.suffix);
    }

    if (matches) {
      // Extract stem
      std::string stem(remaining);
      if (!ending.suffix.empty()) {
        stem = std::string(remaining.substr(0, remaining.size() - ending.suffix.size()));
      }

      // Native カ変 endings are the kana sequence itself (こ/き/くれ/...),
      // so their lexical stem must be empty. A non-empty non-来 prefix is never
      // カ変, and 来 + a kana ending would be an invalid mixed spelling
      // (*来きます). Kanji 来 forms are handled by the existing zero-ending
      // Ichidan match and remapped to Kuru below.
      if (ending.verb_type == VerbType::Kuru && (!isKuruStem(stem) || !stem.empty())) {
        continue;
      }

      // Classical サ変未然形 せ is licensed by the classical negative
      // auxiliaries ず/ぬ (屈せず, 屈せぬ), not by modern ない compounds.
      // Without this guard, 話せなくなる is misread as 話する +
      // せなくなる and ties the genuine potential form 話す + せる.
      if (ending.verb_type == VerbType::Suru && ending.suffix == "せ" &&
          (aux_chain.empty() ||
           !(utf8::startsWith(aux_chain.back(), "ず") || utf8::startsWith(aux_chain.back(), "ぬ")))) {
        continue;
      }

      // Stem should be at least 3 bytes (one Japanese character)
      // Exceptions for irregular verbs where the suffix IS the conjugated form:
      // - Suru verb with す/し→する (empty stem is allowed)
      // - Suru verb with しろ/せよ→する (imperative, empty stem allowed)
      // - Suru verb with しよ→する (volitional, empty stem allowed)
      // - Suru verb with すれ→する (hypothetical, empty stem allowed)
      // - Kuru verb with こ/き→くる (empty stem is allowed for mizenkei/renyokei)
      // - Kuru verb with こい→くる (imperative, empty stem allowed)
      // - Kuru verb with くれ→くる (hypothetical, empty stem allowed)
      // - Kuru verb with こよ→くる (volitional, empty stem allowed)
      // - Kuru verb with くる→くる (dictionary form, empty stem allowed)
      // NOTE: Suru with empty suffix + empty stem + aux is NOT allowed
      // (e.g., なかった should NOT become する)
      // Valid Suru patterns require non-empty suffix (し, さ) to connect to aux
      if (stem.size() < core::kJapaneseCharBytes &&
          !(ending.verb_type == VerbType::Suru &&
            (ending.suffix == "す" || ending.suffix == "し" || ending.suffix == "しろ" || ending.suffix == "せよ" ||
             ending.suffix == "しよ" || ending.suffix == "すれ")) &&
          !(ending.verb_type == VerbType::Kuru &&
            (ending.suffix == "こ" || ending.suffix == "き" || ending.suffix == "こい" || ending.suffix == "くれ" ||
             ending.suffix == "こよ" || ending.suffix == "くる"))) {
        continue;
      }

      // Skip Ichidan with empty suffix when no auxiliaries matched
      // (prevents "書いて" from being parsed as Ichidan "書いてる")
      // Exception: Allow standalone renyokei matching for verb + AUX patterns
      // (e.g., 食べ + ます, 見 + ましょう) - these connect to dictionary AUX
      if (ending.suffix.empty() && aux_chain.empty() && ending.verb_type == VerbType::Ichidan &&
          required_conn != conn::kVerbRenyokei) {
        continue;
      }

      // Reject stems starting with て (hiragana)
      // Japanese verb stems never start with て - it's always a te-form particle
      // This prevents てあげる from being parsed as a verb (should be て + あげる)
      // Real verbs with te-sound use kanji: 照る (teru), 出る (deru)
      if (utf8::startsWith(stem, "て")) {
        continue;
      }

      // Reject i-adjective stems ending in だ (copula)
      // No valid i-adjective stem ends in だ - it's always noun+copula
      // This prevents 教師だそうだ → 教師だい (false i-adj match)
      if (ending.verb_type == VerbType::IAdjective && utf8::endsWith(stem, "だ")) {
        continue;
      }

      // Validate irregular いく pattern: GodanKa with っ-onbin only valid for い/行
      if (ending.verb_type == VerbType::GodanKa && ending.suffix == "っ" && ending.is_onbin) {
        if (!isIkuStem(stem)) {
          continue;  // Skip invalid irregular pattern
        }
      }

      // Validate onbin voicing: a Godan verb's te/ta-form voicing must match its
      // onbin type, uniformly across い/っ/ん onbin (GodanRow::voiced_ta).
      // Unvoiced onbin (書い/立っ/買っ → GodanKa/Ta/Ra/Wa) requires て/た.
      // Voiced onbin (泳い/読ん/飛ん → GodanGa/Ma/Ba/Na) requires で/だ.
      // This rejects mismatches like 立っだ (→立つ) and 読んた (→読む).
      if (ending.is_onbin && !aux_chain.empty()) {
        const Conjugation::GodanRow* row = Conjugation::getGodanRow(ending.verb_type);
        if (row != nullptr) {
          const std::string& first_aux = aux_chain.back();  // First matched aux
          bool is_voiced_aux = isVoicedAux(first_aux);
          bool is_unvoiced_aux = isUnvoicedAux(first_aux);
          if (row->voiced_ta && is_unvoiced_aux) {
            continue;  // Voiced onbin verb requires voiced aux (で/だ), skip unvoiced
          }
          if (!row->voiced_ta && is_voiced_aux) {
            continue;  // Unvoiced onbin verb requires unvoiced aux (て/た), skip voiced
          }
        }
      }

      // Validate Ichidan: reject voiced te-form (で/だ)
      // Ichidan verb te-form is ALWAYS 連用形+て, never 連用形+で
      // で/だ te-form only occurs with Godan verbs after onbin (読んで, 泳いだ)
      // Pattern: 付けで → should be 付け(NOUN)+で(PARTICLE), not 付ける+で
      // Pattern: 食べだ is INVALID, 食べた is correct
      if (ending.verb_type == VerbType::Ichidan && !aux_chain.empty()) {
        const std::string& first_aux = aux_chain.back();  // First matched aux
        if (isVoicedAux(first_aux)) {
          continue;  // Ichidan requires unvoiced aux (て/た), skip voiced で/だ
        }
      }

      // Validate Ichidan: reject stems that would create irregular verb base forms
      // くる (来る) is Kuru verb, not Ichidan. Stem く + る = くる is INVALID.
      // する is Suru verb, not Ichidan. Stem す + る = する is INVALID.
      // こる is not a valid verb - こ is Kuru mizenkei suffix, not Ichidan stem.
      // This prevents くなかった from being parsed as Ichidan く + なかった = くる
      // Note: 来 (kanji) is handled separately - 来なかった should become 来る (Kuru)
      if (ending.verb_type == VerbType::Ichidan && stem.size() == core::kJapaneseCharBytes) {
        if (utf8::equalsAny(stem, {"く", "す", "こ"})) {
          continue;  // Skip - these are irregular verbs (hiragana), not Ichidan
        }
      }

      // Validate Ichidan: reject stems ending with small tsu (っ)
      // Ichidan verbs do NOT have onbin (音便) forms, so stems never end with っ
      // Stems ending with っ are always from Godan verbs (知る→知っ, 買う→買っ, 持つ→持っ)
      // This prevents 買っ from being parsed as Ichidan 買っる
      if (ending.verb_type == VerbType::Ichidan && utf8::endsWith(stem, "っ")) {
        continue;  // Skip - Ichidan stems never end with っ
      }

      // A kanji カ変 stem inflects through the same visible cells as an ichidan
      // stem, so reverse analysis reaches it through the ichidan endings and
      // has to remap the verb type. The empty stem is excluded: that is the
      // kana spelling, which never arrives here as an ichidan match.
      VerbType actual_verb_type = ending.verb_type;
      std::string actual_base_suffix = ending.base_suffix;
      if (ending.verb_type == VerbType::Ichidan && !stem.empty() && isKuruStem(stem)) {
        actual_verb_type = VerbType::Kuru;
        // For Kuru, base form is 来る (stem + る)
        actual_base_suffix = "る";
      }

      // Suru verb stems should not contain particles like で, に, etc.
      // This prevents "本でし" from being parsed as Suru verb "本でする"
      // Valid suru stems are typically all-kanji (勉強, 検討) or katakana loan words
      if (ending.verb_type == VerbType::Suru && !stem.empty()) {
        // Check if stem ends with common particles/hiragana that shouldn't be in suru stems
        bool invalid_stem = false;
        if (stem.size() >= core::kJapaneseCharBytes) {
          std::string_view last_char = utf8::lastChar(stem);
          // These hiragana at the end of stem indicate a particle or non-suru pattern
          if (utf8::equalsAny(last_char, {"で", "に", "を", "が", "は", "も", "と", "へ", "か", "や", "の"})) {
            invalid_stem = true;
          }
          // Reject suru stems containing て/で (te-form markers) anywhere in longer stems
          // E.g., "基づいて処理" should be 基づいて(verb) + 処理する, not a single suru verb
          // This check applies to stems >= 9 bytes (3+ characters) to avoid false positives
          if (stem.size() >= core::kThreeJapaneseCharBytes && utf8::containsAny(stem, {"て", "で"})) {
            invalid_stem = true;
          }
          // For empty suffix suru patterns (e.g., 開催+された), the stem must
          // NOT end with hiragana that could be part of verb conjugations.
          // This prevents 奪われた → 奪わ+された → 奪わする (wrong)
          // Valid suru stems are all-kanji (開催) or katakana (ドライブ)
          if (ending.suffix.empty() && !aux_chain.empty()) {
            // A-row endings are common godan mizenkei; e-row endings are common
            // potential/ichidan stems. Either means the "stem" is really a verb
            // conjugation, so the empty-suffix suru reading is invalid.
            // (prevents 奪われた → 奪わする, 話せなくなった → 話せする, etc.)
            if (endsWithARow(last_char) || endsWithERow(last_char)) {
              invalid_stem = true;
            }
            // Single-kanji stems are NOT valid for empty suffix suru patterns
            // Real suru verb stems have 2+ kanji (開催, 勉強, 検討)
            // This prevents 見+られた → 見する (wrong, should be 見る Ichidan)
            if (stem.size() <= core::kJapaneseCharBytes) {
              invalid_stem = true;
            }
          }
        }
        if (invalid_stem) {
          continue;  // Skip suru verbs with particle-like endings
        }
      }

      // Build base form (use actual_base_suffix for special cases like 来→来る)
      std::string base_form = stem + actual_base_suffix;

      // Build suffix chain string
      std::string suffix_str = ending.suffix;
      for (auto iter = aux_chain.rbegin(); iter != aux_chain.rend(); ++iter) {
        suffix_str += *iter;
      }

      // Calculate total auxiliary length
      size_t aux_total_len = 0;
      for (const auto& aux : aux_chain) {
        aux_total_len += aux.size();
      }
      const bool first_aux_starts_with_te_de =
          !aux_chain.empty() && (utf8::startsWith(aux_chain.back(), "て") || utf8::startsWith(aux_chain.back(), "で"));

      InflectionCandidate candidate;
      candidate.base_form = base_form;
      candidate.stem = stem;
      candidate.suffix = suffix_str;
      candidate.verb_type = actual_verb_type;  // Use remapped type for 来→Kuru
      candidate.confidence = calculateConfidence(actual_verb_type, stem, aux_total_len, aux_chain.size(), required_conn,
                                                 suffix_str.size(), first_aux_starts_with_te_de, &scorer_options_);

      // Ichidan verbs use て/た for te/ta-form, NOT で/だ
      // で/だ are only used for 撥音便 Godan verbs (読む→読んで/読んだ, 遊ぶ→遊んで/遊んだ)
      // Penalize Ichidan + で/だ combinations heavily
      if (actual_verb_type == VerbType::Ichidan && suffix_str.size() >= core::kJapaneseCharBytes) {
        // Use string_view::substr to avoid creating temporary std::string
        std::string_view suffix_view(suffix_str);
        std::string_view first_char = suffix_view.substr(0, core::kJapaneseCharBytes);
        if (utf8::equalsAny(first_char, {"で", "だ"})) {
          candidate.confidence -= 0.6F;  // Strong penalty
          SUZUME_DEBUG_LOG_VERBOSE("  ichidan_voiced_te_ta_invalid: -0.6\n");
        }
      }

      // Contracted progressive past: 見てた, 食べてた should split as 見+て+た, 食べ+て+た
      // MeCab splits these, so penalize single-token analysis with suffix starting with てた/でた
      // This ensures 見 + て(VERB) + た(AUX) path wins over 見てた(VERB)
      if (actual_verb_type == VerbType::Ichidan && suffix_str.size() >= core::kTwoJapaneseCharBytes) {
        // Use starts_with for safer comparison
        if (suffix_str.rfind("てた", 0) == 0 || suffix_str.rfind("でた", 0) == 0) {
          candidate.confidence -= inflection::kPenaltyIchidanContractedProgressivePast;
          SUZUME_DEBUG_LOG_VERBOSE("  ichidan_contracted_progressive_past: -"
                                   << inflection::kPenaltyIchidanContractedProgressivePast << "\n");
        }
      }

      // Contracted progressive verb stem (〜て/〜で ending as stem of 〜てる/〜でる)
      // 見て + た → base=見てる is wrong; should be 見 + て + た
      // Penalize stems ending in て/で that are analyzed as 〜てる/〜でる conjugations
      if (actual_verb_type == VerbType::Ichidan && stem.size() >= core::kTwoJapaneseCharBytes) {
        // Check last character of stem (use std::string_view on stem directly)
        std::string_view stem_view(stem);
        std::string_view stem_end = stem_view.substr(stem_view.size() - core::kJapaneseCharBytes);
        if (isTeDeSurface(stem_end)) {
          // Check if this is analyzing as 〜てる/〜でる form
          // These stems (見て, 食べて) are te-forms, not contracted progressive stems
          candidate.confidence -= inflection::kPenaltyIchidanContractedProgressivePast;
          SUZUME_DEBUG_LOG_VERBOSE("  ichidan_contracted_progressive_stem: -"
                                   << inflection::kPenaltyIchidanContractedProgressivePast << "\n");
        }
      }

      // Contracted progressive for ALL verb types: suffix ending in てる/てた/でる/でた
      // 知ってる, 食べてる, してる should all split as 音便/連用形 + てる
      // MeCab always splits these, so penalize single-token analysis
      // E.g., 知ってる → 知っ + てる, 食べてる → 食べ + てる
      if (suffix_str.size() >= core::kTwoJapaneseCharBytes) {
        std::string_view suffix_view(suffix_str);
        // Check if suffix ends with てる, てた, でる, でた
        std::string_view suffix_end = suffix_view.substr(suffix_view.size() - core::kTwoJapaneseCharBytes);
        if (utf8::equalsAny(suffix_end, {"てる", "てた", "でる", "でた"})) {
          candidate.confidence -= 0.8F;  // Strong penalty to prefer split
          SUZUME_DEBUG_LOG_VERBOSE("  contracted_progressive_ending: -0.8\n");
        }
      }

      candidate.morphemes = aux_chain;

      SUZUME_DEBUG_LOG_TRACE("  [STEM MATCH] \"" << remaining << "\" → base=\"" << base_form << "\" stem=\"" << stem
                                                 << "\" type=" << static_cast<int>(actual_verb_type) << " suffix=\""
                                                 << suffix_str << "\" conf=" << candidate.confidence << "\n");

      candidates.push_back(std::move(candidate));
    }
  }

  return candidates;
}

std::vector<InflectionCandidate> Inflection::analyzeWithAuxiliaries(std::string_view surface,
                                                                    std::vector<std::string>& aux_chain,
                                                                    uint16_t required_conn) const {
  std::vector<InflectionCandidate> candidates;
  candidates.reserve(32);  // Typical max candidates

  // Try to find more auxiliaries
  auto matches = matchAuxiliaries(surface);

  for (const auto& [aux, len] : matches) {
    // Check if this auxiliary can connect to what we need
    if (aux->right_id != required_conn) {
      continue;
    }

    // Recursively analyze remaining (push/pop to avoid copying aux_chain)
    std::string_view remaining = surface.substr(0, surface.size() - len);
    aux_chain.push_back(aux->surface);

    auto sub_candidates = analyzeWithAuxiliaries(remaining, aux_chain, aux->required_conn);
    for (auto& cand : sub_candidates) {
      candidates.push_back(std::move(cand));
    }

    aux_chain.pop_back();
  }

  // Also try to match verb stem directly
  auto stem_candidates = matchVerbStem(surface, aux_chain, required_conn);
  for (auto& cand : stem_candidates) {
    candidates.push_back(std::move(cand));
  }

  return candidates;
}

const std::vector<InflectionCandidate>& Inflection::analyze(std::string_view surface) const {
  // Check cache first
  std::string key(surface);
  auto cache_iter = cache_.find(key);
  if (cache_iter != cache_.end()) {
    SUZUME_DEBUG_LOG_TRACE("[INFLECTION] \"" << surface << "\" (cached, " << cache_iter->second.size()
                                             << " candidates)\n");
    return cache_iter->second;
  }
  auto previous_iter = previous_.find(key);
  if (previous_iter != previous_.end()) {
    SUZUME_DEBUG_LOG_TRACE("[INFLECTION] \"" << surface << "\" (previous generation, " << previous_iter->second.size()
                                             << " candidates)\n");
    return previous_iter->second;
  }

  SUZUME_DEBUG_LOG_VERBOSE("[INFLECTION] Analyzing \"" << surface << "\"\n");

  std::vector<InflectionCandidate> candidates;

  // Early return for very short strings (less than 2 Japanese characters)
  // A conjugated verb needs at least stem + ending
  if (surface.size() < core::kTwoJapaneseCharBytes) {  // 2 Japanese chars = 6 bytes in UTF-8
    rollCacheIfFull();
    auto [iter, inserted] = cache_.emplace(std::move(key), std::move(candidates));
    return iter->second;
  }

  // First, try to match auxiliaries from the end
  auto matches = matchAuxiliaries(surface);

  for (const auto& [aux, len] : matches) {
    std::string_view remaining = surface.substr(0, surface.size() - len);
    std::vector<std::string> aux_chain{aux->surface};

    auto sub_candidates = analyzeWithAuxiliaries(remaining, aux_chain, aux->required_conn);
    for (auto& cand : sub_candidates) {
      candidates.push_back(std::move(cand));
    }
  }

  // Try stripping explanatory のだ/んだ suffixes for inflection analysis.
  // Results are marked with has_explanatory_suffix so candidate generators
  // can skip them — のだ/んだ should be separate tokens (検索単位の原則).
  static constexpr std::string_view kExplanatorySuffixes[] = {
      "んだ", "のだ", "んです", "のです", "んだもの", "んだもん",
  };
  for (const auto& suffix : kExplanatorySuffixes) {
    if (surface.size() > suffix.size() && surface.substr(surface.size() - suffix.size()) == suffix) {
      auto remaining = surface.substr(0, surface.size() - suffix.size());
      // Recursively analyze what's left (it should be a verb in base form)
      const auto& sub = analyze(remaining);
      for (const auto& cand : sub) {
        if (cand.confidence >= 0.5F) {
          auto boosted = cand;
          // Boost confidence: explanatory suffix is a strong signal
          boosted.confidence = std::max(cand.confidence, 0.8F);
          boosted.has_explanatory_suffix = true;
          candidates.push_back(boosted);
        }
      }
    }
  }

  // Also try direct verb stem matching (for base forms and standalone renyokei)
  // Match base form (e.g., 分割する)
  auto base_candidates = matchVerbStem(surface, {}, conn::kVerbBase);
  for (auto& cand : base_candidates) {
    candidates.push_back(std::move(cand));
  }

  // Match renyokei (e.g., 分割し) - used when verb connects to another phrase
  auto renyokei_candidates = matchVerbStem(surface, {}, conn::kVerbRenyokei);
  for (auto& cand : renyokei_candidates) {
    candidates.push_back(std::move(cand));
  }

  // Note: Godan imperative forms (e.g., 書け, 読め) are NOT matched standalone
  // because they can conflict with conditional forms:
  // - 読めば could be split as 読め + ば (where 読め matches GodanMa meireikei)
  // - Similarly for other e-row endings: け, せ, れ, げ, etc.
  //
  // However, these imperative forms ARE safe to match:
  // - Suru (しろ, せよ): する conditional is すれば (no overlap)
  // - Ichidan (食べろ, 起きろ): conditional is 食べれば (no ろ/よ forms)
  // - Kuru (こい): くる conditional is くれば (no overlap)
  auto meireikei_candidates = matchVerbStem(surface, {}, conn::kVerbMeireikei);
  for (auto& cand : meireikei_candidates) {
    // Include Suru, Ichidan, and Kuru imperatives (safe patterns)
    // Exclude Godan imperatives to avoid conditional form regression
    if (cand.verb_type == VerbType::Suru || cand.verb_type == VerbType::Ichidan || cand.verb_type == VerbType::Kuru) {
      candidates.push_back(std::move(cand));
    }
  }
  // Godan imperatives are handled via:
  // 1. Dictionary lookup (やめろ, etc.)
  // 2. Auxiliary chain matching for compound patterns

  // Sort by confidence (descending)
  // Preserve the original order for candidates with equal confidence.
  stableSortByConfidence(candidates);

  // Remove duplicates (same base_form and verb_type)
  auto dup_end = std::unique(candidates.begin(), candidates.end(),
                             [](const InflectionCandidate& lhs, const InflectionCandidate& rhs) {
                               return lhs.base_form == rhs.base_form && lhs.verb_type == rhs.verb_type;
                             });
  candidates.erase(dup_end, candidates.end());

  // Debug: print final candidates (level 1: top candidate only, level 2: all)
  SUZUME_DEBUG_IF(!candidates.empty() && candidates[0].confidence >= 0.5F) {
    SUZUME_DEBUG_STREAM << "[INFLECTION] \"" << surface << "\" → " << candidates[0].base_form << " ("
                        << verbTypeToString(candidates[0].verb_type) << ", conf=" << candidates[0].confidence << ")\n";
  }
  SUZUME_DEBUG_TRACE_BLOCK {
    if (!candidates.empty()) {
      SUZUME_DEBUG_STREAM << "[INFLECTION] Full results for \"" << surface << "\":\n";
      for (size_t i = 0; i < candidates.size() && i < 5; ++i) {
        const auto& c = candidates[i];
        SUZUME_DEBUG_STREAM << "  " << (i + 1) << ". base=\"" << c.base_form << "\" " << verbTypeToString(c.verb_type)
                            << " conf=" << c.confidence << "\n";
      }
      if (candidates.size() > 5) {
        SUZUME_DEBUG_STREAM << "  ... and " << (candidates.size() - 5) << " more\n";
      }
    }
  }

  // Cache the result. Return a reference to the cached entry — safe because
  // unordered_map references survive later inserts. Before an active generation
  // grows past its bound, move it to previous_; its references remain valid
  // while the next generation starts accumulating entries.
  rollCacheIfFull();
  auto [iter, inserted] = cache_.emplace(std::move(key), std::move(candidates));
  return iter->second;
}

void Inflection::rollCache() const {
  rollCacheIfFull();
}

void Inflection::rollCacheIfFull() const {
  if (cache_.size() >= kMaxCacheEntries) {
    previous_.clear();
    previous_.swap(cache_);
  }
}

bool Inflection::looksConjugated(std::string_view surface) const {
  return !analyze(surface).empty();
}

InflectionCandidate Inflection::getBest(std::string_view surface) const {
  const auto& candidates = analyze(surface);
  if (candidates.empty()) {
    return {};
  }
  return candidates.front();
}

}  // namespace suzume::grammar
