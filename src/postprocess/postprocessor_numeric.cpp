#include <utility>

#include "core/debug.h"
#include "core/utf8_constants.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "postprocess/postprocessor.h"
#include "postprocess/postprocessor_resolvers_internal.h"

namespace suzume::postprocess {

namespace {

// Check if a character is a digit (ASCII or fullwidth)
bool isDigitChar(char32_t ch) {
  return (ch >= U'0' && ch <= U'9') || (ch >= U'０' && ch <= U'９');
}

// Check if surface is a numeric expression (starts with digit or contains units)
bool isNumericExpression(const std::string& surface) {
  if (surface.empty())
    return false;

  size_t pos = 0;
  char32_t first_ch = suzume::normalize::decodeUtf8(surface, pos);
  return isDigitChar(first_ch);
}

// Check if a one-character surface closes a number+counter phrase (3時+間, 3人+目)
bool isQuantityPhraseSuffixSurface(const std::string& surface) {
  if (surface.size() != core::kJapaneseCharBytes) {
    return false;
  }
  size_t pos = 0;
  return suzume::normalize::isQuantityPhraseSuffixKanji(suzume::normalize::decodeUtf8(surface, pos));
}

// Check if surface ends with a digit
bool endsWithDigit(const std::string& surface) {
  if (surface.empty())
    return false;

  size_t pos = 0;
  char32_t last_ch = 0;
  while (pos < surface.size()) {
    last_ch = suzume::normalize::decodeUtf8(surface, pos);
  }
  return isDigitChar(last_ch);
}

using normalize::isAllKatakana;
using normalize::isCounterKanji;

// Check if surface looks like a unit (noun that can follow numbers)
// For kanji: must start with a counter kanji (円, 分, 時間, etc.)
// For katakana: any katakana noun merges (MeCab treats number + katakana as one
// quantity token, e.g. 3キロ, 100メダル), so no curated unit list is needed.
bool looksLikeUnit(const std::string& surface) {
  if (surface.empty())
    return false;

  // Only the first codepoint drives the kanji-unit branch; kanji/katakana are
  // 3-byte, so decoding just the leading char avoids allocating a codepoint
  // vector. Non-3-byte leads decode to 0 and fall through to the katakana check.
  char32_t first = utf8::decodeFirstChar(surface);

  // Kanji units: first char must be a counter kanji
  // CJK Unified Ideographs: U+4E00-U+9FFF
  if (first >= 0x4E00 && first <= 0x9FFF) {
    return isCounterKanji(first);
  }

  // Katakana nouns: any all-katakana surface merges with a preceding numeral
  if (isAllKatakana(surface)) {
    return true;
  }

  return false;
}

// Check if surface ends with a numeric unit that can be followed by more numbers
bool endsWithContinuableUnit(const std::string& surface) {
  if (surface.empty())
    return false;

  // Targets 兆/億/万/千/百 are all 3-byte kanji; decode only the trailing char.
  char32_t last_ch = utf8::decodeLastChar(surface);
  // Units that can be followed by more numbers (兆, 億, 万, 千, 百)
  return last_ch == U'兆' || last_ch == U'億' || last_ch == U'万' || last_ch == U'千' || last_ch == U'百';
}

}  // namespace

std::vector<core::Morpheme> Postprocessor::mergeNumericExpressions(std::vector<core::Morpheme> morphemes) {
  if (morphemes.empty()) {
    return morphemes;
  }

  std::vector<core::Morpheme> result;
  result.reserve(morphemes.size());

  size_t idx = 0;
  while (idx < morphemes.size()) {
    const auto& current = morphemes[idx];

    // Pattern 0: Ordinal prefix + numeric expression (第 + 3回 → 第3回).
    // The prefix scopes the complete quantity, including an optional ordinal
    // suffix (第3回目), so retain it as one search unit.
    if (current.pos == core::PartOfSpeech::Noun && utf8::equalsAny(current.surface, {"第"}) &&
        idx + 1 < morphemes.size()) {
      const auto& next = morphemes[idx + 1];
      if (next.pos == core::PartOfSpeech::Noun && isNumericExpression(next.surface)) {
        core::Morpheme merged = current;
        resolver::mergeInto(merged, next);
        merged.lemma = merged.surface;
        size_t merge_end = idx + 2;

        if (merge_end < morphemes.size() && morphemes[merge_end].pos == core::PartOfSpeech::Noun &&
            utf8::equalsAny(morphemes[merge_end].surface, {"目"})) {
          resolver::mergeInto(merged, morphemes[merge_end]);
          merged.lemma = merged.surface;
          ++merge_end;
        }

        result.push_back(std::move(merged));
        idx = merge_end;
        continue;
      }
    }

    // Pattern 1: Merge large numbers (3億 + 5000万円)
    if (current.pos == core::PartOfSpeech::Noun && isNumericExpression(current.surface) &&
        endsWithContinuableUnit(current.surface)) {
      core::Morpheme merged = current;
      size_t merge_end = idx + 1;

      // Collect consecutive numeric expressions
      while (merge_end < morphemes.size()) {
        const auto& next = morphemes[merge_end];
        if (next.pos == core::PartOfSpeech::Noun && isNumericExpression(next.surface)) {
          resolver::mergeInto(merged, next);
          merged.lemma = merged.surface;
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
          if (i > idx)
            SUZUME_DEBUG_STREAM << " + ";
          SUZUME_DEBUG_STREAM << "\"" << morphemes[i].surface << "\"";
        }
        SUZUME_DEBUG_STREAM << " → \"" << merged.surface << "\"\n";
      }

      result.push_back(std::move(merged));
      idx = merge_end;
      continue;
    }

    // Patterns 2/3: Merge a numeric expression with a following unit. The
    // digit-led unit case takes precedence over a quantity suffix exactly as
    // before; only the identical merge/output path is shared.
    if (current.pos == core::PartOfSpeech::Noun && isNumericExpression(current.surface) && idx + 1 < morphemes.size()) {
      const auto& next = morphemes[idx + 1];
      const bool comma_grouped = current.surface.find(',') != std::string::npos;
      if (comma_grouped && next.pos == core::PartOfSpeech::Noun && looksLikeUnit(next.surface)) {
        // The pretokenizer deliberately isolates a grouped numeral from its
        // counter.  Preserve that search boundary while giving the dependent
        // counter its grammatical suffix tag.
        core::Morpheme counter = next;
        counter.pos = core::PartOfSpeech::Suffix;
        counter.extended_pos = core::ExtendedPOS::Suffix;
        counter.lemma = counter.surface;
        result.push_back(std::move(morphemes[idx]));
        result.push_back(std::move(counter));
        idx += 2;
        continue;
      }
      // A comma-grouped numeral is pretokenized as a complete numeric search
      // unit.  Do not absorb its following counter here: unlike a contiguous
      // digit run, the grouping separator supplies an explicit boundary.
      const bool is_number_unit =
          !comma_grouped && endsWithDigit(current.surface) && looksLikeUnit(next.surface) && next.surface != "対";
      const bool is_quantity_suffix = isQuantityPhraseSuffixSurface(next.surface);
      if (next.pos == core::PartOfSpeech::Noun && (is_number_unit || is_quantity_suffix)) {
        core::Morpheme merged = current;
        resolver::mergeInto(merged, next);
        merged.lemma = merged.surface;

        SUZUME_DEBUG_LOG_VERBOSE("[POSTPROC] Merged " << (is_number_unit ? "number+unit" : "numeric+suffix") << ": \""
                                                      << current.surface << "\" + \"" << next.surface << "\" → \""
                                                      << merged.surface << "\"\n");

        result.push_back(std::move(merged));
        idx += 2;
        continue;
      }
    }

    // Pattern 4: Merge indefinite numeral + counter suffix (数 + ヶ月 → 数ヶ月)
    // Indefinite numerals: 数 (suu = some/several), 幾 (iku = how many)
    if ((current.pos == core::PartOfSpeech::Noun || current.pos == core::PartOfSpeech::Pronoun) &&
        utf8::equalsAny(current.surface, {"数", "幾", "何"}) && idx + 1 < morphemes.size()) {
      const auto& next = morphemes[idx + 1];
      if (next.pos == core::PartOfSpeech::Suffix) {
        core::Morpheme merged = current;
        merged.pos = core::PartOfSpeech::Noun;  // Merged result is always NOUN
        resolver::mergeInto(merged, next);
        merged.extended_pos = core::ExtendedPOS::NounNumber;
        merged.lemma = merged.surface;

        SUZUME_DEBUG_LOG_VERBOSE("[POSTPROC] Merged indefinite+suffix: \""
                                 << current.surface << "\" + \"" << next.surface << "\" → \"" << merged.surface
                                 << "\"\n");

        result.push_back(std::move(merged));
        idx += 2;
        continue;
      }
    }

    result.push_back(std::move(morphemes[idx]));
    ++idx;
  }

  return result;
}

}  // namespace suzume::postprocess
