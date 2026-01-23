#!/usr/bin/env perl
# Manage tokenization test cases
# Usage: ./scripts/test_tool.pl <command> [options]
#
# Commands:
#   show <input>             Compare MeCab vs Suzume with diff classification
#                            Default output shows colored diff and test info
#   update <input>           Update test case to match MeCab/Suzume output
#   update -t <test_id>      Update specific test by ID (e.g., verb_ichidan/5)
#   add -f <file> <input>    Add new test case to file (rejects duplicates)
#   search <pattern>         Search test cases by regex pattern
#   failed                   List failed test inputs from /tmp/test.txt
#   compare <before> <after> Compare two test outputs (improved/regressed)
#   diff-suzume              Analyze failures by category (read-only)
#   diff-mecab               Find tests where expected differs from MeCab
#   needs-suzume-update      Find tests where expected != MeCab+SuzumeRules
#   batch-add -f <tgt> <file> Batch add tests from input file
#   check-coverage <file>    Check which inputs in file have tests
#   suggest-file <input>     Suggest which test file for input
#   replace-pos OLD NEW      Replace POS in all test files (dry-run by default)
#   map-pos SURFACE OLD NEW  Replace POS only for specific surface
#   list                     List all test files with case counts
#   list-pos                 List all POS values used in tests
#   validate-ids             Detect/fix non-ASCII or duplicate IDs
#
# Show command options:
#   -b, --brief        One-line compact output
#   --tsv              TSV output (surface/pos/lemma)
#   --debug            Show Suzume scoring details
#   -j, --json         Include JSON expected output
#
# General options:
#   -f, --file FILE    Target test file (without .json), or 'all' for all files
#   -t, --test-id ID   Test ID (format: file/index or file/id_string)
#   --id ID            Case ID for add command
#   -n, --dry-run      Show changes without applying
#   --apply            Apply changes (batch-add/replace-pos/needs-suzume-update)
#   --use-suzume       Force Suzume output for expected
#   --limit N          Limit output count
#   -v, --verbose      Verbose output
#   -h, --help         Show this help
#
# Suzume rules (design differences from MeCab):
#   1. number+unit: "3人", "100円", "35ページ" (number + counter merged)
#   2. date: "2024年12月23日" (full date as single token)
#   3. nai-adjective: "だらしない", "もったいない" (lexical adjectives)
#   4. slang-adjective: "エモい", "キモい" (pre-processed for MeCab)
#   5. kanji-compound: "開始予定", "経済成長" (consecutive kanji merged)
#   6. katakana-compound: "セットリスト", "データベース" (consecutive katakana merged)
#
# MeCab corrections (automatically applied):
#   - Slang adjectives: replaced with standard adjective for parsing, then restored
#   - Particles misclassified as Noun: corrected to Particle
#
# Expected value source priority:
#   1. MeCab output (default)
#   2. MeCab + Suzume merge rules (when input matches Suzume rules above)
#   3. Suzume output (when --use-suzume is specified)
#
# Examples:
#   ./scripts/test_tool.pl show "食べています"
#   ./scripts/test_tool.pl update "食べています"
#   ./scripts/test_tool.pl add -f noun_general "3人"     # auto: Suzume (number+unit)
#   ./scripts/test_tool.pl add -f adjective_i_basic "だらしない"  # auto: Suzume
#
#   # Coverage check and batch add (auto-detect enabled)
#   ./scripts/test_tool.pl check-coverage /tmp/inputs.txt
#   ./scripts/test_tool.pl batch-add -f basic /tmp/inputs.txt --apply
#
#   # Force Suzume output for all inputs
#   ./scripts/test_tool.pl batch-add -f basic /tmp/inputs.txt --use-suzume --apply
#
#   # Analysis
#   ./scripts/test_tool.pl diff-suzume        # analyze failures
#
#   # POS replacement (all files)
#   ./scripts/test_tool.pl replace-pos Auxiliary Aux_Tense -f all --apply
#   ./scripts/test_tool.pl map-pos た Auxiliary Aux_Tense --apply
#
#   # Analysis (merged from diff_analysis.pl)
#   ./scripts/test_tool.pl analyze "食べています"       # MeCab vs Suzume diff
#   ./scripts/test_tool.pl analyze "食べています" -d    # with scoring details
#   ./scripts/test_tool.pl analyze -a                  # all failures
#
#   # Test comparison (merged from compare_tests.pl)
#   ./scripts/test_tool.pl compare /tmp/before.txt /tmp/after.txt
#
#   # Failed inputs (merged from failed_inputs.sh)
#   ./scripts/test_tool.pl failed                      # list failed inputs
#   ./scripts/test_tool.pl failed -v                   # with test IDs

use strict;
use warnings;
use utf8;
use Getopt::Long;
use JSON::PP;
use File::Basename;
use FindBin qw($RealBin);
use lib "$RealBin/lib";
use SuzumeUtils qw(
    get_mecab_tokens
    get_mecab_surfaces
    get_expected_tokens
    get_suzume_rule
    get_suzume_surfaces
    get_suzume_debug_info
    apply_suzume_merge
    normalize_pos
    tokens_match
    format_expected
    print_tokens
    load_json
    save_json
    %POS_MAP
);
use TestFileUtils qw(
    find_test_by_input
    find_test_by_id
    get_test_files
    get_failures_from_test_output
    generate_id
    get_test_data_dir
);

binmode(STDOUT, ':utf8');
binmode(STDERR, ':utf8');

# ANSI color codes (auto-disable if not a terminal)
my $USE_COLOR = -t STDOUT;
my $RED    = $USE_COLOR ? "\e[31m" : '';
my $GREEN  = $USE_COLOR ? "\e[32m" : '';
my $YELLOW = $USE_COLOR ? "\e[33m" : '';
my $CYAN   = $USE_COLOR ? "\e[36m" : '';
my $BOLD   = $USE_COLOR ? "\e[1m"  : '';
my $RESET  = $USE_COLOR ? "\e[0m"  : '';

my $test_file = '';
my $test_id = '';
my $description = '';
my $tags_str = '';
my $dry_run = 0;
my $apply = 0;
my $help = 0;
my $use_suzume = 0;
my $case_id = '';
my $limit = 0;  # 0 = no limit
my $compare = 0;
my $analyze_all = 0;
my $show_debug = 0;
my $json_output = 0;
my $verbose = 0;
my $run_mecab = 0;
my $run_cli = 0;
my $brief = 0;
my $stats_only = 0;
my $grep_pattern = '';
my $tsv_output = 0;

GetOptions(
    'f|file=s'        => \$test_file,
    't|test-id=s'     => \$test_id,
    'd|description=s' => \$description,
    'id=s'            => \$case_id,
    'tags=s'          => \$tags_str,
    'n|dry-run'       => \$dry_run,
    'apply'           => \$apply,
    'use-suzume'      => \$use_suzume,
    'limit=i'         => \$limit,
    'c|compare'       => \$compare,
    'a|all'           => \$analyze_all,
    'debug'           => \$show_debug,
    'j|json'          => \$json_output,
    'v|verbose'       => \$verbose,
    'm|mecab'         => \$run_mecab,
    'cli'             => \$run_cli,
    'b|brief'         => \$brief,
    'stats'           => \$stats_only,
    'grep=s'          => \$grep_pattern,
    'tsv'             => \$tsv_output,
    'h|help'          => \$help,
) or die "Error in command line arguments\n";

my $command = shift @ARGV // '';
my $input = shift @ARGV // '';

# Decode UTF-8 in command line arguments
utf8::decode($input);
utf8::decode($test_file);
utf8::decode($description);

if ($help || !$command) {
    print_help();
    exit 0;
}

# Find project root
my $script_dir = dirname($0);
my $project_root = "$script_dir/..";
my $test_data_dir = get_test_data_dir();

sub print_help {
    print <<'HELP';
Usage: test_tool.pl <command> [options] [input]

Commands:
  show <input>           Compare MeCab vs Suzume with diff classification
                         Default: colored diff + test info
  update <input>         Find and update test case matching input
  update -t <test_id>    Update specific test by ID
  add -f <file> <input>  Add new test case to file
  search <pattern>       Search test cases by regex pattern
  failed                 List failed test inputs from /tmp/test.txt
  compare <before> <after>  Compare two test outputs (improved/regressed)
  diff-suzume            Analyze failures by category (read-only)
  diff-mecab             Find tests not MeCab-compatible
  needs-suzume-update    Find tests where expected != MeCab+SuzumeRules
  batch-add -f <tgt> <file>  Batch add tests from file
  check-coverage <file>  Check which inputs in file have tests
  suggest-file <input>   Suggest which test file for input
  replace-pos OLD NEW    Replace POS in all test files
  map-pos SURFACE OLD NEW Replace POS for specific surface only
  list                   List all test files with case counts
  list-pos               List all POS values used in tests
  validate-ids           Detect/fix non-ASCII or duplicate IDs

Show options:
  -b, --brief            One-line compact output
  --tsv                  TSV output (surface/pos/lemma)
  --debug                Show Suzume scoring details
  -j, --json             Include JSON expected output

General options:
  -f, --file FILE        Target test file (without .json), or 'all'
  -t, --test-id ID       Test ID (format: file/index or file/id_string)
  --id ID                Case ID for add command
  -n, --dry-run          Show changes without applying
  --apply                Apply changes (batch-add/replace-pos/map-pos)
  --use-suzume           Force Suzume output for expected
  --limit N              Limit output count
  -v, --verbose          Verbose output
  -h, --help             Show this help

Examples:
  ./scripts/test_tool.pl show "食べています"        # MeCab/Suzume比較
  ./scripts/test_tool.pl show "3人" --tsv           # TSV出力
  ./scripts/test_tool.pl show "食べたい" --debug    # スコアリング情報
  ./scripts/test_tool.pl search "食べ"              # テスト検索
  ./scripts/test_tool.pl update "食べています"
  ./scripts/test_tool.pl add -f verb_ichidan "食べる"
  ./scripts/test_tool.pl failed                     # 失敗リスト
HELP
}

# Wrapper for get_expected_tokens with force_suzume support
sub get_expected_tokens_with_cli {
    my ($text, $force_suzume, $suzume_cli) = @_;

    if ($force_suzume && -x $suzume_cli) {
        return (get_suzume_tokens_full($suzume_cli, $text), 'Suzume', 'forced');
    }

    return get_expected_tokens($text);
}

# Detect segmentation failure pattern by comparing correct vs suzume tokenization
sub detect_segmentation_pattern {
    my ($correct_str, $suzume_str, $input) = @_;

    my @correct = split /\|/, $correct_str;
    my @suzume = split /\|/, $suzume_str;

    # Find what's merged in suzume but split in correct
    # Build position map for correct tokens
    my $pos = 0;
    my @correct_spans;
    for my $tok (@correct) {
        my $len = length($tok);
        push @correct_spans, { token => $tok, start => $pos, end => $pos + $len };
        $pos += $len;
    }

    $pos = 0;
    for my $suz_tok (@suzume) {
        my $suz_len = length($suz_tok);
        my $suz_end = $pos + $suz_len;

        # Find correct tokens that fall within this suzume token
        my @covered = grep {
            $_->{start} >= $pos && $_->{end} <= $suz_end
        } @correct_spans;

        # If suzume covers multiple correct tokens, it's a merge
        if (@covered > 1) {
            my $merged = join('', map { $_->{token} } @covered);

            # Pattern detection based on what should have been split
            return 'くない未分割' if $merged =~ /く[|]?ない$/;
            return 'ん未分割' if $merged =~ /[|]?ん$/ && @covered >= 2;
            return 'て形未分割' if grep { $_->{token} eq 'て' } @covered;
            return 'ている未分割' if $merged =~ /て[|]?(?:い|いる|いた)$/;
            return 'たい未分割' if grep { $_->{token} eq 'たい' } @covered;
            return 'ます未分割' if grep { $_->{token} =~ /^ま[すせし]/ } @covered;
            return 'た/だ未分割' if grep { $_->{token} =~ /^[たっだ]$/ } @covered;
            return 'ない未分割' if grep { $_->{token} eq 'ない' || $_->{token} eq 'なかっ' } @covered;
            return 'れる/られる未分割' if grep { $_->{token} =~ /^[らりれろ]れ/ || $_->{token} =~ /^れ[るた]/ } @covered;
            return 'せる/させる未分割' if grep { $_->{token} =~ /^さ?せ/ } @covered;

            # Generic: show what was merged
            my $pattern = join('+', map { $_->{token} } @covered);
            return "未分割($pattern)";
        }
        $pos = $suz_end;
    }

    # Reverse case: suzume over-splits (less common)
    if (@suzume > @correct) {
        return '過分割';
    }

    return 'その他';
}

sub get_suzume_tokens {
    my ($cli, $text) = @_;
    my $escaped = $text;
    $escaped =~ s/'/'\\''/g;
    my $output = `'$cli' '$escaped' 2>/dev/null`;
    utf8::decode($output);

    my @tokens;
    for my $line (split /\n/, $output) {
        next if $line eq '' || $line eq 'EOS';
        my ($surface, $pos, $lemma) = split /\t/, $line;
        next unless defined $surface && $surface ne '';
        my $norm_pos = normalize_pos($pos // 'Other');
        push @tokens, {
            surface => $surface,
            pos => $norm_pos,
            lemma => $lemma // $surface,
        };
    }
    return \@tokens;
}

# Alias for backward compatibility
sub get_suzume_tokens_full { goto &get_suzume_tokens; }

# Local wrapper for get_test_files with filter support
sub get_test_files_filtered {
    my ($file_filter) = @_;
    if ($file_filter && $file_filter ne 'all') {
        my $path = "$test_data_dir/$file_filter.json";
        return (-f $path) ? ($path) : ();
    }
    return get_test_files();
}

# Command handlers
if ($command eq 'list') {
    opendir my $dh, $test_data_dir or die "Cannot open $test_data_dir: $!\n";
    my @files = sort grep { /\.json$/ } readdir $dh;
    closedir $dh;

    print "Test files in $test_data_dir:\n";
    for my $f (@files) {
        my $basename = $f;
        $basename =~ s/\.json$//;
        my $data = eval { load_json("$test_data_dir/$f") };
        my $count = 0;
        if (!$@) {
            my $cases = $data->{cases} // $data->{test_cases} // [];
            $count = scalar @$cases;
        }
        print "  $basename ($count cases)\n";
    }
    exit 0;
}

if ($command eq 'show') {
    die "Usage: $0 show <input>\n" unless $input;

    my $suzume_cli = "$project_root/build/bin/suzume-cli";

    # Get expected tokens (MeCab + Suzume rules: symbol skip, compound merge, etc.)
    my ($expected_tokens, $source, $rule) = get_expected_tokens($input);
    my @expected_surfaces = map { $_->{surface} } @$expected_tokens;

    # Get Suzume output
    my $suzume_surfaces = -x $suzume_cli ? get_suzume_surfaces($suzume_cli, $input) : [];

    # Check if test exists
    my $found = find_test_by_input($input);

    # Compute diff classification (compare expected vs suzume)
    my $match = (join('|', @expected_surfaces) eq join('|', @$suzume_surfaces));
    my $diff_type = 'match';
    my @diff_indices;
    if (!$match && @$suzume_surfaces) {
        my $e_count = scalar @expected_surfaces;
        my $s_count = scalar @$suzume_surfaces;
        if ($s_count < $e_count) {
            $diff_type = 'under-split';
        } elsif ($s_count > $e_count) {
            $diff_type = 'over-split';
        } else {
            $diff_type = 'boundary';
        }
        my $max = $e_count > $s_count ? $e_count : $s_count;
        for my $i (0 .. $max - 1) {
            push @diff_indices, $i if (($expected_surfaces[$i] // '') ne ($suzume_surfaces->[$i] // ''));
        }
    }

    # TSV output mode (replaces 'expected' command)
    if ($tsv_output) {
        for my $t (@$expected_tokens) {
            my $line = "$t->{surface}\t$t->{pos}";
            $line .= "\t$t->{lemma}" if $t->{lemma} && $t->{lemma} ne $t->{surface};
            print "$line\n";
        }
        exit 0;
    }

    # Brief mode: compact one-line format
    if ($brief) {
        print "Expected: ", join(' ', @expected_surfaces), "\n";
        print "Suzume:   ", join(' ', @$suzume_surfaces), "\n" if @$suzume_surfaces;
        if ($match) {
            print "${GREEN}✓ Match${RESET}";
        } elsif (@$suzume_surfaces) {
            print "${RED}✗ \U$diff_type${RESET}";
        }
        print "  [${CYAN}$found->{basename}/$found->{case}{id}${RESET}]" if $found;
        print "  ${YELLOW}(rule: $rule)${RESET}" if $rule;
        print "\n";
        exit 0;
    }

    # Normal mode
    print "═" x 60, "\n";
    print "${BOLD}Input:${RESET} $input\n";
    print "─" x 60, "\n";

    # Build colored output
    my %diff_set = map { $_ => 1 } @diff_indices;
    my @exp_parts = map { $diff_set{$_} ? "${YELLOW}$expected_surfaces[$_]${RESET}" : $expected_surfaces[$_] } 0 .. $#expected_surfaces;
    my @suz_parts = map { $diff_set{$_} ? "${RED}$suzume_surfaces->[$_]${RESET}" : $suzume_surfaces->[$_] } 0 .. $#$suzume_surfaces;

    print "Expected: ", join(' ', @exp_parts), "\n";
    print "Suzume:   ", (@$suzume_surfaces ? join(' ', @suz_parts) : "${YELLOW}(not available)${RESET}"), "\n";
    print "─" x 60, "\n";

    # Diff classification
    if ($match) {
        print "${GREEN}✓ Suzume matches Expected${RESET}\n";
    } elsif (@$suzume_surfaces) {
        my %type_labels = (
            'under-split' => "${CYAN}[UNDER-SPLIT]${RESET} Suzume merged too much",
            'over-split'  => "${CYAN}[OVER-SPLIT]${RESET} Suzume split too much",
            'boundary'    => "${CYAN}[BOUNDARY]${RESET} Different token boundaries",
        );
        print "${RED}✗ $type_labels{$diff_type}${RESET}\n";
        for my $i (@diff_indices) {
            my $e = $expected_surfaces[$i] // '(none)';
            my $s = $suzume_surfaces->[$i] // '(none)';
            print "  [$i] ${YELLOW}Expected='$e'${RESET} vs ${RED}Suzume='$s'${RESET}\n";
        }
    }

    # Show applied rule if any
    if ($rule) {
        print "\n${CYAN}Rule applied:${RESET} $rule\n";
    }

    # Test info
    print "─" x 60, "\n";
    if ($found) {
        print "${GREEN}✓ Test exists:${RESET} $found->{basename}/$found->{case}{id}\n";
    } else {
        print "${YELLOW}✗ No existing test${RESET}\n";
    }

    # Debug info
    if ($show_debug && -x $suzume_cli) {
        my $debug = get_suzume_debug_info($suzume_cli, $input);
        if ($debug && $debug->{best_path}) {
            print "─" x 60, "\n";
            print "${CYAN}Suzume Scoring:${RESET}\n";
            printf "  Total cost: %.3f  Margin: %.3f\n", $debug->{total_cost}, $debug->{margin};
            print "  Path: $debug->{best_path}\n";
        }
    }

    # JSON output
    if ($json_output) {
        print "─" x 60, "\n";
        my $json = JSON::PP->new->utf8(0)->pretty->canonical;
        print "${BOLD}Expected JSON:${RESET}\n";
        print $json->encode(format_expected($expected_tokens));
    }

    print "\n";
    exit 0;
}

if ($command eq 'update') {
    my $found;
    my $suzume_cli = "$project_root/build/bin/suzume-cli";

    if ($test_id) {
        $found = find_test_by_id($test_id);
        die "Test not found: $test_id\n" unless $found;
        $input = $found->{case}{input};
    } elsif ($input) {
        $found = find_test_by_input($input);
        die "No test found for input: $input\n" unless $found;
    } else {
        die "Usage: $0 update <input> or $0 update -t <test_id>\n";
    }

    my ($tokens, $source, $rule) = get_expected_tokens_with_cli($input, $use_suzume, $suzume_cli);
    my $expected = format_expected($tokens);

    print "Updating: $found->{basename}/$found->{index}\n";
    print "Input: $input\n";
    print "Source: $source", ($rule ? " (auto: $rule)" : ""), "\n\n";

    print "Old expected:\n";
    my $json = JSON::PP->new->utf8(0)->pretty->canonical;
    print $json->encode($found->{case}{expected});

    print "\nNew expected ($source):\n";
    print $json->encode($expected);

    if ($dry_run) {
        print "\n[DRY-RUN] No changes made.\n";
        exit 0;
    }

    # Update the case
    my $cases_key = exists $found->{data}{cases} ? 'cases' : 'test_cases';
    $found->{data}{$cases_key}[$found->{index}]{expected} = $expected;

    save_json($found->{file}, $found->{data});
    print "\n✓ Updated $found->{file}\n";
    exit 0;
}

if ($command eq 'add') {
    die "Usage: $0 add -f <file> <input>\n" unless $test_file && $input;

    my $path = "$test_data_dir/$test_file.json";
    my $suzume_cli = "$project_root/build/bin/suzume-cli";

    # Check if test already exists (reject duplicates)
    my $existing = find_test_by_input($input);
    if ($existing) {
        print STDERR "ERROR: Duplicate input rejected.\n";
        print STDERR "  Input: $input\n";
        print STDERR "  Exists at: $existing->{basename}/$existing->{index}\n";
        print STDERR "  Use 'update' command to modify existing test.\n";
        exit 1;
    }

    my ($tokens, $source, $rule) = get_expected_tokens_with_cli($input, $use_suzume, $suzume_cli);
    my $expected = format_expected($tokens);

    my @tags = $tags_str ? split(/,/, $tags_str) : ();
    my $desc = $description || "$input - auto-generated";
    my $id = $case_id || generate_id($input);

    my $new_case = {
        id => $id,
        description => $desc,
        input => $input,
        expected => $expected,
    };
    $new_case->{tags} = \@tags if @tags;

    print "Adding to: $test_file\n";
    print "Input: $input\n";
    print "Source: $source", ($rule ? " (auto: $rule)" : ""), "\n\n";

    my $json = JSON::PP->new->utf8(0)->pretty->canonical;
    print "New case:\n";
    print $json->encode($new_case);

    if ($dry_run) {
        print "\n[DRY-RUN] No changes made.\n";
        exit 0;
    }

    # Load or create file
    my $data;
    if (-f $path) {
        $data = load_json($path);
    } else {
        $data = {
            version => "1.0",
            description => "$test_file tests",
            cases => [],
        };
    }

    my $cases_key = exists $data->{cases} ? 'cases' : 'test_cases';
    $data->{$cases_key} //= [];
    push @{$data->{$cases_key}}, $new_case;

    save_json($path, $data);
    my $idx = scalar(@{$data->{$cases_key}}) - 1;
    print "\n✓ Added as $test_file/$idx\n";
    exit 0;
}

# DISABLED: sync command is redundant if needs-suzume-update is used to normalize all expected values
# if ($command eq 'sync') {
#     # Sync all cases where Suzume matches corrected expected (MeCab + Suzume rules)
#     my $failures = get_failures_from_test_output();
#     die "No failures found in /tmp/test.txt\n" unless @$failures;
#
#     my $suzume_cli = "$project_root/build/bin/suzume-cli";
#     die "suzume-cli not found\n" unless -x $suzume_cli;
#
#     my @to_sync;
#     print STDERR "Analyzing ", scalar(@$failures), " failures...\n";
#
#     for my $f (@$failures) {
#         my $input = $f->{input};
#         my $id = $f->{id};
#
#         # Get corrected expected (MeCab + Suzume rules) and Suzume outputs
#         my ($correct_tokens, $source, $rule) = get_expected_tokens($input);
#         my $suzume = get_suzume_tokens($suzume_cli, $input);
#
#         # Check if Suzume matches corrected expected
#         if (tokens_match($correct_tokens, $suzume)) {
#             push @to_sync, {
#                 id => $id,
#                 input => $input,
#                 correct => $correct_tokens,
#                 source => $source,
#                 rule => $rule,
#             };
#         }
#     }
#
#     if (!@to_sync) {
#         print "No cases to sync (no failures where Suzume matches expected)\n";
#         exit 0;
#     }
#
#     print "Found ", scalar(@to_sync), " cases where Suzume=Expected but test fails:\n\n";
#
#     for my $case (@to_sync) {
#         my $suffix = $case->{rule} ? " [$case->{source}:$case->{rule}]" : "";
#         print "  [$case->{id}] $case->{input}$suffix\n";
#     }
#
#     if (!$apply) {
#         print "\n[DRY-RUN] Use --apply to make changes.\n";
#         exit 0;
#     }
#
#     print "\nSyncing...\n";
#
#     my $synced = 0;
#     for my $case (@to_sync) {
#         my $found = find_test_by_id($case->{id});
#         next unless $found;
#
#         my $expected = format_expected($case->{correct});
#         my $cases_key = exists $found->{data}{cases} ? 'cases' : 'test_cases';
#         $found->{data}{$cases_key}[$found->{index}]{expected} = $expected;
#
#         save_json($found->{file}, $found->{data});
#         $synced++;
#         print "  ✓ $case->{id}\n";
#     }
#
#     print "\n✓ Synced $synced cases\n";
#     exit 0;
# }

if ($command eq 'diff-suzume') {
    # Show differences between test expectations and suzume output (read-only)
    my $failures = get_failures_from_test_output();
    die "No failures found in /tmp/test.txt\n" unless @$failures;

    my $suzume_cli = "$project_root/build/bin/suzume-cli";
    die "suzume-cli not found\n" unless -x $suzume_cli;

    my $total_failures = scalar(@$failures);
    my $early_stop = $limit > 0;
    # With --limit N, process at most N*5 items to get representative samples
    my $max_process = $early_stop ? $limit * 5 : 0;

    # Suppress progress output in brief/stats/json mode
    my $quiet_mode = $brief || $stats_only || $json_output;
    print STDERR "Analyzing ", ($early_stop ? "up to $max_process of " : ""), "$total_failures failures...\n\n" unless $quiet_mode;

    my %categories = (
        segmentation => [],  # Different token count
        pos_only => [],      # Same tokens, different POS
        matches_correct => [], # Suzume matches corrected expected but not test expected
    );

    my $processed = 0;

    for my $f (@$failures) {
        # Early termination: stop after processing enough items
        last if $early_stop && $processed >= $max_process;

        my $found = find_test_by_id($f->{id});
        next unless $found;

        my $test_expected = $found->{case}{expected} // [];
        # Get corrected expected (MeCab + Suzume rules)
        my ($correct_tokens, $source, $rule) = get_expected_tokens($f->{input});
        my $suzume = get_suzume_tokens($suzume_cli, $f->{input});
        $processed++;

        my @test_surfaces = map { $_->{surface} } @$test_expected;
        my @suz_surfaces = map { $_->{surface} } @$suzume;
        my @cor_surfaces = map { $_->{surface} } @$correct_tokens;

        my $test_str = join('|', @test_surfaces);
        my $suz_str = join('|', @suz_surfaces);
        my $cor_str = join('|', @cor_surfaces);

        my $entry = {
            id => $f->{id},
            input => $f->{input},
            test_expected => $test_str,
            suzume => $suz_str,
            correct => $cor_str,
            source => $source,
            rule => $rule,
        };

        # Check if Suzume matches corrected expected
        if (tokens_match($correct_tokens, $suzume)) {
            push @{$categories{matches_correct}}, $entry;
        } elsif ($cor_str ne $suz_str && @cor_surfaces != @suz_surfaces) {
            push @{$categories{segmentation}}, $entry;
        } else {
            push @{$categories{pos_only}}, $entry;
        }
    }

    print STDERR "Processed $processed of $total_failures failures\n\n" if $early_stop && $processed < $total_failures && !$quiet_mode;

    # Build summary data for early exit modes
    my %summary = (
        total => $total_failures,
        processed => $processed,
        matches_correct => scalar(@{$categories{matches_correct}}),
        segmentation => scalar(@{$categories{segmentation}}),
        pos_only => scalar(@{$categories{pos_only}}),
    );

    # JSON output mode - early exit
    if ($json_output) {
        my $result = {
            summary => \%summary,
            categories => {
                matches_correct => $categories{matches_correct},
                segmentation => $categories{segmentation},
                pos_only => $categories{pos_only},
            },
        };
        my $json = JSON::PP->new->utf8(0)->pretty->canonical;
        print $json->encode($result);
        exit 0;
    }

    # Stats-only mode - early exit
    if ($stats_only) {
        print "total=$summary{total}\n";
        print "processed=$summary{processed}\n";
        print "matches_correct=$summary{matches_correct}\n";
        print "segmentation=$summary{segmentation}\n";
        print "pos_only=$summary{pos_only}\n";
        exit 0;
    }

    # Brief mode - early exit with TSV
    if ($brief) {
        for my $cat (qw(segmentation pos_only matches_correct)) {
            for my $e (@{$categories{$cat}}) {
                print "$cat\t$e->{id}\t$e->{input}\t$e->{correct}\t$e->{suzume}\n";
            }
        }
        exit 0;
    }

    # Helper to limit display count per category
    my $per_cat_limit = $limit > 0 ? $limit : 0;

    # Print categorized results
    if (@{$categories{matches_correct}}) {
        my $total = scalar(@{$categories{matches_correct}});
        print "=== Suzume=Correct ($total) ===\n";
        print "Suzume output matches corrected expected (MeCab + Suzume rules):\n\n";
        my $shown = 0;
        for my $e (@{$categories{matches_correct}}) {
            last if $per_cat_limit && $shown >= $per_cat_limit;
            my $rule_info = $e->{rule} ? " [$e->{source}:$e->{rule}]" : "";
            print "  [$e->{id}] $e->{input}$rule_info\n";
            print "    Test:    $e->{test_expected}\n";
            print "    Suzume:  $e->{suzume} ✓\n";
            print "    Correct: $e->{correct}\n\n" if $e->{test_expected} ne $e->{correct};
            $shown++;
        }
        print "  ... and ", ($total - $shown), " more\n\n" if $per_cat_limit && $shown < $total;
    }

    if (@{$categories{segmentation}}) {
        my $total = scalar(@{$categories{segmentation}});
        print "=== Segmentation differs ($total) ===\n";
        print "These need implementation fixes:\n\n";

        # Group by failure pattern
        my %patterns;
        for my $e (@{$categories{segmentation}}) {
            my $pattern = detect_segmentation_pattern($e->{correct}, $e->{suzume}, $e->{input});
            push @{$patterns{$pattern}}, $e;
        }

        # Sort patterns by count (descending)
        my @sorted_patterns = sort { @{$patterns{$b}} <=> @{$patterns{$a}} } keys %patterns;

        # Aggregate small patterns (count <= 2) into "その他(詳細)"
        my @major_patterns;
        my @minor_entries;
        for my $pattern (@sorted_patterns) {
            my @entries = @{$patterns{$pattern}};
            if (@entries > 2) {
                push @major_patterns, $pattern;
            } else {
                push @minor_entries, @entries;
            }
        }

        for my $pattern (@major_patterns) {
            my @entries = @{$patterns{$pattern}};
            my $count = scalar(@entries);
            my @examples = map { $_->{input} } @entries[0 .. ($count > 3 ? 2 : $#entries)];
            my $example_str = join(', ', @examples);
            $example_str .= ', ...' if $count > 3;
            print "  $pattern: ${count}件 ($example_str)\n";

            # Show details if requested via limit
            if ($per_cat_limit) {
                my $shown = 0;
                for my $e (@entries) {
                    last if $shown >= $per_cat_limit;
                    print "    [$e->{id}] $e->{input}\n";
                    print "      Correct: $e->{correct}\n";
                    print "      Suzume:  $e->{suzume}\n";
                    $shown++;
                }
                print "\n";
            }
        }

        # Show aggregated minor patterns
        if (@minor_entries) {
            my $count = scalar(@minor_entries);
            my @examples = map { $_->{input} } @minor_entries[0 .. ($count > 3 ? 2 : $#minor_entries)];
            my $example_str = join(', ', @examples);
            $example_str .= ', ...' if $count > 3;
            print "  その他(詳細): ${count}件 ($example_str)\n";

            if ($per_cat_limit) {
                my $shown = 0;
                for my $e (@minor_entries) {
                    last if $shown >= $per_cat_limit;
                    print "    [$e->{id}] $e->{input}\n";
                    print "      Correct: $e->{correct}\n";
                    print "      Suzume:  $e->{suzume}\n";
                    $shown++;
                }
                print "\n";
            }
        }

        print "\n" unless $per_cat_limit;
    }

    if (@{$categories{pos_only}}) {
        my $total = scalar(@{$categories{pos_only}});
        print "=== POS-only differences ($total) ===\n";
        print "Same segmentation, different POS:\n\n";
        my $shown = 0;
        for my $e (@{$categories{pos_only}}) {
            last if $per_cat_limit && $shown >= $per_cat_limit;
            print "  [$e->{id}] $e->{input}\n";
            print "    Correct: $e->{correct}\n";
            print "    Suzume:  $e->{suzume}\n\n";
            $shown++;
        }
        print "  ... and ", ($total - $shown), " more\n\n" if $per_cat_limit && $shown < $total;
    }

    # Summary
    print "=== Summary ===\n";
    if ($early_stop && $processed < $total_failures) {
        print "(Based on $processed of $total_failures failures sampled)\n";
    }
    printf "  Suzume=Correct:                %d\n", scalar(@{$categories{matches_correct}});
    printf "  Segmentation issues:           %d\n", scalar(@{$categories{segmentation}});
    printf "  POS-only differences:          %d\n", scalar(@{$categories{pos_only}});
    if ($early_stop && $processed < $total_failures) {
        print "\nNote: Use --limit 0 to analyze all failures.\n";
    }
    exit 0;
}

if ($command eq 'diff-mecab') {
    # Find all test cases where expected differs from MeCab output
    # Categorize: intentional (Suzume rules) vs potential errors
    my @files = get_test_files_filtered($test_file);
    die "No test files found\n" unless @files;

    print STDERR "Scanning ", scalar(@files), " test files...\n\n";

    my %categories = (
        intentional => [],   # Matches get_suzume_rule
        segmentation => [],  # Different segmentation
        pos_only => [],      # Same segmentation, different POS
        lemma_only => [],    # Same segmentation & POS, different lemma
    );

    my $total_cases = 0;
    my $mecab_compatible = 0;

    for my $path (@files) {
        my $data = eval { load_json($path) };
        next if $@;

        my $basename = basename($path);
        $basename =~ s/\.json$//;

        my $cases_key = exists $data->{cases} ? 'cases' : 'test_cases';
        my $cases = $data->{$cases_key} // [];

        for my $i (0 .. $#$cases) {
            my $case = $cases->[$i];
            my $inp = $case->{input} // '';
            next unless $inp;

            $total_cases++;

            my $expected = $case->{expected} // [];
            my $mecab = get_mecab_tokens($inp);

            my @exp_surfaces = map { $_->{surface} } @$expected;
            my @mec_surfaces = map { $_->{surface} } @$mecab;

            my $exp_str = join('|', @exp_surfaces);
            my $mec_str = join('|', @mec_surfaces);

            # Check if they match completely
            if (tokens_match($expected, $mecab)) {
                $mecab_compatible++;
                next;
            }

            my $rule = get_suzume_rule($inp);
            my $case_id = $case->{id} // $i;

            my $entry = {
                id => "$basename/$case_id",
                input => $inp,
                expected => $exp_str,
                mecab => $mec_str,
                rule => $rule,
            };

            # Add POS details for POS-only differences
            if ($exp_str eq $mec_str) {
                # Same segmentation - check POS and lemma
                my @exp_pos = map { $_->{pos} // '' } @$expected;
                my @mec_pos = map { $_->{pos} // '' } @$mecab;
                my $exp_pos_str = join('|', @exp_pos);
                my $mec_pos_str = join('|', @mec_pos);

                if ($exp_pos_str eq $mec_pos_str) {
                    # POS same, must be lemma difference
                    $entry->{expected_full} = join('|', map { "$_->{surface}/$_->{pos}/" . ($_->{lemma}//$_->{surface}) } @$expected);
                    $entry->{mecab_full} = join('|', map { "$_->{surface}/$_->{pos}/" . ($_->{lemma}//$_->{surface}) } @$mecab);
                    if ($rule) {
                        push @{$categories{intentional}}, $entry;
                    } else {
                        push @{$categories{lemma_only}}, $entry;
                    }
                } else {
                    $entry->{expected_pos} = $exp_pos_str;
                    $entry->{mecab_pos} = $mec_pos_str;
                    if ($rule) {
                        push @{$categories{intentional}}, $entry;
                    } else {
                        push @{$categories{pos_only}}, $entry;
                    }
                }
            } else {
                # Segmentation differs
                if ($rule) {
                    push @{$categories{intentional}}, $entry;
                } else {
                    push @{$categories{segmentation}}, $entry;
                }
            }
        }
    }

    # Print results
    my $incompatible = $total_cases - $mecab_compatible;

    if (@{$categories{intentional}}) {
        print "=== Intentional MeCab differences (", scalar(@{$categories{intentional}}), ") ===\n";
        print "These match Suzume design rules (number+unit, nai-adjective, etc.):\n\n";
        for my $e (@{$categories{intentional}}) {
            print "  [$e->{id}] $e->{input}  (rule: $e->{rule})\n";
            print "    Expected: $e->{expected}\n";
            print "    MeCab:    $e->{mecab}\n\n";
        }
    }

    if (@{$categories{segmentation}}) {
        print "=== Segmentation differs (", scalar(@{$categories{segmentation}}), ") ===\n";
        print "Review these - may need test update or implementation fix:\n\n";
        for my $e (@{$categories{segmentation}}) {
            print "  [$e->{id}] $e->{input}\n";
            print "    Expected: $e->{expected}\n";
            print "    MeCab:    $e->{mecab}\n\n";
        }
    }

    if (@{$categories{pos_only}}) {
        print "=== POS-only differences (", scalar(@{$categories{pos_only}}), ") ===\n";
        print "Same segmentation, different POS tagging:\n\n";
        for my $e (@{$categories{pos_only}}) {
            print "  [$e->{id}] $e->{input}\n";
            print "    Expected POS: $e->{expected_pos}\n";
            print "    MeCab POS:    $e->{mecab_pos}\n\n";
        }
    }

    if (@{$categories{lemma_only}}) {
        print "=== Lemma-only differences (", scalar(@{$categories{lemma_only}}), ") ===\n";
        print "Same segmentation and POS, different lemma:\n\n";
        for my $e (@{$categories{lemma_only}}) {
            print "  [$e->{id}] $e->{input}\n";
            print "    Expected: $e->{expected_full}\n";
            print "    MeCab:    $e->{mecab_full}\n\n";
        }
    }

    # Summary
    print "=== Summary ===\n";
    printf "  Total test cases:             %d\n", $total_cases;
    printf "  MeCab-compatible:             %d (%.1f%%)\n", $mecab_compatible, 100.0 * $mecab_compatible / $total_cases;
    printf "  Incompatible:                 %d\n", $incompatible;
    printf "    - Intentional (Suzume rules): %d\n", scalar(@{$categories{intentional}});
    printf "    - Segmentation differs:       %d\n", scalar(@{$categories{segmentation}});
    printf "    - POS-only:                   %d\n", scalar(@{$categories{pos_only}});
    printf "    - Lemma-only:                 %d\n", scalar(@{$categories{lemma_only}});
    exit 0;
}

if ($command eq 'needs-suzume-update') {
    # Find tests where expected doesn't match "MeCab + Suzume rules" (hybrid)
    # Correct expected = MeCab output with Suzume rule patterns merged
    my @files = get_test_files_filtered($test_file);
    die "No test files found\n" unless @files;

    print STDERR "Scanning ", scalar(@files), " test files...\n";
    print STDERR "Comparing expected vs MeCab+SuzumeRules (hybrid)\n\n";

    my @needs_update;
    my %by_rule;

    for my $path (@files) {
        my $data = eval { load_json($path) };
        next if $@;

        my $basename = basename($path);
        $basename =~ s/\.json$//;

        my $cases_key = exists $data->{cases} ? 'cases' : 'test_cases';
        my $cases = $data->{$cases_key} // [];

        for my $i (0 .. $#$cases) {
            my $case = $cases->[$i];
            my $inp = $case->{input} // '';
            next unless $inp;

            # Get current expected
            my $expected = $case->{expected} // [];

            # Get correct expected: MeCab + Suzume rules (using new MeCab POS-based detection)
            my ($correct, $source, $rule) = get_expected_tokens($inp);
            $rule //= '';

            # Compare surface, POS, and lemma (not just surface)
            my $matches = tokens_match($expected, $correct);

            my @exp_surfaces = map { $_->{surface} } @$expected;
            my @cor_surfaces = map { $_->{surface} } @$correct;
            my @exp_pos = map { $_->{pos} // '' } @$expected;
            my @cor_pos = map { $_->{pos} // '' } @$correct;

            my $exp_str = join('|', @exp_surfaces);
            my $cor_str = join('|', @cor_surfaces);
            my $exp_pos_str = join('|', @exp_pos);
            my $cor_pos_str = join('|', @cor_pos);

            # If expected doesn't match correct (surface, POS, or lemma), it needs update
            if (!$matches) {
                my $case_id = $case->{id} // $i;
                my $diff_type = ($exp_str ne $cor_str) ? 'surface' :
                                ($exp_pos_str ne $cor_pos_str) ? 'pos' : 'lemma';
                my $entry = {
                    id => "$basename/$case_id",
                    file => $path,
                    basename => $basename,
                    index => $i,
                    input => $inp,
                    rule => $rule || 'mecab-only',
                    expected => $exp_str,
                    correct => $cor_str,
                    expected_pos => $exp_pos_str,
                    correct_pos => $cor_pos_str,
                    diff_type => $diff_type,
                    correct_tokens => $correct,
                    data => $data,
                    cases_key => $cases_key,
                };
                push @needs_update, $entry;
                push @{$by_rule{$rule || 'mecab-only'}}, $entry;
            }
        }
    }

    if (!@needs_update) {
        print "All test expected values match MeCab+SuzumeRules.\n";
        exit 0;
    }

    print "=== Tests needing update (", scalar(@needs_update), ") ===\n";
    print "Expected should be: MeCab output + Suzume rule corrections\n\n";

    for my $rule (sort keys %by_rule) {
        print "--- $rule (", scalar(@{$by_rule{$rule}}), ") ---\n";
        for my $e (@{$by_rule{$rule}}) {
            print "  [$e->{id}] $e->{input} [$e->{diff_type}]\n";
            if ($e->{diff_type} eq 'surface') {
                print "    Surface: $e->{expected} → $e->{correct}\n";
            } elsif ($e->{diff_type} eq 'pos') {
                print "    POS: $e->{expected_pos} → $e->{correct_pos}\n";
            } else {
                print "    Current:  $e->{expected}\n";
                print "    Correct:  $e->{correct}\n";
            }
            print "\n";
        }
    }

    if (!$apply) {
        print "[DRY-RUN] Use --apply to update these tests.\n";
        exit 0;
    }

    # Apply updates
    print "Updating tests...\n\n";

    my %files_to_save;
    for my $e (@needs_update) {
        my $expected = format_expected($e->{correct_tokens});
        $e->{data}{$e->{cases_key}}[$e->{index}]{expected} = $expected;
        $files_to_save{$e->{file}} = $e->{data};
        print "  ✓ $e->{id}\n";
    }

    for my $path (keys %files_to_save) {
        save_json($path, $files_to_save{$path});
    }

    print "\n✓ Updated ", scalar(@needs_update), " test cases\n";
    exit 0;
}

if ($command eq 'validate-ids') {
    # Detect and fix non-ASCII or duplicate IDs
    my @files = get_test_files_filtered($test_file);
    die "No test files found\n" unless @files;

    print "Validating test IDs in ", scalar(@files), " files...\n\n";

    my @problems;
    my %global_ids;  # Track all IDs to detect cross-file duplicates

    for my $path (@files) {
        my $data = eval { load_json($path) };
        next if $@;

        my $basename = basename($path);
        $basename =~ s/\.json$//;

        my $cases_key = exists $data->{cases} ? 'cases' : 'test_cases';
        my $cases = $data->{$cases_key} // [];
        my %file_ids;

        for my $i (0 .. $#$cases) {
            my $case = $cases->[$i];
            my $id = $case->{id} // '';
            my $input = $case->{input} // '';

            # Check for non-ASCII characters in ID
            my $has_non_ascii = ($id =~ /[^\x00-\x7F]/);

            # Check what the ID would become after sanitization
            my $sanitized = $id;
            $sanitized =~ s/[^a-zA-Z0-9_]/_/g;
            $sanitized =~ s/_+/_/g;
            $sanitized =~ s/^_|_$//g;

            my $becomes_empty = ($sanitized eq '' || $sanitized =~ /^_*$/);
            my $is_duplicate_in_file = exists $file_ids{$sanitized};
            my $is_duplicate_global = exists $global_ids{$sanitized} && $global_ids{$sanitized} ne $path;

            if ($has_non_ascii || $becomes_empty || $is_duplicate_in_file) {
                my $new_id = generate_id($input);
                # Ensure unique within file
                my $suffix = 1;
                my $base_id = $new_id;
                while (exists $file_ids{$new_id}) {
                    $new_id = "${base_id}_${suffix}";
                    $suffix++;
                }

                push @problems, {
                    file => $path,
                    basename => $basename,
                    index => $i,
                    old_id => $id,
                    new_id => $new_id,
                    sanitized => $sanitized,
                    input => $input,
                    reason => $has_non_ascii ? 'non-ASCII' :
                              $becomes_empty ? 'empty after sanitize' :
                              'duplicate in file',
                    data => $data,
                    cases_key => $cases_key,
                };

                $file_ids{$new_id} = 1;
            } else {
                $file_ids{$sanitized} = 1;
            }

            $global_ids{$sanitized} = $path unless $becomes_empty;
        }
    }

    if (!@problems) {
        print "All test IDs are valid.\n";
        exit 0;
    }

    print "=== Invalid IDs found (", scalar(@problems), ") ===\n\n";

    for my $p (@problems) {
        print "  [$p->{basename}/$p->{index}] \"$p->{input}\"\n";
        print "    ID: \"$p->{old_id}\" → \"$p->{new_id}\"\n";
        print "    Reason: $p->{reason}\n\n";
    }

    if (!$apply) {
        print "[DRY-RUN] Use --apply to fix these IDs.\n";
        exit 0;
    }

    # Apply fixes
    print "Fixing IDs...\n\n";

    my %files_to_save;
    for my $p (@problems) {
        $p->{data}{$p->{cases_key}}[$p->{index}]{id} = $p->{new_id};
        $files_to_save{$p->{file}} = $p->{data};
        print "  ✓ $p->{basename}/$p->{index}: \"$p->{old_id}\" → \"$p->{new_id}\"\n";
    }

    for my $path (keys %files_to_save) {
        save_json($path, $files_to_save{$path});
    }

    print "\n✓ Fixed ", scalar(@problems), " IDs\n";
    exit 0;
}

if ($command eq 'replace-pos') {
    # Replace POS in all test files
    my $old_pos = $input;
    my $new_pos = shift @ARGV;
    die "Usage: $0 replace-pos OLD_POS NEW_POS [--apply]\n" unless $old_pos && $new_pos;

    my @files = get_test_files_filtered($test_file);
    die "No test files found\n" unless @files;

    print "Replacing POS: $old_pos -> $new_pos\n";
    print "Files to check: ", scalar(@files), "\n\n";

    my $total_replacements = 0;
    my @changes;

    for my $path (@files) {
        my $data = eval { load_json($path) };
        next if $@;

        my $cases_key = exists $data->{cases} ? 'cases' : 'test_cases';
        my $cases = $data->{$cases_key} // [];
        my $file_changes = 0;

        for my $case (@$cases) {
            for my $token (@{$case->{expected} // []}) {
                if (($token->{pos} // '') eq $old_pos) {
                    push @changes, {
                        file => basename($path),
                        case_id => $case->{id},
                        surface => $token->{surface},
                    };
                    $token->{pos} = $new_pos if $apply;
                    $file_changes++;
                    $total_replacements++;
                }
            }
        }

        if ($apply && $file_changes > 0) {
            save_json($path, $data);
        }
    }

    if ($total_replacements == 0) {
        print "No occurrences of '$old_pos' found.\n";
        exit 0;
    }

    print "Found $total_replacements occurrences:\n";
    for my $c (@changes) {
        print "  [$c->{file}/$c->{case_id}] $c->{surface}\n";
    }

    if (!$apply) {
        print "\n[DRY-RUN] Use --apply to make changes.\n";
    } else {
        print "\n✓ Replaced $total_replacements occurrences\n";
    }
    exit 0;
}

if ($command eq 'map-pos') {
    # Replace POS only for specific surface
    my $surface = $input;
    my $old_pos = shift @ARGV;
    my $new_pos = shift @ARGV;
    die "Usage: $0 map-pos SURFACE OLD_POS NEW_POS [--apply]\n" unless $surface && $old_pos && $new_pos;

    my @files = get_test_files_filtered($test_file);
    die "No test files found\n" unless @files;

    print "Mapping POS for surface '$surface': $old_pos -> $new_pos\n";
    print "Files to check: ", scalar(@files), "\n\n";

    my $total_replacements = 0;
    my @changes;

    for my $path (@files) {
        my $data = eval { load_json($path) };
        next if $@;

        my $cases_key = exists $data->{cases} ? 'cases' : 'test_cases';
        my $cases = $data->{$cases_key} // [];
        my $file_changes = 0;

        for my $case (@$cases) {
            for my $token (@{$case->{expected} // []}) {
                if (($token->{surface} // '') eq $surface &&
                    ($token->{pos} // '') eq $old_pos) {
                    push @changes, {
                        file => basename($path),
                        case_id => $case->{id},
                        input => $case->{input},
                    };
                    $token->{pos} = $new_pos if $apply;
                    $file_changes++;
                    $total_replacements++;
                }
            }
        }

        if ($apply && $file_changes > 0) {
            save_json($path, $data);
        }
    }

    if ($total_replacements == 0) {
        print "No occurrences of surface='$surface' with pos='$old_pos' found.\n";
        exit 0;
    }

    print "Found $total_replacements occurrences:\n";
    for my $c (@changes) {
        print "  [$c->{file}/$c->{case_id}] $c->{input}\n";
    }

    if (!$apply) {
        print "\n[DRY-RUN] Use --apply to make changes.\n";
    } else {
        print "\n✓ Replaced $total_replacements occurrences\n";
    }
    exit 0;
}

if ($command eq 'list-pos') {
    # List all POS values used in tests
    my @files = get_test_files_filtered($test_file);
    die "No test files found\n" unless @files;

    my %pos_counts;
    my %pos_examples;

    for my $path (@files) {
        my $data = eval { load_json($path) };
        next if $@;

        my $cases_key = exists $data->{cases} ? 'cases' : 'test_cases';
        my $cases = $data->{$cases_key} // [];

        for my $case (@$cases) {
            for my $token (@{$case->{expected} // []}) {
                my $pos = $token->{pos} // 'UNKNOWN';
                $pos_counts{$pos}++;
                $pos_examples{$pos} //= [];
                if (@{$pos_examples{$pos}} < 3) {
                    push @{$pos_examples{$pos}}, $token->{surface};
                }
            }
        }
    }

    print "POS values used in test files:\n\n";
    for my $pos (sort { $pos_counts{$b} <=> $pos_counts{$a} } keys %pos_counts) {
        my $examples = join(', ', @{$pos_examples{$pos}});
        printf "  %-20s %5d  (e.g., %s)\n", $pos, $pos_counts{$pos}, $examples;
    }
    exit 0;
}

if ($command eq 'check-coverage') {
    # Check which inputs have existing tests
    my $input_file = $input;
    die "Usage: $0 check-coverage <file_with_inputs>\n" unless $input_file && -f $input_file;

    open my $fh, '<:utf8', $input_file or die "Cannot open $input_file: $!\n";
    my @inputs = grep { /\S/ } map { chomp; $_ } <$fh>;
    close $fh;

    my @missing;
    my @existing;

    for my $inp (@inputs) {
        my $found = find_test_by_input($inp);
        if ($found) {
            push @existing, { input => $inp, location => "$found->{basename}/$found->{index}" };
        } else {
            push @missing, $inp;
        }
    }

    print "=== Coverage Check ===\n\n";

    if (@existing) {
        print "Existing tests (", scalar(@existing), "):\n";
        for my $e (@existing) {
            print "  ✓ $e->{input}  [$e->{location}]\n";
        }
        print "\n";
    }

    if (@missing) {
        print "Missing tests (", scalar(@missing), "):\n";
        for my $m (@missing) {
            print "  ✗ $m\n";
        }
        print "\n";
    }

    print "Summary: ", scalar(@existing), " existing, ", scalar(@missing), " missing\n";
    exit 0;
}

if ($command eq 'batch-add') {
    # Batch add tests from a file
    die "Usage: $0 batch-add -f <target_file> <input_file>\n" unless $test_file && $input;

    my $input_file = $input;
    die "Input file not found: $input_file\n" unless -f $input_file;

    my $suzume_cli = "$project_root/build/bin/suzume-cli";

    open my $fh, '<:utf8', $input_file or die "Cannot open $input_file: $!\n";
    my @inputs = grep { /\S/ } map { chomp; $_ } <$fh>;
    close $fh;

    my $path = "$test_data_dir/$test_file.json";

    my @to_add;
    my @skipped;
    my %source_counts = (MeCab => 0, Suzume => 0);

    for my $inp (@inputs) {
        # Check if test already exists
        my $existing = find_test_by_input($inp);
        if ($existing) {
            push @skipped, { input => $inp, reason => "exists at $existing->{basename}/$existing->{index}" };
            next;
        }

        my ($tokens, $source, $rule) = get_expected_tokens_with_cli($inp, $use_suzume, $suzume_cli);
        my $expected = format_expected($tokens);
        my $id = generate_id($inp);

        $source_counts{$source}++;
        push @to_add, {
            input => $inp,
            id => $id,
            expected => $expected,
            source => $source,
            rule => $rule,
        };
    }

    print "=== Batch Add Preview ===\n\n";
    print "Target file: $test_file.json\n";
    print "Sources: MeCab=$source_counts{MeCab}, Suzume=$source_counts{Suzume}";
    print " (auto-detect enabled)" if -x $suzume_cli;
    print "\n";
    print "Inputs to add: ", scalar(@to_add), "\n";
    print "Skipped (already exist): ", scalar(@skipped), "\n\n";

    if (@to_add) {
        print "Will add:\n";
        for my $t (@to_add) {
            my @surfaces = map { $_->{surface} } @{$t->{expected}};
            my $suffix = $t->{rule} ? " [$t->{source}:$t->{rule}]" : "";
            print "  + $t->{input}  =>  ", join('|', @surfaces), "$suffix\n";
        }
        print "\n";
    }

    if (@skipped) {
        print "Skipped:\n";
        for my $s (@skipped) {
            print "  - $s->{input}  ($s->{reason})\n";
        }
        print "\n";
    }

    if (!$apply) {
        print "[DRY-RUN] Use --apply to add tests.\n";
        exit 0;
    }

    # Load or create file
    my $data;
    if (-f $path) {
        $data = load_json($path);
    } else {
        $data = {
            version => "1.0",
            description => "$test_file tests",
            cases => [],
        };
    }

    my $cases_key = exists $data->{cases} ? 'cases' : 'test_cases';
    $data->{$cases_key} //= [];

    my $added = 0;
    for my $t (@to_add) {
        my $new_case = {
            id => $t->{id},
            description => "$t->{input} - regression test",
            input => $t->{input},
            expected => $t->{expected},
        };
        push @{$data->{$cases_key}}, $new_case;
        $added++;
        print "  ✓ Added: $t->{input}\n";
    }

    save_json($path, $data);
    print "\n✓ Added $added test cases to $test_file.json\n";
    exit 0;
}

if ($command eq 'suggest-file') {
    # Suggest which file an input should go into based on MeCab analysis
    die "Usage: $0 suggest-file <input>\n" unless $input;

    my $tokens = get_mecab_tokens($input);
    return unless @$tokens;

    # Analyze the input to suggest a test file
    my %pos_count;
    for my $t (@$tokens) {
        $pos_count{$t->{pos}}++;
    }

    my @suggestions;

    # Check for specific patterns
    my $first_pos = $tokens->[0]{pos};
    my $has_verb = exists $pos_count{Verb};
    my $has_aux = exists $pos_count{Auxiliary};
    my $has_adj = exists $pos_count{Adjective};
    my $has_particle = exists $pos_count{Particle};

    if ($has_adj && $pos_count{Adjective} >= 1) {
        if ($tokens->[0]{pos} eq 'Adjective') {
            if ($input =~ /そう$/) {
                push @suggestions, 'adjective_i_compound';
            } elsif ($input =~ /く/) {
                push @suggestions, 'adjective_i_ku';
            } elsif ($input =~ /かっ/) {
                push @suggestions, 'adjective_i_katta';
            } else {
                push @suggestions, 'adjective_i_basic';
            }
        }
    }

    if ($has_verb) {
        # Check verb type from first verb
        for my $t (@$tokens) {
            if ($t->{pos} eq 'Verb') {
                my $lemma = $t->{lemma} // '';
                if ($lemma =~ /する$/) {
                    push @suggestions, 'verb_suru';
                } elsif ($lemma =~ /来る|くる$/) {
                    push @suggestions, 'verb_irregular';
                } elsif ($lemma =~ /[いきぎしちにびみり]る$/) {
                    push @suggestions, 'verb_ichidan';
                } else {
                    # Godan - try to determine row
                    if ($lemma =~ /[うくすつぬふむゆる]$/) {
                        push @suggestions, 'verb_godan_misc';
                    }
                }
                last;
            }
        }

        if (grep { $_->{surface} =~ /^(て|で)$/ } @$tokens) {
            push @suggestions, 'verb_te_ta';
        }
    }

    if ($has_aux && !$has_verb && !$has_adj) {
        push @suggestions, 'auxiliary_modality';
    }

    if ($first_pos eq 'Noun') {
        push @suggestions, 'noun_general';
    }

    # Check for specific patterns
    if ($input =~ /(です|ます)/) {
        push @suggestions, 'auxiliary_politeness';
    }

    if ($input =~ /ない$/) {
        push @suggestions, 'auxiliary_negation';
    }

    if ($input =~ /(だ|である)/) {
        push @suggestions, 'copula';
    }

    # Default suggestion
    push @suggestions, 'basic' unless @suggestions;

    print "Input: $input\n";
    print "Tokens: ", join(' | ', map { "$_->{surface}($_->{pos})" } @$tokens), "\n";
    print "Suggested files:\n";
    my %seen;
    for my $s (@suggestions) {
        next if $seen{$s}++;
        print "  - $s\n";
    }
    exit 0;
}

if ($command eq 'compare') {
    # Compare two test outputs (merged from compare_tests.pl)
    my $before_file = $input;
    my $after_file = shift @ARGV // '';

    die "Usage: test_tool.pl compare <before.txt> <after.txt>\n" unless $before_file && $after_file;
    die "File not found: $before_file\n" unless -f $before_file;
    die "File not found: $after_file\n" unless -f $after_file;

    sub extract_failures_from_file {
        my ($file) = @_;
        my %failures;
        open my $fh, '<:utf8', $file or die "Cannot open $file: $!\n";
        my $inp = '';
        while (<$fh>) {
            if (/Input:\s*(.+)/) { $inp = $1; }
            elsif (/FAILED.*Tokenize\/([^,]+)/) {
                $failures{$1} = $inp if $inp ne '';
                $inp = '';
            }
        }
        close $fh;
        return \%failures;
    }

    my $before = extract_failures_from_file($before_file);
    my $after = extract_failures_from_file($after_file);
    my $before_count = scalar keys %$before;
    my $after_count = scalar keys %$after;

    my (@improved, @regressed);
    for my $id (keys %$before) {
        push @improved, { id => $id, input => $before->{$id} } unless exists $after->{$id};
    }
    for my $id (keys %$after) {
        push @regressed, { id => $id, input => $after->{$id} } unless exists $before->{$id};
    }
    @improved = sort { $a->{id} cmp $b->{id} } @improved;
    @regressed = sort { $a->{id} cmp $b->{id} } @regressed;

    my $delta = $before_count - $after_count;
    print STDERR "=== Test Comparison Summary ===\n";
    print STDERR "Before: $before_count failures\n";
    print STDERR "After:  $after_count failures\n";
    print STDERR "---\n";
    print STDERR "Improved:  ", scalar(@improved), " (was failing, now passing)\n";
    print STDERR "Regressed: ", scalar(@regressed), " (was passing, now failing)\n";
    if ($delta > 0) { print STDERR "Net change: -$delta failures (improvement)\n"; }
    elsif ($delta < 0) { print STDERR "Net change: +", (-$delta), " failures (regression)\n"; }
    else { print STDERR "Net change: 0\n"; }
    print STDERR "\n";

    my $suzume_cli = "$project_root/build/bin/suzume-cli";
    for my $item (@improved) {
        print "  [IMPROVED] [$item->{id}] $item->{input}\n";
        if ($run_cli && -x $suzume_cli) {
            my $out = `'$suzume_cli' '$item->{input}' 2>/dev/null`;
            utf8::decode($out); $out =~ s/\n/ /g; $out =~ s/\s+$//;
            print "    suzume: $out\n";
        }
    }
    for my $item (@regressed) {
        print "  [REGRESSED] [$item->{id}] $item->{input}\n";
        if ($run_cli && -x $suzume_cli) {
            my $out = `'$suzume_cli' '$item->{input}' 2>/dev/null`;
            utf8::decode($out); $out =~ s/\n/ /g; $out =~ s/\s+$//;
            print "    suzume: $out\n";
        }
    }
    exit 0;
}

if ($command eq 'failed') {
    # List failed test inputs (merged from failed_inputs.sh)
    my $test_output_file = $input || '/tmp/test.txt';
    die "File not found: $test_output_file\nRun: ctest --test-dir build --output-on-failure > /tmp/test.txt 2>&1\n"
        unless -f $test_output_file;

    open my $fh, '<:utf8', $test_output_file or die "Cannot open $test_output_file: $!\n";
    my @failures;
    my $inp = '';
    while (<$fh>) {
        if (/Input:\s*(.+)/) { $inp = $1; }
        elsif (/FAILED.*Tokenize\/([^,]+)/) {
            push @failures, { id => $1, input => $inp } if $inp ne '';
            $inp = '';
        }
    }
    close $fh;

    if (!@failures) {
        print "No tokenization test failures found.\n";
        exit 0;
    }

    # Apply grep filter if specified
    if ($grep_pattern) {
        @failures = grep { $_->{input} =~ /$grep_pattern/ || $_->{id} =~ /$grep_pattern/ } @failures;
    }

    # Stats-only mode
    if ($stats_only) {
        print scalar(@failures), "\n";
        exit 0;
    }

    # JSON output
    if ($json_output) {
        my $json = JSON::PP->new->utf8(0)->pretty->canonical;
        print $json->encode({ count => scalar(@failures), failures => \@failures });
        exit 0;
    }

    print STDERR "# ", scalar(@failures), " failed tokenization tests\n";

    my $suzume_cli = "$project_root/build/bin/suzume-cli";
    my $count = 0;
    for my $f (@failures) {
        last if $limit > 0 && $count >= $limit;
        if ($verbose) {
            print "[$f->{id}] $f->{input}\n";
        } else {
            print "$f->{input}\n";
        }
        if ($run_cli && -x $suzume_cli) {
            print "  suzume: ";
            my $out = `'$suzume_cli' '$f->{input}' 2>/dev/null`;
            utf8::decode($out); $out =~ s/\n/ /g; $out =~ s/\s+$//;
            print "$out\n";
        }
        if ($run_mecab) {
            print "  MeCab:  ";
            my $out = `echo '$f->{input}' | mecab | grep -v EOS | cut -f1 | tr '\\n' ' '`;
            utf8::decode($out); $out =~ s/\s+$//;
            print "$out\n";
        }
        $count++;
    }
    exit 0;
}

if ($command eq 'search') {
    # Search test cases by regex pattern
    my $pattern = $input;
    die "Usage: test_tool.pl search <pattern>\n" unless $pattern;

    my $regex = eval { qr/$pattern/i };
    die "Invalid regex: $pattern\n$@" if $@;

    my @matches;
    for my $path (get_test_files()) {
        my $data = eval { load_json($path) };
        next if $@;

        my $basename = $path;
        $basename =~ s/.*\///;
        $basename =~ s/\.json$//;

        my $cases = $data->{cases} // $data->{test_cases} // [];
        for my $i (0 .. $#$cases) {
            my $c = $cases->[$i];
            my $id = $c->{id} // $i;
            my $input_text = $c->{input} // '';
            my @surfaces = map { $_->{surface} // '' } @{$c->{expected} // []};
            my $surfaces_str = join(' ', @surfaces);

            # Search in input, surfaces, and ID
            if ($input_text =~ $regex || $surfaces_str =~ $regex || $id =~ $regex) {
                push @matches, {
                    file => $basename,
                    index => $i,
                    id => $id,
                    input => $input_text,
                    expected => $surfaces_str,
                };
            }
        }
    }

    if (@matches == 0) {
        print "No matches found for: $pattern\n";
        exit 0;
    }

    print "Found ", scalar(@matches), " matches for: $pattern\n\n";
    my $count = 0;
    for my $m (@matches) {
        last if $limit > 0 && $count >= $limit;
        print "${BOLD}[$m->{file}/$m->{id}]${RESET}\n";
        print "  Input:    $m->{input}\n";
        print "  Expected: $m->{expected}\n\n";
        $count++;
    }
    exit 0;
}

print STDERR "Unknown command: $command\n";
print_help();
exit 1;
