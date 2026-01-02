#include "core/types.h"
#include "dictionary/dictionary.h"

#include <vector>

namespace suzume::dictionary::entries {

using POS = core::PartOfSpeech;
using CT = ConjugationType;

// =============================================================================
// Particles (助詞)
// =============================================================================
std::vector<DictionaryEntry> getParticleEntries() {
  return {
      // Case particles (格助詞) - low cost to ensure recognition
      {"が", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},
      {"を", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},
      {"に", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},
      {"で", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},
      {"と", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},
      {"から", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},
      {"まで", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},
      {"より", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"へ", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},

      // Binding particles (係助詞) - low cost for common particles
      {"は", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},
      {"も", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},
      {"こそ", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"さえ", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"でも", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"しか", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},

      // Conjunctive particles (接続助詞)
      {"て", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"ば", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"たら", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"なら", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},
      {"ら", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"ながら", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"のに", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"ので", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},
      {"けれど", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"けど", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"けれども", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"し", POS::Particle, 1.2F, "", false, false, false, CT::None, ""},  // 列挙・理由

      // Quotation particles (引用助詞)
      {"って", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},

      // Final particles (終助詞)
      {"か", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
      {"な", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
      {"ね", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
      {"よ", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
      {"わ", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
      {"の", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
      {"ん", POS::Particle, 0.5F, "の", false, false, false, CT::None, ""},  // colloquial の
      {"じゃん", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"っけ", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
      {"かしら", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},

      // Adverbial particles (副助詞)
      {"ばかり", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
      {"だけ", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
      {"ほど", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
      {"くらい", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"ぐらい", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"など", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
      {"なんて", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},
      {"ってば", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},
      {"ったら", POS::Particle, 0.5F, "", false, false, false, CT::None, ""},
  };
}

// =============================================================================
// Compound Particles (複合助詞)
// =============================================================================
std::vector<DictionaryEntry> getCompoundParticleEntries() {
  return {
      // Relation (関連)
      // Lower cost (0.0F) to beat false positive adjective candidates like につい
      {"について", POS::Particle, 0.0F, "", false, false, false, CT::None, ""},

      // Cause/Means (原因・手段)
      {"によって", POS::Particle, 0.0F, "", false, false, false, CT::None, ""},
      {"により", POS::Particle, 0.0F, "", false, false, false, CT::None, ""},
      {"によると", POS::Particle, 0.0F, "", false, false, false, CT::None, ""},
      {"によれば", POS::Particle, 0.0F, "", false, false, false, CT::None, ""},

      // Place/Situation (場所・状況)
      // Lower cost to prevent split as に + おいて (verb)
      {"において", POS::Particle, -0.3F, "", false, false, false, CT::None, ""},
      {"にて", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},

      // Capacity/Viewpoint (資格・観点)
      {"として", POS::Particle, 0.1F, "", false, false, false, CT::None, ""},
      {"にとって", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      // にとっても: にとって + も (even for X)
      // Prevent に + とっても(ADV) misparse
      {"にとっても", POS::Particle, -0.5F, "", false, false, false, CT::None, ""},
      // Note: 漢字を含む複合助詞（に対して、に関して等）は分割する方針
      // See CLAUDE.md: "複合助詞 with kanji | 分割 | に関して → に+関して"

      // Duration/Scope (範囲・期間)
      {"にわたって", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"にわたり", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"にあたって", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"にあたり", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},

      // Topic/Means (話題・手段)
      {"をめぐって", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"をめぐり", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"をもって", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
  };
}

// =============================================================================
// Auxiliaries (助動詞)
// =============================================================================
std::vector<DictionaryEntry> getAuxiliaryEntries() {
  return {
      // Assertion (断定)
      {"だ", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},
      {"だった", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},
      {"だったら", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},
      {"です", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},
      {"でした", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},
      {"でしたら", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},
      {"である", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},
      {"であった", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},
      {"であれば", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},

      // Polite (丁寧)
      {"ます", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"ました", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"ません", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"ましょう", POS::Auxiliary, 1.0F, "ます", false, false, false, CT::None, ""},

      // Negation (否定)
      {"ない", POS::Auxiliary, 1.0F, "ない", false, false, false, CT::None, ""},
      {"なかった", POS::Auxiliary, 1.0F, "ない", false, false, false, CT::None, ""},
      {"なくて", POS::Auxiliary, 1.0F, "ない", false, false, false, CT::None, ""},
      {"なければ", POS::Auxiliary, 1.0F, "ない", false, false, false, CT::None, ""},
      // Classical negation ぬ (文語否定 連体形) - attaches to mizenkei (未然形)
      // E.g., 消えぬ炎, 揃わぬ意見, 知れぬ心
      // Negative cost to beat NOUN interpretations considering AUX→NOUN connection cost (0.5)
      {"ぬ", POS::Auxiliary, -1.0F, "ぬ", false, false, false, CT::None, ""},
      // Classical negation (古語否定) - ず attaches to mizenkei (未然形)
      {"ず", POS::Auxiliary, 0.5F, "ず", false, false, false, CT::None, ""},
      {"ずに", POS::Auxiliary, 0.5F, "ず", false, false, false, CT::None, ""},
      {"ずとも", POS::Auxiliary, 0.5F, "ず", false, false, false, CT::None, ""},
      // Colloquial negation (口語否定) - じゃ = では
      {"じゃない", POS::Auxiliary, 0.3F, "ではない", false, false, false, CT::None, ""},
      {"じゃなかった", POS::Auxiliary, 0.3F, "ではない", false, false, false, CT::None, ""},
      {"じゃなくて", POS::Auxiliary, 0.3F, "ではない", false, false, false, CT::None, ""},

      // Past/Completion (過去・完了)
      {"た", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},

      // Conjecture (推量)
      {"う", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"よう", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"だろう", POS::Auxiliary, 0.3F, "", false, false, false, CT::None, ""},
      {"でしょう", POS::Auxiliary, 0.1F, "", false, false, false, CT::None, ""},

      // Negative conjecture (否定推量)
      {"まい", POS::Auxiliary, 0.3F, "まい", false, false, false, CT::None, ""},

      // Obligation (当為)
      {"べき", POS::Auxiliary, 0.5F, "べき", false, false, false, CT::None, ""},
      {"べきだ", POS::Auxiliary, 0.3F, "べき", false, false, false, CT::None, ""},
      {"べきだった", POS::Auxiliary, 0.3F, "べき", false, false, false, CT::None, ""},
      {"べきで", POS::Auxiliary, 0.3F, "べき", false, false, false, CT::None, ""},
      {"べきでは", POS::Auxiliary, 0.3F, "べき", false, false, false, CT::None, ""},

      // === Individual auxiliary forms for auxiliary separation (Phase 3) ===
      // Passive/Potential (受身・可能) - attaches to mizenkei (未然形)
      // れ: Godan passive stem (書か+れ+る → 書かれる)
      {"れ", POS::Auxiliary, 0.3F, "れる", false, false, false, CT::Ichidan, ""},
      {"れる", POS::Auxiliary, 0.3F, "れる", false, false, false, CT::Ichidan, ""},
      {"れた", POS::Auxiliary, 0.3F, "れる", false, false, false, CT::Ichidan, ""},
      {"れて", POS::Auxiliary, 0.3F, "れる", false, false, false, CT::Ichidan, ""},
      {"れない", POS::Auxiliary, 0.3F, "れる", false, false, false, CT::Ichidan, ""},
      {"れます", POS::Auxiliary, 0.3F, "れる", false, false, false, CT::Ichidan, ""},
      {"れべき", POS::Auxiliary, 0.3F, "れる", false, false, false, CT::Ichidan, ""},
      // られ: Ichidan passive/potential stem (食べ+られ+る → 食べられる)
      {"られ", POS::Auxiliary, 0.3F, "られる", false, false, false, CT::Ichidan, ""},
      {"られる", POS::Auxiliary, 0.3F, "られる", false, false, false, CT::Ichidan, ""},
      {"られた", POS::Auxiliary, 0.3F, "られる", false, false, false, CT::Ichidan, ""},
      {"られて", POS::Auxiliary, 0.3F, "られる", false, false, false, CT::Ichidan, ""},
      {"られない", POS::Auxiliary, 0.3F, "られる", false, false, false, CT::Ichidan, ""},
      {"られます", POS::Auxiliary, 0.3F, "られる", false, false, false, CT::Ichidan, ""},
      {"られべき", POS::Auxiliary, 0.3F, "られる", false, false, false, CT::Ichidan, ""},

      // Suru passive (サ変受身) - attaches to サ変名詞 (装飾+される → 装飾される)
      {"される", POS::Auxiliary, 0.2F, "される", false, false, false, CT::Ichidan, ""},
      {"された", POS::Auxiliary, 0.2F, "される", false, false, false, CT::Ichidan, ""},
      {"されて", POS::Auxiliary, 0.2F, "される", false, false, false, CT::Ichidan, ""},
      {"されない", POS::Auxiliary, 0.2F, "される", false, false, false, CT::Ichidan, ""},
      {"されます", POS::Auxiliary, 0.2F, "される", false, false, false, CT::Ichidan, ""},
      {"されべき", POS::Auxiliary, 0.2F, "される", false, false, false, CT::Ichidan, ""},

      // Causative (使役) - attaches to mizenkei (未然形)
      // せ: Godan causative stem (書か+せ+る → 書かせる)
      {"せ", POS::Auxiliary, 0.3F, "せる", false, false, false, CT::Ichidan, ""},
      {"せる", POS::Auxiliary, 0.3F, "せる", false, false, false, CT::Ichidan, ""},
      {"せた", POS::Auxiliary, 0.3F, "せる", false, false, false, CT::Ichidan, ""},
      {"せて", POS::Auxiliary, 0.3F, "せる", false, false, false, CT::Ichidan, ""},
      {"せない", POS::Auxiliary, 0.3F, "せる", false, false, false, CT::Ichidan, ""},
      {"せます", POS::Auxiliary, 0.3F, "せる", false, false, false, CT::Ichidan, ""},
      // させ: Ichidan/Suru causative stem (食べ+させ+る → 食べさせる)
      {"させ", POS::Auxiliary, 0.3F, "させる", false, false, false, CT::Ichidan, ""},
      {"させる", POS::Auxiliary, 0.3F, "させる", false, false, false, CT::Ichidan, ""},
      {"させた", POS::Auxiliary, 0.3F, "させる", false, false, false, CT::Ichidan, ""},
      {"させて", POS::Auxiliary, 0.3F, "させる", false, false, false, CT::Ichidan, ""},
      {"させない", POS::Auxiliary, 0.3F, "させる", false, false, false, CT::Ichidan, ""},
      {"させます", POS::Auxiliary, 0.3F, "させる", false, false, false, CT::Ichidan, ""},

      // Desiderative conjugated forms (願望活用形) - attaches to renyokei (連用形)
      // たく: 願望連用形 (食べ+たく+ない → 食べたくない)
      {"たく", POS::Auxiliary, 0.3F, "たい", false, false, false, CT::IAdjective, ""},
      {"たくない", POS::Auxiliary, 0.3F, "たい", false, false, false, CT::IAdjective, ""},
      {"たくなかった", POS::Auxiliary, 0.3F, "たい", false, false, false, CT::IAdjective, ""},
      {"たくて", POS::Auxiliary, 0.3F, "たい", false, false, false, CT::IAdjective, ""},
      {"たかっ", POS::Auxiliary, 0.3F, "たい", false, false, false, CT::IAdjective, ""},
      {"たかった", POS::Auxiliary, 0.3F, "たい", false, false, false, CT::IAdjective, ""},

      // Polite imperative (丁寧命令)
      {"なさい", POS::Auxiliary, 0.3F, "なさい", false, false, false, CT::None, ""},

      // Possibility/Uncertainty (可能性・不確実)
      {"かもしれない", POS::Auxiliary, 0.3F, "かもしれない", false, false, false, CT::None, ""},
      {"かもしれません", POS::Auxiliary, 0.3F, "かもしれない", false, false, false, CT::None, ""},
      {"かもしれなかった", POS::Auxiliary, 0.3F, "かもしれない", false, false, false, CT::None, ""},

      // Certainty (確実) - idiomatic expressions
      // 間違いない: "definitely", "no doubt" (from 間違い + ない)
      // 違いない: "certainly", "must be" (from 違い + ない)
      {"間違いない", POS::Auxiliary, 0.3F, "間違いない", false, false, false, CT::None, "まちがいない"},
      {"違いない", POS::Auxiliary, 0.3F, "違いない", false, false, false, CT::None, "ちがいない"},
      {"に違いない", POS::Auxiliary, 0.3F, "違いない", false, false, false, CT::None, "にちがいない"},

      // Desire (願望)
      {"たい", POS::Auxiliary, 0.3F, "たい", false, false, false, CT::None, ""},
      {"たかった", POS::Adjective, 0.3F, "たい", false, false, false, CT::IAdjective, ""},
      {"たくない", POS::Adjective, 0.3F, "たい", false, false, false, CT::IAdjective, ""},
      {"たくなかった", POS::Adjective, 0.3F, "たい", false, false, false, CT::IAdjective, ""},
      {"たくて", POS::Adjective, 0.3F, "たい", false, false, false, CT::IAdjective, ""},
      {"たければ", POS::Adjective, 0.3F, "たい", false, false, false, CT::IAdjective, ""},
      {"たがる", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},

      // Potential/Passive/Causative
      {"れる", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"られる", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"せる", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"させる", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},

      // Polite existence (丁寧存在)
      {"ございます", POS::Auxiliary, 0.3F, "ございます", false, false, false, CT::None, ""},
      {"ございました", POS::Auxiliary, 0.3F, "ございます", false, false, false, CT::None, ""},
      {"ございましたら", POS::Auxiliary, 0.3F, "ございます", false, false, false, CT::None, ""},
      {"ございません", POS::Auxiliary, 0.3F, "ございます", false, false, false, CT::None, ""},

      // Request (依頼)
      {"ください", POS::Auxiliary, 0.3F, "ください", false, false, false, CT::None, ""},
      {"くださいませ", POS::Auxiliary, 0.3F, "ください", false, false, false, CT::None, ""},

      // Progressive/Continuous (進行・継続)
      {"いる", POS::Auxiliary, 0.3F, "いる", false, false, false, CT::None, ""},
      {"います", POS::Auxiliary, 0.3F, "いる", false, false, false, CT::None, ""},
      {"いました", POS::Auxiliary, 0.3F, "いる", false, false, false, CT::None, ""},
      {"いません", POS::Auxiliary, 0.3F, "いる", false, false, false, CT::None, ""},
      {"いない", POS::Auxiliary, 0.3F, "いる", false, false, false, CT::None, ""},
      {"いなかった", POS::Auxiliary, 0.3F, "いる", false, false, false, CT::None, ""},
      {"いれば", POS::Auxiliary, 0.3F, "いる", false, false, false, CT::None, ""},

      // Completive/Regretful (完了・遺憾) - しまう auxiliary
      // て形 + しまう pattern: 食べてしまった, 忘れてしまった (regret/completion)
      {"しまう", POS::Auxiliary, 0.3F, "しまう", false, false, false, CT::None, ""},
      {"しまった", POS::Auxiliary, 0.3F, "しまう", false, false, false, CT::None, ""},
      {"しまって", POS::Auxiliary, 0.3F, "しまう", false, false, false, CT::None, ""},
      {"しまいます", POS::Auxiliary, 0.3F, "しまう", false, false, false, CT::None, ""},
      {"しまいました", POS::Auxiliary, 0.3F, "しまう", false, false, false, CT::None, ""},
      {"しまいません", POS::Auxiliary, 0.3F, "しまう", false, false, false, CT::None, ""},
      {"しまわない", POS::Auxiliary, 0.3F, "しまう", false, false, false, CT::None, ""},
      {"しまわなかった", POS::Auxiliary, 0.3F, "しまう", false, false, false, CT::None, ""},
      {"しまえば", POS::Auxiliary, 0.3F, "しまう", false, false, false, CT::None, ""},
      // Contracted forms: ちゃう/じゃう = てしまう/でしまう
      {"ちゃう", POS::Auxiliary, 0.3F, "しまう", false, false, false, CT::None, ""},
      {"ちゃった", POS::Auxiliary, 0.3F, "しまう", false, false, false, CT::None, ""},
      {"ちゃって", POS::Auxiliary, 0.3F, "しまう", false, false, false, CT::None, ""},
      {"ちゃいます", POS::Auxiliary, 0.3F, "しまう", false, false, false, CT::None, ""},
      {"ちゃいました", POS::Auxiliary, 0.3F, "しまう", false, false, false, CT::None, ""},
      {"じゃう", POS::Auxiliary, 0.3F, "しまう", false, false, false, CT::None, ""},
      {"じゃった", POS::Auxiliary, 0.3F, "しまう", false, false, false, CT::None, ""},
      {"じゃって", POS::Auxiliary, 0.3F, "しまう", false, false, false, CT::None, ""},
      {"じゃいます", POS::Auxiliary, 0.3F, "しまう", false, false, false, CT::None, ""},
      {"じゃいました", POS::Auxiliary, 0.3F, "しまう", false, false, false, CT::None, ""},

      // Directional auxiliaries (方向補助動詞)
      // て形 + いく/くる pattern: 見ていく (going), 見てくる (coming back)
      // Cost 1.5F so VERB (1.2F) wins for standalone usage, but AUX wins after て-form
      // due to favorable VERB→AUX bigram connection (typically -0.5F bonus)
      // Note: いく already gets char_speech AUX candidates, くる needs explicit entries
      {"くる", POS::Auxiliary, 1.5F, "くる", false, false, false, CT::None, ""},
      {"きます", POS::Auxiliary, 1.5F, "くる", false, false, false, CT::None, ""},
      {"きました", POS::Auxiliary, 1.5F, "くる", false, false, false, CT::None, ""},
      // Lower cost for きた to beat colloquial ってき split in patterns like なってきた
      // Cost 0.5F: word_cost = 0.5 + 0.2(pos) - 1.0(dict) = -0.3
      // This beats な+ってき+た path cost of -1.296 when combined with なって(-0.998)
      {"きた", POS::Auxiliary, 0.5F, "くる", false, false, false, CT::None, ""},
      {"こない", POS::Auxiliary, 1.5F, "くる", false, false, false, CT::None, ""},

      // Explanatory (説明)
      {"のだ", POS::Auxiliary, 0.3F, "のだ", false, false, false, CT::None, ""},
      {"のです", POS::Auxiliary, 0.3F, "のだ", false, false, false, CT::None, ""},
      {"のでした", POS::Auxiliary, 0.3F, "のだ", false, false, false, CT::None, ""},
      {"んだ", POS::Auxiliary, 0.3F, "のだ", false, false, false, CT::None, ""},
      {"んです", POS::Auxiliary, 0.3F, "のだ", false, false, false, CT::None, ""},
      {"んでした", POS::Auxiliary, 0.3F, "のだ", false, false, false, CT::None, ""},

      // Kuruwa-kotoba (廓言葉)
      {"ありんす", POS::Auxiliary, 0.3F, "ある", false, false, false, CT::None, ""},
      {"ありんした", POS::Auxiliary, 0.3F, "ある", false, false, false, CT::None, ""},
      {"ありんせん", POS::Auxiliary, 0.3F, "ある", false, false, false, CT::None, ""},
      {"ざんす", POS::Auxiliary, 0.3F, "ある", false, false, false, CT::None, ""},
      {"ざました", POS::Auxiliary, 0.3F, "ある", false, false, false, CT::None, ""},
      {"ざんせん", POS::Auxiliary, 0.3F, "ある", false, false, false, CT::None, ""},
      {"でありんす", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, ""},
      {"でありんした", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, ""},

      // Cat-like (猫系)
      {"にゃ", POS::Auxiliary, 0.3F, "よ", false, false, false, CT::None, ""},
      {"にゃん", POS::Auxiliary, 0.3F, "よ", false, false, false, CT::None, ""},
      {"にゃー", POS::Auxiliary, 0.3F, "よ", false, false, false, CT::None, ""},
      {"ニャ", POS::Auxiliary, 0.3F, "よ", false, false, false, CT::None, "にゃ"},
      {"ニャン", POS::Auxiliary, 0.3F, "よ", false, false, false, CT::None, "にゃん"},
      {"ニャー", POS::Auxiliary, 0.3F, "よ", false, false, false, CT::None, "にゃー"},
      {"だにゃ", POS::Auxiliary, 0.01F, "だよ", false, false, false, CT::None, ""},
      {"だにゃん", POS::Auxiliary, 0.01F, "だよ", false, false, false, CT::None, ""},
      {"ですにゃ", POS::Auxiliary, 0.01F, "ですよ", false, false, false, CT::None, ""},
      {"ですにゃん", POS::Auxiliary, 0.01F, "ですよ", false, false, false, CT::None, ""},

      // Squid character (イカ娘)
      {"ゲソ", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, "げそ"},
      {"げそ", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, ""},
      {"でゲソ", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, "でげそ"},
      {"でげそ", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, ""},

      // Ojou-sama/Lady speech (お嬢様言葉)
      {"ですわ", POS::Auxiliary, 0.1F, "です", false, false, false, CT::None, ""},
      {"ましたわ", POS::Auxiliary, 0.1F, "ました", false, false, false, CT::None, ""},
      {"ませんわ", POS::Auxiliary, 0.1F, "ません", false, false, false, CT::None, ""},
      {"ですの", POS::Auxiliary, 0.1F, "です", false, false, false, CT::None, ""},
      {"ますの", POS::Auxiliary, 0.1F, "ます", false, false, false, CT::None, ""},
      {"だわ", POS::Auxiliary, 0.1F, "だ", false, false, false, CT::None, ""},

      // Youth slang (若者言葉)
      {"っす", POS::Auxiliary, 0.3F, "です", false, false, false, CT::None, ""},
      {"っした", POS::Auxiliary, 0.3F, "でした", false, false, false, CT::None, ""},
      {"っすか", POS::Auxiliary, 0.3F, "ですか", false, false, false, CT::None, ""},

      // Rabbit-like (兎系)
      {"ぴょん", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, ""},
      {"ピョン", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, "ぴょん"},

      // Ninja/Old-fashioned (忍者・古風)
      {"ござる", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, ""},
      {"でござる", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, ""},
      {"ござった", POS::Auxiliary, 0.3F, "だった", false, false, false, CT::None, ""},
      {"でござった", POS::Auxiliary, 0.3F, "だった", false, false, false, CT::None, ""},
      {"ござらぬ", POS::Auxiliary, 0.3F, "ではない", false, false, false, CT::None, ""},
      {"ござらん", POS::Auxiliary, 0.3F, "ではない", false, false, false, CT::None, ""},
      {"でございます", POS::Auxiliary, 0.3F, "です", false, false, false, CT::None, ""},
      {"ナリ", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, "なり"},
      {"なり", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, ""},
      {"でナリ", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, "でなり"},
      {"でなり", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, ""},

      // Elderly/Archaic (老人・古風)
      {"じゃ", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, ""},
      {"じゃな", POS::Auxiliary, 0.3F, "だ", false, false, false, CT::None, ""},
      {"のじゃ", POS::Auxiliary, 0.3F, "のだ", false, false, false, CT::None, ""},
      {"じゃろう", POS::Auxiliary, 0.3F, "だろう", false, false, false, CT::None, ""},

      // Regional dialects (方言系)
      {"ぜよ", POS::Auxiliary, 1.0F, "だ", false, false, false, CT::None, ""},
      {"だべ", POS::Auxiliary, 1.0F, "だ", false, false, false, CT::None, ""},
      {"やんけ", POS::Auxiliary, 1.0F, "だ", false, false, false, CT::None, ""},
      {"やで", POS::Auxiliary, 1.0F, "だ", false, false, false, CT::None, ""},
      {"やねん", POS::Auxiliary, 1.0F, "だ", false, false, false, CT::None, ""},
      {"だっちゃ", POS::Auxiliary, 1.0F, "だ", false, false, false, CT::None, ""},
      {"ばい", POS::Auxiliary, 1.0F, "だ", false, false, false, CT::None, ""},

      // Robot/Mechanical (ロボット・機械)
      // Moderate cost: avoid マスカラ/デスク splits, but allow as sentence-final
      {"デス", POS::Auxiliary, 1.2F, "です", false, false, false, CT::None, "です"},
      {"マス", POS::Auxiliary, 1.2F, "ます", false, false, false, CT::None, "ます"},
  };
}

// =============================================================================
// Conjunctions (接続詞)
// =============================================================================
std::vector<DictionaryEntry> getConjunctionEntries() {
  return {
      // Sequential (順接) - kanji with reading
      {"従って", POS::Conjunction, 0.5F, "", false, false, false, CT::None, "したがって"},
      {"故に", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "ゆえに"},
      // Sequential (順接) - hiragana only
      {"そして", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"それから", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"すると", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"だから", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"そのため", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      // したがって: hiragana form - prevent したがう(VERB) interpretation at sentence start
      {"したがって", POS::Conjunction, -0.5F, "従って", false, false, false, CT::None, ""},

      // Adversative (逆接)
      {"しかし", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"だが", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"けれども", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      // ところが: "however, but" - prevent ところ(NOUN)+が(PARTICLE) split (cost -1.6)
      {"ところが", POS::Conjunction, -1.5F, "", false, false, false, CT::None, ""},
      {"それでも", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"でも", POS::Conjunction, 0.5F, "", false, false, false, CT::None, ""},
      {"だって", POS::Conjunction, 0.5F, "", false, false, false, CT::None, ""},
      {"にもかかわらず", POS::Conjunction, 0.5F, "", false, false, false, CT::None, ""},
      // ものの: "although" - prevent もの(NOUN)+の(PARTICLE) split
      {"ものの", POS::Conjunction, -1.0F, "", false, false, false, CT::None, ""},

      // Parallel/Addition (並列・添加) - kanji with reading
      {"又", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "また"},
      {"及び", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "および"},
      {"並びに", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "ならびに"},
      {"且つ", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "かつ"},
      {"更に", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "さらに"},
      // Parallel/Addition - hiragana only
      {"しかも", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"そのうえ", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},

      // Alternative (選択) - kanji with reading
      {"或いは", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "あるいは"},
      {"若しくは", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "もしくは"},
      // Alternative - hiragana only
      {"または", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"それとも", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      // あるいは: hiragana form - prevent あるい(ADJ)+は(PARTICLE) split
      {"あるいは", POS::Conjunction, -0.5F, "或いは", false, false, false, CT::None, ""},

      // Explanation/Supplement (説明・補足) - kanji with reading
      {"即ち", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "すなわち"},
      {"例えば", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "たとえば"},
      {"但し", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "ただし"},
      {"尚", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "なお"},
      // Explanation/Supplement - hiragana only
      {"つまり", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"たとえば", POS::Conjunction, 0.3F, "", false, false, false, CT::None, ""},  // lower cost to prefer over たとえ+ば split
      {"なぜなら", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"ちなみに", POS::Conjunction, 0.3F, "", false, false, false, CT::None, ""},
      {"まして", POS::Conjunction, 0.5F, "", false, false, false, CT::None, ""},  // 況して

      // Topic change (転換)
      {"さて", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"ところで", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"では", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"それでは", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},

      // Addition/Emphasis (添加・強調) - B-5
      // のみならず: not only... but also - prevent の+みな+ら+ず split (N6)
      {"のみならず", POS::Conjunction, -0.5F, "", false, false, false, CT::None, ""},

      // Additional conjunctions/adverbs (工程3)
      // いわば: "so to speak, as it were" - prevent OTHER or い+わ+ば split
      {"いわば", POS::Conjunction, 0.1F, "言わば", false, false, false, CT::None, ""},
      {"言わば", POS::Conjunction, 0.1F, "", false, false, false, CT::None, "いわば"},
      // さもないと: "otherwise" - prevent さ+も+ない+と split
      {"さもないと", POS::Conjunction, 0.1F, "", false, false, false, CT::None, ""},
      {"さもなければ", POS::Conjunction, 0.1F, "", false, false, false, CT::None, ""},

      // Additional conjunctions (工程4)
      // そんなら: "in that case" - prevent そんな(DET)+ら(PARTICLE) split
      {"そんなら", POS::Conjunction, -1.0F, "其んなら", false, false, false, CT::None, ""},
      // それにしても: "even so, nevertheless" - prevent それ+に+して+も split
      {"それにしても", POS::Conjunction, -2.0F, "", false, false, false, CT::None, ""},
      // ともかく: "anyway, at any rate" - prevent と+も+かく split
      {"ともかく", POS::Adverb, -1.5F, "", false, false, false, CT::None, ""},
      // いずれにしても: "in any case" - prevent いずれ+に+して+も split
      {"いずれにしても", POS::Conjunction, -2.5F, "", false, false, false, CT::None, ""},
      // いずれにせよ: "in any case" (formal) - prevent split
      {"いずれにせよ", POS::Conjunction, -2.0F, "", false, false, false, CT::None, ""},
  };
}

// =============================================================================
// Determiners (連体詞)
// =============================================================================
std::vector<DictionaryEntry> getDeterminerEntries() {
  return {
      // Demonstrative determiners (指示連体詞) - この/その/あの/どの
      {"この", POS::Determiner, 1.0F, "", false, false, false, CT::None, ""},
      {"その", POS::Determiner, 1.0F, "", false, false, false, CT::None, ""},
      {"あの", POS::Determiner, 1.0F, "", false, false, false, CT::None, ""},
      {"どの", POS::Determiner, 1.0F, "", false, false, false, CT::None, ""},
      // Demonstrative determiners (指示連体詞) - こんな/そんな/あんな/どんな
      {"こんな", POS::Determiner, 0.5F, "", false, false, false, CT::None, ""},
      {"そんな", POS::Determiner, 0.5F, "", false, false, false, CT::None, ""},
      {"あんな", POS::Determiner, 0.5F, "", false, false, false, CT::None, ""},
      {"どんな", POS::Determiner, 0.5F, "", false, false, false, CT::None, ""},

      // Other determiners (連体詞)
      {"ある", POS::Determiner, 1.0F, "", false, false, false, CT::None, ""},
      {"あらゆる", POS::Determiner, 1.0F, "", false, false, false, CT::None, ""},
      {"いわゆる", POS::Determiner, 1.0F, "", false, false, false, CT::None, ""},
      {"おかしな", POS::Determiner, 1.0F, "", false, false, false, CT::None, ""},

      // Demonstrative manner determiners (指示様態連体詞)
      {"こういう", POS::Determiner, 0.5F, "", false, false, false, CT::None, ""},
      {"そういう", POS::Determiner, 0.5F, "", false, false, false, CT::None, ""},
      {"ああいう", POS::Determiner, 0.5F, "", false, false, false, CT::None, ""},
      {"どういう", POS::Determiner, 0.5F, "", false, false, false, CT::None, ""},

      // Quotative determiners (引用連体詞) - prevents incorrect split like 病+とい+う
      // Lower cost to beat と(PARTICLE,-0.4)+いった(VERB,-0.034)+conn(0.2)=-0.232
      {"という", POS::Determiner, -0.2F, "という", false, false, false, CT::None, ""},
      {"といった", POS::Determiner, -0.5F, "という", false, false, false, CT::None, ""},
      {"っていう", POS::Determiner, 0.5F, "という", false, false, false, CT::None, ""},  // colloquial

      // Quotative verb forms (引用動詞活用形) - to + 言う conjugations
      // These compete with と(PARTICLE) + いって(行く, cost 1.2F) paths
      {"といって", POS::Verb, 0.3F, "いう", false, false, false, CT::None, ""},
      {"といっては", POS::Verb, 0.3F, "いう", false, false, false, CT::None, ""},
      {"といっても", POS::Conjunction, -1.5F, "といっても", false, false, false, CT::None, ""},
      {"そういって", POS::Verb, 0.3F, "いう", false, false, false, CT::None, ""},
      {"こういって", POS::Verb, 0.3F, "いう", false, false, false, CT::None, ""},
      {"ああいって", POS::Verb, 0.3F, "いう", false, false, false, CT::None, ""},
      {"どういって", POS::Verb, 0.3F, "いう", false, false, false, CT::None, ""},

      // Determiners with kanji - B51: lowered cost to prioritize over NOUN unknown
      {"大きな", POS::Determiner, 0.5F, "", false, false, false, CT::None, "おおきな"},
      {"小さな", POS::Determiner, 0.5F, "", false, false, false, CT::None, "ちいさな"},
  };
}

// =============================================================================
// Pronouns (代名詞)
// =============================================================================
std::vector<DictionaryEntry> getPronounEntries() {
  return {
      // First person (一人称) - kanji with reading
      // Note: 私/俺 have lower cost to encourage splits (第一私は → 第一+私+は)
      // But 僕 keeps higher cost due to compounds like 下僕
      {"私", POS::Pronoun, 0.3F, "", false, false, false, CT::None, "わたし"},
      {"僕", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "ぼく"},
      {"俺", POS::Pronoun, 0.3F, "", false, false, false, CT::None, "おれ"},
      // First person - hiragana only
      {"わたくし", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},

      // First person plural (一人称複数)
      // Lower cost to beat single pronoun + suffix split
      {"私たち", POS::Pronoun, 0.2F, "", false, false, false, CT::None, "わたしたち"},
      {"僕たち", POS::Pronoun, 0.2F, "", false, false, false, CT::None, "ぼくたち"},
      {"僕ら", POS::Pronoun, 0.2F, "", false, false, false, CT::None, "ぼくら"},
      {"俺たち", POS::Pronoun, 0.2F, "", false, false, false, CT::None, "おれたち"},
      {"俺ら", POS::Pronoun, 0.2F, "", false, false, false, CT::None, "おれら"},
      {"我々", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "われわれ"},

      // Second person (二人称) - kanji with reading
      {"貴方", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "あなた"},
      {"君", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "きみ"},
      // Second person - hiragana/mixed only
      {"あなた", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      // B39: お前 needs low cost to beat PREFIX(お)+NOUN(前) split (connection bonus -1.5)
      // PREFIX→NOUN path has cost ~-1.2, so お前 needs cost < -1.2 to win
      {"お前", POS::Pronoun, -1.5F, "", false, false, true, CT::None, "おまえ"},

      // Second person plural (二人称複数)
      {"あなたたち", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"君たち", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "きみたち"},

      // Third person (三人称) - kanji with reading
      {"彼", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "かれ"},
      {"彼女", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "かのじょ"},
      {"彼ら", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "かれら"},
      {"彼女ら", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "かのじょら"},
      {"彼女たち", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "かのじょたち"},

      // Archaic/Samurai (武家・古風)
      {"拙者", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "せっしゃ"},
      {"貴殿", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "きでん"},
      {"某", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "それがし"},
      {"我輩", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "わがはい"},
      {"吾輩", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "わがはい"},

      // Collective pronouns (集合代名詞)
      {"皆", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "みな"},
      {"皆さん", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "みなさん"},
      {"みんな", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},

      // Demonstrative - proximal (近称)
      {"これ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"ここ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"こちら", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},

      // Demonstrative - medial (中称)
      {"それ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"そこ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"そちら", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},

      // Demonstrative - distal (遠称)
      {"あれ", POS::Pronoun, 0.8F, "", false, false, false, CT::None, ""},
      {"あそこ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"あちら", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},

      // Demonstrative - person reference (こそあど+いつ)
      {"こいつ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"そいつ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"あいつ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"どいつ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},

      // Demonstrative - interrogative (不定称)
      {"どれ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"どこ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"どちら", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},

      // Indefinite (不定代名詞) - kanji with reading
      // Low cost to act as strong anchors against prefix compounds (今何 → 今+何)
      // Cost -1.5 to beat: 今(1.4) + 何(-1.9) + conn(0.5) = 0.0 < 今何(0.5)
      {"誰", POS::Pronoun, -1.5F, "", false, false, true, CT::None, "だれ"},
      {"何", POS::Pronoun, -1.5F, "", false, false, true, CT::None, "なに"},

      // Interrogatives (疑問詞)
      {"いつ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"いくつ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"いくら", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"どう", POS::Adverb, 0.5F, "", false, false, true, CT::None, ""},
      // Note: どうして needs very low cost to prevent split when followed by verb
      // The te-form bonus makes どう+して+VERB cheaper than どうして+VERB
      // is_low_info=false because interrogatives carry high semantic information
      {"どうして", POS::Adverb, -0.2F, "", false, false, false, CT::None, ""},
      {"なぜ", POS::Adverb, 0.5F, "", false, false, true, CT::None, ""},
      {"どんな", POS::Adverb, 0.5F, "", false, false, true, CT::None, ""},
  };
}

// =============================================================================
// Formal Nouns (形式名詞)
// =============================================================================
std::vector<DictionaryEntry> getFormalNounEntries() {
  return {
      // Formal nouns (形式名詞) - hiragana form is canonical in modern Japanese
      // こと/もの are grammatical function words, hiragana is preferred
      {"事", POS::Noun, 0.3F, "こと", false, true, false, CT::None, "こと"},
      {"こと", POS::Noun, -0.5F, "", false, true, false, CT::None, ""},
      {"物", POS::Noun, 0.3F, "もの", false, true, false, CT::None, "もの"},
      {"もの", POS::Noun, 0.25F, "", false, true, false, CT::None, ""},
      {"為", POS::Noun, 2.0F, "", false, true, false, CT::None, "ため"},
      {"所", POS::Noun, 1.0F, "", false, true, false, CT::None, "ところ"},
      {"ところ", POS::Noun, 0.3F, "", false, true, false, CT::None, ""},
      {"時", POS::Noun, 2.0F, "", false, true, false, CT::None, "とき"},
      {"内", POS::Noun, 2.0F, "", false, true, false, CT::None, "うち"},
      {"通り", POS::Noun, 2.0F, "", false, true, false, CT::None, "とおり"},
      {"限り", POS::Noun, 2.0F, "", false, true, false, CT::None, "かぎり"},
      // Suffix-like formal nouns
      // Lower cost for 付け to compete with verb_kanji ichidan pattern
      {"付け", POS::Noun, -0.3F, "", false, true, false, CT::None, "つけ"},
      {"付", POS::Noun, 0.5F, "", false, true, false, CT::None, "つけ"},
      // Hiragana-only forms
      {"よう", POS::Noun, 2.0F, "", false, true, false, CT::None, ""},
      {"ほう", POS::Noun, 0.5F, "", false, true, false, CT::None, ""},  // B49: lowered cost
      {"わけ", POS::Noun, 2.0F, "", false, true, false, CT::None, ""},
      {"はず", POS::Noun, 0.3F, "はず", false, true, false, CT::None, "はず"},
      {"つもり", POS::Noun, 2.0F, "", false, true, false, CT::None, ""},
      {"まま", POS::Noun, 2.0F, "", false, true, false, CT::None, ""},
      {"ほか", POS::Noun, 0.3F, "ほか", false, true, false, CT::None, ""},
      {"他", POS::Noun, 0.3F, "ほか", false, true, false, CT::None, "ほか"},
      // Abstract nouns that don't form suru-verbs
      {"仕方", POS::Noun, 0.3F, "", false, true, false, CT::None, "しかた"},
      {"ありきたり", POS::Noun, 0.3F, "ありきたり", false, true, false, CT::None, ""},  // na-adjective stem, not suru-verb
      {"たたずまい", POS::Noun, 0.3F, "たたずまい", false, true, false, CT::None, ""},  // noun, not suru-verb
      // NOTE: 〜がち forms are now handled by generateGachiSuffixCandidates() in suffix_candidates.cpp
      // B35: Idiom component (eaves bracket - used in うだつが上がらない)
      {"うだつ", POS::Noun, 0.3F, "うだつ", false, true, false, CT::None, ""},
  };
}

// =============================================================================
// Time Nouns (時間名詞)
// =============================================================================
std::vector<DictionaryEntry> getTimeNounEntries() {
  // Time nouns are NOT formal nouns (形式名詞)
  // Formal nouns: こと, もの, ところ - abstract grammatical function words
  // Time nouns: 明日, 昨日, 今日 - concrete time references
  return {
      // Days (日)
      {"今日", POS::Noun, 0.3F, "", false, false, false, CT::None, "きょう"},
      {"明日", POS::Noun, -1.5F, "", false, false, false, CT::None, "あした"},
      {"昨日", POS::Noun, -1.5F, "", false, false, false, CT::None, "きのう"},
      {"明後日", POS::Noun, 0.5F, "", false, false, false, CT::None, "あさって"},
      {"一昨日", POS::Noun, 0.5F, "", false, false, false, CT::None, "おととい"},
      {"毎日", POS::Noun, 0.5F, "", false, false, false, CT::None, "まいにち"},

      // Time of day (時間帯)
      {"今朝", POS::Noun, 0.5F, "", false, false, false, CT::None, "けさ"},
      {"毎朝", POS::Noun, 0.5F, "", false, false, false, CT::None, "まいあさ"},
      {"今晩", POS::Noun, 0.5F, "", false, false, false, CT::None, "こんばん"},
      {"今夜", POS::Noun, 0.5F, "", false, false, false, CT::None, "こんや"},
      {"昨夜", POS::Noun, 0.5F, "", false, false, false, CT::None, "さくや"},
      {"朝", POS::Noun, 0.6F, "", false, false, false, CT::None, "あさ"},
      {"昼", POS::Noun, 0.6F, "", false, false, false, CT::None, "ひる"},
      {"夜", POS::Noun, 0.6F, "", false, false, false, CT::None, "よる"},
      {"夕方", POS::Noun, 0.6F, "", false, false, false, CT::None, "ゆうがた"},

      // Weeks (週)
      {"今週", POS::Noun, 0.5F, "", false, false, false, CT::None, "こんしゅう"},
      {"来週", POS::Noun, 0.5F, "", false, false, false, CT::None, "らいしゅう"},
      {"先週", POS::Noun, 0.5F, "", false, false, false, CT::None, "せんしゅう"},
      {"毎週", POS::Noun, 0.5F, "", false, false, false, CT::None, "まいしゅう"},

      // Months (月)
      {"今月", POS::Noun, 0.5F, "", false, false, false, CT::None, "こんげつ"},
      {"来月", POS::Noun, 0.5F, "", false, false, false, CT::None, "らいげつ"},
      {"先月", POS::Noun, 0.5F, "", false, false, false, CT::None, "せんげつ"},
      {"毎月", POS::Noun, 0.5F, "", false, false, false, CT::None, "まいつき"},

      // Years (年)
      {"今年", POS::Noun, 0.5F, "", false, false, false, CT::None, "ことし"},
      {"来年", POS::Noun, 0.5F, "", false, false, false, CT::None, "らいねん"},
      {"去年", POS::Noun, 0.5F, "", false, false, false, CT::None, "きょねん"},
      {"昨年", POS::Noun, 0.5F, "", false, false, false, CT::None, "さくねん"},
      {"毎年", POS::Noun, 0.5F, "", false, false, false, CT::None, "まいとし"},

      // Other time expressions
      // Note: 今 handled by prefix+single_kanji candidate generation
      {"現在", POS::Noun, 0.5F, "", false, false, false, CT::None, "げんざい"},
      {"最近", POS::Noun, 0.5F, "", false, false, false, CT::None, "さいきん"},
      {"将来", POS::Noun, 0.5F, "", false, false, false, CT::None, "しょうらい"},
      {"過去", POS::Noun, 0.5F, "", false, false, false, CT::None, "かこ"},
      {"未来", POS::Noun, 0.5F, "", false, false, false, CT::None, "みらい"},
      // 時分: time period, around that time (e.g., その時分, 若い時分)
      {"時分", POS::Noun, 0.5F, "時分", false, true, false, CT::None, "じぶん"},
      // Time-related temporal nouns (時間関係名詞)
      // 以来: since then - prevent 以来下着 over-concatenation
      {"以来", POS::Noun, -0.5F, "以来", false, true, false, CT::None, "いらい"},
      // 時間: time (duration) - prevent 時間すら → 時+間すら split
      {"時間", POS::Noun, 0.1F, "時間", false, true, false, CT::None, "じかん"},

      // Hiragana time nouns (ひらがな時間名詞) - B-7
      // あと: after - prevent あ+と particle split (N1)
      {"あと", POS::Noun, -0.5F, "後", false, true, false, CT::None, ""},
      // まえ: before - prevent まえ(OTHER) confusion (N1)
      {"まえ", POS::Noun, -0.5F, "前", false, true, false, CT::None, ""},
      // あとで: later (adverbial) - prevent あと+で split
      {"あとで", POS::Adverb, -0.8F, "後で", false, false, false, CT::None, ""},

      // Kanji time/position nouns (漢字時間/位置名詞) - prevent over-concatenation
      // 後: after - prevent 後猫 concatenation (N2)
      {"後", POS::Noun, 0.5F, "後", false, true, false, CT::None, ""},
      // 前: before/in front
      {"前", POS::Noun, 0.5F, "前", false, true, false, CT::None, ""},
      // 上: above/up - prevent 上今 concatenation (N5)
      {"上", POS::Noun, 0.5F, "上", false, true, false, CT::None, ""},
      // 下: below/down
      {"下", POS::Noun, 0.5F, "下", false, true, false, CT::None, ""},
      // 中: middle/inside (also used as suffix, but needs base entry)
      {"中", POS::Noun, 0.5F, "中", false, true, false, CT::None, ""},
      // 時: time - prevent 時妙な concatenation (N5)
      {"時", POS::Noun, 0.5F, "時", false, true, false, CT::None, ""},
  };
}

// =============================================================================
// Low Info Words (低情報量語)
// =============================================================================
std::vector<DictionaryEntry> getLowInfoEntries() {
  return {
      // Most ichidan renyokei stems removed - handled by verb_candidates.cpp ichidan_stem_rare pattern
      // 伝え/伝える, 届け/届ける, 答え/答える, 教え/教える, 認め/認める, 見せ/見せる, 帯び/帯びる
      // Keep causative patterns (知らせ, 聞かせ) - kanji+ら+せ not handled by E/I row pattern
      {"知らせ", POS::Verb, 0.3F, "知らせる", false, false, false, CT::Ichidan, ""},  // 知らせる renyokei
      {"聞かせ", POS::Verb, 0.5F, "聞かせる", false, false, false, CT::Ichidan, ""},
      // Katakana ichidan verbs (カタカナ一段動詞) - often mistaken for suru-verbs
      {"バレる", POS::Verb, 0.5F, "バレる", false, false, false, CT::Ichidan, ""},
      {"バレた", POS::Verb, 0.5F, "バレる", false, false, false, CT::Ichidan, ""},  // past form
      {"待ち", POS::Verb, 0.5F, "待つ", false, false, false, CT::GodanTa, ""},
      {"願い", POS::Verb, 0.5F, "願う", false, false, false, CT::GodanWa, ""},

      // Nominalized verbs (連用形名詞) - prevent misanalysis as adjective
      {"支払い", POS::Noun, 0.3F, "支払い", false, false, false, CT::None, "しはらい"},
      {"払い", POS::Noun, 0.5F, "払い", false, false, false, CT::None, "はらい"},
      {"見舞い", POS::Noun, 0.3F, "見舞い", false, false, false, CT::None, "みまい"},

      // Counter units (助数詞につく名詞) - enable 何+番線 split
      {"番線", POS::Noun, -0.5F, "番線", false, false, false, CT::None, "ばんせん"},

      // Low information verbs (低情報量動詞)
      {"ある", POS::Verb, 2.0F, "ある", false, false, true, CT::GodanRa, ""},
      {"いる", POS::Verb, 2.0F, "いる", false, false, true, CT::Ichidan, ""},
      {"おる", POS::Verb, 2.0F, "おる", false, false, true, CT::GodanRa, ""},
      {"くる", POS::Verb, 2.0F, "くる", false, false, true, CT::Kuru, ""},
      {"いく", POS::Verb, 2.0F, "いく", false, false, true, CT::GodanKa, ""},
      {"くれる", POS::Verb, 2.0F, "くれる", false, false, true, CT::Ichidan, ""},
      {"あげる", POS::Verb, 2.0F, "あげる", false, false, true, CT::Ichidan, ""},
      {"みる", POS::Verb, 2.0F, "みる", false, false, true, CT::Ichidan, ""},
      {"おく", POS::Verb, 2.0F, "おく", false, false, true, CT::GodanKa, ""},
      {"しまう", POS::Verb, 2.0F, "しまう", false, false, true, CT::GodanWa, ""},
      {"しめる", POS::Verb, 0.4F, "しめる", false, false, false, CT::Ichidan, ""},  // 占める/締める/閉める - prevent し+めた split

      // Common GodanWa/Ra/Ta verbs are defined in getEssentialVerbEntries() with readings
      {"間違う", POS::Verb, 0.5F, "間違う", false, false, false, CT::GodanWa, "まちがう"},
      {"揃う", POS::Verb, 1.0F, "揃う", false, false, false, CT::GodanWa, "そろう"},

      // Regular i-adjective よい (conjugated forms auto-generated by expandAdjectiveEntry)
      {"よい", POS::Adjective, 0.3F, "よい", false, false, false, CT::IAdjective, "よい"},

      // Prefixes (接頭辞)
      {"お", POS::Prefix, 0.3F, "", true, false, false, CT::None, ""},
      {"ご", POS::Prefix, 0.3F, "", true, false, false, CT::None, ""},
      {"御", POS::Prefix, 1.0F, "", true, false, false, CT::None, ""},
      {"何", POS::Prefix, 0.3F, "なん", true, false, false, CT::None, ""},  // Interrogative prefix (何番線→何+番線)
      // Note: 今 (PREFIX) removed to allow 今日中 etc. as compound nouns

      // Suffixes (接尾語)
      {"的", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"化", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"性", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"率", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"法", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"論", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"者", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"家", POS::Suffix, 2.5F, "か", false, false, true, CT::None, ""},
      {"員", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"式", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"感", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"力", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"度", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"線", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"行き", POS::Suffix, 0.5F, "いき", false, false, true, CT::None, ""},
      {"行", POS::Suffix, 1.5F, "ゆき", false, false, true, CT::None, ""},
      // Medical suffixes (医療接尾辞)
      {"病", POS::Suffix, 1.5F, "びょう", false, false, true, CT::None, ""},
      {"症", POS::Suffix, 1.5F, "しょう", false, false, true, CT::None, ""},

      // Plural suffixes (複数接尾語)
      {"たち", POS::Suffix, 0.5F, "たち", false, false, true, CT::None, "たち"},
      {"ら", POS::Suffix, 0.5F, "ら", false, false, true, CT::None, "ら"},
      {"ども", POS::Suffix, 0.8F, "ども", false, false, true, CT::None, "ども"},
      {"がた", POS::Suffix, 0.8F, "がた", false, false, true, CT::None, "がた"},
      // NOTE: がち suffix is now handled by generateGachiSuffixCandidates() and
      // generateProductiveSuffixCandidates() - no dictionary entry needed

      // Temporal suffixes (時間接尾語)
      {"ごろ", POS::Suffix, -2.5F, "", false, false, true, CT::None, ""},
      {"頃", POS::Suffix, -2.5F, "", false, false, true, CT::None, ""},

      // Time period suffixes (時間接尾語)
      // 末: end of (月末, 年末, 週末)
      {"末", POS::Suffix, 0.0F, "まつ", false, false, true, CT::None, ""},

      // Completive/Distributive suffixes (全体・分配接尾語)
      // ごと: "together with" (皮ごと食べる), "every" (日ごと)
      {"ごと", POS::Suffix, 0.3F, "ごと", false, false, true, CT::None, ""},
      {"ごとに", POS::Suffix, 0.3F, "ごと", false, false, true, CT::None, ""},

      // Honorific suffixes (敬称接尾語)
      {"さん", POS::Suffix, 0.5F, "", false, false, true, CT::None, ""},
      {"様", POS::Suffix, 0.5F, "", false, false, true, CT::None, ""},
      {"ちゃん", POS::Suffix, 0.5F, "", false, false, true, CT::None, ""},
      {"くん", POS::Suffix, 0.5F, "", false, false, true, CT::None, ""},
      {"氏", POS::Suffix, 0.8F, "", false, false, true, CT::None, ""},

      // Place/organization suffixes (場所・組織接尾語)
      // 店: shop/store suffix - prevent 店原宿店 over-concatenation
      // Also add NOUN entry for cases like "いつもの店"
      {"店", POS::Suffix, 0.3F, "てん", false, false, true, CT::None, ""},
      // Human counter suffix (人数接尾語)
      // 人: person counter - SUFFIX with higher cost to let NOUN win after VERB/DET
      // Cost 0.8F: VERB→SUFFIX(1.2)+0.8=2.0 loses to VERB→NOUN(0.2)+unknown(1.4)=1.6
      // NOUN→SUFFIX(-0.8)+0.8=0.0 wins for counter patterns (3人)
      {"人", POS::Suffix, 0.8F, "にん", false, false, true, CT::None, ""},

      // Occurrence counter suffix (回数接尾語)
      // 回: times/occurrences - 第一回, 3回, 何回
      {"回", POS::Suffix, 0.5F, "かい", false, false, true, CT::None, ""},

      // Counter suffixes with ヶ (助数詞接尾語)
      // ヶ is read as か in counters (箇の略字)
      {"ヶ月", POS::Suffix, 0.3F, "ヶ月", false, false, true, CT::None, "かげつ"},
      {"ヶ国", POS::Suffix, 0.3F, "ヶ国", false, false, true, CT::None, "かこく"},
      {"ヶ所", POS::Suffix, 0.3F, "ヶ所", false, false, true, CT::None, "かしょ"},
      {"ヶ年", POS::Suffix, 0.5F, "ヶ年", false, false, true, CT::None, "かねん"},

      // Descriptive suffixes (記述接尾語) - B-2
      // だらけ: covered with - prevent だ｜ら｜け split
      {"だらけ", POS::Suffix, -0.5F, "だらけ", false, false, true, CT::None, ""},
      // ずつ/づつ: each/at a time - prevent OTHER confusion
      {"ずつ", POS::Suffix, -0.5F, "ずつ", false, false, true, CT::None, ""},
      {"づつ", POS::Suffix, -0.5F, "ずつ", false, false, true, CT::None, ""},  // variant spelling
  };
}

// =============================================================================
// Greetings (挨拶)
// =============================================================================
std::vector<DictionaryEntry> getGreetingEntries() {
  return {
      // Basic Greetings (基本挨拶)
      {"こんにちは", POS::Other, 0.3F, "こんにちは", false, false, false, CT::None, ""},
      {"こんばんは", POS::Other, 0.3F, "こんばんは", false, false, false, CT::None, ""},
      {"おはよう", POS::Other, 0.3F, "おはよう", false, false, false, CT::None, ""},
      {"おやすみ", POS::Other, 0.3F, "おやすみ", false, false, false, CT::None, ""},
      {"さようなら", POS::Other, 0.3F, "さようなら", false, false, false, CT::None, ""},

      // Thanks and Apologies (感謝・謝罪)
      {"ありがとう", POS::Other, 0.3F, "ありがとう", false, false, false, CT::None, ""},
      {"すみません", POS::Other, 0.3F, "すみません", false, false, false, CT::None, ""},
      {"ごめんなさい", POS::Other, 0.3F, "ごめんなさい", false, false, false, CT::None, ""},
      {"ごめん", POS::Other, 0.3F, "ごめん", false, false, false, CT::None, ""},

      // Interjections (感動詞) - B-6
      // はてな: what?/hmm - prevent は+て+な split (N8)
      {"はてな", POS::Other, -0.5F, "はてな", false, false, false, CT::None, ""},
  };
}

// =============================================================================
// Adverbs (副詞)
// =============================================================================
std::vector<DictionaryEntry> getAdverbEntries() {
  return {
      // Degree adverbs (程度副詞) - kanji with reading
      {"大変", POS::Adverb, -1.0F, "", false, false, false, CT::None, "たいへん"},
      {"全く", POS::Adverb, 0.5F, "", false, false, false, CT::None, "まったく"},
      {"全然", POS::Adverb, 0.5F, "", false, false, false, CT::None, "ぜんぜん"},
      {"少し", POS::Adverb, 0.5F, "", false, false, false, CT::None, "すこし"},
      {"結構", POS::Adverb, 0.5F, "", false, false, false, CT::None, "けっこう"},
      // Degree adverbs - hiragana only
      // Note: とても needs low cost to prevent と+て+も split
      {"とても", POS::Adverb, 0.1F, "", false, false, false, CT::None, ""},
      {"すごく", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"めっちゃ", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"かなり", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"たくさん", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"沢山", POS::Adverb, 0.3F, "", false, false, false, CT::None, "たくさん"},
      {"もっと", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"ずっと", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"さらに", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"まさに", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"あまり", POS::Adverb, 0.2F, "", false, false, false, CT::None, ""},  // Lower cost to prefer ADV over verb あまる
      {"なかなか", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"ほとんど", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"ちょっと", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},

      // Manner adverbs (様態副詞)
      {"ゆっくり", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"しっかり", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"はっきり", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"きちんと", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"ちゃんと", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},

      // Demonstrative adverbs (指示副詞)
      {"そう", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      // Appearance auxiliary そう (様態助動詞) - VERB連用形 + そう
      // E.g., 降りそう (looks like it will fall), 切れそう (looks like it will break)
      // Cost balanced so that:
      // - VERB→そう(AUX) wins (VERB→AUX conn=0, total lower than VERB→ADV conn=0.3)
      // - PARTICLE→そう(ADV) wins (PARTICLE→ADV conn=0.3 + ADV cost 0.5 < PARTICLE→AUX conn=0.5 + AUX cost 0.6)
      {"そう", POS::Auxiliary, 1.0F, "そう", false, false, false, CT::None, ""},
      {"こう", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"ああ", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"どう", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      // Distributive adverbs (分配副詞)
      {"それぞれ", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"おのおの", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      // Interrogative adverbs (疑問副詞)
      {"どうして", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"どうしても", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"どうにか", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"どうも", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},

      // Time adverbs (時間副詞)
      {"すぐ", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"すぐに", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      // 今すぐ needs low cost to beat 今(PREFIX) + すぐ(ADV) split
      {"今すぐ", POS::Adverb, -1.0F, "", false, false, false, CT::None, "いますぐ"},
      {"まだ", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"もう", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      // のち (後): temporal noun meaning "after/later", common in weather forecasts
      // (晴れのち曇り = sunny, later cloudy). Low cost to prevent の+ち split.
      {"のち", POS::Noun, 0.1F, "後", false, false, false, CT::None, ""},
      {"やっと", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"ようやく", POS::Adverb, 0.1F, "", false, false, false, CT::None, ""},  // Low cost to prevent よう+やく split
      {"ついに", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"いつも", POS::Adverb, 0.0F, "", false, false, false, CT::None, ""},
      {"たまに", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"よく", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"たびたび", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},

      // Conditional adverbs (条件副詞)
      {"もし", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"もしも", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"たとえ", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"仮に", POS::Adverb, 0.5F, "", false, false, false, CT::None, "かりに"},
      {"万一", POS::Adverb, 0.5F, "", false, false, false, CT::None, "まんいち"},

      // Contrast adverbs (対比副詞)
      {"むしろ", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},

      // Colloquial degree adverbs (口語程度副詞)
      {"どんだけ", POS::Adverb, 0.3F, "どれだけ", false, false, false, CT::None, ""},
      {"あんだけ", POS::Adverb, 0.3F, "あれだけ", false, false, false, CT::None, ""},
      {"こんだけ", POS::Adverb, 0.3F, "これだけ", false, false, false, CT::None, ""},
      {"そんだけ", POS::Adverb, 0.3F, "それだけ", false, false, false, CT::None, ""},

      // Emphatic adverbs (強調副詞)
      {"一際", POS::Adverb, 0.3F, "", false, false, false, CT::None, "ひときわ"},
      {"俄然", POS::Adverb, 0.3F, "", false, false, false, CT::None, "がぜん"},  // B53: suddenly/all of a sudden
      // Degree/manner adverbs (程度副詞)
      // 別段: "particularly/especially" (often with negative), e.g., 別段恐ろしくない
      {"別段", POS::Adverb, 0.1F, "", false, false, false, CT::None, "べつだん"},
      {"多分", POS::Adverb, 0.1F, "", false, false, false, CT::None, "たぶん"},
      {"随分", POS::Adverb, 0.1F, "", false, false, false, CT::None, "ずいぶん"},
      {"大分", POS::Adverb, 0.1F, "", false, false, false, CT::None, "だいぶ"},
      // とんと: "at all/completely" (with negative), e.g., とんと分からない
      // Low cost to prevent と+ん+と particle split
      {"とんと", POS::Adverb, 0.1F, "", false, false, false, CT::None, ""},
      // てっきり: "surely/certainly" (mistaken assumption), e.g., てっきり来ると思った
      {"てっきり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      // からっきし: "not at all" (with negative), e.g., からっきし駄目だ
      {"からっきし", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},

      // Superlative adverbs (最上級副詞)
      {"一番", POS::Adverb, 0.3F, "", false, false, false, CT::None, "いちばん"},

      // Affirmation/Negation adverbs - kanji with reading
      {"必ず", POS::Adverb, 0.5F, "", false, false, false, CT::None, "かならず"},
      {"決して", POS::Adverb, 0.5F, "", false, false, false, CT::None, "けっして"},
      // Affirmation/Negation - hiragana only
      {"きっと", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"たぶん", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"おそらく", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},

      // Other common adverbs - kanji with reading
      {"実は", POS::Adverb, 0.5F, "", false, false, false, CT::None, "じつは"},
      {"特に", POS::Adverb, 0.5F, "", false, false, false, CT::None, "とくに"},
      {"主に", POS::Adverb, 0.5F, "", false, false, false, CT::None, "おもに"},
      // Other - hiragana only
      {"やはり", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"やっぱり", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},

      // Sequence adverbs (順序副詞)
      {"まず", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"先ず", POS::Adverb, 0.3F, "", false, false, false, CT::None, "まず"},

      // Formal/Business adverbs (敬語・ビジネス)
      {"何卒", POS::Adverb, 0.3F, "", false, false, false, CT::None, "なにとぞ"},
      {"誠に", POS::Adverb, 0.3F, "", false, false, false, CT::None, "まことに"},
      {"甚だ", POS::Adverb, 0.5F, "", false, false, false, CT::None, "はなはだ"},
      {"恐縮", POS::Noun, 0.3F, "", false, false, false, CT::None, "きょうしゅく"},

      // Onomatopoeia/Mimetic adverbs (オノマトペ・擬態語副詞) - B61/B62
      // These have opaque etymology and cannot be inferred from patterns
      {"せっかく", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"うっかり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"すっかり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"ぴったり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"さっぱり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"あっさり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"こっそり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"ぐっすり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"めっきり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"がっかり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"びっくり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"ちゃっかり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"のんびり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"ぼんやり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"どっさり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"どさり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},  // ABり型: 重い物が落ちる音
      {"ばったり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"ばたり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},  // ABり型: 倒れる音
      {"がたり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},  // ABり型: 硬い物がぶつかる音
      {"きっちり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"ぎっしり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"じっくり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"すっきり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"ばっちり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"ざっくり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"たっぷり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"ちょっぴり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"こってり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"ふっくら", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"ぽっかり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"むっつり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      // B36: 思わず is a lexicalized adverb (involuntarily/unconsciously)
      {"思わず", POS::Adverb, 0.3F, "", false, false, false, CT::None, "おもわず"},
      // B40: とっとと is an emphatic adverb (quickly/hurry up)
      {"とっとと", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},

      // D1: High-priority adverbs with opaque etymology (prone to missplit)
      // Confirmation/Understanding
      {"なるほど", POS::Adverb, 0.3F, "成程", false, false, false, CT::None, ""},  // I see/indeed
      {"もちろん", POS::Adverb, 0.3F, "勿論", false, false, false, CT::None, ""},  // of course
      // State/Degree
      {"もはや", POS::Adverb, 0.3F, "最早", false, false, false, CT::None, ""},  // no longer/already
      {"はるかに", POS::Adverb, 0.3F, "遥かに", false, false, false, CT::None, ""},  // far/by far
      {"まるで", POS::Adverb, 0.3F, "丸で", false, false, false, CT::None, ""},  // as if/completely
      {"とっても", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},  // very (colloquial)
      {"だんだん", POS::Adverb, 0.3F, "段々", false, false, false, CT::None, ""},  // gradually
      // Surprise/Unexpectedness
      {"まさか", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},  // no way/surely not
      {"いきなり", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},  // suddenly
      {"ふと", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},  // suddenly/casually
      {"たちまち", POS::Adverb, 0.3F, "忽ち", false, false, false, CT::None, ""},  // immediately
      // Hypothetical/Estimation
      {"もしかして", POS::Adverb, 0.3F, "若しかして", false, false, false, CT::None, ""},  // perhaps
      {"ひとまず", POS::Adverb, 0.3F, "一先ず", false, false, false, CT::None, ""},  // for now
      {"とりあえず", POS::Adverb, 0.3F, "取り敢えず", false, false, false, CT::None, ""},  // for the time being
      // Volition/Attitude
      {"ぜひ", POS::Adverb, 0.3F, "是非", false, false, false, CT::None, ""},  // by all means
      {"あくまで", POS::Adverb, 0.3F, "飽くまで", false, false, false, CT::None, ""},  // to the end
      {"なるべく", POS::Adverb, 0.3F, "成る可く", false, false, false, CT::None, ""},  // as much as possible
      // Manner
      {"なんとなく", POS::Adverb, 0.3F, "何となく", false, false, false, CT::None, ""},  // somehow
      {"いかにも", POS::Adverb, 0.3F, "如何にも", false, false, false, CT::None, ""},  // indeed/truly
      {"かえって", POS::Adverb, 0.3F, "却って", false, false, false, CT::None, ""},  // on the contrary
      {"すでに", POS::Adverb, 0.3F, "既に", false, false, false, CT::None, ""},  // already
      {"つねに", POS::Adverb, 0.3F, "常に", false, false, false, CT::None, ""},  // always
      {"ただちに", POS::Adverb, 0.3F, "直ちに", false, false, false, CT::None, ""},  // immediately
      // Other
      {"ただ", POS::Adverb, 0.3F, "只", false, false, false, CT::None, ""},  // just/only
      {"とにかく", POS::Adverb, 0.3F, "兎に角", false, false, false, CT::None, ""},  // anyway
      {"とりわけ", POS::Adverb, 0.3F, "取り分け", false, false, false, CT::None, ""},  // especially
      {"かろうじて", POS::Adverb, 0.3F, "辛うじて", false, false, false, CT::None, ""},  // barely
      {"あらためて", POS::Adverb, 0.3F, "改めて", false, false, false, CT::None, ""},  // again/anew
      {"あいかわらず", POS::Adverb, 0.3F, "相変わらず", false, false, false, CT::None, ""},  // as usual
      {"さっそく", POS::Adverb, 0.3F, "早速", false, false, false, CT::None, ""},  // immediately

      // D2: Medium-priority adverbs (less commonly missplit)
      {"いざ", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},  // when it comes to
      {"やたら", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},  // randomly/excessively
      {"およそ", POS::Adverb, 0.3F, "凡そ", false, false, false, CT::None, ""},  // roughly/approximately
      {"やがて", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},  // before long/soon
      {"つい", POS::Adverb, 1.5F, "", false, false, false, CT::None, ""},  // unintentionally (higher cost to avoid に+つい+て split)
      {"いまだ", POS::Adverb, 0.3F, "未だ", false, false, false, CT::None, ""},  // still/not yet
      {"しばしば", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},  // often
      {"ともあれ", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},  // anyway
      {"ふんだん", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},  // abundantly
      {"のちほど", POS::Adverb, 0.3F, "後程", false, false, false, CT::None, ""},  // later
      {"いちはやく", POS::Adverb, 0.3F, "いち早く", false, false, false, CT::None, ""},  // quickly/first
      {"とかく", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},  // apt to/prone to
      {"あらかじめ", POS::Adverb, 0.3F, "予め", false, false, false, CT::None, ""},  // in advance
      {"おおむね", POS::Adverb, 0.3F, "概ね", false, false, false, CT::None, ""},  // mostly
      {"わりと", POS::Adverb, 0.3F, "割と", false, false, false, CT::None, ""},  // relatively
      {"あいにく", POS::Adverb, 0.3F, "生憎", false, false, false, CT::None, ""},  // unfortunately
      {"たいてい", POS::Adverb, 0.3F, "大抵", false, false, false, CT::None, ""},  // usually
      {"なんだか", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},  // somehow
      {"あれこれ", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},  // this and that
      {"なおさら", POS::Adverb, 0.3F, "尚更", false, false, false, CT::None, ""},  // all the more
      {"なにげなく", POS::Adverb, 0.3F, "何気なく", false, false, false, CT::None, ""},  // casually
      {"ひょっとして", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},  // possibly
      {"どうやら", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},  // apparently
      {"いっさい", POS::Adverb, 0.3F, "一切", false, false, false, CT::None, ""},  // at all
      {"ひたすら", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},  // earnestly
      {"あえて", POS::Adverb, 0.3F, "敢えて", false, false, false, CT::None, ""},  // daringly
      {"どうせ", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},  // anyway/after all
      {"いっそ", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},  // rather
      {"せいぜい", POS::Adverb, 0.3F, "精々", false, false, false, CT::None, ""},  // at most
      {"つくづく", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},  // deeply/keenly
      // Onomatopoeia/mimetic words (prevent particle splits)
      {"ほんわか", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},  // gently warm
      {"しどろもどろ", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},  // incoherent

      // Livedoor corpus fixes: closed-class adverbs mistokenized as OTHER
      {"いずれ", POS::Adverb, 0.1F, "", false, false, false, CT::None, ""},  // eventually/either
      {"早くも", POS::Adverb, -1.5F, "", false, false, false, CT::None, "はやくも"},  // already/so soon

      // Additional adverbs from comprehensive fix plan (B-1)
      {"全て", POS::Adverb, 0.1F, "", false, false, false, CT::None, "すべて"},  // all - prevent 全｜て split
      {"すべて", POS::Adverb, 0.1F, "全て", false, false, false, CT::None, ""},  // all (hiragana)
      {"実に", POS::Adverb, -0.5F, "", false, false, false, CT::None, "じつに"},  // indeed - prevent 実｜に split (N7)
      {"最も", POS::Adverb, 0.1F, "", false, false, false, CT::None, "もっとも"},  // most - prevent 最｜も split
      {"もっとも", POS::Adverb, 0.1F, "最も", false, false, false, CT::None, ""},  // most (hiragana)
      {"なんとも", POS::Adverb, 0.1F, "", false, false, false, CT::None, ""},  // cannot - prevent な｜ん｜と｜も split
      {"今や", POS::Adverb, -0.5F, "", false, false, false, CT::None, "いまや"},  // now - prevent 今+や(OTHER)
      {"初めて", POS::Adverb, -0.5F, "", false, false, false, CT::None, "はじめて"},  // first time - prevent VERB confusion
      {"到底", POS::Adverb, 0.1F, "", false, false, false, CT::None, "とうてい"},  // utterly - prevent NOUN confusion (N11)

      // Compound adverbs (複合副詞) - functional expressions
      // その上: "moreover/in addition" - needs low cost to beat その+上今 compound
      {"その上", POS::Adverb, -0.5F, "", false, false, false, CT::None, "そのうえ"},
      // その後: "after that" - needs low cost to beat その+後猫 compound
      {"その後", POS::Adverb, -0.5F, "", false, false, false, CT::None, "そのあと"},
      // 第一: "first of all/firstly" - adverbial usage in classical/formal text
      // Low cost (-0.5) to split patterns like 第一毛. Compounds like 第一回 are
      // protected by their dictionary entries or unknown compound generation.
      {"第一", POS::Adverb, -0.5F, "", false, false, false, CT::None, "だいいち"},

      // Closed-class adverbs prone to missplit (工程3)
      // なにしろ: "anyway, at any rate" - prevent なに+しろ(VERB) split
      // Very low cost needed because なに(PRON) + しろ(VERB) is a strong parse
      {"なにしろ", POS::Adverb, -1.5F, "何しろ", false, false, false, CT::None, ""},
      {"何しろ", POS::Adverb, -1.5F, "", false, false, false, CT::None, "なにしろ"},
      // あたかも: "as if, just like" - prevent あた+か+も split
      {"あたかも", POS::Adverb, 0.1F, "恰も", false, false, false, CT::None, ""},
      {"恰も", POS::Adverb, 0.1F, "", false, false, false, CT::None, "あたかも"},
      // さながら: "just like, as though" - prevent さ+ながら split
      {"さながら", POS::Adverb, 0.1F, "宛ら", false, false, false, CT::None, ""},
      // もしかすると: "perhaps, maybe" - prevent も+しか+する+と split
      {"もしかすると", POS::Adverb, 0.1F, "", false, false, false, CT::None, ""},
      // ちっとも: "not at all" (with negative) - prevent ちっ+と+も split
      {"ちっとも", POS::Adverb, 0.1F, "", false, false, false, CT::None, ""},
      // 大いに: "greatly, very much" - prevent 大い(NOUN)+に split
      {"大いに", POS::Adverb, 0.1F, "", false, false, false, CT::None, "おおいに"},
      // 今更 / いまさら: "at this late hour" - prevent いまさ(ADJ)+ら split
      {"今更", POS::Adverb, 0.1F, "", false, false, false, CT::None, "いまさら"},
      {"いまさら", POS::Adverb, 0.1F, "今更", false, false, false, CT::None, ""},
      // 咄嗟に / とっさに: "instantly, reflexively" - prevent とっさ(ADJ)+に split
      {"咄嗟に", POS::Adverb, 0.1F, "", false, false, false, CT::None, "とっさに"},
      {"とっさに", POS::Adverb, 0.1F, "咄嗟に", false, false, false, CT::None, ""},
      // 頻りに / しきりに: "frequently, incessantly" - prevent しきり(VERB)+に split
      {"頻りに", POS::Adverb, 0.1F, "", false, false, false, CT::None, "しきりに"},
      {"しきりに", POS::Adverb, 0.1F, "頻りに", false, false, false, CT::None, ""},
  };
}

// =============================================================================
// Na-Adjectives (形容動詞)
// =============================================================================
std::vector<DictionaryEntry> getNaAdjectiveEntries() {
  return {
      // Common na-adjectives - kanji with reading
      {"新た", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "あらた"},
      {"明らか", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "あきらか"},
      {"静か", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "しずか"},
      {"綺麗", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "きれい"},
      {"大切", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "たいせつ"},
      {"大事", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "だいじ"},
      {"必要", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "ひつよう"},
      {"簡単", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "かんたん"},
      {"丁寧", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "ていねい"},
      {"正確", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "せいかく"},
      // NOTE: 自然 removed - 自然言語処理 等の複合語保持を優先
      {"普通", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "ふつう"},
      {"特別", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "とくべつ"},
      {"便利", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "べんり"},
      {"不便", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "ふべん"},
      {"有名", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "ゆうめい"},
      {"複雑", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "ふくざつ"},
      {"単純", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "たんじゅん"},
      {"重要", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "じゅうよう"},
      {"確か", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "たしか"},
      {"様々", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "さまざま"},
      {"勝手", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "かって"},
      {"慎重", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "しんちょう"},
      {"上手", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "じょうず"},

      // Additional na-adjectives for adverb forms
      {"非常", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "ひじょう"},
      {"本当", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "ほんとう"},
      {"頻繁", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "ひんぱん"},
      {"確実", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "かくじつ"},
      {"同様", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "どうよう"},
      {"大胆", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "だいたん"},
      {"果敢", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "かかん"},
      {"無理", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "むり"},
      {"強引", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "ごういん"},
      {"地味", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "じみ"},
      {"微妙", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "びみょう"},
      {"唐突", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "とうとつ"},
      {"永遠", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "えいえん"},
      {"永久", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "えいきゅう"},
      {"無限", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "むげん"},
      {"滅多", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "めった"},
      {"一緒", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "いっしょ"},

      // Single-kanji na-adjectives (単漢字形容動詞)
      // These need explicit entries to avoid single_kanji penalty (+2.0)
      // 妙: "strange/mysterious" - common in classical text (この時妙なものだと思った)
      {"妙", POS::Adjective, 0.3F, "", false, false, false, CT::NaAdjective, "みょう"},

      // Adverbial na-adjectives
      {"流石", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "さすが"},

      // Hiragana na-adjectives
      {"だめ", POS::Adjective, 0.3F, "だめ", false, false, false, CT::NaAdjective, ""},
      // NOTE: Higher cost (0.6) to prevent "読みたい" from being parsed as 読+みたい
      // instead of 読み+たい (want to read). Legitimate uses like "猫みたい" still work
      // because 猫 as dictionary NOUN has lower cost than 読 as unknown NOUN.
      {"みたい", POS::Adjective, 0.6F, "みたい", false, false, false, CT::NaAdjective, ""},

      // Business/formal na-adjectives
      {"幸い", POS::Adjective, 0.3F, "", false, false, false, CT::NaAdjective, "さいわい"},

      // Common na-adjectives - emotional/preference
      {"好き", POS::Adjective, 0.3F, "", false, false, false, CT::NaAdjective, "すき"},
      {"嫌い", POS::Adjective, 0.3F, "", false, false, false, CT::NaAdjective, "きらい"},
      {"大好き", POS::Adjective, 0.3F, "", false, false, false, CT::NaAdjective, "だいすき"},
      {"大嫌い", POS::Adjective, 0.3F, "", false, false, false, CT::NaAdjective, "だいきらい"},
  };
}

// =============================================================================
// I-Adjectives (形容詞)
// =============================================================================
std::vector<DictionaryEntry> getIAdjectiveEntries() {
  return {
      // Existential adjective 無い (non-existence) - kanji form only
      // Hiragana ない is already registered as negation auxiliary, no reading expansion
      {"無い", POS::Adjective, 0.3F, "無い", false, false, false, CT::IAdjective, ""},

      // Temperature/Sensation
      {"寒い", POS::Adjective, 0.3F, "寒い", false, false, false, CT::IAdjective, "さむい"},
      {"暑い", POS::Adjective, 0.3F, "暑い", false, false, false, CT::IAdjective, "あつい"},
      {"熱い", POS::Adjective, 0.3F, "熱い", false, false, false, CT::IAdjective, "あつい"},
      {"痛い", POS::Adjective, 0.3F, "痛い", false, false, false, CT::IAdjective, "いたい"},
      {"怖い", POS::Adjective, 0.3F, "怖い", false, false, false, CT::IAdjective, "こわい"},
      {"辛い", POS::Adjective, 0.3F, "辛い", false, false, false, CT::IAdjective, "からい"},
      {"甘い", POS::Adjective, 0.3F, "甘い", false, false, false, CT::IAdjective, "あまい"},
      {"苦い", POS::Adjective, 0.3F, "苦い", false, false, false, CT::IAdjective, "にがい"},
      {"酸い", POS::Adjective, 0.3F, "酸い", false, false, false, CT::IAdjective, "すい"},
      {"臭い", POS::Adjective, 0.3F, "臭い", false, false, false, CT::IAdjective, "くさい"},
      {"眠い", POS::Adjective, 0.3F, "眠い", false, false, false, CT::IAdjective, "ねむい"},
      {"硬い", POS::Adjective, 0.3F, "硬い", false, false, false, CT::IAdjective, "かたい"},

      // Colors
      {"赤い", POS::Adjective, 0.3F, "赤い", false, false, false, CT::IAdjective, "あかい"},
      {"青い", POS::Adjective, 0.3F, "青い", false, false, false, CT::IAdjective, "あおい"},
      {"白い", POS::Adjective, 0.3F, "白い", false, false, false, CT::IAdjective, "しろい"},
      {"黒い", POS::Adjective, 0.3F, "黒い", false, false, false, CT::IAdjective, "くろい"},

      // Size/Shape
      {"長い", POS::Adjective, 0.3F, "長い", false, false, false, CT::IAdjective, "ながい"},
      {"高い", POS::Adjective, 0.3F, "高い", false, false, false, CT::IAdjective, "たかい"},
      {"低い", POS::Adjective, 0.3F, "低い", false, false, false, CT::IAdjective, "ひくい"},
      {"広い", POS::Adjective, 0.3F, "広い", false, false, false, CT::IAdjective, "ひろい"},
      {"狭い", POS::Adjective, 0.3F, "狭い", false, false, false, CT::IAdjective, "せまい"},
      {"太い", POS::Adjective, 0.3F, "太い", false, false, false, CT::IAdjective, "ふとい"},
      {"細い", POS::Adjective, 0.3F, "細い", false, false, false, CT::IAdjective, "ほそい"},
      {"深い", POS::Adjective, 0.3F, "深い", false, false, false, CT::IAdjective, "ふかい"},
      {"浅い", POS::Adjective, 0.3F, "浅い", false, false, false, CT::IAdjective, "あさい"},
      {"丸い", POS::Adjective, 0.3F, "丸い", false, false, false, CT::IAdjective, "まるい"},
      {"厚い", POS::Adjective, 0.3F, "厚い", false, false, false, CT::IAdjective, "あつい"},
      {"薄い", POS::Adjective, 0.3F, "薄い", false, false, false, CT::IAdjective, "うすい"},
      {"重い", POS::Adjective, 0.3F, "重い", false, false, false, CT::IAdjective, "おもい"},
      {"軽い", POS::Adjective, 0.3F, "軽い", false, false, false, CT::IAdjective, "かるい"},
      {"固い", POS::Adjective, 0.3F, "固い", false, false, false, CT::IAdjective, "かたい"},
      {"濃い", POS::Adjective, 0.3F, "濃い", false, false, false, CT::IAdjective, ""},
      {"短い", POS::Adjective, 0.3F, "短い", false, false, false, CT::IAdjective, "みじかい"},

      // Time/Speed
      {"早い", POS::Adjective, 0.3F, "早い", false, false, false, CT::IAdjective, "はやい"},
      {"速い", POS::Adjective, 0.3F, "速い", false, false, false, CT::IAdjective, "はやい"},
      {"遅い", POS::Adjective, 0.3F, "遅い", false, false, false, CT::IAdjective, "おそい"},
      {"近い", POS::Adjective, 0.3F, "近い", false, false, false, CT::IAdjective, "ちかい"},
      {"遠い", POS::Adjective, 0.3F, "遠い", false, false, false, CT::IAdjective, "とおい"},
      {"古い", POS::Adjective, 0.3F, "古い", false, false, false, CT::IAdjective, "ふるい"},
      {"若い", POS::Adjective, 0.3F, "若い", false, false, false, CT::IAdjective, "わかい"},

      // Quality/Evaluation
      {"良い", POS::Adjective, 0.3F, "良い", false, false, false, CT::IAdjective, "よい"},
      {"悪い", POS::Adjective, 0.3F, "悪い", false, false, false, CT::IAdjective, "わるい"},
      {"強い", POS::Adjective, 0.3F, "強い", false, false, false, CT::IAdjective, "つよい"},
      {"弱い", POS::Adjective, 0.3F, "弱い", false, false, false, CT::IAdjective, "よわい"},
      {"安い", POS::Adjective, 0.3F, "安い", false, false, false, CT::IAdjective, "やすい"},
      {"汚い", POS::Adjective, 0.3F, "汚い", false, false, false, CT::IAdjective, "きたない"},
      {"醜い", POS::Adjective, 0.3F, "醜い", false, false, false, CT::IAdjective, "みにくい"},
      {"鋭い", POS::Adjective, 0.3F, "鋭い", false, false, false, CT::IAdjective, "するどい"},
      {"鈍い", POS::Adjective, 0.3F, "鈍い", false, false, false, CT::IAdjective, "にぶい"},

      // Quantity
      {"多い", POS::Adjective, 0.3F, "多い", false, false, false, CT::IAdjective, "おおい"},

      // Other single-kanji + い
      {"暗い", POS::Adjective, 0.3F, "暗い", false, false, false, CT::IAdjective, "くらい"},
      {"狡い", POS::Adjective, 0.3F, "狡い", false, false, false, CT::IAdjective, "ずるい"},
      {"旨い", POS::Adjective, 0.3F, "旨い", false, false, false, CT::IAdjective, "うまい"},
      {"上手い", POS::Adjective, 0.3F, "上手い", false, false, false, CT::IAdjective, "うまい"},
      {"うまい", POS::Adjective, 0.2F, "うまい", false, false, false, CT::IAdjective, ""},  // neutral lemma for hiragana
      {"尊い", POS::Adjective, 0.3F, "尊い", false, false, false, CT::IAdjective, "とうとい"},

      // Special irregular adjective いい (良い/よい variant)
      // Uses CT::None because いい only has terminal/attributive forms.
      // All conjugated forms use よ- stem (よかった, よくない, etc.), not い-.
      // Auto-expansion would incorrectly generate いかった, いくない, etc.
      {"いい", POS::Adjective, 0.3F, "よい", false, false, false, CT::None, ""},

      // Hiragana-only adjectives (prevent particle/verb misparse)
      // These adjectives have no kanji forms or the kanji form is rare
      {"めでたい", POS::Adjective, 0.3F, "めでたい", false, false, false, CT::IAdjective, ""},  // auspicious
      {"ありがたい", POS::Adjective, 0.3F, "ありがたい", false, false, false, CT::IAdjective, ""},  // grateful
      {"おもしろい", POS::Adjective, 0.3F, "おもしろい", false, false, false, CT::IAdjective, ""},  // interesting
      {"やわらかい", POS::Adjective, 0.3F, "やわらかい", false, false, false, CT::IAdjective, ""},  // soft
      {"あたたかい", POS::Adjective, 0.3F, "あたたかい", false, false, false, CT::IAdjective, ""},  // warm
      {"つめたい", POS::Adjective, 0.3F, "つめたい", false, false, false, CT::IAdjective, ""},  // cold
      {"つまらない", POS::Adjective, 0.3F, "つまらない", false, false, false, CT::IAdjective, ""},  // boring
      {"たまらない", POS::Adjective, 0.3F, "たまらない", false, false, false, CT::IAdjective, ""},  // unbearable
      {"くだらない", POS::Adjective, 0.3F, "くだらない", false, false, false, CT::IAdjective, ""},  // trivial
      {"みっともない", POS::Adjective, 0.3F, "みっともない", false, false, false, CT::IAdjective, ""},  // disgraceful
      {"しょうもない", POS::Adjective, 0.3F, "しょうもない", false, false, false, CT::IAdjective, ""},  // worthless
      {"おとなしい", POS::Adjective, 0.3F, "おとなしい", false, false, false, CT::IAdjective, ""},  // quiet/gentle
      {"すばらしい", POS::Adjective, 0.3F, "すばらしい", false, false, false, CT::IAdjective, ""},  // wonderful
      // Additional hiragana adjectives (prevent particle/suru misparse)
      {"あぶない", POS::Adjective, 0.3F, "あぶない", false, false, false, CT::IAdjective, ""},  // dangerous
      {"だらしない", POS::Adjective, 0.3F, "だらしない", false, false, false, CT::IAdjective, ""},  // sloppy
      {"ものたりない", POS::Adjective, 0.3F, "ものたりない", false, false, false, CT::IAdjective, ""},  // unsatisfying
      {"いたたまれない", POS::Adjective, 0.3F, "いたたまれない", false, false, false, CT::IAdjective, ""},  // unbearable
      {"かたじけない", POS::Adjective, 0.3F, "かたじけない", false, false, false, CT::IAdjective, ""},  // grateful (archaic)
      {"おびただしい", POS::Adjective, 0.3F, "おびただしい", false, false, false, CT::IAdjective, ""},  // enormous
      {"かまびすしい", POS::Adjective, 0.3F, "かまびすしい", false, false, false, CT::IAdjective, ""},  // noisy
      {"うやうやしい", POS::Adjective, 0.3F, "うやうやしい", false, false, false, CT::IAdjective, ""},  // respectful
      {"こころもとない", POS::Adjective, 0.3F, "こころもとない", false, false, false, CT::IAdjective, ""},  // anxious/uneasy
  };
}

// =============================================================================
// Hiragana Verbs (ひらがな動詞)
// =============================================================================
std::vector<DictionaryEntry> getHiraganaVerbEntries() {
  return {
      // Ichidan verbs (一段動詞)
      // Note: できる is defined in getEssentialVerbEntries() with reading
      {"やめる", POS::Verb, 0.3F, "やめる", false, false, false, CT::Ichidan, ""},
      {"はじめる", POS::Verb, 0.3F, "はじめる", false, false, false, CT::Ichidan, ""},
      // B38: のろける starts with の (particle-like)
      {"のろける", POS::Verb, 0.3F, "のろける", false, false, false, CT::Ichidan, ""},
      // B46: とりあげる starts with と (particle-like)
      {"とりあげる", POS::Verb, 0.3F, "とりあげる", false, false, false, CT::Ichidan, ""},
      // Verbs starting with particle-like chars (prevent し+みる, た+ずねる splits)
      {"しみる", POS::Verb, 0.3F, "しみる", false, false, false, CT::Ichidan, ""},
      {"たずねる", POS::Verb, 0.3F, "たずねる", false, false, false, CT::Ichidan, ""},
      {"おぼれる", POS::Verb, 0.3F, "おぼれる", false, false, false, CT::Ichidan, ""},
      {"そげる", POS::Verb, 0.3F, "そげる", false, false, false, CT::Ichidan, ""},
      {"あきらめる", POS::Verb, 0.3F, "あきらめる", false, false, false, CT::Ichidan, ""},
      {"たばねる", POS::Verb, 0.3F, "たばねる", false, false, false, CT::Ichidan, ""},  // to bundle

      // Godan-Ka verbs (五段カ行)
      // いく/ゆく: fundamental verb meaning "to go"
      // Higher cost to not compete with て-form+いく compound patterns
      // (走っていく, 見ていく should prefer compound form)
      // but still recognized for standalone usage (うまくいく)
      // Explicit conjugated forms needed for proper recognition (e.g., うまくいかなかった)
      {"いく", POS::Verb, 1.2F, "いく", false, false, false, CT::GodanKa, ""},
      {"いかない", POS::Verb, 1.2F, "いく", false, false, false, CT::GodanKa, ""},
      {"いかなかった", POS::Verb, 1.2F, "いく", false, false, false, CT::GodanKa, ""},
      {"いった", POS::Verb, 1.2F, "いく", false, false, false, CT::GodanKa, ""},
      {"いって", POS::Verb, 1.2F, "いく", false, false, false, CT::GodanKa, ""},
      {"いける", POS::Verb, 1.2F, "いく", false, false, false, CT::GodanKa, ""},
      {"いけない", POS::Verb, 1.2F, "いく", false, false, false, CT::GodanKa, ""},
      {"いけなかった", POS::Verb, 1.2F, "いく", false, false, false, CT::GodanKa, ""},
      {"ゆく", POS::Verb, 1.2F, "いく", false, false, false, CT::GodanKa, ""},
      {"ゆかない", POS::Verb, 1.2F, "いく", false, false, false, CT::GodanKa, ""},
      // くる: fundamental verb meaning "to come"
      // Higher cost to not compete with て-form+くる compound patterns
      // (見てくる, 帰ってくる should prefer compound form)
      // but still recognized for standalone usage (気が来る)
      {"くる", POS::Verb, 1.2F, "くる", false, false, false, CT::Kuru, ""},
      {"こない", POS::Verb, 1.2F, "くる", false, false, false, CT::Kuru, ""},
      {"こなかった", POS::Verb, 1.2F, "くる", false, false, false, CT::Kuru, ""},
      {"きた", POS::Verb, 1.2F, "くる", false, false, false, CT::Kuru, ""},
      {"きて", POS::Verb, 1.2F, "くる", false, false, false, CT::Kuru, ""},
      {"きている", POS::Verb, 1.2F, "くる", false, false, false, CT::Kuru, ""},
      {"こられる", POS::Verb, 1.2F, "くる", false, false, false, CT::Kuru, ""},
      {"こられない", POS::Verb, 1.2F, "くる", false, false, false, CT::Kuru, ""},
      // いただく系 (itadaku - to receive, honorific)
      // Uses explicit forms like other honorific verbs (not auto-expanded)
      // This ensures いただきます stays as single token (not split as いただき+ます)
      {"いただく", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
      {"いただいて", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
      {"いただいた", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
      {"いただき", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
      {"いただかない", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
      {"いただきます", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
      {"いただきました", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
      {"いただきまして", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
      {"いただきたい", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
      {"いただける", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
      {"いただけます", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
      {"いただければ", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
      {"いただけない", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
      {"いただけません", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
      {"とく", POS::Verb, 0.3F, "とく", false, false, true, CT::GodanKa, ""},
      {"っとく", POS::Verb, -0.5F, "とく", false, false, true, CT::GodanKa, ""},
      {"てく", POS::Verb, 0.3F, "てく", false, false, true, CT::GodanKa, ""},
      {"ってく", POS::Verb, -0.5F, "てく", false, false, true, CT::GodanKa, ""},
      // Verbs starting with particle-like chars (prevent お+どろく, き+らめく splits)
      {"おどろく", POS::Verb, 0.3F, "おどろく", false, false, false, CT::GodanKa, ""},
      {"きらめく", POS::Verb, 0.3F, "きらめく", false, false, false, CT::GodanKa, ""},
      {"ひらめく", POS::Verb, 0.3F, "ひらめく", false, false, false, CT::GodanKa, ""},
      {"つぶやく", POS::Verb, 0.3F, "つぶやく", false, false, false, CT::GodanKa, ""},
      {"あざむく", POS::Verb, 0.3F, "あざむく", false, false, false, CT::GodanKa, ""},  // to deceive
      {"もがく", POS::Verb, 0.3F, "もがく", false, false, false, CT::GodanKa, ""},  // to struggle

      // Godan-Ra verbs (五段ラ行)
      {"やる", POS::Verb, 0.3F, "やる", false, false, false, CT::GodanRa, ""},
      {"わかる", POS::Verb, 0.3F, "わかる", false, false, false, CT::GodanRa, ""},
      {"なる", POS::Verb, 0.3F, "なる", false, false, false, CT::GodanRa, ""},
      // B48: かかる starts with か (particle-like) - prevent か+かって split
      {"かかる", POS::Verb, 0.3F, "かかる", false, false, false, CT::GodanRa, ""},
      {"できあがる", POS::Verb, 0.3F, "できあがる", false, false, false, CT::GodanRa, ""},
      {"がんばる", POS::Verb, 0.3F, "がんばる", false, false, false, CT::GodanRa, ""},
      // Verbs starting with particle-like chars (prevent し+ゃべる, た+よる splits)
      {"しゃべる", POS::Verb, 0.3F, "しゃべる", false, false, false, CT::GodanRa, ""},
      {"しぼる", POS::Verb, 0.3F, "しぼる", false, false, false, CT::GodanRa, ""},
      {"たよる", POS::Verb, 0.3F, "たよる", false, false, false, CT::GodanRa, ""},
      {"たどる", POS::Verb, 0.3F, "たどる", false, false, false, CT::GodanRa, ""},
      {"かざる", POS::Verb, 0.3F, "かざる", false, false, false, CT::GodanRa, ""},
      {"かぶる", POS::Verb, 0.3F, "かぶる", false, false, false, CT::GodanRa, ""},
      {"めくる", POS::Verb, 0.3F, "めくる", false, false, false, CT::GodanRa, ""},
      {"しまる", POS::Verb, 0.3F, "しまる", false, false, false, CT::GodanRa, ""},
      {"こだわる", POS::Verb, 0.3F, "こだわる", false, false, false, CT::GodanRa, ""},
      {"あなどる", POS::Verb, 0.3F, "あなどる", false, false, false, CT::GodanRa, ""},  // to despise
      {"たたる", POS::Verb, 0.3F, "たたる", false, false, false, CT::GodanRa, ""},  // to curse (祟る)
      {"あおる", POS::Verb, 0.3F, "あおる", false, false, false, CT::GodanRa, ""},  // to incite (煽る)

      // Godan-Wa verbs (五段ワ行)
      {"もらう", POS::Verb, 0.3F, "もらう", false, false, false, CT::GodanWa, ""},
      // Verbs starting with particle-like chars (prevent した+が+う, ため+ら+う splits)
      {"したがう", POS::Verb, 0.3F, "したがう", false, false, false, CT::GodanWa, ""},
      {"ためらう", POS::Verb, 0.3F, "ためらう", false, false, false, CT::GodanWa, ""},
      // うかがう (伺う) - prevent う+か+が+う split
      {"うかがう", POS::Verb, 0.3F, "うかがう", false, false, false, CT::GodanWa, ""},
      {"たわむ", POS::Verb, 0.3F, "たわむ", false, false, false, CT::GodanMa, ""},
      {"たたずむ", POS::Verb, 0.3F, "たたずむ", false, false, false, CT::GodanMa, ""},  // to stand still

      // Godan-Sa verbs (五段サ行)
      {"いたす", POS::Verb, 0.3F, "いたす", false, false, false, CT::GodanSa, ""},
      // Verbs starting with particle-like chars (prevent も+たらす splits)
      {"もたらす", POS::Verb, 0.3F, "もたらす", false, false, false, CT::GodanSa, ""},

      // Godan-Ta verbs (五段タ行) - particle-like starts
      {"たもつ", POS::Verb, 0.3F, "たもつ", false, false, false, CT::GodanTa, ""},

      // Godan-Ga verbs (五段ガ行) - particle-like starts
      {"しのぐ", POS::Verb, 0.3F, "しのぐ", false, false, false, CT::GodanGa, ""},
      // Kanji compound verbs (漢字複合動詞) - common GodanGa patterns
      {"相次ぐ", POS::Verb, 0.3F, "相次ぐ", false, false, false, CT::GodanGa, ""},  // to follow one after another

      // Suru verb (サ変動詞)
      {"する", POS::Verb, 0.5F, "する", false, false, false, CT::Suru, ""},

      // Kuru verb imperative only (カ変動詞命令形)
      {"こい", POS::Verb, 0.2F, "くる", false, false, false, CT::None, ""},

      // Special honorific verbs (特殊敬語動詞)
      // These have irregular conjugation patterns and cannot use automatic expansion.
      //
      // Why manual enumeration is required:
      //   Regular Godan-Ra verbs use 「り」 for renyokei (masu-stem):
      //     わかる → わかり + ます = わかります
      //   Honorific verbs use 「い」 instead (irregular):
      //     くださる → ください + ます = くださいます (NOT くださります)
      //     おっしゃる → おっしゃい + ます = おっしゃいます
      //     いらっしゃる → いらっしゃい + ます = いらっしゃいます
      //     なさる → なさい + ます = なさいます
      //
      // This is a closed set of ~5 verbs in Japanese, so manual enumeration
      // is more practical than implementing a dedicated ConjugationType::Honorific.
      //
      // くださる系 (kudasaru - to give/do for me)
      // Lower cost to prefer VERB over AUX in patterns like ご確認ください
      {"くださる", POS::Verb, 0.1F, "くださる", false, false, false, CT::None, ""},
      {"くださって", POS::Verb, 0.1F, "くださる", false, false, false, CT::None, ""},
      {"くださった", POS::Verb, 0.1F, "くださる", false, false, false, CT::None, ""},
      {"ください", POS::Verb, 0.1F, "くださる", false, false, false, CT::None, ""},
      {"くださらない", POS::Verb, 0.3F, "くださる", false, false, false, CT::None, ""},
      {"くださいます", POS::Verb, 0.3F, "くださる", false, false, false, CT::None, ""},
      {"くださいました", POS::Verb, 0.3F, "くださる", false, false, false, CT::None, ""},
      {"くださいませ", POS::Verb, 0.3F, "くださる", false, false, false, CT::None, ""},
      // おっしゃる系 (ossharu - to say, honorific)
      {"おっしゃる", POS::Verb, 0.3F, "おっしゃる", false, false, false, CT::None, ""},
      {"おっしゃって", POS::Verb, 0.3F, "おっしゃる", false, false, false, CT::None, ""},
      {"おっしゃった", POS::Verb, 0.3F, "おっしゃる", false, false, false, CT::None, ""},
      {"おっしゃい", POS::Verb, 0.3F, "おっしゃる", false, false, false, CT::None, ""},
      {"おっしゃいます", POS::Verb, 0.3F, "おっしゃる", false, false, false, CT::None, ""},
      {"おっしゃいました", POS::Verb, 0.3F, "おっしゃる", false, false, false, CT::None, ""},
      {"おっしゃらない", POS::Verb, 0.3F, "おっしゃる", false, false, false, CT::None, ""},
      // いらっしゃる系 (irassharu - to be/go/come, honorific)
      {"いらっしゃる", POS::Verb, 0.3F, "いらっしゃる", false, false, false, CT::None, ""},
      {"いらっしゃって", POS::Verb, 0.3F, "いらっしゃる", false, false, false, CT::None, ""},
      {"いらっしゃった", POS::Verb, 0.3F, "いらっしゃる", false, false, false, CT::None, ""},
      {"いらっしゃい", POS::Verb, 0.3F, "いらっしゃる", false, false, false, CT::None, ""},
      {"いらっしゃいます", POS::Verb, 0.3F, "いらっしゃる", false, false, false, CT::None, ""},
      {"いらっしゃいました", POS::Verb, 0.3F, "いらっしゃる", false, false, false, CT::None, ""},
      {"いらっしゃらない", POS::Verb, 0.3F, "いらっしゃる", false, false, false, CT::None, ""},
      {"いらっしゃいませ", POS::Verb, 0.3F, "いらっしゃる", false, false, false, CT::None, ""},
      // なさる系 (nasaru - to do, honorific)
      {"なさる", POS::Verb, 0.3F, "なさる", false, false, false, CT::None, ""},
      {"なさって", POS::Verb, 0.3F, "なさる", false, false, false, CT::None, ""},
      {"なさった", POS::Verb, 0.3F, "なさる", false, false, false, CT::None, ""},
      {"なさい", POS::Verb, 0.3F, "なさる", false, false, false, CT::None, ""},
      {"なさいます", POS::Verb, 0.3F, "なさる", false, false, false, CT::None, ""},
      {"なさいました", POS::Verb, 0.3F, "なさる", false, false, false, CT::None, ""},
      {"なさらない", POS::Verb, 0.3F, "なさる", false, false, false, CT::None, ""},
      {"なさいませ", POS::Verb, 0.3F, "なさる", false, false, false, CT::None, ""},
      // ござる系 (gozaru - old-fashioned/ninja speech)
      {"ござる", POS::Verb, 0.3F, "ござる", false, false, false, CT::None, ""},
      // ございます系 (modern polite form, treated as base with AUX)
      {"ございます", POS::Auxiliary, 0.3F, "ございます", false, false, false, CT::None, ""},
      {"ございました", POS::Auxiliary, 0.3F, "ございます", false, false, false, CT::None, ""},
      {"ございません", POS::Auxiliary, 0.3F, "ございます", false, false, false, CT::None, ""},
      {"ございませんでした", POS::Auxiliary, 0.3F, "ございます", false, false, false, CT::None, ""},

      // Nouns derived from verbs
      {"できあがり", POS::Noun, 0.3F, "できあがり", false, false, false, CT::None, ""},
  };
}

// =============================================================================
// Essential Verbs (必須動詞)
// =============================================================================
std::vector<DictionaryEntry> getEssentialVerbEntries() {
  return {
      // Ichidan verbs often confused with Godan
      {"用いる", POS::Verb, 0.3F, "用いる", false, false, false, CT::Ichidan, "もちいる"},
      {"降りる", POS::Verb, 0.3F, "降りる", false, false, false, CT::Ichidan, "おりる"},
      {"出来る", POS::Verb, 0.3F, "出来る", false, false, false, CT::Ichidan, "できる"},
      {"できる", POS::Verb, 0.3F, "できる", false, false, false, CT::Ichidan, "できる"},
      {"上げる", POS::Verb, 0.3F, "上げる", false, false, false, CT::Ichidan, "あげる"},
      {"下げる", POS::Verb, 0.3F, "下げる", false, false, false, CT::Ichidan, "さげる"},
      {"見つける", POS::Verb, 0.3F, "見つける", false, false, false, CT::Ichidan, "みつける"},
      {"見つかる", POS::Verb, 0.3F, "見つかる", false, false, false, CT::GodanRa, "みつかる"},
      {"上がる", POS::Verb, 0.3F, "上がる", false, false, false, CT::GodanRa, "あがる"},
      {"挙がる", POS::Verb, 0.3F, "挙がる", false, false, false, CT::GodanRa, "あがる"},
      {"あがる", POS::Verb, 0.3F, "あがる", false, false, false, CT::GodanRa, "あがる"},
      {"下がる", POS::Verb, 0.3F, "下がる", false, false, false, CT::GodanRa, "さがる"},
      // B26: Colloquial verb with long stem (intrude/meddle)
      {"出しゃばる", POS::Verb, 0.3F, "出しゃばる", false, false, false, CT::GodanRa, "でしゃばる"},
      // B65: 詰まる - past form 詰まった recognized as NOUN without conjugation expansion
      {"詰まる", POS::Verb, 0.5F, "詰まる", false, false, false, CT::GodanRa, "つまる"},

      // Single-kanji Ichidan verbs (単漢字一段動詞)
      // These are critical for correct lemmatization of negation forms
      // Without these, 見ない → 見なう (wrong) instead of 見る (correct)
      {"見る", POS::Verb, 0.3F, "見る", false, false, false, CT::Ichidan, "みる"},
      {"寝る", POS::Verb, 0.3F, "寝る", false, false, false, CT::Ichidan, "ねる"},
      {"着る", POS::Verb, 0.3F, "着る", false, false, false, CT::Ichidan, "きる"},
      {"得る", POS::Verb, 0.3F, "得る", false, false, false, CT::Ichidan, "える"},
      {"出る", POS::Verb, 0.3F, "出る", false, false, false, CT::Ichidan, "でる"},
      {"経る", POS::Verb, 0.3F, "経る", false, false, false, CT::Ichidan, "へる"},
      {"似る", POS::Verb, 0.3F, "似る", false, false, false, CT::Ichidan, "にる"},
      {"煮る", POS::Verb, 0.3F, "煮る", false, false, false, CT::Ichidan, "にる"},
      {"居る", POS::Verb, 0.3F, "居る", false, false, false, CT::Ichidan, "いる"},
      {"射る", POS::Verb, 0.3F, "射る", false, false, false, CT::Ichidan, "いる"},
      {"干る", POS::Verb, 0.3F, "干る", false, false, false, CT::Ichidan, "ひる"},
      {"鋳る", POS::Verb, 0.3F, "鋳る", false, false, false, CT::Ichidan, "いる"},
      // 知れる: for かもしれない decomposition (か + も + しれない)
      {"知れる", POS::Verb, 0.3F, "知れる", false, false, false, CT::Ichidan, "しれる"},
      {"しれる", POS::Verb, 0.3F, "しれる", false, false, false, CT::Ichidan, "しれる"},

      // Common Ichidan verbs with i-row stems (漢字+き/ぎ/り/ち/み)
      // These get ichidan_kanji_i_row_stem penalty without dictionary registration
      {"起きる", POS::Verb, 0.3F, "起きる", false, false, false, CT::Ichidan, "おきる"},
      {"落ちる", POS::Verb, 0.3F, "落ちる", false, false, false, CT::Ichidan, "おちる"},
      {"生きる", POS::Verb, 0.3F, "生きる", false, false, false, CT::Ichidan, "いきる"},
      {"過ぎる", POS::Verb, 0.3F, "過ぎる", false, false, false, CT::Ichidan, "すぎる"},
      {"尽きる", POS::Verb, 0.3F, "尽きる", false, false, false, CT::Ichidan, "つきる"},
      {"浴びる", POS::Verb, 0.3F, "浴びる", false, false, false, CT::Ichidan, "あびる"},

      // Ichidan verbs ending in で (affected by copula_de_pattern penalty)
      {"茹でる", POS::Verb, 0.3F, "茹でる", false, false, false, CT::Ichidan, "ゆでる"},
      {"撫でる", POS::Verb, 0.3F, "撫でる", false, false, false, CT::Ichidan, "なでる"},

      // GodanNa verb - only 死ぬ exists in modern Japanese
      // Critical: Without this, 死ぬ gets misanalyzed as 死(stem)+ぬ(aux)→死る(wrong)
      {"死ぬ", POS::Verb, 0.1F, "死ぬ", false, false, false, CT::GodanNa, "しぬ"},

      // Common Godan verbs (frequently misidentified)
      {"喜ぶ", POS::Verb, 0.3F, "喜ぶ", false, false, false, CT::GodanBa, "よろこぶ"},
      {"学ぶ", POS::Verb, 0.3F, "学ぶ", false, false, false, CT::GodanBa, "まなぶ"},
      {"遊ぶ", POS::Verb, 0.3F, "遊ぶ", false, false, false, CT::GodanBa, "あそぶ"},
      {"飛ぶ", POS::Verb, 0.3F, "飛ぶ", false, false, false, CT::GodanBa, "とぶ"},
      {"呼ぶ", POS::Verb, 0.3F, "呼ぶ", false, false, false, CT::GodanBa, "よぶ"},
      {"選ぶ", POS::Verb, 0.3F, "選ぶ", false, false, false, CT::GodanBa, "えらぶ"},
      {"並ぶ", POS::Verb, 0.3F, "並ぶ", false, false, false, CT::GodanBa, "ならぶ"},
      {"結ぶ", POS::Verb, 0.3F, "結ぶ", false, false, false, CT::GodanBa, "むすぶ"},

      // Godan verbs with special patterns
      {"行う", POS::Verb, 0.5F, "行う", false, false, false, CT::GodanWa, "おこなう"},
      {"伴う", POS::Verb, 0.3F, "伴う", false, false, false, CT::GodanWa, "ともなう"},
      {"伴い", POS::Verb, 0.3F, "伴う", false, false, false, CT::GodanWa, "ともない"},

      // Godan verbs with っ-onbin (促音便) - disambiguation required
      // These verbs share the same っ-onbin pattern, making them ambiguous without dictionary
      {"目立つ", POS::Verb, 0.3F, "目立つ", false, false, false, CT::GodanTa, "めだつ"},
      {"疑う", POS::Verb, 0.3F, "疑う", false, false, false, CT::GodanWa, "うたがう"},
      {"過疎る", POS::Verb, 0.3F, "過疎る", false, false, false, CT::GodanRa, "かそる"},
      // B52: 交う - 飛び交う等で使用、交る/交つ/交うの曖昧性を解消
      {"交う", POS::Verb, 0.3F, "交う", false, false, false, CT::GodanWa, "かう"},
      // B54: 放つ - 言い放つ等で使用、放る/放つの曖昧性を解消
      {"放つ", POS::Verb, 0.3F, "放つ", false, false, false, CT::GodanTa, "はなつ"},

      // Compound verbs (敬語複合動詞)
      {"恐れ入る", POS::Verb, 0.3F, "恐れ入る", false, false, false, CT::GodanRa, "おそれいる"},
      {"申し上げる", POS::Verb, 0.3F, "申し上げる", false, false, false, CT::Ichidan, "もうしあげる"},
      {"差し上げる", POS::Verb, 0.3F, "差し上げる", false, false, false, CT::Ichidan, "さしあげる"},

      // Special Godan verbs with irregular euphonic changes
      // 行く has irregular ta/te forms (行った/行って instead of 行いた/行いて)
      // Base form uses GodanKa expansion, explicit forms use CT::None to prevent re-expansion
      {"行く", POS::Verb, 0.3F, "行く", false, false, false, CT::GodanKa, "いく"},
      {"行った", POS::Verb, 0.1F, "行く", false, false, false, CT::None, "いった"},
      {"行って", POS::Verb, 0.1F, "行く", false, false, false, CT::None, "いって"},

      // Common GodanKa verbs
      {"書く", POS::Verb, 0.3F, "書く", false, false, false, CT::GodanKa, "かく"},
      {"説く", POS::Verb, 0.3F, "説く", false, false, false, CT::GodanKa, "とく"},
      {"聞く", POS::Verb, 0.3F, "聞く", false, false, false, CT::GodanKa, "きく"},
      {"効く", POS::Verb, 0.3F, "効く", false, false, false, CT::GodanKa, "きく"},
      {"利く", POS::Verb, 0.3F, "利く", false, false, false, CT::GodanKa, "きく"},
      {"描く", POS::Verb, 0.3F, "描く", false, false, false, CT::GodanKa, "えがく"},
      {"弾く", POS::Verb, 0.3F, "弾く", false, false, false, CT::GodanKa, "ひく"},
      {"引く", POS::Verb, 0.3F, "引く", false, false, false, CT::GodanKa, "ひく"},
      {"敷く", POS::Verb, 0.3F, "敷く", false, false, false, CT::GodanKa, "しく"},
      {"拭く", POS::Verb, 0.3F, "拭く", false, false, false, CT::GodanKa, "ふく"},
      {"吹く", POS::Verb, 0.3F, "吹く", false, false, false, CT::GodanKa, "ふく"},
      {"置く", POS::Verb, 0.3F, "置く", false, false, false, CT::GodanKa, "おく"},
      {"開く", POS::Verb, 0.3F, "開く", false, false, false, CT::GodanKa, "あく"},
      {"空く", POS::Verb, 0.3F, "空く", false, false, false, CT::GodanKa, "あく"},
      {"着く", POS::Verb, 0.3F, "着く", false, false, false, CT::GodanKa, "つく"},
      {"付く", POS::Verb, 0.3F, "付く", false, false, false, CT::GodanKa, "つく"},
      {"続く", POS::Verb, 0.3F, "続く", false, false, false, CT::GodanKa, "つづく"},
      {"届く", POS::Verb, 0.3F, "届く", false, false, false, CT::GodanKa, "とどく"},
      {"動く", POS::Verb, 0.3F, "動く", false, false, false, CT::GodanKa, "うごく"},
      {"働く", POS::Verb, 0.3F, "働く", false, false, false, CT::GodanKa, "はたらく"},
      {"泣く", POS::Verb, 0.3F, "泣く", false, false, false, CT::GodanKa, "なく"},
      {"鳴く", POS::Verb, 0.3F, "鳴く", false, false, false, CT::GodanKa, "なく"},
      {"咲く", POS::Verb, 0.3F, "咲く", false, false, false, CT::GodanKa, "さく"},
      {"焼く", POS::Verb, 0.3F, "焼く", false, false, false, CT::GodanKa, "やく"},
      {"歩く", POS::Verb, 0.3F, "歩く", false, false, false, CT::GodanKa, "あるく"},
      {"抱く", POS::Verb, 0.3F, "抱く", false, false, false, CT::GodanKa, "だく"},
      {"叩く", POS::Verb, 0.3F, "叩く", false, false, false, CT::GodanKa, "たたく"},
      {"輝く", POS::Verb, 0.3F, "輝く", false, false, false, CT::GodanKa, "かがやく"},
      {"解く", POS::Verb, 0.3F, "解く", false, false, false, CT::GodanKa, "とく"},
      {"突く", POS::Verb, 0.3F, "突く", false, false, false, CT::GodanKa, "つく"},
      {"招く", POS::Verb, 0.3F, "招く", false, false, false, CT::GodanKa, "まねく"},
      {"導く", POS::Verb, 0.3F, "導く", false, false, false, CT::GodanKa, "みちびく"},
      {"築く", POS::Verb, 0.3F, "築く", false, false, false, CT::GodanKa, "きずく"},

      // Basic auxiliary verbs (補助動詞)
      // Note: くる/いく are defined with higher cost earlier to avoid
      // competing with て-form compound patterns
      {"ある", POS::Verb, 0.3F, "ある", false, false, true, CT::GodanRa, "ある"},
      {"いる", POS::Verb, 0.3F, "いる", false, false, true, CT::Ichidan, "いる"},
      {"おる", POS::Verb, 0.3F, "おる", false, false, true, CT::GodanRa, "おる"},
      {"くれる", POS::Verb, 0.3F, "くれる", false, false, true, CT::Ichidan, "くれる"},
      {"あげる", POS::Verb, 0.3F, "あげる", false, false, true, CT::Ichidan, "あげる"},
      {"みる", POS::Verb, 0.3F, "みる", false, false, true, CT::Ichidan, "みる"},
      {"おく", POS::Verb, 0.3F, "おく", false, false, true, CT::GodanKa, "おく"},
      // Cost 0.25F (slightly lower than しまる 0.3F) because:
      // - Pure hiragana しまった is much more commonly しまう (completive) than しまる (close)
      // - When both generate しまった, しまう should win the tie
      // Note: is_low_info=false to avoid +0.5 penalty that would negate the cost advantage
      {"しまう", POS::Verb, 0.25F, "しまう", false, false, false, CT::GodanWa, "しまう"},

      // Common GodanWa verbs
      {"手伝う", POS::Verb, 1.0F, "手伝う", false, false, false, CT::GodanWa, "てつだう"},
      {"買う", POS::Verb, 1.0F, "買う", false, false, false, CT::GodanWa, "かう"},
      {"言う", POS::Verb, 1.0F, "言う", false, false, false, CT::GodanWa, "いう"},
      {"思う", POS::Verb, 1.0F, "思う", false, false, false, CT::GodanWa, "おもう"},
      {"使う", POS::Verb, 1.0F, "使う", false, false, false, CT::GodanWa, "つかう"},
      {"会う", POS::Verb, 1.0F, "会う", false, false, false, CT::GodanWa, "あう"},
      {"合う", POS::Verb, 1.0F, "合う", false, false, false, CT::GodanWa, "あう"},
      {"払う", POS::Verb, 1.0F, "払う", false, false, false, CT::GodanWa, "はらう"},
      {"洗う", POS::Verb, 1.0F, "洗う", false, false, false, CT::GodanWa, "あらう"},
      {"歌う", POS::Verb, 1.0F, "歌う", false, false, false, CT::GodanWa, "うたう"},
      {"習う", POS::Verb, 1.0F, "習う", false, false, false, CT::GodanWa, "ならう"},
      {"笑う", POS::Verb, 1.0F, "笑う", false, false, false, CT::GodanWa, "わらう"},
      {"違う", POS::Verb, 1.0F, "違う", false, false, false, CT::GodanWa, "ちがう"},
      {"追う", POS::Verb, 1.0F, "追う", false, false, false, CT::GodanWa, "おう"},
      {"誘う", POS::Verb, 1.0F, "誘う", false, false, false, CT::GodanWa, "さそう"},
      {"拾う", POS::Verb, 1.0F, "拾う", false, false, false, CT::GodanWa, "ひろう"},
      {"願う", POS::Verb, 0.5F, "願う", false, false, false, CT::GodanWa, "ねがう"},
      // Single-kanji GodanWa verbs (っ-onbin disambiguation)
      // っ-onbin (促音便) is inherently ambiguous: 云った could be 云う/云る/云つ
      // For multi-kanji stems, GodanWa is statistically preferred (手伝う, 買い求う).
      // For single-kanji stems, this statistical preference doesn't hold (張る, 取る
      // are common GodanRa). Dictionary entries are the ONLY reliable solution for
      // single-kanji っ-onbin disambiguation - scoring alone cannot distinguish them.
      {"云う", POS::Verb, 0.5F, "云う", false, false, false, CT::GodanWa, "いう"},
      {"貰う", POS::Verb, 0.5F, "貰う", false, false, false, CT::GodanWa, "もらう"},
      {"失う", POS::Verb, 0.5F, "失う", false, false, false, CT::GodanWa, "うしなう"},
      {"食う", POS::Verb, 0.5F, "食う", false, false, false, CT::GodanWa, "くう"},
      {"吸う", POS::Verb, 0.5F, "吸う", false, false, false, CT::GodanWa, "すう"},
      {"問う", POS::Verb, 0.5F, "問う", false, false, false, CT::GodanWa, "とう"},
      {"負う", POS::Verb, 0.5F, "負う", false, false, false, CT::GodanWa, "おう"},
      {"沿う", POS::Verb, 0.5F, "沿う", false, false, false, CT::GodanWa, "そう"},
      {"狂う", POS::Verb, 0.5F, "狂う", false, false, false, CT::GodanWa, "くるう"},
      {"酔う", POS::Verb, 0.5F, "酔う", false, false, false, CT::GodanWa, "よう"},

      // Common GodanRa verbs
      {"降る", POS::Verb, 1.0F, "降る", false, false, false, CT::GodanRa, "ふる"},
      {"取る", POS::Verb, 1.0F, "取る", false, false, false, CT::GodanRa, "とる"},
      {"乗る", POS::Verb, 1.0F, "乗る", false, false, false, CT::GodanRa, "のる"},
      {"送る", POS::Verb, 1.0F, "送る", false, false, false, CT::GodanRa, "おくる"},
      {"作る", POS::Verb, 1.0F, "作る", false, false, false, CT::GodanRa, "つくる"},
      {"知る", POS::Verb, 1.0F, "知る", false, false, false, CT::GodanRa, "しる"},
      {"座る", POS::Verb, 1.0F, "座る", false, false, false, CT::GodanRa, "すわる"},
      {"帰る", POS::Verb, 1.0F, "帰る", false, false, false, CT::GodanRa, "かえる"},
      {"入る", POS::Verb, 1.0F, "入る", false, false, false, CT::GodanRa, "はいる"},
      {"走る", POS::Verb, 1.0F, "走る", false, false, false, CT::GodanRa, "はしる"},
      {"売る", POS::Verb, 1.0F, "売る", false, false, false, CT::GodanRa, "うる"},
      {"切る", POS::Verb, 1.0F, "切る", false, false, false, CT::GodanRa, "きる"},
      // Single-kanji GodanRa verbs (っ-onbin disambiguation)
      // See comment above for GodanWa - dictionary is required for single-kanji stems.
      {"張る", POS::Verb, 0.5F, "張る", false, false, false, CT::GodanRa, "はる"},
      {"減る", POS::Verb, 0.5F, "減る", false, false, false, CT::GodanRa, "へる"},
      // Multi-kanji GodanRa verbs (often misclassified as GodanWa due to っ-onbin)
      {"頑張る", POS::Verb, 0.3F, "頑張る", false, false, false, CT::GodanRa, "がんばる"},
      {"止まる", POS::Verb, 0.3F, "止まる", false, false, false, CT::GodanRa, "とまる"},
      {"集まる", POS::Verb, 0.3F, "集まる", false, false, false, CT::GodanRa, "あつまる"},
      {"助かる", POS::Verb, 0.3F, "助かる", false, false, false, CT::GodanRa, "たすかる"},
      {"決まる", POS::Verb, 0.3F, "決まる", false, false, false, CT::GodanRa, "きまる"},
      {"始まる", POS::Verb, 0.3F, "始まる", false, false, false, CT::GodanRa, "はじまる"},
      {"泊まる", POS::Verb, 0.3F, "泊まる", false, false, false, CT::GodanRa, "とまる"},
      {"当たる", POS::Verb, 0.3F, "当たる", false, false, false, CT::GodanRa, "あたる"},

      // Common GodanTa verbs
      {"持つ", POS::Verb, 1.0F, "持つ", false, false, false, CT::GodanTa, "もつ"},
      {"待つ", POS::Verb, 1.0F, "待つ", false, false, false, CT::GodanTa, "まつ"},
      {"立つ", POS::Verb, 1.0F, "立つ", false, false, false, CT::GodanTa, "たつ"},
      {"打つ", POS::Verb, 1.0F, "打つ", false, false, false, CT::GodanTa, "うつ"},
      {"勝つ", POS::Verb, 1.0F, "勝つ", false, false, false, CT::GodanTa, "かつ"},
      {"育つ", POS::Verb, 1.0F, "育つ", false, false, false, CT::GodanTa, "そだつ"},
      // Single-kanji GodanTa verbs (っ-onbin disambiguation)
      // See GodanWa comment - dictionary is required for single-kanji stems.
      // 経つ: lower cost than 経る(Ichidan,へる) to ensure 経って→経つ not 経る
      {"経つ", POS::Verb, 0.2F, "経つ", false, false, false, CT::GodanTa, "たつ"},
      {"建つ", POS::Verb, 0.5F, "建つ", false, false, false, CT::GodanTa, "たつ"},
      {"発つ", POS::Verb, 0.5F, "発つ", false, false, false, CT::GodanTa, "たつ"},

      // Common Ichidan verbs
      {"伝える", POS::Verb, 0.5F, "伝える", false, false, false, CT::Ichidan, "つたえる"},
      {"知らせる", POS::Verb, 0.5F, "知らせる", false, false, false, CT::Ichidan, "しらせる"},
      {"届ける", POS::Verb, 0.5F, "届ける", false, false, false, CT::Ichidan, "とどける"},
      {"答える", POS::Verb, 0.5F, "答える", false, false, false, CT::Ichidan, "こたえる"},
      {"教える", POS::Verb, 0.5F, "教える", false, false, false, CT::Ichidan, "おしえる"},
      {"見せる", POS::Verb, 0.5F, "見せる", false, false, false, CT::Ichidan, "みせる"},
      {"入れる", POS::Verb, 0.5F, "入れる", false, false, false, CT::Ichidan, "いれる"},

      // ~する Godan verbs (サ変ラ行)
      {"対する", POS::Verb, 0.3F, "対する", false, false, false, CT::GodanRa, "たいする"},
      {"関する", POS::Verb, 0.3F, "関する", false, false, false, CT::GodanRa, "かんする"},
      {"属する", POS::Verb, 0.3F, "属する", false, false, false, CT::GodanRa, "ぞくする"},
      {"発する", POS::Verb, 0.3F, "発する", false, false, false, CT::GodanRa, "はっする"},
      {"接する", POS::Verb, 0.3F, "接する", false, false, false, CT::GodanRa, "せっする"},
      {"反する", POS::Verb, 0.3F, "反する", false, false, false, CT::GodanRa, "はんする"},
      {"達する", POS::Verb, 0.3F, "達する", false, false, false, CT::GodanRa, "たっする"},
      {"適する", POS::Verb, 0.3F, "適する", false, false, false, CT::GodanRa, "てきする"},
      {"課する", POS::Verb, 0.3F, "課する", false, false, false, CT::GodanRa, "かする"},
      {"解する", POS::Verb, 0.3F, "解する", false, false, false, CT::GodanRa, "かいする"},
      {"介する", POS::Verb, 0.3F, "介する", false, false, false, CT::GodanRa, "かいする"},
      {"帰する", POS::Verb, 0.3F, "帰する", false, false, false, CT::GodanRa, "きする"},
      {"欲する", POS::Verb, 0.3F, "欲する", false, false, false, CT::GodanRa, "ほっする"},
      {"資する", POS::Verb, 0.3F, "資する", false, false, false, CT::GodanRa, "しする"},

      // Other common verbs
      {"引っ越す", POS::Verb, 0.3F, "引っ越す", false, false, false, CT::GodanSa, "ひっこす"},

      // GodanSa verbs easily confused with Suru verbs
      // These have kanji + し pattern that looks like Suru verb renyokei
      {"際す", POS::Verb, 0.3F, "際す", false, false, false, CT::GodanSa, "きわす"},
  };
}

// =============================================================================
// Common Vocabulary (一般語彙)
// =============================================================================
std::vector<DictionaryEntry> getCommonVocabularyEntries() {
  return {
      // Compound nouns (複合名詞)
      {"飲み会", POS::Noun, 0.3F, "", false, false, false, CT::None, "のみかい"},
      {"楽しみ", POS::Noun, 0.3F, "", false, false, false, CT::None, "たのしみ"},
      {"食べ物", POS::Noun, 0.3F, "", false, false, false, CT::None, "たべもの"},
      {"飲み物", POS::Noun, 0.3F, "", false, false, false, CT::None, "のみもの"},
      {"買い物", POS::Noun, 0.3F, "", false, false, false, CT::None, "かいもの"},
      {"手助け", POS::Noun, 0.3F, "", false, false, false, CT::None, "てだすけ"},
      {"おすすめ", POS::Noun, 0.3F, "", false, false, false, CT::None, "おすすめ"},
      {"勘違い", POS::Noun, 0.3F, "", false, false, false, CT::None, "かんちがい"},

      // Degree nouns (程度名詞) - 複合語境界として重要
      {"最高", POS::Noun, 0.1F, "", false, false, false, CT::None, "さいこう"},
      {"最低", POS::Noun, 0.1F, "", false, false, false, CT::None, "さいてい"},
      {"最大", POS::Noun, 0.1F, "", false, false, false, CT::None, "さいだい"},
      {"最小", POS::Noun, 0.1F, "", false, false, false, CT::None, "さいしょう"},

      // Single-kanji nouns that need dictionary entry to avoid penalty
      // 毛: common standalone noun (第一毛, 白毛), but also in compounds (ムダ毛, 産毛)
      // Cost 0.5 balances standalone use vs compound formation
      {"毛", POS::Noun, 0.5F, "", false, false, false, CT::None, "け"},

      // Iteration mark compounds (踊り字複合語) - prevent X + 々 split
      {"人々", POS::Noun, -0.5F, "", false, false, false, CT::None, "ひとびと"},
      {"日々", POS::Noun, -0.5F, "", false, false, false, CT::None, "ひび"},
      {"国々", POS::Noun, -0.5F, "", false, false, false, CT::None, "くにぐに"},
      {"色々", POS::Noun, -0.5F, "", false, false, false, CT::None, "いろいろ"},
      {"時々", POS::Noun, -0.5F, "", false, false, false, CT::None, "ときどき"},
      {"様々", POS::Adjective, -0.5F, "", false, false, false, CT::NaAdjective, "さまざま"},

  };
}

}  // namespace suzume::dictionary::entries
