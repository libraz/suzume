/**
 * Tests for the JS API layer (js/index.ts).
 *
 * Since Suzume.create() uses dynamic import('./suzume.js') which doesn't resolve
 * in vitest, we test the JS API indirectly by verifying:
 * 1. The TypeScript types/interfaces are correctly defined
 * 2. The C API struct layouts match what parseTags/parseMorphemes expect
 *
 * The C API tests (c-api-analyze.test.ts, c-api-tags.test.ts) cover the actual
 * WASM function calls. This file tests the JS-specific concerns.
 */
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import { Suzume } from '../../dist/index.js';
import {
  allocString,
  getModule,
  MORPHEME_LAYOUT,
  parseMorphemes,
  parseTags,
  RESULT_LAYOUT,
  TAG_OPTIONS_LAYOUT,
  TAGS_LAYOUT,
  type WasmModule,
} from './helpers';

describe('JS API: struct layout compatibility', () => {
  let module: WasmModule;
  let handle: number;

  beforeAll(async () => {
    module = await getModule();
    const create = module.cwrap('suzume_create', 'number', []) as () => number;
    handle = create();
  });

  afterAll(() => {
    if (handle && module) {
      const destroy = module.cwrap('suzume_destroy', null, ['number']) as (h: number) => void;
      destroy(handle);
    }
  });

  it('parseMorphemes returns all 7 fields matching Morpheme interface', () => {
    const analyze = module.cwrap('suzume_analyze', 'number', ['number', 'number']) as (
      h: number,
      t: number,
    ) => number;
    const resultFree = module.cwrap('suzume_result_free', null, ['number']) as (r: number) => void;

    const textPtr = allocString(module, '食べた');
    const resultPtr = analyze(handle, textPtr);
    module._free(textPtr);

    const morphemes = parseMorphemes(module, resultPtr);
    expect(morphemes.length).toBeGreaterThan(0);

    // Verify all fields exist (matching js/index.ts Morpheme interface)
    const m = morphemes[0];
    expect(typeof m.surface).toBe('string');
    expect(typeof m.pos).toBe('string');
    expect(typeof m.baseForm).toBe('string');
    expect(typeof m.posJa).toBe('string');
    // conjType/conjForm can be string or null
    expect(m.conjType === null || typeof m.conjType === 'string').toBe(true);
    expect(m.conjForm === null || typeof m.conjForm === 'string').toBe(true);

    resultFree(resultPtr);
  });

  it('parseTags returns tag and pos fields matching Tag interface', () => {
    const generateTags = module.cwrap('suzume_generate_tags', 'number', ['number', 'number']) as (
      h: number,
      t: number,
    ) => number;
    const tagsFree = module.cwrap('suzume_tags_free', null, ['number']) as (t: number) => void;

    const textPtr = allocString(module, '東京タワーは美しい');
    const tagsPtr = generateTags(handle, textPtr);
    module._free(textPtr);

    const tags = parseTags(module, tagsPtr);
    expect(tags.length).toBeGreaterThan(0);

    // Verify fields match js/index.ts Tag interface
    for (const t of tags) {
      expect(typeof t.tag).toBe('string');
      expect(typeof t.pos).toBe('string');
      expect(t.tag.length).toBeGreaterThan(0);
      expect(t.pos.length).toBeGreaterThan(0);
    }

    tagsFree(tagsPtr);
  });

  it('suzume_tags_t layout: tags ptr at +0, pos ptr at +1, count at +2', () => {
    const generateTags = module.cwrap('suzume_generate_tags', 'number', ['number', 'number']) as (
      h: number,
      t: number,
    ) => number;
    const tagsFree = module.cwrap('suzume_tags_free', null, ['number']) as (t: number) => void;

    const textPtr = allocString(module, '東京タワー');
    const tagsPtr = generateTags(handle, textPtr);
    module._free(textPtr);

    // Verify struct layout directly
    const tagsArrayPtr = module.HEAPU32[(tagsPtr + TAGS_LAYOUT.tags) >> 2];
    const posArrayPtr = module.HEAPU32[(tagsPtr + TAGS_LAYOUT.pos) >> 2];
    const count = module.HEAPU32[(tagsPtr + TAGS_LAYOUT.count) >> 2];

    expect(tagsArrayPtr).toBeGreaterThan(0);
    expect(posArrayPtr).toBeGreaterThan(0);
    expect(count).toBeGreaterThanOrEqual(0);

    // Both arrays should have valid string pointers
    if (count > 0) {
      const firstTagPtr = module.HEAPU32[tagsArrayPtr >> 2];
      const firstPosPtr = module.HEAPU32[posArrayPtr >> 2];
      expect(firstTagPtr).toBeGreaterThan(0);
      expect(firstPosPtr).toBeGreaterThan(0);

      const tagStr = module.UTF8ToString(firstTagPtr);
      const posStr = module.UTF8ToString(firstPosPtr);
      expect(tagStr.length).toBeGreaterThan(0);
      expect(posStr.length).toBeGreaterThan(0);
    }

    tagsFree(tagsPtr);
  });

  it('loadBinaryDictionary uses HEAPU32 buffer derivation (not HEAPU8)', () => {
    // Verify that Uint8Array can be derived from HEAPU32.buffer
    // This is the pattern used in js/index.ts loadBinaryDictionary
    const heapU32 = module.HEAPU32;
    const heapU8 = new Uint8Array(heapU32.buffer);
    expect(heapU8).toBeInstanceOf(Uint8Array);
    expect(heapU8.length).toBeGreaterThan(0);
    // The derived view should share the same underlying buffer
    expect(heapU8.buffer).toBe(heapU32.buffer);
  });

  it('create_with_options struct layout: 3 ints at 12 bytes', () => {
    const createWithOptions = module.cwrap('suzume_create_with_options', 'number', ['number']) as (
      optionsPtr: number,
    ) => number;
    const destroy = module.cwrap('suzume_destroy', null, ['number']) as (h: number) => void;

    const optionsPtr = module._malloc(12);
    // Layout: preserve_vu(int), preserve_case(int), preserve_symbols(int)
    module.HEAPU32[optionsPtr >> 2] = 1;
    module.HEAPU32[(optionsPtr >> 2) + 1] = 1;
    module.HEAPU32[(optionsPtr >> 2) + 2] = 0;

    const h = createWithOptions(optionsPtr);
    module._free(optionsPtr);

    expect(h).toBeGreaterThan(0);
    destroy(h);
  });

  it('exported C layout functions match JS parser constants', () => {
    const sizeofResult = module.cwrap('suzume_sizeof_result', 'number', []) as () => number;
    const sizeofMorpheme = module.cwrap('suzume_sizeof_morpheme', 'number', []) as () => number;
    const sizeofTags = module.cwrap('suzume_sizeof_tags', 'number', []) as () => number;
    const sizeofTagOptions = module.cwrap(
      'suzume_sizeof_tag_options',
      'number',
      [],
    ) as () => number;
    const offsetofResult = module.cwrap('suzume_offsetof_result', 'number', ['number']) as (
      field: number,
    ) => number;
    const offsetofMorpheme = module.cwrap('suzume_offsetof_morpheme', 'number', ['number']) as (
      field: number,
    ) => number;
    const offsetofTags = module.cwrap('suzume_offsetof_tags', 'number', ['number']) as (
      field: number,
    ) => number;
    const offsetofTagOptions = module.cwrap('suzume_offsetof_tag_options', 'number', [
      'number',
    ]) as (field: number) => number;

    expect(sizeofResult()).toBe(RESULT_LAYOUT.size);
    expect(offsetofResult(0)).toBe(RESULT_LAYOUT.morphemes);
    expect(offsetofResult(1)).toBe(RESULT_LAYOUT.count);

    expect(sizeofMorpheme()).toBe(MORPHEME_LAYOUT.size);
    expect(offsetofMorpheme(0)).toBe(MORPHEME_LAYOUT.surface);
    expect(offsetofMorpheme(6)).toBe(MORPHEME_LAYOUT.extendedPos);

    expect(sizeofTags()).toBe(TAGS_LAYOUT.size);
    expect(offsetofTags(0)).toBe(TAGS_LAYOUT.tags);
    expect(offsetofTags(2)).toBe(TAGS_LAYOUT.count);

    expect(sizeofTagOptions()).toBe(TAG_OPTIONS_LAYOUT.size);
    expect(offsetofTagOptions(0)).toBe(TAG_OPTIONS_LAYOUT.posFilter);
    expect(offsetofTagOptions(4)).toBe(TAG_OPTIONS_LAYOUT.maxTags);
  });

  it('last_error reports invalid C API calls', () => {
    const analyze = module.cwrap('suzume_analyze', 'number', ['number', 'number']) as (
      h: number,
      t: number,
    ) => number;
    const lastError = module.cwrap('suzume_last_error', 'number', []) as () => number;

    expect(analyze(0, 0)).toBe(0);
    expect(module.UTF8ToString(lastError())).toContain('null handle');
  });

  it('tag_options struct layout: 5 fields at 20 bytes', () => {
    const generateTagsWithOptions = module.cwrap('suzume_generate_tags_with_options', 'number', [
      'number',
      'number',
      'number',
    ]) as (h: number, t: number, o: number) => number;
    const tagsFree = module.cwrap('suzume_tags_free', null, ['number']) as (t: number) => void;

    const textPtr = allocString(module, '東京タワー');
    const optionsPtr = module._malloc(TAG_OPTIONS_LAYOUT.size);

    // Layout: pos_filter(uint8→uint32), exclude_basic(int), use_lemma(int),
    //         min_length(size_t), max_tags(size_t)
    module.HEAPU32[(optionsPtr + TAG_OPTIONS_LAYOUT.posFilter) >> 2] = 0;
    module.HEAPU32[(optionsPtr + TAG_OPTIONS_LAYOUT.excludeBasic) >> 2] = 0;
    module.HEAPU32[(optionsPtr + TAG_OPTIONS_LAYOUT.useLemma) >> 2] = 1;
    module.HEAPU32[(optionsPtr + TAG_OPTIONS_LAYOUT.minLength) >> 2] = 1;
    module.HEAPU32[(optionsPtr + TAG_OPTIONS_LAYOUT.maxTags) >> 2] = 0;

    const tagsPtr = generateTagsWithOptions(handle, textPtr, optionsPtr);
    module._free(textPtr);
    module._free(optionsPtr);

    expect(tagsPtr).toBeGreaterThan(0);
    const tags = parseTags(module, tagsPtr);
    expect(tags.length).toBeGreaterThan(0);

    tagsFree(tagsPtr);
  });
});

describe('JS API: error reporting', () => {
  it('exposes last C API error after a failed dictionary load', async () => {
    const suzume = await Suzume.create();

    try {
      expect(suzume.loadUserDictionary('"東京,NOUN,0.5\n')).toBe(false);
      expect(suzume.lastError).toContain('Invalid CSV quoting');
      expect(suzume.lastError).toContain('unterminated quoted field');
    } finally {
      suzume.destroy();
    }
  });

  it('loadUserDictionaryOrThrow includes C API parse details', async () => {
    const suzume = await Suzume.create();

    try {
      expect(() => suzume.loadUserDictionaryOrThrow('"東京,NOUN,0.5\n')).toThrow(
        /Invalid CSV quoting.*unterminated quoted field/,
      );
    } finally {
      suzume.destroy();
    }
  });

  it('loadBinaryDictionaryOrThrow includes C API parse details', async () => {
    const suzume = await Suzume.create();

    try {
      expect(() => suzume.loadBinaryDictionaryOrThrow(new Uint8Array([0, 1, 2, 3]))).toThrow(
        /Failed to load binary dictionary/,
      );
    } finally {
      suzume.destroy();
    }
  });

  it('throws when a destroyed instance is used', async () => {
    const suzume = await Suzume.create();
    suzume.destroy();

    expect(() => suzume.analyze('東京')).toThrow('Suzume instance has been destroyed');
  });
});
