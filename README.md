# Suzume

A lightweight Japanese morphological analyzer written in C++17.

## Overview

Suzume is a minimal-dependency Japanese text tokenizer and part-of-speech tagger designed for tag extraction and text classification. It provides boundary detection and lemmatization without requiring large external dictionaries.

## Features

- **ICU-independent**: Custom UTF-8 normalization without external Unicode libraries
- **Trie-based dictionary**: Core dictionary + user dictionary support
- **Viterbi algorithm**: Lattice-based optimal path finding
- **Verb conjugation analysis**: 800+ inflection patterns (Godan, Ichidan, Suru, Kuru)
- **Binary dictionary format**: Double-Array Trie with efficient serialization
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
