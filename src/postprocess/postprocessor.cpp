#include "postprocess/postprocessor.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "core/debug.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"
#include "postprocess/postprocessor_internal.h"
#include "postprocess/postprocessor_resolvers_internal.h"

namespace suzume::postprocess {

namespace {

bool isOnlyProlongedSoundMarks(std::string_view surface) {
  if (surface.empty()) {
    return false;
  }
  size_t pos = 0;
  while (pos < surface.size()) {
    if (!normalize::isProlongedSoundMark(normalize::decodeUtf8(surface, pos))) {
      return false;
    }
  }
  return true;
}

void resolveSemanticRolesPreservingSymbols(std::vector<core::Morpheme>& result,
                                           const dictionary::DictionaryManager* dict_manager) {
  std::vector<core::Morpheme> semantic_morphemes;
  std::vector<core::Morpheme> symbols;
  semantic_morphemes.reserve(result.size());
  for (auto& morpheme : result) {
    if (morpheme.pos == core::PartOfSpeech::Symbol) {
      if (symbols.empty()) {
        symbols.reserve(result.size());
      }
      symbols.push_back(std::move(morpheme));
    } else {
      semantic_morphemes.push_back(std::move(morpheme));
    }
  }

  resolveFinalMorphemeRoles(semantic_morphemes, dict_manager);

  result.clear();
  result.reserve(semantic_morphemes.size() + symbols.size());
  size_t semantic_idx = 0;
  size_t symbol_idx = 0;
  while (semantic_idx < semantic_morphemes.size() && symbol_idx < symbols.size()) {
    auto& semantic = semantic_morphemes[semantic_idx];
    auto& symbol = symbols[symbol_idx];
    if (semantic.end <= symbol.start) {
      result.push_back(std::move(semantic));
      ++semantic_idx;
    } else if (symbol.end <= semantic.start) {
      result.push_back(std::move(symbol));
      ++symbol_idx;
    } else {
      // Structural resolver merges must not span punctuation. Preserve both
      // spans deterministically if a future resolver violates that invariant;
      // library code must not terminate its host process for malformed input.
      SUZUME_DEBUG_LOG("[POSTPROC] preserving overlapping symbol span\n");
      if (semantic.start <= symbol.start) {
        result.push_back(std::move(semantic));
        ++semantic_idx;
      } else {
        result.push_back(std::move(symbol));
        ++symbol_idx;
      }
    }
  }
  while (semantic_idx < semantic_morphemes.size()) {
    result.push_back(std::move(semantic_morphemes[semantic_idx++]));
  }
  while (symbol_idx < symbols.size()) {
    result.push_back(std::move(symbols[symbol_idx++]));
  }
}

// Merges each adjacent pair that `accepts` into its head, then lets `retag`
// set the merged category from the merged token and the original pair.
// `accepts` also sees the output built so far.
template <typename Accepts, typename Retag>
std::vector<core::Morpheme> mergeAdjacentPairs(std::vector<core::Morpheme> morphemes, Accepts accepts, Retag retag) {
  if (morphemes.size() < 2) {
    return morphemes;
  }

  std::vector<core::Morpheme> result;
  result.reserve(morphemes.size());
  for (size_t idx = 0; idx < morphemes.size(); ++idx) {
    if (idx + 1 < morphemes.size() && accepts(result, morphemes[idx], morphemes[idx + 1])) {
      core::Morpheme merged = morphemes[idx];
      resolver::mergeInto(merged, morphemes[idx + 1]);
      retag(merged, morphemes[idx], morphemes[idx + 1]);
      result.push_back(std::move(merged));
      ++idx;
      continue;
    }
    result.push_back(std::move(morphemes[idx]));
  }
  return result;
}

}  // namespace

Postprocessor::Postprocessor(const PostprocessOptions& options)
    : options_(options), dict_manager_(nullptr), lemmatizer_() {}

Postprocessor::Postprocessor(const dictionary::DictionaryManager* dict_manager, const PostprocessOptions& options)
    : options_(options), dict_manager_(dict_manager), lemmatizer_(dict_manager) {}

std::vector<core::Morpheme> Postprocessor::process(std::vector<core::Morpheme> result) const {
  [[maybe_unused]] size_t before_count = 0;
  // Input spans are few and already ordered, so a flat list beats an ordered
  // container here and keeps its node machinery out of the build.
  struct SourceLemma {
    size_t start;
    size_t end;
    std::string lemma;
  };
  std::vector<SourceLemma> source_lemmas;
  if (!options_.lemmatize) {
    source_lemmas.reserve(result.size());
    for (const auto& morpheme : result) {
      source_lemmas.push_back({morpheme.start, morpheme.end, morpheme.lemma});
    }
  }

  // NOUN + SUFFIX merging is intentionally NOT applied: tokens stay separate as
  // PREFIX + NOUN + SUFFIX (e.g., お姉さん → お(PREFIX) + 姉(NOUN) + さん(SUFFIX)).

  // Convert PREFIX + VERB to PREFIX + NOUN (renyoukei nominalization)
  // e.g., お願い → お(PREFIX) + 願い(NOUN), not 願い(VERB)
  convertPrefixVerbToNoun(result);
  // Note: this function logs individual changes, so no summary needed

  // Merge consecutive numeric expressions (always applied)
  before_count = result.size();
  result = mergeNumericExpressions(std::move(result));
  if (result.size() != before_count) {
    SUZUME_DEBUG_LOG("[POSTPROC] mergeNumericExpressions: " << before_count << " → " << result.size() << "\n");
  }

  // Keep a na-adjective stem and the attributive copula な as separate
  // grammatical search units.

  // Conjugation-form detection and contextual POS repairs are annotations, not
  // optional lemma output. Always run the batch pass; the option controls only
  // whether its corrected lemmas are retained.
  lemmatizer_.lemmatizeAll(result, options_.lemmatize);
  SUZUME_DEBUG_LOG_VERBOSE("[POSTPROC] lemmatize: " << (options_.lemmatize ? "applied" : "lemma writes suppressed")
                                                    << "\n");

  resolvePrePrefixMorphemeRoles(result, dict_manager_);
  convertPrefixVerbToNoun(result);
  resolvePostPrefixMorphemeRoles(result);

  // Merge verb renyokei + もの → compound noun (食べもの, 飲みもの, etc.)
  // Must run after lemmatize so conj_form is set
  before_count = result.size();
  result = mergeVerbRenyokeiMono(std::move(result));
  if (result.size() != before_count) {
    SUZUME_DEBUG_LOG("[POSTPROC] mergeVerbRenyokeiMono: " << before_count << " → " << result.size() << "\n");
  }

  // Merge nominal stems with the bound temporal noun 途中 (作業途中、移動途中).
  // This is a search unit regardless of the optional general noun-compound mode.
  before_count = result.size();
  result = mergeNounTemporalFormal(std::move(result));
  if (result.size() != before_count) {
    SUZUME_DEBUG_LOG("[POSTPROC] mergeNounTemporalFormal: " << before_count << " → " << result.size() << "\n");
  }

  // Merge lexicalized 副詞 that the lattice mis-split (決して, 大して, ちゃんと)
  before_count = result.size();
  result = mergeLexicalizedAdverbs(std::move(result));
  if (result.size() != before_count) {
    SUZUME_DEBUG_LOG("[POSTPROC] mergeLexicalizedAdverbs: " << before_count << " → " << result.size() << "\n");
  }

  // Merge noun compounds
  if (options_.merge_noun_compounds) {
    before_count = result.size();
    result = mergeNounCompounds(std::move(result));
    if (result.size() != before_count) {
      SUZUME_DEBUG_LOG("[POSTPROC] mergeNounCompounds: " << before_count << " → " << result.size() << "\n");
    }
  }

  // Merge prolonged sound mark (ー) with preceding token
  before_count = result.size();
  result = mergeProlongedSoundMark(std::move(result));
  if (result.size() != before_count) {
    SUZUME_DEBUG_LOG("[POSTPROC] mergeProlongedSoundMark: " << before_count << " → " << result.size() << "\n");
  }

  // Punctuation must not affect neighboring semantic roles. Resolve a
  // symbol-free owning vector, then merge its potentially shortened result
  // back by source spans.
  resolveSemanticRolesPreservingSymbols(result, dict_manager_);

  // Filter unwanted morphemes only after every role has been resolved.
  result = filterMorphemes(std::move(result));
  // The POS of ない follows from its host, which is only final once symbols are gone.
  resolver::resolveNegativeHost(result);
  resolver::resolveSouHost(result);
  resolver::resolveCopulaAru(result);
  resolver::mergeContractedTeruBeforeNominalizer(result);
  resolver::resolveListingTari(result);

  if (!options_.lemmatize) {
    // Role resolution must still run to supply POS and conjugation annotations,
    // but --no-lemmatize promises that surviving input morphemes keep their
    // lattice lemma. Merged tokens have a new source span and retain their
    // structural lemma instead.
    for (auto& morpheme : result) {
      const auto original =
          std::find_if(source_lemmas.begin(), source_lemmas.end(), [&morpheme](const SourceLemma& candidate) {
            return candidate.start == morpheme.start && candidate.end == morpheme.end;
          });
      if (original != source_lemmas.end()) {
        morpheme.lemma = original->lemma;
      }
    }
  }

  return result;
}

std::vector<core::Morpheme> Postprocessor::mergeNounCompounds(std::vector<core::Morpheme> morphemes) {
  if (morphemes.empty()) {
    return morphemes;
  }

  std::vector<core::Morpheme> result;
  result.reserve(morphemes.size());

  size_t idx = 0;
  while (idx < morphemes.size()) {
    const auto& current = morphemes[idx];

    // Check if this is a noun that can be merged
    if (current.pos == core::PartOfSpeech::Noun && !current.isFormalNoun()) {
      // Collect consecutive nouns
      core::Morpheme merged = current;
      size_t merge_end = idx + 1;
      size_t merge_count = 1;

      while (merge_end < morphemes.size()) {
        const auto& next = morphemes[merge_end];
        if (next.pos == core::PartOfSpeech::Noun && !next.isFormalNoun()) {
          // Merge surface and lemma
          resolver::mergeInto(merged, next);
          if (!next.lemma.empty()) {
            merged.lemma += next.lemma;
          } else {
            merged.lemma += next.surface;
          }
          ++merge_end;
          ++merge_count;
        } else {
          break;
        }
      }

      SUZUME_DEBUG_IF(merge_count > 1) {
        SUZUME_DEBUG_STREAM << "[POSTPROC] Merged " << merge_count << " nouns: ";
        for (size_t i = idx; i < merge_end; ++i) {
          if (i > idx)
            SUZUME_DEBUG_STREAM << " + ";
          SUZUME_DEBUG_STREAM << "\"" << morphemes[i].surface << "\"";
        }
        SUZUME_DEBUG_STREAM << " → \"" << merged.surface << "\"\n";
      }

      result.push_back(std::move(merged));
      idx = merge_end;
    } else {
      result.push_back(std::move(morphemes[idx]));
      ++idx;
    }
  }

  return result;
}

std::vector<core::Morpheme> Postprocessor::filterMorphemes(std::vector<core::Morpheme> morphemes) const {
  std::vector<core::Morpheme> result;
  result.reserve(morphemes.size());

  for (auto& morpheme : morphemes) {
    // Skip symbols if option is set
    if (options_.remove_symbols && morpheme.pos == core::PartOfSpeech::Symbol) {
      continue;
    }

    result.push_back(std::move(morpheme));
  }

  return result;
}

std::vector<core::Morpheme> Postprocessor::mergeVerbRenyokeiMono(std::vector<core::Morpheme> morphemes) {
  // VERB + もの(formal noun) → compound NOUN
  // e.g., 食べ+もの → 食べもの, 飲み+もの → 飲みもの, 乗り+もの → 乗りもの.
  // A generated pure-hiragana continuative has only inflectional evidence;
  // without lexical evidence it keeps the formal-noun boundary (たて+もの)
  // instead of being promoted to a compound search unit.
  return mergeAdjacentPairs(
      std::move(morphemes),
      [](const std::vector<core::Morpheme>& /*merged_so_far*/, const core::Morpheme& verb, const core::Morpheme& mono) {
        return verb.pos == core::PartOfSpeech::Verb && verb.conj_form == grammar::ConjForm::Renyokei &&
               mono.surface == "もの" && mono.isFormalNoun() && verb.origin != core::CandidateOrigin::VerbHiragana;
      },
      [](core::Morpheme& merged, [[maybe_unused]] const core::Morpheme& verb, const core::Morpheme& /*mono*/) {
        resolver::retagNounSurface(merged);
        SUZUME_DEBUG_LOG("[POSTPROC] Merged verb+もの: \"" << verb.surface << "\" + \"もの\" → \"" << merged.surface
                                                           << "\"\n");
      });
}

std::vector<core::Morpheme> Postprocessor::mergeNounTemporalFormal(std::vector<core::Morpheme> morphemes) {
  return mergeAdjacentPairs(
      std::move(morphemes),
      [](const std::vector<core::Morpheme>& /*merged_so_far*/, const core::Morpheme& noun,
         const core::Morpheme& formal) {
        return noun.pos == core::PartOfSpeech::Noun && !noun.isFormalNoun() && formal.pos == core::PartOfSpeech::Noun &&
               formal.isFormalNoun() && formal.surface == "途中";
      },
      [](core::Morpheme& merged, [[maybe_unused]] const core::Morpheme& noun, const core::Morpheme& /*formal*/) {
        merged.lemma = merged.surface;
        merged.extended_pos = core::ExtendedPOS::Noun;
        merged.flags = core::withoutFlag(merged.flags, core::EdgeFlags::IsFormalNoun);
        SUZUME_DEBUG_LOG("[POSTPROC] Merged noun+途中: \"" << noun.surface << "\" + \"途中\" → \"" << merged.surface
                                                           << "\"\n");
      });
}

std::vector<core::Morpheme> Postprocessor::mergeLexicalizedAdverbs(std::vector<core::Morpheme> morphemes) {
  return mergeAdjacentPairs(
      std::move(morphemes),
      [](const std::vector<core::Morpheme>& merged_so_far, const core::Morpheme& cur, const core::Morpheme& nxt) {
        if (nxt.pos != core::PartOfSpeech::Particle) {
          return false;
        }
        // 決して/大して: the lattice reads these kanji-initial 副詞 as a non-word サ変 連用形
        // (決す/大す) plus て. They cannot be L1 entries because an L1 決して would swallow the 決 of
        // 解決して. Merging on the already-split lattice is safe: 解決して yields 解決|し|て (no 決し
        // token), so only the genuine 副詞 reading (決し/大し with the non-word lemma) reaches here.
        const bool is_sahen_te =
            cur.pos == core::PartOfSpeech::Verb && nxt.surface == "て" &&
            ((cur.surface == "決し" && cur.lemma == "決す") || (cur.surface == "大し" && cur.lemma == "大す"));

        // ちゃんと: 接尾辞 ちゃん + と. Gated on the previous token not being a Noun so 赤ちゃんと
        // (赤|ちゃん|と) keeps 赤ちゃん together (→ 赤|ちゃんと) rather than merging the ちゃん away.
        const bool is_chanto = cur.surface == "ちゃん" && cur.pos == core::PartOfSpeech::Suffix &&
                               nxt.surface == "と" &&
                               (merged_so_far.empty() || merged_so_far.back().pos != core::PartOfSpeech::Noun);
        return is_sahen_te || is_chanto;
      },
      [](core::Morpheme& merged, [[maybe_unused]] const core::Morpheme& cur,
         [[maybe_unused]] const core::Morpheme& nxt) {
        resolver::retagUninflected(merged, core::PartOfSpeech::Adverb, core::ExtendedPOS::Adverb, merged.surface);
        SUZUME_DEBUG_LOG("[POSTPROC] Merged lexicalized adverb: \"" << cur.surface << "\"+\"" << nxt.surface
                                                                    << "\" → \"" << merged.surface << "\"\n");
      });
}

std::vector<core::Morpheme> Postprocessor::mergeProlongedSoundMark(std::vector<core::Morpheme> morphemes) {
  if (morphemes.size() < 2) {
    return morphemes;
  }

  std::vector<core::Morpheme> result;
  result.reserve(morphemes.size());

  for (size_t i = 0; i < morphemes.size(); ++i) {
    const auto& current = morphemes[i];
    size_t run_end = i + 1;
    while (current.pos != core::PartOfSpeech::Symbol && run_end < morphemes.size() &&
           isOnlyProlongedSoundMarks(morphemes[run_end].surface)) {
      ++run_end;
    }
    if (run_end == i + 1) {
      result.push_back(std::move(morphemes[i]));
      continue;
    }

    core::Morpheme merged = current;
    for (size_t mark = i + 1; mark < run_end; ++mark) {
      resolver::mergeInto(merged, morphemes[mark]);
      if (!merged.lemma.empty()) {
        merged.lemma += morphemes[mark].surface;
      }
    }
    SUZUME_DEBUG_LOG("[POSTPROC] Merged prolonged sound mark: \"" << current.surface << "\" + \"ー\" → \""
                                                                  << merged.surface << "\"\n");
    result.push_back(std::move(merged));
    i = run_end - 1;
  }

  return result;
}

}  // namespace suzume::postprocess
