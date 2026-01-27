/**
 * @file join_candidates.cpp
 * @brief Join-based candidate generation for tokenizer
 */

#include "join_candidates.h"

#include "candidate_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/inflection.h"
#include "normalize/utf8.h"
#include "tokenizer_utils.h"

namespace suzume::analysis {

namespace {

// V2 Subsidiary verbs for compound verb joining
// Verb type determines how to generate renyokei from base form
enum class V2VerbType : uint8_t {
  Godan,   // 五段: 込む→込み, 返す→返し (replace ending with i-row)
  Ichidan, // 一段: 続ける→続け, 上げる→上げ (drop る)
};

struct SubsidiaryVerb {
  const char* surface;      // Kanji form (or hiragana if no kanji)
  const char* reading;      // Hiragana reading (nullptr if same as surface)
  const char* base_ending;  // Base form ending for verb type detection
  V2VerbType verb_type;     // Determines renyokei generation
};

// List of V2 verbs that can form compound verbs
// Renyokei forms are generated automatically from base forms
// NOTE: 始める, 過ぎる, 終わる/終える are NOT included because they are
// grammatical/aspectual auxiliaries that should be tokenized separately
// for MeCab compatibility (e.g., 読み + 始める, not 読み始める)
const SubsidiaryVerb kSubsidiaryVerbs[] = {
    // Godan verbs (五段)
    {"込む", "こむ", "む", V2VerbType::Godan},      // 読み込む, 飛びこむ
    {"出す", "だす", "す", V2VerbType::Godan},      // 呼び出す, 走りだす
    {"続く", "つづく", "く", V2VerbType::Godan},    // 引き続く
    {"返す", "かえす", "す", V2VerbType::Godan},    // 繰り返す, 繰りかえす
    {"返る", "かえる", "る", V2VerbType::Godan},    // 振り返る, 振りかえる
    {"つかる", nullptr, "る", V2VerbType::Godan},   // 見つかる
    {"合う", "あう", "う", V2VerbType::Godan},      // 話し合う, 話しあう
    {"消す", "けす", "す", V2VerbType::Godan},      // 取り消す
    {"直す", "なおす", "す", V2VerbType::Godan},    // やり直す, やりなおす
    {"切る", "きる", "る", V2VerbType::Godan},      // 締め切る, 締めきる
    {"上がる", "あがる", "る", V2VerbType::Godan},  // 立ち上がる, 盛り上がる
    {"下がる", "さがる", "る", V2VerbType::Godan},  // 立ち下がる
    {"回す", "まわす", "す", V2VerbType::Godan},    // 振り回す, 持ち回す
    {"回る", "まわる", "る", V2VerbType::Godan},    // 持ち回る, 振り回る
    {"抜く", "ぬく", "く", V2VerbType::Godan},      // 追い抜く, 突き抜く
    {"掛かる", "かかる", "る", V2VerbType::Godan},  // 取り掛かる
    {"付く", "つく", "く", V2VerbType::Godan},      // 思い付く, 気付く
    {"巡る", "めぐる", "る", V2VerbType::Godan},    // 駆け巡る, 飛び巡る
    {"飛ばす", "とばす", "す", V2VerbType::Godan},  // 吹き飛ばす, 弾き飛ばす
    {"交う", "かう", "う", V2VerbType::Godan},      // 飛び交う, 行き交う
    {"潰す", "つぶす", "す", V2VerbType::Godan},    // 押し潰す, 叩き潰す
    {"崩す", "くずす", "す", V2VerbType::Godan},    // 切り崩す, 打ち崩す
    {"倒す", "たおす", "す", V2VerbType::Godan},    // 打ち倒す, 蹴り倒す
    {"起こす", "おこす", "す", V2VerbType::Godan},  // 引き起こす, 呼び起こす
    // Ichidan verbs (一段)
    {"続ける", "つづける", "ける", V2VerbType::Ichidan}, // 読み続ける, 読みつづける
    {"つける", nullptr, "ける", V2VerbType::Ichidan},    // 見つける
    {"替える", "かえる", "える", V2VerbType::Ichidan},   // 切り替える
    {"換える", "かえる", "える", V2VerbType::Ichidan},   // 入れ換える
    {"合わせる", "あわせる", "せる", V2VerbType::Ichidan}, // 組み合わせる
    {"切れる", "きれる", "れる", V2VerbType::Ichidan},   // 使い切れる
    {"出る", "でる", "る", V2VerbType::Ichidan},         // 飛び出る
    {"上げる", "あげる", "げる", V2VerbType::Ichidan},   // 売り上げる, 取り上げる
    {"下げる", "さげる", "げる", V2VerbType::Ichidan},   // 引き下げる
    {"抜ける", "ぬける", "ける", V2VerbType::Ichidan},   // 突き抜ける
    {"落とす", "おとす", "す", V2VerbType::Godan},       // 切り落とす, 打ち落とす
    {"落ちる", "おちる", "ちる", V2VerbType::Ichidan},   // 転げ落ちる
    {"掛ける", "かける", "ける", V2VerbType::Ichidan},   // 呼び掛ける, 働き掛ける
    {"付ける", "つける", "ける", V2VerbType::Ichidan},   // 押し付ける, 決め付ける
    {"入れる", "いれる", "れる", V2VerbType::Ichidan},   // 取り入れる, 持ち入れる
    {"分ける", "わける", "ける", V2VerbType::Ichidan},   // 切り分ける, 振り分ける
    {"立てる", "たてる", "てる", V2VerbType::Ichidan},   // 組み立てる, 打ち立てる
    {"広げる", "ひろげる", "げる", V2VerbType::Ichidan}, // 繰り広げる, 押し広げる
};

// Generate renyokei surface from base form
// Godan: replace ending with i-row (込む→込み, 返す→返し)
// Ichidan: drop る (続ける→続け)
inline std::string generateRenyokei(std::string_view surface,
                                    std::string_view reading,
                                    V2VerbType verb_type) {
  std::string_view base = reading.empty() ? surface : reading;
  if (base.empty()) return "";

  if (verb_type == V2VerbType::Ichidan) {
    // Drop final る (3 bytes in UTF-8)
    if (base.size() >= 3) {
      return std::string(base.substr(0, base.size() - 3));
    }
    return "";
  }

  // Godan: replace last hiragana with i-row equivalent
  // む→み, す→し, く→き, ぐ→ぎ, う→い, る→り, つ→ち, ぬ→に, ぶ→び
  if (base.size() < 3) return "";
  std::string result(base.substr(0, base.size() - 3));
  std::string_view last = base.substr(base.size() - 3);
  if (last == "む") result += "み";
  else if (last == "す") result += "し";
  else if (last == "く") result += "き";
  else if (last == "ぐ") result += "ぎ";
  else if (last == "う") result += "い";
  else if (last == "る") result += "り";
  else if (last == "つ") result += "ち";
  else if (last == "ぬ") result += "に";
  else if (last == "ぶ") result += "び";
  else return "";  // Unknown ending
  return result;
}

// Te-form euphonic form type for Godan verbs
enum class TeFormType : uint8_t {
  Ionbin,      // イ音便 (書く→書い, 泳ぐ→泳い) + て/で
  Sokuonbin,   // 促音便 (待つ→待っ, 買う→買っ, 帰る→帰っ) + て
  Hatsuonbin,  // 撥音便 (読む→読ん, 飛ぶ→飛ん, 死ぬ→死ん) + で
  Renyokei,    // 連用形 (話す→話し) + て
  Ichidan,     // 一段 (食べる→食べ) + て
};

// Determine te-form type and suffix from verb ending
inline TeFormType getTeFormType(std::string_view base_ending) {
  if (base_ending == "く" || base_ending == "ぐ") return TeFormType::Ionbin;
  if (base_ending == "つ" || base_ending == "う" || base_ending == "る") return TeFormType::Sokuonbin;
  if (base_ending == "む" || base_ending == "ぶ" || base_ending == "ぬ") return TeFormType::Hatsuonbin;
  if (base_ending == "す") return TeFormType::Renyokei;
  // Default for ichidan
  return TeFormType::Ichidan;
}

// Generate te-form euphonic stem (before て/で)
// Returns: stem and whether it uses で (vs て)
inline std::pair<std::string, bool> generateTeFormStem(std::string_view surface,
                                                        std::string_view reading,
                                                        V2VerbType verb_type,
                                                        std::string_view base_ending) {
  std::string_view base = reading.empty() ? surface : reading;
  if (base.empty() || base.size() < 3) return {"", false};

  if (verb_type == V2VerbType::Ichidan) {
    // Drop final る (3 bytes in UTF-8)
    return {std::string(base.substr(0, base.size() - 3)), false};
  }

  // Godan: generate euphonic form based on verb ending
  std::string result(base.substr(0, base.size() - 3));
  TeFormType te_type = getTeFormType(base_ending);

  switch (te_type) {
    case TeFormType::Ionbin:
      result += "い";
      return {result, base_ending == "ぐ"};  // ぐ→いで, く→いて
    case TeFormType::Sokuonbin:
      result += "っ";
      return {result, false};  // っ+て
    case TeFormType::Hatsuonbin:
      result += "ん";
      return {result, true};   // ん+で
    case TeFormType::Renyokei:
      result += "し";
      return {result, false};  // し+て
    default:
      return {"", false};
  }
}

// Generate kanji renyokei from kanji surface
inline std::string generateKanjiRenyokei(std::string_view kanji_surface,
                                         std::string_view reading,
                                         V2VerbType verb_type) {
  if (reading.empty()) {
    return generateRenyokei(kanji_surface, "", verb_type);
  }
  // For kanji+okurigana, need to replace okurigana with renyokei
  // E.g., 込む(こむ) → 込み, 続ける(つづける) → 続け
  std::string hiragana_renyokei = generateRenyokei(reading, "", verb_type);
  if (hiragana_renyokei.empty()) return "";

  // Find where kanji ends and okurigana begins
  // Kanji surface should have same kanji prefix as reading suffix is okurigana
  size_t kanji_bytes = 0;
  for (size_t i = 0; i < kanji_surface.size(); i += 3) {
    auto byte = static_cast<unsigned char>(kanji_surface[i]);
    if (byte >= 0xE4 && byte <= 0xE9) {  // CJK kanji range
      kanji_bytes = i + 3;
    } else {
      break;
    }
  }
  if (kanji_bytes == 0) return "";

  // Kanji prefix + hiragana renyokei suffix
  std::string result(kanji_surface.substr(0, kanji_bytes));
  size_t reading_kanji_len = reading.size() - (kanji_surface.size() - kanji_bytes);
  if (reading_kanji_len < hiragana_renyokei.size()) {
    result += hiragana_renyokei.substr(reading_kanji_len);
  }
  return result;
}

// 連用形 (continuative form) endings for Godan verbs
struct RenyokeiPattern {
  char32_t renyokei;   // 連用形 ending
  char32_t base;       // Base form ending
};

const RenyokeiPattern kGodanRenyokei[] = {
    {U'き', U'く'},  // 書き → 書く
    {U'ぎ', U'ぐ'},  // 泳ぎ → 泳ぐ
    {U'し', U'す'},  // 話し → 話す
    {U'ち', U'つ'},  // 持ち → 持つ
    {U'に', U'ぬ'},  // 死に → 死ぬ
    {U'び', U'ぶ'},  // 飛び → 飛ぶ
    {U'み', U'む'},  // 読み → 読む
    {U'り', U'る'},  // 取り → 取る
    {U'い', U'う'},  // 思い → 思う
};

// Cost bonuses imported from candidate_constants.h:
// candidate::kCompoundVerbBonus, candidate::kVerifiedV1Bonus

// Productive prefixes for prefix+noun joining
struct ProductivePrefix {
  char32_t codepoint;
  float bonus;
  bool needs_kanji;
};

const ProductivePrefix kProductivePrefixes[] = {
    // Note: Honorific prefixes お, ご, 御 are NOT included here.
    // They should be tokenized separately as PREFIX + NOUN.
    // E.g., お水 → お(PREFIX) + 水(NOUN), not お水(NOUN)

    // Negation prefixes
    {U'不', -0.4F, true},   // 不安, 不要, 不便
    {U'未', -0.4F, true},   // 未経験, 未確認
    {U'非', -0.4F, true},   // 非常, 非公開
    {U'無', -0.4F, true},   // 無理, 無料

    // Degree/quantity prefixes
    {U'超', -0.3F, true},   // 超人, 超高速
    {U'再', -0.4F, true},   // 再開, 再確認
    {U'準', -0.4F, true},   // 準備, 準決勝
    {U'副', -0.4F, true},   // 副社長, 副作用
    {U'総', -0.4F, true},   // 総合, 総数
    {U'各', -0.4F, true},   // 各地, 各種
    {U'両', -0.4F, true},   // 両方, 両手
    {U'最', -0.4F, true},   // 最高, 最新
    {U'全', -0.4F, true},   // 全部, 全員
    {U'半', -0.4F, true},   // 半分, 半額
};

constexpr size_t kNumPrefixes = sizeof(kProductivePrefixes) / sizeof(kProductivePrefixes[0]);

// Maximum noun length for prefix joining
constexpr size_t kMaxNounLenForPrefix = 6;

// Cost bonus imported from candidate_constants.h:
// candidate::kVerifiedNounBonus

// Te-form auxiliary verb patterns
struct TeFormAuxiliary {
  const char* stem;
  const char* base_form;
  bool is_benefactive;  // Benefactive verbs should not form negative compounds
};

const TeFormAuxiliary kTeFormAuxiliaries[] = {
    {"い", "いく", false},      // 〜ていく
    {"く", "くる", false},      // 〜てくる
    {"み", "みる", false},      // 〜てみる
    {"お", "おく", false},      // 〜ておく
    {"しま", "しまう", false},  // 〜てしまう
    {"ちゃ", "しまう", false},  // 〜ちゃう (colloquial)
    {"じゃ", "しまう", false},  // 〜じゃう (colloquial)
    {"もら", "もらう", true},   // 〜てもらう (benefactive)
    {"くれ", "くれる", true},   // 〜てくれる (benefactive)
    {"あげ", "あげる", true},   // 〜てあげる (benefactive)
    {"や", "やる", true},       // 〜てやる (benefactive)
};

// Cost bonus imported from candidate_constants.h:
// candidate::kTeFormAuxBonus

/**
 * @brief Check if inflection suffix contains auxiliary verb patterns
 * Looks for た/て/で/ない/れ patterns that indicate complete inflected forms
 * (as opposed to just renyokei endings like し/み)
 * Note: で is for hatsuonbin te-form (読ん+で, 飛ん+で)
 */
inline bool hasAuxiliarySuffix(std::string_view suffix) {
  if (suffix.empty()) return false;
  // Note: "ます" excluded for MeCab-compatible split (e.g., 申し上げます → 申し上げ + ます)
  return utf8::containsAny(suffix, {"た", "て", "で", "ない", "れ"});
}

}  // namespace

void addCompoundVerbJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const dictionary::DictionaryManager& dict_manager,
    const Scorer& scorer) {
  using CharType = normalize::CharType;

  if (start_pos >= char_types.size()) {
    return;
  }

  // Must start with kanji (V1 verb stem)
  if (char_types[start_pos] != CharType::Kanji) {
    return;
  }

  // Find the kanji portion (V1 stem)
  size_t kanji_end = findCharRegionEnd(char_types, start_pos, 4,
                                        CharType::Kanji);

  // Next must be hiragana (連用形 ending)
  if (kanji_end >= char_types.size() ||
      char_types[kanji_end] != CharType::Hiragana) {
    return;
  }

  // Get the hiragana character (potential 連用形 ending)
  char32_t renyokei_char = codepoints[kanji_end];

  // Check if it's a valid 連用形 ending
  char32_t base_ending = 0;
  for (const auto& pattern : kGodanRenyokei) {
    if (pattern.renyokei == renyokei_char) {
      base_ending = pattern.base;
      break;
    }
  }

  // If not a 連用形 ending, this might be an Ichidan verb
  bool is_ichidan = (base_ending == 0);

  // Position after 連用形 (for Godan) or after stem (for Ichidan)
  size_t v2_start;
  if (is_ichidan) {
    // For ichidan verbs, the stem includes the final hiragana character:
    // - Shimo-ichidan (下一段): え-row (抜け from 抜ける, 食べ from 食べる)
    // - Kami-ichidan (上一段): い-row (落ち from 落ちる, 起き from 起きる)
    // - Suru-variant: じ/ぢ (演じ from 演じる, 感じ from 感じる)
    // B63: We need to skip this hiragana when looking for V2
    char32_t hira = codepoints[kanji_end];
    bool is_e_row_stem = grammar::isERowCodepoint(hira);
    // Note: I-row includes some chars also in kGodanRenyokei (き, ぎ, し, ち, etc.)
    // but by the time we reach this branch (is_ichidan=true), those cases
    // have already been excluded because they set base_ending in the loop above.
    bool is_i_row_stem = grammar::isIRowCodepoint(hira);

    // For E/I-row stems (valid ichidan patterns), V2 starts after the stem
    // For non-E/I-row, look for V2 starting at the hiragana position (e.g., つける)
    // This allows patterns like 見 + つける = 見つける where つ is U-row
    v2_start = (is_e_row_stem || is_i_row_stem) ? (kanji_end + 1) : kanji_end;
  } else {
    v2_start = kanji_end + 1;
  }

  if (v2_start >= codepoints.size()) {
    return;
  }

  // Get byte positions
  size_t start_byte = charPosToBytePos(codepoints, start_pos);
  size_t v2_start_byte = charPosToBytePos(codepoints, v2_start);

  // Inflection analyzer for V2 detection
  static grammar::Inflection inflection;

  // Find extent of hiragana after v2_start for inflection analysis
  size_t v2_hiragana_end = findCharRegionEnd(char_types, v2_start, 8,
                                              CharType::Hiragana);

  // Look for V2 (subsidiary verb)
  // We collect the best match rather than returning immediately.
  // This allows renyokei matches (すぎ) to take precedence over inflection
  // matches (すぎた) when the inflection match includes an auxiliary suffix.
  struct V2Match {
    size_t matched_len = 0;
    std::string compound_base;
    bool is_renyokei = false;      // true if matched via renyokei entry
    bool includes_aux = false;     // true if inflection match includes aux suffix
    bool matched_via_reading = false;  // true if V2 was matched via hiragana reading
    std::string v2_reading;        // V2 hiragana reading (for hiragana te-stem generation)
    float confidence = 0.0F;
    V2VerbType v2_verb_type = V2VerbType::Godan;  // V2 verb type
    std::string_view v2_base_ending;              // V2 base form ending (む, す, etc.)
  };
  V2Match best_match;

  for (const auto& v2_verb : kSubsidiaryVerbs) {
    std::string_view v2_surface(v2_verb.surface);
    std::string_view v2_reading(v2_verb.reading ? v2_verb.reading : "");

    // Determine if this is a renyokei entry by checking if base_form != surface
    // Renyokei entries: 過ぎ (base 過ぎる), 出し (base 出す), etc.
    bool is_renyokei_entry = (std::string(v2_verb.surface) != std::string(v2_verb.surface));

    // Check if text at v2_start matches this V2 verb (kanji or reading)
    bool matched_kanji = false;
    bool matched_reading = false;
    bool matched_inflected = false;
    size_t matched_len = 0;
    bool inflection_includes_aux = false;

    // Try kanji match first
    if (v2_start_byte + v2_surface.size() <= text.size()) {
      std::string_view text_at_v2 = text.substr(v2_start_byte, v2_surface.size());
      if (text_at_v2 == v2_surface) {
        matched_kanji = true;
        matched_len = v2_surface.size();
      }
    }

    // Try reading (hiragana) match if kanji didn't match
    if (!matched_kanji && !v2_reading.empty() &&
        v2_start_byte + v2_reading.size() <= text.size()) {
      std::string_view text_at_v2 = text.substr(v2_start_byte, v2_reading.size());
      if (text_at_v2 == v2_reading) {
        matched_reading = true;
        matched_len = v2_reading.size();
      }
    }

    // Try V2 renyokei (連用形) match for MeCab-compatible split
    // e.g., 申し上げます → 申し上げ + ます (match V2 renyokei "上げ", not full "上げます")
    bool matched_renyokei = false;
    if (!matched_kanji && !matched_reading) {
      // Generate V2 renyokei
      std::string kanji_renyokei = generateKanjiRenyokei(v2_surface, v2_reading, v2_verb.verb_type);
      std::string hira_renyokei = generateRenyokei(v2_reading, "", v2_verb.verb_type);

      // Try kanji renyokei match
      if (!kanji_renyokei.empty() &&
          v2_start_byte + kanji_renyokei.size() <= text.size()) {
        std::string_view text_at_v2 = text.substr(v2_start_byte, kanji_renyokei.size());
        if (text_at_v2 == kanji_renyokei) {
          matched_renyokei = true;
          matched_len = kanji_renyokei.size();
          is_renyokei_entry = true;  // Mark as renyokei match
        }
      }

      // Try hiragana renyokei match if kanji didn't match
      if (!matched_renyokei && !hira_renyokei.empty() &&
          v2_start_byte + hira_renyokei.size() <= text.size()) {
        std::string_view text_at_v2 = text.substr(v2_start_byte, hira_renyokei.size());
        if (text_at_v2 == hira_renyokei) {
          // Skip Ichidan V1 + V2「出る」renyokei (で) match
          // Ichidan verbs use て for te-form, never で.
          // E.g., 付けで should be 付け(VERB)+で(PARTICLE), not 付け出る (compound)
          // But Godan+出る is valid: 飛び出る (飛ぶ→飛び+出る)
          if (is_ichidan && hira_renyokei == "で") {
            continue;  // Skip V2「出る」for Ichidan V1
          }
          matched_renyokei = true;
          matched_len = hira_renyokei.size();
          is_renyokei_entry = true;  // Mark as renyokei match
        }
      }
    }

    // Try inflection analysis for inflected V2 forms (e.g., きった, 込んだ, 巡った)
    // Only for base forms (not renyokei entries) to avoid double-matching
    // Skip if already matched via renyokei to prevent aux detection overriding renyokei match
    if (!matched_kanji && !matched_reading && !matched_renyokei && !v2_reading.empty()) {
      std::string_view base_ending(v2_verb.base_ending);
      // Only try inflection for base forms (ending in る/す/く/う/む/つ/ぶ/ぐ/ぬ or ichidan endings)
      if (utf8::equalsAny(base_ending,
          {"る", "す", "く", "う", "む", "つ", "ぶ", "ぐ", "ぬ",
           "める", "ける", "れる", "える", "げる", "てる", "せる", "ちる"})) {

        // Case 1: Hiragana V2 inflected forms (e.g., きった from きる, かった from かう)
        // Try different lengths for V2 inflected form (shortest match first)
        for (size_t v2_end = v2_start + 2; v2_end <= v2_hiragana_end; ++v2_end) {
          size_t v2_end_byte = charPosToBytePos(codepoints, v2_end);
          std::string v2_text(text.substr(v2_start_byte, v2_end_byte - v2_start_byte));

          // Use analyze() to get all candidates, not just the best one.
          // This is needed because for ambiguous stems (e.g., かった could be
          // from かる, かつ, or かう), we need to find the one matching our V2.
          auto infl_results = inflection.analyze(v2_text);
          std::string expected_base = std::string(v2_reading);

          for (const auto& infl_result : infl_results) {
            // Check if this matches the V2 base form (using reading for comparison)
            // Use 0.3 threshold for inflected forms since short stems get lower confidence
            // Require the suffix to contain actual auxiliary patterns (た/て/etc.),
            // not just renyokei endings (し/み/etc.) to ensure complete inflected form
            if (infl_result.confidence >= 0.3F &&
                infl_result.base_form == expected_base &&
                hasAuxiliarySuffix(infl_result.suffix)) {
              matched_inflected = true;
              matched_len = v2_end_byte - v2_start_byte;
              inflection_includes_aux = true;  // Mark that this match includes aux
              break;
            }
          }
          if (matched_inflected) break;
        }

        // Case 2: Kanji V2 inflected forms (e.g., 巡った from 巡る)
        // Check if text starts with V2 kanji prefix, then analyze hiragana suffix
        if (!matched_inflected && char_types[v2_start] == CharType::Kanji) {
          // Extract kanji prefix from V2 surface (e.g., "巡" from "巡る")
          auto v2_surface_decoded = normalize::utf8::decode(std::string(v2_surface));
          size_t kanji_prefix_len = 0;
          for (size_t idx = 0; idx < v2_surface_decoded.size(); ++idx) {
            char32_t c = v2_surface_decoded[idx];
            if (c >= 0x4E00 && c <= 0x9FFF) {
              ++kanji_prefix_len;
            } else {
              break;
            }
          }

          if (kanji_prefix_len > 0 && kanji_prefix_len < v2_surface_decoded.size()) {
            // Check if text at v2_start matches the kanji prefix
            size_t kanji_prefix_byte_len = 0;
            for (size_t idx = 0; idx < kanji_prefix_len; ++idx) {
              kanji_prefix_byte_len += 3;  // Kanji are 3 bytes in UTF-8
            }

            if (v2_start_byte + kanji_prefix_byte_len <= text.size()) {
              std::string_view text_kanji_prefix = text.substr(v2_start_byte, kanji_prefix_byte_len);
              std::string v2_kanji_prefix = normalize::utf8::encode(
                  std::vector<char32_t>(v2_surface_decoded.begin(),
                                        v2_surface_decoded.begin() + kanji_prefix_len));

              if (text_kanji_prefix == v2_kanji_prefix) {
                // Find the hiragana suffix after the kanji prefix
                size_t hira_start = v2_start + kanji_prefix_len;
                if (hira_start < codepoints.size() &&
                    char_types[hira_start] == CharType::Hiragana) {
                  size_t hira_end = findCharRegionEnd(char_types, hira_start, 6,
                                                       CharType::Hiragana);

                  // Try inflection on kanji+hiragana portion (shortest match first)
                  for (size_t v2_end = hira_start + 1; v2_end <= hira_end; ++v2_end) {
                    size_t v2_end_byte = charPosToBytePos(codepoints, v2_end);
                    std::string v2_text(text.substr(v2_start_byte, v2_end_byte - v2_start_byte));

                    // Use analyze() to search all candidates for matching base form
                    auto infl_results = inflection.analyze(v2_text);
                    for (const auto& infl_result : infl_results) {
                      // Check if base form matches V2 surface (kanji form)
                      // Require the suffix to contain actual auxiliary patterns
                      if (infl_result.confidence >= 0.35F &&
                          infl_result.base_form == v2_surface &&
                          hasAuxiliarySuffix(infl_result.suffix)) {
                        matched_inflected = true;
                        matched_len = v2_end_byte - v2_start_byte;
                        inflection_includes_aux = true;  // Mark that this match includes aux
                        break;
                      }
                    }
                    if (matched_inflected) break;
                  }
                }
              }
            }
          }
        }
      }
    }

    if (!matched_kanji && !matched_reading && !matched_renyokei && !matched_inflected) {
      continue;
    }

    // Build the V1 base form for verification
    std::string v1_base;
    size_t v1_end_byte = is_ichidan ? v2_start_byte : charPosToBytePos(codepoints, kanji_end);
    v1_base = std::string(text.substr(start_byte, v1_end_byte - start_byte));

    if (!is_ichidan) {
      v1_base += normalize::utf8::encode({base_ending});
    } else {
      v1_base += "る";
    }

    // Check if V1 base form is in dictionary
    auto v1_results = dict_manager.lookup(v1_base, 0);
    bool v1_verified = false;
    for (const auto& result : v1_results) {
      if (result.entry != nullptr && result.entry->surface == v1_base &&
          result.entry->pos == core::PartOfSpeech::Verb) {
        v1_verified = true;
        break;
      }
    }

    // Fallback: use inflection analysis for unknown V1 verbs
    // This allows compound verbs like 読み込む where 読む is not in dictionary
    // but is recognizable as a verb by inflection patterns
    if (!v1_verified) {
      bool use_inflection_fallback = true;

      // B65: For multi-kanji stems (2+ kanji), require dictionary match.
      // This prevents spurious compound verbs like 大体分交う where 大体分 is
      // incorrectly analyzed as a verb stem. The inflection analyzer is too lenient
      // for long kanji sequences, accepting them with low confidence.
      // Single-kanji stems like 見 (from 見つける) are more likely to be real verbs.
      size_t kanji_count = kanji_end - start_pos;
      if (kanji_count >= 2) {
        // Multi-kanji stem: don't use inflection fallback
        use_inflection_fallback = false;
      }

      // Special case: single-kanji + に patterns
      // に is both a common particle and the renyokei of Godan-Na verbs (死に→死ぬ).
      // But Godan-Na verbs are rare, while kanji+に+VERB is a very common pattern
      // (e.g., 本について = 本 + に + ついて, not 本ぬ compound).
      // Block inflection fallback for single-kanji + に to prevent false positives.
      char32_t renyokei_char = codepoints[kanji_end];
      if (!is_ichidan && kanji_count == 1 && renyokei_char == U'に') {
        use_inflection_fallback = false;
      }

      // Check if V1 renyokei is known as a non-verb (noun, adjective, etc.)
      // If so, don't form compound verb. E.g., 好き is ADJ, not verb renyokei of 好く.
      if (use_inflection_fallback) {
        size_t v1_renyokei_end = is_ichidan ? v2_start_byte : charPosToBytePos(codepoints, kanji_end + 1);
        std::string v1_renyokei(text.substr(start_byte, v1_renyokei_end - start_byte));
        auto renyokei_results = dict_manager.lookup(v1_renyokei, 0);
        for (const auto& result : renyokei_results) {
          if (result.entry != nullptr && result.entry->surface == v1_renyokei &&
              result.entry->pos != core::PartOfSpeech::Verb) {
            // V1 renyokei is a known non-verb word, don't form compound
            use_inflection_fallback = false;
            break;
          }
        }
      }

      if (use_inflection_fallback) {
        // Get V1 renyokei form for inflection analysis
        size_t v1_renyokei_end = is_ichidan ? v2_start_byte : charPosToBytePos(codepoints, kanji_end + 1);
        std::string v1_renyokei(text.substr(start_byte, v1_renyokei_end - start_byte));

        auto infl_result = inflection.getBest(v1_renyokei);

        // Accept if inflection analysis identifies it as a verb with reasonable confidence
        // and the base form matches our constructed v1_base
        // B63: For ichidan verbs in compound verb context, use lower threshold (0.25)
        // because ichidan patterns get penalized by inflection analyzer's potential/godan ambiguity,
        // but the compound verb context (kanji + e-row + known V2) strongly suggests ichidan verb
        float min_confidence = is_ichidan ? 0.25F : 0.5F;
        if (infl_result.confidence >= min_confidence && infl_result.base_form == v1_base) {
          v1_verified = true;
        }
      }
    }

    // Only generate compound verb candidates when V1 is a verified verb
    // This prevents false positives like 試験に落ちる (試験 is not a verb)
    if (!v1_verified) {
      continue;
    }

    // For inflected V2 matches (Case 1/2), check if the full surface could be
    // an adjective instead of a compound verb. This prevents false positives
    // like 美しかった (adjective) being parsed as 美し+交った (compound verb).
    if (matched_inflected && inflection_includes_aux) {
      // Calculate full compound surface
      size_t compound_end_byte = v2_start_byte + matched_len;
      std::string full_surface(text.substr(start_byte, compound_end_byte - start_byte));

      // Check if full surface could be an i-adjective
      auto full_infl = inflection.getBest(full_surface);
      if (full_infl.confidence >= 0.5F &&
          full_infl.verb_type == grammar::VerbType::IAdjective) {
        // Full surface is likely an adjective, skip compound verb
        continue;
      }
    }

    // Build compound verb base form (V1 renyokei + V2 base form)
    // e.g., 走り + 出す = 走り出す, 走り + だす = 走り出す
    std::string compound_base;
    size_t v1_renyokei_end = is_ichidan ? v2_start_byte : charPosToBytePos(codepoints, kanji_end + 1);
    compound_base = std::string(text.substr(start_byte, v1_renyokei_end - start_byte));
    // Use the pre-defined base_form for V2 (always in kanji form for consistency)
    compound_base += v2_verb.surface;

    // Compare with best match and update if this is better
    // Priority:
    // 1. Longer renyokei match beats shorter (出し > 出)
    // 2. Renyokei exact match beats inflection match with aux
    // 3. Match without aux beats match with aux
    bool should_update = false;
    if (best_match.matched_len == 0) {
      // First valid match
      should_update = true;
    } else if (matched_renyokei && best_match.is_renyokei &&
               matched_len > best_match.matched_len) {
      // Longer renyokei match beats shorter renyokei match
      // This makes 出し (6 bytes) beat 出 (3 bytes) for V1+V2 compounds
      should_update = true;
    } else if (is_renyokei_entry && (matched_kanji || matched_reading) &&
               best_match.includes_aux && !best_match.is_renyokei) {
      // Renyokei exact match beats inflection match that includes aux
      // This makes 食べすぎ (renyokei) beat 食べすぎた (inflection+aux)
      should_update = true;
    } else if (!inflection_includes_aux && best_match.includes_aux) {
      // Match without aux beats match with aux
      should_update = true;
    }

    if (should_update) {
      best_match.matched_len = matched_len;
      best_match.compound_base = compound_base;
      best_match.is_renyokei = is_renyokei_entry && (matched_kanji || matched_reading);
      best_match.includes_aux = inflection_includes_aux;
      best_match.matched_via_reading = matched_reading || matched_inflected;
      best_match.v2_reading = std::string(v2_reading);
      best_match.v2_verb_type = v2_verb.verb_type;
      best_match.v2_base_ending = v2_verb.base_ending;
    }
  }

  // After checking all V2 entries, use the best match if found
  if (best_match.matched_len > 0) {
    // Calculate compound verb end position using matched length
    size_t compound_end_byte = v2_start_byte + best_match.matched_len;

    // Find character position for compound end
    size_t compound_end_pos = v2_start;
    size_t byte_count = v2_start_byte;
    while (compound_end_pos < codepoints.size() && byte_count < compound_end_byte) {
      char32_t code = codepoints[compound_end_pos];
      if (code < 0x80) {
        byte_count += 1;
      } else if (code < 0x800) {
        byte_count += 2;
      } else if (code < 0x10000) {
        byte_count += 3;
      } else {
        byte_count += 4;
      }
      ++compound_end_pos;
    }

    // Build the compound verb surface
    std::string compound_surface(text.substr(start_byte, compound_end_byte - start_byte));

    // Calculate cost
    float base_cost = scorer.posPrior(core::PartOfSpeech::Verb);
    const auto& opts = scorer.joinOpts();
    float final_cost = base_cost + opts.compound_verb_bonus + opts.verified_v1_bonus;

    uint8_t flags = core::LatticeEdge::kFromDictionary;

    lattice.addEdge(compound_surface, static_cast<uint32_t>(start_pos),
                    static_cast<uint32_t>(compound_end_pos), core::PartOfSpeech::Verb,
                    final_cost, flags, best_match.compound_base);

    // MeCab compatibility: Generate te-form euphonic stem candidate
    // This enables MeCab-compatible te-form splitting: 話し合って → 話し合っ|て
    // Without this, the compound verb te-form (話し合って) would be a single token.
    auto [te_stem, uses_de] = generateTeFormStem(
        best_match.compound_base, "", best_match.v2_verb_type, best_match.v2_base_ending);

    // Helper lambda to add te-stem edge
    auto addTeStemEdge = [&](const std::string& stem) {
      if (stem.empty() || stem.size() >= compound_surface.size()) return false;
      std::string_view text_prefix = text.substr(start_byte, stem.size());
      if (text_prefix != stem) return false;

      auto stem_decoded = normalize::utf8::decode(stem);
      size_t stem_end_pos = start_pos + stem_decoded.size();
      if (stem_end_pos > codepoints.size()) return false;

      // Determine ExtendedPOS based on te-form type
      auto te_type = getTeFormType(best_match.v2_base_ending);
      core::ExtendedPOS epos;
      if (te_type == TeFormType::Renyokei) {
        epos = core::ExtendedPOS::VerbRenyokei;   // 話し (サ行)
      } else {
        epos = core::ExtendedPOS::VerbOnbinkei;   // 書い, 買っ, 読ん, etc.
      }

      lattice.addEdge(stem, static_cast<uint32_t>(start_pos),
                      static_cast<uint32_t>(stem_end_pos), core::PartOfSpeech::Verb,
                      final_cost, flags, best_match.compound_base,
                      dictionary::ConjugationType::None, {},
                      core::CandidateOrigin::Unknown, 0.0F, "compound_te_stem",
                      epos, "compound_te_stem");
      return true;
    };

    // Try kanji te-stem first
    bool added_te_stem = addTeStemEdge(te_stem);

    // If kanji te-stem didn't match and V2 was matched via hiragana reading,
    // also try a hiragana te-stem. This handles cases like:
    // 演じきった (input uses hiragana き, not kanji 切)
    if (!added_te_stem && best_match.matched_via_reading &&
        !best_match.v2_reading.empty()) {
      // Build hiragana compound base: V1 renyokei + V2 hiragana reading
      // compound_base is V1 renyokei + V2 kanji surface
      // We need to replace V2 kanji with V2 hiragana
      // Find which V2 verb was matched to get its surface length
      for (const auto& v2_verb : kSubsidiaryVerbs) {
        std::string_view v2_surface(v2_verb.surface);
        if (best_match.compound_base.size() >= v2_surface.size() &&
            best_match.compound_base.compare(
                best_match.compound_base.size() - v2_surface.size(),
                v2_surface.size(), v2_surface) == 0) {
          size_t v1_len = best_match.compound_base.size() - v2_surface.size();
          std::string v1_part = best_match.compound_base.substr(0, v1_len);
          std::string hira_compound_base = v1_part + best_match.v2_reading;
          auto [hira_te_stem, hira_uses_de] = generateTeFormStem(
              hira_compound_base, "", best_match.v2_verb_type, best_match.v2_base_ending);
          addTeStemEdge(hira_te_stem);
          break;
        }
      }
    }
  }
}

void addHiraganaCompoundVerbJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const dictionary::DictionaryManager& dict_manager,
    const Scorer& scorer) {
  using CharType = normalize::CharType;

  if (start_pos >= char_types.size()) {
    return;
  }

  // Must start with hiragana (for all-hiragana compound verbs like やりなおす)
  if (char_types[start_pos] != CharType::Hiragana) {
    return;
  }

  // Get byte position for start
  size_t start_byte = charPosToBytePos(codepoints, start_pos);

  // For each V2 subsidiary verb, check if it appears after a potential V1
  for (const auto& v2_verb : kSubsidiaryVerbs) {
    // Only consider V2 with readings (hiragana patterns)
    if (v2_verb.reading == nullptr) {
      continue;
    }
    std::string_view v2_reading(v2_verb.reading);

    // For hiragana compound verbs, V1 must be at least 2 characters
    // Try different V1 lengths (2-4 characters)
    for (size_t v1_len = 2; v1_len <= 4; ++v1_len) {
      size_t v2_start = start_pos + v1_len;
      if (v2_start >= codepoints.size()) {
        break;
      }

      // All characters in V1 must be hiragana
      bool all_hiragana = true;
      for (size_t idx = start_pos; idx < v2_start; ++idx) {
        if (char_types[idx] != CharType::Hiragana) {
          all_hiragana = false;
          break;
        }
      }
      if (!all_hiragana) {
        continue;
      }

      size_t v2_start_byte = charPosToBytePos(codepoints, v2_start);

      // Check if V2 reading (hiragana) or surface (kanji) matches at v2_start
      std::string_view v2_surface(v2_verb.surface);
      size_t matched_v2_len = 0;

      // Try hiragana reading match first
      if (v2_start_byte + v2_reading.size() <= text.size()) {
        std::string_view text_at_v2 = text.substr(v2_start_byte, v2_reading.size());
        if (text_at_v2 == v2_reading) {
          matched_v2_len = v2_reading.size();
        }
      }

      // Try kanji surface match if hiragana didn't match
      // This handles patterns like やり + 直す (hiragana V1 + kanji V2)
      if (matched_v2_len == 0 && v2_start_byte + v2_surface.size() <= text.size()) {
        std::string_view text_at_v2 = text.substr(v2_start_byte, v2_surface.size());
        if (text_at_v2 == v2_surface) {
          matched_v2_len = v2_surface.size();
        }
      }

      if (matched_v2_len == 0) {
        continue;
      }

      // Extract V1 portion and determine its base form
      std::string_view v1_surface = text.substr(start_byte, v2_start_byte - start_byte);

      // Get the last character of V1 to determine verb type
      char32_t last_char = codepoints[v2_start - 1];

      // Check if it's a valid renyokei ending
      char32_t base_ending = 0;
      for (const auto& pattern : kGodanRenyokei) {
        if (pattern.renyokei == last_char) {
          base_ending = pattern.base;
          break;
        }
      }

      // Build V1 base form
      std::string v1_base;
      bool is_ichidan = (base_ending == 0);

      if (!is_ichidan) {
        // Godan: replace last char with base ending
        v1_base = std::string(v1_surface.substr(0, v1_surface.size() - core::kJapaneseCharBytes));  // Remove last hiragana
        v1_base += normalize::utf8::encode({base_ending});
      } else {
        // Ichidan: add る
        v1_base = std::string(v1_surface) + "る";
      }

      // Verify V1 is in dictionary as a verb
      auto v1_results = dict_manager.lookup(v1_base, 0);
      bool v1_verified = false;
      for (const auto& result : v1_results) {
        if (result.entry != nullptr && result.entry->surface == v1_base &&
            result.entry->pos == core::PartOfSpeech::Verb) {
          v1_verified = true;
          break;
        }
      }

      // Fallback: use inflection analysis for unknown V1 verbs
      if (!v1_verified) {
        static grammar::Inflection inflection;
        auto infl_result = inflection.getBest(std::string(v1_surface));

        if (infl_result.confidence >= 0.5F && infl_result.base_form == v1_base) {
          v1_verified = true;
        }
      }

      if (!v1_verified) {
        continue;  // V1 must be a known verb for hiragana compounds
      }

      // Calculate compound verb end position
      size_t compound_end_byte = v2_start_byte + matched_v2_len;

      // Find character position for compound end
      size_t compound_end_pos = v2_start;
      size_t byte_count = v2_start_byte;
      while (compound_end_pos < codepoints.size() && byte_count < compound_end_byte) {
        char32_t code = codepoints[compound_end_pos];
        if (code < 0x80) {
          byte_count += 1;
        } else if (code < 0x800) {
          byte_count += 2;
        } else if (code < 0x10000) {
          byte_count += 3;
        } else {
          byte_count += 4;
        }
        ++compound_end_pos;
      }

      // Build compound verb surface and base form
      std::string compound_surface(text.substr(start_byte, compound_end_byte - start_byte));

      // Compound base = V1 renyokei + V2 base form (in kanji)
      std::string compound_base = std::string(v1_surface) + v2_verb.surface;

      // Calculate cost
      float base_cost = scorer.posPrior(core::PartOfSpeech::Verb);
      const auto& opts = scorer.joinOpts();
      float final_cost = base_cost + opts.compound_verb_bonus + opts.verified_v1_bonus;

      uint8_t flags = core::LatticeEdge::kFromDictionary;

      lattice.addEdge(compound_surface, static_cast<uint32_t>(start_pos),
                      static_cast<uint32_t>(compound_end_pos), core::PartOfSpeech::Verb,
                      final_cost, flags, compound_base);

      return;  // Found a match, stop searching
    }
  }
}

void addAdjectiveSugiruJoinCandidates(
    core::Lattice& /*lattice*/, std::string_view /*text*/,
    const std::vector<char32_t>& /*codepoints*/, size_t /*start_pos*/,
    const std::vector<normalize::CharType>& /*char_types*/,
    const dictionary::DictionaryManager& /*dict_manager*/,
    const Scorer& /*scorer*/) {
  // MeCab compatibility: i-adjective + すぎる should be split as separate tokens
  // MeCab output for 高すぎる:
  //   高    形容詞,自立,*,*,形容詞・アウオ段,ガル接続,高い,タカ,タカ
  //   すぎる 動詞,非自立,*,*,一段,基本形,すぎる,スギル,スギル
  //
  // Previously this function generated compound verb candidates like 高すぎる (VERB)
  // with lemma=高過ぎる, which is not MeCab compatible.
  //
  // Now we rely on:
  // 1. generateAdjectiveStemCandidates() to create the adjective stem (高 as ADJ)
  // 2. Dictionary entries for すぎる/すぎ (VERB) to handle the verb portion
  //
  // This function is now a no-op for MeCab compatibility.
  return;
}

void addKatakanaSugiruJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const Scorer& scorer) {
  // DISABLED: MeCab splits KATAKANA + すぎる into separate tokens:
  //   シンプル 名詞,一般,*,*,*,*,*
  //   すぎる   動詞,非自立,*,*,一段,基本形,すぎる,スギル,スギル
  //
  // Previously this function generated compound verb candidates like
  // シンプルすぎる (VERB) which is not MeCab compatible.
  //
  // Now we rely on:
  // 1. KATAKANA portions recognized as NOUN
  // 2. Dictionary entries for すぎる/すぎ (VERB) to handle the verb portion
  (void)lattice;
  (void)text;
  (void)codepoints;
  (void)start_pos;
  (void)char_types;
  (void)scorer;
  return;

  using CharType = normalize::CharType;

  if (start_pos >= char_types.size()) {
    return;
  }

  // Must start with katakana
  if (char_types[start_pos] != CharType::Katakana) {
    return;
  }

  // Find the katakana portion (minimum 2 characters for meaningful words)
  // Use large max to find all consecutive katakana
  size_t katakana_end = findCharRegionEnd(char_types, start_pos, 20,
                                           CharType::Katakana);

  // Need at least 2 katakana characters
  if (katakana_end - start_pos < 2) {
    return;
  }

  // Next must be hiragana starting with す (for すぎ)
  if (katakana_end >= char_types.size() ||
      char_types[katakana_end] != CharType::Hiragana) {
    return;
  }

  // Check if すぎ follows the katakana
  size_t start_byte = charPosToBytePos(codepoints, start_pos);
  size_t sugi_start_byte = charPosToBytePos(codepoints, katakana_end);

  std::string_view after_katakana = text.substr(sugi_start_byte);
  constexpr std::string_view kSugi = "すぎ";
  if (after_katakana.size() < kSugi.size() ||
      after_katakana.substr(0, kSugi.size()) != kSugi) {
    return;
  }

  // Build compound verb base form: KATAKANA + すぎる
  std::string katakana_part(text.substr(start_byte, sugi_start_byte - start_byte));
  std::string compound_base = katakana_part + "すぎる";

  // Calculate cost with bonus for katakana+sugiru pattern
  float base_cost = scorer.posPrior(core::PartOfSpeech::Verb);
  float final_cost = base_cost + scorer.joinOpts().compound_verb_bonus;
  uint8_t flags = core::LatticeEdge::kFromDictionary;

  // Generate candidates for different forms of すぎる
  // Pattern 1: KATAKANA + すぎ (renyokei) - て/た/ない are separate tokens
  size_t sugi_renyokei_len = 2;  // すぎ is 2 characters
  size_t renyokei_end_pos = katakana_end + sugi_renyokei_len;

  if (renyokei_end_pos <= codepoints.size()) {
    size_t renyokei_end_byte = charPosToBytePos(codepoints, renyokei_end_pos);
    std::string renyokei_surface(text.substr(start_byte, renyokei_end_byte - start_byte));

    lattice.addEdge(renyokei_surface, static_cast<uint32_t>(start_pos),
                    static_cast<uint32_t>(renyokei_end_pos), core::PartOfSpeech::Verb,
                    final_cost, flags, compound_base);
  }

  // Pattern 2: KATAKANA + すぎる (base form) - as single token
  constexpr std::string_view kSugiru = "すぎる";
  if (after_katakana.size() >= kSugiru.size() &&
      after_katakana.substr(0, kSugiru.size()) == kSugiru) {
    size_t sugiru_char_len = 3;  // すぎる is 3 characters
    size_t sugiru_end_pos = katakana_end + sugiru_char_len;

    if (sugiru_end_pos <= codepoints.size()) {
      size_t sugiru_end_byte = charPosToBytePos(codepoints, sugiru_end_pos);
      std::string sugiru_surface(text.substr(start_byte, sugiru_end_byte - start_byte));

      lattice.addEdge(sugiru_surface, static_cast<uint32_t>(start_pos),
                      static_cast<uint32_t>(sugiru_end_pos), core::PartOfSpeech::Verb,
                      final_cost, flags, compound_base);
    }
  }
}

void addPrefixNounJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const dictionary::DictionaryManager& dict_manager,
    const Scorer& scorer) {
  using CharType = normalize::CharType;

  if (start_pos >= codepoints.size()) {
    return;
  }

  // Check if current character is a productive prefix
  char32_t current_char = codepoints[start_pos];
  const ProductivePrefix* matched_prefix = nullptr;

  for (size_t idx = 0; idx < kNumPrefixes; ++idx) {
    if (kProductivePrefixes[idx].codepoint == current_char) {
      matched_prefix = &kProductivePrefixes[idx];
      break;
    }
  }

  if (matched_prefix == nullptr) {
    return;
  }

  // Check if there's a noun part following
  size_t noun_start = start_pos + 1;
  if (noun_start >= codepoints.size()) {
    return;
  }

  // For most prefixes, the noun part should start with kanji
  if (matched_prefix->needs_kanji) {
    if (char_types[noun_start] != CharType::Kanji) {
      return;
    }
  } else {
    if (char_types[noun_start] != CharType::Kanji &&
        char_types[noun_start] != CharType::Katakana) {
      return;
    }
  }

  // Find the end of the noun part
  CharType noun_type = char_types[noun_start];
  size_t noun_end = findCharRegionEnd(char_types, noun_start,
                                       kMaxNounLenForPrefix, noun_type);

  if (noun_end <= noun_start) {
    return;
  }

  // Check dictionary for compound nouns
  size_t noun_start_byte = charPosToBytePos(codepoints, noun_start);
  auto noun_results = dict_manager.lookup(text, noun_start_byte);
  bool noun_in_dict = false;
  size_t dict_noun_end = noun_end;

  for (const auto& result : noun_results) {
    if (result.entry != nullptr &&
        result.entry->pos == core::PartOfSpeech::Noun) {
      if (result.length > dict_noun_end - noun_start) {
        dict_noun_end = noun_start + result.length;
        noun_in_dict = true;
      } else if (result.length == noun_end - noun_start) {
        noun_in_dict = true;
      }
    }
  }

  if (dict_noun_end > noun_end) {
    noun_end = dict_noun_end;
  } else {
    // Skip single-kanji noun when followed by hiragana (likely verb pattern)
    if (noun_end - noun_start == 1 && noun_end < codepoints.size()) {
      if (char_types[noun_end] == CharType::Hiragana) {
        return;
      }
    }
  }

  // Check if the combined form is already in dictionary
  size_t start_byte = charPosToBytePos(codepoints, start_pos);
  auto combined_results = dict_manager.lookup(text, start_byte);

  for (const auto& result : combined_results) {
    if (result.entry != nullptr &&
        result.length == noun_end - start_pos) {
      return;  // Already in dictionary
    }
  }

  // Generate joined candidate
  size_t end_byte = charPosToBytePos(codepoints, noun_end);
  std::string surface(text.substr(start_byte, end_byte - start_byte));

  float base_cost = scorer.posPrior(core::PartOfSpeech::Noun);
  float final_cost = base_cost + matched_prefix->bonus;

  // Apply length penalty to prevent over-concatenation
  // Prefix + noun should be 2-3 chars total for most verified cases
  // (e.g., 全員=2, 再開=2, 不安=2)
  // Longer unverified combinations should be split
  size_t total_len = noun_end - start_pos;
  if (total_len >= 4 && !noun_in_dict) {
    // Strong penalty for unverified 4+ char combinations
    // Must overcome: prefix_bonus(-0.4) + optimal_length_bonus(-0.5) = -0.9
    // Target: make final cost higher than split path (~1.0)
    // Penalty: +2.0 base, +0.5 per extra char
    final_cost += 2.0F + 0.5F * static_cast<float>(total_len - 4);
  } else if (total_len == 3 && !noun_in_dict) {
    // Moderate penalty for 3-char unverified
    final_cost += 0.8F;
  }

  if (noun_in_dict) {
    final_cost += scorer.joinOpts().verified_noun_bonus;
  }

  uint8_t flags = core::LatticeEdge::kIsUnknown;

  lattice.addEdge(surface, static_cast<uint32_t>(start_pos),
                  static_cast<uint32_t>(noun_end), core::PartOfSpeech::Noun,
                  final_cost, flags, "");
}

void addTeFormAuxiliaryCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const Scorer& scorer) {
  using CharType = normalize::CharType;

  if (start_pos >= codepoints.size()) {
    return;
  }

  // Look for て or で at this position
  char32_t current = codepoints[start_pos];
  if (current != U'て' && current != U'で') {
    return;
  }

  // Skip if preceded by verb renyokei - MeCab splits verb+て+auxiliary
  // E.g., してみる → し + て + みる, 助けてもらう → 助け + て + もらう
  // This prevents generating combined "てもらう" candidate when it should be split
  // Check for: kanji (verb stem), い-row (ichidan renyokei), え-row (ichidan renyokei),
  // っ (sokuonbin)
  if (start_pos > 0) {
    // Kanji: verb stems (見て → 見 + て, 食て → 食 + て, etc.)
    if (char_types[start_pos - 1] == CharType::Kanji) {
      return;
    }
    char32_t prev = codepoints[start_pos - 1];
    // Sokuonbin (っ): godan verbs (買って, 行って, 持って)
    if (prev == U'っ') {
      return;
    }
    // い-row: suru renyokei (し), godan renyokei (き, ぎ, etc.), kami-ichidan (起き, etc.)
    // え-row: shimo-ichidan renyokei (食べ, 助け, 見え, etc.)
    if (grammar::isIRowCodepoint(prev) || grammar::isERowCodepoint(prev)) {
      return;
    }
  }

  // Check if there's hiragana following
  size_t aux_start = start_pos + 1;
  if (aux_start >= codepoints.size() ||
      char_types[aux_start] != CharType::Hiragana) {
    return;
  }

  // Get byte positions
  size_t te_byte = charPosToBytePos(codepoints, start_pos);
  size_t aux_start_byte = charPosToBytePos(codepoints, aux_start);

  // Find the extent of hiragana following て/で
  size_t hiragana_end = findCharRegionEnd(char_types, aux_start, 10,
                                           CharType::Hiragana);

  // Use inflection analysis
  static grammar::Inflection inflection;

  // Try each auxiliary pattern
  for (const auto& aux : kTeFormAuxiliaries) {
    std::string_view stem(aux.stem);

    std::string_view text_after_te = text.substr(aux_start_byte);
    if (text_after_te.size() < stem.size()) {
      continue;
    }

    if (text_after_te.substr(0, stem.size()) != stem) {
      continue;
    }

    size_t stem_char_len = normalize::utf8::decode(std::string(stem)).size();

    // Try different lengths after the stem
    for (size_t aux_end = aux_start + stem_char_len;
         aux_end <= hiragana_end && aux_end <= aux_start + 8; ++aux_end) {
      size_t aux_end_byte = charPosToBytePos(codepoints, aux_end);
      std::string aux_surface(text.substr(aux_start_byte,
                                          aux_end_byte - aux_start_byte));

      auto best = inflection.getBest(aux_surface);
      if (best.confidence > 0.4F && best.base_form == aux.base_form) {
        // Skip negative forms of benefactive verbs
        // E.g., てあげない should be split as て + あげない, not combined
        // This allows proper analysis of patterns like 教えてあげない
        if (aux.is_benefactive) {
          // Check if the surface ends with negative patterns
          bool is_negative = utf8::endsWith(aux_surface, "ない") ||
                             utf8::endsWith(aux_surface, "なく") ||
                             utf8::endsWith(aux_surface, "なかった") ||
                             utf8::endsWith(aux_surface, "なくて");
          if (is_negative) {
            continue;  // Don't create compound for benefactive negative
          }
        }

        size_t combo_end_byte = aux_end_byte;
        std::string combo_surface(text.substr(te_byte, combo_end_byte - te_byte));

        float final_cost = scorer.posPrior(core::PartOfSpeech::Verb) +
                           scorer.joinOpts().te_form_aux_bonus;

        uint8_t flags = core::LatticeEdge::kIsUnknown;

        lattice.addEdge(combo_surface, static_cast<uint32_t>(start_pos),
                        static_cast<uint32_t>(aux_end), core::PartOfSpeech::Verb,
                        final_cost, flags, aux.base_form);

        break;
      }
    }
  }
}

void addVerbSuffixNounJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    [[maybe_unused]] const dictionary::DictionaryManager& dict_manager,
    const Scorer& scorer) {
  using CharType = normalize::CharType;

  if (start_pos >= codepoints.size()) {
    return;
  }

  // Must start with kanji (verb stem)
  if (char_types[start_pos] != CharType::Kanji) {
    return;
  }

  // Look for patterns: Kanji + Hiragana + Suffix(物/方/所)
  // Examples: 食べ物, 飲み物, 読み方, 居場所
  size_t pos = start_pos;

  // Find kanji portion (verb stem)
  size_t kanji_end = pos;
  while (kanji_end < codepoints.size() && char_types[kanji_end] == CharType::Kanji) {
    ++kanji_end;
  }

  if (kanji_end == pos) {
    return;  // No kanji found
  }

  // Look for optional hiragana (verb renyokei suffix like べ, み, き)
  size_t hiragana_end = kanji_end;
  while (hiragana_end < codepoints.size() && char_types[hiragana_end] == CharType::Hiragana) {
    // Only allow 1-2 hiragana characters for renyokei
    if (hiragana_end - kanji_end >= 2) {
      break;
    }
    ++hiragana_end;
  }

  // Reject if hiragana ends with な (na-adjective 連体形, not verb renyokei)
  // e.g., 効率的な方 should NOT become a compound noun (it's 効率+的+な+方)
  if (hiragana_end > kanji_end && codepoints[hiragana_end - 1] == U'な') {
    return;
  }

  // Reject if hiragana ends with た (past form, not verb renyokei)
  // e.g., 書いた方 should NOT become a compound noun (it's 書い+た+方)
  // Correct patterns: 歩き方, 食べ方 (V連用形+方)
  if (hiragana_end > kanji_end && codepoints[hiragana_end - 1] == U'た') {
    return;
  }

  // Reject if hiragana ends with の (genitive particle, not verb renyokei)
  // e.g., 今後の方針 should NOT become 今後の方 + 針 (it's 今後+の+方針)
  // の is a case particle, not a verb renyokei ending
  if (hiragana_end > kanji_end && codepoints[hiragana_end - 1] == U'の') {
    return;
  }

  // Reject if hiragana ends with い (i-adjective basic form, not verb renyokei)
  // e.g., 美しい方 should NOT become a compound noun (it's 美しい + 方[person])
  // Valid patterns: 読み方, 書き方 (verb renyokei + 方[method])
  if (hiragana_end > kanji_end && codepoints[hiragana_end - 1] == U'い') {
    return;
  }

  // Reject if hiragana is a single case particle (not verb renyokei)
  // e.g., 東京都渋谷区に所在 should NOT become 東京都渋谷区に所 + ... (it's ...+に+所在+...)
  // Case particles: に, で, と, を, が, は, へ, も, か, や
  // These cannot be verb renyokei endings
  if (hiragana_end - kanji_end == 1) {
    char32_t hira_char = codepoints[kanji_end];
    if (hira_char == U'に' || hira_char == U'で' || hira_char == U'と' ||
        hira_char == U'を' || hira_char == U'が' || hira_char == U'は' ||
        hira_char == U'へ' || hira_char == U'も' || hira_char == U'か' ||
        hira_char == U'や') {
      return;
    }
  }

  // Check for suffix kanji: 物, 方, 所
  if (hiragana_end >= codepoints.size()) {
    return;
  }

  char32_t suffix_char = codepoints[hiragana_end];
  bool is_mono_suffix = (suffix_char == U'物');
  bool is_kata_suffix = (suffix_char == U'方');
  bool is_tokoro_suffix = (suffix_char == U'所');

  if (!is_mono_suffix && !is_kata_suffix && !is_tokoro_suffix) {
    return;
  }

  // We need at least some hiragana between kanji and suffix (verb renyokei ending)
  // Exception: single kanji + suffix is allowed for some patterns
  if (hiragana_end == kanji_end && kanji_end - start_pos < 2) {
    return;  // Too short without hiragana
  }

  // Build the compound noun surface
  size_t end_pos = hiragana_end + 1;  // Include suffix
  size_t start_byte = charPosToBytePos(codepoints, start_pos);
  size_t end_byte = charPosToBytePos(codepoints, end_pos);

  std::string surface(text.substr(start_byte, end_byte - start_byte));

  // Calculate cost with bonus for compound noun pattern
  float base_cost = scorer.posPrior(core::PartOfSpeech::Noun);
  constexpr float kCompoundNounBonus = -1.0F;  // Moderate bonus
  float final_cost = base_cost + kCompoundNounBonus;

  uint8_t flags = core::LatticeEdge::kFromDictionary;

  lattice.addEdge(surface, static_cast<uint32_t>(start_pos),
                  static_cast<uint32_t>(end_pos), core::PartOfSpeech::Noun,
                  final_cost, flags, surface);  // lemma = surface for compound nouns
}

void addTaruAdjectiveJoinCandidates(
    core::Lattice& lattice, std::string_view text,
    const std::vector<char32_t>& codepoints, size_t start_pos,
    const std::vector<normalize::CharType>& char_types,
    const Scorer& scorer) {
  using CharType = normalize::CharType;

  if (start_pos >= codepoints.size()) {
    return;
  }

  // Must start with kanji
  if (char_types[start_pos] != CharType::Kanji) {
    return;
  }

  // Look for X然と pattern where X is 1+ kanji
  // Need at least 3 characters: X + 然 + と
  if (start_pos + 2 >= codepoints.size()) {
    return;
  }

  // Find the kanji portion (including 然)
  size_t kanji_end = start_pos + 1;
  while (kanji_end < codepoints.size() && char_types[kanji_end] == CharType::Kanji) {
    ++kanji_end;
  }

  // Need at least 2 kanji, and the last one must be 然
  if (kanji_end - start_pos < 2) {
    return;
  }

  // Check if the last kanji is 然
  char32_t last_kanji = codepoints[kanji_end - 1];
  if (last_kanji != U'然') {
    return;
  }

  // Next character must be と (hiragana)
  if (kanji_end >= codepoints.size() || codepoints[kanji_end] != U'と') {
    return;
  }

  // Build the surface: X然と
  size_t start_byte = charPosToBytePos(codepoints, start_pos);
  size_t end_pos = kanji_end + 1;  // Include と
  size_t end_byte = charPosToBytePos(codepoints, end_pos);

  std::string surface(text.substr(start_byte, end_byte - start_byte));

  // X然 without と is the lemma
  size_t zen_end_byte = charPosToBytePos(codepoints, kanji_end);
  std::string lemma(text.substr(start_byte, zen_end_byte - start_byte));

  // Calculate cost with bonus for this pattern
  float base_cost = scorer.posPrior(core::PartOfSpeech::Adverb);
  constexpr float kTaruAdverbBonus = -1.5F;  // Strong bonus to beat Noun + Particle
  float final_cost = base_cost + kTaruAdverbBonus;

  uint8_t flags = core::LatticeEdge::kFromDictionary;

  lattice.addEdge(surface, static_cast<uint32_t>(start_pos),
                  static_cast<uint32_t>(end_pos), core::PartOfSpeech::Adverb,
                  final_cost, flags, lemma);
}

}  // namespace suzume::analysis
