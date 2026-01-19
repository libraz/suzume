#!/usr/bin/env perl
# Compare two test outputs and show improved/regressed test cases
# Usage: ./scripts/compare_tests.pl [options] <before.txt> <after.txt>
#
# Options:
#   -i, --improved   Show only improved (was failing, now passing)
#   -r, --regressed  Show only regressed (was passing, now failing)
#   -u, --unchanged  Show unchanged failures
#   -s, --summary    Show only summary counts
#   -c, --cli        Run suzume-cli on changed inputs
#   -m, --mecab      Run MeCab on changed inputs
#   -h, --help       Show this help

use strict;
use warnings;
use utf8;
use Getopt::Long;
use File::Temp qw(tempfile);

binmode(STDOUT, ':utf8');
binmode(STDERR, ':utf8');

my $show_improved = 1;
my $show_regressed = 1;
my $show_unchanged = 0;
my $summary_only = 0;
my $run_cli = 0;
my $run_mecab = 0;
my $help = 0;

GetOptions(
    'i|improved'  => sub { $show_improved = 1; $show_regressed = 0; },
    'r|regressed' => sub { $show_improved = 0; $show_regressed = 1; },
    'u|unchanged' => \$show_unchanged,
    's|summary'   => \$summary_only,
    'c|cli'       => \$run_cli,
    'm|mecab'     => \$run_mecab,
    'h|help'      => \$help,
) or die "Error in command line arguments\n";

if ($help) {
    print <<'HELP';
Usage: compare_tests.pl [options] <before.txt> <after.txt>

Options:
  -i, --improved   Show only improved (was failing, now passing)
  -r, --regressed  Show only regressed (was passing, now failing)
  -u, --unchanged  Show unchanged failures
  -s, --summary    Show only summary counts
  -c, --cli        Run suzume-cli on changed inputs
  -m, --mecab      Run MeCab on changed inputs
  -h, --help       Show this help

Examples:
  ./scripts/compare_tests.pl /tmp/before.txt /tmp/after.txt
  ./scripts/compare_tests.pl -i /tmp/before.txt /tmp/after.txt  # improved only
  ./scripts/compare_tests.pl -r -c /tmp/before.txt /tmp/after.txt  # regressed with cli
HELP
    exit 0;
}

die "Usage: $0 [options] <before.txt> <after.txt>\n" unless @ARGV >= 2;

my ($before_file, $after_file) = @ARGV;

die "File not found: $before_file\n" unless -f $before_file;
die "File not found: $after_file\n" unless -f $after_file;

sub extract_failures {
    my ($file) = @_;
    my %failures;

    open my $fh, '<:utf8', $file or die "Cannot open $file: $!\n";

    my $input = '';
    while (<$fh>) {
        if (/Input:\s*(.+)/) {
            $input = $1;
        }
        elsif (/FAILED.*Tokenize\/([^,]+)/) {
            my $id = $1;
            if ($input ne '') {
                $failures{$id} = $input;
                $input = '';
            }
        }
    }

    close $fh;
    return \%failures;
}

my $before = extract_failures($before_file);
my $after = extract_failures($after_file);

my $before_count = scalar keys %$before;
my $after_count = scalar keys %$after;

# Categorize
my (@improved, @regressed, @unchanged);

for my $id (keys %$before) {
    if (!exists $after->{$id}) {
        push @improved, { id => $id, input => $before->{$id} };
    } else {
        push @unchanged, { id => $id, input => $before->{$id} };
    }
}

for my $id (keys %$after) {
    if (!exists $before->{$id}) {
        push @regressed, { id => $id, input => $after->{$id} };
    }
}

# Sort by ID
@improved = sort { $a->{id} cmp $b->{id} } @improved;
@regressed = sort { $a->{id} cmp $b->{id} } @regressed;
@unchanged = sort { $a->{id} cmp $b->{id} } @unchanged;

my $improved_count = scalar @improved;
my $regressed_count = scalar @regressed;
my $unchanged_count = scalar @unchanged;
my $delta = $before_count - $after_count;

# Summary
print STDERR "=== Test Comparison Summary ===\n";
print STDERR "Before: $before_count failures\n";
print STDERR "After:  $after_count failures\n";
print STDERR "---\n";
print STDERR "Improved:  $improved_count (was failing, now passing)\n";
print STDERR "Regressed: $regressed_count (was passing, now failing)\n";
print STDERR "Unchanged: $unchanged_count (still failing)\n";

if ($delta > 0) {
    print STDERR "Net change: -$delta failures (improvement)\n";
} elsif ($delta < 0) {
    my $abs = -$delta;
    print STDERR "Net change: +$abs failures (regression)\n";
} else {
    print STDERR "Net change: 0 (no change in failure count)\n";
}
print STDERR "\n";

exit 0 if $summary_only;

sub process_item {
    my ($item) = @_;
    my $id = $item->{id};
    my $input = $item->{input};

    print "  [$id] $input\n";

    if ($run_cli && -x './build/bin/suzume-cli') {
        my $output = `./build/bin/suzume-cli '$input' 2>/dev/null`;
        utf8::decode($output);
        $output =~ s/\n/ /g;
        $output =~ s/\s+$//;
        print "    suzume: $output\n";
    }

    if ($run_mecab && `which mecab 2>/dev/null`) {
        my $output = `echo '$input' | mecab | grep -v EOS | cut -f1 | tr '\\n' ' '`;
        utf8::decode($output);
        $output =~ s/\s+$//;
        print "    MeCab:  $output\n";
    }
}

if ($show_improved && @improved) {
    print "### IMPROVED ($improved_count) ###\n";
    process_item($_) for @improved;
    print "\n";
}

if ($show_regressed && @regressed) {
    print "### REGRESSED ($regressed_count) ###\n";
    process_item($_) for @regressed;
    print "\n";
}

if ($show_unchanged && @unchanged) {
    print "### UNCHANGED ($unchanged_count) ###\n";
    process_item($_) for @unchanged;
    print "\n";
}
