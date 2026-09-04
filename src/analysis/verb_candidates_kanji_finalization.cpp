/**
 * @file verb_candidates_kanji_finalization.cpp
 * @brief Final validation and emission for selected kanji verb candidates
 */

#include <algorithm>
#include <cmath>

#include "analysis/bigram_table.h"
#include "analysis/candidate_constants.h"
#include "analysis/scorer_constants.h"
#include "analysis/verb_candidates_helpers.h"
#include "analysis/verb_candidates_kanji_internal.h"
#include "core/debug.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "grammar/inflection_scorer_constants.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "unknown.h"
#include "verb_candidates.h"

namespace suzume::analysis::kanji_verb_detail {
namespace vh = verb_helpers;

void appendSelectedKanjiVerbCandidate(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                      size_t stem_end, size_t end_pos, const std::string& surface,
                                      const std::string& hiragana_part, const grammar::InflectionCandidate& best,
                                      bool is_dict_verified, bool follows_reduplicated_noun,
                                      const grammar::Inflection& inflection,
                                      const dictionary::DictionaryManager* dict_manager,
                                      const VerbCandidateOptions& verb_opts, bool sokuonbin_stem_verified,
                                      const std::string& sokuonbin_lemma, std::vector<UnknownCandidate>& candidates) {
  // Only proceed if we found a matching candidate
  // Use lower threshold for valid i-row ichidan stems (感じ, 信じ, etc.)
  // but not single-kanji + い patterns (人い → 人 + いる)
  bool proceed_is_i_row_ichidan = best.verb_type == grammar::VerbType::Ichidan && vh::isValidIRowIchidanStem(best.stem);
  // A multi-kanji godan-wa renyokei ending in い can be the first half of
  // a productive compound predicate (背負い進む). The inflection scorer
  // conservatively lowers its confidence because the same shape is often
  // an i-adjective. Admit the candidate at the dictionary threshold when
  // a kanji continuation follows; connection scoring will retain it only
  // before a verified verb.
  bool is_multi_kanji_godan_wa_renyokei = best.verb_type == grammar::VerbType::GodanWa &&
                                          utf8::endsWith(surface, "い") && normalize::utf8Length(best.stem) >= 2 &&
                                          end_pos < codepoints.size() &&
                                          normalize::isKanjiCodepoint(codepoints[end_pos]);
  const bool has_mixed_godan_ka_stem =
      best.verb_type == grammar::VerbType::GodanKa && stem_end > kanji_end + 1 &&
      vh::hasConjunctiveParticleDictionaryEntry(dict_manager, normalize::encodeUtf8(codepoints[kanji_end]));

  // A case particle followed by する in its て/た form is a productive
  // nominal construction. Do not reinterpret a short noun plus that
  // closed-class sequence as an unregistered GodanSa verb (本+と+し+た,
  // 紙+に+し+て). Registered lexical verbs remain available.
  bool is_unregistered_godan_sa =
      best.verb_type == grammar::VerbType::GodanSa && !vh::isVerbInDictionary(dict_manager, best.base_form);
  if (is_unregistered_godan_sa && utf8::endsWith(best.stem, "く")) {
    const std::string adjective_base = normalize::concat(utf8::dropLastChar(best.stem), "い");
    if (vh::isAdjectiveInDictionary(dict_manager, adjective_base)) {
      return;
    }
  }
  if (is_unregistered_godan_sa && stem_end == kanji_end + 1 && (best.suffix == "して" || best.suffix == "した") &&
      vh::hasCaseParticleDictionaryEntry(dict_manager, normalize::encodeUtf8(codepoints[stem_end - 1]))) {
    SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << surface << "\" godan_sa case-particle+する pattern\n");
    return;
  }

  // Skip fake ichidan candidates with stem ending in さ (a-row)
  // These are typically suru-verb causative/passive patterns:
  //   勉強させられた → 勉強 + さ + せ + られ + た (NOT ichidan 勉強さる)
  // Valid ichidan stems end in e-row or i-row, not a-row
  if (best.verb_type == grammar::VerbType::Ichidan && !best.stem.empty() &&
      best.stem.size() >= 2 * core::kJapaneseCharBytes) {
    std::string_view last_char(best.stem.data() + best.stem.size() - core::kJapaneseCharBytes,
                               core::kJapaneseCharBytes);
    // さ is a-row hiragana (not valid for ichidan verb stems)
    if (last_char == "さ") {
      SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << surface << "\" stem ends with さ (suru-verb causative pattern)\n");
      return;
    }
  }
  // Dictionary-verified candidates use lower threshold (0.3)
  // This allows hiragana verbs like いわれる (conf=0.33) to be recognized
  float proceed_threshold =
      (is_dict_verified || proceed_is_i_row_ichidan || is_multi_kanji_godan_wa_renyokei)
          ? verb_opts.confidence_ichidan_dict
          : (has_mixed_godan_ka_stem ? (utf8::startsWith(best.suffix, "いた") || utf8::startsWith(best.suffix, "いて")
                                            ? verb_opts.confidence_past_te
                                            : verb_opts.confidence_low)
                                     : verb_opts.confidence_standard);
  if (best.confidence > proceed_threshold ||
      (follows_reduplicated_noun && best.confidence >= verb_opts.confidence_ichidan_dict) ||
      (is_multi_kanji_godan_wa_renyokei && best.confidence >= proceed_threshold)) {
    if (surface == "付け" && end_pos < codepoints.size() && codepoints[end_pos] == U'で') {
      return;  // 付けで is formal noun + particle, not 付ける renyokei.
    }

    // Reject Godan verbs with stems ending in e-row hiragana
    // E-row endings (え,け,せ,て,ね,へ,め,れ) are typically ichidan stems
    // E.g., "伝えいた" falsely matches as GodanKa "伝えく" but 伝える is ichidan
    // Exception: GodanRa (passive/causative) with "られ" suffix is valid
    // E.g., "定められた" has stem "定め" (ichidan) + passive suffix
    bool is_godan = grammar::isGodanVerbType(best.verb_type);
    if (is_godan && stem_end > kanji_end && stem_end <= codepoints.size()) {
      // Check if the last character of the stem is e-row hiragana
      char32_t last_char = codepoints[stem_end - 1];
      if (grammar::isERowCodepoint(last_char)) {
        // Exception: GodanRa with passive/causative suffix (られ) is valid
        // This occurs with ichidan verb stem + passive auxiliary
        bool is_passive_pattern = (best.verb_type == grammar::VerbType::GodanRa && utf8::contains(surface, "られ"));
        if (!is_passive_pattern) {
          return;  // Skip - e-row stem is typically ichidan, not godan
        }
      }
    }

    // Skip Suru verb renyokei (し) if followed by te/ta form particles
    // e.g., "勉強して" should be parsed as single token, not "勉強し" + "て"
    if (best.verb_type == grammar::VerbType::Suru && hiragana_part == "し" && end_pos < codepoints.size()) {
      char32_t next_char = codepoints[end_pos];
      if (next_char == U'て' || next_char == U'た' || next_char == U'で' || next_char == U'だ') {
        return;  // Skip - let the longer te-form candidate win
      }
    }

    // Skip GodanSa renyokei (漢字+し/漢字+とし etc.) when not in dictionary
    // e.g., "得し" misrecognized as GodanSa "得す" renyokei, but actually "得+し"
    // e.g., "証とし" misrecognized as GodanSa "証とす" renyokei, but actually "証+として"
    // MeCab splits as: 得(名詞) + し(する連用形) + た(過去)
    // Exception: Real GodanSa verbs like "愛す", "汚す" should not be skipped
    if (best.verb_type == grammar::VerbType::GodanSa && utf8::endsWith(hiragana_part, "し") &&
        kanji_end - start_pos <= 3) {
      // Check if the base form (stem+す) is a registered GodanSa verb
      // For single-char hiragana_part "し": base = kanji + す
      // For multi-char like "とし": base = kanji + と + す = 証とす
      std::string base_stem = extractSubstring(codepoints, start_pos, stem_end);
      std::string base_form = base_stem + "す";
      if (isInterrogativeKanji(codepoints[start_pos]) || !vh::isVerbInDictionary(dict_manager, base_form)) {
        // An interrogative is a standalone argument, never a verb stem.
        return;
      }
    }

    // Skip GodanMa renyokei (漢字+み) when base form is not in dictionary.
    // Renyokei み competes with auxiliary みたい — without dict verification,
    // 猫みたい (noun+aux) cannot be distinguished from 読みたい (verb+aux).
    // Requires all single-kanji GODAN_MA verbs to be enumerated in L2 dict.
    // A literary auxiliary standing on the み supplies the missing evidence by
    // itself: みたい is one auxiliary, so it can never be followed by a second
    // one that selects a continuative (花を摘み+ぬ, 花を摘み+けり).
    if (best.verb_type == grammar::VerbType::GodanMa && hiragana_part == "み" && kanji_end - start_pos <= 3) {
      std::string base_form = extractSubstring(codepoints, start_pos, kanji_end) + "む";
      if (!vh::isVerbInDictionary(dict_manager, base_form) &&
          !vh::classicalAuxiliaryFollowsAt(dict_manager, codepoints, end_pos)) {
        return;
      }
    }

    // Skip verb + ます auxiliary patterns
    if (vh::shouldSkipMasuAuxPattern(surface, best.verb_type)) {
      return;  // Skip - let the split (verb + dictionary aux) win
    }

    // Skip verb + そう auxiliary patterns
    if (vh::shouldSkipSouPattern(surface, best.verb_type)) {
      return;  // Skip - let the split (verb renyokei + そう) win
    }

    // Skip verb + passive auxiliary patterns (れる, れた, etc.)
    // For auxiliary separation: 書かれる → 書か + れる
    if (vh::shouldSkipPassiveAuxPattern(surface, best.verb_type)) {
      return;  // Skip - let the split (verb mizenkei + passive aux) win
    }

    // Skip verb + causative auxiliary patterns (せる, させる, etc.)
    // For auxiliary separation: 書かせる → 書か + せる
    if (vh::shouldSkipCausativeAuxPattern(surface, best.verb_type)) {
      return;  // Skip - let the split (verb mizenkei + causative aux) win
    }

    // Skip suru-verb auxiliary patterns (して, した, している, etc.)
    // Preserve the noun and suru-verb boundary: 勉強して → 勉強 + して.
    size_t kanji_count = kanji_end - start_pos;
    if (vh::shouldSkipSuruVerbAuxPattern(surface, kanji_count, inflection)) {
      return;  // Skip - let the split (noun + suru-aux) win
    }

    // Skip te-form + subsidiary/aspect verb patterns (てもらう, てくれ, てあげ,
    // ていく, ている, てお, ...): these split as verb te-form + auxiliary
    // (助けてもらう → 助け+て+もらう, 食べていく → 食べ+て+いく).
    // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
    if (vh::guardIsWired(vh::GuardMember::EmbedTeAuxiliary, vh::GuardOrigin::KanjiFinalization) &&
        vh::embedsTeFormAuxiliary(surface)) {
      return;  // Skip - let the split (verb te-form + subsidiary verb) win
    }

    // Coordinate particles たり/だり close a past predicate and must not
    // be absorbed by a fabricated unknown verb (紙だったりする,
    // 高かったりする). Their preceding predicate is handled by the
    // dedicated copula/adjective/verb candidate paths.
    if (utf8::containsAny(surface, {"たり", "だり"})) {
      return;
    }

    // Skip volitional patterns ending with よう (e.g., 食べよう)
    // Preserve the volitional stem and auxiliary boundary: 食べよう → 食べよ + う.
    if (surface.size() >= 6 && surface.compare(surface.size() - 6, 6, "よう") == 0) {
      return;  // Skip - let the split (verb + volitional aux) win
    }

    // Skip godan volitional patterns ending with おう (e.g., 行こう, 書こう)
    // Preserve the volitional stem and auxiliary boundary: 行こう → 行こ + う.
    // O-row + う patterns: こう, ごう, そう, とう, のう, ぼう, もう, ろう, おう.
    // This is a DELIBERATE Godan-mizenkei subset, not the full o-row: よう is
    // the ichidan volitional (handled just above), and を/ど/ほ/ぞ never form a
    // Godan mizenkei. Do NOT widen to kana::isORowCodepoint — it would over-match
    // をう/どう/ほう and wrongly skip valid verb candidates.
    if (surface.size() >= 6) {
      std::string last_two = surface.substr(surface.size() - 6);  // 2 hiragana = 6 bytes
      if (last_two == "こう" || last_two == "ごう" || last_two == "そう" || last_two == "とう" || last_two == "のう" ||
          last_two == "ぼう" || last_two == "もう" || last_two == "ろう" || last_two == "おう") {
        return;  // Skip - let the split (verb mizenkei + う) win
      }
    }

    // Skip onbin + auxiliary verb patterns (買っとく, 読んどく, 行っちゃう).
    // Preserve the onbin stem and contracted auxiliary boundary: 買っとく → 買っ + とく,
    // 読んどく → 読ん + どく. Check whether the suffix after a sokuon or
    // hatsuonbin is a registered aspect auxiliary.
    bool skip_sokuonbin_aux = false;
    if (dict_manager && surface.size() >= 9) {  // っ(3) + 2char auxiliary minimum
      // Find an onbin position and check the following auxiliary in the dictionary.
      auto surface_cps = normalize::utf8::decode(surface);
      for (size_t i = 1; i < surface_cps.size() && !skip_sokuonbin_aux; ++i) {
        if ((surface_cps[i] == U'っ' || surface_cps[i] == U'ん') && i + 1 < surface_cps.size()) {
          // Get the suffix after the onbin.
          std::vector<char32_t> suffix_cps(surface_cps.begin() + i + 1, surface_cps.end());
          std::string suffix = normalize::utf8::encode(suffix_cps);
          // A nasal contraction licenses only the preparatory とく/どく.
          // Completion forms retain their verb-class-dependent analyses
          // after ん, while both aspect auxiliaries are valid after っ.
          auto results = dict_manager->lookup(suffix, 0);
          for (const auto& r : results) {
            const bool is_preparatory = r.entry && r.entry->extended_pos == core::ExtendedPOS::AuxAspectOku;
            const bool is_sokuon_completion =
                r.entry && surface_cps[i] == U'っ' && r.entry->extended_pos == core::ExtendedPOS::AuxAspectShimau;
            if (r.entry && r.entry->surface == suffix && (is_preparatory || is_sokuon_completion)) {
              SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << surface << "\" sokuonbin+aux (" << suffix << ")\n");
              skip_sokuonbin_aux = true;
              break;
            }
          }
        }
      }
    }
    if (skip_sokuonbin_aux) {
      return;  // Skip - let the split (verb sokuonbin + auxiliary) win
    }

    // Lower cost for higher confidence matches
    float base_cost =
        candidate::confidenceScaledCost(verb_opts.base_cost_standard, best.confidence, verb_opts.confidence_cost_scale);
    // Suru verbs are compositional noun + する units; penalize the unified candidate.
    // e.g., 勉強する → 勉強 + する (split preferred)
    if (best.verb_type == grammar::VerbType::Suru && best.stem.size() >= core::kTwoJapaneseCharBytes) {
      // Penalize unified suru-verb to prefer noun + する/される/させる split
      base_cost += candidate::kSuruVerbSplitPenalty;
      SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface << "\" +3.0 (suru_split_penalty)\n");
    }
    // Penalize ALL verb candidates with prefix-like kanji at start
    // e.g., 今何する/今何してる should split, not be single verb
    // This applies to all verb types (suru, ichidan, godan)
    if (best.stem.size() >= core::kTwoJapaneseCharBytes) {
      auto stem_codepoints = normalize::utf8::decode(best.stem);
      if (!stem_codepoints.empty() && isPrefixLikeKanji(stem_codepoints[0])) {
        // Heavy penalty to force split
        base_cost += candidate::kStandaloneKanjiVerbSplitPenalty;
        SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface << "\" +3.0 (prefix_kanji_penalty)\n");
      }
    }
    // Penalize verb candidates starting with interrogative kanji (何, 誰, 幾)
    // e.g., 何してる should split as 何|し|てる, not be single verb
    // Interrogatives are standalone words, not verb stems
    {
      auto stem_codepoints = normalize::utf8::decode(best.stem);
      if (!stem_codepoints.empty() && isInterrogativeKanji(stem_codepoints[0])) {
        // Heavy penalty to force split
        base_cost += candidate::kStandaloneKanjiVerbSplitPenalty;
        SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface << "\" +3.0 (interrogative_kanji_penalty)\n");
      }
    }
    // Skip patterns where removing first kanji leaves a valid dictionary verb
    // e.g., 本買った → 本 + 買った, where 買う is a dict verb
    // This handles particleless noun+verb patterns: 本買った, 服買った, 車買った
    if (dict_manager != nullptr && kanji_count == 2 && !follows_reduplicated_noun) {
      auto stem_cps = normalize::utf8::decode(best.stem);
      if (stem_cps.size() == 2) {
        // Get second kanji as potential verb stem
        std::string remainder_stem = normalize::encodeUtf8(stem_cps[1]);
        // Check if remainder + verb ending is a dictionary verb
        std::string conj_suffix = vh::baseFormSuffix(best.verb_type);
        if (!conj_suffix.empty()) {
          std::string remainder_base = remainder_stem + conj_suffix;
          if (vh::isVerbInDictionary(dict_manager, remainder_base)) {
            // Skip this candidate - prefer noun + verb split
            SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << surface << "\" remainder \"" << remainder_base
                                                      << "\" is dict verb\n");
            return;
          }
        }
      }
    }
    // Penalize single-kanji + いる verb candidates (both godan-ra and ichidan)
    // e.g., 人いる should split as 人 + いる (noun + verb), not be verb
    // Most single kanji + いる patterns are NOUN + existence verb いる
    // Valid single-kanji verbs: 入る, 走る, 居る (いる), 見る, etc.
    // These should be in dictionary, so dictionary bonus will override
    {
      auto surface_cps = normalize::utf8::decode(surface);
      // Check if pattern is: 1 kanji + いる
      if (surface_cps.size() == 3 && normalize::isKanjiCodepoint(surface_cps[0]) && surface_cps[1] == U'い' &&
          surface_cps[2] == U'る') {
        // Single kanji + いる pattern - penalize to prefer NOUN + いる split
        base_cost += candidate::kSingleKanjiIruVerbSplitPenalty;
        SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface << "\" +2.5 (single_kanji_iru_penalty)\n");
      }
    }
    // Check if base form exists in dictionary - significant bonus for known verbs
    // This helps 行われた (base=行う) beat 行(suffix)+われた split
    // Skip compound adjective patterns (verb renyoukei + にくい/やすい/がたい)
    // Skip suru-verbs because noun and する are separate search units.
    bool is_comp_adj = vh::isCompoundAdjectivePattern(surface);
    bool in_dict = vh::isVerbInDictionary(dict_manager, best.base_form);
    bool is_suru = (best.verb_type == grammar::VerbType::Suru);
    // Reject a fabricated conjugation that merely absorbs a trailing
    // focus particle (+ optional negative): 水しかない is noun + 副助詞
    // しか + ない, never a form of the non-word godan-ka verb 水しく, and
    // お金さえない is noun + 係助詞 さえ + ない, never a form of the non-word
    // godan-wa verb 金さう. Real verbs whose surface embeds a particle
    // string (押さえ from 押さえる, 起こそ from 起こす) are protected by
    // their dictionary base form (in_dict).
    // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
    if (!in_dict && vh::endsWithFocusParticleTail(dict_manager, codepoints, start_pos, end_pos)) {
      SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << surface << "\" fabricated verb absorbing focus particle\n");
      return;
    }
    // A closed-class auxiliary may inflect with the same kana as an open
    // class verb. Do not let an unverified whole-span hypothesis swallow
    // its negative form: 過ぎなかった → 過ぎ + なかっ + た.
    if (!in_dict && vh::hasAuxiliaryNegativeBoundary(dict_manager, codepoints, start_pos, end_pos)) {
      SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << surface << "\" fabricated verb absorbing auxiliary negative\n");
      return;
    }
    // A monograde or カ変 candidate whose ending is a classical auxiliary has
    // absorbed that auxiliary: both paradigms inflect on a bare stem, so the
    // kana past it must be a conjugation ending, and no cell of either is
    // spelled like one of the closed class (来ぬ is 来 + ぬ, never a form of
    // 来る). The dictionary base form is no defence here — 来る is registered
    // and still cannot spell that cell — so this runs before the in_dict gate.
    // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
    if ((best.verb_type == grammar::VerbType::Ichidan || best.verb_type == grammar::VerbType::Kuru) &&
        vh::spellsClassicalAuxiliaryEnding(dict_manager, surface, best.stem)) {
      SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << surface << "\" fabricated cell spelling a classical auxiliary\n");
      return;
    }
    // An inflected past predicate stays decomposed before a formal noun.
    // The lexical stem and the past auxiliary are independently available
    // (読ん+だ+ついで, 悟っ+た+時); retain that grammatical boundary unless
    // the whole surface is itself a dictionary verb.
    const bool is_dictionary_surface = vh::hasDictionaryEntry(dict_manager, surface, core::PartOfSpeech::Verb);
    const dictionary::DictionaryEntry* past_entry = (dict_manager != nullptr && !best.morphemes.empty())
                                                        ? dict_manager->lookupExact(best.morphemes.back())
                                                        : nullptr;
    const bool ends_with_past_aux = past_entry != nullptr && past_entry->pos == core::PartOfSpeech::Auxiliary &&
                                    past_entry->extended_pos == core::ExtendedPOS::AuxTenseTa;
    if (!is_dictionary_surface && ends_with_past_aux && vh::formalNounFollowsAt(dict_manager, codepoints, end_pos)) {
      SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << surface << "\" past auxiliary before formal noun\n");
      return;
    }
    // Reject a fabricated conjugation that spans a te-form + the subsidiary
    // verb みる: an internal て/で followed by み is always [verb te-form] +
    // みる (食べてみれば = 食べ + て + みれ + ば), never one conjugated verb.
    // @see fabricated closed-class absorption guards (verb_candidates_helpers.h)
    if (!in_dict && vh::guardIsWired(vh::GuardMember::EmbedTeMiruAuxiliary, vh::GuardOrigin::KanjiFinalization) &&
        vh::embedsTeFormMiruAuxiliary(codepoints, start_pos, end_pos)) {
      SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << surface << "\" fabricated verb spanning te-form + みる\n");
      return;
    }
    if (!is_comp_adj && in_dict && !is_suru) {
      // Found in dictionary - give strong bonus (not for suru-verbs)
      base_cost = candidate::confidenceScaledCost(verb_opts.base_cost_verified, best.confidence,
                                                  verb_opts.confidence_cost_scale_medium);
      // Godan-ra renyokei ambiguity: 降り can be from 降る(godan-ra) or
      // 降りる(ichidan). When ichidan form exists in dict, penalize godan-ra
      // so the more specific ichidan interpretation wins.
      if (best.verb_type == grammar::VerbType::GodanRa && utf8::endsWith(surface, "り")) {
        std::string ichidan_base = surface + "る";
        if (vh::isVerbInDictionary(dict_manager, ichidan_base)) {
          base_cost += candidate::kGodanRaIchidanAmbiguityPenalty;
          SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface << "\" +1.0 (godan_ra_ichidan_ambiguity, " << ichidan_base
                                                   << " in dict)\n");
        }
      }
    }
    // Penalty for compound adjective patterns (verb renyokei + やすい/にくい/がたい)
    // MeCab splits these: 使いにくい → 使い + にくい
    if (is_comp_adj) {
      base_cost += candidate::kAdjSplitForcePenalty;
      SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface << "\" +2.0 (compound_adj_penalty)\n");
    }
    // A noun stem followed by terminal る is a productive denominal verb
    // (事故る). Its lexical noun evidence licenses the derivation itself, so
    // it must not be priced as the same unsupported kanji run as a fabricated
    // multi-kanji verb.
    const bool productive_denominal_ru = !in_dict && best.base_form == surface && utf8::endsWith(surface, "る") &&
                                         !best.stem.empty() && vh::isNounInDictionary(dict_manager, best.stem);

    // A case particle inside the span is a phrase boundary the verb reading has
    // to argue against, and an unattested base form is no argument at all
    // (差|が|ずれる, not 差がず|れる). Two conditions keep the mora from being
    // read as a particle where it is only okurigana: it must be strictly
    // interior, which is why 泳が+ず is untouched, and what precedes it must end
    // at the kanji run, so the が of 昔ながら and the と of 呼びとめる stay
    // word-internal rather than inventing a phrase boundary mid-stem.
    bool spans_interior_case_particle = false;
    if (!in_dict && dict_manager != nullptr && end_pos > start_pos + 2) {
      for (size_t particle_pos = start_pos + 1; particle_pos + 1 < end_pos; ++particle_pos) {
        if (!normalize::isKanjiCodepoint(codepoints[particle_pos - 1])) {
          continue;
        }
        const auto* particle =
            lookupEntryInRange(*dict_manager, codepoints, particle_pos, particle_pos + 1, core::PartOfSpeech::Particle);
        if (particle != nullptr && particle->extended_pos == core::ExtendedPOS::ParticleCase) {
          spans_interior_case_particle = true;
          break;
        }
      }
    }
    if (spans_interior_case_particle) {
      SUZUME_DEBUG_LOG_VERBOSE("[VERB_SKIP] \"" << surface << "\" interior_case_particle\n");
      return;
    }
    // Penalize 2+-kanji verb candidates whose base form is not in dict
    // Most real 2-kanji verbs (行う, 伴う, etc.) are in the dictionary.
    // False 2-kanji patterns like 柿食えば (柿 + 食えば) have base 柿食う
    // which is not a real verb. Apply penalty so noun + verb split wins.
    // Extended from ==2 to >=2: a 3+-leading-kanji "verb" whose base is not
    // in any dictionary is likewise noun+verb over-merge or a suru-compound
    // (全部食べちゃった misparsed with 全部食 as a fake verb stem); real 2-kanji
    // verbs are dict entries, so they are unaffected by widening the range.
    if (kanji_count >= 2 && !in_dict && !productive_denominal_ru && !is_multi_kanji_godan_wa_renyokei &&
        !follows_reduplicated_noun) {
      base_cost += bigram_cost::kRare;
      SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface << "\" +1.0 (two_kanji_non_dict_penalty)\n");
    }
    // A terminal hypothesis must not absorb a classical auxiliary that a
    // dictionary-attested irrealis already licenses: 読ま+む is the irrealis of
    // 読む plus the conjectural, not a verb 読まむ.  Both halves come from the
    // dictionary, so the fabricated one-word reading has no evidence of its own.
    // Lexical verbs ending in the same kana (悩む, 死ぬ) keep no irrealis entry
    // for their own prefix and are unaffected.
    if (!in_dict && dict_manager != nullptr && best.base_form == surface &&
        surface.size() > core::kTwoJapaneseCharBytes) {
      const auto* tail_entry =
          dict_manager->lookupExact(std::string(utf8::lastChar(surface)), core::PartOfSpeech::Auxiliary);
      const auto* head_entry =
          dict_manager->lookupExact(std::string(utf8::dropLastChar(surface)), core::PartOfSpeech::Verb);
      const bool classical_irrealis_tail =
          tail_entry != nullptr && (tail_entry->extended_pos == core::ExtendedPOS::AuxVolitional ||
                                    tail_entry->extended_pos == core::ExtendedPOS::AuxNegativeNu);
      if (classical_irrealis_tail && head_entry != nullptr &&
          head_entry->extended_pos == core::ExtendedPOS::VerbMizenkei) {
        base_cost += bigram_cost::kRare;
        SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface << "\" +" << bigram_cost::kRare
                                                 << " (classical_irrealis_auxiliary_absorbed)\n");
      }
    }
    // A multi-kanji stem followed by the classical サ変 terminal す is
    // compositional for search: 前進+す, 説明+す. The inflection analyzer can
    // also hypothesize an unregistered GodanSa word spanning the boundary;
    // apply a substantial class-level penalty so the independently generated noun
    // and closed-class す entry win. Registered lexical GodanSa verbs and
    // single-kanji stems such as 愛す are unaffected.
    if (!in_dict && kanji_count >= 2 && best.verb_type == grammar::VerbType::GodanSa && best.base_form == surface) {
      base_cost += bigram_cost::kVeryRare;
      SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface << "\" +" << bigram_cost::kVeryRare
                                               << " (multi_kanji_classical_suru_penalty)\n");
    }
    // Penalize ichidan verb candidates with pure single-kanji stem (no hiragana)
    // when base form is not in dict.
    // Real ichidan verbs with single-kanji stems (見る, 着る, 居る, etc.) are in
    // the dictionary, while real multi-char stems like 食べ (食べる) need no penalty.
    // False patterns like 心る (from "心なく" misparsed as ichidan) have a pure
    // single-kanji stem and should be penalized to favor noun + aux split.
    if (!in_dict && best.verb_type == grammar::VerbType::Ichidan && !best.stem.empty() &&
        best.stem.size() == core::kJapaneseCharBytes) {
      base_cost += bigram_cost::kRare;
      SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface << "\" +1.0 (single_kanji_stem_ichidan_non_dict_penalty)\n");
    }
    // Penalize unverified godan candidates that look like NOUN+AUX/VERB misanalysis.
    // Skip ichidan (handled above) and Suru (handled earlier).
    // Two patterns are penalized:
    //   (a) godan-ka with stem ending in な (心なく → 心+なく): なく is AUX_過去 of ない.
    //   (b) hiragana-only portion of base form is a 2+ char dict AUX/VERB
    //       (e.g., 我ある — ある is dict VERB).
    if (!in_dict && kanji_count == 1 && dict_manager != nullptr && best.verb_type != grammar::VerbType::Ichidan &&
        best.verb_type != grammar::VerbType::Suru) {
      bool penalized = false;
      // Pattern (a): godan-ka with stem ending in な
      if (best.verb_type == grammar::VerbType::GodanKa && !best.stem.empty() && utf8::endsWith(best.stem, "な")) {
        base_cost += bigram_cost::kRare;
        SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface << "\" +1.0 (godan_ka_kanji_na_suffix_non_dict_penalty)\n");
        penalized = true;
      }
      // Pattern (b): hiragana-only portion of base form is a 2+ char dict AUX/VERB.
      // Restricted to 2+ chars to avoid false matches with single-char endings
      // (う = AuxVolitional, but all real godan-wa verbs end in う: 思う, 戦う etc.)
      const bool has_attributive_content_follower =
          end_pos < codepoints.size() && normalize::isKanjiCodepoint(codepoints[end_pos]);
      if (!penalized && !best.base_form.empty() && !has_attributive_content_follower) {
        auto base_cps = normalize::utf8::decode(best.base_form);
        if (base_cps.size() >= 3 && normalize::isKanjiCodepoint(base_cps[0])) {
          std::vector<char32_t> hira_only(base_cps.begin() + 1, base_cps.end());
          std::string hira_portion = normalize::utf8::encode(hira_only);
          // A dictionary verb in the tail settles the split on its own: the
          // compound reading would need a nominal as its first element, and no
          // compound verb is built that way (我+ある).
          //
          // An auxiliary does not, because a bound kana tail is exactly what
          // okurigana looks like, and a whole class of verbs is spelled that
          // way (惜しむ, 親しむ, 苦しむ all carry the classical causative しむ).
          // Two things put the split back beyond doubt, either one on its own:
          //   - the auxiliary is one a nominal can host, so noun-plus-auxiliary
          //     is a licensed reading of the same characters (本+どす). The
          //     bigram table already holds that judgment; an auxiliary bound to
          //     a conjugated cell, as the causative is, is barred there.
          //   - the candidate opened inside a kanji run, so its head is a
          //     fragment of a compound (確認 sliced into 認+らむ). Okurigana
          //     belongs to a whole word, and a stem never starts mid-compound,
          //     so the tail cannot be okurigana whatever else it is.
          const auto* aux_entry = dict_manager->lookupExact(hira_portion, core::PartOfSpeech::Auxiliary);
          const bool nominal_hosts_auxiliary =
              aux_entry != nullptr &&
              BigramTable::getCost(core::ExtendedPOS::Noun, aux_entry->extended_pos) <= bigram_cost::kNeutral;
          const bool opens_inside_kanji_run = start_pos > 0 && normalize::isKanjiCodepoint(codepoints[start_pos - 1]);
          if ((aux_entry != nullptr && (nominal_hosts_auxiliary || opens_inside_kanji_run)) ||
              verb_helpers::isVerbInDictionary(dict_manager, hira_portion)) {
            base_cost += bigram_cost::kRare;
            SUZUME_DEBUG_LOG_VERBOSE("[COST_ADJ] \"" << surface
                                                     << "\" +1.0 (single_kanji_godan_hira_is_dict_word_penalty)\n");
          }
        }
      }
    }
    // Penalty for verb candidates containing みたい suffix
    // みたい is a na-adjective (like ~, seems ~), not a verb suffix
    // E.g., 猫みたい should be 猫 + みたい, not VERB 猫む
    if (utf8::contains(surface, "みたい")) {
      base_cost += verb_opts.penalty_single_char;
    }
    // Penalty for verb candidates ending with auxiliary たい/たく/たかっ
    // MeCab splits verb + auxiliary たい (desiderative)
    // E.g., 戦いたい = 戦い + たい, not single VERB
    if (utf8::endsWith(surface, "たい") || utf8::endsWith(surface, "たく") || utf8::endsWith(surface, "たかっ")) {
      base_cost += bigram_cost::kRare;  // Penalize to favor split path
    }
    // Penalty for verb candidates containing causative auxiliary chains
    // MeCab splits: 欠かせない → 欠か+せ+ない, 食べさせた → 食べ+させ+た
    if (vh::containsCausativeAuxPattern(surface)) {
      base_cost += bigram_cost::kStrong;  // Penalize to favor split path
    }
    // Penalty for verb candidates ending with auxiliary まい (negative volitional)
    // MeCab splits verb + auxiliary まい
    // E.g., 出来まい = 出来 + まい, 行くまい = 行く + まい
    if (utf8::endsWith(surface, "まい")) {
      base_cost += bigram_cost::kStrong;  // Penalize to favor split path
    }
    // Penalty for verb candidates ending with らしい (conjecture auxiliary)
    // MeCab splits verb/adj + らしい
    // E.g., 帰りたいらしい = 帰り + たい + らしい
    if (utf8::endsWith(surface, "らしい") || utf8::endsWith(surface, "らしく") || utf8::endsWith(surface, "らしかっ")) {
      base_cost += bigram_cost::kStrong;  // Penalize to favor split path
    }
    // Penalty for verb candidates ending with passive+te form (〜まれて/〜られて)
    // MeCab splits compound verb passive+te: 読み込まれて → 読み込ま|れ|て
    // E.g., 読み込まれていない = 読み込ま + れ + て + い + ない
    if (utf8::endsWith(surface, "まれて") || utf8::endsWith(surface, "まれた") || utf8::endsWith(surface, "られて") ||
        utf8::endsWith(surface, "られた")) {
      base_cost += bigram_cost::kVeryRare + bigram_cost::kNegligible;
    }
    // Penalty for verb candidates containing て+auxiliary verb chains
    // MeCab splits: 付いてくる → 付い+て+くる, 集まってくる → 集まっ+て+くる
    // These are syntactic constructions (V-te + auxiliary), not single verb forms
    if (vh::containsTeFormAuxPattern(surface)) {
      base_cost += bigram_cost::kStrong;  // Penalize to favor split path
    }
    // Penalty for verb candidates absorbing post-verbal particles
    // たばかり/だばかり = V-ta + bakari ("just did V"), always separate tokens
    // E.g., 生まれたばかりだ should be 生まれ+た+ばかり+だ, not one token
    if (utf8::contains(surface, "たばかり") || utf8::contains(surface, "だばかり")) {
      base_cost += bigram_cost::kSevere;
    }
    // Set has_suffix to skip exceeds_dict_length penalty in tokenizer.cpp
    // This applies when:
    // 1. Base form exists in dictionary as verb (in_dict)
    // 2. OR: Ichidan verb with valid i-row stem (感じる, not 人いる)
    //    that passes confidence threshold
    // Valid i-row ichidan stems end in i-row hiragana (not e-row te-form/copula)
    // and exclude single-kanji + い patterns (人い → 人 + いる).
    bool is_ichidan = (best.verb_type == grammar::VerbType::Ichidan);
    bool has_valid_ichidan_stem = is_ichidan && vh::isValidIRowIchidanStem(best.stem);
    bool recognized_ichidan =
        is_ichidan && has_valid_ichidan_stem && best.confidence > verb_opts.confidence_ichidan_dict;
    // Godan verbs with single-kanji stem + high confidence are also
    // recognized (残る, 立つ, 打つ, etc.)
    bool recognized_godan = !is_ichidan && !in_dict && !best.stem.empty() &&
                            best.stem.size() == core::kJapaneseCharBytes &&
                            best.confidence >= verb_opts.confidence_ichidan_dict;
    // A sokuonbin compound built over a verified embedded verb (突っ走り) is a
    // genuine verb even though its multi-kanji stem is absent from the
    // dictionary; exempt it from the exceeds_dict_length penalty so its
    // renyokei competes with the noun split before the ます auxiliary.
    bool has_suffix = in_dict || productive_denominal_ru || recognized_ichidan || recognized_godan ||
                      sokuonbin_stem_verified || is_multi_kanji_godan_wa_renyokei;
    // Determine extended_pos based on verb type and surface ending
    // Godan-wa verbs ending in い are renyokei (戦い), not onbinkei
    // Godan-ka/ga verbs ending in い are onbinkei (書い, 泳い)
    core::ExtendedPOS verb_epos = core::ExtendedPOS::Unknown;  // Auto-detect
    if (grammar::isGodanVerbType(best.verb_type) && best.base_form == surface) {
      // A complete Godan dictionary form ends in its u-row base suffix.
      // The surface-only fallback intentionally cannot infer every u-row
      // ending because short stems are ambiguous, but the inflection result
      // already supplies that evidence here (立つ, 書く, 一つ-as-a-competing
      // hypothesis). Marking it as terminal prevents case-particle rules for
      // true renyokei from spuriously boosting the hypothesis.
      verb_epos = core::ExtendedPOS::VerbShuushikei;
    } else if (utf8::endsWith(surface, "い")) {
      // Skip godan readings of known kami-ichidan renyokei stems (率い,
      // 老い, 強い, ...): the godan lemma would be wrong (率く/率う).
      // The ichidan_renyokei path generates the correct 〜いる candidate.
      if (grammar::inflection::isValidKanjiIStemException(surface)) {
        SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << surface << "\" is kami-ichidan renyokei, skipping godan reading\n");
        return;
      }
      if (best.verb_type == grammar::VerbType::GodanWa) {
        verb_epos = core::ExtendedPOS::VerbRenyokei;
      } else if (best.verb_type == grammar::VerbType::GodanKa || best.verb_type == grammar::VerbType::GodanGa) {
        verb_epos = core::ExtendedPOS::VerbOnbinkei;
      }
    }
    // Skip an unverified bare Godan form when the whole surface is an exact
    // dictionary noun/adjective. This covers nominalized renyokei (思い,
    // 戦い) and u-row homographs (向う) without suppressing a verified verb
    // entry that legitimately shares the surface.
    if (!in_dict && (verb_epos == core::ExtendedPOS::VerbRenyokei || verb_epos == core::ExtendedPOS::VerbShuushikei) &&
        vh::isNounOrAdjectiveInDictionary(dict_manager, surface)) {
      SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << surface << "\" is dict NOUN/ADJ, skipping unverified godan form\n");
      return;  // Skip this candidate, use dictionary entry instead
    }
    // Skip fabricated godan-wa renyokei whose trailing い is really the
    // leading い of the receptive auxiliary いただく. Unverified wa-row
    // hypotheses (覧い ← 覧う) would otherwise absorb the auxiliary's
    // onset (ご覧いただき → 覧い+ただき); dictionary-verified wa-row verbs
    // (使い ← 使う) keep their candidate.
    if (verb_epos == core::ExtendedPOS::VerbRenyokei && !in_dict && end_pos > 0 &&
        vh::itadakuParadigmStartsAt(codepoints, end_pos - 1)) {
      SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << surface << "\" trailing い starts いただく paradigm\n");
      return;  // Skip - keep the い with いただく
    }
    // Skip ichidan ta-form if stem is registered as NOUN in dictionary
    // e.g., 感じた → stem 感じ is dict NOUN, so skip (prefer 感じ(NOUN) + た(AUX))
    // This prevents nominalized verb renyokei forms from appearing as conjugated verbs
    // The stem for ichidan ta-form is the renyokei (e.g., 感じ for 感じた)
    if (best.verb_type == grammar::VerbType::Ichidan && !productive_denominal_ru && !best.stem.empty() &&
        vh::isNounInDictionary(dict_manager, best.stem)) {
      SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << surface << "\" stem \"" << best.stem
                                        << "\" is dict NOUN, skipping ichidan ta-form\n");
      return;  // Skip this candidate, prefer NOUN + た split
    }
    // Skip if surface is already a registered VERB in dictionary
    // The dict entry has correct lemma; this unknown candidate would have wrong lemma
    // E.g., 下さい is dict verb (lemma=下さる), skip godan-wa candidate (lemma=下さう)
    if (!in_dict && vh::isVerbInDictionary(dict_manager, surface)) {
      SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << surface << "\" is dict VERB, skipping unknown candidate\n");
      return;
    }
    // ん directly after an irrealis is the contracted negative ぬ, an auxiliary
    // of its own whose boundary the analysis keeps (知ら+ん, 分から+ん, 待た+ん).
    // A nasal euphony replaces the continuative's own final mora, so its stem
    // never ends in the a-row; where the spelling looks that way anyway
    // (汗ばん+だ) the て/で/た/だ behind it already proves the cell.
    if (utf8::endsWith(surface, "ん") && end_pos >= start_pos + 3 &&
        grammar::isARowCodepoint(codepoints[end_pos - 2]) &&
        (end_pos >= codepoints.size() ||
         (codepoints[end_pos] != U'で' && codepoints[end_pos] != U'だ' && codepoints[end_pos] != U'て'))) {
      SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << surface << "\" is an irrealis before the contracted negative\n");
      return;
    }
    // Skip fake verb candidates homographic with the i-adjective 未然形.
    // Xかろ(+う) can be a verb volitional stem (分かる → 分かろ+う) or the
    // i-adjective 未然形 (高い → 高かろ+う); inflection alone yields a
    // plausible fake base (ichidan 高かる). The lexical signal decides:
    // when the base form is not a known verb and stem + い is a known
    // dictionary adjective, prefer the ADJ 未然形 candidate.
    if (!in_dict && dict_manager != nullptr && utf8::endsWith(surface, "かろ")) {
      std::string iadj_base = surface.substr(0, surface.size() - 2 * core::kJapaneseCharBytes) + "い";
      if (vh::isAdjectiveInDictionary(dict_manager, iadj_base)) {
        SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << surface << "\" ends かろ and " << iadj_base
                                          << " is i-adjective (prefer ADJ 未然形)\n");
        return;
      }
    }
    // Skip fake verb candidates homographic with the classical i-adjective
    // 連体形 (文語). Xき is usually a godan-ka 連用形 (書き ← 書く), but when
    // the hypothesized base verb is not in the dictionary and stem + い is a
    // known dictionary adjective (美しき → 美しい), the surface is the
    // classical attributive form — prefer the ADJ 連体形 candidate.
    if (!in_dict && dict_manager != nullptr && utf8::endsWith(surface, "き")) {
      std::string iadj_base = surface.substr(0, surface.size() - core::kJapaneseCharBytes) + "い";
      if (vh::isAdjectiveInDictionary(dict_manager, iadj_base)) {
        SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << surface << "\" ends き and " << iadj_base
                                          << " is i-adjective (prefer ADJ 連体形)\n");
        return;
      }
    }
    // A one-kanji Ichidan stem spells its continuative bare, and the classical
    // perfect selects exactly that (見+つ). A godan-ta verb of the same two
    // characters is then a fabrication: the paradigm that already owns the
    // kanji accounts for both morae. Kanji outside that closed stem set keep
    // their godan-ta reading (発つ, 断つ, 絶つ), and a dictionary verb never
    // reaches this point: the rule above already drops its unknown twin (待つ).
    if (!in_dict && normalize::utf8Length(surface) == 2 && utf8::endsWith(surface, "つ") &&
        vh::isSingleKanjiIchidan(utf8::decodeFirstChar(surface))) {
      SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << surface << "\" is a one-kanji Ichidan stem before the classical perfect\n");
      return;
    }
    // Penalize verb candidates absorbing adj く-form + なる suffix chain
    // e.g., 得なくなった should split as 得+なく+なっ+た, not merge as 得る(ichidan)
    // The suffix contains くなっ/くなり/くなる/くなれ = adj renyokei + なる conjugation
    if (!best.suffix.empty() && vh::containsKuNaruPattern(best.suffix)) {
      base_cost += bigram_cost::kSevere;  // Force split
      SUZUME_DEBUG_LOG("[COST_ADJ] \"" << surface << "\" +" << bigram_cost::kSevere << " (ku_naru_verb_suffix)\n");
    }
    // A negative inflection is morphologically decomposed as 未然形 plus
    // the negative auxiliary (読ま+ない, 読ま+なかっ+た).  The whole-span
    // verb candidate is retained for lattice coverage, but must not beat
    // that productive auxiliary boundary after a case-marked object.
    if (utf8::contains(best.suffix, "ない") || utf8::contains(best.suffix, "なか") ||
        utf8::contains(best.suffix, "なけ")) {
      base_cost += bigram_cost::kStrong;
      SUZUME_DEBUG_LOG("[COST_ADJ] \"" << surface << "\" +" << bigram_cost::kStrong << " (negative_suffix)\n");
    }
    // Penalize verb candidates absorbing the negative adverbial なく (ない's 連用形).
    // MeCab splits mizenkei + なく: 行かなくて → 行か + なく + て, not 行かなく(verb).
    // The mizenkei-split candidate (is_naku_pattern above) supplies the split path;
    // this penalty stops the inflection analyzer's whole-span reading from winning.
    if (best.suffix.find("なく") != std::string::npos) {
      base_cost += bigram_cost::kSevere;  // Force split
      SUZUME_DEBUG_LOG("[COST_ADJ] \"" << surface << "\" +" << bigram_cost::kSevere << " (negative_naku_suffix)\n");
    }
    // Penalize unverified godan-wa candidates that extend beyond a
    // shorter dict verb at the same position. These false positives
    // absorb い from the next word (いただく, いく, etc.)
    // e.g., 待ちい (base=待ちう) extends beyond 待ち (dict verb 待つ)
    if (best.verb_type == grammar::VerbType::GodanWa && !in_dict && dict_manager != nullptr) {
      auto prefix_results = dict_manager->lookup(surface, 0);
      for (const auto& result : prefix_results) {
        if (result.entry != nullptr && result.entry->pos == core::PartOfSpeech::Verb &&
            result.length < normalize::utf8Length(surface)) {
          base_cost += candidate::kUnverifiedGodanWaExceedsVerbPenalty;
          SUZUME_DEBUG_LOG("[COST_ADJ] \"" << surface << "\" +2.0 (godan_wa_exceeds_dict_verb)\n");
          break;
        }
      }
    }
    // Japanese builds longer verbs on the continuative stem (読み+始める), never
    // on a finite form, so a fabricated candidate whose prefix is already a
    // dictionary 終止形 has no morphological reading: 書くき can only be 書く
    // plus a following morpheme, not a form of the non-word 書くく. Restricting
    // this to the terminal form leaves stem-prefix candidates (待ち → 待つ)
    // to the narrower rule above, and dictionary verbs are exempt throughout.
    if (!in_dict && dict_manager != nullptr) {
      const auto prefix_results = dict_manager->lookup(surface, 0);
      for (const auto& result : prefix_results) {
        if (result.entry != nullptr && result.entry->extended_pos == core::ExtendedPOS::VerbShuushikei &&
            result.length < normalize::utf8Length(surface)) {
          base_cost += candidate::kUnverifiedVerbExceedsTerminalPenalty;
          SUZUME_DEBUG_LOG("[COST_ADJ] \"" << surface << "\" +" << candidate::kUnverifiedVerbExceedsTerminalPenalty
                                           << " (fabricated_verb_extends_terminal)\n");
          break;
        }
      }
    }
    SUZUME_DEBUG_VERBOSE_BLOCK {
      SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface << " base=" << best.base_form << " cost=" << base_cost
                          << " in_dict=" << in_dict << " has_suffix=" << has_suffix << "\n";
    }
    // Don't set lemma here - let lemmatizer derive it with dictionary verification
    // The lemmatizer will use stem-matching logic to pick the correct base form.
    // Exception: sokuonbin compounds carry the lemma built from the embedded verb
    // base, which the surface-based lemmatizer cannot recover past the onbin.
    const char* forced_lemma =
        sokuonbin_stem_verified
            ? sokuonbin_lemma.c_str()
            : (is_multi_kanji_godan_wa_renyokei || has_mixed_godan_ka_stem ? best.base_form.c_str() : "");
    auto verb_candidate = makeVerbCandidate(
        surface, start_pos, end_pos, base_cost, forced_lemma, grammar::verbTypeToConjType(best.verb_type), has_suffix,
        CandidateOrigin::VerbKanji, best.confidence, grammar::verbTypeToString(best.verb_type).data(), verb_epos);
    verb_candidate.lemma_verified = in_dict || has_mixed_godan_ka_stem;
    candidates.push_back(std::move(verb_candidate));
    // Don't break - try other stem lengths too
  }
}

}  // namespace suzume::analysis::kanji_verb_detail
