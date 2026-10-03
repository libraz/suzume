/**
 * @file conjugator.cpp
 * @brief Dynamic conjugation stem generator implementation
 */

#include "conjugator.h"

#include "verb_endings.h"

namespace suzume::grammar {

Conjugator::Conjugator() = default;

std::string Conjugator::getStem(const std::string& base_form, VerbType type) const {
  return conjugation_.getStem(base_form, type);
}

VerbType Conjugator::detectType(const std::string& base_form) const {
  return conjugation_.detectType(base_form);
}

std::vector<StemForm> Conjugator::generateStems(const std::string& base_form, VerbType type) const {
  std::string stem = getStem(base_form, type);

  if (isGodanVerbType(type)) {
    return generateGodanStems(stem, base_form, type);
  }

  switch (type) {
    case VerbType::Ichidan:
      return generateIchidanStems(stem, base_form);

    case VerbType::IAdjective:
      return generateIAdjectiveStems(stem, base_form);

    case VerbType::Suru:
      return generateSuruStems(stem, base_form);

    case VerbType::Kuru:
      return generateKuruStems(base_form);

    default:
      return {};
  }
}

std::vector<StemForm> Conjugator::generateGodanStems(const std::string& stem, const std::string& base_form,
                                                     VerbType type) const {
  std::vector<StemForm> forms;
  const auto* row = Conjugation::getGodanRow(type);
  if (row == nullptr) {
    return forms;
  }

  const auto [base_suffix, a_suffix, i_suffix, e_suffix, o_suffix] = encodeGodanVowels(*row);

  // 終止形 (Base)
  forms.push_back({base_form, type, base_suffix, conn::kVerbBase});

  // 未然形 (Mizenkei): 書か
  forms.push_back({stem + a_suffix, type, base_suffix, conn::kVerbMizenkei});

  // 連用形 (Renyokei): 書き
  forms.push_back({stem + i_suffix, type, base_suffix, conn::kVerbRenyokei});

  // 音便形 (Onbinkei): 書い, 読ん, 持っ — or, for サ行, 連用形 doubles as onbinkei (話し + た).
  forms.push_back({stem + godanOnbinForm(type, stem), type, base_suffix, conn::kVerbOnbinkei});

  // The e-row supplies potential, conditional, and imperative readings. Keep
  // all grammatical cells even though their surfaces coincide.
  if (type != VerbType::GodanRa) {
    forms.push_back({stem + e_suffix, type, base_suffix, conn::kVerbPotential});
  }
  forms.push_back({stem + e_suffix, type, base_suffix, conn::kVerbKatei});
  forms.push_back({stem + e_suffix, type, base_suffix, conn::kVerbMeireikei});

  // 意志形 stem before う: 書こ+う.
  forms.push_back({stem + o_suffix, type, base_suffix, conn::kVerbVolitional});

  return forms;
}

namespace {

// A verb class whose whole paradigm is spelled out in the ending table needs no
// row arithmetic: every form is the stem plus the ending registered for that
// class, taken in table order.
std::vector<StemForm> generateTabulatedStems(const std::string& stem, VerbType type) {
  std::vector<StemForm> forms;
  for (ConjForm form : kAllVerbConjForms) {
    const uint16_t conn_id = kVerbConjFormConnections[static_cast<size_t>(form)];
    for (const auto& ending : getVerbEndingsByForm(form)) {
      if (ending.verb_type == type) {
        forms.push_back({stem + ending.suffix, type, ending.base_suffix, conn_id});
      }
    }
  }
  return forms;
}

}  // namespace

std::vector<StemForm> Conjugator::generateIchidanStems(const std::string& stem, const std::string& base_form) const {
  static_cast<void>(base_form);
  return generateTabulatedStems(stem, VerbType::Ichidan);
}

std::vector<StemForm> Conjugator::generateIAdjectiveStems(const std::string& stem, const std::string& base_form) const {
  constexpr auto type = VerbType::IAdjective;
  return {
      {base_form, type, "い", conn::kVerbBase},       {stem, type, "い", conn::kIAdjStem},
      {stem + "く", type, "い", conn::kVerbRenyokei}, {stem + "かっ", type, "い", conn::kVerbOnbinkei},
      {stem + "けれ", type, "い", conn::kVerbKatei},  {stem + "かろ", type, "い", conn::kVerbVolitional},
  };
}

std::vector<StemForm> Conjugator::generateSuruStems(const std::string& stem, const std::string& base_form) const {
  static_cast<void>(base_form);
  return generateTabulatedStems(stem, VerbType::Suru);
}

std::vector<StemForm> Conjugator::generateKuruStems(const std::string& base_form) const {
  std::vector<StemForm> forms;
  VerbType type = VerbType::Kuru;
  const KuruStemForms kuru = getKuruStemForms(base_form);

  forms.push_back({kuru.base, type, base_form, conn::kVerbBase});
  forms.push_back({kuru.renyokei, type, base_form, conn::kVerbRenyokei});
  forms.push_back({kuru.onbinkei, type, base_form, conn::kVerbOnbinkei});
  forms.push_back({kuru.mizenkei, type, base_form, conn::kVerbMizenkei});
  forms.push_back({kuru.kateikei, type, base_form, conn::kVerbKatei});
  forms.push_back({kuru.ishikei, type, base_form, conn::kVerbVolitional});
  forms.push_back({kuru.meireikei, type, base_form, conn::kVerbMeireikei});

  return forms;
}

}  // namespace suzume::grammar
