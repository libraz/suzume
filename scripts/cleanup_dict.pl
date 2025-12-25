#!/usr/bin/perl
# Dictionary cleanup script for suzume
# Usage: perl cleanup_dict.pl <input.tsv> [--dry-run]
#
# Analyzes each dictionary entry and determines if it's necessary.
# Outputs: input_keep.tsv (needed entries) and input_noise.tsv (unnecessary)

use strict;
use warnings;
use utf8;
use open ':std', ':encoding(UTF-8)';

# Path to suzume binary
my $SUZUME = './build/bin/suzume-cli';

# Check arguments
my $input_file = shift or die "Usage: $0 <input.tsv> [--dry-run]\n";
my $dry_run = (shift // '') eq '--dry-run';

die "File not found: $input_file\n" unless -f $input_file;
die "suzume not found: $SUZUME\n" unless -x $SUZUME;

# Output files
my ($base) = $input_file =~ /(.+)\.tsv$/;
$base //= $input_file;
my $keep_file  = "${base}_keep.tsv";
my $noise_file = "${base}_noise.tsv";

# Character type detection (simplified)
sub get_char_types {
    my ($str) = @_;
    my %types;
    for my $char (split //, $str) {
        my $ord = ord($char);
        if ($ord >= 0x3040 && $ord <= 0x309F) {
            $types{hiragana}++;
        } elsif ($ord >= 0x30A0 && $ord <= 0x30FF) {
            $types{katakana}++;
        } elsif ($ord >= 0x4E00 && $ord <= 0x9FFF) {
            $types{kanji}++;
        } elsif ($ord >= 0x0041 && $ord <= 0x007A) {
            $types{alpha}++;
        } elsif ($ord >= 0x0030 && $ord <= 0x0039) {
            $types{digit}++;
        }
    }
    return keys %types;
}

# Check if entry is a fixed expression with particles (の, を, etc.)
# Only matches patterns like: kanji + particle + kanji/kana (鬼滅の刃, 君の名は)
sub is_fixed_expression {
    my ($str) = @_;
    # Pattern: something + の/を/が/は + something (not just single particle in hiragana)
    # Must have non-particle chars on both sides
    return $str =~ /[一-龯ァ-ヶ][のをがはにへとで][一-龯ぁ-んァ-ヶ]/;
}

# Analyze with suzume and check if split
sub is_split_by_suzume {
    my ($surface) = @_;

    # Run suzume (no user dictionary) - each morpheme is on a separate line
    my $cmd = qq{$SUZUME analyze "$surface" 2>/dev/null};
    my $output = `$cmd`;

    # Count non-empty lines (each line = one morpheme)
    my @lines = grep { /\S/ } split /\n/, $output;

    return scalar(@lines) > 1;
}

# Determine if entry should be kept
sub should_keep {
    my ($surface, $pos, $reading) = @_;

    my $len = length($surface);
    my @types = get_char_types($surface);

    # Rule 1: Very short (1-2 chars) -> likely noise
    if ($len <= 2) {
        return (0, "too_short");
    }

    # Rule 2: Check if suzume can handle it (most important check)
    my $is_split = is_split_by_suzume($surface);

    if (!$is_split) {
        # suzume handles it correctly -> not needed
        return (0, "handled_by_suzume");
    }

    # From here, suzume splits the word, so we need to decide if we should keep it

    # Rule 3: Fixed expression with particle (の, を, etc.) -> KEEP (鬼滅の刃, 君の名は)
    if (is_fixed_expression($surface)) {
        return (1, "fixed_expression");
    }

    # Rule 4: Mixed character types and split -> KEEP (compound like Web開発)
    if (@types > 1) {
        return (1, "mixed_chartype_compound");
    }

    # Rule 5: Same char type but still split -> likely idiom/compound
    # Check if it's 4+ kanji (四字熟語 etc.)
    if ($types[0] && $types[0] eq 'kanji' && $len >= 4) {
        return (1, "kanji_compound");
    }

    # Default: split but not critical -> keep for now
    return (1, "split_other");
}

# Process file
open my $in, '<', $input_file or die "Cannot open $input_file: $!\n";

my @keep;
my @noise;
my $line_num = 0;
my $total = 0;
my $kept = 0;

print STDERR "Processing: $input_file\n";

while (my $line = <$in>) {
    $line_num++;
    chomp $line;

    # Skip comments and empty lines
    if ($line =~ /^\s*#/ || $line =~ /^\s*$/) {
        push @keep, $line;  # Preserve comments in keep file
        next;
    }

    # Parse TSV: surface, pos, reading, cost, [conj_type]
    my @fields = split /\t/, $line;
    next unless @fields >= 3;

    my ($surface, $pos, $reading) = @fields[0, 1, 2];
    $total++;

    # Determine if entry should be kept
    my ($keep, $reason) = should_keep($surface, $pos, $reading);

    if ($keep) {
        push @keep, $line;
        $kept++;
        print STDERR "  KEEP: $surface ($reason)\n" if $dry_run;
    } else {
        push @noise, "$line\t# $reason";
        print STDERR "  DROP: $surface ($reason)\n" if $dry_run;
    }

    # Progress
    if ($line_num % 100 == 0) {
        print STDERR "  ... processed $line_num lines\r";
    }
}
close $in;

print STDERR "\n";
print STDERR "Results: $kept / $total entries kept\n";

# Write output files
unless ($dry_run) {
    open my $out_keep, '>', $keep_file or die "Cannot write $keep_file: $!\n";
    print $out_keep "$_\n" for @keep;
    close $out_keep;
    print STDERR "Written: $keep_file\n";

    open my $out_noise, '>', $noise_file or die "Cannot write $noise_file: $!\n";
    print $out_noise "$_\n" for @noise;
    close $out_noise;
    print STDERR "Written: $noise_file\n";
}

print STDERR "Done.\n";
