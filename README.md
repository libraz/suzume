# Suzume

**Japanese Tokenizer That Actually Works in the Browser**

No more 50MB dictionary files. Lightweight Japanese tokenization under 300KB — runs entirely in the browser, no server required.

> **Suzume is a feature-driven tokenizer** designed for real-world Japanese text on the web.
> The best of both worlds: lightweight footprint meets practical accuracy.

📖 **[Documentation](https://suzume.libraz.net)** · 🎮 **[Live Demo](https://suzume.libraz.net/#demo)**

## Why Suzume?

| Feature | Traditional Analyzers | Suzume |
|---------|----------------------|--------|
| **Bundle Size** | 20–50MB+ (dictionary) | < 300KB gzipped |
| **Browser Support** | Limited or none | Full support |
| **Server Required** | Usually yes | No |
| **Unknown Words** | May struggle | Robust by design |
| **POS Tagging** | ✓ | ✓ |
| **Lemmatization** | ✓ | ✓ |

> Designed for frontend and edge environments where large dictionaries and server-side processing are not viable.

### Key Features

- 🚫 **No Dictionary Hell** — Forget about managing 50MB+ dictionary files
- 🖥️ **True Client-Side** — Runs 100% in the browser, no API calls, no CORS headaches
- 🔮 **Robust to Unknown Words** — Brand names, slang, technical terms — stable tokenization every time
- ⚡ **Production Ready** — C++ compiled to WASM, TypeScript support, works everywhere

## When to Use Suzume

Suzume is ideal for:
- **Frontend applications** that need client-side Japanese processing
- **Edge/serverless environments** with size constraints
- **User-generated content** where unknown words are common

For deep linguistic research or corpus analysis where dictionary coverage is critical, traditional server-side analyzers may be more appropriate.

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
