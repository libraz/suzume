# Grammar module CMakeLists.txt

add_library(suzume_grammar STATIC
  auxiliaries.cpp
  auxiliary_generator.cpp
  char_patterns.cpp
  conjugation.cpp
  conjugator.cpp
  connection.cpp
  inflection.cpp
  inflection_scorer.cpp
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
