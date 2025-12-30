# suzume-cli CMakeLists.txt
# Independent CLI tool (not WASM compatible)

add_executable(suzume-cli
  main.cpp
  cli_common.cpp
  cmd_analyze.cpp
  cmd_dict.cpp
  cmd_test.cpp
  tsv_parser.cpp
  dict_compiler.cpp
  interactive.cpp
  interactive_commands.cpp
  interactive_utils.cpp
)

target_include_directories(suzume-cli PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(suzume-cli PRIVATE
  suzume
)

# Set output directory
set_target_properties(suzume-cli PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install target
install(TARGETS suzume-cli
  RUNTIME DESTINATION bin
)
