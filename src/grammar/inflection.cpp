/**
 * @file inflection.cpp
 * @brief Connection-based reverse inflection analysis implementation
 */

#include "inflection.h"

#include <algorithm>

#include "inflection_scorer.h"
#include "verb_endings.h"

namespace suzume::grammar {

std::vector<std::pair<const AuxiliaryEntry*, size_t>>
Inflection::matchAuxiliaries(std::string_view surface) const {
  std::vector<std::pair<const AuxiliaryEntry*, size_t>> matches;
  const auto& auxiliaries = getAuxiliaries();

  for (const auto& aux : auxiliaries) {
    if (surface.size() >= aux.surface.size()) {
      size_t start = surface.size() - aux.surface.size();
      if (surface.substr(start) == aux.surface) {
        matches.emplace_back(&aux, aux.surface.size());
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
      // Exception: Suru verb with す/し→する (empty stem is allowed for する)
      if (stem.size() < 3 &&
          !(ending.verb_type == VerbType::Suru &&
            (ending.suffix == "す" || ending.suffix == "し"))) {
        continue;
      }

      // Skip Ichidan with empty suffix when no auxiliaries matched
      // (prevents "書いて" from being parsed as Ichidan "書いてる")
      if (ending.suffix.empty() && aux_chain.empty() &&
          ending.verb_type == VerbType::Ichidan) {
        continue;
      }

      // Validate irregular いく pattern: GodanKa with っ-onbin only valid for い/行
      if (ending.verb_type == VerbType::GodanKa && ending.suffix == "っ" &&
          ending.is_onbin) {
        if (stem != "い" && stem != "行") {
          continue;  // Skip invalid irregular pattern
        }
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

      // Build base form
      std::string base_form = stem + ending.base_suffix;

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
      candidate.verb_type = ending.verb_type;
      candidate.confidence = calculateConfidence(
          ending.verb_type, stem, aux_total_len, aux_chain.size(), required_conn);
      candidate.morphemes = aux_chain;

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
  std::vector<InflectionCandidate> candidates;

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

  return candidates;
}

bool Inflection::looksConjugated(std::string_view surface) const {
  return !analyze(surface).empty();
}

InflectionCandidate Inflection::getBest(std::string_view surface) const {
  auto candidates = analyze(surface);
  if (candidates.empty()) {
    return {};
  }
  return candidates.front();
}

}  // namespace suzume::grammar
