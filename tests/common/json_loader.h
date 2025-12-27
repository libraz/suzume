// Minimal JSON loader for test case files.

#pragma once

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "test_case.h"

namespace suzume::test {

// Simple JSON parser for test cases
class JsonLoader {
 public:
  // Load test suite from file
  static TestSuite loadFromFile(const std::string& path);

  // Load test suite from string
  static TestSuite loadFromString(const std::string& json);

 private:
  explicit JsonLoader(const std::string& json) : json_(json), pos_(0) {}

  TestSuite parse();
  TestCase parseTestCase();
  ExpectedMorpheme parseMorpheme();
  std::vector<std::string> parseStringArray();
  std::string parseString();
  void skipWhitespace();
  char peek();
  char consume();
  void expect(char c);
  void expectKey(const std::string& key);
  bool tryKey(const std::string& key);

  std::string json_;
  size_t pos_;
};

// Inline implementations for header-only usage

inline TestSuite JsonLoader::loadFromFile(const std::string& path) {
  std::ifstream file(path);
  if (!file) {
    throw std::runtime_error("Cannot open file: " + path);
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  return loadFromString(buffer.str());
}

inline TestSuite JsonLoader::loadFromString(const std::string& json) {
  JsonLoader loader(json);
  return loader.parse();
}

inline TestSuite JsonLoader::parse() {
  TestSuite suite;
  skipWhitespace();
  expect('{');

  while (peek() != '}') {
    skipWhitespace();
    if (tryKey("version")) {
      expect(':');
      suite.version = parseString();
    } else if (tryKey("cases")) {
      expect(':');
      skipWhitespace();
      expect('[');
      skipWhitespace();
      while (peek() != ']') {
        suite.cases.push_back(parseTestCase());
        skipWhitespace();
        if (peek() == ',') consume();
        skipWhitespace();
      }
      expect(']');
    } else {
      // Skip unknown key
      parseString();
      expect(':');
      // Skip value (simplified: just skip until comma or closing brace)
      int depth = 0;
      while (pos_ < json_.size()) {
        char c = peek();
        if (c == '{' || c == '[') depth++;
        else if (c == '}' || c == ']') {
          if (depth == 0) break;
          depth--;
        } else if (c == ',' && depth == 0) break;
        else if (c == '"') parseString();
        else consume();
      }
    }
    skipWhitespace();
    if (peek() == ',') consume();
    skipWhitespace();
  }
  expect('}');
  return suite;
}

inline TestCase JsonLoader::parseTestCase() {
  TestCase tc;
  skipWhitespace();
  expect('{');

  while (peek() != '}') {
    skipWhitespace();
    if (tryKey("id")) {
      expect(':');
      tc.id = parseString();
    } else if (tryKey("input")) {
      expect(':');
      tc.input = parseString();
    } else if (tryKey("description")) {
      expect(':');
      tc.description = parseString();
    } else if (tryKey("tags")) {
      expect(':');
      tc.tags = parseStringArray();
    } else if (tryKey("expected")) {
      expect(':');
      skipWhitespace();
      expect('[');
      skipWhitespace();
      while (peek() != ']') {
        tc.expected.push_back(parseMorpheme());
        skipWhitespace();
        if (peek() == ',') consume();
        skipWhitespace();
      }
      expect(']');
    } else {
      // Skip unknown key-value pair
      parseString();
      expect(':');
      if (peek() == '"') parseString();
      else if (peek() == '[') {
        int depth = 1;
        consume();
        while (depth > 0 && pos_ < json_.size()) {
          if (peek() == '[') depth++;
          else if (peek() == ']') depth--;
          consume();
        }
      }
    }
    skipWhitespace();
    if (peek() == ',') consume();
    skipWhitespace();
  }
  expect('}');
  return tc;
}

inline ExpectedMorpheme JsonLoader::parseMorpheme() {
  ExpectedMorpheme mor;
  skipWhitespace();
  expect('{');

  while (peek() != '}') {
    skipWhitespace();
    if (tryKey("surface")) {
      expect(':');
      mor.surface = parseString();
    } else if (tryKey("pos")) {
      expect(':');
      mor.pos = parseString();
    } else if (tryKey("lemma")) {
      expect(':');
      mor.lemma = parseString();
    } else {
      // Skip unknown key-value
      parseString();
      expect(':');
      parseString();
    }
    skipWhitespace();
    if (peek() == ',') consume();
    skipWhitespace();
  }
  expect('}');
  return mor;
}

inline std::vector<std::string> JsonLoader::parseStringArray() {
  std::vector<std::string> result;
  skipWhitespace();
  expect('[');
  skipWhitespace();
  while (peek() != ']') {
    result.push_back(parseString());
    skipWhitespace();
    if (peek() == ',') consume();
    skipWhitespace();
  }
  expect(']');
  return result;
}

inline std::string JsonLoader::parseString() {
  skipWhitespace();
  expect('"');
  std::string result;
  while (pos_ < json_.size() && json_[pos_] != '"') {
    if (json_[pos_] == '\\' && pos_ + 1 < json_.size()) {
      pos_++;
      switch (json_[pos_]) {
        case 'n': result += '\n'; break;
        case 't': result += '\t'; break;
        case 'r': result += '\r'; break;
        case '"': result += '"'; break;
        case '\\': result += '\\'; break;
        case 'u': {
          // Unicode escape \uXXXX
          if (pos_ + 4 < json_.size()) {
            std::string hex = json_.substr(pos_ + 1, 4);
            uint32_t codepoint = std::stoul(hex, nullptr, 16);
            // Convert to UTF-8
            if (codepoint < 0x80) {
              result += static_cast<char>(codepoint);
            } else if (codepoint < 0x800) {
              result += static_cast<char>(0xC0 | (codepoint >> 6));
              result += static_cast<char>(0x80 | (codepoint & 0x3F));
            } else {
              result += static_cast<char>(0xE0 | (codepoint >> 12));
              result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
              result += static_cast<char>(0x80 | (codepoint & 0x3F));
            }
            pos_ += 4;
          }
          break;
        }
        default: result += json_[pos_]; break;
      }
    } else {
      result += json_[pos_];
    }
    pos_++;
  }
  expect('"');
  return result;
}

inline void JsonLoader::skipWhitespace() {
  while (pos_ < json_.size() &&
         (json_[pos_] == ' ' || json_[pos_] == '\t' ||
          json_[pos_] == '\n' || json_[pos_] == '\r')) {
    pos_++;
  }
}

inline char JsonLoader::peek() {
  if (pos_ >= json_.size()) {
    throw std::runtime_error("Unexpected end of JSON");
  }
  return json_[pos_];
}

inline char JsonLoader::consume() {
  return json_[pos_++];
}

inline void JsonLoader::expect(char c) {
  skipWhitespace();
  if (peek() != c) {
    throw std::runtime_error(
        std::string("Expected '") + c + "' but got '" + peek() +
        "' at position " + std::to_string(pos_));
  }
  consume();
}

inline void JsonLoader::expectKey(const std::string& key) {
  if (!tryKey(key)) {
    throw std::runtime_error("Expected key: " + key);
  }
}

inline bool JsonLoader::tryKey(const std::string& key) {
  size_t saved_pos = pos_;
  skipWhitespace();
  if (peek() != '"') {
    pos_ = saved_pos;
    return false;
  }
  std::string parsed = parseString();
  if (parsed != key) {
    pos_ = saved_pos;
    return false;
  }
  return true;
}

}  // namespace suzume::test
