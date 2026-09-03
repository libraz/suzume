/**
 * @file compound_verb_emit.cpp
 * @brief Post-match validation, scoring, and edge emission for compound verbs
 */
#include "analysis/dictionary_probe.h"
#include "grammar/honorific_verbs.h"
#include "join_compound_verb_internal.h"

namespace suzume::analysis::compound_verb_detail {

bool beginsNominalForcingParticle(const std::vector<char32_t>& codepoints, size_t pos,
                                  const dictionary::DictionaryManager& dict_manager) {
  return startsNominalForcingParticle(codepoints, pos) && !startsLongerNonParticleEntry(codepoints, pos, &dict_manager);
}

namespace {

bool containsNegativeAuxiliary(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  for (size_t pos = start_pos + 1; pos < end_pos; ++pos) {
    if (verb_helpers::naiNegativeFollowsAt(codepoints, pos)) {
      return true;
    }
  }
  return false;
}

bool followsClosedSuffix(const std::vector<char32_t>& codepoints, size_t start_pos,
                         const dictionary::DictionaryManager& dict_manager) {
  return start_pos > 0 &&
         lookupEntryInRange(dict_manager, codepoints, start_pos - 1, start_pos, core::PartOfSpeech::Suffix) != nullptr;
}

// Whether the て/で in front of V2 is the conjunctive particle rather than the
// okurigana of an Ichidan V1 whose own base ends in てる.
bool hasConjunctiveTeBeforeV2(const std::vector<char32_t>& codepoints, size_t start_pos, size_t v2_start) {
  if (v2_start <= start_pos + 1 || v2_start > codepoints.size()) {
    return false;
  }
  if (codepoints[v2_start - 1] != core::hiragana::kTe && codepoints[v2_start - 1] != U'で') {
    return false;
  }
  // Okurigana attaches to a kanji stem; inflection kana in front of the mora
  // mean the verb reached it by conjugating (食べ+て, 持っ+て, 読ん+で).
  if (normalize::classifyChar(codepoints[v2_start - 2]) != normalize::CharType::Kanji) {
    return true;
  }
  // A bare single kanji of the closed Ichidan class is the continuative itself,
  // so the mora behind it is the particle (出+て, 着+て) rather than okurigana.
  return v2_start == start_pos + 2 && verb_helpers::isSingleKanjiIchidan(codepoints[start_pos]);
}

bool consumesSahenConditional(const std::vector<char32_t>& codepoints, size_t start_pos, size_t compound_end_pos,
                              const dictionary::DictionaryManager& dict_manager) {
  if (compound_end_pos <= start_pos + 2 || compound_end_pos + 1 >= codepoints.size() ||
      codepoints[compound_end_pos - 1] != U'す' || codepoints[compound_end_pos] != U'れ' ||
      codepoints[compound_end_pos + 1] != U'ば') {
    return false;
  }
  const std::string nominal_stem = extractSubstring(codepoints, start_pos, compound_end_pos - 1);
  if (normalize::utf8Length(nominal_stem) < 2 || !grammar::isAllKanji(nominal_stem)) {
    return false;
  }
  const std::string conditional = extractSubstring(codepoints, compound_end_pos - 1, compound_end_pos + 1);
  return hasCompleteVerbLemma(dict_manager, conditional, 2, "する");
}

// A generated split stem is usable only when it is a proper prefix of the
// compound as the text actually spells it, and its end stays inside the
// analysed span. Both split candidates share this admission test; the returned
// end position is where the auxiliary that follows would start.
constexpr size_t kStemNotAdmitted = static_cast<size_t>(-1);

size_t admittedStemEnd(const std::string& stem, std::string_view compound_surface, std::string_view text,
                       size_t start_byte, size_t start_pos, size_t codepoint_count) {
  if (stem.empty() || stem.size() >= compound_surface.size()) {
    return kStemNotAdmitted;
  }
  if (text.substr(start_byte, stem.size()) != stem) {
    return kStemNotAdmitted;
  }
  const size_t stem_end_pos = start_pos + normalize::utf8Length(stem);
  return stem_end_pos > codepoint_count ? kStemNotAdmitted : stem_end_pos;
}

}  // namespace

void emitCompoundVerbCandidates(core::Lattice& lattice, std::string_view text, const std::vector<char32_t>& codepoints,
                                const ByteOffsets& byte_offsets, size_t start_pos, size_t v2_start,
                                const CompoundVerbMatch& match, const dictionary::DictionaryManager& dict_manager,
                                const Scorer& scorer) {
  const CompoundVerbMatch& best_match = match;
  const size_t start_byte = byteOffsetAt(byte_offsets, start_pos);
  const size_t v2_start_byte = byteOffsetAt(byte_offsets, v2_start);
  SUZUME_DEBUG_LOG("[COMPOUND_EMIT] matched_len=" << best_match.matched_len << " start=" << start_pos
                                                  << " v2_start=" << v2_start << "\n");
  // After checking all V2 entries, use the best match if found
  if (best_match.matched_len > 0) {
    // A kanji+し compound may begin after a closed suffix (時間 / 話し込ん,
    // 長い / 間 / 話し込ん).  A bare adjacent kanji has no such boundary
    // evidence and must not license a compound from inside a Sino compound
    // (提出 -> 提 / 出し忘れ).
    if (verb_helpers::startsInsideKanjiRunBeforeShi(codepoints, start_pos) &&
        !grammar::isHumbleHonorificLemma(best_match.compound_base) &&
        !followsClosedSuffix(codepoints, start_pos, dict_manager)) {
      return;
    }
    if (start_pos > 0 && normalize::isKanjiCodepoint(codepoints[start_pos - 1]) &&
        startsInsideRegisteredNoun(dict_manager, text, byte_offsets, start_pos)) {
      return;
    }
    const SubsidiaryVerb& matched_v2 = *best_match.v2_verb;
    const std::string_view matched_v2_reading =
        matched_v2.joins_reading && matched_v2.reading != nullptr ? matched_v2.reading : "";

    // Calculate compound verb end position using matched length
    size_t compound_end_byte = v2_start_byte + best_match.matched_len;

    // Find character position for compound end
    size_t compound_end_pos = advanceCharsToBytePos(codepoints, v2_start, v2_start_byte, compound_end_byte);

    const std::string v2_surface = extractSubstring(codepoints, v2_start, compound_end_pos);
    SUZUME_DEBUG_LOG_VERBOSE("[COMPOUND_EMIT] span=" << extractSubstring(codepoints, start_pos, compound_end_pos)
                                                     << " v2=" << v2_surface << "\n");
    const auto* closed_auxiliary = dict_manager.lookupExact(v2_surface, core::PartOfSpeech::Auxiliary);
    const bool v2_is_closed_particle = dict_manager.lookupExact(v2_surface, core::PartOfSpeech::Particle) != nullptr;
    if (closed_auxiliary != nullptr && closed_auxiliary->extended_pos == core::ExtendedPOS::AuxAppearanceSou) {
      SUZUME_DEBUG_LOG_VERBOSE("[COMPOUND_EMIT] rejected appearance auxiliary\n");
      return;
    }
    // A causative auxiliary is a lexical V2 only behind an i-row continuative
    // (抱き+しめる, 噛み+しめる). Everywhere else the same surface is the
    // auxiliary closing a causative chain on an irrealis host (知ら+しめる,
    // 読ま+せ+しめ), and joining it into a compound erases the voice boundary
    // the chain is built from.
    if (closed_auxiliary != nullptr && closed_auxiliary->extended_pos == core::ExtendedPOS::AuxCausative &&
        (v2_start == 0 || !grammar::isIRowCodepoint(codepoints[v2_start - 1]))) {
      SUZUME_DEBUG_LOG_VERBOSE("[COMPOUND_EMIT] rejected causative auxiliary V2\n");
      return;
    }

    // Build the compound verb surface
    std::string compound_surface(text.substr(start_byte, compound_end_byte - start_byte));

    // A multi-kanji Sahen stem followed by すれば is noun + する仮定形.
    // A V2 such as 出す must not consume only the initial す and leave れ+ば
    // behind (提出+すれ+ば, not 提出す+れ+ば).
    if (consumesSahenConditional(codepoints, start_pos, compound_end_pos, dict_manager)) {
      SUZUME_DEBUG_LOG_VERBOSE("[COMPOUND_EMIT] rejected sahen conditional\n");
      return;
    }

    // Causative endings are auxiliary chains, not V2 compound verbs. This
    // also covers a passive followed by causative (書か+れ+させる), where the
    // permissive V2 matcher could otherwise reinterpret させる as a lexical
    // continuation and erase the voice boundary.
    if (verb_helpers::containsPassiveCausativeAuxPattern(compound_surface)) {
      SUZUME_DEBUG_LOG_VERBOSE("[COMPOUND_EMIT] rejected passive-causative chain\n");
      return;
    }

    // A lexical compound joins V1's continuative to V2, and て is a conjunctive
    // particle rather than a continuative ending. The two readings are told
    // apart by what the mora attaches to: an Ichidan verb whose base ends in
    // てる spells the て as okurigana straight after its kanji stem (捨て去る,
    // 立て直す), while a te-form reaches the mora through inflection kana
    // (食べ+て, 持っ+て, 読ん+で) or through the bare stem of the closed
    // single-kanji Ichidan class (出+て+歩く, 着+て+出かける).
    if (hasConjunctiveTeBeforeV2(codepoints, start_pos, v2_start)) {
      SUZUME_DEBUG_LOG_VERBOSE("[COMPOUND_EMIT] rejected conjunctive te before V2\n");
      return;
    }

    // A ka/ga-row i-onbin followed by で is a conjunctive te-form, not a
    // compound with the homographic V2 でる (急い+で+も). The following
    // particle supplies the closed right boundary for this distinction.
    if (v2_start > start_pos && v2_start < codepoints.size() && codepoints[v2_start] == U'で' &&
        codepoints[v2_start - 1] == U'い' && compound_end_pos < codepoints.size() &&
        lookupEntryInRange(dict_manager, codepoints, compound_end_pos, compound_end_pos + 1,
                           core::PartOfSpeech::Particle) != nullptr) {
      return;
    }

    // Skip if compound surface is registered as NOUN in dictionary,
    // UNLESS followed by an auxiliary suffix (た/て/で/ない) which indicates verb usage.
    // This prevents nominalized compound verbs (売り上げ, 打ち合わせ) from being tokenized as VERB
    // when standalone, while allowing 切り替えた, 打ち合わせて to be parsed as compound verbs.
    if (dict_manager.lookupExact(compound_surface, core::PartOfSpeech::Noun) != nullptr) {
      // Check if followed by auxiliary suffix
      bool followed_by_aux = false;
      if (compound_end_pos < codepoints.size()) {
        char32_t next_cp = codepoints[compound_end_pos];
        // た/て/で/な(い)/れ/ら/ま(す) indicate verb conjugation
        followed_by_aux = (next_cp == U'た' || next_cp == U'て' || next_cp == U'で' || next_cp == U'な' ||
                           next_cp == U'れ' || next_cp == U'ら' || next_cp == U'ま' || next_cp == U'ず');
        // A deverbal suffix also validates the compound continuative.  It is
        // emitted below as a nominal candidate, not retained as a verb.
        followed_by_aux = followed_by_aux || next_cp == U'方' || next_cp == U'手' || next_cp == U'物' ||
                          next_cp == U'所' || next_cp == U'場';
      }
      if (!followed_by_aux) {
        SUZUME_DEBUG_LOG("[COMPOUND_SKIP] \"" << compound_surface << "\" is dict NOUN, skipping compound verb\n");
        return;
      }
      SUZUME_DEBUG_LOG("[COMPOUND] \"" << compound_surface << "\" is dict NOUN but followed by aux, allowing\n");
    }

    // Skip if a hiragana-V2 variant of compound_base is registered as VERB in dictionary.
    // E.g., compound_base=取り掛かる but dict has 取りかかる: prefer dict's hiragana lemma.
    // This handles modern Japanese where mixed kanji+hiragana compounds (取りかかる, 引き起こす)
    // are conventionally written without normalizing V2 to kanji.
    if (best_match.matched_via_reading && !matched_v2_reading.empty() &&
        best_match.compound_base.size() > matched_v2_reading.size()) {
      // The input range up to v2_start is the V1 renyokei for both V1 paths.
      std::string v1_renyokei_text(text.substr(start_byte, v2_start_byte - start_byte));
      std::string hira_v2_compound = normalize::concat(v1_renyokei_text, matched_v2_reading);
      if (hira_v2_compound != best_match.compound_base) {
        const char32_t input_v2_initial = v2_start < codepoints.size() ? codepoints[v2_start] : 0;
        const char32_t reading_initial = utf8::decodeFirstChar(matched_v2_reading);
        if (input_v2_initial == reading_initial &&
            dict_manager.lookupExact(hira_v2_compound, core::PartOfSpeech::Verb) != nullptr) {
          SUZUME_DEBUG_LOG("[COMPOUND_SKIP] kanji compound \""
                           << best_match.compound_base << "\" yields to dict verb \"" << hira_v2_compound << "\"\n");
          return;
        }
      }
    }

    // Calculate cost
    float base_cost = scorer.posPrior(core::PartOfSpeech::Verb);
    const auto& opts = scorer.joinOpts();
    // Dict-verified V1 gets full bonus; inflection-only V1 gets a penalty.
    // Inflection analysis can verify verb forms that aren't real words (e.g., 進す),
    // so unverified compounds should be more expensive to prevent false positives
    // like 進し続ける winning over 前進+し+続ける.
    // A single-kanji ichidan V1 confirmed by inflection (受ける, 植える, 投げる)
    // is an unambiguous open-class verb, absent from the dictionary only because
    // it is rule-derivable — as strong as a dict-confirmed V1. It shares the full
    // bonus so its compound (受け入れ, 投げ入れ) beats a spurious split under a
    // following passive/potential られる (受け+入れ+られる).
    // Embedded-verified V1 (a dictionary verb embedded after a leading kanji,
    // e.g., 仕立てる = 仕 + 立てる) is weaker evidence: the leading kanji is
    // unconstrained, so it keeps the reduced penalty relative to a dict-confirmed V1.
    const bool v1_is_verified =
        best_match.v1_dict_verified || best_match.v1_ichidan_inflection || best_match.v1_godan_inflection;
    float v1_bonus = 0.0F;
    if (v1_is_verified) {
      v1_bonus = opts.verified_v1_bonus;  // -0.3: reward for a confirmed real V1
    } else if (best_match.v1_embedded_verified) {
      v1_bonus = bigram_cost::kMinor;  // +0.5: reduced penalty for partial-evidence V1
    } else {
      v1_bonus = bigram_cost::kRare;  // +1.0: penalty for inflection-only V1
    }
    float final_cost = base_cost + opts.compound_verb_bonus + v1_bonus;

    if (!best_match.is_renyokei && !best_match.is_mizenkei && !best_match.is_volitional && !best_match.is_kateikei &&
        !best_match.includes_aux) {
      final_cost += candidate::kCompleteCompoundVerbBonus;
    }

    // A closed-set bare one-kanji ichidan V1 followed by an allowlisted kanji
    // V2 is a strongly constrained productive compound. This matters when
    // the V2 appears in renyokei before an auxiliary (見回し+た): without the
    // connection, an unrelated multi-kanji noun plus する can win the path.
    if (best_match.v1_bare_ichidan && best_match.renyokei_form) {
      final_cost += bigram_cost::kStrongBonus;
    }

    // A compound whose complete lemma is attested in the dictionary is a
    // lexical search unit.  Prefer it over a coincidental noun + する or
    // verb + verb decomposition, while leaving productive, unregistered
    // compounds to their ordinary compositional scoring.
    const bool compound_lemma_in_dictionary =
        dict_manager.lookupExact(best_match.compound_base, core::PartOfSpeech::Verb) != nullptr;
    const bool compound_source_in_dictionary =
        best_match.is_potential &&
        dict_manager.lookupExact(best_match.compound_source_base, core::PartOfSpeech::Verb) != nullptr;
    const bool compound_honorific_verified = grammar::isHumbleHonorificLemma(best_match.compound_base);
    const bool compound_lemma_verified =
        compound_lemma_in_dictionary || compound_source_in_dictionary || compound_honorific_verified;
    const std::string nominalized_compound = generateRenyokei(best_match.compound_base, "", matched_v2.verb_type);
    const bool compound_nominalization_verified =
        !nominalized_compound.empty() &&
        dict_manager.lookupExact(nominalized_compound, core::PartOfSpeech::Noun) != nullptr;
    if (compound_honorific_verified) {
      final_cost += bigram_cost::kVeryStrongBonus;
    } else if (compound_lemma_verified ||
               (compound_nominalization_verified && !best_match.includes_aux && !best_match.is_mizenkei)) {
      final_cost += bigram_cost::kStrongBonus;
    }

    // A nominalized compound continuative also attests the underlying Godan
    // compound before a causative auxiliary (話し合い → 話し合わ+せる).  This
    // is the evidence that distinguishes the causative from a coincidental
    // V1連用形+合わせる path.
    if (best_match.is_mizenkei && compound_nominalization_verified && compound_end_pos < codepoints.size() &&
        codepoints[compound_end_pos] == U'せ') {
      final_cost += bigram_cost::kStrongBonus;
    }

    // Penalty for compound verbs that absorb auxiliary suffixes (た/て/れる/etc.)
    // When includes_aux is true, the compound has absorbed an inflectional suffix
    // that should split off (e.g., 語り継がれる → 語り継が|れる).
    // Te-stem and mizenkei candidates are generated separately (below) to provide
    // the split path; this penalty ensures the split path wins over the merged form.
    if (best_match.includes_aux) {
      final_cost += bigram_cost::kStrong;
    }

    uint8_t flags = core::LatticeEdge::kFromDictionary;
    // Only direct registration of the complete compound lemma grants lexical
    // boundary ownership. A nominalized continuative is useful scoring
    // evidence above, but must not suppress productive te-form boundaries.
    if (compound_lemma_verified) {
      flags |= core::LatticeEdge::kLemmaVerified;
    }

    // Compound_base preserves the V2's input orthography. Potential forms are
    // lexical terminal forms in the public token contract, so retain their
    // surface lemma (取り戻せる) rather than their Godan source form.
    std::string compound_lemma = best_match.compound_base;
    dictionary::ConjugationType compound_conj_type =
        compoundConjugationType(matched_v2.verb_type, matched_v2.base_ending);

    // Mizenkei match: add VerbMizenkei edge and return (no te-stem/mizenkei derivation)
    // E.g., 打ち込ま (mizenkei of 打ち込む) for passive 打ち込まれ
    if (best_match.is_mizenkei || best_match.is_volitional) {
      const char* pattern = best_match.is_volitional ? "compound_volitional_mizenkei" : "compound_mizenkei";
      lattice.addEdge(compound_surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(compound_end_pos),
                      core::PartOfSpeech::Verb, final_cost, flags, compound_lemma, compound_conj_type,
                      core::CandidateOrigin::VerbCompound, 0.0F, pattern, core::ExtendedPOS::VerbMizenkei, pattern);
      return;
    }

    // Renyokei-form compound (組み立て, 打ち立て): its surface can end in て/た/る, whose
    // auto-detected verb form would be VerbTeForm and wrongly trigger the te-form split
    // penalty in the scorer. Tag such matches explicitly as VerbRenyokei so the whole
    // compound competes fairly with the V1連用+V2 split; base-form matches keep the
    // pos-derived default (Unknown → auto-detect, e.g. 組み立てる → VerbShuushikei).
    // Require a 2+char V2 renyokei (立て, 重ね): a single-mora V2 renyokei ending in
    // て/で (出る→で) is genuinely te-form-ambiguous, so 持ち+で(出) must keep the penalty
    // and lose to 気持ち+で rather than winning as a spurious 持ちで compound.
    bool renyokei_multichar = best_match.renyokei_form && best_match.matched_len >= core::kTwoJapaneseCharBytes;
    core::ExtendedPOS compound_epos =
        best_match.is_imperative
            ? core::ExtendedPOS::VerbMeireikei
            : (best_match.is_kateikei
                   ? core::ExtendedPOS::VerbKateikei
                   : (renyokei_multichar ? core::ExtendedPOS::VerbRenyokei : core::ExtendedPOS::Unknown));
    const bool fuses_past_auxiliary = best_match.includes_aux && utf8::endsWithAny(compound_surface, {"た", "だ"});
    if (!fuses_past_auxiliary) {
      lattice.addEdge(compound_surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(compound_end_pos),
                      core::PartOfSpeech::Verb, final_cost, flags, compound_lemma, compound_conj_type,
                      core::CandidateOrigin::VerbCompound, candidate::kNoOriginConfidence, "compound", compound_epos,
                      "compound");
    }

    // A verified compound continuative directly marked by a case, topic, or
    // nominalizer particle heads a nominal phrase.  Emit its deverbal-noun
    // reading alongside the verbal edge so the particle does not force an
    // artificial split inside the compound (押し下げを, 押し付けは).
    const bool starts_inside_kanji_run = start_pos > 0 && normalize::isKanjiCodepoint(codepoints[start_pos - 1]);
    if (v1_is_verified && !starts_inside_kanji_run && !v2_is_closed_particle &&
        !containsNegativeAuxiliary(codepoints, start_pos, compound_end_pos) &&
        compound_epos == core::ExtendedPOS::VerbRenyokei &&
        beginsNominalForcingParticle(codepoints, compound_end_pos, dict_manager)) {
      const float noun_cost = scorer.posPrior(core::PartOfSpeech::Noun) + candidate::kCompoundVerbSuffixNounBonus;
      lattice.addEdge(compound_surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(compound_end_pos),
                      core::PartOfSpeech::Noun, noun_cost, flags, compound_surface, dictionary::ConjugationType::None,
                      core::CandidateOrigin::VerbCompound, candidate::kNoOriginConfidence, "compound_renyokei_nominal",
                      core::ExtendedPOS::NounVerbal, "compound_renyokei_nominal");
    }

    // A compound verb continuative followed by a deverbal suffix is a single
    // nominal search unit.  The V1/V2 verification above keeps this productive
    // rule from absorbing arbitrary kanji-hiragana sequences.
    if (best_match.renyokei_form && compound_end_pos < codepoints.size() &&
        (codepoints[compound_end_pos] == U'方' || codepoints[compound_end_pos] == U'物' ||
         codepoints[compound_end_pos] == U'所' || codepoints[compound_end_pos] == U'場')) {
      const size_t noun_end_pos = compound_end_pos + 1;
      const size_t noun_end_byte = byteOffsetAt(byte_offsets, noun_end_pos);
      const std::string noun_surface(text.substr(start_byte, noun_end_byte - start_byte));
      const float noun_cost = scorer.posPrior(core::PartOfSpeech::Noun) + candidate::kCompoundVerbSuffixNounBonus;
      lattice.addEdge(noun_surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(noun_end_pos),
                      core::PartOfSpeech::Noun, noun_cost, flags, noun_surface, dictionary::ConjugationType::None,
                      core::CandidateOrigin::VerbCompound, candidate::kNoOriginConfidence,
                      "compound_renyokei_suffix_noun", core::ExtendedPOS::NounVerbal, "compound_renyokei_suffix_noun");
    }

    // The agentive 手 remains a suffix search unit, while the preceding
    // compound continuative is nominalized (引き受け+手).
    if (best_match.renyokei_form && compound_end_pos < codepoints.size() && codepoints[compound_end_pos] == U'手') {
      const float noun_cost = scorer.posPrior(core::PartOfSpeech::Noun) + candidate::kCompoundVerbSuffixNounBonus;
      lattice.addEdge(compound_surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(compound_end_pos),
                      core::PartOfSpeech::Noun, noun_cost, flags, compound_surface, dictionary::ConjugationType::None,
                      core::CandidateOrigin::VerbCompound, candidate::kNoOriginConfidence,
                      "compound_renyokei_agentive_suffix", core::ExtendedPOS::NounVerbal,
                      "compound_renyokei_agentive_suffix");
    }

    // An ichidan V2 forms its conditional from the compound renyokei plus
    // れば.  Keep that inflectional boundary available for every lexical or
    // productive compound (言い換えれ+ば, 組み合わせれ+ば) instead of
    // reinterpreting れ as a passive auxiliary.
    if (matched_v2.verb_type == V2VerbType::Ichidan && best_match.renyokei_form &&
        compound_end_pos + 1 < codepoints.size() && codepoints[compound_end_pos] == U'れ' &&
        codepoints[compound_end_pos + 1] == U'ば') {
      const std::string kateikei_surface = compound_surface + "れ";
      lattice.addEdge(kateikei_surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(compound_end_pos + 1),
                      core::PartOfSpeech::Verb, final_cost, flags, compound_lemma, compound_conj_type,
                      core::CandidateOrigin::VerbCompound, candidate::kNoOriginConfidence, "compound_kateikei",
                      core::ExtendedPOS::VerbKateikei, "compound_kateikei");
    }

    // Generate a te-form euphonic stem candidate so the conjunctive particle
    // remains separate: 話し合って → 話し合っ|て.
    // Without this, the compound verb te-form (話し合って) would be a single token.
    auto [te_stem, uses_de] =
        generateTeFormStem(best_match.compound_base, "", matched_v2.verb_type, matched_v2.base_ending);

    // The te-form stem (e.g., 受け取っ before た) is itself the desired split
    // candidate, so it must not carry the includes_aux merge penalty. That
    // penalty exists only to keep the fully-merged inflected form (受け取った as
    // one token) from beating the stem+auxiliary split; applying it to the stem
    // as well would wrongly hand the win back to the full V1+V2+aux split when
    // V1 is an inflection-only verb (e.g., ichidan 受ける, not in the dictionary).
    float te_stem_cost = final_cost;
    if (best_match.includes_aux) {
      te_stem_cost -= bigram_cost::kStrong;
    }

    // Helper lambda to add te-stem edge
    auto addTeStemEdge = [&](const std::string& stem) {
      const size_t stem_end_pos =
          admittedStemEnd(stem, compound_surface, text, start_byte, start_pos, codepoints.size());
      if (stem_end_pos == kStemNotAdmitted)
        return false;

      // Determine ExtendedPOS based on te-form type
      auto te_type = getTeFormType(matched_v2.base_ending);
      core::ExtendedPOS epos;
      if (te_type == TeFormType::Renyokei) {
        epos = core::ExtendedPOS::VerbRenyokei;  // 話し (サ行)
      } else {
        epos = core::ExtendedPOS::VerbOnbinkei;  // 書い, 買っ, 読ん, etc.
      }

      const std::string stem_lemma = best_match.is_potential ? stem + "る" : compound_lemma;

      lattice.addEdge(stem, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(stem_end_pos),
                      core::PartOfSpeech::Verb, te_stem_cost, flags, stem_lemma, compound_conj_type,
                      core::CandidateOrigin::VerbCompound, 0.0F, "compound_te_stem", epos, "compound_te_stem");
      return true;
    };

    // Try kanji te-stem first
    bool added_te_stem = addTeStemEdge(te_stem);

    // Reuse the matching V2 metadata to produce the spelling variant once for
    // both te-form and mizenkei split candidates. A reading match can retain a
    // kanji table representative in compound_base, so retain the old suffix
    // guard before replacing that representative.
    std::string hira_compound_base;
    if (best_match.matched_via_reading && !matched_v2_reading.empty()) {
      const std::string_view matched_v2_surface(matched_v2.surface);
      if (best_match.compound_base.size() >= matched_v2_surface.size() &&
          best_match.compound_base.compare(best_match.compound_base.size() - matched_v2_surface.size(),
                                           matched_v2_surface.size(), matched_v2_surface) == 0) {
        hira_compound_base =
            best_match.compound_base.substr(0, best_match.compound_base.size() - matched_v2_surface.size());
        hira_compound_base += matched_v2_reading;
      }
    }

    // If kanji te-stem didn't match and V2 was matched via hiragana reading,
    // also try a hiragana te-stem. This handles cases like:
    // 演じきった (input uses hiragana き, not kanji 切)
    if (!added_te_stem && !hira_compound_base.empty()) {
      auto [hira_te_stem, hira_uses_de] =
          generateTeFormStem(hira_compound_base, "", matched_v2.verb_type, matched_v2.base_ending);
      addTeStemEdge(hira_te_stem);
    }

    // Generate a mizenkei candidate for a following passive/causative auxiliary:
    // 読み込まれる → 読み込ま|れる.
    // Without this, the compound verb passive form would be a single token
    // or split as 読み + 込まれる.
    {
      // Generate compound mizenkei: V1 renyokei + V2 mizenkei
      std::string mizenkei = generateMizenkei(best_match.compound_base, "", matched_v2.verb_type);
      // For V2 matched via hiragana reading, also try hiragana mizenkei
      std::string hira_mizenkei;
      if (!hira_compound_base.empty()) {
        hira_mizenkei = generateMizenkei(hira_compound_base, "", matched_v2.verb_type);
      }

      auto addMizenkeiEdge = [&](const std::string& stem) {
        const size_t stem_end_pos =
            admittedStemEnd(stem, compound_surface, text, start_byte, start_pos, codepoints.size());
        if (stem_end_pos == kStemNotAdmitted)
          return false;

        // Check that what follows attaches to mizenkei: a passive/causative
        // marker (れ/せ) or a negative auxiliary (な of ない/なけれ/なかっ, or
        // ず). Without the negative case the split path is missing and the
        // fully merged compound wins (話し合わなければ → one token instead of
        // 話し合わ|なけれ|ば).
        if (stem_end_pos < codepoints.size()) {
          char32_t next_char = codepoints[stem_end_pos];
          const char32_t after_next = stem_end_pos + 1 < codepoints.size() ? codepoints[stem_end_pos + 1] : U'\0';
          if (!isMizenkeiAuxiliaryStarter(next_char, after_next))
            return false;
        }

        // Use base cost without passive+te penalty
        float mizenkei_cost = base_cost + opts.compound_verb_bonus + opts.verified_v1_bonus;
        if ((compound_lemma_verified || compound_nominalization_verified) && stem_end_pos < codepoints.size() &&
            codepoints[stem_end_pos] == U'せ') {
          mizenkei_cost += bigram_cost::kStrongBonus;
        }
        lattice.addEdge(stem, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(stem_end_pos),
                        core::PartOfSpeech::Verb, mizenkei_cost, flags, compound_lemma, compound_conj_type,
                        core::CandidateOrigin::Unknown, 0.0F, "compound_mizenkei", core::ExtendedPOS::VerbMizenkei,
                        "compound_mizenkei");
        return true;
      };

      bool added_mizenkei = addMizenkeiEdge(mizenkei);
      if (!added_mizenkei && !hira_mizenkei.empty()) {
        addMizenkeiEdge(hira_mizenkei);
      }
    }
  }
}

}  // namespace suzume::analysis::compound_verb_detail
