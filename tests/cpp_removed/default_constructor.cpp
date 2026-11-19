// Must not compile: The public Tokenizer() constructor was removed; use Tokenizer::create().
#include "suzume/suzume.hpp"

int main() {
  suzume::Tokenizer tokenizer;
  return tokenizer.mode() == suzume::Mode::Normal ? 0 : 1;
}
