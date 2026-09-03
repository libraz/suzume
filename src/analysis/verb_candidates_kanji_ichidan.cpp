/**
 * @file verb_candidates_kanji_ichidan.cpp
 * @brief Rare and single-kanji ichidan candidate patterns
 */

#include <algorithm>
#include <cmath>

#include "analysis/bigram_table.h"
#include "analysis/candidate_constants.h"
#include "analysis/dictionary_probe.h"
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

void appendIchidanStemRareCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                     size_t hiragana_end, const grammar::Inflection& inflection,
                                     const dictionary::DictionaryManager* dict_manager,
                                     std::vector<UnknownCandidate>& candidates) {
  // Check if followed by られ+X pattern (られた, られる, られべき, られます, etc.)
  bool has_rare_suffix = false;
  size_t stem_end = 0;

  // Pattern 1: Kanji + Ichidan stem + られ+X (信じ+られべき,
  // 認め+られた, 知らせ+られた).  The stem can have more than one
  // okurigana character, so find the passive onset and validate the
  // constructed lemma below instead of assuming that the first kana is the
  // final E/I-row character.
  for (size_t suffix_start = kanji_end + 1; suffix_start + 1 < hiragana_end; ++suffix_start) {
    if (codepoints[suffix_start] != U'ら' || codepoints[suffix_start + 1] != U'れ') {
      continue;
    }
    const char32_t stem_last = codepoints[suffix_start - 1];
    if (grammar::isERowCodepoint(stem_last) || grammar::isIRowCodepoint(stem_last)) {
      has_rare_suffix = true;
      stem_end = suffix_start;
      break;
    }
  }

  // Pattern 2: Single kanji + られ+X (e.g., 見+られべき)
  // Only for known single-kanji Ichidan verbs
  if (!has_rare_suffix && kanji_end == start_pos + 1) {
    char32_t kanji_char = codepoints[start_pos];
    if (vh::isSingleKanjiIchidan(kanji_char)) {
      // Check for られ suffix right after the single kanji
      using namespace suzume::core::hiragana;
      if (kanji_end + 1 < codepoints.size() && codepoints[kanji_end] == kRa && codepoints[kanji_end + 1] == kRe) {
        has_rare_suffix = true;
        stem_end = kanji_end;
      }
    }
  }

  if (has_rare_suffix && stem_end > start_pos) {
    std::string surface = extractSubstring(codepoints, start_pos, stem_end);
    // Construct base form: stem + る (e.g., 信じ → 信じる, 見 → 見る)
    std::string base_form = surface + "る";

    // A multi-kana stem before られ can also be a Godan causative
    // (聞か+せ+られた). Require lexical evidence for that wider stem so the
    // productive causative analysis stays available, while lexical stems such
    // as 知らせ remain valid Ichidan candidates through their dictionary noun
    // or verb entry.
    const bool has_multiple_okurigana = stem_end > kanji_end + 1;
    const bool has_lexical_stem_evidence =
        vh::isVerbInDictionary(dict_manager, base_form) || vh::hasNonVerbDictionaryEntry(dict_manager, surface);

    // Verify the base form exists in dictionary or is valid Ichidan verb
    bool is_valid_verb = vh::isVerbInDictionary(dict_manager, base_form);
    if (!is_valid_verb) {
      // Check if inflection analyzer recognizes this as Ichidan verb
      // Use >= threshold to include edge cases like 信じる (conf=0.3)
      // Check ALL candidates, not just best, because godan/ichidan may have same confidence
      const auto& all_cands = inflection.analyze(base_form);
      for (const auto& cand : all_cands) {
        if (cand.verb_type == grammar::VerbType::Ichidan && cand.confidence >= 0.3F) {
          is_valid_verb = true;
          break;
        }
      }
    }

    if (is_valid_verb && (!has_multiple_okurigana || has_lexical_stem_evidence)) {
      // Negative cost to beat single-verb inflection path (which gets optimal_length -0.5 bonus)
      constexpr float kCost = candidate::verb_cost::kStandardBonus;
      SUZUME_DEBUG_VERBOSE_BLOCK {
        SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface << " ichidan_stem_rare lemma=" << base_form
                            << " cost=" << kCost << "\n";
      }
      candidates.push_back(makeVerbCandidate(surface, start_pos, stem_end, kCost, base_form,
                                             grammar::verbTypeToConjType(grammar::VerbType::Ichidan), true,
                                             CandidateOrigin::VerbKanji, 0.9F, "ichidan_stem_rare"));
    }
  }
}

// Generate single-kanji Ichidan verb candidates for auxiliary patterns
// E.g., 寝ます → 寝(VERB) + ます(AUX), 見ます → 見(VERB) + ます(AUX)
// Covers polite ます / negative ない, た/て, colloquial contractions,
// られる, volitional, and causative auxiliary attachments.
void appendSingleKanjiIchidanCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                        size_t hiragana_end, const dictionary::DictionaryManager* dict_manager,
                                        std::vector<UnknownCandidate>& candidates) {
  // A one-kanji Ichidan renyokei may be followed directly by a kanji-leading
  // closed-class auxiliary (見+損なう, 見+得る). Normal hiragana logic cannot
  // emit the stem here because the adjacent kanji extends the initial run.
  const size_t single_kanji_end = start_pos + 1;
  if (single_kanji_end < codepoints.size() && vh::isSingleKanjiIchidan(codepoints[start_pos]) &&
      normalize::isKanjiCodepoint(codepoints[single_kanji_end])) {
    constexpr size_t kMaxKanjiLeadingAuxLength = 4;
    const size_t max_end = std::min(codepoints.size(), single_kanji_end + kMaxKanjiLeadingAuxLength);
    for (size_t aux_end = single_kanji_end + 1; aux_end <= max_end; ++aux_end) {
      const auto* auxiliary_entry = lookupEntryInRange(*dict_manager, codepoints, single_kanji_end, aux_end);
      const bool is_closed_auxiliary =
          auxiliary_entry != nullptr && (auxiliary_entry->pos == core::PartOfSpeech::Auxiliary ||
                                         auxiliary_entry->extended_pos == core::ExtendedPOS::AuxInability ||
                                         auxiliary_entry->extended_pos == core::ExtendedPOS::AuxExcessive);
      if (!is_closed_auxiliary) {
        continue;
      }
      const std::string surface = extractSubstring(codepoints, start_pos, single_kanji_end);
      candidates.push_back(makeVerbCandidate(surface, start_pos, single_kanji_end, candidate::verb_cost::kStandardBonus,
                                             surface + "る", dictionary::ConjugationType::Ichidan, true,
                                             CandidateOrigin::VerbKanji, candidate::kHighOriginConfidence,
                                             "single_kanji_ichidan_kanji_aux", core::ExtendedPOS::VerbRenyokei));
      break;
    }
  }

  if (kanji_end == start_pos + 1 && hiragana_end > kanji_end) {
    char32_t kanji_char = codepoints[start_pos];

    // The literary volitional ん (a euphonic form of む) attaches to the
    // irrealis stem of 来る: 来んとする. Kanji 来 keeps the same written stem
    // across its irregular forms, so emit that stem explicitly rather than
    // treating 来ん as a spurious onbin form.
    if (kanji_char == U'来' && codepoints[kanji_end] == U'ん' && kanji_end + 1 < codepoints.size() &&
        codepoints[kanji_end + 1] == U'と') {
      const std::string surface = extractSubstring(codepoints, start_pos, kanji_end);
      candidates.push_back(makeVerbCandidate(surface, start_pos, kanji_end, candidate::verb_cost::kStandardBonus,
                                             "来る", dictionary::ConjugationType::Kuru, true,
                                             CandidateOrigin::VerbKanji, candidate::kHighOriginConfidence,
                                             "kuru_literary_volitional_n", core::ExtendedPOS::VerbMizenkei));
    }

    // The irregular mizenkei of 来る keeps the bare kanji before the classical
    // negative auxiliary, just as it does before ない. Resolve the cell from the
    // auxiliary inventory instead of naming one kana, so the whole paradigm is
    // covered at once (来+ず, 来+ぬ, 来+ざる, 来+ね).
    bool classical_negative_follows = false;
    {
      constexpr size_t kNegativeAuxProbe = 3;
      const size_t max_aux_end = std::min(codepoints.size(), kanji_end + kNegativeAuxProbe);
      for (size_t aux_end = kanji_end + 1; aux_end <= max_aux_end; ++aux_end) {
        const auto* negative_entry =
            lookupEntryInRange(*dict_manager, codepoints, kanji_end, aux_end, core::PartOfSpeech::Auxiliary);
        if (negative_entry != nullptr && negative_entry->extended_pos == core::ExtendedPOS::AuxNegativeNu) {
          classical_negative_follows = true;
          break;
        }
      }
    }
    if (kanji_char == U'来' && classical_negative_follows) {
      const std::string surface = extractSubstring(codepoints, start_pos, kanji_end);
      candidates.push_back(makeVerbCandidate(surface, start_pos, kanji_end, candidate::verb_cost::kStandardBonus,
                                             "来る", dictionary::ConjugationType::Kuru, true,
                                             CandidateOrigin::VerbKanji, candidate::kHighOriginConfidence,
                                             "kuru_classical_negative", core::ExtendedPOS::VerbMizenkei));
    }

    // The irregular irrealis stem of 来る is the bare kanji before every
    // ない-family form, including the conditional (来+なけれ+ば).
    if (kanji_char == U'来' && vh::naiConditionalFollowsAt(codepoints, kanji_end)) {
      const std::string surface = extractSubstring(codepoints, start_pos, kanji_end);
      candidates.push_back(
          makeVerbCandidate(surface, start_pos, kanji_end, candidate::verb_cost::kSingleKanjiNegativeConditionalBonus,
                            "来る", dictionary::ConjugationType::Kuru, true, CandidateOrigin::VerbKanji,
                            candidate::kHighOriginConfidence, "kuru_negative_nai", core::ExtendedPOS::VerbMizenkei));
    }

    // 来る is irregular rather than ichidan, but its modern volitional still
    // spells the y-row stem plus the separate auxiliary (来よ+う).
    if (kanji_char == U'来' && kanji_end + 1 < codepoints.size() && codepoints[kanji_end] == U'よ' &&
        codepoints[kanji_end + 1] == U'う') {
      const std::string surface = extractSubstring(codepoints, start_pos, kanji_end + 1);
      candidates.push_back(makeVerbCandidate(surface, start_pos, kanji_end + 1, candidate::verb_cost::kStrongBonus,
                                             "来る", dictionary::ConjugationType::Kuru, true,
                                             CandidateOrigin::VerbKanji, candidate::kHighOriginConfidence,
                                             "kuru_modern_volitional", core::ExtendedPOS::VerbMizenkei));
    }

    if (vh::isSingleKanjiIchidan(kanji_char)) {
      // Check if followed by a polite ます-family auxiliary or negative auxiliary.
      // The ます family (ます/まし(た)/ませ(ん)/ましょ(う)) attaches only to a verb
      // renyokei, so the bare-kanji ichidan renyokei reading is licensed here
      // (見ました → 見+まし+た). Godan-sa homographs are unaffected: their
      // renyokei inserts し before the auxiliary (出しました), so the kanji is
      // never immediately followed by ま in that reading.
      using namespace suzume::core::hiragana;
      char32_t h1 = codepoints[kanji_end];
      char32_t h2 = (kanji_end + 1 < codepoints.size()) ? codepoints[kanji_end + 1] : 0;
      bool is_polite_aux = vh::masuAuxFollowsAt(codepoints, kanji_end);
      // Modern ichidan volitional keeps the y-row stem and the auxiliary
      // boundary (見よ+う, 来よ+う).  The bare kanji is the renyokei/mizenkei
      // spelling in other cells, but emitting it here would let formal noun
      // よう absorb the stem.
      bool is_modern_volitional = h1 == U'よ' && h2 == U'う';
      // Negative auxiliary ない and its conjugations:
      // ない (終止/連体), なく (連用), なかっ (た接続), なけれ (仮定), なきゃ (口語縮約仮定)
      bool is_negative_aux = vh::naiNegativeFollowsAt(codepoints, kanji_end);
      bool is_negative_conditional = vh::naiConditionalFollowsAt(codepoints, kanji_end);
      // The classical negative auxiliary attaches to the ichidan mizenkei
      // (= bare stem): 見ぬ人, 見ざるを得ない → 見 + ざる, 見ずに → 見 + ずに.
      // Resolve the cell from the auxiliary inventory, exactly as the 来 branch
      // above does, so the whole paradigm is covered at once instead of a
      // hand-listed subset — the izenkei ね before ば (見+ね+ば) was missing.
      bool is_classical_negative_aux = classical_negative_follows;
      // The literary volitional ん is distinct from the contracted negative
      // when the quotative particle follows (見んとする, 寝んとする).
      bool is_literary_volitional_n = (h1 == U'ん' && h2 == kTo);
      // Classical む attaches to the same bare ichidan stem (見む, 寝む).
      bool is_classical_volitional_mu = (h1 == U'む');
      std::string following_hiragana = extractSubstring(codepoints, kanji_end, hiragana_end);
      // Classical desiderative まほしき also follows a bare ichidan renyokei.
      bool is_classical_desiderative = grammar::startsClassicalDesiderativeSequence(following_hiragana);
      // The classical past-conjectural auxiliary attaches to the same bare
      // continuative as the classical past does (見けむ, 寝けむ).
      bool is_classical_conjectural = grammar::startsClassicalConjecturalAuxiliary(following_hiragana);
      // The classical negative-conjectural auxiliary attaches to the bare
      // irrealis stem in its attributive form as well (見まじき姿).
      const auto* classical_negative_mai = dict_manager->lookupExact(following_hiragana, core::PartOfSpeech::Auxiliary);
      bool is_classical_negative_mai = classical_negative_mai != nullptr &&
                                       classical_negative_mai->extended_pos == core::ExtendedPOS::AuxNegativeMai;

      // A conjunctive particle licenses the bare renyokei of a one-kanji
      // ichidan verb just as て and ます do.  Resolve this from the particle
      // inventory rather than by surface so every closed-class conjunctive
      // continuation shares the same grammar gate.
      bool is_conjunctive_particle = false;
      bool is_classical_past_aux = false;
      constexpr size_t kMaxConjunctiveParticleLength = 4;
      const size_t max_particle_end = std::min(codepoints.size(), kanji_end + kMaxConjunctiveParticleLength);
      for (size_t particle_end = kanji_end + 1; particle_end <= max_particle_end; ++particle_end) {
        const std::string particle_surface = extractSubstring(codepoints, kanji_end, particle_end);
        const auto* particle = dict_manager->lookupExact(particle_surface, core::PartOfSpeech::Particle);
        if (particle != nullptr && particle->extended_pos == core::ExtendedPOS::ParticleConj) {
          is_conjunctive_particle = true;
        }
        const auto* auxiliary = dict_manager->lookupExact(particle_surface, core::PartOfSpeech::Auxiliary);
        // The perfect selects the same bare continuative that the past けり does
        // (見つ, 経つ). Two conditions keep it off the stems of ordinary words:
        // an i-row opening mora is excluded, because directly after a kanji it
        // spells the stem's own continuative okurigana (居り, 祭り) — exactly
        // where the izenkei-attaching perfect り would sit; and the auxiliary
        // must be able to end the clause where it does, or 見つける would open
        // with a perfect too.
        const bool continuative_perfect = auxiliary != nullptr &&
                                          auxiliary->extended_pos == core::ExtendedPOS::AuxClassicalPerfect &&
                                          !grammar::isIRowCodepoint(h1) &&
                                          vh::classicalPastEnvironmentFollows(*dict_manager, codepoints, particle_end,
                                                                              /*is_izenkei=*/false);
        if (continuative_perfect ||
            (auxiliary != nullptr && auxiliary->extended_pos == core::ExtendedPOS::AuxClassicalKeri)) {
          is_classical_past_aux = true;
        }
        if (is_conjunctive_particle && is_classical_past_aux) {
          break;
        }
      }

      if (is_polite_aux || is_negative_aux || is_classical_negative_aux || is_literary_volitional_n ||
          is_classical_volitional_mu || is_classical_desiderative || is_classical_negative_mai ||
          is_conjunctive_particle || is_classical_past_aux || is_classical_conjectural) {
        std::string surface = extractSubstring(codepoints, start_pos, kanji_end);
        // A one-kanji stem followed by して can instead be the continuative
        // form of a dictionary-confirmed Godan-sa verb. Keep that lexical
        // inflection ahead of an unsupported Ichidan plus suru analysis.
        const bool has_godan_sa_te_competitor =
            h1 == U'し' && h2 == kTe && dict_manager->lookupExact(surface + "す", core::PartOfSpeech::Verb) != nullptr;
        if (!has_godan_sa_te_competitor) {
          std::string base_form = surface + "る";
          const float candidate_cost = is_negative_conditional
                                           ? candidate::verb_cost::kSingleKanjiNegativeConditionalBonus
                                           : candidate::verb_cost::kStandardBonus;
          SUZUME_DEBUG_VERBOSE_BLOCK {
            SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface << " single_kanji_ichidan_polite lemma=" << base_form
                                << " cost=" << candidate_cost << "\n";
          }
          candidates.push_back(makeVerbCandidate(
              surface, start_pos, kanji_end, candidate_cost, base_form,
              grammar::verbTypeToConjType(grammar::VerbType::Ichidan), true, CandidateOrigin::VerbKanji,
              candidate::kHighOriginConfidence,
              (is_literary_volitional_n || is_classical_volitional_mu) ? "single_kanji_ichidan_literary_volitional"
              : is_classical_desiderative                              ? "single_kanji_ichidan_classical_desiderative"
                                                                       : "single_kanji_ichidan_polite",
              (is_negative_conditional || is_literary_volitional_n || is_classical_volitional_mu ||
               is_classical_negative_mai)
                  ? core::ExtendedPOS::VerbMizenkei
              : (is_conjunctive_particle || is_classical_past_aux || is_classical_conjectural)
                  ? core::ExtendedPOS::VerbRenyokei
                  : core::ExtendedPOS::Unknown));
        }
      }

      if (is_modern_volitional) {
        const std::string surface = extractSubstring(codepoints, start_pos, kanji_end + 1);
        const std::string base_form = extractSubstring(codepoints, start_pos, kanji_end) + "る";
        candidates.push_back(makeVerbCandidate(surface, start_pos, kanji_end + 1, candidate::verb_cost::kStrongBonus,
                                               base_form, dictionary::ConjugationType::Ichidan, true,
                                               CandidateOrigin::VerbKanji, candidate::kHighOriginConfidence,
                                               "single_kanji_ichidan_volitional", core::ExtendedPOS::VerbMizenkei));
      }

      // Also handle た and て patterns for single-kanji Ichidan verbs
      // E.g., 寝た → 寝(VERB) + た(AUX), 見て → 見(VERB) + て(PARTICLE)
      // MeCab splits these as: 寝+た, 見+て
      bool is_ta_aux = (h1 == kTa);
      bool is_te_particle = (h1 == kTe);
      if (is_ta_aux || is_te_particle) {
        std::string surface = extractSubstring(codepoints, start_pos, kanji_end);
        std::string base_form = surface + "る";
        constexpr float kCost = candidate::verb_cost::kStrongBonus;  // Strong bonus to beat unified dictionary entry
        SUZUME_DEBUG_VERBOSE_BLOCK {
          SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface << " single_kanji_ichidan_ta_te lemma=" << base_form
                              << " cost=" << kCost << "\n";
        }
        candidates.push_back(makeVerbCandidate(surface, start_pos, kanji_end, kCost, base_form,
                                               grammar::verbTypeToConjType(grammar::VerbType::Ichidan), true,
                                               CandidateOrigin::VerbKanji, 0.9F, "single_kanji_ichidan_ta_te"));
      }

      // Handle colloquial contraction patterns for single-kanji Ichidan verbs
      // MeCab splits these as: 見+とく, 見+ちゃう, etc.
      // と → とく, といた, といて (ておく contraction: 見とく = 見ておく)
      // ち → ちゃう, ちゃった (てしまう contraction: 見ちゃう = 見てしまう)
      // ど → どく, どいた (voiced ておく: only for godan onbin, but check anyway)
      bool is_toku_aux = (h1 == kTo);
      bool is_chau_aux = (h1 == kChi);
      if (is_toku_aux || is_chau_aux) {
        std::string surface = extractSubstring(codepoints, start_pos, kanji_end);
        std::string base_form = surface + "る";
        constexpr float kCost = candidate::verb_cost::kStrongBonus;  // Strong bonus to beat unified contraction entry
        SUZUME_DEBUG_VERBOSE_BLOCK {
          SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface << " single_kanji_ichidan_colloquial lemma=" << base_form
                              << " cost=" << kCost << "\n";
        }
        candidates.push_back(makeVerbCandidate(surface, start_pos, kanji_end, kCost, base_form,
                                               grammar::verbTypeToConjType(grammar::VerbType::Ichidan), true,
                                               CandidateOrigin::VerbKanji, 0.9F, "single_kanji_ichidan_colloquial"));
      }

      // The failure subsidiary そびれる attaches directly to the bare
      // renyokei of single-kanji ichidan verbs (見そびれる, 寝そびれる).
      // License that stem only for the closed auxiliary onset, so ordinary
      // noun-plus-hiragana sequences are unaffected.
      bool is_sobireru_aux =
          h1 == U'そ' && h2 == U'び' && kanji_end + 2 < codepoints.size() && codepoints[kanji_end + 2] == U'れ';
      if (is_sobireru_aux) {
        std::string surface = extractSubstring(codepoints, start_pos, kanji_end);
        std::string base_form = surface + "る";
        constexpr float kCost = candidate::verb_cost::kStrongBonus;
        candidates.push_back(makeVerbCandidate(
            surface, start_pos, kanji_end, kCost, base_form, grammar::verbTypeToConjType(grammar::VerbType::Ichidan),
            true, CandidateOrigin::VerbKanji, candidate::kHighOriginConfidence, "single_kanji_ichidan_sobireru"));
      }

      // Handle られる pattern for single-kanji Ichidan verbs
      // E.g., 見られる → 見(VERB) + られる(AUX), 寝られる → 寝(VERB) + られる(AUX)
      // MeCab splits these as: 見+られる (passive/potential form)
      // Note: For ichidan verbs, the passive/potential is られる (not れる)
      bool is_rareru_aux = (h1 == kRa && h2 == kRe);
      if (is_rareru_aux) {
        std::string surface = extractSubstring(codepoints, start_pos, kanji_end);
        std::string base_form = surface + "る";
        constexpr float kCost =
            candidate::verb_cost::kStrongBonus;  // Strong bonus to beat godan mizenkei interpretation
        SUZUME_DEBUG_VERBOSE_BLOCK {
          SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface << " single_kanji_ichidan_rareru lemma=" << base_form
                              << " cost=" << kCost << "\n";
        }
        candidates.push_back(makeVerbCandidate(surface, start_pos, kanji_end, kCost, base_form,
                                               grammar::verbTypeToConjType(grammar::VerbType::Ichidan), true,
                                               CandidateOrigin::VerbKanji, 0.9F, "single_kanji_ichidan_rareru"));
      }

      // Handle both volitional and literary-imperative forms for single-kanji
      // Ichidan verbs: 見よ+う and 見よ.
      bool has_yo_form = (h1 == kYo);
      if (has_yo_form) {
        const bool is_volitional = (h2 == kU);
        std::string surface = extractSubstring(codepoints, start_pos, kanji_end + 1);
        std::string base_form = extractSubstring(codepoints, start_pos, kanji_end) + "る";
        constexpr float kCost = candidate::verb_cost::kStrongBonus;  // Strong bonus to beat compound interpretation
        SUZUME_DEBUG_VERBOSE_BLOCK {
          SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface
                              << (is_volitional ? " single_kanji_ichidan_volitional lemma="
                                                : " single_kanji_ichidan_imperative lemma=")
                              << base_form << " cost=" << kCost << "\n";
        }
        candidates.push_back(makeVerbCandidate(
            surface, start_pos, kanji_end + 1, kCost, base_form,
            grammar::verbTypeToConjType(grammar::VerbType::Ichidan), true, CandidateOrigin::VerbKanji, 0.9F,
            is_volitional ? "single_kanji_ichidan_volitional" : "single_kanji_ichidan_imperative",
            is_volitional ? core::ExtendedPOS::VerbMizenkei : core::ExtendedPOS::VerbMeireikei));
      }

      // Handle causative させ/させる/させられ pattern for single-kanji Ichidan verbs
      // E.g., 見させる → 見(VERB mizenkei) + させる(AUX causative)
      //       見させられた → 見(VERB mizenkei) + させ + られ + た
      // MeCab splits these as: 見+させる (not 見さ+せる like godan-sa)
      // A Sino-Japanese verbal noun takes the same させ through する's own
      // irrealis (提出+さ+せる), and a kanji run with no boundary in front of it
      // is that noun. Carving its last character out as an Ichidan verb splits
      // the word and leaves 提 with no reading.
      bool is_saseru_aux = (h1 == kSa && h2 == kSe) && !vh::startsInsideKanjiRun(codepoints, start_pos);
      if (is_saseru_aux) {
        std::string surface = extractSubstring(codepoints, start_pos, kanji_end);
        std::string base_form = surface + "る";
        constexpr float kCost = candidate::verb_cost::kStrongBonus;  // Strong bonus to beat NOUN candidate
        SUZUME_DEBUG_VERBOSE_BLOCK {
          SUZUME_DEBUG_STREAM << "[VERB_CAND] " << surface << " single_kanji_ichidan_causative lemma=" << base_form
                              << " cost=" << kCost << "\n";
        }
        candidates.push_back(makeVerbCandidate(
            surface, start_pos, kanji_end, kCost, base_form, grammar::verbTypeToConjType(grammar::VerbType::Ichidan),
            true, CandidateOrigin::VerbKanji, 0.9F, "single_kanji_ichidan_causative", core::ExtendedPOS::VerbMizenkei));
      }
    }
  }
}

}  // namespace suzume::analysis::kanji_verb_detail
