/**
 * Search indexer - build inverted index from Japanese text
 *
 * Demonstrates: tag generation, POS filtering, deduplication,
 * and batch processing of multiple documents.
 */
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "suzume.h"

struct Document {
  int id;
  std::string text;
};

int main() {
  suzume::SuzumeOptions opts;
  opts.lemmatize = true;

  suzume::Suzume analyzer(opts);

  // Sample documents
  std::vector<Document> docs = {
      {1, "東京都渋谷区で新しいカフェがオープンしました"},
      {2, "渋谷のカフェで美味しいコーヒーを飲みました"},
      {3, "新宿駅の近くにある図書館で本を読んでいます"},
  };

  // Build inverted index: tag -> [doc_ids]
  std::unordered_map<std::string, std::vector<int>> index;

  suzume::postprocess::TagGeneratorOptions tag_opts;
  tag_opts.pos_filter = suzume::postprocess::kTagPosNoun |
                        suzume::postprocess::kTagPosVerb;
  tag_opts.exclude_basic = true;
  tag_opts.use_lemma = true;
  tag_opts.min_tag_length = 2;

  for (const auto& doc : docs) {
    auto tags = analyzer.generateTags(doc.text, tag_opts);
    for (const auto& tag : tags) {
      index[tag.tag].push_back(doc.id);
    }
  }

  // Print index
  std::cout << "Inverted index:\n";
  for (const auto& [tag, doc_ids] : index) {
    std::cout << "  " << tag << " -> [";
    for (size_t i = 0; i < doc_ids.size(); ++i) {
      if (i > 0) std::cout << ", ";
      std::cout << doc_ids[i];
    }
    std::cout << "]\n";
  }

  return 0;
}
