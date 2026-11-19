#include "cmd_test.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <vector>

#if !defined(__EMSCRIPTEN__) && !defined(_WIN32)
#include <sys/resource.h>
#endif

#include "suzume.h"

namespace suzume::cli {

namespace {

struct TestCase {
  std::string input;
  std::set<std::string> expected_tags;
  size_t line_number{};
};

SuzumeOptions analyzerOptions(const CommandArgs& args) {
  SuzumeOptions options;
  if (args.mode == "search") {
    options.mode = core::AnalysisMode::Search;
  } else if (args.mode == "split") {
    options.mode = core::AnalysisMode::Split;
  }
  options.normalize_options.preserve_vu = args.preserve_vu;
  options.normalize_options.preserve_case = args.preserve_case;
  options.remove_symbols = !args.preserve_symbols;
  options.skip_user_dictionary = args.skip_bundled_user_dictionary;
  options.skip_core_dictionary = args.skip_core_dictionary;
  options.skip_env_config = args.skip_env_config;
  options.lemmatize = args.lemmatize;
  options.merge_compounds = args.merge_compounds;
  options.report_scorer_config = args.verbose;
  return options;
}

postprocess::TagGeneratorOptions tagOptions(const CommandArgs& args) {
  postprocess::TagGeneratorOptions options;
  options.exclude_particles = args.tag_exclude_particles;
  options.exclude_auxiliaries = args.tag_exclude_auxiliaries;
  options.exclude_formal_nouns = args.tag_exclude_formal_nouns;
  options.exclude_low_info = args.tag_exclude_low_info;
  options.remove_duplicates = args.tag_remove_duplicates;
  options.use_lemma = args.tag_use_base_form;
  options.min_tag_length = args.tag_min_length;
  options.max_tags = args.tag_max_tags;
  options.pos_filter = args.tag_pos_filter;
  options.exclude_basic = args.tag_exclude_basic;
  return options;
}

void stripTrailingCarriageReturn(std::string* line) {
  if (line != nullptr && !line->empty() && line->back() == '\r') {
    line->pop_back();
  }
}

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

double medianMilliseconds(std::vector<double> samples) {
  std::sort(samples.begin(), samples.end());
  const size_t middle = samples.size() / 2;
  if (samples.size() % 2 != 0) {
    return samples[middle];
  }
  return (samples[middle - 1] + samples[middle]) / 2.0;
}

size_t peakResidentSetBytes() {
#if defined(__EMSCRIPTEN__) || defined(_WIN32)
  return 0;
#else
  rusage usage{};
  if (getrusage(RUSAGE_SELF, &usage) != 0 || usage.ru_maxrss < 0) {
    return 0;
  }
#ifdef __APPLE__
  return static_cast<size_t>(usage.ru_maxrss);
#else
  return static_cast<size_t>(usage.ru_maxrss) * 1024;
#endif
#endif
}

bool runSingleTest(Suzume& analyzer, const std::string& input, const std::set<std::string>& expected,
                   const postprocess::TagGeneratorOptions& tag_options, bool verbose) {
  auto tags = analyzer.generateTags(input, tag_options);
  std::set<std::string> actual;
  for (const auto& t : tags) {
    actual.insert(t.tag);
  }

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

      std::set_difference(expected.begin(), expected.end(), actual.begin(), actual.end(), std::back_inserter(missing));
      std::set_difference(actual.begin(), actual.end(), expected.begin(), expected.end(), std::back_inserter(extra));

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

int cmdTestSingle(const std::vector<std::string>& args, const CommandArgs& command_args) {
  std::string input;
  std::string expect_str;

  // Find --expect
  for (size_t idx = 0; idx < args.size(); ++idx) {
    if (args[idx] == "--expect") {
      if (idx + 1 >= args.size()) {
        printError("missing value for --expect");
        return 1;
      }
      expect_str = args[idx + 1];
      ++idx;
    } else if (args[idx].substr(0, 9) == "--expect=") {
      expect_str = args[idx].substr(9);
    } else if (!args[idx].empty() && args[idx][0] == '-') {
      printError("unknown test option: " + args[idx]);
      return 1;
    } else {
      input = args[idx];
    }
  }

  if (input.empty()) {
    printError("no input text provided");
    return 1;
  }

  if (expect_str.empty()) {
    printError("no expected tags provided (use --expect)");
    return 1;
  }

  // Parse expected tags
  auto expected_list = split(expect_str, ',');
  std::set<std::string> expected(expected_list.begin(), expected_list.end());

  // Create analyzer
  SuzumeOptions options = analyzerOptions(command_args);
  Suzume analyzer(options);

  for (const auto& path : command_args.dict_paths) {
    auto load_result = loadUserDictionaryPath(&analyzer, path);
    if (!load_result.hasValue()) {
      printError("failed to load dictionary: " + path + ": " + load_result.error().message);
      return 1;
    }
  }

  bool passed = runSingleTest(analyzer, input, expected, tagOptions(command_args), command_args.verbose);
  return passed ? 0 : 1;
}

int cmdTestFile(const std::vector<std::string>& args, const CommandArgs& command_args) {
  std::string test_file;

  for (size_t idx = 0; idx < args.size(); ++idx) {
    if (args[idx] == "--file") {
      if (idx + 1 >= args.size()) {
        printError("missing value for " + args[idx]);
        return 1;
      }
      test_file = args[++idx];
    } else if (args[idx].rfind("--file=", 0) == 0) {
      test_file = args[idx].substr(7);
    } else {
      printError("unknown test file option: " + args[idx]);
      return 1;
    }
  }

  if (test_file.empty()) {
    printError("no test file provided");
    return 1;
  }

  // Read test file
  std::ifstream file(test_file);
  if (!file) {
    printError("failed to open test file: " + test_file);
    return 1;
  }

  std::vector<TestCase> tests;
  std::string line;
  size_t line_num = 0;

  while (std::getline(file, line)) {
    ++line_num;
    stripTrailingCarriageReturn(&line);
    if (line_num == 1) {
      stripUtf8Bom(&line);
    }

    // Skip empty lines and comments
    if (line.empty() || line[0] == '#') {
      continue;
    }

    // Parse: input<TAB>expected_tags (comma-separated)
    size_t tab = line.find('\t');
    if (tab == std::string::npos) {
      printWarning("invalid test line " + std::to_string(line_num) + ": missing tab");
      continue;
    }

    TestCase test;
    test.input = line.substr(0, tab);
    test.line_number = line_num;

    auto expected_list = split(line.substr(tab + 1), ',');
    test.expected_tags = std::set<std::string>(expected_list.begin(), expected_list.end());

    tests.push_back(std::move(test));
  }

  // Create analyzer
  SuzumeOptions options = analyzerOptions(command_args);
  Suzume analyzer(options);

  for (const auto& path : command_args.dict_paths) {
    auto load_result = loadUserDictionaryPath(&analyzer, path);
    if (!load_result.hasValue()) {
      printError("failed to load dictionary: " + path + ": " + load_result.error().message);
      return 1;
    }
  }

  // Run tests
  size_t passed = 0;
  size_t failed = 0;

  for (const auto& test : tests) {
    if (runSingleTest(analyzer, test.input, test.expected_tags, tagOptions(command_args), command_args.verbose)) {
      ++passed;
    } else {
      ++failed;
    }
  }

  std::cout << "\n";
  std::cout << "Results: " << passed << " passed, " << failed << " failed, " << tests.size() << " total\n";

  return failed > 0 ? 1 : 0;
}

int cmdTestBenchmark(const std::vector<std::string>& args, const CommandArgs& command_args) {
  size_t iterations = 1000;
  size_t samples = 5;
  size_t warmup_iterations = 1;
  std::string corpus_file;

  for (size_t idx = 0; idx < args.size(); ++idx) {
    if (args[idx].substr(0, 13) == "--iterations=") {
      if (!parseSizeOption(args[idx].substr(13), &iterations) || iterations == 0) {
        printError("invalid iterations: " + args[idx].substr(13));
        return 1;
      }
    } else if (args[idx].substr(0, 10) == "--samples=") {
      if (!parseSizeOption(args[idx].substr(10), &samples) || samples == 0) {
        printError("invalid samples: " + args[idx].substr(10));
        return 1;
      }
    } else if (args[idx].substr(0, 9) == "--warmup=") {
      if (!parseSizeOption(args[idx].substr(9), &warmup_iterations)) {
        printError("invalid warmup: " + args[idx].substr(9));
        return 1;
      }
    } else if (args[idx] == "--file" && idx + 1 < args.size()) {
      corpus_file = args[++idx];
    } else if (args[idx].rfind("--file=", 0) == 0) {
      corpus_file = args[idx].substr(7);
    } else {
      printError("unknown benchmark option: " + args[idx]);
      return 1;
    }
  }

  // Load test data
  std::vector<std::string> texts;
  if (!corpus_file.empty()) {
    std::ifstream file(corpus_file);
    if (!file) {
      printError("failed to open corpus file: " + corpus_file);
      return 1;
    }
    std::string line;
    while (std::getline(file, line)) {
      stripTrailingCarriageReturn(&line);
      if (!line.empty()) {
        texts.push_back(line);
      }
    }
  } else {
    // Default test texts
    texts = {
        "東京でテストを行う。",
        "りんごを食べる。",
        "１２３ＡＢＣを確認する。",
    };
  }

  if (texts.empty()) {
    printError("no test texts available");
    return 1;
  }

  // Count total characters
  size_t total_chars = 0;
  for (const auto& text : texts) {
    total_chars += text.size();
  }

  std::cout << "Benchmark: " << iterations << " steady iterations, " << samples << " samples, " << warmup_iterations
            << " warmup iterations, " << texts.size() << " texts, " << total_chars << " bytes per corpus pass\n";

  std::vector<double> initialization_ms;
  std::vector<double> first_analysis_ms;
  std::vector<double> steady_ms;
  size_t analyzed_morphemes = 0;
  size_t steady_morphemes_per_sample = 0;

  for (size_t sample = 0; sample < samples; ++sample) {
    SuzumeOptions options = analyzerOptions(command_args);
    const auto initialization_start = std::chrono::steady_clock::now();
    Suzume analyzer(options);
    for (const auto& path : command_args.dict_paths) {
      auto load_result = loadUserDictionaryPath(&analyzer, path);
      if (!load_result.hasValue()) {
        printError("failed to load dictionary: " + path + ": " + load_result.error().message);
        return 1;
      }
    }

    const auto initialization_end = std::chrono::steady_clock::now();
    initialization_ms.push_back(
        std::chrono::duration<double, std::milli>(initialization_end - initialization_start).count());

    const auto first_analysis_start = std::chrono::steady_clock::now();
    analyzed_morphemes += analyzer.analyze(texts.front()).size();
    const auto first_analysis_end = std::chrono::steady_clock::now();
    first_analysis_ms.push_back(
        std::chrono::duration<double, std::milli>(first_analysis_end - first_analysis_start).count());

    for (size_t warmup = 0; warmup < warmup_iterations; ++warmup) {
      for (const auto& text : texts) {
        analyzed_morphemes += analyzer.analyze(text).size();
      }
    }

    const auto steady_start = std::chrono::steady_clock::now();
    size_t sample_morphemes = 0;
    for (size_t iter = 0; iter < iterations; ++iter) {
      for (const auto& text : texts) {
        const size_t morpheme_count = analyzer.analyze(text).size();
        analyzed_morphemes += morpheme_count;
        sample_morphemes += morpheme_count;
      }
    }
    steady_morphemes_per_sample = sample_morphemes;
    const auto steady_end = std::chrono::steady_clock::now();
    steady_ms.push_back(std::chrono::duration<double, std::milli>(steady_end - steady_start).count());
  }

  const double initialization_median = medianMilliseconds(initialization_ms);
  const double first_analysis_median = medianMilliseconds(first_analysis_ms);
  const double steady_median = medianMilliseconds(steady_ms);
  const double chars_per_second = (static_cast<double>(total_chars) * iterations) / (steady_median / 1000.0);
  const double tokens_per_second = static_cast<double>(steady_morphemes_per_sample) / (steady_median / 1000.0);

  std::cout << "Initialize median: " << initialization_median << " ms\n";
  std::cout << "First analysis median: " << first_analysis_median << " ms\n";
  std::cout << "Steady median: " << steady_median << " ms\n";
  std::cout << "Steady token throughput: " << std::fixed << std::setprecision(3) << tokens_per_second << " tokens/sec\n"
            << std::defaultfloat;
  std::cout << "Steady throughput: " << static_cast<size_t>(chars_per_second) << " bytes/sec\n";
  std::cout << "Steady per text: " << (steady_median / (iterations * texts.size())) << " ms\n";
  std::cout << "Peak RSS: " << peakResidentSetBytes() << " bytes\n";
  if (analyzed_morphemes == 0) {
    printWarning("benchmark inputs produced no morphemes");
  }

  return 0;
}

}  // namespace

int cmdTest(const CommandArgs& args) {
  if (args.help) {
    printTestHelp();
    return 0;
  }

  if (args.args.empty()) {
    printTestHelp(std::cerr);
    return 1;
  }

  const std::string& subcommand = args.args[0];
  std::vector<std::string> subargs(args.args.begin() + 1, args.args.end());

  if (subcommand == "benchmark") {
    return cmdTestBenchmark(subargs, args);
  }
  if (subcommand == "regression" || subcommand == "coverage") {
    printError("test " + subcommand + " is not implemented in this build");
    return 1;
  }

  // Check for --file (file test)
  bool has_file_flag = false;
  for (const auto& arg : args.args) {
    if (arg == "--file" || arg.rfind("--file=", 0) == 0) {
      has_file_flag = true;
      break;
    }
  }

  if (has_file_flag) {
    return cmdTestFile(args.args, args);
  }

  // Single test with --expect
  return cmdTestSingle(args.args, args);
}

}  // namespace suzume::cli
