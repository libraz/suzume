/**
 * Type-level proof that removed names stay removed and the new shapes hold.
 * Checked by `yarn tsc -p tsconfig.types.json`; an unused `@ts-expect-error`
 * fails the check, so each line below must still be a type error.
 */

import type {
  AnalysisMode,
  AnalysisResult,
  Morpheme,
  PosLabel,
  RuntimeOptions,
  Suzume,
  SuzumeOptions,
  Tag,
  TagOptions,
  TagPosLabel,
} from '../../js/index.js';
import * as entry from '../../js/index.js';

declare const sz: Suzume;
declare const morpheme: Morpheme;
declare const tag: Tag;
declare const result: AnalysisResult;

// @ts-expect-error the default export is removed; import { Suzume } instead
entry.default;

// @ts-expect-error TagPosFilterName is replaced by TagPosLabel
type OldFilterName = entry.TagPosFilterName;

// @ts-expect-error the instance version getter is removed; use the free version()
sz.version;

// @ts-expect-error lastError is removed; errors are thrown
sz.lastError;

// @ts-expect-error lastErrorCode is removed; errors are thrown
sz.lastErrorCode;

// @ts-expect-error loadUserDictionaryCount is folded into loadUserDictionary
sz.loadUserDictionaryCount('');

// @ts-expect-error loadUserDictionaryOrThrow is folded into loadUserDictionary
sz.loadUserDictionaryOrThrow('');

// @ts-expect-error loadBinaryDictionaryOrThrow is folded into loadBinaryDictionary
sz.loadBinaryDictionaryOrThrow(new Uint8Array());

// @ts-expect-error useLemma is renamed to useBaseForm
const oldBaseForm: TagOptions = { useLemma: false };

// @ts-expect-error the deprecated pos alias is removed; use posFilter
const oldPosAlias: TagOptions = { pos: ['NOUN'] };

// @ts-expect-error lowercase and long filter names are not labels
const oldFilterValue: TagOptions = { posFilter: ['noun'] };

// @ts-expect-error skipUserDictionary is renamed to skipBundledUserDictionary
const oldSkip: SuzumeOptions = { skipUserDictionary: true };

// @ts-expect-error result fields are readonly
morpheme.surface = 'x';

// @ts-expect-error result arrays are readonly
result.morphemes.push(morpheme);

// @ts-expect-error tags are readonly
tag.tag = 'x';

// @ts-expect-error generateTags returns a readonly array
sz.generateTags('東京').push(tag);

// @ts-expect-error analyze returns a readonly array
sz.analyze('東京').push(morpheme);

// @ts-expect-error a POS label is one of the printed labels, not any string
const notALabel: PosLabel = 'noun';

const newShapes: TagOptions = { posFilter: 'NOUN', useBaseForm: false };
const arrayFilter: TagOptions = { posFilter: ['NOUN', 'VERB'] satisfies TagPosLabel[] };
const runtime: RuntimeOptions = { wasmPath: 'suzume.wasm', freshWasmModule: true };
const create: Parameters<typeof Suzume.create>[0] = {
  skipBundledUserDictionary: true,
  ...runtime,
};
const mode: AnalysisMode = 'search';
const pos: PosLabel = morpheme.pos;
const tagPos: PosLabel = tag.pos;
const versionCall: Promise<string> = entry.version(runtime);

export type { OldFilterName };
export {
  arrayFilter,
  create,
  mode,
  newShapes,
  notALabel,
  oldBaseForm,
  oldFilterValue,
  oldPosAlias,
  oldSkip,
  pos,
  tagPos,
  versionCall,
};
