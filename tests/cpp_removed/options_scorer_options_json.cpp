// Must not compile: Options::scorer_options_json was renamed to scorer_options.
#include "suzume/suzume.hpp"

int main() {
  suzume::Options options;
  options.scorer_options_json = "{}";
  return options.preserve_vu ? 0 : 1;
}
