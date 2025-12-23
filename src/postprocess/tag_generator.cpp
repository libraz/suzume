#include "postprocess/tag_generator.h"

#include "normalize/utf8.h"

namespace suzume::postprocess {

TagGenerator::TagGenerator(const TagGeneratorOptions& options) : options_(options) {}

size_t TagGenerator::countChars(std::string_view str) {
  return normalize::utf8Length(str);
}

bool TagGenerator::shouldInclude(const core::Morpheme& morpheme) const {
  // Exclude by POS
  if (options_.exclude_particles &&
      morpheme.pos == core::PartOfSpeech::Particle) {
    return false;
  }

  if (options_.exclude_auxiliaries &&
      morpheme.pos == core::PartOfSpeech::Auxiliary) {
    return false;
  }

  // Exclude formal nouns
  if (options_.exclude_formal_nouns && morpheme.features.is_formal_noun) {
    return false;
  }

  // Exclude low info words
  if (options_.exclude_low_info && morpheme.features.is_low_info) {
    return false;
  }

  // Exclude conjunctions (typically not useful as tags)
  if (morpheme.pos == core::PartOfSpeech::Conjunction) {
    return false;
  }

  // Exclude symbols
  if (morpheme.pos == core::PartOfSpeech::Symbol) {
    return false;
  }

  return true;
}

std::string TagGenerator::getTagString(const core::Morpheme& morpheme) const {
  if (options_.use_lemma && !morpheme.lemma.empty()) {
    return morpheme.lemma;
  }
  return morpheme.surface;
}

std::vector<std::string> TagGenerator::generate(
    const std::vector<core::Morpheme>& morphemes) const {
  // Post-process morphemes
  auto processed = postprocessor_.process(morphemes);

  std::vector<std::string> tags;
  std::unordered_set<std::string> seen;

  for (const auto& morpheme : processed) {
    if (!shouldInclude(morpheme)) {
      continue;
    }

    std::string tag = getTagString(morpheme);

    // Check minimum length
    if (countChars(tag) < options_.min_tag_length) {
      continue;
    }

    // Check for duplicates
    if (options_.remove_duplicates) {
      if (seen.find(tag) != seen.end()) {
        continue;
      }
      seen.insert(tag);
    }

    tags.push_back(tag);

    // Check max tags
    if (options_.max_tags > 0 && tags.size() >= options_.max_tags) {
      break;
    }
  }

  return tags;
}

std::vector<std::string> TagGenerator::generateFromText(std::string_view /*text*/) {
  // This would require access to Analyzer
  // For now, return empty - caller should use Analyzer + generate()
  return {};
}

}  // namespace postprocess
