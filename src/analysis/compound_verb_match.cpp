/**
 * @file compound_verb_match.cpp
 * @brief V1 verification and V2 matching for compound verbs
 */
#include "analysis/dictionary_probe.h"
#include "compound_verb_v2_cells_internal.h"
#include "grammar/char_patterns.h"
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

// The closed causative conditional せ+れ+ば starting at @p pos.
bool startsCausativeConditional(const std::vector<char32_t>& codepoints, size_t pos) {
  return pos + 2 < codepoints.size() && codepoints[pos] == U'せ' && codepoints[pos + 1] == U'れ' &&
         codepoints[pos + 2] == U'ば';
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

// The hiragana V1 opening checks. Sets @p hiragana_v1_in_dictionary and returns
// true when V1 has reached past its stem into a closed-class word.
bool rejectsHiraganaV1(const CompoundMatchContext& ctx, bool& hiragana_v1_in_dictionary) {
  const auto& codepoints = ctx.codepoints;
  const size_t start_pos = ctx.start_pos;
  const size_t v2_start = ctx.v2_start;
  const auto& dict_manager = ctx.dict_manager;
  const std::string_view v1_surface = ctx.v1_surface;

  // Every form a lexical compound builds on is a continuative or its onbin,
  // and none of them ends in an a-row kana: the i-row and e-row spell the
  // continuative, っ and ん spell the onbin, and the a-row spells the irrealis,
  // which takes an auxiliary rather than a verb. A hiragana V1 ending there
  // has therefore reached past the stem into the auxiliary that closes the
  // predicate (き+た of 流れてきた, れ+た of 落とされた), and joining it would
  // build a lexical verb on top of a finished clause. The V2 side of this
  // boundary is guarded by the past-auxiliary test further down.
  // @see fabricated closed-class absorption guards (verb_candidates_absorption_guards.h)
  if (v2_start > start_pos && kana::isARowCodepoint(codepoints[v2_start - 1])) {
    SUZUME_DEBUG_LOG_VERBOSE("[COMPOUND] rejected a-row tail on hiragana V1: " << v1_surface << "\n");
    return true;
  }
  const std::string v1_base = hiraganaV1Base(v1_surface, ctx.is_ichidan, ctx.base_ending);
  hiragana_v1_in_dictionary = dict_manager.lookupExact(v1_base, core::PartOfSpeech::Verb) != nullptr;
  // A V1 that also reads as a finished adjective (いい) followed by a final
  // particle mora has closed its clause (いい+か+って, いい+よ+って).
  const auto* v2_head_particle =
      lookupEntryInRange(dict_manager, codepoints, v2_start, v2_start + 1, core::PartOfSpeech::Particle);
  if (v2_head_particle != nullptr && v2_head_particle->extended_pos == core::ExtendedPOS::ParticleFinal &&
      dict_manager.lookupExact(v1_surface, core::PartOfSpeech::Adjective) != nullptr) {
    SUZUME_DEBUG_LOG_VERBOSE("[COMPOUND] rejected final particle after adjective-like V1: " << v1_surface << "\n");
    return true;
  }
  const bool hiragana_v1_has_strong_inflection =
      !hiragana_v1_in_dictionary && hasInflectionCandidateForBase(ctx.inflection, v1_surface, v1_base,
                                                                  candidate::verb_cost::kConstructedVerbMinConfidence);
  if (!hiragana_v1_in_dictionary && !ctx.allow_closed_onbin_v1) {
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
        SUZUME_DEBUG_LOG_VERBOSE(
            "[COMPOUND] rejected particle inside hiragana V1: " << extractSubstring(codepoints, pos, pos + 1) << "\n");
        return true;
      }
    }
  }
  return false;
}

bool crossesClauseParticle(const CompoundMatchContext& ctx, bool hiragana_v1_in_dictionary) {
  const auto& codepoints = ctx.codepoints;
  const size_t start_pos = ctx.start_pos;
  const size_t v2_start = ctx.v2_start;

  // A compound verb joins two verbal components directly. If a closed-class
  // particle occurs between the prospective V1 and V2 boundaries, the span is
  // compositional instead (読む+だけ+あって), not a compound verb.
  for (size_t particle_start = start_pos; particle_start < v2_start && !hiragana_v1_in_dictionary; ++particle_start) {
    const std::string particle_probe = extractSubstring(codepoints, particle_start, v2_start);
    // Away from the span's own start, the same u-row evidence the single-char
    // check below relies on decides whether a clause actually closed there.
    // Without it the okurigana of a continuative is read as a particle the
    // moment it happens to spell one (向か+い, where かい is the final
    // particle), and the compound is rejected before V1 is ever analyzed.
    const bool closes_preceding_clause =
        particle_start == start_pos || kana::isURowCodepoint(codepoints[particle_start - 1]);
    if (closes_preceding_clause) {
      for (const auto& match : ctx.dict_manager.lookup(particle_probe, 0)) {
        if (match.entry != nullptr && match.entry->pos == core::PartOfSpeech::Particle &&
            normalize::utf8Length(match.entry->surface) > 1 &&
            particle_start + normalize::utf8Length(match.entry->surface) <= v2_start) {
          return true;
        }
      }
    }

    // A terminal u-row verb followed by a case particle is a clause boundary,
    // even when the particle is a single character (行く+に+越した).  A
    // continuative ending such as し remains eligible for lexical compounds.
    if (particle_start > start_pos && kana::isURowCodepoint(codepoints[particle_start - 1]) &&
        ctx.dict_manager.lookupExact(particle_probe, core::PartOfSpeech::Particle) != nullptr) {
      return true;
    }
  }
  return false;
}

// Whether a matched V2 with a verified V1 still loses the span to the noun,
// quantity, past-auxiliary, suffix or adjective reading of the same text.
bool rejectsCompoundSpan(const CompoundMatchContext& ctx, const V2FormMatch& form, const CompoundV1Verification& v1) {
  const auto& codepoints = ctx.codepoints;
  const size_t start_pos = ctx.start_pos;
  const size_t kanji_end = ctx.kanji_end;
  const size_t v2_start = ctx.v2_start;
  const bool dict_compound_v1 = ctx.dict_compound_v1;

  // A numeral names a quantity and is never a verbal element, so it cannot be
  // the V1 of a compound verb.  The productive single-kanji V1 fallback is
  // dictionary-free and otherwise accepts one (三+切れる, 二+重ねる), stealing
  // the span from the quantity phrase it actually spells.
  if (isNumeralOnlySpan(codepoints, start_pos, v2_start)) {
    return true;
  }

  // A compound may begin inside a preceding kanji noun only when its V1 is
  // independently dictionary-verified (蛙+飛び込む, 報告+申し上げる).  An
  // inflection-only V1 in that position can instead fabricate a compound
  // across the noun/verb boundary (生+涯忘れる).
  const bool starts_inside_kanji_run = start_pos > 0 && normalize::isKanjiCodepoint(codepoints[start_pos - 1]);
  if (starts_inside_kanji_run && startsInsideRegisteredNoun(ctx.dict_manager, ctx.text, ctx.byte_offsets, start_pos)) {
    return true;
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
  if (starts_inside_kanji_run && !follows_counter && !v1_carries_okurigana && !v1.dict_verified && !dict_compound_v1) {
    return true;
  }

  // A hiragana V2 opening with た/だ has the same surface as the past
  // auxiliary wherever the V1 form in front of it is one the auxiliary
  // selects: た after an Ichidan continuative or an い/っ onbin (食べ+た,
  // 書い+た, 買っ+た), だ after an い/ん onbin (読ん+だ, 泳い+だ).  Without
  // lexical attestation of the compound itself the past reading owns that
  // boundary.  Elsewhere the V2 stays available (食べ+だす), as does any
  // kanji-spelled V2 (見+立てる).
  if (!form.matched_kanji && !dict_compound_v1 && v2_start > start_pos &&
      ctx.char_types[v2_start] == CharType::Hiragana) {
    const char32_t v1_tail = codepoints[v2_start - 1];
    const bool past_ta = codepoints[v2_start] == U'た' && (ctx.is_ichidan || v1_tail == U'い' || v1_tail == U'っ');
    const bool past_da = codepoints[v2_start] == U'だ' && (v1_tail == U'い' || v1_tail == U'ん');
    if (past_ta || past_da) {
      return true;
    }
  }

  // A nominal base followed by an independently registered suffix form
  // (税+抜き, 水+抜き) is not evidence for a lexical compound verb.  The
  // productive single-kanji V1 fallback is deliberately dictionary-free,
  // so let the suffix analysis own this boundary unless V1 itself was
  // dictionary-verified.
  if (!v1.dict_verified && !dict_compound_v1 && !ctx.is_sokuonbin && kanji_end < codepoints.size()) {
    const std::string v2_form = extractSubstring(codepoints, v2_start, kanji_end + 1);
    constexpr PartOfSpeechMask kNominalSuffixMask =
        partOfSpeechMask(core::PartOfSpeech::Suffix) | partOfSpeechMask(core::PartOfSpeech::Noun);
    if (hasExactPartOfSpeech(ctx.dict_manager, v2_form, kNominalSuffixMask)) {
      return true;
    }
  }

  // For inflected V2 matches (Case 1/2), check if the full surface could be
  // an adjective instead of a compound verb. This prevents false positives
  // like 美しかった (adjective) being parsed as 美し+交った (compound verb).
  if (form.matched_inflected && form.inflection_includes_aux) {
    // Calculate full compound surface
    size_t compound_end_byte = ctx.v2_start_byte + form.matched_len;
    std::string full_surface(ctx.text.substr(ctx.start_byte, compound_end_byte - ctx.start_byte));

    // Check if full surface could be an i-adjective
    auto full_infl = ctx.inflection.getBest(full_surface);
    if (full_infl.confidence >= 0.5F && full_infl.verb_type == grammar::VerbType::IAdjective && !v1.dict_verified &&
        !dict_compound_v1) {
      // With no independently verified V1, the whole adjective analysis is
      // stronger evidence (美しかった). A dictionary-backed continuative V1
      // followed by an allowed V2 remains a productive compound even when
      // the inflection analyzer fabricates an i-adjective homograph from its
      // shared かった ending (差し掛かった).
      return true;
    }
  }
  return false;
}

// The compound's dictionary form and the form spelled with the input's V2.
struct CompoundBases {
  std::string source_base;
  std::string base;
};

CompoundBases buildCompoundBases(const CompoundMatchContext& ctx, const V2FormMatch& form) {
  // Build the compound base while preserving the V2 orthography supplied by
  // the input. A hiragana V2 is a deliberate spelling choice (読みかける),
  // not an instruction to normalize it to the table's kanji representative
  // (読み掛ける).
  const size_t v1_renyokei_end = ctx.is_ichidan ? ctx.v2_start_byte : byteOffsetAt(ctx.byte_offsets, ctx.kanji_end + 1);
  const std::string_view v1_renyokei = ctx.text.substr(ctx.start_byte, v1_renyokei_end - ctx.start_byte);
  const std::string_view v2_spelling = ctx.char_types[ctx.v2_start] == CharType::Hiragana && !form.v2_reading.empty()
                                           ? form.v2_reading
                                           : form.v2_surface;
  const std::string compound_source_base = normalize::concat(v1_renyokei, v2_spelling);
  const std::string compound_base =
      form.matched_classical ? normalize::concat(v1_renyokei, form.classical_terminal)
      : form.matched_potential
          ? normalize::concat(v1_renyokei, generateGodanPotential(v2_spelling, "", form.v2_verb.verb_type))
          : compound_source_base;
  return {compound_source_base, compound_base};
}

// Whether this V2 reading replaces the best one found so far.
bool outranksBestMatch(const CompoundMatchContext& ctx, const V2FormMatch& form, const std::string& compound_base,
                       const CompoundVerbMatch& best_match) {
  const auto& codepoints = ctx.codepoints;
  const size_t v2_start = ctx.v2_start;
  const size_t v2_start_byte = ctx.v2_start_byte;
  const auto& dict_manager = ctx.dict_manager;
  const auto& v2_verb = form.v2_verb;
  const bool is_renyokei_entry = form.is_renyokei_entry;
  const bool matched_kanji = form.matched_kanji;
  const bool matched_reading = form.matched_reading;
  const bool matched_renyokei = form.matched_renyokei;
  const bool matched_volitional = form.matched_volitional;
  const bool matched_mizenkei = form.matched_mizenkei;
  const bool matched_imperative = form.matched_imperative;
  const bool matched_classical = form.matched_classical;
  const bool inflection_includes_aux = form.inflection_includes_aux;
  const size_t matched_len = form.matched_len;

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
      matched_mizenkei && startsCausativeConditional(codepoints, matched_end_pos);
  const size_t best_mizenkei_end_pos =
      advanceCharsToBytePos(codepoints, v2_start, v2_start_byte, v2_start_byte + best_match.matched_len);
  const bool best_mizenkei_has_causative_conditional =
      best_match.is_mizenkei && startsCausativeConditional(codepoints, best_mizenkei_end_pos);
  bool should_update = false;
  if (best_match.matched_len == 0) {
    // First valid match
    should_update = true;
  } else if (matched_classical || best_match.is_classical_terminal) {
    // The classical cells are the weakest reading: any lexical V2 match of the
    // same text outranks them, and they never replace one.
    should_update = best_match.is_classical_terminal && !matched_classical;
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
        renyokei_end_pos < codepoints.size() && grammar::isDeverbalSuffixKanji(codepoints[renyokei_end_pos]);
    const bool followed_by_nominal_particle = beginsNominalForcingParticle(codepoints, renyokei_end_pos, dict_manager);
    const bool followed_by_ichidan_conditional =
        v2_verb.verb_type == V2VerbType::Ichidan && renyokei_end_pos + 1 < codepoints.size() &&
        codepoints[renyokei_end_pos] == U'れ' && codepoints[renyokei_end_pos + 1] == U'ば';
    if (!best_mizenkei_has_causative_conditional && (followed_by_deverbal_suffix || followed_by_nominal_particle ||
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
  } else if (matched_imperative && best_match.is_imperative) {
    // Two imperatives can overlap when a particle closes the shorter one: the
    // Godan 続け before よ and the Ichidan 続けよ. The longer spells more of
    // the text, and the particle reading is the fallback.
    should_update = matched_len > best_match.matched_len;
  } else if ((matched_kanji || matched_reading || matched_renyokei) && best_match.is_imperative) {
    // Any lexical V2 match outranks an imperative generated off a different
    // Godan V2 that happens to spell the same characters: つけ is the
    // continuative of the listed つける before it is the imperative of つく.
    // A shorter match spells fewer of them (the 出 of 出る inside 出せ).
    should_update = matched_len >= best_match.matched_len;
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
  return should_update;
}

void recordBestMatch(const CompoundMatchContext& ctx, const V2FormMatch& form, const CompoundBases& bases,
                     const CompoundV1Verification& v1, CompoundVerbMatch& best_match) {
  const bool has_kanji_v2_after_bare_ichidan = ctx.has_kanji_v2_after_bare_ichidan;
  const auto& v2_verb = form.v2_verb;
  const bool is_renyokei_entry = form.is_renyokei_entry;
  const bool matched_kanji = form.matched_kanji;
  const bool matched_reading = form.matched_reading;
  const bool matched_inflected = form.matched_inflected;
  const bool matched_kateikei = form.matched_kateikei;
  const bool matched_potential = form.matched_potential;
  const bool matched_renyokei_via_reading = form.matched_renyokei_via_reading;
  const size_t matched_len = form.matched_len;
  const bool inflection_includes_aux = form.inflection_includes_aux;
  const bool matched_renyokei = form.matched_renyokei;
  const bool matched_volitional = form.matched_volitional;
  const bool matched_mizenkei = form.matched_mizenkei;
  const bool matched_imperative = form.matched_imperative;
  const bool matched_classical = form.matched_classical;
  const bool classical_attributive = form.classical_attributive;

  best_match.matched_len = matched_len;
  best_match.compound_base = bases.base;
  best_match.compound_source_base = bases.source_base;
  best_match.is_renyokei = is_renyokei_entry && (matched_kanji || matched_reading);
  best_match.renyokei_form = matched_renyokei;
  best_match.is_mizenkei = matched_mizenkei;
  best_match.is_volitional = matched_volitional;
  best_match.is_kateikei = matched_kateikei;
  best_match.is_imperative = matched_imperative;
  best_match.is_potential = matched_potential;
  best_match.is_classical_terminal = matched_classical;
  best_match.is_classical_attributive = classical_attributive;
  best_match.includes_aux = inflection_includes_aux;
  best_match.matched_via_reading = matched_reading || matched_inflected || matched_renyokei_via_reading;
  best_match.v2_verb = &v2_verb;
  best_match.v1_dict_verified = v1.dict_verified;
  best_match.v1_embedded_verified = v1.embedded_verified;
  best_match.v1_ichidan_inflection = v1.ichidan_inflection;
  best_match.v1_bare_ichidan = has_kanji_v2_after_bare_ichidan && v1.ichidan_inflection;
  best_match.v1_godan_inflection = v1.godan_inflection;
}

// Whether the selected kana V2 reading yields to a dictionary verb or a listed
// particle spelling the same tail.
bool yieldsToListedReading(const CompoundMatchContext& ctx, const CompoundVerbMatch& best_match) {
  const std::string_view text = ctx.text;
  const size_t v2_start_byte = ctx.v2_start_byte;

  // A closed V2 reading must not override a dictionary-verified inflected
  // verb that consumes the same tail with a different lemma.  This resolves
  // arbitrary lexical tails from the existing dictionary (for example an
  // n-onbin Godan-ma form) without copying open-class verbs into the closed
  // compound-V2 table.
  if (!ctx.hiragana_v1 && best_match.v2_verb != nullptr && best_match.matched_via_reading &&
      best_match.matched_len > 0) {
    const size_t matched_chars = normalize::utf8Length(text.substr(v2_start_byte, best_match.matched_len));
    const std::string_view matched_v2_base =
        best_match.v2_verb->reading != nullptr ? best_match.v2_verb->reading : best_match.v2_verb->surface;
    for (const auto& result : ctx.dict_manager.lookup(text, v2_start_byte)) {
      if (result.entry != nullptr && result.length == matched_chars && result.entry->pos == core::PartOfSpeech::Verb &&
          result.entry->lemma != matched_v2_base) {
        return true;
      }
    }
  }

  // Nor may an unverified kanji V1 lend its okurigana to the compound when a
  // dictionary verb starts on that kana and ends with the V2: the listed verb
  // proves the kana belong together (油断+めさる, not 断め+さる).
  if (!ctx.hiragana_v1 && !best_match.v1_dict_verified && best_match.matched_len > 0) {
    const size_t compound_end_pos =
        ctx.v2_start + normalize::utf8Length(text.substr(v2_start_byte, best_match.matched_len));
    for (size_t pos = ctx.kanji_end; pos < ctx.v2_start; ++pos) {
      for (const auto& result : ctx.dict_manager.lookup(text, byteOffsetAt(ctx.byte_offsets, pos))) {
        if (result.entry != nullptr && result.entry->pos == core::PartOfSpeech::Verb &&
            pos + result.length == compound_end_pos) {
          return true;
        }
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
      ctx.dict_manager.lookupExact(text.substr(v2_start_byte, best_match.matched_len), core::PartOfSpeech::Particle) !=
          nullptr) {
    return true;
  }
  return false;
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
  const CompoundMatchContext ctx{
      text,
      codepoints,
      byte_offsets,
      start_pos,
      char_types,
      kanji_end,
      v2_start,
      base_ending,
      is_sokuonbin,
      is_ichidan,
      has_kanji_v2_after_bare_ichidan,
      dict_compound_v1,
      dict_compound_v1_lemma,
      dict_manager,
      inflection,
      hiragana_v1,
      allow_closed_onbin_v1,
      start_byte,
      v2_start_byte,
      v1_surface,
      // Find extent of hiragana after v2_start for inflection analysis
      findCharRegionEnd(char_types, v2_start, 8, CharType::Hiragana),
  };
  bool hiragana_v1_in_dictionary = false;
  if (hiragana_v1 && rejectsHiraganaV1(ctx, hiragana_v1_in_dictionary)) {
    return {};
  }
  if (crossesClauseParticle(ctx, hiragana_v1_in_dictionary)) {
    return {};
  }

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
    V2FormMatch form(v2_verb, v2_surface, v2_reading);
    if (!matchV2Form(ctx, form)) {
      continue;
    }

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
    if (rejectsCompoundSpan(ctx, form, v1)) {
      continue;
    }

    const CompoundBases bases = buildCompoundBases(ctx, form);
    if (outranksBestMatch(ctx, form, bases.base, best_match)) {
      recordBestMatch(ctx, form, bases, v1, best_match);
    }
  }

  SUZUME_DEBUG_LOG_VERBOSE("[COMPOUND] best_match.len=" << best_match.matched_len
                                                        << " base=" << best_match.compound_base << "\n");

  if (yieldsToListedReading(ctx, best_match)) {
    return {};
  }
  return best_match;
}

}  // namespace suzume::analysis::compound_verb_detail
