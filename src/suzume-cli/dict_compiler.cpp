#include "dict_compiler.h"

#include <iostream>

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
      printWarning(issue);
    }
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

  for (const auto& tsv_entry : entries) {
    dictionary::DictionaryEntry entry;
    entry.surface = tsv_entry.surface;
    entry.pos = tsv_entry.pos;
    entry.cost = tsv_entry.cost;
    entry.lemma = tsv_entry.surface;

    writer.addEntry(entry, tsv_entry.conj_type);
    ++entries_compiled_;

    // TODO: Add conjugation expansion for verbs/adjectives
    // For now, just add the base form
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
