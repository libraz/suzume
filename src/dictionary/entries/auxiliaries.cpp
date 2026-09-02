#include "entries_internal.h"

namespace suzume::dictionary::entries {

EntrySpecRange getAuxiliaryEntries() {
  static constexpr EntrySpec kEntries[] = {
      // Copula/Assertion - だ (断定)
      aux("だ", "だ", EPOS::AuxCopulaDa),
      aux("だっ", "だ", EPOS::AuxCopulaDa),  // 連用タ接続形
      aux("で", "だ", EPOS::AuxCopulaDa),    // copula renyokei
      aux("だったら", "だ", EPOS::AuxCopulaDa),
      aux("な", "だ", EPOS::AuxCopulaDa),  // attributive form (連体形)

      // Copula/Assertion - です (丁寧断定)
      aux("です", "です", EPOS::AuxCopulaDesu),
      aux("でし", "です", EPOS::AuxCopulaDesu),  // renyoukei of です
      aux("でしたら", "です", EPOS::AuxCopulaDesu),
      // で+ある pattern - ある is a separate auxiliary (MeCab compatible)
      aux("ある", "ある", EPOS::AuxCopulaDa),  // で+ある (assertion)
      aux("あっ", "ある", EPOS::AuxCopulaDa),  // で+あっ+た (sokuonbin before た)
      aux("あり", "ある", EPOS::AuxCopulaDa),  // で+あり+ます
      aux("あろ", "ある", EPOS::AuxCopulaDa),  // で+あろ+う (volitional)
      // Existential ある has the same surfaces as the formal copula, but remains
      // an independent verb after a nominal predicate (本あってこそ, 本あれば).
      verb("ある", "ある", EPOS::VerbShuushikei),
      verb("あり", "ある", EPOS::VerbRenyokei),
      verb("あっ", "ある", EPOS::VerbOnbinkei),
      verb("あれ", "ある", EPOS::VerbKateikei),

      // 文語尊敬「あらせる」。存在を表す「ある」の一般的な使役ではなく、
      // 尊敬表現としてのみ用いる閉じた文法語彙であり、受身・尊敬形
      // あらせ+られるの語幹を保持する。
      verb("あらせる", "あらせる", EPOS::VerbShuushikei),
      verb("あらせ", "あらせる", EPOS::VerbMizenkei),

      // Polite (丁寧) - ます
      aux("ます", "ます", EPOS::AuxTenseMasu),
      aux("まし", "ます", EPOS::AuxTenseMasu),    // renyoukei
      aux("ませ", "ます", EPOS::AuxTenseMasu),    // mizenkei
      aux("ましょ", "ます", EPOS::AuxTenseMasu),  // mizenkei, connects to う
      aux("ますれ", "ます", EPOS::AuxTenseMasu),  // kateikei, connects to ば

      // Negation - ない (否定)
      aux("ない", "ない", EPOS::AuxNegativeNai),
      aux("なく", "ない", EPOS::AuxNegativeNai),      // 連用形 (読め+なく)
      aux("なかっ", "ない", EPOS::AuxNegativeNai),    // 連用タ接続
      aux("んかっ", "ない", EPOS::AuxNegativeNai),    // contracted negative past stem
      aux("なけれ", "ない", EPOS::AuxNegativeNai),    // 仮定形 (なけれ+ば)
      aux("なきゃ", "ない", EPOS::AuxNegativeNai),    // 仮定形口語縮約 (なければ→なきゃ, 標準終止)
      aux("なけりゃ", "ない", EPOS::AuxNegativeNai),  // 仮定形口語縮約 (なければ→なけりゃ)
      aux("なかろ", "ない", EPOS::AuxNegativeNai),    // 推量形 (なかろ+う)

      // The obligation predicate in 〜てはいけない. The base form いける
      // remains lexical; this negative stem is auxiliary only in the
      // conditional construction, where its connection is gated by scorer rules.
      aux("いけ", "いける", EPOS::AuxPotential),
      verb("いけ", "いける", EPOS::VerbRenyokei),  // dialectal obligation before ん

      // Negation - ぬ/ず (文語否定)
      aux("ぬ", "ぬ", EPOS::AuxNegativeNu),
      aux("ず", "ぬ", EPOS::AuxNegativeNu),  // lemma is ぬ per MeCab
      adj("なき", "ない", EPOS::AdjBasic),
      adj("がたき", "がたい", EPOS::AdjBasic),
      // Contracted negative-conjunctive form. Keep its displayed lemma as ず
      // because the tokenizer emits ずに as one auxiliary token.
      aux("ずに", "ず", EPOS::AuxNegativeNu),
      aux("ざる", "ぬ", EPOS::AuxNegativeNu),           // 連体形 (せざるを得ない)
      aux("ざり", "ぬ", EPOS::AuxNegativeNu),           // 連用形 (行かざりけり)
      aux("ざれ", "ぬ", EPOS::AuxNegativeNu),           // 已然形 (あらざれば)
      aux("ね", "ぬ", EPOS::AuxNegativeNu),             // 已然形 (行かねば, 死なねば, せねば)
      aux("ごとし", "ごとし", EPOS::AuxSimilitudeYou),  // 如し (比況終止形)
      aux("ごとく", "ごとし", EPOS::Adverb),            // 如く (比況連用形)
      aux("ごとき", "ごとし", EPOS::Determiner),        // 如き (比況連体形)
      // じゃない: removed - split as じゃ(AuxCopulaDa) + ない(AuxNegativeNai)
      aux("ん", "ん", EPOS::AuxNegativeNu),

      // Classical assertion/past なり/けり (文語断定・過去)
      aux("なり", "なり", EPOS::AuxClassicalNari),  // 終止/連体形 断定 (春なり)
      aux("なら", "なり", EPOS::AuxClassicalNari),  // 未然形 (静かならず)
      // 連体形 なる (壮大なる, 静かなる): kept distinct from the verb なる (成る) by the higher
      // AuxClassicalNari category cost, winning only via the AdjNaAdj/Noun→なる→Noun bigram bonus.
      aux("なる", "なり", EPOS::AuxClassicalNari),
      aux("なれ", "なり", EPOS::AuxClassicalNari),  // 已然形 (重要なれば)
      aux("けり", "けり", EPOS::AuxClassicalKeri),  // 過去・詠嘆 (なりけり)
      aux("ける", "けり", EPOS::AuxClassicalKeri),  // Adnominal form (なりける)
      aux("けれ", "けり", EPOS::AuxClassicalKeri),  // 已然形 (見ければ)
      aux("けむ", "けむ", EPOS::AuxVolitional),     // 過去推量 (行きけむ)
      aux("らむ", "らむ", EPOS::AuxVolitional),     // 現在推量 (行くらむ)
      // Classical タリ活用 連体形 たる (堂々たる, 確固たる). Only 連体形 is registered:
      // 終止 たり and 未然 たら collide with the parallel particle たり and the
      // conditional たら, and neither collision has a follower that separates it.
      aux("たる", "たり", EPOS::AuxClassicalTari),
      aux("たり", "たり", EPOS::AuxClassicalPerfect),  // 完了 (行きたり)
      // 已然形 たれ (記録したれども).  These two kana also spell the past auxiliary
      // plus the passive (打た+れ), so unlike the two cells above this one does have
      // a separating follower — the conjunctive particle that selects the cell — and
      // the dictionary edge is admitted only there.  See
      // grammar::spellsHypotheticalAuxiliaryCell.
      aux("たれ", "たり", EPOS::AuxClassicalPerfect),
      // 完了の助動詞つ, 終止形 (見つ, 書きつ). One mora, and the tail of a great
      // many words, so the tokenizer admits it only between a continuative and
      // a clause end.
      aux("つ", "つ", EPOS::AuxClassicalPerfect),
      aux("り", "り", EPOS::AuxClassicalPerfect),  // 存続 (行けり)
      // 完了 ぬ (ナ変). Its terminal shares a spelling with the negative ぬ, which
      // is already registered; the cells that do not are the ones a continuative
      // hosts (花散り+ぬる, 心こそ定まり+ぬれ).
      aux("ぬる", "ぬ", EPOS::AuxClassicalPerfect),  // 連体形 (花ぞ散りぬる)
      aux("ぬれ", "ぬ", EPOS::AuxClassicalPerfect),  // 已然形 (花こそ散りぬれ)
      // Classical past き: the 連体形 し and 已然形 しか both attach to a
      // continuative (読みし人, 見しかば). The 終止形 き spells the Godan-ka
      // continuative as well, so it closes a clause only behind another literary
      // auxiliary (行かざり+き); the connection rules carry that restriction.
      aux("し", "き", EPOS::AuxClassicalKi),
      aux("しか", "き", EPOS::AuxClassicalKi),
      aux("き", "き", EPOS::AuxClassicalKi),

      // Past/Completion - た (過去・完了)
      aux("た", "た", EPOS::AuxTenseTa),
      aux("たら", "た", EPOS::AuxTenseTa),  // 仮定形
      aux("だ", "だ", EPOS::AuxTenseTa),    // 連濁形 (泳いだ, 死んだ, 飛んだ, 読んだ)
      aux("だら", "だ", EPOS::AuxTenseTa),

      // Conjecture/Volitional (推量・意志) - う/よう
      aux("う", "う", EPOS::AuxVolitional),
      aux("よう", "よう", EPOS::AuxVolitional),
      aux("む", "む", EPOS::AuxVolitional),  // Classical volitional/conjectural
      // 文語の意志助動詞「む」の撥音便。否定の「ん」と同形だが、
      // 読まんとする のように引用の と が後続する文脈で区別する。
      aux("ん", "ん", EPOS::AuxVolitional),
      aux("だろ", "だ", EPOS::AuxCopulaDa),        // mizenkei, connects to う
      aux("でしょ", "です", EPOS::AuxCopulaDesu),  // mizenkei, connects to う

      // Negative conjecture (否定推量): attaches to 終止形 (godan) / 未然形 (ichidan)
      aux("まい", "まい", EPOS::AuxNegativeMai),
      // The classical negative-volitional じ attaches to the irrealis form.
      // Unlike terminal-form まい, it therefore uses the classical-negative
      // class with the same irrealis connection behavior.
      aux("じ", "じ", EPOS::AuxNegativeNu),
      aux("まじき", "まじ", EPOS::AuxNegativeMai),  // 文語打消推量の連体形
      // 終止形 まじ (確認せまじ). It shares its surface with the colloquial
      // na-adjective, which is separated by the host: this auxiliary needs a
      // terminal or irrealis predicate in front of it, while the adjective
      // follows a nominal or opens the clause.
      aux("まじ", "まじ", EPOS::AuxNegativeMai),
      // 文語の禁止 なかれ (確認するなかれ). It follows a terminal form like まい
      // does, so it shares that class; without the entry the three morae are
      // rebuilt as a copula plus two one-mora fragments.
      aux("なかれ", "なかれ", EPOS::AuxNegativeMai),

      // Conjecture - らしい (推定)
      aux("らしい", "らしい", EPOS::AuxConjectureRashii),
      aux("らしく", "らしい", EPOS::AuxConjectureRashii),
      aux("らしかっ", "らしい", EPOS::AuxConjectureRashii),
      aux("らしから", "らしい", EPOS::AuxConjectureRashii),
      // Nominalizing stem: 本らしさ → 本 + らし + さ. It remains a form
      // of the conjecture auxiliary rather than an independent adjective.
      aux("らし", "らしい", EPOS::AuxConjectureRashii),

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
      quotative_adv("そう"),
      // Note: さそう removed - MeCab splits as な+さ+そう (3 tokens)

      // Obligation (当為)
      // Classical obligation auxiliary べし - connects after verb shuushikei
      // Note: べきだ/べきで/べきでは removed - MeCab splits as べき+だ/で/では
      aux("べき", "べし", EPOS::AuxClassicalBeshi),    // 連体形: 食べるべき, 来たるべき
      aux("べく", "べし", EPOS::AuxClassicalBeshi),    // 連用形: 注意すべく, しかるべく
      aux("べし", "べし", EPOS::AuxClassicalBeshi),    // 終止形: 見るべし, 恐るべし
      aux("べから", "べし", EPOS::AuxClassicalBeshi),  // 未然形: 読むべからず
      // Supplementary (カリ) conjugation. The 未然形 is the cell that hosts
      // classical negation, and the rest of the row hosts the auxiliaries that
      // need an inflecting host: 告げぬべかりし, 行くべかるべし, 惜しむべかれど.
      aux("べかり", "べし", EPOS::AuxClassicalBeshi),  // 連用形: 言ふべかりけり
      aux("べかる", "べし", EPOS::AuxClassicalBeshi),  // 連体形: 行くべかるべし
      aux("べかれ", "べし", EPOS::AuxClassicalBeshi),  // 已然形: 惜しむべかれど

      // Passive/Potential (受身・可能)
      aux("れ", "れる", EPOS::AuxPassive),
      aux("れる", "れる", EPOS::AuxPassive),
      aux("れれ", "れる", EPOS::AuxPassive),  // 仮定形 (書か+れれ+ば)
      aux("られ", "られる", EPOS::AuxPassive),
      aux("られる", "られる", EPOS::AuxPassive),
      aux("られれ", "られる", EPOS::AuxPassive),  // 仮定形 (食べ+られれ+ば)
      // Literary passive る. Only the cells that the modern れる paradigm does
      // not already spell are registered: the terminal る and the continuative れ
      // are shared, while these two are the ones a literary text needs
      // (繰り返さ+るる, 知ら+るれ+ば).
      aux("るる", "る", EPOS::AuxPassive),        // 連体形 (思はるる心)
      aux("るれ", "る", EPOS::AuxPassive),        // 已然形 (知らるれば)
      aux("れよ", "れる", EPOS::AuxPassive),      // 意志形語幹 (書か+れよ+う)
      aux("られよ", "られる", EPOS::AuxPassive),  // 意志形語幹 (食べ+られよ+う)

      // Potential auxiliary - 得る (える/うる)
      // Literary potential: し+え+ない (cannot do), し+える (can do)
      aux("え", "える", EPOS::AuxPotential),    // renyokei: 看過しえない
      aux("える", "える", EPOS::AuxPotential),  // shuushikei: 看過しえる
      aux("うる", "うる", EPOS::AuxPotential),  // alternative shuushikei: 看過しうる
      aux("得", "得る", EPOS::AuxPotential),    // kanji renyokei: 解決し得ない
      aux("得る", "得る", EPOS::AuxPotential),  // kanji shuushikei: 解決し得る

      // Modal subsidiary - かねる (inability or hesitation). Its stem and
      // terminal form follow a verb renyokei: 読みかねる, 言いかねます.
      aux("かね", "かねる", EPOS::AuxInability),
      aux("かねる", "かねる", EPOS::AuxInability),
      aux("たまえ", "たまう", EPOS::AuxHonorific),
      // Failure subsidiary - そびれる. Like かねる, it follows a verb
      // renyokei and expresses an unfulfilled action (読みそびれる).
      aux("そびれ", "そびれる", EPOS::AuxInability),
      aux("そびれる", "そびれる", EPOS::AuxInability),
      // Hesitation subsidiary - あぐねる. The stem attaches after a verb
      // renyokei and keeps the past auxiliary as a separate token.
      aux("あぐね", "あぐねる", EPOS::AuxInability),
      aux("あぐねる", "あぐねる", EPOS::AuxInability),
      // Hiragana spelling of the same closed-class failure auxiliary.
      aux("そこない", "そこなう", EPOS::AuxInability),
      aux("そこなう", "そこなう", EPOS::AuxInability),
      aux("そこなっ", "そこなう", EPOS::AuxInability),
      aux("そこなわ", "そこなう", EPOS::AuxInability),
      aux("そこなえ", "そこなう", EPOS::AuxInability),
      // Kanji spelling of the same closed-class failure subsidiary.
      verb("損なう", "損なう", EPOS::AuxInability),
      verb("損ない", "損なう", EPOS::AuxInability),
      verb("損なっ", "損なう", EPOS::AuxInability),
      verb("損なわ", "損なう", EPOS::AuxInability),
      verb("損なえ", "損なう", EPOS::AuxInability),

      // Suru verb stem forms (サ変動詞語幹活用形) - VERB, not AUX
      verb("し", "する", EPOS::VerbRenyokei),
      verb("す", "する", EPOS::VerbShuushikei),
      // Base form: closed-class irregular sahen. Its surface lives only in the
      // L2 dictionary, so without it here core.dic-disabled (vanilla) parsing
      // has no standalone する verb token and mis-splits 管理する → 管+理する.
      verb("する", "する", EPOS::VerbShuushikei),
      verb("さ", "する", EPOS::VerbMizenkei),
      verb("せ", "する", EPOS::VerbMizenkei),  // 認識せざるを得ない

      // The existential verb いる has an archaic ra-row mizenkei (いら),
      // used productively before negation and the classical causative す.
      verb("いら", "いる", EPOS::VerbMizenkei),

      // Classical conditional stem in しかれども.  It retains its verbal
      // analysis before the concessive particle rather than lexicalizing as a
      // conjunction.
      verb("しかれ", "しかる", EPOS::VerbKateikei),

      // Kuru verb stem form (カ変動詞語幹活用形) - VERB, not AUX
      // MeCab: 来た → 来(連用形) + た(過去)
      verb("来", "来る", EPOS::VerbRenyokei),
      // The kanji passive spelling keeps the established dictionary boundary
      // 来ら+れる. Kana uses the context-gated こ + られる candidate path,
      // because its one-mora stem would otherwise split ordinary hiragana.
      verb("来ら", "来る", EPOS::VerbMizenkei),
      // Likewise retain the established kanji causative boundary while the
      // canonical Kuru paradigm drives the corresponding kana path.
      verb("来さ", "来る", EPOS::VerbMizenkei),

      // Deru verb stem form (一段動詞「出る」) - VERB
      // で+たい/ます needs this to split correctly (外にでたい → 外|に|で|たい)
      verb("で", "出る", EPOS::VerbRenyokei),
      // Suru conjugation stems are separate search units before their auxiliaries.
      verb("しよ", "する", EPOS::VerbMizenkei),  // volitional base: しよ+う
      verb("すれ", "する", EPOS::VerbKateikei),  // conditional base: すれ+ば
      // Suru imperative: VERB (not AUX) - MeCab treats as 動詞
      verb("しろ", "する", EPOS::VerbMeireikei),
      verb("せよ", "する", EPOS::VerbMeireikei),
      aux("しそう", "する", EPOS::AuxAppearanceSou),

      // Causative (使役)
      aux("せ", "せる", EPOS::AuxCausative),
      aux("せる", "せる", EPOS::AuxCausative),
      aux("せれ", "せる", EPOS::AuxCausative),
      aux("せろ", "せる", EPOS::AuxCausative),  // imperative
      aux("せよ", "せる", EPOS::AuxCausative),  // imperative (literary)
      aux("し", "す", EPOS::AuxCausative),      // continuative: いらし+て
      aux("す", "す", EPOS::AuxCausative),      // terminal: いらす
      aux("させ", "させる", EPOS::AuxCausative),
      aux("させる", "させる", EPOS::AuxCausative),
      aux("させれ", "させる", EPOS::AuxCausative),
      aux("させろ", "させる", EPOS::AuxCausative),  // imperative
      aux("させよ", "させる", EPOS::AuxCausative),  // imperative (literary)
      // Classical causative しむ (下二段): attaches to the same 未然形 as せる.
      // Without it the irrealis kana is read as the modern causative
      // continuative (書かし + む) instead of 書か + しむ.
      aux("しむ", "しむ", EPOS::AuxCausative),    // 終止/連体形
      aux("しめ", "しむ", EPOS::AuxCausative),    // 未然/連用形
      aux("しむる", "しむ", EPOS::AuxCausative),  // 連体形
      aux("しむれ", "しむ", EPOS::AuxCausative),  // 已然形
      // The same auxiliary re-inflected as an Ichidan verb, which is the only
      // shape it still takes in modern prose (知らしめる, 書かしめれば). Without
      // these cells the terminal is read as the lexical verb it is homographic
      // with, and the irrealis host in front of it gets absorbed into a
      // fabricated compound.
      aux("しめる", "しめる", EPOS::AuxCausative),  // 終止/連体形
      aux("しめれ", "しめる", EPOS::AuxCausative),  // 仮定形

      // Desiderative - たい (願望)
      aux("たい", "たい", EPOS::AuxDesireTai),
      aux("たく", "たい", EPOS::AuxDesireTai),
      aux("たかっ", "たい", EPOS::AuxDesireTai),
      aux("たけれ", "たい", EPOS::AuxDesireTai),
      aux("たし", "たい", EPOS::AuxDesireTai),  // 文語終止形 (対応たし)
      // たがる (3rd-person desiderative): conjugates like a godan-ra verb
      aux("たがる", "たがる", EPOS::AuxDesireTai),  // 終止/連体
      aux("たがら", "たがる", EPOS::AuxDesireTai),  // 未然 (+ない)
      aux("たがろ", "たがる", EPOS::AuxDesireTai),  // 未然推量 (+う)
      aux("たがり", "たがる", EPOS::AuxDesireTai),  // 連用 (+ます)
      aux("たがっ", "たがる", EPOS::AuxDesireTai),  // 連用促音便 (+た/て)
      aux("たがれ", "たがる", EPOS::AuxDesireTai),  // 仮定 (+ば)
      // Classical desiderative まほし: verb renyokei + ま + ほしき.
      aux("ま", "まほし", EPOS::AuxDesireTai),
      adj("ほしき", "ほしい", EPOS::AdjBasic),
      // Classical honorific subsidiary たまふ: verb renyokei + た + ま + ふ.
      aux("ま", "たまふ", EPOS::AuxHonorific),
      aux("ふ", "たまふ", EPOS::AuxHonorific),
      // Classical terminal component after a kanji stem (候+ふ, 思+ふ).
      // It is context-gated by the tokenizer so it cannot become a free
      // one-mora lexical verb.
      verb("ふ", "ふる", EPOS::VerbShuushikei),
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

      // Desiderative adjective after a te-form: 読んでほしい.  Its stem
      // also connects to appearance そう (読んでほしそうだ), so retain the
      // ordinary i-adjective inflectional forms rather than fusing そう.
      adj("ほしい", "ほしい", EPOS::AdjBasic),
      adj("ほしく", "ほしい", EPOS::AdjRenyokei),
      adj("ほしかっ", "ほしい", EPOS::AdjKatt),
      adj("ほしけれ", "ほしい", EPOS::AdjKeForm),
      adj("ほしかろ", "ほしい", EPOS::AdjMizenkei),
      adj("ほし", "ほしい", EPOS::AdjStem),

      // Literary adjective meaning absence: ことなしに, 本なしに.
      adj("なし", "ない", EPOS::AdjBasic),

      // Difficulty suffix - づらい. This is an adjective that follows a
      // verb renyokei: 読みづらい, 書きづらい.  Keep its adverbial and
      // stem forms so づらくなる and づらさ use the same productive
      // adjective paradigm rather than OTHER or a fabricated verb.
      adj("づらい", "づらい", EPOS::AdjBasic),
      adj("づらく", "づらい", EPOS::AdjRenyokei),
      adj("づらかっ", "づらい", EPOS::AdjKatt),
      adj("づらけれ", "づらい", EPOS::AdjKeForm),
      adj("づらかろ", "づらい", EPOS::AdjMizenkei),
      adj("づら", "づらい", EPOS::AdjStem),

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

      // Honorific prefix 御 (御尽力, 御挨拶, 御協力 - kanji form, mostly literary/formal)
      // MeCab: 御尽力 → 御(接頭辞) + 尽力(名詞)
      prefix("御", "御"),

      // Note: Negation prefixes (未, 非, 不, 無) are NOT registered
      // MeCab splits them but Suzume keeps them unified for practical tokenization
      // e.g., 未確認 → 未確認 (not 未+確認)

      // Nominalization suffix さ (高さ, 美しさ, なさ)
      // MeCab: 高さ → 高(語幹) + さ(名詞), なさそう → な + さ + そう
      suffix("さ", "さ"),

      // Appearance suffix げ (悲しげ, 不安げ). It attaches productively to
      // i-adjective stems and nominal bases, retaining the suffix boundary.
      suffix("げ", "げ"),

      // Productive viewpoint/evaluation suffix (重要視, 問題視).  It remains
      // separate from the preceding noun and the following サ変 verb.
      suffix("視", "視"),

      // Construction/composition suffixes after a quantified counter
      // (二階建て, 二本立て). They retain a Suffix candidate alongside the
      // homographic verb stems, and contextual scoring selects the grammar.
      suffix("建て", "建て"),
      suffix("立て", "立て"),

      // Honorific suffixes
      suffix("さん", "さん"),
      suffix("ちゃん", "ちゃん"),
      suffix("くん", "くん"),
      suffix("さま", "さま"),
      suffix("たん", "たん"),
      suffix("にゃん", "にゃん"),
      suffix("っ娘", "っ娘"),

      // Plural suffix たち (学生たち, 私たち, 子供たち)
      // MeCab: 学生たち → 学生 + たち
      suffix("たち", "たち"),

      // Plural suffix ら (彼ら, 彼女ら, 僕ら, あいつら)
      // MeCab treats these as single tokens, but grammatically ら is a suffix
      suffix("ら", "ら"),

      // Reason/consequence suffix after a demonstrative (それゆえ, これゆえ).
      suffix("ゆえ", "ゆえ"),

      // Tendency suffix after a verb renyokei (読みがち, 食べがち).
      suffix_tendency("がち", "がち"),

      // Quantitative bound suffixes: 1kg未満, 5cm以上, 十件以下, 十件程度.
      suffix("未満", "未満"),
      suffix("以下", "以下"),
      suffix("程度", "程度"),
      suffix("間", "間"),
      suffix("余り", "余り"),
      suffix("まい", "まい"),
      suffix("にん", "にん"),
      suffix("向き", "向き"),
      // Productive nominal suffixes keep their search boundary before a
      // following copula or case particle (終了+後、記録+用). 後 is
      // context-gated to direct orthographic attachment by the tokenizer.
      suffix("後", "後"),
      suffix("用", "用"),
      // Naming suffix after a nominal base (ファイル+名、利用者+名).
      suffix("名", "名"),

      // Note: 的 was previously L1 SUFFIX, but Suzume's tokenizer use case
      // prefers X+的 as one search unit (論理的, 科学的, 経済的). Merging is
      // handled by kanji-merge normalization. 的+な (na-adj formation) still
      // splits as 論理的(NOUN) + な(AuxCopula) without a 的 SUFFIX node.

      // Inclusive suffix ごと (皮ごと, 頭ごと)
      // MeCab: 皮ごと → 皮 + ごと (noun + suffix)
      suffix("ごと", "ごと"),

      // Coverage suffix まみれ (血まみれ, 泥まみれ, 汗まみれ)
      // MeCab: 血まみれ → 血 + まみれ (noun + suffix)
      suffix("まみれ", "まみれ"),

      // Coverage suffix だらけ (傷だらけ, 間違いだらけ)
      // MeCab: 傷だらけ → 傷 + だらけ (noun + suffix)
      suffix("だらけ", "だらけ"),

      // Tendency suffix ぎみ — hiragana spelling of 気味 (風邪ぎみ, 緊張ぎみ, 疲れぎみ)
      // MeCab: 風邪ぎみ → 風邪 + ぎみ (noun + suffix)
      suffix("ぎみ", "ぎみ"),
      suffix("気味", "気味"),

      // Audience/direction suffix: 初心者向け, 家庭向け.
      suffix("向け", "向け"),

      // Manner/conformity suffix: 予定どおり, 指示どおり.
      suffix("どおり", "どおり"),

      // Manner suffix after a verb's renyokei: 読みぶり, 食べぶり.
      suffix("ぶり", "ぶり"),

      // Exclusion suffixes: 税抜き, 水ぬき.
      suffix("抜き", "抜き"),
      suffix("ぬき", "ぬき"),

      // Deverbal suffixes that describe the state or origin of a nominal host
      // (会社帰り, 条件付き, 写真入り). Their host class is open, so only the
      // suffix side can be named; registering it keeps the morpheme boundary
      // that the generic nominalizer would otherwise swallow. The homographic
      // verb continuatives (家に帰り、栓を抜き) stay available as verb candidates.
      suffix("帰り", "帰り"),
      suffix("付き", "付き"),
      suffix("入り", "入り"),

      // All-over suffix: 白ずくめ, 欠点ずくめ.  The voiced allomorph is
      // productive after nominal bases (確認づくめ).
      suffix("ずくめ", "ずくめ"),
      suffix("づくめ", "づくめ"),

      // Exhaustive-listing suffix, voiced allomorph only: ごちそうづくし, 名物づくし.
      suffix("づくし", "づくし"),

      // Intervening-medium suffix: 画面越し, 肩越し, 一年越し.  Bound to a nominal
      // host, so its trailing し is never a sahen continuative.
      suffix("越し", "越し"),
      suffix("ごし", "ごし"),

      // Route suffix, voiced allomorph only: 川づたい, 線路づたい.  The unvoiced
      // 伝い stands alone as a deverbal noun, so only the bound form is nameable.
      suffix("づたい", "づたい"),

      // Creation suffix, voiced allomorph only: 環境づくり, 街づくり, 体づくり.
      // The unvoiced 作り stands alone as a noun, so only the voiced form is
      // bound and nameable here.
      suffix("づくり", "づくり"),

      // Interval suffix: 一日おき, 一時間おき.
      suffix("おき", "おき"),

      // Immediacy / dependency suffix, kana orthography of 次第: 確認しだい,
      // 手当たりしだい. The kanji form is already held together by the kanji run
      // itself; the kana form has no such anchor and otherwise falls apart into
      // the suru continuative plus a fabricated だい.
      suffix("しだい", "しだい"),

      // Verb renyokei suffix っぱなし (出しっぱなし, 置きっぱなし)
      // MeCab: 出しっぱなし → 出し + っぱなし (verb renyokei + suffix)
      suffix("っぱなし", "っぱなし"),

      // Repetitive-action suffix まくり (走りまくり, 食べまくり).
      // It remains a searchable bound suffix after a verb continuative rather
      // than being reinterpreted as the classical auxiliary り.
      suffix("まくり", "まくり"),

      // Recent-completion suffix たて (焼きたて, 作りたて)
      // MeCab: 焼きたて → 焼き + たて (verb renyokei + suffix)
      suffix_recent_completion("たて", "たて"),

      // Completion-state suffix (確認済み, 承認済み).
      suffix_recent_completion("済み", "済み"),

      // Adjective suffixes - connect after verb renyokei (V連用形接続)
      // MeCab: 使いにくい → 使い + にくい, 読みやすい → 読み + やすい
      adj("にくい", "にくい", EPOS::AdjBasic),
      adj("にくく", "にくい", EPOS::AdjRenyokei),
      adj("にくかっ", "にくい", EPOS::AdjKatt),
      adj("にくけれ", "にくい", EPOS::AdjKeForm),
      adj("にくかろ", "にくい", EPOS::AdjMizenkei),
      adj("やすい", "やすい", EPOS::AdjBasic),
      adj("やすく", "やすい", EPOS::AdjRenyokei),
      adj("やすかっ", "やすい", EPOS::AdjKatt),
      adj("やすけれ", "やすい", EPOS::AdjKeForm),
      adj("やすかろ", "やすい", EPOS::AdjMizenkei),
      adj("がたい", "がたい", EPOS::AdjBasic),
      adj("がたく", "がたい", EPOS::AdjRenyokei),
      adj("がたかっ", "がたい", EPOS::AdjKatt),
      adj("がたけれ", "がたい", EPOS::AdjKeForm),
      adj("がたかろ", "がたい", EPOS::AdjMizenkei),
      // Stem form (語幹/ガル接続) for さ-nominalization, mirroring よ/な stems:
      // MeCab: 使いやすさ → 使い + やす(語幹) + さ. Only やす needs this — にく already
      // has a NOUN reading (肉/にく) in the dictionary that carries 読みにくさ, whereas
      // no やす noun exists, so 読みやすさ would otherwise fragment into や+す+さ.
      adj("やす", "やすい", EPOS::AdjStem),
      // Productive difficulty adjective stem (読みにくさ, 分かりにくい). Its
      // derivational reading remains available beside the lexical noun 肉.
      adj("にく", "にくい", EPOS::AdjStem),
      adj("がた", "がたい", EPOS::AdjStem),

      // Adjective suffix っぽい (～っぽい: 子供っぽい, 忘れっぽい)
      // MeCab: 子供っぽい → 子供 + っぽい
      adj("っぽい", "っぽい", EPOS::AdjBasic),
      adj("っぽく", "っぽい", EPOS::AdjRenyokei),
      adj("っぽかっ", "っぽい", EPOS::AdjKatt),
      adj("っぽけれ", "っぽい", EPOS::AdjKeForm),
      adj("っぽかろ", "っぽい", EPOS::AdjMizenkei),
      adj("っぽ", "っぽい", EPOS::AdjStem),

      // Polite imperative - connect after verb renyokei
      aux("なさい", "なさる", EPOS::AuxHonorific),
      // Honorific subsidiary なさる after お+連用形.  Keep its special
      // ra-row inflection as auxiliaries so お読みなさる and its negative,
      // past, and conditional forms do not fall back to lexical verbs.
      aux("なさる", "なさる", EPOS::AuxHonorific),
      aux("なさら", "なさる", EPOS::AuxHonorific),
      aux("なさっ", "なさる", EPOS::AuxHonorific),
      aux("なされ", "なさる", EPOS::AuxHonorific),
      aux("なさろ", "なさる", EPOS::AuxHonorific),

      // Honorific subsidiary いらっしゃる has the same special ra-row
      // inflection. Keep the whole paradigm after a te-form so its initial
      // い is not detached as the progressive auxiliary.
      aux("いらっしゃる", "いらっしゃる", EPOS::AuxHonorific),
      aux("いらっしゃい", "いらっしゃる", EPOS::AuxHonorific),
      aux("いらっしゃら", "いらっしゃる", EPOS::AuxHonorific),
      aux("いらっしゃっ", "いらっしゃる", EPOS::AuxHonorific),
      aux("いらっしゃれ", "いらっしゃる", EPOS::AuxHonorific),
      aux("いらっしゃろ", "いらっしゃる", EPOS::AuxHonorific),

      // Request form of the benefactive くれる, which takes a te-form
      // (読んで+おくれ). Only the imperative cell is registered: the rest of
      // the paradigm is homographic with 遅れる.
      aux("おくれ", "おくれる", EPOS::AuxBenefactive),
      // Tohoku request form of the same benefactive (書いて+けろ). Its first
      // mora spells the final particle け, so it needs the te-form connection
      // the benefactives already have.
      aux("けろ", "けろ", EPOS::AuxBenefactive),

      // Kansai honorific subsidiary はる. It conjugates as a godan-ra verb and
      // attaches to the irrealis of a godan predicate (読ま+はる) or to a
      // te-form (飲んで+はる). Only the terminal cell is registered: the
      // irrealis and continuative cells spell common hiragana nouns (はら, はり)
      // that carry no competing dictionary edge, so they won the sentence-
      // initial position outright.
      aux("はる", "はる", EPOS::AuxHonorific),

      // Possibility/uncertainty: かも + しれ + ない.
      // かも particle is already defined above (line 157)
      verb("しれ", "しれる", EPOS::VerbRenyokei),

      // Certainty expression: nominal unit followed by ない.
      // These are handled by noun + ない connection

      // Note: れる/られる/せる/させる (shuushikei) are registered above with the
      // Passive/Causative groups; no duplicate generic registration needed here.

      // Polite existence - ございます (丁重)
      // MeCab splits: ござい + ます (renyokei + polite)
      aux("ござい", "ござる", EPOS::AuxGozaru),
      // The classical negative retains the same dependent copular role
      // (で+ござら+ぬ).
      aux("ござら", "ござる", EPOS::AuxGozaru),
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
      verb("いただい", "いただく", EPOS::VerbOnbinkei),
      // Potential form of the humble receiving verb. Dictionary candidates
      // retain a verbal shape; postprocessing assigns the benefactive
      // auxiliary role after a te-form or honorific renyokei.
      verb("いただけ", "いただける", EPOS::VerbKateikei),
      verb("いただける", "いただける", EPOS::VerbShuushikei),
      verb("いただけれ", "いただける", EPOS::VerbKateikei),

      // Potential form of the receiving benefactive. After a te-form this
      // remains a closed subsidiary paradigm, including もらえ+ない.
      aux("もらえ", "もらえる", EPOS::AuxBenefactive),
      aux("もらえる", "もらえる", EPOS::AuxBenefactive),
      aux("もらえれ", "もらえる", EPOS::AuxBenefactive),

      // Request - ください is VERB (くださる) in MeCab
      // くださる is special ra-row godan with irregular imperative form ください
      // Uses VerbRenyokei to allow connection to ます (くださいました)
      verb("ください", "くださる", EPOS::VerbRenyokei),
      verb("下さい", "下さる", EPOS::VerbRenyokei),

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
      // Register い separately so aspect and following tense/conjunction remain distinct.
      aux("い", "いる", EPOS::AuxAspectIru),  // renyokei for い+た, い+ます
      aux("いる", "いる", EPOS::AuxAspectIru),
      aux("います", "いる", EPOS::AuxAspectIru),
      aux("いません", "いる", EPOS::AuxAspectIru),
      aux("いない", "いる", EPOS::AuxAspectIru),
      aux("いなかった", "いる", EPOS::AuxAspectIru),
      aux("いれ", "いる", EPOS::AuxAspectIru),  // katei-kei before ば
      aux("いれば", "いる", EPOS::AuxAspectIru),

      // Progressive/Continuous - おる (humble/dialectal form of いる)
      // Used in formal polite speech: ております, おります
      // Add renyokei forms separately from following politeness auxiliaries.
      aux("おる", "おる", EPOS::AuxAspectIru),
      aux("おり", "おる", EPOS::AuxAspectIru),  // renyokei for おり+ます
      // Western-Japanese contractions of ておる / でおる. These retain the
      // progressive auxiliary's Godan-ra inflection after a verb stem or
      // onbin form (食べとる, 書いとった, 読んどらん).
      aux("とる", "とる", EPOS::AuxAspectIru),
      aux("とら", "とる", EPOS::AuxAspectIru),
      aux("とり", "とる", EPOS::AuxAspectIru),
      aux("とっ", "とる", EPOS::AuxAspectIru),
      verb("とれ", "とる", EPOS::VerbKateikei),
      aux("どる", "どる", EPOS::AuxAspectIru),
      aux("どら", "どる", EPOS::AuxAspectIru),
      aux("どり", "どる", EPOS::AuxAspectIru),
      aux("どっ", "どる", EPOS::AuxAspectIru),
      verb("どれ", "どる", EPOS::VerbKateikei),
      // The Shikoku progressive よる and its voiced form after an n-onbin
      // (書きよる, 読んじょる) inflect the same way. Without the auxiliary
      // reading the sequence is taken for a lexical verb and the preceding
      // noun loses its boundary.
      aux("よる", "よる", EPOS::AuxAspectIru),
      aux("よっ", "よる", EPOS::AuxAspectIru),
      aux("じょる", "じょる", EPOS::AuxAspectIru),
      aux("じょっ", "じょる", EPOS::AuxAspectIru),

      // Benefactive auxiliary - くれる (giving, receiving benefit)
      // Used in subsidiary verb patterns: してくれる, 買ってくれた
      // くれる is a dependent verb in benefactive constructions.
      verb("くれる", "くれる", EPOS::VerbShuushikei),
      verb("くれ", "くれる", EPOS::VerbRenyokei),

      // Humble continuative まいる after a te-form is a subsidiary auxiliary.
      aux("まいり", "まいる", EPOS::AuxHonorific),

      // Excessive degree subsidiary verb - すぎる (過度)
      // Used after adjective/verb stems: 高すぎる, 食べすぎる
      // MeCab: 動詞,非自立 (subsidiary verb, not auxiliary 助動詞)
      // MeCab splits: 高 + すぎる (終止形), 高 + すぎ + た (連用形 + た)
      // Use verb() to get POS::Verb, but keep AuxExcessive EPOS for bigram rules
      verb("すぎる", "すぎる", EPOS::AuxExcessive),
      verb("すぎ", "すぎる", EPOS::AuxExcessive),  // renyokei for すぎ+た, すぎ+て
      verb("過ぎる", "過ぎる", EPOS::AuxExcessive),
      verb("過ぎ", "過ぎる", EPOS::AuxExcessive),

      // Inceptive subsidiary verb: 読みはじめる, 食べはじめる.
      aux("はじめる", "はじめる", EPOS::AuxAspectHajimeru),
      aux("はじめ", "はじめる", EPOS::AuxAspectHajimeru),
      // The kanji spelling is the same closed inceptive use. Keep POS=Verb to
      // preserve the lexical-verb surface category while ExtendedPOS carries
      // the dependent, renyokei-selecting grammar used by the scorer.
      verb("始める", "始める", EPOS::AuxAspectHajimeru),
      verb("始め", "始める", EPOS::AuxAspectHajimeru),
      aux("そこね", "そこねる", EPOS::AuxInability),

      // Completive subsidiary verb: 読み尽くす, 食べ尽くした.  This is
      // a closed aspectual use after a verb continuative; the lexical verb
      // use remains available through the general verb candidate generator.
      aux("尽くす", "尽くす", EPOS::AuxAspectShimau),
      aux("尽くさ", "尽くす", EPOS::AuxAspectShimau),
      aux("尽くし", "尽くす", EPOS::AuxAspectShimau),
      aux("尽くせ", "尽くす", EPOS::AuxAspectShimau),
      aux("尽くそ", "尽くす", EPOS::AuxAspectShimau),
      // Keep the lexical Godan-sa readings alongside the subsidiary entries.
      // They are needed before an adjective (筆舌に尽くし難い) and when
      // 尽くす has an ordinary case-marked object.
      verb("尽くす", "尽くす", EPOS::VerbShuushikei),
      verb("尽くさ", "尽くす", EPOS::VerbMizenkei),
      verb("尽くし", "尽くす", EPOS::VerbRenyokei),
      verb("尽くせ", "尽くす", EPOS::VerbKateikei),
      verb("尽くそ", "尽くす", EPOS::VerbMizenkei),

      // Adjective-stem suffix verb - がる (ガル接続)
      // Used after adjective stems: 怖がる, 嫌がる, 可愛がる
      // MeCab: 動詞,接尾 (suffix verb)
      // Godan-ra conjugation: がる, がら, がり, がっ, がれ, がろ
      verb("がる", "がる", EPOS::AuxGaru),
      verb("がら", "がる", EPOS::AuxGaru),  // mizenkei
      verb("がり", "がる", EPOS::AuxGaru),  // renyokei
      verb("がっ", "がる", EPOS::AuxGaru),  // onbinkei (がった, がって)
      verb("がれ", "がる", EPOS::AuxGaru),  // kateikei/meireikei
      verb("がろ", "がる", EPOS::AuxGaru),  // ishikei (がろう)

      // Completive/Regretful - しまう (完了・遺憾)
      // Aspectual しまう is an auxiliary rather than the lexical verb.
      aux("しまう", "しまう", EPOS::AuxAspectShimau),
      aux("しまっ", "しまう", EPOS::AuxAspectShimau),  // te-form/ta-form stem
      aux("しまい", "しまう", EPOS::AuxAspectShimau),  // negative stem
      aux("しまわ", "しまう", EPOS::AuxAspectShimau),  // irrealis before negative auxiliary
      aux("もう", "しまう", EPOS::AuxAspectShimau),    // western contraction: てしまう → てもうた
      // 仕舞う is the standard kanji spelling of the same closed-class
      // completive auxiliary. Register its full Godan-wa paradigm so all
      // following inflections retain the auxiliary boundary after a te-form.
      aux("仕舞う", "しまう", EPOS::AuxAspectShimau),
      aux("仕舞わ", "しまう", EPOS::AuxAspectShimau),
      aux("仕舞い", "しまう", EPOS::AuxAspectShimau),
      aux("仕舞っ", "しまう", EPOS::AuxAspectShimau),
      aux("仕舞え", "しまう", EPOS::AuxAspectShimau),
      aux("仕舞お", "しまう", EPOS::AuxAspectShimau),
      // Keep the lexical-verb readings alongside the auxiliary readings. The
      // te-form connection selects the latter, while a standalone transitive
      // use such as 物を仕舞う remains a verb.
      verb("仕舞う", "仕舞う", EPOS::VerbShuushikei),
      verb("仕舞わ", "仕舞う", EPOS::VerbMizenkei),
      verb("仕舞い", "仕舞う", EPOS::VerbRenyokei),
      verb("仕舞っ", "仕舞う", EPOS::VerbOnbinkei),
      verb("仕舞え", "仕舞う", EPOS::VerbKateikei),
      verb("仕舞お", "仕舞う", EPOS::VerbMizenkei),

      // Contracted forms: ちゃう/じゃう (completion). Both are closed-class
      // contractions of て/で+しまう and remain aspect auxiliaries throughout
      // their Godan-wa inflection.
      aux("ちゃう", "ちゃう", EPOS::AuxAspectShimau),
      aux("ちゃわ", "ちゃう", EPOS::AuxAspectShimau),
      aux("ちゃい", "ちゃう", EPOS::AuxAspectShimau),
      aux("ちゃっ", "ちゃう", EPOS::AuxAspectShimau),
      aux("ちゃえ", "ちゃう", EPOS::AuxAspectShimau),
      aux("ちゃお", "ちゃう", EPOS::AuxAspectShimau),
      // じゃう is the voiced contraction after an n-onbin (読んじゃう).
      aux("じゃう", "じゃう", EPOS::AuxAspectShimau),
      aux("じゃわ", "じゃう", EPOS::AuxAspectShimau),
      aux("じゃい", "じゃう", EPOS::AuxAspectShimau),
      aux("じゃっ", "じゃう", EPOS::AuxAspectShimau),
      aux("じゃえ", "じゃう", EPOS::AuxAspectShimau),
      aux("じゃお", "じゃう", EPOS::AuxAspectShimau),

      // Contracted forms: てる/とく (progressive/preparation)
      // MeCab: 動詞,非自立 → Auxiliary (subsidiary verbs)
      aux("てる", "てる", EPOS::AuxAspectIru),
      aux("て", "てる", EPOS::AuxAspectIru),
      // Voiced contraction after an n-onbin: 読んでる = 読んでいる.
      // Its selection is restricted by the connection scorer so lexical 出る
      // remains available outside that grammatical environment.
      aux("でる", "いる", EPOS::AuxAspectIru),
      // で remains excluded: 出たい must be で(出る連用形)+たい.
      aux("とく", "とく", EPOS::AuxAspectOku),
      aux("どく", "どく", EPOS::AuxAspectOku),
      aux("おい", "おく", EPOS::AuxAspectOku),  // renyokei after て
      // MeCab compat: とい/どい (renyokei) + た/て instead of といた/どいた
      aux("とい", "とく", EPOS::AuxAspectOku),
      aux("どい", "どく", EPOS::AuxAspectOku),
      // Godan-ka kateikei, volitional, and colloquial conditional
      // (書い+とけ+ば, 書い+とこ+う, 書い+ときゃ). The volitional cells are
      // retained as auxiliaries here; their selection is licensed by the
      // preceding onbin and following volitional auxiliary, so homographic
      // lexical readings remain available elsewhere.
      aux("とこ", "とく", EPOS::AuxAspectOku),
      aux("どこ", "どく", EPOS::AuxAspectOku),
      // Godan-ka mizenkei, which the negative and the contracted obligation
      // chains select (書い+とか+ない, 飲ん+どか+なきゃ+いけ+ない). とか also
      // spells the adverbial particle, so its subsidiary reading is admitted
      // only after a te-form onbin by the connection scorer.
      aux("とか", "とく", EPOS::AuxAspectOku),
      aux("どか", "どく", EPOS::AuxAspectOku),
      aux("とけ", "とく", EPOS::AuxAspectOku),
      aux("どけ", "どく", EPOS::AuxAspectOku),
      aux("ときゃ", "とく", EPOS::AuxAspectOku),
      aux("どきゃ", "どく", EPOS::AuxAspectOku),
      // Plain godan-ka renyokei, which the polite auxiliary selects
      // (終わらせ+とき+ました). Its onbin sibling とい covers the te-form and the
      // past; without this cell the same paradigm breaks apart in front of ます.
      aux("とき", "とく", EPOS::AuxAspectOku),
      aux("どき", "どく", EPOS::AuxAspectOku),

      // Directional auxiliaries - いく/くる (方向補助動詞)
      // MeCab tags as 動詞 (Verb), not 助動詞, even in subsidiary use
      // Note: いっ (sokuonbin) is generated by hiragana verb candidate generator
      // with context-sensitive lemma (と+いっ→いう, て+いっ→いく); no L1 entry needed
      verb("いく", "いく", EPOS::VerbShuushikei),
      verb("いか", "いく", EPOS::VerbMizenkei),
      aux("いかない", "いく", EPOS::AuxAspectIku),
      // Literary form ゆく (classical 行く)
      verb("ゆく", "ゆく", EPOS::VerbShuushikei),
      verb("ゆき", "ゆく", EPOS::VerbRenyokei),
      verb("ゆか", "ゆく", EPOS::VerbMizenkei),
      verb("ゆけ", "ゆく", EPOS::VerbMeireikei),
      // Classical honorific おはす.  The independent verbal component keeps
      // the productive prefix boundary お + はす.
      verb("はす", "はする", EPOS::VerbShuushikei),
      aux("くる", "くる", EPOS::AuxAspectKuru),
      // The one-mora renyokei き is generated contextually.  A global entry
      // reopens ordinary words ending in き (でき, 抜き, 咲き).
      aux("く", "", EPOS::AuxAspectIku),
      // Note: no unconditional こ (来る mizenkei) entry — the surface is far too
      // frequent as a word fragment (こと, これ, きのこ, ...). こ is generated
      // context-gated before a ない-family negative in
      // generateHiraganaVerbCandidates (こない → こ + ない).

      // Explanatory (説明) - MeCab compat: split as の/ん + だ/です/でした
      // Removed のだ/のです/のでした/んだ/んです/んでした to allow split

      // Kuruwa-kotoba (廓言葉)
      aux("ありんす", "ある", EPOS::Unknown),
      aux("ありんした", "ある", EPOS::Unknown),
      aux("ありんせん", "ある", EPOS::Unknown),
      aux("ざんす", "ある", EPOS::Unknown),
      aux("ざんせん", "ある", EPOS::Unknown),
      aux("でありんす", "だ", EPOS::Unknown),
      aux("でありんした", "だ", EPOS::Unknown),
      aux("なんし", "ます", EPOS::AuxKuruwaPolite),
      aux("なんした", "ます", EPOS::AuxKuruwaPolite),

      // Cat-like (猫系) - sentence-final particles (な/ね/よ variants)
      particle("にゃ", EPOS::ParticleFinal),
      particle("にゃん", EPOS::ParticleFinal),
      particle("にゃー", EPOS::ParticleFinal),
      aux("だにゃ", "だよ", EPOS::Unknown),
      aux("だにゃん", "だよ", EPOS::Unknown),
      aux("ですにゃ", "ですよ", EPOS::Unknown),
      aux("ですにゃん", "ですよ", EPOS::Unknown),

      // Character-style sentence-final particles (MeCab: noun).
      // Keep the copular particle boundary before this final-particle form.
      particle("ゲソ", EPOS::ParticleFinal),
      particle("げそ", EPOS::ParticleFinal),

      // Ojou-sama/Lady speech (お嬢様言葉)
      aux("ですわ", "です", EPOS::Unknown),
      aux("ですの", "です", EPOS::Unknown),
      aux("ますの", "ます", EPOS::Unknown),

      // Youth slang (若者言葉) - っす/っすか are colloquial です, so tag them as the
      // polite copula rather than falling back to the Auxiliary default (AuxTenseTa),
      // which would wrongly reward a verb 音便形 + っす reading (つい+っす) over the
      // intended stem + っす split (きつい+っす).
      aux("っす", "です", EPOS::AuxCopulaDesu),
      aux("っした", "でした", EPOS::AuxCopulaDesu),
      aux("っすか", "ですか", EPOS::AuxCopulaDesu),

      // Rabbit-like (兎系)
      aux("ぴょん", "だ", EPOS::Unknown),
      aux("ピョン", "だ", EPOS::Unknown),

      // Ninja/Old-fashioned (忍者・古風)
      aux("ござる", "だ", EPOS::Unknown),
      aux("でござる", "だ", EPOS::Unknown),
      aux("ござった", "だった", EPOS::Unknown),
      aux("でござった", "だった", EPOS::Unknown),
      aux("でございます", "です", EPOS::Unknown),
      aux("ナリ", "だ", EPOS::Unknown),
      aux("なり", "だ", EPOS::Unknown),
      aux("でナリ", "だ", EPOS::Unknown),
      aux("でなり", "だ", EPOS::Unknown),

      // Elderly/Archaic (老人・古風)
      aux("じゃ", "だ", EPOS::AuxCopulaDa),
      aux("じゃあ", "だ", EPOS::AuxCopulaDa),
      // Contracted explanatory/copular negative: んじゃ+ない.
      aux("んじゃ", "んだ", EPOS::AuxCopulaDa),
      aux("のじゃ", "のだ", EPOS::Unknown),
      aux("じゃろ", "だろ", EPOS::AuxCopulaDa),

      // Regional dialects (方言系). These are copular forms, so they take the
      // copula's ExtendedPOS: Unknown left them outside every connection rule,
      // and a fabricated i-adjective swallowed the whole clause instead.
      // The predicate-final tails ばい/やんけ are final particles and live in
      // the particle table. ぜよ stays unclassified: ぜ+よ is already the
      // grammatical reading of that sequence.
      aux("ぜよ", "だ", EPOS::Unknown),
      aux("だべ", "だ", EPOS::AuxCopulaDa),
      // Kyoto polite copula, the regional counterpart of です: it takes the
      // same continuative before the past auxiliary (紙どし+た).
      aux("どす", "どす", EPOS::AuxCopulaDesu),
      aux("どし", "どす", EPOS::AuxCopulaDesu),
      // Kansai negative, attaching to a verb irrealis like ない (書か+へん).
      // Its surface also spells the case particle へ plus ん, so it depends on
      // the irrealis connection the negative auxiliary already has. The Tohoku
      // ね is deliberately absent: a single mora shared with the final
      // particle has no such gate and displaced ね across standard usage.
      aux("へん", "へん", EPOS::AuxNegativeNai),

      // Kansai obligation/prohibition predicate, the regional counterpart of
      // いけない. It closes the chain after the contracted negative な
      // (書か+な+あかん) or a conditional (書いたら+あかん).
      aux("あかん", "あかん", EPOS::AuxNegativeNai),
      // The bare Kansai copula stays unclassified: it is homographic with the
      // far commoner coordinating particle (本や紙), which the copula's own
      // connection bonuses would outrank.
      aux("や", "だ", EPOS::Unknown),
      aux("やねん", "だ", EPOS::AuxCopulaDa),
      // Conjectural and confirmative cells of the same Kansai copula
      // (書く+やろ, 読む+やん). やろ is homographic with the volitional stem of
      // やる, which keeps its verb reading before う (紙をやろう).
      aux("やろ", "や", EPOS::AuxCopulaDa),
      // やん is a fused confirmative rather than a cell of や, so it keeps its
      // own dictionary form.
      aux("やん", "やん", EPOS::AuxCopulaDa),
      aux("だっちゃ", "だ", EPOS::AuxCopulaDa),

      // Robot/Mechanical (ロボット・機械)
      aux("デス", "です", EPOS::Unknown),
      aux("マス", "ます", EPOS::Unknown),
  };
  return makeEntrySpecRange(kEntries);
}

}  // namespace suzume::dictionary::entries
