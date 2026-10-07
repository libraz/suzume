import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import type { Tag as ParsedTag } from '../js/index.js';
import {
  allocString,
  allocTagOptions,
  getModule,
  getTagCount,
  parseTags,
  TAG_OPTIONS_LAYOUT,
  type TagOptionValues,
  type WasmModule,
} from './helpers';

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
    function tagsWith(text: string, opts: TagOptionValues): ParsedTag[] {
      const textPtr = allocString(module, text);
      const optionsPtr = allocTagOptions(module, opts);
      const tagsPtr = generateTagsWithOptions(handle, textPtr, optionsPtr);
      module._free(textPtr);
      module._free(optionsPtr);
      try {
        return parseTags(module, tagsPtr);
      } finally {
        tagsFree(tagsPtr);
      }
    }

    it('should filter by POS (noun + adjective only)', () => {
      const tags = tagsWith('東京タワーは美しい観光地です', { posFilter: 1 | 4 });
      expect(tags.length).toBeGreaterThan(0);

      // All tags should be NOUN or ADJ
      for (const t of tags) {
        expect(['NOUN', 'ADJ']).toContain(t.pos);
      }
    });

    it('should filter by POS (verb only)', () => {
      const tags = tagsWith('東京に行って食べた', { posFilter: 2 });
      for (const t of tags) {
        expect(t.pos).toBe('VERB');
      }
    });

    it('should respect max_tags limit', () => {
      const tags = tagsWith('東京タワーは美しい観光地です', { maxTags: 2 });
      expect(tags.length).toBeLessThanOrEqual(2);
    });

    it('should respect min_length filter', () => {
      const tags = tagsWith('東京タワーは美しい観光地です', { minLength: 3 });
      for (const t of tags) {
        // Count characters (not bytes)
        const charCount = [...t.tag].length;
        expect(charCount).toBeGreaterThanOrEqual(3);
      }
    });

    it('should exclude basic words when excludeBasic is set', () => {
      const tags = tagsWith('ある日東京に行った', { excludeBasic: true });
      // "ある" (hiragana-only verb) should be excluded
      const tagTexts = tags.map((t) => t.tag);
      expect(tagTexts).not.toContain('ある');
    });

    it('should allow particles when excludeParticles is false', () => {
      const tags = tagsWith('猫が走る', { minLength: 1, excludeParticles: false });
      const tagTexts = tags.map((t) => t.tag);
      expect(tagTexts).toContain('が');
    });

    it('should select particles and auxiliaries with explicit POS bits', () => {
      const tags = tagsWith('りんごが歩きます', {
        minLength: 1,
        posFilter: 16 | 32,
        excludeParticles: false,
        excludeAuxiliaries: false,
      });
      expect(tags).toEqual([
        { tag: 'が', pos: 'PARTICLE' },
        { tag: 'ます', pos: 'AUX' },
      ]);
    });
  });

  describe('init_tag_options', () => {
    it('writes the documented defaults over dirty heap memory', () => {
      const initTagOptions = module.cwrap('suzume_init_tag_options', null, ['number']) as (
        optionsPtr: number,
      ) => void;

      const optionsPtr = module._malloc(TAG_OPTIONS_LAYOUT.size);
      const heapU8 = new Uint8Array(module.HEAPU32.buffer);
      // Poison the whole struct so a field the initializer forgets shows up as
      // 0xFF rather than an accidental zero.
      heapU8.fill(0xff, optionsPtr, optionsPtr + TAG_OPTIONS_LAYOUT.size);

      initTagOptions(optionsPtr);

      const heapU32 = module.HEAPU32;
      expect(heapU8[optionsPtr + TAG_OPTIONS_LAYOUT.posFilter]).toBe(0);
      expect(heapU8[optionsPtr + TAG_OPTIONS_LAYOUT.excludeBasic]).toBe(0);
      expect(heapU8[optionsPtr + TAG_OPTIONS_LAYOUT.useLemma]).toBe(1);
      expect(heapU32[(optionsPtr + TAG_OPTIONS_LAYOUT.minLength) >> 2]).toBe(2);
      expect(heapU32[(optionsPtr + TAG_OPTIONS_LAYOUT.maxTags) >> 2]).toBe(0);
      expect(heapU8[optionsPtr + TAG_OPTIONS_LAYOUT.excludeParticles]).toBe(1);
      expect(heapU8[optionsPtr + TAG_OPTIONS_LAYOUT.excludeAuxiliaries]).toBe(1);
      expect(heapU8[optionsPtr + TAG_OPTIONS_LAYOUT.excludeFormalNouns]).toBe(1);
      expect(heapU8[optionsPtr + TAG_OPTIONS_LAYOUT.excludeLowInfo]).toBe(1);
      expect(heapU8[optionsPtr + TAG_OPTIONS_LAYOUT.removeDuplicates]).toBe(1);

      module._free(optionsPtr);
    });
  });
});
