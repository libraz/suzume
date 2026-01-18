#include "postprocess/lemmatizer.h"

#include <algorithm>

#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
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

    // Suru-verb te-form + subsidiary verbs (longest first)
    // These are compound patterns where [noun]して[subsidiary] → [noun]する
    // Progressive forms of subsidiary verbs (補助動詞進行形) - longest first
    {"してもらっています", "する"},
    {"してもらっていた", "する"},
    {"してもらっている", "する"},
    {"してあげています", "する"},
    {"してあげていた", "する"},
    {"してあげている", "する"},
    {"してくれています", "する"},
    {"してくれていた", "する"},
    {"してくれている", "する"},
    {"してきています", "する"},
    {"してきていた", "する"},
    {"してきている", "する"},
    {"していっています", "する"},
    {"していっていた", "する"},
    {"していっている", "する"},
    // Base forms of subsidiary verbs (補助動詞基本形)
    {"してもらう", "する"},
    {"してもらった", "する"},
    {"してもらって", "する"},
    {"してあげる", "する"},
    {"してあげた", "する"},
    {"してあげて", "する"},
    {"してみる", "する"},
    {"してみた", "する"},
    {"してみて", "する"},
    {"してくれる", "する"},
    {"してくれた", "する"},
    {"してくれて", "する"},
    {"していく", "する"},
    {"していった", "する"},
    {"していって", "する"},
    {"してくる", "する"},
    {"してきた", "する"},
    {"してきて", "する"},
    {"しておく", "する"},
    {"しておいた", "する"},
    {"しておいて", "する"},
    {"してしまう", "する"},
    {"してしまった", "する"},
    {"してしまって", "する"},

    // Suru-verb colloquial contractions (サ変動詞口語縮約形)
    // してしまう → しちゃう/しちまう
    {"しちゃいます", "する"},
    {"しちゃう", "する"},
    {"しちゃった", "する"},
    {"しちゃって", "する"},
    {"しちまう", "する"},
    {"しちまった", "する"},
    {"しちまって", "する"},
    // しておく → しとく
    {"しときます", "する"},
    {"しとく", "する"},
    {"しといた", "する"},
    {"しといて", "する"},
    // している → してる
    {"してました", "する"},
    {"してます", "する"},
    {"してる", "する"},
    {"してた", "する"},
    // Negative te-form (否定て形)
    {"しなくて", "する"},
    {"しないで", "する"},

    // Colloquial とく/どく contractions (ておく → とく)
    // Ichidan: stem + とく → stem + る
    {"とく", "る"},      // 見とく → 見る, 食べとく → 食べる
    {"といた", "る"},    // 見といた → 見る
    {"といて", "る"},    // 見といて → 見る
    // Godan onbinkei: stem + んどく → stem + む/ぶ/ぬ
    {"んどく", "む"},    // 読んどく → 読む
    {"んどいた", "む"},  // 読んどいた → 読む
    {"んどいて", "む"},  // 読んどいて → 読む
    // Godan i-row onbinkei: stem + いとく → stem + く
    {"いとく", "く"},    // 書いとく → 書く
    {"いといた", "く"},  // 書いといた → 書く
    {"いといて", "く"},  // 書いといて → 書く
    // Godan sokuon + とく: stem + っとく → stem + う/つ/る
    {"っとく", "う"},    // 買っとく → 買う
    {"っといた", "う"},  // 買っといた → 買う
    {"っといて", "う"},  // 買っといて → 買う

    // Colloquial てる/でる contractions (ている → てる)
    // Godan sokuon: stem + ってる → stem + う/つ/る
    {"ってる", "う"},    // 買ってる → 買う, 待ってる → 待つ
    {"ってた", "う"},    // 買ってた → 買う
    // Godan i-row: stem + いてる → stem + く
    {"いてる", "く"},    // 書いてる → 書く
    {"いてた", "く"},    // 書いてた → 書く
    // Godan n-row: stem + んでる → stem + む/ぶ/ぬ
    {"んでる", "む"},    // 読んでる → 読む
    {"んでた", "む"},    // 読んでた → 読む
    // Ichidan: stem + てる → stem + る
    {"てる", "る"},      // 見てる → 見る, 食べてる → 食べる
    {"てた", "る"},      // 見てた → 見る

    // Volitional form (意志形)
    // Ichidan: stem + よう → stem + る
    {"めよう", "める"},  // 始めよう → 始める (avoid false positive on godan)
    {"べよう", "べる"},  // 食べよう → 食べる
    {"ねよう", "ねる"},  // 寝よう → 寝る

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

    // Passive forms (dictionary)
    {"われる", "う"},
    {"かれる", "く"},
    {"がれる", "ぐ"},
    {"される", "す"},
    {"たれる", "つ"},
    {"なれる", "ぬ"},
    {"まれる", "む"},
    {"ばれる", "ぶ"},
    {"られる", "る"},

    // Passive forms (past)
    {"われた", "う"},
    {"かれた", "く"},
    {"がれた", "ぐ"},
    {"された", "す"},
    {"たれた", "つ"},
    {"なれた", "ぬ"},
    {"まれた", "む"},
    {"ばれた", "ぶ"},
    {"られた", "る"},

    // Passive forms (te-form)
    {"われて", "う"},
    {"かれて", "く"},
    {"がれて", "ぐ"},
    {"されて", "す"},
    {"たれて", "つ"},
    {"なれて", "ぬ"},
    {"まれて", "む"},
    {"ばれて", "ぶ"},
    {"られて", "る"},

    // Passive forms (negative)
    {"われない", "う"},
    {"かれない", "く"},
    {"がれない", "ぐ"},
    {"されない", "す"},
    {"たれない", "つ"},
    {"なれない", "ぬ"},
    {"まれない", "む"},
    {"ばれない", "ぶ"},
    {"られない", "る"},

    // Passive forms (polite)
    {"われます", "う"},
    {"かれます", "く"},
    {"がれます", "ぐ"},
    {"されます", "す"},
    {"たれます", "つ"},
    {"なれます", "ぬ"},
    {"まれます", "む"},
    {"ばれます", "ぶ"},
    {"られます", "る"},

    // Passive forms (polite past)
    {"われました", "う"},
    {"かれました", "く"},
    {"がれました", "ぐ"},
    {"されました", "す"},
    {"たれました", "つ"},
    {"なれました", "ぬ"},
    {"まれました", "む"},
    {"ばれました", "ぶ"},
    {"られました", "る"},

    // Passive forms (progressive)
    {"われている", "う"},
    {"かれている", "く"},
    {"がれている", "ぐ"},
    {"されている", "す"},
    {"たれている", "つ"},
    {"なれている", "ぬ"},
    {"まれている", "む"},
    {"ばれている", "ぶ"},
    {"られている", "る"},

    // Causative forms (dictionary)
    {"わせる", "う"},
    {"かせる", "く"},
    {"がせる", "ぐ"},
    {"させる", "す"},
    {"たせる", "つ"},
    {"なせる", "ぬ"},
    {"ませる", "む"},
    {"ばせる", "ぶ"},
    {"らせる", "る"},

    // Causative forms (past)
    {"わせた", "う"},
    {"かせた", "く"},
    {"がせた", "ぐ"},
    {"させた", "す"},
    {"たせた", "つ"},
    {"なせた", "ぬ"},
    {"ませた", "む"},
    {"ばせた", "ぶ"},
    {"らせた", "る"},

    // Causative forms (te-form)
    {"わせて", "う"},
    {"かせて", "く"},
    {"がせて", "ぐ"},
    {"させて", "す"},
    {"たせて", "つ"},
    {"なせて", "ぬ"},
    {"ませて", "む"},
    {"ばせて", "ぶ"},
    {"らせて", "る"},

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
      std::string_view surface_ending = utf8::last3Chars(surface);
      bool has_sugiru_aux = utf8::equalsAny(surface_ending, {"すぎる", "すぎた", "すぎて"});
      std::string_view lemma_ending = utf8::lastChar(lemma);
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

    // Check for サ変動詞 classical form: 漢字2文字以上+す → 漢字+する
    // e.g., 確認す → 確認する, 運動す → 運動する
    // Single kanji + す (出す, 消す) are GodanSa, not Suru
    if (morpheme.pos == core::PartOfSpeech::Verb &&
        endsWith(morpheme.lemma, "す") && !endsWith(morpheme.lemma, "する")) {
      std::string stem = morpheme.lemma.substr(0, morpheme.lemma.size() - core::kJapaneseCharBytes);
      // Check if stem is 2+ kanji characters (6+ bytes)
      if (stem.size() >= core::kTwoJapaneseCharBytes && grammar::isAllKanji(stem)) {
        return stem + "する";
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
    // Check for サ変動詞 classical form: 漢字2文字以上+す → 漢字+する
    // e.g., 勉強す → 勉強する, 運動す → 運動する
    // Single kanji + す (出す, 消す) are GodanSa, not Suru
    if (morpheme.pos == core::PartOfSpeech::Verb &&
        endsWith(grammar_result, "す") && !endsWith(grammar_result, "する")) {
      std::string stem = grammar_result.substr(0, grammar_result.size() - core::kJapaneseCharBytes);
      // Check if stem is 2+ kanji characters (6+ bytes)
      if (stem.size() >= core::kTwoJapaneseCharBytes && grammar::isAllKanji(stem)) {
        return stem + "する";
      }
    }
    // Check for compound verbs that conjugate like サ変: [kanji]しる → [kanji]する
    // e.g., 対しる → 対する, 関しる → 関する, 反しる → 反する
    // These verbs are incorrectly analyzed as ichidan (stem + る) but should be サ変-like
    // Note: 応じる, 存じる are actual ichidan verbs (not サ変)
    if (morpheme.pos == core::PartOfSpeech::Verb &&
        endsWith(grammar_result, "しる")) {
      std::string stem = grammar_result.substr(0, grammar_result.size() - core::kTwoJapaneseCharBytes);
      // Check if stem is a single kanji that forms a compound verb with する
      // Common patterns: 対する, 関する, 反する, 接する, 属する, 達する, etc.
      if (stem.size() == core::kJapaneseCharBytes && grammar::isAllKanji(stem)) {
        return stem + "する";
      }
    }
    // For passive verbs, grammar-based returns the passive form as base (e.g., いわれる)
    // but we want the original base verb (e.g., いう). Use rule-based lemmatization instead.
    // Pattern: 〜れる endings are passive forms of godan verbs
    if (morpheme.pos == core::PartOfSpeech::Verb && grammar_result == morpheme.surface) {
      std::string rule_result = lemmatizeVerb(morpheme.surface);
      if (rule_result != morpheme.surface) {
        return rule_result;
      }
    }
    // B45: Special fix for ない adjective + さ + そう pattern
    // Grammar incorrectly returns なさい, but correct lemma is ない
    // The surface なさそう with grammar result なさい should return ない
    if (grammar_result == "なさい" && morpheme.surface.find("なさそう") != std::string::npos) {
      return "ない";
    }

    // Fix for Godan onbin forms incorrectly lemmatized
    // Grammar returns wrong base: 読ん → 読る, 書い → 書う
    // Should be: 読ん → 読む, 書い → 書く
    if (morpheme.pos == core::PartOfSpeech::Verb) {
      std::string_view sfc = morpheme.surface;
      // 撥音便: surface ends with ん, result ends with る → む
      // This is the most common pattern (五段マ行: 読む, 飲む, 住む, etc.)
      if (endsWith(sfc, "ん") && endsWith(grammar_result, "る") &&
          grammar_result.size() >= core::kTwoJapaneseCharBytes) {
        std::string stem = grammar_result.substr(0, grammar_result.size() - core::kJapaneseCharBytes);
        std::string godan_form = stem + "む";
        // Verify with dictionary if available, otherwise assume む is correct
        if (dict_manager_ != nullptr) {
          auto results = dict_manager_->lookup(godan_form, 0);
          for (const auto& r : results) {
            if (r.entry != nullptr && r.entry->pos == core::PartOfSpeech::Verb) {
              return godan_form;
            }
          }
        }
        // Fallback: if stem is kanji, assume む (most common 撥音便 pattern)
        if (!stem.empty() && grammar::isAllKanji(stem)) {
          return godan_form;
        }
      }
      // イ音便: surface ends with い, result ends with う → く or ぐ
      // GodanKa (書い → 書く) and GodanGa (泳い → 泳ぐ) both have イ音便
      if (endsWith(sfc, "い") && endsWith(grammar_result, "う") &&
          grammar_result.size() >= core::kTwoJapaneseCharBytes) {
        std::string stem = grammar_result.substr(0, grammar_result.size() - core::kJapaneseCharBytes);
        // Check GodanGa first (ぐ), then GodanKa (く)
        // Order matters: if both exist, prefer the dictionary-verified one
        if (dict_manager_ != nullptr) {
          std::string godan_ga_form = stem + "ぐ";
          auto results_ga = dict_manager_->lookup(godan_ga_form, 0);
          for (const auto& r : results_ga) {
            if (r.entry != nullptr && r.entry->pos == core::PartOfSpeech::Verb) {
              return godan_ga_form;
            }
          }
          std::string godan_ka_form = stem + "く";
          auto results_ka = dict_manager_->lookup(godan_ka_form, 0);
          for (const auto& r : results_ka) {
            if (r.entry != nullptr && r.entry->pos == core::PartOfSpeech::Verb) {
              return godan_ka_form;
            }
          }
        }
        // Fallback: if stem is kanji, assume く (most common イ音便 pattern)
        if (!stem.empty() && grammar::isAllKanji(stem)) {
          return stem + "く";
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
  for (size_t i = 0; i < morphemes.size(); ++i) {
    auto& morpheme = morphemes[i];
    // B45: Special fix for ない adjective + さ + そう pattern
    // The adjective candidate generator sets lemma to なさい, but correct is ない
    // なさそう = ない + さそう (looks like there isn't)
    if (morpheme.pos == core::PartOfSpeech::Adjective &&
        morpheme.surface.find("なさそう") != std::string::npos &&
        morpheme.lemma == "なさい") {
      morpheme.lemma = "ない";
    }

    // Fix classical suru-verb lemma: 漢字2文字以上+す → 漢字+する
    // e.g., 確認す → 確認する, 運動す → 運動する
    // The verb_candidates sometimes returns classical form that needs conversion
    if (morpheme.pos == core::PartOfSpeech::Verb &&
        !morpheme.lemma.empty() &&
        endsWith(morpheme.lemma, "す") && !endsWith(morpheme.lemma, "する")) {
      std::string stem = morpheme.lemma.substr(0, morpheme.lemma.size() - core::kJapaneseCharBytes);
      // Check if stem is 2+ kanji characters (6+ bytes)
      if (stem.size() >= core::kTwoJapaneseCharBytes && grammar::isAllKanji(stem)) {
        morpheme.lemma = stem + "する";
      }
    }

    // Preserve lemma if intentionally set (e.g., from verb_candidates for passive verbs)
    // Recalculate if:
    // 1. Lemma is empty, OR
    // 2. Lemma equals surface AND it's a conjugated form (not dictionary form)
    //    Dictionary forms end with: る, う, く, ぐ, す, つ, ぬ, ぶ, む (verbs), い (adjectives)
    bool needs_lemmatization = morpheme.lemma.empty();
    if (!needs_lemmatization && morpheme.lemma == morpheme.surface) {
      if (morpheme.pos == core::PartOfSpeech::Verb) {
        // Check if surface looks like a dictionary form verb
        // Dictionary form verbs end with: る, う, く, ぐ, す, つ, ぬ, ぶ, む
        bool is_dict_form = utf8::endsWithAny(morpheme.surface,
            {"る", "う", "く", "ぐ", "す", "つ", "ぬ", "ぶ", "む"});
        // If it's a dictionary form, lemma == surface is correct
        // If it's a conjugated form (て, た, ない, etc.), recalculate
        if (!is_dict_form) {
          needs_lemmatization = true;
        }
        // But passive verbs ending in 〜れる need lemmatization even though they end with る
        // E.g., いわれる → いう, かかれる → かく
        if (is_dict_form && utf8::endsWithAny(morpheme.surface,
            {"われる", "かれる", "がれる", "される", "たれる",
             "なれる", "まれる", "ばれる", "られる"})) {
          needs_lemmatization = true;
        }
        // Causative forms need lemmatization
        // E.g., 勉強させる → 勉強する, 書かせる → 書く
        if (is_dict_form && utf8::endsWithAny(morpheme.surface,
            {"させる", "わせる", "かせる", "がせる", "たせる",
             "なせる", "ばせる", "ませる", "らせる"})) {
          needs_lemmatization = true;
        }
        // Suru-verb te-form + subsidiary verb patterns need lemmatization
        // E.g., 説明してもらう → 説明する, 勉強してくる → 勉強する
        if (is_dict_form && utf8::endsWithAny(morpheme.surface,
            {"してもらう", "してあげる", "してみる", "してくれる",
             "していく", "してくる", "しておく", "してしまう"})) {
          needs_lemmatization = true;
        }
        // Colloquial とく/どく contractions need lemmatization
        // E.g., 見とく → 見る, 読んどく → 読む, 書いとく → 書く
        if (is_dict_form && utf8::endsWithAny(morpheme.surface, {"とく", "んどく"})) {
          needs_lemmatization = true;
        }
        // Colloquial てる/でる contractions need lemmatization
        // E.g., 見てる → 見る, 読んでる → 読む, 買ってる → 買う
        if (is_dict_form && utf8::endsWithAny(morpheme.surface, {"てる", "でる", "ってる"})) {
          needs_lemmatization = true;
        }
        // Volitional form needs lemmatization
        // E.g., 始めよう → 始める, 食べよう → 食べる
        if (endsWith(morpheme.surface, "よう")) {
          needs_lemmatization = true;
        }
      } else if (morpheme.pos == core::PartOfSpeech::Adjective) {
        // Check if surface looks like a dictionary form adjective (ends with い)
        bool is_dict_form = endsWith(morpheme.surface, "い");
        if (!is_dict_form) {
          needs_lemmatization = true;
        }
      }
    }
    if (needs_lemmatization) {
      morpheme.lemma = lemmatize(morpheme);
    }
    // Get next morpheme's lemma for context-dependent conj_form detection
    std::string_view next_lemma;
    if (i + 1 < morphemes.size()) {
      next_lemma = morphemes[i + 1].lemma;
    }
    morpheme.conj_form = detectConjForm(morpheme.surface, morpheme.lemma, morpheme.pos, next_lemma);
  }
}

grammar::ConjForm Lemmatizer::detectConjForm(std::string_view surface,
                                             std::string_view lemma,
                                             core::PartOfSpeech pos,
                                             std::string_view next_lemma) {
  // Only verbs and adjectives have conjugation forms
  if (pos != core::PartOfSpeech::Verb &&
      pos != core::PartOfSpeech::Adjective) {
    return grammar::ConjForm::Base;
  }

  // If surface equals lemma, it's the base form
  if (surface == lemma) {
    return grammar::ConjForm::Base;
  }

  // For ichidan verbs, mizenkei and renyokei have the same surface form
  // (e.g., 食べ for both). Use next morpheme to distinguish:
  // - ない/ぬ/よう → Mizenkei
  // - て/た/ます → Renyokei
  if (pos == core::PartOfSpeech::Verb && endsWith(lemma, "る")) {
    // Check if this looks like an ichidan verb stem (lemma ends with る, surface is stem)
    // Ichidan verb stem = lemma without final る
    if (lemma.size() >= 3 && surface.size() >= 3) {
      std::string_view lemma_stem(lemma.data(), lemma.size() - 3);  // Remove る (3 bytes)
      if (surface == lemma_stem && !next_lemma.empty()) {
        // This is an ichidan verb stem - check what follows
        if (utf8::equalsAny(next_lemma, {
            "ない", "ぬ", "ず", "よう", "まい",  // negative/volitional
            "れる", "られる",  // passive
            "せる", "させる"   // causative
        })) {
          return grammar::ConjForm::Mizenkei;
        }
        // て/た/ます → Renyokei (will be caught by default below)
      }
    }
  }

  // Check for negative forms (mizenkei)
  if (utf8::endsWithAny(surface,
      {"ない", "なかった", "ぬ", "ず", "ません", "なく",
       "なくて", "なければ", "なきゃ", "なくても"})) {
    return grammar::ConjForm::Mizenkei;
  }

  // Check for passive/causative (mizenkei)
  if (utf8::endsWithAny(surface,
      {"れる", "られる", "せる", "させる", "れた", "られた",
       "せた", "させた", "される", "された"})) {
    return grammar::ConjForm::Mizenkei;
  }

  // Check for volitional form (ishikei)
  if (utf8::endsWithAny(surface, {"う", "よう", "まい"})) {
    // Distinguish from godan base form ending in う
    if (surface != lemma) {
      return grammar::ConjForm::Ishikei;
    }
  }

  // Check for conditional form (kateikei)
  if (utf8::endsWithAny(surface, {"ば", "れば"})) {
    return grammar::ConjForm::Kateikei;
  }

  // Check for imperative form (meireikei)
  if (utf8::endsWithAny(surface, {"ろ", "よ", "なさい"})) {
    // Check if it's likely an imperative
    if (surface.size() > core::kJapaneseCharBytes && surface != lemma) {
      return grammar::ConjForm::Meireikei;
    }
  }

  // Check for te-form onbin patterns (onbinkei)
  if (utf8::endsWithAny(surface,
      {"って", "いて", "いで", "んで", "った", "いた", "いだ", "んだ"})) {
    return grammar::ConjForm::Onbinkei;
  }

  // Check for renyokei (te-form, ta-form, masu-form, etc.)
  if (utf8::endsWithAny(surface,
      {"て", "で", "た", "だ", "ます", "ました", "まして",
       "ている", "ていた", "ておく", "てある", "てみる", "てくる", "ていく",
       "てしまう", "ちゃう", "たい", "たかった", "たら", "たり",
       "きた", "してる", "してた", "しています", "していた", "しました"})) {
    return grammar::ConjForm::Renyokei;
  }

  // For i-adjectives
  if (pos == core::PartOfSpeech::Adjective) {
    if (utf8::endsWithAny(surface, {"く", "くて", "かった", "ければ", "さ", "そう"})) {
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
