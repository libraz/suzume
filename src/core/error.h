#ifndef SUZUME_CORE_ERROR_H_
#define SUZUME_CORE_ERROR_H_

#include <cassert>
#include <cstddef>
#include <string>
#include <string_view>

namespace suzume::core {

/**
 * @brief Error codes
 */
enum class ErrorCode {
  Success = 0,
  InvalidUtf8,           // Invalid UTF-8 input
  DictionaryLoadFailed,  // Dictionary load failed
  FileNotFound,          // File not found
  ParseError,            // Parse error
  OutOfMemory,           // Out of memory
  InvalidInput,          // Invalid input
  InternalError          // Internal error
};

/**
 * @brief Error information
 */
struct Error {
  ErrorCode code;
  std::string message;

  Error(ErrorCode err_code, std::string msg = "") : code(err_code), message(std::move(msg)) {}

  bool isSuccess() const { return code == ErrorCode::Success; }
  explicit operator bool() const { return !isSuccess(); }
};

/**
 * @brief Render an unsigned value as decimal digits
 * @note Diagnostic messages are the only place this library turns a number into
 *       text, and they only ever report counts and line numbers. Formatting them
 *       here keeps the standard library's generic integer formatter, which also
 *       handles signed and wider types, out of the build.
 */
std::string decimalDigits(size_t value);

/**
 * @brief Expected type for error handling (similar to std::expected)
 *
 * Usage:
 *   Expected<std::string, Error> normalize(std::string_view input);
 *   auto result = normalize("test");
 *   if (result.hasValue()) {
 *     // use result.value()
 *   } else {
 *     // use result.error()
 *   }
 */
template <typename E>
struct Unexpected;

template <typename T, typename E>
class Expected {
 public:
  Expected(const T& value) : data_(value), has_value_(true) {}
  Expected(T&& value) : data_(std::move(value)), has_value_(true) {}
  Expected(const E& error) : error_(error), has_value_(false) {}
  Expected(E&& error) : error_(std::move(error)), has_value_(false) {}

  // Conversion from Unexpected
  Expected(const Unexpected<E>& unexp) : error_(unexp.error), has_value_(false) {}
  Expected(Unexpected<E>&& unexp) : error_(std::move(unexp).error), has_value_(false) {}

  bool hasValue() const { return has_value_; }
  explicit operator bool() const { return has_value_; }

  const T& value() const& { return data_; }
  T& value() & { return data_; }
  T&& value() && { return std::move(data_); }

  const E& error() const& {
    assert(!has_value_ && "error() called on a value-holding Expected");
    return error_;
  }
  E& error() & {
    assert(!has_value_ && "error() called on a value-holding Expected");
    return error_;
  }

 private:
  T data_{};
  // Sentinel (non-Success) so a misused error() on a value-holding Expected does
  // not read as Success. Error-holding constructors overwrite this with the real
  // error, leaving the correct case unchanged.
  E error_{ErrorCode::InternalError};
  bool has_value_;
};

/**
 * @brief Create unexpected (error) result
 */
template <typename E>
struct Unexpected {
  E error;
  explicit Unexpected(E err) : error(std::move(err)) {}
};

template <typename E>
Unexpected<E> makeUnexpected(E error) {
  return Unexpected<E>(std::move(error));
}

}  // namespace suzume::core

#endif  // SUZUME_CORE_ERROR_H_
