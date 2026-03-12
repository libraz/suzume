#include "analysis/analyzer.h"

#include <algorithm>

#include "core/debug.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"

namespace suzume::analysis {

namespace {

// Maximum chunk size in bytes (~10K Japanese characters).
// Keeps Viterbi memory under ~3MB per chunk.
constexpr size_t kMaxChunkBytes = 32768;

// Check if byte position is a sentence boundary character.
// Returns the number of bytes to include (0 if not a boundary).
inline size_t sentenceBoundaryLen(std::string_view text, size_t pos) {
  auto c = static_cast<unsigned char>(text[pos]);
  // ASCII: \n, !, ?
  if (c == '\n' || c == '!' || c == '?') {
    return 1;
  }
  // 3-byte UTF-8 sequences
  if (pos + 2 < text.size() && c == 0xE3) {
    auto c1 = static_cast<unsigned char>(text[pos + 1]);
    auto c2 = static_cast<unsigned char>(text[pos + 2]);
    // 。(U+3002) = E3 80 82
    if (c1 == 0x80 && c2 == 0x82) return 3;
  }
  if (pos + 2 < text.size() && c == 0xEF) {
    auto c1 = static_cast<unsigned char>(text[pos + 1]);
    auto c2 = static_cast<unsigned char>(text[pos + 2]);
    // ！(U+FF01) = EF BC 81
    if (c1 == 0xBC && c2 == 0x81) return 3;
    // ？(U+FF1F) = EF BC 9F
    if (c1 == 0xBC && c2 == 0x9F) return 3;
  }
  return 0;
}

// Find the last UTF-8 character boundary at or before pos.
inline size_t findUtf8Boundary(std::string_view text, size_t pos) {
  while (pos > 0 && (static_cast<unsigned char>(text[pos]) & 0xC0) == 0x80) {
    --pos;
  }
  return pos;
}

// Count UTF-8 characters in a byte range.
inline size_t countChars(std::string_view text, size_t from, size_t to) {
  size_t count = 0;
  for (size_t i = from; i < to; ++i) {
    if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80) {
      ++count;
    }
  }
  return count;
}

}  // namespace

Analyzer::Analyzer(const AnalyzerOptions& options)
    : options_(options),
      normalizer_(options.normalize_options),
      pretokenizer_(),
      scorer_(options.scorer_options),
      unknown_gen_(options.unknown_options, &dict_manager_),
      tokenizer_(nullptr) {
  tokenizer_ =
      std::make_unique<Tokenizer>(dict_manager_, scorer_, unknown_gen_);
}

Analyzer::~Analyzer() = default;

void Analyzer::addUserDictionary(
    std::shared_ptr<dictionary::UserDictionary> dict) {
  dict_manager_.addUserDictionary(std::move(dict));
  // Rebuild tokenizer with new dictionary
  tokenizer_ =
      std::make_unique<Tokenizer>(dict_manager_, scorer_, unknown_gen_);
}

bool Analyzer::tryAutoLoadCoreDictionary() {
  bool loaded = dict_manager_.tryAutoLoadCoreDictionary();
  if (loaded) {
    // Rebuild tokenizer with new dictionary
    tokenizer_ =
        std::make_unique<Tokenizer>(dict_manager_, scorer_, unknown_gen_);
  }
  return loaded;
}

bool Analyzer::hasCoreBinaryDictionary() const {
  return dict_manager_.hasCoreBinaryDictionary();
}

std::vector<core::Morpheme> Analyzer::analyze(std::string_view text) const {
  if (text.empty()) {
    return {};
  }

  // Short text: process directly
  if (text.size() <= kMaxChunkBytes) {
    return analyzeWithPretokenizer(text, 0);
  }

  // Long text: split at sentence boundaries before pretokenizer
  // This prevents pretokenizer from scanning 100MB+ in one pass.
  std::vector<core::Morpheme> result;
  size_t pos = 0;
  size_t char_pos = 0;

  while (pos < text.size()) {
    size_t scan_end = std::min(pos + kMaxChunkBytes, text.size());
    size_t best_break = 0;

    for (size_t i = pos; i < scan_end; ) {
      size_t blen = sentenceBoundaryLen(text, i);
      if (blen > 0) {
        best_break = i + blen;
        i += blen;
      } else {
        ++i;
      }
    }

    size_t chunk_end;
    if (scan_end >= text.size()) {
      chunk_end = text.size();
    } else if (best_break > pos) {
      chunk_end = best_break;
    } else {
      chunk_end = findUtf8Boundary(text, scan_end);
      if (chunk_end <= pos) {
        chunk_end = scan_end;
      }
    }

    auto morphemes = analyzeWithPretokenizer(
        text.substr(pos, chunk_end - pos), char_pos);
    for (auto& m : morphemes) {
      result.push_back(std::move(m));
    }

    char_pos += countChars(text, pos, chunk_end);
    pos = chunk_end;
  }

  return result;
}

std::vector<core::Morpheme> Analyzer::analyzeWithPretokenizer(
    std::string_view text, size_t base_char_offset) const {
  if (text.empty()) {
    return {};
  }

  // Run pretokenizer
  auto pretoken_result = pretokenizer_.process(text);

  // If no pretokens found, just analyze normally
  if (pretoken_result.tokens.empty()) {
    return analyzeSpan(text, base_char_offset);
  }

  // Merge pretokens and analyzed spans
  std::vector<core::Morpheme> result;

  // Track current position for character offset calculation
  size_t current_byte = 0;
  size_t current_char = 0;

  // Create a combined view of all items sorted by start position
  struct Item {
    size_t start;
    size_t end;
    bool is_pretoken;
    size_t index;
  };
  std::vector<Item> items;

  for (size_t idx = 0; idx < pretoken_result.tokens.size(); ++idx) {
    const auto& tok = pretoken_result.tokens[idx];
    items.push_back({tok.start, tok.end, true, idx});
  }
  for (size_t idx = 0; idx < pretoken_result.spans.size(); ++idx) {
    const auto& span = pretoken_result.spans[idx];
    items.push_back({span.start, span.end, false, idx});
  }

  std::sort(items.begin(), items.end(),
            [](const Item& lhs, const Item& rhs) { return lhs.start < rhs.start; });

  for (const auto& item : items) {
    // Calculate character offset at this byte position
    while (current_byte < item.start && current_byte < text.size()) {
      if ((static_cast<unsigned char>(text[current_byte]) & 0xC0) != 0x80) {
        ++current_char;
      }
      ++current_byte;
    }
    size_t char_offset = base_char_offset + current_char;

    if (item.is_pretoken) {
      // Convert pretoken to morpheme
      const auto& tok = pretoken_result.tokens[item.index];
      core::Morpheme morpheme;
      morpheme.surface = tok.surface;
      morpheme.pos = tok.pos;
      morpheme.lemma = tok.surface;
      morpheme.start_pos = char_offset;

      // Calculate end char offset
      size_t end_byte = current_byte;
      size_t end_char = current_char;
      while (end_byte < tok.end && end_byte < text.size()) {
        if ((static_cast<unsigned char>(text[end_byte]) & 0xC0) != 0x80) {
          ++end_char;
        }
        ++end_byte;
      }
      morpheme.end_pos = base_char_offset + end_char;
      morpheme.start = char_offset;
      morpheme.end = base_char_offset + end_char;
      morpheme.is_from_dictionary = false;
      morpheme.is_unknown = false;
      result.push_back(std::move(morpheme));
    } else {
      // Analyze span
      const auto& span = pretoken_result.spans[item.index];
      std::string_view span_text = text.substr(span.start, span.end - span.start);
      auto span_morphemes = analyzeSpan(span_text, char_offset);

      for (auto& morph : span_morphemes) {
        result.push_back(std::move(morph));
      }
    }
  }

  return result;
}

std::vector<core::Morpheme> Analyzer::analyzeSpan(std::string_view text,
                                                  size_t char_offset) const {
  if (text.empty()) {
    return {};
  }

  // Short text: analyze directly without chunking overhead
  if (text.size() <= kMaxChunkBytes) {
    return analyzeChunk(text, char_offset);
  }

  // Long text: split at sentence boundaries to bound memory usage
  std::vector<core::Morpheme> result;
  size_t pos = 0;
  size_t char_pos = 0;

  while (pos < text.size()) {
    // Scan forward up to kMaxChunkBytes looking for last sentence boundary
    size_t scan_end = std::min(pos + kMaxChunkBytes, text.size());
    size_t best_break = 0;  // byte position of best break point (after boundary char)

    for (size_t i = pos; i < scan_end; ) {
      size_t blen = sentenceBoundaryLen(text, i);
      if (blen > 0) {
        best_break = i + blen;
        i += blen;
      } else {
        ++i;
      }
    }

    size_t chunk_end;
    if (scan_end >= text.size()) {
      // Last chunk: take everything
      chunk_end = text.size();
    } else if (best_break > pos) {
      // Split at the last sentence boundary within range
      chunk_end = best_break;
    } else {
      // No sentence boundary found: split at UTF-8 character boundary
      chunk_end = findUtf8Boundary(text, scan_end);
      if (chunk_end <= pos) {
        chunk_end = scan_end;  // Safety: advance at least to scan_end
      }
    }

    // Analyze this chunk
    auto morphemes = analyzeChunk(text.substr(pos, chunk_end - pos),
                                  char_offset + char_pos);
    for (auto& m : morphemes) {
      result.push_back(std::move(m));
    }

    // Count characters in this chunk for offset tracking
    char_pos += countChars(text, pos, chunk_end);
    pos = chunk_end;
  }

  return result;
}

std::vector<core::Morpheme> Analyzer::analyzeChunk(std::string_view text,
                                                   size_t char_offset) const {
  if (text.empty()) {
    return {};
  }

  // Normalize text
  auto norm_result = normalizer_.normalize(text);
  if (!core::isSuccess(norm_result)) {
    SUZUME_DEBUG_BLOCK {
      auto& error = std::get<core::Error>(norm_result);
      SUZUME_DEBUG_STREAM << "[ANALYZER] Normalization failed: "
                          << error.message
                          << " (code=" << static_cast<int>(error.code) << ")\n";
    }
    return {};
  }
  std::string normalized = std::get<std::string>(norm_result);
  if (normalized.empty()) {
    return {};
  }

  // Decode to codepoints
  std::vector<char32_t> codepoints = normalize::utf8::decode(normalized);
  if (codepoints.empty()) {
    SUZUME_DEBUG_LOG("[ANALYZER] UTF-8 decode failed\n");
    return {};
  }

  // Get character types
  std::vector<normalize::CharType> char_types;
  char_types.reserve(codepoints.size());
  for (char32_t code : codepoints) {
    char_types.push_back(normalize::classifyChar(code));
  }

  // Build lattice
  core::Lattice lattice =
      tokenizer_->buildLattice(normalized, codepoints, char_types);

  // Check if lattice is valid
  if (!lattice.isValid()) {
    core::Morpheme morpheme;
    morpheme.surface = std::string(text);
    morpheme.pos = core::PartOfSpeech::Noun;
    morpheme.start_pos = char_offset;
    morpheme.end_pos = char_offset + codepoints.size();
    morpheme.start = char_offset;
    morpheme.end = char_offset + codepoints.size();
    return {morpheme};
  }

  // Run Viterbi
  core::ViterbiResult vresult = viterbi_.solve(lattice, scorer_);

  // Convert to morphemes with offset adjustment
  std::vector<core::Morpheme> morphemes;
  morphemes.reserve(vresult.path.size());

  for (size_t edge_id : vresult.path) {
    const core::LatticeEdge& edge = lattice.getEdge(edge_id);

    core::Morpheme morpheme;
    morpheme.surface = std::string(edge.surface);
    morpheme.pos = edge.pos;
    morpheme.extended_pos = edge.extended_pos;
    morpheme.start = char_offset + edge.start;
    morpheme.end = char_offset + edge.end;
    morpheme.start_pos = char_offset + edge.start;
    morpheme.end_pos = char_offset + edge.end;

    if (!edge.lemma.empty()) {
      morpheme.lemma = std::string(edge.lemma);
    } else {
      morpheme.lemma = morpheme.surface;
    }

    if (!edge.reading.empty()) {
      morpheme.reading = std::string(edge.reading);
    }

    morpheme.features.is_dictionary = edge.fromDictionary();
    morpheme.features.is_user_dict = edge.fromUserDict();
    morpheme.features.is_formal_noun = edge.isFormalNoun();
    morpheme.features.is_low_info = edge.isLowInfo();
    morpheme.features.score = edge.cost;
    morpheme.is_from_dictionary = edge.fromDictionary();
    morpheme.is_unknown = edge.isUnknown();
    morpheme.conj_type = edge.conj_type;

    morphemes.push_back(std::move(morpheme));
  }

  return morphemes;
}

std::vector<core::Morpheme> Analyzer::pathToMorphemes(const core::ViterbiResult& result, const core::Lattice& lattice,
                                                      std::string_view /*original_text*/) {
  std::vector<core::Morpheme> morphemes;
  morphemes.reserve(result.path.size());

  for (size_t edge_id : result.path) {
    const core::LatticeEdge& edge = lattice.getEdge(edge_id);

    core::Morpheme morpheme;
    morpheme.surface = std::string(edge.surface);
    morpheme.pos = edge.pos;
    morpheme.extended_pos = edge.extended_pos;
    morpheme.start = edge.start;
    morpheme.end = edge.end;
    morpheme.start_pos = edge.start;
    morpheme.end_pos = edge.end;

    if (!edge.lemma.empty()) {
      morpheme.lemma = std::string(edge.lemma);
    } else {
      morpheme.lemma = morpheme.surface;
    }

    if (!edge.reading.empty()) {
      morpheme.reading = std::string(edge.reading);
    }

    morpheme.features.is_dictionary = edge.fromDictionary();
    morpheme.features.is_user_dict = edge.fromUserDict();
    morpheme.features.is_formal_noun = edge.isFormalNoun();
    morpheme.features.is_low_info = edge.isLowInfo();
    morpheme.features.score = edge.cost;
    morpheme.is_from_dictionary = edge.fromDictionary();
    morpheme.is_unknown = edge.isUnknown();
    morpheme.conj_type = edge.conj_type;

    morphemes.push_back(std::move(morpheme));
  }

  return morphemes;
}

std::vector<core::Morpheme> Analyzer::analyzeDebug(std::string_view text,
                                                   core::Lattice* out_lattice) const {
  if (text.empty()) {
    return {};
  }

  // Normalize text
  auto norm_result = normalizer_.normalize(text);
  if (!core::isSuccess(norm_result)) {
    // Log normalization failure (likely invalid UTF-8 input)
    SUZUME_DEBUG_BLOCK {
      auto& error = std::get<core::Error>(norm_result);
      SUZUME_DEBUG_STREAM << "[ANALYZER] Normalization failed in analyzeDebug: "
                          << error.message
                          << " (code=" << static_cast<int>(error.code) << ")\n";
    }
    return {};  // Return empty vector for invalid input
  }
  std::string normalized = std::get<std::string>(norm_result);
  if (normalized.empty()) {
    return {};
  }

  // Decode to codepoints
  std::vector<char32_t> codepoints = normalize::utf8::decode(normalized);
  if (codepoints.empty()) {
    SUZUME_DEBUG_LOG("[ANALYZER] UTF-8 decode failed in analyzeDebug\n");
    return {};  // Return empty vector for decode failure
  }

  // Get character types
  std::vector<normalize::CharType> char_types;
  char_types.reserve(codepoints.size());
  for (char32_t code : codepoints) {
    char_types.push_back(normalize::classifyChar(code));
  }

  // Build lattice
  core::Lattice lattice =
      tokenizer_->buildLattice(normalized, codepoints, char_types);

  // Check if lattice is valid
  if (!lattice.isValid()) {
    core::Morpheme morpheme;
    morpheme.surface = std::string(text);
    morpheme.pos = core::PartOfSpeech::Noun;
    morpheme.start_pos = 0;
    morpheme.end_pos = codepoints.size();
    return {morpheme};
  }

  // Run Viterbi
  core::ViterbiResult vresult = viterbi_.solve(lattice, scorer_);

  // Convert to morphemes
  auto morphemes = pathToMorphemes(vresult, lattice, text);

  // Move lattice to output if requested (after analysis is done)
  if (out_lattice != nullptr) {
    *out_lattice = std::move(lattice);
  }

  return morphemes;
}

}  // namespace suzume::analysis
