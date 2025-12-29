#include "postprocess/postprocessor.h"

#include "core/debug.h"

namespace suzume::postprocess {

Postprocessor::Postprocessor(const PostprocessOptions& options)
    : options_(options), lemmatizer_() {}

Postprocessor::Postprocessor(const dictionary::DictionaryManager* dict_manager,
                             const PostprocessOptions& options)
    : options_(options), lemmatizer_(dict_manager) {}

std::vector<core::Morpheme> Postprocessor::process(
    const std::vector<core::Morpheme>& morphemes) const {
  std::vector<core::Morpheme> result = morphemes;

  // Note: NOUN + SUFFIX merging is intentionally disabled.
  // We keep tokens separate: PREFIX + NOUN + SUFFIX
  // e.g., お姉さん → お(PREFIX) + 姉(NOUN) + さん(SUFFIX)
  // result = mergeNounSuffix(result);

  // Convert PREFIX + VERB to PREFIX + NOUN (renyoukei nominalization)
  // e.g., お願い → お(PREFIX) + 願い(NOUN), not 願い(VERB)
  result = convertPrefixVerbToNoun(result);

  // Merge consecutive numeric expressions (always applied)
  result = mergeNumericExpressions(result);

  // Merge na-adjective + な into attributive form (always applied)
  result = mergeNaAdjectiveNa(result);

  // Apply lemmatization
  if (options_.lemmatize) {
    lemmatizer_.lemmatizeAll(result);
  }

  // Merge noun compounds
  if (options_.merge_noun_compounds) {
    result = mergeNounCompounds(result);
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

      if (merge_count > 1) {
        SUZUME_DEBUG_LOG("[POSTPROC] Merged " << merge_count << " nouns: ");
        for (size_t i = idx; i < merge_end; ++i) {
          if (i > idx) core::Debug::log() << " + ";
          core::Debug::log() << "\"" << morphemes[i].surface << "\"";
        }
        core::Debug::log() << " → \"" << merged.surface << "\"\n";
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
      if (prefix_surface == "お" || prefix_surface == "ご" ||
          prefix_surface == "御") {
        // Convert VERB to NOUN (renyoukei nominalization)
        // e.g., 願い(VERB) → 願い(NOUN) after お
        if (m.pos == core::PartOfSpeech::Verb) {
          m.pos = core::PartOfSpeech::Noun;
          // Keep surface as lemma for nominalized form
          m.lemma = m.surface;
          SUZUME_DEBUG_LOG("[POSTPROC] Nominalized: " << m.surface
                           << " (VERB → NOUN after " << prefix_surface << ")\n");
        }
      }
    }

    result.push_back(m);
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
        SUZUME_DEBUG_LOG("[POSTPROC] Merged noun+suffix: ");
        for (size_t i = idx; i < merge_end; ++i) {
          if (i > idx) core::Debug::log() << " + ";
          core::Debug::log() << "\"" << morphemes[i].surface << "\"";
        }
        core::Debug::log() << " → \"" << merged.surface << "\"\n";
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

// Check if a character is a numeric unit (億, 万, 千, 百, 円, %, etc.)
bool isNumericUnit(char32_t ch) {
  return ch == U'億' || ch == U'万' || ch == U'千' || ch == U'百' ||
         ch == U'兆' || ch == U'円' || ch == U'％' || ch == U'%' ||
         ch == U'個' || ch == U'人' || ch == U'回' || ch == U'時' ||
         ch == U'分' || ch == U'秒' || ch == U'日' || ch == U'月' ||
         ch == U'年' || ch == U'週' || ch == U'度' || ch == U'倍';
}

// Check if surface is a numeric expression (starts with digit or contains units)
bool isNumericExpression(const std::string& surface) {
  if (surface.empty()) return false;

  // Decode first character
  size_t i = 0;
  char32_t first_ch = 0;
  if ((surface[0] & 0x80) == 0) {
    first_ch = static_cast<char32_t>(surface[0]);
  } else if ((surface[0] & 0xE0) == 0xC0 && i + 1 < surface.size()) {
    first_ch = ((surface[0] & 0x1F) << 6) | (surface[1] & 0x3F);
  } else if ((surface[0] & 0xF0) == 0xE0 && i + 2 < surface.size()) {
    first_ch = ((surface[0] & 0x0F) << 12) | ((surface[1] & 0x3F) << 6) |
               (surface[2] & 0x3F);
  } else if ((surface[0] & 0xF8) == 0xF0 && i + 3 < surface.size()) {
    first_ch = ((surface[0] & 0x07) << 18) | ((surface[1] & 0x3F) << 12) |
               ((surface[2] & 0x3F) << 6) | (surface[3] & 0x3F);
  }

  return isDigitChar(first_ch);
}

// Check if surface ends with a digit
bool endsWithDigit(const std::string& surface) {
  if (surface.empty()) return false;

  // Get last character (UTF-8)
  size_t i = surface.size();
  while (i > 0 && (surface[i - 1] & 0xC0) == 0x80) {
    --i;
  }
  if (i == 0) return false;

  char32_t last_ch = 0;
  size_t start = i - 1;
  if ((surface[start] & 0x80) == 0) {
    last_ch = static_cast<char32_t>(surface[start]);
  } else if ((surface[start] & 0xE0) == 0xC0 && start + 1 < surface.size()) {
    last_ch = ((surface[start] & 0x1F) << 6) | (surface[start + 1] & 0x3F);
  } else if ((surface[start] & 0xF0) == 0xE0 && start + 2 < surface.size()) {
    last_ch = ((surface[start] & 0x0F) << 12) |
              ((surface[start + 1] & 0x3F) << 6) | (surface[start + 2] & 0x3F);
  }

  return isDigitChar(last_ch);
}

// Check if surface looks like a unit (short noun that can follow numbers)
// Examples: 時間, 分, キロ, メートル, 円, ゴールド, etc.
bool looksLikeUnit(const std::string& surface) {
  if (surface.empty()) return false;

  // Units are typically 1-4 characters
  // Count UTF-8 codepoints
  size_t codepoints = 0;
  for (size_t i = 0; i < surface.size(); ) {
    if ((surface[i] & 0x80) == 0) {
      ++i;
    } else if ((surface[i] & 0xE0) == 0xC0) {
      i += 2;
    } else if ((surface[i] & 0xF0) == 0xE0) {
      i += 3;
    } else {
      i += 4;
    }
    ++codepoints;
  }

  // Units are typically 1-5 characters (e.g., キロ, メートル, パーセント)
  return codepoints >= 1 && codepoints <= 5;
}

// Check if surface ends with a numeric unit that can be followed by more numbers
bool endsWithContinuableUnit(const std::string& surface) {
  if (surface.empty()) return false;

  // Get last character (UTF-8)
  size_t i = surface.size();
  while (i > 0 && (surface[i - 1] & 0xC0) == 0x80) {
    --i;
  }
  if (i == 0) return false;

  char32_t last_ch = 0;
  size_t start = i - 1;
  if ((surface[start] & 0x80) == 0) {
    last_ch = static_cast<char32_t>(surface[start]);
  } else if ((surface[start] & 0xE0) == 0xC0 && start + 1 < surface.size()) {
    last_ch = ((surface[start] & 0x1F) << 6) | (surface[start + 1] & 0x3F);
  } else if ((surface[start] & 0xF0) == 0xE0 && start + 2 < surface.size()) {
    last_ch = ((surface[start] & 0x0F) << 12) |
              ((surface[start + 1] & 0x3F) << 6) | (surface[start + 2] & 0x3F);
  } else if ((surface[start] & 0xF8) == 0xF0 && start + 3 < surface.size()) {
    last_ch = ((surface[start] & 0x07) << 18) |
              ((surface[start + 1] & 0x3F) << 12) |
              ((surface[start + 2] & 0x3F) << 6) | (surface[start + 3] & 0x3F);
  }

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

      if (merge_end > idx + 1) {
        SUZUME_DEBUG_LOG("[POSTPROC] Merged numeric: ");
        for (size_t i = idx; i < merge_end; ++i) {
          if (i > idx) core::Debug::log() << " + ";
          core::Debug::log() << "\"" << morphemes[i].surface << "\"";
        }
        core::Debug::log() << " → \"" << merged.surface << "\"\n";
      }

      result.push_back(merged);
      idx = merge_end;
      continue;
    }

    // Pattern 2: Merge number + unit (3 + 時間, 100 + ゴールド, 3時 + 間)
    if (current.pos == core::PartOfSpeech::Noun &&
        isNumericExpression(current.surface) &&
        endsWithDigit(current.surface) &&
        idx + 1 < morphemes.size()) {
      const auto& next = morphemes[idx + 1];
      if (next.pos == core::PartOfSpeech::Noun && looksLikeUnit(next.surface)) {
        core::Morpheme merged = current;
        merged.surface += next.surface;
        merged.lemma = merged.surface;
        merged.end = next.end;
        merged.end_pos = next.end_pos;

        SUZUME_DEBUG_LOG("[POSTPROC] Merged number+unit: \""
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
          (next.surface == "間" || next.surface == "半" ||
           next.surface == "前" || next.surface == "後" ||
           next.surface == "目")) {
        core::Morpheme merged = current;
        merged.surface += next.surface;
        merged.lemma = merged.surface;
        merged.end = next.end;
        merged.end_pos = next.end_pos;

        SUZUME_DEBUG_LOG("[POSTPROC] Merged numeric+suffix: \""
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
      bool is_na_adj = true;
      if (current.surface.size() >= 3) {
        std::string_view last3 = current.surface.substr(current.surface.size() - 3);
        // i-adjectives end with い (exceptions: きれい, きらい, 嫌い, みたい)
        if (last3 == "い" && current.surface != "きれい" &&
            current.surface != "きらい" && current.surface != "嫌い" &&
            current.surface != "みたい") {
          is_na_adj = false;
        }
      }

      if (is_na_adj) {
        // Merge na-adjective + な
        core::Morpheme merged = current;
        merged.surface += morphemes[idx + 1].surface;
        merged.end = morphemes[idx + 1].end;
        merged.end_pos = morphemes[idx + 1].end_pos;
        // Keep lemma as the base form (e.g., 静か)

        SUZUME_DEBUG_LOG("[POSTPROC] Merged na-adj: \""
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

}  // namespace postprocess
