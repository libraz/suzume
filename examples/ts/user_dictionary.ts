/**
 * User dictionary - load custom vocabulary for domain-specific text
 *
 * Demonstrates: loading user dictionary from string data,
 * before/after comparison, and binary dictionary loading.
 *
 * Run: npx tsx examples/ts/user_dictionary.ts
 */
import { Suzume } from '../../dist/index.js';

const suzume = await Suzume.create();

const text = '初音ミクのコスプレ衣装を作りました';

// Without user dictionary
console.log('Without user dictionary:');
for (const m of suzume.analyze(text)) {
  console.log(`  ${m.surface} [${m.pos}]`);
}

// Load user dictionary (TSV format: surface\tPOS\treading)
const dictData = '初音ミク\tNOUN\tはつねみく\n';
const loaded = suzume.loadUserDictionary(dictData);
console.log(`\nDictionary loaded: ${loaded}`);

console.log('\nWith user dictionary:');
for (const m of suzume.analyze(text)) {
  const lemma = m.baseForm !== m.surface ? ` (lemma: ${m.baseForm})` : '';
  console.log(`  ${m.surface} [${m.pos}]${lemma}`);
}

// Binary dictionary: load pre-compiled .dic file
// const response = await fetch('/path/to/user.dic');
// const dicData = new Uint8Array(await response.arrayBuffer());
// suzume.loadBinaryDictionary(dicData);
