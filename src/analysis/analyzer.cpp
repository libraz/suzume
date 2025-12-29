#include "analysis/analyzer.h"

#include <algorithm>

#include "normalize/char_type.h"
#include "normalize/utf8.h"

namespace suzume::analysis {

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

  // Run pretokenizer first
  auto pretoken_result = pretokenizer_.process(text);

  // If no pretokens found, just analyze normally
  if (pretoken_result.tokens.empty()) {
    return analyzeSpan(text, 0);
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
    size_t char_offset = current_char;

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
      morpheme.end_pos = end_char;
      morpheme.start = char_offset;
      morpheme.end = end_char;
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

  // Normalize text
  auto norm_result = normalizer_.normalize(text);
  if (!core::isSuccess(norm_result)) {
    return {};
  }
  std::string normalized = std::get<std::string>(norm_result);
  if (normalized.empty()) {
    return {};
  }

  // Decode to codepoints
  std::vector<char32_t> codepoints = normalize::utf8::decode(normalized);
  if (codepoints.empty()) {
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
    // Fallback: return entire text as single unknown morpheme
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
    return {};
  }
  std::string normalized = std::get<std::string>(norm_result);
  if (normalized.empty()) {
    return {};
  }

  // Decode to codepoints
  std::vector<char32_t> codepoints = normalize::utf8::decode(normalized);
  if (codepoints.empty()) {
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
