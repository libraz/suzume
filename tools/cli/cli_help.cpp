#include "cli_common.h"

namespace suzume {
namespace cli {

void printHelp(std::ostream& output) {
  output << R"(suzume-cli - Japanese tokenizer

Usage:
  suzume-cli [command] [options] [arguments]

Commands:
  analyze     Tokenize text (default)
  dict        Dictionary management
  test        Verification and testing
  version     Show version information
  help        Show this help

Global Options:
  -d, --dict PATH        Load user dictionary (can specify multiple)
  -m, --mode MODE        Analysis mode: normal, search, split
  -f, --format FMT       Output format: morpheme, tags, json, tsv, chasen
  -V, --verbose          Verbose output
  -VV, --very-verbose    Very verbose output (includes lattice dump)
  --debug                Write lattice diagnostics to stderr; preserves the selected output format
  --no-user-dict         Disable user dictionary
  --no-core-dict         Disable auto-loaded core.dic
  --skip-env-config      Ignore scorer configuration environment variables
  --compare              Write user-dictionary comparison diagnostics to stderr; preserves output format
  --normalize-vu         Normalize ヴ to ビ etc. (default: preserve)
  --lowercase            Convert ASCII to lowercase (default: preserve)
  --preserve-symbols     Keep symbols/emoji in output (default: remove)
  --no-lemmatize         Keep source lemmas without post-analysis correction
  --merge-compounds      Merge consecutive noun compounds
  --include-particles    Include particles in tag output
  --include-auxiliaries  Include auxiliaries in tag output
  --include-formal-nouns Include formal nouns in tag output
  --include-low-info     Include low-information words in tag output
  --tag-keep-duplicates  Keep duplicate tags
  --tag-use-surface      Use surface instead of lemma for tags
  --tag-pos POS          Include noun, verb, adjective, adverb, particle, or auxiliary (repeatable)
  --tag-exclude-basic    Exclude hiragana-only basic words
  --tag-min-length N     Minimum tag length (default: 2)
  --tag-max-tags N       Maximum tags (default: 0, unlimited)
  -h, --help             Show help
  -v, --version          Show version

Long options that take a value also accept --option=value.

Environment:
  SUZUME_DEBUG           Debug level: 1=basic, 2=detailed, 3=trace
  SUZUME_SCORER_CONFIG   Path to a scorer-options JSON file
  SUZUME_SCORER_{SECTION}_{KEY}
                         Override one scorer option; sections are JOIN, SPLIT,
                         UNARY, OPTLEN, BIGRAM, VERB, and INFL

Examples:
  suzume-cli "text"                  Analyze text
  suzume-cli analyze -f json "text"  Analyze with JSON output
  suzume-cli dict compile user.tsv   Compile dictionary
  suzume-cli dict -i user.tsv        Interactive dictionary editor

Use 'suzume-cli [command] --help' for command-specific help.
)";
}

void printAnalyzeHelp(std::ostream& output) {
  output << R"(suzume-cli analyze - Tokenize text

Usage:
  suzume-cli analyze [options] [text]
  suzume-cli [options] [text]         (analyze is default)

Options:
  -d, --dict PATH        Load user dictionary (can specify multiple)
  -m, --mode MODE        Analysis mode: normal, search, split
  -f, --format FMT       Output format: morpheme, tags, json, tsv, chasen
  -V, --verbose          Verbose output
  -VV, --very-verbose    Very verbose output (includes lattice dump)
  --debug                Write lattice diagnostics to stderr; preserves the selected output format
  --no-user-dict         Disable user dictionary
  --no-core-dict         Disable auto-loaded core.dic
  --skip-env-config      Ignore scorer configuration environment variables
  --compare              Write user-dictionary comparison diagnostics to stderr; preserves output format
  --normalize-vu         Normalize ヴ to ビ etc. (default: preserve)
  --lowercase            Convert ASCII to lowercase (default: preserve)
  --preserve-symbols     Keep symbols/emoji in output (default: remove)
  --no-lemmatize         Keep source lemmas without post-analysis correction
  --merge-compounds      Merge consecutive noun compounds
  --include-particles    Include particles in tag output
  --include-auxiliaries  Include auxiliaries in tag output
  --include-formal-nouns Include formal nouns in tag output
  --include-low-info     Include low-information words in tag output
  --tag-keep-duplicates  Keep duplicate tags
  --tag-use-surface      Use surface instead of lemma for tags
  --tag-pos POS          Include noun, verb, adjective, adverb, particle, or auxiliary (repeatable)
  --tag-exclude-basic    Exclude hiragana-only basic words
  --tag-min-length N     Minimum tag length (default: 2)
  --tag-max-tags N       Maximum tags (default: 0, unlimited)
  -h, --help             Show this help

Long options that take a value also accept --option=value.

Output Formats:
  morpheme               surface TAB pos TAB lemma TAB start TAB end (default)
  tags                   tag TAB pos, one per line
  json                   JSON format
  tsv                    Alias of morpheme
  chasen                 ChaSen-like format (Japanese POS, conjugation info)
  TAB formats escape backslash, TAB, CR, and LF as \\, \t, \r, and \n
  Morpheme start/end offsets refer to normalized text; JSON includes it as normalized_text

Examples:
  suzume-cli "text"
  suzume-cli analyze "text"
  suzume-cli analyze -d user.dic "text"
  suzume-cli analyze -f json "text"
  suzume-cli analyze -f chasen "text"
  suzume-cli analyze --compare -d user.dic "text"
  suzume-cli analyze --normalize-vu "ヴァイオリン"
  echo "text" | suzume-cli analyze

Environment:
  SUZUME_DATA_DIR        Data root containing core.dic/user.dic and core/*.tsv
  SUZUME_DEBUG           Debug level: 1=basic, 2=detailed, 3=trace
  SUZUME_SCORER_CONFIG   Path to a scorer-options JSON file
  SUZUME_SCORER_{SECTION}_{KEY}
                         Override one scorer option; sections are JOIN, SPLIT,
                         UNARY, OPTLEN, BIGRAM, VERB, and INFL
)";
}

void printDictHelp(std::ostream& output) {
  output << R"(suzume-cli dict - Dictionary management

Usage:
  suzume-cli dict [subcommand] [options] [arguments]

Subcommands:
  list <file> [--pos=POS] [--pattern=PATTERN] [--limit=N]
                         List entries in TSV or binary dictionary
  search <file> <pattern>
                         Search entries by pattern in file
  lookup <word> [file...] Look up word in L1, core/user sources, and optional dictionaries
  new <file.tsv>         Create new dictionary file
  info <file>            Show dictionary information
  validate <file>        Validate dictionary
  compile <in.tsv> [out.dic]
                         Compile to binary format (default: in.dic)
  compile <in1.tsv> <in2.tsv>... <out.dic>
                         Compile multiple inputs or glob matches into one file
      --filter-trivial   Omit 3+ character pure-kanji/katakana entries and report the count
  decompile <in.dic> [out.tsv]
                         Dump expanded entries to TSV (default: in.dump.tsv).
                         A dump is for inspection, not compiler input.
  -i, --interactive [file.tsv]
                         Interactive mode

Environment:
  SUZUME_DATA_DIR        Data root; source lookup reads core/*.tsv and user/*.tsv below it

POS Values:
  NOUN, PROPN, VERB, ADJECTIVE, ADVERB, PARTICLE, AUXILIARY,
  CONJUNCTION, DETERMINER, PRONOUN, PREFIX, SUFFIX, INTERJECTION,
  SYMBOL, OTHER

Conjugation Types / Name Roles:
  ICHIDAN, GODAN_KA, GODAN_GA, GODAN_SA, GODAN_TA,
  GODAN_NA, GODAN_BA, GODAN_MA, GODAN_RA, GODAN_WA,
  SURU, KURU, I_ADJ, NA_ADJ, FAMILY, GIVEN

Examples:
  suzume-cli dict lookup すぎる
  suzume-cli dict new user.tsv
  suzume-cli dict list user.tsv --pos=NOUN --limit=10
  suzume-cli dict compile user.tsv
  suzume-cli dict -i user.tsv
)";
}

void printTestHelp(std::ostream& output) {
  output << R"(suzume-cli test - Verification and testing

Usage:
  suzume-cli test [subcommand] [options] [arguments]

Subcommands:
  <text> --expect <tags>
                         Test single input with expected output
  -f, --file <tests.tsv>
                         Run tests from file
  benchmark [--iterations=N] [--samples=N] [--warmup=N] [-f <corpus.txt>]
                         Report median initialize, first-analysis, and steady-analysis timing
Options:
  -d, --dict PATH        Load user dictionary
  Analysis, normalization, dictionary, and --tag-* options match `analyze`
  -h, --help             Show this help

Test File Format (TSV):
  input<TAB>expected_tags (comma-separated)

Examples:
  suzume-cli test "text" --expect "tag1,tag2"
  suzume-cli test -f tests.tsv
  suzume-cli test -f tests.tsv -d user.dic
  suzume-cli test benchmark --iterations=1000 --samples=5 --warmup=1
)";
}

}  // namespace cli
}  // namespace suzume
