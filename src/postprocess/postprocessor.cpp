#include "postprocess/postprocessor.h"

#include "core/debug.h"
#include "core/utf8_constants.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"

namespace suzume::postprocess {

Postprocessor::Postprocessor(const PostprocessOptions& options)
    : options_(options), lemmatizer_() {}

Postprocessor::Postprocessor(const dictionary::DictionaryManager* dict_manager,
                             const PostprocessOptions& options)
    : options_(options), lemmatizer_(dict_manager) {}

std::vector<core::Morpheme> Postprocessor::process(
    const std::vector<core::Morpheme>& morphemes) const {
  std::vector<core::Morpheme> result = morphemes;
  [[maybe_unused]] size_t before_count = 0;

  // Note: NOUN + SUFFIX merging is intentionally disabled.
  // We keep tokens separate: PREFIX + NOUN + SUFFIX
  // e.g., お姉さん → お(PREFIX) + 姉(NOUN) + さん(SUFFIX)
  // result = mergeNounSuffix(result);
  SUZUME_DEBUG_LOG_VERBOSE("[POSTPROC] mergeNounSuffix: disabled\n");

  // Convert PREFIX + VERB to PREFIX + NOUN (renyoukei nominalization)
  // e.g., お願い → お(PREFIX) + 願い(NOUN), not 願い(VERB)
  before_count = result.size();
  result = convertPrefixVerbToNoun(result);
  // Note: this function logs individual changes, so no summary needed

  // Merge consecutive numeric expressions (always applied)
  before_count = result.size();
  result = mergeNumericExpressions(result);
  if (result.size() != before_count) {
    SUZUME_DEBUG_LOG("[POSTPROC] mergeNumericExpressions: " << before_count << " → " << result.size() << "\n");
  }

  // Disabled for MeCab compatibility (MeCab keeps na-adjective + な separate)
  // result = mergeNaAdjectiveNa(result);
  SUZUME_DEBUG_LOG_VERBOSE("[POSTPROC] mergeNaAdjectiveNa: disabled (MeCab compat)\n");

  // Apply lemmatization
  if (options_.lemmatize) {
    lemmatizer_.lemmatizeAll(result);
    SUZUME_DEBUG_LOG_VERBOSE("[POSTPROC] lemmatize: applied\n");
  }

  // Merge verb renyokei + もの → compound noun (食べもの, 飲みもの, etc.)
  // Must run after lemmatize so conj_form is set
  before_count = result.size();
  result = mergeVerbRenyokeiMono(result);
  if (result.size() != before_count) {
    SUZUME_DEBUG_LOG("[POSTPROC] mergeVerbRenyokeiMono: " << before_count << " → " << result.size() << "\n");
  }

  // Merge noun compounds
  if (options_.merge_noun_compounds) {
    before_count = result.size();
    result = mergeNounCompounds(result);
    if (result.size() != before_count) {
      SUZUME_DEBUG_LOG("[POSTPROC] mergeNounCompounds: " << before_count << " → " << result.size() << "\n");
    }
  }

  // Merge prolonged sound mark (ー) with preceding token
  before_count = result.size();
  result = mergeProlongedSoundMark(result);
  if (result.size() != before_count) {
    SUZUME_DEBUG_LOG("[POSTPROC] mergeProlongedSoundMark: " << before_count << " → " << result.size() << "\n");
  }

  // Filter unwanted morphemes
  result = filterMorphemes(result);

  return result;
}

std::vector<core::Morpheme> Postprocessor::mergeNounCompounds(const std::vector<core::Morpheme>& morphemes) {
  if (morphemes.empty()) {
    return morphemes;
  }

  std::vector<core::Morpheme> result;
  result.reserve(morphemes.size());

  size_t idx = 0;
  while (idx < morphemes.size()) {
    const auto& current = morphemes[idx];

    // Check if this is a noun that can be merged
    if (current.pos == core::PartOfSpeech::Noun && !current.features.is_formal_noun) {
      // Collect consecutive nouns
      core::Morpheme merged = current;
      size_t merge_end = idx + 1;
      size_t merge_count = 1;

      while (merge_end < morphemes.size()) {
        const auto& next = morphemes[merge_end];
        if (next.pos == core::PartOfSpeech::Noun && !next.features.is_formal_noun) {
          // Merge surface and lemma
          merged.surface += next.surface;
          if (!next.lemma.empty()) {
            merged.lemma += next.lemma;
          } else {
            merged.lemma += next.surface;
          }
          merged.end = next.end;
          merged.end_pos = next.end_pos;
          ++merge_end;
          ++merge_count;
        } else {
          break;
        }
      }

      SUZUME_DEBUG_IF(merge_count > 1) {
        SUZUME_DEBUG_STREAM << "[POSTPROC] Merged " << merge_count << " nouns: ";
        for (size_t i = idx; i < merge_end; ++i) {
          if (i > idx) SUZUME_DEBUG_STREAM << " + ";
          SUZUME_DEBUG_STREAM << "\"" << morphemes[i].surface << "\"";
        }
        SUZUME_DEBUG_STREAM << " → \"" << merged.surface << "\"\n";
      }

      result.push_back(merged);
      idx = merge_end;
    } else {
      result.push_back(current);
      ++idx;
    }
  }

  return result;
}

std::vector<core::Morpheme> Postprocessor::filterMorphemes(
    const std::vector<core::Morpheme>& morphemes) const {
  std::vector<core::Morpheme> result;
  result.reserve(morphemes.size());

  for (const auto& morpheme : morphemes) {
    // Skip symbols if option is set
    if (options_.remove_symbols && morpheme.pos == core::PartOfSpeech::Symbol) {
      continue;
    }

    // Skip short morphemes
    if (morpheme.surface.size() < options_.min_surface_length) {
      continue;
    }

    result.push_back(morpheme);
  }

  return result;
}

std::vector<core::Morpheme> Postprocessor::convertPrefixVerbToNoun(
    const std::vector<core::Morpheme>& morphemes) {
  if (morphemes.size() < 2) {
    return morphemes;
  }

  std::vector<core::Morpheme> result;
  result.reserve(morphemes.size());

  for (size_t i = 0; i < morphemes.size(); ++i) {
    core::Morpheme m = morphemes[i];

    // Check if previous morpheme was PREFIX (お or ご)
    if (i > 0 && morphemes[i - 1].pos == core::PartOfSpeech::Prefix) {
      const std::string& prefix_surface = morphemes[i - 1].surface;
      // Only for honorific prefixes お and ご
      if (utf8::equalsAny(prefix_surface, {"お", "ご", "御"})) {
        // Convert VERB to NOUN (renyoukei nominalization)
        // e.g., 願い(VERB) → 願い(NOUN) after お
        // Exception: when followed by causative auxiliary (せ/させ),
        // the verb is part of a causative construction (お聞かせ, お知らせ)
        // and should remain as VERB
        if (m.pos == core::PartOfSpeech::Verb) {
          bool followed_by_causative = false;
          if (i + 1 < morphemes.size()) {
            const auto& next = morphemes[i + 1];
            if (next.extended_pos == core::ExtendedPOS::AuxCausative) {
              followed_by_causative = true;
            }
          }
          if (!followed_by_causative) {
            m.pos = core::PartOfSpeech::Noun;
            m.extended_pos = core::ExtendedPOS::Noun;
            // Keep surface as lemma for nominalized form
            m.lemma = m.surface;
            SUZUME_DEBUG_LOG_VERBOSE("[POSTPROC] Nominalized: " << m.surface
                             << " (VERB → NOUN after " << prefix_surface << ")\n");
          }
        }
      }
    }

    result.push_back(m);
  }

  return result;
}

std::vector<core::Morpheme> Postprocessor::mergeVerbRenyokeiMono(
    const std::vector<core::Morpheme>& morphemes) {
  if (morphemes.size() < 2) {
    return morphemes;
  }

  std::vector<core::Morpheme> result;
  result.reserve(morphemes.size());

  for (size_t i = 0; i < morphemes.size(); ++i) {
    // Check: VERB + もの(formal noun) → compound NOUN
    // e.g., 食べ+もの → 食べもの, 飲み+もの → 飲みもの, 乗り+もの → 乗りもの
    if (i + 1 < morphemes.size() &&
        morphemes[i].pos == core::PartOfSpeech::Verb &&
        morphemes[i].conj_form == grammar::ConjForm::Renyokei &&
        morphemes[i + 1].surface == "もの" &&
        morphemes[i + 1].features.is_formal_noun) {
      core::Morpheme merged = morphemes[i];
      merged.surface += morphemes[i + 1].surface;
      merged.pos = core::PartOfSpeech::Noun;
      merged.extended_pos = core::ExtendedPOS::Noun;
      merged.lemma = merged.surface;
      merged.end = morphemes[i + 1].end;
      merged.end_pos = morphemes[i + 1].end_pos;
      SUZUME_DEBUG_LOG("[POSTPROC] Merged verb+もの: \"" << morphemes[i].surface
                       << "\" + \"もの\" → \"" << merged.surface << "\"\n");
      result.push_back(merged);
      ++i;  // skip もの
      continue;
    }
    result.push_back(morphemes[i]);
  }

  return result;
}

std::vector<core::Morpheme> Postprocessor::mergeNounSuffix(
    const std::vector<core::Morpheme>& morphemes) {
  if (morphemes.empty()) {
    return morphemes;
  }

  std::vector<core::Morpheme> result;
  result.reserve(morphemes.size());

  size_t idx = 0;
  while (idx < morphemes.size()) {
    const auto& current = morphemes[idx];

    // Check if this is a noun followed by suffix(es)
    if (current.pos == core::PartOfSpeech::Noun ||
        current.pos == core::PartOfSpeech::Pronoun) {
      core::Morpheme merged = current;
      size_t merge_end = idx + 1;

      // Collect consecutive suffixes
      while (merge_end < morphemes.size() &&
             morphemes[merge_end].pos == core::PartOfSpeech::Suffix) {
        const auto& suffix = morphemes[merge_end];
        merged.surface += suffix.surface;
        merged.end = suffix.end;
        merged.end_pos = suffix.end_pos;
        ++merge_end;
      }

      if (merge_end > idx + 1) {
        // Merged at least one suffix - result is always NOUN
        merged.pos = core::PartOfSpeech::Noun;
        merged.lemma = merged.surface;  // Compound noun lemma is itself
        SUZUME_DEBUG_BLOCK {
          SUZUME_DEBUG_STREAM << "[POSTPROC] Merged noun+suffix: ";
          for (size_t i = idx; i < merge_end; ++i) {
            if (i > idx) SUZUME_DEBUG_STREAM << " + ";
            SUZUME_DEBUG_STREAM << "\"" << morphemes[i].surface << "\"";
          }
          SUZUME_DEBUG_STREAM << " → \"" << merged.surface << "\"\n";
        }
      }

      result.push_back(merged);
      idx = merge_end;
    } else {
      result.push_back(current);
      ++idx;
    }
  }

  return result;
}

namespace {

// Check if a character is a digit (ASCII or fullwidth)
bool isDigitChar(char32_t ch) {
  return (ch >= U'0' && ch <= U'9') || (ch >= U'０' && ch <= U'９');
}

// Check if surface is a numeric expression (starts with digit or contains units)
bool isNumericExpression(const std::string& surface) {
  if (surface.empty()) return false;

  size_t pos = 0;
  char32_t first_ch = suzume::normalize::decodeUtf8(surface, pos);
  return isDigitChar(first_ch);
}

// Check if surface ends with a digit
bool endsWithDigit(const std::string& surface) {
  if (surface.empty()) return false;

  auto codepoints = suzume::normalize::toCodepoints(surface);
  if (codepoints.empty()) return false;

  return isDigitChar(codepoints.back());
}

using normalize::isCounterKanji;

// Check if surface looks like a unit (noun that can follow numbers)
// For kanji: must start with a counter kanji (円, 分, 時間, etc.)
// For katakana: length-based heuristic (キロ, メートル, etc.)
bool looksLikeUnit(const std::string& surface) {
  if (surface.empty()) return false;

  auto codepoints = suzume::normalize::toCodepoints(surface);
  if (codepoints.empty()) return false;

  char32_t first = codepoints[0];

  // Kanji units: first char must be a counter kanji
  // CJK Unified Ideographs: U+4E00-U+9FFF
  if (first >= 0x4E00 && first <= 0x9FFF) {
    return isCounterKanji(first);
  }

  // Katakana units (キロ, メートル, パーセント, etc.): length heuristic
  // Katakana: U+30A0-U+30FF
  if (first >= 0x30A0 && first <= 0x30FF) {
    size_t len = codepoints.size();
    return len >= 1 && len <= 5;
  }

  return false;
}

// Check if surface ends with a numeric unit that can be followed by more numbers
bool endsWithContinuableUnit(const std::string& surface) {
  if (surface.empty()) return false;

  auto codepoints = suzume::normalize::toCodepoints(surface);
  if (codepoints.empty()) return false;

  char32_t last_ch = codepoints.back();
  // Units that can be followed by more numbers (兆, 億, 万, 千, 百)
  return last_ch == U'兆' || last_ch == U'億' || last_ch == U'万' ||
         last_ch == U'千' || last_ch == U'百';
}

}  // namespace

std::vector<core::Morpheme> Postprocessor::mergeNumericExpressions(
    const std::vector<core::Morpheme>& morphemes) {
  if (morphemes.empty()) {
    return morphemes;
  }

  std::vector<core::Morpheme> result;
  result.reserve(morphemes.size());

  size_t idx = 0;
  while (idx < morphemes.size()) {
    const auto& current = morphemes[idx];

    // Pattern 1: Merge large numbers (3億 + 5000万円)
    if (current.pos == core::PartOfSpeech::Noun &&
        isNumericExpression(current.surface) &&
        endsWithContinuableUnit(current.surface)) {
      core::Morpheme merged = current;
      size_t merge_end = idx + 1;

      // Collect consecutive numeric expressions
      while (merge_end < morphemes.size()) {
        const auto& next = morphemes[merge_end];
        if (next.pos == core::PartOfSpeech::Noun &&
            isNumericExpression(next.surface)) {
          merged.surface += next.surface;
          merged.lemma = merged.surface;
          merged.end = next.end;
          merged.end_pos = next.end_pos;
          ++merge_end;

          // Continue if this also ends with a continuable unit
          if (!endsWithContinuableUnit(next.surface)) {
            break;
          }
        } else {
          break;
        }
      }

      SUZUME_DEBUG_IF(merge_end > idx + 1) {
        SUZUME_DEBUG_STREAM << "[POSTPROC] Merged numeric: ";
        for (size_t i = idx; i < merge_end; ++i) {
          if (i > idx) SUZUME_DEBUG_STREAM << " + ";
          SUZUME_DEBUG_STREAM << "\"" << morphemes[i].surface << "\"";
        }
        SUZUME_DEBUG_STREAM << " → \"" << merged.surface << "\"\n";
      }

      result.push_back(merged);
      idx = merge_end;
      continue;
    }

    // Pattern 2: Merge number + unit (3 + 時間, 100 + ゴールド, 3時 + 間)
    // Exception: 対 (versus) should not merge - 2対1 should be 2|対|1
    if (current.pos == core::PartOfSpeech::Noun &&
        isNumericExpression(current.surface) &&
        endsWithDigit(current.surface) &&
        idx + 1 < morphemes.size()) {
      const auto& next = morphemes[idx + 1];
      bool is_versus = (next.surface == "対");
      if (next.pos == core::PartOfSpeech::Noun && looksLikeUnit(next.surface) && !is_versus) {
        core::Morpheme merged = current;
        merged.surface += next.surface;
        merged.lemma = merged.surface;
        merged.end = next.end;
        merged.end_pos = next.end_pos;

        SUZUME_DEBUG_LOG_VERBOSE("[POSTPROC] Merged number+unit: \""
                         << current.surface << "\" + \"" << next.surface
                         << "\" → \"" << merged.surface << "\"\n");

        result.push_back(merged);
        idx += 2;
        continue;
      }
    }

    // Pattern 3: Merge numeric with unit suffix (3時 + 間 → 3時間)
    if (current.pos == core::PartOfSpeech::Noun &&
        isNumericExpression(current.surface) &&
        idx + 1 < morphemes.size()) {
      const auto& next = morphemes[idx + 1];
      // Check for common time/counter suffixes that get split
      if (next.pos == core::PartOfSpeech::Noun &&
          utf8::equalsAny(next.surface, {"間", "半", "目"})) {
        core::Morpheme merged = current;
        merged.surface += next.surface;
        merged.lemma = merged.surface;
        merged.end = next.end;
        merged.end_pos = next.end_pos;

        SUZUME_DEBUG_LOG_VERBOSE("[POSTPROC] Merged numeric+suffix: \""
                         << current.surface << "\" + \"" << next.surface
                         << "\" → \"" << merged.surface << "\"\n");

        result.push_back(merged);
        idx += 2;
        continue;
      }
    }

    // Pattern 4: Merge indefinite numeral + counter suffix (数 + ヶ月 → 数ヶ月)
    // Indefinite numerals: 数 (suu = some/several), 幾 (iku = how many)
    if ((current.pos == core::PartOfSpeech::Noun ||
         current.pos == core::PartOfSpeech::Pronoun) &&
        utf8::equalsAny(current.surface, {"数", "幾", "何"}) &&
        idx + 1 < morphemes.size()) {
      const auto& next = morphemes[idx + 1];
      if (next.pos == core::PartOfSpeech::Suffix) {
        core::Morpheme merged = current;
        merged.pos = core::PartOfSpeech::Noun;  // Merged result is always NOUN
        merged.surface += next.surface;
        merged.lemma = merged.surface;
        merged.end = next.end;
        merged.end_pos = next.end_pos;

        SUZUME_DEBUG_LOG_VERBOSE("[POSTPROC] Merged indefinite+suffix: \""
                         << current.surface << "\" + \"" << next.surface
                         << "\" → \"" << merged.surface << "\"\n");

        result.push_back(merged);
        idx += 2;
        continue;
      }
    }

    result.push_back(current);
    ++idx;
  }

  return result;
}

std::vector<core::Morpheme> Postprocessor::mergeNaAdjectiveNa(
    const std::vector<core::Morpheme>& morphemes) {
  if (morphemes.size() < 2) {
    return morphemes;
  }

  std::vector<core::Morpheme> result;
  result.reserve(morphemes.size());

  size_t idx = 0;
  while (idx < morphemes.size()) {
    const auto& current = morphemes[idx];

    // Check if this is a na-adjective followed by な particle
    if (idx + 1 < morphemes.size() &&
        current.pos == core::PartOfSpeech::Adjective &&
        morphemes[idx + 1].pos == core::PartOfSpeech::Particle &&
        morphemes[idx + 1].surface == "な") {
      // Check if the adjective is a na-adjective (doesn't end with い)
      // Use lemma for checking since surface may be normalized
      bool is_na_adj = true;
      std::string_view check_str = current.lemma.empty() ? current.surface : current.lemma;
      if (check_str.size() >= core::kJapaneseCharBytes) {
        std::string_view last_char = check_str.substr(check_str.size() - core::kJapaneseCharBytes);
        // i-adjectives end with い (exceptions: きれい, きらい, 嫌い, みたい)
        if (last_char == "い" &&
            !utf8::equalsAny(check_str, {"きれい", "きらい", "嫌い", "みたい"})) {
          is_na_adj = false;
          SUZUME_DEBUG_LOG_VERBOSE("[POSTPROC] Detected i-adjective: \"" << check_str
                           << "\", not merging with な\n");
        }
      }

      if (is_na_adj) {
        // Merge na-adjective + な
        core::Morpheme merged = current;
        merged.surface += morphemes[idx + 1].surface;
        merged.end = morphemes[idx + 1].end;
        merged.end_pos = morphemes[idx + 1].end_pos;
        // Keep lemma as the base form (e.g., 静か)

        SUZUME_DEBUG_LOG_VERBOSE("[POSTPROC] Merged na-adj: \""
                         << current.surface << "\" + \"な\" → \""
                         << merged.surface << "\"\n");

        result.push_back(merged);
        idx += 2;
        continue;
      }
    }

    result.push_back(current);
    ++idx;
  }

  return result;
}

std::vector<core::Morpheme> Postprocessor::mergeProlongedSoundMark(
    const std::vector<core::Morpheme>& morphemes) {
  if (morphemes.size() < 2) {
    return morphemes;
  }

  std::vector<core::Morpheme> result;
  result.reserve(morphemes.size());

  for (size_t i = 0; i < morphemes.size(); ++i) {
    // Check if next morpheme is ー (or consecutive ーs)
    if (i + 1 < morphemes.size()) {
      const auto& next = morphemes[i + 1];
      bool next_is_prolonged = true;
      for (char32_t cp : normalize::toCodepoints(next.surface)) {
        if (cp != 0x30FC) {  // ー
          next_is_prolonged = false;
          break;
        }
      }

      if (next_is_prolonged && !next.surface.empty()) {
        const auto& current = morphemes[i];
        // Only merge if preceding token is not a symbol
        if (current.pos != core::PartOfSpeech::Symbol) {
          core::Morpheme merged = current;
          // Merge consecutive ーs into one ー
          merged.surface += "ー";
          merged.end = next.end;
          merged.end_pos = next.end_pos;
          // Update lemma
          if (!merged.lemma.empty()) {
            merged.lemma += "ー";
          }

          // Skip any additional ー tokens
          size_t skip = i + 2;
          while (skip < morphemes.size()) {
            bool is_prolonged = true;
            for (char32_t cp : normalize::toCodepoints(morphemes[skip].surface)) {
              if (cp != 0x30FC) {
                is_prolonged = false;
                break;
              }
            }
            if (!is_prolonged) break;
            merged.end = morphemes[skip].end;
            merged.end_pos = morphemes[skip].end_pos;
            ++skip;
          }

          SUZUME_DEBUG_LOG("[POSTPROC] Merged prolonged sound mark: \""
                          << current.surface << "\" + \"ー\" → \""
                          << merged.surface << "\"\n");
          result.push_back(merged);
          i = skip - 1;  // Will be incremented by loop
          continue;
        }
      }
    }
    result.push_back(morphemes[i]);
  }

  return result;
}

}  // namespace postprocess
