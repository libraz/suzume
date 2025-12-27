# Suzume Makefile
# Convenience wrapper for CMake build system

.PHONY: help build test clean rebuild format format-check configure \
        wasm wasm-test wasm-clean wasm-rebuild dict

# Build directories
BUILD_DIR := build
WASM_BUILD_DIR := build-wasm

# clang-format command (can be overridden: make CLANG_FORMAT=clang-format-18 format)
CLANG_FORMAT ?= clang-format

# Default target
.DEFAULT_GOAL := build

help:
	@echo "Suzume Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  make build        - Build the project (default)"
	@echo "  make dict         - Build dictionaries"
	@echo "  make test         - Run all tests (includes dict)"
	@echo "  make clean        - Clean build directory"
	@echo "  make rebuild      - Clean and rebuild"
	@echo "  make format       - Format code with clang-format"
	@echo "  make format-check - Check code formatting"
	@echo "  make configure    - Configure CMake"
	@echo ""
	@echo "WASM targets:"
	@echo "  make wasm         - Build WASM module"
	@echo "  make wasm-test    - Run WASM tests"
	@echo "  make wasm-clean   - Clean WASM build"
	@echo "  make wasm-rebuild - Clean and rebuild WASM"
	@echo ""
	@echo "  make help         - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make              # Build the project"
	@echo "  make test         # Run all tests"
	@echo "  make wasm         # Build WASM module"

# Configure CMake
configure:
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release $(CMAKE_OPTIONS) ..

# Build the project
build: configure
	@echo "Building Suzume..."
	cmake --build $(BUILD_DIR) --parallel
	@echo "Build complete!"

# Build dictionaries
dict: build
	@echo "Building dictionaries..."
	cmake --build $(BUILD_DIR) --target build-dict
	@echo "Dictionary build complete!"

# Run tests
test: dict
	@echo "Running tests..."
	ctest --test-dir $(BUILD_DIR) --output-on-failure
	@echo "Tests complete!"

# Clean build directory
clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)
	@echo "Clean complete!"

# Rebuild from scratch
rebuild: clean build

# Format code with clang-format
format:
	@echo "Formatting code..."
	@find src tests -type f \( -name "*.cpp" -o -name "*.h" \) | xargs $(CLANG_FORMAT) -i
	@echo "Format complete!"

# Check code formatting
format-check:
	@echo "Checking code formatting..."
	@find src tests -type f \( -name "*.cpp" -o -name "*.h" \) | xargs $(CLANG_FORMAT) --dry-run --Werror
	@echo "Format check passed!"

# ============================================
# WASM Targets
# ============================================

# Configure WASM build
wasm-configure:
	@echo "Configuring WASM build..."
	emcmake cmake -B $(WASM_BUILD_DIR) -DBUILD_WASM=ON -DCMAKE_BUILD_TYPE=Release

# Build WASM module
wasm: wasm-configure
	@echo "Building WASM module..."
	cmake --build $(WASM_BUILD_DIR) --parallel
	@echo "WASM build complete!"
	@ls -lh dist/suzume-wasm.wasm dist/suzume.js

# Run WASM tests
wasm-test: wasm
	@echo "Running WASM tests..."
	yarn test
	@echo "WASM tests complete!"

# Clean WASM build
wasm-clean:
	@echo "Cleaning WASM build..."
	rm -rf $(WASM_BUILD_DIR)
	@echo "WASM clean complete!"

# Rebuild WASM from scratch
wasm-rebuild: wasm-clean wasm
