#include "dict_compiler.h"

#include <iostream>
#include <set>
#include <tuple>

#include "cli_common.h"
#include "core/utf8_constants.h"
#include "dictionary/binary_dict.h"
#include "grammar/conjugation.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"

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
    // MeCab compatibility: かった/くなかった are NOT stored as single tokens
    // They should be split: かった → かっ + た, くなかった → く + なかっ + た
    {"かっ", core::ExtendedPOS::AdjKatt},           // Ta-connection (連用タ接続): 美しかっ+た
    {"くない", core::ExtendedPOS::AdjBasic},        // Negative: 美しくない
    {"くなかっ", core::ExtendedPOS::AdjBasic},      // Negative past before た: 美しくなかっ+た
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

  // Skip irregular adjective いい - its conjugations (よく, よければ, etc.) are
  // already in L1 dictionary as よい forms. Expanding いい would incorrectly
  // generate いく instead of よく.
  if (entry.surface == "いい") {
    result.push_back(entry);  // Just add the base form
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
  // DISABLED: MeCab splits くだらん as くだら(verb未然形)+ん(助動詞)
  // Keeping contracted forms in dictionary prevents MeCab-compatible splitting
  // if (utf8::endsWith(stem, "な")) {
  //   std::string contracted_stem = std::string(utf8::dropLastChar(stem)) + "ん";
  //   static const std::vector<std::string> kContractedSuffixes = {
  //       "",           // くだらん
  //       "かった",     // くだらんかった
  //       "ければ",     // くだらんければ
  //       "かったら",   // くだらんかったら
  //   };
  //   for (const auto& suffix : kContractedSuffixes) {
  //     dictionary::DictionaryEntry new_entry;
  //     new_entry.surface = contracted_stem + suffix;
  //     new_entry.pos = core::PartOfSpeech::Adjective;
  //     new_entry.extended_pos = core::ExtendedPOS::AdjBasic;
  //     new_entry.lemma = lemma;
  //     result.push_back(new_entry);
  //   }
  // }

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
    new_entry.extended_pos = suf.extended_pos;  // Use ExtendedPOS from suffix
    new_entry.lemma = lemma;
    result.push_back(new_entry);
  }

  return result;
}

}  // namespace

bool isTrivialEntry(std::string_view surface) {
  using normalize::CharType;

  // Entries containing spaces are always non-trivial (multi-word)
  if (surface.find(' ') != std::string_view::npos) {
    return false;
  }

  auto codepoints = normalize::utf8::decode(surface);

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

  // All meaningful characters are the same type (or none found): trivial
  return true;
}

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
    if (verbose_ && trivial_count > 0) {
      printInfo("Filtered " + std::to_string(trivial_count) +
                " trivial entries (kept " +
                std::to_string(filtered_entries.size()) + ")");
    }
    active_entries = &filtered_entries;

    if (active_entries->empty()) {
      return core::makeUnexpected(core::Error(
          core::ErrorCode::InvalidInput,
          "No entries remaining after trivial filtering"));
    }
  }

  const auto& work_entries = *active_entries;

  dictionary::BinaryDictWriter writer;
  entries_compiled_ = 0;
  conj_expanded_ = 0;
  size_t duplicates_skipped = 0;

  // Deduplication: track surfaces (trie requires unique keys)
  // When duplicate surfaces arise from different verb base forms (e.g., 降り from
  // both 降る and 降りる), prefer the entry with the longer lemma (more specific).
  // Only replace same-POS entries to avoid VERB overriding NOUN (e.g., 晴れ).
  struct SeenEntry { size_t lemma_len; core::PartOfSpeech pos; };
  std::unordered_map<std::string, SeenEntry> seen_surfaces;

  // Two-pass processing:
  // Pass 1: Process non-conjugating entries (nouns, adverbs, etc.) first
  //         This ensures noun entries like 思い take priority over verb renyokei
  // Pass 2: Process conjugating entries (verbs, adjectives) with expansion

  // Helper lambda to check if entry needs conjugation expansion
  auto needsExpansion = [](const TsvEntry& entry) {
    return (entry.pos == core::PartOfSpeech::Adjective &&
            entry.conj_type == dictionary::ConjugationType::IAdjective) ||
           entry.pos == core::PartOfSpeech::Verb;
  };

  // Pass 1: Non-conjugating entries
  for (const auto& tsv_entry : work_entries) {
    if (needsExpansion(tsv_entry)) {
      continue;  // Skip conjugating entries in pass 1
    }

    dictionary::DictionaryEntry base_entry;
    base_entry.surface = tsv_entry.surface;
    base_entry.pos = tsv_entry.pos;
    base_entry.lemma = tsv_entry.surface;

    if (tsv_entry.conj_type == dictionary::ConjugationType::Interjection) {
      base_entry.extended_pos = core::ExtendedPOS::Interjection;
    } else if (tsv_entry.conj_type == dictionary::ConjugationType::ProperFamily) {
      base_entry.extended_pos = core::ExtendedPOS::NounProperFamily;
    } else if (tsv_entry.conj_type == dictionary::ConjugationType::ProperGiven) {
      base_entry.extended_pos = core::ExtendedPOS::NounProperGiven;
    } else {
      base_entry.extended_pos = core::ExtendedPOS::Unknown;
    }

    if (seen_surfaces.count(base_entry.surface) > 0) {
      ++duplicates_skipped;
      continue;
    }
    seen_surfaces.emplace(base_entry.surface, SeenEntry{base_entry.lemma.size(), base_entry.pos});
    writer.addEntry(base_entry);
    ++entries_compiled_;
  }

  // Pass 2: Conjugating entries (verbs and i-adjectives with expansion)
  for (const auto& tsv_entry : work_entries) {
    if (!needsExpansion(tsv_entry)) {
      continue;  // Skip non-conjugating entries in pass 2
    }

    // Create base entry
    dictionary::DictionaryEntry base_entry;
    base_entry.surface = tsv_entry.surface;
    base_entry.pos = tsv_entry.pos;
    base_entry.extended_pos = core::ExtendedPOS::Unknown;
    base_entry.lemma = tsv_entry.surface;

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
    }

    // Add expanded entries with deduplication (by surface)
    // When duplicate surfaces exist and both are VERB, prefer the longer lemma
    // (e.g., 降りる > 降る for surface 降り — ichidan is more specific)
    // Don't replace non-VERB entries (e.g., 晴れ NOUN should not be replaced by 晴れる VERB)
    for (const auto& exp_entry : expanded_entries) {
      auto it = seen_surfaces.find(exp_entry.surface);
      if (it != seen_surfaces.end()) {
        if (exp_entry.pos == it->second.pos &&
            exp_entry.lemma.size() > it->second.lemma_len) {
          // Replace with more specific entry (longer lemma, same POS)
          it->second = SeenEntry{exp_entry.lemma.size(), exp_entry.pos};
          writer.replaceEntry(exp_entry);
        }
        ++duplicates_skipped;
        continue;
      }
      seen_surfaces.emplace(exp_entry.surface, SeenEntry{exp_entry.lemma.size(), exp_entry.pos});

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
