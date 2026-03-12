/**
 * User dictionary - load custom vocabulary for domain-specific text
 *
 * Demonstrates: user dictionary loading from file and from memory,
 * before/after comparison of analysis results.
 */
#include <iostream>

#include "suzume.h"

void printMorphemes(const std::vector<suzume::core::Morpheme>& morphemes) {
  for (const auto& m : morphemes) {
    std::cout << "  " << m.surface << " ["
              << suzume::core::posToString(m.pos) << "]";
    if (!m.lemma.empty() && m.lemma != m.surface) {
      std::cout << " (lemma: " << m.lemma << ")";
    }
    std::cout << "\n";
  }
}

int main() {
  suzume::SuzumeOptions opts;
  opts.skip_user_dictionary = true;  // Start clean
  suzume::Suzume analyzer(opts);

  std::string text = "初音ミクのコスプレ衣装を作りました";

  // Without user dictionary
  std::cout << "Without user dictionary:\n";
  printMorphemes(analyzer.analyze(text));

  // Load user dictionary from memory (TSV format: surface\tPOS\treading)
  std::string dict_data = "初音ミク\tNOUN\tはつねみく\n";
  if (analyzer.loadUserDictionaryFromMemory(dict_data.data(),
                                            dict_data.size())) {
    std::cout << "\nWith user dictionary:\n";
    printMorphemes(analyzer.analyze(text));
  }

  // Or load from file:
  // analyzer.loadUserDictionary("/path/to/user.tsv");

  return 0;
}
