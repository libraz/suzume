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
  /** Part of speech (Japanese) */
  posJa: string;
  /** Conjugation type (Japanese, e.g., "一段", "五段・カ行") - null for non-conjugating words */
  conjType: string | null;
  /** Conjugation form (Japanese, e.g., "連用形", "終止形") - null for non-conjugating words */
  conjForm: string | null;
  /** Extended POS subcategory (English, e.g., "VerbRenyokei", "AuxTenseTa") */
  extendedPos: string;
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

interface CLayouts {
  result: {
    size: number;
    morphemes: number;
    count: number;
  };
  morpheme: {
    size: number;
    surface: number;
    pos: number;
    baseForm: number;
    posJa: number;
    conjType: number;
    conjForm: number;
    extendedPos: number;
  };
  tags: {
    size: number;
    tags: number;
    pos: number;
    count: number;
  };
  tagOptions: {
    size: number;
    posFilter: number;
    excludeBasic: number;
    useLemma: number;
    minLength: number;
    maxTags: number;
  };
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
  private _lastError: () => number;
  private layouts: CLayouts;
  private unregisterToken = {};

  private constructor(module: EmscriptenModule, handle: number) {
    this.module = module;
    this.handle = handle;
    this.cleanupRef = { module, handle };
    registry.register(this, this.cleanupRef, this.unregisterToken);

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
    this._lastError = module.cwrap('suzume_last_error', 'number', []) as () => number;
    this.layouts = Suzume.loadCLayouts(module);
  }

  private static loadCLayouts(module: EmscriptenModule): CLayouts {
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

    return {
      result: {
        size: sizeofResult(),
        morphemes: offsetofResult(0),
        count: offsetofResult(1),
      },
      morpheme: {
        size: sizeofMorpheme(),
        surface: offsetofMorpheme(0),
        pos: offsetofMorpheme(1),
        baseForm: offsetofMorpheme(2),
        posJa: offsetofMorpheme(3),
        conjType: offsetofMorpheme(4),
        conjForm: offsetofMorpheme(5),
        extendedPos: offsetofMorpheme(6),
      },
      tags: {
        size: sizeofTags(),
        tags: offsetofTags(0),
        pos: offsetofTags(1),
        count: offsetofTags(2),
      },
      tagOptions: {
        size: sizeofTagOptions(),
        posFilter: offsetofTagOptions(0),
        excludeBasic: offsetofTagOptions(1),
        useLemma: offsetofTagOptions(2),
        minLength: offsetofTagOptions(3),
        maxTags: offsetofTagOptions(4),
      },
    };
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
    const moduleOptions: Record<string, unknown> = {};
    if (wasmPath) {
      moduleOptions.locateFile = (path: string) => (path.endsWith('.wasm') ? wasmPath : path);
    }
    const module: EmscriptenModule = await createModule.default(moduleOptions);

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
      const lastError = module.cwrap('suzume_last_error', 'number', []) as () => number;
      const message = module.UTF8ToString(lastError());
      throw new Error(
        message
          ? `Failed to create Suzume instance: ${message}`
          : 'Failed to create Suzume instance',
      );
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
    this.ensureAlive();

    const textBytes = this.module.lengthBytesUTF8(text) + 1;
    const textPtr = this.module._malloc(textBytes);

    try {
      this.module.stringToUTF8(text, textPtr, textBytes);
      const resultPtr = this._analyze(this.handle, textPtr);

      if (resultPtr === 0) {
        throw new Error(`Suzume analyze failed: ${this.lastError || 'unknown error'}`);
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
    this.ensureAlive();

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

        const optionsPtr = this.module._malloc(this.layouts.tagOptions.size);

        try {
          const heapU32 = (this.module as unknown as { HEAPU32: Uint32Array }).HEAPU32;
          const layout = this.layouts.tagOptions;

          heapU32[(optionsPtr + layout.posFilter) >> 2] = posFilter;
          heapU32[(optionsPtr + layout.excludeBasic) >> 2] = options.excludeBasic ? 1 : 0;
          heapU32[(optionsPtr + layout.useLemma) >> 2] = options.useLemma !== false ? 1 : 0;
          heapU32[(optionsPtr + layout.minLength) >> 2] = options.minLength ?? 2;
          heapU32[(optionsPtr + layout.maxTags) >> 2] = options.maxTags ?? 0;

          const tagsPtr = this._generateTagsWithOptions(this.handle, textPtr, optionsPtr);
          if (tagsPtr === 0) {
            throw new Error(`Suzume tag generation failed: ${this.lastError || 'unknown error'}`);
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
        throw new Error(`Suzume tag generation failed: ${this.lastError || 'unknown error'}`);
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
    this.ensureAlive();

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
   * Load user dictionary from string data, throwing with C API details on failure.
   *
   * @param data - Dictionary data in CSV format
   */
  loadUserDictionaryOrThrow(data: string): void {
    if (!this.loadUserDictionary(data)) {
      throw new Error(`Suzume user dictionary load failed: ${this.lastError || 'unknown error'}`);
    }
  }

  /**
   * Load binary dictionary from buffer data (as user dictionary)
   *
   * @param data - Binary dictionary data (.dic format)
   * @returns true on success
   */
  loadBinaryDictionary(data: Uint8Array): boolean {
    this.ensureAlive();

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
   * Load binary dictionary from buffer data, throwing with C API details on failure.
   *
   * @param data - Binary dictionary data (.dic format)
   */
  loadBinaryDictionaryOrThrow(data: Uint8Array): void {
    if (!this.loadBinaryDictionary(data)) {
      throw new Error(`Suzume binary dictionary load failed: ${this.lastError || 'unknown error'}`);
    }
  }

  /**
   * Get Suzume version string
   */
  get version(): string {
    this.ensureAlive();

    const versionPtr = this._version();
    return this.module.UTF8ToString(versionPtr);
  }

  /**
   * Last C API error for this thread, or empty string if the last C API call succeeded.
   */
  get lastError(): string {
    return this.module.UTF8ToString(this._lastError());
  }

  /**
   * Destroy the Suzume instance and free resources.
   * Called automatically via FinalizationRegistry when garbage collected,
   * but can be called explicitly for immediate cleanup.
   */
  destroy(): void {
    if (this.handle !== 0) {
      registry.unregister(this.unregisterToken);
      const destroyHandle = this.module.cwrap('suzume_destroy', null, ['number']) as (
        handle: number,
      ) => void;
      destroyHandle(this.handle);
      this.handle = 0;
      this.cleanupRef.handle = 0;
    }
  }

  private ensureAlive(): void {
    if (this.handle === 0) {
      throw new Error('Suzume instance has been destroyed');
    }
  }

  // Parse suzume_result_t structure from WASM memory
  private parseResult(resultPtr: number): Morpheme[] {
    const HEAPU32 = (this.module as unknown as { HEAPU32: Uint32Array }).HEAPU32;
    const resultLayout = this.layouts.result;
    const morphemeLayout = this.layouts.morpheme;

    const morphemesPtr = HEAPU32[(resultPtr + resultLayout.morphemes) >> 2];
    const count = HEAPU32[(resultPtr + resultLayout.count) >> 2];

    const morphemes: Morpheme[] = [];

    for (let idx = 0; idx < count; idx++) {
      const morphPtr = morphemesPtr + idx * morphemeLayout.size;
      const surfacePtr = HEAPU32[(morphPtr + morphemeLayout.surface) >> 2];
      const posPtr = HEAPU32[(morphPtr + morphemeLayout.pos) >> 2];
      const baseFormPtr = HEAPU32[(morphPtr + morphemeLayout.baseForm) >> 2];
      const posJaPtr = HEAPU32[(morphPtr + morphemeLayout.posJa) >> 2];
      const conjTypePtr = HEAPU32[(morphPtr + morphemeLayout.conjType) >> 2];
      const conjFormPtr = HEAPU32[(morphPtr + morphemeLayout.conjForm) >> 2];
      const extendedPosPtr = HEAPU32[(morphPtr + morphemeLayout.extendedPos) >> 2];

      morphemes.push({
        surface: this.module.UTF8ToString(surfacePtr),
        pos: this.module.UTF8ToString(posPtr),
        baseForm: this.module.UTF8ToString(baseFormPtr),
        posJa: this.module.UTF8ToString(posJaPtr),
        conjType: conjTypePtr !== 0 ? this.module.UTF8ToString(conjTypePtr) : null,
        conjForm: conjFormPtr !== 0 ? this.module.UTF8ToString(conjFormPtr) : null,
        extendedPos: this.module.UTF8ToString(extendedPosPtr),
      });
    }

    return morphemes;
  }

  // Parse suzume_tags_t structure from WASM memory
  private parseTags(tagsPtr: number): Tag[] {
    const HEAPU32 = (this.module as unknown as { HEAPU32: Uint32Array }).HEAPU32;
    const layout = this.layouts.tags;

    const tagsArrayPtr = HEAPU32[(tagsPtr + layout.tags) >> 2];
    const posArrayPtr = HEAPU32[(tagsPtr + layout.pos) >> 2];
    const count = HEAPU32[(tagsPtr + layout.count) >> 2];

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
