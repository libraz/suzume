import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import createModule from '../../dist/suzume.js';

interface WasmModule {
  cwrap: (
    name: string,
    returnType: string | null,
    argTypes: string[],
  ) => (...args: unknown[]) => unknown;
  UTF8ToString: (ptr: number) => string;
  stringToUTF8: (str: string, ptr: number, maxBytes: number) => void;
  lengthBytesUTF8: (str: string) => number;
  _malloc: (size: number) => number;
  _free: (ptr: number) => void;
  HEAPU32: Uint32Array;
}

describe('Suzume WASM', () => {
  let module: WasmModule;
  let handle: number;

  beforeAll(async () => {
    module = (await createModule()) as WasmModule;
    const create = module.cwrap('suzume_create', 'number', []) as () => number;
    handle = create();
  });

  afterAll(() => {
    if (handle && module) {
      const destroy = module.cwrap('suzume_destroy', null, ['number']) as (h: number) => void;
      destroy(handle);
    }
  });

  it('should create a valid handle', () => {
    expect(handle).toBeGreaterThan(0);
  });

  it('should return version string', () => {
    const version = module.cwrap('suzume_version', 'number', []) as () => number;
    const versionPtr = version();
    const versionStr = module.UTF8ToString(versionPtr);
    expect(versionStr).toMatch(/^\d+\.\d+\.\d+$/);
  });

  it('should analyze Japanese text', () => {
    const analyze = module.cwrap('suzume_analyze', 'number', ['number', 'number']) as (
      h: number,
      t: number,
    ) => number;
    const resultFree = module.cwrap('suzume_result_free', null, ['number']) as (r: number) => void;

    const text = '日本語';
    const textBytes = module.lengthBytesUTF8(text) + 1;
    const textPtr = module._malloc(textBytes);
    module.stringToUTF8(text, textPtr, textBytes);

    const resultPtr = analyze(handle, textPtr);
    module._free(textPtr);

    expect(resultPtr).toBeGreaterThan(0);

    const count = module.HEAPU32[(resultPtr >> 2) + 1];
    expect(count).toBeGreaterThan(0);

    resultFree(resultPtr);
  });

  it('should generate tags from text', () => {
    const generateTags = module.cwrap('suzume_generate_tags', 'number', ['number', 'number']) as (
      h: number,
      t: number,
    ) => number;
    const tagsFree = module.cwrap('suzume_tags_free', null, ['number']) as (t: number) => void;

    const text = '東京タワー';
    const textBytes = module.lengthBytesUTF8(text) + 1;
    const textPtr = module._malloc(textBytes);
    module.stringToUTF8(text, textPtr, textBytes);

    const tagsPtr = generateTags(handle, textPtr);
    module._free(textPtr);

    expect(tagsPtr).toBeGreaterThan(0);

    const count = module.HEAPU32[(tagsPtr >> 2) + 1];
    expect(count).toBeGreaterThanOrEqual(0);

    tagsFree(tagsPtr);
  });

  it('should handle empty text', () => {
    const analyze = module.cwrap('suzume_analyze', 'number', ['number', 'number']) as (
      h: number,
      t: number,
    ) => number;
    const resultFree = module.cwrap('suzume_result_free', null, ['number']) as (r: number) => void;

    const text = '';
    const textBytes = module.lengthBytesUTF8(text) + 1;
    const textPtr = module._malloc(textBytes);
    module.stringToUTF8(text, textPtr, textBytes);

    const resultPtr = analyze(handle, textPtr);
    module._free(textPtr);

    expect(resultPtr).toBeGreaterThan(0);

    const count = module.HEAPU32[(resultPtr >> 2) + 1];
    expect(count).toBe(0);

    resultFree(resultPtr);
  });

  it('should handle mixed Japanese and English', () => {
    const analyze = module.cwrap('suzume_analyze', 'number', ['number', 'number']) as (
      h: number,
      t: number,
    ) => number;
    const resultFree = module.cwrap('suzume_result_free', null, ['number']) as (r: number) => void;

    const text = 'Hello世界';
    const textBytes = module.lengthBytesUTF8(text) + 1;
    const textPtr = module._malloc(textBytes);
    module.stringToUTF8(text, textPtr, textBytes);

    const resultPtr = analyze(handle, textPtr);
    module._free(textPtr);

    expect(resultPtr).toBeGreaterThan(0);

    const count = module.HEAPU32[(resultPtr >> 2) + 1];
    expect(count).toBeGreaterThan(0);

    resultFree(resultPtr);
  });
});
