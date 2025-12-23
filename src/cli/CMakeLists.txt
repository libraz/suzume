# CLI module CMakeLists.txt

add_executable(suzume_cli
  main.cpp
)

target_link_libraries(suzume_cli
  PRIVATE
    suzume
)

set_target_properties(suzume_cli PROPERTIES
  OUTPUT_NAME suzume
)
