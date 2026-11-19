/**
 * Tag generation example - extract content word tags for indexing/search
 *
 * Build from the project root with `make examples`, or as an installed
 * consumer through `examples/consumer`.
 */
#include <iostream>
#include <string>

#include "suzume/suzume.hpp"

int main() {
  auto analyzer = suzume::Tokenizer::create();
  if (!analyzer) {
    std::cerr << "failed to create tokenizer: " << analyzer.error().message << "\n";
    return 1;
  }

  const std::string text = "東京都渋谷区で開催されたイベントに参加しました";

  // Content words suitable for search indexing
  auto tags = analyzer->generateTags(text);
  if (!tags) {
    std::cerr << "tag generation failed: " << tags.error().message << "\n";
    return 1;
  }
  if (tags->empty()) {
    std::cerr << "tag generation returned no tags\n";
    return 1;
  }
  std::cout << "Tags:\n";
  for (const auto& tag : *tags) {
    std::cout << "  " << tag.tag << " (" << tag.pos << ")\n";
  }

  // Nouns only, excluding basic words
  suzume::TagOptions opts;
  opts.pos_filter = {suzume::TagPos::Noun};
  opts.exclude_basic = true;

  auto noun_tags = analyzer->generateTags(text, opts);
  if (!noun_tags) {
    std::cerr << "noun tag generation failed: " << noun_tags.error().message << "\n";
    return 1;
  }
  if (noun_tags->empty()) {
    std::cerr << "noun tag generation returned no tags\n";
    return 1;
  }
  std::cout << "\nNoun tags (excluding basic):\n";
  for (const auto& tag : *noun_tags) {
    std::cout << "  " << tag.tag << "\n";
  }
  return 0;
}
