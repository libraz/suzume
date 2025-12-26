#include "types.h"

namespace suzume::core {

std::string_view posToString(PartOfSpeech pos) {
  switch (pos) {
    case PartOfSpeech::Noun:
      return "NOUN";
    case PartOfSpeech::Verb:
      return "VERB";
    case PartOfSpeech::Adjective:
      return "ADJ";
    case PartOfSpeech::Adverb:
      return "ADV";
    case PartOfSpeech::Particle:
      return "PARTICLE";
    case PartOfSpeech::Auxiliary:
      return "AUX";
    case PartOfSpeech::Conjunction:
      return "CONJ";
    case PartOfSpeech::Determiner:
      return "DET";
    case PartOfSpeech::Pronoun:
      return "PRON";
    case PartOfSpeech::Symbol:
      return "SYMBOL";
    case PartOfSpeech::Other:
    case PartOfSpeech::Unknown:
    default:
      return "OTHER";
  }
}

std::string_view posToJapanese(PartOfSpeech pos) {
  switch (pos) {
    case PartOfSpeech::Noun:
      return "名詞";
    case PartOfSpeech::Verb:
      return "動詞";
    case PartOfSpeech::Adjective:
      return "形容詞";
    case PartOfSpeech::Adverb:
      return "副詞";
    case PartOfSpeech::Particle:
      return "助詞";
    case PartOfSpeech::Auxiliary:
      return "助動詞";
    case PartOfSpeech::Conjunction:
      return "接続詞";
    case PartOfSpeech::Determiner:
      return "連体詞";
    case PartOfSpeech::Pronoun:
      return "代名詞";
    case PartOfSpeech::Symbol:
      return "記号";
    case PartOfSpeech::Other:
    case PartOfSpeech::Unknown:
    default:
      return "その他";
  }
}

PartOfSpeech stringToPos(std::string_view str) {
  // English names
  if (str == "NOUN" || str == "名詞") {
    return PartOfSpeech::Noun;
  }
  if (str == "VERB" || str == "動詞") {
    return PartOfSpeech::Verb;
  }
  if (str == "ADJ" || str == "形容詞") {
    return PartOfSpeech::Adjective;
  }
  if (str == "ADV" || str == "副詞") {
    return PartOfSpeech::Adverb;
  }
  if (str == "PARTICLE" || str == "助詞") {
    return PartOfSpeech::Particle;
  }
  if (str == "AUX" || str == "助動詞") {
    return PartOfSpeech::Auxiliary;
  }
  if (str == "CONJ" || str == "接続詞") {
    return PartOfSpeech::Conjunction;
  }
  if (str == "DET" || str == "連体詞") {
    return PartOfSpeech::Determiner;
  }
  if (str == "PRON" || str == "代名詞") {
    return PartOfSpeech::Pronoun;
  }
  if (str == "SYMBOL" || str == "記号") {
    return PartOfSpeech::Symbol;
  }
  return PartOfSpeech::Other;
}

bool isTaggable(PartOfSpeech pos) {
  return pos == PartOfSpeech::Noun || pos == PartOfSpeech::Verb || pos == PartOfSpeech::Adjective ||
         pos == PartOfSpeech::Adverb;
}

bool isContentWord(PartOfSpeech pos) {
  return pos == PartOfSpeech::Noun || pos == PartOfSpeech::Verb || pos == PartOfSpeech::Adjective ||
         pos == PartOfSpeech::Adverb;
}

bool isFunctionWord(PartOfSpeech pos) {
  return pos == PartOfSpeech::Particle || pos == PartOfSpeech::Auxiliary;
}

}  // namespace suzume::core
