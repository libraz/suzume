/**
 * @file verb_candidates_kanji_mizenkei.cpp
 * @brief Kanji verb mizenkei candidate patterns
 */

#include <algorithm>
#include <cmath>

#include "analysis/bigram_table.h"
#include "analysis/candidate_constants.h"
#include "analysis/dictionary_probe.h"
#include "analysis/scorer_constants.h"
#include "analysis/tokenizer_utils.h"
#include "analysis/verb_candidates_absorption_guards.h"
#include "analysis/verb_candidates_auxiliary_patterns.h"
#include "analysis/verb_candidates_dictionary_probes.h"
#include "analysis/verb_candidates_kanji_internal.h"
#include "analysis/verb_candidates_verb_stems.h"
#include "core/debug.h"
#include "core/kana_constants.h"
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

namespace {

// Whether some analysis reads the base form as the given verb type.
bool analyzesAsVerbType(const grammar::Inflection& inflection, const std::string& base_form,
                        grammar::VerbType verb_type) {
  const auto& results = inflection.analyze(base_form);
  return std::any_of(results.begin(), results.end(), [&](const grammar::InflectionCandidate& cand) {
    return cand.verb_type == verb_type && cand.confidence >= 0.4F;
  });
}

// Whether the observed span [start_pos, end_pos) analyzes as the given verb
// type and base form with constructed-verb confidence.
bool observedFormConfirms(const grammar::Inflection& inflection, const std::vector<char32_t>& codepoints,
                          size_t start_pos, size_t end_pos, grammar::VerbType verb_type, const std::string& base_form) {
  return vh::readsAsBaseForm(inflection, extractSubstring(codepoints, start_pos, end_pos), base_form, verb_type,
                             candidate::verb_cost::kConstructedVerbMinConfidence);
}

// Godan mizenkei stem candidates for auxiliary separation: kanji + one a-row
// okurigana mora before a passive, causative, negative or classical auxiliary
// (書か, 読ま, 話さ). Those auxiliaries connect to the stem as their own tokens.
void appendSingleOkuriganaMizenkeiCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                             size_t kanji_end, size_t hiragana_end,
                                             const grammar::Inflection& inflection,
                                             const dictionary::DictionaryManager* dict_manager,
                                             std::vector<UnknownCandidate>& candidates) {
  const size_t mizenkei_end = kanji_end + 1;
  if (mizenkei_end >= hiragana_end || !kana::isARowCodepoint(codepoints[kanji_end])) {
    return;
  }
  const char32_t first_hira = codepoints[kanji_end];
  const char32_t next_char = codepoints[mizenkei_end];
  // 1. Classical べき patterns: 書かれべき, 読まれべき
  // 2. Passive patterns: 書か+れる, 言わ+れ+た. Strict ま-branch: bare ま
  //    requires a following す/せ (れます/れません).
  const bool is_beki_pattern = next_char == U'れ' && mizenkei_end + 2 < codepoints.size() &&
                               codepoints[mizenkei_end + 1] == U'べ' && codepoints[mizenkei_end + 2] == U'き';
  const bool is_passive_pattern = next_char == U'れ' && !is_beki_pattern &&
                                  vh::isPassiveAuxContinuation(codepoints, mizenkei_end + 1, /*strict_masu=*/true);
  // Classical negation ぬ: 揃わぬ → 揃わ (mizenkei) + ぬ (AUX)
  const bool is_nu_pattern = next_char == U'ぬ';
  // The literary conjectural む selects the irrealis just as ぬ does
  // (成ら+む, 咲か+む). Its modern siblings う / よう take the o-row
  // irrealis instead, so at this a-row position an AuxVolitional entry
  // can only be the literary one — the dictionary category decides,
  // not the spelling. Without the boundary the run reads as one
  // fabricated Godan-ma verb whose lemma is its own surface (成らむ).
  // A complete auxiliary can also start at the a-row mora itself
  // (確認+らむ), and there the mora is that auxiliary's onset rather than
  // the verb's okurigana. Only in that position does the kanji run's
  // length carry information, because the same two morae equally spell an
  // irrealis plus the conjectural behind a one-kanji stem (成+ら+む); the
  // run has to be complete, not merely one character measured from an
  // interior position, or 確認's second kanji passes for 成. Where no
  // auxiliary starts at the mora the reading is unambiguous, so a stem
  // that begins inside a kanji run is admissible (心|迷わ+む).
  const bool stem_is_lone_kanji =
      kanji_end - start_pos == 1 && (start_pos == 0 || !normalize::isKanjiCodepoint(codepoints[start_pos - 1]));
  const auto is_volitional = [](const dictionary::DictionaryEntry& entry) {
    return entry.extended_pos == core::ExtendedPOS::AuxVolitional;
  };
  const auto is_conjectural = [](const dictionary::DictionaryEntry& entry) {
    return entry.extended_pos == core::ExtendedPOS::AuxVolitional ||
           entry.extended_pos == core::ExtendedPOS::AuxClassicalConjectureTerminal;
  };
  const bool okurigana_opens_auxiliary = vh::auxiliaryFollowsAt(dict_manager, codepoints, kanji_end, is_conjectural);
  const bool is_classical_conjecture_pattern =
      (stem_is_lone_kanji || !okurigana_opens_auxiliary) && kanji_end - start_pos == 1 &&
      vh::auxiliaryFollowsAt(dict_manager, codepoints, mizenkei_end, is_volitional);
  // Colloquial contracted negative ん (行か+ん, 言わ+ん). A single kanji + さ
  // is the honorific さん (姉+さん). The contracted negative closes the
  // predicate, so the past auxiliary cannot follow it: 〜んだ attaches to an
  // attributive, not to an irrealis. In that environment the ん belongs to the
  // verb as its ma/ba/na-row 音便 (黄ばん+だ, not 黄ば+ん+だ). A registered onbin
  // cell spanning the ん is that verb's own euphony (汗ばん+で of 汗ばむ), not a
  // negative on an unattested 汗ぶ.
  bool is_n_pattern = false;
  if (next_char == U'ん') {
    const bool is_honorific_san = kanji_end == start_pos + 1 && first_hira == U'さ';
    const bool past_auxiliary_follows = mizenkei_end + 1 < codepoints.size() && codepoints[mizenkei_end + 1] == U'だ';
    const auto* spanning_verb =
        dict_manager == nullptr
            ? nullptr
            : lookupEntryInRange(*dict_manager, codepoints, start_pos, mizenkei_end + 1, core::PartOfSpeech::Verb);
    const bool registered_onbin_spans_n =
        spanning_verb != nullptr && spanning_verb->extended_pos == core::ExtendedPOS::VerbOnbinkei;
    is_n_pattern = !is_honorific_san && !past_auxiliary_follows && !registered_onbin_spans_n;
  }
  // Standard negative ない (行か+ない), the past-negative stem なかっ
  // (書か+なかっ+た), and the negative adverbial なく (行か+なく+て), so that
  // なく is not absorbed into a spurious verb form.
  const bool is_nai_pattern =
      next_char == U'な' && mizenkei_end + 1 < codepoints.size() && codepoints[mizenkei_end + 1] == U'い';
  const bool is_nakatt_pattern = next_char == U'な' && mizenkei_end + 3 < codepoints.size() &&
                                 codepoints[mizenkei_end + 1] == U'か' && codepoints[mizenkei_end + 2] == U'っ';
  const bool is_naku_pattern =
      next_char == U'な' && mizenkei_end + 1 < codepoints.size() && codepoints[mizenkei_end + 1] == U'く';
  // The causative auxiliary せ (聞か+せ+られ+た, 書か+せる).
  bool is_causative_pattern = false;
  bool is_shortened_causative_passive = false;
  if (next_char == U'せ' && mizenkei_end + 1 < codepoints.size()) {
    // A lexical Ichidan verb can share the surface of a productive
    // causative (知らせる, 合わせる).  Its dictionary entry is evidence
    // that the whole form is one search unit; otherwise the ordinary
    // Godan mizenkei + causative auxiliary boundary is productive.
    const bool has_lexical_causative =
        vh::isVerbInDictionary(dict_manager, extractSubstring(codepoints, start_pos, mizenkei_end) + "せる");
    const char32_t after_se = codepoints[mizenkei_end + 1];
    if (after_se == U'ら') {
      // Causative-passive chains: せられる. A bare せる/せた/せて remains the
      // lexical causative verb (知らせる, 眠らせた), rather than being split again.
      is_causative_pattern = true;
    } else if (after_se == U'れ' && vh::isPassiveAuxContinuation(codepoints, mizenkei_end + 2, /*strict_masu=*/true)) {
      // Shortened causative-passive: 負わ+さ+れる, 読ま+さ+れた. The さ is the
      // causative auxiliary and the following れ starts the passive auxiliary,
      // not an inflection of an independent lexical verb.
      is_causative_pattern = true;
      is_shortened_causative_passive = true;
    } else if (!has_lexical_causative &&
               (after_se == U'る' || after_se == U'た' || after_se == U'て' ||
                (after_se == U'な' && mizenkei_end + 2 < codepoints.size() && codepoints[mizenkei_end + 2] == U'い'))) {
      // Bare causative inflection remains productive unless a lexical verb
      // with the same full dictionary form is attested.
      is_causative_pattern = true;
    }
  }
  if (!is_beki_pattern && !is_nu_pattern && !is_n_pattern && !is_nai_pattern && !is_nakatt_pattern &&
      !is_naku_pattern && !is_passive_pattern && !is_causative_pattern && !is_classical_conjecture_pattern) {
    return;
  }
  const grammar::VerbType verb_type = grammar::verbTypeFromARowCodepoint(first_hira);
  if (verb_type == grammar::VerbType::Unknown) {
    return;
  }
  const std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
  // Skip GodanSa mizenkei for all-kanji stems of 2+ kanji: 装飾さ is 装飾 +
  // される (サ変名詞), not 装飾す mizenkei. A single-kanji GodanSa causative is
  // an ichidan verb + させ (見+させ+られ+た, not 見さ+せ of a non-word 見す);
  // real godan-sa verbs (話す, 出す, 消す) have multi-char stems.
  const size_t kanji_count = kanji_end - start_pos;
  // The kuru kanji is no Godan stem: its irrealis is 来 itself (来+させる,
  // 来+られる), never 来さ or 来ら.
  if (kanji_count == 1 && grammar::isKuruKanjiStem(codepoints[start_pos])) {
    return;
  }
  if (verb_type == grammar::VerbType::GodanSa &&
      ((grammar::isAllKanji(kanji_stem) && kanji_count >= 2) || (is_causative_pattern && kanji_count == 1))) {
    return;
  }
  // Base suffix, e.g. か → く for GodanKa
  const std::string_view base_suffix = grammar::godanBaseSuffixFromARow(first_hira);
  if (base_suffix.empty()) {
    return;
  }
  const std::string base_form = normalize::concat(kanji_stem, base_suffix);

  // Verify the base form is a valid verb
  // First check dictionary, then fall back to inflection analysis
  // IMPORTANT: For passive pattern, require dictionary check only for
  // most verb rows. The inflection analyzer is too permissive and will
  // accept patterns like 泊む (from 泊まれる) which don't exist.
  // EXCEPTIONS that allow inflection fallback:
  // - WA-row (わ行): passive (奪われる) doesn't conflict with potential
  // - RA-row (ら行): Xらる is not a valid modern verb, so Xられる
  //   is always passive of Xる (e.g., 縛られる = passive of 縛る)
  // - SA-row (さ行): Xさ+れる is the productive passive of
  //   an open-class Godan-sa verb. The all-kanji sahen guard
  //   above retains the nominal + される analysis where needed.
  const bool is_base_dict_verb = vh::isVerbInDictionary(dict_manager, base_form);
  bool is_valid_verb = is_base_dict_verb;
  if (!is_valid_verb && (!is_passive_pattern || first_hira == U'わ' || first_hira == U'ら' || first_hira == U'さ')) {
    // WA-row passive uses a higher confidence threshold
    const float threshold = is_passive_pattern ? candidate::verb_cost::kConstructedVerbPassiveMinConfidence
                                               : candidate::verb_cost::kConstructedVerbMinConfidence;
    is_valid_verb = vh::isVerifiedVerbBase(dict_manager, inflection, base_form, threshold, true);
  }
  // A bare open-class Godan-sa base can be too short for the generic analyzer
  // to rank confidently, while its explicit passive chain supplies the missing
  // inflectional evidence. Validate that complete observed form before
  // rejecting the productive mizenkei candidate; this remains type- and
  // lemma-checked rather than accepting an arbitrary kanji+さ.
  if (!is_valid_verb && is_passive_pattern && first_hira == U'さ') {
    is_valid_verb = observedFormConfirms(inflection, codepoints, start_pos, hiragana_end, verb_type, base_form);
  }
  if (!is_valid_verb) {
    return;
  }
  // Irregular 来る: its passive is 来+られる, not 来ら+れる
  if (is_passive_pattern && grammar::isKuruKanjiBaseForm(base_form)) {
    return;
  }
  // The shortened causative-passive is surface-ambiguous with a lexical
  // Godan-sa passive (明かさ+れる), so it needs an attested base.
  if (is_shortened_causative_passive && !is_base_dict_verb) {
    return;
  }
  // Skip godan mizenkei passive when the surface + れる is a known ichidan
  // verb in the dictionary. E.g., 囚われる is ichidan, not passive of 囚う.
  // The dictionary entry provides the correct candidate with proper lemma.
  if (is_passive_pattern &&
      vh::isVerbInDictionary(dict_manager, extractSubstring(codepoints, start_pos, mizenkei_end) + "れる")) {
    return;
  }
  // The classical conjectural attaches to an irrealis, so the
  // a-row mora in front of it belongs to a verb of its own —
  // except where the okurigana spells a cell of the productive
  // ma-row verbalizing suffix, which is the one derivation shaped
  // like an irrealis plus this auxiliary (黄ばむ, 汗ばむ). There
  // the split has to be decided lexically, and an attested base is
  // the evidence that the reading is the irrealis after all
  // (呼ば+む, 学ば+む). Every other okurigana carries no such
  // homography, so the boundary follows from the auxiliary alone
  // and needs no dictionary support (咲か+む, 迷わ+む, 去ら+む).
  if (is_classical_conjecture_pattern && !is_base_dict_verb && mizenkei_end < codepoints.size() &&
      spellsGodanMaSuffixVerbCell(extractSubstring(codepoints, kanji_end, mizenkei_end + 1))) {
    return;
  }

  const std::string surface = extractSubstring(codepoints, start_pos, mizenkei_end);
  // Negative and passive splits need a negative cost to beat the combined
  // verb form (揃わぬ, 行かん, 行かない, 言われる); べき takes a moderate cost.
  const float cost = (is_nu_pattern || is_n_pattern || is_nai_pattern || is_passive_pattern) ? -0.5F : 0.2F;
  const char* debug_pattern = is_nu_pattern                     ? "nu"
                              : is_n_pattern                    ? "n"
                              : is_nai_pattern                  ? "nai"
                              : is_passive_pattern              ? "passive"
                              : is_classical_conjecture_pattern ? "mu"
                                                                : "beki";
  SUZUME_DEBUG_VERBOSE_BLOCK {
    SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface << " godan_mizenkei lemma=" << base_form << " cost=" << cost
                        << " pattern=" << debug_pattern << "\n";
  }
  const char* info_pattern = is_nu_pattern                     ? "godan_mizenkei_nu"
                             : is_n_pattern                    ? "godan_mizenkei_n"
                             : is_nai_pattern                  ? "godan_mizenkei_nai"
                             : is_nakatt_pattern               ? "godan_mizenkei_nakatt"
                             : is_passive_pattern              ? "godan_mizenkei_passive"
                             : is_classical_conjecture_pattern ? "godan_mizenkei_mu"
                                                               : "godan_mizenkei";
  // Use explicit VerbMizenkei EPOS for negative/passive patterns to enable bigram connection
  const core::ExtendedPOS epos = (is_nu_pattern || is_n_pattern || is_nai_pattern || is_nakatt_pattern ||
                                  is_passive_pattern || is_causative_pattern || is_classical_conjecture_pattern)
                                     ? core::ExtendedPOS::VerbMizenkei
                                     : core::ExtendedPOS::Unknown;
  candidates.push_back(makeVerbCandidate(surface, start_pos, mizenkei_end, cost, base_form,
                                         grammar::verbTypeToConjType(verb_type), true, CandidateOrigin::VerbKanji,
                                         candidate::kHighOriginConfidence, info_pattern, epos));
}

}  // namespace

void appendGodanMizenkeiPassiveCausativeCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                                   size_t kanji_end, size_t hiragana_end,
                                                   const grammar::Inflection& inflection,
                                                   const dictionary::DictionaryManager* dict_manager,
                                                   std::vector<UnknownCandidate>& candidates) {
  if (kanji_end - start_pos != 1 || kanji_end >= hiragana_end || kanji_end + 1 >= codepoints.size()) {
    return;
  }
  const char32_t a_row = codepoints[kanji_end];
  const char32_t after_a = codepoints[kanji_end + 1];
  // A-row + れ (passive) or A-row + せ (causative)
  if (!kana::isARowCodepoint(a_row) || (after_a != U'れ' && after_a != U'せ')) {
    return;
  }
  const grammar::VerbType verb_type = grammar::verbTypeFromARowCodepoint(a_row);
  const std::string_view base_suffix = grammar::godanBaseSuffixFromARow(a_row);
  if (verb_type == grammar::VerbType::Unknown || base_suffix.empty()) {
    return;
  }
  const std::string base_form = normalize::concat(extractSubstring(codepoints, start_pos, kanji_end), base_suffix);
  const std::string surface = extractSubstring(codepoints, start_pos, kanji_end + 1);

  // The kuru kanji takes its voice auxiliaries on the irrealis 来 itself.
  if (grammar::isKuruKanjiStem(codepoints[start_pos])) {
    return;
  }
  // A closed-class irregular form in L1 is authoritative over this
  // productive Godan fallback.  In particular, do not fabricate a
  // Godan lemma for an irregular verb's causative connection form.
  if (dict_manager != nullptr) {
    const dictionary::DictionaryEntry* exact = dict_manager->lookupExact(surface);
    if (exact != nullptr && exact->pos == core::PartOfSpeech::Verb &&
        exact->extended_pos == core::ExtendedPOS::VerbMizenkei) {
      return;
    }
  }

  // A causative せ needs an auxiliary after it, so a bare せ that closes the word is
  // the imperative of a registered godan-sa verb (転がせ ← 転がす, not 転が+せ).
  const bool closes_word = kanji_end + 2 >= codepoints.size() || !kana::isHiraganaCodepoint(codepoints[kanji_end + 2]);
  if (after_a == U'せ' && closes_word &&
      vh::isVerbInDictionary(dict_manager,
                             normalize::concat(extractSubstring(codepoints, start_pos, kanji_end + 1), "す"))) {
    return;
  }

  // Verify via inflection analysis of base form
  if (!analyzesAsVerbType(inflection, base_form, verb_type)) {
    return;
  }

  // Skip if kanji+A-row+る is a known godan-ra verb in dictionary
  // (potential form conflict). E.g., 泊まれる = potential of
  // 泊まる (godan-ra), not passive of 泊む (godan-ma).
  // 囲まれる = passive of 囲む is OK because 囲まる is not
  // in the dictionary.
  //
  // Also skip if kanji+A-row+れる is a known ichidan verb in
  // dictionary. E.g., 生まれる is ichidan, not passive of 生む.
  // Without this check, 生ま(mizenkei)+れ(passive) would
  // incorrectly win over the dictionary ichidan entry.
  if (after_a == U'れ' && (vh::isVerbInDictionary(dict_manager, surface + "る") ||
                           vh::isVerbInDictionary(dict_manager, surface + "れる"))) {
    return;
  }

  constexpr float kCost = candidate::verb_cost::kWeakPenalty;
  SUZUME_DEBUG_LOG("[VERB_CAND] " << surface << " godan_mizenkei_passive lemma=" << base_form << " cost=" << kCost
                                  << "\n");
  candidates.push_back(makeVerbCandidate(surface, start_pos, kanji_end + 1, kCost, base_form,
                                         grammar::verbTypeToConjType(verb_type), true, CandidateOrigin::VerbKanji, 0.8F,
                                         "godan_mizenkei_passive", core::ExtendedPOS::VerbMizenkei));
}

// Contracted sa-row mizenkei: kanji + しゃ + れ/せ/し
// Colloquial contraction さ→しゃ in passive/causative/emphatic negation
// E.g., 殺しゃれる → 殺しゃ (contracted mizenkei of 殺す) + れる (passive)
//       話しゃれる → 話しゃ (contracted mizenkei of 話す) + れる (passive)
//       出しゃしない → 出しゃ (contracted) + し + ない (emphatic neg)
// Only single-kanji stems (same constraint as godan-sa mizenkei above)
void appendSaRowContractedMizenkeiCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                             size_t kanji_end, size_t hiragana_end,
                                             const grammar::Inflection& inflection,
                                             std::vector<UnknownCandidate>& candidates) {
  if (kanji_end - start_pos != 1 || kanji_end + 1 >= hiragana_end || codepoints[kanji_end] != U'し' ||
      codepoints[kanji_end + 1] != U'ゃ' || kanji_end + 2 >= codepoints.size()) {
    return;
  }
  // しゃ + れ (passive) or しゃ + せ (causative) or しゃ + し (emphatic)
  const char32_t after = codepoints[kanji_end + 2];
  if (after != U'れ' && after != U'せ' && after != U'し') {
    return;
  }
  const std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
  const std::string base_form = kanji_stem + "す";
  if (!analyzesAsVerbType(inflection, base_form, grammar::VerbType::GodanSa)) {
    return;
  }
  const std::string surface = kanji_stem + "しゃ";
  constexpr float kCost = candidate::verb_cost::kWeakPenalty;
  SUZUME_DEBUG_LOG("[VERB_CAND] " << surface << " godan_sa_contracted_mizenkei lemma=" << base_form << " cost=" << kCost
                                  << "\n");
  candidates.push_back(makeVerbCandidate(
      surface, start_pos, kanji_end + 2, kCost, base_form, grammar::verbTypeToConjType(grammar::VerbType::GodanSa),
      false, CandidateOrigin::VerbKanji, 0.8F, "godan_sa_contracted_mizenkei", core::ExtendedPOS::VerbMizenkei));
}

// Godan mizenkei pattern: kanji + mizenkei ending + a classical negative
// auxiliary (ず/ざる/ざれ/ね/まじ)
// E.g., 抜かずに → 抜か (mizenkei of 抜く) + ず + に
//       行かずに → 行か (mizenkei of 行く) + ず + に
//       書かずに → 書か (mizenkei of 書く) + ず + に
//       欠かさず → 欠かさ (mizenkei of 欠かす) + ず
// The main loop skips short A-row hiragana as particles, so generate the
// complete mizenkei candidate explicitly when followed by a classical
// negative auxiliary.
void appendGodanMizenkeiZuCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                     size_t hiragana_end, const grammar::Inflection& inflection,
                                     const dictionary::DictionaryManager* dict_manager,
                                     std::vector<UnknownCandidate>& candidates) {
  // Every cell of the ぬ paradigm selects the irrealis, and so does the
  // classical negative conjecture (開か+ず, 開か+ざれ, 開か+ね, 吹か+まじ), so
  // the trigger is the auxiliary's class rather than any one spelling of it.
  // Naming individual cells leaves their siblings to be rebuilt as one
  // fabricated verb (開かねる) or as a bare kanji plus two particles.
  // The contracted ん belongs to the same class but also spells the honorific
  // さん and the ma/ba/na-row 音便, so it is left to the guarded branch in
  // appendGodanMizenkeiCandidates.
  size_t negative_pos = kanji_end;
  while (negative_pos < hiragana_end) {
    if (codepoints[negative_pos] != U'ん' &&
        vh::auxiliaryFollowsAt(dict_manager, codepoints, negative_pos, [](const dictionary::DictionaryEntry& entry) {
          return entry.extended_pos == core::ExtendedPOS::AuxNegativeNu ||
                 entry.extended_pos == core::ExtendedPOS::AuxNegativeMai;
        })) {
      break;
    }
    ++negative_pos;
  }
  if (negative_pos >= hiragana_end || negative_pos <= kanji_end) {
    return;
  }
  const bool is_single_kanji_stem = kanji_end - start_pos == 1;
  const char32_t mizenkei_ending = codepoints[negative_pos - 1];
  if (!kana::isARowCodepoint(mizenkei_ending)) {
    return;
  }
  const grammar::VerbType verb_type = grammar::verbTypeFromARowCodepoint(mizenkei_ending);
  const std::string_view base_suffix = grammar::godanBaseSuffixFromARow(mizenkei_ending);
  if (verb_type == grammar::VerbType::Unknown || base_suffix.empty()) {
    return;
  }
  const std::string surface = extractSubstring(codepoints, start_pos, negative_pos);
  const std::string base_form = normalize::concat(utf8::dropLastChar(surface), base_suffix);

  // A closed particle inside the proposed stem marks a morpheme
  // boundary (静けさ|のみ|なら|ず).  It cannot be evidence for an
  // unknown Godan mizenkei candidate.  Inspect the finite particle
  // lexicon rather than enumerating particle surfaces.
  const auto has_internal_particle = [&]() {
    if (dict_manager == nullptr) {
      return false;
    }
    for (size_t particle_start = start_pos + 1; particle_start + 1 < negative_pos; ++particle_start) {
      for (size_t particle_end = particle_start + 2; particle_end < negative_pos; ++particle_end) {
        if (lookupEntryInRange(*dict_manager, codepoints, particle_start, particle_end, core::PartOfSpeech::Particle) !=
            nullptr) {
          return true;
        }
      }
    }
    return false;
  };
  // So does an auxiliary standing on an onbin stem (書い+て+おら+ず).
  const bool contains_internal_particle =
      has_internal_particle() || vh::embedsAuxiliaryOnOnbinStem(codepoints, start_pos, negative_pos, dict_manager);
  // Verify via dictionary or inflection analysis of conjugated form
  const bool dictionary_verified = !contains_internal_particle && vh::isVerbInDictionary(dict_manager, base_form);
  bool is_valid = dictionary_verified;
  // The one-kana conjecture じ also spells a noun tail after the sa-row (小さじ,
  // 大さじ), so inference alone does not license a sa-row stem before it; it
  // needs a dictionary base (話さ+じ). Every other row has no such noun, so the
  // stem is inferred (咲か+じ). The ぬ paradigm and まじ keep the inferred stem.
  const bool non_ji_auxiliary_follows =
      vh::auxiliaryFollowsAt(dict_manager, codepoints, negative_pos, [](const dictionary::DictionaryEntry& entry) {
        return (entry.extended_pos == core::ExtendedPOS::AuxNegativeNu ||
                entry.extended_pos == core::ExtendedPOS::AuxNegativeMai) &&
               entry.lemma != "じ";
      });
  // The na-row is the closed class of 死ぬ/往ぬ, so it is never inferred
  // (暮れ+な+ず is not a coined 暮れぬ).
  const bool inferred_stem_licensed =
      (non_ji_auxiliary_follows || mizenkei_ending != U'さ') && verb_type != grammar::VerbType::GodanNa;
  if (!contains_internal_particle && !is_valid && is_single_kanji_stem && inferred_stem_licensed) {
    // Analyze mizenkei+ない form (standard negative) for better confidence
    // Base form alone may not be recognized. Multi-kanji stems require
    // dictionary evidence so a preceding noun cannot be absorbed.
    is_valid = vh::readsAsBaseForm(inflection, surface + "ない", base_form, verb_type, 0.3F);
  }

  // An irrealis whose own last mora is a registered case particle is
  // spelled exactly like the nominative or accusative phrase it sits in
  // (差|が|ずれる, not 差が|ず|れる). The particle reading needs no
  // lexical evidence, so the verb reading has to bring some: 泳が+ず and
  // 急が+ず keep their candidate because 泳ぐ and 急ぐ are attested,
  // while 差ぐ, 刻ぐ and 程ぐ are not words at all.
  const bool irrealis_ends_on_case_particle =
      !dictionary_verified && negative_pos > start_pos &&
      vh::oneMoraParticleEndsAt(dict_manager, codepoints, negative_pos, core::ExtendedPOS::ParticleCase);
  if (is_valid && !irrealis_ends_on_case_particle) {
    // A lexicalized verb+ず entry (思わず) wins unless the following に
    // explicitly creates the productive ずに auxiliary construction.
    const bool followed_by_zu = codepoints[negative_pos] == U'ず';
    const bool dict_has_zu_form =
        followed_by_zu && dict_manager != nullptr && dict_manager->lookupExact(surface + "ず") != nullptr;
    const bool followed_by_case_ni =
        followed_by_zu && negative_pos + 1 < codepoints.size() && codepoints[negative_pos + 1] == U'に';
    if (!dict_has_zu_form || followed_by_case_ni) {
      constexpr float kCost = candidate::verb_cost::kWeakPenalty;
      SUZUME_DEBUG_LOG("[VERB_CAND] " << surface << " godan_mizenkei_zu lemma=" << base_form << " cost=" << kCost
                                      << "\n");
      auto candidate = makeVerbCandidate(surface, start_pos, negative_pos, kCost, base_form,
                                         grammar::verbTypeToConjType(verb_type), true, CandidateOrigin::VerbKanji, 0.8F,
                                         "godan_mizenkei_zu", core::ExtendedPOS::VerbMizenkei);
      candidate.lemma_verified = dictionary_verified;
      candidates.push_back(std::move(candidate));
    }
  }
}

// Try Ichidan renyokei pattern: kanji + e-row/i-row hiragana
// 下一段 (shimo-ichidan): e-row ending (食べ, 見せ, 教え)
// 上一段 (kami-ichidan): i-row ending (感じ, 見, 居)
// These are standalone verb forms that connect to ます, ましょう, etc.
// The stem IS the entire surface (no conjugation suffix)

void appendKanjiMizenkeiStemCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                       size_t hiragana_end, const grammar::Inflection& inflection,
                                       const dictionary::DictionaryManager* dict_manager,
                                       std::vector<UnknownCandidate>& candidates) {
  // The colloquial negative contracts the Godan-ra irrealis and ない as
  // stem+ん+ない (分かん+ない ← 分かる).  The existing mizenkei scan keys on
  // the visible a-row mora, which this contraction has removed, so rebuild
  // the ra-row terminal from the stem immediately before ん instead.  The
  // contraction is on the irrealis, not on the auxiliary, so every cell of
  // ない takes it (分かん+なかっ+た, 分かん+なけれ+ば) — pinning the terminal
  // cell here left the rest of the paradigm cut at the ん.
  // The terminal contracts the same way before the prohibitive な and the
  // nominalizer の (帰ん+な+よ ← 帰るなよ, 分かん+の ← 分かるの); a ない-family
  // cell after ん is the irrealis case above.
  for (size_t n_pos = kanji_end; n_pos + 1 < hiragana_end; ++n_pos) {
    // The colloquial ねえ is the same negative (分かん+ねえ), as the hiragana
    // sibling of this scan already reads it.
    const bool negative_follows =
        vh::naiNegativeFollowsAt(codepoints, n_pos + 1) || vh::colloquialNegativeFollowsAt(codepoints, n_pos + 1);
    const bool terminal_follows =
        !negative_follows && (codepoints[n_pos + 1] == U'な' || codepoints[n_pos + 1] == U'の');
    // Before the copula and the reason で the ん is the nominalizer standing
    // for のだ/ので, so the contracted terminal ends at the stem (帰+ん+だ).
    const bool nominalizer_follows = codepoints[n_pos + 1] == U'だ' || codepoints[n_pos + 1] == U'で';
    if (codepoints[n_pos] != U'ん' || (!negative_follows && !terminal_follows && !nominalizer_follows)) {
      continue;
    }
    // A registered suffix reaching over the ん is that suffix: 姉+さん+の is
    // not the contracted terminal of a coined 姉さる. A kana noun there is
    // not evidence (曲が+ん+ない is not 曲+がん).
    if (dict_manager != nullptr) {
      bool covered = false;
      for (size_t begin = start_pos; begin < n_pos && !covered; ++begin) {
        for (const auto& result : lookupResultsInRange(*dict_manager, codepoints, begin, n_pos + 1)) {
          covered = covered || (result.entry != nullptr && begin + result.length > n_pos &&
                                result.entry->pos == core::PartOfSpeech::Suffix);
        }
      }
      if (covered) {
        continue;
      }
    }
    const std::string stem = extractSubstring(codepoints, start_pos, n_pos);
    // A stem closed by the te-form is the contracted aspect (見て+ん+の), and
    // one that passes through it is a subsidiary verb's contraction
    // (走って+くん+の), not a verb of its own.
    if ((terminal_follows || nominalizer_follows) &&
        std::any_of(codepoints.begin() + static_cast<std::ptrdiff_t>(kanji_end),
                    codepoints.begin() + static_cast<std::ptrdiff_t>(n_pos),
                    [](char32_t kana) { return kana == U'て' || kana == U'で'; })) {
      continue;
    }
    // A stem ending on the irrealis of a registered Godan verb puts the
    // contracted negative before な/の (知ら+ん+なあ ← 知る), not the る→ん
    // contraction of a coined 知らる.
    if (terminal_follows && vh::endsOnRegisteredGodanIrrealis(dict_manager, codepoints, start_pos, n_pos)) {
      continue;
    }
    const std::string base_form = stem + "る";
    // A bare kanji stem is also an Ichidan stem (見る, 着る); only an attested
    // ら irrealis proves the Godan-ra row the negative contraction needs
    // (帰ん+ない). The terminal contracts る→ん on either row, so before な/の a
    // bare Ichidan stem qualifies as well (見ん+な, 寝ん+な).
    const bool bare_ichidan_terminal = terminal_follows && n_pos == kanji_end && n_pos == start_pos + 1 &&
                                       vh::isSingleKanjiIchidan(codepoints[start_pos]);
    if (n_pos == kanji_end && !bare_ichidan_terminal && !vh::attestsGodanRaIrrealis(dict_manager, stem)) {
      continue;
    }
    // A kana okurigana before the nominalizer is a complete predicate of its
    // own (行く+ん+で), which the explanatory path handles, unless the stem is
    // a registered verb whose る the contraction removed (終わ+んだ ← 終わるんだ).
    const bool contracts_godan_ra_okurigana =
        nominalizer_follows && n_pos != kanji_end && vh::isVerbInDictionary(dict_manager, base_form);
    if (nominalizer_follows && n_pos != kanji_end && !contracts_godan_ra_okurigana) {
      continue;
    }
    // An okurigana stem contracts its Ichidan terminal the same way
    // (忘れん+な ← 忘れるな); only the negative needs the Godan-ra irrealis.
    const bool ichidan_terminal =
        bare_ichidan_terminal || (terminal_follows && n_pos != kanji_end &&
                                  !vh::isVerifiedVerbBase(dict_manager, inflection, base_form,
                                                          candidate::verb_cost::kConstructedVerbMinConfidence, true) &&
                                  vh::isVerifiedVerbBase(dict_manager, inflection, base_form,
                                                         candidate::verb_cost::kConstructedVerbMinConfidence, false));
    if (!ichidan_terminal && !vh::isVerifiedVerbBase(dict_manager, inflection, base_form,
                                                     candidate::verb_cost::kConstructedVerbMinConfidence, true)) {
      continue;
    }
    candidates.push_back(makeVerbCandidate(
        codepoints, start_pos, nominalizer_follows ? n_pos : n_pos + 1, candidate::verb_cost::kStandardBonus, base_form,
        ichidan_terminal ? dictionary::ConjugationType::Ichidan : dictionary::ConjugationType::GodanRa, true,
        CandidateOrigin::VerbKanji, candidate::kVerifiedConfidence,
        negative_follows ? "kanji_n_onbin_nai" : "kanji_n_contracted_terminal",
        negative_follows ? core::ExtendedPOS::VerbMizenkei : core::ExtendedPOS::VerbShuushikei));
    break;
  }

  // A passive may follow a Godan stem with more than one okurigana mora
  // (明かさ+れる).  Locate the A-row mora immediately before an explicit れ,
  // then validate the complete observed inflection.  Requiring that れ fixes
  // the ambiguity with lexical forms such as 知らせる, where the A-row mora is
  // followed by せ rather than a passive auxiliary.
  const std::string kanji_prefix = extractSubstring(codepoints, start_pos, kanji_end);
  const bool is_multi_kanji_nominal = kanji_end - start_pos >= 2 && grammar::isAllKanji(kanji_prefix);
  if (!is_multi_kanji_nominal) {
    for (size_t mizenkei_end = kanji_end + 2; mizenkei_end < hiragana_end; ++mizenkei_end) {
      const char32_t mizenkei_ending = codepoints[mizenkei_end - 1];
      if (codepoints[mizenkei_end] != U'れ' || !kana::isARowCodepoint(mizenkei_ending) ||
          !vh::isPassiveAuxContinuation(codepoints, mizenkei_end + 1, /*strict_masu=*/true)) {
        continue;
      }
      const grammar::VerbType verb_type = grammar::verbTypeFromARowCodepoint(mizenkei_ending);
      const std::string_view base_suffix = grammar::godanBaseSuffixFromARow(mizenkei_ending);
      if (verb_type == grammar::VerbType::Unknown || base_suffix.empty()) {
        continue;
      }

      // Do not absorb a dictionary-verified causative-passive chain into a
      // lexical Godan-ra/sa proposal.  聞か+せ+られ and 書か+さ+れ retain
      // their auxiliary boundaries; 明かさ+れ remains eligible because the
      // competing shorter base 明く is not attested.
      size_t underlying_a_row_pos = codepoints.size();
      if (mizenkei_ending == U'ら' && mizenkei_end >= kanji_end + 3 && codepoints[mizenkei_end - 2] == U'せ' &&
          kana::isARowCodepoint(codepoints[mizenkei_end - 3])) {
        underlying_a_row_pos = mizenkei_end - 3;
      } else if (mizenkei_ending == U'さ' && mizenkei_end >= kanji_end + 2 &&
                 kana::isARowCodepoint(codepoints[mizenkei_end - 2])) {
        underlying_a_row_pos = mizenkei_end - 2;
      }
      if (underlying_a_row_pos < codepoints.size()) {
        const dictionary::DictionaryEntry* underlying_exact =
            dict_manager == nullptr
                ? nullptr
                : lookupEntryInRange(*dict_manager, codepoints, start_pos, underlying_a_row_pos + 1);
        if (underlying_exact != nullptr && underlying_exact->pos == core::PartOfSpeech::Verb &&
            underlying_exact->extended_pos == core::ExtendedPOS::VerbMizenkei) {
          continue;
        }
        const std::string_view underlying_suffix = grammar::godanBaseSuffixFromARow(codepoints[underlying_a_row_pos]);
        const std::string underlying_base =
            normalize::concat(extractSubstring(codepoints, start_pos, underlying_a_row_pos), underlying_suffix);
        if (!underlying_suffix.empty() && vh::isVerbInDictionary(dict_manager, underlying_base)) {
          continue;
        }
      }

      const std::string surface = extractSubstring(codepoints, start_pos, mizenkei_end);
      const std::string stem = extractSubstring(codepoints, start_pos, mizenkei_end - 1);
      const std::string base_form = normalize::concat(stem, base_suffix);
      bool is_valid_verb = vh::isVerbInDictionary(dict_manager, base_form);
      // Before an unattested さ+れ, a continuative vowel (噛み+さ, 頬ずり+さ)
      // or a case/topic particle right after the kanji (話が+さ) closes a
      // nominal that takes the passive of する; no su-row stem ends there.
      if (!is_valid_verb && mizenkei_ending == U'さ') {
        const char32_t before_sa = codepoints[mizenkei_end - 2];
        const bool argument_before_sa =
            mizenkei_end - 2 == kanji_end &&
            (vh::oneMoraParticleEndsAt(dict_manager, codepoints, kanji_end + 1, core::ExtendedPOS::ParticleCase) ||
             vh::oneMoraParticleEndsAt(dict_manager, codepoints, kanji_end + 1, core::ExtendedPOS::ParticleTopic));
        if (kana::isIRowCodepoint(before_sa) || kana::isERowCodepoint(before_sa) || argument_before_sa) {
          SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << surface << "\" nominal + される, not a su-row stem\n");
          break;
        }
      }
      if (!is_valid_verb) {
        // Validate exactly one closed auxiliary inflection after れ.  Cutting
        // れなかった at れなか loses its base-form evidence, while consuming
        // beyond れて into a following aspect chain (れていない) crosses a
        // morpheme boundary.
        const size_t continuation_pos = mizenkei_end + 1;
        size_t observed_end = mizenkei_end + 2;  // れる / れた / れて
        const size_t negative_length = vh::naiNegativeFormLengthAt(codepoints, continuation_pos);
        if (negative_length != 0) {
          observed_end = continuation_pos + negative_length;
        } else if (codepoints[continuation_pos] == U'ま' && observed_end < hiragana_end) {
          ++observed_end;  // Preserve the existing れまし validation span.
        }
        observed_end = std::min(observed_end, hiragana_end);
        is_valid_verb = observedFormConfirms(inflection, codepoints, start_pos, observed_end, verb_type, base_form);
      }

      const std::string competing_ichidan = surface + "れる";
      if (is_valid_verb && !vh::isVerbInDictionary(dict_manager, competing_ichidan)) {
        candidates.push_back(makeVerbCandidate(surface, start_pos, mizenkei_end, candidate::verb_cost::kStrongBonus,
                                               base_form, grammar::verbTypeToConjType(verb_type), true,
                                               CandidateOrigin::VerbKanji, candidate::kHighOriginConfidence,
                                               "godan_mizenkei_passive_multi", core::ExtendedPOS::VerbMizenkei));
      }
      break;
    }
  }

  // A godan potential verb inflects as Ichidan. In the negative adverbial
  // pattern 読めなく/書けなく, its e-row stem must therefore be available as
  // the mizenkei of 読める/書ける, rather than only as the conditional form of
  // 読む/書く. Validate the underlying godan verb so ordinary Ichidan stems
  // such as 食べなく do not acquire a fabricated potential reading.
  if (kanji_end - start_pos == 1 && kanji_end + 2 < hiragana_end && kana::isERowCodepoint(codepoints[kanji_end]) &&
      codepoints[kanji_end + 1] == U'な' && codepoints[kanji_end + 2] == U'く') {
    const std::string_view base_suffix = grammar::godanBaseSuffixFromERow(codepoints[kanji_end]);
    if (!base_suffix.empty()) {
      const std::string kanji_stem = extractSubstring(codepoints, start_pos, kanji_end);
      const std::string base_form = normalize::concat(kanji_stem, base_suffix);
      if (vh::isVerifiedVerbBase(dict_manager, inflection, base_form,
                                 candidate::verb_cost::kConstructedVerbMinConfidence, true)) {
        const std::string surface = extractSubstring(codepoints, start_pos, kanji_end + 1);
        const std::string potential_lemma = surface + "る";
        candidates.push_back(makeVerbCandidate(surface, start_pos, kanji_end + 1, candidate::verb_cost::kWeakPenalty,
                                               potential_lemma, dictionary::ConjugationType::Ichidan, true,
                                               CandidateOrigin::VerbKanji, candidate::kHighOriginConfidence,
                                               "godan_potential_negative", core::ExtendedPOS::VerbMizenkei));
      }
    }
  }

  appendSingleOkuriganaMizenkeiCandidates(codepoints, start_pos, kanji_end, hiragana_end, inflection, dict_manager,
                                          candidates);

  // Generate mizenkei candidates for verbs with multiple okurigana + negative patterns
  // E.g., 分からない → 分から (mizenkei of 分かる) + ない
  //       分からなかった → 分から (mizenkei of 分かる) + なかっ + た
  //       始まらない → 始まら (mizenkei of 始まる) + ない
  // These are Godan verbs where the okurigana includes 2+ hiragana before the A-row ending
  if (hiragana_end >= kanji_end + 3) {
    const bool has_multi_kanji_stem = kanji_end - start_pos >= 2;
    const bool follows_case_particle =
        has_multi_kanji_stem &&
        vh::oneMoraParticleEndsAt(dict_manager, codepoints, start_pos, core::ExtendedPOS::ParticleCase);
    // Look for A-row hiragana + negative patterns (ない, なかっ, or ん)
    const size_t scan_start = has_multi_kanji_stem && follows_case_particle ? kanji_end : kanji_end + 1;
    for (size_t scan_pos = scan_start; scan_pos < hiragana_end - 1; ++scan_pos) {
      char32_t cur_char = codepoints[scan_pos];
      char32_t next_char = codepoints[scan_pos + 1];
      // Check if cur_char is A-row and followed by negative pattern
      if (!kana::isARowCodepoint(cur_char)) {
        continue;
      }
      bool is_nai_pattern = next_char == U'な' && scan_pos + 2 < codepoints.size() && codepoints[scan_pos + 2] == U'い';
      bool is_nakatt_pattern = next_char == U'な' && scan_pos + 3 < codepoints.size() &&
                               codepoints[scan_pos + 2] == U'か' && codepoints[scan_pos + 3] == U'っ';
      // Check for contracted negative ん pattern (分からん, 始まらん)
      // ん must be at the end of the string (hiragana_end == scan_pos + 2)
      bool is_n_pattern = next_char == U'ん' && scan_pos + 2 == hiragana_end;
      // Check for classical negative ぬ pattern (分からぬ, 変わらぬ)
      bool is_nu_pattern = next_char == U'ぬ';
      if (is_nai_pattern || is_nakatt_pattern || is_n_pattern || is_nu_pattern) {
        // Found A-row + negative pattern at scan_pos
        // The mizenkei would be from start_pos to scan_pos + 1
        size_t multi_miz_end = scan_pos + 1;
        grammar::VerbType verb_type = grammar::verbTypeFromARowCodepoint(cur_char);
        if (verb_type != grammar::VerbType::Unknown) {
          // Construct the base form
          // E.g., 分から → 分かる (replace A-row ending with U-row)
          std::string_view base_suffix = grammar::godanBaseSuffixFromARow(cur_char);
          if (!base_suffix.empty()) {
            std::string stem = extractSubstring(codepoints, start_pos, scan_pos);
            std::string base_form = normalize::concat(stem, base_suffix);
            std::string surface = extractSubstring(codepoints, start_pos, multi_miz_end);
            // An internal te-form followed by a subsidiary/aspect verb is a
            // grammatical boundary, not the irrealis of one lexical verb
            // (描いていかない → 描い + て + いか + ない).
            // @see fabricated closed-class absorption guards (verb_candidates_absorption_guards.h)
            if (vh::guardIsWired(vh::GuardMember::EmbedTeAuxiliary, vh::GuardOrigin::KanjiMizenkei) &&
                vh::embedsTeFormAuxiliary(surface)) {
              continue;
            }
            // The scan for the irrealis mora starts inside the okurigana and
            // runs to the end of the kana region, so it reaches past the word
            // and into the next phrase. A case particle in between marks an
            // argument boundary, which no single predicate spans: 資料 + を +
            // しら is read as the irrealis of the non-word 料をしる. Okurigana
            // that merely spells a case particle stays exempt, because the
            // irrealis mora closes the span immediately after it and the guard
            // requires kana on both sides of the particle (落と+さ+ない).
            // @see fabricated closed-class absorption guards (verb_candidates_absorption_guards.h)
            if (vh::embedsCaseParticle(dict_manager, codepoints, start_pos, multi_miz_end)) {
              continue;
            }
            // The scan also reaches past an auxiliary written with a kanji, and
            // an auxiliary heads nothing: 如く is a cell of the comparative 如し
            // and the あら behind it opens the next predicate, not okurigana of
            // the non-word 如くある.
            // @see fabricated closed-class absorption guards (verb_candidates_absorption_guards.h)
            if (vh::opensOnCompleteAuxiliary(dict_manager, codepoints, start_pos, multi_miz_end)) {
              continue;
            }
            // A span that is already a complete terminal form (遊ぶ, 行く) closes
            // its verb there; the a-row mora after it opens the next word
            // (遊ぶ+わん), not the irrealis of a verb spelled 遊ぶう.
            // The same holds for a complete i-adjective: the a-row mora after
            // it opens the next word (楽しい+わん), not a verb spelled 楽しいう.
            if (is_n_pattern && (vh::isVerifiedVerbBase(dict_manager, inflection, stem,
                                                        candidate::verb_cost::kConstructedVerbMinConfidence, true) ||
                                 vh::isAdjectiveInDictionary(dict_manager, stem))) {
              continue;
            }
            // A formal noun written in kanji is a word of its own, so kana after
            // it that open a dictionary verb cell belong to that verb (他+なら+ない),
            // not to an unregistered verb built on the noun.
            // @see fabricated closed-class absorption guards (verb_candidates_absorption_guards.h)
            if (dict_manager != nullptr && !vh::isVerbInDictionary(dict_manager, base_form)) {
              const auto* host =
                  lookupEntryInRange(*dict_manager, codepoints, start_pos, kanji_end, core::PartOfSpeech::Noun);
              if (host != nullptr && host->extended_pos == core::ExtendedPOS::NounFormal &&
                  lookupEntryInRange(*dict_manager, codepoints, kanji_end, multi_miz_end, core::PartOfSpeech::Verb) !=
                      nullptr) {
                continue;
              }
            }
            // Nor may an unregistered verb take a closed cell standing on its own
            // host (咲き+たら+ん, 一方+なら+ぬ).
            if (!vh::isVerbInDictionary(dict_manager, base_form) &&
                vh::absorbsRegisteredClosedCell(dict_manager, inflection, codepoints, start_pos, kanji_end,
                                                multi_miz_end)) {
              continue;
            }
            // Verify this is a valid verb
            const bool verified_base = vh::isVerifiedVerbBase(
                dict_manager, inflection, base_form, candidate::verb_cost::kConstructedVerbMinConfidence, true);
            bool is_valid_verb = verified_base;
            if (!is_valid_verb) {
              is_valid_verb =
                  observedFormConfirms(inflection, codepoints, start_pos, hiragana_end, verb_type, base_form);
            }
            // Reject a fabricated mizenkei that merely absorbs a trailing
            // binding particle (係助詞): 水すらない is noun + すら + ない, never
            // the mizenkei of a non-word godan-ra verb 水する. Only すら ends in
            // an a-row mora among binding particles, and no genuine godan verb
            // ends in 〜する, so this cannot suppress a real conjugation.
            // @see fabricated closed-class absorption guards (verb_candidates_absorption_guards.h)
            if (is_valid_verb && !vh::isVerbInDictionary(dict_manager, base_form) &&
                vh::endsWithParticleTailOfPos(dict_manager, codepoints, start_pos, multi_miz_end,
                                              core::ExtendedPOS::ParticleBinding)) {
              SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << extractSubstring(codepoints, start_pos, multi_miz_end)
                                                << "\" fabricated mizenkei absorbing binding particle\n");
              is_valid_verb = false;
            }
            // The same fabrication reaches one mora further out when the
            // absorbed particle marks a case: 変わりがない is 変わり + が + ない,
            // never the irrealis of the non-word 変わりぐ. The case particle sits
            // at the end of the span, so the embedded-particle guard above has no
            // suffix to see.
            // @see fabricated closed-class absorption guards (verb_candidates_absorption_guards.h)
            if (is_valid_verb && !vh::isVerbInDictionary(dict_manager, base_form) &&
                vh::endsWithCaseParticleAfterContinuative(dict_manager, inflection, codepoints, start_pos,
                                                          multi_miz_end)) {
              SUZUME_DEBUG_LOG("[VERB_SKIP] \"" << extractSubstring(codepoints, start_pos, multi_miz_end)
                                                << "\" fabricated mizenkei absorbing case particle\n");
              is_valid_verb = false;
            }
            // As in the single-okurigana irrealis: one ending on a registered
            // case particle needs a verified base (泳が+ない, 和らが+ない), not
            // just a whole-span reading, or it is the nominative phrase it is
            // spelled like (塩っけ+が+ない).
            if (is_valid_verb && !verified_base &&
                vh::oneMoraParticleEndsAt(dict_manager, codepoints, multi_miz_end, core::ExtendedPOS::ParticleCase)) {
              is_valid_verb = false;
            }
            if (is_valid_verb) {
              constexpr float kCost = candidate::verb_cost::kStandardBonus;  // Same as other negative patterns
              const char* pattern = is_nakatt_pattern ? "multi_mizenkei_nakatt"
                                    : is_n_pattern    ? "multi_mizenkei_n"
                                    : is_nu_pattern   ? "multi_mizenkei_nu"
                                                      : "multi_mizenkei_nai";
              SUZUME_DEBUG_VERBOSE_BLOCK {
                SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface << " " << pattern << " lemma=" << base_form
                                    << " cost=" << kCost << "\n";
              }
              candidates.push_back(makeVerbCandidate(surface, start_pos, multi_miz_end, kCost, base_form,
                                                     grammar::verbTypeToConjType(verb_type), true,
                                                     CandidateOrigin::VerbKanji, candidate::kHighOriginConfidence,
                                                     pattern, core::ExtendedPOS::VerbMizenkei));
            }
          }
        }
        break;  // Only generate one candidate per position
      }
    }
  }
}

}  // namespace suzume::analysis::kanji_verb_detail
