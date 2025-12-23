# Core module CMakeLists.txt

add_library(suzume_core STATIC
  types.cpp
  error.cpp
  string_pool.cpp
  lattice.cpp
  viterbi.cpp
)

target_include_directories(suzume_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
