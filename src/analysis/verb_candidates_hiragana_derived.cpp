/**
 * @file verb_candidates_hiragana_derived.cpp
 * @brief Derived pure-hiragana verb candidate families
 */

#include <algorithm>
#include <cmath>

#include "analysis/bigram_table.h"
#include "analysis/candidate_constants.h"
#include "analysis/dictionary_probe.h"
#include "analysis/scorer_constants.h"
#include "analysis/tokenizer_utils.h"
#include "analysis/verb_candidates_absorption_guards.h"
#include "analysis/verb_candidates_auxiliary_patterns.h"
#include "analysis/verb_candidates_dictionary_probes.h"
#include "analysis/verb_candidates_hiragana_internal.h"
#include "analysis/verb_candidates_verb_stems.h"
#include "core/debug.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/auxiliaries.h"
#include "grammar/char_patterns.h"
#include "grammar/conjugation.h"
#include "grammar/connection.h"
#include "grammar/honorific_verbs.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "unknown.h"
#include "verb_candidates.h"

namespace suzume::analysis::hiragana_verb_detail {
namespace vh = verb_helpers;

namespace {

// Subsidiary verbs conjugate as verbs and share their cells with lexical verbs
// (いれ, おくれ, かね), unlike the closed non-verbal auxiliaries (まし, だっ).
bool isSubsidiaryVerbAuxiliary(core::ExtendedPOS extended_pos) {
  switch (extended_pos) {
    case core::ExtendedPOS::AuxPassive:
    case core::ExtendedPOS::AuxCausative:
    case core::ExtendedPOS::AuxPotential:
    case core::ExtendedPOS::AuxHonorific:
    case core::ExtendedPOS::AuxExcessive:
    case core::ExtendedPOS::AuxInability:
    case core::ExtendedPOS::AuxBenefactive:
      return true;
    default:
      return core::isAspectAuxiliaryType(extended_pos);
  }
}

bool startsWithRenyokeiAuxiliary(std::string_view following_surface) {
  for (const auto& auxiliary : grammar::getAuxiliaries()) {
    if (auxiliary.required_conn == grammar::conn::kVerbRenyokei &&
        utf8::startsWith(following_surface, auxiliary.surface)) {
      return true;
    }
  }
  return false;
}

}  // namespace

void appendHiraganaDerivedCandidates(const std::vector<char32_t>& codepoints, size_t start_pos, size_t hiragana_end,
                                     const std::vector<normalize::CharType>& char_types,
                                     const grammar::Inflection& inflection,
                                     const dictionary::DictionaryManager* dict_manager,
                                     std::vector<UnknownCandidate>& candidates) {
  // Generate Godan mizenkei stem candidates for hiragana passive patterns
  // E.g., いわれる → いわ (mizenkei of いう) + れる (passive AUX)
  appendPassiveMizenkeiCandidates(codepoints, start_pos, hiragana_end, inflection, dict_manager, candidates);

  // Generate Ichidan verb stem candidates for hiragana られる pattern
  // E.g., いられる → い (renyokei of いる) + られる (potential/passive AUX)
  appendIchidanRareruCandidates(codepoints, start_pos, hiragana_end, inflection, dict_manager, candidates);

  // Generate Godan mizenkei stem candidates for contracted negative ん pattern
  // E.g., くだらん → くだら (mizenkei of くだる) + ん (contracted negative)
  appendMizenkeiNCandidates(codepoints, start_pos, hiragana_end, dict_manager, candidates);

  // Generate Godan mizenkei stem candidates for negative auxiliary ない pattern
  // E.g., わからない → わから (mizenkei of わかる) + ない (negative auxiliary)
  appendMizenkeiNegativeCandidates(codepoints, start_pos, hiragana_end, inflection, dict_manager, candidates);

  // Generate Godan mizenkei stem candidates before なきゃ/なければ contraction
  // E.g., やらなきゃ → やら (mizenkei of やる) + なきゃ (contraction of なければ)
  appendMizenkeiNakyaCandidates(codepoints, start_pos, hiragana_end, inflection, dict_manager, candidates);

  // Generate Godan-ra ん音便 stem candidates for colloquial ん+ない pattern
  // E.g., たまんない → たまん (ん音便 of たまる) + ない (negative auxiliary)
  appendNOnbinNaiCandidates(codepoints, start_pos, hiragana_end, inflection, dict_manager, candidates);

  // Generate Godan onbin stem candidates for contraction auxiliary patterns
  // E.g., やっとく → やっ (onbin of やる) + とく (ておく contraction), 読んでる → 読ん + でる
  appendOnbinContractionCandidates(codepoints, start_pos, hiragana_end, inflection, dict_manager, candidates);

  // Generate 1-char ichidan renyokei stem candidates
  // E.g., ねて → ね (renyokei of ねる) + て (particle)
  appendIchidanRenyokei1CharCandidates(codepoints, start_pos, dict_manager, candidates);

  // Generate 2+ char ichidan renyokei stem candidates
  // E.g., つけて → つけ (renyokei of つける) + て (particle)
  //       たべて → たべ (renyokei of たべる) + て (particle)
  //       あけて → あけ (renyokei of あける) + て (particle)
  //       すぎて → すぎ (renyokei of すぎる) + て (particle)
  // MeCab splits: つけて → つけ(動詞,一段,連用形) + て(助詞,接続助詞)
  // Pattern: 2+ char sequence ending with e-row or i-row hiragana followed by て or た
  // Note: Ichidan verbs have both e-row stems (食べる) and i-row stems (感じる, 過ぎる)
  // Uses inflection analysis confidence to validate (dictionary lookup as bonus)
  //
  // A run that starts with で right after a hatsuonbin ん is the voiced te-form
  // particle of the preceding verb (読ん+で, 飲ん+で), so its でき is
  // で(particle)+き(くる), never the renyokei of できる. The continuative-form
  // licensers are suppressed for this shape (see the checks below), so
  // 読んできました stays 読ん+で+き+まし+た. The plain て/た path is deliberately
  // left intact — 取り組んできた already resolves to …+で+き+た on its own, and
  // blocking the reading there would instead flip it to でき+た.
  const bool leading_de_after_hatsuonbin =
      codepoints[start_pos] == U'で' && start_pos > 0 && codepoints[start_pos - 1] == U'ん';
  for (size_t end_pos = start_pos + 2; end_pos < hiragana_end; ++end_pos) {
    // Check if position end_pos-1 is e-row or i-row hiragana (ichidan renyokei ending)
    // E-row: 食べる, 見える → 食べ, 見え
    // I-row: 感じる, 過ぎる → 感じ, すぎ
    char32_t stem_end_char = codepoints[end_pos - 1];
    if (!kana::isERowCodepoint(stem_end_char) && !kana::isIRowCodepoint(stem_end_char)) {
      continue;
    }

    // Check if followed by te/ta particle, polite ます auxiliary, or conditional れば
    if (end_pos >= codepoints.size()) {
      continue;
    }
    char32_t next_char = codepoints[end_pos];
    bool is_followed_by_te_ta = (next_char == U'て' || next_char == U'た');
    // ながら / つつ
    const bool is_followed_by_renyokei_conj =
        (next_char == U'な' && end_pos + 2 < codepoints.size() && codepoints[end_pos + 1] == U'が' &&
         codepoints[end_pos + 2] == U'ら') ||
        (next_char == U'つ' && end_pos + 1 < codepoints.size() && codepoints[end_pos + 1] == U'つ');

    // て and で are more commonly particles. A stem closing on で stays open
    // only before た/て or ながら/つつ, which the copula で never takes (なで+た).
    if (stem_end_char == U'て' || (stem_end_char == U'で' && !is_followed_by_te_ta && !is_followed_by_renyokei_conj)) {
      continue;
    }
    // For Godan-ta, any auxiliary whose declared required connection is
    // VerbRenyokei licenses the continuative reading (もち+たい, たち+ます).
    // Derive this from the grammar table instead of enumerating ます/たい/etc.
    // Other rows retain their established, narrower gates because enabling every
    // Ichidan-looking stem here would fabricate verbs such as られ+ちゃう.
    const std::string following_surface = extractSubstring(codepoints, end_pos, hiragana_end);
    const bool is_followed_by_renyokei_aux =
        !leading_de_after_hatsuonbin && startsWithRenyokeiAuxiliary(following_surface);
    const auto* following_auxiliary =
        dict_manager != nullptr ? dict_manager->lookupExact(following_surface, core::PartOfSpeech::Auxiliary) : nullptr;
    const bool is_followed_by_classical_adnominal_tari =
        following_auxiliary != nullptr && following_auxiliary->extended_pos == core::ExtendedPOS::AuxClassicalTari;
    const bool is_followed_by_masu = vh::masuAuxFollowsAt(codepoints, end_pos) && !leading_de_after_hatsuonbin;
    const bool godan_ta_before_declared_renyokei_aux = stem_end_char == U'ち' && is_followed_by_renyokei_aux;
    // Conditional れば (できれば → でき + れ + ば), handled below for the kateikei stem
    const bool is_followed_by_reba =
        next_char == U'れ' && end_pos + 1 < codepoints.size() && codepoints[end_pos + 1] == U'ば';
    // The ichidan volitional opens on the irrealis stem+よ (あげ+よ+う).
    const bool is_followed_by_volitional = kana::isERowCodepoint(stem_end_char) && next_char == U'よ' &&
                                           end_pos + 1 < codepoints.size() &&
                                           codepoints[end_pos + 1] == core::hiragana::kU;
    // Negative ない (できない → でき + ない)
    const bool is_followed_by_nai =
        next_char == U'な' && end_pos + 1 < codepoints.size() && codepoints[end_pos + 1] == U'い';
    // The appearance そう takes a godan continuative (ふり+そう). An i-row し
    // is left out: it ends the stems of the しい adjectives (うれし+そう), as
    // does a stem whose い form is a dictionary adjective (おおき+そう).
    const bool godan_before_appearance_sou =
        kana::isIRowCodepoint(stem_end_char) && stem_end_char != U'し' && next_char == U'そ' &&
        end_pos + 1 < codepoints.size() && codepoints[end_pos + 1] == core::hiragana::kU &&
        !vh::isAdjectiveInDictionary(dict_manager, extractSubstring(codepoints, start_pos, end_pos) + "い");
    if (!is_followed_by_te_ta && !is_followed_by_masu && !godan_ta_before_declared_renyokei_aux &&
        !is_followed_by_renyokei_conj && !is_followed_by_classical_adnominal_tari && !is_followed_by_reba &&
        !is_followed_by_nai && !is_followed_by_volitional && !godan_before_appearance_sou) {
      continue;
    }

    // Construct stem and base form. Small-kana starts (っぱいし, ゃい, …) are
    // already rejected by the function-entry guard.
    std::string stem_surface = extractSubstring(codepoints, start_pos, end_pos);
    std::string base_form = stem_surface + "る";

    // Use inflection analysis to validate - check if stem is recognized as ichidan
    const auto& stem_analysis = inflection.analyze(stem_surface);
    bool found_ichidan = false;
    float ichidan_confidence = 0.0F;
    for (const auto& cand : stem_analysis) {
      if (cand.verb_type == grammar::VerbType::Ichidan && cand.base_form == base_form) {
        found_ichidan = true;
        ichidan_confidence = cand.confidence;
        break;
      }
    }

    // Skip if not recognized as ichidan stem by inflection analysis
    // Threshold 0.3 catches most valid cases while filtering noise. A で-final
    // stem reached this far is already licensed by its follower, so the
    // analyzer's te-form penalty on that shape says nothing here.
    if (!found_ichidan || (ichidan_confidence < 0.3F && stem_end_char != U'で')) {
      continue;
    }

    // Default to the ichidan interpretation (stem + る). A following auxiliary
    // that requires VerbRenyokei licenses the Godan continuative reading
    // (泳ぎ→泳ぐ, もち→もつ), so prefer it when at least as confident. Godan-sa is
    // the sole row whose ordinary continuative also attaches directly to て/た
    // (話し+た); preserve that analysis instead of fabricating 話しる. Other
    // Godan rows require onbin or an a-row mizenkei in those contexts.
    std::string chosen_base = base_form;
    dictionary::ConjugationType chosen_conj = dictionary::ConjugationType::Ichidan;
    float chosen_confidence = ichidan_confidence;
    const bool godan_sa_before_te_ta = is_followed_by_te_ta && stem_end_char == U'し';
    const bool stem_is_closed_auxiliary =
        vh::hasDictionaryEntry(dict_manager, stem_surface, core::PartOfSpeech::Auxiliary);
    const grammar::VerbType godan_type = grammar::verbTypeFromIRowCodepoint(stem_end_char);
    if (!stem_is_closed_auxiliary &&
        (is_followed_by_masu || is_followed_by_renyokei_conj || is_followed_by_classical_adnominal_tari ||
         godan_sa_before_te_ta || godan_ta_before_declared_renyokei_aux || godan_before_appearance_sou)) {
      if (godan_type != grammar::VerbType::Unknown) {
        std::string godan_base = extractSubstring(codepoints, start_pos, end_pos - 1) +
                                 std::string(grammar::godanBaseSuffixFromIRow(stem_end_char));
        for (const auto& cand : stem_analysis) {
          if (cand.verb_type == godan_type && cand.base_form == godan_base && cand.confidence >= chosen_confidence) {
            chosen_base = godan_base;
            chosen_conj = grammar::verbTypeToConjType(godan_type);
            chosen_confidence = cand.confidence;
            break;
          }
        }
      }
    }

    // Before そう only the godan reading is licensed (no ふりる).
    if (godan_before_appearance_sou && chosen_conj == dictionary::ConjugationType::Ichidan) {
      continue;
    }

    // Check if base form is in dictionary (gives confidence boost)
    bool is_dict_verb = vh::isVerbInDictionary(dict_manager, chosen_base);

    // A verb that exists only behind the honorific prefix お is not a stem on
    // its own: は+し+ます is the topic particle plus する, not はす.
    if (is_dict_verb && grammar::isPrefixBoundHonorificVerbLemma(chosen_base) &&
        !(start_pos > 0 && codepoints[start_pos - 1] == U'お')) {
      continue;
    }

    // A registered inflectional surface already supplies its own lemma and
    // conjugation type.  Do not overlay an unverified Ichidan reconstruction
    // such as あり→ありる on that closed lexical evidence. A surface registered
    // as another cell leaves this one open (いれ is the hypothetical of いる).
    const auto* registered_cell =
        is_dict_verb || dict_manager == nullptr
            ? nullptr
            : lookupEntryInRange(*dict_manager, codepoints, start_pos, end_pos, core::PartOfSpeech::Verb);
    if (registered_cell != nullptr && registered_cell->extended_pos != core::ExtendedPOS::VerbKateikei) {
      continue;
    }
    // A surface registered as a cell of a non-verbal auxiliary is that
    // closed-class word, not an unverified lexical continuative (支払い+まし+た,
    // not a verb ます). Subsidiary verbs share their cells with lexical verbs
    // (いれ, おくれ, かね) and stay open.
    if (!is_dict_verb && dict_manager != nullptr) {
      const auto* auxiliary_cell =
          lookupEntryInRange(*dict_manager, codepoints, start_pos, end_pos, core::PartOfSpeech::Auxiliary);
      if (auxiliary_cell != nullptr && !isSubsidiaryVerbAuxiliary(auxiliary_cell->extended_pos)) {
        continue;
      }
    }

    // Likewise, a short candidate beginning inside a registered verb is its
    // final morae rather than an independent predicate (しまい+ます, not
    // し+まい(まいる)+ます). A case particle behind a kanji host closes that
    // host instead (木+が+かれ, not がかれ).
    const bool follows_case_particle_on_kanji =
        start_pos > 1 && normalize::isKanjiCodepoint(codepoints[start_pos - 2]) &&
        vh::oneMoraParticleEndsAt(dict_manager, codepoints, start_pos, core::ExtendedPOS::ParticleCase);
    if (!is_dict_verb && end_pos - start_pos == 2 && start_pos > 0 && !follows_case_particle_on_kanji &&
        vh::isVerbInDictionary(dict_manager, codepoints, start_pos - 1, end_pos)) {
      continue;
    }

    // Skip causative+passive auxiliary chain patterns
    // E.g., "せられ" should be split as せ(causative) + られ(passive), not single verb
    // Preserve the causative, passive, and tense morpheme boundaries.
    if (utf8::endsWith(stem_surface, "せられ")) {
      continue;
    }

    // A passive cell followed by its next auxiliary is not an independent
    // lexical continuative.  In particular, a weak reconstruction of
    // やめられ must leave the productive やめ + られ + ない/ます boundary
    // available.  The same rule is structural, so it also covers other
    // hiragana Ichidan hosts without naming a lexical verb.
    if (utf8::endsWith(stem_surface, "られ") && (is_followed_by_nai || is_followed_by_masu)) {
      continue;
    }

    // Skip stems ending in なけ - this is the negative auxiliary ない kateikei (なけれ),
    // not an ichidan verb なける. Prevents a false single-verb reading for
    // mizenkei + なければ: やらなければ must split as やら + なけれ(ない) + ば,
    // never become a fabricated ichidan やらなける.
    if (utf8::endsWith(stem_surface, "なけ")) {
      continue;
    }

    // Skip stems ending in し where the prefix is a dictionary noun (サ変 pattern)
    // E.g., しっぱいし → しっぱい(dict NOUN) + し(する連用), not しっぱいしる
    // This prevents false ichidan candidates from サ変 noun + する patterns
    // Directly after the object or subject marker a one-mora prefix cannot be
    // the closed-class word it spells (を+だ, が+か), so there it is a verb
    // stem (手紙を+だし+た).
    if (!is_dict_verb && utf8::endsWith(stem_surface, "し") && stem_surface.size() > 3) {  // More than just し
      std::string prefix = stem_surface.substr(0, stem_surface.size() - 3);
      const bool follows_argument_marker = prefix.size() == core::kJapaneseCharBytes && start_pos > 0 &&
                                           (codepoints[start_pos - 1] == U'を' || codepoints[start_pos - 1] == U'が');
      if (!follows_argument_marker && vh::hasNonVerbDictionaryEntry(dict_manager, prefix)) {
        continue;
      }
      // Pure hiragana stems with sokuon ending in し are almost always
      // false サ変 patterns (noun+する where noun contains っ)
      if (utf8::contains(stem_surface, "っ")) {
        continue;
      }
    }

    // Skip て+subsidiary verb patterns that should be split
    // E.g., "してくれ" should be し + て + くれ, not single verb
    //       "してもら" should be し + て + もら, not single verb
    // These patterns contain て-form (して) followed by subsidiary verb stem
    if (utf8::containsAny(stem_surface, {"てくれ", "てもら", "てあげ"})) {
      continue;
    }

    // Skip te-form + subsidiary みる spans: an internal て/で followed by み is
    // always [verb te-form] + みる (やってみ = やっ + て + み, われてみ =
    // われ + て + み), never a single ichidan verb やってみる. This also
    // suppresses the kateikei variant below (やってみれ from やってみれば).
    // @see fabricated closed-class absorption guards (verb_candidates_absorption_guards.h)
    if (!is_dict_verb && vh::guardIsWired(vh::GuardMember::EmbedTeMiruAuxiliary, vh::GuardOrigin::HiraganaDerived) &&
        vh::embedsTeFormMiruAuxiliary(codepoints, start_pos, end_pos)) {
      continue;
    }

    // Skip a fabricated verb that spans an auxiliary prefix + auxiliary tail:
    // でござい → で(AuxCopulaDa) + ござい(AuxGozaru). MeCab always keeps a
    // closed-class auxiliary chain split, so an open-class verb whose leading
    // codepoint is itself an AUX and whose remainder after that codepoint is
    // exactly an AUX is re-merging what must stay apart. Both halves must be
    // dictionary auxiliaries: the remainder condition alone would wrongly skip
    // real verbs like しまう (し is a particle, not an AUX → しまい stays), and
    // the 2-codepoint floor on the remainder protects genuine short stems かい
    // (い = いる 連用形) and でき (き = くる 連用形).
    if (!is_dict_verb && dict_manager != nullptr && end_pos - start_pos >= 3) {
      std::string aux_remainder = extractSubstring(codepoints, start_pos + 1, end_pos);
      if (lookupEntryInRange(*dict_manager, codepoints, start_pos, start_pos + 1, core::PartOfSpeech::Auxiliary) !=
              nullptr &&
          vh::hasDictionaryEntry(dict_manager, aux_remainder, core::PartOfSpeech::Auxiliary)) {
        continue;
      }
    }

    // Strong negative cost to beat NOUN + て(VERB from てる) split
    // Dictionary-verified verbs get stronger bonus
    // Non-dictionary verbs get moderate positive cost to avoid spurious candidates
    // competing with dictionary compound particles like について
    // But not too high to break valid patterns like してほしい
    float cost = is_dict_verb ? -0.8F : 0.5F;
    const std::string following = extractSubstring(codepoints, end_pos, std::min(end_pos + 2, codepoints.size()));
    // An e-row stem before し+ます is an ichidan continuative plus する's し
    // (お+つたえ+し+ます), not the godan-sa continuative of a coined verb.
    if (!is_dict_verb && following == "ます" && end_pos >= start_pos + 3 && codepoints[end_pos - 1] == U'し' &&
        kana::isERowCodepoint(codepoints[end_pos - 2])) {
      continue;
    }
    // The polite auxiliary completes the renyokei frame for an otherwise
    // unregistered stem (くみ+ます). Its closed morphology is stronger evidence
    // than the generic bracketed-noun fallback after an object particle.
    if (!is_dict_verb && following == "ます") {
      cost += bigram_cost::kVeryStrongBonus;
    }
    // A godan-wa renyokei starting か…い immediately after a pronoun (誰かい, なにかい)
    // is spurious: the か is the particle か and い is いる's renyokei
    // (誰か + い + ます). Discourage it so the particle reading wins.
    if (codepoints[start_pos] == U'か' && pronounEndsAt(dict_manager, codepoints, start_pos)) {
      cost += bigram_cost::kStrong;
    }
    SUZUME_DEBUG_VERBOSE_BLOCK {
      SUZUME_DEBUG_STREAM << "[VERB_CAND] " << stem_surface << " hiragana_renyokei lemma=" << chosen_base
                          << " conf=" << chosen_confidence << (is_dict_verb ? " [dict]" : "") << " cost=" << cost
                          << "\n";
    }
    const bool is_negative_continuation = utf8::startsWithAny(following, {"ない", "なか"});
    // A stem after a clear te/de boundary belongs to a subsidiary-verb
    // construction.  Leave that category to its dedicated candidate so an
    // otherwise valid Ichidan reconstruction cannot turn 〜てやらない into a
    // lexical predicate.  Outside that boundary, the negative confirms that
    // the ambiguous Ichidan stem is mizenkei (さけ+ない, かけ+ない).
    const bool is_lexical_negative_continuation =
        is_negative_continuation && !isClearTeFormBeforeSubsidiary(codepoints, start_pos, true);
    // A following て/た/ます validates the inflectional shape, but it does not prove
    // a word boundary when the candidate begins immediately after kanji. In
    // that position the hiragana can instead be the okurigana tail of a
    // kanji-starting predicate. Keep the ordinary candidate, but reserve the
    // context-validated origin (and its strong auxiliary-connection evidence)
    // for starts that are not inside that mixed-script predicate shape.
    const bool has_kanji_immediately_before = start_pos > 0 && normalize::isKanjiCodepoint(codepoints[start_pos - 1]);
    const core::CandidateOrigin origin =
        is_lexical_negative_continuation
            ? CandidateOrigin::VerbHiraganaNegativeRenyokei
            : ((is_followed_by_te_ta || is_followed_by_masu || is_followed_by_renyokei_conj) &&
                       !has_kanji_immediately_before
                   ? CandidateOrigin::VerbHiraganaInflectedRenyokei
                   : CandidateOrigin::VerbHiragana);
    // Ichidan stems share their surface in renyokei and mizenkei. A following
    // negative auxiliary determines the latter, which must receive the normal
    // VerbMizenkei → AuxNegativeNai connection instead of competing as a
    // continuative verb (さけ+ない, かけ+ない).
    const core::ExtendedPOS extended_pos =
        is_lexical_negative_continuation ? core::ExtendedPOS::VerbMizenkei : core::ExtendedPOS::VerbRenyokei;
    // The する irrealis さ plus the passive is a voice chain, not the stem of a
    // coined ichidan verb (と+さ+れ+て, と+さ+れれ+ば), as the inflected path holds.
    const auto embeds_suru_passive = [&](size_t chain_end) {
      return chain_end <= codepoints.size() &&
             vh::shouldSkipPassiveAuxPattern(extractSubstring(codepoints, start_pos, chain_end),
                                             grammar::VerbType::Suru);
    };
    if (!is_dict_verb && (embeds_suru_passive(end_pos + 1) || embeds_suru_passive(end_pos + 2))) {
      continue;
    }
    if (!is_dict_verb && (vh::endsWithFocusParticleTail(dict_manager, codepoints, start_pos, end_pos) ||
                          vh::closesOnPoliteCopula(dict_manager, codepoints, start_pos, end_pos) ||
                          vh::coinedVerbOpensOnArgumentParticle(dict_manager, inflection, codepoints, start_pos,
                                                                end_pos, chosen_confidence))) {
      continue;
    }
    candidates.push_back(makeVerbCandidate(stem_surface, start_pos, end_pos, cost, chosen_base, chosen_conj, true,
                                           origin, chosen_confidence, "hiragana_renyokei", extended_pos));

    // Also generate kateikei stem if followed by れば
    // E.g., できれば → できれ (kateikei of できる) + ば
    // MeCab splits: できれば → できれ(動詞,仮定形) + ば(接続助詞)
    // Skip suru-verb negative patterns: しなけれ should be し + なけれ, not single verb
    // Pattern: し + な (negative stem prefix)
    // stem_surface = しなけ → base_form = しなける (false ichidan)
    const bool is_suru_negative_pattern = utf8::startsWith(stem_surface, "しな");
    const bool embeds_te_conditional_auxiliary =
        is_followed_by_reba && vh::embedsTeFormVerbCell(dict_manager, codepoints, start_pos, end_pos + 1);
    if (is_followed_by_reba && !is_suru_negative_pattern && !embeds_te_conditional_auxiliary) {
      std::string kateikei_surface = stem_surface + "れ";  // 連用形 + れ = 仮定形
      size_t kateikei_end = end_pos + 1;                   // renyokei + れ
      constexpr float kKateikeiCost = candidate::verb_cost::kStrongBonus;
      SUZUME_DEBUG_VERBOSE_BLOCK {
        SUZUME_DEBUG_STREAM << "[VERB_CAND] " << kateikei_surface << " hiragana_ichidan_kateikei lemma=" << base_form
                            << " conf=" << ichidan_confidence << " cost=" << kKateikeiCost << "\n";
      }
      candidates.push_back(makeVerbCandidate(kateikei_surface, start_pos, kateikei_end, kKateikeiCost, base_form,
                                             dictionary::ConjugationType::Ichidan, true, CandidateOrigin::VerbHiragana,
                                             ichidan_confidence, "hiragana_ichidan_kateikei",
                                             core::ExtendedPOS::VerbKateikei));
    }
    if (is_followed_by_volitional) {
      SUZUME_DEBUG_VERBOSE_BLOCK {
        SUZUME_DEBUG_STREAM << "[VERB_CAND] " << stem_surface << "よ hiragana_ichidan_volitional lemma=" << base_form
                            << " conf=" << ichidan_confidence << " cost=" << cost << "\n";
      }
      candidates.push_back(makeVerbCandidate(stem_surface + "よ", start_pos, end_pos + 1, cost, base_form,
                                             dictionary::ConjugationType::Ichidan, true, CandidateOrigin::VerbHiragana,
                                             ichidan_confidence, "hiragana_ichidan_volitional",
                                             core::ExtendedPOS::VerbMizenkei));
    }
  }

  // Generate dictionary-verified Godan っ/ん onbin candidates before their
  // respective te/past continuations.  The general onbin generator owns all
  // unregistered-stem fallbacks, so this path emits only a verified lemma.
  const size_t hira_extent_end =
      findCharRegionEnd(char_types, start_pos, candidate::kMaxHiraganaOnbinProbeChars, normalize::CharType::Hiragana);
  if (hira_extent_end - start_pos >= 3) {
    const char32_t onbin_char = codepoints[hira_extent_end - 2];
    const char32_t continuation = codepoints[hira_extent_end - 1];
    const bool is_sokuonbin = onbin_char == U'っ' && (continuation == U'た' || continuation == U'て');
    const bool is_hatsuonbin = onbin_char == U'ん' && (continuation == U'だ' || continuation == U'で');
    if (is_sokuonbin || is_hatsuonbin) {
      const size_t onbin_end = hira_extent_end - 1;
      const std::string onbin_surface = extractSubstring(codepoints, start_pos, onbin_end);
      const std::string stem = extractSubstring(codepoints, start_pos, onbin_end - 1);
      const std::string_view onbin = is_sokuonbin ? "っ" : "ん";
      auto onbin_match = vh::firstGodanOnbinDictBase(dict_manager, stem, onbin);
      // The base must also inflect as the Godan row the onbin implies (no する from すっ+て).
      if (is_sokuonbin && onbin_match.matched &&
          (grammar::isSuruBaseForm(onbin_match.base_form) ||
           !vh::readsAsBaseForm(inflection, onbin_match.base_form, onbin_match.base_form, onbin_match.verb_type))) {
        onbin_match.matched = false;
      }
      // 来い|って: the っ belongs to the quotative after a finished predicate.
      if (is_sokuonbin && onbin_match.matched &&
          vh::closedPredicateEndsAt(dict_manager, codepoints, start_pos, onbin_end - 1, true)) {
        onbin_match.matched = false;
      }
      if (onbin_match.matched) {
        constexpr float kHiraganaOnbinCost = candidate::verb_cost::kStandardBonus;
        const char* origin = is_sokuonbin ? "hiragana_sokuonbin" : "hiragana_hatsuonbin";
        SUZUME_DEBUG_VERBOSE_BLOCK {
          SUZUME_DEBUG_STREAM << "[VERB_CAND] " << onbin_surface << ' ' << origin << " lemma=" << onbin_match.base_form
                              << " type=" << grammar::verbTypeToString(onbin_match.verb_type)
                              << " cost=" << kHiraganaOnbinCost << "\n";
        }
        auto onbin_candidate =
            makeVerbCandidate(onbin_surface, start_pos, onbin_end, kHiraganaOnbinCost, onbin_match.base_form,
                              grammar::verbTypeToConjType(onbin_match.verb_type), true, CandidateOrigin::VerbHiragana,
                              candidate::kHighOriginConfidence, origin, core::ExtendedPOS::VerbOnbinkei);
        onbin_candidate.lemma_verified = true;
        candidates.push_back(std::move(onbin_candidate));
      }
    }
  }
}

}  // namespace suzume::analysis::hiragana_verb_detail
