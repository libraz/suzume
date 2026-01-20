#!/usr/bin/env perl
# Compare test expectations vs MeCab vs Suzume output
# Usage: ./scripts/diff_analysis.pl [options] [input_or_test_file]
#
# Options:
#   -i, --input "文"   Analyze a single input string
#   -f, --file FILE    Analyze test cases from JSON file
#   -t, --test-id ID   Analyze specific test ID from /tmp/test.txt
#   -a, --all          Analyze all failures from /tmp/test.txt
#   -j, --json         Output as JSON for Claude
#   -h, --help         Show this help
#
# Examples:
#   ./scripts/diff_analysis.pl -i "食べています"
#   ./scripts/diff_analysis.pl -t "basic/0042"
#   ./scripts/diff_analysis.pl -a
#   ./scripts/diff_analysis.pl -a -j > /tmp/analysis.json

use strict;
use warnings;
use utf8;
use Getopt::Long;
use JSON::PP;
use File::Basename;

binmode(STDOUT, ':utf8');
binmode(STDERR, ':utf8');

my $input_str = '';
my $test_file = '';
my $test_id = '';
my $analyze_all = 0;
my $json_output = 0;
my $show_debug = 0;
my $help = 0;

GetOptions(
    'i|input=s'   => \$input_str,
    'f|file=s'    => \$test_file,
    't|test-id=s' => \$test_id,
    'a|all'       => \$analyze_all,
    'j|json'      => \$json_output,
    'd|debug'     => \$show_debug,
    'h|help'      => \$help,
) or die "Error in command line arguments\n";

if ($help || (!$input_str && !$test_file && !$test_id && !$analyze_all)) {
    print <<'HELP';
Usage: diff_analysis.pl [options]

Compare tokenization: Expected vs MeCab vs Suzume

Options:
  -i, --input "文"   Analyze a single input string
  -f, --file FILE    Analyze test cases from JSON file
  -t, --test-id ID   Analyze specific test ID from /tmp/test.txt
  -a, --all          Analyze all failures from /tmp/test.txt
  -d, --debug        Show Suzume scoring details (costs, connections)
  -j, --json         Output as JSON (for Claude)
  -h, --help         Show this help

Examples:
  ./scripts/diff_analysis.pl -i "食べています"
  ./scripts/diff_analysis.pl -i "食べています" -d   # with cost breakdown
  ./scripts/diff_analysis.pl -t "basic/0042"
  ./scripts/diff_analysis.pl -a
  ./scripts/diff_analysis.pl -a -j
HELP
    exit 0;
}

# Find project root
my $script_dir = dirname($0);
my $project_root = "$script_dir/..";
my $suzume_cli = "$project_root/build/bin/suzume-cli";
my $test_data_dir = "$project_root/tests/data/tokenization";

sub get_mecab_tokens {
    my ($text) = @_;
    my $escaped = $text;
    $escaped =~ s/'/'\\''/g;
    my $output = `echo '$escaped' | mecab 2>/dev/null`;
    utf8::decode($output);

    my @tokens;
    for my $line (split /\n/, $output) {
        last if $line eq 'EOS' || $line eq '';
        my ($surface) = split /\t/, $line;
        push @tokens, $surface if defined $surface && $surface ne '';
    }
    return \@tokens;
}

sub get_suzume_tokens {
    my ($text) = @_;
    return [] unless -x $suzume_cli;

    my $escaped = $text;
    $escaped =~ s/'/'\\''/g;
    my $output = `'$suzume_cli' '$escaped' 2>/dev/null`;
    utf8::decode($output);

    # Output is tab-separated: surface\tPOS\tbase_form
    my @tokens;
    for my $line (split /\n/, $output) {
        next if $line eq '' || $line eq 'EOS';
        my ($surface) = split /\t/, $line;
        push @tokens, $surface if defined $surface && $surface ne '';
    }
    return \@tokens;
}

sub get_suzume_debug_info {
    my ($text) = @_;
    return {} unless -x $suzume_cli;

    my $escaped = $text;
    $escaped =~ s/'/'\\''/g;
    my $output = `SUZUME_DEBUG=2 '$suzume_cli' '$escaped' 2>&1`;
    utf8::decode($output);

    my %info = (
        best_path => '',
        total_cost => 0,
        margin => 0,
        tokens => [],
        connections => [],
    );

    # Parse best path line
    if ($output =~ /\[VITERBI\] Best path \(cost=([-\d.]+)\): (.+?) \[margin=([-\d.]+)\]/) {
        $info{total_cost} = $1;
        $info{best_path} = $2;
        $info{margin} = $3;
    }

    # Parse token costs from best path
    # Format: "surface"(POS/EPOS) → ...
    my @path_parts = split / → /, $info{best_path};
    for my $part (@path_parts) {
        if ($part =~ /"(.+?)"\((\w+)\/(\w+)\)/) {
            push @{$info{tokens}}, {
                surface => $1,
                pos => $2,
                epos => $3,
            };
        }
    }

    # Parse CONN lines for connection costs
    while ($output =~ /\[CONN\] "(.+?)" \((\w+)\/\w+\) → "(.+?)" \((\w+)\/\w+\): bigram=([-\d.]+) epos_adj=([-\d.]+) \(([^)]+)\) total=([-\d.]+)/g) {
        push @{$info{connections}}, {
            from_surface => $1,
            from_pos => $2,
            to_surface => $3,
            to_pos => $4,
            bigram => $5,
            epos_adj => $6,
            reason => $7,
            total => $8,
        };
    }

    # Parse WORD lines for word costs
    my @word_costs;
    while ($output =~ /\[WORD\] "(.+?)" \(([^)]+)\) cost=([-\d.]+) \(from edge\) \[cat=([-\d.]+) edge=([-\d.]+) epos=([^\]]+)\]/g) {
        push @word_costs, {
            surface => $1,
            source => $2,
            cost => $3,
            cat_cost => $4,
            edge_cost => $5,
            epos => $6,
        };
    }
    $info{word_costs} = \@word_costs;

    return \%info;
}

sub get_expected_tokens {
    my ($test_id) = @_;

    # Parse test_id like "basic/0042" or "verb/0015"
    my ($category, $num) = split /\//, $test_id;
    return [] unless $category && $num;

    my $json_file = "$test_data_dir/$category.json";
    return [] unless -f $json_file;

    open my $fh, '<:utf8', $json_file or return [];
    my $content = do { local $/; <$fh> };
    close $fh;

    my $data = eval { decode_json($content) };
    return [] if $@;

    my $cases = $data->{test_cases} // $data->{cases} // [];
    my $idx = int($num);

    return [] if $idx >= scalar @$cases;

    my $case = $cases->[$idx];
    my $expected = $case->{expected} // [];

    # Handle both string array and object array
    my @tokens;
    for my $e (@$expected) {
        if (ref $e eq 'HASH') {
            push @tokens, $e->{surface} // $e->{token} // '';
        } else {
            push @tokens, $e;
        }
    }
    return \@tokens;
}

sub find_input_for_test {
    my ($test_id) = @_;

    my ($category, $num) = split /\//, $test_id;
    return '' unless $category && $num;

    my $json_file = "$test_data_dir/$category.json";
    return '' unless -f $json_file;

    open my $fh, '<:utf8', $json_file or return '';
    my $content = do { local $/; <$fh> };
    close $fh;

    my $data = eval { decode_json($content) };
    return '' if $@;

    my $cases = $data->{test_cases} // $data->{cases} // [];
    my $idx = int($num);

    return '' if $idx >= scalar @$cases;
    return $cases->[$idx]{input} // '';
}

sub tokens_match {
    my ($a, $b) = @_;
    return 0 unless @$a == @$b;
    for my $i (0 .. $#$a) {
        return 0 if $a->[$i] ne $b->[$i];
    }
    return 1;
}

sub find_diff_indices {
    my ($base, $other) = @_;
    my @diffs;
    my $max = @$base > @$other ? @$base : @$other;
    for my $i (0 .. $max - 1) {
        my $b = $base->[$i] // '';
        my $o = $other->[$i] // '';
        push @diffs, $i if $b ne $o;
    }
    return \@diffs;
}

sub format_tokens {
    my ($tokens, $diff_indices) = @_;
    my %diff_set = map { $_ => 1 } @{$diff_indices // []};

    my @parts;
    for my $i (0 .. $#$tokens) {
        my $t = $tokens->[$i];
        if ($diff_set{$i}) {
            push @parts, "[$t]";  # Mark differences with brackets
        } else {
            push @parts, $t;
        }
    }
    return join(' ', @parts);
}

sub analyze_input {
    my ($input, $test_id, $expected, $with_debug) = @_;

    $expected //= [];
    my $mecab = get_mecab_tokens($input);
    my $suzume = get_suzume_tokens($input);

    # If no expected provided, try to get from test_id
    if (!@$expected && $test_id) {
        $expected = get_expected_tokens($test_id);
    }

    my $exp_vs_mecab = tokens_match($expected, $mecab);
    my $suzume_vs_mecab = tokens_match($suzume, $mecab);
    my $suzume_vs_exp = tokens_match($suzume, $expected);

    my $mecab_diff = find_diff_indices($mecab, $suzume);
    my $exp_diff = find_diff_indices($expected, $mecab) if @$expected;

    my $result = {
        input => $input,
        test_id => $test_id,
        expected => $expected,
        mecab => $mecab,
        suzume => $suzume,
        matches => {
            expected_vs_mecab => $exp_vs_mecab,
            suzume_vs_mecab => $suzume_vs_mecab,
            suzume_vs_expected => $suzume_vs_exp,
        },
        diff_indices => {
            suzume_vs_mecab => $mecab_diff,
            expected_vs_mecab => $exp_diff // [],
        },
    };

    # Add debug info if requested
    if ($with_debug) {
        $result->{debug} = get_suzume_debug_info($input);
    }

    return $result;
}

sub print_analysis {
    my ($result) = @_;

    my $id_str = $result->{test_id} ? "[$result->{test_id}]" : "";
    print "═" x 60, "\n";
    print "Input: $result->{input} $id_str\n";
    print "─" x 60, "\n";

    my $mecab_str = join(' ', @{$result->{mecab}});
    my $suzume_diff = $result->{diff_indices}{suzume_vs_mecab};
    my $suzume_str = format_tokens($result->{suzume}, $suzume_diff);

    print "MeCab:  $mecab_str\n";
    print "Suzume: $suzume_str\n";

    if (@{$result->{expected}}) {
        my $exp_diff = $result->{diff_indices}{expected_vs_mecab};
        my $exp_str = format_tokens($result->{expected}, $exp_diff);
        print "Expect: $exp_str\n";

        if (!$result->{matches}{expected_vs_mecab}) {
            print "⚠ WARNING: Expected differs from MeCab\n";
        }
    }

    print "─" x 60, "\n";

    if ($result->{matches}{suzume_vs_mecab}) {
        print "✓ Suzume matches MeCab\n";
    } else {
        print "✗ Suzume differs at: ", join(', ', @$suzume_diff), "\n";

        # Show token-by-token comparison at diff positions
        for my $i (@$suzume_diff) {
            my $m = $result->{mecab}[$i] // '(none)';
            my $s = $result->{suzume}[$i] // '(none)';
            print "  [$i] MeCab='$m' vs Suzume='$s'\n";
        }
    }

    # Show debug info if available
    if ($result->{debug} && $result->{debug}{best_path}) {
        my $debug = $result->{debug};
        print "─" x 60, "\n";
        print "Suzume Scoring Details:\n";
        printf "  Total cost: %.3f  Margin: %.3f\n", $debug->{total_cost}, $debug->{margin};
        print "  Path: $debug->{best_path}\n";

        # Show word costs for tokens in best path
        if (@{$debug->{tokens}}) {
            print "\n  Token costs:\n";
            for my $tok (@{$debug->{tokens}}) {
                # Find matching word cost
                my ($wc) = grep { $_->{surface} eq $tok->{surface} } @{$debug->{word_costs}};
                if ($wc) {
                    printf "    \"%s\" (%s): cost=%.2f [cat=%.2f edge=%.2f src=%s]\n",
                        $tok->{surface}, $tok->{epos}, $wc->{cost}, $wc->{cat_cost}, $wc->{edge_cost}, $wc->{source};
                } else {
                    printf "    \"%s\" (%s/%s)\n", $tok->{surface}, $tok->{pos}, $tok->{epos};
                }
            }
        }

        # Show relevant connections
        if (@{$debug->{connections}}) {
            print "\n  Connections:\n";
            my %seen;
            for my $conn (@{$debug->{connections}}) {
                # Only show connections for tokens in best path
                my $key = "$conn->{from_surface}→$conn->{to_surface}";
                next if $seen{$key}++;
                my $in_path = grep { $_->{surface} eq $conn->{from_surface} || $_->{surface} eq $conn->{to_surface} } @{$debug->{tokens}};
                next unless $in_path;
                printf "    \"%s\"→\"%s\": bigram=%.2f epos=%.2f (%s)\n",
                    $conn->{from_surface}, $conn->{to_surface},
                    $conn->{bigram}, $conn->{epos_adj}, $conn->{reason};
            }
        }
    }

    print "\n";
}

sub get_failures_from_test_output {
    my $file = '/tmp/test.txt';
    return [] unless -f $file;

    open my $fh, '<:utf8', $file or return [];

    my @failures;
    my $input = '';

    while (<$fh>) {
        if (/Input:\s*(.+)/) {
            $input = $1;
        }
        elsif (/FAILED.*Tokenize\/([^,]+)/) {
            my $id = $1;
            if ($input ne '') {
                push @failures, { id => $id, input => $input };
                $input = '';
            }
        }
    }

    close $fh;
    return \@failures;
}

# Main logic
my @results;

if ($input_str) {
    my $result = analyze_input($input_str, '', [], $show_debug);
    push @results, $result;
}
elsif ($test_id) {
    my $input = find_input_for_test($test_id);
    if ($input) {
        my $result = analyze_input($input, $test_id, [], $show_debug);
        push @results, $result;
    } else {
        die "Could not find test case: $test_id\n";
    }
}
elsif ($analyze_all) {
    my $failures = get_failures_from_test_output();
    if (!@$failures) {
        print STDERR "No failures found in /tmp/test.txt\n";
        exit 0;
    }

    print STDERR "Analyzing ", scalar(@$failures), " failures...\n\n";

    for my $f (@$failures) {
        # Only show debug for first few when analyzing all (expensive)
        my $with_debug = $show_debug && @results < 5;
        my $result = analyze_input($f->{input}, $f->{id}, [], $with_debug);
        push @results, $result;
    }
}

# Output
if ($json_output) {
    my $json = JSON::PP->new->utf8(0)->pretty->canonical;
    print $json->encode(\@results);
} else {
    for my $r (@results) {
        print_analysis($r);
    }

    # Summary
    if (@results > 1) {
        my $match_count = grep { $_->{matches}{suzume_vs_mecab} } @results;
        my $mismatch_count = @results - $match_count;
        my $exp_mismatch = grep { @{$_->{expected}} && !$_->{matches}{expected_vs_mecab} } @results;

        print "═" x 60, "\n";
        print "SUMMARY: $match_count matches, $mismatch_count differs from MeCab\n";
        print "         $exp_mismatch test expectations differ from MeCab\n" if $exp_mismatch;
    }
}
