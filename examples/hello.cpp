// Minimal C++ consumer using the header-only wrapper (suzume/suzume.hpp).
//
// Build in-tree with -DSUZUME_BUILD_EXAMPLES=ON, or standalone against an
// installed package via examples/consumer/CMakeLists.txt.
#include <cstdio>
#include <string>
#include <vector>

#include "suzume/suzume.hpp"

int main(int argc, char** argv) {
  const std::string text = argc > 1 ? argv[1] : "東京都に住んでいます";

  auto tokenizer = suzume::Tokenizer::create();
  if (!tokenizer) {
    std::fprintf(stderr, "failed to create tokenizer: %s\n", tokenizer.error().message.c_str());
    return 1;
  }

  auto morphemes = tokenizer->analyze(text);
  if (!morphemes) {
    std::fprintf(stderr, "analysis failed: %s\n", morphemes.error().message.c_str());
    return 1;
  }
  std::printf("suzume %s: %zu morpheme(s)\n", suzume::version().c_str(), morphemes->size());
  for (const suzume::Morpheme& morph : *morphemes) {
    std::printf("  %s\t%s\t%s\n", morph.surface.c_str(), morph.pos.c_str(), morph.base_form.c_str());
  }

  auto tags = tokenizer->generateTags(text);
  if (!tags) {
    std::fprintf(stderr, "tag generation failed: %s\n", tags.error().message.c_str());
    return 1;
  }
  std::printf("tags:");
  for (const suzume::Tag& entry : *tags) {
    std::printf(" %s", entry.tag.c_str());
  }
  std::printf("\n");

  return morphemes->empty() ? 1 : 0;
}
