import createModule from '../dist/suzume.js';
import { posEnglish } from '../js/abi_labels.js';
import { C_LAYOUTS } from '../js/abi_layout.js';
import { decodeAnalysisResult, decodeTags } from '../js/decode.js';
import type { Morpheme as ParsedMorpheme, Tag as ParsedTag } from '../js/index.js';

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

export const RESULT_LAYOUT = C_LAYOUTS.result;
export const MORPHEME_LAYOUT = C_LAYOUTS.morpheme;
export const TAGS_LAYOUT = C_LAYOUTS.tags;
export const TAG_OPTIONS_LAYOUT = C_LAYOUTS.tagOptions;
export const EXTENDED_OPTIONS_LAYOUT = C_LAYOUTS.extendedOptions;

// Shared module instance (loaded once across all test files)
let cachedModule: WasmModule | null = null;

export async function getModule(): Promise<WasmModule> {
  if (!cachedModule) {
    const module = (await createModule()) as WasmModule & Record<string, unknown>;
    // C-API tests intentionally exercise functions by their public C names.
    // Production code calls the exported `_suzume_*` functions directly, so
    // keep this compatibility adapter in tests rather than shipping cwrap.
    module.cwrap = (name: string) => {
      const fn = module[`_${name}`];
      if (typeof fn !== 'function') {
        throw new Error(`Missing WASM export: ${name}`);
      }
      return fn as (...args: unknown[]) => unknown;
    };
    cachedModule = module;
  }
  return cachedModule;
}

export function allocString(module: WasmModule, text: string): number {
  const bytes = module.lengthBytesUTF8(text) + 1;
  const ptr = module._malloc(bytes);
  module.stringToUTF8(text, ptr, bytes);
  return ptr;
}

export interface TagOptionValues {
  posFilter?: number;
  excludeBasic?: boolean;
  useLemma?: boolean;
  minLength?: number;
  maxTags?: number;
  excludeParticles?: boolean;
  excludeAuxiliaries?: boolean;
  excludeFormalNouns?: boolean;
  excludeLowInfo?: boolean;
  removeDuplicates?: boolean;
}

/** Allocate a suzume_tag_options_t with every field written (defaults match the C initializer). */
export function allocTagOptions(module: WasmModule, opts: TagOptionValues = {}): number {
  const ptr = module._malloc(TAG_OPTIONS_LAYOUT.size);
  const heapU8 = new Uint8Array(module.HEAPU32.buffer);
  heapU8[ptr + TAG_OPTIONS_LAYOUT.posFilter] = (opts.posFilter ?? 0) & 0xff;
  heapU8[ptr + TAG_OPTIONS_LAYOUT.excludeBasic] = opts.excludeBasic ? 1 : 0;
  heapU8[ptr + TAG_OPTIONS_LAYOUT.useLemma] = opts.useLemma !== false ? 1 : 0;
  module.HEAPU32[(ptr + TAG_OPTIONS_LAYOUT.minLength) >> 2] = opts.minLength ?? 2;
  module.HEAPU32[(ptr + TAG_OPTIONS_LAYOUT.maxTags) >> 2] = opts.maxTags ?? 0;
  heapU8[ptr + TAG_OPTIONS_LAYOUT.excludeParticles] = opts.excludeParticles !== false ? 1 : 0;
  heapU8[ptr + TAG_OPTIONS_LAYOUT.excludeAuxiliaries] = opts.excludeAuxiliaries !== false ? 1 : 0;
  heapU8[ptr + TAG_OPTIONS_LAYOUT.excludeFormalNouns] = opts.excludeFormalNouns !== false ? 1 : 0;
  heapU8[ptr + TAG_OPTIONS_LAYOUT.excludeLowInfo] = opts.excludeLowInfo !== false ? 1 : 0;
  heapU8[ptr + TAG_OPTIONS_LAYOUT.removeDuplicates] = opts.removeDuplicates !== false ? 1 : 0;
  return ptr;
}

export function parseMorphemes(module: WasmModule, resultPtr: number): ParsedMorpheme[] {
  const label = (name: string, code: number): string | null => {
    const ptr = module.cwrap(name, 'number', ['number'])(code) as number;
    return ptr === 0 ? null : module.UTF8ToString(ptr);
  };
  return decodeAnalysisResult(
    module,
    resultPtr,
    (code) => label('suzume_conjugation_type_label', code),
    (code) => label('suzume_conjugation_form_label', code),
    (code) => label('suzume_extended_pos_label', code) ?? 'UNKNOWN',
    posEnglish,
  ).morphemes;
}

export function parseTags(module: WasmModule, tagsPtr: number): ParsedTag[] {
  return decodeTags(module, tagsPtr, posEnglish);
}

export function getTagCount(module: WasmModule, tagsPtr: number): number {
  return module.HEAPU32[(tagsPtr + TAGS_LAYOUT.count) >> 2];
}
