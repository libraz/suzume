# Suzume

A lightweight Japanese tokenizer that runs in the browser via WebAssembly. Uses feature-based analysis instead of large dictionary files (<400KB gzipped vs 20-50MB+).

## Usage

```typescript
import { Suzume } from '@libraz/suzume'

const suzume = await Suzume.create()

const tokens = suzume.analyze('すもももももももものうち')
for (const t of tokens) {
  console.log(`${t.surface} [${t.posJa}]`)
}

// Tag extraction (returns { tag, pos } objects)
const tags = suzume.generateTags('東京スカイツリーに行きました')
// → [{ tag: '東京', pos: 'NOUN' }, { tag: 'スカイツリー', pos: 'NOUN' }, { tag: '行く', pos: 'VERB' }]

// Nouns only
suzume.generateTags('美味しいラーメンを食べた', { pos: ['noun'] })
// → [{ tag: 'ラーメン', pos: 'NOUN' }]

// Exclude basic words (hiragana-only lemma like する, ある, いい)
suzume.generateTags('今日はいい天気ですね', { excludeBasic: true })
// → [{ tag: '今日', pos: 'NOUN' }, { tag: '天気', pos: 'NOUN' }]
```

### Browser (CDN)

```html
<script type="module">
  import { Suzume } from 'https://esm.sh/@libraz/suzume'

  const suzume = await Suzume.create()
  console.log(suzume.analyze('こんにちは'))
</script>
```

### Bundlers (webpack, Next.js, etc.)

If your bundler doesn't automatically resolve the `.wasm` file, you can specify its path manually:

```typescript
import wasmUrl from '@libraz/suzume/wasm?url' // Vite
const suzume = await Suzume.create({ wasmPath: wasmUrl })
```

Or specify the path directly:

```typescript
const suzume = await Suzume.create({
  wasmPath: new URL('@libraz/suzume/wasm', import.meta.url).href
})
```

## Documentation

- [Getting Started](https://suzume.libraz.net/docs/getting-started)
- [API Reference](https://suzume.libraz.net/docs/api)
- [User Dictionary](https://suzume.libraz.net/docs/user-dictionary)
- [How It Works](https://suzume.libraz.net/docs/how-it-works)

## License

[Apache License 2.0](https://github.com/libraz/suzume/blob/main/LICENSE)
