#include "cmd_test.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

#include "suzume.h"

namespace suzume::cli {

namespace {

struct TestCase {
  std::string input;
  std::set<std::string> expected_tags;
  size_t line_number{};
};

std::vector<std::string> split(const std::string& str, char delim) {
  std::vector<std::string> result;
  std::stringstream sstr(str);
  std::string item;
  while (std::getline(sstr, item, delim)) {
    if (!item.empty()) {
      result.push_back(item);
    }
  }
  return result;
}

bool runSingleTest(Suzume& analyzer, const std::string& input,
                   const std::set<std::string>& expected, bool verbose) {
  auto tags = analyzer.generateTags(input);
  std::set<std::string> actual(tags.begin(), tags.end());

  bool passed = (actual == expected);

  if (!passed || verbose) {
    if (!passed) {
      std::cout << "FAIL: " << input << "\n";
    } else {
      std::cout << "PASS: " << input << "\n";
    }

    if (verbose || !passed) {
      std::cout << "  Expected: ";
      for (const auto& tag : expected) {
        std::cout << tag << " ";
      }
      std::cout << "\n";

      std::cout << "  Actual:   ";
      for (const auto& tag : actual) {
        std::cout << tag << " ";
      }
      std::cout << "\n";

      // Show diff
      std::vector<std::string> missing;
      std::vector<std::string> extra;

      std::set_difference(expected.begin(), expected.end(), actual.begin(),
                          actual.end(), std::back_inserter(missing));
      std::set_difference(actual.begin(), actual.end(), expected.begin(),
                          expected.end(), std::back_inserter(extra));

      if (!missing.empty()) {
        std::cout << "  Missing:  ";
        for (const auto& tag : missing) {
          std::cout << tag << " ";
        }
        std::cout << "\n";
      }
      if (!extra.empty()) {
        std::cout << "  Extra:    ";
        for (const auto& tag : extra) {
          std::cout << tag << " ";
        }
        std::cout << "\n";
      }
    }
  }

  return passed;
}

int cmdTestSingle(const std::vector<std::string>& args, bool verbose,
                  const std::vector<std::string>& dict_paths) {
  std::string input;
  std::string expect_str;

  // Find --expect
  for (size_t idx = 0; idx < args.size(); ++idx) {
    if (args[idx] == "--expect" && idx + 1 < args.size()) {
      expect_str = args[idx + 1];
    } else if (args[idx].substr(0, 9) == "--expect=") {
      expect_str = args[idx].substr(9);
    } else if (args[idx][0] != '-') {
      input = args[idx];
    }
  }

  if (input.empty()) {
    printError("No input text provided");
    return 1;
  }

  if (expect_str.empty()) {
    printError("No expected tags provided (use --expect)");
    return 1;
  }

  // Parse expected tags
  auto expected_list = split(expect_str, ',');
  std::set<std::string> expected(expected_list.begin(), expected_list.end());

  // Create analyzer
  SuzumeOptions options;
  Suzume analyzer(options);

  for (const auto& path : dict_paths) {
    if (!analyzer.loadUserDictionary(path)) {
      printWarning("Failed to load dictionary: " + path);
    }
  }

  bool passed = runSingleTest(analyzer, input, expected, verbose);
  return passed ? 0 : 1;
}

int cmdTestFile(const std::vector<std::string>& args, bool verbose,
                const std::vector<std::string>& dict_paths) {
  std::string test_file;

  for (size_t idx = 0; idx < args.size(); ++idx) {
    if ((args[idx] == "-f" || args[idx] == "--file") && idx + 1 < args.size()) {
      test_file = args[idx + 1];
      break;
    }
  }

  if (test_file.empty()) {
    printError("No test file provided");
    return 1;
  }

  // Read test file
  std::ifstream file(test_file);
  if (!file) {
    printError("Failed to open test file: " + test_file);
    return 1;
  }

  std::vector<TestCase> tests;
  std::string line;
  size_t line_num = 0;

  while (std::getline(file, line)) {
    ++line_num;

    // Skip empty lines and comments
    if (line.empty() || line[0] == '#') {
      continue;
    }

    // Parse: input<TAB>expected_tags (comma-separated)
    size_t tab = line.find('\t');
    if (tab == std::string::npos) {
      printWarning("Invalid test line " + std::to_string(line_num) +
                   ": missing tab");
      continue;
    }

    TestCase test;
    test.input = line.substr(0, tab);
    test.line_number = line_num;

    auto expected_list = split(line.substr(tab + 1), ',');
    test.expected_tags = std::set<std::string>(expected_list.begin(),
                                                expected_list.end());

    tests.push_back(std::move(test));
  }

  // Create analyzer
  SuzumeOptions options;
  Suzume analyzer(options);

  for (const auto& path : dict_paths) {
    if (!analyzer.loadUserDictionary(path)) {
      printWarning("Failed to load dictionary: " + path);
    }
  }

  // Run tests
  size_t passed = 0;
  size_t failed = 0;

  for (const auto& test : tests) {
    if (runSingleTest(analyzer, test.input, test.expected_tags, verbose)) {
      ++passed;
    } else {
      ++failed;
    }
  }

  std::cout << "\n";
  std::cout << "Results: " << passed << " passed, " << failed << " failed, "
            << tests.size() << " total\n";

  return failed > 0 ? 1 : 0;
}

int cmdTestBenchmark(const std::vector<std::string>& args,
                     bool /* verbose */,
                     const std::vector<std::string>& dict_paths) {
  size_t iterations = 1000;
  std::string corpus_file;

  for (size_t idx = 0; idx < args.size(); ++idx) {
    if (args[idx].substr(0, 13) == "--iterations=") {
      iterations = std::stoul(args[idx].substr(13));
    } else if ((args[idx] == "-f" || args[idx] == "--file") &&
               idx + 1 < args.size()) {
      corpus_file = args[idx + 1];
    }
  }

  // Create analyzer
  SuzumeOptions options;
  Suzume analyzer(options);

  for (const auto& path : dict_paths) {
    if (!analyzer.loadUserDictionary(path)) {
      printWarning("Failed to load dictionary: " + path);
    }
  }

  // Load test data
  std::vector<std::string> texts;
  if (!corpus_file.empty()) {
    std::ifstream file(corpus_file);
    if (!file) {
      printError("Failed to open corpus file: " + corpus_file);
      return 1;
    }
    std::string line;
    while (std::getline(file, line)) {
      if (!line.empty()) {
        texts.push_back(line);
      }
    }
  } else {
    // Default test texts
    texts = {
        "Tokyo",
        "Hello world",
        "This is a test.",
    };
  }

  if (texts.empty()) {
    printError("No test texts available");
    return 1;
  }

  // Count total characters
  size_t total_chars = 0;
  for (const auto& text : texts) {
    total_chars += text.size();
  }

  std::cout << "Benchmark: " << iterations << " iterations, " << texts.size()
            << " texts, " << total_chars << " chars total\n";

  // Warmup
  for (const auto& text : texts) {
    analyzer.analyze(text);
  }

  // Benchmark
  auto start = std::chrono::high_resolution_clock::now();

  for (size_t iter = 0; iter < iterations; ++iter) {
    for (const auto& text : texts) {
      auto morphemes = analyzer.analyze(text);
      // Prevent optimization
      if (morphemes.empty() && !text.empty()) {
        std::cerr << "Unexpected empty result\n";
      }
    }
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  auto ms_total = static_cast<double>(duration.count());
  double chars_per_sec =
      (static_cast<double>(total_chars) * iterations) / (ms_total / 1000.0);

  std::cout << "Time: " << ms_total << " ms\n";
  std::cout << "Throughput: " << static_cast<size_t>(chars_per_sec)
            << " chars/sec\n";
  std::cout << "Per text: " << (ms_total / (iterations * texts.size()))
            << " ms avg\n";

  return 0;
}

}  // namespace

int cmdTest(const CommandArgs& args) {
  if (args.help) {
    printTestHelp();
    return 0;
  }

  if (args.args.empty()) {
    printTestHelp();
    return 1;
  }

  const std::string& subcommand = args.args[0];
  std::vector<std::string> subargs(args.args.begin() + 1, args.args.end());

  if (subcommand == "benchmark") {
    return cmdTestBenchmark(subargs, args.verbose, args.dict_paths);
  }

  // Check for -f flag (file test)
  bool has_file_flag = false;
  for (const auto& arg : args.args) {
    if (arg == "-f" || arg == "--file") {
      has_file_flag = true;
      break;
    }
  }

  if (has_file_flag) {
    return cmdTestFile(args.args, args.verbose, args.dict_paths);
  }

  // Single test with --expect
  return cmdTestSingle(args.args, args.verbose, args.dict_paths);
}

}  // namespace suzume::cli
