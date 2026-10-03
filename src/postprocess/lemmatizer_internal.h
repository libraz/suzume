#ifndef SUZUME_POSTPROCESS_LEMMATIZER_INTERNAL_H_
#define SUZUME_POSTPROCESS_LEMMATIZER_INTERNAL_H_

#include <string>
#include <string_view>

#include "core/morpheme.h"
#include "dictionary/dictionary.h"

namespace suzume::postprocess::lemmatizer_detail {

bool hasExactVerbEntry(const dictionary::DictionaryManager* dict_manager, std::string_view surface);
std::string fixSuruClassical(std::string_view lemma, dictionary::ConjugationType conj_type);
std::string fixShiru(std::string_view lemma, std::string_view surface);
std::string fixSpecialRaRowLemma(std::string_view lemma, const dictionary::DictionaryManager* dict_manager);
std::string fixGodanRenyokeiBeforeLiteraryTe(std::string_view surface, std::string_view lemma,
                                             std::string_view next_surface,
                                             const dictionary::DictionaryManager* dict_manager);
std::string fixIchidanRenyokeiBeforeTe(std::string_view surface, std::string_view lemma, std::string_view next_surface,
                                       const dictionary::DictionaryManager* dict_manager);
std::string fixPotentialVerb(const core::Morpheme& morpheme);
std::string fixTariAdverb(std::string_view surface);
std::string fixHatsuonbin(std::string_view stem, const dictionary::DictionaryManager* dict_manager);
std::string lemmatizeContractedVerbWithDictionary(std::string_view surface,
                                                  const dictionary::DictionaryManager* dict_manager);
std::string lemmatizeSuruPassiveWithDictionary(std::string_view surface,
                                               const dictionary::DictionaryManager* dict_manager);
std::string lemmatizeVerbFallback(std::string_view surface);

}  // namespace suzume::postprocess::lemmatizer_detail

#endif  // SUZUME_POSTPROCESS_LEMMATIZER_INTERNAL_H_
