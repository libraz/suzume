/**
 * `posFilter` takes the output POS labels, exactly as `Tag.pos` prints them.
 * The rejection cases come from the pos_filter group of
 * bindings/conformance/errors.json, shared with the Python suite.
 */
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import {
  ErrorCode,
  Suzume,
  SuzumeError,
  type TagOptions,
  type TagPosLabel,
} from '../dist/index.js';

interface PosFilterCase {
  id: string;
  group: string;
  value: unknown;
  only: 'npm' | 'python' | null;
  message: string;
}

const fixturePath = fileURLToPath(new URL('../../conformance/errors.json', import.meta.url));
const { cases } = JSON.parse(readFileSync(fixturePath, 'utf8')) as { cases: PosFilterCase[] };
const posFilterCases = cases.filter(
  (testCase) => testCase.group === 'pos_filter' && testCase.only !== 'python',
);
const NAMES: readonly TagPosLabel[] = ['NOUN', 'VERB', 'ADJ', 'ADV', 'PARTICLE', 'AUX'];
const TEXT = '彼らは美味しいラーメンを食べた';
const KEEP_ALL: TagOptions = { minLength: 1, excludeLowInfo: false, excludeFormalNouns: false };

function buildValue(value: unknown): unknown {
  if (Array.isArray(value)) {
    return value.map(buildValue);
  }
  if (value !== null && typeof value === 'object' && '$special' in value) {
    const special = (value as { $special: string }).$special;
    if (special === 'undefined') {
      return undefined;
    }
    if (special === 'object') {
      return { key: 1 };
    }
    throw new Error(`unknown $special in fixture: ${special}`);
  }
  return value;
}

function caught(action: () => unknown): unknown {
  try {
    action();
  } catch (error) {
    return error;
  }
  return undefined;
}

describe('posFilter', () => {
  let sz: Suzume;

  beforeAll(async () => {
    sz = await Suzume.create();
  });

  afterAll(() => {
    sz.destroy();
  });

  it('has the shared pos_filter cases', () => {
    expect(posFilterCases.length).toBeGreaterThanOrEqual(10);
  });

  it.each(posFilterCases.map((testCase) => [testCase.id, testCase] as const))(
    '%s',
    (_id, testCase) => {
      const error = caught(() =>
        sz.generateTags('東京', { posFilter: buildValue(testCase.value) as TagPosLabel }),
      );
      expect(error).toBeInstanceOf(SuzumeError);
      expect((error as SuzumeError).code).toBe(ErrorCode.InvalidInput);
      expect((error as SuzumeError).operation).toBe('generate_tags');
      expect((error as SuzumeError).message).toBe(testCase.message);
    },
  );

  it.each(NAMES)('accepts the output label %s', (name) => {
    const tags = sz.generateTags(TEXT, { ...KEEP_ALL, posFilter: name, excludeParticles: false });
    for (const tag of tags) {
      expect(tag.pos === name || (name === 'NOUN' && tag.pos === 'PRON')).toBe(true);
    }
  });

  it('filters a label printed by Tag.pos back to itself', () => {
    const verbs = sz.generateTags(TEXT).filter((tag) => tag.pos === 'VERB');
    expect(verbs.length).toBeGreaterThan(0);
    expect(sz.generateTags(TEXT, { posFilter: verbs[0].pos as TagPosLabel })).toEqual(verbs);
  });

  it('admits pronouns under NOUN', () => {
    const tags = sz.generateTags(TEXT, { ...KEEP_ALL, posFilter: 'NOUN' });
    expect(new Set(tags.map((tag) => tag.pos))).toEqual(new Set(['NOUN', 'PRON']));
  });

  it('selects the union of an array of names', () => {
    const tags = sz.generateTags(TEXT, { posFilter: ['NOUN', 'VERB'] });
    expect(new Set(tags.map((tag) => tag.pos))).toEqual(new Set(['NOUN', 'VERB']));
  });

  it('treats a single name like a one-element array', () => {
    expect(sz.generateTags(TEXT, { posFilter: 'NOUN' })).toEqual(
      sz.generateTags(TEXT, { posFilter: ['NOUN'] }),
    );
  });

  it('gives duplicate names set semantics', () => {
    expect(sz.generateTags(TEXT, { posFilter: ['NOUN', 'NOUN'] })).toEqual(
      sz.generateTags(TEXT, { posFilter: 'NOUN' }),
    );
  });

  it.each([
    ['null', null],
    ['undefined', undefined],
    ['an empty array', []],
  ])('keeps every part of speech for %s', (_label, value) => {
    expect(sz.generateTags(TEXT, { posFilter: value as TagPosLabel | undefined })).toEqual(
      sz.generateTags(TEXT),
    );
  });

  it('admits particles when PARTICLE is named, despite the default exclusion', () => {
    const tags = sz.generateTags('りんごについて', { posFilter: 'PARTICLE', minLength: 1 });
    expect(tags).toEqual([{ tag: 'について', pos: 'PARTICLE' }]);
  });

  it('checks the released state before validating posFilter', async () => {
    const released = await Suzume.create();
    released.destroy();
    const error = caught(() => released.generateTags('東京', { posFilter: 'bogus' as never }));
    expect(error).toBeInstanceOf(SuzumeError);
    expect((error as SuzumeError).message).toBe('Suzume instance has been released');
  });
});
