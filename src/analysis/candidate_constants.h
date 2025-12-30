#ifndef SUZUME_ANALYSIS_CANDIDATE_CONSTANTS_H_
#define SUZUME_ANALYSIS_CANDIDATE_CONSTANTS_H_

// =============================================================================
// Candidate Generation Constants
// =============================================================================
// This file centralizes all penalty and bonus values used in candidate
// generation (join_candidates.cpp and split_candidates.cpp).
//
// Naming convention:
//   kPenalty* - increases cost (discourages pattern)
//   kBonus*   - decreases cost (encourages pattern, note: negative values)
//
// These constants determine which candidate splits/joins are preferred
// during lattice construction, before Viterbi path selection.
// =============================================================================

namespace suzume::analysis::candidate {

// =============================================================================
// Join Candidate Constants (join_candidates.cpp)
// =============================================================================

// Compound verb bonus (連用形 + 補助動詞)
// E.g., 読み+終わる, 書き+始める
constexpr float kCompoundVerbBonus = -0.8F;

// Verified Ichidan verb bonus
// Applied when join creates a valid ichidan verb pattern
constexpr float kVerifiedV1Bonus = -0.3F;

// Verified noun in compound bonus
// Applied when noun component is verified in dictionary
constexpr float kVerifiedNounBonus = -0.3F;

// Te-form + auxiliary bonus
// E.g., 食べて+いる, 走って+しまう
constexpr float kTeFormAuxBonus = -0.8F;

// =============================================================================
// Split Candidate Constants (split_candidates.cpp)
// =============================================================================

// Alpha + Kanji split bonus
// E.g., Web開発, AI研究
constexpr float kAlphaKanjiBonus = -0.3F;

// Alpha + Katakana split bonus
// E.g., APIリクエスト
constexpr float kAlphaKatakanaBonus = -0.3F;

// Digit + Kanji split bonuses
// E.g., 5分, 3月, 100人
constexpr float kDigitKanji1Bonus = -0.2F;  // 1-kanji counter
constexpr float kDigitKanji2Bonus = -0.2F;  // 2-kanji counter
constexpr float kDigitKanji3Penalty = 0.5F; // 3+ kanji (rare, likely wrong)

// Dictionary word split bonus
// Applied when split creates a dictionary-verified word
constexpr float kDictSplitBonus = -0.5F;

// Base cost for split candidates
// Added to all split candidates as baseline cost
constexpr float kSplitBaseCost = 1.0F;

// Noun + Verb split bonus
// E.g., 勉強+する, 説明+する
constexpr float kNounVerbSplitBonus = -1.0F;

// Verified verb in split bonus
// Applied when verb component is verified in dictionary
constexpr float kVerifiedVerbBonus = -0.8F;

}  // namespace suzume::analysis::candidate

#endif  // SUZUME_ANALYSIS_CANDIDATE_CONSTANTS_H_
