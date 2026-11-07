/**
 * @file verb_candidates_hiragana_predicate_run.cpp
 * @brief Extent of the kana run a hiragana verb candidate may span
 *
 * Breaks the run at particle morae unless a closed tail carries it across, and
 * probes the Godan-ra continuation a particle-homographic が/や may hide.
 */

#include <algorithm>

#include "analysis/bigram_table.h"
#include "analysis/candidate_constants.h"
#include "analysis/dictionary_probe.h"
#include "analysis/verb_candidates_absorption_guards.h"
#include "analysis/verb_candidates_auxiliary_patterns.h"
#include "analysis/verb_candidates_classical.h"
#include "analysis/verb_candidates_dictionary_probes.h"
#include "analysis/verb_candidates_emphatic.h"
#include "analysis/verb_candidates_hiragana_internal.h"
#include "core/debug.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "grammar/inflection_scorer_constants.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "tokenizer_utils.h"
#include "unknown.h"
#include "verb_candidates.h"

namespace suzume::analysis {

namespace vh = verb_helpers;
namespace hiragana_verb_detail {

namespace {

bool hasLongGodanWaNegativeEvidence(const std::vector<char32_t>& codepoints, size_t start_pos, size_t current_pos,
                                    const std::vector<normalize::CharType>& char_types) {
  for (size_t negative_pos = current_pos + 1;
       negative_pos + 2 < codepoints.size() && negative_pos - start_pos < kPredicateRunMax; ++negative_pos) {
    if (char_types[negative_pos] != normalize::CharType::Hiragana) {
      break;
    }
    if (negative_pos >= start_pos + 3 && textMatchesAt(codepoints, negative_pos, "わない")) {
      return true;
    }
  }
  return false;
}

// A particle-homographic が/や may occur inside an open-class Godan verb.
// Derive the permitted terminal and sokuonbin shapes from the canonical row
// table, rather than treating those morae as unconditional particle breaks.
// The Sa-row escape uses the complete inflectional shape because がす has no
// row-specific marker before its terminal す.
// Returns the predicate stem end, or zero when the remaining run has no
// complete inflectional shape.
//
// The row is fixed to ra rather than taken from the row table, because the
// homograph identifies no row on its own: the sokuonbin っ is shared with
// ka/ta/wa, and the terminal of every other row closes an ordinary nominative
// clause as if it were one verb (ぼくがいく, とりがとぶ). What this rule fails
// to reach — ながす, ころがす, のがす — is held back by the prefix guard below,
// not by the row.
size_t godanContinuationStemEnd(const std::vector<char32_t>& codepoints, size_t start_pos, size_t current_pos,
                                const std::vector<normalize::CharType>& char_types,
                                const grammar::Inflection& inflection,
                                const dictionary::DictionaryManager* dict_manager,
                                const VerbCandidateOptions& verb_opts) {
  if (current_pos >= codepoints.size() || (codepoints[current_pos] != U'が' && codepoints[current_pos] != U'や')) {
    return 0;
  }
  size_t run_end = current_pos;
  while (run_end < char_types.size() && run_end - start_pos < kPredicateRunMax &&
         char_types[run_end] == normalize::CharType::Hiragana) {
    ++run_end;
  }

  const auto* row = grammar::Conjugation::getGodanRow(grammar::VerbType::GodanRa);
  if (row == nullptr) {
    return 0;
  }

  // A complete Sa-row cell is stronger than the lexical status of its prefix:
  // の/な/さ are all closed-class homographs inside ordinary open-class stems.
  // A real predicate beginning at が/や still marks the boundary (で+やる).
  size_t godan_sa_end = 0;
  for (const auto& candidate : analysesInRange(inflection, codepoints, start_pos, run_end)) {
    if (candidate.verb_type == grammar::VerbType::GodanSa && candidate.confidence >= verb_opts.confidence_low) {
      godan_sa_end = run_end;
      break;
    }
  }
  if (dict_manager != nullptr) {
    const std::string prefix = extractSubstring(codepoints, start_pos, current_pos);
    const std::string suffix = extractSubstring(codepoints, current_pos, run_end);
    constexpr PartOfSpeechMask kPredicateMask =
        partOfSpeechMask(core::PartOfSpeech::Verb) | partOfSpeechMask(core::PartOfSpeech::Auxiliary);
    const auto* suffix_entry = dict_manager->lookupExact(suffix);
    const bool suffix_is_garu = suffix_entry != nullptr && suffix_entry->extended_pos == core::ExtendedPOS::AuxGaru;
    const auto* noun_prefix = dict_manager->lookupExact(prefix, core::PartOfSpeech::Noun);
    const bool prefix_can_host_garu = dict_manager->lookupExact(prefix, core::PartOfSpeech::Adjective) != nullptr ||
                                      (noun_prefix != nullptr && noun_prefix->extended_pos == core::ExtendedPOS::Noun);
    const bool suffix_is_licensed_predicate =
        hasExactPartOfSpeech(*dict_manager, suffix, kPredicateMask) && (!suffix_is_garu || prefix_can_host_garu);
    const bool has_long_noun_prefix = prefix.size() >= 6 &&
                                      dict_manager->lookupExact(prefix, core::PartOfSpeech::Particle) == nullptr &&
                                      dict_manager->lookupExact(prefix, core::PartOfSpeech::Noun) != nullptr;
    if (godan_sa_end != 0 && !suffix_is_licensed_predicate) {
      return godan_sa_end;
    }

    // Preserve the established Ra-row prefix guards for shapes where the
    // particle homograph does not itself identify a conjugation row.
    if (const auto* particle = dict_manager->lookupExact(prefix, core::PartOfSpeech::Particle);
        particle != nullptr && particle->extended_pos != core::ExtendedPOS::ParticleTopic &&
        particle->extended_pos != core::ExtendedPOS::ParticleBinding) {
      return 0;
    }
    constexpr core::PartOfSpeech kClosedClassPrefixes[] = {
        core::PartOfSpeech::Adverb,  core::PartOfSpeech::Conjunction, core::PartOfSpeech::Determiner,
        core::PartOfSpeech::Pronoun, core::PartOfSpeech::Prefix,
    };
    for (const core::PartOfSpeech pos : kClosedClassPrefixes) {
      if (dict_manager->lookupExact(prefix, pos) != nullptr) {
        return 0;
      }
    }
    if (has_long_noun_prefix && !(suffix_is_garu && !prefix_can_host_garu)) {
      return 0;
    }
    // An inflected auxiliary or adjective cell already closes its own word
    // before a predicate (まで+も+なく+やる), and so does a continuative the
    // tables read as an i-adjective's く (かるく+やる).
    constexpr PartOfSpeechMask kInflectedPrefixMask =
        partOfSpeechMask(core::PartOfSpeech::Auxiliary) | partOfSpeechMask(core::PartOfSpeech::Adjective);
    const auto prefix_analyses = analysesInRange(inflection, codepoints, start_pos, current_pos);
    const bool adjective_continuative_prefix =
        utf8::endsWith(prefix, "く") &&
        std::any_of(prefix_analyses.begin(), prefix_analyses.end(), [&](const auto& candidate) {
          return candidate.verb_type == grammar::VerbType::IAdjective &&
                 candidate.confidence >= verb_opts.confidence_low;
        });
    if (suffix_is_licensed_predicate && !suffix_is_garu &&
        (hasExactPartOfSpeech(*dict_manager, prefix, kInflectedPrefixMask) || adjective_continuative_prefix)) {
      return 0;
    }
  } else if (godan_sa_end != 0) {
    return godan_sa_end;
  }

  const char32_t onbin = utf8::decodeFirstChar(grammar::onbinFormOf(*row));
  for (size_t pos = current_pos + 1; pos < char_types.size() && pos - start_pos < kPredicateRunMax; ++pos) {
    if (char_types[pos] != normalize::CharType::Hiragana) {
      break;
    }
    if (codepoints[pos] == row->base_vowel) {
      return pos + 1;
    }
    if (codepoints[pos] == onbin && pos + 1 < char_types.size() &&
        (codepoints[pos + 1] == U'た' || codepoints[pos + 1] == U'だ' || codepoints[pos + 1] == core::hiragana::kTe)) {
      return pos + 1;
    }
  }

  return 0;
}

// Whether a か inside a kana run continues a verb conjugation rather than
// standing as the particle.
bool kaContinuesConjugation(const std::vector<char32_t>& codepoints, size_t ka_pos) {
  // なかった (negative past)
  if (codepoints[ka_pos - 1] == U'な') {
    return true;
  }
  const char32_t next = ka_pos + 1 < codepoints.size() ? codepoints[ka_pos + 1] : 0;
  const char32_t after_next = ka_pos + 2 < codepoints.size() ? codepoints[ka_pos + 2] : 0;
  // Godan-ra endings (わかり, わかる, わかれ, わかっ), the godan-ka irrealis
  // before ない (いかない), causative/transitive/classical negative (つかせる,
  // つかさどる, いかず), and the godan-wa terminal (つかう). Scoring rejects the
  // impossible mizenkei+volitional reading (つか+う) separately.
  if (next == U'り' || next == U'る' || next == U'れ' || next == U'ろ' || next == U'っ' || next == U'な' ||
      next == U'せ' || next == U'さ' || next == U'ず' || next == U'う') {
    return true;
  }
  // GodanMa/Na/Ba onbin te/ta-form (つかんで, 歩かんで)
  if (next == U'ん' && (after_next == U'で' || after_next == U'だ')) {
    return true;
  }
  // Mizenkei + contracted negative (わからん), godan-ra irrealis + negative
  // (わからない), and godan-wa irrealis (つかわない, つかわれる, つかわせる, つかわず)
  if ((kana::isARowCodepoint(next) && after_next == U'ん') || (next == U'ら' && after_next == U'な') ||
      (next == U'わ' && (after_next == U'な' || after_next == U'れ' || after_next == U'せ' || after_next == U'ず'))) {
    return true;
  }
  // Godan-ra ん音便 + the negative paradigm (わかんない, わかんなかった)
  if (next == U'ん' && vh::naiNegativeFollowsAt(codepoints, ka_pos + 2)) {
    return true;
  }
  // Godan-wa continuative before ます (つかい→使う, むかい→向かう). The
  // following ま keeps bare か + いる (誰か+いる) breaking here.
  return next == U'い' && after_next == U'ま';
}

}  // namespace

// Find the end of the hiragana run a verb may span, breaking at particle
// boundaries the closed tails do not cross. Sets @p crossed_particle_guard when
// a closed onbin tail crossed a particle mora, and @p
// godan_ra_continuation_stem_end when a Godan-ra continuation crosses one.
size_t scanPredicateRunEnd(const HiraganaVerbScan& scan, const PredicateRunTails& tails, bool& crossed_particle_guard,
                           size_t& godan_ra_continuation_stem_end) {
  const auto& codepoints = scan.codepoints;
  const size_t start_pos = scan.start_pos;
  const auto& char_types = scan.char_types;
  const auto* dict_manager = scan.dict_manager;
  const size_t complete_case_particle_terminal_end = tails.complete_case_particle_terminal_end;

  // Find hiragana sequence, breaking at particle boundaries
  // Note: Be careful not to break at characters that are part of verb conjugations:
  //   - か can be part of なかった (negative past) or かった (i-adj past)
  //   - で can be part of んで (te-form for godan) or できる (potential verb)
  //   - も can be part of ても (even if) or もらう (receiving verb)
  size_t hiragana_end = start_pos;
  while (hiragana_end < char_types.size() && hiragana_end - start_pos < kPredicateRunMax &&
         char_types[hiragana_end] == normalize::CharType::Hiragana) {
    // Don't include particles that appear after the first hiragana character.
    // E.g., for "りにする", stop at "り" to not include "にする".
    if (hiragana_end > start_pos) {
      char32_t curr = codepoints[hiragana_end];

      // A complete onbin+tense tail in a predicate slot is stronger evidence
      // than a particle-homograph inside its stem (しゃがんだ, ともった).  Carry
      // the scanner only as far as that closed tail; unrelated hiragana after
      // it still goes through the ordinary boundary checks.
      if (tails.closed_onbin_tense_end != 0 && hiragana_end < tails.closed_onbin_tense_end) {
        crossed_particle_guard =
            crossed_particle_guard || normalize::isNeverVerbStemAfterKanji(curr) || normalize::isExtendedParticle(curr);
        ++hiragana_end;
        continue;
      }
      if (complete_case_particle_terminal_end != 0 && hiragana_end < complete_case_particle_terminal_end) {
        ++hiragana_end;
        continue;
      }
      if (tails.comma_clause_end != 0 && hiragana_end < tails.comma_clause_end) {
        ++hiragana_end;
        continue;
      }

      // Check for particle-like characters (common particles + も, や)
      const bool has_godan_wa_negative =
          hasLongGodanWaNegativeEvidence(codepoints, start_pos, hiragana_end, char_types);
      const size_t godan_ra_stem_end = godanContinuationStemEnd(codepoints, start_pos, hiragana_end, char_types,
                                                                scan.inflection, dict_manager, scan.verb_opts);
      if (godan_ra_stem_end != 0) {
        godan_ra_continuation_stem_end = godan_ra_stem_end;
      }
      // A particle is licensed by the phrase in front of it, and one mora of
      // hiragana is not one unless the dictionary names it (ながれた, うながす:
      // the が is the second mora of a stem, not a case marker). The verb
      // window only widens here — the particle keeps its own edge and every
      // candidate the split reading already had, so the two readings still
      // compete on score rather than on which one was generated.
      const bool particle_lacks_nominal_host = hiragana_end == start_pos + 1 && dict_manager != nullptr &&
                                               !hasExactPartOfSpeech(*dict_manager, codepoints, start_pos, hiragana_end,
                                                                     partOfSpeechMask(core::PartOfSpeech::Noun) |
                                                                         partOfSpeechMask(core::PartOfSpeech::Pronoun));
      if (normalize::isNeverVerbStemAfterKanji(curr) && !(curr == U'の' && has_godan_wa_negative) &&
          !particle_lacks_nominal_host && godan_ra_stem_end == 0) {
        SUZUME_DEBUG_LOG_TRACE("[HIRA_SEQ] pos=" << hiragana_end << " char=U+" << std::hex
                                                 << static_cast<uint32_t>(curr) << std::dec
                                                 << " action=break (isNeverVerbStemAfterKanji)\n");
        break;  // These are always particles in this context
      }

      // For か, で, も, と: check if they're part of verb conjugation patterns
      // Don't break if they appear in known conjugation contexts
      if (curr == U'か' || curr == U'で' || curr == U'も' || curr == U'と') {
        // か continues its own conjugation, で follows ん (んで) or き (できる), も
        // follows て (ても), and と follows っ (やっとく = やって + おく) or opens a
        // long godan-wa negative.
        const char32_t prev = codepoints[hiragana_end - 1];
        // で followed by た/て/る or ながら/つつ closes an ichidan stem (なで+た,
        // なでる): neither the copula nor the particle で takes them.
        const auto closes_ichidan_de_stem = [&]() {
          const size_t next = hiragana_end + 1;
          if (next >= codepoints.size()) {
            return false;
          }
          if (codepoints[next] == U'た' || codepoints[next] == U'て' || codepoints[next] == U'る') {
            return true;
          }
          return vh::continuativeConjunctiveFollowsAt(codepoints, next);
        };
        if ((curr == U'か' && kaContinuesConjugation(codepoints, hiragana_end)) ||
            (curr == U'で' && (prev == U'ん' || prev == U'き' || closes_ichidan_de_stem())) ||
            (curr == U'も' && prev == U'て') || (curr == U'と' && (prev == U'っ' || has_godan_wa_negative))) {
          ++hiragana_end;
          continue;
        }

        // Otherwise, treat as particle
        SUZUME_DEBUG_LOG_TRACE("[HIRA_SEQ] pos=" << hiragana_end << " char=U+" << std::hex
                                                 << static_cast<uint32_t>(curr) << std::dec
                                                 << " action=break (unrecognized_particle_context)\n");
        break;
      }
    }
    ++hiragana_end;
  }
  if (tails.complete_godan_wa_terminal_end != 0) {
    hiragana_end = tails.complete_godan_wa_terminal_end;
  }
  if (complete_case_particle_terminal_end != 0) {
    hiragana_end = complete_case_particle_terminal_end;
  }

  // A previously recognized onbin tail can carry the scanner across a
  // particle-homographic や before the ordinary boundary loop reaches it.
  // Probe the final bounded run once so the Godan-ra candidate below is still
  // emitted for forms such as はやった.
  if (godan_ra_continuation_stem_end == 0) {
    for (size_t probe_pos = start_pos + 1; probe_pos < hiragana_end; ++probe_pos) {
      const size_t stem_end = godanContinuationStemEnd(codepoints, start_pos, probe_pos, char_types, scan.inflection,
                                                       dict_manager, scan.verb_opts);
      if (stem_end != 0) {
        godan_ra_continuation_stem_end = stem_end;
        break;
      }
    }
  }

  // The verified contraction spans its own ん even where the boundary scan
  // stopped at a particle-homographic mora (わ+かん).
  hiragana_end = std::max(hiragana_end, tails.contracted_irrealis_end);

  // Log final hiragana sequence bounds
  SUZUME_DEBUG_LOG_TRACE("[HIRA_SEQ] final: start=" << start_pos << " end=" << hiragana_end
                                                    << " len=" << (hiragana_end - start_pos) << "\n");
  return hiragana_end;
}

}  // namespace hiragana_verb_detail
}  // namespace suzume::analysis
