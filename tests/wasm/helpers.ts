import createModule from '../../dist/suzume.js';

export interface WasmModule {
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

export const RESULT_LAYOUT = {
  size: 8,
  morphemes: 0,
  count: 4,
} as const;

export const MORPHEME_LAYOUT = {
  size: 28,
  surface: 0,
  pos: 4,
  baseForm: 8,
  posJa: 12,
  conjType: 16,
  conjForm: 20,
  extendedPos: 24,
} as const;

export const TAGS_LAYOUT = {
  size: 12,
  tags: 0,
  pos: 4,
  count: 8,
} as const;

export const TAG_OPTIONS_LAYOUT = {
  size: 20,
  posFilter: 0,
  excludeBasic: 4,
  useLemma: 8,
  minLength: 12,
  maxTags: 16,
} as const;

// Shared module instance (loaded once across all test files)
let cachedModule: WasmModule | null = null;

export async function getModule(): Promise<WasmModule> {
  if (!cachedModule) {
    cachedModule = (await createModule()) as WasmModule;
  }
  return cachedModule;
}

export function allocString(module: WasmModule, text: string): number {
  const bytes = module.lengthBytesUTF8(text) + 1;
  const ptr = module._malloc(bytes);
  module.stringToUTF8(text, ptr, bytes);
  return ptr;
}

export interface ParsedMorpheme {
  surface: string;
  pos: string;
  baseForm: string;
  posJa: string;
  conjType: string | null;
  conjForm: string | null;
  extendedPos: string;
}

export function parseMorphemes(module: WasmModule, resultPtr: number): ParsedMorpheme[] {
  const morphemesPtr = module.HEAPU32[(resultPtr + RESULT_LAYOUT.morphemes) >> 2];
  const count = module.HEAPU32[(resultPtr + RESULT_LAYOUT.count) >> 2];
  const morphemes: ParsedMorpheme[] = [];

  for (let i = 0; i < count; i++) {
    const morphPtr = morphemesPtr + i * MORPHEME_LAYOUT.size;
    const surfacePtr = module.HEAPU32[(morphPtr + MORPHEME_LAYOUT.surface) >> 2];
    const posPtr = module.HEAPU32[(morphPtr + MORPHEME_LAYOUT.pos) >> 2];
    const baseFormPtr = module.HEAPU32[(morphPtr + MORPHEME_LAYOUT.baseForm) >> 2];
    const posJaPtr = module.HEAPU32[(morphPtr + MORPHEME_LAYOUT.posJa) >> 2];
    const conjTypePtr = module.HEAPU32[(morphPtr + MORPHEME_LAYOUT.conjType) >> 2];
    const conjFormPtr = module.HEAPU32[(morphPtr + MORPHEME_LAYOUT.conjForm) >> 2];
    const extendedPosPtr = module.HEAPU32[(morphPtr + MORPHEME_LAYOUT.extendedPos) >> 2];

    morphemes.push({
      surface: module.UTF8ToString(surfacePtr),
      pos: module.UTF8ToString(posPtr),
      baseForm: module.UTF8ToString(baseFormPtr),
      posJa: module.UTF8ToString(posJaPtr),
      conjType: conjTypePtr !== 0 ? module.UTF8ToString(conjTypePtr) : null,
      conjForm: conjFormPtr !== 0 ? module.UTF8ToString(conjFormPtr) : null,
      extendedPos: module.UTF8ToString(extendedPosPtr),
    });
  }
  return morphemes;
}

// suzume_tags_t layout: { char** tags; const char** pos; size_t count; }
export interface ParsedTag {
  tag: string;
  pos: string;
}

export function parseTags(module: WasmModule, tagsPtr: number): ParsedTag[] {
  const tagsArrayPtr = module.HEAPU32[(tagsPtr + TAGS_LAYOUT.tags) >> 2];
  const posArrayPtr = module.HEAPU32[(tagsPtr + TAGS_LAYOUT.pos) >> 2];
  const count = module.HEAPU32[(tagsPtr + TAGS_LAYOUT.count) >> 2];
  const tags: ParsedTag[] = [];

  for (let i = 0; i < count; i++) {
    const tagStrPtr = module.HEAPU32[(tagsArrayPtr >> 2) + i];
    const posStrPtr = module.HEAPU32[(posArrayPtr >> 2) + i];
    tags.push({
      tag: module.UTF8ToString(tagStrPtr),
      pos: module.UTF8ToString(posStrPtr),
    });
  }
  return tags;
}

export function getTagCount(module: WasmModule, tagsPtr: number): number {
  return module.HEAPU32[(tagsPtr + TAGS_LAYOUT.count) >> 2];
}
