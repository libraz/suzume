/**
 * @file unknown_speech.cpp
 * @brief Character-speech and onomatopoeia candidate generation for unknown words
 *
 * Split from unknown.cpp: houses UnknownWordGenerator::generateCharacterSpeechCandidates
 * and UnknownWordGenerator::generateOnomatopoeiaCandidates.
 */

#include <algorithm>
#include <array>
#include <cstdint>

#include "adjective_candidates.h"
#include "analysis/dictionary_probe.h"
#include "analysis/scorer_constants.h"
#include "analysis/unknown.h"
#include "candidate_constants.h"
#include "core/kana_constants.h"
#include "core/utf8_constants.h"
#include "grammar/char_patterns.h"
#include "normalize/char_type.h"
#include "normalize/exceptions.h"
#include "normalize/utf8.h"
#include "suffix_candidates.h"
#include "tokenizer_utils.h"
#include "verb_candidates.h"

namespace suzume::analysis {

namespace {

bool isBareVowelMora(char32_t codepoint) {
  return codepoint == U'あ' || codepoint == U'い' || codepoint == U'う' || codepoint == U'え' || codepoint == U'お';
}

// The colloquial quotative contracts a volitional う into its geminate
// (行こう+と -> 行こ+っと), so this shape can be a predicate plus a particle
// rather than a mimetic. A mimetic stem never spells a conjugated cell:
// require a listed verb irrealis of at least two morae ending on the o-row
// mora before the geminate, which a contracted volitional always leaves behind
// and a one-mora coincidence (にこっと, ちょこっと) never does.
bool closesContractedVolitional(const std::vector<char32_t>& codepoints, size_t tto_pos,
                                const dictionary::DictionaryManager* dict_manager) {
  constexpr size_t kMaxVolitionalMorae = 4;
  if (dict_manager == nullptr || tto_pos == 0 || !grammar::isORowCodepoint(codepoints[tto_pos - 1])) {
    return false;
  }
  for (size_t len = 2; len <= kMaxVolitionalMorae && len <= tto_pos; ++len) {
    const auto* entry = lookupEntryInRange(*dict_manager, codepoints, tto_pos - len, tto_pos, core::PartOfSpeech::Verb);
    if (entry != nullptr && entry->extended_pos == core::ExtendedPOS::VerbMizenkei) {
      return true;
    }
  }
  return false;
}

// Whether the dictionary carries a word that opens where a shape-derived
// candidate does and reaches past its end.
bool startsLongerDictionaryWord(const std::vector<char32_t>& codepoints, size_t start_pos, size_t end_pos,
                                const dictionary::DictionaryManager* dict_manager) {
  constexpr size_t kMaxTrailingChars = 3;
  if (dict_manager == nullptr) {
    return false;
  }
  const size_t probe_end = std::min(codepoints.size(), end_pos + kMaxTrailingChars);
  for (size_t word_end = end_pos + 1; word_end <= probe_end; ++word_end) {
    if (lookupEntryInRange(*dict_manager, codepoints, start_pos, word_end) != nullptr) {
      return true;
    }
  }
  return false;
}

}  // namespace

void UnknownWordGenerator::generateCharacterSpeechCandidates(std::string_view /*text*/,
                                                             const std::vector<char32_t>& codepoints, size_t start_pos,
                                                             const std::vector<normalize::CharType>& char_types,
                                                             std::vector<UnknownCandidate>& candidates) const {
  if (start_pos >= char_types.size()) {
    return;
  }

  normalize::CharType start_type = char_types[start_pos];

  // Only for hiragana or katakana starting positions
  if (start_type != normalize::CharType::Hiragana && start_type != normalize::CharType::Katakana) {
    return;
  }

  // Skip if starting with common particles (these are handled by dictionary)
  if (start_type == normalize::CharType::Hiragana) {
    char32_t first_char = codepoints[start_pos];
    if (normalize::isExtendedParticle(first_char)) {
      return;
    }
    // Skip small kana (ゃゅょぁぃぅぇぉっ) - these don't start words
    if (kana::isSmallKanaCodepoint(first_char)) {
      return;
    }
  }

  // Skip small katakana as well
  if (start_type == normalize::CharType::Katakana) {
    char32_t first_char = codepoints[start_pos];
    if (kana::isSmallKanaCodepoint(first_char)) {
      return;
    }
  }

  // Whitelist approach: only allow char speech starting with kana that can
  // begin valid auxiliary/character-speech patterns
  if (start_type == normalize::CharType::Hiragana) {
    char32_t first_char = codepoints[start_pos];
    // Valid char speech starters: sentence-ending speech patterns (ぞ,じゃ,のう,etc.)
    // and colloquial auxiliaries (ちゃ,じゃ,etc.)
    // Excludes: grammar chars handled by dict (た,さ,ら,く,あ,け,い,す,る)
    //           and kana that never start auxiliaries (ぱ行,ば行 sound-symbolics, etc.)
    bool valid_starter = first_char == U'ぞ' || first_char == U'じ' || first_char == U'の' || first_char == U'な' ||
                         first_char == U'ね' || first_char == U'よ' || first_char == U'わ' || first_char == U'で' ||
                         first_char == U'だ' || first_char == U'ま' || first_char == U'や' || first_char == U'か' ||
                         first_char == U'が' || first_char == U'べ' || first_char == U'ち' || first_char == U'に' ||
                         first_char == U'せ' || first_char == U'ず' || first_char == U'ど' || first_char == U'て' ||
                         first_char == U'も' || first_char == U'み' || first_char == U'ん' || first_char == U'そ' ||
                         first_char == U'と' || first_char == U'お' || first_char == U'は' || first_char == U'へ';
    if (!valid_starter) {
      return;
    }
  }

  size_t max_len = options_.max_character_speech_length;
  size_t text_len = char_types.size();

  // Find end of same-type sequence (limited to max_character_speech_length)
  size_t end_pos = start_pos + 1;
  while (end_pos < text_len && end_pos - start_pos < max_len && char_types[end_pos] == start_type) {
    ++end_pos;
  }

  // Check if this could be a sentence-end position
  auto isSentenceEndPosition = [&](size_t pos) -> bool {
    if (pos >= text_len) {
      return true;  // End of text
    }

    char32_t next_char = codepoints[pos];

    // Punctuation marks
    if (next_char == U'。' || next_char == U'！' || next_char == U'？' || next_char == U'、' || next_char == U',' ||
        next_char == U'.' || next_char == U'!' || next_char == U'?' || next_char == U'…' || next_char == U'」' ||
        next_char == U'』' || next_char == U'"' || next_char == U'\n' || next_char == U'\r') {
      return true;
    }

    // Whitespace (space, full-width space, tab)
    if (next_char == U' ' || next_char == U'　' || next_char == U'\t') {
      return true;
    }

    return false;
  };

  // Generate candidates for different lengths
  for (size_t len = 1; len <= end_pos - start_pos; ++len) {
    size_t candidate_end = start_pos + len;

    // Only generate if this position could be sentence-end
    if (!isSentenceEndPosition(candidate_end)) {
      continue;
    }

    std::string surface = extractSubstring(codepoints, start_pos, candidate_end);

    if (!surface.empty()) {
      // Skip patterns ending with そう - these are aspectual auxiliary patterns
      // that should be handled by verb/adjective + そう analysis, not as character speech
      if (utf8::endsWith(surface, scorer::kSuffixSou)) {
        continue;
      }

      // Skip generating AUX for common particle surfaces
      // These should be handled by the particle dictionary entries, not as auxiliaries
      // This prevents だけ from being generated as AUX (which gets VerbOnbinkei → AuxTenseTa bonus)
      static constexpr std::array<std::string_view, 13> kParticleSurfaces = {
          "だけ", "ばかり", "ほど", "くらい", "ぐらい", "など", "なんて",
          "しか", "まで",   "より", "から",   "かも",   "でも",
      };
      bool is_particle_surface = false;
      for (const auto& p : kParticleSurfaces) {
        if (surface == p) {
          is_particle_surface = true;
          break;
        }
      }
      if (is_particle_surface) {
        continue;  // Skip - let dictionary entry handle it
      }

      // A generic character-speech auxiliary defaults to the past-tense
      // connection class. Do not emit that lossy homograph when the same span
      // is already a closed auxiliary form: its dictionary candidate carries
      // the precise inflectional type and the same coarse Auxiliary POS.
      const auto* precise_auxiliary =
          dict_manager_ != nullptr ? dict_manager_->lookupExact(surface, core::PartOfSpeech::Auxiliary) : nullptr;
      const auto* lexical_verb =
          dict_manager_ != nullptr ? dict_manager_->lookupExact(surface, core::PartOfSpeech::Verb) : nullptr;
      if (precise_auxiliary != nullptr || lexical_verb != nullptr) {
        continue;
      }

      // Do not let an unknown character-speech edge absorb a regular copula
      // followed by a closed final particle. Both morphemes already have
      // precise dictionary candidates (だ+な, だ+よ, ...); treating their
      // concatenation as an opaque auxiliary permits invalid auxiliary chains.
      if (surface.size() > core::kJapaneseCharBytes &&
          std::string_view(surface).substr(0, core::kJapaneseCharBytes) == "だ" && dict_manager_ != nullptr) {
        const std::string_view tail(surface.data() + core::kJapaneseCharBytes,
                                    surface.size() - core::kJapaneseCharBytes);
        if (dict_manager_->lookupExact(std::string(tail), core::PartOfSpeech::Particle) != nullptr) {
          continue;
        }
      }

      // Independently registered closed forms form a grammatical chain, not
      // an opaque character-speech auxiliary (そう+や+で, 飲ん+だ+き). Test
      // every internal boundary so this stays a category rule instead of
      // naming a dialectal spelling; closed compound entries are handled
      // before this unknown candidate is considered.
      if (dict_manager_ != nullptr && surface.size() >= core::kJapaneseCharBytes * 2) {
        bool joins_two_particles = false;
        bool joins_two_auxiliaries = false;
        for (size_t split = 1; split < char_types.size() && start_pos + split < candidate_end; ++split) {
          const std::string prefix = extractSubstring(codepoints, start_pos, start_pos + split);
          const std::string suffix = extractSubstring(codepoints, start_pos + split, candidate_end);
          if (dict_manager_->lookupExact(prefix, core::PartOfSpeech::Particle) != nullptr &&
              dict_manager_->lookupExact(suffix, core::PartOfSpeech::Particle) != nullptr) {
            joins_two_particles = true;
            break;
          }
          if (dict_manager_->lookupExact(prefix, core::PartOfSpeech::Auxiliary) != nullptr &&
              dict_manager_->lookupExact(suffix, core::PartOfSpeech::Auxiliary) != nullptr) {
            joins_two_auxiliaries = true;
            break;
          }
        }
        if (joins_two_particles || joins_two_auxiliaries) {
          continue;
        }
      }

      // Calculate character count (not byte count)
      size_t char_count = surface.size() / core::kJapaneseCharBytes;

      // For single-character hiragana, only allow valid auxiliary forms
      // This prevents spurious splits like 玉ね+ぎ where ぎ is misanalyzed as た
      if (char_count == 1 && start_type == normalize::CharType::Hiragana) {
        // Valid single-char auxiliaries: た て ぬ む ん い せ れ ず よ ろ
        static const std::string_view kValidSingleCharAux[] = {
            "た", "て", "ぬ", "む", "ん", "い", "せ", "れ", "ず", "よ", "ろ",
        };
        bool is_valid_aux = false;
        for (const auto& valid : kValidSingleCharAux) {
          if (surface == valid) {
            is_valid_aux = true;
            break;
          }
        }
        if (!is_valid_aux) {
          continue;  // Skip invalid single-char auxiliary candidates
        }
      }

      // A multi-character character-speech ending is a copula or politeness
      // auxiliary carrying a final particle (のだ, ですぞ, ざます, だぜ), so it
      // opens on a mora that can begin one. Without the same shape check the
      // single-character branch applies, any two morae at a sentence end became
      // a past-tense auxiliary and peeled the tail off an unregistered
      // hiragana noun (いちご read as い + ちご, 汗まみれ as 汗ま + みれ).
      if (char_count >= 2 && start_type == normalize::CharType::Hiragana) {
        // A nominalizer followed by a second nominalizer is a compositional
        // clause boundary (〜て+ん+の), never one character-speech auxiliary.
        // Keep both dictionary particles available instead of manufacturing
        // an AUX_過去 candidate for んの.
        if (codepoints[start_pos] == U'ん' && codepoints[start_pos + 1] == U'の') {
          continue;
        }
        static constexpr std::string_view kAuxiliaryOpeners[] = {
            "た", "て", "ぬ", "む", "ん", "い", "せ", "れ", "ず", "よ", "ろ", "だ", "で",
            "じ", "ざ", "ま", "な", "の", "に", "っ", "わ", "ぜ", "ぞ", "さ", "や",
        };
        const std::string_view opener = std::string_view(surface).substr(0, core::kJapaneseCharBytes);
        bool opens_auxiliary = false;
        for (const auto& valid : kAuxiliaryOpeners) {
          if (opener == valid) {
            opens_auxiliary = true;
            break;
          }
        }
        if (!opens_auxiliary) {
          continue;
        }
      }

      // Apply length-based penalty for character speech
      // Short patterns (1-2 chars) like ぜ, のだ are common
      // Longer patterns like まむぎ (3+ chars) are rare
      float length_penalty = 0.0F;
      if (char_count >= 3) {
        // Penalty increases with length: 3chars=+2.0, 4chars=+4.0, etc.
        length_penalty = static_cast<float>(char_count - 2) * 2.0F;
      }

      // Skip katakana character speech candidates entirely
      // Katakana words are almost always loanword nouns (パン, キロ), not auxiliaries
      // Character speech (擬態語/擬声語) is almost exclusively written in hiragana
      if (start_type == normalize::CharType::Katakana) {
        continue;  // Skip - let same_type kata_seq handle katakana as NOUN
      }

      // Mark as Auxiliary so it connects properly after verbs/adjectives
      float cost = options_.character_speech_cost + length_penalty;
      auto cand = makeCandidate(surface, start_pos, candidate_end, core::PartOfSpeech::Auxiliary, cost, false,
                                CandidateOrigin::CharacterSpeech);
#ifdef SUZUME_DEBUG_INFO
      cand.confidence = 0.5F;
      cand.pattern = (start_type == normalize::CharType::Hiragana) ? "char_speech_hira" : "char_speech_kata";
#endif
      candidates.push_back(cand);
    }
  }
}

void UnknownWordGenerator::generateOnomatopoeiaCandidates(const std::vector<char32_t>& codepoints, size_t start_pos,
                                                          const std::vector<normalize::CharType>& char_types,
                                                          std::vector<UnknownCandidate>& candidates) const {
  // Need at least 3 characters for ABり pattern (4 for ABAB/AA patterns)
  if (start_pos + 2 >= codepoints.size()) {
    return;
  }

  normalize::CharType start_type = char_types[start_pos];

  // Helper to check if a character belongs to the same script group or is a modifier
  auto isSameScriptOrModifier = [&](size_t pos) -> bool {
    if (pos >= char_types.size())
      return false;
    if (pos >= codepoints.size())
      return false;
    // Same char type
    if (char_types[pos] == start_type)
      return true;
    // Prolonged sound mark (ー) can appear in both hiragana and katakana words
    if (normalize::isProlongedSoundMark(codepoints[pos]))
      return true;
    return false;
  };

  // Helper to check if a character is small kana (part of previous mora)
  auto isSmallKanaAt = [&](size_t pos) -> bool {
    return pos < codepoints.size() && kana::isSmallKanaCodepoint(codepoints[pos]);
  };

  // Find the bounded extent of a same-script mimetic (including ー).  Do not
  // scan an entire unsegmented kana run at every lattice position: mimetics
  // are prosodically bounded, and a small kana only modifies its preceding
  // mora.  The codepoint ceiling also advances through malformed small-kana
  // runs that would otherwise contain no countable mora.
  size_t seq_end = start_pos;
  size_t seq_morae = 0;
  while (seq_end < codepoints.size() && seq_end - start_pos < candidate::kMaxMimeticCodepoints &&
         seq_morae < candidate::kMaxMimeticMorae && isSameScriptOrModifier(seq_end)) {
    if (!isSmallKanaAt(seq_end)) {
      ++seq_morae;
    }
    ++seq_end;
  }

  size_t seq_len = seq_end - start_pos;

  // The quotative marker is only meaningful at the end of the complete
  // same-script run.  A bounded prefix ending in と must not be mistaken for
  // that construction.
  const bool sequence_is_complete = seq_end == codepoints.size() || !isSameScriptOrModifier(seq_end);

  // A quotative と commonly follows a mimetic adverb (ぷうぷうと、ちくたくと).
  // It is hiragana too, so exclude it from the shape check while leaving the
  // particle available as a separate morpheme.
  const bool has_trailing_quotative = sequence_is_complete && seq_len > 4 && codepoints[seq_end - 1] == U'と';
  const size_t mimetic_end = has_trailing_quotative ? seq_end - 1 : seq_end;
  const size_t mimetic_len = mimetic_end - start_pos;
  const size_t mimetic_morae = seq_morae - (has_trailing_quotative ? 1 : 0);

  // Try AA pattern: first half equals second half (ニャーニャー, ワンワン)
  // Sequence must have even length and be at least 4 chars
  if (mimetic_morae <= candidate::kMaxMimeticMorae && mimetic_len >= 4 && mimetic_len % 2 == 0) {
    size_t half_len = mimetic_len / 2;
    bool is_aa = true;

    // Check if first half equals second half
    for (size_t i = 0; i < half_len; ++i) {
      if (codepoints[start_pos + i] != codepoints[start_pos + half_len + i]) {
        is_aa = false;
        break;
      }
    }

    if (is_aa) {
      // Verify the first char of each half is not small kana
      // (small kana should be part of previous mora, not start a unit)
      if (!isSmallKanaAt(start_pos) && !isSmallKanaAt(start_pos + half_len)) {
        std::string surface = extractSubstring(codepoints, start_pos, mimetic_end);
        // Reduplication is a shape, not evidence about the word class. A run the
        // dictionary already carries has a lexical reading of its own, and a
        // mimetic adverb invented over the same span would outbid it on shape
        // alone (めちゃめちゃ is the na-adjective the dictionary lists).
        if (!surface.empty() && (dict_manager_ == nullptr || dict_manager_->lookupExact(surface) == nullptr)) {
          auto cand =
              makeCandidate(surface, start_pos, mimetic_end, core::PartOfSpeech::Adverb,
                            candidate::kMimeticExactReduplicationAdverbCost, true, CandidateOrigin::Onomatopoeia);
#ifdef SUZUME_DEBUG_INFO
          cand.confidence = 1.0F;
          cand.pattern = "aa_doubled";
#endif
          candidates.push_back(cand);
          return;  // Found a match, return early
        }
      }
    }
  }

  // The same reduplication may open a longer kana run instead of exhausting
  // it (ぐちゃぐちゃ|になった, ちょきちょき|した), and there the branch above
  // has nothing to compare halves against. Look for the doubling as a prefix,
  // longest first, so the shape is recognized independently of what follows.
  //
  // The half is measured in codepoints rather than fixed at two, because a
  // mora may be spelled with a small kana: ぐちゃ and ちょき are two morae in
  // three codepoints, and a fixed width reads them out of alignment. The
  // boundary conditions are what keep the halves mora-aligned — neither half
  // may open on a small kana, and the doubling may not end in the middle of a
  // mora either.
  //
  // A prefix is weaker evidence than a reduplication that is the whole run,
  // so it keeps the weaker cost and does not return early: the surrounding
  // readings still compete with it.
  constexpr size_t kMinReduplicationHalf = 2;
  // A run of one repeated codepoint (もももも) is emphatic lengthening, not a
  // reduplicated stem. Every half of such a run matches, so recognizing it
  // once here also keeps the scan below from comparing each width in full.
  size_t uniform_prefix = start_pos;
  while (uniform_prefix < mimetic_end && codepoints[uniform_prefix] == codepoints[start_pos]) {
    ++uniform_prefix;
  }
  for (size_t half = mimetic_len / 2; half >= kMinReduplicationHalf && uniform_prefix < mimetic_end; --half) {
    const size_t doubled_end = start_pos + (2 * half);
    // The doubling repeats the opening codepoint at the second half, so one
    // comparison rejects most widths before the halves are walked.
    if (codepoints[start_pos + half] != codepoints[start_pos] || isSmallKanaAt(start_pos) ||
        isSmallKanaAt(doubled_end) || uniform_prefix - start_pos >= half) {
      continue;
    }
    bool halves_match = true;
    for (size_t offset = 1; offset < half; ++offset) {
      if (codepoints[start_pos + offset] != codepoints[start_pos + half + offset]) {
        halves_match = false;
        break;
      }
    }
    if (!halves_match) {
      continue;
    }
    std::string surface = extractSubstring(codepoints, start_pos, doubled_end);
    if (!surface.empty()) {
      auto cand = makeCandidate(surface, start_pos, doubled_end, core::PartOfSpeech::Adverb, 0.1F, true,
                                CandidateOrigin::Onomatopoeia);
#ifdef SUZUME_DEBUG_INFO
      cand.confidence = 1.0F;
      cand.pattern = "reduplicated_prefix";
#endif
      candidates.push_back(cand);
    }
    break;
  }

  // A heterogeneous four-mora form followed by quotative と is another
  // productive mimetic shape (ちくたくと).  The particle gate prevents a
  // generic four-hiragana run from becoming an adverb without syntactic
  // evidence, while the earlier AA/ABAB branches retain their stronger costs.
  if (has_trailing_quotative && seq_end < codepoints.size() && mimetic_len == 4 &&
      start_type == normalize::CharType::Hiragana && !normalize::isParticleCodepoint(codepoints[start_pos])) {
    bool heterogeneous_has_small_kana = false;
    for (size_t offset = 0; offset < mimetic_len; ++offset) {
      heterogeneous_has_small_kana = heterogeneous_has_small_kana || isSmallKanaAt(start_pos + offset);
    }
    if (!heterogeneous_has_small_kana) {
      std::string surface = extractSubstring(codepoints, start_pos, mimetic_end);
      bool decomposes_as_predicate_particle = false;
      if (dict_manager_ != nullptr) {
        // The particle only has to begin at the split, not fill the rest of the
        // run: a mimetic never opens with a predicate, so whatever follows that
        // particle belongs to the next word (だっ|た+と+なる|と reads the past
        // auxiliary, the quotative and なる as one fabricated adverb たとなる).
        for (size_t split = start_pos + 1; split < mimetic_end && !decomposes_as_predicate_particle; ++split) {
          const std::string left = extractSubstring(codepoints, start_pos, split);
          constexpr PartOfSpeechMask kPredicateMask = partOfSpeechMask(core::PartOfSpeech::Verb) |
                                                      partOfSpeechMask(core::PartOfSpeech::Adjective) |
                                                      partOfSpeechMask(core::PartOfSpeech::Auxiliary);
          if (!hasExactPartOfSpeech(*dict_manager_, left, kPredicateMask)) {
            continue;
          }
          for (size_t particle_end = split + 1; particle_end <= mimetic_end; ++particle_end) {
            if (lookupEntryInRange(*dict_manager_, codepoints, split, particle_end, core::PartOfSpeech::Particle) !=
                nullptr) {
              decomposes_as_predicate_particle = true;
              break;
            }
          }
        }
      }
      if (!surface.empty() && !decomposes_as_predicate_particle) {
        auto cand = makeCandidate(surface, start_pos, mimetic_end, core::PartOfSpeech::Adverb,
                                  candidate::kMimeticHeterogeneousAdverbCost, true, CandidateOrigin::Onomatopoeia);
#ifdef SUZUME_DEBUG_INFO
        cand.confidence = candidate::kHighOriginConfidence;
        cand.pattern = "heterogeneous_four_mora_quotative";
#endif
        candidates.push_back(cand);
      }
    }
  }

  // Nasal manner mimetics: Xんと (しんと) and XんYり (しんみり,
  // すんなり). The fixed nasal position plus the adverbial ending supplies
  // stronger evidence than an arbitrary hiragana run.
  if (start_type == normalize::CharType::Hiragana && seq_len >= 3 && codepoints[start_pos + 1] == U'ん') {
    size_t pattern_end = start_pos;
    const char* pattern = nullptr;
    if (codepoints[start_pos + 2] == U'と') {
      pattern_end = start_pos + 3;
      pattern = "x_nto_pattern";
    } else if (seq_len >= 4 && codepoints[start_pos + 3] == U'り') {
      pattern_end = start_pos + 4;
      pattern = "x_ny_ri_pattern";
    }
    if (pattern != nullptr) {
      auto cand = makeCandidate(extractSubstring(codepoints, start_pos, pattern_end), start_pos, pattern_end,
                                core::PartOfSpeech::Adverb, candidate::kMimeticNtoAdverbBonus, true,
                                CandidateOrigin::Onomatopoeia);
#ifdef SUZUME_DEBUG_INFO
      cand.confidence = candidate::kHighOriginConfidence;
      cand.pattern = pattern;
#endif
      candidates.push_back(std::move(cand));
    }
  }

  // Alternating nasal compound mimetics — two equal halves that each close on
  // ん (さん|ざん, どたん|ばたん) — form one search unit even when the halves
  // differ. The shape alone is also the shape of a kana-spelled Sino-Japanese
  // noun (にんげん, しんぶん), so it needs licensing evidence: either a
  // following quotative と, or sequential voicing on the second half, which is
  // the reduplication marker itself and cannot arise across a word boundary.
  // Like the other reduplication shapes this one is prosodically bounded, so
  // read it off a window at start_pos instead of splitting the whole run: the
  // form keeps its shape in front of a copula (さんざん|だ) where the run does
  // not divide evenly.
  if (start_type == normalize::CharType::Hiragana) {
    for (const size_t half_len : {size_t{2}, size_t{3}}) {
      const size_t second_half = start_pos + half_len;
      const size_t form_end = second_half + half_len;
      if (form_end > mimetic_end || codepoints[second_half - 1] != U'ん' || codepoints[form_end - 1] != U'ん') {
        continue;
      }
      bool voiced_reduplication = kana::isSequentialVoicingPair(codepoints[start_pos], codepoints[second_half]);
      for (size_t offset = 1; offset < half_len && voiced_reduplication; ++offset) {
        voiced_reduplication = codepoints[start_pos + offset] == codepoints[second_half + offset];
      }
      const bool licensed = voiced_reduplication || (has_trailing_quotative && form_end == mimetic_end);
      if (!licensed) {
        continue;
      }
      auto cand = makeCandidate(extractSubstring(codepoints, start_pos, form_end), start_pos, form_end,
                                core::PartOfSpeech::Adverb, candidate::kMimeticAlternatingNasalAdverbCost, true,
                                CandidateOrigin::Onomatopoeia);
#ifdef SUZUME_DEBUG_INFO
      cand.confidence = candidate::kHighOriginConfidence;
      cand.pattern = "alternating_nasal_mimetic";
#endif
      candidates.push_back(std::move(cand));
    }
  }

  // Try ABり / AっBり patterns (e.g., どさり, ばたり, ぐったり,
  // じっくり). The same-script run can continue through quotative と (and the
  // four-character form can precede a hiragana predicate), so recognize the
  // structural prefix rather than requiring り to end the whole run.
  if (seq_len >= 3 && start_type == normalize::CharType::Hiragana) {
    if (codepoints[start_pos + 2] == U'り' && (seq_len == 3 || (seq_len > 3 && codepoints[start_pos + 3] == U'と'))) {
      // Three-character patterns are extended past the run boundary only when
      // followed by the adverbial marker と, which limits prefix false positives.
      // Skip if first char is a common particle (の, は, が, を, に, で, も, と, へ, か)
      // to avoid false matches like のやり, はしり, がわり
      char32_t first = codepoints[start_pos];
      // An attested Godan-ra continuative has the same AB+り shape (めぐり,
      // かぎり). The verb reading owns it; a mimetic candidate here would
      // outrank it on the adverb connection alone.
      const bool is_godan_ra_continuative =
          dict_manager_ != nullptr &&
          dict_manager_->lookupExact(extractSubstring(codepoints, start_pos, start_pos + 2) + "る",
                                     core::PartOfSpeech::Verb) != nullptr;
      if (!normalize::isParticleCodepoint(first) && !isBareVowelMora(first) && !kana::isRaColumnCodepoint(first) &&
          !is_godan_ra_continuative) {
        std::string surface = extractSubstring(codepoints, start_pos, start_pos + 3);
        if (!surface.empty()) {
          auto cand = makeCandidate(surface, start_pos, start_pos + 3, core::PartOfSpeech::Adverb, 0.7F, true,
                                    CandidateOrigin::Onomatopoeia);
#ifdef SUZUME_DEBUG_INFO
          cand.confidence = 0.7F;
          cand.pattern = "ab_ri_pattern";
#endif
          candidates.push_back(cand);
        }
      }
    }

    // Four-character patterns like ぐったり and じっくり.
    if (seq_len >= 4 && isSmallKanaAt(start_pos + 1) && codepoints[start_pos + 3] == U'り') {
      const auto* tail_particle = dict_manager_ != nullptr
                                      ? lookupEntryInRange(*dict_manager_, codepoints, start_pos + 2, start_pos + 4,
                                                           core::PartOfSpeech::Particle)
                                      : nullptr;
      const std::string predicate_stem = extractSubstring(codepoints, start_pos, start_pos + 2);
      constexpr PartOfSpeechMask kPredicateMask =
          partOfSpeechMask(core::PartOfSpeech::Verb) | partOfSpeechMask(core::PartOfSpeech::Auxiliary);
      const bool has_exact_predicate_stem =
          dict_manager_ != nullptr && hasExactPartOfSpeech(*dict_manager_, predicate_stem, kPredicateMask);
      const bool is_conjunctive_auxiliary_tail = has_exact_predicate_stem && tail_particle != nullptr &&
                                                 tail_particle->extended_pos == core::ExtendedPOS::ParticleConj;
      std::string surface = extractSubstring(codepoints, start_pos, start_pos + 4);
      if (!surface.empty() && !is_conjunctive_auxiliary_tail) {
        auto cand = makeCandidate(surface, start_pos, start_pos + 4, core::PartOfSpeech::Adverb,
                                  candidate::kMimeticSokuonMannerAdverbCost, true, CandidateOrigin::Onomatopoeia);
#ifdef SUZUME_DEBUG_INFO
        cand.confidence = candidate::kMimeticSokuonMannerConfidence;
        cand.pattern = "xtu_cv_ri_pattern";
#endif
        candidates.push_back(cand);
      }
    }

    // Four-character AっBら patterns are manner adverbs too.  They share the
    // sokuon-plus-mora shape of the AっBり family, but use the adverbial ら
    // ending; retain the complete structural unit before a following
    // predicate instead of treating its first mora as a verb stem.
    if (seq_len >= 4 && isSmallKanaAt(start_pos + 1) && codepoints[start_pos + 3] == U'ら' &&
        !normalize::isParticleCodepoint(codepoints[start_pos])) {
      std::string surface = extractSubstring(codepoints, start_pos, start_pos + 4);
      if (!surface.empty()) {
        auto cand = makeCandidate(surface, start_pos, start_pos + 4, core::PartOfSpeech::Adverb,
                                  candidate::kMimeticSokuonMannerAdverbCost, true, CandidateOrigin::Onomatopoeia);
#ifdef SUZUME_DEBUG_INFO
        cand.confidence = candidate::kMimeticSokuonMannerConfidence;
        cand.pattern = "xtu_cv_ra_pattern";
#endif
        candidates.push_back(cand);
      }
    }
  }

  // Try XXんと pattern for manner adverbs (きちんと, ちゃんと). A two-or-more
  // mora stem followed by んと is a productive mimetic shape. Require the
  // complete four-character prefix and a non-particle start so that the rule
  // does not absorb ordinary one-mora words or particle sequences.
  if (seq_len >= 4 && start_type == normalize::CharType::Hiragana && codepoints[start_pos + 2] == U'ん' &&
      codepoints[start_pos + 3] == U'と' && !normalize::isParticleCodepoint(codepoints[start_pos])) {
    std::string surface = extractSubstring(codepoints, start_pos, start_pos + 4);
    if (!surface.empty()) {
      auto cand = makeCandidate(surface, start_pos, start_pos + 4, core::PartOfSpeech::Adverb,
                                candidate::kMimeticNtoAdverbBonus, true, CandidateOrigin::Onomatopoeia);
#ifdef SUZUME_DEBUG_INFO
      cand.confidence = candidate::kHighOriginConfidence;
      cand.pattern = "xx_nto_pattern";
#endif
      candidates.push_back(cand);
    }
  }

  // Try Xっと pattern for onomatopoeia adverbs (はっと, ぐっと, どきっと, ぷるんっと)
  // Pattern: [hiragana]{1,4} + っと where the hiragana sequence is the onomatopoeia stem
  // These are mimetic/onomatopoeia adverbs that precede する/くる conjugations
  // E.g., はっとした, ぐっときた, どきっとした, ぷるんっとした
  if (seq_len >= 3 && start_type == normalize::CharType::Hiragana) {
    // Look for っと at various positions within the sequence
    for (size_t tto_pos = start_pos + 1; tto_pos + 1 < seq_end && tto_pos <= start_pos + 4; ++tto_pos) {
      if (codepoints[tto_pos] == U'っ' &&                               // っ (small tsu)
          tto_pos + 1 < seq_end && codepoints[tto_pos + 1] == U'と') {  // と
        size_t adv_end = tto_pos + 2;
        size_t stem_len = tto_pos - start_pos;  // chars before っ
        // Stem should be 1-4 hiragana chars
        if (stem_len >= 1 && stem_len <= 4) {
          // Skip if stem starts with a particle character (e.g., にもっと = に+もっと)
          char32_t first_cp = codepoints[start_pos];
          const bool particle_start = first_cp == U'に' || first_cp == U'は' || first_cp == U'も' ||
                                      first_cp == U'を' || first_cp == U'が' || first_cp == U'で' ||
                                      first_cp == U'と' || first_cp == U'か' || first_cp == U'の' || first_cp == U'へ';
          if (stem_len > 2 && particle_start) {
            break;
          }
          if (closesContractedVolitional(codepoints, tto_pos, dict_manager_)) {
            break;
          }
          // A mimetic is read off the shape of the run, so it must not carve
          // into a word the dictionary carries: the same っと closes the stem
          // of ordinary lexical verbs (のっと+る against のっとる).
          if (startsLongerDictionaryWord(codepoints, start_pos, adv_end, dict_manager_)) {
            break;
          }
          std::string surface = extractSubstring(codepoints, start_pos, adv_end);
          if (!surface.empty()) {
            // Strong bonus for short patterns (はっと, ぐっと = very common)
            // Needs to beat hiragana verb candidates that absorb the っと
            float cost = (stem_len <= 2) ? -1.5F : -0.5F;
            auto cand = makeCandidate(surface, start_pos, adv_end, core::PartOfSpeech::Adverb, cost, true,
                                      CandidateOrigin::Onomatopoeia);
            cand.rejects_preceding_content_edge = particle_start && stem_len == 2;
#ifdef SUZUME_DEBUG_INFO
            cand.confidence = 0.9F;
            cand.pattern = "x_tto_pattern";
#endif
            candidates.push_back(cand);
          }
        }
        break;  // Only match the first っと position
      }
    }
  }
}

}  // namespace suzume::analysis
