"""Constants ported from SuzumeUtils.pm."""

from . import constants_pos, constants_search_units, constants_spelling, constants_text

# Exports from constants_pos.
ADVERBIAL_NA_ADJECTIVES = constants_pos.ADVERBIAL_NA_ADJECTIVES
ADVERB_NOMINAL_HOMOGRAPHS = constants_pos.ADVERB_NOMINAL_HOMOGRAPHS
ADVERB_OVERRIDES = constants_pos.ADVERB_OVERRIDES
ARCHAIC_PERSONAL_PRONOUNS = constants_pos.ARCHAIC_PERSONAL_PRONOUNS
BOUND_SUFFIX_VERB_NOUN_CELLS = constants_pos.BOUND_SUFFIX_VERB_NOUN_CELLS
CLASSICAL_ADJECTIVE_LEMMA_OVERRIDES = constants_pos.CLASSICAL_ADJECTIVE_LEMMA_OVERRIDES
COPULA_SURFACES = constants_pos.COPULA_SURFACES
DERIVATIONAL_SUFFIX_VERB_LEMMAS = constants_pos.DERIVATIONAL_SUFFIX_VERB_LEMMAS
FIXED_FUNCTION_LEMMAS = constants_pos.FIXED_FUNCTION_LEMMAS
HISTORICAL_KANA_RESPELLING = constants_pos.HISTORICAL_KANA_RESPELLING
INTERROGATIVES = constants_pos.INTERROGATIVES
KANA_PERSONAL_PRONOUNS = constants_pos.KANA_PERSONAL_PRONOUNS
KEEP_AS_NOUN_NOT_ADJ = constants_pos.KEEP_AS_NOUN_NOT_ADJ
NA_ADJ_OVERRIDES = constants_pos.NA_ADJ_OVERRIDES
NOUN_AS_PRONOUN = constants_pos.NOUN_AS_PRONOUN
POS_NORM_MAP = constants_pos.POS_NORM_MAP
PRONOUN_OVERRIDES = constants_pos.PRONOUN_OVERRIDES
SUFFIX_AS_NOUN = constants_pos.SUFFIX_AS_NOUN
SUZUME_POS_OVERRIDE = constants_pos.SUZUME_POS_OVERRIDE
TTARA_STEMS = constants_pos.TTARA_STEMS
TTEBA_STEMS = constants_pos.TTEBA_STEMS
VALID_POS = constants_pos.VALID_POS
VERB_NOT_AUX_LEMMAS = constants_pos.VERB_NOT_AUX_LEMMAS

# Exports from constants_search_units.
BENEFACTIVE_REQUEST_LEMMAS = constants_search_units.BENEFACTIVE_REQUEST_LEMMAS
CLASSICAL_KERI_CONJ_TYPE = constants_search_units.CLASSICAL_KERI_CONJ_TYPE
CLASSICAL_KI_CONJ_TYPE = constants_search_units.CLASSICAL_KI_CONJ_TYPE
COLLOQUIAL_PRONOUNS = constants_search_units.COLLOQUIAL_PRONOUNS
COUNTER_UNITS = constants_search_units.COUNTER_UNITS
DERIVED_ADJECTIVE_SUFFIX_LEMMAS = constants_search_units.DERIVED_ADJECTIVE_SUFFIX_LEMMAS
DERIVED_VERB_FRAGMENT_SPAN = constants_search_units.DERIVED_VERB_FRAGMENT_SPAN
DERIVED_VERB_SUFFIX_FORMS = constants_search_units.DERIVED_VERB_SUFFIX_FORMS
DERIVED_VERB_SUFFIX_LEMMAS = constants_search_units.DERIVED_VERB_SUFFIX_LEMMAS
FAMILY_TERMS = constants_search_units.FAMILY_TERMS
FINITE_PREDECESSOR_CONJ_FORM = constants_search_units.FINITE_PREDECESSOR_CONJ_FORM
FIXED_FUNCTION_SEARCH_UNITS = constants_search_units.FIXED_FUNCTION_SEARCH_UNITS
FIXED_INFLECTED_FUNCTION_UNITS = constants_search_units.FIXED_INFLECTED_FUNCTION_UNITS
FIXED_LEADING_SEARCH_UNITS = constants_search_units.FIXED_LEADING_SEARCH_UNITS
GREETING_INTERJECTIONS = constants_search_units.GREETING_INTERJECTIONS
HIRAGANA_COMPOUNDS = constants_search_units.HIRAGANA_COMPOUNDS
HONORIFIC_EXCEPTIONS = constants_search_units.HONORIFIC_EXCEPTIONS
HONORIFIC_FRAME_TAILS = constants_search_units.HONORIFIC_FRAME_TAILS
HONORIFIC_SUFFIXES = constants_search_units.HONORIFIC_SUFFIXES
KANA_COUNTER_SUFFIXES = constants_search_units.KANA_COUNTER_SUFFIXES
KANA_NUMBER_STEMS = constants_search_units.KANA_NUMBER_STEMS
KANJI_PREFIX_COMPOUNDS = constants_search_units.KANJI_PREFIX_COMPOUNDS
KANJI_SUFFIXES_KEPT_SEPARATE = constants_search_units.KANJI_SUFFIXES_KEPT_SEPARATE
LETTER_FORMULAS = constants_search_units.LETTER_FORMULAS
ADDRESS_CLOSING_NOUNS = constants_search_units.ADDRESS_CLOSING_NOUNS
LEXICALIZED_DERIVED_VERBS = constants_search_units.LEXICALIZED_DERIVED_VERBS
PREFIX_EXCEPTIONS = constants_search_units.PREFIX_EXCEPTIONS
QUANTITY_BOUND_SUFFIXES = constants_search_units.QUANTITY_BOUND_SUFFIXES
SEARCH_UNIT_COMPOUNDS = constants_search_units.SEARCH_UNIT_COMPOUNDS
STATE_NOUN_SUFFIXES = constants_search_units.STATE_NOUN_SUFFIXES
TEMPORAL_COMPOUND_UNITS = constants_search_units.TEMPORAL_COMPOUND_UNITS
TEMPORAL_PREFIX_KANJI = constants_search_units.TEMPORAL_PREFIX_KANJI
TEXT_SYMBOLS = constants_search_units.TEXT_SYMBOLS
USER_DICT_COMPOUNDS = constants_search_units.USER_DICT_COMPOUNDS

# Exports from constants_spelling.
CHARACTER_SPEECH_COPULAS = constants_spelling.CHARACTER_SPEECH_COPULAS
CHARACTER_SPEECH_FINAL_PARTICLES = constants_spelling.CHARACTER_SPEECH_FINAL_PARTICLES
COMPOUND_VERB_V2_GODAN = constants_spelling.COMPOUND_VERB_V2_GODAN
COMPOUND_VERB_V2_ICHIDAN = constants_spelling.COMPOUND_VERB_V2_ICHIDAN
COMPOUND_VERB_V2_NOT_AFTER_SURU = constants_spelling.COMPOUND_VERB_V2_NOT_AFTER_SURU
COMPOUND_VERB_V2_SURU_ONLY = constants_spelling.COMPOUND_VERB_V2_SURU_ONLY
DIALECT_FINAL_PARTICLES = constants_spelling.DIALECT_FINAL_PARTICLES
PARTICLE_CORRECTIONS = constants_spelling.PARTICLE_CORRECTIONS
SLANG_ADJ_FOLLOWER = constants_spelling.SLANG_ADJ_FOLLOWER
SLANG_ADJ_STEMS = constants_spelling.SLANG_ADJ_STEMS
SLANG_VERB_STEMS = constants_spelling.SLANG_VERB_STEMS
TARI_ADVERB_STEMS = constants_spelling.TARI_ADVERB_STEMS
UNUSUAL_NAMES = constants_spelling.UNUSUAL_NAMES
WORD_EXCEPTIONS = constants_spelling.WORD_EXCEPTIONS
WORD_EXCEPTION_BLOCKED_FOLLOWERS = constants_spelling.WORD_EXCEPTION_BLOCKED_FOLLOWERS

# Exports from constants_text.
COPULAR_PREDICATE_HEADS = constants_text.COPULAR_PREDICATE_HEADS
KANJI_RANGES = constants_text.KANJI_RANGES
KANJI_VERB_ROW_FRAMES = constants_text.KANJI_VERB_ROW_FRAMES
KYUJITAI_TO_SHINJITAI = constants_text.KYUJITAI_TO_SHINJITAI
is_all_kanji = constants_text.is_all_kanji
is_kanji = constants_text.is_kanji
katakana_to_hiragana = constants_text.katakana_to_hiragana


# MeCab POS to Suzume POS mapping (raw mapping from MeCab)
POS_MAP: dict[str, str] = {
    "名詞": "Noun",
    "動詞": "Verb",
    "形容詞": "Adjective",
    "副詞": "Adverb",
    "助詞": "Particle",
    "助動詞": "Auxiliary",
    "接続詞": "Conjunction",
    "感動詞": "Interjection",
    "連体詞": "Adnominal",
    "接頭詞": "Prefix",
    "接尾辞": "Suffix",
    "代名詞": "Pronoun",
    "記号": "Symbol",
    "フィラー": "Filler",
    "その他": "Other",
}


# ナイ形容詞 (adjectives ending in ない that are single lexical units)
NAI_ADJECTIVES: list[str] = [
    "だらしない",
    "つまらない",
    "しょうがない",
    "もったいない",
    "くだらない",
    "せわしない",
    "やるせない",
    "いたたまれない",
    "あどけない",
    "おぼつかない",
    "はしたない",
    "みっともない",
    "ろくでもない",
    "どうしようもない",
    "ものたりない",
    "こころもとない",
]


# Lexical adjectives that MeCab merges even though Suzume preserves the
# productive nominal + independent ない boundary.
NOUN_NAI_COMPOUND_ADJECTIVES: list[str] = [
    "揺るぎない",
]


# Colloquial causatives that IPADIC stores as independent Godan-す headwords,
# despite their productive a-row host plus causative-す boundary.
LEXICALIZED_CAUSATIVE_SU_LEMMAS: frozenset[str] = frozenset({"待たす", "行かす"})


# The classical honorific あらせる (あらせ+られる) is a closed honorific verb, not
# the productive causative of ある; Suzume keeps it as one L1 word.
CLOSED_HONORIFIC_SERU_LEMMAS: frozenset[str] = frozenset({"あらせる"})


# Classical volitional auxiliary followed by a quotative particle.  MeCab
# sometimes treats the closed-class sequence as one noun token; Suzume keeps
# both grammatical search units independent.
LITERARY_VOLITIONAL_PARTICLE_COMPOUNDS: dict[str, tuple[str, str]] = {
    "むと": ("む", "と"),
}
