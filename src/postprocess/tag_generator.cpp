#include "postprocess/tag_generator.h"

#include <algorithm>
#include <utility>

#include "grammar/char_patterns.h"
#include "normalize/utf8.h"

namespace suzume::postprocess {

TagGenerator::TagGenerator(const TagGeneratorOptions& options) : options_(options) {}

size_t TagGenerator::countChars(std::string_view str) {
  return normalize::utf8Length(str);
}

bool TagGenerator::shouldInclude(const core::Morpheme& morpheme) const {
  // Exclude by POS
  if (options_.exclude_particles && morpheme.pos == core::PartOfSpeech::Particle) {
    return false;
  }

  if (options_.exclude_auxiliaries && morpheme.pos == core::PartOfSpeech::Auxiliary) {
    return false;
  }

  // Exclude formal nouns
  if (options_.exclude_formal_nouns && morpheme.isFormalNoun()) {
    return false;
  }

  // Exclude low info words
  if (options_.exclude_low_info && morpheme.isLowInformation()) {
    return false;
  }

  // POS filter (whitelist); categories without a bit (conjunctions, symbols, ...) are excluded.
  // Zero means every filterable content-word category, plus particles/auxiliaries only when their explicit include
  // flags allow it.
  uint8_t pos_bit = 0;
  switch (morpheme.pos) {
    case core::PartOfSpeech::Noun:
    case core::PartOfSpeech::Pronoun:
      pos_bit = kTagPosNoun;
      break;
    case core::PartOfSpeech::Verb:
      pos_bit = kTagPosVerb;
      break;
    case core::PartOfSpeech::Adjective:
      pos_bit = kTagPosAdjective;
      break;
    case core::PartOfSpeech::Adverb:
      pos_bit = kTagPosAdverb;
      break;
    case core::PartOfSpeech::Particle:
      pos_bit = kTagPosParticle;
      break;
    case core::PartOfSpeech::Auxiliary:
      pos_bit = kTagPosAuxiliary;
      break;
    default:
      return false;
  }
  if (options_.pos_filter != 0 &&
      (options_.pos_filter & pos_bit) == 0) {  // NOLINT(hicpp-signed-bitwise): bit flag operation
    return false;
  }

  // Exclude basic words (hiragana-only lemma)
  if (options_.exclude_basic && grammar::isPureHiragana(morpheme.getLemma())) {
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

std::vector<TagEntry> TagGenerator::generate(const std::vector<core::Morpheme>& morphemes) const {
  std::vector<TagEntry> tags;

  for (const auto& morpheme : morphemes) {
    if (!shouldInclude(morpheme)) {
      continue;
    }

    std::string tag = getTagString(morpheme);

    // Check minimum length
    if (countChars(tag) < options_.min_tag_length) {
      continue;
    }

    // Check for duplicates
    if (options_.remove_duplicates &&
        std::any_of(tags.begin(), tags.end(), [&tag](const TagEntry& entry) { return entry.tag == tag; })) {
      continue;
    }

    tags.push_back({std::move(tag), morpheme.pos});

    // Check max tags
    if (options_.max_tags > 0 && tags.size() >= options_.max_tags) {
      break;
    }
  }

  return tags;
}

}  // namespace suzume::postprocess
