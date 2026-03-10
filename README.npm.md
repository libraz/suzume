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

## Documentation

- [Getting Started](https://suzume.libraz.net/docs/getting-started)
- [API Reference](https://suzume.libraz.net/docs/api)
- [User Dictionary](https://suzume.libraz.net/docs/user-dictionary)
- [How It Works](https://suzume.libraz.net/docs/how-it-works)

## License

[Apache License 2.0](https://github.com/libraz/suzume/blob/main/LICENSE)
