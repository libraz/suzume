/**
 * @file verb_candidates.cpp
 * @brief Verb-based unknown word candidate generation
 */

#include "verb_candidates.h"

#include <algorithm>
#include <cmath>

#include "analysis/bigram_table.h"
#include "analysis/candidate_constants.h"
#include "analysis/scorer_constants.h"
#include "analysis/tokenizer_utils.h"
#include "analysis/verb_candidates_auxiliary_patterns.h"
#include "analysis/verb_candidates_dictionary_probes.h"
#include "analysis/verb_candidates_emphatic.h"
#include "core/debug.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "unknown.h"

namespace suzume::analysis {

// Alias for helper functions
namespace vh = verb_helpers;

void generateCompoundVerbCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                    const std::vector<normalize::CharType>& char_types,
                                    const grammar::Inflection& inflection,
                                    const dictionary::DictionaryManager* dict_manager,
                                    const VerbCandidateOptions& verb_opts, std::vector<UnknownCandidate>& candidates) {
  // Requires dictionary to verify base forms
  if (dict_manager == nullptr) {
    return;
  }

  // Pattern: Kanji+ Hiragana(1-3) Kanji+ Hiragana+
  // e.g., 恐(K)れ(H)入(K)ります(H), 差(K)し(H)上(K)げます(H)
  if (start_pos >= char_types.size() || char_types[start_pos] != normalize::CharType::Kanji) {
    return;
  }

  // First kanji portion (1-2 chars) followed by a hiragana renyoukei ending
  size_t kanji1_end = findCharRegionEndBeforeHiragana(char_types, start_pos, 3, normalize::CharType::Kanji);
  if (kanji1_end == start_pos) {
    return;
  }

  // Find first hiragana portion (1-3 chars)
  size_t hira1_end = findCharRegionEnd(char_types, kanji1_end, 4, normalize::CharType::Hiragana);

  // Find second kanji portion (must exist for compound verb)
  if (hira1_end >= char_types.size() || char_types[hira1_end] != normalize::CharType::Kanji) {
    return;
  }

  size_t kanji2_end = findCharRegionEnd(char_types, hira1_end, 3, normalize::CharType::Kanji);

  // Skip compound verb candidates when second verb is aspectual/grammatical
  // These remain separate grammatical/aspectual search units:
  // 終わる/終える, 始める, 続く/続ける, 過ぎる
  // e.g., 読み終わる → 読み + 終わる (not single token)
  if (hira1_end < codepoints.size()) {
    char32_t second_kanji = codepoints[hira1_end];
    if (second_kanji == U'終' || second_kanji == U'始' || second_kanji == U'続' || second_kanji == U'過') {
      return;
    }
  }

  // Find second hiragana portion (conjugation ending)
  if (kanji2_end >= char_types.size() || char_types[kanji2_end] != normalize::CharType::Hiragana) {
    return;
  }

  size_t hira2_end = findCharRegionEnd(char_types, kanji2_end, 10, normalize::CharType::Hiragana);

  // Try different ending lengths
  for (size_t end_pos = hira2_end; end_pos > kanji2_end; --end_pos) {
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);

    // Keep a verb stem and the politeness auxiliary ます separate.
    // e.g., 申し上げます → 申し上げ + ます
    if (utf8::endsWithAny(surface, {"ます", "ました", "ません", "ましょう"})) {
      continue;
    }

    // Use inflection analyzer to get potential base forms
    const auto& inflection_candidates = inflection.analyze(surface);

    for (const auto& infl_cand : inflection_candidates) {
      if (infl_cand.confidence < verb_opts.confidence_low) {
        continue;
      }

      // Skip inflection candidates whose suffix contains particles (を, は, が, etc.)
      // These indicate the inflection analysis crossed a phrase boundary.
      // E.g., 行かざるを得ない: suffix="かざるを得ない" contains を (particle)
      // This is verb + auxiliary phrase, NOT a compound verb.
      if (utf8::containsAny(infl_cand.suffix, {"を", "は", "が"})) {
        continue;
      }

      // Skip inflection candidates whose suffix includes auxiliary endings
      // (た, て, で, たら, etc.) that should be separate tokens.
      // E.g., 振る舞った → suffix="った" includes た (past tense aux)
      // The shorter form (振る舞っ, onbinkei) will be matched in a later iteration,
      // allowing proper split: 振る舞っ + た
      if (utf8::endsWithAny(infl_cand.suffix, {"た", "て", "で", "たら", "たり"})) {
        continue;
      }

      // A causative or voice-chain ending is an auxiliary sequence, not a
      // lexical compound verb. Apply the same guard used by the kanji
      // inflection generator so 書かれさせる keeps 書か+れ+させる.
      if (verb_helpers::shouldSkipCausativeAuxPattern(surface, infl_cand.verb_type)) {
        continue;
      }

      // A bare potential stem with nothing to carry is the listed verb's own
      // e-row cell (咲き誇れ+心), not the potential (見つけ出せ+なかった).
      if (infl_cand.verb_type == grammar::VerbType::Ichidan && infl_cand.suffix.empty() &&
          verb_helpers::isStrandedPotentialStem(dict_manager, infl_cand.base_form, codepoints, end_pos)) {
        continue;
      }
      // Check if base form exists in dictionary as a verb
      if (verb_helpers::isVerbInDictionary(dict_manager, infl_cand.base_form)) {
        // v0.8: conj_type removed - just verify verb exists in dictionary
        // Note: Don't set lemma here - let lemmatizer derive it more accurately
        auto compound = makeVerbCandidate(surface, start_pos, end_pos, verb_opts.base_cost_low, "",
                                          dictionary::ConjugationType::None,  // v0.8: conj_type no longer used
                                          false, CandidateOrigin::VerbCompound, infl_cand.confidence,
                                          grammar::verbTypeToString(infl_cand.verb_type).data());
        // The complete inflection lemma was just verified above. Preserve that
        // evidence even though the public lemma is deferred to the lemmatizer,
        // so internal homograph candidates cannot reopen this compound span.
        compound.lemma_verified = true;
        candidates.push_back(std::move(compound));
        return;  // Return first valid match
      }
    }
  }
}

void generateKatakanaVerbCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                    const std::vector<normalize::CharType>& char_types,
                                    const grammar::Inflection& inflection,
                                    const dictionary::DictionaryManager* dict_manager,
                                    const VerbCandidateOptions& verb_opts, std::vector<UnknownCandidate>& candidates) {
  // The buffer is shared with the other generators for this position, so the
  // variant, cost, and ordering steps below stay inside the appended range.
  const size_t candidate_start = candidates.size();

  // Only process katakana-starting positions
  if (start_pos >= char_types.size() || char_types[start_pos] != normalize::CharType::Katakana) {
    return;
  }

  // Katakana portion (1-8 characters for slang verb stems) followed by hiragana conjugation endings
  size_t kata_end = findCharRegionEndBeforeHiragana(char_types, start_pos, 8, normalize::CharType::Katakana);
  if (kata_end == start_pos) {
    return;
  }

  // Check if first hiragana could be a verb ending
  // Common verb endings start with: る, っ, ん, ら, り, れ, ろ, さ, し, せ, た, て, etc.
  char32_t first_hira = codepoints[kata_end];
  // Skip if it's clearly a particle
  if (normalize::isParticleCodepoint(first_hira)) {
    return;
  }

  // Find hiragana portion (conjugation endings, up to 10 chars)
  size_t hira_end = findCharRegionEnd(char_types, kata_end, 10, normalize::CharType::Hiragana);

  // Need at least 1 hiragana for conjugation
  if (hira_end <= kata_end) {
    return;
  }

  // A katakana run closing on an e-row or i-row mora is an ichidan stem when
  // what follows selects a continuative (ウケ+ました, バテ+た, コケ+て). The
  // negative selects the irrealis, which for an ichidan verb is the same cell
  // and no evidence of a verb over a noun (ペン+ない), so it counts only after
  // an e-row mora, where the noun reading is rare (ブレ+ない, ズレ+なかった). A
  // long-vowel mark spells a loanword, never a native ichidan stem (ケーキ), and
  // the stem is the whole katakana run, not its tail (ケー+キ).
  bool splits_negative_stem = false;
  const bool opens_katakana_run = start_pos == 0 || char_types[start_pos - 1] != normalize::CharType::Katakana;
  const bool has_long_vowel_mark = std::find(codepoints.begin() + static_cast<std::ptrdiff_t>(start_pos),
                                             codepoints.begin() + static_cast<std::ptrdiff_t>(kata_end),
                                             U'ー') != codepoints.begin() + static_cast<std::ptrdiff_t>(kata_end);
  if (dict_manager != nullptr && opens_katakana_run && !has_long_vowel_mark) {
    constexpr char32_t kKatakanaFirst = U'ァ';
    constexpr char32_t kKatakanaLast = U'ヶ';
    constexpr char32_t kKatakanaToHiragana = U'ァ' - U'ぁ';
    const char32_t stem_final = codepoints[kata_end - 1];
    const char32_t stem_final_hiragana =
        stem_final >= kKatakanaFirst && stem_final <= kKatakanaLast ? stem_final - kKatakanaToHiragana : 0;
    // A content word reaching past the selecting auxiliary is the reading the
    // kana belongs to (ケーキ+たべた, ケーキ+たのしみ), not the auxiliary.
    size_t selecting_end = 0;
    size_t content_end = 0;
    bool selects_irrealis = false;
    if (kana::isERowCodepoint(stem_final_hiragana) || kana::isIRowCodepoint(stem_final_hiragana)) {
      for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, kata_end, hira_end)) {
        if (match.entry == nullptr) {
          continue;
        }
        const auto epos = match.entry->extended_pos;
        const size_t match_end = kata_end + match.length;
        const bool negative_after_e_row =
            epos == core::ExtendedPOS::AuxNegativeNai && kana::isERowCodepoint(stem_final_hiragana);
        if (epos == core::ExtendedPOS::AuxTenseMasu || epos == core::ExtendedPOS::AuxTenseTa ||
            epos == core::ExtendedPOS::AuxDesireTai || negative_after_e_row ||
            (epos == core::ExtendedPOS::ParticleConj && codepoints[kata_end] == U'て')) {
          selecting_end = std::max(selecting_end, match_end);
          selects_irrealis = selects_irrealis || negative_after_e_row;
        } else if (match.entry->pos == core::PartOfSpeech::Verb || match.entry->pos == core::PartOfSpeech::Adjective ||
                   match.entry->pos == core::PartOfSpeech::Noun || match.entry->pos == core::PartOfSpeech::Adverb) {
          content_end = std::max(content_end, match_end);
        }
      }
    }
    const bool selects_continuative = selecting_end > 0 && content_end <= selecting_end;
    // Near-neutral, so the selecting auxiliary's connection decides against the noun.
    if (selects_continuative) {
      const std::string stem = extractSubstring(codepoints, start_pos, kata_end);
      candidates.push_back(
          makeVerbCandidate(stem, start_pos, kata_end, candidate::verb_cost::kWeakPenalty, stem + "る",
                            dictionary::ConjugationType::Ichidan, true, CandidateOrigin::VerbKatakana,
                            candidate::kNoConfidence, "katakana_ichidan_renyokei",
                            selects_irrealis ? core::ExtendedPOS::VerbMizenkei : core::ExtendedPOS::VerbRenyokei));
    }
    // The stem and the negative are separate tokens, as for a kanji ichidan
    // verb (食べ+ない), so the whole-verb candidate below is not offered.
    splits_negative_stem = selects_continuative && selects_irrealis;
  }

  // A katakana stem followed by an auxiliary or a する cell is split rather
  // than read as one verb: すぎ (シンプル+すぎる), the appearance そう,
  // し/さ/せ (コピー+し+て, コピー+さ+れる, コピー+さ+せる), and the
  // desiderative たい/たく (the past バテた is unaffected).
  std::string hira_part = extractSubstring(codepoints, kata_end, hira_end);
  if (utf8::startsWithAny(hira_part, {"すぎ", "そう", "し", "さ", "せ", "たい", "たく"})) {
    return;
  }

  // The surface って is ambiguous between a verb's te-form and the
  // quotative particle.  A full unknown-verb candidate cannot resolve that
  // lexical ambiguity: it would make every katakana noun look like a
  // godan-ra verb.  Leave the boundary to the verified sokuonbin path below.
  const bool starts_quotative_tte = utf8::startsWith(hira_part, "って");

  // Katakana denominal verbs are all godan-ra (バグる, メモる, トラブる), so
  // every stem cell below shares the reconstructed lemma.
  const std::string denominal_lemma = extractSubstring(codepoints, start_pos, kata_end) + "る";
  const std::string stem_cell = extractSubstring(codepoints, start_pos, kata_end + 1);

  // A katakana stem of two or more morae productively forms a denominal
  // terminal verb with る (テンパる, バグる). This is a morphological rule,
  // not a lexical list: the complete terminal shape is enough evidence, while
  // the one-mora stems remain with the ordinary ambiguity handling below.
  if (kata_end - start_pos >= 2 && hira_part == "る") {
    const std::string surface = extractSubstring(codepoints, start_pos, hira_end);
    candidates.push_back(makeVerbCandidate(surface, start_pos, hira_end, verb_opts.base_cost_standard, surface,
                                           dictionary::ConjugationType::GodanRa, true, CandidateOrigin::VerbKatakana,
                                           candidate::kNoConfidence, "katakana_denominal_ru",
                                           core::ExtendedPOS::VerbShuushikei));
  }

  // The same denominal verb spells its continuative with り before what a
  // continuative selects (ディスり+すぎ, テンパり+ます).
  if (kata_end - start_pos >= 2 && hira_end > kata_end + 1 && codepoints[kata_end] == U'り' &&
      dict_manager != nullptr) {
    bool selects_continuative = false;
    for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, kata_end + 1, hira_end)) {
      if (match.entry == nullptr) {
        continue;
      }
      const auto epos = match.entry->extended_pos;
      selects_continuative = selects_continuative || epos == core::ExtendedPOS::AuxExcessive ||
                             epos == core::ExtendedPOS::AuxTenseMasu || epos == core::ExtendedPOS::AuxDesireTai ||
                             epos == core::ExtendedPOS::AuxAppearanceSou || epos == core::ExtendedPOS::ParticleConj;
    }
    if (selects_continuative) {
      candidates.push_back(makeVerbCandidate(stem_cell, start_pos, kata_end + 1, verb_opts.base_cost_standard,
                                             denominal_lemma, dictionary::ConjugationType::GodanRa, true,
                                             CandidateOrigin::VerbKatakana, candidate::kNoConfidence,
                                             "katakana_denominal_renyokei", core::ExtendedPOS::VerbRenyokei));
    }
  }

  // Try different ending lengths, starting from longest
  for (size_t end_pos = hira_end; !starts_quotative_tte && end_pos > kata_end; --end_pos) {
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);

    // Check if this looks like a conjugated verb using inflection analyzer
    auto best = inflection.getBest(surface);

    // Only accept verb types (not IAdjective) and require reasonable confidence
    // Reject Suru type when hiragana doesn't start with する conjugation (し/さ/せ/す)
    // e.g., ハメた → inflection says Suru but た is not する conjugation → use ichidan instead
    if (best.verb_type == grammar::VerbType::Suru) {
      // Suru inflection was incorrectly matched; try next best candidate
      const auto& all_results = inflection.analyze(surface);
      best.confidence = 0.0F;
      for (const auto& cand : all_results) {
        if (cand.verb_type != grammar::VerbType::Suru && cand.verb_type != grammar::VerbType::IAdjective &&
            cand.confidence > best.confidence) {
          best = cand;
        }
      }
    }
    if (best.confidence > verb_opts.confidence_katakana && best.verb_type != grammar::VerbType::IAdjective) {
      // Lower cost than pure katakana noun to prefer verb reading
      // Cost: 0.4-0.55 based on confidence (lower = better)
      float cost = candidate::confidenceScaledCost(verb_opts.base_cost_standard, best.confidence,
                                                   verb_opts.confidence_cost_scale);
      // An ichidan stem ends on a full-size e-row or i-row mora, so a run closing
      // on anything else (ペン, カフェ, コーヒー) has no ichidan reading at all.
      const char32_t stem_final = codepoints[kata_end - 1];
      const bool small_vowel = utf8::equalsAny(normalize::encodeUtf8(stem_final), {"ァ", "ィ", "ゥ", "ェ", "ォ"});
      const char32_t stem_final_hiragana =
          stem_final >= U'ァ' && stem_final <= U'ヶ' ? stem_final - (U'ァ' - U'ぁ') : 0;
      const bool ichidan_stem_final =
          !small_vowel && (kana::isERowCodepoint(stem_final_hiragana) || kana::isIRowCodepoint(stem_final_hiragana));
      const bool whole_negative_form = best.verb_type == grammar::VerbType::Ichidan &&
                                       vh::naiNegativeFormLengthAt(codepoints, kata_end) == end_pos - kata_end;
      if (whole_negative_form && splits_negative_stem) {
        continue;
      }
      // After an i-row mora the noun plus the adjective nai is the natural
      // reading (ピンチ+ない), so the fused whole verb is discouraged there too.
      if (whole_negative_form && (!ichidan_stem_final || kana::isIRowCodepoint(stem_final_hiragana))) {
        cost += candidate::verb_cost::kKatakanaIchidanNegativePenalty;
      }
      candidates.push_back(makeVerbCandidate(
          surface, start_pos, end_pos, cost, best.base_form, grammar::verbTypeToConjType(best.verb_type), false,
          CandidateOrigin::VerbKatakana, best.confidence, grammar::verbTypeToString(best.verb_type).data()));
    }
  }

  // Add emphatic variants (パニくるっ, etc.)
  vh::addEmphaticVariants(candidates, codepoints, candidate_start);

  auto push_stem_cell = [&](float cost, const char* label, core::ExtendedPOS extended_pos) {
    SUZUME_DEBUG_VERBOSE_BLOCK {
      SUZUME_DEBUG_STREAM << "[VERB_CAND] " << stem_cell << " " << label << " lemma=" << denominal_lemma
                          << " cost=" << cost << "\n";
    }
    candidates.push_back(makeVerbCandidate(stem_cell, start_pos, kata_end + 1, cost, denominal_lemma,
                                           dictionary::ConjugationType::GodanRa, true, CandidateOrigin::VerbKatakana,
                                           0.9F, label, extended_pos));
  };

  // Sokuonbin stem before た/て/だ/で (バズっ+た, ググっ+て).
  if (utf8::startsWithAny(hira_part, {"った", "って", "っだ", "っで"})) {
    // A loanword noun plus the quotative って has the same shape as a
    // godan-ra te-form, so that cell still needs lexical evidence for the
    // reconstructed lemma. The past cell has no such homograph -- った is
    // no particle -- and the denominal godan-ra row is productive, so the
    // past candidate stands on its own.
    const bool quotative_homograph = utf8::startsWithAny(hira_part, {"って", "っで"});
    // A follower the quotative cannot take but the te-form can (ます, the
    // aspectual いる) leaves only the te-form reading for って. One neither
    // takes (おい as an interjection) is no evidence either way.
    bool quotative_cannot_continue = false;
    if (dict_manager != nullptr && kata_end + 2 < codepoints.size()) {
      for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, kata_end + 2, codepoints.size())) {
        if (match.entry == nullptr) {
          continue;
        }
        const auto epos = match.entry->extended_pos;
        quotative_cannot_continue =
            quotative_cannot_continue ||
            (BigramTable::getCost(core::ExtendedPOS::ParticleQuote, epos) >= bigram_cost::kAlmostNever &&
             BigramTable::getCost(core::ExtendedPOS::ParticleConj, epos) < bigram_cost::kAlmostNever);
      }
    }
    if (quotative_homograph && !verb_helpers::isVerbInDictionary(dict_manager, denominal_lemma) &&
        !verb_helpers::contractedTeContinuationFollowsAt(codepoints, kata_end + 2) && !quotative_cannot_continue) {
      SUZUME_DEBUG_VERBOSE_BLOCK {
        SUZUME_DEBUG_STREAM << "[VERB_SKIP] \"" << denominal_lemma
                            << "\" is not a verified verb, skip katakana_sokuonbin\n";
      }
    } else {
      // Neutral cost — let bigram connections decide between verb+て and noun+って
      push_stem_cell(0.1F, "katakana_sokuonbin", core::ExtendedPOS::VerbOnbinkei);
    }
  }

  // Irrealis stems before the negative, the causative and the volitional う
  // (ググら+ない, バズら+せる, ググろ+う); the negative cost beats unsplit forms.
  constexpr float kIrrealisStemCost = -0.5F;
  if (utf8::startsWithAny(hira_part, {"らな", "らせ"})) {
    push_stem_cell(kIrrealisStemCost, "katakana_mizenkei", core::ExtendedPOS::VerbMizenkei);
  }
  if (utf8::startsWith(hira_part, "ろう")) {
    push_stem_cell(kIrrealisStemCost, "katakana_volitional", core::ExtendedPOS::VerbMizenkei);
  }

  // Sort by cost
  vh::sortCandidatesByCost(candidates, candidate_start);
}

}  // namespace suzume::analysis
