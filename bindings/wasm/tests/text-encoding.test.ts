/**
 * Every text argument rejects an unpaired surrogate with the same error code.
 */
import { describe, expect, it } from 'vitest';
import { ErrorCode, Suzume, SuzumeError } from '../dist/index.js';

async function expectInvalidUtf8(action: () => Promise<unknown>): Promise<void> {
  const error = await action().then(
    () => undefined,
    (thrown: unknown) => thrown,
  );
  expect(error).toBeInstanceOf(SuzumeError);
  expect((error as SuzumeError).code).toBe(ErrorCode.InvalidUtf8);
}

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
