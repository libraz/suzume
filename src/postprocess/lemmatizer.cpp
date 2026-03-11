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

// Potential verb (可能動詞) endings: godan stem + れる
// E.g., 書ける (かける), 泊まれる (とまれる), 読める (よめる)
// Single token 〜れる verbs are treated as potential (ichidan), not passive.
// Passive forms are split (読ま+れる), so single token 〜れる is likely potential.
constexpr std::string_view kPotentialVerbEndings[] = {
    "われる", "かれる", "がれる", "される", "たれる",
    "なれる", "まれる", "ばれる", "られる"};

// Check if surface ends with any potential verb ending
bool endsWithPotentialVerbSuffix(std::string_view surface) {
  for (const auto& ending : kPotentialVerbEndings) {
    if (surface.size() >= ending.size() &&
        surface.substr(surface.size() - ending.size()) == ending) {
      return true;
    }
  }
  return false;
}

// Verb endings and their base forms
struct VerbEnding {
  std::string_view suffix;
  std::string_view base;
};

// =============================================================================
// Godan verb conjugation base mapping
// Each entry maps a consonant row (未然形語尾) to its dictionary form ending
// =============================================================================
// clang-format off

// Godan consonant → dictionary form mapping
// Format: {consonant, base} where consonant is 未然形 and base is 終止形
#define GODAN_ROWS \
  X("わ", "う") X("か", "く") X("が", "ぐ") X("さ", "す") X("た", "つ") \
  X("な", "ぬ") X("ま", "む") X("ば", "ぶ") X("ら", "る")

// Generate passive forms for all godan rows: C + suffix → base
// Passive: 未然形 + れる (e.g., 買わ + れる → 買う)
#define PASSIVE(suffix) \
  X("わ" suffix, "う") X("か" suffix, "く") X("が" suffix, "ぐ") \
  X("さ" suffix, "す") X("た" suffix, "つ") X("な" suffix, "ぬ") \
  X("ま" suffix, "む") X("ば" suffix, "ぶ") X("ら" suffix, "る")

// Generate causative forms for all godan rows: C + suffix → base
// Causative: 未然形 + せる (e.g., 書か + せる → 書く)
#define CAUSATIVE(suffix) \
  X("わ" suffix, "う") X("か" suffix, "く") X("が" suffix, "ぐ") \
  X("さ" suffix, "す") X("た" suffix, "つ") X("な" suffix, "ぬ") \
  X("ま" suffix, "む") X("ば" suffix, "ぶ") X("ら" suffix, "る")

// Generate causative-passive forms (no さ row - される is ambiguous with passive)
#define CAUSATIVE_PASSIVE(suffix) \
  X("わ" suffix, "う") X("か" suffix, "く") X("が" suffix, "ぐ") \
  X("た" suffix, "つ") X("な" suffix, "ぬ") \
  X("ま" suffix, "む") X("ば" suffix, "ぶ") X("ら" suffix, "る")

// Generate negative forms: C + ない → base
#define NEGATIVE(suffix) \
  X("わ" suffix, "う") X("か" suffix, "く") X("が" suffix, "ぐ") \
  X("さ" suffix, "す") X("た" suffix, "つ") X("な" suffix, "ぬ") \
  X("ま" suffix, "む") X("ば" suffix, "ぶ") X("ら" suffix, "る")

// X macro helper for VerbEnding generation
#define X(suffix, base) {suffix, base},

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

    // =========================================================================
    // Passive forms (受身形) - generated via PASSIVE macro
    // Pattern: 未然形 + れる/れた/れて/れない/れます/れました/れている
    // =========================================================================
    PASSIVE("れている")   // Progressive (longest first)
    PASSIVE("れました")   // Polite past
    PASSIVE("れない")     // Negative
    PASSIVE("れます")     // Polite
    PASSIVE("れる")       // Dictionary
    PASSIVE("れた")       // Past
    PASSIVE("れて")       // Te-form

    // =========================================================================
    // Causative forms (使役形) - generated via CAUSATIVE macro
    // Pattern: 未然形 + せる/せた/せて
    // =========================================================================
    CAUSATIVE("せる")     // Dictionary
    CAUSATIVE("せた")     // Past
    CAUSATIVE("せて")     // Te-form

    // =========================================================================
    // Causative-passive forms (使役受身形)
    // Pattern: 未然形 + される (note: さ row excluded - ambiguous with passive)
    // =========================================================================
    CAUSATIVE_PASSIVE("された")

    // Godan verbs (onbin forms)
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

    // Nai-form - generated via NEGATIVE macro
    NEGATIVE("ない")
    {"ない", "る"},  // Ichidan fallback

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

    // Passive/Causative (ichidan)
    {"られる", "る"},
    {"させる", "する"},
};

// Cleanup macros
#undef X
#undef GODAN_ROWS
#undef PASSIVE
#undef CAUSATIVE
#undef CAUSATIVE_PASSIVE
#undef NEGATIVE
// clang-format on

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

    break;
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
  // First, check if surface itself is a base form (not conjugated form) in dictionary
  // (e.g., 差し上げる should return 差し上げる, not 差し上ぐ)
  // We check that lemma == surface, meaning it's the dictionary form, not a conjugated form
  // Conjugated forms like 使い (from 使う) have lemma != surface (lemma = 使う)
  if (dict_manager_ != nullptr) {
    auto results = dict_manager_->lookup(surface, 0);
    for (const auto& result : results) {
      if (result.entry != nullptr &&
          result.entry->surface == surface &&
          result.entry->lemma == surface &&  // Must be base form, not conjugated
          (result.entry->pos == core::PartOfSpeech::Verb ||
           result.entry->pos == core::PartOfSpeech::Adjective)) {
        // Surface is a valid base form in dictionary
        return std::string(surface);
      }
    }
  }

  // Get all candidates (const reference to cached result)
  const auto& all_candidates = inflection_.analyze(surface);

  if (all_candidates.empty()) {
    return std::string(surface);
  }

  // Apply POS/conjugation filters into a local copy only when needed
  // Otherwise use the cached reference directly to avoid copying
  std::vector<grammar::InflectionCandidate> filtered_storage;
  const std::vector<grammar::InflectionCandidate>* candidates = &all_candidates;

  // Filter candidates by POS if specified
  // For Adjective POS, only accept IAdjective verb_type
  // This prevents 美味しそう (ADJ) from getting lemma 美味する (Suru verb)
  if (pos == core::PartOfSpeech::Adjective) {
    for (const auto& cnd : *candidates) {
      if (cnd.verb_type == grammar::VerbType::IAdjective) {
        filtered_storage.push_back(cnd);
      }
    }
    if (!filtered_storage.empty()) {
      candidates = &filtered_storage;
    } else {
      // No IAdjective candidates → na-adjective (大変, 不思議, etc.)
      // Na-adjectives don't conjugate, lemma = surface
      return std::string(surface);
    }
  }

  // Filter candidates by conjugation type if specified
  // This helps when verb_candidates.cpp has determined the correct verb type
  // e.g., for 話しそう with conj_type=GodanSa, prefer 話す (GodanSa) over 話しい (IAdjective)
  if (conj_type != dictionary::ConjugationType::None) {
    std::vector<grammar::InflectionCandidate> conj_filtered;
    for (const auto& cnd : *candidates) {
      if (grammar::verbTypeToConjType(cnd.verb_type) == conj_type) {
        conj_filtered.push_back(cnd);
      }
    }
    if (!conj_filtered.empty()) {
      filtered_storage = std::move(conj_filtered);
      candidates = &filtered_storage;
    }
  }

  // If dictionary is available, try to find a verified candidate
  // For dictionary-verified candidates, use lower confidence threshold (0.3F)
  // Dictionary verification compensates for confidence penalties from heuristics
  // (e.g., all-kanji i-adjective stems like 面白 get penalized but are valid)
  if (dict_manager_ != nullptr) {
    for (const auto& candidate : *candidates) {
      if (candidate.confidence > 0.3F && verifyCandidateWithDictionary(candidate)) {
        return candidate.base_form;
      }
    }
  }

  // Fall back to the best candidate if no dictionary match found
  // Use >= 0.5F threshold since inflection_scorer caps minimum at 0.5F
  const auto& best = candidates->front();
  if (!best.base_form.empty() && best.confidence >= 0.5F) {
    return best.base_form;
  }

  return std::string(surface);
}

std::string Lemmatizer::lemmatize(const core::Morpheme& morpheme) const {
  // If morpheme is from dictionary and has distinct lemma set, trust it
  // (lemma != surface means it was explicitly set, not defaulted)
  // When lemma == surface, we need to re-derive for conjugated forms
  if (morpheme.is_from_dictionary && !morpheme.lemma.empty() &&
      morpheme.lemma != morpheme.surface) {
    return morpheme.lemma;
  }

  // Tari-adjective adverbs: remove trailing と from lemma
  // e.g., 颯爽と → 颯爽, 堂々と → 堂々
  // Pattern: 漢字2文字 + と (6 bytes kanji + 3 bytes と = 9 bytes)
  // This check runs even for dictionary entries where lemma == surface
  if (morpheme.pos == core::PartOfSpeech::Adverb) {
    constexpr size_t kTariAdverbLen = core::kTwoJapaneseCharBytes + core::kJapaneseCharBytes;
    if (morpheme.surface.size() == kTariAdverbLen &&
        endsWith(morpheme.surface, "と")) {
      std::string stem = morpheme.surface.substr(0, core::kTwoJapaneseCharBytes);
      // Verify stem is 2 kanji characters (or iteration mark pattern like 堂々)
      if (grammar::isAllKanji(stem) ||
          (stem.size() == core::kTwoJapaneseCharBytes &&
           stem.substr(core::kJapaneseCharBytes) == "々")) {
        return stem;
      }
    }
  }

  // If lemma is already set and different from surface, use it
  // (lemma == surface means it's a default that may need re-derivation)
  if (!morpheme.lemma.empty() && morpheme.lemma != morpheme.surface) {
    // Fix potential verb (可能動詞) lemma FIRST: 泊まれる should have lemma=泊まれる, not 泊む
    // Potential verbs are single tokens ending in 〜れる (e.g., 書ける, 泊まれる, 読める)
    // The inflection analyzer incorrectly derives lemma as godan base (泊む from 泊まれる)
    // but potential verbs are ichidan verbs - their lemma should be surface itself
    // Passive forms are split (読ま+れる), so single token 〜れる is likely potential
    if (morpheme.pos == core::PartOfSpeech::Verb &&
        endsWithPotentialVerbSuffix(morpheme.surface)) {
      // Single token 〜れる verb: treat as potential verb, lemma = surface
      return morpheme.surface;
    }

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

    // Fix special ra-row (ラ行特殊活用) verb lemma: ~いる → ~る
    // Verbs like ござる, いらっしゃる have renyokei ending in い (not り)
    // The inflection analyzer incorrectly reconstructs ~いる as base form
    // E.g., ござい → ございる (wrong) → ござる (correct)
    if (morpheme.pos == core::PartOfSpeech::Verb &&
        endsWith(morpheme.lemma, "いる") &&
        morpheme.lemma.size() >= core::kThreeJapaneseCharBytes &&
        dict_manager_ != nullptr) {
      // Try replacing いる with る to find the special conjugation base form
      std::string stem = morpheme.lemma.substr(
          0, morpheme.lemma.size() - core::kTwoJapaneseCharBytes);
      std::string ru_form = stem + "る";
      auto results = dict_manager_->lookup(ru_form, 0);
      for (const auto& r : results) {
        if (r.entry != nullptr && r.entry->pos == core::PartOfSpeech::Verb &&
            r.entry->surface == ru_form) {
          return ru_form;
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

    // Fix 撥音便 lemma: if lemma ends with む but dictionary has ぶ or ぬ, use that
    // E.g., 学ん → lemma=学む (wrong) → should be 学ぶ (correct)
    // The candidate generator may produce wrong lemma when dictionary lookup fails
    if (morpheme.pos == core::PartOfSpeech::Verb &&
        endsWith(morpheme.surface, "ん") &&
        endsWith(morpheme.lemma, "む") &&
        morpheme.lemma.size() >= core::kTwoJapaneseCharBytes &&
        dict_manager_ != nullptr) {
      std::string stem = morpheme.lemma.substr(0, morpheme.lemma.size() - core::kJapaneseCharBytes);
      // Try ぶ (学ぶ, 遊ぶ, 飛ぶ, etc.)
      std::string bu_form = stem + "ぶ";
      auto results_bu = dict_manager_->lookup(bu_form, 0);
      for (const auto& r : results_bu) {
        if (r.entry != nullptr && r.entry->pos == core::PartOfSpeech::Verb) {
          return bu_form;
        }
      }
      // Try ぬ (死ぬ)
      std::string nu_form = stem + "ぬ";
      auto results_nu = dict_manager_->lookup(nu_form, 0);
      for (const auto& r : results_nu) {
        if (r.entry != nullptr && r.entry->pos == core::PartOfSpeech::Verb) {
          return nu_form;
        }
      }
      // Original む form - keep it
    }

    // Fix potential verb (可能動詞) lemma: 泊まれる should have lemma=泊まれる, not 泊む
    // Potential verbs are single tokens ending in 〜れる (e.g., 書ける, 泊まれる, 読める)
    // The inflection analyzer incorrectly derives lemma as godan base (泊む from 泊まれる)
    // but potential verbs are ichidan verbs - their lemma should be surface itself
    // Passive forms are split (読ま+れる), so single token 〜れる is likely potential
    if (morpheme.pos == core::PartOfSpeech::Verb &&
        endsWithPotentialVerbSuffix(morpheme.surface)) {
      // Single token 〜れる verb: treat as potential verb, lemma = surface
      return morpheme.surface;
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
    case core::PartOfSpeech::Adverb: {
      // Tari-adjective adverbs: remove trailing と from lemma
      // e.g., 颯爽と → 颯爽, 堂々と → 堂々
      // Pattern: 漢字2文字 + と (6 bytes kanji + 3 bytes と = 9 bytes)
      constexpr size_t kTariAdverbLen = core::kTwoJapaneseCharBytes + core::kJapaneseCharBytes;
      if (morpheme.surface.size() == kTariAdverbLen &&
          endsWith(morpheme.surface, "と")) {
        std::string stem = morpheme.surface.substr(0, core::kTwoJapaneseCharBytes);
        // Verify stem is 2 kanji characters (or iteration mark pattern like 堂々)
        if (grammar::isAllKanji(stem) ||
            (stem.size() == core::kTwoJapaneseCharBytes &&
             stem.substr(core::kJapaneseCharBytes) == "々")) {
          return stem;
        }
      }
      return morpheme.surface;
    }
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
    // Check for compound verbs that conjugate like サ変: [stem]しる → [stem]する
    // e.g., 対しる → 対する, 関しる → 関する, やりなおしる → やりなおす
    // These verbs are incorrectly analyzed as ichidan (stem + る) but should be サ変/godan-sa
    // Note: 応じる, 存じる, 信じる, 感じる are actual ichidan verbs (じる, not しる)
    if (morpheme.pos == core::PartOfSpeech::Verb &&
        endsWith(grammar_result, "しる")) {
      std::string stem = grammar_result.substr(0, grammar_result.size() - core::kTwoJapaneseCharBytes);
      if (stem.size() >= core::kJapaneseCharBytes) {
        // Single kanji stem: compound する-verb (対する, 関する, etc.)
        if (stem.size() == core::kJapaneseCharBytes && grammar::isAllKanji(stem)) {
          return stem + "する";
        }
        // Multi-character stem: compound godan-sa verb (やりなおす, etc.)
        // Convert しる → す (godan-sa base form)
        return stem + "す";
      }
    }
    // For passive verbs, grammar-based returns the passive form as base (e.g., いわれる)
    // but we want the original base verb (e.g., いう). Use rule-based lemmatization instead.
    // Pattern: 〜れる endings are passive forms of godan verbs
    // EXCEPTION: Potential verbs (可能動詞) like 書ける, 泊まれる should keep lemma=surface
    // Potential verbs are ichidan verbs derived from godan verbs (e.g., 泊まる→泊まれる)
    // They are single tokens, not split like passive (読ま+れる)
    // NOTE: When a 〜れる verb is recognized as a single token (not split), it's likely
    // a potential verb. Passive forms are usually split (e.g., 読ま+れる).
    if (morpheme.pos == core::PartOfSpeech::Verb && grammar_result == morpheme.surface) {
      // Check if this is a potential verb (可能動詞)
      // Potential verbs have pattern: godan_stem + e-row + る
      // E.g., 書ける (kak+e+ru), 泊まれる (tomar+e+ru), 読める (yom+e+ru)
      // vs Passive: godan_mizen + れる → split as 読ま+れる
      // Since this morpheme is a single token ending in 〜れる, it's likely a potential verb.
      // (Passive forms would be split into 未然形 + れる by the tokenizer)
      if (endsWithPotentialVerbSuffix(morpheme.surface)) {
        // Single token 〜れる verb: treat as potential verb, lemma = surface
        return morpheme.surface;
      }

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
      // 撥音便: surface ends with ん, result ends with る → む/ぶ/ぬ
      // Godan verbs with 撥音便:
      // - GodanMa (む): 読む, 飲む, 住む, etc.
      // - GodanBa (ぶ): 学ぶ, 遊ぶ, 飛ぶ, etc.
      // - GodanNa (ぬ): 死ぬ
      if (endsWith(sfc, "ん") && endsWith(grammar_result, "る") &&
          grammar_result.size() >= core::kTwoJapaneseCharBytes) {
        std::string stem = grammar_result.substr(0, grammar_result.size() - core::kJapaneseCharBytes);
        // Try all three 撥音便 endings with dictionary verification
        if (dict_manager_ != nullptr) {
          // Try む first (most common: 読む, 飲む, etc.)
          std::string godan_mu_form = stem + "む";
          auto results_mu = dict_manager_->lookup(godan_mu_form, 0);
          for (const auto& r : results_mu) {
            if (r.entry != nullptr && r.entry->pos == core::PartOfSpeech::Verb) {
              return godan_mu_form;
            }
          }
          // Try ぶ (学ぶ, 遊ぶ, 飛ぶ, etc.)
          std::string godan_bu_form = stem + "ぶ";
          auto results_bu = dict_manager_->lookup(godan_bu_form, 0);
          for (const auto& r : results_bu) {
            if (r.entry != nullptr && r.entry->pos == core::PartOfSpeech::Verb) {
              return godan_bu_form;
            }
          }
          // Try ぬ (死ぬ)
          std::string godan_nu_form = stem + "ぬ";
          auto results_nu = dict_manager_->lookup(godan_nu_form, 0);
          for (const auto& r : results_nu) {
            if (r.entry != nullptr && r.entry->pos == core::PartOfSpeech::Verb) {
              return godan_nu_form;
            }
          }
        }
        // Fallback: if stem is kanji, assume む (most common 撥音便 pattern)
        std::string godan_form = stem + "む";
        if (!stem.empty() && grammar::isAllKanji(stem)) {
          return godan_form;
        }
      }
      // イ音便: surface ends with い, result ends with う → く or ぐ
      // GodanKa (書い → 書く) and GodanGa (泳い → 泳ぐ) both have イ音便
      // BUT: Godan-wa renyokei also ends with い and gives 〜う (使い → 使う)
      // Only apply onbin fix if grammar_result (〜う) is NOT in dictionary
      if (endsWith(sfc, "い") && endsWith(grammar_result, "う") &&
          grammar_result.size() >= core::kTwoJapaneseCharBytes) {
        // First check if grammar_result is a valid verb in dictionary
        // If so, it's likely a godan-wa verb (使う, 買う, etc.), not onbin
        if (dict_manager_ != nullptr) {
          auto results_u = dict_manager_->lookup(grammar_result, 0);
          for (const auto& r : results_u) {
            if (r.entry != nullptr && r.entry->pos == core::PartOfSpeech::Verb) {
              // grammar_result (e.g., 使う) is valid - use it directly
              return grammar_result;
            }
          }
        }
        // grammar_result not found in dictionary - try onbin correction
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
        // No dictionary verification available - return grammar_result as-is
        // The lemmatizeAll() will fix onbin patterns using next morpheme context
        // This allows godan-wa renyokei (使い → 使う) to work correctly
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

    // Fix classical negative auxiliary lemma: ず → ぬ (MeCab compatibility)
    // The auxiliary ず (classical negative) has lemma ぬ in MeCab
    if (morpheme.pos == core::PartOfSpeech::Auxiliary &&
        morpheme.surface == "ず" && morpheme.lemma == "ず") {
      morpheme.lemma = "ぬ";
    }

    // Fix tari-adjective adverb lemma: 颯爽と → 颯爽, 堂々と → 堂々
    // Pattern: 漢字2文字 + と (6 bytes kanji + 3 bytes と = 9 bytes)
    // MeCab uses stem only as lemma for tari-adverbs
    constexpr size_t kTariAdverbLen = core::kTwoJapaneseCharBytes + core::kJapaneseCharBytes;
    if (morpheme.pos == core::PartOfSpeech::Adverb &&
        morpheme.surface.size() == kTariAdverbLen &&
        endsWith(morpheme.surface, "と")) {
      std::string stem = morpheme.surface.substr(0, core::kTwoJapaneseCharBytes);
      // Verify stem is 2 kanji characters (or iteration mark pattern like 堂々)
      if (grammar::isAllKanji(stem) ||
          (stem.size() == core::kTwoJapaneseCharBytes &&
           stem.substr(core::kJapaneseCharBytes) == "々")) {
        morpheme.lemma = stem;
      }
    }

    // Fix potential verb (可能動詞) lemma: 泊まれる should have lemma=泊まれる, not 泊む
    // Potential verbs are single tokens ending in 〜れる (e.g., 書ける, 泊まれる, 読める)
    // The inflection analyzer incorrectly derives lemma as godan base (泊む from 泊まれる)
    // but potential verbs are ichidan verbs - their lemma should be surface itself
    // Passive forms are split (読ま+れる), so single token 〜れる is likely potential
    if (morpheme.pos == core::PartOfSpeech::Verb &&
        morpheme.lemma != morpheme.surface &&
        endsWithPotentialVerbSuffix(morpheme.surface)) {
      // Single token 〜れる verb: treat as potential verb, lemma = surface
      morpheme.lemma = morpheme.surface;
    }

    // Fix compound verbs analyzed as ichidan but actually godan-sa
    // E.g., lemma=やりなおしる → やりなおす, lemma=たのしる → たのす (if non-kanji stem)
    if (morpheme.pos == core::PartOfSpeech::Verb &&
        endsWith(morpheme.lemma, "しる") &&
        morpheme.lemma.size() > core::kTwoJapaneseCharBytes) {
      std::string stem = morpheme.lemma.substr(
          0, morpheme.lemma.size() - core::kTwoJapaneseCharBytes);
      if (stem.size() == core::kJapaneseCharBytes && grammar::isAllKanji(stem)) {
        // Single kanji + しる → する (サ変: 対する, 関する)
        morpheme.lemma = stem + "する";
      } else if (stem.size() > core::kJapaneseCharBytes) {
        // Multi-char stem + しる → す (godan-sa: やりなおす)
        morpheme.lemma = stem + "す";
      }
    }

    // Fix special ra-row (ラ行特殊活用) verb lemma: ~いる → ~る
    // Verbs like ござる, いらっしゃる have renyokei ending in い (not り)
    // The inflection analyzer incorrectly reconstructs ~いる as base form
    // E.g., ござい → ございる (wrong) → ござる (correct)
    if (morpheme.pos == core::PartOfSpeech::Verb &&
        endsWith(morpheme.lemma, "いる") &&
        morpheme.lemma.size() >= core::kThreeJapaneseCharBytes &&
        dict_manager_ != nullptr) {
      std::string stem = morpheme.lemma.substr(
          0, morpheme.lemma.size() - core::kTwoJapaneseCharBytes);
      std::string ru_form = stem + "る";
      auto results = dict_manager_->lookup(ru_form, 0);
      for (const auto& r : results) {
        if (r.entry != nullptr && r.entry->pos == core::PartOfSpeech::Verb &&
            r.entry->surface == ru_form) {
          morpheme.lemma = ru_form;
          break;
        }
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
        // NOTE: Passive verbs ending in 〜れる (e.g., いわれる → いう, かかれる → かく)
        // are usually SPLIT by the tokenizer (読ま+れる), not kept as single tokens.
        // Single token 〜れる verbs are typically potential verbs (可能動詞) like:
        // 書ける, 泊まれる, 読める - these should keep lemma = surface.
        // So we DON'T mark 〜れる verbs for re-lemmatization here.
        // The earlier fix already sets lemma = surface for these patterns.
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
    // Get next morpheme for context-dependent fixes
    std::string_view next_surface;
    std::string_view next_lemma;
    if (i + 1 < morphemes.size()) {
      next_surface = morphemes[i + 1].surface;
      next_lemma = morphemes[i + 1].lemma;
    }
    // Fix onbin lemma using next morpheme context
    // イ音便: 書い+た/て → lemma should be 書く (not 書う)
    // 連用形: 使い+ます/にくい → lemma should be 使う (correct)
    // Pattern: surface ends with い, lemma ends with う, next is た/て/だ/で
    if (morpheme.pos == core::PartOfSpeech::Verb &&
        endsWith(morpheme.surface, "い") &&
        endsWith(morpheme.lemma, "う") &&
        morpheme.lemma.size() >= core::kTwoJapaneseCharBytes &&
        utf8::equalsAny(next_surface, {"た", "て", "だ", "で"})) {
      // This is onbin form - fix lemma from 〜う to 〜く or 〜ぐ
      std::string stem = morpheme.lemma.substr(0, morpheme.lemma.size() - core::kJapaneseCharBytes);
      // Check if next is voiced (だ/で) → 〜ぐ, otherwise → 〜く
      if (utf8::equalsAny(next_surface, {"だ", "で"})) {
        morpheme.lemma = stem + "ぐ";  // GodanGa: 泳い+だ → 泳ぐ
      } else {
        morpheme.lemma = stem + "く";  // GodanKa: 書い+た → 書く
      }
    }
    // Fix irregular sokuonbin: いっ+た/て → いく (not いう)
    // いく is the only godan-ka verb that uses 促音便 instead of イ音便
    // Apply when preceded by motion particle (に/へ) or adjective renyokei (〜く)
    // Do NOT apply after quotative markers (と/そう/こう etc.) where いっ = 言う
    if (morpheme.pos == core::PartOfSpeech::Verb &&
        endsWith(morpheme.surface, "いっ") &&
        morpheme.lemma.size() >= core::kTwoJapaneseCharBytes &&
        endsWith(morpheme.lemma, "いう") &&
        utf8::equalsAny(next_surface, {"た", "て", "たら", "ちゃ"}) &&
        i > 0) {
      bool has_motion_particle =
          utf8::equalsAny(morphemes[i - 1].surface, {"に", "へ"});
      bool has_adj_renyokei =
          morphemes[i - 1].pos == core::PartOfSpeech::Adjective &&
          endsWith(morphemes[i - 1].surface, "く");
      if (has_motion_particle || has_adj_renyokei) {
        std::string stem = morpheme.lemma.substr(
            0, morpheme.lemma.size() - core::kTwoJapaneseCharBytes);
        morpheme.lemma = stem + "いく";
      }
    }
    // Fix 仮定形 lemma: verb + ば → godan conditional, not ichidan potential
    // E.g., 書け+ば → lemma=書く (not 書ける), 行け+ば → lemma=行く (not 行ける)
    if (morpheme.pos == core::PartOfSpeech::Verb &&
        utf8::equalsAny(next_surface, {"ば"}) &&
        morpheme.lemma.size() >= core::kTwoJapaneseCharBytes) {
      // Special case: なけれ+ば → lemma=ない
      if (morpheme.surface == "なけれ") {
        morpheme.lemma = "ない";
      } else if (utf8::endsWithAny(morpheme.surface,
                     {"え", "け", "げ", "せ", "て", "ね", "べ", "め", "れ"})) {
        // Check if lemma looks like ichidan potential (ends with e-row + る)
        if (utf8::endsWithAny(morpheme.lemma,
                {"える", "ける", "げる", "せる", "てる",
                 "ねる", "べる", "める", "れる"})) {
          // Convert ichidan potential lemma to godan base
          // Remove trailing える/ける/... (6 bytes) and get the stem
          std::string stem = morpheme.lemma.substr(
              0, morpheme.lemma.size() - core::kTwoJapaneseCharBytes);
          // Map e-row ending to godan base: え→う, け→く, げ→ぐ, etc.
          std::string_view surface_tail(
              morpheme.surface.data() + morpheme.surface.size() - core::kJapaneseCharBytes,
              core::kJapaneseCharBytes);
          std::string godan_base;
          if (surface_tail == "え") godan_base = "う";
          else if (surface_tail == "け") godan_base = "く";
          else if (surface_tail == "げ") godan_base = "ぐ";
          else if (surface_tail == "せ") godan_base = "す";
          else if (surface_tail == "て") godan_base = "つ";
          else if (surface_tail == "ね") godan_base = "ぬ";
          else if (surface_tail == "べ") godan_base = "ぶ";
          else if (surface_tail == "め") godan_base = "む";
          else if (surface_tail == "れ") {
            // Check if this is ichidan conditional (食べれ+ば → 食べる)
            // rather than godan-ra conditional (取れ+ば → 取る)
            // For ichidan: surface_stem + る == original lemma
            std::string surface_stem = morpheme.surface.substr(
                0, morpheme.surface.size() - core::kJapaneseCharBytes);
            if (surface_stem + "る" == morpheme.lemma) {
              // Ichidan conditional - lemma is already correct, don't convert
            } else {
              godan_base = "る";
            }
          }
          if (!godan_base.empty()) {
            morpheme.lemma = stem + godan_base;
          }
        } else if (endsWith(morpheme.surface, "れ") &&
                   endsWith(morpheme.lemma, "る") &&
                   morpheme.surface.size() >= core::kTwoJapaneseCharBytes) {
          // Ichidan conditional: 食べれ+ば → lemma=食べる
          // Surface = stem + れ, correct lemma = stem + る
          std::string stem = morpheme.surface.substr(
              0, morpheme.surface.size() - core::kJapaneseCharBytes);
          morpheme.lemma = stem + "る";
        }
      }
    }
    // Fix 命令形 ろ ending lemma: ichidan imperative
    // E.g., 見ろ → lemma=見る (not 見ろる), 寝ろ → lemma=寝る (not 寝ろる)
    if (morpheme.pos == core::PartOfSpeech::Verb &&
        endsWith(morpheme.surface, "ろ")) {
      if (endsWith(morpheme.lemma, "ろる")) {
        // Lemma incorrectly derived as 〜ろる → fix to 〜る
        morpheme.lemma = morpheme.lemma.substr(
            0, morpheme.lemma.size() - core::kTwoJapaneseCharBytes) + "る";
      } else if (morpheme.lemma == morpheme.surface &&
                 morpheme.surface.size() >= core::kTwoJapaneseCharBytes) {
        // Lemma equals surface (e.g., 起きろ→起きろ) → fix to stem + る
        morpheme.lemma = morpheme.surface.substr(
            0, morpheme.surface.size() - core::kJapaneseCharBytes) + "る";
      }
    }
    // Fix たら lemma: should be た (not たら)
    if (morpheme.surface == "たら" && morpheme.lemma == "たら") {
      morpheme.lemma = "た";
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
