#ifndef SUZUME_CORE_TYPES_H_
#define SUZUME_CORE_TYPES_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace suzume::core {

/**
 * @brief Part of speech types (minimal set for tag generation)
 */
enum class PartOfSpeech : uint8_t {
  Unknown,       // 不明
  Noun,          // 名詞
  Verb,          // 動詞
  Adjective,     // 形容詞
  Adverb,        // 副詞
  Particle,      // 助詞
  Auxiliary,     // 助動詞
  Conjunction,   // 接続詞
  Determiner,    // 連体詞
  Pronoun,       // 代名詞
  Prefix,        // 接頭辞
  Suffix,        // 接尾辞
  Interjection,  // 感動詞
  Symbol,        // 記号
  Other,         // その他
  Count_         // Number of POS values
};

/**
 * @brief Extended part of speech for fine-grained connection rules
 *
 * This enum provides ~55 categories for bigram-based connection cost lookup.
 * Design principles:
 *   1. All connection costs are determined by ExtendedPOS bigram (no exceptions)
 *   2. If behavior is wrong, either the category or assignment is wrong
 *   3. Grammar-based categorization (not MeCab-compatible by design)
 *
 * Category counts:
 *   - Verb forms: 7 (by conjugation form)
 *   - Adjective forms: 4 (by conjugation form)
 *   - Auxiliaries: 12 (by function)
 *   - Particles: 6 (by grammatical role)
 *   - Others: ~10 (nouns, adverbs, etc.)
 */
enum class ExtendedPOS : uint8_t {
  // =========================================================================
  // Unknown/Default (0)
  // =========================================================================
  Unknown = 0,

  // =========================================================================
  // Verb forms (1-10) - by conjugation form (活用形)
  // =========================================================================
  VerbShuushikei,  // 終止形: dictionary form (食べる, 書く)
  VerbRenyokei,    // 連用形: continuative form (食べ, 書き)
  VerbMizenkei,    // 未然形: irrealis form (食べ-, 書か-)
  VerbOnbinkei,    // 音便形: euphonic change form (書い-, 泳い-)
  VerbTeForm,      // て形: te-form (食べて, 書いて)
  VerbKateikei,    // 仮定形: conditional form (食べれば, 書けば)
  VerbMeireikei,   // 命令形: imperative form (食べろ, 書け)
  VerbRentaikei,   // 連体形: attributive form (same as shuushi for modern Japanese)
  VerbTaForm,      // た形: past form (食べた, 書いた) - for unified past verbs
  VerbTaraForm,    // たら形: conditional past form (食べたら, 書いたら)

  // =========================================================================
  // Adjective forms (11-20) - by conjugation form
  // =========================================================================
  AdjBasic,     // 終止形: basic form (美しい, 高い)
  AdjRenyokei,  // 連用形(く): adverbial form (美しく, 高く)
  AdjStem,      // 語幹: stem for ガル接続 (美しー, 高ー)
  AdjKatt,      // かっ形: past stem (美しかっ-, 高かっ-)
  AdjKeForm,    // け形: conditional stem (美しけれ-, 高けれ-)
  AdjNaAdj,     // ナ形容詞: na-adjective stem (静か, 綺麗)

  // =========================================================================
  // Auxiliaries (21-40) - by function (機能別)
  // =========================================================================
  // Tense auxiliaries
  AuxTenseTa,    // 過去: た、だ
  AuxTenseMasu,  // 丁寧: ます、まし、ませ

  // Negation auxiliaries
  AuxNegativeNai,  // 否定: ない、なかっ (adjective-like conjugation)
  AuxNegativeNu,   // 否定(古語): ぬ、ん

  // Desire/Volition auxiliaries
  AuxDesireTai,   // 願望: たい、たかっ (adjective-like)
  AuxVolitional,  // 意志/推量: う、よう

  // Voice auxiliaries
  AuxPassive,    // 受身: れる、られる、られ
  AuxCausative,  // 使役: せる、させる、させ
  AuxPotential,  // 可能: れる、られる (context-dependent)

  // Aspect auxiliaries
  AuxAspectIru,       // 継続: いる、い、おる
  AuxAspectShimau,    // 完了: しまう、ちゃう、じゃう
  AuxAspectOku,       // 準備: おく、とく (contraction)
  AuxAspectMiru,      // 試行: みる
  AuxAspectIku,       // 進行方向: いく
  AuxAspectKuru,      // 接近: くる
  AuxAspectHajimeru,  // 開始: はじめる

  // Appearance/Conjecture auxiliaries
  AuxAppearanceSou,     // 様態: そう (after renyokei/stem)
  AuxConjectureRashii,  // 推定: らしい
  AuxConjectureMitai,   // 推定: みたい

  // Copula auxiliaries
  AuxCopulaDa,    // 断定: だ、で、な、なら
  AuxCopulaDesu,  // 丁寧断定: です、でし

  // Other auxiliaries
  AuxHonorific,  // 尊敬: れる、られる (honorific use)
  AuxGozaru,     // 丁重: ござる、ございます
  AuxExcessive,  // 過度: すぎる (excessive degree)
  AuxGaru,       // ガル接続: がる (adjective stem → verb)

  // =========================================================================
  // Particles (41-50) - by grammatical role
  // =========================================================================
  ParticleCase,       // 格助詞: が、を、に、で、へ、と、から、まで、より
  ParticleTopic,      // 係助詞: は、も
  ParticleFinal,      // 終助詞: ね、よ、わ、な、か、さ
  ParticleConj,       // 接続助詞: て、で、ば、ながら、たり、から、けど
  ParticleQuote,      // 引用助詞: と（引用）
  ParticleAdverbial,  // 副助詞: ばかり、だけ、ほど、しか、など、まで
  ParticleNo,         // 準体助詞: の (nominalizer)
  ParticleBinding,    // 係結び: こそ、さえ、すら

  // =========================================================================
  // Nouns (51-55)
  // =========================================================================
  Noun,              // 普通名詞: general nouns
  NounFormal,        // 形式名詞: こと、もの、ところ、わけ
  NounVerbal,        // 動詞連用形転成名詞: 読み、書き (nominalized verb)
  NounProper,        // 固有名詞: proper nouns
  NounProperFamily,  // 固有名詞(姓): family names (優木, 田中)
  NounProperGiven,   // 固有名詞(名): given names (せつ菜, 太郎)
  NounNumber,        // 数詞: numbers

  // =========================================================================
  // Pronouns (56-58)
  // =========================================================================
  Pronoun,               // 代名詞: 私、あなた、これ、それ
  PronounInterrogative,  // 疑問詞: 何、誰、どこ、いつ

  // =========================================================================
  // Other categories (59-65)
  // =========================================================================
  Adverb,           // 副詞: general adverbs
  AdverbQuotative,  // 引用副詞: そう、こう、ああ、どう
  Conjunction,      // 接続詞: しかし、そして、だから
  Determiner,       // 連体詞: この、その、あの、どんな
  Prefix,           // 接頭辞: お、ご、不、未
  Suffix,           // 接尾辞: さん、様、的、化
  Symbol,           // 記号: punctuation and symbols
  Interjection,     // 感動詞: 何だ、ああ、おい
  Other,            // その他

  // =========================================================================
  // Late additions
  // Appended at the end because ExtendedPOS values are serialized as uint8
  // in compiled binary dictionaries (core.dic/user.dic); inserting into the
  // groups above would shift existing values and corrupt EPOS decoding.
  // =========================================================================
  AdjMizenkei,  // 形容詞未然形(かろ): irrealis stem for 推量 (高かろ-, 美しかろ-)

  AuxNegativeMai,  // 打消推量: まい (attaches to 終止形, unlike ぬ/ん which take 未然形)

  AuxClassicalNari,        // 文語断定: なり (春なり = it is spring)
  AuxClassicalKeri,        // 文語過去/詠嘆: けり (なりけり)
  AuxClassicalTari,        // 文語断定タリ活用連体形: たる (堂々たる, 確固たる)
  AuxClassicalPerfect,     // 文語完了・存続: たり/り (行きたり, 行けり)
  AuxClassicalBeshi,       // 文語当為べし連体形: べき (来たるべき, 読むべき)
  AuxInability,            // 不可能・躊躇: かねる
  AuxBenefactive,          // 授受: あげる（〜てあげる）
  SuffixRecentCompletion,  // 完成直後: たて
  SuffixTendency,          // 傾向: がち
  DeterminerQuotative,     // 引用連体: という、っていう
  AuxSimilitudeYou,        // 比況・様態: よう（読むようだ、次のように）
  AuxKuruwaPolite,         // Regional polite auxiliary: なんし
  AuxClassicalKi,          // 文語過去き連体形: し (読みし人), 已然形: しか

  // 仮定形口語縮約: 行きゃ, 食べりゃ, すりゃ, 早けりゃ. The conjunctive particle
  // ば has fused into the inflection, so unlike VerbKateikei the form closes a
  // conditional clause instead of opening a place for the particle.
  VerbContractedKateikei,

  // 終止形接続の接続助詞: が (逆接). Conjunctive particles divide by the form
  // they attach to, and the two halves take opposite connection values: て,
  // ながら and つつ follow a continuative, while this one follows a finite
  // predicate. が is the member that needs the distinction, because its case
  // homograph demands a nominal on the left and the two readings therefore
  // compete for every position. The remaining finite-attaching members (から,
  // けれど, ので, のに) have no such homograph and stay in the general class.
  ParticleConjFinite,

  // Count marker (for array sizing)
  Count_  // Total number of categories
};

/**
 * @brief Analysis mode
 */
enum class AnalysisMode : uint8_t {
  Normal,  // Normal mode
  Search,  // Search mode (keep noun compounds)
  Split    // Split mode (fine-grained segmentation)
};

/**
 * @brief Check whether a PartOfSpeech value is inside the declared enum range.
 */
[[nodiscard]] constexpr bool isValidPartOfSpeech(PartOfSpeech pos) noexcept {
  return static_cast<size_t>(pos) < static_cast<size_t>(PartOfSpeech::Count_);
}

/**
 * @brief Check whether an ExtendedPOS value is inside the declared enum range.
 */
[[nodiscard]] constexpr bool isValidExtendedPos(ExtendedPOS extended_pos) noexcept {
  return static_cast<size_t>(extended_pos) < static_cast<size_t>(ExtendedPOS::Count_);
}

/**
 * @brief Whether an extended POS is normally omitted from search tags.
 *
 * This is a grammatical category predicate, not a surface-word list. Formal
 * nouns and pronouns carry little standalone search value while remaining
 * visible in morphological analysis.
 */
[[nodiscard]] constexpr bool isLowInformation(ExtendedPOS extended_pos) noexcept {
  return extended_pos == ExtendedPOS::NounFormal || extended_pos == ExtendedPOS::Pronoun ||
         extended_pos == ExtendedPOS::PronounInterrogative;
}

/**
 * @brief Origin of candidate generation (for debug)
 *
 * Tracks which generator produced a candidate for debugging purposes.
 */
enum class CandidateOrigin : uint8_t {
  Unknown = 0,
  Dictionary,                     // 辞書からの直接候補
  VerbKanji,                      // 漢字+ひらがな動詞 (食べる)
  VerbHiragana,                   // ひらがな動詞 (いく, できる)
  VerbHiraganaPassiveRenyokei,    // ひらがな一段連用形 + られる
  VerbHiraganaNegativeRenyokei,   // ひらがな連用形 + ない
  VerbHiraganaInflectedRenyokei,  // ひらがな一段連用形 + て/た
  VerbKatakana,                   // カタカナ動詞 (バズる)
  VerbCompound,                   // 複合動詞 (恐れ入る)
  AdjectiveI,                     // イ形容詞 (美しい)
  AdjectiveIHiragana,             // ひらがなイ形容詞 (まずい)
  AdjectiveNa,                    // ナ形容詞・的形容詞 (理性的)
  NominalizedNoun,                // 連用形転成名詞 (手助け)
  SuffixPattern,                  // 接尾辞パターン (〜化, 〜性)
  SameType,                       // 同一文字種 (漢字列, カタカナ列)
  Alphanumeric,                   // 英数字
  Onomatopoeia,                   // オノマトペ (わくわく)
  CharacterSpeech,                // キャラ語尾 (ナリ, ござる)
  Split,                          // 分割候補 (NOUN+VERB)
  Join,                           // 結合候補 (複合動詞結合)
  KanjiHiraganaCompound,          // 漢字+ひらがな複合名詞 (玉ねぎ)
  KanjiHiraganaNominalCompound,   // 右の名詞句助詞で検証された漢字+ひらがな名詞
  SelectedNominalHead,            // 左の名詞選択要素と右助詞で境界検証された名詞主要部
  BracketedNoun,                  // 両側の文法境界で検証された未知ひらがな名詞
  Counter,                        // 数量詞パターン (一つ〜九つ)
  PrefixCompound,                 // 接頭的複合語 (今日, 本日, 全国)
};

/**
 * @brief Convert CandidateOrigin to string for debug output
 */
const char* originToString(CandidateOrigin origin);

/**
 * @brief Convert part of speech to string (English)
 */
std::string_view posToString(PartOfSpeech pos);

/**
 * @brief Convert part of speech to Japanese string
 */
std::string_view posToJapanese(PartOfSpeech pos);

/**
 * @brief Parse a part-of-speech name, returning nullopt for unrecognized input.
 *
 * Canonical POS parser shared by the TSV loader, user-dictionary loader, and
 * interactive editor. Accepts the English canonical short forms, their long
 * aliases (e.g. ADJECTIVE, ADVERB), and the Japanese names. PROPN/PROPER_NOUN
 * map to Noun (Suzume has no dedicated proper-noun POS); OTHER/PHRASE/その他
 * map to Other.
 */
std::optional<PartOfSpeech> stringToPosStrict(std::string_view str);

/**
 * @brief Convert string to part of speech, returning Other for unknown input.
 */
PartOfSpeech stringToPos(std::string_view str);

/**
 * @brief Map PartOfSpeech to a default ExtendedPOS
 *
 * Used by user dictionary (Layer 4) entries which only specify POS,
 * not ExtendedPOS. Maps to the most common/neutral ExtendedPOS for each POS.
 */
ExtendedPOS posToDefaultExtendedPOS(PartOfSpeech pos);

/**
 * @brief Check if POS is taggable (content word)
 */
bool isTaggable(PartOfSpeech pos);

/**
 * @brief Check if POS is content word
 */
bool isContentWord(PartOfSpeech pos);

/**
 * @brief Check if POS is function word
 */
bool isFunctionWord(PartOfSpeech pos);

/**
 * @brief Convert extended POS to string (English)
 */
std::string_view extendedPosToString(ExtendedPOS epos);

/**
 * @brief Get the base PartOfSpeech from ExtendedPOS
 *
 * Maps extended categories back to the base POS for API compatibility.
 * E.g., VerbRenyokei -> Verb, AuxTenseTa -> Auxiliary
 */
PartOfSpeech extendedPosToPos(ExtendedPOS epos);

/**
 * @brief Get a default ExtendedPOS from PartOfSpeech
 *
 * Provides initial mapping before fine-grained categorization.
 * E.g., Verb -> VerbShuushikei, Noun -> Noun
 */
ExtendedPOS posToExtendedPos(PartOfSpeech pos);

/**
 * @brief Check if ExtendedPOS is a verb form
 */
constexpr bool isVerbForm(ExtendedPOS epos) {
  return (epos >= ExtendedPOS::VerbShuushikei && epos <= ExtendedPOS::VerbTaraForm) ||
         epos == ExtendedPOS::VerbContractedKateikei;
}

/**
 * @brief Check if ExtendedPOS is an adjective form
 */
constexpr bool isAdjectiveForm(ExtendedPOS epos) {
  return (epos >= ExtendedPOS::AdjBasic && epos <= ExtendedPOS::AdjNaAdj) || epos == ExtendedPOS::AdjMizenkei;
}

/**
 * @brief Check if ExtendedPOS is an auxiliary
 * Note: AuxExcessive (すぎる) and AuxGaru (がる) are excluded because they
 * map to Verb (補助動詞/接尾動詞).
 */
constexpr bool isAuxiliaryType(ExtendedPOS epos) {
  // Late additions are listed explicitly because their serialized enum values
  // cannot be inserted into the contiguous auxiliary range.
  return (epos >= ExtendedPOS::AuxTenseTa && epos <= ExtendedPOS::AuxGozaru) || epos == ExtendedPOS::AuxNegativeMai ||
         epos == ExtendedPOS::AuxClassicalNari || epos == ExtendedPOS::AuxClassicalKeri ||
         epos == ExtendedPOS::AuxClassicalTari || epos == ExtendedPOS::AuxClassicalPerfect ||
         epos == ExtendedPOS::AuxClassicalBeshi || epos == ExtendedPOS::AuxInability ||
         epos == ExtendedPOS::AuxBenefactive || epos == ExtendedPOS::AuxSimilitudeYou ||
         epos == ExtendedPOS::AuxKuruwaPolite || epos == ExtendedPOS::AuxClassicalKi;
}

/**
 * @brief Check if ExtendedPOS is a literary (文語) auxiliary
 *
 * These auxiliaries stand on an inflected verb cell as free morphemes with their
 * own connection rules, and none of them is ever produced by the inflection
 * analyzer as part of a modern paradigm. Their presence in a run is therefore
 * boundary evidence in both directions: it licenses the continuative or irrealis
 * reading of the material in front of them, and it forbids a fabricated verb
 * from spanning them.
 *
 * AuxVolitional is included because its modern members う / よう follow the o-row
 * irrealis, so at a continuative or a-row irrealis position only the literary む
 * family can match.
 */
constexpr bool isClassicalAuxiliaryType(ExtendedPOS epos) {
  return epos == ExtendedPOS::AuxNegativeNu || epos == ExtendedPOS::AuxVolitional ||
         epos == ExtendedPOS::AuxClassicalNari || epos == ExtendedPOS::AuxClassicalKeri ||
         epos == ExtendedPOS::AuxClassicalTari || epos == ExtendedPOS::AuxClassicalPerfect ||
         epos == ExtendedPOS::AuxClassicalBeshi || epos == ExtendedPOS::AuxClassicalKi;
}

/**
 * @brief Check if ExtendedPOS is a particle type
 */
constexpr bool isParticleType(ExtendedPOS epos) {
  // The original particle block is contiguous; later particle categories are
  // appended after it because ExtendedPOS is serialized as uint8 in the
  // compiled dictionaries, so the range alone no longer covers the class.
  return (epos >= ExtendedPOS::ParticleCase && epos <= ExtendedPOS::ParticleBinding) ||
         epos == ExtendedPOS::ParticleConjFinite;
}

/**
 * @brief Check if ExtendedPOS is a noun type
 */
constexpr bool isNounType(ExtendedPOS epos) {
  // Keep this explicit: ExtendedPOS is serialized, so later nominal categories
  // are appended after the original contiguous noun block.
  switch (epos) {
    case ExtendedPOS::Noun:
    case ExtendedPOS::NounFormal:
    case ExtendedPOS::NounVerbal:
    case ExtendedPOS::NounProper:
    case ExtendedPOS::NounProperFamily:
    case ExtendedPOS::NounProperGiven:
    case ExtendedPOS::NounNumber:
      return true;
    default:
      return false;
  }
}

/**
 * @brief Check if ExtendedPOS is a pronoun type
 */
constexpr bool isPronounType(ExtendedPOS epos) {
  switch (epos) {
    case ExtendedPOS::Pronoun:
    case ExtendedPOS::PronounInterrogative:
      return true;
    default:
      return false;
  }
}

/**
 * @brief Detect verb conjugation form from surface and suffix
 *
 * Analyzes the verb surface and suffix chain to determine the conjugation form:
 * - VerbShuushikei: Dictionary form (食べる, 書く)
 * - VerbRenyokei: Continuative form (食べ, 書き)
 * - VerbMizenkei: Irrealis form (食べ-, 書か-)
 * - VerbOnbinkei: Euphonic change form (書い-, 泳い-)
 * - VerbTeForm: Te-form (食べて, 書いて)
 * - VerbKateikei: Conditional form (食べれば, 書けば)
 * - VerbMeireikei: Imperative form (食べろ, 書け)
 * - VerbTaForm: Past form (食べた, 書いた)
 * - VerbTaraForm: Conditional past form (食べたら, 書いたら)
 *
 * @param surface The verb surface form
 * @param suffix The suffix chain (from inflection analysis, may be empty)
 * @return The detected ExtendedPOS verb form
 */
ExtendedPOS detectVerbForm(std::string_view surface, std::string_view suffix = {}, bool godan_imperative_hint = false,
                           bool godan_i_onbin_hint = false);

/**
 * @brief Detect adjective conjugation form from surface
 *
 * Analyzes the adjective surface to determine the conjugation form:
 * - AdjBasic: Dictionary form (美しい, 高い)
 * - AdjRenyokei: Adverbial form (美しく, 高く)
 * - AdjStem: Stem form (美し-, 高-)
 * - AdjKatt: Past stem (美しかっ-, 高かっ-)
 * - AdjKeForm: Conditional stem (美しけれ-, 高けれ-)
 * - AdjNaAdj: Na-adjective (静か, 綺麗)
 *
 * @param surface The adjective surface form
 * @param is_na_adj True if this is a na-adjective
 * @return The detected ExtendedPOS adjective form
 */
ExtendedPOS detectAdjForm(std::string_view surface, bool is_na_adj = false);

}  // namespace suzume::core

#endif  // SUZUME_CORE_TYPES_H_
