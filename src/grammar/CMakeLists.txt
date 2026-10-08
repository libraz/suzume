# Grammar module CMakeLists.txt

add_library(suzume_grammar OBJECT
  auxiliaries.cpp
  auxiliary_generator.cpp
  char_patterns.cpp
  char_patterns_lexical.cpp
  conjugation.cpp
  conjugator.cpp
  dictionary_expansion.cpp
  inflection.cpp
  inflection_scorer.cpp
  inflection_scorer_form_potential.cpp
  inflection_scorer_stem_godan.cpp
  honorific_verbs.cpp
  patterns.cpp
  verb_endings.cpp
)

target_include_directories(suzume_grammar PUBLIC
  ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(suzume_grammar
  PUBLIC
    suzume_core
)
