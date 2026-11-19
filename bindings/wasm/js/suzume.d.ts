/**
 * Type declarations for the Emscripten-generated WASM module
 */

export interface EmscriptenModuleOptions {
  locateFile?: (path: string, prefix: string) => string;
  onRuntimeInitialized?: () => void;
  print?: (text: string) => void;
  printErr?: (text: string) => void;
}

export interface EmscriptenModule {
  UTF8ToString: (ptr: number, maxBytesToRead?: number) => string;
  stringToUTF8: (str: string, outPtr: number, maxBytesToWrite: number) => void;
  lengthBytesUTF8: (str: string) => number;
  _malloc: (size: number) => number;
  _free: (ptr: number) => void;
  HEAPU32: Uint32Array;
  _suzume_create: () => number;
  _suzume_init_options: (optionsPtr: number) => void;
  _suzume_create_with_options: (optionsPtr: number) => number;
  _suzume_destroy: (handle: number) => void;
  _suzume_set_mode: (handle: number, mode: number) => number;
  _suzume_mode: (handle: number) => number;
  _suzume_analyze: (handle: number, textPtr: number) => number;
  _suzume_analyze_n: (handle: number, textPtr: number, size: number) => number;
  _suzume_result_free: (resultPtr: number) => void;
  _suzume_generate_tags: (handle: number, textPtr: number) => number;
  _suzume_generate_tags_n: (handle: number, textPtr: number, size: number) => number;
  _suzume_init_tag_options: (optionsPtr: number) => void;
  _suzume_generate_tags_with_options: (
    handle: number,
    textPtr: number,
    optionsPtr: number,
  ) => number;
  _suzume_generate_tags_with_options_n: (
    handle: number,
    textPtr: number,
    size: number,
    optionsPtr: number,
  ) => number;
  _suzume_tags_free: (tagsPtr: number) => void;
  _suzume_load_user_dictionary: (handle: number, dataPtr: number, size: number) => number;
  _suzume_load_binary_dictionary: (handle: number, dataPtr: number, size: number) => number;
  _suzume_clear_user_dictionaries: (handle: number) => number;
  _suzume_has_core_dictionary: (handle: number) => number;
  _suzume_version: () => number;
  _suzume_last_error: () => number;
  _suzume_last_error_code: () => number;
  _suzume_conjugation_type_label: (code: number) => number;
  _suzume_extended_pos_label: (code: number) => number;
  _suzume_conjugation_form_label: (code: number) => number;
  _suzume_pos_label: (code: number) => number;
  _suzume_pos_label_ja: (code: number) => number;
  _suzume_dictionary_warning_count: (handle: number) => number;
  _suzume_dictionary_warning: (handle: number, index: number) => number;
}

declare function createModule(options?: EmscriptenModuleOptions): Promise<EmscriptenModule>;

export default createModule;
