/**
 * @file inflection.cpp
 * @brief Connection-based reverse inflection analysis implementation
 */

#include "inflection.h"

#include <algorithm>

namespace suzume::grammar {

namespace {

// Verb ending patterns for direct stem matching
struct VerbEnding {
  std::string suffix;       // Ending suffix
  std::string base_suffix;  // Base form suffix to restore
  VerbType verb_type;       // Verb type
  uint16_t provides_conn;   // Connection ID this stem provides
  bool is_onbin;            // True if this is euphonic form
};

// All verb endings for reverse lookup
const std::vector<VerbEnding>& getVerbEndings() {
  static const std::vector<VerbEnding> kEndings = {
      // 五段カ行 (書く)
      {"い", "く", VerbType::GodanKa, conn::kVerbOnbinkei, true},
      {"っ", "く", VerbType::GodanKa, conn::kVerbOnbinkei, true},  // Irregular: いく only
      {"き", "く", VerbType::GodanKa, conn::kVerbRenyokei, false},
      {"か", "く", VerbType::GodanKa, conn::kVerbMizenkei, false},
      {"く", "く", VerbType::GodanKa, conn::kVerbBase, false},
      {"け", "く", VerbType::GodanKa, conn::kVerbPotential, false},  // Potential stem
      {"こ", "く", VerbType::GodanKa, conn::kVerbVolitional, false}, // Volitional stem

      // 五段ガ行 (泳ぐ)
      {"い", "ぐ", VerbType::GodanGa, conn::kVerbOnbinkei, true},
      {"ぎ", "ぐ", VerbType::GodanGa, conn::kVerbRenyokei, false},
      {"が", "ぐ", VerbType::GodanGa, conn::kVerbMizenkei, false},
      {"げ", "ぐ", VerbType::GodanGa, conn::kVerbPotential, false},  // Potential stem
      {"ご", "ぐ", VerbType::GodanGa, conn::kVerbVolitional, false}, // Volitional stem
      {"ぐ", "ぐ", VerbType::GodanGa, conn::kVerbBase, false},       // Base/dictionary form

      // 五段サ行 (話す) - no onbin
      {"し", "す", VerbType::GodanSa, conn::kVerbRenyokei, false},
      {"し", "す", VerbType::GodanSa, conn::kVerbOnbinkei, true},
      {"さ", "す", VerbType::GodanSa, conn::kVerbMizenkei, false},
      {"せ", "す", VerbType::GodanSa, conn::kVerbPotential, false},  // Potential stem
      {"そ", "す", VerbType::GodanSa, conn::kVerbVolitional, false}, // Volitional stem
      {"す", "す", VerbType::GodanSa, conn::kVerbBase, false},       // Base/dictionary form

      // 五段ラ行 (取る) - most common っ-onbin, prioritized
      // Note: "れ" potential stem removed - conflicts with Ichidan stems (忘れる etc.)
      {"っ", "る", VerbType::GodanRa, conn::kVerbOnbinkei, true},
      {"り", "る", VerbType::GodanRa, conn::kVerbRenyokei, false},
      {"ら", "る", VerbType::GodanRa, conn::kVerbMizenkei, false},
      {"ろ", "る", VerbType::GodanRa, conn::kVerbVolitional, false}, // Volitional stem

      // 五段タ行 (持つ)
      {"っ", "つ", VerbType::GodanTa, conn::kVerbOnbinkei, true},
      {"ち", "つ", VerbType::GodanTa, conn::kVerbRenyokei, false},
      {"た", "つ", VerbType::GodanTa, conn::kVerbMizenkei, false},
      {"て", "つ", VerbType::GodanTa, conn::kVerbPotential, false},  // Potential stem
      {"と", "つ", VerbType::GodanTa, conn::kVerbVolitional, false}, // Volitional stem
      {"つ", "つ", VerbType::GodanTa, conn::kVerbBase, false},       // Base/dictionary form

      // 五段マ行 (読む) - most common ん-onbin, prioritized
      {"ん", "む", VerbType::GodanMa, conn::kVerbOnbinkei, true},
      {"み", "む", VerbType::GodanMa, conn::kVerbRenyokei, false},
      {"ま", "む", VerbType::GodanMa, conn::kVerbMizenkei, false},
      {"め", "む", VerbType::GodanMa, conn::kVerbPotential, false},  // Potential stem
      {"も", "む", VerbType::GodanMa, conn::kVerbVolitional, false}, // Volitional stem
      {"む", "む", VerbType::GodanMa, conn::kVerbBase, false},       // Base/dictionary form

      // 五段バ行 (遊ぶ)
      {"ん", "ぶ", VerbType::GodanBa, conn::kVerbOnbinkei, true},
      {"び", "ぶ", VerbType::GodanBa, conn::kVerbRenyokei, false},
      {"ば", "ぶ", VerbType::GodanBa, conn::kVerbMizenkei, false},
      {"べ", "ぶ", VerbType::GodanBa, conn::kVerbPotential, false},  // Potential stem
      {"ぼ", "ぶ", VerbType::GodanBa, conn::kVerbVolitional, false}, // Volitional stem
      {"ぶ", "ぶ", VerbType::GodanBa, conn::kVerbBase, false},       // Base/dictionary form

      // 五段ナ行 (死ぬ) - rare
      {"ん", "ぬ", VerbType::GodanNa, conn::kVerbOnbinkei, true},
      {"に", "ぬ", VerbType::GodanNa, conn::kVerbRenyokei, false},
      {"な", "ぬ", VerbType::GodanNa, conn::kVerbMizenkei, false},
      {"ね", "ぬ", VerbType::GodanNa, conn::kVerbPotential, false},  // Potential stem
      {"の", "ぬ", VerbType::GodanNa, conn::kVerbVolitional, false}, // Volitional stem
      {"ぬ", "ぬ", VerbType::GodanNa, conn::kVerbBase, false},       // Base/dictionary form

      // 五段ワ行 (買う)
      {"っ", "う", VerbType::GodanWa, conn::kVerbOnbinkei, true},
      {"い", "う", VerbType::GodanWa, conn::kVerbRenyokei, false},
      {"わ", "う", VerbType::GodanWa, conn::kVerbMizenkei, false},
      {"え", "う", VerbType::GodanWa, conn::kVerbPotential, false},  // Potential stem
      {"お", "う", VerbType::GodanWa, conn::kVerbVolitional, false}, // Volitional stem
      {"う", "う", VerbType::GodanWa, conn::kVerbBase, false},       // Base/dictionary form

      // 一段 (食べる)
      {"", "る", VerbType::Ichidan, conn::kVerbOnbinkei, true},
      {"", "る", VerbType::Ichidan, conn::kVerbRenyokei, false},
      {"", "る", VerbType::Ichidan, conn::kVerbMizenkei, false},
      {"よ", "る", VerbType::Ichidan, conn::kVerbVolitional, false}, // Volitional stem
      {"る", "る", VerbType::Ichidan, conn::kVerbBase, false},       // Base/dictionary form

      // サ変 (する)
      {"し", "する", VerbType::Suru, conn::kVerbOnbinkei, true},
      {"し", "する", VerbType::Suru, conn::kVerbRenyokei, false},
      {"し", "する", VerbType::Suru, conn::kVerbMizenkei, false},  // しない
      {"さ", "する", VerbType::Suru, conn::kVerbMizenkei, false},  // させる/される
      {"しよ", "する", VerbType::Suru, conn::kVerbVolitional, false}, // しよう
      {"する", "する", VerbType::Suru, conn::kVerbBase, false},    // Base/dictionary form
      {"す", "する", VerbType::Suru, conn::kVerbBase, false},      // すべき special

      // カ変 (来る)
      {"き", "くる", VerbType::Kuru, conn::kVerbOnbinkei, true},
      {"き", "くる", VerbType::Kuru, conn::kVerbRenyokei, false},
      {"こ", "くる", VerbType::Kuru, conn::kVerbMizenkei, false},
      {"こよ", "くる", VerbType::Kuru, conn::kVerbVolitional, false}, // こよう
      {"くる", "くる", VerbType::Kuru, conn::kVerbBase, false},    // Base/dictionary form

      // い形容詞 (美しい)
      {"", "い", VerbType::IAdjective, conn::kIAdjStem, false},
  };
  return kEndings;
}

// Check if stem ends with e-row hiragana (common Ichidan endings)
bool endsWithERow(std::string_view stem) {
  if (stem.size() < 3) {
    return false;
  }
  // Get last character (UTF-8: hiragana is 3 bytes)
  std::string_view last = stem.substr(stem.size() - 3);
  // E-row hiragana: え, け, せ, て, ね, へ, め, れ, べ, ぺ, げ, ぜ, で
  return last == "え" || last == "け" || last == "せ" || last == "て" ||
         last == "ね" || last == "へ" || last == "め" || last == "れ" ||
         last == "べ" || last == "ぺ" || last == "げ" || last == "ぜ" ||
         last == "で";
}

// Check if stem ends with specific hiragana patterns
bool endsWithChar(std::string_view stem, const char* chars[], size_t count) {
  if (stem.size() < 3) {
    return false;
  }
  std::string_view last = stem.substr(stem.size() - 3);
  for (size_t idx = 0; idx < count; ++idx) {
    if (last == chars[idx]) {
      return true;
    }
  }
  return false;
}

// Onbin endings: い, っ, ん
const char* kOnbinEndings[] = {"い", "っ", "ん"};
const size_t kOnbinCount = 3;

// Mizenkei (a-row) endings: か, が, さ, た, な, ば, ま, ら, わ
const char* kMizenkeiEndings[] = {"か", "が", "さ", "た", "な", "ば", "ま", "ら", "わ"};
const size_t kMizenkeiCount = 9;

// Renyokei (i-row) endings: き, ぎ, し, ち, に, び, み, り
const char* kRenyokeiEndings[] = {"き", "ぎ", "し", "ち", "に", "び", "み", "り"};
const size_t kRenyokeiCount = 8;

// Check if stem ends with kanji (CJK Unified Ideographs: U+4E00-U+9FFF)
// Used to identify potential サ変 verb stems (勉強, 準備, etc.)
bool endsWithKanji(std::string_view stem) {
  if (stem.size() < 3) {
    return false;
  }
  // Decode last UTF-8 character
  // Kanji in UTF-8 is 3 bytes: E4-E9 xx xx (U+4E00-U+9FFF)
  const unsigned char* ptr =
      reinterpret_cast<const unsigned char*>(stem.data() + stem.size() - 3);
  if ((ptr[0] & 0xF0) == 0xE0) {
    // 3-byte UTF-8 sequence
    char32_t codepoint = ((ptr[0] & 0x0F) << 12) | ((ptr[1] & 0x3F) << 6) |
                         (ptr[2] & 0x3F);
    // CJK Unified Ideographs: U+4E00-U+9FFF
    // CJK Extension A: U+3400-U+4DBF
    return (codepoint >= 0x4E00 && codepoint <= 0x9FFF) ||
           (codepoint >= 0x3400 && codepoint <= 0x4DBF);
  }
  return false;
}

// Calculate confidence based on match quality and context
float calculateConfidence(VerbType type, std::string_view stem,
                          size_t aux_total_len, size_t aux_count,
                          uint16_t required_conn) {
  float base = 0.6F;
  size_t stem_len = stem.size();

  // Stem length penalties/bonuses
  // Very long stems are suspicious (likely wrong analysis)
  if (stem_len >= 12) {
    base -= 0.25F;
  } else if (stem_len >= 9) {
    base -= 0.15F;
  } else if (stem_len >= 6) {
    // 2-char stems (6 bytes) are common
    base += 0.02F;
  } else if (stem_len >= 3) {
    // 1-char stems (3 bytes) are possible but less common
    base += 0.01F;
  }

  // Longer auxiliary chain = higher confidence (matched more grammar)
  base += static_cast<float>(aux_total_len) * 0.02F;

  // Ichidan validation based on connection context
  if (type == VerbType::Ichidan) {
    if (endsWithERow(stem)) {
      // E-row endings (食べ, 見せ, etc.) are very common for Ichidan
      // But 2-char stems with e-row ending (書け, 読め) could be Godan potential
      // Apply penalty when in renyokei context and stem looks like potential
      if (stem_len == 6 && required_conn == conn::kVerbRenyokei &&
          endsWithKanji(stem.substr(0, 3))) {
        // 書け + ませんでした could be Ichidan 書ける or Godan potential 書く
        // Prefer Godan potential interpretation when first char is kanji
        base -= 0.15F;
      } else {
        base += 0.12F;
      }
    } else {
      // Check for context-specific Godan patterns
      bool looks_godan = false;

      // Onbin context: stems ending in い, っ, ん suggest Godan
      if (required_conn == conn::kVerbOnbinkei) {
        looks_godan = endsWithChar(stem, kOnbinEndings, kOnbinCount);
      }
      // Mizenkei context: stems ending in a-row suggest Godan
      else if (required_conn == conn::kVerbMizenkei) {
        looks_godan = endsWithChar(stem, kMizenkeiEndings, kMizenkeiCount);
      }
      // Renyokei context: stems ending in i-row suggest Godan
      else if (required_conn == conn::kVerbRenyokei) {
        looks_godan = endsWithChar(stem, kRenyokeiEndings, kRenyokeiCount);
      }

      if (looks_godan) {
        // Stem matches Godan conjugation pattern for this context
        base -= 0.15F;
      }
      // Other endings (起き for onbin, etc.) are valid Ichidan - no penalty
    }

    // Single-kanji Ichidan stems are rare but valid (見る, 着る, etc.)
    // Problem: 殺されて can be parsed as 殺 + されて (wrong) or 殺さ + れて (correct)
    // The させられた/させられて patterns (15 bytes) are legitimate Ichidan causative-passive
    // When aux_count == 1 and aux_total_len == 15, it's likely させられた (correct)
    // When aux_count >= 2 (e.g., きた + されて), it's likely wrong
    if (stem_len == 3 && endsWithKanji(stem)) {
      if (aux_count == 1 && aux_total_len >= 12) {
        // Single long aux match like させられた (15 bytes) or させられる (15 bytes)
        // This is legitimate Ichidan causative-passive (見させられた → 見る)
        base += 0.10F;
      } else {
        // Multiple aux matches or short single match
        // Likely wrong match via される pattern (殺されて → 殺る WRONG)
        // Apply penalty to prefer the れて path (殺されて → 殺さ + れて → 殺す)
        base -= 0.30F;
      }
    }
  }

  // Kuru validation: only 来る conjugates as Kuru
  if (type == VerbType::Kuru) {
    if (stem != "来") {
      // Any stem other than 来 is invalid for Kuru
      base -= 0.25F;
    }
  }

  // I-adjective validation: single-kanji stems are very rare
  // Most I-adjectives have multi-character stems (美しい, 高い, 長い)
  // Single-kanji stems like 書い (from mismatched 書く) are usually wrong
  if (type == VerbType::IAdjective && stem_len == 3) {
    base -= 0.25F;  // Strong penalty for single-kanji I-adjective stems
  }

  // I-adjective stems ending with e-row hiragana are extremely rare
  // E-row endings (食べ, 見え, 教え) are typical of ichidan verb stems
  // This prevents "食べそう" from being parsed as i-adjective "食べい"
  if (type == VerbType::IAdjective && endsWithERow(stem)) {
    base -= 0.35F;  // Strong penalty for e-row i-adjective stems
  }

  // I-adjective stems that look like godan verb renyokei (kanji + i-row)
  // Pattern: 書き, 読み, 飲み (2 chars = 6 bytes, ends with i-row hiragana)
  // These are typical godan verb stems, not i-adjective stems
  // This prevents "書きそう" from being parsed as i-adjective "書きい"
  // Apply when the stem could be confused with verb renyokei
  if (type == VerbType::IAdjective && stem_len == 6) {
    std::string_view last = stem.substr(3);  // Last 3 bytes = 1 hiragana
    if (last == "き" || last == "ぎ" || last == "ち" ||
        last == "に" || last == "び" || last == "み" || last == "り" ||
        last == "い") {
      // Check if first char is kanji (typical verb renyokei pattern)
      // But exclude し which is common in real i-adj stems like 美し, 楽し
      if (endsWithKanji(stem.substr(0, 3))) {
        base -= 0.30F;  // Penalty for kanji + i-row pattern
      }
    }
  }

  // Godan verb stems in onbinkei context should not end with a-row hiragana
  // a-row endings (か, が, さ, etc.) are mizenkei forms, not onbinkei
  // This prevents "美しかった" from being parsed as verb "美しかる"
  if (required_conn == conn::kVerbOnbinkei && stem_len >= 6) {
    std::string_view last = stem.substr(stem_len - 3);
    if (last == "か" || last == "が" || last == "さ" || last == "た" ||
        last == "な" || last == "ば" || last == "ま" || last == "ら" ||
        last == "わ") {
      // Stems ending in a-row are suspicious for onbinkei context
      base -= 0.30F;
    }
  }

  // Godan potential form boost: 書けない → 書く is more likely than 書ける
  // Potential forms of Godan verbs behave like Ichidan, creating ambiguity
  // Only boost when stem length is 1 char (3 bytes) - typical for potential forms
  // This prevents false matches like 食べて → 食ぶ (should be 食べる)
  if (required_conn == conn::kVerbPotential && stem_len == 3) {
    if (type != VerbType::Ichidan && type != VerbType::Suru &&
        type != VerbType::Kuru) {
      base += 0.10F;  // Boost Godan potential interpretation
    }
  }

  // Suru vs GodanSa disambiguation
  // Multi-kanji stems strongly suggest サ変 verb (勉強する, 準備する)
  // Single-kanji stems (出す, 消す) are typically GodanSa
  // Stems ending in hiragana/katakana suggest Godan verb (話す, 返す)
  if (endsWithKanji(stem)) {
    bool is_shi_context = (required_conn == conn::kVerbRenyokei ||
                           required_conn == conn::kVerbOnbinkei);
    if (is_shi_context) {
      // Only apply Suru boost for 2-kanji stems (6 bytes)
      // Single-kanji stems (3 bytes) like 出す, 消す are GodanSa
      // Longer stems (9+ bytes) might be verb compounds (考え直す)
      if (stem_len == 6) {
        if (type == VerbType::Suru) {
          base += 0.20F;  // Boost Suru for 2-kanji stems in し-context
        } else if (type == VerbType::GodanSa) {
          base -= 0.30F;  // Strong penalty for GodanSa with 2-kanji stems
        }
      } else if (stem_len >= 9) {
        // Longer stems (3+ kanji) might be verb compounds - reduce boost
        if (type == VerbType::Suru) {
          base += 0.05F;  // Small boost for longer Suru stems
        }
      } else if (stem_len == 3) {
        // Single-kanji stem: prefer GodanSa (出す, 消す, etc.)
        if (type == VerbType::GodanSa) {
          base += 0.10F;  // Boost GodanSa for single-kanji stems
        } else if (type == VerbType::Suru) {
          base -= 0.15F;  // Penalty for Suru with single-kanji stems
        }
      }
    }
    // In mizenkei context for single-kanji, also boost GodanSa
    if (required_conn == conn::kVerbMizenkei && stem_len == 3) {
      if (type == VerbType::GodanSa) {
        base += 0.10F;  // Slight boost for single-kanji GodanSa (殺す, 消す)
      }
    }
  }

  return std::min(0.95F, std::max(0.5F, base));
}

}  // namespace

Inflection::Inflection() { initAuxiliaries(); }

void Inflection::initAuxiliaries() {
  using namespace conn;

  // Auxiliary entries: surface, lemma, left_id, right_id, required_conn
  auxiliaries_ = {
      // === Polite (ます系) ===
      {"ます", "ます", kAuxMasu, kAuxOutMasu, kVerbRenyokei},
      {"ました", "ます", kAuxMasu, kAuxOutTa, kVerbRenyokei},
      {"ません", "ます", kAuxMasu, kAuxOutBase, kVerbRenyokei},

      // === Past (た系) ===
      {"た", "た", kAuxTa, kAuxOutTa, kVerbOnbinkei},
      {"だ", "た", kAuxTa, kAuxOutTa, kVerbOnbinkei},

      // === Te-form (て系) ===
      {"て", "て", kAuxTe, kAuxOutTe, kVerbOnbinkei},
      {"で", "て", kAuxTe, kAuxOutTe, kVerbOnbinkei},

      // === Progressive (ている系) ===
      {"いる", "いる", kAuxTeiru, kAuxOutBase, kAuxOutTe},
      {"いた", "いる", kAuxTeiru, kAuxOutTa, kAuxOutTe},
      {"います", "いる", kAuxTeiru, kAuxOutMasu, kAuxOutTe},
      {"いました", "いる", kAuxTeiru, kAuxOutTa, kAuxOutTe},
      {"いて", "いる", kAuxTeiru, kAuxOutTe, kAuxOutTe},

      // === Completion (てしまう系) ===
      {"しまう", "しまう", kAuxTeshimau, kAuxOutBase, kAuxOutTe},
      {"しまった", "しまう", kAuxTeshimau, kAuxOutTa, kAuxOutTe},
      {"しまって", "しまう", kAuxTeshimau, kAuxOutTe, kAuxOutTe},
      {"しまいます", "しまう", kAuxTeshimau, kAuxOutMasu, kAuxOutTe},
      {"ちゃう", "しまう", kAuxTeshimau, kAuxOutBase, kAuxOutTe},
      {"ちゃった", "しまう", kAuxTeshimau, kAuxOutTa, kAuxOutTe},
      {"ちゃって", "しまう", kAuxTeshimau, kAuxOutTe, kAuxOutTe},
      {"じゃう", "しまう", kAuxTeshimau, kAuxOutBase, kAuxOutTe},
      {"じゃった", "しまう", kAuxTeshimau, kAuxOutTa, kAuxOutTe},
      {"じゃって", "しまう", kAuxTeshimau, kAuxOutTe, kAuxOutTe},
      // Colloquial direct connection (contraction skips て/で)
      {"ちゃう", "しまう", kAuxTeshimau, kAuxOutBase, kVerbOnbinkei},
      {"ちゃった", "しまう", kAuxTeshimau, kAuxOutTa, kVerbOnbinkei},
      {"ちゃって", "しまう", kAuxTeshimau, kAuxOutTe, kVerbOnbinkei},
      {"じゃう", "しまう", kAuxTeshimau, kAuxOutBase, kVerbOnbinkei},
      {"じゃった", "しまう", kAuxTeshimau, kAuxOutTa, kVerbOnbinkei},
      {"じゃって", "しまう", kAuxTeshimau, kAuxOutTe, kVerbOnbinkei},

      // === Preparation (ておく系) ===
      {"おく", "おく", kAuxTeoku, kAuxOutBase, kAuxOutTe},
      {"おいた", "おく", kAuxTeoku, kAuxOutTa, kAuxOutTe},
      {"おいて", "おく", kAuxTeoku, kAuxOutTe, kAuxOutTe},
      {"おきます", "おく", kAuxTeoku, kAuxOutMasu, kAuxOutTe},
      {"とく", "おく", kAuxTeoku, kAuxOutBase, kAuxOutTe},
      {"といた", "おく", kAuxTeoku, kAuxOutTa, kAuxOutTe},

      // === Direction (てくる/ていく系) ===
      {"くる", "くる", kAuxTekuru, kAuxOutBase, kAuxOutTe},
      {"きた", "くる", kAuxTekuru, kAuxOutTa, kAuxOutTe},
      {"きて", "くる", kAuxTekuru, kAuxOutTe, kAuxOutTe},
      {"きます", "くる", kAuxTekuru, kAuxOutMasu, kAuxOutTe},
      {"いく", "いく", kAuxTeiku, kAuxOutBase, kAuxOutTe},
      {"いった", "いく", kAuxTeiku, kAuxOutTa, kAuxOutTe},
      {"いって", "いく", kAuxTeiku, kAuxOutTe, kAuxOutTe},

      // === Attempt (てみる系) ===
      {"みる", "みる", kAuxTemiru, kAuxOutBase, kAuxOutTe},
      {"みた", "みる", kAuxTemiru, kAuxOutTa, kAuxOutTe},
      {"みて", "みる", kAuxTemiru, kAuxOutTe, kAuxOutTe},
      {"みます", "みる", kAuxTemiru, kAuxOutMasu, kAuxOutTe},

      // === Benefactive (てもらう/てくれる/てあげる系) ===
      {"もらう", "もらう", kAuxTemorau, kAuxOutBase, kAuxOutTe},
      {"もらった", "もらう", kAuxTemorau, kAuxOutTa, kAuxOutTe},
      {"もらいます", "もらう", kAuxTemorau, kAuxOutMasu, kAuxOutTe},
      {"もらって", "もらう", kAuxTemorau, kAuxOutTe, kAuxOutTe},
      {"くれる", "くれる", kAuxTekureru, kAuxOutBase, kAuxOutTe},
      {"くれた", "くれる", kAuxTekureru, kAuxOutTa, kAuxOutTe},
      {"くれます", "くれる", kAuxTekureru, kAuxOutMasu, kAuxOutTe},
      {"あげる", "あげる", kAuxTeageru, kAuxOutBase, kAuxOutTe},
      {"あげた", "あげる", kAuxTeageru, kAuxOutTa, kAuxOutTe},
      {"あげます", "あげる", kAuxTeageru, kAuxOutMasu, kAuxOutTe},

      // === Negation (ない系) ===
      {"ない", "ない", kAuxNai, kAuxOutBase, kVerbMizenkei},
      {"なかった", "ない", kAuxNai, kAuxOutTa, kVerbMizenkei},
      {"なくて", "ない", kAuxNai, kAuxOutTe, kVerbMizenkei},

      // === Desire (たい系) ===
      {"たい", "たい", kAuxTai, kAuxOutBase, kVerbRenyokei},
      {"たかった", "たい", kAuxTai, kAuxOutTa, kVerbRenyokei},
      {"たくない", "たい", kAuxTai, kAuxOutBase, kVerbRenyokei},

      // === Passive/Potential (れる/られる系) ===
      {"れる", "れる", kAuxReru, kAuxOutBase, kVerbMizenkei},
      {"れた", "れる", kAuxReru, kAuxOutTa, kVerbMizenkei},
      {"れて", "れる", kAuxReru, kAuxOutTe, kVerbMizenkei},
      {"れます", "れる", kAuxReru, kAuxOutMasu, kVerbMizenkei},
      {"れない", "れる", kAuxReru, kAuxOutBase, kVerbMizenkei},
      {"れなかった", "れる", kAuxReru, kAuxOutTa, kVerbMizenkei},
      {"られる", "られる", kAuxReru, kAuxOutBase, kVerbMizenkei},
      {"られた", "られる", kAuxReru, kAuxOutTa, kVerbMizenkei},
      {"られて", "られる", kAuxReru, kAuxOutTe, kVerbMizenkei},
      {"られます", "られる", kAuxReru, kAuxOutMasu, kVerbMizenkei},
      {"られない", "られる", kAuxReru, kAuxOutBase, kVerbMizenkei},
      {"られなかった", "られる", kAuxReru, kAuxOutTa, kVerbMizenkei},

      // === Causative (せる/させる系) ===
      {"せる", "せる", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"せた", "せる", kAuxSeru, kAuxOutTa, kVerbMizenkei},
      {"せて", "せる", kAuxSeru, kAuxOutTe, kVerbMizenkei},
      {"せない", "せる", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"せなかった", "せる", kAuxSeru, kAuxOutTa, kVerbMizenkei},
      {"せます", "せる", kAuxSeru, kAuxOutMasu, kVerbMizenkei},
      {"させる", "させる", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"させた", "させる", kAuxSeru, kAuxOutTa, kVerbMizenkei},
      {"させて", "させる", kAuxSeru, kAuxOutTe, kVerbMizenkei},
      {"させない", "させる", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"させなかった", "させる", kAuxSeru, kAuxOutTa, kVerbMizenkei},
      {"させます", "させる", kAuxSeru, kAuxOutMasu, kVerbMizenkei},

      // === Causative-passive (させられる系) - for Ichidan ===
      {"させられる", "させられる", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"させられた", "させられる", kAuxSeru, kAuxOutTa, kVerbMizenkei},
      {"させられて", "させられる", kAuxSeru, kAuxOutTe, kVerbMizenkei},
      {"させられない", "させられる", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"させられます", "させられる", kAuxSeru, kAuxOutMasu, kVerbMizenkei},
      // === Causative-passive (せられる系) - for Godan ===
      // 書く → 書か + せられる = 書かせられる
      {"せられる", "せられる", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"せられた", "せられる", kAuxSeru, kAuxOutTa, kVerbMizenkei},
      {"せられて", "せられる", kAuxSeru, kAuxOutTe, kVerbMizenkei},
      {"せられない", "せられる", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"せられます", "せられる", kAuxSeru, kAuxOutMasu, kVerbMizenkei},
      // Short form causative-passive for Godan (歩かされる = 歩か + される)
      {"される", "される", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"された", "される", kAuxSeru, kAuxOutTa, kVerbMizenkei},
      {"されて", "される", kAuxSeru, kAuxOutTe, kVerbMizenkei},
      {"されない", "される", kAuxSeru, kAuxOutBase, kVerbMizenkei},
      {"されます", "される", kAuxSeru, kAuxOutMasu, kVerbMizenkei},

      // === Humble progressive (ておる系) ===
      {"おる", "おる", kAuxTeiru, kAuxOutBase, kAuxOutTe},
      {"おった", "おる", kAuxTeiru, kAuxOutTa, kAuxOutTe},
      {"おります", "おる", kAuxTeiru, kAuxOutMasu, kAuxOutTe},
      {"おりました", "おる", kAuxTeiru, kAuxOutTa, kAuxOutTe},
      {"おりまして", "おる", kAuxTeiru, kAuxOutTe, kAuxOutTe},

      // === Polite receiving (ていただく系) ===
      {"いただく", "いただく", kAuxTemorau, kAuxOutBase, kAuxOutTe},
      {"いただいた", "いただく", kAuxTemorau, kAuxOutTa, kAuxOutTe},
      {"いただいて", "いただく", kAuxTemorau, kAuxOutTe, kAuxOutTe},
      {"いただきます", "いただく", kAuxTemorau, kAuxOutMasu, kAuxOutTe},
      {"いただきました", "いただく", kAuxTemorau, kAuxOutTa, kAuxOutTe},
      {"いただける", "いただく", kAuxTemorau, kAuxOutBase, kAuxOutTe},
      {"いただけます", "いただく", kAuxTemorau, kAuxOutMasu, kAuxOutTe},
      {"いただけますか", "いただく", kAuxTemorau, kAuxOutBase, kAuxOutTe},

      // === Honorific giving (てくださる系) ===
      {"くださる", "くださる", kAuxTekureru, kAuxOutBase, kAuxOutTe},
      {"くださった", "くださる", kAuxTekureru, kAuxOutTa, kAuxOutTe},
      {"くださって", "くださる", kAuxTekureru, kAuxOutTe, kAuxOutTe},
      {"ください", "くださる", kAuxTekureru, kAuxOutBase, kAuxOutTe},
      {"くださいます", "くださる", kAuxTekureru, kAuxOutMasu, kAuxOutTe},
      {"くださいました", "くださる", kAuxTekureru, kAuxOutTa, kAuxOutTe},

      // === Wanting (てほしい系) ===
      {"ほしい", "ほしい", kAuxTai, kAuxOutBase, kAuxOutTe},
      {"ほしかった", "ほしい", kAuxTai, kAuxOutTa, kAuxOutTe},
      {"ほしくない", "ほしい", kAuxTai, kAuxOutBase, kAuxOutTe},

      // === Existence (てある系) ===
      {"ある", "ある", kAuxTeiru, kAuxOutBase, kAuxOutTe},
      {"あった", "ある", kAuxTeiru, kAuxOutTa, kAuxOutTe},
      {"あります", "ある", kAuxTeiru, kAuxOutMasu, kAuxOutTe},

      // === Negative te-form (ないで系) ===
      {"ないで", "ないで", kAuxNai, kAuxOutTe, kVerbMizenkei},
      {"ないでいる", "ないで", kAuxNai, kAuxOutBase, kVerbMizenkei},
      {"ないでいた", "ないで", kAuxNai, kAuxOutTa, kVerbMizenkei},

      // === Polite negative past (ませんでした) ===
      {"ませんでした", "ます", kAuxMasu, kAuxOutTa, kVerbRenyokei},

      // === Potential form endings (可能形: 書ける etc.) ===
      // These attach to potential stem (e-row) and form Ichidan-like verbs
      {"る", "る", kAuxReru, kAuxOutBase, kVerbPotential},      // 書け + る
      {"た", "る", kAuxReru, kAuxOutTa, kVerbPotential},        // 書け + た
      {"て", "る", kAuxReru, kAuxOutTe, kVerbPotential},        // 書け + て
      {"ない", "る", kAuxReru, kAuxOutBase, kVerbPotential},    // 書け + ない
      {"なかった", "る", kAuxReru, kAuxOutTa, kVerbPotential},  // 書け + なかった
      {"ます", "る", kAuxReru, kAuxOutMasu, kVerbPotential},    // 書け + ます
      {"ません", "る", kAuxReru, kAuxOutBase, kVerbPotential},  // 書け + ません
      {"ませんでした", "る", kAuxReru, kAuxOutTa, kVerbPotential},

      // === Renyokei compounds (連用形 + 補助動詞) ===
      // These attach to renyokei (連用形) directly
      {"ながら", "ながら", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"すぎる", "すぎる", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"すぎた", "すぎる", kAuxRenyokei, kAuxOutTa, kVerbRenyokei},
      {"すぎて", "すぎる", kAuxRenyokei, kAuxOutTe, kVerbRenyokei},
      {"すぎている", "すぎる", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"やすい", "やすい", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"やすかった", "やすい", kAuxRenyokei, kAuxOutTa, kVerbRenyokei},
      {"にくい", "にくい", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"にくかった", "にくい", kAuxRenyokei, kAuxOutTa, kVerbRenyokei},
      {"かける", "かける", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"かけた", "かける", kAuxRenyokei, kAuxOutTa, kVerbRenyokei},
      {"かけて", "かける", kAuxRenyokei, kAuxOutTe, kVerbRenyokei},
      {"かけている", "かける", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"出す", "出す", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"出した", "出す", kAuxRenyokei, kAuxOutTa, kVerbRenyokei},
      {"出して", "出す", kAuxRenyokei, kAuxOutTe, kVerbRenyokei},
      {"終わる", "終わる", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"終わった", "終わる", kAuxRenyokei, kAuxOutTa, kVerbRenyokei},
      {"終わって", "終わる", kAuxRenyokei, kAuxOutTe, kVerbRenyokei},
      {"終える", "終える", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"終えた", "終える", kAuxRenyokei, kAuxOutTa, kVerbRenyokei},
      {"終えて", "終える", kAuxRenyokei, kAuxOutTe, kVerbRenyokei},
      {"続ける", "続ける", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"続けた", "続ける", kAuxRenyokei, kAuxOutTa, kVerbRenyokei},
      {"続けて", "続ける", kAuxRenyokei, kAuxOutTe, kVerbRenyokei},
      {"続けている", "続ける", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"直す", "直す", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},
      {"直した", "直す", kAuxRenyokei, kAuxOutTa, kVerbRenyokei},
      {"直して", "直す", kAuxRenyokei, kAuxOutTe, kVerbRenyokei},
      {"直している", "直す", kAuxRenyokei, kAuxOutBase, kVerbRenyokei},

      // === Sou form (そう - appearance/likelihood) ===
      // Attaches to renyokei (連用形)
      {"そう", "そう", kAuxSou, kAuxOutBase, kVerbRenyokei},
      {"そうだ", "そう", kAuxSou, kAuxOutBase, kVerbRenyokei},
      {"そうだった", "そう", kAuxSou, kAuxOutTa, kVerbRenyokei},
      {"そうです", "そう", kAuxSou, kAuxOutBase, kVerbRenyokei},
      {"そうでした", "そう", kAuxSou, kAuxOutTa, kVerbRenyokei},

      // === Tari form (たり) ===
      {"たり", "たり", kAuxTa, kAuxOutBase, kVerbOnbinkei},      // 書い + たり
      {"だり", "たり", kAuxTa, kAuxOutBase, kVerbOnbinkei},      // 読ん + だり
      {"たりする", "たり", kAuxTa, kAuxOutBase, kVerbOnbinkei},  // 書い + たりする
      {"だりする", "たり", kAuxTa, kAuxOutBase, kVerbOnbinkei},  // 読ん + だりする
      {"たりした", "たり", kAuxTa, kAuxOutTa, kVerbOnbinkei},    // 書い + たりした
      {"だりした", "たり", kAuxTa, kAuxOutTa, kVerbOnbinkei},    // 読ん + だりした
      {"たりして", "たり", kAuxTa, kAuxOutTe, kVerbOnbinkei},    // 書い + たりして
      {"だりして", "たり", kAuxTa, kAuxOutTe, kVerbOnbinkei},    // 読ん + だりして

      // === I-adjective endings (い形容詞) ===
      // These attach to I-adjective stem (美し, 高, etc.)
      {"い", "い", kAuxNai, kAuxOutBase, kIAdjStem},        // 美し + い (base)
      {"かった", "い", kAuxNai, kAuxOutTa, kIAdjStem},      // 美し + かった (past)
      {"くない", "い", kAuxNai, kAuxOutBase, kIAdjStem},    // 美し + くない (negative)
      {"くなかった", "い", kAuxNai, kAuxOutTa, kIAdjStem},  // 美し + くなかった (neg past)
      {"くて", "い", kAuxNai, kAuxOutTe, kIAdjStem},        // 美し + くて (te-form)
      {"ければ", "い", kAuxNai, kAuxOutBase, kIAdjStem},    // 美し + ければ (conditional)
      {"く", "い", kAuxNai, kAuxOutBase, kIAdjStem},        // 美し + く (adverb form)
      {"かったら", "い", kAuxNai, kAuxOutBase, kIAdjStem},  // 美し + かったら (cond past)
      {"くなる", "い", kAuxNai, kAuxOutBase, kIAdjStem},    // 美し + くなる
      {"くなった", "い", kAuxNai, kAuxOutTa, kIAdjStem},    // 美し + くなった
      {"くなって", "い", kAuxNai, kAuxOutTe, kIAdjStem},    // 美し + くなって
      {"そう", "い", kAuxNai, kAuxOutBase, kIAdjStem},      // 美し + そう (looks like)
      {"さ", "い", kAuxNai, kAuxOutBase, kIAdjStem},        // 美し + さ (noun form)

      // === Volitional + とする (ようとする: attempting) ===
      // 書こうとする = trying to write
      {"うとする", "とする", kAuxNai, kAuxOutBase, kVerbVolitional},
      {"うとした", "とする", kAuxNai, kAuxOutTa, kVerbVolitional},
      {"うとして", "とする", kAuxNai, kAuxOutTe, kVerbVolitional},
      {"うとしている", "とする", kAuxNai, kAuxOutBase, kVerbVolitional},
      {"うとしていた", "とする", kAuxNai, kAuxOutTa, kVerbVolitional},
      // Ichidan volitional (食べようとする)
      {"ようとする", "とする", kAuxNai, kAuxOutBase, kVerbVolitional},
      {"ようとした", "とする", kAuxNai, kAuxOutTa, kVerbVolitional},
      {"ようとして", "とする", kAuxNai, kAuxOutTe, kVerbVolitional},
      {"ようとしている", "とする", kAuxNai, kAuxOutBase, kVerbVolitional},
      {"ようとしていた", "とする", kAuxNai, kAuxOutTa, kVerbVolitional},

      // === Obligation patterns (ないといけない/なければならない: must do) ===
      // 書かないといけない = must write
      {"ないといけない", "ないといけない", kAuxNai, kAuxOutBase, kVerbMizenkei},
      {"ないといけなかった", "ないといけない", kAuxNai, kAuxOutTa, kVerbMizenkei},
      {"なくてはいけない", "なくてはいけない", kAuxNai, kAuxOutBase, kVerbMizenkei},
      {"なくてはいけなかった", "なくてはいけない", kAuxNai, kAuxOutTa, kVerbMizenkei},
      {"なければならない", "なければならない", kAuxNai, kAuxOutBase, kVerbMizenkei},
      {"なければならなかった", "なければならない", kAuxNai, kAuxOutTa, kVerbMizenkei},
      {"なきゃいけない", "なきゃいけない", kAuxNai, kAuxOutBase, kVerbMizenkei},
      {"なきゃならない", "なきゃならない", kAuxNai, kAuxOutBase, kVerbMizenkei},
      // Colloquial forms
      {"なくちゃ", "なくちゃ", kAuxNai, kAuxOutBase, kVerbMizenkei},
      {"なきゃ", "なきゃ", kAuxNai, kAuxOutBase, kVerbMizenkei},

      // === Ability patterns (ことができる: can do) ===
      // 書くことができる = can write (uses dictionary form)
      {"ことができる", "ことができる", kAuxNai, kAuxOutBase, kVerbBase},
      {"ことができた", "ことができる", kAuxNai, kAuxOutTa, kVerbBase},
      {"ことができない", "ことができる", kAuxNai, kAuxOutBase, kVerbBase},
      {"ことができなかった", "ことができる", kAuxNai, kAuxOutTa, kVerbBase},
      {"ことができて", "ことができる", kAuxNai, kAuxOutTe, kVerbBase},

      // === ようになる (come to be able / start doing) ===
      // 読めるようになる = come to be able to read (potential base + ようになる)
      {"ようになる", "ようになる", kAuxNai, kAuxOutBase, kAuxOutBase},
      {"ようになった", "ようになる", kAuxNai, kAuxOutTa, kAuxOutBase},
      {"ようになって", "ようになる", kAuxNai, kAuxOutTe, kAuxOutBase},
      {"ようになってきた", "ようになる", kAuxNai, kAuxOutTa, kAuxOutBase},
      {"ようになっている", "ようになる", kAuxNai, kAuxOutBase, kAuxOutBase},

      // === Casual explanatory forms (んだ/のだ系) ===
      // 書くんだ = it's that I write (explanatory)
      {"んだ", "のだ", kAuxNai, kAuxOutBase, kVerbBase},
      {"んです", "のだ", kAuxNai, kAuxOutMasu, kVerbBase},
      {"んだもん", "のだ", kAuxNai, kAuxOutBase, kVerbBase},
      {"んだもの", "のだ", kAuxNai, kAuxOutBase, kVerbBase},
      {"のだ", "のだ", kAuxNai, kAuxOutBase, kVerbBase},
      {"のです", "のだ", kAuxNai, kAuxOutMasu, kVerbBase},
      // Past form + んだ (書いたんだ)
      {"んだ", "のだ", kAuxNai, kAuxOutBase, kAuxOutTa},
      {"んだもん", "のだ", kAuxNai, kAuxOutBase, kAuxOutTa},
      {"んだもの", "のだ", kAuxNai, kAuxOutBase, kAuxOutTa},

      // === Prohibition patterns (てはいけない/てはならない) ===
      // 書いてはいけない = must not write
      {"はいけない", "はいけない", kAuxNai, kAuxOutBase, kAuxOutTe},
      {"はいけなかった", "はいけない", kAuxNai, kAuxOutTa, kAuxOutTe},
      {"はならない", "はならない", kAuxNai, kAuxOutBase, kAuxOutTe},
      {"はならなかった", "はならない", kAuxNai, kAuxOutTa, kAuxOutTe},
      {"はだめだ", "はだめだ", kAuxNai, kAuxOutBase, kAuxOutTe},

      // === Permission patterns (てもいい/てもかまわない) ===
      // 書いてもいい = may write
      {"もいい", "もいい", kAuxNai, kAuxOutBase, kAuxOutTe},
      {"もいいですか", "もいい", kAuxNai, kAuxOutBase, kAuxOutTe},
      {"もかまわない", "もかまわない", kAuxNai, kAuxOutBase, kAuxOutTe},
      {"もかまわなかった", "もかまわない", kAuxNai, kAuxOutTa, kAuxOutTe},

      // === べき (should) patterns ===
      // 書くべきだ = should write (requires dictionary form)
      {"べきだ", "べきだ", kAuxNai, kAuxOutBase, kVerbBase},
      {"べきだった", "べきだ", kAuxNai, kAuxOutTa, kVerbBase},
      {"べきではない", "べきだ", kAuxNai, kAuxOutBase, kVerbBase},
      {"べきではなかった", "べきだ", kAuxNai, kAuxOutTa, kVerbBase},
      {"べきです", "べきだ", kAuxNai, kAuxOutMasu, kVerbBase},

      // === ところだ (aspect) patterns ===
      // 書くところだ = about to write (requires dictionary form)
      {"ところだ", "ところだ", kAuxNai, kAuxOutBase, kVerbBase},
      {"ところだった", "ところだ", kAuxNai, kAuxOutTa, kVerbBase},
      {"ところです", "ところだ", kAuxNai, kAuxOutMasu, kVerbBase},
      // 食べたところだ = just ate (requires ta-form)
      {"ところだ", "ところだ", kAuxNai, kAuxOutBase, kAuxOutTa},
      {"ところだった", "ところだ", kAuxNai, kAuxOutTa, kAuxOutTa},
      {"ところです", "ところだ", kAuxNai, kAuxOutMasu, kAuxOutTa},
      // 読んでいるところだ = in the middle of reading (requires base form)
      {"ところだ", "ところだ", kAuxNai, kAuxOutBase, kAuxOutBase},
      {"ところだった", "ところだ", kAuxNai, kAuxOutTa, kAuxOutBase},

      // === ばかり (just did) patterns ===
      // 書いたばかりだ = just wrote (requires ta-form)
      {"ばかりだ", "ばかりだ", kAuxNai, kAuxOutBase, kAuxOutTa},
      {"ばかりだった", "ばかりだ", kAuxNai, kAuxOutTa, kAuxOutTa},
      {"ばかりです", "ばかりだ", kAuxNai, kAuxOutMasu, kAuxOutTa},
      {"ばかりなのに", "ばかりだ", kAuxNai, kAuxOutBase, kAuxOutTa},

      // === っぱなし (leaving in state) patterns ===
      // 開けっぱなし = left open (requires renyokei)
      {"っぱなしだ", "っぱなし", kAuxNai, kAuxOutBase, kVerbRenyokei},
      {"っぱなしにする", "っぱなし", kAuxNai, kAuxOutBase, kVerbRenyokei},
      {"っぱなしで", "っぱなし", kAuxNai, kAuxOutTe, kVerbRenyokei},

      // === ざるを得ない (cannot help but) patterns ===
      // 書かざるを得ない = cannot help but write (requires mizenkei)
      {"ざるを得ない", "ざるを得ない", kAuxNai, kAuxOutBase, kVerbMizenkei},
      {"ざるを得なかった", "ざるを得ない", kAuxNai, kAuxOutTa, kVerbMizenkei},
      {"ざるを得ません", "ざるを得ない", kAuxNai, kAuxOutMasu, kVerbMizenkei},

      // === ずにはいられない (cannot help doing) patterns ===
      // 笑わずにはいられない = cannot help laughing (requires mizenkei)
      {"ずにはいられない", "ずにはいられない", kAuxNai, kAuxOutBase, kVerbMizenkei},
      {"ずにはいられなかった", "ずにはいられない", kAuxNai, kAuxOutTa, kVerbMizenkei},

      // === わけにはいかない (cannot afford to) patterns ===
      // 書かないわけにはいかない = cannot not write (requires base form after ない)
      {"わけにはいかない", "わけにはいかない", kAuxNai, kAuxOutBase, kAuxOutBase},
      {"わけにはいかなかった", "わけにはいかない", kAuxNai, kAuxOutTa, kAuxOutBase},
      {"わけにはいきません", "わけにはいかない", kAuxNai, kAuxOutMasu, kAuxOutBase},
      // 食べるわけにはいかない = cannot afford to eat (requires dictionary form)
      {"わけにはいかない", "わけにはいかない", kAuxNai, kAuxOutBase, kVerbBase},
      {"わけにはいかなかった", "わけにはいかない", kAuxNai, kAuxOutTa, kVerbBase},

      // === Potential stem + なくなる (stop being able to) ===
      // 話せなくなる = stop being able to speak (requires potential stem)
      {"なくなる", "なくなる", kAuxNai, kAuxOutBase, kVerbPotential},
      {"なくなった", "なくなる", kAuxNai, kAuxOutTa, kVerbPotential},
      {"なくなって", "なくなる", kAuxNai, kAuxOutTe, kVerbPotential},
      {"なくなってしまう", "なくなる", kAuxNai, kAuxOutBase, kVerbPotential},
      {"なくなってしまった", "なくなる", kAuxNai, kAuxOutTa, kVerbPotential},
  };

  // Sort by surface length (longest first) for greedy matching
  std::sort(auxiliaries_.begin(), auxiliaries_.end(),
            [](const AuxiliaryEntry& lhs, const AuxiliaryEntry& rhs) {
              return lhs.surface.size() > rhs.surface.size();
            });
}

std::vector<std::pair<const AuxiliaryEntry*, size_t>>
Inflection::matchAuxiliaries(std::string_view surface) const {
  std::vector<std::pair<const AuxiliaryEntry*, size_t>> matches;

  for (const auto& aux : auxiliaries_) {
    if (surface.size() >= aux.surface.size()) {
      size_t start = surface.size() - aux.surface.size();
      if (surface.substr(start) == aux.surface) {
        matches.emplace_back(&aux, aux.surface.size());
      }
    }
  }

  return matches;
}

std::vector<InflectionCandidate> Inflection::matchVerbStem(
    std::string_view remaining,
    const std::vector<std::string>& aux_chain,
    uint16_t required_conn) const {
  std::vector<InflectionCandidate> candidates;
  const auto& endings = getVerbEndings();

  for (const auto& ending : endings) {
    // Check if connection is valid
    if (ending.provides_conn != required_conn) {
      continue;
    }

    // Check if remaining ends with this verb ending
    if (remaining.size() < ending.suffix.size()) {
      continue;
    }

    bool matches = true;
    if (!ending.suffix.empty()) {
      size_t start = remaining.size() - ending.suffix.size();
      matches = (remaining.substr(start) == ending.suffix);
    }

    if (matches) {
      // Extract stem
      std::string stem(remaining);
      if (!ending.suffix.empty()) {
        stem = std::string(remaining.substr(0, remaining.size() - ending.suffix.size()));
      }

      // Stem should be at least 3 bytes (one Japanese character)
      // Exception: Suru verb with す→する (empty stem is allowed)
      if (stem.size() < 3 &&
          !(ending.verb_type == VerbType::Suru && ending.suffix == "す")) {
        continue;
      }

      // Skip Ichidan with empty suffix when no auxiliaries matched
      // (prevents "書いて" from being parsed as Ichidan "書いてる")
      if (ending.suffix.empty() && aux_chain.empty() &&
          ending.verb_type == VerbType::Ichidan) {
        continue;
      }

      // Validate irregular いく pattern: GodanKa with っ-onbin only valid for い/行
      if (ending.verb_type == VerbType::GodanKa && ending.suffix == "っ" &&
          ending.is_onbin) {
        if (stem != "い" && stem != "行") {
          continue;  // Skip invalid irregular pattern
        }
      }

      // Build base form
      std::string base_form = stem + ending.base_suffix;

      // Build suffix chain string
      std::string suffix_str = ending.suffix;
      for (auto iter = aux_chain.rbegin(); iter != aux_chain.rend(); ++iter) {
        suffix_str += *iter;
      }

      // Calculate total auxiliary length
      size_t aux_total_len = 0;
      for (const auto& aux : aux_chain) {
        aux_total_len += aux.size();
      }

      InflectionCandidate candidate;
      candidate.base_form = base_form;
      candidate.stem = stem;
      candidate.suffix = suffix_str;
      candidate.verb_type = ending.verb_type;
      candidate.confidence = calculateConfidence(
          ending.verb_type, stem, aux_total_len, aux_chain.size(), required_conn);
      candidate.morphemes = aux_chain;

      candidates.push_back(candidate);
    }
  }

  return candidates;
}

std::vector<InflectionCandidate> Inflection::analyzeWithAuxiliaries(
    std::string_view surface,
    const std::vector<std::string>& aux_chain,
    uint16_t required_conn) const {
  std::vector<InflectionCandidate> candidates;

  // Try to find more auxiliaries
  auto matches = matchAuxiliaries(surface);

  for (const auto& [aux, len] : matches) {
    // Check if this auxiliary can connect to what we need
    if (aux->right_id != required_conn) {
      continue;
    }

    // Recursively analyze remaining
    std::string_view remaining = surface.substr(0, surface.size() - len);
    std::vector<std::string> new_chain = aux_chain;
    new_chain.push_back(aux->surface);

    auto sub_candidates = analyzeWithAuxiliaries(
        remaining, new_chain, aux->required_conn);
    for (auto& cand : sub_candidates) {
      candidates.push_back(std::move(cand));
    }
  }

  // Also try to match verb stem directly
  auto stem_candidates = matchVerbStem(surface, aux_chain, required_conn);
  for (auto& cand : stem_candidates) {
    candidates.push_back(std::move(cand));
  }

  return candidates;
}

std::vector<InflectionCandidate> Inflection::analyze(
    std::string_view surface) const {
  std::vector<InflectionCandidate> candidates;

  // First, try to match auxiliaries from the end
  auto matches = matchAuxiliaries(surface);

  for (const auto& [aux, len] : matches) {
    std::string_view remaining = surface.substr(0, surface.size() - len);
    std::vector<std::string> aux_chain{aux->surface};

    auto sub_candidates = analyzeWithAuxiliaries(
        remaining, aux_chain, aux->required_conn);
    for (auto& cand : sub_candidates) {
      candidates.push_back(std::move(cand));
    }
  }

  // Also try direct verb stem matching (for base forms only)
  // Only match base form when no auxiliaries are found
  auto base_candidates = matchVerbStem(surface, {}, conn::kVerbBase);
  for (auto& cand : base_candidates) {
    candidates.push_back(std::move(cand));
  }

  // Sort by confidence (descending)
  std::sort(candidates.begin(), candidates.end(),
            [](const InflectionCandidate& lhs, const InflectionCandidate& rhs) {
              return lhs.confidence > rhs.confidence;
            });

  // Remove duplicates (same base_form and verb_type)
  auto dup_end = std::unique(
      candidates.begin(), candidates.end(),
      [](const InflectionCandidate& lhs, const InflectionCandidate& rhs) {
        return lhs.base_form == rhs.base_form && lhs.verb_type == rhs.verb_type;
      });
  candidates.erase(dup_end, candidates.end());

  return candidates;
}

bool Inflection::looksConjugated(std::string_view surface) const {
  return !analyze(surface).empty();
}

InflectionCandidate Inflection::getBest(std::string_view surface) const {
  auto candidates = analyze(surface);
  if (candidates.empty()) {
    return {};
  }
  return candidates.front();
}

}  // namespace suzume::grammar
