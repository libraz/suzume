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

// Prefix helper: creates PREFIX entry
// Usage: prefix("お"), prefix("ご")
inline DictionaryEntry prefix(const char* s, const char* lemma = "") {
  return {s, POS::Prefix, EPOS::Prefix, lemma};
}

// Pronoun helper: creates PRONOUN entry
// Usage: pronoun("私")
inline DictionaryEntry pronoun(const char* s, const char* lemma = "") {
  return {s, POS::Pronoun, EPOS::Pronoun, lemma};
}

// Interjection helper: creates INTERJECTION entry
// Usage: intj("えっ")
inline DictionaryEntry intj(const char* s, const char* lemma = "") {
  return {s, POS::Interjection, EPOS::Interjection, lemma};
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
      particle("で", EPOS::ParticleConj),  // te-form for hatsuonbin verbs (読んで, 飛んで)
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
      particle("ぞ", EPOS::ParticleFinal),
      particle("ぜ", EPOS::ParticleFinal),
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
      // Note: による removed - grammatically に+よる (格助詞+動詞連体形)
      // Note: によると removed - MeCab splits as に+よる+と (引用表現)
      // Note: によれば removed - grammatically に+よれ+ば
      // These compound particles are better split for grammatical accuracy

      // Place/Situation (場所・状況)
      particle("において", EPOS::ParticleConj),  // prevent に+おい(verb)+て split
      particle("にて", EPOS::ParticleCase),

      // Capacity/Viewpoint (資格・観点)
      particle("として", EPOS::ParticleConj),  // prevent と+し(VERB)+て split
      particle("にとって", EPOS::ParticleConj),
      particle("にとっても", EPOS::ParticleAdverbial),  // prevent に+とっても(ADV) misparse
      particle("に関して", EPOS::ParticleConj),  // MeCab compatible
      particle("に際して", EPOS::ParticleConj),  // MeCab compatible
      particle("に対して", EPOS::ParticleConj),

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
      // で+ある pattern - ある is a separate auxiliary (MeCab compatible)
      aux("ある", "ある", EPOS::AuxCopulaDa),   // で+ある (assertion)
      aux("あっ", "ある", EPOS::AuxCopulaDa),   // で+あっ+た (sokuonbin before た)
      aux("あり", "ある", EPOS::AuxCopulaDa),   // で+あり+ます
      aux("あれ", "ある", EPOS::AuxCopulaDa),   // で+あれ+ば (conditional)

      // Polite (丁寧) - ます
      aux("ます", "ます", EPOS::AuxTenseMasu),
      aux("まし", "ます", EPOS::AuxTenseMasu),  // renyoukei
      aux("ませ", "ます", EPOS::AuxTenseMasu),  // mizenkei
      aux("ましょ", "ます", EPOS::AuxTenseMasu),  // mizenkei, connects to う

      // Negation - ない (否定)
      aux("ない", "ない", EPOS::AuxNegativeNai),
      aux("なかっ", "ない", EPOS::AuxNegativeNai),  // 連用タ接続
      aux("なけれ", "ない", EPOS::AuxNegativeNai),  // 仮定形 (なけれ+ば)

      // Negation - ぬ/ず (文語否定)
      aux("ぬ", "ぬ", EPOS::AuxNegativeNu),
      aux("ず", "ぬ", EPOS::AuxNegativeNu),   // lemma is ぬ per MeCab
      aux("ずに", "ぬ", EPOS::AuxNegativeNu),
      aux("ずとも", "ぬ", EPOS::AuxNegativeNu),
      aux("ごとく", "ごとく", EPOS::Unknown),
      aux("ごとき", "ごとき", EPOS::Unknown),
      aux("じゃない", "ではない", EPOS::AuxNegativeNai),
      aux("ん", "ん", EPOS::AuxNegativeNu),

      // Past/Completion - た (過去・完了)
      aux("た", "た", EPOS::AuxTenseTa),
      aux("たら", "た", EPOS::AuxTenseTa),  // 仮定形
      aux("だ", "だ", EPOS::AuxTenseTa),    // 連濁形 (泳いだ, 死んだ, 飛んだ, 読んだ)
      aux("だら", "だ", EPOS::AuxTenseTa),

      // Conjecture/Volitional (推量・意志) - う/よう
      aux("う", "う", EPOS::AuxVolitional),
      aux("よう", "よう", EPOS::AuxVolitional),
      aux("だろ", "だ", EPOS::AuxCopulaDa),  // mizenkei, connects to う
      aux("でしょ", "です", EPOS::AuxCopulaDesu),  // mizenkei, connects to う

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

      // Demonstrative そう (指示詞/副詞的用法) - sentence-initial そうですね, etc.
      // MeCab treats "そうですね" as フィラー, but normalizes to そう(形容動詞語幹)+です+ね
      // This competes with AuxAppearanceSou; bigram rules select based on context
      na_adj("そう", "そう"),
      // MeCab: そうかもしれない → そう(副詞,助詞類接続) + かも + しれ...
      // When followed by particles (not だ/な), MeCab treats そう as adverb
      adv("そう"),
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

      // Deru verb stem form (一段動詞「出る」) - VERB
      // で+たい/ます needs this to split correctly (外にでたい → 外|に|で|たい)
      verb("で", "出る", EPOS::VerbRenyokei),
      // Suru verb conjugation forms - split for MeCab compatibility
      // MeCab: 勉強しよう → 勉強|しよ|う, 勉強すれば → 勉強|すれ|ば
      verb("しよ", "する", EPOS::VerbMizenkei),  // volitional base: しよ+う
      verb("すれ", "する", EPOS::VerbKateikei),  // conditional base: すれ+ば
      aux("しろ", "する", EPOS::VerbMeireikei),
      aux("せよ", "する", EPOS::VerbMeireikei),
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
      // いい is colloquial form of よい, shares conjugated forms (よかった, よければ, etc.)
      adj("いい", "いい", EPOS::AdjBasic),  // いい天気, いいです
      adj("よい", "よい", EPOS::AdjBasic),  // よい天気, よいです
      adj("よけれ", "よい", EPOS::AdjKeForm),
      adj("よかっ", "よい", EPOS::AdjKatt),
      adj("よく", "よい", EPOS::AdjRenyokei),
      adj("よ", "よい", EPOS::AdjStem),  // MeCab: よさ → よ(語幹/ガル接続) + さ(接尾辞)

      // Irregular i-adjective ない (形容詞・アウオ段)
      // MeCab: なさそう → な(語幹/ガル接続) + さ(名詞化接尾辞) + そう(様態)
      // 金がない → 金 + が + ない (existential negative adjective)
      // vs 食べない → 食べ + ない (negation auxiliary)
      adj("ない", "ない", EPOS::AdjBasic),
      adj("なく", "ない", EPOS::AdjRenyokei),
      adj("なかっ", "ない", EPOS::AdjKatt),
      adj("な", "ない", EPOS::AdjStem),

      // Kanji form of ない (無い) - used in formal writing
      // MeCab: 休むこと無く → 休む + こと + 無く (形容詞連用形)
      adj("無", "無い", EPOS::AdjStem),
      adj("無く", "無い", EPOS::AdjRenyokei),

      // Honorific prefix お (お待ち, お世話, お嬢様)
      // MeCab: お待ち → お(接頭辞) + 待ち(名詞)
      prefix("お", "お"),

      // Honorific prefix ご (ご確認, ご報告, ご連絡)
      // MeCab: ご確認 → ご(接頭辞) + 確認(名詞)
      prefix("ご", "ご"),

      // Note: Negation prefixes (未, 非, 不, 無) are NOT registered
      // MeCab splits them but Suzume keeps them unified for practical tokenization
      // e.g., 未確認 → 未確認 (not 未+確認)

      // Nominalization suffix さ (高さ, 美しさ, なさ)
      // MeCab: 高さ → 高(語幹) + さ(名詞), なさそう → な + さ + そう
      suffix("さ", "さ"),

      // Honorific suffix さん (田中さん, お姉さん)
      // MeCab: 田中さん → 田中 + さん
      suffix("さん", "さん"),

      // Plural suffix たち (学生たち, 私たち, 子供たち)
      // MeCab: 学生たち → 学生 + たち
      suffix("たち", "たち"),

      // Plural suffix ら (彼ら, 彼女ら, 僕ら, あいつら)
      // MeCab treats these as single tokens, but grammatically ら is a suffix
      suffix("ら", "ら"),

      // Na-adjective forming suffix 的 (論理的, 科学的, 経済的)
      // MeCab: 論理的な → 論理 + 的 + な (suffix + copula rentaikei)
      suffix("的", "的"),

      // NOTE: 中 suffix removed - MeCab treats 世界中/一日中 as single NOUN

      // Inclusive suffix ごと (皮ごと, 頭ごと)
      // MeCab: 皮ごと → 皮 + ごと (noun + suffix)
      suffix("ごと", "ごと"),

      // Adjective suffixes - connect after verb renyokei (V連用形接続)
      // MeCab: 使いにくい → 使い + にくい, 読みやすい → 読み + やすい
      adj("にくい", "にくい", EPOS::AdjBasic),
      adj("にくく", "にくい", EPOS::AdjRenyokei),
      adj("にくかっ", "にくい", EPOS::AdjKatt),
      adj("やすい", "やすい", EPOS::AdjBasic),
      adj("やすく", "やすい", EPOS::AdjRenyokei),
      adj("やすかっ", "やすい", EPOS::AdjKatt),

      // Adjective suffix っぽい (～っぽい: 子供っぽい, 忘れっぽい)
      // MeCab: 子供っぽい → 子供 + っぽい
      adj("っぽい", "っぽい", EPOS::AdjBasic),
      adj("っぽく", "っぽい", EPOS::AdjRenyokei),
      adj("っぽかっ", "っぽい", EPOS::AdjKatt),

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
      // ござっ is onbinkei (促音便形) for ござった
      // MeCab splits: ござっ + た (onbinkei + ta)
      verb("ござっ", "ござる", EPOS::VerbOnbinkei),

      // Humble verb - いたす (謙譲語)
      // MeCab treats いたし as 動詞,非自立 (dependent verb)
      // Used in: お願いいたします, ご連絡いたします
      verb("いたし", "いたす", EPOS::VerbRenyokei),

      // Receiving verb - いただく (謙譲語)
      // Used in: いただきます, 食べていただく
      // Must be registered to prevent い+た+だき split
      verb("いただき", "いただく", EPOS::VerbRenyokei),

      // Request - ください is VERB (くださる) in MeCab
      // くださる is special ra-row godan with irregular imperative form ください
      verb("ください", "くださる", EPOS::VerbShuushikei),
      verb("下さい", "下さる", EPOS::VerbShuushikei),
      verb("くださいませ", "くださる", EPOS::VerbShuushikei),

      // Special ra-row godan verbs (五段ラ行特殊) with い-form renyokei
      // These honorific/humble verbs use い instead of り for renyokei:
      // いらっしゃる → いらっしゃい+ます (not いらっしゃり)
      // ござる → ござい+ます (not ござり)
      // なさる → なさい+ます (not なさり)
      // おっしゃる → おっしゃい+ます (not おっしゃり)
      verb("いらっしゃい", "いらっしゃる", EPOS::VerbRenyokei),
      verb("ござい", "ござる", EPOS::VerbRenyokei),
      verb("なさい", "なさる", EPOS::VerbRenyokei),
      verb("おっしゃい", "おっしゃる", EPOS::VerbRenyokei),

      // Progressive/Continuous - いる (進行・継続)
      // MeCab splits て+い+た, て+い+て (not て+いた/て+いて)
      // い is registered separately for MeCab-compatible splits
      aux("い", "いる", EPOS::AuxAspectIru),  // renyokei for い+た, い+ます
      aux("いる", "いる", EPOS::AuxAspectIru),
      aux("います", "いる", EPOS::AuxAspectIru),
      aux("いません", "いる", EPOS::AuxAspectIru),
      aux("いない", "いる", EPOS::AuxAspectIru),
      aux("いなかった", "いる", EPOS::AuxAspectIru),
      aux("いれば", "いる", EPOS::AuxAspectIru),

      // Progressive/Continuous - おる (humble/dialectal form of いる)
      // Used in formal polite speech: ております, おります
      // Note: Only add renyokei forms, not combined forms like おります
      // to allow MeCab-compatible split: おり+ます
      aux("おる", "おる", EPOS::AuxAspectIru),
      aux("おり", "おる", EPOS::AuxAspectIru),  // renyokei for おり+ます

      // Benefactive auxiliary - くれる (giving, receiving benefit)
      // Used in subsidiary verb patterns: してくれる, 買ってくれた
      // Note: MeCab treats くれる as 動詞,非自立 (dependent verb)
      aux("くれる", "くれる", EPOS::AuxAspectKuru),
      aux("くれ", "くれる", EPOS::AuxAspectKuru),  // renyokei for くれ+ます

      // Excessive degree subsidiary verb - すぎる (過度)
      // Used after adjective/verb stems: 高すぎる, 食べすぎる
      // MeCab: 動詞,非自立 (subsidiary verb, not auxiliary 助動詞)
      // MeCab splits: 高 + すぎる (終止形), 高 + すぎ + た (連用形 + た)
      // Use verb() to get POS::Verb, but keep AuxExcessive EPOS for bigram rules
      verb("すぎる", "すぎる", EPOS::AuxExcessive),
      verb("すぎ", "すぎる", EPOS::AuxExcessive),  // renyokei for すぎ+た, すぎ+て

      // Completive/Regretful - しまう (完了・遺憾)
      // MeCab treats しまう as 動詞,非自立 (non-independent verb) → maps to Auxiliary
      // Use aux() to get POS::Auxiliary for MeCab compatibility
      aux("しまう", "しまう", EPOS::AuxAspectShimau),
      aux("しまっ", "しまう", EPOS::AuxAspectShimau),  // te-form/ta-form stem
      aux("しまい", "しまう", EPOS::AuxAspectShimau),  // negative stem

      // Contracted forms: ちゃう/じゃう (completion)
      verb("ちゃう", "ちゃう", EPOS::AuxAspectShimau),
      verb("ちゃっ", "ちゃう", EPOS::AuxAspectShimau),
      verb("ちゃい", "ちゃう", EPOS::AuxAspectShimau),
      verb("じゃう", "じゃう", EPOS::AuxAspectShimau),
      verb("じゃっ", "じゃう", EPOS::AuxAspectShimau),
      verb("じゃい", "じゃう", EPOS::AuxAspectShimau),

      // Contracted forms: てる/とく (progressive/preparation)
      // MeCab: 動詞,非自立 → Auxiliary (subsidiary verbs)
      aux("てる", "てる", EPOS::AuxAspectIru),
      aux("て", "てる", EPOS::AuxAspectIru),
      // Note: で/でる removed from AuxAspectIru - they conflict with 出る renyokei
      // 「出たい」should be で(出る連用形)+たい, not で(補助動詞)+たい
      aux("とく", "とく", EPOS::AuxAspectOku),
      aux("どく", "どく", EPOS::AuxAspectOku),
      // MeCab compat: とい/どい (renyokei) + た/て instead of といた/どいた
      aux("とい", "とく", EPOS::AuxAspectOku),
      aux("どい", "どく", EPOS::AuxAspectOku),

      // Directional auxiliaries - いく/くる (方向補助動詞)
      aux("いく", "いく", EPOS::AuxAspectIku),
      aux("いった", "いく", EPOS::AuxAspectIku),
      aux("いって", "いく", EPOS::AuxAspectIku),
      verb("いか", "いく", EPOS::VerbShuushikei),
      aux("いかない", "いく", EPOS::AuxAspectIku),
      aux("くる", "くる", EPOS::AuxAspectKuru),
      // MeCab compat: split き+た/て/ます separately
      aux("き", "くる", EPOS::AuxAspectKuru),
      aux("こない", "くる", EPOS::AuxAspectKuru),

      // Explanatory (説明) - MeCab compat: split as の/ん + だ/です/でした
      // Removed のだ/のです/のでした/んだ/んです/んでした to allow split

      // Kuruwa-kotoba (廓言葉)
      aux("ありんす", "ある", EPOS::Unknown), aux("ありんした", "ある", EPOS::Unknown), aux("ありんせん", "ある", EPOS::Unknown),
      aux("ざんす", "ある", EPOS::Unknown), aux("ざんせん", "ある", EPOS::Unknown),
      aux("でありんす", "だ", EPOS::Unknown), aux("でありんした", "だ", EPOS::Unknown),
      aux("なんし", "ます", EPOS::Unknown), aux("なんした", "ます", EPOS::Unknown),

      // Cat-like (猫系) - sentence-final particles (な/ね/よ variants)
      particle("にゃ", EPOS::ParticleFinal), particle("にゃん", EPOS::ParticleFinal), particle("にゃー", EPOS::ParticleFinal),
      aux("だにゃ", "だよ", EPOS::Unknown), aux("だにゃん", "だよ", EPOS::Unknown),
      aux("ですにゃ", "ですよ", EPOS::Unknown), aux("ですにゃん", "ですよ", EPOS::Unknown),

      // Squid character (イカ娘) - sentence-final particle (MeCab: Noun)
      // Note: で+ゲソ should split as で(Particle)+ゲソ(Noun)
      particle("ゲソ", EPOS::ParticleFinal), particle("げそ", EPOS::ParticleFinal),

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
      aux("のじゃ", "のだ", EPOS::Unknown), aux("じゃろ", "だろ", EPOS::AuxCopulaDa),

      // Regional dialects (方言系)
      aux("ぜよ", "だ", EPOS::Unknown), aux("だべ", "だ", EPOS::Unknown), aux("やんけ", "だ", EPOS::Unknown),
      aux("や", "だ", EPOS::Unknown), aux("やねん", "だ", EPOS::Unknown),
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
      conj("そして", ""), conj("それから", ""), conj("それで", ""),
      conj("だから", ""), conj("そのため", ""),
      conj("したがって", "従って"),

      // Adversative (逆接)
      conj("しかし", ""), conj("だが", ""), conj("けれども", ""),
      conj("だけど", ""),  // colloquial variant
      conj("ところが", ""), conj("それでも", ""),
      conj("でも", ""), conj("だって", ""),  // にもかかわらず removed for MeCab compat
      conj("ものの", ""),

      // Parallel/Addition (並列・添加)
      conj("又", ""), conj("及び", ""),
      conj("並びに", ""), conj("且つ", ""),
      conj("更に", ""), conj("次に", ""),
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
      // そんなら removed: MeCab splits as そん+なら
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

      // Note: Quotative verb forms (といって, こういって, etc.) removed for MeCab compatibility
      // MeCab splits as と+いっ+て, こう+いっ+て, etc.

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
      // First person - hiragana/colloquial
      pronoun("わたくし", ""),
      pronoun("あたし", ""),
      pronoun("あたい", ""),
      pronoun("あちき", ""),
      pronoun("わし", ""),
      pronoun("おいら", ""),

      // First person plural: 僕ら/俺ら handled as pronoun + ら suffix
      pronoun("我々", ""),

      // Second person (二人称) - kanji with reading
      pronoun("貴方", ""),
      pronoun("君", ""),
      // Second person - hiragana/mixed only
      pronoun("あなた", ""),
      // B39: お前 needs low cost to beat PREFIX(お)+NOUN(前) split (connection bonus -1.5)
      // PREFIX→NOUN path has cost ~-1.2, so お前 needs cost < -1.2 to win
      pronoun("お前", ""),

      // Second person plural removed - use pronoun + たち suffix

      // Third person (三人称) - kanji with reading
      pronoun("彼", ""),
      pronoun("彼女", ""),
      pronoun("彼氏", ""),
      pronoun("奴", ""),
      // 彼ら/彼女ら removed - handled as pronoun + ら suffix

      // Archaic/Samurai (武家・古風)
      pronoun("我", ""),
      pronoun("拙者", ""),
      pronoun("貴殿", ""),
      pronoun("某", ""),
      pronoun("我輩", ""),
      pronoun("吾輩", ""),

      // Collective pronouns (集合代名詞)
      // Note: 皆さん is split as 皆+さん for MeCab compatibility
      pronoun("皆", ""),
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
      // どう/いかが can take だ/です (どうですか, いかがですか)
      // Register as both adverb and na-adjective for correct copula connection
      adv("どう", ""),
      na_adj("どう", "どう"),
      adv("いかが", ""),
      na_adj("いかが", "いかが"),
      // Note: どうして needs very low cost to prevent split when followed by verb
      // The te-form bonus makes どう+して+VERB cheaper than どうして+VERB
      adv("どうして", ""),
      adv("なぜ", ""),

      // Degree adverbs (程度副詞) - very common, prevent misparse
      // とても could be split as と+て+も without this entry
      adv("とても", ""),
      adv("かなり", ""),
      adv("すごく", ""),
      adv("ちょっと", ""),
      adv("もっと", ""),
      adv("ずっと", ""),
      adv("やっと", ""),
      adv("きっと", ""),
      adv("ちょうど", ""),
      // Temporal adverbs - common, prevent misclassification
      adv("まだ", ""),
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
      // Note: 漢字「所」は削除 - 複合語（所在、場所）の一部として分割を妨げるため
      // ひらがな「ところ」のみ残す
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

// =============================================================================
// Interjections (感動詞)
// =============================================================================
std::vector<DictionaryEntry> getInterjectionEntries() {
  return {
      // Common interjections (exclamations)
      intj("えっ"),      // Surprise
      intj("ええ"),      // Affirmation/Surprise
      intj("あっ"),      // Realization
      intj("ああ"),      // Agreement/Sigh
      intj("おお"),      // Amazement
      intj("うわ"),      // Surprise
      intj("うわっ"),    // Surprise (emphatic)
      intj("わあ"),      // Amazement
      intj("へえ"),      // Interest
      intj("ふーん"),    // Understanding/Disinterest
      intj("ふうん"),    // Understanding
      // Note: ほう removed - formal noun usage (ほうがいい) is more common
      intj("おい"),      // Calling attention
      intj("おーい"),    // Calling from afar
      intj("あれ"),      // Confusion
      intj("あれっ"),    // Confusion (emphatic)
      intj("まあ"),      // Surprise/Moderation
      intj("ねえ"),      // Attention-getting (also particle, but standalone usage)
      // Responses
      intj("はい"),      // Yes
      intj("いいえ"),    // No
      intj("うん"),      // Casual yes
      intj("ううん"),    // Casual no
      // Hesitation/Filler
      intj("えーと"),    // Hesitation
      intj("えっと"),    // Hesitation
      intj("あの"),      // Hesitation (also determiner)
      intj("その"),      // Hesitation (rare, also determiner)
  };
}


}  // namespace suzume::dictionary::entries
