/**
 * @file unknown_bracketed_noun.cpp
 * @brief Bracketed hiragana noun rescue for unknown words
 *
 * A hiragana run bracketed by particles, the clause edges, a copula, a suffix
 * or a kanji head reads as a content noun even when the same-type scan cut it
 * short. This phase scans for such runs independently and offers additive
 * noun candidates for them.
 */

#include <algorithm>
#include <cstddef>

#include "analysis/unknown.h"
#include "analysis/unknown_bracketed_noun_internal.h"
#include "analysis/unknown_same_type_internal.h"
#include "candidate_constants.h"
#include "core/kana_constants.h"
#include "normalize/char_type.h"

namespace suzume {
namespace analysis {

namespace {
using bracketed_noun_detail::admitsPromotedRun;
using bracketed_noun_detail::appendPromotedRunCandidates;
using bracketed_noun_detail::BracketedNounContext;
using bracketed_noun_detail::BracketedScan;
using bracketed_noun_detail::classifyLeftBrackets;
using bracketed_noun_detail::classifyRightBrackets;
using bracketed_noun_detail::closesShortNounAt;
using bracketed_noun_detail::closesTeFormAt;
using bracketed_noun_detail::endsOnPredicateTail;
using bracketed_noun_detail::isImpossibleHiraganaStart;
using bracketed_noun_detail::PromotedRun;
using bracketed_noun_detail::pronounLengthAt;
using bracketed_noun_detail::readPromotedSurface;
using bracketed_noun_detail::registeredAdjectiveOpensAt;
using bracketed_noun_detail::scanBracketedRun;
using bracketed_noun_detail::suffixLengthAt;
using same_type_detail::BoundAuxiliary;
using same_type_detail::boundAuxiliaryAt;
using same_type_detail::isRightBoundaryParticle;
}  // namespace

void UnknownWordGenerator::appendBracketedHiraganaNounCandidates(const std::vector<char32_t>& codepoints,
                                                                 const std::vector<normalize::CharType>& char_types,
                                                                 const SameTypeRun& run,
                                                                 std::vector<UnknownCandidate>& candidates) const {
  const size_t start_pos = run.start_pos;
  const normalize::CharType start_type = run.start_type;
  const BracketedNounContext ctx{codepoints,    char_types,  start_pos,
                                 dict_manager_, inflection_, options_.verb_candidate_options};

  // Bracketed hiragana noun promotion. A short hiragana run genuinely bracketed by
  // particles (私は|たばこ|を, 彼は|ともだち|と) reads as a content noun, but the
  // same-type scan above truncates at the first internal particle character and a
  // particle-initial run (にんじん) is only ever emitted as a penalized particle-noun,
  // so the correct whole-run candidate never reaches the lattice. This dedicated
  // scan is independent of that truncation and emits an ADDITIVE Noun candidate; the
  // Other/particle candidates remain and any real dictionary/verb/adverb reading of
  // the span still outranks it, so it wins only when nothing better spans the
  // bracket and never shatters the run. Left bracket: a boundary particle after a
  // non-hiragana content word (私は…), or a clause boundary — sentence start / a
  // preceding symbol. Right bracket: a boundary particle. の is not a boundary
  // particle (genitive marks a compound boundary).
  BracketedScan bracketed = classifyLeftBrackets(ctx);
  if (start_type == normalize::CharType::Hiragana && !isImpossibleHiraganaStart(codepoints[start_pos])) {
    constexpr size_t kDefaultBracketedNounLength = 4;
    constexpr size_t kLongDeverbalNounLength = 5;
    const bool long_deverbal_object_shape =
        getMaxLength(start_type) >= kLongDeverbalNounLength &&
        start_pos + kLongDeverbalNounLength < codepoints.size() &&
        std::all_of(char_types.begin() + static_cast<std::ptrdiff_t>(start_pos),
                    char_types.begin() + static_cast<std::ptrdiff_t>(start_pos + kLongDeverbalNounLength),
                    [](normalize::CharType type) { return type == normalize::CharType::Hiragana; }) &&
        kana::isERowCodepoint(codepoints[start_pos + kLongDeverbalNounLength - 1]) &&
        codepoints[start_pos + kLongDeverbalNounLength] == U'を';
    const size_t bracketed_noun_limit =
        long_deverbal_object_shape ? kLongDeverbalNounLength : kDefaultBracketedNounLength;
    scanBracketedRun(ctx, bracketed_noun_limit, bracketed);
    // A te-form on the left closes its clause, so the run after it needs neither
    // a nominal nor an attributive in front of the connective (嬉しく+て+うれぴ).
    bracketed.left_te_bracket = start_pos > 0 && closesTeFormAt(ctx, start_pos - 1);
    // End of the maximal run when a boundary particle closes it; the shorter
    // runs offered below lie inside it.
    size_t longer_rescue_end = 0;
    auto emit_promoted_run = [&](size_t run_end) {
      if (endsOnPredicateTail(ctx, run_end)) {
        return false;
      }
      PromotedRun promoted = classifyRightBrackets(ctx, bracketed, run_end);
      readPromotedSurface(ctx, bracketed, promoted);
      if (!admitsPromotedRun(ctx, bracketed, promoted)) {
        return false;
      }
      float noun_cost = getCostForType(start_type, promoted.len) + candidate::kPostParticleNounPenalty;
      appendPromotedRunCandidates(ctx, bracketed, promoted, noun_cost, longer_rescue_end, candidates);
      return true;
    };
    const size_t scan = bracketed.scan;
    if (emit_promoted_run(scan) && scan < codepoints.size() && isRightBoundaryParticle(codepoints[scan])) {
      longer_rescue_end = scan;
    }
    // At the clause start or after a determiner a case particle or the topic は
    // inside the scanned run may equally close a short noun (そら|は|いつも,
    // ねこ|が, この+つめ|が), so the run that stops there is offered beside the
    // maximal one and scoring weighs them. Word-final は is read as the particle alone; も, か and the other
    // focus particles end native nouns as often as not (こども, くも).
    for (size_t particle_pos = start_pos + 2;
         (bracketed.left_clause_bracket || bracketed.left_determiner_bracket) && particle_pos < scan; ++particle_pos) {
      if (closesShortNounAt(dict_manager_, codepoints, particle_pos)) {
        emit_promoted_run(particle_pos);
      }
    }
    // A registered adjective opening inside the scanned run closes the noun in
    // front of it, so that shorter run is offered too (ねこ|かわいすぎ).
    for (size_t adjective_pos = start_pos + 2; adjective_pos < scan; ++adjective_pos) {
      if (registeredAdjectiveOpensAt(ctx, adjective_pos)) {
        emit_promoted_run(adjective_pos);
        break;
      }
    }
    // The scan runs on through a genitive inside its first three morae, so the
    // two-mora run it closes at the clause start is offered on its own.
    constexpr size_t kShortRunLength = 2;
    if (bracketed.left_clause_bracket && start_pos + kShortRunLength < scan &&
        codepoints[start_pos + kShortRunLength] == U'の') {
      emit_promoted_run(start_pos + kShortRunLength);
    }
    // The run that stops in front of a trailing auxiliary is offered beside the
    // maximal one, so the copula after an unregistered hiragana noun has
    // something to attach to (りんご|だ|と instead of りんごだ|と, which the scan
    // cannot reach because it only breaks at particle-shaped boundaries). The
    // auxiliary has to carry its own continuation to count: a clause-final one is
    // indistinguishable from word-final kana (たたずむ, まばたき, ありがち), while
    // anything bound behind it shows the auxiliary heading its own predicate.
    // The continuation is a boundary particle or a further auxiliary, because an
    // inflected auxiliary selects the next one and the pair is then the whole
    // predicate (りんご + だっ + た). Requiring the auxiliary to end where the run
    // ends would miss exactly that case, since the scan stops at its length limit
    // in the middle of the auxiliary. Offering both runs rather than moving the
    // break is what keeps a noun that merely spans those kana intact (からだ,
    // たなばた).
    // Every such position is offered rather than only the first: a one-mora
    // auxiliary can also sit word-internally in front of the real break (みか|ん|
    // だ|と against みかん|だ|と), and stopping there would hide the run the
    // copula actually brackets.
    // A non-copula auxiliary whose only continuation is the particle that
    // closes the maximal run is that run's last mora (まなざ|し|に is
    // まなざし|に): it selects a predicate cell, not the particle.
    for (size_t trimmed = start_pos + 1; trimmed < scan; ++trimmed) {
      const BoundAuxiliary bound = boundAuxiliaryAt(codepoints, trimmed, dict_manager_,
                                                    bracketed.left_particle_bracket || bracketed.left_clause_bracket);
      const bool auxiliary_is_last_mora =
          bound.length > 0 && !bound.is_copula && trimmed + bound.length == longer_rescue_end;
      if ((bound.length > 0 && !auxiliary_is_last_mora) ||
          (trimmed >= start_pos + 2 && (suffixLengthAt(ctx, trimmed) > 0 || pronounLengthAt(ctx, trimmed) > 0))) {
        emit_promoted_run(trimmed);
      }
    }
  }
}

}  // namespace analysis
}  // namespace suzume
