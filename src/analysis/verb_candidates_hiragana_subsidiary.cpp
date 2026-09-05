/**
 * @file verb_candidates_hiragana_subsidiary.cpp
 * @brief Context-gated hiragana subsidiary-verb candidates
 */

#include <utility>

#include "analysis/bigram_table.h"
#include "analysis/candidate_constants.h"
#include "analysis/tokenizer_utils.h"
#include "analysis/verb_candidates_helpers.h"
#include "analysis/verb_candidates_hiragana_internal.h"
#include "core/utf8_constants.h"
#include "normalize/exceptions.h"
#include "unknown.h"

namespace suzume::analysis::hiragana_verb_detail {
namespace vh = verb_helpers;

bool isClearTeFormBeforeSubsidiary(const std::vector<char32_t>& codepoints, size_t start_pos, bool allow_emphatic_mo) {
  if (start_pos == 0) {
    return false;
  }
  if (codepoints[start_pos - 1] == core::hiragana::kTe) {
    return true;
  }
  // Emphatic ても/でも keeps the same te-form attachment (思ってもみる,
  // 読んでもみる). Retain the voiced-onbin guard for で so an ordinary
  // case-particle sequence such as 外でもみる stays lexical.
  if (allow_emphatic_mo && codepoints[start_pos - 1] == U'も' && start_pos >= 2) {
    if (codepoints[start_pos - 2] == core::hiragana::kTe) {
      return true;
    }
    return start_pos >= 3 && codepoints[start_pos - 2] == U'で' &&
           (codepoints[start_pos - 3] == core::hiragana::kI || codepoints[start_pos - 3] == U'ん');
  }
  // A voiced te-form before みる comes from い/ん音便 (泳いでみる,
  // 読んでみる). Requiring the onbin keeps an ordinary case-particle で in
  // 外でみる from being reinterpreted as a te-form boundary.
  return start_pos >= 2 && codepoints[start_pos - 1] == U'で' &&
         (codepoints[start_pos - 2] == core::hiragana::kI || codepoints[start_pos - 2] == U'ん');
}

void appendContextualSubsidiaryCandidate(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                         std::string_view lemma, dictionary::ConjugationType conj_type,
                                         core::ExtendedPOS extended_pos, const char* pattern, float candidate_cost,
                                         std::vector<UnknownCandidate>& candidates, core::PartOfSpeech pos) {
  auto candidate = makeCandidate(codepoints, start_pos, end_pos, pos, candidate_cost, true,
                                 CandidateOrigin::VerbHiragana, extended_pos, pattern);
  candidate.lemma = lemma;
  candidate.conj_type = conj_type;
  candidates.push_back(std::move(candidate));
}

namespace {

bool grammaticalStemFollowerStartsAt(const std::vector<char32_t>& codepoints, size_t start_pos,
                                     const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || start_pos >= codepoints.size()) {
    return false;
  }
  const std::string remaining = extractClosedClassProbe(codepoints, start_pos);
  for (const auto& result : dict_manager->lookup(remaining, 0)) {
    if (result.entry == nullptr) {
      continue;
    }
    if (result.entry->pos == core::PartOfSpeech::Auxiliary ||
        result.entry->extended_pos == core::ExtendedPOS::ParticleConj) {
      return true;
    }
  }
  return false;
}

// The contextual Ichidan subsidiary forms share one shape: a continuative
// stem is allowed only before a grammatical follower, while its finite,
// conditional, imperative, and volitional forms retain the whole surface.
// Keeping this in the subsidiary owner centralizes the shared te-form grammar.
void appendContextualIchidanSubsidiaryForms(const std::vector<char32_t>& codepoints, size_t start_pos, size_t stem_end,
                                            std::string_view lemma, const char* pattern,
                                            const dictionary::DictionaryManager* dict_manager,
                                            std::vector<UnknownCandidate>& candidates) {
  if (stem_end >= codepoints.size()) {
    return;
  }

  // The Ichidan volitional is stem + よう, never the bare renyokei stem + よう.
  const bool is_volitional_stem = codepoints[stem_end] == core::hiragana::kYo;
  if (!is_volitional_stem && grammaticalStemFollowerStartsAt(codepoints, stem_end, dict_manager)) {
    appendContextualSubsidiaryCandidate(codepoints, start_pos, stem_end, lemma, dictionary::ConjugationType::Ichidan,
                                        core::ExtendedPOS::AuxAspectMiru, pattern, bigram_cost::kMinor, candidates);
  }

  const char32_t ending = codepoints[stem_end];
  if (ending == core::hiragana::kRu || ending == core::hiragana::kRe || ending == U'ろ' ||
      ending == core::hiragana::kYo) {
    appendContextualSubsidiaryCandidate(codepoints, start_pos, stem_end + 1, lemma,
                                        dictionary::ConjugationType::Ichidan, core::ExtendedPOS::AuxAspectMiru, pattern,
                                        bigram_cost::kMinor, candidates);
  }

  // Colloquial conditional: the Ichidan れば contracts to りゃ (みれば → みりゃ),
  // so the cell keeps the same paradigm and loses only the particle.
  if (ending == U'り' && stem_end + 1 < codepoints.size() && codepoints[stem_end + 1] == U'ゃ') {
    appendContextualSubsidiaryCandidate(codepoints, start_pos, stem_end + 2, lemma,
                                        dictionary::ConjugationType::Ichidan, core::ExtendedPOS::AuxAspectMiru, pattern,
                                        bigram_cost::kMinor, candidates);
  }
}

}  // namespace

// The directional subsidiary いく shares its いけ/いけれ spelling with the
// independent potential verb いける. After a clear te-form, however, negative
// conditional, and volitional continuations identify the subsidiary paradigm
// (読んでいけない, 読んでいければ, 読んでいこう). Keep the short forms
// contextual so standalone lexical uses retain their verb analysis.
void appendIkuAuxiliaryCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                  std::vector<UnknownCandidate>& candidates) {
  if (start_pos + 1 >= codepoints.size() || codepoints[start_pos] != core::hiragana::kI ||
      (codepoints[start_pos + 1] != U'け' && codepoints[start_pos + 1] != U'こ') ||
      !isClearTeFormBeforeSubsidiary(codepoints, start_pos, false)) {
    return;
  }

  if (start_pos + 3 < codepoints.size() && codepoints[start_pos + 2] == U'れ' && codepoints[start_pos + 3] == U'ば') {
    appendContextualSubsidiaryCandidate(codepoints, start_pos, start_pos + 3, "いける",
                                        dictionary::ConjugationType::GodanKa, core::ExtendedPOS::AuxAspectIku,
                                        "hiragana_iku_auxiliary", candidate::verb_cost::kStrongBonus, candidates);
    return;
  }

  if (start_pos + 2 < codepoints.size() && codepoints[start_pos + 1] == U'こ' && codepoints[start_pos + 2] == U'う') {
    appendContextualSubsidiaryCandidate(codepoints, start_pos, start_pos + 2, "いく",
                                        dictionary::ConjugationType::GodanKa, core::ExtendedPOS::AuxAspectIku,
                                        "hiragana_iku_auxiliary", candidate::verb_cost::kStrongBonus, candidates);
    return;
  }

  if (vh::naiNegativeFollowsAt(codepoints, start_pos + 2)) {
    appendContextualSubsidiaryCandidate(codepoints, start_pos, start_pos + 2, "いける",
                                        dictionary::ConjugationType::GodanKa, core::ExtendedPOS::AuxAspectIku,
                                        "hiragana_iku_auxiliary", candidate::verb_cost::kStrongBonus, candidates);
  }
}

// 授受の補助動詞 やる has the same irrealis and renyokei stems as the
// independent verb. A clear te-form boundary makes the benefactive reading
// productive before negative and desiderative auxiliaries.
void appendYaruBenefactiveCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                     std::vector<UnknownCandidate>& candidates) {
  if (start_pos + 1 >= codepoints.size() || codepoints[start_pos] != U'や' ||
      !isClearTeFormBeforeSubsidiary(codepoints, start_pos, false)) {
    return;
  }
  if (codepoints[start_pos + 1] == U'ら' && vh::naiNegativeFollowsAt(codepoints, start_pos + 2)) {
    appendContextualSubsidiaryCandidate(codepoints, start_pos, start_pos + 2, "やる",
                                        dictionary::ConjugationType::GodanRa, core::ExtendedPOS::AuxBenefactive,
                                        "hiragana_yaru_benefactive", candidate::verb_cost::kStrongBonus, candidates);
  }
  if (codepoints[start_pos + 1] != U'り') {
    return;
  }
  appendContextualSubsidiaryCandidate(codepoints, start_pos, start_pos + 2, "やる",
                                      dictionary::ConjugationType::GodanRa, core::ExtendedPOS::AuxBenefactive,
                                      "hiragana_yaru_benefactive", candidate::verb_cost::kStrongBonus, candidates);
}

// 試行の補助動詞 みる is a closed-class te-form attachment. Generate its
// one-stage conjugation forms only after a clear て/で boundary, rather than
// registering the highly ambiguous single-kana stem み unconditionally.
// The stem form is emitted only when a dictionary-backed auxiliary or
// conjunctive particle follows (み+ます/た/て/ない/たい/られ...). The remaining
// finite Ichidan forms share the same contextual gate.
void appendMiruAuxiliaryCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                   const dictionary::DictionaryManager* dict_manager,
                                   std::vector<UnknownCandidate>& candidates) {
  if (start_pos >= codepoints.size() || codepoints[start_pos] != U'み' ||
      !isClearTeFormBeforeSubsidiary(codepoints, start_pos, true)) {
    return;
  }

  appendContextualIchidanSubsidiaryForms(codepoints, start_pos, start_pos + 1, "みる", "hiragana_miru_auxiliary",
                                         dict_manager, candidates);
}

// 見せる is a closed subsidiary verb after a te-form (読んでみせる). Its
// two-kana stem is unambiguous only in that context, so generate the Ichidan
// paradigm there instead of registering a broad hiragana dictionary entry.
void appendMiseruAuxiliaryCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                     const dictionary::DictionaryManager* dict_manager,
                                     std::vector<UnknownCandidate>& candidates) {
  if (start_pos + 1 >= codepoints.size() || codepoints[start_pos] != U'み' || codepoints[start_pos + 1] != U'せ' ||
      !isClearTeFormBeforeSubsidiary(codepoints, start_pos, true)) {
    return;
  }
  appendContextualIchidanSubsidiaryForms(codepoints, start_pos, start_pos + 2, "みせる", "hiragana_miseru_auxiliary",
                                         dict_manager, candidates);
}

// The benefactive auxiliary あげる has the ichidan stem あげ before the
// potential/passive auxiliary (〜てあげられる). Emit it only after a clear
// te-form and only when the following token is grammatical, preserving the
// ordinary lexical verb reading elsewhere.
void appendAgeruBenefactiveCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                      const dictionary::DictionaryManager* dict_manager,
                                      std::vector<UnknownCandidate>& candidates) {
  if (start_pos + 2 >= codepoints.size() || codepoints[start_pos] != U'あ' || codepoints[start_pos + 1] != U'げ' ||
      codepoints[start_pos + 2] != U'ら' || !isClearTeFormBeforeSubsidiary(codepoints, start_pos, true)) {
    return;
  }
  if (!grammaticalStemFollowerStartsAt(codepoints, start_pos + 2, dict_manager)) {
    return;
  }
  appendContextualSubsidiaryCandidate(codepoints, start_pos, start_pos + 2, "あげる",
                                      dictionary::ConjugationType::Ichidan, core::ExtendedPOS::AuxBenefactive,
                                      "hiragana_ageru_benefactive", bigram_cost::kMinor, candidates,
                                      core::PartOfSpeech::Verb);
}

// 準備の補助動詞 おく is homographic with the lexical verb and its short
// conjugation forms are common word fragments. Emit the closed auxiliary
// paradigm only after a clear te-form boundary instead of registering it
// globally in L1.
void appendOkuAuxiliaryCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                  std::vector<UnknownCandidate>& candidates) {
  if (start_pos + 1 >= codepoints.size() || codepoints[start_pos] != U'お' ||
      !isClearTeFormBeforeSubsidiary(codepoints, start_pos, false)) {
    return;
  }

  // 五段カ行: 未然おか/おこ, 連用おき, 音便おい, 終止おく, 仮定・命令おけ.
  const char32_t ending = codepoints[start_pos + 1];
  if (ending != U'か' && ending != U'き' && ending != U'い' && ending != U'く' && ending != U'け' && ending != U'こ') {
    return;
  }
  // Only the irrealis stem can take the negative auxiliary. Score this
  // context-gated inflection locally so an unrelated おく contraction (どい)
  // cannot acquire the same preference across a particle boundary.
  float candidate_cost = bigram_cost::kMinor;
  if (ending == U'か' && vh::naiNegativeFollowsAt(codepoints, start_pos + 2)) {
    candidate_cost += bigram_cost::kDoubleVeryStrongBonus;
  }
  appendContextualSubsidiaryCandidate(codepoints, start_pos, start_pos + 2, "おく",
                                      dictionary::ConjugationType::GodanKa, core::ExtendedPOS::AuxAspectOku,
                                      "hiragana_oku_auxiliary", candidate_cost, candidates);
}

}  // namespace suzume::analysis::hiragana_verb_detail
