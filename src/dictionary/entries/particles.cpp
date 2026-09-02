#include "entries_internal.h"

namespace suzume::dictionary::entries {

// =============================================================================
EntrySpecRange getParticleEntries() {
  static constexpr EntrySpec kEntries[] = {
      // Case particles (格助詞)
      particle("が", EPOS::ParticleCase),
      particle("を", EPOS::ParticleCase),
      particle("に", EPOS::ParticleCase),
      particle("で", EPOS::ParticleCase),  // low cost for te-form split
      particle("と", EPOS::ParticleCase),
      particle("から", EPOS::ParticleCase),
      // まで is an adverbial particle, not an argument-marking case particle:
      // it stacks on top of one (にまで, からまで) and the case-stacking penalty
      // would otherwise break that chain apart at its own mora boundary.
      particle("まで", EPOS::ParticleAdverbial),
      particle("より", EPOS::ParticleCase),
      particle("へ", EPOS::ParticleCase),

      // Topic/Binding particles (係助詞)
      particle("は", EPOS::ParticleTopic),
      particle("も", EPOS::ParticleTopic),
      particle("こそ", EPOS::ParticleBinding),
      particle("なむ", EPOS::ParticleBinding),  // 文語係助詞
      particle("さえ", EPOS::ParticleBinding),
      particle("すら", EPOS::ParticleBinding),
      particle("しか", EPOS::ParticleBinding),
      particle("でも", EPOS::ParticleAdverbial),

      // Conjunctive particles (接続助詞)
      particle("て", EPOS::ParticleConj),  // low cost for te-form split
      particle("で", EPOS::ParticleConj),  // te-form for hatsuonbin verbs (読んで, 飛んで)
      particle("ば", EPOS::ParticleConj),
      particle("たら", EPOS::ParticleConj),
      particle("なら", EPOS::ParticleConj),
      // Causal から after a predicate (食べたから, 読むからといって). The case
      // reading above marks a nominal source (東京から), so both are needed.
      particle("から", EPOS::ParticleConj),

      // Binding particles (係助詞)
      particle("しも", EPOS::ParticleBinding),
      // Contracted conditional ちゃ (= ては): なく+ちゃ+いけない.
      particle("ちゃ", EPOS::ParticleConj),
      // Voiced counterpart じゃ (= では), selected by the same 撥音便 stem that
      // takes で: 読ん+じゃ+だめ, 飲ん+じゃ+いけない. The copula reading of じゃ
      // is a separate entry among the auxiliaries.
      particle("じゃ", EPOS::ParticleConj),
      // ら is not a conditional particle here: たら owns that role, while the
      // plural suffix ら is an L1 entry in auxiliaries.cpp.
      particle("ながら", EPOS::ParticleConj),
      particle("つつ", EPOS::ParticleConj),  // 反復・並行の接続助詞 (連用形接続): 重ね+つつ, 増加し+つつ+ある
      // 即時: 聞くや否や, 着くや否や. A fixed classical compound with no
      // independent inflection inside it, so it stays one search unit; the
      // kana-only spelling いなや is deliberately absent, being far more often
      // the disjunctive や plus a lexical word.
      particle("や否や", EPOS::ParticleConj),
      particle("とともに", EPOS::ParticleConj),  // 並行・同時: 読むとともに書く
      particle("と共に", EPOS::ParticleConj),    // 漢字交じり表記
      particle("とも", EPOS::ParticleConj),      // 譲歩: 読まずとも, 食べずとも
      particle("ど", EPOS::ParticleConj),        // 文語的譲歩: といえど
      particle("ども", EPOS::ParticleConj),      // 譲歩: といえども, いかに…ども
      particle("のに", EPOS::ParticleConj),
      particle("ので", EPOS::ParticleConj),
      // Causal premise: 書いたからには, 高いからには.
      particle("からには", EPOS::ParticleConj),
      particle("けれど", EPOS::ParticleConj),
      particle("けど", EPOS::ParticleConj),
      particle("けども", EPOS::ParticleConj),
      particle("けれども", EPOS::ParticleConj),
      particle("ものの", EPOS::ParticleConj),
      particle("し", EPOS::ParticleConj),         // 列挙・理由 (接続助詞)
      particle("たり", EPOS::ParticleConj),       // 並立助詞 (食べたり飲んだり)
      particle("だり", EPOS::ParticleConj),       // 並立助詞 (voiced: 飲んだり)
      particle("なり", EPOS::ParticleConj),       // 動作直後: 鳴るなり
      particle("や", EPOS::ParticleConj),         // 並立助詞 (AやB)
      particle("だの", EPOS::ParticleAdverbial),  // 列挙・並立: 赤だの青だの

      // Quotation particles (引用助詞)
      particle("って", EPOS::ParticleQuote),
      // Colloquial quotative closing a resolution, with the reporting verb
      // elided (行こう+と(思う) -> 行こ+っと). The volitional contracts its う
      // into the geminate, so the particle carries that mora and the verb keeps
      // its o-row cell; without an edge of its own the geminate was absorbed
      // leftward and the predicate lost its boundary (行こっ+と).
      particle("っと", EPOS::ParticleQuote),

      // Final particles (終助詞)
      particle("か", EPOS::ParticleFinal),
      particle("かい", EPOS::ParticleFinal),
      particle("け", EPOS::ParticleFinal),  // colloquial variant (こんだけ → こん+だ+け)
      particle("な", EPOS::ParticleFinal),
      particle("なあ", EPOS::ParticleFinal),
      particle("ね", EPOS::ParticleFinal),
      particle("ねえ", EPOS::ParticleFinal),
      particle("よ", EPOS::ParticleFinal),
      particle("さ", EPOS::ParticleFinal),
      particle("わ", EPOS::ParticleFinal),
      particle("ぞ", EPOS::ParticleFinal),
      // ぞ is also the classical binding particle, like こそ and なむ above: it
      // licenses a nominal in the inverted kakari-musubi order (散りたる+ぞ+花).
      // The modern sentence-final reading stays alongside it.
      particle("ぞ", EPOS::ParticleBinding),
      particle("ぜ", EPOS::ParticleFinal),
      particle("の", EPOS::ParticleNo),               // nominalizer
      {"ん", POS::Particle, EPOS::ParticleNo, "の"},  // colloquial の
      particle("じゃん", EPOS::ParticleFinal),
      particle("っけ", EPOS::ParticleFinal),
      particle("かしら", EPOS::ParticleFinal),
      particle("だい", EPOS::ParticleFinal),
      // Regional final particles. They attach to a predicate terminal like the
      // standard ones above, unlike the dialect copulas in the auxiliary table.
      particle("ばい", EPOS::ParticleFinal),
      particle("やんけ", EPOS::ParticleFinal),
      particle("ねん", EPOS::ParticleFinal),  // Kansai explanatory (読む+ねん)
      particle("え", EPOS::ParticleFinal),    // Kyoto clause-final (ます+え)
      // Regional causal conjunctions, attaching to a predicate terminal
      // (飲む+さかい, 読む+けん). Both are homographic with a frequent
      // standard-language piece — the nominalizer さ, the final particle け —
      // so they rely on that terminal connection rather than on their own
      // cost. The Tosa き is deliberately absent: a single mora identical to
      // the continuative ending has no boundary of its own to stand on, and
      // registering it pulled き out of ordinary continuatives (解き, 瞬き).
      particle("さかい", EPOS::ParticleConj),
      particle("けん", EPOS::ParticleConj),

      // Adverbial particles (副助詞)
      particle("かも", EPOS::ParticleAdverbial),  // prevent か+も split in かもしれない
      particle("なんか", EPOS::ParticleAdverbial),
      particle("ばかり", EPOS::ParticleAdverbial),
      particle("だけ", EPOS::ParticleAdverbial),
      particle("のみ", EPOS::ParticleAdverbial),
      particle("ほど", EPOS::ParticleAdverbial),
      particle("くらい", EPOS::ParticleAdverbial),
      particle("ぐらい", EPOS::ParticleAdverbial),
      particle("など", EPOS::ParticleAdverbial),
      particle("とか", EPOS::ParticleAdverbial),  // 並立 (AとかBとか)
      particle("とも", EPOS::ParticleAdverbial),  // 集合の全称 (二人とも, 三つとも)
      particle("なんて", EPOS::ParticleAdverbial),
      particle("だって", EPOS::ParticleAdverbial),
      particle("だに", EPOS::ParticleAdverbial),    // 文語的な最小限定・強調
      particle("おろか", EPOS::ParticleAdverbial),  // 強調・追加: 基本はおろか応用も
      // 対比・強調: 確認どころか. It always attaches to the phrase on its left,
      // so it is a binding particle, not a clause-opening conjunction —
      // それどころか is the lexicalized conjunction built on top of it.
      particle("どころか", EPOS::ParticleAdverbial),
      particle("しも", EPOS::ParticleAdverbial),  // 強調・限定: 誰しも、必ずしも
      particle("きり", EPOS::ParticleAdverbial),  // 限定: 一度きり、これきり
      particle("ずつ", EPOS::ParticleAdverbial),  // distributive 副助詞 - prevent ず(打消)+つ split after a quantity
      particle("ってば", EPOS::ParticleFinal),
      particle("ったら", EPOS::ParticleFinal),

  };
  return makeEntrySpecRange(kEntries);
}

}  // namespace suzume::dictionary::entries
