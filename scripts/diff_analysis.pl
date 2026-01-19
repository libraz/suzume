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
my $help = 0;

GetOptions(
    'i|input=s'   => \$input_str,
    'f|file=s'    => \$test_file,
    't|test-id=s' => \$test_id,
    'a|all'       => \$analyze_all,
    'j|json'      => \$json_output,
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
  -j, --json         Output as JSON (for Claude)
  -h, --help         Show this help

Examples:
  ./scripts/diff_analysis.pl -i "食べています"
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
    my ($input, $test_id, $expected) = @_;

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

    return {
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
    my $result = analyze_input($input_str, '', []);
    push @results, $result;
}
elsif ($test_id) {
    my $input = find_input_for_test($test_id);
    if ($input) {
        my $result = analyze_input($input, $test_id, []);
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
        my $result = analyze_input($f->{input}, $f->{id}, []);
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
