/**
 * @file verb_candidates_kanji_classical.cpp
 * @brief Classical kanji-stem candidates: the ハ行四段 paradigm and ク語法
 */

#include <algorithm>
#include <initializer_list>

#include "analysis/candidate_constants.h"
#include "analysis/verb_candidates_helpers.h"
#include "analysis/verb_candidates_kanji_internal.h"
#include "core/debug.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "grammar/verb_endings.h"
#include "normalize/char_type.h"
#include "tokenizer_utils.h"
#include "unknown.h"
#include "verb_candidates.h"

namespace suzume::analysis::kanji_verb_detail {
namespace vh = verb_helpers;

namespace {

// Longest closed-class tail probed after a paradigm cell (ざり, ども).
constexpr size_t kClassicalTailProbeChars = 3;

// Longest okurigana run allowed between the kanji stem and the row kana
// (移ろ+ひ, 恥ぢら+ひ).
constexpr size_t kOkuriganaProbeChars = 2;

// The classical nominalizer that turns an irrealis cell into a noun (言わ+く).
constexpr char32_t kKuNominalizer = U'く';

/**
 * @brief Paradigm cell named by a ha-row tail kana.
 *
 * The classical ハ行四段 row is the historical-kana spelling of the modern
 * ワ行五段 row (思ふ/思う, 移ろふ/移ろう): only the tail kana differs, so the
 * paradigm needs no conjugation table of its own.  The tail names the cell and
 * the base form keeps the historical terminal ふ.  終止形 and 連体形 share one
 * form; 已然形 and 命令形 also share one, taken here as the modern conditional
 * slot and narrowed to the imperative when the clause ends there.
 */
core::ExtendedPOS classicalHaRowCell(char32_t tail) {
  switch (tail) {
    case U'は':
      return core::ExtendedPOS::VerbMizenkei;
    case U'ひ':
      return core::ExtendedPOS::VerbRenyokei;
    case U'ふ':
      return core::ExtendedPOS::VerbShuushikei;
    case U'へ':
      return core::ExtendedPOS::VerbKateikei;
    default:
      return core::ExtendedPOS::Unknown;
  }
}

bool dictionaryTailFollowsAt(const std::vector<char32_t>& codepoints, size_t pos,
                             const dictionary::DictionaryManager* dict_manager, core::PartOfSpeech pos_class,
                             std::initializer_list<core::ExtendedPOS> accepted) {
  if (dict_manager == nullptr || pos >= codepoints.size()) {
    return false;
  }
  const size_t probe_end = std::min(codepoints.size(), pos + kClassicalTailProbeChars);
  for (size_t end = pos + 1; end <= probe_end; ++end) {
    const auto* entry = dict_manager->lookupExact(extractSubstring(codepoints, pos, end), pos_class);
    if (entry == nullptr) {
      continue;
    }
    for (const core::ExtendedPOS candidate_pos : accepted) {
      if (entry->extended_pos == candidate_pos) {
        return true;
      }
    }
  }
  return false;
}

/**
 * @brief Whether a predicate form ends exactly where this span begins.
 *
 * A one-kanji ハ行四段 stem that sits on a finished predicate is a subsidiary
 * verb (書き+給へ, 読ませ+給へ). Requiring that host keeps the imperative cell
 * away from a bare nominal plus the direction particle (東京+へ).
 */
bool predicateEndsAt(const std::vector<char32_t>& codepoints, size_t pos,
                     const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || pos == 0) {
    return false;
  }
  const size_t probe_start = (pos >= 2) ? pos - 2 : 0;
  return hasDictionaryEntryEndingAt(
      *dict_manager, codepoints, probe_start, pos,
      partOfSpeechMask(core::PartOfSpeech::Verb) | partOfSpeechMask(core::PartOfSpeech::Auxiliary));
}

bool clauseEndsAt(const std::vector<char32_t>& codepoints, size_t pos) {
  if (pos >= codepoints.size()) {
    return true;
  }
  const char32_t next = codepoints[pos];
  return next == U'。' || next == U'、' || next == U'！' || next == U'？' || next == U'」';
}

// A 終止形 closes its clause or carries an auxiliary that attaches to one: the
// conjectural べし and its negative counterpart まじ, the volitional む, and the
// hearsay なり. Anything else after the cell belongs to a different form.
bool shuushikeiEndsAt(const std::vector<char32_t>& codepoints, size_t pos,
                      const dictionary::DictionaryManager* dict_manager) {
  return clauseEndsAt(codepoints, pos) ||
         dictionaryTailFollowsAt(codepoints, pos, dict_manager, core::PartOfSpeech::Auxiliary,
                                 {core::ExtendedPOS::AuxClassicalBeshi, core::ExtendedPOS::AuxNegativeMai,
                                  core::ExtendedPOS::AuxVolitional, core::ExtendedPOS::AuxClassicalNari});
}

/**
 * @brief Evidence found after a paradigm cell.
 *
 * Every ha-row kana is also a frequent modern particle (topic は, direction へ)
 * or a plain noun ending, so a cell is admitted only where what follows it
 * selects the classical paradigm.  A closed-class tail names the cell outright
 * and is stronger evidence than a bare clause boundary.
 */
struct HaRowLicense {
  bool licensed = false;
  bool closed_class_tail = false;
  core::ExtendedPOS cell = core::ExtendedPOS::Unknown;
};

HaRowLicense haRowCellLicense(core::ExtendedPOS cell, const std::vector<char32_t>& codepoints, size_t start_pos,
                              size_t end_pos, const dictionary::DictionaryManager* dict_manager) {
  HaRowLicense license;
  license.cell = cell;
  switch (cell) {
    case core::ExtendedPOS::VerbMizenkei:
      // 未然形 exists only under a cell that selects an irrealis: the classical
      // negatives (思は+ず), the conjectural む, the causative しむ, the passive
      // る, or the conditional particle ば (言は+ば). The cell kana is also the
      // topic particle, so nothing weaker may license it.
      // Those cells are frequent behind an ordinary nominal too (確認+は+する,
      // 彼+は+とも…), so the stem itself must name a verb. The row is the
      // historical-kana spelling of the modern ワ行五段 one, and that headword
      // is the form the dictionary carries.
      license.closed_class_tail =
          vh::isVerbInDictionary(dict_manager, extractSubstring(codepoints, start_pos, end_pos - 1) + "う") &&
          (dictionaryTailFollowsAt(codepoints, end_pos, dict_manager, core::PartOfSpeech::Auxiliary,
                                   {core::ExtendedPOS::AuxNegativeNu, core::ExtendedPOS::AuxVolitional,
                                    core::ExtendedPOS::AuxCausative, core::ExtendedPOS::AuxPassive}) ||
           dictionaryTailFollowsAt(codepoints, end_pos, dict_manager, core::PartOfSpeech::Particle,
                                   {core::ExtendedPOS::ParticleConj}));
      break;
    case core::ExtendedPOS::VerbRenyokei:
      // 連用形 heads a classical predicate chain or closes a clause.
      license.closed_class_tail =
          dictionaryTailFollowsAt(codepoints, end_pos, dict_manager, core::PartOfSpeech::Auxiliary,
                                  {core::ExtendedPOS::AuxClassicalKeri, core::ExtendedPOS::AuxClassicalPerfect,
                                   core::ExtendedPOS::AuxVolitional, core::ExtendedPOS::AuxDesireTai});
      license.licensed = clauseEndsAt(codepoints, end_pos);
      break;
    case core::ExtendedPOS::VerbShuushikei:
      license.licensed = shuushikeiEndsAt(codepoints, end_pos, dict_manager);
      license.closed_class_tail = license.licensed && !clauseEndsAt(codepoints, end_pos);
      break;
    case core::ExtendedPOS::VerbKateikei:
      // 已然形 stands before a concessive or conditional conjunction (思へ+ど,
      // 思へ+ば). The same form ends an imperative clause (書き給へ。), which is
      // the only other environment the row kana reaches without a following
      // closed-class word.
      license.closed_class_tail = dictionaryTailFollowsAt(
          codepoints, end_pos, dict_manager, core::PartOfSpeech::Particle, {core::ExtendedPOS::ParticleConj});
      if (!license.closed_class_tail && clauseEndsAt(codepoints, end_pos) &&
          predicateEndsAt(codepoints, start_pos, dict_manager)) {
        license.licensed = true;
        license.cell = core::ExtendedPOS::VerbMeireikei;
      }
      break;
    default:
      return license;
  }
  license.licensed = license.licensed || license.closed_class_tail;
  return license;
}

// Classical lower-bigrade verbs have the continuative/irrealis vowel へ
// (終へ+ぬ, 終へ+た).  The surface is otherwise indistinguishable from the
// direction particle, so emit it only when a following auxiliary names the
// predicate boundary.  The historical terminal ふ is retained as the lemma;
// the current public conjugation enum has no lower-bigrade row.
void appendClassicalShimoNidanCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                         size_t hiragana_end, const dictionary::DictionaryManager* dict_manager,
                                         std::vector<UnknownCandidate>& candidates) {
  if (kanji_end != start_pos + 1 || kanji_end >= hiragana_end || codepoints[kanji_end] != U'へ') {
    return;
  }
  const size_t end_pos = kanji_end + 1;
  const bool follows_auxiliary = dictionaryTailFollowsAt(
      codepoints, end_pos, dict_manager, core::PartOfSpeech::Auxiliary,
      {core::ExtendedPOS::AuxNegativeNu, core::ExtendedPOS::AuxTenseTa, core::ExtendedPOS::AuxClassicalKeri,
       core::ExtendedPOS::AuxClassicalPerfect, core::ExtendedPOS::AuxVolitional});
  if (!follows_auxiliary) {
    return;
  }
  const std::string surface = extractSubstring(codepoints, start_pos, end_pos);
  const std::string lemma = extractSubstring(codepoints, start_pos, kanji_end) + "ふ";
  candidates.push_back(makeVerbCandidate(surface, start_pos, end_pos, candidate::verb_cost::kClassicalHaRowLicensedCost,
                                         lemma, dictionary::ConjugationType::GodanWa, true, CandidateOrigin::VerbKanji,
                                         candidate::kNoConfidence, "classical_shimo_nidan",
                                         core::ExtendedPOS::VerbRenyokei));
}

// A predicate slot is opened by the argument in front of it: a case particle,
// or the genitive の, which marks the subject of a subordinate clause and is
// interchangeable with が there (影の見ゆる時 for 影が見ゆる時).
bool opensPredicateSlot(const std::vector<char32_t>& codepoints, size_t start_pos,
                        const dictionary::DictionaryManager* dict_manager) {
  if (vh::followsCaseParticle(dict_manager, codepoints, start_pos)) {
    return true;
  }
  if (start_pos == 0) {
    return false;
  }
  // A focus particle opens a 係り結び whose 結び is the attributive cell, so it
  // marks the same predicate slot the case particles do (これ+ぞ+求むる+物,
  // これ+なむ+求むる+道). Classical や is the same 係助詞 but is carried as the
  // modern coordinating particle, so the conjunctive class joins the probe: what
  // it proves is only that the position starts a word rather than sitting inside
  // one, which any particle boundary establishes. Its members run to two morae,
  // so probe back that far.
  constexpr size_t kFocusParticleChars = 2;
  const size_t scan_start = start_pos > kFocusParticleChars ? start_pos - kFocusParticleChars : 0;
  for (size_t particle_start = scan_start; dict_manager != nullptr && particle_start < start_pos; ++particle_start) {
    const auto* particle = dict_manager->lookupExact(extractSubstring(codepoints, particle_start, start_pos),
                                                     core::PartOfSpeech::Particle);
    if (particle != nullptr && (particle->extended_pos == core::ExtendedPOS::ParticleNo ||
                                particle->extended_pos == core::ExtendedPOS::ParticleBinding ||
                                particle->extended_pos == core::ExtendedPOS::ParticleTopic ||
                                particle->extended_pos == core::ExtendedPOS::ParticleConj ||
                                particle->extended_pos == core::ExtendedPOS::ParticleFinal)) {
      return true;
    }
  }
  // Classical Japanese drops the nominative marker as readily as it writes it,
  // so a bare nominal opens the same slot a case particle does (月老ゆる for
  // 月が老ゆる). Script tells that apart from a position inside a word: a
  // nominal ends on kanji, katakana or a numeral, while a hiragana neighbour
  // that is not one of the particles above is the okurigana of the word this
  // position sits in.
  return normalize::classifyChar(codepoints[start_pos - 1]) != normalize::CharType::Hiragana;
}

// A bigrade verb spells its 終止形 as the kanji stem plus the row's U-row kana
// and its 連体形 by adding る. Only ヤ行 needs a terminal candidate — every other
// row's kana also ends a Godan verb, so the conjugation table reaches 受く and
// 過ぐ on its own, while ゆ ends nothing in the modern paradigm and its mora
// falls out as an unknown fragment. The attributive needs one for every row:
// its trailing る otherwise reads as a separate auxiliary, or the whole span
// fabricates a Godan terminal that is its own lemma (流るる, 見ゆる). The syntax
// names both cells — a predicate slot opened by its own argument, closing a
// clause (山を|越ゆ) or standing in front of the nominal it modifies
// (影の|見ゆる|時) — and both keep the terminal as their lemma.
void appendClassicalNidanCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                    size_t hiragana_end, const dictionary::DictionaryManager* dict_manager,
                                    std::vector<UnknownCandidate>& candidates) {
  // The stem is one kanji, and it is the last one of the run: whatever precedes
  // it is the nominal argument that opens the predicate slot, which is the same
  // evidence a written case particle gives (木の|葉|落つる, 灯|消ゆる).
  if (kanji_end == start_pos || kanji_end >= hiragana_end) {
    return;
  }
  const size_t stem_start = kanji_end - 1;
  if (!opensPredicateSlot(codepoints, stem_start, dict_manager)) {
    return;
  }
  const char32_t terminal = codepoints[kanji_end];
  const bool is_attributive = kanji_end + 1 < hiragana_end && codepoints[kanji_end + 1] == core::hiragana::kRu;
  // The same kana spell classical auxiliaries that take a 未然形 or a 連用形
  // (見+つる, 見+ぬる). Behind a stem that is a verb on its own, the kana is that
  // auxiliary and not the row's ending.
  if (grammar::isClassicalAuxiliaryHomographKana(terminal) && vh::isSingleKanjiIchidan(codepoints[stem_start])) {
    return;
  }
  if (!grammar::isBigradeTerminalKana(terminal)) {
    return;
  }
  // The 連体形 needs a candidate on every row, since its trailing る otherwise
  // reads as a separate auxiliary. The 終止形 needs one only where the modern
  // paradigm cannot reach the form: the rows it kept are built by the
  // conjugation table on their own (受く, 過ぐ), and the ha row has its own
  // paradigm above, which leaves 越ゆ and 出づ.
  if (!is_attributive &&
      (grammar::isModernGodanTerminalKana(terminal) || classicalHaRowCell(terminal) != core::ExtendedPOS::Unknown ||
       !shuushikeiEndsAt(codepoints, kanji_end + 1, dict_manager))) {
    return;
  }
  const std::string lemma = extractSubstring(codepoints, stem_start, kanji_end + 1);
  const size_t end_pos = is_attributive ? kanji_end + 2 : kanji_end + 1;
  candidates.push_back(
      makeVerbCandidate(extractSubstring(codepoints, stem_start, end_pos), stem_start, end_pos,
                        candidate::verb_cost::kClassicalHaRowLicensedCost, lemma, dictionary::ConjugationType::Ichidan,
                        true, CandidateOrigin::VerbKanji, candidate::kNoConfidence, "classical_nidan_cell",
                        is_attributive ? core::ExtendedPOS::VerbRentaikei : core::ExtendedPOS::VerbShuushikei));
}

}  // namespace

void appendClassicalHaRowCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                    size_t hiragana_end, const dictionary::DictionaryManager* dict_manager,
                                    std::vector<UnknownCandidate>& candidates) {
  if (kanji_end == start_pos || kanji_end >= hiragana_end) {
    return;
  }
  const size_t scan_end = std::min(hiragana_end, kanji_end + kOkuriganaProbeChars + 1);
  for (size_t tail_pos = kanji_end; tail_pos < scan_end; ++tail_pos) {
    const core::ExtendedPOS cell = classicalHaRowCell(codepoints[tail_pos]);
    if (cell == core::ExtendedPOS::Unknown) {
      continue;
    }
    // は is both the topic particle and the first mora of the formal noun はず,
    // and both of those follow a word that is already complete (読む+はず,
    // 村の+はずれ). Only a bare kanji stem leaves the paradigm as the sole
    // reading, so the 未然形 cell takes no okurigana in front of it.
    if (cell == core::ExtendedPOS::VerbMizenkei && tail_pos != kanji_end) {
      continue;
    }
    const size_t end_pos = tail_pos + 1;
    const HaRowLicense license = haRowCellLicense(cell, codepoints, start_pos, end_pos, dict_manager);
    if (!license.licensed) {
      continue;
    }
    const std::string surface = extractSubstring(codepoints, start_pos, end_pos);
    const std::string lemma = extractSubstring(codepoints, start_pos, tail_pos) + "ふ";
    const float cost = license.closed_class_tail ? candidate::verb_cost::kClassicalHaRowLicensedCost
                                                 : candidate::verb_cost::kClassicalHaRowCost;
    auto candidate = makeVerbCandidate(
        surface, start_pos, end_pos, cost, lemma, grammar::verbTypeToConjType(grammar::VerbType::GodanWa), true,
        CandidateOrigin::VerbKanji, candidate::kNoConfidence, "classical_ha_row", license.cell);
    candidates.push_back(std::move(candidate));
    SUZUME_DEBUG_LOG_VERBOSE("[VERB_CAND] " << surface << " classical_ha_row lemma=" << lemma << "\n");
  }
  appendClassicalShimoNidanCandidates(codepoints, start_pos, kanji_end, hiragana_end, dict_manager, candidates);
  appendClassicalNidanCandidates(codepoints, start_pos, kanji_end, hiragana_end, dict_manager, candidates);
}

size_t appendKuNominalizationCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                        size_t hiragana_end, const dictionary::DictionaryManager* dict_manager,
                                        std::vector<UnknownCandidate>& candidates) {
  if (dict_manager == nullptr || kanji_end == start_pos || kanji_end + 1 >= hiragana_end ||
      codepoints[kanji_end + 1] != kKuNominalizer) {
    return start_pos;
  }
  // ク語法 names a predicate by attaching く to its 未然形, so the reading stands
  // or falls with the verb that cell belongs to. Reverse the cell through the
  // canonical paradigm table rather than a kana map of its own, and admit the
  // span only when the dictionary carries the base form it reconstructs: 言わ
  // reaches 言う and licenses 言わく, while the adjective stems that share the
  // shape (危な, 少な) reach no verb at all. The nominalizer takes no okurigana
  // in front of it for the same reason the 未然形 cell above does not — a longer
  // kana run belongs to a word that is already complete.
  const std::string stem = extractSubstring(codepoints, start_pos, kanji_end);
  const std::string cell_kana = extractSubstring(codepoints, kanji_end, kanji_end + 1);
  // The historical ハ行四段 irrealis fills the same cell as the modern ワ行五段
  // one it is the older spelling of, and the paradigm table carries only the
  // modern row. Reaching the same headword through it keeps the derivation
  // available in historical kana (言はく beside 言わく).
  const bool is_historical_ha_row_cell = codepoints[kanji_end] == U'は';
  for (const grammar::VerbEnding& ending : grammar::getVerbEndingsByForm(grammar::ConjForm::Mizenkei)) {
    if ((ending.suffix != cell_kana && !is_historical_ha_row_cell) || ending.base_suffix.empty()) {
      continue;
    }
    if (is_historical_ha_row_cell && ending.base_suffix != "う") {
      continue;
    }
    if (dict_manager->lookupExact(stem + ending.base_suffix, core::PartOfSpeech::Verb) == nullptr) {
      continue;
    }
    const size_t end_pos = kanji_end + 2;
    const std::string surface = extractSubstring(codepoints, start_pos, end_pos);
    // The span carries the nominalizing suffix, which is what keeps it out of
    // the over-long-unknown penalty: its length comes from a derivation the
    // dictionary licenses, not from a run of characters nothing accounts for.
    candidates.push_back(makeNounCandidate(surface, start_pos, end_pos, candidate::verb_cost::kKuNominalizationCost,
                                           true, CandidateOrigin::NominalizedNoun, core::ExtendedPOS::Noun,
                                           "ku_nominalization"));
    SUZUME_DEBUG_LOG_VERBOSE("[NOUN_CAND] " << surface << " ku_nominalization base=" << stem + ending.base_suffix
                                            << "\n");
    return end_pos;
  }
  return start_pos;
}

}  // namespace suzume::analysis::kanji_verb_detail
