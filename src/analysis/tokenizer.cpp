/**
 * @file tokenizer.cpp
 * @brief Tokenizer that builds lattice from text
 *
 * This file orchestrates candidate generation for tokenization:
 * - Dictionary candidates (direct lookup)
 * - Unknown word candidates (delegated to UnknownWordGenerator)
 * - Split candidates (delegated to split_candidates.h)
 * - Join candidates (delegated to join_candidates.h)
 */

#include "analysis/tokenizer.h"

#include "analysis/category_cost.h"
#include "core/debug.h"
#include "core/utf8_constants.h"
#include "join_candidates.h"
#include "normalize/utf8.h"
#include "split_candidates.h"
#include "tokenizer_utils.h"
#include "verb_candidates_helpers.h"

namespace suzume::analysis {

using verb_helpers::getHiraganaVowel;

Tokenizer::Tokenizer(const dictionary::DictionaryManager& dict_manager,
                     const Scorer& scorer,
                     const UnknownWordGenerator& unknown_gen)
    : dict_manager_(dict_manager),
      scorer_(scorer),
      unknown_gen_(unknown_gen),
      inflection_(unknown_gen.inflection()) {}

core::Lattice Tokenizer::buildLattice(
    std::string_view text, const std::vector<char32_t>& codepoints,
    const std::vector<normalize::CharType>& char_types) const {
  core::Lattice lattice(codepoints.size());

  // Process each position
  for (size_t pos = 0; pos < codepoints.size(); ++pos) {
    // These run at every position
    addDictionaryCandidates(lattice, text, codepoints, pos);
    addUnknownCandidates(lattice, text, codepoints, pos, char_types);
    addMixedScriptCandidates(lattice, text, codepoints, pos, char_types);

    // CharType-based dispatch: skip generators that can't match at this position
    auto ct = char_types[pos];
    if (ct == normalize::CharType::Kanji) {
      addCompoundSplitCandidates(lattice, text, codepoints, pos, char_types);
      addNounVerbSplitCandidates(lattice, text, codepoints, pos, char_types);
      addCompoundVerbJoinCandidates(lattice, text, codepoints, pos, char_types);
      addPrefixNounJoinCandidates(lattice, text, codepoints, pos, char_types);
      addTaruAdjectiveJoinCandidates(lattice, text, codepoints, pos, char_types);
      addVerbSuffixNounJoinCandidates(lattice, text, codepoints, pos, char_types);
    } else if (ct == normalize::CharType::Hiragana) {
      addHiraganaCompoundVerbJoinCandidates(lattice, text, codepoints, pos, char_types);
      addTeFormAuxiliaryCandidates(lattice, text, codepoints, pos, char_types);
    } else if (ct == normalize::CharType::Katakana) {
      addKatakanaSugiruJoinCandidates(lattice, text, codepoints, pos, char_types);
    }
    // addAdjectiveSugiruJoinCandidates is a no-op — removed

    SUZUME_DEBUG_LOG("[LATTICE] pos=" << pos << " candidates=" << lattice.edgesAt(pos).size() << "\n");
  }

  // Fallback: ensure every position has at least one edge
  // This prevents the lattice from becoming invalid when no candidates are generated
  // (e.g., positions starting with small kana like っ, ゃ, ゅ, ょ)
  for (size_t pos = 0; pos < codepoints.size(); ++pos) {
    if (lattice.edgesAt(pos).empty()) {
      // Generate a single-character fallback candidate with high penalty
      size_t byte_start = charPosToBytePos(codepoints, pos);
      size_t byte_end = charPosToBytePos(codepoints, pos + 1);
      std::string surface(text.substr(byte_start, byte_end - byte_start));

      // Use OTHER POS with high cost - this should only be chosen as last resort
      constexpr float kFallbackCost = 5.0F;
      lattice.addEdge(surface, static_cast<uint32_t>(pos),
                      static_cast<uint32_t>(pos + 1), core::PartOfSpeech::Other,
                      kFallbackCost, core::LatticeEdge::kIsUnknown);
    }
  }

  return lattice;
}

size_t Tokenizer::charPosToBytePos(const std::vector<char32_t>& codepoints, size_t char_pos) {
  return analysis::charPosToBytePos(codepoints, char_pos);
}

void Tokenizer::addDictionaryCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos) const {
  // Convert to byte position for dictionary lookup
  size_t byte_pos = charPosToBytePos(codepoints, start_pos);

  // Lookup in dictionary
  auto results = dict_manager_.lookup(text, byte_pos);

  for (const auto& result : results) {
    if (result.entry == nullptr) {
      continue;
    }

    // Calculate end position in characters
    size_t end_pos = start_pos + result.length;

    // Create edge
    // v0.8: flags derived from extended_pos, cost from getCategoryCost()
    uint8_t flags = core::LatticeEdge::kFromDictionary;
    if (result.entry->extended_pos == core::ExtendedPOS::NounFormal) {
      flags |= core::LatticeEdge::kIsFormalNoun;
    }
    // Note: is_low_info removed - can be derived from extended_pos if needed

    // Cost is now derived from ExtendedPOS via getCategoryCost()
    float cost = analysis::getCategoryCost(result.entry->extended_pos);

    lattice.addEdge(result.entry->surface,
                    static_cast<uint32_t>(start_pos),
                    static_cast<uint32_t>(end_pos), result.entry->pos,
                    cost, flags, result.entry->lemma,
                    dictionary::ConjugationType::None, {},  // conj_type and reading removed
                    core::CandidateOrigin::Dictionary, 1.0F, {},
                    result.entry->extended_pos, "dict");

    // Emphatic suffix pattern: word + っ/ッ/ー/ぁぃぅぇぉ/ァィゥェォ (colloquial emphasis)
    // E.g., です→ですっ, ます→ますっ, 行く→行くっ, やばいーー, だぁー
    // Handles consecutive sokuon (っっ), chouon (ーー), and small vowels (ぁぃぅぇぉ)
    // Also handles vowel repetition: きた + ああああ → きたああああ
    // Only apply to verbs, auxiliaries, and adjectives (typical emphatic targets)
    if (end_pos < codepoints.size() &&
        (result.entry->pos == core::PartOfSpeech::Verb ||
         result.entry->pos == core::PartOfSpeech::Auxiliary ||
         result.entry->pos == core::PartOfSpeech::Adjective)) {
      // Count consecutive emphatic characters (sokuon/chouon/small vowels)
      size_t emphatic_end = end_pos;
      std::string emphatic_suffix;
      while (emphatic_end < codepoints.size()) {
        char32_t c = codepoints[emphatic_end];
        // Sokuon (促音)
        if (c == core::hiragana::kSmallTsu) {
          emphatic_suffix += "っ";
        } else if (c == U'ッ') {
          emphatic_suffix += "ッ";
        // Chouon (長音)
        } else if (c == U'ー') {
          emphatic_suffix += "ー";
        // Small hiragana vowels (小書きひらがな母音)
        } else if (c == U'ぁ') {
          emphatic_suffix += "ぁ";
        } else if (c == U'ぃ') {
          emphatic_suffix += "ぃ";
        } else if (c == U'ぅ') {
          emphatic_suffix += "ぅ";
        } else if (c == U'ぇ') {
          emphatic_suffix += "ぇ";
        } else if (c == U'ぉ') {
          emphatic_suffix += "ぉ";
        // Small katakana vowels (小書きカタカナ母音)
        } else if (c == U'ァ') {
          emphatic_suffix += "ァ";
        } else if (c == U'ィ') {
          emphatic_suffix += "ィ";
        } else if (c == U'ゥ') {
          emphatic_suffix += "ゥ";
        } else if (c == U'ェ') {
          emphatic_suffix += "ェ";
        } else if (c == U'ォ') {
          emphatic_suffix += "ォ";
        } else {
          break;
        }
        ++emphatic_end;
      }

      // Track standard emphatic chars separately for cost calculation
      size_t standard_emphatic_chars = emphatic_suffix.size() / core::kJapaneseCharBytes;

      // Also check for repeated vowels matching the final character's vowel
      // E.g., きた + ああああ → きたああああ (た ends in あ-vowel)
      // Requires at least 2 consecutive vowels to be considered emphatic
      size_t vowel_repeat_count = 0;
      if (end_pos > start_pos && emphatic_end < codepoints.size()) {
        // Get final character of the dictionary entry
        char32_t final_char = codepoints[end_pos - 1];
        char32_t expected_vowel = getHiraganaVowel(final_char);

        if (expected_vowel != 0) {
          size_t vowel_start = emphatic_end;

          // Count consecutive occurrences of the expected vowel
          while (emphatic_end < codepoints.size() &&
                 codepoints[emphatic_end] == expected_vowel) {
            ++vowel_repeat_count;
            ++emphatic_end;
          }

          // Require at least 2 repeated vowels for emphatic pattern
          if (vowel_repeat_count >= 2) {
            for (size_t i = 0; i < vowel_repeat_count; ++i) {
              emphatic_suffix += normalize::encodeUtf8(expected_vowel);
            }
          } else {
            // Not enough repetition, reset position
            emphatic_end = vowel_start;
            vowel_repeat_count = 0;
          }
        }
      }

      // Add emphatic variant if we found any emphatic characters
      if (!emphatic_suffix.empty()) {
        // Skip emphatic form if it looks like the sokuon is part of a polite auxiliary
        // E.g., いいっす should be いい+っす, not いいっ+す
        // Check: if emphatic_suffix is just っ and followed by す/さ/せ
        if (emphatic_suffix == "っ" && emphatic_end < codepoints.size()) {
          char32_t after_sokuon = codepoints[emphatic_end];
          // Skip if followed by す/さ/せ (common polite auxiliary patterns)
          if (after_sokuon == U'す' || after_sokuon == U'さ' || after_sokuon == U'せ') {
            // Don't create emphatic form - let っす/っさ/っせ be parsed separately
            emphatic_suffix.clear();
          }
          // Skip emphatic for verbs + っ + て/と (quotative って pattern)
          // E.g., 行くって should be 行く+って, not 行くっ+て
          // Godan verbs in 終止形 (ending く/す/つ/う/ぐ/ぶ/む/ぬ) + っ + て = quotative
          else if (after_sokuon == U'て' || after_sokuon == U'と') {
            if (result.entry->pos == core::PartOfSpeech::Verb && end_pos > start_pos) {
              char32_t verb_final = codepoints[end_pos - 1];
              // Godan 終止形 endings that shouldn't have emphatic っ before quotative て
              if (verb_final == U'く' || verb_final == U'す' || verb_final == U'つ' ||
                  verb_final == U'う' || verb_final == U'ぐ' || verb_final == U'ぶ' ||
                  verb_final == U'む' || verb_final == U'ぬ' || verb_final == U'る') {
                emphatic_suffix.clear();
              }
            }
          }
        }
      }
      if (!emphatic_suffix.empty()) {
        std::string emphatic_surface = result.entry->surface + emphatic_suffix;
        float cost_adjustment;

        if (vowel_repeat_count >= 2) {
          // Give a BONUS for vowel repetition to compete with split alternatives
          // E.g., きたああああ should beat きた + ああああ
          float char_count = static_cast<float>(emphatic_suffix.size() / core::kJapaneseCharBytes);
          cost_adjustment = -0.5F + 0.05F * char_count;
        } else {
          // Standard emphatic chars (sokuon/chouon/small vowels) use penalty
          cost_adjustment = 0.3F * static_cast<float>(standard_emphatic_chars);
        }

        // Determine extended_pos for emphatic form
        // Sokuon-ending verb forms should be VerbOnbinkei (音便形)
        core::ExtendedPOS emphatic_epos = result.entry->extended_pos;
        if (result.entry->pos == core::PartOfSpeech::Verb &&
            emphatic_suffix == "っ") {
          // E.g., い(連用形) + っ → いっ(音便形) for と+いっ+て pattern
          emphatic_epos = core::ExtendedPOS::VerbOnbinkei;
        }

        lattice.addEdge(emphatic_surface,
                        static_cast<uint32_t>(start_pos),
                        static_cast<uint32_t>(emphatic_end),
                        result.entry->pos,
                        cost + cost_adjustment,  // cost from getCategoryCost()
                        flags,
                        result.entry->lemma,
                        dictionary::ConjugationType::None, {},  // v0.8: removed
                        core::CandidateOrigin::Dictionary, 1.0F, {},
                        emphatic_epos, "dict_emphatic");
      }
    }
  }
}

void Tokenizer::addUnknownCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  // Check for dictionary entries at this position to penalize longer unknown words
  size_t byte_pos = charPosToBytePos(codepoints, start_pos);
  auto dict_results = dict_manager_.lookup(text, byte_pos);

  size_t max_dict_length = 0;
  for (const auto& result : dict_results) {
    if (result.entry != nullptr) {
      max_dict_length = std::max(max_dict_length, result.length);
    }
  }

  // Generate unknown word candidates
  auto candidates =
      unknown_gen_.generate(text, codepoints, start_pos, char_types);

  for (const auto& candidate : candidates) {
    uint8_t flags = core::LatticeEdge::kIsUnknown;
    float adjusted_cost = candidate.cost;

    // Penalize unknown words that extend beyond dictionary entries
    bool skip_penalty = false;
    [[maybe_unused]] const char* skip_reason = nullptr;

    // Skip penalty for adverbs (onomatopoeia like わくわく)
    if (candidate.pos == core::PartOfSpeech::Adverb) {
      skip_penalty = true;
      skip_reason = "adverb";
    }

    if (!skip_penalty &&
        (candidate.pos == core::PartOfSpeech::Verb ||
         candidate.pos == core::PartOfSpeech::Adjective)) {
      // Exception: Don't skip verb candidates ending with ず (adverbialized negatives)
      // e.g., 思わず, 絶えず - these are lexicalized adverbs from verb + ず
      bool ends_with_zu = (candidate.surface.size() >= 3 &&
                           candidate.surface.substr(candidate.surface.size() - 3) == "ず");
      for (const auto& result : dict_results) {
        if (result.entry != nullptr) {
          // Case 1: Dictionary entry is also a verb/adjective
          // But allow ず-ending candidates (adverbialized forms)
          if ((result.entry->pos == core::PartOfSpeech::Verb ||
               result.entry->pos == core::PartOfSpeech::Adjective) &&
              !ends_with_zu) {
            skip_penalty = true;
            skip_reason = "dict_has_verb_adj";
            break;
          }
          // Case 2: Pure hiragana verb candidate vs short dictionary entry
          // Also allow prolonged sound mark (ー) as part of hiragana sequence
          // for colloquial patterns like すごーい, やばーい, かわいー
          if (result.length <= 2 &&
              candidate.end - candidate.start >= 3) {
            bool all_hiragana_or_choon = true;
            for (size_t idx = candidate.start; idx < candidate.end && idx < char_types.size(); ++idx) {
              bool is_valid = (char_types[idx] == normalize::CharType::Hiragana);
              // Allow prolonged sound mark (ー, U+30FC) as part of hiragana
              if (!is_valid && idx < codepoints.size() &&
                  normalize::isProlongedSoundMark(codepoints[idx])) {
                is_valid = true;
              }
              if (!is_valid) {
                all_hiragana_or_choon = false;
                break;
              }
            }
            if (all_hiragana_or_choon) {
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
    if (!skip_penalty && candidate.pos == core::PartOfSpeech::Verb) {
      std::string_view surface = candidate.surface;
      if (utf8::endsWith(surface, "っとく")) {
        skip_penalty = true;
        skip_reason = "colloquial_contraction";
      }
    }

    // Case 5: Short hiragana verb candidates ending with te/de-form
    // Handles cases like ねて (寝る), でて (出る), みて (見る) where
    // dictionary only has kanji form but surface is pure hiragana.
    // These 2-char patterns don't meet Case 2's ≥3 char threshold.
    if (!skip_penalty && candidate.pos == core::PartOfSpeech::Verb) {
      std::string_view surface = candidate.surface;
      size_t len = candidate.end - candidate.start;
      // Check for 2-char hiragana verbs ending in て/で
      if (len == 2 && surface.size() >= core::kJapaneseCharBytes) {
        bool all_hiragana = true;
        for (size_t idx = candidate.start; idx < candidate.end && idx < char_types.size(); ++idx) {
          if (char_types[idx] != normalize::CharType::Hiragana) {
            all_hiragana = false;
            break;
          }
        }
        if (all_hiragana) {
          // Check if ends with て or で (te-form markers)
          std::string_view last_char = utf8::lastChar(surface);
          if (utf8::equalsAny(last_char, {"て", "で"})) {
            skip_penalty = true;
            skip_reason = "short_te_form";
          }
        }
      }
    }

    // Case 6: Revoke skip for long hiragana verbs ending with ちゃう/ちゃっ/ちゃい
    // These are auxiliary chains (e.g., されちゃう = さ+れ+ちゃう,
    // なっちゃう = なっ+ちゃう, やっちゃう = やっ+ちゃう) that should split.
    if (skip_penalty && candidate.pos == core::PartOfSpeech::Verb &&
        candidate.end - candidate.start >= 4) {
      std::string_view surface = candidate.surface;
      bool ends_chau = utf8::endsWith(surface, "ちゃう") ||
                       utf8::endsWith(surface, "ちゃっ") ||
                       utf8::endsWith(surface, "ちゃい");
      if (ends_chau) {
        // Check if all hiragana
        bool all_hira = true;
        for (size_t idx = candidate.start;
             idx < candidate.end && idx < char_types.size(); ++idx) {
          if (char_types[idx] != normalize::CharType::Hiragana) {
            all_hira = false;
            break;
          }
        }
        if (all_hira) {
          skip_penalty = false;
          skip_reason = nullptr;
        }
      }
    }

    // Case 4: Pure hiragana OTHER (likely readings/furigana)
    // Reduce penalty for long varied hiragana sequences
    // Also allow prolonged sound mark (ー) as part of hiragana sequence
    bool reduced_penalty = false;
    bool skip_dict_penalty = false;
    [[maybe_unused]] const char* skip_dict_reason = nullptr;
    if (!skip_penalty && candidate.pos == core::PartOfSpeech::Other &&
        candidate.end - candidate.start >= 4) {
      bool all_hiragana_or_choon = true;
      bool all_same = true;
      char32_t first_cp = 0;
      for (size_t idx = candidate.start;
           idx < candidate.end && idx < char_types.size(); ++idx) {
        bool is_valid = (char_types[idx] == normalize::CharType::Hiragana);
        // Allow prolonged sound mark (ー, U+30FC) as part of hiragana
        if (!is_valid && idx < codepoints.size() &&
            normalize::isProlongedSoundMark(codepoints[idx])) {
          is_valid = true;
        }
        if (!is_valid) {
          all_hiragana_or_choon = false;
          break;
        }
        if (idx < codepoints.size()) {
          if (idx == candidate.start) {
            first_cp = codepoints[idx];
          } else if (codepoints[idx] != first_cp) {
            all_same = false;
          }
        }
      }
      if (all_hiragana_or_choon && !all_same) {
        reduced_penalty = true;
      }
    }

    // Skip dict length penalty for katakana sequences (loanwords)
    // Loanwords like マスカラ, デスクトップ often exceed dictionary coverage
    if (!skip_penalty && candidate.pos == core::PartOfSpeech::Noun &&
        candidate.end - candidate.start >= 3) {
      bool all_katakana = true;
      for (size_t idx = candidate.start;
           idx < candidate.end && idx < char_types.size(); ++idx) {
        if (char_types[idx] != normalize::CharType::Katakana &&
            !(idx < codepoints.size() &&
              normalize::isProlongedSoundMark(codepoints[idx]))) {
          all_katakana = false;
          break;
        }
      }
      if (all_katakana) {
        skip_dict_penalty = true;
        skip_dict_reason = "all_katakana";
      }
    }

    // Skip dict length penalty for kanji compound sequences (2-6 chars)
    // Common compounds like 人工知能, 自然言語処理 may not be in dictionary
    // Keep compounds connected - splitting should be driven by PREFIX/SUFFIX
    // markers or dictionary entries, not length heuristics
    if (!skip_penalty && !skip_dict_penalty &&
        candidate.pos == core::PartOfSpeech::Noun) {
      size_t len = candidate.end - candidate.start;
      if (len >= 2 && len <= 6) {
        bool all_kanji = true;
        for (size_t idx = candidate.start;
             idx < candidate.end && idx < char_types.size(); ++idx) {
          if (char_types[idx] != normalize::CharType::Kanji) {
            all_kanji = false;
            break;
          }
        }
        if (all_kanji) {
          skip_dict_penalty = true;
          skip_dict_reason = "all_kanji_compound";

          // When a dictionary entry exists as a proper prefix of this compound,
          // add a moderate penalty to prefer the dict-split path.
          // E.g., 第一(dict) + 毛 should beat 第一毛(compound)
          // Only when the prefix covers a significant portion (>= half)
          // to avoid splitting 自然言語処理 at 自然(2/6).
          for (const auto& result : dict_results) {
            if (result.entry != nullptr && result.length >= 2 &&
                result.length < len && result.length * 2 >= len &&
                result.entry->pos != core::PartOfSpeech::Noun) {
              constexpr float kDictPrefixPenalty = 1.5F;
              adjusted_cost += kDictPrefixPenalty;
              SUZUME_DEBUG_LOG_VERBOSE("[TOK_UNK] \"" << candidate.surface
                  << "\" (NOUN): +" << kDictPrefixPenalty
                  << " (kanji_compound_dict_prefix, dict=\""
                  << result.entry->surface << "\")\n");
              break;
            }
          }

          // When a non-NOUN dict entry from a prior position overlaps with
          // this compound's first character, penalize the compound.
          // E.g., その後(dict ADV, pos=0, len=3) overlaps with 後猫(pos=2)
          // → penalize 後猫 to prefer その後+猫 split.
          constexpr size_t kMaxLookback = 4;
          bool found_overlap = false;
          for (size_t back = 1;
               back <= kMaxLookback && back <= start_pos && !found_overlap;
               ++back) {
            size_t prev_pos = start_pos - back;
            size_t prev_byte = charPosToBytePos(codepoints, prev_pos);
            auto prev_results = dict_manager_.lookup(text, prev_byte);
            for (const auto& result : prev_results) {
              if (result.entry != nullptr && result.length >= 2 &&
                  result.length > back &&
                  result.entry->pos != core::PartOfSpeech::Noun) {
                constexpr float kDictOverlapPenalty = 1.5F;
                adjusted_cost += kDictOverlapPenalty;
                SUZUME_DEBUG_LOG_VERBOSE("[TOK_UNK] \"" << candidate.surface
                    << "\" (NOUN): +" << kDictOverlapPenalty
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
    bool is_suru_verb = (candidate.pos == core::PartOfSpeech::Verb &&
                         candidate.conj_type == dictionary::ConjugationType::Suru);

    // Check for pure hiragana verb (e.g., ねる, もらう, あげる)
    // These should not be penalized heavily - they are legitimate verb forms
    bool is_pure_hiragana_verb = false;
    if (candidate.pos == core::PartOfSpeech::Verb &&
        candidate.end - candidate.start >= 2) {
      bool all_hiragana = true;
      for (size_t idx = candidate.start;
           idx < candidate.end && idx < char_types.size(); ++idx) {
        if (char_types[idx] != normalize::CharType::Hiragana) {
          all_hiragana = false;
          break;
        }
      }
      // Only skip penalty for short pure hiragana verbs (2-4 chars)
      // Longer ones might be suspicious (e.g., いただきます could be wrong split)
      if (all_hiragana && candidate.end - candidate.start <= 4) {
        is_pure_hiragana_verb = true;
      }
    }

    // Check for single-kanji stem + hiragana verb (e.g., 残って, 通る, 飛ぶ)
    // Single-kanji verb stems are common in Japanese (残る, 立つ, 打つ, etc.)
    // These should not be penalized for exceeding dict length
    bool is_kanji_stem_verb = false;
    if (candidate.pos == core::PartOfSpeech::Verb &&
        candidate.end - candidate.start >= 2 &&
        candidate.start < char_types.size() &&
        char_types[candidate.start] == normalize::CharType::Kanji) {
      // Check: first char is kanji, rest are hiragana
      bool rest_hiragana = true;
      for (size_t idx = candidate.start + 1;
           idx < candidate.end && idx < char_types.size(); ++idx) {
        if (char_types[idx] != normalize::CharType::Hiragana) {
          rest_hiragana = false;
          break;
        }
      }
      if (rest_hiragana) {
        is_kanji_stem_verb = true;
      }
    }

    bool exceeds_dict = (max_dict_length > 0 &&
                         candidate.end - candidate.start > max_dict_length);
    if (exceeds_dict) {
      if (skip_penalty) {
        SUZUME_DEBUG_LOG_VERBOSE("[TOK_SKIP] \"" << candidate.surface << "\" ("
                          << core::posToString(candidate.pos) << "): "
                          << "skip exceeds_dict_length (" << skip_reason << ")\n");
      } else if (skip_dict_penalty) {
        SUZUME_DEBUG_LOG_VERBOSE("[TOK_SKIP] \"" << candidate.surface << "\" ("
                          << core::posToString(candidate.pos) << "): "
                          << "skip exceeds_dict_length (" << skip_dict_reason << ")\n");
      } else if (is_suru_verb) {
        SUZUME_DEBUG_LOG_VERBOSE("[TOK_SKIP] \"" << candidate.surface << "\" ("
                          << core::posToString(candidate.pos) << "): "
                          << "skip exceeds_dict_length (suru_verb)\n");
      } else if (candidate.has_suffix) {
        SUZUME_DEBUG_LOG_VERBOSE("[TOK_SKIP] \"" << candidate.surface << "\" ("
                          << core::posToString(candidate.pos) << "): "
                          << "skip exceeds_dict_length (has_suffix)\n");
      } else if (is_pure_hiragana_verb) {
        SUZUME_DEBUG_LOG_VERBOSE("[TOK_SKIP] \"" << candidate.surface << "\" ("
                          << core::posToString(candidate.pos) << "): "
                          << "skip exceeds_dict_length (pure_hiragana_verb)\n");
      } else if (is_kanji_stem_verb) {
        SUZUME_DEBUG_LOG_VERBOSE("[TOK_SKIP] \"" << candidate.surface << "\" ("
                          << core::posToString(candidate.pos) << "): "
                          << "skip exceeds_dict_length (kanji_stem_verb)\n");
      } else {
        float penalty = reduced_penalty ? 1.0F : 3.5F;
        adjusted_cost += penalty;
        SUZUME_DEBUG_LOG_VERBOSE("[TOK_UNK] \"" << candidate.surface << "\" ("
                          << core::posToString(candidate.pos) << "): +"
                          << penalty << " (exceeds_dict_length"
                          << (reduced_penalty ? ", pure_hiragana" : "")
                          << ", dict_max=" << max_dict_length << ")\n");
      }
    }

    // For verb candidates, check if the hiragana suffix is a known particle
    if (candidate.pos == core::PartOfSpeech::Verb &&
        candidate.end > candidate.start) {
      size_t hiragana_start = candidate.start;
      while (hiragana_start < candidate.end &&
             hiragana_start < char_types.size() &&
             char_types[hiragana_start] != normalize::CharType::Hiragana) {
        ++hiragana_start;
      }

      if (hiragana_start < candidate.end) {
        size_t suffix_byte_start = charPosToBytePos(codepoints, hiragana_start);
        size_t suffix_byte_end = charPosToBytePos(codepoints, candidate.end);
        std::string_view hiragana_suffix =
            text.substr(suffix_byte_start, suffix_byte_end - suffix_byte_start);

        // Don't penalize verb conjugation endings
        // - te-form: て/で/って/んで/いて/いで
        // - renyoukei し: extremely common for suru/godan verbs (分割し, 話し)
        bool is_verb_ending = utf8::equalsAny(hiragana_suffix,
            {"て", "で", "って", "んで", "いて", "いで", "し"});

        // Skip penalty if:
        // - Known verb conjugation ending (te-form, renyoukei)
        // - Candidate has has_suffix flag (mizenkei for ぬ/れべき patterns)
        if (!is_verb_ending && !candidate.has_suffix) {
          size_t suffix_byte_pos = charPosToBytePos(codepoints, hiragana_start);
          auto suffix_results = dict_manager_.lookup(text, suffix_byte_pos);

          for (const auto& result : suffix_results) {
            if (result.entry != nullptr &&
                result.entry->pos == core::PartOfSpeech::Particle) {
              size_t suffix_len = candidate.end - hiragana_start;
              if (result.length == suffix_len) {
                adjusted_cost += 1.5F;
                SUZUME_DEBUG_LOG_VERBOSE("[TOK_UNK] \"" << candidate.surface
                                  << "\": +1.5 (particle_suffix=\""
                                  << hiragana_suffix << "\")\n");
                break;
              }
            }
          }
        }
      }
    }

    std::string surface_str(candidate.surface);

    // Set HasSuffix flag for verb/adj candidates with suffix marking
    if (candidate.has_suffix) {
      flags |= static_cast<uint8_t>(core::EdgeFlags::HasSuffix);
    }

#ifdef SUZUME_DEBUG_INFO
    lattice.addEdge(surface_str, static_cast<uint32_t>(candidate.start),
                    static_cast<uint32_t>(candidate.end), candidate.pos,
                    adjusted_cost, flags, candidate.lemma, candidate.conj_type,
                    {},  // reading
                    candidate.origin, candidate.confidence, candidate.pattern,
                    candidate.extended_pos, candidate.epos_source);
#else
    lattice.addEdge(surface_str, static_cast<uint32_t>(candidate.start),
                    static_cast<uint32_t>(candidate.end), candidate.pos,
                    adjusted_cost, flags, candidate.lemma, candidate.conj_type,
                    {},  // reading
                    core::CandidateOrigin::Unknown, 0.0F, {},  // debug params
                    candidate.extended_pos);
#endif
  }
}

void Tokenizer::addMixedScriptCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  analysis::addMixedScriptCandidates(lattice, text, codepoints, start_pos,
                                      char_types, scorer_, dict_manager_);
}

void Tokenizer::addCompoundSplitCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  analysis::addCompoundSplitCandidates(lattice, text, codepoints, start_pos,
                                        char_types, dict_manager_, scorer_);
}

void Tokenizer::addNounVerbSplitCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  analysis::addNounVerbSplitCandidates(lattice, text, codepoints, start_pos,
                                        char_types, dict_manager_, scorer_,
                                        inflection_);
}

void Tokenizer::addCompoundVerbJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  analysis::addCompoundVerbJoinCandidates(lattice, text, codepoints, start_pos,
                                           char_types, dict_manager_, scorer_,
                                           inflection_);
}

void Tokenizer::addHiraganaCompoundVerbJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  analysis::addHiraganaCompoundVerbJoinCandidates(
      lattice, text, codepoints, start_pos, char_types, dict_manager_, scorer_,
      inflection_);
}

void Tokenizer::addPrefixNounJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  analysis::addPrefixNounJoinCandidates(lattice, text, codepoints, start_pos,
                                         char_types, dict_manager_, scorer_);
}

void Tokenizer::addAdjectiveSugiruJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  analysis::addAdjectiveSugiruJoinCandidates(lattice, text, codepoints, start_pos,
                                              char_types, dict_manager_, scorer_);
}

void Tokenizer::addKatakanaSugiruJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  analysis::addKatakanaSugiruJoinCandidates(lattice, text, codepoints, start_pos,
                                             char_types, scorer_);
}

void Tokenizer::addTeFormAuxiliaryCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  analysis::addTeFormAuxiliaryCandidates(lattice, text, codepoints, start_pos,
                                          char_types, scorer_, inflection_);
}

void Tokenizer::addTaruAdjectiveJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  analysis::addTaruAdjectiveJoinCandidates(lattice, text, codepoints, start_pos,
                                            char_types, scorer_);
}

void Tokenizer::addVerbSuffixNounJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types) const {
  analysis::addVerbSuffixNounJoinCandidates(lattice, text, codepoints, start_pos,
                                             char_types, dict_manager_, scorer_);
}

}  // namespace suzume::analysis
