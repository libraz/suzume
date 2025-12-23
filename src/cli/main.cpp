#include <cstring>
#include <iostream>
#include <string>

#include "suzume.h"

void printUsage(const char* program) {
  std::cerr << "Usage: " << program << " [options] <text>\n"
            << "Options:\n"
            << "  -a, --analyze    Output morpheme analysis (default)\n"
            << "  -t, --tags       Output tags only\n"
            << "  -d, --dict PATH  Load user dictionary\n"
            << "  -m, --mode MODE  Analysis mode (normal, search, split)\n"
            << "  -v, --version    Show version\n"
            << "  -h, --help       Show this help\n";
}

int main(int argc, char* argv[]) {
  bool show_tags = false;
  std::string dict_path;
  std::string mode_str = "normal";
  std::string text;

  // Parse arguments
  for (int idx = 1; idx < argc; ++idx) {
    std::string arg = argv[idx];

    if (arg == "-h" || arg == "--help") {
      printUsage(argv[0]);
      return 0;
    }

    if (arg == "-v" || arg == "--version") {
      std::cout << "suzume " << suzume::Suzume::version() << "\n";
      return 0;
    }

    if (arg == "-a" || arg == "--analyze") {
      show_tags = false;
      continue;
    }

    if (arg == "-t" || arg == "--tags") {
      show_tags = true;
      continue;
    }

    if ((arg == "-d" || arg == "--dict") && idx + 1 < argc) {
      dict_path = argv[++idx];
      continue;
    }

    if ((arg == "-m" || arg == "--mode") && idx + 1 < argc) {
      mode_str = argv[++idx];
      continue;
    }

    // Remaining argument is text
    if (arg[0] != '-') {
      text = arg;
    }
  }

  // Read from stdin if no text argument
  if (text.empty()) {
    std::getline(std::cin, text);
  }

  if (text.empty()) {
    std::cerr << "Error: No input text\n";
    printUsage(argv[0]);
    return 1;
  }

  // Create Suzume instance
  suzume::SuzumeOptions options;

  // Set mode
  if (mode_str == "search") {
    options.mode = suzume::core::AnalysisMode::Search;
  } else if (mode_str == "split") {
    options.mode = suzume::core::AnalysisMode::Split;
  } else {
    options.mode = suzume::core::AnalysisMode::Normal;
  }

  suzume::Suzume analyzer(options);

  // Load user dictionary if specified
  if (!dict_path.empty()) {
    if (!analyzer.loadUserDictionary(dict_path)) {
      std::cerr << "Warning: Failed to load dictionary: " << dict_path << "\n";
    }
  }

  if (show_tags) {
    // Output tags
    auto tags = analyzer.generateTags(text);
    for (const auto& tag : tags) {
      std::cout << tag << "\n";
    }
  } else {
    // Output morpheme analysis
    auto morphemes = analyzer.analyze(text);
    for (const auto& morpheme : morphemes) {
      std::cout << morpheme.surface << "\t"
                << suzume::core::posToString(morpheme.pos) << "\t"
                << morpheme.lemma << "\n";
    }
  }

  return 0;
}