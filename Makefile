# Suzume Makefile
# Convenience wrapper for CMake build system

.PHONY: help build test clean rebuild format format-check configure

# Build directory
BUILD_DIR := build

# clang-format command (can be overridden: make CLANG_FORMAT=clang-format-18 format)
CLANG_FORMAT ?= clang-format

# Default target
.DEFAULT_GOAL := build

help:
	@echo "Suzume Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  make build        - Build the project (default)"
	@echo "  make test         - Run all tests"
	@echo "  make clean        - Clean build directory"
	@echo "  make rebuild      - Clean and rebuild"
	@echo "  make format       - Format code with clang-format"
	@echo "  make format-check - Check code formatting"
	@echo "  make configure    - Configure CMake"
	@echo "  make help         - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make              # Build the project"
	@echo "  make test         # Run all tests"
	@echo "  make rebuild      # Clean and rebuild from scratch"

# Configure CMake
configure:
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release $(CMAKE_OPTIONS) ..

# Build the project
build: configure
	@echo "Building Suzume..."
	cmake --build $(BUILD_DIR) --parallel
	@echo "Build complete!"

# Run tests
test: build
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
