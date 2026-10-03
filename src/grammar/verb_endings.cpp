/**
 * @file verb_endings.cpp
 * @brief Verb ending patterns for reverse inflection analysis
 *
 * Godan patterns are generated from Conjugation::getGodanRows() for consistency.
 * Irregular verb patterns (Ichidan, Suru, Kuru, IAdjective) are manually defined.
 */

#include "verb_endings.h"

#include <array>
#include <iterator>
#include <utility>
#include <vector>

namespace suzume::grammar {

namespace {

// Fixed Godan-type iteration order for deterministic ending generation.
//
// Keep the reverse-lookup preference explicit: it differs from the canonical
// table's verb-type order for shared onbin forms and is part of the cross-binding
// output contract.
//
// The order is chosen to agree with Conjugation::getGodanTypesByOnbin()'s declared
// onbin preference, so that when no dictionary entry breaks the tie the same lemma
// wins everywhere:
//   - い-onbin: Ka before Ga
//   - っ-onbin: Ka before Ra before Ta before Wa
//   - ん-onbin: Ma before Ba before Na
constexpr VerbType kGodanTypeOrder[] = {
    VerbType::GodanKa, VerbType::GodanGa, VerbType::GodanSa, VerbType::GodanMa, VerbType::GodanBa,
    VerbType::GodanNa, VerbType::GodanRa, VerbType::GodanTa, VerbType::GodanWa,
};

struct TaggedVerbEnding {
  VerbEnding ending;
  uint16_t provides_conn;
};

// Generate all Godan verb endings from Conjugation::getGodanRow()
std::vector<TaggedVerbEnding> generateGodanEndings() {
  std::vector<TaggedVerbEnding> endings;
  endings.reserve(80);  // Approx 9 types * 9 forms

  for (VerbType type : kGodanTypeOrder) {
    const auto* row_ptr = Conjugation::getGodanRow(type);
    if (row_ptr == nullptr) {
      continue;
    }
    const auto& row = *row_ptr;
    const auto [base, a_row, i_row, e_row, o_row] = encodeGodanVowels(row);

    // Onbinkei (音便形): explicit onbin (い/っ/ん) or, for サ行, the い段 form.
    endings.push_back({{onbinFormOf(row), base, type, true}, conn::kVerbOnbinkei});

    // Special case: GodanKa also has っ-onbin for いく (irregular).
    // Derive the surface through the shared lexical-irregularity helper.
    if (type == VerbType::GodanKa) {
      endings.push_back({{godanOnbinForm(type, "い"), base, type, true}, conn::kVerbOnbinkei});
    }
    // A closed GodanWa subclass uses う音便 (問うた) instead of the row's
    // regular 促音便.  Include the surface in reverse lookup; dictionary
    // verification at the consumer selects only attested base forms.
    if (type == VerbType::GodanWa) {
      endings.push_back({{"う", base, type, true}, conn::kVerbOnbinkei});
    }

    // Renyokei (連用形)
    endings.push_back({{i_row, base, type, false}, conn::kVerbRenyokei});

    // Mizenkei (未然形)
    endings.push_back({{a_row, base, type, false}, conn::kVerbMizenkei});

    // Potential (可能形) - skip for GodanRa (conflicts with Ichidan stems)
    if (type != VerbType::GodanRa) {
      endings.push_back({{e_row, base, type, false}, conn::kVerbPotential});
    }

    // Kateikei (仮定形)
    endings.push_back({{e_row, base, type, false}, conn::kVerbKatei});

    // Meireikei (命令形)
    endings.push_back({{e_row, base, type, false}, conn::kVerbMeireikei});

    // Volitional (意志形)
    endings.push_back({{o_row, base, type, false}, conn::kVerbVolitional});

    // Base/dictionary form (終止形)
    endings.push_back({{base, base, type, false}, conn::kVerbBase});
  }

  return endings;
}

// Manually defined irregular verb patterns
struct VerbEndingSpec {
  const char* suffix;
  const char* base_suffix;
  VerbType verb_type;
  uint16_t provides_conn;
  bool is_onbin;
};

constexpr VerbEndingSpec kIrregularEndings[] = {
    // 一段 (食べる)
    {"", "る", VerbType::Ichidan, conn::kVerbOnbinkei, true},
    {"", "る", VerbType::Ichidan, conn::kVerbRenyokei, false},
    {"", "る", VerbType::Ichidan, conn::kVerbMizenkei, false},
    {"れ", "る", VerbType::Ichidan, conn::kVerbKatei, false},       // Hypothetical: 食べれ(ば)
    {"ろ", "る", VerbType::Ichidan, conn::kVerbMeireikei, false},   // Imperative: 食べろ
    {"よ", "る", VerbType::Ichidan, conn::kVerbVolitional, false},  // Volitional stem
    {"る", "る", VerbType::Ichidan, conn::kVerbBase, false},        // Base/dictionary form

    // サ変 (する)
    {"し", "する", VerbType::Suru, conn::kVerbOnbinkei, true},
    {"し", "する", VerbType::Suru, conn::kVerbRenyokei, false},
    {"し", "する", VerbType::Suru, conn::kVerbMizenkei, false},  // しない
    {"さ", "する", VerbType::Suru, conn::kVerbMizenkei, false},  // させる/される
    {"せ", "する", VerbType::Suru, conn::kVerbMizenkei, false},  // せず/せぬ
    // Empty suffix for suru-verb + passive/causative (開催+された → 開催する)
    {"", "する", VerbType::Suru, conn::kVerbMizenkei, false},
    // Empty suffix for suru-verb + してる/してた contraction
    {"", "する", VerbType::Suru, conn::kVerbOnbinkei, true},
    {"すれ", "する", VerbType::Suru, conn::kVerbKatei, false},       // すれば
    {"しろ", "する", VerbType::Suru, conn::kVerbMeireikei, false},   // Imperative: しろ
    {"せよ", "する", VerbType::Suru, conn::kVerbMeireikei, false},   // Imperative (classical): せよ
    {"しよ", "する", VerbType::Suru, conn::kVerbVolitional, false},  // しよう
    {"する", "する", VerbType::Suru, conn::kVerbBase, false},        // Base/dictionary form
    {"す", "する", VerbType::Suru, conn::kVerbBase, false},          // すべき special

    // い形容詞 (美しい)
    {"", "い", VerbType::IAdjective, conn::kIAdjStem, false},
};

constexpr size_t kEndingGroupCount = conn::kVerbMeireikei - conn::kVerbBase + 1;

struct VerbEndingGroup {
  size_t offset;
  size_t size;
};

struct VerbEndingTable {
  std::vector<VerbEnding> endings;
  std::array<VerbEndingGroup, kEndingGroupCount> groups;
};

VerbEndingTable buildVerbEndingTable() {
  std::vector<TaggedVerbEnding> tagged = generateGodanEndings();
  tagged.reserve(tagged.size() + std::size(kIrregularEndings) + 7);
  for (const auto& spec : kIrregularEndings) {
    tagged.push_back({{spec.suffix, spec.base_suffix, spec.verb_type, spec.is_onbin}, spec.provides_conn});
  }

  // カ変 is derived from the same kana stem record used by generation and
  // dictionary expansion. Keeping it out of kIrregularEndings avoids a fourth
  // independent list of こ/き/くれ/こよ/こい spellings.
  const KuruStemForms kuru = getKuruStemForms("くる");
  tagged.push_back({{kuru.onbinkei, kuru.base, VerbType::Kuru, true}, conn::kVerbOnbinkei});
  tagged.push_back({{kuru.renyokei, kuru.base, VerbType::Kuru, false}, conn::kVerbRenyokei});
  tagged.push_back({{kuru.mizenkei, kuru.base, VerbType::Kuru, false}, conn::kVerbMizenkei});
  tagged.push_back({{kuru.kateikei, kuru.base, VerbType::Kuru, false}, conn::kVerbKatei});
  tagged.push_back({{kuru.meireikei, kuru.base, VerbType::Kuru, false}, conn::kVerbMeireikei});
  tagged.push_back({{kuru.ishikei, kuru.base, VerbType::Kuru, false}, conn::kVerbVolitional});
  tagged.push_back({{kuru.base, kuru.base, VerbType::Kuru, false}, conn::kVerbBase});

  VerbEndingTable table;
  table.endings.reserve(tagged.size());
  // There are only nine connection groups. Scanning the small initialization
  // table once per group is cheaper in code size than instantiating stable_sort,
  // and preserves the original order within every group.
  for (size_t group_index = 0; group_index < kEndingGroupCount; ++group_index) {
    const uint16_t provides_conn = static_cast<uint16_t>(conn::kVerbBase + group_index);
    const size_t offset = table.endings.size();
    for (auto& item : tagged) {
      if (item.provides_conn == provides_conn) {
        table.endings.push_back(std::move(item.ending));
      }
    }
    table.groups[group_index] = {offset, table.endings.size() - offset};
  }
  return table;
}

const VerbEndingTable& verbEndingTable() {
  static const VerbEndingTable kTable = buildVerbEndingTable();
  return kTable;
}

}  // namespace

VerbEndingRange getVerbEndingsByConn(uint16_t provides_conn) {
  const auto& table = verbEndingTable();
  if (provides_conn < conn::kVerbBase || provides_conn > conn::kVerbMeireikei) {
    return {table.endings.data(), 0};
  }
  const auto& group = table.groups[provides_conn - conn::kVerbBase];
  return {table.endings.data() + group.offset, group.size};
}

VerbEndingRange getVerbEndingsByForm(ConjForm form) {
  const size_t index = static_cast<size_t>(form);
  if (index >= kVerbConjFormConnections.size()) {
    const auto& table = verbEndingTable();
    return {table.endings.data(), 0};
  }
  return getVerbEndingsByConn(kVerbConjFormConnections[index]);
}

}  // namespace suzume::grammar
