import process from 'node:process';

import { Suzume } from '../bindings/wasm/dist/index.js';

let payload = '';
for await (const chunk of process.stdin) {
  payload += chunk;
}
const cases = JSON.parse(payload);
if (!cases || !Array.isArray(cases.analysis) || !Array.isArray(cases.tags)) {
  throw new TypeError('expected { analysis: [...], tags: [...] }');
}

function renameDefined(options, pairs) {
  return Object.fromEntries(
    pairs.filter(([snake]) => options[snake] !== undefined).map(([snake, camel]) => [camel, options[snake]]),
  );
}

const EXTENDED_OPTION_PAIRS = [
  ['mode', 'mode'],
  ['preserve_vu', 'preserveVu'],
  ['preserve_case', 'preserveCase'],
  ['preserve_symbols', 'preserveSymbols'],
  ['lemmatize', 'lemmatize'],
  ['merge_compounds', 'mergeCompounds'],
];

const TAG_OPTION_PAIRS = [
  ['pos_filter', 'posFilter'],
  ['exclude_basic', 'excludeBasic'],
  ['use_lemma', 'useLemma'],
  ['min_length', 'minLength'],
  ['max_tags', 'maxTags'],
  ['exclude_particles', 'excludeParticles'],
  ['exclude_auxiliaries', 'excludeAuxiliaries'],
  ['exclude_formal_nouns', 'excludeFormalNouns'],
  ['exclude_low_info', 'excludeLowInfo'],
  ['remove_duplicates', 'removeDuplicates'],
];

const extendedOptions = (options) => renameDefined(options, EXTENDED_OPTION_PAIRS);
const tagOptions = (options) => renameDefined(options, TAG_OPTION_PAIRS);

function morphemeRecord(morpheme) {
  return {
    surface: morpheme.surface,
    pos: morpheme.pos,
    lemma: morpheme.baseForm,
    conj_type: morpheme.conjType,
    conj_form: morpheme.conjForm,
    extended_pos: morpheme.extendedPos,
    start: morpheme.start,
    end: morpheme.end,
    flags: {
      user_dict: morpheme.isUserDict,
      formal_noun: morpheme.isFormalNoun,
      low_info: morpheme.isLowInfo,
      unknown: morpheme.isUnknown,
      from_dictionary: morpheme.isFromDictionary,
      conjugatable: morpheme.conjForm !== null,
    },
    score: morpheme.score,
  };
}

const analysis = [];
for (const testCase of cases.analysis) {
  const analyzer = await Suzume.create(extendedOptions(testCase.options));
  try {
    const result = analyzer.analyzeWithNormalizedText(testCase.text);
    analysis.push({
      normalized_text: result.normalizedText,
      morphemes: result.morphemes.map(morphemeRecord),
    });
  } finally {
    analyzer.destroy();
  }
}

const tags = [];
for (const testCase of cases.tags) {
  const analyzer = await Suzume.create();
  try {
    tags.push(analyzer.generateTags(testCase.text, tagOptions(testCase.options)));
  } finally {
    analyzer.destroy();
  }
}

process.stdout.write(`${JSON.stringify({ analysis, tags })}\n`);
