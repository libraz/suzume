/**
 * Suzume - Lightweight Japanese morphological analyzer
 *
 * @example
 * ```typescript
 * import { Suzume } from 'suzume';
 *
 * const suzume = await Suzume.create();
 * const result = suzume.analyze('すもももももももものうち');
 * console.log(result);
 * ```
 */

// Types for Emscripten module
interface EmscriptenModule {
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
}

/**
 * Options for creating a Suzume instance
 */
export interface SuzumeOptions {
  /** Preserve ヴ (don't normalize to ビ etc.), default: true */
  preserveVu?: boolean;
  /** Preserve case (don't lowercase ASCII), default: true */
  preserveCase?: boolean;
  /** Preserve symbols/emoji in output, default: false */
  preserveSymbols?: boolean;
}

/**
 * Morpheme - A single unit of morphological analysis
 */
export interface Morpheme {
  /** Surface form (as it appears in the text) */
  surface: string;
  /** Part of speech (English) */
  pos: string;
  /** Base/dictionary form */
  baseForm: string;
  /** Reading in katakana */
  reading: string;
  /** Part of speech (Japanese) */
  posJa: string;
  /** Conjugation type (Japanese, e.g., "一段", "五段・カ行") - null for non-conjugating words */
  conjType: string | null;
  /** Conjugation form (Japanese, e.g., "連用形", "終止形") - null for non-conjugating words */
  conjForm: string | null;
}

/**
 * Tag entry with POS information
 */
export interface Tag {
  /** Tag text (surface or lemma) */
  tag: string;
  /** Part of speech (English) */
  pos: string;
}

/**
 * Options for tag generation
 */
export interface TagOptions {
  /** POS categories to include (default: all content words) */
  pos?: ('noun' | 'verb' | 'adjective' | 'adverb')[];
  /** Exclude basic/common words with hiragana-only lemma (default: false) */
  excludeBasic?: boolean;
  /** Use lemma instead of surface form (default: true) */
  useLemma?: boolean;
  /** Minimum tag length in characters (default: 2) */
  minLength?: number;
  /** Maximum number of tags, 0 for unlimited (default: 0) */
  maxTags?: number;
}

// Release handle ref for destructor so the destroy fn is not bound to the Suzume instance
interface CleanupRef {
  module: EmscriptenModule;
  handle: number;
}

const registry = new FinalizationRegistry((ref: CleanupRef) => {
  if (ref.handle !== 0) {
    const destroyHandle = ref.module.cwrap('suzume_destroy', null, ['number']) as (
      handle: number,
    ) => void;
    destroyHandle(ref.handle);
    ref.handle = 0;
  }
});

/**
 * Suzume instance for Japanese morphological analysis
 */
export class Suzume {
  private module: EmscriptenModule;
  private handle: number;
  private cleanupRef: CleanupRef;
  private _analyze: (handle: number, textPtr: number) => number;
  private _resultFree: (resultPtr: number) => void;
  private _generateTags: (handle: number, textPtr: number) => number;
  private _generateTagsWithOptions: (handle: number, textPtr: number, optionsPtr: number) => number;
  private _tagsFree: (tagsPtr: number) => void;
  private _loadUserDict: (handle: number, dataPtr: number, size: number) => number;
  private _loadBinaryDict: (handle: number, dataPtr: number, size: number) => number;
  private _version: () => number;

  private constructor(module: EmscriptenModule, handle: number) {
    this.module = module;
    this.handle = handle;
    this.cleanupRef = { module, handle };
    registry.register(this, this.cleanupRef);

    // Wrap C functions
    this._analyze = module.cwrap('suzume_analyze', 'number', ['number', 'number']) as (
      handle: number,
      textPtr: number,
    ) => number;

    this._resultFree = module.cwrap('suzume_result_free', null, ['number']) as (
      resultPtr: number,
    ) => void;

    this._generateTags = module.cwrap('suzume_generate_tags', 'number', ['number', 'number']) as (
      handle: number,
      textPtr: number,
    ) => number;

    this._generateTagsWithOptions = module.cwrap('suzume_generate_tags_with_options', 'number', [
      'number',
      'number',
      'number',
    ]) as (handle: number, textPtr: number, optionsPtr: number) => number;

    this._tagsFree = module.cwrap('suzume_tags_free', null, ['number']) as (
      tagsPtr: number,
    ) => void;

    this._loadUserDict = module.cwrap('suzume_load_user_dict', 'number', [
      'number',
      'number',
      'number',
    ]) as (handle: number, dataPtr: number, size: number) => number;

    this._loadBinaryDict = module.cwrap('suzume_load_binary_dict', 'number', [
      'number',
      'number',
      'number',
    ]) as (handle: number, dataPtr: number, size: number) => number;

    this._version = module.cwrap('suzume_version', 'number', []) as () => number;
  }

  /**
   * Create a new Suzume instance
   *
   * @param options - Optional configuration options
   * @returns Promise resolving to Suzume instance
   */
  static async create(options?: SuzumeOptions & { wasmPath?: string }): Promise<Suzume> {
    const wasmPath = options?.wasmPath;

    // Dynamic import of the Emscripten-generated module
    const createModule = await import('./suzume.js');
    const module: EmscriptenModule = await createModule.default({
      locateFile: (path: string) => {
        if (path.endsWith('.wasm') && wasmPath) {
          return wasmPath;
        }
        return path;
      },
    });

    let handle: number;

    if (
      options &&
      (options.preserveVu !== undefined ||
        options.preserveCase !== undefined ||
        options.preserveSymbols !== undefined)
    ) {
      // Create with options
      // suzume_options_t layout: 3 ints (12 bytes on wasm32)
      const OPTIONS_SIZE = 12;
      const optionsPtr = module._malloc(OPTIONS_SIZE);

      try {
        const heap = (module as unknown as { HEAPU32: Uint32Array }).HEAPU32;
        // preserve_vu: default true
        heap[optionsPtr >> 2] = options.preserveVu !== false ? 1 : 0;
        // preserve_case: default true
        heap[(optionsPtr >> 2) + 1] = options.preserveCase !== false ? 1 : 0;
        // preserve_symbols: default false
        heap[(optionsPtr >> 2) + 2] = options.preserveSymbols === true ? 1 : 0;

        const createWithOptions = module.cwrap('suzume_create_with_options', 'number', [
          'number',
        ]) as (optionsPtr: number) => number;
        handle = createWithOptions(optionsPtr);
      } finally {
        module._free(optionsPtr);
      }
    } else {
      // Create with default options
      const createHandle = module.cwrap('suzume_create', 'number', []) as () => number;
      handle = createHandle();
    }

    if (handle === 0) {
      throw new Error('Failed to create Suzume instance');
    }

    return new Suzume(module, handle);
  }

  /**
   * Analyze Japanese text into morphemes
   *
   * @param text - UTF-8 encoded Japanese text
   * @returns Array of morphemes
   */
  analyze(text: string): Morpheme[] {
    const textBytes = this.module.lengthBytesUTF8(text) + 1;
    const textPtr = this.module._malloc(textBytes);

    try {
      this.module.stringToUTF8(text, textPtr, textBytes);
      const resultPtr = this._analyze(this.handle, textPtr);

      if (resultPtr === 0) {
        return [];
      }

      try {
        return this.parseResult(resultPtr);
      } finally {
        this._resultFree(resultPtr);
      }
    } finally {
      this.module._free(textPtr);
    }
  }

  /**
   * Generate tags from Japanese text
   *
   * @param text - UTF-8 encoded Japanese text
   * @param options - Optional tag generation options
   * @returns Array of tag entries with POS information
   */
  generateTags(text: string, options?: TagOptions): Tag[] {
    const textBytes = this.module.lengthBytesUTF8(text) + 1;
    const textPtr = this.module._malloc(textBytes);

    try {
      this.module.stringToUTF8(text, textPtr, textBytes);

      if (options) {
        // Build pos_filter bitmask
        let posFilter = 0;
        if (options.pos) {
          const posMap: Record<string, number> = { noun: 1, verb: 2, adjective: 4, adverb: 8 };
          for (const pos of options.pos) {
            posFilter |= posMap[pos] ?? 0;
          }
        }

        // suzume_tag_options_t layout (wasm32):
        //   offset 0: pos_filter (uint8_t, padded to 4 bytes due to next int)
        //   offset 4: exclude_basic (int, 4 bytes)
        //   offset 8: use_lemma (int, 4 bytes)
        //   offset 12: min_length (size_t = uint32 on wasm, 4 bytes)
        //   offset 16: max_tags (size_t = uint32 on wasm, 4 bytes)
        const OPTIONS_SIZE = 20;
        const optionsPtr = this.module._malloc(OPTIONS_SIZE);

        try {
          const heapU32 = (this.module as unknown as { HEAPU32: Uint32Array }).HEAPU32;

          // pos_filter is uint8_t at offset 0, padded to 4 bytes — write as uint32
          heapU32[optionsPtr >> 2] = posFilter;
          heapU32[(optionsPtr + 4) >> 2] = options.excludeBasic ? 1 : 0;
          heapU32[(optionsPtr + 8) >> 2] = options.useLemma !== false ? 1 : 0;
          heapU32[(optionsPtr + 12) >> 2] = options.minLength ?? 2;
          heapU32[(optionsPtr + 16) >> 2] = options.maxTags ?? 0;

          const tagsPtr = this._generateTagsWithOptions(this.handle, textPtr, optionsPtr);
          if (tagsPtr === 0) {
            return [];
          }

          try {
            return this.parseTags(tagsPtr);
          } finally {
            this._tagsFree(tagsPtr);
          }
        } finally {
          this.module._free(optionsPtr);
        }
      }

      const tagsPtr = this._generateTags(this.handle, textPtr);

      if (tagsPtr === 0) {
        return [];
      }

      try {
        return this.parseTags(tagsPtr);
      } finally {
        this._tagsFree(tagsPtr);
      }
    } finally {
      this.module._free(textPtr);
    }
  }

  /**
   * Load user dictionary from string data
   *
   * @param data - Dictionary data in CSV format
   * @returns true on success
   */
  loadUserDictionary(data: string): boolean {
    const dataBytes = this.module.lengthBytesUTF8(data) + 1;
    const dataPtr = this.module._malloc(dataBytes);

    try {
      this.module.stringToUTF8(data, dataPtr, dataBytes);
      return this._loadUserDict(this.handle, dataPtr, dataBytes - 1) === 1;
    } finally {
      this.module._free(dataPtr);
    }
  }

  /**
   * Load binary dictionary from buffer data (as user dictionary)
   *
   * @param data - Binary dictionary data (.dic format)
   * @returns true on success
   */
  loadBinaryDictionary(data: Uint8Array): boolean {
    const dataPtr = this.module._malloc(data.byteLength);
    try {
      // Derive Uint8Array view from HEAPU32's underlying buffer (HEAPU8 may not be exported)
      const heapU32 = (this.module as unknown as { HEAPU32: Uint32Array }).HEAPU32;
      const heapU8 = new Uint8Array(heapU32.buffer);
      heapU8.set(data, dataPtr);
      return this._loadBinaryDict(this.handle, dataPtr, data.byteLength) === 1;
    } finally {
      this.module._free(dataPtr);
    }
  }

  /**
   * Get Suzume version string
   */
  get version(): string {
    const versionPtr = this._version();
    return this.module.UTF8ToString(versionPtr);
  }

  /**
   * Destroy the Suzume instance and free resources.
   * Called automatically via FinalizationRegistry when garbage collected,
   * but can be called explicitly for immediate cleanup.
   */
  destroy(): void {
    if (this.handle !== 0) {
      registry.unregister(this);
      const destroyHandle = this.module.cwrap('suzume_destroy', null, ['number']) as (
        handle: number,
      ) => void;
      destroyHandle(this.handle);
      this.handle = 0;
      this.cleanupRef.handle = 0;
    }
  }

  // Parse suzume_result_t structure from WASM memory
  private parseResult(resultPtr: number): Morpheme[] {
    // suzume_result_t layout:
    // - morphemes: pointer (4 bytes on wasm32)
    // - count: size_t (4 bytes on wasm32)
    const HEAPU32 = (this.module as unknown as { HEAPU32: Uint32Array }).HEAPU32;

    const morphemesPtr = HEAPU32[resultPtr >> 2];
    const count = HEAPU32[(resultPtr >> 2) + 1];

    const morphemes: Morpheme[] = [];

    // suzume_morpheme_t layout (7 pointers = 28 bytes on wasm32):
    // - surface: pointer
    // - pos: pointer
    // - base_form: pointer
    // - reading: pointer
    // - pos_ja: pointer
    // - conj_type: pointer
    // - conj_form: pointer
    const MORPHEME_SIZE = 28;

    for (let idx = 0; idx < count; idx++) {
      const morphPtr = morphemesPtr + idx * MORPHEME_SIZE;
      const surfacePtr = HEAPU32[morphPtr >> 2];
      const posPtr = HEAPU32[(morphPtr >> 2) + 1];
      const baseFormPtr = HEAPU32[(morphPtr >> 2) + 2];
      const readingPtr = HEAPU32[(morphPtr >> 2) + 3];
      const posJaPtr = HEAPU32[(morphPtr >> 2) + 4];
      const conjTypePtr = HEAPU32[(morphPtr >> 2) + 5];
      const conjFormPtr = HEAPU32[(morphPtr >> 2) + 6];

      morphemes.push({
        surface: this.module.UTF8ToString(surfacePtr),
        pos: this.module.UTF8ToString(posPtr),
        baseForm: this.module.UTF8ToString(baseFormPtr),
        reading: this.module.UTF8ToString(readingPtr),
        posJa: this.module.UTF8ToString(posJaPtr),
        conjType: conjTypePtr !== 0 ? this.module.UTF8ToString(conjTypePtr) : null,
        conjForm: conjFormPtr !== 0 ? this.module.UTF8ToString(conjFormPtr) : null,
      });
    }

    return morphemes;
  }

  // Parse suzume_tags_t structure from WASM memory
  private parseTags(tagsPtr: number): Tag[] {
    // suzume_tags_t layout (wasm32):
    // - tags: pointer to char** (4 bytes)
    // - pos: pointer to const char** (4 bytes)
    // - count: size_t (4 bytes)
    const HEAPU32 = (this.module as unknown as { HEAPU32: Uint32Array }).HEAPU32;

    const tagsArrayPtr = HEAPU32[tagsPtr >> 2];
    const posArrayPtr = HEAPU32[(tagsPtr >> 2) + 1];
    const count = HEAPU32[(tagsPtr >> 2) + 2];

    const tags: Tag[] = [];

    for (let idx = 0; idx < count; idx++) {
      const tagPtr = HEAPU32[(tagsArrayPtr >> 2) + idx];
      const posPtr = HEAPU32[(posArrayPtr >> 2) + idx];
      tags.push({
        tag: this.module.UTF8ToString(tagPtr),
        pos: this.module.UTF8ToString(posPtr),
      });
    }

    return tags;
  }
}

// Default export
export default Suzume;
