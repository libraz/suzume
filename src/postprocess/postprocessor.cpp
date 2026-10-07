#include "postprocess/postprocessor.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "core/debug.h"
#include "grammar/char_patterns.h"
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

using MergePass = std::vector<core::Morpheme> (*)(std::vector<core::Morpheme>);

// Runs one token-merging pass, logging how many tokens it absorbed.
void runMergePass(std::vector<core::Morpheme>& result, MergePass pass, [[maybe_unused]] const char* name) {
  [[maybe_unused]] const size_t before_count = result.size();
  result = pass(std::move(result));
  if (result.size() != before_count) {
    SUZUME_DEBUG_LOG("[POSTPROC] " << name << ": " << before_count << " → " << result.size() << "\n");
  }
}

}  // namespace

Postprocessor::Postprocessor(const PostprocessOptions& options)
    : options_(options), dict_manager_(nullptr), lemmatizer_() {}

Postprocessor::Postprocessor(const dictionary::DictionaryManager* dict_manager, const PostprocessOptions& options)
    : options_(options), dict_manager_(dict_manager), lemmatizer_(dict_manager) {}

std::vector<core::Morpheme> Postprocessor::process(std::vector<core::Morpheme> result) const {
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
  runMergePass(result, mergeNumericExpressions, "mergeNumericExpressions");

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
  runMergePass(result, mergeVerbRenyokeiMono, "mergeVerbRenyokeiMono");

  // Merge nominal stems with the bound temporal noun 途中 (作業途中、移動途中).
  // This is a search unit regardless of the optional general noun-compound mode.
  runMergePass(result, mergeNounTemporalFormal, "mergeNounTemporalFormal");

  // Merge lexicalized 副詞 that the lattice mis-split (決して, 大して, ちゃんと)
  runMergePass(result, mergeLexicalizedAdverbs, "mergeLexicalizedAdverbs");

  // Merge noun compounds
  if (options_.merge_noun_compounds) {
    runMergePass(result, mergeNounCompounds, "mergeNounCompounds");
  }

  // Merge prolonged sound mark (ー) with preceding token
  runMergePass(result, mergeProlongedSoundMark, "mergeProlongedSoundMark");

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
  resolver::resolveExcessiveDeverbalNoun(result);
  resolver::resolveHonorificContinuativePast(result);
  resolver::resolveAdverbBeforeCase(result);
  resolver::resolvePredicateCellLemmas(result);
  resolver::resolveFrameRepairs(result);
  resolver::resolveVerbFrameRepairs(result);

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
          merged.lemma += next.getLemma();
          ++merge_end;
          ++merge_count;
        } else {
          break;
        }
      }

      SUZUME_DEBUG_IF(merge_count > 1) {
        SUZUME_DEBUG_STREAM << "[POSTPROC] Merged " << merge_count << " nouns: ";
        for (size_t merged_idx = idx; merged_idx < merge_end; ++merged_idx) {
          if (merged_idx > idx)
            SUZUME_DEBUG_STREAM << " + ";
          SUZUME_DEBUG_STREAM << "\"" << morphemes[merged_idx].surface << "\"";
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
  if (options_.remove_symbols) {
    morphemes.erase(
        std::remove_if(morphemes.begin(), morphemes.end(),
                       [](const core::Morpheme& morpheme) { return morpheme.pos == core::PartOfSpeech::Symbol; }),
        morphemes.end());
  }
  return morphemes;
}

std::vector<core::Morpheme> Postprocessor::mergeVerbRenyokeiMono(std::vector<core::Morpheme> morphemes) {
  // VERB + もの(formal noun) → compound NOUN
  // e.g., 食べ+もの → 食べもの, 飲み+もの → 飲みもの, 乗り+もの → 乗りもの.
  // The formal noun takes an attributive modifier, never a continuative or a
  // bare noun, so a kana continuative or kana noun in front of it is the first
  // member of the compound as well (たて+もの, のり+もの).
  return mergeAdjacentPairs(
      std::move(morphemes),
      [](const std::vector<core::Morpheme>& /*merged_so_far*/, const core::Morpheme& verb, const core::Morpheme& mono) {
        const bool continuative = verb.pos == core::PartOfSpeech::Verb && verb.conj_form == grammar::ConjForm::Renyokei;
        const bool kana_nominal = verb.pos == core::PartOfSpeech::Noun && !verb.isFormalNoun() &&
                                  normalize::utf8Length(verb.surface) >= 2 && grammar::isPureHiragana(verb.surface);
        return (continuative || kana_nominal) && mono.surface == "もの" && mono.isFormalNoun() &&
               verb.end == mono.start;
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

  for (size_t idx = 0; idx < morphemes.size(); ++idx) {
    const auto& current = morphemes[idx];
    size_t run_end = idx + 1;
    while (current.pos != core::PartOfSpeech::Symbol && run_end < morphemes.size() &&
           isOnlyProlongedSoundMarks(morphemes[run_end].surface)) {
      ++run_end;
    }
    if (run_end == idx + 1) {
      result.push_back(std::move(morphemes[idx]));
      continue;
    }

    core::Morpheme merged = current;
    for (size_t mark = idx + 1; mark < run_end; ++mark) {
      resolver::mergeInto(merged, morphemes[mark]);
      // A held vowel stretches an interjection without changing it, so its lemma
      // stays the greeting itself (おつかれー → おつかれ).
      if (!merged.lemma.empty() && merged.pos != core::PartOfSpeech::Interjection) {
        merged.lemma += morphemes[mark].surface;
      }
    }
    SUZUME_DEBUG_LOG("[POSTPROC] Merged prolonged sound mark: \"" << current.surface << "\" + \"ー\" → \""
                                                                  << merged.surface << "\"\n");
    result.push_back(std::move(merged));
    idx = run_end - 1;
  }

  return result;
}

}  // namespace suzume::postprocess
