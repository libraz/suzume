// Must not compile: Tokenizer::valid() was removed; Tokenizer::create() reports failure.
#include "suzume/suzume.hpp"

int main() {
  auto created = suzume::Tokenizer::create();
  if (!created) {
    return 1;
  }
  return created->valid() ? 0 : 1;
}
