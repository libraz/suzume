/**
 * @file verb_endings.cpp
 * @brief Verb ending patterns for reverse inflection analysis
 */

#include "verb_endings.h"

namespace suzume::grammar {

const std::vector<VerbEnding>& getVerbEndings() {
  static const std::vector<VerbEnding> kEndings = {
      // 五段カ行 (書く)
      {"い", "く", VerbType::GodanKa, conn::kVerbOnbinkei, true},
      {"っ", "く", VerbType::GodanKa, conn::kVerbOnbinkei, true},  // Irregular: いく only
      {"き", "く", VerbType::GodanKa, conn::kVerbRenyokei, false},
      {"か", "く", VerbType::GodanKa, conn::kVerbMizenkei, false},
      {"く", "く", VerbType::GodanKa, conn::kVerbBase, false},
      {"け", "く", VerbType::GodanKa, conn::kVerbPotential, false},  // Potential stem
      {"け", "く", VerbType::GodanKa, conn::kVerbKatei, false},      // Hypothetical stem
      {"け", "く", VerbType::GodanKa, conn::kVerbMeireikei, false},  // Imperative: 書け
      {"こ", "く", VerbType::GodanKa, conn::kVerbVolitional, false}, // Volitional stem

      // 五段ガ行 (泳ぐ)
      {"い", "ぐ", VerbType::GodanGa, conn::kVerbOnbinkei, true},
      {"ぎ", "ぐ", VerbType::GodanGa, conn::kVerbRenyokei, false},
      {"が", "ぐ", VerbType::GodanGa, conn::kVerbMizenkei, false},
      {"げ", "ぐ", VerbType::GodanGa, conn::kVerbPotential, false},  // Potential stem
      {"げ", "ぐ", VerbType::GodanGa, conn::kVerbKatei, false},      // Hypothetical stem
      {"げ", "ぐ", VerbType::GodanGa, conn::kVerbMeireikei, false},  // Imperative: 泳げ
      {"ご", "ぐ", VerbType::GodanGa, conn::kVerbVolitional, false}, // Volitional stem
      {"ぐ", "ぐ", VerbType::GodanGa, conn::kVerbBase, false},       // Base/dictionary form

      // 五段サ行 (話す) - no onbin
      {"し", "す", VerbType::GodanSa, conn::kVerbRenyokei, false},
      {"し", "す", VerbType::GodanSa, conn::kVerbOnbinkei, true},
      {"さ", "す", VerbType::GodanSa, conn::kVerbMizenkei, false},
      {"せ", "す", VerbType::GodanSa, conn::kVerbPotential, false},  // Potential stem
      {"せ", "す", VerbType::GodanSa, conn::kVerbKatei, false},      // Hypothetical stem
      {"せ", "す", VerbType::GodanSa, conn::kVerbMeireikei, false},  // Imperative: 話せ
      {"そ", "す", VerbType::GodanSa, conn::kVerbVolitional, false}, // Volitional stem
      {"す", "す", VerbType::GodanSa, conn::kVerbBase, false},       // Base/dictionary form

      // 五段ラ行 (取る) - most common っ-onbin, prioritized
      // Note: "れ" potential stem removed - conflicts with Ichidan stems (忘れる etc.)
      // Note: "れ" imperative also conflicts with Ichidan stems, handled via disambiguation
      {"っ", "る", VerbType::GodanRa, conn::kVerbOnbinkei, true},
      {"り", "る", VerbType::GodanRa, conn::kVerbRenyokei, false},
      {"ら", "る", VerbType::GodanRa, conn::kVerbMizenkei, false},
      {"れ", "る", VerbType::GodanRa, conn::kVerbKatei, false},      // Hypothetical stem
      {"れ", "る", VerbType::GodanRa, conn::kVerbMeireikei, false},  // Imperative: 取れ
      {"ろ", "る", VerbType::GodanRa, conn::kVerbVolitional, false}, // Volitional stem

      // 五段タ行 (持つ)
      {"っ", "つ", VerbType::GodanTa, conn::kVerbOnbinkei, true},
      {"ち", "つ", VerbType::GodanTa, conn::kVerbRenyokei, false},
      {"た", "つ", VerbType::GodanTa, conn::kVerbMizenkei, false},
      {"て", "つ", VerbType::GodanTa, conn::kVerbPotential, false},  // Potential stem
      {"て", "つ", VerbType::GodanTa, conn::kVerbKatei, false},      // Hypothetical stem
      {"て", "つ", VerbType::GodanTa, conn::kVerbMeireikei, false},  // Imperative: 持て
      {"と", "つ", VerbType::GodanTa, conn::kVerbVolitional, false}, // Volitional stem
      {"つ", "つ", VerbType::GodanTa, conn::kVerbBase, false},       // Base/dictionary form

      // 五段マ行 (読む) - most common ん-onbin, prioritized
      {"ん", "む", VerbType::GodanMa, conn::kVerbOnbinkei, true},
      {"み", "む", VerbType::GodanMa, conn::kVerbRenyokei, false},
      {"ま", "む", VerbType::GodanMa, conn::kVerbMizenkei, false},
      {"め", "む", VerbType::GodanMa, conn::kVerbPotential, false},  // Potential stem
      {"め", "む", VerbType::GodanMa, conn::kVerbKatei, false},      // Hypothetical stem
      {"め", "む", VerbType::GodanMa, conn::kVerbMeireikei, false},  // Imperative: 読め
      {"も", "む", VerbType::GodanMa, conn::kVerbVolitional, false}, // Volitional stem
      {"む", "む", VerbType::GodanMa, conn::kVerbBase, false},       // Base/dictionary form

      // 五段バ行 (遊ぶ)
      {"ん", "ぶ", VerbType::GodanBa, conn::kVerbOnbinkei, true},
      {"び", "ぶ", VerbType::GodanBa, conn::kVerbRenyokei, false},
      {"ば", "ぶ", VerbType::GodanBa, conn::kVerbMizenkei, false},
      {"べ", "ぶ", VerbType::GodanBa, conn::kVerbPotential, false},  // Potential stem
      {"べ", "ぶ", VerbType::GodanBa, conn::kVerbKatei, false},      // Hypothetical stem
      {"べ", "ぶ", VerbType::GodanBa, conn::kVerbMeireikei, false},  // Imperative: 遊べ
      {"ぼ", "ぶ", VerbType::GodanBa, conn::kVerbVolitional, false}, // Volitional stem
      {"ぶ", "ぶ", VerbType::GodanBa, conn::kVerbBase, false},       // Base/dictionary form

      // 五段ナ行 (死ぬ) - rare
      {"ん", "ぬ", VerbType::GodanNa, conn::kVerbOnbinkei, true},
      {"に", "ぬ", VerbType::GodanNa, conn::kVerbRenyokei, false},
      {"な", "ぬ", VerbType::GodanNa, conn::kVerbMizenkei, false},
      {"ね", "ぬ", VerbType::GodanNa, conn::kVerbPotential, false},  // Potential stem
      {"ね", "ぬ", VerbType::GodanNa, conn::kVerbKatei, false},      // Hypothetical stem
      {"ね", "ぬ", VerbType::GodanNa, conn::kVerbMeireikei, false},  // Imperative: 死ね
      {"の", "ぬ", VerbType::GodanNa, conn::kVerbVolitional, false}, // Volitional stem
      {"ぬ", "ぬ", VerbType::GodanNa, conn::kVerbBase, false},       // Base/dictionary form

      // 五段ワ行 (買う)
      {"っ", "う", VerbType::GodanWa, conn::kVerbOnbinkei, true},
      {"い", "う", VerbType::GodanWa, conn::kVerbRenyokei, false},
      {"わ", "う", VerbType::GodanWa, conn::kVerbMizenkei, false},
      {"え", "う", VerbType::GodanWa, conn::kVerbPotential, false},  // Potential stem
      {"え", "う", VerbType::GodanWa, conn::kVerbKatei, false},      // Hypothetical stem
      {"え", "う", VerbType::GodanWa, conn::kVerbMeireikei, false},  // Imperative: 買え
      {"お", "う", VerbType::GodanWa, conn::kVerbVolitional, false}, // Volitional stem
      {"う", "う", VerbType::GodanWa, conn::kVerbBase, false},       // Base/dictionary form

      // 一段 (食べる)
      {"", "る", VerbType::Ichidan, conn::kVerbOnbinkei, true},
      {"", "る", VerbType::Ichidan, conn::kVerbRenyokei, false},
      {"", "る", VerbType::Ichidan, conn::kVerbMizenkei, false},
      {"れ", "る", VerbType::Ichidan, conn::kVerbKatei, false},      // Hypothetical: 食べれ(ば)
      {"ろ", "る", VerbType::Ichidan, conn::kVerbMeireikei, false},  // Imperative: 食べろ
      {"よ", "る", VerbType::Ichidan, conn::kVerbVolitional, false}, // Volitional stem
      {"る", "る", VerbType::Ichidan, conn::kVerbBase, false},       // Base/dictionary form

      // サ変 (する)
      {"し", "する", VerbType::Suru, conn::kVerbOnbinkei, true},
      {"し", "する", VerbType::Suru, conn::kVerbRenyokei, false},
      {"し", "する", VerbType::Suru, conn::kVerbMizenkei, false},  // しない
      {"さ", "する", VerbType::Suru, conn::kVerbMizenkei, false},  // させる/される
      // Empty suffix for suru-verb + passive/causative (開催+された → 開催する)
      // The さ is included in auxiliary patterns like された, させた
      {"", "する", VerbType::Suru, conn::kVerbMizenkei, false},
      // Empty suffix for suru-verb + してる/してた contraction
      // (勉強+してる → 勉強する, 勉強+してた → 勉強する)
      {"", "する", VerbType::Suru, conn::kVerbOnbinkei, true},
      {"すれ", "する", VerbType::Suru, conn::kVerbKatei, false},   // すれば
      {"しろ", "する", VerbType::Suru, conn::kVerbMeireikei, false}, // Imperative: しろ
      {"せよ", "する", VerbType::Suru, conn::kVerbMeireikei, false}, // Imperative (classical): せよ
      {"しよ", "する", VerbType::Suru, conn::kVerbVolitional, false}, // しよう
      {"する", "する", VerbType::Suru, conn::kVerbBase, false},    // Base/dictionary form
      {"す", "する", VerbType::Suru, conn::kVerbBase, false},      // すべき special

      // カ変 (来る)
      {"き", "くる", VerbType::Kuru, conn::kVerbOnbinkei, true},
      {"き", "くる", VerbType::Kuru, conn::kVerbRenyokei, false},
      {"こ", "くる", VerbType::Kuru, conn::kVerbMizenkei, false},
      {"くれ", "くる", VerbType::Kuru, conn::kVerbKatei, false},     // くれば
      {"こい", "くる", VerbType::Kuru, conn::kVerbMeireikei, false}, // Imperative: こい
      {"こよ", "くる", VerbType::Kuru, conn::kVerbVolitional, false}, // こよう
      {"くる", "くる", VerbType::Kuru, conn::kVerbBase, false},    // Base/dictionary form

      // い形容詞 (美しい)
      {"", "い", VerbType::IAdjective, conn::kIAdjStem, false},
  };
  return kEndings;
}

}  // namespace suzume::grammar
