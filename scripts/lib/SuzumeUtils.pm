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

# MeCab POS to Suzume POS mapping (raw mapping from MeCab)
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
    '代名詞'   => 'Pronoun',
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

# Slang verb stems → standard replacement for MeCab preprocessing
# バズる (godan ラ行) → 走る for correct conjugation parsing
our %SLANG_VERB_STEMS = (
    'バズ' => '走',  # バズった → 走った → 走っ+た → バズっ+た
);

# タリ活用副詞: stem + と → Adverb (MeCab splits as Noun + Particle)
our @TARI_ADVERB_STEMS = qw(
    泰然 堂々 悠々 淡々 粛々 颯爽 毅然 漫然 茫然 呆然 唖然 愕然
    断然 俄然 歴然 整然 雑然 騒然 憮然 黙然 昂然 凛然 厳然
);

# Fictional/unusual proper nouns → standard name for MeCab preprocessing
# がお (吾輩は猫である) → 吉田 for correct proper noun parsing
our %UNUSUAL_NAMES = (
    'がお' => '吉田',  # がおさん → 吉田さん → 吉田+さん → がお+さん
);

# Words that MeCab incorrectly splits but should stay together
# Use rare words as placeholders (must not be split by MeCab)
our %WORD_EXCEPTIONS = (
    '小供' => '供給',      # old spelling of 子供
    'とうきょう' => '瑠璃',  # hiragana Tokyo → rare word that stays together
    'どさり' => 'ゆっくり',  # onomatopoeia → standard adverb
    'にゃー' => 'ねえ',     # colloquial particle with elongation
    '打ち合わせ' => '会議',  # noun 打ち合わせ, not 打ち合わ+せ (causative)
    'おいで' => 'お出で',   # honorific おいで, not おい+で
    'ほんわか' => 'ゆっくり', # onomatopoeia → standard adverb
    'ありきたり' => '当たり前', # na-adjective → standard word
    'ばたり' => 'ゆっくり',  # onomatopoeia → standard adverb
    'がたり' => 'ゆっくり',  # onomatopoeia → standard adverb
    # Colloquial elongated/repeated patterns
    'すごいいいい' => 'すごい',  # vowel repeat → standard form
    'すごーーい' => 'すごい',    # choon repeat → standard form
    'かわいーー' => 'かわいい',  # choon repeat → standard form
    'もうってば' => 'もう',      # emphatic particle → simpler form
    'あなたったら' => 'あなた',  # emphatic topic → simpler form
    '無意識' => '意識',          # kanji compound → stays together
    '翌営業日' => '明日',        # business term → stays together
    'お疲れ様' => 'お願い',      # greeting → stays together
    '日付け' => '日付',          # date noun → stays together
    '再確認' => '確認',          # prefix+noun compound → stays together
    # Emphatic sokuon patterns (colloquial emphasis)
    # Note: Only add patterns that won't cause false matches
    'ですっ' => 'です',          # emphatic です
    'ますっっ' => 'ます',        # emphatic ます (double sokuon)
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
    return 'kanji-compound' if $text =~ /\p{Script=Han}{2,}/;

    # 7. Katakana compound: consecutive katakana stay together (no dictionary)
    return 'katakana-compound' if $text =~ /[\x{30A0}-\x{30FF}]{4,}/;

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

        # Calculate position in text (guard against length mismatch from preprocessing)
        my $pos_in_text = 0;
        for my $k (0 .. $i - 1) { $pos_in_text += length($tokens->[$k]{surface}); }
        my $remaining = $pos_in_text < length($text) ? substr($text, $pos_in_text) : '';

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

        # 2. Number + counter/katakana: 名詞,数 + (助数詞 or カタカナ名詞 or 万億兆)
        #    Suzume has no dictionary, so it cannot split number+katakana (e.g., 50アデナ)
        #    Also combines number+万/億/兆+円 patterns (e.g., 100万円, 3億5000万円)
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
                # Katakana noun after number (e.g., 50アデナ, 100ゴールド)
                my $is_katakana_noun = ($next->{pos} // '') eq '名詞'
                    && ($next->{surface} // '') =~ /^[\x{30A0}-\x{30FF}]+$/;
                # 中 suffix after counter (一日中, 一年中)
                my $is_chuu_suffix = ($next->{surface} // '') eq '中'
                    && ($next->{pos} // '') eq '名詞'
                    && ($next->{pos_sub1} // '') eq '接尾';
                # 目 suffix after counter (5回目, 3番目, 1人目)
                my $is_me_suffix = ($next->{surface} // '') eq '目'
                    && ($next->{pos} // '') eq '名詞'
                    && ($next->{pos_sub1} // '') eq '接尾';
                # Large number units: 万/億/兆 (名詞,数) to combine 100万円, 3億5000万円
                my $is_large_unit = ($next->{pos} // '') eq '名詞'
                    && ($next->{pos_sub1} // '') eq '数'
                    && ($next->{surface} // '') =~ /^[万億兆]$/;
                # Number after large unit (億/万/兆): continue 3億+5000+万+円 → 3億5000万円
                my $is_number_after_large = $combined =~ /[万億兆]$/
                    && ($next->{pos} // '') eq '名詞'
                    && ($next->{pos_sub1} // '') eq '数';
                if ($is_counter || $is_calendar_month || $is_katakana_noun || $is_chuu_suffix || $is_me_suffix || $is_large_unit || $is_number_after_large) {
                    $combined .= $next->{surface};
                    $j++;
                    # Stop after katakana noun, 中, or 目 (don't chain multiple)
                    last if $is_katakana_noun || $is_chuu_suffix || $is_me_suffix;
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

        # 2b. Prefix + number + counter: 接頭詞,数接続 + 名詞,数 (+ 助数詞)
        #     e.g., 第一回, 第10回
        if (!$merged && $t->{pos} eq '接頭詞' && ($t->{pos_sub1} // '') eq '数接続') {
            my $j = $i + 1;
            my $combined = $t->{surface};
            while ($j < @$tokens) {
                my $next = $tokens->[$j];
                my $is_number = ($next->{pos} // '') eq '名詞'
                    && ($next->{pos_sub1} // '') eq '数';
                my $is_counter = ($next->{pos} // '') eq '名詞'
                    && ($next->{pos_sub1} // '') eq '接尾'
                    && ($next->{pos_sub2} // '') eq '助数詞';
                if ($is_number || $is_counter) {
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

        # 2c. Noun + 書 suffix: 名詞 + 書(名詞,接尾,一般)
        #     e.g., 請求書, 申請書, 報告書, 契約書
        if (!$merged && $t->{pos} eq '名詞' && $i + 1 < @$tokens) {
            my $next = $tokens->[$i + 1];
            if (($next->{surface} // '') eq '書'
                && ($next->{pos} // '') eq '名詞'
                && ($next->{pos_sub1} // '') eq '接尾') {
                my $combined = $t->{surface} . '書';
                push @result, { surface => $combined, pos => '名詞', lemma => $combined };
                $i += 2;
                $merged = 1;
                $applied_rule //= 'noun+sho';
            }
        }

        # 2d. Prefix + Noun: 接頭詞,名詞接続 + 名詞 (kanji only)
        #     e.g., 本研究, 本論文, 各部門, 全社員, 未確認, 再起動
        #     Only merge when both prefix and noun are kanji (Suzume keeps these as 1 token)
        #     Mixed script like 本+サービス stays split
        if (!$merged && $t->{pos} eq '接頭詞' && ($t->{pos_sub1} // '') eq '名詞接続' && $i + 1 < @$tokens) {
            my $next = $tokens->[$i + 1];
            if (($next->{pos} // '') eq '名詞') {
                my $combined = $t->{surface} . $next->{surface};
                # Only merge if combined is all kanji (no katakana/hiragana mix)
                if ($combined =~ /^[\p{Han}]+$/) {
                    push @result, { surface => $combined, pos => '名詞', lemma => $combined };
                    $i += 2;
                    $merged = 1;
                    $applied_rule //= 'prefix+noun';
                }
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

        # 4. Elongated adjective: やば+ー+い → やばーい, かわい+ー → かわいー
        #    MeCab splits colloquial elongated adjectives, Suzume keeps them together
        #    Patterns: stem+ー+い, stem+ー+いparticle, stem+ー (no い)
        #    Lemma uses standard form (すごい, not すごーい)
        if (!$merged && $t->{pos} eq '形容詞' && ($t->{conj_form} // '') eq 'ガル接続') {
            my $j = $i + 1;
            if ($j < @$tokens && ($tokens->[$j]{surface} // '') eq 'ー') {
                my $combined = $t->{surface} . 'ー';
                my $lemma = $t->{lemma} // ($t->{surface} . 'い');  # standard form
                $j++;
                # Check what follows ー
                if ($j < @$tokens) {
                    my $next_surface = $tokens->[$j]{surface} // '';
                    if ($next_surface eq 'い') {
                        # stem+ー+い → stem+ーい
                        $combined .= 'い';
                        $j++;
                    } elsif ($next_surface =~ /^い(ね|よ|な|わ|ぞ|さ|か|の|けど)$/) {
                        # stem+ー+い+particle → stem+ーい + particle
                        my $particle = substr($next_surface, 1);
                        $combined .= 'い';
                        push @result, { surface => $combined, pos => '形容詞', lemma => $lemma };
                        push @result, { surface => $particle, pos => '助詞', lemma => $particle };
                        $i = $j + 1;
                        $merged = 1;
                        $applied_rule //= 'elongated-adjective';
                    }
                }
                # If not handled above, just merge stem+ー (e.g., かわいー)
                if (!$merged) {
                    push @result, { surface => $combined, pos => '形容詞', lemma => $lemma };
                    $i = $j;
                    $merged = 1;
                    $applied_rule //= 'elongated-adjective';
                }
            }
        }

        # 5. タリ活用副詞: merge 泰然 + と → 泰然と (Adverb)
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

        # 5a. Verb renyokei + 会: 飲み+会 → 飲み会 (compound noun)
        #     These are common compound nouns where verb renyokei + 会 forms a single word
        if (!$merged && $t->{pos} eq '動詞' && ($t->{conj_form} // '') eq '連用形') {
            my $j = $i + 1;
            if ($j < @$tokens) {
                my $next = $tokens->[$j];
                if (($next->{surface} // '') eq '会'
                    && ($next->{pos} // '') eq '名詞'
                    && ($next->{pos_sub1} // '') eq '接尾') {
                    my $combined = $t->{surface} . $next->{surface};
                    push @result, { surface => $combined, pos => '名詞', lemma => $combined };
                    $i = $j + 1;
                    $merged = 1;
                    $applied_rule //= 'verb-renyokei+kai';
                }
            }
        }

        # 5. Proper noun + region suffix: 東京+都+渋谷+区 → 東京都渋谷区
        #    Merge 名詞,固有名詞,地域 with 名詞,接尾,地域 sequences
        #    Exception: 行き (東京行き should stay split as 東京|行き)
        if (!$merged && $t->{pos} eq '名詞' && ($t->{pos_sub1} // '') eq '固有名詞'
            && ($t->{pos_sub2} // '') eq '地域') {
            my $j = $i + 1;
            my $combined = $t->{surface};
            while ($j < @$tokens) {
                my $next = $tokens->[$j];
                my $next_surface = $next->{surface} // '';
                # Skip 行き - it's a suffix but should not merge with place names
                last if $next_surface eq '行き';
                my $is_proper_region = ($next->{pos} // '') eq '名詞'
                    && ($next->{pos_sub1} // '') eq '固有名詞'
                    && ($next->{pos_sub2} // '') eq '地域';
                my $is_region_suffix = ($next->{pos} // '') eq '名詞'
                    && ($next->{pos_sub1} // '') eq '接尾'
                    && ($next->{pos_sub2} // '') eq '地域';
                if ($is_proper_region || $is_region_suffix) {
                    $combined .= $next->{surface};
                    $j++;
                } else {
                    last;
                }
            }
            if ($j > $i + 1) {
                push @result, { surface => $combined, pos => '名詞', pos_sub1 => '固有名詞', lemma => $combined };
                $i = $j;
                $merged = 1;
                $applied_rule //= 'proper-noun';
            }
        }

        # 6. Kanji compound: merge consecutive kanji-only noun tokens
        #    Skip: suffix tokens (的, 性, 化), adverbs (時々), proper nouns (佐藤+先生)
        #    Skip: adverbial nouns (毎日, 今日) - should not merge with following nouns
        my $is_mergeable_kanji = $t->{surface} =~ /^\p{Script=Han}+$/
            && $t->{pos} eq '名詞'
            && ($t->{pos_sub1} // '') ne '接尾'
            && ($t->{pos_sub1} // '') ne '固有名詞'
            && ($t->{pos_sub1} // '') ne '副詞可能';  # 毎日, 今日 etc.
        if (!$merged && $is_mergeable_kanji) {
            my $j = $i + 1;
            my $combined = $t->{surface};
            while ($j < @$tokens) {
                my $next = $tokens->[$j];
                my $next_mergeable = $next->{surface} =~ /^\p{Script=Han}+$/
                    && $next->{pos} eq '名詞'
                    && ($next->{pos_sub1} // '') ne '接尾'
                    && ($next->{pos_sub1} // '') ne '固有名詞'
                    && ($next->{pos_sub1} // '') ne '形容動詞語幹'  # 妙 etc.
                    && ($next->{pos_sub1} // '') ne '副詞可能';     # 毎日 etc.
                last unless $next_mergeable;
                $combined .= $next->{surface};
                $j++;
            }
            # Merge with common noun-forming suffixes
            # Suzume cannot split kanji+suffix without dictionary
            # 付け (日付け, 月付け), 者 (代表者, 責任者), 人 (日本人)
            if ($j < @$tokens) {
                my $next_surface = $tokens->[$j]{surface} // '';
                if ($next_surface =~ /^(付け|者|人)$/) {
                    $combined .= $next_surface;
                    $j++;
                }
            }
            if ($j > $i + 1) {
                push @result, { surface => $combined, pos => '名詞', lemma => $combined };
                $i = $j;
                $merged = 1;
                $applied_rule //= 'kanji-compound';
            }
        }

        # 7. Katakana compound: merge consecutive katakana-only tokens
        #    Suzume has no dictionary, so it cannot split katakana loanwords
        if (!$merged && $t->{surface} =~ /^[\x{30A0}-\x{30FF}]+$/ && $t->{pos} eq '名詞') {
            my $j = $i + 1;
            my $combined = $t->{surface};
            while ($j < @$tokens && $tokens->[$j]{surface} =~ /^[\x{30A0}-\x{30FF}]+$/
                   && $tokens->[$j]{pos} eq '名詞') {
                $combined .= $tokens->[$j]{surface};
                $j++;
            }
            if ($j > $i + 1) {
                push @result, { surface => $combined, pos => '名詞', lemma => $combined };
                $i = $j;
                $merged = 1;
                $applied_rule //= 'katakana-compound';
            }
        }

        # 8. Colloquial pronouns: merge どい+つ → どいつ, こい+つ → こいつ, etc.
        #    MeCab incorrectly splits these as verb+auxiliary (どく連用形+つ)
        #    These are single pronouns meaning "which/this/that/which one (person)"
        if (!$merged) {
            for my $pronoun (qw(どいつ こいつ そいつ あいつ)) {
                if ($remaining =~ /^\Q$pronoun\E/) {
                    my $len = 0;
                    my $j = $i;
                    while ($j < @$tokens && $len < length($pronoun)) {
                        $len += length($tokens->[$j]{surface});
                        $j++;
                    }
                    if ($len == length($pronoun)) {
                        push @result, { surface => $pronoun, pos => '代名詞', lemma => $pronoun };
                        $i = $j;
                        $merged = 1;
                        $applied_rule //= 'colloquial-pronoun';
                        last;
                    }
                }
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

    # Post-process: merge か+も → かも (compound particle)
    my @kamo_merged;
    my $skip_next = 0;
    for my $j (0 .. $#result) {
        if ($skip_next) {
            $skip_next = 0;
            next;
        }
        my $curr = $result[$j];
        if ($j < $#result
            && ($curr->{surface} // '') eq 'か'
            && ($curr->{pos} // '') eq '助詞'
            && ($result[$j + 1]{surface} // '') eq 'も'
            && ($result[$j + 1]{pos} // '') eq '助詞') {
            push @kamo_merged, { surface => 'かも', pos => '助詞', lemma => 'かも' };
            $skip_next = 1;
        } else {
            push @kamo_merged, $curr;
        }
    }
    @result = @kamo_merged;

    # Post-process: merge の+に → のに (conjunctive particle after た/だ)
    # MeCab splits のに in context as 名詞,非自立 + 格助詞, but grammatically
    # のに after past tense is the conjunctive particle meaning "even though/despite"
    my @noni_merged;
    $skip_next = 0;
    for my $j (0 .. $#result) {
        if ($skip_next) {
            $skip_next = 0;
            next;
        }
        my $curr = $result[$j];
        # Check for の + に pattern after た/だ (past tense)
        if ($j >= 1 && $j < $#result
            && ($curr->{surface} // '') eq 'の'
            && ($result[$j + 1]{surface} // '') eq 'に'
            && (($result[$j - 1]{surface} // '') =~ /^[たっだ]$/ ||
                ($result[$j - 1]{pos} // '') eq '助動詞')) {
            push @noni_merged, { surface => 'のに', pos => '助詞', lemma => 'のに' };
            $skip_next = 1;
            $applied_rule //= 'noni-merge';
        } else {
            push @noni_merged, $curr;
        }
    }
    @result = @noni_merged;

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

    # Post-process: split honorific patterns (Suzume has no dictionary, so it splits these)
    # Suzume splits: お客様 → お+客+様, 姉さん → 姉+さん, 皆様 → 皆+様
    my @split_result;
    my $honorific_re = qr/さん|ちゃん|様|君|殿|さま/;
    for my $t (@result) {
        my $surface = $t->{surface} // '';
        # Pattern: (お)? + kanji(s) + honorific
        if ($surface =~ /^(お)?(\p{Script=Han}+)($honorific_re)$/) {
            my ($prefix, $kanji, $suffix) = ($1, $2, $3);
            if (defined $prefix) {
                push @split_result, { surface => $prefix, pos => '接頭詞', lemma => $prefix };
            }
            push @split_result, { surface => $kanji, pos => '名詞', lemma => $kanji };
            push @split_result, { surface => $suffix, pos => '名詞', pos_sub1 => '接尾', lemma => $suffix };
            $applied_rule //= 'honorific-split';
        } else {
            push @split_result, $t;
        }
    }
    @result = @split_result;

    # Post-process: split お/ご+noun patterns (Suzume has no dictionary, splits prefix)
    # お菓子 → お+菓子, ご飯 → ご+飯
    # Skip: suffixes like ごろ, ごと (these should stay together)
    # Skip: お出で/おいで (honorific verb, should stay as one token)
    my @prefix_split;
    # Exceptions: words where お/ご is part of the lexeme, not a separable prefix
    # - お金/お前: MeCab treats as single token (not prefix+noun)
    # - お出で/おいで: honorific verb form
    # - おすすめ: recommendation (lexicalized)
    # - お疲れ様: greeting/interjection
    my %prefix_exceptions = (
        'お出で' => 1, 'おいで' => 1, 'おすすめ' => 1, 'お疲れ様' => 1,
        'お金' => 1, 'お前' => 1,
    );
    for my $t (@result) {
        my $surface = $t->{surface} // '';
        my $pos = $t->{pos} // '';
        my $pos_sub1 = $t->{pos_sub1} // '';
        # Pattern: お/ご + noun (kanji or hiragana), but not suffixes or exceptions
        if ($pos eq '名詞' && $pos_sub1 ne '接尾'
            && !$prefix_exceptions{$surface}
            && $surface =~ /^(お|ご)([\p{Script=Han}\p{Script=Hiragana}]+)$/) {
            my ($prefix, $noun) = ($1, $2);
            push @prefix_split, { surface => $prefix, pos => '接頭詞', lemma => $prefix };
            push @prefix_split, { surface => $noun, pos => '名詞', lemma => $noun };
            $applied_rule //= 'prefix-split';
        } else {
            push @prefix_split, $t;
        }
    }
    @result = @prefix_split;

    # Post-process: split filler tokens that can be grammatically analyzed
    # MeCab treats そうですね as フィラー, but Suzume analyzes grammatically
    my @filler_split;
    for my $t (@result) {
        my $surface = $t->{surface} // '';
        my $pos = $t->{pos} // '';
        # Split filler: そうですね → そう + です + ね
        if ($pos eq 'フィラー' && $surface =~ /^(そう)(です)(ね|か|よ|よね)?$/) {
            my ($sou, $desu, $particle) = ($1, $2, $3);
            push @filler_split, { surface => $sou, pos => '名詞', pos_sub1 => '形容動詞語幹', lemma => $sou };
            push @filler_split, { surface => $desu, pos => '助動詞', lemma => 'です' };
            if (defined $particle && $particle ne '') {
                push @filler_split, { surface => $particle, pos => '助詞', lemma => $particle };
            }
            $applied_rule //= 'filler-split';
        } else {
            push @filler_split, $t;
        }
    }
    @result = @filler_split;

    # Post-process: fix POS/lemmas for dialectal/special patterns
    # おいでなんし: おいで=Adverb, なん=Noun (dialectal), し=Verb (する連用形)
    for my $j (0 .. $#result) {
        my $curr = $result[$j];
        my $surface = $curr->{surface} // '';

        # おいで: Adverb (honorific imperative)
        if ($surface eq 'おいで' || $surface eq 'お出で') {
            $curr->{pos} = '副詞';
            $curr->{lemma} = 'おいで';
        }

        # なん + し pattern (dialectal)
        if ($j < $#result) {
            my $next = $result[$j + 1];
            if ($surface eq 'なん' && ($next->{surface} // '') eq 'し') {
                $curr->{pos} = '名詞';
                $curr->{lemma} = 'なん';
                $next->{pos} = '動詞';
                $next->{lemma} = 'する';
            }
        }
    }

    return (\@result, $applied_rule);
}

# Apply Suzume split rules: split MeCab's single tokens into multiple
# This handles cases where MeCab merges tokens that Suzume (correctly) splits
sub apply_suzume_split {
    my ($tokens) = @_;
    my @result;
    my $applied_rule;

    for my $t (@$tokens) {
        my $surface = $t->{surface};

        # 1. ったら topic particle: あなたったら → あなた|ったら
        # MeCab treats this as a single noun, but it's pronoun + particle
        if ($surface =~ /^(.+)(ったら)$/ && length($1) >= 3) {
            my ($stem, $particle) = ($1, $2);
            # Only split if stem is a pronoun-like word
            if ($stem =~ /^(あなた|おまえ|きみ|君|彼|彼女|あいつ|こいつ|誰|何|これ|それ|あれ)$/) {
                push @result, { surface => $stem, pos => '名詞', lemma => $stem };
                push @result, { surface => $particle, pos => '助詞', lemma => $particle };
                $applied_rule //= 'ttara-split';
                next;
            }
        }

        # 2. ってば emphatic particle: もうってば → もう|ってば
        if ($surface =~ /^(.+)(ってば)$/ && length($1) >= 2) {
            my ($stem, $particle) = ($1, $2);
            if ($stem =~ /^(もう|いい|だめ|ダメ|嫌|やだ)$/) {
                push @result, { surface => $stem, pos => '副詞', lemma => $stem };
                push @result, { surface => $particle, pos => '助詞', lemma => $particle };
                $applied_rule //= 'tteba-split';
                next;
            }
        }

        # 3. Unnatural kanji compounds: 時分学校 → 時分|学校
        # MeCab sometimes merges unrelated kanji sequences
        if ($surface =~ /^(時分)(学校)$/) {
            push @result, { surface => $1, pos => '名詞', lemma => $1 };
            push @result, { surface => $2, pos => '名詞', lemma => $2 };
            $applied_rule //= 'compound-split';
            next;
        }

        # No split needed
        push @result, $t;
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
        my $surface = $token->{surface} // '';

        # Surface-based POS overrides (for words MeCab misclassifies)
        # These words are adverbs but MeCab classifies them as 名詞 or 形容動詞語幹
        my %adverb_overrides = (
            'いかが' => 1,      # MeCab: 名詞,形容動詞語幹 → Adverb
            'いずれ' => 1,      # MeCab: 名詞,副詞可能 or 代名詞 → Adverb
            'しどろもどろ' => 1, # MeCab: 名詞,形容動詞語幹 → Adverb
            'なるほど' => 1,    # MeCab: 感動詞 → Adverb
            'たくさん' => 1,    # MeCab: 名詞,副詞可能 → Adverb
        );
        if ($adverb_overrides{$surface}) {
            return 'Adverb';
        }

        # Interjection overrides
        if ($surface eq 'お疲れ様') {
            return 'Interjection';
        }

        # Na-adjective stems → Adjective
        # MeCab classifies these as 名詞 but they function as na-adjectives
        my %na_adj_overrides = (
            # Hiragana na-adjectives (MeCab: 名詞,サ変接続)
            'しんちょう' => 1,  # 慎重 in hiragana
            'しずか' => 1,      # 静か in hiragana
            'おだやか' => 1,    # 穏やか in hiragana
            'げんき' => 1,      # 元気 in hiragana
            'きれい' => 1,      # 綺麗 in hiragana
            'ありきたり' => 1,  # 在り来たり (na-adjective)
            # Kanji na-adjectives (MeCab: 名詞,一般 but used with に particle)
            '無限' => 1,        # 無限に
        );
        if ($na_adj_overrides{$surface} && $pos eq '名詞') {
            return 'Adjective';
        }

        # 名詞,接尾,助動詞語幹 → Auxiliary (e.g., そう, よう)
        if ($pos eq '名詞' && $pos_sub1 eq '接尾' && $pos_sub2 eq '助動詞語幹') {
            return 'Auxiliary';
        }

        # 名詞,特殊,助動詞語幹 → Auxiliary (伝聞の「そう」)
        if ($pos eq '名詞' && $pos_sub1 eq '特殊' && $pos_sub2 eq '助動詞語幹') {
            return 'Auxiliary';
        }

        # 名詞,非自立,形容動詞語幹 (みたい) → Auxiliary (比況の助動詞)
        if ($pos eq '名詞' && $pos_sub1 eq '非自立' && $pos_sub2 eq '形容動詞語幹') {
            return 'Auxiliary';
        }

        # 動詞,非自立 → Auxiliary (subsidiary verbs: いる, ある, くる, いく, etc.)
        # Exception: すぎる stays as Verb (補助動詞 is grammatically a verb)
        # Exception: くださる stays as Verb (polite request verb, Suzume treats as VERB)
        if ($pos eq '動詞' && $pos_sub1 eq '非自立') {
            my $lemma = $token->{lemma} // '';
            return 'Verb' if $lemma eq 'すぎる';   # すぎる is subsidiary verb, not auxiliary
            return 'Verb' if $lemma eq 'くださる'; # ください is polite request verb
            return 'Auxiliary';
        }

        # 動詞,接尾 → Auxiliary (れる/られる=受身, せる/させる=使役)
        if ($pos eq '動詞' && $pos_sub1 eq '接尾') {
            return 'Auxiliary';
        }

        # 名詞,代名詞 → Pronoun (e.g., これ, それ, 私, 彼)
        # MeCab classifies pronouns as 名詞,代名詞 but Suzume treats them as Pronoun
        if ($pos eq '名詞' && $pos_sub1 eq '代名詞') {
            return 'Pronoun';
        }

        # 名詞,形容動詞語幹 → Adjective (e.g., 好き, 静か, 綺麗)
        # Suzume treats na-adjective stems as Adjective, not Noun
        if ($pos eq '名詞' && $pos_sub1 eq '形容動詞語幹') {
            return 'Adjective';
        }

        # 嫌い: always Adjective (ナ形容詞), not Verb
        # MeCab classifies standalone 嫌い as 動詞連用形 from 嫌う, but modern usage is na-adjective
        if (($token->{surface} // '') eq '嫌い' && $pos eq '動詞') {
            $token->{lemma} = '嫌い';
            return 'Adjective';
        }

        # 名詞,非自立,一般 の → Particle (準体助詞)
        # MeCab classifies nominalizer の as 名詞,非自立 but Suzume treats it as Particle
        if ($surface eq 'の' && $pos eq '名詞' && $pos_sub1 eq '非自立') {
            return 'Particle';
        }

        # 名詞,接尾 → Suffix (e.g., たち, さん, 的, さ)
        # Exception: 中 → Noun (多義的: 今日中, 中学校, 箱の中)
        if ($pos eq '名詞' && $pos_sub1 eq '接尾') {
            return 'Noun' if $surface eq '中';
            return 'Suffix';
        }

        # よく: always Adjective (連用形 of よい), not Adverb
        # MeCab inconsistently classifies よく as 副詞 or 形容詞 depending on context
        if ($surface eq 'よく' && $pos eq '副詞') {
            $token->{lemma} = 'よい';  # Also fix lemma
            return 'Adjective';
        }

        # 無い/無く (kanji): Adjective (存在否定の形容詞), not Auxiliary
        # ない (hiragana) as negative suffix remains Auxiliary
        if ($surface =~ /^無[いく]$/ && $pos eq '助動詞') {
            $token->{lemma} = '無い';
            return 'Adjective';
        }

        # という: Determiner (連体詞), not Particle
        # MeCab classifies as 助詞,格助詞,連語 but grammatically this is an adnominal
        # Note: といった stays as Particle (Suzume outputs PARTICLE)
        if ($surface eq 'という' && $pos eq '助詞') {
            return 'Determiner';
        }
    }

    # Otherwise map from Japanese
    return $POS_MAP{$pos} // 'Other';
}

# Normalize Suzume POS variations and apply Suzume-specific mappings
sub normalize_pos {
    my ($pos) = @_;

    # First: normalize case variations
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
        'DET'         => 'Determiner',
        'DETERMINER'  => 'Determiner',
        'ADNOMINAL'   => 'Adnominal',
        'PRON'        => 'Pronoun',
        'PRONOUN'     => 'Pronoun',
    );

    my $normalized = $norm_map{uc($pos)} // $pos;

    # Second: apply Suzume-specific mappings (MeCab POS → Suzume POS)
    # These override MeCab's classification to match Suzume's implementation
    my %suzume_override = (
        'Adnominal' => 'Determiner',  # 連体詞: MeCab=Adnominal, Suzume=Determiner
        # Pronoun is kept as-is (Suzume now supports pronouns)
    );

    return $suzume_override{$normalized} // $normalized;
}

# Correct MeCab POS misclassifications
sub correct_mecab_pos {
    my ($tokens) = @_;

    for my $t (@$tokens) {
        my $surface = $t->{surface};
        my $pos = $t->{pos};

        # Fix よく: always 形容詞 (連用形 of よい), not 副詞
        if ($surface eq 'よく' && $pos eq '副詞') {
            $t->{pos} = '形容詞';
            $t->{lemma} = 'よい';
        }

        # Fix particles misclassified as Noun
        if (exists $PARTICLE_CORRECTIONS{$surface} && $pos eq 'Noun') {
            $t->{pos} = $PARTICLE_CORRECTIONS{$surface};
        }

        # Fix 「の」(名詞,非自立) as Particle (準体助詞)
        # MeCab classifies nominalizer の as 名詞,非自立 but Suzume treats it as Particle
        if ($surface eq 'の' && $pos eq '名詞' && ($t->{pos_sub1} // '') eq '非自立') {
            $t->{pos} = 'Particle';
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
# Slang Preprocessing
# =============================================================================

# Replace slang stems with standard ones before MeCab
sub _preprocess_for_mecab {
    my ($text) = @_;
    my %replacements;

    # Slang adjectives: エモい, キモい, etc. (い-adjective conjugations)
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

    # Slang verbs: バズる, バズった, etc. (godan ラ行 conjugations)
    for my $slang (keys %SLANG_VERB_STEMS) {
        my $standard = $SLANG_VERB_STEMS{$slang};
        while ($text =~ /\Q$slang\E([らりるれろっ])/g) {
            my $match_start = $-[0];
            $replacements{$match_start} = {
                original => $slang,
                replacement => $standard,
                length => length($slang),
            };
        }
    }

    # Unusual names: がお → 吉田 (for proper noun + honorific parsing)
    for my $name (keys %UNUSUAL_NAMES) {
        my $standard = $UNUSUAL_NAMES{$name};
        while ($text =~ /\Q$name\E(さん|ちゃん|様|君|殿)/g) {
            my $match_start = $-[0];
            $replacements{$match_start} = {
                original => $name,
                replacement => $standard,
                length => length($name),
            };
        }
    }

    # Word exceptions: 小供 → 子供 (prevent MeCab from splitting)
    for my $word (keys %WORD_EXCEPTIONS) {
        my $standard = $WORD_EXCEPTIONS{$word};
        while ($text =~ /\Q$word\E/g) {
            my $match_start = $-[0];
            $replacements{$match_start} = {
                original => $word,
                replacement => $standard,
                length => length($word),
            };
        }
    }

    # Emphatic sokuon: 行くっ → 行く (but not 行くって)
    # Match verb+っ only when NOT followed by て
    my %emphatic_sokuon = (
        '行くっ' => '行く',
    );
    for my $pattern (keys %emphatic_sokuon) {
        my $standard = $emphatic_sokuon{$pattern};
        # Negative lookahead: match only if not followed by て
        while ($text =~ /\Q$pattern\E(?!て)/g) {
            my $match_start = $-[0];
            $replacements{$match_start} = {
                original => $pattern,
                replacement => $standard,
                length => length($pattern),
            };
        }
    }

    for my $pos (sort { $b <=> $a } keys %replacements) {
        my $r = $replacements{$pos};
        substr($text, $pos, $r->{length}) = $r->{replacement};
    }

    return ($text, \%replacements);
}

# Restore slang terms in tokens after MeCab processing
sub _postprocess_mecab_tokens {
    my ($tokens, $original_text, $replacements) = @_;
    return $tokens unless %$replacements;

    # Use pattern matching instead of position-based (positions shift with different-length replacements)
    for my $t (@$tokens) {
        my $surface = $t->{surface};
        for my $r (values %$replacements) {
            if ($surface =~ /\Q$r->{replacement}\E/) {
                $surface =~ s/\Q$r->{replacement}\E/$r->{original}/g;
                $t->{surface} = $surface;
            }
            if (defined $t->{lemma} && $t->{lemma} =~ /\Q$r->{replacement}\E/) {
                $t->{lemma} =~ s/\Q$r->{replacement}\E/$r->{original}/g;
            }
        }
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
    my ($merged, $merge_rule) = apply_suzume_merge($raw_tokens, $text);

    # Apply Suzume split rules (split MeCab's merged tokens)
    my ($split, $split_rule) = apply_suzume_split($merged);

    # Combine rule names
    my $applied_rule = $merge_rule // $split_rule;
    if ($merge_rule && $split_rule) {
        $applied_rule = "$merge_rule+$split_rule";
    }

    # Map POS to Suzume format and apply Suzume-specific normalization
    # Skip symbols (Suzume's basic mode excludes them from output)
    my @tokens;
    for my $t (@$split) {
        my $pos = normalize_pos(map_mecab_pos($t));
        next if $pos eq 'Symbol';  # Skip symbols
        push @tokens, {
            surface => $t->{surface},
            pos     => $pos,
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
