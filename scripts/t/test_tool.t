#!/usr/bin/env perl
# Integration tests for test_tool.pl
use strict;
use warnings;
use utf8;
use Test::More;
use FindBin qw($RealBin);
use File::Temp qw(tempfile tempdir);

binmode STDOUT, ':utf8';
binmode STDERR, ':utf8';

my $script = "$RealBin/../test_tool.pl";
my $project_root = "$RealBin/../..";

# Helper to run script and capture output
sub run_cmd {
    my (@args) = @_;
    my $cmd = join(' ', map { qq{"$_"} } ($script, @args));
    my $output = `$cmd 2>&1`;
    utf8::decode($output);
    return ($output, $?);
}

# =============================================================================
# Basic functionality tests
# =============================================================================

subtest 'help and list commands' => sub {
    my ($out, $rc) = run_cmd('--help');
    like($out, qr/Usage:/, 'help shows usage');

    ($out, $rc) = run_cmd('list');
    like($out, qr/Test files in/, 'list shows test files');
    like($out, qr/\d+ cases/, 'list shows case counts');
};

subtest 'show --tsv command' => sub {
    my ($out, $rc) = run_cmd('show', '--tsv', '食べる');
    is($rc, 0, 'show --tsv command succeeds');
    like($out, qr/食べる\tVerb/, 'show --tsv outputs verb');

    ($out, $rc) = run_cmd('show', '--tsv', 'だらしない');
    like($out, qr/だらしない\tAdjective/, 'nai-adjective merged');
};

subtest 'show command' => sub {
    my ($out, $rc) = run_cmd('show', '食べる');
    is($rc, 0, 'show command succeeds');
    like($out, qr/MeCab:/, 'show displays MeCab output');
    like($out, qr/Suzume:/, 'show displays Suzume output');
};

subtest 'list-pos command' => sub {
    my ($out, $rc) = run_cmd('list-pos');
    is($rc, 0, 'list-pos command succeeds');
    like($out, qr/POS values used/, 'list-pos shows header');
    like($out, qr/Noun/, 'list-pos includes Noun');
    like($out, qr/Verb/, 'list-pos includes Verb');
};

subtest 'validate-ids command' => sub {
    my ($out, $rc) = run_cmd('validate-ids');
    is($rc, 0, 'validate-ids command succeeds');
    like($out, qr/Validating test IDs/, 'validate-ids shows progress');
};

subtest 'needs-suzume-update command' => sub {
    my ($out, $rc) = run_cmd('needs-suzume-update');
    is($rc, 0, 'needs-suzume-update command succeeds');
    like($out, qr/Scanning/, 'needs-suzume-update scans files');
};

subtest 'diff-suzume command' => sub {
    my ($out, $rc) = run_cmd('diff-suzume', '--limit', '1');
    is($rc, 0, 'diff-suzume command succeeds');
    like($out, qr/Analyzing/, 'diff-suzume shows analysis');
};

# =============================================================================
# Compare command tests
# =============================================================================

subtest 'compare command' => sub {
    my $dir = tempdir(CLEANUP => 1);
    my $before = "$dir/before.txt";
    my $after = "$dir/after.txt";

    # Create test files
    open my $fh1, '>:utf8', $before or die;
    print $fh1 "Input: テスト1\nFAILED Tokenize/test, GetParam() = Basic/test_1\n";
    close $fh1;

    open my $fh2, '>:utf8', $after or die;
    print $fh2 "Input: テスト2\nFAILED Tokenize/test, GetParam() = Basic/test_2\n";
    close $fh2;

    my ($out, $rc) = run_cmd('compare', $before, $after);
    is($rc, 0, 'compare command succeeds');
    like($out, qr/Test Comparison Summary/, 'compare shows summary');
    like($out, qr/Improved:/, 'compare shows improved');
    like($out, qr/Regressed:/, 'compare shows regressed');
};

# =============================================================================
# Failed command tests
# =============================================================================

subtest 'failed command' => sub {
    # Skip if no test output file
    SKIP: {
        skip "No /tmp/test.txt", 2 unless -f '/tmp/test.txt';

        my ($out, $rc) = run_cmd('failed');
        is($rc, 0, 'failed command succeeds');
        like($out, qr/failed tokenization tests|No tokenization test failures/,
             'failed shows results or no failures');
    }
};

# =============================================================================
# Library integration tests
# =============================================================================

subtest 'SuzumeUtils integration' => sub {
    use lib "$RealBin/../lib";
    require_ok('SuzumeUtils');

    can_ok('SuzumeUtils', qw(
        mecab_analyze
        get_mecab_tokens
        get_mecab_surfaces
        get_expected_tokens
        get_suzume_surfaces
        get_suzume_debug_info
        get_char_types
        map_mecab_pos
    ));

    # Test get_char_types
    my @types = SuzumeUtils::get_char_types('テスト');
    ok(grep { $_ eq 'katakana' } @types, 'get_char_types detects katakana');

    @types = SuzumeUtils::get_char_types('漢字');
    ok(grep { $_ eq 'kanji' } @types, 'get_char_types detects kanji');

    @types = SuzumeUtils::get_char_types('test123');
    ok(grep { $_ eq 'alpha' } @types, 'get_char_types detects alpha');
    ok(grep { $_ eq 'digit' } @types, 'get_char_types detects digit');
};

subtest 'TestFileUtils integration' => sub {
    use lib "$RealBin/../lib";
    require_ok('TestFileUtils');

    can_ok('TestFileUtils', qw(
        find_test_by_input
        find_test_by_id
        get_test_files
        get_failures_from_test_output
        generate_id
        get_test_data_dir
    ));

    # Test generate_id
    my $id = TestFileUtils::generate_id('テスト');
    ok($id =~ /^[a-z0-9_]+$/, 'generate_id produces ASCII');

    # Test get_test_files
    my @files = TestFileUtils::get_test_files();
    ok(@files > 0, 'get_test_files returns files');
    ok($files[0] =~ /\.json$/, 'get_test_files returns JSON files');
};

done_testing();
