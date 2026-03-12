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

// suzume_morpheme_t layout: 8 pointers (32 bytes on wasm32)
export const MORPHEME_SIZE = 32;

export interface ParsedMorpheme {
  surface: string;
  pos: string;
  baseForm: string;
  reading: string;
  posJa: string;
  conjType: string | null;
  conjForm: string | null;
  extendedPos: string;
}

export function parseMorphemes(module: WasmModule, resultPtr: number): ParsedMorpheme[] {
  const morphemesPtr = module.HEAPU32[resultPtr >> 2];
  const count = module.HEAPU32[(resultPtr >> 2) + 1];
  const morphemes: ParsedMorpheme[] = [];

  for (let i = 0; i < count; i++) {
    const morphPtr = morphemesPtr + i * MORPHEME_SIZE;
    const surfacePtr = module.HEAPU32[morphPtr >> 2];
    const posPtr = module.HEAPU32[(morphPtr >> 2) + 1];
    const baseFormPtr = module.HEAPU32[(morphPtr >> 2) + 2];
    const readingPtr = module.HEAPU32[(morphPtr >> 2) + 3];
    const posJaPtr = module.HEAPU32[(morphPtr >> 2) + 4];
    const conjTypePtr = module.HEAPU32[(morphPtr >> 2) + 5];
    const conjFormPtr = module.HEAPU32[(morphPtr >> 2) + 6];
    const extendedPosPtr = module.HEAPU32[(morphPtr >> 2) + 7];

    morphemes.push({
      surface: module.UTF8ToString(surfacePtr),
      pos: module.UTF8ToString(posPtr),
      baseForm: module.UTF8ToString(baseFormPtr),
      reading: module.UTF8ToString(readingPtr),
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
  const tagsArrayPtr = module.HEAPU32[tagsPtr >> 2];
  const posArrayPtr = module.HEAPU32[(tagsPtr >> 2) + 1];
  const count = module.HEAPU32[(tagsPtr >> 2) + 2];
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
  return module.HEAPU32[(tagsPtr >> 2) + 2];
}
