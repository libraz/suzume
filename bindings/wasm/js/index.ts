/**
 * Suzume - Lightweight Japanese tokenizer
 *
 * @example
 * ```typescript
 * import { Suzume } from '@libraz/suzume';
 *
 * const suzume = await Suzume.create();
 * const morphemes = suzume.analyze('すもももももももものうち');
 * console.log(morphemes);
 * ```
 */

import { C_LAYOUTS } from './abi_layout.js';
import { decodeAnalysisResult, decodeTags, heapBytes } from './decode.js';
import type { EmscriptenModule, EmscriptenModuleOptions } from './suzume.js';

/**
 * Error categories carried by {@link SuzumeError.code}. Values 0-7 mirror the
 * C ABI's `SUZUME_ERROR_*`; `Unknown` (255) is reserved for bindings and is
 * what any other value maps to.
 */
export const ErrorCode = Object.freeze({
  Success: 0,
  InvalidUtf8: 1,
  DictionaryLoadFailed: 2,
  FileNotFound: 3,
  Parse: 4,
  OutOfMemory: 5,
  InvalidInput: 6,
  Internal: 7,
  Unknown: 255,
} as const);

/** One of the {@link ErrorCode} values. */
export type ErrorCode = (typeof ErrorCode)[keyof typeof ErrorCode];

/**
 * The call that raised a {@link SuzumeError}. Tokens are snake_case and shared
 * with the other bindings; a setter carries its getter's token.
 */
export type SuzumeOperation =
  | 'create'
  | 'analyze'
  | 'generate_tags'
  | 'load_user_dictionary'
  | 'load_binary_dictionary'
  | 'clear_user_dictionaries'
  | 'mode'
  | 'version'
  | 'dictionary_warnings'
  | 'has_core_dictionary'
  | 'wasm_memory_bytes';

const ERROR_CODE_VALUES: ReadonlySet<number> = new Set(Object.values(ErrorCode));

/**
 * Every error this package throws: invalid input (`code` InvalidInput, a
 * message naming the option and the accepted values), use after `destroy()`,
 * and native failures, whose message is the library's own text.
 */
export class SuzumeError extends Error {
  readonly code: ErrorCode;
  /** The call that raised the error; undefined only on an error constructed without one. */
  readonly operation?: SuzumeOperation;
  /** The wrapped non-native error (module load, scorer-options serialization). */
  readonly cause?: unknown;

  /**
   * @param code - any number; values outside {@link ErrorCode} become `ErrorCode.Unknown`
   */
  constructor(
    message: string,
    code: number = ErrorCode.Internal,
    options?: { operation?: SuzumeOperation; cause?: unknown },
  ) {
    super(message);
    this.name = 'SuzumeError';
    this.code = ERROR_CODE_VALUES.has(code) ? (code as ErrorCode) : ErrorCode.Unknown;
    if (options?.operation !== undefined) {
      this.operation = options.operation;
    }
    if (options !== undefined && 'cause' in options) {
      this.cause = options.cause;
    }
  }
}

const RELEASED_MESSAGE = 'Suzume instance has been released';
const MAX_SIZE_OPTION = 0xffffffff;

/** Render a rejected value the same way in every binding. */
function renderValue(value: unknown): string {
  if (typeof value === 'string') {
    return JSON.stringify(value);
  }
  if (typeof value === 'number' || typeof value === 'boolean' || value === undefined) {
    return String(value);
  }
  if (value === null) {
    return 'null';
  }
  if (Array.isArray(value)) {
    return 'array';
  }
  if (typeof value === 'object') {
    return 'object';
  }
  return 'value';
}

function invalidInput(operation: SuzumeOperation, message: string, cause?: unknown): SuzumeError {
  return new SuzumeError(
    message,
    ErrorCode.InvalidInput,
    cause === undefined ? { operation } : { operation, cause },
  );
}

/** The snake_case spelling every binding uses in messages (`preserveVu` -> `preserve_vu`). */
function messageName(name: string): string {
  return name.replace(/[A-Z]/g, (letter) => `_${letter.toLowerCase()}`);
}

const modulePromises = new Map<string, Promise<EmscriptenModule>>();

async function instantiateModule(
  wasmPath: string | undefined,
  freshWasmModule: boolean,
): Promise<EmscriptenModule> {
  const createModule = await import('./suzume.js');
  const moduleOptions: EmscriptenModuleOptions = {};
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
  const pending = createModule.default(moduleOptions);
  modulePromises.set(key, pending);
  try {
    return await pending;
  } catch (error) {
    modulePromises.delete(key);
    throw error;
  }
}

/** Analysis mode: `normal`, `search` (finer split for indexing) or `split`. */
export type AnalysisMode = 'normal' | 'search' | 'split';

/**
 * Options for creating a Suzume instance. Unknown keys are ignored at runtime;
 * TypeScript rejects them at compile time.
 */
export interface SuzumeOptions {
  /** Preserve ヴ (don't normalize to ビ etc.), default: true */
  preserveVu?: boolean;
  /** Preserve case (don't lowercase ASCII), default: true */
  preserveCase?: boolean;
  /** Keep punctuation-like SYMBOL tokens; emoji and content symbols stay OTHER either way. Default: false */
  preserveSymbols?: boolean;
  /** Analysis mode, default: normal */
  mode?: AnalysisMode;
  /** Correct base forms after analysis; conjugation and POS annotations are always computed. Default: true */
  lemmatize?: boolean;
  /** Merge consecutive noun compounds, default: false */
  mergeCompounds?: boolean;
  /** Skip automatic loading of the bundled user dictionary, default: false */
  skipBundledUserDictionary?: boolean;
  /** Skip automatic loading of the bundled core dictionary, default: false */
  skipCoreDictionary?: boolean;
  /**
   * Ignore native scorer configuration environment variables, default: false.
   * The WebAssembly build reads no environment, so this has no effect here.
   */
  skipEnvConfig?: boolean;
  /** Add scorer configuration diagnostics to dictionaryWarnings, default: false */
  reportScorerConfig?: boolean;
  /** Final-priority scorer override JSON, or an object serialized to JSON; null means none */
  scorerOptions?: string | Record<string, unknown> | null;
}

/** How the WebAssembly runtime is located and shared; accepted by `Suzume.create` and `version`. */
export interface RuntimeOptions {
  /** URL or path of `suzume.wasm`, for bundlers that move it */
  wasmPath?: string;
  /** Create an isolated WASM runtime instead of sharing the cached module, default: false */
  freshWasmModule?: boolean;
}

const ANALYSIS_MODE_CODES: Readonly<Record<AnalysisMode, number>> = {
  normal: 0,
  search: 1,
  split: 2,
};

const ANALYSIS_MODE_NAMES: Readonly<Record<number, AnalysisMode>> = Object.fromEntries(
  (Object.keys(ANALYSIS_MODE_CODES) as AnalysisMode[]).map((name) => [
    ANALYSIS_MODE_CODES[name],
    name,
  ]),
);

// Keep the binding's public defaults explicit so CI can compare them with the
// C ABI initializer. Values are consumed below rather than duplicated there.
const EXTENDED_OPTION_DEFAULTS = {
  preserveVu: true,
  preserveCase: true,
  preserveSymbols: false,
  mode: 'normal',
  lemmatize: true,
  mergeCompounds: false,
  skipBundledUserDictionary: false,
  skipCoreDictionary: false,
  skipEnvConfig: false,
  reportScorerConfig: false,
  scorerOptions: null,
  dataDirectory: null,
} as const;

/** Look up a caller-supplied name in a table, ignoring inherited keys such as `constructor`. */
function ownEntry<Value>(table: Readonly<Record<string, Value>>, key: string): Value | undefined {
  return Object.getOwnPropertyDescriptor(table, key)?.value;
}

/** Validate a tag option that the C struct stores as a 32-bit `size_t`; -0 becomes 0. */
function sizeOption(operation: SuzumeOperation, name: string, value: unknown): number {
  if (
    typeof value !== 'number' ||
    !Number.isInteger(value) ||
    value < 0 ||
    value > MAX_SIZE_OPTION
  ) {
    throw invalidInput(
      operation,
      `invalid ${messageName(name)}: ${renderValue(value)} (expected an integer from 0 to ${MAX_SIZE_OPTION})`,
    );
  }
  return value + 0;
}

function modeCode(operation: SuzumeOperation, mode: unknown): number {
  const code = typeof mode === 'string' ? ownEntry(ANALYSIS_MODE_CODES, mode) : undefined;
  if (code === undefined) {
    throw invalidInput(
      operation,
      `invalid mode: ${renderValue(mode)} (expected one of: ${Object.keys(ANALYSIS_MODE_CODES).join(', ')})`,
    );
  }
  return code;
}

function booleanOption(operation: SuzumeOperation, name: string, value: unknown): void {
  if (value !== undefined && typeof value !== 'boolean') {
    throw invalidInput(
      operation,
      `invalid ${messageName(name)}: ${renderValue(value)} (expected a boolean)`,
    );
  }
}

function stringArgument(operation: SuzumeOperation, name: string, value: unknown): string {
  if (typeof value !== 'string') {
    throw invalidInput(operation, `invalid ${name}: ${renderValue(value)} (expected a string)`);
  }
  return value;
}

/** Serialize `scorerOptions` to the JSON text the C struct carries, or null when absent. */
function scorerOptionsJson(value: unknown): string | null {
  if (value === undefined || value === null) {
    return null;
  }
  let json: string;
  if (typeof value === 'string') {
    json = value;
  } else {
    try {
      // JSON.stringify escapes a lone surrogate as ASCII, so check each key and string first.
      json = JSON.stringify(value, (key, member: unknown) => {
        assertWellFormedUtf16('create', 'scorer_options', key);
        if (typeof member === 'string') {
          assertWellFormedUtf16('create', 'scorer_options', member);
        }
        return member;
      });
    } catch (error) {
      if (error instanceof SuzumeError) {
        throw error;
      }
      throw invalidInput('create', 'invalid scorer_options: not serializable as JSON', error);
    }
  }
  assertWellFormedUtf16('create', 'scorer_options', json);
  if (json.includes('\0')) {
    throw invalidInput('create', 'invalid scorer_options: contains a NUL character');
  }
  return json;
}

const EXTENDED_BOOL_FIELDS = [
  'preserveVu',
  'preserveCase',
  'preserveSymbols',
  'lemmatize',
  'mergeCompounds',
  'skipBundledUserDictionary',
  'skipCoreDictionary',
  'skipEnvConfig',
  'reportScorerConfig',
] as const;

/** A part-of-speech label as {@link Morpheme.pos} and {@link Tag.pos} print it. */
export type PosLabel =
  | 'NOUN'
  | 'PRON'
  | 'VERB'
  | 'ADJ'
  | 'ADV'
  | 'PARTICLE'
  | 'AUX'
  | 'CONJ'
  | 'DET'
  | 'PREFIX'
  | 'SUFFIX'
  | 'INTJ'
  | 'SYMBOL'
  | 'OTHER';

/**
 * Morpheme - A single token produced by analysis
 */
export interface Morpheme {
  /** Surface form (as it appears in the text) */
  readonly surface: string;
  /** Part of speech (English label) */
  readonly pos: PosLabel;
  /** Base (dictionary) form */
  readonly baseForm: string;
  /** Part of speech (Japanese) */
  readonly posJa: string;
  /** Conjugation type (Japanese, e.g., "一段", "五段・カ行") - null for non-conjugating words */
  readonly conjType: string | null;
  /** Conjugation form (Japanese, e.g., "連用形", "終止形") - null for non-conjugating words */
  readonly conjForm: string | null;
  /** Stable extended POS code (e.g., "VERB_連用", "AUX_過去") */
  readonly extendedPos: string;
  /** Start Unicode code-point offset in normalized text */
  readonly start: number;
  /** End Unicode code-point offset in normalized text */
  readonly end: number;
  /** Start JavaScript UTF-16 offset, suitable for normalizedText.slice() */
  readonly startUtf16: number;
  /** End JavaScript UTF-16 offset, suitable for normalizedText.slice() */
  readonly endUtf16: number;
  /** True if matched from a user dictionary */
  readonly isUserDict: boolean;
  /** True if the morpheme is a formal noun */
  readonly isFormalNoun: boolean;
  /** True if the morpheme is low information for tag generation */
  readonly isLowInfo: boolean;
  /** True if generated as an unknown word */
  readonly isUnknown: boolean;
  /** True if matched from any dictionary */
  readonly isFromDictionary: boolean;
  /** Candidate score/cost */
  readonly score: number;
}

/** Normalized input together with its morphemes. */
export interface AnalysisResult {
  readonly normalizedText: string;
  readonly morphemes: readonly Morpheme[];
}

/**
 * Tag entry with POS information
 */
export interface Tag {
  /** Tag text (surface or base form) */
  readonly tag: string;
  /** Part of speech (English label) */
  readonly pos: PosLabel;
}

/** A part of speech `posFilter` can select: the labels {@link Tag.pos} prints, case-sensitive. */
export type TagPosLabel = 'NOUN' | 'VERB' | 'ADJ' | 'ADV' | 'PARTICLE' | 'AUX';

/**
 * Options for tag generation
 */
export interface TagOptions {
  /**
   * Parts of speech to keep: one label or an array of labels. `NOUN` also
   * keeps pronouns (`PRON`). Naming `PARTICLE` or `AUX` keeps those tags
   * although `excludeParticles` and `excludeAuxiliaries` default to true.
   * Absent, null or an empty array keeps every part of speech.
   */
  posFilter?: TagPosLabel | readonly TagPosLabel[] | null;
  /** Exclude basic/common words whose base form is hiragana only (default: false) */
  excludeBasic?: boolean;
  /** Use the base form instead of the surface form (default: true) */
  useBaseForm?: boolean;
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
  useBaseForm: true,
  minLength: 2,
  maxTags: 0,
  excludeParticles: true,
  excludeAuxiliaries: true,
  excludeFormalNouns: true,
  excludeLowInfo: true,
  removeDuplicates: true,
} as const;

const TAG_BOOL_FIELDS = [
  'excludeBasic',
  'useBaseForm',
  'excludeParticles',
  'excludeAuxiliaries',
  'excludeFormalNouns',
  'excludeLowInfo',
  'removeDuplicates',
] as const;

// Each name's native SUZUME_TAG_POS_* bit.
const TAG_POS_FILTER_BITS: Readonly<Record<TagPosLabel, number>> = {
  NOUN: 1,
  VERB: 2,
  ADJ: 4,
  ADV: 8,
  PARTICLE: 16,
  AUX: 32,
};

/** Write each (already validated) boolean field as one heap byte, falling back to its default when unset. */
function writeBoolFields<Field extends string>(
  heap: Uint8Array,
  structPtr: number,
  layout: Readonly<Record<NoInfer<Field>, number>>,
  fields: readonly Field[],
  options: Readonly<Partial<Record<NoInfer<Field>, boolean>>>,
  defaults: Readonly<Record<NoInfer<Field>, boolean>>,
): void {
  for (const field of fields) {
    heap[structPtr + layout[field]] = (options[field] ?? defaults[field]) ? 1 : 0;
  }
}

/** Read a native C string; a NULL pointer and an empty string both mean "missing". */
function nativeString(module: EmscriptenModule, pointer: number): string | undefined {
  if (pointer === 0) {
    return undefined;
  }
  const value = module.UTF8ToString(pointer);
  return value === '' ? undefined : value;
}

/**
 * Build the error for a native call that reported failure: the library's own
 * text and code, or `<operation> failed` when it left no text. A failure whose
 * code reads Success is the library breaking its contract, i.e. Internal.
 */
function nativeError(module: EmscriptenModule, operation: SuzumeOperation): SuzumeError {
  const code = module._suzume_last_error_code();
  const fallback = `${operation} failed`;
  if (code === ErrorCode.Success) {
    return new SuzumeError(fallback, ErrorCode.Internal, { operation });
  }
  const text = nativeString(module, module._suzume_last_error());
  return new SuzumeError(text ?? fallback, code, { operation });
}

function readVersion(module: EmscriptenModule): string {
  return module.UTF8ToString(module._suzume_version());
}

/** Instantiate the WASM runtime, reporting a load failure as an Internal error of `operation`. */
async function loadModule(
  operation: SuzumeOperation,
  wasmPath: string | undefined,
  freshWasmModule: boolean,
): Promise<EmscriptenModule> {
  try {
    return await instantiateModule(wasmPath, freshWasmModule);
  } catch (error) {
    const text = error instanceof Error ? error.message : '';
    return Promise.reject(
      new SuzumeError(text || `${operation} failed`, ErrorCode.Internal, {
        operation,
        cause: error,
      }),
    );
  }
}

/** The native bitmask for a label or an array of labels; absent or null selects every POS. */
function tagPosFilter(value: unknown): number {
  if (value === undefined || value === null) {
    return 0;
  }
  const names: readonly unknown[] = Array.isArray(value) ? value : [value];
  let filter = 0;
  for (const name of names) {
    const bit = typeof name === 'string' ? ownEntry(TAG_POS_FILTER_BITS, name) : undefined;
    if (bit === undefined) {
      throw invalidInput(
        'generate_tags',
        `invalid pos_filter: ${renderValue(name)} (expected one of: ${Object.keys(TAG_POS_FILTER_BITS).join(', ')})`,
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

/** Rejects an unpaired surrogate, which has no UTF-8 encoding, as invalid input named `name`. */
function assertWellFormedUtf16(operation: SuzumeOperation, name: string, value: string): void {
  for (let idx = 0; idx < value.length; idx++) {
    const codeUnit = value.charCodeAt(idx);
    if (codeUnit < 0xd800 || codeUnit > 0xdfff) {
      continue;
    }
    if (codeUnit <= 0xdbff) {
      // charCodeAt past the end is NaN, which no range test admits.
      const next = value.charCodeAt(idx + 1);
      if (next >= 0xdc00 && next <= 0xdfff) {
        idx++;
        continue;
      }
    }
    throw invalidInput(operation, `invalid ${name}: contains an unpaired surrogate`);
  }
}

/**
 * Copy a well-formed string into freshly malloc'd NUL-terminated UTF-8; the
 * caller frees `pointer`. `payloadByteLength` excludes the trailing NUL.
 */
function allocUtf8(
  module: EmscriptenModule,
  value: string,
): { pointer: number; payloadByteLength: number } {
  const payloadByteLength = module.lengthBytesUTF8(value);
  const bufferSize = payloadByteLength + 1;
  const pointer = module._malloc(bufferSize);
  try {
    module.stringToUTF8(value, pointer, bufferSize);
  } catch (error) {
    module._free(pointer);
    throw error;
  }
  return { pointer, payloadByteLength };
}

/**
 * Suzume instance for Japanese tokenization.
 *
 * Every call on an instance is synchronous and runs to completion, so one
 * instance can be shared freely within a JavaScript thread. A worker needs its
 * own instance, since a WebAssembly module belongs to the thread that loaded it.
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
  private readonly _posLabels = new Map<number, string>();
  private readonly _posJapaneseLabels = new Map<number, string>();
  private readonly _conjugationTypeLabels = new Map<number, string | null>();
  private readonly _conjugationFormLabels = new Map<number, string | null>();
  private readonly _extendedPosLabels = new Map<number, string>();
  private unregisterToken = {};

  private constructor(module: EmscriptenModule, handle: number) {
    this.module = module;
    this.handle = handle;
    this.cleanupRef = { module, handle };
    registry.register(this, this.cleanupRef, this.unregisterToken);
  }

  /**
   * Create a new Suzume instance
   *
   * @param options - Optional configuration options; unknown keys are ignored
   * @returns Promise resolving to Suzume instance
   * @throws {SuzumeError} InvalidInput for an invalid option (before the module
   *   loads), Internal when the WASM module fails to load, or the native code
   *   and text when the library rejects the configuration
   */
  static async create(options?: SuzumeOptions & RuntimeOptions): Promise<Suzume> {
    const modeValue = options?.mode === undefined ? EXTENDED_OPTION_DEFAULTS.mode : options.mode;
    const mode = modeCode('create', modeValue);
    for (const field of EXTENDED_BOOL_FIELDS) {
      booleanOption('create', field, options?.[field]);
    }
    const scorerJson = scorerOptionsJson(options?.scorerOptions);

    const wasmPath = options?.wasmPath;
    const module = await loadModule('create', wasmPath, options?.freshWasmModule === true);

    let handle: number;

    if (
      options &&
      (EXTENDED_BOOL_FIELDS.some((field) => options[field] !== undefined) ||
        options.mode !== undefined ||
        scorerJson !== null)
    ) {
      // Create with options
      const layout = C_LAYOUTS.options;
      const OPTIONS_SIZE = layout.size;
      const optionsPtr = module._malloc(OPTIONS_SIZE);
      let scorerOptionsPtr = 0;

      try {
        // _malloc hands back uninitialized heap, so seed the struct with the C
        // defaults before overriding fields. Every field below is written today,
        // but a field added to the C struct would otherwise be read as garbage.
        module._suzume_init_options(optionsPtr);

        const heap = heapBytes(module);
        heap[optionsPtr + layout.mode] = mode;
        writeBoolFields(
          heap,
          optionsPtr,
          layout,
          EXTENDED_BOOL_FIELDS,
          options,
          EXTENDED_OPTION_DEFAULTS,
        );
        if (scorerJson !== null) {
          scorerOptionsPtr = allocUtf8(module, scorerJson).pointer;
          module.HEAPU32[(optionsPtr + layout.scorerOptionsJson) >> 2] = scorerOptionsPtr;
        }

        handle = module._suzume_create_with_options(optionsPtr);
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
      throw nativeError(module, 'create');
    }

    return new Suzume(module, handle);
  }

  /**
   * Analyze Japanese text into morphemes
   *
   * @param text - Japanese text
   * @returns The morphemes, in text order
   * @throws {SuzumeError} InvalidInput for a non-string or an unpaired surrogate,
   *   or after {@link destroy}
   */
  analyze(text: string): readonly Morpheme[] {
    return this.analyzeWithNormalizedText(text).morphemes;
  }

  /**
   * Current analysis mode for this instance.
   *
   * @throws {SuzumeError} InvalidInput for a name other than normal, search or
   *   split, or after {@link destroy}
   */
  get mode(): AnalysisMode {
    this.ensureAlive('mode');
    const mode = ANALYSIS_MODE_NAMES[this.module._suzume_mode(this.handle)];
    if (mode === undefined) {
      throw nativeError(this.module, 'mode');
    }
    return mode;
  }

  /** Change analysis mode without reloading dictionaries. */
  set mode(value: AnalysisMode) {
    this.ensureAlive('mode');
    if (this.module._suzume_set_mode(this.handle, modeCode('mode', value)) !== 1) {
      throw nativeError(this.module, 'mode');
    }
  }

  /**
   * Analyze text and return the exact normalized text used for offsets.
   *
   * @throws {SuzumeError} as {@link analyze}
   */
  analyzeWithNormalizedText(text: string): AnalysisResult {
    this.ensureAlive('analyze');

    return this.withUtf8String('analyze', 'text', text, (textPtr, textByteLength) => {
      const resultPtr = this.module._suzume_analyze_n(this.handle, textPtr, textByteLength);

      if (resultPtr === 0) {
        throw nativeError(this.module, 'analyze');
      }

      try {
        return this.parseResult(resultPtr);
      } finally {
        this.module._suzume_result_free(resultPtr);
      }
    });
  }

  /**
   * Generate tags from Japanese text
   *
   * @param text - Japanese text
   * @param options - Optional tag generation options; unknown keys are ignored
   * @returns The tags with their POS labels
   * @throws {SuzumeError} InvalidInput for an invalid text or option, or after
   *   {@link destroy}
   */
  generateTags(text: string, options?: TagOptions): readonly Tag[] {
    this.ensureAlive('generate_tags');

    let posFilter = 0;
    let minLength: number = TAG_OPTION_DEFAULTS.minLength;
    let maxTags: number = TAG_OPTION_DEFAULTS.maxTags;
    if (options) {
      posFilter = tagPosFilter(options.posFilter);
      if (options.minLength !== undefined) {
        minLength = sizeOption('generate_tags', 'minLength', options.minLength);
      }
      if (options.maxTags !== undefined) {
        maxTags = sizeOption('generate_tags', 'maxTags', options.maxTags);
      }
      for (const field of TAG_BOOL_FIELDS) {
        booleanOption('generate_tags', field, options[field]);
      }
    }

    return this.withUtf8String('generate_tags', 'text', text, (textPtr, textByteLength) => {
      if (options) {
        const optionsPtr = this.module._malloc(C_LAYOUTS.tagOptions.size);

        try {
          // Same reason as create(): seed the C defaults into freshly malloc'd
          // memory, including the struct padding the field writes never touch.
          this.module._suzume_init_tag_options(optionsPtr);

          const heapU32 = this.module.HEAPU32;
          const heapU8 = heapBytes(this.module);
          const layout = C_LAYOUTS.tagOptions;

          heapU8[optionsPtr + layout.posFilter] = posFilter & 0xff;
          heapU32[(optionsPtr + layout.minLength) >> 2] = minLength;
          heapU32[(optionsPtr + layout.maxTags) >> 2] = maxTags;
          writeBoolFields(
            heapU8,
            optionsPtr,
            layout,
            TAG_BOOL_FIELDS,
            options,
            TAG_OPTION_DEFAULTS,
          );
          return this.consumeTags(
            this.module._suzume_generate_tags_with_options_n(
              this.handle,
              textPtr,
              textByteLength,
              optionsPtr,
            ),
          );
        } finally {
          this.module._free(optionsPtr);
        }
      }

      return this.consumeTags(
        this.module._suzume_generate_tags_n(this.handle, textPtr, textByteLength),
      );
    });
  }

  /**
   * Add entries from tab-separated dictionary text. Loads are additive; a
   * failed load leaves earlier dictionaries installed.
   *
   * @param data - Rows of `surface<TAB>POS[<TAB>conj_type][<TAB>base_form]`
   * @returns The number of installed entries (always at least 1)
   * @throws {SuzumeError} with the native code and text when the source cannot
   *   be loaded (an empty or entry-less source included), InvalidInput for a
   *   non-string, or after {@link destroy}
   */
  loadUserDictionary(data: string): number {
    this.ensureAlive('load_user_dictionary');

    const installed = this.withUtf8String(
      'load_user_dictionary',
      'dictionary',
      data,
      (dataPtr, dataByteLength) =>
        this.module._suzume_load_user_dictionary(this.handle, dataPtr, dataByteLength),
    );
    if (installed === 0) {
      throw nativeError(this.module, 'load_user_dictionary');
    }
    return installed;
  }

  /**
   * Add a compiled binary dictionary (.dic) as a user dictionary.
   *
   * @param data - Binary dictionary bytes
   * @returns The number of installed entries (always at least 1)
   * @throws {SuzumeError} DictionaryLoadFailed with the native text when the
   *   bytes cannot be loaded, InvalidInput for a non-Uint8Array, or after
   *   {@link destroy}
   */
  loadBinaryDictionary(data: Uint8Array): number {
    this.ensureAlive('load_binary_dictionary');
    if (!(data instanceof Uint8Array)) {
      throw invalidInput(
        'load_binary_dictionary',
        `invalid dictionary: ${renderValue(data)} (expected binary data)`,
      );
    }

    const dataPtr = this.module._malloc(data.byteLength);
    let installed: number;
    try {
      heapBytes(this.module).set(data, dataPtr);
      installed = this.module._suzume_load_binary_dictionary(this.handle, dataPtr, data.byteLength);
    } finally {
      this.module._free(dataPtr);
    }
    if (installed === 0) {
      throw nativeError(this.module, 'load_binary_dictionary');
    }
    return installed;
  }

  /**
   * Remove caller-loaded dictionaries while retaining the bundled user dictionary.
   *
   * @throws {SuzumeError} after {@link destroy}
   */
  clearUserDictionaries(): void {
    this.ensureAlive('clear_user_dictionaries');
    if (this.module._suzume_clear_user_dictionaries(this.handle) !== 1) {
      throw nativeError(this.module, 'clear_user_dictionaries');
    }
  }

  /**
   * Current WebAssembly linear-memory size in bytes.
   *
   * @throws {SuzumeError} after {@link destroy}
   */
  wasmMemoryBytes(): number {
    this.ensureAlive('wasm_memory_bytes');
    return this.module.HEAPU32.buffer.byteLength;
  }

  /**
   * Dictionary-loading, parsing, and scorer-configuration diagnostics; empty when there are none.
   *
   * @throws {SuzumeError} after {@link destroy}
   */
  get dictionaryWarnings(): readonly string[] {
    this.ensureAlive('dictionary_warnings');
    const count = this.module._suzume_dictionary_warning_count(this.handle);
    const warnings: string[] = [];
    for (let idx = 0; idx < count; idx++) {
      const warning = nativeString(
        this.module,
        this.module._suzume_dictionary_warning(this.handle, idx),
      );
      if (warning !== undefined) {
        warnings.push(warning);
      }
    }
    return warnings;
  }

  /**
   * Whether the bundled L2 core dictionary is loaded.
   *
   * @throws {SuzumeError} after {@link destroy}
   */
  get hasCoreDictionary(): boolean {
    this.ensureAlive('has_core_dictionary');
    return this.module._suzume_has_core_dictionary(this.handle) === 1;
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

  private ensureAlive(operation: SuzumeOperation): void {
    if (this.handle === 0) {
      throw new SuzumeError(RELEASED_MESSAGE, ErrorCode.InvalidInput, { operation });
    }
  }

  /** Validate `value` as the string argument `name`, then pass it to `body` as UTF-8. */
  private withUtf8String<T>(
    operation: SuzumeOperation,
    name: string,
    value: unknown,
    body: (pointer: number, byteLength: number) => T,
  ): T {
    const text = stringArgument(operation, name, value);
    assertWellFormedUtf16(operation, name, text);
    const { pointer, payloadByteLength } = allocUtf8(this.module, text);
    try {
      return body(pointer, payloadByteLength);
    } finally {
      this.module._free(pointer);
    }
  }

  private consumeTags(tagsPtr: number): readonly Tag[] {
    if (tagsPtr === 0) {
      throw nativeError(this.module, 'generate_tags');
    }
    try {
      return decodeTags(this.module, tagsPtr, (code) => this.posLabel(code));
    } finally {
      this.module._suzume_tags_free(tagsPtr);
    }
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
      (code) => this.posJapaneseLabel(code),
    );
  }

  private posLabel(code: number): PosLabel {
    // The native table holds exactly the PosLabel members, and 'OTHER' for any other code.
    return this.cachedLabel(
      this._posLabels,
      this.module._suzume_pos_label,
      code,
      'OTHER',
    ) as PosLabel;
  }

  private posJapaneseLabel(code: number): string {
    return this.cachedLabel(
      this._posJapaneseLabels,
      this.module._suzume_pos_label_ja,
      code,
      'その他',
    );
  }

  private conjugationTypeLabel(code: number): string | null {
    return this.cachedLabel(
      this._conjugationTypeLabels,
      this.module._suzume_conjugation_type_label,
      code,
      null,
    );
  }

  private conjugationFormLabel(code: number): string | null {
    return this.cachedLabel(
      this._conjugationFormLabels,
      this.module._suzume_conjugation_form_label,
      code,
      null,
    );
  }

  private extendedPosLabel(code: number): string {
    return this.cachedLabel(
      this._extendedPosLabels,
      this.module._suzume_extended_pos_label,
      code,
      'UNKNOWN',
    );
  }

  private cachedLabel<T extends string | null>(
    cache: Map<number, string | T>,
    nativeLabel: (code: number) => number,
    code: number,
    fallback: T,
  ): string | T {
    const cached = cache.get(code);
    if (cached !== undefined) {
      return cached;
    }
    const native = nativeString(this.module, nativeLabel(code));
    const label: string | T = native === undefined ? fallback : native;
    cache.set(code, label);
    return label;
  }
}

/**
 * Return the package version without creating an analyzer handle.
 *
 * @throws {SuzumeError} Internal when the WASM module fails to load
 */
export async function version(options?: RuntimeOptions): Promise<string> {
  const module = await loadModule('version', options?.wasmPath, options?.freshWasmModule === true);
  return readVersion(module);
}
