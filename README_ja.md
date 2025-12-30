# Suzume

**ブラウザで動く日本語トークナイザー**

50MBの辞書ファイルはもう不要。250KB以下の軽量日本語トークン化 — ブラウザで完結、サーバー不要。

> **Suzumeは特徴量ベースのトークナイザーです。**
> 軽量さと実用的な精度、両方のいいとこ取り。

📖 **[ドキュメント](https://suzume.libraz.net/ja/)** · 🎮 **[デモ](https://suzume.libraz.net/ja/#demo)**

## なぜ Suzume？

| 機能 | 従来の形態素解析器 | Suzume |
|------|-------------------|--------|
| **バンドルサイズ** | 20〜50MB超（辞書） | 250KB以下（gzip） |
| **ブラウザ対応** | 限定的または非対応 | フル対応 |
| **サーバー必須** | 通常は必要 | 不要 |
| **未知語対応** | 破綻する可能性 | 設計上堅牢 |
| **品詞タグ** | ✓ | ✓ |
| **原形復元** | ✓ | ✓ |

> 大規模辞書やサーバーサイド処理が現実的でない、フロントエンド・エッジ環境向けに設計されています。

### 主な特長

- 🚫 **辞書地獄からの解放** — 50MB超の辞書ファイル管理は不要
- 🖥️ **真のクライアントサイド** — 100%ブラウザで完結、APIコール不要、CORS問題なし
- 🔮 **未知語に堅牢** — ブランド名、スラング、専門用語も安定してトークン化
- ⚡ **本番投入可能** — C++からWASMにコンパイル、TypeScript対応、どこでも動作

## Suzumeの適用領域

Suzumeが最適なケース：
- **フロントエンドアプリ** — クライアントサイドで日本語処理が必要な場合
- **エッジ/サーバーレス環境** — サイズ制約がある場合
- **ユーザー生成コンテンツ** — 未知語が頻出する場合

辞書カバレッジが重要な深い言語学研究やコーパス分析には、従来のサーバーサイド解析器がより適切な場合があります。

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
