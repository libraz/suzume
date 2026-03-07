#!/usr/bin/env perl
# L2 Dictionary helper for adding entries safely
# Usage: ./scripts/dict_tool.pl <command> [options]
#
# Commands:
#   check <word>             Check if word can be added (MeCab analysis + duplicates)
#   add <word> <pos> [conj]  Add word to dictionary (with safety checks)
#   remove <word>            Remove word from dictionary
#   disable <word>           Comment out word (keeps in file, but inactive)
#   enable <word>            Uncomment disabled word
#   validate                 Validate entire dictionary for issues
#   suggest <word>           Suggest POS and conj_type based on MeCab
#   cleanup <input.tsv>      Analyze dictionary, separate needed/unneeded entries
#
# Options:
#   -f, --force              Add even if MeCab splits the word (requires confirmation)
#   --dry-run                Show what would be added without modifying
#   --fix                    Apply fixes (for validate command)
#   -h, --help               Show this help
#
# POS values: ADJECTIVE, ADVERB, VERB, NOUN, PROPER_NOUN, PRONOUN, PREFIX, SUFFIX, INTERJECTION, PARTICLE, AUX, OTHER
# Conjugation types: I_ADJ, NA_ADJ, GODAN_KA/GA/SA/TA/NA/BA/MA/RA/WA, ICHIDAN, IRREGULAR
#
# Examples:
#   ./scripts/dict_tool.pl check "Wi-Fi"           # Check if Wi-Fi can be added
#   ./scripts/dict_tool.pl suggest "食べる"         # Get POS/conj suggestion
#   ./scripts/dict_tool.pl add "Wi-Fi" NOUN        # Add Wi-Fi as noun
#   ./scripts/dict_tool.pl add "美味しい" ADJECTIVE I_ADJ  # Add i-adjective
#   ./scripts/dict_tool.pl validate                # Check dictionary for issues
#   ./scripts/dict_tool.pl cleanup data/dict.tsv  # Analyze dictionary entries
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
use FindBin qw($RealBin);
use lib "$RealBin/lib";
use SuzumeUtils qw(mecab_analyze get_suzume_rule apply_suzume_merge get_char_types map_mecab_pos @NAI_ADJECTIVES);

binmode(STDOUT, ':utf8');
binmode(STDERR, ':utf8');

# Decode command-line arguments as UTF-8
@ARGV = map { decode('utf-8', $_) } @ARGV;

# POS → dictionary file mapping (split dictionary)
my %pos_to_file = (
    ADJECTIVE   => 'data/core/adjectives.tsv',
    ADVERB      => 'data/core/adverbs.tsv',
    VERB        => 'data/core/verbs.tsv',
    NOUN        => 'data/core/nouns.tsv',
    PROPER_NOUN => 'data/core/nouns.tsv',
    PRONOUN     => 'data/core/expressions.tsv',
    INTERJECTION => 'data/core/expressions.tsv',
    CONJUNCTION => 'data/core/expressions.tsv',
    PARTICLE    => 'data/core/expressions.tsv',
    SUFFIX      => 'data/core/suffixes.tsv',
    PREFIX      => 'data/core/suffixes.tsv',
    OTHER       => 'data/core/expressions.tsv',
    PHRASE      => 'data/core/expressions.tsv',
    AUX         => 'data/core/expressions.tsv',
);
my @all_dict_files = (
    'data/core/adjectives.tsv',
    'data/core/adverbs.tsv',
    'data/core/verbs.tsv',
    'data/core/nouns.tsv',
    'data/core/expressions.tsv',
    'data/core/suffixes.tsv',
);
my $dict_path = 'data/core/nouns.tsv';  # default fallback
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

# Decode UTF-8 command line arguments
utf8::decode($_) for @ARGV;

if ($command eq 'check') {
    cmd_check(@ARGV);
} elsif ($command eq 'add') {
    cmd_add(@ARGV);
} elsif ($command eq 'remove') {
    cmd_remove(@ARGV);
} elsif ($command eq 'disable') {
    cmd_disable(@ARGV);
} elsif ($command eq 'enable') {
    cmd_enable(@ARGV);
} elsif ($command eq 'validate') {
    cmd_validate(@ARGV);
} elsif ($command eq 'suggest') {
    cmd_suggest(@ARGV);
} elsif ($command eq 'cleanup') {
    cmd_cleanup(@ARGV);
} else {
    die "Unknown command: $command\n";
}

sub print_help {
    print <<'HELP';
L2 Dictionary helper for adding entries safely

Usage: ./scripts/dict_tool.pl <command> [options]

Commands:
  check <word>             Check if word can be added (MeCab analysis + duplicates)
  add <word> <pos> [conj]  Add word to dictionary (with safety checks)
  remove <word>            Remove word from dictionary
  disable <word>           Comment out word (keeps in file, but inactive)
  enable <word>            Uncomment disabled word
  validate                 Validate entire dictionary for issues
  suggest <word>           Suggest POS and conj_type based on MeCab
  cleanup <input.tsv>      Analyze dictionary, separate needed/unneeded entries

Options:
  -f, --force              Add even if MeCab splits the word (requires confirmation)
  --dry-run                Show what would be added without modifying
  --fix                    Apply fixes (for validate command)
  -h, --help               Show this help

POS values: ADJECTIVE, ADVERB, VERB, NOUN, PROPER_NOUN, PRONOUN, PREFIX, SUFFIX, INTERJECTION, PARTICLE, AUX, OTHER
Conjugation types: I_ADJ, NA_ADJ, GODAN_KA/GA/SA/TA/NA/BA/MA/RA/WA, ICHIDAN, IRREGULAR

Examples:
  ./scripts/dict_tool.pl check "Wi-Fi"
  ./scripts/dict_tool.pl suggest "食べる"
  ./scripts/dict_tool.pl add "Wi-Fi" NOUN
  ./scripts/dict_tool.pl add "美味しい" ADJECTIVE I_ADJ
  ./scripts/dict_tool.pl remove "不要な単語"
  ./scripts/dict_tool.pl disable "一時無効"
  ./scripts/dict_tool.pl enable "一時無効"
  ./scripts/dict_tool.pl validate
  ./scripts/dict_tool.pl cleanup data/dict.tsv
HELP
}

# =============================================================================
# Local utilities (not in shared module)
# =============================================================================

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

    # Skip if Suzume has a rule to keep as single token (e.g., nai-adjectives)
    my $suzume_rule = get_suzume_rule($surface);
    return undef if $suzume_rule;

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

# Convert SuzumeUtils POS to dictionary format (uppercase)
sub to_dict_pos {
    my ($pos) = @_;
    return undef unless $pos;

    my %dict_pos_map = (
        'Noun'        => 'NOUN',
        'Verb'        => 'VERB',
        'Adjective'   => 'ADJECTIVE',
        'Adverb'      => 'ADVERB',
        'Prefix'      => 'PREFIX',
        'Suffix'      => 'SUFFIX',
        'Pronoun'     => 'PRONOUN',
        'Interjection'=> 'INTERJECTION',
        'Adnominal'   => 'ADNOMINAL',
        'Conjunction' => 'CONJUNCTION',
        'Particle'    => 'PARTICLE',
        'Auxiliary'   => 'AUXILIARY',
        'Symbol'      => 'SYMBOL',
        'Filler'      => 'FILLER',
        'Other'       => 'OTHER',
    );

    return $dict_pos_map{$pos} // uc($pos);
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

sub get_dict_file_for_pos {
    my ($pos) = @_;
    return $pos_to_file{$pos} // 'data/core/nouns.tsv';
}

sub load_dictionary {
    my @entries;
    my %by_surface;

    for my $file (@all_dict_files) {
        next unless -f $file;
        open my $fh, '<:utf8', $file or die "Cannot open $file: $!\n";

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
                file      => $file,
            };

            push @entries, $entry;
            push @{$by_surface{$fields[0]}}, $entry;
        }

        close $fh;
    }
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
    die "Usage: dict_tool.pl check <word>\n" unless $word;

    print "=" x 60, "\n";
    print "Checking: $word\n";
    print "=" x 60, "\n\n";

    # 0. Check Suzume normalization rules first
    my $suzume_rule = get_suzume_rule($word);
    if ($suzume_rule) {
        print "ℹ️  Suzume rule detected: $suzume_rule\n";
        print "   Suzume treats '$word' as 1 token (MeCab may split it)\n\n";
    }

    # 1. MeCab analysis
    print "MeCab analysis:\n";
    my @tokens = @{mecab_analyze($word)};

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
        if ($suzume_rule) {
            print "✓  MeCab splits, but Suzume rule ($suzume_rule) keeps as 1 token\n\n";
        } else {
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
        my $suggested_pos = to_dict_pos(map_mecab_pos($tokens[0]));
        my $suggested_conj = map_conj_type($tokens[0]);

        if ($suggested_pos) {
            print "Suggested entry:\n";
            my $entry = "$word\t$suggested_pos";
            $entry .= "\t$suggested_conj" if $suggested_conj;
            print "  $entry\n\n";

            print "To add:\n";
            print "  ./scripts/dict_tool.pl add \"$word\" $suggested_pos";
            print " $suggested_conj" if $suggested_conj;
            print "\n";
        }
    } elsif (@tokens > 1) {
        print "For compound words:\n";
        print "  ./scripts/dict_tool.pl add \"$word\" NOUN --force\n\n";
        print "For proper nouns (titles, names):\n";
        print "  ./scripts/dict_tool.pl add \"$word\" PROPER_NOUN\n";
    }

    return 1;
}

sub cmd_suggest {
    my ($word) = @_;
    die "Usage: dict_tool.pl suggest <word>\n" unless $word;

    my @tokens = @{mecab_analyze($word)};

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
        print "  ./scripts/dict_tool.pl add \"$word\" NOUN\n";
        return;
    }

    my $t = $tokens[0];
    my $pos = to_dict_pos(map_mecab_pos($t));
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
        print "  ./scripts/dict_tool.pl add \"$word\" $pos";
        print " $conj" if $conj;
        print "\n";
    } else {
        print "Cannot suggest POS for: $t->{pos}\n";
        print "This may be a closed-class word (use L1 instead)\n";
    }
}

sub cmd_add {
    my ($word, $pos, $conj_type) = @_;
    die "Usage: dict_tool.pl add <word> <pos> [conj_type]\n" unless $word && $pos;

    # Validate POS
    my @valid_pos = qw(ADJECTIVE ADVERB VERB NOUN PROPER_NOUN PRONOUN PREFIX SUFFIX INTERJECTION ADNOMINAL CONJUNCTION PARTICLE AUX OTHER);
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
    my @tokens = @{mecab_analyze($word)};
    my $skip_conj_check = ($pos eq 'PROPER_NOUN');
    my $suzume_rule = get_suzume_rule($word);

    if (@tokens > 1 && !$force && !$skip_conj_check) {
        # Allow if Suzume rule keeps it as 1 token
        if ($suzume_rule) {
            print "ℹ️  MeCab splits but Suzume rule ($suzume_rule) keeps as 1 token - OK\n";
        } else {
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
        }
    } elsif (@tokens > 1 && $skip_conj_check) {
        print "ℹ️  Note: MeCab splits '$word' but allowing as PROPER_NOUN\n";
    }

    # Build entry line
    my $entry_line = "$word\t$pos";
    $entry_line .= "\t$conj_type" if $conj_type;

    # Determine target file based on POS
    my $target_file = get_dict_file_for_pos($pos);
    $dict_path = $target_file;

    # Find insertion point (grouped by POS and conj_type)
    my ($insert_after, $placement) = find_insertion_point($pos, $conj_type);

    if ($dry_run) {
        print "Would add to $target_file:\n";
        print "  $entry_line\n";
        print "  Placement: $placement (line $insert_after)\n" if $insert_after >= 0;
        return 1;
    }

    if ($insert_after < 0) {
        # No section found, append to end
        open my $fh, '>>:utf8', $target_file or die "Cannot open $target_file: $!\n";
        print $fh "$entry_line\n";
        close $fh;
        print "✓ Added: $entry_line (appended to end of $target_file)\n";
    } else {
        # Insert after the found line
        insert_line_at($target_file, $insert_after, $entry_line);
        my $placement_msg = {
            same_group    => "grouped with same $pos" . ($conj_type ? "/$conj_type" : ""),
            same_pos      => "added to $pos section",
            section_end   => "added at end of section",
            section_start => "added at start of section",
        }->{$placement} // $placement;
        print "✓ Added: $entry_line ($placement_msg in $target_file)\n";
    }

    print "  Recompile: ./build/bin/suzume-cli dict compile data/core/*.tsv data/core.dic\n";

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

# Find insertion point for entry, grouping by POS and conj_type
sub find_insertion_point {
    my ($target_pos, $target_conj) = @_;
    $target_conj //= '';

    open my $fh, '<:utf8', $dict_path or die "Cannot open $dict_path: $!\n";
    my @lines = <$fh>;
    close $fh;

    my $section = find_section_for_pos($target_pos);
    my $in_section = 0;
    my $section_start = -1;
    my $section_end = -1;
    my $last_matching_line = -1;
    my $last_same_pos_line = -1;

    for my $i (0 .. $#lines) {
        my $line = $lines[$i];
        chomp $line;

        # Track section boundaries
        if ($line =~ /^# $section\b/) {
            $in_section = 1;
            $section_start = $i;
            next;
        }

        # Handle section header's closing === line (skip it)
        if ($in_section && $section_start == $i - 1 && $line =~ /^# =+$/) {
            next;  # Skip closing === of section header
        }

        # End of section (next major section)
        if ($in_section && $line =~ /^# =+$/) {
            $section_end = $i;
            last;
        }

        next unless $in_section;
        next if $line =~ /^#/ || $line =~ /^\s*$/;

        # Parse entry
        my @fields = split /\t/, $line;
        next unless @fields >= 2;

        my ($surface, $pos, $conj) = @fields;
        $conj //= '';

        # Track entries with same POS
        if ($pos eq $target_pos) {
            $last_same_pos_line = $i;

            # Track entries with matching conj_type
            if ($conj eq $target_conj) {
                $last_matching_line = $i;
            }
        }
    }

    # If no section header found (split dict files), scan entire file
    if ($section_start < 0) {
        for my $i (0 .. $#lines) {
            my $line = $lines[$i];
            chomp $line;
            next if $line =~ /^#/ || $line =~ /^\s*$/;

            my @fields = split /\t/, $line;
            next unless @fields >= 2;

            my ($surface, $pos, $conj) = @fields;
            $conj //= '';

            if ($pos eq $target_pos) {
                $last_same_pos_line = $i;
                if ($conj eq $target_conj) {
                    $last_matching_line = $i;
                }
            }
        }
    }

    # Priority: 1) After matching conj_type, 2) After same POS, 3) Section end
    if ($last_matching_line >= 0) {
        return ($last_matching_line, 'same_group');
    } elsif ($last_same_pos_line >= 0) {
        return ($last_same_pos_line, 'same_pos');
    } elsif ($section_end >= 0) {
        return ($section_end - 1, 'section_end');
    } elsif ($section_start >= 0) {
        return ($section_start, 'section_start');
    }

    return (-1, 'not_found');
}

sub insert_line_at {
    my ($file, $after_line, $new_line) = @_;

    open my $in, '<:utf8', $file or die "Cannot open $file: $!\n";
    my @lines = <$in>;
    close $in;

    splice @lines, $after_line + 1, 0, "$new_line\n";

    open my $out, '>:utf8', $file or die "Cannot write $file: $!\n";
    print $out @lines;
    close $out;
}

sub find_word_in_files {
    my ($word) = @_;
    for my $file (@all_dict_files) {
        next unless -f $file;
        open my $fh, '<:utf8', $file or next;
        while (<$fh>) {
            if (/^([^\t#]+)\t/ && $1 eq $word) {
                close $fh;
                return $file;
            }
            if (/^#DISABLED#\s+([^\t]+)\t/ && $1 eq $word) {
                close $fh;
                return $file;
            }
        }
        close $fh;
    }
    return undef;
}

sub cmd_remove {
    my ($word) = @_;
    die "Usage: dict_tool.pl remove <word>\n" unless $word;

    my $target_file = find_word_in_files($word);
    die "Word not found in dictionary: $word\n" unless $target_file;

    open my $fh, '<:utf8', $target_file or die "Cannot open $target_file: $!\n";
    my @lines = <$fh>;
    close $fh;

    my $found = 0;
    my @new_lines;
    for my $line (@lines) {
        if ($line =~ /^([^\t#]+)\t/ && $1 eq $word) {
            $found = 1;
            print "Removing: $line";
            next;
        }
        push @new_lines, $line;
    }

    if ($dry_run) {
        print "\n[dry-run] Would remove '$word' from $target_file\n";
        return;
    }

    open my $out, '>:utf8', $target_file or die "Cannot write $target_file: $!\n";
    print $out @new_lines;
    close $out;

    print "\n✓ Removed '$word' from $target_file\n";
    print "  Recompile: ./build/bin/suzume-cli dict compile data/core/*.tsv data/core.dic\n";
}

sub cmd_disable {
    my ($word) = @_;
    die "Usage: dict_tool.pl disable <word>\n" unless $word;

    my $target_file = find_word_in_files($word);
    die "Word not found in dictionary: $word\n" unless $target_file;

    open my $fh, '<:utf8', $target_file or die "Cannot open $target_file: $!\n";
    my @lines = <$fh>;
    close $fh;

    my $found = 0;
    for my $line (@lines) {
        if ($line =~ /^([^\t#]+)\t/ && $1 eq $word) {
            $found = 1;
            my $old = $line;
            $line = "#DISABLED# $line";
            print "Disabling: $old";
            print "       --> $line";
        }
    }

    if ($dry_run) {
        print "\n[dry-run] Would disable '$word'\n";
        return;
    }

    open my $out, '>:utf8', $target_file or die "Cannot write $target_file: $!\n";
    print $out @lines;
    close $out;

    print "\n✓ Disabled '$word' in $target_file\n";
    print "  Use 'enable' to re-activate, or 'remove' to delete permanently\n";
    print "  Recompile: ./build/bin/suzume-cli dict compile data/core/*.tsv data/core.dic\n";
}

sub cmd_enable {
    my ($word) = @_;
    die "Usage: dict_tool.pl enable <word>\n" unless $word;

    my $target_file = find_word_in_files($word);
    die "Disabled word not found: $word\n" unless $target_file;

    open my $fh, '<:utf8', $target_file or die "Cannot open $target_file: $!\n";
    my @lines = <$fh>;
    close $fh;

    my $found = 0;
    for my $line (@lines) {
        if ($line =~ /^#DISABLED#\s+([^\t]+)\t/ && $1 eq $word) {
            $found = 1;
            my $old = $line;
            $line =~ s/^#DISABLED#\s+//;
            print "Enabling: $old";
            print "      --> $line";
        }
    }

    if (!$found) {
        die "Disabled word not found: $word (looking for '#DISABLED# $word')\n";
    }

    if ($dry_run) {
        print "\n[dry-run] Would enable '$word'\n";
        return;
    }

    open my $out, '>:utf8', $target_file or die "Cannot write $target_file: $!\n";
    print $out @lines;
    close $out;

    print "\n✓ Enabled '$word' in $target_file\n";
    print "  Recompile: ./build/bin/suzume-cli dict compile data/core/*.tsv data/core.dic\n";
}

sub cmd_validate {
    print "=" x 60, "\n";
    print "Validating dictionaries: ", join(', ', map { basename($_) } @all_dict_files), "\n";
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
        my @tokens = @{mecab_analyze($word)};

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
            print "  ./build/bin/suzume-cli dict compile data/core/*.tsv data/core.dic\n";
        } else {
            print "\nTo apply fixes, run:\n";
            print "  ./scripts/dict_tool.pl validate --fix\n";
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

sub cmd_cleanup {
    # Analyze dictionary and separate needed/unneeded entries (merged from cleanup_dict.pl)
    my ($input_file) = @_;
    die "Usage: dict_tool.pl cleanup <input.tsv>\n" unless $input_file && -f $input_file;

    my $SUZUME = './build/bin/suzume-cli';
    die "suzume not found: $SUZUME (build first)\n" unless -x $SUZUME;

    # Output files
    my ($base) = $input_file =~ /(.+)\.tsv$/;
    $base //= $input_file;
    my $keep_file  = "${base}_keep.tsv";
    my $noise_file = "${base}_noise.tsv";

    my $is_fixed_expression = sub {
        my ($str) = @_;
        return $str =~ /[一-龯ァ-ヶ][のをがはにへとで][一-龯ぁ-んァ-ヶ]/;
    };

    my $is_split_by_suzume = sub {
        my ($surface) = @_;
        my $cmd = qq{$SUZUME analyze "$surface" 2>/dev/null};
        my $output = `$cmd`;
        my @lines = grep { /\S/ } split /\n/, $output;
        return scalar(@lines) > 1;
    };

    my $should_keep = sub {
        my ($surface, $pos) = @_;
        my $len = length($surface);
        my @types = get_char_types($surface);

        return (0, "too_short") if $len <= 2;

        my $is_split = $is_split_by_suzume->($surface);
        return (0, "handled_by_suzume") unless $is_split;

        return (1, "fixed_expression") if $is_fixed_expression->($surface);
        return (1, "mixed_chartype_compound") if @types > 1;

        if (@types && $types[0] eq 'kanji' && $len >= 4) {
            return (1, "kanji_compound");
        }

        return (1, "split_other");
    };

    open my $in, '<:utf8', $input_file or die "Cannot open $input_file: $!\n";
    my @keep;
    my @noise;
    my $line_num = 0;
    my $total = 0;
    my $kept = 0;

    print STDERR "Processing: $input_file\n";

    while (my $line = <$in>) {
        $line_num++;
        chomp $line;

        if ($line =~ /^\s*#/ || $line =~ /^\s*$/) {
            push @keep, $line;
            next;
        }

        my @fields = split /\t/, $line;
        next unless @fields >= 3;

        my ($surface, $pos, $reading) = @fields[0, 1, 2];
        $total++;

        my ($keep_entry, $reason) = $should_keep->($surface, $pos);

        if ($keep_entry) {
            push @keep, $line;
            $kept++;
            print STDERR "  KEEP: $surface ($reason)\n" if $dry_run;
        } else {
            push @noise, "$line\t# $reason";
            print STDERR "  DROP: $surface ($reason)\n" if $dry_run;
        }

        print STDERR "  ... processed $line_num lines\r" if $line_num % 100 == 0;
    }
    close $in;

    print STDERR "\n";
    print STDERR "Results: $kept / $total entries kept\n";

    unless ($dry_run) {
        open my $out_keep, '>:utf8', $keep_file or die "Cannot write $keep_file: $!\n";
        print $out_keep "$_\n" for @keep;
        close $out_keep;
        print STDERR "Written: $keep_file\n";

        open my $out_noise, '>:utf8', $noise_file or die "Cannot write $noise_file: $!\n";
        print $out_noise "$_\n" for @noise;
        close $out_noise;
        print STDERR "Written: $noise_file\n";
    }

    print STDERR "Done.\n";
}
