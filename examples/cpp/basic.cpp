/**
 * Basic morphological analysis example
 *
 * Build (from project root):
 *   cmake -B build && cmake --build build --parallel
 *   g++ -std=c++17 -Isrc -Lbuild/lib -o basic examples/cpp/basic.cpp \
 *     -lsuzume -lsuzume_analysis -lsuzume_postprocess -lsuzume_grammar \
 *     -lsuzume_dictionary -lsuzume_core -lsuzume_normalize -lsuzume_pretokenizer
 */
#include <iostream>

#include "core/types.h"
#include "suzume.h"

int main() {
  suzume::Suzume analyzer;

  // Analyze Japanese text
  auto morphemes = analyzer.analyze("すもももももももものうち");

  for (const auto& m : morphemes) {
    std::cout << m.surface << "\t" << suzume::core::posToString(m.pos) << "\t"
              << m.getLemma() << "\n";
  }

  return 0;
}
