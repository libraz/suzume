# Suzume

[![CI](https://img.shields.io/github/actions/workflow/status/libraz/suzume/ci.yml?branch=main&label=CI)](https://github.com/libraz/suzume/actions)
[![npm](https://img.shields.io/npm/v/@libraz/suzume)](https://www.npmjs.com/package/@libraz/suzume)
[![codecov](https://codecov.io/gh/libraz/suzume/branch/main/graph/badge.svg)](https://codecov.io/gh/libraz/suzume)
[![License](https://img.shields.io/github/license/libraz/suzume)](https://github.com/libraz/suzume/blob/main/LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue?logo=c%2B%2B)](https://en.cppreference.com/w/cpp/17)

A lightweight Japanese tokenizer that runs in the browser via WebAssembly. Uses feature-based analysis instead of large dictionary files.

[Documentation](https://suzume.libraz.net) | [Live Demo](https://suzume.libraz.net/#demo)

## Overview

Suzume tokenizes Japanese text using character patterns, connection rules, and a small dictionary (~400KB), rather than the large dictionaries (20-50MB+) used by traditional morphological analyzers like MeCab or Kuromoji. The WASM build is around 360KB gzipped.

| | Traditional Analyzers | Suzume |
|---|---|---|
| **Bundle Size** | 20-50MB+ (dictionary) | <400KB gzipped |
| **Browser Support** | Limited or none | Supported (WASM) |
| **Server Required** | Usually yes | No |
| **POS Tagging** | Yes | Yes |
| **Lemmatization** | Yes | Yes |

### Trade-offs

- **Smaller footprint** — No large dictionary download; suitable for frontend, edge, and serverless environments
- **Handles unknown words** — Feature-based analysis doesn't fail on words missing from a dictionary
- **Less accurate on edge cases** — Traditional dictionary-based analyzers will be more accurate for specialized vocabulary and complex linguistic analysis

## Installation

```bash
npm install @libraz/suzume
```

Or use yarn/pnpm/bun:
```bash
yarn add @libraz/suzume
pnpm add @libraz/suzume
bun add @libraz/suzume
```

## Quick Start

### JavaScript / TypeScript

```typescript
import { Suzume } from '@libraz/suzume'

const suzume = await Suzume.create()

const tokens = suzume.analyze('すもももももももものうち')
for (const t of tokens) {
  console.log(`${t.surface} [${t.posJa}]`)
}

// Tag extraction (returns { tag, pos } objects)
const tags = suzume.generateTags('東京スカイツリーに行きました')
// → [{ tag: '東京', pos: 'noun' }, { tag: 'スカイツリー', pos: 'noun' }, { tag: '行く', pos: 'verb' }]

// Nouns only
suzume.generateTags('美味しいラーメンを食べた', { pos: ['noun'] })
// → [{ tag: 'ラーメン', pos: 'noun' }]

// Exclude basic words (hiragana-only lemma like する, ある, いい)
suzume.generateTags('今日はいい天気ですね', { excludeBasic: true })
// → [{ tag: '今日', pos: 'noun' }, { tag: '天気', pos: 'noun' }]
```

### Browser (CDN)

```html
<script type="module">
  import { Suzume } from 'https://esm.sh/@libraz/suzume'

  const suzume = await Suzume.create()
  console.log(suzume.analyze('こんにちは'))
</script>
```

### C++

```cpp
#include "suzume.h"

suzume::Suzume tokenizer;
auto tokens = tokenizer.analyze("東京に行きました");

for (const auto& t : tokens) {
    std::cout << t.surface << "\t" << t.lemma << std::endl;
}
```

Build from source (requires C++17, CMake 3.15+):

```bash
make          # Build
make test     # Run tests
```

## Documentation

- [Getting Started](https://suzume.libraz.net/docs/getting-started) — Installation and basic usage
- [API Reference](https://suzume.libraz.net/docs/api) — API documentation
- [User Dictionary](https://suzume.libraz.net/docs/user-dictionary) — Adding custom words
- [How It Works](https://suzume.libraz.net/docs/how-it-works) — Technical details

## License

[Apache License 2.0](LICENSE)
