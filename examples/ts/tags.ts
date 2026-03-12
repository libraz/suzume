/**
 * Tag generation example - extract content word tags for indexing/search
 *
 * Run:
 *   npx tsx examples/ts/tags.ts
 *
 * Requires: npm package built (yarn build)
 */
import { Suzume } from '../../dist/index.js';

const suzume = await Suzume.create();

const text = '東京都渋谷区で開催されたイベントに参加しました';

// Generate all content word tags
const tags = suzume.generateTags(text);
console.log('All tags:');
for (const t of tags) {
  console.log(`  ${t.tag} (${t.pos})`);
}

// Nouns only, exclude basic words
const nounTags = suzume.generateTags(text, {
  pos: ['noun'],
  excludeBasic: true,
});
console.log('\nNoun tags (excluding basic):');
for (const t of nounTags) {
  console.log(`  ${t.tag}`);
}

// Verbs and adjectives with lemma
const verbAdjTags = suzume.generateTags(text, {
  pos: ['verb', 'adjective'],
  useLemma: true,
});
console.log('\nVerb/Adjective tags (lemma):');
for (const t of verbAdjTags) {
  console.log(`  ${t.tag} (${t.pos})`);
}
