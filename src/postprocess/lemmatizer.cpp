#include "postprocess/lemmatizer.h"

#include <algorithm>

#include "core/utf8_constants.h"
#include "grammar/conjugation.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"

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
    {"そうだった", "い"},
    {"そうです", "い"},
    {"そうだ", "い"},
    {"そうに", "い"},
    {"そうな", "い"},
    {"そう", "い"},
    {"くなかった", "い"},
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
  // B45: Special handling for ない adjective + さ + そう pattern
  // なさそう = ない + さ + そう (looks like there isn't)
  // Without this, lemmatizer would incorrectly return なさい (from そう → い rule)
  // This pattern also covers: なさそうな, なさそうに, なさそうだ, etc.
  if (surface.find("なさそう") == 0) {
    // Replace なさそう... with ない
    return "ない";
  }
  // Also handle なさ alone (noun form of ない)
  if (surface == "なさ") {
    return "ない";
  }

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

  bool found_verb_or_adj = false;
  bool type_matches = false;

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

    // Found a verb/adjective with matching surface
    found_verb_or_adj = true;

    // Verify conjugation type matches
    auto dict_verb_type = grammar::conjTypeToVerbType(result.entry->conj_type);
    if (dict_verb_type == candidate.verb_type) {
      type_matches = true;
      break;  // Exact match found
    }
  }

  // Accept if base_form exists as verb/adjective in dictionary
  // Type mismatch is acceptable - inflection analysis may have wrong type
  // but dictionary presence validates the base_form itself
  // e.g., 見せられた → base="見せる" with wrong type=GodanRa should still
  // be accepted because 見せる exists as Ichidan verb in dictionary
  return found_verb_or_adj;
}

std::string Lemmatizer::lemmatizeByGrammar(std::string_view surface,
                                            core::PartOfSpeech pos,
                                            dictionary::ConjugationType conj_type) const {
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

  // Filter candidates by POS if specified
  // For Adjective POS, only accept IAdjective verb_type
  // This prevents 美味しそう (ADJ) from getting lemma 美味する (Suru verb)
  if (pos == core::PartOfSpeech::Adjective) {
    std::vector<grammar::InflectionCandidate> filtered;
    for (const auto& c : candidates) {
      if (c.verb_type == grammar::VerbType::IAdjective) {
        filtered.push_back(c);
      }
    }
    if (!filtered.empty()) {
      candidates = std::move(filtered);
    }
  }

  // Filter candidates by conjugation type if specified
  // This helps when verb_candidates.cpp has determined the correct verb type
  // e.g., for 話しそう with conj_type=GodanSa, prefer 話す (GodanSa) over 話しい (IAdjective)
  if (conj_type != dictionary::ConjugationType::None) {
    std::vector<grammar::InflectionCandidate> filtered;
    for (const auto& c : candidates) {
      if (grammar::verbTypeToConjType(c.verb_type) == conj_type) {
        filtered.push_back(c);
      }
    }
    if (!filtered.empty()) {
      candidates = std::move(filtered);
    }
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
  // Use >= 0.5F threshold since inflection_scorer caps minimum at 0.5F
  const auto& best = candidates.front();
  if (!best.base_form.empty() && best.confidence >= 0.5F) {
    return best.base_form;
  }

  return std::string(surface);
}

std::string Lemmatizer::lemmatize(const core::Morpheme& morpheme) const {
  // If morpheme is from dictionary and has lemma set, trust it
  // (even if lemma == surface, which is correct for base forms)
  if (morpheme.is_from_dictionary && !morpheme.lemma.empty()) {
    return morpheme.lemma;
  }

  // If lemma is already set and different from surface, use it
  // (lemma == surface means it's a default that may need re-derivation)
  if (!morpheme.lemma.empty() && morpheme.lemma != morpheme.surface) {
    // B45: Special fix for ない adjective + さ + そう pattern
    // なさそう = ない + さ + そう (looks like there isn't)
    // The inflection analyzer incorrectly derives lemma なさい (from なさ + そう)
    // but the correct lemma is ない (from な + さそう)
    if (morpheme.pos == core::PartOfSpeech::Adjective &&
        morpheme.surface.find("なさそう") == 0) {
      return "ない";
    }

    // Special fix for katakana + すぎる patterns
    // The inflection analyzer incorrectly derives lemma like ワンパターンる
    // when the correct form is ワンパターンすぎる
    std::string_view surface = morpheme.surface;
    std::string_view lemma = morpheme.lemma;
    if (surface.size() >= core::kThreeJapaneseCharBytes &&
        lemma.size() >= core::kJapaneseCharBytes) {
      std::string_view surface_ending = surface.substr(surface.size() - core::kThreeJapaneseCharBytes);
      bool has_sugiru_aux = (surface_ending == "すぎる" || surface_ending == "すぎた" || surface_ending == "すぎて");
      std::string_view lemma_ending = lemma.substr(lemma.size() - core::kJapaneseCharBytes);
      // Check if lemma ends with just る but surface ends with すぎる
      // E.g., surface=ワンパターンすぎる, lemma=ワンパターンる (incorrect)
      if (has_sugiru_aux && lemma_ending == "る" && lemma.size() < surface.size()) {
        // Check if the stem (lemma minus る) is katakana
        std::string stem(lemma.substr(0, lemma.size() - core::kJapaneseCharBytes));
        if (!stem.empty()) {
          auto codepoints = normalize::toCodepoints(stem);
          if (!codepoints.empty() &&
              normalize::classifyChar(codepoints[0]) == normalize::CharType::Katakana) {
            // Correct the lemma: stem + すぎる
            return stem + "すぎる";
          }
        }
      }
    }
    return morpheme.lemma;
  }

  // Skip grammar-based lemmatization for non-conjugating POS
  // Only verbs and adjectives conjugate
  switch (morpheme.pos) {
    case core::PartOfSpeech::Noun:
    case core::PartOfSpeech::Pronoun:
    case core::PartOfSpeech::Particle:
    case core::PartOfSpeech::Auxiliary:
    case core::PartOfSpeech::Conjunction:
    case core::PartOfSpeech::Adverb:
    case core::PartOfSpeech::Suffix:
    case core::PartOfSpeech::Symbol:
    case core::PartOfSpeech::Other:
      // These don't conjugate - return surface as-is
      return morpheme.surface;
    default:
      break;
  }

  // Try grammar-based lemmatization for verbs and adjectives
  // Pass POS and conj_type to filter candidates appropriately
  std::string grammar_result = lemmatizeByGrammar(morpheme.surface, morpheme.pos, morpheme.conj_type);
  // Grammar-based lemmatization is authoritative - use its result even if
  // it equals the surface (which means the surface is already a dictionary form)
  // Only fall back to rule-based if grammar analysis returned empty/failed
  if (!grammar_result.empty()) {
    // Check for 五段ラ行動詞 pattern: 漢字+す → 漢字+する
    // e.g., 対す → 対する, 関す → 関する
    if (morpheme.pos == core::PartOfSpeech::Verb &&
        endsWith(grammar_result, "す") && !endsWith(grammar_result, "する") &&
        dict_manager_ != nullptr) {
      std::string suru_form = grammar_result.substr(0, grammar_result.size() - core::kJapaneseCharBytes) + "する";
      auto lookup = dict_manager_->lookup(suru_form, 0);
      for (const auto& r : lookup) {
        if (r.entry != nullptr &&
            r.entry->surface == suru_form &&
            r.entry->pos == core::PartOfSpeech::Verb) {
          return suru_form;
        }
      }
    }
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
    if (surface.size() > core::kJapaneseCharBytes && surface != lemma) {
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
