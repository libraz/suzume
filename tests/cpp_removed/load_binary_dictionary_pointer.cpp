// Must not compile: loadBinaryDictionary(const uint8_t*, size_t) was removed; pass the bytes as std::string_view.
#include "suzume/suzume.hpp"

int main() {
  auto created = suzume::Tokenizer::create();
  if (!created) {
    return 1;
  }
  const std::uint8_t bytes[] = {0, 1, 2, 3};
  return created->loadBinaryDictionary(bytes, sizeof(bytes)) ? 0 : 1;
}
