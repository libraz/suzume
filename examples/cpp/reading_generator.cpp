/**
 * Reading (furigana) generator - extract readings for kanji words
 *
 * Demonstrates: morpheme-level analysis, reading field,
 * conjugation info, and lemma access.
 */
#include <iostream>
#include <string_view>

#include "core/types.h"
#include "grammar/conjugation.h"
#include "suzume.h"

int main() {
  suzume::Suzume analyzer;

  std::string_view text = "彼女は図書館で難しい本を読んでいた";

  auto morphemes = analyzer.analyze(text);

  // Display with readings
  std::cout << "Surface\tReading\tPOS\tLemma\tConj\n";
  std::cout << "------\t-------\t---\t-----\t----\n";

  for (const auto& m : morphemes) {
    std::cout << m.surface << "\t" << m.reading << "\t"
              << suzume::core::posToString(m.pos) << "\t"
              << m.getLemma() << "\t";

    if (m.conj_form != suzume::grammar::ConjForm::Base) {
      std::cout << suzume::grammar::conjFormToString(m.conj_form);
    } else {
      std::cout << "-";
    }
    std::cout << "\n";
  }

  // Generate furigana HTML
  std::cout << "\nFurigana HTML:\n";
  for (const auto& m : morphemes) {
    if (m.reading.empty() || m.surface == m.reading) {
      std::cout << m.surface;
    } else {
      std::cout << "<ruby>" << m.surface << "<rp>(</rp><rt>" << m.reading
                << "</rt><rp>)</rp></ruby>";
    }
  }
  std::cout << "\n";

  return 0;
}
