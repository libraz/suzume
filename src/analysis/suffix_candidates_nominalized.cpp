/**
 * @file suffix_candidates_nominalized.cpp
 * @brief Nominalized noun candidate generation
 */
#include <algorithm>

#include "analysis/dictionary_probe.h"
#include "candidate_constants.h"
#include "core/debug.h"
#include "core/utf8_constants.h"
#include "dictionary/dictionary.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "grammar/inflection.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "tokenizer_utils.h"
#include "unknown.h"
#include "verb_candidates_helpers.h"

namespace suzume::analysis {

namespace {

#ifdef SUZUME_DEBUG_INFO
// Reported confidence shared by the nominalized-noun candidates; debug metadata
// only, it never reaches the lattice score.
constexpr float kNominalizedNounReportedConfidence = 0.6F;
#endif

bool hasNominalizedNounParticleContinuation(const std::vector<char32_t>& codepoints, size_t end_pos,
                                            const dictionary::DictionaryManager* dict_manager) {
  return end_pos < codepoints.size() && normalize::isParticleCodepoint(codepoints[end_pos]) &&
         codepoints[end_pos] != U'て' && codepoints[end_pos] != U'で' &&
         !startsLongerNonParticleEntry(codepoints, end_pos, dict_manager);
}

// The test above sees a particle only when its first mora is itself a particle
// character, which leaves the multi-mora members of the same closed class
// invisible (まで, ほど). Reading them from the dictionary matters where the
// nominalization is productive rather than lexicalized — a two-mora okurigana,
// whose continuative noun has nothing but the following particle to mark its
// position as nominal (暮らし+まで).
bool selectsNominalHostByListedParticle(const std::vector<char32_t>& codepoints, size_t end_pos,
                                        const dictionary::DictionaryManager* dict_manager) {
  return end_pos < codepoints.size() && codepoints[end_pos] != U'て' && codepoints[end_pos] != U'で' &&
         hasNominalForcingParticleContinuation(codepoints, end_pos, dict_manager) &&
         !startsLongerNonParticleEntry(codepoints, end_pos, dict_manager);
}

bool hasInferredVerbContinuative(const grammar::Inflection& inflection, std::string_view surface) {
  const auto& analyses = inflection.analyze(surface);
  return std::any_of(analyses.begin(), analyses.end(), [](const auto& analysis) {
    return analysis.verb_type != grammar::VerbType::Unknown && analysis.verb_type != grammar::VerbType::IAdjective &&
           analysis.suffix.empty();
  });
}

// A clause-final particle selects a completed nominal host just as a case
// particle does (重み|ね). Read the closed class from the dictionary so the
// homographic classical auxiliary remains available after verbal forms.
bool hasClauseFinalParticleContinuation(const std::vector<char32_t>& codepoints,
                                        const std::vector<normalize::CharType>& char_types, size_t end_pos,
                                        const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr || end_pos >= codepoints.size()) {
    return false;
  }
  constexpr size_t kParticleProbe = 3;
  const size_t probe_end = std::min(codepoints.size(), end_pos + kParticleProbe);
  for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, end_pos, probe_end)) {
    if (match.entry == nullptr || match.entry->extended_pos != core::ExtendedPOS::ParticleFinal) {
      continue;
    }
    const size_t after_particle = end_pos + match.length;
    if (after_particle == codepoints.size() ||
        (after_particle < char_types.size() && char_types[after_particle] == normalize::CharType::Symbol)) {
      return true;
    }
  }
  return false;
}

// A short hiragana noun can end in a kana that is homographic with a particle
// (ひも, もの).  When a real nominal particle closes that run, the preceding
// continuative is nominal too: 組み|ひも|を, not the finite 組み|ひも|を reading.
// The whole-compound candidate remains available for lexical units such as
// 食べもの; this only supplies the productive internal noun boundary.
bool hasParticleFinalHiraganaNounContinuation(const std::vector<char32_t>& codepoints,
                                              const std::vector<normalize::CharType>& char_types, size_t start_pos,
                                              const dictionary::DictionaryManager* dict_manager) {
  constexpr size_t kMaximumHiraganaNounLength = 4;
  size_t end_pos = start_pos;
  while (end_pos < char_types.size() && end_pos - start_pos < kMaximumHiraganaNounLength &&
         char_types[end_pos] == normalize::CharType::Hiragana) {
    ++end_pos;
    if (end_pos - start_pos >= 2 && normalize::isParticleCodepoint(codepoints[end_pos - 1]) &&
        hasNominalizedNounParticleContinuation(codepoints, end_pos, dict_manager)) {
      return true;
    }
  }
  return false;
}

// A genitive/adnominal の followed by a continuative-shaped word that closes
// at a clause boundary forms a complete noun phrase (雨の匂い。). Requiring
// both sides keeps attributive predicates such as 父の残した手紙 verbal.
bool isGenitiveClauseFinalNominal(const std::vector<char32_t>& codepoints,
                                  const std::vector<normalize::CharType>& char_types, size_t start_pos, size_t end_pos,
                                  const dictionary::DictionaryManager* dict_manager) {
  const bool closes_clause = end_pos == codepoints.size() ||
                             (end_pos < char_types.size() && char_types[end_pos] == normalize::CharType::Symbol);
  if (start_pos == 0 || !closes_clause) {
    return false;
  }
  if (codepoints[start_pos - 1] == U'の') {
    return true;
  }
  // A determiner is the same kind of left bracket as the genitive: both exist to
  // select a nominal head, and neither can stand in front of a continuative. The
  // longest one bounds the lookbehind (いわゆる, ありとあらゆる).
  constexpr size_t kDeterminerLookbehind = 6;
  if (dict_manager == nullptr) {
    return false;
  }
  const size_t scan_start = start_pos > kDeterminerLookbehind ? start_pos - kDeterminerLookbehind : 0;
  for (size_t determiner_start = scan_start; determiner_start < start_pos; ++determiner_start) {
    if (lookupEntryInRange(*dict_manager, codepoints, determiner_start, start_pos, core::PartOfSpeech::Determiner) !=
        nullptr) {
      return true;
    }
  }
  return false;
}

// Whether the kanji immediately before @p okurigana_pos, taken together with
// the single okurigana there, spells the continuative of a verb the dictionary
// knows. Both live paradigms are inverted by rule rather than listed: a godan
// continuative replaces the dictionary form's u-row mora with the i-row one
// (書き → 書く), and an ichidan continuative is the dictionary form without its
// る (上げ → 上げる, 落ち → 落ちる). Trying both is what lets the same test cover
// an e-row okurigana, which only an ichidan verb can end on.
bool namesDictionaryVerbContinuative(const dictionary::DictionaryManager* dict_manager,
                                     const std::vector<char32_t>& codepoints, size_t okurigana_pos) {
  if (dict_manager == nullptr || okurigana_pos == 0 || okurigana_pos >= codepoints.size()) {
    return false;
  }
  const std::string stem = extractSubstring(codepoints, okurigana_pos - 1, okurigana_pos);
  const std::string_view godan_ending = grammar::godanBaseSuffixFromIRow(codepoints[okurigana_pos]);
  if (!godan_ending.empty() && verb_helpers::isVerbInDictionary(dict_manager, normalize::concat(stem, godan_ending))) {
    return true;
  }
  return verb_helpers::isVerbInDictionary(dict_manager, stem + normalize::encodeUtf8(codepoints[okurigana_pos]) +
                                                            normalize::encodeUtf8(core::hiragana::kRu));
}

// The light verb opens either on its continuative し before an auxiliary of its
// own paradigm, or on its dictionary form. Both cells take a nominal host.
bool startsLightVerb(const std::vector<char32_t>& codepoints, size_t pos) {
  if (pos + 1 >= codepoints.size()) {
    return false;
  }
  if (grammar::isSuruRenyokeiSurface(normalize::encodeUtf8(codepoints[pos]))) {
    return verb_helpers::isSuruAuxiliaryStarter(codepoints[pos + 1]);
  }
  return grammar::isSuruBaseForm(extractSubstring(codepoints, pos, pos + 2));
}

// A deverbal noun and the continuative of the verb it is built on are spelled
// alike, so what stands to the right decides which one the span is. The two
// sets of selectors are disjoint closed classes: a case particle, the copula
// and the light verb する all require a nominal, while the auxiliaries that
// take a continuative (ます, たい, ながら) never follow one. Anything that is
// not hiragana — kanji, katakana, punctuation, end of text — is a nominal
// position as well, since no auxiliary can begin there. The clause comma is the
// exception: it joins predicates rather than closing a phrase, so a continuative
// standing in front of it carries the clause on instead of heading a noun.
bool selectsNominalHost(const dictionary::DictionaryManager* dict_manager, const std::vector<char32_t>& codepoints,
                        const std::vector<normalize::CharType>& char_types, size_t pos) {
  if (pos < codepoints.size() && normalize::isClauseChainingComma(codepoints[pos])) {
    return false;
  }
  if (pos >= char_types.size() || char_types[pos] != normalize::CharType::Hiragana) {
    return true;
  }
  if (normalize::isParticleCodepoint(codepoints[pos])) {
    return true;
  }
  if (dict_manager == nullptr) {
    return false;
  }
  // Read the copula's cells out of its own paradigm instead of listing them.
  const size_t probe_end = std::min(codepoints.size(), pos + 3);
  for (const auto& result : lookupResultsInRange(*dict_manager, codepoints, pos, probe_end)) {
    if (result.entry != nullptr && (result.entry->extended_pos == core::ExtendedPOS::AuxCopulaDa ||
                                    result.entry->extended_pos == core::ExtendedPOS::AuxCopulaDesu)) {
      return true;
    }
  }
  return startsLightVerb(codepoints, pos);
}

// A closed suffix beginning inside a mixed kanji+hiragana span is normally a
// stronger morpheme boundary than the generic nominalization candidate.  The
// exception is a suffix spelling that overlaps the final dictionary-backed
// verb continuative: 越し in 年越し and げ in 値上げ are part of 越す/上げる,
// not suffixes attached to 年/値上.  Other suffixes inside the same span keep
// their boundary.  A lexical noun registered for the whole span also remains
// intact.
bool hasClosedSuffixBoundary(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                             const dictionary::DictionaryManager* dict_manager) {
  if (dict_manager == nullptr) {
    return false;
  }
  if (lookupEntryInRange(*dict_manager, codepoints, start_pos, end_pos, core::PartOfSpeech::Noun) != nullptr) {
    return false;
  }
  const bool ends_in_verb_continuative =
      end_pos > start_pos + 1 && namesDictionaryVerbContinuative(dict_manager, codepoints, end_pos - 1);
  for (size_t split = start_pos + 1; split < end_pos; ++split) {
    const std::string suffix = extractSubstring(codepoints, split, end_pos);
    if (dict_manager->lookupExact(suffix, core::PartOfSpeech::Suffix) != nullptr) {
      const bool overlaps_final_verb_continuative = ends_in_verb_continuative && split + 2 >= end_pos;
      if (overlaps_final_verb_continuative) {
        continue;
      }
      return true;
    }
    // A classical auxiliary is a complete closed-class morpheme and cannot be
    // swallowed by a generated noun (確認+けり).  The conjectural らし needs a
    // narrower condition because it is also the continuative ending in
    // productive nouns such as 山+暮らし+を.
    const auto* auxiliary = dict_manager->lookupExact(suffix, core::PartOfSpeech::Auxiliary);
    const bool overlaps_final_verb_continuative = ends_in_verb_continuative && split + 2 >= end_pos;
    if (auxiliary != nullptr && core::isClassicalAuxiliaryType(auxiliary->extended_pos) &&
        !overlaps_final_verb_continuative) {
      return true;
    }
    if (auxiliary != nullptr && auxiliary->extended_pos == core::ExtendedPOS::AuxConjectureRashii) {
      const bool rashii_paradigm_continues =
          end_pos < codepoints.size() &&
          (codepoints[end_pos] == U'い' || codepoints[end_pos] == U'さ' || codepoints[end_pos] == U'く' ||
           codepoints[end_pos] == U'か' || codepoints[end_pos] == U'け');
      if (rashii_paradigm_continues) {
        return true;
      }
    }
  }
  return false;
}

bool hasPeriodEndNominalBoundary(const std::vector<char32_t>& codepoints, size_t start_pos, size_t kanji_end,
                                 const dictionary::DictionaryManager* dict_manager) {
  constexpr size_t kMinimumKanjiCount = 3;
  if (kanji_end - start_pos < kMinimumKanjiCount || codepoints[kanji_end - 2] != U'末') {
    return false;
  }
  const size_t candidate_end = kanji_end + 1;
  if (candidate_end > codepoints.size()) {
    return false;
  }
  return dict_manager == nullptr ||
         lookupEntryInRange(*dict_manager, codepoints, start_pos, candidate_end, core::PartOfSpeech::Noun) == nullptr;
}

}  // namespace

void generateNominalizedNounCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                       const std::vector<normalize::CharType>& char_types,
                                       const grammar::Inflection& inflection,
                                       const dictionary::DictionaryManager* dict_manager,
                                       std::vector<UnknownCandidate>& candidates) {
  if (start_pos >= char_types.size() || char_types[start_pos] != normalize::CharType::Kanji) {
    return;
  }

  // Find kanji portion (typically 1-3 characters for nominalized nouns)
  size_t kanji_end = findCharRegionEnd(char_types, start_pos, 4, normalize::CharType::Kanji);

  // Need at least 1 kanji
  if (kanji_end == start_pos) {
    return;
  }

  // Look for 1-2 hiragana after kanji (nominalization endings)
  if (kanji_end >= char_types.size() || char_types[kanji_end] != normalize::CharType::Hiragana) {
    return;
  }

  char32_t first_hiragana = codepoints[kanji_end];

  // Skip particles that never form nominalizations
  if (normalize::isParticleCodepoint(first_hiragana)) {
    return;
  }

  // Common nominalization endings (renyokei stems)
  bool is_nominalization_ending =
      (first_hiragana == U'け' || first_hiragana == U'げ' || first_hiragana == U'せ' || first_hiragana == U'い' ||
       first_hiragana == U'り' || first_hiragana == U'ら' || first_hiragana == U'ち' || first_hiragana == U'き' ||
       first_hiragana == U'ぎ' || first_hiragana == U'し' || first_hiragana == U'ま' || first_hiragana == U'み' ||
       first_hiragana == U'び' || first_hiragana == U'え' || first_hiragana == U'れ' || first_hiragana == U'め');

  if (!is_nominalization_ending) {
    return;
  }

  // Do not turn the first mora of a dictionary particle into a nominalized
  // noun. The particle candidate owns the whole span (最後|まで, not 最後ま|で).
  bool begins_particle = false;
  if (dict_manager != nullptr) {
    size_t probe_end = std::min(codepoints.size(), kanji_end + static_cast<size_t>(4));
    for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, kanji_end, probe_end)) {
      if (match.entry != nullptr && match.entry->pos == core::PartOfSpeech::Particle &&
          normalize::utf8Length(match.entry->surface) > 1) {
        begins_particle = true;
        break;
      }
    }
  }
  if (begins_particle) {
    return;
  }

  // Skip potential suru-verb patterns: 漢字2字+し followed by suru-auxiliary
  // e.g., 勉強しちゃった → 勉強 + し + ちゃっ + た (not 勉強し + ちゃった)
  size_t kanji_count = kanji_end - start_pos;
  // 暮らし is a productive continuative-form nominal head.  Preceding kanji
  // form its modifier rather than an opaque compound (山+暮らし, 田舎+暮らし),
  // so only the final kanji owns this nominalization candidate.  A lexicalized
  // compound such as 一人暮らし remains available through its dictionary entry.
  if (first_hiragana == U'ら' && kanji_count > 1 && kanji_end + 1 < codepoints.size() &&
      codepoints[kanji_end + 1] == U'し') {
    return;
  }
  // When a multi-kanji nominal prefix precedes a longer verified verb
  // continuative, this start position owns only the prefix. Do not emit a
  // nominalized candidate spanning the right-hand verb (総合|見直し,
  // 長期|借入れ). A one-kanji host remains a productive compound search unit
  // (顔見知り), while candidate generation at the verified start still emits
  // the standalone continuative.
  const size_t following_verb_start =
      longestNominalVerbContinuativeStart(codepoints, char_types, start_pos, kanji_end, inflection, dict_manager);
  if (following_verb_start > start_pos + 1 && following_verb_start < kanji_end) {
    return;
  }
  // For sahen-compatible 2+ kanji nouns, せ is mizenkei (勉強せよ), not a
  // nominalization ending. Skip nominalized noun candidate here so the
  // 勉強+せよ dictionary path can win.
  if (first_hiragana == U'せ' && kanji_count >= 2) {
    size_t next_pos = kanji_end + 1;
    if (next_pos < codepoints.size()) {
      char32_t next_char = codepoints[next_pos];
      // せ followed by imperative よ, passive ら/れ, causative ら, etc.
      if (next_char == U'よ' || next_char == U'ら' || next_char == U'れ' || next_char == U'ず') {
        return;
      }
    }
  }
  if (first_hiragana == U'し' && kanji_count >= 2) {
    // Check for suru-auxiliary patterns following し
    size_t next_pos = kanji_end + 1;
    if (next_pos < codepoints.size()) {
      char32_t next_char = codepoints[next_pos];
      if (verb_helpers::isSuruAuxiliaryStarter(next_char)) {
        // This looks like a suru-verb pattern - skip nominalization
        return;
      }
      // Kanji after し indicates suru-verb renyoukei + kanji verb/noun
      // e.g., 解決し得ない → 解決+し+得+ない (not 解決し+得ない)
      if (next_pos < char_types.size() && char_types[next_pos] == normalize::CharType::Kanji) {
        return;
      }
    }
  }
  // A kanji+し token that is NOT a genuine deverbal noun (last kanji + す ∉ dict)
  // is a sahen renyokei that must split off, not a nominalized noun. Apply this
  // only to a multi-kanji stem (遅刻し→遅刻+し, 遅刻す∉dict) or a fragment starting
  // mid kanji-run (刻し inside 遅刻し, 刻す∉dict); a standalone single kanji + し is
  // left alone so the classical adjective-stem nominalization stays a noun
  // (寒し, 美し — both 寒す/美す ∉ dict). Deverbal compounds keep the noun reading
  // regardless of position (丸出し→出す, 手渡し→渡す, 年越し→越す, 話し→話す).
  const bool preceded_by_kanji = start_pos > 0 && char_types[start_pos - 1] == normalize::CharType::Kanji;
  if (first_hiragana == U'し' && dict_manager != nullptr && (kanji_count >= 2 || preceded_by_kanji)) {
    std::string_view base_ending = grammar::godanBaseSuffixFromIRow(first_hiragana);
    std::string verb_base = normalize::concat(normalize::encodeUtf8(codepoints[kanji_end - 1]), base_ending);
    if (!verb_helpers::isVerbInDictionary(dict_manager, verb_base)) {
      return;
    }
  }

  // Check for 1 or 2 hiragana (e.g., け or 上げ)
  size_t hiragana_end = kanji_end + 1;

  // Check for 2-hiragana patterns if second char is also valid
  if (hiragana_end < char_types.size() && char_types[hiragana_end] == normalize::CharType::Hiragana) {
    char32_t second_hiragana = codepoints[hiragana_end];
    const bool second_starts_classical_conjectural_auxiliary =
        grammar::startsClassicalConjecturalAuxiliary(extractSubstring(codepoints, hiragana_end, codepoints.size()));
    // Common 2-char nominalization endings
    // Note: い is excluded — kanji+2hira ending in い is overwhelmingly
    // i-adjective (美しい, 正しい, 激しい), not nominalized noun
    if (second_hiragana == U'げ' || second_hiragana == U'け' || second_hiragana == U'り' || second_hiragana == U'え' ||
        second_hiragana == U'し' || second_hiragana == U'み') {
      // Trailing し followed by a suru-auxiliary (or kanji) is ichidan renyokei
      // + する (お伝えします, お届けして), not a nominalization — skip the noun
      // so the verb split can win. し at end of text or before a particle keeps
      // the noun candidate (genuine nominalizations survive).
      const std::string surface = extractSubstring(codepoints, start_pos, hiragana_end + 1);
      const bool has_particle_continuation =
          hasNominalizedNounParticleContinuation(codepoints, hiragana_end + 1, dict_manager) ||
          selectsNominalHostByListedParticle(codepoints, hiragana_end + 1, dict_manager);
      bool trailing_shi_is_suru = false;
      // The auxiliary is recognized by its opening mora, which several
      // multi-mora particles share (まで opens like ます, から like かける). A
      // nominal-forcing particle actually listed at that position is the
      // stronger evidence and settles the position as nominal (暮らし+まで).
      if (second_hiragana == U'し' && !has_particle_continuation) {
        size_t after_shi_pos = hiragana_end + 1;
        if (after_shi_pos < codepoints.size()) {
          char32_t after_shi = codepoints[after_shi_pos];
          if (verb_helpers::isSuruAuxiliaryStarter(after_shi) ||
              (after_shi_pos < char_types.size() && char_types[after_shi_pos] == normalize::CharType::Kanji)) {
            trailing_shi_is_suru = true;
          }
        }
      }
      const bool selects_nominal_host = selectsNominalHost(dict_manager, codepoints, char_types, hiragana_end + 1);
      const bool has_explicit_nominal_selector =
          has_particle_continuation ||
          (hiragana_end + 1 < char_types.size() && char_types[hiragana_end + 1] == normalize::CharType::Hiragana &&
           selects_nominal_host);
      const bool has_inferred_verb_continuative = hasInferredVerbContinuative(inflection, surface);
      // A single-kanji stem followed by two hiragana can be a productive
      // deverbal noun when a particle, copula, or light verb explicitly
      // selects a nominal host. In that frame, a homographic closed suffix
      // cannot erase the complete continuative.
      const bool crosses_closed_suffix =
          hasClosedSuffixBoundary(codepoints, start_pos, hiragana_end + 1, dict_manager) &&
          !(kanji_count == 1 && has_inferred_verb_continuative && has_explicit_nominal_selector);
      if (!trailing_shi_is_suru && !second_starts_classical_conjectural_auxiliary && !crosses_closed_suffix) {
        if (!surface.empty()) {
          float nom2_cost = 0.8F;
          if (has_particle_continuation || selects_nominal_host ||
              isGenitiveClauseFinalNominal(codepoints, char_types, start_pos, hiragana_end + 1, dict_manager)) {
            nom2_cost += candidate::kNominalizedNounParticleBonus;
          }
          auto cand =
              makeCandidate(surface, start_pos, hiragana_end + 1, core::PartOfSpeech::Noun, nom2_cost,
                            has_particle_continuation || selects_nominal_host, CandidateOrigin::NominalizedNoun);
#ifdef SUZUME_DEBUG_INFO
          cand.confidence = 0.8F;
          cand.pattern = "nominalized_2hira";
#endif
          candidates.push_back(cand);
        }
      }
    }
  }

  // Generate 1-hiragana candidate
  bool skip_single_char =
      grammar::startsClassicalConjecturalAuxiliary(extractSubstring(codepoints, kanji_end + 1, codepoints.size()));
  if (kanji_end + 1 < char_types.size() && char_types[kanji_end + 1] == normalize::CharType::Hiragana) {
    char32_t next_char = codepoints[kanji_end + 1];
    if (next_char == U'な') {
      skip_single_char = true;
    }
  }
  // Skip kanji+い when kanji ends with 的 (teki na-adjective suffix)
  // 理性的い, 経済的い don't make sense — 的 forms na-adjectives, not i-adjectives
  if (first_hiragana == U'い' && kanji_end > start_pos) {
    char32_t last_kanji = codepoints[kanji_end - 1];
    if (last_kanji == U'的') {
      skip_single_char = true;
    }
  }
  // Skip kanji+い followed by た/て: this い is godan-ka i-onbin forming a
  // past/te-form verb (続いた, 書いて), not a nominalized renyokei. True
  // nominalized nouns (間違い, 度合い) never take past た directly.
  // だ/で are intentionally excluded: copula after a real nominalization
  // (度合いだ) must keep the noun candidate, and godan-ga onbin (泳いだ)
  // is on the だ/で side as well.
  if (first_hiragana == U'い' && kanji_end + 1 < codepoints.size()) {
    char32_t after_i = codepoints[kanji_end + 1];
    if (after_i == U'た' || after_i == U'て') {
      skip_single_char = true;
    }
  }
  // A dictionary i-adjective (甘い、辛い) is not a deverbal noun merely
  // because its final mora is also an i-row renyokei ending.
  if (first_hiragana == U'い' && dict_manager != nullptr) {
    if (lookupEntryInRange(*dict_manager, codepoints, start_pos, kanji_end + 1, core::PartOfSpeech::Adjective) !=
        nullptr) {
      skip_single_char = true;
    }
  }

  // A deverbal noun is built on the continuative stem (読み, 調べ), never on
  // the irrealis. An a-row tail is therefore a conjugational boundary rather
  // than a nominalization, and fabricating a noun across it swallows the verb
  // together with its host (水飲ま+ね instead of 水/飲ま/ね). Fossilized a-row
  // nominals are not deverbal at all and are carried by their own entries
  // (自ら, 半ば), so the dictionary reading still wins where one exists.
  if (grammar::isARowCodepoint(first_hiragana)) {
    if (dict_manager == nullptr ||
        lookupEntryInRange(*dict_manager, codepoints, start_pos, kanji_end + 1, core::PartOfSpeech::Noun) == nullptr) {
      skip_single_char = true;
    }
  }

  // A temporal boundary noun ending in 末 is complete before a following
  // continuative (月末|締め, 週末|届け). The generic nominalizer otherwise
  // fabricates one unknown compound over both constituents. Preserve an exact
  // L2 noun for genuinely lexicalized compounds.
  if (hasPeriodEndNominalBoundary(codepoints, start_pos, kanji_end, dict_manager)) {
    skip_single_char = true;
  }

  // A long kanji sequence ending in an attested godan stem normally contains a
  // nominal boundary (東京+行き, 翌月+払い, ご確認+願い), rather than one unknown
  // nominalization. The boundary shows up both before a particle and before a
  // verbal continuation, so the trailing hiragana carries the verb: 確認願います
  // is 確認 + 願い + ます, never a 確認願い noun. Copula だ and a non-hiragana or
  // final position leave the nominal reading intact (翌月払いだ). Two-kanji
  // deverbal compounds are handled by the verified compound path below.
  if (kanji_count >= 3 && following_verb_start != start_pos + 1 && dict_manager != nullptr &&
      kanji_end + 1 < codepoints.size() && char_types[kanji_end + 1] == normalize::CharType::Hiragana &&
      codepoints[kanji_end + 1] != U'だ') {
    const std::string_view base_ending = grammar::godanBaseSuffixFromIRow(first_hiragana);
    if (!base_ending.empty()) {
      const std::string verb_base = normalize::concat(normalize::encodeUtf8(codepoints[kanji_end - 1]), base_ending);
      if (verb_helpers::isVerbInDictionary(dict_manager, verb_base)) {
        skip_single_char = true;
      }
    }
  }

  if (!skip_single_char) {
    std::string surface = extractSubstring(codepoints, start_pos, kanji_end + 1);
    if (!surface.empty()) {
      // Scale cost higher for long kanji sequences to prevent absorbing
      // following tokens (e.g., 触手画像み should not beat 触手画像+みんな)
      float nom1_cost = 1.2F;
      if (kanji_count >= 3) {
        nom1_cost += static_cast<float>(kanji_count - 2) * 0.5F;
      }
      const float base_nom1_cost = nom1_cost;
      // A following particle makes the renyokei a nominalized search unit:
      // 答えは, 始まりは, 決まりを.  Prefer that productive noun reading over
      // a finite-verb candidate whose continuation is grammatically absent.
      const bool has_particle_continuation =
          hasNominalizedNounParticleContinuation(codepoints, kanji_end + 1, dict_manager);
      const bool has_final_particle_continuation =
          first_hiragana == U'み' &&
          hasClauseFinalParticleContinuation(codepoints, char_types, kanji_end + 1, dict_manager);
      const bool has_hiragana_noun_continuation =
          namesDictionaryVerbContinuative(dict_manager, codepoints, kanji_end) &&
          hasParticleFinalHiraganaNounContinuation(codepoints, char_types, kanji_end + 1, dict_manager);
      // The honorific construction お+連用形名詞+いただく keeps the deverbal
      // search unit intact (お目通し+いただければ).  Its prefix and receptive
      // auxiliary are structural evidence that the preceding kanji-plus-し
      // span is nominalized; without it the shorter kanji noun plus a
      // fabricated し(する) can win solely on connection cost.
      const bool has_humble_auxiliary_continuation =
          start_pos > 0 && grammar::isHonorificPrefix(extractSubstring(codepoints, start_pos - 1, start_pos)) &&
          verb_helpers::itadakuParadigmStartsAt(codepoints, kanji_end + 1);
      const bool has_temporal_nominal_continuation =
          grammar::startsClosedTemporalNominal(extractSubstring(codepoints, kanji_end + 1, codepoints.size()));
      if (has_particle_continuation || has_final_particle_continuation ||
          isGenitiveClauseFinalNominal(codepoints, char_types, start_pos, kanji_end + 1, dict_manager) ||
          has_temporal_nominal_continuation || has_hiragana_noun_continuation || has_humble_auxiliary_continuation) {
        nom1_cost += candidate::kNominalizedNounParticleBonus;
      }
      // Deverbal compound noun (連用形転成名詞の複合). The longest verified
      // continuative owns either the whole candidate (見直し, 借入れ) or all
      // but one host kanji (顔見知り, 手書き). A longer nominal prefix keeps its
      // boundary and returned above (総合|見直し, 翌月|払い).
      const bool is_deverbal_compound =
          (kanji_count == 2 && namesDictionaryVerbContinuative(dict_manager, codepoints, kanji_end)) ||
          (kanji_count >= 3 && (following_verb_start == start_pos + 1 || following_verb_start == start_pos));
      // The compound and the [noun] + [continuative] split of the same run are
      // told apart by what selects them, so the reading holds wherever a
      // nominal is selected — before a particle, the copula, or the light verb
      // (手書きの / 手書きだ / 手書きした) — and yields to a continuation that
      // requires a continuative (ながら, ます, たい).
      const bool nominal_compound =
          is_deverbal_compound && selectsNominalHost(dict_manager, codepoints, char_types, kanji_end + 1);
      if (nominal_compound) {
        nom1_cost += candidate::kDeverbalCompoundNounBonus;
      }
      // A one-kanji i-adjective may use the classical terminal -し form at
      // the end of a predicate. Keep that attested terminal form as one
      // lexical unit instead of reanalyzing its final し as a suru stem.
      const bool is_classical_iadjective_terminal =
          kanji_count == 1 && first_hiragana == U'し' && kanji_end + 1 == codepoints.size() &&
          verb_helpers::isAdjectiveInDictionary(dict_manager,
                                                extractSubstring(codepoints, start_pos, kanji_end) + "い");
      if (is_classical_iadjective_terminal) {
        nom1_cost += candidate::kClassicalIAdjectiveTerminalNounBonus;
      }
      // Each bonus above is evidence from the frame that the span is nominal.
      // With none of them a multi-kanji candidate is only a guess about an
      // open-class word, while the same span also reads as a noun heading a
      // continuative the grammar derives from a verb it knows (水|流れ). Price
      // the guess above that split so a fabricated compound cannot undercut its
      // own constituents.
      const bool has_nominal_evidence = nom1_cost < base_nom1_cost || has_particle_continuation || nominal_compound ||
                                        is_classical_iadjective_terminal;
      if (!has_nominal_evidence && kanji_count >= 2) {
        nom1_cost += candidate::kUnselectedNominalizationPenalty;
      }
      // Single kanji + し followed by sentence punctuation (、。) is almost
      // always 一字漢語サ変動詞 renyokei in formal/literary text (呈し、訴し、),
      // not a nominalized noun. Skip to let the VERB candidate win.
      bool skip_nom_single_kanji_shi = false;
      if (kanji_count == 1 && first_hiragana == U'し' && kanji_end + 1 < codepoints.size()) {
        char32_t after = codepoints[kanji_end + 1];
        if (after == U'、' || after == U'。') {
          skip_nom_single_kanji_shi = true;
        }
      }
      // A nominalization ends on a verb's continuative, so a run whose last
      // kanji plus this okurigana is a dictionary i-adjective ends on a
      // predicate instead: the kanji before it is a separate noun (本|重い,
      // 頭|痛い). One-kanji runs are exempt — the adjective itself is the whole
      // span there, and the classical terminal handled above needs its candidate.
      const bool ends_on_dictionary_adjective =
          kanji_count >= 2 && verb_helpers::isAdjectiveInDictionary(
                                  dict_manager, extractSubstring(codepoints, kanji_end - 1, kanji_end + 1));
      // The end of the input selects a nominal too. A bare continuative is not a
      // finite form, so it cannot close a sentence on its own — the 連用中止 use
      // hands the clause on and shows up before a comma, never at the end (似た
      // 輝き, 優れた働き). Without this the deverbal reading has no candidate at
      // all wherever the frame is a clause end rather than a particle.
      const bool has_explicit_nominal_selector =
          has_particle_continuation || kanji_end + 1 == codepoints.size() ||
          (kanji_end + 1 < char_types.size() && char_types[kanji_end + 1] == normalize::CharType::Hiragana &&
           selectsNominalHost(dict_manager, codepoints, char_types, kanji_end + 1));
      // A particle-selected continuative compound is a complete nominal even
      // when its final mora is homographic with a classical auxiliary. For
      // example, 山登りを has productive verb-shape evidence plus a case
      // particle; treating its final り as an auxiliary would erase the
      // compound search unit. An unselected literary chain still keeps the
      // closed-class boundary.
      const bool crosses_closed_suffix =
          hasClosedSuffixBoundary(codepoints, start_pos, kanji_end + 1, dict_manager) &&
          !(hasInferredVerbContinuative(inflection, surface) && has_explicit_nominal_selector);
      if (!skip_nom_single_kanji_shi && !crosses_closed_suffix && !ends_on_dictionary_adjective) {
        // The verified compound is a construction the grammar recognizes, not
        // an opaque run, so it is exempt from the dictionary-length penalty the
        // same way the particle-selected nominalization is. Without the
        // exemption a compound whose first kanji happens to be a registered
        // noun loses to its own split while an unregistered one does not
        // (手書き against 下書き).
        auto cand = makeCandidate(surface, start_pos, kanji_end + 1, core::PartOfSpeech::Noun, nom1_cost,
                                  has_particle_continuation || has_final_particle_continuation || nominal_compound ||
                                      has_hiragana_noun_continuation,
                                  CandidateOrigin::NominalizedNoun);
#ifdef SUZUME_DEBUG_INFO
        cand.confidence = kNominalizedNounReportedConfidence;
        cand.pattern = "nominalized_1hira";
#endif
        candidates.push_back(cand);
      }
    }
  }

  // Deverbal compound noun: a continuative verb form written as kanji plus one
  // okurigana, followed by a single-kanji noun, is a productive nominal compound
  // (笑い声, 泣き声, 立ち姿, 聞き耳, 呼び声).  The continuative must reconstruct a
  // dictionary verb, the noun must be a kanji run of exactly one character so a
  // longer kanji compound keeps its own left boundary (読み文章 stays split), and
  // the span must close in a nominal frame — a following predicate suffix means
  // the second kanji heads a verb rather than closing a noun.
  if (kanji_count == 1 && dict_manager != nullptr && kanji_end + 2 <= char_types.size() &&
      char_types[kanji_end + 1] == normalize::CharType::Kanji &&
      (kanji_end + 2 == char_types.size() || char_types[kanji_end + 2] != normalize::CharType::Kanji)) {
    // The continuative shape alone is not enough: 高い山 and 及び水 have it too,
    // and only an attested verb base separates them from a real compound. The
    // reconstructed base therefore has to be a known verb, on either the godan
    // (i-row okurigana) or the ichidan (whole stem + る) paradigm.
    const std::string stem = extractSubstring(codepoints, start_pos, kanji_end + 1);
    const std::string_view base_ending = grammar::godanBaseSuffixFromIRow(first_hiragana);
    const bool is_verb_continuative =
        (!base_ending.empty() &&
         verb_helpers::isVerbInDictionary(
             dict_manager, normalize::concat(normalize::encodeUtf8(codepoints[kanji_end - 1]), base_ending))) ||
        (grammar::isERowCodepoint(first_hiragana) && verb_helpers::isVerbInDictionary(dict_manager, stem + "る"));
    // A closed suffix on the right is its own morpheme (書き|先, 崩し|的), so it
    // never becomes the second half of a lexical compound.
    const bool crosses_suffix = hasClosedSuffixBoundary(codepoints, start_pos, kanji_end + 2, dict_manager);
    // A span that is already lexicalized as a closed-class word keeps that
    // reading even when its shape also parses as a continuative (及び, 従って).
    bool stem_is_closed_class = false;
    for (const auto& match : dict_manager->lookup(stem, 0)) {
      if (match.entry != nullptr && match.entry->surface.size() == stem.size() &&
          match.entry->pos != core::PartOfSpeech::Verb && match.entry->pos != core::PartOfSpeech::Noun) {
        stem_is_closed_class = true;
        break;
      }
    }
    bool nominal_context = true;
    if (kanji_end + 2 < char_types.size() && char_types[kanji_end + 2] == normalize::CharType::Hiragana) {
      const char32_t after = codepoints[kanji_end + 2];
      // The existential negative ない takes a nominal subject, so it closes the
      // compound exactly as a particle does (申し分ない, 差し支えない). Its own
      // i-adjective inflection is what identifies it: a continuative followed by
      // a verb okurigana never reaches な here, because a negated compound verb
      // carries its mizenkei vowel on that syllable instead (受け取ら|ない).
      const bool heads_existential_negative = after == U'な' && kanji_end + 3 < codepoints.size() && [&] {
        switch (codepoints[kanji_end + 3]) {
          case U'い':
          case U'く':
          case U'か':
          case U'け':
          case U'さ':
            return true;
          default:
            return false;
        }
      }();
      nominal_context = normalize::isParticleCodepoint(after) || after == U'だ' || heads_existential_negative;
    }
    // A lexical entry reaching past the compound owns the span: the second kanji
    // is then the head of a compound verb, not a noun (取り逃がす, not 取り逃+が+す).
    bool longer_entry_starts_here = false;
    {
      constexpr size_t kLexicalProbe = 6;
      const size_t probe_end = std::min(codepoints.size(), start_pos + kLexicalProbe);
      for (const auto& match : lookupResultsInRange(*dict_manager, codepoints, start_pos, probe_end)) {
        if (match.entry != nullptr && normalize::utf8Length(match.entry->surface) > kanji_end + 2 - start_pos) {
          longer_entry_starts_here = true;
          break;
        }
      }
    }
    if (is_verb_continuative && nominal_context && !crosses_suffix && !stem_is_closed_class &&
        !longer_entry_starts_here) {
      auto cand = makeCandidate(extractSubstring(codepoints, start_pos, kanji_end + 2), start_pos, kanji_end + 2,
                                core::PartOfSpeech::Noun, candidate::kDeverbalCompoundNounCost, true,
                                CandidateOrigin::NominalizedNoun);
#ifdef SUZUME_DEBUG_INFO
      cand.confidence = kNominalizedNounReportedConfidence;
      cand.pattern = "deverbal_compound_noun";
#endif
      candidates.push_back(cand);
    }
  }

  return;
}

void generateReciprocalActionNounCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                            const std::vector<normalize::CharType>& char_types,
                                            const dictionary::DictionaryManager* dict_manager,
                                            std::vector<UnknownCandidate>& candidates) {
  if (dict_manager == nullptr || start_pos >= char_types.size() ||
      char_types[start_pos] != normalize::CharType::Hiragana) {
    return;
  }
  // The nominalizer needs a continuative long enough to be one on its own: a
  // one-mora head would turn the opening kana of a lexical word into a stem.
  constexpr size_t kMinStemLength = 2;
  // Long enough for a compound continuative (追いかけっこ) without running past
  // the construction.
  constexpr size_t kMaxStemLength = 4;
  const size_t max_stem_end = std::min(codepoints.size(), start_pos + kMaxStemLength);
  for (size_t stem_end = start_pos + kMinStemLength; stem_end <= max_stem_end; ++stem_end) {
    const size_t end_pos = stem_end + 2;
    if (end_pos > codepoints.size() || codepoints[stem_end] != core::hiragana::kSmallTsu ||
        codepoints[stem_end + 1] != U'こ') {
      continue;
    }
    if (!hasExactPartOfSpeech(*dict_manager, extractSubstring(codepoints, start_pos, stem_end),
                              partOfSpeechMask(core::PartOfSpeech::Verb))) {
      continue;
    }
    std::string surface = extractSubstring(codepoints, start_pos, end_pos);
    auto cand = makeCandidate(surface, start_pos, end_pos, core::PartOfSpeech::Noun,
                              candidate::kReciprocalActionNounCost, false, CandidateOrigin::NominalizedNoun);
    cand.lemma = surface;
#ifdef SUZUME_DEBUG_INFO
    cand.pattern = "reciprocal_action_kko";
#endif
    candidates.push_back(cand);
  }
}

void generateHumbleNominalCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                     const grammar::Inflection& inflection,
                                     const dictionary::DictionaryManager* /*dict_manager*/,
                                     std::vector<UnknownCandidate>& candidates) {
  if (start_pos == 0 || start_pos >= codepoints.size()) {
    return;
  }
  if (!grammar::isHonorificPrefix(extractSubstring(codepoints, start_pos - 1, start_pos))) {
    return;
  }
  // A one-mora stem carries no continuative evidence of its own and would turn
  // the opening kana of a lexical word into a nominal (おいしい).
  constexpr size_t kMinStemLength = 2;
  // Long enough for a compound continuative (問い合わせ) without scanning past
  // the frame.
  constexpr size_t kMaxStemLength = 5;
  const size_t max_end = std::min(codepoints.size(), start_pos + kMaxStemLength);
  for (size_t end_pos = start_pos + kMinStemLength; end_pos <= max_end; ++end_pos) {
    if (end_pos >= codepoints.size()) {
      break;
    }
    // する closes the frame in its dictionary form or through its continuative
    // し, which carries the polite and past chains (お伝えします, おかけした).
    const char32_t suru_head = codepoints[end_pos];
    const bool closes_frame = suru_head == U'し' || (suru_head == U'す' && end_pos + 1 < codepoints.size() &&
                                                     codepoints[end_pos + 1] == U'る');
    if (!closes_frame) {
      continue;
    }
    // A kanji-headed continuative already carries its own deverbal-noun and
    // verbal candidates inside this frame, so only the pure-hiragana stem is
    // left with nothing to oppose the fabricated verb.
    const std::string stem = extractSubstring(codepoints, start_pos, end_pos);
    if (!grammar::isPureHiragana(stem)) {
      continue;
    }
    // A continuative ends in an e-row or i-row mora. Without this the a-row
    // tail of an ordinary lexical word (おこがましい, ございました) reads as a
    // stem, because the inflection analyzer reconstructs a nominal ichidan
    // paradigm for any kana run at its floor confidence.
    const char32_t stem_end = codepoints[end_pos - 1];
    if (!grammar::isERowCodepoint(stem_end) && !grammar::isIRowCodepoint(stem_end)) {
      continue;
    }
    const auto& stem_analysis = inflection.analyze(stem);
    const std::string ichidan_base = stem + "る";
    const grammar::VerbType godan_type = grammar::verbTypeFromIRowCodepoint(codepoints[end_pos - 1]);
    std::string godan_base;
    if (godan_type != grammar::VerbType::Unknown) {
      godan_base = extractSubstring(codepoints, start_pos, end_pos - 1) +
                   std::string(grammar::godanBaseSuffixFromIRow(codepoints[end_pos - 1]));
    }
    bool is_continuative = false;
    for (const auto& cand : stem_analysis) {
      if (cand.confidence <= candidate::kHumbleNominalStemMinConfidence) {
        continue;
      }
      if ((cand.verb_type == grammar::VerbType::Ichidan && cand.base_form == ichidan_base) ||
          (cand.verb_type == godan_type && !godan_base.empty() && cand.base_form == godan_base)) {
        is_continuative = true;
        break;
      }
    }
    if (!is_continuative) {
      continue;
    }
    auto cand = makeCandidate(stem, start_pos, end_pos, core::PartOfSpeech::Noun,
                              candidate::kHumbleNominalCandidateBonus, true, CandidateOrigin::NominalizedNoun);
#ifdef SUZUME_DEBUG_INFO
    cand.pattern = "humble_nominal";
#endif
    candidates.push_back(cand);
  }
}

}  // namespace suzume::analysis
