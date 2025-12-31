#include "cmd_analyze.h"

#include <cstdlib>
#include <iostream>
#include <sstream>

#include "grammar/conjugation.h"
#include "normalize/utf8.h"
#include "suzume.h"

namespace suzume::cli {

namespace {

// Convert hiragana to katakana
std::string hiraganaToKatakana(std::string_view hiragana) {
  if (hiragana.empty()) {
    return "";
  }

  std::vector<char32_t> codepoints = normalize::utf8::decode(hiragana);

  for (char32_t& cp : codepoints) {
    // Hiragana range: U+3041-U+3096
    // Katakana range: U+30A1-U+30F6
    // Offset: 0x60 (96)
    if (cp >= 0x3041 && cp <= 0x3096) {
      cp += 0x60;
    }
  }

  return normalize::utf8::encode(codepoints);
}

void outputMorpheme(const std::vector<core::Morpheme>& morphemes) {
  for (const auto& morpheme : morphemes) {
    std::cout << morpheme.surface << "\t" << core::posToString(morpheme.pos)
              << "\t" << morpheme.lemma << "\n";
  }
}

void outputTags(const std::vector<std::string>& tags) {
  for (const auto& tag : tags) {
    std::cout << tag << "\n";
  }
}

void outputJson(const std::string& input,
                const std::vector<core::Morpheme>& morphemes) {
  std::cout << "{\n";
  std::cout << R"(  "input": ")" << input << "\",\n";
  std::cout << "  \"morphemes\": [\n";

  for (size_t idx = 0; idx < morphemes.size(); ++idx) {
    const auto& mor = morphemes[idx];
    std::cout << "    {";
    std::cout << R"("surface": ")" << mor.surface << "\", ";
    std::cout << R"("pos": ")" << core::posToString(mor.pos) << "\", ";
    std::cout << R"("lemma": ")" << mor.lemma << "\"";
    std::cout << "}";
    if (idx + 1 < morphemes.size()) {
      std::cout << ",";
    }
    std::cout << "\n";
  }

  std::cout << "  ]\n";
  std::cout << "}\n";
}

void outputTsv(const std::vector<core::Morpheme>& morphemes) {
  for (const auto& mor : morphemes) {
    std::cout << mor.surface << "\t" << core::posToString(mor.pos) << "\t"
              << mor.lemma << "\t" << mor.start_pos << "\t" << mor.end_pos
              << "\n";
  }
}

void outputChasen(const std::vector<core::Morpheme>& morphemes) {
  for (const auto& mor : morphemes) {
    // Surface form
    std::cout << mor.surface << "\t";

    // Reading (in katakana)
    if (!mor.reading.empty()) {
      std::cout << hiraganaToKatakana(mor.reading) << "\t";
    } else {
      std::cout << "*\t";
    }

    // Lemma (base form)
    std::cout << mor.getLemma() << "\t";

    // Part of speech (Japanese)
    std::cout << core::posToJapanese(mor.pos) << "\t";

    // Conjugation type and form (for verbs and adjectives)
    if (mor.pos == core::PartOfSpeech::Verb ||
        mor.pos == core::PartOfSpeech::Adjective) {
      auto verb_type = grammar::conjTypeToVerbType(mor.conj_type);
      std::cout << grammar::verbTypeToJapanese(verb_type) << "\t";
      std::cout << grammar::conjFormToJapanese(mor.conj_form);
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
    // Read from stdin
    std::ostringstream oss;
    std::string line;
    while (std::getline(std::cin, line)) {
      if (!oss.str().empty()) {
        oss << "\n";
      }
      oss << line;
    }
    text = oss.str();
  }

  if (text.empty()) {
    printError("No input text provided");
    printAnalyzeHelp();
    return 1;
  }

  // Create analyzer
  SuzumeOptions options;
  options.mode = parseMode(args.mode);
  // Default is preserve (true), flags invert to normalize
  options.normalize_options.preserve_vu = !args.normalize_vu;
  options.normalize_options.preserve_case = !args.lowercase;
  // Default is remove symbols (true), flag inverts to preserve
  options.remove_symbols = !args.preserve_symbols;

  Suzume analyzer(options);

  // Load dictionaries
  for (const auto& dict_path : args.dict_paths) {
    if (!analyzer.loadUserDictionary(dict_path)) {
      printWarning("Failed to load dictionary: " + dict_path);
    } else if (args.verbose) {
      printInfo("Loaded dictionary: " + dict_path);
    }
  }

  // Compare mode
  if (args.compare && !args.dict_paths.empty()) {
    // Analyze without user dictionary
    Suzume base_analyzer(options);
    auto base_morphemes = base_analyzer.analyze(text);

    std::cout << "[Without user dictionary]\n";
    outputMorpheme(base_morphemes);
    std::cout << "\n";

    // Analyze with user dictionary
    auto morphemes = analyzer.analyze(text);

    std::cout << "[With user dictionary]\n";
    outputMorpheme(morphemes);
    std::cout << "\n";

    // Show diff (simplified)
    std::cout << "[Diff]\n";
    if (base_morphemes.size() != morphemes.size()) {
      std::cout << "Morpheme count: " << base_morphemes.size() << " -> "
                << morphemes.size() << "\n";
    } else {
      std::cout << "No structural difference\n";
    }

    return 0;
  }

  // Debug mode - show lattice candidates
  if (args.debug) {
    // Enable all debug output when --debug is used
    // Note: setenv is POSIX, may need alternative on Windows
#ifdef _WIN32
    _putenv_s("SUZUME_DEBUG", "1");
#else
    setenv("SUZUME_DEBUG", "1", 1);
#endif

    std::cout << "=== Debug Mode ===\n";
    std::cout << "Input: \"" << text << "\"\n\n";

    core::Lattice lattice(0);
    auto morphemes = analyzer.analyzeDebug(text, &lattice);

    std::cout << "\n=== Lattice Candidates ===\n";
    for (size_t pos = 0; pos < lattice.textLength(); ++pos) {
      const auto& edges = lattice.edgesAt(pos);
      if (!edges.empty()) {
        std::cout << "Position " << pos << ":\n";
        for (const auto& edge : edges) {
          std::cout << "  [" << edge.start << "-" << edge.end << "] "
                    << edge.surface << " (" << core::posToString(edge.pos)
                    << ") cost=" << edge.cost;
          if (!edge.lemma.empty()) {
            std::cout << " lemma=" << edge.lemma;
          }
          // Show source info
          if (edge.fromDictionary()) {
            std::cout << " [dict";
            if (edge.fromUserDict()) {
              std::cout << ":user";
            }
            std::cout << "]";
          }
          if (edge.isUnknown()) {
            std::cout << " [unk]";
          }
          std::cout << " id=" << edge.id;
          std::cout << "\n";
        }
      }
    }

    std::cout << "\n=== Result ===\n";
    outputMorpheme(morphemes);
    return 0;
  }

  // Normal analysis
  switch (args.format) {
    case OutputFormat::Morpheme: {
      auto morphemes = analyzer.analyze(text);
      outputMorpheme(morphemes);
      break;
    }
    case OutputFormat::Tags: {
      auto tags = analyzer.generateTags(text);
      outputTags(tags);
      break;
    }
    case OutputFormat::Json: {
      auto morphemes = analyzer.analyze(text);
      outputJson(text, morphemes);
      break;
    }
    case OutputFormat::Tsv: {
      auto morphemes = analyzer.analyze(text);
      outputTsv(morphemes);
      break;
    }
    case OutputFormat::Chasen: {
      auto morphemes = analyzer.analyze(text);
      outputChasen(morphemes);
      break;
    }
  }

  return 0;
}

}  // namespace suzume::cli
