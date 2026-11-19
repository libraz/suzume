// Must not compile: TagOptions::use_lemma was renamed to use_base_form.
#include "suzume/suzume.hpp"

int main() {
  suzume::TagOptions options;
  options.use_lemma = false;
  return options.min_length == 0 ? 1 : 0;
}
