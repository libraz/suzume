// Must not compile: Tokenizer::lastErrorCode() was removed; read Expected::error().code.
#include "suzume/suzume.hpp"

int main() {
  return static_cast<int>(suzume::Tokenizer::lastErrorCode());
}
