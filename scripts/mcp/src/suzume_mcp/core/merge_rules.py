"""Merge rules ported from SuzumeUtils.pm apply_suzume_merge()."""

import regex

from .constants import (
    COLLOQUIAL_PRONOUNS,
    COMPOUND_VERB_V2_GODAN,
    COMPOUND_VERB_V2_ICHIDAN,
    FAMILY_TERMS,
    HIRAGANA_COMPOUNDS,
    HONORIFIC_EXCEPTIONS,
    HONORIFIC_SUFFIXES,
    NAI_ADJECTIVES,
    PREFIX_EXCEPTIONS,
    SEARCH_UNIT_COMPOUNDS,
    TARI_ADVERB_STEMS,
)


def apply_suzume_merge(tokens: list[dict], text: str) -> tuple[list[dict], str | None]:
    """Apply Suzume merge rules to MeCab tokens.

    Returns:
        Tuple of (merged tokens, applied rule name or None).
    """
    result: list[dict] = []
    i = 0
    applied_rule: str | None = None

    while i < len(tokens):
        t = tokens[i]
        merged = False

        # Calculate position in text
        pos_in_text = sum(len(tokens[k].get("surface", "")) for k in range(i))
        remaining = text[pos_in_text:] if pos_in_text < len(text) else ""

        # 1. Full date pattern
        if not merged:
            m = regex.match(r"^(\d+年\d+月\d+日)", remaining)
            if m:
                date = m.group(1)
                length = 0
                j = i
                while j < len(tokens) and length < len(date):
                    length += len(tokens[j].get("surface", ""))
                    j += 1
                if length == len(date):
                    result.append({"surface": date, "pos": "名詞", "lemma": date})
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = "date"

        # 1.3. お + family/honorific terms
        if not merged and t.get("surface") == "お" and "接頭詞" in t.get("pos", "") and i + 1 < len(tokens):
            next_surface = tokens[i + 1].get("surface", "")
            if next_surface in FAMILY_TERMS:
                combined = "お" + next_surface
                result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                i += 2
                merged = True
                if applied_rule is None:
                    applied_rule = "family-merge"

        # 1.5. URL pattern
        if not merged:
            m = regex.match(r"^(https?://[a-zA-Z0-9\-._~:/?#\[\]@!$&'()*+,;=%]+)", remaining)
            if m:
                url = m.group(1)
                url = regex.sub(r"[.,)\]']+$", "", url)
                length = 0
                j = i
                while j < len(tokens) and length < len(url):
                    length += len(tokens[j].get("surface", ""))
                    j += 1
                if length == len(url):
                    result.append({"surface": url, "pos": "名詞", "lemma": url})
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = "url"

        # 1.6. ASCII with dots pattern
        if not merged:
            m = regex.match(r"^([a-zA-Z][a-zA-Z0-9]*(?:\.[a-zA-Z0-9]+)+)", remaining)
            if m:
                ascii_seq = m.group(1)
                length = 0
                j = i
                while j < len(tokens) and length < len(ascii_seq):
                    length += len(tokens[j].get("surface", ""))
                    j += 1
                if length == len(ascii_seq):
                    result.append({"surface": ascii_seq, "pos": "名詞", "lemma": ascii_seq})
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = "ascii-dots"

        # 2. Number + counter/katakana
        if not merged and t.get("pos") == "名詞" and t.get("pos_sub1") == "数":
            j = i + 1
            combined = t.get("surface", "")
            while j < len(tokens):
                nxt = tokens[j]
                ns = nxt.get("surface", "")
                np = nxt.get("pos", "")
                ns1 = nxt.get("pos_sub1", "")
                ns2 = nxt.get("pos_sub2", "")

                is_counter = np == "名詞" and ns1 == "接尾" and ns2 == "助数詞"
                is_calendar_month = ns == "月" and regex.match(r"^(?:1[0-2]|[1-9])$", combined)
                is_katakana_noun = np == "名詞" and regex.match(r"^[\u30A0-\u30FF]+$", ns)
                is_chuu_suffix = ns == "中" and np == "名詞" and ns1 == "接尾"
                is_me_suffix = ns == "目" and np == "名詞" and ns1 == "接尾"
                is_large_unit = np == "名詞" and ns1 == "数" and ns in ("万", "億", "兆")
                is_number_after_large = combined.endswith(("万", "億", "兆")) and np == "名詞" and ns1 == "数"
                is_number_after_decimal = combined.endswith(".") and np == "名詞" and ns1 == "数"
                is_counter_aux = ns == "つ" and np in ("助動詞", "動詞")
                is_percent = ns == "%"
                is_decimal = ns == "."
                is_consecutive_number = np == "名詞" and ns1 == "数" and regex.match(r"^[0-9０-９]+$", ns)
                is_alpha_unit = regex.match(r"^[A-Za-z]+$", ns) and np == "名詞"

                if any(
                    [
                        is_counter,
                        is_calendar_month,
                        is_katakana_noun,
                        is_chuu_suffix,
                        is_me_suffix,
                        is_large_unit,
                        is_number_after_large,
                        is_number_after_decimal,
                        is_counter_aux,
                        is_percent,
                        is_decimal,
                        is_alpha_unit,
                        is_consecutive_number,
                    ]
                ):
                    combined += ns
                    j += 1
                    if any([is_katakana_noun, is_chuu_suffix, is_me_suffix, is_counter_aux, is_percent, is_alpha_unit]):
                        break
                else:
                    break
            if j > i + 1:
                result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "number+unit"

        # 2a2. Address number pattern
        if not merged and t.get("pos") == "名詞" and t.get("pos_sub1") == "数":
            j = i + 1
            combined = t.get("surface", "")
            has_hyphen = False
            while j + 1 < len(tokens):
                hyphen = tokens[j]
                next_num = tokens[j + 1]
                if hyphen.get("surface") == "-" and next_num.get("pos") == "名詞" and next_num.get("pos_sub1") == "数":
                    combined += "-" + next_num.get("surface", "")
                    j += 2
                    has_hyphen = True
                else:
                    break
            if has_hyphen:
                result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "address-number"

        # 2b. Prefix + number + counter
        if not merged and t.get("pos") == "接頭詞" and t.get("pos_sub1") == "数接続":
            j = i + 1
            combined = t.get("surface", "")
            while j < len(tokens):
                nxt = tokens[j]
                is_number = nxt.get("pos") == "名詞" and nxt.get("pos_sub1") == "数"
                is_counter = (
                    nxt.get("pos") == "名詞" and nxt.get("pos_sub1") == "接尾" and nxt.get("pos_sub2") == "助数詞"
                )
                if is_number or is_counter:
                    combined += nxt.get("surface", "")
                    j += 1
                else:
                    break
            if j > i + 1:
                result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "number+unit"

        # 2c. Noun + 書/誌 suffix
        if not merged and t.get("pos") == "名詞" and i + 1 < len(tokens):
            nxt = tokens[i + 1]
            if nxt.get("surface", "") in ("書", "誌") and nxt.get("pos") == "名詞" and nxt.get("pos_sub1") == "接尾":
                combined = t.get("surface", "") + nxt["surface"]
                result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                i += 2
                merged = True
                if applied_rule is None:
                    applied_rule = "noun+suffix-char"

        # 2c2. Noun + 時/率/性 suffix
        if not merged and t.get("pos") == "名詞" and i + 1 < len(tokens):
            nxt = tokens[i + 1]
            if (
                nxt.get("surface", "") in ("時", "率", "性")
                and nxt.get("pos") == "名詞"
                and nxt.get("pos_sub1") == "接尾"
            ):
                combined = t.get("surface", "") + nxt["surface"]
                result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                i += 2
                merged = True
                if applied_rule is None:
                    applied_rule = "noun+suffix"

        # 2c3. Version number
        if not merged and t.get("surface", "") in ("v", "V") and i + 1 < len(tokens):
            j = i + 1
            combined = t["surface"]
            while j < len(tokens):
                ns = tokens[j].get("surface", "")
                if regex.match(r"^\d+$", ns) or ns == ".":
                    combined += ns
                    j += 1
                else:
                    break
            if j > i + 1:
                result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "version"

        # 2c4. Brand + number
        if not merged and regex.match(r"^[A-Za-z]+$", t.get("surface", "")) and t.get("pos") == "名詞":
            if i + 1 < len(tokens):
                nxt = tokens[i + 1]
                ns = nxt.get("surface", "")
                if regex.match(r"^\d+$", ns) and nxt.get("pos") == "名詞":
                    combined = t["surface"] + ns
                    result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                    i += 2
                    merged = True
                    if applied_rule is None:
                        applied_rule = "brand+number"

        # 2d. Prefix + Noun (kanji only)
        if not merged and t.get("pos") == "接頭詞" and t.get("pos_sub1") == "名詞接続" and i + 1 < len(tokens):
            nxt = tokens[i + 1]
            if nxt.get("pos") == "名詞":
                combined = t.get("surface", "") + nxt.get("surface", "")
                if regex.match(r"^[\p{Han}]+$", combined):
                    result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                    i += 2
                    merged = True
                    if applied_rule is None:
                        applied_rule = "prefix+noun"

        # 3. Nai-adjective merge
        if not merged:
            for adj in NAI_ADJECTIVES:
                if remaining.startswith(adj):
                    length = 0
                    j = i
                    while j < len(tokens) and length < len(adj):
                        length += len(tokens[j].get("surface", ""))
                        j += 1
                    if length == len(adj):
                        result.append({"surface": adj, "pos": "形容詞", "lemma": adj})
                        i = j
                        merged = True
                        if applied_rule is None:
                            applied_rule = "nai-adjective"
                        break

        # 4. Elongated adjective
        if not merged and t.get("pos") == "形容詞" and t.get("conj_form") == "ガル接続":
            j = i + 1
            if j < len(tokens) and tokens[j].get("surface") == "ー":
                combined = t.get("surface", "") + "ー"
                lemma = t.get("lemma") or (t.get("surface", "") + "い")
                j += 1
                if j < len(tokens):
                    ns = tokens[j].get("surface", "")
                    if ns == "い":
                        combined += "い"
                        j += 1
                    elif regex.match(r"^い(ね|よ|な|わ|ぞ|さ|か|の|けど)$", ns):
                        particle = ns[1:]
                        combined += "い"
                        result.append({"surface": combined, "pos": "形容詞", "lemma": lemma})
                        result.append({"surface": particle, "pos": "助詞", "lemma": particle})
                        i = j + 1
                        merged = True
                        if applied_rule is None:
                            applied_rule = "elongated-adjective"
                if not merged:
                    result.append({"surface": combined, "pos": "形容詞", "lemma": lemma})
                    i = j
                    merged = True
                    if applied_rule is None:
                        applied_rule = "elongated-adjective"

        # 4b. Vowel repetition: verb + repeated う (2+)
        if not merged and t.get("pos") == "動詞":
            j = i + 1
            combined = t.get("surface", "")
            lemma = t.get("lemma") or t.get("surface", "")
            u_count = 0
            while j < len(tokens):
                nxt = tokens[j]
                if nxt.get("surface") == "う" and nxt.get("pos") == "助動詞":
                    combined += "う"
                    u_count += 1
                    j += 1
                else:
                    break
            if u_count >= 2:
                result.append({"surface": combined, "pos": "動詞", "lemma": lemma})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "vowel-repeat"

        # 4c. Emphatic sokuon in past tense
        if not merged and t.get("pos") == "動詞" and "連用" in (t.get("conj_form") or ""):
            j = i + 1
            if j < len(tokens) and tokens[j].get("surface") == "たっ":
                combined = t.get("surface", "") + "たっ"
                lemma = t.get("lemma") or t.get("surface", "")
                result.append({"surface": combined, "pos": "動詞", "lemma": lemma})
                i = j + 1
                merged = True
                if applied_rule is None:
                    applied_rule = "emphatic-sokuon"

        # 4d. Adjective vowel repetition
        if not merged and t.get("pos") == "形容詞" and t.get("surface", "").endswith("い"):
            j = i + 1
            if j < len(tokens):
                nxt = tokens[j]
                if nxt.get("surface") == "いい" and nxt.get("pos") == "形容詞":
                    combined = t.get("surface", "") + "いい"
                    lemma = t.get("lemma") or t.get("surface", "")
                    result.append({"surface": combined, "pos": "形容詞", "lemma": lemma})
                    i = j + 1
                    merged = True
                    if applied_rule is None:
                        applied_rule = "vowel-repeat"

        # 5. タリ活用副詞
        if not merged:
            for stem in TARI_ADVERB_STEMS:
                adverb = stem + "と"
                if remaining.startswith(adverb):
                    length = 0
                    j = i
                    while j < len(tokens) and length < len(adverb):
                        length += len(tokens[j].get("surface", ""))
                        j += 1
                    if length == len(adverb):
                        result.append({"surface": adverb, "pos": "副詞", "lemma": stem})
                        i = j
                        merged = True
                        if applied_rule is None:
                            applied_rule = "tari-adverb"
                        break

        # 5a. Verb renyokei + 会
        if not merged and t.get("pos") == "動詞" and t.get("conj_form") == "連用形":
            j = i + 1
            if j < len(tokens):
                nxt = tokens[j]
                if nxt.get("surface") == "会" and nxt.get("pos") == "名詞" and nxt.get("pos_sub1") == "接尾":
                    combined = t.get("surface", "") + "会"
                    result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                    i = j + 1
                    merged = True
                    if applied_rule is None:
                        applied_rule = "verb-renyokei+kai"

        # 5b. Proper noun + region suffix
        if not merged and t.get("pos") == "名詞" and t.get("pos_sub1") == "固有名詞" and t.get("pos_sub2") == "地域":
            j = i + 1
            combined = t.get("surface", "")
            while j < len(tokens):
                nxt = tokens[j]
                ns = nxt.get("surface", "")
                if ns == "行き":
                    break
                is_proper_region = (
                    nxt.get("pos") == "名詞" and nxt.get("pos_sub1") == "固有名詞" and nxt.get("pos_sub2") == "地域"
                )
                is_region_suffix = (
                    nxt.get("pos") == "名詞" and nxt.get("pos_sub1") == "接尾" and nxt.get("pos_sub2") == "地域"
                )
                is_kanji_noun = (
                    nxt.get("pos") == "名詞" and regex.match(r"^[\p{Han}]+$", ns) and nxt.get("pos_sub1") != "接尾"
                )
                if is_proper_region or is_region_suffix or is_kanji_noun:
                    combined += ns
                    j += 1
                else:
                    break
            if j > i + 1:
                result.append({"surface": combined, "pos": "名詞", "pos_sub1": "固有名詞", "lemma": combined})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "proper-noun"

        # 6. Kanji compound
        is_mergeable_kanji = (
            regex.match(r"^[\p{Han}]+$", t.get("surface", ""))
            and t.get("pos") == "名詞"
            and t.get("pos_sub1", "") not in ("接尾", "固有名詞", "副詞可能")
        )
        if not merged and is_mergeable_kanji:
            j = i + 1
            combined = t.get("surface", "")
            while j < len(tokens):
                nxt = tokens[j]
                ns = nxt.get("surface", "")
                next_mergeable = (
                    regex.match(r"^[\p{Han}]+$", ns)
                    and nxt.get("pos") == "名詞"
                    and nxt.get("pos_sub1", "") not in ("接尾", "固有名詞", "形容動詞語幹", "副詞可能")
                )
                if not next_mergeable:
                    break
                combined += ns
                j += 1
            # Merge with common noun-forming suffixes
            if j < len(tokens):
                ns = tokens[j].get("surface", "")
                if ns in ("付け", "者", "人"):
                    combined += ns
                    j += 1
            if j > i + 1:
                result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "kanji-compound"

        # 7. Katakana compound
        if not merged and regex.match(r"^[\u30A0-\u30FF]+$", t.get("surface", "")) and t.get("pos") == "名詞":
            j = i + 1
            combined = t.get("surface", "")
            while (
                j < len(tokens)
                and regex.match(r"^[\u30A0-\u30FF]+$", tokens[j].get("surface", ""))
                and tokens[j].get("pos") == "名詞"
            ):
                combined += tokens[j].get("surface", "")
                j += 1
            if j > i + 1:
                result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "katakana-compound"

        # 7b. Alphabet + Katakana/Kanji compound
        if not merged and regex.match(r"^[A-Za-z]+$", t.get("surface", "")) and t.get("pos") == "名詞":
            j = i + 1
            combined = t.get("surface", "")
            while j < len(tokens):
                nxt = tokens[j]
                ns = nxt.get("surface", "")
                np = nxt.get("pos", "")
                is_katakana = regex.match(r"^[\u30A0-\u30FF]+$", ns) and np == "名詞"
                is_kanji = regex.match(r"^[\p{Han}]+$", ns) and np == "名詞"
                if is_katakana or is_kanji:
                    combined += ns
                    j += 1
                    break  # Only merge one following token
                else:
                    break
            if j > i + 1:
                result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "alphabet-compound"

        # 7c. Snake_case identifier
        if not merged and regex.match(r"^[A-Za-z0-9]+$", t.get("surface", "")) and t.get("pos") == "名詞":
            j = i + 1
            combined = t.get("surface", "")
            found_underscore = False
            while j < len(tokens):
                nxt = tokens[j]
                if nxt.get("surface") == "_":
                    if j + 1 < len(tokens):
                        after = tokens[j + 1]
                        if regex.match(r"^[A-Za-z0-9]+$", after.get("surface", "")):
                            combined += "_" + after["surface"]
                            j += 2
                            found_underscore = True
                            continue
                    break
                else:
                    break
            if found_underscore:
                result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "snake-case"

        # 7b. Mention pattern
        if not merged and t.get("surface") == "@":
            j = i + 1
            combined = "@"
            found_mention = False
            while j < len(tokens):
                ns = tokens[j].get("surface", "")
                if regex.match(r"^[A-Za-z0-9]+$", ns):
                    combined += ns
                    j += 1
                    found_mention = True
                elif ns == "_" and found_mention:
                    if j + 1 < len(tokens) and regex.match(r"^[A-Za-z0-9]+$", tokens[j + 1].get("surface", "")):
                        combined += "_"
                        j += 1
                    else:
                        break
                else:
                    break
            if found_mention:
                result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                i = j
                merged = True
                if applied_rule is None:
                    applied_rule = "mention"

        # 7c. Hashtag pattern
        if not merged and t.get("surface") in ("#", "＃"):
            if i + 1 < len(tokens):
                ns = tokens[i + 1].get("surface", "")
                if regex.match(r"^[\p{Katakana}\p{Han}A-Za-z0-9_]+$", ns):
                    combined = t["surface"] + ns
                    result.append({"surface": combined, "pos": "名詞", "lemma": combined})
                    i += 2
                    merged = True
                    if applied_rule is None:
                        applied_rule = "hashtag"

        # 8. Colloquial pronouns
        if not merged:
            for pronoun in COLLOQUIAL_PRONOUNS:
                if remaining.startswith(pronoun):
                    length = 0
                    j = i
                    while j < len(tokens) and length < len(pronoun):
                        length += len(tokens[j].get("surface", ""))
                        j += 1
                    if length == len(pronoun):
                        result.append({"surface": pronoun, "pos": "代名詞", "lemma": pronoun})
                        i = j
                        merged = True
                        if applied_rule is None:
                            applied_rule = "colloquial-pronoun"
                        break

        # 8b. Character speech: にゃ+ん -> にゃん
        if not merged and t.get("surface") == "にゃ":
            if i + 1 < len(tokens) and tokens[i + 1].get("surface") == "ん":
                result.append({"surface": "にゃん", "pos": "助詞", "lemma": "にゃん"})
                i += 2
                merged = True
                if applied_rule is None:
                    applied_rule = "character-speech"

        # 9. Compound verbs
        if not merged and t.get("pos") == "動詞" and "連用" in (t.get("conj_form") or ""):
            j = i + 1
            if j < len(tokens):
                nxt = tokens[j]
                if nxt.get("pos") == "動詞":
                    next_lemma = nxt.get("lemma") or nxt.get("surface", "")
                    v2_base = ""
                    for v2 in COMPOUND_VERB_V2_GODAN + COMPOUND_VERB_V2_ICHIDAN:
                        if next_lemma == v2:
                            v2_base = v2
                            break
                    if v2_base:
                        combined = t.get("surface", "") + nxt.get("surface", "")
                        compound_lemma = t.get("surface", "") + v2_base
                        result.append({"surface": combined, "pos": "動詞", "lemma": compound_lemma})
                        i = j + 1
                        merged = True
                        if applied_rule is None:
                            applied_rule = "compound-verb"

        # 10. Lexicalized hiragana words
        if not merged:
            for word in sorted(HIRAGANA_COMPOUNDS.keys(), key=len, reverse=True):
                if remaining.startswith(word):
                    length = 0
                    j = i
                    while j < len(tokens) and length < len(word):
                        length += len(tokens[j].get("surface", ""))
                        j += 1
                    if length == len(word):
                        result.append({"surface": word, "pos": HIRAGANA_COMPOUNDS[word], "lemma": word})
                        i = j
                        merged = True
                        if applied_rule is None:
                            applied_rule = "hiragana-compound"
                        break

        # 11. Colloquial intensifier めちゃ
        if not merged and t.get("surface") == "め" and t.get("pos") == "名詞":
            if i + 1 < len(tokens) and tokens[i + 1].get("surface") == "ちゃ":
                result.append({"surface": "めちゃ", "pos": "その他", "lemma": "めちゃ"})
                i += 2
                merged = True
                if applied_rule is None:
                    applied_rule = "mecha-merge"

        # 11b. ず+に -> ずに
        if not merged and t.get("surface") == "ず" and t.get("pos") == "助動詞":
            if i + 1 < len(tokens) and tokens[i + 1].get("surface") == "に":
                result.append({"surface": "ずに", "pos": "助動詞", "lemma": "ず"})
                i += 2
                merged = True
                if applied_rule is None:
                    applied_rule = "zu-ni-merge"

        # No merge: pass through
        if not merged:
            result.append(
                {
                    "surface": t.get("surface", ""),
                    "pos": t.get("pos", ""),
                    "pos_sub1": t.get("pos_sub1"),
                    "pos_sub2": t.get("pos_sub2"),
                    "conj_type": t.get("conj_type"),
                    "conj_form": t.get("conj_form"),
                    "lemma": t.get("lemma") or t.get("surface", ""),
                }
            )
            i += 1

    # Post-process passes
    result = _postprocess_kamo(result, applied_rule)
    result, applied_rule = _postprocess_noni(result, applied_rule)
    result, applied_rule = _postprocess_atode(result, applied_rule)
    _postprocess_epenthetic_sa(result)
    result, applied_rule = _postprocess_honorific_split(result, applied_rule)
    result, applied_rule = _postprocess_prefix_split(result, applied_rule)
    result, applied_rule = _postprocess_nde_split(result, applied_rule)
    result, applied_rule = _postprocess_filler_split(result, applied_rule)
    result, applied_rule = _postprocess_kuruwa(result, applied_rule)
    result, applied_rule = _postprocess_kanji_merge(result, applied_rule)
    result, applied_rule = _postprocess_search_unit_split(result, applied_rule)
    result, applied_rule = _postprocess_onomatopoeia_tto_merge(result, applied_rule)
    result, applied_rule = _postprocess_ascii_dot_merge(result, applied_rule)
    _postprocess_dialectal(result)

    return result, applied_rule


def _postprocess_kamo(result: list[dict], applied_rule: str | None) -> list[dict]:
    """Merge か+も -> かも (compound particle)."""
    merged = []
    skip_next = False
    for j, curr in enumerate(result):
        if skip_next:
            skip_next = False
            continue
        if (
            j < len(result) - 1
            and curr.get("surface") == "か"
            and curr.get("pos") == "助詞"
            and result[j + 1].get("surface") == "も"
            and result[j + 1].get("pos") == "助詞"
        ):
            merged.append({"surface": "かも", "pos": "助詞", "lemma": "かも"})
            skip_next = True
        else:
            merged.append(curr)
    return merged


def _postprocess_noni(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Merge の+に -> のに after past tense."""
    merged = []
    skip_next = False
    for j, curr in enumerate(result):
        if skip_next:
            skip_next = False
            continue
        if (
            j >= 1
            and j < len(result) - 1
            and curr.get("surface") == "の"
            and result[j + 1].get("surface") == "に"
            and (result[j - 1].get("surface", "") in ("た", "っ", "だ") or result[j - 1].get("pos") == "助動詞")
        ):
            merged.append({"surface": "のに", "pos": "助詞", "lemma": "のに"})
            skip_next = True
            if applied_rule is None:
                applied_rule = "noni-merge"
        else:
            merged.append(curr)
    return merged, applied_rule


def _postprocess_atode(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Split 後で(副詞) -> 後+で."""
    new_result = []
    for t in result:
        if t.get("surface") == "後で" and t.get("pos") == "副詞":
            new_result.append({"surface": "後", "pos": "名詞", "lemma": "後"})
            new_result.append({"surface": "で", "pos": "助詞", "lemma": "で"})
            if applied_rule is None:
                applied_rule = "atode-split"
        else:
            new_result.append(t)
    return new_result, applied_rule


def _postprocess_epenthetic_sa(result: list[dict]) -> None:
    """Fix epenthetic さ in adjective+さ+そう pattern."""
    for j in range(1, len(result) - 1):
        prev = result[j - 1]
        curr = result[j]
        nxt = result[j + 1]
        if (
            prev.get("pos") == "形容詞"
            and curr.get("surface") == "さ"
            and curr.get("pos_sub1") == "接尾"
            and nxt.get("surface") == "そう"
        ):
            curr["pos"] = "Suffix"
            curr.pop("pos_sub1", None)
            curr.pop("pos_sub2", None)


def _postprocess_honorific_split(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Split honorific patterns like 皆様 -> 皆+様."""
    honorific_re = "|".join(regex.escape(s) for s in HONORIFIC_SUFFIXES)

    new_result = []
    for t in result:
        surface = t.get("surface", "")
        if surface not in HONORIFIC_EXCEPTIONS:
            m = regex.match(rf"^(お)?([\p{{Han}}]+)({honorific_re})$", surface)
            if m:
                prefix, kanji, suffix = m.group(1), m.group(2), m.group(3)
                if prefix:
                    new_result.append({"surface": prefix, "pos": "接頭詞", "lemma": prefix})
                new_result.append({"surface": kanji, "pos": "名詞", "lemma": kanji})
                new_result.append({"surface": suffix, "pos": "名詞", "pos_sub1": "接尾", "lemma": suffix})
                if applied_rule is None:
                    applied_rule = "honorific-split"
                continue
        new_result.append(t)
    return new_result, applied_rule


def _postprocess_prefix_split(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Split お/ご+noun patterns."""
    new_result = []
    for t in result:
        surface = t.get("surface", "")
        pos = t.get("pos", "")
        pos_sub1 = t.get("pos_sub1", "")
        if pos == "名詞" and pos_sub1 != "接尾" and surface not in PREFIX_EXCEPTIONS:
            m = regex.match(r"^(お|ご)([\p{Han}\p{Hiragana}]+)$", surface)
            if m:
                prefix, noun = m.group(1), m.group(2)
                new_result.append({"surface": prefix, "pos": "接頭詞", "lemma": prefix})
                new_result.append({"surface": noun, "pos": "名詞", "lemma": noun})
                if applied_rule is None:
                    applied_rule = "prefix-split"
                continue
        new_result.append(t)
    return new_result, applied_rule


def _postprocess_nde_split(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Split contracted んでる verb forms."""
    new_result = []
    for t in result:
        surface = t.get("surface", "")
        pos = t.get("pos", "")
        m = regex.match(r"^(.+ん)(で)(る)$", surface)
        if pos == "動詞" and m:
            stem, de, ru = m.group(1), m.group(2), m.group(3)
            new_result.append({"surface": stem, "pos": "動詞", "lemma": t.get("lemma") or stem})
            new_result.append({"surface": de, "pos": "助詞", "lemma": de})
            new_result.append({"surface": ru, "pos": "動詞", "lemma": "いる"})
            if applied_rule is None:
                applied_rule = "nde-contract-split"
        else:
            new_result.append(t)
    return new_result, applied_rule


def _postprocess_filler_split(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Split filler tokens like そうですね -> そう+です+ね."""
    new_result = []
    for t in result:
        surface = t.get("surface", "")
        pos = t.get("pos", "")
        m = regex.match(r"^(そう)(です)(ね|か|よ|よね)?$", surface)
        if pos == "フィラー" and m:
            sou, desu, particle = m.group(1), m.group(2), m.group(3)
            new_result.append({"surface": sou, "pos": "名詞", "pos_sub1": "形容動詞語幹", "lemma": sou})
            new_result.append({"surface": desu, "pos": "助動詞", "lemma": "です"})
            if particle:
                new_result.append({"surface": particle, "pos": "助詞", "lemma": particle})
            if applied_rule is None:
                applied_rule = "filler-split"
        else:
            new_result.append(t)
    return new_result, applied_rule


def _postprocess_kuruwa(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Fix kuruwa kotoba segmentation: あ+りん -> あり+ん."""
    new_result = []
    skip_next = False
    for j, curr in enumerate(result):
        if skip_next:
            skip_next = False
            continue
        if curr.get("surface") == "あ" and j + 1 < len(result) and result[j + 1].get("surface") == "りん":
            new_result.append({"surface": "あり", "pos": "動詞", "lemma": "ある"})
            new_result.append({"surface": "ん", "pos": "助動詞", "lemma": "ん"})
            skip_next = True
            if applied_rule is None:
                applied_rule = "kuruwa-fix"
        else:
            new_result.append(curr)
    return new_result, applied_rule


def _postprocess_kanji_merge(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Merge consecutive all-kanji tokens."""
    merged = []
    for curr in result:
        surface = curr.get("surface", "")
        if (
            merged
            and regex.match(r"^[\p{Han}]+$", surface)
            and surface != "的"
            and regex.match(r"^[\p{Han}]+$", merged[-1].get("surface", ""))
            and "々" not in merged[-1].get("surface", "")
            and merged[-1].get("pos_sub1", "") not in ("副詞可能", "固有名詞")
            and merged[-1].get("pos", "") != "副詞"
            and curr.get("pos_sub1", "") != "接尾"
        ):
            merged[-1]["surface"] += surface
            merged[-1]["lemma"] = merged[-1]["surface"]
            merged[-1]["pos"] = "名詞"
            if applied_rule is None:
                applied_rule = "kanji-merge"
        else:
            merged.append(curr)
    return merged, applied_rule


def _postprocess_search_unit_split(
    result: list[dict], applied_rule: str | None
) -> tuple[list[dict], str | None]:
    """Re-split kanji-merged tokens that absorbed part of a search-unit compound.

    Example: kanji-merge produces AB+C, but BC should be one token.
    This splits AB → A+B, then merges B+C → BC.
    """
    new_result: list[dict] = []
    skip_next = False
    for j, curr in enumerate(result):
        if skip_next:
            skip_next = False
            continue
        if j < len(result) - 1:
            nxt = result[j + 1]
            curr_surface = curr.get("surface", "")
            nxt_surface = nxt.get("surface", "")
            for word, word_pos in SEARCH_UNIT_COMPOUNDS.items():
                # Check if word spans across curr (ending) + nxt (beginning)
                for split_pos in range(1, len(word)):
                    prefix = word[:split_pos]
                    suffix = word[split_pos:]
                    if curr_surface.endswith(prefix) and nxt_surface == suffix:
                        head = curr_surface[: -len(prefix)]
                        if head:
                            new_result.append({"surface": head, "pos": curr.get("pos", ""), "lemma": head})
                        new_result.append({"surface": word, "pos": word_pos, "lemma": word})
                        skip_next = True
                        if applied_rule is None:
                            applied_rule = "search-unit-split"
                        break
                if skip_next:
                    break
        if not skip_next or j < len(result) - 1:
            if not skip_next:
                new_result.append(curr)
    # Handle last token if not skipped
    if not skip_next and len(result) > 0:
        pass  # Already appended in the loop
    return new_result, applied_rule


def _postprocess_ascii_dot_merge(result: list[dict], applied_rule: str | None) -> tuple[list[dict], str | None]:
    """Merge ASCII + dot + ASCII/number tokens."""
    merged = []
    for j, curr in enumerate(result):
        surface = curr.get("surface", "")
        if (
            surface == "."
            and merged
            and regex.match(r"^[a-zA-Z]+$", merged[-1].get("surface", ""))
            and j + 1 < len(result)
            and regex.match(r"^[a-zA-Z0-9]+$", result[j + 1].get("surface", ""))
        ):
            merged[-1]["surface"] += "."
            merged[-1]["lemma"] = merged[-1]["surface"]
            if applied_rule is None:
                applied_rule = "ascii-dot-merge"
        elif merged and merged[-1].get("surface", "").endswith(".") and regex.match(r"^[a-zA-Z0-9]+$", surface):
            merged[-1]["surface"] += surface
            merged[-1]["lemma"] = merged[-1]["surface"]
        else:
            merged.append(curr)
    return merged, applied_rule


def _postprocess_onomatopoeia_tto_merge(
    result: list[dict], applied_rule: str | None
) -> tuple[list[dict], str | None]:
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
