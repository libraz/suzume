// Must not compile: Tokenizer::lastError() was removed; read Expected::error().
#include "suzume/suzume.hpp"

int main() {
  return static_cast<int>(suzume::Tokenizer::lastError().size());
}
