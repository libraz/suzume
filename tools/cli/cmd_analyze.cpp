#include "cmd_analyze.h"

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

#include "normalize/utf8.h"
#include "suzume.h"
#include "suzume/suzume_c.h"

namespace suzume::cli {

namespace {

void outputMorphemes(std::ostream& output, const std::vector<core::Morpheme>& morphemes) {
  for (const auto& morpheme : morphemes) {
    output << tabEscape(morpheme.surface) << "\t" << core::posToString(morpheme.pos) << "\t"
           << tabEscape(morpheme.getLemma()) << "\t" << morpheme.start << "\t" << morpheme.end << "\n";
  }
}

void outputTags(const std::vector<postprocess::TagEntry>& tags) {
  for (const auto& tag : tags) {
    std::cout << tabEscape(tag.tag) << "\t" << core::posToString(tag.pos) << "\n";
  }
}

/** @brief Conjugation labels of a VERB/ADJECTIVE/AUXILIARY morpheme; empty for every other POS. */
struct ConjugationLabels {
  std::string_view type;
  std::string_view form;
};

ConjugationLabels conjugationLabels(const core::Morpheme& morpheme) {
  if (morpheme.pos != core::PartOfSpeech::Verb && morpheme.pos != core::PartOfSpeech::Adjective &&
      morpheme.pos != core::PartOfSpeech::Auxiliary) {
    return {};
  }
  return {suzume_conjugation_type_label(static_cast<suzume_conjugation_type_t>(morpheme.conj_type)),
          suzume_conjugation_form_label(static_cast<suzume_conjugation_form_t>(morpheme.conj_form))};
}

void outputJsonLabel(std::string_view key, std::string_view label) {
  std::cout << '"' << key << "\": ";
  if (label.empty()) {
    std::cout << "null";
  } else {
    std::cout << '"' << jsonEscape(label) << '"';
  }
  std::cout << ", ";
}

void outputJson(const std::string& input, const std::string& normalized_text,
                const std::vector<core::Morpheme>& morphemes) {
  std::cout << "{\n";
  std::cout << R"(  "input": ")" << jsonEscape(input) << "\",\n";
  std::cout << R"(  "normalized_text": ")" << jsonEscape(normalized_text) << "\",\n";
  std::cout << "  \"morphemes\": [\n";

  for (size_t idx = 0; idx < morphemes.size(); ++idx) {
    const auto& mor = morphemes[idx];
    std::cout << "    {";
    std::cout << R"("surface": ")" << jsonEscape(mor.surface) << "\", ";
    std::cout << R"("pos": ")" << core::posToString(mor.pos) << "\", ";
    std::cout << R"("base_form": ")" << jsonEscape(mor.getLemma()) << "\", ";
    std::cout << R"("pos_ja": ")" << core::posToJapanese(mor.pos) << "\", ";
    const ConjugationLabels labels = conjugationLabels(mor);
    outputJsonLabel("conj_type", labels.type);
    outputJsonLabel("conj_form", labels.form);
    std::cout << R"("extended_pos": ")" << core::extendedPosToString(mor.extended_pos) << "\", ";
    std::cout << R"("start": )" << mor.start << ", ";
    std::cout << R"("end": )" << mor.end << ", ";
    std::cout << R"("is_user_dict": )" << (mor.fromUserDict() ? "true" : "false") << ", ";
    std::cout << R"("is_formal_noun": )" << (mor.isFormalNoun() ? "true" : "false") << ", ";
    std::cout << R"("is_low_info": )" << (mor.isLowInformation() ? "true" : "false") << ", ";
    std::cout << R"("is_unknown": )" << (mor.isUnknown() ? "true" : "false") << ", ";
    std::cout << R"("is_from_dictionary": )" << (mor.fromDictionary() ? "true" : "false") << ", ";
    std::cout << R"("score": )" << std::setprecision(std::numeric_limits<float>::max_digits10) << mor.score;
    std::cout << "}";
    if (idx + 1 < morphemes.size()) {
      std::cout << ",";
    }
    std::cout << "\n";
  }

  std::cout << "  ]\n";
  std::cout << "}\n";
}

void outputChasen(const std::vector<core::Morpheme>& morphemes) {
  for (const auto& mor : morphemes) {
    // Surface form
    std::cout << tabEscape(mor.surface) << "\t";

    // Reading column placeholder (Suzume does not generate readings)
    std::cout << "*\t";

    // Base form
    std::cout << tabEscape(mor.getLemma()) << "\t";

    // Part of speech (Japanese)
    std::cout << core::posToJapanese(mor.pos) << "\t";

    // Conjugation type and form; "*" for a word that does not conjugate.
    const ConjugationLabels labels = conjugationLabels(mor);
    std::cout << (labels.type.empty() ? "*" : labels.type) << "\t";
    std::cout << (labels.form.empty() ? "*" : labels.form) << "\n";
  }
  std::cout << "EOS\n";
}

core::AnalysisMode parseMode(const std::string& mode_str) {
  if (mode_str == "search") {
    return core::AnalysisMode::Search;
  }
  if (mode_str == "split") {
    return core::AnalysisMode::Split;
  }
  return core::AnalysisMode::Normal;
}

bool sameStructure(const core::Morpheme& left, const core::Morpheme& right) {
  return left.surface == right.surface && left.pos == right.pos && left.lemma == right.lemma &&
         left.start == right.start && left.end == right.end;
}

void outputDiffMorpheme(std::ostream& output, std::string_view marker, size_t index, const core::Morpheme& morpheme) {
  output << marker << index << ": " << tabEscape(morpheme.surface) << "\t" << core::posToString(morpheme.pos) << "\t"
         << tabEscape(morpheme.getLemma()) << "\t" << morpheme.start << "\t" << morpheme.end << "\n";
}

}  // namespace

int cmdAnalyze(const CommandArgs& args) {
  if (args.help) {
    printAnalyzeHelp();
    return 0;
  }

  // Get input text
  std::string text;
  if (!args.args.empty()) {
    // Join all positional arguments as text
    std::ostringstream oss;
    for (size_t idx = 0; idx < args.args.size(); ++idx) {
      if (idx > 0) {
        oss << " ";
      }
      oss << args.args[idx];
    }
    text = oss.str();
  } else if (isTerminal()) {
    printError("no input text provided");
    return 2;
  } else {
    // Read stdin byte-for-byte so embedded newlines are preserved.
    std::ostringstream oss;
    oss << std::cin.rdbuf();
    if (std::cin.bad()) {
      printError("failed to read input");
      return 1;
    }
    text = oss.str();
  }
  stripUtf8Bom(&text);
  // Match one std::getline-style EOF boundary: remove one final LF (and its CR
  // in CRLF input), while preserving all preceding newlines.
  if (text.empty() == false && text.back() == '\n') {
    text.pop_back();
    if (text.empty() == false && text.back() == '\r') {
      text.pop_back();
    }
  }

  // Validate once before analysis or any format-specific output. This keeps
  // malformed scalar sequences away from both the tokenizer and JSON output.
  if (!normalize::isValidUtf8(text)) {
    printError("invalid UTF-8 input");
    return 1;
  }

  // An empty operand or an empty pipe is valid input with nothing to report.
  if (text.empty()) {
    return 0;
  }

  if (args.debug) {
    // Debug level is cached on first use, which can happen during dictionary loading.
#ifdef _WIN32
    _putenv_s("SUZUME_DEBUG", "1");
#else
    setenv("SUZUME_DEBUG", "1", 1);
#endif
  }

  // Create analyzer
  SuzumeOptions options;
  options.mode = parseMode(args.mode);
  options.normalize_options.preserve_vu = args.preserve_vu;
  options.normalize_options.preserve_case = args.preserve_case;
  options.remove_symbols = !args.preserve_symbols;
  options.skip_user_dictionary = args.skip_bundled_user_dictionary;
  options.skip_core_dictionary = args.skip_core_dictionary;
  options.skip_env_config = args.skip_env_config;
  options.lemmatize = args.lemmatize;
  options.merge_compounds = args.merge_compounds;
  options.report_scorer_config = args.verbose;

  Suzume analyzer(options);
  // Warnings accumulate across loads; print each one once, right after the load that produced it.
  size_t printed_warnings = 0;
  const auto print_new_warnings = [&analyzer, &printed_warnings]() {
    const auto& warnings = analyzer.dictionaryWarnings();
    for (; printed_warnings < warnings.size(); ++printed_warnings) {
      printWarning(warnings[printed_warnings]);
    }
  };
  print_new_warnings();

  // Source and binary dictionaries share the same additive user layer.
  for (const auto& dict_path : args.dict_paths) {
    auto load_result = loadUserDictionaryPath(&analyzer, dict_path);
    if (!load_result.hasValue()) {
      printError("failed to load dictionary: " + dict_path + ": " + load_result.error().message);
      return 1;
    }
    print_new_warnings();
    if (args.verbose) {
      printInfo("loaded dictionary: " + dict_path);
    }
  }

  // Compare mode
  if (args.compare) {
    // Analyze without user dictionary
    SuzumeOptions base_options = options;
    base_options.skip_user_dictionary = true;
    Suzume base_analyzer(base_options);
    for (const auto& warning : base_analyzer.dictionaryWarnings()) {
      printWarning(warning);
    }
    auto base_analysis = base_analyzer.analyzeWithNormalizedTextResult(text);
    if (!base_analysis.hasValue()) {
      printError("analysis failed: " + base_analysis.error().message);
      return 1;
    }
    auto base_morphemes = std::move(base_analysis.value().morphemes);

    std::cerr << "[Without user dictionary]\n";
    outputMorphemes(std::cerr, base_morphemes);
    std::cerr << "\n";

    // Analyze with user dictionary
    auto analysis = analyzer.analyzeWithNormalizedTextResult(text);
    if (!analysis.hasValue()) {
      printError("analysis failed: " + analysis.error().message);
      return 1;
    }
    auto morphemes = std::move(analysis.value().morphemes);

    std::cerr << "[With user dictionary]\n";
    outputMorphemes(std::cerr, morphemes);
    std::cerr << "\n";

    // Compare the public structural tuple rather than only the token count.
    std::cerr << "[Diff]\n";
    bool has_difference = base_morphemes.size() != morphemes.size();
    const size_t shared_count = std::min(base_morphemes.size(), morphemes.size());
    for (size_t index = 0; index < shared_count; ++index) {
      if (!sameStructure(base_morphemes[index], morphemes[index])) {
        has_difference = true;
        outputDiffMorpheme(std::cerr, "- ", index, base_morphemes[index]);
        outputDiffMorpheme(std::cerr, "+ ", index, morphemes[index]);
      }
    }
    for (size_t index = shared_count; index < base_morphemes.size(); ++index) {
      outputDiffMorpheme(std::cerr, "- ", index, base_morphemes[index]);
    }
    for (size_t index = shared_count; index < morphemes.size(); ++index) {
      outputDiffMorpheme(std::cerr, "+ ", index, morphemes[index]);
    }
    if (!has_difference) {
      std::cerr << "No structural difference\n";
    }
  }

  // Debug mode - show lattice candidates
  if (args.debug) {
    auto analysis = analyzer.analyzeWithNormalizedTextResult(text);
    if (!analysis.hasValue()) {
      printError("analysis failed: " + analysis.error().message);
      return 1;
    }
    std::cerr << "=== Debug Mode ===\n";
    std::cerr << "Input: \"" << text << "\"\n\n";
    std::cerr << "Normalized text: \"" << analysis.value().normalized_text << "\"\n\n";

    core::Lattice lattice(0);
    auto morphemes = analyzer.analyzeDebug(text, &lattice);

    std::cerr << "\n=== Lattice Candidates ===\n";
    for (size_t pos = 0; pos < lattice.textLength(); ++pos) {
      const auto& edge_ids = lattice.edgeIdsAt(pos);
      if (!edge_ids.empty()) {
        std::cerr << "Position " << pos << ":\n";
        for (const uint32_t edge_id : edge_ids) {
          const auto& edge = lattice.getEdge(edge_id);
          std::cerr << "  [" << edge.start << "-" << edge.end << "] " << edge.surface << " ("
                    << core::posToString(edge.pos) << ") cost=" << edge.cost;
          if (!edge.lemma.empty()) {
            std::cerr << " lemma=" << edge.lemma;
          }
          // Show source info
          if (edge.dictionaryBacked()) {
            std::cerr << " [dict";
            if (edge.fromUserDict()) {
              std::cerr << ":user";
            }
            std::cerr << "]";
          }
          if (edge.isUnknown()) {
            std::cerr << " [unk]";
          }
          std::cerr << " id=" << edge.id;
          std::cerr << "\n";
        }
      }
    }

    std::cerr << "\n=== Result ===\n";
    outputMorphemes(std::cerr, morphemes);
  }

  if (args.format == OutputFormat::Tags) {
    postprocess::TagGeneratorOptions tag_options;
    tag_options.exclude_particles = args.tag_exclude_particles;
    tag_options.exclude_auxiliaries = args.tag_exclude_auxiliaries;
    tag_options.exclude_formal_nouns = args.tag_exclude_formal_nouns;
    tag_options.exclude_low_info = args.tag_exclude_low_info;
    tag_options.remove_duplicates = args.tag_remove_duplicates;
    tag_options.use_lemma = args.tag_use_base_form;
    tag_options.min_tag_length = args.tag_min_length;
    tag_options.max_tags = args.tag_max_tags;
    tag_options.pos_filter = args.tag_pos_filter;
    tag_options.exclude_basic = args.tag_exclude_basic;
    outputTags(analyzer.generateTags(text, tag_options));
    return 0;
  }

  auto analysis = analyzer.analyzeWithNormalizedTextResult(text);
  if (!analysis.hasValue()) {
    printError("analysis failed: " + analysis.error().message);
    return 1;
  }
  const auto& output = analysis.value();
  switch (args.format) {
    case OutputFormat::Tsv:
      outputMorphemes(std::cout, output.morphemes);
      break;
    case OutputFormat::Json:
      outputJson(text, output.normalized_text, output.morphemes);
      break;
    case OutputFormat::Chasen:
      outputChasen(output.morphemes);
      break;
    case OutputFormat::Tags:
      break;
  }

  return 0;
}

}  // namespace suzume::cli
