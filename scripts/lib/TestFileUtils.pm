package TestFileUtils;
# Test file utilities for Suzume test scripts

use strict;
use warnings;
use utf8;
use Exporter 'import';
use FindBin qw($RealBin);
use File::Basename qw(dirname);
use lib dirname(__FILE__);
use SuzumeUtils qw(load_json);

our @EXPORT_OK = qw(
    find_test_by_input
    find_test_by_id
    get_test_files
    get_failures_from_test_output
    generate_id
    get_test_data_dir
);

# =============================================================================
# Configuration
# =============================================================================

my $project_root;
my $test_data_dir;

sub _init_paths {
    return if defined $project_root;
    $project_root = "$RealBin/..";
    $test_data_dir = "$project_root/tests/data/tokenization";
}

sub get_test_data_dir {
    _init_paths();
    return $test_data_dir;
}

# =============================================================================
# Test File Operations
# =============================================================================

sub get_test_files {
    _init_paths();
    opendir my $dh, $test_data_dir or die "Cannot open $test_data_dir: $!\n";
    my @files = sort grep { /\.json$/ } readdir $dh;
    closedir $dh;
    return map { "$test_data_dir/$_" } @files;
}

sub find_test_by_input {
    my ($input_text) = @_;
    _init_paths();

    for my $path (get_test_files()) {
        my $data = eval { load_json($path) };
        next if $@;

        my $cases = $data->{cases} // $data->{test_cases} // [];
        for my $i (0 .. $#$cases) {
            if ($cases->[$i]{input} eq $input_text) {
                my $basename = $path;
                $basename =~ s/.*\///;
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
    _init_paths();
    my ($basename, $idx) = split /\//, $id;
    return undef unless defined $basename && defined $idx;

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

# =============================================================================
# Failure Parsing
# =============================================================================

sub get_failures_from_test_output {
    my ($test_output_file) = @_;
    $test_output_file //= '/tmp/test.txt';

    open my $fh, '<:utf8', $test_output_file
        or die "Cannot open $test_output_file (run ctest first)\n";

    my @failures;
    while (<$fh>) {
        # Match lines like: FAILED: Verb_ichidan/5 "食べている"
        if (/FAILED:\s+(\S+)\s+"(.+)"/) {
            push @failures, { id => $1, input => $2 };
        }
    }
    close $fh;
    return \@failures;
}

# =============================================================================
# ID Generation
# =============================================================================

sub generate_id {
    my ($input) = @_;
    my $id = $input;

    # Character type mappings for common Japanese
    my %char_map = (
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
        'っ'=>'tt','ー'=>'_',
    );

    # Replace known hiragana
    for my $char (keys %char_map) {
        $id =~ s/$char/$char_map{$char}/g;
    }

    # Replace remaining non-ASCII with underscore
    $id =~ s/[^\x00-\x7F]+/_/g;

    # Clean up
    $id =~ s/\s+/_/g;
    $id =~ s/_+/_/g;
    $id =~ s/^_|_$//g;
    $id = lc($id);

    return $id || 'unnamed';
}

1;  # Module must return true
