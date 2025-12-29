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
      {"て", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},
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

      // Quotation particles (引用助詞)
      {"って", POS::Particle, 0.8F, "", false, false, false, CT::None, ""},

      // Final particles (終助詞)
      {"か", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
      {"な", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
      {"ね", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
      {"よ", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
      {"わ", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
      {"の", POS::Particle, 1.0F, "", false, false, false, CT::None, ""},
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
      {"について", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},

      // Cause/Means (原因・手段)
      {"によって", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"により", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"によると", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},

      // Place/Situation (場所・状況)
      {"において", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},
      {"にて", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},

      // Capacity/Viewpoint (資格・観点)
      {"として", POS::Particle, 0.1F, "", false, false, false, CT::None, ""},
      {"にとって", POS::Particle, 0.3F, "", false, false, false, CT::None, ""},

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

      // Negation (否定)
      {"ない", POS::Auxiliary, 1.0F, "ない", false, false, false, CT::None, ""},
      {"なかった", POS::Auxiliary, 1.0F, "ない", false, false, false, CT::None, ""},
      {"なくて", POS::Auxiliary, 1.0F, "ない", false, false, false, CT::None, ""},
      {"なければ", POS::Auxiliary, 1.0F, "ない", false, false, false, CT::None, ""},
      {"ぬ", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},

      // Past/Completion (過去・完了)
      {"た", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},

      // Conjecture (推量)
      {"う", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"よう", POS::Auxiliary, 1.0F, "", false, false, false, CT::None, ""},
      {"だろう", POS::Auxiliary, 0.5F, "", false, false, false, CT::None, ""},
      {"でしょう", POS::Auxiliary, 0.5F, "", false, false, false, CT::None, ""},

      // Negative conjecture (否定推量)
      {"まい", POS::Auxiliary, 0.3F, "まい", false, false, false, CT::None, ""},

      // Obligation (当為)
      {"べき", POS::Auxiliary, 0.5F, "べき", false, false, false, CT::None, ""},
      {"べきだ", POS::Auxiliary, 0.3F, "べき", false, false, false, CT::None, ""},
      {"べきだった", POS::Auxiliary, 0.3F, "べき", false, false, false, CT::None, ""},
      {"べきで", POS::Auxiliary, 0.3F, "べき", false, false, false, CT::None, ""},
      {"べきでは", POS::Auxiliary, 0.3F, "べき", false, false, false, CT::None, ""},

      // Polite imperative (丁寧命令)
      {"なさい", POS::Auxiliary, 0.3F, "なさい", false, false, false, CT::None, ""},

      // Possibility/Uncertainty (可能性・不確実)
      {"かもしれない", POS::Auxiliary, 0.3F, "かもしれない", false, false, false, CT::None, ""},
      {"かもしれません", POS::Auxiliary, 0.3F, "かもしれない", false, false, false, CT::None, ""},
      {"かもしれなかった", POS::Auxiliary, 0.3F, "かもしれない", false, false, false, CT::None, ""},

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
      {"デス", POS::Auxiliary, 0.3F, "です", false, false, false, CT::None, "です"},
      {"マス", POS::Auxiliary, 0.3F, "ます", false, false, false, CT::None, "ます"},
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

      // Adversative (逆接)
      {"しかし", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"だが", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"けれども", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"ところが", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"それでも", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"でも", POS::Conjunction, 0.5F, "", false, false, false, CT::None, ""},
      {"だって", POS::Conjunction, 0.5F, "", false, false, false, CT::None, ""},
      {"にもかかわらず", POS::Conjunction, 0.5F, "", false, false, false, CT::None, ""},

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

      // Explanation/Supplement (説明・補足) - kanji with reading
      {"即ち", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "すなわち"},
      {"例えば", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "たとえば"},
      {"但し", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "ただし"},
      {"尚", POS::Conjunction, 1.0F, "", false, false, false, CT::None, "なお"},
      // Explanation/Supplement - hiragana only
      {"つまり", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"なぜなら", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"ちなみに", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},

      // Topic change (転換)
      {"さて", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"ところで", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"では", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
      {"それでは", POS::Conjunction, 1.0F, "", false, false, false, CT::None, ""},
  };
}

// =============================================================================
// Determiners (連体詞)
// =============================================================================
std::vector<DictionaryEntry> getDeterminerEntries() {
  return {
      // Demonstrative determiners (指示連体詞)
      {"この", POS::Determiner, 1.0F, "", false, false, false, CT::None, ""},
      {"その", POS::Determiner, 1.0F, "", false, false, false, CT::None, ""},
      {"あの", POS::Determiner, 1.0F, "", false, false, false, CT::None, ""},
      {"どの", POS::Determiner, 1.0F, "", false, false, false, CT::None, ""},

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

      // Determiners with kanji
      {"大きな", POS::Determiner, 1.0F, "", false, false, false, CT::None, "おおきな"},
      {"小さな", POS::Determiner, 1.0F, "", false, false, false, CT::None, "ちいさな"},
  };
}

// =============================================================================
// Pronouns (代名詞)
// =============================================================================
std::vector<DictionaryEntry> getPronounEntries() {
  return {
      // First person (一人称) - kanji with reading
      {"私", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "わたし"},
      {"僕", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "ぼく"},
      {"俺", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "おれ"},
      // First person - hiragana only
      {"わたくし", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},

      // First person plural (一人称複数)
      {"私たち", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "わたしたち"},
      {"僕たち", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "ぼくたち"},
      {"僕ら", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "ぼくら"},
      {"俺たち", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "おれたち"},
      {"俺ら", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "おれら"},
      {"我々", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "われわれ"},

      // Second person (二人称) - kanji with reading
      {"貴方", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "あなた"},
      {"君", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "きみ"},
      // Second person - hiragana/mixed only
      {"あなた", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"お前", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "おまえ"},

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

      // Demonstrative - interrogative (不定称)
      {"どれ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"どこ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"どちら", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},

      // Indefinite (不定代名詞) - kanji with reading
      {"誰", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "だれ"},
      {"何", POS::Pronoun, 0.5F, "", false, false, true, CT::None, "なに"},

      // Interrogatives (疑問詞)
      {"いつ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"いくつ", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"いくら", POS::Pronoun, 0.5F, "", false, false, true, CT::None, ""},
      {"どう", POS::Adverb, 0.5F, "", false, false, true, CT::None, ""},
      {"どうして", POS::Adverb, 0.5F, "", false, false, true, CT::None, ""},
      {"なぜ", POS::Adverb, 0.5F, "", false, false, true, CT::None, ""},
      {"どんな", POS::Adverb, 0.5F, "", false, false, true, CT::None, ""},
  };
}

// =============================================================================
// Formal Nouns (形式名詞)
// =============================================================================
std::vector<DictionaryEntry> getFormalNounEntries() {
  return {
      // Kanji forms with reading
      {"事", POS::Noun, 0.3F, "", false, true, false, CT::None, "こと"},
      {"こと", POS::Noun, -0.5F, "事", false, true, false, CT::None, ""},
      {"物", POS::Noun, 0.3F, "", false, true, false, CT::None, "もの"},
      {"もの", POS::Noun, -0.5F, "物", false, true, false, CT::None, ""},
      {"為", POS::Noun, 2.0F, "", false, true, false, CT::None, "ため"},
      {"所", POS::Noun, 1.0F, "", false, true, false, CT::None, "ところ"},
      {"ところ", POS::Noun, 0.3F, "", false, true, false, CT::None, ""},
      {"時", POS::Noun, 2.0F, "", false, true, false, CT::None, "とき"},
      {"内", POS::Noun, 2.0F, "", false, true, false, CT::None, "うち"},
      {"通り", POS::Noun, 2.0F, "", false, true, false, CT::None, "とおり"},
      {"限り", POS::Noun, 2.0F, "", false, true, false, CT::None, "かぎり"},
      // Suffix-like formal nouns
      {"付け", POS::Noun, 0.3F, "", false, true, false, CT::None, "つけ"},
      {"付", POS::Noun, 0.5F, "", false, true, false, CT::None, "つけ"},
      // Hiragana-only forms
      {"よう", POS::Noun, 2.0F, "", false, true, false, CT::None, ""},
      {"ほう", POS::Noun, 2.0F, "", false, true, false, CT::None, ""},
      {"わけ", POS::Noun, 2.0F, "", false, true, false, CT::None, ""},
      {"はず", POS::Noun, 2.0F, "", false, true, false, CT::None, ""},
      {"つもり", POS::Noun, 2.0F, "", false, true, false, CT::None, ""},
      {"まま", POS::Noun, 2.0F, "", false, true, false, CT::None, ""},
  };
}

// =============================================================================
// Time Nouns (時間名詞)
// =============================================================================
std::vector<DictionaryEntry> getTimeNounEntries() {
  return {
      // Days (日)
      {"今日", POS::Noun, 0.5F, "", false, true, false, CT::None, "きょう"},
      {"明日", POS::Noun, 0.5F, "", false, true, false, CT::None, "あした"},
      {"昨日", POS::Noun, 0.5F, "", false, true, false, CT::None, "きのう"},
      {"明後日", POS::Noun, 0.5F, "", false, true, false, CT::None, "あさって"},
      {"一昨日", POS::Noun, 0.5F, "", false, true, false, CT::None, "おととい"},
      {"毎日", POS::Noun, 0.5F, "", false, true, false, CT::None, "まいにち"},

      // Time of day (時間帯)
      {"今朝", POS::Noun, 0.5F, "", false, true, false, CT::None, "けさ"},
      {"毎朝", POS::Noun, 0.5F, "", false, true, false, CT::None, "まいあさ"},
      {"今晩", POS::Noun, 0.5F, "", false, true, false, CT::None, "こんばん"},
      {"今夜", POS::Noun, 0.5F, "", false, true, false, CT::None, "こんや"},
      {"昨夜", POS::Noun, 0.5F, "", false, true, false, CT::None, "さくや"},
      {"朝", POS::Noun, 0.6F, "", false, true, false, CT::None, "あさ"},
      {"昼", POS::Noun, 0.6F, "", false, true, false, CT::None, "ひる"},
      {"夜", POS::Noun, 0.6F, "", false, true, false, CT::None, "よる"},
      {"夕方", POS::Noun, 0.6F, "", false, true, false, CT::None, "ゆうがた"},

      // Weeks (週)
      {"今週", POS::Noun, 0.5F, "", false, true, false, CT::None, "こんしゅう"},
      {"来週", POS::Noun, 0.5F, "", false, true, false, CT::None, "らいしゅう"},
      {"先週", POS::Noun, 0.5F, "", false, true, false, CT::None, "せんしゅう"},
      {"毎週", POS::Noun, 0.5F, "", false, true, false, CT::None, "まいしゅう"},

      // Months (月)
      {"今月", POS::Noun, 0.5F, "", false, true, false, CT::None, "こんげつ"},
      {"来月", POS::Noun, 0.5F, "", false, true, false, CT::None, "らいげつ"},
      {"先月", POS::Noun, 0.5F, "", false, true, false, CT::None, "せんげつ"},
      {"毎月", POS::Noun, 0.5F, "", false, true, false, CT::None, "まいつき"},

      // Years (年)
      {"今年", POS::Noun, 0.5F, "", false, true, false, CT::None, "ことし"},
      {"来年", POS::Noun, 0.5F, "", false, true, false, CT::None, "らいねん"},
      {"去年", POS::Noun, 0.5F, "", false, true, false, CT::None, "きょねん"},
      {"昨年", POS::Noun, 0.5F, "", false, true, false, CT::None, "さくねん"},
      {"毎年", POS::Noun, 0.5F, "", false, true, false, CT::None, "まいとし"},

      // Other time expressions
      {"今", POS::Noun, 0.5F, "", false, true, false, CT::None, "いま"},
      {"現在", POS::Noun, 0.5F, "", false, true, false, CT::None, "げんざい"},
      {"最近", POS::Noun, 0.5F, "", false, true, false, CT::None, "さいきん"},
      {"将来", POS::Noun, 0.5F, "", false, true, false, CT::None, "しょうらい"},
      {"過去", POS::Noun, 0.5F, "", false, true, false, CT::None, "かこ"},
      {"未来", POS::Noun, 0.5F, "", false, true, false, CT::None, "みらい"},
  };
}

// =============================================================================
// Low Info Words (低情報量語)
// =============================================================================
std::vector<DictionaryEntry> getLowInfoEntries() {
  return {
      // Honorific pattern verbs
      {"伝え", POS::Verb, 0.5F, "伝える", false, false, false, CT::Ichidan, ""},
      {"伝える", POS::Verb, 0.5F, "伝える", false, false, false, CT::Ichidan, ""},
      {"知らせ", POS::Verb, 0.5F, "知らせる", false, false, false, CT::Ichidan, ""},
      {"知らせる", POS::Verb, 0.5F, "知らせる", false, false, false, CT::Ichidan, ""},
      {"届け", POS::Verb, 0.5F, "届ける", false, false, false, CT::Ichidan, ""},
      {"届ける", POS::Verb, 0.5F, "届ける", false, false, false, CT::Ichidan, ""},
      {"答え", POS::Verb, 0.5F, "答える", false, false, false, CT::Ichidan, ""},
      {"答える", POS::Verb, 0.5F, "答える", false, false, false, CT::Ichidan, ""},
      {"教え", POS::Verb, 0.5F, "教える", false, false, false, CT::Ichidan, ""},
      {"教える", POS::Verb, 0.5F, "教える", false, false, false, CT::Ichidan, ""},
      {"見せ", POS::Verb, 0.5F, "見せる", false, false, false, CT::Ichidan, ""},
      {"見せる", POS::Verb, 0.5F, "見せる", false, false, false, CT::Ichidan, ""},
      {"聞かせ", POS::Verb, 0.5F, "聞かせる", false, false, false, CT::Ichidan, ""},
      {"待ち", POS::Verb, 0.5F, "待つ", false, false, false, CT::GodanTa, ""},
      {"願い", POS::Verb, 0.5F, "願う", false, false, false, CT::GodanWa, ""},
      {"願う", POS::Verb, 0.5F, "願う", false, false, false, CT::GodanWa, ""},

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

      // Common verbs with っ-onbin ambiguity
      {"買う", POS::Verb, 1.0F, "買う", false, false, false, CT::GodanWa, ""},
      {"言う", POS::Verb, 1.0F, "言う", false, false, false, CT::GodanWa, ""},
      {"思う", POS::Verb, 1.0F, "思う", false, false, false, CT::GodanWa, ""},
      {"使う", POS::Verb, 1.0F, "使う", false, false, false, CT::GodanWa, ""},
      {"会う", POS::Verb, 1.0F, "会う", false, false, false, CT::GodanWa, ""},
      {"払う", POS::Verb, 1.0F, "払う", false, false, false, CT::GodanWa, ""},
      {"洗う", POS::Verb, 1.0F, "洗う", false, false, false, CT::GodanWa, ""},
      {"歌う", POS::Verb, 1.0F, "歌う", false, false, false, CT::GodanWa, ""},
      {"習う", POS::Verb, 1.0F, "習う", false, false, false, CT::GodanWa, ""},
      {"笑う", POS::Verb, 1.0F, "笑う", false, false, false, CT::GodanWa, ""},
      {"違う", POS::Verb, 1.0F, "違う", false, false, false, CT::GodanWa, ""},
      {"追う", POS::Verb, 1.0F, "追う", false, false, false, CT::GodanWa, ""},
      {"誘う", POS::Verb, 1.0F, "誘う", false, false, false, CT::GodanWa, ""},
      {"拾う", POS::Verb, 1.0F, "拾う", false, false, false, CT::GodanWa, ""},
      // Common GodanRa verbs
      {"取る", POS::Verb, 1.0F, "取る", false, false, false, CT::GodanRa, ""},
      {"乗る", POS::Verb, 1.0F, "乗る", false, false, false, CT::GodanRa, ""},
      {"送る", POS::Verb, 1.0F, "送る", false, false, false, CT::GodanRa, ""},
      {"作る", POS::Verb, 1.0F, "作る", false, false, false, CT::GodanRa, ""},
      {"知る", POS::Verb, 1.0F, "知る", false, false, false, CT::GodanRa, ""},
      {"座る", POS::Verb, 1.0F, "座る", false, false, false, CT::GodanRa, ""},
      {"帰る", POS::Verb, 1.0F, "帰る", false, false, false, CT::GodanRa, ""},
      {"入る", POS::Verb, 1.0F, "入る", false, false, false, CT::GodanRa, ""},
      {"走る", POS::Verb, 1.0F, "走る", false, false, false, CT::GodanRa, ""},
      {"売る", POS::Verb, 1.0F, "売る", false, false, false, CT::GodanRa, ""},
      {"切る", POS::Verb, 1.0F, "切る", false, false, false, CT::GodanRa, ""},
      // Common GodanTa verbs
      {"持つ", POS::Verb, 1.0F, "持つ", false, false, false, CT::GodanTa, ""},
      {"待つ", POS::Verb, 1.0F, "待つ", false, false, false, CT::GodanTa, ""},
      {"立つ", POS::Verb, 1.0F, "立つ", false, false, false, CT::GodanTa, ""},
      {"打つ", POS::Verb, 1.0F, "打つ", false, false, false, CT::GodanTa, ""},
      {"勝つ", POS::Verb, 1.0F, "勝つ", false, false, false, CT::GodanTa, ""},
      {"育つ", POS::Verb, 1.0F, "育つ", false, false, false, CT::GodanTa, ""},

      // Irregular adjective よい conjugations
      {"よい", POS::Adjective, 0.3F, "よい", false, false, false, CT::IAdjective, ""},
      {"よく", POS::Adverb, 0.5F, "よい", false, false, false, CT::IAdjective, ""},
      {"よくない", POS::Adjective, 0.5F, "よい", false, false, false, CT::IAdjective, ""},
      {"よくて", POS::Adjective, 0.5F, "よい", false, false, false, CT::IAdjective, ""},
      {"よかった", POS::Adjective, 0.5F, "よい", false, false, false, CT::IAdjective, ""},
      {"よければ", POS::Adjective, 0.5F, "よい", false, false, false, CT::IAdjective, ""},

      // Prefixes (接頭辞)
      {"お", POS::Prefix, 0.3F, "", true, false, false, CT::None, ""},
      {"ご", POS::Prefix, 0.3F, "", true, false, false, CT::None, ""},
      {"御", POS::Prefix, 1.0F, "", true, false, false, CT::None, ""},
      {"何", POS::Prefix, 0.8F, "なん", true, false, false, CT::None, ""},

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

      // Plural suffixes (複数接尾語)
      {"たち", POS::Suffix, 0.5F, "", false, false, true, CT::None, ""},
      {"ら", POS::Suffix, 0.5F, "", false, false, true, CT::None, ""},
      {"ども", POS::Suffix, 0.8F, "", false, false, true, CT::None, ""},
      {"がた", POS::Suffix, 0.8F, "", false, false, true, CT::None, ""},

      // Temporal suffixes (時間接尾語)
      {"ごろ", POS::Suffix, -2.5F, "", false, false, true, CT::None, ""},
      {"頃", POS::Suffix, -2.5F, "", false, false, true, CT::None, ""},

      // Honorific suffixes (敬称接尾語)
      {"さん", POS::Suffix, 0.5F, "", false, false, true, CT::None, ""},
      {"様", POS::Suffix, 0.5F, "", false, false, true, CT::None, ""},
      {"ちゃん", POS::Suffix, 0.5F, "", false, false, true, CT::None, ""},
      {"くん", POS::Suffix, 0.5F, "", false, false, true, CT::None, ""},
      {"氏", POS::Suffix, 0.8F, "", false, false, true, CT::None, ""},
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
      {"とても", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"すごく", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"めっちゃ", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"かなり", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"たくさん", POS::Adverb, 0.3F, "", false, false, false, CT::None, ""},
      {"沢山", POS::Adverb, 0.3F, "", false, false, false, CT::None, "たくさん"},
      {"もっと", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"ずっと", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"さらに", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"まさに", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"あまり", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
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
      {"今すぐ", POS::Adverb, 0.5F, "", false, false, false, CT::None, "いますぐ"},
      {"まだ", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"もう", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"やっと", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"ついに", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"いつも", POS::Adverb, -1.2F, "", false, false, false, CT::None, ""},
      {"たまに", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"よく", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"たびたび", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},

      // Conditional adverbs (条件副詞)
      {"もし", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"もしも", POS::Adverb, 0.5F, "", false, false, false, CT::None, ""},
      {"仮に", POS::Adverb, 0.5F, "", false, false, false, CT::None, "かりに"},
      {"万一", POS::Adverb, 0.5F, "", false, false, false, CT::None, "まんいち"},

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
      {"自然", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "しぜん"},
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

      // Adverbial na-adjectives
      {"流石", POS::Adjective, 0.5F, "", false, false, false, CT::NaAdjective, "さすが"},

      // Hiragana na-adjectives
      {"だめ", POS::Adjective, 0.3F, "だめ", false, false, false, CT::NaAdjective, ""},
      {"みたい", POS::Adjective, 0.1F, "みたい", false, false, false, CT::NaAdjective, ""},

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

      // Special irregular adjective
      {"いい", POS::Adjective, 0.3F, "いい", false, false, false, CT::IAdjective, ""},
  };
}

// =============================================================================
// Hiragana Verbs (ひらがな動詞)
// =============================================================================
std::vector<DictionaryEntry> getHiraganaVerbEntries() {
  return {
      // Ichidan verbs (一段動詞)
      {"できる", POS::Verb, 0.3F, "できる", false, false, false, CT::Ichidan, ""},
      {"やめる", POS::Verb, 0.3F, "やめる", false, false, false, CT::Ichidan, ""},
      {"はじめる", POS::Verb, 0.3F, "はじめる", false, false, false, CT::Ichidan, ""},

      // Godan-Ka verbs (五段カ行)
      {"いただく", POS::Verb, 0.3F, "いただく", false, false, false, CT::GodanKa, ""},
      {"とく", POS::Verb, 0.3F, "とく", false, false, true, CT::GodanKa, ""},
      {"っとく", POS::Verb, -0.5F, "とく", false, false, true, CT::GodanKa, ""},
      {"てく", POS::Verb, 0.3F, "てく", false, false, true, CT::GodanKa, ""},
      {"ってく", POS::Verb, -0.5F, "てく", false, false, true, CT::GodanKa, ""},

      // Godan-Ra verbs (五段ラ行)
      {"やる", POS::Verb, 0.3F, "やる", false, false, false, CT::GodanRa, ""},
      {"わかる", POS::Verb, 0.3F, "わかる", false, false, false, CT::GodanRa, ""},
      {"なる", POS::Verb, 0.3F, "なる", false, false, false, CT::GodanRa, ""},
      {"できあがる", POS::Verb, 0.3F, "できあがる", false, false, false, CT::GodanRa, ""},
      {"がんばる", POS::Verb, 0.3F, "がんばる", false, false, false, CT::GodanRa, ""},

      // Godan-Wa verbs (五段ワ行)
      {"もらう", POS::Verb, 0.3F, "もらう", false, false, false, CT::GodanWa, ""},

      // Godan-Sa verbs (五段サ行)
      {"いたす", POS::Verb, 0.3F, "いたす", false, false, false, CT::GodanSa, ""},

      // Suru verb (サ変動詞)
      {"する", POS::Verb, 0.5F, "する", false, false, false, CT::Suru, ""},

      // Kuru verb imperative only (カ変動詞命令形)
      {"こい", POS::Verb, 0.2F, "くる", false, false, false, CT::None, ""},

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
      {"下がる", POS::Verb, 0.3F, "下がる", false, false, false, CT::GodanRa, "さがる"},

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

      // Compound verbs (敬語複合動詞)
      {"恐れ入る", POS::Verb, 0.3F, "恐れ入る", false, false, false, CT::GodanRa, "おそれいる"},
      {"申し上げる", POS::Verb, 0.3F, "申し上げる", false, false, false, CT::Ichidan, "もうしあげる"},
      {"差し上げる", POS::Verb, 0.3F, "差し上げる", false, false, false, CT::Ichidan, "さしあげる"},

      // Special Godan verbs with irregular euphonic changes
      {"行く", POS::Verb, 0.3F, "行く", false, false, false, CT::GodanKa, "いく"},
      {"行った", POS::Verb, 0.3F, "行く", false, false, false, CT::GodanKa, "いった"},
      {"行って", POS::Verb, 0.3F, "行く", false, false, false, CT::GodanKa, "いって"},

      // Common GodanKa verbs
      {"書く", POS::Verb, 0.3F, "書く", false, false, false, CT::GodanKa, "かく"},
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
      {"ある", POS::Verb, 0.3F, "ある", false, false, true, CT::GodanRa, "ある"},
      {"いる", POS::Verb, 0.3F, "いる", false, false, true, CT::Ichidan, "いる"},
      {"おる", POS::Verb, 0.3F, "おる", false, false, true, CT::GodanRa, "おる"},
      {"くる", POS::Verb, 0.3F, "くる", false, false, true, CT::Kuru, "くる"},
      {"くれる", POS::Verb, 0.3F, "くれる", false, false, true, CT::Ichidan, "くれる"},
      {"あげる", POS::Verb, 0.3F, "あげる", false, false, true, CT::Ichidan, "あげる"},
      {"みる", POS::Verb, 0.3F, "みる", false, false, true, CT::Ichidan, "みる"},
      {"おく", POS::Verb, 0.3F, "おく", false, false, true, CT::GodanKa, "おく"},
      {"しまう", POS::Verb, 0.3F, "しまう", false, false, true, CT::GodanWa, "しまう"},

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

      // Common GodanTa verbs
      {"持つ", POS::Verb, 1.0F, "持つ", false, false, false, CT::GodanTa, "もつ"},
      {"待つ", POS::Verb, 1.0F, "待つ", false, false, false, CT::GodanTa, "まつ"},
      {"立つ", POS::Verb, 1.0F, "立つ", false, false, false, CT::GodanTa, "たつ"},
      {"打つ", POS::Verb, 1.0F, "打つ", false, false, false, CT::GodanTa, "うつ"},
      {"勝つ", POS::Verb, 1.0F, "勝つ", false, false, false, CT::GodanTa, "かつ"},
      {"育つ", POS::Verb, 1.0F, "育つ", false, false, false, CT::GodanTa, "そだつ"},

      // Common Ichidan verbs
      {"伝える", POS::Verb, 0.5F, "伝える", false, false, false, CT::Ichidan, "つたえる"},
      {"知らせる", POS::Verb, 0.5F, "知らせる", false, false, false, CT::Ichidan, "しらせる"},
      {"届ける", POS::Verb, 0.5F, "届ける", false, false, false, CT::Ichidan, "とどける"},
      {"答える", POS::Verb, 0.5F, "答える", false, false, false, CT::Ichidan, "こたえる"},
      {"教える", POS::Verb, 0.5F, "教える", false, false, false, CT::Ichidan, "おしえる"},
      {"見せる", POS::Verb, 0.5F, "見せる", false, false, false, CT::Ichidan, "みせる"},

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
  };
}

}  // namespace suzume::dictionary::entries
