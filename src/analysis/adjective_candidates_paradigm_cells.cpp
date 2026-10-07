/**
 * @file adjective_candidates_paradigm_cells.cpp
 * @brief Individual i-adjective paradigm cells shared by the kanji and hiragana generators
 */

#include <algorithm>
#include <string>
#include <vector>

#include "adjective_candidates.h"
#include "adjective_candidates_internal.h"
#include "analysis/candidate_constants.h"
#include "analysis/dictionary_probe.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "tokenizer_utils.h"
#include "unknown.h"
#include "verb_candidates_helpers.h"

namespace suzume::analysis {

using verb_helpers::isAdjectiveInDictionary;

using adj_detail::makeIAdjCellCandidate;

namespace {

// Append a strongly-preferred i-adjective paradigm cell with the shared origin and confidence.
void pushIAdjCell(std::vector<UnknownCandidate>& candidates, const std::string& surface, size_t start, size_t end,
                  const std::string& lemma, core::ExtendedPOS extended_pos, const char* pattern) {
  candidates.push_back(makeIAdjCellCandidate(surface, start, end, lemma, extended_pos,
                                             candidate::verb_cost::kStrongBonus, CandidateOrigin::AdjectiveI,
                                             candidate::kIAdjKaroConfidence, pattern));
}

bool isModernIAdjective(const std::string& lemma, const grammar::Inflection& inflection,
                        const dictionary::DictionaryManager* dict_manager) {
  if (isAdjectiveInDictionary(dict_manager, lemma)) {
    return true;
  }
  const float minimum_confidence =
      grammar::containsKanji(lemma) ? candidate::kCompoundAdjConfMin : candidate::kHiraAdjConfMin;
  return adj_detail::firstConfidenceAtLeast(inflection.analyze(lemma), grammar::VerbType::IAdjective,
                                            minimum_confidence) != candidate::kNoOriginConfidence;
}

// The modern base of a ku-paradigm stem that ends in け, when that base is a
// na-adjective spelling the same stem with か (静け -> 静か). Empty when no such
// adjective is registered, which is the ordinary case for a verb stem in け.
std::string classicalKuStemNaAdjectiveBase(const std::string& stem, const dictionary::DictionaryManager* dict_manager) {
  if (!utf8::endsWith(stem, "け")) {
    return {};
  }
  const std::string base = normalize::concat(utf8::dropLastChar(stem), "か");
  return isAdjectiveInDictionary(dict_manager, base) ? base : std::string{};
}

}  // namespace

void appendIAdjClassicalTerminalCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t scan_start,
                                           size_t scan_end, const dictionary::DictionaryManager* dict_manager,
                                           std::vector<UnknownCandidate>& candidates) {
  for (size_t shi_pos = scan_start; shi_pos < scan_end; ++shi_pos) {
    if (shi_pos <= start_pos || codepoints[shi_pos] != U'し') {
      continue;
    }
    // The terminal closes its clause. Any hiragana behind it continues some
    // other paradigm — the modern adjective's own (美し+かった), the sahen
    // predicate's (確認し+て) — and that reading owns the mora. A prolonged
    // sound mark is the same continuation spelled colloquially: しー is how
    // the modern ending しい is written when the vowel is drawn out, so the
    // mora belongs to that adjective (おいしーー is おいしい elongated, not a
    // literary terminal followed by emphasis).
    // A quotation or the copula is the exception: each takes a complete clause
    // or a quoted phrase, and the terminal is exactly that (暗し+という,
    // 暗し+だ, 高し+と).
    const auto takes_closed_terminal = [&](size_t pos) {
      if (codepoints[pos] == core::hiragana::kTo) {
        return true;
      }
      constexpr size_t kQuoteProbe = 4;
      if (dict_manager != nullptr) {
        const size_t probe_end = std::min(codepoints.size(), pos + kQuoteProbe);
        for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, pos, probe_end)) {
          if (match.entry != nullptr && (match.entry->extended_pos == core::ExtendedPOS::ParticleQuote ||
                                         match.entry->extended_pos == core::ExtendedPOS::DeterminerQuotative)) {
            return true;
          }
        }
      }
      return verb_helpers::auxiliaryFollowsAt(dict_manager, codepoints, pos,
                                              [](const dictionary::DictionaryEntry& entry) {
                                                return entry.extended_pos == core::ExtendedPOS::AuxCopulaDa ||
                                                       entry.extended_pos == core::ExtendedPOS::AuxCopulaDesu;
                                              });
    };
    if (shi_pos + 1 < codepoints.size() &&
        ((normalize::classifyChar(codepoints[shi_pos + 1]) == normalize::CharType::Hiragana &&
          !takes_closed_terminal(shi_pos + 1)) ||
         normalize::isProlongedSoundMark(codepoints[shi_pos + 1]))) {
      continue;
    }
    const std::string stem = extractSubstring(codepoints, start_pos, shi_pos);
    const std::string surface = extractSubstring(codepoints, start_pos, shi_pos + 1);
    // A registered su-row base makes the same spelling that row's continuative
    // (話し from 話す), which is the incomparably more frequent reading.
    if (verb_helpers::isVerbInDictionary(dict_manager, stem + "す") ||
        verb_helpers::hasNonVerbDictionaryEntry(dict_manager, surface)) {
      continue;
    }
    // The ku paradigm spells its terminal by adding し to the stem the modern
    // base keeps (高い -> 高し); the shiku paradigm already ends in that mora and
    // spells the terminal with the stem itself (欲しい -> 欲し). Only a dictionary
    // base licenses the reading: the analyzer endorses an i-adjective shape for
    // any run ending in し, which would fabricate one per kanji run.
    std::string lemma = surface + "い";
    if (!verb_helpers::isAdjectiveInDictionary(dict_manager, lemma)) {
      lemma = stem + "い";
      if (!verb_helpers::isAdjectiveInDictionary(dict_manager, lemma)) {
        // Part of the ku paradigm survives into the modern language as a
        // na-adjective rather than an i-adjective, and there the classical stem
        // replaces that adjective's final か with け (静か -> 静け+し, 明らか ->
        // 明らけ+し). Probing the か spelling recovers the base the surface no
        // longer carries; without it the terminal falls back on an invented
        // ichidan verb over the same stem.
        lemma = classicalKuStemNaAdjectiveBase(stem, dict_manager);
        if (lemma.empty()) {
          continue;
        }
      }
    }
    pushIAdjCell(candidates, surface, start_pos, shi_pos + 1, lemma, core::ExtendedPOS::AdjBasic,
                 "i_adjective_classical_shi");
  }
}

void appendIAdjOnbinRenyokeiCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t scan_start,
                                       size_t scan_end, const grammar::Inflection& inflection,
                                       const dictionary::DictionaryManager* dict_manager,
                                       std::vector<UnknownCandidate>& candidates) {
  // The polite continuative replaces the く of an i-adjective's renyokei with
  // う, and the mora in front of it carries the glide that vowel change
  // produces: an i-row kana grows its ゅ digraph (よろしく → よろしゅう) while a
  // kanji stem keeps its spelling and only loses the く (高く → 高う). The cell
  // is missing from the paradigm, so the run gets cut at the glide instead.
  //
  // Only those two shapes are admitted. A bare kana stem plus う would accept
  // any two morae whose first plus い happens to be an adjective, which is what
  // the formal noun よう spells.
  for (size_t u_pos = scan_start; u_pos < scan_end; ++u_pos) {
    if (codepoints[u_pos] != U'う' || u_pos <= start_pos) {
      continue;
    }
    size_t stem_end = u_pos;
    bool glide_shape = false;
    if (codepoints[u_pos - 1] == U'ゅ') {
      if (u_pos < start_pos + 2 || !kana::isIRowCodepoint(codepoints[u_pos - 2])) {
        continue;
      }
      stem_end = u_pos - 1;
      glide_shape = true;
    } else if (!normalize::isKanjiCodepoint(codepoints[u_pos - 1])) {
      continue;
    }
    if (stem_end <= start_pos) {
      continue;
    }
    const std::string lemma = extractSubstring(codepoints, start_pos, stem_end) + "い";
    // The glide is itself the evidence: nothing else produces し+ゅ+う. Without
    // it the shape is just a kanji stem plus う, which every wa-row Godan
    // terminal also spells (思う, 使う), so that side needs the dictionary.
    if (glide_shape ? !isModernIAdjective(lemma, inflection, dict_manager)
                    : !isAdjectiveInDictionary(dict_manager, lemma)) {
      continue;
    }
    pushIAdjCell(candidates, extractSubstring(codepoints, start_pos, u_pos + 1), start_pos, u_pos + 1, lemma,
                 core::ExtendedPOS::AdjRenyokei, "i_adjective_onbin_renyokei");
  }
}

void appendIAdjKaroCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t scan_start,
                              size_t scan_end, const grammar::Inflection& inflection,
                              const dictionary::DictionaryManager* dict_manager,
                              std::vector<UnknownCandidate>& candidates) {
  for (size_t karo_pos = scan_start; karo_pos + 1 < scan_end; ++karo_pos) {
    if (karo_pos <= start_pos) {
      continue;  // The stem before かろ must be non-empty
    }
    if (codepoints[karo_pos] != U'か' || codepoints[karo_pos + 1] != U'ろ') {
      continue;
    }
    // Require a following う (推量): Xかろ+う. Without う, Xかろ is far more likely
    // a verb form, so leave it to the verb candidate paths.
    if (karo_pos + 2 >= codepoints.size() || codepoints[karo_pos + 2] != U'う') {
      continue;
    }
    std::string lemma = extractSubstring(codepoints, start_pos, karo_pos) + "い";
    // ない is both the adjective 無い and the negative auxiliary; in the かろ form
    // (〜ではなかろうか) the auxiliary reading dominates, so leave なかろ to the
    // auxiliary path rather than tagging it Adjective.
    //
    // The same ambiguity survives one morpheme to the left: an irrealis stem
    // plus the auxiliary has exactly the shape of a lexical ない-adjective, and
    // the inflection analyzer scores 知らない like 少ない. Attestation is the
    // only thing that separates them, so a base ending in ない has to come from
    // the dictionary rather than from the analyzer's shape guess.
    if (lemma == "ない" || (utf8::endsWith(lemma, "ない") && !isAdjectiveInDictionary(dict_manager, lemma))) {
      continue;
    }
    if (adj_detail::hasDictionaryVerbAnalysis(analysesInRange(inflection, codepoints, start_pos, karo_pos + 3),
                                              dict_manager)) {
      continue;
    }
    // Decisive lexical signal: the reconstructed base is a dictionary adjective,
    // or the inflection analyzer recognizes it as an i-adjective. This rejects the
    // verb-volitional homograph (分かろう → 分か+い is not an adjective).
    if (!isModernIAdjective(lemma, inflection, dict_manager)) {
      continue;
    }
    // Verified adjective: make the 未然形 win over fake verb interpretations
    // (ichidan Xかる etc.), mirroring the ke-form handling. AdjMizenkei feeds the
    // AdjMizenkei→AuxVolitional bigram.
    pushIAdjCell(candidates, extractSubstring(codepoints, start_pos, karo_pos + 2), start_pos, karo_pos + 2, lemma,
                 core::ExtendedPOS::AdjMizenkei, "i_adjective_karo");
  }
}

namespace {

// The supplementary (カリ) conjugation follows the ラ変 pattern, so the kana
// after か identifies the cell by its vowel row. Anything else is not a カリ form.
core::ExtendedPOS classicalKariCell(char32_t after_ka) {
  switch (after_ka) {
    case U'ら':
      return core::ExtendedPOS::AdjMizenkei;
    case U'り':
      return core::ExtendedPOS::AdjRenyokei;
    // The attributive and terminal cells share one form for i-adjectives.
    // The paradigm has no 已然形 (the plain conjugation supplies けれ).
    case U'る':
      return core::ExtendedPOS::AdjBasic;
    // The 命令形 closes a clause exactly as the terminal cell does, so it shares
    // that ExtendedPOS. It needs a licensing environment of its own rather than
    // the classical auxiliary the other cells take, because かれ is
    // overwhelmingly a godan irrealis plus the passive れ (書か+れ).
    case U'れ':
      return core::ExtendedPOS::AdjBasic;
    default:
      return core::ExtendedPOS::Unknown;
  }
}

bool classicalConjunctiveFollowsAt(const std::vector<char32_t>& codepoints, size_t pos,
                                   const dictionary::DictionaryManager* dict_manager) {
  constexpr size_t kClassicalTailProbeChars = 3;
  return hasDictionaryEntryFrom(dict_manager, codepoints, pos, 1, kClassicalTailProbeChars,
                                core::PartOfSpeech::Particle, [](const dictionary::DictionaryEntry& entry) {
                                  // The optative/imperative cell can be followed by the quotative と.
                                  // Its dictionary entry is often labelled as a case particle even though
                                  // this construction is a clausal connective (高かれ+と願う).
                                  return entry.extended_pos == core::ExtendedPOS::ParticleConj ||
                                         entry.extended_pos == core::ExtendedPOS::ParticleQuote ||
                                         (entry.extended_pos == core::ExtendedPOS::ParticleCase &&
                                          grammar::isSingleHiragana(entry.surface, core::hiragana::kTo));
                                });
}

// The imperative cell also stands in the paired concessive, where two of them are
// juxtaposed with nothing in between (遅かれ早かれ, 多かれ少なかれ). Neither member
// carries the conjunctive particle that licenses the cell everywhere else: the
// first hands the clause straight to its partner and the second to the predicate,
// so what identifies the construction is the adjacency itself. Each member
// therefore looks for its partner on the side the other one is on. Both halves
// still have to pass the adjective evidence the cell needs on its own, which is
// what keeps an ordinary verb ending in the same two kana out (道が分かれ、).
bool pairedImperativeCellAdjacent(const std::vector<char32_t>& codepoints, size_t start_pos, size_t cell_end) {
  // The partner's stem is a word of its own, so a few codepoints reach its cell.
  constexpr size_t kPairedStemProbeChars = 4;
  if (start_pos >= 2 && codepoints[start_pos - 2] == U'か' && codepoints[start_pos - 1] == U'れ') {
    return true;
  }
  const size_t probe_end = std::min(codepoints.size(), cell_end + kPairedStemProbeChars);
  for (size_t pos = cell_end; pos + 1 < probe_end; ++pos) {
    if (codepoints[pos] == U'か' && codepoints[pos + 1] == U'れ') {
      return true;
    }
  }
  return false;
}

bool classicalClauseEndsAt(const std::vector<char32_t>& codepoints, size_t pos) {
  if (pos >= codepoints.size()) {
    return true;
  }
  switch (codepoints[pos]) {
    case U'。':
    case U'、':
    case U'！':
    case U'？':
    case U'」':
      return true;
    default:
      return false;
  }
}

// The supplementary conjugation exists only to carry the classical auxiliaries
// the plain paradigm cannot take, so require one to start at the given position.
// Gating on the classical auxiliary class rather than on auxiliaries in general
// keeps the colloquial homographs out (や, registered as a copula variant, must
// not license 最初から as an adjective). Probes the longest form so multi-kana
// auxiliaries (けり, べし) count alongside single-kana ones (ず, き).
bool classicalAuxiliaryFollowsAt(const std::vector<char32_t>& codepoints, size_t pos, size_t scan_end,
                                 const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || pos >= codepoints.size()) {
    return false;
  }
  // A spelling may carry more than one auxiliary registration, and the classical
  // one is not always the first: the counterfactual's せ shares its mora with the
  // causative. The question here is whether any auxiliary of the classical class
  // starts at this position, so the probe walks every entry the span opens rather
  // than the single one an exact lookup answers with.
  const size_t probe_end = std::min({codepoints.size(), scan_end + 1, pos + 3});
  for (const auto& result : lookupResultsInRange(*dict_manager, codepoints, pos, probe_end)) {
    const auto* entry = result.entry;
    if (entry == nullptr || entry->pos != core::PartOfSpeech::Auxiliary) {
      continue;
    }
    // The negative conjecture takes a terminal exactly as the affirmative one
    // does, so the cell it selects is the same (高かる+まじ next to 高かる+らむ).
    if (core::isClassicalAuxiliaryType(entry->extended_pos) ||
        entry->extended_pos == core::ExtendedPOS::AuxNegativeMai) {
      return true;
    }
  }
  return false;
}

}  // namespace

void appendIAdjKaraZuCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t scan_start,
                                size_t scan_end, const grammar::Inflection& inflection,
                                const dictionary::DictionaryManager* dict_manager,
                                std::vector<UnknownCandidate>& candidates) {
  for (size_t kara_pos = scan_start; kara_pos + 1 < scan_end; ++kara_pos) {
    if (kara_pos <= start_pos || codepoints[kara_pos] != U'か') {
      continue;
    }
    // The supplementary (カリ) conjugation of an i-adjective inflects on the ラ変
    // pattern: 未然 から, 連用 かり, 連体 かる, 已然/命令 かれ. Select the cell from
    // the row of the kana after か instead of enumerating the forms.
    const core::ExtendedPOS cell = classicalKariCell(codepoints[kara_pos + 1]);
    if (cell == core::ExtendedPOS::Unknown) {
      continue;
    }
    // Only a closed-class follower licenses the supplementary conjugation; it
    // exists precisely to carry auxiliaries the plain paradigm cannot
    // (大きから+ず, 高かり+けり, 冷たかる+べし). Without one, the same kana are an
    // ordinary noun or godan verb (明かり, 見つかる).
    //
    const size_t cell_end = kara_pos + 2;
    // The 已然/命令 cell is selected by a conjunctive particle, including the
    // concessive ど as well as the optative quotative と.  Either continuation
    // rules out the homographic passive; without a closed particle the cell is
    // not emitted. The continuative かり may also close a literary clause.
    const bool is_kare = codepoints[kara_pos + 1] == U'れ';
    const bool follows_conjunctive = is_kare && (classicalConjunctiveFollowsAt(codepoints, cell_end, dict_manager) ||
                                                 pairedImperativeCellAdjacent(codepoints, start_pos, cell_end));
    const bool terminal_renyokei = codepoints[kara_pos + 1] == U'り' && classicalClauseEndsAt(codepoints, cell_end);
    const bool licensed = follows_conjunctive || terminal_renyokei ||
                          (!is_kare && classicalAuxiliaryFollowsAt(codepoints, cell_end, scan_end, dict_manager));
    if (!licensed) {
      continue;
    }
    std::string lemma = extractSubstring(codepoints, start_pos, kara_pos) + "い";
    // A シク adjective carries its し in the stem the supplementary conjugation
    // attaches to, and the modern base keeps that mora only sometimes
    // (美し+から -> 美しい, 悪し+から -> 悪い). Fall back to the stem without it,
    // the same two-step probe the classical terminal uses.
    if (!isAdjectiveInDictionary(dict_manager, lemma) && codepoints[kara_pos - 1] == U'し' &&
        kara_pos - 1 > start_pos) {
      const std::string shiku_lemma = extractSubstring(codepoints, start_pos, kara_pos - 1) + "い";
      if (isAdjectiveInDictionary(dict_manager, shiku_lemma)) {
        lemma = shiku_lemma;
      }
    }
    // The かれ cell is also the passive auxiliary after a Godan-ka irrealis
    // (書か+れ+ども).  The inflection engine intentionally recognizes broad
    // i-adjective-shaped runs, which is not enough to distinguish that path.
    // A classical カリ reading therefore needs adjective evidence; genuine
    // bases such as 美しい、高い、多い are L2-backed while the passive remains a
    // regular productive verb chain. The open シク class cannot be listed
    // exhaustively, so a productively formed -しい terminal counts as the same
    // evidence: its し is the stem mora the supplementary conjugation attaches
    // to, which the passive's a-row irrealis can never supply.
    // The requirement holds for the other cells too, even though a classical
    // auxiliary already licenses them: accepting a merely well-formed base there
    // lets the run start one word early and coin a compound out of whatever
    // stands in front of it (山+高かり as 山高い, 言ふ+べかり as 言ふべい), which is
    // the compound-stem ambiguity the lexicon is what settles.
    if (!isAdjectiveInDictionary(dict_manager, lemma) &&
        !verb_helpers::isProductiveShiiAdjectiveTerminal(lemma, inflection)) {
      continue;
    }
    // A productive stem stays inside one word: a conjunctive て/で after a kana
    // closes a te-form clause, and what follows it starts the next predicate
    // (実行され+て+しかる+べき, not a coined 実行されてしい).
    if (!isAdjectiveInDictionary(dict_manager, lemma)) {
      bool embeds_te_form = false;
      for (size_t te_pos = start_pos + 1; te_pos < kara_pos && !embeds_te_form; ++te_pos) {
        embeds_te_form = (codepoints[te_pos] == core::hiragana::kTe || codepoints[te_pos] == U'で') &&
                         kana::isHiraganaCodepoint(codepoints[te_pos - 1]);
      }
      if (embeds_te_form) {
        continue;
      }
    }
    // A カリ form is also an ordinary godan-ra inflection plus a classical
    // auxiliary. A complete analysis whose reconstructed verb lemma is
    // dictionary-attested is stronger than the weak, generic i-adjective
    // hypothesis (分からぬ -> 分かる, not fictitious 分い).
    // The other cells need their auxiliary inside the probe for the verb
    // analysis to complete (分から+ぬ -> 分かる). The 命令形 is followed by a
    // particle instead, which no verb analysis spans, so the probe stops at the
    // cell itself — that is what still recognizes 分かれ as 分かれる before と.
    const size_t probe_end = is_kare ? cell_end : std::min(kara_pos + 3, scan_end);
    if (adj_detail::hasDictionaryVerbAnalysis(analysesInRange(inflection, codepoints, start_pos, probe_end),
                                              dict_manager)) {
      continue;
    }
    const std::string surface = extractSubstring(codepoints, start_pos, kara_pos + 2);
    if (dict_manager != nullptr && dict_manager->lookupExact(surface, core::PartOfSpeech::Auxiliary) != nullptr) {
      continue;
    }
    pushIAdjCell(candidates, surface, start_pos, kara_pos + 2, lemma, cell, "i_adjective_kari");
  }
}

}  // namespace suzume::analysis
