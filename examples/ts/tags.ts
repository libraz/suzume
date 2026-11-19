/**
 * Tag generation example - extract content word tags for indexing/search
 *
 * Run:
 *   npx tsx examples/ts/tags.ts
 *
 * Requires: npm package built (yarn build)
 */
import { Suzume } from '../../bindings/wasm/dist/index.js';

const suzume = await Suzume.create();

const text = '東京都渋谷区で開催されたイベントに参加しました';

// Generate all content word tags
const tags = suzume.generateTags(text);
if (tags.length === 0) {
  throw new Error('tag generation returned no tags');
}
console.log('All tags:');
for (const t of tags) {
  console.log(`  ${t.tag} (${t.pos})`);
}

// Nouns only, exclude basic words
const nounTags = suzume.generateTags(text, {
  posFilter: 'NOUN',
  excludeBasic: true,
});
if (nounTags.length === 0) {
  throw new Error('noun tag generation returned no tags');
}
console.log('\nNoun tags (excluding basic):');
for (const t of nounTags) {
  console.log(`  ${t.tag}`);
}

// Verbs and adjectives by base form
const verbAdjTags = suzume.generateTags(text, {
  posFilter: ['VERB', 'ADJ'],
  useBaseForm: true,
});
console.log('\nVerb/Adjective tags (base form):');
for (const t of verbAdjTags) {
  console.log(`  ${t.tag} (${t.pos})`);
}
