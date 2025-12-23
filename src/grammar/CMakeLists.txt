# Grammar module CMakeLists.txt

add_library(suzume_grammar STATIC
  conjugation.cpp
  conjugator.cpp
  connection.cpp
  inflection.cpp
)

target_include_directories(suzume_grammar PUBLIC
  ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(suzume_grammar
  PUBLIC
    suzume_core
)
