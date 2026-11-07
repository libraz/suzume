#ifndef SUZUME_GRAMMAR_DICTIONARY_EXPANSION_H_
#define SUZUME_GRAMMAR_DICTIONARY_EXPANSION_H_

#include <cstddef>
#include <vector>

#include "dictionary/dictionary.h"
#include "dictionary/source_parser.h"

namespace suzume {
namespace grammar {

struct DictionaryExpansionResult {
  std::vector<dictionary::DictionaryEntry> entries;
  size_t expanded_forms{0};
  size_t duplicates_skipped{0};
};

struct DictionaryExpansionOptions {
  // Binary dictionaries retain distinct POS readings for one surface, while
  // collapsing competing expanded forms inside one broad POS deterministically.
  bool preserve_same_pos_homographs{true};
  // Explicit source homographs are lexical evidence. Inflected forms which
  // merely collide after expansion are not, and binary dictionaries collapse
  // those generated collisions to the stable first entry.
  bool preserve_generated_surface_homographs{true};
};

/**
 * @brief Describe an explicit conjugation marker that cannot inflect surface.
 *
 * An empty result means that the marker and surface agree.  The check is kept
 * beside expansion so every source-dictionary path can preserve an invalid
 * surface literally instead of deriving words from an arbitrary truncated
 * stem.  CLI validation presents this diagnostic to dictionary authors.
 */
std::string dictionaryConjugationTypeIssue(const dictionary::SourceEntry& source_entry);

/**
 * @brief Expand explicitly typed dictionary source bases into lexical forms.
 *
 * Entries without an explicit verb or i-adjective conjugation marker are kept
 * as one literal surface. This preserves legacy CSV entries whose surface may
 * already be inflected.
 */
std::vector<dictionary::DictionaryEntry> expandDictionarySourceEntry(const dictionary::SourceEntry& source_entry);

/**
 * @brief Expand and exactly deduplicate a dictionary source batch.
 *
 * Non-conjugating entries are processed first to retain stable registration
 * order. Only identical (surface, POS, extended POS, lemma) tuples are removed;
 * grammatical homographs remain available to runtime source dictionaries.
 */
DictionaryExpansionResult expandDictionarySourceEntries(const std::vector<dictionary::SourceEntry>& source_entries,
                                                        DictionaryExpansionOptions options = {});

}  // namespace grammar
}  // namespace suzume

#endif  // SUZUME_GRAMMAR_DICTIONARY_EXPANSION_H_
