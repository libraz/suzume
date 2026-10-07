/**
 * @file compound_verb_v2_cells.cpp
 * @brief Which conjugation cell of a compound-verb V2 spells the text
 *
 * matchV2Form tries one V2 entry's cells in a fixed order (base form,
 * continuative, potential, conditional, volitional, inflected and irrealis
 * forms, imperative, classical terminal) and records the first that spells the
 * text at v2_start. Choosing between V2 entries is findCompoundVerbMatch's job.
 */
#include "analysis/dictionary_probe.h"
#include "analysis/verb_candidates_helpers.h"
#include "compound_verb_v2_cells_internal.h"
#include "grammar/char_patterns.h"

namespace suzume::analysis::compound_verb_detail {

namespace {

// The Ichidan imperative has two endings, the colloquial ろ and the literary
// よ, and both attach to the bare stem.
constexpr std::array<const char*, 2> kIchidanImperativeEndings = {"ろ", "よ"};

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

// Whether the non-empty @p form is spelled in @p text starting at @p start_byte.
bool spellsAt(std::string_view text, size_t start_byte, std::string_view form) {
  return !form.empty() && utf8::startsWith(text.substr(start_byte), form);
}

// The kanji base form, then its hiragana reading. Returns false where the
// reading is a contracted auxiliary rather than this V2.
bool matchV2BaseForm(const CompoundMatchContext& ctx, V2FormMatch& form) {
  const std::string_view text = ctx.text;
  const size_t v2_start_byte = ctx.v2_start_byte;
  const std::string_view v2_reading = form.v2_reading;
  bool& matched_kanji = form.matched_kanji;

  // Check if text at v2_start matches this V2 verb (kanji or reading)
  // Try kanji match first
  if (form.v2_verb.joins_surface && spellsAt(text, v2_start_byte, form.v2_surface)) {
    matched_kanji = true;
    form.matched_len = form.v2_surface.size();
  }

  // The hiragana spellings とる and どる are contracted progressive
  // auxiliaries after a verb stem. Keep the lexical V2 entries available
  // in their kanji spelling (受け取る), but do not build a false compound
  // candidate over the productive auxiliary sequence (食べとった).
  if (!matched_kanji && ctx.char_types[ctx.v2_start] == CharType::Hiragana &&
      (v2_reading == "とる" || v2_reading == "どる")) {
    const std::string full_compound = normalize::concat(ctx.v1_surface, v2_reading);
    if (ctx.dict_manager.lookupExact(full_compound, core::PartOfSpeech::Verb) == nullptr) {
      return false;
    }
  }

  // Hiragana そう before conditional/copular な is the appearance
  // auxiliary (起こり+そう+なら), not the lexical compound V2 添う.
  // Kanji-written 添う remains available as an ordinary compound verb.
  if (v2_reading == "そう" && ctx.char_types[ctx.v2_start] == CharType::Hiragana &&
      utf8::startsWith(text.substr(v2_start_byte), "そうな")) {
    return false;
  }

  // Try reading (hiragana) match if kanji didn't match
  if (!matched_kanji && spellsAt(text, v2_start_byte, v2_reading)) {
    form.matched_reading = true;
    form.matched_len = v2_reading.size();
  }
  return true;
}

// Returns false where the hiragana continuative で is a te-form boundary.
bool matchV2Renyokei(const CompoundMatchContext& ctx, V2FormMatch& form) {
  const auto& v2_verb = form.v2_verb;
  bool& matched_renyokei = form.matched_renyokei;

  // Try a V2 renyokei match so a following auxiliary stays separate.
  // e.g., 申し上げます → 申し上げ + ます (match V2 renyokei "上げ", not full "上げます")
  if (!form.matched_kanji && !form.matched_reading) {
    // Generate V2 renyokei
    std::string kanji_renyokei =
        v2_verb.joins_surface ? generateKanjiRenyokei(form.v2_surface, form.v2_reading, v2_verb.verb_type) : "";
    std::string hira_renyokei = generateRenyokei(form.v2_reading, "", v2_verb.verb_type);

    // Try kanji renyokei match
    if (spellsAt(ctx.text, ctx.v2_start_byte, kanji_renyokei)) {
      matched_renyokei = true;
      form.matched_len = kanji_renyokei.size();
      form.is_renyokei_entry = true;
    }

    // Try hiragana renyokei match if kanji didn't match
    if (!matched_renyokei && spellsAt(ctx.text, ctx.v2_start_byte, hira_renyokei)) {
      // Ichidan verbs take て, never で, so an Ichidan V1 before the 出る
      // renyokei で is a te-form boundary (付け+で), unlike Godan 飛び出る.
      if (ctx.is_ichidan && hira_renyokei == "で") {
        return false;
      }
      matched_renyokei = true;
      form.matched_renyokei_via_reading = true;
      form.matched_len = hira_renyokei.size();
      form.is_renyokei_entry = true;
    }
  }
  return true;
}

void matchV2Potential(const CompoundMatchContext& ctx, V2FormMatch& form) {
  const std::string_view text = ctx.text;
  const size_t v2_start_byte = ctx.v2_start_byte;
  const auto& v2_verb = form.v2_verb;
  bool& matched_potential = form.matched_potential;
  size_t& matched_len = form.matched_len;

  // A Godan potential form belongs to the same search unit as its compound
  // base: 取り + 戻せる → 取り戻せる. Generate it from every allowlisted
  // V2 rather than adding per-verb potential entries.
  if (!form.matched_kanji && !form.matched_reading && !form.matched_renyokei) {
    std::string kanji_potential =
        v2_verb.joins_surface ? generateGodanPotential(form.v2_surface, "", v2_verb.verb_type) : "";
    std::string hira_potential = generateGodanPotential(form.v2_reading, "", v2_verb.verb_type);
    if (spellsAt(text, v2_start_byte, kanji_potential)) {
      matched_potential = true;
      matched_len = kanji_potential.size();
    } else if (spellsAt(text, v2_start_byte, hira_potential)) {
      matched_potential = true;
      matched_len = hira_potential.size();
      form.matched_renyokei_via_reading = true;
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
        form.matched_renyokei_via_reading = via_reading;
      }
    };
    tryPotentialStem(kanji_potential, false);
    tryPotentialStem(hira_potential, true);
  }
}

void matchV2Kateikei(const CompoundMatchContext& ctx, V2FormMatch& form) {
  const std::string_view text = ctx.text;
  const size_t v2_start_byte = ctx.v2_start_byte;
  const auto& v2_verb = form.v2_verb;

  // A Godan V2 forms its conditional from the e-row stem plus ば
  // (踏み外せ+ば, 行き違え+ば).  The e-row surface is also the stem of a
  // potential verb, so require the following ば before treating it as
  // kateikei; unrestricted matching would incorrectly absorb an independent
  // potential predicate.
  if (!form.matched_kanji && !form.matched_reading && !form.matched_renyokei && !form.matched_potential &&
      v2_verb.verb_type == V2VerbType::Godan) {
    const std::string kanji_kateikei =
        v2_verb.joins_surface ? generateKateikei(form.v2_surface, "", v2_verb.verb_type) : "";
    const std::string hira_kateikei =
        !form.v2_reading.empty() ? generateKateikei(form.v2_reading, "", v2_verb.verb_type) : "";
    auto tryKateikei = [&](const std::string& kateikei, bool via_reading) {
      if (form.matched_kateikei || kateikei.empty() ||
          v2_start_byte + kateikei.size() + core::kJapaneseCharBytes > text.size()) {
        return;
      }
      const size_t after_kateikei = v2_start_byte + kateikei.size();
      if (text.substr(v2_start_byte, kateikei.size()) == kateikei &&
          text.substr(after_kateikei, core::kJapaneseCharBytes) == "ば") {
        form.matched_kateikei = true;
        form.matched_len = kateikei.size();
        form.matched_renyokei_via_reading = via_reading;
      }
    };
    tryKateikei(kanji_kateikei, false);
    tryKateikei(hira_kateikei, true);
  }
}

void matchV2Volitional(const CompoundMatchContext& ctx, V2FormMatch& form) {
  const std::string_view text = ctx.text;
  const size_t v2_start = ctx.v2_start;
  const size_t v2_start_byte = ctx.v2_start_byte;
  const auto& v2_verb = form.v2_verb;
  bool& matched_volitional = form.matched_volitional;

  // A V2 exposes its volitional stem before the closed auxiliary う — the
  // o-row for a Godan verb (考え出そ+う, 取り戻そ+う), the bare stem plus よ for
  // an Ichidan one (呼び続けよ+う). Match the stem even when a shorter V2
  // renyokei is homographic with its prefix (出る→出, and every Ichidan stem),
  // because the following auxiliary supplies decisive inflectional evidence.
  {
    const std::string kanji_volitional =
        v2_verb.joins_surface ? generateVolitionalStem(form.v2_surface, "", v2_verb.verb_type) : "";
    const std::string hira_volitional =
        !form.v2_reading.empty() ? generateVolitionalStem(form.v2_reading, "", v2_verb.verb_type) : "";
    auto tryVolitional = [&](const std::string& stem, bool via_reading) {
      if (matched_volitional || stem.empty() || v2_start_byte + stem.size() + core::kJapaneseCharBytes > text.size()) {
        return;
      }
      if (text.substr(v2_start_byte, stem.size()) != stem ||
          text.substr(v2_start_byte + stem.size(), core::kJapaneseCharBytes) != "う") {
        return;
      }
      if (!ctx.hiragana_v1) {
        const size_t volitional_end = v2_start + normalize::utf8Length(stem) + 1;
        for (size_t split_pos = v2_start + 1; split_pos < volitional_end; ++split_pos) {
          const auto* left_auxiliary =
              lookupEntryInRange(ctx.dict_manager, ctx.codepoints, v2_start, split_pos, core::PartOfSpeech::Auxiliary);
          const auto* right_auxiliary = lookupEntryInRange(ctx.dict_manager, ctx.codepoints, split_pos, volitional_end,
                                                           core::PartOfSpeech::Auxiliary);
          if (left_auxiliary != nullptr && right_auxiliary != nullptr &&
              right_auxiliary->extended_pos == core::ExtendedPOS::AuxAppearanceSou) {
            return;
          }
        }
      }
      matched_volitional = true;
      form.matched_len = stem.size();
      form.matched_renyokei_via_reading = via_reading;
    };
    tryVolitional(kanji_volitional, false);
    tryVolitional(hira_volitional, true);
    if (matched_volitional) {
      form.matched_renyokei = false;
      form.is_renyokei_entry = false;
    }
  }
}

// Cases 1 and 2: an inflected V2 whose suffix carries an actual auxiliary.
void matchV2InflectedForm(const CompoundMatchContext& ctx, V2FormMatch& form) {
  const std::string_view text = ctx.text;
  const auto& char_types = ctx.char_types;
  const size_t v2_start = ctx.v2_start;
  const size_t v2_start_byte = ctx.v2_start_byte;
  const auto& v2_verb = form.v2_verb;
  const std::string_view v2_surface = form.v2_surface;
  bool& matched_inflected = form.matched_inflected;

  std::string_view base_ending(v2_verb.base_ending);
  // Only try inflection for base forms (ending in る/す/く/う/む/つ/ぶ/ぐ/ぬ or ichidan endings)
  if (utf8::equalsAny(base_ending, {"る", "す", "く", "う", "む", "つ", "ぶ", "ぐ", "ぬ", "める", "ける", "れる",
                                    "える", "げる", "てる", "せる", "ちる"})) {
    // Case 1: Hiragana V2 inflected forms (e.g., きった from きる, かった from かう)
    // Try different lengths for V2 inflected form (shortest match first)
    for (size_t v2_end = v2_start + 2; v2_end <= ctx.v2_hiragana_end && !matched_inflected; ++v2_end) {
      size_t v2_end_byte = byteOffsetAt(ctx.byte_offsets, v2_end);
      const std::string_view v2_text = text.substr(v2_start_byte, v2_end_byte - v2_start_byte);

      // Use analyze() to get all candidates, not just the best one.
      // This is needed because for ambiguous stems (e.g., かった could be
      // from かる, かつ, or かう), we need to find the one matching our V2.
      for (const auto& infl_result : ctx.inflection.analyze(v2_text)) {
        // Check if this matches the V2 base form (using reading for comparison)
        // Use 0.3 threshold for inflected forms since short stems get lower confidence
        // Require the suffix to contain actual auxiliary patterns (た/て/etc.),
        // not just renyokei endings (し/み/etc.) to ensure complete inflected form
        //
        // Verify verb type consistency: if V2 is godan, reject ichidan
        // inflection matches (and vice versa). This prevents e.g. いた
        // (ichidan いる ta-form) from falsely matching godan 入る(いる).
        if (infl_result.confidence >= 0.3F && infl_result.base_form == form.v2_reading &&
            hasAuxiliarySuffix(infl_result.suffix) &&
            !(v2_verb.verb_type == V2VerbType::Godan && infl_result.verb_type == grammar::VerbType::Ichidan) &&
            !(v2_verb.verb_type == V2VerbType::Ichidan && infl_result.verb_type != grammar::VerbType::Ichidan)) {
          matched_inflected = true;
          form.matched_len = v2_end_byte - v2_start_byte;
          form.inflection_includes_aux = true;
          break;
        }
      }
    }

    // Case 2: Kanji V2 inflected forms (e.g., 巡った from 巡る): match the
    // V2's kanji prefix (all 3-byte CJK codepoints), then analyze the
    // kanji+hiragana span after it.
    if (!matched_inflected && char_types[v2_start] == CharType::Kanji) {
      const auto v2_surface_decoded = normalize::toCodepoints(v2_surface);
      size_t kanji_prefix_len = 0;
      while (kanji_prefix_len < v2_surface_decoded.size() &&
             kana::isKanjiCodepoint(v2_surface_decoded[kanji_prefix_len])) {
        ++kanji_prefix_len;
      }
      const size_t hira_start = v2_start + kanji_prefix_len;
      if (kanji_prefix_len > 0 && kanji_prefix_len < v2_surface_decoded.size() &&
          spellsAt(text, v2_start_byte, v2_surface.substr(0, kanji_prefix_len * core::kJapaneseCharBytes)) &&
          hira_start < ctx.codepoints.size() && char_types[hira_start] == CharType::Hiragana) {
        const size_t hira_end = findCharRegionEnd(char_types, hira_start, 6, CharType::Hiragana);
        // Shortest match first; the suffix must carry an actual auxiliary.
        for (size_t v2_end = hira_start + 1; v2_end <= hira_end && !matched_inflected; ++v2_end) {
          const size_t v2_end_byte = byteOffsetAt(ctx.byte_offsets, v2_end);
          const std::string_view v2_text = text.substr(v2_start_byte, v2_end_byte - v2_start_byte);
          for (const auto& infl_result : ctx.inflection.analyze(v2_text)) {
            if (infl_result.confidence >= 0.35F && infl_result.base_form == v2_surface &&
                hasAuxiliarySuffix(infl_result.suffix)) {
              matched_inflected = true;
              form.matched_len = v2_end_byte - v2_start_byte;
              form.inflection_includes_aux = true;
              break;
            }
          }
        }
      }
    }
  }
}

// The inflected forms (Cases 1 and 2) and the mizenkei before an auxiliary
// (Case 3), which share the mizenkei-boundary test.
void matchV2AuxiliaryForms(const CompoundMatchContext& ctx, V2FormMatch& form) {
  const std::string_view text = ctx.text;
  const size_t v2_start_byte = ctx.v2_start_byte;
  const auto& v2_verb = form.v2_verb;
  const std::string_view v2_reading = form.v2_reading;

  // Keep a Godan mizenkei before its auxiliary separate.  Otherwise an
  // inflection match over the longer span (しきらない) would hide the
  // grammatical boundary that the mizenkei candidate below represents.
  const std::string kanji_mizen = v2_verb.joins_surface ? generateMizenkei(form.v2_surface, "", v2_verb.verb_type) : "";
  const std::string hira_mizen = !v2_reading.empty() ? generateMizenkei(v2_reading, "", v2_verb.verb_type) : "";
  const bool mizenkei_before_aux = beginsMizenkeiAuxiliary(text, v2_start_byte, kanji_mizen) ||
                                   beginsMizenkeiAuxiliary(text, v2_start_byte, hira_mizen);

  // Try inflection analysis for inflected V2 forms (e.g., きった, 込んだ, 巡った)
  // Only for base forms (not renyokei entries) to avoid double-matching
  // Skip if already matched via renyokei to prevent aux detection overriding renyokei match
  if (!form.matched_kanji && !form.matched_reading && !form.matched_renyokei && !form.matched_potential &&
      !form.matched_kateikei && !form.matched_volitional && !mizenkei_before_aux && !v2_reading.empty()) {
    matchV2InflectedForm(ctx, form);
  }

  // Case 3: V2 mizenkei form match before passive, causative, or negative auxiliaries.
  // E.g., 打ち込まれ, 取り込ませ, 見当たらない.
  if (!form.matched_kanji && !form.matched_reading && !form.matched_renyokei && !form.matched_potential &&
      !form.matched_kateikei && !form.matched_volitional && !form.matched_inflected) {
    if (mizenkei_before_aux) {
      form.matched_mizenkei = true;
      form.matched_len =
          beginsMizenkeiAuxiliary(text, v2_start_byte, kanji_mizen) ? kanji_mizen.size() : hira_mizen.size();
    }
  }
}

void matchV2Imperative(const CompoundMatchContext& ctx, V2FormMatch& form) {
  const auto& codepoints = ctx.codepoints;
  const auto& char_types = ctx.char_types;
  const auto& v2_verb = form.v2_verb;
  const std::string_view v2_reading = form.v2_reading;
  bool& matched_imperative = form.matched_imperative;

  // Case 4: the V2 imperative. A Godan verb spells it with the e-row, the
  // same surface the kateikei is built on (刻み+込め, 書き+込め); an Ichidan
  // verb spells it as its stem plus ろ or よ (呼び続けろ, 呼び続けよ). Nothing
  // follows it, and that is what separates it from the competing readings of
  // those characters: the conditional needs its ば, the potential's stem needs
  // the auxiliary it is a stem for, and the Ichidan stem alone is a
  // continuative, which no sentence ends on. A predicate closing the sentence
  // is the imperative, so the compound keeps its boundary here as it does in
  // every other cell.
  // The Ichidan stem is also what the renyokei match reads, and that match is
  // there to leave a following auxiliary as its own token (申し上げ+ます). ろ
  // and よ closing the text are not auxiliaries, and a continuative does not
  // end a sentence, so the imperative replaces the renyokei reading there.
  if (!form.matched_kanji && !form.matched_reading && !form.matched_potential && !form.matched_kateikei &&
      !form.matched_volitional && !form.matched_inflected && !form.matched_mizenkei &&
      (!form.matched_renyokei || v2_verb.verb_type == V2VerbType::Ichidan)) {
    auto tryImperative = [&](const std::string& imperative, bool via_reading) {
      if (matched_imperative || !spellsAt(ctx.text, ctx.v2_start_byte, imperative)) {
        return;
      }
      // Nothing predicative follows an imperative. That is the end of the
      // text, a punctuation mark closing the clause for it (呼び続けよ、…), or
      // the next word starting on kanji (走り出せ今). The competing readings
      // all need kana right after: ば for the conditional, the auxiliary for
      // the potential's stem.
      // A sentence-final or quotative particle also closes it (走り出せ+よ,
      // 書き直せ+と+言う); ば and the auxiliaries of the potential do not.
      const size_t imperative_end = ctx.v2_start + normalize::utf8Length(imperative);
      const bool closed_by_particle =
          verb_helpers::oneMoraParticleEndsAt(&ctx.dict_manager, codepoints, imperative_end + 1,
                                              core::ExtendedPOS::ParticleFinal) ||
          verb_helpers::oneMoraParticleEndsAt(&ctx.dict_manager, codepoints, imperative_end + 1,
                                              core::ExtendedPOS::ParticleQuote) ||
          (imperative_end < codepoints.size() && codepoints[imperative_end] == core::hiragana::kTo);
      if (imperative_end < char_types.size() && char_types[imperative_end] != CharType::Symbol &&
          char_types[imperative_end] != CharType::Kanji && !closed_by_particle) {
        return;
      }
      matched_imperative = true;
      form.matched_len = imperative.size();
      form.matched_renyokei_via_reading = via_reading;
    };
    if (v2_verb.verb_type == V2VerbType::Godan) {
      tryImperative(v2_verb.joins_surface ? generateKateikei(form.v2_surface, "", v2_verb.verb_type) : "", false);
      tryImperative(!v2_reading.empty() ? generateKateikei(v2_reading, "", v2_verb.verb_type) : "", true);
    } else {
      const std::string kanji_stem =
          v2_verb.joins_surface ? generateRenyokei(form.v2_surface, "", v2_verb.verb_type) : "";
      const std::string hira_stem = !v2_reading.empty() ? generateRenyokei(v2_reading, "", v2_verb.verb_type) : "";
      for (const auto* ending : kIchidanImperativeEndings) {
        tryImperative(kanji_stem.empty() ? "" : normalize::concat(kanji_stem, ending), false);
        tryImperative(hira_stem.empty() ? "" : normalize::concat(hira_stem, ending), true);
      }
      if (matched_imperative) {
        form.matched_renyokei = false;
        form.is_renyokei_entry = false;
      }
    }
  }
}

void matchV2ClassicalTerminal(const CompoundMatchContext& ctx, V2FormMatch& form) {
  const auto& codepoints = ctx.codepoints;
  const auto& v2_verb = form.v2_verb;

  // Case 5: the classical lower-bigrade 終止形 and 連体形 of an Ichidan V2
  // (詰める → 詰む, 詰むる). Their kana is also a modern Godan ending, so no
  // other V2 reading claims the span; they are read only where the classical
  // paradigm is named, a clause end or a classical auxiliary behind a finite
  // cell, or the 連体形's own trailing る.
  if (!form.matched_kanji && !form.matched_reading && !form.matched_renyokei && !form.matched_potential &&
      !form.matched_kateikei && !form.matched_volitional && !form.matched_inflected && !form.matched_mizenkei &&
      !form.matched_imperative && v2_verb.verb_type == V2VerbType::Ichidan) {
    auto tryClassical = [&](const std::string& stem) {
      if (form.matched_classical || stem.size() <= core::kJapaneseCharBytes) {
        return;
      }
      const std::string stem_head = stem.substr(0, stem.size() - core::kJapaneseCharBytes);
      size_t tail_pos = stem_head.size();
      const char32_t terminal_kana = grammar::bigradeTerminalFromIrrealis(normalize::decodeUtf8(stem, tail_pos));
      if (terminal_kana == 0) {
        return;
      }
      const std::string terminal = stem_head + normalize::encodeUtf8(terminal_kana);
      if (!spellsAt(ctx.text, ctx.v2_start_byte, terminal)) {
        return;
      }
      const size_t terminal_end = ctx.v2_start + normalize::utf8Length(terminal);
      const bool attributive = terminal_end < codepoints.size() && codepoints[terminal_end] == core::hiragana::kRu;
      if (!attributive && !verb_helpers::shuushikeiEndsAt(codepoints, terminal_end, &ctx.dict_manager)) {
        return;
      }
      form.matched_classical = true;
      form.classical_attributive = attributive;
      form.classical_terminal = terminal;
      form.matched_len = terminal.size() + (attributive ? core::kJapaneseCharBytes : 0);
    };
    tryClassical(v2_verb.joins_surface ? generateRenyokei(form.v2_surface, "", v2_verb.verb_type) : "");
    tryClassical(!form.v2_reading.empty() ? generateRenyokei(form.v2_reading, "", v2_verb.verb_type) : "");
  }
}

}  // namespace

bool matchV2Form(const CompoundMatchContext& ctx, V2FormMatch& form) {
  const std::string_view text = ctx.text;
  const auto& codepoints = ctx.codepoints;
  const size_t v2_start = ctx.v2_start;
  const size_t v2_start_byte = ctx.v2_start_byte;

  if (grammar::isSuruRenyokeiSurface(ctx.v1_surface) && !form.v2_verb.joins_suru) {
    return false;
  }

  // The hiragana reading of compound V2 入る overlaps with the aspect
  // auxiliary いる.  A preceding て/で is a grammatical boundary
  // (異なっ|て|いる), not a renyokei stem for a compound ending in 入る.
  // Keep real compounds such as 立ち入る eligible: their V2 begins directly
  // after the V1 renyokei and therefore has no te-form particle before it.
  if (form.v2_reading == "いる" && v2_start > ctx.start_pos &&
      (codepoints[v2_start - 1] == U'て' || codepoints[v2_start - 1] == U'で')) {
    return false;
  }

  // The hiragana V2 reading 切る also overlaps with the lexical potential
  // verb できる.  Its leading で completes that word, rather than forming an
  // ichidan V1 stem (化でる) before a compound-verb V2.  Kanji-written V2
  // compounds such as 撫で切る are unaffected.
  if (form.v2_reading == "きる" && v2_start > ctx.start_pos && codepoints[v2_start - 1] == U'で') {
    return false;
  }

  if (!matchV2BaseForm(ctx, form) || !matchV2Renyokei(ctx, form)) {
    return false;
  }
  matchV2Potential(ctx, form);
  matchV2Kateikei(ctx, form);
  matchV2Volitional(ctx, form);
  matchV2AuxiliaryForms(ctx, form);
  matchV2Imperative(ctx, form);
  matchV2ClassicalTerminal(ctx, form);

  const bool matched_kanji = form.matched_kanji;
  const bool matched_reading = form.matched_reading;
  const bool matched_renyokei = form.matched_renyokei;
  const bool matched_potential = form.matched_potential;
  const bool matched_kateikei = form.matched_kateikei;
  const bool matched_volitional = form.matched_volitional;
  const bool matched_inflected = form.matched_inflected;
  const bool matched_mizenkei = form.matched_mizenkei;
  const bool matched_imperative = form.matched_imperative;
  const bool matched_classical = form.matched_classical;
  const size_t matched_len = form.matched_len;
  if (!matched_kanji && !matched_reading && !matched_renyokei && !matched_potential && !matched_kateikei &&
      !matched_volitional && !matched_inflected && !matched_mizenkei && !matched_imperative && !matched_classical) {
    return false;
  }

  // Do not let a kanji V2 ending in 「く」consume the first mora of the
  // polite request auxiliary 「ください」.  In ご理解ください, for example,
  // 解く must not create the spurious compound candidate 理解く; the
  // remaining ださい is not a valid auxiliary boundary.  The check is
  // surface-independent and leaves real V2 compounds untouched.
  if (matched_len >= core::kJapaneseCharBytes && v2_start_byte + matched_len < text.size() &&
      text.substr(v2_start_byte + matched_len - core::kJapaneseCharBytes, core::kJapaneseCharBytes) == "く" &&
      utf8::startsWith(text.substr(v2_start_byte + matched_len), "ださい")) {
    return false;
  }

  SUZUME_DEBUG_LOG_VERBOSE("[COMPOUND] V2 matched: "
                           << form.v2_verb.surface << " kanji=" << matched_kanji << " reading=" << matched_reading
                           << " renyokei=" << matched_renyokei << " potential=" << matched_potential
                           << " kateikei=" << matched_kateikei << " inflected=" << matched_inflected
                           << " mizenkei=" << matched_mizenkei << " volitional=" << matched_volitional
                           << " imperative=" << matched_imperative << " len=" << matched_len << "\n");
  return true;
}

}  // namespace suzume::analysis::compound_verb_detail
