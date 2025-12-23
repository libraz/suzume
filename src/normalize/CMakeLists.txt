# Normalize module CMakeLists.txt

add_library(suzume_normalize STATIC
  normalizer.cpp
  char_type.cpp
  utf8.cpp
)

target_include_directories(suzume_normalize
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(suzume_normalize
  PUBLIC
    suzume_core
)
