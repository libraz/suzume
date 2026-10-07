/**
 * @file tokenizer_dictionary_licensing_guards.cpp
 * @brief Dictionary function words and paradigm cells whose licensing host or follower is absent
 */

#include <algorithm>

#include "analysis/dictionary_probe.h"
#include "candidate_constants.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "grammar/honorific_verbs.h"
#include "normalize/utf8.h"
#include "tokenizer_dictionary_internal.h"
#include "verb_candidates_helpers.h"

namespace suzume::analysis::tokenizer_dictionary_detail {

namespace {

// A kana determiner can begin at the final mora of a productive verb
// continuative (たなびき+たる, not たなび+きたる).  Probe the predicate run
// following the nearest particle that can introduce a predicate, and permit a
// suffix probe because the confidence scorer deliberately discounts long
// all-hiragana stems while still recognizing their productive tail.  An
// immediately preceding particle means the determiner starts at a real
// boundary and must remain available (そして+きたる).
bool hasProductiveContinuativeCrossingDeterminer(const core::Lattice& lattice, const grammar::Inflection& inflection,
                                                 const dictionary::DictionaryManager& dict_manager,
                                                 const std::vector<char32_t>& codepoints, size_t determiner_start) {
  if (determiner_start == 0 || !kana::isIRowCodepoint(codepoints[determiner_start])) {
    return false;
  }

  size_t host_start = dictionaryLookbehindStart(determiner_start);
  for (size_t boundary = determiner_start; boundary > host_start; --boundary) {
    const bool follows_predicate_introducing_particle = core::anyEdgeEndingAt(
        lattice, boundary, [](const core::LatticeEdge& edge) { return isNominalForcingParticle(edge.extended_pos); });
    if (follows_predicate_introducing_particle) {
      host_start = boundary;
      break;
    }
  }
  if (host_start == determiner_start) {
    return false;
  }

  for (size_t probe_start = host_start; probe_start < determiner_start; ++probe_start) {
    const std::string continuative = extractSubstring(codepoints, probe_start, determiner_start + 1);
    if (dict_manager.lookupExact(continuative, core::PartOfSpeech::Verb) != nullptr) {
      return true;
    }
    const auto inflection_candidates = inflection.analyze(continuative);
    if (std::any_of(inflection_candidates.begin(), inflection_candidates.end(),
                    [](const grammar::InflectionCandidate& inflection_candidate) {
                      return inflection_candidate.verb_type != grammar::VerbType::IAdjective &&
                             !inflection_candidate.suffix.empty() &&
                             inflection_candidate.confidence >= candidate::verb_cost::kConstructedVerbMinConfidence;
                    })) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool lacksLicensingEnvironment(const DictionaryCandidateContext& ctx, const core::Lattice& lattice,
                               const dictionary::LookupResult& result, size_t end_pos,
                               std::string_view following_text) {
  const auto& dict_manager = ctx.dict_manager;
  const auto& inflection = ctx.inflection;
  const std::string_view text = ctx.text;
  const auto& codepoints = ctx.codepoints;
  const auto& byte_offsets = ctx.byte_offsets;
  const size_t start_pos = ctx.start_pos;
  const size_t byte_pos = ctx.byte_pos;
  const auto& lookup_results = ctx.lookup_results;
  // かねる takes a continuative, which a voice auxiliary also supplies
  // (損なわ+れ+かね, 行か+せ+かね).
  if (result.entry->extended_pos == core::ExtendedPOS::AuxInability &&
      !hasPrecedingExtendedPOS(
          lattice, start_pos,
          {core::ExtendedPOS::VerbRenyokei, core::ExtendedPOS::AuxPassive, core::ExtendedPOS::AuxCausative})) {
    return true;
  }

  // The uninflected progressive contraction とう/どう exists only on an
  // onbin host (終わっ+とう, 読ん+どう); elsewhere it is ordinary kana.
  if (result.entry->extended_pos == core::ExtendedPOS::AuxAspectIru &&
      grammar::isDialectalOruContractionLemma(result.entry->lemma) && utf8::endsWith(result.entry->lemma, "う") &&
      !hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbOnbinkei)) {
    return true;
  }

  // The contracted ておく (とく/どく) fuses the て of a predicate, so it stands
  // only on a continuative, an onbin or a voice auxiliary (書い+とく,
  // 読ん+どく, 見+とく, させ+とく); after a particle it is kana (と+どく).
  if (result.entry->extended_pos == core::ExtendedPOS::AuxAspectOku &&
      utf8::equalsAny(result.entry->lemma, {"とく", "どく"}) &&
      !hasPrecedingExtendedPOS(lattice, start_pos,
                               {core::ExtendedPOS::VerbRenyokei, core::ExtendedPOS::VerbOnbinkei,
                                core::ExtendedPOS::AuxCausative, core::ExtendedPOS::AuxPassive})) {
    return true;
  }

  // An adverb spelled like a te-form (至って, 決して) is that te-form when an
  // auxiliary selecting the te-form follows it (至っ+て+おら+ず, 至っ+て+ませ+ん).
  if (result.entry->pos == core::PartOfSpeech::Adverb && result.length >= 2 &&
      (codepoints[end_pos - 1] == U'て' || codepoints[end_pos - 1] == U'で')) {
    bool te_selecting_auxiliary_follows = false;
    for (size_t aux_end = end_pos + 1; aux_end <= std::min(codepoints.size(), end_pos + 2); ++aux_end) {
      const auto* aux = lookupEntryInRange(dict_manager, codepoints, end_pos, aux_end, core::PartOfSpeech::Auxiliary);
      te_selecting_auxiliary_follows =
          te_selecting_auxiliary_follows || (aux != nullptr && (aux->extended_pos == core::ExtendedPOS::AuxAspectIru ||
                                                                aux->extended_pos == core::ExtendedPOS::AuxTenseMasu));
    }
    // The mora may instead open a following word (かえって+いい).
    bool content_word_follows = false;
    for (size_t word_end = end_pos + 2; word_end <= std::min(codepoints.size(), end_pos + 3); ++word_end) {
      content_word_follows = content_word_follows || lookupEntryInRange(dict_manager, codepoints, end_pos, word_end,
                                                                        core::PartOfSpeech::Adjective) != nullptr;
    }
    if (te_selecting_auxiliary_follows && !content_word_follows) {
      return true;
    }
  }

  // The u-onbin とう of the desiderative stands on a continuative before the
  // negative or the humble predicates it heads (食べ+とう+ない, 行き+とう+ござる).
  if (result.entry->extended_pos == core::ExtendedPOS::AuxDesireTai && result.entry->lemma != result.entry->surface &&
      utf8::endsWith(result.entry->surface, "う") &&
      !(hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbRenyokei) &&
        (verb_helpers::naiNegativeFollowsAt(codepoints, end_pos) ||
         utf8::startsWithAny(following_text, {"ござ", "存じ"})))) {
    return true;
  }

  // An interrogative adverb fused with か (どうか) is one adverb only when a
  // predicate follows it (どうか+してる, どうか+助けて); in かどうか and
  // before a particle (どうか+な) it stays the adverb plus the particle か.
  const auto* interrogative_head =
      result.entry->pos == core::PartOfSpeech::Adverb && result.length >= 2 && codepoints[end_pos - 1] == U'か'
          ? lookupEntryInRange(dict_manager, codepoints, start_pos, end_pos - 1, core::PartOfSpeech::Adverb)
          : nullptr;
  if (interrogative_head != nullptr && interrogative_head->extended_pos == core::ExtendedPOS::AdverbQuotative) {
    const bool after_ka = start_pos > 0 && codepoints[start_pos - 1] == U'か';
    const bool predicate_follows = end_pos < codepoints.size() &&
                                   (normalize::isKanjiCodepoint(codepoints[end_pos]) ||
                                    grammar::isSuruRenyokeiSurface(extractSubstring(codepoints, end_pos, end_pos + 1)));
    if (after_ka || !predicate_follows) {
      return true;
    }
  }

  // The aspect おる's irrealis おら stands only behind the connective て/で
  // (書いて+おら+ず); elsewhere おら is the pronoun of おらが村 or a verb.
  if (result.entry->extended_pos == core::ExtendedPOS::AuxAspectIru && result.entry->lemma == "おる" &&
      grammar::endsWithARow(result.entry->surface) &&
      !(start_pos > 0 && (codepoints[start_pos - 1] == U'て' || codepoints[start_pos - 1] == U'で') &&
        hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::ParticleConj))) {
    return true;
  }

  // The pejorative ったらしい opens on the geminate that the past た also
  // takes after an onbin stem or the copula (言っ+た+らしい, だっ+た+らしい);
  // a registered predicate cell ending at that っ claims it.
  // Without the geminate, its host is a nominal written outside the kana run:
  // after kana it is the tail of a word (あ+たらしい for あたらしい) or of a
  // stem closing on っ.
  if (result.entry->pos == core::PartOfSpeech::Adjective && utf8::endsWith(result.entry->lemma, "たらしい") &&
      ((codepoints[start_pos] == U'っ' &&
        (endsDictionaryVerbSpanningBack(dict_manager, codepoints, start_pos, start_pos + 1) ||
         (start_pos > 0 && lookupEntryInRange(dict_manager, codepoints, start_pos - 1, start_pos + 1,
                                              core::PartOfSpeech::Auxiliary) != nullptr))) ||
       (codepoints[start_pos] != U'っ' && start_pos > 0 && kana::isHiraganaCodepoint(codepoints[start_pos - 1])))) {
    return true;
  }

  // The one-mora contracted polite copula す stands only on the nominalizer
  // ん (行くん+す+か); everywhere else す is a verb.
  if (result.entry->extended_pos == core::ExtendedPOS::AuxCopulaDesu && end_pos == start_pos + 1 &&
      !(start_pos > 0 && codepoints[start_pos - 1] == U'ん' &&
        hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::ParticleNo))) {
    return true;
  }

  // The one-mora classical desiderative auxiliary ま is valid only as the
  // first component of まほしき.  Keeping it context-gated prevents a
  // common temporal adverb such as いま from being split as い+ま.
  if (result.entry->extended_pos == core::ExtendedPOS::AuxDesireTai &&
      grammar::isClassicalDesiderativeMarker(result.entry->surface) &&
      !grammar::startsClassicalDesiderativeSequence(text.substr(byte_pos))) {
    return true;
  }

  // The classical honorific たまふ is represented as た+ま+ふ.  Its
  // one-mora pieces are admitted only inside that exact auxiliary chain.
  if (result.entry->extended_pos == core::ExtendedPOS::AuxHonorific &&
      grammar::isClassicalHonorificComponent(result.entry->surface)) {
    const bool is_marker = grammar::isClassicalDesiderativeMarker(result.entry->surface);
    const bool has_honorific_start = grammar::startsClassicalHonorificSequence(text.substr(byte_pos));
    const bool follows_honorific_marker =
        start_pos > 0 && grammar::isClassicalDesiderativeMarker(extractSubstring(codepoints, start_pos - 1, start_pos));
    if ((is_marker && !has_honorific_start) || (!is_marker && !follows_honorific_marker)) {
      return true;
    }
  }

  // The classical past keeps only its 連体形 し and 已然形 しか, so each has
  // exactly one environment: し modifies a following nominal or closes the
  // clause (読みし人, 読まざりし。) and しか takes the conditional particle
  // (見しかば).  Anywhere else the same kana is the サ変 continuative
  // (消し+ます, 落ち+し+て).
  // The classical perfect たり contributes its own 已然形 たれ, which needs the
  // same conjunctive particle (記録したれ+ども).
  const bool classical_perfect_izenkei = result.entry->extended_pos == core::ExtendedPOS::AuxClassicalPerfect &&
                                         grammar::spellsHypotheticalAuxiliaryCell(result.entry->surface);
  const bool is_classical_izenkei = classical_perfect_izenkei || end_pos - start_pos > 1;
  // The irrealis せ of that same paradigm is licensed by neither test below:
  // its cell never closes a clause and never heads a nominal, because the
  // counterfactual is the only construction that selects it. What identifies
  // it is the pair of hosts around it — the continuative it attaches to, and
  // the conditional particle the construction ends in (高かり+せ+ば). The two
  // far commoner readings of the mora take neither: the サ変 irrealis follows
  // the nominal it turns into a predicate (勉強+せ+ば), and the causative
  // follows an irrealis rather than a continuative.
  const bool classical_past_irrealis = result.entry->extended_pos == core::ExtendedPOS::AuxClassicalKi &&
                                       grammar::spellsClassicalPastIrrealis(result.entry->surface);
  if (classical_past_irrealis) {
    const bool follows_renyokei =
        hasPrecedingExtendedPOS(lattice, start_pos, {core::ExtendedPOS::VerbRenyokei, core::ExtendedPOS::AdjRenyokei});
    if (!follows_renyokei || !verb_helpers::hypotheticalParticleFollowsAt(dict_manager, codepoints, end_pos)) {
      return true;
    }
  } else if (result.entry->extended_pos == core::ExtendedPOS::AuxClassicalKi || classical_perfect_izenkei) {
    // 係り結び leaves the 已然形 as the clause's own predicate, so the cell also
    // stands with no particle after it at all (雨こそ降りたれ, 月を見しか). What
    // marks it there is the continuative it attaches to, not the follower: a
    // case particle in that slot leaves the same kana as the ordinary noun it
    // introduces (料理に+たれ, 背も+たれ), and requiring the binding particle
    // itself would reject the same cell wherever the clause carries no 係助詞,
    // which is the reading the oracle takes (彼が知り+たれ). The continuative may
    // belong to an auxiliary rather than the verb, because a voice auxiliary
    // hosts the perfect from the same cell (開か+れ+たれ).
    const bool follows_continuative =
        hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbRenyokei) ||
        hasPrecedingPartOfSpeech(lattice, start_pos, partOfSpeechMask(core::PartOfSpeech::Auxiliary));
    const bool izenkei_closes_clause =
        is_classical_izenkei && verb_helpers::clauseEndsAt(codepoints, end_pos) && follows_continuative;
    // The 連体形 also nominalizes, and the nominal it forms takes a particle of
    // its own (告げぬべかりし+に, 読みし+を). The host separates that from the サ変
    // continuative the same kana spells: the classical past attaches to a
    // continuative, while the サ変 verb takes the nominal it turns into a
    // predicate, or the particle that introduces one (話を+し+に行く).
    const bool rentaikei_nominalizes = !is_classical_izenkei && follows_continuative &&
                                       verb_helpers::caseParticleFollowsAt(dict_manager, codepoints, end_pos);
    if (!izenkei_closes_clause && !rentaikei_nominalizes &&
        !verb_helpers::classicalPastEnvironmentFollows(dict_manager, codepoints, end_pos, is_classical_izenkei)) {
      return true;
    }
  }

  // A one-mora classical perfect is the tail of far more words than it is an
  // auxiliary (待つ, 一つ, いつの間にか), so it is admitted only where the
  // paradigm cell it attaches to actually precedes it. The realis is evidence
  // enough on its own, because り is the only auxiliary that takes it
  // (行け+り). A continuative precedes half the lattice, so the terminal つ
  // additionally needs the clause end its form implies (書き+つ).
  // The continuative cell is licensed by a different follower: it hands the
  // predicate to the literary past instead of closing the clause its own form
  // would end (来+に+けり).
  // The terminal also hands its clause to a further literary auxiliary
  // (散り+ぬ+べし, 確認し+ぬ+らむ), which is as decisive as the clause end.
  const bool continuative_environment =
      (grammar::spellsClassicalPerfectContinuative(result.entry->surface) &&
       verb_helpers::literaryPastAuxiliaryFollowsAt(dict_manager, codepoints, end_pos)) ||
      (!grammar::spellsClassicalPerfectContinuative(result.entry->surface) &&
       verb_helpers::classicalAuxiliaryFollowsAt(&dict_manager, codepoints, end_pos));
  if (result.entry->extended_pos == core::ExtendedPOS::AuxClassicalPerfect && end_pos == start_pos + 1 &&
      !hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbKateikei) &&
      !(hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbRenyokei) &&
        (verb_helpers::classicalPastEnvironmentFollows(dict_manager, codepoints, end_pos, false) ||
         continuative_environment))) {
    return true;
  }

  // An interjection is an utterance of its own, closed by punctuation or by a
  // change of script rather than continued by more kana. Where its surface is
  // also the irrealis of a dictionary verb, that verb owns the paradigm behind
  // it (あら、素敵ね keeps the interjection; あらう, あらば, あらゆる stay with
  // ある). Interjections with no such reading are unaffected.
  if (result.entry->pos == core::PartOfSpeech::Interjection && end_pos < codepoints.size() && end_pos > start_pos + 1 &&
      normalize::classifyChar(codepoints[end_pos]) == normalize::CharType::Hiragana) {
    const std::string_view base_suffix = grammar::godanBaseSuffixFromARow(codepoints[end_pos - 1]);
    if (!base_suffix.empty() &&
        dict_manager.lookupExact(normalize::concat(extractSubstring(codepoints, start_pos, end_pos - 1), base_suffix),
                                 core::PartOfSpeech::Verb) != nullptr) {
      return true;
    }
  }

  // A 終助詞 closes its clause, so the nominalizer cannot follow it. The な in
  // そう+な+ん+です is the copula's attributive form instead; the indefinite
  // stack the bigram favors (いくつ+か+の) uses the の spelling and is untouched.
  if (result.entry->extended_pos == core::ExtendedPOS::ParticleFinal && end_pos < codepoints.size() &&
      codepoints[end_pos] == U'ん') {
    return true;
  }

  // The contracted directional く is the いく renyokei with its い elided, so
  // it exists only directly after a te-form (読ん+で+く).  Anywhere else the
  // same single kana is an adjective continuative or a stem fragment, and
  // admitting the auxiliary there splits the negative continuative (な+く for
  // 書か+なく+ない).
  if (result.entry->extended_pos == core::ExtendedPOS::AuxAspectIku && end_pos - start_pos == 1 &&
      (start_pos == 0 || (codepoints[start_pos - 1] != U'て' && codepoints[start_pos - 1] != U'で'))) {
    return true;
  }

  // A 副助詞 attaches to a 体言 and a 接続詞 opens a clause; neither follows a
  // verb onbin stem.  Where one that begins with だ appears to (読ん+だって,
  // 脱い+だって), the だ is the voiced past auxiliary and the rest is its own
  // word (読ん+だ+って).  The stem is a kanji verb whose dictionary base takes
  // the voiced allomorph, or the hatsuonbin shape kanji + ん; an ordinary
  // noun (みかん+だって) and every other left context stay untouched.
  if ((result.entry->extended_pos == core::ExtendedPOS::ParticleAdverbial ||
       result.entry->pos == core::PartOfSpeech::Conjunction) &&
      start_pos >= 2 && codepoints[start_pos] == U'だ' &&
      ((codepoints[start_pos - 1] == U'ん' && normalize::isKanjiCodepoint(codepoints[start_pos - 2])) ||
       (normalize::isKanjiCodepoint(codepoints[start_pos - 2]) &&
        isDictionaryOnbinPast(dict_manager, extractSubstring(codepoints, start_pos - 2, start_pos + 1))))) {
    return true;
  }

  // An adverb takes no object, so a te-ending adverb behind the object
  // marker is the continuative of a verb plus the conjunctive て whenever the
  // stem names a dictionary verb (約束を+果たし+て, never 果たして).
  if (result.entry->pos == core::PartOfSpeech::Adverb && end_pos - start_pos >= 3 && start_pos > 0 &&
      codepoints[start_pos - 1] == U'を' && codepoints[end_pos - 1] == U'て' &&
      verb_helpers::namesDictionaryVerbContinuative(&dict_manager, codepoints, start_pos, end_pos - 2)) {
    return true;
  }

  // A pure-hiragana adnominal begins with a kana that is also an inflectional
  // ending, so it cannot start where a productive verb continuative already
  // straddles the boundary (書き+たる, たなびき+たる).  A real boundary
  // immediately before the determiner and unrelated kana contexts remain
  // untouched.
  if (result.entry->pos == core::PartOfSpeech::Determiner &&
      hasProductiveContinuativeCrossingDeterminer(lattice, inflection, dict_manager, codepoints, start_pos)) {
    return true;
  }

  // The historical terminal component ふ is meaningful only after a kanji
  // stem.  The positional gate retains separations such as 候+ふ and 思+ふ
  // without admitting a free one-mora verb in ordinary hiragana text.
  if (result.entry->pos == core::PartOfSpeech::Verb &&
      result.entry->extended_pos == core::ExtendedPOS::VerbShuushikei &&
      grammar::isClassicalFuruTerminal(result.entry->surface) &&
      (start_pos == 0 || !normalize::isKanjiCodepoint(codepoints[start_pos - 1]))) {
    return true;
  }

  // A classical honorific written as its bare kanji (候) is the predicate of
  // a clause, so it follows a continuative, a particle or a clause boundary.
  // After another kanji it is the second element of a compound noun
  // (天候, 兆候), and after the genitive or an adnominal it is the noun a
  // modifier heads (新緑の候). A registered multi-kanji noun that ends here
  // (御座) is a complete host instead, and the honorific is its predicate.
  if (result.entry->pos == core::PartOfSpeech::Verb && grammar::isHumbleHonorificLemma(result.entry->lemma) &&
      result.length == 1 && normalize::isKanjiCodepoint(codepoints[start_pos]) && start_pos > 0 &&
      ((normalize::isKanjiCodepoint(codepoints[start_pos - 1]) &&
        !core::anyEdgeEndingAt(lattice, start_pos,
                               [](const core::LatticeEdge& edge) {
                                 return edge.pos == core::PartOfSpeech::Noun && edge.fromDictionary() &&
                                        edge.end - edge.start >= 2;
                               })) ||
       core::anyEdgeEndingAt(lattice, start_pos, [](const core::LatticeEdge& edge) {
         return edge.extended_pos == core::ExtendedPOS::ParticleNo || edge.pos == core::PartOfSpeech::Determiner;
       }))) {
    return true;
  }

  // A dictionary noun homographic with a verb renyokei (知らせ) cannot
  // precede the closed classical honorific auxiliary chain たまふ.  Keep the
  // verb boundary available in that grammatical environment.
  if (result.entry->pos == core::PartOfSpeech::Noun &&
      grammar::startsClassicalHonorificAuxiliaryChain(following_text)) {
    return true;
  }

  // In an interrogative emphatic sequence, として is not the viewpoint
  // compound particle: it is と+し+て before the focus particle も.
  if (result.entry->extended_pos == core::ExtendedPOS::ParticleCase &&
      grammar::isQuotativeSuruTeCompoundParticle(result.entry->surface) && end_pos < codepoints.size() &&
      codepoints[end_pos] == U'も' && hasInterrogativeEndingAt(dict_manager, text, byte_offsets, start_pos)) {
    return true;
  }

  // The contracted preparative auxiliary has a genuine mizenkei+volitional
  // cell (とこ+う / どこ+う), but those spellings are also ordinary lexical
  // words. Emit them only in their complete verb-onbin auxiliary context.
  // Other AuxAspectOku forms before う are the invalid とい+う path.
  if (result.entry->extended_pos == core::ExtendedPOS::AuxAspectOku) {
    const bool follows_volitional = verb_helpers::volitionalEndingFollowsAt(codepoints, end_pos);
    // The contraction is て + おく, so its host is whichever cell that て
    // selects: the onbin form of a Godan verb (書い+とこう) but the plain
    // continuative of an Ichidan or サ変 one (見+とこう, 作成し+とこう).
    // Admitting only the onbin cell left the other two conjugations to fall
    // back on the case particle plus the homographic adverb.
    const bool contracted_volitional =
        utf8::equalsAny(result.entry->surface, {"とこ", "どこ"}) && follows_volitional &&
        hasPrecedingExtendedPOS(lattice, start_pos, {core::ExtendedPOS::VerbOnbinkei, core::ExtendedPOS::VerbRenyokei});
    if ((utf8::equalsAny(result.entry->surface, {"とこ", "どこ"}) && !contracted_volitional) ||
        (follows_volitional && !contracted_volitional)) {
      return true;
    }
  }

  // At the beginning of a clause, a one-mora continuative cannot steal the
  // first mora of a longer dictionary conjunction (しかも, しかし). The
  // conjunction is already a complete closed-class candidate at this
  // boundary; letting its prefix reach a following particle manufactures a
  // predicate with no host.
  if (start_pos == 0 && result.entry->extended_pos == core::ExtendedPOS::VerbRenyokei && result.length == 1) {
    const bool has_longer_conjunction = lookupResultsHaveLongerPartOfSpeech(
        lookup_results, partOfSpeechMask(core::PartOfSpeech::Conjunction), result.length);
    if (has_longer_conjunction) {
      return true;
    }
  }

  // A conjunction introduces a new predicate. Do not start that predicate
  // with a one-character nominal/suffix homograph when a longer dictionary
  // verb begins at the same boundary (しかも+間違えた, not しかも+間+違えた).
  if ((result.entry->pos == core::PartOfSpeech::Noun || result.entry->pos == core::PartOfSpeech::Suffix) &&
      result.length == 1 &&
      hasPrecedingPartOfSpeech(lattice, start_pos, partOfSpeechMask(core::PartOfSpeech::Conjunction))) {
    const bool has_longer_verb =
        lookupResultsHaveLongerPartOfSpeech(lookup_results, partOfSpeechMask(core::PartOfSpeech::Verb), result.length);
    if (has_longer_verb) {
      return true;
    }
  }

  // A one-kanji suffix closing the kanji run right after a numeral+counter is
  // part of the quantity phrase (三割+強, 二時間+弱), which keeps no lone
  // kanji of its own; relational 前/後 stand alone (三日|後).
  if (result.entry->pos == core::PartOfSpeech::Suffix && result.length == 1 &&
      verb_helpers::isQuantityClosingSuffixAt(&dict_manager, codepoints, start_pos)) {
    size_t counter_start = start_pos;
    while (counter_start > 0 && normalize::isCounterKanji(codepoints[counter_start - 1])) {
      --counter_start;
    }
    if (counter_start < start_pos && counter_start > 0 &&
        normalize::isNumeralCodepoint(codepoints[counter_start - 1])) {
      return true;
    }
  }

  // A deverbal suffix after a kana continuative binds to it only when it
  // closes the kanji run; otherwise it opens the next noun, whether listed
  // alone or as the tail of a lexical noun (申し込み+手続き, 置き+場所).
  if (result.entry->pos == core::PartOfSpeech::Suffix || result.entry->pos == core::PartOfSpeech::Noun) {
    const size_t suffix_pos = start_pos + result.length - 1;
    if (suffix_pos > 0 && kana::isHiraganaCodepoint(codepoints[suffix_pos - 1]) &&
        grammar::isDeverbalSuffixKanji(codepoints[suffix_pos]) &&
        !grammar::isBoundDeverbalSuffixAt(codepoints, suffix_pos)) {
      return true;
    }
  }

  if (result.entry->pos == core::PartOfSpeech::Particle && utf8::equalsAny(result.entry->surface, {"だの"}) &&
      end_pos < codepoints.size() && codepoints[end_pos] == U'は' &&
      hasPrecedingExtendedPOS(lattice, start_pos, core::ExtendedPOS::VerbOnbinkei)) {
    return true;
  }

  // The contrastive nominal construction のでは keeps the nominalizer,
  // copular connective, and topic particle independently searchable.  The
  // causal compound particle ので cannot consume its initial two morae.
  if (result.entry->extended_pos == core::ExtendedPOS::ParticleConj &&
      grammar::isCausalParticleBeforeTopic(result.entry->surface, following_text)) {
    return true;
  }

  // Skip a dictionary adjective ending in double い when its final い is the
  // leading い of the receptive auxiliary いただく: the adjective reading
  // would fuse a wa-row renyokei's い with the auxiliary's onset
  // (お使いいただく → 使い+いただく, not 使+いい+ただく). Plain いい in
  // predicate/attributive position is untouched (no ただ+inflection follows).
  if (result.entry->pos == core::PartOfSpeech::Adjective && result.length >= 2 && codepoints[end_pos - 1] == U'い' &&
      codepoints[end_pos - 2] == U'い' && verb_helpers::itadakuParadigmStartsAt(codepoints, end_pos - 1)) {
    return true;
  }
  return false;
}

}  // namespace suzume::analysis::tokenizer_dictionary_detail
