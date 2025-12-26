/**
 * @file auxiliaries.cpp
 * @brief Auxiliary verb entries for inflection analysis
 */

#include "auxiliaries.h"

#include <algorithm>

#include "connection.h"

namespace suzume::grammar {

namespace {

std::vector<AuxiliaryEntry> initAuxiliaries() {
  using namespace conn;

  std::vector<AuxiliaryEntry> auxiliaries = {
      // === Polite (ます系) ===
      {"ます", "ます", kAuxMasu, kAuxOutMasu, kVerbRenyokei},
      {"ました", "ます", kAuxMasu, kAuxOutTa, kVerbRenyokei},
      {"ません", "ます", kAuxMasu, kAuxOutBase, kVerbRenyokei},
      {"ましょう", "ます", kAuxMasu, kAuxOutBase, kVerbRenyokei},
      {"ませんでした", "ます", kAuxMasu, kAuxOutTa, kVerbRenyokei},

      // === Past (た系) ===
      {"た", "た", kAuxTa, kAuxOutTa, kVerbOnbinkei},
      {"だ", "た", kAuxTa, kAuxOutTa, kVerbOnbinkei},

      // === Te-form (て系) ===
      {"て", "て", kAuxTe, kAuxOutTe, kVerbOnbinkei},
      {"で", "て", kAuxTe, kAuxOutTe, kVerbOnbinkei},

      // === Progressive (ている系) ===
      {"いる", "いる", kAuxTeiru, kAuxOutBase, kAuxOutTe},
      {"いた", "いる", kAuxTeiru, kAuxOutTa, kAuxOutTe},
      {"います", "いる", kAuxTeiru, kAuxOutMasu, kAuxOutTe},
      {"いました", "いる", kAuxTeiru, kAuxOutTa, kAuxOutTe},
      {"いて", "いる", kAuxTeiru, kAuxOutTe, kAuxOutTe},

      // === Completion (てしまう系) ===
      {"しまう", "しまう", kAuxTeshimau, kAuxOutBase, kAuxOutTe},
      {"しまった", "しまう", kAuxTeshimau, kAuxOutTa, kAuxOutTe},
      {"しまって", "しまう", kAuxTeshimau, kAuxOutTe, kAuxOutTe},
      {"しまいます", "しまう", kAuxTeshimau, kAuxOutMasu, kAuxOutTe},
      {"ちゃう", "しまう", kAuxTeshimau, kAuxOutBase, kAuxOutTe},
      {"ちゃった", "しまう", kAuxTeshimau, kAuxOutTa, kAuxOutTe},
      {"ちゃって", "しまう", kAuxTeshimau, kAuxOutTe, kAuxOutTe},
      {"じゃう", "しまう", kAuxTeshimau, kAuxOutBase, kAuxOutTe},
      {"じゃった", "しまう", kAuxTeshimau, kAuxOutTa, kAuxOutTe},
      {"じゃって", "しまう", kAuxTeshimau, kAuxOutTe, kAuxOutTe},
      // Colloquial direct connection (contraction skips て/で)
      {"ちゃう", "しまう", kAuxTeshimau, kAuxOutBase, kVerbOnbinkei},
      {"ちゃった", "しまう", kAuxTeshimau, kAuxOutTa, kVerbOnbinkei},
      {"ちゃって", "しまう", kAuxTeshimau, kAuxOutTe, kVerbOnbinkei},
      {"じゃう", "しまう", kAuxTeshimau, kAuxOutBase, kVerbOnbinkei},
      {"じゃった", "しまう", kAuxTeshimau, kAuxOutTa, kVerbOnbinkei},
      {"じゃって", "しまう", kAuxTeshimau, kAuxOutTe, kVerbOnbinkei},

      // === Preparation (ておく系) ===
      {"おく", "おく", kAuxTeoku, kAuxOutBase, kAuxOutTe},
      {"おいた", "おく", kAuxTeoku, kAuxOutTa, kAuxOutTe},
      {"おいて", "おく", kAuxTeoku, kAuxOutTe, kAuxOutTe},
      {"おきます", "おく", kAuxTeoku, kAuxOutMasu, kAuxOutTe},
      {"とく", "おく", kAuxTeoku, kAuxOutBase, kAuxOutTe},
      {"といた", "おく", kAuxTeoku, kAuxOutTa, kAuxOutTe},

      // === Direction (てくる/ていく系) ===
      {"くる", "くる", kAuxTekuru, kAuxOutBase, kAuxOutTe},
      {"きた", "くる", kAuxTekuru, kAuxOutTa, kAuxOutTe},
      {"きて", "くる", kAuxTekuru, kAuxOutTe, kAuxOutTe},
      {"きます", "くる", kAuxTekuru, kAuxOutMasu, kAuxOutTe},
      {"いく", "いく", kAuxTeiku, kAuxOutBase, kAuxOutTe},
      {"いった", "いく", kAuxTeiku, kAuxOutTa, kAuxOutTe},
      {"いって", "いく", kAuxTeiku, kAuxOutTe, kAuxOutTe},

      // === Attempt (てみる系) ===
      {"みる", "みる", kAuxTemiru, kAuxOutBase, kAuxOutTe},
      {"みた", "みる", kAuxTemiru, kAuxOutTa, kAuxOutTe},
      {"みて", "みる", kAuxTemiru, kAuxOutTe, kAuxOutTe},
      {"みます", "みる", kAuxTemiru, kAuxOutMasu, kAuxOutTe},

      // === Benefactive (てもらう/てくれる/てあげる系) ===
      {"もらう", "もらう", kAuxTemorau, kAuxOutBase, kAuxOutTe},
      {"もらった", "もらう", kAuxTemorau, kAuxOutTa, kAuxOutTe},
      {"もらいます", "もらう", kAuxTemorau, kAuxOutMasu, kAuxOutTe},
      {"もらって", "もらう", kAuxTemorau, kAuxOutTe, kAuxOutTe},
      {"くれる", "くれる", kAuxTekureru, kAuxOutBase, kAuxOutTe},
      {"くれた", "くれる", kAuxTekureru, kAuxOutTa, kAuxOutTe},
      {"くれます", "くれる", kAuxTekureru, kAuxOutMasu, kAuxOutTe},
      {"あげる", "あげる", kAuxTeageru, kAuxOutBase, kAuxOutTe},
      {"あげた", "あげる", kAuxTeageru, kAuxOutTa, kAuxOutTe},
      {"あげます", "あげる", kAuxTeageru, kAuxOutMasu, kAuxOutTe},

      // === Negation (ない系) ===
      {"ない", "ない", kAuxNai, kAuxOutBase, kVerbMizenkei},
      {"なかった", "ない", kAuxNai, kAuxOutTa, kVerbMizenkei},
      {"なくて", "ない", kAuxNai, kAuxOutTe, kVerbMizenkei},
      {"なければ", "ない", kAuxNai, kAuxOutBase, kVerbMizenkei},  // negative conditional

      // === Desire (たい系) ===
      {"たい", "たい", kAuxTai, kAuxOutBase, kVerbRenyokei},
      {"たかった", "たい", kAuxTai, kAuxOutTa, kVerbRenyokei},
      {"たくない", "たい", kAuxTai, kAuxOutBase, kVerbRenyokei},
      {"たくなかった", "たい", kAuxTai, kAuxOutTa, kVerbRenyokei},
      {"たくて", "たい", kAuxTai, kAuxOutTe, kVerbRenyokei},

      // === Passive/Potential (れる/られる系) ===
      {"れる", "れる", kAuxReru, kAuxOutBase, kVerbMizenkei},
      {"れた", "れる", kAuxReru, kAuxOutTa, kVerbMizenkei},
      {"れて", "れる", kAuxReru, kAuxOutTe, kVerbMizenkei},
      {"れます", "れる", kAuxReru, kAuxOutMasu, kVerbMizenkei},
      {"れない", "れる", kAuxReru, kAuxOutBase, kVerbMizenkei},
      {"れなかった", "れる", kAuxReru, kAuxOutTa, kVerbMizenkei},
      {"られる", "られる", kAuxReru, kAuxOutBase, kVerbMizenkei},
      {"られた", "られる", kAuxReru, kAuxOutTa, kVerbMizenkei},
      {"られて", "られる", kAuxReru, kAuxOutTe, kVerbMizenkei},
      {"られます", "られる", kAuxReru, kAuxOutMasu, kVerbMizenkei},
      {"られない", "られる", kAuxReru, kAuxOutBase, kVerbMizenkei},
      {"られなかった", "られる", kAuxReru, kAuxOutTa, kVerbMizenkei},
      {"られなくて", "られる", kAuxReru, kAuxOutTe, kVerbMizenkei},
      {"れなくて", "れる", kAuxReru, kAuxOutTe, kVerbMizenkei},
      // Passive + なくなる (stop being done) - Godan verbs (読まれなくなった)
      {"れなくなる", "れる", kAuxReru, kAuxOutBase, kVerbMizenkei},
      {"れなくなった", "れる", kAuxReru, kAuxOutTa, kVerbMizenkei},
      {"れなくなって", "れる", kAuxReru, kAuxOutTe, kVerbMizenkei},
      // Passive + なくなる - Ichidan verbs (食べられなくなった) - already added in potential section

      // === Causative (せる/させる系) ===
      {"せる", "せる", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"せた", "せる", kAuxSeru, kAuxOutTa, kVerbMizenkei},
      {"せて", "せる", kAuxSeru, kAuxOutTe, kVerbMizenkei},
      {"せない", "せる", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"せなかった", "せる", kAuxSeru, kAuxOutTa, kVerbMizenkei},
      {"せなくて", "せる", kAuxSeru, kAuxOutTe, kVerbMizenkei},
      {"せます", "せる", kAuxSeru, kAuxOutMasu, kVerbMizenkei},
      {"させる", "させる", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"させた", "させる", kAuxSeru, kAuxOutTa, kVerbMizenkei},
      {"させて", "させる", kAuxSeru, kAuxOutTe, kVerbMizenkei},
      {"させない", "させる", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"させなかった", "させる", kAuxSeru, kAuxOutTa, kVerbMizenkei},
      {"させなくて", "させる", kAuxSeru, kAuxOutTe, kVerbMizenkei},
      {"させます", "させる", kAuxSeru, kAuxOutMasu, kVerbMizenkei},

      // === Causative-passive (させられる系) - for Ichidan ===
      {"させられる", "させられる", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"させられた", "させられる", kAuxSeru, kAuxOutTa, kVerbMizenkei},
      {"させられて", "させられる", kAuxSeru, kAuxOutTe, kVerbMizenkei},
      {"させられない", "させられる", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"させられなくて", "させられる", kAuxSeru, kAuxOutTe, kVerbMizenkei},
      {"させられます", "させられる", kAuxSeru, kAuxOutMasu, kVerbMizenkei},
      // Causative-passive + なくなる (stop being made to do something)
      {"させられなくなる", "させられる", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"させられなくなった", "させられる", kAuxSeru, kAuxOutTa, kVerbMizenkei},
      {"させられなくなって", "させられる", kAuxSeru, kAuxOutTe, kVerbMizenkei},
      // Causative-passive + たい (want to be made to do)
      {"させられたい", "させられる", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"させられたかった", "させられる", kAuxSeru, kAuxOutTa, kVerbMizenkei},
      {"させられたくない", "させられる", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"させられたくなかった", "させられる", kAuxSeru, kAuxOutTa, kVerbMizenkei},
      {"させられたくて", "させられる", kAuxSeru, kAuxOutTe, kVerbMizenkei},
      // === Causative-passive (せられる系) - for Godan ===
      // 書く → 書か + せられる = 書かせられる
      {"せられる", "せられる", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"せられた", "せられる", kAuxSeru, kAuxOutTa, kVerbMizenkei},
      {"せられて", "せられる", kAuxSeru, kAuxOutTe, kVerbMizenkei},
      {"せられない", "せられる", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"せられなくて", "せられる", kAuxSeru, kAuxOutTe, kVerbMizenkei},
      {"せられます", "せられる", kAuxSeru, kAuxOutMasu, kVerbMizenkei},
      {"せられました", "せられる", kAuxSeru, kAuxOutTa, kVerbMizenkei},
      {"せられません", "せられる", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      // Causative-passive + なくなる (stop being made to do something) - Godan
      {"せられなくなる", "せられる", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"せられなくなった", "せられる", kAuxSeru, kAuxOutTa, kVerbMizenkei},
      {"せられなくなって", "せられる", kAuxSeru, kAuxOutTe, kVerbMizenkei},
      // Causative-passive + たい (want to be made to do) - Godan
      {"せられたい", "せられる", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"せられたかった", "せられる", kAuxSeru, kAuxOutTa, kVerbMizenkei},
      {"せられたくない", "せられる", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"せられたくなかった", "せられる", kAuxSeru, kAuxOutTa, kVerbMizenkei},
      {"せられたくて", "せられる", kAuxSeru, kAuxOutTe, kVerbMizenkei},
      // Short form causative-passive for Godan (歩かされる = 歩か + される)
      {"される", "される", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"された", "される", kAuxSeru, kAuxOutTa, kVerbMizenkei},
      {"されて", "される", kAuxSeru, kAuxOutTe, kVerbMizenkei},
      {"されない", "される", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"されなかった", "される", kAuxSeru, kAuxOutTa, kVerbMizenkei},
      {"されなくて", "される", kAuxSeru, kAuxOutTe, kVerbMizenkei},
      {"されます", "される", kAuxSeru, kAuxOutMasu, kVerbMizenkei},
      {"されました", "される", kAuxSeru, kAuxOutTa, kVerbMizenkei},
      {"されません", "される", kAuxSeru, kAuxOutBase, kVerbMizenkei},

      // === Humble progressive (ておる系) ===
      {"おる", "おる", kAuxTeiru, kAuxOutBase, kAuxOutTe},
      {"おった", "おる", kAuxTeiru, kAuxOutTa, kAuxOutTe},
      {"おります", "おる", kAuxTeiru, kAuxOutMasu, kAuxOutTe},
      {"おりました", "おる", kAuxTeiru, kAuxOutTa, kAuxOutTe},
      {"おりまして", "おる", kAuxTeiru, kAuxOutTe, kAuxOutTe},

      // === Polite receiving (ていただく系) ===
      {"いただく", "いただく", kAuxTemorau, kAuxOutBase, kAuxOutTe},
      {"いただいた", "いただく", kAuxTemorau, kAuxOutTa, kAuxOutTe},
      {"いただいて", "いただく", kAuxTemorau, kAuxOutTe, kAuxOutTe},
      {"いただきます", "いただく", kAuxTemorau, kAuxOutMasu, kAuxOutTe},
      {"いただきました", "いただく", kAuxTemorau, kAuxOutTa, kAuxOutTe},
      {"いただける", "いただく", kAuxTemorau, kAuxOutBase, kAuxOutTe},
      {"いただけます", "いただく", kAuxTemorau, kAuxOutMasu, kAuxOutTe},
      {"いただけますか", "いただく", kAuxTemorau, kAuxOutBase, kAuxOutTe},

      // === Honorific giving (てくださる系) ===
      {"くださる", "くださる", kAuxTekureru, kAuxOutBase, kAuxOutTe},
      {"くださった", "くださる", kAuxTekureru, kAuxOutTa, kAuxOutTe},
      {"くださって", "くださる", kAuxTekureru, kAuxOutTe, kAuxOutTe},
      {"ください", "くださる", kAuxTekureru, kAuxOutBase, kAuxOutTe},
      {"くださいます", "くださる", kAuxTekureru, kAuxOutMasu, kAuxOutTe},
      {"くださいました", "くださる", kAuxTekureru, kAuxOutTa, kAuxOutTe},

      // === Wanting (てほしい系) ===
      {"ほしい", "ほしい", kAuxTai, kAuxOutBase, kAuxOutTe},
      {"ほしかった", "ほしい", kAuxTai, kAuxOutTa, kAuxOutTe},
      {"ほしくない", "ほしい", kAuxTai, kAuxOutBase, kAuxOutTe},

      // === Existence (てある系) ===
      {"ある", "ある", kAuxTeiru, kAuxOutBase, kAuxOutTe},
      {"あった", "ある", kAuxTeiru, kAuxOutTa, kAuxOutTe},
      {"あります", "ある", kAuxTeiru, kAuxOutMasu, kAuxOutTe},

      // === Negative te-form (ないで系) ===
      {"ないで", "ないで", kAuxNai, kAuxOutTe, kVerbMizenkei},
      {"ないでいる", "ないで", kAuxNai, kAuxOutBase, kVerbMizenkei},
      {"ないでいた", "ないで", kAuxNai, kAuxOutTa, kVerbMizenkei},

      // === Polite negative past (ませんでした) ===
      {"ませんでした", "ます", kAuxMasu, kAuxOutTa, kVerbRenyokei},

      // === Potential form endings (可能形: 書ける etc.) ===
      // These attach to potential stem (e-row) and form Ichidan-like verbs
      {"る", "る", kAuxReru, kAuxOutBase, kVerbPotential},      // 書け + る
      {"た", "る", kAuxReru, kAuxOutTa, kVerbPotential},        // 書け + た
      {"て", "る", kAuxReru, kAuxOutTe, kVerbPotential},        // 書け + て
      {"ない", "る", kAuxReru, kAuxOutBase, kVerbPotential},    // 書け + ない
      {"なかった", "る", kAuxReru, kAuxOutTa, kVerbPotential},  // 書け + なかった
      {"ます", "る", kAuxReru, kAuxOutMasu, kVerbPotential},    // 書け + ます
      {"ません", "る", kAuxReru, kAuxOutBase, kVerbPotential},  // 書け + ません
      {"ませんでした", "る", kAuxReru, kAuxOutTa, kVerbPotential},

      // === Conditional form (仮定形 + ば) ===
      // Attaches to hypothetical stem (e-row for Godan, れ for Ichidan/Suru/Kuru)
      {"ば", "ば", kAuxNai, kAuxOutBase, kVerbKatei},           // 書けば, 食べれば, すれば

      // === Renyokei compounds (連用形 + 補助動詞) ===
      // These attach to renyokei (連用形) directly
      {"ながら", "ながら", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"すぎる", "すぎる", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"すぎた", "すぎる", kAuxRenyokei, kAuxOutTa, kVerbRenyokei},
      {"すぎて", "すぎる", kAuxRenyokei, kAuxOutTe, kVerbRenyokei},
      {"すぎている", "すぎる", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"やすい", "やすい", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"やすかった", "やすい", kAuxRenyokei, kAuxOutTa, kVerbRenyokei},
      {"にくい", "にくい", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"にくかった", "にくい", kAuxRenyokei, kAuxOutTa, kVerbRenyokei},
      {"かける", "かける", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"かけた", "かける", kAuxRenyokei, kAuxOutTa, kVerbRenyokei},
      {"かけて", "かける", kAuxRenyokei, kAuxOutTe, kVerbRenyokei},
      {"かけている", "かける", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"出す", "出す", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"出した", "出す", kAuxRenyokei, kAuxOutTa, kVerbRenyokei},
      {"出して", "出す", kAuxRenyokei, kAuxOutTe, kVerbRenyokei},
      {"終わる", "終わる", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"終わった", "終わる", kAuxRenyokei, kAuxOutTa, kVerbRenyokei},
      {"終わって", "終わる", kAuxRenyokei, kAuxOutTe, kVerbRenyokei},
      {"終える", "終える", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"終えた", "終える", kAuxRenyokei, kAuxOutTa, kVerbRenyokei},
      {"終えて", "終える", kAuxRenyokei, kAuxOutTe, kVerbRenyokei},
      {"続ける", "続ける", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"続けた", "続ける", kAuxRenyokei, kAuxOutTa, kVerbRenyokei},
      {"続けて", "続ける", kAuxRenyokei, kAuxOutTe, kVerbRenyokei},
      {"続けている", "続ける", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"直す", "直す", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"直した", "直す", kAuxRenyokei, kAuxOutTa, kVerbRenyokei},
      {"直して", "直す", kAuxRenyokei, kAuxOutTe, kVerbRenyokei},
      {"直している", "直す", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},

      // === Sou form (そう - appearance/likelihood) ===
      // Attaches to renyokei (連用形)
      {"そう", "そう", kAuxSou, kAuxOutBase, kVerbRenyokei},
      {"そうだ", "そう", kAuxSou, kAuxOutBase, kVerbRenyokei},
      {"そうだった", "そう", kAuxSou, kAuxOutTa, kVerbRenyokei},
      {"そうです", "そう", kAuxSou, kAuxOutBase, kVerbRenyokei},
      {"そうでした", "そう", kAuxSou, kAuxOutTa, kVerbRenyokei},

      // === Tari form (たり) ===
      {"たり", "たり", kAuxTa, kAuxOutBase, kVerbOnbinkei},      // 書い + たり
      {"だり", "たり", kAuxTa, kAuxOutBase, kVerbOnbinkei},      // 読ん + だり
      {"たりする", "たり", kAuxTa, kAuxOutBase, kVerbOnbinkei},  // 書い + たりする
      {"だりする", "たり", kAuxTa, kAuxOutBase, kVerbOnbinkei},  // 読ん + だりする
      {"たりした", "たり", kAuxTa, kAuxOutTa, kVerbOnbinkei},    // 書い + たりした
      {"だりした", "たり", kAuxTa, kAuxOutTa, kVerbOnbinkei},    // 読ん + だりした
      {"たりして", "たり", kAuxTa, kAuxOutTe, kVerbOnbinkei},    // 書い + たりして
      {"だりして", "たり", kAuxTa, kAuxOutTe, kVerbOnbinkei},    // 読ん + だりして

      // === I-adjective endings (い形容詞) ===
      // These attach to I-adjective stem (美し, 高, etc.)
      {"い", "い", kAuxNai, kAuxOutBase, kIAdjStem},        // 美し + い (base)
      {"かった", "い", kAuxNai, kAuxOutTa, kIAdjStem},      // 美し + かった (past)
      {"くない", "い", kAuxNai, kAuxOutBase, kIAdjStem},    // 美し + くない (negative)
      {"くなかった", "い", kAuxNai, kAuxOutTa, kIAdjStem},  // 美し + くなかった (neg past)
      {"くて", "い", kAuxNai, kAuxOutTe, kIAdjStem},        // 美し + くて (te-form)
      {"ければ", "い", kAuxNai, kAuxOutBase, kIAdjStem},    // 美し + ければ (conditional)
      {"く", "い", kAuxNai, kAuxOutBase, kIAdjStem},        // 美し + く (adverb form)
      {"かったら", "い", kAuxNai, kAuxOutBase, kIAdjStem},  // 美し + かったら (cond past)
      {"くなる", "い", kAuxNai, kAuxOutBase, kIAdjStem},    // 美し + くなる
      {"くなった", "い", kAuxNai, kAuxOutTa, kIAdjStem},    // 美し + くなった
      {"くなって", "い", kAuxNai, kAuxOutTe, kIAdjStem},    // 美し + くなって
      {"そう", "い", kAuxNai, kAuxOutBase, kIAdjStem},      // 美し + そう (looks like)
      {"そうだ", "い", kAuxNai, kAuxOutBase, kIAdjStem},    // 美し + そうだ
      {"そうだった", "い", kAuxNai, kAuxOutTa, kIAdjStem},  // 美し + そうだった
      {"そうに", "い", kAuxNai, kAuxOutBase, kIAdjStem},    // 美し + そうに (adverbially)
      {"そうな", "い", kAuxNai, kAuxOutBase, kIAdjStem},    // 美し + そうな (attributively)
      {"さ", "い", kAuxNai, kAuxOutBase, kIAdjStem},        // 美し + さ (noun form)
      // I-adjective + すぎる (excess: too ~)
      {"すぎる", "い", kAuxRenyokei, kAuxOutBase, kIAdjStem},     // 難し + すぎる
      {"すぎた", "い", kAuxRenyokei, kAuxOutTa, kIAdjStem},       // 難し + すぎた
      {"すぎて", "い", kAuxRenyokei, kAuxOutTe, kIAdjStem},       // 難し + すぎて
      {"すぎている", "い", kAuxRenyokei, kAuxOutBase, kIAdjStem}, // 難し + すぎている
      {"すぎない", "い", kAuxRenyokei, kAuxOutBase, kIAdjStem},   // 難し + すぎない
      {"すぎなかった", "い", kAuxRenyokei, kAuxOutTa, kIAdjStem}, // 難し + すぎなかった
      {"すぎます", "い", kAuxRenyokei, kAuxOutMasu, kIAdjStem},   // 難し + すぎます
      {"すぎました", "い", kAuxRenyokei, kAuxOutTa, kIAdjStem},   // 難し + すぎました

      // === Volitional + とする (ようとする: attempting) ===
      // 書こうとする = trying to write
      {"うとする", "とする", kAuxNai, kAuxOutBase, kVerbVolitional},
      {"うとした", "とする", kAuxNai, kAuxOutTa, kVerbVolitional},
      {"うとして", "とする", kAuxNai, kAuxOutTe, kVerbVolitional},
      {"うとしている", "とする", kAuxNai, kAuxOutBase, kVerbVolitional},
      {"うとしていた", "とする", kAuxNai, kAuxOutTa, kVerbVolitional},
      // Ichidan volitional (食べようとする)
      {"ようとする", "とする", kAuxNai, kAuxOutBase, kVerbVolitional},
      {"ようとした", "とする", kAuxNai, kAuxOutTa, kVerbVolitional},
      {"ようとして", "とする", kAuxNai, kAuxOutTe, kVerbVolitional},
      {"ようとしている", "とする", kAuxNai, kAuxOutBase, kVerbVolitional},
      {"ようとしていた", "とする", kAuxNai, kAuxOutTa, kVerbVolitional},

      // === Obligation patterns (ないといけない/なければならない: must do) ===
      // 書かないといけない = must write
      {"ないといけない", "ないといけない", kAuxNai, kAuxOutBase, kVerbMizenkei},
      {"ないといけなかった", "ないといけない", kAuxNai, kAuxOutTa, kVerbMizenkei},
      {"なくてはいけない", "なくてはいけない", kAuxNai, kAuxOutBase, kVerbMizenkei},
      {"なくてはいけなかった", "なくてはいけない", kAuxNai, kAuxOutTa, kVerbMizenkei},
      {"なければならない", "なければならない", kAuxNai, kAuxOutBase, kVerbMizenkei},
      {"なければならなかった", "なければならない", kAuxNai, kAuxOutTa, kVerbMizenkei},
      {"なきゃいけない", "なきゃいけない", kAuxNai, kAuxOutBase, kVerbMizenkei},
      {"なきゃならない", "なきゃならない", kAuxNai, kAuxOutBase, kVerbMizenkei},
      // Colloquial forms
      {"なくちゃ", "なくちゃ", kAuxNai, kAuxOutBase, kVerbMizenkei},
      {"なきゃ", "なきゃ", kAuxNai, kAuxOutBase, kVerbMizenkei},

      // === Ability patterns (ことができる: can do) ===
      // 書くことができる = can write (uses dictionary form)
      {"ことができる", "ことができる", kAuxNai, kAuxOutBase, kVerbBase},
      {"ことができた", "ことができる", kAuxNai, kAuxOutTa, kVerbBase},
      {"ことができない", "ことができる", kAuxNai, kAuxOutBase, kVerbBase},
      {"ことができなかった", "ことができる", kAuxNai, kAuxOutTa, kVerbBase},
      {"ことができて", "ことができる", kAuxNai, kAuxOutTe, kVerbBase},

      // === ようになる (come to be able / start doing) ===
      // 読めるようになる = come to be able to read (potential base + ようになる)
      {"ようになる", "ようになる", kAuxNai, kAuxOutBase, kAuxOutBase},
      {"ようになった", "ようになる", kAuxNai, kAuxOutTa, kAuxOutBase},
      {"ようになって", "ようになる", kAuxNai, kAuxOutTe, kAuxOutBase},
      {"ようになってきた", "ようになる", kAuxNai, kAuxOutTa, kAuxOutBase},
      {"ようになっている", "ようになる", kAuxNai, kAuxOutBase, kAuxOutBase},

      // === Casual explanatory forms (んだ/のだ系) ===
      // 書くんだ = it's that I write (explanatory)
      {"んだ", "のだ", kAuxNai, kAuxOutBase, kVerbBase},
      {"んです", "のだ", kAuxNai, kAuxOutMasu, kVerbBase},
      {"んだもん", "のだ", kAuxNai, kAuxOutBase, kVerbBase},
      {"んだもの", "のだ", kAuxNai, kAuxOutBase, kVerbBase},
      {"のだ", "のだ", kAuxNai, kAuxOutBase, kVerbBase},
      {"のです", "のだ", kAuxNai, kAuxOutMasu, kVerbBase},
      // Past form + んだ (書いたんだ)
      {"んだ", "のだ", kAuxNai, kAuxOutBase, kAuxOutTa},
      {"んだもん", "のだ", kAuxNai, kAuxOutBase, kAuxOutTa},
      {"んだもの", "のだ", kAuxNai, kAuxOutBase, kAuxOutTa},

      // === Prohibition patterns (てはいけない/てはならない) ===
      // 書いてはいけない = must not write
      {"はいけない", "はいけない", kAuxNai, kAuxOutBase, kAuxOutTe},
      {"はいけなかった", "はいけない", kAuxNai, kAuxOutTa, kAuxOutTe},
      {"はならない", "はならない", kAuxNai, kAuxOutBase, kAuxOutTe},
      {"はならなかった", "はならない", kAuxNai, kAuxOutTa, kAuxOutTe},
      {"はだめだ", "はだめだ", kAuxNai, kAuxOutBase, kAuxOutTe},

      // === Permission patterns (てもいい/てもかまわない) ===
      // 書いてもいい = may write
      {"もいい", "もいい", kAuxNai, kAuxOutBase, kAuxOutTe},
      {"もいいですか", "もいい", kAuxNai, kAuxOutBase, kAuxOutTe},
      {"もかまわない", "もかまわない", kAuxNai, kAuxOutBase, kAuxOutTe},
      {"もかまわなかった", "もかまわない", kAuxNai, kAuxOutTa, kAuxOutTe},

      // === べき (should) patterns ===
      // 書くべきだ = should write (requires dictionary form)
      {"べきだ", "べきだ", kAuxNai, kAuxOutBase, kVerbBase},
      {"べきだった", "べきだ", kAuxNai, kAuxOutTa, kVerbBase},
      {"べきではない", "べきだ", kAuxNai, kAuxOutBase, kVerbBase},
      {"べきではなかった", "べきだ", kAuxNai, kAuxOutTa, kVerbBase},
      {"べきです", "べきだ", kAuxNai, kAuxOutMasu, kVerbBase},

      // === ところだ (aspect) patterns ===
      // 書くところだ = about to write (requires dictionary form)
      {"ところだ", "ところだ", kAuxNai, kAuxOutBase, kVerbBase},
      {"ところだった", "ところだ", kAuxNai, kAuxOutTa, kVerbBase},
      {"ところです", "ところだ", kAuxNai, kAuxOutMasu, kVerbBase},
      // 食べたところだ = just ate (requires ta-form)
      {"ところだ", "ところだ", kAuxNai, kAuxOutBase, kAuxOutTa},
      {"ところだった", "ところだ", kAuxNai, kAuxOutTa, kAuxOutTa},
      {"ところです", "ところだ", kAuxNai, kAuxOutMasu, kAuxOutTa},
      // 読んでいるところだ = in the middle of reading (requires base form)
      {"ところだ", "ところだ", kAuxNai, kAuxOutBase, kAuxOutBase},
      {"ところだった", "ところだ", kAuxNai, kAuxOutTa, kAuxOutBase},

      // === ばかり (just did) patterns ===
      // 書いたばかりだ = just wrote (requires ta-form)
      {"ばかりだ", "ばかりだ", kAuxNai, kAuxOutBase, kAuxOutTa},
      {"ばかりだった", "ばかりだ", kAuxNai, kAuxOutTa, kAuxOutTa},
      {"ばかりです", "ばかりだ", kAuxNai, kAuxOutMasu, kAuxOutTa},
      {"ばかりなのに", "ばかりだ", kAuxNai, kAuxOutBase, kAuxOutTa},

      // === っぱなし (leaving in state) patterns ===
      // 開けっぱなし = left open (requires renyokei)
      {"っぱなしだ", "っぱなし", kAuxNai, kAuxOutBase, kVerbRenyokei},
      {"っぱなしにする", "っぱなし", kAuxNai, kAuxOutBase, kVerbRenyokei},
      {"っぱなしで", "っぱなし", kAuxNai, kAuxOutTe, kVerbRenyokei},

      // === ざるを得ない (cannot help but) patterns ===
      // 書かざるを得ない = cannot help but write (requires mizenkei)
      {"ざるを得ない", "ざるを得ない", kAuxNai, kAuxOutBase, kVerbMizenkei},
      {"ざるを得なかった", "ざるを得ない", kAuxNai, kAuxOutTa, kVerbMizenkei},
      {"ざるを得ません", "ざるを得ない", kAuxNai, kAuxOutMasu, kVerbMizenkei},

      // === ずにはいられない (cannot help doing) patterns ===
      // 笑わずにはいられない = cannot help laughing (requires mizenkei)
      {"ずにはいられない", "ずにはいられない", kAuxNai, kAuxOutBase, kVerbMizenkei},
      {"ずにはいられなかった", "ずにはいられない", kAuxNai, kAuxOutTa, kVerbMizenkei},

      // === わけにはいかない (cannot afford to) patterns ===
      // 書かないわけにはいかない = cannot not write (requires base form after ない)
      {"わけにはいかない", "わけにはいかない", kAuxNai, kAuxOutBase, kAuxOutBase},
      {"わけにはいかなかった", "わけにはいかない", kAuxNai, kAuxOutTa, kAuxOutBase},
      {"わけにはいきません", "わけにはいかない", kAuxNai, kAuxOutMasu, kAuxOutBase},
      // 食べるわけにはいかない = cannot afford to eat (requires dictionary form)
      {"わけにはいかない", "わけにはいかない", kAuxNai, kAuxOutBase, kVerbBase},
      {"わけにはいかなかった", "わけにはいかない", kAuxNai, kAuxOutTa, kVerbBase},

      // === Potential stem + なくなる (stop being able to) ===
      // 話せなくなる = stop being able to speak (requires potential stem)
      {"なくなる", "なくなる", kAuxNai, kAuxOutBase, kVerbPotential},
      {"なくなった", "なくなる", kAuxNai, kAuxOutTa, kVerbPotential},
      {"なくなって", "なくなる", kAuxNai, kAuxOutTe, kVerbPotential},
      {"なくなってしまう", "なくなる", kAuxNai, kAuxOutBase, kVerbPotential},
      {"なくなってしまった", "なくなる", kAuxNai, kAuxOutTa, kVerbPotential},

      // === Ichidan mizenkei + なくなる (stop doing) ===
      // 食べなくなる = stop eating (Ichidan verb mizenkei + なくなる)
      // Note: Ichidan mizenkei = stem (食べ), so 食べ + なくなった → 食べる
      {"なくなる", "なくなる", kAuxNai, kAuxOutBase, kVerbMizenkei},
      {"なくなった", "なくなる", kAuxNai, kAuxOutTa, kVerbMizenkei},
      {"なくなって", "なくなる", kAuxNai, kAuxOutTe, kVerbMizenkei},
      {"なくなってしまう", "なくなる", kAuxNai, kAuxOutBase, kVerbMizenkei},
      {"なくなってしまった", "なくなる", kAuxNai, kAuxOutTa, kVerbMizenkei},

      // === Ichidan potential + なくなる (stop being able to) ===
      // 食べられなくなる = stop being able to eat (Ichidan verb + られ + なくなる)
      {"られなくなる", "られる", kAuxReru, kAuxOutBase, kVerbMizenkei},
      {"られなくなった", "られる", kAuxReru, kAuxOutTa, kVerbMizenkei},
      {"られなくなって", "られる", kAuxReru, kAuxOutTe, kVerbMizenkei},
      {"られなくなってしまう", "られる", kAuxReru, kAuxOutBase, kVerbMizenkei},
      {"られなくなってしまった", "られる", kAuxReru, kAuxOutTa, kVerbMizenkei},

  };

  // Sort by surface length (longest first) for greedy matching
  std::sort(auxiliaries.begin(), auxiliaries.end(),
            [](const AuxiliaryEntry& lhs, const AuxiliaryEntry& rhs) {
              return lhs.surface.size() > rhs.surface.size();
            });

  return auxiliaries;
}

}  // namespace

const std::vector<AuxiliaryEntry>& getAuxiliaries() {
  static const std::vector<AuxiliaryEntry> kAuxiliaries = initAuxiliaries();
  return kAuxiliaries;
}

}  // namespace suzume::grammar
