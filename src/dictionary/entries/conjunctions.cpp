#include "entries_internal.h"

namespace suzume::dictionary::entries {

EntrySpecRange getConjunctionEntries() {
  static constexpr EntrySpec kEntries[] = {
      // Sequential (順接)
      conj("従って", ""), conj("故に", ""), conj("ゆえに", ""), conj("そして", ""), conj("そうして", ""),
      conj("そうすると", ""), conj("すると", ""), conj("それから", ""), conj("それで", ""), conj("だから", ""),
      conj("したがって", ""), conj("ついては", ""), conj("もって", ""), conj("よって", ""), conj("だからといって", ""),
      conj("だからこそ", ""),
      // Note: そのため is not listed. Unlike the copula-initial connectives
      // above it has no clause-opening reading of its own — it is the
      // determiner plus a formal noun in every position, exactly as このため
      // is, and listing only one member of that pair split them apart.

      // Adversative (逆接)
      conj("しかし", ""), conj("然し", ""), conj("しかしながら", ""), conj("だが", ""), conj("けれども", ""),
      conj("だけど", ""),  // colloquial variant
      conj("ところが", ""), conj("それでも", ""), conj("それなのに", ""), conj("でも", ""),
      conj("だって", ""),  // にもかかわらず removed for MeCab compat
      conj("それどころか", ""), conj("されど", ""), conj("さりとて", ""), conj("しかるに", ""), conj("もっとも", ""),
      conj("尤も", ""),

      // Parallel/Addition (並列・添加)
      conj("又", ""), conj("及び", ""), conj("および", ""), conj("並びに", ""), conj("ならびに", ""), conj("且つ", ""),
      conj("かつ", "かつ"), adv("更に", ""), conj("次いで", ""),
      // 次に is 次(noun)+に(particle), not a closed-class conjunction — the oracle
      // splits it, so keep it out of L1 to avoid a spurious single-token merge.
      conj("しかも", ""), conj("そのうえ", ""),

      // Alternative (選択)
      conj("或いは", ""), conj("又は", ""), conj("若しくは", ""), conj("または", ""), conj("ないしは", ""),
      conj("ないし", ""), conj("それとも", ""), conj("あるいは", "或いは"), conj("もしくは", ""),

      // Explanation/Supplement (説明・補足)
      conj("即ち", ""), conj("すなわち", ""), conj("例えば", ""), conj("但し", ""), conj("ただし", ""), conj("尚", ""),
      conj("なお", ""), conj("つまり", ""), conj("たとえば", ""), conj("なぜなら", ""), conj("ちなみに", ""),
      conj("まして", ""), conj("ましてや", ""),

      // Topic change (転換)
      conj("さて", ""), adv("さては", ""), verb("さておき", "さておく", EPOS::VerbRenyokei), conj("ところで", ""),
      // では is the copula continuative fused with the topic particle, the same
      // shape as でも. It opens a clause only where nothing precedes it; with a
      // host in front the same characters are the copula predicating over that
      // host plus the particle (東京+で+は+なく). The scorer keeps the two apart
      // through isCopulaFusedConjunction, which covers both members of the pair.
      conj("では", ""), conj("それでは", ""),

      // Additional conjunctions
      conj("いわば", "言わば"), conj("言わば", ""), conj("さもないと", ""), conj("さもなければ", ""),
      // Note: とすれば removed - grammatically と + すれ (する仮定形) + ば
      // Colloquial conditional conjunction. Keep the closed search unit
      // rather than following MeCab's non-lexical そん+なら split.
      conj("そんなら", ""), conj("それなら", ""), conj("それにしても", ""), adv("ともかく", ""),
      // Lexicalized adverb (何かと忙しい). Its pieces are all closed class, so
      // nothing recovers the adverbial reading once they are split apart, and
      // the sibling sequences stay productive (何かに, 何かで).
      adv("何かと", ""),
      // Keep productive particle/auxiliary sequences searchable as their
      // constituent morphemes (いずれ + に + し + て + も), rather than
      // treating the whole sequence as a fixed conjunction.

      // Closed-class function adverbs that over-split into non-word verbs without an L1 anchor
      // (呼応副詞 めったに requires a 否定; 陳述副詞 どうぞ). Like the ともかく entry above, these
      // are function adverbs kept in L1 to beat the spurious verb decompositions. Kanji-initial
      // 決して is intentionally NOT registered here: it would swallow the 決 of 解決して
      // (解決|し|て → 解|決して); its 決し(非語 VERB)+て over-split needs a candidate-side fix.
      adv("もとより", ""),     // 追加・強調: 本はもとより水を読む
      adv("いとも", ""),       // 文語の程度副詞
      adv("たえず", ""),       // 文語の頻度副詞
      adv("あまねく", ""),     // 文語の範囲副詞
      na_adj("もっとも", ""),  // 評価用法: もっともな理由
      noun("すべて", ""),      // 全称の閉じた名詞用法
      // 多く は数量形容詞の連用形がそのまま名詞化した用法を持つ（本の多く、
      // 多くの本）。連用形そのものは活用規則から出るので、規則が供給できない
      // 名詞の読みだけを足して接続規則に選ばせる。
      noun("多く", "多く"), conj("ともあれ", ""),                     // 譲歩・話題転換: ともあれ始める
      conj("いっぽう", ""), conj("そこで", ""), adv("とりわけ", ""),  // Focus adverb
      conj("なかんずく", ""),                                         // Literary additive conjunction
      adv("取り分け", ""),                                            // Orthographic variant
      adv("目の当たり", ""),                                          // Fixed evidential adverb
      adv("めったに", ""),      // 滅多に〜ない - prevent めった(非語 VERB める)+に split
      adv("めちゃ", ""),        // Colloquial degree adverb
      na_adj("めった", ""),     // めったなことではない
      adv("どうぞ", ""),        // 陳述副詞 - prevent どう(ADJ)+ぞ split
      adv("あえて", ""),        // 意図的選択: あえ(非語一段動詞)+て を防ぐ
      adv("あくまで", ""),      // 限定・強調: あく(動詞)+まで を防ぐ
      adv("あくまでも", ""),    // 強調形: 閉じた限定副詞として最大一致
      adv("はっきり", ""),      // 固定した様態副詞
      adv("飽くまで", ""),      // Orthographic variant
      adv("いたって", ""),      // 程度: いたっ(動詞音便)+て を防ぐ
      adv("すこぶる", ""),      // 程度: す+こぶる の非語分解を防ぐ
      adv("おおいに", ""),      // 程度: おお+い+に の分解を防ぐ
      adv("また", ""),          // 追加・反復副詞
      adv("たいてい", ""),      // 頻度副詞
      adv("ふたたび", ""),      // 反復副詞
      adv("どのみち", ""),      // 結論副詞
      adv("どうにか", ""),      // 様態副詞
      adv("ふいに", ""),        // 突発の様態副詞
      adv("いま", ""),          // 時間副詞（いまなお は いま + なお）
      noun("それなり", ""),     // 指示的な程度名詞: それなり+に/の/だ
      noun("万が一", ""),       // 仮定の定型名詞: 内部の が は生産的な格助詞ではない
      adv("いっさい", ""),      // 否定呼応の限定副詞
      adv("いっこうに", ""),    // 否定呼応の程度副詞
      adv("ゆっくり", ""),      // 様態副詞
      adv("とうに", ""),        // 時間副詞
      adv("さしも", ""),        // 強調副詞
      adv("つとめて", ""),      // 努力: つ+とめ+て の非語分解を防ぐ
      adv("ひいては", ""),      // 帰結・拡張: ひい+て+は を防ぐ
      adv("かえって", ""),      // 逆接・予想外: かえっ(動詞音便)+て を防ぐ
      adv("却って", ""),        // Orthographic variant
      adv("たった", ""),        // 限定の程度副詞: たっ(動詞音便)+た を防ぐ
      adv("直ちに", ""),        // 即時: 直ち+に の分解を防ぐ
      adv("いかにも", ""),      // 強意: いかに+も の分解を防ぐ
      adv("まさしく", ""),      // 強意: OTHER フォールバックを防ぐ
      adv("至って", ""),        // 程度: 至+って の分解を防ぐ
      adv("案外", ""),          // 評価副詞: 後続ナ形容詞との未知語併合を防ぐ
      adv("いかんせん", ""),    // 評価・譲歩の定型副詞
      adv("おしなべて", ""),    // 総括副詞
      adv("総じて", ""),        // 総括副詞
      adv("さしあたり", ""),    // 当面の時間副詞
      adv("かたがた", ""),      // 目的併記の定型副詞
      adv("かねて", ""),        // Fixed temporal adverb
      adv("予て", ""),          // Orthographic variant
      adv("なんら", ""),        // 否定呼応の総称副詞
      adv("互いに", ""),        // 相互副詞
      adv("なにせ", ""),        // 理由強調副詞
      adv("何せ", ""),          // Orthographic variant
      adv("あいかわらず", ""),  // 継続副詞
      adv("あいにく", ""),      // 逆接副詞
      adv("生憎", ""),          // Orthographic variant
      adv("つねに", ""),        // 恒常副詞
      adv("おそらくは", ""),    // Fixed probability adverb
      particle("がてら", EPOS::ParticleConj),     // purpose-combining conjunctive expression
      particle("ていう", EPOS::ParticleQuote),    // 口語引用表現
      particle("やら", EPOS::ParticleAdverbial),  // 列挙助詞
  };
  return makeEntrySpecRange(kEntries);
}

}  // namespace suzume::dictionary::entries
