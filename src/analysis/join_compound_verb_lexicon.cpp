/**
 * @file join_compound_verb_lexicon.cpp
 * @brief Closed V2 lexicon for compound-verb candidate generation
 */

#include "join_compound_verb_internal.h"

namespace suzume::analysis::compound_verb_detail {

// List of V2 verbs that can form compound verbs
// Renyokei forms are generated automatically from base forms
// This is intentionally a closed lexical allowlist, not a generic "any verb can
// be V2" rule. Japanese compound-verb productivity is high, but unrestricted
// generation over-splits ordinary kanji+verb sequences and creates many false
// positives. Keep this table synchronized with tokenization tests when adding
// new V2 verbs.
// 始める・過ぎる・終わる／終える are aspectual auxiliaries and remain
// separate search units. 続ける is intentionally retained because productive
// V1+続ける compounds form the search unit represented by this table.
//
// Admission criterion for a new entry, so the table grows by evidence rather
// than one word at a time:
//   1. The verb heads at least two established compounds with distinct V1s.
//      Following a continuative once is a sentence, not a V2 slot.
//   2. Neither its surface nor its reading is homographic, in that position,
//      with a productive auxiliary or particle chain. When only one spelling
//      collides, restrict it with the joins_* flags instead of omitting it.
//   3. Its base form is an ordinary verb, not a bound suffix.
// A verb that fails (1) is a lexicalized whole word and belongs in L2 as one
// entry (取り壊す), not here.
extern const SubsidiaryVerb kSubsidiaryVerbs[kSubsidiaryVerbCount] = {
    // Godan verbs (五段)
    {"込む", "こむ", "む", V2VerbType::Godan},        // 読み込む, 飛びこむ
    {"出す", "だす", "す", V2VerbType::Godan},        // 呼び出す, 走りだす
    {"続く", "つづく", "く", V2VerbType::Godan},      // 引き続く
    {"返す", "かえす", "す", V2VerbType::Godan},      // 繰り返す, 繰りかえす
    {"戻す", "もどす", "す", V2VerbType::Godan},      // 取り戻す, 取りもどす
    {"返る", "かえる", "る", V2VerbType::Godan},      // 振り返る, 振りかえる
    {"帰る", "かえる", "る", V2VerbType::Godan},      // 持ち帰る
    {"変わる", "かわる", "る", V2VerbType::Godan},    // 移り変わる, 生まれ変わる
    {"替わる", "かわる", "る", V2VerbType::Godan},    // 入れ替わる, 切り替わる
    {"つかる", nullptr, "る", V2VerbType::Godan},     // 見つかる
    {"合う", "あう", "う", V2VerbType::Godan},        // 話し合う, 話しあう
    {"扱う", "あつかう", "う", V2VerbType::Godan},    // 取り扱う
    {"運ぶ", "はこぶ", "ぶ", V2VerbType::Godan},      // 持ち運ぶ
    {"過ごす", "すごす", "す", V2VerbType::Godan},    // 見過ごす
    {"消す", "けす", "す", V2VerbType::Godan},        // 取り消す
    {"直す", "なおす", "す", V2VerbType::Godan},      // やり直す, やりなおす
    {"切る", "きる", "る", V2VerbType::Godan},        // 締め切る, 締めきる
    {"上がる", "あがる", "る", V2VerbType::Godan},    // 立ち上がる, 盛り上がる
    {"下がる", "さがる", "る", V2VerbType::Godan},    // 立ち下がる
    {"回す", "まわす", "す", V2VerbType::Godan},      // 振り回す, 持ち回す
    {"回る", "まわる", "る", V2VerbType::Godan},      // 持ち回る, 振り回る
    {"抜く", "ぬく", "く", V2VerbType::Godan},        // 追い抜く, 突き抜く
    {"掛かる", "かかる", "る", V2VerbType::Godan},    // 取り掛かる
    {"付く", "つく", "く", V2VerbType::Godan},        // 思い付く, 気付く
    {"当たる", "あたる", "る", V2VerbType::Godan},    // 見当たる, 行き当たる
    {"巡る", "めぐる", "る", V2VerbType::Godan},      // 駆け巡る, 飛び巡る
    {"飛ばす", "とばす", "す", V2VerbType::Godan},    // 吹き飛ばす, 弾き飛ばす
    {"交う", "かう", "う", V2VerbType::Godan},        // 飛び交う, 行き交う
    {"潰す", "つぶす", "す", V2VerbType::Godan},      // 押し潰す, 叩き潰す
    {"崩す", "くずす", "す", V2VerbType::Godan},      // 切り崩す, 打ち崩す
    {"倒す", "たおす", "す", V2VerbType::Godan},      // 打ち倒す, 蹴り倒す
    {"壊す", "こわす", "す", V2VerbType::Godan},      // 打ち壊す, ぶち壊す
    {"砕く", "くだく", "く", V2VerbType::Godan},      // 打ち砕く, 噛み砕く
    {"盛る", "さかる", "る", V2VerbType::Godan},      // 燃え盛る, 咲き盛る
    {"起こす", "おこす", "す", V2VerbType::Godan},    // 引き起こす, 呼び起こす
    {"去る", "さる", "る", V2VerbType::Godan},        // 立ち去る, 走り去る
    {"開く", "ひらく", "く", V2VerbType::Godan},      // 切り開く, 押し開く
    {"組む", "くむ", "む", V2VerbType::Godan},        // 取り組む, 組み組む
    {"上る", "のぼる", "る", V2VerbType::Godan},      // 立ち上る, 這い上る
    {"こもる", "こもる", "る", V2VerbType::Godan},    // 閉じこもる, 立てこもる, 引きこもる
    {"籠る", nullptr, "る", V2VerbType::Godan},       // 閉じ籠る, 立て籠る
    {"籠もる", nullptr, "る", V2VerbType::Godan},     // 閉じ籠もる, 立て籠もる
    {"鳴らす", "ならす", "す", V2VerbType::Godan},    // 打ち鳴らす
    {"惜しむ", "おしむ", "む", V2VerbType::Godan},    // 出し惜しむ
    {"違う", "ちがう", "う", V2VerbType::Godan},      // 行き違う
    {"外す", "はずす", "す", V2VerbType::Godan},      // 踏み外す, 取り外す
    {"計らう", "はからう", "う", V2VerbType::Godan},  // 見計らう, 取り計らう
    {"悩む", "なやむ", "む", V2VerbType::Godan},      // 思い悩む
    {"知る", "しる", "る", V2VerbType::Godan},        // 思い知る
    // し立つ is not a compound; the kana reading otherwise swallows the
    // concessive particle たって behind a Sahen continuative (そうし+たって).
    {"立つ", "たつ", "つ", V2VerbType::Godan, false},               // 思い立つ
    {"通す", "とおす", "す", V2VerbType::Godan},                    // 押し通す
    {"持つ", "もつ", "つ", V2VerbType::Godan},                      // 受け持つ
    {"流す", "ながす", "す", V2VerbType::Godan},                    // 受け流す, 聞き流す
    {"記す", "しるす", "す", V2VerbType::Godan},                    // 書き記す
    {"巻く", "まく", "く", V2VerbType::Godan},                      // 取り巻く
    {"会う", "あう", "う", V2VerbType::Godan},                      // 立ち会う
    {"寄る", "よる", "る", V2VerbType::Godan},                      // 立ち寄る
    {"迫る", "せまる", "る", V2VerbType::Godan},                    // 差し迫る, 押し迫る
    {"延ばす", "のばす", "す", V2VerbType::Godan},                  // 引き延ばす
    {"離す", "はなす", "す", V2VerbType::Godan},                    // 引き離す, 切り離す
    {"渡す", "わたす", "す", V2VerbType::Godan},                    // 言い渡す
    {"表す", "あらわす", "す", V2VerbType::Godan},                  // 言い表す
    {"残す", "のこす", "す", V2VerbType::Godan},                    // 言い残す, 書き残す
    {"解く", "とく", "く", V2VerbType::Godan, false, true, false},  // 読み解く
    {"募る", "つのる", "る", V2VerbType::Godan},                    // 言い募る
    {"ふける", "ふける", "る", V2VerbType::Godan},                  // 読みふける
    {"敷く", "しく", "く", V2VerbType::Godan},                      // 組み敷く
    {"払う", "はらう", "う", V2VerbType::Godan},                    // 追い払う
    {"失う", "うしなう", "う", V2VerbType::Godan},                  // 見失う
    {"破る", "やぶる", "る", V2VerbType::Godan},                    // 見破る
    {"下ろす", "おろす", "す", V2VerbType::Godan},                  // 見下ろす, 書き下ろす
    {"送る", "おくる", "る", V2VerbType::Godan},                    // 見送る
    {"放す", "はなす", "す", V2VerbType::Godan},                    // 見放す
    {"及ぶ", "およぶ", "ぶ", V2VerbType::Godan},                    // 聞き及ぶ
    {"かじる", "かじる", "る", V2VerbType::Godan},                  // 聞きかじる
    {"漏らす", "もらす", "す", V2VerbType::Godan},                  // 聞き漏らす
    {"写す", "うつす", "す", V2VerbType::Godan},                    // 書き写す
    {"散らす", "ちらす", "す", V2VerbType::Godan},                  // 書き散らす
    {"囲む", "かこむ", "む", V2VerbType::Godan},                    // 取り囲む
    {"締まる", "しまる", "る", V2VerbType::Godan},                  // 取り締まる
    {"仕切る", "しきる", "る", V2VerbType::Godan},                  // 降りしきる, 鳴りしきる
    {"次ぐ", "つぐ", "ぐ", V2VerbType::Godan},                      // 取り次ぐ
    {"除く", "のぞく", "く", V2VerbType::Godan},                    // 取り除く
    {"移る", "うつる", "る", V2VerbType::Godan},                    // 乗り移る
    {"散る", "ちる", "る", V2VerbType::Godan},                      // 飛び散る
    {"退く", "のく", "く", V2VerbType::Godan},                      // 飛び退く
    // Ichidan verbs (一段)
    {"続ける", "つづける", "ける", V2VerbType::Ichidan},                         // 読み続ける, 読みつづける
    {"果てる", "はてる", "てる", V2VerbType::Ichidan, true, true, true, false},  // 疲れはてる
    {"まとめる", "まとめる", "める", V2VerbType::Ichidan},                       // 取りまとめる
    {"つける", nullptr, "ける", V2VerbType::Ichidan},                            // 見つける
    {"替える", "かえる", "える", V2VerbType::Ichidan},                           // 切り替える
    {"換える", "かえる", "える", V2VerbType::Ichidan},                           // 入れ換える
    {"合わせる", "あわせる", "せる", V2VerbType::Ichidan},                       // 組み合わせる
    {"浮かべる", "うかべる", "べる", V2VerbType::Ichidan},                       // 思い浮かべる
    {"切れる", "きれる", "れる", V2VerbType::Ichidan},                           // 使い切れる
    {"間違える", "まちがえる", "える", V2VerbType::Ichidan, false},              // 書き間違える
    {"出る", "でる", "る", V2VerbType::Ichidan},                                 // 飛び出る
    {"上げる", "あげる", "げる", V2VerbType::Ichidan},                           // 売り上げる, 取り上げる
    {"下げる", "さげる", "げる", V2VerbType::Ichidan},                           // 引き下げる
    {"抜ける", "ぬける", "ける", V2VerbType::Ichidan},                           // 突き抜ける
    {"着く", "つく", "く", V2VerbType::Godan},                                   // 落ち着く, たどり着く
    {"取る", "とる", "る", V2VerbType::Godan},                                   // 搾り取る, 掠め取る
    {"越す", "こす", "す", V2VerbType::Godan},                                   // 引っ越す, 追い越す
    {"越える", "こえる", "える", V2VerbType::Ichidan},                           // 乗り越える, 飛び越える
    {"張る", "はる", "る", V2VerbType::Godan},                                   // 引っ張る, 頑張る
    {"叫ぶ", "さけぶ", "ぶ", V2VerbType::Godan},                                 // 泣き叫ぶ, 喚き叫ぶ
    {"注ぐ", "そそぐ", "ぐ", V2VerbType::Godan},                                 // 降り注ぐ, 流し注ぐ
    {"継ぐ", "つぐ", "ぐ", V2VerbType::Godan},                   // 語り継ぐ, 受け継ぐ, 引き継ぐ
    {"挟む", "はさむ", "む", V2VerbType::Godan},                 // 差し挟む
    {"招く", "まねく", "く", V2VerbType::Godan},                 // 差し招く
    {"歩く", "あるく", "く", V2VerbType::Godan},                 // 渡り歩く
    {"ほどく", "ほどく", "く", V2VerbType::Godan},               // 振りほどく
    {"向く", "むく", "く", V2VerbType::Godan},                   // 振り向く
    {"描く", "えがく", "く", V2VerbType::Godan},                 // 思い描く
    {"誤る", "あやまる", "る", V2VerbType::Godan},               // 読み誤る
    {"尽くす", "つくす", "す", V2VerbType::Godan},               // 立ち尽くす
    {"聞かす", "きかす", "す", V2VerbType::Godan},               // 言い聞かす
    {"引く", "ひく", "く", V2VerbType::Godan},                   // 差し引く, 値引く
    {"向かう", "むかう", "う", V2VerbType::Godan},               // 立ち向かう
    {"並ぶ", "ならぶ", "ぶ", V2VerbType::Godan},                 // 立ち並ぶ
    {"果たす", "はたす", "す", V2VerbType::Godan},               // 使い果たす
    {"こなす", "こなす", "す", V2VerbType::Godan},               // 使いこなす
    {"刺す", "さす", "す", V2VerbType::Godan},                   // 突き刺す, 差し刺す
    {"望む", "のぞむ", "む", V2VerbType::Godan},                 // 待ち望む, 見望む
    {"落とす", "おとす", "す", V2VerbType::Godan},               // 切り落とす, 打ち落とす
    {"落ちる", "おちる", "ちる", V2VerbType::Ichidan},           // 転げ落ちる
    {"掛ける", "かける", "ける", V2VerbType::Ichidan},           // 呼び掛ける, 働き掛ける
    {"掛ける", "がける", "ける", V2VerbType::Ichidan},           // Compound-internal rendaku reading
    {"付ける", "つける", "ける", V2VerbType::Ichidan},           // 押し付ける, 決め付ける
    {"当てる", "あてる", "てる", V2VerbType::Ichidan},           // 振り当てる
    {"向ける", "むける", "ける", V2VerbType::Ichidan},           // 差し向ける
    {"遂げる", "とげる", "げる", V2VerbType::Ichidan},           // やり遂げる
    {"戻る", "もどる", "る", V2VerbType::Godan},                 // 立ち戻る
    {"入れる", "いれる", "れる", V2VerbType::Ichidan},           // 取り入れる, 持ち入れる
    {"分ける", "わける", "ける", V2VerbType::Ichidan},           // 切り分ける, 振り分ける
    {"立てる", "たてる", "てる", V2VerbType::Ichidan},           // 組み立てる, 打ち立てる
    {"重ねる", "かさねる", "ねる", V2VerbType::Ichidan},         // 積み重ねる, 折り重ねる
    {"広げる", "ひろげる", "げる", V2VerbType::Ichidan},         // 繰り広げる, 押し広げる
    {"支える", "ささえる", "える", V2VerbType::Ichidan},         // 差し支える
    {"受ける", "うける", "ける", V2VerbType::Ichidan},           // 引き受ける, 請け受ける
    {"降りる", "おりる", "りる", V2VerbType::Ichidan},           // 乗り降りる
    {"締める", "しめる", "める", V2VerbType::Ichidan},           // 抱きしめる, 締め締める
    {"止める", "とめる", "める", V2VerbType::Ichidan},           // 受け止める, 食い止める
    {"入る", "いる", "る", V2VerbType::Godan},                   // 飛び入る, 立ち入る
    {"止まる", "とまる", "る", V2VerbType::Godan},               // 立ち止まる, 踏み止まる, 思い止まる
    {"留める", "とめる", "める", V2VerbType::Ichidan},           // 書き留める
    {"寄せる", "よせる", "せる", V2VerbType::Ichidan},           // 取り寄せる, 引き寄せる
    {"伸べる", "のべる", "べる", V2VerbType::Ichidan},           // 差し伸べる
    {"控える", "ひかえる", "える", V2VerbType::Ichidan},         // 差し控える
    {"逃れる", "のがれる", "れる", V2VerbType::Ichidan},         // 言い逃れる
    {"聞かせる", "きかせる", "せる", V2VerbType::Ichidan},       // 言い聞かせる
    {"伏せる", "ふせる", "せる", V2VerbType::Ichidan},           // 組み伏せる
    {"混ぜる", "まぜる", "ぜる", V2VerbType::Ichidan},           // 取り混ぜる
    {"詰める", "つめる", "める", V2VerbType::Ichidan},           // 追い詰める, 追いつめる
    {"求める", "もとめる", "める", V2VerbType::Ichidan},         // 追い求める
    {"捨てる", "すてる", "てる", V2VerbType::Ichidan},           // 見捨てる
    {"届ける", "とどける", "ける", V2VerbType::Ichidan},         // 聞き届ける
    {"添える", "そえる", "える", V2VerbType::Ichidan},           // 書き添える
    {"揃える", "そろえる", "える", V2VerbType::Ichidan},         // 取り揃える, 買い揃える
    {"押さえる", "おさえる", "える", V2VerbType::Ichidan},       // 取り押さえる
    {"調べる", "しらべる", "べる", V2VerbType::Ichidan},         // 取り調べる
    {"違える", "ちがえる", "える", V2VerbType::Ichidan},         // 取り違える
    {"退ける", "のける", "ける", V2VerbType::Ichidan},           // 押し退ける
    {"遅れる", "おくれる", "れる", V2VerbType::Ichidan},         // 乗り遅れる
    {"忘れる", "わすれる", "れる", V2VerbType::Ichidan, false},  // 置き忘れる, 言い忘れる
    {"起きる", "おきる", "きる", V2VerbType::Ichidan},           // 飛び起きる
    {"下りる", "おりる", "りる", V2VerbType::Ichidan},           // 飛び下りる
    {"そこなう", "そこなう", "う", V2VerbType::Godan, true, false},  // 確認しそこなう
    {"損じる", "そんじる", "じる", V2VerbType::Ichidan},             // 書き損じる, 読み損じる
    {"渡る", "わたる", "る", V2VerbType::Godan},                     // 飛び渡る, 歩き渡る
    {"かざす", "かざす", "す", V2VerbType::Godan},                   // 振りかざす, 差しかざす
    {"置く", "おく", "く", V2VerbType::Godan},                       // 書き置く, 取り置く
    {"足す", "たす", "す", V2VerbType::Godan},                       // 付け足す, 書き足す
    {"直る", "なおる", "る", V2VerbType::Godan},                     // 向き直る, 座り直る
    {"下す", "くだす", "す", V2VerbType::Godan},                     // 書き下す, 読み下す
    {"交わす", "かわす", "す", V2VerbType::Godan},                   // 取り交わす, 書き交わす
    {"添う", "そう", "う", V2VerbType::Godan},                       // 寄り添う, 書き添う
    {"乱れる", "みだれる", "れる", V2VerbType::Ichidan},             // 入り乱れる, 打ち乱れる
    {"混じる", "まじる", "じる", V2VerbType::Godan},                 // 入り混じる, 溶け混じる
};

}  // namespace suzume::analysis::compound_verb_detail
