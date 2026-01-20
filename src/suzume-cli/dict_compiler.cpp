#include "dict_compiler.h"

#include <iostream>
#include <set>
#include <tuple>

#include "cli_common.h"
#include "core/utf8_constants.h"
#include "dictionary/binary_dict.h"
#include "grammar/conjugation.h"

namespace suzume::cli {

namespace {

// I-adjective conjugation suffixes for dictionary expansion
// These are the forms that should be stored in dictionary (MeCab-compatible)
// Excludes compound forms that should be split (e.g., くなる → く + なる)
struct IAdjSuffix {
  const char* suffix;
  core::ExtendedPOS extended_pos;
};

const std::vector<IAdjSuffix> kIAdjSuffixes = {
    {"い", core::ExtendedPOS::AdjBasic},            // Base form: 美しい
    {"かった", core::ExtendedPOS::AdjBasic},        // Past: 美しかった
    {"かっ", core::ExtendedPOS::AdjKatt},           // Ta-connection (連用タ接続): 美しかっ+た
    {"くない", core::ExtendedPOS::AdjBasic},        // Negative: 美しくない
    {"くなかった", core::ExtendedPOS::AdjBasic},    // Negative past: 美しくなかった
    {"くて", core::ExtendedPOS::AdjRenyokei},       // Te-form: 美しくて
    {"ければ", core::ExtendedPOS::AdjKeForm},       // Conditional: 美しければ
    {"く", core::ExtendedPOS::AdjRenyokei},         // Adverbial/Renyokei: 美しく
    {"かったら", core::ExtendedPOS::AdjKeForm},     // Conditional past: 美しかったら
    {"そう", core::ExtendedPOS::AdjStem},           // Looks like: 美しそう
};

// Expand i-adjective entry into all conjugated forms
std::vector<dictionary::DictionaryEntry> expandIAdjective(
    const dictionary::DictionaryEntry& entry) {
  std::vector<dictionary::DictionaryEntry> result;

  // Check if it ends with い
  if (!utf8::endsWith(entry.surface, "い")) {
    result.push_back(entry);
    return result;
  }

  // Get stem by removing final い
  std::string stem(utf8::dropLastChar(entry.surface));
  std::string lemma = entry.lemma.empty() ? entry.surface : entry.lemma;

  for (const auto& entry_info : kIAdjSuffixes) {
    dictionary::DictionaryEntry new_entry;
    new_entry.surface = stem + entry_info.suffix;
    new_entry.pos = core::PartOfSpeech::Adjective;
    new_entry.extended_pos = entry_info.extended_pos;
    new_entry.lemma = lemma;
    result.push_back(new_entry);
  }

  // Contracted forms for ない-ending adjectives (e.g., くだらない → くだらん)
  if (utf8::endsWith(stem, "な")) {
    std::string contracted_stem = std::string(utf8::dropLastChar(stem)) + "ん";
    static const std::vector<std::string> kContractedSuffixes = {
        "",           // くだらん
        "かった",     // くだらんかった
        "ければ",     // くだらんければ
        "かったら",   // くだらんかったら
    };
    for (const auto& suffix : kContractedSuffixes) {
      dictionary::DictionaryEntry new_entry;
      new_entry.surface = contracted_stem + suffix;
      new_entry.pos = core::PartOfSpeech::Adjective;
      new_entry.extended_pos = core::ExtendedPOS::AdjBasic;
      new_entry.lemma = lemma;
      result.push_back(new_entry);
    }
  }

  return result;
}

// Expand verb entry into conjugated forms using Conjugation engine
std::vector<dictionary::DictionaryEntry> expandVerb(
    const dictionary::DictionaryEntry& entry,
    grammar::VerbType verb_type) {
  std::vector<dictionary::DictionaryEntry> result;

  if (verb_type == grammar::VerbType::Unknown) {
    result.push_back(entry);
    return result;
  }

  static grammar::Conjugation conj;
  auto suffixes = conj.getDictionarySuffixes(verb_type);
  std::string stem = grammar::Conjugation::getStem(entry.surface, verb_type);
  std::string lemma = entry.lemma.empty() ? entry.surface : entry.lemma;

  for (const auto& suf : suffixes) {
    dictionary::DictionaryEntry new_entry;
    new_entry.surface = stem + suf.suffix;
    new_entry.pos = core::PartOfSpeech::Verb;
    new_entry.extended_pos = core::ExtendedPOS::VerbShuushikei;
    new_entry.lemma = lemma;
    result.push_back(new_entry);
  }

  return result;
}

}  // namespace

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

  // Deduplication: track surfaces only (trie requires unique keys)
  std::set<std::string> seen_surfaces;

  for (const auto& tsv_entry : entries) {
    // Create base entry
    dictionary::DictionaryEntry base_entry;
    base_entry.surface = tsv_entry.surface;
    base_entry.pos = tsv_entry.pos;
    base_entry.extended_pos = core::ExtendedPOS::Unknown;  // Will be set during expansion
    base_entry.lemma = tsv_entry.surface;

    // Determine if we should expand this entry
    std::vector<dictionary::DictionaryEntry> expanded_entries;

    if (tsv_entry.pos == core::PartOfSpeech::Adjective &&
        tsv_entry.conj_type == dictionary::ConjugationType::IAdjective) {
      // I-adjective: expand to all conjugated forms
      base_entry.extended_pos = core::ExtendedPOS::AdjBasic;
      expanded_entries = expandIAdjective(base_entry);
    } else if (tsv_entry.pos == core::PartOfSpeech::Verb) {
      // Verb: detect type and expand
      grammar::VerbType verb_type = grammar::VerbType::Unknown;

      // Use conj_type hint if available, otherwise detect
      switch (tsv_entry.conj_type) {
        case dictionary::ConjugationType::Ichidan:
          verb_type = grammar::VerbType::Ichidan;
          break;
        case dictionary::ConjugationType::GodanKa:
          verb_type = grammar::VerbType::GodanKa;
          break;
        case dictionary::ConjugationType::GodanGa:
          verb_type = grammar::VerbType::GodanGa;
          break;
        case dictionary::ConjugationType::GodanSa:
          verb_type = grammar::VerbType::GodanSa;
          break;
        case dictionary::ConjugationType::GodanTa:
          verb_type = grammar::VerbType::GodanTa;
          break;
        case dictionary::ConjugationType::GodanNa:
          verb_type = grammar::VerbType::GodanNa;
          break;
        case dictionary::ConjugationType::GodanBa:
          verb_type = grammar::VerbType::GodanBa;
          break;
        case dictionary::ConjugationType::GodanMa:
          verb_type = grammar::VerbType::GodanMa;
          break;
        case dictionary::ConjugationType::GodanRa:
          verb_type = grammar::VerbType::GodanRa;
          break;
        case dictionary::ConjugationType::GodanWa:
          verb_type = grammar::VerbType::GodanWa;
          break;
        case dictionary::ConjugationType::Suru:
          verb_type = grammar::VerbType::Suru;
          break;
        case dictionary::ConjugationType::Kuru:
          verb_type = grammar::VerbType::Kuru;
          break;
        default:
          // Auto-detect if not specified
          verb_type = grammar::Conjugation::detectType(tsv_entry.surface);
          break;
      }

      base_entry.extended_pos = core::ExtendedPOS::VerbShuushikei;
      expanded_entries = expandVerb(base_entry, verb_type);
    } else {
      // No expansion needed
      expanded_entries.push_back(base_entry);
    }

    // Add expanded entries with deduplication (by surface only)
    for (const auto& exp_entry : expanded_entries) {
      if (seen_surfaces.count(exp_entry.surface) > 0) {
        ++duplicates_skipped;
        continue;
      }
      seen_surfaces.insert(exp_entry.surface);

      writer.addEntry(exp_entry);
      ++entries_compiled_;
      if (expanded_entries.size() > 1) {
        ++conj_expanded_;
      }
    }
  }

  if (verbose_ && conj_expanded_ > 0) {
    printInfo("Expanded " + std::to_string(conj_expanded_) +
              " conjugated forms");
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
