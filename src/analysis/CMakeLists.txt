# Analysis module CMakeLists.txt

add_library(suzume_analysis STATIC
  analyzer.cpp
  tokenizer.cpp
  scorer.cpp
  unknown.cpp
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
