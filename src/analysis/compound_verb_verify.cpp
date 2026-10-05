/**
 * @file compound_verb_verify.cpp
 * @brief V1 reconstruction and verification for compound verbs
 */
#include "analysis/dictionary_probe.h"
#include "join_compound_verb_internal.h"

namespace suzume::analysis::compound_verb_detail {

namespace {

// Whether a dictionary particle spells the tail of @p v1_renyokei from some
// kana boundary at or after @p first_split bytes: a particle there is a
// compositional boundary, not part of the V1.
bool endsInDictionaryParticle(const dictionary::DictionaryManager& dict_manager, std::string_view v1_renyokei,
                              size_t first_split) {
  for (size_t split = first_split; split < v1_renyokei.size(); split += core::kJapaneseCharBytes) {
    if (dict_manager.lookupExact(v1_renyokei.substr(split), core::PartOfSpeech::Particle) != nullptr) {
      return true;
    }
  }
  return false;
}

}  // namespace

std::string hiraganaV1Base(std::string_view v1_surface, bool is_ichidan, char32_t base_ending) {
  if (grammar::isSuruRenyokeiSurface(v1_surface)) {
    return "する";
  }
  if (is_ichidan) {
    return normalize::concat(v1_surface, "る");
  }
  return normalize::concat(v1_surface.substr(0, v1_surface.size() - core::kJapaneseCharBytes),
                           normalize::encodeUtf8(base_ending));
}

bool hasInflectionCandidateForBase(const grammar::Inflection& inflection, std::string_view surface,
                                   std::string_view base_form, float min_confidence) {
  for (const auto& candidate : inflection.analyze(surface)) {
    if (candidate.confidence >= min_confidence && candidate.base_form == base_form) {
      return true;
    }
  }
  return false;
}

CompoundV1Verification verifyCompoundVerbV1(const CompoundV1VerificationRequest& request) {
  const std::string_view text = request.text;
  const auto& codepoints = request.codepoints;
  const auto& byte_offsets = request.byte_offsets;
  const size_t start_pos = request.start_pos;
  const size_t kanji_end = request.kanji_end;
  const size_t v2_start = request.v2_start;
  const size_t start_byte = request.start_byte;
  const size_t v2_start_byte = request.v2_start_byte;
  const char32_t base_ending = request.base_ending;
  const bool is_sokuonbin = request.is_sokuonbin;
  const bool is_ichidan = request.is_ichidan;
  const bool has_kanji_v2_after_bare_ichidan = request.has_kanji_v2_after_bare_ichidan;
  const bool dict_compound_v1 = request.dict_compound_v1;
  const bool hiragana_v1 = request.hiragana_v1;
  const bool allow_closed_onbin_v1 = request.allow_closed_onbin_v1;
  const std::string_view dict_compound_v1_lemma = request.dict_compound_v1_lemma;
  const auto& dict_manager = request.dict_manager;
  const auto& inflection = request.inflection;

  CompoundV1Verification result;
  std::string v1_base;
  bool& v1_verified = result.verified;
  bool& v1_dict_verified = result.dict_verified;
  bool& v1_embedded_verified = result.embedded_verified;
  bool& v1_ichidan_inflection = result.ichidan_inflection;
  bool& v1_godan_inflection = result.godan_inflection;

  if (hiragana_v1) {
    const std::string_view v1_surface = text.substr(start_byte, v2_start_byte - start_byte);
    v1_base = hiraganaV1Base(v1_surface, is_ichidan, base_ending);

    const auto* verb_entry = dict_manager.lookupExact(v1_base, core::PartOfSpeech::Verb);
    v1_verified = verb_entry != nullptr || grammar::isSuruRenyokeiSurface(v1_surface);
    v1_dict_verified = v1_verified;

    const auto* auxiliary_entry = dict_manager.lookupExact(v1_base, core::PartOfSpeech::Auxiliary);
    if (auxiliary_entry != nullptr && auxiliary_entry->extended_pos == core::ExtendedPOS::AuxHonorific) {
      return {};
    }

    if (!v1_verified) {
      const float min_confidence =
          allow_closed_onbin_v1
              ? candidate::verb_cost::kClosedOnbinCompoundV1MinConfidence
              : (is_ichidan && v2_start < codepoints.size() && normalize::isKanjiCodepoint(codepoints[v2_start])
                     ? candidate::verb_cost::kCompoundVerbIchidanMinConfidence
                     : candidate::verb_cost::kConstructedVerbMinConfidence);
      if (!verb_helpers::hasNonVerbDictionaryEntry(&dict_manager, v1_surface) &&
          hasInflectionCandidateForBase(inflection, v1_surface, v1_base, min_confidence)) {
        v1_verified = true;
        v1_ichidan_inflection = is_ichidan;
        v1_godan_inflection = !is_ichidan;
      }
    }
    return result;
  }

  // Build the V1 base form for verification.
  const size_t v1_end_byte = is_ichidan ? v2_start_byte : byteOffsetAt(byte_offsets, kanji_end);
  v1_verified = dict_compound_v1;
  v1_dict_verified = dict_compound_v1;
  if (dict_compound_v1) {
    // Already resolved: V1 is the dict-verified compound verb (引きずる).
    v1_base = dict_compound_v1_lemma;
  } else {
    v1_base = std::string(text.substr(start_byte, v1_end_byte - start_byte));

    if (!is_sokuonbin && !is_ichidan) {
      v1_base += normalize::encodeUtf8(base_ending);
    } else if (!is_sokuonbin) {
      v1_base += "る";
    }

    if (is_sokuonbin) {
      // Try all sokuonbin-compatible godan endings.
      for (char32_t ending : kSokuonbinEndings) {
        std::string candidate = v1_base + normalize::encodeUtf8(ending);
        if (dict_manager.lookupExact(candidate, core::PartOfSpeech::Verb) != nullptr) {
          v1_verified = true;
          v1_dict_verified = true;
          v1_base = candidate;
          break;
        }
      }
    } else if (dict_manager.lookupExact(v1_base, core::PartOfSpeech::Verb) != nullptr) {
      v1_verified = true;
      v1_dict_verified = true;
    }
  }

  // A kana span of two or more morae that is a registered particle is a phrase
  // boundary (予定+より+遅れる), not okurigana; a one-mora span stays open to
  // the continuative (立て+直す).
  const size_t kanji_end_byte = byteOffsetAt(byte_offsets, kanji_end);
  const bool kana_span_is_particle =
      v2_start >= kanji_end + 2 && dict_manager.lookupExact(text.substr(kanji_end_byte, v2_start_byte - kanji_end_byte),
                                                            core::PartOfSpeech::Particle) != nullptr;

  // A kanji-led V1 can have more than one kana before its continuative
  // ending (混じり+合う). The first kana may look like an Ichidan stem, but
  // the complete span can instead prove a Godan continuative. Preserve the
  // Ichidan reading unless inflection recognizes the whole span as Godan
  // and its final kana is that row's continuative form.
  if (!v1_verified && !dict_compound_v1 && is_ichidan && v2_start > kanji_end + 1 && !kana_span_is_particle) {
    const std::string v1_renyokei(text.substr(start_byte, v2_start_byte - start_byte));
    // Weigh every reading rather than the top-scoring one alone: the Godan
    // continuative ending い is also the i-adjective ending, so a span such as
    // 向かい is reported as an adjective first and the continuative that
    // proves the compound sits behind it.
    for (const auto& inflection_candidate : inflection.analyze(v1_renyokei)) {
      const auto* godan_row = grammar::Conjugation::getGodanRow(inflection_candidate.verb_type);
      if (godan_row != nullptr &&
          inflection_candidate.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence &&
          codepoints[v2_start - 1] == godan_row->i_row) {
        v1_base = inflection_candidate.base_form;
        v1_verified = true;
        v1_godan_inflection = true;
        break;
      }
    }
  }

  SUZUME_DEBUG_LOG_VERBOSE("[COMPOUND] V1 base=" << v1_base << " verified=" << v1_verified
                                                 << " sokuonbin=" << is_sokuonbin << "\n");

  // Fallback: use inflection analysis for unknown V1 verbs.
  if (!v1_verified) {
    const size_t kanji_count = has_kanji_v2_after_bare_ichidan ? 1 : kanji_end - start_pos;

    // A single-kanji sokuonbin V1 plus a verified V2 is sufficient evidence.
    if (is_sokuonbin && kanji_count == 1) {
      v1_verified = true;
      for (char32_t ending : kSokuonbinEndings) {
        std::string candidate = v1_base + normalize::encodeUtf8(ending);
        if (dict_manager.lookupExact(candidate) != nullptr) {
          v1_base = candidate;
          break;
        }
      }
    }

    // For multi-kanji ichidan V1 stems, accept when stripping the leading
    // kanji yields a dictionary verb (e.g., 仕立てる = 仕 + 立てる).
    if (!v1_verified && is_ichidan && kanji_count >= 2) {
      const size_t v1_second_char_byte = byteOffsetAt(byte_offsets, start_pos + 1);
      std::string embedded_base(text.substr(v1_second_char_byte, v1_end_byte - v1_second_char_byte));
      embedded_base += "る";
      if (dict_manager.lookupExact(embedded_base, core::PartOfSpeech::Verb) != nullptr) {
        v1_verified = true;
        v1_embedded_verified = true;
        SUZUME_DEBUG_LOG_VERBOSE("[COMPOUND] V1 verified via embedded dict verb \"" << embedded_base << "\"\n");
      }
    }

    bool use_inflection_fallback = !v1_verified;
    const size_t v1_renyokei_end = is_ichidan ? v2_start_byte : byteOffsetAt(byte_offsets, kanji_end + 1);
    const std::string_view v1_renyokei = text.substr(start_byte, v1_renyokei_end - start_byte);

    // Multi-kanji stems require direct dictionary evidence. The inflection
    // analyzer accepts long kanji sequences too freely for this boundary.
    if (use_inflection_fallback && kanji_count >= 2) {
      use_inflection_fallback = false;
    }

    // Single-kanji + に is normally a noun-plus-particle boundary, not a
    // Godan-Na continuative.
    const char32_t renyokei_char = codepoints[kanji_end];
    if (!is_ichidan && kanji_count == 1 && renyokei_char == U'に') {
      use_inflection_fallback = false;
    }

    // A known non-verb continuative blocks fallback unless an exact
    // single-kanji Godan analysis proves the productive V1.
    if (use_inflection_fallback) {
      if (verb_helpers::hasNonVerbDictionaryEntry(&dict_manager, v1_renyokei)) {
        const auto inflection_candidate = inflection.getBest(v1_renyokei);
        const bool is_complete_dictionary_adjective =
            dict_manager.lookupExact(v1_renyokei, core::PartOfSpeech::Adjective) != nullptr;
        const bool productive_godan_compound =
            !is_complete_dictionary_adjective && !is_ichidan && kanji_count == 1 &&
            inflection_candidate.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence &&
            inflection_candidate.base_form == v1_base;
        if (!productive_godan_compound) {
          use_inflection_fallback = false;
        }
      }
    }

    // An irrealis plus the passive or causative auxiliary (書か+さ+れ, 書か+せ)
    // is a voice chain, not a lexical V1: the auxiliary never heads a compound,
    // so the V2 behind it is a subsidiary of its own (書か+さ+れ+かけ+た).
    if (use_inflection_fallback && is_ichidan && kanji_count == 1 && v2_start >= kanji_end + 2) {
      const char32_t voice = codepoints[v2_start - 1];
      const bool shortened_causative = v2_start == kanji_end + 3 && codepoints[kanji_end + 1] == U'さ';
      if (kana::isARowCodepoint(codepoints[kanji_end]) && (voice == U'れ' || voice == U'せ') &&
          (v2_start == kanji_end + 2 || shortened_causative)) {
        use_inflection_fallback = false;
      }
    }

    // A particle inside the proposed V1 is a compositional boundary. Check it
    // before accepting the productive single-kanji Ichidan fallback: that
    // otherwise treats adjectival adverbs such as 静かに+続く as compounds.
    if (use_inflection_fallback && is_ichidan && kanji_count == 1 &&
        (kana_span_is_particle || endsInDictionaryParticle(dict_manager, v1_renyokei, core::kTwoJapaneseCharBytes))) {
      use_inflection_fallback = false;
    }

    // The continuative of a coined V1 must not be the stem of a registered
    // i-adjective whose terminal い is the very next character. That い closes
    // the adjective, so handing it to a V2 that happens to begin with the same
    // kana fabricates a verb out of an adjective stem (忙しい|って, not
    // 忙しいっ|て with the non-word lemma 忙しいる). The whole -しい class sits
    // on this boundary in front of the quotative contraction, and the evidence
    // is the dictionary entry rather than an analysis of the coined form.
    if (use_inflection_fallback && v2_start < codepoints.size() && codepoints[v2_start] == U'い') {
      std::string adjective_terminal(text.substr(start_byte, v2_start_byte - start_byte));
      adjective_terminal += "い";
      if (dict_manager.lookupExact(adjective_terminal, core::PartOfSpeech::Adjective) != nullptr) {
        use_inflection_fallback = false;
      }
    }

    // A single-kanji Ichidan stem followed by a verified V2 is productive,
    // except at known copular, hatsuonbin, and formal-noun boundaries.
    bool starts_inside_formal_noun = false;
    if (use_inflection_fallback && is_ichidan && kanji_count == 1 && start_pos > 0) {
      const auto* enclosing_entry =
          lookupEntryInRange(dict_manager, codepoints, start_pos - 1, start_pos + 1, core::PartOfSpeech::Noun);
      starts_inside_formal_noun =
          enclosing_entry != nullptr && enclosing_entry->extended_pos == core::ExtendedPOS::NounFormal;
    }
    if (use_inflection_fallback && is_ichidan && kanji_count == 1) {
      const bool bare_ichidan_stem = v2_start == kanji_end;
      const bool bound_verb_prefix =
          bare_ichidan_stem && grammar::isBoundVerbPrefix(extractSubstring(codepoints, start_pos, start_pos + 1));
      if (bare_ichidan_stem && !verb_helpers::isSingleKanjiIchidan(codepoints[start_pos]) && !bound_verb_prefix) {
        use_inflection_fallback = false;
      } else if (renyokei_char == U'で' || (renyokei_char == U'ん' && v2_start == kanji_end) ||
                 starts_inside_formal_noun) {
        use_inflection_fallback = false;
      } else {
        v1_verified = true;
        v1_embedded_verified = bound_verb_prefix;
        v1_ichidan_inflection = !bound_verb_prefix;
        use_inflection_fallback = false;
      }
    }

    // A single-kanji Godan renyokei must reconstruct exactly to its V1 base.
    if (use_inflection_fallback && !is_ichidan && kanji_count == 1) {
      if (hasInflectionCandidateForBase(inflection, v1_renyokei, v1_base,
                                        candidate::verb_cost::kConstructedVerbMinConfidence)) {
        v1_verified = true;
        v1_godan_inflection = true;
        use_inflection_fallback = false;
      }
    }

    if (use_inflection_fallback && endsInDictionaryParticle(dict_manager, v1_renyokei, core::kJapaneseCharBytes)) {
      use_inflection_fallback = false;
    }

    if (use_inflection_fallback) {
      const auto infl_result = inflection.getBest(v1_renyokei);
      const float min_confidence = is_ichidan ? candidate::verb_cost::kCompoundVerbIchidanMinConfidence
                                              : candidate::verb_cost::kConstructedVerbMinConfidence;
      if (infl_result.confidence >= min_confidence) {
        if (infl_result.base_form == v1_base) {
          v1_verified = true;
          if (is_ichidan && kanji_count == 1) {
            v1_ichidan_inflection = true;
          }
        } else if (is_sokuonbin) {
          for (char32_t ending : kSokuonbinEndings) {
            std::string candidate = v1_base + normalize::encodeUtf8(ending);
            if (infl_result.base_form == candidate) {
              v1_verified = true;
              v1_base = candidate;
              break;
            }
          }
        }
      }
    }
  }

  // A ka/ga-row verb's i-onbin followed by で is its conjunctive te-form,
  // not a compound whose V2 happens to be the verb でる (急い+で+も).
  if (v1_verified && v2_start < codepoints.size() && v2_start > start_pos && codepoints[v2_start] == U'で' &&
      codepoints[v2_start - 1] == U'い' && utf8::endsWithAny(v1_base, {"く", "ぐ"})) {
    v1_verified = false;
    v1_dict_verified = false;
    v1_embedded_verified = false;
    v1_ichidan_inflection = false;
    v1_godan_inflection = false;
  }

  return result;
}

}  // namespace suzume::analysis::compound_verb_detail
