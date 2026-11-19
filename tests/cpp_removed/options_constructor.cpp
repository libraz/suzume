// Must not compile: The public Tokenizer(Options) constructor was removed; use Tokenizer::create(options).
#include "suzume/suzume.hpp"

int main() {
  suzume::Tokenizer tokenizer(suzume::Options{});
  return tokenizer.mode() == suzume::Mode::Normal ? 0 : 1;
}
