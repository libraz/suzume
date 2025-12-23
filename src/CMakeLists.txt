# Main source CMakeLists.txt

# Add subdirectories
add_subdirectory(core)
add_subdirectory(normalize)
add_subdirectory(dictionary)
add_subdirectory(grammar)
add_subdirectory(pretokenizer)
add_subdirectory(analysis)
add_subdirectory(postprocess)

# Main suzume library
add_library(suzume STATIC
  suzume.cpp
)

target_link_libraries(suzume
  PUBLIC
    suzume_core
    suzume_normalize
    suzume_dictionary
    suzume_grammar
    suzume_pretokenizer
    suzume_analysis
    suzume_postprocess
)

target_include_directories(suzume
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# WASM C API library
if(BUILD_WASM)
  add_library(suzume_c STATIC
    suzume_c.cpp
  )
  target_link_libraries(suzume_c PUBLIC suzume)
endif()

# CLI executable (legacy)
if(BUILD_CLI AND NOT BUILD_WASM)
  add_subdirectory(cli)
endif()

# Independent CLI tool (suzume-cli)
if(NOT BUILD_WASM)
  add_subdirectory(suzume-cli)
endif()
