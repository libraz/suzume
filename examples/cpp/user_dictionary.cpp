/**
 * User dictionary - load custom vocabulary for domain-specific text
 *
 * Demonstrates: loading a user dictionary from memory and comparing
 * analysis results before and after.
 */
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "suzume/suzume.hpp"

void printMorphemes(const std::vector<suzume::Morpheme>& morphemes) {
  for (const auto& morpheme : morphemes) {
    std::cout << "  " << morpheme.surface << " [" << morpheme.pos << "]";
    if (!morpheme.base_form.empty() && morpheme.base_form != morpheme.surface) {
      std::cout << " (base form: " << morpheme.base_form << ")";
    }
    std::cout << "\n";
  }
}

int main() {
  auto analyzer = suzume::Tokenizer::create();
  if (!analyzer) {
    std::cerr << "failed to create tokenizer: " << analyzer.error().message << "\n";
    return 1;
  }

  const std::string text = "青空りんご園の案内図を作りました";

  auto before = analyzer->analyze(text);
  if (!before) {
    std::cerr << "analysis failed: " << before.error().message << "\n";
    return 1;
  }
  std::cout << "Without user dictionary:\n";
  printMorphemes(*before);

  // TSV: surface<TAB>POS[<TAB>conj_type][<TAB>base_form]
  auto loaded = analyzer->loadUserDictionary("青空りんご園\tNOUN\n");
  if (!loaded) {
    std::cerr << "failed to load in-memory user dictionary: " << loaded.error().message << "\n";
    return 1;
  }
  std::cout << "\nDictionary loaded: " << *loaded << "\n";

  auto after = analyzer->analyze(text);
  if (!after) {
    std::cerr << "analysis failed: " << after.error().message << "\n";
    return 1;
  }
  std::cout << "\nWith user dictionary:\n";
  printMorphemes(*after);
  const bool found_user_entry =
      std::any_of(after->begin(), after->end(), [](const auto& morpheme) { return morpheme.is_user_dict; });
  if (!found_user_entry) {
    std::cerr << "loaded user dictionary entry was not used\n";
    return 1;
  }
  return 0;
}
