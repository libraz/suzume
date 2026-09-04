/**
 * @file suffix_candidates_suffix.cpp
 * @brief Suffix-based unknown word candidate generation
 */

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

#include "candidate_constants.h"
#include "core/debug.h"
#include "core/utf8_constants.h"
#include "dictionary/dictionary.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "grammar/inflection.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "tokenizer_utils.h"
#include "unknown.h"
#include "verb_candidates_helpers.h"

namespace suzume::analysis {

namespace {

/** @brief One inflected cell of a productive suffix verb */
struct SuffixVerbForm {
  std::string_view inflection;
  core::ExtendedPOS extended_pos;
};

/**
 * @brief The continuations a cell needs before its productive reading holds
 *
 * Each gate constrains a different cell of the paradigm, so a paradigm may need
 * more than one of them at once; they combine as flags rather than excluding
 * one another.
 */
enum SuffixCellGate : uint8_t {
  /** Every cell in the paradigm stands on its own */
  kGateNone = 0,
  /**
   * The bare continuative is also the nominalization of the same base, so it
   * needs an auxiliary only a verb can host (voice, te/past, negative, polite).
   */
  kGateRenyokeiNeedsVerbHost = 1U << 0U,
  /**
   * The ん-onbin cell exists only before the past だ / connective で. Every
   * other cell of that paradigm is spelled like a ma-row irrealis plus the
   * classical conjectural む, so it stays behind a two-character base.
   */
  kGateOnbinNeedsPastHost = 1U << 1U,
  /**
   * A Godan conditional cannot host the passive auxiliary: 〜づけられ is the
   * Ichidan stem of 〜づける, not the conditional of the Godan 〜づく.
   */
  kGateKateikeiRejectsPassive = 1U << 2U,
};

/** @brief One productive nominal-base suffix verb paradigm */
struct ProductiveSuffixVerb {
  const SuffixVerbForm* forms;
  size_t form_count;
  std::string_view lemma_suffix;
  dictionary::ConjugationType conj_type;
  const char* pattern;
  uint8_t gates;
};

/** @brief Whether an auxiliary only a verb can host starts at @p pos */
bool verbOnlyHostFollowsAt(const std::vector<char32_t>& codepoints, size_t pos) {
  if (pos >= codepoints.size()) {
    return false;
  }
  const char32_t head = codepoints[pos];
  if (head == U'て' || head == U'た' || head == U'ま') {
    return true;
  }
  if (pos + 1 >= codepoints.size()) {
    return false;
  }
  const char32_t next = codepoints[pos + 1];
  return (head == U'ら' && next == U'れ') || (head == U'な' && next == U'い');
}

// ～ばむ derives a Godan-ma verb of incipient appearance from a nominal base
// (気色ばむ, 黄ばむ, 汗ばむ). Its 終止形 is spelled like the irrealis of a
// ma-row verb plus the classical conjectural む, so the ordinary cells stay
// behind a two-kanji base. The ん-onbin cell has no such reading: that form
// exists only before the past だ / connective で, and requiring them lets a
// single-kanji base through (黄ばんだ, not 黄ば + ん + だ).
// The paradigm sits at file scope because the kanji verb generator has to know
// which okurigana this homography covers; see spellsGodanMaSuffixVerbCell.
constexpr std::array<SuffixVerbForm, 6> kGodanMaBamuForms = {{
    {"ばむ", core::ExtendedPOS::VerbShuushikei},
    {"ばま", core::ExtendedPOS::VerbMizenkei},
    {"ばも", core::ExtendedPOS::VerbMizenkei},
    {"ばみ", core::ExtendedPOS::VerbRenyokei},
    {"ばん", core::ExtendedPOS::VerbOnbinkei},
    {"ばめ", core::ExtendedPOS::VerbKateikei},
}};
constexpr ProductiveSuffixVerb kBamu = {
    kGodanMaBamuForms.data(),       kGodanMaBamuForms.size(), "ばむ", dictionary::ConjugationType::GodanMa,
    "nominal_godan_ma_bamu_suffix", kGateOnbinNeedsPastHost};

/**
 * @brief Emit the first cell of @p spec the surface at @p attach_pos spells
 *
 * The paradigms differ only in their table, lemma suffix, conjugation type and
 * the continuation one cell needs, so they share this scan rather than each
 * repeating the span arithmetic, the lemma construction and the candidate
 * fields. Cells are tried in table order and the first match wins, which is the
 * order each paradigm's own table already encodes.
 *
 * @param attach_pos Where the inflected suffix begins; the lemma keeps
 *        [start_pos, attach_pos) as its base
 * @return true when a candidate was emitted
 */
bool appendProductiveSuffixVerbCells(const std::vector<char32_t>& codepoints, size_t start_pos, size_t attach_pos,
                                     const ProductiveSuffixVerb& spec, std::vector<UnknownCandidate>& candidates) {
  for (size_t index = 0; index < spec.form_count; ++index) {
    const SuffixVerbForm& form = spec.forms[index];
    const size_t candidate_end = attach_pos + normalize::utf8Length(form.inflection);
    if (candidate_end > codepoints.size() ||
        extractSubstring(codepoints, attach_pos, candidate_end) != form.inflection) {
      continue;
    }
    bool gated_out = false;
    if ((spec.gates & kGateRenyokeiNeedsVerbHost) != 0U) {
      gated_out = gated_out || (form.extended_pos == core::ExtendedPOS::VerbRenyokei &&
                                !verbOnlyHostFollowsAt(codepoints, candidate_end));
    }
    if ((spec.gates & kGateOnbinNeedsPastHost) != 0U) {
      const bool past_or_te_follows = candidate_end < codepoints.size() &&
                                      (codepoints[candidate_end] == U'だ' || codepoints[candidate_end] == U'で');
      const bool licensed =
          form.extended_pos == core::ExtendedPOS::VerbOnbinkei ? past_or_te_follows : attach_pos - start_pos >= 2;
      gated_out = gated_out || !licensed;
    }
    if ((spec.gates & kGateKateikeiRejectsPassive) != 0U) {
      gated_out =
          gated_out || (form.extended_pos == core::ExtendedPOS::VerbKateikei && candidate_end + 1 < codepoints.size() &&
                        codepoints[candidate_end] == U'ら' && codepoints[candidate_end + 1] == U'れ');
    }
    if (gated_out) {
      continue;
    }

    const std::string surface = extractSubstring(codepoints, start_pos, candidate_end);
    const std::string lemma = normalize::concat(extractSubstring(codepoints, start_pos, attach_pos), spec.lemma_suffix);
    auto candidate = makeVerbCandidate(surface, start_pos, candidate_end, candidate::kProductiveSuffixVerbCost, lemma,
                                       spec.conj_type, true, CandidateOrigin::SuffixPattern,
                                       candidate::kDictionaryOriginConfidence, spec.pattern, form.extended_pos);
    // The productive suffix fixes both the lemma and the inflection, so this is
    // not an unconstrained kanji onbin candidate.
    candidate.lemma_verified = true;
    candidates.push_back(std::move(candidate));
    return true;
  }
  return false;
}

}  // namespace

bool spellsGodanMaSuffixVerbCell(std::string_view okurigana) {
  for (size_t index = 0; index < kBamu.form_count; ++index) {
    if (okurigana == kBamu.forms[index].inflection) {
      return true;
    }
  }
  return false;
}

// =============================================================================
// Suffix Candidate Factory Helpers
// =============================================================================

/**
 * @brief Create a suffix pattern candidate with lemma
 */
inline UnknownCandidate makeSuffixCandidate(const std::string& surface, size_t start, size_t end,
                                            core::PartOfSpeech pos, float cost, const std::string& lemma,
                                            [[maybe_unused]] float confidence, [[maybe_unused]] const char* pattern,
                                            dictionary::ConjugationType conj_type = dictionary::ConjugationType::None) {
  auto cand = makeCandidate(surface, start, end, pos, cost, true, CandidateOrigin::SuffixPattern);
  cand.lemma = lemma;
  cand.conj_type = conj_type;
#ifdef SUZUME_DEBUG_INFO
  cand.confidence = confidence;
  cand.pattern = pattern;
#endif
  return cand;
}

/**
 * @brief Create a suffix pattern candidate without lemma
 */
inline UnknownCandidate makeSuffixCandidateNoLemma(const std::string& surface, size_t start, size_t end,
                                                   core::PartOfSpeech pos, float cost,
                                                   [[maybe_unused]] float confidence,
                                                   [[maybe_unused]] const char* pattern) {
  auto cand = makeCandidate(surface, start, end, pos, cost, true, CandidateOrigin::SuffixPattern);
#ifdef SUZUME_DEBUG_INFO
  cand.confidence = confidence;
  cand.pattern = pattern;
#endif
  return cand;
}

SuffixEntryRange getSuffixEntries() {
  static constexpr SuffixEntry kSuffixes[] = {
      // Tokenizer use case: keep X+SUFFIX as one search unit. The following
      // suffixes are merged via kanji-merge normalization, not split here:
      //   家/力/法/論/員/式/感/的 (productive but one search unit)
      {"化", true},   // 国際化, 自動化
      {"視", false},  // 重要視, 問題視
      {"性", true},
      // {"率", core::PartOfSpeech::Suffix},  // Removed: causes over-segmentation (降水確率→降水確+率)
      // {"法", core::PartOfSpeech::Suffix},  // Merge via kanji-merge (解決法, 民法)
      // {"論", core::PartOfSpeech::Suffix},  // Merge via kanji-merge (進化論, 理論)
      // {"者", core::PartOfSpeech::Suffix},  // Removed: causes over-segmentation (代表者→代表+者)
      // {"家", core::PartOfSpeech::Suffix},  // Removed: causes over-segmentation (大家/思想家/政治家/etc.)
      // {"員", core::PartOfSpeech::Suffix},  // Merge via kanji-merge (会社員, 公務員)
      // {"式", core::PartOfSpeech::Suffix},  // Merge via kanji-merge (計算式, 結婚式)
      // {"感", core::PartOfSpeech::Suffix},  // Merge via kanji-merge (達成感, 違和感)
      // {"力", core::PartOfSpeech::Suffix},  // Merge via kanji-merge (説得力, 影響力)
      {"度", true},
      {"方", false},  // 歩き方, やり方 (V連用形+方)
      {"中", false},  // 一日中, 今日中 (N+中) - MeCab treats as suffix
      {"末", false},  // 年度末, 学期末
      // N中 compounds (今日中, 世界中, 一日中) are handled as compound nouns
      // Administrative suffixes (行政接尾辞)
      {"県", false},
      {"都", false},
      {"府", false},
      {"道", false},
      {"市", false},
      {"区", false},
      {"町", false},
      {"村", false},
      {"庁", false},
      {"署", false},
      {"局", false},
      {"省", false},
      {"院", false},
      {"所", false},
  };
  return {kSuffixes, std::size(kSuffixes)};
}

const std::array<std::string_view, 1>& getNaAdjSuffixes() {
  static constexpr std::array<std::string_view, 1> kNaAdjSuffixes = {
      "的",  // 理性的, 論理的, etc.
  };
  return kNaAdjSuffixes;
}

// =============================================================================
// Productive Hiragana Suffix Patterns (生産的接尾辞)
// =============================================================================

void generateProductiveSuffixCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                        const std::vector<normalize::CharType>& char_types,
                                        std::vector<UnknownCandidate>& candidates) {
  // Only for hiragana sequences
  if (start_pos >= char_types.size() || char_types[start_pos] != normalize::CharType::Hiragana) {
    return;
  }

  constexpr size_t kPpoiLen = 9;  // "っぽい" = 3 chars * 3 bytes

  // Try different lengths of hiragana (3 to 6 chars for stem + がち/っぽい)
  for (size_t hira_len = 3; hira_len <= 8; ++hira_len) {
    size_t candidate_end = start_pos + hira_len;
    if (candidate_end > char_types.size()) {
      break;
    }

    // Check all positions are hiragana
    bool all_hiragana = true;
    for (size_t i = start_pos; i < candidate_end; ++i) {
      if (char_types[i] != normalize::CharType::Hiragana) {
        all_hiragana = false;
        break;
      }
    }
    if (!all_hiragana) {
      break;  // No more hiragana
    }

    std::string surface = extractSubstring(codepoints, start_pos, candidate_end);

    // がち (tendency suffix) is intentionally NOT merged here: MeCab splits
    // あり|がち, なり|がち (verb renyokei + suffix), so the split path wins.

    // Pattern 2: V連用形 + っぽい (resemblance suffix)
    // Examples: 子供っぽい、安っぽい、忘れっぽい
    if (surface.size() >= kPpoiLen + 3 && utf8::endsWith(surface, "っぽい")) {
      std::string_view stem = std::string_view(surface).substr(0, surface.size() - kPpoiLen);
      // っぽい attaches to nouns and verb stems, less strict check
      if (stem.size() >= 3) {  // At least 1 character stem
        candidates.push_back(makeSuffixCandidate(surface, start_pos, candidate_end, core::PartOfSpeech::Adjective, 0.4F,
                                                 surface, 0.85F, "stem_ppoi", dictionary::ConjugationType::IAdjective));
        return;  // Found valid っぽい candidate
      }
    }

    // Pattern 3: Short hiragana nickname + ちゃん/くん, plus an honorific
    // family-style stem followed by さん.
    // Examples: たっちゃん, ゆうちゃん, けんちゃん, わんちゃん, けんくん.
    // The honorific さん remains an independent suffix: it is a useful search
    // boundary and short ordinary hiragana words must not be reclassified as
    // nicknames merely because they precede it. Lexicalized family terms are
    // supplied by the dictionary.
    if (surface.size() >= 9) {  // at least 1-char stem (3 bytes) + 2+ char honorific
      for (const auto* honorific : {"ちゃん", "くん", "さん"}) {
        std::string_view h(honorific);
        if (!utf8::endsWith(surface, h)) {
          continue;
        }
        std::string_view stem = std::string_view(surface).substr(0, surface.size() - h.size());
        size_t stem_chars = stem.size() / 3;  // Each hiragana = 3 bytes in UTF-8
        if (stem_chars >= 2 && stem_chars <= 3) {
          // Only an honorific-style stem can lexicalize with さん. Ordinary
          // さん terms remain dictionary-backed or split above.
          bool starts_with_honorific_prefix =
              stem.size() >= 3 && (stem.compare(0, 3, "お") == 0 || stem.compare(0, 3, "ご") == 0);
          if (h == "さん" && !starts_with_honorific_prefix) {
            break;
          }
          float cost = starts_with_honorific_prefix ? -1.5F : -0.5F;
          candidates.push_back(makeSuffixCandidate(surface, start_pos, candidate_end, core::PartOfSpeech::Noun, cost,
                                                   surface, 0.9F, "hira_nickname"));
          return;
        }
        break;
      }
    }
  }
}

void generateProductiveSuffixVerbCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                            const std::vector<normalize::CharType>& char_types,
                                            std::vector<UnknownCandidate>& candidates) {
  if (start_pos >= char_types.size() || char_types[start_pos] != normalize::CharType::Kanji) {
    return;
  }

  size_t base_end = start_pos;
  while (base_end < char_types.size() && char_types[base_end] == normalize::CharType::Kanji) {
    ++base_end;
  }
  if (base_end == start_pos) {
    return;
  }

  // A productive suffix verb may attach after a repeated quantity unit, but
  // it cannot begin inside that already-complete unit and cross its right
  // boundary. Share the same identical numeral+noun-unit evidence used by the
  // counter generator, so candidates start at the predicate rather than the
  // second half of 一語一語-like expressions.
  const auto crosses_repeated_quantity_boundary = [&](size_t scan_start, size_t scan_end) {
    for (size_t unit_start = scan_start; unit_start < scan_end; ++unit_start) {
      const size_t unit_end = repeatedNumeralNounUnitEndAt(codepoints, char_types, unit_start);
      if (unit_end > start_pos && unit_end < base_end) {
        return true;
      }
    }
    return false;
  };
  if (crosses_repeated_quantity_boundary(start_pos, base_end) || crosses_repeated_quantity_boundary(0, start_pos)) {
    return;
  }

  // A numeral-led kanji sequence before ～づく is an independent quantity or
  // repeated-count noun followed by a lexical verb (一歩一歩+近づく), not the
  // nominal base of a productive suffix verb.  Without this boundary the
  // suffix fallback can absorb the whole preceding count expression.
  const bool numeral_led_base = normalize::isNumeralCodepoint(codepoints[start_pos]);

  static constexpr std::array<SuffixVerbForm, 5> kIchidanTsukeruForms = {{
      {"付ける", core::ExtendedPOS::VerbShuushikei},
      {"付け", core::ExtendedPOS::VerbRenyokei},
      {"付けれ", core::ExtendedPOS::VerbKateikei},
      {"付けよ", core::ExtendedPOS::VerbMeireikei},
      {"付けろ", core::ExtendedPOS::VerbMeireikei},
  }};

  // 漢字語幹+付ける is a productive compound-verb pattern (関連付ける、
  // 位置付ける、名付ける). The base only has to be non-empty: the competing
  // reading of a one-kanji base is a bare nominal in front of a finite verb
  // with no case particle, which is not a clause. The continuative cell is
  // homographic with the deverbal nominal (味付け、値付け), and there the
  // nominal candidate wins on category cost rather than needing a gate.
  // Both halves attach to the same nominal bases, so they share one admission
  // test: the kanji run ends in the suffix character and leaves a non-empty
  // base in front of it.
  const size_t tsukeru_base_end = base_end > start_pos ? base_end - 1 : start_pos;
  const bool has_kanji_tsuku_base =
      !numeral_led_base && tsukeru_base_end > start_pos && codepoints[tsukeru_base_end] == U'付';
  if (has_kanji_tsuku_base) {
    static constexpr ProductiveSuffixVerb kTsukeru = {
        kIchidanTsukeruForms.data(),          kIchidanTsukeruForms.size(), "付ける",
        dictionary::ConjugationType::Ichidan, "nominal_ichidan_suffix",    kGateNone};
    if (appendProductiveSuffixVerbCells(codepoints, start_pos, tsukeru_base_end, kTsukeru, candidates)) {
      return;
    }
  }

  // 付く is the Godan half of the same pair, and the kanji spelling loses the
  // sequential voicing that makes づく self-evidently compound-internal
  // (根付く, 色付く, 傷付く). The non-clause argument above does not cover the
  // continuative: 〜付き is also a nominal suffix over the same bases (目付き,
  // 条件付き), so that cell needs a verb-only host.
  static constexpr std::array<SuffixVerbForm, 6> kGodanTsukuForms = {{
      {"付く", core::ExtendedPOS::VerbShuushikei},
      {"付か", core::ExtendedPOS::VerbMizenkei},
      {"付き", core::ExtendedPOS::VerbRenyokei},
      {"付い", core::ExtendedPOS::VerbOnbinkei},
      {"付け", core::ExtendedPOS::VerbKateikei},
      {"付こ", core::ExtendedPOS::VerbMizenkei},
  }};
  if (has_kanji_tsuku_base) {
    static constexpr ProductiveSuffixVerb kTsuku = {kGodanTsukuForms.data(),
                                                    kGodanTsukuForms.size(),
                                                    "付く",
                                                    dictionary::ConjugationType::GodanKa,
                                                    "nominal_godan_ka_suffix",
                                                    kGateKateikeiRejectsPassive | kGateRenyokeiNeedsVerbHost};
    if (appendProductiveSuffixVerbCells(codepoints, start_pos, tsukeru_base_end, kTsuku, candidates)) {
      return;
    }
  }

  if (numeral_led_base) {
    return;
  }

  // The productive nominal suffix ～づける is an Ichidan verb (意味づける,
  // 印象づける), distinct from the Godan ～づく.  Its renyokei is locally
  // ambiguous with the conditional of ～づく, so require an Ichidan-only
  // continuation before emitting it.  This lets voice and te-form chains
  // select the grammatically licensed lemma without changing standalone
  // nominal or conditional uses.
  static constexpr std::array<SuffixVerbForm, 5> kIchidanZukeruForms = {{
      {"づける", core::ExtendedPOS::VerbShuushikei},
      {"づけ", core::ExtendedPOS::VerbRenyokei},
      {"づけれ", core::ExtendedPOS::VerbKateikei},
      {"づけよ", core::ExtendedPOS::VerbMeireikei},
      {"づけろ", core::ExtendedPOS::VerbMeireikei},
  }};
  static constexpr ProductiveSuffixVerb kZukeru = {
      kIchidanZukeruForms.data(),           kIchidanZukeruForms.size(),      "づける",
      dictionary::ConjugationType::Ichidan, "nominal_ichidan_zukeru_suffix", kGateRenyokeiNeedsVerbHost};
  if (appendProductiveSuffixVerbCells(codepoints, start_pos, base_end, kZukeru, candidates)) {
    return;
  }

  // ～びる derives an Ichidan verb meaning "to take on the character of" from a
  // nominal base (大人びる, 田舎びる, 都会びる). Its stem is the whole kanji run,
  // which the general kanji+okurigana path cannot reach: that path scores a
  // longer stem as less likely and settles on a shorter one inside the run
  // (大人びた read as 大 + 人び + た). Two or more kanji are required so the
  // lexical single-kanji verbs spelled the same way (帯びる, 浴びる) keep their
  // ordinary candidate path.
  static constexpr std::array<SuffixVerbForm, 5> kIchidanBiruForms = {{
      {"びる", core::ExtendedPOS::VerbShuushikei},
      {"び", core::ExtendedPOS::VerbRenyokei},
      {"びれ", core::ExtendedPOS::VerbKateikei},
      {"びよ", core::ExtendedPOS::VerbMeireikei},
      {"びろ", core::ExtendedPOS::VerbMeireikei},
  }};
  if (base_end - start_pos >= 2) {
    static constexpr ProductiveSuffixVerb kBiru = {
        kIchidanBiruForms.data(),      kIchidanBiruForms.size(),  "びる", dictionary::ConjugationType::Ichidan,
        "nominal_ichidan_biru_suffix", kGateRenyokeiNeedsVerbHost};
    if (appendProductiveSuffixVerbCells(codepoints, start_pos, base_end, kBiru, candidates)) {
      return;
    }
  }

  if (appendProductiveSuffixVerbCells(codepoints, start_pos, base_end, kBamu, candidates)) {
    return;
  }

  // めかす derives a transitive verb from the same nominal bases as めく
  // (冗談めかす, 秘密めかした) but inflects as Godan-sa, so its cells are not
  // reachable from the Godan-ka table below. Without them the surface is read
  // as めく's irrealis plus the classical causative す, which puts the boundary
  // one mora early and turns the continuative into an auxiliary.
  static constexpr std::array<SuffixVerbForm, 5> kMekasuForms = {{
      {"めかす", core::ExtendedPOS::VerbShuushikei},
      {"めかさ", core::ExtendedPOS::VerbMizenkei},
      {"めかし", core::ExtendedPOS::VerbRenyokei},
      {"めかせ", core::ExtendedPOS::VerbKateikei},
      {"めかそ", core::ExtendedPOS::VerbMizenkei},
  }};
  static constexpr ProductiveSuffixVerb kMekasu = {kMekasuForms.data(),
                                                   kMekasuForms.size(),
                                                   "めかす",
                                                   dictionary::ConjugationType::GodanSa,
                                                   "nominal_godan_sa_mekasu_suffix",
                                                   kGateNone};
  if (appendProductiveSuffixVerbCells(codepoints, start_pos, base_end, kMekasu, candidates)) {
    return;
  }

  // ～めく and ～づく are the Godan-ka pair over the same nominal bases. Their
  // conditional cell is spelled like the Ichidan stem of ～づける, so it keeps
  // its valid conditional context (～づけば) while the impossible voice
  // attachment (～づけられ) is rejected.
  static constexpr std::array<SuffixVerbForm, 6> kMekuForms = {{
      {"めく", core::ExtendedPOS::VerbShuushikei},
      {"めか", core::ExtendedPOS::VerbMizenkei},
      {"めき", core::ExtendedPOS::VerbRenyokei},
      {"めい", core::ExtendedPOS::VerbOnbinkei},
      {"めけ", core::ExtendedPOS::VerbKateikei},
      {"めこ", core::ExtendedPOS::VerbMizenkei},
  }};
  static constexpr std::array<SuffixVerbForm, 6> kZukuForms = {{
      {"づく", core::ExtendedPOS::VerbShuushikei},
      {"づか", core::ExtendedPOS::VerbMizenkei},
      {"づき", core::ExtendedPOS::VerbRenyokei},
      {"づい", core::ExtendedPOS::VerbOnbinkei},
      {"づけ", core::ExtendedPOS::VerbKateikei},
      {"づこ", core::ExtendedPOS::VerbMizenkei},
  }};
  static constexpr std::array<ProductiveSuffixVerb, 2> kGodanKaSuffixes = {{
      {kMekuForms.data(), kMekuForms.size(), "めく", dictionary::ConjugationType::GodanKa, "nominal_godan_ka_suffix",
       kGateKateikeiRejectsPassive},
      {kZukuForms.data(), kZukuForms.size(), "づく", dictionary::ConjugationType::GodanKa, "nominal_godan_ka_suffix",
       kGateKateikeiRejectsPassive},
  }};
  for (const auto& spec : kGodanKaSuffixes) {
    if (appendProductiveSuffixVerbCells(codepoints, start_pos, base_end, spec, candidates)) {
      return;
    }
  }
}

// Administrative suffix codepoints for intermediate boundary detection
const std::array<char32_t, 8>& getAdminSuffixCodepoints() {
  static constexpr std::array<char32_t, 8> kAdminSuffixes = {U'県', U'都', U'府', U'道', U'市', U'区', U'町', U'村'};
  return kAdminSuffixes;
}

void generateAdminBoundaryCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                     const std::vector<normalize::CharType>& char_types,
                                     std::vector<UnknownCandidate>& candidates) {
  if (start_pos >= char_types.size() || char_types[start_pos] != normalize::CharType::Kanji) {
    return;
  }

  const auto& admin_suffixes = getAdminSuffixCodepoints();

  // Scan through kanji sequence looking for administrative suffixes
  for (size_t pos = start_pos + 1; pos < char_types.size() && pos < start_pos + 6; ++pos) {
    if (char_types[pos] != normalize::CharType::Kanji) {
      break;
    }

    char32_t cp = codepoints[pos];
    bool is_admin_suffix = std::find(admin_suffixes.begin(), admin_suffixes.end(), cp) != admin_suffixes.end();

    if (is_admin_suffix) {
      // Found administrative suffix at position pos
      // Generate candidate from start_pos to pos+1 (including the suffix)
      size_t end_with_suffix = pos + 1;
      std::string surface = extractSubstring(codepoints, start_pos, end_with_suffix);
      candidates.push_back(makeSuffixCandidateNoLemma(surface, start_pos, end_with_suffix, core::PartOfSpeech::Noun,
                                                      0.3F, 0.95F, "admin_boundary"));
    }
  }
}

void generateWithSuffix(const std::vector<char32_t>& codepoints, size_t start_pos,
                        const std::vector<normalize::CharType>& char_types, const UnknownOptions& options,
                        std::vector<UnknownCandidate>& candidates) {
  if (start_pos >= char_types.size() || char_types[start_pos] != normalize::CharType::Kanji) {
    return;
  }

  // First, generate candidates for administrative boundaries
  generateAdminBoundaryCandidates(codepoints, start_pos, char_types, candidates);

  // Find kanji sequence
  size_t end_pos = start_pos;
  while (end_pos < char_types.size() && end_pos - start_pos < options.max_kanji_length &&
         char_types[end_pos] == normalize::CharType::Kanji) {
    ++end_pos;
  }

  if (end_pos <= start_pos + 1) {
    return;
  }

  std::string kanji_seq = extractSubstring(codepoints, start_pos, end_pos);
  const auto& suffixes = getSuffixEntries();

  // Check for suffixes
  for (const auto& [suffix, forms_derived_compound] : suffixes) {
    if (kanji_seq.size() > suffix.size() &&
        kanji_seq.compare(kanji_seq.size() - suffix.size(), suffix.size(), suffix) == 0) {
      // Calculate stem length in codepoints
      const size_t suffix_length = normalize::utf8Length(suffix);
      size_t stem_end = end_pos - suffix_length;
      size_t stem_codepoint_len = stem_end - start_pos;

      // Restrict suffix-stem split to 2-char kanji stems.
      // Typical kango "X+suffix" patterns (思想家, 国際法, 公務員) all have a 2-char stem.
      // 3+ char stems before a 1-char suffix usually indicate the kanji_seq is actually
      // two adjacent kango compounds (e.g., 新規手法 = 新規 + 手法, not 新規手 + 法).
      // Longer stems with a real suffix (大企業家) are reached via the PREFIX path
      // (大 prefix + 企業家 → 企業 + 家).
      if (stem_codepoint_len > 2) {
        continue;
      }

      // Skip suffix-stem when the stem starts with the L1 PREFIX kanji 御.
      // The prefix path (御 + 尽力 NOUN) should win over 御尽 + 力 (suffix path).
      // Without this skip, suffix path cost (0.7) + NOUN→SUFFIX bonus (-0.8) makes
      // 御尽 + 力 cheaper than 御(PREFIX) + 尽力(kanji_seq).
      if (codepoints[start_pos] == U'御') {
        continue;
      }

      if (stem_end > start_pos + 1) {
        // Add stem candidate
        std::string stem_surface = extractSubstring(codepoints, start_pos, stem_end);

        UnknownCandidate stem;
        stem.surface = stem_surface;
        stem.start = start_pos;
        stem.end = stem_end;
        stem.pos = core::PartOfSpeech::Noun;
        stem.cost = 1.0F + options.suffix_separation_bonus;
        stem.has_suffix = false;
#ifdef SUZUME_DEBUG_INFO
        stem.origin = CandidateOrigin::SuffixPattern;
        stem.confidence = 1.0F;
        stem.pattern = normalize::concat("stem_before_", suffix);
#endif
        candidates.push_back(stem);

        // Add whole word candidate too
        UnknownCandidate whole;
        whole.surface = kanji_seq;
        whole.start = start_pos;
        whole.end = end_pos;
        whole.pos = core::PartOfSpeech::Noun;
        whole.cost =
            forms_derived_compound ? candidate::kDerivedSuffixCompoundNounCost : candidate::kSuffixWholeCandidateCost;
        whole.has_suffix = true;
#ifdef SUZUME_DEBUG_INFO
        whole.origin = CandidateOrigin::SuffixPattern;
        whole.confidence = 1.0F;
        whole.pattern = normalize::concat("with_suffix_", suffix);
#endif
        candidates.push_back(whole);

        break;  // Use longest matching suffix
      }
    }
  }
}

}  // namespace suzume::analysis
