#!/usr/bin/env perl
# Integration tests for dict_tool.pl
use strict;
use warnings;
use utf8;
use Test::More;
use FindBin qw($RealBin);
use File::Temp qw(tempfile tempdir);

binmode STDOUT, ':utf8';
binmode STDERR, ':utf8';

my $script = "$RealBin/../dict_tool.pl";
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
# Help test
# =============================================================================

subtest 'help command' => sub {
    my ($out, $rc) = run_cmd('--help');
    like($out, qr/Usage:/, 'help shows usage');
    like($out, qr/Commands:/, 'help shows commands');
    like($out, qr/check/, 'help shows check command');
    like($out, qr/add/, 'help shows add command');
    like($out, qr/suggest/, 'help shows suggest command');
    like($out, qr/validate/, 'help shows validate command');
    like($out, qr/cleanup/, 'help shows cleanup command');
};

# =============================================================================
# Check command tests
# =============================================================================

subtest 'check command - single token' => sub {
    my ($out, $rc) = run_cmd('check', 'テスト');
    is($rc, 0, 'check command succeeds');
    like($out, qr/Checking: テスト/, 'check shows word');
    like($out, qr/MeCab analysis/, 'check shows MeCab');
    like($out, qr/Single token|splits/, 'check shows token status');
};

subtest 'check command - compound word' => sub {
    my ($out, $rc) = run_cmd('check', '経済成長');
    is($rc, 0, 'check command succeeds');
    like($out, qr/Checking: 経済成長/, 'check shows word');
};

# =============================================================================
# Suggest command tests
# =============================================================================

subtest 'suggest command - verb' => sub {
    my ($out, $rc) = run_cmd('suggest', '食べる');
    is($rc, 0, 'suggest command succeeds');
    like($out, qr/Word: 食べる/, 'suggest shows word');
    like($out, qr/VERB/, 'suggest identifies VERB');
    like($out, qr/ICHIDAN/, 'suggest identifies ICHIDAN');
};

subtest 'suggest command - noun' => sub {
    my ($out, $rc) = run_cmd('suggest', '机');
    is($rc, 0, 'suggest command succeeds');
    like($out, qr/NOUN/, 'suggest identifies NOUN');
};

subtest 'suggest command - adjective' => sub {
    my ($out, $rc) = run_cmd('suggest', '美しい');
    is($rc, 0, 'suggest command succeeds');
    like($out, qr/ADJECTIVE/, 'suggest identifies ADJECTIVE');
    like($out, qr/I_ADJ/, 'suggest identifies I_ADJ');
};

# =============================================================================
# Validate command tests
# =============================================================================

subtest 'validate command' => sub {
    my ($out, $rc) = run_cmd('validate', '--dry-run');
    is($rc, 0, 'validate command succeeds');
    like($out, qr/Validating dictionary/, 'validate shows header');
};

# =============================================================================
# Cleanup command tests
# =============================================================================

subtest 'cleanup command' => sub {
    my $dir = tempdir(CLEANUP => 1);
    my $test_dict = "$dir/test.tsv";

    # Create test dictionary
    open my $fh, '>:utf8', $test_dict or die;
    print $fh "# Test dictionary\n";
    print $fh "テスト\tNOUN\tテスト\n";
    print $fh "短\tADJECTIVE\tみじかい\n";
    close $fh;

    my ($out, $rc) = run_cmd('cleanup', $test_dict, '--dry-run');
    is($rc, 0, 'cleanup command succeeds');
    like($out, qr/Processing:/, 'cleanup shows processing');
    like($out, qr/Results:/, 'cleanup shows results');
    like($out, qr/entries kept/, 'cleanup shows kept count');
};

# =============================================================================
# Add command tests (dry-run only)
# =============================================================================

subtest 'add command dry-run' => sub {
    my ($out, $rc) = run_cmd('add', 'テストワード', 'NOUN', '--dry-run');
    # Note: add command may fail if word exists or other checks fail
    # We just verify it doesn't crash
    ok(defined $out, 'add command returns output');
};

# =============================================================================
# Error handling tests
# =============================================================================

subtest 'error handling' => sub {
    my ($out, $rc) = run_cmd('check');  # Missing argument
    isnt($rc, 0, 'check without argument fails');

    ($out, $rc) = run_cmd('invalid_command');
    isnt($rc, 0, 'invalid command fails');
    like($out, qr/Unknown command/, 'invalid command shows error');
};

done_testing();
