/**
 * Basic morphological analysis example
 *
 * Run:
 *   npx tsx examples/ts/basic.ts
 *
 * Requires: npm package built (yarn build)
 */
import { Suzume } from '../../dist/index.js';

const suzume = await Suzume.create();

// Analyze Japanese text
const morphemes = suzume.analyze('すもももももももものうち');

for (const m of morphemes) {
  console.log(`${m.surface}\t${m.pos}\t${m.baseForm}`);
}

// Detailed output
console.log('\nDetailed:');
for (const m of morphemes) {
  console.log({
    surface: m.surface,
    pos: m.pos,
    posJa: m.posJa,
    baseForm: m.baseForm,
    reading: m.reading,
    conjType: m.conjType,
    conjForm: m.conjForm,
  });
}
