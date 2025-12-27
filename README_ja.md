# Suzume

ブラウザと Node.js で動く、軽量な日本語形態素解析ライブラリです。

> **Note:** Suzume は MeCab のような汎用形態素解析器ではありません。
> 未知語処理・検索インデックス生成・クライアントサイド利用に特化しています。

## Suzume の特長

**辞書が少なくても動きます。** 従来の解析器は未知語に弱く、辞書にない単語で精度が落ちます。Suzume は未知語を文字パターンから候補生成し、辞書語と同列に評価します。辞書が不完全でも壊れません。

**どこでも動きます。** ICU や Boost への依存がありません。同じコードがネイティブでも、ブラウザの WebAssembly でも動作します。

**辞書を育てられます。** 最小限の辞書から始めて、必要に応じて単語を追加できます。接続表の再構築や再学習は不要です。

| 従来の解析器 | Suzume |
|-------------|--------|
| 未知語で精度低下 | 未知語も適切に処理 |
| 大規模辞書が必須 | 最小辞書でも動作 |
| ネイティブ実行のみ | ブラウザ + Node.js + ネイティブ |
| セットアップが複雑 | `npm install suzume` |

## インストール

### npm（ブラウザ / Node.js）

```bash
npm install suzume
```

### ソースからビルド（C++）

```bash
make          # ビルド
make test     # テスト実行
```

必要環境: C++17 コンパイラ、CMake 3.15+

## クイックスタート

### JavaScript / TypeScript

```typescript
import { Suzume } from 'suzume';

const suzume = await Suzume.create();

// 形態素解析
const result = suzume.analyze('すもももももももものうち');
for (const m of result) {
  console.log(`${m.surface}\t${m.pos}\t${m.baseForm}`);
}

// タグ抽出
const tags = suzume.generateTags('東京スカイツリーに行きました');
console.log(tags); // ['東京', 'スカイツリー', ...]

// ユーザー辞書の追加 (CSV: surface,pos,cost,lemma)
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
suzume-cli analyze -f json "テキスト"    # JSON 出力
suzume-cli analyze -f chasen "テキスト"  # ChaSen 形式
suzume-cli -i                            # 対話モード
```

## 機能

- **800 以上の動詞活用パターン** — 五段・一段・サ変・カ変、複合動詞に対応
- **プリトークナイザー** — URL、メール、日付、数値を単一トークンとして保持
- **ユーザー辞書** — ドメイン固有の用語を実行時に追加可能
- **複数の出力形式** — JSON、TSV、ChaSen 互換形式

## 想定ユースケース

- 検索インデックスの生成
- タグ抽出・分類
- クライアントサイドで日本語処理する Web アプリ
- 新語・固有名詞が多いドメイン

## 設計思想

従来の解析器（MeCab、ChaSen など）は、辞書エントリを正解として扱い、未知語を失敗とみなします。新聞記事のような整備されたドメインでは有効ですが、以下のケースでは破綻します：

- 新語が頻出するユーザー生成コンテンツ
- ドメイン固有の専門用語
- 軽量環境（WASM、組み込み）

Suzume は異なるアプローチを取ります。

**辞書は正解ではなく特徴量。** 辞書一致は信頼度を上げますが、必須ではありません。文字パターン（漢字連続、カタカナ列、英数字複合語）から候補を生成し、辞書語と同列に評価します。

**非逐次的なトークン化。** 左から右への貪欲な決定ではなく、すべての分割可能性をラティスとして構築し、Viterbi で大域最適解を求めます。

**接続表に依存しない。** 従来の解析器は巨大な品詞遷移行列を必要とします。Suzume は軽量な表層特徴と品詞 bigram を使用するため、可搬性が高く、辞書変更にも頑健です。

## 辞書

```
data/
├── core/           # ソース TSV
├── user/           # ユーザー TSV
├── core.dic        # コンパイル済み辞書（自動読込）
└── user.dic        # ユーザー辞書（自動読込）
```

**TSV 形式**（タブ区切り）: `surface	pos	reading	cost	conj_type`
**CSV 形式**（カンマ区切り）: `surface,pos,cost,lemma`

最小必須フィールド: `surface,pos`

自動読込の検索順序: `$SUZUME_DATA_DIR` → `./data/` → `~/.suzume/` → `/usr/local/share/suzume/`

```bash
# TSV をバイナリにコンパイル
suzume-cli dict compile user.tsv user.dic
```

## プロジェクト構成

```
suzume/
├── src/                   # C++ ソース
│   ├── suzume.h/cpp       # 公開 API
│   ├── suzume_c.h/cpp     # WASM 用 C API
│   └── ...
├── js/                    # TypeScript ラッパー
├── dist/                  # WASM ビルド出力
├── data/                  # 辞書ファイル
└── tests/                 # ユニットテスト
```

## ライセンス

[Apache License 2.0](LICENSE)

## コントリビューション

Issue や Pull Request を歓迎します。

## 作者

- libraz <libraz@libraz.net>
