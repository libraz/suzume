#include "bigram_table_internal.h"

namespace suzume::analysis::bigram_rules {

using EPOS = core::ExtendedPOS;
namespace cost = bigram_cost;

void setParticleAndLexicalCosts(BigramMatrix& table) {
  static constexpr BigramRule kRules[] = {
      // =========================================================================
      // Particle → Various (Particles can connect to many things)
      // =========================================================================

      // ParticleAdverbial → ParticleCase (だけ+で, ばかり+に, ほど+に) - very strong bonus
      // Without this, PART_副→VERB_連用 bonus makes で(出る) beat で(格助詞) after だけ
      // Needs to overcome: base bigram diff (0.5-0.2=0.3) + VERB_連用 bonus (-0.8)
      {EPOS::ParticleAdverbial, EPOS::ParticleCase, cost::kVeryStrongBonus},

      // An adverbial particle can be followed by a conditional particle
      // (くらい+なら, ほど+なら, だけ+なら). Prefer the grammatical particle
      // chain over a homographic mizenkei verb.
      {EPOS::ParticleAdverbial, EPOS::ParticleConj, cost::kStrongBonus},

      // Focus particle → polite copula (だけ+です, さえ+です). A focus
      // particle can close a nominal predicate, so prefer the copula over the
      // unrelated で + す segmentation.
      {EPOS::ParticleAdverbial, EPOS::AuxCopulaDesu, cost::kVeryStrongBonus},
      {EPOS::ParticleBinding, EPOS::AuxCopulaDesu, cost::kVeryStrongBonus},

      // Focus particles can also precede the plain copula (のみ+である,
      // だけ+である). This keeps the copular boundary over a verb homograph.
      {EPOS::ParticleAdverbial, EPOS::AuxCopulaDa, cost::kVeryStrongBonus},

      // ParticleAdverbial → ParticleNo (など+の, まで+の, ばかり+の)
      // Very strong bonus: adverbial particle + の is extremely natural.
      // Needs to overcome DET→NOUN bonus (-2.5) when competing with な+どの path.
      {EPOS::ParticleAdverbial, EPOS::ParticleNo, cost::kVeryStrongBonus},

      // A case-marked modifier can be followed by a predicative adjective
      // (これまでに+ない方法). Prefer this grammatical boundary to a
      // dictionary noun that happens to span the adjective and next kanji.
      {EPOS::ParticleCase, EPOS::AdjBasic, cost::kVeryStrongBonus},

      // A bare na-adjective cannot modify an independent noun without an
      // adnominal copula. Prefer the competing nominal compound boundary
      // (自然+言語) over attaching the adjective directly.
      {EPOS::AdjNaAdj, EPOS::Noun, cost::kStrong},

      // ParticleAdverbial → VerbRenyokei (かも+しれ in かもしれない, など+あり, でも+あり)
      // - strong bonus. Favors かも+しれ+ない over か+もし+れない and keeps a real
      //   verb renyokei after a subsidiary particle. A single-mora renyokei after
      //   this particle is a false over-split (し+かね misread as しか+ね←寝る) and
      //   is penalized by surface length in scorer_connection_cost.cpp.
      {EPOS::ParticleAdverbial, EPOS::VerbRenyokei, cost::kStrongBonus},

      // An adverb can be followed by a focus particle (そう+かも, もっとも+でも).
      // Prefer that grammatical boundary over a homographic short verb.
      // The binding particles are the other half of that class and scope over an
      // adverb just as readily (ゆっくり+こそ, ちょっと+さえ); without the row an
      // adverb spelled like a determiner plus a formal noun loses its own span,
      // because the formal noun collects a binding-particle bonus the adverb has
      // no counterpart for (そのまま+こそ misread as その+まま+こそ).
      {EPOS::Adverb, EPOS::ParticleAdverbial, cost::kVeryStrongBonus},
      {EPOS::Adverb, EPOS::ParticleBinding, cost::kVeryStrongBonus},

      // The regional polite auxiliary follows an invitation formal noun
      // (おいで+なんし).
      {EPOS::NounFormal, EPOS::AuxKuruwaPolite, cost::kStrongBonus},

      // ParticleAdverbial → VerbShuushikei (でも+行く) - strong bonus
      // This favors でも+行く over で+も+行く
      {EPOS::ParticleAdverbial, EPOS::VerbShuushikei, cost::kStrongBonus},

      // Adverbial particles scope naturally over an evaluative adjective
      // (何でも+いい, どちらでも+よい).
      {EPOS::ParticleAdverbial, EPOS::AdjBasic, cost::kStrongBonus},
      {EPOS::ParticleAdverbial, EPOS::AdjNaAdj, cost::kStrongBonus},

      // A topic particle directly scopes a negative predicate (は+ない,
      // では+なく). Prefer the auxiliary reading over the homographic
      // adjective continuation.
      {EPOS::ParticleTopic, EPOS::AuxNegativeNai, cost::kMinorBonus},

      // ParticleCase → Adverb (か+もし) - moderate penalty
      // This discourages splitting かもしれない as か+もし+れない
      {EPOS::ParticleCase, EPOS::Adverb, cost::kRare},

      // A case-marked phrase requires a predicate; a discourse conjunction
      // cannot directly consume that slot. This keeps a dictionary predicate
      // whose prefix is a conjunction homograph intact (を+さておく).
      {EPOS::ParticleCase, EPOS::Conjunction, cost::kSevere},

      // A final particle closes a predicate and cannot directly introduce an
      // adverb. This is an ExtendedPOS-only rule, so keep it in the table
      // rather than in Scorer::connectionCost().
      {EPOS::ParticleFinal, EPOS::Adverb, cost::kAlmostNever},

      // A conjunction can be followed by a binding particle (だけど+も).
      {EPOS::Conjunction, EPOS::ParticleBinding, cost::kDoubleVeryStrongBonus},

      // A terminal predicate can be followed by a binding particle in the
      // exclusive construction (する+しか+ない, 見る+しか+ない).
      {EPOS::VerbShuushikei, EPOS::ParticleBinding, cost::kVeryStrongBonus},

      // An irrealis verb form selects auxiliaries, not a particle. The stem is
      // unfinished, so nothing that attaches to a completed phrase can follow
      // it — a case particle no more than a focus or topic marker
      // (昔+の+まま+で, never 昔+のま+まで). The conjunctive class is
      // deliberately absent: the classical conditional really is 未然形+ば.
      {EPOS::VerbMizenkei, EPOS::ParticleCase, cost::kAlmostNever},
      {EPOS::VerbMizenkei, EPOS::ParticleAdverbial, cost::kAlmostNever},
      {EPOS::VerbMizenkei, EPOS::ParticleBinding, cost::kAlmostNever},
      {EPOS::VerbMizenkei, EPOS::ParticleTopic, cost::kAlmostNever},
      // The nominalizer belongs to the same group: の and its contraction ん
      // take an attributive form, so an irrealis in front of one means the
      // stem was cut short of its own onbin cell (分かん+ない, not 分か+ん+ない).
      {EPOS::VerbMizenkei, EPOS::ParticleNo, cost::kAlmostNever},

      // A derivational suffix completes a nominal unit and cannot directly
      // take a predicate connective. Case-marked nominalizations such as
      // 高さ+で remain governed by Suffix -> ParticleCase.
      {EPOS::Suffix, EPOS::ParticleConj, cost::kAlmostNever},
      {EPOS::SuffixRecentCompletion, EPOS::ParticleConj, cost::kAlmostNever},
      {EPOS::SuffixRecentCompletion, EPOS::AuxAspectIru, cost::kAlmostNever},
      {EPOS::SuffixRecentCompletion, EPOS::AuxTenseTa, cost::kAlmostNever},

      // The aspectual いく auxiliary follows a te-form, not a bare renyokei.
      {EPOS::VerbRenyokei, EPOS::AuxAspectIku, cost::kAlmostNever},

      // A case particle cannot be followed directly by a sentence-final
      // particle (を+な is a fragment of a longer predicate).
      {EPOS::ParticleCase, EPOS::ParticleFinal, cost::kAlmostNever},

      // A determiner directly modifies an i-adjective stem nominalized by a
      // following suffix (この+大き+さ).
      {EPOS::Determiner, EPOS::AdjStem, cost::kStrongBonus},

      // The conjecture auxiliary productively follows demonstrative and
      // personal pronouns (それ+らしい, 彼+らしい).
      {EPOS::Pronoun, EPOS::AuxConjectureRashii, cost::kExtremeBonus},

      // A terminal volitional auxiliary cannot directly precede the past
      // auxiliary.
      {EPOS::AuxVolitional, EPOS::AuxTenseTa, cost::kAlmostNever},

      // The contracted negative followed by the copula is the productive
      // んで construction (読まんで).
      {EPOS::AuxNegativeNu, EPOS::AuxCopulaDa, cost::kDoubleVeryStrongBonus},

      // ParticleCase → Noun (が+学生) - neutral
      {EPOS::ParticleCase, EPOS::Noun, cost::kNeutral},

      // ParticleCase → VerbShuushikei (を+食べる) - neutral
      {EPOS::ParticleCase, EPOS::VerbShuushikei, cost::kNeutral},

      // ParticleCase → VerbMizenkei (に+なら+ない, を+読ま+ない) -
      // moderate bonus for ordinary negative predicates.
      {EPOS::ParticleCase, EPOS::VerbMizenkei, cost::kModerateBonus},

      // A case particle can introduce an onbin predicate (荷物を受け取っ+た).
      // This is a complete predicate boundary, unlike the renyokei case that
      // remains deliberately unscored for quotative constructions.
      {EPOS::ParticleCase, EPOS::VerbOnbinkei, cost::kVeryStrongBonus},

      // Conditional predicates introduce concessive clauses (しかれ+ども,
      // 読め+ども) and must outrank a homographic particle-plus-auxiliary path.
      {EPOS::VerbKateikei, EPOS::ParticleConj, cost::kStrongBonus},

      // A renyokei immediately before a case particle normally functions as a
      // nominalization (香り+を, 読み+を, 流れ+に). Left context can override this
      // for purpose constructions such as 本を買いに行く.
      {EPOS::VerbRenyokei, EPOS::ParticleCase, cost::kStrong},

      // A continuative verb immediately before の normally denotes a lexicalized
      // nominal form (思い+の, 帰り+の), whose search unit is a noun. Finite verbs
      // remain available for the productive nominalizer construction (食べる+の).
      {EPOS::VerbRenyokei, EPOS::ParticleNo, cost::kStrong},

      // A contrastive topic can attach to a continuative verb in emphatic
      // negation (減り+は+し+ない). Preserve the verbal reading alongside
      // ordinary nominalized renyokei uses.
      {EPOS::VerbRenyokei, EPOS::ParticleTopic, cost::kStrongBonus},

      // ParticleTopic → ParticleNo (は+ん) - prohibitive
      // The binding particle attaches to a nominal phrase, while the
      // nominalizer needs the attributive form of a predicate, so neither can
      // host the other. The pair only ever appears where a hiragana run was cut
      // into closed-class fragments (ごはん read as ご+は+ん).
      {EPOS::ParticleTopic, EPOS::ParticleNo, cost::kAlmostNever},

      // ParticleTopic/ParticleCase → Pronoun (は+いつ, は+どこ, に+何, で+誰)
      // Particles naturally precede pronouns in questions and relative clauses
      // は+いつ, も+何, に+どこ are very common patterns
      {EPOS::ParticleTopic, EPOS::Pronoun, cost::kModerateBonus},
      {EPOS::ParticleCase, EPOS::Pronoun, cost::kMinorBonus},
      // Enumerative particles can introduce the first member of a list
      // (やら+これ+やら), where the homographic verb reading is impossible.
      {EPOS::ParticleAdverbial, EPOS::Pronoun, cost::kMinorBonus},

      // ParticleTopic → VerbShuushikei (は+食べる) - neutral
      {EPOS::ParticleTopic, EPOS::VerbShuushikei, cost::kNeutral},

      // ParticleTopic → VerbRenyokei (は+あり in はありますか) - minor bonus
      // Helps は+あり+ます beat はあり+ます (particle-starting verb)
      // Note: ParticleCase → VerbRenyokei is intentionally not added to avoid
      // breaking という patterns (と is ParticleCase, いう is VerbRenyokei)
      {EPOS::ParticleTopic, EPOS::VerbRenyokei, cost::kMinorBonus},

      // A topicalized phrase can introduce a hypothetical verb form
      // (と+は+言え, 本+は+食べれ+ば). This keeps the particle boundary
      // available before an independent following predicate.
      {EPOS::ParticleTopic, EPOS::VerbKateikei, cost::kStrongBonus},

      // A topicalized predicate can begin with its mizenkei before negation
      // (は+なら+ない, は+食べ+ない). Preserve that boundary over a generated
      // verb that absorbs the topic particle.
      {EPOS::ParticleTopic, EPOS::VerbMizenkei, cost::kStrongBonus},

      // A topicalized adjective predicate can likewise begin with its mizenkei
      // before negation or conjecture (本+は+高から+ず, 本+は+高かろ+う). Without
      // the counterpart of the verb entry above, the same topic boundary favors
      // a fabricated godan verb (高かる) over the verified adjective.
      {EPOS::ParticleTopic, EPOS::AdjMizenkei, cost::kStrongBonus},

      // ParticleTopic → AdjBasic (は+良い, も+美しい, は+ない) - very
      // strong bonus. A topic particle can directly introduce a terminal
      // adjective; this prevents the false noun はな + auxiliary い path.
      {EPOS::ParticleTopic, EPOS::AdjBasic, cost::kVeryStrongBonus},

      // ParticleConj → VerbShuushikei (食べて+帰る) - neutral.
      // A te-form productively connects coordinate predicates, so it must not
      // lose to a noun + copula reinterpretation merely because the following
      // verb is lexical rather than an auxiliary.
      {EPOS::ParticleConj, EPOS::VerbShuushikei, cost::kNeutral},

      // A focus particle can precede a finite predicate (にすら+ある,
      // まで+届く). Prefer the grammatical particle boundary over an
      // unknown noun that absorbs the particle sequence.
      {EPOS::ParticleBinding, EPOS::VerbShuushikei, cost::kVeryStrongBonus},

      // The attributive closes the same slot: it heads the noun phrase a focus
      // particle scopes over (まで+届く+荷物), and in classical Japanese it is the
      // form a 係助詞 actually governs (水+なむ+流るる). Without this row the
      // finite bonus above is the only one on offer, so a fabricated 終止形 that
      // spells the attributive wins over the attributive cell itself.
      {EPOS::ParticleBinding, EPOS::VerbRentaikei, cost::kVeryStrongBonus},

      // The same holds for the non-finite stems that carry a following
      // auxiliary (しか+でき+ない, しか+読ま+ない, さえ+すれ+ば). Without these
      // rows the particle's own first mora is cheap enough to be re-read as a
      // separate closed-class token (しか split into the suru continuative and
      // the interrogative か).
      {EPOS::ParticleBinding, EPOS::VerbRenyokei, cost::kVeryStrongBonus},
      {EPOS::ParticleBinding, EPOS::VerbMizenkei, cost::kVeryStrongBonus},

      // Whether a conjunctive particle may take a binding particle depends on
      // its surface: productive て/で combinations are rewarded by the
      // surface-aware scorer.  Keep the category pair neutral here so short
      // homographs such as や+すら do not split ordinary lexical words.
      {EPOS::ParticleConj, EPOS::ParticleBinding, cost::kNeutral},

      // A connective particle can be topicalized productively (て+は,
      // て+も). Preserve the predicate boundary instead of preferring an
      // overlapping compound case-particle candidate.
      {EPOS::ParticleConj, EPOS::ParticleTopic, cost::kStrongBonus},

      // ParticleConj → AuxAspectIru (て+いる) - strong bonus for the aspectual construction
      // Allows 食べ+て+いる to beat unified 食べて+いる path
      {EPOS::ParticleConj, EPOS::AuxAspectIru, cost::kStrongBonus},

      // ParticleConj → AuxAspectShimau (て+しまう) - very strong bonus
      {EPOS::ParticleConj, EPOS::AuxAspectShimau, cost::kVeryStrongBonus},

      // ParticleConj → AuxAspectOku (て+おく) - strong bonus
      {EPOS::ParticleConj, EPOS::AuxAspectOku, cost::kStrongBonus},

      // A quotative clause can be followed by the preparative auxiliary
      // (〜っておいて). Keep that auxiliary chain ahead of a fabricated
      // katakana onbin verb that would consume the quotation's っ.
      {EPOS::ParticleQuote, EPOS::AuxAspectOku, cost::kVeryStrongBonus},

      // ParticleConj → AuxAspectMiru (て+みる) - strong bonus
      {EPOS::ParticleConj, EPOS::AuxAspectMiru, cost::kVeryStrongBonus},

      // ParticleConj → AuxBenefactive (て+あげ/もらえる) - very strong
      // bonus. The closed benefactive paradigm must outrank a homographic
      // independent verb when the preceding te-form already licenses it.
      {EPOS::ParticleConj, EPOS::AuxBenefactive, cost::kVeryStrongBonus},

      // Emphatic も preserves the te-form attachment in 〜てもみる/〜でもみる.
      // AuxAspectMiru candidates are context-gated during generation, so this does
      // not license a standalone particle + lexical みる sequence.
      {EPOS::ParticleAdverbial, EPOS::AuxAspectMiru, cost::kVeryStrongBonus},
      {EPOS::ParticleTopic, EPOS::AuxAspectMiru, cost::kVeryStrongBonus},

      // ParticleConj → AuxAspectIku (て+いく) - strong bonus
      {EPOS::ParticleConj, EPOS::AuxAspectIku, cost::kStrongBonus},

      // ParticleConj → AuxAspectKuru (て+くる) - strong bonus
      {EPOS::ParticleConj, EPOS::AuxAspectKuru, cost::kStrongBonus},

      // =========================================================================
      // Penalties: Invalid or Rare Connections
      // =========================================================================

      // Suffix → Adverb (さ+そう) - strong penalty
      // Prevents なさそう → な + さ(SUFFIX) + そう(ADV) over correct な + さ + そう(AUX)
      // Suffix + Adverb is grammatically unusual; そう after さ is AuxAppearance
      {EPOS::Suffix, EPOS::Adverb, cost::kVeryRare},

      // AuxVolitional → ParticleCase (う+と quotative) - minor bonus
      // Volitional + と is common (書こうと思う, 食べようとする)
      // Keep bonus small to avoid changing よう POS in 次のように (NounFormal vs AuxVolitional)
      {EPOS::AuxVolitional, EPOS::ParticleCase, cost::kMinorBonus},

      // A volitional auxiliary can license a concessive conjunctive particle
      // (降ろ+う+とも).  The superficially similar として is ParticleCase, so
      // its quotative boundary remains governed by the case-particle path.
      {EPOS::AuxVolitional, EPOS::ParticleConj, cost::kModerateBonus},

      // AuxAspectOku → AuxVolitional (とい+う) - strong penalty
      // Prevents とい+う from beating という (quotative determiner)
      // とい (contracted ておく form) + う (volitional) is grammatically invalid
      {EPOS::AuxAspectOku, EPOS::AuxVolitional, cost::kVeryRare},

      // AuxAspectOku → ParticleQuote (とい+って) - strong penalty
      // Prevents とい+って from beating と+いっ+て (と言って)
      // とい (contracted ておく form) + って (quote particle) is unlikely in this context
      {EPOS::AuxAspectOku, EPOS::ParticleQuote, cost::kVeryRare},

      // VerbShuushikei → AuxTenseMasu (食べる+ます) - prohibitive
      // (ます attaches to renyokei, not shuushikei)
      {EPOS::VerbShuushikei, EPOS::AuxTenseMasu, cost::kAlmostNever},

      // VerbShuushikei → AuxDesireTai (食べる+たい) - prohibitive
      {EPOS::VerbShuushikei, EPOS::AuxDesireTai, cost::kAlmostNever},

      // VerbShuushikei → AuxTenseTa (食べる+た) - prohibitive
      {EPOS::VerbShuushikei, EPOS::AuxTenseTa, cost::kAlmostNever},

      // AuxTenseTa → AuxTenseTa (た+た) - prohibitive
      {EPOS::AuxTenseTa, EPOS::AuxTenseTa, cost::kAlmostNever},

      // AuxTenseTa → AuxTenseMasu (た+ます) - prohibitive
      {EPOS::AuxTenseTa, EPOS::AuxTenseMasu, cost::kAlmostNever},

      // AuxTenseTa → AuxNegative (た+ない/ん) - prohibitive
      // Past tense cannot be followed by negation; correct order is negation→past
      {EPOS::AuxTenseTa, EPOS::AuxNegativeNai, cost::kAlmostNever},
      {EPOS::AuxTenseTa, EPOS::AuxNegativeNu, cost::kAlmostNever},

      // AuxTenseTa → AuxAspectKuru (た+き) - prohibitive
      // Prevents いただき → い+た+だ+き (きた split creates standalone き entry)
      {EPOS::AuxTenseTa, EPOS::AuxAspectKuru, cost::kAlmostNever},

      // AuxTenseTa → AuxGaru (た+がる) - strong penalty
      // Desiderative がる attaches to renyokei/stem (食べ+たがる, 怖+がる), never to
      // past た. Prevents 食べたがる → 食べ+た+がる over 食べ+たがる (願望 auxiliary).
      {EPOS::AuxTenseTa, EPOS::AuxGaru, cost::kStrong},

      // AuxCopulaDa → AuxAspectKuru (だ+き) - prohibitive
      // Prevents いただき → い+た+だ+き
      {EPOS::AuxCopulaDa, EPOS::AuxAspectKuru, cost::kAlmostNever},

      // ParticleNo → AuxTenseTa (ん/の+た) - prohibitive
      // Nominalizer の/ん is followed by copula (のだ/のです), not past tense
      {EPOS::ParticleNo, EPOS::AuxTenseTa, cost::kAlmostNever},

      // Same slot, same reason: a conjunction joins clauses and cannot fill the
      // copula the nominalizer requires, so 〜んだけど / 〜のだが stay as ん+だ+けど
      // rather than collapsing the copula into the homographic conjunction.
      {EPOS::ParticleNo, EPOS::Conjunction, cost::kAlmostNever},

      // A quotative 連体詞 quotes the clause ahead of the noun it introduces, and
      // the nominalizer has already closed that clause into a nominal, so the two
      // cannot stack (なんという stays なんと+いう). The plain 連体詞 row is
      // deliberately absent: the classical comparison ごとき takes exactly the
      // nominal the nominalizer produces (かくのごとき).
      {EPOS::ParticleNo, EPOS::DeterminerQuotative, cost::kAlmostNever},

      // ParticleFinal → VerbShuushikei (ね+食べる) - strong penalty
      // (sentence-final particles rarely continue to verbs)
      {EPOS::ParticleFinal, EPOS::VerbShuushikei, cost::kVeryRare},

      // A sentence-final particle cannot introduce the contracted negative
      // auxiliary. This preserves a following honorific suffix as a single
      // unit instead of fragmenting it into a final particle plus ん.
      {EPOS::ParticleFinal, EPOS::AuxNegativeNu, cost::kAlmostNever},

      // Nor the conjectural auxiliary, for the same reason: the particle has
      // already closed the clause, so a following む belongs to a predicate that
      // starts inside the run in front of it (花咲か+む, not 花咲+か+む).
      {EPOS::ParticleFinal, EPOS::AuxVolitional, cost::kAlmostNever},

      // ParticleFinal → VerbOnbinkei (な+いん) - prohibit
      // (prevents ないんだ → な+いん+だ over ない+ん+だ)
      {EPOS::ParticleFinal, EPOS::VerbOnbinkei, cost::kAlmostNever},

      // ParticleFinal → VerbMizenkei (な+さ) - strong penalty
      // (prevents なさそう → な(終助詞)+さ(未然)+そう over な(形容詞)+さ(接尾辞)+そう)
      {EPOS::ParticleFinal, EPOS::VerbMizenkei, cost::kVeryRare},

      // A sentence-final particle cannot govern a hypothetical predicate.
      // Otherwise ため+さ+なけれ+ば reopens the stem at さ and lets the
      // fabricated なける conditional collect the strong →ば connection.
      {EPOS::ParticleFinal, EPOS::VerbKateikei, cost::kAlmostNever},

      // The continuative belongs to the same paradigm as the cells above and
      // is barred for the same reason: the particle has already closed the
      // clause, so a predicate that opens a new one cannot follow it
      // (向かい+合わせ, not 向+かい+合わせ).
      {EPOS::ParticleFinal, EPOS::VerbRenyokei, cost::kVeryRare},

      // A sentence-final particle cannot introduce an independent adjective.
      // This preserves verb+auxiliary sequences such as そう+なる+まい over
      // a spurious そう+な+るまい segmentation.
      {EPOS::ParticleFinal, EPOS::AdjBasic, cost::kVeryRare},

      // AuxCopulaDa → VerbOnbinkei (な+いん) - prohibit
      // (prevents ないんだ → な+いん+だ over ない+ん+だ)
      {EPOS::AuxCopulaDa, EPOS::VerbOnbinkei, cost::kAlmostNever},

      // AuxCopulaDa → VerbMizenkei (だ+くさ) - strong penalty
      // Copula followed by verb mizenkei is grammatically unusual
      // Prevents 盛りだくさん → 盛り+だ+くさ+ん over dictionary entry
      {EPOS::AuxCopulaDa, EPOS::VerbMizenkei, cost::kVeryRare},

      // AuxCopulaDa → VerbRenyokei/VerbShuushikei - penalty for copula + general verb
      // E.g., 公園で遊ぶ should be NOUN+PART_格+VERB, not NOUN+AUX_断定+VERB
      // Copula 「で」 rarely followed by general verbs (usually followed by ある/ない/ございます)
      // This helps PART_格(で)+VERB win over AUX_断定(で)+VERB
      {EPOS::AuxCopulaDa, EPOS::VerbRenyokei, cost::kMinor},
      {EPOS::AuxCopulaDa, EPOS::VerbShuushikei, cost::kMinor},

      // Two sentence-final particles cannot form an unqualified boundary.
      // Surface-specific valid stacks are restored by connection rules.
      {EPOS::ParticleFinal, EPOS::ParticleFinal, cost::kStrong},

      // ParticleFinal → ParticleNo (か+の) - moderate bonus (indefinite pronoun pattern)
      // いくつかの, 何かの, 誰かの, どれかの - か functions as indefinite marker, not sentence-ender
      {EPOS::ParticleFinal, EPOS::ParticleNo, cost::kModerateBonus},

      // Nominalizer → sentence-final question (の+か). This is productive in
      // predicate questions such as 静かなのかな and prevents an unknown noun
      // candidate from absorbing the two particles.
      {EPOS::ParticleNo, EPOS::ParticleFinal, cost::kStrongBonus},

      // Nominalizer → conditional particle (の+なら). This productive
      // predicate boundary prevents なら from being analyzed as なる's
      // irrealis form after a clause nominalizer.
      {EPOS::ParticleNo, EPOS::ParticleConj, cost::kStrongBonus},

      // =========================================================================
      // Copula → Negation (ではない pattern)
      // =========================================================================

      // AuxCopulaDa (で form) → ParticleTopic (で+は/も in ではない/でもない)
      // Moderate bonus to promote 彼女|で|も|ない over 彼女|でも|ない
      {EPOS::AuxCopulaDa, EPOS::ParticleTopic, cost::kModerateBonus},

      // AuxCopulaDa → AuxNegativeNai (じゃ+ない, で+ない) - moderate bonus.
      {EPOS::AuxCopulaDa, EPOS::AuxNegativeNai, cost::kModerateBonus},

      // The copular negative でなく is the adverbial adjective form of ない,
      // unlike a verb's negative auxiliary 〜なく.
      {EPOS::AuxCopulaDa, EPOS::AdjRenyokei, cost::kExtraStrongBonus},

      // AuxCopulaDa → AuxGozaru (で+ございます) - strong bonus
      // Must beat the で(出る連用形)+ございます verb-candidate reading
      {EPOS::AuxCopulaDa, EPOS::AuxGozaru, cost::kStrongBonus},

      // The polite existence auxiliary also follows a topicalized copula
      // (で+は+ござい+ます), retaining its dependent honorific reading.
      {EPOS::ParticleTopic, EPOS::AuxGozaru, cost::kStrongBonus},

      // AuxGozaru → AuxTenseMasu (ござい+ます). The dependent honorific
      // reading remains preferred after its licensed copular/interjection
      // contexts, while a sentence-initial ござい keeps its lexical verb POS.
      {EPOS::AuxGozaru, EPOS::AuxTenseMasu, cost::kModerateBonus},

      // AuxCopulaDa → AuxCopulaDa (で+ある/あれ/あろ) - very strong bonus for
      // the formal copula and its volitional form であろう.
      // MeCab splits である as で(だ連用形) + ある(助動詞), not で(出る連用形) + ある
      // Copular forms cannot be chained directly (で+な). The copular
      // negative uses the adverbial adjective form なく as one token.
      {EPOS::AuxCopulaDa, EPOS::AuxCopulaDa, cost::kAlmostNever},

      // =========================================================================
      // Appearance/Conjecture connections
      // =========================================================================

      // VerbRenyokei → AuxAppearanceSou (食べ+そう) - decisive bonus
      // The lexical demonstrative reading has an additional u-final adverb
      // prior, so this inflectional connection must remain stronger.
      {EPOS::VerbRenyokei, EPOS::AuxAppearanceSou, cost::kAppearanceAuxiliaryBonus},

      // Na-adjective stems take appearance そう directly (静か+そう).
      {EPOS::AdjNaAdj, EPOS::AuxAppearanceSou, cost::kAppearanceAuxiliaryBonus},

      // AuxAspectShimau → AuxAppearanceSou (しまい+そう) - very strong bonus
      // しまいそう (about to end up doing) is natural; AUX chain must beat ADJ+ADV path
      // Strong bonus needed because そう(ADV) has dict bonus (-0.5) vs そう(AUX) cost (0.4)
      {EPOS::AuxAspectShimau, EPOS::AuxAppearanceSou, cost::kVeryStrongBonus},

      // Appearance そう can take a conditional connective (降り+そう+なら).
      // This preserves the auxiliary reading over the homographic
      // demonstrative adverb, whose general adverbial conditional path is
      // otherwise cheaper.
      {EPOS::AuxAppearanceSou, EPOS::ParticleConj, cost::kStrongBonus},

      // Other → AuxAppearanceSou - penalty (様態そう shouldn't appear at BOS)
      // At sentence start, そう should be demonstrative na-adjective, not appearance aux
      {EPOS::Other, EPOS::AuxAppearanceSou, cost::kMinor},

      // Other → AuxAspectIku - penalty (いく as aspect aux shouldn't appear at BOS)
      // At sentence start, いく should be verb (行く) or part of pronoun (いくつ)
      // AuxAspectIku is only valid after て-form (食べていく, 走っていく)
      {EPOS::Other, EPOS::AuxAspectIku, cost::kRare},

      // Particle → AuxAppearanceSou - penalty (様態そう shouldn't follow particles)
      // E.g., そうかもしれません: そう is demonstrative, not appearance auxiliary
      {EPOS::ParticleCase, EPOS::AuxAppearanceSou, cost::kMinor},
      {EPOS::ParticleTopic, EPOS::AuxAppearanceSou, cost::kMinor},
      {EPOS::ParticleAdverbial, EPOS::AuxAppearanceSou, cost::kMinor},
      {EPOS::ParticleQuote, EPOS::AuxAppearanceSou, cost::kMinor},

      // AdjBasic → AuxConjectureRashii (美しい+らしい) - strong bonus
      {EPOS::AdjBasic, EPOS::AuxConjectureRashii, cost::kStrongBonus},

      // VerbShuushikei → AuxConjectureRashii (食べる+らしい) - moderate bonus
      {EPOS::VerbShuushikei, EPOS::AuxConjectureRashii, cost::kModerateBonus},

      // VerbShuushikei → AuxAppearanceSou (食べる+そう hearsay) - strong bonus
      // Hearsay そう (伝聞) attaches to 終止形: 食べる+そうだ, する+そうです
      // Different from appearance そう (様態) which attaches to 連用形: 食べ+そう, し+そう
      {EPOS::VerbShuushikei, EPOS::AuxAppearanceSou, cost::kStrongBonus},

      // VerbShuushikei → AuxConjectureMitai (食べる+みたい) - strong bonus
      {EPOS::VerbShuushikei, EPOS::AuxConjectureMitai, cost::kStrongBonus},

      // VerbShuushikei → AuxVolitional (食べる+べき) - strong bonus for obligation
      {EPOS::VerbShuushikei, EPOS::AuxVolitional, cost::kStrongBonus},

      // AdjBasic → AuxConjectureMitai (美しい+みたい) - moderate bonus
      {EPOS::AdjBasic, EPOS::AuxConjectureMitai, cost::kModerateBonus},

      // Noun → AuxConjectureMitai (学生+みたい) - moderate bonus
      {EPOS::Noun, EPOS::AuxConjectureMitai, cost::kModerateBonus},

      // Noun → AuxConjectureRashii (春+らしい) - strong bonus
      {EPOS::Noun, EPOS::AuxConjectureRashii, cost::kStrongBonus},

      // AuxConjectureRashii → AuxNegativeNai (子供らしく+ない) - extreme bonus, mirroring
      // AdjRenyokei → AuxNegativeNai. らしく is the 連用形 of the auxiliary らしい and the
      // only AuxConjectureRashii form that precedes ない, so this negation split (子供 +
      // らしく + ない) is favored the same way as a genuine adjective's く-form + ない.
      // Genuine derived adjectives (素晴らしい) stay merged via their dict-inflected 連用形.
      {EPOS::AuxConjectureRashii, EPOS::AuxNegativeNai, cost::kExtremeBonus},

      // The same 連用形 らしく also modifies a following predicate directly
      // (資料らしく見える, 学生らしく振る舞う). Without the row that reading has
      // no support at all and the fabricated i-adjective over the whole span
      // wins, which made the host merge or split depending only on which cell
      // of らしい happened to be written.
      {EPOS::AuxConjectureRashii, EPOS::VerbShuushikei, cost::kStrongBonus},

      // The conjectural auxiliary's stem (らし) cannot be followed by the
      // progressive auxiliary く. Its adverbial form is the single inflected
      // token らしく, so this rejects a spurious らし+く segmentation.
      {EPOS::AuxConjectureRashii, EPOS::AuxAspectIku, cost::kAlmostNever},

      // AuxAspectIru → AuxConjectureRashii/Mitai (ている+らしい/みたい) - mirror the
      // VerbShuushikei rows above so the aspectual reading of the 補助動詞 いる is not
      // undercut at the following conjecture aux. Without these, the Aux→Aux base cost
      // cancels いる's aspect bonus and the main-verb いる reading wrongly wins.
      {EPOS::AuxAspectIru, EPOS::AuxConjectureRashii, cost::kModerateBonus},
      {EPOS::AuxAspectIru, EPOS::AuxConjectureMitai, cost::kStrongBonus},

      // AuxTenseTa → AuxConjectureRashii (疲れてい+た+らしい) retains the
      // past predicate before the conjectural auxiliary.  Without this
      // connection, the lattice can split らしい into unrelated fragments.
      {EPOS::AuxTenseTa, EPOS::AuxConjectureRashii, cost::kExtremeBonus},

      // AuxConjectureMitai → AuxCopulaDa (みたい+な) - strong bonus because な is
      // the attributive form of the following copula.
      {EPOS::AuxConjectureMitai, EPOS::AuxCopulaDa, cost::kStrongBonus},

      // =========================================================================
      // Prohibited/Penalized Connections (Grammatically Invalid or Unlikely)
      // =========================================================================

      // Note: VerbRenyokei → VerbRenyokei is NOT explicitly bonused because
      // a bonus breaks compound verbs (抱きしめて→抱き+しめ+て).
      // Legitimate patterns like 食べ+すぎる are handled by compound verb path.
      // Honorific patterns (待ち+いただけ) are handled by penalizing false
      // godan-wa candidates in verb_candidates_kanji.cpp.

      // VerbTaForm → VerbMizenkei (盛りだ+くさ) - strong penalty
      // Two verbs in sequence without auxiliary/particle is grammatically unusual
      // Prevents 盛りだくさん → 盛りだ+くさ+ん over dictionary entry
      {EPOS::VerbTaForm, EPOS::VerbMizenkei, cost::kVeryRare},

      // VerbRenyokei → VerbMizenkei (盛り+だくさ) - strong penalty
      // Renyokei followed by mizenkei is grammatically unusual
      // Legitimate patterns like 食べ+すぎ use Renyokei→Renyokei (すぎ is renyokei)
      {EPOS::VerbRenyokei, EPOS::VerbMizenkei, cost::kVeryRare},

      // VerbRenyokei → VerbOnbinkei (突き+刺さっ) - minor penalty
      // Verb renyokei directly followed by another verb in onbin form only occurs
      // in compound verbs (突き刺さる, 走り出す). When the compound is in the
      // dictionary, the merged token should be preferred over the split path.
      // Surface-based bonus in scorer.cpp adds extra penalty for kanji verbs.
      {EPOS::VerbRenyokei, EPOS::VerbOnbinkei, cost::kNegligible},

      // AdjBasic → VerbMizenkei (盛りだく+さ) - strong penalty
      // Adjective 終止形 followed by verb 未然形 is grammatically unusual
      // Prevents 盛りだくさん → 盛りだく+さ+ん over dictionary entry
      {EPOS::AdjBasic, EPOS::VerbMizenkei, cost::kVeryRare},

      // AdjBasic → AuxTenseTa (対応い+た) - severe penalty
      // An i-adjective 終止形 directly followed by past た is grammatically
      // impossible: the adjectival past is 連用形かっ + た (高かっ+た), which
      // travels the separate ADJ_かっ → AUX_過去 edge. Killing this edge removes
      // fabricated i-adjective paths such as 対応い+た+しか+ね for 対応いたしかねます.
      {EPOS::AdjBasic, EPOS::AuxTenseTa, cost::kSevere},

      // AdjStem → AuxConjectureMitai: unnatural (美し+みたい should be 美しい+みたい)
      {EPOS::AdjStem, EPOS::AuxConjectureMitai, cost::kAlmostNever},

      // AdjStem → AuxConjectureRashii: unnatural (美し+らしい should be 美しい+らしい)
      {EPOS::AdjStem, EPOS::AuxConjectureRashii, cost::kAlmostNever},

      // AdjStem → AdjBasic: prohibit (好+みらしい should be 好み+らしい)
      // Adjective stems don't connect to unrelated i-adjective endings
      {EPOS::AdjStem, EPOS::AdjBasic, cost::kAlmostNever},

      // AdjStem → Verb/Aux: prohibit (な+い should not split ない as な(AdjStem)+い)
      // な(AdjStem of ない) should only connect to さ(nominalization) or そう(appearance)
      // Also prevents 高+すぎた winning over 高+すぎ+た
      {EPOS::AdjStem, EPOS::VerbRenyokei, cost::kAlmostNever},
      {EPOS::AdjStem, EPOS::VerbOnbinkei, cost::kAlmostNever},
      {EPOS::AdjStem, EPOS::VerbShuushikei, cost::kAlmostNever},
      {EPOS::AdjStem, EPOS::VerbMizenkei, cost::kAlmostNever},
      {EPOS::AdjStem, EPOS::VerbTaForm, cost::kAlmostNever},
      {EPOS::AdjStem, EPOS::VerbTaraForm, cost::kAlmostNever},
      {EPOS::AdjStem, EPOS::AuxAspectIru, cost::kAlmostNever},    // な+い(いる)
      {EPOS::AdjNaAdj, EPOS::AuxAspectIru, cost::kAlmostNever},   // 理性的+い(いる)
      {EPOS::AdjStem, EPOS::AuxNegativeNai, cost::kAlmostNever},  // な+ない
      {EPOS::AdjStem, EPOS::Other, cost::kAlmostNever},           // な+い(OTHER)

      // Note: Particle → AdjStem is allowed for patterns like やる気がなさそう (が+な+さ+そう)
  };
  applyRules(table, kRules, sizeof(kRules) / sizeof(kRules[0]));

  setParticleAndLexicalPenaltyCosts(table);

  static constexpr BigramRule kClassicalRules[] = {
      // =========================================================================
      // Classical assertion/past なり/けり (文語断定・過去)
      // =========================================================================
      // AuxClassicalNari → AuxClassicalKeri (なり+けり: 春なりけり) - extreme bonus
      // なり alone competes with the VerbRenyokei(なる) reading and the character-speech
      // copula reading (でナリ系), so this chain-specific bonus is what makes the
      // classical parse win once けり is present; without a following けり, なり still
      // loses to those readings (see individual word tests for それなり/大人なり/etc.).
      // Needs to be extreme (not just very-strong) to also beat the single-kanji-noun
      // fallback penalty that makes the whole thing collapse into one unsplit
      // kanji_hira_compound NOUN token (e.g. 春 with no dictionary entry of its own).
      {EPOS::AuxClassicalNari, EPOS::AuxClassicalKeri, cost::kExtremeBonus},
      // The irrealis なら takes the classical negative (静か+なら+ず).
      {EPOS::AuxClassicalNari, EPOS::AuxNegativeNu, cost::kStrongBonus},

      // The classical copula attaches to a nominal predicate before the past
      // auxiliary (春+なり+けり). This preserves the closed auxiliary chain
      // over an unknown kanji-hiragana verb candidate.
      // A formal noun heads a nominal predicate as readily as a lexical one
      // (美しき+物+なり), so it takes the copula at the same rate. Without the
      // pair the copula loses its class to the homographic listing particle.
      {EPOS::Noun, EPOS::AuxClassicalNari, cost::kVeryStrongBonus},
      {EPOS::NounFormal, EPOS::AuxClassicalNari, cost::kVeryStrongBonus},

      // The copula also takes a verbal predicate, and there it selects the
      // attributive (灯+消ゆる+なり). Without the pair the only bonus on offer
      // goes to the finite form in front of the homographic conjunctive
      // particle, so a kanji run that swallows the stem wins the clause.
      {EPOS::VerbRentaikei, EPOS::AuxClassicalNari, cost::kVeryStrongBonus},

      // An auxiliary in the attributive fills that slot as readily as a verb
      // does (見つけ+たる+なり). Without the pair the chain has no bonus at all
      // and a hiragana run ending in なり is read as one adverb.
      {EPOS::AuxClassicalTari, EPOS::AuxClassicalNari, cost::kVeryStrongBonus},

      // The classical past auxiliary follows a verb's renyokei and its
      // 已然形 licenses the conditional particle (見+けれ+ば). These two
      // connections disambiguate the auxiliary chain from an unrelated
      // modern ichidan conditional such as 見けれ+ば.
      {EPOS::VerbRenyokei, EPOS::AuxClassicalKeri, cost::kVeryStrongBonus},
      {EPOS::AuxClassicalKeri, EPOS::ParticleConj, cost::kVeryStrongBonus},
      // けり is the 終止形 alone, so it closes the clause instead of heading a
      // noun phrase: the adnominal that a case particle would attach to is
      // spelled ける. Without this the two kana win over the deverbal noun
      // they share their spelling with (壁にけりを入れる).
      {EPOS::AuxClassicalKeri, EPOS::ParticleCase, cost::kProhibitive},

      // An adjective reaches the same auxiliary through its supplementary (カリ)
      // conjugation, whose whole purpose is to carry these forms
      // (高かり+けり, 美しかり+たり). Without the counterpart of the verb entry
      // above, the auxiliary favors a fabricated godan verb (高かる) instead.
      {EPOS::AdjRenyokei, EPOS::AuxClassicalKeri, cost::kVeryStrongBonus},
      {EPOS::AdjRenyokei, EPOS::AuxClassicalPerfect, cost::kVeryStrongBonus},

      // A nominal predicate takes the same classical auxiliaries directly, the
      // copula being elided in the literary register (確認けり, 確認べし,
      // 確認ごとし). Without these the auxiliary loses to a fabricated verb or
      // to an unknown run that swallows it whole.
      {EPOS::Noun, EPOS::AuxClassicalKeri, cost::kStrongBonus},
      {EPOS::Noun, EPOS::AuxClassicalBeshi, cost::kVeryStrongBonus},
      {EPOS::Noun, EPOS::AuxSimilitudeYou, cost::kVeryStrongBonus},

      // Only the 連体形 し and 已然形 しか of the classical past survive in
      // ordinary text, and both attach to a continuative: a verb's own
      // (読み+し), the negative's (読まざり+し) or the supplementary adjective
      // conjugation's (小さかり+し). Without these the one-mora surface is read
      // as the サ変 continuative it shares its spelling with.
      {EPOS::VerbRenyokei, EPOS::AuxClassicalKi, cost::kVeryStrongBonus},
      // The classical past auxiliary closes or modifies a clause; it cannot
      // directly introduce a new verb continuative. This keeps a sahen
      // continuative before a following classical predicate (記録+し+終へ)
      // from being re-read as the homographic past き.
      {EPOS::AuxClassicalKi, EPOS::VerbRenyokei, cost::kAlmostNever},
      // The negative's continuative ざり needs the wider margin: the サ変
      // continuative it competes with is the cheapest word in the lattice.
      {EPOS::AuxNegativeNu, EPOS::AuxClassicalKi, cost::kExtremeBonus},
      {EPOS::AdjRenyokei, EPOS::AuxClassicalKi, cost::kVeryStrongBonus},
      // The 終止形 き closes a clause behind the same continuatives, but its two
      // kana also spell the regional conjunctive particle, which instead takes a
      // finite predicate or the modern past (紙書く+き, 水飲んだ+き). Those two
      // hosts are the particle's alone, so the auxiliary is barred from them.
      // The copula is barred with them: the literary past reaches an assertion
      // through なり (春なりき), never through だ.
      {EPOS::VerbShuushikei, EPOS::AuxClassicalKi, cost::kProhibitive},
      {EPOS::AuxTenseTa, EPOS::AuxClassicalKi, cost::kProhibitive},
      {EPOS::AuxCopulaDa, EPOS::AuxClassicalKi, cost::kProhibitive},
      // A nominal is barred for the same reason: every host of the classical
      // past is a continuative cell, and a free connection here let a
      // fabricated kanji run take the mora that belongs to a classical
      // adjective terminal (道遠+し instead of 道+遠し). The sibling けり does
      // take a nominal predicate, and keeps its own bonus.
      {EPOS::Noun, EPOS::AuxClassicalKi, cost::kStrong},

      // A passive predicate reaches the same auxiliaries through the passive's
      // own continuative (飲ま+れ+けり). Without the counterpart of the verb
      // entry above, the passive stays inside the preceding verb (飲まれ+けり)
      // and the auxiliary loses its host.
      {EPOS::AuxPassive, EPOS::AuxClassicalKeri, cost::kVeryStrongBonus},
      {EPOS::AuxPassive, EPOS::AuxClassicalPerfect, cost::kVeryStrongBonus},

      // Classical perfect り attaches to a verbal inflection, never directly
      // to a noun or numeral. Blocking that impossible path prevents manner
      // mimetics such as AっBり from becoming a numeral plus auxiliary.
      {EPOS::Noun, EPOS::AuxClassicalPerfect, cost::kNever},
      {EPOS::NounNumber, EPOS::AuxClassicalPerfect, cost::kNever},

      // 連体形 なる (壮大なる計画): a na-adjective stem + なる is the classical adnominal 断定, not
      // the verb 成る. Only the left context (AdjNaAdj→なる) is rewarded: a right-context なる→Noun
      // bonus would misfire on the 終助詞 なり (鳴るなり法隆寺), and 〜になる/〜となる keep the verb
      // reading because a particle, not a na-adjective stem, precedes なる.
      {EPOS::AdjNaAdj, EPOS::AuxClassicalNari, cost::kExtremeBonus},

      // Classical タリ活用 連体形 たる (堂々たる, 確固たる, 暗澹たる). It is adnominal, so it
      // MUST be followed by a nominal (…たる態度) or the special particle や (…たるや). Keying
      // the bonus on this RIGHT-hand context — not on the preceding noun — is what separates
      // the auxiliary from a single-kanji-stem verb: sentence-final 当たる/隔たる have nothing
      // after たる, so they keep the verb reading, while 堂々たる態度 gets the boost. (A
      // left-side Noun→たる bonus cannot make this distinction: the single-kanji verb reading
      // 当たる is itself heavily penalized, so any left bonus wrongly flips it to 当|たる.)
      // A Suffix→たる bonus IS safe (a single-kanji verb stem is a Noun, never a Suffix) and
      // keeps the nominalizer さ as a Suffix in 美しさ+たる (…さたるや).
      {EPOS::Suffix, EPOS::AuxClassicalTari, cost::kStrongBonus},
      {EPOS::AuxClassicalTari, EPOS::Noun, cost::kStrongBonus},
      // A productive deverbal noun is still a nominal head under adnominal
      // たる (暗澹たる気持ち).  Treating NOUN_転成 differently from an
      // ordinary noun lets an internal shorter nominalization restart inside
      // the same search unit (気+持ち), even though both are selected by the
      // same closed adnominal auxiliary.
      {EPOS::AuxClassicalTari, EPOS::NounVerbal, cost::kStrongBonus},
      // A formal noun is a nominal head too (たるもの, たるゆえん). Without the
      // row it is cheaper to restart the formal noun as a focus particle plus a
      // nominalizer (もの split into も + の).
      {EPOS::AuxClassicalTari, EPOS::NounFormal, cost::kStrongBonus},
      {EPOS::AuxClassicalTari, EPOS::ParticleBinding, cost::kStrongBonus},
      {EPOS::AuxClassicalTari, EPOS::ParticleCase, cost::kStrongBonus},

      // AuxClassicalBeshi (当為べし 連体形 べき). べし attaches to the 連体形 of ラ変-type words,
      // so たる+べき joins (来たるべき, 然るべき); as a 連体形 it must precede a nominal, hence
      // べき→Noun/NounFormal (来たるべき日, やるべきこと). The 終止形→べき and 受身→べき cells
      // migrate the former AuxVolitional bonuses (食べるべき, 書かれるべき) now that べき is its
      // own EPOS; the original AuxVolitional cells stay in place for genuine う/よう volitional paths.
      {EPOS::AuxClassicalTari, EPOS::AuxClassicalBeshi, cost::kStrongBonus},
      {EPOS::AuxClassicalBeshi, EPOS::Noun, cost::kModerateBonus},
      {EPOS::AuxClassicalBeshi, EPOS::NounFormal, cost::kModerateBonus},
      // Predicative obligation keeps the formal boundary (べき+だ), rather
      // than being swallowed by an unrelated hiragana verb candidate.
      {EPOS::AuxClassicalBeshi, EPOS::AuxCopulaDa, cost::kStrongBonus},
      // The classical copula is the literary member of that same predicative
      // slot (読むべき+なり), where the conjunctive particle of the same kana
      // would have nothing to attach to.
      {EPOS::AuxClassicalBeshi, EPOS::AuxClassicalNari, cost::kStrongBonus},
      {EPOS::VerbShuushikei, EPOS::AuxClassicalBeshi, cost::kStrongBonus},
      {EPOS::AuxPassive, EPOS::AuxClassicalBeshi, cost::kStrongBonus},
      {EPOS::AuxNegativeNu, EPOS::AuxClassicalBeshi, cost::kStrongBonus},
      {EPOS::ParticleConj, EPOS::AuxHonorific, cost::kStrongBonus},
      // Closed honorific inflections retain the auxiliary reading before
      // their connective (なされ+ば), ahead of the homographic lexical verb.
      {EPOS::AuxHonorific, EPOS::ParticleConj, cost::kDoubleVeryStrongBonus},
  };
  applyRules(table, kClassicalRules, sizeof(kClassicalRules) / sizeof(kClassicalRules[0]));
}

}  // namespace suzume::analysis::bigram_rules
