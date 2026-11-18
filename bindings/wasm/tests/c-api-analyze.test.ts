import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import {
  allocString,
  EXTENDED_OPTIONS_LAYOUT,
  getModule,
  parseMorphemes,
  type WasmModule,
} from './helpers';

describe('C API: analyze', () => {
  let module: WasmModule;
  let handle: number;
  let analyze: (h: number, t: number) => number;
  let resultFree: (r: number) => void;

  beforeAll(async () => {
    module = await getModule();
    const create = module.cwrap('suzume_create', 'number', []) as () => number;
    handle = create();
    analyze = module.cwrap('suzume_analyze', 'number', ['number', 'number']) as typeof analyze;
    resultFree = module.cwrap('suzume_result_free', null, ['number']) as typeof resultFree;
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
    const textPtr = allocString(module, '日本語');
    const resultPtr = analyze(handle, textPtr);
    module._free(textPtr);

    expect(resultPtr).toBeGreaterThan(0);
    const morphemes = parseMorphemes(module, resultPtr);
    expect(morphemes.length).toBeGreaterThan(0);
    resultFree(resultPtr);
  });

  it('should handle empty text', () => {
    const textPtr = allocString(module, '');
    const resultPtr = analyze(handle, textPtr);
    module._free(textPtr);

    expect(resultPtr).toBeGreaterThan(0);
    const morphemes = parseMorphemes(module, resultPtr);
    expect(morphemes.length).toBe(0);
    resultFree(resultPtr);
  });

  it('should handle mixed Japanese and English', () => {
    const textPtr = allocString(module, 'Hello世界');
    const resultPtr = analyze(handle, textPtr);
    module._free(textPtr);

    expect(resultPtr).toBeGreaterThan(0);
    const morphemes = parseMorphemes(module, resultPtr);
    expect(morphemes.length).toBeGreaterThan(0);
    resultFree(resultPtr);
  });

  it('should return chasen-style fields for verbs', () => {
    const textPtr = allocString(module, '食べた');
    const resultPtr = analyze(handle, textPtr);
    module._free(textPtr);

    const morphemes = parseMorphemes(module, resultPtr);
    expect(morphemes.length).toBeGreaterThan(0);

    // First morpheme: 食べ (verb)
    const verb = morphemes[0];
    expect(verb.pos).toBe('VERB');
    expect(verb.posJa).toBe('動詞');
    expect(verb.conjType).toBe('一段');
    expect(verb.conjForm).not.toBeNull();
    expect(verb.conjForm?.length).toBeGreaterThan(0);

    resultFree(resultPtr);
  });

  it('should expose conjugation fields for auxiliaries', () => {
    const textPtr = allocString(module, '書かなかった');
    const resultPtr = analyze(handle, textPtr);
    module._free(textPtr);

    const morphemes = parseMorphemes(module, resultPtr);
    const negative = morphemes.find((morpheme) => morpheme.surface === 'なかっ');
    expect(negative).toBeDefined();
    expect(negative?.pos).toBe('AUX');
    expect(negative?.conjType).toBeNull();
    expect(negative?.conjForm).toBe('連用形');

    resultFree(resultPtr);
  });

  it('should return null conj fields for non-conjugating words', () => {
    const textPtr = allocString(module, '東京');
    const resultPtr = analyze(handle, textPtr);
    module._free(textPtr);

    const morphemes = parseMorphemes(module, resultPtr);
    expect(morphemes.length).toBeGreaterThan(0);

    const noun = morphemes[0];
    expect(noun.pos).toBe('NOUN');
    expect(noun.posJa).toBe('名詞');
    expect(noun.conjType).toBeNull();
    expect(noun.conjForm).toBeNull();

    resultFree(resultPtr);
  });

  it('should return base_form field', () => {
    const textPtr = allocString(module, '食べた');
    const resultPtr = analyze(handle, textPtr);
    module._free(textPtr);

    const morphemes = parseMorphemes(module, resultPtr);
    const verb = morphemes[0];
    // base_form should be the dictionary form
    expect(verb.baseForm.length).toBeGreaterThan(0);

    resultFree(resultPtr);
  });

  it('should return extendedPos for all morphemes', () => {
    const textPtr = allocString(module, '猫が走る');
    const resultPtr = analyze(handle, textPtr);
    module._free(textPtr);

    const morphemes = parseMorphemes(module, resultPtr);
    expect(morphemes.length).toBeGreaterThan(0);

    for (const m of morphemes) {
      expect(typeof m.extendedPos).toBe('string');
      expect(m.extendedPos.length).toBeGreaterThan(0);
    }

    resultFree(resultPtr);
  });

  it('should return offsets and diagnostic fields for all morphemes', () => {
    const textPtr = allocString(module, '猫が走る');
    const resultPtr = analyze(handle, textPtr);
    module._free(textPtr);

    const morphemes = parseMorphemes(module, resultPtr);
    expect(morphemes.length).toBeGreaterThan(0);

    for (const m of morphemes) {
      expect(typeof m.start).toBe('number');
      expect(typeof m.end).toBe('number');
      expect(m.end).toBeGreaterThanOrEqual(m.start);
      expect(typeof m.isUserDict).toBe('boolean');
      expect(typeof m.isFormalNoun).toBe('boolean');
      expect(typeof m.isLowInfo).toBe('boolean');
      expect(typeof m.isUnknown).toBe('boolean');
      expect(typeof m.isFromDictionary).toBe('boolean');
      expect(typeof m.score).toBe('number');
    }

    resultFree(resultPtr);
  });

  it('should return correct extendedPos for verb conjugation forms', () => {
    // 食べた → 食べ(VerbRenyokei) + た(AuxPast)
    const textPtr = allocString(module, '食べた');
    const resultPtr = analyze(handle, textPtr);
    module._free(textPtr);

    const morphemes = parseMorphemes(module, resultPtr);
    const verb = morphemes[0];
    expect(verb.surface).toBe('食べ');
    expect(verb.extendedPos).toBe('VERB_連用');

    resultFree(resultPtr);
  });

  it('should analyze full sentence with mixed POS', () => {
    const textPtr = allocString(module, '私は東京に行った');
    const resultPtr = analyze(handle, textPtr);
    module._free(textPtr);

    const morphemes = parseMorphemes(module, resultPtr);
    // Should have multiple morphemes: 私/は/東京/に/行っ/た
    expect(morphemes.length).toBeGreaterThanOrEqual(5);

    const nouns = morphemes.filter((m) => m.pos === 'NOUN');
    const particles = morphemes.filter((m) => m.pos === 'PARTICLE');
    const verbs = morphemes.filter((m) => m.pos === 'VERB');

    expect(nouns.length).toBeGreaterThanOrEqual(1);
    expect(particles.length).toBeGreaterThanOrEqual(1);
    expect(verbs.length).toBeGreaterThanOrEqual(1);

    for (const noun of nouns) {
      expect(noun.posJa).toBe('名詞');
      expect(noun.conjType).toBeNull();
    }
    for (const particle of particles) {
      expect(particle.posJa).toBe('助詞');
      expect(particle.conjType).toBeNull();
    }
    for (const verb of verbs) {
      expect(verb.posJa).toBe('動詞');
      expect(verb.conjType === null || typeof verb.conjType === 'string').toBe(true);
    }

    resultFree(resultPtr);
  });
});

describe('C API: create_with_extended_options', () => {
  let module: WasmModule;

  beforeAll(async () => {
    module = await getModule();
  });

  it('keeps emoji as text-bearing OTHER when preserveSymbols is enabled', () => {
    const initOptions = module.cwrap('suzume_init_extended_options', null, ['number']) as (
      optionsPtr: number,
    ) => void;
    const createWithOptions = module.cwrap('suzume_create_with_extended_options', 'number', [
      'number',
    ]) as (optionsPtr: number) => number;
    const analyze = module.cwrap('suzume_analyze', 'number', ['number', 'number']) as (
      h: number,
      t: number,
    ) => number;
    const resultFree = module.cwrap('suzume_result_free', null, ['number']) as (r: number) => void;
    const destroy = module.cwrap('suzume_destroy', null, ['number']) as (h: number) => void;

    const optionsPtr = module._malloc(EXTENDED_OPTIONS_LAYOUT.size);
    initOptions(optionsPtr);
    new Uint8Array(module.HEAPU32.buffer)[optionsPtr + EXTENDED_OPTIONS_LAYOUT.preserveSymbols] = 1;
    const h = createWithOptions(optionsPtr);
    module._free(optionsPtr);
    expect(h).toBeGreaterThan(0);

    const textPtr = allocString(module, 'こんにちは😊');
    const resultPtr = analyze(h, textPtr);
    module._free(textPtr);

    const morphemes = parseMorphemes(module, resultPtr);
    expect(morphemes.length).toBe(2);
    expect(morphemes[1].surface).toBe('😊');
    expect(morphemes[1].pos).toBe('OTHER');

    resultFree(resultPtr);
    destroy(h);
  });

  it('keeps emoji by default for full offset coverage', () => {
    const create = module.cwrap('suzume_create', 'number', []) as () => number;
    const analyze = module.cwrap('suzume_analyze', 'number', ['number', 'number']) as (
      h: number,
      t: number,
    ) => number;
    const resultFree = module.cwrap('suzume_result_free', null, ['number']) as (r: number) => void;
    const destroy = module.cwrap('suzume_destroy', null, ['number']) as (h: number) => void;

    const h = create();
    const textPtr = allocString(module, 'こんにちは😊');
    const resultPtr = analyze(h, textPtr);
    module._free(textPtr);

    const morphemes = parseMorphemes(module, resultPtr);
    expect(morphemes.length).toBe(2);
    expect(morphemes[1].surface).toBe('😊');
    expect(morphemes[1].pos).toBe('OTHER');

    resultFree(resultPtr);
    destroy(h);
  });
});
