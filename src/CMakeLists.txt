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

# WASM C API library and executable
if(BUILD_WASM)
  add_library(suzume_c STATIC
    suzume_c.cpp
  )
  target_link_libraries(suzume_c PUBLIC suzume)

  # WASM executable (produces .js + .wasm)
  add_executable(suzume-wasm
    suzume_c.cpp
  )
  target_link_libraries(suzume-wasm PRIVATE suzume)

  # Copy output to dist directory (keep original names for JS compatibility)
  add_custom_command(TARGET suzume-wasm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${WASM_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy
      "$<TARGET_FILE_DIR:suzume-wasm>/suzume-wasm.js"
      "${WASM_OUTPUT_DIR}/suzume.js"
    COMMAND ${CMAKE_COMMAND} -E copy
      "$<TARGET_FILE_DIR:suzume-wasm>/suzume-wasm.wasm"
      "${WASM_OUTPUT_DIR}/suzume-wasm.wasm"
    COMMENT "Copying WASM files to dist/"
  )
endif()

# CLI executable (legacy)
if(BUILD_CLI AND NOT BUILD_WASM)
  add_subdirectory(cli)
endif()

# Independent CLI tool (suzume-cli)
if(NOT BUILD_WASM)
  add_subdirectory(suzume-cli)
endif()
