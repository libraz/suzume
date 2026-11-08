#include "cmd_analyze.h"

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

#include "grammar/conjugation.h"
#include "normalize/utf8.h"
#include "suzume.h"
#include "suzume/suzume_c.h"

namespace suzume::cli {

namespace {

void outputMorphemes(std::ostream& output, const std::vector<core::Morpheme>& morphemes) {
  for (const auto& morpheme : morphemes) {
    output << tabEscape(morpheme.surface) << "\t" << core::posToString(morpheme.pos) << "\t"
           << tabEscape(morpheme.lemma) << "\t" << morpheme.start << "\t" << morpheme.end << "\n";
  }
}

void outputTags(const std::vector<postprocess::TagEntry>& tags) {
  for (const auto& tag : tags) {
    std::cout << tabEscape(tag.tag) << "\t" << core::posToString(tag.pos) << "\n";
  }
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
    std::cout << R"("lemma": ")" << jsonEscape(mor.lemma) << "\", ";
    std::cout << R"("start": )" << mor.start << ", ";
    std::cout << R"("end": )" << mor.end << ", ";
    std::cout << R"("extended_pos": ")" << core::extendedPosToString(mor.extended_pos) << "\", ";
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

    // Lemma (base form)
    std::cout << tabEscape(mor.getLemma()) << "\t";

    // Part of speech (Japanese)
    std::cout << core::posToJapanese(mor.pos) << "\t";

    // Conjugation type and form (for verbs, adjectives, and auxiliaries).
    if (mor.pos == core::PartOfSpeech::Verb || mor.pos == core::PartOfSpeech::Adjective ||
        mor.pos == core::PartOfSpeech::Auxiliary) {
      const char* canonical_type = suzume_conjugation_type_label(static_cast<suzume_conjugation_type_t>(mor.conj_type));
      const std::string conjugation_type = canonical_type != nullptr ? canonical_type : "";
      const std::string conjugation_form(grammar::conjFormToJapanese(mor.conj_form));
      std::cout << (conjugation_type.empty() ? "*" : conjugation_type) << "\t";
      std::cout << (conjugation_form.empty() ? "*" : conjugation_form);
    } else {
      std::cout << "*\t*";
    }
    std::cout << "\n";
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
         << tabEscape(morpheme.lemma) << "\t" << morpheme.start << "\t" << morpheme.end << "\n";
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
  } else if (!isTerminal()) {
    // Read stdin byte-for-byte so embedded newlines are preserved.
    std::ostringstream oss;
    oss << std::cin.rdbuf();
    if (std::cin.bad()) {
      printError("Failed to read input");
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
    printError("Invalid UTF-8 input");
    return 1;
  }

  if (text.empty()) {
    printError("No input text provided");
    printAnalyzeHelp(std::cerr);
    return 1;
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
  // Default is preserve (true), flags invert to normalize
  options.normalize_options.preserve_vu = !args.normalize_vu;
  options.normalize_options.preserve_case = !args.lowercase;
  // Default is remove symbols (true), flag inverts to preserve
  options.remove_symbols = !args.preserve_symbols;
  options.skip_user_dictionary = args.no_user_dict;
  options.skip_core_dictionary = args.no_core_dict;
  options.skip_env_config = args.skip_env_config;
  options.lemmatize = !args.no_lemmatize;
  options.merge_compounds = args.merge_compounds;
  options.report_scorer_config = args.verbose;

  Suzume analyzer(options);
  for (const auto& warning : analyzer.dictionaryWarnings()) {
    printWarning(warning);
  }

  // Source and binary dictionaries share the same additive user layer.
  for (const auto& dict_path : args.dict_paths) {
    auto load_result = loadUserDictionaryPath(&analyzer, dict_path);
    if (!load_result.hasValue()) {
      printError("Failed to load dictionary: " + dict_path + ": " + load_result.error().message);
      return 1;
    } else if (args.verbose) {
      printInfo("Loaded dictionary: " + dict_path);
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
      printError("Analysis failed: " + base_analysis.error().message);
      return 1;
    }
    auto base_morphemes = std::move(base_analysis.value().morphemes);

    std::cerr << "[Without user dictionary]\n";
    outputMorphemes(std::cerr, base_morphemes);
    std::cerr << "\n";

    // Analyze with user dictionary
    auto analysis = analyzer.analyzeWithNormalizedTextResult(text);
    if (!analysis.hasValue()) {
      printError("Analysis failed: " + analysis.error().message);
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
      printError("Analysis failed: " + analysis.error().message);
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
          if (edge.fromDictionary()) {
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
    tag_options.exclude_particles = !args.tag_include_particles;
    tag_options.exclude_auxiliaries = !args.tag_include_auxiliaries;
    tag_options.exclude_formal_nouns = !args.tag_include_formal_nouns;
    tag_options.exclude_low_info = !args.tag_include_low_info;
    tag_options.remove_duplicates = !args.tag_keep_duplicates;
    tag_options.use_lemma = !args.tag_use_surface;
    tag_options.min_tag_length = args.tag_min_length;
    tag_options.max_tags = args.tag_max_tags;
    tag_options.pos_filter = args.tag_pos_filter;
    tag_options.exclude_basic = args.tag_exclude_basic;
    outputTags(analyzer.generateTags(text, tag_options));
    return 0;
  }

  auto analysis = analyzer.analyzeWithNormalizedTextResult(text);
  if (!analysis.hasValue()) {
    printError("Analysis failed: " + analysis.error().message);
    return 1;
  }
  const auto& output = analysis.value();
  switch (args.format) {
    case OutputFormat::Morpheme:
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
