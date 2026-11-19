/**
 * Shared error conformance: every case in bindings/conformance/errors.json
 * except the pos_filter group (options.test.ts) must raise the decided error
 * type, code, operation and byte-identical message through the public API.
 */
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import { ErrorCode, Suzume, SuzumeError } from '../dist/index.js';

interface NpmCall {
  member: string;
  kind: 'call' | 'get' | 'set' | 'construct';
}

interface ConformanceCase {
  id: string;
  group: string;
  call: { npm: NpmCall | null };
  option: { npm: string } | null;
  value: unknown;
  released: boolean;
  only: 'npm' | 'python' | null;
  expect: { npm: { type: string; code: number; operation?: string } | null };
  message: string | null;
}

const fixturePath = fileURLToPath(new URL('../../conformance/errors.json', import.meta.url));
const userDictionaryUrl = new URL('../../../data/user.dic', import.meta.url);
const { cases } = JSON.parse(readFileSync(fixturePath, 'utf8')) as { cases: ConformanceCase[] };

const npmCases = cases.filter(
  (testCase) =>
    testCase.group !== 'pos_filter' && testCase.only !== 'python' && testCase.call.npm !== null,
);

function buildValue(value: unknown): unknown {
  if (value === null || typeof value !== 'object' || !('$special' in value)) {
    return value;
  }
  const special = (value as { $special: string }).$special;
  switch (special) {
    case 'NaN':
      return Number.NaN;
    case 'Infinity':
      return Number.POSITIVE_INFINITY;
    case '-Infinity':
      return Number.NEGATIVE_INFINITY;
    case 'undefined':
      return undefined;
    case 'negzero':
      return -0;
    case 'lone_surrogate':
      return '\ud800';
    case 'array':
      return [1];
    case 'object':
      return { key: 1 };
    case 'cycle': {
      const cyclic: Record<string, unknown> = {};
      cyclic.self = cyclic;
      return cyclic;
    }
    default:
      throw new Error(`unknown $special in fixture: ${special}`);
  }
}

async function captureError(action: () => unknown): Promise<unknown> {
  try {
    await action();
  } catch (error) {
    return error;
  }
  return undefined;
}

describe('error conformance (bindings/conformance/errors.json)', () => {
  let live: Suzume;

  beforeAll(async () => {
    live = await Suzume.create();
  });

  afterAll(() => {
    live.destroy();
  });

  it('has npm cases for every non-pos_filter group', () => {
    const groups = new Set(npmCases.map((testCase) => testCase.group));
    for (const group of [
      'error-model',
      'released',
      'unknown-code',
      'size',
      'mode',
      'surrogate',
      'scorer',
      'boolean',
      'argument',
      'render',
    ]) {
      expect(groups.has(group), group).toBe(true);
    }
  });

  it.each(npmCases.map((testCase) => [testCase.id, testCase] as const))(
    '%s',
    async (_id, testCase) => {
      const call = testCase.call.npm as NpmCall;
      const expected = testCase.expect.npm;
      if (expected === null) {
        throw new Error(`fixture case ${testCase.id} has no npm expectation`);
      }
      const value = buildValue(testCase.value);
      const option = testCase.option?.npm;

      if (call.kind === 'construct') {
        const error = new SuzumeError(testCase.message ?? '', value as number);
        expect(error).toBeInstanceOf(Error);
        expect(error.code).toBe(expected.code);
        expect(error.message).toBe(testCase.message);
        expect(error.operation).toBeUndefined();
        return;
      }

      let target = live;
      if (testCase.released) {
        target = await Suzume.create();
        target.destroy();
      }
      const subject = target as unknown as Record<string, unknown>;

      const error = await captureError(() => {
        if (call.member === 'create') {
          return Suzume.create(option === undefined ? undefined : { [option]: value });
        }
        if (call.kind === 'get') {
          return subject[call.member];
        }
        if (call.kind === 'set') {
          subject[call.member] = value;
          return undefined;
        }
        const method = subject[call.member] as (...args: unknown[]) => unknown;
        if (call.member === 'generateTags') {
          return method.call(
            target,
            '東京',
            option === undefined ? undefined : { [option]: value },
          );
        }
        return method.call(target, value);
      });

      expect(error).toBeInstanceOf(SuzumeError);
      const suzumeError = error as SuzumeError;
      expect(suzumeError.name).toBe('SuzumeError');
      expect(suzumeError.code).toBe(expected.code);
      expect(suzumeError.operation).toBe(expected.operation);
      if (testCase.message !== null) {
        expect(suzumeError.message).toBe(testCase.message);
      }
    },
  );
});

describe('SuzumeError shape', () => {
  it('exposes ErrorCode as a frozen value table with Unknown = 255', () => {
    expect(ErrorCode.InvalidInput).toBe(6);
    expect(ErrorCode.Unknown).toBe(255);
    expect(Object.isFrozen(ErrorCode)).toBe(true);
  });

  it('carries operation and cause from its options', () => {
    const cause = new Error('inner');
    const error = new SuzumeError('outer', ErrorCode.Internal, { operation: 'create', cause });
    expect(error.operation).toBe('create');
    expect(error.cause).toBe(cause);
    expect(new SuzumeError('plain').code).toBe(ErrorCode.Internal);
    expect(new SuzumeError('plain').cause).toBeUndefined();
  });

  it('keeps the serialization error as cause for unserializable scorer options', async () => {
    const cyclic: Record<string, unknown> = {};
    cyclic.self = cyclic;
    const error = await captureError(() => Suzume.create({ scorerOptions: cyclic }));
    expect(error).toBeInstanceOf(SuzumeError);
    expect((error as SuzumeError).cause).toBeInstanceOf(TypeError);
  });

  it('wraps a module load failure as an Internal create error with cause', async () => {
    const error = await captureError(() =>
      Suzume.create({ wasmPath: '/nonexistent/suzume.wasm', freshWasmModule: true }),
    );
    expect(error).toBeInstanceOf(SuzumeError);
    const suzumeError = error as SuzumeError;
    expect(suzumeError.code).toBe(ErrorCode.Internal);
    expect(suzumeError.operation).toBe('create');
    expect(suzumeError.cause).toBeDefined();
  });
});

describe('dictionary loaders', () => {
  let suzume: Suzume;

  beforeAll(async () => {
    suzume = await Suzume.create();
  });

  afterAll(() => {
    suzume.destroy();
  });

  it('returns the installed entry count', () => {
    expect(suzume.loadUserDictionary('検査語\tNOUN\n')).toBe(1);
    expect(suzume.loadUserDictionary('検査する\tVERB\tSURU\n')).toBeGreaterThan(1);
  });

  it('returns the installed entry count from a binary dictionary', () => {
    const dictionary = new Uint8Array(readFileSync(fileURLToPath(userDictionaryUrl)));
    const count = suzume.loadBinaryDictionary(dictionary);
    expect(typeof count).toBe('number');
    expect(count).toBeGreaterThanOrEqual(1);
  });

  it('throws a native binary load failure with its own text', () => {
    const error = (() => {
      try {
        suzume.loadBinaryDictionary(new Uint8Array([0, 1, 2, 3]));
      } catch (thrown) {
        return thrown;
      }
      return undefined;
    })();
    expect(error).toBeInstanceOf(SuzumeError);
    expect((error as SuzumeError).code).toBe(ErrorCode.DictionaryLoadFailed);
    expect((error as SuzumeError).operation).toBe('load_binary_dictionary');
    expect((error as SuzumeError).message).not.toMatch(/^Suzume /);
  });
});
