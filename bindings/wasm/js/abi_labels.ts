// Every label (POS, extended POS, conjugation) is decoded through the canonical
// C ABI label functions; only the morpheme flag bits are mirrored here.

export const MORPHEME_FLAG = {
  userDict: 1 << 0,
  formalNoun: 1 << 1,
  lowInfo: 1 << 2,
  unknown: 1 << 3,
  fromDictionary: 1 << 4,
  conjugatable: 1 << 5,
} as const;
