#include "bigram_table_internal.h"

namespace suzume::analysis::bigram_rules {

using EPOS = core::ExtendedPOS;
namespace cost = bigram_cost;
constexpr float kDeterminerNounBonus = -2.5F;

void setAuxiliaryAndNounCosts(BigramMatrix& table) {
  static constexpr BigramRule kRules[] = {
      // =========================================================================
      // Auxiliary → Auxiliary Chains
      // =========================================================================

      // AuxTenseMasu → AuxTenseTa (まし+た) - strong bonus
      {EPOS::AuxTenseMasu, EPOS::AuxTenseTa, cost::kStrongBonus},

      // The polite auxiliary attaches to a continuative predicate and to nothing
      // else, so no particle can host it. Its 連用形 まし is also the na-adjective
      // まし (こちらの方がましだ) and the opening of ましい-derived adjectives, and
      // those readings are the ones a particle actually selects.
      {EPOS::ParticleCase, EPOS::AuxTenseMasu, cost::kSevere},
      {EPOS::ParticleTopic, EPOS::AuxTenseMasu, cost::kSevere},
      {EPOS::ParticleBinding, EPOS::AuxTenseMasu, cost::kSevere},
      {EPOS::ParticleAdverbial, EPOS::AuxTenseMasu, cost::kSevere},

      // AuxTenseMasu → AuxNegativeNu (ませ+ん for polite negative) - strong bonus
      // Ensures ません → ませ+ん (aux) over ませ+ん (particle の)
      {EPOS::AuxTenseMasu, EPOS::AuxNegativeNu, cost::kStrongBonus},

      // Both appearance and hearsay そう can take the polite copula
      // (降りそうです, 来るそうです). Preserve the auxiliary chain over the
      // homographic na-adjective followed by です.
      // Appearance そう takes the copula in both predicative and attributive
      // forms (高そう+だ/です、高そう+な山).  Without the な connection, a
      // noun homograph before adjectival そう can win after the correct
      // adjective-stem path has already been generated.
      {EPOS::AuxAppearanceSou, EPOS::AuxCopulaDa, cost::kVeryStrongBonus},
      {EPOS::AuxAppearanceSou, EPOS::AuxCopulaDesu, cost::kTripleVeryStrongBonus},

      // The negative ending after the polite auxiliary is always the
      // auxiliary ん, never the nominalizer (読み+ませ+ん+か).
      {EPOS::AuxTenseMasu, EPOS::ParticleNo, cost::kAlmostNever},

      // AuxTenseMasu → AuxVolitional (ましょ+う) - strong bonus for the volitional boundary
      {EPOS::AuxTenseMasu, EPOS::AuxVolitional, cost::kStrongBonus},

      // A polite predicate may be followed by the conjectural polite copula
      // (ございます+でしょ+うか). Keep that inflectional boundary ahead of
      // the unrelated connective-particle plus fabricated verb route.
      {EPOS::AuxTenseMasu, EPOS::AuxCopulaDesu, cost::kVeryStrongBonus},

      // Past predicate → concessive particle (読んだものの進まない).
      {EPOS::AuxTenseTa, EPOS::ParticleConj, cost::kDoubleVeryStrongBonus},

      // The aspectual auxiliary いく only attaches to a connective verb form
      // (読んで+いく). It cannot follow the past auxiliary た; this prevents
      // a desiderative form such as たく from becoming た+く.
      {EPOS::AuxTenseTa, EPOS::AuxAspectIku, cost::kAlmostNever},

      // The classical perfect り attaches to a 四段已然形 or サ変未然形, never to
      // the modern past た. Without this the alternating たり of a coordinated
      // list decomposes at the end of a clause (読んだり書い+た+り).
      {EPOS::AuxTenseTa, EPOS::AuxClassicalPerfect, cost::kSevere},
      {EPOS::VerbTaForm, EPOS::AuxAspectIku, cost::kAlmostNever},

      // AuxTenseMasu → ParticleConj (まし+て, ますれ+ば) - decisive bonus
      // for the closed polite paradigm.  In ますれ+ば it must beat the
      // homographic fabricated lexical verb まする in 仮定形.
      {EPOS::AuxTenseMasu, EPOS::ParticleConj, cost::kDoubleVeryStrongBonus},

      // AuxCopulaDesu → AuxVolitional (でしょ+う) - very strong bonus for the
      // inflectional boundary.  This must beat the fabricated で+しょう
      // (case particle + unknown godan verb) path.
      {EPOS::AuxCopulaDesu, EPOS::AuxVolitional, cost::kVeryStrongBonus},

      // AuxCopulaDesu → AuxTenseTa (でし+た/たら) - very-strong bonus for the
      // past and conditional forms of the polite copula.
      {EPOS::AuxCopulaDesu, EPOS::AuxTenseTa, cost::kVeryStrongBonus},

      // AuxCopulaDa → AuxVolitional (だろ+う) - very strong bonus for the volitional boundary
      // The copular conjecture must beat a fabricated pure-hiragana verb.
      {EPOS::AuxCopulaDa, EPOS::AuxVolitional, cost::kVeryStrongBonus},

      // AuxCausative → AuxPassive (せ+られ in causative-passive) - strong bonus
      // Ensures 聞かせられた → 聞か+せ+られ+た over 聞か+せられた
      {EPOS::AuxCausative, EPOS::AuxPassive, cost::kStrongBonus},

      // AuxPassive → AuxCausative (れ+させる in passive causative) - strong
      // bonus. Voice auxiliaries retain their boundary in both grammatical
      // orders: 書か+れ+させる as well as 聞か+せ+られる.
      {EPOS::AuxPassive, EPOS::AuxCausative, cost::kStrongBonus},

      // AuxCausative → AuxNegativeNai (せ+ない, させ+ない in causative negative) - moderate bonus
      // Ensures 読ませない → 読ま+せ+ない with せ as causative せる, not せ=する (サ変未然).
      // せ+ない is always causative negation; する negation is しない, so no ambiguity.
      // Parallel to AuxPassive → AuxNegativeNai; without this the VerbMizenkei→AuxNegativeNai
      // bonus on the せ=する reading wins asymmetrically only when ない follows.
      {EPOS::AuxCausative, EPOS::AuxNegativeNai, cost::kModerateBonus},

      // A bare causative auxiliary stem cannot be nominally case/topic marked.
      // This rejects 知ら+せ+を/は in favour of the lexical noun 知らせ; a
      // causative predicate needs a finite form or nominalizer before particles.
      {EPOS::AuxCausative, EPOS::ParticleCase, cost::kAlmostNever},
      {EPOS::AuxCausative, EPOS::ParticleTopic, cost::kAlmostNever},

      // A causative auxiliary cannot attach directly to a copula. A surface
      // such as し+だった must instead be licensed as a lexical noun before
      // the copula; a causative predicate needs a finite ending first.
      {EPOS::AuxCausative, EPOS::AuxCopulaDa, cost::kAlmostNever},

      // Honorific subsidiary negative: いただけ+ない, なさら+ない. Keep
      // the dependent auxiliary reading ahead of a homographic lexical verb.
      {EPOS::AuxHonorific, EPOS::AuxNegativeNai, cost::kDoubleVeryStrongBonus},

      // AuxCausative → AuxTenseMasu (せ+ます, させ+ます) - strong bonus.
      {EPOS::AuxCausative, EPOS::AuxTenseMasu, cost::kStrongBonus},

      // Honorific subsidiary inflection continues into polite and past
      // auxiliaries (なさい+ます, なさっ+た) rather than a lexical verb path.
      {EPOS::AuxHonorific, EPOS::AuxTenseMasu, cost::kStrongBonus},
      {EPOS::AuxHonorific, EPOS::AuxTenseTa, cost::kStrongBonus},

      // AuxPassive → AuxTenseMasu (れ+ます in passive polite) - extra-strong
      // bonus. This preserves the auxiliary boundary in longer polite forms
      // such as 読ま+れ+まし+て as well as 言わ+れ+ます.
      {EPOS::AuxPassive, EPOS::AuxTenseMasu, cost::kExtraStrongBonus},

      // AuxPassive → AuxNegativeNai (れ+ない in passive negative) -
      // very strong bonus. This preserves the passive boundary before the
      // continuative negative as well (読ま+れ+なく+て).
      {EPOS::AuxPassive, EPOS::AuxNegativeNai, cost::kVeryStrongBonus},

      // An adverbial ない can follow a passive predicate before a connective
      // (読ま+れ+なく+て). It competes with the auxiliary reading, which stays
      // available before an independent change-of-state verb.
      {EPOS::AuxPassive, EPOS::AdjRenyokei, cost::kVeryStrongBonus},

      // AuxPassive → AuxTenseTa (れ+た in passive past) - strong bonus
      // Ensures 言われた → 言わ+れ+た over 言われ+た
      {EPOS::AuxPassive, EPOS::AuxTenseTa, cost::kStrongBonus},

      // A passive stem can take the classical negative auxiliary
      // (行か+れ+ぬ, 読ま+れ+ず).
      {EPOS::AuxPassive, EPOS::AuxNegativeNu, cost::kStrongBonus},

      // AuxPassive → AuxDesireTai (れ+たい in passive desiderative) - strong bonus
      // Ensures 見られたい → 見+られ+たい over 見+られ+た+い
      {EPOS::AuxPassive, EPOS::AuxDesireTai, cost::kStrongBonus},

      // AuxPassive → AuxVolitional (れる+べき in passive obligation) - strong bonus
      // Ensures 書かれるべき → 書か+れる+べき(dict) over char_speech べき(AUX_過去) path
      {EPOS::AuxPassive, EPOS::AuxVolitional, cost::kStrongBonus},

      // AuxPassive → ParticleConj (れ+ながら, れ+ば in passive+conjunctive) - moderate bonus
      // Ensures 揉まれながら → 揉ま+れ+ながら over 揉まれ+ながら
      {EPOS::AuxPassive, EPOS::ParticleConj, cost::kModerateBonus},

      // The 已然形 of the classical perfect is selected by a conjunctive
      // particle (記録し+たれ+ども), rather than by the modern past-plus-passive
      // homograph し+た+れ+ども.
      {EPOS::AuxClassicalPerfect, EPOS::ParticleConj, cost::kStrongBonus},

      // A finite passive predicate modifies a formal noun just as a lexical
      // terminal verb does: 成し遂げ+られる+もの, 委ね+られる+ほか.
      {EPOS::AuxPassive, EPOS::NounFormal, cost::kVeryStrongBonus},

      // AuxNegativeNai → AuxTenseTa (なかっ+た) - strong bonus
      {EPOS::AuxNegativeNai, EPOS::AuxTenseTa, cost::kStrongBonus},

      // AuxNegativeNai → AuxVolitional (なかろ+う) - strong bonus
      {EPOS::AuxNegativeNai, EPOS::AuxVolitional, cost::kStrongBonus},

      // Negative predicates commonly take a formal noun (ない+つもり/わけ/はず).
      // Prefer that productive boundary over an unrelated hiragana adverb that
      // happens to begin at the final い of ない. The auxiliary modifies a
      // formal noun exactly as its classical sibling and the passive do, so it
      // carries their margin; the weaker one let the homographic adjective win
      // the position and pulled an irrealis split in behind it
      // (分か+ん+ない+こと, not 分かん+ない+こと).
      {EPOS::AuxNegativeNai, EPOS::NounFormal, cost::kVeryStrongBonus},
      {EPOS::AuxNegativeNai, EPOS::Noun, cost::kStrongBonus},

      // Classical negation also retains its attributive boundary before a
      // formal noun (行か+ぬ+わけ、知ら+ぬ+ふり).
      {EPOS::AuxNegativeNu, EPOS::NounFormal, cost::kVeryStrongBonus},

      // The contracted negative also modifies an ordinary noun (読ま+ん+人),
      // rather than selecting the homographic nominalizer particle.
      {EPOS::AuxNegativeNu, EPOS::Noun, cost::kVeryStrongBonus},

      // Negative conjectural forms can be attributive (行くまじき行為, 行くまい人).
      {EPOS::AuxNegativeMai, EPOS::Noun, cost::kStrongBonus},

      // AuxNegativeNu → AuxTenseTa (んかっ+た for contracted negative past)
      // Ensures くだらんかった → くだら+ん+かっ+た over くだ+らんかっ+た
      {EPOS::AuxNegativeNu, EPOS::AuxTenseTa, cost::kStrongBonus},

      // AuxNegativeNai → ParticleNo (ない+ん for のだ/んだ) - strong bonus
      // Ensures ないんだ → ない+ん+だ over な+いん+だ
      {EPOS::AuxNegativeNai, EPOS::ParticleNo, cost::kStrongBonus},

      // AuxNegativeNai → ParticleConj (なけれ+ば, ない+のに, ない+ので) - decisive bonus
      // Ensures the negative conditional and conjunctive forms retain their auxiliaries.
      {EPOS::AuxNegativeNai, EPOS::ParticleConj, cost::kDoubleVeryStrongBonus},

      // ParticleNo → AuxCopulaDesu (ん+です/でし for んです/んでした) - strong bonus
      // Ensures んでした → ん+でし+た over ん+で+し+た
      {EPOS::ParticleNo, EPOS::AuxCopulaDesu, cost::kStrongBonus},

      // AuxNegativeNu → AuxCopulaDesu (ん+でし for ませんでした) - decisive bonus
      // Ensures ませんでした → ませ+ん+でし+た (negative aux ん)
      {EPOS::AuxNegativeNu, EPOS::AuxCopulaDesu, cost::kDoubleVeryStrongBonus},

      // AuxNegativeNai → AuxCopulaDesu (ない+でしょ/です) - the polite copula
      // follows the terminal negative just as it follows the classical ん.
      // Without it the competing ない+で conjunctive connection above wins and
      // でしょ is re-cut as で plus a fabricated verb (ないでしょうか).
      {EPOS::AuxNegativeNai, EPOS::AuxCopulaDesu, cost::kVeryStrongBonus},

      // The classical/contracted negative ん attaches to a verb form, not to
      // the already negative auxiliary ない. In ないんだ the ん is the
      // nominalizer の, followed by the copula.
      {EPOS::AuxNegativeNai, EPOS::AuxNegativeNu, cost::kAlmostNever},

      // AuxNegativeNu → ParticleTopic (ずに+は for ずにはいられない) - strong bonus
      // Ensures ずには → ずに+は(topic) over ずに+はいられ(verb)
      {EPOS::AuxNegativeNu, EPOS::ParticleTopic, cost::kStrongBonus},

      // Classical negative predicates can be case-marked (ざる+を+得ない,
      // ぬ+を+知らない). Preserve the case-particle boundary over an unknown
      // fallback for the one-mora particle.
      {EPOS::AuxNegativeNu, EPOS::ParticleCase, cost::kStrongBonus},

      // Classical negative → concessive particle (読ま+ず+とも).
      {EPOS::AuxNegativeNu, EPOS::ParticleConj, cost::kStrongBonus},

      // The 已然形 of the classical copula takes the conditional/conjunctive
      // particle (重要+なれ+ば). Its passive homograph is not licensed after
      // a copula, so retain the registered copular paradigm as one edge.
      {EPOS::AuxClassicalNari, EPOS::ParticleConj, cost::kDoubleVeryStrongBonus},

      // The irrealis form of the classical obligation auxiliary is followed
      // by classical negation (べから+ず). Keep that inflectional chain ahead
      // of a fabricated verb stem spanning both auxiliaries.
      {EPOS::AuxClassicalBeshi, EPOS::AuxNegativeNu, cost::kDoubleVeryStrongBonus},

      // ParticleNo → AuxCopulaDa (ん+だ for んだ) - strong bonus
      // Ensures んだ → ん+だ over ん+だ(VERB)
      {EPOS::ParticleNo, EPOS::AuxCopulaDa, cost::kStrongBonus},

      // ParticleNo → Noun (の+学生, の+画像) - strong bonus
      // Genitive の + noun is fundamental Japanese grammar
      {EPOS::ParticleNo, EPOS::Noun, cost::kStrongBonus},

      // ParticleNo → NounNumber (の+3分の1, の+二人) - same strong bonus
      // A quantity nominal heads a genitive phrase exactly as a plain noun
      // does; without this row the quantity reading loses to a plain-noun
      // prefix of itself purely because the connection was unscored.
      {EPOS::ParticleNo, EPOS::NounNumber, cost::kStrongBonus},

      // AuxDesireTai → AuxTenseTa (たかっ+た) - strong bonus
      {EPOS::AuxDesireTai, EPOS::AuxTenseTa, cost::kStrongBonus},

      // AuxDesireTai → AuxNegativeNai (たく+ない/なかっ) - moderate bonus
      // 走り出したくなかった → 走り出し+たく+なかっ+た (not 走り+出したく+なかっ+た)
      {EPOS::AuxDesireTai, EPOS::AuxNegativeNai, cost::kModerateBonus},

      // A desiderative predicate can modify a formal noun (確かめ+たい+こと).
      // Without this connection, the lattice can reinterpret the final mora
      // of the lexical verb as the start of an unrelated hiragana predicate.
      {EPOS::AuxDesireTai, EPOS::NounFormal, cost::kStrongBonus},

      // AuxTenseTa → verb forms - prohibit. A completed predicate cannot take a
      // second bare verb without a connective boundary. Cover every verb form;
      // limiting this to onbin/past shapes permits fragments such as た+だく.
      {EPOS::AuxTenseTa, EPOS::VerbShuushikei, cost::kAlmostNever},
      {EPOS::AuxTenseTa, EPOS::VerbRenyokei, cost::kAlmostNever},
      {EPOS::AuxTenseTa, EPOS::VerbMizenkei, cost::kAlmostNever},
      {EPOS::AuxTenseTa, EPOS::VerbOnbinkei, cost::kAlmostNever},
      {EPOS::AuxTenseTa, EPOS::VerbTeForm, cost::kAlmostNever},
      {EPOS::AuxTenseTa, EPOS::VerbKateikei, cost::kAlmostNever},
      {EPOS::AuxTenseTa, EPOS::VerbMeireikei, cost::kAlmostNever},
      {EPOS::AuxTenseTa, EPOS::VerbRentaikei, cost::kAlmostNever},
      {EPOS::AuxTenseTa, EPOS::VerbTaForm, cost::kAlmostNever},
      {EPOS::AuxTenseTa, EPOS::VerbTaraForm, cost::kAlmostNever},

      // A completed predicate can be followed by the quotative attributive
      // determiner: 読んだ+という, 書いた+っていう.
      {EPOS::AuxTenseTa, EPOS::DeterminerQuotative, cost::kDoubleVeryStrongBonus},

      // ParticleTopic → AuxTenseTa - prohibit
      // The past auxiliary attaches to a verb/adjective renyokei, never to a topic
      // particle, so は+た is not a real boundary. Prevents an isolated hiragana noun
      // from splitting into は(係助詞)+た(過去)+… (はたけ → は+た+け).
      {EPOS::ParticleTopic, EPOS::AuxTenseTa, cost::kSevere},
      // The same holds for an adverbial particle (いつまで+たっ+て+も, never
      // いつまで+た+って+も).
      {EPOS::ParticleAdverbial, EPOS::AuxTenseTa, cost::kSevere},
      // A pronoun is a nominal, so it reaches the past through the copula
      // (これ+だっ+た) and never hosts the tense auxiliary directly. Without
      // this the voiced past だ can pose as the copula behind an interrogative
      // and pull a colloquial のだ clause into it (そう+なん+だ, not そう+な+ん+だ).
      {EPOS::Pronoun, EPOS::AuxTenseTa, cost::kSevere},
      {EPOS::PronounInterrogative, EPOS::AuxTenseTa, cost::kSevere},

      // AuxAspectIru → AuxTenseTa (い+た) - moderate bonus
      {EPOS::AuxAspectIru, EPOS::AuxTenseTa, cost::kModerateBonus},

      // AuxAspectOku → AuxTenseTa (とい+た, どい+た) - strong bonus
      // Contracted ~ておく form + past tense: 見とい+た, 読んどい+た
      {EPOS::AuxAspectOku, EPOS::AuxTenseTa, cost::kStrongBonus},

      // The preparative auxiliary itself inflects to the te-form
      // (書い+て+おい+て).  Keep that closed auxiliary cell ahead of the
      // homographic open-verb onbin candidate.
      {EPOS::AuxAspectOku, EPOS::ParticleConj, cost::kVeryStrongBonus},

      // AuxAspectOku → AuxNegativeNai (とか+ない, どか+なきゃ) - decisive bonus
      // Only the mizenkei cell of the contracted preparation auxiliary can
      // precede the negative, and its surface とか also spells the adverbial
      // particle, whose dictionary anchor is otherwise cheaper.
      {EPOS::AuxAspectOku, EPOS::AuxNegativeNai, cost::kVeryStrongBonus},

      // The completive subsidiary しまう conjugates as a Godan-wa auxiliary.
      // Its written variants share this grammar: 仕舞っ+た, 仕舞わ+ない,
      // 仕舞い+ます, 仕舞え+ば, and 仕舞お+う.
      {EPOS::AuxAspectShimau, EPOS::AuxTenseTa, cost::kVeryStrongBonus},
      {EPOS::AuxAspectShimau, EPOS::AuxNegativeNai, cost::kVeryStrongBonus},
      {EPOS::AuxAspectShimau, EPOS::AuxTenseMasu, cost::kStrongBonus},
      {EPOS::AuxAspectShimau, EPOS::AuxDesireTai, cost::kStrongBonus},
      {EPOS::AuxAspectHajimeru, EPOS::AuxTenseTa, cost::kStrongBonus},
      // The completive auxiliary can take the volitional ending (しまお+う).
      // Prefer this auxiliary sequence over a homographic lexical godan verb.
      {EPOS::AuxAspectShimau, EPOS::AuxVolitional, cost::kCompletiveVolitionalBonus},
      {EPOS::AuxAspectShimau, EPOS::ParticleConj, cost::kVeryStrongBonus},

      // The excessive subsidiary can inflect or be nominalized. Keep its
      // auxiliary analysis ahead of the homographic nominalized-stem candidate.
      {EPOS::AuxExcessive, EPOS::ParticleConj, cost::kStrongBonus},
      {EPOS::AuxExcessive, EPOS::ParticleCase, cost::kVeryStrongBonus},
      {EPOS::AuxExcessive, EPOS::ParticleTopic, cost::kVeryStrongBonus},

      // Directional いく likewise inflects before the negative and conditional
      // continuations (読んでいけない, 読んでいければ). These paths are
      // available only through context-gated auxiliary candidates, so the
      // inflection connections do not license a bare lexical いける reading.
      {EPOS::AuxAspectIku, EPOS::AuxNegativeNai, cost::kDoubleVeryStrongBonus},
      {EPOS::AuxAspectIku, EPOS::ParticleConj, cost::kStrongBonus},
      {EPOS::AuxAspectIku, EPOS::AuxVolitional, cost::kStrongBonus},
      // The progressive auxiliary い cannot be followed by the directional
      // auxiliary く. Keep 〜て+いく as one directional verb rather than the
      // impossible 〜て+い+く auxiliary chain.
      {EPOS::AuxAspectIru, EPOS::AuxAspectIku, cost::kAlmostNever},
      {EPOS::AuxAspectKuru, EPOS::AuxVolitional, cost::kStrongBonus},

      // Directional くる inflects after a te-form as well: 読ん+で+き+た,
      // 読ん+で+き+まし+た, 読ん+で+き+て. These rules keep its auxiliary
      // reading ahead of the lexical homograph できる.
      {EPOS::AuxAspectKuru, EPOS::AuxTenseTa, cost::kVeryStrongBonus},
      {EPOS::AuxAspectKuru, EPOS::AuxTenseMasu, cost::kStrongBonus},
      {EPOS::AuxAspectKuru, EPOS::ParticleConj, cost::kVeryStrongBonus},
      // The directional subsidiary can itself be potential/passive
      // (書い+て+こ+られ+た), rather than opening a new lexical られる.
      {EPOS::AuxAspectKuru, EPOS::AuxPassive, cost::kStrongBonus},

      // The directional subsidiary is negated by ない (持ってこない), and its
      // mizenkei candidate is context-gated on exactly that ending.
      {EPOS::AuxAspectKuru, EPOS::AuxNegativeNai, cost::kStrongBonus},

      // Both contracted and uncontracted renyokei forms accept polite ます.
      // The stronger connection resolves their lexical homographs (おき/とき).
      {EPOS::AuxAspectOku, EPOS::AuxTenseMasu, cost::kVeryStrongBonus},

      // The preparative subsidiary requires a preceding verb and continues
      // into its own inflection (とい+た/て). It cannot introduce an adverb;
      // rejecting that path preserves quotation boundaries in と+いう+より.
      {EPOS::AuxAspectOku, EPOS::Adverb, cost::kAlmostNever},

      // The progressive auxiliary conjugates as an Ichidan verb. Its stem い
      // therefore takes the negative auxiliary directly (覚えて+い+なかった).
      // This also distinguishes subsidiary い from the independent verb いる.
      {EPOS::AuxAspectIru, EPOS::AuxNegativeNai, cost::kStrongBonus},

      // A progressive auxiliary cannot take the independent adjective ない.
      // The negative in 〜ていない is always the auxiliary continuation.
      {EPOS::AuxAspectIru, EPOS::AdjBasic, cost::kAlmostNever},

      // AuxAspectIru → AuxTenseMasu (い+ます) - strong bonus for aspect plus politeness
      // Ensures 学んで+い+ます uses AuxAspectIru (auxiliary) not VerbRenyokei
      {EPOS::AuxAspectIru, EPOS::AuxTenseMasu, cost::kStrongBonus},

      // The terminal progressive also introduces a quotation: 食べている+という.
      {EPOS::AuxAspectIru, EPOS::DeterminerQuotative, cost::kDoubleVeryStrongBonus},

      // A terminal progressive predicate can be nominalized before a following
      // particle (食べてる+の+に). Prefer that boundary over a fused lexical
      // verb reading of the preceding progressive form.
      {EPOS::AuxAspectIru, EPOS::ParticleNo, cost::kStrongBonus},

      // The progressive is a finite predicate, so it heads a subordinate clause
      // through a conjunctive particle exactly as the other aspect auxiliaries
      // do (走ってる+のに, 走ってる+ので). Without the same weight the
      // nominalizer row above carries, the two-morpheme reading always
      // undercuts the listed conjunctive particle that spells the same run.
      {EPOS::AuxAspectIru, EPOS::ParticleConj, cost::kStrongBonus},

      // The trial subsidiary みる conjugates as an Ichidan auxiliary. Its stem
      // therefore accepts the same independent tense, negation, and desiderative
      // auxiliaries as a lexical Ichidan renyokei (試してみ+ます/ない/たい).
      {EPOS::AuxAspectMiru, EPOS::AuxTenseMasu, cost::kStrongBonus},
      {EPOS::AuxAspectMiru, EPOS::AuxNegativeNai, cost::kStrongBonus},
      {EPOS::AuxAspectMiru, EPOS::AuxDesireTai, cost::kDoubleVeryStrongBonus},
      {EPOS::AuxAspectMiru, EPOS::AuxTenseTa, cost::kModerateBonus},
      {EPOS::AuxAspectMiru, EPOS::AuxVolitional, cost::kModerateBonus},
      {EPOS::AuxAspectMiru, EPOS::ParticleConj, cost::kStrongBonus},

      // A dependency-marked trial auxiliary is itself the predicate stem. It
      // cannot hand off to an unrelated bare lexical irrealis form (て+み+せ+ない).
      // Longer closed subsidiary predicates such as て+みせ+ない remain valid.
      {EPOS::AuxAspectMiru, EPOS::VerbMizenkei, cost::kAlmostNever},

      // The dependent copular ござる keeps its classical negative boundary:
      // で+ござら+ぬ, parallel to the polite で+ござい+ます chain.
      {EPOS::AuxGozaru, EPOS::AuxNegativeNu, cost::kStrongBonus},

      // The preparative subsidiary おく and benefactive やる both take the
      // desiderative auxiliary directly in 〜ておきたい / 〜てやりたい.
      {EPOS::AuxAspectOku, EPOS::AuxDesireTai, cost::kVeryStrongBonus},
      {EPOS::AuxBenefactive, EPOS::AuxDesireTai, cost::kVeryStrongBonus},

      // The inability subsidiary かねる conjugates like an Ichidan auxiliary:
      // 読み+かね+ます/ない, 読み+かねる+た.
      {EPOS::AuxInability, EPOS::AuxTenseMasu, cost::kStrongBonus},
      {EPOS::AuxInability, EPOS::AuxNegativeNai, cost::kVeryStrongBonus},
      {EPOS::AuxInability, EPOS::AuxTenseTa, cost::kModerateBonus},
      {EPOS::AuxInability, EPOS::AuxVolitional, cost::kModerateBonus},

      // A binding particle such as しか cannot govern the classical negative
      // auxiliary (しか+ね). The modern construction しか+ない is unaffected.
      {EPOS::ParticleBinding, EPOS::AuxNegativeNu, cost::kAlmostNever},

      // The benefactive subsidiary あげる takes the potential/passive
      // auxiliary directly in 〜てあげ+られる.
      {EPOS::AuxBenefactive, EPOS::AuxPotential, cost::kStrongBonus},
      {EPOS::AuxBenefactive, EPOS::AuxPassive, cost::kStrongBonus},

      // Benefactive subsidiaries retain their dependent reading before
      // negation (読んで+もらえ+ない, 食べて+あげ+ない).
      {EPOS::AuxBenefactive, EPOS::AuxNegativeNai, cost::kVeryStrongBonus},

      // A benefactive auxiliary in potential form continues into polite ます
      // (聞かせて+もらえ+ます). This keeps the licensed te-form chain ahead
      // of the homographic independent-verb reading.
      {EPOS::AuxBenefactive, EPOS::AuxTenseMasu, cost::kDoubleVeryStrongBonus},

      // The same request chain continues into an honorific subsidiary
      // (読んで+おくれ+やす). Without it the honorific prefix reading of the
      // benefactive's initial mora wins (お+くれ+やす).
      {EPOS::AuxBenefactive, EPOS::AuxHonorific, cost::kDoubleVeryStrongBonus},

      // AuxAspectIru → AuxPassive (い+られ in potential/passive) - moderate bonus
      // いられる = いる + られる (potential: can stay/be)
      // E.g., はいられない → は + い + られ + ない
      {EPOS::AuxAspectIru, EPOS::AuxPassive, cost::kModerateBonus},

      // AuxAspectIru → VerbShuushikei (い+ける) - penalty
      // Progressive auxiliary い cannot be followed by a new verb
      // Prevents て+い+ける from beating て+いける (potential of いく)
      {EPOS::AuxAspectIru, EPOS::VerbShuushikei, cost::kRare},

      // AuxCopulaDa → AuxTenseTa (だっ+た) - strong bonus
      {EPOS::AuxCopulaDa, EPOS::AuxTenseTa, cost::kStrongBonus},

      // AuxCopulaDa → AuxTenseMasu (あり+ます in であります) - strong bonus
      // Ensures で+あり+ます uses AuxCopulaDa for both で and あり
      {EPOS::AuxCopulaDa, EPOS::AuxTenseMasu, cost::kStrongBonus},

      // AuxTenseTa → AuxCopulaDesu (た+です) - moderate bonus for polite past
      // e.g., 長かっ+た+です, 美しかっ+た+です (adjective past polite)
      {EPOS::AuxTenseTa, EPOS::AuxCopulaDesu, cost::kModerateBonus},

      // =========================================================================
      // Auxiliary → Particle
      // =========================================================================

      // AuxCopulaDa → ParticleConj (な+ので, な+のに) - strong bonus
      // Ensures なので → な+ので over な+の+で
      // Without this, PART_準体→AUX_断定 bonus makes の+で(AUX) path win
      {EPOS::AuxCopulaDa, EPOS::ParticleConj, cost::kStrongBonus},

      // A copular predicate takes the causal particle directly (だ+から).
      // This retains 静か+だ+から instead of letting the sentence-initial
      // conjunction だから absorb the copula.
      {EPOS::AuxCopulaDa, EPOS::ParticleCase, cost::kStrongBonus},

      // A connective te-form followed by ある uses the lexical existential
      // verb, not a copular or determiner-homograph candidate (並べ+て+ある).
      {EPOS::ParticleConj, EPOS::AuxCopulaDa, cost::kUncommon},
      // Suspending a clause before a determiner needs punctuation, which is its
      // own boundary, so this joins the finite-verb and past-auxiliary rows
      // below at the same weight: without it the classical determiner takes the
      // mora that completes the focus particle (し+かの for しか+の).
      {EPOS::ParticleConj, EPOS::Determiner, cost::kSevere},

      // A copula can be followed by a binding particle: でこそ, でさえ,
      // ですら, でしか. This preserves the nominal-predicate boundary over
      // the unrelated polite-copula plus final-particle path (でし+か).
      {EPOS::AuxCopulaDa, EPOS::ParticleBinding, cost::kVeryStrongBonus},

      // AuxTenseTa → Noun/Pronoun (食べた+人, 来た+彼) - moderate bonus for past+noun
      // POS-level AUX→NOUN=0.5 penalty is too harsh for this natural connection
      {EPOS::AuxTenseTa, EPOS::Noun, cost::kModerateBonus},
      {EPOS::AuxTenseTa, EPOS::Pronoun, cost::kModerateBonus},
      {EPOS::AuxTenseTa, EPOS::NounFormal, cost::kStrongBonus},

      // Past auxiliary → adverbial particle (た+ばかり, た+だけ, た+ほど).
      // This grammatical chain must outrank a sequence of short particles.
      {EPOS::AuxTenseTa, EPOS::ParticleAdverbial, cost::kStrongBonus},

      // A completed clause can be followed directly by a coordinating
      // conjunction even when punctuation is omitted (確認し+た+従って…).
      {EPOS::AuxTenseTa, EPOS::Conjunction, cost::kDoubleVeryStrongBonus},

      // AuxTenseTa → ParticleFinal (た+ね/よ) - minor bonus
      {EPOS::AuxTenseTa, EPOS::ParticleFinal, cost::kMinorBonus},

      // AuxTenseMasu → ParticleFinal (ます+ね/よ) - minor bonus
      {EPOS::AuxTenseMasu, EPOS::ParticleFinal, cost::kMinorBonus},

      // AuxCopulaDesu → ParticleFinal (です+ね/よ) - minor bonus
      {EPOS::AuxCopulaDesu, EPOS::ParticleFinal, cost::kMinorBonus},

      // AuxCopulaDa → ParticleFinal (だ+ね/よ) - minor bonus
      {EPOS::AuxCopulaDa, EPOS::ParticleFinal, cost::kMinorBonus},

      // ParticleConj → ParticleFinal (ない+で+よ, 待って+て+ね) - a request or
      // prohibition ends on the conjunctive particle, and the closing particle
      // is the only thing that can follow it there.
      {EPOS::ParticleConj, EPOS::ParticleFinal, cost::kModerateBonus},

      // The negative auxiliary inflects like an i-adjective and closes a clause
      // the same way (行か+ない+か, 〜じゃ+ない+ね), so it earns the AdjBasic
      // bonus. Without it the homographic adjective ない wins that position on
      // the connection alone and takes the copula in front of it down with it.
      {EPOS::AuxNegativeNai, EPOS::ParticleFinal, cost::kModerateBonus},

  };
  applyRules(table, kRules, sizeof(kRules) / sizeof(kRules[0]));

  setNominalParticleCosts(table);

  static constexpr BigramRule kLexicalNominalRules[] = {
      // =========================================================================
      // Pronoun → Particles
      // =========================================================================
      // Pronoun → AuxCopulaDesu (何+です, これ+です) - moderate bonus
      // Pronouns naturally take polite copula; matches Noun→AuxCopulaDesu bonus
      {EPOS::Pronoun, EPOS::AuxCopulaDesu, cost::kModerateBonus},

      // Pronouns can directly govern a continuative predicate (何もかも+忘れ
      // た, どれ+を+選び). Keep a lexicalized pronoun from losing to an
      // internally segmented particle chain before the predicate.
      {EPOS::Pronoun, EPOS::VerbRenyokei, cost::kStrongBonus},

      // An interrogative pronoun cannot directly govern a continuative verb.
      // This preserves the indefinite particle boundary in 誰+か+いる.
      {EPOS::PronounInterrogative, EPOS::VerbRenyokei, cost::kAlmostNever},

      // Pronoun → Adverb penalty (何+もし should be 何+も+し, not 何+もし(ADV))
      // Pronouns are followed by particles, not adverbs. PRON→ADV is rarely valid.
      {EPOS::Pronoun, EPOS::Adverb, cost::kRare},

      // =========================================================================
      // Determiner → Noun (連体詞は名詞を修飾)
      // =========================================================================

      // Determiner → Noun (そんな+こと, こんな+話) - very strong bonus
      // Ensures そんなことない → そんな+こと+ない over そん+な+こと+ない
      // Ensures あんな+人 over あん+な+人 (NOUN→AUX_断定→NOUN chain has -2.5 total)
      {EPOS::Determiner, EPOS::Noun, kDeterminerNounBonus},
      {EPOS::Determiner, EPOS::NounFormal, kDeterminerNounBonus},
      {EPOS::Determiner, EPOS::NounProper, kDeterminerNounBonus},

      // A demonstrative or degree adverb attaches to the predicate it modifies,
      // so an interrogative argument between the two is the marked order. The
      // margin is deliberately negligible: the order is possible (もう+何+も), it
      // just must not outrank the copula chain the adverb's own adjectival-noun
      // reading takes (そう+な+ん+だ, not そう+なん+だ).
      {EPOS::Adverb, EPOS::PronounInterrogative, cost::kNegligible},

      // A degree determiner heads a quantity adverb (ほんの+少し, ごく+わずか),
      // where the head word is adverbial rather than nominal. Without a rule the
      // pair is charged the unlisted-connection penalty and the same surface's
      // nominalized reading wins on the determiner bonus alone.
      {EPOS::Determiner, EPOS::Adverb, cost::kModerateBonus},

      // A lexical noun followed by と+いう retains the quotative particle and
      // predicate boundary (希望+と+いう+より).  The fused attributive entry
      // is selected from contexts that can actually follow a determiner, not
      // by rewarding it immediately after every noun.
      {EPOS::Noun, EPOS::DeterminerQuotative, cost::kStrongBonus},

      // Formal nouns can take a sentence-final particle directly in colloquial
      // nominal predicates (どういうこと+だい, そんなこと+さ). Prefer this
      // grammatical boundary to a copula followed by an unrelated short token.
      {EPOS::NounFormal, EPOS::ParticleFinal, cost::kVeryStrongBonus},

      // Determiner → ParticleNo (という+の, こんな+の)
      // 準体助詞の follows determiners naturally (same grammatical slot as nouns)
      // Use same bonus as DET→NOUN so the の+は split path can compete
      {EPOS::Determiner, EPOS::ParticleNo, kDeterminerNounBonus},

      // Quotative determiners have the same attributive distribution as
      // ordinary determiners (ということ, っていう話).
      {EPOS::DeterminerQuotative, EPOS::Noun, kDeterminerNounBonus},
      {EPOS::DeterminerQuotative, EPOS::NounFormal, kDeterminerNounBonus},
      {EPOS::DeterminerQuotative, EPOS::NounProper, kDeterminerNounBonus},
      {EPOS::DeterminerQuotative, EPOS::ParticleNo, kDeterminerNounBonus},

      // The complementary requirement — that a quotative determiner is followed
      // by a nominal head at all — is a condition on the whole right-hand
      // category rather than on four of them, so it lives with the connection
      // rules (computeParticleDeterminerBonus) instead of being enumerated here.

      // Determiner → Adjective (その+薄暗い+部屋, この+大きい+建物)
      // Determiners modify adjective+noun combinations in Japanese
      // Uses same bonus as DET→NOUN to allow adjective path to compete
      {EPOS::Determiner, EPOS::AdjBasic, kDeterminerNounBonus},
      {EPOS::Determiner, EPOS::AdjRenyokei, kDeterminerNounBonus},

      // Determiner → Determiner (そんな+大きな, こんな+小さな) - strong bonus
      // Demonstrative determiners stack with descriptive ones; without this the
      // high POS-level DET→DET default (0.8) makes the fragment path win
      // (そんな大きな → そん(NOUN)+な(AUX_断定)+大きな).
      {EPOS::Determiner, EPOS::Determiner, cost::kStrongBonus},

      // ParticleCase → Determiner (rare; 連体詞 rarely follows case particles)
      // Determiners introduce a new modifier clause and don't follow が/を/に/と/から/etc.
      // Counteracts overly strong DET→NOUN bonus for verb-ambiguous hiragana DET like かかる
      // (e.g., 壁にかかる絵 should be VERB, not DET).
      {EPOS::ParticleCase, EPOS::Determiner, cost::kStrong},

      // AuxTenseTa → Determiner (past tense should not be followed by determiner)
      // Prevents over-greedy match of L1 DET like かの in `た+か+の` (e.g., 覚めたかのような).
      // The correct parse is た(past) + か(question particle) + の(particle).
      // Needs kSevere to outweigh the DET→NounFormal bonus (-2.5 for かの→よう).
      {EPOS::AuxTenseTa, EPOS::Determiner, cost::kSevere},

      // A finite verb cannot directly take a determiner. This preserves the
      // particle sequence in clause-final similatives such as ある+か+の+よう
      // instead of selecting the unrelated determiner かの.
      {EPOS::VerbShuushikei, EPOS::Determiner, cost::kSevere},

      // A continuative cannot take one either: suspending a clause on it needs
      // punctuation, which is its own boundary. Same かの over-match as the two
      // rows above, one paradigm cell over (なにが+し+かの for なに+が+しか+の).
      {EPOS::VerbRenyokei, EPOS::Determiner, cost::kSevere},

      // The formal copula である also cannot directly take a determiner.
      // In であるかのよう, retain the intervening final and nominalizing
      // particles rather than joining them as かの.
      {EPOS::AuxCopulaDa, EPOS::Determiner, cost::kSevere},

      // Pronoun → Determiner (pronoun does not directly take a determiner)
      // Prevents over-greedy match of L1 DET like かの in `いくつ+か+の` (e.g., いくつかの限界).
      // The correct parse is いくつ(pronoun) + か(particle) + の(particle).
      {EPOS::Pronoun, EPOS::Determiner, cost::kStrong},
      {EPOS::PronounInterrogative, EPOS::Determiner, cost::kAlmostNever},

      // =========================================================================
      // Noun → Verb (サ変動詞パターン)
      // =========================================================================

      // Noun → VerbRenyokei (得+し for サ変動詞 得する) - moderate bonus
      // This favors 名詞+し split over 名詞し as single token
      {EPOS::Noun, EPOS::VerbRenyokei, cost::kModerateBonus},

      // =========================================================================
      // Noun → Copula/Negative
      // =========================================================================

      // The surface だっ is also the voiced allomorph of the past auxiliary,
      // but a noun or na-adjective before it forms a copular predicate
      // (本+だっ+たら、静か+だっ+たら), never a verbal past form.
      {EPOS::Noun, EPOS::AuxTenseTa, cost::kAlmostNever},
      {EPOS::AdjNaAdj, EPOS::AuxTenseTa, cost::kAlmostNever},

      // Noun → AuxCopulaDesu (学生+です) - moderate bonus
      {EPOS::Noun, EPOS::AuxCopulaDesu, cost::kModerateBonus},

      // Noun → AuxNegativeNai (間違い+ない, 違い+ない) - moderate bonus
      // For idiomatic patterns meaning "certain" or "no doubt"
      {EPOS::Noun, EPOS::AuxNegativeNai, cost::kModerateBonus},

      // A noun cannot take the contracted negative auxiliary ん directly.
      // A nominal ん must arise as ParticleNo after its predicate boundary,
      // while 読んだ starts from a verb onbin candidate rather than 読+ん+だ.
      {EPOS::Noun, EPOS::AuxNegativeNu, cost::kAlmostNever},

      // The contracted/classical negative auxiliary requires a predicate
      // before it.  An unknown hiragana fragment must not become a fake
      // negative clause (あそ+ん+で); real forms begin at a verb mizenkei,
      // a polite auxiliary, or another licensed auxiliary category.
      {EPOS::Other, EPOS::AuxNegativeNu, cost::kAlmostNever},

      // Neither a nominalizer, a particle, nor an adjective stem can host the
      // contracted negative. These penalties keep ordinary hatsuonbin forms
      // (のんだ, とんだ, やすんで) on their verb + tense path.
      {EPOS::ParticleNo, EPOS::AuxNegativeNu, cost::kAlmostNever},
      {EPOS::ParticleQuote, EPOS::AuxNegativeNu, cost::kAlmostNever},
      {EPOS::ParticleCase, EPOS::AuxNegativeNu, cost::kAlmostNever},
      {EPOS::ParticleAdverbial, EPOS::AuxNegativeNu, cost::kAlmostNever},
      {EPOS::AdjStem, EPOS::AuxNegativeNu, cost::kAlmostNever},

      // The contracted volitional ん (from む) reads off the same irrealis form,
      // so it rejects the same hosts. Listing it beside the negative keeps one
      // host set for the two auxiliaries that share the mora.
      {EPOS::ParticleNo, EPOS::AuxVolitional, cost::kAlmostNever},
      {EPOS::ParticleQuote, EPOS::AuxVolitional, cost::kAlmostNever},
      {EPOS::ParticleCase, EPOS::AuxVolitional, cost::kAlmostNever},
      {EPOS::ParticleAdverbial, EPOS::AuxVolitional, cost::kAlmostNever},
      {EPOS::AdjStem, EPOS::AuxVolitional, cost::kAlmostNever},
      // A volitional auxiliary must attach to an inflecting predicate. This
      // blocks a stray hiragana fragment from posing as its irrealis stem
      // (そ+う in そうとも言える).
      {EPOS::Other, EPOS::AuxVolitional, cost::kAlmostNever},

      // An adjective stem cannot govern an object or case particle. This
      // preserves a competing lexical noun reading before the particle.
      {EPOS::AdjStem, EPOS::ParticleCase, cost::kAlmostNever},

      // The contracted negative requires the irrealis form of a verb. A
      // terminal verb candidate (や+す+んで, むす+んで) is not an alternative
      // analysis of a hatsuonbin form.
      {EPOS::VerbShuushikei, EPOS::AuxNegativeNu, cost::kAlmostNever},

      // Neither a conjunctive particle nor a terminal volitional auxiliary can
      // introduce a causative auxiliary. This blocks fabricated chains such
      // as や+す+んで and む+す+んで without affecting causative irrealis forms.
      {EPOS::ParticleConj, EPOS::AuxCausative, cost::kAlmostNever},
      {EPOS::AuxVolitional, EPOS::AuxCausative, cost::kAlmostNever},

      // A conjunctive particle cannot directly introduce the volitional
      // auxiliary. This prevents manner adverbs such as ちゃんと from being
      // split into ちゃ + ん + と while retaining verb-irrealis + ん chains.
      {EPOS::ParticleConj, EPOS::AuxVolitional, cost::kAlmostNever},

      // The volitional/conjectural auxiliary attaches to an irrealis form, so
      // it cannot follow a bare nominal either. Without this a run of kanji
      // absorbs a following verb stem and leaves its ending behind
      // (本読む → 本読 + む). The literary register does host the conjectural
      // on a nominal predicate with an elided copula (確認+らむ), which is why
      // this stays a penalty rather than a prohibition.
      {EPOS::Noun, EPOS::AuxVolitional, cost::kStrong},

      // A verb irrealis form cannot directly precede an independent onbin
      // form. This rejects fabricated chains such as よろ + こん + で while
      // preserving a single lexical onbin stem (よろこん + で).
      {EPOS::VerbMizenkei, EPOS::VerbOnbinkei, cost::kAlmostNever},

      // The volitional closes its predicate, so what follows it is a particle
      // or another auxiliary -- never a fresh verb stem. Without this the final
      // particles behind it are swallowed by a euphonic stem that happens to
      // spell them (行こう+か+な+って → 行こう + かなっ + て).
      {EPOS::AuxVolitional, EPOS::VerbOnbinkei, cost::kAlmostNever},
      {EPOS::AuxVolitional, EPOS::VerbRenyokei, cost::kAlmostNever},

      // Noun → AuxCausative (色褪+せる) - strong penalty
      // Causative auxiliary only follows verb mizenkei, never nouns
      {EPOS::Noun, EPOS::AuxCausative, cost::kStrong},

      // Noun → AuxPassive (色褪+れる) - strong penalty
      // Passive auxiliary only follows verb mizenkei, never nouns
      {EPOS::Noun, EPOS::AuxPassive, cost::kStrong},

      // An adverb is no more a 未然形 host than a noun is, and the same two rows
      // are missing on that side: 一切+れ steals the okurigana off a counter
      // (一切れ), 少々+せる off a verb stem.
      {EPOS::Adverb, EPOS::AuxPassive, cost::kStrong},
      {EPOS::Adverb, EPOS::AuxCausative, cost::kStrong},
      // The past auxiliary attaches to a predicate's onbin stem, so an adverb
      // cannot carry it. The pair is reachable only because だ spells both the
      // past auxiliary and the copula, and a stem that is both an adverb and a
      // na-adjective then takes its adverb reading in a predicate slot
      // (たいへん+だ).
      {EPOS::Adverb, EPOS::AuxTenseTa, cost::kSevere},

      // Noun → aspect auxiliary いる/くる (驚+い, 先生+き): aspect attaches only to a
      // te-form, never a bare noun (食べて+いた, 走って+きた). Prevents 間続+い+た and
      // overcomes the DET→NOUN bonus on prefix compounds like 先生.
      {EPOS::Noun, EPOS::AuxAspectIru, cost::kSevere},
      {EPOS::Noun, EPOS::AuxAspectKuru, cost::kProhibitive},
      // The progressive auxiliary い also requires a preceding te-form. An
      // unclassified span, including retained content symbols, cannot license
      // it as a predicate continuation.
      {EPOS::Other, EPOS::AuxAspectIru, cost::kAlmostNever},

      // Binding particle (は/も) → aspect auxiliary: aspect attaches only to a
      // te-form, so は/も before き/いく/いる is a mis-parse (ではきもの → で+は+きもの).
      {EPOS::ParticleTopic, EPOS::AuxAspectKuru, cost::kProhibitive},
      {EPOS::ParticleTopic, EPOS::AuxAspectIku, cost::kProhibitive},
      {EPOS::ParticleTopic, EPOS::AuxAspectIru, cost::kSevere},

      // NaAdj → AuxCopulaDa (静か+だ) - strong bonus
      {EPOS::AdjNaAdj, EPOS::AuxCopulaDa, cost::kVeryStrongBonus},

      // NaAdj → AuxCopulaDesu (静か+です) - strong bonus
      {EPOS::AdjNaAdj, EPOS::AuxCopulaDesu, cost::kVeryStrongBonus},

      // Adverb → AuxCopulaDa/Desu - penalty: adverbs modify verbs/adjectives, they
      // don't directly take copula (そうです: そう should be na-adjective, not adverb).
      {EPOS::Adverb, EPOS::AuxCopulaDa, cost::kStrong},
      {EPOS::Adverb, EPOS::AuxCopulaDesu, cost::kStrong},

      // AuxCopulaDa → Noun (さすがな+人, 静かな+部屋) - strong bonus
      // Copula な(連体形 of だ) + Noun is the na-adjective attributive pattern
      {EPOS::AuxCopulaDa, EPOS::Noun, cost::kStrongBonus},

      // AuxCopulaDa → NounFormal (な+もの, な+こと) - strong bonus
      // Ensures 妙なもの → 妙+な+もの over 妙+な+も+の
      // Without this, AUX_断定→PART_係(-0.5) makes も path win over もの
      {EPOS::AuxCopulaDa, EPOS::NounFormal, cost::kStrongBonus},

      // The attributive copula precedes the nominalizer in な+の+か.
      {EPOS::AuxCopulaDa, EPOS::ParticleNo, cost::kStrongBonus},

      // AdjStem → Suffix (な+さ in なさそう) - strong bonus for nominalization
      // This favors な(ADJ stem of ない) + さ(nominalization suffix) over さ(する mizenkei)
      {EPOS::AdjStem, EPOS::Suffix, cost::kVeryStrongBonus},

      // The conjecture auxiliary らしい nominalizes through its stem:
      // 本らしさ → 本 + らし + さ.
      {EPOS::AuxConjectureRashii, EPOS::Suffix, cost::kVeryStrongBonus},

      // The attributive form of らしい modifies a following noun:
      // 本らしい本 → 本 + らしい + 本.
      {EPOS::AuxConjectureRashii, EPOS::Noun, cost::kMinorBonus},

      // The conjectural auxiliary completes a predicate and cannot directly
      // take a case particle or a copula. This leaves a productive nominalized
      // continuative available for compounds such as 山暮らしを and 日暮らしだ.
      {EPOS::AuxConjectureRashii, EPOS::ParticleCase, cost::kAlmostNever},
      {EPOS::AuxConjectureRashii, EPOS::AuxCopulaDa, cost::kAlmostNever},

      // Na-adjective stem → suffix (豊か+さ, 静か+さ) nominalizes the
      // adjective; the homographic さ cannot be the suru irrealis here.
      {EPOS::AdjNaAdj, EPOS::Suffix, cost::kVeryStrongBonus},

      // VerbRenyokei → Suffix (遅れ+がち, 疲れ+気味) - very strong bonus
      // This favors verb renyokei + suffix pattern over merged tokens
      {EPOS::VerbRenyokei, EPOS::Suffix, cost::kVeryStrongBonus},

      // Classical negative ず + completion suffix (見+ず+じまい).
      {EPOS::AuxNegativeNu, EPOS::Suffix, cost::kStrongBonus},

      // A derivational suffix cannot directly take the contracted negative
      // auxiliary. This prevents a name suffix from being fragmented into a
      // one-mora suffix plus ん, while verbal irrealis stems retain their
      // dedicated VerbMizenkei → AuxNegativeNu connection.
      {EPOS::Suffix, EPOS::AuxNegativeNu, cost::kAlmostNever},

      // Nor the plain negative auxiliary, for the same reason: a suffix derives
      // a nominal, and a nominal is negated by the independent adjective. The
      // adjective reading of the same surface is promoted alongside, so the
      // negation does not fall apart into a final particle (負け+っこ+ない).
      {EPOS::Suffix, EPOS::AuxNegativeNai, cost::kRare},
      {EPOS::Suffix, EPOS::AdjBasic, cost::kStrongBonus},

      // VerbRenyokei → recent-completion suffix (焼き+たて, 作り+たて).
      // This productive suffix competes directly with the past た + connective
      // て chain, so it needs a stronger lexicalized grammatical connection.
      {EPOS::VerbRenyokei, EPOS::SuffixRecentCompletion, cost::kTripleVeryStrongBonus},

      // Tendency suffix follows a verb's continuative form (忘れ+がち,
      // 読み+がち), rather than a homographic nominalized verb stem.
      {EPOS::VerbRenyokei, EPOS::SuffixTendency, cost::kDoubleVeryStrongBonus},

      // Nominal completion-state suffix (確認+済み, 承認+済み).
      {EPOS::Noun, EPOS::SuffixRecentCompletion, cost::kStrongBonus},
  };
  applyRules(table, kLexicalNominalRules, sizeof(kLexicalNominalRules) / sizeof(kLexicalNominalRules[0]));

  // The causative and passive auxiliaries select a verb's irrealis form and
  // nothing else, so no particle of any role can host one. Stating that over
  // the particle range keeps the rule from being rediscovered one particle at
  // a time — it was previously written for 接続助詞 alone, which left の+せる
  // free to outscore the kana verb のせる that spans it.
  constexpr uint8_t kNoParticleHost = encodeCost(cost::kAlmostNever);
  for (size_t epos = static_cast<size_t>(EPOS::ParticleCase); epos <= static_cast<size_t>(EPOS::ParticleBinding);
       ++epos) {
    table[epos][static_cast<size_t>(EPOS::AuxCausative)] = kNoParticleHost;
    table[epos][static_cast<size_t>(EPOS::AuxPassive)] = kNoParticleHost;
  }
}  // namespace suzume::analysis::bigram_rules

}  // namespace suzume::analysis::bigram_rules
