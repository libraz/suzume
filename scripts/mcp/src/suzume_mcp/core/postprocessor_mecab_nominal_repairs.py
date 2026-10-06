"""Repairs for nominal and verbal-noun boundaries that MeCab misreads."""

import regex

from .constants import CLOSED_HONORIFIC_SERU_LEMMAS
from .core_lexicon import core_headwords
from .mecab import is_single_token_of_pos, mecab_analyze


def repair_kanji_prefix_before_kana_noun(tokens: list[dict]) -> None:
    """Rebuild the boundary where a stem's okurigana was read as a noun.

    A kanji prefix forms a compound noun with the noun it attaches to, so that
    host is written in kanji as well — the merge rule that joins the pair asks
    for exactly that. A bare-hiragana noun therefore never continues one, and
    when the analyzer emits that pair it has taken a stem's okurigana for the
    start of the following word: 抗いし者 comes back as 抗(接頭詞) + いし(名詞) +
    者, a split that leaves a lemma the sentence never contained and that the
    analyzer itself does not make when the same two morae end the input
    (抗いし alone is read 抗い + し).

    The dictionary says where the boundary belongs: the prefix plus the noun's
    first kana is a headword of its own, which is what makes the kanji a stem
    with okurigana rather than a prefix. Both pieces are re-analyzed from
    there, so the class each one lands in stays the dictionary's own.
    """
    for index in range(len(tokens) - 2, -1, -1):
        token = tokens[index]
        follower = tokens[index + 1]
        if token.get("pos") != "接頭詞" or token.get("pos_sub1") != "名詞接続":
            continue
        prefix = token.get("surface", "")
        noun = follower.get("surface", "")
        if not regex.fullmatch(r"\p{Han}+", prefix):
            continue
        if follower.get("pos") != "名詞" or not regex.fullmatch(r"\p{Hiragana}{2,}", noun):
            continue
        head = prefix + noun[0]
        analyzed = mecab_analyze(head)
        if len(analyzed) != 1 or analyzed[0].get("surface") != head:
            continue
        tokens[index : index + 2] = [analyzed[0], *mecab_analyze(noun[1:])]


def split_transparent_suru_te_adverb(tokens: list[dict]) -> None:
    """Split a lexical adverb that is transparently 名詞 + し + て.

    The reference dictionary files 心して as one adverb while the identically
    built 用心して and 安心して stay 名詞+し+て, which puts a lexical boundary across
    an inflecting stem and its conjunctive particle. Two of the dictionary's own
    marks decide which it is, and the fossilized adverbs fail one or the other:

    - The entry is still filed as taking particles (助詞類接続) rather than as a
      plain adverb, which already separates 心して from 概して, 決して and 大して.
    - Its reading is the noun's own reading followed by シテ. 決して reads ケッシテ
      rather than ケツ+シテ and 大して reads タイシテ rather than ダイ+シテ, while a
      kana stem (どうして, まして) never enters the rule at all.
    """
    for index in range(len(tokens) - 1, -1, -1):
        token = tokens[index]
        surface = token.get("surface", "")
        reading = token.get("reading", "")
        if token.get("pos") != "副詞" or token.get("pos_sub1") != "助詞類接続":
            continue
        if not surface.endswith("して") or len(surface) <= 2:
            continue
        stem = surface[:-2]
        if not regex.fullmatch(r"\p{Han}+", stem) or not reading.endswith("シテ"):
            continue
        analyzed = mecab_analyze(stem)
        if len(analyzed) != 1 or analyzed[0].get("pos") != "名詞":
            continue
        if analyzed[0].get("reading", "") != reading[:-2]:
            continue
        tokens[index : index + 1] = [
            analyzed[0],
            {
                "surface": "し",
                "pos": "動詞",
                "pos_sub1": "自立",
                "conj_type": "サ変・スル",
                "conj_form": "連用形",
                "lemma": "する",
                "reading": "シ",
            },
            {
                "surface": "て",
                "pos": "助詞",
                "pos_sub1": "接続助詞",
                "conj_type": "",
                "conj_form": "",
                "lemma": "て",
                "reading": "テ",
            },
        ]


# The ない-family cells a predicate can spell: ない / なく(て) / なかっ(た) /
# なけれ(ば) / なけりゃ / なきゃ. The analyzer cuts each of them at the tail, so
# only the head is matched here.
_NAI_NEGATIVE_HEAD = regex.compile(r"^な(い|く|かっ|けれ|けりゃ|きゃ)")


def repair_kko_nominalizer(tokens: list[dict]) -> None:
    """Rebuild the bound nominalizer っこ before a ない-family predicate.

    The reference dictionary has no entry for っこ, so it reads the two morae as
    the emphatic sokuon plus the irrealis of 来る and then reconstructs a verb
    around whatever is left: 負ける becomes 負/ける with the okurigana glued to the
    sokuon (負+けっ+こ), できる becomes で+きっ+こ, and a stem whose okurigana is
    already a full continuative simply keeps a standalone っ (分かり+っ+こ). Every
    host breaks, so the suffix is restored here rather than corrected per word.

    The continuative in front of the suffix is recovered by re-analyzing the
    prefix under ます, which selects that cell and nothing else, and the ない that
    follows is left as MeCab tagged it — it is the predicate of the construction.
    Re-analysis starts after the previous repair, because feeding an already
    repaired っこ back to the analyzer would only break it the same way again.
    """
    idx = 1
    repaired_end = 0
    while idx < len(tokens) - 1:
        token = tokens[idx]
        previous = tokens[idx - 1]
        if (
            token.get("surface") != "こ"
            or not previous.get("surface", "").endswith("っ")
            or not _NAI_NEGATIVE_HEAD.match(tokens[idx + 1].get("surface", ""))
        ):
            idx += 1
            continue
        prefix = "".join(t.get("surface", "") for t in tokens[repaired_end:idx])[:-1]
        continuative = mecab_analyze(prefix + "ます")
        if not continuative or continuative[-1].get("surface") != "ます":
            idx += 1
            continue
        suffix = {"surface": "っこ", "pos": "名詞", "pos_sub1": "接尾", "lemma": "っこ"}
        tokens[repaired_end : idx + 1] = [*continuative[:-1], suffix]
        repaired_end += len(continuative)
        idx = repaired_end
    return


def repair_assimilated_koto_copula(tokens: list[dict]) -> None:
    """Rebuild こっ+ちゃ, the formal noun こと assimilated to the copula じゃ.

    えらいこっちゃ and 知ったこっちゃない are ことじゃ with the copula's voiced
    onset pulled into a geminate. The reference dictionary reads the pair as
    the verb 凝る plus the contracted ては, or as the pronoun こっち plus an
    unknown run that starts with ゃ. Only a modifier can host a formal noun, so
    a particle or the start of the clause in front keeps the verb reading
    (肩がこっちゃ). The tokens match what ことじゃ already yields.
    """
    idx = 1
    while idx < len(tokens) - 1:
        token, following = tokens[idx], tokens[idx + 1]
        host_pos = tokens[idx - 1].get("pos")
        verb_reading = (
            token.get("surface") == "こっ" and token.get("pos") == "動詞" and following.get("surface") == "ちゃ"
        )
        pronoun_reading = token.get("surface") == "こっち" and following.get("surface", "").startswith("ゃ")
        if host_pos in ("助詞", "記号") or not (verb_reading or pronoun_reading):
            idx += 1
            continue
        rest = following.get("surface", "")[1:] if pronoun_reading else ""
        tail_end = idx + 2
        # A negative read against ては is the adjective; against the copula it
        # is the auxiliary, as in ことじゃない.
        if (
            tail_end < len(tokens)
            and tokens[tail_end].get("pos") == "形容詞"
            and tokens[tail_end].get("lemma") == "ない"
        ):
            rest += tokens[tail_end].get("surface", "")
            tail_end += 1
        tokens[idx:tail_end] = [
            {
                "surface": "こっ",
                "pos": "名詞",
                "pos_sub1": "非自立",
                "pos_sub2": "一般",
                "lemma": "こと",
                "reading": "コッ",
            },
            {
                "surface": "ちゃ",
                "pos": "助動詞",
                "pos_sub1": "*",
                "conj_type": "特殊・ダ",
                "conj_form": "連用形",
                "lemma": "だ",
                "reading": "チャ",
            },
            # The tail is read after ことじゃ so it keeps the copula's context.
            *(mecab_analyze("ことじゃ" + rest)[2:] if rest else []),
        ]
        idx += 2


def merge_honorific_kana_verbal_noun(tokens: list[dict]) -> None:
    """Rebuild ご + a fragmented kana verbal noun + する as one noun + する.

    The reference cuts the kana spelling of a Sino-Japanese verbal noun into
    kanji-readable pieces after the prefix ご (ご+れん+らく+し, ご+あん+ない+
    し(助詞)), while it keeps ご+あんしん whole; the run between ご and する (or
    its humble いたす) in any cell is one noun.
    """

    def closes_frame(position: int) -> bool:
        token = tokens[position]
        if token.get("pos") == "動詞" and token.get("lemma") in ("する", "いたす"):
            return True
        return (
            token.get("surface") == "し" and position + 1 < len(tokens) and tokens[position + 1].get("pos") == "助動詞"
        )

    idx = 0
    while idx < len(tokens) - 3:
        if tokens[idx].get("surface") != "ご" or tokens[idx].get("pos") != "接頭詞":
            idx += 1
            continue
        end = idx + 1
        while end < len(tokens) and regex.fullmatch(r"\p{Hiragana}+", tokens[end].get("surface", "")):
            if closes_frame(end):
                break
            end += 1
        if end - idx - 1 >= 2 and end < len(tokens) and closes_frame(end):
            noun = "".join(token.get("surface", "") for token in tokens[idx + 1 : end])
            verb = tokens[end]
            if verb.get("pos") != "動詞":
                verb = {"surface": "し", "pos": "動詞", "pos_sub1": "自立", "lemma": "する"}
            tokens[idx + 1 : end + 1] = [
                {"surface": noun, "pos": "名詞", "pos_sub1": "サ変接続", "lemma": noun},
                verb,
            ]
        idx += 1


def split_demonstrative_dake(tokens: list[dict]) -> None:
    """Split the reference's adverb これだけ/それだけ into pronoun + だけ.

    The reference splits それ+だけ before the copula (それ+だけ+だ) but keeps
    one adverb before a final particle or a verb (それだけ+よ); the pronoun
    and the adverbial particle are separate words in both positions.
    """
    idx = 0
    while idx < len(tokens):
        token = tokens[idx]
        surface = token.get("surface", "")
        if token.get("pos") == "副詞" and regex.fullmatch(r"[こそあど]れだけ", surface):
            tokens[idx : idx + 1] = [
                {"surface": surface[:2], "pos": "名詞", "pos_sub1": "代名詞", "lemma": surface[:2]},
                {"surface": "だけ", "pos": "助詞", "pos_sub1": "副助詞", "lemma": "だけ"},
            ]
            idx += 2
            continue
        idx += 1


def repair_mimetic_n_to_suru(tokens: list[dict]) -> None:
    """Rebuild a two-mora kana noun in ん + と + する as the mimetic adverb.

    The reference reads 部屋がしんとした as the noun しん (芯) + と, yet
    returns しんと as one adverb before a content verb; a CVん mora pair
    marked by と before する is the mimetic manner adverb (ぽつんと+した).
    """
    idx = 0
    while idx < len(tokens) - 2:
        noun, to, verb = tokens[idx], tokens[idx + 1], tokens[idx + 2]
        surface = noun.get("surface", "")
        if (
            noun.get("pos") == "名詞"
            and regex.fullmatch(r"\p{Hiragana}ん", surface)
            and to.get("surface") == "と"
            and to.get("pos") == "助詞"
            and verb.get("lemma") == "する"
        ):
            tokens[idx : idx + 2] = [
                {"surface": surface + "と", "pos": "副詞", "pos_sub1": "一般", "lemma": surface + "と"}
            ]
        idx += 1


def repair_adjective_yo_quotative(tokens: list[dict]) -> None:
    """Split a kana verb in よっ before て that is an adjective plus よ + って.

    The reference reads いいよって as the kana spelling of 言い寄って; the
    kana run is the finished adjective いい, the final particle よ and the
    quotative って, as いいかって already comes back.
    """
    idx = 0
    while idx < len(tokens) - 1:
        token, te = tokens[idx], tokens[idx + 1]
        surface = token.get("surface", "")
        host = surface[:-2]
        if (
            token.get("pos") == "動詞"
            and surface.endswith("よっ")
            and regex.fullmatch(r"\p{Hiragana}+", surface)
            and te.get("surface") == "て"
            and host
        ):
            analyzed = mecab_analyze(host)
            if len(analyzed) == 1 and analyzed[0].get("pos") == "形容詞" and analyzed[0].get("conj_form") == "基本形":
                tokens[idx : idx + 2] = [
                    analyzed[0],
                    {"surface": "よ", "pos": "助詞", "pos_sub1": "終助詞", "lemma": "よ"},
                    {"surface": "って", "pos": "助詞", "pos_sub1": "格助詞", "lemma": "って"},
                ]
                idx += 3
                continue
        idx += 1


def merge_conjunction_with_rashii(tokens: list[dict]) -> None:
    """Rebuild an adjective in らしい the reference dictionary reads as a conjunction.

    A conjunction takes no auxiliary, so もっとも+らしい is not もっとも plus the
    conjectural; it is the adjective もっともらしい the dictionary lacks, and the
    らしい token already carries its inflection.
    """
    idx = 0
    while idx < len(tokens) - 1:
        head, tail = tokens[idx], tokens[idx + 1]
        if head.get("pos") != "接続詞" or tail.get("pos") != "助動詞" or tail.get("lemma") != "らしい":
            idx += 1
            continue
        tokens[idx : idx + 2] = [
            {
                **tail,
                "surface": head.get("surface", "") + tail.get("surface", ""),
                "pos": "形容詞",
                "pos_sub1": "自立",
                "lemma": head.get("surface", "") + "らしい",
            }
        ]
        idx += 1


def repair_productive_causative(tokens: list[dict]) -> None:
    """Split a productive causative the reference lexicon lists as one verb.

    笑わせ+ない and 泣かせ+た are the irrealis of 笑う and 泣く plus the causative
    せる, as 飛ば+せ+た already is; the reference dictionary keeps some of them
    whole only because it carries the ichidan headword, and only in some cells
    (笑わ+せる but 笑わせ+ない). Suzume's own lexicon decides which are
    lexicalized words (知らせる, 合わせる, and compounds ending in one such as
    組み合わせる), and a stem that is not the irrealis of a verb is no causative
    at all (見せる).
    """
    from .split_rules import base_from_mizenkei

    idx = 0
    while idx < len(tokens):
        token = tokens[idx]
        lemma = token.get("lemma") or ""
        surface = token.get("surface", "")
        stem = lemma[:-2]
        base = base_from_mizenkei(stem) if lemma.endswith("せる") and len(lemma) > 3 else None
        if (
            token.get("pos") != "動詞"
            or token.get("conj_type") != "一段"
            or base is None
            or not surface.startswith(stem)
            or len(surface) == len(stem)
            or lemma in CLOSED_HONORIFIC_SERU_LEMMAS
            or any(lemma[start:] in core_headwords("verbs.tsv") for start in range(len(lemma) - 2))
            or not is_single_token_of_pos(base, "動詞")
        ):
            idx += 1
            continue
        tokens[idx : idx + 1] = [
            {"surface": stem, "pos": "動詞", "pos_sub1": "自立", "lemma": base, "conj_form": "未然形"},
            {**token, "surface": surface[len(stem) :], "lemma": "せる", "pos_sub1": "接尾"},
        ]
        idx += 2
