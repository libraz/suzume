#include "cli_common.h"

#include <gtest/gtest.h>

namespace suzume::cli {
namespace {

TEST(CliCommonTest, JsonEscapeEscapesSpecialCharacters) {
  EXPECT_EQ(jsonEscape("a\"b\\c"), "a\\\"b\\\\c");
  EXPECT_EQ(jsonEscape("line\nnext\tend"), "line\\nnext\\tend");
}

TEST(CliCommonTest, WildcardMatchesLiteralAndWildcardCharacters) {
  EXPECT_TRUE(wildcardMatches("記号*終", "記号.^$+()[]{}|\\終"));
  EXPECT_TRUE(wildcardMatches("??", "ab"));
  EXPECT_FALSE(wildcardMatches("??", "a"));
  EXPECT_TRUE(wildcardMatches("a**b", "axxxb"));
  EXPECT_FALSE(wildcardMatches("a*b", "axxxc"));
}

TEST(CliCommonTest, WildcardValidationRejectsExcessiveStars) {
  const std::string pattern(65, '*');
  auto result = validateWildcardPattern(pattern);

  EXPECT_FALSE(result.hasValue());
  EXPECT_EQ(result.error().code, core::ErrorCode::InvalidInput);
  EXPECT_NE(result.error().message.find("too many '*'"), std::string::npos);
}

TEST(CliCommonTest, DictionaryExtensionsAreCaseInsensitive) {
  EXPECT_TRUE(hasExtension("user.dic", ".dic"));
  EXPECT_TRUE(hasExtension("user.DIC", ".dic"));
  EXPECT_TRUE(hasExtension("user.TsV", ".tsv"));
  EXPECT_FALSE(hasExtension("user.dic.tmp", ".dic"));
  EXPECT_EQ(swapOrAppendExtension("user.TSV", ".tsv", ".dic"), "user.dic");
}

TEST(CliCommonTest, JsonEscapeEscapesControlCharacters) {
  std::string input;
  input.push_back('\x01');
  input += "ok";

  EXPECT_EQ(jsonEscape(input), "\\u0001ok");
}

TEST(CliCommonTest, JsonEscapeMatchesPythonJsonDumpsWithoutAsciiEscaping) {
  EXPECT_EQ(jsonEscape("\b\f\x1f"), "\\b\\f\\u001f");
  EXPECT_EQ(jsonEscape("\x7f"), "\x7f");
  EXPECT_EQ(jsonEscape("東京/é"), "東京/é");
}

TEST(CliCommonTest, TabEscapeKeepsEveryRecordOnOneLine) {
  EXPECT_EQ(tabEscape("a\\b\tc\r\nd"), "a\\\\b\\tc\\r\\nd");
}

TEST(CliCommonTest, JsonEscapeRejectsInvalidUnicodeScalars) {
  EXPECT_EQ(jsonEscape(std::string("\xFF", 1)), "\\ufffd");
  EXPECT_EQ(jsonEscape(std::string("\xC0\xAF", 2)), "\\ufffd\\ufffd");
  EXPECT_EQ(jsonEscape(std::string("\xED\xA0\x80", 3)), "\\ufffd\\ufffd\\ufffd");
  EXPECT_EQ(jsonEscape(std::string("\xF4\x90\x80\x80", 4)), "\\ufffd\\ufffd\\ufffd\\ufffd");
}

TEST(CliCommonTest, ParseSizeOptionRejectsInvalidInput) {
  size_t value = 123;

  EXPECT_FALSE(parseSizeOption("", &value));
  EXPECT_FALSE(parseSizeOption("12x", &value));
  EXPECT_FALSE(parseSizeOption("-1", &value));
  EXPECT_FALSE(parseSizeOption("1.5", &value));
  EXPECT_EQ(value, 123u);
}

TEST(CliCommonTest, ParseSizeOptionAcceptsSize) {
  size_t value = 0;

  EXPECT_TRUE(parseSizeOption("42", &value));
  EXPECT_EQ(value, 42u);
}

TEST(CliCommonTest, LimitZeroMeansUnlimited) {
  EXPECT_FALSE(limitReached(0, 0));
  EXPECT_FALSE(limitReached(100, 0));
  EXPECT_FALSE(limitReached(2, 3));
  EXPECT_TRUE(limitReached(3, 3));
}

template <size_t N>
CommandArgs parse(const char* (&argv)[N]) {
  return parseArgs(static_cast<int>(N), const_cast<char**>(argv));
}

TEST(CliCommonTest, ParseArgsAcceptsTagOptions) {
  const char* argv[] = {"suzume-cli",
                        "-f",
                        "tags",
                        "--tag-no-exclude-particles",
                        "--tag-no-exclude-auxiliaries",
                        "--tag-no-exclude-formal-nouns",
                        "--tag-no-exclude-low-info",
                        "--tag-no-remove-duplicates",
                        "--tag-no-use-base-form",
                        "--tag-pos-filter",
                        "NOUN",
                        "--tag-pos-filter=VERB",
                        "--tag-pos-filter",
                        "PARTICLE",
                        "--tag-pos-filter=AUX",
                        "--tag-pos-filter",
                        "NOUN",
                        "--tag-exclude-basic",
                        "--tag-min-length",
                        "1",
                        "--tag-max-tags",
                        "3",
                        "猫が走る"};
  auto args = parse(argv);

  EXPECT_TRUE(args.parse_error.empty()) << args.parse_error;
  EXPECT_EQ(args.format, OutputFormat::Tags);
  EXPECT_FALSE(args.tag_exclude_particles);
  EXPECT_FALSE(args.tag_exclude_auxiliaries);
  EXPECT_FALSE(args.tag_exclude_formal_nouns);
  EXPECT_FALSE(args.tag_exclude_low_info);
  EXPECT_FALSE(args.tag_remove_duplicates);
  EXPECT_FALSE(args.tag_use_base_form);
  EXPECT_EQ(args.tag_pos_filter, 51u);
  EXPECT_TRUE(args.tag_exclude_basic);
  EXPECT_EQ(args.tag_min_length, 1u);
  EXPECT_EQ(args.tag_max_tags, 3u);
}

TEST(CliCommonTest, ParseArgsDefaultsMatchLibraryOptions) {
  const char* argv[] = {"suzume-cli", "テスト"};
  auto args = parse(argv);

  EXPECT_EQ(args.format, OutputFormat::Tsv);
  EXPECT_TRUE(args.preserve_vu);
  EXPECT_TRUE(args.preserve_case);
  EXPECT_FALSE(args.preserve_symbols);
  EXPECT_TRUE(args.lemmatize);
  EXPECT_FALSE(args.merge_compounds);
  EXPECT_FALSE(args.skip_core_dictionary);
  EXPECT_FALSE(args.skip_bundled_user_dictionary);
  EXPECT_TRUE(args.tag_exclude_particles);
  EXPECT_TRUE(args.tag_exclude_auxiliaries);
  EXPECT_TRUE(args.tag_exclude_formal_nouns);
  EXPECT_TRUE(args.tag_exclude_low_info);
  EXPECT_TRUE(args.tag_remove_duplicates);
  EXPECT_TRUE(args.tag_use_base_form);
  EXPECT_EQ(args.tag_pos_filter, 0u);
}

TEST(CliCommonTest, ParseArgsAcceptsPosFilterNamesExactly) {
  for (const char* name : {"NOUN", "VERB", "ADJ", "ADV", "PARTICLE", "AUX"}) {
    const char* argv[] = {"suzume-cli", "-f", "tags", "--tag-pos-filter", name, "テスト"};
    auto args = parse(argv);
    EXPECT_TRUE(args.parse_error.empty()) << name << ": " << args.parse_error;
  }
  for (const char* name : {"noun", "adjective", "Adj", "", "OTHER"}) {
    const char* argv[] = {"suzume-cli", "-f", "tags", "--tag-pos-filter", name, "テスト"};
    auto args = parse(argv);
    EXPECT_EQ(args.parse_error, std::string("invalid --tag-pos-filter value: ") + name +
                                    " (expected one of: NOUN, VERB, ADJ, ADV, PARTICLE, AUX)");
  }
}

TEST(CliCommonTest, ParseArgsRejectsRemovedFlagSpellings) {
  for (const char* removed :
       {"--normalize-vu", "--lowercase", "--no-core-dict", "--no-user-dict", "--include-particles",
        "--include-auxiliaries", "--include-formal-nouns", "--include-low-info", "--tag-keep-duplicates",
        "--tag-use-surface", "--tag-pos", "--dict"}) {
    const char* argv[] = {"suzume-cli", removed, "テスト"};
    auto args = parse(argv);
    EXPECT_EQ(args.parse_error, std::string("unknown option: ") + removed);
  }
  const char* morpheme_format[] = {"suzume-cli", "-f", "morpheme", "テスト"};
  EXPECT_EQ(parse(morpheme_format).parse_error, "invalid format: morpheme (expected one of: tsv, json, chasen, tags)");
}

TEST(CliCommonTest, ParseArgsRequiresTagsFormatForTagOptionsInAnalyze) {
  const char* with_json[] = {"suzume-cli", "-f", "json", "--tag-min-length", "1", "テスト"};
  EXPECT_EQ(parse(with_json).parse_error, "--tag-* options require -f tags");

  const char* with_default[] = {"suzume-cli", "analyze", "--tag-exclude-basic", "テスト"};
  EXPECT_EQ(parse(with_default).parse_error, "--tag-* options require -f tags");

  const char* format_after[] = {"suzume-cli", "--tag-no-use-base-form", "-f", "tags", "テスト"};
  EXPECT_TRUE(parse(format_after).parse_error.empty());

  const char* in_test[] = {"suzume-cli", "test", "--tag-min-length", "1", "テスト", "--expect", "テスト"};
  auto test_args = parse(in_test);
  EXPECT_TRUE(test_args.parse_error.empty()) << test_args.parse_error;
  EXPECT_EQ(test_args.tag_min_length, 1u);
}

TEST(CliCommonTest, ParseArgsRejectsUnknownAnalysisOption) {
  const char* argv[] = {"suzume-cli", "-テスト"};
  auto args = parse(argv);

  EXPECT_EQ(args.command, "analyze");
  EXPECT_TRUE(args.args.empty());
  EXPECT_EQ(args.parse_error, "unknown option: -テスト");
}

TEST(CliCommonTest, ParseArgsAcceptsLeadingDashTextAfterOptionTerminator) {
  const char* argv[] = {"suzume-cli", "--", "-テスト"};
  auto args = parse(argv);

  EXPECT_EQ(args.command, "analyze");
  ASSERT_EQ(args.args.size(), 1u);
  EXPECT_EQ(args.args[0], "-テスト");
  EXPECT_TRUE(args.parse_error.empty());
}

TEST(CliCommonTest, ParseArgsConnectsAdvancedAnalyzeOptions) {
  const char* argv[] = {"suzume-cli",
                        "-VV",
                        "--skip-core-dictionary",
                        "--skip-bundled-user-dictionary",
                        "--skip-env-config",
                        "--no-lemmatize",
                        "--merge-compounds",
                        "--no-preserve-vu",
                        "--no-preserve-case",
                        "--preserve-symbols",
                        "--dictionary",
                        "a.tsv",
                        "-d",
                        "b.dic",
                        "--dictionary=c.tsv",
                        "テスト"};
  auto args = parse(argv);

  EXPECT_TRUE(args.parse_error.empty()) << args.parse_error;
  EXPECT_TRUE(args.verbose);
  EXPECT_TRUE(args.very_verbose);
  EXPECT_TRUE(args.debug);
  EXPECT_TRUE(args.skip_core_dictionary);
  EXPECT_TRUE(args.skip_bundled_user_dictionary);
  EXPECT_TRUE(args.skip_env_config);
  EXPECT_FALSE(args.lemmatize);
  EXPECT_TRUE(args.merge_compounds);
  EXPECT_FALSE(args.preserve_vu);
  EXPECT_FALSE(args.preserve_case);
  EXPECT_TRUE(args.preserve_symbols);
  EXPECT_EQ(args.dict_paths, (std::vector<std::string>{"a.tsv", "b.dic", "c.tsv"}));
}

TEST(CliCommonTest, ParseArgsRejectsInvalidModeAndFormat) {
  const char* invalid_mode[] = {"suzume-cli", "analyze", "--mode", "wide", "テスト"};
  EXPECT_EQ(parse(invalid_mode).parse_error, "invalid mode: wide (expected one of: normal, search, split)");

  const char* capitalized_mode[] = {"suzume-cli", "--mode=Search", "テスト"};
  EXPECT_EQ(parse(capitalized_mode).parse_error, "invalid mode: Search (expected one of: normal, search, split)");

  const char* invalid_format[] = {"suzume-cli", "--format=yaml", "テスト"};
  EXPECT_EQ(parse(invalid_format).parse_error, "invalid format: yaml (expected one of: tsv, json, chasen, tags)");

  for (const char* name : {"tsv", "json", "chasen", "tags"}) {
    const char* argv[] = {"suzume-cli", "-f", name, "テスト"};
    EXPECT_TRUE(parse(argv).parse_error.empty()) << name;
  }
}

TEST(CliCommonTest, ParseArgsRejectsMissingOptionValues) {
  const char* missing_dict[] = {"suzume-cli", "analyze", "--dictionary"};
  EXPECT_EQ(parse(missing_dict).parse_error, "missing value for --dictionary");

  const char* missing_mode[] = {"suzume-cli", "--mode", "--format", "json", "テスト"};
  EXPECT_EQ(parse(missing_mode).parse_error, "missing value for --mode");

  const char* missing_tag_pos[] = {"suzume-cli", "-f", "tags", "--tag-pos-filter"};
  EXPECT_EQ(parse(missing_tag_pos).parse_error, "missing value for --tag-pos-filter");

  const char* empty_equals[] = {"suzume-cli", "--dictionary=", "テスト"};
  EXPECT_EQ(parse(empty_equals).parse_error, "missing value for --dictionary");
}

TEST(CliCommonTest, ParseArgsRejectsValueOnFlagOption) {
  const char* argv[] = {"suzume-cli", "--merge-compounds=yes", "テスト"};
  EXPECT_EQ(parse(argv).parse_error, "unknown option: --merge-compounds=yes");
}

TEST(CliCommonTest, ParseArgsPreservesDictAndTestSubcommandOptions) {
  const char* dict_argv[] = {"suzume-cli", "dict", "list", "test.tsv", "--pos=NOUN", "--limit=3"};
  auto dict_args = parse(dict_argv);
  EXPECT_EQ(dict_args.command, "dict");
  EXPECT_EQ(dict_args.args, (std::vector<std::string>{"list", "test.tsv", "--pos=NOUN", "--limit=3"}));
  EXPECT_TRUE(dict_args.parse_error.empty());

  const char* test_argv[] = {"suzume-cli", "test", "--file", "cases.tsv", "-d", "user.tsv"};
  auto test_args = parse(test_argv);
  EXPECT_EQ(test_args.command, "test");
  EXPECT_EQ(test_args.args, (std::vector<std::string>{"--file", "cases.tsv"}));
  EXPECT_EQ(test_args.dict_paths, (std::vector<std::string>{"user.tsv"}));
  EXPECT_TRUE(test_args.parse_error.empty());

  const char* expect_argv[] = {"suzume-cli", "test", "-テスト", "--expect", "-テスト"};
  auto expect_args = parse(expect_argv);
  EXPECT_TRUE(expect_args.parse_error.empty()) << expect_args.parse_error;
  EXPECT_EQ(expect_args.args, (std::vector<std::string>{"-テスト", "--expect", "-テスト"}));
}

TEST(CliCommonTest, ParseArgsTreatsTestDashFAsOutputFormat) {
  const char* argv[] = {"suzume-cli", "test", "-f", "cases.tsv"};
  EXPECT_EQ(parse(argv).parse_error, "invalid format: cases.tsv (expected one of: tsv, json, chasen, tags)");
}

TEST(CliCommonTest, ParseArgsKeepsTestOnlyOptionsOutOfAnalyze) {
  const char* argv[] = {"suzume-cli", "--file", "cases.tsv"};
  EXPECT_EQ(parse(argv).parse_error, "unknown option: --file");
}

TEST(CliCommonTest, ParseArgsRecordsVersionWithoutExiting) {
  const char* argv[] = {"suzume-cli", "-v"};
  auto args = parseArgs(static_cast<int>(sizeof(argv) / sizeof(argv[0])), const_cast<char**>(argv));

  EXPECT_TRUE(args.version);
  EXPECT_TRUE(args.parse_error.empty());
}

TEST(CliCommonTest, ParseArgsKeepsHelpOnlyAtTopLevel) {
  const char* argv[] = {"suzume-cli", "--help"};
  auto args = parseArgs(static_cast<int>(sizeof(argv) / sizeof(argv[0])), const_cast<char**>(argv));

  EXPECT_TRUE(args.help);
  EXPECT_TRUE(args.command.empty());
}

TEST(CliCommonTest, ParseArgsKeepsExplicitAnalyzeHelpCommand) {
  const char* argv[] = {"suzume-cli", "analyze", "--help"};
  auto args = parseArgs(static_cast<int>(sizeof(argv) / sizeof(argv[0])), const_cast<char**>(argv));

  EXPECT_TRUE(args.help);
  EXPECT_EQ(args.command, "analyze");
}

TEST(CliCommonTest, ParseArgsDefaultsNoArgumentsToAnalyze) {
  const char* argv[] = {"suzume-cli"};
  auto args = parseArgs(static_cast<int>(sizeof(argv) / sizeof(argv[0])), const_cast<char**>(argv));

  EXPECT_FALSE(args.help);
  EXPECT_EQ(args.command, "analyze");
}

TEST(CliCommonTest, StripUtf8BomRemovesOnlyLeadingBom) {
  std::string value = "\xEF\xBB\xBFtext";
  stripUtf8Bom(&value);
  EXPECT_EQ(value, "text");

  value = "text\xEF\xBB\xBF";
  stripUtf8Bom(&value);
  EXPECT_EQ(value, "text\xEF\xBB\xBF");
}

}  // namespace
}  // namespace suzume::cli
