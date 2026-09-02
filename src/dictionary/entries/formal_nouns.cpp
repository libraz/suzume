#include "entries_internal.h"

namespace suzume::dictionary::entries {

EntrySpecRange getFormalNounEntries() {
  static constexpr EntrySpec kEntries[] = {
      // Formal nouns (形式名詞) - hiragana form is canonical in modern Japanese
      // こと/もの are grammatical function words, hiragana is preferred
      formal_noun("事", "こと"),
      formal_noun("こと", ""),
      formal_noun("物", "もの"),
      formal_noun("もの", ""),
      // Honorific nominal used in fixed request and invitation frames.
      formal_noun("おいで", ""),
      // Colloquial contraction of the formal noun (行くもんか, うまいもんだ).
      // Without it the two morae are read as a focus particle plus the
      // nominalizer, or absorbed into a fabricated verb.
      formal_noun("もん", ""),
      formal_noun("為", ""),
      formal_noun("ため", ""),
      // Formal noun in the negative-experience construction (行ったためしがない).
      formal_noun("ためし", ""),
      // Note: 漢字「所」は削除 - 複合語（所在、場所）の一部として分割を妨げるため
      // ひらがな「ところ」のみ残す
      formal_noun("ところ", ""),
      formal_noun("どころ", ""),
      formal_noun("ころ", ""),
      formal_noun("時", ""),
      formal_noun("内", ""),
      // Bound temporal/spatial suffix after a nominal stem (期間内、期限内).
      // Keep the formal-noun entry above for standalone uses as well.
      suffix("内", ""),
      // State suffix after a nominal stem (作業中、確認中). The standalone
      // formal-noun reading remains available through the preceding entry.
      suffix("中", ""),
      // Agentive suffix after a deverbal stem (引き受け手、書き手).
      suffix("手", ""),
      // Destination suffix after a deverbal nominal (問い合わせ先、送り先).
      suffix("先", ""),
      // Temporal endpoint formal noun (年度末、学期末). Short lexical compounds
      // such as 月末 retain their whole-word candidate.
      formal_noun("末", ""),
      // Approximation suffixes on a quantity phrase (三割強、二時間弱). They
      // modify the amount rather than join it, so the quantity keeps its own
      // boundary — the digit spelling already splits (10割|強) and the kanji
      // spelling has to agree. Lexical compounds (勉強、衰弱) keep their
      // whole-word candidate because only a numeral+counter head reaches the
      // quantity boundary rule.
      suffix("強", ""),
      suffix("弱", ""),
      formal_noun("あいだ", ""),
      formal_noun("うち", ""),
      formal_noun("途中", ""),
      formal_noun("たび", ""),
      // Temporal reference-point formal nouns (開始以来、開始以降).
      formal_noun("以来", ""),
      formal_noun("以降", ""),
      formal_noun("以前", ""),
      // Approximate temporal point (中旬ごろ、夕方ごろに).
      formal_noun("ごろ", ""),
      formal_noun("どき", ""),
      formal_noun("通り", ""),
      formal_noun("とおり", ""),
      formal_noun("限り", ""),
      formal_noun("かぎり", ""),
      // Suffix-like formal nouns
      // Lower cost for 付け to compete with verb_kanji ichidan pattern
      formal_noun("付け", ""),
      // Per-unit distributive formal noun (一人当たり, 利用者当たり).
      formal_noun("当たり", ""),
      // Hiragana-only forms
      formal_noun("よう", ""),
      formal_noun("ほう", ""),  // Must compete with ordinary-noun candidates.
      // Formal noun for conditions and prerequisites (読むうえで, 読んだうえで).
      formal_noun("うえ", ""),
      // Formal noun for comparison and degree (読むわりに, 食べるわりだ).
      formal_noun("わり", ""),
      // Concessive formal noun after an attributive clause (読むくせに,
      // 高いくせに, 静かなくせに).
      formal_noun("くせ", ""),
      // Formal noun for substitution or contrast (読むかわりに, 本のかわりに).
      formal_noun("かわり", ""),
      formal_noun("代わり", ""),
      // 風 (manner/style): bound formal noun (こんなふうに, そういうふうに).
      // Registered so the u-ending verb candidate path does not fabricate a
      // ふう/VERB reading now that 2-char う stems are admitted.
      formal_noun("ふう", ""),
      // Literary formal noun in conditional expressions (いかんにかかわらず,
      // 結果いかんで).
      formal_noun("いかん", "いかん"),
      formal_noun("わけ", ""),
      formal_noun("すべ", ""),
      formal_noun("よし", ""),
      // Causal formal noun followed by a copula (読むゆえだ, 本のゆえだ).
      // The demonstrative-bound suffix reading remains available separately.
      formal_noun("ゆえ", ""),
      // Formal noun for a basis or surrounding condition (読むもとで,
      // 食べるもとに).
      formal_noun("もと", ""),
      // Formal noun in the certainty predicate (本にちがいない,
      // 読むに違いない). Keep its nominal reading over the homographic
      // Godan verb renyokei.
      formal_noun("ちがい", ""),
      formal_noun("違い", "ちがい"),
      // Causal formal noun after an attributive clause or nominalizer:
      // 本のせいで, 読むせいで.
      formal_noun("せい", ""),
      // Formal noun expressing a risk after an attributive clause
      // (遅れるおそれがある, 欠けるおそれはない).
      formal_noun("おそれ", ""),
      // Lexicalized nominal expressions whose initial お is no longer a
      // productive honorific prefix. Keep each as one search unit.
      formal_noun("おかげ", ""),
      formal_noun("おしまい", ""),
      formal_noun("はず", "はず"),
      // Formal noun for conditional cases (読む場合、必要な場合).
      formal_noun("場合", ""),
      // Fixed degree expression (ある程度は理解できる).
      adv("ある程度", ""),
      formal_noun("つもり", ""),
      // Intended target/purpose after an attributive predicate
      // (読むあてがない, 確認するあてもない).
      formal_noun("あて", ""),
      // Formal noun for an incidental accompanying action (書いたついでに).
      formal_noun("ついで", ""),
      // Formal noun for a simultaneous/parallel action (読むかたわら書く).
      formal_noun("かたわら", ""),
      // Formal noun for a feigned action or state (知らないふりをする).
      formal_noun("ふり", ""),
      // Temporal formal noun after a past clause (読んだとたん書く).
      formal_noun("とたん", ""),
      // Temporal formal noun after a past clause (書いたそばから読む).
      formal_noun("そば", ""),
      // Immediate-sequence expression (読むや否や書く).
      formal_noun("否や", ""),
      // Resulting-state formal noun after a past clause (読んだあげく書く).
      formal_noun("あげく", ""),
      // Result formal noun after a past clause (考えすぎたあまり眠れない).
      formal_noun("あまり", ""),
      formal_noun("まま", ""),
      formal_noun("ほか", "ほか"),
      formal_noun("他", "ほか"),
      // Fixed negative predicate: ほかなら+ない (none other than).
      verb("ほかなら", "ほかなる", EPOS::VerbMizenkei),
      // Abstract nouns that don't form suru-verbs
      formal_noun("仕方", ""),
      formal_noun("しかた", ""),
      formal_noun("たたずまい", "たたずまい"),  // noun, not suru-verb
      // Bound suffix in the negative-completion construction (見+ず+じまい).
      suffix("じまい", "じまい"),
      // Exclusivity suffix attached to a nominal predicate (地域+ならでは+の).
      suffix("ならでは", "ならでは"),
      // NOTE: 〜がち forms are split as V連用形 + がち (suffix) by the split path, not merged.
      // Bound noun used as the fixed component of うだつが上がらない.
      formal_noun("うだつ", "うだつ"),
  };
  return makeEntrySpecRange(kEntries);
}

}  // namespace suzume::dictionary::entries
