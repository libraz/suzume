// Must not compile: Tokenizer::loadUserDictionaryCount() was removed; loadUserDictionary() returns the count.
#include "suzume/suzume.hpp"

int main() {
  auto created = suzume::Tokenizer::create();
  if (!created) {
    return 1;
  }
  return static_cast<int>(created->loadUserDictionaryCount("word\tNOUN\n"));
}
