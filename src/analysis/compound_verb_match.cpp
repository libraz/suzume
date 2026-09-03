/**
 * @file compound_verb_match.cpp
 * @brief V1 verification and V2 matching for compound verbs
 */
#include "analysis/dictionary_probe.h"
#include "join_compound_verb_internal.h"

namespace suzume::analysis::compound_verb_detail {

namespace {

// A compound-verb candidate needs inflectional evidence, not a bare
// continuative ending. Politeness is intentionally excluded: ます remains a
// separate auxiliary token.
bool isNumeralOnlySpan(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  if (start_pos >= end_pos || end_pos > codepoints.size()) {
    return false;
  }
  for (size_t pos = start_pos; pos < end_pos; ++pos) {
    if (!normalize::isNumeralCodepoint(codepoints[pos])) {
      return false;
    }
  }
  return true;
}

bool hasAuxiliarySuffix(std::string_view suffix) {
  return !suffix.empty() && utf8::containsAny(suffix, {"た", "て", "で", "だ", "ない", "れ"});
}

bool beginsMizenkeiAuxiliary(std::string_view text, size_t start_byte, std::string_view mizenkei) {
  if (mizenkei.empty() || start_byte + mizenkei.size() + core::kJapaneseCharBytes > text.size() ||
      text.substr(start_byte, mizenkei.size()) != mizenkei) {
    return false;
  }
  const std::string_view suffix = text.substr(start_byte + mizenkei.size());
  size_t following_pos = start_byte + mizenkei.size();
  const char32_t starter = normalize::decodeUtf8(text, following_pos);
  const char32_t after_starter = following_pos < text.size() ? normalize::decodeUtf8(text, following_pos) : U'\0';
  if (isMizenkeiAuxiliaryStarter(starter, after_starter)) {
    return true;
  }

  // The shortened causative-passive inserts さ before the passive auxiliary:
  // 考え込ま+さ+れた.  A bare さ is not sufficient evidence because it also
  // nominalizes adjectives; require a valid continuation of れる.
  if (!utf8::startsWith(suffix, "され")) {
    return false;
  }
  const std::string_view passive_tail = suffix.substr(core::kTwoJapaneseCharBytes);
  return utf8::startsWithAny(passive_tail, {"る", "た", "て", "ない", "なかっ", "なけれ", "ます", "ませ", "ば"});
}

bool isCompoundVerbOrNominalizationAttested(const dictionary::DictionaryManager& dict_manager, std::string_view base,
                                            V2VerbType verb_type) {
  if (base.empty()) {
    return false;
  }
  if (dict_manager.lookupExact(base, core::PartOfSpeech::Verb) != nullptr) {
    return true;
  }
  const std::string nominalized = generateRenyokei(base, "", verb_type);
  return !nominalized.empty() && dict_manager.lookupExact(nominalized, core::PartOfSpeech::Noun) != nullptr;
}

}  // namespace

CompoundVerbMatch findCompoundVerbMatch(
    std::string_view text, const std::vector<char32_t>& codepoints, const ByteOffsets& byte_offsets, size_t start_pos,
    const std::vector<normalize::CharType>& char_types, size_t kanji_end, size_t v2_start, char32_t base_ending,
    bool is_sokuonbin, bool is_ichidan, bool has_kanji_v2_after_bare_ichidan, bool dict_compound_v1,
    std::string_view dict_compound_v1_lemma, const dictionary::DictionaryManager& dict_manager,
    const grammar::Inflection& inflection, bool hiragana_v1, bool allow_closed_onbin_v1) {
  if (v2_start >= codepoints.size()) {
    return {};
  }

  const size_t start_byte = byteOffsetAt(byte_offsets, start_pos);
  const size_t v2_start_byte = byteOffsetAt(byte_offsets, v2_start);
  const std::string_view v1_surface = text.substr(start_byte, v2_start_byte - start_byte);
  bool hiragana_v1_in_dictionary = false;
  if (hiragana_v1) {
    // Every form a lexical compound builds on is a continuative or its onbin,
    // and none of them ends in an a-row kana: the i-row and e-row spell the
    // continuative, っ and ん spell the onbin, and the a-row spells the irrealis,
    // which takes an auxiliary rather than a verb. A hiragana V1 ending there
    // has therefore reached past the stem into the auxiliary that closes the
    // predicate (き+た of 流れてきた, れ+た of 落とされた), and joining it would
    // build a lexical verb on top of a finished clause. The V2 side of this
    // boundary is guarded by the past-auxiliary test further down.
    // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
    if (v2_start > start_pos && grammar::isARowCodepoint(codepoints[v2_start - 1])) {
      SUZUME_DEBUG_LOG_VERBOSE("[COMPOUND] rejected a-row tail on hiragana V1: " << v1_surface << "\n");
      return {};
    }
    std::string v1_base;
    if (grammar::isSuruRenyokeiSurface(v1_surface)) {
      v1_base = "する";
    } else if (is_ichidan) {
      v1_base = normalize::concat(v1_surface, "る");
    } else {
      v1_base = std::string(v1_surface.substr(0, v1_surface.size() - core::kJapaneseCharBytes));
      v1_base += normalize::encodeUtf8(base_ending);
    }
    hiragana_v1_in_dictionary = dict_manager.lookupExact(v1_base, core::PartOfSpeech::Verb) != nullptr;
    bool hiragana_v1_has_strong_inflection = false;
    if (!hiragana_v1_in_dictionary) {
      for (const auto& candidate : inflection.analyze(v1_surface)) {
        if (candidate.base_form == v1_base &&
            candidate.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence) {
          hiragana_v1_has_strong_inflection = true;
          break;
        }
      }
    }
    if (!hiragana_v1_in_dictionary && !allow_closed_onbin_v1) {
      for (size_t pos = start_pos; pos < v2_start; ++pos) {
        // A particle reading is unmistakable at V1's own start and behind the
        // u-row ending of a finite predicate: no okurigana sits there. Anywhere
        // else one kana may merely share a particle's spelling (the ど of
        // たどり), and a V1 the conjugation table reconstructs keeps it. The
        // table reconstructs a base for almost any kana run, though, so it is
        // no defence at a real clause boundary (るにし -> るにす, which turns
        // 食べる+に into 食べ + るにしたっ).
        const bool particle_position_is_unambiguous = pos == start_pos || kana::isURowCodepoint(codepoints[pos - 1]);
        if (hiragana_v1_has_strong_inflection && !particle_position_is_unambiguous) {
          continue;
        }
        const auto* particle = lookupEntryInRange(dict_manager, codepoints, pos, pos + 1, core::PartOfSpeech::Particle);
        if (particle != nullptr && particle->extended_pos != core::ExtendedPOS::ParticleFinal) {
          SUZUME_DEBUG_LOG_VERBOSE("[COMPOUND] rejected particle inside hiragana V1: "
                                   << extractSubstring(codepoints, pos, pos + 1) << "\n");
          return {};
        }
      }
    }
  }

  // A compound verb joins two verbal components directly. If a closed-class
  // particle occurs between the prospective V1 and V2 boundaries, the span is
  // compositional instead (読む+だけ+あって), not a compound verb.
  for (size_t particle_start = start_pos; particle_start < v2_start; ++particle_start) {
    const std::string particle_probe = extractSubstring(codepoints, particle_start, v2_start);
    // Away from the span's own start, the same u-row evidence the single-char
    // check below relies on decides whether a clause actually closed there.
    // Without it the okurigana of a continuative is read as a particle the
    // moment it happens to spell one (向か+い, where かい is the final
    // particle), and the compound is rejected before V1 is ever analyzed.
    const bool closes_preceding_clause =
        particle_start == start_pos || kana::isURowCodepoint(codepoints[particle_start - 1]);
    for (const auto& match : dict_manager.lookup(particle_probe, 0)) {
      if (closes_preceding_clause && match.entry != nullptr && match.entry->pos == core::PartOfSpeech::Particle &&
          normalize::utf8Length(match.entry->surface) > 1 &&
          particle_start + normalize::utf8Length(match.entry->surface) <= v2_start) {
        if (!hiragana_v1_in_dictionary) {
          return {};
        }
      }
    }

    // A terminal u-row verb followed by a case particle is a clause boundary,
    // even when the particle is a single character (行く+に+越した).  A
    // continuative ending such as し remains eligible for lexical compounds.
    const auto* particle = dict_manager.lookupExact(particle_probe, core::PartOfSpeech::Particle);
    if (particle != nullptr && particle_start > start_pos && kana::isURowCodepoint(codepoints[particle_start - 1])) {
      if (!hiragana_v1_in_dictionary) {
        return {};
      }
    }
  }

  // Inflection analyzer for V2 detection (shared instance from Tokenizer)

  // Find extent of hiragana after v2_start for inflection analysis
  size_t v2_hiragana_end = findCharRegionEnd(char_types, v2_start, 8, CharType::Hiragana);

  // Look for V2 (subsidiary verb)
  // We collect the best match rather than returning immediately.
  // This allows renyokei matches (すぎ) to take precedence over inflection
  // matches (すぎた) when the inflection match includes an auxiliary suffix.
  CompoundVerbMatch best_match;

  for (const auto& v2_verb : subsidiaryVerbs()) {
    if (!v2_verb.joins_general) {
      continue;
    }
    std::string_view v2_surface(v2_verb.surface);
    std::string_view v2_reading(v2_verb.joins_reading && v2_verb.reading ? v2_verb.reading : "");
    if (grammar::isSuruRenyokeiSurface(v1_surface) && !v2_verb.joins_suru) {
      continue;
    }

    // The hiragana reading of compound V2 入る overlaps with the aspect
    // auxiliary いる.  A preceding て/で is a grammatical boundary
    // (異なっ|て|いる), not a renyokei stem for a compound ending in 入る.
    // Keep real compounds such as 立ち入る eligible: their V2 begins directly
    // after the V1 renyokei and therefore has no te-form particle before it.
    if (v2_reading == "いる" && v2_start > start_pos &&
        (codepoints[v2_start - 1] == U'て' || codepoints[v2_start - 1] == U'で')) {
      continue;
    }

    // The hiragana V2 reading 切る also overlaps with the lexical potential
    // verb できる.  Its leading で completes that word, rather than forming an
    // ichidan V1 stem (化でる) before a compound-verb V2.  Kanji-written V2
    // compounds such as 撫で切る are unaffected.
    if (v2_reading == "きる" && v2_start > start_pos && codepoints[v2_start - 1] == U'で') {
      continue;
    }

    // Determine if this is a renyokei entry by checking if base_form != surface
    // Renyokei entries: 過ぎ (base 過ぎる), 出し (base 出す), etc.
    bool is_renyokei_entry = false;

    // Check if text at v2_start matches this V2 verb (kanji or reading)
    bool matched_kanji = false;
    bool matched_reading = false;
    bool matched_inflected = false;
    bool matched_kateikei = false;
    bool matched_potential = false;
    bool matched_renyokei_via_reading = false;
    size_t matched_len = 0;
    bool inflection_includes_aux = false;

    // Try kanji match first
    if (v2_verb.joins_surface && v2_start_byte + v2_surface.size() <= text.size()) {
      std::string_view text_at_v2 = text.substr(v2_start_byte, v2_surface.size());
      if (text_at_v2 == v2_surface) {
        matched_kanji = true;
        matched_len = v2_surface.size();
      }
    }

    // The hiragana spellings とる and どる are contracted progressive
    // auxiliaries after a verb stem. Keep the lexical V2 entries available
    // in their kanji spelling (受け取る), but do not build a false compound
    // candidate over the productive auxiliary sequence (食べとった).
    if (!matched_kanji && char_types[v2_start] == CharType::Hiragana &&
        (v2_reading == "とる" || v2_reading == "どる")) {
      const std::string full_compound = normalize::concat(v1_surface, v2_reading);
      if (dict_manager.lookupExact(full_compound, core::PartOfSpeech::Verb) == nullptr) {
        continue;
      }
    }

    // Hiragana そう before conditional/copular な is the appearance
    // auxiliary (起こり+そう+なら), not the lexical compound V2 添う.
    // Kanji-written 添う remains available as an ordinary compound verb.
    if (v2_reading == "そう" && char_types[v2_start] == CharType::Hiragana &&
        utf8::startsWith(text.substr(v2_start_byte), "そうな")) {
      continue;
    }

    // Try reading (hiragana) match if kanji didn't match
    if (!matched_kanji && !v2_reading.empty() && v2_start_byte + v2_reading.size() <= text.size()) {
      std::string_view text_at_v2 = text.substr(v2_start_byte, v2_reading.size());
      if (text_at_v2 == v2_reading) {
        matched_reading = true;
        matched_len = v2_reading.size();
      }
    }

    // Try a V2 renyokei match so a following auxiliary stays separate.
    // e.g., 申し上げます → 申し上げ + ます (match V2 renyokei "上げ", not full "上げます")
    bool matched_renyokei = false;
    if (!matched_kanji && !matched_reading) {
      // Generate V2 renyokei
      std::string kanji_renyokei =
          v2_verb.joins_surface ? generateKanjiRenyokei(v2_surface, v2_reading, v2_verb.verb_type) : "";
      std::string hira_renyokei = generateRenyokei(v2_reading, "", v2_verb.verb_type);

      // Try kanji renyokei match
      if (!kanji_renyokei.empty() && v2_start_byte + kanji_renyokei.size() <= text.size()) {
        std::string_view text_at_v2 = text.substr(v2_start_byte, kanji_renyokei.size());
        if (text_at_v2 == kanji_renyokei) {
          matched_renyokei = true;
          matched_len = kanji_renyokei.size();
          is_renyokei_entry = true;  // Mark as renyokei match
        }
      }

      // Try hiragana renyokei match if kanji didn't match
      if (!matched_renyokei && !hira_renyokei.empty() && v2_start_byte + hira_renyokei.size() <= text.size()) {
        std::string_view text_at_v2 = text.substr(v2_start_byte, hira_renyokei.size());
        if (text_at_v2 == hira_renyokei) {
          // Skip Ichidan V1 + V2「出る」renyokei (で) match
          // Ichidan verbs use て for te-form, never で.
          // E.g., 付けで should be 付け(VERB)+で(PARTICLE), not 付け出る (compound)
          // But Godan+出る is valid: 飛び出る (飛ぶ→飛び+出る)
          if (is_ichidan && hira_renyokei == "で") {
            continue;  // Skip V2「出る」for Ichidan V1
          }
          matched_renyokei = true;
          matched_renyokei_via_reading = true;
          matched_len = hira_renyokei.size();
          is_renyokei_entry = true;  // Mark as renyokei match
        }
      }
    }

    // A Godan potential form belongs to the same search unit as its compound
    // base: 取り + 戻せる → 取り戻せる. Generate it from every allowlisted
    // V2 rather than adding per-verb potential entries.
    if (!matched_kanji && !matched_reading && !matched_renyokei) {
      std::string kanji_potential =
          v2_verb.joins_surface ? generateGodanPotential(v2_surface, "", v2_verb.verb_type) : "";
      std::string hira_potential = generateGodanPotential(v2_reading, "", v2_verb.verb_type);
      if (!kanji_potential.empty() && v2_start_byte + kanji_potential.size() <= text.size() &&
          text.substr(v2_start_byte, kanji_potential.size()) == kanji_potential) {
        matched_potential = true;
        matched_len = kanji_potential.size();
      } else if (!hira_potential.empty() && v2_start_byte + hira_potential.size() <= text.size() &&
                 text.substr(v2_start_byte, hira_potential.size()) == hira_potential) {
        matched_potential = true;
        matched_len = hira_potential.size();
        matched_renyokei_via_reading = true;
      }

      // Potential forms conjugate as Ichidan. Expose their stem before a
      // negative auxiliary so compound boundaries survive 取れ+ない/なかっ/なけれ.
      auto tryPotentialStem = [&](const std::string& potential, bool via_reading) {
        if (matched_potential || potential.size() <= core::kJapaneseCharBytes) {
          return;
        }
        std::string stem = potential.substr(0, potential.size() - core::kJapaneseCharBytes);
        size_t after_stem = v2_start_byte + stem.size();
        if (text.substr(v2_start_byte, stem.size()) != stem || after_stem >= text.size()) {
          return;
        }
        std::string_view following = text.substr(after_stem);
        if (utf8::startsWithAny(following, {"ない", "なかっ", "なけれ"})) {
          matched_potential = true;
          matched_len = stem.size();
          matched_renyokei_via_reading = via_reading;
        }
      };
      tryPotentialStem(kanji_potential, false);
      tryPotentialStem(hira_potential, true);
    }

    // A Godan V2 forms its conditional from the e-row stem plus ば
    // (踏み外せ+ば, 行き違え+ば).  The e-row surface is also the stem of a
    // potential verb, so require the following ば before treating it as
    // kateikei; unrestricted matching would incorrectly absorb an independent
    // potential predicate.
    if (!matched_kanji && !matched_reading && !matched_renyokei && !matched_potential &&
        v2_verb.verb_type == V2VerbType::Godan) {
      const std::string kanji_kateikei =
          v2_verb.joins_surface ? generateKateikei(v2_surface, "", v2_verb.verb_type) : "";
      const std::string hira_kateikei = !v2_reading.empty() ? generateKateikei(v2_reading, "", v2_verb.verb_type) : "";
      auto tryKateikei = [&](const std::string& kateikei, bool via_reading) {
        if (matched_kateikei || kateikei.empty() ||
            v2_start_byte + kateikei.size() + core::kJapaneseCharBytes > text.size()) {
          return;
        }
        const size_t after_kateikei = v2_start_byte + kateikei.size();
        if (text.substr(v2_start_byte, kateikei.size()) == kateikei &&
            text.substr(after_kateikei, core::kJapaneseCharBytes) == "ば") {
          matched_kateikei = true;
          matched_len = kateikei.size();
          matched_renyokei_via_reading = via_reading;
        }
      };
      tryKateikei(kanji_kateikei, false);
      tryKateikei(hira_kateikei, true);
    }

    // A Godan V2 exposes its o-row stem before the closed volitional auxiliary
    // う (考え出そ+う, 取り戻そ+う). Match the stem even when a shorter V2
    // renyokei is homographic with its prefix (出る→出), because the following
    // auxiliary supplies decisive inflectional evidence.
    bool matched_volitional = false;
    if (v2_verb.verb_type == V2VerbType::Godan) {
      const std::string kanji_volitional =
          v2_verb.joins_surface ? generateVolitionalStem(v2_surface, "", v2_verb.verb_type) : "";
      const std::string hira_volitional =
          !v2_reading.empty() ? generateVolitionalStem(v2_reading, "", v2_verb.verb_type) : "";
      auto tryVolitional = [&](const std::string& stem, bool via_reading) {
        if (matched_volitional || stem.empty() ||
            v2_start_byte + stem.size() + core::kJapaneseCharBytes > text.size()) {
          return;
        }
        if (text.substr(v2_start_byte, stem.size()) != stem ||
            text.substr(v2_start_byte + stem.size(), core::kJapaneseCharBytes) != "う") {
          return;
        }
        if (!hiragana_v1) {
          const size_t volitional_end = v2_start + normalize::utf8Length(stem) + 1;
          for (size_t split_pos = v2_start + 1; split_pos < volitional_end; ++split_pos) {
            const auto* left_auxiliary =
                lookupEntryInRange(dict_manager, codepoints, v2_start, split_pos, core::PartOfSpeech::Auxiliary);
            const auto* right_auxiliary =
                lookupEntryInRange(dict_manager, codepoints, split_pos, volitional_end, core::PartOfSpeech::Auxiliary);
            if (left_auxiliary != nullptr && right_auxiliary != nullptr &&
                right_auxiliary->extended_pos == core::ExtendedPOS::AuxAppearanceSou) {
              return;
            }
          }
        }
        matched_volitional = true;
        matched_len = stem.size();
        matched_renyokei_via_reading = via_reading;
      };
      tryVolitional(kanji_volitional, false);
      tryVolitional(hira_volitional, true);
    }

    // Keep a Godan mizenkei before its auxiliary separate.  Otherwise an
    // inflection match over the longer span (しきらない) would hide the
    // grammatical boundary that the mizenkei candidate below represents.
    const std::string kanji_mizen = v2_verb.joins_surface ? generateMizenkei(v2_surface, "", v2_verb.verb_type) : "";
    const std::string hira_mizen = !v2_reading.empty() ? generateMizenkei(v2_reading, "", v2_verb.verb_type) : "";
    const bool mizenkei_before_aux = beginsMizenkeiAuxiliary(text, v2_start_byte, kanji_mizen) ||
                                     beginsMizenkeiAuxiliary(text, v2_start_byte, hira_mizen);

    // Try inflection analysis for inflected V2 forms (e.g., きった, 込んだ, 巡った)
    // Only for base forms (not renyokei entries) to avoid double-matching
    // Skip if already matched via renyokei to prevent aux detection overriding renyokei match
    if (!matched_kanji && !matched_reading && !matched_renyokei && !matched_potential && !matched_kateikei &&
        !matched_volitional && !mizenkei_before_aux && !v2_reading.empty()) {
      std::string_view base_ending(v2_verb.base_ending);
      // Only try inflection for base forms (ending in る/す/く/う/む/つ/ぶ/ぐ/ぬ or ichidan endings)
      if (utf8::equalsAny(base_ending, {"る", "す", "く", "う", "む", "つ", "ぶ", "ぐ", "ぬ", "める", "ける", "れる",
                                        "える", "げる", "てる", "せる", "ちる"})) {
        // Case 1: Hiragana V2 inflected forms (e.g., きった from きる, かった from かう)
        // Try different lengths for V2 inflected form (shortest match first)
        for (size_t v2_end = v2_start + 2; v2_end <= v2_hiragana_end; ++v2_end) {
          size_t v2_end_byte = byteOffsetAt(byte_offsets, v2_end);
          std::string v2_text(text.substr(v2_start_byte, v2_end_byte - v2_start_byte));

          // Use analyze() to get all candidates, not just the best one.
          // This is needed because for ambiguous stems (e.g., かった could be
          // from かる, かつ, or かう), we need to find the one matching our V2.
          const auto& infl_results = inflection.analyze(v2_text);
          std::string expected_base = std::string(v2_reading);

          for (const auto& infl_result : infl_results) {
            // Check if this matches the V2 base form (using reading for comparison)
            // Use 0.3 threshold for inflected forms since short stems get lower confidence
            // Require the suffix to contain actual auxiliary patterns (た/て/etc.),
            // not just renyokei endings (し/み/etc.) to ensure complete inflected form
            //
            // Verify verb type consistency: if V2 is godan, reject ichidan
            // inflection matches (and vice versa). This prevents e.g. いた
            // (ichidan いる ta-form) from falsely matching godan 入る(いる).
            if (infl_result.confidence >= 0.3F && infl_result.base_form == expected_base &&
                hasAuxiliarySuffix(infl_result.suffix) &&
                !(v2_verb.verb_type == V2VerbType::Godan && infl_result.verb_type == grammar::VerbType::Ichidan) &&
                !(v2_verb.verb_type == V2VerbType::Ichidan && infl_result.verb_type != grammar::VerbType::Ichidan)) {
              matched_inflected = true;
              matched_len = v2_end_byte - v2_start_byte;
              inflection_includes_aux = true;  // Mark that this match includes aux
              break;
            }
          }
          if (matched_inflected)
            break;
        }

        // Case 2: Kanji V2 inflected forms (e.g., 巡った from 巡る)
        // Check if text starts with V2 kanji prefix, then analyze hiragana suffix
        if (!matched_inflected && char_types[v2_start] == CharType::Kanji) {
          // Extract kanji prefix from V2 surface (e.g., "巡" from "巡る")
          auto v2_surface_decoded = normalize::utf8::decode(v2_surface);
          size_t kanji_prefix_len = 0;
          for (size_t idx = 0; idx < v2_surface_decoded.size(); ++idx) {
            char32_t c = v2_surface_decoded[idx];
            if (kana::isKanjiCodepoint(c)) {
              ++kanji_prefix_len;
            } else {
              break;
            }
          }

          if (kanji_prefix_len > 0 && kanji_prefix_len < v2_surface_decoded.size()) {
            // Check if text at v2_start matches the kanji prefix
            // (kanji prefixes here are all 3-byte CJK codepoints).
            size_t kanji_prefix_byte_len = kanji_prefix_len * core::kJapaneseCharBytes;

            if (v2_start_byte + kanji_prefix_byte_len <= text.size()) {
              std::string_view text_kanji_prefix = text.substr(v2_start_byte, kanji_prefix_byte_len);
              std::string v2_kanji_prefix = normalize::utf8::encode(
                  std::vector<char32_t>(v2_surface_decoded.begin(), v2_surface_decoded.begin() + kanji_prefix_len));

              if (text_kanji_prefix == v2_kanji_prefix) {
                // Find the hiragana suffix after the kanji prefix
                size_t hira_start = v2_start + kanji_prefix_len;
                if (hira_start < codepoints.size() && char_types[hira_start] == CharType::Hiragana) {
                  size_t hira_end = findCharRegionEnd(char_types, hira_start, 6, CharType::Hiragana);

                  // Try inflection on kanji+hiragana portion (shortest match first)
                  for (size_t v2_end = hira_start + 1; v2_end <= hira_end; ++v2_end) {
                    size_t v2_end_byte = byteOffsetAt(byte_offsets, v2_end);
                    std::string v2_text(text.substr(v2_start_byte, v2_end_byte - v2_start_byte));

                    // Use analyze() to search all candidates for matching base form
                    const auto& infl_results = inflection.analyze(v2_text);
                    for (const auto& infl_result : infl_results) {
                      // Check if base form matches V2 surface (kanji form)
                      // Require the suffix to contain actual auxiliary patterns
                      if (infl_result.confidence >= 0.35F && infl_result.base_form == v2_surface &&
                          hasAuxiliarySuffix(infl_result.suffix)) {
                        matched_inflected = true;
                        matched_len = v2_end_byte - v2_start_byte;
                        inflection_includes_aux = true;  // Mark that this match includes aux
                        break;
                      }
                    }
                    if (matched_inflected)
                      break;
                  }
                }
              }
            }
          }
        }
      }
    }

    // Case 3: V2 mizenkei form match before passive, causative, or negative auxiliaries.
    // E.g., 打ち込まれ, 取り込ませ, 見当たらない.
    bool matched_mizenkei = false;
    if (!matched_kanji && !matched_reading && !matched_renyokei && !matched_potential && !matched_kateikei &&
        !matched_volitional && !matched_inflected) {
      auto tryMizenMatch = [&](const std::string& mizen) -> bool {
        return beginsMizenkeiAuxiliary(text, v2_start_byte, mizen);
      };

      if (tryMizenMatch(kanji_mizen)) {
        matched_mizenkei = true;
        matched_len = kanji_mizen.size();
      } else if (tryMizenMatch(hira_mizen)) {
        matched_mizenkei = true;
        matched_len = hira_mizen.size();
      }
    }

    // Case 4: the V2 imperative, which is the same e-row surface the kateikei
    // is built on (刻み+込め, 書き+込め). Nothing follows it, and that is what
    // separates it from the competing readings of those characters: the
    // conditional needs its ば, and the potential's stem needs the auxiliary it
    // is a stem for. A predicate closing the sentence is the imperative, so the
    // compound keeps its boundary here as it does in every other cell.
    bool matched_imperative = false;
    if (!matched_kanji && !matched_reading && !matched_renyokei && !matched_potential && !matched_kateikei &&
        !matched_volitional && !matched_inflected && !matched_mizenkei && v2_verb.verb_type == V2VerbType::Godan) {
      auto tryImperative = [&](const std::string& imperative, bool via_reading) {
        if (matched_imperative || imperative.empty() || v2_start_byte + imperative.size() != text.size()) {
          return;
        }
        if (text.substr(v2_start_byte, imperative.size()) != imperative) {
          return;
        }
        matched_imperative = true;
        matched_len = imperative.size();
        matched_renyokei_via_reading = via_reading;
      };
      tryImperative(v2_verb.joins_surface ? generateKateikei(v2_surface, "", v2_verb.verb_type) : "", false);
      tryImperative(!v2_reading.empty() ? generateKateikei(v2_reading, "", v2_verb.verb_type) : "", true);
    }

    if (!matched_kanji && !matched_reading && !matched_renyokei && !matched_potential && !matched_kateikei &&
        !matched_volitional && !matched_inflected && !matched_mizenkei && !matched_imperative) {
      continue;
    }

    // Do not let a kanji V2 ending in 「く」consume the first mora of the
    // polite request auxiliary 「ください」.  In ご理解ください, for example,
    // 解く must not create the spurious compound candidate 理解く; the
    // remaining ださい is not a valid auxiliary boundary.  The check is
    // surface-independent and leaves real V2 compounds untouched.
    if (matched_len >= core::kJapaneseCharBytes && v2_start_byte + matched_len < text.size() &&
        text[v2_start_byte + matched_len - core::kJapaneseCharBytes] == '\xE3' &&
        text.substr(v2_start_byte + matched_len - core::kJapaneseCharBytes, core::kJapaneseCharBytes) == "く" &&
        utf8::startsWith(text.substr(v2_start_byte + matched_len), "ださい")) {
      continue;
    }

    SUZUME_DEBUG_LOG_VERBOSE("[COMPOUND] V2 matched: "
                             << v2_verb.surface << " kanji=" << matched_kanji << " reading=" << matched_reading
                             << " renyokei=" << matched_renyokei << " potential=" << matched_potential
                             << " kateikei=" << matched_kateikei << " inflected=" << matched_inflected
                             << " mizenkei=" << matched_mizenkei << " volitional=" << matched_volitional
                             << " imperative=" << matched_imperative << " len=" << matched_len << "\n");

    const CompoundV1Verification v1 = verifyCompoundVerbV1({
        text,
        codepoints,
        byte_offsets,
        start_pos,
        kanji_end,
        v2_start,
        start_byte,
        v2_start_byte,
        base_ending,
        is_sokuonbin,
        is_ichidan,
        has_kanji_v2_after_bare_ichidan,
        dict_compound_v1,
        hiragana_v1,
        allow_closed_onbin_v1,
        dict_compound_v1_lemma,
        dict_manager,
        inflection,
    });

    // Only generate compound verb candidates when V1 is a verified verb
    // This prevents false positives like 試験に落ちる (試験 is not a verb)
    if (!v1.verified) {
      continue;
    }

    // A numeral names a quantity and is never a verbal element, so it cannot be
    // the V1 of a compound verb.  The productive single-kanji V1 fallback is
    // dictionary-free and otherwise accepts one (三+切れる, 二+重ねる), stealing
    // the span from the quantity phrase it actually spells.
    if (isNumeralOnlySpan(codepoints, start_pos, v2_start)) {
      continue;
    }

    // A compound may begin inside a preceding kanji noun only when its V1 is
    // independently dictionary-verified (蛙+飛び込む, 報告+申し上げる).  An
    // inflection-only V1 in that position can instead fabricate a compound
    // across the noun/verb boundary (生+涯忘れる).
    const bool starts_inside_kanji_run = start_pos > 0 && normalize::isKanjiCodepoint(codepoints[start_pos - 1]);
    if (starts_inside_kanji_run && startsInsideRegisteredNoun(dict_manager, text, byte_offsets, start_pos)) {
      continue;
    }
    // A numeral plus its counter is a closed quantity phrase, so the kanji run
    // it ends does close before the predicate (3回|見直す, 5〜6回|繰り返す).
    // There the adjacency carries no evidence about the compound's own V1.
    const bool follows_counter = start_pos >= 2 && normalize::isCounterKanji(codepoints[start_pos - 1]) &&
                                 normalize::isNumeralCodepoint(codepoints[start_pos - 2]);
    // Okurigana binds to the kanji directly in front of it, so a V1 written as
    // one kanji plus its own okurigana has a stem boundary on its left by
    // spelling alone: nothing further back in the run can belong to it
    // (時間|考え込む). The fabrication the blanket rule guards against has the
    // opposite shape — the V1 there is bare kanji continuing the run, and the
    // okurigana it would need sits after the V2 (生|涯忘れる). Whether the run
    // in front plus this kanji spells a word of its own (達成|し続ける) is a
    // lexical question, and the registered-noun test above is the one that
    // answers it.
    const bool v1_carries_okurigana =
        kanji_end == start_pos + 1 && kanji_end < v2_start && normalize::isKanjiCodepoint(codepoints[start_pos]);
    if (starts_inside_kanji_run && !follows_counter && !v1_carries_okurigana && !v1.dict_verified &&
        !dict_compound_v1) {
      continue;
    }

    // A hiragana V2 opening with た/だ has the same surface as the past
    // auxiliary wherever the V1 form in front of it is one the auxiliary
    // selects: た after an Ichidan continuative or an い/っ onbin (食べ+た,
    // 書い+た, 買っ+た), だ after an い/ん onbin (読ん+だ, 泳い+だ).  Without
    // lexical attestation of the compound itself the past reading owns that
    // boundary.  Elsewhere the V2 stays available (食べ+だす), as does any
    // kanji-spelled V2 (見+立てる).
    if (!matched_kanji && !v1.dict_verified && !dict_compound_v1 && v2_start > start_pos &&
        char_types[v2_start] == CharType::Hiragana) {
      const char32_t v1_tail = codepoints[v2_start - 1];
      const bool past_ta = codepoints[v2_start] == U'た' && (is_ichidan || v1_tail == U'い' || v1_tail == U'っ');
      const bool past_da = codepoints[v2_start] == U'だ' && (v1_tail == U'い' || v1_tail == U'ん');
      if (past_ta || past_da) {
        continue;
      }
    }

    // A nominal base followed by an independently registered suffix form
    // (税+抜き, 水+抜き) is not evidence for a lexical compound verb.  The
    // productive single-kanji V1 fallback is deliberately dictionary-free,
    // so let the suffix analysis own this boundary unless V1 itself was
    // dictionary-verified.
    if (!v1.dict_verified && !dict_compound_v1 && !is_sokuonbin && kanji_end < codepoints.size()) {
      const std::string v2_form = extractSubstring(codepoints, v2_start, kanji_end + 1);
      constexpr PartOfSpeechMask kNominalSuffixMask =
          partOfSpeechMask(core::PartOfSpeech::Suffix) | partOfSpeechMask(core::PartOfSpeech::Noun);
      if (hasExactPartOfSpeech(dict_manager, v2_form, kNominalSuffixMask)) {
        continue;
      }
    }

    // For inflected V2 matches (Case 1/2), check if the full surface could be
    // an adjective instead of a compound verb. This prevents false positives
    // like 美しかった (adjective) being parsed as 美し+交った (compound verb).
    if (matched_inflected && inflection_includes_aux) {
      // Calculate full compound surface
      size_t compound_end_byte = v2_start_byte + matched_len;
      std::string full_surface(text.substr(start_byte, compound_end_byte - start_byte));

      // Check if full surface could be an i-adjective
      auto full_infl = inflection.getBest(full_surface);
      if (full_infl.confidence >= 0.5F && full_infl.verb_type == grammar::VerbType::IAdjective && !v1.dict_verified &&
          !dict_compound_v1) {
        // With no independently verified V1, the whole adjective analysis is
        // stronger evidence (美しかった). A dictionary-backed continuative V1
        // followed by an allowed V2 remains a productive compound even when
        // the inflection analyzer fabricates an i-adjective homograph from its
        // shared かった ending (差し掛かった).
        continue;
      }
    }

    // Build the compound base while preserving the V2 orthography supplied by
    // the input. A hiragana V2 is a deliberate spelling choice (読みかける),
    // not an instruction to normalize it to the table's kanji representative
    // (読み掛ける).
    std::string compound_base;
    size_t v1_renyokei_end = is_ichidan ? v2_start_byte : byteOffsetAt(byte_offsets, kanji_end + 1);
    compound_base = std::string(text.substr(start_byte, v1_renyokei_end - start_byte));
    std::string compound_source_base = compound_base;
    const bool v2_is_hiragana = char_types[v2_start] == CharType::Hiragana;
    if (matched_potential) {
      std::string potential = generateGodanPotential(v2_surface, "", v2_verb.verb_type);
      compound_base +=
          v2_is_hiragana && !v2_reading.empty() ? generateGodanPotential(v2_reading, "", v2_verb.verb_type) : potential;
      compound_source_base += v2_is_hiragana && !v2_reading.empty() ? v2_reading : v2_surface;
    } else {
      compound_base += v2_is_hiragana && !v2_reading.empty() ? v2_reading : v2_surface;
      compound_source_base = compound_base;
    }

    // Compare with best match and update if this is better
    // Priority:
    // 1. Longer renyokei match beats shorter (出し > 出)
    // 2. Renyokei exact match beats inflection match with aux
    // 3. Match without aux beats match with aux
    const bool current_compound_attested =
        isCompoundVerbOrNominalizationAttested(dict_manager, compound_base, v2_verb.verb_type);
    const bool best_compound_attested =
        best_match.v2_verb != nullptr &&
        isCompoundVerbOrNominalizationAttested(dict_manager, best_match.compound_base, best_match.v2_verb->verb_type);
    const size_t matched_end_pos =
        advanceCharsToBytePos(codepoints, v2_start, v2_start_byte, v2_start_byte + matched_len);
    const bool matched_causative_conditional =
        matched_mizenkei && matched_end_pos + 2 < codepoints.size() && codepoints[matched_end_pos] == U'せ' &&
        codepoints[matched_end_pos + 1] == U'れ' && codepoints[matched_end_pos + 2] == U'ば';
    const size_t best_mizenkei_end_pos =
        advanceCharsToBytePos(codepoints, v2_start, v2_start_byte, v2_start_byte + best_match.matched_len);
    const bool best_mizenkei_has_causative_conditional =
        best_match.is_mizenkei && best_mizenkei_end_pos + 2 < codepoints.size() &&
        codepoints[best_mizenkei_end_pos] == U'せ' && codepoints[best_mizenkei_end_pos + 1] == U'れ' &&
        codepoints[best_mizenkei_end_pos + 2] == U'ば';
    bool should_update = false;
    if (best_match.matched_len == 0) {
      // First valid match
      should_update = true;
    } else if (matched_causative_conditional && !best_match.is_mizenkei) {
      // A Godan compound mizenkei followed by the closed causative
      // conditional (V1+V2あ+せれ+ば) retains the auxiliary boundary.
      // The longer Ichidan compound overlap is available for its own
      // conditional, but it cannot erase this productive voice sequence.
      should_update = true;
    } else if (matched_volitional && !best_match.is_volitional) {
      should_update = true;
    } else if (best_match.is_volitional && !matched_volitional) {
      should_update = false;
    } else if (current_compound_attested && !best_compound_attested && !best_mizenkei_has_causative_conditional) {
      // An attested full compound (降りしきる) must not lose to a shorter
      // overlapping V2 continuative (敷く → しき). Both readings are
      // grammatically possible locally, but only the full compound has
      // lexical evidence.
      should_update = true;
    } else if (matched_renyokei && best_match.is_renyokei && matched_len > best_match.matched_len) {
      // Longer renyokei match beats shorter renyokei match
      // This makes 出し (6 bytes) beat 出 (3 bytes) for V1+V2 compounds
      should_update = true;
    } else if (matched_renyokei && best_match.is_mizenkei && matched_len > best_match.matched_len) {
      // A longer ichidan continuative can overlap with the mizenkei of a
      // different V2 (組み合わせ vs. 組み合わ).  A deverbal suffix or a
      // nominal-forcing particle immediately after it establishes the
      // nominalized compound reading.
      const size_t renyokei_end_pos =
          advanceCharsToBytePos(codepoints, v2_start, v2_start_byte, v2_start_byte + matched_len);
      const bool followed_by_deverbal_suffix =
          renyokei_end_pos < codepoints.size() &&
          (codepoints[renyokei_end_pos] == U'方' || codepoints[renyokei_end_pos] == U'手' ||
           codepoints[renyokei_end_pos] == U'物' || codepoints[renyokei_end_pos] == U'所' ||
           codepoints[renyokei_end_pos] == U'場');
      const bool followed_by_nominal_particle =
          beginsNominalForcingParticle(codepoints, renyokei_end_pos, dict_manager);
      const bool followed_by_ichidan_conditional =
          v2_verb.verb_type == V2VerbType::Ichidan && renyokei_end_pos + 1 < codepoints.size() &&
          codepoints[renyokei_end_pos] == U'れ' && codepoints[renyokei_end_pos + 1] == U'ば';
      if (!best_mizenkei_has_causative_conditional &&
          (followed_by_deverbal_suffix || followed_by_nominal_particle ||
           (followed_by_ichidan_conditional && current_compound_attested))) {
        should_update = true;
      }
    } else if (is_renyokei_entry && (matched_kanji || matched_reading) && best_match.includes_aux &&
               !best_match.is_renyokei) {
      // Renyokei exact match beats inflection match that includes aux
      // This makes 食べすぎ (renyokei) beat 食べすぎた (inflection+aux)
      should_update = true;
    } else if (inflection_includes_aux && best_match.includes_aux && matched_len > best_match.matched_len) {
      // Competing closed V2 readings can share an onbin prefix (たつ vs
      // たたむ in 折りたたんで).  When both consume an inflectional tail, the
      // longer complete V2 is the structurally stronger analysis.
      should_update = true;
    } else if (!inflection_includes_aux && best_match.includes_aux) {
      // Usually a lexical match without auxiliaries beats a candidate that
      // absorbed an auxiliary. The 合う+使役せる / 合わせる overlap is the
      // exception: preserve an attested V1+合う causative unless the competing
      // 合わせる compound (or its nominalized 連用形) is itself attested.
      should_update = !best_compound_attested || current_compound_attested;
    } else if ((matched_kanji || matched_reading) && best_match.is_potential) {
      // A lexical V2 base form (続ける) takes precedence over an overlapping
      // potential form generated from a different Godan V2 (続く→続ける).
      should_update = true;
    } else if ((matched_kanji || matched_reading || matched_renyokei) && best_match.is_imperative) {
      // Any lexical V2 match outranks an imperative generated off a different
      // Godan V2 that happens to spell the same characters: つけ is the
      // continuative of the listed つける before it is the imperative of つく.
      should_update = true;
    } else if ((matched_kanji || matched_reading) && !inflection_includes_aux && !best_match.includes_aux &&
               !best_match.is_renyokei && !best_match.renyokei_form && !best_match.is_mizenkei &&
               !best_match.is_potential && !best_match.is_kateikei && !best_match.is_volitional &&
               !best_match.is_imperative && matched_len > best_match.matched_len) {
      // Two members of the closed V2 class can both spell a base form here, one
      // a prefix of the other (the つく of 付く inside the つくす of 尽くす).
      // Neither carries more evidence than the other, so the ordinary
      // longest-match rule settles it: the longer reading accounts for kana the
      // shorter one has to hand to a separate token (立ち|つくす, not 立ちつく|す).
      should_update = true;
    } else if (best_match.is_mizenkei && (matched_kanji || matched_reading)) {
      // A full V2 base-form match (組み合わせる via ichidan 合わせる) competes
      // with a shorter V2-mizenkei causative/passive reading of another table
      // entry (組み合わ + せる via godan 合う). Prefer the complete, longer
      // member of the closed V2 class. This is a consistent ambiguity policy
      // for arbitrary V1 hosts and does not require registering each compound.
      should_update = matched_len > best_match.matched_len;
    }

    if (should_update) {
      best_match.matched_len = matched_len;
      best_match.compound_base = compound_base;
      best_match.compound_source_base = compound_source_base;
      best_match.is_renyokei = is_renyokei_entry && (matched_kanji || matched_reading);
      best_match.renyokei_form = matched_renyokei;
      best_match.is_mizenkei = matched_mizenkei;
      best_match.is_volitional = matched_volitional;
      best_match.is_kateikei = matched_kateikei;
      best_match.is_imperative = matched_imperative;
      best_match.is_potential = matched_potential;
      best_match.includes_aux = inflection_includes_aux;
      best_match.matched_via_reading = matched_reading || matched_inflected || matched_renyokei_via_reading;
      best_match.v2_verb = &v2_verb;
      best_match.v1_dict_verified = v1.dict_verified;
      best_match.v1_embedded_verified = v1.embedded_verified;
      best_match.v1_ichidan_inflection = v1.ichidan_inflection;
      best_match.v1_bare_ichidan = has_kanji_v2_after_bare_ichidan && v1.ichidan_inflection;
      best_match.v1_godan_inflection = v1.godan_inflection;
    }
  }

  SUZUME_DEBUG_LOG_VERBOSE("[COMPOUND] best_match.len=" << best_match.matched_len
                                                        << " base=" << best_match.compound_base << "\n");

  // A closed V2 reading must not override a dictionary-verified inflected
  // verb that consumes the same tail with a different lemma.  This resolves
  // arbitrary lexical tails from the existing dictionary (for example an
  // n-onbin Godan-ma form) without copying open-class verbs into the closed
  // compound-V2 table.
  if (!hiragana_v1 && best_match.v2_verb != nullptr && best_match.matched_via_reading && best_match.matched_len > 0) {
    const size_t matched_chars = normalize::utf8Length(text.substr(v2_start_byte, best_match.matched_len));
    const std::string_view matched_v2_base =
        best_match.v2_verb->reading != nullptr ? best_match.v2_verb->reading : best_match.v2_verb->surface;
    for (const auto& result : dict_manager.lookup(text, v2_start_byte)) {
      if (result.entry != nullptr && result.length == matched_chars && result.entry->pos == core::PartOfSpeech::Verb &&
          result.entry->lemma != matched_v2_base) {
        return {};
      }
    }
  }

  // Nor may it override a listed particle covering the identical span. A
  // subsidiary verb spelled in kana where its own lemma carries kanji is the
  // marked orthography, so it cannot outrank the unmarked closed-class reading
  // of those same kana (暮らし+より, not 暮らし寄り). The kanji spelling still
  // forms the compound freely (立ち寄り), which is exactly the evidence the
  // kana spelling lacks.
  if (best_match.v2_verb != nullptr && best_match.matched_via_reading && best_match.matched_len > 0 &&
      best_match.v2_verb->reading != nullptr &&
      std::string_view(best_match.v2_verb->surface) != best_match.v2_verb->reading &&
      dict_manager.lookupExact(text.substr(v2_start_byte, best_match.matched_len), core::PartOfSpeech::Particle) !=
          nullptr) {
    return {};
  }

  return best_match;
}

}  // namespace suzume::analysis::compound_verb_detail
