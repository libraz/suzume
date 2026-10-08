#include <cstring>
#include <utility>

#include "cli_common.h"
#include "postprocess/tag_generator.h"

namespace suzume {
namespace cli {

namespace {

bool isKnownCommand(std::string_view value) {
  return value == "analyze" || value == "dict" || value == "test" || value == "version" || value == "help";
}

bool isKnownOption(std::string_view value) {
  return value == "--" || value == "-h" || value == "--help" || value == "-v" || value == "--version" ||
         value == "-V" || value == "--verbose" || value == "-VV" || value == "--very-verbose" || value == "--debug" ||
         value == "-d" || value == "--dict" || value == "-m" || value == "--mode" || value == "-f" ||
         value == "--format" || value == "--no-user-dict" || value == "--no-core-dict" ||
         value == "--skip-env-config" || value == "--compare" || value == "--normalize-vu" || value == "--lowercase" ||
         value == "--preserve-symbols" || value == "--no-lemmatize" || value == "--merge-compounds" ||
         value == "--include-particles" || value == "--include-auxiliaries" || value == "--include-formal-nouns" ||
         value == "--include-low-info" || value == "--tag-keep-duplicates" || value == "--tag-use-surface" ||
         value == "--tag-pos" || value == "--tag-exclude-basic" || value == "--tag-min-length" ||
         value == "--tag-max-tags" || value == "--expect" || value == "--file";
}

bool isValidMode(std::string_view value) {
  return value == "normal" || value == "search" || value == "split";
}

bool tryParseOutputFormat(std::string_view value, OutputFormat* output) {
  if (output == nullptr) {
    return false;
  }
  if (value == "morpheme") {
    *output = OutputFormat::Morpheme;
  } else if (value == "tags") {
    *output = OutputFormat::Tags;
  } else if (value == "json") {
    *output = OutputFormat::Json;
  } else if (value == "tsv") {
    *output = OutputFormat::Morpheme;
  } else if (value == "chasen") {
    *output = OutputFormat::Chasen;
  } else {
    return false;
  }
  return true;
}

bool takeOptionValue(int argc, char* argv[], int* index, std::string_view option, std::string* value,
                     std::string* error) {
  if (*index + 1 >= argc || isKnownOption(argv[*index + 1])) {
    *error = "Missing value for " + std::string(option);
    return false;
  }
  *value = argv[++(*index)];
  return true;
}

bool parseTagPos(std::string_view value, uint8_t* filter) {
  uint8_t bit = 0;
  if (value == "noun") {
    bit = postprocess::kTagPosNoun;
  } else if (value == "verb") {
    bit = postprocess::kTagPosVerb;
  } else if (value == "adjective") {
    bit = postprocess::kTagPosAdjective;
  } else if (value == "adverb") {
    bit = postprocess::kTagPosAdverb;
  } else if (value == "particle") {
    bit = postprocess::kTagPosParticle;
  } else if (value == "auxiliary") {
    bit = postprocess::kTagPosAuxiliary;
  } else {
    return false;
  }
  *filter = static_cast<uint8_t>(*filter | bit);  // NOLINT(hicpp-signed-bitwise): bit flag operation
  return true;
}

}  // namespace

CommandArgs parseArgs(int argc, char* argv[]) {
  CommandArgs args;

  int idx = 1;
  bool positional_only = false;
  while (idx < argc) {
    std::string arg = argv[idx];

    // After a literal "--", every remaining argument is positional.
    if (positional_only) {
      if (args.command.empty()) {
        args.command = "analyze";
      }
      args.args.push_back(arg);
      ++idx;
      continue;
    }

    if (arg == "--") {
      if (args.command.empty()) {
        args.command = "analyze";
      }
      positional_only = true;
      ++idx;
      continue;
    }

    if (arg == "-h" || arg == "--help") {
      args.help = true;
      ++idx;
      continue;
    }

    if (arg == "-v" || arg == "--version") {
      args.version = true;
      ++idx;
      continue;
    }

    if (arg == "-V" || arg == "--verbose") {
      args.verbose = true;
      ++idx;
      continue;
    }

    if (arg == "-VV" || arg == "--very-verbose") {
      args.verbose = true;
      args.very_verbose = true;
      args.debug = true;
      ++idx;
      continue;
    }

    // Dict owns its complete option grammar. Test owns only its expectation,
    // input-file, and benchmark options; shared analysis options continue
    // through the regular parser below.
    if (args.command == "dict") {
      args.args.push_back(arg);
      ++idx;
      continue;
    }
    if (args.command == "test" &&
        (arg == "--expect" || arg.rfind("--expect=", 0) == 0 || arg == "-f" || arg == "--file" ||
         arg.rfind("--file=", 0) == 0 || arg.rfind("--iterations=", 0) == 0 || arg.rfind("--samples=", 0) == 0 ||
         arg.rfind("--warmup=", 0) == 0)) {
      args.args.push_back(arg);
      ++idx;
      continue;
    }

    if (arg[0] != '-') {
      if (args.command.empty() && isKnownCommand(arg)) {
        args.command = arg;
      } else {
        if (args.command.empty()) {
          args.command = "analyze";
        }
        args.args.push_back(arg);
      }
      ++idx;
      continue;
    }

    if (arg == "--debug") {
      args.debug = true;
      ++idx;
      continue;
    }

    if (arg == "-d" || arg == "--dict") {
      std::string value;
      if (!takeOptionValue(argc, argv, &idx, arg, &value, &args.parse_error)) {
        return args;
      }
      args.dict_paths.push_back(std::move(value));
      ++idx;
      continue;
    }
    if (arg.rfind("--dict=", 0) == 0) {
      std::string value = arg.substr(std::strlen("--dict="));
      if (value.empty()) {
        args.parse_error = "Missing value for --dict";
        return args;
      }
      args.dict_paths.push_back(std::move(value));
      ++idx;
      continue;
    }

    if (arg == "-m" || arg == "--mode") {
      std::string value;
      if (!takeOptionValue(argc, argv, &idx, arg, &value, &args.parse_error)) {
        return args;
      }
      if (!isValidMode(value)) {
        args.parse_error = "Invalid mode: " + value + " (expected normal, search, or split)";
        return args;
      }
      args.mode = std::move(value);
      ++idx;
      continue;
    }
    if (arg.rfind("--mode=", 0) == 0) {
      std::string value = arg.substr(std::strlen("--mode="));
      if (!isValidMode(value)) {
        args.parse_error = "Invalid mode: " + value + " (expected normal, search, or split)";
        return args;
      }
      args.mode = std::move(value);
      ++idx;
      continue;
    }

    if (arg == "-f" || arg == "--format") {
      std::string value;
      if (!takeOptionValue(argc, argv, &idx, arg, &value, &args.parse_error)) {
        return args;
      }
      if (!tryParseOutputFormat(value, &args.format)) {
        args.parse_error = "Invalid format: " + value + " (expected morpheme, tags, json, tsv, or chasen)";
        return args;
      }
      ++idx;
      continue;
    }
    if (arg.rfind("--format=", 0) == 0) {
      std::string value = arg.substr(std::strlen("--format="));
      if (!tryParseOutputFormat(value, &args.format)) {
        args.parse_error = "Invalid format: " + value + " (expected morpheme, tags, json, tsv, or chasen)";
        return args;
      }
      ++idx;
      continue;
    }

    if (arg == "--no-user-dict") {
      args.no_user_dict = true;
      ++idx;
      continue;
    }

    if (arg == "--no-core-dict") {
      args.no_core_dict = true;
      ++idx;
      continue;
    }

    if (arg == "--skip-env-config") {
      args.skip_env_config = true;
      ++idx;
      continue;
    }

    if (arg == "--compare") {
      args.compare = true;
      ++idx;
      continue;
    }

    if (arg == "--normalize-vu") {
      args.normalize_vu = true;
      ++idx;
      continue;
    }

    if (arg == "--lowercase") {
      args.lowercase = true;
      ++idx;
      continue;
    }

    if (arg == "--preserve-symbols") {
      args.preserve_symbols = true;
      ++idx;
      continue;
    }

    if (arg == "--no-lemmatize") {
      args.no_lemmatize = true;
      ++idx;
      continue;
    }

    if (arg == "--merge-compounds") {
      args.merge_compounds = true;
      ++idx;
      continue;
    }

    if (arg == "--include-particles") {
      args.tag_include_particles = true;
      ++idx;
      continue;
    }

    if (arg == "--include-auxiliaries") {
      args.tag_include_auxiliaries = true;
      ++idx;
      continue;
    }

    if (arg == "--include-formal-nouns") {
      args.tag_include_formal_nouns = true;
      ++idx;
      continue;
    }

    if (arg == "--include-low-info") {
      args.tag_include_low_info = true;
      ++idx;
      continue;
    }

    if (arg == "--tag-keep-duplicates") {
      args.tag_keep_duplicates = true;
      ++idx;
      continue;
    }

    if (arg == "--tag-use-surface") {
      args.tag_use_surface = true;
      ++idx;
      continue;
    }

    if (arg == "--tag-pos") {
      std::string value;
      if (!takeOptionValue(argc, argv, &idx, arg, &value, &args.parse_error)) {
        return args;
      }
      if (!parseTagPos(value, &args.tag_pos_filter)) {
        args.parse_error =
            "Invalid --tag-pos value: " + value + " (expected noun, verb, adjective, adverb, particle, or auxiliary)";
        return args;
      }
      ++idx;
      continue;
    }
    if (arg.rfind("--tag-pos=", 0) == 0) {
      std::string value = arg.substr(std::strlen("--tag-pos="));
      if (!parseTagPos(value, &args.tag_pos_filter)) {
        args.parse_error =
            "Invalid --tag-pos value: " + value + " (expected noun, verb, adjective, adverb, particle, or auxiliary)";
        return args;
      }
      ++idx;
      continue;
    }

    if (arg == "--tag-exclude-basic") {
      args.tag_exclude_basic = true;
      ++idx;
      continue;
    }

    if (arg == "--tag-min-length") {
      std::string value;
      if (!takeOptionValue(argc, argv, &idx, arg, &value, &args.parse_error)) {
        return args;
      }
      if (!parseSizeOption(value, &args.tag_min_length)) {
        args.parse_error = "Invalid --tag-min-length value: " + value;
        return args;
      }
      ++idx;
      continue;
    }
    if (arg.rfind("--tag-min-length=", 0) == 0) {
      std::string value = arg.substr(std::strlen("--tag-min-length="));
      if (!parseSizeOption(value, &args.tag_min_length)) {
        args.parse_error = "Invalid --tag-min-length value: " + value;
        return args;
      }
      ++idx;
      continue;
    }

    if (arg == "--tag-max-tags") {
      std::string value;
      if (!takeOptionValue(argc, argv, &idx, arg, &value, &args.parse_error)) {
        return args;
      }
      if (!parseSizeOption(value, &args.tag_max_tags)) {
        args.parse_error = "Invalid --tag-max-tags value: " + value;
        return args;
      }
      ++idx;
      continue;
    }
    if (arg.rfind("--tag-max-tags=", 0) == 0) {
      std::string value = arg.substr(std::strlen("--tag-max-tags="));
      if (!parseSizeOption(value, &args.tag_max_tags)) {
        args.parse_error = "Invalid --tag-max-tags value: " + value;
        return args;
      }
      ++idx;
      continue;
    }

    if (args.command == "test") {
      args.args.push_back(arg);
      ++idx;
      continue;
    }
    if (args.command.empty()) {
      args.command = "analyze";
    }
    args.parse_error = "Unknown analysis option: " + arg;
    return args;
  }

  // Default command is analyze
  if (args.command.empty() && !args.help) {
    args.command = "analyze";
  }

  return args;
}

}  // namespace cli
}  // namespace suzume
