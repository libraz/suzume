import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import { allocString, getModule, getTagCount, parseTags, type WasmModule } from './helpers';

describe('C API: generate_tags', () => {
  let module: WasmModule;
  let handle: number;
  let generateTags: (h: number, t: number) => number;
  let generateTagsWithOptions: (h: number, t: number, o: number) => number;
  let tagsFree: (t: number) => void;

  beforeAll(async () => {
    module = await getModule();
    const create = module.cwrap('suzume_create', 'number', []) as () => number;
    handle = create();
    generateTags = module.cwrap('suzume_generate_tags', 'number', [
      'number',
      'number',
    ]) as typeof generateTags;
    generateTagsWithOptions = module.cwrap('suzume_generate_tags_with_options', 'number', [
      'number',
      'number',
      'number',
    ]) as typeof generateTagsWithOptions;
    tagsFree = module.cwrap('suzume_tags_free', null, ['number']) as typeof tagsFree;
  });

  afterAll(() => {
    if (handle && module) {
      const destroy = module.cwrap('suzume_destroy', null, ['number']) as (h: number) => void;
      destroy(handle);
    }
  });

  it('should generate tags from text', () => {
    const textPtr = allocString(module, '東京タワー');
    const tagsPtr = generateTags(handle, textPtr);
    module._free(textPtr);

    expect(tagsPtr).toBeGreaterThan(0);
    const count = getTagCount(module, tagsPtr);
    expect(count).toBeGreaterThanOrEqual(0);

    tagsFree(tagsPtr);
  });

  it('should return tags with POS information', () => {
    const textPtr = allocString(module, '東京タワーは美しい観光地です');
    const tagsPtr = generateTags(handle, textPtr);
    module._free(textPtr);

    expect(tagsPtr).toBeGreaterThan(0);
    const tags = parseTags(module, tagsPtr);
    expect(tags.length).toBeGreaterThan(0);

    // Each tag should have a non-empty tag string and POS
    for (const t of tags) {
      expect(t.tag.length).toBeGreaterThan(0);
      expect(t.pos.length).toBeGreaterThan(0);
    }

    // Should include content word POS types
    const posValues = tags.map((t) => t.pos);
    expect(posValues.some((p) => ['NOUN', 'VERB', 'ADJ', 'ADV'].includes(p))).toBe(true);

    tagsFree(tagsPtr);
  });

  it('should generate tags for empty text', () => {
    const textPtr = allocString(module, '');
    const tagsPtr = generateTags(handle, textPtr);
    module._free(textPtr);

    expect(tagsPtr).toBeGreaterThan(0);
    const count = getTagCount(module, tagsPtr);
    expect(count).toBe(0);

    tagsFree(tagsPtr);
  });

  describe('with options', () => {
    function allocOptions(
      module: WasmModule,
      opts: {
        posFilter?: number;
        excludeBasic?: boolean;
        useLemma?: boolean;
        minLength?: number;
        maxTags?: number;
      },
    ): number {
      const ptr = module._malloc(20);
      module.HEAPU32[ptr >> 2] = opts.posFilter ?? 0;
      module.HEAPU32[(ptr + 4) >> 2] = opts.excludeBasic ? 1 : 0;
      module.HEAPU32[(ptr + 8) >> 2] = opts.useLemma !== false ? 1 : 0;
      module.HEAPU32[(ptr + 12) >> 2] = opts.minLength ?? 2;
      module.HEAPU32[(ptr + 16) >> 2] = opts.maxTags ?? 0;
      return ptr;
    }

    it('should filter by POS (noun + adjective only)', () => {
      const textPtr = allocString(module, '東京タワーは美しい観光地です');
      const optionsPtr = allocOptions(module, { posFilter: 1 | 4 }); // noun + adjective

      const tagsPtr = generateTagsWithOptions(handle, textPtr, optionsPtr);
      module._free(textPtr);
      module._free(optionsPtr);

      const tags = parseTags(module, tagsPtr);
      expect(tags.length).toBeGreaterThan(0);

      // All tags should be NOUN or ADJ
      for (const t of tags) {
        expect(['NOUN', 'ADJ']).toContain(t.pos);
      }

      tagsFree(tagsPtr);
    });

    it('should filter by POS (verb only)', () => {
      const textPtr = allocString(module, '東京に行って食べた');
      const optionsPtr = allocOptions(module, { posFilter: 2 }); // verb only

      const tagsPtr = generateTagsWithOptions(handle, textPtr, optionsPtr);
      module._free(textPtr);
      module._free(optionsPtr);

      const tags = parseTags(module, tagsPtr);
      for (const t of tags) {
        expect(t.pos).toBe('VERB');
      }

      tagsFree(tagsPtr);
    });

    it('should respect max_tags limit', () => {
      const textPtr = allocString(module, '東京タワーは美しい観光地です');
      const optionsPtr = allocOptions(module, { maxTags: 2 });

      const tagsPtr = generateTagsWithOptions(handle, textPtr, optionsPtr);
      module._free(textPtr);
      module._free(optionsPtr);

      const count = getTagCount(module, tagsPtr);
      expect(count).toBeLessThanOrEqual(2);

      tagsFree(tagsPtr);
    });

    it('should respect min_length filter', () => {
      const textPtr = allocString(module, '東京タワーは美しい観光地です');
      const optionsPtr = allocOptions(module, { minLength: 3 });

      const tagsPtr = generateTagsWithOptions(handle, textPtr, optionsPtr);
      module._free(textPtr);
      module._free(optionsPtr);

      const tags = parseTags(module, tagsPtr);
      for (const t of tags) {
        // Count characters (not bytes)
        const charCount = [...t.tag].length;
        expect(charCount).toBeGreaterThanOrEqual(3);
      }

      tagsFree(tagsPtr);
    });

    it('should exclude basic words when excludeBasic is set', () => {
      const textPtr = allocString(module, 'ある日東京に行った');
      const optionsPtr = allocOptions(module, { excludeBasic: true });

      const tagsPtr = generateTagsWithOptions(handle, textPtr, optionsPtr);
      module._free(textPtr);
      module._free(optionsPtr);

      const tags = parseTags(module, tagsPtr);
      // "ある" (hiragana-only verb) should be excluded
      const tagTexts = tags.map((t) => t.tag);
      expect(tagTexts).not.toContain('ある');

      tagsFree(tagsPtr);
    });
  });
});
