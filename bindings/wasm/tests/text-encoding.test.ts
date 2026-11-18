/**
 * Every text argument rejects an unpaired surrogate with the same error code.
 */
import { describe, it } from 'vitest';
import { Suzume } from '../dist/index.js';
import { expectInvalidUtf8 } from './helpers';

describe('unpaired surrogates', () => {
  it('are rejected in user dictionary data', async () => {
    const suzume = await Suzume.create();
    try {
      await expectInvalidUtf8(async () => suzume.loadUserDictionaryCount('\ud800\tNOUN\n'));
    } finally {
      suzume.destroy();
    }
  });

  it.each([
    ['a string', '\ud800'],
    ['an object value', { verb: { name: '\ud800' } }],
    ['an object key', { '\udc00': 1 }],
  ])('are rejected in scorer options given as %s', async (_label, scorerOptions) => {
    await expectInvalidUtf8(() => Suzume.create({ scorerOptions }));
  });

  it('are rejected at the end of analyzed text', async () => {
    const suzume = await Suzume.create();
    try {
      await expectInvalidUtf8(async () => suzume.analyze('東京\ud800'));
    } finally {
      suzume.destroy();
    }
  });
});
