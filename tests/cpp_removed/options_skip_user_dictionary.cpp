// Must not compile: Options::skip_user_dictionary was renamed to skip_bundled_user_dictionary.
#include "suzume/suzume.hpp"

int main() {
  suzume::Options options;
  options.skip_user_dictionary = true;
  return options.preserve_vu ? 0 : 1;
}
