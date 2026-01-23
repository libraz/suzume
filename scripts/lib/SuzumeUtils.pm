package SuzumeUtils;
# Shared utilities for Suzume test and dictionary scripts

use strict;
use warnings;
use utf8;
use Exporter 'import';
use JSON::PP qw(decode_json);

our @EXPORT_OK = qw(
    mecab_analyze
    get_mecab_tokens
    get_mecab_surfaces
    get_expected_tokens
    get_suzume_rule
    get_suzume_surfaces
    get_suzume_debug_info
    get_char_types
    apply_suzume_merge
    map_mecab_pos
    normalize_pos
    tokens_match
    correct_mecab_pos
    format_expected
    print_tokens
    load_json
    save_json
    %POS_MAP
    %SLANG_ADJ_STEMS
    @NAI_ADJECTIVES
    @COUNTER_UNITS
    @TARI_ADVERB_STEMS
);

# =============================================================================
# Constants
# =============================================================================

# MeCab POS to Suzume POS mapping
our %POS_MAP = (
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
    '接尾辞'   => 'Suffix',
    '記号'     => 'Symbol',
    'フィラー' => 'Filler',
    'その他'   => 'Other',
);

# ナイ形容詞 (adjectives ending in ない that are single lexical units)
our @NAI_ADJECTIVES = qw(
    だらしない つまらない しょうがない もったいない くだらない
    せわしない やるせない いたたまれない あどけない おぼつかない
    はしたない みっともない ろくでもない どうしようもない
    ものたりない こころもとない
);

# Counter/unit patterns (unused - kept for reference, detection uses MeCab POS)
our @COUNTER_UNITS = qw(
    人 個 本 枚 台 回 件 円 年 月 日 時 分 秒 階 番 号 歳 才 目
    ページ 頁 時間 分間 秒間 年間 日間 週間 か月 ヶ月 カ月
);

# Slang adjective stems → standard replacement for MeCab preprocessing
our %SLANG_ADJ_STEMS = (
    'エモ' => '赤',
    'キモ' => '赤',
    'ウザ' => '赤',
    'ダサ' => '赤',
    'イタ' => '赤',
);

# タリ活用副詞: stem + と → Adverb (MeCab splits as Noun + Particle)
our @TARI_ADVERB_STEMS = qw(
    泰然 堂々 悠々 淡々 粛々 颯爽 毅然 漫然 茫然 呆然 唖然 愕然
    断然 俄然 歴然 整然 雑然 騒然 憮然 黙然 昂然 凛然 厳然
);

# Particles that MeCab may misclassify as Noun
my %PARTICLE_CORRECTIONS = (
    'の' => 'Particle',
    'が' => 'Particle',
    'を' => 'Particle',
    'に' => 'Particle',
    'へ' => 'Particle',
    'で' => 'Particle',
    'と' => 'Particle',
    'から' => 'Particle',
    'まで' => 'Particle',
    'より' => 'Particle',
    'ほど' => 'Particle',
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
    'くらい' => 'Particle',
    'ぐらい' => 'Particle',
    'など' => 'Particle',
    'なんか' => 'Particle',
    'なんて' => 'Particle',
    'って' => 'Particle',
);

# =============================================================================
# MeCab Interface
# =============================================================================

sub mecab_analyze {
    my ($text) = @_;

    my $escaped = $text;
    $escaped =~ s/'/'\\''/g;
    my $output = `echo '$escaped' | mecab 2>/dev/null`;
    utf8::decode($output);

    my @tokens;
    for my $line (split /\n/, $output) {
        last if $line eq 'EOS' || $line eq '';
        my ($surface, $features) = split /\t/, $line;
        next unless defined $surface && $surface ne '';

        my @f = split /,/, ($features // '');
        push @tokens, {
            surface   => $surface,
            pos       => $f[0] // '',
            pos_sub1  => $f[1] // '',
            pos_sub2  => $f[2] // '',
            pos_sub3  => $f[3] // '',
            conj_type => $f[4] // '',
            conj_form => $f[5] // '',
            lemma     => $f[6] // $surface,
            reading   => $f[7] // '',
        };
    }
    return \@tokens;
}

# =============================================================================
# Suzume Rule Detection
# =============================================================================

# Check if text matches Suzume normalization rules
# Returns: rule name if matched, empty string otherwise
sub get_suzume_rule {
    my ($text) = @_;

    # 1. ナイ形容詞 (adjectives ending in ない that are single words)
    for my $adj (@NAI_ADJECTIVES) {
        return 'nai-adjective' if $text =~ /\Q$adj\E/;
    }

    # 2. Number + counter/unit: detected via MeCab POS (名詞,数 + 名詞,接尾,助数詞)
    # This heuristic just checks for number followed by non-digit for reporting
    return 'number+unit' if $text =~ /\d+[^\d\s]/;

    # 3. Date patterns: "2024年12月23日"
    return 'date' if $text =~ /\d+年\d+月\d+日/;

    # 4. Slang/new adjectives: エモい, キモい, ウザい, やばい
    for my $stem (keys %SLANG_ADJ_STEMS) {
        return 'slang-adjective' if $text =~ /\Q$stem\E[いかくけさ]/;
    }
    return 'slang-adjective' if $text =~ /やばい|やばかっ|やばく/;

    # 5. タリ活用副詞: 泰然と, 堂々と, etc. (stem + と → Adverb)
    for my $stem (@TARI_ADVERB_STEMS) {
        return 'tari-adverb' if $text =~ /\Q$stem\Eと/;
    }

    # 6. Kanji compound: consecutive kanji stay together
    return 'kanji-compound' if $text =~ /\p{Han}{2,}/;

    return '';
}

# =============================================================================
# Token Merging (Apply Suzume Rules to MeCab Output)
# =============================================================================

# Apply Suzume merge rules to MeCab tokens (using MeCab POS for detection)
# Returns: (merged tokens, applied rule name or undef)
sub apply_suzume_merge {
    my ($tokens, $text) = @_;
    my @result;
    my $i = 0;
    my $applied_rule;

    while ($i < @$tokens) {
        my $t = $tokens->[$i];
        my $merged = 0;

        # Calculate position in text
        my $pos_in_text = 0;
        for my $k (0 .. $i - 1) { $pos_in_text += length($tokens->[$k]{surface}); }
        my $remaining = substr($text, $pos_in_text);

        # 1. Full date pattern: 2024年12月23日 → single token
        if (!$merged && $remaining =~ /^(\d+年\d+月\d+日)/) {
            my $date = $1;
            my $len = 0;
            my $j = $i;
            while ($j < @$tokens && $len < length($date)) {
                $len += length($tokens->[$j]{surface});
                $j++;
            }
            if ($len == length($date)) {
                push @result, { surface => $date, pos => '名詞', lemma => $date };
                $i = $j;
                $merged = 1;
                $applied_rule //= 'date';
            }
        }

        # 2. Number + counter: 名詞,数 + 名詞,接尾,助数詞 (MeCab POS-based detection)
        #    Also handles special cases: calendar months (1-12月) where MeCab misclassifies 月
        if (!$merged && $t->{pos} eq '名詞' && ($t->{pos_sub1} // '') eq '数') {
            my $j = $i + 1;
            my $combined = $t->{surface};
            while ($j < @$tokens) {
                my $next = $tokens->[$j];
                # Check for counter suffix: 名詞,接尾,助数詞
                my $is_counter = ($next->{pos} // '') eq '名詞'
                    && ($next->{pos_sub1} // '') eq '接尾'
                    && ($next->{pos_sub2} // '') eq '助数詞';
                # Special case: calendar month (1-12月) - MeCab classifies 月 as 名詞,一般
                my $is_calendar_month = ($next->{surface} // '') eq '月'
                    && $combined =~ /^(?:1[0-2]|[1-9])$/;
                if ($is_counter || $is_calendar_month) {
                    $combined .= $next->{surface};
                    $j++;
                } else {
                    last;
                }
            }
            if ($j > $i + 1) {
                push @result, { surface => $combined, pos => '名詞', lemma => $combined };
                $i = $j;
                $merged = 1;
                $applied_rule //= 'number+unit';
            }
        }

        # 3. Nai-adjective: merge だらし+ない → だらしない
        #    Exception: MeCab splits these as Noun+Adjective, but merged form is Adjective
        if (!$merged) {
            for my $adj (@NAI_ADJECTIVES) {
                if ($remaining =~ /^\Q$adj\E/) {
                    my $len = 0;
                    my $j = $i;
                    while ($j < @$tokens && $len < length($adj)) {
                        $len += length($tokens->[$j]{surface});
                        $j++;
                    }
                    if ($len == length($adj)) {
                        # Explicitly set as 形容詞 (MeCab would classify stem as 名詞)
                        push @result, { surface => $adj, pos => '形容詞', lemma => $adj };
                        $i = $j;
                        $merged = 1;
                        $applied_rule //= 'nai-adjective';
                        last;
                    }
                }
            }
        }

        # 4. タリ活用副詞: merge 泰然 + と → 泰然と (Adverb)
        #    Exception: MeCab splits these as Noun+Particle, but merged form is Adverb
        if (!$merged) {
            for my $stem (@TARI_ADVERB_STEMS) {
                my $adverb = $stem . 'と';
                if ($remaining =~ /^\Q$adverb\E/) {
                    my $len = 0;
                    my $j = $i;
                    while ($j < @$tokens && $len < length($adverb)) {
                        $len += length($tokens->[$j]{surface});
                        $j++;
                    }
                    if ($len == length($adverb)) {
                        # Explicitly set as 副詞 (MeCab would classify stem as 名詞)
                        push @result, { surface => $adverb, pos => '副詞', lemma => $stem };
                        $i = $j;
                        $merged = 1;
                        $applied_rule //= 'tari-adverb';
                        last;
                    }
                }
            }
        }

        # 5. Kanji compound: merge consecutive kanji-only tokens
        #    Skip suffix tokens (的, 性, 化, etc.) - they should remain separate
        if (!$merged && $t->{surface} =~ /^\p{Han}+$/ && ($t->{pos_sub1} // '') ne '接尾') {
            my $j = $i + 1;
            my $combined = $t->{surface};
            while ($j < @$tokens && $tokens->[$j]{surface} =~ /^\p{Han}+$/
                   && ($tokens->[$j]{pos_sub1} // '') ne '接尾') {
                $combined .= $tokens->[$j]{surface};
                $j++;
            }
            if ($j > $i + 1) {
                push @result, { surface => $combined, pos => '名詞', lemma => $combined };
                $i = $j;
                $merged = 1;
                $applied_rule //= 'kanji-compound';
            }
        }

        if (!$merged) {
            push @result, {
                surface => $t->{surface},
                pos     => $t->{pos},  # Keep raw POS for later mapping
                pos_sub1 => $t->{pos_sub1},
                pos_sub2 => $t->{pos_sub2},
                lemma   => $t->{lemma} // $t->{surface}
            };
            $i++;
        }
    }

    # Post-process: fix epenthetic さ in adjective+さ+そう pattern (なさそう, よさそう, etc.)
    # MeCab incorrectly classifies this さ as 名詞,接尾,特殊 but it should be Suffix
    for my $j (1 .. $#result - 1) {
        my $prev = $result[$j - 1];
        my $curr = $result[$j];
        my $next = $result[$j + 1];
        if (($prev->{pos} // '') eq '形容詞'
            && ($curr->{surface} // '') eq 'さ'
            && ($curr->{pos_sub1} // '') eq '接尾'
            && ($next->{surface} // '') eq 'そう') {
            $curr->{pos} = 'Suffix';  # Override to correct POS
            delete $curr->{pos_sub1};
            delete $curr->{pos_sub2};
        }
    }

    return (\@result, $applied_rule);
}

# =============================================================================
# POS Mapping and Correction
# =============================================================================

# Map MeCab POS to Suzume POS
sub map_mecab_pos {
    my ($token) = @_;

    # Handle both hash with 'pos' key and raw POS string
    my $pos = ref($token) eq 'HASH' ? ($token->{pos} // '') : $token;

    # If already an English POS (value in %POS_MAP), return as-is
    my %valid_pos = map { $_ => 1 } values %POS_MAP;
    return $pos if $valid_pos{$pos};

    # Special handling for MeCab subcategories
    if (ref($token) eq 'HASH') {
        my $pos_sub1 = $token->{pos_sub1} // '';
        my $pos_sub2 = $token->{pos_sub2} // '';

        # 名詞,接尾,特殊 → Suffix (e.g., さ in 美しさ, 高さ)
        if ($pos eq '名詞' && $pos_sub1 eq '接尾' && $pos_sub2 eq '特殊') {
            return 'Suffix';
        }

        # 名詞+形容動詞語幹/助動詞語幹 → Auxiliary (e.g., みたい, そう, よう)
        if ($pos eq '名詞' && ($pos_sub2 eq '形容動詞語幹' || $pos_sub2 eq '助動詞語幹')) {
            return 'Auxiliary';
        }
    }

    # Otherwise map from Japanese
    return $POS_MAP{$pos} // 'Other';
}

# Normalize Suzume POS variations
sub normalize_pos {
    my ($pos) = @_;

    my %norm_map = (
        'NOUN'        => 'Noun',
        'VERB'        => 'Verb',
        'ADJ'         => 'Adjective',
        'ADJECTIVE'   => 'Adjective',
        'ADV'         => 'Adverb',
        'ADVERB'      => 'Adverb',
        'PARTICLE'    => 'Particle',
        'PART'        => 'Particle',
        'AUX'         => 'Auxiliary',
        'AUXILIARY'   => 'Auxiliary',
        'CONJ'        => 'Conjunction',
        'CONJUNCTION' => 'Conjunction',
        'INTJ'        => 'Interjection',
        'INTERJECTION'=> 'Interjection',
        'SYMBOL'      => 'Symbol',
        'OTHER'       => 'Other',
        'PREFIX'      => 'Prefix',
        'SUFFIX'      => 'Suffix',
    );

    return $norm_map{uc($pos)} // $pos;
}

# Correct MeCab POS misclassifications
sub correct_mecab_pos {
    my ($tokens) = @_;

    for my $t (@$tokens) {
        my $surface = $t->{surface};
        my $pos = $t->{pos};

        # Fix particles misclassified as Noun
        if (exists $PARTICLE_CORRECTIONS{$surface} && $pos eq 'Noun') {
            $t->{pos} = $PARTICLE_CORRECTIONS{$surface};
        }
    }
}

# =============================================================================
# Token Comparison
# =============================================================================

# Compare two token arrays for equality (surface, pos, lemma)
sub tokens_match {
    my ($a, $b) = @_;
    return 0 unless @$a == @$b;

    for my $i (0 .. $#$a) {
        return 0 if ($a->[$i]{surface} // '') ne ($b->[$i]{surface} // '');

        my $pos_a = normalize_pos($a->[$i]{pos} // '');
        my $pos_b = normalize_pos($b->[$i]{pos} // '');
        return 0 if $pos_a ne $pos_b;

        my $lemma_a = $a->[$i]{lemma} // $a->[$i]{surface};
        my $lemma_b = $b->[$i]{lemma} // $b->[$i]{surface};
        return 0 if $lemma_a ne $lemma_b;
    }
    return 1;
}

# =============================================================================
# Slang Adjective Preprocessing
# =============================================================================

# Replace slang adjective stems with standard ones before MeCab
sub _preprocess_for_mecab {
    my ($text) = @_;
    my %replacements;

    for my $slang (keys %SLANG_ADJ_STEMS) {
        my $standard = $SLANG_ADJ_STEMS{$slang};
        while ($text =~ /\Q$slang\E([いかくけさ])/g) {
            my $match_start = $-[0];
            $replacements{$match_start} = {
                original => $slang,
                replacement => $standard,
                length => length($slang),
            };
        }
    }

    for my $pos (sort { $b <=> $a } keys %replacements) {
        my $r = $replacements{$pos};
        substr($text, $pos, $r->{length}) = $r->{replacement};
    }

    return ($text, \%replacements);
}

# Restore slang adjectives in tokens after MeCab processing
sub _postprocess_mecab_tokens {
    my ($tokens, $original_text, $replacements) = @_;
    return $tokens unless %$replacements;

    my $pos = 0;
    for my $t (@$tokens) {
        my $surface = $t->{surface};
        for my $repl_pos (keys %$replacements) {
            my $r = $replacements->{$repl_pos};
            if ($pos <= $repl_pos && $repl_pos < $pos + length($surface)) {
                my $offset = $repl_pos - $pos;
                my $new_surface = $surface;
                substr($new_surface, $offset, length($r->{replacement})) = $r->{original};
                $t->{surface} = $new_surface;
                if (defined $t->{lemma} && $t->{lemma} =~ /\Q$r->{replacement}\E/) {
                    $t->{lemma} =~ s/\Q$r->{replacement}\E/$r->{original}/;
                }
            }
        }
        $pos += length($surface);
    }
    return $tokens;
}

# =============================================================================
# High-Level MeCab Interface
# =============================================================================

# Get MeCab tokens with slang adjective handling and POS mapping
sub get_mecab_tokens {
    my ($text) = @_;

    my ($processed_text, $replacements) = _preprocess_for_mecab($text);
    my $raw_tokens = mecab_analyze($processed_text);

    my @tokens;
    for my $t (@$raw_tokens) {
        push @tokens, {
            surface => $t->{surface},
            pos     => map_mecab_pos($t),
            lemma   => $t->{lemma} // $t->{surface},
        };
    }

    _postprocess_mecab_tokens(\@tokens, $text, $replacements);
    return \@tokens;
}

# Get expected tokens: MeCab + Suzume rule corrections
sub get_expected_tokens {
    my ($text, $suzume_tokens) = @_;

    # Get raw MeCab tokens (with pos_sub1, pos_sub2 for counter detection)
    my ($processed_text, $replacements) = _preprocess_for_mecab($text);
    my $raw_tokens = mecab_analyze($processed_text);
    _postprocess_mecab_tokens($raw_tokens, $text, $replacements);

    # Apply Suzume merge rules using MeCab POS info
    my ($merged, $applied_rule) = apply_suzume_merge($raw_tokens, $text);

    # Map POS to Suzume format
    my @tokens;
    for my $t (@$merged) {
        push @tokens, {
            surface => $t->{surface},
            pos     => map_mecab_pos($t),
            lemma   => $t->{lemma} // $t->{surface},
        };
    }

    if ($applied_rule) {
        return (\@tokens, 'MeCab+SuzumeRules', $applied_rule);
    }

    # Check for slang adjective rule
    if ($text =~ /(?:エモ|キモ|ウザ|ダサ|イタ)[いかくけさ]/) {
        return (\@tokens, 'MeCab', 'slang-adjective');
    }

    return (\@tokens, 'MeCab', '');
}

# =============================================================================
# Token Formatting and Display
# =============================================================================

# Format tokens for JSON output
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

# Print tokens in human-readable format
sub print_tokens {
    my ($tokens, $label) = @_;
    print "$label:\n";
    for my $t (@$tokens) {
        my $lemma_str = ($t->{lemma} // '') ne $t->{surface} ? " ($t->{lemma})" : "";
        print "  $t->{surface}\t$t->{pos}$lemma_str\n";
    }
}

# =============================================================================
# JSON File Operations
# =============================================================================

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
    open my $fh, '>:utf8', $file or die "Cannot write $file: $!\n";
    print $fh $json->encode($data);
    close $fh;
}

# =============================================================================
# Suzume CLI Interface
# =============================================================================

# Get surfaces only from Suzume CLI output
sub get_suzume_surfaces {
    my ($cli, $text) = @_;
    return [] unless defined $cli && -x $cli;
    my $escaped = $text;
    $escaped =~ s/'/'\\''/g;
    my $output = `'$cli' '$escaped' 2>/dev/null`;
    utf8::decode($output);
    my @surfaces;
    for my $line (split /\n/, $output) {
        next if $line eq '' || $line eq 'EOS';
        my ($surface) = split /\t/, $line;
        push @surfaces, $surface if defined $surface && $surface ne '';
    }
    return \@surfaces;
}

# Get surfaces only from MeCab output
sub get_mecab_surfaces {
    my ($text) = @_;
    my $escaped = $text;
    $escaped =~ s/'/'\\''/g;
    my $output = `echo '$escaped' | mecab 2>/dev/null`;
    utf8::decode($output);
    my @surfaces;
    for my $line (split /\n/, $output) {
        last if $line eq 'EOS' || $line eq '';
        my ($surface) = split /\t/, $line;
        push @surfaces, $surface if defined $surface && $surface ne '';
    }
    return \@surfaces;
}

# Get debug info from Suzume CLI (SUZUME_DEBUG=2)
sub get_suzume_debug_info {
    my ($cli, $text) = @_;
    return {} unless defined $cli && -x $cli;
    my $escaped = $text;
    $escaped =~ s/'/'\\''/g;
    my $output = `SUZUME_DEBUG=2 '$cli' '$escaped' 2>&1`;
    utf8::decode($output);

    my %info = (best_path => '', total_cost => 0, margin => 0, tokens => [], connections => []);
    if ($output =~ /\[VITERBI\] Best path \(cost=([-\d.]+)\): (.+?) \[margin=([-\d.]+)\]/) {
        $info{total_cost} = $1;
        $info{best_path} = $2;
        $info{margin} = $3;
    }
    my @path_parts = split / → /, $info{best_path};
    for my $part (@path_parts) {
        if ($part =~ /"(.+?)"\((\w+)\/(\w+)\)/) {
            push @{$info{tokens}}, { surface => $1, pos => $2, epos => $3 };
        }
    }
    while ($output =~ /\[CONN\] "(.+?)" \((\w+)\/\w+\) → "(.+?)" \((\w+)\/\w+\): bigram=([-\d.]+) epos_adj=([-\d.]+) \(([^)]+)\) total=([-\d.]+)/g) {
        push @{$info{connections}}, {
            from_surface => $1, from_pos => $2, to_surface => $3, to_pos => $4,
            bigram => $5, epos_adj => $6, reason => $7, total => $8,
        };
    }
    my @word_costs;
    while ($output =~ /\[WORD\] "(.+?)" \(([^)]+)\) cost=([-\d.]+) \(from edge\) \[cat=([-\d.]+) edge=([-\d.]+) epos=([^\]]+)\]/g) {
        push @word_costs, { surface => $1, source => $2, cost => $3, cat_cost => $4, edge_cost => $5, epos => $6 };
    }
    $info{word_costs} = \@word_costs;
    return \%info;
}

# =============================================================================
# Character Type Analysis
# =============================================================================

# Get character types present in a string
sub get_char_types {
    my ($str) = @_;
    my %types;
    for my $char (split //, $str) {
        my $ord = ord($char);
        if ($ord >= 0x3040 && $ord <= 0x309F) { $types{hiragana}++; }
        elsif ($ord >= 0x30A0 && $ord <= 0x30FF) { $types{katakana}++; }
        elsif ($ord >= 0x4E00 && $ord <= 0x9FFF) { $types{kanji}++; }
        elsif ($ord >= 0x0041 && $ord <= 0x007A) { $types{alpha}++; }
        elsif ($ord >= 0x0030 && $ord <= 0x0039) { $types{digit}++; }
    }
    return keys %types;
}

1;  # Module must return true
