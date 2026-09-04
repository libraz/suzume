/**
 * @file join_compound_verb_hiragana.cpp
 * @brief Hiragana compound-verb join candidate generation
 */
#include "analysis/dictionary_probe.h"
#include "join_compound_verb_internal.h"

namespace suzume::analysis {

using namespace compound_verb_detail;

namespace {

// A particle-like first mora normally owns the boundary (も+上がる).  The
// narrow exception is a complete V1+V2 onbin construction whose two verbs are
// independently evidenced: an exact Ichidan V1 analysis and a dictionary-
// attested closed V2 followed by its promised た/て form.  Returning the split
// position lets both particle guards use the same proof instead of weakening
// particle handling for an entire word or lemma.
size_t findParticleInitialClosedOnbinSplit(std::string_view text, const std::vector<char32_t>& codepoints,
                                           const ByteOffsets& byte_offsets, size_t start_pos,
                                           const std::vector<normalize::CharType>& char_types,
                                           const dictionary::DictionaryManager& dict_manager,
                                           const grammar::Inflection& inflection, bool has_left_predicate_boundary) {
  if (!has_left_predicate_boundary || start_pos >= codepoints.size()) {
    return codepoints.size();
  }

  if (lookupEntryInRange(dict_manager, codepoints, start_pos, start_pos + 1, core::PartOfSpeech::Particle) == nullptr) {
    return codepoints.size();
  }

  const size_t start_byte = byteOffsetAt(byte_offsets, start_pos);
  for (size_t v1_len = 3; v1_len <= 4; ++v1_len) {
    const size_t v2_start = start_pos + v1_len;
    if (v2_start >= codepoints.size()) {
      break;
    }
    bool all_hiragana = true;
    for (size_t pos = start_pos; pos < v2_start; ++pos) {
      if (char_types[pos] != CharType::Hiragana) {
        all_hiragana = false;
        break;
      }
    }
    if (!all_hiragana || !grammar::isERowCodepoint(codepoints[v2_start - 1])) {
      continue;
    }

    const size_t v2_start_byte = byteOffsetAt(byte_offsets, v2_start);
    const std::string_view v1_surface = text.substr(start_byte, v2_start_byte - start_byte);
    if (verb_helpers::hasNonVerbDictionaryEntry(&dict_manager, v1_surface)) {
      continue;
    }
    const std::string v1_base = normalize::concat(v1_surface, "る");
    bool has_exact_ichidan = false;
    for (const auto& analysis : inflection.analyze(v1_surface)) {
      if (analysis.verb_type == grammar::VerbType::Ichidan && analysis.base_form == v1_base &&
          analysis.confidence >= candidate::verb_cost::kClosedOnbinCompoundV1MinConfidence) {
        has_exact_ichidan = true;
        break;
      }
    }
    if (!has_exact_ichidan) {
      continue;
    }

    for (const auto& v2_verb : subsidiaryVerbs()) {
      if (!v2_verb.joins_general || v2_verb.reading == nullptr || v2_verb.verb_type != V2VerbType::Godan ||
          dict_manager.lookupExact(v2_verb.reading, core::PartOfSpeech::Verb) == nullptr) {
        continue;
      }
      const auto [te_stem, uses_de] = generateTeFormStem(v2_verb.reading, "", v2_verb.verb_type, v2_verb.base_ending);
      if (te_stem.size() <= core::kJapaneseCharBytes ||
          v2_start_byte + te_stem.size() + core::kJapaneseCharBytes > text.size() ||
          text.substr(v2_start_byte, te_stem.size()) != te_stem) {
        continue;
      }
      const std::string_view next = text.substr(v2_start_byte + te_stem.size(), core::kJapaneseCharBytes);
      if (uses_de ? (next == "で" || next == "だ") : (next == "て" || next == "た")) {
        return v2_start;
      }
    }
  }
  return codepoints.size();
}

}  // namespace

void addHiraganaCompoundVerbJoinCandidates(core::Lattice& lattice, std::string_view text,
                                           const std::vector<char32_t>& codepoints, const ByteOffsets& byte_offsets,
                                           size_t start_pos, const std::vector<normalize::CharType>& char_types,
                                           const dictionary::DictionaryManager& dict_manager, const Scorer& scorer,
                                           const grammar::Inflection& inflection) {
  if (start_pos >= char_types.size()) {
    return;
  }

  // Must start with hiragana (for all-hiragana compound verbs like やりなおす)
  if (char_types[start_pos] != CharType::Hiragana) {
    return;
  }

  // The irregular サ変 continuative し may begin a hiragana compound at a
  // real boundary (し続ける). Directly after kanji, however, starting at し
  // would begin inside the preceding predicate's okurigana and fabricate a
  // competing suffix compound (押し下げ -> 押 + し下げ). The kanji-starting
  // compound generator owns the complete span in that context.
  if (codepoints[start_pos] == U'し' && start_pos > 0 && char_types[start_pos - 1] == CharType::Kanji) {
    return;
  }

  const bool has_left_predicate_boundary = start_pos == 0 ||
                                           normalize::classifyChar(codepoints[start_pos - 1]) == CharType::Symbol ||
                                           normalize::isExtendedParticle(codepoints[start_pos - 1]);
  const size_t particle_initial_onbin_split = findParticleInitialClosedOnbinSplit(
      text, codepoints, byte_offsets, start_pos, char_types, dict_manager, inflection, has_left_predicate_boundary);
  if ((verb_helpers::startsInsideDictionaryParticle(codepoints, start_pos, &dict_manager) ||
       verb_helpers::startsWithMultiMoraDictionaryParticle(codepoints, start_pos, &dict_manager)) &&
      particle_initial_onbin_split == codepoints.size()) {
    return;
  }

  // Do not reinterpret a closed particle plus an independently attested verb
  // as a hiragana V1+V2 compound (結果+と+ひきかえる). A whole dictionary verb
  // at the same position still wins this ambiguity (できる, not で+きる).
  size_t hiragana_end = start_pos;
  while (hiragana_end < char_types.size() && char_types[hiragana_end] == CharType::Hiragana) {
    ++hiragana_end;
  }
  if (start_pos + 2 < hiragana_end) {
    const auto* particle =
        lookupEntryInRange(dict_manager, codepoints, start_pos, start_pos + 1, core::PartOfSpeech::Particle);
    if (particle != nullptr && particle->extended_pos != core::ExtendedPOS::ParticleFinal &&
        lookupEntryInRange(dict_manager, codepoints, start_pos, hiragana_end, core::PartOfSpeech::Verb) == nullptr &&
        lookupEntryInRange(dict_manager, codepoints, start_pos + 1, hiragana_end, core::PartOfSpeech::Verb) !=
            nullptr) {
      return;
    }
  }

  const size_t start_byte = byteOffsetAt(byte_offsets, start_pos);
  const size_t min_v1_len = codepoints[start_pos] == U'し' ? 1 : 2;
  for (size_t v1_len = min_v1_len; v1_len <= 4; ++v1_len) {
    const size_t v2_start = start_pos + v1_len;
    if (v2_start >= codepoints.size()) {
      break;
    }
    bool all_hiragana = true;
    for (size_t pos = start_pos; pos < v2_start; ++pos) {
      if (char_types[pos] != CharType::Hiragana) {
        all_hiragana = false;
        break;
      }
    }
    if (!all_hiragana) {
      continue;
    }

    const size_t v2_start_byte = byteOffsetAt(byte_offsets, v2_start);
    const std::string_view v1_surface = text.substr(start_byte, v2_start_byte - start_byte);
    const char32_t v1_tail = codepoints[v2_start - 1];
    const char32_t base_ending = godanRenyokeiBaseCp(v1_tail);
    const bool is_ichidan = base_ending == 0;

    const bool closed_onbin_context = has_left_predicate_boundary && v2_start == particle_initial_onbin_split;
    const char32_t first_char = codepoints[start_pos];
    if ((first_char == U'を' || first_char == U'が' || first_char == U'は' || first_char == U'に' ||
         first_char == U'で' || first_char == U'へ' || first_char == U'の' || first_char == U'も') &&
        !closed_onbin_context) {
      continue;
    }

    if (!grammar::isSuruRenyokeiSurface(v1_surface) &&
        dict_manager.lookupExact(v1_surface, core::PartOfSpeech::Particle) != nullptr) {
      continue;
    }

    // A compound joins a continuative straight to its V2, so nothing closed
    // stands between them. A conjunctive particle at the tail of V1 is exactly
    // such a boundary — it ends the clause the V1 belongs to — and the reading
    // that fits is the particle's own (惜しみ + つつ + 歩く, not the compound
    // みつつ歩く headed by the non-word みつ). The particle needs a predicate to
    // attach to, so this only holds where the mora in front of it is one a
    // continuative ends in: the i-row for a Godan verb, the e-row for an
    // Ichidan one. Elsewhere the same kana are part of a single stem and the
    // compound stands (わたり+あるく, where わ opens no cell at all).
    bool v1_embeds_conjunctive_particle = false;
    for (size_t particle_start = start_pos + 1; particle_start + 1 < v2_start; ++particle_start) {
      const char32_t host_tail = codepoints[particle_start - 1];
      if (!grammar::isIRowCodepoint(host_tail) && !grammar::isERowCodepoint(host_tail)) {
        continue;
      }
      const auto* particle =
          lookupEntryInRange(dict_manager, codepoints, particle_start, v2_start, core::PartOfSpeech::Particle);
      if (particle != nullptr && particle->extended_pos == core::ExtendedPOS::ParticleConj) {
        v1_embeds_conjunctive_particle = true;
        break;
      }
    }
    if (v1_embeds_conjunctive_particle) {
      continue;
    }

    // An e-row form after topic は is an independently closed Godan
    // conditional when its reconstructed lemma is attested.
    if (is_ichidan && grammar::isERowCodepoint(v1_tail) && start_pos > 0 && codepoints[start_pos - 1] == U'は') {
      const std::string_view godan_suffix = grammar::godanBaseSuffixFromERow(v1_tail);
      if (!godan_suffix.empty()) {
        std::string godan_base(v1_surface.substr(0, v1_surface.size() - core::kJapaneseCharBytes));
        godan_base += godan_suffix;
        if (dict_manager.lookupExact(godan_base, core::PartOfSpeech::Verb) != nullptr) {
          continue;
        }
      }
    }

    CompoundVerbMatch match = findCompoundVerbMatch(text, codepoints, byte_offsets, start_pos, char_types, v2_start - 1,
                                                    v2_start, base_ending, false, is_ichidan, false, false, "",
                                                    dict_manager, inflection, true, closed_onbin_context);
    if (match.v2_verb == nullptr) {
      continue;
    }

    const size_t compound_end_byte = v2_start_byte + match.matched_len;
    const std::string compound_surface(text.substr(start_byte, compound_end_byte - start_byte));
    if (utf8::endsWith(compound_surface, "かっ")) {
      const auto adjective_past = inflection.getBest(compound_surface + "た");
      if (adjective_past.verb_type == grammar::VerbType::IAdjective &&
          adjective_past.confidence >= candidate::kIAdjConfMin) {
        continue;
      }
    }

    emitCompoundVerbCandidates(lattice, text, codepoints, byte_offsets, start_pos, v2_start, match, dict_manager,
                               scorer);
    return;
  }
}

}  // namespace suzume::analysis
