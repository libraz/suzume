/**
 * @file conjugator.h
 * @brief Dynamic conjugation stem generator with connection IDs
 *
 * Design: Given a verb base form and type, generates all conjugated
 * stem forms with their connection IDs. Replaces static pattern
 * enumeration in patterns directory.
 */

#ifndef SUZUME_GRAMMAR_CONJUGATOR_H_
#define SUZUME_GRAMMAR_CONJUGATOR_H_

#include <string>
#include <vector>

#include "conjugation.h"
#include "connection.h"

namespace suzume::grammar {

/**
 * @brief Generated stem form with connection metadata
 */
struct StemForm {
  std::string surface;      // Conjugated stem: 書い, 読ん, 食べ
  VerbType verb_type;       // Original verb type
  std::string base_suffix;  // To restore base form: く, む, る
  uint16_t right_id;        // Connection ID for what can attach
};

/**
 * @brief Conjugator - generates stem forms for analysis
 *
 * Usage:
 *   Conjugator conj;
 *   auto stems = conj.generateStems("書く", VerbType::GodanKa);
 *   // → [{surface: "書い", right_id: kVerbOnbinkei, ...}, ...]
 */
class Conjugator {
 public:
  Conjugator();

  /**
   * @brief Generate all stem forms for a verb
   * @param base_form Base form: 書く
   * @param type Verb type: GodanKa
   * @return Vector of stem forms with connection IDs
   */
  std::vector<StemForm> generateStems(const std::string& base_form,
                                      VerbType type) const;

  /**
   * @brief Get the stem (remove ending) from base form
   * @param base_form Base form: 書く
   * @param type Verb type
   * @return Stem: 書
   */
  std::string getStem(const std::string& base_form, VerbType type) const;

  /**
   * @brief Detect verb type from base form (heuristic)
   */
  VerbType detectType(const std::string& base_form) const;

 private:
  Conjugation conjugation_;

  std::vector<StemForm> generateGodanStems(const std::string& stem,
                                           const std::string& base_form,
                                           VerbType type) const;
  std::vector<StemForm> generateIchidanStems(const std::string& stem,
                                             const std::string& base_form) const;
  std::vector<StemForm> generateSuruStems(const std::string& stem,
                                          const std::string& base_form) const;
  std::vector<StemForm> generateKuruStems(const std::string& stem,
                                          const std::string& base_form) const;
};

}  // namespace suzume::grammar

#endif  // SUZUME_GRAMMAR_CONJUGATOR_H_
