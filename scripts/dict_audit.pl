#!/usr/bin/env perl
# dict_audit.pl - Check which dictionary entries are actually needed
# Usage: perl scripts/dict_audit.pl [options] <tsv_file>
#
# Options:
#   --section SECTION  Only check entries in the given section
#   --surface SURFACE  Check a single surface
#   --cli PATH         Path to suzume-cli (default: ./build/bin/suzume-cli)
#   --verbose          Show all entries (not just NEED)
#   --recommend        Show recommended action for each entry
#   --summary          Show section-by-section summary
#   --no-dict          Disable L2 dictionary loading (test without dict)

use strict;
use warnings;
use utf8;
use open ':std', ':encoding(UTF-8)';
use Getopt::Long;

my $cli = './build/bin/suzume-cli';
my $section_filter = '';
my $surface_filter = '';
my $verbose = 0;
my $recommend = 0;
my $summary = 0;
my $no_dict = 0;

GetOptions(
    'section=s' => \$section_filter,
    'surface=s' => \$surface_filter,
    'cli=s'     => \$cli,
    'verbose'   => \$verbose,
    'recommend' => \$recommend,
    'summary'   => \$summary,
    'no-dict'   => \$no_dict,
) or die "Usage: $0 [--section SECTION] [--surface SURFACE] [--verbose] [--recommend] [--summary] <tsv_file>\n";

my $tsv_file = $ARGV[0] or die "Usage: $0 <tsv_file>\n";
die "suzume-cli not found at $cli\n" unless -x $cli;

# Parse TSV - handle both old (4-5 col) and new (3 col) formats
open my $fh, '<:encoding(UTF-8)', $tsv_file or die "Cannot open $tsv_file: $!\n";

my $current_section = '';
my @entries;

while (my $line = <$fh>) {
    chomp $line;
    # Section headers: "# === section_name ===" or "# ====...==== \n # SECTION"
    if ($line =~ /^#\s*===\s*(.+?)\s*===/) {
        $current_section = $1;
        next;
    }
    if ($line =~ /^#\s*={3,}/) {
        my $next = <$fh>;
        if ($next && $next =~ /^#\s*(\S+.*)/) {
            $current_section = $1;
            $current_section =~ s/\s*$//;
        }
        next;
    }
    next if $line =~ /^#/ || $line =~ /^\s*$/;

    my @fields = split /\t/, $line;
    next unless @fields >= 2;

    # Handle both 3-col (surface, pos, conj) and 4-5-col (surface, pos, reading, cost, [conj])
    my ($surface, $pos, $conj);
    if (@fields >= 4 && $fields[3] =~ /^[\d.-]+$/) {
        # Old format: surface, pos, reading, cost, [conj_type]
        $surface = $fields[0];
        $pos = $fields[1];
        $conj = $fields[4] // '';
    } else {
        # New format: surface, pos, conj_type
        $surface = $fields[0];
        $pos = $fields[1];
        $conj = $fields[2] // '';
    }

    push @entries, {
        surface => $surface,
        pos     => $pos,
        conj    => $conj,
        section => $current_section,
        line    => $line,
    };
}
close $fh;

# Filter
if ($section_filter) {
    @entries = grep { $_->{section} =~ /$section_filter/i } @entries;
}
if ($surface_filter) {
    @entries = grep { $_->{surface} eq $surface_filter } @entries;
}

printf "Checking %d entries...\n\n", scalar @entries;

my %stats = (ok => 0, need => 0, correct_split => 0);
my (@ok_list, @need_list, @correct_split_list);
my %section_stats;

# Patterns that represent correct grammatical splits (not over-splitting)
my @correct_split_particles = qw(は が を に で と も へ の から まで より って ば);
my %particle_set = map { $_ => 1 } @correct_split_particles;

sub run_suzume {
    my ($text) = @_;
    my $env = $no_dict ? 'SUZUME_NO_L2_DICT=1 ' : '';
    my $output = `$env$cli "$text" 2>/dev/null`;
    my @tokens;
    for my $line (split /\n/, $output) {
        chomp $line;
        next if $line eq '';
        my @fields = split /\t/, $line;
        push @tokens, { surface => $fields[0], pos => $fields[1] // '', lemma => $fields[2] // '' };
    }
    return @tokens;
}

sub tokens_to_str {
    my @tokens = @_;
    return join('|', map { $_->{surface} } @tokens);
}

sub is_correct_split {
    # Check if the split is grammatically correct (e.g., 何度+も, 少し+も, 本当+に)
    my ($surface, @tokens) = @_;
    return 0 unless @tokens >= 2;

    # Check if last token is a particle - this is often a correct split
    my $last = $tokens[-1];
    if ($particle_set{$last->{surface}} && $last->{pos} =~ /PARTICLE|Particle/) {
        # Verify the first part is meaningful
        my $first_surface = join('', map { $_->{surface} } @tokens[0..$#tokens-1]);
        return 1 if length($first_surface) >= 2;  # Non-trivial first part
    }

    return 0;
}

sub get_recommendation {
    my ($entry, $needed, $is_correct_split, @tokens) = @_;
    my $pos = $entry->{pos};
    my $surface = $entry->{surface};

    return 'keep' if $needed && !$is_correct_split;

    if ($is_correct_split) {
        return 'remove (correct split)';
    }

    # NA-adjective recognized as NOUN - keep for POS distinction
    if ($pos eq 'ADJECTIVE' && ($entry->{conj} eq 'NA_ADJ' || $entry->{conj} eq '')) {
        if (@tokens == 1 && $tokens[0]->{pos} =~ /NOUN/) {
            return 'keep (NA-adj POS)';
        }
    }

    # Domain-specific entries → move to user dict
    if ($entry->{section} =~ /wagahai|tongue|idiom/i) {
        return 'move-to-user' if !$needed;
    }

    return 'remove';
}

for my $entry (@entries) {
    my $surface = $entry->{surface};
    my $pos = $entry->{pos};
    my $conj = $entry->{conj};

    # Test 1: base form as single input
    my @tokens = run_suzume($surface);
    my $joined = join('', map { $_->{surface} } @tokens);
    my $split_str = tokens_to_str(@tokens);

    my $base_ok = (@tokens == 1 && $tokens[0]->{surface} eq $surface);

    # For NA-adjectives, also ok if recognized as NOUN (they're noun-like)
    if (!$base_ok && $pos eq 'ADJECTIVE' && ($conj eq 'NA_ADJ' || $conj eq '')) {
        $base_ok = (@tokens == 1 && $tokens[0]->{pos} =~ /NOUN/);
    }

    # Check if it's a "correct split" (grammatically valid, not over-splitting)
    my $correct_split = 0;
    if (!$base_ok) {
        $correct_split = is_correct_split($surface, @tokens);
    }

    # Test 2: conjugated forms for verbs and i-adjectives
    my @conj_problems;

    if ($pos eq 'VERB' && $conj) {
        my @test_forms;
        if ($conj =~ /GODAN_RA/) {
            (my $stem = $surface) =~ s/る$//;
            push @test_forms, "${stem}って" if $stem ne $surface;
        } elsif ($conj =~ /GODAN_WA/) {
            (my $stem = $surface) =~ s/う$//;
            push @test_forms, "${stem}って" if $stem ne $surface;
        } elsif ($conj =~ /GODAN_SA/) {
            (my $stem = $surface) =~ s/す$//;
            push @test_forms, "${stem}して" if $stem ne $surface;
        } elsif ($conj =~ /GODAN_KA/) {
            (my $stem = $surface) =~ s/く$//;
            push @test_forms, "${stem}いて" if $stem ne $surface;
        } elsif ($conj =~ /GODAN_GA/) {
            (my $stem = $surface) =~ s/ぐ$//;
            push @test_forms, "${stem}いで" if $stem ne $surface;
        } elsif ($conj =~ /GODAN_BA/) {
            (my $stem = $surface) =~ s/ぶ$//;
            push @test_forms, "${stem}んで" if $stem ne $surface;
        } elsif ($conj =~ /GODAN_MA/) {
            (my $stem = $surface) =~ s/む$//;
            push @test_forms, "${stem}んで" if $stem ne $surface;
        } elsif ($conj =~ /GODAN_TA/) {
            (my $stem = $surface) =~ s/つ$//;
            push @test_forms, "${stem}って" if $stem ne $surface;
        } elsif ($conj eq 'ICHIDAN') {
            (my $stem = $surface) =~ s/る$//;
            push @test_forms, "${stem}て" if $stem ne $surface;
        }
        for my $form (@test_forms) {
            my @t = run_suzume($form);
            my $j = join('', map { $_->{surface} } @t);
            if ($j ne $form) {
                push @conj_problems, "$form→" . tokens_to_str(@t);
            }
        }
    }

    if ($pos eq 'ADJECTIVE' && $conj eq 'I_ADJ') {
        (my $stem = $surface) =~ s/い$//;
        if ($stem ne $surface) {
            for my $suffix ("くて", "かった") {
                my $form = "$stem$suffix";
                my @t = run_suzume($form);
                my $j = join('', map { $_->{surface} } @t);
                if ($j ne $form) {
                    push @conj_problems, "$form→" . tokens_to_str(@t);
                }
            }
        }
    }

    my $needed = !$base_ok || @conj_problems;
    # Correct splits are not truly "needed" - the dict entry forces a worse result
    $needed = 0 if $correct_split && !@conj_problems;

    my $rec = $recommend ? get_recommendation($entry, $needed, $correct_split, @tokens) : '';
    my $section = $entry->{section} || '(none)';

    if ($correct_split && !@conj_problems) {
        $stats{correct_split}++;
        push @correct_split_list, $entry;
        $section_stats{$section}{correct_split}++;
        if ($verbose || $recommend) {
            printf "SPLIT %-20s %-12s %-10s  %s", $surface, $pos, $conj, "split:$split_str";
            printf "  [%s]", $rec if $recommend;
            print "\n";
        }
    } elsif ($needed) {
        $stats{need}++;
        my $reason = $base_ok ? '' : "base:$split_str";
        $reason .= ' ' . join(' ', @conj_problems) if @conj_problems;
        printf "NEED  %-20s %-12s %-10s  %s", $surface, $pos, $conj, $reason;
        printf "  [%s]", $rec if $recommend;
        print "\n";
        push @need_list, $entry;
        $section_stats{$section}{need}++;
    } else {
        $stats{ok}++;
        if ($verbose || $recommend) {
            printf "OK    %-20s %-12s %-10s", $surface, $pos, $conj;
            printf "  [%s]", $rec if $recommend;
            print "\n";
        }
        push @ok_list, $entry;
        $section_stats{$section}{ok}++;
    }
}

print "\n" . "=" x 60 . "\n";
printf "Total: %d  |  OK: %d  |  NEED: %d  |  CORRECT_SPLIT: %d\n",
    scalar @entries, $stats{ok}, $stats{need}, $stats{correct_split};
print "=" x 60 . "\n";

if ($summary) {
    print "\n--- Section Summary ---\n";
    for my $sec (sort keys %section_stats) {
        my $s = $section_stats{$sec};
        printf "  %-30s  OK:%-4d  NEED:%-4d  SPLIT:%-4d\n",
            $sec,
            $s->{ok} // 0,
            $s->{need} // 0,
            $s->{correct_split} // 0;
    }
}

if (@ok_list && !$verbose && !$recommend) {
    print "\n--- OK (can remove) ---\n";
    for my $e (@ok_list) {
        printf "  %s\n", $e->{line};
    }
}

if (@correct_split_list && !$verbose && !$recommend) {
    print "\n--- CORRECT SPLIT (can remove, split is grammatically correct) ---\n";
    for my $e (@correct_split_list) {
        printf "  %s\n", $e->{line};
    }
}
