# Native developer CLI (not WASM compatible).

add_executable(suzume-cli
  main.cpp
  cli_common.cpp
  cli_args.cpp
  cli_help.cpp
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

# The developer CLI follows the root install/export switch. Library and wheel
# builds use SUZUME_INSTALL=OFF to avoid creating any installable CLI artifact.
if(SUZUME_INSTALL)
  install(TARGETS suzume-cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endif()
