// Must compile: the replacement for every removed name, so a failing removed-name
// build proves the name is gone rather than that the setup is broken.
#include <string_view>

#include "suzume/suzume.hpp"

int main() {
  suzume::Options options;
  options.skip_bundled_user_dictionary = true;
  options.scorer_options = "{}";
  auto created = suzume::Tokenizer::create(options);
  if (!created) {
    return created.error().code == suzume::ErrorCode::Success ? 1 : 2;
  }
  suzume::Tokenizer tokenizer = std::move(created).value();
  suzume::TagOptions tag_options;
  tag_options.use_base_form = false;
  const auto loaded = tokenizer.loadUserDictionary("word\tNOUN\n");
  const auto binary = tokenizer.loadBinaryDictionary(std::string_view("\x00\x01\x02\x03", 4));
  const auto morphemes = tokenizer.analyze("word");
  const std::size_t base_forms = morphemes ? morphemes->size() : 0;
  const auto tags = tokenizer.generateTags("word", tag_options);
  return (loaded && !binary && tags && base_forms > 0 && !suzume::version().empty()) ? 0 : 3;
}
