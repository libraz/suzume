"""Repairs for contracted, regional, and lengthened forms that MeCab splits at the wrong boundary."""

from .mecab import mecab_analyze


def repair_contracted_volitional(tokens: list[dict]) -> None:
    """Rebuild the volitional う contracted to っ before the question particle.

    行こっか, 食べよっか and 帰っとこっか are 行こう, 食べよう and 帰っとこう
    with the う pulled into a geminate by か. The reference dictionary reads the
    geminate as a verb of its own (よっ as よる, こっ as こう), so the prefix is
    re-analyzed with the plain う, which selects the volitional cell, and the
    geminate takes that う's place.
    """
    idx = 1
    repaired_end = 0
    while idx < len(tokens):
        token, previous = tokens[idx], tokens[idx - 1]
        if (
            token.get("surface") != "か"
            or token.get("pos") != "助詞"
            or previous.get("pos") != "動詞"
            or not previous.get("surface", "").endswith("っ")
        ):
            idx += 1
            continue
        prefix = "".join(t.get("surface", "") for t in tokens[repaired_end:idx])[:-1]
        volitional = mecab_analyze(prefix + "う")
        if (
            len(volitional) < 2
            or volitional[-1].get("surface") != "う"
            or volitional[-1].get("pos") != "助動詞"
            or volitional[-2].get("conj_form") != "未然ウ接続"
        ):
            idx += 1
            continue
        geminate = {**volitional[-1], "surface": "っ", "reading": "ッ"}
        tokens[repaired_end:idx] = [*volitional[:-1], geminate]
        repaired_end += len(volitional)
        idx = repaired_end + 1


def repair_regional_imperative(tokens: list[dict]) -> None:
    """Rebuild two regional imperatives the reference dictionary does not know.

    The Chugoku んさい is なさい contracted (食べんさい, 書きんさい): one honorific
    imperative on the continuative, read instead as an attributive verb in ん
    plus the noun さい. The Kansai continuative imperative of とく drawn out by
    its own vowel (見ときい, しときい) is read as と plus a noun きい; holding a
    final vowel keeps it one word, as for any other lengthening.
    """
    idx = 1
    repaired_end = 0
    while idx < len(tokens):
        token, previous = tokens[idx], tokens[idx - 1]
        following = tokens[idx + 1] if idx + 1 < len(tokens) else None
        clause_end = following is None or following.get("pos") == "記号"
        if (
            token.get("surface") == "さい"
            and token.get("pos") == "名詞"
            and previous.get("pos") == "動詞"
            and previous.get("surface", "").endswith("ん")
        ):
            prefix = "".join(t.get("surface", "") for t in tokens[repaired_end:idx])[:-1]
            continuative = mecab_analyze(prefix + "ます")
            if continuative and continuative[-1].get("surface") == "ます":
                honorific = {
                    "surface": "んさい",
                    "pos": "助動詞",
                    "pos_sub1": "*",
                    "lemma": "んさる",
                    "reading": "ンサイ",
                }
                tokens[repaired_end : idx + 1] = [*continuative[:-1], honorific]
                repaired_end += len(continuative)
                idx = repaired_end
                continue
        if (
            idx >= 2
            and token.get("surface") == "さい"
            and token.get("pos") == "名詞"
            and previous.get("surface") == "ん"
            and tokens[idx - 2].get("pos") == "動詞"
            and tokens[idx - 2].get("conj_form") == "連用形"
        ):
            tokens[idx - 1 : idx + 1] = [
                {"surface": "んさい", "pos": "助動詞", "pos_sub1": "*", "lemma": "んさる", "reading": "ンサイ"}
            ]
            continue
        if (
            idx >= 2
            and token.get("surface") == "きい"
            and previous.get("surface") == "と"
            and previous.get("pos") == "助詞"
            and tokens[idx - 2].get("pos") == "動詞"
            and clause_end
        ):
            tokens[idx - 1 : idx + 1] = [
                {
                    "surface": "ときい",
                    "pos": "動詞",
                    "pos_sub1": "非自立",
                    "lemma": "とく",
                    "conj_type": "五段・カ行イ音便",
                    "conj_form": "連用形",
                    "reading": "トキイ",
                }
            ]
            continue
        idx += 1


def repair_contracted_quotative(tokens: list[dict]) -> None:
    """Rebuild って+いう contracted to っ+つー (そうだっつーの, やるっつったら).

    The quotative shrinks to its geminate and いう takes the つ onset. The
    reference dictionary reads the geminate as part of the host (だっ, いっ) or
    as the verb く, and the rest as つ or つる. The host is re-analyzed in front
    of the plain quotative と, which fixes its own boundary, and the remainder
    is いう in the cell its spelling shows: つー/つう terminal, つっ onbin.
    """
    idx = 1
    repaired_end = 0
    while idx < len(tokens):
        token, previous = tokens[idx], tokens[idx - 1]
        following = tokens[idx + 1] if idx + 1 < len(tokens) else None
        surface = token.get("surface", "")
        if not previous.get("surface", "").endswith("っ") or not surface.startswith("つ"):
            idx += 1
            continue
        consumed = 1
        if surface == "つ" and following is not None and following.get("surface") == "ー":
            verb = {"surface": "つー", "conj_form": "基本形"}
            consumed = 2
        elif surface in ("つう", "つー"):
            verb = {"surface": surface, "conj_form": "基本形"}
        elif surface == "つっ":
            verb = {"surface": surface, "conj_form": "連用タ接続"}
        else:
            idx += 1
            continue
        prefix = "".join(t.get("surface", "") for t in tokens[repaired_end:idx])[:-1]
        host = mecab_analyze(prefix + "と") if prefix else []
        if not host or host[-1].get("surface") != "と" or host[-1].get("pos") != "助詞":
            idx += 1
            continue
        quote = {
            "surface": "っ",
            "pos": "助詞",
            "pos_sub1": "格助詞",
            "pos_sub2": "引用",
            "lemma": "って",
            "reading": "ッ",
        }
        verb.update({"pos": "動詞", "pos_sub1": "自立", "lemma": "いう", "conj_type": "五段・ワ行促音便"})
        tokens[repaired_end : idx + consumed] = [*host[:-1], quote, verb]
        repaired_end += len(host) + 1
        idx = repaired_end + 1


def repair_euphonic_adjective_adverb(tokens: list[dict]) -> None:
    """Read an adjective's euphonic continuative as the adjective it inflects.

    早う is 早く with the く softened to う, the same cell as 高う in 高うございます;
    the reference dictionary lists some of these as adverbs of their own, so a
    う-final adverb is re-analyzed before ございます, which selects that cell.
    """
    for idx, token in enumerate(tokens):
        surface = token.get("surface", "")
        if token.get("pos") != "副詞" or len(surface) < 2 or not surface.endswith("う"):
            continue
        analyzed = mecab_analyze(surface + "ございます")
        if (
            analyzed
            and analyzed[0].get("surface") == surface
            and analyzed[0].get("pos") == "形容詞"
            and analyzed[0].get("conj_form", "").startswith("連用ゴザイ")
        ):
            tokens[idx] = analyzed[0]


def repair_continuative_before_manner_suffix(tokens: list[dict]) -> None:
    """Read the host of the manner suffix よう as the verb continuative it is.

    よう in 読みようがない attaches to a continuative, and the reference
    dictionary analyzes やめ+よう that way; it lists あきらめ and 慰め as nouns of
    their own, though, so the host is re-analyzed before ます, which selects the
    continuative, and adopted when it reads as one.
    """
    for idx in range(len(tokens) - 1):
        token, suffix = tokens[idx], tokens[idx + 1]
        if (
            token.get("pos") != "名詞"
            or token.get("pos_sub1") != "一般"
            or suffix.get("surface") != "よう"
            or suffix.get("pos_sub1") != "接尾"
        ):
            continue
        surface = token.get("surface", "")
        analyzed = mecab_analyze(surface + "ます")
        if (
            analyzed
            and analyzed[0].get("surface") == surface
            and analyzed[0].get("pos") == "動詞"
            and analyzed[0].get("conj_form", "").startswith("連用")
        ):
            tokens[idx] = analyzed[0]


def repair_adjective_stem_before_suffix(tokens: list[dict]) -> None:
    """Read a bare noun before excess すぎ / appearance そう as the adjective stem.

    Utterance-final 高すぎ comes back as noun+suffix although 高すぎた is the
    adjective stem plus the continuative of すぎる; the host is re-analyzed
    before すぎた and adopted, with its すぎ, when it reads as that stem.
    """
    for idx in range(len(tokens) - 1):
        token, suffix = tokens[idx], tokens[idx + 1]
        if (
            token.get("pos") != "名詞"
            or token.get("pos_sub1") != "一般"
            or suffix.get("surface") not in ("すぎ", "そう")
            or suffix.get("pos_sub1") != "接尾"
        ):
            continue
        surface = token.get("surface", "")
        analyzed = mecab_analyze(surface + "すぎた")
        if (
            len(analyzed) >= 2
            and analyzed[0].get("surface") == surface
            and analyzed[0].get("pos") == "形容詞"
            and analyzed[1].get("surface") == "すぎ"
        ):
            tokens[idx] = analyzed[0]
            if suffix.get("surface") == "すぎ":
                tokens[idx + 1] = analyzed[1]


def repair_interrogative_nande(tokens: list[dict]) -> None:
    """Rejoin なん + copula で into the adverb なんで before a copula.

    A copula continuative cannot take another copula, so なん+で+です is the
    adverb なんで the dictionary already gives in なんでだよ.
    """
    idx = 0
    while idx < len(tokens) - 2:
        head, copula, following = tokens[idx], tokens[idx + 1], tokens[idx + 2]
        if (
            head.get("surface") == "なん"
            and head.get("pos") == "名詞"
            and copula.get("surface") == "で"
            and copula.get("pos") == "助動詞"
            and following.get("pos") == "助動詞"
            and following.get("lemma") in ("です", "だ")
        ):
            tokens[idx : idx + 2] = [{"surface": "なんで", "pos": "副詞", "pos_sub1": "一般", "lemma": "なんで"}]
        idx += 1


_REASON_NDE_BLOCKERS = frozenset({"は", "も", "ある", "あり", "あっ", "ござい", "ござる"})


def split_reason_nde(tokens: list[dict]) -> None:
    """Read the reason んで closing a clause as nominalizer ん + conjunctive で.

    The contraction of ので keeps both morae apart, as 好きなんです keeps ん+です;
    the reference fuses them after な (雨なんで) but reads the で after a verb as
    the copula (行くんで). A following topic or existence verb (行くんではない)
    keeps the copula reading.
    """
    idx = 0
    while idx < len(tokens):
        token = tokens[idx]
        if token.get("surface") == "んで" and token.get("pos") == "助詞" and idx > 0:
            tokens[idx : idx + 1] = [
                {"surface": "ん", "pos": "名詞", "pos_sub1": "非自立", "lemma": "ん"},
                {"surface": "で", "pos": "助詞", "pos_sub1": "接続助詞", "lemma": "で"},
            ]
            idx += 2
            continue
        de = tokens[idx + 1] if idx + 1 < len(tokens) else None
        following = tokens[idx + 2] if idx + 2 < len(tokens) else None
        if (
            idx > 0
            and de is not None
            and token.get("surface") == "ん"
            and token.get("pos") == "名詞"
            and token.get("pos_sub1") == "非自立"
            and de.get("surface") == "で"
            and de.get("pos") == "助動詞"
            and (
                following is None
                or following.get("pos") == "記号"
                or (
                    following.get("pos") in ("動詞", "形容詞")
                    and following.get("pos_sub1") == "自立"
                    and following.get("surface") not in _REASON_NDE_BLOCKERS
                )
            )
        ):
            tokens[idx + 1] = {"surface": "で", "pos": "助詞", "pos_sub1": "接続助詞", "lemma": "で"}
        idx += 1


_CONTRACTED_NEGATIVES = frozenset({"ない", "ねえ", "ねぇ", "ねー"})


def repair_contracted_rareru(tokens: list[dict]) -> None:
    """Read らん before a negative as the contracted potential られ.

    The reference has only the noun らん (蘭), so 見てらんない and 信じらんない
    come back as a noun plus an independent ない. After a continuative or て
    and before a negative it is られ with its row nasalized.
    """
    for idx in range(1, len(tokens) - 1):
        host, token, negative = tokens[idx - 1], tokens[idx], tokens[idx + 1]
        if (
            token.get("surface") == "らん"
            and token.get("pos") == "名詞"
            and negative.get("surface") in _CONTRACTED_NEGATIVES
            and (
                # The te-particle may come fused into a misread として (こ+として).
                (host.get("pos") == "助詞" and host.get("surface", "").endswith(("て", "で")))
                or (host.get("pos") == "動詞" and host.get("conj_form", "").startswith("連用"))
            )
        ):
            tokens[idx] = {
                "surface": "らん",
                "pos": "動詞",
                "pos_sub1": "接尾",
                "conj_type": "一段",
                "conj_form": "未然形",
                "lemma": "られる",
            }
            tokens[idx + 1] = {
                "surface": negative.get("surface", ""),
                "pos": "助動詞",
                "conj_type": "特殊・ナイ",
                "conj_form": "基本形",
                "lemma": "ない",
            }


def _evaluates_iika(previous: dict) -> bool:
    """Whether the word before いっか frames an evaluation (まあ, これで, それは)."""
    return previous.get("pos") == "副詞" or (
        previous.get("pos") == "助詞" and previous.get("surface") in ("で", "は", "も")
    )


def repair_contracted_iika(tokens: list[dict]) -> None:
    """Split a clause-final kana いっか into いい contracted before か.

    いいか shortens to いっか (まあいっか, これでいっか), which the reference
    reads as the noun 一家 or 一過; those are written in kanji, and a kana
    noun does not close an utterance on its own after an adverb or particle.
    """
    for idx, token in enumerate(tokens):
        following = tokens[idx + 1] if idx + 1 < len(tokens) else None
        after = tokens[idx + 2] if idx + 2 < len(tokens) else None
        # An onbin いっ cannot carry the final か either (もういっか。).
        if (
            idx > 0
            and token.get("surface") == "いっ"
            and token.get("pos") == "動詞"
            and _evaluates_iika(tokens[idx - 1])
            and following is not None
            and following.get("surface") == "か"
            and following.get("pos_sub1", "").endswith("終助詞")
            and (after is None or after.get("pos") == "記号")
        ):
            tokens[idx] = {"surface": "いっ", "pos": "形容詞", "pos_sub1": "自立", "lemma": "いい"}
            continue
        if (
            idx > 0
            and token.get("surface") == "いっか"
            and token.get("pos") == "名詞"
            and _evaluates_iika(tokens[idx - 1])
            and (following is None or following.get("pos") == "記号" or following.get("pos_sub1") == "終助詞")
        ):
            tokens[idx : idx + 1] = [
                {"surface": "いっ", "pos": "形容詞", "pos_sub1": "自立", "lemma": "いい"},
                {"surface": "か", "pos": "助詞", "pos_sub1": "終助詞", "lemma": "か"},
            ]


def repair_lengthened_negative(tokens: list[dict]) -> None:
    """Read a colloquial ねえ/ねぇ/ねー after an irrealis as the negative ない.

    A ra-row verb's ん-contracted irrealis that the reference cut into another
    verb's irrealis plus ん (わか+ん) is rejoined first.

    The reference reads ねぇ and ねえ as the colloquial ない after a plain
    irrealis, but takes the ー spelling, and every spelling after the
    ん-contracted irrealis of a ra-row verb (変わん, わかん), for the final
    particle ね; a final particle cannot follow a bare irrealis or an
    adjective's continuative.
    """
    # The ん-contracted irrealis can come back cut as another verb's irrealis
    # plus the negative ん (わか+ん for わかん of わかる); a negative after that
    # would be a double negative. When the stem reads as the ra-row verb, the
    # two pieces are its contracted irrealis.
    idx = 1
    while idx + 1 < len(tokens):
        host, nasal, following = tokens[idx - 1], tokens[idx], tokens[idx + 1]
        if (
            host.get("pos") == "動詞"
            and host.get("conj_form") == "未然形"
            and nasal.get("surface") == "ん"
            and nasal.get("pos") == "助動詞"
            and following.get("surface") in _CONTRACTED_NEGATIVES
            and following.get("pos") == "助詞"
        ):
            ra_row = mecab_analyze(host.get("surface", "") + "らない")
            if ra_row and ra_row[0].get("pos") == "動詞" and ra_row[0].get("surface") == host.get("surface", "") + "ら":
                tokens[idx - 1 : idx + 1] = [
                    {
                        "surface": host.get("surface", "") + "ん",
                        "pos": "動詞",
                        "pos_sub1": "自立",
                        "conj_form": "未然特殊",
                        "lemma": ra_row[0].get("lemma", ""),
                    }
                ]
                continue
        idx += 1
    for idx in range(1, len(tokens)):
        host, token = tokens[idx - 1], tokens[idx]
        if token.get("pos") != "助詞" or token.get("surface") not in _CONTRACTED_NEGATIVES:
            continue
        contracted_irrealis = (
            host.get("pos") == "動詞"
            and host.get("surface", "").endswith("ん")
            and host.get("conj_form") in ("未然特殊", "体言接続特殊")
        )
        lengthened_after_irrealis = token.get("surface") == "ねー" and (
            (host.get("pos") == "動詞" and host.get("conj_form") == "未然形")
            or (host.get("pos") == "形容詞" and host.get("conj_form", "").startswith("連用"))
        )
        if contracted_irrealis or lengthened_after_irrealis:
            tokens[idx] = {
                "surface": token.get("surface", ""),
                "pos": "助動詞",
                "conj_type": "特殊・ナイ",
                "conj_form": "音便基本形",
                "lemma": "ない",
            }


def repair_kamo_quotative(tokens: list[dict]) -> None:
    """Rebuild か + もって as the particle かも + quotative って.

    もって (以て) opens a clause, so it cannot follow the particle か; the
    reference reaches it only when the host before か is an adverb (そうかもって),
    and reads 雨かもって as かも + って.
    """
    for idx in range(len(tokens) - 1):
        ka, motte = tokens[idx], tokens[idx + 1]
        if ka.get("surface") == "か" and ka.get("pos") == "助詞" and motte.get("surface") == "もって":
            tokens[idx : idx + 2] = [
                {"surface": "かも", "pos": "助詞", "pos_sub1": "副助詞", "lemma": "かも"},
                {"surface": "って", "pos": "助詞", "pos_sub1": "格助詞", "pos_sub2": "連語", "lemma": "って"},
            ]
