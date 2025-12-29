# Dictionary module CMakeLists.txt

add_library(suzume_dictionary STATIC
  dictionary.cpp
  core_dict.cpp
  user_dict.cpp
  trie.cpp
  double_array.cpp
  binary_dict.cpp
  string_pool.cpp
  entries/entries.cpp
)

target_include_directories(suzume_dictionary
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/entries
)

target_link_libraries(suzume_dictionary
  PUBLIC
    suzume_core
)
