#include "entries_internal.h"

namespace suzume::dictionary::entries {

EntrySpecRange getPronounEntries() {
  static constexpr EntrySpec kEntries[] = {
      // First person (一人称) - kanji with reading
      // Note: 私/俺 have lower cost to encourage splits (第一私は → 第一+私+は)
      // But 僕 keeps higher cost due to compounds like 下僕
      pronoun("私", ""),
      pronoun("僕", ""),
      pronoun("俺", ""),
      // First person - hiragana/colloquial
      pronoun("わたし", ""),
      pronoun("ぼく", ""),
      pronoun("おれ", ""),
      pronoun("わたくし", ""),
      pronoun("あたし", ""),
      pronoun("あたい", ""),
      pronoun("あちき", ""),
      pronoun("わし", ""),
      pronoun("おいら", ""),
      // Katakana spellings of the same first-person set (emphatic or stylized
      // writing); ワシ is left out because it is also the bird.
      pronoun("ボク", ""),
      pronoun("オレ", ""),
      pronoun("ワタシ", ""),
      pronoun("ワタクシ", ""),
      pronoun("アタシ", ""),
      pronoun("オイラ", ""),

      // First person plural: 僕ら/俺ら handled as pronoun + ら suffix
      pronoun("我々", ""),

      // Second person (二人称) - kanji with reading
      pronoun("貴方", ""),
      pronoun("君", ""),
      // Second person - hiragana/mixed only
      pronoun("あなた", ""),
      pronoun("きみ", ""),
      // Prefer お前 over the PREFIX(お)+NOUN(前) split (connection bonus -1.5).
      // PREFIX→NOUN path has cost ~-1.2, so お前 needs cost < -1.2 to win
      pronoun("お前", ""),
      pronoun("おまえ", ""),

      // Second person plural removed - use pronoun + たち suffix

      // Third person (三人称) - kanji with reading
      pronoun("彼", ""),
      pronoun("彼女", ""),
      pronoun("かれ", ""),
      pronoun("かのじょ", ""),
      pronoun("奴", ""),
      pronoun("やつ", ""),
      // 彼ら/彼女ら removed - handled as pronoun + ら suffix

      // Archaic/Samurai (武家・古風)
      pronoun("我", ""),
      pronoun("われ", ""),  // 我/吾 classical first-person pronoun
      pronoun("己", ""),
      pronoun("おのれ", ""),
      pronoun("拙者", ""),
      // Archaic and dialectal personal pronouns, a closed set the reference
      // lexicon splits or misreads (そなた, おぬし, 拙僧).
      pronoun("そなた", ""),
      pronoun("おぬし", ""),
      pronoun("わて", ""),
      pronoun("それがし", ""),
      pronoun("わらわ", ""),
      pronoun("拙僧", ""),
      pronoun("そち", ""),
      pronoun("貴殿", ""),
      pronoun("某", ""),
      pronoun("我輩", ""),
      pronoun("吾輩", ""),

      // Collective pronouns (集合代名詞)
      // Keep the collective pronoun and honorific suffix as separate search units.
      pronoun("皆", ""),
      pronoun("みんな", ""),
      pronoun("みな", ""),

      // Distributive pronoun (分配代名詞)
      pronoun("各々", ""),
      pronoun("各自", ""),

      // Demonstrative - proximal (近称)
      pronoun("これ", ""),
      pronoun("ここ", ""),
      pronoun("こちら", ""),
      // Colloquial demonstratives - prevent っち split
      pronoun("こっち", ""),
      pronoun("そっち", ""),
      pronoun("あっち", ""),
      pronoun("どっち", ""),
      pronoun("あちこち", ""),

      // Demonstrative - medial (中称)
      pronoun("それ", ""),
      pronoun("そこ", ""),
      pronoun("そちら", ""),
      // Fixed medial locative compound (そこかしこ): retain its two
      // demonstrative search units rather than splitting かしこ internally.
      pronoun("かしこ", ""),

      // Demonstrative - distal (遠称)
      pronoun("あれ", ""),
      pronoun("あそこ", ""),
      pronoun("あちら", ""),
      pronoun("かなた", ""),
      // Fixed demonstrative collection: keep the closed expression as one
      // search unit instead of two independent demonstratives.
      pronoun("あれこれ", ""),

      // Demonstrative - person reference (こそあど+いつ)
      pronoun("こいつ", ""),
      pronoun("そいつ", ""),
      pronoun("あいつ", ""),
      pronoun("どいつ", ""),

      // Demonstrative - interrogative (不定称)
      pronoun_interrogative("どれ", ""),
      pronoun_interrogative("どこ", ""),
      pronoun_interrogative("どちら", ""),
      pronoun_interrogative("どなた", ""),

      // Indefinite (不定代名詞) - kanji with reading
      // Low cost to act as strong anchors against prefix compounds (今何 → 今+何)
      // Cost -1.5 to beat: 今(1.4) + 何(-1.9) + conn(0.5) = 0.0 < 今何(0.5)
      pronoun_interrogative("誰", ""),
      pronoun_interrogative("何", ""),
      // Hiragana interrogatives (だれ=誰, なに=何): closed class, mirror the
      // kanji forms so 誰か/何か boundaries resolve in pure-hiragana text
      pronoun_interrogative("だれ", ""),
      pronoun_interrogative("なに", ""),
      // Euphonic variant of なに before だ/の/で/と (なんだ, なんの, なんで).
      // It spells its own lemma, like the other kana interrogatives above.
      pronoun_interrogative("なん", ""),
      // Closed indefinite pronoun, not an interrogative plus final particle.
      pronoun_interrogative("何かしら", ""),
      pronoun("よそ", ""),  // 外所: place/other-party pronoun

      // Interrogatives (疑問詞)
      pronoun_interrogative("いつ", ""),
      pronoun_interrogative("いくつ", ""),
      // Kanji spelling of the same interrogatives. Without them the counter is
      // rebuilt as a godan-ta predicate on the quantity prefix (幾/つか).
      pronoun_interrogative("幾つ", ""),
      pronoun_interrogative("幾ら", ""),
      pronoun_interrogative("いくら", ""),
      pronoun_interrogative("なんぼ", ""),
      // どう/いかが can take だ/です (どうですか, いかがですか)
      // Register as both adverb and na-adjective for correct copula connection
      quotative_adv("どう", ""),
      na_adj("どう", "どう"),
      adv("いかが", ""),
      na_adj("いかが", "いかが"),
      na_adj("さまざま", "さまざま"),
      na_adj("同じ", "同じ"),
      na_adj("おなじ", "おなじ"),
      // Note: どうして needs very low cost to prevent split when followed by verb
      // The te-form bonus makes どう+して+VERB cheaper than どうして+VERB
      interrogative_adv("どうして", ""),
      // どうか is one adverb before a predicate (どうか+してる, どうか+助けて);
      // elsewhere it is どう+か (どうかな). The tokenizer gates the context.
      adv("どうか", ""),
      interrogative_adv("なぜ", ""),
      interrogative_adv("なんで", ""),

      // Classical/literary adverbs (古語・文語副詞)
      adv("かく", ""),      // 斯く - classical demonstrative adverb (=こう/such)
      adv("なんと", ""),    // exclamatory adverb (感嘆副詞)
      adv("なんとか", ""),  // indefinite adverb (somehow/one way or another)
      // Note: 悪しからず is not registered. It is an adjective irrealis plus the
      // negative auxiliary, so the whole-phrase entry crossed a lexical
      // inflection boundary the segmentation rules keep (see AGENTS.md §2).
      adv("何とか", ""),    // kanji-mixed spelling of なんとか
      adv("何とも", ""),    // degree/evaluative adverb (何とも言えない)
      adv("なんとも", ""),  // hiragana orthography

      // Degree adverbs (程度副詞) - very common, prevent misparse
      // とても could be split as と+て+も without this entry
      adv("とても", ""),
      adv("かなり", ""),
      adv("多少なりとも", ""),
      adv("たいそう", ""),
      adv("たいして", ""),
      adv("しいて", ""),
      adv("さほど", ""),
      adv("然程", ""),
      // 大変 is a degree adverb and an adjectival noun at once: it modifies a
      // predicate directly (大変おいしい) and it also inflects through the copula
      // (大変だ, 大変な問題). Both readings have to be present for the connection
      // rules to choose, the same way どう and いかが are registered above.
      adv("たいへん", ""),
      na_adj("たいへん", "たいへん"),
      adv("大変", ""),
      na_adj("大変", "大変"),
      // 当然 is the same double word (当然知っている, 当然だ, 当然な結果).
      adv("当然", ""),
      na_adj("当然", "当然"),
      adv("すごく", ""),
      adv("ちょっと", ""),
      adv("もっと", ""),
      adv("ずっと", ""),
      adv("やっと", ""),
      adv("きっと", ""),
      adv("必ず", ""),
      adv("ちょうど", ""),
      // Temporal adverbs - common, prevent misclassification
      adv("まだ", ""),
      adv("まだしも", ""),

      // Native Japanese numeral counters form a finite closed class.
      noun_number("ひとつ", ""),
      noun_number("ふたつ", ""),
      noun_number("みっつ", ""),
      noun_number("よっつ", ""),
      noun_number("いつつ", ""),
      noun_number("むっつ", ""),
      noun_number("ななつ", ""),
      noun_number("やっつ", ""),
      noun_number("ここのつ", ""),
      noun_number("とお", ""),
      noun_number("ひとり", ""),
      noun_number("いち", ""),
      noun_number("よん", ""),
      noun_number("ひと月", ""),
      noun_number("ふた月", ""),

      // Deictic calendar nouns are a finite temporal class. Keep their full
      // kana spelling rather than reopening internal verb/particle readings.
      noun("おととい", ""),
      noun("きのう", ""),
      noun("あした", ""),
      noun("あさって", ""),

      // Fixed temporal and frequency adverbs. These are closed lexical
      // function words, not productive pronoun-plus-particle sequences.
      adv("いつか", ""),
      adv("間もなく", ""),
      adv("まもなく", ""),
      adv("ときどき", ""),
      adv("ときおり", ""),
      adv("ほどなく", ""),

      // Fixed formal and literary adverbs. These are a finite function-word
      // class; their endings otherwise attract productive particle and verb
      // candidates that do not reflect their synchronic use.
      adv("かならずしも", ""),
      adv("ことに", ""),
      adv("必ずや", ""),
      adv("いたく", ""),
      na_adj("むやみ", "むやみ"),
      adv("よもや", ""),
      adv("いと", ""),
      adv("しだいに", ""),
      adv("わざと", ""),
      adv("ところどころ", ""),
      adv("もしや", ""),
      adv("いたずらに", ""),
      adv("とみに", ""),
      adv("いまや", ""),
      adv("ひるがえって", ""),
      adv("いよいよ", ""),
      adv("やや", ""),
      adv("おもに", ""),
      // Closed negative-polarity degree adverb (あまり食べない,
      // あまり明るくない).  The homographic result formal noun remains
      // available after an attributive past clause (考えすぎた+あまり).
      adv("あまり", ""),
      adv("余りに", ""),
      adv("何ら", ""),
      na_adj("あらた", "あらた"),
      adv("引続き", ""),
      adv("一層", ""),
      adv("たちまち", ""),
      adv("つねづね", ""),
      adv("ひととおり", ""),
      adv("言わずもがな", ""),
      adv("ひじょうに", ""),
      adv("たしかに", ""),
      adv("ただちに", ""),

      // Fixed degree and discourse adverbs. Their surface endings otherwise
      // attract unrelated verb, particle, and auxiliary candidates.
      adv("いっそう", ""),
      adv("最も", ""),
      adv("あらためて", ""),
      adv("とくに", ""),
      adv("まったく", ""),
      adv("全く", ""),
      adv("概して", ""),
      adv("あらかた", ""),
      adv("おおかた", ""),
      adv("あらまし", ""),
      adv("きわめて", ""),
      // Additional fixed adverbial function words. These are closed lexical
      // expressions; treating them as a unit prevents unrelated inflectional
      // fragments from winning in hiragana or mixed-script input.
      adv("全然", ""),
      adv("ずいぶん", ""),
      adv("なにぶん", ""),
      adv("大抵", ""),
      adv("凡そ", ""),
      adv("むしろ", ""),
      adv("いったん", ""),
      adv("いまだに", ""),
      adv("未だに", ""),
      adv("どうしても", ""),
      adv("できるだけ", ""),
      adv("ひとまず", ""),
      adv("ことごとく", ""),
      adv("おおむね", ""),
      adv("せいぜい", ""),
      adv("とうてい", ""),
      adv("いかに", ""),
      adv("いかで", ""),
      adv("いかほど", ""),
      adv("あまつさえ", ""),
      adv("たかだか", ""),
      adv("わずか", ""),
      // Fixed degree adverb. Unlike productive X+的 modifiers (相対的安全),
      // 比較的 selects the following predicate as an independent adverbial.
      adv("比較的", ""),
      adv("何しろ", ""),
      // Note: やむを得ない and ろくでもない are not listed here. An L1 entry
      // carries no conjugation type, so a lexicalized i-adjective registered
      // at this layer only ever matches its citation form and loses its own
      // past and continuative (やむを得なかった). They live in L2 as I_ADJ.
      adv("おおよそ", ""),
      adv("よほど", ""),
      adv("ろくすっぽ", ""),
      adv("さして", ""),
      adv("さも", ""),
      adv("つとに", ""),
      adv("つぶさに", ""),
      adv("おのずと", ""),
      adv("しきりに", ""),
      adv("いまだ", ""),
      adv("このほど", ""),
      adv("概ね", ""),
      adv("今さら", ""),
      adv("一応", ""),
      adv("一切", ""),
      adv("一切合切", ""),
      adv("以上", ""),
      adv("何一つ", ""),
      // Fixed adverbs with productive-looking endings. They form a closed
      // lexical class, while their internal moras otherwise attract prefix,
      // particle, or formal-noun candidates.
      adv("おのずから", ""),
      adv("おもむろに", ""),
      adv("ことさら", ""),
      adv("およそ", ""),
      adv("もっぱら", ""),
      adv("ひとしお", ""),
      adv("ひとたび", ""),
      adv("なにとぞ", ""),
      adv("ぜひとも", ""),
      adv("何もかも", ""),
      adv("何より", ""),
      adv("取りあえず", ""),
      adv("予め", ""),
      adv("予てから", ""),
      adv("ひとしきり", ""),
      adj("余儀なく", "余儀ない", EPOS::AdjRenyokei),

      // Compound adverb (改めて = anew/once more) - 動詞「改める」連用形+て の語彙化
      // MeCab: 改めて → 改めて(副詞)
      adv("改めて", ""),
  };
  return makeEntrySpecRange(kEntries);
}

}  // namespace suzume::dictionary::entries
