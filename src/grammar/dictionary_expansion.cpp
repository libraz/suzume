#include "grammar/dictionary_expansion.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "core/utf8_constants.h"
#include "grammar/conjugation.h"
#include "grammar/honorific_verbs.h"
#include "normalize/utf8.h"

namespace suzume {
namespace grammar {

namespace {

struct IAdjectiveSuffix {
  const char* suffix;
  core::ExtendedPOS extended_pos;
};

// Fixed-width header of the entry-identity key: part of speech, extended part of
// speech, and the surface length.
constexpr size_t kIdentityExtendedPOSBytes = 2;
constexpr size_t kIdentitySurfaceLengthBytes = 4;
constexpr size_t kIdentityHeaderBytes = 1 + kIdentityExtendedPOSBytes + kIdentitySurfaceLengthBytes;

void appendFixedWidth(std::string& key, uint32_t value, size_t width) {
  for (size_t byte_index = 0; byte_index < width; ++byte_index) {
    key.push_back(static_cast<char>((value >> (byte_index * 8U)) & 0xFFU));
  }
}

constexpr IAdjectiveSuffix kIAdjectiveSuffixes[] = {
    {"い", core::ExtendedPOS::AdjBasic},      {"く", core::ExtendedPOS::AdjRenyokei},
    {"かっ", core::ExtendedPOS::AdjKatt},     {"けれ", core::ExtendedPOS::AdjKeForm},
    {"かろ", core::ExtendedPOS::AdjMizenkei},
};

dictionary::DictionaryEntry makeBaseEntry(const dictionary::SourceEntry& source_entry) {
  dictionary::DictionaryEntry entry;
  entry.surface = source_entry.surface;
  entry.pos = source_entry.pos;
  entry.lemma = source_entry.lemma.empty() ? source_entry.surface : source_entry.lemma;

  switch (source_entry.conj_type) {
    case dictionary::ConjugationType::Interjection:
      entry.extended_pos = core::ExtendedPOS::Interjection;
      break;
    case dictionary::ConjugationType::NaAdjective:
      entry.extended_pos = core::ExtendedPOS::AdjNaAdj;
      break;
    case dictionary::ConjugationType::ProperFamily:
      entry.extended_pos = core::ExtendedPOS::NounProperFamily;
      break;
    case dictionary::ConjugationType::ProperGiven:
      entry.extended_pos = core::ExtendedPOS::NounProperGiven;
      break;
    default:
      entry.extended_pos =
          source_entry.is_proper_noun ? core::ExtendedPOS::NounProper : core::posToExtendedPos(entry.pos);
      break;
  }
  return entry;
}

bool needsExpansion(const dictionary::SourceEntry& source_entry) {
  const VerbType verb_type = conjTypeToVerbType(source_entry.conj_type);
  return dictionaryConjugationTypeIssue(source_entry).empty() &&
         ((source_entry.pos == core::PartOfSpeech::Verb && verb_type != VerbType::Unknown &&
           verb_type != VerbType::IAdjective) ||
          (source_entry.pos == core::PartOfSpeech::Adjective &&
           source_entry.conj_type == dictionary::ConjugationType::IAdjective));
}

std::string yoiVariantOf(std::string_view surface) {
  if (!utf8::endsWith(surface, "いい")) {
    return "";
  }
  std::string result(surface);
  result.resize(result.size() - core::kTwoJapaneseCharBytes);
  return result + "よい";
}

std::vector<dictionary::DictionaryEntry> expandIAdjective(const dictionary::DictionaryEntry& base_entry,
                                                          bool has_yoi_variant) {
  // Some いい spellings are supplementary forms whose productive paradigm is
  // carried by a separately registered ～よい sibling.  The sibling is lexical
  // evidence; spelling alone is not, because ordinary adjectives such as
  // かわいい also end in いい and retain the regular い-stem paradigm.
  if (!utf8::endsWith(base_entry.surface, "い") || has_yoi_variant) {
    return {base_entry};
  }

  const std::string stem(utf8::dropLastChar(base_entry.surface));
  std::vector<dictionary::DictionaryEntry> result;
  result.reserve(sizeof(kIAdjectiveSuffixes) / sizeof(kIAdjectiveSuffixes[0]));
  for (const auto& suffix : kIAdjectiveSuffixes) {
    result.push_back({stem + suffix.suffix, core::PartOfSpeech::Adjective, suffix.extended_pos, base_entry.lemma});
  }
  return result;
}

std::vector<dictionary::DictionaryEntry> expandVerb(const dictionary::DictionaryEntry& base_entry, VerbType verb_type) {
  std::vector<dictionary::DictionaryEntry> result;
  if (verb_type == VerbType::Kuru) {
    const std::string_view source_ending = utf8::endsWith(base_entry.surface, "来る") ? "来る" : "くる";
    const std::string prefix = base_entry.surface.substr(0, base_entry.surface.size() - source_ending.size());
    const auto lemma_for = [&](std::string_view ending) {
      if (!utf8::endsWith(base_entry.lemma, source_ending)) {
        return base_entry.lemma;
      }
      return normalize::concat(base_entry.lemma.substr(0, base_entry.lemma.size() - source_ending.size()), ending);
    };
    const std::string kanji_lemma = lemma_for("来る");
    const std::string old_kanji_lemma = lemma_for("來る");
    const std::string kana_lemma = lemma_for("くる");
    for (const auto& form : getKuruDictionaryForms()) {
      if (form.emit_kanji) {
        result.push_back({prefix + form.kanji_surface, core::PartOfSpeech::Verb, form.extended_pos, kanji_lemma});
        // The old spelling is unambiguous the same way the modern one is, so
        // it needs no separate emission gate. It keeps its own lemma because a
        // surface written in the old form belongs to the old-form entry.
        result.push_back(
            {prefix + form.old_kanji_surface, core::PartOfSpeech::Verb, form.extended_pos, old_kanji_lemma});
      }
      // A one-mora kana dictionary edge (き/こ) is indistinguishable from
      // ordinary word-internal kana and receives the short-dictionary-verb
      // bonus. Contextual candidate generation supplies the mizenkei before
      // a selecting auxiliary, so only materialize the safe 2+ mora forms.
      if (form.emit_kana && form.kana_surface.size() > core::kJapaneseCharBytes) {
        result.push_back({prefix + form.kana_surface, core::PartOfSpeech::Verb, form.extended_pos, kana_lemma});
      }
    }
    return result;
  }

  const Conjugation conjugation;
  const auto suffixes = conjugation.getDictionarySuffixes(verb_type, base_entry.surface);
  const std::string stem = Conjugation::getStem(base_entry.surface, verb_type);
  // A bound derivational suffix verb is written only onto a nominal host, so a
  // two-mora inflected spelling of one is indistinguishable from ordinary
  // word-internal kana — the same objection the one-mora くる forms above
  // answer (ばっ inside ばったり). Reverse inflection still reaches those cells
  // from the base entry, which is kept, and it sees the host. Longer stems are
  // unambiguous enough to materialize (がかっ, がかり).
  const bool bound_suffix_verb = isBoundDerivationalSuffixVerbLemma(base_entry.lemma);
  result.reserve(suffixes.size());
  for (const auto& suffix : suffixes) {
    std::string surface = stem + suffix.suffix;
    if (bound_suffix_verb && suffix.extended_pos != core::ExtendedPOS::VerbShuushikei &&
        surface.size() <= 2 * core::kJapaneseCharBytes) {
      continue;
    }
    result.push_back({std::move(surface), core::PartOfSpeech::Verb, suffix.extended_pos, base_entry.lemma});
  }
  return result.empty() ? std::vector<dictionary::DictionaryEntry>{base_entry} : result;
}

}  // namespace

std::string dictionaryConjugationTypeIssue(const dictionary::SourceEntry& source_entry) {
  const VerbType verb_type = conjTypeToVerbType(source_entry.conj_type);
  if (verb_type == VerbType::Unknown) {
    return "";
  }

  if (verb_type == VerbType::IAdjective) {
    if (source_entry.pos != core::PartOfSpeech::Adjective) {
      return "I_ADJ requires ADJECTIVE POS";
    }
    // I-adjective source data may intentionally carry a fixed non-basic form
    // (for example, a literary terminal form). expandIAdjective() preserves
    // such a surface literally, so unlike verb rows it has no unsafe stem
    // truncation to reject here.
    return "";
  }

  if (source_entry.pos != core::PartOfSpeech::Verb) {
    return "verb conjugation type requires VERB POS";
  }

  if (const auto* godan_row = Conjugation::getGodanRow(verb_type); godan_row != nullptr) {
    return utf8::lastChar(source_entry.surface) == normalize::encodeUtf8(godan_row->base_vowel)
               ? ""
               : "conjugation type requires its matching godan terminal kana";
  }
  if (verb_type == VerbType::Ichidan) {
    return utf8::endsWith(source_entry.surface, "る") ? "" : "ICHIDAN requires a surface ending in る";
  }
  if (verb_type == VerbType::Suru) {
    return utf8::endsWith(source_entry.surface, "する") ? "" : "SURU requires a surface ending in する";
  }
  if (verb_type == VerbType::Kuru) {
    return utf8::endsWith(source_entry.surface, "来る") || utf8::endsWith(source_entry.surface, "くる")
               ? ""
               : "KURU requires a surface ending in 来る or くる";
  }
  return "";
}

std::vector<dictionary::DictionaryEntry> expandDictionarySourceEntry(const dictionary::SourceEntry& source_entry) {
  auto base_entry = makeBaseEntry(source_entry);
  if (!needsExpansion(source_entry)) {
    return {std::move(base_entry)};
  }

  if (source_entry.pos == core::PartOfSpeech::Adjective) {
    base_entry.extended_pos = core::ExtendedPOS::AdjBasic;
    return expandIAdjective(base_entry, /*has_yoi_variant=*/false);
  }

  base_entry.extended_pos = core::ExtendedPOS::VerbShuushikei;
  return expandVerb(base_entry, conjTypeToVerbType(source_entry.conj_type));
}

DictionaryExpansionResult expandDictionarySourceEntries(const std::vector<dictionary::SourceEntry>& source_entries,
                                                        DictionaryExpansionOptions options) {
  DictionaryExpansionResult result;
  // The full-identity index is keyed by a string rather than by a struct so it
  // shares the string-keyed container below instead of instantiating a second
  // hash table. The header is fixed width and carries the surface length, so the
  // surface/lemma boundary is explicit and no byte inside either string can
  // forge a collision.
  auto entry_identity_key = [](std::string_view surface, core::PartOfSpeech pos, core::ExtendedPOS extended_pos,
                               std::string_view lemma) {
    std::string key;
    key.reserve(kIdentityHeaderBytes + surface.size() + lemma.size());
    key.push_back(static_cast<char>(pos));
    appendFixedWidth(key, static_cast<uint32_t>(extended_pos), kIdentityExtendedPOSBytes);
    appendFixedWidth(key, static_cast<uint32_t>(surface.size()), kIdentitySurfaceLengthBytes);
    key.append(surface);
    key.append(lemma);
    return key;
  };
  std::unordered_set<std::string> seen_entries;
  std::unordered_set<std::string> suppletive_yoi_variants;
  for (const auto& source_entry : source_entries) {
    if (source_entry.pos == core::PartOfSpeech::Adjective &&
        source_entry.conj_type == dictionary::ConjugationType::IAdjective) {
      suppletive_yoi_variants.insert(source_entry.surface);
    }
  }
  struct SeenSurface {
    size_t index;
    size_t lemma_length;
    core::PartOfSpeech pos;
    bool explicit_surface;
  };
  std::unordered_map<std::string, SeenSurface> seen_surfaces;
  // The same-POS index reuses the surface index's container by prefixing the
  // key with a fixed-width part of speech, so only one keyed container type is
  // instantiated. The prefix is fixed width, so no surface can straddle it.
  auto surface_pos_key = [](std::string_view surface, core::PartOfSpeech pos) {
    std::string key(1, static_cast<char>(pos));
    key.append(surface);
    return key;
  };
  std::unordered_map<std::string, SeenSurface> seen_surface_pos;

  auto append_source = [&](const dictionary::SourceEntry& source_entry) {
    auto base_entry = makeBaseEntry(source_entry);
    std::vector<dictionary::DictionaryEntry> expanded_entries;
    if (!needsExpansion(source_entry)) {
      expanded_entries = {std::move(base_entry)};
    } else if (source_entry.pos == core::PartOfSpeech::Adjective) {
      base_entry.extended_pos = core::ExtendedPOS::AdjBasic;
      expanded_entries =
          expandIAdjective(base_entry, suppletive_yoi_variants.count(yoiVariantOf(source_entry.surface)) > 0);
    } else {
      base_entry.extended_pos = core::ExtendedPOS::VerbShuushikei;
      expanded_entries = expandVerb(base_entry, conjTypeToVerbType(source_entry.conj_type));
    }
    const bool is_expanded = expanded_entries.size() > 1;
    for (auto& entry : expanded_entries) {
      const bool is_explicit_surface = entry.surface.compare(source_entry.surface) == 0;
      if (options.preserve_surface_homographs) {
        if (!options.preserve_generated_surface_homographs) {
          auto found = seen_surfaces.find(entry.surface);
          if (found != seen_surfaces.end()) {
            if (!is_explicit_surface) {
              if (!found->second.explicit_surface && entry.pos == found->second.pos &&
                  entry.lemma.size() > found->second.lemma_length) {
                const auto& previous = result.entries[found->second.index];
                seen_entries.erase(
                    entry_identity_key(previous.surface, previous.pos, previous.extended_pos, previous.lemma));
                result.entries[found->second.index] = std::move(entry);
                const auto& replacement = result.entries[found->second.index];
                found->second.lemma_length = replacement.lemma.size();
                seen_entries.insert(entry_identity_key(replacement.surface, replacement.pos, replacement.extended_pos,
                                                       replacement.lemma));
                if (!options.preserve_same_pos_homographs) {
                  seen_surface_pos.at(surface_pos_key(replacement.surface, replacement.pos)) = found->second;
                }
              }
              ++result.duplicates_skipped;
              continue;
            }
            if (!found->second.explicit_surface) {
              const auto& previous = result.entries[found->second.index];
              seen_entries.erase(
                  entry_identity_key(previous.surface, previous.pos, previous.extended_pos, previous.lemma));
              if (!options.preserve_same_pos_homographs) {
                seen_surface_pos.erase(surface_pos_key(previous.surface, previous.pos));
              }
              result.entries[found->second.index] = std::move(entry);
              const auto& replacement = result.entries[found->second.index];
              found->second = {found->second.index, replacement.lemma.size(), replacement.pos, true};
              seen_entries.insert(entry_identity_key(replacement.surface, replacement.pos, replacement.extended_pos,
                                                     replacement.lemma));
              if (!options.preserve_same_pos_homographs) {
                seen_surface_pos.emplace(surface_pos_key(replacement.surface, replacement.pos), found->second);
              }
              ++result.duplicates_skipped;
              continue;
            }
          }
        }
        auto identity = entry_identity_key(entry.surface, entry.pos, entry.extended_pos, entry.lemma);
        if (!seen_entries.insert(std::move(identity)).second) {
          ++result.duplicates_skipped;
          continue;
        }
        if (!options.preserve_same_pos_homographs) {
          const auto key = surface_pos_key(entry.surface, entry.pos);
          auto found = seen_surface_pos.find(key);
          if (found != seen_surface_pos.end()) {
            if (entry.lemma.size() > found->second.lemma_length) {
              found->second.lemma_length = entry.lemma.size();
              result.entries[found->second.index] = std::move(entry);
            }
            ++result.duplicates_skipped;
            continue;
          }
          seen_surface_pos.emplace(
              key, SeenSurface{result.entries.size(), entry.lemma.size(), entry.pos, is_explicit_surface});
        }
      } else {
        auto found = seen_surfaces.find(entry.surface);
        if (found != seen_surfaces.end()) {
          if (entry.pos == found->second.pos && entry.lemma.size() > found->second.lemma_length) {
            found->second.lemma_length = entry.lemma.size();
            result.entries[found->second.index] = std::move(entry);
          }
          ++result.duplicates_skipped;
          continue;
        }
        seen_surfaces.emplace(entry.surface,
                              SeenSurface{result.entries.size(), entry.lemma.size(), entry.pos, is_explicit_surface});
      }
      result.entries.push_back(std::move(entry));
      seen_surfaces.try_emplace(result.entries.back().surface,
                                SeenSurface{result.entries.size() - 1, result.entries.back().lemma.size(),
                                            result.entries.back().pos, is_explicit_surface});
      if (is_expanded) {
        ++result.expanded_forms;
      }
    }
  };

  for (const auto& source_entry : source_entries) {
    if (!needsExpansion(source_entry)) {
      append_source(source_entry);
    }
  }
  for (const auto& source_entry : source_entries) {
    if (needsExpansion(source_entry)) {
      append_source(source_entry);
    }
  }
  return result;
}

}  // namespace grammar
}  // namespace suzume
