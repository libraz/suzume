#include "postprocess/lemmatizer.h"

#include <algorithm>

#include "grammar/conjugation.h"

namespace suzume::postprocess {

Lemmatizer::Lemmatizer() = default;

Lemmatizer::Lemmatizer(const dictionary::DictionaryManager* dict_manager)
    : dict_manager_(dict_manager) {}

namespace {

// Verb endings and their base forms
struct VerbEnding {
  std::string_view suffix;
  std::string_view base;
};

// Common verb conjugation endings (simplified)
// NOTE: Order matters - longer patterns should come first
const VerbEnding kVerbEndings[] = {
    // Polite humble forms with おります (longest first)
    {"しております", "する"},  // している polite humble
    {"しておりました", "する"},  // していた polite humble
    {"いたしております", "いたす"},  // している super polite
    {"いたしておりました", "いたす"},  // していた super polite
    {"ております", "おる"},  // ている polite humble
    {"ておりました", "おる"},  // ていた polite humble
    {"おります", "おる"},  // いる polite humble

    // Compound verbs (longest first)
    {"ってしまった", "う"},
    {"ってしまった", "つ"},
    {"ってしまった", "る"},
    {"いてしまった", "く"},
    {"んでしまった", "む"},
    {"してしまった", "す"},
    {"てしまった", "る"},

    {"っておいた", "う"},
    {"っておいた", "つ"},
    {"っておいた", "る"},
    {"いておいた", "く"},
    {"んでおいた", "む"},
    {"しておいた", "す"},
    {"ておいた", "る"},

    {"ってみた", "う"},
    {"ってみた", "つ"},
    {"ってみた", "る"},
    {"いてみた", "く"},
    {"んでみた", "む"},
    {"してみた", "す"},
    {"てみた", "る"},

    {"ってきた", "う"},
    {"ってきた", "つ"},
    {"ってきた", "る"},
    {"いてきた", "く"},
    {"んできた", "む"},
    {"してきた", "す"},
    {"てきた", "る"},

    {"っていった", "う"},
    {"っていった", "つ"},
    {"っていった", "る"},
    {"いていった", "く"},
    {"んでいった", "む"},
    {"していった", "す"},
    {"ていった", "る"},

    // Passive forms
    {"われた", "う"},
    {"かれた", "く"},
    {"がれた", "ぐ"},
    {"された", "す"},
    {"たれた", "つ"},
    {"なれた", "ぬ"},
    {"まれた", "む"},
    {"ばれた", "ぶ"},
    {"られた", "る"},

    // Causative forms
    {"わせた", "う"},
    {"かせた", "く"},
    {"がせた", "ぐ"},
    {"させた", "す"},
    {"たせた", "つ"},
    {"なせた", "ぬ"},
    {"ませた", "む"},
    {"ばせた", "ぶ"},
    {"らせた", "る"},

    // Causative-passive forms
    {"わされた", "う"},
    {"かされた", "く"},
    {"がされた", "ぐ"},
    {"たされた", "つ"},
    {"なされた", "ぬ"},
    {"まされた", "む"},
    {"ばされた", "ぶ"},
    {"らされた", "る"},

    // Godan verbs
    {"った", "う"},
    {"った", "つ"},
    {"った", "る"},
    {"いた", "く"},
    {"いだ", "ぐ"},
    {"んだ", "む"},
    {"んだ", "ぶ"},
    {"んだ", "ぬ"},
    {"した", "す"},

    // Te-form
    {"って", "う"},
    {"って", "つ"},
    {"って", "る"},
    {"いて", "く"},
    {"いで", "ぐ"},
    {"んで", "む"},
    {"んで", "ぶ"},
    {"んで", "ぬ"},
    {"して", "す"},

    // Masu-form
    {"います", "う"},
    {"います", "く"},
    {"います", "す"},
    {"きます", "くる"},
    {"します", "する"},
    {"ます", "る"},

    // Nai-form
    {"わない", "う"},
    {"かない", "く"},
    {"さない", "す"},
    {"たない", "つ"},
    {"なない", "ぬ"},
    {"ばない", "ぶ"},
    {"まない", "む"},
    {"らない", "る"},
    {"がない", "ぐ"},
    {"ない", "る"},

    // Potential
    {"える", "う"},
    {"ける", "く"},
    {"せる", "す"},
    {"てる", "つ"},
    {"ねる", "ぬ"},
    {"べる", "ぶ"},
    {"める", "む"},
    {"れる", "る"},
    {"げる", "ぐ"},

    // Passive/Causative
    {"られる", "る"},
    {"させる", "する"},
};

// Adjective endings
const VerbEnding kAdjectiveEndings[] = {
    {"くない", "い"},
    {"かった", "い"},
    {"くて", "い"},
    {"く", "い"},
    {"さ", "い"},
};

}  // namespace

bool Lemmatizer::endsWith(std::string_view str, std::string_view suffix) {
  if (str.size() < suffix.size()) {
    return false;
  }
  return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string Lemmatizer::lemmatizeVerb(std::string_view surface) {
  for (const auto& ending : kVerbEndings) {
    if (endsWith(surface, ending.suffix)) {
      std::string result(surface.substr(0, surface.size() - ending.suffix.size()));
      result += ending.base;
      return result;
    }
  }
  return std::string(surface);
}

std::string Lemmatizer::lemmatizeAdjective(std::string_view surface) {
  for (const auto& ending : kAdjectiveEndings) {
    if (endsWith(surface, ending.suffix)) {
      std::string result(surface.substr(0, surface.size() - ending.suffix.size()));
      result += ending.base;
      return result;
    }
  }
  return std::string(surface);
}

bool Lemmatizer::verifyCandidateWithDictionary(
    const grammar::InflectionCandidate& candidate) const {
  if (dict_manager_ == nullptr) {
    return false;
  }

  // Look up the candidate base form in dictionary
  auto results = dict_manager_->lookup(candidate.base_form, 0);

  for (const auto& result : results) {
    if (result.entry == nullptr) {
      continue;
    }

    // Check if the entry matches exactly (same surface and is a verb/adjective)
    if (result.entry->surface != candidate.base_form) {
      continue;
    }

    // Check if POS is verb or adjective
    if (result.entry->pos != core::PartOfSpeech::Verb &&
        result.entry->pos != core::PartOfSpeech::Adjective) {
      continue;
    }

    // Verify conjugation type matches
    auto dict_verb_type = grammar::conjTypeToVerbType(result.entry->conj_type);
    if (dict_verb_type == candidate.verb_type) {
      return true;  // Exact match found
    }
  }

  return false;
}

std::string Lemmatizer::lemmatizeByGrammar(std::string_view surface) const {
  // First, check if surface itself is a base form in dictionary
  // (e.g., 差し上げる should return 差し上げる, not 差し上ぐ)
  if (dict_manager_ != nullptr) {
    auto results = dict_manager_->lookup(surface, 0);
    for (const auto& result : results) {
      if (result.entry != nullptr &&
          result.entry->surface == surface &&
          (result.entry->pos == core::PartOfSpeech::Verb ||
           result.entry->pos == core::PartOfSpeech::Adjective)) {
        // Surface is a valid base form in dictionary
        return std::string(surface);
      }
    }
  }

  // Get all candidates
  auto candidates = inflection_.analyze(surface);

  if (candidates.empty()) {
    return std::string(surface);
  }

  // If dictionary is available, try to find a verified candidate
  // For dictionary-verified candidates, use lower confidence threshold (0.3F)
  // Dictionary verification compensates for confidence penalties from heuristics
  // (e.g., all-kanji i-adjective stems like 面白 get penalized but are valid)
  if (dict_manager_ != nullptr) {
    for (const auto& candidate : candidates) {
      if (candidate.confidence > 0.3F && verifyCandidateWithDictionary(candidate)) {
        return candidate.base_form;
      }
    }
  }

  // Fall back to the best candidate if no dictionary match found
  const auto& best = candidates.front();
  if (!best.base_form.empty() && best.confidence > 0.5F) {
    return best.base_form;
  }

  return std::string(surface);
}

std::string Lemmatizer::lemmatize(const core::Morpheme& morpheme) const {
  // If lemma is already set and different from surface, use it
  // (lemma == surface means it's a default that may need re-derivation)
  if (!morpheme.lemma.empty() && morpheme.lemma != morpheme.surface) {
    return morpheme.lemma;
  }

  // Skip grammar-based lemmatization for non-conjugating POS
  // Particles, auxiliaries, conjunctions, adverbs, etc. don't conjugate
  switch (morpheme.pos) {
    case core::PartOfSpeech::Particle:
    case core::PartOfSpeech::Auxiliary:
    case core::PartOfSpeech::Conjunction:
    case core::PartOfSpeech::Adverb:
    case core::PartOfSpeech::Symbol:
    case core::PartOfSpeech::Other:
      // These don't conjugate - return surface as-is
      return morpheme.surface;
    default:
      break;
  }

  // Try grammar-based lemmatization for verbs and adjectives
  std::string grammar_result = lemmatizeByGrammar(morpheme.surface);
  // Grammar-based lemmatization is authoritative - use its result even if
  // it equals the surface (which means the surface is already a dictionary form)
  // Only fall back to rule-based if grammar analysis returned empty/failed
  if (!grammar_result.empty()) {
    return grammar_result;
  }

  // Fallback to rule-based for known POS (only if grammar failed)
  switch (morpheme.pos) {
    case core::PartOfSpeech::Verb:
      return lemmatizeVerb(morpheme.surface);
    case core::PartOfSpeech::Adjective:
      return lemmatizeAdjective(morpheme.surface);
    default:
      return morpheme.surface;
  }
}

void Lemmatizer::lemmatizeAll(std::vector<core::Morpheme>& morphemes) const {
  for (auto& morpheme : morphemes) {
    morpheme.lemma = lemmatize(morpheme);
    morpheme.conj_form = detectConjForm(morpheme.surface, morpheme.lemma, morpheme.pos);
  }
}

grammar::ConjForm Lemmatizer::detectConjForm(std::string_view surface,
                                             std::string_view lemma,
                                             core::PartOfSpeech pos) {
  // Only verbs and adjectives have conjugation forms
  if (pos != core::PartOfSpeech::Verb &&
      pos != core::PartOfSpeech::Adjective) {
    return grammar::ConjForm::Base;
  }

  // If surface equals lemma, it's the base form
  if (surface == lemma) {
    return grammar::ConjForm::Base;
  }

  // Check for negative forms (mizenkei)
  if (endsWith(surface, "ない") || endsWith(surface, "なかった") ||
      endsWith(surface, "ぬ") || endsWith(surface, "ず") ||
      endsWith(surface, "ません") || endsWith(surface, "なく") ||
      endsWith(surface, "なくて") || endsWith(surface, "なければ") ||
      endsWith(surface, "なきゃ") || endsWith(surface, "なくても")) {
    return grammar::ConjForm::Mizenkei;
  }

  // Check for passive/causative (mizenkei)
  if (endsWith(surface, "れる") || endsWith(surface, "られる") ||
      endsWith(surface, "せる") || endsWith(surface, "させる") ||
      endsWith(surface, "れた") || endsWith(surface, "られた") ||
      endsWith(surface, "せた") || endsWith(surface, "させた") ||
      endsWith(surface, "される") || endsWith(surface, "された")) {
    return grammar::ConjForm::Mizenkei;
  }

  // Check for volitional form (ishikei)
  if (endsWith(surface, "う") || endsWith(surface, "よう") ||
      endsWith(surface, "まい")) {
    // Distinguish from godan base form ending in う
    if (surface != lemma) {
      return grammar::ConjForm::Ishikei;
    }
  }

  // Check for conditional form (kateikei)
  if (endsWith(surface, "ば") || endsWith(surface, "れば")) {
    return grammar::ConjForm::Kateikei;
  }

  // Check for imperative form (meireikei)
  if (endsWith(surface, "ろ") || endsWith(surface, "よ") ||
      endsWith(surface, "なさい")) {
    // Check if it's likely an imperative
    if (surface.size() > 3 && surface != lemma) {
      return grammar::ConjForm::Meireikei;
    }
  }

  // Check for te-form onbin patterns (onbinkei)
  if (endsWith(surface, "って") || endsWith(surface, "いて") ||
      endsWith(surface, "いで") || endsWith(surface, "んで") ||
      endsWith(surface, "った") || endsWith(surface, "いた") ||
      endsWith(surface, "いだ") || endsWith(surface, "んだ")) {
    return grammar::ConjForm::Onbinkei;
  }

  // Check for renyokei (te-form, ta-form, masu-form, etc.)
  if (endsWith(surface, "て") || endsWith(surface, "で") ||
      endsWith(surface, "た") || endsWith(surface, "だ") ||
      endsWith(surface, "ます") || endsWith(surface, "ました") ||
      endsWith(surface, "まして") || endsWith(surface, "ている") ||
      endsWith(surface, "ていた") || endsWith(surface, "ておく") ||
      endsWith(surface, "てある") || endsWith(surface, "てみる") ||
      endsWith(surface, "てくる") || endsWith(surface, "ていく") ||
      endsWith(surface, "てしまう") || endsWith(surface, "ちゃう") ||
      endsWith(surface, "たい") || endsWith(surface, "たかった") ||
      endsWith(surface, "たら") || endsWith(surface, "たり") ||
      endsWith(surface, "きた") || endsWith(surface, "してる") ||
      endsWith(surface, "してた") || endsWith(surface, "しています") ||
      endsWith(surface, "していた") || endsWith(surface, "しました")) {
    return grammar::ConjForm::Renyokei;
  }

  // For i-adjectives
  if (pos == core::PartOfSpeech::Adjective) {
    if (endsWith(surface, "く") || endsWith(surface, "くて") ||
        endsWith(surface, "かった") || endsWith(surface, "ければ") ||
        endsWith(surface, "さ") || endsWith(surface, "そう")) {
      return grammar::ConjForm::Renyokei;
    }
  }

  // Default to renyokei for conjugated forms we couldn't classify
  if (surface != lemma) {
    return grammar::ConjForm::Renyokei;
  }

  return grammar::ConjForm::Base;
}

}  // namespace postprocess
