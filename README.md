# Suzume

A lightweight Japanese morphological analyzer that runs in browsers and Node.js.

## Why Suzume?

**Works without a large dictionary.** Most analyzers break down when they encounter unknown words. Suzume treats unknown words as normal—it generates candidates from character patterns and lets Viterbi pick the best path.

**Runs anywhere.** No ICU, no Boost, no external dependencies. The same code runs natively or as WebAssembly in your browser.

**Grows with your dictionary.** Start with minimal entries, add words as needed. The analyzer improves naturally without retraining or rebuilding connection tables.

| Traditional Analyzers | Suzume |
|----------------------|--------|
| Breaks on unknown words | Handles them gracefully |
| Requires large dictionaries | Works with minimal dictionary |
| Native runtime only | Browser + Node.js + Native |
| Complex setup | `npm install suzume` |

## Installation

### npm (Browser / Node.js)

```bash
npm install suzume
```

### Build from Source (C++)

```bash
make          # Build
make test     # Run tests
```

Requirements: C++17 compiler, CMake 3.15+

## Quick Start

### JavaScript / TypeScript

```typescript
import { Suzume } from 'suzume';

const suzume = await Suzume.create();

// Morphological analysis
const result = suzume.analyze('すもももももももものうち');
for (const m of result) {
  console.log(`${m.surface}\t${m.pos}\t${m.baseForm}`);
}

// Tag extraction
const tags = suzume.generateTags('東京スカイツリーに行きました');
console.log(tags); // ['東京', 'スカイツリー', ...]

// Add custom words (CSV: surface,pos,cost,lemma)
suzume.loadUserDictionary('スカイツリー,NOUN');

suzume.destroy();
```

### C++

```cpp
#include "suzume.h"

suzume::Suzume analyzer;
auto result = analyzer.analyze("私は東京に住んでいます");

for (const auto& m : result) {
    std::cout << m.surface << "\t" << m.lemma << std::endl;
}
```

### CLI

```bash
suzume-cli "私は東京に住んでいます"
suzume-cli analyze -f json "テキスト"    # JSON output
suzume-cli analyze -f chasen "テキスト"  # ChaSen format
suzume-cli -i                            # Interactive mode
```

## Features

- **800+ verb conjugation patterns** — Godan, Ichidan, Suru, Kuru, and compound forms
- **Pre-tokenizer** — Preserves URLs, emails, dates, and numbers as single tokens
- **User dictionary** — Add domain-specific terms at runtime
- **Multiple output formats** — JSON, TSV, ChaSen-compatible

## Use Cases

- Search index generation
- Tag extraction for classification
- Web apps with client-side Japanese processing
- Domains with lots of neologisms and proper nouns

## Design Philosophy

Traditional analyzers (MeCab, ChaSen, etc.) treat dictionary entries as ground truth and unknown words as failures. This works for well-resourced domains like news articles, but breaks down for:

- User-generated content with neologisms
- Domain-specific terminology
- Lightweight environments (WASM, embedded)

Suzume takes a different approach:

**Dictionary as feature, not answer.** Dictionary matches add confidence but aren't required. The analyzer generates candidates from character patterns (kanji runs, katakana sequences, alphanumeric compounds) and evaluates them alongside dictionary entries.

**Non-incremental tokenization.** Instead of making greedy left-to-right decisions, Suzume builds a lattice of all possible segmentations and uses Viterbi to find the globally optimal path.

**No connection table dependency.** Traditional analyzers rely on massive POS transition matrices. Suzume uses lightweight surface features and POS bigrams, making it portable and robust to dictionary changes.

## Dictionary

```
data/
├── core/           # Source TSV files
├── user/           # User TSV files
├── core.dic        # Compiled dictionary (auto-loaded)
└── user.dic        # User dictionary (auto-loaded)
```

**TSV format** (tab-separated): `surface	pos	reading	cost	conj_type`
**CSV format** (comma-separated): `surface,pos,cost,lemma`

Minimum required fields: `surface,pos`

Auto-load search order: `$SUZUME_DATA_DIR` → `./data/` → `~/.suzume/` → `/usr/local/share/suzume/`

```bash
# Compile TSV to binary
suzume-cli dict compile user.tsv user.dic
```

## Project Structure

```
suzume/
├── src/                   # C++ source
│   ├── suzume.h/cpp       # Public API
│   ├── suzume_c.h/cpp     # C API for WASM
│   └── ...
├── js/                    # TypeScript wrapper
├── dist/                  # WASM build output
├── data/                  # Dictionary files
└── tests/                 # Unit tests
```

## License

[Apache License 2.0](LICENSE)

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

## Authors

- libraz <libraz@libraz.net>
