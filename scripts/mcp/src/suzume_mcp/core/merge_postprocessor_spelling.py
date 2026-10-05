"""Spelling, orthography, and productive-form post-processing passes."""

import regex

from .constants import (
    KANJI_PREFIX_COMPOUNDS,
    KANJI_SUFFIXES_KEPT_SEPARATE,
    LETTER_FORMULAS,
)
from .mecab import mecab_analyze
from .merge_postprocessor_grammar import _CONTINUATIVE_CELL

_IDEOGRAPHIC_SEQUENCE = regex.compile(r"^[\p{Han}\uFE00-\uFE0F\U000E0100-\U000E01EF]+$")


_VARIATION_SELECTORS = regex.compile(r"^[︀-️\U000E0100-\U000E01EF]+$")


def _postprocess_variation_selector_merge(
    result: list[dict], applied_rule: str | None
) -> tuple[list[dict], str | None]:
    """Reattach a variation selector to the character it selects a form of.

    A variation selector is a zero-width combining codepoint: it has no width,
    no reading and no meaning apart from the character in front of it, so it
    never stands alone. The reference dictionary emits one as its own token
    whenever the base character is not a headword, which makes the same single
    grapheme one token or two depending only on whether its emoji presentation
    needs the selector at all.
    """
    merged: list[dict] = []
    for token in result:
        surface = token.get("surface", "")
        if merged and surface and _VARIATION_SELECTORS.fullmatch(surface):
            merged[-1] = {**merged[-1], "surface": merged[-1].get("surface", "") + surface}
            if applied_rule is None:
                applied_rule = "variation-selector-merge"
            continue
        merged.append(token)
    return merged, applied_rule


# What a continuative can hand its clause to. A nominal is not on the list: the
# continuative would have to close a clause to modify one, and closing is what a
# continuative does not do.
_CONTINUATIVE_FOLLOWERS = frozenset({"動詞", "助詞", "助動詞", "形容詞"})


def _postprocess_stranded_okurigana(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Give a nominal back the okurigana that was read as a verb.

    A verb takes its arguments through case particles, so a bare nominal
    directly in front of one heads no dependency at all, and a continuative
    cannot modify the nominal behind it either — it closes nothing for the
    nominal to attach to. Where both neighbours are nominals the middle token
    is therefore not a verb, and when it is a single kana it is the okurigana
    of the nominal in front of it: 類い稀 comes back as 類 + い(いる) + 稀,
    whose lemma the sentence never contained. The same two morae are read as
    one word as soon as they are spelled out (書類 + いる), so the reading turns
    on the length of the kana rather than on the grammar.

    Only a plain nominal qualifies. A verbal noun takes the light verb without
    any particle by definition, and its continuative is that construction
    rather than a stranded kana (遅刻 + し + そう), which is why the class the
    dictionary assigns the host decides.
    """
    merged: list[dict] = []
    for index, token in enumerate(result):
        host = merged[-1] if merged else None
        follower = result[index + 1] if index + 1 < len(result) else None
        if (
            host is None
            or follower is None
            or host.get("pos") != "名詞"
            or host.get("pos_sub1") != "一般"
            or token.get("pos") != "動詞"
            or token.get("pos_sub1") != "自立"
            or token.get("conj_form") != _CONTINUATIVE_CELL
            or len(token.get("surface", "")) != 1
            or follower.get("pos") in _CONTINUATIVE_FOLLOWERS
        ):
            merged.append(token)
            continue
        combined = host.get("surface", "") + token.get("surface", "")
        merged[-1] = {**host, "surface": combined, "lemma": combined}
        if applied_rule is None:
            applied_rule = "stranded-okurigana"
    return merged, applied_rule


def _postprocess_nickname_merge(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Merge hiragana nickname + ちゃん/くん into a single token.

    Tokenizer use case: short hiragana nicknames like たっちゃん / ゆうくん /
    けんちゃん / わんちゃん are one search unit, wherever they stand in the
    sentence (ゆうくんが来た). Kanji or katakana names (太郎+ちゃん, ピー+ちゃん)
    keep splitting, and さん stays a separate suffix after an ordinary stem.

    The reference analyzer often cuts across the stem/honorific boundary
    (ゆう+たく+ん, たっ+ちゃ+ん), so the span is matched on the concatenated
    surface of whole tokens, not on token boundaries inside it. A verb ending
    in く followed by the nominalizer ん (はたらく+ん+だ) is that construction,
    not a nickname, and neither is a closed-class word followed by them.
    """
    honorifics = ("ちゃん", "くん")
    hira_re = regex.compile(r"^[\p{Hiragana}っー]+$")

    def is_closed_class_word(token: dict) -> bool:
        # A pronoun, determiner, adverb or conjunction stands on its own and
        # never hosts an honorific (みんな+ちゃんと, その+くんれん).
        return token.get("pos") in {"連体詞", "副詞", "接続詞"} or (
            token.get("pos") == "名詞" and token.get("pos_sub1") == "代名詞"
        )

    def nickname_end(i: int) -> int:
        run = ""
        for j in range(i, len(result)):
            surface = result[j].get("surface", "")
            if not hira_re.match(surface):
                return -1
            if j == i and is_closed_class_word(result[j]) and 2 <= len(surface) <= 3:
                return -1
            run += surface
            for h in honorifics:
                stem_len = len(run) - len(h)
                if not run.endswith(h) or not 2 <= stem_len <= 3:
                    continue
                verb_nominalizer = (
                    h == "くん"
                    and surface == "ん"
                    and j > i
                    and result[j - 1].get("pos") == "動詞"
                    and result[j - 1].get("surface", "").endswith("く")
                )
                return -1 if verb_nominalizer else j + 1
            if len(run) > 3 + max(len(h) for h in honorifics):
                return -1
        return -1

    merged: list[dict] = []
    i = 0
    while i < len(result):
        # Skip if prev is a prefix (お/ご) — let family-merge handle those. A
        # particle is no nickname's first mora (を+たくさん is not を+たく+さん),
        # but nothing can be a particle at the start of the text (けん+くん).
        prev_is_prefix = merged and merged[-1].get("pos", "") == "接頭詞"
        if not prev_is_prefix and (result[i].get("pos") != "助詞" or not merged):
            j = nickname_end(i)
            if j > 0:
                run = "".join(t.get("surface", "") for t in result[i:j])
                merged.append({"surface": run, "pos": "名詞", "lemma": run})
                i = j
                if applied_rule is None:
                    applied_rule = "nickname-merge"
                continue
        merged.append(result[i])
        i += 1
    return merged, applied_rule


def _postprocess_kanji_merge(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Merge consecutive all-kanji tokens.

    Also merges single-kanji + kanji-starting tokens when MeCab incorrectly
    splits compound words (e.g., 微+笑み → 微笑み).
    """
    merged = []
    for position, curr in enumerate(result):
        surface = curr.get("surface", "")
        following = result[position + 1] if position + 1 < len(result) else {}
        # A counter closes its numeral (第二+次 | 計画).
        after_counter = bool(merged) and merged[-1].get("pos_sub2", "") == "助数詞"
        # Suzume design: tokenizer use case prefers X+suffix as a single search
        # unit. These suffixes are not treated as token boundaries; X+SUFFIX
        # merges via kanji-merge.
        #   家/力/化/法/論/員/式/感/的/安 — productive but one search unit
        # 様/氏 keep splitting (honorific separates from name).
        is_merge_allowed_suffix = surface in ("家", "力", "化", "法", "論", "員", "式", "感", "的", "風", "安")
        # Any other one-kanji general suffix bonds with an ordinary noun host
        # (改正案, 交通費) but not with a temporal one (今|紙).
        is_general_suffix = (
            curr.get("pos_sub1", "") == "接尾"
            and curr.get("pos_sub2", "") == "一般"
            and len(surface) == 1
            and surface not in KANJI_SUFFIXES_KEPT_SEPARATE
        )
        # Suzume design: 御 is a productive prefix that always splits off
        # (御 + 尽力, 御 + 挨拶, 御 + 協力). Skip kanji-merge after 御 prefix tokens.
        prev_is_go_prefix = merged and merged[-1].get("surface", "") == "御" and merged[-1].get("pos", "") == "接頭詞"
        # A na-adjective stem heads a predicate, so a bound modifier in front of
        # it stays outside: 時|不思議|な, 激|簡単|だ, 鬼|簡単|だ.  Such a modifier
        # does bond with a plain noun into one search unit (激安, 超高速), but it
        # stops at a predicate head — the same boundary it already keeps before an
        # i-adjective (激|冷たい).  Only the modifier side is held back: a full
        # noun preceding the stem still compounds (再利用可能), which is why this
        # is restricted to a non-independent noun or a single kanji.  MeCab tags
        # the stem as a noun, so the generic kanji merge would otherwise erase
        # that boundary before POS normalization.
        na_adjective_stem_boundary = (
            merged
            and curr.get("pos", "") == "名詞"
            and curr.get("pos_sub1", "") == "形容動詞語幹"
            and merged[-1].get("pos", "") == "名詞"
            and (merged[-1].get("pos_sub1", "") == "非自立" or len(merged[-1].get("surface", "")) == 1)
        )
        if merged and (
            (
                _IDEOGRAPHIC_SEQUENCE.fullmatch(surface)
                and _IDEOGRAPHIC_SEQUENCE.fullmatch(merged[-1].get("surface", ""))
                # Kanji adjacency alone is not a compound boundary.  In
                # particular, a temporal noun followed by a one-kanji verb
                # stem (the pattern 日+見+た) must retain the predicate
                # boundary.  Restrict this recovery pass to nominal pieces.
                and curr.get("pos", "") == "名詞"
                and merged[-1].get("pos", "") == "名詞"
                and "々" not in merged[-1].get("surface", "")
                and (merged[-1].get("pos_sub1", "") not in ("副詞可能", "固有名詞", "数") or is_merge_allowed_suffix)
                # A temporal suffix closes its time noun (先月|末|決算) the way a
                # temporal noun itself does.
                and not (merged[-1].get("pos_sub1", "") == "接尾" and merged[-1].get("pos_sub2", "") == "副詞可能")
                and merged[-1].get("pos", "") != "副詞"
                and (curr.get("pos_sub1", "") != "接尾" or is_merge_allowed_suffix or is_general_suffix)
                # A number+counter unit (五分, 二時間, 五名) is its own search unit and
                # must not fold into a preceding noun/prefix (徒歩|五分, 約|二時間).
                and curr.get("pos_sub1", "") != "数"
                and not prev_is_go_prefix
                and not na_adjective_stem_boundary
                # A pronoun is a phrase of its own; it opens a compound only
                # with a bound one-kanji element (何+気) and never closes one.
                and curr.get("pos_sub1", "") != "代名詞"
                and (merged[-1].get("pos_sub1", "") != "代名詞" or len(surface) == 1)
                and surface not in LETTER_FORMULAS
                and merged[-1].get("surface", "") not in LETTER_FORMULAS
                # A na-adjective stem directly before an independent predicate
                # is used adverbially (大変|恐れ入る) and heads no compound.
                and not (
                    curr.get("pos_sub1", "") == "形容動詞語幹"
                    and following.get("pos", "") in ("動詞", "形容詞")
                    and following.get("pos_sub1", "") == "自立"
                )
                and not after_counter
            )
            or (surface == "々" and _IDEOGRAPHIC_SEQUENCE.fullmatch(merged[-1].get("surface", "")))
            or (
                merged[-1].get("surface", "") in KANJI_PREFIX_COMPOUNDS
                and surface in KANJI_PREFIX_COMPOUNDS[merged[-1]["surface"]]
                and merged[-1].get("pos") != "Noun"
            )
        ):
            # A compound is right-headed, so whether it is a na-adjective stem
            # is decided by its last piece (交通+安全 is one, 安全+対策 is not).
            if "形容動詞語幹" in (merged[-1].get("pos_sub1"), curr.get("pos_sub1")):
                merged[-1]["pos_sub1"] = "形容動詞語幹" if curr.get("pos_sub1") == "形容動詞語幹" else "一般"
            merged[-1]["surface"] += surface
            merged[-1]["lemma"] = merged[-1]["surface"]
            merged[-1]["pos"] = "名詞"
            if applied_rule is None:
                applied_rule = "kanji-merge"
        else:
            merged.append(curr)
    return merged, applied_rule


# Characters an ASCII word keeps inside itself. The reference tokenizer emits
# them as standalone symbol tokens (Coca + - + Cola), which breaks the search
# unit into fragments no query matches, so the pieces are rejoined here.
_ASCII_WORD_JOINERS = ".-'&/"


def _postprocess_ascii_joiner_merge(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Merge a letter-initial ASCII word with each following joiner + ASCII/number pair.

    One scan per word: only a single-character joiner between two alphanumeric
    tokens is absorbed, so a multi-character symbol run (...) never opens a word
    or attaches to the following one.
    """
    merged = []
    idx = 0
    while idx < len(result):
        curr = result[idx]
        idx += 1
        merged.append(curr)
        if not regex.match(r"^[a-zA-Z][a-zA-Z0-9]*$", curr.get("surface", "")):
            continue
        while (
            idx + 1 < len(result)
            and len(result[idx].get("surface", "")) == 1
            and result[idx].get("surface", "") in _ASCII_WORD_JOINERS
            and regex.match(r"^[a-zA-Z0-9]+$", result[idx + 1].get("surface", ""))
        ):
            curr["surface"] += result[idx].get("surface", "") + result[idx + 1].get("surface", "")
            curr["lemma"] = curr["surface"]
            idx += 2
            if applied_rule is None:
                applied_rule = "ascii-joiner-merge"
    return merged, applied_rule


# Small kana that cannot open a mora, and therefore cannot open a morpheme.
# The sokuon っ and the moraic ん are excluded: both do stand alone as tokens.
_NON_INITIAL_SMALL_KANA = "ゃゅょぁぃぅぇぉゎャュョァィゥェォヮ"


def _postprocess_small_kana_head_merge(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Reattach a token that opens with a small kana to the one before it.

    A palatalised or small-vowel kana is the second half of a mora, so a token
    starting with one is a split inside a mora rather than a morpheme boundary.
    The reference dictionary produces such fragments for surfaces it does not
    know (読ん+じ+ょる).
    """
    merged: list[dict] = []
    for curr in result:
        surface = curr.get("surface", "")
        if merged and surface and surface[0] in _NON_INITIAL_SMALL_KANA:
            # A long unknown fragment is the small kana plus words the
            # dictionary does know (なき+ゃなんないんだ): only the kana
            # belongs to the previous mora, and the rest is read on its own.
            rest = mecab_analyze(surface[1:]) if len(surface) > 3 and curr.get("lemma") in (None, "*") else []
            if (
                rest
                and "".join(token.get("surface", "") for token in rest) == surface[1:]
                and all(token.get("lemma") not in (None, "*") for token in rest)
            ):
                merged[-1]["surface"] += surface[0]
                merged.extend(rest)
                if applied_rule is None:
                    applied_rule = "small-kana-head-merge"
                continue
            merged[-1]["surface"] += surface
            merged[-1]["lemma"] = merged[-1]["surface"]
            if applied_rule is None:
                applied_rule = "small-kana-head-merge"
            continue
        merged.append(curr)
    return merged, applied_rule


# The vowel each hiragana mora carries, used to tell an emphatic lengthening
# from a morpheme that merely starts with the same kana.
_MORA_VOWEL = {
    kana: vowel
    for vowel, row in (
        ("あ", "あかさたなはまやらわがざだばぱゃゎぁ"),
        ("い", "いきしちにひみりゐぎじぢびぴぃ"),
        ("う", "うくすつぬふむゆるぐずづぶぷゅぅゔ"),
        ("え", "えけせてねへめれゑげぜでべぺぇ"),
        ("お", "おこそとのほもよろをごぞどぼぽょぉ"),
    )
    for kana in row
}


def _postprocess_stranded_lengthening_vowel(
    result: list[dict], applied_rule: str | None
) -> tuple[list[dict], str | None]:
    """Reattach an emphatic vowel the reference analyzer left standing alone.

    Drawing out a final vowel is one word, however it is spelled: すごーい and
    すごーい written with a repeated kana are the same emphasis.  The reference
    dictionary merges the spelling only when it happens to hold the lengthened
    form as a headword (まあ, なあ), and otherwise strands the extra mora as a
    filler with no analysis (そりゃ+あ, か+あ) — so the same construction comes
    out two ways depending on the lexicon.  The construction decides instead: a
    one-mora filler repeating the vowel of the mora in front of it is that
    word's lengthening, and it carries no lemma of its own.  A genuine
    interjection is tagged as one and opens its utterance, so it is untouched.
    """
    merged: list[dict] = []
    skip_next = False
    for index, curr in enumerate(result):
        if skip_next:
            skip_next = False
            continue
        surface = curr.get("surface", "")
        previous = merged[-1].get("surface", "") if merged else ""
        following = result[index + 1] if index + 1 < len(result) else None
        # Exactly two え fillers are the dialect adjective ええ the dictionary
        # reads whole everywhere else (でええよ, ええ加減).
        if (
            surface == "え"
            and curr.get("pos") == "フィラー"
            and following is not None
            and following.get("surface") == "え"
            and following.get("pos") == "フィラー"
            and not (index + 2 < len(result) and result[index + 2].get("surface") == "え")
        ):
            merged.append({"surface": "ええ", "pos": "形容詞", "lemma": "ええ"})
            skip_next = True
            if applied_rule is None:
                applied_rule = "stranded-lengthening-vowel"
            continue
        if (
            merged
            and curr.get("pos") == "フィラー"
            and surface in "あいうえお"
            and previous
            and _MORA_VOWEL.get(previous[-1]) == surface
        ):
            merged[-1]["surface"] = previous + surface
            if applied_rule is None:
                applied_rule = "stranded-lengthening-vowel"
            continue
        merged.append(curr)
    return merged, applied_rule


# The colloquial volitional reduces its う to a geminate before the quotative
# と (行こう+と → 行こっと, し+よう+と → しよっと), so a run closing on っと is not
# automatically a mimetic. The reference dictionary marks the difference: a
# predicate leaves an o-row verb stem or the volitional auxiliary directly in
# front of the っと token, while a mimetic has no predicate there at all.
_O_ROW_KANA: frozenset[str] = frozenset("おこそとのほもよろごぞどぼぽょ")


def _opens_volitional_tto(previous: dict | None) -> bool:
    """Whether a token can carry the volitional that っと contracts."""
    if previous is None:
        return False
    if previous.get("pos") == "助動詞" and previous.get("lemma") in ("う", "よう"):
        return True
    return previous.get("pos") == "動詞" and previous.get("surface", "")[-1:] in _O_ROW_KANA


def _postprocess_onomatopoeia_tto_merge(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Merge onomatopoeia stem + っと → Xっと (adverb).

    MeCab splits: どき+っと, ぱっ+と, etc.
    Suzume treats Xっと as a single adverb unit.
    Pattern: short hiragana/katakana token + っと where stem is 1-4 chars.
    """
    merged: list[dict] = []
    skip_next = False
    for j, curr in enumerate(result):
        if skip_next:
            skip_next = False
            continue
        if (
            j < len(result) - 1
            and result[j + 1].get("surface") == "っと"
            and not _opens_volitional_tto(curr)
            # A closed function word (でした+っと) is no mimetic stem.
            and curr.get("pos") not in ("助動詞", "助詞", "Auxiliary", "Particle")
            and regex.match(r"^[\p{Hiragana}\p{Katakana}ー]{1,4}$", curr.get("surface", ""))
        ):
            combined = curr.get("surface", "") + "っと"
            merged.append({"surface": combined, "pos": "副詞", "lemma": combined})
            skip_next = True
            if applied_rule is None:
                applied_rule = "onomatopoeia-tto-merge"
        else:
            merged.append(curr)
    return merged, applied_rule


# A laugh is a bare vowel followed by the ha-row mora of the same vowel,
# repeated (あはは, うふふ, おほほ).
_LAUGHTER_HA_ROW = {"あ": "は", "い": "ひ", "う": "ふ", "え": "へ", "お": "ほ"}


def _is_laughter(surface: str) -> bool:
    """Whether a hiragana run is a laugh: vowel plus its ha-row mora twice or more."""
    if len(surface) < 3 or surface[0] not in _LAUGHTER_HA_ROW:
        return False
    return set(surface[1:]) == {_LAUGHTER_HA_ROW[surface[0]]}


def _mimetic_pos(surface: str) -> str:
    """A laugh is an interjection like the listed うふふ; other mimetics are adverbs."""
    return "感動詞" if _is_laughter(surface) else "副詞"


def _is_productive_mimetic_stem(surface: str) -> bool:
    """Recognize productive hiragana mimetic shapes without a word list."""
    if not regex.fullmatch(r"[\p{Hiragana}ー]{3,12}", surface):
        return False
    if _is_laughter(surface):
        return True
    length = len(surface)
    if length % 2 == 0 and surface[: length // 2] == surface[length // 2 :]:
        return True
    if regex.fullmatch(r".{2,4}ん.{2,4}ん", surface):
        return True
    if regex.fullmatch(r".っ.[らり]", surface):
        return True
    # One mora held for three or more beats is emphasis, and the whole run is
    # the word.  The dictionary segments it by whatever entries its length
    # happens to cover, so the even lengths come back as a reduplication and
    # the odd ones as a leftover beat plus a headword.
    if len(set(surface)) == 1:
        return True
    # Alternating two-mora mimetics such as ちくたく share their closing
    # mora even when the two halves are not identical.
    return length == 4 and surface[1] == surface[3]


def _is_split_reduplication(tokens: list[dict]) -> bool:
    """Whether adjacent tokens are one reduplicated mimetic MeCab tore apart.

    A mimetic half that is also a headword of its own gets segmented as that
    word (ちょき+ちょき), while a half with no entry survives as one adverb
    (くしゃくしゃ).  The construction is the same either way, so the shape
    decides rather than the lexicon: two or more adjacent content tokens with
    the identical hiragana surface are the doubling.
    """
    if len(tokens) < 2:
        return False
    surfaces = [token.get("surface", "") for token in tokens]
    if len(set(surfaces)) != 1 or len(surfaces[0]) < 2:
        return False
    if not regex.fullmatch(r"[\p{Hiragana}ー]+", surfaces[0]):
        return False
    return all(token.get("pos") in {"名詞", "副詞", "感動詞", "その他"} for token in tokens)


# A stem cell selects no suffix of its own, so nothing attaches to it.  The
# euphonic continuative is the opposite case: it exists only to carry the past
# and conjunctive suffixes, whose whole series opens on た / て and their voiced
# counterparts.  Both are named by the reference dictionary's inflection field,
# so the licensing test needs no word list of its own.
_STEM_ONLY_CONJ_FORMS: frozenset[str] = frozenset({"ガル接続", "語幹"})
_EUPHONIC_CONTINUATIVE = "連用タ接続"
_EUPHONIC_SUFFIX_HEADS = frozenset("たてだで")
# The hypothetical particle names its cell just as plainly, and it is the one
# conjunctive particle a single kana can imitate inside a doubled stem.
_HYPOTHETICAL_PARTICLE = "ば"
_HYPOTHETICAL_CELL = "仮定形"


def _is_licensed_attachment(left: dict, right: dict) -> bool:
    """Whether one token attaches to the one before it, proving the boundary.

    Four attachments account for every real split that a mimetic's shape can
    imitate: a bound suffix, whose left edge its host fixes; a particle after a
    nominal, which is the ordinary noun-phrase boundary; a function word on a
    particle, which is the ordinary stack; and an auxiliary or a conjunctive
    particle after a predicate standing in a form that selects it.  A predicate
    whose inflection the dictionary left unnamed proves nothing either way, so
    it does not count as an attachment.
    """
    if right.get("pos_sub1") == "接尾":
        return True
    right_pos = right.get("pos")
    if right_pos not in {"助詞", "助動詞"}:
        return False
    if left.get("pos") in {"名詞", "副詞", "代名詞", "感動詞"}:
        return right_pos == "助詞"
    if left.get("pos") == "助詞":
        return True
    if left.get("pos") not in {"動詞", "形容詞", "助動詞"}:
        return False
    left_form = left.get("conj_form") or ""
    if left_form in {"", "*"} or left_form in _STEM_ONLY_CONJ_FORMS:
        return False
    if right.get("surface") == _HYPOTHETICAL_PARTICLE and right.get("pos_sub1") == "接続助詞":
        return left_form == _HYPOTHETICAL_CELL
    if left_form == _EUPHONIC_CONTINUATIVE:
        return right.get("surface", "")[:1] in _EUPHONIC_SUFFIX_HEADS
    return True


def _spans_one_mimetic(tokens: list[dict], following: dict | None) -> bool:
    """Whether a run of tokens is one mimetic the reference dictionary tore up.

    A mimetic with no entry of its own gets guessed at, and the guess lands
    wherever the unknown-word model happens to find a headword: the same
    adverb comes back as a verb plus a noun in one sentence and an adjective
    plus an auxiliary in the next.  The shape test already says the combined
    surface is a mimetic, so what is left is to rule out a run whose split is
    real — and a split is real exactly when something inside it attaches.
    やっ+たり and あっ+たら put a suffix on the continuative cell that selects
    it, すもも+も is a noun phrase and やさし+さ a derivation; ばっ+ちり puts a
    noun after a continuative cell and ばっち+り hangs an auxiliary off an
    adjective stem, and neither connection exists in the grammar.  A run
    opening on an auxiliary is continuing a predicate that ended earlier, and
    one opening on a determiner (ほんの+ちょっと) is modifying the word after it.

    The attachment that disproves the run can also sit just past its right
    edge, because the shape test measures morae and stops wherever the count
    comes out: もしか + し is four morae of an alternating mimetic until the
    たら behind it shows that the し was a verb in the cell たら selects.
    """
    if len(tokens) == 1:
        # Before する the reference's サ変 noun tag is the same adverb the fused
        # ぞくぞくする already yields.
        if tokens[0].get("pos") == "名詞" and following is not None and following.get("lemma") == "する":
            return True
        return tokens[0].get("pos") in {"その他", "副詞", "感動詞"}
    if tokens[0].get("pos") in {"助動詞", "連体詞"}:
        return False
    if following is not None and _is_licensed_attachment(tokens[-1], following):
        return False
    return not any(_is_licensed_attachment(tokens[pos - 1], token) for pos, token in enumerate(tokens) if pos > 0)


def _postprocess_productive_mimetics(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Rebuild productive mimetic search units from arbitrary MeCab splits.

    Repetition and fixed phonological shapes are lexical content; a following
    adverbial と remains its own particle.  The productive Xっと shape instead
    includes と in the mimetic itself.
    """
    normalized: list[dict] = []
    idx = 0
    while idx < len(result):
        surface = result[idx].get("surface", "")
        mimetic_suru_splits = {
            "している": (
                {"surface": "し", "pos": "動詞", "lemma": "する"},
                {"surface": "て", "pos": "助詞", "lemma": "て"},
                {"surface": "いる", "pos": "助動詞", "lemma": "いる"},
            ),
            "してる": (
                {"surface": "し", "pos": "動詞", "lemma": "する"},
                {"surface": "てる", "pos": "助動詞", "lemma": "てる"},
            ),
            "した": (
                {"surface": "し", "pos": "動詞", "lemma": "する"},
                {"surface": "た", "pos": "助動詞", "lemma": "た"},
            ),
            "して": (
                {"surface": "し", "pos": "動詞", "lemma": "する"},
                {"surface": "て", "pos": "助詞", "lemma": "て"},
            ),
            "する": ({"surface": "する", "pos": "動詞", "lemma": "する"},),
        }
        fused_suffix = next(
            (
                suffix
                for suffix in mimetic_suru_splits
                if surface.endswith(suffix) and _is_productive_mimetic_stem(surface[: -len(suffix)])
            ),
            "",
        )
        if fused_suffix:
            stem = surface[: -len(fused_suffix)]
            normalized.append({"surface": stem, "pos": "副詞", "lemma": stem})
            normalized.extend(mimetic_suru_splits[fused_suffix])
            idx += 1
            if applied_rule is None:
                applied_rule = "productive-mimetic-suru"
            continue

        matched = False
        max_end = min(len(result), idx + 4)
        for end in range(max_end, idx, -1):
            combined = "".join(token.get("surface", "") for token in result[idx:end])
            starts_at_real_boundary = (
                result[idx].get("pos") != "助詞" or idx == 0 or result[idx - 1].get("pos") == "記号"
            )
            closes_volitional_tto = result[end - 1].get("surface") == "っと" and _opens_volitional_tto(
                result[end - 2] if end - 2 >= idx else None
            )
            if (
                starts_at_real_boundary
                and not closes_volitional_tto
                and combined.endswith("っと")
                and regex.fullmatch(r"[\p{Hiragana}ー]{3,12}", combined)
                and _spans_one_mimetic(result[idx:end], result[end] if end < len(result) else None)
            ):
                normalized.append({"surface": combined, "pos": "副詞", "lemma": combined})
                idx = end
                matched = True
            elif (
                result[idx].get("pos") != "助詞"
                and combined.endswith("と")
                and _is_productive_mimetic_stem(combined[:-1])
            ):
                stem = combined[:-1]
                normalized.append({"surface": stem, "pos": _mimetic_pos(stem), "lemma": stem})
                normalized.append({"surface": "と", "pos": "助詞", "lemma": "と"})
                idx = end
                matched = True
            elif (
                _is_split_reduplication(result[idx:end])
                or (
                    starts_at_real_boundary
                    and _is_productive_mimetic_stem(combined)
                    # A doubled particle (は+は, へ+へ) attaches nowhere, so a laugh
                    # needs no attachment test.
                    and (
                        _is_laughter(combined)
                        or _spans_one_mimetic(result[idx:end], result[end] if end < len(result) else None)
                    )
                )
            ):
                normalized.append({"surface": combined, "pos": _mimetic_pos(combined), "lemma": combined})
                idx = end
                matched = True
            if matched:
                if applied_rule is None:
                    applied_rule = "productive-mimetic"
                break
        if not matched:
            normalized.append(result[idx])
            idx += 1
    return normalized, applied_rule


def _postprocess_dialectal(result: list[dict]) -> None:
    """Fix POS/lemmas for dialectal/special patterns."""
    for j, curr in enumerate(result):
        surface = curr.get("surface", "")
        if surface in ("おいで", "お出で"):
            curr["pos"] = "副詞"
            curr["lemma"] = "おいで"
        if j < len(result) - 1:
            nxt = result[j + 1]
            if surface == "なん" and nxt.get("surface") == "し":
                curr["pos"] = "名詞"
                curr["lemma"] = "なん"
                nxt["pos"] = "動詞"
                nxt["lemma"] = "する"
