# Suzume Makefile
# Convenience wrapper for CMake build system

.PHONY: help build test native-test native-test-nodebug native-bench mcp-test oracle-sync-check guardrails asan clean clean-build rebuild format format-check lint configure \
        wasm wasm-configure wasm-dict wasm-test wasm-bench wasm-clean wasm-rebuild wasm-linux dict \
        python-build python-test python-wheel python-sync python-examples version-check binding-parity \
        install uninstall examples examples-test embedded consumer-smoke cmake-smoke

# Build directories
BUILD_DIR := build
WASM_BUILD_DIR := build-wasm
ASAN_BUILD_DIR := build-asan
NODEBUG_BUILD_DIR := build-nodebug
# Apple's ASan runtime aborts when leak detection is requested. Linux enables
# LeakSanitizer in the same target; the CI sanitizer job is the authoritative
# leak gate.
ASAN_DETECT_LEAKS ?= $(if $(filter Darwin,$(shell uname -s)),0,1)

# Build jobs for every `cmake --build` here and in the scripts the recipes call
# (through CMAKE_BUILD_PARALLEL_LEVEL). A bare `--parallel` is an unbounded job
# count with the Makefile generator. Override: make JOBS=4 build
JOBS ?= $(shell getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)
export CMAKE_BUILD_PARALLEL_LEVEL := $(JOBS)

# clang-format command (can be overridden: make CLANG_FORMAT=clang-format-18 format)
CLANG_FORMAT ?= clang-format

# GNU make runs a recipe line with no shell metacharacters directly, doing its
# own PATH search that stops at the first name match even when that match is a
# directory - the exec then fails with "Permission denied". The Emscripten SDK
# puts two such directories on PATH (`<emsdk>/node` and
# `<emsdk>/upstream/emscripten/cmake`), so any target invoking these tools
# breaks once emsdk is active. Resolve the real executables the way a shell
# would, and keep the bare name as a fallback so the error still names the tool.
CMAKE := $(shell command -v cmake 2>/dev/null || echo cmake)
NODE := $(shell command -v node 2>/dev/null || echo node)

# Python environments. `uv run --locked` syncs the project's venv from its
# committed uv.lock on every call and fails if the lock no longer matches
# pyproject.toml, so no target needs a separate provisioning step.
UV_RUN := uv run --locked
# Repository scripts (scripts/*.py) run on the MCP project's pinned interpreter,
# never a bare python3 from PATH, whose version varies by machine.
SCRIPTS_PYTHON := $(UV_RUN) --project scripts/mcp python

# Re-provision both Python environments from their locks.
python-sync:
	cd scripts/mcp && uv sync --locked
	cd bindings/python && uv sync --locked

# Default target
.DEFAULT_GOAL := build

help:
	@echo "Suzume Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  make build        - Build the project (default)"
	@echo "  make dict         - Build dictionaries"
	@echo "  make test         - Run all tests (includes dict)"
	@echo "  make mcp-test     - Run MCP server/oracle tests"
	@echo "  make asan         - Run native tests under ASan/LSan/UBSan"
	@echo "  make native-test-nodebug - Run native tests with the debug features compiled out"
	@echo "  make native-bench - Measure native token throughput and long-input scaling"
	@echo "  make clean        - Remove $(BUILD_DIR) and every scratch build-* directory"
	@echo "  make clean-build  - Remove $(BUILD_DIR) only"
	@echo "  make rebuild      - Clean $(BUILD_DIR) and rebuild"
	@echo "  make format       - Auto-fix format/lint: C++, scripts/MCP, WASM, Python"
	@echo "  make format-check - Check formatting across all languages"
	@echo "  make lint         - Run read-only scripts/MCP, WASM, and Python static checks"
	@echo "  make configure    - Configure CMake"
	@echo "  make python-sync  - Re-provision both Python environments from their uv.lock"
	@echo "  make version-check - Verify version is consistent across binding manifests"
	@echo "  make cmake-smoke  - Verify supported CMake build/install configurations"
	@echo ""
	@echo "C/C++ integration targets:"
	@echo "  make install      - Install libs + headers + find_package/pkg-config (PREFIX=/usr/local)"
	@echo "  make uninstall    - Remove the installed files (run before 'make clean')"
	@echo "  make examples     - Build the in-tree C and C++ examples"
	@echo "  make embedded     - Build the embedded (no-filesystem, dict baked-in) static library"
	@echo "  make consumer-smoke - Install to a temp prefix and build examples via find_package"
	@echo ""
	@echo "Python binding targets:"
	@echo "  make python-build - Build the shared C-ABI library (libsuzume)"
	@echo "  make python-test  - Run Python binding tests (pytest/ruff/mypy)"
	@echo "  make python-wheel - Build a platform-tagged wheel"
	@echo ""
	@echo "WASM targets (debug info disabled for smaller binary):"
	@echo "  make wasm-configure - Configure the Emscripten build"
	@echo "  make wasm         - Build WASM module (includes wasm-dict)"
	@echo "  make wasm-dict    - Build dictionaries for the WASM link"
	@echo "  make wasm-test    - Run WASM tests"
	@echo "  make wasm-bench   - Measure public-API WASM throughput and memory"
	@echo "  make wasm-clean   - Clean WASM build"
	@echo "  make wasm-rebuild - Clean and rebuild WASM"
	@echo "  make wasm-linux   - Build WASM with the pinned Emscripten in build-linux-* (build host)"
	@echo ""
	@echo "Options:"
	@echo "  CMAKE_OPTIONS     - Extra CMake options (e.g., -DENABLE_DEBUG_INFO=OFF)"
	@echo ""
	@echo "  make help         - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make                                  # Build the project"
	@echo "  make test                             # Run all tests"
	@echo "  make wasm                             # Build WASM module"
	@echo "  make CMAKE_OPTIONS=-DENABLE_DEBUG_INFO=OFF  # Build without debug info"

# Configure CMake (always runs to pick up new test files)
configure:
	@mkdir -p $(BUILD_DIR)
	@$(CMAKE) -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release $(CMAKE_OPTIONS)

# Build the project
build: configure
	@echo "Building Suzume..."
	$(CMAKE) --build $(BUILD_DIR) --parallel $(JOBS)
	@echo "Build complete!"

# Build dictionaries
dict: build
	@echo "Building dictionaries..."
	$(CMAKE) --build $(BUILD_DIR) --target build-dict
	@echo "Dictionary build complete!"

# Run every shipped surface, consumer example, and repository invariant. Keep the native binary as
# the source of truth: CTest's configure-time discovery can miss newly added
# JSON golden cases and otherwise starts one process per gtest case.
test: native-test native-test-nodebug mcp-test oracle-sync-check python-test wasm-test examples-test consumer-smoke cmake-smoke binding-parity guardrails
	@echo "All tests complete!"

native-test: dict
	@echo "Running native tests..."
	$(BUILD_DIR)/bin/suzume_test
	ctest --test-dir $(BUILD_DIR) --output-on-failure -R native_cli_contract

# The WASM module and the Python wheel compile the debug features out; the
# default build keeps them. A value assigned only under those guards would
# change shipped output without failing native-test, so run the suite both ways.
native-test-nodebug:
	@echo "Running native tests with debug features compiled out..."
	$(CMAKE) -B $(NODEBUG_BUILD_DIR) -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release \
		-DENABLE_DEBUG_INFO=OFF -DENABLE_DEBUG_LOG=OFF
	$(CMAKE) --build $(NODEBUG_BUILD_DIR) --parallel $(JOBS)
	$(CMAKE) --build $(NODEBUG_BUILD_DIR) --target build-dict
	$(NODEBUG_BUILD_DIR)/bin/suzume_test
	ctest --test-dir $(NODEBUG_BUILD_DIR) --output-on-failure -R native_cli_contract

native-bench: dict
	$(SCRIPTS_PYTHON) scripts/measure_native_metrics.py

# Keep local verification aligned with the CI guardrails, including the WASM
# artifact-size check that needs the shipped module built first.
guardrails: wasm
	LC_ALL=C scripts/check_code_guardrails.sh
	$(SCRIPTS_PYTHON) scripts/check_oracle_overrides.py
	$(SCRIPTS_PYTHON) scripts/check_binding_labels.py
	$(SCRIPTS_PYTHON) scripts/check_compound_v2_sync.py
	$(SCRIPTS_PYTHON) scripts/check_oracle_mirrors.py
	scripts/check_version_mirror.sh
	scripts/check_wasm_size.sh bindings/wasm/dist/suzume.wasm

# AddressSanitizer includes LeakSanitizer on supported platforms. Keep this in
# a separate build tree so normal and sanitized objects can never mix.
asan:
	@echo "Running sanitizers (ASan leak detection=$(ASAN_DETECT_LEAKS), UBSan=on)..."
	$(CMAKE) -B $(ASAN_BUILD_DIR) -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug \
		-DENABLE_SANITIZER=ON -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
	$(CMAKE) --build $(ASAN_BUILD_DIR) --parallel $(JOBS)
	$(CMAKE) --build $(ASAN_BUILD_DIR) --target build-dict
	ASAN_OPTIONS=detect_leaks=$(ASAN_DETECT_LEAKS):halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
	$(ASAN_BUILD_DIR)/bin/suzume_test
	ASAN_OPTIONS=detect_leaks=$(ASAN_DETECT_LEAKS):halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
	ctest --test-dir $(ASAN_BUILD_DIR) --output-on-failure -R native_cli_contract

# Run the MCP server/oracle test suite from its project root so pytest uses
# scripts/mcp/pyproject.toml and imports the local package correctly.
mcp-test:
	@echo "Running MCP server/oracle tests..."
	cd scripts/mcp && $(UV_RUN) pytest -q

oracle-sync-check: build
	@echo "Checking golden expectations against a fresh oracle process..."
	$(SCRIPTS_PYTHON) scripts/check_oracle_sync.py

# Clean the primary build directory only
clean-build:
	@echo "Cleaning $(BUILD_DIR)..."
	rm -rf $(BUILD_DIR)

# Clean the primary build directory plus every scratch build-* directory produced by
# the ad-hoc CMake configurations across the repo (build-wasm, build-python,
# build-embedded, build-smoke, ...). All of them are gitignored.
# Run `make uninstall` BEFORE this if an install needs undoing: the manifest that
# uninstall reads lives under build-install and is removed here.
clean: clean-build
	@echo "Cleaning scratch build directories..."
	rm -rf build-*/ cmake-build-*/
	@echo "Clean complete!"

# Rebuild from scratch. Deliberately only recreates $(BUILD_DIR) — a full clean would
# also drop build-wasm and force an expensive Emscripten rebuild.
rebuild: clean-build build

# ============================================
# C/C++ integration targets
# ============================================

# Install prefix for `make install` (override: make PREFIX=/opt/suzume install)
PREFIX ?= /usr/local

# Build the shared + static libraries, headers, CMake package config, pkg-config,
# and dictionaries, then install them under $(PREFIX) for find_package / pkg-config.
install:
	@echo "Configuring install build (shared + static) ..."
	$(CMAKE) -B build-install -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED=ON -DBUILD_TESTING=OFF \
		-DCMAKE_INSTALL_PREFIX=$(PREFIX) $(CMAKE_OPTIONS)
	$(CMAKE) --build build-install --parallel $(JOBS)
	$(CMAKE) --build build-install --target build-dict
	$(CMAKE) --install build-install
	@echo "Installed suzume under $(PREFIX)"

# Remove what `make install` placed under $(PREFIX), using the manifest CMake wrote.
# Must run before `make clean`, which deletes build-install along with the manifest.
uninstall:
	@if [ -f build-install/install_manifest.txt ]; then \
		xargs rm -f < build-install/install_manifest.txt; \
		echo "Uninstalled suzume from the recorded prefix."; \
	else \
		echo "build-install/install_manifest.txt not found - nothing to uninstall."; \
		echo "It is written by 'make install' and removed by 'make clean'."; \
		exit 1; \
	fi

# Build the in-tree C and C++ examples.
examples: dict
	$(CMAKE) -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release -DSUZUME_BUILD_EXAMPLES=ON $(CMAKE_OPTIONS)
	$(CMAKE) --build $(BUILD_DIR) --target \
		suzume_example_c suzume_example_cpp suzume_example_cpp_basic \
		suzume_example_cpp_search_indexer suzume_example_cpp_tags \
		suzume_example_cpp_user_dictionary --parallel $(JOBS)
	@echo "Built all C/C++ examples under $(BUILD_DIR)/bin"

examples-test: examples
	ctest --test-dir $(BUILD_DIR) --output-on-failure -R '^suzume_example_'

# Build the embedded (no-filesystem) configuration: dictionaries baked in, static
# library, no CLI/tests. Useful for hosted-embedded / RTOS targets.
embedded:
	$(CMAKE) -B build-embedded -DCMAKE_BUILD_TYPE=Release -DSUZUME_EMBED_DICT=ON \
		-DBUILD_TESTING=OFF -DSUZUME_INSTALL=OFF $(CMAKE_OPTIONS)
	$(CMAKE) --build build-embedded --target suzume --parallel $(JOBS)
	@echo "Embedded static library built: build-embedded/lib/"

# Packaging smoke test: install both static and shared configurations, then
# build + run C/C++ consumers against each package via find_package.
consumer-smoke:
	$(CMAKE) -E remove_directory build-smoke-static
	$(CMAKE) -E remove_directory build-smoke-prefix-static
	$(CMAKE) -E remove_directory build-smoke-consumer-static
	$(CMAKE) -B build-smoke-static -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
		-DCMAKE_INSTALL_PREFIX=$(CURDIR)/build-smoke-prefix-static
	$(CMAKE) --build build-smoke-static --parallel $(JOBS)
	$(CMAKE) --build build-smoke-static --target build-dict
	$(CMAKE) --install build-smoke-static
	$(CMAKE) -S examples/consumer -B build-smoke-consumer-static \
		-DCMAKE_PREFIX_PATH=$(CURDIR)/build-smoke-prefix-static
	$(CMAKE) --build build-smoke-consumer-static
	ctest --test-dir build-smoke-consumer-static --output-on-failure
	$(CMAKE) -E remove_directory build-smoke-shared
	$(CMAKE) -E remove_directory build-smoke-prefix-shared
	$(CMAKE) -E remove_directory build-smoke-consumer-shared
	$(CMAKE) -B build-smoke-shared -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DBUILD_SHARED=ON \
		-DCMAKE_INSTALL_PREFIX=$(CURDIR)/build-smoke-prefix-shared
	$(CMAKE) --build build-smoke-shared --parallel $(JOBS)
	$(CMAKE) --build build-smoke-shared --target build-dict
	$(CMAKE) --install build-smoke-shared
	$(CMAKE) -S examples/consumer -B build-smoke-consumer-shared \
		-DCMAKE_PREFIX_PATH=$(CURDIR)/build-smoke-prefix-shared
	$(CMAKE) --build build-smoke-consumer-shared
	ctest --test-dir build-smoke-consumer-shared --output-on-failure
	@echo "Static and shared consumer smoke tests passed."

# Reproducible build/install matrix covering library-only, no-install, and
# multi-config dictionary builds without touching the shared build directories.
cmake-smoke:
	scripts/check_cmake_configurations.sh

# Auto-fix formatting/lint across every language in the repo:
# C++ core (clang-format), MCP/repository scripts (ruff), WASM binding (biome),
# Python binding (ruff).
format:
	@echo "Formatting C++ (clang-format)..."
	@find src include tools tests examples -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.c" \) | xargs $(CLANG_FORMAT) -i
	@echo "Formatting MCP server and repository scripts (ruff)..."
	cd scripts/mcp && $(UV_RUN) ruff format . ../../scripts/*.py && $(UV_RUN) ruff check --fix . ../../scripts/*.py
	@echo "Formatting WASM binding (biome)..."
	cd bindings/wasm && yarn lint:fix
	@echo "Formatting Python binding (ruff)..."
	cd bindings/python && $(UV_RUN) ruff format . && $(UV_RUN) ruff check --fix .
	$(MAKE) lint
	@echo "Format complete!"

# Check-only counterpart for CI (same language fan-out, no writes).
format-check:
	@echo "Checking C++ formatting (clang-format)..."
	@find src include tools tests examples -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.c" \) | xargs $(CLANG_FORMAT) --dry-run --Werror
	@echo "Checking MCP server and repository script formatting (ruff)..."
	cd scripts/mcp && $(UV_RUN) ruff format --check . ../../scripts/*.py
	@echo "Checking Python binding formatting (ruff)..."
	cd bindings/python && $(UV_RUN) ruff format --check .
	$(MAKE) lint
	@echo "Format check passed!"

# Read-only static analysis. This repository does not configure clang-tidy, so
# C++ is covered by clang-format in format-check rather than a nominal lint step.
lint:
	@echo "Linting MCP server and repository scripts (ruff)..."
	cd scripts/mcp && $(UV_RUN) ruff check . ../../scripts/*.py
	@echo "Linting WASM binding (biome)..."
	cd bindings/wasm && yarn lint
	@echo "Linting Python binding (ruff)..."
	cd bindings/python && $(UV_RUN) ruff check .

# ============================================
# Python binding targets
# ============================================

# Build shared library + Python package (editable) into a local venv
python-build:
	@echo "Building shared library for Python binding..."
	$(CMAKE) -B build-shared -DBUILD_SHARED=ON -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release \
		-DENABLE_DEBUG_INFO=OFF -DENABLE_DEBUG_LOG=OFF -DSUZUME_LIB_SOVERSION=OFF
	$(CMAKE) --build build-shared --target suzume_shared --parallel $(JOBS)
	@echo "Shared library built: build-shared/lib/"

# Compile the dictionaries the Python test package loads
python-dict: build
	@echo "Building dictionaries for the Python test package (git-tracked entries only)..."
	$(BUILD_DIR)/bin/suzume-cli dict compile $$(git ls-files 'data/core/*.tsv') data/core.dic
	$(BUILD_DIR)/bin/suzume-cli dict compile $$(git ls-files 'data/user/*.tsv') data/user.dic
	$(CMAKE) -E copy_if_different data/core.dic bindings/python/src/suzume/core.dic
	$(CMAKE) -E copy_if_different data/user.dic bindings/python/src/suzume/user.dic

python-test: python-build python-dict
	@echo "Running Python binding tests..."
	cd bindings/python && $(UV_RUN) pytest -q \
		&& $(UV_RUN) ruff check . \
		&& $(UV_RUN) ruff format --check . \
		&& $(UV_RUN) mypy src/suzume
	$(MAKE) python-examples

python-examples: python-build
	cd bindings/python && $(UV_RUN) python ../../examples/python/basic.py

# Build a platform-tagged wheel
python-wheel:
	cd bindings/python && ./build_wheel.sh

# Verify the version is consistent across every binding manifest
version-check:
	scripts/check_version_mirror.sh

# ============================================
# WASM Targets
# ============================================

# Configure WASM build (debug info disabled for smaller binary)
wasm-configure:
	@echo "Configuring WASM build..."
	emcmake cmake -B $(WASM_BUILD_DIR) -DBUILD_WASM=ON -DENABLE_DEBUG_INFO=OFF -DCMAKE_BUILD_TYPE=Release

# Build dictionaries for the WASM link using the native CLI.
# Produces the same full data/*.dic that the native build-dict target and CI/publish
# generate, so the embedded WASM dictionary matches every other target exactly.
wasm-dict:
	@if [ ! -f $(BUILD_DIR)/bin/suzume-cli ]; then \
		echo "Native CLI not found. Run 'make build' first."; \
		exit 1; \
	fi
	@echo "Building dictionaries for WASM (git-tracked entries only)..."
	$(BUILD_DIR)/bin/suzume-cli dict compile $$(git ls-files 'data/core/*.tsv') data/core.dic
	@# The WASM binary ships publicly, so embed only git-tracked user
	@# dictionaries. Any gitignored, local-only dictionary is excluded here
	@# without being named, matching the CI checkout (which lacks ignored files).
	$(BUILD_DIR)/bin/suzume-cli dict compile $$(git ls-files 'data/user/*.tsv') data/user.dic

# Build WASM module
wasm: wasm-dict wasm-configure
	@echo "Building WASM module..."
	@# Force re-link so updated embedded dictionaries are picked up
	@rm -f $(WASM_BUILD_DIR)/bin/suzume.wasm $(WASM_BUILD_DIR)/bin/suzume.js
	$(CMAKE) --build $(WASM_BUILD_DIR) --parallel $(JOBS)
	@echo "WASM build complete!"
	@ls -lh bindings/wasm/dist/suzume.wasm bindings/wasm/dist/suzume.js

# Build WASM with the Emscripten release CI pins, in build-linux-* directories,
# and run the size gate. .sendaway.yml sends this target to the build host,
# where the pinned SDK sits beside other releases; the result stays there
# (copy it out with `sendaway fetch`). Niced: that host is shared.
wasm-linux:
	nice -n 10 scripts/build_wasm_pinned.sh

# Run WASM tests
wasm-test: wasm
	@echo "Running WASM tests..."
	cd bindings/wasm && yarn test
	cd bindings/wasm && yarn test:examples
	@echo "WASM tests complete!"

binding-parity: build python-build wasm
	$(UV_RUN) --project bindings/python python scripts/check_binding_parity.py

# Measure the shipped public JS API, including result decoding. Override the
# workload with WASM_BENCH_ARGS, for example:
#   make wasm-bench WASM_BENCH_ARGS="--iterations=100 --samples=3"
wasm-bench: wasm
	$(NODE) scripts/measure_wasm_metrics.mjs $(WASM_BENCH_ARGS)

# Clean WASM build
wasm-clean:
	@echo "Cleaning WASM build..."
	rm -rf $(WASM_BUILD_DIR)
	@echo "WASM clean complete!"

# Rebuild WASM from scratch
wasm-rebuild: wasm-clean wasm
