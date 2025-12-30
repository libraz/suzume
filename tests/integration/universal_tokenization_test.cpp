// Universal tokenization test that auto-discovers all JSON test files.
// No need to create separate C++ files for each JSON - just add JSON files
// to tests/data/tokenization/ and they will be automatically picked up.

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "json_loader.h"
#include "suzume.h"
#include "test_case.h"

namespace suzume::test {
namespace {

namespace fs = std::filesystem;

// Test fixture for universal tokenization tests
class UniversalTokenizationTest : public ::testing::Test {
 protected:
  void verifyMorphemes(const std::string& input,
                       const std::vector<core::Morpheme>& result,
                       const std::vector<ExpectedMorpheme>& expected) {
    SCOPED_TRACE("Input: " + input);

    ASSERT_EQ(result.size(), expected.size())
        << "Morpheme count mismatch for: " << input;

    for (size_t i = 0; i < expected.size(); ++i) {
      SCOPED_TRACE("Morpheme index: " + std::to_string(i));

      EXPECT_EQ(result[i].surface, expected[i].surface)
          << "Surface mismatch at index " << i;

      if (!expected[i].pos.empty()) {
        EXPECT_EQ(result[i].pos, expected[i].posEnum())
            << "POS mismatch at index " << i << " for surface '"
            << result[i].surface << "'";
      }

      if (!expected[i].lemma.empty()) {
        EXPECT_EQ(result[i].lemma, expected[i].lemma)
            << "Lemma mismatch at index " << i << " for surface '"
            << result[i].surface << "'";
      }
    }
  }

  Suzume analyzer_;
};

// Convert filename to valid test suite name
std::string fileToSuiteName(const std::string& filename) {
  std::string name = filename;
  // Remove .json extension
  if (name.size() > 5 && name.substr(name.size() - 5) == ".json") {
    name = name.substr(0, name.size() - 5);
  }
  // Replace invalid characters
  for (char& c : name) {
    if (!std::isalnum(c) && c != '_') {
      c = '_';
    }
  }
  // Capitalize first letter for CamelCase suite name
  if (!name.empty() && std::islower(name[0])) {
    name[0] = static_cast<char>(std::toupper(name[0]));
  }
  return name;
}

// Sanitize test case ID to valid GoogleTest name
std::string sanitizeTestName(const std::string& id) {
  std::string name = id;
  for (char& c : name) {
    if (!std::isalnum(c) && c != '_') {
      c = '_';
    }
  }
  return name;
}

// Discover all JSON files in the tokenization test data directory
std::vector<fs::path> discoverJsonFiles() {
  std::vector<fs::path> files;
  const fs::path test_data_dir = "tests/data/tokenization";

  if (!fs::exists(test_data_dir)) {
    return files;
  }

  for (const auto& entry : fs::directory_iterator(test_data_dir)) {
    if (entry.is_regular_file() && entry.path().extension() == ".json") {
      files.push_back(entry.path());
    }
  }

  // Sort for consistent test ordering
  std::sort(files.begin(), files.end());
  return files;
}

// Store test data globally for parameterized tests
struct TestDataEntry {
  std::string suite_name;
  std::string json_path;
  TestCase test_case;

  // For GoogleTest to print readable test parameter info
  friend void PrintTo(const TestDataEntry& entry, std::ostream* os) {
    *os << entry.suite_name << "/" << entry.test_case.id;
  }
};

std::vector<TestDataEntry>& getTestData() {
  static std::vector<TestDataEntry> data;
  return data;
}

// Load all test data from discovered JSON files
void loadAllTestData() {
  auto& data = getTestData();
  if (!data.empty()) {
    return;  // Already loaded
  }

  auto json_files = discoverJsonFiles();
  for (const auto& json_path : json_files) {
    std::string suite_name = fileToSuiteName(json_path.filename().string());
    try {
      auto suite = JsonLoader::loadFromFile(json_path.string());
      for (auto& tc : suite.cases) {
        data.push_back({suite_name, json_path.string(), std::move(tc)});
      }
    } catch (const std::exception& e) {
      // Skip files that fail to load
      std::cerr << "Warning: Failed to load " << json_path << ": " << e.what()
                << std::endl;
    }
  }
}

// Parameterized test class
class UniversalTokenizationParamTest
    : public UniversalTokenizationTest,
      public ::testing::WithParamInterface<TestDataEntry> {};

TEST_P(UniversalTokenizationParamTest, Tokenize) {
  const auto& entry = GetParam();
  auto result = analyzer_.analyze(entry.test_case.input);
  verifyMorphemes(entry.test_case.input, result, entry.test_case.expected);
}

// Custom name generator that includes suite name for disambiguation
struct UniversalTestNameGenerator {
  std::string operator()(
      const ::testing::TestParamInfo<TestDataEntry>& info) const {
    return info.param.suite_name + "_" +
           sanitizeTestName(info.param.test_case.id);
  }
};

// Initialize test data before tests run
struct TestDataInitializer {
  TestDataInitializer() { loadAllTestData(); }
};

// Global initializer - runs before INSTANTIATE_TEST_SUITE_P
static TestDataInitializer g_initializer;

INSTANTIATE_TEST_SUITE_P(AutoDiscovered, UniversalTokenizationParamTest,
                         ::testing::ValuesIn(getTestData()),
                         UniversalTestNameGenerator());

}  // namespace
}  // namespace suzume::test
