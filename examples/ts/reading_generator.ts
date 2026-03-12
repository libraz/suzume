/**
 * Reading (furigana) generator - generate HTML ruby annotations
 *
 * Demonstrates: morpheme-level analysis, reading/conjugation fields,
 * and generating furigana HTML for web display.
 *
 * Run: npx tsx examples/ts/reading_generator.ts
 */
import { Suzume } from '../../dist/index.js';

const suzume = await Suzume.create();

const text = '彼女は図書館で難しい本を読んでいた';
const morphemes = suzume.analyze(text);

// Table output
console.log('Surface\tReading\tPOS\tBase\tConj');
console.log('------\t-------\t---\t----\t----');
for (const m of morphemes) {
  console.log(
    `${m.surface}\t${m.reading}\t${m.pos}\t${m.baseForm}\t${m.conjForm ?? '-'}`,
  );
}

// Generate furigana HTML
function toRubyHtml(morphemes: ReturnType<typeof suzume.analyze>): string {
  return morphemes
    .map((m) => {
      if (!m.reading || m.surface === m.reading) return m.surface;
      return `<ruby>${m.surface}<rp>(</rp><rt>${m.reading}</rt><rp>)</rp></ruby>`;
    })
    .join('');
}

console.log('\nFurigana HTML:');
console.log(toRubyHtml(morphemes));
