"""Tests for merge rules - individual pattern tests using mock token lists."""

from suzume_mcp.core.merge_rules import apply_suzume_merge


def _tok(surface, pos="名詞", **kw):
    """Helper to create a token dict."""
    t = {"surface": surface, "pos": pos, "lemma": surface}
    t.update(kw)
    return t


class TestDateMerge:
    def test_full_date(self):
        tokens = [_tok("2024"), _tok("年"), _tok("12"), _tok("月"), _tok("23"), _tok("日")]
        text = "2024年12月23日"
        result, rule = apply_suzume_merge(tokens, text)
        assert len(result) == 1
        assert result[0]["surface"] == "2024年12月23日"
        assert rule == "date"


class TestNumberUnit:
    def test_number_counter(self):
        tokens = [
            _tok("3", pos="名詞", pos_sub1="数"),
            _tok("人", pos="名詞", pos_sub1="接尾", pos_sub2="助数詞"),
        ]
        text = "3人"
        result, rule = apply_suzume_merge(tokens, text)
        assert len(result) == 1
        assert result[0]["surface"] == "3人"

    def test_large_number(self):
        tokens = [
            _tok("100", pos="名詞", pos_sub1="数"),
            _tok("万", pos="名詞", pos_sub1="数"),
            _tok("円", pos="名詞", pos_sub1="接尾", pos_sub2="助数詞"),
        ]
        text = "100万円"
        result, rule = apply_suzume_merge(tokens, text)
        assert len(result) == 1
        assert result[0]["surface"] == "100万円"


class TestKanjiCompound:
    def test_two_kanji(self):
        tokens = [_tok("経済", pos="名詞"), _tok("成長", pos="名詞")]
        text = "経済成長"
        result, rule = apply_suzume_merge(tokens, text)
        assert len(result) == 1
        assert result[0]["surface"] == "経済成長"

    def test_skip_suffix(self):
        """接尾 tokens should not merge."""
        tokens = [_tok("経済", pos="名詞"), _tok("的", pos="名詞", pos_sub1="接尾")]
        text = "経済的"
        result, _ = apply_suzume_merge(tokens, text)
        assert len(result) == 2


class TestKatakanaCompound:
    def test_katakana_merge(self):
        tokens = [_tok("セット", pos="名詞"), _tok("リスト", pos="名詞")]
        text = "セットリスト"
        result, rule = apply_suzume_merge(tokens, text)
        assert len(result) == 1
        assert result[0]["surface"] == "セットリスト"


class TestNaiAdjective:
    def test_merge_darashinai(self):
        tokens = [_tok("だらし", pos="名詞"), _tok("ない", pos="形容詞")]
        text = "だらしない"
        result, rule = apply_suzume_merge(tokens, text)
        assert len(result) == 1
        assert result[0]["surface"] == "だらしない"
        assert result[0]["pos"] == "形容詞"
        assert rule == "nai-adjective"


class TestTariAdverb:
    def test_merge_taizento(self):
        tokens = [_tok("泰然", pos="名詞"), _tok("と", pos="助詞")]
        text = "泰然と"
        result, rule = apply_suzume_merge(tokens, text)
        assert len(result) == 1
        assert result[0]["surface"] == "泰然と"
        assert result[0]["pos"] == "副詞"


class TestCompoundVerb:
    def test_merge_yomitsuzukeru(self):
        tokens = [
            _tok("読み", pos="動詞", conj_form="連用形"),
            _tok("続ける", pos="動詞", lemma="続ける"),
        ]
        text = "読み続ける"
        result, rule = apply_suzume_merge(tokens, text)
        assert len(result) == 1
        assert result[0]["surface"] == "読み続ける"
        assert rule == "compound-verb"


class TestURLMerge:
    def test_url(self):
        tokens = [
            _tok("https"),
            _tok(":"),
            _tok("//"),
            _tok("example"),
            _tok("."),
            _tok("com"),
        ]
        text = "https://example.com"
        result, rule = apply_suzume_merge(tokens, text)
        assert len(result) == 1
        assert result[0]["surface"] == "https://example.com"


class TestFamilyMerge:
    def test_o_niichan(self):
        tokens = [_tok("お", pos="接頭詞"), _tok("兄ちゃん", pos="名詞")]
        text = "お兄ちゃん"
        result, rule = apply_suzume_merge(tokens, text)
        assert len(result) == 1
        assert result[0]["surface"] == "お兄ちゃん"


class TestPostprocessKanjiMerge:
    def test_kanji_merge_post(self):
        """Post-process kanji merge after main pass."""
        tokens = [
            _tok("二", pos="名詞", pos_sub1="数"),  # Will not trigger #6 (pos_sub1=数)
            _tok("次", pos="名詞"),
        ]
        text = "二次"
        result, _ = apply_suzume_merge(tokens, text)
        # The post-process kanji merge should catch this
        assert any(t["surface"] == "二次" for t in result)


class TestZuNiMerge:
    def test_zu_ni(self):
        tokens = [_tok("ず", pos="助動詞"), _tok("に", pos="助詞")]
        text = "ずに"
        result, rule = apply_suzume_merge(tokens, text)
        assert len(result) == 1
        assert result[0]["surface"] == "ずに"
        assert rule == "zu-ni-merge"


class TestMechaMerge:
    def test_mecha(self):
        tokens = [_tok("め", pos="名詞"), _tok("ちゃ", pos="助詞")]
        text = "めちゃ"
        result, rule = apply_suzume_merge(tokens, text)
        assert len(result) == 1
        assert result[0]["surface"] == "めちゃ"


class TestColloquialPronoun:
    def test_koitsu(self):
        tokens = [_tok("こい", pos="動詞"), _tok("つ", pos="助動詞")]
        text = "こいつ"
        result, rule = apply_suzume_merge(tokens, text)
        assert len(result) == 1
        assert result[0]["surface"] == "こいつ"
        assert result[0]["pos"] == "代名詞"


class TestKamoMerge:
    def test_kamo(self):
        tokens = [_tok("か", pos="助詞"), _tok("も", pos="助詞")]
        text = "かも"
        result, _ = apply_suzume_merge(tokens, text)
        assert len(result) == 1
        assert result[0]["surface"] == "かも"
