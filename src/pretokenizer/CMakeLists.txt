# PreTokenizer library
add_library(suzume_pretokenizer STATIC
    pretokenizer.cpp
)

target_include_directories(suzume_pretokenizer
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(suzume_pretokenizer
    PUBLIC suzume_core
    PUBLIC suzume_normalize
)
