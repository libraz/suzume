import { MORPHEME_FLAG } from './abi_labels.js';
import { C_LAYOUTS } from './abi_layout.js';
import type { AnalysisResult, Morpheme, PosLabel, Tag } from './index.js';

export interface DecodeMemory {
  UTF8ToString: (ptr: number) => string;
  HEAPU32: Uint32Array;
}

const utf8Decoder = new TextDecoder('utf-8');

function decodeUtf8(buffer: ArrayBufferLike, ptr: number, size: number): string {
  return utf8Decoder.decode(new Uint8Array(buffer, ptr, size));
}

/** Byte view over the module heap, derived from HEAPU32 (HEAPU8 may not be exported). */
export function heapBytes(module: Pick<DecodeMemory, 'HEAPU32'>): Uint8Array {
  return new Uint8Array(module.HEAPU32.buffer);
}

/** Maps a native code to its label; `null` means the morpheme has none. */
export type OptionalLabelLookup = (code: number) => string | null;
export type LabelLookup = (code: number) => string;
export type PosLabelLookup = (code: number) => PosLabel;

export function decodeAnalysisResult(
  module: DecodeMemory,
  resultPtr: number,
  conjugationTypeLabel: OptionalLabelLookup,
  conjugationFormLabel: OptionalLabelLookup,
  extendedPosLabel: LabelLookup,
  posLabel: PosLabelLookup,
  posJapaneseLabel: LabelLookup,
): AnalysisResult {
  const heapU32 = module.HEAPU32;
  const heapU8 = heapBytes(module);
  const heapF32 = new Float32Array(heapU32.buffer);
  const resultLayout = C_LAYOUTS.result;
  const morphemeLayout = C_LAYOUTS.morpheme;
  const morphemesPtr = heapU32[(resultPtr + resultLayout.morphemes) >> 2];
  const count = heapU32[(resultPtr + resultLayout.count) >> 2];
  const normalizedTextPtr = heapU32[(resultPtr + resultLayout.normalizedText) >> 2];
  const normalizedTextSize = heapU32[(resultPtr + resultLayout.normalizedTextSize) >> 2];
  const normalizedText = decodeUtf8(heapU32.buffer, normalizedTextPtr, normalizedTextSize);
  const utf16Offsets = [0];
  let utf16Offset = 0;
  for (const codePoint of normalizedText) {
    utf16Offset += codePoint.length;
    utf16Offsets.push(utf16Offset);
  }
  const morphemes: Morpheme[] = [];

  for (let idx = 0; idx < count; idx++) {
    const morphPtr = morphemesPtr + idx * morphemeLayout.size;
    const surfacePtr = heapU32[(morphPtr + morphemeLayout.surface) >> 2];
    const baseFormPtr = heapU32[(morphPtr + morphemeLayout.baseForm) >> 2];
    const posCode = heapU8[morphPtr + morphemeLayout.pos];
    const flags = heapU8[morphPtr + morphemeLayout.flags];
    const conjugates = (flags & MORPHEME_FLAG.conjugatable) !== 0;
    const start = heapU32[(morphPtr + morphemeLayout.start) >> 2];
    const end = heapU32[(morphPtr + morphemeLayout.end) >> 2];
    const surfaceSize = heapU32[(morphPtr + morphemeLayout.surfaceSize) >> 2];
    const baseFormSize = heapU32[(morphPtr + morphemeLayout.baseFormSize) >> 2];
    morphemes.push({
      surface: decodeUtf8(heapU32.buffer, surfacePtr, surfaceSize),
      pos: posLabel(posCode),
      baseForm: decodeUtf8(heapU32.buffer, baseFormPtr, baseFormSize),
      posJa: posJapaneseLabel(posCode),
      conjType: conjugates
        ? conjugationTypeLabel(heapU8[morphPtr + morphemeLayout.conjugationType])
        : null,
      conjForm: conjugates
        ? conjugationFormLabel(heapU8[morphPtr + morphemeLayout.conjugationForm])
        : null,
      extendedPos: extendedPosLabel(heapU8[morphPtr + morphemeLayout.extendedPos]),
      start,
      end,
      startUtf16: utf16Offsets[start] ?? normalizedText.length,
      endUtf16: utf16Offsets[end] ?? normalizedText.length,
      isUserDict: (flags & MORPHEME_FLAG.userDict) !== 0,
      isFormalNoun: (flags & MORPHEME_FLAG.formalNoun) !== 0,
      isLowInfo: (flags & MORPHEME_FLAG.lowInfo) !== 0,
      isUnknown: (flags & MORPHEME_FLAG.unknown) !== 0,
      isFromDictionary: (flags & MORPHEME_FLAG.fromDictionary) !== 0,
      score: heapF32[(morphPtr + morphemeLayout.score) >> 2],
    });
  }

  return {
    normalizedText,
    morphemes,
  };
}

export function decodeTags(module: DecodeMemory, tagsPtr: number, posLabel: PosLabelLookup): Tag[] {
  const heapU32 = module.HEAPU32;
  const heapU8 = heapBytes(module);
  const layout = C_LAYOUTS.tags;
  const tagsArrayPtr = heapU32[(tagsPtr + layout.tags) >> 2];
  const posArrayPtr = heapU32[(tagsPtr + layout.pos) >> 2];
  const count = heapU32[(tagsPtr + layout.count) >> 2];
  const tags: Tag[] = [];

  for (let idx = 0; idx < count; idx++) {
    const tagPtr = heapU32[(tagsArrayPtr >> 2) + idx];
    tags.push({
      tag: module.UTF8ToString(tagPtr),
      pos: posLabel(heapU8[posArrayPtr + idx]),
    });
  }
  return tags;
}
