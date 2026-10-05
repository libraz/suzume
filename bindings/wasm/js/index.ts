/**
 * Suzume - Lightweight Japanese tokenizer
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

import { C_LAYOUTS } from './abi_layout.js';
import { decodeAnalysisResult, decodeTags } from './decode.js';
import type { EmscriptenModule } from './suzume.js';

export enum ErrorCode {
  Success = 0,
  InvalidUtf8 = 1,
  DictionaryLoadFailed = 2,
  FileNotFound = 3,
  Parse = 4,
  OutOfMemory = 5,
  InvalidInput = 6,
  Internal = 7,
}

export class SuzumeError extends Error {
  readonly code: ErrorCode;

  constructor(message: string, code: ErrorCode = ErrorCode.Internal) {
    super(message);
    this.name = 'SuzumeError';
    this.code = code;
  }
}

const modulePromises = new Map<string, Promise<EmscriptenModule>>();

async function instantiateModule(
  wasmPath: string | undefined,
  freshWasmModule: boolean,
): Promise<EmscriptenModule> {
  const createModule = await import('./suzume.js');
  const moduleOptions: Record<string, unknown> = {};
  if (wasmPath) {
    moduleOptions.locateFile = (path: string) => (path.endsWith('.wasm') ? wasmPath : path);
  }
  if (freshWasmModule) {
    return createModule.default(moduleOptions);
  }

  const key = wasmPath ?? '';
  const cached = modulePromises.get(key);
  if (cached) {
    return cached;
  }
  const pending = createModule.default(moduleOptions) as Promise<EmscriptenModule>;
  modulePromises.set(key, pending);
  try {
    return await pending;
  } catch (error) {
    modulePromises.delete(key);
    throw error;
  }
}

/**
 * Options for creating a Suzume instance
 */
export interface SuzumeOptions {
  /** Create an isolated WASM runtime instead of sharing the cached module, default: false */
  freshWasmModule?: boolean;
  /** Preserve ヴ (don't normalize to ビ etc.), default: true */
  preserveVu?: boolean;
  /** Preserve case (don't lowercase ASCII), default: true */
  preserveCase?: boolean;
  /** Preserve symbols/emoji in output, default: false */
  preserveSymbols?: boolean;
  /** Analysis mode, default: normal */
  mode?: 'normal' | 'search' | 'split';
  /** Retain corrected lemmas; conjugation/POS annotations are always computed. Default: true */
  lemmatize?: boolean;
  /** Merge consecutive noun compounds, default: false */
  mergeCompounds?: boolean;
  /** Skip automatic loading of the bundled user dictionary, default: false */
  skipUserDictionary?: boolean;
  /** Skip automatic loading of the bundled core dictionary, default: false */
  skipCoreDictionary?: boolean;
  /** Ignore native scorer configuration environment variables, default: false */
  skipEnvConfig?: boolean;
  /** Add scorer configuration diagnostics to dictionaryWarnings, default: false */
  reportScorerConfig?: boolean;
  /** Final-priority scorer override JSON, or an object serialized to JSON */
  scorerOptions?: string | Record<string, unknown>;
}

type AnalysisMode = NonNullable<SuzumeOptions['mode']>;

const ANALYSIS_MODE_CODES: Readonly<Record<AnalysisMode, number>> = {
  normal: 0,
  search: 1,
  split: 2,
};

const ANALYSIS_MODE_NAMES: Readonly<Record<number, AnalysisMode>> = {
  0: 'normal',
  1: 'search',
  2: 'split',
};

// Keep the binding's public defaults explicit so CI can compare them with the
// C ABI initializer. Values are consumed below rather than duplicated there.
const EXTENDED_OPTION_DEFAULTS = {
  preserveVu: true,
  preserveCase: true,
  preserveSymbols: false,
  mode: 'normal',
  lemmatize: true,
  mergeCompounds: false,
  skipUserDictionary: false,
  skipCoreDictionary: false,
  skipEnvConfig: false,
  reportScorerConfig: false,
  scorerOptions: null,
  dataDirectory: null,
} as const;

/**
 * Morpheme - A single token produced by analysis
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
  /** Stable extended POS code (e.g., "VERB_連用", "AUX_過去") */
  extendedPos: string;
  /** Start Unicode code-point offset in normalized text */
  start: number;
  /** End Unicode code-point offset in normalized text */
  end: number;
  /** Start JavaScript UTF-16 offset, suitable for normalizedText.slice() */
  startUtf16: number;
  /** End JavaScript UTF-16 offset, suitable for normalizedText.slice() */
  endUtf16: number;
  /** True if matched from a user dictionary */
  isUserDict: boolean;
  /** True if the morpheme is a formal noun */
  isFormalNoun: boolean;
  /** True if the morpheme is low information for tag generation */
  isLowInfo: boolean;
  /** True if generated as an unknown word */
  isUnknown: boolean;
  /** True if matched from any dictionary */
  isFromDictionary: boolean;
  /** Candidate score/cost */
  score: number;
}

/** Normalized input together with its morphemes. */
export interface AnalysisResult {
  normalizedText: string;
  morphemes: Morpheme[];
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
export type TagPosFilterName = 'noun' | 'verb' | 'adjective' | 'adverb' | 'particle' | 'auxiliary';

export interface TagOptions {
  /**
   * POS categories to include. An empty array includes all filterable POS,
   * matching the native `pos_filter = 0` default.
   */
  posFilter?: readonly TagPosFilterName[];
  /**
   * Deprecated alias for `posFilter`. When both are present, `posFilter` wins.
   *
   * @deprecated Use `posFilter` instead.
   */
  pos?: readonly TagPosFilterName[];
  /** Exclude basic/common words with hiragana-only lemma (default: false) */
  excludeBasic?: boolean;
  /** Use lemma instead of surface form (default: true) */
  useLemma?: boolean;
  /** Minimum tag length in characters (default: 2) */
  minLength?: number;
  /** Maximum number of tags, 0 for unlimited (default: 0) */
  maxTags?: number;
  /** Exclude particles (default: true) */
  excludeParticles?: boolean;
  /** Exclude auxiliaries (default: true) */
  excludeAuxiliaries?: boolean;
  /** Exclude formal nouns (default: true) */
  excludeFormalNouns?: boolean;
  /** Exclude low information words (default: true) */
  excludeLowInfo?: boolean;
  /** Remove duplicate tags (default: true) */
  removeDuplicates?: boolean;
}

// As with construction options, this is checked against suzume_init_tag_options.
const TAG_OPTION_DEFAULTS = {
  posFilter: 0,
  excludeBasic: false,
  useLemma: true,
  minLength: 2,
  maxTags: 0,
  excludeParticles: true,
  excludeAuxiliaries: true,
  excludeFormalNouns: true,
  excludeLowInfo: true,
  removeDuplicates: true,
} as const;

const TAG_POS_FILTER_BITS: Readonly<Record<string, number>> = {
  noun: 1,
  verb: 2,
  adjective: 4,
  adverb: 8,
  particle: 16,
  auxiliary: 32,
};

function resolveTagPosFilter(options: TagOptions): number {
  const selectedPos = options.posFilter !== undefined ? options.posFilter : options.pos;
  let filter = 0;

  for (const pos of selectedPos ?? []) {
    const bit = TAG_POS_FILTER_BITS[pos];
    if (bit === undefined) {
      throw new Error(
        `unknown POS filter name: ${JSON.stringify(pos)} ` +
          `(expected one of ${Object.keys(TAG_POS_FILTER_BITS).sort().join(', ')})`,
      );
    }
    filter |= bit;
  }

  return filter;
}

// Release handle ref for destructor so the destroy fn is not bound to the Suzume instance
interface CleanupRef {
  module: EmscriptenModule;
  handle: number;
}

const registry = new FinalizationRegistry((ref: CleanupRef) => {
  if (ref.handle !== 0) {
    ref.module._suzume_destroy(ref.handle);
    ref.handle = 0;
  }
});

/**
 * Suzume instance for Japanese tokenization.
 *
 * Error contract note: under the WebAssembly build, a memory-allocation failure
 * aborts the module rather than returning NULL, so the C++ allocation-failure
 * path (which maps to a NULL return and a thrown Error on native/Python) is
 * effectively unreachable here.
 */
export class Suzume {
  private module: EmscriptenModule;
  private handle: number;
  private cleanupRef: CleanupRef;
  private _analyzeN: (handle: number, textPtr: number, size: number) => number;
  private _setMode: (handle: number, mode: number) => number;
  private _mode: (handle: number) => number;
  private _resultFree: (resultPtr: number) => void;
  private _generateTagsN: (handle: number, textPtr: number, size: number) => number;
  private _generateTagsWithOptionsN: (
    handle: number,
    textPtr: number,
    size: number,
    optionsPtr: number,
  ) => number;
  private _tagsFree: (tagsPtr: number) => void;
  private _loadUserDictCount: (handle: number, dataPtr: number, size: number) => number;
  private _loadBinaryDict: (handle: number, dataPtr: number, size: number) => number;
  private _clearUserDictionaries: (handle: number) => number;
  private _hasCoreDictionary: (handle: number) => number;
  private _version: () => number;
  private _lastError: () => number;
  private _lastErrorCode: () => number;
  private _conjugationTypeLabel: (code: number) => number;
  private _extendedPosLabel: (code: number) => number;
  private _conjugationFormLabel: (code: number) => number;
  private _posLabel: (code: number) => number;
  private readonly _posLabels = new Map<number, string>();
  private readonly _conjugationTypeLabels = new Map<number, string | null>();
  private readonly _conjugationFormLabels = new Map<number, string | null>();
  private readonly _extendedPosLabels = new Map<number, string>();
  private _dictionaryWarningCount: (handle: number) => number;
  private _dictionaryWarning: (handle: number, index: number) => number;
  private layouts = C_LAYOUTS;
  private unregisterToken = {};

  private constructor(module: EmscriptenModule, handle: number) {
    this.module = module;
    this.handle = handle;
    this.cleanupRef = { module, handle };
    registry.register(this, this.cleanupRef, this.unregisterToken);

    this._analyzeN = module._suzume_analyze_n;
    this._setMode = module._suzume_set_mode;
    this._mode = module._suzume_mode;
    this._resultFree = module._suzume_result_free;
    this._generateTagsN = module._suzume_generate_tags_n;
    this._generateTagsWithOptionsN = module._suzume_generate_tags_with_options_n;
    this._tagsFree = module._suzume_tags_free;
    this._loadUserDictCount = module._suzume_load_user_dict_count;
    this._loadBinaryDict = module._suzume_load_binary_dict;
    this._clearUserDictionaries = module._suzume_clear_user_dictionaries;
    this._hasCoreDictionary = module._suzume_has_core_dictionary;
    this._version = module._suzume_version;
    this._lastError = module._suzume_last_error;
    this._lastErrorCode = module._suzume_last_error_code;
    this._conjugationTypeLabel = module._suzume_conjugation_type_label;
    this._extendedPosLabel = module._suzume_extended_pos_label;
    this._conjugationFormLabel = module._suzume_conjugation_form_label;
    this._posLabel = module._suzume_pos_label;
    this._dictionaryWarningCount = module._suzume_dictionary_warning_count;
    this._dictionaryWarning = module._suzume_dictionary_warning;
  }

  /**
   * Create a new Suzume instance
   *
   * @param options - Optional configuration options
   * @returns Promise resolving to Suzume instance
   */
  static async create(options?: SuzumeOptions & { wasmPath?: string }): Promise<Suzume> {
    const wasmPath = options?.wasmPath;
    const module = await instantiateModule(wasmPath, options?.freshWasmModule === true);

    let handle: number;

    if (
      options &&
      (options.preserveVu !== undefined ||
        options.preserveCase !== undefined ||
        options.preserveSymbols !== undefined ||
        options.mode !== undefined ||
        options.lemmatize !== undefined ||
        options.mergeCompounds !== undefined ||
        options.skipUserDictionary !== undefined ||
        options.skipCoreDictionary !== undefined ||
        options.skipEnvConfig !== undefined ||
        options.reportScorerConfig !== undefined ||
        options.scorerOptions !== undefined)
    ) {
      // Create with options
      const layout = C_LAYOUTS.extendedOptions;
      const OPTIONS_SIZE = layout.size;
      const optionsPtr = module._malloc(OPTIONS_SIZE);
      let scorerOptionsPtr = 0;

      try {
        // _malloc hands back uninitialized heap, so seed the struct with the C
        // defaults before overriding fields. Every field below is written today,
        // but a field added to the C struct would otherwise be read as garbage.
        module._suzume_init_extended_options(optionsPtr);

        const heap = new Uint8Array(module.HEAPU32.buffer);
        const selectedMode = options.mode ?? EXTENDED_OPTION_DEFAULTS.mode;
        const modeValue = ANALYSIS_MODE_CODES[selectedMode];
        if (modeValue === undefined) {
          throw new Error(`Invalid Suzume mode: ${String(options.mode)}`);
        }
        heap[optionsPtr + layout.preserveVu] =
          (options.preserveVu ?? EXTENDED_OPTION_DEFAULTS.preserveVu) ? 1 : 0;
        heap[optionsPtr + layout.preserveCase] =
          (options.preserveCase ?? EXTENDED_OPTION_DEFAULTS.preserveCase) ? 1 : 0;
        heap[optionsPtr + layout.preserveSymbols] =
          (options.preserveSymbols ?? EXTENDED_OPTION_DEFAULTS.preserveSymbols) ? 1 : 0;
        heap[optionsPtr + layout.mode] = modeValue;
        heap[optionsPtr + layout.lemmatize] =
          (options.lemmatize ?? EXTENDED_OPTION_DEFAULTS.lemmatize) ? 1 : 0;
        heap[optionsPtr + layout.mergeCompounds] =
          (options.mergeCompounds ?? EXTENDED_OPTION_DEFAULTS.mergeCompounds) ? 1 : 0;
        heap[optionsPtr + layout.skipUserDictionary] =
          (options.skipUserDictionary ?? EXTENDED_OPTION_DEFAULTS.skipUserDictionary) ? 1 : 0;
        heap[optionsPtr + layout.skipCoreDictionary] =
          (options.skipCoreDictionary ?? EXTENDED_OPTION_DEFAULTS.skipCoreDictionary) ? 1 : 0;
        heap[optionsPtr + layout.skipEnvConfig] =
          (options.skipEnvConfig ?? EXTENDED_OPTION_DEFAULTS.skipEnvConfig) ? 1 : 0;
        heap[optionsPtr + layout.reportScorerConfig] =
          (options.reportScorerConfig ?? EXTENDED_OPTION_DEFAULTS.reportScorerConfig) ? 1 : 0;
        if (options.scorerOptions !== undefined) {
          const scorerJson =
            typeof options.scorerOptions === 'string'
              ? options.scorerOptions
              : JSON.stringify(options.scorerOptions);
          const scorerBytes = module.lengthBytesUTF8(scorerJson) + 1;
          scorerOptionsPtr = module._malloc(scorerBytes);
          module.stringToUTF8(scorerJson, scorerOptionsPtr, scorerBytes);
          module.HEAPU32[(optionsPtr + layout.scorerOptionsJson) >> 2] = scorerOptionsPtr;
        }

        handle = module._suzume_create_with_extended_options(optionsPtr);
      } finally {
        if (scorerOptionsPtr !== 0) {
          module._free(scorerOptionsPtr);
        }
        module._free(optionsPtr);
      }
    } else {
      // Create with default options
      handle = module._suzume_create();
    }

    if (handle === 0) {
      const message = module.UTF8ToString(module._suzume_last_error());
      throw new SuzumeError(
        message
          ? `Failed to create Suzume instance: ${message}`
          : 'Failed to create Suzume instance',
        module._suzume_last_error_code() as ErrorCode,
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
    return this.analyzeWithNormalizedText(text).morphemes;
  }

  /** Current analysis mode for this instance. */
  get mode(): AnalysisMode {
    this.ensureAlive();
    const mode = ANALYSIS_MODE_NAMES[this._mode(this.handle)];
    if (mode === undefined) {
      throw new SuzumeError(
        `Suzume mode query failed: ${this.lastError || 'unknown error'}`,
        this.lastErrorCode,
      );
    }
    return mode;
  }

  /** Change analysis mode without reloading dictionaries. */
  set mode(value: AnalysisMode) {
    this.ensureAlive();
    const mode = ANALYSIS_MODE_CODES[value];
    if (mode === undefined) {
      throw new Error(`Invalid Suzume mode: ${String(value)}`);
    }
    if (this._setMode(this.handle, mode) !== 1) {
      throw new SuzumeError(
        `Suzume mode change failed: ${this.lastError || 'unknown error'}`,
        this.lastErrorCode,
      );
    }
  }

  /**
   * Analyze text and return the exact normalized text used for offsets.
   */
  analyzeWithNormalizedText(text: string): AnalysisResult {
    this.ensureAlive();

    return this.withUtf8String(text, (textPtr, textBytes) => {
      const resultPtr = this._analyzeN(this.handle, textPtr, textBytes - 1);

      if (resultPtr === 0) {
        throw new SuzumeError(
          `Suzume analyze failed: ${this.lastError || 'unknown error'}`,
          this.lastErrorCode,
        );
      }

      try {
        return this.parseResult(resultPtr);
      } finally {
        this._resultFree(resultPtr);
      }
    });
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

    return this.withUtf8String(text, (textPtr, textBytes) => {
      if (options) {
        const posFilter = resolveTagPosFilter(options);

        const optionsPtr = this.module._malloc(this.layouts.tagOptions.size);

        try {
          // Same reason as create(): seed the C defaults into freshly malloc'd
          // memory, including the struct padding the field writes never touch.
          this.module._suzume_init_tag_options(optionsPtr);

          const heapU32 = this.module.HEAPU32;
          const heapU8 = new Uint8Array(heapU32.buffer);
          const layout = this.layouts.tagOptions;

          heapU8[optionsPtr + layout.posFilter] = posFilter & 0xff;
          heapU8[optionsPtr + layout.excludeBasic] =
            (options.excludeBasic ?? TAG_OPTION_DEFAULTS.excludeBasic) ? 1 : 0;
          heapU8[optionsPtr + layout.useLemma] =
            (options.useLemma ?? TAG_OPTION_DEFAULTS.useLemma) ? 1 : 0;
          heapU32[(optionsPtr + layout.minLength) >> 2] =
            options.minLength ?? TAG_OPTION_DEFAULTS.minLength;
          heapU32[(optionsPtr + layout.maxTags) >> 2] =
            options.maxTags ?? TAG_OPTION_DEFAULTS.maxTags;
          heapU8[optionsPtr + layout.excludeParticles] =
            (options.excludeParticles ?? TAG_OPTION_DEFAULTS.excludeParticles) ? 1 : 0;
          heapU8[optionsPtr + layout.excludeAuxiliaries] =
            (options.excludeAuxiliaries ?? TAG_OPTION_DEFAULTS.excludeAuxiliaries) ? 1 : 0;
          heapU8[optionsPtr + layout.excludeFormalNouns] =
            (options.excludeFormalNouns ?? TAG_OPTION_DEFAULTS.excludeFormalNouns) ? 1 : 0;
          heapU8[optionsPtr + layout.excludeLowInfo] =
            (options.excludeLowInfo ?? TAG_OPTION_DEFAULTS.excludeLowInfo) ? 1 : 0;
          heapU8[optionsPtr + layout.removeDuplicates] =
            (options.removeDuplicates ?? TAG_OPTION_DEFAULTS.removeDuplicates) ? 1 : 0;
          return this.consumeTags(
            this._generateTagsWithOptionsN(this.handle, textPtr, textBytes - 1, optionsPtr),
          );
        } finally {
          this.module._free(optionsPtr);
        }
      }

      return this.consumeTags(this._generateTagsN(this.handle, textPtr, textBytes - 1));
    });
  }

  /**
   * Load user dictionary from string data
   *
   * @param data - Dictionary data in current TSV format (legacy CSV is also accepted)
   * @returns true on success
   */
  loadUserDictionary(data: string): boolean {
    return this.loadUserDictionaryCount(data) > 0;
  }

  /**
   * Load user dictionary and return the number of installed expanded entries.
   */
  loadUserDictionaryCount(data: string): number {
    this.ensureAlive();

    return this.withUtf8String(data, (dataPtr, dataBytes) =>
      this._loadUserDictCount(this.handle, dataPtr, dataBytes - 1),
    );
  }

  /**
   * Load user dictionary from string data, throwing with C API details on failure.
   *
   * @param data - Dictionary data in current TSV format (legacy CSV is also accepted)
   */
  loadUserDictionaryOrThrow(data: string): void {
    if (!this.loadUserDictionary(data)) {
      throw new SuzumeError(
        `Suzume user dictionary load failed: ${this.lastError || 'unknown error'}`,
        this.lastErrorCode,
      );
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
      const heapU32 = this.module.HEAPU32;
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
      throw new SuzumeError(
        `Suzume binary dictionary load failed: ${this.lastError || 'unknown error'}`,
        this.lastErrorCode,
      );
    }
  }

  /**
   * Remove caller-loaded dictionaries while retaining the bundled user dictionary.
   */
  clearUserDictionaries(): void {
    this.ensureAlive();
    if (this._clearUserDictionaries(this.handle) !== 1) {
      throw new SuzumeError(
        `Suzume dictionary clear failed: ${this.lastError || 'unknown error'}`,
        this.lastErrorCode,
      );
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
   * Last C API error for this thread, or empty string if the last C API call succeeded.
   */
  get lastError(): string {
    return this.module.UTF8ToString(this._lastError());
  }

  /** Stable native error category for the last failed C ABI call. */
  get lastErrorCode(): ErrorCode {
    return this._lastErrorCode() as ErrorCode;
  }

  /** Current WebAssembly linear-memory size in bytes. */
  wasmMemoryBytes(): number {
    this.ensureAlive();
    return this.module.HEAPU32.buffer.byteLength;
  }

  /** Dictionary-loading, parsing, and scorer-configuration diagnostics. */
  get dictionaryWarnings(): string[] {
    this.ensureAlive();
    const count = this._dictionaryWarningCount(this.handle);
    const warnings: string[] = [];
    for (let idx = 0; idx < count; idx++) {
      const warningPtr = this._dictionaryWarning(this.handle, idx);
      if (warningPtr !== 0) {
        warnings.push(this.module.UTF8ToString(warningPtr));
      }
    }
    return warnings;
  }

  /** Whether the bundled L2 core dictionary is loaded. */
  get hasCoreDictionary(): boolean {
    this.ensureAlive();
    return this._hasCoreDictionary(this.handle) === 1;
  }

  /**
   * Destroy this analyzer handle. The shared WASM runtime remains cached for
   * other and future Suzume instances.
   */
  destroy(): void {
    if (this.handle !== 0) {
      registry.unregister(this.unregisterToken);
      this.module._suzume_destroy(this.handle);
      this.handle = 0;
      this.cleanupRef.handle = 0;
    }
  }

  private ensureAlive(): void {
    if (this.handle === 0) {
      throw new Error('Suzume instance has been destroyed');
    }
  }

  private withUtf8String<T>(
    value: string,
    operation: (pointer: number, byteLength: number) => T,
  ): T {
    for (let idx = 0; idx < value.length; idx++) {
      const codeUnit = value.charCodeAt(idx);
      if (codeUnit >= 0xd800 && codeUnit <= 0xdbff) {
        const next = value.charCodeAt(idx + 1);
        if (next < 0xdc00 || next > 0xdfff) {
          throw new SuzumeError(
            'Input contains an unpaired UTF-16 surrogate',
            ErrorCode.InvalidUtf8,
          );
        }
        idx++;
      } else if (codeUnit >= 0xdc00 && codeUnit <= 0xdfff) {
        throw new SuzumeError('Input contains an unpaired UTF-16 surrogate', ErrorCode.InvalidUtf8);
      }
    }
    const byteLength = this.module.lengthBytesUTF8(value) + 1;
    const pointer = this.module._malloc(byteLength);
    try {
      this.module.stringToUTF8(value, pointer, byteLength);
      return operation(pointer, byteLength);
    } finally {
      this.module._free(pointer);
    }
  }

  private consumeTags(tagsPtr: number): Tag[] {
    if (tagsPtr === 0) {
      throw new SuzumeError(
        `Suzume tag generation failed: ${this.lastError || 'unknown error'}`,
        this.lastErrorCode,
      );
    }
    try {
      return this.parseTags(tagsPtr);
    } finally {
      this._tagsFree(tagsPtr);
    }
  }

  private conjugationTypeLabel(code: number): string | null {
    if (this._conjugationTypeLabels.has(code)) {
      return this._conjugationTypeLabels.get(code) ?? null;
    }
    const labelPtr = this._conjugationTypeLabel(code);
    const label = labelPtr === 0 ? null : this.module.UTF8ToString(labelPtr);
    this._conjugationTypeLabels.set(code, label);
    return label;
  }

  // Parse suzume_result_t structure from WASM memory
  private parseResult(resultPtr: number): AnalysisResult {
    return decodeAnalysisResult(
      this.module,
      resultPtr,
      (code) => this.conjugationTypeLabel(code),
      (code) => this.conjugationFormLabel(code),
      (code) => this.extendedPosLabel(code),
      (code) => this.posLabel(code),
    );
  }

  // Parse suzume_tags_t structure from WASM memory
  private parseTags(tagsPtr: number): Tag[] {
    return decodeTags(this.module, tagsPtr, (code) => this.posLabel(code));
  }

  private posLabel(code: number): string {
    const cached = this._posLabels.get(code);
    if (cached !== undefined) {
      return cached;
    }
    const labelPtr = this._posLabel(code);
    const label = labelPtr === 0 ? 'OTHER' : this.module.UTF8ToString(labelPtr);
    this._posLabels.set(code, label);
    return label;
  }

  private conjugationFormLabel(code: number): string | null {
    if (this._conjugationFormLabels.has(code)) {
      return this._conjugationFormLabels.get(code) ?? null;
    }
    const labelPtr = this._conjugationFormLabel(code);
    const label = labelPtr === 0 ? null : this.module.UTF8ToString(labelPtr);
    this._conjugationFormLabels.set(code, label);
    return label;
  }

  private extendedPosLabel(code: number): string {
    const cached = this._extendedPosLabels.get(code);
    if (cached !== undefined) {
      return cached;
    }
    const labelPtr = this._extendedPosLabel(code);
    const label = labelPtr === 0 ? 'UNKNOWN' : this.module.UTF8ToString(labelPtr);
    this._extendedPosLabels.set(code, label);
    return label;
  }
}

// Default export
export default Suzume;

/** Return the package version without creating an analyzer handle. */
export async function version(options?: {
  wasmPath?: string;
  freshWasmModule?: boolean;
}): Promise<string> {
  const module = await instantiateModule(options?.wasmPath, options?.freshWasmModule === true);
  return module.UTF8ToString(module._suzume_version());
}
