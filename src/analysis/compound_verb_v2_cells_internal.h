#ifndef SUZUME_ANALYSIS_COMPOUND_VERB_V2_CELLS_INTERNAL_H_
#define SUZUME_ANALYSIS_COMPOUND_VERB_V2_CELLS_INTERNAL_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "join_compound_verb_internal.h"

namespace suzume::analysis::compound_verb_detail {

/**
 * @brief Inputs findCompoundVerbMatch shares with every V2 it tries
 *
 * Holds the caller's arguments plus the byte positions and V1 surface derived
 * from them once per call.
 */
struct CompoundMatchContext {
  std::string_view text;
  const std::vector<char32_t>& codepoints;
  const ByteOffsets& byte_offsets;
  size_t start_pos;
  const std::vector<normalize::CharType>& char_types;
  size_t kanji_end;
  size_t v2_start;
  char32_t base_ending;
  bool is_sokuonbin;
  bool is_ichidan;
  bool has_kanji_v2_after_bare_ichidan;
  bool dict_compound_v1;
  std::string_view dict_compound_v1_lemma;
  const dictionary::DictionaryManager& dict_manager;
  const grammar::Inflection& inflection;
  bool hiragana_v1;
  bool allow_closed_onbin_v1;
  size_t start_byte;
  size_t v2_start_byte;
  std::string_view v1_surface;
  // End of the hiragana run after v2_start that an inflected V2 may span.
  size_t v2_hiragana_end;
};

/**
 * @brief Which cell of one V2 entry spells the text at v2_start
 *
 * At most one matched_* cell is set; matched_len is the byte length it spells.
 */
struct V2FormMatch {
  V2FormMatch(const SubsidiaryVerb& verb, std::string_view surface, std::string_view reading)
      : v2_verb(verb), v2_surface(surface), v2_reading(reading) {}

  const SubsidiaryVerb& v2_verb;
  std::string_view v2_surface;
  std::string_view v2_reading;
  // Determine if this is a renyokei entry by checking if base_form != surface
  // Renyokei entries: 過ぎ (base 過ぎる), 出し (base 出す), etc.
  bool is_renyokei_entry = false;
  bool matched_kanji = false;
  bool matched_reading = false;
  bool matched_inflected = false;
  bool matched_kateikei = false;
  bool matched_potential = false;
  bool matched_renyokei_via_reading = false;
  size_t matched_len = 0;
  bool inflection_includes_aux = false;
  bool matched_renyokei = false;
  bool matched_volitional = false;
  bool matched_mizenkei = false;
  bool matched_imperative = false;
  bool matched_classical = false;
  bool classical_attributive = false;
  std::string classical_terminal;
};

/**
 * @brief Match one V2 entry's conjugation cells at v2_start
 * @return false when the entry does not spell the text there, or spells it
 *         only where a grammatical boundary owns the span
 */
bool matchV2Form(const CompoundMatchContext& ctx, V2FormMatch& form);

}  // namespace suzume::analysis::compound_verb_detail

#endif  // SUZUME_ANALYSIS_COMPOUND_VERB_V2_CELLS_INTERNAL_H_
