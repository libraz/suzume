#include "core/types.h"
#include "dictionary/dictionary.h"

#include <vector>

namespace suzume::dictionary::entries {

using POS = core::PartOfSpeech;
using CT = ConjugationType;

// =============================================================================
// Helper functions for concise DictionaryEntry construction
// =============================================================================

// Particle helper: creates PARTICLE entry with common defaults
// Usage: particle("が"), particle("で", 0.1F)
inline DictionaryEntry particle(const char* s, float cost = 0.5F) {
  return {s, POS::Particle, cost, "", false, false, false, CT::None, ""};
}

// Auxiliary helper: creates AUXILIARY entry
// Usage: aux("です"), aux("た", 0.6F, "た"), aux("た", 0.6F, "た", CT::None, "reading")
inline DictionaryEntry aux(const char* s, float cost = 0.3F,
                           const char* lemma = "", CT ct = CT::None,
                           const char* reading = "") {
  return {s, POS::Auxiliary, cost, lemma, false, false, false, ct, reading};
}

// Verb helper: creates VERB entry
// Usage: verb("し", -0.7F, "する", CT::Suru)
inline DictionaryEntry verb(const char* s, float cost, const char* lemma,
                            CT ct = CT::None, const char* reading = "") {
  return {s, POS::Verb, cost, lemma, false, false, false, ct, reading};
}

// Adjective helper: creates ADJECTIVE entry
// Usage: adj("美しい", 0.5F, "", CT::IAdjective, "reading")
inline DictionaryEntry adj(const char* s, float cost, const char* lemma = "",
                           CT ct = CT::IAdjective, const char* reading = "") {
  return {s, POS::Adjective, cost, lemma, false, false, false, ct, reading};
}

// Na-adjective helper: creates NA_ADJECTIVE entry
// Usage: na_adj("静か", "しずか"), na_adj("好き", "すき", 0.3F)
inline DictionaryEntry na_adj(const char* s, const char* reading,
                              float cost = 0.5F, const char* lemma = "") {
  return {s, POS::Adjective, cost, lemma, false, false, false, CT::NaAdjective, reading};
}

// I-adjective helper: creates I_ADJECTIVE entry
// Usage: i_adj("寒い", "さむい"), i_adj("無い", "", 0.3F, "無い")
inline DictionaryEntry i_adj(const char* s, const char* reading,
                             float cost = 0.3F, const char* lemma = "") {
  // lemma defaults to empty, which is handled by the analyzer
  return {s, POS::Adjective, cost, lemma, false, false, false, CT::IAdjective, reading};
}

// Determiner helper: creates DETERMINER entry
// Usage: det("この"), det("同じ", -0.3F, "おなじ")
inline DictionaryEntry det(const char* s, float cost = 1.0F,
                           const char* reading = "", const char* lemma = "") {
  return {s, POS::Determiner, cost, lemma, false, false, false, CT::None, reading};
}

// Noun helper: creates NOUN entry
// Usage: noun("机", 0.5F), noun("時分", 0.5F, "時分", true, "じぶん")
inline DictionaryEntry noun(const char* s, float cost = 0.5F,
                            const char* lemma = "", bool is_formal = false,
                            const char* reading = "") {
  return {s, POS::Noun, cost, lemma, false, is_formal, false, CT::None, reading};
}

// Formal noun helper: creates FORMAL_NOUN entry (形式名詞)
// Usage: formal_noun("こと", -0.5F), formal_noun("物", 0.3F, "もの", "もの")
inline DictionaryEntry formal_noun(const char* s, float cost = 0.5F,
                                   const char* lemma = "", const char* reading = "") {
  return {s, POS::Noun, cost, lemma, false, true, false, CT::None, reading};
}

// Time noun helper: creates TIME_NOUN entry (時間名詞)
// Usage: time_noun("今日", "きょう"), time_noun("今週", "こんしゅう")
inline DictionaryEntry time_noun(const char* s, const char* reading,
                                 float cost = 0.5F, const char* lemma = "") {
  return {s, POS::Noun, cost, lemma, false, false, false, CT::None, reading};
}

// Conjunction helper: creates CONJUNCTION entry
// Usage: conj("しかし"), conj("従って", 0.5F, "", "したがって")
inline DictionaryEntry conj(const char* s, float cost = 0.5F,
                            const char* lemma = "", const char* reading = "") {
  return {s, POS::Conjunction, cost, lemma, false, false, false, CT::None, reading};
}

// Adverb helper: creates ADVERB entry
// Usage: adv("とても"), adv("大変", -1.0F, "", "たいへん"), adv("なるほど", 0.3F, "成程")
inline DictionaryEntry adv(const char* s, float cost = 0.5F,
                           const char* lemma = "", const char* reading = "") {
  return {s, POS::Adverb, cost, lemma, false, false, false, CT::None, reading};
}

// Suffix helper: creates SUFFIX entry
// Usage: suffix("さん", 0.3F)
inline DictionaryEntry suffix(const char* s, float cost = 0.3F,
                              const char* lemma = "") {
  return {s, POS::Suffix, cost, lemma, false, false, false, CT::None, ""};
}

// Pronoun helper: creates PRONOUN entry
// Usage: pronoun("私", 0.3F, false, "わたし"), pronoun("これ")
inline DictionaryEntry pronoun(const char* s, float cost = 0.5F,
                               bool is_low_info = true,
                               const char* reading = "") {
  return {s, POS::Pronoun, cost, "", false, false, is_low_info, CT::None, reading};
}

// =============================================================================
// Particles (助詞)
// =============================================================================
std::vector<DictionaryEntry> getParticleEntries() {
  return {
      // Case particles (格助詞)
      particle("が"), particle("を"), particle("に"),
      particle("で", 0.1F),  // low cost for te-form split
      particle("と"), particle("から"), particle("まで"),
      particle("より", 0.8F), particle("へ"),

      // Binding particles (係助詞)
      particle("は"), particle("も"),
      particle("こそ", 0.8F), particle("さえ", 0.8F),
      particle("でも", 0.8F), particle("しか", 0.8F),

      // Conjunctive particles (接続助詞)
      particle("て", 0.1F),  // low cost for te-form split
      particle("ば", 0.8F), particle("たら", 0.8F), particle("なら"),
      particle("ら", 0.8F), particle("ながら", 0.8F),
      particle("のに", 0.8F), particle("ので"),
      particle("けれど", 0.8F), particle("けど", 0.8F),
      particle("けども", 0.8F), particle("けれども", 0.8F),
      particle("し", -0.5F),  // 列挙・理由 (接続助詞)

      // Quotation particles (引用助詞)
      particle("って", 0.8F),

      // Final particles (終助詞)
      particle("か", 1.0F), particle("な", 1.0F), particle("ね", 1.0F),
      particle("よ", 1.0F), particle("わ", 1.0F), particle("の", 1.0F),
      {"ん", POS::Particle, 0.5F, "の", false, false, false, CT::None, ""},  // colloquial の
      particle("じゃん", 0.8F), particle("っけ", 0.8F), particle("かしら", 0.8F),

      // Adverbial particles (副助詞)
      particle("かも", -0.5F),  // prevent か+も split in かもしれない
      particle("ばかり", 1.0F), particle("だけ", 1.0F), particle("ほど", 1.0F),
      particle("くらい", 0.3F), particle("ぐらい", 0.3F),
      particle("など", 1.0F), particle("なんて"), particle("ってば"), particle("ったら"),
  };
}

// =============================================================================
// Compound Particles (複合助詞)
// =============================================================================
std::vector<DictionaryEntry> getCompoundParticleEntries() {
  return {
      // Relation (関連)
      particle("について", 0.0F),  // beat false positive adjective candidates

      // Cause/Means (原因・手段)
      particle("によって", -0.8F),  // beat によっ(verb)+て split
      particle("により", -0.5F), particle("による", -0.5F),
      particle("によると", -0.5F), particle("によれば", -0.5F),

      // Place/Situation (場所・状況)
      particle("において", -1.2F),  // prevent に+おい(verb)+て split
      particle("にて", 0.3F),

      // Capacity/Viewpoint (資格・観点)
      particle("として", -0.5F),  // prevent と+し(VERB)+て split
      particle("にとって", 0.3F),
      particle("にとっても", -0.5F),  // prevent に+とっても(ADV) misparse
      // Note: 漢字を含む複合助詞（に対して、に関して等）は分割する方針

      // Duration/Scope (範囲・期間)
      particle("にわたって", 0.3F), particle("にわたり", 0.3F),
      particle("にあたって", 0.3F), particle("にあたり", 0.3F),

      // Topic/Means (話題・手段)
      particle("をめぐって", 0.3F), particle("をめぐり", 0.3F), particle("をもって", 0.3F),
  };
}

// =============================================================================
// Auxiliaries (助動詞)
// =============================================================================
std::vector<DictionaryEntry> getAuxiliaryEntries() {
  return {
      // Assertion (断定)
      aux("だ", 0.1F),
      aux("だっ", 0.3F, "だ"),  // 連用タ接続形
      aux("で", 1.0F, "だ"),    // copula renyokei
      aux("だったら", 0.1F), aux("です", 0.1F),
      aux("でし", -1.0F, "です"),  // renyoukei of です
      aux("でしたら", 0.1F), aux("である", 0.1F), aux("であれば", 0.1F),

      // Polite (丁寧)
      aux("ます", 1.0F, "ます"),
      aux("まし", -0.5F, "ます"),  // renyoukei
      aux("ませ", -0.5F, "ます"),  // mizenkei
      aux("ましょう", 1.0F, "ます"),

      // Negation (否定)
      aux("ない", 1.0F, "ない"),
      aux("なかっ", 0.3F, "ない"),  // 連用タ接続
      aux("なければ", 1.0F, "ない"),
      aux("ぬ", -1.0F, "ぬ"),  // 文語否定
      aux("ず", 0.5F, "ず"), aux("ずに", 0.5F, "ず"), aux("ずとも", 0.5F, "ず"),
      aux("ごとく", -2.0F, "ごとく"), aux("ごとき", -2.0F, "ごとき"),
      aux("じゃない", 0.3F, "ではない"),
      aux("ん", -0.5F, "ん"),

      // Past/Completion (過去・完了)
      aux("た", 0.6F, "た"),
      aux("たら", -0.8F, "た"),  // 仮定形
      aux("だら", -0.8F, "だ"),

      // Conjecture (推量)
      aux("う", 1.0F), aux("よう", 1.0F),
      aux("だろう", 0.3F), aux("でしょう", 0.1F),

      // Negative conjecture (否定推量)
      aux("まい", 0.3F, "まい"),

      // Obligation (当為)
      aux("べき", 0.5F, "べし"),
      aux("べきだ", 0.3F, "べし"), aux("べきで", 0.3F, "べし"), aux("べきでは", 0.3F, "べし"),

      // Passive/Potential (受身・可能)
      aux("れ", 0.3F, "れる", CT::Ichidan), aux("れる", 0.3F, "れる", CT::Ichidan),
      aux("られ", 0.3F, "られる", CT::Ichidan), aux("られる", 0.3F, "られる", CT::Ichidan),

      // Suru verb stem forms (サ変動詞語幹活用形) - VERB, not AUX
      verb("し", -0.7F, "する", CT::Suru),
      verb("す", -0.3F, "する", CT::Suru),
      verb("さ", -1.0F, "する", CT::Suru),
      aux("しよう", 0.2F, "する", CT::Suru),
      aux("しろ", -0.3F, "する", CT::Suru), aux("せよ", -0.5F, "する", CT::Suru),
      aux("すれば", 0.2F, "する", CT::Suru), aux("しそう", 0.3F, "する", CT::Suru),

      // Causative (使役)
      aux("せ", 0.3F, "せる", CT::Ichidan), aux("せる", 0.3F, "せる", CT::Ichidan),
      aux("させ", 1.0F, "させる", CT::Ichidan), aux("させる", 1.0F, "させる", CT::Ichidan),

      // Desiderative conjugated forms (願望活用形)
      aux("たく", 0.3F, "たい", CT::IAdjective),
      aux("たかっ", 0.3F, "たい", CT::IAdjective),

      // Polite imperative
      aux("なさい", 0.3F, "なさい"),

      // Possibility/Uncertainty
      aux("かもしれない", 0.3F, "かもしれない"),

      // Certainty - with reading
      aux("間違いない", -2.0F, "間違いない", CT::None, "まちがいない"),
      aux("違いない", -2.0F, "違いない", CT::None, "ちがいない"),
      aux("に違いない", -2.0F, "違いない", CT::None, "にちがいない"),

      // Desire (願望)
      aux("たい", 0.3F, "たい"),
      adj("たければ", 0.3F, "たい", CT::IAdjective),
      aux("たがる", 1.0F),

      // Potential/Passive/Causative (generic)
      aux("れる", 1.0F), aux("られる", 1.0F), aux("せる", 1.0F), aux("させる", 1.0F),

      // Polite existence
      aux("ございます", 0.3F, "ございます"),

      // Request
      aux("ください", 0.3F, "ください"), aux("くださいませ", 0.3F, "ください"),

      // Progressive/Continuous (進行・継続)
      aux("いる", 0.3F, "いる"), aux("います", 0.3F, "いる"), aux("いません", 0.3F, "いる"),
      aux("い", 0.0F, "いる"),
      aux("いない", 2.0F, "いる"), aux("いなかった", 2.0F, "いる"),
      aux("いれば", 0.3F, "いる"),

      // Completive/Regretful (完了・遺憾) - しまう
      aux("しまう", 0.3F, "しまう"), aux("しまった", 0.3F, "しまう"),
      aux("しまって", 0.3F, "しまう"), aux("しまい", 0.3F, "しまう"),
      aux("しまいます", 0.3F, "しまう"),
      verb("しまわ", 0.0F, "しまう"),  // mizenkei
      aux("しまわない", 0.3F, "しまう"), aux("しまわなかった", 0.3F, "しまう"),
      aux("しまえば", 0.3F, "しまう"),

      // Contracted forms: ちゃう/じゃう
      verb("ちゃう", 0.3F, "ちゃう", CT::GodanWa, "ちゃう"),
      verb("ちゃっ", 0.5F, "ちゃう", CT::GodanWa, "ちゃう"),
      verb("ちゃい", 0.5F, "ちゃう", CT::GodanWa, "ちゃう"),
      verb("じゃう", 0.3F, "じゃう", CT::GodanWa, "じゃう"),
      verb("じゃっ", 0.5F, "じゃう", CT::GodanWa, "じゃう"),
      verb("じゃい", 0.5F, "じゃう", CT::GodanWa, "じゃう"),

      // Contracted forms: てる/とく
      verb("てる", -0.3F, "てる", CT::Ichidan, "てる"),
      verb("て", 0.65F, "てる", CT::Ichidan, "てる"),
      verb("で", 0.65F, "でる", CT::Ichidan, "でる"),
      verb("でる", 0.5F, "でる", CT::Ichidan, "でる"),
      verb("とく", -0.3F, "とく", CT::GodanKa, "とく"),
      verb("どく", -0.3F, "どく", CT::GodanKa, "どく"),
      verb("といた", 0.3F, "とく", CT::GodanKa, "とく"),
      verb("といて", 0.3F, "とく", CT::GodanKa, "とく"),
      verb("どいた", 0.3F, "どく", CT::GodanKa, "どく"),
      verb("どいて", 0.3F, "どく", CT::GodanKa, "どく"),

      // Directional auxiliaries (方向補助動詞)
      aux("いく", 1.5F, "いく"), aux("いった", 1.5F, "いく"), aux("いって", 1.5F, "いく"),
      verb("いか", -0.5F, "いく"),
      aux("いかない", 1.5F, "いく"),
      aux("くる", 1.5F, "くる"), aux("きます", 1.5F, "くる"),
      aux("きた", 0.5F, "くる"), aux("こない", 1.5F, "くる"),

      // Explanatory (説明)
      aux("のだ", 0.3F, "のだ"), aux("のです", 0.3F, "のだ"), aux("のでした", 0.3F, "のだ"),
      aux("んだ", 0.3F, "のだ"), aux("んです", 0.3F, "のだ"), aux("んでした", 0.3F, "のだ"),

      // Kuruwa-kotoba (廓言葉)
      aux("ありんす", 0.3F, "ある"), aux("ありんした", 0.3F, "ある"), aux("ありんせん", 0.3F, "ある"),
      aux("ざんす", 0.3F, "ある"), aux("ざんせん", 0.3F, "ある"),
      aux("でありんす", 0.3F, "だ"), aux("でありんした", 0.3F, "だ"),
      aux("なんし", 0.3F, "ます"), aux("なんした", 0.3F, "ます"),

      // Cat-like (猫系)
      aux("にゃ", 0.3F, "よ"), aux("にゃん", 0.3F, "よ"), aux("にゃー", 0.3F, "よ"),
      aux("だにゃ", 0.01F, "だよ"), aux("だにゃん", 0.01F, "だよ"),
      aux("ですにゃ", 0.01F, "ですよ"), aux("ですにゃん", 0.01F, "ですよ"),

      // Squid character (イカ娘)
      aux("ゲソ", 0.3F, "だ", CT::None, "げそ"), aux("げそ", 0.3F, "だ"),
      aux("でゲソ", 0.3F, "だ", CT::None, "でげそ"), aux("でげそ", 0.3F, "だ"),

      // Ojou-sama/Lady speech (お嬢様言葉)
      aux("ですわ", 0.1F, "です"), aux("ですの", 0.1F, "です"),
      aux("ますの", 0.1F, "ます"), aux("だわ", 0.1F, "だ"),

      // Youth slang (若者言葉)
      aux("っす", 0.3F, "です"), aux("っした", 0.3F, "でした"), aux("っすか", 0.3F, "ですか"),

      // Rabbit-like (兎系)
      aux("ぴょん", 0.3F, "だ"), aux("ピョン", 0.3F, "だ", CT::None, "ぴょん"),

      // Ninja/Old-fashioned (忍者・古風)
      aux("ござる", 0.3F, "だ"), aux("でござる", 0.3F, "だ"),
      aux("ござった", 0.3F, "だった"), aux("でござった", 0.3F, "だった"),
      aux("ござらぬ", 0.3F, "ではない"), aux("ござらん", 0.3F, "ではない"),
      aux("でございます", 0.3F, "です"),
      aux("ナリ", 0.3F, "だ", CT::None, "なり"), aux("なり", 0.3F, "だ"),
      aux("でナリ", 0.3F, "だ", CT::None, "でなり"), aux("でなり", 0.3F, "だ"),

      // Elderly/Archaic (老人・古風)
      aux("じゃ", 0.3F, "だ"), aux("じゃな", 0.3F, "だ"),
      aux("のじゃ", 0.3F, "のだ"), aux("じゃろう", 0.3F, "だろう"),

      // Regional dialects (方言系)
      aux("ぜよ", 1.0F, "だ"), aux("だべ", 1.0F, "だ"), aux("やんけ", 1.0F, "だ"),
      aux("やで", 1.0F, "だ"), aux("やねん", 1.0F, "だ"),
      aux("だっちゃ", 1.0F, "だ"), aux("ばい", 1.0F, "だ"),

      // Robot/Mechanical (ロボット・機械)
      aux("デス", 1.2F, "です", CT::None, "です"),
      aux("マス", 1.2F, "ます", CT::None, "ます"),
  };
}

// =============================================================================
// Conjunctions (接続詞)
// =============================================================================
std::vector<DictionaryEntry> getConjunctionEntries() {
  return {
      // Sequential (順接)
      conj("従って", 0.5F, "", "したがって"), conj("故に", 1.0F, "", "ゆえに"),
      conj("そして", 1.0F), conj("それから", 1.0F), conj("すると", 1.0F),
      conj("だから", 1.0F), conj("そのため", 1.0F),
      conj("したがって", -0.5F, "従って"),

      // Adversative (逆接)
      conj("しかし", 1.0F), conj("だが", 1.0F), conj("けれども", 1.0F),
      conj("ところが", -1.5F), conj("それでも", 1.0F),
      conj("でも"), conj("だって"), conj("にもかかわらず"),
      conj("ものの", -1.0F),

      // Parallel/Addition (並列・添加)
      conj("又", 1.0F, "", "また"), conj("及び", 1.0F, "", "および"),
      conj("並びに", 1.0F, "", "ならびに"), conj("且つ", 1.0F, "", "かつ"),
      conj("更に", 1.0F, "", "さらに"),
      conj("しかも", 1.0F), conj("そのうえ", 1.0F),

      // Alternative (選択)
      conj("或いは", 1.0F, "", "あるいは"), conj("若しくは", 1.0F, "", "もしくは"),
      conj("または", 1.0F), conj("それとも", 1.0F),
      conj("あるいは", -0.5F, "或いは"),

      // Explanation/Supplement (説明・補足)
      conj("即ち", 1.0F, "", "すなわち"), conj("例えば", 1.0F, "", "たとえば"),
      conj("但し", 1.0F, "", "ただし"), conj("尚", 1.0F, "", "なお"),
      conj("つまり", 1.0F), conj("たとえば", 0.3F), conj("なぜなら", 1.0F),
      conj("ちなみに", 0.3F), conj("まして", -1.5F),

      // Topic change (転換)
      conj("さて", 1.0F), conj("ところで", -1.0F),
      conj("では", 1.0F), conj("それでは", 1.0F),

      // Addition/Emphasis
      conj("のみならず", -0.5F),

      // Additional conjunctions
      conj("いわば", 0.1F, "言わば"), conj("言わば", 0.1F, "", "いわば"),
      conj("さもないと", 0.1F), conj("さもなければ", 0.1F),
      conj("そんなら", -1.0F, "其んなら"),
      conj("それにしても", -2.0F),
      adv("ともかく", -1.5F),
      conj("いずれにしても", -2.5F), conj("いずれにせよ", -2.0F),
  };
}

// =============================================================================
// Determiners (連体詞)
// =============================================================================
std::vector<DictionaryEntry> getDeterminerEntries() {
  return {
      // Demonstrative determiners (指示連体詞) - この/その/あの/どの
      det("この"), det("その"), det("あの"), det("どの"),
      // Demonstrative determiners (指示連体詞) - こんな/そんな/あんな/どんな
      det("こんな", 0.5F), det("そんな", 0.5F), det("あんな", 0.5F), det("どんな", 0.5F),

      // Other determiners (連体詞)
      det("ある"), det("あらゆる"), det("いわゆる"), det("おかしな"),
      det("同じ", -0.3F, "おなじ"),  // same - prevent VERB confusion

      // Demonstrative manner determiners (指示様態連体詞)
      // Lower cost to compete with X + いう (VERB cost 0.3) splits
      det("こういう", -0.3F), det("そういう", -0.3F),
      det("ああいう", -0.3F), det("どういう", -0.3F),

      // Quotative determiners (引用連体詞) - prevents incorrect split like 病+とい+う
      // Lower cost to beat と(PARTICLE,-0.4)+いった(VERB,-0.034)+conn(0.2)=-0.232
      det("という", -0.2F, "", "という"),
      det("といった", -0.5F, "", "という"),
      det("っていう", 0.5F, "", "という"),  // colloquial

      // Quotative verb forms (引用動詞活用形) - to + 言う conjugations
      // These compete with と(PARTICLE) + いって(行く, cost 1.2F) paths
      verb("といって", 0.3F, "いう"),
      verb("といっては", 0.3F, "いう"),
      conj("といっても", -1.5F, "といっても"),
      verb("そういって", 0.3F, "いう"),
      verb("こういって", 0.3F, "いう"),
      verb("ああいって", 0.3F, "いう"),
      verb("どういって", 0.3F, "いう"),

      // Determiners with kanji - B51: lowered cost to prioritize over NOUN unknown
      det("大きな", 0.5F, "おおきな"), det("小さな", 0.5F, "ちいさな"),
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
      pronoun("私", 0.3F, false, "わたし"),
      pronoun("僕", 0.5F, true, "ぼく"),
      pronoun("俺", 0.3F, false, "おれ"),
      // First person - hiragana only
      pronoun("わたくし"),

      // First person plural (一人称複数)
      // Lower cost to beat single pronoun + suffix split
      pronoun("私たち", 0.2F, false, "わたしたち"),
      pronoun("僕たち", 0.2F, false, "ぼくたち"),
      pronoun("僕ら", 0.2F, false, "ぼくら"),
      pronoun("俺たち", 0.2F, false, "おれたち"),
      pronoun("俺ら", 0.2F, false, "おれら"),
      pronoun("我々", 0.5F, true, "われわれ"),

      // Second person (二人称) - kanji with reading
      pronoun("貴方", 0.5F, true, "あなた"),
      pronoun("君", 0.5F, true, "きみ"),
      // Second person - hiragana/mixed only
      pronoun("あなた"),
      // B39: お前 needs low cost to beat PREFIX(お)+NOUN(前) split (connection bonus -1.5)
      // PREFIX→NOUN path has cost ~-1.2, so お前 needs cost < -1.2 to win
      pronoun("お前", -1.5F, true, "おまえ"),

      // Second person plural (二人称複数)
      pronoun("あなたたち"),
      pronoun("君たち", 0.5F, true, "きみたち"),

      // Third person (三人称) - kanji with reading
      pronoun("彼", 0.5F, true, "かれ"),
      pronoun("彼女", 0.5F, true, "かのじょ"),
      pronoun("彼ら", 0.5F, true, "かれら"),
      pronoun("彼女ら", 0.5F, true, "かのじょら"),
      pronoun("彼女たち", 0.5F, true, "かのじょたち"),

      // Archaic/Samurai (武家・古風)
      pronoun("拙者", 0.5F, true, "せっしゃ"),
      pronoun("貴殿", 0.5F, true, "きでん"),
      pronoun("某", 0.5F, true, "それがし"),
      pronoun("我輩", 0.5F, true, "わがはい"),
      pronoun("吾輩", 0.5F, true, "わがはい"),

      // Collective pronouns (集合代名詞)
      pronoun("皆", 0.5F, true, "みな"),
      pronoun("皆さん", 0.5F, true, "みなさん"),
      pronoun("みんな"),

      // Demonstrative - proximal (近称)
      pronoun("これ"), pronoun("ここ"), pronoun("こちら"),
      // Colloquial demonstratives - prevent っち split
      pronoun("こっち", 0.1F), pronoun("そっち", 0.1F),
      pronoun("あっち", 0.1F), pronoun("どっち", 0.1F),

      // Demonstrative - medial (中称)
      pronoun("それ"), pronoun("そこ"), pronoun("そちら"),

      // Demonstrative - distal (遠称)
      pronoun("あれ", 0.8F, false),
      pronoun("あそこ"), pronoun("あちら"),

      // Demonstrative - person reference (こそあど+いつ)
      pronoun("こいつ"), pronoun("そいつ"),
      pronoun("あいつ"), pronoun("どいつ"),

      // Demonstrative - interrogative (不定称)
      pronoun("どれ"), pronoun("どこ"), pronoun("どちら"),

      // Indefinite (不定代名詞) - kanji with reading
      // Low cost to act as strong anchors against prefix compounds (今何 → 今+何)
      // Cost -1.5 to beat: 今(1.4) + 何(-1.9) + conn(0.5) = 0.0 < 今何(0.5)
      pronoun("誰", -1.5F, true, "だれ"),
      pronoun("何", -1.5F, true, "なに"),

      // Interrogatives (疑問詞)
      pronoun("いつ"), pronoun("いくつ"), pronoun("いくら"),
      adv("どう"),
      // Note: どうして needs very low cost to prevent split when followed by verb
      // The te-form bonus makes どう+して+VERB cheaper than どうして+VERB
      adv("どうして", -0.2F),
      adv("なぜ"), adv("どんな"),
  };
}

// =============================================================================
// Formal Nouns (形式名詞)
// =============================================================================
std::vector<DictionaryEntry> getFormalNounEntries() {
  return {
      // Formal nouns (形式名詞) - hiragana form is canonical in modern Japanese
      // こと/もの are grammatical function words, hiragana is preferred
      formal_noun("事", 0.3F, "こと", "こと"),
      formal_noun("こと", -0.5F),
      formal_noun("物", 0.3F, "もの", "もの"),
      formal_noun("もの", 0.25F),
      formal_noun("為", 2.0F, "", "ため"),
      formal_noun("所", 1.0F, "", "ところ"),
      formal_noun("ところ", 0.3F),
      formal_noun("時", 2.0F, "", "とき"),
      formal_noun("内", 2.0F, "", "うち"),
      formal_noun("通り", 2.0F, "", "とおり"),
      formal_noun("限り", 2.0F, "", "かぎり"),
      // Suffix-like formal nouns
      // Lower cost for 付け to compete with verb_kanji ichidan pattern
      formal_noun("付け", -0.3F, "", "つけ"),
      formal_noun("付", 0.5F, "", "つけ"),
      // Hiragana-only forms
      formal_noun("よう", 2.0F),
      formal_noun("ほう", 0.5F),  // B49: lowered cost
      formal_noun("わけ", 2.0F),
      formal_noun("はず", 0.3F, "はず", "はず"),
      formal_noun("つもり", 2.0F),
      formal_noun("まま", 2.0F),
      formal_noun("ほか", 0.3F, "ほか"),
      formal_noun("他", 0.3F, "ほか", "ほか"),
      // Abstract nouns that don't form suru-verbs
      formal_noun("仕方", 0.3F, "", "しかた"),
      formal_noun("ありきたり", -1.0F, "ありきたり"),  // Low cost to prevent あり+き+たり split, na-adjective stem
      formal_noun("たたずまい", 0.3F, "たたずまい"),  // noun, not suru-verb
      // NOTE: 〜がち forms are now handled by generateGachiSuffixCandidates() in suffix_candidates.cpp
      // B35: Idiom component (eaves bracket - used in うだつが上がらない)
      formal_noun("うだつ", 0.3F, "うだつ"),
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
      time_noun("今日", "きょう", 0.3F),
      // MOVED TO L2: 明日, 昨日, 明後日, 一昨日
      time_noun("毎日", "まいにち"),

      // Time of day (時間帯)
      // MOVED TO L2: 今朝, 昨夜
      time_noun("毎朝", "まいあさ"),
      time_noun("今晩", "こんばん"),
      time_noun("今夜", "こんや"),

      // Weeks (週)
      time_noun("今週", "こんしゅう"),
      time_noun("来週", "らいしゅう"),
      time_noun("先週", "せんしゅう"),
      time_noun("毎週", "まいしゅう"),

      // Months (月)
      time_noun("今月", "こんげつ"),
      time_noun("来月", "らいげつ"),
      time_noun("先月", "せんげつ"),
      time_noun("毎月", "まいつき"),

      // Years (年)
      time_noun("今年", "ことし"),
      time_noun("来年", "らいねん"),
      time_noun("去年", "きょねん"),
      time_noun("昨年", "さくねん"),
      time_noun("毎年", "まいとし"),

      // Other time expressions
      // Note: 今 handled by prefix+single_kanji candidate generation
      // 時分: time period, around that time (e.g., その時分, 若い時分)
      formal_noun("時分", 0.5F, "時分", "じぶん"),
      // Time-related temporal nouns (時間関係名詞)
      // 以来: since then - prevent 以来下着 over-concatenation
      formal_noun("以来", -0.5F, "以来", "いらい"),
      // 時間: time (duration) - prevent 時間すら → 時+間すら split
      formal_noun("時間", 0.1F, "時間", "じかん"),

      // Hiragana time nouns (ひらがな時間名詞) - B-7
      // あと: after - prevent あ+と particle split (N1)
      formal_noun("あと", -0.5F, "後"),
      // まえ: before - prevent まえ(OTHER) confusion (N1)
      formal_noun("まえ", -0.5F, "前"),
      // あとで: later (adverbial) - prevent あと+で split
      adv("あとで", -0.8F, "後で"),

      // Kanji time/position nouns (漢字時間/位置名詞) - prevent over-concatenation
      // 後: after - prevent 後猫 concatenation (N2)
      formal_noun("後", 0.5F, "後"),
      // 前: before/in front
      formal_noun("前", 0.5F, "前"),
      // 上: above/up - prevent 上今 concatenation (N5)
      formal_noun("上", 0.5F, "上"),
      // 下: below/down
      formal_noun("下", 0.5F, "下"),
      // 中: middle/inside (also used as suffix, but needs base entry)
      formal_noun("中", 0.5F, "中"),
      // 時: time - prevent 時妙な concatenation (N5)
      formal_noun("時", 0.5F, "時"),
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
      {"待ち", POS::Verb, 0.5F, "待つ", false, false, false, CT::None, ""},  // renyokei, base form 待つ has expansion
      {"願い", POS::Verb, 0.5F, "願う", false, false, false, CT::None, ""},  // renyokei, base form 願う has expansion

      // Nominalized verbs (連用形名詞) - prevent misanalysis as adjective
      // MOVED TO L2: 支払い, 見舞い
      {"払い", POS::Noun, 0.5F, "払い", false, false, false, CT::None, "はらい"},

      // Counter units (助数詞につく名詞) - enable 何+番線 split
      {"番線", POS::Noun, -0.5F, "番線", false, false, false, CT::None, "ばんせん"},

      // Low information verbs (低情報量動詞)
      // NOTE: ALL use CT::None to prevent duplicate expansion.
      // The main entries with expansion are in getEssentialVerbEntries().
      // These high-cost entries exist only to penalize standalone auxiliary-like usage.
      {"ある", POS::Verb, 2.0F, "ある", false, false, true, CT::None, ""},
      {"いる", POS::Verb, 2.0F, "いる", false, false, true, CT::None, ""},
      {"おる", POS::Verb, 2.0F, "おる", false, false, true, CT::None, ""},
      {"くる", POS::Verb, 2.0F, "くる", false, false, true, CT::None, ""},
      {"いく", POS::Verb, 2.0F, "いく", false, false, true, CT::None, ""},
      {"くれる", POS::Verb, 2.0F, "くれる", false, false, true, CT::None, ""},
      {"あげる", POS::Verb, 2.0F, "あげる", false, false, true, CT::None, ""},
      {"みる", POS::Verb, 2.0F, "みる", false, false, true, CT::None, ""},
      {"おく", POS::Verb, 2.0F, "おく", false, false, true, CT::None, ""},
      {"しまう", POS::Verb, 2.0F, "しまう", false, false, true, CT::None, ""},
      {"しめる", POS::Verb, 0.4F, "しめる", false, false, false, CT::Ichidan, ""},  // 占める/締める/閉める - prevent し+めた split

      // Common GodanWa/Ra/Ta verbs are defined in getEssentialVerbEntries() with readings
      {"間違う", POS::Verb, 0.5F, "間違う", false, false, false, CT::GodanWa, "まちがう"},
      {"揃う", POS::Verb, 1.0F, "揃う", false, false, false, CT::GodanWa, "そろう"},

      // Regular i-adjective よい (conjugated forms auto-generated by expandAdjectiveEntry)
      {"よい", POS::Adjective, 0.3F, "よい", false, false, false, CT::IAdjective, "よい"},
      // よ: adjective stem of よい (ガル接続形) for よさ, よそう patterns
      // MeCab: よ(形容詞,ガル接続,よい) + さ(名詞,接尾)
      // Higher cost than よ(PARTICLE,1.0) to avoid 行くよ→よ(ADJ) misparse
      // Relies on ADJ+SUFFIX(さ) connection bonus to win in よさ pattern
      {"よ", POS::Adjective, 0.8F, "よい", false, false, false, CT::None, ""},

      // Prefixes (接頭辞)
      {"お", POS::Prefix, 0.3F, "", true, false, false, CT::None, ""},
      {"ご", POS::Prefix, 0.3F, "", true, false, false, CT::None, ""},
      {"御", POS::Prefix, 1.0F, "", true, false, false, CT::None, ""},
      {"何", POS::Prefix, 0.3F, "なん", true, false, false, CT::None, ""},  // Interrogative prefix (何番線→何+番線)
      // Note: 今 (PREFIX) removed to allow 今日中 etc. as compound nouns

      // Suffixes (接尾語)
      // High-frequency productive suffixes: lower cost to prefer NOUN + SUFFIX split
      {"的", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"化", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"性", POS::Suffix, 0.5F, "", false, false, true, CT::None, ""},  // Very common (可能性, 重要性)
      {"率", POS::Suffix, 0.5F, "", false, false, true, CT::None, ""},  // Very common (確率, 降水確率)
      {"法", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"論", POS::Suffix, 1.5F, "", false, false, true, CT::None, ""},
      {"者", POS::Suffix, 0.5F, "", false, false, true, CT::None, ""},  // Very common (代表者, 関係者)
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

      // Nominalization suffix (名詞化接尾辞)
      // MeCab-compatible: 高さ → 高(ADJ stem) + さ(SUFFIX), よさ → よ(ADJ stem) + さ(SUFFIX)
      // Note: This さ is different from サ変 さ (する→さ), which is POS::Verb
      {"さ", POS::Suffix, -1.5F, "さ", false, false, true, CT::None, "さ"},

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
      {"さようなら", POS::Other, -1.0F, "さようなら", false, false, false, CT::None, ""},  // Low cost to prevent さ+よう+なら split

      // Thanks and Apologies (感謝・謝罪)
      {"ありがとう", POS::Other, 0.3F, "ありがとう", false, false, false, CT::None, ""},
      {"すみません", POS::Other, -0.7F, "すみません", false, false, false, CT::None, ""},  // Low cost to beat すみ+ませ+ん split
      {"ごめんなさい", POS::Other, -0.5F, "ごめんなさい", false, false, false, CT::None, ""},  // Prefer unified over ごめん+なさい split
      {"ごめん", POS::Other, 0.3F, "ごめん", false, false, false, CT::None, ""},

      // Interjections (感動詞) - B-6
      // はてな: what?/hmm - prevent は+て+な split (N8)
      {"はてな", POS::Other, -0.5F, "はてな", false, false, false, CT::None, ""},

      // Honorific expressions (敬語表現)
      // おいで: honorific "come/go/be" - prevent お+い+で split
      // おいでになる, おいでください patterns need this to be recognized as a unit
      {"おいで", POS::Verb, -0.5F, "いらっしゃる", false, false, false, CT::Ichidan, ""},
      // おいでになる compound form (more explicit recognition)
      {"おいでになる", POS::Verb, -1.5F, "いらっしゃる", false, false, false, CT::Ichidan, ""},
      // ご覧になる (goran ni naru) - honorific "to see/look"
      {"ご覧になる", POS::Verb, -1.5F, "ご覧になる", false, false, false, CT::Ichidan, "ごらんになる"},
      {"ご覧になった", POS::Verb, -1.5F, "ご覧になる", false, false, false, CT::Ichidan, "ごらんになった"},
      {"ご覧になって", POS::Verb, -1.5F, "ご覧になる", false, false, false, CT::Ichidan, "ごらんになって"},
      {"ご覧になれる", POS::Verb, -1.5F, "ご覧になる", false, false, false, CT::Ichidan, "ごらんになれる"},
      {"ご覧になります", POS::Verb, -1.5F, "ご覧になる", false, false, false, CT::Ichidan, "ごらんになります"},

      // Common nouns prone to incorrect splitting
      // おやじ: father (colloquial) - prevent お+やじ split
      {"おやじ", POS::Noun, -0.5F, "親父", false, false, false, CT::None, ""},
      {"親父", POS::Noun, 0.3F, "", false, false, false, CT::None, "おやじ"},
      // 猫: cat - low cost so 猫みたい = 猫+みたい (not 猫み+たい)
      // 外出: going out - prevent 外+出 split (suru-verb noun)
      // 時分: time/period - temporal noun
      {"時分", POS::Noun, 0.0F, "", false, false, false, CT::None, "じぶん"},
      // 申し訳: excuse/apology - compound noun, prevent 申し+訳 split
      {"申し訳", POS::Noun, -0.5F, "", false, false, false, CT::None, "もうしわけ"},
      // 検討: review/consideration - suru-verb noun
      // 署名: signature - suru-verb noun
      // 逢う: to meet - GodanWa verb (prevent lemma 逢る)
      {"逢う", POS::Verb, -0.5F, "逢う", false, false, false, CT::GodanWa, "あう"},
      // MOVED TO L2: 引きこもり, みじん切り
      // 胡散臭い: suspicious - compound adjective, prevent 胡散+臭い split
      {"胡散臭い", POS::Adjective, -0.5F, "", false, false, false, CT::IAdjective, "うさんくさい"},
      // 感じ/感じる: feeling/to feel - ichidan verb, nominalized noun
      {"感じる", POS::Verb, -0.5F, "感じる", false, false, false, CT::Ichidan, "かんじる"},
      {"感じ", POS::Noun, 0.5F, "", false, false, false, CT::None, "かんじ"},
  };
}

// =============================================================================
// Adverbs (副詞)
// =============================================================================
std::vector<DictionaryEntry> getAdverbEntries() {
  return {
      // Degree adverbs (程度副詞) - kanji with reading
      adv("大変", -1.0F, "", "たいへん"),
      adv("全く", 0.5F, "", "まったく"),
      adv("全然", 0.5F, "", "ぜんぜん"),
      adv("少し", 0.5F, "", "すこし"),
      adv("結構", 0.5F, "", "けっこう"),
      // Degree adverbs - hiragana only
      // Note: とても needs low cost to prevent と+て+も split
      adv("とても", 0.1F),
      adv("すごく"), adv("めっちゃ"), adv("かなり"),
      adv("たくさん", -0.5F),  // Low cost to prevent たく+さん split
      adv("沢山", -0.5F, "", "たくさん"),
      adv("もっと"), adv("ずっと"), adv("さらに"), adv("まさに"),
      adv("あまり", 0.2F),  // Lower cost to prefer ADV over verb あまる
      adv("なかなか"), adv("ほとんど"), adv("ちょっと"),
      adv("ほぼ", 0.3F),

      // Quantity/degree adverbs often incorrectly tagged as NOUN
      // Lower cost (-0.5) to beat SPLIT_NV bonus when followed by verb
      adv("少々", -0.5F, "", "しょうしょう"),
      adv("多少", -0.5F, "", "たしょう"),
      adv("若干", -0.5F, "", "じゃっかん"),
      adv("大体", -0.5F, "", "だいたい"),
      // Hiragana forms need negative cost to prevent splitting
      // MeCab-compatible: hiragana input has hiragana lemma
      adv("だいたい", -0.5F, "だいたい"),
      adv("いったい", -0.5F, "いったい"),
      // せめて needs low cost to beat VERB interpretation (せめる te-form)
      adv("せめて", -0.3F, "せめて"),
      // さすが needs low cost to beat ADJ interpretation
      // MeCab-compatible: hiragana input has hiragana lemma
      adv("さすが", 0.0F, "さすが"),
      adv("流石", 0.0F, "流石", "さすが"),

      // Manner adverbs (様態副詞)
      adv("ゆっくり"), adv("しっかり"), adv("はっきり"),
      adv("きちんと"), adv("ちゃんと"),

      // Demonstrative adverbs (指示副詞)
      adv("そう", 0.3F),
      // Appearance auxiliary そう (様態助動詞) - VERB連用形/ADJ語幹 + そう
      // E.g., 降りそう (looks like it will fall), 難しそう (looks difficult)
      // MeCab outputs 名詞,接尾,助動詞語幹 but semantically it's auxiliary
      // Cost higher than ADV (0.3F) so standalone "そうだ" prefers ADV path
      // Connection rule gives bonus for ADJ-stem/VERB-renyokei → そう(AUX)
      aux("そう", 2.5F, "そう"),
      adv("こう", 0.3F), adv("ああ", 0.3F), adv("どう", 0.3F),
      // Distributive adverbs (分配副詞)
      adv("それぞれ", 0.3F), adv("おのおの"),
      // Interrogative adverbs (疑問副詞)
      adv("どうして", 0.3F), adv("どうしても", 0.3F), adv("どうにか", 0.3F),
      adv("どうも", 0.3F), adv("いかが", 0.3F),  // how (polite)

      // Polite expression adverbs (敬語副詞)
      adv("よろしく", 0.3F),  // "please" (requesting favor)

      // Time adverbs (時間副詞)
      adv("すぐ"), adv("すぐに"),
      // 今すぐ needs low cost to beat 今(PREFIX) + すぐ(ADV) split
      adv("今すぐ", -1.0F, "", "いますぐ"),
      adv("まだ"), adv("もう"),
      // のち (後): temporal noun meaning "after/later", common in weather forecasts
      // (晴れのち曇り = sunny, later cloudy). Low cost to prevent の+ち split.
      noun("のち", 0.1F, "後"),
      adv("やっと"),
      adv("ようやく", 0.1F),  // Low cost to prevent よう+やく split
      adv("ついに"),
      adv("いつも", 0.0F),
      adv("たまに"), adv("よく"), adv("たびたび"),

      // Conditional adverbs (条件副詞)
      adv("もし"), adv("もしも"),
      adv("たとえ", 0.3F),
      adv("仮に", 0.5F, "", "かりに"),
      adv("万一", 0.5F, "", "まんいち"),

      // Contrast adverbs (対比副詞)
      adv("むしろ", 0.3F),

      // Colloquial degree adverbs (口語程度副詞)
      adv("どんだけ", 0.3F, "どれだけ"),
      adv("あんだけ", 0.3F, "あれだけ"),
      adv("こんだけ", 0.3F, "これだけ"),
      adv("そんだけ", 0.3F, "それだけ"),
      // Slang emphatic adverbs (スラング強調副詞)
      // まじで/マジで: "seriously" - prevent まじ(VERB)+で split
      adv("まじで", -0.5F, "マジで"),
      adv("マジで", -0.5F, "マジで"),
      // ガチで: "for real/seriously" - prevent ガチ(NOUN)+で split
      adv("ガチで", -0.5F),

      // Emphatic adverbs (強調副詞)
      adv("一際", 0.3F, "", "ひときわ"),
      adv("俄然", 0.3F, "", "がぜん"),  // B53: suddenly/all of a sudden
      // Degree/manner adverbs (程度副詞)
      // 別段: "particularly/especially" (often with negative), e.g., 別段恐ろしくない
      adv("別段", 0.1F, "", "べつだん"),
      adv("多分", 0.1F, "", "たぶん"),
      adv("随分", 0.1F, "", "ずいぶん"),
      adv("大分", 0.1F, "", "だいぶ"),
      // とんと: "at all/completely" (with negative), e.g., とんと分からない
      // Low cost to prevent と+ん+と particle split
      adv("とんと", 0.1F),
      // てっきり: "surely/certainly" (mistaken assumption), e.g., てっきり来ると思った
      adv("てっきり", 0.3F),
      // からっきし: "not at all" (with negative), e.g., からっきし駄目だ
      adv("からっきし", 0.3F),

      // Superlative adverbs (最上級副詞)
      adv("一番", 0.3F, "", "いちばん"),

      // Affirmation/Negation adverbs - kanji with reading
      adv("必ず", 0.5F, "", "かならず"),
      adv("決して", 0.5F, "", "けっして"),
      // Affirmation/Negation - hiragana only
      adv("きっと"), adv("たぶん"), adv("おそらく"),

      // Other common adverbs - kanji with reading
      adv("実は", 0.5F, "", "じつは"),
      adv("特に", 0.5F, "", "とくに"),
      adv("主に", 0.5F, "", "おもに"),
      // Other - hiragana only
      adv("やはり"), adv("やっぱり"),

      // Sequence adverbs (順序副詞)
      adv("まず", 0.3F),
      adv("先ず", 0.3F, "", "まず"),

      // Formal/Business adverbs (敬語・ビジネス)
      adv("何卒", 0.3F, "", "なにとぞ"),
      adv("誠に", 0.3F, "", "まことに"),
      adv("甚だ", 0.5F, "", "はなはだ"),
      noun("恐縮", 0.3F),  // with reading きょうしゅく

      // Onomatopoeia/Mimetic adverbs (オノマトペ・擬態語副詞) - B61/B62
      // These have opaque etymology and cannot be inferred from patterns
      adv("せっかく", 0.3F), adv("うっかり", 0.3F), adv("すっかり", 0.3F),
      adv("ぴったり", 0.3F), adv("さっぱり", 0.3F), adv("あっさり", 0.3F),
      adv("こっそり", 0.3F), adv("ぐっすり", 0.3F), adv("めっきり", 0.3F),
      adv("がっかり", 0.3F), adv("びっくり", 0.3F), adv("ちゃっかり", 0.3F),
      adv("のんびり", 0.3F), adv("ぼんやり", 0.3F), adv("どっさり", 0.3F),
      adv("どさり", 0.3F),   // ABり型: 重い物が落ちる音
      adv("ばったり", 0.3F),
      adv("ばたり", 0.3F),   // ABり型: 倒れる音
      adv("がたり", 0.3F),   // ABり型: 硬い物がぶつかる音
      adv("きっちり", 0.3F), adv("ぎっしり", 0.3F), adv("じっくり", 0.3F),
      adv("すっきり", 0.3F), adv("ばっちり", 0.3F), adv("ざっくり", 0.3F),
      adv("たっぷり", 0.3F), adv("ちょっぴり", 0.3F), adv("こってり", 0.3F),
      adv("ふっくら", 0.3F), adv("ぽっかり", 0.3F), adv("むっつり", 0.3F),
      // B36: 思わず is a lexicalized adverb (involuntarily/unconsciously)
      adv("思わず", 0.3F, "", "おもわず"),
      // B40: とっとと is an emphatic adverb (quickly/hurry up)
      adv("とっとと", 0.3F),

      // D1: High-priority adverbs with opaque etymology (prone to missplit)
      // Confirmation/Understanding
      adv("なるほど", 0.3F, "成程"),  // I see/indeed
      adv("もちろん", 0.3F, "勿論"),  // of course
      // State/Degree
      adv("もはや", 0.3F, "最早"),    // no longer/already
      adv("はるかに", 0.3F, "遥かに"),  // far/by far
      adv("まるで", 0.3F, "丸で"),    // as if/completely
      adv("とっても", 0.3F),          // very (colloquial)
      adv("だんだん", 0.3F, "段々"),  // gradually
      // Surprise/Unexpectedness
      adv("まさか", 0.3F),            // no way/surely not
      adv("いきなり", 0.3F),          // suddenly
      adv("ふと", 0.3F),              // suddenly/casually
      adv("たちまち", 0.3F, "忽ち"),  // immediately
      // Hypothetical/Estimation
      adv("もしかして", 0.3F, "若しかして"),  // perhaps
      adv("ひとまず", 0.3F, "一先ず"),        // for now
      adv("とりあえず", 0.3F, "取り敢えず"),  // for the time being
      // Volition/Attitude
      adv("ぜひ", 0.3F, "是非"),          // by all means
      adv("あくまで", 0.3F, "飽くまで"),  // to the end
      adv("なるべく", 0.3F, "成る可く"),  // as much as possible
      // Manner
      adv("なんとなく", 0.3F, "何となく"),  // somehow
      adv("いかにも", 0.3F, "如何にも"),    // indeed/truly
      adv("かえって", 0.3F, "却って"),      // on the contrary
      adv("すでに", 0.3F, "既に"),          // already
      adv("つねに", 0.3F, "常に"),          // always
      adv("ただちに", 0.3F, "直ちに"),      // immediately
      // Other
      adv("ただ", 0.3F, "只"),              // just/only
      adv("とにかく", 0.3F, "兎に角"),      // anyway
      adv("とりわけ", 0.3F, "取り分け"),    // especially
      adv("かろうじて", 0.3F, "辛うじて"),  // barely
      adv("あらためて", 0.3F, "改めて"),    // again/anew
      adv("あいかわらず", 0.3F, "相変わらず"),  // as usual
      adv("さっそく", 0.3F, "早速"),        // immediately

      // D2: Medium-priority adverbs (less commonly missplit)
      adv("いざ", 0.3F),              // when it comes to
      adv("やたら", 0.3F),            // randomly/excessively
      adv("およそ", 0.3F, "凡そ"),    // roughly/approximately
      adv("やがて", 0.3F),            // before long/soon
      adv("つい", 1.5F),              // unintentionally (higher cost to avoid に+つい+て split)
      adv("いまだ", 0.3F, "未だ"),    // still/not yet
      adv("しばしば", 0.3F),          // often
      adv("ともあれ", 0.3F),          // anyway
      adv("ふんだん", 0.3F),          // abundantly
      adv("のちほど", 0.3F, "後程"),  // later
      adv("いちはやく", 0.3F, "いち早く"),  // quickly/first
      adv("とかく", 0.3F),            // apt to/prone to
      adv("あらかじめ", 0.3F, "予め"),  // in advance
      adv("おおむね", 0.3F, "概ね"),    // mostly
      adv("わりと", 0.3F, "割と"),      // relatively
      adv("あいにく", 0.3F, "生憎"),    // unfortunately
      adv("たいてい", 0.3F, "大抵"),    // usually
      adv("なんだか", 0.3F),            // somehow
      adv("あれこれ", 0.3F),            // this and that
      adv("なおさら", 0.3F, "尚更"),    // all the more
      adv("なにげなく", 0.3F, "何気なく"),  // casually
      adv("ひょっとして", 0.3F),        // possibly
      adv("どうやら", 0.3F),            // apparently
      adv("いっさい", 0.3F, "一切"),    // at all
      adv("ひたすら", 0.3F),            // earnestly
      adv("あえて", 0.3F, "敢えて"),    // daringly
      adv("どうせ", 0.3F),              // anyway/after all
      adv("いっそ", 0.3F),              // rather
      adv("せいぜい", 0.3F, "精々"),    // at most
      adv("つくづく", 0.3F),            // deeply/keenly
      // Onomatopoeia/mimetic words (prevent particle splits)
      adv("ほんわか", 0.3F),            // gently warm
      adv("しどろもどろ", 0.3F),        // incoherent

      // Livedoor corpus fixes: closed-class adverbs mistokenized as OTHER
      adv("いずれ", 0.1F),              // eventually/either
      adv("早くも", -1.5F, "", "はやくも"),  // already/so soon

      // Additional adverbs from comprehensive fix plan (B-1)
      adv("全て", 0.1F, "", "すべて"),      // all - prevent 全｜て split
      adv("すべて", 0.1F, "全て"),          // all (hiragana)
      adv("実に", -0.5F, "", "じつに"),     // indeed - prevent 実｜に split (N7)
      adv("最も", 0.1F, "", "もっとも"),    // most - prevent 最｜も split
      adv("もっとも", 0.1F, "最も"),        // most (hiragana)
      adv("なんとも", 0.1F),                // cannot - prevent な｜ん｜と｜も split
      adv("今や", -0.5F, "", "いまや"),     // now - prevent 今+や(OTHER)
      adv("初めて", -0.5F, "", "はじめて"), // first time - prevent VERB confusion
      adv("到底", -1.0F, "", "とうてい"),   // utterly - beat SPLIT_NV NOUN
      adv("再び", -0.5F, "", "ふたたび"),   // again - prevent NOUN confusion

      // Compound adverbs (複合副詞) - functional expressions
      // その上: "moreover/in addition" - needs low cost to beat その+上今 compound
      adv("その上", -0.5F, "", "そのうえ"),
      // その後: "after that" - needs low cost to beat その+後猫 compound
      adv("その後", -0.5F, "", "そのあと"),
      // 第一: "first of all/firstly" - adverbial usage in classical/formal text
      // Low cost (-0.5) to split patterns like 第一毛. Compounds like 第一回 are
      // protected by their dictionary entries or unknown compound generation.
      adv("第一", -0.5F, "", "だいいち"),

      // Closed-class adverbs prone to missplit
      // なにしろ: "anyway, at any rate" - prevent なに+しろ(VERB) split
      // Very low cost needed because なに(PRON) + しろ(VERB) is a strong parse
      adv("なにしろ", -1.5F, "何しろ"),
      adv("何しろ", -1.5F, "", "なにしろ"),
      // あたかも: "as if, just like" - prevent あた+か+も split
      adv("あたかも", 0.1F, "恰も"),
      adv("恰も", 0.1F, "", "あたかも"),
      // さながら: "just like, as though" - prevent さ+ながら split
      adv("さながら", 0.1F, "宛ら"),
      // もしかすると: "perhaps, maybe" - prevent も+しか+する+と split
      adv("もしかすると", 0.1F),
      // ちっとも: "not at all" (with negative) - prevent ちっ+と+も split
      adv("ちっとも", 0.1F),
      // 大いに: "greatly, very much" - prevent 大い(NOUN)+に split
      adv("大いに", 0.1F, "", "おおいに"),
      // 今更 / いまさら: "at this late hour" - prevent いまさ(ADJ)+ら split
      adv("今更", 0.1F, "", "いまさら"),
      adv("いまさら", 0.1F, "今更"),
      // 咄嗟に / とっさに: "instantly, reflexively" - prevent とっさ(ADJ)+に split
      adv("咄嗟に", 0.1F, "", "とっさに"),
      adv("とっさに", 0.1F, "咄嗟に"),
      // 頻りに / しきりに: "frequently, incessantly" - prevent しきり(VERB)+に split
      adv("頻りに", 0.1F, "", "しきりに"),
      adv("しきりに", 0.1F, "頻りに"),

      // Adverbs incorrectly tagged as OTHER
      // さほど: "not so much" (with negative) - prevent さ+ほど split
      adv("さほど", 0.1F),
      // もっぱら: "exclusively, solely" - prevent も+っぱ+ら split
      adv("もっぱら", 0.1F, "専ら"),
      adv("専ら", 0.1F, "", "もっぱら"),

      // Hiragana adverbs incorrectly split after particles
      // e.g., がくれぐれ → が + くれ + ぐれ (wrong) → が + くれぐれ (correct)
      adv("くれぐれ", 0.1F),
      adv("くれぐれも", 0.0F),
      adv("しょっちゅう", 0.1F),
      adv("あっけらかん", 0.1F),
  };
}

// =============================================================================
// Na-Adjectives (形容動詞)
// =============================================================================
std::vector<DictionaryEntry> getNaAdjectiveEntries() {
  return {
      // Common na-adjectives - kanji with reading
      na_adj("新た", "あらた"), na_adj("明らか", "あきらか"),
      na_adj("静か", "しずか"), na_adj("綺麗", "きれい"),
      na_adj("大切", "たいせつ"), na_adj("大事", "だいじ"),
      na_adj("必要", "ひつよう"), na_adj("簡単", "かんたん"),
      na_adj("丁寧", "ていねい"), na_adj("正確", "せいかく"),
      // NOTE: 自然 removed - 自然言語処理 等の複合語保持を優先
      na_adj("普通", "ふつう"), na_adj("特別", "とくべつ"),
      na_adj("便利", "べんり"), na_adj("不便", "ふべん"),
      na_adj("有名", "ゆうめい"), na_adj("複雑", "ふくざつ"),
      na_adj("単純", "たんじゅん"), na_adj("重要", "じゅうよう"),
      na_adj("確か", "たしか"), na_adj("様々", "さまざま"),
      na_adj("勝手", "かって"), na_adj("慎重", "しんちょう"),
      na_adj("上手", "じょうず"),

      // Additional na-adjectives for adverb forms
      na_adj("非常", "ひじょう"), na_adj("本当", "ほんとう"),
      na_adj("頻繁", "ひんぱん"), na_adj("確実", "かくじつ"),
      na_adj("同様", "どうよう"), na_adj("大胆", "だいたん"),
      na_adj("果敢", "かかん"), na_adj("無理", "むり"),
      na_adj("強引", "ごういん"), na_adj("地味", "じみ"),
      na_adj("微妙", "びみょう"), na_adj("唐突", "とうとつ"),
      na_adj("永遠", "えいえん"), na_adj("永久", "えいきゅう"),
      na_adj("無限", "むげん"), na_adj("滅多", "めった"),
      na_adj("一緒", "いっしょ"),

      // Single-kanji na-adjectives (単漢字形容動詞)
      // These need explicit entries to avoid single_kanji penalty (+2.0)
      // 妙: "strange/mysterious" - common in classical text (この時妙なものだと思った)
      na_adj("妙", "みょう", 0.3F),

      // Adverbial na-adjectives
      na_adj("流石", "さすが"),

      // Hiragana na-adjectives
      na_adj("だめ", "", 0.3F, "だめ"),
      // NOTE: Higher cost (0.6) to prevent "読みたい" from being parsed as 読+みたい
      // instead of 読み+たい (want to read). Legitimate uses like "猫みたい" still work
      // because 猫 as dictionary NOUN has lower cost than 読 as unknown NOUN.
      na_adj("みたい", "", 0.6F, "みたい"),

      // Business/formal na-adjectives
      na_adj("幸い", "さいわい", 0.3F),

      // Common na-adjectives - emotional/preference
      na_adj("好き", "すき", 0.3F), na_adj("嫌", "いや", 0.3F),
      na_adj("嫌い", "きらい", 0.3F), na_adj("大好き", "だいすき", 0.3F),
      na_adj("大嫌い", "だいきらい", 0.3F),
  };
}

// =============================================================================
// I-Adjectives (形容詞)
// =============================================================================
std::vector<DictionaryEntry> getIAdjectiveEntries() {
  return {
      // Existential adjective 無い (non-existence) - kanji form only
      // Hiragana ない is already registered as negation auxiliary, no reading expansion
      i_adj("無い", "", 0.3F, "無い"),

      // Temperature/Sensation
      i_adj("寒い", "さむい", 0.3F, "寒い"),
      i_adj("暑い", "あつい", 0.3F, "暑い"),
      i_adj("熱い", "あつい", 0.3F, "熱い"),
      i_adj("痛い", "いたい", 0.3F, "痛い"),
      i_adj("怖い", "こわい", 0.3F, "怖い"),
      i_adj("辛い", "からい", 0.3F, "辛い"),
      i_adj("甘い", "あまい", 0.3F, "甘い"),
      i_adj("苦い", "にがい", 0.3F, "苦い"),
      i_adj("酸い", "すい", 0.3F, "酸い"),
      i_adj("臭い", "くさい", 0.3F, "臭い"),
      i_adj("眠い", "ねむい", 0.3F, "眠い"),
      i_adj("硬い", "かたい", 0.3F, "硬い"),

      // Colors
      i_adj("赤い", "あかい", 0.3F, "赤い"),
      i_adj("青い", "あおい", 0.3F, "青い"),
      i_adj("白い", "しろい", 0.3F, "白い"),
      i_adj("黒い", "くろい", 0.3F, "黒い"),

      // Size/Shape
      i_adj("長い", "ながい", 0.3F, "長い"),
      i_adj("高い", "たかい", 0.3F, "高い"),
      i_adj("低い", "ひくい", 0.3F, "低い"),
      i_adj("広い", "ひろい", 0.3F, "広い"),
      i_adj("狭い", "せまい", 0.3F, "狭い"),
      i_adj("太い", "ふとい", 0.3F, "太い"),
      i_adj("細い", "ほそい", 0.3F, "細い"),
      i_adj("深い", "ふかい", 0.3F, "深い"),
      i_adj("浅い", "あさい", 0.3F, "浅い"),
      i_adj("丸い", "まるい", 0.3F, "丸い"),
      i_adj("厚い", "あつい", 0.3F, "厚い"),
      i_adj("薄い", "うすい", 0.3F, "薄い"),
      i_adj("重い", "おもい", 0.3F, "重い"),
      i_adj("軽い", "かるい", 0.3F, "軽い"),
      i_adj("固い", "かたい", 0.3F, "固い"),
      i_adj("濃い", "", 0.3F, "濃い"),
      i_adj("短い", "みじかい", 0.3F, "短い"),

      // Time/Speed
      i_adj("早い", "はやい", 0.3F, "早い"),
      i_adj("速い", "はやい", 0.3F, "速い"),
      i_adj("遅い", "おそい", 0.3F, "遅い"),
      i_adj("近い", "ちかい", 0.3F, "近い"),
      i_adj("遠い", "とおい", 0.3F, "遠い"),
      i_adj("古い", "ふるい", 0.3F, "古い"),
      i_adj("新しい", "あたらしい", 0.3F, "新しい"),
      i_adj("若い", "わかい", 0.3F, "若い"),

      // Quality/Evaluation
      i_adj("良い", "よい", 0.3F, "良い"),
      i_adj("善い", "よい", 0.3F, "よい"),  // classical variant of 良い
      i_adj("好い", "よい", 0.3F, "よい"),  // classical variant of 良い
      i_adj("悪い", "わるい", 0.3F, "悪い"),
      i_adj("強い", "つよい", 0.3F, "強い"),
      i_adj("弱い", "よわい", 0.3F, "弱い"),
      i_adj("安い", "やすい", 0.3F, "安い"),
      i_adj("汚い", "きたない", 0.3F, "汚い"),
      i_adj("醜い", "みにくい", 0.3F, "醜い"),
      i_adj("鋭い", "するどい", 0.3F, "鋭い"),
      i_adj("鈍い", "にぶい", 0.3F, "鈍い"),

      // Quantity
      i_adj("多い", "おおい", 0.3F, "多い"),

      // Other single-kanji + い
      i_adj("暗い", "くらい", 0.3F, "暗い"),
      i_adj("狡い", "ずるい", 0.3F, "狡い"),
      i_adj("旨い", "うまい", 0.3F, "旨い"),
      i_adj("上手い", "うまい", 0.3F, "上手い"),
      i_adj("うまい", "", 0.2F, "うまい"),  // neutral lemma for hiragana
      i_adj("尊い", "とうとい", 0.3F, "尊い"),

      // Special irregular adjective いい (良い/よい variant)
      // Uses CT::None because いい only has terminal/attributive forms.
      // All conjugated forms use よ- stem (よかった, よくない, etc.), not い-.
      // Auto-expansion would incorrectly generate いかった, いくない, etc.
      adj("いい", 0.3F, "よい", CT::None, ""),

      // Hiragana-only adjectives (prevent particle/verb misparse)
      // These adjectives have no kanji forms or the kanji form is rare
      i_adj("めでたい", "", 0.3F, "めでたい"),       // auspicious
      i_adj("ありがたい", "", 0.3F, "ありがたい"),   // grateful
      i_adj("おもしろい", "", 0.3F, "おもしろい"),   // interesting
      i_adj("やわらかい", "", 0.3F, "やわらかい"),   // soft
      i_adj("あたたかい", "", 0.3F, "あたたかい"),   // warm
      i_adj("つめたい", "", -3.0F, "つめたい"),      // cold (low cost to beat verb+たい pattern)
      i_adj("つまらない", "", 0.3F, "つまらない"),   // boring
      i_adj("たまらない", "", 0.3F, "たまらない"),   // unbearable
      i_adj("くだらない", "", 0.3F, "くだらない"),   // trivial
      i_adj("みっともない", "", 0.3F, "みっともない"), // disgraceful
      i_adj("しょうもない", "", 0.3F, "しょうもない"), // worthless
      i_adj("おとなしい", "", 0.3F, "おとなしい"),   // quiet/gentle
      i_adj("すばらしい", "", 0.3F, "すばらしい"),   // wonderful
      i_adj("美味しい", "おいしい", -0.3F, "美味しい"),  // delicious (multi-kanji しい-adj)
      i_adj("おいしい", "", -0.5F, "おいしい"),      // delicious (hiragana, prefer over kanji)
      // Additional hiragana adjectives (prevent particle/suru misparse)
      i_adj("あぶない", "", 0.3F, "あぶない"),       // dangerous
      // だらしない: MeCab splits as だらし(NOUN,ナイ形容詞語幹)+ない(AUX) but
      // grammatically it's a single i-adjective (ナイ形容詞). We prioritize grammar.
      i_adj("だらしない", "", -2.0F, "だらしない"),
      i_adj("ねたい", "", -3.5F, "ねたい"),          // envious/jealous (low cost to beat ね+たい split)
      i_adj("ものたりない", "", 0.3F, "ものたりない"),   // unsatisfying
      i_adj("いたたまれない", "", 0.3F, "いたたまれない"), // unbearable
      i_adj("かたじけない", "", 0.3F, "かたじけない"),   // grateful (archaic)
      i_adj("おびただしい", "", 0.3F, "おびただしい"),   // enormous
      i_adj("かまびすしい", "", 0.3F, "かまびすしい"),   // noisy
      i_adj("うやうやしい", "", 0.3F, "うやうやしい"),   // respectful
      i_adj("こころもとない", "", 0.3F, "こころもとない"), // anxious/uneasy

      // Classical adjective forms (古語形容詞終止形) - シク活用
      // Only add forms that don't conflict with modern 〜そう patterns
      // 寒し = 寒い終止形 (寒さは寒し pattern in wagahai)
      adj("寒し", 0.3F, "寒い", CT::None, "さむし"),
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
      // Verbs starting with particle-like chars (prevent し+みる, た+ずねる, ね+て splits)
      {"しみる", POS::Verb, 0.3F, "しみる", false, false, false, CT::Ichidan, ""},
      {"たずねる", POS::Verb, 0.3F, "たずねる", false, false, false, CT::Ichidan, ""},
      // B62: たおれる starts with た (AUX), prevents たおれる → た+おれる split
      {"たおれる", POS::Verb, -0.3F, "たおれる", false, false, false, CT::Ichidan, ""},
      {"たおれ", POS::Verb, -0.3F, "たおれる", false, false, false, CT::None, ""},  // 連用形
      // B47: ねる starts with ね (終助詞), prevents ねて → ね+て (particle) split
      {"ねる", POS::Verb, 0.3F, "ねる", false, false, false, CT::Ichidan, ""},
      {"おぼれる", POS::Verb, 0.3F, "おぼれる", false, false, false, CT::Ichidan, ""},
      {"そげる", POS::Verb, 0.3F, "そげる", false, false, false, CT::Ichidan, ""},
      {"あきらめる", POS::Verb, 0.3F, "あきらめる", false, false, false, CT::Ichidan, ""},
      {"たばねる", POS::Verb, 0.3F, "たばねる", false, false, false, CT::Ichidan, ""},  // to bundle
      // B61: のろける starts with の (particle), prevents のろけて → の+ろけて split
      // の is blocked by isNeverVerbStemAtStart, so we need explicit 連用形 entry
      {"のろける", POS::Verb, 0.3F, "のろける", false, false, false, CT::Ichidan, ""},
      {"のろけ", POS::Verb, -0.5F, "のろける", false, false, false, CT::None, ""},  // 連用形

      // Godan-Ka verbs (五段カ行)
      // いく/ゆく: fundamental verb meaning "to go"
      // Higher cost to not compete with て-form+いく compound patterns
      // (走っていく, 見ていく should prefer compound form)
      // but still recognized for standalone usage (うまくいく)
      // NOTE: ひらがな いく has irregular past (いった not いいた), so explicit forms are needed.
      // CT::None prevents incorrect expansion from these conjugated forms.
      {"いく", POS::Verb, 1.2F, "いく", false, false, false, CT::None, ""},  // base form only, no expansion
      {"いっ", POS::Verb, 0.5F, "いく", false, false, false, CT::None, ""},  // sokuonbin form: いった→いっ+た, いって→いっ+て
      // いう (言う) sokuonbin form - GodanWa has っ-onbin
      // MeCab: そういって → そう + いっ(lemma=いう) + て
      // Slightly higher cost than いく - connection rules handle context
      {"いっ", POS::Verb, 0.8F, "いう", false, false, false, CT::None, ""},
      {"いかない", POS::Verb, 1.2F, "いく", false, false, false, CT::None, ""},
      {"いかなかった", POS::Verb, 1.2F, "いく", false, false, false, CT::None, ""},
      // いった/いって: MeCab splits as いっ+た/いっ+て, so no full-form entries needed
      // The split form is enabled by the いっ entry above + connection rules
      {"いける", POS::Verb, 1.2F, "いく", false, false, false, CT::None, ""},
      {"いけない", POS::Verb, 1.2F, "いく", false, false, false, CT::None, ""},
      {"いけなかった", POS::Verb, 1.2F, "いく", false, false, false, CT::None, ""},
      {"ゆく", POS::Verb, 1.2F, "いく", false, false, false, CT::None, ""},
      {"ゆかない", POS::Verb, 1.2F, "いく", false, false, false, CT::None, ""},
      // くる: fundamental verb meaning "to come"
      // Higher cost to not compete with て-form+くる compound patterns
      // (見てくる, 帰ってくる should prefer compound form)
      // but still recognized for standalone usage (気が来る)
      // NOTE: Most conjugated forms are auto-generated by CT::Kuru expansion.
      // Only non-generated forms are listed explicitly.
      {"くる", POS::Verb, 1.2F, "くる", false, false, false, CT::Kuru, ""},
      {"きている", POS::Verb, 1.2F, "くる", false, false, false, CT::None, ""},  // progressive, not generated
      {"こられない", POS::Verb, 1.2F, "くる", false, false, false, CT::None, ""},  // potential negative, not generated
      // 来る (kanji form): fundamental verb meaning "to come"
      // NOTE: Most conjugated forms are auto-generated by CT::Kuru expansion.
      // Only non-generated or special-cost forms are listed explicitly.
      {"来", POS::Verb, -0.5F, "来る", false, false, false, CT::None, ""},  // renyokei: 来ます, 来ている (low cost to offset single_kanji penalty)
      {"来る", POS::Verb, 0.3F, "来る", false, false, false, CT::Kuru, ""},
      // 来て removed: MeCab splits as 来+て (particle)
      // いただく系 (itadaku - to receive, honorific)
      // Uses explicit forms like other honorific verbs (not auto-expanded)
      // This ensures いただきます stays as single token (not split as いただき+ます)
      {"いただく", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
      {"いただいて", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
      {"いただいた", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
      {"いただき", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
      {"いただかない", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
      {"いただきます", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
      {"いただきたい", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
      {"いただける", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
      {"いただけます", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
      {"いただければ", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
      {"いただけない", POS::Verb, 0.3F, "いただく", false, false, false, CT::None, ""},
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
      {"分かる", POS::Verb, 0.3F, "分かる", false, false, false, CT::GodanRa, "わかる"},
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
      // MeCab-compatible: もらわない → もらわ(VERB) + ない(AUX)
      // CT::None to prevent expansion - this is the mizenkei form directly
      {"もらわ", POS::Verb, -0.5F, "もらう", false, false, true, CT::None, "もらわ"},
      // いわ/いわれ - for hiragana passive patterns like いわれる, いわれません
      // Note: NOT adding いう as it would break quotative patterns (っていう, etc.)
      // Verbs starting with particle-like chars (prevent した+が+う, ため+ら+う splits)
      {"したがう", POS::Verb, 0.3F, "したがう", false, false, false, CT::GodanWa, ""},
      {"ためらう", POS::Verb, 0.3F, "ためらう", false, false, false, CT::GodanWa, ""},
      // うかがう (伺う) - prevent う+か+が+う split
      {"うかがう", POS::Verb, 0.3F, "うかがう", false, false, false, CT::GodanWa, ""},
      {"たわむ", POS::Verb, 0.3F, "たわむ", false, false, false, CT::GodanMa, ""},
      {"たたずむ", POS::Verb, 0.3F, "たたずむ", false, false, false, CT::GodanMa, ""},  // to stand still

      // Common GodanMa verbs (五段マ行)
      // These need explicit entries for correct hatsuonbin lemma (飲んだ → 飲む)
      {"飲む", POS::Verb, 0.3F, "飲む", false, false, false, CT::GodanMa, "のむ"},
      {"読む", POS::Verb, 0.3F, "読む", false, false, false, CT::GodanMa, "よむ"},
      {"進む", POS::Verb, 0.3F, "進む", false, false, false, CT::GodanMa, "すすむ"},
      {"住む", POS::Verb, 0.3F, "住む", false, false, false, CT::GodanMa, "すむ"},
      {"組む", POS::Verb, 0.3F, "組む", false, false, false, CT::GodanMa, "くむ"},
      {"込む", POS::Verb, 0.3F, "込む", false, false, false, CT::GodanMa, "こむ"},
      {"包む", POS::Verb, 0.3F, "包む", false, false, false, CT::GodanMa, "つつむ"},
      {"踏む", POS::Verb, 0.3F, "踏む", false, false, false, CT::GodanMa, "ふむ"},
      {"頼む", POS::Verb, 0.3F, "頼む", false, false, false, CT::GodanMa, "たのむ"},
      {"楽しむ", POS::Verb, 0.3F, "楽しむ", false, false, false, CT::GodanMa, "たのしむ"},
      {"休む", POS::Verb, 0.3F, "休む", false, false, false, CT::GodanMa, "やすむ"},
      {"噛む", POS::Verb, 0.3F, "噛む", false, false, false, CT::GodanMa, "かむ"},
      {"産む", POS::Verb, 0.3F, "産む", false, false, false, CT::GodanMa, "うむ"},
      {"沈む", POS::Verb, 0.3F, "沈む", false, false, false, CT::GodanMa, "しずむ"},
      {"掴む", POS::Verb, 0.3F, "掴む", false, false, false, CT::GodanMa, "つかむ"},
      {"望む", POS::Verb, 0.3F, "望む", false, false, false, CT::GodanMa, "のぞむ"},
      {"悩む", POS::Verb, 0.3F, "悩む", false, false, false, CT::GodanMa, "なやむ"},

      // Godan-Sa verbs (五段サ行)
      {"いたす", POS::Verb, 0.3F, "いたす", false, false, false, CT::GodanSa, ""},
      {"いたしております", POS::Verb, -0.5F, "いたす", false, false, false, CT::GodanSa, ""},  // humble progressive
      {"いたします", POS::Verb, -0.3F, "いたす", false, false, false, CT::GodanSa, ""},  // humble polite
      // Verbs starting with particle-like chars (prevent も+たらす splits)
      {"もたらす", POS::Verb, 0.3F, "もたらす", false, false, false, CT::GodanSa, ""},

      // Godan-Ta verbs (五段タ行) - particle-like starts
      {"たもつ", POS::Verb, 0.3F, "たもつ", false, false, false, CT::GodanTa, ""},

      // Godan-Ga verbs (五段ガ行) - particle-like starts
      {"しのぐ", POS::Verb, 0.3F, "しのぐ", false, false, false, CT::GodanGa, ""},
      // Kanji compound verbs (漢字複合動詞) - common GodanGa patterns
      {"相次ぐ", POS::Verb, 0.3F, "相次ぐ", false, false, false, CT::GodanGa, ""},  // to follow one after another
      // Common GodanGa verbs (for イ音便 lemma resolution: 泳い → 泳ぐ)
      {"泳ぐ", POS::Verb, 0.3F, "泳ぐ", false, false, false, CT::GodanGa, "およぐ"},
      {"漕ぐ", POS::Verb, 0.3F, "漕ぐ", false, false, false, CT::GodanGa, "こぐ"},
      {"防ぐ", POS::Verb, 0.3F, "防ぐ", false, false, false, CT::GodanGa, "ふせぐ"},
      {"注ぐ", POS::Verb, 0.3F, "注ぐ", false, false, false, CT::GodanGa, "そそぐ"},
      {"騒ぐ", POS::Verb, 0.3F, "騒ぐ", false, false, false, CT::GodanGa, "さわぐ"},

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
      // MeCab-compatible: くださった → くださっ + た
      // 促音便 stem enables て/た splitting
      {"くださる", POS::Verb, 0.1F, "くださる", false, false, false, CT::None, ""},
      {"くださっ", POS::Verb, 0.1F, "くださる", false, false, false, CT::None, ""},
      {"ください", POS::Verb, 0.1F, "くださる", false, false, false, CT::None, ""},
      {"くださら", POS::Verb, 0.3F, "くださる", false, false, false, CT::None, ""},
      {"くださり", POS::Verb, 0.3F, "くださる", false, false, false, CT::None, ""},
      {"くださいます", POS::Verb, 0.3F, "くださる", false, false, false, CT::None, ""},
      {"くださいませ", POS::Verb, 0.3F, "くださる", false, false, false, CT::None, ""},
      // おっしゃる系 (ossharu - to say, honorific)
      {"おっしゃる", POS::Verb, 0.3F, "おっしゃる", false, false, false, CT::None, ""},
      {"おっしゃっ", POS::Verb, 0.3F, "おっしゃる", false, false, false, CT::None, ""},
      {"おっしゃい", POS::Verb, 0.3F, "おっしゃる", false, false, false, CT::None, ""},
      {"おっしゃら", POS::Verb, 0.3F, "おっしゃる", false, false, false, CT::None, ""},
      {"おっしゃり", POS::Verb, 0.3F, "おっしゃる", false, false, false, CT::None, ""},
      {"おっしゃいます", POS::Verb, 0.3F, "おっしゃる", false, false, false, CT::None, ""},
      // いらっしゃる系 (irassharu - to be/go/come, honorific)
      {"いらっしゃる", POS::Verb, 0.3F, "いらっしゃる", false, false, false, CT::None, ""},
      {"いらっしゃっ", POS::Verb, 0.3F, "いらっしゃる", false, false, false, CT::None, ""},
      {"いらっしゃい", POS::Verb, 0.3F, "いらっしゃる", false, false, false, CT::None, ""},
      {"いらっしゃら", POS::Verb, 0.3F, "いらっしゃる", false, false, false, CT::None, ""},
      {"いらっしゃり", POS::Verb, 0.3F, "いらっしゃる", false, false, false, CT::None, ""},
      {"いらっしゃいます", POS::Verb, 0.3F, "いらっしゃる", false, false, false, CT::None, ""},
      {"いらっしゃいませ", POS::Verb, 0.3F, "いらっしゃる", false, false, false, CT::None, ""},
      // なさる系 (nasaru - to do, honorific)
      // Lower cost for なさっ to beat な(PARTICLE) + さっ(する) path
      {"なさる", POS::Verb, 0.3F, "なさる", false, false, false, CT::None, ""},
      {"なさっ", POS::Verb, -0.5F, "なさる", false, false, false, CT::None, ""},
      {"なさい", POS::Verb, 0.3F, "なさる", false, false, false, CT::None, ""},
      {"なさら", POS::Verb, 0.3F, "なさる", false, false, false, CT::None, ""},
      {"なさり", POS::Verb, 0.3F, "なさる", false, false, false, CT::None, ""},
      {"なさいます", POS::Verb, 0.3F, "なさる", false, false, false, CT::None, ""},
      {"なさいませ", POS::Verb, 0.3F, "なさる", false, false, false, CT::None, ""},
      // ござる系 (gozaru - old-fashioned/ninja speech)
      {"ござる", POS::Verb, 0.3F, "ござる", false, false, false, CT::None, ""},
      // ございます系 (modern polite form, treated as base with AUX)
      {"ございます", POS::Auxiliary, 0.3F, "ございます", false, false, false, CT::None, ""},

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
      // B63: 用い renyokei - prevents godan analysis (用く) over ichidan (用いる)
      {"用い", POS::Verb, -0.5F, "用いる", false, false, false, CT::None, "もちいる"},
      // B64: 帯びる ichidan - prevents godan analysis (帯ぶ) over ichidan (帯びる)
      {"帯びる", POS::Verb, 0.3F, "帯びる", false, false, false, CT::Ichidan, "おびる"},
      {"帯び", POS::Verb, -0.5F, "帯びる", false, false, false, CT::None, "おびる"},
      {"降りる", POS::Verb, 0.3F, "降りる", false, false, false, CT::Ichidan, "おりる"},
      {"出来る", POS::Verb, -0.5F, "出来る", false, false, false, CT::Ichidan, "できる"},
      // Lower cost to prevent split as で(PARTICLE) + きます(くる)
      {"できる", POS::Verb, -0.5F, "できる", false, false, false, CT::Ichidan, "できる"},
      // Renyokei form to prevent で(PARTICLE) + きます(くる) misparse
      // CT::None to prevent expansion (でき is already renyokei, not base form)
      {"でき", POS::Verb, -2.0F, "できる", false, false, false, CT::None, "できる"},
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
      // 知れる/しれる: ichidan verb meaning "to be possible/known"
      // Negative cost to prefer over し+れる (する+passive) split in かもしれない
      // Cost must be lower than し(-0.7) + れ(0.3) = -0.4, plus connection bonuses
      // Use CT::Ichidan for base form (will expand conjugations)
      {"知れる", POS::Verb, -1.5F, "知れる", false, false, false, CT::Ichidan, "しれる"},
      {"しれる", POS::Verb, -1.5F, "しれる", false, false, false, CT::Ichidan, "しれる"},
      // Add mizenkei/renyokei forms explicitly with CT::None (no expansion)
      {"しれ", POS::Verb, -1.5F, "しれる", false, false, false, CT::None, "しれる"},
      {"知れ", POS::Verb, -1.5F, "知れる", false, false, false, CT::None, "しれる"},
      // 疲れる: standalone ichidan verb, NOT passive of 付く
      // Without this, つかれる → つく (wrong lemma)
      {"疲れる", POS::Verb, 0.3F, "疲れる", false, false, false, CT::Ichidan, "つかれる"},
      {"つかれる", POS::Verb, 0.3F, "つかれる", false, false, false, CT::Ichidan, "つかれる"},

      // Common Ichidan verbs with i-row stems (漢字+き/ぎ/り/ち/み)
      // These get ichidan_kanji_i_row_stem penalty without dictionary registration
      {"起きる", POS::Verb, 0.3F, "起きる", false, false, false, CT::Ichidan, "おきる"},
      {"落ちる", POS::Verb, 0.3F, "落ちる", false, false, false, CT::Ichidan, "おちる"},
      {"生きる", POS::Verb, 0.3F, "生きる", false, false, false, CT::Ichidan, "いきる"},
      {"過ぎる", POS::Verb, 0.3F, "過ぎる", false, false, false, CT::Ichidan, "すぎる"},
      // Hiragana forms of grammatical auxiliaries (for MeCab-compatible splitting)
      // E.g., 食べすぎた → 食べ + すぎ + た (not 食べ + すぎた)
      {"すぎる", POS::Verb, 0.3F, "すぎる", false, false, false, CT::Ichidan, ""},
      {"すぎ", POS::Verb, 0.3F, "すぎる", false, false, false, CT::None, ""},  // renyokei
      {"はじめる", POS::Verb, 0.3F, "はじめる", false, false, false, CT::Ichidan, ""},
      {"はじめ", POS::Verb, 0.3F, "はじめる", false, false, false, CT::None, ""},  // renyokei
      {"おわる", POS::Verb, 0.3F, "おわる", false, false, false, CT::GodanRa, ""},
      {"おわり", POS::Verb, 0.3F, "おわる", false, false, false, CT::None, ""},  // renyokei
      {"つづける", POS::Verb, 0.3F, "つづける", false, false, false, CT::Ichidan, ""},
      {"つづけ", POS::Verb, 0.3F, "つづける", false, false, false, CT::None, ""},  // renyokei
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
      // B54: 放る/放つ曖昧性 - 両方登録して文脈で使い分け
      // 放る(ほうる) - "throw away, neglect" (casual, 放ってる)
      // 放つ(はなつ) - "release, fire" (formal, 放っている)
      {"放る", POS::Verb, 0.2F, "放る", false, false, false, CT::GodanRa, "ほうる"},
      {"放つ", POS::Verb, 0.3F, "放つ", false, false, false, CT::GodanTa, "はなつ"},

      // Compound verbs (敬語複合動詞)
      {"恐れ入る", POS::Verb, 0.3F, "恐れ入る", false, false, false, CT::GodanRa, "おそれいる"},
      {"申し上げる", POS::Verb, 0.3F, "申し上げる", false, false, false, CT::Ichidan, "もうしあげる"},
      {"差し上げる", POS::Verb, 0.3F, "差し上げる", false, false, false, CT::Ichidan, "さしあげる"},

      // Special Godan verbs with irregular euphonic changes
      // 行く has irregular ta/te forms (行った/行って instead of 行いた/行いて)
      // Base form uses GodanKa expansion, explicit forms use CT::None to prevent re-expansion
      // 行っ (sokuonbin form) is needed for MeCab-compatible split: 行ったら → 行っ + たら
      {"行く", POS::Verb, 0.3F, "行く", false, false, false, CT::GodanKa, "いく"},
      {"行っ", POS::Verb, -0.5F, "行く", false, false, false, CT::None, "いっ"},
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
      {"いられる", POS::Verb, 0.3F, "いる", false, false, true, CT::Ichidan, "いられる"},  // potential/passive of いる
      {"おる", POS::Verb, 0.3F, "おる", false, false, true, CT::GodanRa, "おる"},
      {"くれる", POS::Verb, 0.3F, "くれる", false, false, true, CT::Ichidan, "くれる"},
      // MeCab-compatible: くれます → くれ(VERB) + ます(AUX)
      // CT::None to prevent expansion - this is the renyokei form directly
      {"くれ", POS::Verb, -0.5F, "くれる", false, false, true, CT::None, "くれ"},
      {"あげる", POS::Verb, 0.3F, "あげる", false, false, true, CT::Ichidan, "あげる"},
      // MeCab-compatible: あげます → あげ(VERB) + ます(AUX)
      // CT::None to prevent expansion - this is the renyokei form directly
      {"あげ", POS::Verb, -0.5F, "あげる", false, false, true, CT::None, "あげ"},
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
      // Note: Common GodanSa verbs (話す, 出す, etc.) are in L2 dictionary (dictionary.tsv)
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
      // MOVED TO L2: おすすめ
      {"勘違い", POS::Noun, 0.3F, "", false, false, false, CT::None, "かんちがい"},
      // Verb stem compounds (動詞語幹複合名詞) - prevent splitting
      // MOVED TO L2: 受け取り, 問い合わせ, 申し込み, 支払い
      {"日付け", POS::Noun, -0.5F, "", false, false, false, CT::None, "ひづけ"},  // date
      {"日付", POS::Noun, -0.5F, "", false, false, false, CT::None, "ひづけ"},  // date (variant)
      {"売り上げ", POS::Noun, -0.5F, "", false, false, false, CT::None, "うりあげ"},  // sales
      {"買い上げ", POS::Noun, -0.5F, "", false, false, false, CT::None, "かいあげ"},  // purchase
      {"金儲け", POS::Noun, -0.5F, "", false, false, false, CT::None, "かねもうけ"},  // money-making

      // Degree nouns (程度名詞) - 複合語境界として重要

      // Single-kanji nouns that need dictionary entry to avoid penalty
      // 毛: common standalone noun (第一毛, 白毛), but also in compounds (ムダ毛, 産毛)
      // Cost 0.5 balances standalone use vs compound formation
      {"毛", POS::Noun, 0.5F, "", false, false, false, CT::None, "け"},

      // Iteration mark compounds (踊り字複合語) - prevent X + 々 split
      // MOVED TO L2: 人々, 日々, 国々, 色々, 時々
      {"様々", POS::Adjective, -0.5F, "", false, false, false, CT::NaAdjective, "さまざま"},

      // Hiragana nouns often incorrectly split after particles
      // MOVED TO L2: きっかけ, おかげ
      {"ところ", POS::Noun, 0.2F, "", false, false, false, CT::None, "ところ"},
      {"ため", POS::Noun, 0.2F, "為", false, true, false, CT::None, ""},
      // Hiragana nouns prone to splitting
      // おかし: moderate cost so おかしい(ADJ) wins but お+か+し loses
      {"おかし", POS::Noun, 0.3F, "お菓子", false, false, false, CT::None, ""},  // snack/sweets
      {"すもも", POS::Noun, -0.5F, "", false, false, false, CT::None, ""},  // plum
      {"もも", POS::Noun, -0.5F, "", false, false, false, CT::None, ""},  // peach
      {"しびれ", POS::Noun, -0.5F, "", false, false, false, CT::None, ""},  // numbness
      {"ひと味", POS::Noun, -0.5F, "", false, false, false, CT::None, ""},  // a touch
      // MOVED TO L2: ぬいぐるみ, 盛りだくさん
      {"でっちあげ", POS::Noun, -0.5F, "捏ち上げ", false, false, false, CT::None, ""},  // fabrication
      {"あだ名", POS::Noun, -0.5F, "渾名", false, false, false, CT::None, "あだな"},  // nickname
      {"なし崩し", POS::Noun, -0.5F, "済し崩し", false, false, false, CT::None, "なしくずし"},  // gradual
      {"何でもあり", POS::Noun, -0.5F, "", false, false, false, CT::None, "なんでもあり"},  // anything goes
      {"うち", POS::Noun, 0.3F, "内", false, false, false, CT::None, ""},  // inside/among

      // Prefix + noun compounds that should NOT be split
      // ご飯 should be single token, not ご(PREFIX) + 飯(NOUN)
      {"ご飯", POS::Noun, -0.5F, "", false, false, false, CT::None, "ごはん"},
      {"御飯", POS::Noun, -0.5F, "", false, false, false, CT::None, "ごはん"},

      // Colloquial/slang terms (俗語・ネットスラング)
      // MOVED TO L2: 推し

      // Common i-adjectives (形容詞) that are often hiragana
      {"しんどい", POS::Adjective, 0.3F, "しんどい", false, false, false, CT::IAdjective, ""},  // exhausting/tough

  };
}

}  // namespace suzume::dictionary::entries
