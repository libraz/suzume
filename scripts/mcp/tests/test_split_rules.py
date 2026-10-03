"""Tests for split rules."""

from suzume_mcp.core.split_rules import apply_suzume_split


def _tok(surface, pos="名詞", **kw):
    t = {"surface": surface, "pos": pos, "lemma": surface}
    t.update(kw)
    return t


class TestFixedLeadingSearchUnit:
    def test_splits_closed_unit_from_following_noun(self):
        cases = [
            ("わが国", "わが", "連体詞", "国"),
            ("ひととおり目", "ひととおり", "副詞", "目"),
            ("以下確認", "以下", "接尾辞", "確認"),
            ("程度確認", "程度", "接尾辞", "確認"),
        ]

        for surface, leading, pos, noun in cases:
            result, rule = apply_suzume_split([_tok(surface)])
            assert result == [
                {"surface": leading, "pos": pos, "lemma": leading},
                {"surface": noun, "pos": "名詞", "lemma": noun},
            ]
            assert rule == "fixed-leading-search-unit"

    def test_keeps_standalone_closed_unit(self):
        result, rule = apply_suzume_split([_tok("わが", pos="連体詞")])
        assert result == [_tok("わが", pos="連体詞")]
        assert rule is None


class TestFixedFunctionSearchUnit:
    def test_keeps_closed_kanji_adverb_ending_in_ni(self):
        result, rule = apply_suzume_split([_tok("更に", pos="副詞")])
        assert result == [_tok("更に", pos="副詞")]
        assert rule is None

    def test_splits_kanji_nominal_and_ni_independent_of_reference_pos(self):
        for reference_pos in ("副詞", "名詞"):
            result, rule = apply_suzume_split([_tok("次に", pos=reference_pos)])
            assert result == [
                {"surface": "次", "pos": "名詞", "lemma": "次"},
                {"surface": "に", "pos": "助詞", "lemma": "に"},
            ]
        assert rule == "adverb-ni-split"


class TestDegreeSuffixGe:
    def test_splits_lexicalized_i_adjective_derivative(self, monkeypatch):
        monkeypatch.setattr(
            "suzume_mcp.core.split_rules.mecab_analyze",
            lambda text: [_tok(text, pos="形容詞", lemma=text)],
        )

        result, rule = apply_suzume_split([_tok("寂しげ", pos="名詞", pos_sub1="一般")])

        assert result == [
            {"surface": "寂し", "pos": "形容詞", "lemma": "寂しい"},
            {"surface": "げ", "pos": "名詞", "pos_sub1": "接尾", "lemma": "げ"},
        ]
        assert rule == "degree-suffix-ge"

    def test_keeps_unverified_noun_ending(self, monkeypatch):
        monkeypatch.setattr(
            "suzume_mcp.core.split_rules.mecab_analyze",
            lambda text: [_tok(text, pos="名詞")],
        )

        result, rule = apply_suzume_split([_tok("押しげ", pos="名詞", pos_sub1="一般")])

        assert result == [_tok("押しげ", pos="名詞", pos_sub1="一般")]
        assert rule is None


class TestInterrogativeNominalAdverb:
    def test_keeps_productive_morpheme_boundaries(self):
        result, rule = apply_suzume_split([_tok("いつの間にか", pos="副詞")])
        assert [token["surface"] for token in result] == ["いつ", "の", "間", "に", "か"]
        assert [token["pos"] for token in result] == ["名詞", "助詞", "名詞", "助詞", "助詞"]
        assert result[0]["pos_sub1"] == "代名詞"
        assert rule == "interrogative-nominal-adverb-boundary"


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
        tokens = [_tok("二次サンプル", pos="名詞")]
        result, rule = apply_suzume_split(tokens)
        assert len(result) == 2
        assert result[0]["surface"] == "二次"
        assert result[1]["surface"] == "サンプル"

    def test_skip_user_dict(self):
        tokens = [_tok("東京テスト", pos="名詞")]
        result, _ = apply_suzume_split(tokens)
        assert len(result) == 1
        assert result[0]["surface"] == "東京テスト"


class TestCopulaNegationSplit:
    def test_janai(self):
        tokens = [_tok("じゃない", pos="助動詞")]
        result, rule = apply_suzume_split(tokens)
        assert len(result) == 2
        assert result[0]["surface"] == "じゃ"
        assert result[1]["surface"] == "ない"
        assert rule == "copula-negation-split"


class TestLexicalizedCasePredicateSplit:
    def test_restores_particle_and_inflection_boundaries(self):
        cases = [
            ("気に入る", "気に入る", "入る"),
            ("気に入ら", "気に入る", "入る"),
            ("気に入っ", "気に入る", "入る"),
            ("気にいら", "気にいる", "いる"),
            ("間に合わ", "間に合う", "合う"),
        ]
        for surface, lemma, predicate_lemma in cases:
            result, rule = apply_suzume_split([_tok(surface, pos="動詞", lemma=lemma)])
            assert [token["surface"] for token in result] == [
                lemma.split("に", 1)[0],
                "に",
                surface.split("に", 1)[1],
            ]
            assert result[-1]["lemma"] == predicate_lemma
            assert rule == "lexicalized-morpheme-boundary"

    def test_restores_case_particle_before_negative_predicate(self):
        result, rule = apply_suzume_split([_tok("やむを得ない", pos="形容詞")])
        assert [token["surface"] for token in result] == ["やむ", "を", "得", "ない"]
        assert [token["pos"] for token in result] == ["動詞", "助詞", "動詞", "助動詞"]
        assert rule == "lexicalized-morpheme-boundary"

    def test_restores_genitive_particle_inside_noun_headword(self):
        result, rule = apply_suzume_split([_tok("年の瀬", pos="名詞")])
        assert [token["surface"] for token in result] == ["年", "の", "瀬"]
        assert rule == "lexicalized-morpheme-boundary"

    def test_restores_classical_negative_auxiliary(self):
        result, rule = apply_suzume_split([_tok("知らず", pos="名詞")])
        assert [token["surface"] for token in result] == ["知ら", "ず"]
        assert [token["pos"] for token in result] == ["動詞", "助動詞"]
        assert rule == "lexicalized-morpheme-boundary"

    def test_keeps_closed_compound_particle(self):
        tokens = [_tok("について", pos="助詞")]
        result, rule = apply_suzume_split(tokens)
        assert result == tokens
        assert rule is None


class TestKangoToshiteSplit:
    def test_adverbial_particle_is_not_part_of_the_lemma(self):
        result, rule = apply_suzume_split([_tok("依然として", pos="副詞")])
        assert result[:3] == [
            {"surface": "依然と", "pos": "副詞", "lemma": "依然"},
            {"surface": "し", "pos": "動詞", "lemma": "する"},
            {"surface": "て", "pos": "助詞", "lemma": "て"},
        ]
        assert rule == "kango-toshite-split"

    def test_volitional_quotative_suru_te_keeps_morpheme_boundaries(self):
        result, rule = apply_suzume_split([_tok("う", pos="助動詞"), _tok("として", pos="助詞")])
        assert result == [
            _tok("う", pos="助動詞"),
            {"surface": "と", "pos": "助詞", "lemma": "と"},
            {"surface": "し", "pos": "動詞", "lemma": "する"},
            {"surface": "て", "pos": "助詞", "lemma": "て"},
        ]
        assert rule == "quotative-suru-te-split"

    def test_closed_compound_particle_is_preserved_after_noun(self):
        result, rule = apply_suzume_split([_tok("代表", pos="名詞"), _tok("として", pos="助詞")])
        assert [token["surface"] for token in result] == ["代表", "として"]
        assert rule is None


class TestNaAdjectiveSaSuffixSplit:
    def test_lexicalized_nominalization_keeps_productive_suffix(self):
        result, rule = apply_suzume_split([_tok("華やかさ", pos="名詞")])
        assert result == [
            {"surface": "華やか", "pos": "形容詞", "lemma": "華やか"},
            {"surface": "さ", "pos": "接尾辞", "pos_sub1": "接尾", "lemma": "さ"},
        ]
        assert rule == "na-adjective-sa-suffix"


class TestProductiveTateSuffixSplit:
    def test_lexicalized_noun_is_split_by_inflection_shape(self):
        result, rule = apply_suzume_split([_tok("作りたて", pos="名詞")])
        assert result == [
            {"surface": "作り", "pos": "動詞", "lemma": "作る"},
            {"surface": "たて", "pos": "接尾辞", "pos_sub1": "接尾", "lemma": "たて"},
        ]
        assert rule == "productive-tate-suffix"

    def test_unrelated_tate_word_is_not_split(self):
        result, rule = apply_suzume_split([_tok("仕立て", pos="名詞")])
        assert [token["surface"] for token in result] == ["仕立て"]
        assert rule is None

    def test_compound_verb_stem_before_te_is_not_suffix(self):
        result, rule = apply_suzume_split([_tok("とりたて", pos="動詞", lemma="とりたてる"), _tok("て", pos="助詞")])
        assert [token["surface"] for token in result] == ["とりたて", "て"]
        assert rule is None


class TestProductiveCompletiveTsukusuSplit:
    def test_lexicalized_terminal_is_split_at_auxiliary_boundary(self):
        result, rule = apply_suzume_split([_tok("立ち尽くす", pos="動詞", lemma="立ち尽くす")])
        assert result == [
            {"surface": "立ち", "pos": "動詞", "lemma": "立つ"},
            {"surface": "尽くす", "pos": "助動詞", "lemma": "尽くす"},
        ]
        assert rule == "productive-completive-tsukusu"

    def test_lexicalized_continuative_is_split_productively(self):
        result, rule = apply_suzume_split([_tok("焼き尽くし", pos="動詞", lemma="焼き尽くす")])
        assert result == [
            {"surface": "焼き", "pos": "動詞", "lemma": "焼く"},
            {"surface": "尽くし", "pos": "助動詞", "lemma": "尽くす"},
        ]
        assert rule == "productive-completive-tsukusu"

    def test_independent_tsukusu_is_not_split(self):
        result, rule = apply_suzume_split([_tok("力尽くす", pos="動詞", lemma="尽くす")])
        assert [token["surface"] for token in result] == ["力尽くす"]
        assert rule is None


class TestProductiveCausativeConditionalSplit:
    def test_splits_lexicalized_nakunaru_at_inflection_boundary(self):
        result, rule = apply_suzume_split([_tok("無くなっ", pos="動詞", lemma="無くなる")])

        assert result == [
            {"surface": "無く", "pos": "形容詞", "lemma": "無い"},
            {"surface": "なっ", "pos": "動詞", "lemma": "なる"},
        ]
        assert rule == "nakunaru-inflection-boundary"

    def test_splits_lexicalized_causative_su_stem(self):
        result, rule = apply_suzume_split([_tok("待たさ", pos="動詞", lemma="待たす")])

        assert result == [
            {"surface": "待た", "pos": "動詞", "lemma": "待つ"},
            {"surface": "さ", "pos": "助動詞", "lemma": "す"},
        ]
        assert rule == "productive-causative-su-boundary"

    def test_lexicalized_godan_causative_keeps_auxiliary_boundary(self):
        result, rule = apply_suzume_split([_tok("遊ばせれ", pos="動詞", lemma="遊ばせる")])

        assert result == [
            {"surface": "遊ば", "pos": "動詞", "lemma": "遊ぶ"},
            {"surface": "せれ", "pos": "助動詞", "lemma": "せる"},
        ]
        assert rule == "productive-causative-conditional-boundary"

    def test_non_godan_lexeme_ending_in_seru_is_not_reconstructed(self):
        result, rule = apply_suzume_split([_tok("見せれ", pos="動詞", lemma="見せる")])

        assert result == [_tok("見せれ", pos="動詞", lemma="見せる")]
        assert rule is None


class TestProductiveCausativeVolitionalSplit:
    def test_splits_reference_suru_imperative_tail(self):
        tokens = [
            _tok("書か", pos="動詞", lemma="書く"),
            _tok("せよ", pos="動詞", lemma="する"),
            _tok("う", pos="助動詞"),
        ]

        result, rule = apply_suzume_split(tokens)

        assert result == [
            _tok("書か", pos="動詞", lemma="書く"),
            {"surface": "せ", "pos": "助動詞", "lemma": "せる"},
            {"surface": "よう", "pos": "助動詞", "lemma": "よう"},
        ]
        assert rule == "productive-causative-volitional-boundary"

    def test_splits_lexicalized_causative_stem(self):
        tokens = [_tok("泳がせよ", pos="動詞", lemma="泳がせる"), _tok("う", pos="助動詞")]

        result, rule = apply_suzume_split(tokens)

        assert result == [
            {"surface": "泳が", "pos": "動詞", "lemma": "泳ぐ"},
            {"surface": "せ", "pos": "助動詞", "lemma": "せる"},
            {"surface": "よう", "pos": "助動詞", "lemma": "よう"},
        ]
        assert rule == "productive-causative-volitional-boundary"


class TestLexicalizedNegativeAdjectiveSplit:
    def test_kanji_spelling_follows_its_kana_analysis(self):
        tokens = [
            _tok("は", pos="助詞"),
            _tok("済まない", pos="形容詞", lemma="済まない"),
        ]
        result, rule = apply_suzume_split(tokens)
        assert [token["surface"] for token in result] == ["は", "済ま", "ない"]
        assert result[1] == {"surface": "済ま", "pos": "動詞", "lemma": "済む"}
        assert result[2] == {"surface": "ない", "pos": "助動詞", "lemma": "ない"}
        assert rule == "lexicalized-morpheme-boundary"

    def test_ordinary_lexical_adjective_is_unchanged(self):
        tokens = [_tok("仕方", pos="名詞"), _tok("が", pos="助詞"), _tok("ない", pos="形容詞")]
        result, rule = apply_suzume_split(tokens)
        assert [token["surface"] for token in result] == ["仕方", "が", "ない"]
        assert rule is None


class TestNounNaiCompoundSplit:
    def test_i_row_stem_is_nominal_not_godan_negative(self):
        result, rule = apply_suzume_split([_tok("揺るぎない", pos="形容詞")])
        assert result == [
            {"surface": "揺るぎ", "pos": "名詞", "lemma": "揺るぎ"},
            {"surface": "ない", "pos": "形容詞", "lemma": "ない"},
        ]
        assert rule == "noun-nai-compound-split"


class TestLiteraryVolitionalParticleSplit:
    def test_does_not_reemit_original_after_an_earlier_rule(self):
        tokens = [
            _tok("じゃない", pos="助動詞"),
            _tok("むと", pos="名詞"),
        ]

        result, rule = apply_suzume_split(tokens)

        assert [token["surface"] for token in result] == ["じゃ", "ない", "む", "と"]
        assert rule == "copula-negation-split"


class TestNoSplit:
    def test_godan_sa_mizenkei_before_passive_is_preserved(self):
        tokens = [
            _tok("果たさ", pos="動詞", lemma="果たす", conj_form="未然形"),
            _tok("れ", pos="動詞", lemma="れる", pos_sub1="接尾"),
        ]
        result, rule = apply_suzume_split(tokens)
        assert [token["surface"] for token in result] == ["果たさ", "れ"]
        assert rule is None

    def test_passthrough(self):
        tokens = [_tok("食べ", pos="動詞"), _tok("た", pos="助動詞")]
        result, rule = apply_suzume_split(tokens)
        assert len(result) == 2
        assert rule is None
