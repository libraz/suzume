/**
 * Tag generation example - extract content word tags for indexing/search
 *
 * Build (from project root):
 *   cmake -B build && cmake --build build --parallel
 *   g++ -std=c++17 -Isrc -Lbuild/lib -o tags examples/cpp/tags.cpp \
 *     -lsuzume -lsuzume_analysis -lsuzume_postprocess -lsuzume_grammar \
 *     -lsuzume_dictionary -lsuzume_core -lsuzume_normalize -lsuzume_pretokenizer
 */
#include <iostream>

#include "core/types.h"
#include "suzume.h"

int main() {
  suzume::Suzume analyzer;

  // Generate tags (content words suitable for search indexing)
  auto tags = analyzer.generateTags("東京都渋谷区で開催されたイベントに参加しました");

  std::cout << "Tags:\n";
  for (const auto& tag : tags) {
    std::cout << "  " << tag.tag << " ("
              << suzume::core::posToString(tag.pos) << ")\n";
  }

  // With custom options: nouns only, exclude basic words
  suzume::postprocess::TagGeneratorOptions opts;
  opts.pos_filter = suzume::postprocess::kTagPosNoun;
  opts.exclude_basic = true;

  auto noun_tags = analyzer.generateTags(
      "東京都渋谷区で開催されたイベントに参加しました", opts);

  std::cout << "\nNoun tags (excluding basic):\n";
  for (const auto& tag : noun_tags) {
    std::cout << "  " << tag.tag << "\n";
  }

  return 0;
}
