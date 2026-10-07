/**
 * @file tokenizer_dictionary_contextual.cpp
 * @brief Context-licensed closed-class edges added before the dictionary lookup results
 */

#include <array>

#include "analysis/category_cost.h"
#include "analysis/tokenizer_utils.h"
#include "candidate_constants.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "join_candidates.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "tokenizer_dictionary_internal.h"
#include "verb_candidates_helpers.h"

namespace suzume::analysis::tokenizer_dictionary_detail {

namespace {

// The passive される may follow a productive Sahen nominal, but an arbitrary
// one-kanji unknown noun is not enough evidence for that omitted する. A
// dictionary noun can establish the lexical exception (愛+さ+れる), while an
// unregistered multi-kanji Sino-Japanese run remains a productive Sahen host
// (反映+さ+れる). This leaves a one-kanji verb's own irrealis candidate to
// own the boundary in 許さ+れる.
bool hasPrecedingSahenNominal(const core::Lattice& lattice, size_t end_pos) {
  return core::anyEdgeEndingAt(lattice, end_pos, [](const core::LatticeEdge& edge) {
    // Sahen is productive over both nominal scripts: a kanji compound and a
    // loanword take する alike (実施する, キャンセルする), so an unregistered
    // katakana run heads the construction just as an unregistered kanji run
    // does. Requiring kanji made the passive boundary depend on the host's
    // script rather than on its class.
    constexpr size_t kMinSahenNominalLength = 2;
    return edge.pos == core::PartOfSpeech::Noun &&
           (edge.fromDictionary() || ((grammar::isAllKanji(edge.surface) || normalize::isAllKatakana(edge.surface)) &&
                                      normalize::utf8Length(edge.surface) >= kMinSahenNominalLength));
  });
}

// An indefinite か can close a short interrogative nominal phrase rather than
// only an immediately preceding pronoun: いつ+の+間+に+か, 誰+に+か. Walk
// backward over lattice edges that can stay inside such a phrase. The bounded
// reverse index keeps this proportional to the local candidate count.
bool hasInterrogativeNominalPhraseEndingAt(const core::Lattice& lattice, size_t end_pos) {
  const size_t scan_start = dictionaryLookbehindStart(end_pos);
  std::array<bool, kDictionaryLookbehindChars + 1> reachable{};
  reachable[end_pos - scan_start] = true;

  for (size_t boundary = end_pos; boundary > scan_start; --boundary) {
    if (!reachable[boundary - scan_start]) {
      continue;
    }
    for (const uint32_t edge_id : lattice.edgeIdsEndingAt(boundary)) {
      const auto& edge = lattice.getEdge(edge_id);
      if (edge.start < scan_start) {
        continue;
      }
      if (edge.extended_pos == core::ExtendedPOS::PronounInterrogative) {
        return true;
      }
      const bool stays_in_nominal_phrase =
          core::isNounType(edge.extended_pos) || edge.pos == core::PartOfSpeech::Suffix ||
          edge.extended_pos == core::ExtendedPOS::ParticleCase || edge.extended_pos == core::ExtendedPOS::ParticleNo ||
          edge.extended_pos == core::ExtendedPOS::ParticleBinding ||
          edge.extended_pos == core::ExtendedPOS::ParticleTopic;
      if (stays_in_nominal_phrase) {
        reachable[edge.start - scan_start] = true;
      }
    }
  }
  return false;
}

// The literary conjunctive expression ～につけ attaches to a preceding finite
// predicate and introduces a following clause (聞くにつけ、思い出す). It must
// not compete with the unrelated verb つける in sentence-initial につけて or
// in a construction such as 順位につけている, so require both the preceding
// lattice verb boundary and the clause-separating comma.
bool startsLiteraryNitsukeAt(const core::Lattice& lattice, const std::vector<char32_t>& codepoints, size_t start_pos) {
  constexpr size_t kNitsukeLength = 3;
  if (start_pos == 0 || start_pos + kNitsukeLength >= codepoints.size() || codepoints[start_pos] != U'に' ||
      codepoints[start_pos + 1] != U'つ' || codepoints[start_pos + 2] != U'け' ||
      codepoints[start_pos + kNitsukeLength] != U'、') {
    return false;
  }
  return hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbShuushikei);
}

// The method suffix 方 attaches to a kanji-containing deverbal noun
// (打ち合わせ+方). The unknown-word path can create the deverbal noun before
// the suffix position but has no all-kanji suffix rule to supply 方 itself.
bool hasPrecedingDeverbalNoun(const core::Lattice& lattice, size_t start_pos) {
  bool has_noun = false;
  bool has_renyokei = false;
  for (const uint32_t edge_id : lattice.edgeIdsEndingAt(start_pos)) {
    const auto& edge = lattice.getEdge(edge_id);
    if (grammar::containsKanji(edge.surface) && edge.pos == core::PartOfSpeech::Noun) {
      has_noun = true;
    }
    if (grammar::containsKanji(edge.surface) && edge.extended_pos == core::ExtendedPOS::VerbRenyokei) {
      has_renyokei = true;
    }
  }
  return has_noun && has_renyokei;
}

// Kyoto honorific やす is a closed auxiliary, but its two-mora surface is also
// the stem of the productive difficulty adjective やすい. Admit the honorific
// only after the local honorific construction that licenses it: お/ご plus a
// verb continuative, or a benefactive request form. This keeps 読み+やすかっ
// as an adjective while preserving お+見+やす and 読んで+おくれ+やす.
bool followsKyotoHonorificYasuHost(const core::Lattice& lattice, size_t start_pos) {
  return core::anyEdgeEndingAt(lattice, start_pos, [&lattice](const core::LatticeEdge& edge) {
    if (edge.extended_pos == core::ExtendedPOS::AuxBenefactive) {
      return true;
    }
    return edge.extended_pos == core::ExtendedPOS::VerbRenyokei &&
           core::anyEdgeEndingAt(lattice, edge.start, [](const core::LatticeEdge& prefix) {
             return prefix.extended_pos == core::ExtendedPOS::Prefix && grammar::isHonorificPrefix(prefix.surface);
           });
  });
}

}  // namespace

// The emphatic interrogative construction (何と+し+て+も, 誰と+し+て+も)
// is compositional.  Its quoted particle and する te-form must not be hidden
// by the otherwise valid compound-particle candidate として.  Look for a
// dictionary-verified interrogative ending exactly at the candidate boundary;
// this keeps ordinary nominal uses such as 道具としても intact.
bool hasInterrogativeEndingAt(const dictionary::DictionaryManager& dict_manager, std::string_view text,
                              const ByteOffsets& byte_offsets, size_t end_pos) {
  const size_t scan_start = dictionaryLookbehindStart(end_pos);
  for (size_t start_pos = scan_start; start_pos < end_pos; ++start_pos) {
    const size_t byte_pos = byteOffsetAt(byte_offsets, start_pos);
    for (const auto& result : dict_manager.lookup(text, byte_pos)) {
      if (result.entry != nullptr && result.entry->extended_pos == core::ExtendedPOS::PronounInterrogative &&
          start_pos + result.length == end_pos) {
        return true;
      }
    }
  }
  return false;
}

// The temporal noun 間 is licensed after a completed attributive predicate.
// Generate it only at that boundary instead of registering a global one-kanji
// noun that would reopen 間もなく, 時間, or 間違える internally.
bool hasPrecedingAttributivePredicate(const core::Lattice& lattice, size_t start_pos) {
  return hasPrecedingExtendedPOS(
      lattice, start_pos,
      {core::ExtendedPOS::VerbShuushikei, core::ExtendedPOS::VerbRentaikei, core::ExtendedPOS::AdjBasic,
       core::ExtendedPOS::AuxTenseTa, core::ExtendedPOS::AuxTenseMasu, core::ExtendedPOS::AuxNegativeNai,
       core::ExtendedPOS::AuxCopulaDa, core::ExtendedPOS::AuxCopulaDesu});
}

ContextualDictionaryCandidateState addContextualDictionaryCandidates(
    core::Lattice& lattice, const dictionary::DictionaryManager& dict_manager, std::string_view text,
    const std::vector<char32_t>& codepoints, const ByteOffsets& byte_offsets, size_t start_pos) {
  ContextualDictionaryCandidateState state;

  if (startsLiteraryNitsukeAt(lattice, codepoints, start_pos)) {
    tokenizer_dictionary_detail::addClosedClassEdge(lattice, "につけ", start_pos, start_pos + 3,
                                                    core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleConj,
                                                    "literary_nitsuke");
  }

  const bool starts_kyoto_honorific_yasu = start_pos + 1 < codepoints.size() && codepoints[start_pos] == U'や' &&
                                           codepoints[start_pos + 1] == U'す' &&
                                           followsKyotoHonorificYasuHost(lattice, start_pos);
  if (starts_kyoto_honorific_yasu) {
    tokenizer_dictionary_detail::addClosedClassEdge(lattice, "やす", start_pos, start_pos + 2,
                                                    core::PartOfSpeech::Auxiliary, core::ExtendedPOS::AuxHonorific,
                                                    "kyoto_honorific_yasu");
  }

  // The regional causal き is indistinguishable from an ordinary
  // continuative in isolation, so it has no global L1 entry. A completed past
  // auxiliary supplies the only unambiguous host (飲ん+だ+き), allowing this
  // context-licensed particle edge without cutting き out of lexical verbs.
  if (codepoints[start_pos] == U'き' && hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::AuxTenseTa)) {
    tokenizer_dictionary_detail::addClosedClassEdge(lattice, "き", start_pos, start_pos + 1,
                                                    core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleConj,
                                                    "regional_causal_ki");
  }

  if (grammar::isBoundDeverbalSuffixAt(codepoints, start_pos) && hasPrecedingDeverbalNoun(lattice, start_pos)) {
    const std::string suffix = normalize::encodeUtf8(codepoints[start_pos]);
    lattice.addEdge(suffix, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(start_pos + 1),
                    core::PartOfSpeech::Suffix, candidate::kDeverbalMethodSuffixCost,
                    core::LatticeEdge::kFromDictionary, suffix, dictionary::ConjugationType::None,
                    core::CandidateOrigin::SuffixPattern, candidate::kDictionaryOriginConfidence, {},
                    core::ExtendedPOS::Suffix, "deverbal_suffix");
  }

  state.has_attributive_temporal_ma =
      codepoints[start_pos] == U'間' && hasPrecedingAttributivePredicate(lattice, start_pos);
  if (state.has_attributive_temporal_ma) {
    tokenizer_dictionary_detail::addClosedClassEdge(
        lattice, "間", start_pos, start_pos + 1, core::PartOfSpeech::Noun, core::ExtendedPOS::NounFormal,
        "attributive_temporal_ma", core::LatticeEdge::kFromDictionary | core::LatticeEdge::kIsFormalNoun);
  }

  if (codepoints[start_pos] == U'か' && (hasInterrogativeEndingAt(dict_manager, text, byte_offsets, start_pos) ||
                                         hasInterrogativeNominalPhraseEndingAt(lattice, start_pos))) {
    tokenizer_dictionary_detail::addClosedClassEdge(lattice, "か", start_pos, start_pos + 1,
                                                    core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleAdverbial,
                                                    "indefinite_particle_ka");
  }

  // Keep the first/last か of a closed interrogative frame available when it
  // follows a finite predicate (読めるかどうか).
  const bool opens_interrogative_frame = codepoints[start_pos] == U'か' && start_pos + 3 < codepoints.size() &&
                                         codepoints[start_pos + 1] == U'ど' && codepoints[start_pos + 2] == U'う' &&
                                         codepoints[start_pos + 3] == U'か' &&
                                         hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbShuushikei);
  const bool closes_interrogative_frame =
      codepoints[start_pos] == U'か' && start_pos >= 3 && codepoints[start_pos - 3] == U'か' &&
      codepoints[start_pos - 2] == U'ど' && codepoints[start_pos - 1] == U'う' &&
      hasPrecedingExtendedPOS(lattice, start_pos - 3, core::ExtendedPOS::VerbShuushikei);
  if (opens_interrogative_frame || closes_interrogative_frame) {
    tokenizer_dictionary_detail::addClosedClassEdge(lattice, "か", start_pos, start_pos + 1,
                                                    core::PartOfSpeech::Particle, core::ExtendedPOS::ParticleAdverbial,
                                                    "interrogative_frame_ka");
  }

  // In shortened causative-passive, さ retains the lexical verb's mizenkei
  // boundary (読ま + さ + れ + た), rather than becoming a global する form.
  // A quotative predicate followed by される is する's irrealis plus the
  // passive auxiliary (…と + さ + れる), never the shortened causative
  // auxiliary.  Do not use the preceding character alone here: an irrealis
  // such as 書か is itself commonly also a particle character.
  const bool starts_quoted_passive =
      start_pos > 0 && codepoints[start_pos] == core::hiragana::kSa && start_pos + 1 < codepoints.size() &&
      codepoints[start_pos + 1] == U'れ' &&
      // The closed entry for と is POS-tagged as a case particle; its
      // quotative role is determined by this passive continuation.
      hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::ParticleCase) &&
      // A Godan irrealis ending here outranks the particle reading of its own
      // okurigana: 急が+さ+れ+た is the shortened causative-passive of 急ぐ, and
      // reading the が as a case particle turns the auxiliary into する. Only an
      // a-row kana can be that okurigana, which keeps a genuine quotative と in
      // the same position (確認したと+さ+れ+て).
      !(kana::isARowCodepoint(codepoints[start_pos - 1]) &&
        hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbMizenkei)) &&
      verb_helpers::isPassiveAuxContinuation(codepoints, start_pos + 2, /*strict_masu=*/true);
  if (starts_quoted_passive) {
    lattice.addEdge(
        "さ", static_cast<uint32_t>(start_pos), static_cast<uint32_t>(start_pos + 1), core::PartOfSpeech::Verb,
        getCategoryCost(core::ExtendedPOS::VerbMizenkei) + candidate::verb_cost::kQuotedPassiveSuruBonus,
        core::LatticeEdge::kFromDictionary | core::LatticeEdge::kHasCustomCost, "する",
        dictionary::ConjugationType::Suru, core::CandidateOrigin::Dictionary, candidate::kDictionaryOriginConfidence,
        "quoted_passive_suru", core::ExtendedPOS::VerbMizenkei, "quoted_passive_suru");
  }
  // A sahen nominal immediately before される supplies する's irrealis;
  // preserve its independent passive boundary (反映+さ+れ+ます).
  const bool starts_sahen_passive =
      !starts_quoted_passive && start_pos > 0 && codepoints[start_pos] == core::hiragana::kSa &&
      start_pos + 1 < codepoints.size() && codepoints[start_pos + 1] == U'れ' &&
      hasPrecedingSahenNominal(lattice, start_pos) &&
      verb_helpers::isPassiveAuxContinuation(codepoints, start_pos + 2, /*strict_masu=*/true);
  if (starts_sahen_passive) {
    tokenizer_dictionary_detail::addDictionaryOriginEdge(
        lattice, "さ", start_pos, start_pos + 1, core::PartOfSpeech::Verb,
        getCategoryCost(core::ExtendedPOS::VerbMizenkei) + candidate::verb_cost::kSahenPassiveSuruBonus,
        core::LatticeEdge::kFromDictionary | core::LatticeEdge::kHasCustomCost, "する",
        dictionary::ConjugationType::Suru, core::ExtendedPOS::VerbMizenkei, "sahen_passive_suru");
  }
  state.starts_shortened_causative_passive =
      start_pos > 0 && codepoints[start_pos] == core::hiragana::kSa && start_pos + 1 < codepoints.size() &&
      !starts_quoted_passive && codepoints[start_pos + 1] == U'れ' &&
      hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbMizenkei) &&
      (verb_helpers::isPassiveAuxContinuation(codepoints, start_pos + 2, /*strict_masu=*/true) ||
       subsidiaryVerbContinuativeAt(codepoints, start_pos + 2));
  if (state.starts_shortened_causative_passive) {
    lattice.addEdge(
        "さ", static_cast<uint32_t>(start_pos), static_cast<uint32_t>(start_pos + 1), core::PartOfSpeech::Auxiliary,
        getCategoryCost(core::ExtendedPOS::AuxCausative) + candidate::verb_cost::kShortenedCausativePassiveBonus,
        core::LatticeEdge::kFromDictionary | core::LatticeEdge::kHasCustomCost, "す",
        dictionary::ConjugationType::GodanSa, core::CandidateOrigin::Dictionary, candidate::kDictionaryOriginConfidence,
        "shortened_causative_passive", core::ExtendedPOS::AuxCausative, "shortened_causative_passive");
  }

  return state;
}

}  // namespace suzume::analysis::tokenizer_dictionary_detail
