cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0077 NEW)
project(suzume VERSION 1.0.0 LANGUAGES CXX)

# Use C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_CLI "Build CLI executable" ON)
option(BUILD_TESTING "Build tests" ON)
option(BUILD_WASM "Build for WebAssembly" OFF)
option(ENABLE_COVERAGE "Enable code coverage" OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Enable testing at the top level
enable_testing()

# Sanitizer options
option(ENABLE_SANITIZER "Enable sanitizers" OFF)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

# Configure sanitizers if enabled
if(ENABLE_SANITIZER)
  message(STATUS "Sanitizers enabled")

  set(SANITIZER_FLAGS "")

  if(ENABLE_ASAN OR (NOT ENABLE_ASAN AND NOT ENABLE_UBSAN AND NOT ENABLE_TSAN))
    message(STATUS "AddressSanitizer enabled")
    set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fsanitize=address")
  endif()

  if(ENABLE_UBSAN OR (NOT ENABLE_ASAN AND NOT ENABLE_UBSAN AND NOT ENABLE_TSAN))
    message(STATUS "UndefinedBehaviorSanitizer enabled")
    set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fsanitize=undefined")
  endif()

  if(ENABLE_TSAN)
    message(STATUS "ThreadSanitizer enabled")
    set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fsanitize=thread")
  endif()

  # Add frame pointers for better stack traces
  set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fno-omit-frame-pointer")

  # Apply sanitizer flags
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZER_FLAGS}")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${SANITIZER_FLAGS}")
endif()

# Coverage settings
if(ENABLE_COVERAGE)
  message(STATUS "Code coverage enabled")
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -fprofile-instr-generate -fcoverage-mapping")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
  endif()
endif()

# macOS specific settings
if(APPLE)
  execute_process(
    COMMAND xcrun --show-sdk-path
    OUTPUT_VARIABLE MACOS_SDK_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(MACOS_SDK_PATH)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isysroot ${MACOS_SDK_PATH}")
  endif()
  add_compile_options(-Wno-unused-command-line-argument -Qunused-arguments)
endif()

# Compiler flags
if(MSVC)
  add_compile_options(/W4)
  add_compile_options(/wd4456 /wd4267 /wd4244 /wd4101)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Source directories
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests")

# Include directories
include_directories(${SOURCE_DIR})

# WASM configuration
if(BUILD_WASM)
  message(STATUS "Building for WebAssembly")
  set(CMAKE_EXECUTABLE_SUFFIX ".js")
  add_compile_options(-s WASM=1 -s MODULARIZE=1 -s EXPORT_ES6=1)
  add_link_options(-s WASM=1 -s MODULARIZE=1 -s EXPORT_ES6=1)
  add_link_options(-s EXPORTED_FUNCTIONS="['_malloc','_free']")
  add_link_options(-s EXPORTED_RUNTIME_METHODS="['cwrap','ccall','UTF8ToString','stringToUTF8']")
endif()

# Add subdirectories
add_subdirectory(src)

if(BUILD_TESTING AND NOT BUILD_WASM)
  add_subdirectory(tests)
endif()
