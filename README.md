# Suzume

A lightweight Japanese morphological analyzer written in C++17.

## Why Suzume?

Existing Japanese morphological analyzers (MeCab, ChaSen, etc.) assume:
- Large dictionaries are always available
- Dictionary entries are "ground truth"
- Unknown words are exceptions to minimize

**Suzume takes a different stance:**

| Traditional | Suzume |
|-------------|--------|
| Dictionary = correct answer | Dictionary = feature signal |
| Unknown word = failure | Unknown word = normal case |
| Incremental decisions | All candidates → Viterbi decides |
| Large connection tables | Lightweight feature-based decoding |
| Native runtime only | WASM-first design |

### Key Design Principles

1. **Dictionary-growth-first** — Works with minimal dictionary, improves as you add entries. No catastrophic failures from missing words.

2. **Non-incremental tokenization** — Generates both merge and split candidates, lets Viterbi find global optimum. No premature commitments.

3. **No connection table dependency** — Uses surface features + lightweight POS bigram instead of massive transition matrices. Simple, portable, robust.

4. **Unknown word tolerance** — Character-type sequences, kanji runs, katakana neologisms, alphanumeric compounds are all first-class candidates evaluated alongside dictionary entries.

5. **WASM-ready** — No ICU, no Boost, no file I/O in core. Runs in browser/Node.js with small memory footprint.

### Use Cases

- Search index generation
- Tag extraction / classification
- Dictionary bootstrapping pipelines
- Web/WASM Japanese text processing
- Domains with high neologism density

## Features

- **Zero external dependencies**: Custom UTF-8 normalization (no ICU)
- **Trie-based dictionary**: Core + user dictionary with Double-Array Trie
- **Viterbi algorithm**: Lattice-based global optimization
- **Verb conjugation**: 800+ inflection patterns (Godan, Ichidan, Suru, Kuru)
- **Binary dictionary**: Efficient serialization format
- **CLI tool**: Interactive mode, JSON/TSV output, dictionary management

## Requirements

- C++17 compiler (GCC 9+ or Clang 10+)
- CMake 3.15+
- Make

## Build

```bash
make          # Build the project
make test     # Run tests
make clean    # Clean build
make rebuild  # Clean and rebuild
make help     # Show all targets
```

## Usage

### Library API

```cpp
#include "suzume.h"

suzume::Suzume analyzer;
auto result = analyzer.analyze("私は東京に住んでいます");

for (const auto& morpheme : result) {
    std::cout << morpheme.surface << "\t"
              << core::posToString(morpheme.pos) << "\t"
              << morpheme.lemma << std::endl;
}
```

### CLI

```bash
# Morphological analysis
suzume-cli "私は東京に住んでいます"

# JSON output
suzume-cli analyze -f json "Hello world"

# Compile user dictionary
suzume-cli dict compile user.tsv

# Use compiled dictionary
suzume-cli -d user.dic "テキスト"

# Interactive mode
suzume-cli dict -i
```

### Output Example

```
私           NOUN      私
は           PARTICLE  は
東京         NOUN      東京
に           PARTICLE  に
住んでいます  VERB      住む
```

## Dictionary

### Structure

```
data/
├── core/           # Source TSV files (version controlled)
│   └── basic.tsv
├── user/           # User TSV files
├── core.dic        # Compiled core dictionary (auto-loaded)
└── user.dic        # Compiled user dictionary (auto-loaded)
```

### Auto-load Search Order

Suzume automatically loads `core.dic` and `user.dic` from the first found path:

1. `$SUZUME_DATA_DIR`
2. `./data/`
3. `~/.suzume/`
4. `/usr/local/share/suzume/`
5. `/usr/share/suzume/`

### Compiling Dictionaries

```bash
# Compile TSV to binary dictionary (output to data/core.dic)
suzume-cli dict compile data/core/basic.tsv data/core.dic

# Auto-generate output path (input.tsv -> input.dic)
suzume-cli dict compile user.tsv
```

## Project Structure

```
suzume/
├── src/
│   ├── suzume.h/cpp       # Public API
│   ├── core/              # Types, lattice, Viterbi
│   ├── normalize/         # UTF-8, character classification
│   ├── dictionary/        # Trie, Double-Array, binary format
│   ├── grammar/           # Conjugation, inflection (800+ patterns)
│   ├── analysis/          # Tokenizer, scorer, unknown word handling
│   ├── postprocess/       # Lemmatization, tag generation
│   └── suzume-cli/        # CLI tool
├── data/                  # Dictionary data
└── tests/                 # Unit tests (Google Test)
```

## License

MIT License

## Author

libraz
