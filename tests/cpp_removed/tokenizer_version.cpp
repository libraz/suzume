// Must not compile: Tokenizer::version() was removed; use the free suzume::version().
#include "suzume/suzume.hpp"

int main() {
  return static_cast<int>(suzume::Tokenizer::version().size());
}
