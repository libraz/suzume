#include <array>
#include <cstring>
#include <utility>

#include "cli_common.h"
#include "postprocess/tag_generator.h"

namespace suzume {
namespace cli {

namespace {

/** @brief Which commands an option belongs to. */
enum class OptionScope : uint8_t {
  Global,    // every command, including dict, which owns the rest of its grammar
  Analysis,  // analyze and test
  TestOnly   // passed through verbatim to the test command's own parser
};

/** @brief Applies a parsed option; returns false with @p error set on an invalid value. */
using OptionApply = bool (*)(CommandArgs* args, std::string_view value, std::string* error);

/**
 * @brief One command-line option. The parser, the known-option check used for
 *        missing values, and the test passthrough all read this table.
 */
struct OptionSpec {
  std::string_view long_name;
  std::string_view short_name;  // empty when the option has no short form
  bool takes_value;
  OptionScope scope;
  OptionApply apply;
};

bool isKnownCommand(std::string_view value) {
  return value == "analyze" || value == "dict" || value == "test" || value == "version" || value == "help";
}

bool parseMode(std::string_view value, std::string* mode, std::string* error) {
  if (value != "normal" && value != "search" && value != "split") {
    *error = "invalid mode: " + std::string(value) + " (expected one of: normal, search, split)";
    return false;
  }
  *mode = std::string(value);
  return true;
}

bool parseFormat(std::string_view value, OutputFormat* format, std::string* error) {
  if (value == "tsv") {
    *format = OutputFormat::Tsv;
  } else if (value == "json") {
    *format = OutputFormat::Json;
  } else if (value == "chasen") {
    *format = OutputFormat::Chasen;
  } else if (value == "tags") {
    *format = OutputFormat::Tags;
  } else {
    *error = "invalid format: " + std::string(value) + " (expected one of: tsv, json, chasen, tags)";
    return false;
  }
  return true;
}

bool parseTagPos(std::string_view value, uint8_t* filter, std::string* error) {
  static constexpr std::array<std::pair<std::string_view, uint8_t>, 6> kNames = {{
      {"NOUN", postprocess::kTagPosNoun},
      {"VERB", postprocess::kTagPosVerb},
      {"ADJ", postprocess::kTagPosAdjective},
      {"ADV", postprocess::kTagPosAdverb},
      {"PARTICLE", postprocess::kTagPosParticle},
      {"AUX", postprocess::kTagPosAuxiliary},
  }};
  for (const auto& [name, bit] : kNames) {
    if (value == name) {
      *filter = static_cast<uint8_t>(*filter | bit);  // NOLINT(hicpp-signed-bitwise): bit flag operation
      return true;
    }
  }
  *error = "invalid --tag-pos-filter value: " + std::string(value) +
           " (expected one of: NOUN, VERB, ADJ, ADV, PARTICLE, AUX)";
  return false;
}

bool parseSize(std::string_view option, std::string_view value, size_t* out, std::string* error) {
  if (!parseSizeOption(value, out)) {
    *error = "invalid " + std::string(option) + " value: " + std::string(value);
    return false;
  }
  return true;
}

// Captureless lambdas convert to OptionApply. Flags ignore the value argument.
#define SUZUME_CLI_FLAG(member, set_to) /* NOLINT(cppcoreguidelines-macro-usage) */ \
  [](CommandArgs* args, std::string_view, std::string*) { return (args->member = (set_to), true); }

const OptionSpec kOptions[] = {
    // NOLINT(cppcoreguidelines-avoid-c-arrays): size follows the entries
    {"--help", "-h", false, OptionScope::Global, SUZUME_CLI_FLAG(help, true)},
    {"--version", "-v", false, OptionScope::Global, SUZUME_CLI_FLAG(version, true)},
    {"--verbose", "-V", false, OptionScope::Global, SUZUME_CLI_FLAG(verbose, true)},
    {"--very-verbose", "-VV", false, OptionScope::Global,
     [](CommandArgs* args, std::string_view, std::string*) {
       args->verbose = true;
       args->very_verbose = true;
       args->debug = true;
       return true;
     }},
    {"--debug", "", false, OptionScope::Analysis, SUZUME_CLI_FLAG(debug, true)},
    {"--dictionary", "-d", true, OptionScope::Analysis,
     [](CommandArgs* args, std::string_view value, std::string*) {
       args->dict_paths.emplace_back(value);
       return true;
     }},
    {"--mode", "-m", true, OptionScope::Analysis,
     [](CommandArgs* args, std::string_view value, std::string* error) {
       return parseMode(value, &args->mode, error);
     }},
    {"--format", "-f", true, OptionScope::Analysis,
     [](CommandArgs* args, std::string_view value, std::string* error) {
       return parseFormat(value, &args->format, error);
     }},
    {"--skip-core-dictionary", "", false, OptionScope::Analysis, SUZUME_CLI_FLAG(skip_core_dictionary, true)},
    {"--skip-bundled-user-dictionary", "", false, OptionScope::Analysis,
     SUZUME_CLI_FLAG(skip_bundled_user_dictionary, true)},
    {"--skip-env-config", "", false, OptionScope::Analysis, SUZUME_CLI_FLAG(skip_env_config, true)},
    {"--compare", "", false, OptionScope::Analysis, SUZUME_CLI_FLAG(compare, true)},
    {"--no-preserve-vu", "", false, OptionScope::Analysis, SUZUME_CLI_FLAG(preserve_vu, false)},
    {"--no-preserve-case", "", false, OptionScope::Analysis, SUZUME_CLI_FLAG(preserve_case, false)},
    {"--preserve-symbols", "", false, OptionScope::Analysis, SUZUME_CLI_FLAG(preserve_symbols, true)},
    {"--no-lemmatize", "", false, OptionScope::Analysis, SUZUME_CLI_FLAG(lemmatize, false)},
    {"--merge-compounds", "", false, OptionScope::Analysis, SUZUME_CLI_FLAG(merge_compounds, true)},
    {"--tag-no-exclude-particles", "", false, OptionScope::Analysis, SUZUME_CLI_FLAG(tag_exclude_particles, false)},
    {"--tag-no-exclude-auxiliaries", "", false, OptionScope::Analysis, SUZUME_CLI_FLAG(tag_exclude_auxiliaries, false)},
    {"--tag-no-exclude-formal-nouns", "", false, OptionScope::Analysis,
     SUZUME_CLI_FLAG(tag_exclude_formal_nouns, false)},
    {"--tag-no-exclude-low-info", "", false, OptionScope::Analysis, SUZUME_CLI_FLAG(tag_exclude_low_info, false)},
    {"--tag-exclude-basic", "", false, OptionScope::Analysis, SUZUME_CLI_FLAG(tag_exclude_basic, true)},
    {"--tag-no-remove-duplicates", "", false, OptionScope::Analysis, SUZUME_CLI_FLAG(tag_remove_duplicates, false)},
    {"--tag-no-use-base-form", "", false, OptionScope::Analysis, SUZUME_CLI_FLAG(tag_use_base_form, false)},
    {"--tag-pos-filter", "", true, OptionScope::Analysis,
     [](CommandArgs* args, std::string_view value, std::string* error) {
       return parseTagPos(value, &args->tag_pos_filter, error);
     }},
    {"--tag-min-length", "", true, OptionScope::Analysis,
     [](CommandArgs* args, std::string_view value, std::string* error) {
       return parseSize("--tag-min-length", value, &args->tag_min_length, error);
     }},
    {"--tag-max-tags", "", true, OptionScope::Analysis,
     [](CommandArgs* args, std::string_view value, std::string* error) {
       return parseSize("--tag-max-tags", value, &args->tag_max_tags, error);
     }},
    {"--expect", "", true, OptionScope::TestOnly, nullptr},
    {"--file", "", true, OptionScope::TestOnly, nullptr},
    {"--iterations", "", true, OptionScope::TestOnly, nullptr},
    {"--samples", "", true, OptionScope::TestOnly, nullptr},
    {"--warmup", "", true, OptionScope::TestOnly, nullptr},
    {"--", "", false, OptionScope::Global, nullptr},
};

#undef SUZUME_CLI_FLAG

const OptionSpec* findOption(std::string_view name) {
  for (const auto& spec : kOptions) {
    if (name == spec.long_name || (!spec.short_name.empty() && name == spec.short_name)) {
      return &spec;
    }
  }
  return nullptr;
}

bool isTagOption(const OptionSpec& spec) {
  return spec.long_name.rfind("--tag-", 0) == 0;
}

/** @brief The table entry an argument names, with any attached "--name=value" value split off. */
struct MatchedOption {
  const OptionSpec* spec = nullptr;
  bool has_attached_value = false;
  std::string_view attached_value;
};

MatchedOption matchOption(std::string_view arg) {
  MatchedOption match;
  match.spec = findOption(arg);
  if (match.spec != nullptr || arg.rfind("--", 0) != 0) {
    return match;
  }
  const size_t equals = arg.find('=');
  if (equals == std::string_view::npos) {
    return match;
  }
  const OptionSpec* spec = findOption(arg.substr(0, equals));
  if (spec != nullptr && spec->takes_value) {
    match.spec = spec;
    match.has_attached_value = true;
    match.attached_value = arg.substr(equals + 1);
  }
  return match;
}

}  // namespace

CommandArgs parseArgs(int argc, char* argv[]) {
  CommandArgs args;

  int idx = 1;
  bool positional_only = false;
  for (; idx < argc; ++idx) {
    const std::string_view arg = argv[idx];

    // After a literal "--", every remaining argument is positional.
    if (positional_only || arg == "--") {
      if (args.command.empty()) {
        args.command = "analyze";
      }
      if (positional_only) {
        args.args.emplace_back(arg);
      }
      positional_only = true;
      continue;
    }

    const MatchedOption match = matchOption(arg);
    const OptionSpec* spec = match.spec;

    // Dict owns its complete option grammar after the global options.
    if (args.command == "dict" && (spec == nullptr || spec->scope != OptionScope::Global)) {
      args.args.emplace_back(arg);
      continue;
    }

    if (arg.empty() || arg[0] != '-') {
      if (args.command.empty() && isKnownCommand(arg)) {
        args.command = std::string(arg);
      } else {
        if (args.command.empty()) {
          args.command = "analyze";
        }
        args.args.emplace_back(arg);
      }
      continue;
    }

    if (spec != nullptr && spec->scope == OptionScope::TestOnly && args.command == "test") {
      args.args.emplace_back(arg);
      if (!match.has_attached_value && idx + 1 < argc) {
        args.args.emplace_back(argv[++idx]);
      }
      continue;
    }

    if (spec == nullptr || spec->scope == OptionScope::TestOnly) {
      // The test command's own parser reports options it does not know.
      if (args.command == "test") {
        args.args.emplace_back(arg);
        continue;
      }
      if (args.command.empty()) {
        args.command = "analyze";
      }
      args.parse_error = "unknown option: " + std::string(arg);
      return args;
    }

    std::string_view value;
    if (match.has_attached_value) {
      value = match.attached_value;
      if (value.empty()) {
        args.parse_error = "missing value for " + std::string(spec->long_name);
        return args;
      }
    } else if (spec->takes_value) {
      if (idx + 1 >= argc || findOption(argv[idx + 1]) != nullptr) {
        args.parse_error = "missing value for " + std::string(arg);
        return args;
      }
      value = argv[++idx];
    }

    if (!spec->apply(&args, value, &args.parse_error)) {
      return args;
    }
    if (isTagOption(*spec)) {
      args.tag_option_given = true;
    }
  }

  // Default command is analyze
  if (args.command.empty() && !args.help) {
    args.command = "analyze";
  }

  // Tag options shape only -f tags output; elsewhere they would be silently ignored.
  // The test command consumes them for tag test files without -f tags.
  if (args.command == "analyze" && !args.help && args.tag_option_given && args.format != OutputFormat::Tags) {
    args.parse_error = "--tag-* options require -f tags";
  }

  return args;
}

}  // namespace cli
}  // namespace suzume
