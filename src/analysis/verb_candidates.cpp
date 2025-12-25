/**
 * @file verb_candidates.cpp
 * @brief Verb-based unknown word candidate generation
 */

#include "verb_candidates.h"

#include <algorithm>

#include "grammar/conjugation.h"
#include "suffix_candidates.h"
#include "unknown.h"

namespace suzume::analysis {

std::vector<UnknownCandidate> generateCompoundVerbCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const grammar::Inflection& inflection,
    const dictionary::DictionaryManager* dict_manager) {
  std::vector<UnknownCandidate> candidates;

  // Requires dictionary to verify base forms
  if (dict_manager == nullptr) {
    return candidates;
  }

  // Pattern: Kanji+ Hiragana(1-3) Kanji+ Hiragana+
  // e.g., 恐(K)れ(H)入(K)ります(H), 差(K)し(H)上(K)げます(H)
  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Kanji) {
    return candidates;
  }

  // Find first kanji portion (1-2 chars)
  size_t kanji1_end = start_pos;
  while (kanji1_end < char_types.size() &&
         kanji1_end - start_pos < 3 &&
         char_types[kanji1_end] == normalize::CharType::Kanji) {
    ++kanji1_end;
  }

  if (kanji1_end == start_pos || kanji1_end >= char_types.size()) {
    return candidates;
  }

  // Find first hiragana portion (1-3 chars, typically verb renyoukei ending)
  if (char_types[kanji1_end] != normalize::CharType::Hiragana) {
    return candidates;
  }

  size_t hira1_end = kanji1_end;
  while (hira1_end < char_types.size() &&
         hira1_end - kanji1_end < 4 &&
         char_types[hira1_end] == normalize::CharType::Hiragana) {
    ++hira1_end;
  }

  // Find second kanji portion (must exist for compound verb)
  if (hira1_end >= char_types.size() ||
      char_types[hira1_end] != normalize::CharType::Kanji) {
    return candidates;
  }

  size_t kanji2_end = hira1_end;
  while (kanji2_end < char_types.size() &&
         kanji2_end - hira1_end < 3 &&
         char_types[kanji2_end] == normalize::CharType::Kanji) {
    ++kanji2_end;
  }

  // Find second hiragana portion (conjugation ending)
  if (kanji2_end >= char_types.size() ||
      char_types[kanji2_end] != normalize::CharType::Hiragana) {
    return candidates;
  }

  size_t hira2_end = kanji2_end;
  while (hira2_end < char_types.size() &&
         hira2_end - kanji2_end < 10 &&
         char_types[hira2_end] == normalize::CharType::Hiragana) {
    ++hira2_end;
  }

  // Try different ending lengths
  for (size_t end_pos = hira2_end; end_pos > kanji2_end; --end_pos) {
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);
    if (surface.empty()) {
      continue;
    }

    // Use inflection analyzer to get potential base forms
    auto inflection_candidates = inflection.analyze(surface);

    for (const auto& infl_cand : inflection_candidates) {
      if (infl_cand.confidence < 0.4F) {
        continue;
      }

      // Check if base form exists in dictionary as a verb
      auto results = dict_manager->lookup(infl_cand.base_form, 0);
      for (const auto& result : results) {
        if (result.entry == nullptr) {
          continue;
        }
        if (result.entry->surface != infl_cand.base_form) {
          continue;
        }
        if (result.entry->pos != core::PartOfSpeech::Verb) {
          continue;
        }

        // Verify conjugation type matches
        auto dict_verb_type =
            grammar::conjTypeToVerbType(result.entry->conj_type);
        if (dict_verb_type == infl_cand.verb_type) {
          // Found a match! Generate candidate
          UnknownCandidate cand;
          cand.surface = surface;
          cand.start = start_pos;
          cand.end = end_pos;
          cand.pos = core::PartOfSpeech::Verb;
          // Low cost to prefer dictionary-verified compound verbs
          cand.cost = 0.3F;
          cand.has_suffix = false;
          candidates.push_back(cand);
          return candidates;  // Return first valid match
        }
      }
    }
  }

  return candidates;
}

std::vector<UnknownCandidate> generateVerbCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const grammar::Inflection& inflection,
    const dictionary::DictionaryManager* dict_manager) {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Kanji) {
    return candidates;
  }

  // Find kanji portion (typically 1-2 characters for verbs)
  size_t kanji_end = start_pos;
  while (kanji_end < char_types.size() &&
         kanji_end - start_pos < 3 &&  // Max 3 kanji for verb stem
         char_types[kanji_end] == normalize::CharType::Kanji) {
    ++kanji_end;
  }

  if (kanji_end == start_pos) {
    return candidates;
  }

  // Look for hiragana after kanji
  if (kanji_end >= char_types.size() ||
      char_types[kanji_end] != normalize::CharType::Hiragana) {
    return candidates;
  }

  // Check if first hiragana is a particle that can NEVER be part of a verb
  // E.g., "領収書を" - を is a particle, not part of a verb
  // Note about が and に:
  // - が can be part of verbs: 上がる, 下がる, 受かる, etc.
  // - に can be part of verbs: 煮る (niru), etc.
  // However, these verbs should be in the dictionary, not generated as unknown.
  // Including が/に here prevents false positives like 金がない→金ぐ, 家にいます→家にう
  // Note about か:
  // - か can be part of verbs: 書かない (negative), 動かす, etc.
  // - These are valid conjugation patterns that should be generated
  char32_t first_hiragana = codepoints[kanji_end];
  if (first_hiragana == U'を' || first_hiragana == U'が' ||
      first_hiragana == U'は' || first_hiragana == U'も' ||
      first_hiragana == U'へ' || first_hiragana == U'の' ||
      first_hiragana == U'に' || first_hiragana == U'や') {
    return candidates;  // Not a verb - these particles follow nouns
  }

  size_t hiragana_end = kanji_end;
  while (hiragana_end < char_types.size() &&
         hiragana_end - kanji_end < 12 &&  // Max 12 hiragana for conjugation + aux
         char_types[hiragana_end] == normalize::CharType::Hiragana) {
    // Note: We no longer break at particle-like characters here.
    // The inflection module will determine if the full string is a valid
    // conjugated verb. This allows patterns like "飲みながら" (nomi-nagara)
    // where "が" is part of the auxiliary "ながら", not a standalone particle.
    ++hiragana_end;
  }

  // Need at least some hiragana for a conjugated verb
  if (hiragana_end <= kanji_end) {
    return candidates;
  }

  // Try different stem lengths (kanji only, or kanji + 1 hiragana for ichidan)
  // This handles both godan (kanji stem) and ichidan (kanji + hiragana stem)
  for (size_t stem_end = kanji_end; stem_end <= kanji_end + 1 && stem_end < hiragana_end; ++stem_end) {
    // Try different ending lengths, starting from longest
    for (size_t end_pos = hiragana_end; end_pos > stem_end; --end_pos) {
      std::string surface = extractSubstring(codepoints, start_pos, end_pos);

      if (surface.empty()) {
        continue;
      }

      // Check for particle/copula patterns that should NOT be treated as verbs
      // Kanji + particle or copula (で, に, を, が, は, も, へ, と, や, か, の, etc.)
      std::string hiragana_part = extractSubstring(codepoints, kanji_end, end_pos);
      if (hiragana_part == "で" || hiragana_part == "に" || hiragana_part == "を" ||
          hiragana_part == "が" || hiragana_part == "は" || hiragana_part == "も" ||
          hiragana_part == "へ" || hiragana_part == "と" || hiragana_part == "や" ||
          hiragana_part == "か" || hiragana_part == "の" || hiragana_part == "から" ||
          hiragana_part == "まで" || hiragana_part == "より" || hiragana_part == "ほど" ||
          hiragana_part == "です" || hiragana_part == "だ" || hiragana_part == "だった" ||
          hiragana_part == "でした" || hiragana_part == "でし" || hiragana_part == "である") {
        continue;  // Skip particle/copula patterns
      }

      // Skip patterns where hiragana part is a known suffix in dictionary
      // (e.g., たち, さん, ら, etc.) - let NOUN+suffix split win instead
      // Only skip if kanji is 2+ characters, as single kanji + suffix
      // could be a valid verb stem (立ち → 立つ)
      // Note: Only skip for OTHER (suffixes), not VERB (する is a verb, not suffix)
      bool is_suffix_pattern = false;
      if (kanji_end - start_pos >= 2 && dict_manager != nullptr) {
        auto suffix_results = dict_manager->lookup(hiragana_part, 0);
        for (const auto& result : suffix_results) {
          if (result.entry != nullptr &&
              result.entry->surface == hiragana_part &&
              result.entry->is_low_info &&
              result.entry->pos == core::PartOfSpeech::Other) {
            // This hiragana part is a registered suffix - skip verb candidate
            is_suffix_pattern = true;
            break;
          }
        }
      }
      if (is_suffix_pattern) {
        continue;
      }

      // Skip patterns that end with particles (noun renyokei + particle)
      // e.g., 切りに (切り + に), 飲みに (飲み + に), 行きに (行き + に)
      // These are nominalized verb stems followed by particles, not verb forms
      size_t hp_size = hiragana_part.size();
      if (hp_size >= 6) {  // At least 2 hiragana (6 bytes)
        // Get last hiragana character (particle candidate)
        char32_t last_char = codepoints[end_pos - 1];
        if (last_char == U'に' || last_char == U'で' || last_char == U'を' ||
            last_char == U'が' || last_char == U'は' || last_char == U'も' ||
            last_char == U'へ' || last_char == U'の' || last_char == U'と') {
          // Check if the preceding part could be a valid verb renyokei
          // Renyokei typically ends in い/り/き/ぎ/し/み/び/ち/に
          char32_t second_last_char = codepoints[end_pos - 2];
          if (second_last_char == U'い' || second_last_char == U'り' ||
              second_last_char == U'き' || second_last_char == U'ぎ' ||
              second_last_char == U'し' || second_last_char == U'み' ||
              second_last_char == U'び' || second_last_char == U'ち') {
            continue;  // Skip - likely nominalized noun + particle
          }
        }
      }

      // Check if this looks like a conjugated verb
      auto best = inflection.getBest(surface);
      // Only accept verb types (not IAdjective) and require confidence > 0.48
      if (best.confidence > 0.48F &&
          best.verb_type != grammar::VerbType::IAdjective) {
        // Verify the stem matches
        std::string expected_stem = extractSubstring(codepoints, start_pos, stem_end);
        if (best.stem == expected_stem) {
          // Reject Godan verbs with stems ending in e-row hiragana
          // E-row endings (え,け,せ,て,ね,へ,め,れ) are typically ichidan stems
          // E.g., "伝えいた" falsely matches as GodanKa "伝えく" but 伝える is ichidan
          bool is_godan = (best.verb_type == grammar::VerbType::GodanKa ||
                           best.verb_type == grammar::VerbType::GodanGa ||
                           best.verb_type == grammar::VerbType::GodanSa ||
                           best.verb_type == grammar::VerbType::GodanTa ||
                           best.verb_type == grammar::VerbType::GodanNa ||
                           best.verb_type == grammar::VerbType::GodanBa ||
                           best.verb_type == grammar::VerbType::GodanMa ||
                           best.verb_type == grammar::VerbType::GodanRa ||
                           best.verb_type == grammar::VerbType::GodanWa);
          if (is_godan && stem_end > kanji_end && stem_end <= codepoints.size()) {
            // Check if the last character of the stem is e-row hiragana
            char32_t last_char = codepoints[stem_end - 1];
            if (last_char == U'え' || last_char == U'け' || last_char == U'せ' ||
                last_char == U'て' || last_char == U'ね' || last_char == U'へ' ||
                last_char == U'め' || last_char == U'れ') {
              continue;  // Skip - e-row stem is typically ichidan, not godan
            }
          }

          // Skip Suru verb renyokei (し) if followed by te/ta form particles
          // e.g., "勉強して" should be parsed as single token, not "勉強し" + "て"
          if (best.verb_type == grammar::VerbType::Suru &&
              hiragana_part == "し" && end_pos < codepoints.size()) {
            char32_t next_char = codepoints[end_pos];
            if (next_char == U'て' || next_char == U'た' ||
                next_char == U'で' || next_char == U'だ') {
              continue;  // Skip - let the longer te-form candidate win
            }
          }

          UnknownCandidate candidate;
          candidate.surface = surface;
          candidate.start = start_pos;
          candidate.end = end_pos;
          candidate.pos = core::PartOfSpeech::Verb;
          // Lower cost for higher confidence matches
          candidate.cost = 0.4F + (1.0F - best.confidence) * 0.3F;
          candidate.has_suffix = false;
          candidates.push_back(candidate);
          // Don't break - try other stem lengths too
        }
      }
    }
  }

  // Sort by cost and return best candidates
  std::sort(candidates.begin(), candidates.end(),
            [](const UnknownCandidate& lhs, const UnknownCandidate& rhs) {
              return lhs.cost < rhs.cost;
            });

  return candidates;
}

std::vector<UnknownCandidate> generateHiraganaVerbCandidates(
    const std::vector<char32_t>& codepoints,
    size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const grammar::Inflection& inflection,
    const dictionary::DictionaryManager* dict_manager) {
  std::vector<UnknownCandidate> candidates;

  if (start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Hiragana) {
    return candidates;
  }

  // Skip if starting character is a particle that is NEVER a verb stem
  // Note: Exclude characters that CAN be verb stems:
  //   - な→なる/なくす
  //   - て→できる
  //   - や→やる (important: must NOT skip や)
  char32_t first_char = codepoints[start_pos];
  if (first_char == U'を' || first_char == U'が' || first_char == U'は' ||
      first_char == U'に' || first_char == U'へ' || first_char == U'の' ||
      first_char == U'か' || first_char == U'ね' ||
      first_char == U'よ' || first_char == U'わ') {
    return candidates;
  }

  // Skip if starting with demonstrative pronouns (これ, それ, あれ, どれ, etc.)
  // These are commonly mistaken for verbs (これる, それる, etc.)
  if (start_pos + 1 < codepoints.size()) {
    char32_t second_char = codepoints[start_pos + 1];
    // Check for こ/そ/あ/ど + れ/こ/ち patterns (demonstrative pronouns)
    if ((first_char == U'こ' || first_char == U'そ' ||
         first_char == U'あ' || first_char == U'ど') &&
        (second_char == U'れ' || second_char == U'こ' || second_char == U'ち')) {
      return candidates;
    }

    // Skip if starting with 「ない」(auxiliary verb/i-adjective for negation)
    // These should be recognized as AUX by dictionary, not as hiragana verbs.
    // E.g., 「ないんだ」→「ない」+「んだ」, not a single verb「ないむ」
    if (first_char == U'な' && second_char == U'い') {
      return candidates;
    }
  }

  // Find hiragana sequence, breaking at particle boundaries
  // Note: Be careful not to break at characters that are part of verb conjugations:
  //   - か can be part of なかった (negative past) or かった (i-adj past)
  //   - で can be part of んで (te-form for godan) or できる (potential verb)
  //   - も can be part of ても (even if) or もらう (receiving verb)
  size_t hiragana_end = start_pos;
  while (hiragana_end < char_types.size() &&
         hiragana_end - start_pos < 12 &&  // Max 12 hiragana for verb + endings
         char_types[hiragana_end] == normalize::CharType::Hiragana) {
    // Don't include particles that appear after the first hiragana character.
    // E.g., for "りにする", stop at "り" to not include "にする".
    if (hiragana_end > start_pos) {
      char32_t curr = codepoints[hiragana_end];

      // Check for particle-like characters
      if (curr == U'を' || curr == U'が' || curr == U'は' ||
          curr == U'に' || curr == U'へ' || curr == U'の' ||
          curr == U'と' || curr == U'や') {
        break;  // These are always particles in this context
      }

      // For か, で, も: check if they're part of verb conjugation patterns
      // Don't break if they appear in known conjugation contexts
      if (curr == U'か' || curr == U'で' || curr == U'も') {
        // Check the preceding character for conjugation patterns
        char32_t prev = codepoints[hiragana_end - 1];

        // か: OK if preceded by な (なかった = negative past)
        if (curr == U'か' && prev == U'な') {
          ++hiragana_end;
          continue;
        }

        // で: OK if preceded by ん (んで = te-form) or き (できる)
        if (curr == U'で' && (prev == U'ん' || prev == U'き')) {
          ++hiragana_end;
          continue;
        }

        // も: OK if preceded by て (ても = even if)
        if (curr == U'も' && prev == U'て') {
          ++hiragana_end;
          continue;
        }

        // Otherwise, treat as particle
        break;
      }
    }
    ++hiragana_end;
  }

  // Need at least 2 hiragana for a verb
  if (hiragana_end <= start_pos + 1) {
    return candidates;
  }

  // Try different lengths, starting from longest
  for (size_t end_pos = hiragana_end; end_pos > start_pos + 1; --end_pos) {
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);

    if (surface.empty()) {
      continue;
    }

    // Check if this looks like a conjugated verb
    auto best = inflection.getBest(surface);
    // Only accept verb types (not IAdjective)
    if (best.confidence > 0.48F &&
        best.verb_type != grammar::VerbType::IAdjective) {
      // Lower cost for higher confidence matches
      float base_cost = 0.5F + (1.0F - best.confidence) * 0.3F;

      // Check if the base form exists in the dictionary
      // This gives a significant bonus for conjugations of known verbs
      // (e.g., できなくて → できる which is in the dictionary)
      bool is_dictionary_verb = false;
      if (dict_manager != nullptr && !best.base_form.empty()) {
        auto results = dict_manager->lookup(best.base_form, 0);
        for (const auto& result : results) {
          if (result.entry != nullptr &&
              result.entry->surface == best.base_form &&
              result.entry->pos == core::PartOfSpeech::Verb) {
            is_dictionary_verb = true;
            break;
          }
        }
      }

      // Give significant bonus for dictionary-verified hiragana verbs
      // This helps them beat the particle+adj+particle split path
      // Only apply to longer forms (5+ chars) to avoid boosting short forms like
      // "あった" (ある) which can interfere with copula recognition (であった)
      size_t candidate_len = end_pos - start_pos;
      if (is_dictionary_verb && candidate_len >= 5) {
        base_cost = 0.1F + (1.0F - best.confidence) * 0.2F;
      }

      UnknownCandidate candidate;
      candidate.surface = surface;
      candidate.start = start_pos;
      candidate.end = end_pos;
      candidate.pos = core::PartOfSpeech::Verb;
      candidate.cost = base_cost;
      candidate.has_suffix = false;
      candidates.push_back(candidate);
    }
  }

  // Sort by cost
  std::sort(candidates.begin(), candidates.end(),
            [](const UnknownCandidate& lhs, const UnknownCandidate& rhs) {
              return lhs.cost < rhs.cost;
            });

  return candidates;
}

}  // namespace suzume::analysis
