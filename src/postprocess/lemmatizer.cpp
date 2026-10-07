#include "postprocess/lemmatizer.h"

#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/inflection_scorer_constants.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "postprocess/lemmatizer_internal.h"

namespace suzume::postprocess {

Lemmatizer::Lemmatizer() = default;

Lemmatizer::Lemmatizer(const dictionary::DictionaryManager* dict_manager) : dict_manager_(dict_manager) {}

using namespace lemmatizer_detail;

std::string Lemmatizer::lemmatize(const core::Morpheme& morpheme) const {
  // A terminal small-tsu can be a colloquial emphasis mark after a past form
  // (来たっ).  Analyze the underlying past form, while leaving dictionary and
  // non-past emphatic words such as 行くっ untouched.
  if (morpheme.pos == core::PartOfSpeech::Verb && utf8::endsWith(morpheme.surface, "たっ")) {
    const std::string_view past_surface = utf8::dropLastChar(morpheme.surface);
    if (std::string lemma = lemmatizeByGrammar(past_surface, morpheme.pos, morpheme.conj_type); !lemma.empty()) {
      return lemma;
    }
  }

  // If morpheme is from dictionary and has distinct lemma set, trust it
  // (lemma != surface means it was explicitly set, not defaulted)
  // When lemma == surface, we need to re-derive for conjugated forms
  if (morpheme.fromDictionary() && !morpheme.lemma.empty() && morpheme.lemma != morpheme.surface) {
    return morpheme.lemma;
  }

  // An ichidan verb in 終止形/連体形 whose surface ends in てる (立てる, 捨てる, 組み立てる)
  // is already its own dictionary form, so its lemma IS the surface. Return it directly,
  // overriding any lemma the candidate edge may have mis-derived (verb_candidates gives
  // 立てる the bogus ichidan base 立る) and skipping the re-derivation below, whose
  // ている-contraction rule (kVerbEndings "てる"→"る") would otherwise corrupt these to
  // 立る/捨る/組み立る. Real ている-contractions (見てる) never reach here — the tokenizer
  // always splits them (見+てる) — so no whole 終止形 てる-verb is a contraction. Scoped to
  // てる (not all base verbs) so genuine re-derivations stay intact, e.g. potential
  // バズれる→バズる. A dictionary entry with its own distinct lemma is returned above.
  if (morpheme.pos == core::PartOfSpeech::Verb &&
      (morpheme.extended_pos == core::ExtendedPOS::VerbShuushikei ||
       morpheme.extended_pos == core::ExtendedPOS::VerbRentaikei) &&
      utf8::endsWith(morpheme.surface, "てる")) {
    return std::string(morpheme.surface);
  }

  // Likewise for dictionary-backed 終止形/連体形 verbs ending in せる (見せる,
  // 合わせる, and lexical compounds like 組み合わせる): the surface is already
  // the dictionary form, so its lemma IS the surface. The grammar re-derivation
  // below would misread the ichidan せる ending as the causative auxiliary and
  // strip it (組み合わせる → 組み合う). Scoped to dictionary-backed edges: a
  // token that is genuinely mizenkei+させる never reaches here as one
  // dictionary-flagged 終止形 token (the tokenizer splits it: 話し合わ+せる).
  if (morpheme.fromDictionary() && morpheme.pos == core::PartOfSpeech::Verb &&
      (morpheme.extended_pos == core::ExtendedPOS::VerbShuushikei ||
       morpheme.extended_pos == core::ExtendedPOS::VerbRentaikei) &&
      utf8::endsWith(morpheme.surface, "せる")) {
    return std::string(morpheme.surface);
  }

  // Tari-adjective adverbs: remove trailing と from lemma (颯爽と → 颯爽, 堂々と → 堂々)
  // This check runs even for dictionary entries where lemma == surface
  if (morpheme.pos == core::PartOfSpeech::Adverb) {
    std::string tari_stem = fixTariAdverb(morpheme.surface);
    if (!tari_stem.empty()) {
      return tari_stem;
    }
  }

  // If lemma is already set and different from surface, use it
  // (lemma == surface means it's a default that may need re-derivation)
  if (!morpheme.lemma.empty() && morpheme.lemma != morpheme.surface) {
    // Fix potential verb (可能動詞) lemma FIRST: 泊まれる should have lemma=泊まれる, not 泊む
    if (std::string potential = fixPotentialVerb(morpheme); !potential.empty()) {
      return potential;
    }

    // Preserve the ない lemma in the adjective + さ + そう pattern.
    // なさそう = ない + さ + そう (looks like there isn't)
    // The inflection analyzer incorrectly derives lemma なさい (from なさ + そう)
    // but the correct lemma is ない (from な + さそう)
    if (morpheme.pos == core::PartOfSpeech::Adjective && morpheme.surface.find("なさそう") == 0) {
      return "ない";
    }

    // Special fix for katakana + すぎる patterns
    // The inflection analyzer incorrectly derives lemma like ワンパターンる
    // when the correct form is ワンパターンすぎる
    std::string_view surface = morpheme.surface;
    std::string_view lemma = morpheme.lemma;
    if (surface.size() >= core::kThreeJapaneseCharBytes && lemma.size() >= core::kJapaneseCharBytes) {
      std::string_view surface_ending = utf8::last3Chars(surface);
      bool has_sugiru_aux = utf8::equalsAny(surface_ending, {"すぎる", "すぎた", "すぎて"});
      std::string_view lemma_ending = utf8::lastChar(lemma);
      // Check if lemma ends with just る but surface ends with すぎる
      // E.g., surface=ワンパターンすぎる, lemma=ワンパターンる (incorrect)
      if (has_sugiru_aux && lemma_ending == "る" && lemma.size() < surface.size()) {
        // A katakana stem (lemma minus る) takes the whole すぎる back.
        const std::string_view stem = utf8::dropLastChar(lemma);
        if (!stem.empty() && normalize::classifyChar(utf8::decodeFirstChar(stem)) == normalize::CharType::Katakana) {
          return normalize::concat(stem, "すぎる");
        }
      }
    }

    // Fix special ra-row (ラ行特殊活用) verb lemma: ~いる → ~る
    // Verbs like ござる, いらっしゃる have renyokei ending in い (not り)
    // The inflection analyzer incorrectly reconstructs ~いる as base form
    // E.g., ござい → ございる (wrong) → ござる (correct)
    if (morpheme.pos == core::PartOfSpeech::Verb) {
      if (std::string ru_form = fixSpecialRaRowLemma(morpheme.lemma, dict_manager_); !ru_form.empty()) {
        return ru_form;
      }
    }

    // Check for サ変動詞 classical form: 漢字2文字以上+す → 漢字+する (確認す → 確認する)
    if (morpheme.pos == core::PartOfSpeech::Verb) {
      if (std::string suru = fixSuruClassical(morpheme.lemma, morpheme.conj_type); !suru.empty()) {
        return suru;
      }
    }

    // Fix 撥音便 lemma: if lemma ends with む but dictionary has ぶ or ぬ, use that
    // E.g., 学ん → lemma=学む (wrong) → should be 学ぶ (correct)
    // The candidate generator may produce wrong lemma when dictionary lookup fails
    if (morpheme.pos == core::PartOfSpeech::Verb && utf8::endsWith(morpheme.surface, "ん") &&
        utf8::endsWith(morpheme.lemma, "む") && morpheme.lemma.size() >= core::kTwoJapaneseCharBytes) {
      if (std::string fixed = fixHatsuonbin(utf8::dropLastChar(morpheme.lemma), dict_manager_); !fixed.empty()) {
        return fixed;
      }
      // No correction found - keep the original む form
    }

    return morpheme.lemma;
  }

  if (morpheme.pos == core::PartOfSpeech::Verb) {
    std::string suru_passive = lemmatizeSuruPassiveWithDictionary(morpheme.surface, dict_manager_);
    if (!suru_passive.empty()) {
      return suru_passive;
    }
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
      // Tari-adjective adverbs: remove trailing と from lemma (颯爽と → 颯爽, 堂々と → 堂々).
      // For Adverb this call is a no-op — an Adverb surface was already run through
      // fixTariAdverb at the top of lemmatize() and returned there if it matched, so
      // by here it never matches. It is retained (not narrowed) because this case
      // also covers Noun/Pronoun/Particle/Auxiliary/Conjunction, which do NOT hit
      // that early path and legitimately need the correction.
      if (std::string tari_stem = fixTariAdverb(morpheme.surface); !tari_stem.empty()) {
        return tari_stem;
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

  if (morpheme.pos == core::PartOfSpeech::Verb) {
    std::string contracted = lemmatizeContractedVerbWithDictionary(morpheme.surface, dict_manager_);
    if (!contracted.empty()) {
      return contracted;
    }
  }

  // Try grammar-based lemmatization for verbs and adjectives
  // Pass POS and conj_type to filter candidates appropriately
  std::string grammar_result = lemmatizeByGrammar(morpheme.surface, morpheme.pos, morpheme.conj_type);
  // Grammar-based lemmatization is authoritative - use its result even if
  // it equals the surface (which means the surface is already a dictionary form)
  // Only fall back to rule-based if grammar analysis returned empty/failed
  if (!grammar_result.empty()) {
    // Check for サ変動詞 classical form: 漢字2文字以上+す → 漢字+する (勉強す → 勉強する)
    // Check for compound しる verbs: 対しる → 対する, やりなおしる → やりなおす
    if (morpheme.pos == core::PartOfSpeech::Verb) {
      if (std::string suru = fixSuruClassical(grammar_result, morpheme.conj_type); !suru.empty()) {
        return suru;
      }
      // fixShiru rewrites an ichidan-misanalyzed サ変/godan-sa ~しる (対しる→対する).
      // A genuine GodanRa verb ending in しる (走る/はしる) must not be touched.
      if (morpheme.conj_type != dictionary::ConjugationType::GodanRa) {
        if (std::string shiru = fixShiru(grammar_result, morpheme.surface); !shiru.empty()) {
          return shiru;
        }
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
      if (std::string potential = fixPotentialVerb(morpheme); !potential.empty()) {
        return potential;
      }

      std::string rule_result = lemmatizeVerb(morpheme.surface);
      if (rule_result != morpheme.surface) {
        return rule_result;
      }
    }
    // Preserve the ない lemma in the adjective + さ + そう pattern.
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
      if (utf8::endsWith(sfc, "ん") && utf8::endsWith(grammar_result, "る") &&
          grammar_result.size() >= core::kTwoJapaneseCharBytes) {
        if (std::string fixed = fixHatsuonbin(utf8::dropLastChar(grammar_result), dict_manager_); !fixed.empty()) {
          return fixed;
        }
      }
      // イ音便: surface ends with い, result ends with う → く or ぐ
      // GodanKa (書い → 書く) and GodanGa (泳い → 泳ぐ) both have イ音便
      // BUT: Godan-wa renyokei also ends with い and gives 〜う (使い → 使う)
      // Only apply onbin fix if grammar_result (〜う) is NOT in dictionary
      if (utf8::endsWith(sfc, "い") && utf8::endsWith(grammar_result, "う") &&
          grammar_result.size() >= core::kTwoJapaneseCharBytes) {
        // First check if grammar_result is a valid verb in dictionary
        // If so, it's likely a godan-wa verb (使う, 買う, etc.), not onbin
        if (hasExactVerbEntry(dict_manager_, grammar_result)) {
          // grammar_result (e.g., 使う) is valid - use it directly. Prefix
          // entries are not evidence for the complete generated lemma.
          return grammar_result;
        }
        // grammar_result not found in dictionary - try onbin correction.
        // Reverse-derive the godan base from the イ音便 table (く before ぐ),
        // matching candidate generation's order so this fallback and the analysis
        // layer agree on ties (a stem in the dictionary as both, e.g. つく/つぐ).
        const std::string_view stem = utf8::dropLastChar(grammar_result);
        if (std::string base_form = firstAttestedGodanBase(dict_manager_, stem, "い"); !base_form.empty()) {
          return base_form;
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

}  // namespace suzume::postprocess
