#!/usr/bin/env perl
# L2 Dictionary helper for adding entries safely
# Usage: ./scripts/dict_add.pl <command> [options]
#
# Commands:
#   check <word>             Check if word can be added (MeCab analysis + duplicates)
#   add <word> <pos> [conj]  Add word to dictionary (with safety checks)
#   validate                 Validate entire dictionary for issues
#   suggest <word>           Suggest POS and conj_type based on MeCab
#
# Options:
#   -f, --force              Add even if MeCab splits the word (requires confirmation)
#   --dry-run                Show what would be added without modifying
#   --fix                    Apply fixes (for validate command)
#   -h, --help               Show this help
#
# POS values: ADJECTIVE, ADVERB, VERB, NOUN, PROPER_NOUN, PRONOUN, PREFIX, SUFFIX, INTERJECTION
# Conjugation types: I_ADJ, NA_ADJ, GODAN_KA/GA/SA/TA/NA/BA/MA/RA/WA, ICHIDAN, IRREGULAR
#
# Examples:
#   ./scripts/dict_add.pl check "Wi-Fi"           # Check if Wi-Fi can be added
#   ./scripts/dict_add.pl suggest "食べる"         # Get POS/conj suggestion
#   ./scripts/dict_add.pl add "Wi-Fi" NOUN        # Add Wi-Fi as noun
#   ./scripts/dict_add.pl add "美味しい" ADJECTIVE I_ADJ  # Add i-adjective
#   ./scripts/dict_add.pl validate                # Check dictionary for issues
#
# Safety checks:
#   - Rejects words that MeCab splits (unless --force)
#   - Rejects words already in dictionary
#   - Rejects conjugated forms (e.g., よくない is よい + ない)
#   - Shows MeCab analysis for verification

use strict;
use warnings;
use utf8;
use Getopt::Long;
use File::Basename;
use Encode qw(decode encode);

binmode(STDOUT, ':utf8');
binmode(STDERR, ':utf8');

# Decode command-line arguments as UTF-8
@ARGV = map { decode('utf-8', $_) } @ARGV;

my $dict_path = 'data/core/dictionary.tsv';
my $force = 0;
my $dry_run = 0;
my $fix = 0;
my $help = 0;

GetOptions(
    'f|force'   => \$force,
    'dry-run'   => \$dry_run,
    'fix'       => \$fix,
    'h|help'    => \$help,
) or die "Error in command line arguments\n";

if ($help || @ARGV == 0) {
    print_help();
    exit 0;
}

my $command = shift @ARGV;

if ($command eq 'check') {
    cmd_check(@ARGV);
} elsif ($command eq 'add') {
    cmd_add(@ARGV);
} elsif ($command eq 'validate') {
    cmd_validate(@ARGV);
} elsif ($command eq 'suggest') {
    cmd_suggest(@ARGV);
} else {
    die "Unknown command: $command\n";
}

sub print_help {
    print <<'HELP';
L2 Dictionary helper for adding entries safely

Usage: ./scripts/dict_add.pl <command> [options]

Commands:
  check <word>             Check if word can be added (MeCab analysis + duplicates)
  add <word> <pos> [conj]  Add word to dictionary (with safety checks)
  validate                 Validate entire dictionary for issues
  suggest <word>           Suggest POS and conj_type based on MeCab

Options:
  -f, --force              Add even if MeCab splits the word (requires confirmation)
  --dry-run                Show what would be added without modifying
  -h, --help               Show this help

POS values: ADJECTIVE, ADVERB, VERB, NOUN, PRONOUN, PREFIX, SUFFIX, INTERJECTION
Conjugation types: I_ADJ, NA_ADJ, GODAN_KA/GA/SA/TA/NA/BA/MA/RA/WA, ICHIDAN, IRREGULAR

Examples:
  ./scripts/dict_add.pl check "Wi-Fi"
  ./scripts/dict_add.pl suggest "食べる"
  ./scripts/dict_add.pl add "Wi-Fi" NOUN
  ./scripts/dict_add.pl add "美味しい" ADJECTIVE I_ADJ
  ./scripts/dict_add.pl validate
HELP
}

# =============================================================================
# MeCab interface
# =============================================================================

sub mecab_analyze {
    my ($word) = @_;
    # Use pipe to avoid shell escaping issues
    open my $mecab, '|-', 'mecab 2>/dev/null > /tmp/mecab_out.txt' or die "Cannot run mecab: $!";
    binmode($mecab, ':utf8');
    print $mecab $word, "\n";
    close $mecab;

    open my $fh, '<:utf8', '/tmp/mecab_out.txt' or die "Cannot read mecab output: $!";
    my $output = do { local $/; <$fh> };
    close $fh;

    my @tokens;
    for my $line (split /\n/, $output) {
        last if $line eq 'EOS';
        next unless $line =~ /\t/;

        my ($surface, $features) = split /\t/, $line, 2;
        my @f = split /,/, $features;

        push @tokens, {
            surface  => $surface,
            pos      => $f[0] // '',
            pos_sub1 => $f[1] // '',
            pos_sub2 => $f[2] // '',
            pos_sub3 => $f[3] // '',
            conj_type=> $f[4] // '',
            conj_form=> $f[5] // '',
            lemma    => $f[6] // $surface,
            reading  => $f[7] // '',
        };
    }
    return @tokens;
}

sub is_base_form {
    my ($token) = @_;
    return ($token->{conj_form} // '') eq '基本形';
}

# Determine if a multi-token result represents a conjugated form that shouldn't be in dictionary
# Returns: reason string if conjugated (should remove), undef if OK to keep
sub is_conjugated_form {
    my ($tokens_ref, $entry) = @_;
    my @tokens = @$tokens_ref;
    my $pos = $entry->{pos} // '';
    my $surface = $entry->{surface} // '';

    # Fixed expressions whitelist (MeCab splits but should be kept as single token)
    my %fixed_expressions = (
        '申し訳ない' => 1,  # 慣用形容詞
        '仕方ない'   => 1,  # 慣用形容詞
        '仕方がない' => 1,  # 慣用形容詞
        '違いない'   => 1,  # 慣用表現
        'やむを得ない' => 1, # 慣用形容詞
        # 追加可能
    );
    return undef if $fixed_expressions{$surface};

    # Skip check for entries intentionally marked as compound or proper nouns
    # (ADVERB often contains fixed expressions that MeCab splits)
    # (PROPER_NOUN for titles/names that may look like conjugated forms)
    return undef if $pos eq 'ADVERB';
    return undef if $pos eq 'INTERJECTION';
    return undef if $pos eq 'CONJUNCTION';
    return undef if $pos eq 'PROPER_NOUN';

    # 2-token patterns
    if (@tokens == 2) {
        my ($t1, $t2) = @tokens;

        # Pattern: 形容詞/動詞 + 助動詞 (e.g., よくない, 食べた)
        # These are conjugated forms and should not be in dictionary
        if ($t2->{pos} eq '助動詞') {
            my $aux = $t2->{surface};
            # Common conjugation auxiliaries
            if ($aux =~ /^(ない|た|だ|です|ます|れる|られる|せる|させる|ぬ|ん)$/) {
                return "verb/adj + $aux (auxiliary)";
            }
        }

        # Pattern: 形容詞連用形 + て (e.g., よくて)
        if ($t2->{surface} eq 'て' && $t1->{conj_form} =~ /連用/) {
            return "renyokei + te";
        }

        # Pattern: 形容詞 + ば (e.g., よければ)
        if ($t2->{surface} eq 'ば' && $t1->{pos} eq '形容詞') {
            return "adjective + ba (conditional)";
        }

        # Pattern: 動詞連用形 + た (e.g., 這入った)
        if ($t2->{surface} eq 'た' && $t1->{conj_form} =~ /連用/) {
            return "renyokei + ta";
        }
    }

    # Not a conjugated form - OK to keep
    return undef;
}

sub map_mecab_pos {
    my ($token) = @_;
    my $pos = $token->{pos};
    my $sub1 = $token->{pos_sub1};

    if ($pos eq '名詞') {
        return 'NOUN';
    } elsif ($pos eq '動詞') {
        return 'VERB';
    } elsif ($pos eq '形容詞') {
        return 'ADJECTIVE';
    } elsif ($pos eq '副詞') {
        return 'ADVERB';
    } elsif ($pos eq '接頭詞') {
        return 'PREFIX';
    } elsif ($pos eq '接尾辞') {
        return 'SUFFIX';
    } elsif ($pos eq '代名詞') {
        return 'PRONOUN';
    } elsif ($pos eq '感動詞') {
        return 'INTERJECTION';
    } elsif ($pos eq '連体詞') {
        return 'ADNOMINAL';
    } elsif ($pos eq '接続詞') {
        return 'CONJUNCTION';
    } else {
        return undef;
    }
}

sub map_conj_type {
    my ($token) = @_;
    my $ctype = $token->{conj_type} // '';

    # Verb conjugation types
    if ($ctype =~ /一段/) {
        return 'ICHIDAN';
    } elsif ($ctype =~ /五段・カ行/) {
        return 'GODAN_KA';
    } elsif ($ctype =~ /五段・ガ行/) {
        return 'GODAN_GA';
    } elsif ($ctype =~ /五段・サ行/) {
        return 'GODAN_SA';
    } elsif ($ctype =~ /五段・タ行/) {
        return 'GODAN_TA';
    } elsif ($ctype =~ /五段・ナ行/) {
        return 'GODAN_NA';
    } elsif ($ctype =~ /五段・バ行/) {
        return 'GODAN_BA';
    } elsif ($ctype =~ /五段・マ行/) {
        return 'GODAN_MA';
    } elsif ($ctype =~ /五段・ラ行/) {
        return 'GODAN_RA';
    } elsif ($ctype =~ /五段・ワ行/) {
        return 'GODAN_WA';
    } elsif ($ctype =~ /サ変/) {
        return 'SURU';
    } elsif ($ctype =~ /カ変/) {
        return 'KURU';
    }

    # Adjective conjugation types
    if ($ctype =~ /形容詞/) {
        return 'I_ADJ';
    }

    return undef;
}

# =============================================================================
# Dictionary operations
# =============================================================================

sub load_dictionary {
    open my $fh, '<:utf8', $dict_path or die "Cannot open $dict_path: $!\n";

    my @entries;
    my %by_surface;

    while (my $line = <$fh>) {
        chomp $line;
        next if $line =~ /^#/ || $line =~ /^\s*$/;

        my @fields = split /\t/, $line;
        my $entry = {
            surface   => $fields[0],
            pos       => $fields[1] // '',
            conj_type => $fields[2] // '',
            line_num  => $.,
            raw       => $line,
        };

        push @entries, $entry;
        push @{$by_surface{$fields[0]}}, $entry;
    }

    close $fh;
    return (\@entries, \%by_surface);
}

sub find_section_for_pos {
    my ($pos) = @_;

    my %sections = (
        'ADJECTIVE'    => 'ADJECTIVES',
        'ADVERB'       => 'ADVERBS',
        'VERB'         => 'VERBS',
        'NOUN'         => 'NOUNS',
        'PROPER_NOUN'  => 'PROPER NOUNS',
        'PRONOUN'      => 'PRONOUNS',
        'PREFIX'       => 'PREFIXES',
        'SUFFIX'       => 'SUFFIXES',
        'INTERJECTION' => 'INTERJECTIONS',
    );

    return $sections{$pos} // 'MISC';
}

# =============================================================================
# Commands
# =============================================================================

sub cmd_check {
    my ($word) = @_;
    die "Usage: dict_add.pl check <word>\n" unless $word;

    print "=" x 60, "\n";
    print "Checking: $word\n";
    print "=" x 60, "\n\n";

    # 1. MeCab analysis
    print "MeCab analysis:\n";
    my @tokens = mecab_analyze($word);

    my $is_single_token = (@tokens == 1);
    my $is_base = $is_single_token && is_base_form($tokens[0]);

    for my $t (@tokens) {
        printf "  %s\t%s,%s\t(%s)\tlemma=%s\n",
            $t->{surface},
            $t->{pos},
            $t->{pos_sub1},
            $t->{conj_form} || '-',
            $t->{lemma};
    }
    print "\n";

    # 2. Token count warning
    if (!$is_single_token) {
        print "⚠️  WARNING: MeCab splits this word into ", scalar(@tokens), " tokens\n";
        print "   This is typically fine for compound words (Wi-Fi, 経済成長)\n";
        print "   but NOT for conjugated forms (よくない = よい + ない)\n\n";

        # Check if it looks like a conjugated form
        if (@tokens == 2) {
            my $t2 = $tokens[1];
            if ($t2->{pos} eq '助動詞' || $t2->{pos} eq '助詞') {
                print "⚠️  CAUTION: Looks like a conjugated form\n";
                print "   '$word' = '$tokens[0]{surface}' + '$t2->{surface}' ($t2->{pos})\n";
                print "   If this is a proper noun (title, name), use PROPER_NOUN\n";
                print "   Otherwise, register the base form '$tokens[0]{lemma}' instead\n\n";
            }
        }
    } else {
        print "✓  Single token\n";
        if ($is_base) {
            print "✓  Base form\n";
        } else {
            print "⚠️  Not base form: $tokens[0]{conj_form}\n";
            print "   Consider registering lemma: $tokens[0]{lemma}\n";
        }
        print "\n";
    }

    # 3. Dictionary check
    my ($entries, $by_surface) = load_dictionary();

    if (exists $by_surface->{$word}) {
        print "❌ DUPLICATE: '$word' already exists in dictionary:\n";
        for my $e (@{$by_surface->{$word}}) {
            printf "   Line %d: %s\t%s\t%s\n",
                $e->{line_num}, $e->{surface}, $e->{pos}, $e->{conj_type};
        }
        print "\n";
        return 0;
    } else {
        print "✓  Not in dictionary (new entry)\n\n";
    }

    # 4. Suggestion
    if ($is_single_token) {
        my $suggested_pos = map_mecab_pos($tokens[0]);
        my $suggested_conj = map_conj_type($tokens[0]);

        if ($suggested_pos) {
            print "Suggested entry:\n";
            my $entry = "$word\t$suggested_pos";
            $entry .= "\t$suggested_conj" if $suggested_conj;
            print "  $entry\n\n";

            print "To add:\n";
            print "  ./scripts/dict_add.pl add \"$word\" $suggested_pos";
            print " $suggested_conj" if $suggested_conj;
            print "\n";
        }
    } elsif (@tokens > 1) {
        print "For compound words:\n";
        print "  ./scripts/dict_add.pl add \"$word\" NOUN --force\n\n";
        print "For proper nouns (titles, names):\n";
        print "  ./scripts/dict_add.pl add \"$word\" PROPER_NOUN\n";
    }

    return 1;
}

sub cmd_suggest {
    my ($word) = @_;
    die "Usage: dict_add.pl suggest <word>\n" unless $word;

    my @tokens = mecab_analyze($word);

    if (@tokens == 0) {
        print "Unknown word (not in MeCab dictionary)\n";
        return;
    }

    if (@tokens > 1) {
        print "MeCab splits '$word' into ", scalar(@tokens), " tokens:\n";
        for my $t (@tokens) {
            print "  $t->{surface}\t$t->{pos},$t->{pos_sub1}\n";
        }
        print "\nIf this is a compound word, use:\n";
        print "  ./scripts/dict_add.pl add \"$word\" NOUN\n";
        return;
    }

    my $t = $tokens[0];
    my $pos = map_mecab_pos($t);
    my $conj = map_conj_type($t);

    print "Word: $word\n";
    print "MeCab POS: $t->{pos},$t->{pos_sub1}\n";
    print "Lemma: $t->{lemma}\n";
    print "Conjugation form: $t->{conj_form}\n\n";

    if ($pos) {
        print "Suggested:\n";
        print "  POS: $pos\n";
        print "  Conj: $conj\n" if $conj;
        print "\nCommand:\n";
        print "  ./scripts/dict_add.pl add \"$word\" $pos";
        print " $conj" if $conj;
        print "\n";
    } else {
        print "Cannot suggest POS for: $t->{pos}\n";
        print "This may be a closed-class word (use L1 instead)\n";
    }
}

sub cmd_add {
    my ($word, $pos, $conj_type) = @_;
    die "Usage: dict_add.pl add <word> <pos> [conj_type]\n" unless $word && $pos;

    # Validate POS
    my @valid_pos = qw(ADJECTIVE ADVERB VERB NOUN PROPER_NOUN PRONOUN PREFIX SUFFIX INTERJECTION ADNOMINAL CONJUNCTION);
    unless (grep { $_ eq $pos } @valid_pos) {
        die "Invalid POS: $pos\nValid values: @valid_pos\n";
    }

    # Validate conj_type if provided
    if ($conj_type) {
        my @valid_conj = qw(I_ADJ NA_ADJ GODAN_KA GODAN_GA GODAN_SA GODAN_TA GODAN_NA GODAN_BA GODAN_MA GODAN_RA GODAN_WA ICHIDAN SURU KURU IRREGULAR);
        unless (grep { $_ eq $conj_type } @valid_conj) {
            die "Invalid conj_type: $conj_type\nValid values: @valid_conj\n";
        }
    }

    # Check for duplicates
    my ($entries, $by_surface) = load_dictionary();
    if (exists $by_surface->{$word}) {
        die "❌ DUPLICATE: '$word' already exists in dictionary\n";
    }

    # MeCab check (skip for proper nouns - titles, names can look like conjugated forms)
    my @tokens = mecab_analyze($word);
    my $skip_conj_check = ($pos eq 'PROPER_NOUN');

    if (@tokens > 1 && !$force && !$skip_conj_check) {
        print "⚠️  MeCab splits '$word' into ", scalar(@tokens), " tokens:\n";
        for my $t (@tokens) {
            print "    $t->{surface}\t$t->{pos}\n";
        }

        # Check for conjugated forms
        if (@tokens == 2 && ($tokens[1]{pos} eq '助動詞' || $tokens[1]{pos} eq '助詞')) {
            die "\n❌ REJECT: This appears to be a conjugated form.\n" .
                "   Register '$tokens[0]{lemma}' instead, or use PROPER_NOUN for titles/names.\n";
        }

        print "\nTo add anyway, use --force\n";
        return 0;
    } elsif (@tokens > 1 && $skip_conj_check) {
        print "ℹ️  Note: MeCab splits '$word' but allowing as PROPER_NOUN\n";
    }

    # Build entry line
    my $entry_line = "$word\t$pos";
    $entry_line .= "\t$conj_type" if $conj_type;

    if ($dry_run) {
        print "Would add to $dict_path:\n";
        print "  $entry_line\n";
        return 1;
    }

    # Find insertion point (at end of appropriate section)
    my $section = find_section_for_pos($pos);
    my $insert_after = find_section_end($section);

    if ($insert_after < 0) {
        # No section found, append to end
        open my $fh, '>>:utf8', $dict_path or die "Cannot open $dict_path: $!\n";
        print $fh "\n$entry_line\n";
        close $fh;
    } else {
        # Insert after section end
        insert_line_at($dict_path, $insert_after, $entry_line);
    }

    print "✓ Added: $entry_line\n";
    print "\nRemember to recompile the dictionary:\n";
    print "  ./build/bin/suzume-cli dict compile data/core/dictionary.tsv data/core.dic\n";

    return 1;
}

sub find_section_end {
    my ($section) = @_;

    open my $fh, '<:utf8', $dict_path or die "Cannot open $dict_path: $!\n";

    my $in_section = 0;
    my $last_entry_line = -1;
    my $line_num = 0;

    while (my $line = <$fh>) {
        $line_num++;
        chomp $line;

        if ($line =~ /^# =+$/ && $in_section) {
            # End of section
            close $fh;
            return $last_entry_line;
        }

        if ($line =~ /^# $section/) {
            $in_section = 1;
            next;
        }

        if ($in_section && $line !~ /^#/ && $line !~ /^\s*$/) {
            $last_entry_line = $line_num;
        }
    }

    close $fh;
    return $last_entry_line;
}

sub insert_line_at {
    my ($file, $after_line, $new_line) = @_;

    open my $in, '<:utf8', $file or die "Cannot open $file: $!\n";
    my @lines = <$in>;
    close $in;

    splice @lines, $after_line, 0, "$new_line\n";

    open my $out, '>:utf8', $file or die "Cannot write $file: $!\n";
    print $out @lines;
    close $out;
}

sub cmd_validate {
    print "=" x 60, "\n";
    print "Validating dictionary: $dict_path\n";
    print "=" x 60, "\n\n";

    my ($entries, $by_surface) = load_dictionary();

    my $issues = 0;
    my @mecab_split;
    my @duplicates;
    my @conjugated_forms;
    my @lines_to_remove;  # Line numbers to remove (1-based)

    # Check each entry
    for my $entry (@$entries) {
        my $word = $entry->{surface};
        my @tokens = mecab_analyze($word);

        # Check for MeCab splits
        if (@tokens > 1) {
            # Check if it's a conjugated form that should be removed
            my $is_conjugated = is_conjugated_form(\@tokens, $entry);
            if ($is_conjugated) {
                push @conjugated_forms, {
                    entry => $entry,
                    tokens => \@tokens,
                    lemma => $tokens[0]{lemma},
                    reason => $is_conjugated,
                };
                push @lines_to_remove, $entry->{line_num};
            } else {
                push @mecab_split, {
                    entry => $entry,
                    tokens => \@tokens,
                };
            }
        }
    }

    # Check for surface duplicates
    for my $surface (keys %$by_surface) {
        my $list = $by_surface->{$surface};
        if (@$list > 1) {
            push @duplicates, {
                surface => $surface,
                entries => $list,
            };
            # Keep first entry, remove the rest
            for my $i (1 .. $#$list) {
                push @lines_to_remove, $list->[$i]{line_num};
            }
        }
    }

    # Report conjugated forms (high priority)
    if (@conjugated_forms) {
        print "❌ CONJUGATED FORMS (should be removed):\n";
        print "   These are generated from base forms by conjugation rules.\n\n";
        for my $issue (@conjugated_forms) {
            printf "   Line %d: %s (%s) → register '%s' instead\n",
                $issue->{entry}{line_num},
                $issue->{entry}{surface},
                $issue->{reason},
                $issue->{lemma};
        }
        print "\n";
        $issues += @conjugated_forms;
    }

    # Report duplicates
    if (@duplicates) {
        print "❌ DUPLICATE SURFACES:\n\n";
        for my $dup (@duplicates) {
            print "   '$dup->{surface}' appears ", scalar(@{$dup->{entries}}), " times:\n";
            for my $e (@{$dup->{entries}}) {
                my $keep = ($e == $dup->{entries}[0]) ? " [keep]" : " [remove]";
                printf "      Line %d: %s\t%s%s\n",
                    $e->{line_num}, $e->{pos}, $e->{conj_type} // '', $keep;
            }
        }
        print "\n";
        $issues += @duplicates;
    }

    # Report MeCab splits (informational for compounds)
    if (@mecab_split) {
        print "ℹ️  COMPOUND WORDS (MeCab splits, but OK for dictionary):\n";
        print "   These are intentionally registered as single tokens.\n\n";
        my $show_count = 0;
        for my $item (@mecab_split) {
            last if $show_count++ >= 10;
            my $tokens_str = join ' | ', map { $_->{surface} } @{$item->{tokens}};
            printf "   Line %d: %s → [%s]\n",
                $item->{entry}{line_num},
                $item->{entry}{surface},
                $tokens_str;
        }
        if (@mecab_split > 10) {
            print "   ... and ", (@mecab_split - 10), " more\n";
        }
        print "\n";
    }

    # Summary
    print "=" x 60, "\n";
    print "Summary:\n";
    print "  Total entries: ", scalar(@$entries), "\n";
    print "  Conjugated forms (remove): ", scalar(@conjugated_forms), "\n";
    print "  Duplicates: ", scalar(@duplicates), "\n";
    print "  Compound words: ", scalar(@mecab_split), " (OK)\n";
    print "=" x 60, "\n";

    # Apply fixes if requested
    if (@lines_to_remove > 0) {
        if ($fix) {
            print "\nApplying fixes...\n";
            remove_lines($dict_path, \@lines_to_remove);
            print "✓ Removed ", scalar(@lines_to_remove), " lines\n";
            print "\nRemember to recompile the dictionary:\n";
            print "  ./build/bin/suzume-cli dict compile data/core/dictionary.tsv data/core.dic\n";
        } else {
            print "\nTo apply fixes, run:\n";
            print "  ./scripts/dict_add.pl validate --fix\n";
            print "\nThis will remove ", scalar(@lines_to_remove), " lines:\n";
            for my $ln (@lines_to_remove) {
                print "  Line $ln\n";
            }
        }
    }

    return $issues == 0;
}

sub remove_lines {
    my ($file, $lines_ref) = @_;
    my %to_remove = map { $_ => 1 } @$lines_ref;

    open my $in, '<:utf8', $file or die "Cannot open $file: $!\n";
    my @lines = <$in>;
    close $in;

    open my $out, '>:utf8', $file or die "Cannot write $file: $!\n";
    my $line_num = 0;
    for my $line (@lines) {
        $line_num++;
        print $out $line unless $to_remove{$line_num};
    }
    close $out;
}
