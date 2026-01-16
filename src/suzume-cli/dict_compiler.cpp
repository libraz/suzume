#include "dict_compiler.h"

#include <iostream>
#include <set>
#include <tuple>

#include "cli_common.h"
#include "dictionary/binary_dict.h"

namespace suzume::cli {

DictCompiler::DictCompiler() = default;

core::Expected<size_t, core::Error> DictCompiler::compile(
    const std::string& tsv_path, const std::string& dic_path) {
  TsvParser parser;
  auto parse_result = parser.parseFile(tsv_path);

  if (!parse_result.hasValue()) {
    return core::makeUnexpected(parse_result.error());
  }

  const auto& entries = parse_result.value();

  if (verbose_) {
    printInfo("Parsed " + std::to_string(entries.size()) + " entries from " +
              tsv_path);
  }

  // Validate entries
  std::vector<std::string> issues;
  size_t issue_count = suzume::cli::TsvParser::validate(entries, &issues);
  if (issue_count > 0) {
    for (const auto& issue : issues) {
      printError(issue);
    }
    return core::makeUnexpected(core::Error(
        core::ErrorCode::InvalidInput,
        "Validation failed: " + std::to_string(issue_count) + " error(s)"));
  }

  return compileEntries(entries, dic_path);
}

core::Expected<size_t, core::Error> DictCompiler::compileEntries(
    const std::vector<TsvEntry>& entries, const std::string& dic_path) {
  if (entries.empty()) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput,
                                             "No entries to compile"));
  }

  dictionary::BinaryDictWriter writer;
  entries_compiled_ = 0;
  conj_expanded_ = 0;
  size_t duplicates_skipped = 0;

  // Deduplication: track (surface, pos, cost) tuples
  std::set<std::tuple<std::string, core::PartOfSpeech, float>> seen;

  size_t reading_entries_added = 0;

  for (const auto& tsv_entry : entries) {
    // Create key for deduplication
    auto key = std::make_tuple(tsv_entry.surface, tsv_entry.pos, tsv_entry.cost);

    // Skip if already seen (same surface, POS, and cost)
    if (seen.count(key) > 0) {
      ++duplicates_skipped;
      continue;
    }
    seen.insert(key);

    dictionary::DictionaryEntry entry;
    entry.surface = tsv_entry.surface;
    entry.pos = tsv_entry.pos;
    entry.cost = tsv_entry.cost;
    entry.lemma = tsv_entry.surface;

    writer.addEntry(entry, tsv_entry.conj_type);
    ++entries_compiled_;

    // Auto-generate reading-based entry for safe POS types
    // Only closed class / function words are safe for hiragana expansion
    // Regular nouns are excluded because of many homophones
    // (e.g., 橋/箸/端 all read はし)
    if (!tsv_entry.reading.empty() && tsv_entry.reading != tsv_entry.surface) {
      bool should_expand = false;
      switch (tsv_entry.pos) {
        case core::PartOfSpeech::Adjective:   // 形容詞
        case core::PartOfSpeech::Adverb:      // 副詞
        case core::PartOfSpeech::Conjunction: // 接続詞
        case core::PartOfSpeech::Pronoun:     // 代名詞
          should_expand = true;
          break;
        default:
          break;
      }

      if (should_expand) {
        auto reading_key =
            std::make_tuple(tsv_entry.reading, tsv_entry.pos, tsv_entry.cost);
        if (seen.count(reading_key) == 0) {
          seen.insert(reading_key);

          dictionary::DictionaryEntry reading_entry;
          reading_entry.surface = tsv_entry.reading;
          reading_entry.pos = tsv_entry.pos;
          reading_entry.cost = tsv_entry.cost;
          reading_entry.lemma = tsv_entry.reading;  // Lemma is the reading itself (MeCab-compatible)

          writer.addEntry(reading_entry, tsv_entry.conj_type);
          ++entries_compiled_;
          ++reading_entries_added;
        }
      }
    }
  }

  if (verbose_ && reading_entries_added > 0) {
    printInfo("Added " + std::to_string(reading_entries_added) +
              " reading-based entries");
  }

  if (verbose_ && duplicates_skipped > 0) {
    printInfo("Skipped " + std::to_string(duplicates_skipped) +
              " duplicate entries");
  }

  auto write_result = writer.writeToFile(dic_path);
  if (!write_result.hasValue()) {
    return core::makeUnexpected(write_result.error());
  }

  if (verbose_) {
    printInfo("Compiled " + std::to_string(entries_compiled_) +
              " entries to " + dic_path);
    printInfo("Output size: " + std::to_string(write_result.value()) +
              " bytes");
  }

  return entries_compiled_;
}

core::Expected<size_t, core::Error> DictCompiler::compileMultiple(
    const std::vector<std::string>& tsv_paths, const std::string& dic_path) {
  if (tsv_paths.empty()) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::InvalidInput, "No input files specified"));
  }

  std::vector<TsvEntry> all_entries;
  TsvParser parser;

  // Parse all TSV files
  for (const auto& tsv_path : tsv_paths) {
    auto parse_result = parser.parseFile(tsv_path);
    if (!parse_result.hasValue()) {
      return core::makeUnexpected(core::Error(
          core::ErrorCode::ParseError,
          "Failed to parse " + tsv_path + ": " + parse_result.error().message));
    }

    const auto& entries = parse_result.value();
    all_entries.insert(all_entries.end(), entries.begin(), entries.end());

    if (verbose_) {
      printInfo("Parsed " + std::to_string(entries.size()) + " entries from " +
                tsv_path);
    }
  }

  if (verbose_) {
    printInfo("Total entries before deduplication: " +
              std::to_string(all_entries.size()));
  }

  // Deduplicate by surface (trie requires unique keys)
  std::set<std::string> seen_surfaces;
  std::vector<TsvEntry> unique_entries;
  unique_entries.reserve(all_entries.size());

  for (const auto& entry : all_entries) {
    if (seen_surfaces.count(entry.surface) == 0) {
      seen_surfaces.insert(entry.surface);
      unique_entries.push_back(entry);
    }
  }

  if (verbose_) {
    size_t skipped = all_entries.size() - unique_entries.size();
    if (skipped > 0) {
      printInfo("Skipped " + std::to_string(skipped) +
                " duplicate surfaces during merge");
    }
  }

  // Validate entries
  std::vector<std::string> issues;
  size_t issue_count = suzume::cli::TsvParser::validate(unique_entries, &issues);
  if (issue_count > 0) {
    for (const auto& issue : issues) {
      printError(issue);
    }
    return core::makeUnexpected(core::Error(
        core::ErrorCode::InvalidInput,
        "Validation failed: " + std::to_string(issue_count) + " error(s)"));
  }

  return compileEntries(unique_entries, dic_path);
}

core::Expected<size_t, core::Error> DictCompiler::decompile(const std::string& dic_path,
                                                            const std::string& tsv_path) const {
  dictionary::BinaryDictionary dict;
  auto load_result = dict.loadFromFile(dic_path);

  if (!load_result.hasValue()) {
    return core::makeUnexpected(load_result.error());
  }

  std::vector<TsvEntry> entries;
  entries.reserve(dict.size());

  for (size_t idx = 0; idx < dict.size(); ++idx) {
    const auto* entry = dict.getEntry(static_cast<uint32_t>(idx));
    if (entry != nullptr) {
      TsvEntry tsv_entry;
      tsv_entry.surface = entry->surface;
      tsv_entry.pos = entry->pos;
      tsv_entry.cost = entry->cost;
      tsv_entry.reading = "";  // Binary format doesn't store reading
      tsv_entry.conj_type = dictionary::ConjugationType::None;
      entries.push_back(std::move(tsv_entry));
    }
  }

  auto write_result = writeTsvFile(tsv_path, entries);
  if (!write_result.hasValue()) {
    return core::makeUnexpected(write_result.error());
  }

  if (verbose_) {
    printInfo("Decompiled " + std::to_string(entries.size()) +
              " entries to " + tsv_path);
  }

  return entries.size();
}

}  // namespace suzume::cli
