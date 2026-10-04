#include "dict_compiler.h"

#include <fstream>
#include <iostream>
#include <utility>
#include <variant>

#include "cli_common.h"
#include "dictionary/binary_dict.h"
#include "grammar/dictionary_expansion.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"

namespace suzume::cli {

namespace {

// Marker written at the top of a decompiled dump and recognized by compile().
// Compiling a dump is a category error rather than a syntax error: see
// writeDecompiledDump() for why the two formats cannot be the same.
constexpr std::string_view kDumpMarker = "# suzume dictionary dump";

// Reject a decompiled dump before parsing: parsing it yields one "invalid
// conjugation type" error per expanded form, which buries the real problem.
core::Expected<std::monostate, core::Error> rejectDecompiledDump(const std::string& tsv_path) {
  std::ifstream probe(tsv_path);
  std::string first_line;
  if (probe && std::getline(probe, first_line) && first_line.rfind(kDumpMarker, 0) == 0) {
    return core::makeUnexpected(
        core::Error(core::ErrorCode::InvalidInput, tsv_path + " is a decompiled dump, not dictionary source. "
                                                              "Compilation expands conjugations, so a dump cannot be "
                                                              "recompiled; edit the source TSV under data/ instead."));
  }
  return std::monostate{};
}

}  // namespace

bool isTrivialEntry(std::string_view surface) {
  using normalize::CharType;

  // Entries containing spaces are always non-trivial (multi-word)
  if (surface.find(' ') != std::string_view::npos) {
    return false;
  }

  auto codepoints = normalize::toCodepoints(surface);

  // 2-char entries are always non-trivial (short words need dict help)
  if (codepoints.size() <= 2) {
    return false;
  }

  // Check if all meaningful chars have the same type
  CharType primary_type = CharType::Unknown;
  for (char32_t cpt : codepoints) {
    auto char_type = normalize::classifyChar(cpt);
    // Ignore Symbol and Unknown characters for type comparison
    if (char_type == CharType::Symbol || char_type == CharType::Unknown) {
      continue;
    }
    if (primary_type == CharType::Unknown) {
      primary_type = char_type;
    } else if (char_type != primary_type) {
      // Mixed character types found: non-trivial
      return false;
    }
  }

  // A "trivial" entry is a run the tokenizer already reconstructs from character
  // type alone: a pure-katakana run (loanword) or a pure-kanji run (compound).
  // Pure-hiragana words carry real lexical value and are NOT reconstructable
  // (e.g. つめあわせ would mis-split into つめあわ + せ), so they are never
  // trivial — nor are alphabet/digit-only or symbol-only surfaces.
  return primary_type == CharType::Katakana || primary_type == CharType::Kanji;
}

DictCompiler::DictCompiler() = default;

core::Expected<size_t, core::Error> DictCompiler::compile(const std::string& tsv_path, const std::string& dic_path) {
  auto dump_check = rejectDecompiledDump(tsv_path);
  if (!dump_check.hasValue()) {
    return core::makeUnexpected(dump_check.error());
  }

  TsvParser parser;
  auto parse_result = parser.parseFile(tsv_path);

  if (!parse_result.hasValue()) {
    return core::makeUnexpected(parse_result.error());
  }

  const auto& entries = parse_result.value();

  if (verbose_) {
    printInfo("Parsed " + std::to_string(entries.size()) + " entries from " + tsv_path);
  }

  // Validate entries
  std::vector<std::string> issues;
  size_t issue_count = suzume::cli::TsvParser::validate(entries, &issues);
  if (issue_count > 0) {
    for (const auto& issue : issues) {
      printError(issue);
    }
    return core::makeUnexpected(
        core::Error(core::ErrorCode::InvalidInput, "Validation failed: " + std::to_string(issue_count) + " error(s)"));
  }

  return compileEntries(entries, dic_path);
}

core::Expected<size_t, core::Error> DictCompiler::compileEntries(const std::vector<TsvEntry>& entries,
                                                                 const std::string& dic_path) {
  if (entries.empty()) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "No entries to compile"));
  }

  // Apply trivial entry filter if enabled
  const std::vector<TsvEntry>* active_entries = &entries;
  std::vector<TsvEntry> filtered_entries;
  if (filter_trivial_) {
    filtered_entries.reserve(entries.size());
    size_t trivial_count = 0;
    for (const auto& entry : entries) {
      if (isTrivialEntry(entry.surface)) {
        ++trivial_count;
      } else {
        filtered_entries.push_back(entry);
      }
    }
    if (trivial_count > 0) {
      printInfo("Filtered " + std::to_string(trivial_count) + " trivial entries (kept " +
                std::to_string(filtered_entries.size()) + ")");
    }
    active_entries = &filtered_entries;

    if (active_entries->empty()) {
      return core::makeUnexpected(
          core::Error(core::ErrorCode::InvalidInput, "No entries remaining after trivial filtering"));
    }
  }

  dictionary::BinaryDictWriter writer;
  grammar::DictionaryExpansionOptions expansion_options;
  expansion_options.preserve_surface_homographs = true;
  expansion_options.preserve_same_pos_homographs = false;
  expansion_options.preserve_generated_surface_homographs = false;
  auto expanded = grammar::expandDictionarySourceEntries(*active_entries, expansion_options);
  for (const auto& entry : expanded.entries) {
    writer.addEntry(entry);
  }
  entries_compiled_ = expanded.entries.size();
  conj_expanded_ = expanded.expanded_forms;

  if (verbose_ && conj_expanded_ > 0) {
    printInfo("Expanded " + std::to_string(conj_expanded_) + " conjugated forms");
  }

  if (expanded.duplicates_skipped > 0) {
    printWarning("Skipped " + std::to_string(expanded.duplicates_skipped) +
                 " duplicate expanded entries inside the same POS");
  }

  auto write_result = writer.writeToFile(dic_path);
  if (!write_result.hasValue()) {
    return core::makeUnexpected(write_result.error());
  }

  if (verbose_) {
    printInfo("Compiled " + std::to_string(entries_compiled_) + " entries to " + dic_path);
    printInfo("Output size: " + std::to_string(write_result.value()) + " bytes");
  }

  return entries_compiled_;
}

core::Expected<size_t, core::Error> DictCompiler::compileMultiple(const std::vector<std::string>& tsv_paths,
                                                                  const std::string& dic_path) {
  if (tsv_paths.empty()) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InvalidInput, "No input files specified"));
  }

  std::vector<TsvEntry> all_entries;
  TsvParser parser;

  // Parse all TSV files
  for (const auto& tsv_path : tsv_paths) {
    auto dump_check = rejectDecompiledDump(tsv_path);
    if (!dump_check.hasValue()) {
      return core::makeUnexpected(dump_check.error());
    }

    auto parse_result = parser.parseFile(tsv_path);
    if (!parse_result.hasValue()) {
      return core::makeUnexpected(core::Error(core::ErrorCode::ParseError,
                                              "Failed to parse " + tsv_path + ": " + parse_result.error().message));
    }

    auto entries = std::move(parse_result.value());
    all_entries.reserve(all_entries.size() + entries.size());
    for (auto& entry : entries) {
      all_entries.push_back(std::move(entry));
    }

    if (verbose_) {
      printInfo("Parsed " + std::to_string(entries.size()) + " entries from " + tsv_path);
    }
  }

  if (verbose_) {
    printInfo("Total entries before deduplication: " + std::to_string(all_entries.size()));
  }

  // Validate entries
  std::vector<std::string> issues;
  size_t issue_count = suzume::cli::TsvParser::validate(all_entries, &issues);
  if (issue_count > 0) {
    for (const auto& issue : issues) {
      printError(issue);
    }
    return core::makeUnexpected(
        core::Error(core::ErrorCode::InvalidInput, "Validation failed: " + std::to_string(issue_count) + " error(s)"));
  }

  return compileEntries(all_entries, dic_path);
}

core::Expected<size_t, core::Error> DictCompiler::decompile(const std::string& dic_path,
                                                            const std::string& tsv_path) const {
  dictionary::BinaryDictionary dict;
  auto load_result = dict.loadFromFile(dic_path);

  if (!load_result.hasValue()) {
    return core::makeUnexpected(load_result.error());
  }

  std::ofstream file(tsv_path);
  if (!file) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InternalError, "Failed to create file: " + tsv_path));
  }

  // A compiled dictionary holds the EXPANDED entries: one source row such as
  // "黙る VERB GODAN_RA" becomes 黙ら/黙り/黙る/黙れ/黙ろ/…, each with its own
  // extended POS and lemma, and the source row itself is not kept. There is
  // therefore no source TSV to recover — writing one would re-expand every
  // conjugated form on the next compile. This dump reports what the binary
  // actually contains, and compile() refuses to read it back.
  file << kDumpMarker << " (expanded entries, not compiler input)\n";
  file << "# Source rows are not recoverable: compilation expands conjugations.\n";
  file << "# Format: surface<TAB>pos<TAB>extended_pos<TAB>lemma\n";
  file << "\n";

  size_t written = 0;
  for (size_t idx = 0; idx < dict.size(); ++idx) {
    const auto* entry = dict.getEntry(static_cast<uint32_t>(idx));
    if (entry == nullptr) {
      continue;
    }
    file << entry->surface << "\t" << core::posToString(entry->pos) << "\t"
         << core::extendedPosToString(entry->extended_pos) << "\t" << entry->lemma << "\n";
    ++written;
  }

  if (!file) {
    return core::makeUnexpected(core::Error(core::ErrorCode::InternalError, "Failed to write file: " + tsv_path));
  }

  if (verbose_) {
    printInfo("Decompiled " + std::to_string(written) + " entries to " + tsv_path);
  }

  return written;
}

}  // namespace suzume::cli
