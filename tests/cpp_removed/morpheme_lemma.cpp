// Must not compile: Morpheme::lemma was removed; use base_form.
#include "suzume/suzume.hpp"

int main() {
  suzume::Morpheme morpheme;
  return static_cast<int>(morpheme.lemma.size());
}
