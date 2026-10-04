/**
 * @file join_compound_verb.cpp
 * @brief Kanji-led compound-verb join candidate generation
 */

#include <algorithm>

#include "grammar/honorific_verbs.h"
#include "join_compound_verb_internal.h"

namespace suzume::analysis {

using namespace compound_verb_detail;

namespace {

void addDictionaryVerifiedIchidanCompoundNominalCandidate(core::Lattice& lattice, std::string_view text,
                                                          const std::vector<char32_t>& codepoints,
                                                          const ByteOffsets& byte_offsets, size_t start_pos,
                                                          const std::vector<normalize::CharType>& char_types,
                                                          const dictionary::DictionaryManager& dict_manager,
                                                          const Scorer& scorer) {
  // A single-kanji Ichidan V1 may be written without its stem vowel before a kanji
  // V2 (見+立てる), so a lexicalized compound of that shape carries no medial
  // hiragana at all. Its minimum length is one character shorter for the same
  // reason. The dictionary lemma required below is what licenses either shape.
  constexpr size_t kMinBareIchidanV1Length = 3;
  constexpr size_t kMinRenyokeiV1Length = 4;
  const bool bare_ichidan_v1_shape = start_pos + 1 < char_types.size() && char_types[start_pos + 1] == CharType::Kanji;
  const size_t min_length = bare_ichidan_v1_shape ? kMinBareIchidanV1Length : kMinRenyokeiV1Length;

  size_t end_pos = start_pos + 1;
  bool has_hiragana = false;
  bool has_kanji_after_hiragana = false;

  while (end_pos < codepoints.size() && end_pos - start_pos <= 6) {
    if (beginsNominalForcingParticle(codepoints, end_pos, dict_manager)) {
      break;
    }
    if (char_types[end_pos] == CharType::Hiragana) {
      has_hiragana = true;
    } else if (char_types[end_pos] == CharType::Kanji) {
      has_kanji_after_hiragana = has_kanji_after_hiragana || has_hiragana;
    } else {
      return;
    }
    ++end_pos;
  }

  if (end_pos >= codepoints.size() || end_pos - start_pos < min_length ||
      !(has_kanji_after_hiragana || bare_ichidan_v1_shape) || !kana::isERowCodepoint(codepoints[end_pos - 1]) ||
      !beginsNominalForcingParticle(codepoints, end_pos, dict_manager)) {
    return;
  }

  const std::string surface = extractSubstring(codepoints, start_pos, end_pos);
  const std::string lemma = surface + "る";
  if (dict_manager.lookupExact(lemma, core::PartOfSpeech::Verb) == nullptr) {
    return;
  }

  lattice.addEdge(textRange(text, byte_offsets, start_pos, end_pos), static_cast<uint32_t>(start_pos),
                  static_cast<uint32_t>(end_pos), core::PartOfSpeech::Noun,
                  scorer.posPrior(core::PartOfSpeech::Noun) + candidate::kCompoundVerbSuffixNounBonus,
                  core::LatticeEdge::kFromDictionary, surface, dictionary::ConjugationType::None,
                  core::CandidateOrigin::VerbCompound, candidate::kNoOriginConfidence,
                  "dictionary_ichidan_compound_nominal", core::ExtendedPOS::NounVerbal,
                  "dictionary_ichidan_compound_nominal");
}

void addDictionaryVerifiedGodanCompoundNominalCandidate(core::Lattice& lattice, std::string_view text,
                                                        const std::vector<char32_t>& codepoints,
                                                        const ByteOffsets& byte_offsets, size_t start_pos,
                                                        const std::vector<normalize::CharType>& char_types,
                                                        const dictionary::DictionaryManager& dict_manager,
                                                        const Scorer& scorer) {
  const size_t v1_kanji_end = findCharRegionEnd(char_types, start_pos, 3, CharType::Kanji);
  if (v1_kanji_end >= codepoints.size() || char_types[v1_kanji_end] != CharType::Hiragana ||
      !kana::isIRowCodepoint(codepoints[v1_kanji_end])) {
    return;
  }
  const std::string_view v1_base_ending = grammar::godanBaseSuffixFromIRow(codepoints[v1_kanji_end]);
  if (v1_base_ending.empty()) {
    return;
  }
  const std::string v1_base = normalize::concat(extractSubstring(codepoints, start_pos, v1_kanji_end), v1_base_ending);
  if (dict_manager.lookupExact(v1_base, core::PartOfSpeech::Verb) == nullptr) {
    return;
  }

  const size_t v2_start = v1_kanji_end + 1;
  if (v2_start >= codepoints.size() || char_types[v2_start] != CharType::Kanji) {
    return;
  }
  size_t end_pos = v2_start;
  while (end_pos < codepoints.size() && end_pos - start_pos <= 7) {
    if (beginsNominalForcingParticle(codepoints, end_pos, dict_manager)) {
      break;
    }
    if (char_types[end_pos] != CharType::Kanji && char_types[end_pos] != CharType::Hiragana) {
      return;
    }
    ++end_pos;
  }
  if (end_pos >= codepoints.size() || end_pos <= v2_start + 1 || !kana::isIRowCodepoint(codepoints[end_pos - 1]) ||
      !beginsNominalForcingParticle(codepoints, end_pos, dict_manager)) {
    return;
  }

  // An i-row V2 can also be an Ichidan stem (使い過ぎる).  Those known
  // subsidiary verbs are handled by the ordinary compound matcher; this
  // fallback is only for an otherwise unregistered Godan V2 such as 畳む.
  const std::string v2_surface = extractSubstring(codepoints, v2_start, end_pos);
  if (dict_manager.lookupExact(v2_surface + "る", core::PartOfSpeech::Verb) != nullptr) {
    return;
  }

  // An aspectual V2 stays an independent search unit, exactly as it does in its
  // finite form (読み|終わる).  Without this the nominalized form of the same
  // compound would join (読み終わり) while the finite one splits.
  const std::string v2_base = extractSubstring(codepoints, v2_start, end_pos - 1) +
                              std::string(grammar::godanBaseSuffixFromIRow(codepoints[end_pos - 1]));
  if (grammar::isAspectualSubsidiaryLemma(v2_base)) {
    return;
  }

  const std::string surface = extractSubstring(codepoints, start_pos, end_pos);
  lattice.addEdge(textRange(text, byte_offsets, start_pos, end_pos), static_cast<uint32_t>(start_pos),
                  static_cast<uint32_t>(end_pos), core::PartOfSpeech::Noun,
                  scorer.posPrior(core::PartOfSpeech::Noun) + candidate::kCompoundVerbSuffixNounBonus,
                  core::LatticeEdge::kFromDictionary, surface, dictionary::ConjugationType::None,
                  core::CandidateOrigin::VerbCompound, candidate::kNoOriginConfidence,
                  "dictionary_godan_compound_nominal", core::ExtendedPOS::NounVerbal,
                  "dictionary_godan_compound_nominal");
}

}  // namespace

void addCompoundVerbJoinCandidates(core::Lattice& lattice, std::string_view text,
                                   const std::vector<char32_t>& codepoints, const ByteOffsets& byte_offsets,
                                   size_t start_pos, const std::vector<normalize::CharType>& char_types,
                                   const dictionary::DictionaryManager& dict_manager, const Scorer& scorer,
                                   const grammar::Inflection& inflection) {
  if (start_pos >= char_types.size()) {
    return;
  }

  // Must start with kanji (V1 verb stem)
  if (char_types[start_pos] != CharType::Kanji) {
    return;
  }

  // A lexicalized Ichidan compound can appear as a nominalized continuative
  // before a case particle (折り曲げを).  Such forms must remain one search
  // unit even when their V2 is not in the closed subsidiary-verb lexicon.
  // The complete dictionary lemma is required, so arbitrary V1+V2 sequences
  // and aspectual forms such as 書き始め stay compositional.
  addDictionaryVerifiedIchidanCompoundNominalCandidate(lattice, text, codepoints, byte_offsets, start_pos, char_types,
                                                       dict_manager, scorer);
  addDictionaryVerifiedGodanCompoundNominalCandidate(lattice, text, codepoints, byte_offsets, start_pos, char_types,
                                                     dict_manager, scorer);

  // Find the kanji portion (V1 stem)
  size_t kanji_end = findCharRegionEnd(char_types, start_pos, 4, CharType::Kanji);

  // Next must be hiragana (連用形 ending), except for a single-kanji
  // ichidan V1 directly followed by a kanji V2 (見上げる).
  if (kanji_end >= char_types.size() || char_types[kanji_end] != CharType::Hiragana) {
    return;
  }

  // Get the hiragana character (potential 連用形 ending)
  char32_t renyokei_char = codepoints[kanji_end];

  // A case/topic particle after a kanji noun cannot be the first mora of a
  // compound-verb V1 stem. Without this boundary, an embedded verb later in
  // the phrase can incorrectly license the whole span (本をとりかえる).
  if (normalize::isNeverVerbStemAfterKanji(renyokei_char)) {
    return;
  }

  // A leading one-kanji ichidan stem can be written without its stem vowel
  // before a kanji V2. The initial kanji run then contains both verbs, so
  // start matching V2 after the first kanji instead of treating its last
  // hiragana as V1's stem ending. A Godan renyokei remains a boundary after
  // the full kanji sequence (提出+し), not evidence for a bare Ichidan stem.
  //
  // Only a known single-kanji Ichidan verb can be read this way: the stem
  // vowel is unwritten, so nothing else in the span identifies the kanji as
  // verbal. Without that evidence any kanji whose neighbour happens to open a
  // dictionary verb becomes a V1, and the compound is carved out of the
  // interior of a two-kanji kango (提+出す for 提出する). The same closed set
  // already gates the retry below, which spells this construction the other
  // way round.
  const bool has_bound_verb_prefix = grammar::isBoundVerbPrefix(extractSubstring(codepoints, start_pos, start_pos + 1));
  const bool has_kanji_v2_after_bare_ichidan =
      kanji_end >= start_pos + 2 && char_types[start_pos + 1] == CharType::Kanji &&
      godanRenyokeiBaseCp(renyokei_char) == 0 &&
      (verb_helpers::isSingleKanjiIchidan(codepoints[start_pos]) || has_bound_verb_prefix);

  if (addPassiveContinuativeTailCandidates(lattice, codepoints, kanji_end)) {
    return;
  }

  // Check if it's a valid 連用形 ending
  char32_t base_ending = godanRenyokeiBaseCp(renyokei_char);

  // Check for sokuonbin (っ) compound pattern: 突っ込む, 引っ張る, ぶっ壊す
  // Sokuonbin verbs: godan-ka(く), godan-ta(つ), godan-wa(う), godan-ra(る)
  bool is_sokuonbin = (base_ending == 0 && renyokei_char == U'っ');
  SUZUME_DEBUG_LOG_VERBOSE("[COMPOUND] pos=" << start_pos << " kanji_end=" << kanji_end
                                             << " renyokei=" << normalize::encodeUtf8(renyokei_char) << " base_ending="
                                             << (base_ending ? normalize::encodeUtf8(base_ending) : "0")
                                             << " sokuonbin=" << is_sokuonbin << "\n");

  // If not a 連用形 ending, this might be an Ichidan verb
  bool is_ichidan = (base_ending == 0 && !is_sokuonbin) || has_kanji_v2_after_bare_ichidan;

  // Position after 連用形 (for Godan) or after stem (for Ichidan)
  size_t v2_start;
  if (has_kanji_v2_after_bare_ichidan) {
    v2_start = start_pos + 1;
  } else if (is_sokuonbin) {
    // For sokuonbin: V2 starts after っ
    v2_start = kanji_end + 1;
  } else if (is_ichidan) {
    // For ichidan verbs, the stem includes the final hiragana character:
    // - Shimo-ichidan (下一段): え-row (抜け from 抜ける, 食べ from 食べる)
    // - Kami-ichidan (上一段): い-row (落ち from 落ちる, 起き from 起きる)
    // - Suru-variant: じ/ぢ (演じ from 演じる, 感じ from 感じる)
    // This hiragana belongs to V1, so V2 begins after it.
    // The i-row Godan continuatives (き, し, ...) already set base_ending above,
    // so an e/i-row kana here is a single-kana ichidan stem: 食べ+込む, 落ち+着く.
    const char32_t hira = codepoints[kanji_end];
    if (kana::isERowCodepoint(hira) || kana::isIRowCodepoint(hira)) {
      v2_start = kanji_end + 1;
    } else {
      // A multi-kana ichidan stem ends on the next e/i-row kana (生まれ+変わる);
      // without one, V2 may start at the kana itself (見+つける).
      v2_start = kanji_end;
      const size_t scan_limit = std::min(kanji_end + 3, codepoints.size());
      for (size_t scan_pos = kanji_end + 1; scan_pos < scan_limit && char_types[scan_pos] == CharType::Hiragana;
           ++scan_pos) {
        if (kana::isERowCodepoint(codepoints[scan_pos]) || kana::isIRowCodepoint(codepoints[scan_pos])) {
          v2_start = scan_pos + 1;
          break;
        }
      }
    }
  } else {
    v2_start = kanji_end + 1;
  }

  // Dict-verified compound V1: a lexicalized multi-morpheme verb (引きずる, L2) has a
  // renyokei (引きずり) with a hiragana-medial stem that the single-kanji-stem rule above
  // cannot represent, so a following subsidiary V2 never joins (引きずり|出す). If the
  // dictionary holds such a Verb renyokei (lemma≠surface, ≥3 chars, one leading kanji run
  // followed by hiragana) spanning start_pos, adopt it as V1: point V2 past it and mark it
  // ichidan-like so v1_renyokei_end == v2_start (compound base = 引きずり + 出す). Reuses the
  // whole V2-matching / emission path below. Scoped to the hiragana-medial shape so it does
  // not duplicate rule-handled compounds (走り出す, 話し合う).
  bool dict_compound_v1 = false;
  std::string dict_compound_v1_lemma;
  // Adopt a dictionary-verified V1 whose stem ends where V2 starts.
  const auto adopt_dictionary_v1 = [&](size_t verified_v2_start) {
    v2_start = verified_v2_start;
    is_sokuonbin = false;
    is_ichidan = true;  // makes v1_renyokei_end == v2_start_byte below (multi-hiragana stem)
    dict_compound_v1 = true;
  };
  {
    size_t probe_byte = byteOffsetAt(byte_offsets, start_pos);
    size_t best_len = 0;
    for (const auto& res : dict_manager.lookup(text, probe_byte)) {
      if (res.entry == nullptr || res.entry->pos != core::PartOfSpeech::Verb || res.length < 3 ||
          res.entry->lemma.empty() || res.entry->lemma == res.entry->surface ||
          start_pos + res.length >= codepoints.size() || res.length <= best_len) {
        continue;
      }
      // Require exactly one leading kanji run then hiragana (引 + きずり): the shape the rule
      // path mis-segments. Rejects multi-kanji-run compounds (話 し 合 い) already handled.
      if (char_types[start_pos + 1] != CharType::Hiragana) {
        continue;
      }
      const auto span_end = char_types.begin() + static_cast<std::ptrdiff_t>(start_pos + res.length);
      if (std::find(char_types.begin() + static_cast<std::ptrdiff_t>(start_pos + 2), span_end, CharType::Kanji) !=
          span_end) {
        continue;
      }
      best_len = res.length;
      dict_compound_v1_lemma = res.entry->lemma;
    }
    // Only adopt the dict V1 when a further kanji-written verb follows (出す, 回す): the
    // subsidiary chain is what this path is for. A hiragana continuation is an auxiliary
    // (引きずり|ます) that the plain dict renyokei edge already handles, so leave it alone.
    if (best_len > 0 && char_types[start_pos + best_len] == CharType::Kanji) {
      adopt_dictionary_v1(start_pos + best_len);
    }
  }

  // A multi-kanji ichidan V1 can end in the first hiragana after the leading
  // kanji run (仕立てる, 心掛ける).  The bare-ichidan branch above starts V2
  // after only the first kanji, which is right for 見上げる but loses that
  // lexical V1 shape.  When the reconstructed V1 is dictionary-verified,
  // retain its entire stem and begin matching the subsidiary V2 after it.
  if (!dict_compound_v1 && has_kanji_v2_after_bare_ichidan && kanji_end > start_pos + 1 &&
      kanji_end + 1 < codepoints.size() && char_types[kanji_end + 1] == CharType::Kanji &&
      (kana::isERowCodepoint(renyokei_char) || kana::isIRowCodepoint(renyokei_char))) {
    const std::string multi_kanji_ichidan_base = extractSubstring(codepoints, start_pos, kanji_end + 1) + "る";
    if (dict_manager.lookupExact(multi_kanji_ichidan_base, core::PartOfSpeech::Verb) != nullptr) {
      adopt_dictionary_v1(kanji_end + 1);
      dict_compound_v1_lemma = multi_kanji_ichidan_base;
    }
  }

  // Sokuonbin compound V1: 単漢字 + っ + 漢字 + 連用形 (引っ張り) whose embedded second verb
  // (張る) is dict-verified. The sokuonbin compound 引っ張る is rule-derivable, not in the
  // dictionary, so the branch above misses it; here the whole 引っ張り becomes V1 so a further
  // subsidiary (出す) chains → 引っ張り出す. The sokuon between two kanji is the compound
  // signal; a single sokuonbin compound with no trailing verb (突っ込み) is left untouched.
  if (!dict_compound_v1 && kanji_end == start_pos + 1 && kanji_end < codepoints.size() &&
      codepoints[kanji_end] == U'っ') {
    size_t k2_start = kanji_end + 1;
    if (k2_start < char_types.size() && char_types[k2_start] == CharType::Kanji) {
      size_t k2_end = findCharRegionEnd(char_types, k2_start, 3, CharType::Kanji);
      // Require a further kanji-written verb after the renyokei (引っ張り + 出す): this path
      // only chains a trailing subsidiary. Without it, 引っ越しました (引っ越す + aux) would be
      // hijacked and the plain 引っ越す compound lost.
      if (k2_end + 1 < codepoints.size() && char_types[k2_end] == CharType::Hiragana &&
          char_types[k2_end + 1] == CharType::Kanji) {
        char32_t base2 = godanRenyokeiBaseCp(codepoints[k2_end]);
        if (base2 != 0) {
          std::string embedded2(textRange(text, byte_offsets, k2_start, k2_end));
          embedded2 += normalize::encodeUtf8(base2);
          if (dict_manager.lookupExact(embedded2, core::PartOfSpeech::Verb) != nullptr) {
            adopt_dictionary_v1(k2_end + 1);
            dict_compound_v1_lemma = embedded2;  // best-effort; unused once verified
          }
        }
      }
    }
  }

  CompoundVerbMatch best_match = findCompoundVerbMatch(
      text, codepoints, byte_offsets, start_pos, char_types, kanji_end, v2_start, base_ending, is_sokuonbin, is_ichidan,
      has_kanji_v2_after_bare_ichidan, dict_compound_v1, dict_compound_v1_lemma, dict_manager, inflection);

  // A kanji V2 can itself end in a Godan continuative (見+回し).  The
  // full-kanji path correctly treats that し as a boundary for ordinary
  // words, but it obscures a preceding bare single-kanji ichidan V1. Try
  // that boundary only after the ordinary analysis has no V2 match and only
  // for the closed set of known single-kanji ichidan verbs.
  if (best_match.v2_verb == nullptr && !dict_compound_v1 && kanji_end >= start_pos + 2 &&
      char_types[start_pos + 1] == CharType::Kanji &&
      (verb_helpers::isSingleKanjiIchidan(codepoints[start_pos]) || has_bound_verb_prefix)) {
    CompoundVerbMatch bare_ichidan_match =
        findCompoundVerbMatch(text, codepoints, byte_offsets, start_pos, char_types, kanji_end, start_pos + 1, 0, false,
                              true, true, false, "", dict_manager, inflection);
    if (bare_ichidan_match.v2_verb != nullptr) {
      v2_start = start_pos + 1;
      best_match = std::move(bare_ichidan_match);
    }
  }

  // The first kana after a kanji can itself be an i-row ichidan stem (降り
  // + 合う), while a longer span can be a Godan continuative (混じり + 合う).
  // Keep the established ichidan boundary when it produces a match. Only if
  // it does not, test the next one or two hiragana positions as a complete
  // Godan V1 and require inflectional evidence before moving the V2 boundary.
  if (best_match.v2_verb == nullptr && is_ichidan && !dict_compound_v1 && !has_kanji_v2_after_bare_ichidan &&
      v2_start == kanji_end + 1) {
    const size_t alternate_limit = std::min(v2_start + 2, codepoints.size());
    for (size_t alternate_v2_start = v2_start + 1; alternate_v2_start <= alternate_limit; ++alternate_v2_start) {
      if (char_types[alternate_v2_start - 1] != CharType::Hiragana) {
        break;
      }

      const std::string alternate_v1 = extractSubstring(codepoints, start_pos, alternate_v2_start);
      const auto inflection_candidate = inflection.getBest(alternate_v1);
      const auto* godan_row = grammar::Conjugation::getGodanRow(inflection_candidate.verb_type);
      if (godan_row == nullptr ||
          inflection_candidate.confidence < candidate::verb_cost::kConstructedVerbMinConfidence ||
          codepoints[alternate_v2_start - 1] != godan_row->i_row) {
        continue;
      }

      CompoundVerbMatch alternate_match =
          findCompoundVerbMatch(text, codepoints, byte_offsets, start_pos, char_types, kanji_end, alternate_v2_start,
                                base_ending, is_sokuonbin, is_ichidan, has_kanji_v2_after_bare_ichidan,
                                dict_compound_v1, dict_compound_v1_lemma, dict_manager, inflection);
      if (alternate_match.v2_verb != nullptr) {
        v2_start = alternate_v2_start;
        best_match = std::move(alternate_match);
        break;
      }
    }
  }

  emitCompoundVerbCandidates(lattice, text, codepoints, byte_offsets, start_pos, v2_start, best_match, dict_manager,
                             scorer);
}

}  // namespace suzume::analysis
