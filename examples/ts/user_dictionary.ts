/**
 * User dictionary - load custom vocabulary for domain-specific text
 *
 * Demonstrates: loading user dictionary from string data,
 * before/after comparison, and binary dictionary loading.
 *
 * Run: npx tsx examples/ts/user_dictionary.ts
 */
import { Suzume } from '../../bindings/wasm/dist/index.js';

const suzume = await Suzume.create();

const text = '青空りんご園の案内図を作りました';

// Without user dictionary
console.log('Without user dictionary:');
for (const m of suzume.analyze(text)) {
  console.log(`  ${m.surface} [${m.pos}]`);
}

// TSV: surface<TAB>POS[<TAB>conj_type][<TAB>base_form]
const dictData = '青空りんご園\tNOUN\n';
const loaded = suzume.loadUserDictionary(dictData);
console.log(`\nDictionary loaded: ${loaded}`);

console.log('\nWith user dictionary:');
const withDictionary = suzume.analyze(text);
if (!withDictionary.some((m) => m.isUserDict)) {
  throw new Error('loaded user dictionary entry was not used');
}
for (const m of withDictionary) {
  const baseForm = m.baseForm !== m.surface ? ` (base form: ${m.baseForm})` : '';
  console.log(`  ${m.surface} [${m.pos}]${baseForm}`);
}

// Binary dictionary: load pre-compiled .dic file
// const response = await fetch('/path/to/user.dic');
// const dicData = new Uint8Array(await response.arrayBuffer());
// suzume.loadBinaryDictionary(dicData);
