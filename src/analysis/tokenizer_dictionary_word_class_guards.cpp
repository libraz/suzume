/**
 * @file tokenizer_dictionary_word_class_guards.cpp
 * @brief Dictionary readings whose word class cannot stand in their environment
 */

#include <algorithm>

#include "analysis/dictionary_probe.h"
#include "candidate_constants.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/honorific_verbs.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "tokenizer_dictionary_internal.h"
#include "verb_candidates_auxiliary_patterns.h"
#include "verb_candidates_dictionary_probes.h"
#include "verb_candidates_verb_stems.h"

namespace suzume::analysis::tokenizer_dictionary_detail {

namespace {

// A kanji run ending in な is an attributive na-adjective candidate.  A
// preceding one-kanji formal noun remains a separate grammatical unit in this
// environment (時 + 不思議 + な), unlike an ordinary lexical kanji compound.
bool isKanjiRunFollowedByAttributiveNa(const std::vector<char32_t>& codepoints, size_t start_pos) {
  size_t pos = start_pos;
  while (pos < codepoints.size() && normalize::isKanjiCodepoint(codepoints[pos])) {
    ++pos;
  }
  return pos > start_pos && pos < codepoints.size() && codepoints[pos] == U'な';
}

// A pure-hiragana na-adjective can share its surface with the interior of a
// kanji-led inflected verb. If a previously generated verb edge already
// crosses this position, the adjective cannot begin here without cutting the
// verb stem (読まれ, 生まれて, 止まれ). Scan only the immediately preceding
// kanji run; this keeps the check bounded and leaves genuine clause-initial or
// post-particle adjective uses available.
bool startsInsideKanjiLedVerb(const core::Lattice& lattice, const std::vector<char32_t>& codepoints, size_t start_pos) {
  if (start_pos == 0 || !normalize::isKanjiCodepoint(codepoints[start_pos - 1])) {
    return false;
  }

  size_t kanji_start = start_pos;
  while (kanji_start > 0 && normalize::isKanjiCodepoint(codepoints[kanji_start - 1])) {
    --kanji_start;
  }
  for (size_t pos = kanji_start; pos < start_pos; ++pos) {
    if (core::anyEdgeStartingAt(lattice, pos, [start_pos](const core::LatticeEdge& edge) {
          return edge.pos == core::PartOfSpeech::Verb && edge.end > start_pos && edge.lemmaVerified();
        })) {
      return true;
    }
  }
  return false;
}

// A dictionary adverb cannot begin inside an already verified inflected
// predicate.  Short literary adverbs can be homographic with the tail of an
// adjective or auxiliary followed by a particle (ない+と, らしい+と).  Keep
// the adverb available at a real boundary while protecting the longer
// grammatical edge that crosses this position.
//
// A predicate only counts when its own right edge could be a word boundary.
// Small kana cannot open a word, so an edge that ends just before one has not
// finished the word it belongs to and is in no position to claim the span:
// のめ (the potential stem of 飲む) ends before the っ of のめっちゃ, and
// letting it suppress the adverb hands those morae to a fragment instead.
bool startsInsideVerifiedPredicate(const core::Lattice& lattice, const std::vector<char32_t>& codepoints,
                                   size_t start_pos) {
  const size_t scan_start = dictionaryLookbehindStart(start_pos);
  for (size_t edge_start = scan_start; edge_start < start_pos; ++edge_start) {
    // A predicate opening inside a dictionary function word that ends exactly
    // at start_pos (な|んか|もう → かも) is that word's fragment, not a witness.
    const bool opens_inside_function_word =
        core::anyEdgeEndingAt(lattice, start_pos, [edge_start](const core::LatticeEdge& word) {
          return word.start < edge_start && word.fromDictionary() &&
                 (word.pos == core::PartOfSpeech::Particle || word.pos == core::PartOfSpeech::Auxiliary);
        });
    if (opens_inside_function_word) {
      continue;
    }
    if (core::anyEdgeStartingAt(lattice, edge_start, [&codepoints, start_pos](const core::LatticeEdge& edge) {
          return edge.end > start_pos && edge.lemmaVerified() &&
                 (edge.pos == core::PartOfSpeech::Verb || edge.pos == core::PartOfSpeech::Adjective ||
                  edge.pos == core::PartOfSpeech::Auxiliary) &&
                 (edge.end >= codepoints.size() || !kana::isSmallKanaCodepoint(codepoints[edge.end]));
        })) {
      return true;
    }
  }
  return false;
}

// A dictionary adverb may open on the last mora of a longer content word and
// carry an independent particle along with it (事実+に read as 事+実に, 勢い+と
// as 勢+いと, 勢い+とも as 勢+いとも). The same adverb stays available at a real
// boundary (実に+難しい, いとも+簡単に), so the guard is not about the entry but
// about the offset: reject it only when the mora it opens on completes a
// content edge that starts earlier, and what remains of the adverb after that
// mora is itself a registered particle. Both halves of the competing reading
// are then lexically attested, which the adverb's own span is not.
// This uses lattice structure rather than enumerating open-class words.
bool opensOnContentWordTailBeforeParticle(const core::Lattice& lattice,
                                          const dictionary::DictionaryManager& dict_manager,
                                          const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  if (start_pos == 0 || end_pos <= start_pos + 1 || end_pos > codepoints.size()) {
    return false;
  }
  if (lookupEntryInRange(dict_manager, codepoints, start_pos + 1, end_pos, core::PartOfSpeech::Particle) == nullptr) {
    return false;
  }
  const size_t content_end = start_pos + 1;
  // An unverified content edge is evidence only when no dictionary word ends
  // inside it: 事実 is one opaque run, while 私+少 and 明日+少 are fabricated
  // across a registered word's boundary (私+少し, 明日+少し).
  const auto crosses_dictionary_boundary = [&lattice](const core::LatticeEdge& edge) {
    for (size_t interior = edge.start + 1; interior < edge.end; ++interior) {
      if (core::anyEdgeEndingAt(lattice, interior,
                                [](const core::LatticeEdge& inner) { return inner.fromDictionary(); })) {
        return true;
      }
    }
    return false;
  };
  for (size_t content_start = 0; content_start < start_pos; ++content_start) {
    if (core::anyEdgeStartingAt(lattice, content_start, [&](const core::LatticeEdge& edge) {
          return edge.end == content_end &&
                 (edge.pos == core::PartOfSpeech::Noun || edge.pos == core::PartOfSpeech::Adjective) &&
                 (edge.lemmaVerified() || !crosses_dictionary_boundary(edge));
        })) {
      return true;
    }
  }
  return false;
}

// The temporal adverb いま overlaps the full polite forms of いる
// (います/いました/いません/…).  At a clause boundary the closed inflectional
// chain is more specific than the accidental いま+verb path.  Do not apply
// this inside a longer lexical continuation: いますぐ remains いま+すぐ.
bool startsIruPoliteFormAt(const std::vector<char32_t>& codepoints, size_t start_pos) {
  if (start_pos >= codepoints.size() || codepoints[start_pos] != U'い') {
    return false;
  }
  const size_t masu_length = verb_helpers::finiteMasuFormLengthAt(codepoints, start_pos + 1);
  if (masu_length == 0) {
    return false;
  }
  const size_t end_pos = start_pos + 1 + masu_length;
  if (end_pos >= codepoints.size()) {
    return true;
  }
  const char32_t following = codepoints[end_pos];
  return normalize::isExtendedParticle(following) || following == U'。' || following == U'、' || following == U'」' ||
         following == U'）';
}

// An intentional auxiliary is structurally meaningful here only when it
// closes the verb form selected by that auxiliary. Looking merely for any
// candidate ending at start_pos mistakes homographic word endings for an
// independent auxiliary and suppresses the following particle.
bool hasPrecedingVerbVolitionalChain(const core::Lattice& lattice, size_t start_pos) {
  return core::anyEdgeEndingAt(lattice, start_pos, [&lattice](const core::LatticeEdge& edge) {
    if (edge.extended_pos != core::ExtendedPOS::AuxVolitional &&
        edge.extended_pos != core::ExtendedPOS::AuxNegativeMai) {
      return false;
    }
    return core::anyEdgeEndingAt(lattice, edge.start, [&edge](const core::LatticeEdge& verb) {
      // The a-row mizenkei of する (さ) only hosts the passive/causative, never ん
      // (田中さ+ん+と+し+て is 田中+さん+として).
      const bool is_suru_passive_stem =
          grammar::isSuruBaseForm(verb.lemma) && kana::isARowCodepoint(utf8::decodeFirstChar(verb.surface));
      const bool licenses_volitional = edge.extended_pos == core::ExtendedPOS::AuxVolitional &&
                                       verb.extended_pos == core::ExtendedPOS::VerbMizenkei && !is_suru_passive_stem;
      const bool licenses_negative_intent = edge.extended_pos == core::ExtendedPOS::AuxNegativeMai &&
                                            verb.extended_pos == core::ExtendedPOS::VerbShuushikei;
      return licenses_volitional || licenses_negative_intent;
    });
  });
}

// A two-mora conjunction candidate can straddle the productive boundary in
// AdjNaAdj + な + お/ご + nominal (重要+な+お+知らせ). A preceding
// dictionary-verified na-adjective and a kanji/katakana head on the right make
// that structure explicit, including open-class heads absent from the
// dictionary, so the discourse-conjunction homograph is unavailable. Requiring
// dictionary evidence keeps incidental unknown adjective candidates from
// suppressing a real conjunction after a completed clause.
bool crossesAttributiveNaHonorificNominal(const core::Lattice& lattice, const std::vector<char32_t>& codepoints,
                                          size_t start_pos, size_t end_pos) {
  if (end_pos != start_pos + 2 || end_pos >= codepoints.size() || codepoints[start_pos] != U'な' ||
      !grammar::isHonorificPrefix(extractSubstring(codepoints, start_pos + 1, end_pos)) ||
      (!normalize::isKanjiCodepoint(codepoints[end_pos]) &&
       normalize::classifyChar(codepoints[end_pos]) != normalize::CharType::Katakana)) {
    return false;
  }

  return core::anyEdgeEndingAt(lattice, start_pos, [](const core::LatticeEdge& edge) {
    return edge.extended_pos == core::ExtendedPOS::AdjNaAdj && edge.fromDictionary();
  });
}

// generateTemporalNounBoundaryCandidates() marks the left side of a
// lexicalized all-kanji + 間もなく sequence as a PrefixCompound noun.  Use
// that structural edge to distinguish 終了|間もなく from a candidate that
// would reopen the interior of 時間 (時|間もなく).
bool hasPrecedingTemporalCompoundBoundary(const core::Lattice& lattice, size_t start_pos) {
  return core::anyEdgeEndingAt(lattice, start_pos, [](const core::LatticeEdge& edge) {
    return edge.pos == core::PartOfSpeech::Noun && edge.origin == core::CandidateOrigin::PrefixCompound;
  });
}

bool startsFormalNounParticleAfterPredicate(const core::Lattice& lattice,
                                            const dictionary::DictionaryManager& dict_manager,
                                            const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  if (!hasPrecedingPartOfSpeech(lattice, start_pos, kVerbAdjectiveMask)) {
    return false;
  }
  for (size_t split = start_pos + 1; split < end_pos; ++split) {
    const auto* noun = lookupEntryInRange(dict_manager, codepoints, start_pos, split, core::PartOfSpeech::Noun);
    if (noun == nullptr || noun->extended_pos != core::ExtendedPOS::NounFormal) {
      continue;
    }
    if (lookupEntryInRange(dict_manager, codepoints, split, end_pos, core::PartOfSpeech::Particle) != nullptr) {
      return true;
    }
  }
  return false;
}

// A dictionary noun opening on the counter of a numeral+counter cuts the
// quantity in two when what follows the counter is a formal noun (三日+付け,
// not 三+日付け), the same boundary the counter generator binds.
bool cutsNumeralCounterBeforeFormalNoun(const dictionary::DictionaryManager& dict_manager,
                                        const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  if (start_pos == 0 || end_pos <= start_pos + 1 || !normalize::isNumeralCodepoint(codepoints[start_pos - 1]) ||
      !(normalize::isCounterKanji(codepoints[start_pos]) || normalize::isTemporalCounterKanji(codepoints[start_pos]))) {
    return false;
  }
  const auto* formal = lookupEntryInRange(dict_manager, codepoints, start_pos + 1, end_pos, core::PartOfSpeech::Noun);
  return formal != nullptr && formal->extended_pos == core::ExtendedPOS::NounFormal;
}

// A case particle immediately before an ABAB mimetic is a stronger boundary
// than a homographic multi-mora dictionary entry beginning at that particle
// (鈴+が+りんりんと, not がり+んりんと).
bool startsParticleBeforeReduplicatedMimetic(const std::vector<char32_t>& codepoints, size_t start_pos) {
  if (start_pos + 5 >= codepoints.size() || !normalize::isParticleCodepoint(codepoints[start_pos])) {
    return false;
  }
  const size_t rest = start_pos + 1;
  return codepoints[rest] == codepoints[rest + 2] && codepoints[rest + 1] == codepoints[rest + 3] &&
         codepoints[rest + 4] == U'と';
}

// A closed determiner may happen to share a whole surface with a dictionary
// Godan onbin + past form.  At a sentence boundary the finite predicate owns
// that construction: the determiner requires a following nominal, whereas
// the attested verb stem and its matching past allomorph form a complete
// clause.  Resolve this from the conjugation table and lexical base evidence,
// never from a particular homographic surface.
// Both the past and the connective suffix select their voiced allomorph from
// the same Godan row, so the two forms differ only in which kana pair closes
// the sequence.
bool isDictionaryOnbinBefore(const dictionary::DictionaryManager& dict_manager, std::string_view surface,
                             std::string_view unvoiced, std::string_view voiced) {
  const std::string_view suffix = utf8::lastChar(surface);
  if (suffix != unvoiced && suffix != voiced) {
    return false;
  }
  const std::string_view onbin_stem = utf8::dropLastChar(surface);
  const std::string_view onbin = utf8::lastChar(onbin_stem);
  const std::string_view lexical_stem = utf8::dropLastChar(onbin_stem);
  if (lexical_stem.empty()) {
    return false;
  }
  const auto match = verb_helpers::firstGodanOnbinDictBase(&dict_manager, lexical_stem, onbin);
  if (!match.matched) {
    return false;
  }
  const auto* row = grammar::Conjugation::getGodanRow(match.verb_type);
  return row != nullptr && (row->voiced_ta ? suffix == voiced : suffix == unvoiced);
}

bool isDictionaryOnbinTeForm(const dictionary::DictionaryManager& dict_manager, std::string_view surface) {
  return isDictionaryOnbinBefore(dict_manager, surface, "て", "で");
}

bool startsKuruConditional(const std::vector<char32_t>& codepoints, size_t start_pos) {
  return start_pos + 2 < codepoints.size() && codepoints[start_pos] == U'く' && codepoints[start_pos + 1] == U'れ' &&
         codepoints[start_pos + 2] == U'ば';
}

}  // namespace

bool isDictionaryOnbinPast(const dictionary::DictionaryManager& dict_manager, std::string_view surface) {
  return isDictionaryOnbinBefore(dict_manager, surface, "た", "だ");
}

bool isOutOfPlaceForWordClass(const DictionaryCandidateContext& ctx, const core::Lattice& lattice,
                              const dictionary::LookupResult& result, size_t end_pos) {
  const auto& dict_manager = ctx.dict_manager;
  const auto& inflection = ctx.inflection;
  const std::string_view text = ctx.text;
  const auto& codepoints = ctx.codepoints;
  const auto& byte_offsets = ctx.byte_offsets;
  const size_t start_pos = ctx.start_pos;
  const auto& lookup_results = ctx.lookup_results;
  const bool suppress_prefixed_noun_interior = ctx.suppress_prefixed_noun_interior;
  const size_t longest_conjunction = ctx.longest_conjunction;
  const size_t longest_fixed_conjunction = ctx.longest_fixed_conjunction;
  const size_t longest_adverb = ctx.longest_adverb;
  const size_t longest_potential_benefactive = ctx.longest_potential_benefactive;
  // A word that selects a te-form: the progressive いる, the benefactive verbs, the
  // humble honorific ください, and the conjunctive から (てから).
  const auto starts_te_selecting_word = [&](size_t pos) {
    if (pos >= codepoints.size()) {
      return false;
    }
    const auto following = dict_manager.lookup(text, byteOffsetAt(byte_offsets, pos));
    return std::any_of(following.begin(), following.end(), [](const auto& candidate) {
      const auto* entry = candidate.entry;
      return entry != nullptr &&
             (entry->lemma == "いる" || grammar::isBenefactiveLemma(entry->lemma) ||
              grammar::isHumbleHonorificLemma(entry->lemma) ||
              (entry->extended_pos == core::ExtendedPOS::ParticleConj && utf8::equalsAny(entry->surface, {"から"})));
    });
  };

  // Prefer the longest member only within the same particle class. This
  // keeps closed concessives such as ども/けれども intact without
  // suppressing productive boundaries whose shorter member has another
  // grammatical role (で+も, と+も).
  if (result.entry->pos == core::PartOfSpeech::Particle) {
    // A compound case particle ends an adpositional phrase and cannot host
    // a word that selects a te-form (the aspectual いる, ください, てから).
    // When that continuation is present, keep the shorter internal
    // case-particle boundary so the adjoining verb te-form can carry it
    // (目を+通し+て+いる, 席に+つい+て+ください).
    const bool compound_particle_before_aspect = result.entry->extended_pos == core::ExtendedPOS::ParticleCase &&
                                                 result.length > 1 && utf8::endsWith(result.entry->surface, "て") &&
                                                 starts_te_selecting_word(end_pos);
    if (compound_particle_before_aspect) {
      return true;
    }
    const bool follows_volitional = hasPrecedingVerbVolitionalChain(lattice, start_pos);
    // After an explicit volitional auxiliary, a multi-mora case particle
    // would hide the productive quotative + suru sequence
    // (書こ+う+と+し+て). Keep the one-mora quotative candidate even when a
    // longer case-particle entry shares its prefix. Only an entry opening on
    // that same quotative mora can hide it; a compound particle beginning
    // anywhere else shares nothing with the sequence and stays available
    // (いかん+によって, where the ん also reads as the literary volitional).
    if (follows_volitional && result.entry->extended_pos == core::ExtendedPOS::ParticleCase && result.length > 1 &&
        utf8::decodeFirstChar(result.entry->surface) == core::hiragana::kTo) {
      return true;
    }
    const bool has_longer_same_class =
        std::any_of(lookup_results.begin(), lookup_results.end(), [&](const auto& other) {
          const size_t other_end = start_pos + other.length;
          return other.entry != nullptr && other.entry->pos == core::PartOfSpeech::Particle &&
                 other.entry->extended_pos == result.entry->extended_pos && other.length > result.length &&
                 !grammar::isAttributiveCompoundParticleSurface(other.entry->surface) &&
                 !(other.entry->extended_pos == core::ExtendedPOS::ParticleCase && other.length > 1 &&
                   utf8::endsWith(other.entry->surface, "て") && starts_te_selecting_word(other_end));
        });
    const bool keep_interrogative_quotative =
        result.entry->extended_pos == core::ExtendedPOS::ParticleCase && result.length == 1 &&
        grammar::isSingleHiragana(result.entry->surface, core::hiragana::kTo) &&
        std::any_of(lookup_results.begin(), lookup_results.end(),
                    [&](const auto& other) {
                      const size_t other_end = start_pos + other.length;
                      return other.entry != nullptr && other.entry->extended_pos == core::ExtendedPOS::ParticleCase &&
                             grammar::isQuotativeSuruTeCompoundParticle(other.entry->surface) &&
                             other_end < codepoints.size() && codepoints[other_end] == U'も';
                    }) &&
        hasInterrogativeEndingAt(dict_manager, text, byte_offsets, start_pos);
    const bool keep_volitional_quotative =
        follows_volitional && result.entry->extended_pos == core::ExtendedPOS::ParticleCase && result.length == 1;
    if (has_longer_same_class && !keep_volitional_quotative && !keep_interrogative_quotative) {
      return true;
    }
  }

  if (result.entry->pos == core::PartOfSpeech::Adverb &&
      adverbAbsorbsQuotedQuestion(codepoints, result.length, end_pos)) {
    return true;
  }

  // A lexical adverb homographic with a dictionary-verified verb te-form
  // cannot govern the progressive auxiliary いる. Preserve the verb stem +
  // connective boundary in that environment while leaving ordinary adverb
  // uses untouched.
  if (result.entry->pos == core::PartOfSpeech::Adverb && end_pos + 1 < codepoints.size() &&
      codepoints[end_pos] == U'い' && codepoints[end_pos + 1] == U'る' &&
      utf8::endsWithAny(result.entry->surface, {"て", "で"})) {
    bool is_verified_verb_te_form = false;
    for (const auto& inflection_candidate : inflection.analyze(result.entry->surface)) {
      if (inflection_candidate.verb_type != grammar::VerbType::IAdjective &&
          (verb_helpers::isVerbInDictionary(&dict_manager, inflection_candidate.base_form) ||
           inflection_candidate.confidence >= candidate::kAdverbVerbTeHomographMinConfidence)) {
        is_verified_verb_te_form = true;
        break;
      }
    }
    if (is_verified_verb_te_form) {
      return true;
    }
  }

  // A conjunction that is also a productive verb+particle sequence is
  // lexical only at a clause boundary. Inside a phrase, keep the ordinary
  // predicate boundary (もしか+する+と).
  // A final particle spelled with a leading sokuon (っちゃ, っぴ) geminates
  // onto a finished predicate; with none in front, the sokuon belongs to the
  // word before it (行っ+ちゃ+だめ, not 行+っちゃ).
  if (result.entry->extended_pos == core::ExtendedPOS::ParticleFinal &&
      codepoints[start_pos] == core::hiragana::kSmallTsu &&
      !hasPrecedingPartOfSpeech(lattice, start_pos, kPredicateHostMask)) {
    return true;
  }

  if (result.entry->pos == core::PartOfSpeech::Conjunction && start_pos > 0) {
    if (crossesAttributiveNaHonorificNominal(lattice, codepoints, start_pos, end_pos)) {
      return true;
    }
    // A conjunction opens a clause, so it cannot start inside a registered
    // particle that began one mora earlier (か|も of かも+って).
    bool opens_inside_particle = false;
    for (size_t split = 1; split < result.length; ++split) {
      if (lookupEntryInRange(dict_manager, codepoints, start_pos - 1, start_pos + split,
                             core::PartOfSpeech::Particle) != nullptr) {
        opens_inside_particle = true;
        break;
      }
    }
    if (opens_inside_particle) {
      return true;
    }
    // A conjunction introduces what follows, so one closed off by punctuation
    // or the sentence end right after a content word has nothing to join
    // (時間+ない+し、 not 時間+ないし、) — the EOS AfterContent gate, mirrored.
    constexpr PartOfSpeechMask kContentMask = kNounPronounMask | kVerbAdjectiveMask;
    if (verb_helpers::clauseEndsAt(codepoints, end_pos) && hasPrecedingPartOfSpeech(lattice, start_pos, kContentMask)) {
      return true;
    }
    bool decomposes_as_verb_particle = false;
    for (size_t split = 1; split < result.length; ++split) {
      if (lookupEntryInRange(dict_manager, codepoints, start_pos, start_pos + split, core::PartOfSpeech::Verb) !=
              nullptr &&
          lookupEntryInRange(dict_manager, codepoints, start_pos + split, end_pos, core::PartOfSpeech::Particle) !=
              nullptr) {
        decomposes_as_verb_particle = true;
        break;
      }
    }
    const bool coordinates_nominals = hasPrecedingNominal(lattice, start_pos) && end_pos < codepoints.size() &&
                                      (normalize::isKanjiCodepoint(codepoints[end_pos]) ||
                                       normalize::classifyChar(codepoints[end_pos]) == normalize::CharType::Katakana);
    const bool follows_completed_clause = hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::AuxTenseTa);
    if (decomposes_as_verb_particle && !coordinates_nominals && !follows_completed_clause &&
        normalize::classifyChar(codepoints[start_pos - 1]) != normalize::CharType::Symbol) {
      return true;
    }
  }

  // A conjunction must not absorb a dictionary-verified te-form immediately
  // before the conditional directional 来る. The latter is a productive
  // predicate chain (持っ+て+くれ+ば), while the conjunction cannot govern
  // that auxiliary inflection.
  if (result.entry->pos == core::PartOfSpeech::Conjunction &&
      isDictionaryOnbinTeForm(dict_manager, result.entry->surface) && startsKuruConditional(codepoints, end_pos)) {
    return true;
  }

  if (result.length > 1 && startsParticleBeforeReduplicatedMimetic(codepoints, start_pos) && end_pos > start_pos + 1) {
    return true;
  }

  if (result.entry->pos == core::PartOfSpeech::Noun &&
      cutsNumeralCounterBeforeFormalNoun(dict_manager, codepoints, start_pos, end_pos)) {
    return true;
  }

  if (result.entry->pos == core::PartOfSpeech::Adverb && result.length == 2 &&
      startsIruPoliteFormAt(codepoints, start_pos)) {
    return true;
  }

  if (result.entry->pos == core::PartOfSpeech::Adverb &&
      startsInsideVerifiedPredicate(lattice, codepoints, start_pos)) {
    return true;
  }

  // The lexical temporal adverb can follow a verified compound boundary,
  // but must not reopen the final 間 of a shorter duration noun.
  if (result.entry->pos == core::PartOfSpeech::Adverb && codepoints[start_pos] == U'間' && start_pos > 0 &&
      normalize::isKanjiCodepoint(codepoints[start_pos - 1]) &&
      !hasPrecedingTemporalCompoundBoundary(lattice, start_pos)) {
    return true;
  }

  if (result.entry->pos == core::PartOfSpeech::Adverb &&
      opensOnContentWordTailBeforeParticle(lattice, dict_manager, codepoints, start_pos, end_pos)) {
    return true;
  }

  // Do not reopen the interior of a kanji-led verb as a pure-hiragana
  // dictionary na-adjective. The same adjective remains available at a real
  // boundary (sentence start or after a particle).
  if (result.entry->extended_pos == core::ExtendedPOS::AdjNaAdj && grammar::isPureHiragana(result.entry->surface) &&
      startsInsideKanjiLedVerb(lattice, codepoints, start_pos)) {
    return true;
  }

  // A period suffix cannot head an interval compound.  In a numeral-led
  // expression such as 10分間隔, the counter generator already supplies
  // 10分 and the following lexical noun must remain 間隔, not 間+隔.
  if (result.entry->extended_pos == core::ExtendedPOS::Suffix && result.length == 1 &&
      start_pos + 1 < codepoints.size() && codepoints[start_pos] == U'間' &&
      normalize::isIntervalCompoundSecondKanji(codepoints[start_pos + 1])) {
    return true;
  }

  // 時間接尾辞「後」は終了+後・三日+後のように内容語へ直接接合
  // する。ひらがな活用や助詞の後では独立時間名詞なので、suffix
  // edgeを出さず既存のnoun候補へ任せる（食べた+後、ので+後）。
  if (result.entry->extended_pos == core::ExtendedPOS::Suffix &&
      grammar::isDirectAttachmentTemporalSuffix(result.entry->surface)) {
    if (start_pos == 0) {
      return true;
    }
    const auto preceding_type = normalize::classifyChar(codepoints[start_pos - 1]);
    const bool directly_attached_to_nominal =
        preceding_type == normalize::CharType::Kanji || preceding_type == normalize::CharType::Katakana ||
        preceding_type == normalize::CharType::Alphabet || preceding_type == normalize::CharType::Digit;
    if (!directly_attached_to_nominal) {
      return true;
    }
  }

  // A one-kanji formal noun cannot head an adjacent kanji compound.  The
  // formal reading remains available at a word boundary (ない+事), while a
  // lexical compound such as 事情 or 事実 keeps its complete search unit.
  // After a registered noun and before a kanji compound it closes the left
  // noun instead (先月+末+決算).
  if (result.entry->extended_pos == core::ExtendedPOS::NounFormal && result.length == 1 &&
      end_pos < codepoints.size() && normalize::isKanjiCodepoint(codepoints[end_pos]) &&
      !isKanjiRunFollowedByAttributiveNa(codepoints, end_pos)) {
    const bool closes_preceding_noun =
        start_pos >= 2 && end_pos + 2 <= codepoints.size() &&
        lookupEntryInRange(dict_manager, codepoints, start_pos - 2, start_pos, core::PartOfSpeech::Noun) != nullptr &&
        normalize::isKanjiCodepoint(codepoints[end_pos + 1]);
    if (!closes_preceding_noun) {
      return true;
    }
  }

  // わりに is an adverb at clause start, but after an attributive の or a
  // finite predicate it is the formal noun わり followed by the case
  // particle に (本の+わりに, 読む+わりに). Before an adjective it instead
  // forms the fixed comparative adverb (年齢の+わりに+若い).
  if (result.entry->pos == core::PartOfSpeech::Adverb && result.entry->lemma == "わりに" && start_pos > 0) {
    if (startsInsideKanjiLedVerb(lattice, codepoints, start_pos)) {
      return true;
    }
    const char32_t preceding = codepoints[start_pos - 1];
    const bool followed_by_adjective =
        end_pos < codepoints.size() &&
        lookupResultsHavePartOfSpeech(dict_manager.lookup(text, byteOffsetAt(byte_offsets, end_pos)),
                                      partOfSpeechMask(core::PartOfSpeech::Adjective));
    if (!followed_by_adjective &&
        (preceding == U'の' || preceding == U'る' || preceding == U'く' || preceding == U'む' || preceding == U'ぶ' ||
         preceding == U'ぬ' || preceding == U'す' || preceding == U'つ' || preceding == U'ぐ')) {
      return true;
    }
  }

  if (result.entry->pos == core::PartOfSpeech::Adverb &&
      startsFormalNounParticleAfterPredicate(lattice, dict_manager, codepoints, start_pos, end_pos)) {
    return true;
  }

  const bool fused_demo_after_te_form = result.length == 2 && codepoints[start_pos] == U'で' &&
                                        codepoints[start_pos + 1] == U'も' &&
                                        hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbOnbinkei);
  if (fused_demo_after_te_form) {
    return true;
  }

  if (suppress_prefixed_noun_interior) {
    return true;
  }

  // Prefer the maximal closed-class conjunction at this position: 又は,
  // not 又+は. Shorter prefixes remain available when no longer conjunction
  // matches the input.
  if (result.entry->pos == core::PartOfSpeech::Conjunction && result.length < longest_conjunction) {
    return true;
  }

  // Members of the closed adverb lexicon use maximal matching within their
  // own class (必ずしも, どうしても). Shorter dictionary adverbs remain
  // available whenever no longer adverb actually covers the input.
  if (result.entry->pos == core::PartOfSpeech::Adverb && result.length < longest_adverb) {
    return true;
  }

  // A complete member of the closed potential-benefactive paradigm is
  // authoritative over shorter homographs starting at the same position.
  // This keeps いただけ(る/ない/ます) from reopening as い+た+だけ while
  // leaving every position without that exact closed-class match untouched.
  if (result.length < longest_potential_benefactive) {
    return true;
  }

  // At sentence start, a longer closed-class conjunction takes precedence
  // over a homographic auxiliary prefix.  After a topic/focus particle,
  // suppress only the polite auxiliary prefix: ます requires a verb
  // renyokei, so に+も+まし+て cannot be a polite chain.  Other auxiliaries
  // remain available (本+も+だ+けど).
  const bool sentence_initial_auxiliary = start_pos == 0 && result.entry->pos == core::PartOfSpeech::Auxiliary;
  const bool unlicensed_polite_after_topic =
      result.entry->extended_pos == core::ExtendedPOS::AuxTenseMasu &&
      hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::ParticleTopic);
  if ((sentence_initial_auxiliary || unlicensed_polite_after_topic) && result.length < longest_fixed_conjunction) {
    return true;
  }

  // A bound derivational suffix verb has no independent use, so without a
  // nominal host in front the entry is not a candidate at all.
  if (result.entry->pos == core::PartOfSpeech::Verb &&
      grammar::isBoundDerivationalSuffixVerbLemma(result.entry->lemma) &&
      !verb_helpers::hasNominalHostBefore(codepoints, start_pos)) {
    return true;
  }

  // Past た/だ is an auxiliary boundary, not part of a dictionary verb
  // token.  Inflected dictionary entries still provide the stem/onbin edge;
  // discard only the fused full-past alternative.
  if (result.entry->extended_pos == core::ExtendedPOS::VerbTaForm &&
      utf8::endsWithAny(result.entry->surface, {"た", "だ"})) {
    return true;
  }
  return false;
}

}  // namespace suzume::analysis::tokenizer_dictionary_detail
