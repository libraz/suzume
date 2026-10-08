/**
 * @file tokenizer_unknown.cpp
 * @brief Unknown-word candidate generation for the tokenizer
 */

#include <algorithm>
#include <optional>

#include "analysis/category_cost.h"
#include "analysis/dictionary_probe.h"
#include "analysis/tokenizer.h"
#include "candidate_constants.h"
#include "core/debug.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/honorific_verbs.h"
#include "normalize/utf8.h"
#include "tokenizer_unknown_internal.h"
#include "tokenizer_utils.h"
#include "verb_candidates_dictionary_probes.h"

namespace suzume::analysis {

using tokenizer_unknown_detail::hasCompleteInternalConstituentBoundary;

namespace {

// True if every char position in [start, end) has CharType `type`. When
// `allow_choon` is set, the prolonged sound mark (ー) is also accepted as part
// of the run (colloquial すごーい, katakana loanwords). Bounds-checked against
// both char_types and codepoints so callers can pass raw candidate ranges.
bool allCharsAre(const std::vector<normalize::CharType>& char_types, const std::vector<char32_t>& codepoints,
                 size_t start, size_t end, normalize::CharType type, bool allow_choon) {
  for (size_t idx = start; idx < end && idx < char_types.size(); ++idx) {
    if (char_types[idx] == type) {
      continue;
    }
    if (allow_choon && idx < codepoints.size() && normalize::isProlongedSoundMark(codepoints[idx])) {
      continue;
    }
    return false;
  }
  return true;
}

// A copular irrealis form followed by the volitional auxiliary is a
// grammatical auxiliary sequence, not an unknown lexical verb.  Keeping the
// sequence visible prevents a short pure-hiragana verb candidate from hiding
// a dictionary-backed copula + volitional analysis.
bool isCopulaVolitionalSequence(const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                const ByteOffsets& byte_offsets, size_t start, size_t end) {
  constexpr size_t kMinimumMorphemeCount = 2;
  if (end - start < kMinimumMorphemeCount) {
    return false;
  }

  const std::string_view prefix = textRange(text, byte_offsets, start, end - 1);
  const std::string_view suffix = textRange(text, byte_offsets, end - 1, end);
  const auto* copula = dict_manager.lookupExact(prefix, core::PartOfSpeech::Auxiliary);
  const auto* volitional = dict_manager.lookupExact(suffix, core::PartOfSpeech::Auxiliary);
  return copula != nullptr && copula->extended_pos == core::ExtendedPOS::AuxCopulaDa && volitional != nullptr &&
         volitional->extended_pos == core::ExtendedPOS::AuxVolitional;
}

using suzume::analysis::verb_helpers::isProductiveShiiAdjectiveTerminal;

// The closed adverbial sequence Noun + ながら + に exposes a morpheme
// boundary inside an otherwise plausible unknown verb continuative
// (涙ながらに, not a deverbal noun 涙ながら + に). Require all three pieces:
// a noun spanning the candidate's left side, the exact dictionary conjunctive
// particle, and the following case particle. Before の the same sequence is an
// adnominal search unit (昔ながらの, 生まれながらの) and stays whole, which is
// why the following particle is part of the test rather than an afterthought.
bool hasNounNagaraNiBoundary(const core::Lattice& lattice, const dictionary::DictionaryManager& dict_manager,
                             std::string_view text, const std::vector<char32_t>& codepoints,
                             const ByteOffsets& byte_offsets, const std::vector<UnknownCandidate>& batch_candidates,
                             const UnknownCandidate& candidate) {
  constexpr size_t kNagaraLength = 3;
  if (candidate.end < candidate.start + kNagaraLength + 1 || candidate.end >= codepoints.size() ||
      codepoints[candidate.end] != U'に') {
    return false;
  }

  const size_t nagara_start = candidate.end - kNagaraLength;
  if (codepoints[nagara_start] != U'な' || codepoints[nagara_start + 1] != U'が' ||
      codepoints[nagara_start + 2] != U'ら') {
    return false;
  }

  const auto* nagara = dict_manager.lookupExact(textRange(text, byte_offsets, nagara_start, candidate.end),
                                                core::PartOfSpeech::Particle);
  const auto* case_particle = dict_manager.lookupExact(textRange(text, byte_offsets, candidate.end, candidate.end + 1),
                                                       core::PartOfSpeech::Particle);
  if (nagara == nullptr || nagara->extended_pos != core::ExtendedPOS::ParticleConj || case_particle == nullptr ||
      case_particle->extended_pos != core::ExtendedPOS::ParticleCase) {
    return false;
  }

  if (core::anyEdgeStartingAt(lattice, candidate.start, [nagara_start](const core::LatticeEdge& edge) {
        return edge.end == nagara_start && edge.pos == core::PartOfSpeech::Noun;
      })) {
    return true;
  }
  return std::any_of(batch_candidates.begin(), batch_candidates.end(), [&](const UnknownCandidate& alternative) {
    return alternative.start == candidate.start && alternative.end == nagara_start &&
           alternative.pos == core::PartOfSpeech::Noun;
  });
}

/**
 * @brief Whether a span is a numeral plus a counter written with okurigana
 *
 * A deverbal counter carries its okurigana into the quantity phrase (一切れ,
 * 三重ね), so the span always extends past the numeral+kanji prefix that a
 * dictionary entry happens to cover (the adverb 一切, the noun 三重).  The
 * registered continuative behind the counter is what licenses the extra kana, so
 * the coincidental prefix must not price the phrase out.
 */
bool isNumeralOkuriganaCounterPhrase(const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                     const std::vector<size_t>& byte_offsets, const std::vector<char32_t>& codepoints,
                                     const std::vector<normalize::CharType>& char_types, size_t start_pos,
                                     size_t end_pos) {
  if (end_pos < start_pos + 3 || end_pos > codepoints.size()) {
    return false;
  }
  const size_t counter_pos = end_pos - 2;
  if (char_types[counter_pos] != normalize::CharType::Kanji ||
      char_types[end_pos - 1] != normalize::CharType::Hiragana) {
    return false;
  }
  for (size_t pos = start_pos; pos < counter_pos; ++pos) {
    if (!normalize::isNumeralCodepoint(codepoints[pos])) {
      return false;
    }
  }
  return hasExactPartOfSpeech(dict_manager, textRange(text, byte_offsets, counter_pos, end_pos),
                              partOfSpeechMask(core::PartOfSpeech::Verb));
}

// Whether a surface is 第 followed only by numerals (第一, 第二十).
bool isOrdinalNounSurface(const std::string& surface) {
  const auto prefix_codepoints = normalize::toCodepoints(surface);
  return prefix_codepoints.size() >= 2 && prefix_codepoints.front() == U'第' &&
         std::all_of(prefix_codepoints.begin() + 1, prefix_codepoints.end(), normalize::isNumeralCodepoint);
}

// Price an accepted candidate against the dictionary coverage at its start.
// Returns nullopt when the span absorbs a suru imperative and is no candidate.
std::optional<float> unknownCandidateCost(const tokenizer_unknown_detail::UnknownCandidateContext& ctx,
                                          const UnknownCandidate& candidate) {
  const auto& dict_manager = ctx.dict_manager;
  const std::string_view text = ctx.text;
  const auto& codepoints = ctx.codepoints;
  const auto& byte_offsets = ctx.byte_offsets;
  const size_t start_pos = ctx.start_pos;
  const auto& char_types = ctx.char_types;
  const auto& dict_results = ctx.dict_results;
  const size_t max_dict_length = ctx.max_dict_length;
  float adjusted_cost = candidate.cost;

  // Penalize unknown words that extend beyond dictionary entries
  bool skip_penalty = false;
  [[maybe_unused]] const char* skip_reason = nullptr;

  // Skip penalty for adverbs (onomatopoeia like わくわく)
  if (candidate.pos == core::PartOfSpeech::Adverb) {
    skip_penalty = true;
    skip_reason = "adverb";
  }

  if (!skip_penalty && candidate.pos == core::PartOfSpeech::Noun &&
      isNumeralOkuriganaCounterPhrase(dict_manager, text, byte_offsets, codepoints, char_types, candidate.start,
                                      candidate.end)) {
    skip_penalty = true;
    skip_reason = "numeral_okurigana_counter";
  }

  // A quantity phrase is licensed by its own numeral and counter structure, so
  // a shorter registered word that spells its head (十分 in 十分の三) does not
  // make it a fabricated span.
  if (!skip_penalty && candidate.origin == CandidateOrigin::Counter && candidate.pos == core::PartOfSpeech::Noun) {
    skip_penalty = true;
    skip_reason = "counter_quantity";
  }

  if (!skip_penalty && (candidate.pos == core::PartOfSpeech::Verb || candidate.pos == core::PartOfSpeech::Adjective)) {
    // Exception: Don't skip verb candidates ending with ず (adverbialized negatives)
    // e.g., 思わず, 絶えず - these are lexicalized adverbs from verb + ず
    const bool ends_with_zu = utf8::endsWith(candidate.surface, "ず");
    // An explicitly generated irrealis stem is already licensed by a
    // following closed-class inflection (negative, causative, or
    // passive).  A shorter dictionary verb/adjective must not suppress
    // that productive boundary: 確かめ+させる is not 確か+めさせる.
    const bool is_explicit_mizenkei =
        candidate.origin == CandidateOrigin::VerbKanji && candidate.extended_pos == core::ExtendedPOS::VerbMizenkei;
    for (const auto& result : dict_results) {
      if (result.entry != nullptr) {
        // Case 1: Dictionary entry is also a verb/adjective
        // But allow ず-ending candidates (adverbialized forms)
        if ((result.entry->pos == core::PartOfSpeech::Verb || result.entry->pos == core::PartOfSpeech::Adjective) &&
            !ends_with_zu && !candidate.lemma_verified && !is_explicit_mizenkei) {
          skip_penalty = true;
          skip_reason = "dict_has_verb_adj";
          break;
        }
        // Case 2: Pure hiragana verb candidate vs short dictionary entry
        // Also allow prolonged sound mark (ー) as part of hiragana sequence
        // for colloquial patterns like すごーい, やばーい, かわいー
        if (result.length <= 2 && candidate.end - candidate.start >= 3) {
          if (allCharsAre(char_types, codepoints, candidate.start, candidate.end, normalize::CharType::Hiragana,
                          /*allow_choon=*/true) &&
              !isCopulaVolitionalSequence(dict_manager, text, byte_offsets, candidate.start, candidate.end)) {
            skip_penalty = true;
            skip_reason = "pure_hiragana_verb";
            break;
          }
        }
      }
    }
  }

  // Case 3: Colloquial verb contraction (ておく→っとく)
  // っとく is a valid compound verb ending that shouldn't be penalized for length
  // Note: っちゃう/っじゃう are handled by Case 6 (revoke skip for ちゃう endings)
  if (!skip_penalty && candidate.pos == core::PartOfSpeech::Verb && utf8::endsWith(candidate.surface, "っとく")) {
    skip_penalty = true;
    skip_reason = "colloquial_contraction";
  }

  // Case 5: Short hiragana verb candidates ending with te/de-form
  // Handles cases like ねて (寝る), でて (出る), みて (見る) where
  // dictionary only has kanji form but surface is pure hiragana.
  // These 2-char patterns don't meet Case 2's ≥3 char threshold.
  if (!skip_penalty && candidate.pos == core::PartOfSpeech::Verb && candidate.end - candidate.start == 2 &&
      candidate.surface.size() >= core::kJapaneseCharBytes &&
      allCharsAre(char_types, codepoints, candidate.start, candidate.end, normalize::CharType::Hiragana,
                  /*allow_choon=*/false) &&
      grammar::isTeDeSurface(utf8::lastChar(candidate.surface))) {
    skip_penalty = true;
    skip_reason = "short_te_form";
  }

  // Case 6: Revoke skip for long hiragana verbs ending with ちゃう/ちゃっ/ちゃい
  // These are auxiliary chains (e.g., されちゃう = さ+れ+ちゃう,
  // なっちゃう = なっ+ちゃう, やっちゃう = やっ+ちゃう) that should split.
  if (skip_penalty && candidate.pos == core::PartOfSpeech::Verb && candidate.end - candidate.start >= 4) {
    if (utf8::endsWithAny(candidate.surface, {"ちゃう", "ちゃっ", "ちゃい"}) &&
        allCharsAre(char_types, codepoints, candidate.start, candidate.end, normalize::CharType::Hiragana,
                    /*allow_choon=*/false)) {
      skip_penalty = false;
      skip_reason = nullptr;
    }
  }

  // Case 4: Pure hiragana OTHER (likely readings/furigana)
  // Reduce penalty for long varied hiragana sequences
  // Also allow prolonged sound mark (ー) as part of hiragana sequence
  bool reduced_penalty = false;
  bool skip_dict_penalty = false;
  [[maybe_unused]] const char* skip_dict_reason = nullptr;
  if (candidate.origin == CandidateOrigin::SuffixPattern) {
    skip_dict_penalty = true;
    skip_dict_reason = "verified_suffix_construction";
  } else if (candidate.origin == CandidateOrigin::KanjiHiraganaNominalCompound) {
    skip_dict_penalty = true;
    skip_dict_reason = "nominal_context_compound";
  }
  // Three morae is the ordinary length of a native hiragana noun (ねずみ,
  // たまご, さくら), so the run is not evidence of a fabricated span the way a
  // shorter one would be.
  constexpr size_t kMinVariedHiraganaRun = 3;
  if (!skip_penalty && candidate.pos == core::PartOfSpeech::Other &&
      candidate.end - candidate.start >= kMinVariedHiraganaRun) {
    if (allCharsAre(char_types, codepoints, candidate.start, candidate.end, normalize::CharType::Hiragana,
                    /*allow_choon=*/true)) {
      // Reduce penalty only for varied sequences, not runs of one repeated
      // char (ーーーー, ああああ) which are usually noise.
      for (size_t idx = candidate.start + 1; idx < candidate.end && idx < codepoints.size(); ++idx) {
        if (codepoints[idx] != codepoints[candidate.start]) {
          reduced_penalty = true;
          break;
        }
      }
    }
  }

  // Skip dict length penalty for katakana sequences (loanwords)
  // Loanwords like マスカラ, デスクトップ often exceed dictionary coverage
  if (!skip_penalty && candidate.pos == core::PartOfSpeech::Noun && candidate.end - candidate.start >= 3) {
    if (allCharsAre(char_types, codepoints, candidate.start, candidate.end, normalize::CharType::Katakana,
                    /*allow_choon=*/true)) {
      skip_dict_penalty = true;
      skip_dict_reason = "all_katakana";
    }
  }

  // Skip dict length penalty for kanji compound sequences (2-6 chars)
  // Common compounds like 人工知能, 自然言語処理 may not be in dictionary
  // Keep compounds connected - splitting should be driven by PREFIX/SUFFIX
  // markers or dictionary entries, not length heuristics
  if (!skip_penalty && !skip_dict_penalty && candidate.pos == core::PartOfSpeech::Noun) {
    size_t len = candidate.end - candidate.start;
    if (len >= 2 && len <= 6) {
      if (allCharsAre(char_types, codepoints, candidate.start, candidate.end, normalize::CharType::Kanji,
                      /*allow_choon=*/false)) {
        skip_dict_penalty = true;
        skip_dict_reason = "all_kanji_compound";

        // When a dictionary entry exists as a proper prefix of this compound,
        // add a moderate penalty to prefer the dict-split path.
        // E.g., 第一(dict) + 毛 should beat 第一毛(compound)
        // Only when the prefix covers a significant portion (>= half)
        // to avoid splitting 自然言語処理 at 自然(2/6).
        for (const auto& result : dict_results) {
          // A na-adjective stem heads compounds as freely as a noun does
          // (健康管理, 安全対策, 重要性), so only heads that form none — adverbs,
          // determiners, ordinals — mark the prefix as a separate word.
          if (result.entry != nullptr && result.length >= 2 && result.length < len && result.length * 2 >= len &&
              ((result.entry->pos != core::PartOfSpeech::Noun &&
                result.entry->extended_pos != core::ExtendedPOS::AdjNaAdj) ||
               isOrdinalNounSurface(result.entry->surface))) {
            constexpr float kDictPrefixPenalty = 1.5F;
            adjusted_cost += kDictPrefixPenalty;
            SUZUME_DEBUG_LOG_VERBOSE("[TOK_UNK] \"" << candidate.surface << "\" (NOUN): +" << kDictPrefixPenalty
                                                    << " (kanji_compound_dict_prefix, dict=\"" << result.entry->surface
                                                    << "\")\n");
            break;
          }
        }

        // When a non-NOUN dict entry from a prior position overlaps with
        // this compound's first character, penalize the compound.
        // E.g., その後(dict ADV, pos=0, len=3) overlaps with 後猫(pos=2)
        // → penalize 後猫 to prefer その後+猫 split.
        constexpr size_t kMaxLookback = 4;
        bool found_overlap = false;
        for (size_t back = 1; back <= kMaxLookback && back <= start_pos && !found_overlap; ++back) {
          size_t prev_pos = start_pos - back;
          size_t prev_byte = byteOffsetAt(byte_offsets, prev_pos);
          auto prev_results = dict_manager.lookup(text, prev_byte);
          for (const auto& result : prev_results) {
            if (result.entry != nullptr && result.length >= 2 && result.length > back &&
                result.entry->pos != core::PartOfSpeech::Noun && result.entry->pos != core::PartOfSpeech::Pronoun) {
              constexpr float kDictOverlapPenalty = 1.5F;
              adjusted_cost += kDictOverlapPenalty;
              SUZUME_DEBUG_LOG_VERBOSE("[TOK_UNK] \"" << candidate.surface << "\" (NOUN): +" << kDictOverlapPenalty
                                                      << " (kanji_compound_dict_overlap, dict=\""
                                                      << result.entry->surface << "\")\n");
              found_overlap = true;
              break;
            }
          }
        }
      }
    }
  }

  // Skip exceeds_dict_length penalty for suffix pattern candidates
  // These are morphologically recognized patterns (e.g., がち, っぽい)
  // that should not be penalized for exceeding dictionary coverage
  // Also skip for katakana loanwords (マスカラ, デスクトップ)
  // Also skip for Suru verb candidates (所在する, 延期する) - these are productive
  bool is_suru_verb =
      (candidate.pos == core::PartOfSpeech::Verb && candidate.conj_type == dictionary::ConjugationType::Suru);

  // Check for pure hiragana verb (e.g., ねる, もらう, あげる)
  // These should not be penalized heavily - they are legitimate verb forms
  bool is_pure_hiragana_verb = false;
  if (candidate.pos == core::PartOfSpeech::Verb && candidate.end - candidate.start >= 2) {
    const size_t candidate_length = candidate.end - candidate.start;
    const bool is_short_form = candidate_length <= 4;
    const bool has_left_predicate_boundary =
        candidate.start == 0 || char_types[candidate.start - 1] == normalize::CharType::Symbol ||
        dict_manager.lookupExact(textRange(text, byte_offsets, candidate.start - 1, candidate.start),
                                 core::PartOfSpeech::Particle) != nullptr;
    const bool has_right_predicate_boundary =
        candidate.end == codepoints.size() || char_types[candidate.end] == normalize::CharType::Symbol ||
        char_types[candidate.end] == normalize::CharType::Kanji ||
        dict_manager.lookupExact(textRange(text, byte_offsets, candidate.end, candidate.end + 1),
                                 core::PartOfSpeech::Particle) != nullptr;
    const bool is_bounded_terminal_form = candidate_length <= 8 && candidate.surface.compare(candidate.lemma) == 0 &&
                                          has_left_predicate_boundary && has_right_predicate_boundary;
    if ((is_short_form || is_bounded_terminal_form) &&
        allCharsAre(char_types, codepoints, candidate.start, candidate.end, normalize::CharType::Hiragana,
                    /*allow_choon=*/false)) {
      is_pure_hiragana_verb = true;
    }
  }
  if (is_pure_hiragana_verb &&
      isCopulaVolitionalSequence(dict_manager, text, byte_offsets, candidate.start, candidate.end)) {
    is_pure_hiragana_verb = false;
  }

  // Check for single-kanji stem + hiragana verb (e.g., 残って, 通る, 飛ぶ)
  // Single-kanji verb stems are common in Japanese (残る, 立つ, 打つ, etc.)
  // These should not be penalized for exceeding dict length
  const bool is_kanji_stem_verb =
      candidate.pos == core::PartOfSpeech::Verb && candidate.end - candidate.start >= 2 &&
      candidate.start < char_types.size() && char_types[candidate.start] == normalize::CharType::Kanji &&
      allCharsAre(char_types, codepoints, candidate.start + 1, candidate.end, normalize::CharType::Hiragana,
                  /*allow_choon=*/false);

  bool exceeds_dict = (max_dict_length > 0 && candidate.end - candidate.start > max_dict_length);
  bool absorbs_suru_imperative = false;
  if (candidate.pos == core::PartOfSpeech::Verb && candidate.end - candidate.start >= 4) {
    for (size_t split_pos = candidate.start + 2; split_pos < candidate.end; ++split_pos) {
      if (!allCharsAre(char_types, codepoints, candidate.start, split_pos, normalize::CharType::Kanji,
                       /*allow_choon=*/false)) {
        continue;
      }
      if (grammar::isSuruImperativeSurface(textRange(text, byte_offsets, split_pos, candidate.end))) {
        absorbs_suru_imperative = true;
        break;
      }
    }
  }
  if (absorbs_suru_imperative) {
    return std::nullopt;
  }
  if (exceeds_dict) {
    const bool skips_length_penalty = skip_penalty || skip_dict_penalty || is_suru_verb || candidate.has_suffix ||
                                      is_pure_hiragana_verb || is_kanji_stem_verb;
    if (skips_length_penalty) {
      SUZUME_DEBUG_LOG_VERBOSE("[TOK_SKIP] \"" << candidate.surface << "\" (" << core::posToString(candidate.pos)
                                               << "): " << "skip exceeds_dict_length ("
                                               << (skip_penalty            ? skip_reason
                                                   : skip_dict_penalty     ? skip_dict_reason
                                                   : is_suru_verb          ? "suru_verb"
                                                   : candidate.has_suffix  ? "has_suffix"
                                                   : is_pure_hiragana_verb ? "pure_hiragana_verb"
                                                                           : "kanji_stem_verb")
                                               << ")\n");
    } else {
      float penalty = reduced_penalty ? 1.0F : 3.5F;
      adjusted_cost += penalty;
      SUZUME_DEBUG_LOG_VERBOSE("[TOK_UNK] \"" << candidate.surface << "\" (" << core::posToString(candidate.pos)
                                              << "): +" << penalty << " (exceeds_dict_length"
                                              << (reduced_penalty ? ", pure_hiragana" : "")
                                              << ", dict_max=" << max_dict_length << ")\n");
    }
  }

  if (candidate.pos == core::PartOfSpeech::Other && candidate.end - candidate.start < max_dict_length &&
      allCharsAre(char_types, codepoints, candidate.start, candidate.end, normalize::CharType::Hiragana,
                  /*allow_choon=*/false)) {
    adjusted_cost += candidate::kTruncatedDictionaryWordPenalty;
    SUZUME_DEBUG_LOG_VERBOSE("[TOK_UNK] \"" << candidate.surface << "\" (OTHER): +"
                                            << candidate::kTruncatedDictionaryWordPenalty
                                            << " (truncates_dictionary_word, dict_max=" << max_dict_length << ")\n");
  }

  // For verb candidates, check if the hiragana suffix is a known particle
  if (candidate.pos == core::PartOfSpeech::Verb && candidate.end > candidate.start) {
    size_t hiragana_start = candidate.start;
    while (hiragana_start < candidate.end && hiragana_start < char_types.size() &&
           char_types[hiragana_start] != normalize::CharType::Hiragana) {
      ++hiragana_start;
    }

    if (hiragana_start < candidate.end) {
      const std::string_view hiragana_suffix = textRange(text, byte_offsets, hiragana_start, candidate.end);

      // Don't penalize verb conjugation endings
      // - te-form: て/で/って/んで/いて/いで
      // - renyoukei し: extremely common for suru/godan verbs (分割し, 話し)
      bool is_verb_ending = utf8::equalsAny(hiragana_suffix, {"て", "で", "って", "んで", "いて", "いで", "し"}) ||
                            candidate.extended_pos == core::ExtendedPOS::VerbRenyokei;

      // Skip penalty if:
      // - Known verb conjugation ending (te-form, renyoukei)
      // - Candidate has has_suffix flag (mizenkei for ぬ/れべき patterns)
      if (!is_verb_ending && !candidate.has_suffix) {
        if (dict_manager.lookupExact(hiragana_suffix, core::PartOfSpeech::Particle) != nullptr) {
          adjusted_cost += 1.5F;
          SUZUME_DEBUG_LOG_VERBOSE("[TOK_UNK] \"" << candidate.surface << "\": +1.5 (particle_suffix=\""
                                                  << hiragana_suffix << "\")\n");
        }
      }
    }
  }

  return adjusted_cost;
}

// Add the candidate's own edge and its deverbal-noun re-reading.
void addUnknownCandidateEdges(core::Lattice& lattice, const tokenizer_unknown_detail::UnknownCandidateContext& ctx,
                              const UnknownCandidate& candidate, float adjusted_cost) {
  const auto& dict_manager = ctx.dict_manager;
  const auto& inflection = ctx.inflection;
  const std::string_view text = ctx.text;
  const auto& codepoints = ctx.codepoints;
  const auto& byte_offsets = ctx.byte_offsets;
  const auto& candidates = ctx.candidates;
  uint8_t flags = core::LatticeEdge::kIsUnknown;

  std::string surface_str(candidate.surface);

  // Relay dict-verified-lemma marking so the scorer can exempt genuine verb
  // onbin forms from the spurious-onbin penalty.
  if (candidate.lemma_verified) {
    flags |= static_cast<uint8_t>(core::EdgeFlags::LemmaVerified);
  }

  // A continuative predicate immediately selected by a nominal particle is
  // productively usable as a deverbal noun (隔たり+を, 読み+が). Preserve
  // that POS alternative without registering each open-class nominalization.
  if (candidate.pos == core::PartOfSpeech::Verb && candidate.extended_pos == core::ExtendedPOS::VerbRenyokei &&
      candidate.end < codepoints.size()) {
    const bool nominal_particle = hasNominalForcingParticleContinuation(codepoints, candidate.end, &dict_manager);
    // A particle homograph must not hide a longer dependent predicate or
    // closed derivational suffix beginning at the same boundary
    // (理解+し+がたい, 読み+やすい, 遅刻+し+がち).
    // Longest closed-class evidence takes priority over the nominalized
    // renyokei alternative; a standalone が/を still licenses it.
    bool longer_dependent_follows = false;
    const size_t probe_end = std::min(codepoints.size(), candidate.end + static_cast<size_t>(4));
    const std::string_view following_probe = textRange(text, byte_offsets, candidate.end, probe_end);
    for (const auto& result : dict_manager.lookup(following_probe, 0)) {
      if (result.entry != nullptr && result.length > 1 &&
          (result.entry->pos == core::PartOfSpeech::Adjective || result.entry->pos == core::PartOfSpeech::Auxiliary ||
           result.entry->extended_pos == core::ExtendedPOS::SuffixTendency)) {
        longer_dependent_follows = true;
        break;
      }
    }
    const auto same_surface_entries = dict_manager.lookup(surface_str, 0);
    const bool has_lexical_nonverb_reading =
        std::any_of(same_surface_entries.begin(), same_surface_entries.end(), [&](const auto& match) {
          return match.entry != nullptr && match.length == candidate.end - candidate.start &&
                 match.entry->pos != core::PartOfSpeech::Verb;
        });
    const bool is_complete_shii_adjective = isProductiveShiiAdjectiveTerminal(surface_str, inflection);
    // The one-mora と is case-particle-shaped in the dictionary, but after a
    // complete predicate it marks quotation. In that position even a
    // productive adjective whose lemma equals its terminal surface is enough
    // to reject a fabricated deverbal-noun homograph. Other nominal particles
    // retain the ambiguity needed by おもい+が/を.
    const bool follows_predicate_quote =
        nominal_particle && candidate.end < codepoints.size() && codepoints[candidate.end] == core::hiragana::kTo;
    const auto* following_particle = candidate.end < codepoints.size()
                                         ? lookupEntryInRange(dict_manager, codepoints, candidate.end,
                                                              candidate.end + 1, core::PartOfSpeech::Particle)
                                         : nullptr;
    const bool follows_nominalizer =
        following_particle != nullptr && following_particle->extended_pos == core::ExtendedPOS::ParticleNo;
    // A finished adjective reading of the same span is a complete inflectional
    // analysis, so a deverbal re-reading built on a fabricated verb must stand
    // down: 高かれ+と is the カリ 命令形 of 高い, not a noun from 高かれる. The
    // lemma normally has to differ from the surface, unless the following
    // quotation particle supplies the predicate-position evidence.
    const bool same_span_adjective_analysis =
        std::any_of(candidates.begin(), candidates.end(), [&](const UnknownCandidate& other) {
          return other.pos == core::PartOfSpeech::Adjective && other.extended_pos == core::ExtendedPOS::AdjBasic &&
                 other.start == candidate.start && other.end == candidate.end &&
                 (other.lemma != other.surface || follows_predicate_quote || follows_nominalizer);
        });
    const bool crosses_complete_internal_boundary =
        hasCompleteInternalConstituentBoundary(lattice, dict_manager, text, byte_offsets, candidates, candidate);
    const bool crosses_noun_nagara_ni_boundary =
        hasNounNagaraNiBoundary(lattice, dict_manager, text, codepoints, byte_offsets, candidates, candidate);
    // The nominalization is a POS re-reading of an accepted continuative, not
    // independent evidence for the span. Once the verb path prices the span
    // worse than an outright unknown word (秋来ぬ, charged as the fabricated
    // サ変 秋来する across a clause boundary), there is no continuative left to
    // re-read, so the bonus must not resurrect it. Ordinary deverbal nouns
    // (身なり, 足取り) stay well inside the unknown-word band.
    const bool verb_reading_rejected = adjusted_cost > getCategoryCost(core::ExtendedPOS::Unknown);
    const bool bound_suffix_after_host =
        verb_helpers::isBoundSuffixAfterNominalHost(&dict_manager, codepoints, candidate.start, candidate.surface);
    // A particle is a closed boundary, never the first mora of a deverbal
    // nominalization.  This prevents an unverified kana verb hypothesis
    // from absorbing the genitive in productive sequences such as の+わり
    // and の+うち.
    const bool starts_with_closed_particle =
        lookupEntryInRange(dict_manager, codepoints, candidate.start, candidate.start + 1,
                           core::PartOfSpeech::Particle) != nullptr;
    // A deverbal noun re-reads the continuative cell on its own. An analysis
    // that had to match an auxiliary chain to reach the lemma describes a
    // complete predicate instead (ためさ + ない), so nominalizing that span
    // would bury the auxiliary — here the negation — inside the noun. The
    // productive nominalizations reach their lemma from the bare cell and
    // carry no chain (読み, 身なり, 隔たり).
    // A quotation closes a clause, so a span in front of it is read as a
    // predicate wherever it can be one. When the continuative's own tail
    // spells a complete multi-mora auxiliary and its lemma is a fabrication,
    // that auxiliary is the predicate: 金なり+と is 金 + なり (the classical
    // copula) under quotation, not a deverbal noun from the non-word 金なる.
    // The other nominal particles select an argument instead of closing a
    // clause, so they keep the ambiguity that 身なり+を needs.
    bool auxiliary_tail_before_quote = false;
    if (follows_predicate_quote && !verb_helpers::isVerbInDictionary(&dict_manager, candidate.lemma)) {
      // A one-mora tail is also the last mora of ordinary words, so only
      // multi-mora auxiliaries count; none of the closed class is longer than
      // four morae.
      constexpr size_t kMaxAuxiliaryLen = 4;
      const size_t max_len = std::min(kMaxAuxiliaryLen, candidate.end - candidate.start - 1);
      for (size_t tail_len = 2; tail_len <= max_len; ++tail_len) {
        if (lookupEntryInRange(dict_manager, codepoints, candidate.end - tail_len, candidate.end,
                               core::PartOfSpeech::Auxiliary) != nullptr) {
          auxiliary_tail_before_quote = true;
          break;
        }
      }
    }
    const auto& span_analyses = inflection.analyze(surface_str);
    const bool carries_auxiliary_chain = std::any_of(
        span_analyses.begin(), span_analyses.end(),
        [&](const auto& analysis) { return analysis.base_form == candidate.lemma && !analysis.morphemes.empty(); });
    if (nominal_particle && !longer_dependent_follows && !has_lexical_nonverb_reading && !is_complete_shii_adjective &&
        !same_span_adjective_analysis && !crosses_complete_internal_boundary && !crosses_noun_nagara_ni_boundary &&
        !verb_reading_rejected && !bound_suffix_after_host && !candidate.has_suffix && !starts_with_closed_particle &&
        !carries_auxiliary_chain && !auxiliary_tail_before_quote) {
      lattice.addEdge(surface_str, static_cast<uint32_t>(candidate.start), static_cast<uint32_t>(candidate.end),
                      core::PartOfSpeech::Noun,
                      getCategoryCost(core::ExtendedPOS::NounVerbal) + candidate::kNominalizedNounParticleBonus, flags,
                      surface_str, dictionary::ConjugationType::None, candidate.origin, candidate::kNoOriginConfidence,
                      "nominalized_renyokei_before_particle", core::ExtendedPOS::NounVerbal,
                      "nominalized_renyokei_before_particle");
    }
  }

  lattice.addEdge(surface_str, static_cast<uint32_t>(candidate.start), static_cast<uint32_t>(candidate.end),
                  candidate.pos, adjusted_cost, flags, candidate.lemma, candidate.conj_type, candidate.origin,
#ifdef SUZUME_DEBUG_INFO
                  candidate.confidence, candidate.pattern, candidate.extended_pos, candidate.epos_source);
#else
                  0.0F, {}, candidate.extended_pos);
#endif
}

}  // namespace

void Tokenizer::addUnknownCandidates(core::Lattice& lattice, std::string_view text,
                                     const std::vector<char32_t>& codepoints, const ByteOffsets& byte_offsets,
                                     size_t start_pos, const std::vector<normalize::CharType>& char_types,
                                     const std::vector<dictionary::LookupResult>& dict_results) const {
  // A pure-hiragana sequence enclosed by brackets is a parenthetical reading
  // (東京（とうきょう）). It is annotation text, so retain it as one searchable
  // content token instead of a sequence of incidental particles and auxiliaries.
  if (start_pos > 0 && normalize::isOpeningBracket(codepoints[start_pos - 1])) {
    const size_t reading_end = findCharRegionEnd(char_types, start_pos, candidate::kParentheticalReadingMaxLength,
                                                 normalize::CharType::Hiragana);
    if (reading_end > start_pos && reading_end < codepoints.size() &&
        normalize::isClosingBracket(codepoints[reading_end])) {
      lattice.addEdge(textRange(text, byte_offsets, start_pos, reading_end), static_cast<uint32_t>(start_pos),
                      static_cast<uint32_t>(reading_end), core::PartOfSpeech::Noun,
                      candidate::kParentheticalReadingCandidateCost, core::LatticeEdge::kIsUnknown, {},
                      dictionary::ConjugationType::None, core::CandidateOrigin::Unknown, candidate::kNoOriginConfidence,
                      {}, core::ExtendedPOS::Noun, "parenthetical_reading");
    }
  }

  // Check for dictionary entries at this position to penalize longer unknown words
  size_t max_dict_length = 0;
  for (const auto& result : dict_results) {
    // Closed classes mark grammatical boundaries but are not lexical evidence
    // against a longer unknown content word beginning at the same character.
    // In particular, a one-kanji suffix such as 内 must not make an entire
    // Sino compound pay the dictionary-length penalty while arbitrary shorter
    // fragments stay cheap.
    if (result.entry != nullptr && core::isContentWord(result.entry->pos) &&
        result.entry->extended_pos != core::ExtendedPOS::NounFormal) {
      max_dict_length = std::max(max_dict_length, result.length);
    }
  }

  // Generate unknown word candidates
  auto candidates = unknown_gen_.generate(text, codepoints, start_pos, char_types);
  const size_t kanji_end =
      start_pos < char_types.size() && char_types[start_pos] == normalize::CharType::Kanji
          ? findCharRegionEnd(char_types, start_pos, char_types.size() - start_pos, normalize::CharType::Kanji)
          : start_pos;
  const size_t following_verb_start = kanji_end - start_pos >= 3
                                          ? longestNominalVerbContinuativeStart(codepoints, char_types, start_pos,
                                                                                kanji_end, inflection_, &dict_manager_)
                                          : kanji_end;

  const tokenizer_unknown_detail::UnknownCandidateContext ctx{
      dict_manager_, inflection_,  unknown_gen_, text,      codepoints,           byte_offsets,   start_pos,
      char_types,    dict_results, candidates,   kanji_end, following_verb_start, max_dict_length};

  for (const auto& candidate : candidates) {
    if (tokenizer_unknown_detail::isRejectedUnknownCandidate(ctx, lattice, candidate)) {
      continue;
    }
    const std::optional<float> adjusted_cost = unknownCandidateCost(ctx, candidate);
    if (!adjusted_cost) {
      continue;
    }
    addUnknownCandidateEdges(lattice, ctx, candidate, *adjusted_cost);
  }
}

}  // namespace suzume::analysis