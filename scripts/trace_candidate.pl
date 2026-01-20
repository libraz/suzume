#!/usr/bin/env perl
# Trace where a specific candidate is generated in Suzume
# Usage: ./scripts/trace_candidate.pl <surface> --input "文"
#
# Examples:
#   ./scripts/trace_candidate.pl "す" --input "天気です"
#   ./scripts/trace_candidate.pl "て" --input "食べています"
#   ./scripts/trace_candidate.pl "食べて" --input "食べています"

use strict;
use warnings;
use utf8;
use Getopt::Long;
use File::Basename;

binmode(STDOUT, ':utf8');
binmode(STDERR, ':utf8');

my $input_str = '';
my $help = 0;

GetOptions(
    'i|input=s' => \$input_str,
    'h|help'    => \$help,
) or die "Error in command line arguments\n";

my $target_surface = shift @ARGV // '';
utf8::decode($target_surface);
utf8::decode($input_str);

if ($help || !$target_surface || !$input_str) {
    print <<'HELP';
Trace candidate generation for a specific surface in Suzume

Usage: trace_candidate.pl <surface> --input "文"

Options:
  -i, --input "文"   Input text to analyze
  -h, --help         Show this help

Examples:
  ./scripts/trace_candidate.pl "す" --input "天気です"
  ./scripts/trace_candidate.pl "て" --input "食べています"
  ./scripts/trace_candidate.pl "食べて" --input "食べています"

Output shows:
  - Source: where the candidate was generated (dict, verb_kanji, char_speech, etc.)
  - Position: character position in input
  - ExtendedPOS: assigned extended POS
  - Cost: word cost (category + edge)
HELP
    exit 0;
}

my $script_dir = dirname($0);
my $project_root = "$script_dir/..";
my $suzume_cli = "$project_root/build/bin/suzume-cli";

die "suzume-cli not found (build first)\n" unless -x $suzume_cli;

# Run with debug output
my $escaped = $input_str;
$escaped =~ s/'/'\\''/g;
my $output = `SUZUME_DEBUG=2 '$suzume_cli' '$escaped' 2>&1`;
utf8::decode($output);

print "═" x 60, "\n";
print "Tracing: \"$target_surface\" in \"$input_str\"\n";
print "═" x 60, "\n\n";

# Parse WORD lines for candidate info
my @candidates;
while ($output =~ /\[WORD\] "([^"]+)" \(([^)]+)\) cost=([-\d.]+) \(from edge\) \[cat=([-\d.]+) edge=([-\d.]+) epos=([^\]]+)\]/g) {
    my ($surface, $source, $cost, $cat, $edge, $epos) = ($1, $2, $3, $4, $5, $6);
    next unless $surface eq $target_surface;
    push @candidates, {
        surface => $surface,
        source => $source,
        cost => $cost,
        cat_cost => $cat,
        edge_cost => $edge,
        epos => $epos,
    };
}

# Parse VITERBI lines for position info
my %viterbi_info;
while ($output =~ /\[VITERBI\] pos=(\d+) "([^"]+)" \((\w+)\/([^)]+)\) \[src:([^\]]+)\] from (\w+) word=([-\d.]+) conn=([-\d.]+) total=([-\d.]+)/g) {
    my ($pos, $surface, $pos_tag, $epos, $src, $from, $word, $conn, $total) = ($1, $2, $3, $4, $5, $6, $7, $8, $9);
    next unless $surface eq $target_surface;
    my $key = "$pos:$src:$epos";
    $viterbi_info{$key} //= {
        pos => $pos,
        surface => $surface,
        pos_tag => $pos_tag,
        epos => $epos,
        source => $src,
        word_cost => $word,
        connections => [],
    };
    push @{$viterbi_info{$key}{connections}}, {
        from => $from,
        conn_cost => $conn,
        total => $total,
    };
}

# Parse generation sources from debug output
my %generation_info;

# VERB_CAND lines
while ($output =~ /\[VERB_CAND\] (\S+) base=(\S+) cost=([-\d.]+) in_dict=(\d) has_suffix=(\d)/g) {
    my ($surface, $base, $cost, $in_dict, $has_suffix) = ($1, $2, $3, $4, $5);
    next unless $surface eq $target_surface;
    $generation_info{verb_cand} //= [];
    push @{$generation_info{verb_cand}}, {
        base => $base,
        cost => $cost,
        in_dict => $in_dict,
        has_suffix => $has_suffix,
    };
}

# TOK_UNK lines
while ($output =~ /\[TOK_UNK\] "([^"]+)" \((\w+)\): \+([-\d.]+) \(([^)]+)\)/g) {
    my ($surface, $pos, $penalty, $reasons) = ($1, $2, $3, $4);
    next unless $surface eq $target_surface;
    $generation_info{tok_unk} //= [];
    push @{$generation_info{tok_unk}}, {
        pos => $pos,
        penalty => $penalty,
        reasons => $reasons,
    };
}

if (!@candidates && !%viterbi_info) {
    print "No candidates found for \"$target_surface\"\n";
    print "\nPossible reasons:\n";
    print "  - Surface not generated as a candidate\n";
    print "  - Check position in input string\n";
    exit 1;
}

# Output candidate info
if (@candidates) {
    print "Candidate Generation:\n";
    print "─" x 40, "\n";
    for my $c (@candidates) {
        print "  Source: $c->{source}\n";
        print "  ExtendedPOS: $c->{epos}\n";
        printf "  Cost: %.3f (cat=%.2f + edge=%.2f)\n", $c->{cost}, $c->{cat_cost}, $c->{edge_cost};
        print "\n";
    }
}

# Output generation details
if (%generation_info) {
    print "Generation Details:\n";
    print "─" x 40, "\n";
    if ($generation_info{verb_cand}) {
        for my $v (@{$generation_info{verb_cand}}) {
            print "  [VERB_CAND] base=$v->{base} cost=$v->{cost}";
            print " in_dict" if $v->{in_dict};
            print " has_suffix" if $v->{has_suffix};
            print "\n";
        }
    }
    if ($generation_info{tok_unk}) {
        for my $u (@{$generation_info{tok_unk}}) {
            print "  [TOK_UNK] pos=$u->{pos} penalty=+$u->{penalty} ($u->{reasons})\n";
        }
    }
    print "\n";
}

# Output Viterbi paths
if (%viterbi_info) {
    print "Viterbi Paths:\n";
    print "─" x 40, "\n";
    for my $key (sort keys %viterbi_info) {
        my $v = $viterbi_info{$key};
        print "  Position: $v->{pos}\n";
        print "  POS: $v->{pos_tag}/$v->{epos}\n";
        print "  Source: $v->{source}\n";
        printf "  Word cost: %.3f\n", $v->{word_cost};
        print "  Connections:\n";
        for my $conn (@{$v->{connections}}) {
            printf "    from %s: conn=%.2f → total=%.3f\n",
                $conn->{from}, $conn->{conn_cost}, $conn->{total};
        }
        print "\n";
    }
}

# Check if in best path
if ($output =~ /\[VITERBI\] Best path.*"$target_surface"/) {
    print "✓ IN BEST PATH\n";
} else {
    print "✗ NOT in best path\n";
}
