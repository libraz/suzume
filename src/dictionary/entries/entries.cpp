#include "core/types.h"
#include "dictionary/dictionary.h"

#include <vector>

namespace suzume::dictionary::entries {

using POS = core::PartOfSpeech;
using EPOS = core::ExtendedPOS;

// =============================================================================
// Helper functions for concise DictionaryEntry construction (v0.8 simplified)
//
// Design: No cost parameter - cost is derived from ExtendedPOS via getCategoryCost()
// =============================================================================

// Particle helper: creates PARTICLE entry
// Usage: particle("が", EPOS::ParticleCase)
inline DictionaryEntry particle(const char* s, EPOS epos) {
  return {s, POS::Particle, epos, ""};
}

// Auxiliary helper: creates AUXILIARY entry
// Usage: aux("た", "た", EPOS::AuxTenseTa)
inline DictionaryEntry aux(const char* s, const char* lemma, EPOS epos) {
  return {s, POS::Auxiliary, epos, lemma};
}

// Verb helper: creates VERB entry
// Usage: verb("し", "する", EPOS::VerbRenyokei)
inline DictionaryEntry verb(const char* s, const char* lemma, EPOS epos) {
  return {s, POS::Verb, epos, lemma};
}

// Adjective helper: creates ADJECTIVE entry
// Usage: adj("美しい", "", EPOS::AdjBasic)
inline DictionaryEntry adj(const char* s, const char* lemma, EPOS epos) {
  return {s, POS::Adjective, epos, lemma};
}

// Na-adjective helper: creates NA_ADJECTIVE entry
// Usage: na_adj("静か", "")
inline DictionaryEntry na_adj(const char* s, const char* lemma = "") {
  return {s, POS::Adjective, EPOS::AdjNaAdj, lemma};
}

// I-adjective helper: creates I_ADJECTIVE entry
// Usage: i_adj("寒い", "")
inline DictionaryEntry i_adj(const char* s, const char* lemma = "") {
  return {s, POS::Adjective, EPOS::AdjBasic, lemma};
}

// Determiner helper: creates DETERMINER entry
// Usage: det("この")
inline DictionaryEntry det(const char* s, const char* lemma = "") {
  return {s, POS::Determiner, EPOS::Determiner, lemma};
}

// Noun helper: creates NOUN entry
// Usage: noun("机"), noun("こと", "", EPOS::NounFormal)
inline DictionaryEntry noun(const char* s, const char* lemma = "",
                            EPOS epos = EPOS::Noun) {
  return {s, POS::Noun, epos, lemma};
}

// Formal noun helper: creates FORMAL_NOUN entry (形式名詞)
// Usage: formal_noun("こと")
inline DictionaryEntry formal_noun(const char* s, const char* lemma = "") {
  return {s, POS::Noun, EPOS::NounFormal, lemma};
}

// Time noun helper: creates TIME_NOUN entry (時間名詞)
// Usage: time_noun("今日")
inline DictionaryEntry time_noun(const char* s, const char* lemma = "") {
  return {s, POS::Noun, EPOS::Noun, lemma};
}

// Conjunction helper: creates CONJUNCTION entry
// Usage: conj("しかし")
inline DictionaryEntry conj(const char* s, const char* lemma = "") {
  return {s, POS::Conjunction, EPOS::Conjunction, lemma};
}

// Adverb helper: creates ADVERB entry
// Usage: adv("とても")
inline DictionaryEntry adv(const char* s, const char* lemma = "") {
  return {s, POS::Adverb, EPOS::Adverb, lemma};
}

// Suffix helper: creates SUFFIX entry
// Usage: suffix("さん")
inline DictionaryEntry suffix(const char* s, const char* lemma = "") {
  return {s, POS::Suffix, EPOS::Suffix, lemma};
}

// Pronoun helper: creates PRONOUN entry
// Usage: pronoun("私")
inline DictionaryEntry pronoun(const char* s, const char* lemma = "") {
  return {s, POS::Pronoun, EPOS::Pronoun, lemma};
}

// =============================================================================
// Particles (助詞)
// =============================================================================
std::vector<DictionaryEntry> getParticleEntries() {
  return {
      // Case particles (格助詞)
      particle("が", EPOS::ParticleCase),
      particle("を", EPOS::ParticleCase),
      particle("に", EPOS::ParticleCase),
      particle("で", EPOS::ParticleCase),  // low cost for te-form split
      particle("と", EPOS::ParticleCase),
      particle("から", EPOS::ParticleCase),
      particle("まで", EPOS::ParticleCase),
      particle("より", EPOS::ParticleCase),
      particle("へ", EPOS::ParticleCase),

      // Topic/Binding particles (係助詞)
      particle("は", EPOS::ParticleTopic),
      particle("も", EPOS::ParticleTopic),
      particle("こそ", EPOS::ParticleBinding),
      particle("さえ", EPOS::ParticleBinding),
      particle("でも", EPOS::ParticleAdverbial),
      particle("しか", EPOS::ParticleAdverbial),

      // Conjunctive particles (接続助詞)
      particle("て", EPOS::ParticleConj),  // low cost for te-form split
      particle("ば", EPOS::ParticleConj),
      particle("たら", EPOS::ParticleConj),
      particle("なら", EPOS::ParticleConj),
      // Note: ら removed - たら handles conditional, ら suffix is in L2 as SUFFIX
      particle("ながら", EPOS::ParticleConj),
      particle("のに", EPOS::ParticleConj),
      particle("ので", EPOS::ParticleConj),
      particle("けれど", EPOS::ParticleConj),
      particle("けど", EPOS::ParticleConj),
      particle("けども", EPOS::ParticleConj),
      particle("けれども", EPOS::ParticleConj),
      particle("し", EPOS::ParticleConj),  // 列挙・理由 (接続助詞)

      // Quotation particles (引用助詞)
      particle("って", EPOS::ParticleQuote),

      // Final particles (終助詞)
      particle("か", EPOS::ParticleFinal),
      particle("け", EPOS::ParticleFinal),  // colloquial variant (こんだけ → こん+だ+け)
      particle("な", EPOS::ParticleFinal),
      particle("ね", EPOS::ParticleFinal),
      particle("よ", EPOS::ParticleFinal),
      particle("わ", EPOS::ParticleFinal),
      particle("の", EPOS::ParticleNo),  // nominalizer
      {"ん", POS::Particle, EPOS::ParticleNo, "の"},  // colloquial の
      particle("じゃん", EPOS::ParticleFinal),
      particle("っけ", EPOS::ParticleFinal),
      particle("かしら", EPOS::ParticleFinal),

      // Adverbial particles (副助詞)
      particle("かも", EPOS::ParticleAdverbial),  // prevent か+も split in かもしれない
      particle("ばかり", EPOS::ParticleAdverbial),
      particle("だけ", EPOS::ParticleAdverbial),
      particle("ほど", EPOS::ParticleAdverbial),
      particle("くらい", EPOS::ParticleAdverbial),
      particle("ぐらい", EPOS::ParticleAdverbial),
      particle("など", EPOS::ParticleAdverbial),
      particle("なんて", EPOS::ParticleAdverbial),
      particle("ってば", EPOS::ParticleFinal),
      particle("ったら", EPOS::ParticleFinal),
  };
}

// =============================================================================
// Compound Particles (複合助詞)
// =============================================================================
std::vector<DictionaryEntry> getCompoundParticleEntries() {
  return {
      // Relation (関連)
      particle("について", EPOS::ParticleCase),  // beat false positive adjective candidates

      // Cause/Means (原因・手段)
      particle("によって", EPOS::ParticleConj),  // beat によっ(verb)+て split
      particle("により", EPOS::ParticleConj),
      particle("による", EPOS::ParticleConj),
      particle("によると", EPOS::ParticleConj),
      particle("によれば", EPOS::ParticleConj),

      // Place/Situation (場所・状況)
      particle("において", EPOS::ParticleConj),  // prevent に+おい(verb)+て split
      particle("にて", EPOS::ParticleCase),

      // Capacity/Viewpoint (資格・観点)
      particle("として", EPOS::ParticleConj),  // prevent と+し(VERB)+て split
      particle("にとって", EPOS::ParticleConj),
      particle("にとっても", EPOS::ParticleAdverbial),  // prevent に+とっても(ADV) misparse
      // Note: 漢字を含む複合助詞（に対して、に関して等）は分割する方針

      // Duration/Scope (範囲・期間)
      particle("にわたって", EPOS::ParticleConj),
      particle("にわたり", EPOS::ParticleConj),
      particle("にあたって", EPOS::ParticleConj),
      particle("にあたり", EPOS::ParticleConj),

      // Topic/Means (話題・手段)
      particle("をめぐって", EPOS::ParticleConj),
      particle("をめぐり", EPOS::ParticleConj),
      particle("をもって", EPOS::ParticleConj),
  };
}

// =============================================================================
// Auxiliaries (助動詞)
// =============================================================================
std::vector<DictionaryEntry> getAuxiliaryEntries() {
  return {
      // Copula/Assertion - だ (断定)
      aux("だ", "だ", EPOS::AuxCopulaDa),
      aux("だっ", "だ", EPOS::AuxCopulaDa),  // 連用タ接続形
      aux("で", "だ", EPOS::AuxCopulaDa),    // copula renyokei
      aux("だったら", "だ", EPOS::AuxCopulaDa),
      aux("な", "だ", EPOS::AuxCopulaDa),    // attributive form (連体形)

      // Copula/Assertion - です (丁寧断定)
      aux("です", "です", EPOS::AuxCopulaDesu),
      aux("でし", "です", EPOS::AuxCopulaDesu),  // renyoukei of です
      aux("でしたら", "です", EPOS::AuxCopulaDesu),
      aux("である", "である", EPOS::AuxCopulaDa),
      aux("であれば", "である", EPOS::AuxCopulaDa),

      // Polite (丁寧) - ます
      aux("ます", "ます", EPOS::AuxTenseMasu),
      aux("まし", "ます", EPOS::AuxTenseMasu),  // renyoukei
      aux("ませ", "ます", EPOS::AuxTenseMasu),  // mizenkei
      aux("ましょう", "ます", EPOS::AuxVolitional),

      // Negation - ない (否定)
      aux("ない", "ない", EPOS::AuxNegativeNai),
      aux("なかっ", "ない", EPOS::AuxNegativeNai),  // 連用タ接続
      aux("なければ", "ない", EPOS::AuxNegativeNai),

      // Negation - ぬ/ず (文語否定)
      aux("ぬ", "ぬ", EPOS::AuxNegativeNu),
      aux("ず", "ず", EPOS::AuxNegativeNu),
      aux("ずに", "ず", EPOS::AuxNegativeNu),
      aux("ずとも", "ず", EPOS::AuxNegativeNu),
      aux("ごとく", "ごとく", EPOS::Unknown),
      aux("ごとき", "ごとき", EPOS::Unknown),
      aux("じゃない", "ではない", EPOS::AuxNegativeNai),
      aux("ん", "ん", EPOS::AuxNegativeNu),

      // Past/Completion - た (過去・完了)
      aux("た", "た", EPOS::AuxTenseTa),
      aux("たら", "た", EPOS::AuxTenseTa),  // 仮定形
      aux("だら", "だ", EPOS::AuxTenseTa),

      // Conjecture/Volitional (推量・意志) - う/よう
      aux("う", "う", EPOS::AuxVolitional),
      aux("よう", "よう", EPOS::AuxVolitional),
      aux("だろう", "だろう", EPOS::AuxVolitional),
      aux("でしょう", "でしょう", EPOS::AuxVolitional),

      // Negative conjecture (否定推量)
      aux("まい", "まい", EPOS::AuxNegativeNu),

      // Conjecture - らしい (推定)
      aux("らしい", "らしい", EPOS::AuxConjectureRashii),
      aux("らしく", "らしい", EPOS::AuxConjectureRashii),
      aux("らしかっ", "らしい", EPOS::AuxConjectureRashii),

      // Conjecture - みたい (様態推定)
      // Note: みたいだ/みたいに removed - MeCab splits as みたい+だ/に
      aux("みたい", "みたい", EPOS::AuxConjectureMitai),

      // Appearance - そう (様態)
      // Note: そうだ/そうに removed - MeCab splits as そう+だ/に
      aux("そう", "そう", EPOS::AuxAppearanceSou),
      // Note: さそう removed - MeCab splits as な+さ+そう (3 tokens)

      // Obligation (当為)
      // Classical obligation auxiliary べし - connects after verb shuushikei
      // Note: べきだ/べきで/べきでは removed - MeCab splits as べき+だ/で/では
      aux("べき", "べし", EPOS::AuxVolitional),  // 連体形: 食べるべき

      // Passive/Potential (受身・可能)
      aux("れ", "れる", EPOS::AuxPassive),
      aux("れる", "れる", EPOS::AuxPassive),
      aux("られ", "られる", EPOS::AuxPassive),
      aux("られる", "られる", EPOS::AuxPassive),

      // Suru verb stem forms (サ変動詞語幹活用形) - VERB, not AUX
      verb("し", "する", EPOS::VerbRenyokei),
      verb("す", "する", EPOS::VerbShuushikei),
      verb("さ", "する", EPOS::VerbMizenkei),

      // Kuru verb stem form (カ変動詞語幹活用形) - VERB, not AUX
      // MeCab: 来た → 来(連用形) + た(過去)
      verb("来", "来る", EPOS::VerbRenyokei),
      aux("しよう", "する", EPOS::AuxVolitional),
      aux("しろ", "する", EPOS::VerbMeireikei),
      aux("せよ", "する", EPOS::VerbMeireikei),
      aux("すれば", "する", EPOS::VerbKateikei),
      aux("しそう", "する", EPOS::AuxAppearanceSou),

      // Causative (使役)
      aux("せ", "せる", EPOS::AuxCausative),
      aux("せる", "せる", EPOS::AuxCausative),
      aux("させ", "させる", EPOS::AuxCausative),
      aux("させる", "させる", EPOS::AuxCausative),

      // Desiderative - たい (願望)
      aux("たい", "たい", EPOS::AuxDesireTai),
      aux("たく", "たい", EPOS::AuxDesireTai),
      aux("たかっ", "たい", EPOS::AuxDesireTai),
      adj("たければ", "たい", EPOS::AuxDesireTai),
      aux("たがる", "たがる", EPOS::AuxDesireTai),

      // Irregular i-adjective よい/いい (形容詞・アウオ段)
      // MeCab: よければ → よけれ(仮定形) + ば, よかった → よかっ(連用タ接続) + た
      adj("よけれ", "よい", EPOS::AdjKeForm),
      adj("よかっ", "よい", EPOS::AdjKatt),
      adj("よく", "よい", EPOS::AdjRenyokei),

      // Irregular i-adjective ない (形容詞・アウオ段)
      // MeCab: なさそう → な(語幹/ガル接続) + さ(名詞化接尾辞) + そう(様態)
      adj("な", "ない", EPOS::AdjStem),

      // Nominalization suffix さ (高さ, 美しさ, なさ)
      // MeCab: 高さ → 高(語幹) + さ(名詞), なさそう → な + さ + そう
      suffix("さ", "さ"),

      // Na-adjective forming suffix 的 (論理的, 科学的, 経済的)
      // MeCab: 論理的な → 論理 + 的 + な (suffix + copula rentaikei)
      suffix("的", "的"),

      // Polite imperative - connect after verb renyokei
      aux("なさい", "なさる", EPOS::AuxHonorific),

      // Possibility/Uncertainty - split form for MeCab compatibility
      // かもしれない → かも + しれ + ない (not かもしれない as single token)
      // かも particle is already defined above (line 157)

      // Certainty - split form for MeCab compatibility
      // 間違いない → 間違い + ない (not 間違いない as single token)
      // These are handled by noun + ない connection

      // Potential/Passive/Causative (generic)
      aux("れる", "れる", EPOS::AuxPassive),
      aux("られる", "られる", EPOS::AuxPassive),
      aux("せる", "せる", EPOS::AuxCausative),
      aux("させる", "させる", EPOS::AuxCausative),

      // Polite existence - ございます (丁重)
      // MeCab splits: ござい + ます (renyokei + polite)
      aux("ござい", "ござる", EPOS::AuxGozaru),

      // Request
      aux("ください", "ください", EPOS::Unknown),
      aux("くださいませ", "ください", EPOS::Unknown),

      // Progressive/Continuous - いる (進行・継続)
      aux("いる", "いる", EPOS::AuxAspectIru),
      aux("います", "いる", EPOS::AuxAspectIru),
      aux("いません", "いる", EPOS::AuxAspectIru),
      aux("い", "いる", EPOS::AuxAspectIru),
      aux("いない", "いる", EPOS::AuxAspectIru),
      aux("いなかった", "いる", EPOS::AuxAspectIru),
      aux("いれば", "いる", EPOS::AuxAspectIru),

      // Completive/Regretful - しまう (完了・遺憾)
      // MeCab treats しまう as a regular verb, not auxiliary
      // Complete forms removed - let inflection system handle conjugations
      // This allows しまった → しまっ + た splitting (MeCab compatible)

      // Contracted forms: ちゃう/じゃう (completion)
      verb("ちゃう", "ちゃう", EPOS::AuxAspectShimau),
      verb("ちゃっ", "ちゃう", EPOS::AuxAspectShimau),
      verb("ちゃい", "ちゃう", EPOS::AuxAspectShimau),
      verb("じゃう", "じゃう", EPOS::AuxAspectShimau),
      verb("じゃっ", "じゃう", EPOS::AuxAspectShimau),
      verb("じゃい", "じゃう", EPOS::AuxAspectShimau),

      // Contracted forms: てる/とく (progressive/preparation)
      verb("てる", "てる", EPOS::AuxAspectIru),
      verb("て", "てる", EPOS::AuxAspectIru),
      // Note: で/でる removed from AuxAspectIru - they conflict with 出る renyokei
      // 「出たい」should be で(出る連用形)+たい, not で(補助動詞)+たい
      verb("とく", "とく", EPOS::AuxAspectOku),
      verb("どく", "どく", EPOS::AuxAspectOku),
      // MeCab compat: とい/どい (renyokei) + た/て instead of といた/どいた
      verb("とい", "とく", EPOS::AuxAspectOku),
      verb("どい", "どく", EPOS::AuxAspectOku),

      // Directional auxiliaries - いく/くる (方向補助動詞)
      aux("いく", "いく", EPOS::AuxAspectIku),
      aux("いった", "いく", EPOS::AuxAspectIku),
      aux("いって", "いく", EPOS::AuxAspectIku),
      verb("いか", "いく", EPOS::VerbShuushikei),
      aux("いかない", "いく", EPOS::AuxAspectIku),
      aux("くる", "くる", EPOS::AuxAspectKuru),
      aux("きます", "くる", EPOS::AuxAspectKuru),
      aux("きた", "くる", EPOS::AuxAspectKuru),
      aux("こない", "くる", EPOS::AuxAspectKuru),

      // Explanatory (説明) - MeCab compat: split as の/ん + だ/です/でした
      // Removed のだ/のです/のでした/んだ/んです/んでした to allow split

      // Kuruwa-kotoba (廓言葉)
      aux("ありんす", "ある", EPOS::Unknown), aux("ありんした", "ある", EPOS::Unknown), aux("ありんせん", "ある", EPOS::Unknown),
      aux("ざんす", "ある", EPOS::Unknown), aux("ざんせん", "ある", EPOS::Unknown),
      aux("でありんす", "だ", EPOS::Unknown), aux("でありんした", "だ", EPOS::Unknown),
      aux("なんし", "ます", EPOS::Unknown), aux("なんした", "ます", EPOS::Unknown),

      // Cat-like (猫系)
      aux("にゃ", "よ", EPOS::Unknown), aux("にゃん", "よ", EPOS::Unknown), aux("にゃー", "よ", EPOS::Unknown),
      aux("だにゃ", "だよ", EPOS::Unknown), aux("だにゃん", "だよ", EPOS::Unknown),
      aux("ですにゃ", "ですよ", EPOS::Unknown), aux("ですにゃん", "ですよ", EPOS::Unknown),

      // Squid character (イカ娘)
      aux("ゲソ", "だ", EPOS::Unknown), aux("げそ", "だ", EPOS::Unknown),
      aux("でゲソ", "だ", EPOS::Unknown), aux("でげそ", "だ", EPOS::Unknown),

      // Ojou-sama/Lady speech (お嬢様言葉)
      aux("ですわ", "です", EPOS::Unknown), aux("ですの", "です", EPOS::Unknown),
      aux("ますの", "ます", EPOS::Unknown), aux("だわ", "だ", EPOS::Unknown),

      // Youth slang (若者言葉)
      aux("っす", "です", EPOS::Unknown), aux("っした", "でした", EPOS::Unknown), aux("っすか", "ですか", EPOS::Unknown),

      // Rabbit-like (兎系)
      aux("ぴょん", "だ", EPOS::Unknown), aux("ピョン", "だ", EPOS::Unknown),

      // Ninja/Old-fashioned (忍者・古風)
      aux("ござる", "だ", EPOS::Unknown), aux("でござる", "だ", EPOS::Unknown),
      aux("ござった", "だった", EPOS::Unknown), aux("でござった", "だった", EPOS::Unknown),
      aux("ござらぬ", "ではない", EPOS::Unknown), aux("ござらん", "ではない", EPOS::Unknown),
      aux("でございます", "です", EPOS::Unknown),
      aux("ナリ", "だ", EPOS::Unknown), aux("なり", "だ", EPOS::Unknown),
      aux("でナリ", "だ", EPOS::Unknown), aux("でなり", "だ", EPOS::Unknown),

      // Elderly/Archaic (老人・古風)
      aux("じゃ", "だ", EPOS::Unknown), aux("じゃな", "だ", EPOS::Unknown),
      aux("のじゃ", "のだ", EPOS::Unknown), aux("じゃろう", "だろう", EPOS::Unknown),

      // Regional dialects (方言系)
      aux("ぜよ", "だ", EPOS::Unknown), aux("だべ", "だ", EPOS::Unknown), aux("やんけ", "だ", EPOS::Unknown),
      aux("やで", "だ", EPOS::Unknown), aux("やねん", "だ", EPOS::Unknown),
      aux("だっちゃ", "だ", EPOS::Unknown), aux("ばい", "だ", EPOS::Unknown),

      // Robot/Mechanical (ロボット・機械)
      aux("デス", "です", EPOS::Unknown),
      aux("マス", "ます", EPOS::Unknown),
  };
}

// =============================================================================
// Conjunctions (接続詞)
// =============================================================================
std::vector<DictionaryEntry> getConjunctionEntries() {
  return {
      // Sequential (順接)
      conj("従って", ""), conj("故に", ""),
      conj("そして", ""), conj("それから", ""), conj("すると", ""),
      conj("だから", ""), conj("そのため", ""),
      conj("したがって", "従って"),

      // Adversative (逆接)
      conj("しかし", ""), conj("だが", ""), conj("けれども", ""),
      conj("ところが", ""), conj("それでも", ""),
      conj("でも", ""), conj("だって", ""), conj("にもかかわらず", ""),
      conj("ものの", ""),

      // Parallel/Addition (並列・添加)
      conj("又", ""), conj("及び", ""),
      conj("並びに", ""), conj("且つ", ""),
      conj("更に", ""),
      conj("しかも", ""), conj("そのうえ", ""),

      // Alternative (選択)
      conj("或いは", ""), conj("若しくは", ""),
      conj("または", ""), conj("それとも", ""),
      conj("あるいは", "或いは"),

      // Explanation/Supplement (説明・補足)
      conj("即ち", ""), conj("例えば", ""),
      conj("但し", ""), conj("尚", ""),
      conj("つまり", ""), conj("たとえば", ""), conj("なぜなら", ""),
      conj("ちなみに", ""), conj("まして", ""),

      // Topic change (転換)
      conj("さて", ""), conj("ところで", ""),
      // Note: では removed to allow で+は splitting in ではない patterns
      // MeCab splits 彼女ではない as 彼女+で+は+ない, not 彼女+では+ない
      conj("それでは", ""),

      // Addition/Emphasis
      conj("のみならず", ""),

      // Additional conjunctions
      conj("いわば", "言わば"), conj("言わば", ""),
      conj("さもないと", ""), conj("さもなければ", ""),
      conj("そんなら", "其んなら"),
      conj("それにしても", ""),
      adv("ともかく", ""),
      conj("いずれにしても", ""), conj("いずれにせよ", ""),
  };
}

// =============================================================================
// Determiners (連体詞)
// =============================================================================
std::vector<DictionaryEntry> getDeterminerEntries() {
  return {
      // Demonstrative determiners (指示連体詞) - この/その/あの/どの
      det("この", ""), det("その", ""), det("あの", ""), det("どの", ""),
      // Demonstrative determiners (指示連体詞) - こんな/そんな/あんな/どんな
      det("こんな", ""), det("そんな", ""), det("あんな", ""), det("どんな", ""),

      // Other determiners (連体詞)
      det("ある", ""), det("あらゆる", ""), det("いわゆる", ""), det("おかしな", ""),
      det("同じ", ""),  // same - prevent VERB confusion

      // Demonstrative manner determiners (指示様態連体詞)
      // Lower cost to compete with X + いう (VERB cost 0.3) splits
      det("こういう", ""), det("そういう", ""),
      det("ああいう", ""), det("どういう", ""),

      // Quotative determiners (引用連体詞) - prevents incorrect split like 病+とい+う
      // Lower cost to beat と(PARTICLE,-0.4)+いった(VERB,-0.034)+conn(0.2)=-0.232
      det("という", ""),
      det("といった", ""),
      det("っていう", ""),  // colloquial

      // Quotative verb forms (引用動詞活用形) - to + 言う conjugations
      // These compete with と(PARTICLE) + いって(行く, cost 1.2F) paths
      verb("といって", "いう", EPOS::VerbShuushikei),
      verb("といっては", "いう", EPOS::VerbShuushikei),
      conj("といっても", "といっても"),
      verb("そういって", "いう", EPOS::VerbShuushikei),
      verb("こういって", "いう", EPOS::VerbShuushikei),
      verb("ああいって", "いう", EPOS::VerbShuushikei),
      verb("どういって", "いう", EPOS::VerbShuushikei),

      // Determiners with kanji - B51: lowered cost to prioritize over NOUN unknown
      det("大きな", ""), det("小さな", ""),
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
      pronoun("私", ""),
      pronoun("僕", ""),
      pronoun("俺", ""),
      // First person - hiragana only
      pronoun("わたくし", ""),

      // First person plural (一人称複数)
      // Lower cost to beat single pronoun + suffix split
      pronoun("私たち", ""),
      pronoun("僕たち", ""),
      pronoun("僕ら", ""),
      pronoun("俺たち", ""),
      // Note: 俺ら removed for MeCab compat (MeCab splits 俺+ら, unlike 僕ら)
      pronoun("我々", ""),

      // Second person (二人称) - kanji with reading
      pronoun("貴方", ""),
      pronoun("君", ""),
      // Second person - hiragana/mixed only
      pronoun("あなた", ""),
      // B39: お前 needs low cost to beat PREFIX(お)+NOUN(前) split (connection bonus -1.5)
      // PREFIX→NOUN path has cost ~-1.2, so お前 needs cost < -1.2 to win
      pronoun("お前", ""),

      // Second person plural (二人称複数)
      pronoun("あなたたち", ""),
      pronoun("君たち", ""),

      // Third person (三人称) - kanji with reading
      pronoun("彼", ""),
      pronoun("彼女", ""),
      pronoun("彼ら", ""),
      pronoun("彼女ら", ""),
      pronoun("彼女たち", ""),

      // Archaic/Samurai (武家・古風)
      pronoun("拙者", ""),
      pronoun("貴殿", ""),
      pronoun("某", ""),
      pronoun("我輩", ""),
      pronoun("吾輩", ""),

      // Collective pronouns (集合代名詞)
      pronoun("皆", ""),
      pronoun("皆さん", ""),
      pronoun("みんな", ""),

      // Demonstrative - proximal (近称)
      pronoun("これ", ""), pronoun("ここ", ""), pronoun("こちら", ""),
      // Colloquial demonstratives - prevent っち split
      pronoun("こっち", ""), pronoun("そっち", ""),
      pronoun("あっち", ""), pronoun("どっち", ""),

      // Demonstrative - medial (中称)
      pronoun("それ", ""), pronoun("そこ", ""), pronoun("そちら", ""),

      // Demonstrative - distal (遠称)
      pronoun("あれ", ""),
      pronoun("あそこ", ""), pronoun("あちら", ""),

      // Demonstrative - person reference (こそあど+いつ)
      pronoun("こいつ", ""), pronoun("そいつ", ""),
      pronoun("あいつ", ""), pronoun("どいつ", ""),

      // Demonstrative - interrogative (不定称)
      pronoun("どれ", ""), pronoun("どこ", ""), pronoun("どちら", ""),

      // Indefinite (不定代名詞) - kanji with reading
      // Low cost to act as strong anchors against prefix compounds (今何 → 今+何)
      // Cost -1.5 to beat: 今(1.4) + 何(-1.9) + conn(0.5) = 0.0 < 今何(0.5)
      pronoun("誰", ""),
      pronoun("何", ""),

      // Interrogatives (疑問詞)
      pronoun("いつ", ""), pronoun("いくつ", ""), pronoun("いくら", ""),
      adv("どう", ""),
      // Note: どうして needs very low cost to prevent split when followed by verb
      // The te-form bonus makes どう+して+VERB cheaper than どうして+VERB
      adv("どうして", ""),
      adv("なぜ", ""), adv("どんな", ""),
  };
}

// =============================================================================
// Formal Nouns (形式名詞)
// =============================================================================
std::vector<DictionaryEntry> getFormalNounEntries() {
  return {
      // Formal nouns (形式名詞) - hiragana form is canonical in modern Japanese
      // こと/もの are grammatical function words, hiragana is preferred
      formal_noun("事", "こと"),
      formal_noun("こと", ""),
      formal_noun("物", "もの"),
      formal_noun("もの", ""),
      formal_noun("為", ""),
      formal_noun("所", ""),
      formal_noun("ところ", ""),
      formal_noun("時", ""),
      formal_noun("内", ""),
      formal_noun("通り", ""),
      formal_noun("限り", ""),
      // Suffix-like formal nouns
      // Lower cost for 付け to compete with verb_kanji ichidan pattern
      formal_noun("付け", ""),
      formal_noun("付", ""),
      // Hiragana-only forms
      formal_noun("よう", ""),
      formal_noun("ほう", ""),  // B49: lowered cost
      formal_noun("わけ", ""),
      formal_noun("はず", "はず"),
      formal_noun("つもり", ""),
      formal_noun("まま", ""),
      formal_noun("ほか", "ほか"),
      formal_noun("他", "ほか"),
      // Abstract nouns that don't form suru-verbs
      formal_noun("仕方", ""),
      formal_noun("ありきたり", "ありきたり"),  // Low cost to prevent あり+き+たり split, na-adjective stem
      formal_noun("たたずまい", "たたずまい"),  // noun, not suru-verb
      // NOTE: 〜がち forms are now handled by generateGachiSuffixCandidates() in suffix_candidates.cpp
      // B35: Idiom component (eaves bracket - used in うだつが上がらない)
      formal_noun("うだつ", "うだつ"),
  };
}


}  // namespace suzume::dictionary::entries
