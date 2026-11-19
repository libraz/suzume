/**
 * Every text argument rejects an unpaired surrogate as invalid input named after the argument.
 */
import { describe, it } from 'vitest';
import { ErrorCode, Suzume } from '../dist/index.js';
import { expectSuzumeError } from './helpers';

describe('unpaired surrogates', () => {
  it('are rejected in user dictionary data', async () => {
    const suzume = await Suzume.create();
    try {
      await expectSuzumeError(
        () => suzume.loadUserDictionary('\ud800\tNOUN\n'),
        ErrorCode.InvalidInput,
        'load_user_dictionary',
        'invalid dictionary: contains an unpaired surrogate',
      );
    } finally {
      suzume.destroy();
    }
  });

  it.each([
    ['a string', '\ud800'],
    ['an object value', { verb: { name: '\ud800' } }],
    ['an object key', { '\udc00': 1 }],
  ])('are rejected in scorer options given as %s', async (_label, scorerOptions) => {
    await expectSuzumeError(
      () => Suzume.create({ scorerOptions }),
      ErrorCode.InvalidInput,
      'create',
      'invalid scorer_options: contains an unpaired surrogate',
    );
  });

  it('are rejected at the end of analyzed text', async () => {
    const suzume = await Suzume.create();
    try {
      await expectSuzumeError(
        () => suzume.analyze('東京\ud800'),
        ErrorCode.InvalidInput,
        'analyze',
        'invalid text: contains an unpaired surrogate',
      );
    } finally {
      suzume.destroy();
    }
  });
});
