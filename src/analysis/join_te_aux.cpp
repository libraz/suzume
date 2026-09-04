/**
 * @file join_te_aux.cpp
 * @brief Taru-adjective join candidate generation
 */
#include "analysis/dictionary_probe.h"
#include "join_candidates.h"
#include "tokenizer_utils.h"

namespace suzume::analysis {

namespace {

using CharType = normalize::CharType;

}  // namespace

void addTaruAdjectiveJoinCandidates(core::Lattice& lattice, std::string_view text,
                                    const std::vector<char32_t>& codepoints, const ByteOffsets& byte_offsets,
                                    size_t start_pos, const std::vector<normalize::CharType>& char_types,
                                    const dictionary::DictionaryManager& dict_manager, const Scorer& scorer) {
  if (start_pos >= codepoints.size()) {
    return;
  }

  // Must start with kanji
  if (char_types[start_pos] != CharType::Kanji) {
    return;
  }

  // Look for the taru-adverb shape: a kanji nominal marked as one, plus と.
  // Need at least 3 characters: X + marker + と
  if (start_pos + 2 >= codepoints.size()) {
    return;
  }

  // Find the kanji portion (including 然)
  size_t kanji_end = start_pos + 1;
  while (kanji_end < codepoints.size() && char_types[kanji_end] == CharType::Kanji) {
    ++kanji_end;
  }

  // Need at least 2 kanji, and the last one must mark the nominal as taru-style
  if (kanji_end - start_pos < 2) {
    return;
  }

  // Two markers derive the same adverbial: the taru-adjective suffix 然
  // (整然と, 毅然と) and the iteration mark, whose reduplication is the other
  // productive source of this class (淡々と, 黙々と, 延々と). Both name a manner,
  // and neither reading leaves と as a case particle governed by the nominal.
  char32_t last_kanji = codepoints[kanji_end - 1];
  if (last_kanji != U'然' && last_kanji != U'々') {
    return;
  }

  // Next character must be と (hiragana)
  if (kanji_end >= codepoints.size() || codepoints[kanji_end] != U'と') {
    return;
  }

  // In a genitive-selected nominal frame (Xの N とは), と is the quotative
  // particle and は is the topic particle; joining Nと as a taru-style adverb
  // would erase both grammatical boundaries.  This structural frame is
  // distinct from an ordinary adverbial use such as 毅然と進む.
  if (start_pos > 0 && kanji_end + 1 < codepoints.size() && codepoints[kanji_end + 1] == U'は') {
    const auto* left_no =
        lookupEntryInRange(dict_manager, codepoints, start_pos - 1, start_pos, core::PartOfSpeech::Particle);
    const auto* right_wa =
        lookupEntryInRange(dict_manager, codepoints, kanji_end + 1, kanji_end + 2, core::PartOfSpeech::Particle);
    if (left_no != nullptr && left_no->extended_pos == core::ExtendedPOS::ParticleNo && right_wa != nullptr &&
        right_wa->extended_pos == core::ExtendedPOS::ParticleTopic) {
      return;
    }
  }

  // Build the surface: X然と
  size_t end_pos = kanji_end + 1;  // Include と
  std::string surface(textRange(text, byte_offsets, start_pos, end_pos));

  // The nominal without と is the lemma. A listed nominal has a reading of its
  // own, and with the iteration mark that reading is usually a plural whose と
  // is the comitative case particle (人々と話す, 我々と行く) — the opposite
  // analysis. Any part of speech counts here, since the plural pronouns are not
  // nouns.
  std::string lemma(textRange(text, byte_offsets, start_pos, kanji_end));
  if (!dict_manager.lookup(lemma, 0).empty()) {
    return;
  }

  // Calculate cost with bonus for this pattern
  float base_cost = scorer.posPrior(core::PartOfSpeech::Adverb);
  constexpr float kTaruAdverbBonus = -1.5F;  // Strong bonus to beat Noun + Particle
  float final_cost = base_cost + kTaruAdverbBonus;

  uint8_t flags = core::LatticeEdge::kFromDictionary;

  lattice.addEdge(surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(end_pos), core::PartOfSpeech::Adverb,
                  final_cost, flags, lemma);
}

}  // namespace suzume::analysis
