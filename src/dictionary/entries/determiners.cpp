#include "entries_internal.h"

namespace suzume::dictionary::entries {

EntrySpecRange getDeterminerEntries() {
  static constexpr EntrySpec kEntries[] = {
      // Demonstrative determiners (指示連体詞) - この/その/あの/どの
      det("この", ""),
      det("その", ""),
      det("其の", ""),  // kanji variant of その
      det("あの", ""),
      det("どの", ""),
      // Demonstrative determiners (指示連体詞) - こんな/そんな/あんな/どんな
      det("こんな", ""),
      det("そんな", ""),
      det("あんな", ""),
      det("どんな", ""),

      // Other determiners (連体詞)
      det("ある", ""),
      det("あらゆる", ""),
      det("いかなる", ""),
      det("いわゆる", ""),
      det("あまりの", ""),  // excessive-degree determiner (あまりの暑さ)
      det("ほんの", ""),    // small-degree determiner (ほんの少し)
      det("いろんな", ""),  // colloquial variety determiner (= いろいろな), not an adjective
      det("おかしな", ""),
      det("同じ", ""),      // same - prevent VERB confusion
      det("単なる", ""),    // fixed attributive determiner, not a finite verb
      det("たいした", ""),  // 大した - prevent 願望たい+し+た split (たいした問題)
      det("大した", ""),    // kanji spelling of the fixed evaluative determiner
      det("あくる", ""),    // 時間名詞を修飾する固定連体詞
      // Evaluative determiner (とんだ災難). Homographic with the past tense of
      // 飛ぶ/跳ぶ, which is written with its kanji when that verb is meant.
      det("とんだ", ""),
      det("何らかの", ""),  // indefinite determiner (何らかの方法)
      // Fixed attributive whose stem is not a word of its own: 名だ is the
      // copula on 名, which is why the productive たり paradigm does not reach
      // this form. Without the entry the run splits at that false boundary.
      det("名だたる", ""),

      // Demonstrative manner determiners (指示様態連体詞)
      // Lower cost to compete with X + いう (VERB cost 0.3) splits
      det("こういう", ""),
      det("そういう", ""),
      det("ああいう", ""),
      det("どういう", ""),

      // Quotative determiners (引用連体詞) - prevents incorrect split like 病+とい+う
      // Lower cost to beat と(PARTICLE,-0.4)+いった(VERB,-0.034)+conn(0.2)=-0.232
      quotative_det("という", ""),
      quotative_det("といった", ""),
      quotative_det("っていう", ""),  // colloquial
      // Naming quotative built on the 並立助詞 とか (確認とかいう話). It heads the
      // same attributive slot as という, so without the entry the shared trailing
      // い is read as adjective okurigana on the noun to its left.
      quotative_det("とかいう", ""),

      // Quotative verb forms are compositional particle/adverb + verb + particle units.

      // Kanji determiners must win over unknown-noun candidates.
      det("大きな", ""),
      det("小さな", ""),
      det("おっきな", ""),  // colloquial variant of 大きな

      // Classical possessive determiner (我が家, 我が子, 我が国)
      det("我が", ""),
      det("わが", ""),
      det("吾が", ""),

      // Classical/literary determiner (斯かる = such, this kind of)
      // Note: shares hiragana surface with godan-ra verb 掛かる/懸かる (L2: かかる).
      // L1 Determiner competes with VERB in determiner+NOUN contexts (かかる事態).
      det("かかる", ""),

      // Classical/literary determiner (彼の = that, the aforementioned)
      // Without L1, over-splits to か(unknown)+の(particle).
      // Same pattern as かかる above.
      det("かの", ""),

      // Fixed literary determiner (大いなる希望).
      det("大いなる", ""),

      // Fixed formal determiner (更なる説明).
      det("更なる", ""),
      det("しかるべき", ""),

      // Residual formal determiners form a closed attributive class.
      det("さる", ""),
      det("きたる", ""),
      det("こうした", ""),
      det("そうした", ""),
  };
  return makeEntrySpecRange(kEntries);
}

}  // namespace suzume::dictionary::entries
