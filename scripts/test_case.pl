#!/usr/bin/env perl
# Manage tokenization test cases
# Usage: ./scripts/test_case.pl <command> [options]
#
# Commands:
#   show <input>             Show MeCab output, JSON expected, and check if test exists
#                            Also shows current expected if test exists
#   update <input>           Update test case to match MeCab/Suzume output
#   update -t <test_id>      Update specific test by ID (e.g., verb_ichidan/5)
#                            Shows source (MeCab/MeCab+SuzumeRules) and auto-rule
#   add -f <file> <input>    Add new test case to file (rejects duplicates)
#                            Use --id to specify ID, or auto-generates snake_case ASCII ID
#   sync                     Sync tests where Suzume=MeCab (dry-run by default)
#   diff-suzume              Analyze failures by category (read-only)
#   diff-mecab               Find tests where expected differs from MeCab
#                            (detects intentional Suzume rules vs potential errors)
#   needs-suzume-update      Find tests where expected != MeCab+SuzumeRules
#                            Use --apply to auto-update to correct expected values
#   check-coverage <file>    Check which inputs in file have tests
#   batch-add -f <tgt> <file> Batch add tests from input file (skips duplicates)
#                            Shows source/rule for each added test
#   suggest-file <input>     Suggest which test file for input
#   replace-pos OLD NEW      Replace POS in all test files (dry-run by default)
#   map-pos SURFACE OLD NEW  Replace POS only for specific surface (dry-run by default)
#   list                     List all test files with case counts
#   list-pos                 List all POS values used in tests with counts and examples
#   validate-ids             Detect/fix non-ASCII or duplicate IDs (--apply to fix)
#
# Options:
#   -f, --file FILE    Target test file (without .json), or 'all' for all files
#   -t, --test-id ID   Test ID (format: file/index or file/id_string)
#   --id ID            Case ID for add command (snake_case recommended)
#   -d, --description  Description for new test
#   --tags TAG,TAG     Comma-separated tags
#   -n, --dry-run      Show changes without applying
#   --apply            Actually apply changes (sync/batch-add/replace-pos/map-pos/
#                      needs-suzume-update/validate-ids)
#   --use-suzume       Force Suzume output for expected (update/add/batch-add)
#   --limit N          Limit output per category (diff-suzume/diff-mecab)
#   -h, --help         Show this help
#
# Suzume rules (design differences from MeCab):
#   1. number+unit: "3人", "100円", "35ページ" (number + counter merged)
#   2. date: "2024年12月23日" (full date as single token)
#   3. nai-adjective: "だらしない", "もったいない" (lexical adjectives)
#   4. slang-adjective: "エモい", "キモい" (pre-processed for MeCab)
#   5. kanji-compound: "開始予定", "経済成長" (consecutive kanji merged)
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
#   ./scripts/test_case.pl show "食べています"
#   ./scripts/test_case.pl update "食べています"
#   ./scripts/test_case.pl add -f noun_general "3人"     # auto: Suzume (number+unit)
#   ./scripts/test_case.pl add -f adjective_i_basic "だらしない"  # auto: Suzume
#
#   # Coverage check and batch add (auto-detect enabled)
#   ./scripts/test_case.pl check-coverage /tmp/inputs.txt
#   ./scripts/test_case.pl batch-add -f basic /tmp/inputs.txt --apply
#
#   # Force Suzume output for all inputs
#   ./scripts/test_case.pl batch-add -f basic /tmp/inputs.txt --use-suzume --apply
#
#   # Sync and analysis
#   ./scripts/test_case.pl sync --apply       # sync where Suzume=MeCab
#   ./scripts/test_case.pl diff-suzume        # analyze failures
#
#   # POS replacement (all files)
#   ./scripts/test_case.pl replace-pos Auxiliary Aux_Tense -f all --apply
#   ./scripts/test_case.pl map-pos た Auxiliary Aux_Tense --apply

use strict;
use warnings;
use utf8;
use Getopt::Long;
use JSON::PP;
use File::Basename;

binmode(STDOUT, ':utf8');
binmode(STDERR, ':utf8');

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
my $test_data_dir = "$project_root/tests/data/tokenization";

# MeCab POS to Suzume POS mapping
my %pos_map = (
    '名詞'     => 'Noun',
    '動詞'     => 'Verb',
    '形容詞'   => 'Adjective',
    '副詞'     => 'Adverb',
    '助詞'     => 'Particle',
    '助動詞'   => 'Auxiliary',
    '接続詞'   => 'Conjunction',
    '感動詞'   => 'Interjection',
    '連体詞'   => 'Adnominal',
    '接頭詞'   => 'Prefix',
    '記号'     => 'Symbol',
    'フィラー' => 'Filler',
    'その他'   => 'Other',
);

sub print_help {
    print <<'HELP';
Usage: test_case.pl <command> [options] [input]

Commands:
  show <input>           Show what MeCab outputs (dry-run)
  update <input>         Find and update test case matching input
  update -t <test_id>    Update specific test by ID
  add -f <file> <input>  Add new test case to file
  sync                   Sync all tests where Suzume=MeCab (dry-run by default)
  diff-suzume            Analyze failures: categorize by type (read-only)
  diff-mecab             Find tests not MeCab-compatible (intentional or errors)
  check-coverage <file>  Check which inputs in file have tests
  batch-add -f <target> <file>  Batch add tests from file
  suggest-file <input>   Suggest which test file for input
  replace-pos OLD NEW    Replace POS in all test files
  map-pos SURFACE OLD NEW Replace POS only for specific surface
  list                   List all test files
  list-pos               List all POS values used in tests

Options:
  -f, --file FILE        Target test file (without .json), or 'all' for all files
  -t, --test-id ID       Test ID (format: file/index)
  --id ID                Case ID for add command (snake_case recommended)
  -d, --description STR  Description for new test
  --tags TAG,TAG         Comma-separated tags
  -n, --dry-run          Show changes without applying
  --apply                Actually apply changes (required for sync/batch-add/replace-pos/map-pos)
  --use-suzume           Use Suzume output as expected (for grammar-first cases)
  -h, --help             Show this help

Examples:
  ./scripts/test_case.pl show "食べています"
  ./scripts/test_case.pl update "食べています"
  ./scripts/test_case.pl update -t "verb_ichidan/5"
  ./scripts/test_case.pl add -f verb_ichidan "食べています"
  ./scripts/test_case.pl add -f verb_ichidan --id eating_progressive "食べています"
  ./scripts/test_case.pl sync               # dry-run: Suzume=MeCab cases
  ./scripts/test_case.pl sync --apply       # actually sync
  ./scripts/test_case.pl diff-suzume        # analyze failures by category
  ./scripts/test_case.pl check-coverage /tmp/inputs.txt  # check coverage
  ./scripts/test_case.pl batch-add -f basic /tmp/inputs.txt  # dry-run batch add
  ./scripts/test_case.pl batch-add -f basic /tmp/inputs.txt --apply  # add tests
  ./scripts/test_case.pl suggest-file "食べたい"  # suggest test file
  ./scripts/test_case.pl replace-pos Auxiliary Auxiliary_Tense_Ta --apply
  ./scripts/test_case.pl map-pos た Auxiliary Auxiliary_Tense_Ta --apply
  ./scripts/test_case.pl list-pos           # show all POS values used
HELP
}

# Patterns where Suzume intentionally diverges from MeCab
# These should use Suzume output as expected value
# AGREED RULES ONLY - everything else follows MeCab
sub should_use_suzume_rule {
    my ($text) = @_;

    # 1. Number + counter/unit (数字+助数詞): "3人", "35ページ", "100円"
    #    Suzume keeps as single token, MeCab splits
    return 'number+unit' if $text =~ /\d+[人個本枚台回件円年月日時分秒ページ頁階番号歳才個所箇所]/;

    # 2. Date patterns: "2024年12月23日"
    return 'date' if $text =~ /\d+年\d+月\d+日/;

    # 3. ナイ形容詞 (adjectives ending in ない that are single words)
    #    e.g., だらしない, つまらない, しょうがない, もったいない
    my @nai_adjectives = qw(
        だらしない つまらない しょうがない もったいない くだらない
        せわしない やるせない いたたまれない あどけない おぼつかない
        はしたない みっともない ろくでもない どうしようもない
    );
    for my $adj (@nai_adjectives) {
        return 'nai-adjective' if $text =~ /\Q$adj\E/;
    }

    # 4. New/slang adjectives and their inflections
    #    e.g., エモい, エモかった, エモくない, キモい, ウザい, やばい
    #    MeCab doesn't recognize these as adjectives
    my @slang_adj_stems = qw(エモ キモ ウザ ダサ イタ);
    for my $stem (@slang_adj_stems) {
        return 'slang-adjective' if $text =~ /\Q$stem\E[いかくけさ]/;
    }
    return 'slang-adjective' if $text =~ /やばい|やばかっ|やばく/;

    # 5. Kanji compound: consecutive kanji stay together
    #    Suzume doesn't have dictionary to split compound nouns
    #    e.g., 開始予定, 経済成長, 形態素解析
    return 'kanji-compound' if $text =~ /\p{Han}{2,}/;

    return '';  # Use MeCab
}

# Correct MeCab POS misclassifications
# MeCab sometimes labels particles (助詞) as nouns (名詞)
sub correct_mecab_pos {
    my ($tokens) = @_;

    # Known particles that MeCab may misclassify as Noun
    my %particle_corrections = (
        'の' => 'Particle',
        'が' => 'Particle',
        'を' => 'Particle',
        'に' => 'Particle',
        'へ' => 'Particle',
        'と' => 'Particle',
        'で' => 'Particle',
        'から' => 'Particle',
        'まで' => 'Particle',
        'より' => 'Particle',
        'は' => 'Particle',
        'も' => 'Particle',
        'か' => 'Particle',
        'な' => 'Particle',
        'ね' => 'Particle',
        'よ' => 'Particle',
        'わ' => 'Particle',
        'ぞ' => 'Particle',
        'さ' => 'Particle',
        'けど' => 'Particle',
        'けれど' => 'Particle',
        'し' => 'Particle',
        'のに' => 'Particle',
        'ので' => 'Particle',
        'ながら' => 'Particle',
        'ばかり' => 'Particle',
        'だけ' => 'Particle',
        'しか' => 'Particle',
        'ほど' => 'Particle',
        'くらい' => 'Particle',
        'ぐらい' => 'Particle',
        'など' => 'Particle',
        'なんか' => 'Particle',
        'なんて' => 'Particle',
        'って' => 'Particle',
    );

    for my $t (@$tokens) {
        my $surface = $t->{surface} // '';
        if (exists $particle_corrections{$surface} && $t->{pos} eq 'Noun') {
            $t->{pos} = $particle_corrections{$surface};
        }
    }

    return $tokens;
}

# Slang adjective stems and their standard replacement for MeCab
# MeCab doesn't know エモい etc., so we replace with 赤い (standard i-adjective)
my %slang_adj_stems = (
    'エモ' => '赤',
    'キモ' => '赤',
    'ウザ' => '赤',
    'ダサ' => '赤',
    'イタ' => '赤',
);

# Replace slang adjective stems with standard ones before MeCab
sub preprocess_for_mecab {
    my ($text) = @_;
    my %replacements;  # Track what we replaced for restoration

    for my $slang (keys %slang_adj_stems) {
        my $standard = $slang_adj_stems{$slang};
        # Replace slang stem followed by i-adjective endings
        # エモい, エモかった, エモくない, エモければ, etc.
        while ($text =~ /\Q$slang\E([いかくけさ])/g) {
            my $match_start = $-[0];
            my $match_end = $+[0];
            $replacements{$match_start} = {
                original => $slang,
                replacement => $standard,
                length => length($slang),
            };
        }
    }

    # Apply replacements (reverse order to preserve positions)
    for my $pos (sort { $b <=> $a } keys %replacements) {
        my $r = $replacements{$pos};
        substr($text, $pos, $r->{length}) = $r->{replacement};
    }

    return ($text, \%replacements);
}

# Restore slang adjectives in tokens after MeCab processing
sub postprocess_mecab_tokens {
    my ($tokens, $original_text, $replacements) = @_;

    return $tokens unless %$replacements;

    # Rebuild token surfaces based on original text positions
    my $pos = 0;
    for my $t (@$tokens) {
        my $surface = $t->{surface};

        # Check if this token's position had a replacement
        for my $repl_pos (keys %$replacements) {
            my $r = $replacements->{$repl_pos};
            # If token starts at or contains replacement position
            if ($pos <= $repl_pos && $repl_pos < $pos + length($surface)) {
                # Calculate offset within token
                my $offset = $repl_pos - $pos;
                # Replace the standard stem with original slang stem
                my $new_surface = $surface;
                substr($new_surface, $offset, length($r->{replacement})) = $r->{original};
                $t->{surface} = $new_surface;
                # Update lemma if it contains the replacement
                if (defined $t->{lemma} && $t->{lemma} =~ /\Q$r->{replacement}\E/) {
                    $t->{lemma} =~ s/\Q$r->{replacement}\E/$r->{original}/;
                }
            }
        }
        $pos += length($surface);
    }

    return $tokens;
}

# Counter/unit patterns that should be merged with preceding numbers
my @counter_units = qw(
    人 個 本 枚 台 回 件 円 年 月 日 時 分 秒 階 番 号 歳 才 目
    ページ 頁 時間 分間 秒間 年間 日間 週間 か月 ヶ月 カ月
);

# Post-process MeCab tokens to apply Suzume rules (number+unit, date, nai-adjective merging)
sub apply_suzume_merge_rules {
    my ($tokens, $text) = @_;
    my @result;
    my $i = 0;

    while ($i < @$tokens) {
        my $t = $tokens->[$i];
        my $merged = 0;

        # 1. Full date pattern: 2024年12月23日 → single token
        if (!$merged && $t->{surface} =~ /^\d+$/) {
            # Check if this starts a date pattern
            my $pos_in_text = 0;
            for my $k (0 .. $i - 1) { $pos_in_text += length($tokens->[$k]{surface}); }
            my $remaining = substr($text, $pos_in_text);

            if ($remaining =~ /^(\d+年\d+月\d+日)/) {
                my $date = $1;
                # Count tokens to merge
                my $len = 0;
                my $j = $i;
                while ($j < @$tokens && $len < length($date)) {
                    $len += length($tokens->[$j]{surface});
                    $j++;
                }
                if ($len == length($date)) {
                    push @result, { surface => $date, pos => 'Noun', lemma => $date };
                    $i = $j;
                    $merged = 1;
                }
            }
        }

        # 2. Number + unit: merge 数字 + 助数詞
        if (!$merged && $t->{surface} =~ /^\d+$/) {
            my $j = $i + 1;
            my $combined = $t->{surface};
            while ($j < @$tokens) {
                my $next_surface = $tokens->[$j]{surface};
                # Check if next token is a counter/unit
                my $is_counter = 0;
                for my $unit (@counter_units) {
                    if ($next_surface eq $unit) {
                        $is_counter = 1;
                        last;
                    }
                }
                if ($is_counter) {
                    $combined .= $next_surface;
                    $j++;
                } else {
                    last;
                }
            }
            if ($j > $i + 1) {
                push @result, { surface => $combined, pos => 'Noun', lemma => $combined };
                $i = $j;
                $merged = 1;
            }
        }

        # 3. Nai-adjective: merge だらし+ない → だらしない
        if (!$merged) {
            my @nai_adjs = qw(だらしない つまらない しょうがない もったいない くだらない
                             せわしない やるせない いたたまれない あどけない おぼつかない
                             はしたない みっともない ろくでもない どうしようもない);
            my $pos_in_text = 0;
            for my $k (0 .. $i - 1) { $pos_in_text += length($tokens->[$k]{surface}); }
            my $remaining = substr($text, $pos_in_text);

            for my $adj (@nai_adjs) {
                if ($remaining =~ /^\Q$adj\E/) {
                    my $len = 0;
                    my $j = $i;
                    while ($j < @$tokens && $len < length($adj)) {
                        $len += length($tokens->[$j]{surface});
                        $j++;
                    }
                    if ($len == length($adj)) {
                        push @result, { surface => $adj, pos => 'Adjective', lemma => $adj };
                        $i = $j;
                        $merged = 1;
                        last;
                    }
                }
            }
        }

        # 4. Kanji compound: merge consecutive kanji-only tokens
        #    開始 + 予定 → 開始予定
        if (!$merged && $t->{surface} =~ /^\p{Han}+$/) {
            my $j = $i + 1;
            my $combined = $t->{surface};
            while ($j < @$tokens && $tokens->[$j]{surface} =~ /^\p{Han}+$/) {
                $combined .= $tokens->[$j]{surface};
                $j++;
            }
            if ($j > $i + 1) {
                push @result, { surface => $combined, pos => 'Noun', lemma => $combined };
                $i = $j;
                $merged = 1;
            }
        }

        if (!$merged) {
            push @result, $t;
            $i++;
        }
    }
    return \@result;
}

# Get expected tokens: MeCab + Suzume rule corrections
sub get_expected_tokens {
    my ($text, $force_suzume, $suzume_cli) = @_;

    if ($force_suzume && -x $suzume_cli) {
        return (get_suzume_tokens_full($suzume_cli, $text), 'Suzume', 'forced');
    }

    # Get MeCab tokens (slang adjectives already handled via pre/post-processing)
    my $tokens = get_mecab_tokens($text);

    # Apply Suzume merge rules (number+unit, date, nai-adjective, kanji-compound)
    my $rule = should_use_suzume_rule($text);
    if ($rule && $rule =~ /^(number\+unit|date|nai-adjective|kanji-compound)$/) {
        $tokens = apply_suzume_merge_rules($tokens, $text);
        return ($tokens, 'MeCab+SuzumeRules', $rule);
    }

    return ($tokens, 'MeCab', $rule || '');
}

sub get_mecab_tokens {
    my ($text) = @_;

    # Preprocess: replace slang adjectives with standard ones
    my ($processed_text, $replacements) = preprocess_for_mecab($text);

    my $escaped = $processed_text;
    $escaped =~ s/'/'\\''/g;
    my $output = `echo '$escaped' | mecab 2>/dev/null`;
    utf8::decode($output);

    my @tokens;
    for my $line (split /\n/, $output) {
        last if $line eq 'EOS' || $line eq '';
        my ($surface, $features) = split /\t/, $line;
        next unless defined $surface && $surface ne '';

        my @feats = split /,/, ($features // '');
        my $pos_ja = $feats[0] // 'その他';
        my $pos = $pos_map{$pos_ja} // 'Other';
        my $lemma = $feats[6] // $surface;

        push @tokens, {
            surface => $surface,
            pos => $pos,
            lemma => $lemma,
        };
    }

    # Postprocess: restore slang adjectives
    postprocess_mecab_tokens(\@tokens, $text, $replacements);

    # Apply POS corrections (e.g., MeCab's particle→noun misclassification)
    correct_mecab_pos(\@tokens);

    return \@tokens;
}

sub format_expected {
    my ($tokens) = @_;
    my @result;
    for my $t (@$tokens) {
        my %entry = (surface => $t->{surface}, pos => $t->{pos});
        my $lemma = $t->{lemma} // '';
        $entry{lemma} = $lemma if $lemma ne '' && $lemma ne $t->{surface};
        push @result, \%entry;
    }
    return \@result;
}

sub load_json {
    my ($file) = @_;
    open my $fh, '<:raw', $file or die "Cannot open $file: $!\n";
    my $content = do { local $/; <$fh> };
    close $fh;
    return decode_json($content);
}

sub save_json {
    my ($file, $data) = @_;
    my $json = JSON::PP->new->utf8(0)->pretty->canonical->indent_length(2);
    # Ensure proper sorting for readability
    open my $fh, '>:utf8', $file or die "Cannot write $file: $!\n";
    print $fh $json->encode($data);
    close $fh;
}

sub find_test_by_input {
    my ($input_text) = @_;

    opendir my $dh, $test_data_dir or die "Cannot open $test_data_dir: $!\n";
    my @files = grep { /\.json$/ } readdir $dh;
    closedir $dh;

    for my $file (sort @files) {
        my $path = "$test_data_dir/$file";
        my $data = eval { load_json($path) };
        next if $@;

        my $cases = $data->{cases} // $data->{test_cases} // [];
        for my $i (0 .. $#$cases) {
            if ($cases->[$i]{input} eq $input_text) {
                my $basename = $file;
                $basename =~ s/\.json$//;
                return {
                    file => $path,
                    basename => $basename,
                    index => $i,
                    case => $cases->[$i],
                    data => $data,
                };
            }
        }
    }
    return undef;
}

sub find_test_by_id {
    my ($id) = @_;
    my ($basename, $idx) = split /\//, $id;
    return undef unless defined $basename && defined $idx;

    # Normalize basename to lowercase (test output uses CamelCase)
    my $basename_lc = lc($basename);

    my $path = "$test_data_dir/$basename_lc.json";
    return undef unless -f $path;

    my $data = eval { load_json($path) };
    return undef if $@;

    my $cases = $data->{cases} // $data->{test_cases} // [];

    # Try numeric index first
    if ($idx =~ /^\d+$/) {
        my $i = int($idx);
        if ($i < scalar @$cases) {
            return {
                file => $path,
                basename => $basename,
                index => $i,
                case => $cases->[$i],
                data => $data,
            };
        }
    }

    # Try matching by case id
    for my $i (0 .. $#$cases) {
        if (($cases->[$i]{id} // '') eq $idx) {
            return {
                file => $path,
                basename => $basename,
                index => $i,
                case => $cases->[$i],
                data => $data,
            };
        }
    }

    return undef;
}

sub print_tokens {
    my ($tokens, $label) = @_;
    print "$label:\n";
    for my $t (@$tokens) {
        my $lemma_str = $t->{lemma} ne $t->{surface} ? " ($t->{lemma})" : "";
        print "  $t->{surface}\t$t->{pos}$lemma_str\n";
    }
}

sub generate_id {
    my ($input) = @_;
    # Generate ASCII-only ID from input
    # Japanese characters are converted to romaji-like representation
    my $id = $input;

    # Simple hiragana to romaji mapping (common characters)
    my %hira_to_roma = (
        'あ'=>'a','い'=>'i','う'=>'u','え'=>'e','お'=>'o',
        'か'=>'ka','き'=>'ki','く'=>'ku','け'=>'ke','こ'=>'ko',
        'さ'=>'sa','し'=>'shi','す'=>'su','せ'=>'se','そ'=>'so',
        'た'=>'ta','ち'=>'chi','つ'=>'tsu','て'=>'te','と'=>'to',
        'な'=>'na','に'=>'ni','ぬ'=>'nu','ね'=>'ne','の'=>'no',
        'は'=>'ha','ひ'=>'hi','ふ'=>'fu','へ'=>'he','ほ'=>'ho',
        'ま'=>'ma','み'=>'mi','む'=>'mu','め'=>'me','も'=>'mo',
        'や'=>'ya','ゆ'=>'yu','よ'=>'yo',
        'ら'=>'ra','り'=>'ri','る'=>'ru','れ'=>'re','ろ'=>'ro',
        'わ'=>'wa','を'=>'wo','ん'=>'n',
        'が'=>'ga','ぎ'=>'gi','ぐ'=>'gu','げ'=>'ge','ご'=>'go',
        'ざ'=>'za','じ'=>'ji','ず'=>'zu','ぜ'=>'ze','ぞ'=>'zo',
        'だ'=>'da','ぢ'=>'di','づ'=>'du','で'=>'de','ど'=>'do',
        'ば'=>'ba','び'=>'bi','ぶ'=>'bu','べ'=>'be','ぼ'=>'bo',
        'ぱ'=>'pa','ぴ'=>'pi','ぷ'=>'pu','ぺ'=>'pe','ぽ'=>'po',
        'っ'=>'tt','ゃ'=>'ya','ゅ'=>'yu','ょ'=>'yo',
    );

    # Convert hiragana to romaji
    for my $hira (keys %hira_to_roma) {
        $id =~ s/$hira/$hira_to_roma{$hira}/g;
    }

    # Convert katakana to hiragana first, then to romaji
    $id =~ tr/ァ-ン/ぁ-ん/;
    for my $hira (keys %hira_to_roma) {
        $id =~ s/$hira/$hira_to_roma{$hira}/g;
    }

    # Replace kanji with 'k' + codepoint (to make unique)
    $id =~ s/(\p{Han})/'k'.sprintf('%x',ord($1))/ge;

    # Replace any remaining non-ASCII with underscore
    $id =~ s/[^a-zA-Z0-9_]/_/g;

    # Collapse multiple underscores
    $id =~ s/_+/_/g;
    $id =~ s/^_|_$//g;

    # Ensure non-empty and reasonable length
    $id = 'test_' . time() if $id eq '' || $id =~ /^_*$/;
    $id = substr($id, 0, 40) if length($id) > 40;

    return $id;
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
        elsif (/FAILED.*GetParam\(\)\s*=\s*(\S+)\/(\S+)/) {
            # Extract file/case_id from GetParam() = Adjective_i_katta/yokatta_desu_polite_past
            my $file_part = $1;
            my $case_id = $2;
            if ($input ne '') {
                push @failures, {
                    id => "$file_part/$case_id",
                    file => $file_part,
                    case_id => $case_id,
                    input => $input,
                };
                $input = '';
            }
        }
    }

    close $fh;
    return \@failures;
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
        # Normalize POS (Suzume uses ADJ, PARTICLE, etc.)
        my $norm_pos = normalize_pos($pos // '');
        push @tokens, { surface => $surface, pos => $norm_pos, lemma => $lemma // $surface };
    }
    return \@tokens;
}

sub normalize_pos {
    my ($pos) = @_;
    # Map Suzume POS to standard form
    my %suzume_pos_map = (
        'NOUN'        => 'Noun',
        'VERB'        => 'Verb',
        'ADJ'         => 'Adjective',
        'ADVERB'      => 'Adverb',
        'PARTICLE'    => 'Particle',
        'AUX'         => 'Auxiliary',
        'CONJ'        => 'Conjunction',
        'INTJ'        => 'Interjection',
        'PRON'        => 'Pronoun',
        'DET'         => 'Determiner',
        'PREFIX'      => 'Prefix',
        'SUFFIX'      => 'Suffix',
        'PUNCT'       => 'Punctuation',
        'SYM'         => 'Symbol',
        'OTHER'       => 'Other',
    );
    return $suzume_pos_map{uc($pos)} // $pos;
}

sub tokens_match {
    my ($a, $b) = @_;
    return 0 unless @$a == @$b;
    for my $i (0 .. $#$a) {
        return 0 if $a->[$i]{surface} ne $b->[$i]{surface};
        # Also compare POS (both should be normalized)
        my $pos_a = $a->[$i]{pos} // '';
        my $pos_b = $b->[$i]{pos} // '';
        return 0 if $pos_a ne $pos_b;
        # Also compare lemma
        my $lemma_a = $a->[$i]{lemma} // $a->[$i]{surface};
        my $lemma_b = $b->[$i]{lemma} // $b->[$i]{surface};
        return 0 if $lemma_a ne $lemma_b;
    }
    return 1;
}

sub get_suzume_tokens_full {
    # Get full token info (surface, pos, lemma) from suzume-cli
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

        # Normalize Suzume POS to test format
        my $norm_pos = normalize_pos($pos // 'Other');
        my %token = (surface => $surface, pos => $norm_pos);
        $token{lemma} = $lemma if defined $lemma && $lemma ne $surface;
        push @tokens, \%token;
    }
    return \@tokens;
}

sub get_test_files {
    my ($file_filter) = @_;
    my @files;

    if ($file_filter && $file_filter ne 'all') {
        my $path = "$test_data_dir/$file_filter.json";
        push @files, $path if -f $path;
    } else {
        opendir my $dh, $test_data_dir or die "Cannot open $test_data_dir: $!\n";
        @files = map { "$test_data_dir/$_" } grep { /\.json$/ } readdir $dh;
        closedir $dh;
    }

    return sort @files;
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

    my $tokens = get_mecab_tokens($input);
    print "Input: $input\n\n";
    print_tokens($tokens, "MeCab");

    my $expected = format_expected($tokens);
    print "\nJSON expected:\n";
    my $json = JSON::PP->new->utf8(0)->pretty->canonical;
    print $json->encode($expected);

    # Check if test already exists
    my $found = find_test_by_input($input);
    if ($found) {
        print "\n✓ Test exists: $found->{basename}/$found->{index}\n";
        my $current = $found->{case}{expected} // [];
        print "\nCurrent expected:\n";
        print $json->encode($current);
    } else {
        print "\n✗ No existing test for this input\n";
    }
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

    my ($tokens, $source, $rule) = get_expected_tokens($input, $use_suzume, $suzume_cli);
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

    my ($tokens, $source, $rule) = get_expected_tokens($input, $use_suzume, $suzume_cli);
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

if ($command eq 'sync') {
    # Sync all cases where Suzume matches MeCab but test expected differs
    my $failures = get_failures_from_test_output();
    die "No failures found in /tmp/test.txt\n" unless @$failures;

    my $suzume_cli = "$project_root/build/bin/suzume-cli";
    die "suzume-cli not found\n" unless -x $suzume_cli;

    my @to_sync;
    print STDERR "Analyzing ", scalar(@$failures), " failures...\n";

    for my $f (@$failures) {
        my $input = $f->{input};
        my $id = $f->{id};

        # Get MeCab and Suzume outputs
        my $mecab = get_mecab_tokens($input);
        my $suzume = get_suzume_tokens($suzume_cli, $input);

        # Check if they match
        if (tokens_match($mecab, $suzume)) {
            push @to_sync, {
                id => $id,
                input => $input,
                mecab => $mecab,
            };
        }
    }

    if (!@to_sync) {
        print "No cases to sync (no failures where Suzume matches MeCab)\n";
        exit 0;
    }

    print "Found ", scalar(@to_sync), " cases where Suzume=MeCab but test fails:\n\n";

    for my $case (@to_sync) {
        print "  [$case->{id}] $case->{input}\n";
    }

    if (!$apply) {
        print "\n[DRY-RUN] Use --apply to make changes.\n";
        exit 0;
    }

    print "\nSyncing...\n";

    my $synced = 0;
    for my $case (@to_sync) {
        my $found = find_test_by_id($case->{id});
        next unless $found;

        my $expected = format_expected($case->{mecab});
        my $cases_key = exists $found->{data}{cases} ? 'cases' : 'test_cases';
        $found->{data}{$cases_key}[$found->{index}]{expected} = $expected;

        save_json($found->{file}, $found->{data});
        $synced++;
        print "  ✓ $case->{id}\n";
    }

    print "\n✓ Synced $synced cases\n";
    exit 0;
}

if ($command eq 'diff-suzume') {
    # Show differences between test expectations and suzume output (read-only)
    my $failures = get_failures_from_test_output();
    die "No failures found in /tmp/test.txt\n" unless @$failures;

    my $suzume_cli = "$project_root/build/bin/suzume-cli";
    die "suzume-cli not found\n" unless -x $suzume_cli;

    print STDERR "Analyzing ", scalar(@$failures), " failures...\n\n";

    my %categories = (
        segmentation => [],  # Different token count
        pos_only => [],      # Same tokens, different POS
        matches_mecab => [], # Suzume matches MeCab but not expected
    );

    for my $f (@$failures) {
        my $found = find_test_by_id($f->{id});
        next unless $found;

        my $expected = $found->{case}{expected} // [];
        my $suzume = get_suzume_tokens_full($suzume_cli, $f->{input});
        my $mecab = get_mecab_tokens($f->{input});

        my @exp_surfaces = map { $_->{surface} } @$expected;
        my @suz_surfaces = map { $_->{surface} } @$suzume;
        my @mec_surfaces = map { $_->{surface} } @$mecab;

        my $exp_str = join('|', @exp_surfaces);
        my $suz_str = join('|', @suz_surfaces);
        my $mec_str = join('|', @mec_surfaces);

        my $entry = {
            id => $f->{id},
            input => $f->{input},
            expected => $exp_str,
            suzume => $suz_str,
            mecab => $mec_str,
        };

        if ($suz_str eq $mec_str && $suz_str ne $exp_str) {
            push @{$categories{matches_mecab}}, $entry;
        } elsif ($exp_str ne $suz_str && @exp_surfaces != @suz_surfaces) {
            push @{$categories{segmentation}}, $entry;
        } else {
            push @{$categories{pos_only}}, $entry;
        }
    }

    # Helper to limit display count per category
    my $per_cat_limit = $limit > 0 ? $limit : 0;

    # Print categorized results
    if (@{$categories{matches_mecab}}) {
        my $total = scalar(@{$categories{matches_mecab}});
        print "=== Suzume=MeCab but Expected differs ($total) ===\n";
        print "These can be safely synced with 'sync' command:\n\n";
        my $shown = 0;
        for my $e (@{$categories{matches_mecab}}) {
            last if $per_cat_limit && $shown >= $per_cat_limit;
            print "  [$e->{id}] $e->{input}\n";
            print "    Expected: $e->{expected}\n";
            print "    Suzume:   $e->{suzume} ✓\n\n";
            $shown++;
        }
        print "  ... and ", ($total - $shown), " more\n\n" if $per_cat_limit && $shown < $total;
    }

    if (@{$categories{segmentation}}) {
        my $total = scalar(@{$categories{segmentation}});
        print "=== Segmentation differs ($total) ===\n";
        print "These need implementation fixes:\n\n";
        my $shown = 0;
        for my $e (@{$categories{segmentation}}) {
            last if $per_cat_limit && $shown >= $per_cat_limit;
            print "  [$e->{id}] $e->{input}\n";
            print "    Expected: $e->{expected}\n";
            print "    Suzume:   $e->{suzume}\n";
            print "    MeCab:    $e->{mecab}\n\n";
            $shown++;
        }
        print "  ... and ", ($total - $shown), " more\n\n" if $per_cat_limit && $shown < $total;
    }

    if (@{$categories{pos_only}}) {
        my $total = scalar(@{$categories{pos_only}});
        print "=== POS-only differences ($total) ===\n";
        print "Same segmentation, different POS:\n\n";
        my $shown = 0;
        for my $e (@{$categories{pos_only}}) {
            last if $per_cat_limit && $shown >= $per_cat_limit;
            print "  [$e->{id}] $e->{input}\n";
            print "    Expected: $e->{expected}\n";
            print "    Suzume:   $e->{suzume}\n\n";
            $shown++;
        }
        print "  ... and ", ($total - $shown), " more\n\n" if $per_cat_limit && $shown < $total;
    }

    # Summary
    print "=== Summary ===\n";
    printf "  Suzume=MeCab (safe to sync): %d\n", scalar(@{$categories{matches_mecab}});
    printf "  Segmentation issues:         %d\n", scalar(@{$categories{segmentation}});
    printf "  POS-only differences:        %d\n", scalar(@{$categories{pos_only}});
    exit 0;
}

if ($command eq 'diff-mecab') {
    # Find all test cases where expected differs from MeCab output
    # Categorize: intentional (Suzume rules) vs potential errors
    my @files = get_test_files($test_file);
    die "No test files found\n" unless @files;

    print STDERR "Scanning ", scalar(@files), " test files...\n\n";

    my %categories = (
        intentional => [],   # Matches should_use_suzume_rule
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

            my $rule = should_use_suzume_rule($inp);
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
    my @files = get_test_files($test_file);
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

            # Get correct expected: MeCab + Suzume rules
            my $mecab = get_mecab_tokens($inp);
            my $rule = should_use_suzume_rule($inp);
            my $correct = $rule ? apply_suzume_merge_rules($mecab, $inp) : $mecab;

            my @exp_surfaces = map { $_->{surface} } @$expected;
            my @cor_surfaces = map { $_->{surface} } @$correct;

            my $exp_str = join('|', @exp_surfaces);
            my $cor_str = join('|', @cor_surfaces);

            # If expected doesn't match correct, it needs update
            if ($exp_str ne $cor_str) {
                my $case_id = $case->{id} // $i;
                my $entry = {
                    id => "$basename/$case_id",
                    file => $path,
                    basename => $basename,
                    index => $i,
                    input => $inp,
                    rule => $rule || 'mecab-only',
                    expected => $exp_str,
                    correct => $cor_str,
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
            print "  [$e->{id}] $e->{input}\n";
            print "    Current:  $e->{expected}\n";
            print "    Correct:  $e->{correct}\n\n";
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
    my @files = get_test_files($test_file);
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

    my @files = get_test_files($test_file);
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

    my @files = get_test_files($test_file);
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
    my @files = get_test_files($test_file);
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

        my ($tokens, $source, $rule) = get_expected_tokens($inp, $use_suzume, $suzume_cli);
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

print STDERR "Unknown command: $command\n";
print_help();
exit 1;
