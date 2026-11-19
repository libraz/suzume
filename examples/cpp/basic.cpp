/**
 * Basic tokenization example
 *
 * Build from the project root with `make examples`, or as an installed
 * consumer through `examples/consumer`.
 */
#include <iostream>

#include "suzume/suzume.hpp"

int main() {
  auto analyzer = suzume::Tokenizer::create();
  if (!analyzer) {
    std::cerr << "failed to create tokenizer: " << analyzer.error().message << "\n";
    return 1;
  }

  auto morphemes = analyzer->analyze("すもももももももものうち");
  if (!morphemes) {
    std::cerr << "analysis failed: " << morphemes.error().message << "\n";
    return 1;
  }
  if (morphemes->empty()) {
    std::cerr << "analysis returned no morphemes\n";
    return 1;
  }

  for (const auto& morpheme : *morphemes) {
    std::cout << morpheme.surface << "\t" << morpheme.pos << "\t" << morpheme.base_form << "\n";
  }
  return 0;
}
