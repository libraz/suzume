#include "analysis/scorer_options_loader.h"

#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#ifndef __EMSCRIPTEN__
#include <fstream>
#include <iostream>
#include <sstream>
#endif
#include <string>
#include <string_view>
#include <type_traits>

#ifndef __EMSCRIPTEN__
#ifdef _WIN32
extern char** _environ;
#else
extern char** environ;
#endif
#endif

namespace suzume::analysis {

namespace {

// Every tunable option is a float inside ScorerOptions, so one byte offset locates
// any of them. Addressing options that way lets a single reader serve all sections
// instead of instantiating the same lookup-and-assign code per option struct.
static_assert(std::is_standard_layout_v<ScorerOptions>, "scorer options are addressed by byte offset");

struct OptionSpec {
  std::string_view name;
  size_t offset;  // relative to the section struct, not to ScorerOptions
};

// The public option name and the member it writes are one definition; stringizing
// the member keeps a renamed field from silently keeping its old JSON name.
#define SUZUME_OPTION(Section, field) \
  OptionSpec {                        \
    #field, offsetof(Section, field)  \
  }

constexpr std::array<OptionSpec, 3> kJoinOptionSpecs{{
    SUZUME_OPTION(JoinOptions, compound_verb_bonus),
    SUZUME_OPTION(JoinOptions, verified_v1_bonus),
    SUZUME_OPTION(JoinOptions, verified_noun_bonus),
}};

constexpr std::array<OptionSpec, 10> kSplitOptionSpecs{{
    SUZUME_OPTION(SplitOptions, alpha_kanji_bonus),
    SUZUME_OPTION(SplitOptions, alpha_katakana_bonus),
    SUZUME_OPTION(SplitOptions, digit_kanji_1_bonus),
    SUZUME_OPTION(SplitOptions, digit_kanji_2_bonus),
    SUZUME_OPTION(SplitOptions, duration_period_bonus),
    SUZUME_OPTION(SplitOptions, digit_kanji_3_penalty),
    SUZUME_OPTION(SplitOptions, dict_split_bonus),
    SUZUME_OPTION(SplitOptions, split_base_cost),
    SUZUME_OPTION(SplitOptions, noun_verb_split_bonus),
    SUZUME_OPTION(SplitOptions, verified_verb_bonus),
}};

constexpr std::array<OptionSpec, 8> kUnaryOptionSpecs{{
    SUZUME_OPTION(ScorerOptions, noun_prior),
    SUZUME_OPTION(ScorerOptions, verb_prior),
    SUZUME_OPTION(ScorerOptions, adj_prior),
    SUZUME_OPTION(ScorerOptions, adv_prior),
    SUZUME_OPTION(ScorerOptions, particle_prior),
    SUZUME_OPTION(ScorerOptions, aux_prior),
    SUZUME_OPTION(ScorerOptions, pronoun_prior),
    SUZUME_OPTION(ScorerOptions, user_dict_bonus),
}};

constexpr std::array<OptionSpec, 21> kVerbOptionSpecs{{
    SUZUME_OPTION(VerbCandidateOptions, confidence_low),
    SUZUME_OPTION(VerbCandidateOptions, confidence_standard),
    SUZUME_OPTION(VerbCandidateOptions, confidence_past_te),
    SUZUME_OPTION(VerbCandidateOptions, confidence_ichidan_dict),
    SUZUME_OPTION(VerbCandidateOptions, confidence_short_godan_base),
    SUZUME_OPTION(VerbCandidateOptions, confidence_dict_verb),
    SUZUME_OPTION(VerbCandidateOptions, confidence_katakana),
    SUZUME_OPTION(VerbCandidateOptions, confidence_high),
    SUZUME_OPTION(VerbCandidateOptions, confidence_very_high),
    SUZUME_OPTION(VerbCandidateOptions, base_cost_standard),
    SUZUME_OPTION(VerbCandidateOptions, base_cost_high),
    SUZUME_OPTION(VerbCandidateOptions, base_cost_low),
    SUZUME_OPTION(VerbCandidateOptions, base_cost_verified),
    SUZUME_OPTION(VerbCandidateOptions, base_cost_long_verified),
    SUZUME_OPTION(VerbCandidateOptions, bonus_ichidan),
    SUZUME_OPTION(VerbCandidateOptions, bonus_long_dict),
    SUZUME_OPTION(VerbCandidateOptions, bonus_long_verified),
    SUZUME_OPTION(VerbCandidateOptions, penalty_single_char),
    SUZUME_OPTION(VerbCandidateOptions, confidence_cost_scale),
    SUZUME_OPTION(VerbCandidateOptions, confidence_cost_scale_small),
    SUZUME_OPTION(VerbCandidateOptions, confidence_cost_scale_medium),
}};

constexpr std::array<OptionSpec, 28> kInflectionOptionSpecs{{
    SUZUME_OPTION(grammar::InflectionScorerOptions, base_confidence),
    SUZUME_OPTION(grammar::InflectionScorerOptions, confidence_floor),
    SUZUME_OPTION(grammar::InflectionScorerOptions, confidence_ceiling),
    SUZUME_OPTION(grammar::InflectionScorerOptions, penalty_stem_very_long),
    SUZUME_OPTION(grammar::InflectionScorerOptions, penalty_stem_long),
    SUZUME_OPTION(grammar::InflectionScorerOptions, bonus_stem_two_char),
    SUZUME_OPTION(grammar::InflectionScorerOptions, bonus_aux_length_per_byte),
    SUZUME_OPTION(grammar::InflectionScorerOptions, penalty_ichidan_potential_ambiguity),
    SUZUME_OPTION(grammar::InflectionScorerOptions, bonus_ichidan_e_row),
    SUZUME_OPTION(grammar::InflectionScorerOptions, penalty_ichidan_looks_godan),
    SUZUME_OPTION(grammar::InflectionScorerOptions, penalty_ichidan_kanji_i),
    SUZUME_OPTION(grammar::InflectionScorerOptions, penalty_ichidan_kanji_hiragana_stem),
    SUZUME_OPTION(grammar::InflectionScorerOptions, penalty_ichidan_irregular_stem),
    SUZUME_OPTION(grammar::InflectionScorerOptions, penalty_i_adj_single_kanji),
    SUZUME_OPTION(grammar::InflectionScorerOptions, penalty_i_adj_verb_aux_pattern),
    SUZUME_OPTION(grammar::InflectionScorerOptions, bonus_i_adj_compound_yasui_nikui),
    SUZUME_OPTION(grammar::InflectionScorerOptions, penalty_i_adj_e_row_stem),
    SUZUME_OPTION(grammar::InflectionScorerOptions, penalty_i_adj_ru_stem_invalid),
    SUZUME_OPTION(grammar::InflectionScorerOptions, penalty_i_adj_verb_rashii_pattern),
    SUZUME_OPTION(grammar::InflectionScorerOptions, bonus_suru_two_kanji),
    SUZUME_OPTION(grammar::InflectionScorerOptions, penalty_godan_sa_two_kanji),
    SUZUME_OPTION(grammar::InflectionScorerOptions, bonus_godan_sa_single_kanji),
    SUZUME_OPTION(grammar::InflectionScorerOptions, penalty_suru_single_kanji),
    SUZUME_OPTION(grammar::InflectionScorerOptions, penalty_ichidan_single_hiragana_particle),
    SUZUME_OPTION(grammar::InflectionScorerOptions, penalty_pure_hiragana_stem),
    SUZUME_OPTION(grammar::InflectionScorerOptions, penalty_godan_single_hiragana_stem),
    SUZUME_OPTION(grammar::InflectionScorerOptions, penalty_godan_non_ra_pure_hiragana),
    SUZUME_OPTION(grammar::InflectionScorerOptions, penalty_godan_te_stem),
}};

#undef SUZUME_OPTION

// Where each section's struct starts inside ScorerOptions. The unary options are
// members of ScorerOptions itself, so their section starts at the root.
constexpr size_t kUnaryBase = 0;
constexpr size_t kJoinBase = offsetof(ScorerOptions, candidates) + offsetof(CandidateOptions, join);
constexpr size_t kSplitBase = offsetof(ScorerOptions, candidates) + offsetof(CandidateOptions, split);
constexpr size_t kVerbBase = offsetof(ScorerOptions, candidates) + offsetof(CandidateOptions, verb);
constexpr size_t kInflectionBase = offsetof(ScorerOptions, inflection);

struct SectionSpec {
  std::string_view key;   // key inside its parent object
  std::string_view path;  // qualified name used in diagnostics
  size_t base;
  const OptionSpec* options;
  size_t option_count;
};

constexpr std::array<SectionSpec, 2> kCandidateSections{{
    {"join", "candidates.join", kJoinBase, kJoinOptionSpecs.data(), kJoinOptionSpecs.size()},
    {"split", "candidates.split", kSplitBase, kSplitOptionSpecs.data(), kSplitOptionSpecs.size()},
}};

constexpr std::array<SectionSpec, 3> kRootOptionSections{{
    {"unary", "unary", kUnaryBase, kUnaryOptionSpecs.data(), kUnaryOptionSpecs.size()},
    {"verb_candidates", "verb_candidates", kVerbBase, kVerbOptionSpecs.data(), kVerbOptionSpecs.size()},
    {"inflection", "inflection", kInflectionBase, kInflectionOptionSpecs.data(), kInflectionOptionSpecs.size()},
}};

float& optionAt(ScorerOptions& options, size_t offset) {
  return *reinterpret_cast<float*>(reinterpret_cast<std::byte*>(&options) + offset);
}

const SectionSpec* findSection(const SectionSpec* sections, size_t count, std::string_view key) {
  for (size_t index = 0; index < count; ++index) {
    if (sections[index].key == key) {
      return &sections[index];
    }
  }
  return nullptr;
}

const OptionSpec* findOption(const SectionSpec& section, std::string_view name) {
  for (size_t index = 0; index < section.option_count; ++index) {
    if (section.options[index].name == name) {
      return &section.options[index];
    }
  }
  return nullptr;
}

// Powers of ten that are exactly representable in double; 1e22 is the largest.
constexpr int kMaxExactPowerOfTen = 22;
constexpr std::array<double, kMaxExactPowerOfTen + 1> kPowersOfTen{{
    1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
    1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22,
}};

// A 19-digit significand is the most that fits a 64-bit integer exactly.
constexpr int kMaxSignificandDigits = 19;
// Beyond these decimal exponents no 19-digit significand can land inside the
// normal float range, so the value is rejected without scaling it digit by digit.
constexpr int kOverflowExponent = 39;
constexpr int kUnderflowExponent = -60;

// Convert a scanned JSON number token to float.
//
// std::strtof would do this, but the libc scanner it calls computes in 128-bit
// long double, which drags the software floating-point helpers (__addtf3,
// __multf3, __divtf3, fmodl and the scanner internals) into the WASM binary —
// about 14 KB of code for one configuration knob. The caller has already limited
// the token to [-]digits[.digits][(e|E)[+-]digits], so a direct conversion
// covers the whole accepted grammar.
//
// Rounding: the significand is accumulated exactly and scaled by exact powers of
// ten, so the double intermediate carries a single rounding at 2^-53 before it is
// narrowed to float at 2^-24. Magnitudes outside the normal float range are
// rejected, which is what the strtof path did through ERANGE.
bool convertDecimalToFloat(std::string_view text, float& out) {
  size_t pos = 0;
  bool negative = false;
  if (pos < text.size() && (text[pos] == '-' || text[pos] == '+')) {
    negative = text[pos] == '-';
    ++pos;
  }

  uint64_t significand = 0;
  int exponent = 0;
  int digits = 0;
  bool has_digit = false;
  while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
    has_digit = true;
    if (digits < kMaxSignificandDigits) {
      significand = significand * 10 + static_cast<uint64_t>(text[pos] - '0');
      if (significand != 0) {
        ++digits;
      }
    } else {
      // Digits past the significand still scale the value.
      ++exponent;
    }
    ++pos;
  }
  if (pos < text.size() && text[pos] == '.') {
    ++pos;
    while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
      has_digit = true;
      if (digits < kMaxSignificandDigits) {
        significand = significand * 10 + static_cast<uint64_t>(text[pos] - '0');
        if (significand != 0) {
          ++digits;
        }
        --exponent;
      }
      ++pos;
    }
  }
  if (!has_digit) {
    return false;
  }

  if (pos < text.size() && (text[pos] == 'e' || text[pos] == 'E')) {
    ++pos;
    bool exponent_negative = false;
    if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) {
      exponent_negative = text[pos] == '-';
      ++pos;
    }
    if (pos >= text.size() || text[pos] < '0' || text[pos] > '9') {
      return false;
    }
    int written = 0;
    while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
      if (written < 1000) {
        written = written * 10 + (text[pos] - '0');
      }
      ++pos;
    }
    exponent += exponent_negative ? -written : written;
  }
  if (pos != text.size()) {
    return false;
  }

  if (significand == 0) {
    // A zero significand still carries the sign the token spelled out.
    float zero{};
    out = negative ? -zero : zero;
    return true;
  }
  if (exponent > kOverflowExponent || exponent < kUnderflowExponent) {
    return false;
  }

  double value = static_cast<double>(significand);
  while (exponent > kMaxExactPowerOfTen) {
    value *= kPowersOfTen.back();
    exponent -= kMaxExactPowerOfTen;
  }
  while (exponent < -kMaxExactPowerOfTen) {
    value /= kPowersOfTen.back();
    exponent += kMaxExactPowerOfTen;
  }
  if (exponent > 0) {
    value *= kPowersOfTen[static_cast<size_t>(exponent)];
  } else if (exponent < 0) {
    value /= kPowersOfTen[static_cast<size_t>(-exponent)];
  }

  float result = static_cast<float>(negative ? -value : value);
  float magnitude = result < 0 ? -result : result;
  if (magnitude < FLT_MIN || magnitude > FLT_MAX) {
    return false;
  }
  out = result;
  return true;
}

// Single-pass reader for a scorer configuration document.
//
// The accepted document is a fixed two-level shape — section, option name, number —
// so the reader checks names, converts values, and stages them in one traversal
// rather than materializing a generic value tree and walking it again. Values are
// staged on the caller's behalf only after the whole document reads cleanly.
//
// Diagnostics keep the wording the tree-based loader used. A document with several
// independent mistakes now reports the first one in document order instead of the
// order the sections happened to be validated in.
class ConfigReader {
 public:
  ConfigReader(std::string_view json, ScorerOptions& staged, std::string* error_message)
      : json_(json), staged_(staged), error_message_(error_message) {}

  bool read();

 private:
  /**
   * @brief Consume the members of the object starting at the current position
   * @param on_member Called with each member key, having consumed the ':';
   *        it reads that member's value and reports whether it was accepted
   * @note The caller has already established that the current character is '{'.
   */
  template <typename OnMember>
  bool forEachObjectMember(OnMember on_member) {
    ++pos_;  // '{'
    skipWhitespace();
    if (match('}')) {
      return true;
    }
    while (true) {
      std::string key;
      if (!readSectionKeyValue(key)) {
        return false;
      }
      if (!on_member(key)) {
        return false;
      }
      skipWhitespace();
      if (match('}')) {
        return true;
      }
      if (!match(',')) {
        return failParse("Expected ',' or '}' in object");
      }
      skipWhitespace();
    }
  }

  bool readRootObject();
  bool readCandidates();
  bool readOptionSection(const SectionSpec& section);
  bool readBigramSection();
  bool readSectionKeyValue(std::string& key);
  bool readOptionValue(std::string_view path, const std::string& name, float& out);
  bool enterSection(std::string_view path);
  bool readString(std::string& out);
  bool readNumber(float& out);
  bool skipValue();
  void skipDigits();
  void skipWhitespace();
  char peek() const;
  bool match(char expected);
  bool fail(std::string message);
  bool failParse(const char* message);

  std::string_view json_;
  ScorerOptions& staged_;
  std::string* error_message_;
  size_t pos_{0};
};

void ConfigReader::skipWhitespace() {
  while (pos_ < json_.size() &&
         (json_[pos_] == ' ' || json_[pos_] == '\t' || json_[pos_] == '\n' || json_[pos_] == '\r')) {
    ++pos_;
  }
}

char ConfigReader::peek() const {
  return pos_ < json_.size() ? json_[pos_] : '\0';
}

bool ConfigReader::match(char expected) {
  if (peek() == expected) {
    ++pos_;
    return true;
  }
  return false;
}

bool ConfigReader::fail(std::string message) {
  if (error_message_ != nullptr) {
    *error_message_ = std::move(message);
  }
  return false;
}

bool ConfigReader::failParse(const char* message) {
  return fail(std::string("JSON parse error: ") + message);
}

bool ConfigReader::readString(std::string& out) {
  if (!match('"')) {
    return failParse("Expected string");
  }
  while (pos_ < json_.size() && json_[pos_] != '"') {
    if (json_[pos_] == '\\' && pos_ + 1 < json_.size()) {
      ++pos_;
      switch (json_[pos_]) {
        case 'n':
          out += '\n';
          break;
        case 't':
          out += '\t';
          break;
        default:
          out += json_[pos_];
          break;
      }
    } else {
      out += json_[pos_];
    }
    ++pos_;
  }
  if (pos_ >= json_.size()) {
    return failParse("Unterminated string");
  }
  ++pos_;  // closing quote
  return true;
}

void ConfigReader::skipDigits() {
  while (pos_ < json_.size() && json_[pos_] >= '0' && json_[pos_] <= '9') {
    ++pos_;
  }
}

bool ConfigReader::readNumber(float& out) {
  const size_t start = pos_;
  if (peek() == '-') {
    ++pos_;
  }
  skipDigits();
  if (pos_ < json_.size() && json_[pos_] == '.') {
    ++pos_;
    skipDigits();
  }
  if (pos_ < json_.size() && (json_[pos_] == 'e' || json_[pos_] == 'E')) {
    ++pos_;
    if (pos_ < json_.size() && (json_[pos_] == '+' || json_[pos_] == '-')) {
      ++pos_;
    }
    skipDigits();
  }
  if (pos_ == start || !convertDecimalToFloat(json_.substr(start, pos_ - start), out)) {
    return failParse("Invalid number in JSON");
  }
  return true;
}

bool ConfigReader::skipValue() {
  const char lead = peek();
  if (lead == '{') {
    return forEachObjectMember([this](const std::string&) { return skipValue(); });
  }
  if (lead == '[') {
    ++pos_;
    skipWhitespace();
    if (match(']')) {
      return true;
    }
    while (true) {
      if (peek() == '\0') {
        return failParse("Unterminated array");
      }
      if (!skipValue()) {
        return false;
      }
      skipWhitespace();
      if (match(']')) {
        return true;
      }
      if (!match(',')) {
        return failParse("Expected ',' or ']' in array");
      }
      skipWhitespace();
    }
  }
  if (lead == '"') {
    std::string ignored;
    return readString(ignored);
  }
  if (lead == '-' || (lead >= '0' && lead <= '9')) {
    float ignored{};
    return readNumber(ignored);
  }
  if (json_.compare(pos_, 4, "null") == 0 || json_.compare(pos_, 4, "true") == 0) {
    pos_ += 4;
    return true;
  }
  if (json_.compare(pos_, 5, "false") == 0) {
    pos_ += 5;
    return true;
  }
  return failParse("Unexpected character in JSON");
}

bool ConfigReader::enterSection(std::string_view path) {
  if (peek() == '{') {
    return true;
  }
  // The document still has to be well formed before its shape is judged, so a
  // broken value reports the scanner's complaint rather than the type error.
  if (!skipValue()) {
    return false;
  }
  return fail("Scorer section must be an object: " + std::string(path));
}

bool ConfigReader::readSectionKeyValue(std::string& key) {
  if (peek() == '\0') {
    return failParse("Unterminated object");
  }
  if (peek() != '"') {
    return failParse("Expected string key in object");
  }
  if (!readString(key)) {
    return false;
  }
  skipWhitespace();
  if (!match(':')) {
    return failParse("Expected ':' in object");
  }
  skipWhitespace();
  return true;
}

// JSON booleans stand in for numbers, which is how the tree-based loader read
// them: true became 1 and false became 0.
constexpr float kJsonTrue = 1.0F;
constexpr float kJsonFalse = 0.0F;

bool ConfigReader::readOptionValue(std::string_view path, const std::string& name, float& out) {
  const char lead = peek();
  if (lead == '-' || (lead >= '0' && lead <= '9')) {
    return readNumber(out);
  }
  if (json_.compare(pos_, 4, "true") == 0) {
    pos_ += 4;
    out = kJsonTrue;
    return true;
  }
  if (json_.compare(pos_, 5, "false") == 0) {
    pos_ += 5;
    out = kJsonFalse;
    return true;
  }
  if (!skipValue()) {
    return false;
  }
  return fail("Scorer option must be numeric: " + std::string(path) + "." + name);
}

bool ConfigReader::readOptionSection(const SectionSpec& section) {
  if (!enterSection(section.path)) {
    return false;
  }
  return forEachObjectMember([&](const std::string& name) {
    const OptionSpec* option = findOption(section, name);
    if (option == nullptr) {
      return fail("Unknown scorer option: " + std::string(section.path) + "." + name);
    }
    float value{};
    if (!readOptionValue(section.path, name, value)) {
      return false;
    }
    optionAt(staged_, section.base + option->offset) = value;
    return true;
  });
}

bool ConfigReader::readBigramSection() {
  if (!enterSection("bigram")) {
    return false;
  }
  return forEachObjectMember([&](const std::string& name) {
    const BigramOverrideSpec* override_spec = findBigramSpec(name);
    if (override_spec == nullptr) {
      return fail("Unknown scorer option: bigram." + name);
    }
    float value{};
    if (!readOptionValue("bigram", name, value)) {
      return false;
    }
    staged_.bigram.*(override_spec->value) = value;
    return true;
  });
}

bool ConfigReader::readCandidates() {
  if (!enterSection("candidates")) {
    return false;
  }
  return forEachObjectMember([&](const std::string& key) {
    const SectionSpec* section = findSection(kCandidateSections.data(), kCandidateSections.size(), key);
    if (section == nullptr) {
      return fail("Unknown scorer section: candidates." + key);
    }
    return readOptionSection(*section);
  });
}

bool ConfigReader::readRootObject() {
  return forEachObjectMember([&](const std::string& key) {
    if (key == "candidates") {
      return readCandidates();
    }
    if (key == "bigram") {
      return readBigramSection();
    }
    const SectionSpec* section = findSection(kRootOptionSections.data(), kRootOptionSections.size(), key);
    if (section == nullptr) {
      return fail("Unknown scorer section: " + key);
    }
    return readOptionSection(*section);
  });
}

bool ConfigReader::read() {
  skipWhitespace();
  if (peek() != '{') {
    // The scanner ran to completion before the tree-based loader judged the root,
    // so a malformed document still reports the scanner's complaint first.
    if (!skipValue()) {
      return false;
    }
    skipWhitespace();
    if (peek() != '\0') {
      return failParse("Trailing content after JSON value");
    }
    return fail("JSON root must be an object");
  }
  if (!readRootObject()) {
    return false;
  }
  skipWhitespace();
  if (peek() != '\0') {
    return failParse("Trailing content after JSON value");
  }
  return true;
}

}  // namespace

bool ScorerOptionsLoader::loadFromFile(const std::string& path, ScorerOptions& options, std::string* error_msg) {
#ifdef __EMSCRIPTEN__
  (void)path;
  (void)options;
  if (error_msg != nullptr) {
    *error_msg = "File scorer configuration is unavailable in WebAssembly";
  }
  return false;
#else
  std::ifstream file(path);
  if (!file) {
    if (error_msg)
      *error_msg = "Cannot open file: " + path;
    return false;
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  return loadFromJsonString(buffer.str(), options, error_msg);
#endif
}

bool ScorerOptionsLoader::loadFromJsonString(const std::string& json, ScorerOptions& options, std::string* error_msg) {
  // Values are staged on a copy, so a document that fails partway through leaves
  // the caller's options untouched exactly as validating before applying did.
  ScorerOptions staged = options;
  ConfigReader reader(json, staged, error_msg);
  if (!reader.read()) {
    return false;
  }
  options = staged;
  return true;
}

// =============================================================================
// Environment Variable Override Implementation
// =============================================================================

#ifndef __EMSCRIPTEN__

namespace env_override_internal {

// Helper to try parsing float from environment variable
bool tryGetEnvFloat(const char* name, float& out, bool report_warnings,
                    std::vector<std::string>* collected_warnings = nullptr) {
  const char* value = std::getenv(name);
  if (!value)
    return false;

  char* end = nullptr;
  float parsed = std::strtof(value, &end);
  if (end == value || *end != '\0' || !std::isfinite(parsed)) {
    if (report_warnings) {
      const std::string warning = "Invalid value for " + std::string(name) + ": " + value;
      if (collected_warnings != nullptr) {
        collected_warnings->push_back(warning);
      } else {
        std::cerr << "warning: " << warning << "\n";
      }
    }
    return false;
  }
  out = parsed;
  return true;
}

int applySpecs(const char* section, ScorerOptions& options, size_t base, const OptionSpec* specs, size_t count,
               bool report_warnings, std::vector<std::string>* collected_warnings = nullptr) {
  int applied = 0;
  for (size_t index = 0; index < count; ++index) {
    const std::string variable_name = std::string("SUZUME_SCORER_") + section + "_" + std::string(specs[index].name);
    if (tryGetEnvFloat(variable_name.c_str(), optionAt(options, base + specs[index].offset), report_warnings,
                       collected_warnings)) {
      ++applied;
    }
  }
  return applied;
}

// Environment sections in application order; a null spec is the bigram section.
struct EnvSection {
  const char* name;
  const SectionSpec* spec;
};

constexpr std::array<EnvSection, 6> kEnvSections{{
    {"JOIN", &kCandidateSections[0]},
    {"SPLIT", &kCandidateSections[1]},
    {"UNARY", &kRootOptionSections[0]},
    {"BIGRAM", nullptr},
    {"VERB", &kRootOptionSections[1]},
    {"INFL", &kRootOptionSections[2]},
}};

int applyAllEnvOverrides(ScorerOptions& options, bool report_warnings,
                         std::vector<std::string>* collected_warnings = nullptr) {
  int count = 0;
  for (const EnvSection& section : kEnvSections) {
    if (section.spec != nullptr) {
      count += applySpecs(section.name, options, section.spec->base, section.spec->options, section.spec->option_count,
                          report_warnings, collected_warnings);
      continue;
    }
    for (const BigramOverrideSpec& spec : kBigramOverrideSpecs) {
      const std::string variable_name = std::string("SUZUME_SCORER_BIGRAM_") + spec.name;
      if (tryGetEnvFloat(variable_name.c_str(), options.bigram.*(spec.value), report_warnings, collected_warnings)) {
        ++count;
      }
    }
  }
  return count;
}

bool isKnownScorerEnvironmentVariable(std::string_view name) {
  if (name == "SUZUME_SCORER_CONFIG") {
    return true;
  }
  for (const EnvSection& section : kEnvSections) {
    const std::string prefix = std::string("SUZUME_SCORER_") + section.name + "_";
    if (name.substr(0, prefix.size()) != prefix) {
      continue;
    }
    const std::string_view option = name.substr(prefix.size());
    if (section.spec == nullptr) {
      if (findBigramSpec(option) != nullptr) {
        return true;
      }
      continue;
    }
    if (findOption(*section.spec, option) != nullptr) {
      return true;
    }
  }
  return false;
}

void collectUnknownScorerEnvironmentWarnings(std::vector<std::string>& warnings) {
#ifdef _WIN32
  char** environment = ::_environ;
#else
  char** environment = ::environ;
#endif
  constexpr std::string_view kPrefix = "SUZUME_SCORER_";
  for (char** entry = environment; entry != nullptr && *entry != nullptr; ++entry) {
    const std::string_view assignment(*entry);
    const size_t separator = assignment.find('=');
    const std::string_view name = assignment.substr(0, separator);
    if (name.substr(0, kPrefix.size()) == kPrefix && !isKnownScorerEnvironmentVariable(name)) {
      warnings.push_back("Unknown scorer environment variable: " + std::string(name));
    }
  }
}

}  // namespace env_override_internal

int ScorerOptionsLoader::applyEnvOverrides(ScorerOptions& options) {
  return applyEnvOverrides(options, true);
}

int ScorerOptionsLoader::applyEnvOverrides(ScorerOptions& options, bool report_warnings) {
  using namespace env_override_internal;
  return applyAllEnvOverrides(options, report_warnings);
}

ScorerLoadResult ScorerOptionsLoader::loadFromEnv(ScorerOptions& options) {
  return loadFromEnv(options, true);
}

ScorerLoadResult ScorerOptionsLoader::loadFromEnv(ScorerOptions& options, bool report_warnings) {
  ScorerLoadResult result;

  // Check for SUZUME_SCORER_CONFIG environment variable (JSON file path)
  const char* config_path = std::getenv("SUZUME_SCORER_CONFIG");
  if (config_path && config_path[0] != '\0') {
    std::string error_msg;
    if (loadFromFile(config_path, options, &error_msg)) {
      result.config_path = config_path;
    } else {
      if (report_warnings) {
        result.warnings.push_back("Failed to load scorer config from SUZUME_SCORER_CONFIG: " + error_msg);
      }
    }
  }

  // Apply individual environment variable overrides (highest priority)
  result.env_override_count = env_override_internal::applyAllEnvOverrides(options, report_warnings, &result.warnings);
  if (report_warnings) {
    env_override_internal::collectUnknownScorerEnvironmentWarnings(result.warnings);
  }

  return result;
}

#endif  // __EMSCRIPTEN__

}  // namespace suzume::analysis
