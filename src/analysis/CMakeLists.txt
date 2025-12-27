# Analysis module CMakeLists.txt

add_library(suzume_analysis STATIC
  adjective_candidates.cpp
  analyzer.cpp
  connection_rules.cpp
  join_candidates.cpp
  scorer.cpp
  split_candidates.cpp
  suffix_candidates.cpp
  tokenizer.cpp
  tokenizer_utils.cpp
  unknown.cpp
  verb_candidates.cpp
)

target_include_directories(suzume_analysis
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(suzume_analysis
  PUBLIC
    suzume_core
    suzume_normalize
    suzume_dictionary
    suzume_grammar
    suzume_pretokenizer
)
