#include "bigram_table_internal.h"

namespace suzume::analysis::bigram_rules {

using EPOS = core::ExtendedPOS;
namespace cost = bigram_cost;

void setParticleAndLexicalPenaltyCosts(BigramMatrix& table) {
  static constexpr BigramRule kRules[] = {
      // =========================================================================
      // Particle → Particle penalties (unnatural adjacent particle chains)
      // =========================================================================
      // These particle combinations never occur adjacent in valid Japanese.
      // Penalizing them helps hiragana words (はし, もも, かし) compete against
      // false particle-chain interpretations (は+し, も+も, か+し).

      // PART_係 → PART_接続 (は+し, も+て): topic particle directly followed by
      // conjunctive particle is grammatically invalid (need content between
      // them). Valued with its two siblings below rather than a step under
      // them: the three describe the same impossibility, and the lower value
      // let a kana noun be read as the chain instead (朝もや as 朝+も+や).
      {EPOS::ParticleTopic, EPOS::ParticleConj, cost::kVeryRare},

      // PART_係 → PART_格 (は+が, は+を, も+に): topic+case markers never stack
      // adjacent on the same phrase (は...が with content between is fine)
      {EPOS::ParticleTopic, EPOS::ParticleCase, cost::kRare},

      // PART_係 → PART_係 (は+も, も+は): double topic marking never adjacent
      {EPOS::ParticleTopic, EPOS::ParticleTopic, cost::kVeryRare},

      // PART_格 → PART_格 (が+を, を+に, に+で): case particles never stack
      {EPOS::ParticleCase, EPOS::ParticleCase, cost::kVeryRare},

      // Note: PART_格 → PART_係 (に+は, で+は, と+は) is valid Japanese,
      // so preserve the stacked-particle boundary. This also covers に+も,
      // whose focus-particle reading is productive before a predicate.
      {EPOS::ParticleCase, EPOS::ParticleTopic, cost::kVeryStrongBonus},

      // A suffix cannot take a case particle as its host.  This keeps a
      // dictionary suffix homograph from winning at the start of the next
      // phrase (時間+に+間+に, where the second 間 is an independent noun).
      {EPOS::ParticleCase, EPOS::Suffix, cost::kProhibitive},

      // PART_副 → PART_係 (だけ+は, まで+は, ばかり+は) stacks the same way. The
      // adverbial particle is at least two morae, so this cannot re-bond a
      // short conjunctive homograph the way the note below describes.
      {EPOS::ParticleAdverbial, EPOS::ParticleTopic, cost::kVeryStrongBonus},

      // Note: PART_接続 → PART_係 bonus is NOT set here because short particles
      // like て, し also have PART_接続 and would incorrectly bond with は, も.
      // Instead, compound particle (≥3 chars) + topic particle bonus is handled
      // in scorer.cpp with surface length check.

      // =========================================================================
      // Particle → Other penalties (prevents over-segmentation of hiragana words)
      // =========================================================================
      // Patterns like も+ちろん, と+にかく are not valid Japanese morphology.
      // Single-char particles followed by unknown hiragana are usually misanalyses.

      {EPOS::ParticleTopic, EPOS::Other, cost::kRare},
      {EPOS::ParticleCase, EPOS::Other, cost::kRare},
      {EPOS::ParticleFinal, EPOS::Other, cost::kRare},
      {EPOS::ParticleConj, EPOS::Other, cost::kUncommon},

      // A case particle marks an argument, so what follows it heads a phrase:
      // a tense auxiliary has nothing to attach to there. The pair only arises
      // when a kana run is cut at a mora that also spells the past auxiliary
      // (を+た+し+かめる for を+たしかめる).
      {EPOS::ParticleCase, EPOS::AuxTenseTa, cost::kSevere},

      // A sentence-final particle cannot serve as an attributive marker for
      // a following nominal.  In X+な+名詞, this blocks the homographic final
      // particle path and lets the copular attributive form compete instead.
      {EPOS::ParticleFinal, EPOS::Noun, cost::kProhibitive},
      {EPOS::ParticleFinal, EPOS::NounFormal, cost::kProhibitive},
      {EPOS::ParticleFinal, EPOS::NounProper, cost::kProhibitive},

      // An aspectual くる is introduced by a conjunctive te/de-form, never by
      // an adverbial particle.  Reject paths such as 手+ほど+き while keeping
      // lexical verbs after particles (これ+だけ+来た) as ordinary verbs.
      {EPOS::ParticleAdverbial, EPOS::AuxAspectKuru, cost::kProhibitive},

      // =========================================================================
      // Conjunction → auxiliary and predicate rules
      // =========================================================================
      // Conjunctions like でも/だって do not directly precede auxiliaries.
      // 彼女でもない is 彼女|で|も|ない, not 彼女|でも(CONJ)|ない.
      {EPOS::Conjunction, EPOS::AuxNegativeNai, cost::kVeryRare},
      {EPOS::Conjunction, EPOS::ParticleFinal, cost::kRare},
      {EPOS::Conjunction, EPOS::VerbShuushikei, cost::kStrongBonus},
      {EPOS::Conjunction, EPOS::VerbRenyokei, cost::kStrongBonus},
      {EPOS::Conjunction, EPOS::AdjBasic, cost::kStrongBonus},
      {EPOS::Conjunction, EPOS::AdjStem, cost::kStrongBonus},
      {EPOS::Conjunction, EPOS::AdjRenyokei, cost::kStrongBonus},
      {EPOS::Conjunction, EPOS::AdjNaAdj, cost::kStrongBonus},

      // =========================================================================
      // Interjection and adverbial lexical boundaries
      // =========================================================================
      {EPOS::Adverb, EPOS::Interjection, cost::kStrongBonus},
      {EPOS::Interjection, EPOS::AuxGozaru, cost::kStrongBonus},
      {EPOS::Interjection, EPOS::AuxCopulaDesu, cost::kDoubleVeryStrongBonus},
      {EPOS::Adverb, EPOS::ParticleTopic, cost::kMinorBonus},
      {EPOS::Adverb, EPOS::ParticleCase, cost::kStrongBonus},
      {EPOS::Adverb, EPOS::ParticleFinal, cost::kVeryStrongBonus},
      {EPOS::Adverb, EPOS::Noun, cost::kModerateBonus},
      {EPOS::Adverb, EPOS::AdjBasic, cost::kStrongBonus},
      {EPOS::Adverb, EPOS::AdjRenyokei, cost::kStrongBonus},
      {EPOS::Adverb, EPOS::AdjNaAdj, cost::kStrongBonus},
      {EPOS::Adverb, EPOS::AdjKatt, cost::kStrongBonus},
      {EPOS::Adverb, EPOS::AdjStem, cost::kVeryRare},
      {EPOS::Adverb, EPOS::VerbRenyokei, cost::kModerateBonus},
      {EPOS::Adverb, EPOS::VerbShuushikei, cost::kModerateBonus},
      {EPOS::Adverb, EPOS::VerbOnbinkei, cost::kModerateBonus},
      {EPOS::Adverb, EPOS::VerbTaForm, cost::kModerateBonus},
      {EPOS::Prefix, EPOS::Noun, cost::kStrongBonus},
      {EPOS::Prefix, EPOS::VerbRenyokei, cost::kStrongBonus},

      // The honorific prefixes are bound morphemes: they need a nominal or a
      // continuative host on their right, so no particle can follow one. Left
      // unstated, a stray prefix edge was cheap enough to head a phrase and
      // hand the rest of a native noun to the particle it spells word-
      // internally (お|と|なに|なる instead of おとな|に|なる).
      {EPOS::Prefix, EPOS::ParticleCase, cost::kAlmostNever},
      {EPOS::Prefix, EPOS::ParticleTopic, cost::kAlmostNever},
      {EPOS::Prefix, EPOS::ParticleFinal, cost::kAlmostNever},
      {EPOS::Prefix, EPOS::ParticleConj, cost::kAlmostNever},
      {EPOS::Prefix, EPOS::ParticleQuote, cost::kAlmostNever},
      {EPOS::Prefix, EPOS::ParticleAdverbial, cost::kAlmostNever},
      {EPOS::Prefix, EPOS::ParticleNo, cost::kAlmostNever},
      {EPOS::Prefix, EPOS::ParticleBinding, cost::kAlmostNever},

      // Particles do not introduce interjections within a running phrase.
      {EPOS::ParticleCase, EPOS::Interjection, cost::kAlmostNever},
      {EPOS::ParticleTopic, EPOS::Interjection, cost::kAlmostNever},
      {EPOS::ParticleNo, EPOS::Interjection, cost::kAlmostNever},
      {EPOS::ParticleAdverbial, EPOS::Interjection, cost::kAlmostNever},
      {EPOS::ParticleConj, EPOS::Interjection, cost::kAlmostNever},
      {EPOS::ParticleQuote, EPOS::Interjection, cost::kAlmostNever},
      {EPOS::ParticleFinal, EPOS::Interjection, cost::kAlmostNever},
  };
  applyRules(table, kRules, sizeof(kRules) / sizeof(kRules[0]));
}

}  // namespace suzume::analysis::bigram_rules
