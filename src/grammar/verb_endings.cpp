/**
 * @file verb_endings.cpp
 * @brief Verb ending patterns for reverse inflection analysis
 *
 * Godan patterns are generated from Conjugation::getGodanRows() for consistency.
 * Irregular verb patterns (Ichidan, Suru, Kuru, IAdjective) are manually defined.
 */

#include "verb_endings.h"

#include <array>
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

// Append the Godan endings providing `provides_conn`, generated from Conjugation::getGodanRow()
void appendGodanEndings(uint16_t provides_conn, std::vector<VerbEnding>& endings) {
  const auto emit = [&](uint16_t conn, VerbEnding ending) {
    if (conn == provides_conn) {
      endings.push_back(std::move(ending));
    }
  };

  for (VerbType type : kGodanTypeOrder) {
    const auto* row_ptr = Conjugation::getGodanRow(type);
    if (row_ptr == nullptr) {
      continue;
    }
    const auto& row = *row_ptr;
    const auto [base, a_row, i_row, e_row, o_row] = encodeGodanVowels(row);

    // Onbinkei (音便形): explicit onbin (い/っ/ん) or, for サ行, the い段 form.
    emit(conn::kVerbOnbinkei, {onbinFormOf(row), base, type, true});

    // Special case: GodanKa also has っ-onbin for いく (irregular).
    // Derive the surface through the shared lexical-irregularity helper.
    if (type == VerbType::GodanKa) {
      emit(conn::kVerbOnbinkei, {godanOnbinForm(type, "い"), base, type, true});
    }
    // A closed GodanWa subclass uses う音便 (問うた) instead of the row's
    // regular 促音便.  Include the surface in reverse lookup; dictionary
    // verification at the consumer selects only attested base forms.
    if (type == VerbType::GodanWa) {
      emit(conn::kVerbOnbinkei, {"う", base, type, true});
    }

    // Renyokei (連用形)
    emit(conn::kVerbRenyokei, {i_row, base, type, false});

    // Mizenkei (未然形)
    emit(conn::kVerbMizenkei, {a_row, base, type, false});

    // Potential (可能形) - skip for GodanRa (conflicts with Ichidan stems)
    if (type != VerbType::GodanRa) {
      emit(conn::kVerbPotential, {e_row, base, type, false});
    }

    // Kateikei (仮定形)
    emit(conn::kVerbKatei, {e_row, base, type, false});

    // Meireikei (命令形)
    emit(conn::kVerbMeireikei, {e_row, base, type, false});

    // Volitional (意志形)
    emit(conn::kVerbVolitional, {o_row, base, type, false});

    // Base/dictionary form (終止形)
    emit(conn::kVerbBase, {base, base, type, false});
  }
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
  // カ変 is derived from the same kana stem record used by generation and
  // dictionary expansion. Keeping it out of kIrregularEndings avoids a fourth
  // independent list of こ/き/くれ/こよ/こい spellings.
  const KuruStemForms kuru = getKuruStemForms("くる");

  VerbEndingTable table;
  // There are only nine connection groups. Generating the small initialization
  // set once per group is cheaper in code size than tagging and sorting it,
  // and preserves the generation order within every group.
  for (size_t group_index = 0; group_index < kEndingGroupCount; ++group_index) {
    const uint16_t provides_conn = static_cast<uint16_t>(conn::kVerbBase + group_index);
    const size_t offset = table.endings.size();
    appendGodanEndings(provides_conn, table.endings);
    for (const auto& spec : kIrregularEndings) {
      if (spec.provides_conn == provides_conn) {
        table.endings.push_back({spec.suffix, spec.base_suffix, spec.verb_type, spec.is_onbin});
      }
    }
    for (const auto& cell : kKuruConnectionCells) {
      if (cell.provides_conn == provides_conn) {
        table.endings.push_back(
            {kuru.*cell.form, kuru.base, VerbType::Kuru, cell.provides_conn == conn::kVerbOnbinkei});
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
