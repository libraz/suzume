#ifndef SUZUME_ANALYSIS_SCORER_OPTIONS_LOADER_H_
#define SUZUME_ANALYSIS_SCORER_OPTIONS_LOADER_H_

// =============================================================================
// Scorer Options Loader
// =============================================================================
// Loads scorer options from JSON configuration files and environment variables.
// Provides partial override capability - only specified fields are updated.
//
// Environment variable format: SUZUME_SCORER_{SECTION}_{KEY}=value
//   SECTION: EDGE, CONN, JOIN, SPLIT
//   KEY: Field name (e.g., penalty_invalid_adj_sou)
//
// Priority: Default < JSON file < Environment variables
// =============================================================================

#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

#ifndef __EMSCRIPTEN__
#include <cstdlib>
#include <iostream>
#endif

#include "candidate_options.h"
#include "connection_rule_options.h"
#include "scorer.h"

namespace suzume::analysis {

/// Simple JSON value representation
struct JsonValue {
  enum class Type { Null, Number, String, Object, Array };
  Type type = Type::Null;
  float number_value = 0.0F;
  std::string string_value;
  std::unordered_map<std::string, JsonValue> object_value;

  bool isNumber() const { return type == Type::Number; }
  bool isObject() const { return type == Type::Object; }

  float asFloat() const { return number_value; }
  const JsonValue* get(const std::string& key) const {
    auto it = object_value.find(key);
    return it != object_value.end() ? &it->second : nullptr;
  }
};

/// Result of loading scorer options from environment
struct ScorerLoadResult {
  std::string config_path;   // Path to JSON config file (if loaded)
  int env_override_count{0}; // Number of individual env overrides applied

  bool hasConfig() const { return !config_path.empty() || env_override_count > 0; }
};

/// Parses JSON and loads scorer options
class ScorerOptionsLoader {
 public:
  /// Load scorer options from file (simple interface)
  /// @param path Path to JSON config file
  /// @param options Output options (modified on success)
  /// @param error_msg Optional error message output (nullptr to ignore)
  /// @return true on success, false on failure
  static bool loadFromFile(const std::string& path, ScorerOptions& options,
                           std::string* error_msg = nullptr);

#ifndef __EMSCRIPTEN__
  /// Apply environment variable overrides to scorer options
  /// Environment variables: SUZUME_SCORER_{SECTION}_{KEY}=value
  /// @param options Options to modify
  /// @return Number of overrides applied
  static int applyEnvOverrides(ScorerOptions& options);

  /// Load scorer options from environment variables
  /// Checks SUZUME_SCORER_CONFIG for JSON file path, then individual overrides
  /// @param options Options to modify
  /// @return Result containing what was loaded
  static ScorerLoadResult loadFromEnv(ScorerOptions& options);
#endif

 private:
  /// Apply edge options from JSON
  static void applyEdgeOptions(EdgeOptions& opts, const JsonValue& json);

  /// Apply connection options from JSON
  static void applyConnectionOptions(ConnectionOptions& opts, const JsonValue& json);

  /// Apply join options from JSON
  static void applyJoinOptions(JoinOptions& opts, const JsonValue& json);

  /// Apply split options from JSON
  static void applySplitOptions(SplitOptions& opts, const JsonValue& json);

  // JSON parser internals (exception-free)
  class Parser {
   public:
    explicit Parser(const std::string& json) : json_(json), pos_(0) {}
    JsonValue parse();

    // Error state accessors
    bool hasError() const { return has_error_; }
    const std::string& errorMessage() const { return error_message_; }

   private:
    JsonValue parseValue();
    JsonValue parseObject();
    JsonValue parseArray();
    JsonValue parseString();
    JsonValue parseNumber();
    void skipWhitespace();
    char peek() const;
    char consume();
    bool match(char c);
    void setError(const char* msg);

    std::string json_;
    size_t pos_{0};
    bool has_error_{false};
    std::string error_message_;
  };
};

// =============================================================================
// Inline Implementations
// =============================================================================

inline JsonValue ScorerOptionsLoader::Parser::parse() {
  skipWhitespace();
  return parseValue();
}

inline void ScorerOptionsLoader::Parser::setError(const char* msg) {
  if (!has_error_) {
    has_error_ = true;
    error_message_ = msg;
  }
}

inline JsonValue ScorerOptionsLoader::Parser::parseValue() {
  if (has_error_) return JsonValue{};
  skipWhitespace();
  char c = peek();
  if (c == '{') return parseObject();
  if (c == '[') return parseArray();
  if (c == '"') return parseString();
  if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
  if (c == 'n') {  // null
    pos_ += 4;
    return JsonValue{};
  }
  if (c == 't') {  // true
    pos_ += 4;
    JsonValue v;
    v.type = JsonValue::Type::Number;
    v.number_value = 1.0F;
    return v;
  }
  if (c == 'f') {  // false
    pos_ += 5;
    JsonValue v;
    v.type = JsonValue::Type::Number;
    v.number_value = 0.0F;
    return v;
  }
  setError("Unexpected character in JSON");
  return JsonValue{};
}

inline JsonValue ScorerOptionsLoader::Parser::parseObject() {
  JsonValue result;
  if (has_error_) return result;
  result.type = JsonValue::Type::Object;
  consume();  // '{'
  skipWhitespace();
  while (!has_error_ && peek() != '}' && peek() != '\0') {
    auto key = parseString();
    if (has_error_) return result;
    skipWhitespace();
    if (!match(':')) {
      setError("Expected ':' in object");
      return result;
    }
    skipWhitespace();
    result.object_value[key.string_value] = parseValue();
    if (has_error_) return result;
    skipWhitespace();
    if (peek() == ',') consume();
    skipWhitespace();
  }
  if (peek() == '}') consume();  // '}'
  return result;
}

inline JsonValue ScorerOptionsLoader::Parser::parseArray() {
  JsonValue result;
  if (has_error_) return result;
  result.type = JsonValue::Type::Array;
  consume();  // '['
  skipWhitespace();
  while (!has_error_ && peek() != ']' && peek() != '\0') {
    parseValue();  // Skip array values for now
    if (has_error_) return result;
    skipWhitespace();
    if (peek() == ',') consume();
    skipWhitespace();
  }
  if (peek() == ']') consume();  // ']'
  return result;
}

inline JsonValue ScorerOptionsLoader::Parser::parseString() {
  JsonValue result;
  if (has_error_) return result;
  result.type = JsonValue::Type::String;
  consume();  // '"'
  while (!has_error_ && pos_ < json_.size() && json_[pos_] != '"') {
    if (json_[pos_] == '\\' && pos_ + 1 < json_.size()) {
      pos_++;
      switch (json_[pos_]) {
        case 'n': result.string_value += '\n'; break;
        case 't': result.string_value += '\t'; break;
        case '"': result.string_value += '"'; break;
        case '\\': result.string_value += '\\'; break;
        default: result.string_value += json_[pos_]; break;
      }
    } else {
      result.string_value += json_[pos_];
    }
    pos_++;
  }
  if (pos_ < json_.size() && json_[pos_] == '"') {
    consume();  // '"'
  }
  return result;
}

inline JsonValue ScorerOptionsLoader::Parser::parseNumber() {
  JsonValue result;
  if (has_error_) return result;
  result.type = JsonValue::Type::Number;
  size_t start = pos_;
  if (peek() == '-') consume();
  while (pos_ < json_.size() && (json_[pos_] >= '0' && json_[pos_] <= '9')) pos_++;
  if (pos_ < json_.size() && json_[pos_] == '.') {
    pos_++;
    while (pos_ < json_.size() && (json_[pos_] >= '0' && json_[pos_] <= '9')) pos_++;
  }
  if (pos_ < json_.size() && (json_[pos_] == 'e' || json_[pos_] == 'E')) {
    pos_++;
    if (pos_ < json_.size() && (json_[pos_] == '+' || json_[pos_] == '-')) pos_++;
    while (pos_ < json_.size() && (json_[pos_] >= '0' && json_[pos_] <= '9')) pos_++;
  }
  if (pos_ > start) {
    result.number_value = std::stof(json_.substr(start, pos_ - start));
  }
  return result;
}

inline void ScorerOptionsLoader::Parser::skipWhitespace() {
  while (pos_ < json_.size() &&
         (json_[pos_] == ' ' || json_[pos_] == '\t' ||
          json_[pos_] == '\n' || json_[pos_] == '\r')) {
    pos_++;
  }
}

inline char ScorerOptionsLoader::Parser::peek() const {
  return pos_ < json_.size() ? json_[pos_] : '\0';
}

inline char ScorerOptionsLoader::Parser::consume() {
  if (pos_ >= json_.size()) {
    setError("Unexpected end of JSON");
    return '\0';
  }
  return json_[pos_++];
}

inline bool ScorerOptionsLoader::Parser::match(char c) {
  if (peek() == c) {
    consume();
    return true;
  }
  return false;
}

// Helper macro to set option from JSON
#define SET_OPT(opts, field, json, key) \
  do { \
    auto* v = json.get(key); \
    if (v && v->isNumber()) opts.field = v->asFloat(); \
  } while (0)

inline void ScorerOptionsLoader::applyEdgeOptions(EdgeOptions& opts, const JsonValue& json) {
  SET_OPT(opts, penalty_invalid_adj_sou, json, "penalty_invalid_adj_sou");
  SET_OPT(opts, penalty_invalid_tai_pattern, json, "penalty_invalid_tai_pattern");
  SET_OPT(opts, penalty_verb_aux_in_adj, json, "penalty_verb_aux_in_adj");
  SET_OPT(opts, penalty_shimai_as_adj, json, "penalty_shimai_as_adj");
  SET_OPT(opts, penalty_verb_onbin_as_adj, json, "penalty_verb_onbin_as_adj");
  SET_OPT(opts, penalty_short_stem_hiragana_adj, json, "penalty_short_stem_hiragana_adj");
  SET_OPT(opts, penalty_verb_tai_rashii, json, "penalty_verb_tai_rashii");
  SET_OPT(opts, penalty_verb_nai_pattern, json, "penalty_verb_nai_pattern");
  SET_OPT(opts, bonus_unified_verb_aux, json, "bonus_unified_verb_aux");
}

inline void ScorerOptionsLoader::applyConnectionOptions(ConnectionOptions& opts, const JsonValue& json) {
  SET_OPT(opts, penalty_copula_after_verb, json, "penalty_copula_after_verb");
  SET_OPT(opts, penalty_ichidan_renyokei_te, json, "penalty_ichidan_renyokei_te");
  SET_OPT(opts, bonus_tai_after_renyokei, json, "bonus_tai_after_renyokei");
  SET_OPT(opts, penalty_yasui_after_renyokei, json, "penalty_yasui_after_renyokei");
  SET_OPT(opts, penalty_nagara_split, json, "penalty_nagara_split");
  SET_OPT(opts, penalty_sou_after_renyokei, json, "penalty_sou_after_renyokei");
  SET_OPT(opts, penalty_te_form_split, json, "penalty_te_form_split");
  SET_OPT(opts, penalty_taku_te_split, json, "penalty_taku_te_split");
  SET_OPT(opts, penalty_takute_after_renyokei, json, "penalty_takute_after_renyokei");
  SET_OPT(opts, bonus_conditional_verb_to_verb, json, "bonus_conditional_verb_to_verb");
  SET_OPT(opts, bonus_verb_renyokei_compound_aux, json, "bonus_verb_renyokei_compound_aux");
  SET_OPT(opts, penalty_toku_contraction_split, json, "penalty_toku_contraction_split");
  SET_OPT(opts, bonus_te_form_verb_to_verb, json, "bonus_te_form_verb_to_verb");
  SET_OPT(opts, bonus_rashii_after_predicate, json, "bonus_rashii_after_predicate");
  SET_OPT(opts, penalty_tai_after_aux, json, "penalty_tai_after_aux");
  SET_OPT(opts, penalty_masen_de_split, json, "penalty_masen_de_split");
  SET_OPT(opts, penalty_invalid_single_char_aux, json, "penalty_invalid_single_char_aux");
  SET_OPT(opts, penalty_te_form_ta_contraction, json, "penalty_te_form_ta_contraction");
  SET_OPT(opts, penalty_noun_mai, json, "penalty_noun_mai");
  SET_OPT(opts, penalty_short_aux_after_particle, json, "penalty_short_aux_after_particle");
  SET_OPT(opts, bonus_noun_mitai, json, "bonus_noun_mitai");
  SET_OPT(opts, bonus_verb_mitai, json, "bonus_verb_mitai");
  SET_OPT(opts, penalty_iru_aux_after_noun, json, "penalty_iru_aux_after_noun");
  SET_OPT(opts, bonus_iru_aux_after_te_form, json, "bonus_iru_aux_after_te_form");
  SET_OPT(opts, bonus_shimau_aux_after_te_form, json, "bonus_shimau_aux_after_te_form");
  SET_OPT(opts, penalty_character_speech_split, json, "penalty_character_speech_split");
  SET_OPT(opts, bonus_adj_ku_naru, json, "bonus_adj_ku_naru");
  SET_OPT(opts, penalty_compound_aux_after_renyokei, json, "penalty_compound_aux_after_renyokei");
  SET_OPT(opts, penalty_yoru_night_after_ni, json, "penalty_yoru_night_after_ni");
  SET_OPT(opts, penalty_formal_noun_before_kanji, json, "penalty_formal_noun_before_kanji");
  SET_OPT(opts, penalty_same_particle_repeated, json, "penalty_same_particle_repeated");
  SET_OPT(opts, penalty_hiragana_noun_starts_with_particle, json, "penalty_hiragana_noun_starts_with_particle");
  SET_OPT(opts, penalty_particle_before_single_hiragana_other, json, "penalty_particle_before_single_hiragana_other");
  SET_OPT(opts, penalty_particle_before_multi_hiragana_other, json, "penalty_particle_before_multi_hiragana_other");
  SET_OPT(opts, bonus_shi_after_i_adj, json, "bonus_shi_after_i_adj");
  SET_OPT(opts, bonus_shi_after_verb, json, "bonus_shi_after_verb");
  SET_OPT(opts, bonus_shi_after_aux, json, "bonus_shi_after_aux");
  SET_OPT(opts, penalty_shi_after_noun, json, "penalty_shi_after_noun");
  SET_OPT(opts, penalty_suffix_at_start, json, "penalty_suffix_at_start");
  SET_OPT(opts, penalty_suffix_after_symbol, json, "penalty_suffix_after_symbol");
  SET_OPT(opts, penalty_prefix_before_verb, json, "penalty_prefix_before_verb");
  SET_OPT(opts, penalty_noun_before_verb_aux, json, "penalty_noun_before_verb_aux");
}

inline void ScorerOptionsLoader::applyJoinOptions(JoinOptions& opts, const JsonValue& json) {
  SET_OPT(opts, compound_verb_bonus, json, "compound_verb_bonus");
  SET_OPT(opts, verified_v1_bonus, json, "verified_v1_bonus");
  SET_OPT(opts, verified_noun_bonus, json, "verified_noun_bonus");
  SET_OPT(opts, te_form_aux_bonus, json, "te_form_aux_bonus");
}

inline void ScorerOptionsLoader::applySplitOptions(SplitOptions& opts, const JsonValue& json) {
  SET_OPT(opts, alpha_kanji_bonus, json, "alpha_kanji_bonus");
  SET_OPT(opts, alpha_katakana_bonus, json, "alpha_katakana_bonus");
  SET_OPT(opts, digit_kanji_1_bonus, json, "digit_kanji_1_bonus");
  SET_OPT(opts, digit_kanji_2_bonus, json, "digit_kanji_2_bonus");
  SET_OPT(opts, digit_kanji_3_penalty, json, "digit_kanji_3_penalty");
  SET_OPT(opts, dict_split_bonus, json, "dict_split_bonus");
  SET_OPT(opts, split_base_cost, json, "split_base_cost");
  SET_OPT(opts, noun_verb_split_bonus, json, "noun_verb_split_bonus");
  SET_OPT(opts, verified_verb_bonus, json, "verified_verb_bonus");
}

#undef SET_OPT

inline bool ScorerOptionsLoader::loadFromFile(const std::string& path, ScorerOptions& options,
                                              std::string* error_msg) {
  std::ifstream file(path);
  if (!file) {
    if (error_msg) *error_msg = "Cannot open file: " + path;
    return false;
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string json = buffer.str();

  Parser parser(json);
  auto root = parser.parse();

  // Check for parse errors
  if (parser.hasError()) {
    if (error_msg) *error_msg = std::string("JSON parse error: ") + parser.errorMessage();
    return false;
  }

  if (!root.isObject()) {
    if (error_msg) *error_msg = "JSON root must be an object";
    return false;
  }

  // Apply connection_rules section
  if (auto* conn_rules = root.get("connection_rules")) {
    if (conn_rules->isObject()) {
      if (auto* edge = conn_rules->get("edge")) {
        if (edge->isObject()) applyEdgeOptions(options.connection_rules.edge, *edge);
      }
      if (auto* conn = conn_rules->get("connection")) {
        if (conn->isObject()) applyConnectionOptions(options.connection_rules.connection, *conn);
      }
    }
  }

  // Apply candidates section
  if (auto* cands = root.get("candidates")) {
    if (cands->isObject()) {
      if (auto* join = cands->get("join")) {
        if (join->isObject()) applyJoinOptions(options.candidates.join, *join);
      }
      if (auto* split = cands->get("split")) {
        if (split->isObject()) applySplitOptions(options.candidates.split, *split);
      }
    }
  }

  return true;
}

// =============================================================================
// Environment Variable Override Implementation
// =============================================================================

#ifndef __EMSCRIPTEN__

namespace env_override_internal {

// Helper to try parsing float from environment variable
inline bool tryGetEnvFloat(const char* name, float& out) {
  const char* value = std::getenv(name);
  if (!value) return false;

  char* end = nullptr;
  float parsed = std::strtof(value, &end);
  if (end == value || *end != '\0') {
    std::cerr << "warning: Invalid value for " << name << ": " << value << "\n";
    return false;
  }
  out = parsed;
  return true;
}

// Macro to check and apply single environment variable
#define TRY_ENV(section, field) \
  if (tryGetEnvFloat("SUZUME_SCORER_" section "_" #field, opts.field)) count++

}  // namespace env_override_internal

inline int ScorerOptionsLoader::applyEnvOverrides(ScorerOptions& options) {
  using namespace env_override_internal;
  int count = 0;

  // Edge options (SUZUME_SCORER_EDGE_*)
  {
    auto& opts = options.connection_rules.edge;
    TRY_ENV("EDGE", penalty_invalid_adj_sou);
    TRY_ENV("EDGE", penalty_invalid_tai_pattern);
    TRY_ENV("EDGE", penalty_verb_aux_in_adj);
    TRY_ENV("EDGE", penalty_shimai_as_adj);
    TRY_ENV("EDGE", penalty_verb_onbin_as_adj);
    TRY_ENV("EDGE", penalty_short_stem_hiragana_adj);
    TRY_ENV("EDGE", penalty_verb_tai_rashii);
    TRY_ENV("EDGE", penalty_verb_nai_pattern);
    TRY_ENV("EDGE", bonus_unified_verb_aux);
  }

  // Connection options (SUZUME_SCORER_CONN_*)
  {
    auto& opts = options.connection_rules.connection;
    TRY_ENV("CONN", penalty_copula_after_verb);
    TRY_ENV("CONN", penalty_ichidan_renyokei_te);
    TRY_ENV("CONN", bonus_tai_after_renyokei);
    TRY_ENV("CONN", penalty_yasui_after_renyokei);
    TRY_ENV("CONN", penalty_nagara_split);
    TRY_ENV("CONN", penalty_sou_after_renyokei);
    TRY_ENV("CONN", penalty_te_form_split);
    TRY_ENV("CONN", penalty_taku_te_split);
    TRY_ENV("CONN", penalty_takute_after_renyokei);
    TRY_ENV("CONN", bonus_conditional_verb_to_verb);
    TRY_ENV("CONN", bonus_verb_renyokei_compound_aux);
    TRY_ENV("CONN", penalty_toku_contraction_split);
    TRY_ENV("CONN", bonus_te_form_verb_to_verb);
    TRY_ENV("CONN", bonus_rashii_after_predicate);
    TRY_ENV("CONN", penalty_verb_to_case_particle);
    TRY_ENV("CONN", penalty_tai_after_aux);
    TRY_ENV("CONN", penalty_masen_de_split);
    TRY_ENV("CONN", penalty_invalid_single_char_aux);
    TRY_ENV("CONN", penalty_te_form_ta_contraction);
    TRY_ENV("CONN", penalty_noun_mai);
    TRY_ENV("CONN", penalty_short_aux_after_particle);
    TRY_ENV("CONN", bonus_noun_mitai);
    TRY_ENV("CONN", bonus_verb_mitai);
    TRY_ENV("CONN", penalty_iru_aux_after_noun);
    TRY_ENV("CONN", bonus_iru_aux_after_te_form);
    TRY_ENV("CONN", bonus_shimau_aux_after_te_form);
    TRY_ENV("CONN", penalty_character_speech_split);
    TRY_ENV("CONN", bonus_adj_ku_naru);
    TRY_ENV("CONN", penalty_compound_aux_after_renyokei);
    TRY_ENV("CONN", penalty_yoru_night_after_ni);
    TRY_ENV("CONN", penalty_formal_noun_before_kanji);
    TRY_ENV("CONN", penalty_same_particle_repeated);
    TRY_ENV("CONN", penalty_hiragana_noun_starts_with_particle);
    TRY_ENV("CONN", penalty_particle_before_single_hiragana_other);
    TRY_ENV("CONN", penalty_particle_before_multi_hiragana_other);
    TRY_ENV("CONN", bonus_shi_after_i_adj);
    TRY_ENV("CONN", bonus_shi_after_verb);
    TRY_ENV("CONN", bonus_shi_after_aux);
    TRY_ENV("CONN", penalty_shi_after_noun);
    TRY_ENV("CONN", penalty_suffix_at_start);
    TRY_ENV("CONN", penalty_suffix_after_symbol);
    TRY_ENV("CONN", penalty_prefix_before_verb);
    TRY_ENV("CONN", penalty_noun_before_verb_aux);
    TRY_ENV("CONN", penalty_prefix_short_stem_hiragana_adj);
  }

  // Join options (SUZUME_SCORER_JOIN_*)
  {
    auto& opts = options.candidates.join;
    TRY_ENV("JOIN", compound_verb_bonus);
    TRY_ENV("JOIN", verified_v1_bonus);
    TRY_ENV("JOIN", verified_noun_bonus);
    TRY_ENV("JOIN", te_form_aux_bonus);
  }

  // Split options (SUZUME_SCORER_SPLIT_*)
  {
    auto& opts = options.candidates.split;
    TRY_ENV("SPLIT", alpha_kanji_bonus);
    TRY_ENV("SPLIT", alpha_katakana_bonus);
    TRY_ENV("SPLIT", digit_kanji_1_bonus);
    TRY_ENV("SPLIT", digit_kanji_2_bonus);
    TRY_ENV("SPLIT", digit_kanji_3_penalty);
    TRY_ENV("SPLIT", dict_split_bonus);
    TRY_ENV("SPLIT", split_base_cost);
    TRY_ENV("SPLIT", noun_verb_split_bonus);
    TRY_ENV("SPLIT", verified_verb_bonus);
  }

  return count;
}

inline ScorerLoadResult ScorerOptionsLoader::loadFromEnv(ScorerOptions& options) {
  ScorerLoadResult result;

  // Check for SUZUME_SCORER_CONFIG environment variable (JSON file path)
  const char* config_path = std::getenv("SUZUME_SCORER_CONFIG");
  if (config_path && config_path[0] != '\0') {
    std::string error_msg;
    if (loadFromFile(config_path, options, &error_msg)) {
      result.config_path = config_path;
    } else {
      std::cerr << "warning: Failed to load scorer config from SUZUME_SCORER_CONFIG: "
                << error_msg << "\n";
    }
  }

  // Apply individual environment variable overrides (highest priority)
  result.env_override_count = applyEnvOverrides(options);

  return result;
}

#undef TRY_ENV

#endif  // __EMSCRIPTEN__

}  // namespace suzume::analysis

#endif  // SUZUME_ANALYSIS_SCORER_OPTIONS_LOADER_H_
