/**
 * @file inflection.cpp
 * @brief Connection-based reverse inflection analysis implementation
 */

#include "inflection.h"

#include <algorithm>

#include "core/debug.h"
#include "inflection_scorer.h"
#include "verb_endings.h"

namespace suzume::grammar {

std::vector<std::pair<const AuxiliaryEntry*, size_t>>
Inflection::matchAuxiliaries(std::string_view surface) const {
  std::vector<std::pair<const AuxiliaryEntry*, size_t>> matches;
  matches.reserve(8);  // Typical max matches
  const auto& auxiliaries = getAuxiliaries();

  for (const auto& aux : auxiliaries) {
    if (surface.size() >= aux.surface.size()) {
      size_t start = surface.size() - aux.surface.size();
      if (surface.substr(start) == aux.surface) {
        matches.emplace_back(&aux, aux.surface.size());
        SUZUME_DEBUG_AUX("  [AUX MATCH] \"" << surface << "\" ends with \""
                         << aux.surface << "\" (lemma=" << aux.lemma
                         << ", left_id=0x" << std::hex << aux.left_id
                         << ", right_id=0x" << aux.right_id
                         << ", requires=0x" << aux.required_conn << std::dec << ")\n");
      }
    }
  }

  return matches;
}

std::vector<InflectionCandidate> Inflection::matchVerbStem(
    std::string_view remaining,
    const std::vector<std::string>& aux_chain,
    uint16_t required_conn) const {
  std::vector<InflectionCandidate> candidates;
  candidates.reserve(16);  // Typical max candidates
  const auto& endings = getVerbEndings();

  for (const auto& ending : endings) {
    // Check if connection is valid
    if (ending.provides_conn != required_conn) {
      continue;
    }

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

      // Stem should be at least 3 bytes (one Japanese character)
      // Exceptions for irregular verbs where the suffix IS the conjugated form:
      // - Suru verb with す/し→する (empty stem is allowed)
      // - Kuru verb with こ/き→くる (empty stem is allowed for mizenkei/renyokei)
      if (stem.size() < 3 &&
          !(ending.verb_type == VerbType::Suru &&
            (ending.suffix == "す" || ending.suffix == "し")) &&
          !(ending.verb_type == VerbType::Kuru &&
            (ending.suffix == "こ" || ending.suffix == "き"))) {
        continue;
      }

      // Skip Ichidan with empty suffix when no auxiliaries matched
      // (prevents "書いて" from being parsed as Ichidan "書いてる")
      if (ending.suffix.empty() && aux_chain.empty() &&
          ending.verb_type == VerbType::Ichidan) {
        continue;
      }

      // Reject stems starting with て (hiragana)
      // Japanese verb stems never start with て - it's always a te-form particle
      // This prevents てあげる from being parsed as a verb (should be て + あげる)
      // Real verbs with te-sound use kanji: 照る (teru), 出る (deru)
      if (stem.size() >= 3 && stem.substr(0, 3) == "て") {
        continue;
      }

      // Validate irregular いく pattern: GodanKa with っ-onbin only valid for い/行
      if (ending.verb_type == VerbType::GodanKa && ending.suffix == "っ" &&
          ending.is_onbin) {
        if (stem != "い" && stem != "行") {
          continue;  // Skip invalid irregular pattern
        }
      }

      // Validate Ichidan: reject stems that would create irregular verb base forms
      // くる (来る) is Kuru verb, not Ichidan. Stem く + る = くる is INVALID.
      // する is Suru verb, not Ichidan. Stem す + る = する is INVALID.
      // こる is not a valid verb - こ is Kuru mizenkei suffix, not Ichidan stem.
      // This prevents くなかった from being parsed as Ichidan く + なかった = くる
      // Note: 来 (kanji) is handled separately - 来なかった should become 来る (Kuru)
      if (ending.verb_type == VerbType::Ichidan && stem.size() == 3) {
        if (stem == "く" || stem == "す" || stem == "こ") {
          continue;  // Skip - these are irregular verbs (hiragana), not Ichidan
        }
      }

      // Special handling for kanji 来: should be Kuru, not Ichidan
      // Remap 来 + Ichidan patterns to Kuru verb type
      VerbType actual_verb_type = ending.verb_type;
      std::string actual_base_suffix = ending.base_suffix;
      if (ending.verb_type == VerbType::Ichidan && stem == "来") {
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
        if (stem.size() >= 3) {
          std::string_view last_char = std::string_view(stem).substr(stem.size() - 3);
          // These hiragana at the end of stem indicate a particle or non-suru pattern
          if (last_char == "で" || last_char == "に" || last_char == "を" ||
              last_char == "が" || last_char == "は" || last_char == "も" ||
              last_char == "と" || last_char == "へ" || last_char == "か" ||
              last_char == "や" || last_char == "の") {
            invalid_stem = true;
          }
          // For empty suffix suru patterns (e.g., 開催+された), the stem must
          // NOT end with hiragana that could be part of verb conjugations.
          // This prevents 奪われた → 奪わ+された → 奪わする (wrong)
          // Valid suru stems are all-kanji (開催) or katakana (ドライブ)
          if (ending.suffix.empty() && !aux_chain.empty()) {
            // Check if last character is hiragana (verb conjugation suffix)
            // A-row: あ か が さ た な は ば ま ら わ
            // These are common mizenkei endings for godan verbs
            if (last_char == "あ" || last_char == "か" || last_char == "が" ||
                last_char == "さ" || last_char == "た" || last_char == "な" ||
                last_char == "ば" || last_char == "ま" || last_char == "ら" ||
                last_char == "わ") {
              invalid_stem = true;
            }
            // E-row: け げ せ て ね べ め れ え
            // These are common potential stems or Ichidan stem endings
            // This prevents 話せ + なくなった → 話せする (wrong)
            if (last_char == "け" || last_char == "げ" || last_char == "せ" ||
                last_char == "て" || last_char == "ね" || last_char == "べ" ||
                last_char == "め" || last_char == "れ" || last_char == "え") {
              invalid_stem = true;
            }
            // Single-kanji stems are NOT valid for empty suffix suru patterns
            // Real suru verb stems have 2+ kanji (開催, 勉強, 検討)
            // This prevents 見+られた → 見する (wrong, should be 見る Ichidan)
            if (stem.size() <= 3) {
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

      InflectionCandidate candidate;
      candidate.base_form = base_form;
      candidate.stem = stem;
      candidate.suffix = suffix_str;
      candidate.verb_type = actual_verb_type;  // Use remapped type for 来→Kuru
      candidate.confidence = calculateConfidence(
          actual_verb_type, stem, aux_total_len, aux_chain.size(), required_conn);
      candidate.morphemes = aux_chain;

      SUZUME_DEBUG_INFL("  [STEM MATCH] \"" << remaining << "\" → base=\""
                        << base_form << "\" stem=\"" << stem
                        << "\" type=" << static_cast<int>(actual_verb_type)
                        << " suffix=\"" << suffix_str
                        << "\" conf=" << candidate.confidence << "\n");

      candidates.push_back(candidate);
    }
  }

  return candidates;
}

std::vector<InflectionCandidate> Inflection::analyzeWithAuxiliaries(
    std::string_view surface,
    const std::vector<std::string>& aux_chain,
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

    // Recursively analyze remaining
    std::string_view remaining = surface.substr(0, surface.size() - len);
    std::vector<std::string> new_chain = aux_chain;
    new_chain.push_back(aux->surface);

    auto sub_candidates = analyzeWithAuxiliaries(
        remaining, new_chain, aux->required_conn);
    for (auto& cand : sub_candidates) {
      candidates.push_back(std::move(cand));
    }
  }

  // Also try to match verb stem directly
  auto stem_candidates = matchVerbStem(surface, aux_chain, required_conn);
  for (auto& cand : stem_candidates) {
    candidates.push_back(std::move(cand));
  }

  return candidates;
}

std::vector<InflectionCandidate> Inflection::analyze(
    std::string_view surface) const {
  // Check cache first
  std::string key(surface);
  auto iter = cache_.find(key);
  if (iter != cache_.end()) {
    SUZUME_DEBUG_INFL("[INFLECTION] \"" << surface << "\" (cached, "
                      << iter->second.size() << " candidates)\n");
    return iter->second;
  }

  SUZUME_DEBUG_INFL("[INFLECTION] Analyzing \"" << surface << "\"\n");

  std::vector<InflectionCandidate> candidates;
  candidates.reserve(32);  // Typical max candidates

  // Early return for very short strings (less than 2 Japanese characters)
  // A conjugated verb needs at least stem + ending
  if (surface.size() < 6) {  // 2 Japanese chars = 6 bytes in UTF-8
    cache_[key] = candidates;
    return candidates;
  }

  // First, try to match auxiliaries from the end
  auto matches = matchAuxiliaries(surface);

  for (const auto& [aux, len] : matches) {
    std::string_view remaining = surface.substr(0, surface.size() - len);
    std::vector<std::string> aux_chain{aux->surface};

    auto sub_candidates = analyzeWithAuxiliaries(
        remaining, aux_chain, aux->required_conn);
    for (auto& cand : sub_candidates) {
      candidates.push_back(std::move(cand));
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

  // Sort by confidence (descending)
  // Use stable_sort to preserve the original order for candidates with equal
  // confidence. This ensures consistent behavior regardless of pattern count.
  std::stable_sort(candidates.begin(), candidates.end(),
                   [](const InflectionCandidate& lhs,
                      const InflectionCandidate& rhs) {
                     return lhs.confidence > rhs.confidence;
                   });

  // Remove duplicates (same base_form and verb_type)
  auto dup_end = std::unique(
      candidates.begin(), candidates.end(),
      [](const InflectionCandidate& lhs, const InflectionCandidate& rhs) {
        return lhs.base_form == rhs.base_form && lhs.verb_type == rhs.verb_type;
      });
  candidates.erase(dup_end, candidates.end());

  // Debug: print final candidates
  if (core::Debug::isInflectionEnabled() && !candidates.empty()) {
    core::Debug::log() << "[INFLECTION] Results for \"" << surface << "\":\n";
    for (size_t i = 0; i < candidates.size() && i < 5; ++i) {
      const auto& c = candidates[i];
      core::Debug::log() << "  " << (i + 1) << ". base=\"" << c.base_form
                         << "\" type=" << static_cast<int>(c.verb_type)
                         << " conf=" << c.confidence << "\n";
    }
    if (candidates.size() > 5) {
      core::Debug::log() << "  ... and " << (candidates.size() - 5) << " more\n";
    }
  }

  // Cache the result
  cache_[key] = candidates;
  return candidates;
}

bool Inflection::looksConjugated(std::string_view surface) const {
  // Check cache first to avoid copying
  std::string key(surface);
  auto iter = cache_.find(key);
  if (iter != cache_.end()) {
    return !iter->second.empty();
  }
  return !analyze(surface).empty();
}

InflectionCandidate Inflection::getBest(std::string_view surface) const {
  // Check cache first to avoid copying the entire vector
  std::string key(surface);
  auto iter = cache_.find(key);
  if (iter != cache_.end()) {
    if (iter->second.empty()) {
      return {};
    }
    return iter->second.front();  // Only copy the first element
  }

  // Not in cache, do full analysis
  auto candidates = analyze(surface);
  if (candidates.empty()) {
    return {};
  }
  return candidates.front();
}

}  // namespace suzume::grammar
