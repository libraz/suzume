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

  it('should return chasen-style fields for verbs', () => {
    const analyze = module.cwrap('suzume_analyze', 'number', ['number', 'number']) as (
      h: number,
      t: number,
    ) => number;
    const resultFree = module.cwrap('suzume_result_free', null, ['number']) as (r: number) => void;

    const text = '食べた';
    const textBytes = module.lengthBytesUTF8(text) + 1;
    const textPtr = module._malloc(textBytes);
    module.stringToUTF8(text, textPtr, textBytes);

    const resultPtr = analyze(handle, textPtr);
    module._free(textPtr);

    expect(resultPtr).toBeGreaterThan(0);

    const morphemesPtr = module.HEAPU32[resultPtr >> 2];
    const count = module.HEAPU32[(resultPtr >> 2) + 1];
    expect(count).toBeGreaterThan(0);

    // Check first morpheme (食べ - verb stem)
    // suzume_morpheme_t layout: 7 pointers (surface, pos, base_form, reading, pos_ja, conj_type, conj_form)
    const morphPtr = morphemesPtr;
    const posPtr = module.HEAPU32[(morphPtr >> 2) + 1];
    const posJaPtr = module.HEAPU32[(morphPtr >> 2) + 4];
    const conjTypePtr = module.HEAPU32[(morphPtr >> 2) + 5];
    const conjFormPtr = module.HEAPU32[(morphPtr >> 2) + 6];

    const pos = module.UTF8ToString(posPtr);
    const posJa = module.UTF8ToString(posJaPtr);

    // Verb should have Japanese POS
    expect(pos).toBe('VERB');
    expect(posJa).toBe('動詞');

    // Verb should have conjugation info
    expect(conjTypePtr).toBeGreaterThan(0);
    expect(conjFormPtr).toBeGreaterThan(0);

    const conjType = module.UTF8ToString(conjTypePtr);
    const conjForm = module.UTF8ToString(conjFormPtr);

    expect(conjType).toBe('一段');
    expect(conjForm.length).toBeGreaterThan(0);

    resultFree(resultPtr);
  });

  it('should analyze full sentence with chasen fields', () => {
    const analyze = module.cwrap('suzume_analyze', 'number', ['number', 'number']) as (
      h: number,
      t: number,
    ) => number;
    const resultFree = module.cwrap('suzume_result_free', null, ['number']) as (r: number) => void;

    const text = '私は東京に行った';
    const textBytes = module.lengthBytesUTF8(text) + 1;
    const textPtr = module._malloc(textBytes);
    module.stringToUTF8(text, textPtr, textBytes);

    const resultPtr = analyze(handle, textPtr);
    module._free(textPtr);

    expect(resultPtr).toBeGreaterThan(0);

    const morphemesPtr = module.HEAPU32[resultPtr >> 2];
    const count = module.HEAPU32[(resultPtr >> 2) + 1];

    // Should have multiple morphemes: 私/は/東京/に/行っ/た
    expect(count).toBeGreaterThanOrEqual(5);

    // Collect all morphemes
    const morphemes: Array<{
      surface: string;
      pos: string;
      posJa: string;
      conjType: string | null;
      conjForm: string | null;
    }> = [];

    const MORPHEME_SIZE = 28;
    for (let i = 0; i < count; i++) {
      const morphPtr = morphemesPtr + i * MORPHEME_SIZE;
      const surfacePtr = module.HEAPU32[morphPtr >> 2];
      const posPtr = module.HEAPU32[(morphPtr >> 2) + 1];
      const posJaPtr = module.HEAPU32[(morphPtr >> 2) + 4];
      const conjTypePtr = module.HEAPU32[(morphPtr >> 2) + 5];
      const conjFormPtr = module.HEAPU32[(morphPtr >> 2) + 6];

      morphemes.push({
        surface: module.UTF8ToString(surfacePtr),
        pos: module.UTF8ToString(posPtr),
        posJa: module.UTF8ToString(posJaPtr),
        conjType: conjTypePtr !== 0 ? module.UTF8ToString(conjTypePtr) : null,
        conjForm: conjFormPtr !== 0 ? module.UTF8ToString(conjFormPtr) : null,
      });
    }

    // Check that we have nouns (私, 東京), particles (は, に), verb (行っ)
    const nouns = morphemes.filter((m) => m.pos === 'NOUN');
    const particles = morphemes.filter((m) => m.pos === 'PARTICLE');
    const verbs = morphemes.filter((m) => m.pos === 'VERB');

    expect(nouns.length).toBeGreaterThanOrEqual(1);
    expect(particles.length).toBeGreaterThanOrEqual(1);
    expect(verbs.length).toBeGreaterThanOrEqual(1);

    // All nouns should have posJa = '名詞' and null conj fields
    for (const noun of nouns) {
      expect(noun.posJa).toBe('名詞');
      expect(noun.conjType).toBeNull();
      expect(noun.conjForm).toBeNull();
    }

    // All particles should have posJa = '助詞' and null conj fields
    for (const particle of particles) {
      expect(particle.posJa).toBe('助詞');
      expect(particle.conjType).toBeNull();
      expect(particle.conjForm).toBeNull();
    }

    // All verbs should have conjugation info
    for (const verb of verbs) {
      expect(verb.posJa).toBe('動詞');
      expect(verb.conjType).not.toBeNull();
      expect(verb.conjForm).not.toBeNull();
    }

    resultFree(resultPtr);
  });

  it('should return null conj fields for non-conjugating words', () => {
    const analyze = module.cwrap('suzume_analyze', 'number', ['number', 'number']) as (
      h: number,
      t: number,
    ) => number;
    const resultFree = module.cwrap('suzume_result_free', null, ['number']) as (r: number) => void;

    const text = '東京';
    const textBytes = module.lengthBytesUTF8(text) + 1;
    const textPtr = module._malloc(textBytes);
    module.stringToUTF8(text, textPtr, textBytes);

    const resultPtr = analyze(handle, textPtr);
    module._free(textPtr);

    expect(resultPtr).toBeGreaterThan(0);

    const morphemesPtr = module.HEAPU32[resultPtr >> 2];
    const count = module.HEAPU32[(resultPtr >> 2) + 1];
    expect(count).toBeGreaterThan(0);

    // Check first morpheme (東京 - noun)
    const morphPtr = morphemesPtr;
    const posPtr = module.HEAPU32[(morphPtr >> 2) + 1];
    const posJaPtr = module.HEAPU32[(morphPtr >> 2) + 4];
    const conjTypePtr = module.HEAPU32[(morphPtr >> 2) + 5];
    const conjFormPtr = module.HEAPU32[(morphPtr >> 2) + 6];

    const pos = module.UTF8ToString(posPtr);
    const posJa = module.UTF8ToString(posJaPtr);

    // Noun should have Japanese POS
    expect(pos).toBe('NOUN');
    expect(posJa).toBe('名詞');

    // Noun should NOT have conjugation info (null pointers)
    expect(conjTypePtr).toBe(0);
    expect(conjFormPtr).toBe(0);

    resultFree(resultPtr);
  });

  describe('with preserveSymbols option', () => {
    let handleWithSymbols: number;

    beforeAll(() => {
      const createWithOptions = module.cwrap('suzume_create_with_options', 'number', [
        'number',
      ]) as (optionsPtr: number) => number;

      // Allocate options struct (3 ints = 12 bytes)
      const optionsPtr = module._malloc(12);
      module.HEAPU32[optionsPtr >> 2] = 1; // preserve_vu = true
      module.HEAPU32[(optionsPtr >> 2) + 1] = 1; // preserve_case = true
      module.HEAPU32[(optionsPtr >> 2) + 2] = 1; // preserve_symbols = true

      handleWithSymbols = createWithOptions(optionsPtr);
      module._free(optionsPtr);
    });

    afterAll(() => {
      if (handleWithSymbols && module) {
        const destroy = module.cwrap('suzume_destroy', null, ['number']) as (h: number) => void;
        destroy(handleWithSymbols);
      }
    });

    it('should preserve emoji when preserveSymbols is enabled', () => {
      const analyze = module.cwrap('suzume_analyze', 'number', ['number', 'number']) as (
        h: number,
        t: number,
      ) => number;
      const resultFree = module.cwrap('suzume_result_free', null, ['number']) as (
        r: number,
      ) => void;

      const text = 'こんにちは😊';
      const textBytes = module.lengthBytesUTF8(text) + 1;
      const textPtr = module._malloc(textBytes);
      module.stringToUTF8(text, textPtr, textBytes);

      const resultPtr = analyze(handleWithSymbols, textPtr);
      module._free(textPtr);

      expect(resultPtr).toBeGreaterThan(0);

      const morphemesPtr = module.HEAPU32[resultPtr >> 2];
      const count = module.HEAPU32[(resultPtr >> 2) + 1];

      // Should have 2 morphemes: こんにちは and 😊
      expect(count).toBe(2);

      // Check last morpheme is emoji symbol
      const MORPHEME_SIZE = 28;
      const lastMorphPtr = morphemesPtr + (count - 1) * MORPHEME_SIZE;
      const surfacePtr = module.HEAPU32[lastMorphPtr >> 2];
      const posPtr = module.HEAPU32[(lastMorphPtr >> 2) + 1];

      const surface = module.UTF8ToString(surfacePtr);
      const pos = module.UTF8ToString(posPtr);

      expect(surface).toBe('😊');
      expect(pos).toBe('SYMBOL');

      resultFree(resultPtr);
    });

    it('should remove emoji by default', () => {
      const analyze = module.cwrap('suzume_analyze', 'number', ['number', 'number']) as (
        h: number,
        t: number,
      ) => number;
      const resultFree = module.cwrap('suzume_result_free', null, ['number']) as (
        r: number,
      ) => void;

      const text = 'こんにちは😊';
      const textBytes = module.lengthBytesUTF8(text) + 1;
      const textPtr = module._malloc(textBytes);
      module.stringToUTF8(text, textPtr, textBytes);

      // Use default handle (without preserveSymbols)
      const resultPtr = analyze(handle, textPtr);
      module._free(textPtr);

      expect(resultPtr).toBeGreaterThan(0);

      const count = module.HEAPU32[(resultPtr >> 2) + 1];

      // Should have only 1 morpheme (emoji removed)
      expect(count).toBe(1);

      resultFree(resultPtr);
    });
  });
});
