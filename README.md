# Suzume

**Japanese Tokenizer That Actually Works in the Browser**

Tired of MeCab's 50MB dictionary? Suzume brings lightweight Japanese tokenization to the frontend — under 250KB gzipped, no server required.

> **Suzume is not a dictionary-based morphological analyzer.**
> It is a lightweight, feature-driven tokenizer designed for real-world Japanese text on the web.

📖 **[Documentation](https://suzume.libraz.net)** · 🎮 **[Live Demo](https://suzume.libraz.net/#demo)**

## Why Suzume?

| | MeCab / Sudachi | Suzume |
|---|-----------------|--------|
| **Design Goal** | Linguistic accuracy | Frontend stability |
| **Size** | 50MB+ dictionary | < 250KB gzipped |
| **Runtime** | Server-side | Browser + Node.js + Native |
| **Unknown Words** | May break | Robust by design |

> Suzume is designed for frontend and edge environments, where large dictionaries and server-side processing are not viable.

### Key Features

- 🚫 **No Dictionary Hell** — Forget about managing 50MB+ dictionary files
- 🖥️ **True Client-Side** — Runs 100% in the browser, no API calls
- 🔮 **Robust to Unknown Words** — Designed to not break on new or noisy input
- ⚡ **Production Ready** — C++ compiled to WASM, TypeScript support

## What Suzume is NOT

- Not a full morphological analyzer like MeCab or Sudachi
- Not a replacement for dictionary-based NLP pipelines
- Not designed for deep linguistic or grammatical analysis

If you need academic-grade linguistic analysis, use MeCab or Sudachi. Suzume is for when you need **stable tokenization in constrained environments**.

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

// Tag extraction
const tags = suzume.generateTags('東京スカイツリーに行きました')
console.log(tags) // ['東京', 'スカイツリー']

suzume.destroy()
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

Full documentation is available at **[suzume.libraz.net](https://suzume.libraz.net)**:

- [Getting Started](https://suzume.libraz.net/docs/getting-started) — Installation and basic usage
- [API Reference](https://suzume.libraz.net/docs/api) — Complete API documentation
- [User Dictionary](https://suzume.libraz.net/docs/user-dictionary) — Adding custom words
- [How It Works](https://suzume.libraz.net/docs/how-it-works) — Technical deep-dive

## Use Cases

- **Search indexing** — Tokenize text for full-text search
- **Tag extraction** — Generate keywords for classification
- **Browser apps** — Client-side Japanese processing without a server
- **User-generated content** — Stable tokenization for noisy input

## License

[Apache License 2.0](LICENSE)

## Contributing

Contributions welcome! Please submit issues and pull requests on GitHub.

## Author

libraz <libraz@libraz.net>
