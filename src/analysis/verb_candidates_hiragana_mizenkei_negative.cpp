/**
 * @file verb_candidates_hiragana_mizenkei_negative.cpp
 * @brief Negative pure-hiragana Godan mizenkei candidate families
 */

#include <algorithm>
#include <cmath>

#include "analysis/bigram_table.h"
#include "analysis/candidate_constants.h"
#include "analysis/dictionary_probe.h"
#include "analysis/scorer_constants.h"
#include "analysis/verb_candidates_helpers.h"
#include "analysis/verb_candidates_hiragana_internal.h"
#include "core/debug.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "unknown.h"
#include "verb_candidates.h"

namespace suzume::analysis::hiragana_verb_detail {
namespace vh = verb_helpers;

// Detect a formal-noun prefix boundary inside an unverified hiragana verb stem.
// A stem that begins with a dictionary formal noun (わけ, こと, もの, ところ, ...)
// followed by a remainder of two or more characters is usually a noun + verb
// sequence (わけ + わから), not a single verb (わけわかる is not a word).
// Formal nouns form independent word boundaries, so callers add a
// split-preference penalty when this returns true.
bool hasFormalNounPrefixBoundary(const dictionary::DictionaryManager* dict_manager,
                                 const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  if (dict_manager == nullptr || end_pos <= start_pos) {
    return false;
  }
  const size_t total_len = end_pos - start_pos;
  // Both the noun prefix and the verb remainder need at least two characters
  if (total_len < 4) {
    return false;
  }
  for (size_t prefix_len = 2; prefix_len + 2 <= total_len; ++prefix_len) {
    const auto* entry =
        lookupEntryInRange(*dict_manager, codepoints, start_pos, start_pos + prefix_len, core::PartOfSpeech::Noun);
    if (entry != nullptr && entry->extended_pos == core::ExtendedPOS::NounFormal) {
      return true;
    }
  }
  return false;
}

// A formal noun that ends immediately before the final A-row mora is not an
// independent noun + predicate boundary: the remainder is only the irrealis
// ending itself. When the following negative auxiliary validates the complete
// cell, the formal-noun spelling is therefore a stem homograph (ためさ+ない),
// unlike a substantive remainder that forms its own predicate
// (わけ+わから+ない).
bool hasFormalNounStemHomograph(const dictionary::DictionaryManager* dict_manager,
                                const std::vector<char32_t>& codepoints, size_t start_pos, size_t mizenkei_end) {
  if (dict_manager == nullptr || mizenkei_end < start_pos + 3) {
    return false;
  }
  const auto* entry =
      lookupEntryInRange(*dict_manager, codepoints, start_pos, mizenkei_end - 1, core::PartOfSpeech::Noun);
  return entry != nullptr && entry->extended_pos == core::ExtendedPOS::NounFormal;
}

// What a derived irrealis is worth as evidence: whether some reading of the
// analysed form reconstructs the same verb, whether the base form is attested,
// and whether it is an unattested sa-row irrealis.
struct MizenkeiEvidence {
  bool is_valid_verb;
  bool is_in_dict;
  bool unattested_sa_irrealis;
};

// The gates below differ only in the form they analyse — the auxiliary the text
// actually carries, or the equivalent ない form — and weigh the result the same
// way afterwards, so they ask for it here rather than each repeating the scan.
MizenkeiEvidence judgeMizenkeiForms(const dictionary::DictionaryManager* dict_manager,
                                    const grammar::Inflection& inflection, const std::string& full_form,
                                    const GodanMizenkeiForms& forms, const std::vector<char32_t>& codepoints,
                                    size_t start_pos, size_t mizenkei_end) {
  MizenkeiEvidence evidence{};
  for (const auto& cand : inflection.analyze(full_form)) {
    if (cand.verb_type == forms.verb_type && cand.base_form == forms.base_form) {
      evidence.is_valid_verb = true;
      break;
    }
  }
  evidence.is_in_dict = vh::isVerbInDictionary(dict_manager, forms.base_form);
  if (!evidence.is_valid_verb) {
    evidence.is_valid_verb = evidence.is_in_dict;
  }
  const bool formal_noun_stem_homograph = hasFormalNounStemHomograph(dict_manager, codepoints, start_pos, mizenkei_end);
  evidence.unattested_sa_irrealis = forms.verb_type == grammar::VerbType::GodanSa && !evidence.is_in_dict &&
                                    grammar::isPureHiragana(forms.stem) && !formal_noun_stem_homograph;
  return evidence;
}

// An unverified mizenkei may start by absorbing a particle before a real verb
// (に+行かない, は+ならない, も+ならない).  Preserve that closed-class
// boundary when the remainder reconstructs to a dictionary verb.
bool hasLeadingParticleVerbBoundary(const dictionary::DictionaryManager* dict_manager,
                                    const std::vector<char32_t>& codepoints, size_t start_pos, size_t mizenkei_end,
                                    std::string_view base_suffix) {
  if (dict_manager == nullptr || mizenkei_end - start_pos < 3 || base_suffix.empty()) {
    return false;
  }
  for (size_t split_pos = start_pos + 1; split_pos + 1 < mizenkei_end; ++split_pos) {
    const std::string particle = extractSubstring(codepoints, start_pos, split_pos);
    if (!vh::hasParticleDictionaryEntry(dict_manager, particle)) {
      continue;
    }
    const std::string remainder_stem = extractSubstring(codepoints, split_pos, mizenkei_end - 1);
    const std::string remainder_base = normalize::concat(remainder_stem, base_suffix);
    if (vh::isVerbInDictionary(dict_manager, remainder_base)) {
      return true;
    }
  }
  return false;
}

void appendMizenkeiNCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t hiragana_end,
                               const dictionary::DictionaryManager* dict_manager,
                               std::vector<UnknownCandidate>& candidates) {
  // Pattern: A-row hiragana (mizenkei ending) + ん
  // NOTE: Skip さ+ん pattern - さん/さま are honorific suffixes, not verb 未然形+ん
  for (size_t end_pos = hiragana_end; end_pos > start_pos + 1; --end_pos) {
    // Check if the string ends with ん at this position
    if (end_pos >= codepoints.size() || codepoints[end_pos] != U'ん') {
      continue;
    }

    // Check if position end_pos-1 is A-row hiragana (mizenkei ending)
    size_t mizenkei_end = end_pos;  // Position of ん (exclusive end of mizenkei)
    if (mizenkei_end <= start_pos)
      continue;

    GodanMizenkeiForms forms;
    if (!deriveGodanMizenkeiForms(codepoints, start_pos, mizenkei_end, forms)) {
      continue;
    }
    // Skip さ+ん pattern - さん is almost always an honorific suffix, not verb 未然形+ん
    // E.g., おねえさん, おかあさん, おじさん should not be parsed as verb + contracted negative
    if (forms.a_row_char == U'さ') {
      continue;
    }
    grammar::VerbType verb_type = forms.verb_type;
    const std::string& mizenkei_surface = forms.mizenkei_surface;
    const std::string& base_form = forms.base_form;

    // Validate: check if base form exists in dictionary
    // The inflection analysis is too permissive and will match almost any input,
    // so we require dictionary confirmation to avoid false positives
    // like おねえさん → おねえさ + ん (おねえす is not a real verb)
    bool is_valid_verb = vh::isVerbInDictionary(dict_manager, base_form);

    // Minimum stem length check: need at least 2 chars in mizenkei to be meaningful
    // This prevents false positives like "かん" → "か" + "ん"
    if (!is_valid_verb && mizenkei_surface.size() < 6) {  // 2 chars = 6 bytes in UTF-8
      continue;
    }

    // Get lemma from dictionary entry if available
    std::string lemma = vh::lookupVerbLemma(dict_manager, mizenkei_surface, base_form);

    // Generate mizenkei candidate with explicit VerbMizenkei EPOS for bigram connection
    // Use negative cost for valid verbs (to beat unsplit form)
    // Use positive cost for unconfirmed verbs (long hiragana that might be verbs)
    // This prevents false positives like おねえさん → おねえさ + ん
    float cost = is_valid_verb ? -0.5F : 1.0F;
    // Unverified stems starting with a formal noun are noun + verb sequences
    // (わけわから+ん should split as わけ + わから + ん)
    if (!is_valid_verb && hasFormalNounPrefixBoundary(dict_manager, codepoints, start_pos, mizenkei_end)) {
      cost += bigram_cost::kStrong;
    }
    SUZUME_DEBUG_VERBOSE_BLOCK {
      SUZUME_DEBUG_STREAM << "[VERB_CAND] " << mizenkei_surface << " hiragana_mizenkei_n lemma=" << lemma
                          << " cost=" << cost << "\n";
    }
    candidates.push_back(makeVerbCandidate(mizenkei_surface, start_pos, mizenkei_end, cost, lemma,
                                           grammar::verbTypeToConjType(verb_type), true, CandidateOrigin::VerbHiragana,
                                           0.9F, "hiragana_mizenkei_n", core::ExtendedPOS::VerbMizenkei));
    break;  // Only generate one candidate per position
  }
}

// Godan mizenkei + a negative auxiliary (わからない → わから + ない,
// わからなかった → わから + なかっ + た, しらざりき → しら + ざり + き).
// Loop includes end_pos == start_pos + 2 for 2-char stems like いか (いく).
void appendMizenkeiNegativeCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t hiragana_end,
                                      const grammar::Inflection& inflection,
                                      const dictionary::DictionaryManager* dict_manager,
                                      std::vector<UnknownCandidate>& candidates) {
  // Pattern: A-row hiragana (mizenkei ending) + a negative auxiliary. The whole
  // paradigm of that auxiliary licenses the cell equally — the terminal ない is
  // one of its forms, not the condition — so the trigger reads the closed class
  // out of the dictionary rather than naming a single form.
  for (size_t end_pos = hiragana_end; end_pos >= start_pos + 2; --end_pos) {
    const size_t aux_len = vh::negativeAuxiliaryLengthAt(dict_manager, codepoints, end_pos);
    if (aux_len == 0) {
      continue;
    }

    // Check if position end_pos-1 is A-row hiragana (mizenkei ending)
    size_t mizenkei_end = end_pos;  // Position of な (exclusive end of mizenkei)
    if (mizenkei_end <= start_pos)
      continue;

    GodanMizenkeiForms forms;
    if (!deriveGodanMizenkeiForms(codepoints, start_pos, mizenkei_end, forms)) {
      continue;
    }
    grammar::VerbType verb_type = forms.verb_type;
    const std::string& mizenkei_surface = forms.mizenkei_surface;
    const std::string& base_form = forms.base_form;

    // Validate: analyze the full form (including the auxiliary) to check if it's a valid verb
    const std::string full_form = mizenkei_surface + extractSubstring(codepoints, end_pos, end_pos + aux_len);
    const MizenkeiEvidence evidence =
        judgeMizenkeiForms(dict_manager, inflection, full_form, forms, codepoints, start_pos, mizenkei_end);
    const bool is_valid_verb = evidence.is_valid_verb;
    const bool is_in_dict = evidence.is_in_dict;
    const bool unattested_sa_irrealis = evidence.unattested_sa_irrealis;
    // Reject a fabricated mizenkei that merely absorbs a trailing adverbial
    // particle (みるしか / やるしか = verb + しか, never the 未然形 of a non-word).
    if (!is_in_dict && endsWithParticleAfterVerb(dict_manager, inflection, codepoints, start_pos, mizenkei_end)) {
      continue;
    }
    if (!is_in_dict && vh::endsWithFocusParticleTail(dict_manager, codepoints, start_pos, mizenkei_end)) {
      continue;
    }

    // Minimum stem length check: need at least 2 chars in mizenkei to be meaningful
    // This prevents false positives like "かない" → "か" + "ない"
    if (!is_valid_verb && mizenkei_surface.size() < 6) {  // 2 chars = 6 bytes in UTF-8
      continue;
    }

    // Get lemma from dictionary entry if available
    std::string lemma = vh::lookupVerbLemma(dict_manager, mizenkei_surface, base_form);

    // Dict-verified verbs get standard bonus; unverified get weaker cost
    // to prevent false hiragana verb candidates (e.g., はいか from はいく)
    // from beating particle+verb splits (は+いか)
    float cost_negative = candidate::verb_cost::kStandardBonus;  // -0.5
    if (!is_in_dict && mizenkei_surface.size() >= 6) {           // 2+ char stems
      cost_negative = 0.5F;                                      // Positive cost for unverified candidates
    }
    // Both charges describe the same thing — the reading rests on a mora that
    // is also spelled like something else — so the weaker evidence sets the
    // price rather than the two compounding into a near-prohibition and taking
    // the sa-row irrealis before the classical negative with it (手を+かさ+ず).
    const float sa_row_ambiguity =
        unattested_sa_irrealis ? candidate::verb_cost::kPureHiraganaSaIrrealisPenalty : bigram_cost::kNeutral;
    const float auxiliary_ambiguity =
        aux_len == 1 && !is_in_dict ? candidate::verb_cost::kMonomoraNegativeIrrealisPenalty : bigram_cost::kNeutral;
    cost_negative += std::max(sa_row_ambiguity, auxiliary_ambiguity);
    // A case particle immediately before the stem supplies a predicate slot,
    // so the classical negative is stronger evidence than the opaque-noun
    // fallback (資料を+しら+ぬ).  This remains tied to the auxiliary class,
    // rather than granting every one-mora continuation the same discount.
    const auto* following_auxiliary =
        dict_manager == nullptr
            ? nullptr
            : lookupEntryInRange(*dict_manager, codepoints, end_pos, end_pos + aux_len, core::PartOfSpeech::Auxiliary);
    const auto* preceding_particle =
        start_pos == 0 || dict_manager == nullptr
            ? nullptr
            : lookupEntryInRange(*dict_manager, codepoints, start_pos - 1, start_pos, core::PartOfSpeech::Particle);
    if (!is_in_dict && following_auxiliary != nullptr &&
        following_auxiliary->extended_pos == core::ExtendedPOS::AuxNegativeNu && preceding_particle != nullptr &&
        preceding_particle->extended_pos == core::ExtendedPOS::ParticleCase) {
      cost_negative += candidate::verb_cost::kStrongBonus;
    }
    // Unverified stems starting with a formal noun are noun + verb sequences
    // (わけわから+ない should split as わけ + わから + ない)
    if (!is_in_dict && hasFormalNounPrefixBoundary(dict_manager, codepoints, start_pos, mizenkei_end)) {
      cost_negative += bigram_cost::kStrong;
    }
    if (!is_in_dict &&
        hasLeadingParticleVerbBoundary(dict_manager, codepoints, start_pos, mizenkei_end, forms.base_suffix)) {
      cost_negative += bigram_cost::kStrong;
    }
    if (!is_in_dict &&
        vh::hasInternalVerbChainBoundary(codepoints, start_pos, mizenkei_end, inflection, dict_manager)) {
      continue;
    }
    SUZUME_DEBUG_VERBOSE_BLOCK {
      SUZUME_DEBUG_STREAM << "[VERB_CAND] " << mizenkei_surface << " hiragana_mizenkei_negative lemma=" << lemma
                          << " cost=" << cost_negative << "\n";
    }
    candidates.push_back(makeVerbCandidate(mizenkei_surface, start_pos, mizenkei_end, cost_negative, lemma,
                                           grammar::verbTypeToConjType(verb_type), true, CandidateOrigin::VerbHiragana,
                                           0.9F, "hiragana_mizenkei_negative", core::ExtendedPOS::VerbMizenkei));
    break;  // Only generate one candidate per position
  }
}

// Godan mizenkei before なきゃ/なければ (やらなきゃ → やら + なきゃ). The
// contraction is an unambiguous mizenkei signal, so the candidate gets a bonus.
void appendMizenkeiNakyaCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t hiragana_end,
                                   const grammar::Inflection& inflection,
                                   const dictionary::DictionaryManager* dict_manager,
                                   std::vector<UnknownCandidate>& candidates) {
  // Pattern: A-row hiragana (mizenkei ending) + なきゃ OR + なけれ(ば)
  for (size_t end_pos = hiragana_end; end_pos >= start_pos + 2; --end_pos) {
    // Follow pattern begins at end_pos (the mizenkei is start_pos..end_pos)
    // なきゃ = な + き + ゃ ; なけれ = な + け + れ
    if (end_pos + 3 > codepoints.size() || codepoints[end_pos] != U'な') {
      continue;
    }
    bool is_nakya = (codepoints[end_pos + 1] == U'き' && codepoints[end_pos + 2] == U'ゃ');
    bool is_nakere = (codepoints[end_pos + 1] == U'け' && codepoints[end_pos + 2] == U'れ');
    if (!is_nakya && !is_nakere) {
      continue;
    }

    // Check if position end_pos-1 is A-row hiragana (godan mizenkei ending)
    size_t mizenkei_end = end_pos;
    if (mizenkei_end <= start_pos) {
      continue;
    }
    GodanMizenkeiForms forms;
    if (!deriveGodanMizenkeiForms(codepoints, start_pos, mizenkei_end, forms)) {
      continue;
    }
    grammar::VerbType verb_type = forms.verb_type;
    const std::string& mizenkei_surface = forms.mizenkei_surface;
    const std::string& base_form = forms.base_form;

    // Validate: analyze the equivalent ない form to confirm it is a valid verb.
    // E.g., for やら validate やらない → やる (godan-ra). Dictionary is a fallback.
    const std::string full_form = mizenkei_surface + "ない";
    const MizenkeiEvidence evidence =
        judgeMizenkeiForms(dict_manager, inflection, full_form, forms, codepoints, start_pos, mizenkei_end);
    const bool is_valid_verb = evidence.is_valid_verb;
    const bool is_in_dict = evidence.is_in_dict;
    const bool unattested_sa_irrealis = evidence.unattested_sa_irrealis;
    if (!is_valid_verb) {
      continue;
    }

    // Get lemma from dictionary entry if the mizenkei surface is registered
    std::string lemma = vh::lookupVerbLemma(dict_manager, mizenkei_surface, base_form);

    // The なきゃ/なければ contraction is an unambiguous mizenkei signal, so give a
    // bonus (verified verbs stronger) to beat the particle split や + らなきゃ.
    float cost = is_in_dict ? candidate::verb_cost::kStrongBonus : candidate::verb_cost::kStandardBonus;
    if (unattested_sa_irrealis) {
      cost += candidate::verb_cost::kPureHiraganaSaIrrealisPenalty;
    }
    // Unverified stems starting with a formal noun are noun + verb sequences
    // (わけわから+なきゃ should split as わけ + わから + なきゃ)
    if (!is_in_dict && hasFormalNounPrefixBoundary(dict_manager, codepoints, start_pos, mizenkei_end)) {
      cost += bigram_cost::kStrong;
    }
    SUZUME_DEBUG_VERBOSE_BLOCK {
      SUZUME_DEBUG_STREAM << "[VERB_CAND] " << mizenkei_surface << " hiragana_mizenkei_nakya lemma=" << lemma
                          << " cost=" << cost << "\n";
    }
    candidates.push_back(makeVerbCandidate(mizenkei_surface, start_pos, mizenkei_end, cost, lemma,
                                           grammar::verbTypeToConjType(verb_type), true, CandidateOrigin::VerbHiragana,
                                           0.9F, "hiragana_mizenkei_nakya", core::ExtendedPOS::VerbMizenkei));
    break;  // Only generate one candidate per position
  }
}

// Godan-ra ん音便 + negative ない (たまんない → たまん + ない), where stem + る
// is a godan-ra verb.
void appendNOnbinNaiCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t hiragana_end,
                               const grammar::Inflection& inflection, const dictionary::DictionaryManager* dict_manager,
                               std::vector<UnknownCandidate>& candidates) {
  // The progressive negative also contracts productively: V+ていられない
  // becomes V+て+らん+ない.  The reduced tail is not a lexical adjective;
  // emit the oracle's nominal residual only after the full te+らん+ない frame
  // proves the contraction.
  if (start_pos > 0 && start_pos + 3 < codepoints.size() && codepoints[start_pos - 1] == U'て' &&
      codepoints[start_pos] == U'ら' && codepoints[start_pos + 1] == U'ん' && codepoints[start_pos + 2] == U'な' &&
      codepoints[start_pos + 3] == U'い') {
    candidates.push_back(makeNounCandidate(extractSubstring(codepoints, start_pos, start_pos + 2), start_pos,
                                           start_pos + 2, candidate::verb_cost::kStrongBonus, true,
                                           CandidateOrigin::VerbHiragana, core::ExtendedPOS::Noun,
                                           "contracted_progressive_negative"));
    // The frame has already decided what these two morae are, and the scan
    // below would read the same span as a godan-ra irrealis whose base is the
    // classical passive らる rather than a verb of the modern paradigm it
    // reconstructs.
    return;
  }

  // Pattern: stem + ん + ない where stem + る is a godan-ra verb
  for (size_t n_pos = start_pos + 1; n_pos < hiragana_end; ++n_pos) {
    if (codepoints[n_pos] != U'ん')
      continue;

    // The contraction is licensed by the negative that selects the irrealis,
    // and every cell of that paradigm selects the same one (わかん+ない,
    // やん+なきゃ, やん+なかっ+た). Reading only the dictionary form left the
    // remaining cells to fall back on a particle sequence.
    if (!vh::naiNegativeFollowsAt(codepoints, n_pos + 1))
      continue;

    // Get stem (part before ん) — need at least 1 char
    if (n_pos <= start_pos)
      continue;
    std::string stem = extractSubstring(codepoints, start_pos, n_pos);

    // Construct base form: stem + る (godan-ra)
    std::string base_form = stem + "る";

    // Validate: check if the standard form (stem + らない) is a valid verb
    std::string standard_form = stem + "らない";
    const auto& analysis = inflection.analyze(standard_form);
    bool is_valid_verb = false;
    for (const auto& cand : analysis) {
      if (cand.verb_type == grammar::VerbType::GodanRa && cand.base_form == base_form) {
        is_valid_verb = true;
        break;
      }
    }

    // Also check if base form exists in dictionary
    bool is_in_dict = vh::isVerbInDictionary(dict_manager, base_form);
    if (!is_valid_verb) {
      is_valid_verb = is_in_dict;
    }

    if (!is_valid_verb)
      continue;

    // Surface: stem + ん (the ん音便 form)
    std::string onbin_surface = stem + "ん";
    size_t onbin_end = n_pos + 1;

    // Get lemma from dictionary if available
    std::string standard_mizenkei = stem + "ら";
    std::string lemma = vh::lookupVerbLemma(dict_manager, standard_mizenkei, base_form);

    float cost_n_onbin = candidate::verb_cost::kStandardBonus;
    // Unverified stems starting with a formal noun are noun + verb sequences
    // (わけわかん+ない should split as わけ + わかん + ない)
    if (!is_in_dict && hasFormalNounPrefixBoundary(dict_manager, codepoints, start_pos, onbin_end)) {
      cost_n_onbin += bigram_cost::kStrong;
    }
    SUZUME_DEBUG_VERBOSE_BLOCK {
      SUZUME_DEBUG_STREAM << "[VERB_CAND] " << onbin_surface << " hiragana_n_onbin_nai lemma=" << lemma
                          << " cost=" << cost_n_onbin << "\n";
    }
    candidates.push_back(makeVerbCandidate(onbin_surface, start_pos, onbin_end, cost_n_onbin, lemma,
                                           grammar::verbTypeToConjType(grammar::VerbType::GodanRa), true,
                                           CandidateOrigin::VerbHiragana, 0.9F, "hiragana_n_onbin_nai",
                                           core::ExtendedPOS::VerbMizenkei));
    break;
  }
}

// Godan onbin stems before contraction/tense auxiliaries. Handles
// っ + と/ち/た/て (GodanRa/Ta/Wa) and ん + ど/じ/で/だ (GodanMa/Ba/Na).

}  // namespace suzume::analysis::hiragana_verb_detail
