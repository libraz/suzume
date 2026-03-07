#include "postprocess/postprocessor.h"

#include "core/debug.h"
#include "core/utf8_constants.h"
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

  // Merge noun compounds
  if (options_.merge_noun_compounds) {
    before_count = result.size();
    result = mergeNounCompounds(result);
    if (result.size() != before_count) {
      SUZUME_DEBUG_LOG("[POSTPROC] mergeNounCompounds: " << before_count << " → " << result.size() << "\n");
    }
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
        if (m.pos == core::PartOfSpeech::Verb) {
          m.pos = core::PartOfSpeech::Noun;
          // Keep surface as lemma for nominalized form
          m.lemma = m.surface;
          SUZUME_DEBUG_LOG_VERBOSE("[POSTPROC] Nominalized: " << m.surface
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

// Check if a kanji codepoint is a counter/unit that naturally follows digits
bool isCounterKanji(char32_t cp) {
  switch (cp) {
    case U'円': case U'銭': case U'万': case U'億': case U'兆':
    case U'分': case U'秒': case U'時': case U'日': case U'月':
    case U'年': case U'週': case U'期': case U'世':
    case U'個': case U'本': case U'人': case U'台': case U'枚':
    case U'杯': case U'回': case U'歳': case U'才': case U'階':
    case U'号': case U'番': case U'匹': case U'冊': case U'件':
    case U'丁': case U'通': case U'発': case U'点': case U'票':
    case U'頭': case U'羽': case U'着': case U'足': case U'軒':
    case U'組': case U'曲': case U'巻': case U'畳': case U'割':
    case U'部': case U'面': case U'問': case U'章': case U'棟':
    case U'戸': case U'席': case U'食': case U'泊': case U'口':
    case U'束': case U'両': case U'機': case U'基': case U'隻':
    case U'度': case U'倍': case U'段': case U'級': case U'位':
    case U'種': case U'色': case U'名': case U'話': case U'連':
    case U'敗': case U'勝': case U'戦':
    case U'間': case U'紀':
      return true;
    default:
      return false;
  }
}

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
    if (current.pos == core::PartOfSpeech::Noun &&
        utf8::equalsAny(current.surface, {"数", "幾"}) &&
        idx + 1 < morphemes.size()) {
      const auto& next = morphemes[idx + 1];
      if (next.pos == core::PartOfSpeech::Suffix) {
        core::Morpheme merged = current;
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

}  // namespace postprocess
