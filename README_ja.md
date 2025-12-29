# Suzume

**ブラウザで動く日本語トークナイザー**

MeCabの50MB辞書に疲れたあなたへ。軽量な日本語トークン化をフロントエンドで — 250KB以下（gzip）、サーバー不要。

> **Suzumeは辞書ベースの形態素解析器ではありません。**
> Web上の実際の日本語テキストのために設計された、軽量な特徴量ベースのトークナイザーです。

📖 **[ドキュメント](https://suzume.libraz.net/ja/)** · 🎮 **[デモ](https://suzume.libraz.net/ja/#demo)**

## なぜ Suzume？

| | MeCab / Sudachi | Suzume |
|---|-----------------|--------|
| **設計目標** | 言語学的精度 | フロントエンドでの安定性 |
| **サイズ** | 50MB超の辞書 | 250KB以下（gzip） |
| **実行環境** | サーバーサイド | ブラウザ + Node.js + ネイティブ |
| **未知語** | 破綻する可能性 | 設計上堅牢 |

> Suzumeは大規模辞書やサーバーサイド処理が現実的でない、フロントエンド・エッジ環境向けに設計されています。

### 主な特長

- 🚫 **辞書地獄からの解放** — 50MB超の辞書ファイル管理は不要
- 🖥️ **真のクライアントサイド** — 100%ブラウザで完結、APIコール不要
- 🔮 **未知語に堅牢** — 新語やノイズの多い入力でも破綻しない設計
- ⚡ **本番投入可能** — C++からWASMにコンパイル、TypeScript対応

## Suzumeでないもの

- MeCabやSudachiのような本格的な形態素解析器ではない
- 辞書ベースのNLPパイプラインの代替ではない
- 深い言語学的・文法的解析向けではない

学術レベルの言語解析が必要なら、MeCabやSudachiを使ってください。Suzumeは**制約のある環境で安定したトークン化**が必要なときのためのものです。

## インストール

```bash
npm install @libraz/suzume
```

または yarn/pnpm/bun:
```bash
yarn add @libraz/suzume
pnpm add @libraz/suzume
bun add @libraz/suzume
```

## クイックスタート

### JavaScript / TypeScript

```typescript
import { Suzume } from '@libraz/suzume'

const suzume = await Suzume.create()

const tokens = suzume.analyze('すもももももももものうち')
for (const t of tokens) {
  console.log(`${t.surface} [${t.posJa}]`)
}

// タグ抽出
const tags = suzume.generateTags('東京スカイツリーに行きました')
console.log(tags) // ['東京', 'スカイツリー']

suzume.destroy()
```

### ブラウザ（CDN）

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

ソースからビルド（C++17、CMake 3.15+が必要）:

```bash
make          # ビルド
make test     # テスト実行
```

## ドキュメント

詳細なドキュメントは **[suzume.libraz.net](https://suzume.libraz.net/ja/)** で公開しています:

- [はじめる](https://suzume.libraz.net/ja/docs/getting-started) — インストールと基本的な使い方
- [API リファレンス](https://suzume.libraz.net/ja/docs/api) — 完全な API ドキュメント
- [ユーザー辞書](https://suzume.libraz.net/ja/docs/user-dictionary) — カスタム単語の追加
- [仕組み](https://suzume.libraz.net/ja/docs/how-it-works) — 技術的な解説

## 想定ユースケース

- **検索インデックス** — 全文検索用のトークン化
- **タグ抽出** — 分類用のキーワード生成
- **ブラウザアプリ** — サーバー不要のクライアントサイド日本語処理
- **ユーザー生成コンテンツ** — ノイズの多い入力でも安定したトークン化

## ライセンス

[Apache License 2.0](LICENSE)

## コントリビューション

Issue や Pull Request を歓迎します。

## 作者

libraz <libraz@libraz.net>
