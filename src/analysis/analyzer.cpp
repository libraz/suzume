#include "analysis/analyzer.h"

#include <algorithm>
#include <functional>

#include "core/debug.h"
#include "core/text_boundaries.h"
#include "normalize/char_type.h"
#include "normalize/utf8.h"

namespace suzume::analysis {

namespace {

// Maximum chunk size in bytes (~10K Japanese characters).
// This is an input-size boundary, not a memory budget: peak lattice memory
// depends on candidate density, so embeddings must be sized from their workload.
constexpr size_t kMaxChunkBytes = 32768;

// Check if byte position is a sentence boundary character.
// Returns the number of bytes to include (0 if not a boundary).
inline size_t sentenceBoundaryLen(std::string_view text, size_t pos) {
  size_t end = pos;
  const char32_t codepoint = normalize::decodeUtf8(text, end);
  return core::isSentenceBoundaryCodepoint(codepoint) ? end - pos : 0;
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

std::vector<normalize::CharType> classifyCodepoints(const std::vector<char32_t>& codepoints) {
  std::vector<normalize::CharType> char_types;
  char_types.reserve(codepoints.size());
  for (const char32_t codepoint : codepoints) {
    char_types.push_back(normalize::classifyChar(codepoint));
  }
  return char_types;
}

// Split long text into sentence-level chunks and analyze each one.
//
// Scans forward up to kMaxChunkBytes looking for the last sentence boundary; if
// none is found, falls back to a UTF-8 character boundary (and finally to
// scan_end) so progress is always made. Each chunk is handed to `process` along
// with its character offset (base_offset plus the characters consumed so far),
// and the results are concatenated in order.
//
// Non-templated by design (std::function) so the compiled body is shared by all
// call sites rather than instantiated per lambda type.
std::vector<core::Morpheme> chunkBySentenceBoundary(
    std::string_view text, size_t base_offset,
    const std::function<std::vector<core::Morpheme>(std::string_view chunk, size_t chunk_char_offset)>& process) {
  std::vector<core::Morpheme> result;
  size_t pos = 0;
  size_t char_pos = 0;

  while (pos < text.size()) {
    // Scan forward up to kMaxChunkBytes looking for last sentence boundary
    size_t scan_end = std::min(pos + kMaxChunkBytes, text.size());
    size_t best_break = 0;  // byte position of best break point (after boundary char)

    for (size_t i = pos; i < scan_end;) {
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
      chunk_end = normalize::findUtf8Boundary(text, scan_end);
      if (chunk_end <= pos) {
        chunk_end = scan_end;  // Safety: advance at least to scan_end
      }
    }

    auto morphemes = process(text.substr(pos, chunk_end - pos), base_offset + char_pos);
    for (auto& morpheme : morphemes) {
      result.push_back(std::move(morpheme));
    }

    char_pos += countChars(text, pos, chunk_end);
    pos = chunk_end;
  }

  return result;
}

}  // namespace

Analyzer::Analyzer(const AnalyzerOptions& options)
    : options_(options),
      normalizer_(options.normalize_options),
      pretokenizer_(),
      scorer_(options.scorer_options),
      unknown_gen_(options.unknown_options, &dict_manager_),
      tokenizer_(nullptr) {
  tokenizer_ = std::make_unique<Tokenizer>(dict_manager_, scorer_, unknown_gen_, options_.mode);
}

Analyzer::~Analyzer() = default;

void Analyzer::addUserDictionary(std::shared_ptr<dictionary::UserDictionary> dict) {
  dict_manager_.addUserDictionary(std::move(dict));
  // Rebuild tokenizer with new dictionary
  tokenizer_ = std::make_unique<Tokenizer>(dict_manager_, scorer_, unknown_gen_, options_.mode);
}

bool Analyzer::hasCoreBinaryDictionary() const {
  return dict_manager_.hasCoreBinaryDictionary();
}

void Analyzer::setMode(core::AnalysisMode mode) {
  if (options_.mode == mode) {
    return;
  }
  options_.mode = mode;
  tokenizer_ = std::make_unique<Tokenizer>(dict_manager_, scorer_, unknown_gen_, options_.mode);
}

core::Expected<std::vector<core::Morpheme>, core::Error> Analyzer::analyze(std::string_view text) const {
  return core::takeMorphemes(analyzeWithNormalizedText(text));
}

core::Expected<core::AnalysisOutput, core::Error> Analyzer::analyzeWithNormalizedText(std::string_view text) const {
  if (text.empty()) {
    return core::AnalysisOutput{};
  }

  // Normalize once up front so pretoken boundaries and morpheme offsets share a
  // single normalized-text coordinate system. Length-changing normalization
  // (e.g. ｶ+ﾞ → ガ) would otherwise desync raw pretoken and normalized offsets.
  auto norm_result = normalizer_.normalize(text);
  if (!core::isSuccess(norm_result)) {
    SUZUME_DEBUG_BLOCK {
      auto& error = std::get<core::Error>(norm_result);
      SUZUME_DEBUG_STREAM << "[ANALYZER] Normalization failed: " << error.message
                          << " (code=" << static_cast<int>(error.code) << ")\n";
    }
    return core::makeUnexpected(std::get<core::Error>(std::move(norm_result)));
  }
  std::string normalized = std::get<std::string>(std::move(norm_result));
  if (normalized.empty()) {
    return core::AnalysisOutput{std::move(normalized), {}};
  }
  std::string_view norm_text = normalized;

  if (norm_text.size() <= kMaxChunkBytes) {  // short text: process directly
    auto morphemes = analyzeWithPretokenizer(norm_text, 0);
    return core::AnalysisOutput{std::move(normalized), std::move(morphemes)};
  }

  // Long text: split at sentence boundaries before pretokenizer
  // This prevents pretokenizer from scanning 100MB+ in one pass.
  auto morphemes = chunkBySentenceBoundary(norm_text, 0, [this](std::string_view chunk, size_t chunk_char_offset) {
    return analyzeWithPretokenizer(chunk, chunk_char_offset);
  });
  return core::AnalysisOutput{std::move(normalized), std::move(morphemes)};
}

std::vector<core::Morpheme> Analyzer::analyzeWithPretokenizer(std::string_view text, size_t base_char_offset) const {
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

  // process() appends each collection in source order, so merge the two sorted
  // sequences directly instead of allocating and sorting a combined list.
  size_t token_idx = 0;
  size_t span_idx = 0;
  while (token_idx < pretoken_result.tokens.size() || span_idx < pretoken_result.spans.size()) {
    const bool is_pretoken = span_idx == pretoken_result.spans.size() ||
                             (token_idx < pretoken_result.tokens.size() &&
                              pretoken_result.tokens[token_idx].start < pretoken_result.spans[span_idx].start);
    const size_t item_start =
        is_pretoken ? pretoken_result.tokens[token_idx].start : pretoken_result.spans[span_idx].start;

    // Calculate character offset at this byte position
    const size_t item_byte = std::max(current_byte, std::min(item_start, text.size()));
    current_char += countChars(text, current_byte, item_byte);
    current_byte = item_byte;
    size_t char_offset = base_char_offset + current_char;

    if (is_pretoken) {
      // Convert pretoken to morpheme
      const auto& tok = pretoken_result.tokens[token_idx++];
      core::Morpheme morpheme;
      morpheme.surface = tok.surface;
      morpheme.pos = tok.pos;
      morpheme.extended_pos = core::posToExtendedPos(tok.pos);
      morpheme.lemma = tok.surface;

      morpheme.start = char_offset;
      morpheme.end = char_offset + countChars(text, current_byte, std::min(tok.end, text.size()));
      result.push_back(std::move(morpheme));
    } else {
      // Analyze span
      const auto& span = pretoken_result.spans[span_idx++];
      std::string_view span_text = text.substr(span.start, span.end - span.start);
      auto span_morphemes = analyzeSpan(span_text, char_offset);
      for (auto& morpheme : span_morphemes) {
        result.push_back(std::move(morpheme));
      }
    }
  }

  return result;
}

std::vector<core::Morpheme> Analyzer::analyzeSpan(std::string_view text, size_t char_offset) const {
  if (text.empty()) {
    return {};
  }

  // Short text: analyze directly without chunking overhead
  if (text.size() <= kMaxChunkBytes) {
    return analyzeChunk(text, char_offset);
  }

  // Long text: split at sentence boundaries to bound memory usage
  return chunkBySentenceBoundary(text, char_offset, [this](std::string_view chunk, size_t chunk_char_offset) {
    return analyzeChunk(chunk, chunk_char_offset);
  });
}

std::vector<core::Morpheme> Analyzer::analyzeChunk(std::string_view text, size_t char_offset) const {
  if (text.empty()) {
    return {};
  }

  // Chunk boundary: no inflection result from the previous chunk is referenced
  // any more, so an older retained cache generation may be discarded here.
  unknown_gen_.inflection().rollCache();

  // Text is already normalized by analyze(); decode directly to codepoints.
  std::vector<char32_t> codepoints = normalize::toCodepoints(text);
  if (codepoints.empty()) {
    SUZUME_DEBUG_LOG("[ANALYZER] UTF-8 decode failed\n");
    return {};
  }

  // Build lattice
  core::Lattice lattice = tokenizer_->buildLattice(text, codepoints, classifyCodepoints(codepoints));

  // Run Viterbi
  core::ViterbiResult vresult = viterbi_.solve(lattice, scorer_);

  // Convert to morphemes with offset adjustment
  return pathToMorphemes(vresult, lattice, char_offset);
}

std::vector<core::Morpheme> Analyzer::pathToMorphemes(const core::ViterbiResult& result, const core::Lattice& lattice,
                                                      size_t base_char_offset) {
  std::vector<core::Morpheme> morphemes;
  morphemes.reserve(result.path.size());

  for (size_t path_index = 0; path_index < result.path.size(); ++path_index) {
    const size_t edge_id = result.path[path_index];
    const core::LatticeEdge& edge = lattice.getEdge(edge_id);
    const core::LatticeEdge* next_edge =
        path_index + 1 < result.path.size() ? &lattice.getEdge(result.path[path_index + 1]) : nullptr;

    core::Morpheme morpheme;
    morpheme.surface = std::string(edge.surface);
    morpheme.pos = edge.pos;
    morpheme.extended_pos = edge.extended_pos;
    morpheme.start = base_char_offset + edge.start;
    morpheme.end = base_char_offset + edge.end;

    morpheme.lemma = edge.lemma.empty() ? morpheme.surface : std::string(edge.lemma);

    morpheme.flags = edge.flags;
    morpheme.origin = edge.origin;
    morpheme.score = edge.cost;
    morpheme.conj_type = edge.conj_type;
    const grammar::ConjForm form = grammar::conjFormFromExtendedPos(
        edge.extended_pos, next_edge == nullptr ? core::ExtendedPOS::Unknown : next_edge->extended_pos,
        next_edge == nullptr ? std::string_view{} : next_edge->lemma);
    if (form != grammar::ConjForm::Count_) {
      morpheme.conj_form = form;
    }

    morphemes.push_back(std::move(morpheme));
  }

  return morphemes;
}

std::vector<core::Morpheme> Analyzer::analyzeDebug(std::string_view text, core::Lattice* out_lattice) const {
  auto analyzed = analyzeWithNormalizedText(text);
  if (!analyzed.hasValue()) {
    SUZUME_DEBUG_LOG("[ANALYZER] analyzeDebug failed: " << analyzed.error().message << "\n");
    return {};
  }

  if (out_lattice != nullptr) {
    *out_lattice = core::Lattice(0);
    const std::string& normalized = analyzed.value().normalized_text;
    if (!normalized.empty() && normalized.size() <= kMaxChunkBytes) {
      const auto pretokenized = pretokenizer_.process(normalized);
      std::string_view debug_span = normalized;
      if (!pretokenized.tokens.empty()) {
        debug_span = {};
        for (const auto& span : pretokenized.spans) {
          const std::string_view candidate = std::string_view(normalized).substr(span.start, span.end - span.start);
          if (candidate.size() > debug_span.size()) {
            debug_span = candidate;
          }
        }
      }
      if (!debug_span.empty()) {
        const std::vector<char32_t> codepoints = normalize::toCodepoints(debug_span);
        *out_lattice = tokenizer_->buildLattice(debug_span, codepoints, classifyCodepoints(codepoints));
      }
    }
  }

  return std::move(analyzed.value().morphemes);
}

}  // namespace suzume::analysis
