/**
 * Search indexer - build inverted index from Japanese text
 *
 * Demonstrates: tag generation with POS filtering, batch processing,
 * and building a search index in the browser/Node.js.
 *
 * Run: npx tsx examples/ts/search_indexer.ts
 */
import { Suzume } from '../../dist/index.js';

const suzume = await Suzume.create();

interface Document {
  id: number;
  text: string;
}

const docs: Document[] = [
  { id: 1, text: '東京都渋谷区で新しいカフェがオープンしました' },
  { id: 2, text: '渋谷のカフェで美味しいコーヒーを飲みました' },
  { id: 3, text: '新宿駅の近くにある図書館で本を読んでいます' },
];

// Build inverted index: tag -> Set<doc_id>
const index = new Map<string, Set<number>>();

for (const doc of docs) {
  const tags = suzume.generateTags(doc.text, {
    pos: ['noun', 'verb'],
    excludeBasic: true,
    useLemma: true,
    minLength: 2,
  });

  for (const tag of tags) {
    if (!index.has(tag.tag)) {
      index.set(tag.tag, new Set());
    }
    index.get(tag.tag)!.add(doc.id);
  }
}

// Print index
console.log('Inverted index:');
for (const [tag, docIds] of index) {
  console.log(`  ${tag} -> [${[...docIds].join(', ')}]`);
}

// Search
function search(query: string): number[] {
  const queryTags = suzume.generateTags(query, {
    pos: ['noun', 'verb'],
    useLemma: true,
  });

  const scores = new Map<number, number>();
  for (const tag of queryTags) {
    const docIds = index.get(tag.tag);
    if (docIds) {
      for (const id of docIds) {
        scores.set(id, (scores.get(id) ?? 0) + 1);
      }
    }
  }

  return [...scores.entries()]
    .sort((a, b) => b[1] - a[1])
    .map(([id]) => id);
}

console.log('\nSearch "渋谷 カフェ":', search('渋谷 カフェ'));
console.log('Search "図書館 読む":', search('図書館 読む'));
