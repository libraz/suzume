/**
 * @file tokenizer_dictionary.cpp
 * @brief Dictionary-backed candidate generation for the tokenizer
 */

#include <algorithm>

#include "analysis/bigram_table.h"
#include "analysis/category_cost.h"
#include "analysis/dictionary_probe.h"
#include "analysis/tokenizer.h"
#include "candidate_constants.h"
#include "core/debug.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/honorific_verbs.h"
#include "join_candidates.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "split_candidates.h"
#include "suffix_candidates.h"
#include "tokenizer_dictionary_internal.h"
#include "tokenizer_utils.h"
#include "verb_candidates_auxiliary_patterns.h"
#include "verb_candidates_dictionary_probes.h"
#include "verb_candidates_emphatic.h"

namespace suzume::analysis {

using tokenizer_dictionary_detail::addClippedInterjectionCandidates;
using tokenizer_dictionary_detail::addContextualDictionaryCandidates;
using tokenizer_dictionary_detail::addElidedProlongedDictionaryCandidates;
using tokenizer_dictionary_detail::addTruncatedAdverbCandidates;
using tokenizer_dictionary_detail::adverbAbsorbsQuotedQuestion;
using tokenizer_dictionary_detail::ContextualDictionaryCandidateState;
using tokenizer_dictionary_detail::crossesEstablishedBoundary;
using tokenizer_dictionary_detail::hasPrecedingNominal;
using tokenizer_dictionary_detail::isOutOfPlaceForWordClass;
using tokenizer_dictionary_detail::lacksLicensingEnvironment;
using tokenizer_dictionary_detail::losesHomographReading;

namespace {

// Whether a listed word opening before @p start_pos reaches @p end_pos or
// further, so the span is inside that word rather than a word of its own.
bool hasDictionaryEntrySpanningBack(const dictionary::DictionaryManager& dict_manager,
                                    const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos) {
  constexpr size_t kMaxHostChars = 4;
  constexpr size_t kMaxTailChars = 4;
  const size_t scan_start = lookbehindStart(start_pos, kMaxHostChars);
  const size_t probe_end = std::min(codepoints.size(), end_pos + kMaxTailChars);
  for (size_t host_start = scan_start; host_start < start_pos; ++host_start) {
    for (const auto& match : lookupResultsInRange(dict_manager, codepoints, host_start, probe_end)) {
      if (match.entry != nullptr && host_start + match.length >= end_pos) {
        return true;
      }
    }
  }
  return false;
}

// A lexicalized noun beginning with お/ご can contain a suffix that happens to
// be a verb form.  Once the lattice has reached that suffix, prefer the whole
// dictionary noun and do not reopen it as a low-cost verb/auxiliary chain.
// The verb-tail check is essential: ordinary prefixed nouns such as おかし
// retain their independently searchable prefix + noun analysis.
bool startsHonorificPrefixedNounWithVerbTail(const dictionary::DictionaryManager& dict_manager, std::string_view text,
                                             const std::vector<char32_t>& codepoints, const ByteOffsets& byte_offsets,
                                             size_t start_pos) {
  if (start_pos == 0 || !grammar::isHonorificPrefix(extractSubstring(codepoints, start_pos - 1, start_pos))) {
    return false;
  }

  const size_t prefix_pos = start_pos - 1;
  const size_t prefix_byte_pos = byteOffsetAt(byte_offsets, prefix_pos);
  for (const auto& result : dict_manager.lookup(text, prefix_byte_pos)) {
    if (result.entry == nullptr || result.entry->pos != core::PartOfSpeech::Noun || result.length <= 1) {
      continue;
    }

    const size_t noun_end = prefix_pos + result.length;
    if (noun_end <= start_pos || noun_end > codepoints.size()) {
      continue;
    }

    if (lookupEntryInRange(dict_manager, codepoints, start_pos, noun_end, core::PartOfSpeech::Verb) != nullptr) {
      return true;
    }
  }
  return false;
}

// Whether a surface spells the continuative of a registered verb.
bool namesVerbContinuative(const dictionary::DictionaryManager& dict_manager, std::string_view surface) {
  const char32_t tail = utf8::decodeLastChar(surface);
  const std::string_view stem = utf8::dropLastChar(surface);
  if (stem.empty()) {
    return false;
  }
  if (verb_helpers::hasDictionaryGodanBaseFromIRow(&dict_manager, stem, tail)) {
    return true;
  }
  return kana::isERowCodepoint(tail) &&
         verb_helpers::isVerbInDictionary(&dict_manager, normalize::concat(surface, "る"));
}

// Whether a surface carries lexical content on its own, either as a listed
// entry or as the continuative of a listed verb.
bool namesContentUnit(const dictionary::DictionaryManager& dict_manager, std::string_view surface) {
  return dict_manager.lookupExact(surface) != nullptr || namesVerbContinuative(dict_manager, surface);
}

// A listed noun that spells the continuative of a listed verb and divides no
// further is a simplex deverbal noun: its only competitor is that verb reading
// over the very same span (楽しみ from 楽しむ). Saying so on the edge lets the
// word scorer keep the length evidence it uses against a path that fragments a
// span from also deciding a question it has none about, namely which of two
// same-span readings is right. A deverbal noun that does divide (折れ+曲がり,
// 間違い as 間+違い, 取り+込み) has a fragmenting competitor after all, so it is
// left as an ordinary noun and keeps that evidence.
bool namesSimplexDeverbalNoun(const dictionary::DictionaryManager& dict_manager,
                              const dictionary::DictionaryEntry& entry) {
  if (entry.pos != core::PartOfSpeech::Noun || entry.extended_pos != core::ExtendedPOS::Noun ||
      !namesVerbContinuative(dict_manager, entry.surface)) {
    return false;
  }
  const std::vector<char32_t> codepoints = normalize::toCodepoints(entry.surface);
  for (size_t split = 1; split + 1 <= codepoints.size(); ++split) {
    const std::string head = extractSubstring(codepoints, 0, split);
    const std::string tail = extractSubstring(codepoints, split, codepoints.size());
    if (namesContentUnit(dict_manager, head) && namesContentUnit(dict_manager, tail)) {
      return false;
    }
  }
  return true;
}

// Add the verb-stem edges a dictionary entry implies. Returns false when the
// entry itself is not a candidate, either because its stem edge replaces it or
// because its span is not a word.
bool addDictionaryVerbStemEdges(core::Lattice& lattice,
                                const tokenizer_dictionary_detail::DictionaryCandidateContext& ctx,
                                const dictionary::LookupResult& result, size_t end_pos) {
  const auto& dict_manager = ctx.dict_manager;
  const auto& codepoints = ctx.codepoints;
  const size_t start_pos = ctx.start_pos;
  if (result.entry->pos == core::PartOfSpeech::Verb && utf8::endsWith(result.entry->surface, "ぬ") &&
      result.entry->lemma != result.entry->surface) {
    const std::string stem_surface = std::string(utf8::dropLastChar(result.entry->surface));
    tokenizer_dictionary_detail::addDictionaryOriginEdge(
        lattice, stem_surface, start_pos, end_pos - 1, core::PartOfSpeech::Verb,
        getCategoryCost(core::ExtendedPOS::VerbMizenkei), core::LatticeEdge::kFromDictionary, result.entry->lemma,
        dictionary::ConjugationType::None, core::ExtendedPOS::VerbMizenkei, "dictionary_classical_negative_stem");
    return false;
  }

  if ((result.entry->pos == core::PartOfSpeech::Verb || result.entry->pos == core::PartOfSpeech::Adjective ||
       result.entry->pos == core::PartOfSpeech::Noun) &&
      result.length > 1 && codepoints[start_pos] == U'は' && codepoints[end_pos - 1] == U'な' &&
      end_pos + 1 < codepoints.size() && codepoints[end_pos] == U'か' && codepoints[end_pos + 1] == U'っ') {
    return false;
  }

  if (result.entry->pos == core::PartOfSpeech::Noun && end_pos < codepoints.size() &&
      codepoints[end_pos - 1] == U'し' && codepoints[end_pos] == U'て') {
    const std::string verb_base = normalize::concat(utf8::dropLastChar(result.entry->surface), "す");
    const auto* verb = dict_manager.lookupExact(verb_base, core::PartOfSpeech::Verb);
    if (verb != nullptr) {
      tokenizer_dictionary_detail::addDictionaryOriginEdge(
          lattice, result.entry->surface, start_pos, end_pos, core::PartOfSpeech::Verb,
          getCategoryCost(core::ExtendedPOS::VerbRenyokei) + candidate::kVerifiedTailCompoundVerbBonus +
              candidate::kVerifiedVerbBonus,
          core::LatticeEdge::kFromDictionary, verb_base, dictionary::ConjugationType::GodanSa,
          core::ExtendedPOS::VerbRenyokei, "dictionary_godan_sa_renyokei");
    }
  }

  if (result.entry->pos == core::PartOfSpeech::Noun && end_pos < codepoints.size() &&
      normalize::isKanjiCodepoint(codepoints[end_pos]) && kana::isIRowCodepoint(codepoints[end_pos - 1])) {
    const std::string_view base_suffix = grammar::godanBaseSuffixFromIRow(codepoints[end_pos - 1]);
    if (!base_suffix.empty()) {
      const std::string verb_base = normalize::concat(utf8::dropLastChar(result.entry->surface), base_suffix);
      const auto* verb = dict_manager.lookupExact(verb_base, core::PartOfSpeech::Verb);
      if (verb != nullptr) {
        const auto conj_type = grammar::verbTypeToConjType(
            grammar::verbTypeFromBaseCodepoint(utf8::decodeFirstChar(utf8::lastChar(verb_base))));
        tokenizer_dictionary_detail::addDictionaryOriginEdge(
            lattice, result.entry->surface, start_pos, end_pos, core::PartOfSpeech::Verb,
            getCategoryCost(core::ExtendedPOS::VerbRenyokei), core::LatticeEdge::kFromDictionary, verb_base, conj_type,
            core::ExtendedPOS::VerbRenyokei, "dictionary_godan_renyokei_before_predicate");
      }
    }
  }
  return true;
}

// Add the entry extended with colloquial emphasis at the cost and flags of its
// own edge.
void addEmphaticDictionaryEdge(core::Lattice& lattice,
                               const tokenizer_dictionary_detail::DictionaryCandidateContext& ctx,
                               const dictionary::LookupResult& result, size_t end_pos, float cost, uint8_t flags) {
  const auto& dict_manager = ctx.dict_manager;
  const auto& inflection = ctx.inflection;
  const std::string_view text = ctx.text;
  const auto& codepoints = ctx.codepoints;
  const auto& byte_offsets = ctx.byte_offsets;
  const size_t start_pos = ctx.start_pos;
  // Extend predicates, adverbs and particles with colloquial emphasis
  // (ですっ, 行くーー, きたあああ, 行くよっ). Unknown candidates use the same
  // matcher. A particle takes the mark for the same reason a predicate does —
  // it closes the utterance — and the bare-sokuon guard below is what keeps
  // the mark from being taken out of the next word (よっぽど, ねっとり).
  if (end_pos < codepoints.size() &&
      (result.entry->pos == core::PartOfSpeech::Verb || result.entry->pos == core::PartOfSpeech::Auxiliary ||
       result.entry->pos == core::PartOfSpeech::Adjective || result.entry->pos == core::PartOfSpeech::Adverb ||
       result.entry->pos == core::PartOfSpeech::Particle)) {
    // A dictionary irrealis stem cannot absorb っ before て/た as emphasis:
    // 染まっ+て belongs to the GodanRa verb 染まる, not 染ま(染む)+っ+て.
    // The hypothetical stem is barred for the same reason, and it is where
    // the productive potential forms are registered: かえ is the ichidan stem
    // of かえる (the potential of 買う), which has no sokuonbin at all, so
    // かえっ+て can only belong to the godan かえる and must keep that lemma.
    // An auxiliary cannot either: っ+て after one is the concessive particle
    // って (書い+た+って), and every genuine auxiliary onbin cell (だっ, たかっ,
    // じゃっ) is a dictionary entry in its own right.
    const bool sokuon_before_te_or_ta =
        end_pos + 1 < codepoints.size() && codepoints[end_pos] == core::hiragana::kSmallTsu &&
        (codepoints[end_pos + 1] == core::hiragana::kTe || codepoints[end_pos + 1] == core::hiragana::kTa) &&
        (result.entry->extended_pos == core::ExtendedPOS::VerbMizenkei ||
         result.entry->extended_pos == core::ExtendedPOS::VerbKateikei ||
         result.entry->pos == core::PartOfSpeech::Auxiliary);
    auto emphatic = sokuon_before_te_or_ta
                        ? verb_helpers::EmphaticSuffixMatch{}
                        : verb_helpers::matchEmphaticSuffix(codepoints, end_pos, result.entry->pos,
                                                            verb_helpers::SokuonOnsetPolicy::DictionaryEntry);
    // One repeated vowel is below the generic emphasis floor, but a final
    // particle drawn out by its own full-size vowel (けど+さあ) is that hold,
    // and so is a continuative closing the clause as the regional imperative
    // (見+ときい, し+ときい): an inflected i-row cell, then nothing.
    const bool closes_after_held_vowel =
        end_pos + 1 >= codepoints.size() ||
        normalize::classifyChar(codepoints[end_pos + 1]) == normalize::CharType::Symbol;
    const bool imperative_continuative =
        (result.entry->pos == core::PartOfSpeech::Verb || result.entry->pos == core::PartOfSpeech::Auxiliary) &&
        !result.entry->lemma.empty() && result.entry->lemma != result.entry->surface &&
        kana::isIRowCodepoint(codepoints[end_pos - 1]) && closes_after_held_vowel;
    if (emphatic.empty() &&
        (result.entry->extended_pos == core::ExtendedPOS::ParticleFinal || imperative_continuative) &&
        end_pos < codepoints.size() && codepoints[end_pos] == grammar::getVowelForChar(codepoints[end_pos - 1])) {
      emphatic.suffix = extractSubstring(codepoints, end_pos, end_pos + 1);
      emphatic.end = end_pos + 1;
      emphatic.repeated_vowel_count = 1;
    }
    // A bare sokuon after a predicate is one of two things: the genuine 促音便,
    // which needs て/た/で/だ behind it (と+いっ+て), or colloquial emphasis, which
    // closes the clause (行くっ！). Before any other kana it is neither, and taking
    // it eats the opening mora of the following word (にらめっ+こ for にらめっこ).
    const bool bare_sokuon = emphatic.suffix == "っ";
    // Only a verb's own 音便形 owns the sokuon in front of the connective. The
    // continuative does not: the 促音便 replaces that form's last mora rather
    // than following it (買う has 買っ, built on the stem, while 買い is the
    // continuative and 買いっ is no cell at all), and the genuine cell reaches
    // the lattice as an entry of its own. An i-adjective closes its terminal
    // on い and builds its own onbin elsewhere (忙し|かっ|た), and a na-adjective
    // stem has no inflection at all, so a っ after either is the emphatic —
    // which needs a clause end, not a following word (忙しい|っていう, not
    // 忙しいっ|ていう).
    const bool host_owns_sokuonbin_cell =
        result.entry->pos == core::PartOfSpeech::Verb && result.entry->extended_pos == core::ExtendedPOS::VerbOnbinkei;
    const bool unlicensed_bare_sokuon =
        bare_sokuon && emphatic.end < codepoints.size() &&
        normalize::classifyChar(codepoints[emphatic.end]) == normalize::CharType::Hiragana &&
        !(host_owns_sokuonbin_cell &&
          utf8::equalsAny(extractSubstring(codepoints, emphatic.end, emphatic.end + 1), {"て", "た", "で", "だ"}));
    // A particle takes the glottal stop, which closes the utterance, but not
    // the prolonged mark: after a one-mora particle that spelling is also the
    // tail of a lengthened word, and taking it there cuts the word in two
    // (おいしーー as おい + しーー).
    // A sentence-final particle may hold its own vowel at a clause end
    // (さ+あ, よ+お, さ+ー): one mora, the particle's vowel, then nothing.
    // The clause ends after the emphatic, or a final particle closes it
    // (行くけえ+ね).
    auto closes_clause_after_emphatic = [&]() {
      const auto* next_final = emphatic.end < codepoints.size()
                                   ? lookupEntryInRange(dict_manager, codepoints, emphatic.end, emphatic.end + 1,
                                                        core::PartOfSpeech::Particle)
                                   : nullptr;
      return emphatic.end >= codepoints.size() ||
             normalize::classifyChar(codepoints[emphatic.end]) == normalize::CharType::Symbol ||
             (next_final != nullptr && next_final->extended_pos == core::ExtendedPOS::ParticleFinal);
    };
    // A conjunctive particle of two morae or more left closing the clause
    // holds its vowel the same way (けど+ぉ, から+ー). Final particles stay
    // limited to one mora: a longer one (わい) is as often the tail of a word
    // whose own vowel is drawn out (か+わい+ー for かわいー).
    auto holds_final_particle_vowel = [&]() {
      const bool clause_closing_conjunctive = result.entry->extended_pos == core::ExtendedPOS::ParticleConj &&
                                              normalize::utf8Length(result.entry->surface) >= 2;
      const bool one_mora_final = result.entry->extended_pos == core::ExtendedPOS::ParticleFinal &&
                                  normalize::utf8Length(result.entry->surface) == 1;
      if ((!one_mora_final && !clause_closing_conjunctive) || emphatic.end != end_pos + 1 ||
          !closes_clause_after_emphatic()) {
        return false;
      }
      const char32_t held = codepoints[end_pos];
      const char32_t vowel = grammar::getVowelForChar(codepoints[end_pos - 1]);
      // Small vowels sit one codepoint below their full-size form (ぁ, あ).
      return held == U'ー' || held == vowel || (kana::isSmallKanaCodepoint(held) && held + 1 == vowel);
    };
    // Exactly two repeated vowels that themselves spell a dictionary word
    // starting there (で+ええ, そう+ああ) are that word, not emphasis.
    auto lengthening_spells_word_at = [&]() {
      // A content word draws its own vowel out (やばいいい); only a function
      // word's "lengthening" can be a following word instead.
      const bool function_word_host =
          result.entry->pos == core::PartOfSpeech::Particle || result.entry->pos == core::PartOfSpeech::Auxiliary;
      if (!function_word_host || emphatic.repeated_vowel_count != 2 || emphatic.standard_char_count != 0) {
        return false;
      }
      const auto following_results = dict_manager.lookup(text, byteOffsetAt(byte_offsets, end_pos));
      return std::any_of(following_results.begin(), following_results.end(),
                         [](const auto& following) { return following.entry != nullptr && following.length == 2; });
    };
    // A one-mora host whose vowel is held with ー or a small vowel can respell
    // a registered two-mora word (ね+ー, ね+ぇ for ねえ): the span is that
    // word, with its own class and lemma, and not the host drawn out.
    if (normalize::utf8Length(result.entry->surface) == 1 && emphatic.end == end_pos + 1) {
      const char32_t held = codepoints[end_pos];
      const char32_t vowel = grammar::getVowelForChar(codepoints[end_pos - 1]);
      if (held == U'ー' || (kana::isSmallKanaCodepoint(held) && held + 1 == vowel)) {
        const std::string respelled = result.entry->surface + normalize::encodeUtf8(vowel);
        bool respells_word = false;
        for (const auto& word : dict_manager.lookup(respelled, 0)) {
          if (word.entry == nullptr || word.length != normalize::utf8Length(respelled)) {
            continue;
          }
          respells_word = true;
          // Each host reading at this position reaches here; one of them adds the word.
          if (result.entry->pos == core::PartOfSpeech::Particle) {
            tokenizer_dictionary_detail::addDictionaryOriginEdge(
                lattice, extractSubstring(codepoints, start_pos, emphatic.end), start_pos, emphatic.end,
                word.entry->pos, getCategoryCost(word.entry->extended_pos), core::LatticeEdge::kFromDictionary,
                word.entry->lemma.empty() ? respelled : word.entry->lemma, dictionary::ConjugationType::None,
                word.entry->extended_pos, "dict_respelled");
          }
        }
        if (respells_word) {
          return;
        }
      }
    }
    // A vowel drawn out after a verb continuative, or after an auxiliary cell
    // that has no い to take, can instead complete an i-adjective spelled in
    // kana (おい+し+い is おいしい, つつ+まし+い is つつましい), so the span from a
    // hiragana run in front of it reads as an adjective.
    auto lengthening_completes_adjective = [&]() {
      constexpr size_t kMaxAdjectiveLookback = 6;
      // An auxiliary cell that takes the continuative い of いる (し+とき+い) is
      // a chain there, not the opening of an adjective.
      const bool auxiliary_without_i_cell =
          result.entry->pos == core::PartOfSpeech::Auxiliary &&
          BigramTable::getCost(result.entry->extended_pos, core::ExtendedPOS::AuxAspectIru) >= bigram_cost::kNever;
      if ((result.entry->pos != core::PartOfSpeech::Verb && !auxiliary_without_i_cell) ||
          emphatic.standard_char_count != 0 || emphatic.repeated_vowel_count != 1 || start_pos == 0) {
        return false;
      }
      size_t run_start = start_pos;
      while (run_start > 0 && start_pos - run_start < kMaxAdjectiveLookback &&
             normalize::classifyChar(codepoints[run_start - 1]) == normalize::CharType::Hiragana) {
        --run_start;
      }
      for (size_t from = run_start; from < start_pos; ++from) {
        const auto& spans = analysesInRange(inflection, codepoints, from, emphatic.end);
        if (std::any_of(spans.begin(), spans.end(), [](const grammar::InflectionCandidate& inflection_candidate) {
              return inflection_candidate.verb_type == grammar::VerbType::IAdjective &&
                     inflection_candidate.confidence >= candidate::kIAdjConfMin;
            })) {
          return true;
        }
      }
      return false;
    };
    const bool lengthening_spells_word = lengthening_spells_word_at() || lengthening_completes_adjective();
    const bool unlicensed_particle_lengthening =
        result.entry->pos == core::PartOfSpeech::Particle && !bare_sokuon && !holds_final_particle_vowel();
    // A conjugated word drawn out with the prolonged mark closes the
    // utterance; a following word means the mark belongs to that word instead
    // (しー+ん is the mimetic しーん, not する continuative plus ん).
    const bool unlicensed_open_prolongation =
        (result.entry->pos == core::PartOfSpeech::Verb || result.entry->pos == core::PartOfSpeech::Auxiliary) &&
        utf8::endsWith(emphatic.suffix, "ー") && !closes_clause_after_emphatic();
    if (!emphatic.empty() && !unlicensed_bare_sokuon && !unlicensed_particle_lengthening &&
        !unlicensed_open_prolongation && !lengthening_spells_word) {
      // Determine extended_pos for emphatic form
      // A sokuon on a continuative reads as its onbin cell (い → いっ for
      // と+いっ+て); on a finished form (待て+っ) it is only emphasis, and the
      // form keeps its own cell.
      core::ExtendedPOS emphatic_epos = result.entry->extended_pos;
      if (result.entry->pos == core::PartOfSpeech::Verb && emphatic.suffix == "っ" &&
          result.entry->extended_pos == core::ExtendedPOS::VerbRenyokei) {
        // E.g., い(連用形) + っ → いっ(音便形) for と+いっ+て pattern
        emphatic_epos = core::ExtendedPOS::VerbOnbinkei;
      }

      const std::string emphatic_surface = result.entry->surface + emphatic.suffix;
      // Emphasis adds nothing to the word, so the entry's own base form is the
      // lemma whatever the host (ですっ → です, すごいいいい → すごい).
      const std::string_view emphatic_lemma =
          result.entry->lemma.empty() ? std::string_view(result.entry->surface) : std::string_view(result.entry->lemma);
      lattice.addEdge(emphatic_surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(emphatic.end),
                      result.entry->pos, cost + verb_helpers::emphaticCostAdjustment(emphatic), flags, emphatic_lemma,
                      dictionary::ConjugationType::None, core::CandidateOrigin::Dictionary, 1.0F, {}, emphatic_epos,
                      "dict_emphatic");
    }
  }
}

// Add the entry's own edge together with the context-licensed readings that
// share its cost.
void addDictionaryEntryEdges(core::Lattice& lattice, const tokenizer_dictionary_detail::DictionaryCandidateContext& ctx,
                             const dictionary::LookupResult& result, size_t end_pos, std::string_view following_text) {
  const auto& dict_manager = ctx.dict_manager;
  const auto& codepoints = ctx.codepoints;
  const size_t start_pos = ctx.start_pos;
  const auto& lookup_results = ctx.lookup_results;
  const size_t longest_interjection = ctx.longest_interjection;
  uint8_t flags = core::LatticeEdge::kFromDictionary;
  if (result.from_user_dict) {
    flags |= core::LatticeEdge::kFromUserDict;
  }
  if (result.entry->extended_pos == core::ExtendedPOS::NounFormal) {
    flags |= core::LatticeEdge::kIsFormalNoun;
  }

  float cost = analysis::getCategoryCost(result.entry->extended_pos);
  // A tuned cost is flagged so the scorer honours it even when it lands on
  // exactly 0.0, which would otherwise read as unset.
  const auto add_custom_cost = [&cost, &flags](float adjustment) {
    cost += adjustment;
    flags |= core::LatticeEdge::kHasCustomCost;
  };

  if (result.entry->pos == core::PartOfSpeech::Noun && result.length >= 2 &&
      grammar::isAllKanji(result.entry->surface)) {
    add_custom_cost(candidate::kVerifiedMultiCharacterNounBonus);
  }

  if (result.entry->extended_pos == core::ExtendedPOS::PronounInterrogative && result.length >= longest_interjection) {
    add_custom_cost(candidate::kInterrogativePronounBonus);
  }

  if (result.entry->pos == core::PartOfSpeech::Verb &&
      result.entry->extended_pos == core::ExtendedPOS::VerbShuushikei &&
      utf8::endsWith(result.entry->surface, "せる")) {
    add_custom_cost(candidate::kLexicalSeruBaseBonus);
  }

  if (result.entry->extended_pos == core::ExtendedPOS::NounFormal && end_pos + 1 < codepoints.size() &&
      codepoints[end_pos] == U'で' && (codepoints[end_pos + 1] == U'は' || codepoints[end_pos + 1] == U'も')) {
    add_custom_cost(candidate::kFormalNounCopularTopicBonus);
  }

  if (result.entry->pos == core::PartOfSpeech::Adverb && end_pos + 1 < codepoints.size() &&
      codepoints[end_pos] == U'な' && codepoints[end_pos + 1] == U'の') {
    add_custom_cost(candidate::kAdverbExplanatoryCopulaBonus);
  }

  // In the explanatory interrogative opener, an adverb ends before the
  // sentence-final question particle and quotative predicate (なぜ+かというと).
  // Keep this productive boundary available instead of preferring an
  // accidental lexicalized adverb that absorbs か.
  if (result.entry->pos == core::PartOfSpeech::Adverb &&
      grammar::startsInterrogativeQuoteIntroduction(following_text)) {
    add_custom_cost(candidate::kInterrogativeQuoteIntroductionBonus);
  }

  // A dictionary-backed mixed-script noun can be a lexicalized compound
  // containing an inflected verbal segment. Prefer that registered search
  // unit over a coincidental inflection path.
  if (result.entry->pos == core::PartOfSpeech::Noun && result.length >= 3) {
    bool has_kanji = false;
    bool has_hiragana = false;
    for (size_t idx = start_pos; idx < end_pos; ++idx) {
      has_kanji = has_kanji || normalize::isKanjiCodepoint(codepoints[idx]);
      has_hiragana = has_hiragana || kana::isHiraganaCodepoint(codepoints[idx]);
    }
    const bool ichidan_predicate_continuation =
        has_kanji && has_hiragana && end_pos < codepoints.size() &&
        dict_manager.lookupExact(result.entry->surface + "る", core::PartOfSpeech::Verb) != nullptr &&
        (codepoints[end_pos] == U'て' ||
         (end_pos + 1 < codepoints.size() && codepoints[end_pos] == U'ら' && codepoints[end_pos + 1] == U'れ'));
    if (has_kanji && has_hiragana && !ichidan_predicate_continuation) {
      add_custom_cost(candidate::kLexicalizedMixedScriptNounBonus);
    }
  }

  const bool is_fused_demo = result.length == 2 && end_pos >= 2 && codepoints[end_pos - 2] == U'で' &&
                             codepoints[end_pos - 1] == U'も' &&
                             result.entry->extended_pos == core::ExtendedPOS::ParticleAdverbial;
  if (is_fused_demo && verb_helpers::naiNegativeFollowsAt(codepoints, end_pos) &&
      hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::AdjNaAdj)) {
    return;
  }

  // A bare え-row dict-verb imperative closing a clause (書け, 止まれ) is the 命令形 of the
  // base verb, not the potential-verb renyokei; without this the spurious 未然+受身れ split
  // (止ま+れ, lemma 止む) wins. Gated so any auxiliary/ば continuation (走れます/走れば/止まれる)
  // leaves the connection scores byte-identical.
  if (result.entry->pos == core::PartOfSpeech::Verb &&
      (result.entry->extended_pos == core::ExtendedPOS::VerbKateikei ||
       result.entry->extended_pos == core::ExtendedPOS::VerbMeireikei) &&
      grammar::containsKanji(result.entry->surface)) {
    const bool continues = end_pos < codepoints.size() &&
                           (codepoints[end_pos] == U'ば' ||
                            verb_helpers::isPassiveAuxContinuation(codepoints, end_pos, /*strict_masu=*/true));
    if (!continues) {
      add_custom_cost(candidate::verb_cost::kImperativeFinalBonus);
    }
  }

  // A single-token godan potential (読める) is analyzed as an independent ichidan verb, so its
  // lemma is its surface. The boost lets that dict form beat an unrelated ichidan reading. Excluded: independent
  // ichidan verbs (割れる==割れる have lemma == surface, and 自他 pairs like 切れる are registered
  // as ICHIDAN so no potential form is generated); られる passive/potential (来られる); and
  // irregular L1 forms whose lemma differs for other reasons (す→する) that do not end え-row + る.
  const bool is_godan_potential =
      result.entry->pos == core::PartOfSpeech::Verb &&
      result.entry->extended_pos == core::ExtendedPOS::VerbShuushikei &&
      std::string_view(result.entry->lemma) != std::string_view(result.entry->surface) &&
      utf8::endsWith(result.entry->surface, "る") && !utf8::endsWith(result.entry->surface, "られる") &&
      grammar::endsWithERow(
          std::string_view(result.entry->surface).substr(0, result.entry->surface.size() - core::kJapaneseCharBytes));
  if (is_godan_potential) {
    add_custom_cost(candidate::verb_cost::kImperativeFinalBonus);
  }

  const std::string_view lemma =
      is_godan_potential ? std::string_view(result.entry->surface) : std::string_view(result.entry->lemma);
  dictionary::ConjugationType conj_type = dictionary::ConjugationType::None;
  // Dictionary entries deliberately omit conjugation metadata. For a verb
  // whose dictionary-form ending uniquely identifies a Godan row, preserve
  // that information on the lattice edge so a low-cost dictionary match does
  // not discard the type carried by an equivalent generated candidate.
  if (result.entry->pos == core::PartOfSpeech::Verb && !lemma.empty()) {
    const char32_t final_cp = utf8::decodeFirstChar(utf8::lastChar(lemma));
    conj_type = grammar::verbTypeToConjType(grammar::verbTypeFromBaseCodepoint(final_cp));
    // The る-final row is unknown from the lemma, but stem+よ is a cell only
    // the ichidan paradigm has (あきらめ+よ), never a godan-ra one.
    const std::string_view surface = result.entry->surface;
    if (conj_type == dictionary::ConjugationType::None && utf8::endsWith(lemma, "る") &&
        utf8::endsWith(surface, "よ") &&
        surface.substr(0, surface.size() - core::kJapaneseCharBytes) ==
            lemma.substr(0, lemma.size() - core::kJapaneseCharBytes)) {
      conj_type = dictionary::ConjugationType::Ichidan;
    }
  }

  // A godan e-row form followed by past た cannot be a conditional or an
  // imperative; it is the continuative stem of the derived potential verb
  // (書け+た, 見渡せ+た). Keep the dictionary's conditional edge for ば,
  // and add this context-licensed potential edge without registering every
  // productive potential form as a separate verb.
  if (result.entry->pos == core::PartOfSpeech::Verb && result.entry->extended_pos == core::ExtendedPOS::VerbKateikei &&
      end_pos < codepoints.size() && codepoints[end_pos] == U'た' && grammar::endsWithERow(result.entry->surface)) {
    tokenizer_dictionary_detail::addDictionaryOriginEdge(
        lattice, result.entry->surface, start_pos, end_pos, core::PartOfSpeech::Verb,
        getCategoryCost(core::ExtendedPOS::VerbRenyokei), core::LatticeEdge::kFromDictionary,
        normalize::concat(result.entry->surface, "る"), dictionary::ConjugationType::Ichidan,
        core::ExtendedPOS::VerbRenyokei, "dictionary_potential_renyokei_before_past");
  }
  // A listed verb's continuative right after a noun and before a nominal
  // particle is the deverbal head of an object-verb compound (水+やり+を),
  // the same re-reading an unlisted continuative gets in the unknown-word
  // path (草+むしり+を), under the same gates: a bound suffix verb stays bound
  // to its host (手+がかり), and a listed non-verb reading of the span keeps it
  // (走り+まくり). A one-mora cell is the tail of too many other words
  // (美しい, 夜深し), and a listed word reaching over the host already owns
  // the span (気持ち). The noun itself is not listed, so it is not priced as
  // a dictionary noun.
  if (result.entry->pos == core::PartOfSpeech::Verb && result.entry->extended_pos == core::ExtendedPOS::VerbRenyokei &&
      end_pos >= start_pos + 2 && end_pos < codepoints.size() &&
      hasNominalForcingParticleContinuation(codepoints, end_pos, &dict_manager) &&
      hasPrecedingNominal(lattice, start_pos) &&
      !hasDictionaryEntrySpanningBack(dict_manager, codepoints, start_pos, end_pos) &&
      !verb_helpers::isBoundSuffixAfterNominalHost(&dict_manager, codepoints, start_pos, result.entry->surface) &&
      std::none_of(lookup_results.begin(), lookup_results.end(), [&](const dictionary::LookupResult& other) {
        return other.entry != nullptr && other.length == result.length && other.entry->pos != core::PartOfSpeech::Verb;
      })) {
    lattice.addEdge(result.entry->surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(end_pos),
                    core::PartOfSpeech::Noun, cost, 0, result.entry->surface, dictionary::ConjugationType::None,
                    core::CandidateOrigin::NominalizedNoun, candidate::kNoOriginConfidence, {},
                    core::ExtendedPOS::NounVerbal, "dictionary_renyokei_nominalized_after_noun");
  }
  // An auxiliary cell spelled with a final sokuon is an onbin form, and what
  // it can connect to follows from the paradigm it belongs to. The past た is
  // always available. The connective て needs a paradigm that has a te-form at
  // all: an auxiliary inflected as a Godan verb does (たがっ+て), while the
  // copula's continuative is で and it has no such cell, so its onbin before て
  // is really the plain form plus the quotative (無理|だ|って, not 無理|だっ|て).
  const bool auxiliary_inflects_as_godan =
      grammar::isModernGodanTerminalKana(utf8::decodeLastChar(result.entry->lemma));
  const bool unlicensed_auxiliary_onbin = result.entry->pos == core::PartOfSpeech::Auxiliary &&
                                          utf8::endsWith(result.entry->surface, "っ") && end_pos < codepoints.size() &&
                                          !auxiliary_inflects_as_godan &&
                                          !utf8::equalsAny(extractSubstring(codepoints, end_pos, end_pos + 1), {"た"});
  if (!unlicensed_auxiliary_onbin) {
    const core::ExtendedPOS entry_epos = namesSimplexDeverbalNoun(dict_manager, *result.entry)
                                             ? core::ExtendedPOS::NounVerbal
                                             : result.entry->extended_pos;
    lattice.addEdge(result.entry->surface, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(end_pos),
                    result.entry->pos, cost, flags, lemma, conj_type, core::CandidateOrigin::Dictionary, 1.0F, {},
                    entry_epos, "dict");
  }

  addEmphaticDictionaryEdge(lattice, ctx, result, end_pos, cost, flags);
}

}  // namespace

void Tokenizer::addDictionaryCandidates(core::Lattice& lattice, std::string_view text,
                                        const std::vector<char32_t>& codepoints, const ByteOffsets& byte_offsets,
                                        size_t start_pos, std::vector<dictionary::LookupResult>& lookup_results) const {
  const size_t byte_pos = byteOffsetAt(byte_offsets, start_pos);
  dict_manager_.lookupInto(text, byte_pos, lookup_results);
  const bool suppress_prefixed_noun_interior =
      startsHonorificPrefixedNounWithVerbTail(dict_manager_, text, codepoints, byte_offsets, start_pos);

  const ContextualDictionaryCandidateState contextual_candidates =
      addContextualDictionaryCandidates(lattice, dict_manager_, text, codepoints, byte_offsets, start_pos);
  const bool has_attributive_temporal_ma = contextual_candidates.has_attributive_temporal_ma;
  const bool starts_shortened_causative_passive = contextual_candidates.starts_shortened_causative_passive;

  size_t longest_conjunction = 0;
  size_t longest_fixed_conjunction = 0;
  size_t longest_interjection = 0;
  size_t longest_adverb = 0;
  size_t longest_noun = 0;
  size_t longest_potential_benefactive = 0;
  for (const auto& result : lookup_results) {
    if (result.entry == nullptr) {
      continue;
    }
    if (result.entry->pos == core::PartOfSpeech::Conjunction) {
      longest_conjunction = std::max(longest_conjunction, result.length);
      // A conjunction whose surface also spells a productive chain does not own
      // its span the way a fixed expression does: でも and では are the copula
      // continuative with a binding particle, and the と-final members are a
      // predicate plus the conditional と. Both readings compete for the same
      // characters at a sentence start too (ではあるまいか is で+は+ある+まい+か),
      // so these must not suppress the shorter auxiliary prefix below. The word
      // scorer excludes the same two classes from the fixed-expression bonus.
      if (!grammar::isCopulaFusedConjunction(result.entry->surface) &&
          !grammar::isConditionalToConjunction(result.entry->surface)) {
        longest_fixed_conjunction = std::max(longest_fixed_conjunction, result.length);
      }
    }
    if (result.entry->pos == core::PartOfSpeech::Interjection) {
      longest_interjection = std::max(longest_interjection, result.length);
    }
    if (result.entry->pos == core::PartOfSpeech::Adverb && !has_attributive_temporal_ma &&
        !adverbAbsorbsQuotedQuestion(codepoints, result.length, start_pos + result.length)) {
      longest_adverb = std::max(longest_adverb, result.length);
    }
    if (result.entry->pos == core::PartOfSpeech::Noun) {
      longest_noun = std::max(longest_noun, result.length);
    }
    if (grammar::isPotentialBenefactiveLemma(result.entry->lemma)) {
      longest_potential_benefactive = std::max(longest_potential_benefactive, result.length);
    }
  }

  const tokenizer_dictionary_detail::DictionaryCandidateContext ctx{dict_manager_,
                                                                    inflection_,
                                                                    text,
                                                                    codepoints,
                                                                    byte_offsets,
                                                                    start_pos,
                                                                    byte_pos,
                                                                    lookup_results,
                                                                    has_attributive_temporal_ma,
                                                                    starts_shortened_causative_passive,
                                                                    suppress_prefixed_noun_interior,
                                                                    longest_conjunction,
                                                                    longest_fixed_conjunction,
                                                                    longest_interjection,
                                                                    longest_adverb,
                                                                    longest_noun,
                                                                    longest_potential_benefactive};

  for (const auto& result : lookup_results) {
    if (result.entry == nullptr) {
      continue;
    }

    // Calculate end position in characters before context-sensitive candidate
    // guards below inspect the following lexical head.
    const size_t end_pos = start_pos + result.length;
    const std::string_view following_text = text.substr(byteOffsetAt(byte_offsets, end_pos));

    if (crossesEstablishedBoundary(ctx, lattice, result, end_pos) ||
        losesHomographReading(ctx, lattice, result, end_pos, following_text) ||
        isOutOfPlaceForWordClass(ctx, lattice, result, end_pos)) {
      continue;
    }
    if (!addDictionaryVerbStemEdges(lattice, ctx, result, end_pos)) {
      continue;
    }
    if (lacksLicensingEnvironment(ctx, lattice, result, end_pos, following_text)) {
      continue;
    }
    addDictionaryEntryEdges(lattice, ctx, result, end_pos, following_text);
  }

  tokenizer_dictionary_detail::appendSpecialGrammarCandidates(lattice, text, codepoints, start_pos, byte_pos);
  addElidedProlongedDictionaryCandidates(lattice, dict_manager_, codepoints, start_pos);
  addTruncatedAdverbCandidates(lattice, dict_manager_, codepoints, start_pos);
  addClippedInterjectionCandidates(lattice, dict_manager_, codepoints, start_pos);
}

}  // namespace suzume::analysis