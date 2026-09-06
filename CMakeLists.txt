cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0077 NEW)

# Default to Release build if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Version is the single source of truth here and is mirrored into every binding
# manifest (bindings/wasm/package.json, bindings/python/pyproject.toml). Keep them
# in lockstep; scripts/check_version_mirror.sh gates drift.
project(suzume VERSION 0.9.11 LANGUAGES CXX)

# Use C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_TESTING "Build tests" ON)
option(BUILD_WASM "Build for WebAssembly" OFF)
option(BUILD_SHARED "Build the shared C-ABI library (libsuzume) for ctypes bindings" OFF)
option(BUILD_CLI "Build the native developer CLI" ON)
option(SUZUME_EMBED_DICT "Embed the compiled dictionaries into the library (no filesystem access at runtime)" OFF)
option(SUZUME_LIB_SOVERSION "Attach VERSION/SOVERSION to the shared library (off for the flat wheel build)" ON)
option(SUZUME_INSTALL "Generate install/export rules (find_package, pkg-config)" ON)
option(SUZUME_BUILD_EXAMPLES "Build the C/C++ integration examples" OFF)
option(ENABLE_COVERAGE "Enable code coverage" OFF)

# The shared C-ABI library links the static core objects, so they must be PIC.
if(BUILD_SHARED)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# Default debug features OFF for WASM, ON otherwise
if(BUILD_WASM)
  option(ENABLE_DEBUG_INFO "Enable debug origin tracking in candidates" OFF)
  option(ENABLE_DEBUG_LOG "Enable debug logging (SUZUME_DEBUG)" OFF)
else()
  option(ENABLE_DEBUG_INFO "Enable debug origin tracking in candidates" ON)
  option(ENABLE_DEBUG_LOG "Enable debug logging (SUZUME_DEBUG)" ON)
endif()

# Add debug compile definitions
if(ENABLE_DEBUG_INFO)
  add_compile_definitions(SUZUME_DEBUG_INFO)
  message(STATUS "Debug origin tracking: ENABLED")
else()
  message(STATUS "Debug origin tracking: DISABLED")
endif()

if(ENABLE_DEBUG_LOG)
  add_compile_definitions(SUZUME_DEBUG)
  message(STATUS "Debug logging: ENABLED")
else()
  message(STATUS "Debug logging: DISABLED")
endif()

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

# Standard install layout (CMAKE_INSTALL_LIBDIR / INCLUDEDIR / DATADIR / BINDIR).
include(GNUInstallDirs)

# Source directories
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests")
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
# Dictionary paths are defined independently of BUILD_CLI so library-only
# configurations remain coherent. Compiling and installing these generated
# runtime files still requires the native CLI and is guarded below.
set(DICT_DATA_DIR "${CMAKE_SOURCE_DIR}/data")
file(GLOB CORE_TSV_FILES "${DICT_DATA_DIR}/core/*.tsv")
file(GLOB USER_TSV_FILES "${DICT_DATA_DIR}/user/*.tsv")

# Include directories: src/ for internal headers, include/ for the public C ABI header
include_directories(${SOURCE_DIR} ${INCLUDE_DIR})

# WASM configuration
if(BUILD_WASM)
  message(STATUS "Building for WebAssembly")
  set(CMAKE_EXECUTABLE_SUFFIX ".js")

  # Size-first WASM optimization. Compared with compiling the analysis layer
  # at -Os, full -Oz saves about 38 KB raw / 10 KB gzip but makes the measured
  # analysis benchmark about 11% slower. This trade-off is intentional.
  # Disabling C++ exceptions/RTTI saves another ~3.8 KB raw / 1.9 KB gzip.
  # Allocation failure therefore aborts the WASM module instead of becoming a
  # recoverable C API error; this was already the effective browser behavior.
  # -fno-threadsafe-statics was also measured here but changed neither the raw
  # nor gzip artifact size, so it is intentionally omitted.
  add_compile_options(-Oz -flto -fno-exceptions -fno-rtti)
  add_link_options(-Oz -flto)

  # Emscripten link options
  # The TypeScript wrapper only supplies locateFile to the generated module;
  # accepting Emscripten's full Module option surface retained ~1.9 KB of JS.
  # Both option-struct initializers are exported: the wrapper calls them to seed
  # freshly malloc'd memory with the C defaults, and the supported low-level
  # cwrap API and its layout tests call them directly.
  add_link_options(
    -sWASM=1
    -sMODULARIZE=1
    -sEXPORT_ES6=1
    "-sINCOMING_MODULE_JS_API=['locateFile']"
    "-sENVIRONMENT=['web','worker','node']"
    "-sEXPORTED_FUNCTIONS=['_malloc','_free','_suzume_create','_suzume_init_extended_options','_suzume_create_with_extended_options','_suzume_destroy','_suzume_set_mode','_suzume_mode','_suzume_analyze','_suzume_analyze_n','_suzume_result_free','_suzume_generate_tags','_suzume_generate_tags_n','_suzume_init_tag_options','_suzume_generate_tags_with_options','_suzume_generate_tags_with_options_n','_suzume_tags_free','_suzume_load_user_dict','_suzume_load_user_dict_count','_suzume_load_binary_dict','_suzume_clear_user_dictionaries','_suzume_has_core_dictionary','_suzume_version','_suzume_last_error','_suzume_last_error_code','_suzume_conjugation_type_label','_suzume_extended_pos_label','_suzume_conjugation_form_label','_suzume_pos_label','_suzume_dictionary_warning_count','_suzume_dictionary_warning','_suzume_sizeof_result','_suzume_sizeof_morpheme','_suzume_sizeof_tags','_suzume_sizeof_tag_options','_suzume_sizeof_extended_options','_suzume_offsetof_result','_suzume_offsetof_morpheme','_suzume_offsetof_tags','_suzume_offsetof_tag_options','_suzume_offsetof_extended_options']"
    "-sEXPORTED_RUNTIME_METHODS=['UTF8ToString','stringToUTF8','lengthBytesUTF8','HEAPU32']"
    -sALLOW_MEMORY_GROWTH=1
    -sFILESYSTEM=0
    -sMALLOC=emmalloc
    -sSTACK_SIZE=1048576
  )

  # Output directory for npm package
  set(WASM_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bindings/wasm/dist")
endif()

# Add subdirectories
add_subdirectory(src)

if(BUILD_CLI AND NOT BUILD_WASM)
  add_subdirectory(tools)
endif()

if(BUILD_TESTING AND NOT BUILD_WASM)
  add_subdirectory(tests)
endif()

if(BUILD_CLI AND NOT BUILD_WASM)
  # Dictionary build targets use the native developer CLI compiler.
  # This native target globs every local TSV, so it includes any gitignored,
  # local-only user dictionary. The public WASM build instead regenerates
  # data/user.dic from git-tracked TSVs only (see the wasm-dict target in the
  # Makefile), so local-only dictionaries never reach the shipped binary; CI has
  # no ignored files checked out, so its native and WASM dictionaries coincide.
  add_custom_target(build-dict
    COMMAND ${CMAKE_COMMAND} -E echo "Building dictionaries..."
    COMMAND $<TARGET_FILE:suzume-cli> dict compile
            ${CORE_TSV_FILES} ${DICT_DATA_DIR}/core.dic
    COMMAND $<TARGET_FILE:suzume-cli> dict compile
            ${USER_TSV_FILES} ${DICT_DATA_DIR}/user.dic
    COMMAND ${CMAKE_COMMAND} -E echo "Dictionary build complete: ${DICT_DATA_DIR}"
    DEPENDS suzume-cli
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Compiling TSV dictionaries to binary format"
  )

  add_custom_target(validate-dict
    COMMAND ${CMAKE_COMMAND} -E echo "Validating core dictionaries..."
    DEPENDS suzume-cli
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Validating TSV dictionary files"
  )

  foreach(TSV_FILE ${CORE_TSV_FILES})
    get_filename_component(TSV_NAME ${TSV_FILE} NAME)
    add_custom_command(TARGET validate-dict POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E echo "  Validating ${TSV_NAME}..."
      COMMAND $<TARGET_FILE:suzume-cli> dict validate ${TSV_FILE}
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
  endforeach()

  add_custom_command(TARGET validate-dict POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "All dictionaries validated successfully"
  )
endif()

# ---------------------------------------------------------------------------
# Install & export: find_package(suzume) + pkg-config
# ---------------------------------------------------------------------------
if(SUZUME_INSTALL AND NOT BUILD_WASM)
  include(CMakePackageConfigHelpers)

  set(SUZUME_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/suzume")

  # The static library is always installed; the shared library when it is built.
  set(SUZUME_INSTALL_TARGETS suzume)
  if(TARGET suzume_shared)
    list(APPEND SUZUME_INSTALL_TARGETS suzume_shared)
  endif()

  install(TARGETS ${SUZUME_INSTALL_TARGETS}
    EXPORT suzumeTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )

  # Public headers: C ABI (suzume_c.h) and the header-only C++ wrapper (suzume.hpp).
  install(DIRECTORY ${INCLUDE_DIR}/suzume
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
  )

  # Exported targets: suzume::suzume (static) and, when built, suzume::suzume_shared.
  install(EXPORT suzumeTargets
    NAMESPACE suzume::
    FILE suzumeTargets.cmake
    DESTINATION ${SUZUME_INSTALL_CMAKEDIR}
  )

  # find_package(suzume) searches for suzumeConfig.cmake or suzume-config.cmake,
  # spelled exactly as the package is named. A capitalized file only resolves on
  # a case-insensitive filesystem, so the name must match the package here.
  configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/suzumeConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/suzumeConfig.cmake
    INSTALL_DESTINATION ${SUZUME_INSTALL_CMAKEDIR}
  )
  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/suzumeConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/suzumeConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/suzumeConfigVersion.cmake
    DESTINATION ${SUZUME_INSTALL_CMAKEDIR}
  )

  # pkg-config for Autotools/Meson consumers.
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/suzume.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/suzume.pc
    @ONLY
  )
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/suzume.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
  )

  # Compiled dictionaries are produced by the native CLI. BUILD_CLI=OFF is a
  # coherent library-only install, so it intentionally omits these generated
  # runtime files while still installing libraries, headers, and package metadata.
  if(NOT SUZUME_EMBED_DICT AND BUILD_CLI)
    install(FILES
      ${DICT_DATA_DIR}/core.dic
      ${DICT_DATA_DIR}/user.dic
      DESTINATION ${CMAKE_INSTALL_DATADIR}/suzume
    )
  endif()
endif()

# C/C++ integration examples (built against the in-tree targets).
if(SUZUME_BUILD_EXAMPLES AND NOT BUILD_WASM)
  add_subdirectory(examples)
endif()
