/**
 * @file tokenizer_dictionary_variants.cpp
 * @brief Dictionary edges for elided, truncated and clipped spellings of listed words
 */

#include <algorithm>
#include <array>

#include "analysis/category_cost.h"
#include "analysis/dictionary_probe.h"
#include "analysis/tokenizer_utils.h"
#include "candidate_constants.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "tokenizer_dictionary_internal.h"

namespace suzume::analysis::tokenizer_dictionary_detail {

namespace {

// The longest closed-class entry a drawn-out spelling can hide, in morae. The
// scan below runs at every position, so the window is bounded rather than open.
constexpr size_t kElidedLookupWindow = 6;

// A predicate host in front hosts a final particle; one reaching across a held
// particle's span is the word the mark draws out (やばーい), and owns the mark instead.
constexpr PartOfSpeechMask kHeldFinalParticleHostMask = kPredicateHostMask | kNounPronounMask;

}  // namespace

// Colloquial emphasis may hold a mora in the middle of a function word rather
// than at its end (飲みたーい, ませーん, でーす). The mark carries no segment of
// its own, so the word is still there — but a lookup over the literal text
// stops at the mark and leaves whatever homograph fits the fragment (たー as the
// past auxiliary, and い as a verb behind it). Look the entry up again with the
// interior marks elided, and give the match the span it actually occupies.
//
// Only auxiliaries are admitted. An auxiliary is selected by the predicate it
// attaches to, so its left edge is fixed by that predicate's inflection and the
// match cannot float. A particle attaches to anything, so an elided match would
// be free to open anywhere inside a kana run and would cut into longer words
// through the mark (the dialectal ばい inside やばーい).
void addElidedProlongedDictionaryCandidates(core::Lattice& lattice, const dictionary::DictionaryManager& dict_manager,
                                            const std::vector<char32_t>& codepoints, size_t start_pos) {
  const size_t window_end = std::min(codepoints.size(), start_pos + kElidedLookupWindow);
  // The mark holds the mora in front of it, so it can neither open the window
  // nor be the only thing in it. Scanning for one before building anything keeps
  // the ordinary position — which has no mark — free of allocation.
  size_t first_mark = window_end;
  for (size_t pos = start_pos + 1; pos < window_end; ++pos) {
    if (normalize::isProlongedSoundMark(codepoints[pos])) {
      first_mark = pos;
      break;
    }
    if (normalize::classifyChar(codepoints[pos]) != normalize::CharType::Hiragana) {
      return;
    }
  }
  if (first_mark + 1 >= window_end || normalize::classifyChar(codepoints[start_pos]) != normalize::CharType::Hiragana) {
    return;
  }

  std::string elided;
  std::array<size_t, kElidedLookupWindow> elided_to_original{};
  size_t elided_length = 0;
  for (size_t pos = start_pos; pos < window_end; ++pos) {
    if (normalize::isProlongedSoundMark(codepoints[pos])) {
      continue;
    }
    if (normalize::classifyChar(codepoints[pos]) != normalize::CharType::Hiragana) {
      break;
    }
    elided += normalize::encodeUtf8(codepoints[pos]);
    elided_to_original[elided_length] = pos;
    ++elided_length;
  }

  std::vector<dictionary::LookupResult> elided_results;
  dict_manager.lookupInto(elided, 0, elided_results);
  for (const auto& result : elided_results) {
    if (result.entry == nullptr || result.length < 2 || result.length > elided_length) {
      continue;
    }
    // A match that stops before the first elided mark is the plain reading the
    // ordinary lookup already produced.
    const size_t end_pos = elided_to_original[result.length - 1] + 1;
    const size_t elided_marks = (end_pos - start_pos) - result.length;
    if (elided_marks == 0) {
      continue;
    }
    // A sentence-final particle may hold the mora before its last one
    // (にゃ+ー+ん) once a predicate or nominal has closed in front of it: that
    // host fixes its left edge the way inflection fixes an auxiliary's, which
    // keeps it out of a drawn-out word (やばーい is no ば+い).
    const size_t last_mora = elided_to_original[result.length - 1];
    const bool held_final_particle =
        result.entry->extended_pos == core::ExtendedPOS::ParticleFinal &&
        last_mora == elided_to_original[result.length - 2] + 1 + elided_marks &&
        hasPrecedingPartOfSpeech(lattice, start_pos, kHeldFinalParticleHostMask) &&
        !core::anyEdgeEndingAt(lattice, end_pos, [start_pos](const core::LatticeEdge& edge) {
          return edge.start < start_pos && (partOfSpeechMask(edge.pos) & kPredicateHostMask) != 0;
        });
    if (result.entry->pos != core::PartOfSpeech::Auxiliary && !held_final_particle) {
      continue;
    }
    const std::string surface = extractSubstring(codepoints, start_pos, end_pos);
    const std::string_view lemma =
        result.entry->lemma.empty() ? std::string_view(result.entry->surface) : std::string_view(result.entry->lemma);
    // Charged the same per mark as emphasis at a word's end: the drawn-out
    // spelling is the marked one and must not undercut a word that owns the
    // whole run unmarked.
    const float cost = getCategoryCost(result.entry->extended_pos) +
                       (candidate::kEmphaticCharacterPenalty * static_cast<float>(elided_marks));
    tokenizer_dictionary_detail::addDictionaryOriginEdge(
        lattice, surface, start_pos, end_pos, result.entry->pos, cost,
        core::LatticeEdge::kFromDictionary | core::LatticeEdge::kHasCustomCost, lemma,
        dictionary::ConjugationType::None, result.entry->extended_pos, "dict_elided_prolonged");
  }
}

// Interrupted speech cuts an adverb before its closing と at the sokuon
// (ちょっ…待って for ちょっと). The cut form ends at a pause the utterance resumes
// after, so a kana run closed by っ in front of punctuation followed by more
// text is looked up with the と put back, and an adverb found that way spans
// the cut surface with its own lemma.
void addTruncatedAdverbCandidates(core::Lattice& lattice, const dictionary::DictionaryManager& dict_manager,
                                  const std::vector<char32_t>& codepoints, size_t start_pos) {
  const size_t window_end = std::min(codepoints.size(), start_pos + kElidedLookupWindow);
  for (size_t pos = start_pos + 1; pos < window_end; ++pos) {
    if (normalize::classifyChar(codepoints[pos]) != normalize::CharType::Hiragana) {
      return;
    }
    if (codepoints[pos] != U'っ') {
      continue;
    }
    const size_t end_pos = pos + 1;
    size_t resume_pos = end_pos;
    while (resume_pos < codepoints.size() &&
           normalize::classifyChar(codepoints[resume_pos]) == normalize::CharType::Symbol) {
      ++resume_pos;
    }
    if (resume_pos == end_pos || resume_pos >= codepoints.size()) {
      return;
    }
    const std::string surface = extractSubstring(codepoints, start_pos, end_pos);
    const auto* adverb = dict_manager.lookupExact(surface + "と", core::PartOfSpeech::Adverb);
    if (adverb == nullptr) {
      return;
    }
    tokenizer_dictionary_detail::addDictionaryOriginEdge(
        lattice, surface, start_pos, end_pos, adverb->pos, getCategoryCost(adverb->extended_pos),
        core::LatticeEdge::kFromDictionary,
        adverb->lemma.empty() ? std::string_view(adverb->surface) : std::string_view(adverb->lemma),
        dictionary::ConjugationType::None, adverb->extended_pos, "dict_truncated_adverb");
    return;
  }
}

// Casual speech clips the long vowel off a greeting (ありがと, おはよ), spells
// it with the prolonged mark (ありがとー) or closes it on a sokuon (おはよっ). The listed interjection ends on its
// o-row mora plus う, so a kana run closing on that mora is looked up with the
// う put back; the clipped span keeps the entry's own lemma. It must close the
// run, as the full greeting would, or hand off to a final or quotative particle.
void addClippedInterjectionCandidates(core::Lattice& lattice, const dictionary::DictionaryManager& dict_manager,
                                      const std::vector<char32_t>& codepoints, size_t start_pos) {
  const size_t window_end = std::min(codepoints.size(), start_pos + kElidedLookupWindow);
  for (size_t end_pos = start_pos + 2; end_pos <= window_end; ++end_pos) {
    if (normalize::classifyChar(codepoints[end_pos - 1]) != normalize::CharType::Hiragana) {
      return;
    }
    if (!kana::isORowCodepoint(codepoints[end_pos - 1])) {
      continue;
    }
    size_t span_end = end_pos;
    while (span_end < codepoints.size() && normalize::isProlongedSoundMark(codepoints[span_end])) {
      ++span_end;
    }
    // A glottal stop closing the clipped greeting (おはよっ！) clips it the
    // same way.
    if (span_end == end_pos && span_end < codepoints.size() && codepoints[span_end] == core::hiragana::kSmallTsu &&
        (span_end + 1 >= codepoints.size() ||
         normalize::classifyChar(codepoints[span_end + 1]) != normalize::CharType::Hiragana)) {
      ++span_end;
    }
    bool closes_run =
        span_end >= codepoints.size() || normalize::classifyChar(codepoints[span_end]) != normalize::CharType::Hiragana;
    // A final particle or the quotative that cites the greeting takes over
    // the clause (ありがと+ね, ありがと+って).
    if (!closes_run && span_end == end_pos) {
      constexpr size_t kHandoffParticleProbe = 2;
      for (const auto& match : lookupResultsInRange(dict_manager, codepoints, span_end,
                                                    std::min(codepoints.size(), span_end + kHandoffParticleProbe))) {
        closes_run = closes_run || (match.entry != nullptr && match.entry->pos == core::PartOfSpeech::Particle &&
                                    (match.entry->extended_pos == core::ExtendedPOS::ParticleFinal ||
                                     (match.entry->extended_pos == core::ExtendedPOS::ParticleQuote &&
                                      match.length == kHandoffParticleProbe)));
      }
    }
    if (!closes_run) {
      continue;
    }
    const auto* interjection = lookupClippedInterjection(dict_manager, codepoints, start_pos, end_pos);
    if (interjection == nullptr) {
      continue;
    }
    tokenizer_dictionary_detail::addDictionaryOriginEdge(
        lattice, extractSubstring(codepoints, start_pos, span_end), start_pos, span_end, interjection->pos,
        getCategoryCost(interjection->extended_pos), core::LatticeEdge::kFromDictionary,
        interjection->lemma.empty() ? std::string_view(interjection->surface) : std::string_view(interjection->lemma),
        dictionary::ConjugationType::None, interjection->extended_pos, "dict_clipped_interjection");
  }
}

}  // namespace suzume::analysis::tokenizer_dictionary_detail
