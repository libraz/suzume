"""Tests for split rules."""

from suzume_mcp.core.split_rules import apply_suzume_split


def _tok(surface, pos="名詞", **kw):
    t = {"surface": surface, "pos": pos, "lemma": surface}
    t.update(kw)
    return t


class TestPluralRaSplit:
    def test_karera(self):
        tokens = [_tok("彼ら")]
        result, rule = apply_suzume_split(tokens)
        assert len(result) == 2
        assert result[0]["surface"] == "彼"
        assert result[1]["surface"] == "ら"
        assert rule == "ra-suffix-split"


class TestTtaraSplit:
    def test_anata_ttara(self):
        tokens = [_tok("あなたったら")]
        result, rule = apply_suzume_split(tokens)
        assert len(result) == 2
        assert result[0]["surface"] == "あなた"
        assert result[1]["surface"] == "ったら"


class TestNetaiSplit:
    def test_netai(self):
        tokens = [_tok("ねたい", pos="形容詞")]
        result, rule = apply_suzume_split(tokens)
        assert len(result) == 2
        assert result[0]["surface"] == "ね"
        assert result[0]["pos"] == "動詞"
        assert result[1]["surface"] == "たい"


class TestPrefectureCitySplit:
    def test_kanagawa_yokohama(self):
        tokens = [_tok("神奈川県横浜市")]
        result, rule = apply_suzume_split(tokens)
        assert len(result) == 2
        assert result[0]["surface"] == "神奈川県"
        assert result[1]["surface"] == "横浜市"


class TestKanjiKatakanaSplit:
    def test_split(self):
        tokens = [_tok("二次ロリ", pos="名詞")]
        result, rule = apply_suzume_split(tokens)
        assert len(result) == 2
        assert result[0]["surface"] == "二次"
        assert result[1]["surface"] == "ロリ"

    def test_skip_user_dict(self):
        tokens = [_tok("二次エロ", pos="名詞")]
        result, _ = apply_suzume_split(tokens)
        assert len(result) == 1
        assert result[0]["surface"] == "二次エロ"


class TestCopulaNegationSplit:
    def test_janai(self):
        tokens = [_tok("じゃない", pos="助動詞")]
        result, rule = apply_suzume_split(tokens)
        assert len(result) == 2
        assert result[0]["surface"] == "じゃ"
        assert result[1]["surface"] == "ない"
        assert rule == "copula-negation-split"


class TestNoSplit:
    def test_passthrough(self):
        tokens = [_tok("食べ", pos="動詞"), _tok("た", pos="助動詞")]
        result, rule = apply_suzume_split(tokens)
        assert len(result) == 2
        assert rule is None
